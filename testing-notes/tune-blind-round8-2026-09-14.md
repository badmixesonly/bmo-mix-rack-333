# Blind listening answers -- round eight: the detector's period discipline

**Audio in `field-audio/blind-2026-09-14-round8/`** (gitignored -- open from
Explorer). `KEY.txt` sits beside it; don't open it until this is filled in.

Three letters, four groups. Built on **AURORA**, 2026-09-14. One of the three
is Antares, as an anchor; the other two are BMO, and they differ by one thing.

## What changed, and why this is the question

Round seven closed the rest as an axis -- all four arms unacceptable, all four
indistinguishable -- and in closing it, it pointed at what was left. Five of
the six pops you timestamped fire at the **same millisecond at every rest**,
and in the 40 ms before each one the detector's own f0 spans a ratio of 1.76
to 3.94, against 1.00 to 1.02 before the quiet ones.

Instrumenting the coarse search on Failure at **17.409 s** found the line. The
scan breaks out as soon as any lobe's raw correlation clears 0.95 -- before the
continuity weighting, before McLeod's peak-fraction rule, before every octave
guard. The coarse window *is* the lag, so at a short lag it is about 1.3 ms,
roughly one cycle of the vowel's first formant, and that rings at 0.96. At
17.409 s the candidate list had **one entry**, 787.5 Hz. The real period,
219 Hz at 0.974, was never scored at all.

787.5 is not a harmonic of 219 -- it is 3.67x -- so guard 4 could not climb
back either; it moves in steps of 2 and 3. And losing the period halves the
hop to 0.5 ms, after which continuity defends the wrong lag and the law's
"confirm a jump by the next estimate" is satisfied half a millisecond later.
The collapsed period shuts the engine's read window under the pointer, and it
splices by a quarter of the real cycle. **That step is the pop.**

Guard 6, the arm being tested: a leap to a shorter period, while a note is
held, has to beat the period being held on a common window long enough to
judge them both. Upward in pitch only -- vetoing the other direction latches
the very fault it exists to stop, measured. Two more things went with it: the
early exit is retired (it saves nothing -- every lag's correlation is computed
before the scan begins), and the low-band sums now step, so the guards cost
the same at every sample rate.

| | BMO now | BMO guard 6 |
|---|---:|---:|
| Failure -- splices taken while the detector had lost the period | 12 of 37 | **2 of 27** |
| Failure -- on the note | 90.2 % | 92.8 % |
| Failure -- detector jump flips | 65 | 7 |
| Fuji -- splices taken while the detector had lost the period | 4 of 16 | **2 of 15** |
| corpus mean gross error | 1.9344 % | 1.9344 %, no item worse |

Of the six splices you timestamped, **2.893, 14.455, the 16.1-16.8 cluster and
17.410 no longer happen**, and 7.019's landing error goes 0.58 to 0.07. One
remains, at 6.137 s, and it is a different fault -- a phrase end where clarity
falls to 0.05 and the detector keeps tracking noise. That is the voicing item,
not this one.

**But measurement has now been wrong three times about what you would hear** --
splice count, splice landing error, and the rest. So the numbers pick the
candidates and you pick the winner. Two of the three files are BMO; one is
Antares. Nothing tells you which.

## What to listen for

**Pops on Failure.** Your words, and the worst thing left. If guard 6 is real
you should hear materially fewer of them, at both speeds, and the ones that
remain should be in different places.

Also: tracking through scoops, anything that sounds processed on held notes,
and whether anything new has appeared at phrase ends -- guard 6 holds the
period for up to 2 ms when it vetoes, and a phrase end is where that hold is
most likely to be wrong.

The files are aligned and level-matched, so only the tuning compares. The
latency cost of this change is nil; the measured cost is 0.05 ms of correction
lag and 0.02 cents of vibrato residue at A2, and that is a separate question
waiting on this answer.

## Failure 0 ms

Ranking (best to worst): 

- A: 
- B: 
- C: 

## Failure 20 ms

Ranking (best to worst): 

- A: 
- B: 
- C: 

## Fuji 0 ms

Ranking (best to worst): 

- A: 
- B: 
- C: 

## Fuji 20 ms

Ranking (best to worst): 

- A: 
- B: 
- C: 

## Anything else

Fewer pops than you are used to on any of them? Which, and how many fewer? 

Are any two indistinguishable? Which? 

Anything new or worse at phrase ends? 

The single worst thing left: 

Listened on: AURORA, 
