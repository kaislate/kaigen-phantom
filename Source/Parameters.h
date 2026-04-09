#pragma once
#include <JuceHeader.h>

namespace ParamID
{
    // ── Mode & Global ─────────────────────────────────────────────────
    inline constexpr auto MODE               = "mode";
    inline constexpr auto GHOST              = "ghost";
    inline constexpr auto GHOST_MODE         = "ghost_mode";
    inline constexpr auto PHANTOM_THRESHOLD  = "phantom_threshold";
    inline constexpr auto PHANTOM_STRENGTH   = "phantom_strength";
    inline constexpr auto OUTPUT_GAIN        = "output_gain";

    // ── Recipe Engine ─────────────────────────────────────────────────
    inline constexpr auto RECIPE_H2          = "recipe_h2";
    inline constexpr auto RECIPE_H3          = "recipe_h3";
    inline constexpr auto RECIPE_H4          = "recipe_h4";
    inline constexpr auto RECIPE_H5          = "recipe_h5";
    inline constexpr auto RECIPE_H6          = "recipe_h6";
    inline constexpr auto RECIPE_H7          = "recipe_h7";
    inline constexpr auto RECIPE_H8          = "recipe_h8";
    inline constexpr auto RECIPE_PHASE_H2    = "recipe_phase_h2";
    inline constexpr auto RECIPE_PHASE_H3    = "recipe_phase_h3";
    inline constexpr auto RECIPE_PHASE_H4    = "recipe_phase_h4";
    inline constexpr auto RECIPE_PHASE_H5    = "recipe_phase_h5";
    inline constexpr auto RECIPE_PHASE_H6    = "recipe_phase_h6";
    inline constexpr auto RECIPE_PHASE_H7    = "recipe_phase_h7";
    inline constexpr auto RECIPE_PHASE_H8    = "recipe_phase_h8";
    inline constexpr auto RECIPE_PRESET      = "recipe_preset";
    inline constexpr auto RECIPE_ROTATION    = "recipe_rotation";
    inline constexpr auto HARMONIC_SATURATION = "harmonic_saturation";

    // ── Binaural ──────────────────────────────────────────────────────
    inline constexpr auto BINAURAL_MODE      = "binaural_mode";
    inline constexpr auto BINAURAL_WIDTH     = "binaural_width";

    // ── Pitch Tracker ─────────────────────────────────────────────────
    inline constexpr auto TRACKING_SENSITIVITY = "tracking_sensitivity";
    inline constexpr auto TRACKING_GLIDE       = "tracking_glide";

    // ── Deconfliction ─────────────────────────────────────────────────
    inline constexpr auto DECONFLICTION_MODE = "deconfliction_mode";
    inline constexpr auto MAX_VOICES         = "max_voices";
    inline constexpr auto STAGGER_DELAY      = "stagger_delay";

    // ── Sidechain & Stereo ────────────────────────────────────────────
    inline constexpr auto SIDECHAIN_DUCK_AMOUNT  = "sidechain_duck_amount";
    inline constexpr auto SIDECHAIN_DUCK_ATTACK  = "sidechain_duck_attack";
    inline constexpr auto SIDECHAIN_DUCK_RELEASE = "sidechain_duck_release";
    inline constexpr auto STEREO_WIDTH           = "stereo_width";
}

// Warm preset harmonic amplitudes for H2..H8
inline constexpr float kWarmAmps[7]       = { 0.80f, 0.70f, 0.50f, 0.35f, 0.20f, 0.12f, 0.07f };
inline constexpr float kAggressiveAmps[7] = { 0.40f, 0.50f, 0.90f, 1.00f, 0.80f, 0.50f, 0.30f };
inline constexpr float kHollowAmps[7]     = { 0.10f, 0.80f, 0.10f, 0.70f, 0.10f, 0.60f, 0.10f };
inline constexpr float kDenseAmps[7]      = { 0.85f, 0.85f, 0.85f, 0.85f, 0.85f, 0.85f, 0.85f };

