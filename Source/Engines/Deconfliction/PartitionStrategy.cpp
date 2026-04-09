#include "PartitionStrategy.h"
#include "../HarmonicGenerator.h"

void PartitionStrategy::resolve(std::vector<Voice>& voices)
{
    std::vector<Voice*> active;
    for (auto& v : voices) if (v.active) active.push_back(&v);
    if (active.size() <= 1) return;

    const int n = (int)active.size();
    for (int i = 0; i < 7; ++i)
    {
        int owner = i % n;
        for (int v = 0; v < n; ++v)
            if (v != owner) active[v]->amps[i] = 0.0f;
    }
}
