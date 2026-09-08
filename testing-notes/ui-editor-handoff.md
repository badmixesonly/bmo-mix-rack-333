# UI editor — handoff

What the `ui-editor` branch did, why each thing is the way it is, what was
tried and thrown away, and what is still open. Written for someone picking
this up cold.

Twenty-three commits, no parameter, spec, preset or DSP file touched by any of
them. All nine ctest suites pass at every commit. If you change anything here
and a DSP test moves, something has gone wrong that this branch was not
supposed to be able to do.

---

## 1. The build, and two traps

Both former blockers are gone: cmake 4.4.3 is on the machine PATH, the
licensed faces resolve from outside the repository, and `tools/snapshot`
renders headlessly on Windows.

    cmake -S . -B build
    cmake --build build --config Debug --parallel
    ./build/tools/Debug/snapshot.exe eq   snapshots/eq.png
    ./build/tools/Debug/snapshot.exe rack snapshots/rack.png chain=util,eq,sat,opto

A correct configure prints the font folder it chose:

    -- BMO fonts: C:/Users/stefr/Documents/FONTS (.bmo-fontdir)

**`.bmo-fontdir` does not travel.** It is a working-tree file and is not in
git, so a fresh clone or a second worktree does not get it and the build stops
with a fatal error naming a missing `.otf`. Fix per working copy with
`scripts/set-font-dir.sh`, or machine-wide with `setx BMO_FONT_DIR`.

**`scripts/build.sh` works on Windows now**, so the four lines above are only
what you type when you want one render rather than all of them:

    scripts/build.sh --snapshots

It was broken in three places, not the one this file used to name. It assumed a
single-config generator throughout: it left the configuration off the build,
left `-C` off `ctest` — which finds no tests at all on a multi-config build —
and then looked for the snapshot tool at `build/tools/snapshot`, one directory
above where Visual Studio puts it. It now reads `CMAKE_CONFIGURATION_TYPES` out
of the cache and adapts, and searches for the tool rather than assuming a path.

`snapshots/` is gitignored, so renders never travel with a branch. Re-run the
tool.

---

## 2. Read these two first

- **`Palette Book/palette-book.md`** — every colour in the suite measured on
  the ground it is actually drawn on, the reasoning behind the light and dark
  sets, and the dead ends. It is the design record; this file is the change
  record.
- **`docs/ui-workflow-brief.md`** — cherry-picked onto this branch from
  `ui-workflow-brief`. Its premise about there being no local toolchain is now
  stale, but its proposals are not, and its house rule is the one that matters
  below.

**Assert absolutes, not comparisons.** That rule came out of a DSP test that
passed for a whole release while both modes were broken, because it only
compared them to each other. "Better contrast than before" is the same trap.
Every ratio in this branch is a fixed number against a named ground.

---

## 3. What changed, and why

### The token split

`pointer` was `#ffffff` doing five jobs. They stopped agreeing the moment the
plate was allowed to go dark, and three were already wrong on the pale one. It
is now `pointer`, `ringFace` and `meterInk`, plus two *derivations* rather than
colours:

- `accentTextOn (colour, ground)` — steps a colour away from a stated ground
  until it clears a ratio. Used where the ground is known and is not the plate:
  a polarity switch's white fill, the ring a band's marker sits on, the meter
  face.
- `accentInk (accent)` — a module's colour as ink **on the plate**. Paired with
  `faceOf`, and the two trade places between appearances; see below.
- `onAccentOf (fill)` — ink on a filled control, derived from the fill.

Measured on the result: knob captions 1.95 → 4.57-4.69:1, section legends
1.72 → 4.57-4.69, knob pointer 1.39 → ~9.5, selected band legend 2.22 → 4.62,
text on a switch 1.98 → derived, text on `switchOff` 2.43 → 4.94.

`meterFace`, `knobTint`, `neutral` and `polarity` all became tokens during this
branch. **There is no raw hex left in any panel.**

### Dark mode

`darkTokens()` is the whole palette. A machine-wide preference in
`LT3 Audio/UI.json` chooses it, and the toggle is in the preset dropdown.

It is **not a parameter**, deliberately: `specs()` is frozen and append-only, a
parameter would be automatable, and a look does not belong in a session. The
1 Hz theme poll that already existed carries the choice to every open editor —
standalone and in a rack, this plugin and the one in the next track — within a
second, with no instance holding a reference to any other.

Structural greys are spaced by **CIE L\***, not by contrast ratio. Below about
L\* 20 the ratio is useless: from the dark plate the most contrast available by
going darker, all the way to black, is **1.29:1**. Spaced by lightness the dark
set carries the light set's own intervals within a few tenths.

The knob cap and its caption **trade places** between appearances. Light: cap
is a wash, caption is the accent stepped down. Dark: cap is the accent at full
strength, caption is the wash. Both directions measure better than they went
in, which is what makes it a swap rather than a compromise.

