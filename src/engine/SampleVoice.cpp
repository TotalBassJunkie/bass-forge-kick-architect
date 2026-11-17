#include "engine/SampleVoice.h"

#include <cmath>

namespace bfka
{

// ---------------------------------------------------------------------------
// Fade-Kurven
// ---------------------------------------------------------------------------
float SampleVoice::shapeGain (FadeShape shape, float t, float amount) noexcept
{
    const auto s = juce::jlimit (0.0f, 1.0f, t);
    const auto a = juce::jlimit (-1.0f, 1.0f, amount);

    switch (shape)
    {
        case FadeShape::linear:
            // Kein Biegeparameter: eine Gerade hat keine Biegung.
            return s;

        case FadeShape::curve:
        {
            // p = 4^a: a = 0 -> linear, a > 0 -> konvex (haelt zurueck und holt
            // spaeter auf), a < 0 -> konkav (kommt frueh hoch und flacht ab).
            const auto p = std::pow (4.0f, a);
            return std::pow (s, p);
        }

        case FadeShape::sCurve:
        {
            // Smoothstep, mit demselben Biegeparameter vorverzerrt: bleibt bei
            // a = 0 punktsymmetrisch (g(0.5) = 0.5).
            const auto p = std::pow (4.0f, a);
            const auto u = std::pow (s, p);
            return u * u * (3.0f - 2.0f * u);
        }

        case FadeShape::fast:
        {
            // Schneller Anstieg mit exponentiell abfallender Steigung.
            const auto k = 4.0f * std::pow (2.0f, a);
            const auto denominator = 1.0f - std::exp (-k);

            if (! (denominator > 0.0f))
                return s;   // Sicherheitsnetz (numerisch nicht erreichbar)

            return (1.0f - std::exp (-k * s)) / denominator;
        }

        case FadeShape::slow:
        {
            // Logarithmisch ansteigend: sanfter als `fast`, aber ebenfalls
            // frueher als die Gerade.
            const auto c = 4.0f * std::pow (4.0f, a);
            const auto denominator = std::log1p (c);

            if (! (denominator > 0.0f))
                return s;   // Sicherheitsnetz

            return std::log1p (c * s) / denominator;
        }
    }

    return s;
}

// ---------------------------------------------------------------------------
// Konfiguration
// ---------------------------------------------------------------------------
void SampleVoice::setSource (const juce::AudioBuffer<float>* sample, double sourceSampleRate)
{
    source = sample;
    sourceRate = (sourceSampleRate > 0.0) ? sourceSampleRate : outputRate;

    active  = false;
    elapsed = 0;

    updateDerived();
}

void SampleVoice::prepare (double outputSampleRate, int maxBlockSize)
{
    // Die Voice haelt keinen eigenen Puffer; die Blockgroesse ist fuer sie
    // bedeutungslos. Sie wird nur geprueft, damit der Aufruf nicht wie ein
    // Versehen aussieht.
    juce::ignoreUnused (maxBlockSize);

    outputRate = (outputSampleRate > 0.0) ? outputSampleRate : 48000.0;

    active  = false;
    elapsed = 0;

    updateDerived();
}

void SampleVoice::setSettings (const VoiceSettings& newSettings)
{
    voiceSettings = newSettings;

    updateDerived();

    // Eine laufende Voice, die durch die neuen Einstellungen stumm oder
    // unspielbar wird, wird angehalten. Alles andere laeuft weiter und rechnet
    // ab dem naechsten Sample mit dem neuen Fenster.
    if (active && (! playable || voiceSettings.mute))
        active = false;
}

// ---------------------------------------------------------------------------
// Abgeleitete Werte
// ---------------------------------------------------------------------------
void SampleVoice::updateDerived() noexcept
{
    playable             = false;
    voiceLengthSamples   = 0;
    voiceLengthSeconds   = 0.0;
    readStartSourcePos   = 0.0;
    startDelaySamples    = 0;
    fadeInSeconds        = 0.0;
    fadeOutSeconds       = 0.0;

    const auto safeOutputRate = juce::jmax (1.0, outputRate);
    const auto safeSourceRate = juce::jmax (1.0, sourceRate);

    // Erweiterungspunkt B-44: Pitch und Stretch wirken NUR hier. In P3 sind
    // beide Faktoren 1,0, der Schritt ist damit genau sourceRate/outputRate.
    stepPerOutputSample = (sourceRate > 0.0 && outputRate > 0.0)
                              ? (safeSourceRate / safeOutputRate) * pitchRatio / juce::jmax (1.0e-9, stretchRatio)
                              : 1.0;

    if (source == nullptr || (! (stepPerOutputSample > 0.0)))
        return;

    const auto sourceSamples = source->getNumSamples();

    if (sourceSamples <= 0)
        return;

    const auto sourceLengthSeconds = (double) sourceSamples / safeSourceRate;

    // --- Trim-Fenster ------------------------------------------------------
    const auto trimStart = juce::jlimit (0.0, sourceLengthSeconds,
                                         juce::jmax (0.0, voiceSettings.trimStartSeconds));

    auto trimEnd = voiceSettings.trimEndSeconds;

    // 0 oder "Ende vor Start" heisst: bis zum Ende des Samples.
    if (! (trimEnd > trimStart))
        trimEnd = sourceLengthSeconds;

    trimEnd = juce::jlimit (trimStart, sourceLengthSeconds, trimEnd);

    const auto windowSeconds = trimEnd - trimStart;

    if (! (windowSeconds > 0.0))
        return;

    readStartSourcePos = trimStart * safeSourceRate;

    // --- Laenge in Ausgabe-Samples ----------------------------------------
    // Bewusst NICHT ueber "Quell-Samples / step" gerechnet: dabei wuerde sich
    // sourceRate im Zaehler und Nenner nur fast herauskuerzen und die Laenge
    // koennte je nach Rundung um ein Sample springen (44,1 kHz -> 48 kHz).
    // Die Dauer folgt direkt aus dem Zeitbezug:
    //     Dauer = Fensterlaenge * stretchRatio / pitchRatio
    // (bei Pitch = Stretch = 1,0 also genau die Fensterlaenge in Sekunden).
    const auto durationSeconds = windowSeconds
                                     * juce::jmax (1.0e-9, stretchRatio)
                                     / juce::jmax (1.0e-9, pitchRatio);

    const auto lengthSamples = durationSeconds * safeOutputRate;

    if (! (lengthSamples >= 1.0))
        return;

    voiceLengthSamples = (juce::int64) std::ceil (lengthSamples);
    voiceLengthSeconds = (double) voiceLengthSamples / safeOutputRate;

    // --- Fades: relativ zur getrimmten Laenge, auf diese geklemmt ---------
    fadeInSeconds  = juce::jlimit (0.0, voiceLengthSeconds,
                                   juce::jmax (0.0, voiceSettings.fadeIn.lengthSeconds));
    fadeOutSeconds = juce::jlimit (0.0, voiceLengthSeconds,
                                   juce::jmax (0.0, voiceSettings.fadeOut.lengthSeconds));

    // --- Position: Verschiebung auf der Kick-Zeitachse --------------------
    startDelaySamples = (juce::int64) std::llround (juce::jmax (0.0, voiceSettings.positionSeconds)
                                                        * safeOutputRate);

    playable = true;
}

// ---------------------------------------------------------------------------
// Wiedergabe
// ---------------------------------------------------------------------------
void SampleVoice::startAt (juce::int64 outputSamplePosition)
{
    // Fenster neu rechnen: die Einstellungen koennen sich seit dem letzten Start
    // geaendert haben (Trim, Position, Fades).
    updateDerived();

    elapsed = 0;

    // updateDerived() hat startDelaySamples aus `positionSeconds` gesetzt; der
    // Trigger-Zeitpunkt kommt additiv dazu (in der Regel 0).
    startDelaySamples += juce::jmax ((juce::int64) 0, outputSamplePosition);

    active = playable && (! voiceSettings.mute);
}

void SampleVoice::reset()
{
    active  = false;
    elapsed = 0;
}

float SampleVoice::gainForSample (double timeInVoice, juce::int64 sampleInVoice) const noexcept
{
    auto gain = voiceSettings.gainLinear;

    // --- Einblenden --------------------------------------------------------
    auto fadeInFactor = 1.0f;

    if (fadeInSeconds > 0.0 && timeInVoice < fadeInSeconds)
        fadeInFactor = shapeGain (voiceSettings.fadeIn.shape,
                                  (float) (timeInVoice / fadeInSeconds),
                                  voiceSettings.fadeIn.shapeAmount);

    // Declick am Anfang: die Rampe ist eine OBERgrenze (min), keine Untergrenze.
    // Dadurch erzwingt sie mindestens kDeclickSamples Anstieg, veraendert aber
    // einen bereits laufenden (laengeren) Fade nicht.
    const auto attackRamp = juce::jlimit (0.0f, 1.0f,
                                          (float) ((double) (sampleInVoice + 1)
                                                       / (double) kDeclickSamples));

    gain *= juce::jmin (fadeInFactor, attackRamp);

    // --- Ausblenden --------------------------------------------------------
    // Der Fade-Out wird ueber den VERBLEIBENDEN Anteil ausgewertet: bei
    // p = 1 (Beginn des Fades) gilt der Kurvenwert 1, bei p = 0 der Wert 0.
    auto fadeOutFactor = 1.0f;

    if (fadeOutSeconds > 0.0 && timeInVoice > voiceLengthSeconds - fadeOutSeconds)
        fadeOutFactor = shapeGain (voiceSettings.fadeOut.shape,
                                   (float) ((voiceLengthSeconds - timeInVoice) / fadeOutSeconds),
                                   voiceSettings.fadeOut.shapeAmount);

    // Declick am Ende: das letzte Sample der Voice ist exakt 0.
    const auto remaining   = (double) (voiceLengthSamples - 1 - sampleInVoice);
    const auto releaseRamp = juce::jlimit (0.0f, 1.0f,
                                           (float) (remaining / (double) kDeclickSamples));

    gain *= juce::jmin (fadeOutFactor, releaseRamp);

    return gain;
}

void SampleVoice::renderNextBlock (juce::AudioBuffer<float>& target, int startSample, int numSamples)
{
    if (! active || source == nullptr)
        return;

    const auto targetChannels = target.getNumChannels();
    const auto targetSamples  = target.getNumSamples();

    if (targetChannels <= 0 || targetSamples <= 0 || numSamples <= 0)
        return;

    if (startSample < 0 || startSample >= targetSamples)
        return;

    const auto samples = juce::jmin (numSamples, targetSamples - startSample);

    if (samples <= 0)
        return;

    const auto sourceChannels = juce::jmin (source->getNumChannels(), kMaxChannels);

    if (sourceChannels <= 0)
        return;

    const auto lastSourceIndex = source->getNumSamples() - 1;

    if (lastSourceIndex < 0)
        return;

    // Zeiger einmal holen (kein Allokieren, nur Adressrechnung).
    const float* sourceData[kMaxChannels] = { nullptr, nullptr };

    for (int ch = 0; ch < sourceChannels; ++ch)
        sourceData[ch] = source->getReadPointer (ch);

    const auto outputRateSafe = juce::jmax (1.0, outputRate);

    for (int n = 0; n < samples; ++n)
    {
        const auto local = elapsed + (juce::int64) n;

        // Position verschiebt den Start auf der Kick-Zeitachse.
        if (local < startDelaySamples)
            continue;

        const auto voiceIndex = local - startDelaySamples;

        if (voiceIndex >= voiceLengthSamples)
        {
            active = false;   // Ende der Voice erreicht
            break;
        }

        const auto timeInVoice = (double) voiceIndex / outputRateSafe;
        const auto gain        = gainForSample (timeInVoice, voiceIndex);

        if (gain == 0.0f)
            continue;

        // --- Leseposition mit linearer Interpolation ----------------------
        const auto readPos = readStartSourcePos + (double) voiceIndex * stepPerOutputSample;

        auto index = (int) readPos;
        index = juce::jlimit (0, lastSourceIndex, index);

        const auto fraction = juce::jlimit (0.0f, 1.0f, (float) (readPos - (double) index));
        const auto nextIndex = juce::jmin (lastSourceIndex, index + 1);

        float sourceValue[kMaxChannels] = { 0.0f, 0.0f };

        for (int ch = 0; ch < sourceChannels; ++ch)
        {
            const auto a = sourceData[ch][index];
            const auto b = sourceData[ch][nextIndex];
            sourceValue[ch] = a + (b - a) * fraction;
        }

        // --- Additiv in das Ziel ------------------------------------------
        if (targetChannels == 1)
        {
            // Mono-Ziel: Mono-Quelle direkt, Stereo-Quelle als Mittelwert.
            auto value = sourceValue[0];

            if (sourceChannels > 1)
                value = 0.5f * (sourceValue[0] + sourceValue[1]);

            target.getWritePointer (0)[startSample + n] += value * gain;
        }
        else
        {
            for (int ch = 0; ch < targetChannels; ++ch)
            {
                const auto sourceChannel = (sourceChannels == 1) ? 0
                                                                 : juce::jmin (ch, sourceChannels - 1);

                target.getWritePointer (ch)[startSample + n] += sourceValue[sourceChannel] * gain;
            }
        }
    }

    // Die lokale Sample-Zeit laeuft auch dann weiter, wenn die Voice innerhalb
    // dieses Blocks zu Ende ging: der Renderer zaehlt ebenfalls den ganzen Block.
    elapsed += (juce::int64) samples;

    if (active && elapsed >= startDelaySamples + voiceLengthSamples)
        active = false;
}

} // namespace bfka
