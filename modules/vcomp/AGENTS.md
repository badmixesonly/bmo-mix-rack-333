# modules/vcomp -- BMO Vcomp

The vocal compressor. Two knobs and a gate handle on the face, five more knobs
behind a switch, and a modern feedforward detector under all of it.

Frosty set the brief on 2026-09-13: **the sound of Waves RVox and RComp, the
simplicity of RVox and Klanghelm DC1A**, with attack, release and a sidechain
filter behind a "complex mode" and nothing but AMOUNT and OUTPUT before that.
The gate, the band controls and the three-bar meter came in the same session,
after the first build was heard about. Everything below follows from that brief
pulling against itself.

## What this module is not

It is not BMO Opto with different numbers, and the two should not be merged.
Opto models two pieces of hardware and inherits their behaviour -- a feedback
cell whose delivered ratio wanders with programme level, a release that
remembers how long the cell has been loaded, no makeup at all because neither
unit has one. Those are the *point* of Opto. Every one of them is the opposite
of what this module is for. If a change here starts making Vcomp more
characterful and less predictable, it belongs in Opto.

## The decisions the code cannot tell you

### AMOUNT moves three things, and the ratio sweep is what makes zero inert

`curveFor` sweeps threshold (-6 to -40 dBFS), knee (12 to 6 dB) and ratio (1:1
to 8:1) together. The ratio starting at 1:1 is not decoration: a 1:1 ratio is a
slope of zero, so at AMOUNT 0 the module is a wire at *every* level, not merely
quiet.

The first build did that job with the threshold instead -- ratio 2:1 at the
bottom, the knee's lower edge parked at 0 dBFS so nothing below full scale was
touched. Also inert, and it cost the bottom third of the knob: at AMOUNT 20 the
knee had only reached -8.6 dBFS, so a vocal tracked at a sensible -10 got
nothing at all until the knob was a quarter round. `testAmountGrabsHarder` is
what caught it.

`measure_vcomp curve` prints the whole sweep. That table is the first place to
look when the module feels wrong at one end of the knob.

### The makeup reference is a peak figure, and getting it wrong is silent

`autoMakeupDb` adds back the reduction the curve applies at `kReferenceDb`, so
AMOUNT buys density rather than level. The reference has to be the level the
**peak detector** will see, which on a vocal is 12-15 dB above where the meter
sits.

It was first set to -10 dBFS by reading "where a vocal sits" off an RMS meter.
Nothing broke; every factory preset simply came out below the level it went in,
worst at the *bottom* of the knob where the makeup is smallest but the peaks are
reduced just as hard. The suite's `voice()` source is normalised to -18 dBFS RMS
and peaks at -3.6 (a 14.4 dB crest), so the makeup was compensating for a signal
6 dB quieter than the detector was hearing. At -7 dBFS every factory preset
lands inside 2.5 dB.

This is the failure mode to watch for in any revoicing: it does not announce
itself, and only `VcompTests`' level-matching check holds it.

### ARC's slow branch is programme-dependent because of its *attack*

The one piece of arithmetic most likely to be "simplified" into something that
does nothing. Two release branches with different time constants, combined with
`max()`, is just the slower of the two -- at every sample, for any input,
because a slower one-pole fed the same signal is never below a faster one.
There is no programme dependence in it whatsoever.

What makes it work is that the slow branch **charges slowly**
(`kArcChargeScale` x RELEASE). A consonant barely moves it, so it has nothing
to release slowly; a sustained loud phrase charges it most of the way, and then
it is the branch that decides the recovery.

`testArcIsProgrammeDependent` compares recovery after a 30 ms hit with recovery
after a 3 s hit at the same level, and checks that the ARC-off control case
recovers identically after both. A weaker test -- "the release is slow" -- would
pass the broken version.

### The gate is an expander, and it is first for a reason

It exists because of the auto makeup, which is indiscriminate: at AMOUNT 80 the
module adds about 26 dB to the voice and to the room tone, headphone bleed and
mic noise between lines alike. Cleaning that up is the other half of making a
one-knob compressor usable, which is why RVox ships the same pairing.

- **Ahead of the compressor**, because what it closes has to be closed before
  the makeup amplifies it.
- **Keyed off the raw input**, so the threshold is an absolute level the user
  can set against the track rather than one that moves when AMOUNT does.
- **An expander with a floor**, not a hard gate: a hard gate chatters on
  breaths and bites the tails off words, and both are audible on a voice in a
  way they are not on a tom.
- **Fast to open (0.5 ms), slow to close (150 ms) with a 40 ms hold.** Opening
  is the direction that costs a syllable if it is wrong. `measure_vcomp gate`
  prints the first 20 ms of the phrase separately for exactly that reason; at
  every threshold in the table the onset moves by at most 0.06 dB.

### LOW THRU and HIGH THRU are not the sidechain filter

SIDECHAIN changes what the compressor **listens to**. LOW THRU and HIGH THRU
change what it **acts on**: those bands are split off the audio, pass through
uncompressed, and are added back. Keeping both is deliberate -- the detector
reads the full gated signal through SIDECHAIN, *not* the mid band, so moving
LOW THRU changes which parts of the signal the gain is applied to and does not
change how hard the compressor works.

Two things about the crossover are worth knowing before touching it:

- **The low band goes through the second split's allpass.** Splitting at LOW
  THRU and then splitting only the remainder at HIGH THRU leaves the low band
  having been through one crossover and the other two through two, so they stop
  summing flat -- a dip around the upper crossover that moves when HIGH THRU
  moves. `testBandsReconstruct` holds it, and `measure_vcomp bands` prints the
  reconstruction error across the spectrum (0.00 dB everywhere, today).
