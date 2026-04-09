#pragma once
#include "IDeconflictionStrategy.h"

class OddEvenStrategy : public IDeconflictionStrategy {
public:
    void resolve(std::vector<Voice>& voices) override;
};
