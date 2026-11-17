#include "core/PresetIO.h"

#include "core/Version.h"

namespace bfka
{

namespace
{
    const juce::Identifier kPresetType { "BFKAPreset" };
    const juce::Identifier kKickType   { "BFKAKick" };
    const juce::Identifier kModuleType { "Module" };

    constexpr int kMaxFileBytes = 32 * 1024 * 1024;

    juce::String nowIsoString()
    {
        return juce::Time::getCurrentTime().toISO8601 (true);
    }

    /** Kopf-Attribute setzen; leere Werte werden weggelassen. */
    void writeHeader (juce::ValueTree& root, const PresetInfo& info, bool isModule)
    {
        root.setProperty ("version", PresetInfo::schemaVersion, nullptr);
        root.setProperty ("kind", isModule ? "module" : "kick", nullptr);
        root.setProperty ("name", info.name, nullptr);

        if (info.author.isNotEmpty())
            root.setProperty ("author", info.author, nullptr);

        if (info.styleId.isNotEmpty())
            root.setProperty ("styleId", info.styleId, nullptr);

        if (info.moduleType.isNotEmpty())
            root.setProperty ("type", info.moduleType, nullptr);

        root.setProperty ("created", info.created.isNotEmpty() ? info.created : nowIsoString(), nullptr);
        root.setProperty ("appVersion", info.appVersion.isNotEmpty() ? info.appVersion
                                                                    : juce::String (Version::get()),
                          nullptr);
    }

    PresetInfo readHeader (const juce::ValueTree& root)
    {
        PresetInfo info;
        info.name       = root.getProperty ("name", "").toString();
        info.author     = root.getProperty ("author", "").toString();
        info.styleId    = root.getProperty ("styleId", "").toString();
        info.created    = root.getProperty ("created", "").toString();
        info.appVersion = root.getProperty ("appVersion", "").toString();
        info.isModule   = root.getProperty ("kind", "kick").toString().equalsIgnoreCase ("module");
        info.moduleType = root.getProperty ("type", "").toString();
        return info;
    }

    /** Baut `<Module>` im Schema von `FxChainIO` (data als erstes Kind). */
    juce::ValueTree moduleToValueTree (const FxModuleState& module)
    {
        juce::ValueTree tree (kModuleType);
        tree.setProperty ("type", module.type, nullptr);
        tree.setProperty ("on", module.enabled, nullptr);
        tree.setProperty ("mix", module.mix, nullptr);
        tree.setProperty ("outGain", module.outGain, nullptr);

        if (module.data.isValid())
            tree.appendChild (module.data.createCopy(), nullptr);

        return tree;
    }

    std::optional<FxModuleState> moduleFromValueTree (const juce::ValueTree& tree)
    {
        if (! tree.isValid() || ! tree.hasType (kModuleType))
            return std::nullopt;

        FxModuleState module;
        module.type    = tree.getProperty ("type", "").toString();
        module.enabled = (bool) tree.getProperty ("on", true);
        module.mix     = (float) (double) tree.getProperty ("mix", 1.0);
        module.outGain = (float) (double) tree.getProperty ("outGain", 1.0);

        if (module.type.isEmpty())
            return std::nullopt;

        for (int i = 0; i < tree.getNumChildren(); ++i)
        {
            const auto child = tree.getChild (i);

            if (child.hasType (kModuleType))
            {
                module.data = child.createCopy();
                break;
            }
        }

        return module;
    }

    /** Schreibt einen `ValueTree` als XML-Datei (UTF-8, LF, kein BOM). */
    bool writeTreeToFile (const juce::File& file, const juce::ValueTree& tree, juce::String& error)
    {
        const auto xml = tree.createXml();

        if (xml == nullptr)
        {
            error = "Preset konnte nicht als XML erzeugt werden.";
            return false;
        }

        auto target = PresetIO::withPresetExtension (file);

        if (target.getParentDirectory().createDirectory().failed())
        {
            error = "Ordner konnte nicht angelegt werden: "
                    + target.getParentDirectory().getFullPathName();
            return false;
        }

        // `replaceWithText` schreibt UTF-8 ohne BOM; Zeilenenden kommen aus dem
        // erzeugten XML (JUCE nutzt "\n").
        if (! target.replaceWithText (xml->toString()))
        {
            error = "Datei konnte nicht geschrieben werden: " + target.getFullPathName();
            return false;
        }

        return true;
    }

    /** Liest eine Preset-Datei und parst sie. */
    std::optional<juce::ValueTree> readTreeFromFile (const juce::File& file, juce::String& error)
    {
        if (! file.existsAsFile())
        {
            error = "Datei nicht gefunden: " + file.getFullPathName();
            return std::nullopt;
        }

        if (file.getSize() > kMaxFileBytes)
        {
            error = "Datei ist zu gross fuer ein Preset ("
                    + juce::String (file.getSize() / (1024 * 1024)) + " MB).";
            return std::nullopt;
        }

        auto xml = juce::XmlDocument::parse (file);

        if (xml == nullptr)
        {
            error = "Datei ist kein gueltiges XML: " + file.getFileName();
            return std::nullopt;
        }

        auto tree = juce::ValueTree::fromXml (*xml);

        if (! tree.isValid())
        {
            error = "Preset-Baum ist leer: " + file.getFileName();
            return std::nullopt;
        }

        return tree;
    }
} // namespace

// ---------------------------------------------------------------------------
// Format und Ablage
// ---------------------------------------------------------------------------
juce::String PresetIO::fileExtension()
{
    return juce::String (Version::presetExt);
}

juce::String PresetIO::fileWildcard()
{
    return juce::String (Version::presetWildcard);
}

bool PresetIO::hasPresetExtension (const juce::File& file)
{
    return file.getFileExtension().equalsIgnoreCase (fileExtension());
}

juce::File PresetIO::withPresetExtension (const juce::File& file)
{
    return hasPresetExtension (file) ? file : juce::File (file.getFullPathName() + fileExtension());
}

juce::File PresetIO::userPresetDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("BassForge")
               .getChildFile ("KickArchitect")
               .getChildFile ("Presets");
}

