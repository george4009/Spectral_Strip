# Spectral Strip – building and development notes

A modular spectral multi-effect for macOS (AU, VST3, standalone), Windows and Linux
(VST3, standalone), built with JUCE.

Spectral Strip runs up to 20 effects — **Freeze, Gate / Tilt, Delay, Blur, Tremolo and
Shimmer** — in any order, inside one shared short-time Fourier transform. A common
LOW–HIGH band decides which frequencies the effects touch; a band filter, dry/wet and
output sit at the end of the chain.

![Spectral Strip editor](screenshot.png)

## Modules

| Module | What it does | Controls |
| --- | --- | --- |
| **Freeze** | Captures the current spectrum and sustains it; phases keep advancing at the measured rate so the held sound stays alive. | Freeze (capture), power. *Smear* (temporal smoothing before capture) is kept for automation but hidden. |
| **Gate / Tilt** | Per-bin soft-knee gate with its own attack/release, plus an independent tone tilt around 1 kHz. | Threshold, Tilt (±6 dB/oct, ±18 dB at the band ends), Attack, Release, Gate, Invert |
| **Delay** | Spectral delay with feedback. Dispersion spreads the delay times across the band (evenly in octaves); Damping darkens every repeat. | Time, Dispersion, Feedback, Damping, Mix, power |
| **Blur** | Spreads energy between neighbouring frequencies while keeping the band's total power. | Amount, Width (20 Hz–4 kHz), Mix, power |
| **Tremolo** | Amplitude modulation applied sample by sample inside each frame, so fast rates survive large FFT sizes. | Rate (0.1–20 Hz), Depth, Shape (sine → rounded square → short pulses), power |
| **Shimmer** | An eight-line feedback delay network reverb with an octave-up voice in the input and in the feedback loop. | Decay (0.5–20 s), Size, Shimmer, Tone, Mix, power |

Every module can appear several times. Each copy (an *instance*) keeps its own parameters
and memory, and moving it in the strip keeps both.

Global controls (right sidebar and bottom bar):

- **Low / High** – the effect band, also draggable as the two dots on the analyser.
- **Filter** – also filters the processed signal outside the band. It acts on the wet
  signal only: set Dry/wet to 1.00 for a complete cut.
- **Dry/wet** – latency-compensated mix. **Output** – final gain (no limiter).
- **FFT** – 512 … 8192 samples. Larger sizes give finer frequency resolution and more
  latency. Changing it fades out, clears Freeze/Delay memory and fades back in.

The info bar at the bottom shows the name, value and help of the control under the
mouse, or the current FFT size and latency.

## Latency

The plug-in reports its latency to the host: one FFT frame (2048 samples = 42.7 ms at
48 kHz by default), plus 3/4 of a frame for each Shimmer module, which works in the time
domain between an extra pair of transforms. It is not designed for low-latency live
monitoring.

## Building

