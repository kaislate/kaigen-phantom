#include "PhantomEngine.h"

void PhantomEngine::prepare(double sr, int blockSize, int nCh)
{
    sampleRate   = sr;
    maxBlockSize = blockSize;
    numChannels  = juce::jmax(1, nCh);

    bassExtractor.prepare(sr, blockSize, numChannels);

    synthL.prepare(sr);
    synthR.prepare(sr);
    resynL.prepare(sr);
    resynR.prepare(sr);

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

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sr;
    spec.maximumBlockSize = (juce::uint32) blockSize;
    spec.numChannels      = (juce::uint32) numChannels;
    lpf1pole.prepare(spec);
    hpf1pole.prepare(spec);
    lpf1pole.setType(juce::dsp::FirstOrderTPTFilterType::lowpass);
    hpf1pole.setType(juce::dsp::FirstOrderTPTFilterType::highpass);

    lastLPFHz = 20000.0f;
    lastHPFHz = 20.0f;
    recomputeLPFCoefs();
    recomputeHPFCoefs();
    lpfL_sa.reset(); lpfR_sa.reset(); lpfL_sb.reset(); lpfR_sb.reset();
    hpfL_sa.reset(); hpfR_sa.reset(); hpfL_sb.reset(); hpfR_sb.reset();
}

void PhantomEngine::reset()
{
    bassExtractor.reset();
    synthL.reset();
    synthR.reset();
    resynL.reset();
    resynR.reset();
    envelopeL.reset();
    envelopeR.reset();
    lowBuf.clear();
    highBuf.clear();
    lpf1pole.reset(); hpf1pole.reset();
    lpfL_sa.reset(); lpfR_sa.reset(); lpfL_sb.reset(); lpfR_sb.reset();
    hpfL_sa.reset(); hpfR_sa.reset(); hpfL_sb.reset(); hpfR_sb.reset();
}

// ── Parameter setters ──────────────────────────────────────────────────────

void PhantomEngine::setCrossoverHz(float hz)
{
    crossoverHz = juce::jlimit(20.0f, 20000.0f, hz);
    bassExtractor.setCrossoverHz(crossoverHz);
}

void PhantomEngine::setHarmonicAmplitudes(const std::array<float, 7>& amps)
{
    synthL.setHarmonicAmplitudes(amps);
    synthR.setHarmonicAmplitudes(amps);
    resynL.setHarmonicAmplitudes(amps);
    resynR.setHarmonicAmplitudes(amps);
}

void PhantomEngine::setSaturation(float s)
{
    smoothSatL.setTargetValue(juce::jlimit(0.0f, 1.0f, s));
    smoothSatR.setTargetValue(juce::jlimit(0.0f, 1.0f, s));
}

void PhantomEngine::setSynthStep(float step)
{
    synthL.setStep(step); synthR.setStep(step);
    resynL.setStep(step); resynR.setStep(step);
}
void PhantomEngine::setSynthDuty(float duty)
{
    synthL.setDutyCycle(duty); synthR.setDutyCycle(duty);
    resynL.setDutyCycle(duty); resynR.setDutyCycle(duty);
}
void PhantomEngine::setSynthSkip(int n)
{
    synthL.setSkipCount(n); synthR.setSkipCount(n);
    resynL.setSkipCount(n); resynR.setSkipCount(n);
}
void PhantomEngine::setGhostAmount(float a)       { ghostAmount    = juce::jlimit(0.0f, 1.0f, a); }
void PhantomEngine::setGhostMode(int m)           { ghostMode      = juce::jlimit(0, 2, m); }
void PhantomEngine::setPhantomStrength(float s)   { phantomStrength = juce::jlimit(0.0f, 1.0f, s); }
void PhantomEngine::setOutputGainDb(float db)     { outputGainLin  = std::pow(10.0f, db * 0.05f); }
void PhantomEngine::setInputDetectionGain(float lin) { inputDetectionGain = juce::jmax(1e-4f, lin); }
void PhantomEngine::setMidiTriggerEnabled(bool on)   { midiTriggerEnabled = on; }
void PhantomEngine::setMidiGateRelease(bool on)      { midiGateRelease    = on; }

