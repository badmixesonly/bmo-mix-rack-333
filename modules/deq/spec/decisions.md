# BMO DEQ — decisions

The spec (`spec-v0.1.md`) is kept as received. Decisions that change it are
recorded here, newest first, with who made them and what evidence they rest on.

## 2026-09-12 — band solo, and the analyser

Both are new since the spec. Both were asked for before the UI pass rather than
after it, which is the right order: solo may move the parameter layout, and the
analyser changes what the panel *is*, so neither can be laid out around after
the fact.

**Band solo: the band's output, momentary** (Frosty, 2026-09-12). Soloing a
dynamic band's output gives the moving version — the band's filtered
contribution with its gain reduction applied live — so on a de-esser you hear
the sibilance actually being grabbed. That covers most of what a sidechain
listen is for.

**Sidechain listen: worth having, not required.** It costs nothing in latency:
the detector's sidechain is its own zero-latency filter on the dry input and
does not tap the band's filter, and nothing in the path can delay
(`latencySamples()` is a `constexpr 0`, protected by block-size invariance).
Its real cost is a second momentary state and a control on a busy panel. Build
solo so listen is a variant of it; decide whether it gets a control in the UI
pass, when the space is visible.

**Both momentary, neither a parameter.** A solo left on in a saved session is a
support ticket, and twelve solo parameters would move the lane allocation for
no automation anyone wants. This makes solo the suite's **first non-parameter
path from editor to engine** — worth building carefully, because Opto and
Dimension will want it.

**Analyser: post-EQ, one drawn tap, two taps in the plumbing.** Post reads as
pre when the bands are off, which covers "what am I working on" at the cost of
having to stop processing to see it — a toggle-and-look, not a comparison.
Building the data path with two taps and wiring one means a pre curve later is
a UI change rather than a DSP change. The tap is a read-only copy into a
lock-free FIFO: no latency, no DSP state touched, and its cost belongs in the
throughput budget. It must not run with the editor closed.

**Analyser colour: a preference with five options** (Frosty, 2026-09-12), and
not the accent by default-only. Measured against the well, `#1b1b1f`:

| option | hex | hue | on well | nearest claimed hue |
|---|---|---|---|---|
| Accent (DEQ's teal) | `#5ecfc0` | 172.0° | 9.13:1 | it *is* the accent — no separation from the curve |
| Orange | `#ef8b4a` | 23.6° | 6.92:1 | 8° from BMO Saturator |
| Gold | `#e8c95a` | 46.9° | 10.57:1 | 5° from Opto's amber state |
| Pink | `#e6949f` | 352.0° | 7.44:1 | 16° from BMO EQ |
| Neutral | `#aeb4c0` | 220.0° | 8.25:1 | claims nothing |

Pink is the true complement of the teal — 352.0° against 172.0° — lifted from
the `#cf5e6d` the complement gives at the teal's own saturation and lightness,
which measures only 4.48:1 and is the faintest thing on the panel. The hue is
the complement's; the lightness is the suite's legibility.

**None of these is an accent**, and the table belongs beside Opto's red and
amber in `products/AGENTS.md` for the same reason: a colour that means
something inside one panel, that never touches a cap, a caption or a header
bar, and that claims no hue for the module. Say so where it is recorded, or a
later module will read DEQ as owning 23.6°.

**How the preference is stored: the `view` pattern.** An expandable module's
view is an attribute on the saved session's PARAMS element, written by
`getStateInformation` and never by the `captureState` a preset is made from
(`core/product/ModuleDef.h`). The analyser's colour and its on/off follow it
exactly: saved with the session, absent from presets, not automatable. The
five are named tokens in `core/ui/Tokens.h` so a theme file can still override
them, which is what every other colour in the suite allows.

**Open, for the UI pass:** where the chooser lives. Five options do not want
five buttons on a panel this busy; a right-click on the analyser's own toggle
is the cheap answer, and "Accent" rather than "Teal" is the right name for the
first option so the whole thing generalises when Dimension or Opto wants an
analyser.

## 2026-09-11 (later) — Frosty, on the first build's renders

**The panel is mockups A and C, not a reading of them.** The first build had
drifted: shape as a row of five switches, small knobs, THR / ATK / REL, a
readout over the curve. Rebuilt to the mockups' own geometry (their rows,
rules and cell order; `DeqPanel.cpp` gives the mockup y for each). Deviations
kept, each for a reason:

- The mockups' M/S amount cell holds the band's **ON** switch (the lean set
  has no M/S amount, and ON had no home in either mockup).
- Lit switches follow the suite's table (`modules/AGENTS.md`): DEQ in the teal,
  everything else -- ON, MID, DYN, AUTO -- in `switchAlt`. The mockups lit them
  all teal, which was a drawing shortcut, not a proposal.
- Compact: the second rule is at 360, not 384, because with values under
  every knob the two dynamics strips need the room the dropped M/S knob left.
