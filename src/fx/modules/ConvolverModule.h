#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <vector>

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

#include "fx/FxModule.h"
#include "fx/dsp/Biquad.h"

namespace bfka
{

/**
 * Faltungs-Hall (`FxType::convolver`, Referenz-String `"reverb"`).
 *
 * Funktionsintegration des **Convology XT** (Impulse Record / Wave Arts) in das
 * Rack. Die Parameter des Referenzformats (`wet`, `stereo`, `tone`, `speed`,
 * `ir`) bleiben in ihrer Reihenfolge erhalten; die Convology-Funktionen sind
 * zusaetzlich als Attribute bzw. Zustands-Kinder verfuegbar.
 *
 * ## Funktionsumfang
 *
 * * IR-Laden aus WAV/AIF, mono, stereo und **True Stereo** (4 Kanaele:
 *   L->L, L->R, R->L, R->R).
 * * **Time**: Start-Crop, Attack, Hold, Release als Huellkurve auf der IR.
 * * **Freq**: EQ-Kurve (11 Stuetzpunkte, ± 24 dB) und frequenzabhaengige
 *   **Decay-Kurve** (11 Stuetzpunkte, 0,1x … 2x).
 * * **IR Settings**: Normalize, IR Gain, Reverse, Mono Input.
 * * **Modulation**: Stereo-Modulator, 90°-Quadratur, Depth, Rate.
 * * **Scale**: Decay (exponentielle IR-Huellkurve), Stretch (Resampling der IR,
 *   aendert die wahrgenommene Tonhoehe), Predelay.
 * * **Master**: MIX (Ketten-MIX), Output Gain (Ketten-OUT-GAIN), Width
 *   (0 % mono … 200 % Shuffler).
 *
 * ## Faltung
 *
 * `juce::dsp::Convolution`: Zero-Latency-Kopf (non-uniform, 1024er-Kopf) oder
 * wahlweise fester Latenz (2048 Samples, CPU-sparsamer). True-Stereo laeuft
 * als zwei parallele Stereo-Faltungen mit getrennten IR-Kanaelen.
 *
 * ## Threading
 *
 * Die IR-Aufbereitung (Resampling, Huellkurven, Normalisierung) laeuft auf dem
 * Message-Thread; das Ergebnis wird als `IrKernel` ueber einen atomaren
 * Zeiger an den Audio-Thread uebergeben. Der Audio-Thread ruft nur
 * `loadImpulseResponse()` (laut JUCE wait-free) auf und allokiert nicht.
 */
class ConvolverModule final : public FxModule
{
public:
    static constexpr int numCurvePoints = 11;
    static constexpr int maxChannels    = 2;

    ConvolverModule();
    ~ConvolverModule() override;

    // --- FxModule ---------------------------------------------------------
    FxType type() const noexcept override { return FxType::convolver; }

    const std::vector<FxParamDesc>& params() const noexcept override { return descriptors; }

    void prepare (double newSampleRate, int maxBlockSize) override;
    void reset() override;

    void setParam (int index, float value) override;
    float getParam (int index) const override;

    void process (juce::AudioBuffer<float>& buffer) override;

    int latencySamples() const noexcept override { return reportedLatency; }

    juce::String displayName() const override { return "Convolver"; }

    juce::ValueTree getState() const override;
    void setState (const juce::ValueTree& state) override;

    // --- Diagnose / Tests -------------------------------------------------
    bool   hasImpulseResponse() const noexcept { return processedIrLength > 0; }
    int    currentIrLength() const noexcept { return processedIrLength; }
    int    currentIrChannels() const noexcept { return processedIrChannels; }
    double currentIrSampleRate() const noexcept { return rawIrSampleRate; }
    bool   isTrueStereo() const noexcept { return rawIrChannels >= 4; }

    /** Laedt eine IR direkt aus einem Puffer (Tests und eingebettete IRs). */
    void loadImpulseFromBuffer (const juce::AudioBuffer<float>& ir, double irSampleRate);

    /** Setzt IR-Pfad (wie das `ir`-Attribut im Referenzformat). */
    void setImpulsePath (const juce::String& path);

    const juce::String& impulsePath() const noexcept { return irPath; }

    /** EQ-/Decay-Kurve (Werte in dB bzw. Faktor) setzen; Count wird geklemmt. */
    void setEqCurve (const float* values, int count);
    void setDecayCurve (const float* values, int count);

private:
    enum ParamIndex
    {
        pWet = 0,
        pStereo,
        pTone,
        pSpeed,
        pPredelay,
        pDecay,
        pModDepth,
        pModRate,
        pMod90,
        pReverse,
        pMonoIn,
        pNormalize,
        pIrGain,
        pTrueStereo,
        pZeroLat,
        pEnvStart,
        pEnvAttack,
        pEnvHold,
        pEnvRelease,
        numParams
    };

