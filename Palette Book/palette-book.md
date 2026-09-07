# BMO Palette Book

Every colour the suite draws with, measured on the ground it is actually
drawn on — and the direction chosen off the back of it.

Measured 2026-09-08 on branch `ui-editor`, from the literal values in
`core/ui/Tokens.h`, `modules/*/Module.cpp` and
`modules/opto/panel/OptoPanel.cpp`. Panel observations are read from 2×
`tools/snapshot` renders of all four modules and the rack chain
`util,eq,sat,opto` — not from the source.

Contrast is WCAG 2.x relative luminance, sRGB. The house rule from
`docs/ui-workflow-brief.md` applies throughout: **assert absolutes, never
comparisons.** "Better than it was" is the trap a relative release test fell
into for a whole release.

---

## 1. The finding

**The suite has no dark end.** Ink on the faceplate runs from 1.15:1 to
4.37:1, and everything a person actually reads sits in the bottom half of
that. The only dark value in all four modules is the Opto meter face
`#3a3a3a`, added last release to fix exactly this problem, and never carried
back out.

**Fifteen of sixteen ink/ground pairs fail. The one that passes is the one
that was measured.**

### Every ink, on its real ground, worst first

| Ink | Ground | Where it is used | Ratio |
|---|---|---|---|
| `#ffffff` pointer | `#ead2ff` Opto cap | knob pointer | **1.39:1** |
| `#e0b040` meterHigh | `#d6d6d6` well | output meter bar | **1.38:1** |
| `#ffffff` pointer | `#f8c6da` EQ cap | knob pointer | **1.49:1** |
| `#7fd0f2` trackFill | `#efefef` plate | selected preset, popup highlight | **1.50:1** |
| `#7fc98a` Util accent | `#efefef` plate | section legend, 11 pt | **1.72:1** |
| `#d4a4ff` Opto accent | `#efefef` plate | raw accent as ink | **1.73:1** |
| `#b4b4b4` hairline | `#efefef` plate | every section rule | **1.80:1** |
| `#efa552` Sat accent | `#efefef` plate | section legend, 11 pt | **1.80:1** |
| `#4fb8e8` track | `#efefef` plate | **every knob caption**, 15 pt | **1.95:1** |
| `#f08cb4` EQ accent | `#efefef` plate | section legend, 11 pt | **2.00:1** |
| `#4cacdc` switchAlt | `#efefef` plate | selected frequency legend | **2.22:1** |
| `#ffffff` | `#a6a6a6` switchOff | text on a disengaged switch | **2.43:1** |
| `#9a9a9a` text2 | `#efefef` plate | unselected frequency legend | **2.45:1** |
| `#9c71c3` | `#efefef` plate | Opto captions, hardcoded in the panel | 3.28:1 |
| `#6f6f6f` text1 | `#efefef` plate | header and preset bar only | 4.37:1 |
| `#ffffff` | `#3a3a3a` meter face | VU needle and scale | **11.37:1** |

Note the two states of a switch: white on the engaged fill measures
1.98–2.55:1 depending on module, and white on the disengaged grey measures
2.43:1. The label is equally hard to read either way, so **on and off are
separated by hue alone** — which fails for a colourblind user and fails in a
screenshot.

### The structural greys are five shades of one grey

| Token | Hex | vs plate |
|---|---|---|
| `plate` | `#efefef` | — |
| `plateEdge` | `#e4e4e4` | 1.07:1 |
| `well` | `#d6d6d6` | 1.30:1 |
| `hairline` | `#b4b4b4` | 1.80:1 |
| `outline` | `#9e9e9e` | 2.33:1 |

`plate` to `well` spans `0x19`. A recess at 1.30:1 against its own surround
is not a recess.

---

## 2. What the rack render shows that the source did not

Three of these are new; the rest confirm what the arithmetic predicted.