- Knob faces are a little under the mockups' at the small sizes: the suite's
  dotted track is 10 px outside the cap, not the mockup's 6.

**Knobs show their values.** Every band knob, both widths; the only module in
the suite that does. Full: the host's text ("2.10 kHz", "-2.0 dB"). Compact:
mockup C's short form ("2.10k", "-2.0"). The readout over the curve is gone.

**AUTO is built.** BMO EQ's rule -- the reciprocal of the static curve's mean
magnitude, 48 log points 20 Hz - 20 kHz -- as parameter 158 (`auto_gain`,
appended; nothing moved). Not compensated: dynamic movement (a de-esser made
up by its own output is not a de-esser) and side bands. Capped at +-18 dB.
`dsp/AutoGain.h`.

**Shelf Q stops at 2** (`kShelfMaxQ`). At or under 2 a shelf is within 0.87 dB
of the analogue ideal everywhere; above it, up to 6 dB out near Nyquist. The
host parameter keeps 0.1-40 (one Q parameter a band, whatever its shape); the
engine and curve run a shelf at no more than 2, and the panel writes the knob
back down to 2 after a user action leaves a shelf above it.

**Init is the only preset** for now, confirmed.

## 2026-09-11 — Frosty

**Two widths.** The panel is compact (320, mockup C) and full (600, mockup A).
**A rack opens it compact; standalone opens it full.** Either can be switched
per instance, and the choice is kept with the session but not with presets.
The switch is **on the host's bar** -- the standalone header and the rack's
slot bar -- not on the panel. Mockups: `panel-mockups.html` (the "BMO DEQ
Panel Options" artifact).

**Rack lanes.** The 32 host lanes a rack slot has go to: output, then bands
1-6 by frequency, gain, Q, threshold and range, then the DEQ in/out switch.
Everything else is off the rack's grid (automatable standalone only).

**Band controls: the lean set, plus direction.** Per band: on, shape,
frequency, gain, Q, placement (stereo / mid / side), dynamics on, direction
(above / below threshold), threshold, range, ratio, attack, release -- 13, and
158 parameters in all (159 once AUTO was appended, above). No continuous
M/S blend; knee fixed at 6 dB; peak
detection. Direction was added after the options offered had left it out; the
spec requires both directions (§5.6).

**Twelve bands.** Supersedes spec A6's 24-band minimum. The engine's fixed
array stays larger (`kMaxBands`), so this is a product limit, set in
`params.h`.

## 2026-09-10 — Frosty

**Identity: BMO DEQ, "DEQ" for short.** BMO DEQ takes over the slot reserved
for BMO Parametric (the teal), as allocated on `main` by PR #8: module id `deq`,
plugin code `Bpar`, bundle id `com.lt3audio.bmodeq`, presets `.bmodeq`.

**No parameter limit for BMO DEQ.** Delivered on `main` as `SlotOverflow`: a
module's first 32 parameters take the slot's host lanes, the rest are held off
the grid.

**Serial band summing. Settled by ear 2026-09-12; it supersedes spec C4
(parallel).** Evidence: `topology-options.md` for the measurements,
`testing-notes/deq-blind-2026-09-11.md` for the listening. Serial is the only
option whose response is its band curves added in dB, and the only one in
which a low cut still cuts under an overlapping boost. Latency and CPU are
identical.

57 blind pairs, seven sources, all seven controls indistinguishable. Every
audible difference ran the way the measurements predicted, and nothing was
reported as a fault on either topology. Round one's preferences for parallel
were all cases of it doing less; round two matched the two for *amount* so
that only the shape of the dynamic catch differed, and four of six pairs were
then indistinguishable while the other two were preferred as serial.

**No user-facing topology switch** (Frosty, 2026-09-12). What parallel was
preferred for is reachable in serial by asking for less — 0.76 dB for stacked
cuts, 0.63 for stacked boosts, exactly for the amount of dynamic reduction —
and nothing serial does is reachable in parallel at all.

`Topology::parallel` stays in `DspCore` for now, for `measure_deq render`.
Deleting it is a separate call, and it costs the ability to A/B this again.

## 2026-09-10 — from the review (`review-v0.1.md`), in the code, not yet ruled on

These deviate from the spec's wording because the wording could not be met or
measured. They stand until the spec is revised to match or overrules them:

- T2 absolute targets gated at f0 ≤ 200 Hz; the rest held by the comparative
  gate and regression ceilings.
- T3's pole invariant stated against the prototype's poles, not the knob
  values.
- T5 overshoot defined as "never past the static target"; timing measured on
  the linear envelope, with release held to the two-stage cascade.
- T6 blend continuity read back from the audio.
- T7 denormals checked deterministically, not by timing.
- The time-constant convention is tau (§12 Q2), matching BMO Opto.
- The SVF structure with matched-Z coefficients (§12 Q1).

