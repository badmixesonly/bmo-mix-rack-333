# Blind listening answers -- round seven: how low can the rest go

**Audio in `field-audio/blind-2026-09-13-round7/`** (gitignored -- open from
Explorer). `KEY.txt` sits beside it; don't open it until this is filled in.

Four letters, three groups. **Every arm is the same build.** The only
difference is the rest -- how far behind the input the engine reads, which is
the latency.

## Why this is being asked again

You said audible splices are the biggest enemy and low latency is the highest
priority. Those two trade against each other along exactly one number, and
that number's setting is out of date.

The rest is 4 ms because in round four 0.4 ms gave 120 splices against 38.
But that was measured **before the prediction existed**, when the rest was
doing two jobs: window room *and* aligning the read with the detector. It is
not doing the second any more. At a 0.4 ms rest the correction lag is now
**1.01 ms, where it used to be 6.21**. Low latency no longer costs tracking,
so the question is open again on different terms.

Measured, current build, four rests:

| rest | true latency | correction lag | vibrato residue | splices (Failure 20 ms) |
|---|---:|---:|---:|---:|
| 0.4 ms | **6.77 ms** | 1.01 ms | 2.33 c | **123** |
| 1 ms | 7.39 | 1.68 | 2.29 c | 72 |
| 2 ms | 7.55 | 1.38 | 1.89 c | 49 |
| 4 ms (now) | 10.26 | **0.71** | **1.24 c** | **38** |

Dropping to 0.4 ms takes 3.5 ms off the latency and clears one more cell of
the Waves curve. It triples the splice count.

**And the splice count cannot answer whether that matters.** It has now failed
twice as a predictor of what you hear: 120 -> 38 across round four with no
audible change, and the landing-error measure built to replace it improved
while round six got worse. Only your ears have been right so far.

## What to listen for

**Which of these is usable, and where it stops being usable.** They differ in
one thing. The lowest-numbered arms have more splices; the question is whether
you can hear them, not whether they exist.

Rank all four per group, and say where the line is -- the lowest-latency arm
you would put on a record. If two are indistinguishable, say so: that is the
most useful answer of all, because it means the latency is free.

**You will not feel the latency here** -- the files are aligned so only the
tuning is compared. Feeling it is the Ableton pass, which has still never been
done and is the other half of this question.

## Failure 0 ms

Ranking (best to worst): 

- A: 
- B: 
- C: 
- D: 

Lowest-latency arm you would still use: 

## Failure 20 ms

Ranking (best to worst): 

- A: 
- B: 
- C: 
- D: 

Lowest-latency arm you would still use: 

## Fuji 0 ms

Ranking (best to worst): 

- A: 
- B: 
- C: 
- D: 

Lowest-latency arm you would still use: 

## Anything else

Are any two indistinguishable? Which? 

The single worst thing left: 

Listened on: AURORA, 
