# AmpForge

A guitar amp emulator audio plugin for your DAW, built with [JUCE](https://juce.com).
Builds as **VST3**, **AU** (Audio Unit, for Logic/GarageBand), and a **Standalone** app.

## Signal chain

```
input
  → [ DS-1 style distortion pedal ]   (optional, switchable)
        input HP → Dist gain → hard diode clip (4x oversampled)
        → tone tilt + mid notch → Level
  → AMP  — one of:
        (a) NAM capture (.nam)     neural model of a real amp (tone3000.com)
        (b) algorithmic amp        drive (+sag) → cascaded tube waveshaper
                                   (4x oversampled) → tone stack → Presence
  → cabinet (convolution IR)   (shared by both amp paths, switchable)
  → Master
```

## NAM amp captures (Neural Amp Modeler)

AmpForge can load **`.nam` captures** — neural-network models of real amplifiers,
e.g. the thousands of free captures at **[tone3000.com](https://www.tone3000.com/)**.
Click **Load NAM…**, pick a `.nam` file, and the **NAM Amp** toggle turns on;
the capture then replaces the built-in algorithmic amp.

- Powered by the open-source [NeuralAmpModelerCore](https://github.com/sdatkinson/NeuralAmpModelerCore) (MIT), fetched + built by CMake.
- Models are **mono** and have an expected sample rate (usually 48 kHz). For the
  most accurate tone, run your DAW at the model's rate — the UI warns if they differ.
- "Amp only" captures pair well with a cab IR (keep the Cabinet on); "Amp + Cab"
  captures already include the speaker, so you may want the Cabinet off.

Both non-linear stages are **4× oversampled** to keep aliasing out of the
distortion, and the plugin reports its processing latency so the DAW stays in
sync.

## Controls

**Amp** — pick a voicing from the **Amp** menu: *Modern, Fender Clean, Plexi,
JCM800, Rectifier*. Each changes the tone-stack circuit, gain staging and input
tightness.

| Control  | What it does                                             |
|----------|----------------------------------------------------------|
| Gate     | Noise gate before the amp (0 = off) — tames hiss on high gain |
| Drive    | Pre-gain into the cascaded tube stages                   |
| Bass/Mid/Treble | Interactive passive **tone stack** — modelled from the real Marshall (JCM800) / Fender component network, so the controls load each other like the actual circuit |
| Presence | Power-amp high-shelf sparkle                             |
| Master   | Output level                                             |
| Cabinet  | Toggle the convolution cab; **Cab** menu picks Modern / Vintage 4x12 voicing |

The Bass/Mid/Treble stack uses the actual analog transfer function (component
values + coefficients from the Faust tonestacks library), discretised with the
bilinear transform — this is what makes the JCM800 voicing sound like a Marshall.

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

Requires CMake ≥ 3.22 and a C++20 compiler. JUCE, NAM core and Eigen are all
fetched automatically by CMake on the first configure.

Formats built per platform: **VST3 + Standalone** everywhere; **AU** on macOS
only (Audio Unit is Apple-specific).

### macOS / Linux

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### Windows (Visual Studio 2022)

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

If `cmake` isn't on your PATH, the one bundled with Visual Studio works:
`"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"`.

Because `COPY_PLUGIN_AFTER_BUILD` is on, the plugin is installed into your user
plugin folder automatically:

- **VST3 (macOS):** `~/Library/Audio/Plug-Ins/VST3/`
- **AU (macOS):** `~/Library/Audio/Plug-Ins/Components/`
- **VST3 (Windows):** `C:\Program Files\Common Files\VST3\`

Rescan plugins in your DAW and load **AmpForge**. To try it without a DAW, run
the Standalone build in `build/AmpForge_artefacts/Release/Standalone/`
(`AmpForge.exe` on Windows).

On macOS, validate the AU (as Logic does) with:

```bash
auval -v aufx Ampf Bsmi
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
