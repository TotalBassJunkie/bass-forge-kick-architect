#include "engine/KickAssembler.h"

#include "core/Log.h"
#include "core/ParameterRegistry.h"

#include <cmath>

namespace bfka
{

namespace
{
    /** dB -> linear. */
    float dbToLinear (float db) noexcept
    {
        return db <= -100.0f ? 0.0f : std::pow (10.0f, db * 0.05f);
    }

    /** Liest einen normalisierten Parameter und gibt den Anzeigewert zurueck. */
    float engineering (const KickState& state, const juce::String& id, float fallback)
    {
        const auto* desc = ParameterRegistry::get().find (id);

        if (desc == nullptr)
            return fallback;

        return desc->toEngineering (state.getParam (id));
    }

    FadeShape shapeFromCurve (float curvePercent) noexcept
    {
        // `ficurve` ist der Biegegrad des Fades (0..100). Wir bilden ihn auf die
        // fuenf Kurvenformen ab: Mitte = linear, Extremwerte = Fast/Slow.
        if (curvePercent <= 5.0f)
            return FadeShape::linear;

        if (curvePercent < 35.0f)
            return FadeShape::fast;

        if (curvePercent > 95.0f)
            return FadeShape::slow;

        if (curvePercent > 65.0f)
            return FadeShape::sCurve;

        return FadeShape::curve;
    }
}

KickAssembler::KickAssembler()
{
    formats.registerBasicFormats();
}

juce::File KickAssembler::resolveSampleFile (const SampleRef& ref, const juce::File& contentRoot)
{
    if (ref.relPath.isNotEmpty())
    {
        const auto relative = contentRoot.getChildFile (ref.relPath);

        if (relative.existsAsFile())
            return relative;
    }

    if (ref.absPath.isNotEmpty())
    {
        const juce::File absolute (ref.absPath);

        if (absolute.existsAsFile())
            return absolute;
    }

    return {};
}

VoiceSettings KickAssembler::voiceSettingsForSlot (const KickState& state,
                                                   int slot,
                                                   double trimmedLengthSeconds)
{
    VoiceSettings settings;
    const auto prefix = "l" + juce::String (slot) + "_";

    const auto volDb = engineering (state, prefix + "vol", 0.0f);
    settings.gainLinear = dbToLinear (volDb);
    settings.mute       = engineering (state, prefix + "mute", 0.0f) >= 0.5f;
    settings.solo       = engineering (state, prefix + "solo", 0.0f) >= 0.5f;

    // `pos` ist der Versatz des Parts auf der Kick-Zeitachse (0..100 %).
    // Die genaue Skala ist offen (O2 in docs/specs/voice-and-render.md);
    // bis zur Messung am Original gilt: 50 % = keine Verschiebung.
    const auto posPercent = engineering (state, prefix + "pos", 50.0f);
    settings.positionSeconds = (posPercent - 50.0) * 0.01;

    const auto trimStartPercent = engineering (state, prefix + "trimstart", 0.0f);
    const auto trimEndPercent   = engineering (state, prefix + "trimend", 100.0f);
    settings.trimStartSeconds = trimmedLengthSeconds * (double) (trimStartPercent * 0.01f);
    settings.trimEndSeconds   = trimmedLengthSeconds * (double) (trimEndPercent * 0.01f);

    const auto fadeInPercent  = engineering (state, prefix + "fadein", 0.0f);
    const auto fadeOutPercent = engineering (state, prefix + "fadeout", 0.0f);
    const auto curvePercent   = engineering (state, prefix + "ficurve", 50.0f);
    const auto shapeAmount    = engineering (state, prefix + "fishape", 0.0f);

    settings.fadeIn.lengthSeconds  = trimmedLengthSeconds * (double) (fadeInPercent * 0.01f);
    settings.fadeIn.shape          = shapeFromCurve (curvePercent);
    settings.fadeIn.shapeAmount    = shapeAmount;

    settings.fadeOut.lengthSeconds = trimmedLengthSeconds * (double) (fadeOutPercent * 0.01f);
    settings.fadeOut.shape         = shapeFromCurve (curvePercent);
    settings.fadeOut.shapeAmount   = -shapeAmount;

    // B-42/B-43: Pitch in Halbtoenen (-24..+24), Stretch als Zeitfaktor
    // (0,25..8) und die globale Betriebsart KEEP PITCH / REPITCH.
    settings.pitchSemitones = (double) engineering (state, prefix + "pitch", 0.0f);
    settings.stretchFactor  = (double) engineering (state, prefix + "stretch", 1.0f);
    settings.mode           = state.stretchMode();

    return settings;
}

bool KickAssembler::loadSlotWithSettings (int slot,
                                          const juce::File& file,
                                          const VoiceSettings& settings,
                                          KickRenderer& renderer,
                                          juce::StringArray& warnings)
{
    if (! juce::isPositiveAndBelow (slot, KickState::kNumSlots))
        return false;

    // Ein fehlgeschlagener Ladevorgang darf keine alte Huelle stehen lassen.
    envelopes[(size_t) slot] = {};

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr)
    {
        warnings.add ("Nicht lesbar: " + file.getFileName());
        return false;
    }

