#include "BinauralStrategy.h"
#include "../HarmonicGenerator.h"
#include <array>

void BinauralStrategy::resolve(std::vector<Voice>& voices) noexcept
{
    std::array<Voice*, 8> active {};
    int n = 0;
    for (auto& v : voices)
        if (v.active && n < 8) active[n++] = &v;
    if (n == 0) return;

    panPositions.fill(0.0f);

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
