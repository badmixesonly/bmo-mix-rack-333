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

### And then the low end was bare

Moving the dot off the minimum exposed something it had been hiding: the dot
used to sit at the bottom of the sweep and anchor that end by accident. With it
gone, a knob that only goes up had a `+` at one end of its track and nothing at
the other, because the minus was drawn only where `range.getStart() < 0`.

**Frosty's call: both ends carry a symbol on every tracked knob.** `−` and `+`
mean **less and more**, which is how a hardware faceplate marks a knob and is
true of every control in the suite — the Saturator's DRIVE runs from less drive
to more, and nothing about that claims it cuts. The old reading, "negative and
positive", is what left the unipolar controls half-marked.

This supersedes the rule stated at `PlainKnob` in `core/ui/Controls.h`, which
said a knob carries "a plus one side and, where it cuts, a minus the other".
That doc comment is updated with it.

It also means a unipolar control whose default *is* its minimum — DIFFUSE,
SHUFFLE, BMO Opto's CRUSH — now shows `−` there and no dot, by the collision
rule above. That is the rule working, not an exception to it.

## 4. Verified

Rendered before and after, and diffed by pixel rather than by eye:

- **Saturator** — three bands changed, totalling 371 px: DRIVE's dot appears at
  40%, DRIVE's old dot leaves the sweep bottom, TONE's and MIX's are removed.
  INPUT and OUTPUT are byte-identical, as bipolar controls must be.
- **Util** — changed (WIDTH to 12 o'clock).
- **EQ, Opto** — byte-identical. Correct: EQ's only affected parameter, MIX,
  has no control on the panel, and none of Opto's are unipolar with a
  non-minimum default.
- DRIVE's dot measures **−30°** against a predicted **−28.8°** at 6° bin
  resolution; TONE and MIX show no dot cluster at all.

  The sweep is JUCE's default, `1.2π` to `2.8π` — **−144° to +144°** clockwise
  from twelve, 288° in all. Worth stating because it is easy to read the
  symbols' own positions as the sweep's ends: the 0.11 rad inset puts the minus
  and plus at ∓137.7°, and a first pass at these figures took ∓138° for the
  sweep itself and had every prediction about 1° out as a result.
- `ctest` 10/10 on `main`'s four modules.

**Measure, do not look.** The original diagnosis was reached with a
`System.Drawing` scan of the render after the panel merely *looked* wrong;
an earlier visual read in the same session was flatly wrong about a DIFFUSE
pointer, and the numbers corrected it. `tools/inspect/` is the tool for this
when a toolchain is present.

## 5. Still open — the sweeping pass this wants

Not done here, deliberately: this branch fixes the drawing, and the questions
below are Kevin's and Frosty's together.

- **Dimension is not covered by this branch**, because it is not on `main`. The
  fix reaches it as soon as the two branches meet.

  **Previewed, though.** The two were merged locally — clean, no conflicts —
  and Dimension's panel rendered with the fix in place. Five of its nine knobs
  move, and at Init the rest dots now land under their own pointers: CENTS and
  FREQ upper-left, DEPTH and WIDTH straight up, ROTATE and ASYM at twelve.
  DIFFUSE and SHUFFLE default to their minimums, so they show a minus there and
  no dot. It reads as a panel whose opening state was chosen rather than
  arrived at, which is the whole point of the change.

  That preview is also the argument for landing this **before** Dimension's
  listening pass: section 4 of `dim-testing-checklist.md` asks whether the panel
  reads as a set of controls, and it is about to read differently.
- **Nothing tests this.** `ui_layout` asserts component bounds, and a rest dot
  is *painted* rather than placed, so no assertion could have caught it — the
  same gap recorded against the GR scale's appearance. A test that a knob's
  drawn rest mark agrees with its parameter default would be cheap and would
  have caught this on the day it shipped.
- **EQ has a `Mix` parameter with no control on the panel.** Noticed in
  passing, unrelated to this fix, unexplained. Worth someone confirming that is
  deliberate.
- **~~The other end of the question.~~ Measured.** A default that lands *near*
  but not on an end symbol draws a dot close to it, and the worst case in the
  suite is BMO Dimension's RATE: 0.05–5.0 with a default of 0.40, so 7.1% along
  its own sweep and the nearest thing to a collision that is not one.

  Rendered and measured on the merged branch: the dot centres at **−121.7°**
  and the minus at **−138.0°**, **20.8 px apart** at that knob's track radius,
  against a 7 px suppression threshold. Clear, and visibly so. The threshold is
  therefore calibrated by a real case rather than only by the one it suppresses
  — but it is still a threshold, and a control defaulting to 2–3% of its range
  would sit inside it. There is no such control today.
