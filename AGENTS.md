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
  so (spec §8). `TuneCore` is float in, float out, a parameter struct and MIDI
  notes; `TuneDsp` puts it behind the rack's `ModuleDsp` so a wrapper built on
  the rack's `core/product` drives it like any module.
- The rack's `core/dsp/ModuleDsp.h` and `core/state/ParamSpec.h` are used
  **from a BMO Mix Rack checkout**, not copied: `BMO_RACK_DIR`, default
  `../bmo-mix-rack-333`. When this repository gets a remote, that becomes a
  submodule under `libs/`. Do not fork those headers into this tree.
- Two include roots that never collide: this repository owns `modules/tune`,
  `tools/` and `tests/`; the rack owns `core/`.
- `tools/` are the spec's offline harness (T-1, T-3, T-5, T-11). Every one
  drives the real DSP library; none has its own copy of any of it.

## What must not change (from 0.1 on)

Nothing is frozen yet. From the first saved session, the same list as the
rack's: parameter ids, their order in `specs()`, ranges, steps, defaults.
`tests/dsp/SchemaTests.cpp` writes the table out in full; a change is argued
for there, and a new parameter goes at the **end** of `specs()`.

Not yet allocated, and to be allocated in the rack's `products/AGENTS.md`
identity table when a plugin is built: plugin code, bundle id
(`com.lt3audio.bmotunert` is the obvious one), preset extension, accent.

## Before you say it is done

```
scripts/build.sh              # Release build, all eight test suites
scripts/build.sh --corpus     # then generate, render and score the corpus
```

Both must pass. If the DSP changed, also run
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
