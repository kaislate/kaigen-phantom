#pragma once
#include "IDeconflictionStrategy.h"

class ResidueStrategy : public IDeconflictionStrategy {
public:
    void resolve(std::vector<Voice>& voices) override;
};
