#pragma once
#include "IDeconflictionStrategy.h"

class PartitionStrategy : public IDeconflictionStrategy {
public:
    void resolve(std::vector<Voice>& voices) override;
};
