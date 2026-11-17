#pragma once

#include <algorithm>
#include <cmath>

/**
 * Kleine, header-only DSP-Bausteine fuer die neuen FX-Module (Phase 2).
 *
 * Bewusst frei von JUCE-Abhaengigkeiten (ausser dem, was die Module selbst
 * einbinden), damit die Formeln testbar und die Module untereinander
 * konsistent sind.
 */
namespace bfka::dsp
{

inline constexpr double kPi = 3.14159265358979323846;

/** dB -> linearer Faktor. */
inline double dbToGain (double db) noexcept
{
    return std::pow (10.0, db / 20.0);
}

/** linearer Faktor -> dB (mit Untergrenze gegen -inf). */
inline double gainToDb (double gain) noexcept
{
    return 20.0 * std::log10 (std::max (gain, 1.0e-12));
}

/** Filtertypen, deren Koeffizienten `designBiquad()` kennt (RBJ-Cookbook). */
enum class BiquadKind : int
{
    peaking = 0,
    lowShelf,
    highShelf,
    lowPass,
    highPass,
    bandPass,
    notch
};

/** Direkt formte Koeffizienten (bereits durch a0 geteilt). */
struct BiquadCoeffs
{
    double b0 = 1.0;
    double b1 = 0.0;
    double b2 = 0.0;
    double a1 = 0.0;
    double a2 = 0.0;
};

/**
 * RBJ-Cookbook-Design fuer alle Typen.
 *
 * `q` ist fuer Shelves der RBJ-Shelf-Slope-Parameter (0.5 = 1 Oktave,
 * 0.7071 = Butterworth-Kniewinkel), fuer Bandtypen die Guete.
 */
inline BiquadCoeffs designBiquad (BiquadKind kind,
                                  double sampleRate,
                                  double freqHz,
                                  double q,
                                  double gainDb) noexcept
{
    const double nyquist = 0.5 * sampleRate;
    const double f       = std::clamp (freqHz, 10.0, nyquist * 0.98);
    const double qq      = std::max (q, 0.05);

    const double w0    = 2.0 * kPi * f / sampleRate;
    const double cosw  = std::cos (w0);
    const double sinw  = std::sin (w0);
    const double alpha = sinw / (2.0 * qq);
    const double a     = std::pow (10.0, gainDb / 40.0);

    double b0 = 1.0, b1 = 0.0, b2 = 0.0;
    double a0 = 1.0, a1 = 0.0, a2 = 0.0;

    switch (kind)
    {
        case BiquadKind::peaking:
            b0 = 1.0 + alpha * a;
            b1 = -2.0 * cosw;
            b2 = 1.0 - alpha * a;
            a0 = 1.0 + alpha / a;
            a1 = -2.0 * cosw;
            a2 = 1.0 - alpha / a;
            break;

        case BiquadKind::lowShelf:
        {
            const double twoSqrtAlpha = 2.0 * std::sqrt (a) * alpha;
            b0 = a * ((a + 1.0) - (a - 1.0) * cosw + twoSqrtAlpha);
            b1 = 2.0 * a * ((a - 1.0) - (a + 1.0) * cosw);
            b2 = a * ((a + 1.0) - (a - 1.0) * cosw - twoSqrtAlpha);
            a0 = (a + 1.0) + (a - 1.0) * cosw + twoSqrtAlpha;
            a1 = -2.0 * ((a - 1.0) + (a + 1.0) * cosw);
            a2 = (a + 1.0) + (a - 1.0) * cosw - twoSqrtAlpha;
            break;
        }

        case BiquadKind::highShelf:
        {
            const double twoSqrtAlpha = 2.0 * std::sqrt (a) * alpha;
            b0 = a * ((a + 1.0) + (a - 1.0) * cosw + twoSqrtAlpha);
            b1 = -2.0 * a * ((a - 1.0) + (a + 1.0) * cosw);
            b2 = a * ((a + 1.0) + (a - 1.0) * cosw - twoSqrtAlpha);
            a0 = (a + 1.0) - (a - 1.0) * cosw + twoSqrtAlpha;
            a1 = 2.0 * ((a - 1.0) - (a + 1.0) * cosw);
            a2 = (a + 1.0) - (a - 1.0) * cosw - twoSqrtAlpha;
            break;
        }

        case BiquadKind::lowPass:
            b0 = (1.0 - cosw) * 0.5;
            b1 = 1.0 - cosw;
            b2 = (1.0 - cosw) * 0.5;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cosw;
            a2 = 1.0 - alpha;
            break;

        case BiquadKind::highPass:
            b0 = (1.0 + cosw) * 0.5;
            b1 = -(1.0 + cosw);
            b2 = (1.0 + cosw) * 0.5;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cosw;
            a2 = 1.0 - alpha;
            break;

        case BiquadKind::bandPass:
            // konstante 0-dB-Spitze (Audio-EQ-Variante)
            b0 = alpha;
            b1 = 0.0;
            b2 = -alpha;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cosw;
            a2 = 1.0 - alpha;
            break;

        case BiquadKind::notch:
            b0 = 1.0;
            b1 = -2.0 * cosw;
            b2 = 1.0;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cosw;
            a2 = 1.0 - alpha;
            break;
    }

    const double invA0 = 1.0 / a0;

    return { b0 * invA0, b1 * invA0, b2 * invA0, a1 * invA0, a2 * invA0 };
}

/** Zustand einer Transposed-Direct-Form-II-Stufe (double). */
struct BiquadState
{
    double z1 = 0.0;
    double z2 = 0.0;

