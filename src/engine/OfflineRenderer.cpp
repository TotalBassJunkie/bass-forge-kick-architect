// B-110: Offline-Renderer fuer Kick + FX-Kette.

#include "engine/OfflineRenderer.h"

#include <cmath>

#include "engine/KickRenderer.h"
#include "fx/FxChainProcessor.h"

namespace bfka
{

OfflineRenderResult OfflineRenderer::render (KickRenderer& renderer,
                                             FxChainProcessor& fxChain,
                                             double sampleRate,
                                             int blockSize,
                                             double lengthSeconds)
{
    OfflineRenderResult result;

    if (sampleRate <= 0.0 || blockSize <= 0)
    {
        result.error = "Ungueltige Rate oder Blockgroesse";
        return result;
    }

    // Beide Stufen auf dieselbe Rate/Blockgroesse bringen.
    renderer.prepare (sampleRate, blockSize);
    fxChain.prepare (sampleRate, blockSize);

    // Ziellaenge: explizit gewuenscht oder Kick-Laenge plus FX-Nachhall.
    const double targetSeconds = (lengthSeconds > 0.0)
                                     ? lengthSeconds
                                     : juce::jmax (0.05,
                                                   renderer.kickLengthSeconds()
                                                       + fxChain.tailSeconds());

    int totalSamples = (int) std::lround (targetSeconds * sampleRate);

    if (totalSamples < 1)
        totalSamples = 1;

    result.buffer.setSize (2, totalSamples, false, true, true);
    result.buffer.clear();

    juce::AudioBuffer<float> scratch (2, blockSize);

    renderer.noteOn();

    for (int pos = 0; pos < totalSamples; pos += blockSize)
    {
        const int n = juce::jmin (blockSize, totalSamples - pos);

        scratch.clear();
        renderer.renderNextBlock (scratch, 0, n);
        fxChain.process (scratch);

        for (int ch = 0; ch < 2; ++ch)
            result.buffer.copyFrom (ch, pos, scratch, ch, 0, n);
    }

    // Der Renderer wird gestoppt, die Kette bleibt unangetastet.
    renderer.reset();

    result.sampleRate = sampleRate;
    result.ok         = true;
    return result;
}

int OfflineRenderer::blockCountFor (double seconds, double sampleRate, int blockSize) noexcept
{
    if (seconds <= 0.0 || sampleRate <= 0.0 || blockSize <= 0)
        return 0;

    return (int) std::ceil (seconds * sampleRate / (double) blockSize);
}

} // namespace bfka
