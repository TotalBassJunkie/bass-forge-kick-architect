#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

#include "fx/modules/FiltersModule.h"

using namespace bfka;
using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int    kBlockSize  = 512;

    /** Sinus in einen Buffer schreiben. */
    void fillSine (juce::AudioBuffer<float>& buffer, double frequency, float amplitude, double sampleRate)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getWritePointer (ch);

            for (int n = 0; n < buffer.getNumSamples(); ++n)
                data[n] = amplitude * (float) std::sin (2.0 * juce::MathConstants<double>::pi
                                                        * frequency * n / sampleRate);
        }
    }

    /**
     * Laesst einen Sinus durch das Modul und gibt den Pegel in dB zurueck.
     * Die erste Haelfte des Puffers wird verworfen, damit Einschwingvorgaenge
     * die Messung nicht verfaelschen.
     */
    double measureGainDb (FiltersModule& module,
                          double frequency,
                          int type, int slope, float cutoff, float reso, float drive = 0.0f)
    {
        module.setParam (FiltersModule::paramType,   (float) type);
        module.setParam (FiltersModule::paramSlope,  (float) slope);
        module.setParam (FiltersModule::paramCutoff, cutoff);
        module.setParam (FiltersModule::paramReso,   reso);
        module.setParam (FiltersModule::paramDrive,  drive);
        module.setParam (FiltersModule::paramPolarity, 0.0f);

        module.reset();

        const auto numSamples = (int) (kSampleRate * 0.5);
        juce::AudioBuffer<float> buffer (2, numSamples);
        fillSine (buffer, frequency, 0.25f, kSampleRate);

        module.process (buffer);

        const auto start = numSamples / 2;
        double sumOut = 0.0;
        double sumIn  = 0.0;

        for (int n = start; n < numSamples; ++n)
        {
            const auto out = (double) buffer.getSample (0, n);
            const auto in  = (double) (0.25f * (float) std::sin (2.0 * juce::MathConstants<double>::pi
                                                                 * frequency * n / kSampleRate));
            sumOut += out * out;
            sumIn  += in * in;
        }

        if (sumIn <= 0.0 || sumOut <= 0.0)
            return -200.0;

        return 10.0 * std::log10 (sumOut / sumIn);
    }

    bool allFinite (const juce::AudioBuffer<float>& buffer)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int n = 0; n < buffer.getNumSamples(); ++n)
                if (! std::isfinite (buffer.getSample (ch, n)))
                    return false;

        return true;
    }
}

TEST_CASE ("Filters: Parameterliste entspricht dem Referenzvertrag", "[fx][filt]")
{
    FiltersModule module;

    REQUIRE (module.params().size() == 6);
    REQUIRE (module.params()[0].id == "type");
    REQUIRE (module.params()[1].id == "slope");
    REQUIRE (module.params()[2].id == "cutoff");
    REQUIRE (module.params()[3].id == "reso");
    REQUIRE (module.params()[4].id == "drive");
    REQUIRE (module.params()[5].id == "polarity");
    REQUIRE (module.type() == FxType::filters);

    // Polaritaet startet wie im Referenzprodukt auf "-".
    REQUIRE (module.getParam (FiltersModule::paramPolarity) >= 0.5f);
}

TEST_CASE ("Filters: Tiefpass daempft oberhalb der Grenzfrequenz", "[fx][filt]")
{
    FiltersModule module;
    module.prepare (kSampleRate, kBlockSize);

    const auto pass = measureGainDb (module, 100.0, 0 /* LP */, 1 /* 12 dB */, 1000.0f, 0.0f);
    const auto stop = measureGainDb (module, 10000.0, 0, 1, 1000.0f, 0.0f);

    INFO ("Durchlass " << pass << " dB, Sperre " << stop << " dB");
    REQUIRE_THAT (pass, WithinAbs (0.0, 0.6));
    REQUIRE (stop < -20.0);
}

TEST_CASE ("Filters: Slope-Stufen wirken wie angegeben", "[fx][filt]")
{
    FiltersModule module;
    module.prepare (kSampleRate, kBlockSize);

    // Eine Oktave ueber der Grenzfrequenz.
    const auto gentle = measureGainDb (module, 2000.0, 0, 1 /* 12 dB/Okt */, 1000.0f, 0.0f);
    const auto steep  = measureGainDb (module, 2000.0, 0, 3 /* 24 dB/Okt */, 1000.0f, 0.0f);

    INFO ("12 dB/Okt " << gentle << " dB, 24 dB/Okt " << steep << " dB");
    REQUIRE (gentle < -5.0);
    REQUIRE (gentle > -13.0);
    REQUIRE (steep < -17.0);
    REQUIRE (steep > -27.0);
    REQUIRE (steep < gentle - 5.0);
}

