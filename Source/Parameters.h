#pragma once
#include <JuceHeader.h>

namespace ParamID
{
    // ── Mode & Global ─────────────────────────────────────────────────
    inline constexpr auto MODE               = "mode";
    inline constexpr auto BYPASS             = "bypass";
    inline constexpr auto GHOST              = "ghost";
    inline constexpr auto GHOST_MODE         = "ghost_mode";
    inline constexpr auto PHANTOM_THRESHOLD  = "phantom_threshold";
    inline constexpr auto PHANTOM_STRENGTH   = "phantom_strength";
    inline constexpr auto OUTPUT_GAIN        = "output_gain";

    // ── Recipe Engine (ZeroCrossingSynth harmonic amplitudes H2..H8) ────
    inline constexpr auto RECIPE_H2          = "recipe_h2";
    inline constexpr auto RECIPE_H3          = "recipe_h3";
    inline constexpr auto RECIPE_H4          = "recipe_h4";
    inline constexpr auto RECIPE_H5          = "recipe_h5";
    inline constexpr auto RECIPE_H6          = "recipe_h6";
    inline constexpr auto RECIPE_H7          = "recipe_h7";
    inline constexpr auto RECIPE_H8          = "recipe_h8";
    inline constexpr auto RECIPE_PRESET      = "recipe_preset";
    inline constexpr auto HARMONIC_SATURATION = "harmonic_saturation";

    // ── Waveform shape ────────────────────────────────────────────────────
    /** 0 = pure sine, 100 = square. Morphs the synthesised oscillator shape. */
    inline constexpr auto SYNTH_STEP         = "synth_step";
    /** Pulse width: 50 = symmetric. Controls even/odd harmonic balance. */
    inline constexpr auto SYNTH_DUTY         = "synth_duty";
    /** Zero-crossing skip count [1-8]. Each +1 halves the effective fundamental,
     *  shifting all harmonics down and introducing sub-harmonic content. */
    inline constexpr auto SYNTH_SKIP         = "synth_skip";

    // ── Envelope Follower ────────────────────────────────────────────
    inline constexpr auto ENV_ATTACK_MS      = "env_attack_ms";
    inline constexpr auto ENV_RELEASE_MS     = "env_release_ms";

    // ── Binaural ──────────────────────────────────────────────────────
    inline constexpr auto BINAURAL_MODE      = "binaural_mode";
    inline constexpr auto BINAURAL_WIDTH     = "binaural_width";

    // ── Stereo ────────────────────────────────────────────────────────
    inline constexpr auto STEREO_WIDTH       = "stereo_width";

    // ── Synth Filter ──────────────────────────────────────────────────
    /** Low-pass filter on synthesised harmonics. 200–20000 Hz. Default 20000 (transparent). */
    inline constexpr auto SYNTH_LPF_HZ      = "synth_lpf_hz";
    /** High-pass filter on synthesised harmonics. 20–2000 Hz. Default 20 (transparent). */
    inline constexpr auto SYNTH_HPF_HZ      = "synth_hpf_hz";

    // ── RESYN (WaveletSynth) controls ─────────────────────────────────────
    /** Fraction of each wavelet period to synthesise. 0.05–1.0. Default 1.0 (full). */
    inline constexpr auto SYNTH_WAVELET_LENGTH = "synth_wavelet_length";
    /** Gate threshold: min negative-peak amplitude for a crossing to be valid. 0–1. Default 0. */
    inline constexpr auto SYNTH_GATE_THRESHOLD = "synth_gate_threshold";
}

// ─── Preset amplitude tables — Chebyshev polynomial weights ────────────
// Each row gives H2..H8 coefficients. Values in [0, 1].
inline constexpr float kWarmAmps[7]       = { 0.80f, 0.60f, 0.40f, 0.28f, 0.18f, 0.10f, 0.05f };
inline constexpr float kAggressiveAmps[7] = { 0.50f, 0.70f, 0.85f, 0.75f, 0.55f, 0.35f, 0.20f };
inline constexpr float kHollowAmps[7]     = { 0.00f, 0.80f, 0.00f, 0.60f, 0.00f, 0.40f, 0.00f };
inline constexpr float kDenseAmps[7]      = { 0.70f, 0.70f, 0.70f, 0.70f, 0.70f, 0.70f, 0.70f };
inline constexpr float kStableAmps[7]     = { 1.00f, 0.00f, 0.70f, 0.00f, 0.50f, 0.00f, 0.30f };
inline constexpr float kWeirdAmps[7]      = { 0.00f, 1.00f, 0.00f, 0.80f, 0.00f, 0.60f, 0.00f };

