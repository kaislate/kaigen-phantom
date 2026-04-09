#pragma once
#include "IDeconflictionStrategy.h"
#include <array>

class BinauralStrategy : public IDeconflictionStrategy
{
public:
    BinauralStrategy() noexcept { panPositions.fill(0.0f); }

    void  resolve(std::vector<Voice>& voices) noexcept override;
    float getPanForVoice(int voiceIndex) const noexcept;

private:
    std::array<float, 8> panPositions {};
};
