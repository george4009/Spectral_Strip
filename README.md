# Spectral Strip

A modular spectral multi-effect plug-in (AU, VST3 and standalone) made with [JUCE](https://juce.com).

![Spectral Strip](docs/screenshot.png)

Chain up to 20 spectral effects in any order and choose which frequencies they act on.

## Effects

- **Freeze** – holds the current sound as a sustained, evolving texture.
- **Gate / Tilt** – removes quiet frequency components; tilts the tone darker or brighter.
- **Delay** – spectral echoes, with different delay times for lows and highs.
- **Blur** – smears energy between neighbouring frequencies.
- **Tremolo** – rhythmic volume modulation, from smooth sine to short pulses.
- **Shimmer** – a lush reverb with an octave-up voice.

Every effect can be added several times, reordered by dragging its title, and switched on and off.
The **Low / High** band (also draggable on the analyser), **Filter**, **Dry/wet** and **Output** are
in the sidebar; the **FFT** size is at the bottom right.

## Download

Ready-made builds for macOS, Windows and Linux are on the
[Releases](https://github.com/george4009/Spectral_Strip/releases) page.

- **macOS**: copy `Spectral Strip.component` (AU) to `~/Library/Audio/Plug-Ins/Components/`
  and `Spectral Strip.vst3` to `~/Library/Audio/Plug-Ins/VST3/`.
- **Windows**: copy `Spectral Strip.vst3` to `C:\Program Files\Common Files\VST3\`.
- **Linux**: copy `Spectral Strip.vst3` to `~/.vst3/`.

Then rescan plug-ins in your DAW. The builds are not signed by a registered developer, so
macOS and Windows may ask for confirmation the first time.

## Build from source

You need CMake 3.22+, a C++17 compiler and JUCE 9:

```sh
git clone https://github.com/juce-framework/JUCE.git --branch 9.0.2
git -C JUCE apply ../Spectral_Strip/patches/juce-au-automatable-parameter-list.patch
cmake -S Spectral_Strip -B build -DJUCE_PATH=$PWD/JUCE -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

More details (tests, scripts, code overview) are in [docs/BUILDING.md](docs/BUILDING.md).
Changes are listed in [CHANGELOG.md](CHANGELOG.md).

## License

GNU AGPL v3 – see [LICENSE](LICENSE). The interface font is a subset of Lato (SIL Open Font License,
see [licenses](licenses)).