1. **Knob captions are azure on every module except Opto.** INPUT, DRIVE,
   TONE, MIX, GAIN, PAN, WIDTH and OUTPUT are all `#4fb8e8`, sitting directly
   under orange, green and pink knobs. Side by side it reads as a mistake
   rather than a system. **Opto's purple captions are the only ones that
   belong to their module.**

2. **The section rules do not line up across modules.** `AGENTS.md` states
   the fixed 688 px content height exists so "a rack of them reads as one
   surface with the section rules lining up across modules." In the render,
   Util's first rule, EQ's and Sat's all sit at different heights. The stated
   goal is not delivered by the implementation.

3. **Opto is ahead of the others, not behind them.** Its captions match its
   module, its switches are the shared 70×26, its meter is the only readable
   element in the suite. What drifted is the *code* — a hardcoded hex in a
   panel file, a bespoke meter class, no section rules. The look is the model
   to copy.

4. **The EQ MID legend collides with the rest-position dot** — `1k6 · 3k2`,
   with the pink track dot landing between the two labels.

5. **The Opto VU scale overlapped and the face was ~45 % empty.** Fixed; see
   section 5.

6. **Switch widths are 56 / 62 / 70 / 70 px** across four adjacent modules.

7. **Panels butt plate-to-plate with no divider.** Only the slot bar carries
   a 1 px edge. "Reads as one instrument" has overshot into "cannot tell
   where a module ends."

8. **The dBFS meters are placed three different ways** — Util centred, EQ and
   Sat right of OUTPUT, Opto none — and the 9 pt `dBFS` label at 2.45:1 is
   the only thing saying the meter is clickable.

---

## 3. One token is doing five jobs

`pointer` is `#ffffff`, and it is used for the knob pointer, the selector-ring
annulus, the text on an engaged switch, the VU needle, and the scale ticks.
Those five want different values the moment the plate stops being pale, which
is why no single edit fixes the pointer today, and why the theme switch cannot
land before the split.

| Current use | Ratio | Should become |
|---|---|---|
| knob pointer on a pale cap | 1.39:1 | `pointer` — dark in light, light in dark |
| text on an engaged switch | 1.98:1 | `onAccent` — black or white, whichever reads |
| selector-ring annulus | 1.15:1 | `ringFace` |
| VU needle and ticks | 11.37:1 | `meterInk` — already correct, keep |

Alongside it, `accent` stays a *fill* and gains a derived `accentText` for
ink. `core/AGENTS.md` says "Tokens are the only place colours live"; Opto had
to break that rule because the token it needed did not exist, and hardcoded
`#9c71c3` in `OptoPanel.cpp`. Deriving `accentText` and `onAccent` from the
accent rather than typing them alongside it means module six never hand-rolls
a hex.

---

## 4. The direction: Option A, "Repair"

Chosen 2026-09-08. Fix the ink, keep every surface. Pale caps stay exactly as
they are; the pointer flips to dark and captions and legends move to a derived
per-module `accentText`.

| | now | Option A |
|---|---|---|
| Knob pointer | 1.39:1 | **10.19:1** — dark on the same cap |
| Knob caption | 1.95:1 | **4.5:1** — and it matches the module |
| Section legend | 1.72:1 | **4.5:1** — same derived colour |
| Text on switchOff | 2.43:1 | **4.94:1** — one grey darkened |
| Surfaces | — | unchanged |

### Derived `accentText`, per module

| Module | Accent | `accentText` | on plate | white on it |
|---|---|---|---|---|
| BMO EQ | `#f08cb4` | `#975871` | 4.62:1 | 5.32:1 |
| BMO Saturator | `#efa552` | `#8f6331` | 4.57:1 | 5.25:1 |
| BMO Util | `#7fc98a` | `#4b7751` | 4.50:1 | 5.18:1 |
| BMO Opto | `#d4a4ff` | `#7d6196` | 4.54:1 | 5.22:1 |

`#7d6196` supersedes the hardcoded `#9c71c3` in `OptoPanel.cpp`.

### Option B, "Re-value", declined for now

