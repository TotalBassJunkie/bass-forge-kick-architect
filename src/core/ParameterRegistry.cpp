#include "core/ParameterRegistry.h"

#include <cmath>
#include <array>

namespace bfka
{

namespace
{
    constexpr float kStretchMin = 0.25f;
    constexpr float kStretchMax = 8.0f;

    /**
     * Exponent der Stretch-Kurve.

     * Aus den Werk-Presets gemessen: 1,0x (also unveraenderte Laenge) wird als
     * 0.374989420 gespeichert. Eine Logarithmus-Kurve ueber [0.25, 8] wuerde
     * 0.4 ergeben — das ist messbar falsch. Eine Potenzkurve ueber denselben
     * Bereich trifft den Wert mit exponent = 0.42 auf 6 Stellen genau.
     */
    constexpr float kStretchExponent = 0.42f;

    /** Logarithmische Abbildung mit sicherer Untergrenze. */
    float logMap (float normalised, float lo, float hi) noexcept
    {
        const auto n = juce::jlimit (0.0f, 1.0f, normalised);
        const auto a = std::log (juce::jmax (lo, 1.0e-6f));
        const auto b = std::log (juce::jmax (hi, lo * 1.0001f));
        return std::exp (a + (b - a) * n);
    }

    float logUnmap (float value, float lo, float hi) noexcept
    {
        const auto a = std::log (juce::jmax (lo, 1.0e-6f));
        const auto b = std::log (juce::jmax (hi, lo * 1.0001f));
        const auto v = juce::jlimit (a, b, std::log (juce::jmax (value, 1.0e-6f)));
        return (v - a) / (b - a);
    }

    /** Potenzkurve: norm 0..1 -> eng lo..hi, mit fester Biegeexponent. */
    float powerMap (float normalised, float lo, float hi, float exponent) noexcept
    {
        const auto n = juce::jlimit (0.0f, 1.0f, normalised);
        return lo + (hi - lo) * std::pow (n, 1.0f / juce::jmax (0.01f, exponent));
    }

