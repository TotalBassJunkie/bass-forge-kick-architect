// B-16 -- Zustands-Migration ueber Schema-Versionen (v1 -> v2).
#include "core/KickState.h"
#include "core/FxChainIO.h"
#include "core/ParameterRegistry.h"

namespace bfka
{

namespace
{
    const juce::Identifier kStateType { "BFKAKick" };
    const juce::Identifier kSlotsType { "Slots" };
    const juce::Identifier kSlotType  { "Slot" };
    const juce::Identifier kParamsType{ "Params" };
    const juce::Identifier kPType     { "P" };

    // B-16: Property-Namen der Migration, an einer Stelle.
    const juce::Identifier kSchemaVersion { "schemaVersion" };
    const juce::Identifier kBpmMode       { "bpmMode" };
    const juce::Identifier kStretchMode   { "stretchMode" };
    const juce::Identifier kAlignPhase    { "alignPhase" };
    const juce::Identifier kAlignStretch  { "alignStretchCrunch" };

    /** Setzt eine Property nur, wenn sie noch fehlt (B-16: nie ueberschreiben). */
    void ensureProperty (juce::ValueTree& tree, const juce::Identifier& id, const juce::var& value)
    {
        if (! tree.hasProperty (id))
            tree.setProperty (id, value, nullptr);
    }

    /**
     * Stufe 1 -> 2 (B-16): Tempo-/Stretch-Modus und die beiden
     * Align-Schalter ergaenzen, soweit sie fehlen.
     *
     * Vorhandene Werte bleiben stehen: ein v1-Baum, den ein neuerer Stand
     * bereits mit `bpmMode` geschrieben hat, verliert seine Wahl nicht.
     */
    void migrateV1ToV2 (juce::ValueTree& tree)
    {
        ensureProperty (tree, kBpmMode,     (int) BpmMode::syncBpm);
        ensureProperty (tree, kStretchMode, (int) StretchMode::keepPitch);
        ensureProperty (tree, kAlignPhase,  true);
        ensureProperty (tree, kAlignStretch, true);
    }
}
}