Requirements: macOS 11+, Xcode, CMake 3.22+ and [JUCE](https://juce.com) 8 or later
(the project is developed with JUCE 9.0.2).

The project applies a small patch to JUCE's AU wrapper (`patches/`) so that the list of
automatable AU parameters can change safely while a host is reading it. The scripts
below apply it automatically; with your own JUCE checkout run:

```sh
patch -p1 -d /path/to/JUCE < patches/juce-au-automatable-parameter-list.patch
```

Generate and open the Xcode project:

```sh
JUCE_PATH=/path/to/JUCE scripts/generate-xcode.command
```

or build from the command line:

```sh
cmake -S . -B Builds -G Xcode -DJUCE_PATH=/path/to/JUCE
cmake --build Builds --config Release --target SpectralStrip_All SpectralStripTests
```

Products end up in `Builds/SpectralStrip_artefacts/Release/` (`AU/`, `VST3/`, `Standalone/`).
To use them in a DAW, copy the `.component` to `~/Library/Audio/Plug-Ins/Components/`
and/or the `.vst3` to `~/Library/Audio/Plug-Ins/VST3/`, then rescan.

`scripts/build-and-install.command` does all of it in one go: configures, builds the
Release AU/VST3 and the tests, runs the tests, creates an installer package, installs
into your user plug-in folders (keeping a backup of the previous copy) and validates the
AU with `auval`. `scripts/build-installer.command` only creates the `.pkg`. Packages are
signed ad hoc and not notarised.

The standalone app starts with the audio input muted to avoid feedback; choose the input
in its settings and use headphones.

### Windows and Linux

The same CMake project builds the VST3 and the standalone app on Windows (Visual Studio
2022) and Linux (GCC or Clang, with the ALSA, FreeType, fontconfig and X11 development
packages). JUCE does not support cross-compiling, so each platform is built on its own
system.

`.github/workflows/build.yml` builds and tests all three platforms on GitHub Actions for
every push and attaches the archives to a release when a `v*` tag is pushed.

## Tests

`SpectralStripTests` is a console runner that checks, among other things, transparent
reconstruction, dry latency compensation, every FFT size and size changes, Freeze
sustain, gate and delay timing, the band filter, state recall (including sessions from
older versions), chain edits while audio is running and the reverb's tail.

```sh
ctest --test-dir Builds -C Release --output-on-failure
```

Extra modes of the runner:

- `--benchmark [scenario]` – DSP time relative to real time for a few typical chains.
- `--dump file` – writes the raw output of many automated scenarios, to compare two
  versions bit for bit.

`Tests/AUParameterStress.mm` stresses the built AU's parameter list from several threads,
and `SpectralStripPreview` opens the editor in a window without a host.

## Source overview

| Path | Contents |
| --- | --- |
| `Source/SpectralEngine.h` | The STFT engine and all spectral effects. Real-time safe. |
| `Source/ShimmerReverb.h` | The time-domain reverb used by the Shimmer module. |
| `Source/PluginProcessor.*` | Parameters, state and session compatibility, the module chain, the audio callback. |
| `Source/PluginEditor.*` | The editor window: title, sidebar, bottom bar and info bar. |
| `Source/UI/SpectrumView.*` | The analyser and the draggable band edges. |
| `Source/UI/ModuleStrip.*` | The scrolling row of modules: controls, reordering, the "+" slot. |
| `Source/UI/ModuleSpecs.h` | Which controls each module shows. |
| `Source/UI/ModuleArt.*` | The parameter-driven illustrations at the top of each module. |
| `Source/UI/StripLookAndFeel.*`, `Theme.h` | Colours, layout constants and control drawing. |
| `Tests/` | The test runner, the AU stress tool and the editor preview app. |

Parameter IDs and their order are part of saved sessions and host automation. New
parameters are appended, and old ones are kept even when the editor no longer shows them.

## History and limitations

See [CHANGELOG.md](../CHANGELOG.md). The original development notes and validation log (in
Italian) are in this folder.

Known limitations: no preset browser, no XY pad or morphing, no tempo sync for Delay or
Tremolo, Freeze and Delay memory are not saved with the session, and the Tremolo and
Shimmer modules cost noticeably more CPU than the others.

## License

Spectral Strip is free software, released under the GNU Affero General Public License
v3.0 (see [LICENSE](../LICENSE)), which is the open-source license JUCE is available under.

The editor embeds a Latin subset of the [Lato](https://www.latofonts.com) typeface by
Łukasz Dziedzic (renamed "Strip Sans", as the license requires for modified versions),
under the SIL Open Font License 1.1 (see [licenses/Lato-OFL.txt](../licenses/Lato-OFL.txt)).
`scripts/make-editor-fonts.py` regenerates it.

The visual design is inspired by the style of contemporary spectral tools; Spectral Strip
contains no third-party trademarks, artwork or code beyond JUCE and Lato.
