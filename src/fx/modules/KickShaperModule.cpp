#include "fx/modules/KickShaperModule.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "fx/dsp/Biquad.h"

namespace bfka
{

namespace
{
    /** Parametertabelle: Reihenfolge = params()-Reihenfolge = XML-Reihenfolge. */
    const std::vector<FxParamDesc>& descriptorTable()
    {
        static const std::vector<FxParamDesc> table
        {
            { "sPre",   "Pre Gain",  0.0f,                                     3.5f,  1.0f,   "x" },
            { "sPost",  "Post Gain", 0.0f,                                     3.5f,  1.0f,   "x" },
            { "sLine",  "Linear",    0.0f,                                     1.0f,  1.0f,   ""  },
            { "shape",  "Shape",     0.0f, (float) (KickShaperModule::kNumShapes - 1), 0.0f, "" },
            { "amount", "Amount",    0.0f,                                   100.0f, 100.0f, "%" }
        };

        return table;
    }

    /**
     * Werks-Huellkurve: zwei Punkte mit `y = 1` - also der neutrale Faktor
     * ueber die ganze Kick-Laenge. Damit ist das Modul ohne `<Pt>`-Kinder
     * klanglich neutral (Spec Abschnitt 4.2).
     */
    std::vector<KickShaperModule::Pt> makeDefaultEnvelope()
    {
        return { { 0.0, 1.0, 0 }, { 1.0, 1.0, 0 } };
    }

