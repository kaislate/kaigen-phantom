#pragma once
#include <JuceHeader.h>

namespace ParamID
{
    #define KAIGEN_PER_ENGINE(IDENT, STR_LEAF)                            \
        inline constexpr auto A_##IDENT    = "a_" STR_LEAF;               \
        inline constexpr auto B_##IDENT    = "b_" STR_LEAF;               \
        inline constexpr auto LEAF_##IDENT = STR_LEAF;

    // ── Mode & Global ─────────────────────────────────────────────────
    KAIGEN_PER_ENGINE(MODE, "mode")
    inline constexpr auto BYPASS             = "bypass";       // global
    KAIGEN_PER_ENGINE(GHOST, "ghost")
    KAIGEN_PER_ENGINE(GHOST_MODE, "ghost_mode")
    KAIGEN_PER_ENGINE(PHANTOM_THRESHOLD, "phantom_threshold")
    KAIGEN_PER_ENGINE(PHANTOM_STRENGTH, "phantom_strength")
    inline constexpr auto INPUT_GAIN         = "input_gain";       // global
    inline constexpr auto INPUT_GAIN_AUTO    = "input_gain_auto";  // global
    KAIGEN_PER_ENGINE(OUTPUT_GAIN, "output_gain")

    // ── Recipe Engine ──────────────────────────────────────────────────
    KAIGEN_PER_ENGINE(RECIPE_H2, "recipe_h2")
    KAIGEN_PER_ENGINE(RECIPE_H3, "recipe_h3")
    KAIGEN_PER_ENGINE(RECIPE_H4, "recipe_h4")
    KAIGEN_PER_ENGINE(RECIPE_H5, "recipe_h5")
    KAIGEN_PER_ENGINE(RECIPE_H6, "recipe_h6")
    KAIGEN_PER_ENGINE(RECIPE_H7, "recipe_h7")
    KAIGEN_PER_ENGINE(RECIPE_H8, "recipe_h8")
    KAIGEN_PER_ENGINE(RECIPE_PRESET, "recipe_preset")
    KAIGEN_PER_ENGINE(HARMONIC_SATURATION, "harmonic_saturation")

    // ── Waveform shape ────────────────────────────────────────────────
    KAIGEN_PER_ENGINE(SYNTH_STEP, "synth_step")
    KAIGEN_PER_ENGINE(SYNTH_DUTY, "synth_duty")
    KAIGEN_PER_ENGINE(SYNTH_SKIP, "synth_skip")

    // ── Envelope Follower ─────────────────────────────────────────────
    KAIGEN_PER_ENGINE(ENV_ATTACK_MS, "env_attack_ms")
    KAIGEN_PER_ENGINE(ENV_RELEASE_MS, "env_release_ms")
    KAIGEN_PER_ENGINE(ENV_SOURCE, "env_source")

    // ── Binaural ──────────────────────────────────────────────────────
    KAIGEN_PER_ENGINE(BINAURAL_MODE, "binaural_mode")
    KAIGEN_PER_ENGINE(BINAURAL_WIDTH, "binaural_width")

    // ── Stereo ────────────────────────────────────────────────────────
    KAIGEN_PER_ENGINE(STEREO_WIDTH, "stereo_width")

    // ── Synth Filter ──────────────────────────────────────────────────
    KAIGEN_PER_ENGINE(SYNTH_FILTER_SLOPE, "synth_filter_slope")
    KAIGEN_PER_ENGINE(SYNTH_LPF_HZ, "synth_lpf_hz")
    KAIGEN_PER_ENGINE(SYNTH_HPF_HZ, "synth_hpf_hz")

    // ── RESYN (WaveletSynth) ──────────────────────────────────────────
    KAIGEN_PER_ENGINE(SYNTH_WAVELET_LENGTH, "synth_wavelet_length")
    KAIGEN_PER_ENGINE(SYNTH_GATE_THRESHOLD, "synth_gate_threshold")
    KAIGEN_PER_ENGINE(SYNTH_H1, "synth_h1")
    KAIGEN_PER_ENGINE(SYNTH_SUB, "synth_sub")
    KAIGEN_PER_ENGINE(SYNTH_TRIM, "synth_trim")

