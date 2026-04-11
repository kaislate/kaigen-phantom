#pragma once
#include <JuceHeader.h>
#include "Parameters.h"
#include "Engines/PhantomEngine.h"

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

    // Public state for the editor
    juce::AudioProcessorValueTreeState apvts;

    // Real-time data exposed to the UI
    std::atomic<float> currentPitch { -1.0f };
    std::atomic<float> peakInL  { 0.0f };
    std::atomic<float> peakInR  { 0.0f };
    std::atomic<float> peakOutL { 0.0f };
    std::atomic<float> peakOutR { 0.0f };

    static constexpr int kSpectrumBins = 80;
    std::array<float, kSpectrumBins> spectrumData {};
    std::atomic<bool> spectrumReady { false };

private:
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();

    void syncParamsToEngine();

    PhantomEngine engine;

    double sampleRate = 44100.0;

    // FFT for spectrum analysis
    static constexpr int kFftOrder = 11;
    static constexpr int kFftSize  = 1 << kFftOrder;
    juce::dsp::FFT spectrumFFT { kFftOrder };
    std::array<float, kFftSize * 2> fftBuffer {};
    int fftWritePos = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhantomProcessor)
};