    /**
     * Zahl fuer ein XML-Attribut: ganzzahlige Werte als Integer, sonst auf 6
     * Nachkommastellen gerundet (vermeidet Float-Rauschen wie 0.300000011...).
     */
    juce::var toXmlNumber (double value)
    {
        const auto rounded = std::round (value * 1.0e6) / 1.0e6;

        if (std::abs (rounded - std::round (rounded)) < 1.0e-9 && std::abs (rounded) < 2.0e9)
            return juce::var ((int) std::lround (rounded));

        return juce::var (rounded);
    }
} // namespace

// ===========================================================================
// Konstruktion
// ===========================================================================

KickShaperModule::KickShaperModule()
{
    const auto& table = descriptorTable();

    for (size_t i = 0; i < values.size() && i < table.size(); ++i)
        values[i] = table[i].defaultValue;

    points = makeDefaultEnvelope();
    rebuildPath();

    publishTargets();
    resetInternal();
}

// ===========================================================================
// Parameter
// ===========================================================================

const std::vector<FxParamDesc>& KickShaperModule::params() const noexcept
{
    return descriptorTable();
}

void KickShaperModule::setParam (int index, float value)
{
    if (index < 0 || index >= (int) values.size())
        return;

    // NaN/Inf nicht uebernehmen (wuerde die Kette vergiften).
    if (! std::isfinite (value))
        return;

    const auto& desc = descriptorTable()[(size_t) index];

    values[(size_t) index] = juce::jlimit (desc.minValue, desc.maxValue, value);
    publishTargets();
}

float KickShaperModule::getParam (int index) const
{
    if (index < 0 || index >= (int) values.size())
        return 0.0f;

    return values[(size_t) index];
}

void KickShaperModule::publishTargets() noexcept
{
    targetPreGain.store  (values[(size_t) idxSPre],  std::memory_order_relaxed);
    targetPostGain.store (values[(size_t) idxSPost], std::memory_order_relaxed);

    // "Linear" ist ein Schalter: 1 = Sicherung an, 0 = Umformung aktiv.
    targetLinear.store (values[(size_t) idxSLine] >= 0.5f, std::memory_order_relaxed);

    targetShape.store (juce::jlimit (0, kNumShapes - 1,
                                     (int) std::lround (values[(size_t) idxShape])),
                       std::memory_order_relaxed);

    targetAmount.store (values[(size_t) idxAmount] * 0.01f, std::memory_order_relaxed);
}

// ===========================================================================
// Vorbereitung / Reset
// ===========================================================================

void KickShaperModule::prepare (double sampleRate, int maxBlockSize)
{
    preparedSampleRate = (sampleRate > 0.0) ? sampleRate : 48000.0;
    preparedBlockSize  = juce::jmax (1, maxBlockSize);

    // Enthaelt reset(): Uhr, Trigger und Glaettung starten bei jedem prepare()
    // frisch, damit sich nichts aus einem alten Zustand fortschreibt.
    resetInternal();
}

void KickShaperModule::reset()
{
    resetInternal();
}

void KickShaperModule::resetInternal() noexcept
{
    envelopePhase = 0.0;
    triggerActive = false;

    // Glaettung auf die aktuellen Ziele setzen: nach prepare()/reset() gibt es
    // keine Einlauf-Rampe, und ein Test mit amount == 0 ist ab dem ersten
    // Sample bitgenau.
    smoothedPreGain  = juce::jlimit (kGainMin, kGainMax,
                                     targetPreGain.load (std::memory_order_relaxed));
    smoothedPostGain = juce::jlimit (kGainMin, kGainMax,
                                     targetPostGain.load (std::memory_order_relaxed));
    smoothedAmount   = juce::jlimit (0.0f, 1.0f,
                                     targetAmount.load (std::memory_order_relaxed));

    updateCoefficients();

    diagPosition.store (0.0f, std::memory_order_relaxed);
    diagEnvelope.store (1.0f, std::memory_order_relaxed);
    diagAmount.store (smoothedAmount, std::memory_order_relaxed);
    diagPreGain.store (smoothedPreGain, std::memory_order_relaxed);
    diagPostGain.store (smoothedPostGain, std::memory_order_relaxed);
    diagTrigger.store (false, std::memory_order_relaxed);
}

void KickShaperModule::updateCoefficients() noexcept
{
    paramSmoothCoeff = onePoleCoefficient (kParamSmoothMs, preparedSampleRate);
}

float KickShaperModule::onePoleCoefficient (float timeMs, double sampleRate) noexcept
{
    if (timeMs <= 0.0f || sampleRate <= 0.0)
        return 1.0f;

    const auto samples = (double) timeMs * 0.001 * sampleRate;

    // Sehr kurze Zeitkonstanten (unter einem Sample) sind "sofort".
    if (samples <= 1.0)
        return 1.0f;

    return (float) (1.0 - std::exp (-1.0 / samples));
}

// ===========================================================================
// Audio
// ===========================================================================

void KickShaperModule::process (juce::AudioBuffer<float>& buffer)
{
    const auto numChannels = buffer.getNumChannels();
    const auto numSamples  = buffer.getNumSamples();

    if (numChannels <= 0 || numSamples <= 0)
        return;

    // Parameterziele einmal pro Block lesen (atomar, ohne Sperre).
    const auto newPreGain  = juce::jlimit (kGainMin, kGainMax,
                                           targetPreGain.load (std::memory_order_relaxed));
    const auto newPostGain = juce::jlimit (kGainMin, kGainMax,
                                           targetPostGain.load (std::memory_order_relaxed));
    const auto newAmount   = juce::jlimit (0.0f, 1.0f,
                                           targetAmount.load (std::memory_order_relaxed));
    const auto shapeMode   = juce::jlimit (0, kNumShapes - 1,
                                           targetShape.load (std::memory_order_relaxed));
    const auto linearSafe  = targetLinear.load (std::memory_order_relaxed);

    const auto kickLengthSeconds = juce::jlimit (kMinRenderLengthSeconds, kMaxRenderLengthSeconds,
                                                 lengthSeconds.load (std::memory_order_relaxed));

    // Ein Sample = ein Schritt der Huellkurven-Uhr (Kick-Laenge in Sekunden).
    const auto phaseIncrement = (kickLengthSeconds > 0.0 && preparedSampleRate > 0.0)
                                    ? 1.0 / (kickLengthSeconds * preparedSampleRate)
                                    : 1.0;

    const auto smoothCoeff = paramSmoothCoeff;

    auto preGain  = smoothedPreGain;
    auto postGain = smoothedPostGain;
    auto amount   = smoothedAmount;
    auto phase    = envelopePhase;
    auto triggered = triggerActive;
    auto lastEnvelope = 1.0f;
    auto lastPosition = (float) phase;
    auto lastAmount   = amount;

    float* const* channelData = buffer.getArrayOfWritePointers();

    for (int n = 0; n < numSamples; ++n)
    {
        // --- Parameter glaetten (20 ms) -------------------------------------
        preGain  = (float) dsp::smoothTowards ((double) preGain,  (double) newPreGain,  (double) smoothCoeff);
        postGain = (float) dsp::smoothTowards ((double) postGain, (double) newPostGain, (double) smoothCoeff);
        amount   = (float) dsp::smoothTowards ((double) amount,   (double) newAmount,   (double) smoothCoeff);

        // --- Trigger: Flanke auf dem Eingangssummenpegel ---------------------
        // Der Pegel wird VOR sPre gemessen (der Puffer ist hier noch der
        // Eingang), damit die Schwelle unabhaengig vom Eingangsgain greift.
        auto level = 0.0f;

        for (int ch = 0; ch < numChannels; ++ch)
            level = juce::jmax (level, std::abs (channelData[ch][n]));

        if (triggered)
        {
            if (level < kTriggerLowLevel)
                triggered = false;   // re-armieren (Hysterese)
        }
        else if (level >= kTriggerHighLevel)
        {
            triggered = true;
            phase = 0.0;             // jeder Trigger startet an Punkt 0
        }

        // Ohne Trigger parkt die Uhr am Anfang der Huellkurve.
        if (! triggered)
            phase = 0.0;

        const auto envelope = evaluatePath (path, shapeMode, phase);

        lastEnvelope = envelope;
        lastPosition = (float) phase;
        lastAmount   = amount;

        if (triggered)
        {
            phase += phaseIncrement;

            if (phase > 1.0)
                phase = 1.0;
        }

        // --- Umformung und Gains --------------------------------------------
        // `sLine` ist eine harte Sicherung: dann wirkt nur die Verstaerkung.
        const auto blend = linearSafe ? 0.0f : amount;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto in = channelData[ch][n];
            const auto x  = in * preGain;

            auto y = x;

            if (blend > 0.0f)
            {
                // out = in*(1-a) + shaped*a  mit  shaped = x * env. Die
                // Differenzform ist zahlengleich und bei a == 0 exakt.
                const auto shaped = x * envelope;
                y = x + blend * (shaped - x);
            }

            channelData[ch][n] = y * postGain;
        }
    }

