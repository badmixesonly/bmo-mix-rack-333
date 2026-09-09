# BMO Dimension — Ableton testing checklist

Build under test: `add-bmo-dimension`, on top of `ee6721d` plus the review
pass. VST3 bundles come from the GitHub Actions run's **BMO-Windows**
artifact — Dimension is in the package now, which it was not in `ee6721d`
(`tools/packager/package.sh` drove a hardcoded product list and had already
dropped BMO Opto the same way once; it reads the build tree now).

This is "what to listen for", in priority order. The things a test can
settle are settled — 12/12 ctest, mono sum exact to 2.4e-7 at 44.1/48/96 kHz,
zero latency, no denormal tail. What is left is what only ears can answer.

## 1. The width throb — the big one

The generate stage runs two voices, one up and one down by CENTS, and
injects their **difference** into the side signal. Two tones that close beat
against each other, so the side signal periodically nulls and reopens.
Measured, side envelope peak-to-trough on a 1 kHz tone:

| CENTS | depth | beat rate |
|---|---|---|
| 5 | 25.2 dB | ~6 Hz |
| **10 (default)** | **19.6 dB** | ~12 Hz |
| 25 | 11.5 dB | ~29 Hz |
| 10, on a 110 Hz tone | 34.6 dB | ~1.3 Hz |

Average width is steady — mean side level barely moves across those. It is
the *modulation* that is deep.

In the conventional wiring (voice up to L, voice down to R) the mid picks up
the complementary sum, so when the side nulls the mid peaks and you hear the
image **swing**. Here the mid is a wire, so when the side nulls the image
**collapses to mono and reopens**. That is the price of the mono-safety, and
nobody has heard it yet.

- On a **mono vocal**, DETUNE on, CENTS 10: does it read as MicroPitch-style
  shimmer, or as an audible tremolo/flutter of the width?
- Does it get better or worse as CENTS goes up? The measurement says the
  throb gets *faster and shallower* — which is backwards from what a user
  would expect, since the gentle setting throbs hardest.
- On **bass or a low pad**: the beat is slowest and deepest down there
  (34.6 dB at 110 Hz). Is it unusable on low material?
- Compare against **MicroPitch or CLA Vocals** on the same source. Those comb
  in mono and this does not — is the trade audible in the direction we want?

If this reads badly, the fix is a design decision, not a bug fix: the voices
could be decorrelated, or one voice dropped, or the pair fed at unequal
depths. Flag what you hear before anyone changes it.

## 2. ASYMMETRY — does the far side widening read as depth or as phasiness

Rebuilt this pass, from the S1 manual rather than from a guess. It was a
plain balance control and moved a dead-centre source (0.5/0.5 at +50% came
out 0.75/0.25), which is the one thing Gerzon's control is defined as not
doing. It is now a shear: the centre never moves at any setting.

The cost is that the side it attenuates also **widens** — 114 % at a quarter
of the knob, 133 % at half, 200 % at the top. No linear law can hold the
centre and also pin hard-panned material at the edges.

- On a mix with **hard-panned guitars and a centre vocal**: at ASYM 25–50 %,
  does the vocal stay put? (It should, exactly.)
- Does the attenuated side read as **receding** — quieter and less tightly
  localised — or as **phasey/hollow**?
- Check it in **mono**. The sum is intended to change here, and does; nothing
  should cancel.

If the widening reads badly there is a documented fallback at the shear in
`modules/dim/dsp/DspCore.h` — `b = a/2` halves it at the cost of 2.18 dB of
centre drift at half knob. **Not** the quadratic blend, which is also written
up there and whose width is non-monotonic.

## 3. Does it work at all

- Loads in a track, and in BMO Mix Rack as a slot module. No crashes.
- **Toggle DETUNE repeatedly during a quiet passage.** The voice buffers
  used to hold 30 ms of stale audio and burst it back out on re-engage —
  measured at 0.90 peak over silence. Should be silent now.
- **Put it on a mono track.** It should be a wire — a stereo imager has no
  image to work on. It used to comb the channel instead.
- Automate WIDTH across its range: no stepping, no zipper.

## 4. The controls, as controls

Nothing here is a bug; it is whether the panel reads.

- **WIDTH at 0 silently disables everything above it.** WIDTH is downstream
  of the generate stage, so DETUNE does nothing with WIDTH parked low.
  Measured: peak side 0.00000. Does that trip you up in use?
- **CENTS is paired with DIFFUSE** on one row under the DETUNE switch, and
  the switch gates only CENTS. Does the row read as one gated pair?
- **RATE and DEPTH do nothing at the DIFFUSE 0 % default** — three of ten
  knobs are dead on a fresh insert. Worth dimming inactive knobs?
  `PlainKnob::setKnobEnabled` exists and nothing in the suite uses it yet.
- **ROTATE +30° moves the image LEFT.** The S1 manual specifies the law but
  not the sign, so this is a free choice — and currently the opposite of a
  pan knob. One line to flip.
- **No output trim, and up to +15.5 dB available**: SHUFFLE 3.0 × WIDTH 200 %
  on anti-phase 80 Hz took a 0.5 peak to 2.98. Every other module has an
  output stage. Does Dimension need one?
- **The DETUNE switch lights `switchAlt` blue**, per the table in
  `modules/AGENTS.md` — it is not a bypass, mono or polarity. It is the only
  non-lavender thing on the panel. Does it pull the eye wrongly?

## 5. Presets

Named for the job. The three that turn DETUNE on are the ones that work on a
mono track; the rest need a stereo source to do anything.

- Do "Wide Vocal", "Mono to Stereo" and "Thicken" separate from each other,
  or are they three points on one line?
- "Bass Shuffle" sits on the S1 manual's own recommendation (2.0 @ 650 Hz)
  — does that read as more spacious, or just louder in the low end?
- Anything that jumps in level against Init at the same settings.

## 6. Light mode (low priority)

The panel is nine captions in lavender, and on the pale plate that is
1.73:1. This is *not* new and is not a Dimension decision — captions carry
the raw accent by Frosty's call in 0.2.3, documented at
`core/ui/Controls.cpp` ("do not fix it"), and the whole suite sits in the
1.72–2.00:1 band. Dimension is the first panel where *every* caption carries
it, so it is the worst case of an accepted trade. Gut-check only.
