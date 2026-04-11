#include "PhantomEngine.h"

void PhantomEngine::prepare(double sr, int blockSize, int nCh)
{
    sampleRate   = sr;
    maxBlockSize = blockSize;
    numChannels  = juce::jmax(1, nCh);

    bassExtractor.prepare(sr, blockSize, numChannels);

    envelopeL.prepare(sr);
    envelopeR.prepare(sr);
    envelopeL.setAttackMs(1.0f);
    envelopeR.setAttackMs(1.0f);
    envelopeL.setReleaseMs(50.0f);
    envelopeR.setReleaseMs(50.0f);

    binaural.prepare(sr, blockSize);

    lowBuf    .setSize(numChannels, blockSize, false, true, true);
    highBuf   .setSize(numChannels, blockSize, false, true, true);
    phantomBuf.setSize(numChannels, blockSize, false, true, true);
}

void PhantomEngine::reset()
{
    bassExtractor.reset();
    envelopeL.reset();
    envelopeR.reset();
    lowBuf.clear();
    highBuf.clear();
    phantomBuf.clear();
}

// ── Parameter setters ──────────────────────────────────────────────────────
void PhantomEngine::setCrossoverHz(float hz)       { bassExtractor.setCrossoverHz(hz); }
void PhantomEngine::setHarmonicAmplitudes(const std::array<float, 7>& amps)
{
    waveshaperL.setHarmonicAmplitudes(amps);
    waveshaperR.setHarmonicAmplitudes(amps);
}
void PhantomEngine::setDrive(float d)              { waveshaperL.setDrive(d); waveshaperR.setDrive(d); }
void PhantomEngine::setSaturation(float s)         { waveshaperL.setSaturation(s); waveshaperR.setSaturation(s); }
void PhantomEngine::setGhostAmount(float a)        { ghostAmount = juce::jlimit(0.0f, 1.0f, a); }
void PhantomEngine::setGhostReplace(bool r)        { ghostReplace = r; }
void PhantomEngine::setPhantomStrength(float s)    { phantomStrength = juce::jlimit(0.0f, 1.0f, s); }
void PhantomEngine::setOutputGainDb(float db)      { outputGainLin = std::pow(10.0f, db * 0.05f); }
void PhantomEngine::setEnvelopeAttackMs(float ms)  { envelopeL.setAttackMs(ms); envelopeR.setAttackMs(ms); }
void PhantomEngine::setEnvelopeReleaseMs(float ms) { envelopeL.setReleaseMs(ms); envelopeR.setReleaseMs(ms); }
void PhantomEngine::setBinauralMode(int m)
{
    binaural.setMode(m == 0 ? BinauralMode::Off
                   : m == 1 ? BinauralMode::Spread
                            : BinauralMode::VoiceSplit);
}
void PhantomEngine::setBinauralWidth(float w)      { binaural.setWidth(juce::jlimit(0.0f, 1.0f, w)); }
void PhantomEngine::setStereoWidth(float w)        { stereoWidth = juce::jlimit(0.0f, 2.0f, w); }

// ── Processing ─────────────────────────────────────────────────────────────
void PhantomEngine::process(juce::AudioBuffer<float>& buffer)
{
    const int n   = buffer.getNumSamples();
    const int nCh = juce::jmin(buffer.getNumChannels(), numChannels);
    if (n == 0 || nCh == 0) return;

    // Ensure working buffers are large enough
    if (lowBuf.getNumSamples() < n || lowBuf.getNumChannels() < nCh)
    {
        lowBuf    .setSize(nCh, juce::jmax(n, maxBlockSize), false, true, true);
        highBuf   .setSize(nCh, juce::jmax(n, maxBlockSize), false, true, true);
        phantomBuf.setSize(nCh, juce::jmax(n, maxBlockSize), false, true, true);
    }

    // 1. Split input into bass and highs per channel
    for (int ch = 0; ch < nCh; ++ch)
    {
        bassExtractor.process(ch,
            buffer.getReadPointer(ch),
            lowBuf.getWritePointer(ch),
            highBuf.getWritePointer(ch),
            n);
    }

    // 2. Waveshape the bass band → phantomBuf
    for (int ch = 0; ch < nCh; ++ch)
    {
        auto& shaper = (ch == 0) ? waveshaperL : waveshaperR;
        shaper.process(lowBuf.getReadPointer(ch),
                        phantomBuf.getWritePointer(ch),
                        n);
    }

    // 3. Per-sample: envelope-follow input, mix with ghost, recombine
    for (int ch = 0; ch < nCh; ++ch)
    {
        auto& env = (ch == 0) ? envelopeL : envelopeR;
        const float* low     = lowBuf.getReadPointer(ch);
        const float* high    = highBuf.getReadPointer(ch);
        const float* phantom = phantomBuf.getReadPointer(ch);
        float* out = buffer.getWritePointer(ch);

        for (int i = 0; i < n; ++i)
        {
            // Envelope from the LOW band — phantom amplitude follows bass loudness
            const float inLvl = env.process(low[i]);

            // Envelope-gated phantom signal (scale by 3x to compensate for
            // waveshaper output level, then by phantom strength)
            const float phantomOut = phantom[i] * inLvl * 3.0f * phantomStrength;

            // Ghost mix: either replace or add the low band with phantom
            float mixedLow;
            if (ghostReplace)
                mixedLow = low[i] * (1.0f - ghostAmount) + phantomOut * ghostAmount;
            else
                mixedLow = low[i] + phantomOut * ghostAmount;

            // Recombine with untouched upper band and apply output gain
            out[i] = (mixedLow + high[i]) * outputGainLin;
        }
    }

    // 4. Optional binaural processing applied on the full mix
    binaural.process(buffer);

    // 5. Stereo width (simple M/S scaling) — only if stereo
    if (nCh >= 2 && std::abs(stereoWidth - 1.0f) > 1e-4f)
    {
        auto* L = buffer.getWritePointer(0);
        auto* R = buffer.getWritePointer(1);
        for (int i = 0; i < n; ++i)
        {
            const float mid  = 0.5f * (L[i] + R[i]);
            const float side = 0.5f * (L[i] - R[i]) * stereoWidth;
            L[i] = mid + side;
            R[i] = mid - side;
        }
    }
}
