#include "PhantomEngine.h"

void PhantomEngine::prepare(double sr, int blockSize, int nCh)
{
    sampleRate   = sr;
    maxBlockSize = blockSize;
    numChannels  = juce::jmax(1, nCh);

    bassExtractor.prepare(sr, blockSize, numChannels);

    synthL.prepare(sr);
    synthR.prepare(sr);

    envelopeL.prepare(sr);
    envelopeR.prepare(sr);
    envelopeL.setAttackMs(1.0f);
    envelopeR.setAttackMs(1.0f);
    envelopeL.setReleaseMs(50.0f);
    envelopeR.setReleaseMs(50.0f);

    binaural.prepare(sr, blockSize);

    const double rampSec = 0.010;
    smoothSatL.reset(sr, rampSec);
    smoothSatR.reset(sr, rampSec);
    smoothSatL.setCurrentAndTargetValue(0.0f);
    smoothSatR.setCurrentAndTargetValue(0.0f);

    lowBuf .setSize(numChannels, blockSize, false, true, true);
    highBuf.setSize(numChannels, blockSize, false, true, true);

    const auto lpfCoeff = juce::IIRCoefficients::makeLowPass(sr, 20000.0);
    const auto hpfCoeff = juce::IIRCoefficients::makeHighPass(sr, 20.0);
    lpfL.setCoefficients(lpfCoeff); lpfR.setCoefficients(lpfCoeff);
    hpfL.setCoefficients(hpfCoeff); hpfR.setCoefficients(hpfCoeff);
    lpfL.reset(); lpfR.reset();
    hpfL.reset(); hpfR.reset();
    lastLPFHz = 20000.0f;
    lastHPFHz = 20.0f;
}

void PhantomEngine::reset()
{
    bassExtractor.reset();
    synthL.reset();
    synthR.reset();
    envelopeL.reset();
    envelopeR.reset();
    lowBuf.clear();
    highBuf.clear();
    lpfL.reset(); lpfR.reset();
    hpfL.reset(); hpfR.reset();
}

// ── Parameter setters ──────────────────────────────────────────────────────

void PhantomEngine::setCrossoverHz(float hz)
{
    crossoverHz = juce::jlimit(20.0f, 500.0f, hz);
    bassExtractor.setCrossoverHz(crossoverHz);
}

void PhantomEngine::setHarmonicAmplitudes(const std::array<float, 7>& amps)
{
    synthL.setHarmonicAmplitudes(amps);
    synthR.setHarmonicAmplitudes(amps);
}

void PhantomEngine::setSaturation(float s)
{
    smoothSatL.setTargetValue(juce::jlimit(0.0f, 1.0f, s));
    smoothSatR.setTargetValue(juce::jlimit(0.0f, 1.0f, s));
}

void PhantomEngine::setSynthStep(float step)      { synthL.setStep(step); synthR.setStep(step); }
void PhantomEngine::setSynthDuty(float duty)      { synthL.setDutyCycle(duty); synthR.setDutyCycle(duty); }
void PhantomEngine::setSynthSkip(int n)           { synthL.setSkipCount(n); synthR.setSkipCount(n); }
void PhantomEngine::setGhostAmount(float a)       { ghostAmount    = juce::jlimit(0.0f, 1.0f, a); }
void PhantomEngine::setGhostReplace(bool r)       { ghostReplace   = r; }
void PhantomEngine::setPhantomStrength(float s)   { phantomStrength = juce::jlimit(0.0f, 1.0f, s); }
void PhantomEngine::setOutputGainDb(float db)     { outputGainLin  = std::pow(10.0f, db * 0.05f); }
void PhantomEngine::setEnvelopeAttackMs(float ms) { envelopeL.setAttackMs(ms);  envelopeR.setAttackMs(ms); }
void PhantomEngine::setEnvelopeReleaseMs(float ms){ envelopeL.setReleaseMs(ms); envelopeR.setReleaseMs(ms); }
void PhantomEngine::setBinauralMode(int m)
{
    binaural.setMode(m == 0 ? BinauralMode::Off
                   : m == 1 ? BinauralMode::Spread
                            : BinauralMode::VoiceSplit);
}
void PhantomEngine::setBinauralWidth(float w)  { binaural.setWidth(juce::jlimit(0.0f, 1.0f, w)); }
void PhantomEngine::setStereoWidth(float w)    { stereoWidth = juce::jlimit(0.0f, 2.0f, w); }

void PhantomEngine::setSynthLPF(float hz)
{
    hz = juce::jlimit(200.0f, 20000.0f, hz);
    if (hz == lastLPFHz) return;
    lastLPFHz = hz;
    const auto coeff = juce::IIRCoefficients::makeLowPass(sampleRate, (double) hz);
    lpfL.setCoefficients(coeff);
    lpfR.setCoefficients(coeff);
}

