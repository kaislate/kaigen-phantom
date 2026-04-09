#include "SpectralLaneStrategy.h"
#include "../HarmonicGenerator.h"
#include <algorithm>
#include <array>
#include <cmath>

void SpectralLaneStrategy::resolve(std::vector<Voice>& voices) noexcept
{
    std::array<Voice*, 8> active {};
    int n = 0;
    for (auto& v : voices)
        if (v.active && n < 8) active[n++] = &v;
    if (n <= 1) return;

    std::stable_sort(active.begin(), active.begin() + n,
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
