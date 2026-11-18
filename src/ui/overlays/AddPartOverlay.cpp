// B-103: "+ ADD PART"-Picker (zweistufig, Gruppensymbole, "NO FREE SLOT").
// B-104: "+ ADD STYLE"-Overlay (Name, Basis-Style, Akzentfarbe).

#include "ui/overlays/AddPartOverlay.h"

#include "ui/UiTheme.h"

namespace bfka
{

namespace
{
    constexpr int kTitleHeight  = 44;
    constexpr int kButtonHeight = 40;
    constexpr int kButtonGap    = 8;
    constexpr int kRowGap       = 24;
    constexpr int kInset        = 16;

    /** Button-Optik an die Panels des Editors anlehnen. */
    void styleButton (juce::TextButton& b, bool highlighted)
    {
        b.setColour (juce::TextButton::buttonColourId,
                     highlighted ? ui::theme::panelAlt : ui::theme::panel);
        b.setColour (juce::TextButton::textColourOffId, ui::theme::text);
        b.setColour (juce::TextButton::textColourOnId, ui::theme::text);
        b.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }
} // namespace

// ============================================================================
// AddPartOverlay (B-103)
// ============================================================================

AddPartOverlay::AddPartOverlay()
{
    setWantsKeyboardFocus (true);
}

AddPartOverlay::~AddPartOverlay() = default;

void AddPartOverlay::setGroups (std::vector<AddPartGroup> groups)
{
    groups_ = std::move (groups);
    currentGroup = -1;

    groupButtons.clear();
    itemButtons.clear();

    for (int i = 0; i < (int) groups_.size(); ++i)
    {
        auto button = std::make_unique<juce::TextButton>();

        // Stufe 1: Symbol vor dem Gruppennamen, z. B. "PU  PUNCH".
        const auto& group = groups_[(size_t) i];
        juce::String symbol;

        if (! group.items.empty())
            symbol = group.items.front().symbol.trim();

        button->setButtonText (symbol.isNotEmpty() ? symbol + "  " + group.name
                                                   : group.name);
        styleButton (*button, false);
        button->onClick = [this, i] { selectGroup (i); };

        addAndMakeVisible (*button);
        groupButtons.push_back (std::move (button));
    }

    resized();
    repaint();
}

int AddPartOverlay::groupCount() const
{
    return (int) groups_.size();
}

juce::String AddPartOverlay::groupName (int index) const
{
    if (! juce::isPositiveAndBelow (index, (int) groups_.size()))
        return {};

    return groups_[(size_t) index].name;
}

int AddPartOverlay::stage() const noexcept
{
    return currentGroup < 0 ? 0 : 1;
}

void AddPartOverlay::selectGroup (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) groups_.size()))
        return;

    currentGroup = index;

    // Stufe 2: Eintraege der aktiven Gruppe als Buttons.
    itemButtons.clear();

    const auto& items = groups_[(size_t) currentGroup].items;

    for (int i = 0; i < (int) items.size(); ++i)
    {
        auto button = std::make_unique<juce::TextButton>();

        button->setButtonText (items[(size_t) i].name);
        styleButton (*button, items[(size_t) i].freeSlots > 0);
        button->setEnabled (items[(size_t) i].freeSlots > 0);

        // chooseItem() meldet den Treffer selbst ueber onPartChosen.
        button->onClick = [this, i]
        {
            juce::String chosen;
            chooseItem (i, chosen);
        };

        addAndMakeVisible (*button);
        itemButtons.push_back (std::move (button));
    }

    resized();
    repaint();
}

int AddPartOverlay::selectedGroup() const noexcept
{
    return currentGroup;
}

void AddPartOverlay::goBack()
{
    currentGroup = -1;
    itemButtons.clear();

    resized();
    repaint();
}

int AddPartOverlay::itemCount() const
{
    if (! juce::isPositiveAndBelow (currentGroup, (int) groups_.size()))
        return 0;

    return (int) groups_[(size_t) currentGroup].items.size();
}

juce::String AddPartOverlay::itemName (int index) const
{
    if (! juce::isPositiveAndBelow (currentGroup, (int) groups_.size()))
        return {};

    const auto& items = groups_[(size_t) currentGroup].items;

    if (! juce::isPositiveAndBelow (index, (int) items.size()))
        return {};

    return items[(size_t) index].name;
}

bool AddPartOverlay::chooseItem (int index, juce::String& chosenId)
{
    if (freeSlotCount <= 0)
        return false;

    if (! juce::isPositiveAndBelow (currentGroup, (int) groups_.size()))
        return false;

    const auto& items = groups_[(size_t) currentGroup].items;

    if (! juce::isPositiveAndBelow (index, (int) items.size()))
        return false;

    const auto& item = items[(size_t) index];

    // Eintraege ohne eigenen freien Platz sind nicht waehlbar.
    if (item.freeSlots <= 0)
        return false;

    chosenId = item.id;

    if (onPartChosen)
        onPartChosen (chosenId);

    return true;
}

