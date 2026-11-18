// B-16 -- Preset-Baum nutzt die Zustands-Migration aus core/KickState.
#include "core/PresetIO.h"

#include "core/Version.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <memory>

namespace bfka
{

namespace
{
    const juce::Identifier kPresetType  { "BFKAPreset" };
    const juce::Identifier kKickType    { "BFKAKick" };
    const juce::Identifier kModuleType  { "Module" };
    const juce::Identifier kSamplesType { "Samples" };
    const juce::Identifier kSampleType  { "Sample" };

    /**
     * Hoechstzahl an Bytes, die ein Preset beim Lesen haben darf.
     * Grosszuegiger als das Einbett-Limit, weil Base64 die Nutzdaten um etwa
     * ein Drittel aufblaeht: ein Preset am 32-MB-Limit ist rund 43 MB gross.
     */
    constexpr juce::int64 kMaxFileBytes = 64 * 1024 * 1024;

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

    /** Liefert die Quelldatei eines Sample-Verweises (absolut vor relativ). */
    juce::File sourceFileFor (const SampleRef& ref)
    {
        if (ref.absPath.isNotEmpty())
            return juce::File (ref.absPath);

        return juce::File (ref.relPath);
    }

    /**
     * Liest eine Audiodatei und kodiert sie als 16-bit-WAV in Base64 (B-95).
     * Gibt `false` zurueck, wenn die Datei fehlt oder nicht lesbar ist.
     */
    bool readSampleAsBase64 (const juce::File& source, juce::String& base64, juce::int64& rawBytes)
    {
        if (! source.existsAsFile())
            return false;

        juce::AudioFormatManager formats;
        formats.registerBasicFormats();

        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (source));

        if (reader == nullptr || reader->lengthInSamples <= 0)
            return false;

        const auto numSamples  = (int) juce::jmin (reader->lengthInSamples, (juce::int64) (1 << 26));
        const auto numChannels = (int) juce::jlimit (1u, 2u, reader->numChannels);

        juce::AudioBuffer<float> buffer (numChannels, numSamples);
        buffer.clear();

        if (! reader->read (&buffer, 0, numSamples, 0, true, numChannels > 1))
            return false;

        const auto sampleRate = reader->sampleRate > 0.0 ? reader->sampleRate : 48000.0;

        // Der Writer uebernimmt den Stream bei Erfolg; der Block bleibt uns.
        juce::MemoryBlock block;
        auto stream = std::make_unique<juce::MemoryOutputStream> (block, false);

        juce::WavAudioFormat wavFormat;
        std::unique_ptr<juce::AudioFormatWriter> writer (
            wavFormat.createWriterFor (stream.get(), sampleRate, (unsigned int) numChannels, 16, {}, 0));

        if (writer == nullptr)
            return false;

        stream.release();   // ab hier besitzt der Writer den Stream

        if (! writer->writeFromAudioSampleBuffer (buffer, 0, numSamples))
        {
            writer.reset();
            return false;
        }

        writer.reset();   // schliesst den Stream und schreibt den WAV-Kopf fertig

        if (block.getSize() == 0)
            return false;

        rawBytes = (juce::int64) block.getSize();
        base64 = juce::Base64::toBase64 (block.getData(), block.getSize());
        return base64.isNotEmpty();
    }