// ─── ID registry ───────────────────────────────────────────────────────
inline std::vector<juce::String> getAllParameterIDs()
{
    return {
        ParamID::MODE,
        ParamID::BYPASS,
        ParamID::GHOST,
        ParamID::GHOST_MODE,
        ParamID::PHANTOM_THRESHOLD,
        ParamID::PHANTOM_STRENGTH,
        ParamID::OUTPUT_GAIN,
        ParamID::RECIPE_H2,
        ParamID::RECIPE_H3,
        ParamID::RECIPE_H4,
        ParamID::RECIPE_H5,
        ParamID::RECIPE_H6,
        ParamID::RECIPE_H7,
        ParamID::RECIPE_H8,
        ParamID::RECIPE_PRESET,
        ParamID::HARMONIC_SATURATION,
        ParamID::SYNTH_STEP,
        ParamID::SYNTH_DUTY,
        ParamID::SYNTH_SKIP,
        ParamID::ENV_ATTACK_MS,
        ParamID::ENV_RELEASE_MS,
        ParamID::BINAURAL_MODE,
        ParamID::BINAURAL_WIDTH,
        ParamID::STEREO_WIDTH,
        ParamID::SYNTH_LPF_HZ,
        ParamID::SYNTH_HPF_HZ,
        ParamID::SYNTH_WAVELET_LENGTH,
        ParamID::SYNTH_GATE_THRESHOLD,
    };
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    using APF  = AudioParameterFloat;
    using APC  = AudioParameterChoice;

    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    // ── Mode & Global ─────────────────────────────────────────────────
    params.push_back(std::make_unique<APC>(
        ParamID::MODE, "Mode", StringArray{ "Effect", "RESYN" }, 0));
    params.push_back(std::make_unique<AudioParameterBool>(
        ParamID::BYPASS, "Bypass", false));
    params.push_back(std::make_unique<APF>(
        ParamID::GHOST, "Ghost",
        NormalisableRange<float>(0.0f, 100.0f), 100.0f,
        AudioParameterFloatAttributes().withLabel("%")));
    params.push_back(std::make_unique<APC>(
        ParamID::GHOST_MODE, "Ghost Mode",
        StringArray{ "Replace", "Add" }, 0));
    params.push_back(std::make_unique<APF>(
        ParamID::PHANTOM_THRESHOLD, "Phantom Threshold",
        NormalisableRange<float>(20.0f, 250.0f), 120.0f,
        AudioParameterFloatAttributes().withLabel("Hz")));
    params.push_back(std::make_unique<APF>(
        ParamID::PHANTOM_STRENGTH, "Phantom Strength",
        NormalisableRange<float>(0.0f, 100.0f), 80.0f,
        AudioParameterFloatAttributes().withLabel("%")));
    params.push_back(std::make_unique<APF>(
        ParamID::OUTPUT_GAIN, "Output Gain",
        NormalisableRange<float>(-24.0f, 12.0f), 0.0f,
        AudioParameterFloatAttributes().withLabel("dB")));

    // ── Recipe Engine (Chebyshev H2..H8) ──────────────────────────────
    const char* ampIDs[7] = {
        ParamID::RECIPE_H2, ParamID::RECIPE_H3, ParamID::RECIPE_H4,
        ParamID::RECIPE_H5, ParamID::RECIPE_H6, ParamID::RECIPE_H7, ParamID::RECIPE_H8
    };
    const char* ampNames[7] = { "H2 Amp","H3 Amp","H4 Amp","H5 Amp","H6 Amp","H7 Amp","H8 Amp" };
    for (int i = 0; i < 7; ++i)
        params.push_back(std::make_unique<APF>(
            ampIDs[i], ampNames[i],
            NormalisableRange<float>(0.0f, 100.0f), kStableAmps[i] * 100.0f,
            AudioParameterFloatAttributes().withLabel("%")));

    params.push_back(std::make_unique<APC>(
        ParamID::RECIPE_PRESET, "Recipe Preset",
        StringArray{ "Warm", "Aggressive", "Hollow", "Dense", "Stable", "Weird", "Custom" }, 4));
    params.push_back(std::make_unique<APF>(
        ParamID::HARMONIC_SATURATION, "Harmonic Saturation",
        NormalisableRange<float>(0.0f, 100.0f), 0.0f,
        AudioParameterFloatAttributes().withLabel("%")));

    // ── Waveform shape ────────────────────────────────────────────────────
    params.push_back(std::make_unique<APF>(
        ParamID::SYNTH_STEP, "Step",
        NormalisableRange<float>(0.0f, 100.0f), 0.0f,
        AudioParameterFloatAttributes().withLabel("%")));
    params.push_back(std::make_unique<APF>(
        ParamID::SYNTH_DUTY, "Duty Cycle",
        NormalisableRange<float>(5.0f, 95.0f), 50.0f,
        AudioParameterFloatAttributes().withLabel("%")));
    params.push_back(std::make_unique<APF>(
        ParamID::SYNTH_SKIP, "Skip",
        NormalisableRange<float>(1.0f, 8.0f, 1.0f), 1.0f,
        AudioParameterFloatAttributes()));

    // ── Envelope Follower ────────────────────────────────────────────
    params.push_back(std::make_unique<APF>(
        ParamID::ENV_ATTACK_MS, "Envelope Attack",
        NormalisableRange<float>(0.1f, 2000.0f, 0.0f, 0.3f), 1.0f,
        AudioParameterFloatAttributes().withLabel("ms")));
    params.push_back(std::make_unique<APF>(
        ParamID::ENV_RELEASE_MS, "Envelope Release",
        NormalisableRange<float>(5.0f, 5000.0f, 0.0f, 0.3f), 50.0f,
        AudioParameterFloatAttributes().withLabel("ms")));

    // ── Synth Filter ──────────────────────────────────────────────────
    params.push_back(std::make_unique<APF>(
        ParamID::SYNTH_LPF_HZ, "Synth LPF",
        NormalisableRange<float>(200.0f, 20000.0f, 0.0f, 0.3f), 20000.0f,
        AudioParameterFloatAttributes().withLabel("Hz")));
    params.push_back(std::make_unique<APF>(
        ParamID::SYNTH_HPF_HZ, "Synth HPF",
        NormalisableRange<float>(20.0f, 2000.0f, 0.0f, 0.3f), 20.0f,
        AudioParameterFloatAttributes().withLabel("Hz")));

    // ── RESYN controls ────────────────────────────────────────────────
    params.push_back(std::make_unique<APF>(
        ParamID::SYNTH_WAVELET_LENGTH, "Wavelet Length",
        NormalisableRange<float>(0.05f, 1.0f), 1.0f));
    params.push_back(std::make_unique<APF>(
        ParamID::SYNTH_GATE_THRESHOLD, "Gate Threshold",
        NormalisableRange<float>(0.0f, 1.0f), 0.0f));

    // ── Binaural ──────────────────────────────────────────────────────
    params.push_back(std::make_unique<APC>(
        ParamID::BINAURAL_MODE, "Binaural Mode",
        StringArray{ "Off", "Spread", "Voice-Split" }, 0));
    params.push_back(std::make_unique<APF>(
        ParamID::BINAURAL_WIDTH, "Binaural Width",
        NormalisableRange<float>(0.0f, 100.0f), 50.0f,
        AudioParameterFloatAttributes().withLabel("%")));

    // ── Stereo ────────────────────────────────────────────────────────
    params.push_back(std::make_unique<APF>(
        ParamID::STEREO_WIDTH, "Stereo Width",
        NormalisableRange<float>(0.0f, 200.0f), 100.0f,
        AudioParameterFloatAttributes().withLabel("%")));

    return { params.begin(), params.end() };
}
