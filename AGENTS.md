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
measured the same way on the same stimulus** (Frosty, 2026-09-11). True
latency is how late the audio really is, not what the plugin reports -- both
report 0. Within the ceiling a change may make the audio later without
asking; say the new figure and the headroom left in the commit body.
Re-measure when Waves updates; `testing-notes/latency-and-lag-2026-09-11.md`
has the commands.

**The ceiling is a curve, not a number, and the comparison is per note**
(Frosty, 2026-09-11). Waves' delay while correcting is nearly proportional to
the period -- 19.2 ms at E2, 0.7 ms at A5, 1.68 ms per ms of period -- so a
single figure is only that tuner's delay at one note and says nothing about
any other. `references::ceilingMsAt (hz)` reads it off the measured curve in
`tools/tune/common/References.h`; hold changes to that. Before 2026-09-11 the
rule was the scalar 10.62 ms, which is Waves at A2, and it got the answer
wrong in both directions.

**The curve stops at E2, because the product does** (Frosty, 2026-09-11:
"it's a vocal tuner so no need to drop below E2"). Nothing below E2 is
measured and nothing below it is judged -- but note that Bass and Instrument
still declare a 55 Hz floor in `params.h`, so either those ranges come up or
the curve goes down; until one of those happens, their bottom two and a half
tones are unjudged.

**What the rule is really protecting is live monitoring**, and Waves is the
proxy for it, not the point (Frosty, 2026-09-11: per note is preferred, "so
long as it remains fast enough for live monitor we can adjust"). So a change
that is later than Waves at some note, but still comfortably inside what a
singer monitoring through the plugin can work with, is arguable rather than
forbidden -- argue it with a figure and Frosty's ear, and write the budget
down here when there is one. No budget is encoded today, deliberately: the
only thing a number would do right now is turn a red test green without
changing the plugin.

`modules/tune/AGENTS.md` and `modules/tune/README.md` cite this rule to this
file, and until 2026-09-11 it was not written down anywhere but in the notes.

**It is broken today**, at the top of the range. BMO's rest is a constant
4 ms where Waves' delay tracks the note, so the two cross at about C3: BMO is
under Waves below it and over it above, by 3.90 ms at A5.
`tests/dsp/tune/HardTuneTests.cpp` fails on it per note; the worst-against-
worst form still passes and is kept as necessary but not sufficient. See
`testing-notes/tune-latency-review-2026-09-11.md`, which also leaves two
judgements open for Frosty: whether the rule is meant per note or worst
against worst, and what the curve should do outside the notes it was measured
at.

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
