#include "BassExtractor.h"

void BassExtractor::prepare(double sr, int blockSize, int numChannels)
{
    sampleRate = sr;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sr;
    spec.maximumBlockSize = (juce::uint32) blockSize;
    spec.numChannels      = (juce::uint32) juce::jmax(1, numChannels);

    filter.prepare(spec);
    filter.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);  // type is unused in dual-output mode
    setCrossoverHz(crossoverHz);
}

void BassExtractor::reset()
{
    filter.reset();
}

void BassExtractor::setCrossoverHz(float hz)
{
    crossoverHz = juce::jlimit(20.0f, 20000.0f, hz);
    filter.setCutoffFrequency(crossoverHz);
}

void BassExtractor::process(int channel, const float* in, float* lowOut, float* highOut, int n)
{
    // The dual-output processSample produces both low and high bands from a
    // single filter state — guaranteeing perfect reconstruction.
    for (int i = 0; i < n; ++i)
    {
        float lo = 0.0f, hi = 0.0f;
        filter.processSample(channel, in[i], lo, hi);
        lowOut[i]  = lo;
        highOut[i] = hi;
    }
}
