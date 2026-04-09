#include "PartitionStrategy.h"
#include "../HarmonicGenerator.h"
#include <array>

void PartitionStrategy::resolve(std::vector<Voice>& voices) noexcept
{
    std::array<Voice*, 8> active {};
    int n = 0;
    for (auto& v : voices)
        if (v.active && n < 8) active[n++] = &v;
    if (n <= 1) return;

    for (int i = 0; i < 7; ++i)
    {
        int owner = i % n;
        for (int v = 0; v < n; ++v)
            if (v != owner) active[v]->amps[i] = 0.0f;
    }
}
