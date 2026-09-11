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
| §3.5 guard 5: median-of-3 on the note decision | a jump > 3/4 semitone waits for the next estimate to agree | A note median paired a held note with a pitch that had already moved: every leap was briefly corrected by its own interval, and a one-frame octave error drove +1200 cents. `CorrectionLaw::confirmPitch`, `CorrectionTests` |
| §4.3a: `((u-u0)/(u1-u0))^2`, "C1 at both ends" | real smoothstep, `3t^2 - 2t^3` | The square has slope 2/(u1-u0) at u1; the applied correction kinks there. `CorrectionTests` |
| §4.3b: `Q(p_slow) + beta p_vib` | the same split, written on the error | Equivalent with the target held; on the error a note change is a step the slow state can be shifted by. At vibrato 0 the note follows the raw pitch -- see "open" below |
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
| CPU, one core, 48 kHz / 128 | 0.9 % median, 1.2 % p99 (re-run 2026-09-11, CLASSIC only) | < 1.5 % |
| Reported latency, every range | 0 samples; rest delay 0.40 ms in every cell (re-run 2026-09-11) | Live: 0 |

Live's figure is the floor. While it corrects, the read wanders up to a period
above it (mean ~ floor + T/2): 1.5 ms at A4, 2.5 ms at A3. The latency tool's
table records both; the spec's 1.5 ms is met by the floor, not by the mean.

## Open, and not for one session to settle

- **The hiccups heard in 0.1** (Frosty, Ableton, 2026-09-11: "it works ...
  it's got some hiccups"). Not yet described -- which material, which
  settings, clicks or dropouts or wrong notes. The first thing to pin down;
  each should become a corpus item and a failing test before it is fixed.
- **Vibrato 0 % warbles on a boundary.** The note decision follows the raw
  pitch at vibrato 0, so a vibrato straddling a note boundary flips between
  the two notes -- the classic hard-tune sound, kept on purpose because hard
  tuning is this plugin's point. The spec's formula would decide on the slow
  pitch instead and never flip, at the cost of semitone steps landing ~37 ms
  late. Both behaviours are asserted in `CorrectionTests`; which one is the
  default is a listening decision.
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
