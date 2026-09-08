# UI editor — handoff

What the `ui-editor` branch did, why each thing is the way it is, what was
tried and thrown away, and what is still open. Written for someone picking
this up cold.

Thirty-three commits, no parameter, spec, preset or DSP file touched by any of
them. All ctest suites pass at every commit -- nine of them until 8 Sep, ten
since `ui_layout` joined them. If you change anything here and a DSP test
moves, something has gone wrong that this branch was not supposed to be able
to do.

---

## 1. The build, and the one trap left

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

0.2.3 added five more, each of which caught something that eyeballing a zoom
had missed:

- **Scan a line, do not squint at a crop.** Dump a run-length of one row or
  column straight through the thing you are measuring — start, end, width,
  exact hex. It is how the dark selector ring was found: at a glance it looked
  like a ring, and the scanline showed `#8d8d98`, `#8e8e93`, `#8d8d98` running
  together as one 21 px slab, because the hairlines and the face were the same
  colour to within 1.01:1. No amount of looking at it would have produced that
  number. Same tool proved three panels' rules land on the same two pixel rows.
- **Hash every render before and after anything that should move nothing.** A
  refactor that claims to be pure is a claim you can settle rather than argue:
  render all five panels, refactor, render again, compare hashes. Byte-identical
  or it was not a refactor.
- **Both appearances, every time.** Half the faults on this branch existed in
  one only. A change measured on the pale plate has not been checked.
- **Check the opening state specifically.** Two of the worst faults were only
  visible in Init — the high shelf's 12 kHz default put the gain's rest dot on
  the band marker, and BMO Opto's COLOR read *off* while the DSP held it on.
  Both are the first thing anyone sees and neither showed at any other setting.
- **A ratio without a named ground is not a measurement.** Say what it is
  against. "The marker is 4.67:1" was true and useless; against `ringFace`
  rather than the plate it was the whole story.

The helpers used for all of this were throwaway PowerShell in a scratchpad and
did not survive the session. **They are now `tools/inspect/` and do survive** —
`scan`, `hist`, `crop` and `sheet`, plus `hash` and `ratio`, which the list
above named as practices without a tool behind them. C# over `System.Drawing`, `csc`-built
from one file, outside CMake for the reason `tools/measure/renders` is.

Both findings above were re-derived with it as the check that it works: column
260 of `ring-dark.png` scans as a 22 px slab, and `#497450` against `#317290`
comes back 1.01:1 at 0.3 L\* apart. Read `tools/inspect/README.md` before
reaching for a screenshot and a squint.

---

## 7. Still open

- **~~Two loose `.patch` files in the repository root.~~ Settled 8 Sep: spent,
  deleted.** Recorded here because they were untracked, so the deletion leaves
  no trace in git and this note is the only record of it.

  Settled by content, not by the subject match. Below the header — dropping the
  hash, author and date lines, which are expected to differ —
  `0001` against `15d4808` and `0002` against `2cacb0a` each differed in exactly
  two lines:

      Subject: [PATCH 1/2] ...   vs   Subject: [PATCH] ...
      2.43.0                     vs   2.55.0.windows.5

  The first is an artifact of the check itself: the loose files were generated
  as a two-patch series, and `git format-patch -1` on a single commit numbers it
  `[PATCH]`. Not a content difference.

  The second is the answer to what happened. That trailer is the version of git
  that *wrote* the patch, and `2.43.0` is not this machine's git. The two files
  were generated somewhere else, carried here, and applied — which is why the
  commits exist with the right subjects and the wrong hashes, and why
  `c1ab3222` and `ed5a9569` are not objects in this repository.

  Every diff line was identical. Nothing existed only in those files.

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
  `docs/ui-workflow-brief.md` §4, and §8 below for how to build the harness
  they both need.

---

## 8. What is left

`tests/` had nine suites and not one of them touched the UI. 8a below is now
built and is the tenth; 8b is the piece still open.

### 8a. A UI test harness, and layout assertions on it — **done, 8 Sep**

