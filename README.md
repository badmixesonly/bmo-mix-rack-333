# BMO Tune RT

**BMO Tune RT** is a low-latency monophonic vocal pitch corrector by **LT3a**
(LT3 Audio), part of the BMO (Bad Mixes Only) family. It is built around hard
tuning and retune speed: the snap the genre is made of, at the lowest latency
a software-only tuner can honestly claim.

It is not a BMO Mix Rack module -- it takes MIDI, is mono, and has a latency
contract a rack slot cannot carry -- but it is built the same way and reuses
the suite's shared code from a BMO Mix Rack checkout.

Two engines share one detector:

| Engine | Method | Sound | Latency |
|---|---|---|---|
| **CLASSIC** | cycle-repeat/delete rate converter | bright, formants move with pitch | lowest |
| **HYBRID** | pitch-synchronous overlap-add + LPC formant preservation | natural, formants stay put | about a period more |

No FFT anywhere in the correction path. The design document is
`bmo-tune-rt-implementation-and-test-spec.md` (v0.1, 2026-09-10); where this
code departs from it, `AGENTS.md` says where and why.

## Building

```
cmake -S . -B build -DBMO_RACK_DIR=../bmo-mix-rack-333
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Requirements: CMake 3.22+, a C++20 compiler (MSVC 2022 / Xcode 15 / GCC 12),
and a BMO Mix Rack checkout beside this one (or wherever `BMO_RACK_DIR`
points). The DSP, its tests and every tool build with no framework at all.

## Status

The DSP is complete and measured; there is no plugin wrapper yet and nothing
has been heard in a DAW. Every figure the spec gates on that can be measured
offline meets its gate -- `modules/tune/AGENTS.md` has the table, and
`testing-notes/` the state of play.

## Layout

```
modules/tune/   params and the JUCE-free DSP (bmo::tune); its AGENTS.md and README.md
tools/          the offline harness, one executable each:
                  bmo-tune-cli      WAV in, WAV out, --dump-analysis CSV
                  bmo-tune-gen      the synthetic corpus, with exact ground truth
                  bmo-tune-score    GPE / FPE / RPA / RCA / VDE / time to lock
                  bmo-tune-latency  the manual's latency table, measured
                  bmo-tune-bench    CPU per block: median, p99, max
tests/          eight DSP suites, run by CTest
scripts/        build.sh, score-corpus.sh
testing-notes/  handoffs
```

## Hearing it

```
build/tools/Release/bmo-tune-cli in.wav out.wav --set retune=0 --set scale=Minor --set key=A
build/tools/Release/bmo-tune-cli --list        # every parameter, its range and default
```

## Licence

GNU AGPL v3, see `LICENSE` -- the same file BMO Mix Rack carries. The
typefaces are not part of the licence and are not in the repository.
