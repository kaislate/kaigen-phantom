#pragma once

#include <juce_core/juce_core.h>
#include <string>
#include <vector>

namespace kaigen::phantom {

// Preset metadata (name, type, designer, favorite status)
struct PresetMetadata {
    juce::String name;           // "Warm Bass Boost"
    juce::String type;           // "Piano", "Drone", "Synth", "Bass", "Experimental"
    juce::String designer;       // "Kai Slate" or username
    bool isFavorite = false;
    bool isFactory = false;      // True if from Factory/ folder
    juce::String packName;       // "Factory", "User", "Analog Vibes", etc.
};

// Preset file info with full path
struct PresetInfo {
    PresetMetadata metadata;
    juce::File file;             // Full path to .fxp file
};

} // namespace kaigen::phantom