`tests/ui/LayoutTests.cpp`, wired in as `ui_layout`. A `bmo_add_tool` suite,
not `bmo_add_dsp_tool`: nothing in it renders, but `Tokens.h` includes
`juce_gui_basics`, so `docs/ui-workflow-brief.md` §4 is wrong that this could
run in the DSP-only job. No processor gymnastics were needed — a panel is laid
out by its editor's constructor at design size whatever the editor is scaled
to afterwards, so constructing the editor and walking it is enough.

What it pins, all as absolute rows:

- **The shared ends.** Input knob 4..81 and a rule centred on 90; a rule
  centred on 566, switches 574..601, output knob 602..679. EQ and the
  Saturator take both sections. Util reserves the output one and adopts
  neither half, and is asserted to have no OUTPUT knob — it is the case that
  proves a reservation is worth anything.
- **BMO EQ's band column**, row by row: 98, 226 and 354 at 112 tall, the low
  cut at 482, and the column ending flush on 558. This is the one that
  matters. Both sections come off the two ends *before* the bands get what is
  left, so a one-pixel change to `kBandRow` moves every row below it while the
  input knob, the output knob, the switch row and both shared rules stay
  exactly where they were.
- Nothing escapes its panel, no two controls overlap, every caption fits.

**Both new assertion classes have been seen to fail**, which was the condition:

    kBandRow 112 -> 111
      FAIL: eq MID band top -- expected 226, got 225
      FAIL: eq band column should end flush against the output rule,
            and its foot -- expected 558, got 555
    MAKEUP -> MAKEUPMAKEUP
      FAIL: opto caption 'MAKEUPMAKEUP' overflows its box by 66.9 px

Three things went in to make it possible, all UI-side and all pixel-neutral:

- **`ModulePanel` owns the section rules now.** EQ, the Saturator and Util
  each had a private `struct Rule`, a private vector and a byte-identical
  `paintPanel`. `getRules()` is public because a rule is *painted* rather than
  placed, so it is the one thing on a panel with no bounds a test can read —
  and the rules are exactly what the panels are supposed to agree about.
  `paintRules` is out of line in a new `ModulePanel.cpp`, because `ModuleDef.h`
  includes `ModulePanel.h` and the header can therefore only forward-declare
  `ModuleDef`.
- **Controls name themselves** after the caption a reader sees, so the test
  finds OUTPUT by the word printed on the panel. BMO EQ names its four bands
  after the rules they sit under, being the only controls there with no
  caption of their own.
- **`PlainKnob::captionOverflow`**, with the caption box factored out so
  `paint` and the assertion read the same box in the same font. A fit test
  that measured it its own way could have agreed with the bug it exists to
  catch.

`ui_layout_tests --dump` prints every panel's controls and rules. Use it: the
numbers above were read off the panels, not derived from the constants and
then asserted against the derivation.

**What it does not cover.** Switch label fit (only knob captions are measured),
the rack's own composition, contrast ratios, and BMO Opto's meter modes, which
cannot be laid out differently because they are not parameters — that is 8b.

### 8b. Meter-mode injection

`DynamicsMeter::Mode` is UI state set through `setMode`, not a parameter — and
rightly, since `specs()` is frozen and append-only and a meter mode does not
belong in a session. But it means `tools/snapshot` can only ever render `OUT`,
so every VU change on this branch was verified in one mode of three, including
0.2.3's resizing of the IN/GR/OUT row itself.

The route is a virtual on `ui::ModulePanel` — `setUiState(key, value)`,
returning false for anything it does not know — overridden by `OptoPanel` to
accept `meter=IN|GR|OUT`, and an arg in `tools/snapshot/main.cpp` that routes
`ui.<key>=<value>` to it. Roughly forty lines and it touches no DSP, no
parameter and no panel geometry.

Make it refuse what it does not understand rather than ignoring it. The tool
already learned this once: a mistyped choice name used to come back 0.0 from
`getFloatValue()` and render a plausible panel of entirely the wrong thing.
`realValueFor` now refuses. `setUiState` should too.

**Done looks like:** three renders of BMO Opto that differ, and the GR one
showing a needle on a 0..24 dB scale rather than a VU one.

---

*Branch `ui-editor`, 33 commits on top of `main` at 6fdf8d9.*
