# Handoff: BMO Tune RT after rounds five and six

Written on **AURORA**, 2026-09-13, at the end of a session that reviewed the
2026-09-11 handoff, found what was wrong with it, fixed one thing, and
**measured four other things and threw them away**. This supersedes
`tune-handoff.md` for everything after `9b18577`; that file still has rounds
three and four and is still worth reading first for how the engine works.

Branch **`bmo-tune-work`**, off `integration`, worktree
`../bmo-mix-rack-333-tunework`. `WORKFLOWS.md` on `integration` has the branch
map and the stages.

---

## The short version

**The best-sounding build is still `31b30ef` -- "round four", the 4 ms rest,
no prediction.** Nothing built since has beaten it by ear. One change since
is a draw and stays in; three are losses and are out.

The day's real finding is not a fix. It is that **two different measures of
the pops both improved while the sound did not**, so neither is the thing,
and the next person should not bring a third.

---

## Which build won the blinds

Every round: Frosty on AURORA, Apollo Twin X gen 2 into HEDD Type 20 mk2,
answers written before the key was opened.

### Round five, 2026-09-12 -- the prediction

Antares, round four (`31b30ef`), and the prediction build (`b4bfc73`).

| group | order |
|---|---|
| Failure 0 ms | Antares > **prediction** > round four |
| Failure 20 ms | Antares > round four > **prediction** |
| Fuji 0 ms | round four > **prediction** > Antares |
| Fuji 20 ms | Antares > **prediction** > round four |

**Two all.** The prediction also drew the round's one new complaint,
"audible formant shift", on Failure 20 ms.

### Round six, 2026-09-13 -- the splice landing, and a note transition

Antares, round four, prediction + splice landing (`1a289c9`), and that plus a
1 ms note transition.

| group | order |
|---|---|
| Failure 0 ms | Antares > **round four** > landing > +1 ms |
| Failure 20 ms | Antares > (**round four** = landing) > +1 ms |
| Fuji 0 ms | (**round four** = Antares) > landing > +1 ms |
| Fuji 20 ms | **round four** > landing > Antares > +1 ms |

**Round four: 3 wins, 1 tie, 0 losses.** The 1 ms transition was worst of
four in all four groups.

### Standing

- **Best BMO by ear: round four (`31b30ef`).** Never beaten.
- **Current HEAD** is round four + the prediction. Against round four it is a
  draw (2-2, round five). It stays because it is a large measured improvement
  -- correction lag 3.19 -> 0.71 ms, vibrato residue 3.35 -> 1.24 cents,
  which meets Antares' 1.30 for the first time -- and costs nothing by ear.
  **It is not "earned" and must not be described as one.**
- **Antares wins Failure**, at both speeds, in every round it has been in.
- **BMO beats Antares on Fuji**, in both rounds, at both speeds.
- The worst thing left, in Frosty's words: **"Pops on Failure."**

---

## Avenues tested and failed

Do not re-derive these. Each cost most of a day between them, each has its
numbers in the commit that tried it.

| what | result | where |
|---|---|---|
| **A bigger constant rest** (8 ms) | Trades octaves against each other -- A2's residue 5.85 -> 2.10 c, A3's 0.95 -> 3.13. A constant pays the alignment debt at one pitch only (~290 Hz). Not shipped. | `2be99bb` |
| **A period-proportional rest** (0.5-1.25 x T) | Worse on every axis: latency no better, lag 0.71 -> 1.8-3.5 ms, residue 1.24 -> 2.9-3.8 c, splices 38 -> 63-74, and it clears one Waves cell of six. Mechanism in the tree, `kRestPeriods = 0`. | `5c58198` |
| **Note transition smoothing** (0.5-10 ms) | More splices, not fewer: 10 ms nearly doubles them. At 1 ms free but pointless. **Heard as worst of four in all four groups.** Kills the formant hypothesis. `noteTransitionMs = 0`. | `fc129b1` |
| **Splice similarity search** (WSOLA) | Landing error improved everywhere and the worst splice in the take went 1.97 -> 0.37. **Heard as 0 wins, 1 tie, 3 losses.** Reverted. | `1a289c9`, `14bbae7` |
| **Splice COUNT as the metric** | 120 -> 38 across round four with no audible change. Refuted. | `00d4333` |
| **Splice LANDING ERROR as the metric** | Built to replace the count. Improved; sound did not. Refuted. | `14bbae7` |
| **"The pops are dropouts being corrected"** | Frosty's hypothesis, tested: of 38 splices only 4 fall within 100 ms of a voicing gap. Fixing voicing leaves 34 of 38 untouched. | `00d4333` |
| **A full period of search reach** | Every period multiple correlates equally, so shimmer picks a different cycle: landing error on a *correct* period 3e-10 -> 0.13, splices 8 -> 15, sine THD+N to +46 dB. | `1a289c9` |
| **A one-pole on the prediction slope** | Only ever cost, monotonically (0.97 ms / 1.23 c at 0; 1.15 / 1.47 at 5 ms). `predictSlopeMs = 0`. | `b4bfc73` |
| **A flat 1.5-hop staleness correction** | Overshoots every vibrato into negative lag, residue back to 1.46 c. Replaced by anchoring the slope to the estimate it was measured at. | `b4bfc73` |