    const auto numSamples  = (int) juce::jmin (reader->lengthInSamples, (juce::int64) (1 << 26));
    const auto numChannels = (int) juce::jlimit (1u, 2u, reader->numChannels);

    if (numSamples <= 0)
    {
        warnings.add ("Leer: " + file.getFileName());
        return false;
    }

    auto buffer = std::make_unique<juce::AudioBuffer<float>> (numChannels, numSamples);
    buffer->clear();

    reader->read (buffer.get(), 0, numSamples, 0, true, numChannels > 1);

    buffers[(size_t) slot]     = std::move (buffer);
    sourceRates[(size_t) slot] = reader->sampleRate > 0.0 ? reader->sampleRate : 48000.0;

    auto effective = settings;
    const auto sampleLength = (double) numSamples / sourceRates[(size_t) slot];

    // Trim-Endwert 0 bedeutet "bis zum Ende".
    if (effective.trimEndSeconds <= effective.trimStartSeconds)
        effective.trimEndSeconds = sampleLength;

    renderer.setSlot (slot, buffers[(size_t) slot].get(), sourceRates[(size_t) slot], effective);

    buildEnvelope (*buffers[(size_t) slot], sourceRates[(size_t) slot], envelopes[(size_t) slot]);
    return true;
}

bool KickAssembler::loadSlot (int slot,
                              const juce::File& file,
                              const KickState& state,
                              KickRenderer& renderer,
                              juce::StringArray& warnings)
{
    std::unique_ptr<juce::AudioFormatReader> probe (formats.createReaderFor (file));

    if (probe == nullptr)
    {
        warnings.add ("Nicht lesbar: " + file.getFileName());
        return false;
    }

    const auto rate   = probe->sampleRate > 0.0 ? probe->sampleRate : 48000.0;
    const auto length = probe->lengthInSamples > 0
                            ? (double) probe->lengthInSamples / rate
                            : 0.0;
    probe.reset();

    return loadSlotWithSettings (slot, file, voiceSettingsForSlot (state, slot, length), renderer, warnings);
}

int KickAssembler::loadFromState (const KickState& state,
                                  const juce::File& contentRoot,
                                  KickRenderer& renderer,
                                  juce::StringArray& warnings)
{
    int loaded = 0;

    for (int slot = 0; slot < KickState::kNumSlots; ++slot)
    {
        const auto& ref = state.getSample (slot);

        if (! ref.has_value() || ! ref->isValid())
        {
            clearSlot (slot, renderer);
            continue;
        }

        const auto file = resolveSampleFile (*ref, contentRoot);

        if (! file.existsAsFile())
        {
            warnings.add ("Fehlt: " + ref->fileName);
            clearSlot (slot, renderer);
            continue;
        }

        if (loadSlot (slot, file, state, renderer, warnings))
            ++loaded;
    }

    auto renderSettings = renderer.renderSettings();
    renderSettings.bpm = state.getBpm();
    renderer.setRenderSettings (renderSettings);

    return loaded;
}

bool KickAssembler::loadOneSlot (int slot,
                                 const KickState& state,
                                 const juce::File& contentRoot,
                                 KickRenderer& renderer,
                                 juce::StringArray& warnings)
{
    if (! juce::isPositiveAndBelow (slot, KickState::kNumSlots))
        return false;

    const auto& ref = state.getSample (slot);

    if (! ref.has_value() || ! ref->isValid())
    {
        clearSlot (slot, renderer);
        return false;
    }

    const auto file = resolveSampleFile (*ref, contentRoot);

    if (! file.existsAsFile())
    {
        warnings.add ("Fehlt: " + ref->fileName);
        clearSlot (slot, renderer);
        return false;
    }

    return loadSlot (slot, file, state, renderer, warnings);
}

void KickAssembler::clearSlot (int slot, KickRenderer& renderer)
{
    if (! juce::isPositiveAndBelow (slot, KickState::kNumSlots))
        return;

    buffers[(size_t) slot].reset();
    envelopes[(size_t) slot] = {};

    renderer.clearSlot (slot);
}

bool KickAssembler::applySlotSettings (int slot, const KickState& state, KickRenderer& renderer)
{
    if (! juce::isPositiveAndBelow (slot, KickState::kNumSlots))
        return false;

    if (buffers[(size_t) slot] == nullptr)
        return false;

    const auto length = slotLengthSeconds (slot);
    const auto settings = voiceSettingsForSlot (state, slot, length);

    // Trim-Ende 0 heisst "bis zum Ende" — dieselbe Regel wie beim Laden.
    auto effective = settings;

    if (effective.trimEndSeconds <= effective.trimStartSeconds)
        effective.trimEndSeconds = length;

    renderer.setSlotSettings (slot, effective);
    return true;
}

