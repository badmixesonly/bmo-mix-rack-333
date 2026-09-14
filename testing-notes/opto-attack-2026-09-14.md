# BMO Opto — attack at heavy reduction, measured

Written on **AURORA**, 2026-09-14, for the `opto-high-gr` workflow in
`WORKFLOWS.md`. Frosty asked, before the 0.2.4 listening pass, about "attack
times on heavy gain reduction of 10 dB plus". This is what the shipped cells
actually do, measured on the shipped `modules/opto/dsp/Detector.h` at
`cbd0939`, and two candidates rendered for his ears. Nothing here is a change
to the tree.

## Method

A step: 1 kHz at −50 dBFS for 1 s, then −6 / −12 / −18 dBFS for 1.5 s, then
−50 again. 48 kHz, mono, 512-sample blocks, LINK off, LEVEL 0, Tele's drive on
(it has no off switch), Stressed's COLOR off. Reduction is dry minus wet in
0.5 ms RMS windows. The harness is a 150-line C++ file that includes the real
`DspCore.h`; it lives in this session's scratchpad and is worth lifting into
`tools/measure/opto` as an `attack` command when that tool is next opened.

## What ships

GR in dB reached N ms after the step. `t63`/`t90` are ms to reach that
fraction of the settled reduction. `leak` is how far the peak of the first
2 ms of output sits above the settled output peak.

| mode | CRUSH | in | final GR | 1 ms | 2 ms | 5 ms | 10 ms | 20 ms | 50 ms | t63 | t90 | leak |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Tele | 50 | −12 | 7.7 | −0.1 | 0.1 | 2.9 | 5.2 | 6.5 | 7.1 | 9 | 32 | 7.8 |
| Tele | 75 | −12 | 12.3 | 1.3 | 4.1 | 8.1 | 10.2 | 11.2 | 11.7 | 5 | 17 | 12.4 |
| Tele | 100 | −12 | 17.2 | 6.6 | 9.9 | 13.8 | 15.6 | 16.3 | 16.7 | 3 | 10 | 17.2 |
| Tele | 75 | −18 | 8.5 | 0.1 | 0.9 | 4.0 | 6.2 | 7.4 | 8.0 | 8 | 27 | 8.5 |
| Stressed | 50 | −12 | 9.7 | 0.0 | 0.0 | 0.5 | 3.3 | 6.1 | 8.4 | 21 | 66 | 9.7 |
| Stressed | 75 | −12 | 16.5 | 0.0 | 1.4 | 6.4 | 10.0 | 12.9 | 15.2 | 11 | 42 | 16.5 |
| Stressed | 100 | −12 | 23.3 | 4.1 | 7.9 | 13.2 | 16.8 | 19.7 | 22.0 | 7 | 31 | 23.3 |
| Stressed | 75 | −18 | 11.1 | 0.0 | 0.0 | 1.4 | 4.7 | 7.5 | 9.7 | 18 | 59 | 11.1 |

Three things follow, and the first two are what the ear will hear.

1. **The first millisecond or two of every onset passes at unity gain,
   whatever the reduction.** `leak` equals the final GR in every row: a
   one-pole with a 10 ms time constant has done nothing by the first peak.
   At CRUSH 75 on a −12 dBFS peak that is a 12 dB (Tele) or 16 dB (Stressed)
   spike on the front of each word relative to the settled level. On a vocal
   it reads as a tick or a spit on hard consonants. Every compressor without
   lookahead does this; the LA-2A does it; it is louder here because CRUSH
   at 75 is deep. Only lookahead removes it, and Opto declares zero latency.

2. **The reduction arrives slowly at the drive a vocal actually sees, and
   Stressed is slower than Tele.** Both cells have the same 10 ms attack
   constant, but Tele is a feedback loop, which makes its effective time
   constant `tau / (1 + slope)` = 3.3 ms at 3:1, while Stressed is
   feedforward and gets the full 10 ms. So at CRUSH 50 on a −12 dBFS peak,
   Tele reaches 63 % of its reduction in 9 ms and Stressed in 21 ms; 90 %
   takes 32 ms and 66 ms. The checklist describes Stressed as "grabbier";
   on attack it is the opposite. Both real units are faster than this at
   the level they are usually hit at.

3. **The reduction keeps creeping for hundreds of milliseconds.** The
   detector rectifies and compares each sample to the envelope, so the
   envelope only rises on the crests of each cycle and sags between them
   at the release rate; as `chargeDb` builds over 300 ms the release
   lengthens, the sag shrinks, and the envelope creeps up. That is why the
   50 ms column is still short of the final figure in every row. In Tele
   it is the T4 cell's character; in Stressed it is a side effect.