    /** Aufbereitete IR-Buffers fuer die beiden Faltungen. */
    struct IrKernel
    {
        juce::AudioBuffer<float> a;   ///< (L->L, L->R) bzw. (L, R)
        juce::AudioBuffer<float> b;   ///< (R->L, R->R) nur bei True Stereo
        double sampleRate = 0.0;
        bool   trueStereo = false;
    };

    /** Kontext fuer `juce::dsp::Convolution` mit getrennten Ein-/Ausgaengen. */
    struct ConvolutionContext
    {
        using SampleType = float;

        juce::dsp::AudioBlock<const float> inputBlock;
        juce::dsp::AudioBlock<float>       outputBlock;
        bool isBypassed = false;

        const juce::dsp::AudioBlock<const float>& getInputBlock() const noexcept { return inputBlock; }

        juce::dsp::AudioBlock<float>& getOutputBlock() const noexcept
        {
            return const_cast<ConvolutionContext*> (this)->outputBlock;
        }
    };

    double smoothingCoefficient (int numSamples, double timeMs) const noexcept;

    void createEngines();
    void deleteEngines();
    void reprepareEngines();
    void rebuildKernel (bool synchronous);
    void loadKernelIntoEngines (IrKernel& kernel);

    bool loadImpulseFile (const juce::String& path);

    static void applyTimeEnvelope (juce::AudioBuffer<float>& ir, double sampleRateToUse,
                                   double startMs, double attackMs, double holdMs, double releaseMs);
    static void applyDecayCurve (juce::AudioBuffer<float>& ir, double sampleRateToUse,
                                 const std::array<float, numCurvePoints>& curve);
    static void applyReverse (juce::AudioBuffer<float>& ir);
    static void applyStretch (juce::AudioBuffer<float>& ir, double semitones);
    static void applyDecayScale (juce::AudioBuffer<float>& ir, double decay);
    static void applyNormalisation (juce::AudioBuffer<float>& ir);

    static double curveValueAt (const std::array<float, numCurvePoints>& curve, double position01) noexcept;

    void updateEqFilters();
    void applyEqAndTone (juce::AudioBuffer<float>& buffer, int numSamples);
    void applyWidth (juce::AudioBuffer<float>& buffer, int numSamples);
    void applyModulation (juce::AudioBuffer<float>& buffer, int numSamples);

    // --- Konfiguration ----------------------------------------------------
    double sampleRate   = 48000.0;
    int    maxBlockSize = 512;

    std::vector<FxParamDesc> descriptors;

    std::atomic<float> paramTargets[numParams];
    double             paramValues[numParams] {};

    // --- IR-Quelle (Message-Thread) ---------------------------------------
    juce::AudioBuffer<float> rawIr;
    double rawIrSampleRate = 0.0;
    int    rawIrChannels   = 0;

    juce::String irPath;

    int processedIrLength   = 0;
    int processedIrChannels = 0;

    std::array<float, numCurvePoints> eqCurve {};
    std::array<float, numCurvePoints> decayCurve {};

    // --- Handover Message -> Audio ----------------------------------------
    std::atomic<IrKernel*> pendingKernel { nullptr };

    // --- Engine (Audio-Thread) --------------------------------------------
    std::unique_ptr<juce::dsp::Convolution> convolutionA;
    std::unique_ptr<juce::dsp::Convolution> convolutionB;
    bool bKernelLoaded = false;

    juce::AudioBuffer<float> inputA;
    juce::AudioBuffer<float> inputB;
    juce::AudioBuffer<float> outputA;
    juce::AudioBuffer<float> outputB;

    juce::AudioBuffer<float> wetBuffer;

    // Predelay-Ringe
    std::array<std::vector<float>, (size_t) maxChannels> predelayLines;
    std::array<int, (size_t) maxChannels> predelayWrite {};
    int predelaySamples = 0;

    // Modulations-Delaylines
    std::array<std::vector<float>, (size_t) maxChannels> modLines;
    std::array<int, (size_t) maxChannels> modWrite {};
    int modLineSize = 0;
    double modPhase = 0.0;

    // Tone (Tilt) und EQ-Kurve
    std::array<dsp::Biquad, (size_t) maxChannels> toneLow;
    std::array<dsp::Biquad, (size_t) maxChannels> toneHigh;
    std::array<std::array<dsp::Biquad, numCurvePoints>, (size_t) maxChannels> eqBands;

    double toneGainDb = 0.0;

    bool  prepared       = false;
    int   reportedLatency = 0;
    bool  engineIsZeroLatency = true;

    juce::AudioFormatManager formatManager;
};

} // namespace bfka
