# The UI pass — module 2, BMO DEQ

**On AURORA, 2026-09-15. Branch `ui-pass`, worktree `../bmo-mix-rack-333-ui`,
off `71d6d01`.** Module 1 (LTV Comp) is in `ui-pass-2026-09-14.md` and is
waiting on Leteveon; do not change it. The tools and the before-numbers are in
`ui-pass-render-loop.md`, the triage in `ui-pass-handoff-2026-09-14.md`.

**One defect fixed. One measurement that is bigger than the item that led to
it. Five decisions gathered for Frosty and none taken.**

---

## 1. Baselines, and both compact numbers, which nobody had taken

Rendered from this worktree's `build-release`, `signal=-18`, both appearances.
`snapshots/` is gitignored — these are on AURORA only, re-run the tool.

| | before | after `71d6d01` |
|---|---|---|
| expanded dark | `8b15e62102cad9a6` | `13e5c1b6526f7e37` |
| expanded light | `325623156690ffec` | `6b4aba73320e1d63` |
| compact dark | `35df0fbe474b0551` | unmoved |
| compact light | `20e67b4acc38959a` | unmoved |

The compact pair is new: `ui-pass-render-loop.md` §4 records the expanded row
and says the 320 "is a separate baseline nobody has taken yet". It is taken.

`signal=-18` renders **byte-identically to a bare render** on this module, in
both views. That is correct rather than a broken flag: every band ships off, so
Init is a wire, DEQ has no level meters, and the GR bar is the only thing a
signal could move. To see the meter you have to switch a band on *and* give it
dynamics — see §3.

Largest bare band is **30 px at 536..565** expanded and **20 px at 340..359**
compact, identical in both appearances and unchanged by this session's commit.
DEQ is the two tightest panels in the suite. There is no airiness question here.

---

## 2. Fixed: the GR readout said "−12."  (`71d6d01`)

The gain-reduction bar's cell on the full panel was **36 px** — the 12 px bar
and a margin — and the figure printed under it needs **45.9**. It has been
drawing `−12.` instead of `−12.0` since the module's first build, in both
appearances, and no test looked at it.

**It is the MAKEUP → MAKEU class, one container over.** `checkCaptionFits`
walks `PlainKnob` captions; `checkSwitchLabelsFit` walks `SwitchButton`s and, since
DEQ's first render clipped BELL to "BEL", bare `ToggleButton`s too. The GR bar
is a `Component` that paints its own caption and readout by hand, so all three
checks went past it. Every time this bug has appeared it has been in whatever
container nobody had got round to measuring yet.

So the measurement ships with the fix:

- `GainReductionBar::valueOverflow()` — the widest word the bar can draw,
  against the box it actually has.
- `GainReductionBar::widestValue()` — fixed at `−24.0`, not sampled.
  `DspCore::currentGainReductionDb` returns the deepest **single** band rather
  than a sum, and a band's offset is bounded by its own range parameter, whose
  floor is −24 dB. A cell sized to the reading somebody happened to see when
  they looked is a cell that clips later.
- `checkDeqPanel` asserts it at both widths, and **was seen to fail on the
  fault first**: `deq GR bar's widest word ("−24.0") overflows its 36 px cell
  by 9.9 px`.

The compact panel shows no value, so only `GR` is measured there and its 36
stands — both compact renders are byte-identical across the change.

**BMO DEQ also gets `checkTrimKnobHeights`**, which until now ran on LTV Comp
alone. That was the carried-forward item: `removeFromTop` clamps rather than
overflows, so a panel that no longer fits shrinks silently and the layout suite
still passes. DEQ is the other content-dense panel — thirteen controls a band,
twelve bands, a curve and a meter, and the compact half does all of it in 320 —
and OUTPUT is its one trim knob.

---

## 3. Measured, not settled: the GR bar is blind to half of what it meters

The checklist says "GR bar shows the deepest cut across bands; an upward band
shows nothing. Label or redesign." That is now a number rather than a claim.

Same panel, same signal, one parameter's **sign** changed:

```
b7_on=1 b7_dyn=1 b7_freq=1000 b7_thr=-30 b7_ratio=4 signal=-6

  b7_range=-12   bar column: 88 px of #4fb8e8, then 90 px of well
  b7_range=+12   bar column: 180 px of well. Nothing.
```

A band moving the signal by 12 dB upward reads as a dead meter. `DspCore.cpp:357`
is `deepest = max(deepest, -offsetDb * tick)`, so a positive offset contributes
zero by construction, and `ModuleContext::gainReductionDb` is documented
"always >= 0" — the callback cannot carry the other sign.

**Frosty's call. Four options and what each costs:**

1. **Leave it, and say so in `spec/decisions.md`.** The bar meters reduction;
   upward bands are not shown. Free, and honest once written down.
2. **Rename the caption.** `GR` → `CUT`. Free in code, but GR is the term every
   other compressor in the suite uses and this would be the one that differs.
