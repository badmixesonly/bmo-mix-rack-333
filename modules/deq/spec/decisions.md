# BMO DEQ — decisions

The spec (`spec-v0.1.md`) is kept as received. Decisions that change it are
recorded here, newest first, with who made them and what evidence they rest on.

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