void AddPartOverlay::setFreeSlots (int slots)
{
    freeSlotCount = juce::jmax (0, slots);
    repaint();
}

int AddPartOverlay::freeSlots() const noexcept
{
    return freeSlotCount;
}

bool AddPartOverlay::showsNoFreeSlot() const noexcept
{
    return freeSlotCount <= 0;
}

void AddPartOverlay::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat();

    g.setColour (ui::theme::background);
    g.fillRoundedRectangle (area, 10.0f);
    g.setColour (ui::theme::grid);
    g.drawRoundedRectangle (area.reduced (0.5f), 10.0f, 1.0f);

    auto header = getLocalBounds().reduced (kInset, 0).removeFromTop (kTitleHeight);

    g.setColour (ui::theme::text);
    g.setFont (ui::theme::titleFont());
    g.drawFittedText ("ADD PART", header, juce::Justification::centredLeft, 1);

    if (showsNoFreeSlot())
    {
        g.setColour (ui::theme::error);
        g.setFont (juce::Font (juce::FontOptions (28.0f).withStyle ("Bold")));
        g.drawFittedText ("NO FREE SLOT", header, juce::Justification::centredRight, 1);
    }

    // Stufe 2 bekommt eine Rueckzeile; Stufe 1 eine Erklaerung.
    if (stage() == 1)
    {
        g.setColour (ui::theme::textDim);
        g.setFont (ui::theme::smallFont());
        g.drawFittedText ("ESC = ZURUECK  |  " + groupName (currentGroup),
                          getLocalBounds().reduced (kInset, 0).removeFromBottom (18),
                          juce::Justification::centredLeft, 1);
    }
}

void AddPartOverlay::resized()
{
    // Stufe 1 zeigt Gruppen, Stufe 2 nur die Eintraege.
    for (auto& b : groupButtons)
        b->setVisible (stage() == 0);

    for (auto& b : itemButtons)
        b->setVisible (stage() == 1);

    auto area = getLocalBounds().reduced (kInset, 0);

    area.removeFromTop (kTitleHeight);
    area.removeFromBottom (kRowGap);

    if (stage() == 0)
    {
        const int columns = juce::jmax (1, area.getWidth() / 240);

        for (int i = 0; i < (int) groupButtons.size(); ++i)
        {
            const int col = i % columns;
            const int row = i / columns;

            const int w = (area.getWidth() - (columns - 1) * kButtonGap) / columns;

            groupButtons[(size_t) i]->setBounds (area.getX() + col * (w + kButtonGap),
                                                 area.getY() + row * (kButtonHeight + kButtonGap),
                                                 w, kButtonHeight);
        }
    }
    else
    {
        for (int i = 0; i < (int) itemButtons.size(); ++i)
        {
            itemButtons[(size_t) i]->setBounds (area.removeFromTop (kButtonHeight));

            if (i + 1 < (int) itemButtons.size())
                area.removeFromTop (kButtonGap);
        }
    }
}

bool AddPartOverlay::keyPressed (const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::escapeKey)
    {
        if (stage() == 1)
            goBack();
        else if (onCloseRequested)
            onCloseRequested();

        return true;
    }

    return false;
}

// ============================================================================
// AddStyleOverlay (B-104)
// ============================================================================

AddStyleOverlay::AddStyleOverlay()
{
    setWantsKeyboardFocus (true);

    nameEditor.setMultiLine (false);
    nameEditor.setReturnKeyStartsNewLine (false);
    nameEditor.setColour (juce::TextEditor::backgroundColourId, ui::theme::panelDeep);
    nameEditor.setColour (juce::TextEditor::textColourId, ui::theme::text);
    nameEditor.setColour (juce::TextEditor::outlineColourId, ui::theme::grid);
    nameEditor.setColour (juce::TextEditor::focusedOutlineColourId, ui::theme::accent);
    nameEditor.onTextChange = [this] { styleName = nameEditor.getText(); };
    addAndMakeVisible (nameEditor);

    baseBox.setColour (juce::ComboBox::backgroundColourId, ui::theme::panelDeep);
    baseBox.setColour (juce::ComboBox::textColourId, ui::theme::text);
    baseBox.setColour (juce::ComboBox::outlineColourId, ui::theme::grid);
    baseBox.setColour (juce::ComboBox::arrowColourId, ui::theme::textDim);
    baseBox.onChange = [this]
    {
        const int selected = baseBox.getSelectedId();

        if (selected > 0 && selected <= bases.size())
            baseId = bases[selected - 1];
    };
    addAndMakeVisible (baseBox);
}

AddStyleOverlay::~AddStyleOverlay() = default;

void AddStyleOverlay::setBaseStyles (juce::StringArray baseStyleIds)
{
    bases = std::move (baseStyleIds);

    baseBox.clear (juce::dontSendNotification);

    for (int i = 0; i < bases.size(); ++i)
        baseBox.addItem (bases[i], i + 1);

    // Gemerkte Auswahl wiederherstellen, sonst zuruecksetzen.
    const int index = bases.indexOf (baseId);

    if (index >= 0)
        baseBox.setSelectedId (index + 1, juce::dontSendNotification);
    else
        baseId.clear();
}