---

## What is true, and stands

1. **Every pop is a splice.** Frosty timestamped seven on Failure; all seven
   landed on one, six within 61 ms. There is no second mechanism.
2. **Only 7 of 38 splices are audible.** Four in five make no sound. This is
   why the count never tracked the pops, and it is still the central puzzle:
   **what separates them is unknown.**
3. **The correction was landing late because the read and the detector were
   not aligned.** The detector's estimate refers to 1.07 x T behind the
   newest sample; the engine read a flat 4 ms behind. The residue is
   `(analysis lag - read delay) x pitch slope`. Verified 1:1 by experiment.
   Fixed by prediction, which costs no latency.
4. **The latency rule is a curve, not a scalar.** Waves' delay is ~1.68 x the
   period. Held to one number it is wrong in both directions. `References.h`
   carries the measured curve; `ceilingMsAt` reads it.
5. **The rule is unreachable at the top of the range.** At A5 Waves' whole
   delay is 0.709 ms, less than one period (1.136 ms); BMO's floor plus one
   whole-cycle excursion is 1.491 ms. Arithmetic, not effort.
6. **Nothing holds the read at the rest.** It is a window edge and a homing
   target, and homing only runs on unvoiced settled material. Inside a phrase
   the read sits where correction history left it. This is why the
   period-proportional rest could not work.

---

## Open, in the order worth doing

1. **What makes a splice audible.** The blocker for everything else. Two
   metrics refuted. Start from the five splices Frosty hears that did NOT
   improve -- 1.560, 6.080, 7.002, 14.438, 17.360 s on Failure -- and ask
   what is physically different about them, rather than inventing a third
   number.
2. **The detector's octave and twelfth errors.** 15 of 38 splices sit within
   50 ms of a note change of 7+ semitones, and they are disproportionately
   the audible ones (5 of 15 against 3 of 23). Untouched. The engine cannot
   fix them -- the search reach that would is the one that thrashes.
3. **The live-monitoring budget.** Frosty's to set. Until it exists the
   per-note latency rule can never be green; it is now an open check under
   `--target` rather than a build blocker.
4. **`hardtune_target`'s last check**: worst correction lag 1.97 ms at A2
   against Antares' 1.66. Every other vibrato is inside half a millisecond.
5. **Bass and Instrument declare 55 Hz** while the latency curve stops at E2
   (Frosty, 2026-09-12: "it's a vocal tuner so no need to drop below E2").
   One of the two has to move. Low priority, Frosty's call.
6. **Waves across block sizes** -- measured at 128 and 2048 only, and the
   whole ceiling curve rests on it.

---

## Things that were wrong in the tree, now fixed

- **`field-audio/` was not gitignored.** The rule "field audio, blind sets and
  the licensed fonts are never committed" came across with Tune as prose; the
  `.gitignore` line did not. On a public fork. (`2d3af87`)
- **`bmo-tune-latency` tested the rest delay** against a ceiling documented as
  a worst case, so it passed whatever the engine did. (`7835096`)
- **The stimulus held a correction only on A3 and D3** -- a 2.3-octave plugin
  judged through a five-semitone window. Now E2 to A5. (`3c95284`)
- **The ruler's envelope was too short** for its own lowest note: 10 ms
  against A2's 9.09 ms period, reading an ideal corrector 0.31 ms out. Now
  25 ms with 40 ms markers; marked-segment correlations 0.78-1.00 -> 0.95-1.00.
- **The latency rule was cited to the root `AGENTS.md`** by three files and was
  not in it. (`2be99bb`)
- **The regression ratchet was two generations stale** (6.22 ms / 6.61 c, from
  before the 4 ms rest), so it would not have noticed that rest being
  reverted. Now 0.71 / 1.24.
- **The law copied the engine's rest once** at `applyParams`; correct only
  while that rest was constant. Both read `contract::liveRest` now.

---

## Rules that still hold

- Name the machine: AURORA (laptop, `C:\Users\thesp`) or ICE QUEEN (desktop,
  `C:\Users\stefr`).
- Measure, never judge by ear alone -- **and never by measurement alone**,
  which is this session's lesson. Nothing is fixed that a test did not first
  fail on; Frosty hears every fix before it is called fixed.
- The schema is frozen; new meaning gets a new id (`kRetiredIds`).
- No FFT in the correction path; nothing allocates after `prepare()`; the host
  is told 0.
- Field audio, blind sets and the licensed fonts are never committed -- and
  now `.gitignore` agrees.
- Pushes only with Frosty's say. A CI round trip is about 22 minutes.
