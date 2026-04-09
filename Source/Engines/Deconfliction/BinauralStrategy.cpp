#include "BinauralStrategy.h"
#include "../HarmonicGenerator.h"

void BinauralStrategy::resolve(std::vector<Voice>& voices)
{
    std::vector<Voice*> active;
    for (auto& v : voices) if (v.active) active.push_back(&v);
    if (active.empty()) return;

    panPositions.fill(0.0f);

    const int n = (int)active.size();
    if (n == 1)
    {
        panPositions[active[0]->voiceIndex] = 0.0f;
        return;
    }

    for (int i = 0; i < n; ++i)
    {
        float pan = -0.7f + (1.4f * (float)i / (float)(n - 1));
        panPositions[active[i]->voiceIndex] = pan;
    }
}

float BinauralStrategy::getPanForVoice(int voiceIndex) const noexcept
{
    if (voiceIndex < 0 || voiceIndex >= (int)panPositions.size()) return 0.0f;
    return panPositions[voiceIndex];
}