    // ── Crossing detection / pitch ────────────────────────────────────
    KAIGEN_PER_ENGINE(SYNTH_MIN_SAMPLES, "synth_min_samples")
    KAIGEN_PER_ENGINE(SYNTH_MAX_SAMPLES, "synth_max_samples")
    KAIGEN_PER_ENGINE(TRACKING_SPEED, "tracking_speed")
    KAIGEN_PER_ENGINE(PUNCH_ENABLED, "punch_enabled")
    KAIGEN_PER_ENGINE(PUNCH_AMOUNT, "punch_amount")
    KAIGEN_PER_ENGINE(SYNTH_BOOST_THRESHOLD, "synth_boost_threshold")
    KAIGEN_PER_ENGINE(SYNTH_BOOST_AMOUNT, "synth_boost_amount")

    // ── MIDI triggering ───────────────────────────────────────────────
    KAIGEN_PER_ENGINE(MIDI_TRIGGER_ENABLED, "midi_trigger_enabled")
    KAIGEN_PER_ENGINE(MIDI_GATE_RELEASE, "midi_gate_release")

    // ── Advanced UI toggle (global; UI-only) ───────────────────────────
    inline constexpr auto ADVANCED_OPEN = "advanced_open";

    // ── Morph crossfader (top-level, always-on) ────────────────────────
    inline constexpr auto MORPH_AMOUNT             = "morph_amount";
    inline constexpr auto MORPH_CURVE              = "morph_curve";
    inline constexpr auto MORPH_A_LEVEL_DB         = "morph_a_level_db";
    inline constexpr auto MORPH_B_LEVEL_DB         = "morph_b_level_db";
    inline constexpr auto MORPH_BYPASS_IDLE_ENGINE = "morph_bypass_idle_engine";

    // ── Macros (PR3a) — global, automatable ────────────────────────────
    inline constexpr auto MACRO1 = "macro1";
    inline constexpr auto MACRO2 = "macro2";
    inline constexpr auto MACRO3 = "macro3";
    inline constexpr auto MACRO4 = "macro4";

