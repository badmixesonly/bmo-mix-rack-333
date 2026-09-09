# The rest dot marked zero, not the default — **critical**

Found 2026-09-09 while reviewing BMO Dimension's opening state. It is not a
Dimension fault: it is suite-wide, it has been shipping since the dot was
introduced, and it is on every panel that has a knob which only goes up.

**Status: fixed on this branch for the four modules on `main`. Flagged for a
wider UI pass — see §5. To be raised with Kevin after Dimension's listening
pass, so the two can be taken together.**

---

## 1. What was wrong

Every knob with a dotted track carries a heavy dot on that track. Its own
comment in `ConcentricBand` says what it is for:

> the rest dot is where the pointer rests

It did not do that. `BmoLookAndFeel::drawRotarySlider` placed it at **zero**,
clamped into the control's range:

    (0.0 - range.getStart()) / range.getLength()

On a control that cuts and boosts, zero and the default are the same point.
That is why this survived so long — BMO EQ's band gains are bipolar and default
to 0 dB, so the dot sat straight up, exactly where it belonged, and the rule
looked correct.

**On a control that only goes up they are not the same point at all.** The dot
went to the bottom of the sweep regardless of where the knob actually rests:

| module | control | range | default | dot was at |
|---|---|---|---|---|
| Saturator | TONE | 0–100 | **100** | 0 |
| Saturator | MIX | 0–100 | **100** | 0 |
| Saturator | DRIVE | 0–100 | 40 | 0 |
| Util | WIDTH | 0–200 | 100 | 0 |
| EQ | MIX | 0–100 | **100** | 0 (no control on the panel) |
| Dimension | WIDTH | 0–200 | 100 | 0 |
| Dimension | DEPTH | 0–100 | 50 | 0 |
| Dimension | CENTS | 0–25 | 10 | 0 |
| Dimension | FREQ | 350–1400 | 700 | 350 |
| Dimension | RATE | 0.05–5.0 | 0.40 | 0.05 |

TONE and MIX are the starkest: they rest at their **maximum**, and the dot
marked the opposite end of the dial from anywhere the control had ever been.

**Dimension is the worst-affected panel** — five of its nine knobs — for the
same reason it is the worst case for caption contrast: it is the newest module
and the one with the most unipolar controls. Again, not its fault.

## 2. Why it matters more than it looks

Double-clicking a knob returns it to its default. That already worked, on every
module, and always has: `juce::SliderParameterAttachment` calls
`setDoubleClickReturnValue` with the parameter's own default when it attaches.

So the panel was drawing a mark that looks like "home", in a place double-click
would never take you. A user reading the dot as the rest position was being
told the wrong thing by the panel, on five of Dimension's nine knobs and three
of the Saturator's five.

## 3. The fix

One line of behaviour, in `BmoLookAndFeel::drawRotarySlider`: read
`slider.getDoubleClickReturnValue()` instead of computing zero.

That value is not ours to set and is deliberately **not** restated — JUCE has
already put the parameter's default there. Reading it back rather than deriving
the default a second time means the mark and the gesture are one fact and
cannot drift, which is what `modules/AGENTS.md` asks for by name.

`valueToProportionOfLength` rather than arithmetic across the range: it is the
same mapping the pointer goes through, so a skewed control keeps the two
together. Nothing in the suite is skewed today, which is why it is worth
spending the call now rather than after something is.

### The collision, and Frosty's call on it

A control whose default *is* one of its ends puts the dot on top of the symbol
already marking that end. Rendered, TONE's and MIX's dots fused with their `+`
into one malformed glyph.

**Frosty's call: if the default collides with an existing `+` or `−`, the
symbol is enough and no dot is drawn. The dot is only needed where the default
lands on an otherwise unmarked position on the dotted track.**

Measured as a pixel clearance converted to an angle at each knob's own track
radius, because these knobs run 36–53 px and a fixed angle would not hold
across them.

## 4. Verified

Rendered before and after, and diffed by pixel rather than by eye:

- **Saturator** — three bands changed, totalling 371 px: DRIVE's dot appears at
  40%, DRIVE's old dot leaves the sweep bottom, TONE's and MIX's are removed.
  INPUT and OUTPUT are byte-identical, as bipolar controls must be.
- **Util** — changed (WIDTH to 12 o'clock).
- **EQ, Opto** — byte-identical. Correct: EQ's only affected parameter, MIX,
  has no control on the panel, and none of Opto's are unipolar with a
  non-minimum default.
- DRIVE's dot measures **−30°** against a predicted −27.7° at 6° bin
  resolution; TONE and MIX show no dot cluster at all.
- `ctest` 10/10 on `main`'s four modules.

**Measure, do not look.** The original diagnosis was reached with a
`System.Drawing` scan of the render after the panel merely *looked* wrong;
an earlier visual read in the same session was flatly wrong about a DIFFUSE
pointer, and the numbers corrected it. `tools/inspect/` is the tool for this
when a toolchain is present.

## 5. Still open — the sweeping pass this wants

Not done here, deliberately: this branch fixes the drawing, and the questions
below are Kevin's and Frosty's together.

- **Dimension is not covered.** It is not on `main`. The fix reaches it as soon
  as the two branches meet, and its panel should be re-rendered then — it has
  the most to gain and nobody has seen it with correct dots.
- **Nothing tests this.** `ui_layout` asserts component bounds, and a rest dot
  is *painted* rather than placed, so no assertion could have caught it — the
  same gap recorded against the GR scale's appearance. A test that a knob's
  drawn rest mark agrees with its parameter default would be cheap and would
  have caught this on the day it shipped.
- **EQ has a `Mix` parameter with no control on the panel.** Noticed in
  passing, unrelated to this fix, unexplained. Worth someone confirming that is
  deliberate.
- **The other end of the question.** If a default lands *near* but not on an
  end symbol, the dot is drawn close to it. Nothing renders badly today, but
  the clearance rule is a threshold and thresholds want a rendered ladder.
