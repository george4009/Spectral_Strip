# Changelog

## 0.8.3

- Removed the centre mark on the thumbs of bipolar sliders (Tilt, Dispersion, Output).

## 0.8.2

- The instance number ("#n") is no longer shown in the module headers.

## 0.8.1

- Fixed: the window resize corner was scaled with the interface and drifted away from the corner.

## 0.8.0 – first public release

- Code cleanup for publication: the engine's effects are split into one function each,
  the editor into small components (`SpectrumView`, `ModuleStrip`, look and feel,
  module specs and illustrations), per-module parameters are described by tables
  instead of magic indices, and every comment is in English.
- Shimmer: the two octave shifters of each voice share their grain window, halving the
  trigonometry per sample. The audio output is bit-identical to 0.7.22 (verified on all
  effects, FFT sizes and sample rates).
- Host parameter names of module instances are readable ("Gate 4 threshold" instead of
  "instance4_threshold"). Parameter IDs and order are unchanged.
- New help texts for Dispersion, Damping and Invert in the info bar.
- English README, AGPLv3 license, Lato OFL notice, scripts moved to `scripts/`.

## 0.7.x – modular strip, new effects and the new interface

- **0.7.22** – Continuous scrolling of the strip (trackpad 1:1, animated mouse wheel,
  auto-scroll while dragging a module).
- **0.7.21** – The "+" of the empty slot opens the module list right below itself.
- **0.7.20** – Module illustrations follow their parameters (Freeze and Tremolo animate).
- **0.7.19** – New flat, dark interface: horizontal sliders, power buttons, vertical
  sidebar, info bar, analyser with draggable band edges, embedded Lato.
- **0.7.16** – Smear rewritten with two cascaded envelopes and continuous phase; the
  visible Gate Tilt became a tone tilt around 1 kHz (new `toneTilt` parameters).
- **0.7.15** – Fixed memory corruption when adding modules while the AU parameter list
  was being read (JUCE patch in `patches/`); installer with correct permissions.
- **0.7.13** – No allocation in the audio callback: Delay and Shimmer memory is
  allocated on the message thread when a module is inserted.
- **0.7.10–0.7.12** – Only the parameters of modules in the strip are automatable;
  removed modules keep their settings for when they come back.
- **0.7.8** – Up to 20 modules.
- **0.7.6–0.7.7** – Optimisation (per-bin tables, shared band mask, exact fast phase
  wrapping), benchmark and dump modes in the test runner.
- **0.7.3** – Tremolo Shape from sine to short pulses.
- **0.7.1** – Shimmer reverb module (eight-line FDN with an octave-up voice).

## 0.1 – 0.6

- **0.6.0** – Octave "shimmer" inside Freeze (later replaced by the Shimmer module).
- **0.5.0** – Tremolo module.
- **0.4.0** – Blur module; **0.4.3** the band filter can close completely.
- **0.3.0** – Modular strip: reorderable modules, several instances of each effect.
- **0.2.0** – Gate attack/release, Delay damping, logarithmic dispersion.
- **0.1.0** – Freeze, Gate and Delay on a selectable-size STFT with a LOW–HIGH band;
  **0.1.1** clean restarts on play and seek; **0.1.2** band filter.