TEST_CASE ("Filters: Hochpass ist spiegelbildlich", "[fx][filt]")
{
    FiltersModule module;
    module.prepare (kSampleRate, kBlockSize);

    const auto low  = measureGainDb (module, 100.0, 1 /* HP */, 1, 1000.0f, 0.0f);
    const auto high = measureGainDb (module, 10000.0, 1, 1, 1000.0f, 0.0f);

    INFO ("100 Hz " << low << " dB, 10 kHz " << high << " dB");
    REQUIRE (low < -20.0);
    REQUIRE_THAT (high, WithinAbs (0.0, 0.6));
}

TEST_CASE ("Filters: Notch schneidet die Mittenfrequenz weg", "[fx][filt]")
{
    FiltersModule module;
    module.prepare (kSampleRate, kBlockSize);

    const auto reference = measureGainDb (module, 100.0, 3 /* NOTCH */, 1, 1000.0f, 20.0f);
    const auto atNotch   = measureGainDb (module, 1000.0, 3, 1, 1000.0f, 20.0f);

    INFO ("100 Hz " << reference << " dB, 1 kHz " << atNotch << " dB");
    REQUIRE (atNotch < reference - 20.0);
}

TEST_CASE ("Filters: Resonanz hebt den Pegel bei CUTOFF", "[fx][filt]")
{
    FiltersModule module;
    module.prepare (kSampleRate, kBlockSize);

    const auto flat  = measureGainDb (module, 1000.0, 0, 1, 1000.0f, 0.0f);
    const auto sharp = measureGainDb (module, 1000.0, 0, 1, 1000.0f, 80.0f);

    INFO ("ohne Resonanz " << flat << " dB, mit " << sharp << " dB");
    REQUIRE (sharp > flat + 3.0);
}

TEST_CASE ("Filters: bleibt bei Extremwerten stabil", "[fx][filt]")
{
    FiltersModule module;
    module.prepare (kSampleRate, kBlockSize);

    for (int type = 0; type <= 5; ++type)
    {
        for (float cutoff : { 20.0f, 20000.0f })
        {
            for (float reso : { 0.0f, 100.0f })
            {
                module.setParam (FiltersModule::paramType, (float) type);
                module.setParam (FiltersModule::paramSlope, 4.0f);
                module.setParam (FiltersModule::paramCutoff, cutoff);
                module.setParam (FiltersModule::paramReso, reso);
                module.setParam (FiltersModule::paramDrive, 100.0f);
                module.reset();

                juce::AudioBuffer<float> buffer (2, kBlockSize);

                for (int block = 0; block < 10; ++block)
                {
                    fillSine (buffer, 60.0, 1.0f, kSampleRate);
                    module.process (buffer);

                    INFO ("Typ " << type << " Cutoff " << cutoff << " Reso " << reso);
                    REQUIRE (allFinite (buffer));
                }
            }
        }
    }
}

TEST_CASE ("Filters: DRIVE bleibt im Zaun", "[fx][filt]")
{
    FiltersModule module;
    module.prepare (kSampleRate, kBlockSize);

    module.setParam (FiltersModule::paramType, 0.0f);
    module.setParam (FiltersModule::paramCutoff, 8000.0f);
    module.setParam (FiltersModule::paramDrive, 100.0f);
    module.reset();

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    fillSine (buffer, 200.0, 1.0f, kSampleRate);
    module.process (buffer);

    float peak = 0.0f;
    for (int n = 0; n < buffer.getNumSamples(); ++n)
        peak = juce::jmax (peak, std::abs (buffer.getSample (0, n)));

    REQUIRE (peak <= 2.0f);
}

TEST_CASE ("Filters: Polaritaet veraendert die Phaser-Summation", "[fx][filt]")
{
    FiltersModule module;
    module.prepare (kSampleRate, kBlockSize);

    // Die Allpass-Kerben liegen um 800 Hz; dort ist die Summe aus trocken und
    // nass am empfindlichsten gegen die Polaritaet.
    const auto measureRms = [&module] (float polarity)
    {
        module.setParam (FiltersModule::paramType, 4.0f);
        module.setParam (FiltersModule::paramSlope, 2.0f);
        module.setParam (FiltersModule::paramCutoff, 800.0f);
        module.setParam (FiltersModule::paramReso, 60.0f);
        module.setParam (FiltersModule::paramPolarity, polarity);
        module.reset();

        juce::AudioBuffer<float> buffer (1, (int) kSampleRate);
        fillSine (buffer, 800.0, 0.25f, kSampleRate);

        module.process (buffer);

        double sum = 0.0;
        for (int n = 1000; n < buffer.getNumSamples(); ++n)
            sum += (double) buffer.getSample (0, n) * buffer.getSample (0, n);

        return std::sqrt (sum / (double) (buffer.getNumSamples() - 1000));
    };

    const auto withPlus  = measureRms (0.0f);
    const auto withMinus = measureRms (1.0f);

    INFO ("+ " << withPlus << " / - " << withMinus);
    REQUIRE (withPlus > 0.0);
    REQUIRE (withMinus > 0.0);
    // Die Polaritaet muss die Summe hoerbar veraendern (mind. 10 % Unterschied).
    REQUIRE (std::abs (withPlus - withMinus) / juce::jmax (withPlus, withMinus) > 0.10);
}
