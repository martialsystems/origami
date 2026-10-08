# Vendored copy

- Source: `jidai-collection` repository, folder `jidai-common/`
- Branch: `redesign/jidai`
- Commit: `24ee621` ("jidai-common 1.1.1: SHOGUN + RONIN fix batch (re-vendor this commit)"), version 1.1.1
- Copied verbatim with `git archive 24ee621 jidai-common`; no local edits. Only this VENDOR.md was added.

ORIGAMI uses:
- `jidai/dsp/TripleShaper.h`: the three folding stages (first-order ADAA), the DC blocker, LEVEL COMP, and
  `planBlock`, called once per block.
- `jidai/dsp/Halfband.h` (93 taps, 23 samples each way): the 2x QUALITY up/down sampler (46 samples total).
- `jidai/jcs/Volts.h` and `jidai/jcs/State.h`: +-5 V audio scaling, the R15 OVER LED, and the state format.
- `tests/TestFft.h`: the FFT helper used by `OrigamiTests` (test code only).

When ORIGAMI is built inside a host that already defines `jidai::common` (the Jidai Rack), this copy is not added:
the host's target is used, so the host binary has a single jidai-common include path.

To update: replace this folder with a newer `git archive` of `jidai-common/`, update the commit above, rebuild,
and run `ctest` (OrigamiTests and OrigamiPluginTests). The library's own tests are off in this build
(`JIDAI_COMMON_TESTS` defaults to OFF when vendored); build the folder on its own to check a new copy.
