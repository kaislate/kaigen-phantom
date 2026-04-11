#pragma once
#include <JuceHeader.h>
#include "Parameters.h"
#include "Engines/PitchTracker.h"
#include "Engines/HarmonicGenerator.h"
#include "Engines/BinauralStage.h"
#include "Engines/PerceptualOptimizer.h"
#include "Engines/CrossoverBlend.h"
#include "Engines/Deconfliction/PartitionStrategy.h"
#include "Engines/Deconfliction/SpectralLaneStrategy.h"
#include "Engines/Deconfliction/StaggerStrategy.h"
#include "Engines/Deconfliction/OddEvenStrategy.h"
#include "Engines/Deconfliction/ResidueStrategy.h"
#include "Engines/Deconfliction/BinauralStrategy.h"

class PhantomProcessor : public juce::AudioProcessor,
                         private juce::AudioProcessorValueTreeState::Listener
{
public:
    PhantomProcessor();
    ~PhantomProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    juce::AudioProcessorParameter* getBypassParameter() const override
    {
        return apvts.getParameter(ParamID::BYPASS);
    }

    const juce::String getName() const override { return "Kaigen Phantom"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> currentPitch { -1.0f };

    // Peak levels for I/O meters
    std::atomic<float> peakInL  { 0.0f };
    std::atomic<float> peakInR  { 0.0f };
    std::atomic<float> peakOutL { 0.0f };
    std::atomic<float> peakOutR { 0.0f };

    // Spectrum data — 80 log-spaced bins
    static constexpr int kSpectrumBins = 80;
    std::array<float, kSpectrumBins> spectrumData {};
    std::atomic<bool> spectrumReady { false };

    // Diagnostics
    std::atomic<int>   fftRunCount   { 0 };
    std::atomic<float> fftMaxMagnitude { 0.0f };
    std::atomic<int>   processBlockCount { 0 };

private:
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void syncEnginesFromApvts(bool isInstrumentMode);
    void updateDeconflictionStrategy(int modeIndex);

    PitchTracker         pitchTracker;
    HarmonicGenerator    harmonicGen;
    BinauralStage        binauralStage;
    PerceptualOptimizer  perceptualOpt;
    CrossoverBlend       crossoverBlend;

    PartitionStrategy    stratPartition;
    SpectralLaneStrategy stratLane;
    StaggerStrategy      stratStagger;
    OddEvenStrategy      stratOddEven;
    ResidueStrategy      stratResidue;
    BinauralStrategy     stratBinaural;

    int    lastDeconflictionMode = -1;
    double sampleRate = 44100.0;

    juce::AudioBuffer<float> phantomBuf;
    juce::AudioBuffer<float> dryBuf;

    // FFT for spectrum analysis
    static constexpr int kFftOrder = 11;                     // 2048-point FFT
    static constexpr int kFftSize  = 1 << kFftOrder;         // 2048
    juce::dsp::FFT spectrumFFT { kFftOrder };
    std::array<float, kFftSize * 2> fftBuffer {};
    int fftWritePos = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhantomProcessor)
};
