#pragma once
#include "IDeconflictionStrategy.h"
#include <array>

class StaggerStrategy : public IDeconflictionStrategy
{
public:
    StaggerStrategy() noexcept { delaySamples.fill(0); }

    void setDelayMs(float ms, double sampleRate) noexcept;
    void resolve(std::vector<Voice>& voices) noexcept override;
    int  getDelaySamplesForVoice(int voiceIndex) const noexcept;

private:
    float  delayMs    = 8.0f;
    double sampleRate = 44100.0;
    std::array<int, 8> delaySamples {};
};
