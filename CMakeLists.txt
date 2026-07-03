cmake_minimum_required(VERSION 3.22)

project(AmpForge VERSION 0.1.0 LANGUAGES C CXX)

# NAM core requires C++20, so the whole project uses it.
set(CMAKE_CXX_STANDARD 20)
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

# ---------------------------------------------------------------------------
# Fetch Neural Amp Modeler core + Eigen so AmpForge can load .nam captures
# (e.g. from tone3000.com). We only need the sources, not their own CMake
# builds/tests, so SOURCE_SUBDIR points at a non-existent dir to skip
# add_subdirectory. GIT_SUBMODULES "" avoids pulling NAM's heavy submodules
# (we supply Eigen ourselves; nlohmann json is checked into the NAM repo).
# ---------------------------------------------------------------------------
FetchContent_Declare(
    namcore
    GIT_REPOSITORY https://github.com/sdatkinson/NeuralAmpModelerCore.git
    GIT_TAG        v0.5.4
    GIT_SHALLOW    TRUE
    GIT_SUBMODULES ""
    SOURCE_SUBDIR  does-not-exist
)
# Pinned to the exact Eigen commit NAM v0.5.4 uses. The 3.4.0 release is too
# old (missing Eigen::placeholders::lastN that NAM's lstm.h needs).
FetchContent_Declare(
    Eigen
    GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
    GIT_TAG        bc3b39870ecb690a623a3f49149a358b95c5781d
    SOURCE_SUBDIR  does-not-exist
)
FetchContent_MakeAvailable(JUCE namcore Eigen)

# Build the NAM core DSP as a static library we link into the plugin.
file(GLOB NAM_SOURCES
    "${namcore_SOURCE_DIR}/NAM/*.cpp"
    "${namcore_SOURCE_DIR}/NAM/wavenet/*.cpp")
add_library(nam STATIC ${NAM_SOURCES})
target_include_directories(nam SYSTEM PUBLIC
    "${namcore_SOURCE_DIR}/NAM"
    "${namcore_SOURCE_DIR}/Dependencies/nlohmann"
    "${eigen_SOURCE_DIR}")
target_compile_features(nam PUBLIC cxx_std_20)
target_compile_definitions(nam PUBLIC NAM_ENABLE_A2_FAST)

# ---------------------------------------------------------------------------
# The plugin target. Builds VST3 + Standalone everywhere; AU is macOS-only
# (Audio Unit is an Apple format and cannot be built by MSVC on Windows).
# ---------------------------------------------------------------------------
if(APPLE)
    set(AMPFORGE_FORMATS VST3 AU Standalone)
else()
    set(AMPFORGE_FORMATS VST3 Standalone)
endif()

juce_add_plugin(AmpForge
    COMPANY_NAME              "Brian Smith"
    PLUGIN_MANUFACTURER_CODE  Bsmi        # 4 chars, one uppercase
    PLUGIN_CODE               Ampf        # 4 chars, one uppercase; unique per plugin
    BUNDLE_ID                 com.briansmith.ampforge   # no spaces (COMPANY_NAME has one)
    FORMATS                   ${AMPFORGE_FORMATS}
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
    # Whole-archive link: NAM's model architectures (WaveNet, SlimmableContainer
    # for A2 captures, LSTM, ConvNet) self-register via static initializers in
    # separate object files. Without whole-archive the linker drops those files
    # and get_dsp() reports "No config parser registered for architecture: ...".
    "$<LINK_LIBRARY:WHOLE_ARCHIVE,nam>"
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