    /**
     * Haengt die Slot-Samples als Base64-WAV an `<Samples>` (B-95).
     *
     * Nicht lesbare Slots werden uebersprungen. Ueberschreitet die Summe
     * `options.maxEmbeddedBytes`, wird `false` geliefert und der Baum bleibt
     * unvollstaendig - der Aufrufer schreibt dann keine Datei.
     */
    bool embedSamplesIntoTree (const KickState& state,
                               juce::ValueTree& root,
                               const PresetWriteOptions& options,
                               juce::String& error)
    {
        juce::ValueTree samplesTree (kSamplesType);
        juce::int64 totalBytes = 0;

        for (int slot = 0; slot < KickState::kNumSlots; ++slot)
        {
            const auto& ref = state.getSample (slot);

            if (! ref.has_value())
                continue;

            const auto source = sourceFileFor (*ref);
            juce::String base64;
            juce::int64 rawBytes = 0;

            if (! readSampleAsBase64 (source, base64, rawBytes))
                continue;   // fehlt oder nicht lesbar: Slot bleibt aussen vor

            // Gezaehlt wird die WAV-Nutzlast; Base64 blaeht sie beim Schreiben
            // noch um etwa ein Drittel auf (siehe `kMaxFileBytes`).
            totalBytes += rawBytes;

            if (totalBytes > options.maxEmbeddedBytes)
            {
                error = "Preset zu gross: eingebettete Samples ueberschreiten das Limit.";
                return false;
            }

            juce::ValueTree sample (kSampleType);
            sample.setProperty ("slot", slot, nullptr);
            sample.setProperty ("fileName",
                                ref->fileName.isNotEmpty() ? ref->fileName : source.getFileName(),
                                nullptr);
            sample.setProperty ("format", "wav", nullptr);
            sample.setProperty ("bits", 16, nullptr);
            sample.setProperty ("data", base64, nullptr);
            samplesTree.appendChild (sample, nullptr);
        }

        if (samplesTree.getNumChildren() > 0)
            root.appendChild (samplesTree, nullptr);

        return true;
    }

    /**
     * Entpackt `<Samples>` in den Sample-Cache und traegt die Slots ein (B-95).
     * Fehler lassen das Laden nicht scheitern - der Slot bleibt dann, wie er war.
     */
    void extractSamplesFromTree (const juce::ValueTree& root, KickState& state)
    {
        const auto samplesTree = root.getChildWithName (kSamplesType);

        if (! samplesTree.isValid())
            return;

        for (int i = 0; i < samplesTree.getNumChildren(); ++i)
        {
            const auto sample = samplesTree.getChild (i);

            if (! sample.hasType (kSampleType))
                continue;

            const int slot = (int) sample.getProperty ("slot", -1);

            if (! juce::isPositiveAndBelow (slot, KickState::kNumSlots))
                continue;

            const auto fileName = sample.getProperty ("fileName", "").toString();
            const auto data     = sample.getProperty ("data", "").toString();

            if (fileName.isEmpty() || data.isEmpty())
                continue;

            // Nur reine Dateinamen: ein Preset darf nicht aus dem Cache-Ordner
            // heraus schreiben ("..\\..\\irgendwas").
            if (fileName.contains ("/") || fileName.contains ("\\") || fileName.contains (".."))
                continue;

            juce::MemoryOutputStream decoded;
            const auto decodedOk = juce::Base64::convertFromBase64 (decoded, data);

            if (! decodedOk || decoded.getDataSize() == 0)
                continue;

            const auto directory = PresetIO::sampleCacheDirectory();
            directory.createDirectory();

            const auto target = directory.getChildFile (fileName);

            if (! target.replaceWithData (decoded.getData(), decoded.getDataSize()))
                continue;

            SampleRef ref;
            ref.fileName = fileName;
            ref.absPath  = target.getFullPathName();
            ref.relPath  = {};
            ref.roleId   = {};
            state.setSample (slot, ref);
        }
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

juce::File PresetIO::sampleCacheDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("BassForge")
               .getChildFile ("KickArchitect")
               .getChildFile ("SampleCache");
}

// ---------------------------------------------------------------------------
// Baeume
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
    // B-16: `KickState::fromValueTree` hebt den Baum selbst auf
    // `KickState::currentSchemaVersion`; hier wird keine eigene Version
    // geprueft, damit alte Presets weiterhin laden.
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
    return saveKickPreset (file, state, info, PresetWriteOptions{}, error);
}

bool PresetIO::saveKickPreset (const juce::File& file,
                               const KickState& state,
                               const PresetInfo& info,
                               const PresetWriteOptions& options,
                               juce::String& error)
{
    auto header = info;

    if (header.name.isEmpty())
        header.name = file.getFileNameWithoutExtension();

    header.isModule = false;

    auto tree = kickPresetToValueTree (state, header);

    if (options.embedSamples)
    {
        if (! embedSamplesIntoTree (state, tree, options, error))
            return false;   // Limit gerissen: keine Datei schreiben
    }

    return writeTreeToFile (file, tree, error);
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

    // Eingebettete Samples entpacken (B-95); Fehler sind nicht fatal.
    extractSamplesFromTree (*tree, data->state);

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
