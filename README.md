# AmpForge

A guitar amp emulator audio plugin for your DAW, built with [JUCE](https://juce.com).
Builds as **VST3**, **AU** (Audio Unit, for Logic/GarageBand), and a **Standalone** app.

## Signal chain

```
input → drive (pre-gain) → tube-style waveshaper
      → tone stack (Bass / Mid / Treble)
      → Presence (high shelf) → Cabinet (low-pass)
      → Master
```

## Controls

| Knob     | What it does                                             |
|----------|----------------------------------------------------------|
| Drive    | Pre-gain into the non-linearity — more = more distortion |
| Bass     | Low-shelf tone control                                   |
| Mid      | Midrange peak control                                    |
| Treble   | High-shelf tone control                                  |
| Presence | Extra high-end sparkle after the tone stack             |
| Cabinet  | Low-pass cutoff emulating a speaker cabinet              |
| Master   | Output level                                             |

## Building

Requires CMake ≥ 3.22 and a C++17 compiler. JUCE is fetched automatically by CMake.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The first configure downloads JUCE (pinned in `CMakeLists.txt`), so it takes a
few minutes. Because `COPY_PLUGIN_AFTER_BUILD` is on, the VST3/AU are copied
into your user plugin folders automatically:

- **VST3:** `~/Library/Audio/Plug-Ins/VST3/`
- **AU:** `~/Library/Audio/Plug-Ins/Components/`

Rescan plugins in your DAW and load **AmpForge**. To try it without a DAW, run
the Standalone build in `build/AmpForge_artefacts/`.

## Project layout

```
CMakeLists.txt              # build config + JUCE fetch
source/
  PluginProcessor.{h,cpp}   # audio processor + parameters (APVTS)
  PluginEditor.{h,cpp}      # the knob UI
  dsp/AmpEngine.h           # the amp DSP (drive, tone stack, cab)
```
