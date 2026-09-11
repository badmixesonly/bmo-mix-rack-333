# Working in this repository

BMO Tune RT, a low-latency monophonic pitch corrector by LT3a. It is not a
BMO Mix Rack module, but it is built the rack's way and reuses the rack's
shared code, so read the rack's root `AGENTS.md` for the conventions this
repository inherits. Then [`modules/tune/AGENTS.md`](modules/tune/AGENTS.md),
which is where everything specific to the DSP lives.

The design document is `bmo-tune-rt-implementation-and-test-spec.md` (v0.1,
2026-09-10), which is not in this repository. Where the code departs from it,
`modules/tune/AGENTS.md` says where, why, and what was measured.

## Which machine you are on

Frosty works on two Windows machines, and the rack's `AGENTS.md` explains
how to tell them apart: **AURORA** (laptop, `C:\Users\thesp`) and **ICE QUEEN**
(desktop, `C:\Users\stefr`). This repository was started on AURORA on
2026-09-10. Name the machine in anything that records where a build, test,
measurement or listening result happened.

## How the pieces fit

- `modules/tune/dsp` is the whole product's sound, JUCE-free, and must stay
  so (spec §8). `TuneCore` is float in, float out and a parameter struct --
  the voice is the only input: no MIDI, no sidechain (Frosty, 2026-09-10).
  `TuneDsp` puts it behind the rack's `ModuleDsp`, which is all the rack's
  `SingleModuleProcessor` needs to host it, so the wrapper is a product built
  on Kevin's `core/product` like every other BMO product.
- Shared code comes from **Kevin's main, pinned as a submodule** at
  `libs/bmo-mix-rack` (`git submodule update --init libs/bmo-mix-rack`; not
  recursive until the wrapper needs JUCE). Today that is
  `core/dsp/ModuleDsp.h` and `core/state/ParamSpec.h`; the wrapper will add
  `core/product`, `core/state` and `core/ui`. Do not fork any of it into this
  tree, and do not point the build at a working copy of the rack -- a working
  copy is on whatever branch someone left it on. Move the pin forward with
  `git -C libs/bmo-mix-rack pull origin main` and commit the new pointer.
- Two include roots that never collide: this repository owns `modules/tune`,
  `tools/` and `tests/`; the rack owns `core/`.
- `tools/` are the spec's offline harness (T-1, T-3, T-5, T-11). Every one
  drives the real DSP library; none has its own copy of any of it.

## What must not change (from 0.1 on)

**Frozen from the first plugin build (Frosty, 2026-09-10)** -- the same list
as the rack's: parameter ids, their order in `specs()`, kinds, ranges, steps,
defaults, and every choice's names and order. `tests/dsp/SchemaTests.cpp`
writes the table out in full; a change is argued for there, and a new
parameter goes at the **end** of `specs()`. Plugin code `Btun`, bundle id
`com.lt3audio.bmotunert` and manufacturer `LT3a` are equally permanent: they
are what a host finds the plugin by.

## Identity and colour

A standalone product, so its identity lives here rather than in the rack's
`products/AGENTS.md` table, and its colours are chosen fresh rather than from
the rack's reserved list (Frosty, 2026-09-10). The suite's colour *rules* still
apply unchanged: `faceOf`, `accentInk`, `accentTextOn`, `onAccentOf`, the
switch-colour table, and both appearances checked every time.

| | |
|---|---|
| Accent | lime `#b6e35d`, derived by the suite's rules on both plates |
| Selectors | lit in the accent (Scale, Range, Latency, Formant, the mode banner) |
| Keyboard | lime, the accent, for now. Complements of lime (violet `#8f7cf8`, periwinkle `#6f8ef5`, orchid `#b56cf0`) were tried and rejected: good on the dark plate, not on the light one (Frosty, 2026-09-10) |
| Lower section | a clock around a centred Retune: Vibrato and Flex at 10 and 2 o'clock; on HYBRID, Glide and Shift at 8 and 4, Formant Keep/Follow under Retune between them. Hidden controls leave their places empty (panel studies, round 7) |
| Plugin code, bundle id, preset extension | `Btun`, `com.lt3audio.bmotunert`, `.bmotune` (Frosty, 2026-09-10). Not yet in the rack's `products/AGENTS.md` allocation table -- that is Kevin's repository, so it goes in with his say |

## Before you say it is done

```
scripts/build.sh              # Release build, all nine DSP suites
scripts/build.sh --plugin     # the plugin too: eleven suites, and snapshots/
scripts/build.sh --corpus     # then generate, render and score the corpus
```

All three must pass. If the panel changed, look at `snapshots/` in both
modes and both appearances -- the panel test checks the hide rule, fit and
overlap, but not whether it looks right. If the DSP changed, also run
`build/tools/Release/bmo-tune-latency --range all` (it fails if Studio's
measured delay ever leaves its reported PDC) and `bmo-tune-bench --quick`.

**Measure, never judge a render by eye or ear alone** -- the rack's session
handoff of 2026-09-09 is the reason, and every fault found in this tree so
far was found by a number, several of them in code that looked right.

## Conventions

The rack's, unchanged: the root is the include root; comments explain *why*,
in full sentences; `bmo::tune` for this product's code; tests are plain
executables with one `check` per claim, written as a sentence. Measured
numbers are printed with `report` even when the check passes, so a drift
toward failure is visible before it fails.

`corpus/`, `renders/` and `reports/` are gitignored working folders: the
generators and tools are the source of truth, and regenerate the same bytes.
