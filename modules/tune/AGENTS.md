# modules/tune/

BMO Tune RT's parameters (`params.h`) and its DSP (`dsp/`), JUCE-free. The
signal path, in the order a sample meets it:

```
Detector        recursive E/H kernel -> coarse NSDF at ~12 kHz -> full-rate refinement
CorrectionLaw   jump confirmation -> quantize -> vibrato split -> flex -> retune -> gates
ClassicEngine   fractional-rate read, whole-period splices, Live window
```

`TuneCore` joins them; `TuneDsp` is the `ModuleDsp` adapter. Every stage is a
per-sample state machine, which is what makes the output bit-identical at any
block size (`tests/dsp/CoreTests.cpp` checks it).

One engine and one latency contract since 2026-09-11. HYBRID (PSOLA), the
Studio contract, Glide and the formant controls were built, measured and set
aside when CLASSIC sounded better in Ableton; branch `archive/hybrid-studio`
has the code and `testing-notes/nrt-tune-handoff-2026-09-11.md` what it was
and what it measured.

## The invariants

- **No FFT in the correction path** (spec §0). Nothing here uses one.
- **Nothing allocates after `prepare()`.** A pitch-range change arrives on the
  audio thread, so the detector is prepared for the widest range any setting
  can ask for (40 Hz - 2 kHz) and a range change only moves its active window.
- **Live only: the host is told 0, always** (`LatencyContract.h`,
  `TuneCore::kReportedLatency`). No parameter may move it -- a PDC change
  mid-session is a timing jump on the whole track. `bmo-tune-hostcheck`
  checks it on the built VST3.
- **True latency no more than Waves Tune Real-Time's** -- the latency rule in
  the root `AGENTS.md` (Frosty, 2026-09-11): 10.62 ms on the reference
  stimulus; BMO measured 3.82 ms. Within that, a change may make the audio
  later without asking. `HardTuneTests` checks it every run.
- **The correction and the note always come from the same pitch.** See
  "the octave bug" below; this is the one that produced a +1200-cent glitch.
- **Retired parameter ids stay retired.** `engine`, `glide`, `formant`,
  `formant_shift` and `latency` were in 0.1's saved sessions; a new parameter
  under one of those ids would be fed a value meant for something else.
  `kRetiredIds` in `params.h`, checked by `SchemaTests`.
- **Tests measure with `tools/common/Analysis.h`, never with the plugin's own
  detector.** Measuring the output with the code that decided the correction
  agrees with itself whatever it did.

## Where the code departs from the spec, and why

Each of these was measured before it was taken. The test that holds it is
named; undoing one should fail that test.