    void reset() noexcept { z1 = 0.0; z2 = 0.0; }
};

/** Eine Biquad-Stufe in TDF2 mit double-Koeffizienten und -Zustand. */
class Biquad
{
public:
    void setCoefficients (const BiquadCoeffs& newCoeffs) noexcept { coeffs = newCoeffs; }
    const BiquadCoeffs& getCoefficients() const noexcept { return coeffs; }

    void reset() noexcept { state.reset(); }

    float processSample (float x) noexcept
    {
        return (float) processSampleDouble ((double) x);
    }

    double processSampleDouble (double x) noexcept
    {
        const double y = coeffs.b0 * x + state.z1;
        state.z1 = coeffs.b1 * x - coeffs.a1 * y + state.z2;
        state.z2 = coeffs.b2 * x - coeffs.a2 * y;
        return y;
    }

private:
    BiquadCoeffs coeffs;
    BiquadState  state;
};

/**
 * Huellkurven-Follower (Peak, einpolig) mit getrennten Attack-/Release-Zeiten.
 */
class EnvelopeFollower
{
public:
    void prepare (double newSampleRate) noexcept
    {
        sampleRate = (newSampleRate > 0.0) ? newSampleRate : 48000.0;
        updateCoefficients();
    }

    void setTimes (double attackMsToUse, double releaseMsToUse) noexcept
    {
        attackMs  = std::max (0.01, attackMsToUse);
        releaseMs = std::max (0.01, releaseMsToUse);
        updateCoefficients();
    }

    void reset (double initialValue = 0.0) noexcept { env = std::max (0.0, initialValue); }

    double value() const noexcept { return env; }

    /** Verarbeitet einen Sample-Wert und liefert die aktuelle Huelle. */
    double process (double x) noexcept
    {
        const double level = std::abs (x);
        const double coeff = (level > env) ? attackCoeff : releaseCoeff;

        env = level + coeff * (env - level);
        return env;
    }

private:
    void updateCoefficients() noexcept
    {
        attackCoeff  = std::exp (-1.0 / (std::max (0.01, attackMs)  * 0.001 * sampleRate));
        releaseCoeff = std::exp (-1.0 / (std::max (0.01, releaseMs) * 0.001 * sampleRate));
    }

    double sampleRate   = 48000.0;
    double attackMs     = 10.0;
    double releaseMs    = 100.0;
    double attackCoeff  = 0.0;
    double releaseCoeff = 0.0;
    double env          = 0.0;
};

/** Einpoliger Glaetter fuer Parameter (pro Block aufgerufen). */
inline double smoothTowards (double current, double target, double coefficient) noexcept
{
    return current + (target - current) * coefficient;
}

} // namespace bfka::dsp
