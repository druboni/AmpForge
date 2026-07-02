# AmpForge

A guitar amp emulator audio plugin for your DAW, built with [JUCE](https://juce.com).
Builds as **VST3**, **AU** (Audio Unit, for Logic/GarageBand), and a **Standalone** app.

## Signal chain

```
input
  → [ DS-1 style distortion pedal ]   (optional, switchable)
        input HP → Dist gain → hard diode clip (4x oversampled)
        → tone tilt + mid notch → Level
  → AMP
        drive (+ power-supply sag) → tube waveshaper (4x oversampled)
        → tone stack (Bass / Mid / Treble) → Presence
        → cabinet (convolution IR) → Master
```

Both non-linear stages are **4× oversampled** to keep aliasing out of the
distortion, and the plugin reports its processing latency so the DAW stays in
sync.

## Controls

**Amp**

| Knob     | What it does                                             |
|----------|----------------------------------------------------------|
| Drive    | Pre-gain into the tube waveshaper                        |
| Bass     | Low-shelf tone control                                   |
| Mid      | Midrange peak control                                    |
| Treble   | High-shelf tone control                                  |
| Presence | Extra high-end sparkle after the tone stack             |
| Master   | Output level                                             |
| Cab      | Toggle the convolution cabinet on/off                    |

**DS-1 Distortion pedal** (toggle to enable; runs *before* the amp)

| Knob  | What it does                                             |
|-------|----------------------------------------------------------|
| Dist  | Pre-clip gain — how hard it slams the diode clipper      |
| Tone  | Dark ↔ bright tilt with the classic DS-1 mid character   |
| Level | Pedal output level into the amp                          |

**Presets:** Clean · Crunch · Lead · DS-1 Lead (menu in the top-right).

## Cabinet impulse responses

The cab uses a `juce::dsp::Convolution` engine. Out of the box it loads a
synthesized 4×12-style impulse response. To use your own measured IR, call
`CabinetSim::loadIR (juce::File)` from the processor (e.g. wire it to a
"Load IR" button) — any mono/stereo `.wav` IR works.

## Building

Requires CMake ≥ 3.22 and a C++17 compiler. JUCE is fetched automatically by CMake.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The first configure downloads JUCE (pinned in `CMakeLists.txt`). Because
`COPY_PLUGIN_AFTER_BUILD` is on, the VST3/AU are installed into your user
plugin folders automatically:

- **VST3:** `~/Library/Audio/Plug-Ins/VST3/`
- **AU:** `~/Library/Audio/Plug-Ins/Components/`

Rescan plugins in your DAW and load **AmpForge**. To try it without a DAW, run
the Standalone build in `build/AmpForge_artefacts/`.

Validate the AU (as Logic does) with:

```bash
auval -v aufx Ampf Drbn
```

## Project layout

```
CMakeLists.txt                # build config + JUCE fetch
source/
  PluginProcessor.{h,cpp}     # processor, parameters (APVTS), presets, latency
  PluginEditor.{h,cpp}        # knob UI + pedal section + preset menu
  dsp/
    AmpEngine.h               # amp: drive, sag, oversampled waveshaper, tone
    DistortionPedal.h         # DS-1 style pedal: oversampled diode clipper
    CabinetSim.h              # convolution cabinet + synthesized default IR
```
