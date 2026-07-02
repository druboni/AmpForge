cmake_minimum_required(VERSION 3.22)

project(AmpForge VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Fetch JUCE (pinned to a release). This downloads JUCE at configure time so
# there is no submodule to manage. Change GIT_TAG to bump the JUCE version.
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------------------
# The plugin target. Builds VST3, AU and a Standalone app.
# ---------------------------------------------------------------------------
juce_add_plugin(AmpForge
    COMPANY_NAME              "druboni"
    PLUGIN_MANUFACTURER_CODE  Drbn        # 4 chars, one uppercase
    PLUGIN_CODE               Ampf        # 4 chars, one uppercase; unique per plugin
    FORMATS                   VST3 AU Standalone
    PRODUCT_NAME              "AmpForge"
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    # Required so the macOS standalone can receive live audio input (guitar).
    # Without this the OS hands the app silence and you hear nothing.
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT    "AmpForge needs audio input access to process your guitar signal."
    COPY_PLUGIN_AFTER_BUILD   TRUE        # install into the user plugin folders on build
)

target_sources(AmpForge PRIVATE
    source/PluginProcessor.cpp
    source/PluginEditor.cpp
)

target_compile_definitions(AmpForge PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(AmpForge PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