void PhantomEngine::handleMidiNoteOn() noexcept
{
    if (!midiTriggerEnabled) return;
    envelopeL.retrigger();
    envelopeR.retrigger();
}

void PhantomEngine::handleMidiNoteOff() noexcept
{
    if (!midiTriggerEnabled || !midiGateRelease) return;
    envelopeL.forceRelease();
    envelopeR.forceRelease();
}
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
    recomputeLPFCoefs();
}

void PhantomEngine::setSynthHPF(float hz)
{
    hz = juce::jlimit(20.0f, 2000.0f, hz);
    if (hz == lastHPFHz) return;
    lastHPFHz = hz;
    recomputeHPFCoefs();
}

void PhantomEngine::setSynthFilterSlope(int dBPerOct)
{
    const int s = (dBPerOct <= 6)  ? 6
                : (dBPerOct >= 24) ? 24
                                   : 12;
    if (s == filterSlope) return;
    filterSlope = s;
    // Stage-A Q differs between -12 (Butterworth, 0.7071) and -24 (cascade Q1=0.541).
    // Recompute both filter sections so each stage's coefficients match the new slope.
    recomputeLPFCoefs();
    recomputeHPFCoefs();
}

void PhantomEngine::recomputeLPFCoefs()
{
    lpf1pole.setCutoffFrequency((double) lastLPFHz);

    // -12 dB/oct: stage A alone, Butterworth Q = 1/sqrt(2).
    // -24 dB/oct: stage A with Q₁ = 0.541, stage B with Q₂ = 1.307 → 4th-order Butterworth.
    const double qA = (filterSlope == 24) ? 0.541196100 : 0.707106781;
    const double qB =                       1.306562965;
    const auto coefA = juce::IIRCoefficients::makeLowPass(sampleRate, (double) lastLPFHz, qA);
    const auto coefB = juce::IIRCoefficients::makeLowPass(sampleRate, (double) lastLPFHz, qB);
    lpfL_sa.setCoefficients(coefA); lpfR_sa.setCoefficients(coefA);
    lpfL_sb.setCoefficients(coefB); lpfR_sb.setCoefficients(coefB);
}

void PhantomEngine::recomputeHPFCoefs()
{
    hpf1pole.setCutoffFrequency((double) lastHPFHz);

    const double qA = (filterSlope == 24) ? 0.541196100 : 0.707106781;
    const double qB =                       1.306562965;
    const auto coefA = juce::IIRCoefficients::makeHighPass(sampleRate, (double) lastHPFHz, qA);
    const auto coefB = juce::IIRCoefficients::makeHighPass(sampleRate, (double) lastHPFHz, qB);
    hpfL_sa.setCoefficients(coefA); hpfR_sa.setCoefficients(coefA);
    hpfL_sb.setCoefficients(coefB); hpfR_sb.setCoefficients(coefB);
}

void PhantomEngine::setWaveletLength(float len)
{
    resynL.setWaveletLength(len);
    resynR.setWaveletLength(len);
    // synthL/synthR not forwarded — Length is RESYN-only
}

void PhantomEngine::setGateThreshold(float thr)
{
    resynL.setGateThreshold(thr);
    resynR.setGateThreshold(thr);
    // synthL/synthR not forwarded — Gate is RESYN-only
}

void PhantomEngine::setSynthMode(int mode)
{
    synthMode.store(juce::jlimit(0, 1, mode), std::memory_order_relaxed);
}

void PhantomEngine::setEnvSource(int s)   { envSource = juce::jlimit(0, 1, s); }
void PhantomEngine::setTrackingSpeed(float speed)
{
    synthL.setTrackingSpeed(speed);
    synthR.setTrackingSpeed(speed);
    resynL.setTrackingSpeed(speed);
    resynR.setTrackingSpeed(speed);
}

