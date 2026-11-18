// B-76: Lesen/Schreiben nutzereigener Styles (`.bfkastyle`).
#include "core/StyleIO.h"

#include <memory>

namespace bfka
{

namespace
{
    const juce::Identifier kStyleType  { "BFKAStyle" };
    const juce::Identifier kNotesType  { "Notes" };

    /** Tag-Name des Notiz-Kindes (XML und `ValueTree` nutzen denselben). */
    juce::String notesTag() { return kNotesType.toString(); }

    /** Hoechste bekannte Schema-Version des Style-Kopfes. */
    constexpr int kSchemaVersion = 1;

    /** Grenze gegen Unsinnsdateien; ein Style ist reiner Text. */
    constexpr juce::int64 kMaxFileBytes = 1024 * 1024;

    /**
     * Liest ein Attribut als Schalter.
     *
     * Aus dem XML kommen die Werte als Text ("1"/"true"); aus einem
     * `ValueTree` als echter `bool`. Beide Formen werden akzeptiert.
     */
    bool readFlag (const juce::ValueTree& tree, const juce::String& name)
    {
        const auto value = tree.getProperty (juce::Identifier (name), false);

        if (value.isBool())
            return (bool) value;

        const auto text = value.toString().trim();

        return text == "1" || text.equalsIgnoreCase ("true") || text.equalsIgnoreCase ("yes");
    }

    /** Ergaenzt eine fehlende Endung (kein doppeltes `.bfkastyle`). */
    juce::File withStyleExtension (const juce::File& file)
    {
        if (file.getFileExtension().equalsIgnoreCase ("." + StyleIO::fileExtension()))
            return file;

        return juce::File (file.getFullPathName() + "." + StyleIO::fileExtension());
    }

    /**
     * Baut das XML aus `toValueTree`.
     *
     * `ValueTree` kennt keinen Textknoten; der Notiztext wird deshalb
     * nachtraeglich als Elementinhalt von `<Notes>` eingesetzt - so steht in
     * der Datei `<Notes>handmade</Notes>` und nicht ein Attribut.
     */
    std::unique_ptr<juce::XmlElement> createXml (const UserStyle& style)
    {
        auto xml = StyleIO::toValueTree (style).createXml();

        if (xml == nullptr)
            return nullptr;

        if (auto* notes = xml->getChildByName (notesTag()))
        {
            notes->removeAttribute ("text");

            if (style.notes.isNotEmpty())
                notes->addTextElement (style.notes);
        }

        return xml;
    }
} // namespace

juce::String StyleIO::fileExtension()
{
    return "bfkastyle";
}

juce::String StyleIO::fileWildcard()
{
    return "*." + fileExtension();
}

juce::ValueTree StyleIO::toValueTree (const UserStyle& style)
{
    juce::ValueTree tree (kStyleType);

    tree.setProperty ("version", kSchemaVersion, nullptr);
    tree.setProperty ("id", style.id, nullptr);
    tree.setProperty ("name", style.name, nullptr);

    if (style.basedOn.isNotEmpty())
        tree.setProperty ("basedOn", style.basedOn, nullptr);

    if (style.accentColor.isNotEmpty())
        tree.setProperty ("accentColor", style.accentColor, nullptr);

    tree.setProperty ("bothLibraries", style.bothLibraries, nullptr);
    tree.setProperty ("freeBuild", style.freeBuild, nullptr);

    // `ValueTree` kennt keinen Textknoten: der Notiztext reist hier als
    // Eigenschaft `text`; `save()` schreibt ihn als Elementinhalt.
    juce::ValueTree notes (kNotesType);
    notes.setProperty ("text", style.notes, nullptr);
    tree.appendChild (notes, nullptr);

    return tree;
}

std::optional<UserStyle> StyleIO::fromValueTree (const juce::ValueTree& tree, juce::String& error)
{
    if (! tree.isValid() || ! tree.hasType (kStyleType))
    {
        error = "Wurzel ist kein <BFKAStyle>.";
        return std::nullopt;
    }

    UserStyle style;
    style.id            = tree.getProperty ("id", "").toString().trim();
    style.name          = tree.getProperty ("name", "").toString().trim();
    style.basedOn       = tree.getProperty ("basedOn", "").toString();
    style.accentColor   = tree.getProperty ("accentColor", "").toString();
    style.bothLibraries = readFlag (tree, "bothLibraries");
    style.freeBuild     = readFlag (tree, "freeBuild");

    const auto notes = tree.getChildWithName (kNotesType);

    if (notes.isValid())
        style.notes = notes.getProperty ("text", "").toString();

    if (! style.isValid())
    {
        error = "Style braucht eine id und einen Namen.";
        return std::nullopt;
    }

    return style;
}

bool StyleIO::save (const juce::File& file, const UserStyle& style, juce::String& error)
{
    if (! style.isValid())
    {
        error = "Style braucht eine id und einen Namen.";
        return false;
    }

    const auto target = withStyleExtension (file);

    if (target.getParentDirectory().createDirectory().failed())
    {
        error = "Ordner konnte nicht angelegt werden: "
                + target.getParentDirectory().getFullPathName();
        return false;
    }

    const auto xml = createXml (style);

    if (xml == nullptr || ! xml->writeTo (target))
    {
        error = "Datei konnte nicht geschrieben werden: " + target.getFullPathName();
        return false;
    }

    return true;
}

std::optional<UserStyle> StyleIO::load (const juce::File& file, juce::String& error)
{
    if (! file.existsAsFile())
    {
        error = "Datei nicht gefunden: " + file.getFullPathName();
        return std::nullopt;
    }

    if (file.getSize() > kMaxFileBytes)
    {
        error = "Datei ist zu gross fuer einen Style: " + file.getFileName();
        return std::nullopt;
    }

    auto xml = juce::XmlDocument::parse (file);

    if (xml == nullptr)
    {
        error = "Datei ist kein gueltiges XML: " + file.getFileName();
        return std::nullopt;
    }

    // Der Notiztext steht in der Datei als Elementinhalt. `ValueTree` kennt
    // keine Textknoten und wuerde beim Umwandeln eine Zusicherung ausloesen;
    // deshalb wird der Text vorher gelesen und als Attribut gesichert.
    if (auto* notes = xml->getChildByName (notesTag()))
    {
        const auto text = notes->getAllSubText();
        notes->deleteAllTextElements();

        if (text.isNotEmpty())
            notes->setAttribute ("text", text);
    }

    auto tree = juce::ValueTree::fromXml (*xml);

    if (! tree.isValid())
    {
        error = "Style-Baum ist leer: " + file.getFileName();
        return std::nullopt;
    }

    return fromValueTree (tree, error);
}

} // namespace bfka
