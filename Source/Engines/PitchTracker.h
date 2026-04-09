#pragma once
#include <JuceHeader.h>
#include <vector>

class PitchTracker
{
public:
    void prepare(double sampleRate, int maxBlockSize);

    // Returns detected fundamental in Hz, or -1.0f if no pitch detected.
    // YIN confidence threshold: lower = stricter (0.05 strict, 0.30 loose)
    float detectPitch(const float* samples, int numSamples);

    void setConfidenceThreshold(float threshold) noexcept { yinThreshold = threshold; }
    void setGlideMs(float ms) noexcept { glideMs = ms; }

    // Smoothed pitch after glide applied — call after detectPitch()
    float getSmoothedPitch() const noexcept { return smoothedPitch; }

private:
    double sampleRate    = 44100.0;
    float  yinThreshold  = 0.15f;
    float  glideMs       = 20.0f;
    float  smoothedPitch = -1.0f;

    std::vector<float> diffBuf;
    std::vector<float> cmndBuf;

    float computeRawPitch(const float* samples, int numSamples);
    void  applyGlide(float rawPitch);
};
