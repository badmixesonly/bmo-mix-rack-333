# Working in this repository

This is the BMO plugin suite by LT3a: three modules (EQ, Saturator, Util),
one rack, and the shared code under `core/`. Read this first; then the
`AGENTS.md` in whichever of `core/`, `modules/`, `products/` you are touching.

## Which machine you are on

Frosty works on two Windows machines:

| name | machine |
|---|---|
| **AURORA** | laptop |
| **ICE QUEEN** | desktop |

They have the **same user folder and repo path**
(`C:\Users\stefr\bmo-mix-rack-333`), and Windows reports the same computer
name on both. Nothing in a path or a hostname tells them apart. So each
machine names itself in its own user-level `C:\Users\stefr\.claude\CLAUDE.md`,
which is machine-local and never committed. AURORA's was set up on
2026-09-10.

**On ICE QUEEN, if that file does not name the machine, create it.** Confirm
with Frosty that this is the desktop before writing it; do not decide from
the paths. Use this, which mirrors AURORA's:

```markdown
# This machine: ICE QUEEN

This is **ICE QUEEN**, Frosty's **desktop**. Frosty's laptop is **AURORA**.

The two machines have the same user folder (`C:\Users\stefr`), the same repo
paths, and Windows reports the same computer name on both. Nothing in a path or
a hostname tells them apart; this file is the only thing that does. It is
machine-local on purpose: do not copy it to AURORA, and do not commit it
anywhere.

- **Say where work happened.** When you record where something was done — a
  build, a test, a measurement, an install, a listening result — in a handoff
  doc, `testing-notes/`, a PR description, or a summary to Frosty, name the
  machine: "on ICE QUEEN". Name AURORA only when Frosty says the work happened
  there.
- **Never infer AURORA from this disk.** Installed plugins, build trees and
  local files here are ICE QUEEN's alone. If it matters what AURORA has, ask.
```

**Why it matters:** a record of where something happened is only useful if it
names the machine. On 2026-09-10 a session on AURORA read AURORA's installed
VST3 — a pre-review build — as evidence of what Frosty had heard in host tests
on ICE QUEEN, and nearly recorded a pass against the wrong build. When you
write down where a build, test, install or listening result happened, in
`testing-notes/`, a handoff or a PR, name the machine.

## What must not change

A saved session references these, so they are permanent once shipped:

- Parameter IDs, their **order** in `specs()`, ranges, steps and defaults.
- Plugin codes (`Fsty`, `Bsat`, `Butl`, `Brck`), bundle IDs
  (`com.lt3audio.*`), the manufacturer code `LT3a`, and product names.
- Module ids (`eq`, `sat`, `util`) and the state tags `PARAMS`, `RACK`, `SLOT`.
- The rack grid: 8 slots x 32 parameters, spec index `i` on `slotN_p(i+1)`.

`tests/plugin/*Tests.cpp` write all of this out and fail on drift. If a
change is genuinely wanted, the test is where the decision gets recorded --
add a parameter at the **end** of `specs()`, never in the middle.

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