void PhantomEngine::setMinPeriodSamples(float samples)
{
    synthL.setMinPeriodSamples(samples);
    synthR.setMinPeriodSamples(samples);
    resynL.setMinPeriodSamples(samples);
    resynR.setMinPeriodSamples(samples);
}

void PhantomEngine::setMaxPeriodSamples(float samples)
{
    synthL.setMaxPeriodSamples(samples);
    synthR.setMaxPeriodSamples(samples);
    resynL.setMaxPeriodSamples(samples);
    resynR.setMaxPeriodSamples(samples);
}

void PhantomEngine::setH1Amplitude(float amp)
{
    resynL.setH1Amplitude(amp);
    resynR.setH1Amplitude(amp);
}

void PhantomEngine::setSubAmplitude(float amp)
{
    resynL.setSubAmplitude(amp);
    resynR.setSubAmplitude(amp);
}

void PhantomEngine::setUsePunch(bool on)          { usePunch    = on; }
void PhantomEngine::setPunchAmount(float amount)  { punchAmount = juce::jlimit(0.0f, 1.0f, amount); }

void PhantomEngine::setBoostThreshold(float thr)
{
    resynL.setBoostThreshold(thr);
    resynR.setBoostThreshold(thr);
}

void PhantomEngine::setBoostAmount(float amt)
{
    resynL.setBoostAmount(amt);
    resynR.setBoostAmount(amt);
}

float PhantomEngine::getEstimatedHz() const noexcept
{
    return synthMode.load(std::memory_order_relaxed) == 1
        ? resynL.getEstimatedHz()
        : synthL.getEstimatedHz();
}

float PhantomEngine::getSynthInputPeak() const noexcept
{
    // Gate only exists in RESYN mode; return the RESYN synth's bass-band peak.
    return resynL.getInputPeak();
}

// ── Processing ─────────────────────────────────────────────────────────────

