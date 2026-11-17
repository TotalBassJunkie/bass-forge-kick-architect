#include "fx/modules/DistortionModule.h"

#include <algorithm>
#include <cmath>

namespace bfka
{

namespace
{
    // -----------------------------------------------------------------------
    // Kennwerte der Transferfunktionen (alle als benannte Konstanten, damit die
    // Spezifikation in docs/specs/fx-distortion.md 1:1 nachvollziehbar bleibt)
    // -----------------------------------------------------------------------

    /** SOFT CLIP: Steilheit des tanh-Knies. */
    constexpr float kSoftClipDrive = 1.5f;

    /** SATURATE: Steilheit des tanh-Knies (hoeher = "dicker"). */
    constexpr float kSaturateDrive = 3.0f;

    /** SATURATE: Gewicht des kubischen Terms (3. Harmonische). */
    constexpr float kSaturateCubic = 0.25f;

    /** FUZZ: Gate-Schwelle (darunter wird hart auf 0 geschnitten). */
    constexpr float kFuzzGate = 0.02f;

    /** FUZZ: Vorverstaerkung vor dem harten Schnitt. */
    constexpr float kFuzzGain = 5.0f;

    /** FUZZ: Pegel der negativen Halbwelle (Asymmetrie = zerrige Textur). */
    constexpr float kFuzzAsym = 0.85f;

    /** BITCRUSH: Amplituden-Stufen pro Einheit (8 -> 12,5 % Aufloesung). */
    constexpr float kBitcrushSteps = 8.0f;

    /** STEPS/BLOCKS: Anzahl der Stufen je Segment. */
    constexpr float kSegmentSteps = 4.0f;

    /** AGC: Zeitkonstanten (Sekunden). */
    constexpr double kAgcAttackSeconds  = 0.005; // 5 ms
    constexpr double kAgcReleaseSeconds = 0.120; // 120 ms
    constexpr double kAgcSmoothSeconds  = 0.002; // Glaettung der Korrektur

    /** AGC: maximale Korrektur. 10^(+-12/20) */
    constexpr float kAgcMinGain = 0.25118864f;
    constexpr float kAgcMaxGain = 3.98107171f;

    /** AGC: ab dieser Leistung gilt der Zweig als "still" (kein Verhaeltnis). */
    constexpr float kAgcSilencePower = 1.0e-12f;

    /** EQ-Chip: Tilt-Eckfrequenz und die beiden Plateau-Pegel (+-3 dB). */
    constexpr double kTiltFrequency = 700.0;
    constexpr float  kTiltLowGain   = 0.70794578f; // -3 dB
    constexpr float  kTiltHighGain  = 1.41253754f; // +3 dB

    /** Ceiling: 0 dBFS. */
    constexpr float kCeiling = 1.0f;

    /** 2x-Oversampling (log2-Faktor 1). */
    constexpr size_t kOversamplingFactorLog2 = 1;

    // -----------------------------------------------------------------------
    // Hilfen
    // -----------------------------------------------------------------------

    /** Werkskurve: drei Knoten, linear (exakte Identitaet). */
    std::vector<DistortionModule::WNode> makeDefaultCurve()
    {
        using Seg = DistortionModule::SegmentMode;

        return { { -1.0, -1.0, 0.5, (int) Seg::linear },
                 {  0.0,  0.0, 0.5, (int) Seg::linear },
                 {  1.0,  1.0, 0.5, (int) Seg::linear } };
    }