| Spec says | Code does | Why, measured |
|---|---|---|
| §5.5: 16-tap windowed sinc, "< -100 dB at 0.4 fs" | 32 taps, Kaiser beta 8 | 16 taps reach -24 dB at 0.4 fs. 32 reach -78; -93 at beta 10. `InterpolatorTests` |
| §5.4: oversample 2x, or lowpass at fs/(2 rho) | full-band kernel until an alias could reach 20 kHz, then 0.90/rho | A read at rho folds f to fs - rho f: inaudible below +267 c at 48 kHz. Above it, -68 dB of alias, -0.29 dB at 18 kHz. `SincBank`, `InterpolatorTests` |
| §3.1: the patent's window = lag | kept, plus a whole-cycle mean test | At short lags the window sees a crest fragment of a slow wave; a 1230 Hz lobe beat 110 Hz every half cycle. A real period of a highpassed signal averages to zero. `Detector::spansWholeCycles`, `DetectorTests` |
| §3.4: voicing on clarity + gate + zcr | plus a stability gate | False onset candidates move between hops (1143, 1655, 1043 Hz on consecutive hops of a 147 Hz sine); real ones hold still. `DetectorTests` |
| §3.5 guards 1-3: shorter periods only (sub-multiples, peak fraction, continuity) | plus guard 4: 2 or 3 x the period, if it is much less aperiodic over one long window | Real vocals the corpus lacked. Failure (2026-09-11): a D4 with its fundamental under its second harmonic read at D5 on 8.5 % of voiced frames, a twelfth up on 2 %, the estimate swinging a semitone each evaluation, 427 note flips; Frosty heard "pops and clicks", "hunting". Now 0.7 % and 0.5 %. Aperiodicity as a ratio (0.25; 0.9 when the multiple is the period just held and the period is clearly aperiodic), on the anti-alias lowpass so sub-sample rounding cannot pick the lag, over two long periods and again over the most recent one (else it held the old note ~9 ms past an instant step), decided every 2 ms, and a move to a period not already held needs two runs to agree (else a 2 %-jitter voice doubled by chance). Corpus: mean gross error 1.935 -> 1.934 %; one frame more on transition_octave_0ms, onset_220Hz locks 0.11 ms later; everything else equal or better. CPU median unchanged within noise. `Detector::preferWholeCycle`, `VoiceTests`, `DetectorTests` |
| §3.5 guard 5: median-of-3 on the note decision | a jump > 3/4 semitone waits for the next estimate to agree | A note median paired a held note with a pitch that had already moved: every leap was briefly corrected by its own interval, and a one-frame octave error drove +1200 cents. `CorrectionLaw::confirmPitch`, `CorrectionTests` |
| §4.2: Retune a 0-100 knob, exponential to 0-400 ms | `retune_ms`, 146 steps in ms: 0.0-5.0 by 0.1, then 6-100 by 1; tau in ms | Frosty, 2026-09-11: "display ms", those increments. The unitless knob got a shoot-out mislabelled -- its 10 was 1.2 ms. A new id, the old one retired (a saved 36 meant 10 ms). tau matched to Antares' and Waves' 10 and 20 ms landed with them on real vocals. `SchemaTests`, `CorrectionTests` |
| §4.3a: `((u-u0)/(u1-u0))^2`, "C1 at both ends" | real smoothstep, `3t^2 - 2t^3` | The square has slope 2/(u1-u0) at u1; the applied correction kinks there. `CorrectionTests` |
| §4.3b: `Q(p_slow) + beta p_vib` | the same split, written on the error | Equivalent with the target held; on the error a note change is a step the slow state can be shifted by. At vibrato 0 the note follows the raw pitch, with the dwell below |
| §4.1: nearest allowed note, with hysteresis | at vibrato 0, a switch by less than 60 cents of margin must hold for 40 ms | Frosty, 2026-09-11, after the blind test heard BMO "hunting": "hold the note steadier". A singer sitting between two scale notes (Failure's D#, midway between D and E) flipped with every wobble. 30 cents of margin was tried first and still flipped there. A real step arrives 100-200 cents closer and is taken at once (0 ms, `CorrectionTests`); a pitch settling just past a midpoint moves after 40 ms. Neighbour-note flips: Failure 71 -> 37, Fuji 37 -> 16; corpus note changes 1902 -> 1140. `CorrectionLaw::holdOrSwitch`, `CorrectionTests` |
| §4.6: MIDI target, MIDI as scale, latch, "MIDI required" | none; the key and scale are parameters | Frosty, 2026-09-10: nothing is tracked but the vocal being corrected. It is also what lets the rack's own `SingleModuleProcessor` host this, which does not accept MIDI. MIDI could be appended in a later version without moving a saved session |
| §4.1: key + scale, ten scales in the first build | Chromatic, Major, Minor | Frosty, 2026-09-10: the three a hard-tune session uses. More are appended to the choice list, never inserted |
| §6, §2: a HYBRID engine and a Studio latency contract | neither, in this product | Both were built and passed every gate they had (formants 1.000 +/- 0.001 of scale; Studio measured = reported in every cell). Frosty heard 0.1 in Ableton on 2026-09-11 and CLASSIC sounded better. Kept for a non-real-time tuner: `testing-notes/nrt-tune-handoff-2026-09-11.md`, branch `archive/hybrid-studio` -- which also has the §6.2 LPC groundwork and why it never entered the signal path |

## Measured, 2026-09-10, AURORA, 48 kHz unless stated

| | measured | spec §9 |
|---|---|---|
| Fine pitch error, steady voice | 0.001 - 0.05 c | < 5 c |
| Gross pitch error, 59 pitched corpus items | 0.10 % mean, 1.06 % worst (10 dB pink) | < 1 % clean, < 3 % at 20 dB |
| Output tuning at retune 0 | within 0.04 c, 110 - 880 Hz | < 3 c |
| Time to lock (sawtooth onset) | 2.2 - 2.6 periods | -- |
| Live floor | 19 samples, 0.40 ms, every pitch | <= 1.5 ms CLASSIC >= 200 Hz |
| THD+N, CLASSIC, +/-40 c on a sine | -76 dB | < -60 dB |
| CPU, one core, 48 kHz / 128 | 0.9 % median, 1.2 % p99 (re-run 2026-09-11, CLASSIC only); +0.05 points with guard 4, same day, side by side | < 1.5 % |
| Reported latency, every range | 0 samples; rest delay 0.40 ms in every cell (re-run 2026-09-11) | Live: 0 |
| True latency, reference stimulus (2026-09-11) | 3.82 ms worst (in tune 0.66-1.01, correcting 2.94-3.82); Antares 6.49, Waves 10.62 | <= Waves (the latency rule) |
| Correction lag at 0 ms, vibrato flattened (2026-09-11) | 6.22 ms mean, 3.98 (A3) to 8.95 (A2): about one cycle; Antares -0.24 mean, 1.66 worst | as Antares (open) |

Live's figure is the floor. While it corrects, the read wanders up to a period
above it (mean ~ floor + T/2): 1.5 ms at A4, 2.5 ms at A3. The latency tool's
table records both; the spec's 1.5 ms is met by the floor, not by the mean.

## Open, and not for one session to settle

- **Hard tune trails a moving voice by about a cycle.** The 2026-09-11
  shoot-out (`testing-notes/shootout-2026-09-11.md`): on a held note BMO is
  close to Antares (0.4 c against 0.2 c median on Failure), but the faster
  the pitch moves the further behind it lands -- 5.7 c against 2.0 c at
  10-20 cents per 10 ms, 14.9 against 6.6 beyond. On the reference stimulus the
  correction lag is 4.0 ms at A3 and 9.0 ms at A2, nearly all of the error
  (the fit leaves 1.2-2.5 c). Antares' is -0.24 ms mean: it spends its
  6.5 ms of true latency looking ahead. `HardTuneTests --target` fails on it;
  enable `hardtune_target` in the change that fixes it. Two ways in, both
  measurable there: predict the pitch forward by the estimate's age (no
  latency cost), or delay the audio so the estimate is on time -- which the
  latency rule now allows up to Waves' 10.62 ms, and BMO has 6.8 ms of that
  to spend. Frosty hears the result before it is called fixed.
