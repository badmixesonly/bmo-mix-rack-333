# Blind listening answers -- round nine: the phrase end

**Audio in `field-audio/blind-2026-09-14-round9/`** (gitignored, in the main
worktree; open from Explorer). `KEY.txt` sits beside it; don't open it until
this is filled in.

Three letters, four groups, the same four as round eight. Built on
**AURORA**, 2026-09-14, branch `tune-phrase-end` off `review-0.2.4`. One of
the three is Antares, as an anchor; one is the shipped 0.2.4 build (guard 6,
the round-eight winner); one is guard 6 plus one change.

## What changed, and why this is the question

Round eight left one pop standing at **6.137 s** on Failure, and the note
called it "a phrase end where clarity falls to 0.05 and the detector keeps
tracking noise -- the voicing item." The 0.2.4 review read the analysis
dump at that moment and found the mechanism, and it is not voicing.

When clarity drops under the detector's floor the period **freezes** where it
was (spec §3.4), but the detector keeps re-presenting that frozen period to
the correction law every evaluation as if it were new. The law confirms a
note jump by "the next estimate agreeing with it" -- and the previous
estimate, echoed back, always agrees. So at a phrase end, where the last
thing the detector read was a formant lobe, a leap of an octave or more was
confirmed by its own echo: at 0 ms the note zips D4 → C#4 → D5 → A5 → D3
inside 20 ms and the engine splices on each, one of them (6.146 s) the
worst-landing splice in the whole take at 1.95. At 20 ms only the first
splice fires.

**The change:** a frozen period is not an estimate. The detector marks each
estimate `fresh` when its period was measured this evaluation, and the law
ignores the hold: the pitch, the note and the correction stay what they were
until a real measurement arrives or voicing closes. Two lines of arithmetic,
no constant, no latency.

| | 0.2.4 (guard 6) | phrase end |
|---|---:|---:|
| Failure 0 ms -- worst splice landing | **1.95** (6.146 s) | **0.76** |
| Failure 0 ms -- splices over 0.5 | 4 of 33 | 3 of 33 |
| Failure 0 ms -- LOST the period | 4 of 33 | 3 of 33 |
| Failure -- detector jump flips (both speeds) | 7 | 5 |
| Failure -- note-name changes | 197 | 180 |
| Failure -- dropouts | 12 | 12 |
| Failure 20 ms -- splices | 27 | 28 |
| Fuji 0 ms -- worst landing / splices | 1.54 / 11 | 0.79 / 10 |
| Fuji 20 ms -- worst landing / splices | 1.55 / 9 | 0.95 / 8 |
| Fuji -- neighbour flips | 41 | 48 |
| corpus mean gross error (72 items) | 1.9344 % | 1.9344 %, no item worse |
| corpus note changes / splices | 1172 / 195 | 952 / 192 |

The 6.146 s splice no longer happens. The one figure that moved the wrong
way is Fuji's neighbour flips, 41 → 48: with the echo gone, a note that used
to be pinned by its own frozen period during a soft syllable can now be
re-decided when the real measurement comes back. Whether that is heard as
steadier or as busier is what this round is for. One ON NOISE splice on
Failure 20 ms (10.752 s, −45 dB) lands worse, 0.69 → 1.04; it is in a quiet
gap and the field tool's scoring of quiet splices is itself on the review's
list.

**Measurement has been wrong three times about what you would hear**, and
was right once, in round eight. So, as before: the numbers picked the
candidate and you pick the winner. Nothing tells you which letter is which.

## What to listen for

**Phrase ends.** The end of the held D4 before the next phrase on Failure,
6.13–6.16 s, at 0 ms especially: a chirp or zip rather than a click. Any
held-note tail into breath or creak. Then the usual: pops anywhere,
tracking through scoops, anything that sounds processed on held notes.

On Fuji, whether soft syllables inside a word sound steadier or busier than
in round eight.

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

Fewer pops than round eight on any of them? Which?

Are any two indistinguishable? Which?

Anything new or worse at phrase ends?

The single worst thing left:

Listened on: AURORA,

---

## If the candidate wins

It ships as it is on `tune-phrase-end`: `PitchEstimate::fresh` in
`Detector.h`, set once per evaluation in `Detector::evaluate`, and the gate
in `CorrectionLaw::tick`. `tune_correction`'s hand-built estimates are
measurements and default to fresh. The ratchet constants in
`HardTuneTests.cpp` did not move (8 of 8 tune suites green on the branch).
Still open on the same phrase-end mechanism, and deliberately not in this
arm so the round tests one thing: the voicing release counter is zeroed by
any single frame in the 0.60–0.85 band (`Detector.cpp:328`), so a decaying
note whose clarity flickers to 0.61 never closes. That is the next
candidate if this one is heard as better.