### BMO Opto

Was the module drifting furthest from the suite and is now the one that follows
it most closely. The panel is greyscale in both modes, so the only colour on it
means "engaged" — red in Tele, amber in Stressed, both taken from the suite's
own `meterClip` and `meterHigh` rather than invented. TELE and ELD are a
stacked pair at the head mirroring LINK/COLOR at the foot. The VU meter's arc
was sized against the wrong dimension and is fixed; the panel is laid out on
one rhythm instead of three thirds. Its accent is grey, so its header bar is
too — it is the one module with no colour of its own.

### The rules a module now inherits

In `modules/AGENTS.md`, because four call sites is how the previous ones
drifted:

| switch | colour |
|---|---|
| the module's bypass | the module's accent |
| mono | the module's accent, so it matches the header bar |
| **polarity** | **`tokens().polarity`, always** |
| anything else | `tokens().switchAlt` |

Plus the sizes — `Tokens::switchWidth/Height/Gap`, one constant each — and
`DynamicsMeter`'s contract: which colours are the module's, which are the
suite's, that gain reduction is always `>= 0`, and that it wants a landscape
box.

---

## 4. Frosty's calls, with the numbers he took them on

These trade measured contrast for the look, deliberately. They are recorded at
the call sites too. Do not "fix" them.

| | measures | instead of |
|---|---|---|
| White knob pointer, light mode | 1.39-1.49:1 | ~9.5:1 dark |
| Section legends in the raw accent | 1.72-2.00:1 light, 5.87 dark | 4.57-4.69 / 9.07 |

The legend size went to 13 pt as part of the second one — a legend set in a
colour that pale has to be big enough to survive it.

---

## 5. Tried and thrown away

Recorded so nobody re-derives them.

- **Dark knob caps by mixing the accent toward the dark plate.** Pink went to a
  clean maroon and green to a deep green; the Saturator's orange landed on a
  brown.
- **Dark caps by lifting the accent's saturation ×1.35.** Measured fine — every
  module held 4.5:1 or better cap-to-pointer — and the caps shouted. *Neither
  of these failed on numbers. Render, do not compute.*
- **Pale caps in both with only the pointer inverting.** Correct and dull; the
  module's colour never got to be the loud thing on a dark panel.
- **Outlined section legends**, white with an accent stroke. It read, but a
  label that needs an outline is a label in the wrong colour — which is why
  outlines were taken out of this suite years before. The rule in
  `modules/AGENTS.md` and the note in `Fonts.h` both stand.
- **Blender for the frequency legends.** Its x-height is larger, so at the same
  point size the numbers came out *bigger*. It would need about 9/8 to sit
  where Minerva does at 11/10.

---

## 6. Verified, and how

Everything visual on this branch was checked against a render, not against the
source. Three times the *check* was wrong rather than the code, so:

- **Render to a unique filename.** A stale read of a path just re-rendered cost
  a full debugging cycle chasing a bug that did not exist.
- **Do not sample "the darkest pixel in a box"** to identify a colour. Two inks
  128 apart in hex can be two apart in luminance — `#497450` and `#317290`
  measure 104 and 102 — so the answer is a coin flip between a caption and its
  neighbouring rule. Count exact colour matches, or dump the computed value.
- **Check what the baseline actually is.** Two "regressions" were a baseline
  with different switch states, and a baseline that predated an intervening
  commit.

---

## 7. Still open

- **Low-cut crowding.** Parked at Frosty's request. Five legends around a 13 px
  face; the circle is even and the radius is at the cell's limit, so more room
  means a taller row, and BMO EQ has no vertical slack — measured, no empty
  band over 16 px anywhere on the panel.
- **Meter modes cannot be rendered.** `IN` and `GR` are UI state rather than
  parameters, so `tools/snapshot` can only ever show `OUT`. Every VU change on
  this branch was verified in one mode of three. This is the one verification
  hole left, and `docs/ui-workflow-brief.md` §2 is the fix.
- **Section rules line up at the ends and nowhere else.** The input and output
  sections are `ui::ModulePanel`'s now — `takeInputSection` off the top,
  `takeOutputSection` off the bottom — so every panel that opts in puts its
  input knob, its bypass row and its output knob on the same lines. BMO EQ and
  the Saturator take both; Util takes neither but keeps the reservation, which
  is what puts its lower rule on their line. What is still per-panel is
  everything between: the rules inside a module's own middle sit where that
  module's rows put them. Frosty's call was "some should, some should not", so
  that half wants a module-by-module pass rather than another shared constant.
- **Contrast and text-fit assertions.** Both bug classes that shipped in 0.2.1
  are pure functions of the tokens and a `resized()`, and neither is tested.
  `docs/ui-workflow-brief.md` §4.

---

*Branch `ui-editor`, 25 commits on top of `main` at 6fdf8d9.*