namespace bfka
{

juce::String roleToString (Role role)
{
    switch (role)
    {
        case Role::punch:            return "PUNCH";
        case Role::crunch:           return "CRUNCH";
        case Role::rumble:           return "RUMBLE";
        case Role::click:            return "TRANSIENT";
        case Role::top:              return "TOP";
        case Role::punchEnhancer:    return "PUNCH";
        case Role::rumbleEnhancer:   return "RUMBLE";
        case Role::schranzEnhancer:  return "TAIL+";
        case Role::none:             break;
    }

    return {};
}

Role roleFromString (const juce::String& text)
{
    const auto t = text.trim().toUpperCase();

    if (t == "PUNCH")   return Role::punch;
    if (t == "CRUNCH")  return Role::crunch;
    if (t == "RUMBLE")  return Role::rumble;
    if (t == "TOP")     return Role::top;
    if (t == "TRANSIENT" || t == "CLICK") return Role::click;

    // Pool-Kennungen (Kleinbuchstaben) direkt
    const auto lower = text.trim().toLowerCase();
    if (lower == "punch")             return Role::punch;
    if (lower == "crunch")            return Role::crunch;
    if (lower == "rumble")            return Role::rumble;
    if (lower == "click")             return Role::click;
    if (lower == "top")               return Role::top;
    if (lower == "punch_enhancer")    return Role::punchEnhancer;
    if (lower == "rumble_enhancer")   return Role::rumbleEnhancer;
    if (lower == "schranz_enhancer")  return Role::schranzEnhancer;

    return Role::none;
}

// ---------------------------------------------------------------------------

KickState::KickState()
{
    const auto& registry = ParameterRegistry::get();
    paramValues.assign ((size_t) registry.size(), 0.0f);

    for (const auto& d : registry.all())
        paramValues[(size_t) (&d - registry.all().data())] = d.normalisedDefault();

    roles.fill (Role::none);
    locked.fill (false);
    keyTracked.fill (false);
}

const std::optional<SampleRef>& KickState::getSample (int slot) const noexcept
{
    jassert (juce::isPositiveAndBelow (slot, kNumSlots));
    return samples[(size_t) slot];
}

void KickState::setSample (int slot, std::optional<SampleRef> sample)
{
    if (! juce::isPositiveAndBelow (slot, kNumSlots))
        return;

    samples[(size_t) slot] = std::move (sample);
    dirty = true;
}

void KickState::clearSample (int slot)
{
    setSample (slot, std::nullopt);
}

Role KickState::getRole (int slot) const noexcept
{
    return juce::isPositiveAndBelow (slot, kNumSlots) ? roles[(size_t) slot] : Role::none;
}

void KickState::setRole (int slot, Role role) noexcept
{
    if (juce::isPositiveAndBelow (slot, kNumSlots))
        roles[(size_t) slot] = role;
}

bool KickState::isLocked (int slot) const noexcept
{
    return juce::isPositiveAndBelow (slot, kNumSlots) && locked[(size_t) slot];
}

void KickState::setLocked (int slot, bool shouldBeLocked) noexcept
{
    if (juce::isPositiveAndBelow (slot, kNumSlots))
        locked[(size_t) slot] = shouldBeLocked;
}

bool KickState::isKeyTracked (int slot) const noexcept
{
    return juce::isPositiveAndBelow (slot, kNumSlots) && keyTracked[(size_t) slot];
}

void KickState::setKeyTracked (int slot, bool shouldTrack) noexcept
{
    if (juce::isPositiveAndBelow (slot, kNumSlots))
        keyTracked[(size_t) slot] = shouldTrack;
}

float KickState::getParam (const juce::String& id) const noexcept
{
    const auto& registry = ParameterRegistry::get();
    const auto& all = registry.all();

    for (size_t i = 0; i < all.size(); ++i)
        if (all[i].id == id)
            return paramValues[i];

    jassertfalse; // unbekannte ID
    return 0.0f;
}

void KickState::setParam (const juce::String& id, float normalisedValue) noexcept
{
    const auto& registry = ParameterRegistry::get();
    const auto& all = registry.all();

    for (size_t i = 0; i < all.size(); ++i)
    {
        if (all[i].id == id)
        {
            paramValues[i] = juce::jlimit (0.0f, 1.0f, normalisedValue);
            dirty = true;
            return;
        }
    }

    jassertfalse;
}

// ---------------------------------------------------------------------------
// Schema-Version und Migration (B-16)
// ---------------------------------------------------------------------------

int KickState::schemaVersionOf (const juce::ValueTree& tree) noexcept
{
    return (int) tree.getProperty (kSchemaVersion, 0);
}

void KickState::setSchemaVersion (juce::ValueTree& tree, int version)
{
    if (! tree.isValid())
        return;

    tree.setProperty (kSchemaVersion, version, nullptr);
}

juce::ValueTree KickState::migrateTo (juce::ValueTree tree, int targetVersion)
{
    // Nichts zu tun: ungueltiger Baum, keine Zielversion, oder der Baum ist
    // bereits auf oder ueber der Zielversion (dann bleibt seine Version).
    if (! tree.isValid() || targetVersion <= 0)
        return tree;

    // Eigene Kopie: `ValueTree` teilt seine Daten, ein direkter `setProperty`
    // wuerde den Baum des Aufrufers mitveraendern.
    juce::ValueTree copy = tree.createCopy();

    // Fehlende Version heisst v1 (B-16). Jede Stufe ergaenzt nur Fehlendes,
    // unbekannte Properties und Kinder bleiben unangetastet.
    const int fromVersion = juce::jmax (1, schemaVersionOf (copy));

    for (int version = fromVersion; version < targetVersion; ++version)
    {
        switch (version)
        {
            case 1:  migrateV1ToV2 (copy); break;
            default: break;   // kuenftige Stufen kommen hier hinein
        }
    }

    setSchemaVersion (copy, targetVersion);
    return copy;
}

// ---------------------------------------------------------------------------

juce::ValueTree KickState::toValueTree() const
{
    const auto& registry = ParameterRegistry::get();
    const auto& all = registry.all();

    juce::ValueTree root (kStateType);
    root.setProperty (kSchemaVersion, currentSchemaVersion, nullptr);
    root.setProperty ("styleId", styleId, nullptr);
    root.setProperty ("presetName", presetName, nullptr);
    root.setProperty ("bpm", bpmValue, nullptr);
    root.setProperty (kStretchMode, (int) stretch, nullptr);
    root.setProperty (kBpmMode, (int) bpm, nullptr);

    juce::ValueTree slots (kSlotsType);
    for (int slot = 0; slot < kNumSlots; ++slot)
    {
        juce::ValueTree s (kSlotType);
        s.setProperty ("index", slot, nullptr);
        s.setProperty ("role", (int) roles[(size_t) slot], nullptr);
        s.setProperty ("locked", locked[(size_t) slot], nullptr);
        s.setProperty ("keyTracked", keyTracked[(size_t) slot], nullptr);

        if (const auto& sample = samples[(size_t) slot]; sample.has_value())
        {
            s.setProperty ("relPath", sample->relPath, nullptr);
            s.setProperty ("absPath", sample->absPath, nullptr);
            s.setProperty ("fileName", sample->fileName, nullptr);
            s.setProperty ("roleId", sample->roleId, nullptr);
        }

        slots.appendChild (s, nullptr);
    }
    root.appendChild (slots, nullptr);

    juce::ValueTree params (kParamsType);
    for (size_t i = 0; i < all.size(); ++i)
    {
        juce::ValueTree p (kPType);
        p.setProperty ("id", all[i].id, nullptr);
        p.setProperty ("v", paramValues[i], nullptr);
        params.appendChild (p, nullptr);
    }
    root.appendChild (params, nullptr);

    // FX-Kette: Schema liegt in core/FxChainIO.cpp (B-143).
    root.appendChild (fxChainToValueTree (chain), nullptr);

    return root;
}

std::optional<KickState> KickState::fromValueTree (const juce::ValueTree& tree, juce::String& error)
{
    if (! tree.isValid() || ! tree.hasType (kStateType))
    {
        error = "ValueTree ist kein BFKA-Kick-Zustand.";
        return std::nullopt;
    }

    // B-16: alten Stand zuerst auf das aktuelle Schema heben. Fehlt die
    // Version, gilt der Baum als v1; unbekannte Felder bleiben erhalten.
    const auto migrated = migrateTo (tree, currentSchemaVersion);

    KickState state;
    state.styleId    = migrated.getProperty ("styleId", "free_build").toString();
    state.presetName = migrated.getProperty ("presetName", "").toString();
    state.bpmValue   = (double) migrated.getProperty ("bpm", 150.0);
    state.stretch    = (StretchMode) (int) migrated.getProperty (kStretchMode, 0);
    state.bpm        = (BpmMode)     (int) migrated.getProperty (kBpmMode, 0);

    const auto slots = migrated.getChildWithName (kSlotsType);
    for (int i = 0; i < slots.getNumChildren(); ++i)
    {
        const auto s = slots.getChild (i);
        const int slot = (int) s.getProperty ("index", i);

        if (! juce::isPositiveAndBelow (slot, kNumSlots))
            continue;

        state.roles[(size_t) slot]      = (Role) (int) s.getProperty ("role", 0);
        state.locked[(size_t) slot]     = (bool) s.getProperty ("locked", false);
        state.keyTracked[(size_t) slot] = (bool) s.getProperty ("keyTracked", false);

        const auto relPath = s.getProperty ("relPath", "").toString();
        const auto absPath = s.getProperty ("absPath", "").toString();

        if (relPath.isNotEmpty() || absPath.isNotEmpty())
        {
            SampleRef ref;
            ref.relPath  = relPath;
            ref.absPath  = absPath;
            ref.fileName = s.getProperty ("fileName", "").toString();
            ref.roleId   = s.getProperty ("roleId", "").toString();
            state.samples[(size_t) slot] = std::move (ref);
        }
    }

    const auto params = migrated.getChildWithName (kParamsType);
    for (int i = 0; i < params.getNumChildren(); ++i)
    {
        const auto p = params.getChild (i);
        const auto id = p.getProperty ("id", "").toString();
        if (id.isEmpty())
            continue;

        state.setParam (id, (float) (double) p.getProperty ("v", 0.0));
    }

    if (auto parsed = fxChainFromValueTree (migrated))
        state.chain = std::move (*parsed);

    state.dirty = false;
    error.clear();
    return state;
}

} // namespace bfka