    float powerUnmap (float value, float lo, float hi, float exponent) noexcept
    {
        const auto span = hi - lo;
        if (std::abs (span) < 1.0e-9f)
            return 0.0f;

        const auto t = juce::jlimit (0.0f, 1.0f, (value - lo) / span);
        return std::pow (t, exponent);
    }
}

float ParamDesc::toEngineering (float normalised) const noexcept
{
    const auto n = juce::jlimit (0.0f, 1.0f, normalised);

    switch (curve)
    {
        case ParamCurve::linear:
        case ParamCurve::decibel:
        case ParamCurve::semitones:
        case ParamCurve::percent:
            return engMin + (engMax - engMin) * n;

        case ParamCurve::frequency:
            return logMap (n, engMin, engMax);

        case ParamCurve::stretch:
            return powerMap (n, engMin, engMax, kStretchExponent);

        case ParamCurve::boolean:
            return n >= 0.5f ? engMax : engMin;
    }

    return engMin + (engMax - engMin) * n;
}

float ParamDesc::toNormalised (float engineering) const noexcept
{
    switch (curve)
    {
        case ParamCurve::linear:
        case ParamCurve::decibel:
        case ParamCurve::semitones:
        case ParamCurve::percent:
            break;

        case ParamCurve::frequency:
            return juce::jlimit (0.0f, 1.0f, logUnmap (engineering, engMin, engMax));

        case ParamCurve::stretch:
            return juce::jlimit (0.0f, 1.0f,
                                 powerUnmap (engineering, engMin, engMax, kStretchExponent));

        case ParamCurve::boolean:
            return engineering >= (engMin + engMax) * 0.5f ? 1.0f : 0.0f;
    }

    const auto span = engMax - engMin;
    if (std::abs (span) < 1.0e-9f)
        return 0.0f;

    return juce::jlimit (0.0f, 1.0f, (engineering - engMin) / span);
}

juce::String ParamDesc::format (float normalised) const
{
    const auto v = toEngineering (normalised);

    if (curve == ParamCurve::boolean)
        return v >= (engMin + engMax) * 0.5f ? "ON" : "OFF";

    juce::String text;
    if (curve == ParamCurve::semitones && v > 0.0f)
        text << "+";

    text << juce::String (v, decimals);

    if (unit.isNotEmpty())
        text << " " << unit;

    return text;
}

// ---------------------------------------------------------------------------

ParameterRegistry::ParameterRegistry()
{
    params.reserve ((size_t) (numSlots() * numSlotParams()
                              + numGlobalParams()
                              + numFxSlots() * numFxParams()));

    // --- Slot-Parameter ----------------------------------------------------
    // Reihenfolge ist bewusst stabil: sie definiert die Serialisierung.
    struct SlotParamDef
    {
        const char* suffix;
        const char* name;
        ParamCurve  curve;
        float       min, max, def;
        int         decimals;
        const char* unit;
    };

    static const std::array<SlotParamDef, 20> slotDefs {{
        { "vol",       "Volume",      ParamCurve::decibel,   -60.0f,  10.0f,   0.0f, 2, "dB" },
        { "pitch",     "Pitch",       ParamCurve::semitones, -24.0f,  24.0f,   0.0f, 2, "st" },
        { "mute",      "Mute",        ParamCurve::boolean,     0.0f,   1.0f,   0.0f, 0, nullptr },
        { "solo",      "Solo",        ParamCurve::boolean,     0.0f,   1.0f,   0.0f, 0, nullptr },
        { "pos",       "Position",    ParamCurve::percent,     0.0f, 100.0f,  50.0f, 1, "%" },
        { "trimstart", "Trim Start",  ParamCurve::percent,     0.0f, 100.0f,   0.0f, 1, "%" },
        { "trimend",   "Trim End",    ParamCurve::percent,     0.0f, 100.0f, 100.0f, 1, "%" },
        { "fadein",    "Fade In",     ParamCurve::percent,     0.0f, 100.0f,   0.0f, 1, "%" },
        { "fadeout",   "Fade Out",    ParamCurve::percent,     0.0f, 100.0f,   0.0f, 1, "%" },
        { "stretch",   "Stretch",     ParamCurve::stretch,  kStretchMin, kStretchMax, 1.0f, 3, "x" },
        { "filter",    "Filter",      ParamCurve::frequency,  20.0f, 20000.0f, 20000.0f, 1, "Hz" },
        { "filterq",   "Filter Q",    ParamCurve::linear,      0.1f,  10.0f,   0.7f, 2, nullptr },
        { "dlytime",   "Delay Time",  ParamCurve::percent,     0.0f, 100.0f,  30.0f, 1, "%" },
        { "dlymix",    "Delay Mix",   ParamCurve::percent,     0.0f, 100.0f,   0.0f, 1, "%" },
        { "spare1",    "Reserve 1",   ParamCurve::percent,     0.0f, 100.0f,  50.0f, 1, "%" },
        { "spare2",    "Reserve 2",   ParamCurve::percent,     0.0f, 100.0f,  50.0f, 1, "%" },
        { "ficurve",   "Fade In Curve",  ParamCurve::percent,  0.0f, 100.0f,  50.0f, 1, "%" },
        { "fishape",   "Fade In Shape",  ParamCurve::linear,  -1.0f,   1.0f,   0.0f, 3, nullptr },
        { "foshape",   "Fade Out Shape", ParamCurve::linear,  -1.0f,   1.0f,   0.0f, 3, nullptr },
        // Erst in der neuesten Preset-Generation vorhanden; steht dort hinter
        // foshape. Bedeutung: Startpunkt des Fade-In-Ankers (siehe Handbuch,
        // "Fade anchors"). Hypothese, in B-11 zu verifizieren.
        { "fistart",   "Fade In Start",  ParamCurve::percent,   0.0f, 100.0f,   0.0f, 1, "%" },
    }};

    for (int slot = 0; slot < numSlots(); ++slot)
    {
        for (int i = 0; i < (int) slotDefs.size(); ++i)
        {
            const auto& d = slotDefs[(size_t) i];
            ParamDesc p;
            p.id          = "l" + juce::String (slot) + "_" + d.suffix;
            p.displayName = d.name;
            p.domain      = ParamDomain::slot;
            p.slot        = slot;
            p.index       = i;
            p.curve       = d.curve;
            p.engMin      = d.min;
            p.engMax      = d.max;
            p.engDefault  = d.def;
            p.decimals    = d.decimals;
            p.unit        = d.unit != nullptr ? juce::String (d.unit) : juce::String();
            params.push_back (std::move (p));
        }
    }

    // --- Globals -----------------------------------------------------------
    struct GlobalParamDef
    {
        const char* id;
        const char* name;
        ParamCurve  curve;
        float       min, max, def;
        int         decimals;
        const char* unit;
    };

    static const std::array<GlobalParamDef, 12> globalDefs {{
        { "sat_amount",   "Saturation",    ParamCurve::percent,   0.0f, 100.0f,  0.0f, 1, "%" },
        { "reverb_size",  "Reverb Size",   ParamCurve::percent,   0.0f, 100.0f, 50.0f, 1, "%" },
        { "reverb_damp",  "Reverb Damp",   ParamCurve::percent,   0.0f, 100.0f, 50.0f, 1, "%" },
        { "reverb_mix",   "Reverb Mix",    ParamCurve::percent,   0.0f, 100.0f,  0.0f, 1, "%" },
        { "hollow_amt",   "Hollow",        ParamCurve::percent,   0.0f, 100.0f,  0.0f, 1, "%" },
        { "max_amount",   "Maximizer",     ParamCurve::percent,   0.0f, 100.0f,  0.0f, 1, "%" },
        { "master_vol",   "Master",        ParamCurve::decibel, -60.0f,   6.0f,  0.0f, 2, "dB" },
        { "midi_mode",    "MIDI Mode",     ParamCurve::linear,    0.0f,   3.0f,  0.0f, 0, nullptr },
        { "root_note",    "Root Note",     ParamCurve::linear,    0.0f, 127.0f, 36.0f, 0, nullptr },
        { "gspare1",      "Global Reserve 1", ParamCurve::percent, 0.0f, 100.0f, 0.0f, 1, "%" },
        { "gspare2",      "Global Reserve 2", ParamCurve::percent, 0.0f, 100.0f, 50.0f, 1, "%" },
        { "gspare3",      "Global Reserve 3", ParamCurve::percent, 0.0f, 100.0f, 50.0f, 1, "%" },
    }};

    for (int i = 0; i < (int) globalDefs.size(); ++i)
    {
        const auto& d = globalDefs[(size_t) i];
        ParamDesc p;
        p.id          = d.id;
        p.displayName = d.name;
        p.domain      = ParamDomain::global;
        p.slot        = -1;
        p.index       = i;
        p.curve       = d.curve;
        p.engMin      = d.min;
        p.engMax      = d.max;
        p.engDefault  = d.def;
        p.decimals    = d.decimals;
        p.unit        = d.unit != nullptr ? juce::String (d.unit) : juce::String();
        params.push_back (std::move (p));
    }

    // --- FX-Proxy-Parameter (8 Slots x 14) ---------------------------------
    for (int fxSlot = 0; fxSlot < numFxSlots(); ++fxSlot)
    {
        for (int i = 0; i < numFxParams(); ++i)
        {
            ParamDesc p;
            p.id          = "fx" + juce::String (fxSlot) + "_p" + juce::String (i);
            p.displayName = "FX " + juce::String (fxSlot + 1) + " P" + juce::String (i);
            p.domain      = ParamDomain::fx;
            p.slot        = fxSlot;
            p.index       = i;
            p.curve       = ParamCurve::percent;
            p.engMin      = 0.0f;
            p.engMax      = 100.0f;
            p.engDefault  = 50.0f;
            p.decimals    = 1;
            p.unit        = "%";
            params.push_back (std::move (p));
        }
    }

    // --- Index der Slot-Bloecke -------------------------------------------
    slotIndexStart.resize ((size_t) numSlots(), 0);
    for (int slot = 0; slot < numSlots(); ++slot)
        slotIndexStart[(size_t) slot] = slot * numSlotParams();
}

const ParameterRegistry& ParameterRegistry::get()
{
    static const ParameterRegistry instance;
    return instance;
}

const ParamDesc* ParameterRegistry::find (const juce::String& id) const noexcept
{
    for (const auto& p : params)
        if (p.id == id)
            return &p;

    return nullptr;
}

std::vector<const ParamDesc*> ParameterRegistry::forSlot (int slotIndex) const
{
    std::vector<const ParamDesc*> result;
    if (slotIndex < 0 || slotIndex >= numSlots())
        return result;

    const auto start = (size_t) (slotIndex * numSlotParams());
    const auto end   = juce::jmin (params.size(), start + (size_t) numSlotParams());

    result.reserve (end - start);
    for (auto i = start; i < end; ++i)
        result.push_back (&params[i]);

    return result;
}

} // namespace bfka
