#include "engine/KickRenderer.h"

#include <cmath>

namespace bfka
{

namespace
{
    /** Lineare Verstaerkung zu einem dB-Wert. */
    float dbToGain (double decibels) noexcept
    {
        return (float) std::pow (10.0, decibels / 20.0);
    }
} // namespace

// ---------------------------------------------------------------------------
// Lebenszyklus
// ---------------------------------------------------------------------------
void KickRenderer::prepare (double newSampleRate, int maxBlockSize)
{
    preparedSampleRate = (newSampleRate > 0.0) ? newSampleRate : 48000.0;
    preparedBlockSize  = juce::jmax (1, maxBlockSize);

    // Einzige Allokation des Renderers; im Audio-Thread wird nie wieder
    // Speicher angefordert.
    scratch.setSize (kChannels, preparedBlockSize, false, true, true);

    prepared = true;

    updateTimeline();

    // Die Voices kennen die Ausgaberate; die Quellen bleiben erhalten.
    for (auto& voice : voices)
        voice.prepare (preparedSampleRate, preparedBlockSize);

    reset();
}

void KickRenderer::reset()
{
    playing             = false;
    samplesSinceTrigger = 0;

    // Eine evtl. laufende Rampe sofort abschliessen, damit nach einem Stop kein
    // halb ausgesteuerter Zustand stehen bleibt.
    autoGainRampRemaining = 0;
    autoGainRampStep      = 0.0f;
    autoGainCurrent       = autoGainTarget;

    resetBodyMeasurement();

    for (auto& voice : voices)
        voice.reset();
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------
void KickRenderer::setSlot (int slotIndex,
                            const juce::AudioBuffer<float>* sample,
                            double sourceSampleRate,
                            const VoiceSettings& newSettings)
{
    if (slotIndex < 0 || slotIndex >= kNumSlots)
        return;

    const auto index = (size_t) slotIndex;

    voiceSettings[index] = newSettings;
    slotHasSample[index] = (sample != nullptr);

    auto& voice = voices[index];
    voice.prepare (preparedSampleRate, preparedBlockSize);
    voice.setSource (sample, sourceSampleRate);
    voice.setSettings (newSettings);

    updateSoloState();

    // Die Belegung hat sich geaendert: der Autogain-Messwert ist ungueltig.
    // Die Verstaerkung selbst bleibt stehen (kein Pegelsprung); der naechste
    // Kick misst neu und korrigiert ueber die Rampe.
    resetBodyMeasurement();
}

void KickRenderer::clearSlot (int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= kNumSlots)
        return;

    const auto index = (size_t) slotIndex;

    voiceSettings[index] = VoiceSettings();
    slotHasSample[index] = false;

    voices[index].setSource (nullptr, preparedSampleRate);
    voices[index].reset();

    updateSoloState();
    resetBodyMeasurement();
}

const VoiceSettings& KickRenderer::slotSettings (int slotIndex) const noexcept
{
    static const VoiceSettings empty;

    if (slotIndex < 0 || slotIndex >= kNumSlots)
        return empty;

    return voiceSettings[(size_t) slotIndex];
}

void KickRenderer::setSlotSettings (int slotIndex, const VoiceSettings& newSettings)
{
    if (slotIndex < 0 || slotIndex >= kNumSlots)
        return;

    const auto index = (size_t) slotIndex;

    voiceSettings[index] = newSettings;
    voices[index].setSettings (newSettings);

    updateSoloState();
}

bool KickRenderer::isSlotFilled (int slotIndex) const noexcept
{
    if (slotIndex < 0 || slotIndex >= kNumSlots)
        return false;

    return slotHasSample[(size_t) slotIndex];
}

void KickRenderer::updateSoloState() noexcept
{
    anySolo = false;

    for (size_t i = 0; i < (size_t) kNumSlots; ++i)
    {
        // Nur belegte Slots loesen Solo aus: ein leerer Solo-Slot darf den Kick
        // nicht stumm schalten.
        if (slotHasSample[i] && voiceSettings[i].solo)
        {
            anySolo = true;
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// Einstellungen und Zeitachse
// ---------------------------------------------------------------------------
// B-83b: `setRenderSettings()` laeuft auch aus `processBlock()` (FOLLOW DAW)
// und muss deshalb allokationsfrei bleiben. `updateTimeline()` rechnet nur,
// `startAutoGainRamp()` setzt Zaehler; keine der beiden Funktionen fordert
// Speicher an.
void KickRenderer::setRenderSettings (const RenderSettings& newSettings)
{
    settings = newSettings;

    updateTimeline();

    // Autogain abgeschaltet: zurueck auf Einheitsverstaerkung (mit Rampe).
    if (! settings.autoGain)
        startAutoGainRamp (1.0f);
}

void KickRenderer::updateTimeline() noexcept
{
    const auto rate = juce::jmax (1.0, preparedSampleRate);

    beatSamples = (juce::int64) std::llround (beatLengthSeconds() * rate);

    if (beatSamples < 1)
        beatSamples = 1;

    tailFadeSamples = (juce::int64) std::llround (juce::jmax (0.0, settings.tailFadeSeconds) * rate);
    tailFadeSamples = juce::jlimit ((juce::int64) 0, beatSamples, tailFadeSamples);

    // Messfenster des Autogains (20 ms .. 120 ms nach dem Trigger).
    bodyStartSample = (juce::int64) std::llround (kBodyWindowStartSeconds * rate);
    bodyEndSample   = (juce::int64) std::llround (kBodyWindowEndSeconds * rate);

    if (bodyEndSample <= bodyStartSample)
        bodyEndSample = bodyStartSample + 1;
}

double KickRenderer::beatLengthSeconds() const noexcept
{
    auto tempo = settings.bpm;

    if (! (tempo > 0.0))          // faengt auch NaN ab
        tempo = 150.0;

    tempo = juce::jlimit (kMinBpm, kMaxBpm, tempo);

    return 60.0 / tempo;
}

double KickRenderer::kickLengthSeconds() const noexcept
{
    if (settings.cutToOneBeat)
        return beatLengthSeconds();

    // KICK LENGTH = FULL SAMPLE: die laengste belegte Voice bestimmt die Laenge.
    double longest = 0.0;

    for (size_t i = 0; i < voices.size(); ++i)
        if (slotHasSample[i] && voices[i].hasPlayableWindow())
            longest = juce::jmax (longest, voices[i].lengthSeconds());

    return longest > 0.0 ? longest : beatLengthSeconds();
}

// ---------------------------------------------------------------------------
// Wiedergabe
// ---------------------------------------------------------------------------
void KickRenderer::noteOn()
{
    if (! prepared)
        return;

    updateSoloState();

    playing             = true;
    samplesSinceTrigger = 0;

    resetBodyMeasurement();

    for (size_t i = 0; i < (size_t) kNumSlots; ++i)
    {
        auto& voice = voices[i];

        const auto audible = slotHasSample[i]
                                 && (! voiceSettings[i].mute)
                                 && ((! anySolo) || voiceSettings[i].solo);

        if (audible)
            voice.startAt (0);
        else
            voice.reset();
    }
}

void KickRenderer::renderNextBlock (juce::AudioBuffer<float>& target, int startSample, int numSamples)
{
    if (! prepared || (! playing) || numSamples <= 0)
        return;

    const auto targetChannels = target.getNumChannels();
    const auto targetSamples  = target.getNumSamples();

    if (targetChannels <= 0 || targetSamples <= 0)
        return;

    if (startSample < 0 || startSample >= targetSamples)
        return;

    auto samples = juce::jmin (numSamples, targetSamples - startSample, scratch.getNumSamples());

    if (samples <= 0)
        return;

    // KICK LENGTH = 1 BEAT: hinter dem Beat traegt der Renderer nichts mehr bei.
    if (settings.cutToOneBeat)
    {
        const auto remaining = beatSamples - samplesSinceTrigger;

        samples = (int) juce::jlimit ((juce::int64) 0, (juce::int64) samples, remaining);
    }

    if (samples <= 0)
    {
        finishKick();
        return;
    }

    // --- 1) Summe der Voices ----------------------------------------------
    scratch.clear (0, samples);

    for (auto& voice : voices)
        if (voice.isActive())
            voice.renderNextBlock (scratch, 0, samples);

    // --- 2) Body-Pegel messen (bewusst VOR der Autogain-Stufe) -------------
    accumulateBodyEnergy (samples);

    // --- 3) Kick-Laenge und Autogain ---------------------------------------
    applyOutputGain (samples);

    // --- 4) Additiv in das Ziel --------------------------------------------
    addToTarget (target, startSample, samples);

    samplesSinceTrigger += (juce::int64) samples;

    if ((! bodyFinalised) && samplesSinceTrigger >= bodyEndSample)
        finaliseBodyMeasurement();

    if (settings.cutToOneBeat && samplesSinceTrigger >= beatSamples)
        finishKick();
    else if (! anyVoiceActive())
        finishKick();
}

void KickRenderer::finishKick() noexcept
{
    finaliseBodyMeasurement();

    playing = false;

    for (auto& voice : voices)
        voice.reset();
}

bool KickRenderer::anyVoiceActive() const noexcept
{
    return numActiveVoices() > 0;
}

int KickRenderer::numActiveVoices() const noexcept
{
    int count = 0;

    for (const auto& voice : voices)
        if (voice.isActive())
            ++count;

    return count;
}

// ---------------------------------------------------------------------------
// Autogain
// ---------------------------------------------------------------------------
void KickRenderer::resetAutoGain() noexcept
{
    resetBodyMeasurement();
    startAutoGainRamp (1.0f);
}

void KickRenderer::resetBodyMeasurement() noexcept
{
    bodySumSquares = 0.0;
    bodyValues     = 0;
    bodyFinalised  = false;
    bodyMeasured   = false;
    bodyRmsDbValue = 0.0f;
}

void KickRenderer::accumulateBodyEnergy (int numSamples) noexcept
{
    if (bodyFinalised || numSamples <= 0)
        return;

    const auto from = juce::jmax (samplesSinceTrigger, bodyStartSample);
    const auto to   = juce::jmin (samplesSinceTrigger + (juce::int64) numSamples, bodyEndSample);

    if (to <= from)
        return;

    const auto offset   = (int) (from - samplesSinceTrigger);
    const auto count    = (int) (to - from);
    const auto channels = scratch.getNumChannels();

    double sum = 0.0;

    for (int ch = 0; ch < channels; ++ch)
    {
        const auto* data = scratch.getReadPointer (ch, offset);

        for (int n = 0; n < count; ++n)
        {
            const auto value = (double) data[n];
            sum += value * value;
        }
    }

    bodySumSquares += sum;
    bodyValues     += (juce::int64) count * (juce::int64) channels;
}

void KickRenderer::finaliseBodyMeasurement() noexcept
{
    if (bodyFinalised)
        return;

    bodyFinalised = true;

    if ((! settings.autoGain) || settings.autoGainTargetDb != settings.autoGainTargetDb)
        return;

    if (bodyValues <= 0)
        return;

    const auto meanSquare = bodySumSquares / (double) bodyValues;

    if (! (meanSquare > 0.0))
        return;   // Stille: keine Korrektur, keine Division durch 0

    // 20 * log10 (rms) == 10 * log10 (meanSquare)
    bodyRmsDbValue = (float) (10.0 * std::log10 (meanSquare));
    bodyMeasured   = true;

    const auto correctionDb = juce::jlimit (-(double) kMaxAutoGainDb,
                                            (double) kMaxAutoGainDb,
                                            (double) settings.autoGainTargetDb
                                                - (double) bodyRmsDbValue);

    startAutoGainRamp (dbToGain (correctionDb));
}

void KickRenderer::startAutoGainRamp (float targetLinear) noexcept
{
    autoGainTarget = (targetLinear > 0.0f) ? targetLinear : 0.0f;

    const auto rampSamples = juce::jmax (1,
                                         (int) std::lround (kAutoGainRampSeconds
                                                                * juce::jmax (1.0, preparedSampleRate)));

    const auto delta = autoGainTarget - autoGainCurrent;

    if (std::abs (delta) <= 1.0e-6f)
    {
        autoGainCurrent       = autoGainTarget;
        autoGainRampStep      = 0.0f;
        autoGainRampRemaining = 0;
        return;
    }

    autoGainRampStep      = delta / (float) rampSamples;
    autoGainRampRemaining = rampSamples;
}

// ---------------------------------------------------------------------------
// Huelle und Summenausgabe
// ---------------------------------------------------------------------------
void KickRenderer::applyOutputGain (int numSamples) noexcept
{
    juce::int64 fadeLength = 0;
    juce::int64 fadeStart  = 0;

    if (settings.cutToOneBeat && tailFadeSamples > 0)
    {
        fadeLength = tailFadeSamples;
        fadeStart  = juce::jmax ((juce::int64) 0, beatSamples - fadeLength);
    }

    const auto channels = scratch.getNumChannels();

    for (int n = 0; n < numSamples; ++n)
    {
        auto gain = 1.0f;

        // Weiches Ausblenden am Beat-Ende: raised cosine von 1 auf 0. Das letzte
        // Sample vor dem Beat landet exakt auf 0, damit der Schnitt klicktfrei ist.
        if (fadeLength > 0)
        {
            const auto position = samplesSinceTrigger + (juce::int64) n + 1;

            if (position > fadeStart)
            {
                const auto u = juce::jlimit (0.0, 1.0,
                                             (double) (position - fadeStart) / (double) fadeLength);

                gain = (float) (0.5 * (1.0 + std::cos (juce::MathConstants<double>::pi * u)));
            }
        }

        // Autogain: lineare Rampe ueber mindestens kAutoGainRampSeconds.
        if (autoGainRampRemaining > 0)
        {
            autoGainCurrent += autoGainRampStep;
            --autoGainRampRemaining;

            if (autoGainRampRemaining == 0)
                autoGainCurrent = autoGainTarget;
        }

        gain *= autoGainCurrent;

        if (gain == 1.0f)
            continue;

        for (int ch = 0; ch < channels; ++ch)
            scratch.getWritePointer (ch)[n] *= gain;
    }
}

void KickRenderer::addToTarget (juce::AudioBuffer<float>& target,
                                int startSample,
                                int numSamples) const noexcept
{
    const auto targetChannels  = target.getNumChannels();
    const auto scratchChannels = scratch.getNumChannels();

    if (targetChannels <= 0 || scratchChannels <= 0)
        return;

    if (targetChannels == 1)
    {
        // Mono-Ziel: Mittelwert aus links und rechts (der Kick ist stereo).
        const auto* left  = scratch.getReadPointer (0);
        const auto* right = (scratchChannels > 1) ? scratch.getReadPointer (1) : left;
        auto* output      = target.getWritePointer (0);

        for (int n = 0; n < numSamples; ++n)
            output[startSample + n] += 0.5f * (left[n] + right[n]);

        return;
    }

    for (int ch = 0; ch < targetChannels; ++ch)
    {
        const auto sourceChannel = juce::jmin (ch, scratchChannels - 1);

        const auto* input = scratch.getReadPointer (sourceChannel);
        auto* output      = target.getWritePointer (ch);

        for (int n = 0; n < numSamples; ++n)
            output[startSample + n] += input[n];
    }
}

} // namespace bfka