3. **Make the bar bipolar**, growing down from a centre line for cut and up for
   boost. Needs `currentGainReductionDb` to report a signed deepest offset,
   which changes a `ModuleContext` callback's contract for every module.
4. **A sixth callback.** The same shape as the note Tune's empty middle wants
   ("it would be a sixth callback and a float from `TuneCore`"). Two modules now
   want the context widened; worth deciding once rather than twice.

---

## 4. The headline: a dimmed caption is **1.23:1** on the pale plate

This started as the checklist's carried item — DEQ's raw legend at 1.64:1,
"worse than Dimension's 1.73 and nobody has raised it". It is confirmed
(`#5ecfc0` on `#efefef`, 1.64:1, ΔL\* 17.9) and it is **not the interesting
number**.

BMO DEQ is the only module in the suite that ships with dimmed knobs.
`refreshEnablement` dims the five detector knobs whenever DYN is off — which is
every band's default — and GAIN whenever the band is a cut. So **six of the
eight band captions on a freshly opened DEQ are in the disabled state.**

`core/ui/Controls.cpp:145` dims a caption with a flat `ink.withAlpha (0.4f)`.
Measured off the render, not computed:

| | ink | on the pale plate | on the dark plate |
|---|---|---|---|
| enabled caption | `#5ecfc0` | 1.64:1 | 7.19:1 |
| **disabled caption, today** | **`#b5e2dc` / `#426f6b`** | **1.23:1** | 2.36:1 |

**1.23:1 is the lowest figure measured anywhere in this suite** — below the
1.72–2.00 band the raw legends spend, and below Tune's lime at 1.29, which is
the number Frosty looked at and kept.

The flat alpha is the fault, and it is one-sided. On the dark plate the accent
is *lighter* than the plate so dimming walks it down to a workable 2.36; on the
pale plate the accent is *darker* than the plate, so the same 0.4 walks it into
the plate. And the pale side cannot be rescued by the alpha, because the
accent itself is the ceiling:

| alpha | pale plate | dark plate |
|---|---|---|
| 0.40 (today) | 1.23:1 | 2.36:1 |
| 0.50 | 1.29:1 | 2.94:1 |
| 0.60 | 1.35:1 | 3.59:1 |
| 0.70 | 1.42:1 | 4.34:1 |
| 1.00 (no dim) | **1.64:1** | 7.19:1 |

Three candidates rendered, both appearances, in
`snapshots/_dq-dims-l.png` and `_dq-dims-d.png`:

- **A — as it is.** 1.23 / 2.36.
- **B — a disabled caption drops to `text2`.** 2.45 / 4.85. Clears the band in
  both appearances and is a named themeable token. Rendered, it is the most
  legible and the least honest: at `text2` the word is *darker* than an enabled
  caption on the pale plate, so disabled reads as more emphatic than enabled.
- **C — the knob dims, the word does not.** 1.64 / 7.19. The face, the dotted
  track, the pointer, the ∓ symbols and the value line all still carry the
  disabled state at their own alphas; only the name stays readable. Rendered,
  this works on the pale plate and is too strong on the dark one, where a
  disabled caption becomes indistinguishable from an enabled one.

**The bind, stated plainly, because it is the actual decision.** On the pale
plate the *enabled* caption is 1.64:1 by Frosty's own call — `Controls.cpp:128`
records the 0.2.3 swap that cost it and says "Do not 'fix' it". So a disabled
caption that clears the raw-legend band must be **darker than the enabled one**,
which inverts the hierarchy. Any option that keeps the order caps out at 1.42.
The only way to have both is to raise the enabled caption too, and that reopens
a decision already taken.

**Not mine to pick.** But note the scope: `setKnobEnabled` is used by BMO DEQ
and nothing else today, and the checklist's Dimension item ("dim CENTS when
DETUNE is off — `PlainKnob::setKnobEnabled` exists and is unused suite-wide,
this is its case") is module 4. Whatever is decided here is decided for
Dimension as well, so it is worth taking before module 4 rather than twice.

---

## 5. Checked and correct — do not re-find these

Recorded so the next session does not spend the time again. Three of them looked
like faults at 1:1 and were not; each was settled by `scan` or `hash`, never by
looking harder. The habit in `ui-pass-render-loop.md` §6 is load-bearing.

- **The SHAPE dial.** Read at full-panel scale it appears to point at BELL while
  band 1 is a Low Cut. It is not: rendered at all five shapes and hashed, the
  baseline is byte-identical to `b1_shape=Low Cut`, the pointer tracks, and the
  active legend lights in the accent. `snapshots/_dq-shapesheet.png`.
- **A render with any parameter set hashes differently from a bare one, even
  set to its own default.** The header reads `Init *` rather than `Init` —
  correct dirty-preset behaviour, and nothing to do with the panel. It cost a
  detour here. **Compare like with like**: a baseline for a change made with
  `k=v` must itself be rendered with a `k=v`.
