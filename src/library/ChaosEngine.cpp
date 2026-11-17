// B-75: CHAOS-Engine -- deterministischer Wuerfel fuer Pitch, Stretch,
// FX-Kette und FX-Module.
#include "library/ChaosEngine.h"

#include <juce_core/juce_core.h>

#include <algorithm>
#include <cstddef>

namespace bfka
{

namespace
{
    /** Bereich der Pitch-Parameter in Halbtoenen (linear, siehe Registry). */
    constexpr double kPitchEngMin = -24.0;
    constexpr double kPitchEngMax =  24.0;

    /** Spanne des Pitch-Parameters in Halbtoenen. */
    constexpr double kPitchSpan = kPitchEngMax - kPitchEngMin;

    /** Groesster Stretch-Schritt pro Wurf. */
    constexpr double kMaxStretchStep = 0.05;

    /** Groesster Mix-Schritt pro FX-Modul. */
    constexpr double kMaxMixStep = 0.15;

    /** Obergrenze der ein-/ausgeschalteten Module je Wurf. */
    constexpr int kMaxModuleToggles = 2;

    /** Normalisierter Wert -> Halbtoene. */
    double normToSemitones (double normalised) noexcept
    {
        return normalised * kPitchSpan + kPitchEngMin;
    }

    /** Halbtoene -> normalisierter Wert (geklemmt auf 0..1). */
    double semitonesToNorm (double semitones) noexcept
    {
        const auto n = (semitones - kPitchEngMin) / kPitchSpan;
        return std::clamp (n, 0.0, 1.0);
    }

    /** Nur "eq" und "dist" bekommen einen Mix-Wurf. */
    bool isMixTarget (const juce::String& type) noexcept
    {
        return type == "eq" || type == "dist";
    }
} // namespace

// ---------------------------------------------------------------------------

ChaosEngine::ChaosEngine (RandomizerEngine& engineToUse, std::uint64_t seedToUse)
    : engine (engineToUse),
      seedValue (seedToUse),
      flags_ {},
      rng (seedToUse)
{
    // Der Vertrag verlangt die Referenz; CHAOS nutzt die Engine nicht.
    juce::ignoreUnused (engine);
}

int ChaosEngine::applyTo (KickState& state)
{
    return applyTo (state, flags_);
}

int ChaosEngine::applyTo (KickState& state, ChaosFlags useFlags)
{
    // Jeder Aufruf beginnt bei demselben RNG-Zustand: gleicher Seed und
    // gleicher Eingangszustand liefern damit exakt dasselbe Ergebnis.
    rng.seed (seedValue);

    int changes = 0;

    // --- 1. Pitch je Slot ---------------------------------------------------
    if (useFlags.pitch)
    {
        std::uniform_real_distribution<double> dist (-kMaxPitchSemitones,
                                                     kMaxPitchSemitones);

        for (int slot = 0; slot < KickState::kNumSlots; ++slot)
        {
            const juce::String id = "l" + juce::String (slot) + "_pitch";

            const double currentNorm = (double) state.getParam (id);
            const double currentEng  = normToSemitones (currentNorm);
            const double delta       = dist (rng);

            const double newEng = std::clamp (currentEng + delta,
                                              kPitchEngMin, kPitchEngMax);

            state.setParam (id, (float) semitonesToNorm (newEng));
            ++changes;
        }
    }

    // --- 2. Stretch je Slot -------------------------------------------------
    if (useFlags.stretch)
    {
        std::uniform_real_distribution<double> dist (-kMaxStretchStep,
                                                     kMaxStretchStep);

        for (int slot = 0; slot < KickState::kNumSlots; ++slot)
        {
            const juce::String id = "l" + juce::String (slot) + "_stretch";

            const double currentNorm = (double) state.getParam (id);
            const double newNorm     = std::clamp (currentNorm + dist (rng),
                                                   0.0, 1.0);

            state.setParam (id, (float) newNorm);
            ++changes;
        }
    }

    // --- 3. FX-Kette: Module ein-/ausschalten -------------------------------
    if (useFlags.fxChain)
    {
        auto& modules = state.fxChain().modules;
        const int count = (int) modules.size();

        if (count > 0)
        {
            std::uniform_int_distribution<int> pickModule (0, count - 1);
            std::uniform_int_distribution<int> pickToggles (0, kMaxModuleToggles);

            const int toggles = pickToggles (rng);

            for (int i = 0; i < toggles; ++i)
            {
                auto& module = modules[(size_t) pickModule (rng)];
                module.enabled = ! module.enabled;
                ++changes;
            }
        }
    }

    // --- 4. FX-Module: MIX leicht verschieben -------------------------------
    if (useFlags.fxModules)
    {
        auto& modules = state.fxChain().modules;

        std::uniform_real_distribution<double> dist (-kMaxMixStep, kMaxMixStep);

        for (auto& module : modules)
        {
            if (! isMixTarget (module.type))
                continue;

            const double currentMix = (double) module.mix;
            module.mix = (float) std::clamp (currentMix + dist (rng), 0.0, 1.0);
            ++changes;
        }
    }

    return changes;
}

} // namespace bfka
