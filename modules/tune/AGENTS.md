# modules/tune/

BMO Tune RT's parameters (`params.h`) and its DSP (`dsp/`), JUCE-free. The
signal path, in the order a sample meets it:

```
Detector        recursive E/H kernel -> coarse NSDF at ~12 kHz -> full-rate refinement
CorrectionLaw   jump confirmation -> quantize -> glide -> vibrato split -> flex -> retune -> gates
ClassicEngine   fractional-rate read, whole-period splices          } one plays, the other is fed;
HybridEngine    PSOLA, analysis one period per grain, grain-rate formants } a switch crossfades 20 ms
```

`TuneCore` joins them; `TuneDsp` is the `ModuleDsp` adapter. Every stage is a
per-sample state machine, which is what makes the output bit-identical at any
block size (`tests/dsp/CoreTests.cpp`, `HybridTests.cpp` check it).

## The invariants

- **No FFT in the correction path** (spec §0). Nothing here uses one.
- **Nothing allocates after `prepare()`.** A pitch-range change arrives on the
  audio thread, so the detector is prepared for the widest range any setting
  can ask for (40 Hz - 2 kHz) and a range change only moves its active window.
- **Both engines report the same Studio latency** (`LatencyContract.h`). A
  host is told the plugin's latency; switching engines must never change it.
- **The correction and the note always come from the same pitch.** See
  "the octave bug" below; this is the one that produced a +1200-cent glitch.
- **The panel hides what the chosen engine does not use** -- hidden, not
  greyed (Frosty, 2026-09-10). `isHybridOnly()` in `params.h` is the one list
  (Glide, Formant, Formant Shift) and `tests/dsp/ModeTests.cpp` holds it to
  the DSP both ways: each is bit-exactly inert on CLASSIC and audible on
  HYBRID. A parameter joins the list only with that test agreeing.
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
| §6.1: PSOLA costs ~T0 of lookahead; §7 pitch marks | analysis advances one period per grain; no mark detector | Neighbouring grains one cycle apart is all pitch-synchronous needs. HYBRID keeps CLASSIC's latency: 19 samples idle, not ~T0. `HybridTests` |
| §6.2: LPC inverse / PSOLA / resynthesis | not in the signal path; formants by PSOLA, shift by grain rate | Order 24 at 48 kHz modelled the empty band to Nyquist and no formants; a 16 kHz envelope mapped up through its LSFs was too ill-conditioned (coefficients ~1e5) to survive grain interpolation. PSOLA alone holds formants at 1.000 +/- 0.001 of scale; the spec gate is 2 %. `HybridTests` |

`dsp/Lpc.h` is the §6.2 groundwork and stays, tested (`LpcTests`): Levinson,
conditioning, LSF by Chebyshev root-finding, interpolation, and a formant warp
that replaced plain LSF scaling (scaling pinned the top LSFs against pi and the
synthesis filter's guard kept resetting). To put an LPC stage back, it needs a
structure that is well-conditioned at the host rate: warped LPC (allpass
delays, lambda ~0.7 at 48 kHz), or poles mapped from a 16 kHz envelope into a
cascade of biquads. Either should be measured against PSOLA alone first --
the case for it is formant *accuracy* on large shifts, which nothing here has
shown PSOLA to lack at tuner-sized ones.

## Measured, 2026-09-10, AURORA, 48 kHz unless stated

| | measured | spec §9 |
|---|---|---|
| Fine pitch error, steady voice | 0.001 - 0.05 c | < 5 c |
| Gross pitch error, 59 pitched corpus items | 0.10 % mean, 1.06 % worst (10 dB pink) | < 1 % clean, < 3 % at 20 dB |
| Output tuning at retune 0 | within 0.04 c, both engines, 110 - 880 Hz | < 3 c |
| Time to lock (sawtooth onset) | 2.2 - 2.6 periods | -- |
| Live floor | 19 samples, 0.40 ms, every pitch | <= 1.5 ms CLASSIC >= 200 Hz |
| Studio PDC, Auto / Soprano / Alto-Tenor / Bass / Instrument | 9.19 / 4.77 / 7.44 / 13.21 / 13.21 ms, measured = reported in every cell | fixed, = measured |
| Formants, HYBRID, +/-1 semitone | scale 1.000 / 1.001 | F1 - F3 within 2 % |
| THD+N, CLASSIC, +/-40 c on a sine | -76 dB | < -60 dB |
| CPU, one core, 48 kHz / 128 | CLASSIC 0.9 % median 1.2 % p99; HYBRID 1.0 % / 1.4 % | < 1.5 % / < 3 % |

Live's figure is the floor. While it corrects, the read wanders up to a period
above it (mean ~ floor + T/2): 1.5 ms at A4, 2.5 ms at A3. The latency tool's
table records both; the spec's 1.5 ms is met by the floor, not by the mean.

## Open, and not for one session to settle

- **Formant: keep or cut** -- kept until it can be heard in Ableton (Frosty,
  2026-09-10). A Keep/Follow choice since the schema froze; cutting it now
  means hiding it, since its id stays in the schema for good. In
  HYBRID, Keep keeps the singer's formants, Follow lets them follow the
  correction as CLASSIC's do. Cutting it changes nothing else; the only
  combinations lost are HYBRID with formants following the pitch *and* Glide,
  or *and* Formant Shift, since CLASSIC has neither. Its code is one line in
  `HybridEngine::scheduleGrains`. The audible difference on chromatic
  corrections (<= 50 cents, formants moved <= 3 %) is small; on a semitone
  or more it is obvious.
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