- **Measure the bands as modulation, not as level.** The obvious test -- "the
  low tone comes out louder with LOW THRU up" -- is wrong, and looks right
  until you do the arithmetic: the makeup gives back almost exactly what the
  curve took at the reference, so a steady tone near that level comes out at
  the same place either way. The first version of that test asserted a 6 dB
  difference and found 0.9. What LOW THRU actually buys is that the low end
  stops being *pumped* by whatever else triggers the compressor.

### The filter cutoff clamp is a real control range, not a safety detail

A cutoff cannot be placed at Nyquist, so every filter here clamps. The clamp
started at 0.45 of Nyquist, which is 10.8 kHz at 48 kHz -- and HIGH THRU's range
runs to 20 kHz. The result was that engaging LOW THRU put the *upper* split at
10.8 kHz while the panel read 20 kHz, and everything above 10.8 kHz was quietly
handed to the thru band and stopped being compressed.

Nothing sounded broken. `measure_vcomp bands` is what caught it: 4.7 dB of
pumping on a 12 kHz tone where the un-split case had 11.5, sitting in a table
next to three figures that were right. Fixed two ways -- each side of the split
is now engaged independently, and the clamp is 0.98 of Nyquist so the whole
range is reachable at 44.1 and 48 kHz. `testOneSideEngagedLeavesTheOtherAlone`
is the test that was missing.

### Three bars, not a needle

BMO Opto's `DynamicsMeter` is a period instrument -- VU ballistics on a 1940s
scale -- which is right for a module modelling an LA-2A and wrong for this one.
It also shows one reading at a time behind a three-way switch, and the three
readings a compressor user wants are wanted together. IN and OUT read left to
right in dBFS; GR reads right to left from zero, the way every gain-reduction
meter has ever read.

`LevelBar` is **module-local on purpose** and should move to `core/ui` the
moment a second module wants one -- the same rule `modules/AGENTS.md` applies to
`DynamicsMeter`'s scale. GR is painted in `meterGr` rather than the low/high/
clip zones: a compressor working hard is not a compressor in trouble, and 24 dB
of reduction in the clip red would be the meter telling the user off for using
the module.

## A gap in the shared layout test

`ui_layout` passed with "LOW THRU" and "HIGH THRU" rendering as "LOW THR" and
"HIGH TH". `PlainKnob::captionOverflow` measures the same font into the same box
the caption is drawn into and reported that both fitted. The render disagreed.

They were given half the panel each rather than the arithmetic being argued
with, so the panel is right today -- but **the measurement and the render
disagree for these two strings**, and that is a shared-code question affecting
every module's caption, not something to patch from inside one module's layout.
It is unresolved. If you are in `core/ui/Controls.cpp` for other reasons, this
is worth half an hour.

## What waits on an ear

Nothing here has been heard on real programme material. `tools/measure/vcomp`
exists for that pass: `curve`, `presets`, `gate` and `bands` each print a table
and write WAVs of the same render, so a number and a listen are never of
different things, and `gen` exports the harness's own signals so the same file
can be fed through RVox or RComp for comparison.

Specifically open:

- **The curve's three sweeps** are round numbers at a shape, not tuned figures.
- **The eight factory presets** are AMOUNT positions with names on them.
- **`kArcFastScale` / `kArcChargeScale` / `kArcSlowScale`** (0.35 / 2 / 5 x
  RELEASE). The charge scale decides how much material counts as "sustained"
  and is the first knob to turn if ARC feels wrong.
- **`kStandardAttackMs` = 5** is the single number standard mode's whole feel
  rests on, since a user in standard mode cannot change it.
- **The gate's `kGateRatio` = 3:1 and `kGateRangeDb` = 50.** 3:1 is gentle; at
  a threshold 8 dB above the noise it shuts by 16 dB, which may not be decisive
  enough on a bleedy track.
- **Output peaks.** `measure_vcomp presets` shows "In Front" and "Keep The
  Chest" peaking above 0 dBFS on a -18 dBFS RMS source. That is what full
  makeup with no limiter does, and it is the strongest argument for the
  limiter below.

## Open, and deliberately not built

- **No lookahead, and therefore no latency at any setting.** The one thing a
  Pro-C-class compressor has that this does not, and what lets the module sit
  on a vocal while the singer is listening to it. Adding it later changes
  `latencyForParams` and the module's place in a tracking chain, so it is a
  product decision rather than a feature to slip in. The band split does not
  change this: the crossover is IIR, so it costs phase rather than samples.
- **No limiter.** RVox is gate -> compressor -> limiter and this is the missing
  third. See the output peaks above; this is the likeliest next addition.
- **No parallel MIX.** Neither RVox nor DC1A has one.
- **No stereo LINK switch.** Stereo is always linked, because two channels of
  one voice compressed independently is a wandering image rather than a stereo
  option. Opto has the switch because it is a general-purpose box that ends up
  across a mix.

Any of these is a parameter appended to `specs()` with a default that leaves
old sessions sounding the same -- see `modules/AGENTS.md`, "Changing a module".

## Shared code this module moved

`core/dsp/GainComputer.h` is new, and it is BMO Opto's arithmetic: `Curve`, the
feedforward/feedback slope conversions and the Reiss & McPherson soft-knee gain
computer, which both modules need. Opto's `Detector.h` now pulls them in under
the same names it used before, so the rest of that file reads as it always did
and its numbers are unchanged -- `opto_dsp` passing is the check on that. The
derivation of why a feedback cell cannot state what it needs as a ratio is
Opto's finding and went with the code; it is why the shared type carries a
slope and not a ratio.

`tools/measure/Wav.h` is also new, lifted verbatim from
`tools/measure/opto/main.cpp`. The five older harnesses still hold their own
copies; move each one when it is next opened, and delete its copy then.