    smoothedPreGain  = preGain;
    smoothedPostGain = postGain;
    smoothedAmount   = amount;
    envelopePhase    = phase;
    triggerActive    = triggered;

    diagPosition.store (lastPosition, std::memory_order_relaxed);
    diagEnvelope.store (lastEnvelope, std::memory_order_relaxed);
    diagAmount.store (lastAmount, std::memory_order_relaxed);
    diagPreGain.store (preGain, std::memory_order_relaxed);
    diagPostGain.store (postGain, std::memory_order_relaxed);
    diagTrigger.store (triggered, std::memory_order_relaxed);
}

// ===========================================================================
// Kick-Laenge und Huellkurve
// ===========================================================================

void KickShaperModule::setRenderLength (double seconds)
{
    if (! std::isfinite (seconds))
        return;

    lengthSeconds.store (juce::jlimit (kMinRenderLengthSeconds, kMaxRenderLengthSeconds, seconds),
                        std::memory_order_relaxed);
}

void KickShaperModule::setEnvelopePoints (const std::vector<Pt>& newPoints)
{
    points = newPoints;

    if (points.size() > (size_t) maxPoints)
        points.resize ((size_t) maxPoints);

    for (auto& point : points)
    {
        point.x = juce::jlimit (0.0, 1.0, point.x);
        point.y = juce::jlimit (0.0, 1.0, point.y);
        point.t = juce::jlimit (0, kNumSegmentTypes - 1, point.t);
    }

    if (points.empty())
        points = makeDefaultEnvelope();

    rebuildPath();
}