void PhantomEngine::setSynthHPF(float hz)
{
    hz = juce::jlimit(20.0f, 2000.0f, hz);
    if (hz == lastHPFHz) return;
    lastHPFHz = hz;
    const auto coeff = juce::IIRCoefficients::makeHighPass(sampleRate, (double) hz);
    hpfL.setCoefficients(coeff);
    hpfR.setCoefficients(coeff);
}

// ── Processing ─────────────────────────────────────────────────────────────

void PhantomEngine::process(juce::AudioBuffer<float>& buffer)
{
    const int n   = buffer.getNumSamples();
    const int nCh = juce::jmin(buffer.getNumChannels(), numChannels);
    if (n == 0 || nCh == 0) return;

    if (lowBuf.getNumSamples() < n || lowBuf.getNumChannels() < nCh)
    {
        lowBuf .setSize(nCh, juce::jmax(n, maxBlockSize), false, true, true);
        highBuf.setSize(nCh, juce::jmax(n, maxBlockSize), false, true, true);
    }

    // 1. Split into bass (low) and pass-through (high) bands
    for (int ch = 0; ch < nCh; ++ch)
    {
        bassExtractor.process(ch,
            buffer.getReadPointer(ch),
            lowBuf.getWritePointer(ch),
            highBuf.getWritePointer(ch),
            n);
    }

    // 2. Per-sample: detect period → synthesise harmonics → ghost mix
    //
    // The ZeroCrossingSynth receives the amplitude-normalised bass signal so
    // its zero crossings are consistent regardless of input level, and returns
    // only harmonics H2-H8 with no fundamental component.
    //
    // Unlike Chebyshev, no HPF is needed in Replace mode — there is no
    // fundamental leakage to filter out.
    int oscWp = oscSynthWrPos.load(std::memory_order_relaxed);
    for (int ch = 0; ch < nCh; ++ch)
    {
        auto& env  = (ch == 0) ? envelopeL  : envelopeR;
        auto& syn  = (ch == 0) ? synthL     : synthR;
        auto& sat  = (ch == 0) ? smoothSatL : smoothSatR;
        auto& lpf  = (ch == 0) ? lpfL       : lpfR;
        auto& hpf  = (ch == 0) ? hpfL       : hpfR;

        const float* low  = lowBuf .getReadPointer(ch);
        const float* high = highBuf.getReadPointer(ch);
        float*       out  = buffer.getWritePointer(ch);

        for (int i = 0; i < n; ++i)
        {
            const float inLvl = env.process(low[i]);

            // Normalise to ~unit amplitude so zero crossings are consistent
            // and the synthesised harmonics have a stable reference amplitude.
            // Gate below -80 dBFS to avoid noise-driven period estimates.
            const float normIn = (inLvl > 1e-4f)
                ? juce::jlimit(-1.0f, 1.0f, low[i] / inLvl)
                : 0.0f;

            // Generate H2-H8 — fundamental is never synthesised
            float phantomSample = syn.process(normIn);

            // Optional post-synthesis tanh saturation.
            // Applied to the harmonic sum so the fundamental cannot reappear.
            const float saturation = sat.getNextValue();
            if (saturation > 0.0f)
            {
                const float satCurve = std::tanh(phantomSample * 3.0f);
                phantomSample = phantomSample * (1.0f - saturation * 0.5f)
                              + satCurve * saturation;
            }

            // Scale by envelope so phantom tracks input dynamics
            const float phantomOut = phantomSample * inLvl * phantomStrength;

            // Synth filter: LPF then HPF, applied to harmonics only (before envelope scale)
            phantomSample = lpf.processSingleSampleRaw(phantomSample);
            phantomSample = hpf.processSingleSampleRaw(phantomSample);

            // Oscilloscope capture (left channel only)
            if (ch == 0)
            {
                oscSynthBuf[(size_t) oscWp] = phantomOut;
                oscWp = (oscWp + 1) & (kOscBufSize - 1);
            }

            // Ghost mix
            float mixedLow;
            if (ghostReplace)
                mixedLow = low[i] * (1.0f - ghostAmount) + phantomOut * ghostAmount;
            else
                mixedLow = low[i] + phantomOut * ghostAmount;

            out[i] = (mixedLow + high[i]) * outputGainLin;
        }
    }

    oscSynthWrPos.store(oscWp, std::memory_order_relaxed);

    // 3. Optional binaural processing
    binaural.process(buffer);

    // 4. Stereo width (M/S scaling)
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