    #undef KAIGEN_PER_ENGINE
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
/** Flat enumeration of every APVTS param ID. Used for set-membership checks
 *  (preset migration, smoke tests). The order here is for human readability
 *  only — it does NOT match `createParameterLayout()`'s emission order
 *  (which groups A's per-engine block, then B's, then globals + morph).
 *  Downstream code must not rely on this order being meaningful. */
inline std::vector<juce::String> getAllParameterIDs()
{
    std::vector<juce::String> ids;
    auto addAB = [&](const char* a, const char* b) { ids.push_back(a); ids.push_back(b); };

    addAB(ParamID::A_MODE, ParamID::B_MODE);
    ids.push_back(ParamID::BYPASS);
    addAB(ParamID::A_GHOST, ParamID::B_GHOST);
    addAB(ParamID::A_GHOST_MODE, ParamID::B_GHOST_MODE);
    addAB(ParamID::A_PHANTOM_THRESHOLD, ParamID::B_PHANTOM_THRESHOLD);
    addAB(ParamID::A_PHANTOM_STRENGTH, ParamID::B_PHANTOM_STRENGTH);
    ids.push_back(ParamID::INPUT_GAIN);
    ids.push_back(ParamID::INPUT_GAIN_AUTO);
    addAB(ParamID::A_OUTPUT_GAIN, ParamID::B_OUTPUT_GAIN);

    addAB(ParamID::A_RECIPE_H2, ParamID::B_RECIPE_H2);
    addAB(ParamID::A_RECIPE_H3, ParamID::B_RECIPE_H3);
    addAB(ParamID::A_RECIPE_H4, ParamID::B_RECIPE_H4);
    addAB(ParamID::A_RECIPE_H5, ParamID::B_RECIPE_H5);
    addAB(ParamID::A_RECIPE_H6, ParamID::B_RECIPE_H6);
    addAB(ParamID::A_RECIPE_H7, ParamID::B_RECIPE_H7);
    addAB(ParamID::A_RECIPE_H8, ParamID::B_RECIPE_H8);
    addAB(ParamID::A_RECIPE_PRESET, ParamID::B_RECIPE_PRESET);
    addAB(ParamID::A_HARMONIC_SATURATION, ParamID::B_HARMONIC_SATURATION);

    addAB(ParamID::A_SYNTH_STEP, ParamID::B_SYNTH_STEP);
    addAB(ParamID::A_SYNTH_DUTY, ParamID::B_SYNTH_DUTY);
    addAB(ParamID::A_SYNTH_SKIP, ParamID::B_SYNTH_SKIP);

    addAB(ParamID::A_ENV_ATTACK_MS, ParamID::B_ENV_ATTACK_MS);
    addAB(ParamID::A_ENV_RELEASE_MS, ParamID::B_ENV_RELEASE_MS);
    addAB(ParamID::A_ENV_SOURCE, ParamID::B_ENV_SOURCE);
    addAB(ParamID::A_MIDI_TRIGGER_ENABLED, ParamID::B_MIDI_TRIGGER_ENABLED);
    addAB(ParamID::A_MIDI_GATE_RELEASE, ParamID::B_MIDI_GATE_RELEASE);

    addAB(ParamID::A_BINAURAL_MODE, ParamID::B_BINAURAL_MODE);
    addAB(ParamID::A_BINAURAL_WIDTH, ParamID::B_BINAURAL_WIDTH);
    addAB(ParamID::A_STEREO_WIDTH, ParamID::B_STEREO_WIDTH);

    addAB(ParamID::A_SYNTH_LPF_HZ, ParamID::B_SYNTH_LPF_HZ);
    addAB(ParamID::A_SYNTH_HPF_HZ, ParamID::B_SYNTH_HPF_HZ);
    addAB(ParamID::A_SYNTH_FILTER_SLOPE, ParamID::B_SYNTH_FILTER_SLOPE);
    addAB(ParamID::A_SYNTH_WAVELET_LENGTH, ParamID::B_SYNTH_WAVELET_LENGTH);
    addAB(ParamID::A_SYNTH_GATE_THRESHOLD, ParamID::B_SYNTH_GATE_THRESHOLD);
    addAB(ParamID::A_SYNTH_H1, ParamID::B_SYNTH_H1);
    addAB(ParamID::A_SYNTH_SUB, ParamID::B_SYNTH_SUB);
    addAB(ParamID::A_SYNTH_TRIM, ParamID::B_SYNTH_TRIM);
    addAB(ParamID::A_SYNTH_MIN_SAMPLES, ParamID::B_SYNTH_MIN_SAMPLES);
    addAB(ParamID::A_SYNTH_MAX_SAMPLES, ParamID::B_SYNTH_MAX_SAMPLES);
    addAB(ParamID::A_TRACKING_SPEED, ParamID::B_TRACKING_SPEED);
    addAB(ParamID::A_PUNCH_ENABLED, ParamID::B_PUNCH_ENABLED);
    addAB(ParamID::A_PUNCH_AMOUNT, ParamID::B_PUNCH_AMOUNT);
    addAB(ParamID::A_SYNTH_BOOST_THRESHOLD, ParamID::B_SYNTH_BOOST_THRESHOLD);
    addAB(ParamID::A_SYNTH_BOOST_AMOUNT, ParamID::B_SYNTH_BOOST_AMOUNT);

    ids.push_back(ParamID::ADVANCED_OPEN);
    ids.push_back(ParamID::MORPH_AMOUNT);
    ids.push_back(ParamID::MORPH_CURVE);
    ids.push_back(ParamID::MORPH_A_LEVEL_DB);
    ids.push_back(ParamID::MORPH_B_LEVEL_DB);
    ids.push_back(ParamID::MORPH_BYPASS_IDLE_ENGINE);

    ids.push_back(ParamID::MACRO1);
    ids.push_back(ParamID::MACRO2);
    ids.push_back(ParamID::MACRO3);
    ids.push_back(ParamID::MACRO4);

    return ids;
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    using APF  = AudioParameterFloat;
    using APC  = AudioParameterChoice;

    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    // Per-engine params: emitted twice (once for each of A and B) via this lambda.
    // `prefix` is the param-ID prefix ("a_" or "b_") and `displayPrefix` is the
    // user-visible display-name prefix ("A. " or "B. ").
    auto makeEngineParams = [&params](const char* prefix, const char* displayPrefix)
    {
        auto pid  = [prefix]       (const char* name)    { return juce::String(prefix)        + name; };
        auto disp = [displayPrefix](const char* display) { return juce::String(displayPrefix) + display; };

        // ── Mode & Global (per-engine portion) ────────────────────────
        params.push_back(std::make_unique<APC>(
            pid("mode"), disp("Mode"), juce::StringArray{ "Effect", "RESYN" }, 0));
        params.push_back(std::make_unique<APF>(
            pid("ghost"), disp("Ghost"),
            juce::NormalisableRange<float>(0.0f, 100.0f), 100.0f,
            juce::AudioParameterFloatAttributes().withLabel("%")));
        params.push_back(std::make_unique<APC>(
            pid("ghost_mode"), disp("Ghost Mode"),
            juce::StringArray{ "Replace", "Combine", "Phantom Only" }, 0));
        params.push_back(std::make_unique<APF>(
            pid("phantom_threshold"), disp("Phantom Threshold"),
            juce::NormalisableRange<float>(20.0f, 20000.0f, 0.0f, 0.25f), 120.0f,
            juce::AudioParameterFloatAttributes().withLabel("Hz")));
        params.push_back(std::make_unique<APF>(
            pid("phantom_strength"), disp("Phantom Strength"),
            juce::NormalisableRange<float>(0.0f, 100.0f), 80.0f,
            juce::AudioParameterFloatAttributes().withLabel("%")));
        params.push_back(std::make_unique<APF>(
            pid("output_gain"), disp("Output Gain"),
            juce::NormalisableRange<float>(-24.0f, 12.0f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel("dB")));

        // ── Recipe Engine (Chebyshev H2..H8) ──────────────────────────
        const char* ampLogical[7] = {
            "recipe_h2", "recipe_h3", "recipe_h4",
            "recipe_h5", "recipe_h6", "recipe_h7", "recipe_h8"
        };
        const char* ampNames[7] = { "H2 Amp","H3 Amp","H4 Amp","H5 Amp","H6 Amp","H7 Amp","H8 Amp" };
        for (int i = 0; i < 7; ++i)
            params.push_back(std::make_unique<APF>(
                pid(ampLogical[i]), disp(ampNames[i]),
                juce::NormalisableRange<float>(0.0f, 100.0f), kStableAmps[i] * 100.0f,
                juce::AudioParameterFloatAttributes().withLabel("%")));

        params.push_back(std::make_unique<APC>(
            pid("recipe_preset"), disp("Recipe Preset"),
            juce::StringArray{ "Warm", "Aggressive", "Hollow", "Dense", "Stable", "Weird",
                               "Custom 1", "Custom 2", "Custom 3" }, 4));
        params.push_back(std::make_unique<APF>(
            pid("harmonic_saturation"), disp("Harmonic Saturation"),
            juce::NormalisableRange<float>(0.0f, 100.0f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel("%")));

        // ── Waveform shape ────────────────────────────────────────────
        params.push_back(std::make_unique<APF>(
            pid("synth_step"), disp("Step"),
            juce::NormalisableRange<float>(0.0f, 100.0f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel("%")));
        params.push_back(std::make_unique<APF>(
            pid("synth_duty"), disp("Duty Cycle"),
            juce::NormalisableRange<float>(5.0f, 95.0f), 50.0f,
            juce::AudioParameterFloatAttributes().withLabel("%")));
        params.push_back(std::make_unique<APF>(
            pid("synth_skip"), disp("Skip"),
            juce::NormalisableRange<float>(0.0f, 8.0f, 1.0f), 0.0f,
            juce::AudioParameterFloatAttributes()));

        // ── Envelope Follower ────────────────────────────────────────
        params.push_back(std::make_unique<APF>(
            pid("env_attack_ms"), disp("Envelope Attack"),
            juce::NormalisableRange<float>(0.1f, 2000.0f, 0.0f, 0.3f), 1.0f,
            juce::AudioParameterFloatAttributes().withLabel("ms")));
        params.push_back(std::make_unique<APF>(
            pid("env_release_ms"), disp("Envelope Release"),
            juce::NormalisableRange<float>(5.0f, 5000.0f, 0.0f, 0.3f), 50.0f,
            juce::AudioParameterFloatAttributes().withLabel("ms")));
        params.push_back(std::make_unique<APC>(
            pid("env_source"), disp("Envelope Source"),
            juce::StringArray{ "Input", "Sidechain" }, 0));
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            pid("midi_trigger_enabled"), disp("MIDI Trigger"), false));
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            pid("midi_gate_release"),    disp("MIDI Gate Release"), false));

        // ── Synth Filter ──────────────────────────────────────────────
        params.push_back(std::make_unique<APF>(
            pid("synth_lpf_hz"), disp("Synth LPF"),
            juce::NormalisableRange<float>(200.0f, 20000.0f, 0.0f, 0.3f), 20000.0f,
            juce::AudioParameterFloatAttributes().withLabel("Hz")));
        params.push_back(std::make_unique<APF>(
            pid("synth_hpf_hz"), disp("Synth HPF"),
            juce::NormalisableRange<float>(20.0f, 2000.0f, 0.0f, 0.3f), 20.0f,
            juce::AudioParameterFloatAttributes().withLabel("Hz")));
        params.push_back(std::make_unique<APC>(
            pid("synth_filter_slope"), disp("Filter Slope"),
            juce::StringArray{ "-6 dB/oct", "-12 dB/oct", "-24 dB/oct" }, 1));

        // ── RESYN controls ────────────────────────────────────────────
        params.push_back(std::make_unique<APF>(
            pid("synth_wavelet_length"), disp("Wavelet Length"),
            juce::NormalisableRange<float>(5.0f, 100.0f), 100.0f,
            juce::AudioParameterFloatAttributes().withLabel("%")));
        params.push_back(std::make_unique<APF>(
            pid("synth_gate_threshold"), disp("Gate Threshold"),
            juce::NormalisableRange<float>(0.0f, 100.0f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel("%")));
        params.push_back(std::make_unique<APF>(
            pid("synth_h1"), disp("H1 Amp"),
            juce::NormalisableRange<float>(0.0f, 200.0f), 100.0f,
            juce::AudioParameterFloatAttributes().withLabel("%")));
        params.push_back(std::make_unique<APF>(
            pid("synth_sub"), disp("Sub Amp"),
            juce::NormalisableRange<float>(0.0f, 200.0f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel("%")));

        // Synth trim — post-envelope multiplier on the synth output. Lets the
        // user push the synth above the input's natural amplitude (the
        // envelope follower otherwise anchors synth loudness to the input).
        params.push_back(std::make_unique<APF>(
            pid("synth_trim"), disp("Synth Trim"),
            juce::NormalisableRange<float>(0.0f, 400.0f), 100.0f,
            juce::AudioParameterFloatAttributes().withLabel("%")));

        // ── Crossing detection ────────────────────────────────────────
        // Skew 0.35: more resolution at the low end where most useful values live.
        params.push_back(std::make_unique<APF>(
            pid("synth_min_samples"), disp("Min Waveset"),
            juce::NormalisableRange<float>(2.0f, 500.0f, 1.0f, 0.35f), 11.0f,
            juce::AudioParameterFloatAttributes().withLabel("smp")));

        params.push_back(std::make_unique<APF>(
            pid("synth_max_samples"), disp("Max Waveset"),
            juce::NormalisableRange<float>(100.0f, 8000.0f, 1.0f, 0.35f), 5513.0f,
            juce::AudioParameterFloatAttributes().withLabel("smp")));

        // ── Pitch tracking ────────────────────────────────────────────
        // Range 0.1–80 maps to alpha 0.001–0.800 (÷1000 in processor).
        // Skew 0.25: most knob travel covers the slow/glide region.
        params.push_back(std::make_unique<APF>(
            pid("tracking_speed"), disp("Tracking Speed"),
            juce::NormalisableRange<float>(0.1f, 100.0f, 0.0f, 0.25f), 15.0f,
            juce::AudioParameterFloatAttributes().withLabel("%")));

        // ── Punch ─────────────────────────────────────────────────────
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            pid("punch_enabled"), disp("Punch"), false));
        params.push_back(std::make_unique<APF>(
            pid("punch_amount"), disp("Punch Amount"),
            juce::NormalisableRange<float>(0.0f, 100.0f), 100.0f,
            juce::AudioParameterFloatAttributes().withLabel("%")));
        params.push_back(std::make_unique<APF>(
            pid("synth_boost_threshold"), disp("Boost Threshold"),
            juce::NormalisableRange<float>(0.0f, 100.0f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel("%")));
        params.push_back(std::make_unique<APF>(
            pid("synth_boost_amount"), disp("Boost Amount"),
            juce::NormalisableRange<float>(0.0f, 200.0f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel("%")));

        // ── Binaural ──────────────────────────────────────────────────
        params.push_back(std::make_unique<APC>(
            pid("binaural_mode"), disp("Binaural Mode"),
            juce::StringArray{ "Off", "Spread", "Voice-Split" }, 0));
        params.push_back(std::make_unique<APF>(
            pid("binaural_width"), disp("Binaural Width"),
            juce::NormalisableRange<float>(0.0f, 100.0f), 50.0f,
            juce::AudioParameterFloatAttributes().withLabel("%")));

        // ── Stereo ────────────────────────────────────────────────────
        params.push_back(std::make_unique<APF>(
            pid("stereo_width"), disp("Stereo Width"),
            juce::NormalisableRange<float>(0.0f, 200.0f), 100.0f,
            juce::AudioParameterFloatAttributes().withLabel("%")));
    };

    makeEngineParams("a_", "A. ");
    makeEngineParams("b_", "B. ");

    // Global (un-prefixed) ───────────────────────────────────────────────
    params.push_back(std::make_unique<AudioParameterBool>(
        ParamID::BYPASS, "Bypass", false));
    params.push_back(std::make_unique<APF>(
        ParamID::INPUT_GAIN, "Input Gain",
        NormalisableRange<float>(-12.0f, 24.0f), 0.0f,
        AudioParameterFloatAttributes().withLabel("dB")));
    params.push_back(std::make_unique<AudioParameterBool>(
        ParamID::INPUT_GAIN_AUTO, "Input Auto Gain", false));
    params.push_back(std::make_unique<AudioParameterBool>(
        ParamID::ADVANCED_OPEN, "Advanced Panel", false,
        juce::AudioParameterBoolAttributes().withAutomatable(false)));

    // Morph crossfader ───────────────────────────────────────────────────
    params.push_back(std::make_unique<APF>(
        ParamID::MORPH_AMOUNT, "Morph",
        NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<APC>(
        ParamID::MORPH_CURVE, "Morph Curve",
        StringArray{ "Linear", "Eq-Power", "S-Curve" }, 0));
    params.push_back(std::make_unique<APF>(
        ParamID::MORPH_A_LEVEL_DB, "Morph A Level",
        NormalisableRange<float>(-24.0f, 12.0f), 0.0f,
        AudioParameterFloatAttributes().withLabel("dB")));
    params.push_back(std::make_unique<APF>(
        ParamID::MORPH_B_LEVEL_DB, "Morph B Level",
        NormalisableRange<float>(-24.0f, 12.0f), 0.0f,
        AudioParameterFloatAttributes().withLabel("dB")));
    params.push_back(std::make_unique<AudioParameterBool>(
        ParamID::MORPH_BYPASS_IDLE_ENGINE, "Morph Bypass Idle Engine", true));

    // Macros (PR3a) — global APVTS params, automatable. Read by Macro
    // modulators in ModulationEngine.
    params.push_back(std::make_unique<APF>(
        ParamID::MACRO1, "Macro 1",
        NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<APF>(
        ParamID::MACRO2, "Macro 2",
        NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<APF>(
        ParamID::MACRO3, "Macro 3",
        NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<APF>(
        ParamID::MACRO4, "Macro 4",
        NormalisableRange<float>(0.0f, 1.0f), 0.0f));

    return { params.begin(), params.end() };
}
