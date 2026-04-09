#include "StaggerStrategy.h"
#include "../HarmonicGenerator.h"

void StaggerStrategy::setDelayMs(float ms, double sr) noexcept
{
    delayMs    = ms;
    sampleRate = sr;
}

void StaggerStrategy::resolve(std::vector<Voice>& voices)
{
    // delaySamples is a fixed-size std::array — no heap allocation here
    delaySamples.fill(0);
    for (auto& v : voices)
    {
        if (!v.active) continue;
        if (v.voiceIndex >= 0 && v.voiceIndex < (int)delaySamples.size())
            delaySamples[v.voiceIndex] = (int)(v.voiceIndex * delayMs * sampleRate / 1000.0);
    }
}

int StaggerStrategy::getDelaySamplesForVoice(int voiceIndex) const noexcept
{
    if (voiceIndex < 0 || voiceIndex >= (int)delaySamples.size()) return 0;
    return delaySamples[voiceIndex];
}
