# UI editor — handoff

What the `ui-editor` branch did, why each thing is the way it is, what was
tried and thrown away, and what is still open. Written for someone picking
this up cold.

Twenty-three commits, no parameter, spec, preset or DSP file touched by any of
them. All nine ctest suites pass at every commit. If you change anything here
and a DSP test moves, something has gone wrong that this branch was not
supposed to be able to do.

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
did not survive the session: a crop-and-magnify, a run-length scanline for a
row and for a column, an exact-colour histogram over a box, and a side-by-side
compositor with labels. About twenty lines each over `System.Drawing`.
Rebuilding them is half an hour; committing them under `tools/` so the next
person does not is a good small first task.

---

## 7. Still open

- **FIRST: two loose `.patch` files in the repository root.** Settle these
  before anything else, because they are the only open item where the answer
  might be "something is not in git".

  `0001-Add-BMO-Opto-a-two-knob-opto-style-leveling-compress.patch` (60,738
  bytes) and `0002-opto-include-algorithm-directly-in-DspCore.h.patch` (913
  bytes), both dated 5 Sep 19:49, both untracked and both `git format-patch`
  output.

  What is known: their headers name commits `c1ab3222` and `ed5a9569`, and
  **neither hash is an object in this repository**. But commits with exactly
  those two subject lines *are* here — `15d4808 Add BMO Opto: a two-knob
  opto-style leveling compressor` and `2cacb0a opto: include <algorithm>
  directly in DspCore.h`.

  What that means is undetermined. Matching subjects with different hashes is
  what a rebase or a cherry-pick leaves behind, in which case the patches are
  spent and deleting them loses nothing. It is equally what an *earlier draft*
  looks like — work that was never applied, or was applied and then modified.
  A subject line is not evidence of content.

  **Do not delete on the subject match.** These are untracked, so a delete is
  not recoverable from git. Settle it by content:

      git format-patch -1 15d4808 --stdout > /tmp/a.patch
      diff <(sed '1,3d' 0001-*.patch) <(sed '1,3d' /tmp/a.patch)

  dropping the first three lines of each to skip the hash, author and date,
  which are expected to differ. Same for `2cacb0a` against `0002`. If both are
  identical below the header, the patches are spent — delete them and say so in
  the commit. If either differs, **do not delete it**: work out what the
  difference is and flag it, because the delta is then something that exists
  only in a loose file in a working tree.

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

## 8. The next two, in order

`tests/` has nine suites and not one of them touches the UI. Everything in §6
was done by hand. These are the two pieces that change that; do the first
before the second, because the first is what fails when something breaks and
the second only lets you look.

### 8a. A UI test harness, and layout assertions on it

**Why this one first.** 0.2.3 left three hand-matched alignments holding the
rack together: BMO Util's two gaps, and the Saturator's adoption of BMO EQ's
row heights. They are load-bearing for how a rack reads and nothing catches
them. Add a band to BMO EQ, or change `kBandRow`, and every one of them drifts
silently. That is the regression this repository is currently most exposed to.

**The harness.** New `bmo_add_tool` suite — JUCE, not `bmo_add_dsp_tool`.
Note that `docs/ui-workflow-brief.md` §4 is wrong on this point: it says
contrast assertions "need no rendering at all — could run in the DSP-only job",
and they cannot, because `Tokens.h` includes `juce_gui_basics`. No rendering is
needed, but JUCE is. Construct a processor the way `tests/plugin/TestUtil.h`
does, make its editor, `setBounds` at design size, and assert on component
bounds. `tools/snapshot` already proves headless construction works on Windows,
so the ground is not new.

**What to assert, absolutely and against named numbers:**

- Every panel that takes the input section puts its rule's centre at y **90**;
  every panel that takes or reserves the output section puts its rule's centre
  at y **566**, its switch row at **574-601**, its output knob row at
  **602-679**. Those are `ui::ModulePanel`'s constants; the test is that the
  panels actually land on them.
- BMO Util reserves without adopting, so it is the one that proves the
  reservation works. Assert its lower rule at 566 with no output knob present.
- No control's bounds fall outside its panel, and no two controls in a column
  overlap. `MAKEUP` clipping to `MAKEU` was a five-character overflow nobody
  saw for a release.

**Then text fit, on the same harness.** After `resized()`, assert every
caption's `GlyphArrangement::getStringWidth` is inside its box. This is *not*
blocked on the licensed fonts, which I assumed for most of a session and was
wrong about: `CMakeLists.txt:58` fails the build outright on a missing `.otf`,
so any build that succeeds has them.

**Done looks like:** the suite passes; then change `kBandRow` from 112 to 111
by hand and confirm it fails, naming the panel and the number. A layout test
that has never been seen to fail is not evidence of anything.

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

*Branch `ui-editor`, 25 commits on top of `main` at 6fdf8d9.*
