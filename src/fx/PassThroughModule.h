#pragma once

#include <atomic>

#include "fx/FxModule.h"

namespace bfka
{

/**
 * Platzhalter-Modul fuer das FX-Rack (Tests und Reihenfolge-Pruefungen).
 *
 * Das Modul bearbeitet das Signal **nicht** — es ist klanglich ein
 * Durchreichen. Damit sich die MIX-Blende und die OUT-GAIN-Stufe der Kette
 * trotzdem pruefen lassen, hat es genau einen Parameter:
 *
 *   `amount` (0…400 %, Default 100 %)
 *   100 %  -> bitidentisches Durchreichen (Gain exakt 1.0)
 *   sonst  -> reine Pegelskalierung mit `amount / 100`
 *   > 100 % ist bewusst erlaubt, damit Tests die MIX-Blende gegen einen
 *   verstaerkten Nasspfad pruefen koennen (z. B. 200 % -> Nass = 2x Trocken).
 *
 * Es erzeugt damit keinen Klang und keine Verzerrung; es skaliert nur.
 *
 * `reportedLatencySamples` ist eine reine Test-/Diagnosehilfe: Module, die
 * ihre Latenz nur melden, ohne selbst zu verzoegern, erlauben es, die
 * Latenzbehandlung der Kette (trocken/nass-Flucht, Bypass, PDC-Summe) ohne
 * echten DSP zu pruefen. Standard 0.
 */
class PassThroughModule final : public FxModule
{
public:
    explicit PassThroughModule (FxType moduleType, int reportedLatencySamples = 0)
        : typeOfModule (moduleType),
          reportedLatency (juce::jmax (0, reportedLatencySamples))
    {
        descriptors.push_back ({ "amount", "Amount", 0.0f, 400.0f, 100.0f, "%" });
        values.push_back (100.0f);
    }

    // --- FxModule ---------------------------------------------------------
    FxType type() const noexcept override { return typeOfModule; }

    const std::vector<FxParamDesc>& params() const noexcept override { return descriptors; }

    void prepare (double sampleRate, int maxBlockSize) override
    {
        preparedSampleRate = sampleRate;
        preparedBlockSize  = maxBlockSize;
        reset();
    }

    void reset() override
    {
        // Kein interner Zustand: es gibt nichts zu loeschen.
    }

    void setParam (int index, float value) override
    {
        if (index < 0 || index >= (int) descriptors.size())
            return;

        const auto& desc = descriptors[(size_t) index];
        const auto clamped = juce::jlimit (desc.minValue, desc.maxValue, value);

        values[(size_t) index] = clamped;

        if (index == 0)
            gain.store (clamped * 0.01f, std::memory_order_relaxed);
    }

    float getParam (int index) const override
    {
        if (index < 0 || index >= (int) values.size())
            return 0.0f;

        return values[(size_t) index];
    }

    void process (juce::AudioBuffer<float>& buffer) override
    {
        const auto g = gain.load (std::memory_order_relaxed);

        if (g != 1.0f)
            buffer.applyGain (g);
    }

    int latencySamples() const noexcept override { return reportedLatency; }

    juce::String displayName() const override
    {
        return "Pass-Through (" + toString (typeOfModule) + ")";
    }

    // --- nur fuer Tests ---------------------------------------------------
    /** Zuletzt vorbereitete Sample-Rate. */
    double sampleRate() const noexcept { return preparedSampleRate; }

    /** Zuletzt vorbereitete Blockgroesse. */
    int blockSize() const noexcept { return preparedBlockSize; }

private:
    FxType typeOfModule;
    int    reportedLatency    = 0;
    double preparedSampleRate = 48000.0;
    int    preparedBlockSize  = 512;

    std::vector<FxParamDesc> descriptors;
    std::vector<float>       values;
    std::atomic<float>       gain { 1.0f };
};

} // namespace bfka
