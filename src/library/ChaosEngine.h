// B-75: CHAOS-Engine -- deterministischer Wuerfel fuer Pitch, Stretch,
// FX-Kette und FX-Module. Gleicher Seed + gleicher Zustand = gleiches Ergebnis.
#pragma once

#include "core/KickState.h"
#include "library/RandomizerEngine.h"

#include <cstdint>
#include <random>

namespace bfka
{

/** Welche Bereiche CHAOS wuerfelt (B-75). */
struct ChaosFlags
{
    bool pitch     = true;
    bool stretch   = true;
    bool fxChain   = true;
    bool fxModules = true;
};

/**
 * Deterministischer CHAOS-Wuerfel (Ticket B-75).
 * Gleicher Seed + gleicher Eingangszustand = gleiches Ergebnis.
 * Nur Message-Thread; darf allokieren.
 */
class ChaosEngine
{
public:
    ChaosEngine (RandomizerEngine& engine, std::uint64_t seedToUse);

    void setFlags (ChaosFlags newFlags) noexcept { flags_ = newFlags; }
    ChaosFlags flags() const noexcept { return flags_; }
    std::uint64_t seed() const noexcept { return seedValue; }

    /** Aendert den Zustand anhand der Flags; liefert die Zahl der Aenderungen. */
    int applyTo (KickState& state);
    int applyTo (KickState& state, ChaosFlags useFlags);

private:
    RandomizerEngine& engine;
    std::uint64_t seedValue;
    ChaosFlags flags_;
    std::mt19937_64 rng;
    static constexpr double kMaxPitchSemitones = 3.0;
};

} // namespace bfka
