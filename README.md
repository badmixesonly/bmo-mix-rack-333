# BMO Tune RT

**BMO Tune RT** is a low-latency monophonic vocal pitch corrector by **LT3a**
(LT3 Audio), part of the BMO (Bad Mixes Only) family. It is built around hard
tuning and retune speed: the snap the genre is made of, at the lowest latency
a software-only tuner can honestly claim.

It has a repository of its own and is not a BMO Mix Rack module -- it is
mono, and has a latency contract a rack slot is not built around -- but it
follows every suite convention and reuses the shared code from Kevin's
main, pinned as a submodule. It ships standalone for now; bringing it into
the rack later is the hope, not a requirement (Frosty, 2026-09-11), and the
schema still fits a rack slot's 32 parameters so that door stays open.

One engine, CLASSIC: a cycle-repeat/delete rate converter, bright, with
formants that move with the pitch, 0.4 ms behind at rest and reporting 0 to
the host. A second engine (HYBRID, PSOLA) and a fixed-latency Studio mode
were built and measured, and set aside on 2026-09-11 when CLASSIC sounded
better in Ableton -- `testing-notes/nrt-tune-handoff-2026-09-11.md` keeps
them for a possible non-real-time tuner, and branch `archive/hybrid-studio`
keeps the code.

No FFT anywhere in the correction path. The design document is
`bmo-tune-rt-implementation-and-test-spec.md` (v0.1, 2026-09-10); where this
code departs from it, `AGENTS.md` says where and why.

## Building

```
git submodule update --init libs/bmo-mix-rack     # Kevin's main
scripts/build.sh                                  # DSP, tests, tools: no JUCE
scripts/build.sh --plugin                         # + VST3, Standalone, panel snapshots
```

Requirements: CMake 3.22+, a C++20 compiler (MSVC 2022 / Xcode 15 / GCC 12),
and the pinned BMO Mix Rack submodule. The DSP, its tests and every tool
build with no framework at all.

The plugin also needs JUCE -- the rack's own submodule at
`libs/bmo-mix-rack/libs/JUCE`, cloned from a local rack working copy or from
GitHub (CMake prints both commands) -- and the licensed fonts, whose folder
goes in a gitignored `.bmo-fontdir`. The VST3 lands in
`build-plugin/products/tune/BmoTuneRT_artefacts/Release/VST3/` and is never
copied into the system plugin folder by the build.

## Status

The DSP is complete and measured, and the plugin builds: VST3 and Standalone
on the suite's own `SingleModuleProcessor`. 0.1 was heard in Ableton on
2026-09-11 -- it works, with some hiccups still to be pinned down -- and the
product went CLASSIC and Live only. Every figure the spec gates on that can be measured
offline meets its gate -- `modules/tune/AGENTS.md` has the table, and
`testing-notes/` the state of play.

Against Antares Auto-Tune Artist and Waves Tune Real-Time (2026-09-11, on
AURORA): the lowest true latency of the three (3.8 ms worst, against 6.5 and
10.6), level with them at 10 and 20 ms retune, and behind Antares at hard
tune on a moving voice -- its correction lands about a cycle late. That is
the open work: `tests/dsp/HardTuneTests.cpp` holds it, and
`testing-notes/latency-and-lag-2026-09-11.md` has the numbers.

## Layout

```
modules/tune/   params and the JUCE-free DSP (bmo::tune); its AGENTS.md and README.md
tools/          the offline harness, one executable each:
                  bmo-tune-cli      WAV in, WAV out, --dump-analysis CSV
                  bmo-tune-gen      the synthetic corpus, with exact ground truth
                  bmo-tune-score    GPE / FPE / RPA / RCA / VDE / time to lock
                  bmo-tune-latency  the manual's latency table, measured
                  bmo-tune-bench    CPU per block: median, p99, max
                  bmo-tune-ref      the reference stimulus, and any tuner's render of it scored
                  bmo-tune-blind    a blind listening set from several tuners' renders
                  bmo-tune-snapshot the panel to a PNG, no display needed
                  bmo-tune-hostcheck loads the built VST3 as a host would
                  bmo-tune-hostrender renders a WAV through any VST3, uncompensated
tests/          seven DSP suites, the panel test and the host check, by CTest
                (and hardtune_target, disabled until it passes)
products/tune/  the plugin target: Btun, com.lt3audio.bmotunert
design/         the panel studies the panel was built from
scripts/        build.sh, score-corpus.sh
testing-notes/  handoffs
```

## Hearing it

```
build/tools/Release/bmo-tune-cli in.wav out.wav --set retune_ms=0 --set scale=Minor --set key=A
build/tools/Release/bmo-tune-cli --list        # every parameter, its range and default
```

## Licence

GNU AGPL v3, see `LICENSE` -- the same file BMO Mix Rack carries. The
typefaces are not part of the licence and are not in the repository.
