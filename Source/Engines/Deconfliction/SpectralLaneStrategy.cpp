#include "SpectralLaneStrategy.h"
#include "../HarmonicGenerator.h"
#include <algorithm>
#include <cmath>

void SpectralLaneStrategy::resolve(std::vector<Voice>& voices)
{
    std::vector<Voice*> active;
    for (auto& v : voices) if (v.active) active.push_back(&v);
    if (active.size() <= 1) return;

    const int n = (int)active.size();
    std::sort(active.begin(), active.end(),
              [](const Voice* a, const Voice* b) { return a->fundamentalHz < b->fundamentalHz; });

    for (int v = 0; v < n; ++v)
    {
        const float centre = (n > 1) ? (float)v / (float)(n - 1) : 0.5f;
        for (int i = 0; i < 7; ++i)
        {
            const float pos    = (float)i / 6.0f;
            const float dist   = std::abs(pos - centre);
            const float weight = std::exp(-dist * dist / (2.0f * 0.4f * 0.4f));
            active[v]->amps[i] *= weight;
        }
    }
}
