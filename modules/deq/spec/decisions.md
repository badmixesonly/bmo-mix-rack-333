# BMO DEQ — decisions

The spec (`spec-v0.1.md`) is kept as received. Decisions that change it are
recorded here, newest first, with who made them and what evidence they rest on.

## 2026-09-10 — Frosty

**Identity: BMO DEQ, "DEQ" for short.** BMO DEQ takes over the slot reserved for
BMO Parametric (the teal). The rename is done on `main` first, as its own
change; this module adopts whatever that change allocates (plugin code, bundle
id, preset extension, module id). The spec's placeholder `BMO-DEQ` becomes the
real name.

**No parameter limit for BMO DEQ.** `main` exempts it from the 32-per-slot
rule. This resolves the band-budget problem: spec A6's 24-band minimum stands.
The mechanism is defined on `main`.

**Serial band summing, pending a listening test.** It supersedes spec C4
(parallel). Evidence: `topology-options.md`. Serial is the only option whose
response is its band curves added in dB, and the only one in which a low cut
still cuts under an overlapping boost. Latency and CPU are identical. Test:
`testing-notes/deq-topology-listening.md`. If the test confirms serial,
`Topology::parallel` is deleted. If serial's stacking is judged too much, the
hybrid is the measured fallback.

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