- **The hiccups heard in 0.1** (Frosty's blind test, 2026-09-11: BMO last in
  four of six groups -- "pops and clicks", "hunting for pitch", "skipping /
  dropouts in the pitch hold", "weak at the end of each phrase"). The worst
  was the detector reading a weak-fundamental voice an octave or a twelfth
  up: guard 4 above, heard in a second blind round as clearly better than
  0.1 on every Failure group, still behind Antares ("skips/pops but few and
  far between"). Since, the dwell for vibrato 0 and guard 4's two later
  checks, not yet heard. Still open, measured on Failure (note-name flips
  407 -> 103 over the day): 66 flips that are detector jumps (0.5 %
  twelfths, 0.7 % octaves up, mostly on scoops -- each a splice by a wrong
  period, the likely source of the pops still heard), 37 neighbour flips,
  1.4 % of frames an octave DOWN (creaky phrase ends: the note name and so
  the correction are unchanged), and 13 mid-phrase voicing dropouts under
  80 ms ("weak at the end of each phrase" is probably these).
- **Vibrato 0 % at a boundary: decided, not yet heard.** Frosty chose "hold
  the note steadier" (2026-09-11), done as the dwell in the table above: a
  vibrato that only just crosses a boundary now holds its note, one that
  goes well across still warbles. Frosty hears it in the next blind round
  before it is called done. The remaining neighbour flips on Failure (37)
  are fast crossings with a clear margin, mostly scoops through a
  neighbouring note on the way into the target.
- **Refinement lumpiness at 192 kHz.** One full-rate refinement lands in one
  host block; at 192 kHz / 32 that is up to ~30 % of the block at p99. Spread
  the refinement across its hop to fix. Not an xrun risk at 48 kHz.
- **The voice generator's release.** Its formant resonators ring for tens of
  ms after the source stops, and the detector (correctly) calls that ringing
  voiced; `onset_880Hz` scores it as a 3 % false alarm. Fix the truth, not
  the detector.
- **Real corpora (spec T-2)** -- PTDB-TUG, CMU Arctic, MDB-stem-synth,
  VocalSet -- need downloading and are not here. Everything above is
  synthetic.
- **rtsan, TSan, UBSan** need clang, which this machine does not have; MSVC's
  ASan is wired (`-DBMO_SANITIZE=address`).
- **Flex** exists and defaults to 0. The two patents the spec flagged for it
  (US 9,147,385 B2, US 8,868,411 B2) are Smule's karaoke patents, not
  Antares' -- a miscitation in the source digest, checked at Google Patents on
  2026-09-10. Not legal advice.
