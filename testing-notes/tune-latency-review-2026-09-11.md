# Review: the 4 ms rest, the correction lag, and a broken latency rule

Written on **AURORA**, 2026-09-11, reviewing `bmo-tune-work` at `9b18577`
(rounds three and four, `testing-notes/tune-handoff.md`). Everything below was
re-measured on AURORA against that commit; nothing is quoted from the earlier
notes without checking it.

Two findings. The first explains why Antares and Waves still clear BMO on
every group but one. The second is a live violation of the latency rule that
both gates were blind to.

## 1. The rest is an alignment, and 4 ms only aligns one pitch

**The mechanism.** `TuneCore::process` pushes the detector the newest sample
while the engine reads `rest` samples behind it. So the correction applied to
any output sample was computed from input the detector saw a while ago, and
applied to input the engine is reading somewhere else. The residue off the
note is the difference:

```
out - target  =  (detector's analysis lag - engine's read delay) x pitch slope
```

The detector's analysis lag is **one period**, not a constant: `fullNsdf`
correlates a one-period window against a block one period older
(`Detector.cpp`), and the hop is a quarter period. The engine's read delay is
`contract::kLiveRestMs`, a flat 4.0 ms.

**The evidence, from numbers already in the tree.** From the per-segment table
in `latency-and-lag-2026-09-11.md`, at the old 0.40 ms rest:

| note | T (ms) | BMO lag | BMO delay | sum | **sum ÷ T** |
|---|---:|---:|---:|---:|---:|
| A2 | 9.09 | 8.95 | 0.85 | 9.80 | **1.078** |
| D3 | 6.81 | 6.41 | 0.66 | 7.07 | **1.038** |
| E3 | 6.07 | 5.52 | 1.01 | 6.53 | **1.076** |
| A3 | 4.55 | 3.98 | 0.89 | 4.87 | **1.071** |

Flat to 4 % over two octaves. The lag the shoot-out measures is one period
less the rest.

**The evidence, by experiment.** `kLiveRestMs` set to 8.0, nothing else
changed, rebuilt, `hardtune --target` re-run on AURORA:

| vibrato | at rest 4.0 | predicted at 8.0 | **measured at 8.0** |
|---|---:|---:|---:|
| A2 | 6.069 | 2.069 | **2.116** |
| D3 | 3.392 | −0.608 | **−0.606** |
| E3 | 2.478 | −1.522 | **−1.537** |
| A3 | 0.807 | −3.193 | **−3.203** |

Adding delay subtracts from the lag 1 for 1, to within 0.05 ms on every note.
It is pure alignment; no splice, window or "room to move" term appears.

**Why a constant cannot be the fix.** RMS off the note at the same two rests:

| rest | A2 | D3 | E3 | A3 | mean |
|---|---:|---:|---:|---:|---:|
| 4.0 ms | 5.85 | 3.32 | 3.27 | **0.95** | 3.35 |
| 8.0 ms | 2.10 | 0.82 | 2.09 | **3.13** | 2.04 |

A3 gets 3.3x worse. A constant rest picks which octave to sacrifice. It pays
the debt in full at about 290 Hz and nowhere else — which is why the lag
tracks the period, and why Fuji cleared while Failure did not.

**How the others solve it.** Same table, same renders:

- **Waves' in-tune delay is T + 1.26 ms** (1.41, 0.88, 1.38, 1.38 above the
  period at the four notes). It rests one period back — period-proportional —
  and lands at 0.92–2.13 ms of residual lag. That is what its large ceiling
  buys.
- **Antares' analysis lag is a flat 4.42 ms** (lag + delay = 4.66, 3.82, 4.53,
  4.65; spread 0.84). A constant delay aligns Antares at every pitch because
  its detector's lag is constant. Copying a constant from Antares cannot work
  for a detector whose lag is a period.

## 2. The splice curve was the wrong thing to tune against

`LatencyContract.h` chose 4 ms as "where the splice curve flattens": 120, 50,
35, 31 splices for rests of 0.40, 2, 4, 6 ms. But under a **sustained**
correction the splice rate has no rest and no window width in it. The pointer
drifts `|1 - rho|` per sample and each splice moves it exactly `T`, so

```
splices per second = |1 - rho| x fs / T
```

Widening the window removes *transient* excursions only. On Failure the singer
sits on D# in D major — permanently ~50 cents from either allowed note — so
`|1 - rho| ~ 0.029` is sustained; at T ~ 284 samples that is ~4.5 splices/s
whenever that note is held. 35 → 31 is the count asymptoting to that floor,
not a benefit running out.

So 4 ms was set by a metric that had stopped responding, while the metric that
was still falling steeply — the lag, 3.19 → 1.20 from 4 to 6 ms — was read as
a side benefit. **The remaining 38 pops on Failure will not yield to more
window.**

The fix was already written down and then not taken: `modules/tune/AGENTS.md`
said *"delay the audio so the estimate is on time — which the latency rule now
allows"*. `31b30ef` did delay the audio, but as a constant, and was written up
as a splice fix, so nothing downstream checked it against what it was actually
doing and `hardtune_target` stayed off.

## 3. The latency rule is broken today, and both gates missed it

`bmo-tune-latency --range all` at `9b18577`, correcting worst per cell against
Waves' 10.62 ms:

| range | cells over | worst |
|---|---:|---|
| Auto | 11 of 50 | 15.33 ms at E2 |
| Bass | 18 of 39 | 15.33 ms at E2 |
| Instrument | 18 of 61 | 15.33 ms at E2 |
| Alto/Tenor | 7 of 40 | 13.44 ms at G#2 |
| Soprano | 0 of 38 | — |

**54 cells over**, including A2 and D3 — the two notes the stimulus is scored
on. At E2 even the correcting *mean* is 11.33 ms. Why neither gate saw it:

- **`bmo-tune-latency` tested the wrong column.** It compared `row.restMs` —
  the *in-tune* delay, 4.00 ms in every cell — against
  `kWaves.trueLatencyMs`, which `References.h` documents as "worst delay, in
  tune **or** correcting". It computed `worstMs` and never tested it, so it
  printed `every cell's rest delay is under the ceiling` and exited 0. Fixed
  in this commit; it now fails.
- **The stimulus has no corrected note below D3.** `HardTuneTests` reads
  6.53 ms because the only marked (held off-pitch) segments are A3 and D3, and
  the envelope cross-correlation reads a correlation peak over a held note
  rather than a maximum. Fixed in this commit by adding a marked A2.

**The cause is `hi = rest + T`** in `ClassicEngine::process`: an absolute
delay of rest plus a whole period, on a rest that is already 4 ms. Waves does
not do this — its correcting delay (7.33–10.62) barely exceeds its in-tune
delay (5.93–10.50). The upper bound wants to be an excursion above the rest,
not a period on top of it.

## What this means for the fix

**The delay route is dead.** There is no headroom to spend at the bottom of
the range; there is a deficit. Aligning by delay alone would need rest ~ 1.07 x
T — 9.7 ms at A2, 13.4 ms at E2 — before the window's own excursion is added.

I tried it anyway, to price it: `rest = 1.14 x T` with a +/- T/2 window took
the mean lag from 3.19 to **0.54 ms** and the true latency to **13.44 ms**,
failing the rule. It also exposed a third thing worth knowing:

**The engine has no mechanism to seek a rest.** The window only *bounds* the
read pointer; nothing drives it to a target delay. Homing exists but is gated
on `settled` — unvoiced **and** correction faded (`TuneCore.cpp`) — so inside
a continuous phrase the delay is a free-running consequence of correction
history. In that run the in-tune delay at D3, E3 and A3 never moved off
4.4 ms at all. This is also the likeliest reason Failure and Fuji differ:
Failure is denser, so the pointer rarely re-homes and sits at an arbitrary
offset for whole phrases.

**Prediction is the route that is left.** The analysis lag is ~1.07 x T and
known at run time, so `CorrectionLaw` can extrapolate `pitchIn` forward by it
at no latency cost. Not started; it needs a failing test and then Frosty's
ears, as everything here does.

**And the rule itself needs settling.** The ceiling is a scalar, but what it
bounds is pitch-dependent for every tuner in the comparison: Waves' own delay
is T + 1.26 ms, and 10.62 ms is that evaluated at A2, the lowest note the
stimulus holds. A change that beats Waves at every note can still fail a
scalar taken at one. That is Frosty's call, not a session's.

## What this commit changes, and what it does not

Changed: the documentation that was wrong (below), the `bmo-tune-latency`
gate, the stimulus, and the `HardTuneTests` ratchet. **No DSP was touched.**
The engine, the detector and the correction law are byte for byte as they were
at `9b18577`.

Corrected claims, all of which had gone stale when `31b30ef` landed:

| where | said | is |
|---|---|---|
| `README.md` | costs 0.4 ms; 3.8 ms at worst; "the least late of the three tuners" | 4.0 ms; 6.53 ms on the stimulus; **later than Antares' 6.49** |
| `ClassicEngine.h` | rests 19 samples, 0.4 ms | 192 samples, 4.0 ms |
| `TuneCore.h` | runs 0.4 ms behind at rest | 4.0 ms |
| `modules/tune/AGENTS.md` | Live floor 0.40 ms, meets spec's <= 1.5 ms | 4.00 ms, **fails** that gate |
| `modules/tune/AGENTS.md` | true latency 3.82 ms; lag 6.22 ms mean | 6.53 ms; 3.19 ms mean |
| `modules/tune/AGENTS.md` | `bmo-tune-latency` "checks it per semitone" | it checked the rest, not the rule |
| root `AGENTS.md` | (cited by three files as holding the latency rule) | did not mention it at all; now does |

Minor, also corrected: `LatencyContract.h` said 35 splices at 4 ms where the
handoff says 38, for the same configuration; and "every voice cell resting at
exactly 4.000 ms" holds at 48 kHz only — at 44.1 kHz, which both shoot-out
takes are, `liveRestSamples` gives 176 samples = 3.991 ms.

## Pick up here

1. **Prediction**, per section "What this means for the fix". Failing test
   first (`hardtune_target` is already the shape of it), then ears.
2. **Bound the window** so the correcting delay stops exceeding the rest by a
   whole period. This is what clears the 54 cells, and it is separable from
   the alignment work — do it first, since the rule is broken now.
3. **Decide whether the ceiling is a scalar or a curve.** Frosty's call.
4. **The 4 ms rest has still never been felt.** Unchanged from the handoff:
   the blind sets align it away and the installed VST3 on AURORA is 0.1.