void AddStyleOverlay::setName (const juce::String& name)
{
    styleName = name;
    nameEditor.setText (name, false);
}

juce::String AddStyleOverlay::name() const
{
    return styleName;
}

void AddStyleOverlay::setBaseStyle (const juce::String& styleId)
{
    baseId = styleId;

    const int index = bases.indexOf (baseId);
    baseBox.setSelectedId (index >= 0 ? index + 1 : 0, juce::dontSendNotification);
}

juce::String AddStyleOverlay::baseStyle() const
{
    return baseId;
}

void AddStyleOverlay::setAccentHex (const juce::String& hex)
{
    accent = hex;
    repaint();
}

juce::String AddStyleOverlay::accentHex() const
{
    return accent;
}

bool AddStyleOverlay::canCreate() const
{
    return styleName.trim().isNotEmpty() && baseId.isNotEmpty();
}

bool AddStyleOverlay::create (juce::String& idOut, juce::String& error) const
{
    const auto trimmedName = styleName.trim();

    if (trimmedName.isEmpty())
    {
        error = "Name fehlt.";
        return false;
    }

    if (baseId.isEmpty())
    {
        error = "Basis-Style fehlt.";
        return false;
    }

    // Id: "user-" + klein geschriebener Name, Leerzeichen -> "-",
    // nur [a-z0-9-] bleibt erhalten.
    juce::String slug;

    const auto lower = trimmedName.toLowerCase();

    for (int i = 0; i < lower.length(); ++i)
    {
        const auto ch = lower[i];

        if (ch == ' ')
            slug += "-";
        else if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '-')
            slug += juce::String::charToString (ch);
    }

    // Mehrfache Trennstriche zusammenfassen, Raender entfernen.
    while (slug.contains ("--"))
        slug = slug.replace ("--", "-");

    slug = slug.trimCharactersAtStart ("-").trimCharactersAtEnd ("-");

    if (slug.isEmpty())
    {
        error = "Name enthaelt keine verwendbaren Zeichen.";
        return false;
    }

    idOut = "user-" + slug;
    error.clear();
    return true;
}

void AddStyleOverlay::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat();

    g.setColour (ui::theme::background);
    g.fillRoundedRectangle (area, 10.0f);
    g.setColour (ui::theme::grid);
    g.drawRoundedRectangle (area.reduced (0.5f), 10.0f, 1.0f);

    auto header = getLocalBounds().reduced (kInset, 0).removeFromTop (kTitleHeight);

    g.setColour (ui::theme::text);
    g.setFont (ui::theme::titleFont());
    g.drawFittedText ("ADD STYLE", header, juce::Justification::centredLeft, 1);

    auto body = getLocalBounds().reduced (kInset, 0);
    body.removeFromTop (kTitleHeight);

    g.setColour (ui::theme::textDim);
    g.setFont (ui::theme::smallFont());

    const auto nameLabel = body.removeFromTop (16);
    g.drawFittedText ("NAME", nameLabel, juce::Justification::centredLeft, 1);

    body.removeFromTop (32 + 12);

    const auto baseLabel = body.removeFromTop (16);
    g.drawFittedText ("BASIS-STYLE", baseLabel, juce::Justification::centredLeft, 1);

    body.removeFromTop (32 + 12);

    const auto accentLabel = body.removeFromTop (16);
    g.drawFittedText ("AKZENT", accentLabel, juce::Justification::centredLeft, 1);

    // Farbfeld rechts neben dem Akzent-Label.
    const juce::Rectangle<int> swatch (getWidth() - kInset - 56,
                                       accentLabel.getCentreY() - 8, 56, 16);

    g.setColour (juce::Colour::fromString (accent.startsWith ("#") ? accent : "#" + accent));
    g.fillRoundedRectangle (swatch.toFloat(), 3.0f);
    g.setColour (ui::theme::grid);
    g.drawRoundedRectangle (swatch.toFloat(), 3.0f, 1.0f);
}

void AddStyleOverlay::resized()
{
    auto area = getLocalBounds().reduced (kInset, 0);

    area.removeFromTop (kTitleHeight);

    // Reihenfolge muss zu paint() passen: Label 16 px, Feld 32 px, Abstand 12 px.
    area.removeFromTop (16);
    nameEditor.setBounds (area.removeFromTop (32));

    area.removeFromTop (12);
    area.removeFromTop (16);
    baseBox.setBounds (area.removeFromTop (32));
}

bool AddStyleOverlay::keyPressed (const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::escapeKey)
    {
        if (onCloseRequested)
            onCloseRequested();

        return true;
    }

    if (key.getKeyCode() == juce::KeyPress::returnKey)
    {
        juce::String id, error;

        if (create (id, error) && onStyleCreated)
            onStyleCreated (id);

        return true;
    }

    return false;
}

} // namespace bfka
