#include "ResidueStrategy.h"
#include "../HarmonicGenerator.h"
#include <cmath>

void ResidueStrategy::resolve(std::vector<Voice>& voices)
{
    std::vector<Voice*> active;
    for (auto& v : voices) if (v.active) active.push_back(&v);
    if (active.size() <= 1) return;

    const int n = (int)active.size();
    for (int i = 0; i < 7; ++i)
    {
        const float harmNum = (float)(i + 2);
        std::vector<float> freqs;
        for (auto* v : active) freqs.push_back(harmNum * v->fundamentalHz);

        for (int a = 0; a < n; ++a)
        {
            int sharedCount = 0;
            for (int b = 0; b < n; ++b)
            {
                if (a == b) continue;
                if (std::abs(freqs[a] - freqs[b]) < 5.0f) ++sharedCount;
            }
            const float factor = (sharedCount > 0)
                ? 1.0f / (float)(1 + sharedCount)
                : 1.2f;
            active[a]->amps[i] *= juce::jlimit(0.0f, 1.0f, factor);
        }
    }
}