double KickAssembler::slotLengthSeconds (int slot) const noexcept
{
    if (! juce::isPositiveAndBelow (slot, KickState::kNumSlots))
        return 0.0;

    const auto& buffer = buffers[(size_t) slot];

    if (buffer == nullptr || buffer->getNumSamples() <= 0)
        return 0.0;

    const auto rate = sourceRates[(size_t) slot] > 0.0 ? sourceRates[(size_t) slot] : 48000.0;
    return (double) buffer->getNumSamples() / rate;
}

const KickAssembler::SampleEnvelope& KickAssembler::envelope (int slot) const noexcept
{
    static const SampleEnvelope empty;

    if (! juce::isPositiveAndBelow (slot, KickState::kNumSlots))
        return empty;

    return envelopes[(size_t) slot];
}

void KickAssembler::buildEnvelope (const juce::AudioBuffer<float>& buffer,
                                   double sampleRate,
                                   SampleEnvelope& target)
{
    target = {};

    const auto numSamples  = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0)
        return;

    target.valid         = true;
    target.lengthSeconds = (double) numSamples / (sampleRate > 0.0 ? sampleRate : 48000.0);

    const auto buckets = (int) SampleEnvelope::kBuckets;
    auto peak = 0.0f;

    for (int b = 0; b < buckets; ++b)
    {
        const auto first = (juce::int64) b * numSamples / buckets;
        const auto last  = (juce::int64) (b + 1) * numSamples / buckets;

        auto lo = 0.0f;
        auto hi = 0.0f;
        auto seen = false;

        for (auto i = first; i < last; ++i)
        {
            auto value = 0.0f;

            for (int ch = 0; ch < numChannels; ++ch)
                value += buffer.getSample (ch, (int) i);

            value /= (float) numChannels;

            if (! seen)
            {
                lo = hi = value;
                seen = true;
            }

            lo = juce::jmin (lo, value);
            hi = juce::jmax (hi, value);
            peak = juce::jmax (peak, std::abs (value));
            peak = juce::jmax (peak, juce::jmax (std::abs (lo), std::abs (hi)));
        }

        target.minimum[(size_t) b] = lo;
        target.maximum[(size_t) b] = hi;
    }

    target.peak = peak;
}

void KickAssembler::findDemoSamples (const juce::File& contentRoot,
                                     std::array<juce::File, 3>& files)
{
    // Bewusst schlicht: je ein Punch, ein Rumble und ein Click aus dem Techno-Umfeld.
    static const char* const folders[] {
        "Main - Punches/Techno",
        "Main - Rumbles/Rumbles (Techno)",
        "Transients (Clicks)"
    };

    for (size_t i = 0; i < files.size(); ++i)
        files[i] = findFirstSample (contentRoot, folders[i]);
}

juce::File KickAssembler::findFirstSample (const juce::File& contentRoot, const juce::String& subFolder)
{
    const auto folder = contentRoot.getChildFile (subFolder);

    if (! folder.isDirectory())
        return {};

    juce::File found;

    for (const auto& entry : juce::RangedDirectoryIterator (folder, true, "*.wav", juce::File::findFiles))
    {
        // Der erste Treffer genuegt; kein vollstaendiger Verzeichnis-Scan.
        if (entry.getFile().existsAsFile())
        {
            found = entry.getFile();
            break;
        }
    }

    return found;
}

bool KickAssembler::buildDemoKick (const juce::File& contentRoot,
                                   KickRenderer& renderer,
                                   juce::StringArray& warnings)
{
    struct DemoSlot
    {
        int slot;
        const char* folder;
        float gainDb;
    };

    // Bewusst schlicht: je ein Punch, ein Rumble und ein Click aus dem Techno-Umfeld.
    static const DemoSlot demoSlots[] {
        { 0, "Main - Punches/Techno",          0.0f },
        { 1, "Main - Rumbles/Rumbles (Techno)", -6.0f },
        { 2, "Transients (Clicks)",            -12.0f },
    };

    int loaded = 0;

    for (const auto& demo : demoSlots)
    {
        const auto file = findFirstSample (contentRoot, demo.folder);

        if (! file.existsAsFile())
        {
            warnings.add (juce::String ("Kein Sample in ") + demo.folder);
            continue;
        }

        VoiceSettings settings;
        settings.gainLinear = dbToLinear (demo.gainDb);

        if (loadSlotWithSettings (demo.slot, file, settings, renderer, warnings))
            ++loaded;

        log::i ("Demo-Slot " + juce::String (demo.slot) + ": " + file.getFileName());
    }

    auto renderSettings = renderer.renderSettings();
    renderSettings.bpm = 150.0;
    renderSettings.cutToOneBeat = true;
    renderer.setRenderSettings (renderSettings);

    return loaded > 0;
}

} // namespace bfka
