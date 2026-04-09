#include "ResidueStrategy.h"
#include "../HarmonicGenerator.h"
#include <cmath>
#include <array>

void ResidueStrategy::resolve(std::vector<Voice>& voices) noexcept
{
    std::array<Voice*, 8> active {};
    int n = 0;
    for (auto& v : voices)
        if (v.active && n < 8) active[n++] = &v;
    if (n <= 1) return;

    for (int i = 0; i < 7; ++i)
    {
        const float harmNum = (float)(i + 2);
        std::array<float, 8> freqs {};
        for (int fi = 0; fi < n; ++fi) freqs[fi] = harmNum * active[fi]->fundamentalHz;

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
            active[a]->amps[i] = juce::jlimit(0.0f, 1.0f, active[a]->amps[i] * factor);
        }
    }
}