void KickShaperModule::rebuildPath()
{
    // Fuer die Auswertung wird nach x sortiert; der Zustand (`points`) behaelt
    // die Reihenfolge, in der er gesetzt wurde (Round-Trip).
    path = points;

    std::sort (path.begin(), path.end(),
               [] (const Pt& a, const Pt& b) { return a.x < b.x; });

    std::vector<Pt> unique;
    unique.reserve (path.size());

    for (const auto& point : path)
    {
        // Entartete Positionen: der zuletzt gelesene Punkt gewinnt.
        if (! unique.empty() && point.x - unique.back().x <= kPointEpsilon)
            unique.back() = point;
        else
            unique.push_back (point);
    }

    path = unique;
}

// ===========================================================================
// Huellkurven-Interpolation
// ===========================================================================

double KickShaperModule::shapeBend (int shapeMode, double u) noexcept
{
    const auto x = juce::jlimit (0.0, 1.0, u);

    switch (juce::jlimit (0, kNumShapes - 1, shapeMode))
    {
        case 0:
            // Halten: der Wert des Startpunkts steht bis zum naechsten Punkt.
            return 0.0;

        case 1:
            // Lineare Rampe.
            return x;

        case 2:
            // Rampe mit Spiegelung: Dreieck, das den Zielwert in der Mitte des
            // Segments erreicht und zum Ende wieder auf den Startwert laeuft.
            return 1.0 - std::abs (2.0 * x - 1.0);

        case 3:
            // Stufen: Sprung auf den Zielwert am Segmentende.
            return (x < 1.0) ? 0.0 : 1.0;

        case 4:
        {
            // Exponentielle Annaeherung: normiert, damit f(0) = 0 und f(1) = 1.
            const auto k = kApproachConstant;
            return (1.0 - std::exp (-k * x)) / (1.0 - std::exp (-k));
        }

        case 5:
        {
            // Halten + Spiegel: erste Haelfte auf dem Startwert, zweite Haelfte
            // eine gespiegelte (beschleunigende) Rampe auf den Zielwert.
            if (x < 0.5)
                return 0.0;

            const auto v = 2.0 * x - 1.0;
            return v * v;
        }

        case 6:
            // Halbwellen-Rampe: sin(u*pi/2), steil am Anfang, flach am Ende.
            return std::sin (x * juce::MathConstants<double>::halfPi);

        case 7:
            // Invertierte Rampe: der Wert wandert vom Ziel zum Startwert.
            return 1.0 - x;

        default:
            break;
    }

    return x;
}

double KickShaperModule::segmentBend (int shapeMode, int segmentType, double u) noexcept
{
    const auto x = juce::jlimit (0.0, 1.0, u);

    switch (juce::jlimit (0, kNumSegmentTypes - 1, segmentType))
    {
        case 1:
            // Halten: der Startwert steht bis zum naechsten Punkt (t = 1).
            return 0.0;

        case 2:
            // Rampe/Spiegel: derselbe Modus, aber mit gespiegeltem Fortschritt.
            return shapeBend (shapeMode, 1.0 - x);

        case 3:
            // Stufe: Sprung auf den Zielwert genau am Segmentende.
            return (x < 1.0) ? 0.0 : 1.0;

        default:
            break;
    }

    // t == 0 ("linear"): das Segment folgt dem globalen `shape`-Modus.
    return shapeBend (shapeMode, x);
}

