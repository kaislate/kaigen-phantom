#include "HarmonicGenerator.h"
#include <cmath>
#include <algorithm>

static constexpr float kTwoPi = 6.28318530718f;

void HarmonicGenerator::prepare(double sr, int /*maxBlockSize*/)
{
    sampleRate = sr;
    voicePool.resize(8);
    for (auto& v : voicePool) v.reset();
    effectVoice.reset();
}

void HarmonicGenerator::reset()
{
    for (auto& v : voicePool) v.reset();
    effectVoice.reset();
}

void HarmonicGenerator::setEffectModePitch(float hz) noexcept
{
    effectVoice.fundamentalHz = hz;
    effectVoice.active        = (hz > 0.0f);
    for (int i = 0; i < 7; ++i)
        effectVoice.amps[i] = recipeAmps[i];
}

void HarmonicGenerator::noteOn(int midiNote, int /*velocity*/) noexcept
{
    if (findVoiceForNote(midiNote) >= 0) return;

    int idx = findFreeVoice();
    if (idx < 0) return;

    Voice& v = voicePool[idx];
    v.reset();
    v.midiNote      = midiNote;
    v.fundamentalHz = midiNoteToHz(midiNote);
    v.voiceIndex    = idx;
    v.active        = true;
    for (int i = 0; i < 7; ++i) v.amps[i] = recipeAmps[i];
}

void HarmonicGenerator::noteOff(int midiNote) noexcept
{
    int idx = findVoiceForNote(midiNote);
    if (idx >= 0) voicePool[idx].reset();
}

int HarmonicGenerator::getActiveVoiceCount() const noexcept
{
    int count = 0;
    for (const auto& v : voicePool)
        if (v.active) ++count;
    return count;
}

void HarmonicGenerator::setPreset(RecipePreset preset) noexcept
{
    switch (preset)
    {
        case RecipePreset::Warm:
            recipeAmps = { 0.80f, 0.70f, 0.50f, 0.35f, 0.20f, 0.12f, 0.07f }; break;
        case RecipePreset::Aggressive:
            recipeAmps = { 0.40f, 0.50f, 0.90f, 1.00f, 0.80f, 0.50f, 0.30f }; break;
        case RecipePreset::Hollow:
            recipeAmps = { 0.10f, 0.80f, 0.10f, 0.70f, 0.10f, 0.60f, 0.10f }; break;
        case RecipePreset::Dense:
            recipeAmps = { 0.85f, 0.85f, 0.85f, 0.85f, 0.85f, 0.85f, 0.85f }; break;
        case RecipePreset::Custom:
            break;
    }
}

void HarmonicGenerator::setHarmonicAmp(int harmonic, float amp) noexcept
{
    if (harmonic >= 2 && harmonic <= 8)
        recipeAmps[harmonic - 2] = juce::jlimit(0.0f, 1.0f, amp);
}

void HarmonicGenerator::setHarmonicPhase(int harmonic, float deg) noexcept
{
    if (harmonic >= 2 && harmonic <= 8)
        recipePhases[harmonic - 2] = deg;
}

void HarmonicGenerator::process(juce::AudioBuffer<float>& buffer)
{
    if (phantomStrength < 1e-5f) return;

    const int numSamples = buffer.getNumSamples();

    if (effectVoice.active)
    {
        for (int i = 0; i < 7; ++i) effectVoice.amps[i] = recipeAmps[i];
        renderVoice(effectVoice, buffer, numSamples);
    }
    else
    {
        if (deconfliction != nullptr)
            deconfliction->resolve(voicePool);

        for (auto& v : voicePool)
            if (v.active) renderVoice(v, buffer, numSamples);
    }
}

void HarmonicGenerator::renderVoice(Voice& v, juce::AudioBuffer<float>& buffer, int numSamples)
{
    if (v.fundamentalHz <= 0.0f) return;

    const float sr    = (float)sampleRate;
    const float scale = phantomStrength / 7.0f;

    const float rotNorm  = rotationDeg / 360.0f;
    const int   rotSteps = (int)(rotNorm * 7.0f + 0.5f) % 7;

    for (int i = 0; i < 7; ++i)
    {
        const int   harmIdx     = (i + rotSteps) % 7;
        const float harmNum     = (float)(harmIdx + 2);
        const float phaseOffset = recipePhases[harmIdx] * (kTwoPi / 360.0f);
        const float amp         = v.amps[harmIdx] * scale;
        if (amp < 1e-6f) continue;

        const float phaseInc = kTwoPi * harmNum * v.fundamentalHz / sr;
        const float k     = (saturation > 1e-4f) ? (1.0f + saturation * 9.0f) : 0.0f;
        const float tanhK = (saturation > 1e-4f) ? std::tanh(k) : 1.0f;

        for (int s = 0; s < numSamples; ++s)
        {
            float sample = std::sin(v.phases[harmIdx] + phaseOffset) * amp;

            if (saturation > 1e-4f)
                sample = std::tanh(sample * k) / tanhK;

            buffer.addSample(0, s, sample);
            buffer.addSample(1, s, sample);

            v.phases[harmIdx] += phaseInc;
            v.phases[harmIdx] = std::fmod(v.phases[harmIdx], kTwoPi);
        }
    }
}

float HarmonicGenerator::midiNoteToHz(int note) const noexcept
{
    return 440.0f * std::pow(2.0f, (note - 69) / 12.0f);
}

int HarmonicGenerator::findFreeVoice() const noexcept
{
    // Count active voices across the whole pool first.
    int active = 0;
    for (const auto& v : voicePool)
        if (v.active) ++active;
    if (active >= maxVoices) return -1;

    // Safe to allocate — find the first free slot.
    for (int i = 0; i < (int)voicePool.size(); ++i)
        if (!voicePool[i].active) return i;

    return -1;
}

int HarmonicGenerator::findVoiceForNote(int midiNote) const noexcept
{
    for (int i = 0; i < (int)voicePool.size(); ++i)
        if (voicePool[i].active && voicePool[i].midiNote == midiNote) return i;
    return -1;
}