juce::File PresetIO::ensureUserPresetDirectory()
{
    auto directory = userPresetDirectory();
    directory.createDirectory();
    return directory;
}

// ---------------------------------------------------------------------------
// Bäume
// ---------------------------------------------------------------------------
juce::ValueTree PresetIO::kickPresetToValueTree (const KickState& state, const PresetInfo& info)
{
    juce::ValueTree root (kPresetType);
    writeHeader (root, info, false);
    root.appendChild (state.toValueTree(), nullptr);
    return root;
}

juce::ValueTree PresetIO::modulePresetToValueTree (const FxModuleState& module, const PresetInfo& info)
{
    juce::ValueTree root (kPresetType);
    writeHeader (root, info, true);
    root.appendChild (moduleToValueTree (module), nullptr);
    return root;
}

std::optional<PresetData> PresetIO::fromValueTree (const juce::ValueTree& tree, juce::String& error)
{
    if (! tree.isValid())
    {
        error = "Leerer Preset-Baum.";
        return std::nullopt;
    }

    // Host-Zustand: nackte BFKAKick-Wurzel ohne Kopf.
    if (tree.hasType (kKickType))
    {
        auto state = KickState::fromValueTree (tree, error);

        if (! state.has_value())
            return std::nullopt;

        PresetData data;
        data.state = *state;
        return data;
    }

    if (! tree.hasType (kPresetType))
    {
        error = "Unbekannte Wurzel \"" + tree.getType().toString()
                + "\" (erwartet wird BFKAPreset).";
        return std::nullopt;
    }

    PresetData data;
    data.info = readHeader (tree);

    if (data.info.isModule)
    {
        auto module = moduleFromValueTree (tree.getChildWithName (kModuleType));

        if (! module.has_value())
        {
            error = "Modul-Preset ohne gueltiges <Module>.";
            return std::nullopt;
        }

        data.module = *module;

        if (data.info.moduleType.isEmpty())
            data.info.moduleType = data.module.type;

        return data;
    }

    auto kick = tree.getChildWithName (kKickType);

    if (! kick.isValid())
    {
        error = "Kick-Preset ohne <BFKAKick>.";
        return std::nullopt;
    }

    auto state = KickState::fromValueTree (kick, error);

    if (! state.has_value())
        return std::nullopt;

    data.state = *state;
    return data;
}

// ---------------------------------------------------------------------------
// Dateien
// ---------------------------------------------------------------------------
bool PresetIO::saveKickPreset (const juce::File& file,
                               const KickState& state,
                               const PresetInfo& info,
                               juce::String& error)
{
    auto header = info;

    if (header.name.isEmpty())
        header.name = file.getFileNameWithoutExtension();

    header.isModule = false;
    return writeTreeToFile (file, kickPresetToValueTree (state, header), error);
}

bool PresetIO::saveModulePreset (const juce::File& file,
                                 const FxModuleState& module,
                                 const PresetInfo& info,
                                 juce::String& error)
{
    if (module.type.isEmpty())
    {
        error = "Modul ohne Typ kann nicht gespeichert werden.";
        return false;
    }

    auto header = info;

    if (header.name.isEmpty())
        header.name = file.getFileNameWithoutExtension();

    header.isModule   = true;
    header.moduleType = module.type;

    return writeTreeToFile (file, modulePresetToValueTree (module, header), error);
}

std::optional<PresetData> PresetIO::loadKickPreset (const juce::File& file, juce::String& error)
{
    auto tree = readTreeFromFile (file, error);

    if (! tree.has_value())
        return std::nullopt;

    auto data = fromValueTree (*tree, error);

    if (! data.has_value())
        return std::nullopt;

    if (data->info.isModule)
    {
        error = "Das ist ein Modul-Preset, kein Kick-Preset: " + file.getFileName();
        return std::nullopt;
    }

    if (data->info.name.isEmpty())
        data->info.name = file.getFileNameWithoutExtension();

    return data;
}

std::optional<PresetData> PresetIO::loadModulePreset (const juce::File& file, juce::String& error)
{
    auto tree = readTreeFromFile (file, error);

    if (! tree.has_value())
        return std::nullopt;

    auto data = fromValueTree (*tree, error);

    if (! data.has_value())
        return std::nullopt;

    if (! data->info.isModule)
    {
        error = "Das ist ein Kick-Preset, kein Modul-Preset: " + file.getFileName();
        return std::nullopt;
    }

    if (data->info.name.isEmpty())
        data->info.name = file.getFileNameWithoutExtension();

    return data;
}

} // namespace bfka
