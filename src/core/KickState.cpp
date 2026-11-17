#include "core/KickState.h"
#include "core/ParameterRegistry.h"

namespace bfka
{

namespace
{
    const juce::Identifier kStateType { "BFKAKick" };
    const juce::Identifier kSlotsType { "Slots" };
    const juce::Identifier kSlotType  { "Slot" };
    const juce::Identifier kParamsType{ "Params" };
    const juce::Identifier kFxType    { "FxChain" };
    const juce::Identifier kPType     { "P" };
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

juce::ValueTree KickState::toValueTree() const
{
    const auto& registry = ParameterRegistry::get();
    const auto& all = registry.all();

    juce::ValueTree root (kStateType);
    root.setProperty ("schemaVersion", 1, nullptr);
    root.setProperty ("styleId", styleId, nullptr);
    root.setProperty ("presetName", presetName, nullptr);
    root.setProperty ("bpm", bpmValue, nullptr);
    root.setProperty ("stretchMode", (int) stretch, nullptr);
    root.setProperty ("bpmMode", (int) bpm, nullptr);

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

    juce::ValueTree fx (kFxType);
    fx.setProperty ("bypassAll", chain.bypassAll, nullptr);
    for (const auto& m : chain.modules)
    {
        juce::ValueTree module ("Module");
        module.setProperty ("type", m.type, nullptr);
        module.setProperty ("on", m.enabled, nullptr);
        module.setProperty ("mix", m.mix, nullptr);
        module.setProperty ("outGain", m.outGain, nullptr);
        if (m.data.isValid())
            module.appendChild (m.data.createCopy(), nullptr);
        fx.appendChild (module, nullptr);
    }
    root.appendChild (fx, nullptr);

    return root;
}

std::optional<KickState> KickState::fromValueTree (const juce::ValueTree& tree, juce::String& error)
{
    if (! tree.isValid() || ! tree.hasType (kStateType))
    {
        error = "ValueTree ist kein BFKA-Kick-Zustand.";
        return std::nullopt;
    }

    KickState state;
    state.styleId    = tree.getProperty ("styleId", "free_build").toString();
    state.presetName = tree.getProperty ("presetName", "").toString();
    state.bpmValue   = (double) tree.getProperty ("bpm", 150.0);
    state.stretch    = (StretchMode) (int) tree.getProperty ("stretchMode", 0);
    state.bpm        = (BpmMode)     (int) tree.getProperty ("bpmMode", 0);

    const auto slots = tree.getChildWithName (kSlotsType);
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

    const auto params = tree.getChildWithName (kParamsType);
    for (int i = 0; i < params.getNumChildren(); ++i)
    {
        const auto p = params.getChild (i);
        const auto id = p.getProperty ("id", "").toString();
        if (id.isEmpty())
            continue;

        state.setParam (id, (float) (double) p.getProperty ("v", 0.0));
    }

    const auto fx = tree.getChildWithName (kFxType);
    if (fx.isValid())
    {
        state.chain.bypassAll = (bool) fx.getProperty ("bypassAll", false);
        for (int i = 0; i < fx.getNumChildren(); ++i)
        {
            const auto m = fx.getChild (i);
            FxModuleState module;
            module.type    = m.getProperty ("type", "").toString();
            module.enabled = (bool) m.getProperty ("on", true);
            module.mix     = (float) (double) m.getProperty ("mix", 1.0);
            module.outGain = (float) (double) m.getProperty ("outGain", 1.0);

            if (m.getNumChildren() > 0)
                module.data = m.getChild (0).createCopy();

            if (module.type.isNotEmpty())
                state.chain.modules.push_back (std::move (module));
        }
    }

    state.dirty = false;
    error.clear();
    return state;
}

} // namespace bfka
