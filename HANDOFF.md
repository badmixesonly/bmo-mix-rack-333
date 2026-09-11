# Handoff: after the 2026-09-11 shoot-out and the first hiccup fixes

For the next session on BMO Tune RT. Written on **AURORA** at the end of the
2026-09-11 session that ran the shoot-out, measured the competition, and
fixed the worst of the hiccups. Frosty wants the next session to **review
the maths, the tests and the test architecture**, then **pick up the
testing where it stopped** -- round three of the blind listening.

Read first, in this order: root `AGENTS.md` (the latency rule is new),
`modules/tune/AGENTS.md` (the departures table has three new rows; "Open"
is current), `testing-notes/shootout-2026-09-11.md` (what was heard, three
rounds), `testing-notes/latency-and-lag-2026-09-11.md` (what was measured).

## Where everything is

| | |
|---|---|
| Code | this repository. Fork `badmixesonly/bmo-mix-rack-333`, branch **`bmo-tune-rt`**, pushed to `f9e479b`. Local `main` is **ahead**: `f2b0f8a` (steadier note, guard 4's later checks) and the commit that adds this file and `bmo-tune-field`. **Not pushed: Frosty's say first.** Never push to Kevin's repository. |
| Field audio | gitignored, on AURORA only: `field-audio/shootout-2026-09-11/` -- the drys and Antares/Waves exports (`failure/`, `fuji/`, the redo folders), and BMO renders by generation: `bmo-renders/` (0.1's detector), `bmo-renders-guard4/` (`989a5ef`), `bmo-renders-hold/` (`f2b0f8a`). The blind manifests are beside them. |
| Blind sets | `field-audio/blind-2026-09-11/` round one (answered), `-guard4/` round two (answered), **`-round3/` not yet heard.** |
| Reference stimulus | `field-audio/reference/stimulus-48k.wav` (regenerate with `bmo-tune-ref stimulus`). Frosty's Ableton bounces of it: `C:\Users\thesp\OneDrive\Desktop\BMO TUNE refs\`. |
| Checklist | the artifact "Tune RT Reference Run", https://claude.ai/code/artifact/beb2a260-931d-43a7-b431-ebbd563a267b -- pinned in Frosty's sidebar; rounds one and two ticked, round three added. It saves itself (the `artifact` capability): read it with the Artifact tool to see Frosty's ticks and calls. |
| Installed VST3 | AURORA still has 0.1 in `C:\Program Files\Common Files\VST3`. Frosty has not heard any of today's changes in Ableton, only in blind renders. |

## What happened on 2026-09-11

| | found | done | heard |
|---|---|---|---|
| Retune | a unitless knob; the shoot-out's "10 ms" BMO was 1.2 ms | `retune_ms`, 146 steps in ms (`fd714e7`) | -- |
| Latency | BMO 3.8 ms true, Antares 6.5 (reports 2.33), Waves 10.6 (reports 0) | the latency rule: never later than Waves (`9599b0f`) | -- |
| Hard tune | BMO's correction lands about a cycle late on a moving voice | `hardtune_target` holds it, disabled until fixed | -- |
| Round one | BMO last in 4 of 6 groups: pops, clicks, hunting, skips, weak phrase ends | -- | blind, Frosty |
| Detector | a weak-fundamental voice read an octave or a twelfth up, estimate swinging a semitone | guard 4 (`989a5ef`) | round two: better than 0.1 in every group, still behind Antares |
| Note decision | at vibrato 0, a singer between two scale notes flipped with every wobble | the dwell, "hold the note steadier" (Frosty's call); guard 4 refined (`f2b0f8a`) | **round three, not yet** |

## The measurement and test architecture -- for review

Everything below is in the repository. The house rule stands: measure,
never judge by eye or ear alone; nothing is fixed that a test does not
first fail on; and Frosty hears every fix before it is called fixed.

**The ruler** -- `tools/common/Analysis.h` `measureHz`: a full-segment
NSDF, McLeod's peak rule, then a golden-section search on the sinc-shifted
self-difference. Shares nothing with the plugin's detector, so it cannot
agree with it by construction. On a voice whose fundamental is under its
second harmonic it must be **kept to the voice's range** (60-400 Hz for a
low male) or it is fooled the same way (Failure 1.00 s: 606 Hz unbounded,
303 bounded). Its sinc kernel is now computed once per trial period (same
answer to ~1e-10 c, the suites several times faster).

**The reference stimulus** -- `tools/common/Stimulus.h`, 19.5 s at 48 kHz:
four in-tune held notes (A2 D3 E3 A3, with 30 % shimmer so a waveform
cross-correlation has one peak), four vibratos to flatten (40-45 c,
5.5-6.5 Hz), two notes held 30-35 c off and marked with 20 ms level dips.
- *True latency*: the worst of the in-tune delays (waveform
  cross-correlation) and the held-off delays (10 ms RMS envelope
  cross-correlation, which a pitch shift does not move).
- *Correction lag*: a correction L late leaves `out - target = L x slope`;
  per vibrato, `L = sum(dev * slope) / sum(slope^2)` over frames of the
  bounded ruler, the slope taken from the known contour at the output's
  delay. Negative means looking ahead.
- `HardTuneTests` first proves the ruler: a plain 2.5 ms delay reads 2.5 ms
  within 0.05; ideal correctors 0, 5 and 2 ms late (the last with 3 ms of
  audio delay) read their lag within 0.04 ms.

**References** -- `tools/common/References.h`: Antares Auto-Tune Artist
(Low Male) and Waves Tune Real-Time (Mono), each rendered through
`bmo-tune-hostrender` uncompensated at 48 kHz / 128, settings recorded.
Frosty's Ableton bounces (buffer 2048, Delay Compensation off) matched: Antares
to -105 dB, Waves bit for bit.

**Suites** (`scripts/build.sh`): kernel, detector, interpolator,
correction, core, schema, **hardtune** (the latency rule every run; the
regression guard on lag), **voice** (weak fundamental, `VoiceSettings::
fundamentalDb`, 15 cases + a held D4 through the whole plugin), and
`hardtune_target` (disabled, fails today by design: as close as Antares).
`--plugin` adds panel and hostcheck. `--corpus` scores 72 synthetic items.

**Tools**:

| tool | what for |
|---|---|
| `bmo-tune-ref` | writes the stimulus; scores any render of it; `bmo` renders and scores BMO |
| `bmo-tune-hostrender` | any VST3, uncompensated, no pre-roll unless `--preroll` (pre-roll shifts BMO's detector grid). Auto-Tune's choices only take `--setn` (Low Male = 0.5) |
| `bmo-tune-blind` | a blind set from a manifest: onset-aligned (within 0.11 ms), level-matched, 24-bit, shuffled letters, `KEY.txt` apart |
| **`bmo-tune-field`** | new: the hiccup numbers for a real take -- detector vs ruler (on the note / octave up / twelfth up / octave down), note-name flips (neighbours vs jumps), dropouts, splices |
| `bmo-tune-cli`, `-latency`, `-bench`, `-gen`, `-score` | as before |

**Things learned the hard way** -- a 16-bit dithered dry cannot null
against a render sample for sample (compare pitch tracks instead); Ableton
matches the offline core bit for bit until 4.71 s of the stimulus and then
drifts at -79 dB (cause unknown, moves no score); Waves' output depends on
the host block size; the Files pane has no save button, so Frosty answers
by commenting on the lines of `ANSWERS.md` and the session writes them in;
the scratchpad is wiped between sessions, so tools belong in the repository.

## Numbers to hold -- the baseline at `f2b0f8a`, on AURORA

```
build/tools/Release/bmo-tune-field "field-audio/shootout-2026-09-11/failure/Antares Failure DRY.wav" --set key=D --set scale=Major --set retune_ms=0
  on the note 90.2 % | octave up 0.7 % | twelfth up 0.5 % | octave down 1.4 % | other 5.8 % | unvoiced 1.4 %
  note-name changes 322; flips back within 80 ms 99 (neighbours 33, jumps 66); dropouts 15; splices 155
build/tools/Release/bmo-tune-field "field-audio/shootout-2026-09-11/fuji/Waves Tune Fuji no tune.wav" --set key=G --set scale=Major --set retune_ms=0
  on the note 91.2 % | octave up 0.4 % | twelfth up 0.1 % | octave down 0.0 % | other 4.2 % | unvoiced 4.1 %
  note-name changes 149; flips back within 80 ms 19 (neighbours 16, jumps 3); dropouts 12; splices 96
build/tools/Release/bmo-tune-ref bmo
  true latency 3.66 ms | correction lag 6.21 ms mean, 8.96 worst | RMS 6.61 c   (Waves ceiling 10.62)
```

Corpus: mean gross error 1.934 % (1.935 % before today). Two residuals
against "no score worse anywhere": `transition_octave_0ms` one frame
(0.3791 -> 0.3794 %), `onset_220Hz` 20 ms and 100 ms lock 0.11 ms later.
Frosty has not accepted those; they are Frosty's call. CPU median 0.88-0.94 %
at 48 kHz / 128. (Note: 0.1 on Failure had 8.5 % octave up and 2 % twelfth
up; the flip counts from the day's scratch scripts, 407 -> 103, counted
across unvoiced gaps and read a few higher than `bmo-tune-field`.)

## Pick up here, in order

1. **Round three.** Frosty listens to `field-audio/blind-2026-09-11-round3`
   (Failure 0 and 20 ms: round two's BMO against today's; Fuji 0 ms: 0.1
   against today's; Antares in each). Answers arrive as comments on the
   lines of its `ANSWERS.md` -- write them in, then open `KEY.txt`, then
   record the result in `testing-notes/shootout-2026-09-11.md`, as rounds
   one and two are. The checklist has the same three groups.
2. **Frosty's calls** (on the checklist): push `f2b0f8a` and this commit;
   accept the two corpus residuals or keep working.
3. **The pops still heard** -- the 66 jump flips on Failure, mostly on
   scoops: a harmonic read during a slide, each a splice by a wrong period.
   `bmo-tune-field --csv` gives BMO's estimate per evaluation; the ruler
   bounded gives the truth. Rebuild one synthetically (a scoop of 150-200 c
   in 50-80 ms on a weak-fundamental voice is the first guess), make it fail
   in `VoiceTests`, fix, re-measure on both takes and the corpus.
4. **Dropouts and weak phrase ends** -- 15 on Failure, 12 on Fuji under 80
   ms; the voicing hysteresis (0.85 on / 0.60 off), the -55 dB gate and the
   10 ms release are the suspects.
5. **The correction lag** -- `hardtune_target`. Predict the pitch forward by
   the estimate's age, or delay the audio up to the Waves ceiling (BMO has
   ~7 ms of headroom). Frosty listens before it is called fixed.
6. **Waves' ceiling across block sizes** -- measured at 128 and 2048 only;
   its output depends on the block size. Sweep 32-2048 with `hostrender`
   and keep the lowest as the ceiling if they differ.

## Rules that still hold

- Name the machine in anything that records where something happened:
  AURORA (laptop, `C:\Users\thesp`) or ICE QUEEN (desktop, `C:\Users\stefr`).
- The schema is frozen; new meaning gets a new id (`kRetiredIds`).
- No FFT in the correction path; nothing allocates after `prepare()`; the
  host is told 0; true latency never over Waves' (the latency rule).
- Field audio, blind sets and the licensed fonts are never committed.
- Pushes only with Frosty's say, to the fork's `bmo-tune-rt`.
