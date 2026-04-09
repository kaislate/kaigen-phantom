#pragma once
#include <JuceHeader.h>
#include <vector>

struct Voice;  // forward declared — full definition in HarmonicGenerator.h

class IDeconflictionStrategy
{
public:
    virtual ~IDeconflictionStrategy() = default;

    // Called before rendering. Implementations may redistribute harmonic
    // amplitudes across voices to reduce phantom collisions.
    // voices: all voices in the pool (check active flag before processing)
    virtual void resolve(std::vector<Voice>& voices) noexcept = 0;
};
