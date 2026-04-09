#pragma once
#include <JuceHeader.h>
#include <array>
#include <vector>
#include "Deconfliction/IDeconflictionStrategy.h"

enum class RecipePreset { Warm, Aggressive, Hollow, Dense, Custom };

struct Voice
{
    float fundamentalHz = 0.0f;
    float phases[7]     = {};       // H2..H8 oscillator phases (radians)
    float amps[7]       = {};       // effective amplitudes after deconfliction
    int   midiNote      = -1;
    int   voiceIndex    = 0;
    bool  active        = false;

    void reset()
    {
        fundamentalHz = 0.0f;
        std::fill(phases, phases + 7, 0.0f);
        std::fill(amps,   amps   + 7, 0.0f);
        midiNote    = -1;
        voiceIndex  = 0;
        active      = false;
    }
};

class HarmonicGenerator
{
public:
    void prepare(double sampleRate, int maxBlockSize);
    void reset();

    void setEffectModePitch(float hz) noexcept;

    void noteOn(int midiNote, int velocity);
    void noteOff(int midiNote);
    int  getActiveVoiceCount() const noexcept;
    void setMaxVoices(int n) noexcept { maxVoices = juce::jlimit(1, 8, n); }

    void setPreset(RecipePreset preset);
    void setHarmonicAmp(int harmonic, float amp);
    void setHarmonicPhase(int harmonic, float deg);
    void setRotation(float degrees) noexcept { rotationDeg = degrees; }
    void setSaturation(float amount) noexcept { saturation = juce::jlimit(0.0f, 1.0f, amount); }
    std::array<float, 7> getHarmonicAmplitudes() const noexcept { return recipeAmps; }

    void setPhantomStrength(float s) noexcept { phantomStrength = juce::jlimit(0.0f, 1.0f, s); }

    void setDeconflictionStrategy(IDeconflictionStrategy* strategy) noexcept { deconfliction = strategy; }

    void process(juce::AudioBuffer<float>& buffer);

private:
    double sampleRate      = 44100.0;
    float  phantomStrength = 1.0f;
    float  rotationDeg     = 0.0f;
    float  saturation      = 0.0f;
    int    maxVoices       = 4;

    std::array<float, 7> recipeAmps   = { 0.80f, 0.70f, 0.50f, 0.35f, 0.20f, 0.12f, 0.07f };
    std::array<float, 7> recipePhases = {};

    std::vector<Voice> voicePool;
    Voice effectVoice;

    IDeconflictionStrategy* deconfliction = nullptr;

    void renderVoice(Voice& v, juce::AudioBuffer<float>& buffer, int numSamples);
    float midiNoteToHz(int note) const noexcept;
    int   findFreeVoice() const noexcept;
    int   findVoiceForNote(int midiNote) const noexcept;
};
