#pragma once
#include "IDeconflictionStrategy.h"

class SpectralLaneStrategy : public IDeconflictionStrategy {
public:
    void resolve(std::vector<Voice>& voices) noexcept override;
};