Saturating the caps back toward the true accent (EQ `#c57394`, Sat `#c48743`,
Util `#68a571`, Opto `#ae86d1`) gives the knob more presence — cap-on-plate
goes from 1.24:1 to 2.65:1. But the white pointer on a saturated cap only
reaches 3.05:1, so B needs the dark pointer anyway. That makes the cap change
taste rather than legibility, and it belongs in its own decision rather than
riding along with this one.

---

## 5. Dark mode

**The accents were always dark-mode colours.** On a `#202024` plate all four
raw accents clear 7:1 with no adjustment at all:

| Module | Accent | on `#efefef` | on `#202024` |
|---|---|---|---|
| BMO EQ | `#f08cb4` | 2.00:1 | **7.05:1** |
| BMO Saturator | `#efa552` | 1.80:1 | **7.87:1** |
| BMO Util | `#7fc98a` | 1.72:1 | **8.21:1** |
| BMO Opto | `#d4a4ff` | 1.73:1 | **8.16:1** |

The palette was designed for a dark plate and has been shipping on a pale one.

### The dark set, provisional

| Token | Hex | Note |
|---|---|---|
| `well` | `#141418` | 1.13:1 vs plate — too narrow, wants a render pass |
| `plate` | `#202024` | text1 `#e6e6ea` reads 13.04:1 |
| `plateEdge` | `#2a2a30` | 1.14:1 vs plate — also too narrow |
| `hairline` | `#3c3d43` | 1.50:1 |
| `outline` | `#62636e` | 2.73:1 |
| `switchOff` | `#4a4a53` | white text 8.77:1 |
| `onAccent` | `#101012` | on Opto accent 9.55:1 |

The structural greys carry the same too-narrow interval the light set has and
should be widened once they can be seen on a panel.

### How it is built

**Dark mode is not a second design. It is a second binding of one token set.**
Option A applies to both. That is what makes the moon icon a change at the
paint layer rather than a parallel set of panels to maintain, and it is why
the token split in section 3 has to land first.

The preference is **machine-wide**, stored beside the theme JSON — not a
plugin parameter. A parameter would touch `specs()`, which `AGENTS.md` freezes
as permanent and append-only and which every golden schema test pins, and it
would expose "dark mode" to host automation. The existing 1 Hz theme poll in
`ProductEditor` and `RackEditor` already propagates a change to every open
editor, standalone and rack, within a second.

---

## 6. Done so far on this branch

- **VU meter geometry** (`c4f4440`). The radius came from
  `jmin(width/2, height)`, but a 124° sweep is limited by width alone, so on
  any face taller than half its width the arc was sized to the wrong
  dimension — 0.2.1 shipped with roughly 40 % of the meter empty above the
  needle. The radius now comes from the width and the drawn block is centred
  in whatever face it is given. The scale opened at −20, which is also where
  the needle rests in silence, so an idle meter left the needle lying across
  its own leftmost numeral; it now opens on an unprinted point at −30. −7 and
  −3 lost their numbers, and an unnumbered tick at −15 fills the low-end
  stretch. All nine ctest suites pass.

## 7. Still open

- **Rule alignment across modules.** Some should, some should not; needs a
  module-by-module pass before the row grid moves.
- **Opto's panel rhythm.** Three dead bands remain — TELE→COMP, meter→MAKEUP,
  and below LINK. The 52 px reclaimed from the meter is not yet redistributed.
- **The reusable VU meter base**, with swappable colour and its own ruleset,
  so a dynamics module six does not inherit a bespoke class.
- **EQ legend crowding** — the MID rest-dot collision and the five-legend
  low-cut selector on a 13 px face radius.
- **Switch geometry**, 56 / 62 / 70 / 70 across four adjacent modules.
- **Rack module separation**, and the three placements of the dBFS meter.

---

*LT3a · BMO Mix Rack · branch `ui-editor` · 2026-09-08*
*Interactive version with rendered swatches:
<https://claude.ai/code/artifact/8df1a5fa-72ec-4110-824e-2cf331e63a40>*