    /**
     * Zahl fuer ein XML-Attribut: ganzzahlige Werte als Integer (das
     * Referenzformat schreibt `drive="45"`, nicht `45.0`), sonst auf 6
     * Nachkommastellen gerundet (vermeidet Float-Rauschen wie 2.400000095...).
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
// Lebenszyklus
// ===========================================================================

DistortionModule::DistortionModule()
{
    // Parameter in Anzeige-Reihenfolge (Index == Nummer fuer setParam/getParam).
    descriptors.push_back ({ "drive",  "Drive",   0.0f, 100.0f, 25.0f, "%" });
    descriptors.push_back ({ "mode",   "Mode",    0.0f,   9.0f,  1.0f, ""  });
    descriptors.push_back ({ "folds",  "Folds",   2.0f,   8.0f,  2.0f, ""  });
    descriptors.push_back ({ "agc",    "AGC",     0.0f,   1.0f,  1.0f, ""  });
    descriptors.push_back ({ "eqmode", "EQ",      0.0f,   2.0f,  0.0f, ""  });
    descriptors.push_back ({ "flip",   "Flip",    0.0f,   1.0f,  0.0f, ""  });

    for (size_t i = 0; i < descriptors.size() && i < paramValues.size(); ++i)
        paramValues[i].store (descriptors[i].defaultValue, std::memory_order_relaxed);

    nodes = makeDefaultCurve();
    rebuildCurveTables();
}

void DistortionModule::prepare (double sampleRate, int maxBlockSize)
{
    preparedSampleRate = (sampleRate > 0.0) ? sampleRate : 48000.0;
    preparedBlockSize  = juce::jmax (1, maxBlockSize);

    // Immer 2x ueberabgetastet - auch fuer lineare Einstellungen. Damit ist die
    // Latenz konstant und der Trockenpfad der Kette bleibt gueltig.
    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
                      (size_t) kMaxProcessingChannels,
                      kOversamplingFactorLog2,
                      juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
                      true,   // maximale Qualitaet
                      true);  // ganzzahlige Latenz (fraktionaler Ausgleich intern)
    oversampler->initProcessing ((size_t) preparedBlockSize);

    reportedLatency = juce::jmax (0, (int) std::lround (oversampler->getLatencyInSamples()));

    // AGC laeuft im ueberabgetasteten Bereich.
    const double oversampledRate = preparedSampleRate * 2.0;

    agcAttackCoef  = (float) (1.0 - std::exp (-1.0 / (kAgcAttackSeconds  * oversampledRate)));
    agcReleaseCoef = (float) (1.0 - std::exp (-1.0 / (kAgcReleaseSeconds * oversampledRate)));
    agcSmoothCoef  = (float) (1.0 - std::exp (-1.0 / (kAgcSmoothSeconds  * oversampledRate)));

    // Tilt-EQ laeuft auf der Basis-Rate (vor bzw. nach dem Oversampling).
    tiltCoef = (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi
                                        * kTiltFrequency / preparedSampleRate));

    // Laenger als die Latenz, damit Lesen und Schreiben nie kollidieren.
    delayRing.setSize (kMaxProcessingChannels,
                       reportedLatency + preparedBlockSize + 2,
                       false, true, false);

    reset();
}

void DistortionModule::reset()
{
    if (oversampler != nullptr)
        oversampler->reset();

    delayRing.clear();
    delayWritePos = 0;

    for (auto& state : channelState)
        state = ChannelState {};
}

// ===========================================================================
// Parameter
// ===========================================================================

void DistortionModule::setParam (int index, float value)
{
    if (index < 0 || index >= (int) descriptors.size() || index >= (int) paramValues.size())
        return;

    const auto& desc = descriptors[(size_t) index];
    const auto clamped = juce::jlimit (desc.minValue, desc.maxValue, value);

    paramValues[(size_t) index].store (clamped, std::memory_order_relaxed);
}

float DistortionModule::getParam (int index) const
{
    if (index < 0 || index >= (int) descriptors.size() || index >= (int) paramValues.size())
        return 0.0f;

    return paramValues[(size_t) index].load (std::memory_order_relaxed);
}

// ===========================================================================
// Audio
// ===========================================================================

void DistortionModule::process (juce::AudioBuffer<float>& buffer)
{
    const int numChannels = juce::jmin (buffer.getNumChannels(), kMaxProcessingChannels);
    const int numSamples  = buffer.getNumSamples();

    if (oversampler == nullptr || numChannels <= 0 || numSamples <= 0)
        return;

    // Parameter einmal pro Block lesen (alles atomar, keine Sperre).
    Params params;
    params.drive  = juce::jlimit (0.0f, 1.0f, getParam (paramDrive) * 0.01f);
    params.mode   = toMode ((int) std::lround (getParam (paramMode)));
    params.folds  = getParam (paramFolds);
    params.agc    = getParam (paramAgc) >= 0.5f;
    params.eqMode = juce::jlimit (0, 2, (int) std::lround (getParam (paramEqMode)));
    params.flip   = getParam (paramFlip) >= 0.5f;

    for (int offset = 0; offset < numSamples; offset += preparedBlockSize)
        processChunk (buffer, numChannels, offset,
                      juce::jmin (preparedBlockSize, numSamples - offset),
                      params);
}

void DistortionModule::processChunk (juce::AudioBuffer<float>& buffer,
                                     int numChannels,
                                     int startSample,
                                     int numSamples,
                                     const Params& params)
{
    // 1) EQ-Chip PRE (formt, was in die Verzerrung geht)
    if (params.eqMode == 1)
        applyTilt (buffer, numChannels, startSample, numSamples);

    // 2) Kern: Kurve. Bei DRIVE 0 wird nur verzoegert (bitidentisch clean).
    if (params.drive <= 0.0f)
    {
        copyDelayed (buffer, numChannels, startSample, numSamples);
    }
    else
    {
        feedDelayLine (buffer, numChannels, startSample, numSamples);
        applyShaping (buffer, numChannels, startSample, numSamples, params);
    }

    // 3) EQ-Chip POST (formt, was herauskommt)
    if (params.eqMode == 2)
        applyTilt (buffer, numChannels, startSample, numSamples);

    // 4) Ceiling (0 dBFS) - letzte Instanz, auch nach dem POST-EQ.
    clampRange (buffer, numChannels, startSample, numSamples);
}

void DistortionModule::applyShaping (juce::AudioBuffer<float>& buffer,
                                     int numChannels,
                                     int startSample,
                                     int numSamples,
                                     const Params& params)
{
    juce::dsp::AudioBlock<float> fullBlock (buffer);

    auto rangeBlock = fullBlock.getSubsetChannelBlock (0, (size_t) numChannels)
                               .getSubBlock ((size_t) startSample, (size_t) numSamples);

    auto oversampled = oversampler->processSamplesUp (rangeBlock);

    const auto numOversampledSamples = oversampled.getNumSamples();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = oversampled.getChannelPointer ((size_t) ch);
        auto& state = channelState[(size_t) ch];

        for (size_t n = 0; n < numOversampledSamples; ++n)
            data[n] = processShapedSample (data[n], params, state);
    }

    oversampler->processSamplesDown (rangeBlock);
}

float DistortionModule::processShapedSample (float x,
                                             const Params& params,
                                             ChannelState& state) const noexcept
{
    // Kurve + DRIVE-Blende: 0 % clean, 100 % exakt die Kurve.
    const float curve = shapeImpl (x, params.mode, params.folds, params.flip);
    float y = x + params.drive * (curve - x);

    if (params.agc)
    {
        // Huellkurven-Folger auf der Leistung (RMS-Verhaeltnis).
        const float inPower  = x * x;
        const float outPower = y * y;

        state.agcEnvIn  += (inPower  - state.agcEnvIn)
                        * (inPower  > state.agcEnvIn  ? agcAttackCoef : agcReleaseCoef);
        state.agcEnvOut += (outPower - state.agcEnvOut)
                        * (outPower > state.agcEnvOut ? agcAttackCoef : agcReleaseCoef);

        float target = 1.0f;

        if (state.agcEnvOut > kAgcSilencePower)
            target = juce::jlimit (kAgcMinGain, kAgcMaxGain,
                                   std::sqrt (state.agcEnvIn / state.agcEnvOut));

        state.agcGain += (target - state.agcGain) * agcSmoothCoef;
        y *= state.agcGain;
    }
    else
    {
        // Ohne Regelung sofort neutral - kein Nachklingen beim Wiederanschalten.
        state.agcGain = 1.0f;
    }

    return juce::jlimit (-kCeiling, kCeiling, y);
}

// ---------------------------------------------------------------------------
// EQ-Chip (Tilt)
// ---------------------------------------------------------------------------

void DistortionModule::applyTilt (juce::AudioBuffer<float>& buffer,
                                  int numChannels,
                                  int startSample,
                                  int numSamples)
{
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch, startSample);
        auto& low  = channelState[(size_t) ch].tiltLow;

        for (int n = 0; n < numSamples; ++n)
        {
            const float x = data[n];
            low += tiltCoef * (x - low);

            const float lowBand  = low;
            const float highBand = x - lowBand;

            data[n] = lowBand * kTiltLowGain + highBand * kTiltHighGain;
        }
    }
}

// ---------------------------------------------------------------------------
// Verzoegerungsleitung fuer den reinen Durchreich-Fall
// ---------------------------------------------------------------------------

void DistortionModule::feedDelayLine (juce::AudioBuffer<float>& buffer,
                                      int numChannels,
                                      int startSample,
                                      int numSamples)
{
    const int ringSize = delayRing.getNumSamples();

    if (ringSize <= 0 || reportedLatency <= 0)
        return;

    std::array<float*, (size_t) kMaxProcessingChannels> ring {};
    std::array<const float*, (size_t) kMaxProcessingChannels> input {};

    for (int ch = 0; ch < numChannels; ++ch)
    {
        ring[(size_t) ch]  = delayRing.getWritePointer (ch);
        input[(size_t) ch] = buffer.getReadPointer (ch, startSample);
    }

    for (int n = 0; n < numSamples; ++n)
    {
        for (int ch = 0; ch < numChannels; ++ch)
            ring[(size_t) ch][delayWritePos] = input[(size_t) ch][n];

        delayWritePos = (delayWritePos + 1) % ringSize;
    }
}

void DistortionModule::copyDelayed (juce::AudioBuffer<float>& buffer,
                                    int numChannels,
                                    int startSample,
                                    int numSamples)
{
    const int ringSize = delayRing.getNumSamples();

    if (ringSize <= 0 || reportedLatency <= 0)
        return;

    std::array<float*, (size_t) kMaxProcessingChannels> ring {};
    std::array<float*, (size_t) kMaxProcessingChannels> data {};

    for (int ch = 0; ch < numChannels; ++ch)
    {
        ring[(size_t) ch] = delayRing.getWritePointer (ch);
        data[(size_t) ch] = buffer.getWritePointer (ch, startSample);
    }

    for (int n = 0; n < numSamples; ++n)
    {
        const int readPos = (delayWritePos + ringSize - reportedLatency) % ringSize;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float in = data[(size_t) ch][n];
            data[(size_t) ch][n] = ring[(size_t) ch][readPos];
            ring[(size_t) ch][delayWritePos] = in;
        }

        delayWritePos = (delayWritePos + 1) % ringSize;
    }
}

void DistortionModule::clampRange (juce::AudioBuffer<float>& buffer,
                                   int numChannels,
                                   int startSample,
                                   int numSamples) const noexcept
{
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch, startSample);

        for (int n = 0; n < numSamples; ++n)
            data[n] = juce::jlimit (-kCeiling, kCeiling, data[n]);
    }
}

// ===========================================================================
// Transferfunktionen
// ===========================================================================

DistortionModule::Mode DistortionModule::toMode (int modeIndex) noexcept
{
    return (Mode) juce::jlimit (0, kNumModes - 1, modeIndex);
}

float DistortionModule::shape (float x) const noexcept
{
    return shapeImpl (x, currentMode(), currentFolds(), currentFlip());
}

float DistortionModule::shapeForMode (int mode, float x) const noexcept
{
    return shapeImpl (x, toMode (mode), currentFolds(), currentFlip());
}

float DistortionModule::shapeImpl (float x, Mode mode, float folds, bool flip) const noexcept
{
    switch (mode)
    {
        case Mode::hardClip:
            // Harter Schnitt am Ceiling: die Kante ist die Verzerrung.
            return juce::jlimit (-kCeiling, kCeiling, x);

        case Mode::softClip:
            // Weiche Saettigung in ein rundes Ceiling: tanh(1,5*x) ist streng
            // monoton und betragsmaessig immer < 1.
            return std::tanh (kSoftClipDrive * x);

        case Mode::saturate:
        {
            // "Dickes" Wachstum: t = tanh(3x), y = t - 0,25*t^3.
            // Der kubische Term hebt die 3. Harmonische an, |y| <= 0,75.
            const float t = std::tanh (kSaturateDrive * x);
            return t * (1.0f - kSaturateCubic * t * t);
        }

        case Mode::wavefold:
        {
            // Wellenfalter: sin(pi/2 * folds * x). folds = Anzahl der Faltungen
            // ueber den Aussteuerbereich; der Sinus haelt |y| <= 1.
            const float folding = juce::jlimit (2.0f, 8.0f, folds);
            return std::sin (juce::MathConstants<float>::halfPi * folding * x);
        }

        case Mode::fuzz:
        {
            // Gated Fuzz: unterhalb der Gate-Schwelle wird hart auf 0
            // geschnitten (zerrissene Textur), darueber harter Schnitt mit
            // asymmetrischem Pegel fuer die positive/negative Halbwelle.
            if (std::abs (x) < kFuzzGate)
                return 0.0f;

            const float hot = juce::jlimit (-kCeiling, kCeiling, kFuzzGain * x);

            return (x >= 0.0f) ? hot : kFuzzAsym * hot;
        }

        case Mode::rectify:
            // Vollwellen-Gleichrichtung: verdoppelt die Grundfrequenz.
            // tanh begrenzt die Auslenkung, |y| <= 1.
            return std::abs (std::tanh (kSoftClipDrive * x));

        case Mode::halfWave:
        {
            // Eine Halbwelle formen; flip waehlt, welche Haelfte stehen bleibt.
            const float t = std::tanh (kSoftClipDrive * x);
            return flip ? juce::jmin (0.0f, t) : juce::jmax (0.0f, t);
        }

        case Mode::bitcrush:
        {
            // Amplituden-Quantisierung (8 Stufen pro Einheit), danach Ceiling.
            const float quantised = std::round (kBitcrushSteps * x) / kBitcrushSteps;
            return juce::jlimit (-kCeiling, kCeiling, quantised);
        }

        case Mode::waveshaper:
            // Freie Transferkurve: eine Linie fuer beide Haelften.
            return lookupTable (symTable, juce::jlimit (-1.0f, 1.0f, x) * 0.5f + 0.5f);

        case Mode::waveshaperAsym:
            // Mittelpunkt (x = 0) trennt die Haelften, jede hat ihre eigene Kurve.
            if (x < 0.0f)
                return lookupTable (asymNegTable, juce::jlimit (-1.0f, 1.0f, x) + 1.0f);

            return lookupTable (asymPosTable, juce::jlimit (0.0f, 1.0f, x));
    }

    return 0.0f;
}

// ===========================================================================
// Waveshaper-Kurve
// ===========================================================================

void DistortionModule::setCurveNodes (const std::vector<WNode>& newNodes)
{
    nodes = newNodes;

    if (nodes.empty())
        nodes = makeDefaultCurve();

    for (auto& node : nodes)
    {
        node.x   = juce::jlimit (-1.0, 1.0, node.x);
        node.y   = juce::jlimit (-1.0, 1.0, node.y);
        node.seg = juce::jlimit (0, (int) SegmentMode::blocks, node.seg);
        // `ten` bleibt roh: das Referenzformat enthaelt auch negative Werte
        // (-0,8473 beobachtet). Die Uebertragung klemmt erst beim Backen.
    }

    rebuildCurveTables();
}

juce::ValueTree DistortionModule::getState() const
{
    juce::ValueTree state ("Module");

    // 1) Skalare als Attribute - dieselben IDs wie params()/XML.
    for (int i = 0; i < (int) descriptors.size(); ++i)
        state.setProperty (juce::Identifier (descriptors[(size_t) i].id),
                           toXmlNumber ((double) getParam (i)),
                           nullptr);

    // 2) Kurve als <WNode x y ten seg/> in Knotenreihenfolge.
    for (const auto& node : nodes)
    {
        juce::ValueTree child ("WNode");
        child.setProperty ("x",   toXmlNumber (node.x),   nullptr);
        child.setProperty ("y",   toXmlNumber (node.y),   nullptr);
        child.setProperty ("ten", toXmlNumber (node.ten), nullptr);
        child.setProperty ("seg", node.seg, nullptr);
        state.appendChild (child, nullptr);
    }

    return state;
}

void DistortionModule::setState (const juce::ValueTree& state)
{
    if (! state.isValid())
        return;

    // 1) Skalare, sofern vorhanden (die Kette setzt sie normalerweise vorher
    //    ueber setParam(); unbekannte/fehlende Attribute bleiben Default).
    for (int i = 0; i < (int) descriptors.size(); ++i)
    {
        const auto& desc = descriptors[(size_t) i];
        const juce::Identifier attribute (desc.id);

        if (! state.hasProperty (attribute))
            continue;

        const auto value = state.getProperty (attribute);

        if (! (value.isDouble() || value.isInt() || value.isInt64() || value.isBool()))
            continue;

        setParam (i, juce::jlimit (desc.minValue, desc.maxValue, (float) (double) value));
    }

    // 2) Kurve: alle <WNode>-Kinder in Reihenfolge; kein Knoten = Werkskurve.
    static const juce::Identifier wNodeType ("WNode");

    std::vector<WNode> parsed;

    for (int i = 0; i < state.getNumChildren(); ++i)
    {
        const auto child = state.getChild (i);

        if (! child.hasType (wNodeType))
            continue;

        WNode node;
        node.x   = (double) child.getProperty ("x",   0.0);
        node.y   = (double) child.getProperty ("y",   0.0);
        node.ten = (double) child.getProperty ("ten", 0.5);
        node.seg = (int) child.getProperty ("seg", 0);

        parsed.push_back (node);
    }

    if (parsed.empty())
        parsed = makeDefaultCurve();

    setCurveNodes (parsed);
}

void DistortionModule::rebuildCurveTables()
{
    // Knoten nach x sortieren und entartete (gleiches x) entfernen.
    std::vector<WNode> sorted = nodes;

    std::sort (sorted.begin(), sorted.end(),
               [] (const WNode& a, const WNode& b) { return a.x < b.x; });

    std::vector<WNode> path;
    path.reserve (sorted.size());

    for (const auto& node : sorted)
        if (path.empty() || node.x - path.back().x > 1.0e-6)
            path.push_back (node);

    // Symmetrische Kurve: ein Pfad ueber den ganzen Bereich [-1, +1].
    for (int i = 0; i < kCurveTableSize; ++i)
    {
        const float x = -1.0f + 2.0f * (float) i / (float) (kCurveTableSize - 1);
        symTable[(size_t) i] = evalCurvePath (path, x);
    }

    // Asymmetrisch: je Haelfte ein eigener Pfad (x <= 0 bzw. x >= 0).
    std::vector<WNode> negativeHalf;
    std::vector<WNode> positiveHalf;

    for (const auto& node : path)
    {
        if (node.x <= 1.0e-6)
            negativeHalf.push_back (node);

        if (node.x >= -1.0e-6)
            positiveHalf.push_back (node);
    }

    for (int i = 0; i < kCurveTableSize; ++i)
    {
        const float t = (float) i / (float) (kCurveTableSize - 1);

        asymNegTable[(size_t) i] = evalCurvePath (negativeHalf, t - 1.0f);
        asymPosTable[(size_t) i] = evalCurvePath (positiveHalf, t);
    }
}

float DistortionModule::evalCurvePath (const std::vector<WNode>& path, float x) const noexcept
{
    // Kein Knoten -> Identitaet.
    if (path.empty())
        return juce::jlimit (-1.0f, 1.0f, x);

    const double xd = (double) x;

    // Ausserhalb des Knotenbereichs wird auf den Randwert geklemmt.
    if (xd <= path.front().x)
        return juce::jlimit (-1.0f, 1.0f, (float) path.front().y);

    if (xd >= path.back().x)
        return juce::jlimit (-1.0f, 1.0f, (float) path.back().y);

    for (size_t i = 1; i < path.size(); ++i)
    {
        const auto& right = path[i];

        if (xd > right.x)
            continue;

        const auto& left = path[i - 1];
        const double span = right.x - left.x;
        const float t = (span > 1.0e-12) ? (float) ((xd - left.x) / span) : 1.0f;

        return juce::jlimit (-1.0f, 1.0f, evalSegment (left, right, t));
    }

    return juce::jlimit (-1.0f, 1.0f, (float) path.back().y);
}

float DistortionModule::evalSegment (const WNode& a, const WNode& b, float t) noexcept
{
    // Der rechte Knoten beschreibt das Segment davor: `seg` = Modus,
    // `ten` = Biegegrad (0,5 = keine Biegung).
    const float u    = juce::jlimit (0.0f, 1.0f, t);
    const float ten  = (float) juce::jlimit (0.0, 1.0, b.ten);
    const auto  mode = (SegmentMode) juce::jlimit (0, (int) SegmentMode::blocks, b.seg);

    float bend = u;

    switch (mode)
    {
        case SegmentMode::linear:
            bend = u;
            break;

        case SegmentMode::curve:
        {
            // Exponent 2^((ten-0,5)*2): 0,5 = Gerade, 1,0 = 2, 0,0 = 0,5.
            const float exponent = std::pow (2.0f, (ten - 0.5f) * 2.0f);
            bend = std::pow (u, exponent);
            break;
        }

        case SegmentMode::halfSine:
            bend = std::sin (u * juce::MathConstants<float>::halfPi);
            break;

        case SegmentMode::sine:
            bend = 0.5f - 0.5f * std::cos (u * juce::MathConstants<float>::pi);
            break;

        case SegmentMode::doubleWave:
            bend = u + 0.25f * std::sin (juce::MathConstants<float>::twoPi * u);
            break;

        case SegmentMode::square:
            // Sprung bei `ten` (Tastverhaeltnis).
            bend = (u < ten) ? 0.0f : 1.0f;
            break;

        case SegmentMode::steps:
            bend = std::floor (u * kSegmentSteps) / kSegmentSteps;
            break;

        case SegmentMode::blocks:
            bend = juce::jlimit (0.0f, 1.0f,
                                 std::round (u * kSegmentSteps) / kSegmentSteps);
            break;
    }

    return (float) (a.y + (b.y - a.y) * (double) bend);
}

float DistortionModule::lookupTable (const std::array<float, (size_t) kCurveTableSize>& table,
                                    float position) noexcept
{
    const float p = juce::jlimit (0.0f, 1.0f, position);
    const float scaled = p * (float) (kCurveTableSize - 1);
    const int   index0 = (int) scaled;
    const int   index1 = juce::jmin (index0 + 1, kCurveTableSize - 1);
    const float frac   = scaled - (float) index0;

    const float value0 = table[(size_t) index0];

    return value0 + frac * (table[(size_t) index1] - value0);
}

} // namespace bfka