float KickShaperModule::evaluatePath (const std::vector<Pt>& curve,
                                      int shapeMode,
                                      double position) const noexcept
{
    if (curve.empty())
        return 1.0f;   // keine Punkte: neutral

    const auto p = juce::jlimit (0.0, 1.0, position);

    if (curve.size() == 1 || p <= curve.front().x)
        return (float) juce::jlimit (0.0, 1.0, curve.front().y);

    if (p >= curve.back().x)
        return (float) juce::jlimit (0.0, 1.0, curve.back().y);

    for (size_t i = 1; i < curve.size(); ++i)
    {
        const auto& right = curve[i];

        if (p > right.x)
            continue;

        const auto& left = curve[i - 1];
        const auto span  = right.x - left.x;
        const auto u     = (span > 1.0e-12) ? (p - left.x) / span : 1.0;
        const auto bend  = segmentBend (shapeMode, left.t, u);
        const auto value = left.y + (right.y - left.y) * bend;

        return (float) juce::jlimit (0.0, 1.0, value);
    }

    return (float) juce::jlimit (0.0, 1.0, curve.back().y);
}

float KickShaperModule::evaluateEnvelope (double position) const noexcept
{
    return evaluatePath (path, targetShape.load (std::memory_order_relaxed), position);
}

float KickShaperModule::evaluateEnvelopeForShape (int shapeMode, double position) const noexcept
{
    return evaluatePath (path, juce::jlimit (0, kNumShapes - 1, shapeMode), position);
}

// ===========================================================================
// Struktur-Zustand
// ===========================================================================

juce::ValueTree KickShaperModule::getState() const
{
    juce::ValueTree state ("Module");

    // 1) Skalare als Attribute - dieselben IDs wie params()/XML.
    const auto& table = descriptorTable();

    for (int i = 0; i < (int) values.size() && i < (int) table.size(); ++i)
        state.setProperty (juce::Identifier (table[(size_t) i].id),
                           toXmlNumber ((double) values[(size_t) i]),
                           nullptr);

    // 2) Huellkurve als <Pt x y t/> in Punktreihenfolge.
    for (const auto& point : points)
    {
        juce::ValueTree child ("Pt");
        child.setProperty ("x", toXmlNumber (point.x), nullptr);
        child.setProperty ("y", toXmlNumber (point.y), nullptr);
        child.setProperty ("t", point.t, nullptr);
        state.appendChild (child, nullptr);
    }

    return state;
}

void KickShaperModule::setState (const juce::ValueTree& state)
{
    if (! state.isValid())
        return;

    // 1) Skalare, sofern vorhanden (die Kette setzt sie normalerweise vorher
    //    ueber setParam(); fehlende Attribute bleiben auf ihrem Wert).
    const auto& table = descriptorTable();

    for (int i = 0; i < (int) table.size(); ++i)
    {
        const auto& desc = table[(size_t) i];
        const juce::Identifier attribute (desc.id);

        if (! state.hasProperty (attribute))
            continue;

        const auto value = state.getProperty (attribute);

        if (! (value.isDouble() || value.isInt() || value.isInt64() || value.isBool()))
            continue;

        setParam (i, juce::jlimit (desc.minValue, desc.maxValue, (float) (double) value));
    }

    // 2) Huellkurve: alle <Pt>-Kinder in Reihenfolge; kein Punkt = Werkskurve.
    static const juce::Identifier ptType ("Pt");

    std::vector<Pt> parsed;

    for (int i = 0; i < state.getNumChildren() && (int) parsed.size() < maxPoints; ++i)
    {
        const auto child = state.getChild (i);

        if (! child.hasType (ptType))
            continue;   // unbekannte Kinder ignorieren

        Pt point;
        point.x = (double) child.getProperty ("x", 0.0);
        point.y = (double) child.getProperty ("y", 1.0);
        point.t = (int) child.getProperty ("t", 0);

        parsed.push_back (point);
    }

    setEnvelopePoints (parsed);
}

} // namespace bfka
