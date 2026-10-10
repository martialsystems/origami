
# Origami

**A stereo triple wave folder. Part of the Jidai Collection by Martial Systems.**

ORIGAMI folds your sound three times over. Feed it a bass line, a pad or a full mix, turn one WAVE knob, and the signal folds back on itself through three stages in series, from a warm edge to dense, glassy harmonics. Every stage can be shaped on its own and modulated at audio rate, and the whole thing stays level and time-aligned with your dry signal, so you can blend it in like any other insert.
It runs as a VST3 effect, as a standalone app, and as a device in the JIDAI RACK, where its audio and CV jacks patch into the rest of the rack.

## Download:

- [Mac](https://github.com/martialsystems/origami/releases/latest/download/ORIGAMI-macOS.zip)
- [Windows](https://github.com/martialsystems/origami/releases/latest/download/ORIGAMI-Windows.zip)
  
- [Jidai Collection](https://github.com/martialsystems/jidai-collection)
  
**Found a bug?** Please [open a GitHub issue](https://github.com/martialsystems/origami/issues/new) and fill in the bug report form.

## Highlights

- **Three folding stages, one macro.** WAVE sweeps stages 1, 2 and 3 in turn, so a single knob goes from clean to deeply folded. STAGE 1–3 push each stage above or below the macro, and SYM (plus SYM 1–3 per stage) tilts the fold for even harmonics.
- **True bypass at zero.** With WAVE at 0 and everything else at its default, ORIGAMI passes your audio through bit for bit.
- **Audio-rate modulation.** VC 1, VC 2 and VC 3 each drive one stage's amount and symmetry. Feed them from the input itself, the envelope follower or the sidechain (in the rack, the SC L/R jacks on the back), or in the rack from any jack.
- **Pre-fold VCA.** Drive the folder from the envelope follower or from CV, with a linear or exponential law, ATTACK and RELEASE, and a CV scale.
- **Emphasis.** EMPH boosts the highs before the folder and removes the boost exactly after it, so the fold bites harder on bright material without making it harsher.
- **Even loudness.** LEVEL COMP holds the output level steady while you sweep WAVE. An 8 Hz DC block keeps asymmetric folds centred.
- **Parallel blend.** MIX is dry/wet with the dry path delayed to match the wet, so blending never smears. LEVEL sets the output.
- **Clean or cleaner.** QUALITY 1x uses antiderivative anti-aliasing with no latency. 2x adds a 93-tap halfband oversampler for very low aliasing and reports its latency to your DAW.
- **Stereo-linked.** Left and right share every control and the detectors, so the image holds.
- **Four pages.** MAIN, STAGES, DYNAMICS and SETUP, with input, output and follower meters and an OVER light.
- **33 factory presets in banks.** INIT plus banks voiced for each Jidai source, picked from the preset box in the header (in the rack, in the jack row) or from your DAW's program list.

## Factory presets

Click the preset name in the header for a menu grouped by bank, or step with the arrows. Your DAW lists the same presets as programs, named `BANK: Name`. Presets are built into the plugin, so nothing needs installing on any platform. Every preset has LEVEL COMP on and is level-checked to stay below 0 dBFS on the test material.

| Bank | Voiced for | Presets |
| --- | --- | --- |
| INIT | Everything at default: true bypass | INIT |
| RONIN | RONIN's VCO and filter levels: basses and leads | Acid Grit, Acid Squelch Fold, Reese Growl, Wavefolded Bass, Lead Bite, Pluck Edge, Hard Sync-Style Lead, Sub-Safe Bass Drive |
| SHOGUN | SHOGUN's drum levels and transients | Drum Smash, Kick Fold, Hat Sizzle, Drum-Bus Glue, Snare Crack, Transient Fold, Tom Bloom, Parallel Drum Grit |
| BUSHIDO | VC 1–3 on their jacks, ready for BUSHIDO's CV A/B/C; sensible unpatched | Stepped Fold Sequence, Stepped Symmetry, Row C Fold Accent, Pitch-Tracked Fold, Fold Arpeggio, Gate-Opened Fold, Stepped Bite |
| GENERIC | Any source, buses and full mixes | Warm Bus Glue, Analog Edge, Even-Order Bloom, Lopsided Fold, Self-Modulated Fold, Stereo Shimmer Fold, Sidechain Pump Fold, Full Fold, Shattered |

The bank lives in `presets/factory.xml`. Each preset is a complete saved state, in the same format the plugin and the rack device save.

## Quick start

1. Build ORIGAMI (see Build), then copy `ORIGAMI.vst3` into your VST3 folder:
   - macOS: `~/Library/Audio/Plug-Ins/VST3/`
   - Linux: `~/.vst3/`
2. Rescan plugins in your DAW and insert ORIGAMI on a track.
3. Raise WAVE until you hear the fold, set SYM for colour, and use MIX to blend it back in.

The standalone app opens your default audio input and output.

## Build

Needs CMake 3.22 or later and a C++20 compiler. JUCE 8.0.4 is downloaded during configure.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The plugin lands in `build/Origami_artefacts/Release/`, in `VST3/ORIGAMI.vst3` and `Standalone/ORIGAMI`. To use a JUCE 8.0.4 checkout you already have, add `-DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE`.

## Tests

```sh
ctest --test-dir build --output-on-failure
```

- **`origami`** tests the DSP core on its own. It checks true bypass, latency, stereo link, the exact emphasis inverse, level, symmetry, audio-rate VC, aliasing at 1x and 2x, and silence, NaN and denormal handling.
- **`origami_plugin`** tests the plugin without a host. It checks the standalone input normals, latency reporting, the saved state and its round trip, the editor's sizes and controls, and every factory preset: it loads, its state round-trips byte for byte, and on a sine sweep plus drum hits (and on a bass line for RONIN or a drum loop for SHOGUN, at −1 dBFS) its output is finite, audible and below 0 dBFS. Run `build/OrigamiPluginTests_artefacts/Release/OrigamiPluginTests <folder>` to also save screenshots of every page.

## Compatibility

ORIGAMI follows the **Jidai Cable Standard v1.1**, the patching rules every Jidai Collection device shares, so in the JIDAI RACK it patches into the other devices with no adapters.

- **Levels.** Audio is ±5 V. VCA CV is 0 to 5 V. The OVER light shows when a signal goes past the rails.
- **Pitch.** The collection uses 1 V/oct with C3 = 130.81 Hz at 0 V. ORIGAMI has no pitch jacks, so any pitch CV patched into it is treated as ordinary CV.
- **Latency.** 0 samples at 1x and 46 samples at 2x. ORIGAMI reports it to the DAW, and the rack compensates for it on every path to its output.
- **Presets.** One saved-state format for the plugin and the rack device, so a preset loads in either one.
- **Formats.** VST3 and Standalone, for macOS and Linux.
- **Same numbers everywhere.** ORIGAMI's targets build with `-ffp-contract=off` on clang and gcc, so Apple silicon, Intel and Linux compute the same samples. The option `ORIGAMI_STRICT_FP` (on by default) controls it.

The shared standard code lives in `third_party/jidai-common`. `VENDOR.md` there names the exact version.

## In the JIDAI RACK

Inside the [JIDAI RACK](https://github.com/martialsystems/jidai-collection), ORIGAMI is a 3 U effect. Open it for the full panel with its jacks, close it to a 1 U strip, or flip the rack to the back to see every jack:

- **IN L and IN R** take audio. IN R follows IN L when only IN L is patched.
- **VCA CV** sets the pre-fold VCA when VCA SOURCE is CV.
- **VC 1, VC 2 and VC 3** modulate each stage at audio rate. A patched jack takes over from the internal source.
- **OUT L and OUT R** send audio.
- **HOST IN L/R** (back only) feed IN L/R while those are unpatched. **HOST OUT L/R** (back only) carry the same signal as OUT L/R.

The rack includes ORIGAMI from this repository at a pinned commit through CMake `FetchContent`:

```cmake
FetchContent_Declare(origami GIT_REPOSITORY https://github.com/martialsystems/origami.git GIT_TAG <commit>)
FetchContent_MakeAvailable(origami)
target_link_libraries(<target> PUBLIC origami::dsp)   # #include "origami/dsp/OrigamiCore.h"
# compile ${ORIGAMI_UI_SOURCES} into your JUCE target     # #include "origami/plugin/OrigamiPanel.h"
```

If your project already defines `jidai::common`, ORIGAMI uses it instead of its own copy. To offer the factory presets, also compile `${ORIGAMI_PRESET_SOURCES}` and link `origami::factory_data`.

## Legal

Copyright © 2026 Martial Systems LLC. All rights reserved. See [LICENSE](LICENSE).
