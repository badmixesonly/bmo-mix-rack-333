# Working in this repository

This is the BMO plugin suite by LT3a: three modules (EQ, Saturator, Util),
one rack, and the shared code under `core/`. Read this first; then the
`AGENTS.md` in whichever of `core/`, `modules/`, `products/` you are touching.

## Which machine you are on

Frosty works on two Windows machines:

| name | machine | user folder |
|---|---|---|
| **ICE QUEEN** | desktop | `C:\Users\stefr` |
| **AURORA** | laptop | `C:\Users\thesp` |

Windows reports the same computer name on both, so the hostname is no help.
The user folder in your working path tells you which one you are on. Each
machine also names itself in its own user-level `~\.claude\CLAUDE.md`, which
is machine-local and never committed. ICE QUEEN's was set up on 2026-09-10.

**On AURORA, if `C:\Users\thesp\.claude\CLAUDE.md` does not name the machine,
create it.** Confirm with Frosty that this is the laptop before writing it.
Use this, which mirrors ICE QUEEN's:

```markdown
# This machine: AURORA

This is **AURORA**, Frosty's **laptop**, user folder `C:\Users\thesp`.
Frosty's desktop is **ICE QUEEN**, user folder `C:\Users\stefr`. Windows
reports the same computer name on both, so the user folder and this file are
what tell them apart. This file is machine-local on purpose: do not copy it to
ICE QUEEN, and do not commit it anywhere.

- **Say where work happened.** When you record where something was done — a
  build, a test, a measurement, an install, a listening result — in a handoff
  doc, `testing-notes/`, a PR description, or a summary to Frosty, name the
  machine: "on AURORA". Name ICE QUEEN only when Frosty says the work happened
  there.
- **Never infer ICE QUEEN from this disk.** Installed plugins, build trees and
  local files here are AURORA's alone. If it matters what ICE QUEEN has, ask.
```

**Why it matters:** a record of where something happened is only useful if it
names the machine. On 2026-09-10 a session on ICE QUEEN was wrongly taken to be
on the laptop. For a while it recorded its own measurements, and Frosty's host
tests, against the wrong machine — and discounted the installed plugin that
showed which build had been tested. When you write down where a build, test,
install or listening result happened, in `testing-notes/`, a handoff or a PR,
name the machine. Where it matters which build was heard, check the installed
binary on that machine by date and SHA-256.

## What must not change

A saved session references these, so they are permanent once shipped:

- Parameter IDs, their **order** in `specs()`, ranges, steps and defaults.
- Plugin codes (`Fsty`, `Bsat`, `Butl`, `Brck`), bundle IDs
  (`com.lt3audio.*`), the manufacturer code `LT3a`, and product names.
- Module ids (`eq`, `sat`, `util`) and the state tags `PARAMS`, `RACK`, `SLOT`.
- The rack grid: 8 slots x 32 parameters, spec index `i` on `slotN_p(i+1)`.
  That is a count of host lanes, not a cap on a module: one with more than
  32 parameters keeps the rest off the grid (`core/rack/SlotOverflow.h`),
  so they cannot be automated in a rack.

`tests/plugin/*Tests.cpp` write all of this out and fail on drift. If a
change is genuinely wanted, the test is where the decision gets recorded --
add a parameter at the **end** of `specs()`, never in the middle.

## The latency rule (BMO Tune RT)

**BMO Tune RT's true latency may never exceed Waves Tune Real-Time's,
measured the same way on the same stimulus** (Frosty, 2026-09-11). Waves'
figure is **10.62 ms**, measured on AURORA and recorded with its settings in
`tools/tune/common/References.h`. True latency is how late the audio really
is, not what the plugin reports -- both report 0. Within the ceiling a change
may make the audio later without asking; say the new figure and the headroom
left in the commit body. Re-measure the ceiling when Waves updates;
`testing-notes/latency-and-lag-2026-09-11.md` has the commands.

`modules/tune/AGENTS.md` and `modules/tune/README.md` cite this rule to this
file, and until 2026-09-11 it was not written down anywhere but in the notes.

**It is broken today**, at the bottom of the range: 54 cells of
`bmo-tune-latency`'s table are over the ceiling, worst 15.3 ms at E2. Both
gates missed it -- see `testing-notes/tune-latency-review-2026-09-11.md`.
Two further things the rule needs settled, which that note argues:

- **The ceiling is a scalar, but what it bounds is pitch-dependent for every
  tuner in the comparison.** Waves' own delay is one period plus ~1.26 ms;
  10.62 ms is that evaluated at A2, the lowest note the stimulus holds. A
  change that beats Waves at every note can still fail a scalar taken at one.
- **Measure the worst, not a mean.** The stimulus figure is a correlation
  peak over a held note; the per-semitone sweep is the worst.

## How the pieces fit

- `core/dsp` is JUCE-free and must stay so: `BMO_DSP_ONLY=ON` builds the DSP
  tests and measurement tools with no framework.
- A module = `params.h` (specs + `enum Index`), `dsp/` (a `ModuleDsp`),
  `panel/` (a `ModulePanel`), `presets/FactoryPresets.h`, and `Module.cpp`
  which exposes a `ModuleDef`. The standalone product and the rack both
  drive the same `ModuleDef`; there is no rack-specific version of anything.
- `ParamSet` is the one way DSP and panels read parameters, so a panel
  works over an APVTS parameter standalone and over a `SlotParameter` in
  the rack without knowing which.
- Panels are laid out at design size (width per module, height 688 under a
  28 px header and 24 px preset/slot bar = 740) and scaled as a whole.

## Before you say it is done

```
scripts/build.sh              # builds everything and runs ctest
scripts/build.sh --snapshots  # then look at snapshots/*.png
```

Both must pass. If a panel changed, look at the snapshot. If DSP changed,
`build/tools/measure_<module>` prints the curves; the DSP tests say what
the numbers are supposed to be.

## Documenting new work

When you add a new module, or any directory that carries its own context
future contributors will need (e.g. a `presets/` folder, a new `core/`
subsystem), create an `AGENTS.md` there explaining what lives in it, why,
and anything a future AI contributor would otherwise have to re-derive.
Add a `README.md` alongside it for human-facing context. Link the new
`AGENTS.md` from its parent so the reading chain in the first paragraph
above stays unbroken.

## Conventions

- Root is the include root: `#include "core/state/ParamSet.h"`.
- Comments explain *why*, in full sentences; the code says what.
- Names are `bmo::` for shared code, `bmo::<module>` per module,
  `bmo::ui` for panels and controls, `bmo::products` for the thin wrappers.
- The fonts in `assets/fonts` are licensed and gitignored; never commit
  them, never look a face up by name at runtime.
- `plans/` and `packages/` are gitignored working folders.
