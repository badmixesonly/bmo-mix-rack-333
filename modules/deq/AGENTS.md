# modules/deq/ — BMO DEQ, the zero-latency dynamic EQ

Read `modules/AGENTS.md` first. This file is what cannot be read off the code:
the invariants, what the spec asked for that could not be delivered as written,
and what is still open.

The spec is `spec/spec-v0.1.md` (as received, unedited). `spec/review-v0.1.md`
is the review of it, with the measurements behind every disagreement.

## Status

**DSP only.** No `params.h`, no panel, no product, not in the rack registry.
`bmo_add_module(deq ...)` builds the JUCE-free library, `deq_dsp_tests` holds it
to the spec, `measure_deq` prints the numbers. Nothing a host can load exists
yet, and nothing in the shared rack/product files has been touched — so this
cannot conflict with another module's branch.

**Decided 2026-09-10** (`spec/decisions.md`):

- **Identity: BMO DEQ** ("DEQ" for short). It takes over the slot reserved
  for BMO Parametric (the teal). The rename lands on `main` first, in a
  separate change; this module follows what that change allocates. `deq` stays
  a working id until then.
- **No parameter limit.** `main` exempts BMO DEQ from the 32-per-slot rule.
  How the exemption works (rack mapping, host parameter list) is `main`'s to
  define; `params.h` waits for it.
- **Serial topology**, pending the listening test in
  `testing-notes/deq-topology-listening.md`.

## The invariant: zero latency, structurally

Reported latency is 0 in every mode, and the *reason* it is 0 is that nothing
in the audio path can delay: every band is a recursive filter whose output at
n depends only on input up to n. No delay line, no lookahead, no FIR, no
block-buffered stage. `DspCore::latencySamples()` is a `constexpr 0`.

The test that protects this is not the impulse test — it is **block-size
invariance**: one 4096-sample block and 4096 one-sample blocks give the same
bits, with dynamics running. Any block-level state fails it. That is why:

- **Coefficient redesign runs on an absolute sample counter** (every
  `kControlInterval` = 8 samples from `prepare()`/`reset()`), never at block
  boundaries. Redesigning "once per block" would be cheaper and would break
  this.
- **Denormal flushing runs on the same cadence.** Flushing at the end of a
  block makes the output depend on where the blocks fall.

A mutation test (a `tickPhase = 0` at the top of `process`) failed 12 checks.
Keep it that way.

## Design decisions and why

| decision | why | evidence |
|---|---|---|
| Matched-Z poles, fitted zeros | bilinear cramps near Nyquist; oversampling to fix it is latency | bells 3.7x-58x better than the cookbook in region 3 (`measure_deq gate`) |
| **TPT SVF structure, loaded from a biquad** | an SVF glides under modulation, a direct form does not; `SvfCoeffs::fromBiquad` realises *any* stable biquad | IR matches DF-I to 5e-14 |
| Bell: DC, gain at f0, zero slope at f0 | the peak is where the knob says, at the height it says | exact to 1e-5 dB at every rate |
| Shelves, high cut: least squares, DC pinned | three-point fits failed outright on some shelves and were 5 dB out on others | see review |
| **High shelf built from the low shelf** | a boosted HS's poles are at f0·√A, above Nyquist for big boosts up top, and e^(sT) aliases them (25 dB error). HS(A) = A²/LS(A) exactly | HS error = −(LS error), worst 0.87 dB at Q ≤ 2 |
| **Cut = reciprocal of the boost** | analogue H(−g) = 1/H(g); boost-then-cut nulls exactly, and cuts come out as accurate as boosts instead of ~3x worse | symmetry 1e-6 dB (spec asked 0.01) |
| Detector hears the **dry** input through **its own** sidechain filter | tapping the band's filter gives a detector whose gain moves with the band's (bell pole Q = A·Q): 0 dB at −12, +12 dB at +12 — a feedback loop | spec §3.2 "the tap is free" is wrong for a dynamic band |
| Every band filters M and S, not L and R | H(L) = H(M) + H(S), so one filter pair gives both the L/R and M/S contributions; the M/S blend is exact at every value with no warm-up | T6 blend sweep reads back continuous and monotone |
| tau time constants | BMO Opto already uses them (`coeffFor`); one suite, one convention | T5 attack within 5 % |

## Where the spec was changed in the tests, and why

Every one of these is argued with numbers in `spec/review-v0.1.md`.

- **T2 absolute targets** are asserted only at f0 ≤ 200 Hz. Above that, a wide
  loud band's skirt extends past Nyquist and no biquad can follow it (bells
  pass 42/60 at 5 kHz, 14/60 at 18 kHz). The rest of the grid is held by the
  spec's **comparative gate** (passes, 3.7x minimum) and by **regression
  ceilings** in `testAccuracyCeilings` — measured values +5 %.
- **T3 `a2 = e^(−w0/Q)`** is false for bells and shelves with the knob values.
  Asserted instead against the prototype's own poles, `e^(−(d1/d2)/Fs)`.