// ── ID registry — every parameter ID in declaration order ─────────────
// Used by tests and serialization to enumerate all IDs without requiring
// iteration over the opaque ParameterLayout type.
inline std::vector<juce::String> getAllParameterIDs()
{
    return {
        ParamID::MODE,
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
        ParamID::RECIPE_PHASE_H2,
        ParamID::RECIPE_PHASE_H3,
        ParamID::RECIPE_PHASE_H4,
        ParamID::RECIPE_PHASE_H5,
        ParamID::RECIPE_PHASE_H6,
        ParamID::RECIPE_PHASE_H7,
        ParamID::RECIPE_PHASE_H8,
        ParamID::RECIPE_PRESET,
        ParamID::RECIPE_ROTATION,
        ParamID::HARMONIC_SATURATION,
        ParamID::BINAURAL_MODE,
        ParamID::BINAURAL_WIDTH,
        ParamID::TRACKING_SENSITIVITY,
        ParamID::TRACKING_GLIDE,
        ParamID::DECONFLICTION_MODE,
        ParamID::MAX_VOICES,
        ParamID::STAGGER_DELAY,
        ParamID::SIDECHAIN_DUCK_AMOUNT,
        ParamID::SIDECHAIN_DUCK_ATTACK,
        ParamID::SIDECHAIN_DUCK_RELEASE,
        ParamID::STEREO_WIDTH,
    };
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    using APF  = AudioParameterFloat;
    using APC  = AudioParameterChoice;
    using APFI = AudioParameterInt;

    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    // ── Mode & Global ─────────────────────────────────────────────────
    params.push_back(std::make_unique<APC>(
        ParamID::MODE, "Mode", StringArray{ "Effect", "Instrument" }, 0));
    params.push_back(std::make_unique<APF>(
        ParamID::GHOST, "Ghost",
        NormalisableRange<float>(0.0f, 100.0f), 100.0f,
        AudioParameterFloatAttributes().withLabel("%")));
    params.push_back(std::make_unique<APC>(
        ParamID::GHOST_MODE, "Ghost Mode",
        // WARNING: order is serialized — do not insert or reorder.
        StringArray{ "Replace", "Add" }, 0));
    params.push_back(std::make_unique<APF>(
        ParamID::PHANTOM_THRESHOLD, "Phantom Threshold",
        NormalisableRange<float>(20.0f, 150.0f), 80.0f,
        AudioParameterFloatAttributes().withLabel("Hz")));
    params.push_back(std::make_unique<APF>(
        ParamID::PHANTOM_STRENGTH, "Phantom Strength",
        NormalisableRange<float>(0.0f, 100.0f), 80.0f,
        AudioParameterFloatAttributes().withLabel("%")));
    params.push_back(std::make_unique<APF>(
        ParamID::OUTPUT_GAIN, "Output Gain",
        NormalisableRange<float>(-24.0f, 12.0f), 0.0f,
        AudioParameterFloatAttributes().withLabel("dB")));

    // ── Recipe Engine — amplitudes ────────────────────────────────────
    const char* ampIDs[7] = {
        ParamID::RECIPE_H2, ParamID::RECIPE_H3, ParamID::RECIPE_H4,
        ParamID::RECIPE_H5, ParamID::RECIPE_H6, ParamID::RECIPE_H7, ParamID::RECIPE_H8
    };
    const char* ampNames[7] = { "H2 Amp","H3 Amp","H4 Amp","H5 Amp","H6 Amp","H7 Amp","H8 Amp" };
    for (int i = 0; i < 7; ++i)
        params.push_back(std::make_unique<APF>(
            ampIDs[i], ampNames[i],
            NormalisableRange<float>(0.0f, 100.0f), kWarmAmps[i] * 100.0f,
            AudioParameterFloatAttributes().withLabel("%")));

    // ── Recipe Engine — phases ────────────────────────────────────────
    const char* phaseIDs[7] = {
        ParamID::RECIPE_PHASE_H2, ParamID::RECIPE_PHASE_H3, ParamID::RECIPE_PHASE_H4,
        ParamID::RECIPE_PHASE_H5, ParamID::RECIPE_PHASE_H6, ParamID::RECIPE_PHASE_H7,
        ParamID::RECIPE_PHASE_H8
    };
    const char* phaseNames[7] = { "H2 Phase","H3 Phase","H4 Phase","H5 Phase","H6 Phase","H7 Phase","H8 Phase" };
    for (int i = 0; i < 7; ++i)
        params.push_back(std::make_unique<APF>(
            phaseIDs[i], phaseNames[i],
            NormalisableRange<float>(0.0f, 360.0f), 0.0f,
            AudioParameterFloatAttributes().withLabel("deg")));

    params.push_back(std::make_unique<APC>(
        ParamID::RECIPE_PRESET, "Recipe Preset",
        // WARNING: order is serialized — do not insert or reorder.
        StringArray{ "Warm", "Aggressive", "Hollow", "Dense", "Custom" }, 0));
    params.push_back(std::make_unique<APF>(
        ParamID::RECIPE_ROTATION, "Recipe Rotation",
        NormalisableRange<float>(-180.0f, 180.0f), 0.0f,
        AudioParameterFloatAttributes().withLabel("deg")));
    params.push_back(std::make_unique<APF>(
        ParamID::HARMONIC_SATURATION, "Harmonic Saturation",
        NormalisableRange<float>(0.0f, 100.0f), 0.0f,
        AudioParameterFloatAttributes().withLabel("%")));

    // ── Binaural ──────────────────────────────────────────────────────
    params.push_back(std::make_unique<APC>(
        ParamID::BINAURAL_MODE, "Binaural Mode",
        StringArray{ "Off", "Spread", "Voice-Split" }, 0));
    params.push_back(std::make_unique<APF>(
        ParamID::BINAURAL_WIDTH, "Binaural Width",
        NormalisableRange<float>(0.0f, 100.0f), 50.0f,
        AudioParameterFloatAttributes().withLabel("%")));

    // ── Pitch Tracker ─────────────────────────────────────────────────
    params.push_back(std::make_unique<APF>(
        ParamID::TRACKING_SENSITIVITY, "Tracking Sensitivity",
        NormalisableRange<float>(0.0f, 100.0f), 70.0f,
        AudioParameterFloatAttributes().withLabel("%")));
    params.push_back(std::make_unique<APF>(
        ParamID::TRACKING_GLIDE, "Tracking Glide",
        NormalisableRange<float>(0.0f, 200.0f), 20.0f,
        AudioParameterFloatAttributes().withLabel("ms")));

    // ── Deconfliction ─────────────────────────────────────────────────
    params.push_back(std::make_unique<APC>(
        ParamID::DECONFLICTION_MODE, "Deconfliction Mode",
        // WARNING: order is serialized — do not insert or reorder.
        StringArray{ "Partition","Lane","Stagger","Odd-Even","Residue","Binaural" }, 0));
    params.push_back(std::make_unique<APFI>(
        ParamID::MAX_VOICES, "Max Voices", 1, 8, 4));
    params.push_back(std::make_unique<APF>(
        ParamID::STAGGER_DELAY, "Stagger Delay",
        NormalisableRange<float>(0.0f, 30.0f), 8.0f,
        AudioParameterFloatAttributes().withLabel("ms")));

    // ── Sidechain & Stereo ────────────────────────────────────────────
    params.push_back(std::make_unique<APF>(
        ParamID::SIDECHAIN_DUCK_AMOUNT, "Duck Amount",
        NormalisableRange<float>(0.0f, 100.0f), 0.0f,
        AudioParameterFloatAttributes().withLabel("%")));
    params.push_back(std::make_unique<APF>(
        ParamID::SIDECHAIN_DUCK_ATTACK, "Duck Attack",
        NormalisableRange<float>(1.0f, 100.0f), 5.0f,
        AudioParameterFloatAttributes().withLabel("ms")));
    params.push_back(std::make_unique<APF>(
        ParamID::SIDECHAIN_DUCK_RELEASE, "Duck Release",
        NormalisableRange<float>(10.0f, 500.0f), 80.0f,
        AudioParameterFloatAttributes().withLabel("ms")));
    params.push_back(std::make_unique<APF>(
        ParamID::STEREO_WIDTH, "Stereo Width",
        NormalisableRange<float>(0.0f, 200.0f), 100.0f,
        AudioParameterFloatAttributes().withLabel("%")));

    return { params.begin(), params.end() };
}
