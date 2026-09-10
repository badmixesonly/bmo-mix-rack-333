# The UI pass, batched — planned 2026-09-09

Everything cosmetic waits for one pass and one build, rather than a build per
tweak. Frosty's call: *"we'll make it pretty in full before the next build."*

**Ordering.** Nothing here starts until the ROTATE *direction* is confirmed by
ear on the `bc89293` artifact. If the sign is wrong the fix is in the DSP, not
the panel, and it should ride along with this pass rather than take a build of
its own.

---

## 1. ROTATE reads L and R, not − and +

The knob means left-and-right, not less-and-more, so it should say so.

**Not a one-line change**, and the reason is written into the code it touches.
`core/ui/LookAndFeel.cpp:226`:

> Drawn rather than set. Neither panel face has a minus sign that matches its
> plus, and two strokes and a bar are the one case where drawing beats setting:
> they match each other exactly, at any size, on any machine.

So the pair are filled rectangles, not glyphs — `arm`/`weight` scaled for
concentric versus bare-face knobs. Going to letters means:

- **Typeset instead of drawn.** Which face, which size, and how to keep L and R
  optically balanced against each other — two rectangles are balanced by
  construction, two letters are not.
- **A per-knob opt-in.** This is ROTATE only. The drawing lives in shared
  `LookAndFeel` code that every tracked knob in the suite paints through, so it
  needs a switch on the knob rather than a change to the default.
- **Look at the render.** Text at the ends of a 64 px track, at whatever inset
  the current `symbolInset` gives, is exactly the kind of thing that reads fine
  in code and clips on the panel. `tools/inspect` measures it; do not eyeball.

**Consider ASYM at the same time.** It has the same problem — it is also a
left-against-right control wearing less-and-more marks — and doing one without
the other leaves the bottom row inconsistent. Frosty has not asked for it.

## 2. §4's panel questions, re-asked against the panel that exists

The checklist's §4 items described a nine-knob layout. It is **seven knobs and
a switch** since RATE and DEPTH lost their controls, so those questions need
re-asking rather than answering as written:

- Does **CENTS paired with DIFFUSE** under the DETUNE switch read as one gated
  pair? The switch gates only CENTS, and DIFFUSE now sits beside it with no LFO
  row beneath — the row means something different than it did.
- Does the **DETUNE switch** pull the eye wrongly? It lights `switchAlt` blue
  per the table in `modules/AGENTS.md` and is the only non-lavender thing on
  the panel.
- **WIDTH at 0 silently disables everything above it.** Measured: peak side
  0.00000. Does that trip a user up, and is there a visual answer — this is the
  remaining case for `PlainKnob::setKnobEnabled`, which is still unused
  suite-wide.
- **Does the panel now read as sparse?** A row came out and the gap is derived,
  so the remaining five blocks re-spaced rather than leaving a hole. Rendered it
  looks right; that is a judgement, not a measurement.

**Closed by the removal:** Frosty's "not in love with the default dot for RATE"
is moot — RATE has no control to carry a dot.

## 3. Knob sizing, now that there is room

Pairs are 64 px, chosen when there were ten controls in the column. There are
eight. `DimPanel.cpp` deliberately keeps 64 to match the rest of the suite
rather than growing into the space — worth a second look in a full pass, since
"matches the suite" and "uses the panel well" now pull in different directions.

## 4. Suite-wide, already open before this module

From `testing-notes/ui-editor-handoff.md` §7 and `rest-dot-finding.md` §5.
These are not Dimension's, but a UI pass is when they get cheap:

- **Contrast assertions.** Called "the cheapest thing still open" — a pure
  function of the tokens, needing no rendering and no component.
  `docs/ui-workflow-brief.md` §4.
- **Nothing tests the rest dot.** `ui_layout` asserts component *bounds* and a
  rest dot is *painted*, so no assertion could have caught the bug that shipped
  it at zero. A test that a knob's drawn rest mark agrees with
  `getDoubleClickReturnValue()` would be cheap and would have caught it.
- **Section rules line up at the ends and nowhere else.** Frosty's call was
  "some should, some should not", which wants a module-by-module pass rather
  than another shared constant.
- **Low-cut crowding** in BMO EQ. Parked at Frosty's request; noted so the pass
  does not rediscover it as new.

## 5. Light mode — decide, do not drift

Dimension's captions measure **1.73:1** on the pale plate (`#d4a4ff` on
`#efefef`, L\* 74.7 on 94.4). The whole suite sits in a 1.72–2.00:1 band, and
captions carry the raw accent by Frosty's call in 0.2.3, documented at
`core/ui/Controls.cpp` with "do not fix it".

Dimension is the first panel where **every** caption carries the accent, so it
is the worst case of an accepted trade rather than a new fault. Either that
trade is still accepted and this stops being raised every pass, or it is
revisited once — but it should not keep surfacing as an open question when it
has a documented answer.

---

## What this pass must not do

- **Not touch the DSP.** The listening pass cleared a specific binary. Any DSP
  change invalidates that and needs its own listen.
- **Not renumber or remove parameters.** `kRate` and `kDepth` stay in
  `params.h` with no controls. IDs are permanent and append-only, and a session
  that automated them must still load.
- **Not judge a render by eye.** `tools/inspect` exists because three faults in
  this suite were visible only under measurement, and one of them was found in
  this session — RATE's pointer read as sitting at 40 % of sweep at panel scale
  and measured at its documented 7 %.
