#pragma once

#include "fx/FxModule.h"

namespace bfka
{

/**
 * Modul "Filters" (Referenz-Typ "filt").

 * Sechs Filtertypen auf der Summe:
 *
 *   LP / HP   echte Flanken 6, 12, 18, 24, 30 dB/Oktave mit resonantem Knie
 *   BAND      Bandpass mit 12 oder 24 dB
 *   NOTCH     Kerbe bis zum Boden; SLOPE breit/eng, RESO schaerfer
 *   PHASER    1 bis 6 Allpass-Kerben um CUTOFF, Polaritaet +/- (Start: -)
 *   COMB      Feedback-Kamm, auf CUTOFF gestimmt, Polaritaet +/-
 *
 * DRIVE saettigt das Signal vor dem Filter. Es gibt keine Struktur-Liste,
 * `getState()/setState()` bleiben deshalb auf den Defaults.
 */
class FiltersModule final : public FxModule
{
public:
    /** Reihenfolge der Parameter in `params()`; zugleich Index fuer setParam(). */
    enum ParamIndex
    {
        paramType = 0,
        paramSlope,
        paramCutoff,
        paramReso,
        paramDrive,
        paramPolarity,
        numParams
    };

    /** Filtertypen; die Zahlenwerte entsprechen dem Referenzwert von `type`. */
    enum class FilterType { lowPass = 0, highPass = 1, band = 2, notch = 3, phaser = 4, comb = 5 };

    FiltersModule();
    ~FiltersModule() override;

    // --- FxModule ---------------------------------------------------------
    FxType type() const noexcept override { return FxType::filters; }
    const std::vector<FxParamDesc>& params() const noexcept override { return descriptors; }
    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() override;
    void setParam (int index, float value) override;
    float getParam (int index) const override;
    void process (juce::AudioBuffer<float>& buffer) override;
    juce::String displayName() const override { return "Filters"; }

private:
    /** Anzeige-Einheiten eins Blocks. */
    struct BlockParams
    {
        FilterType type = FilterType::lowPass;
        int   poles   = 2;      ///< 1..5 fuer 6..30 dB/Oktave
        float cutoff  = 1000.0f;
        float reso    = 0.2f;   ///< 0..1
        float drive   = 0.0f;   ///< 0..1
        bool  invert  = false;
    };

    void updateCoefficients (const BlockParams& params);
    void processFilters (juce::AudioBuffer<float>& buffer, int numChannels, int numSamples);
    void processComb (juce::AudioBuffer<float>& buffer, int numChannels, int numSamples, const BlockParams& params);

    static constexpr int kMaxStages = 3;
    static constexpr int kMaxPhaserNotches = 6;
    static constexpr int kMaxChannels = 2;

    std::vector<FxParamDesc> descriptors;
    std::array<float, (size_t) numParams> values {};

    double preparedSampleRate = 48000.0;
    int    preparedBlockSize  = 512;

    // Biquad-Kaskade (TDF2) je Kanal und Stufe
    struct Biquad
    {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
        double z1[kMaxChannels] {}, z2[kMaxChannels] {};

        float process (float in, int channel) noexcept;
        void  reset() noexcept;
    };

    std::array<Biquad, (size_t) kMaxStages> filters;
    int    activeStages = 0;
    bool   bypassFilters = false;

    // Allpass-Kaskade fuer den Phaser
    std::array<Biquad, (size_t) kMaxPhaserNotches> allpasses;
    int    activeNotches = 0;

    // Kamm-Filter
    juce::AudioBuffer<float> combBuffer;
    int combWritePos = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FiltersModule)
};

} // namespace bfka