- **T3 analytic vs IR** uses an FFT as long as the filter needs
  (`irLengthFor`), and the *engine's* IR, not a direct-form stand-in. The
  spec's fixed 64k was 0.35 dB out on slow filters from truncation alone.
- **T5 overshoot** as worded (≤ 3 dB in the first 3 ms at 0.1 ms attack; none
  at ≥ 10 ms) is impossible for any causal detector: the first sample's error
  is ~10 dB whatever the code. Asserted instead: the gain never passes its
  static target, and approaches it monotonically.
- **T5 timing**: attack is measured on the linear envelope with the clock
  started one sample before the step; release is held to the two-stage
  cascade, which is what the §5.5 detector *is* (attack 100 / release 10
  measures 110 ms, correctly).
- **T6 "no zero-crossing artefact"** is not measurable as worded. The output is
  linear in the blend, so the test reads the blend back from the audio and
  requires it continuous, monotone and in [0, 1].
- **T7 denormals** are checked deterministically (no subnormal in any state),
  not by timing blocks — the harness may not depend on the wall clock (§6).

## Numbers (2026-09-10, `measure_deq`)

Accuracy, worst |dB| vs the prototype, regions 20 Hz–0.25 Fs / 0.25–0.40 / 0.40–0.45:

| shape | 44.1 kHz | 48 kHz |
|---|---|---|
| bell, all Q | 1.33 / 0.93 / 1.22 | 0.85 / 0.69 / 1.24 |
| shelves, Q ≤ 2 | 0.30 / 0.51 / 0.87 | 0.31 / 0.49 / 0.85 |
| shelves, all Q | 1.24 / 4.14 / 6.22 | 1.41 / 5.20 / 5.44 |
| low cut (−60 dB floor) | 1.79 / 1.61 / 2.88 | 0.89 / 1.52 / 2.61 |
| high cut (−60 dB floor) | 0.23 / 0.63 / 1.25 | 0.23 / 0.62 / 1.14 |

The cookbook bilinear design is 3–30 dB out on the same grid.

CPU at 48 kHz, 128-sample blocks, stereo (one machine; informative only):
24 static bands 37 µs/block (1.4 % of real time), 24 dynamic 114 µs (4.3 %).
A redesign costs 135 ns (bell) to 300 ns (shelf) with the engine's prebuilt
`DesignGrid`; without it a shelf was 2.4 µs.

## Open

**Waiting on `main`:**

1. **The rename** that turns BMO Parametric into BMO DEQ: plugin code, bundle
   id, preset extension, module id and the accent row all come from that
   change. Do not allocate any of them here.
2. **The parameter-limit exemption.** `SlotParameter` is a fixed 8 × 32 grid,
   and `RackTests` pins it. Whatever `main` does to exempt DEQ decides how
   `params.h` is shaped.

**Serial, and the listening test.** Chosen from `spec/topology-options.md`,
where it is the only option whose response is its band curves added in dB,
and the only one in which a low cut still cuts under an overlapping boost.
Parallel stays in `DspCore` only so `measure_deq render` can A/B it; delete
it and its tests when the test confirms serial. Serial has one property worth
knowing: **dynamic bands commute only while still.** Static bands are
order-independent to −300 dB. A dynamic band's moving coefficients do not
commute with its neighbours', so reversing the band order differs by −86 to
−88 dB on test material, −37 to −51 with fast overlapping bands, and −20 in
the test's deliberately extreme 24-band case (the regression bound). Any
serial dynamic EQ has this; the detectors reading the dry input keep it that
small.

**Engineering, not blocked:**

- **Resonant shelves** (Q > 2) are up to 6 dB out near Nyquist. Q > 2 on a
  shelf may simply not be offered.
- **External sidechain** is not possible through `ModuleDsp::process`, which
  takes the audio channels only. Needs a core interface change.
- **Linear ranges only** (`ParamSpec`, and `RackTests` pins it): a continuous
  20 Hz–20 kHz frequency knob needs a log mapping, which means a normalised or
  octave-valued parameter, or a core change to both range implementations.
- **Parameters arrive once per block** (`ModuleDsp::setParams`). The spec's
  "sample-accurate automation within a block" (T8) is not available; changes
  glide from the block boundary.
- **T4 zipper metric** (≤ −80 dB excess energy) is not implemented; the
  modulation tests assert stability, boundedness and gain-step overshoot.
- **T9 SIMD across bands** not attempted. The per-sample loop is scalar.
- **Bell precision at f0 < 3e-4 Fs**: 2e-6 dB off the knob gain from
  cancellation in the zero fit. Inaudible; fixable by computing 1 ± a1 + a2
  from the pole radius and angle rather than from a1, a2.
- The spec's **C6** forbids linear-phase FIR oversampling, and
  `core/dsp/Oversampler.h` is exactly that. Irrelevant until something in this
  module wants to oversample.
