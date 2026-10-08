# ORIGAMI

**A stereo triple wave folder from Martial Systems.**

ORIGAMI folds a signal through three wave-folding stages in series, in the west-coast tradition. One WAVE macro sweeps all three stages, staggered, from clean to dense. Each stage also has its own trim, symmetry, and a VC input that runs at audio rate. Around the folder are a pre-fold VCA (envelope follower or CV), pre/post emphasis, an 8 Hz DC block, LEVEL COMP to hold loudness through the sweep, and a dry/wet MIX that stays aligned with the wet signal. ORIGAMI is part of the Jidai Collection. It runs as a standalone effect and as a device in the Jidai Rack, and both use the same DSP core and the same state format.

## Build

Needs CMake 3.22+ and a C++20 compiler. JUCE 8.0.4 is fetched automatically, the same way the Jidai Collection rack fetches it.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

This builds the VST3 and Standalone in `build/Origami_artefacts/Release/`. To use an existing JUCE 8.0.4 checkout, pass `-DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE`.

## Tests

```sh
ctest --test-dir build --output-on-failure
```

- `origami` (`OrigamiTests`): the DSP core without JUCE. Covers true bypass, latency, stereo link, the exact emphasis inverse, level, symmetry, VC, aliasing at 1x and 2x, and NaN/denormal hygiene.
- `origami_plugin` (`OrigamiPluginTests`): the processor and editor without a host. Covers the standalone normals, latency reporting, the state format and its round trip, and the editor's sizes and controls. Run `build/OrigamiPluginTests_artefacts/Release/OrigamiPluginTests <folder>` to also write panel screenshots.

## Jidai Cable Standard v1.1

ORIGAMI follows the Jidai Cable Standard v1.1 through the shared `jidai-common` 1.1.1 headers, vendored verbatim in `third_party/jidai-common`. `VENDOR.md` there records the source commit.

- **Levels:** audio is ±5 V. Host floats are scaled ×5 in and ÷5 out, which is exact. VCA CV is 0..5 V, and the R15 OVER LED marks over-range signals.
- **Pitch:** ORIGAMI has no pitch jacks. Its jacks are audio and CV only. Any pitch in a Jidai patch follows the shared 1 V/oct law (C3 = 130.8127826502993 Hz at 0 V), and ORIGAMI does not convert pitch.
- **Jacks:** ids are `ORIGAMI#N/SECTION:LABEL` (R6), and colors follow the R14 roles.
- **Latency:** QUALITY 1x adds 0 samples. QUALITY 2x adds 46 samples (93-tap halfband, 23 samples up and 23 down), which ORIGAMI reports to the host. The dry path is delayed to match (R11).
- **State:** format 1, unit `ORIGAMI`. Parameter ids are never reused (R7).

## Jidai Rack integration

The Jidai Rack ([jidai-collection](https://github.com/martialsystems/jidai-collection)) pins this repository by commit through CMake `FetchContent`:

```cmake
FetchContent_Declare(origami GIT_REPOSITORY https://github.com/martialsystems/origami.git GIT_TAG <commit>)
FetchContent_MakeAvailable(origami)
target_link_libraries(<core> PUBLIC origami::dsp)    # include "origami/dsp/OrigamiCore.h"
# compile ${ORIGAMI_UI_SOURCES} into the host's JUCE targets  # include "origami/plugin/OrigamiPanel.h"
```

If the host already defines `jidai::common`, ORIGAMI uses that target instead of its vendored copy, so the host binary has a single jidai-common include path. The rack's `OrigamiDevice` runs the same `OrigamiCore` and reads the same state, so a plugin preset loads in the rack and a rack preset loads in the plugin.

Its jacks in the rack:
- **Audio in:** IN L and IN R. IN R is normalled to IN L.
- **CV in:** VCA CV, and VC 1, VC 2 and VC 3.
- **Audio out:** OUT L and OUT R.
- **Back panel:** HOST IN and HOST OUT pairs.

## Legal

Copyright © 2026 Martial Systems LLC. All rights reserved. See [LICENSE](LICENSE). ORIGAMI is an original Martial Systems design.