- **The GR well is not a filled bar.** At 1:1 it reads as a light slab in dark
  mode. Scanned, it is `#1b1b1f` on `#2e2e32` (and `#d6d6d6` on `#efefef`) — a
  recess, empty, correct.
- **The band-1 node clears the printed `0`.** This was the Opto
  needle-through-the-zero check, done first: at Init the node ring sits right of
  the `0` on the response line with clean plate between them, in both
  appearances. `snapshots/_dq-nodesheet.png`. A band that is off draws a hollow
  ring, one that is on draws a filled node with its range stem — the two states
  are distinguishable.

### The rest dots are right, and they update `rest-dot-finding.md`

Checked against the rule, not the render: every DEQ knob whose default is
mid-sweep shows a dot under its own pointer (GAIN and OUTPUT at 12 o'clock,
Q at 32.7%, THRESH 60%, RANGE 37.5%, RATIO 23.1%, ATTACK 51.5%, RELEASE 53.1%).

FREQ is the one that varies, because its default is the band's own spread
frequency. **Bands 1 and 12 show no rest dot at all**, and that is the
suppression rule firing correctly — band 1's 30 Hz is 5.9% along a log sweep,
10.6° from the `−`, against a threshold of 11.8° at that knob's track radius;
band 12's 18 kHz is 98.5% and 1.9° from the `+`. Bands 2–11 all show one.
Rendered and confirmed at 1, 2 and 12: `snapshots/_dq-restdots.png`.

**`rest-dot-finding.md` §5 needs one line changed.** It says the nearest thing
to a collision that is not one is Dimension's RATE at 20.8 px clear, and that
"a control defaulting to 2–3% of its range would sit inside it. There is no such
control today." There is now, and it post-dates the note: DEQ band 1's FREQ
defaults to 30 Hz, which is *not* its minimum of 20, and is suppressed anyway.
The panel marks no rest position for a control that has one. That is the
threshold working as specified rather than a defect, but the note's claim is
stale and should not be read as current.

---

## 6. Blocked on Frosty — evidence gathered, nothing decided

1. **The disabled-caption contrast**, §4. The largest item on this module, and
   it reaches BMO Dimension too.
2. **The GR bar and upward bands**, §3.
3. **Solo and the analyser.** Confirmed exactly as the checklist has it. Both
   are in the engine and tested — `DeqDsp::setSolo`, `DeqDsp::analyser()`,
   `DspCore` carries `AnalyserTap pre, post` and an atomic solo band — and
   `modules/deq/panel/` contains **not one reference to either**. Meanwhile
   `spec/decisions.md` records them in detail as settled: momentary solo, the
   suite's first non-parameter control, post-EQ analyser, a five-option colour
   preference, a right-click on the analyser's own toggle. Wire them in this
   pass, or say in the decisions file that they wait. Shipping a decisions file
   that reads as if they exist is the one option that is not defensible.
4. **The response view draws the 48 kHz design whatever the rate.**
   `ResponseView.h:99` is `static constexpr double kDisplayRate = 48000.0`, and
   the view has no path to the real rate — `ModuleContext` does not carry one.
   Up to about 1 dB out in the top octave at 44.1 or 96 k. The rate in
   `ModuleContext`, or a note.
5. **The compact 320 in a rack.** Rendered next to BMO EQ and BMO Util, both
   appearances (`snapshots/_dq-rack-dark.png`, `_dq-rack-light.png`). It holds
   its width and reads as its own module. Two things to look at rather than
   fix: the GR bar reads as an unexplained pale slab at that size with its `GR`
   flush to the panel edge, and §4's dimmed captions are at their worst here,
   because a rack is where a panel gets glanced at rather than read.

---

## 7. Still open, not raised here

- **Contrast assertions** (`docs/ui-workflow-brief.md` §4) still do not exist.
  §4 above is exactly the bug class they would catch, and it would have been
  caught on the day DEQ shipped: it is a pure function of the tokens and the
  alpha, and needs no rendering. It stayed out of this commit because the floor
  it should assert is the decision in §4, and asserting a floor the suite does
  not meet is a red suite, not a guard. **Do it as soon as §4 is settled.**
- **Nothing tests a rest dot.** Still true, and `rest-dot-finding.md` §5 still
  says so. The mark is painted, and the suppression rule lives inside
  `drawRotarySlider` mixed in with the geometry. A pure `restMarkFor (slider,
  trackRadius)` returning the proportion and whether one is drawn would be
  assertable without rendering — but it is a `core/ui` refactor touching every
  module's paint path, so it is a suite item and not a DEQ one.
- Nothing pushed. **24 commits on `ui-pass` locally**, CI untouched this pass.

## 8. Next

**BMO Tune RT**, module 3 — but only once §4 has an answer, because Dimension
(module 4) inherits it and because it is the one item on this module that
changes what a panel looks like.

*Everything above measured on **AURORA**.*