void PhantomEngine::process(juce::AudioBuffer<float>& buffer, const juce::AudioBuffer<float>* sidechain)
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
    // Effect mode (synthMode==0): ZeroCrossingSynth — synthesises H2-H8 only,
    // continuous phase, no fundamental component.
    //
    // RESYN mode (synthMode==1): WaveletSynth — phase resets at every zero
    // crossing, synthesises H1 (always 1.0) + H2-H8, grittier wavelet character.
    //
    // Both engines receive the amplitude-normalised bass band. No HPF is needed
    // in Replace mode — neither engine outputs the fundamental by design.
    const int  mode  = synthMode.load(std::memory_order_relaxed);
    const bool punch = usePunch;
    const float punchAmt = punchAmount;
    const float detGain    = inputDetectionGain;
    const float detGainInv = 1.0f / detGain;

    // Ghost-mode mix coefficients. Hoisting these out of the sample loop makes
    // the per-sample math branchless.
    //   Replace (0):      out = [low*(1-ghost) + phantom*ghost] + high
    //   Combine (1):      out = [low + phantom*ghost]           + high
    //   Phantom Only (2): out =  phantom*ghost                   (dry + high muted)
    const float dryLowCoef = (ghostMode == 0) ? (1.0f - ghostAmount)
                           : (ghostMode == 1) ? 1.0f
                                              : 0.0f;
    const float highCoef   = (ghostMode == 2) ? 0.0f : 1.0f;

    int oscWp = oscSynthWrPos.load(std::memory_order_relaxed);
    for (int ch = 0; ch < nCh; ++ch)
    {
        auto& env   = (ch == 0) ? envelopeL  : envelopeR;
        auto& syn   = (ch == 0) ? synthL     : synthR;
        auto& resyn = (ch == 0) ? resynL     : resynR;
        auto& sat   = (ch == 0) ? smoothSatL : smoothSatR;
        auto& lpfSa = (ch == 0) ? lpfL_sa : lpfR_sa;
        auto& lpfSb = (ch == 0) ? lpfL_sb : lpfR_sb;
        auto& hpfSa = (ch == 0) ? hpfL_sa : hpfR_sa;
        auto& hpfSb = (ch == 0) ? hpfL_sb : hpfR_sb;

        const float* low  = lowBuf .getReadPointer(ch);
        const float* high = highBuf.getReadPointer(ch);
        float*       out  = buffer.getWritePointer(ch);

        for (int i = 0; i < n; ++i)
        {
            // Envelope drives phantom output amplitude — it sees the natural signal so
            // Input Gain does not raise phantom loudness.
            const float envIn = (envSource == 1 && sidechain != nullptr && sidechain->getNumChannels() > 0)
                ? sidechain->getReadPointer(juce::jmin(ch, sidechain->getNumChannels() - 1))[i]
                : (low[i] + high[i]);
            const float inLvl = env.process(envIn);

            // While the envelope is in a MIDI-gated release, keep the synth in
            // free-run so it rings out past the normal amplitude floor.
            const bool freeRunNow = env.isForceReleasing();
            syn  .setFreeRun(freeRunNow);
            resyn.setFreeRun(freeRunNow);

            // Synth receives detection-scaled input (period, gate RMS, upward-expander
            // benefit from a hotter signal). Output is amplitude-normalised internally.
            const float synIn = low[i] * detGain;
            float phantomSample = (mode == 1)
                ? resyn.process(synIn)
                : syn.process(synIn);

            // Optional post-synthesis tanh saturation.
            // Applied to the harmonic sum so the fundamental cannot reappear.
            const float saturation = sat.getNextValue();
            if (saturation > 0.0f)
            {
                const float satCurve = std::tanh(phantomSample * 3.0f);
                phantomSample = phantomSample * (1.0f - saturation * 0.5f)
                              + satCurve * saturation;
            }

            // Synth filter: LPF then HPF on harmonics before envelope scaling.
            // Topology depends on filterSlope (6/12/24 dB/oct).
            if (filterSlope == 6)
            {
                phantomSample = lpf1pole.processSample(ch, phantomSample);
                phantomSample = hpf1pole.processSample(ch, phantomSample);
            }
            else if (filterSlope == 12)
            {
                phantomSample = lpfSa.processSingleSampleRaw(phantomSample);
                phantomSample = hpfSa.processSingleSampleRaw(phantomSample);
            }
            else // 24
            {
                phantomSample = lpfSa.processSingleSampleRaw(phantomSample);
                phantomSample = lpfSb.processSingleSampleRaw(phantomSample);
                phantomSample = hpfSa.processSingleSampleRaw(phantomSample);
                phantomSample = hpfSb.processSingleSampleRaw(phantomSample);
            }

            // Punch: blend envelope with per-wavelet peak. getWaveletPeak() is on the
            // detection-scaled signal, so de-scale before blending with natural inLvl.
            float level = inLvl;
            if (punch)
            {
                const float wvPeakRaw = (mode == 1) ? resyn.getWaveletPeak() : syn.getWaveletPeak();
                const float wvPeak    = wvPeakRaw * detGainInv;
                level = inLvl + (wvPeak - inLvl) * punchAmt;
                level = juce::jlimit(0.0f, 1.0f, level);
            }

            // Scale by envelope so phantom tracks input dynamics
            const float phantomOut = phantomSample * level * phantomStrength;

            // Oscilloscope capture (left channel only)
            if (ch == 0)
            {
                oscSynthBuf[(size_t) oscWp] = phantomOut;
                oscWp = (oscWp + 1) & (kOscBufSize - 1);
            }

            // Ghost mix (coefficients hoisted above).
            const float mixedLow = low[i] * dryLowCoef + phantomOut * ghostAmount;
            out[i] = (mixedLow + high[i] * highCoef) * outputGainLin;
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