The release, read from 20 ms after the hold ends (past a small DC blip from
Tele's drive stage, see below), matches the 0.2.1 handoff: Tele at CRUSH 50
is half released in 0.33 s and 90 % in 0.74 s; at CRUSH 100 it holds 50 %
for 1.1 s. Stressed at CRUSH 75 and above still holds more than half its
reduction 2 s after a 1.5 s hit at −12 dBFS, which is the dosage memory the
handoff describes and is by design.

**A DC blip, for the record, not for action.** Tele's drive stage adds an
even-order term and DC-blocks it at 20 Hz. When a loud passage ends, the
blocker's stored offset comes out as an 8 ms sub-20 Hz transient around
−56 dBFS. Inaudible; it only matters because it makes a naive release
measurement in the first 20 ms read wrong.

## Two candidates, rendered for the ear

Neither is in the tree. Both are one-line changes to `Detector.h`, patched
into scratchpad copies and measured with the same harness.

- **Candidate A — Stressed's attack constant 10 ms → 3 ms.** Tele untouched.
  The Distressor is a VCA unit whose attack is a knob; its Opto setting is
  about release. Stressed at CRUSH 50 / −12 then reaches 63 % in 6 ms and
  90 % in 20 ms (was 21 / 66), and matches Tele's speed at every setting.
  Leak unchanged.
- **Candidate B — both cells get a light-dependent attack**: the time
  constant is `10 ms / (1 + overdrive / 10 dB)` with a 1 ms floor, where
  overdrive is how far the rectified sample sits above the envelope. A CdS
  photocell does respond faster the harder it is lit, so this is the
  physically defensible version for Tele. Tele at CRUSH 75 / −12 reaches
  63 % in 2 ms and 90 % in 10 ms (was 5 / 17); at CRUSH 100 the leak drops
  17.2 → 14.5 dB. Stressed moves about half as far as under A. At CRUSH 35
  nothing changes, because there is little overdrive.

| | Tele 75 / −12 t63 / t90 | Stressed 50 / −12 t63 / t90 | Stressed 75 / −12 t63 / t90 |
|---|---|---|---|
| shipped | 5 / 17 ms | 21 / 66 ms | 11 / 42 ms |
| candidate A | 5 / 17 | 6 / 20 | 3.5 / 12.5 |
| candidate B | 2 / 10 | 11.5 / 50 | 5.5 / 29.5 |

**Blind set:** `field-audio/opto-attack-2026-09-14/` in the main worktree
(gitignored). Two groups, Tele 75 and Stressed 75, three letters each, the
Failure take through each variant, LINK on, COLOR off, RMS-matched to the
dry. `ANSWERS.md` is the form, `KEY.txt` the decode. Max reduction on that
take is 16.3 dB in Tele and 20.8 to 22.4 dB in Stressed, so this is the
"10 dB plus" case Frosty asked about.

## What this does not settle

Whether any of it should change is Frosty's call, on those renders. The
notes say the 10 ms attack has "no source supporting it moving"; candidate B
is the case that a photocell's attack does move with light, and candidate A
is the case that the Distressor's does not need to be slow at all. If a
candidate wins, it goes in as a constant (A) or a five-line change (B) in
`Detector.h`, with `testAttackReachesReductionInTime`-style absolute
assertions on the table above, and the thirteen preset levels re-solved
with `BMO_PRINT_PRESET_LEVELS` because a faster attack takes slightly more
average gain.

---

# Heard, 2026-09-14, on AURORA — candidate B wins, and the control fired

Frosty's rankings, written before `KEY.txt` was opened; the sheet is
`field-audio/opto-attack-2026-09-14/ANSWERS.md`. Decoded:

| group | 1st | 2nd | 3rd |
|---|---|---|---|
| Tele 75 | **candidate B** "best, slight advantage" | candidate A "almost best, tie" | shipped "slight pumping" |
| Stressed 75 | **candidate B** "best" | shipped = candidate A, tied | — |

**Candidate B placed first in both groups.** Candidate A never beat the shipped
build anywhere it differs from it.

## The Tele group is a null, and it says so itself

Candidate A changes the Distressor cell only, so in **Tele** it is the shipped
build. The two files were rendered separately and are byte-identical:

    Tele 75/B.wav  4034f625b1f48e2e89d3e848c51c39d2b2fa4620f6210c22bdea74c3da6d7e29
    Tele 75/C.wav  4034f625b1f48e2e89d3e848c51c39d2b2fa4620f6210c22bdea74c3da6d7e29

They were ranked 3rd ("slight pumping") and 2nd ("almost best, tie"). One file,
two placings, one position apart — which is the same margin that separated
candidate B from them. **So the Tele separation is at or below the resolution
of this comparison and cannot carry a decision.** That is the control working
exactly as a control is supposed to: it did not tell us the listener is
unreliable, it told us this particular difference is too small to rank, which
is a fact about the change and not about the ear.

## The Stressed group is real

Three genuinely different files, and the ranking is 1 / 2 = 2:

- **candidate B best** — the light-dependent attack, both cells.
- **shipped and candidate A tied for second.** Candidate A is the Distressor
  attack at 3 ms against the shipped 10 ms, which is the larger change of the
  two on paper (t63 11 ms → 3.5 ms, t90 42 ms → 12.5 ms). Heard flat against
  the shipped build.

That last line is the finding worth keeping: **the big simple number did
nothing and the small programme-dependent one was heard.** Candidate A makes
the cell uniformly faster; candidate B is fast only on the first millisecond of
a hit and back to 10 ms as it settles. What was audible was not "faster", it
was "faster *at the onset only*".

## Recommendation

Take candidate B, on one group's evidence and a null in the other. It won where
the comparison is trustworthy, it was never ranked below the shipped build in
either group, and it is the variant with the physical argument behind it — a
photocell's attack does move with light.

It is not a strong result. One confirming round on Stressed alone, three
letters, shipped against candidate B twice, would settle whether the margin
survives a second hearing — and would cost one render pass. Worth it before the
thirteen preset levels are re-solved, since that re-solve is the expensive part
and is wasted if the margin evaporates.

**Not implemented.** Nothing in any tree has changed. When it goes in it is the
five-line change in `Detector.h` described above, with absolute attack
assertions on the t63/t90 table and the thirteen preset levels re-solved with
`BMO_PRINT_PRESET_LEVELS`, because a faster attack takes slightly more average
gain.
