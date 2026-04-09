#include "PitchTracker.h"
#include <cmath>
#include <algorithm>

void PitchTracker::prepare(double sr, int maxBlockSize)
{
    sampleRate = sr;
    const int tauMax = maxBlockSize / 2;
    diffBuf.assign(tauMax, 0.0f);
    cmndBuf.assign(tauMax, 0.0f);
}

float PitchTracker::detectPitch(const float* samples, int numSamples)
{
    float raw = computeRawPitch(samples, numSamples);
    applyGlide(raw);
    return smoothedPitch;
}

float PitchTracker::computeRawPitch(const float* samples, int numSamples)
{
    const int tauMax = numSamples / 2;
    if (tauMax < 2) return -1.0f;

    // Resize buffers if called with larger block than prepare() saw
    if ((int)diffBuf.size() < tauMax)
    {
        diffBuf.assign(tauMax, 0.0f);
        cmndBuf.assign(tauMax, 0.0f);
    }

    // 1. Difference function
    for (int tau = 0; tau < tauMax; ++tau)
    {
        diffBuf[tau] = 0.0f;
        for (int j = 0; j < tauMax; ++j)
        {
            float delta = samples[j] - samples[j + tau];
            diffBuf[tau] += delta * delta;
        }
    }

    // 2. Cumulative mean normalized difference function (CMND)
    cmndBuf[0] = 1.0f;
    float runningSum = 0.0f;
    for (int tau = 1; tau < tauMax; ++tau)
    {
        runningSum += diffBuf[tau];
        cmndBuf[tau] = (runningSum > 0.0f)
            ? diffBuf[tau] * (float)tau / runningSum
            : 1.0f;
    }

    // 3. Absolute threshold + parabolic interpolation for sub-sample accuracy
    for (int tau = 2; tau < tauMax - 1; ++tau)
    {
        if (cmndBuf[tau] < yinThreshold
            && cmndBuf[tau] < cmndBuf[tau - 1]
            && cmndBuf[tau] < cmndBuf[tau + 1])
        {
            float s0 = cmndBuf[tau - 1];
            float s1 = cmndBuf[tau];
            float s2 = cmndBuf[tau + 1];
            float denom = 2.0f * (2.0f * s1 - s0 - s2);
            float betterTau = (std::abs(denom) > 1e-7f)
                ? (float)tau + (s2 - s0) / denom
                : (float)tau;

            if (betterTau > 1.0f)
                return (float)sampleRate / betterTau;
        }
    }

    return -1.0f;
}

void PitchTracker::applyGlide(float rawPitch)
{
    if (rawPitch < 0.0f)
        return;  // no pitch — hold last smoothed value

    if (smoothedPitch < 0.0f || glideMs < 1.0f)
    {
        smoothedPitch = rawPitch;
        return;
    }

    // 1-pole IIR glide
    const float blockDuration = 512.0f / (float)sampleRate * 1000.0f;
    const float coeff = 1.0f - std::exp(-blockDuration / glideMs);
    smoothedPitch = smoothedPitch + coeff * (rawPitch - smoothedPitch);
}
