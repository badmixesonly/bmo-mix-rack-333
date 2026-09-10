# BMO Dimension

A stereo imager that works on the side signal only.

Split to mid/side, do everything to the side, sum back. The mid path is left as
a plain wire, so — apart from two controls that say so on the tin — anything
this module does disappears in a mono sum instead of comb-filtering it.

## The three stages

| Stage | What it does | In the spirit of |
|---|---|---|
| **Generate** | Two detuned voices, one up and one down, their *difference* injected into the side signal | MicroPitch, CLA Vocals |
| **Diffuse** | A modulated all-pass cascade on the side signal | Dimension D, phasers |
| **Image** | Width, Gerzon bass shuffler, rotation, asymmetry | Waves S1 |

Generate is the only stage that *manufactures* signal rather than shaping it,
and the only one that is not mono-safe — which is why it is the one on a
switch. It is also what makes the module do anything at all on a mono source:
a mono track has no side signal, and an all-pass of zero is zero.

## Controls

**WIDTH** — the side signal scaled, 100 % being unity. Same law and range as
BMO Util's width, deliberately.

**SHUFFLE** / **FREQ** — Gerzon's bass shuffler, widening the low end alone to
correct for the ears hearing stereo as narrower in the bass. 1.0 is off; the S1
manual puts the useful range at 1.6–2.5 below about 650–700 Hz.

**DETUNE** switch and **CENTS** — the spreader. Around 10 cents is the classic
setting. The range stops at 25 rather than MicroPitch's 50, because past about
25 it stops widening and starts sounding out of tune.

**DIFFUSE** — how much side signal goes through the all-pass network. Its LFO
rate and depth are fixed at 0.40 Hz and 50 %: neither was audible enough to
earn a control, so neither has one. Slow on purpose — this is a widener, and an
audible sweep is a different job.

**ROTATE** — the whole soundfield turned, without changing the relative levels
of anything standing on it. Positive degrees move the image **right**, like a
pan knob.

**ASYM** — left against right, with centre material left exactly where it is.
This is not a balance control and not a pan; a dead-centre vocal does not move
at any setting. It is the reason this module is not just a width knob with a
crossover.

## Presets

Init is a wire — a stereo imager that widened the moment you inserted it would
be making a decision you have not made yet.

The rest split on one line: whether your source already has side content.

- **Wide Vocal**, **Mono to Stereo**, **Thicken** turn DETUNE on, so they work
  on a mono track.
- **Bass Shuffle**, **Diffuse Pad**, **Narrow** only scale and steer what is
  already there, so they need a stereo source to do anything.

## Things worth knowing before you use it

- **WIDTH at 0 turns the whole module off**, DETUNE included — width sits
  downstream of everything else.
- **CENTS is inactive until DETUNE is switched on.** It is the only control on
  the panel that does nothing where it stands, and the switch above it says so.
- **There is no output trim yet**, and extreme SHUFFLE and WIDTH together can
  add real level. Watch what leaves it.
- **On a mono track it is a wire**, by design. There is no image to work on.
- **The detune stage throbs.** The two voices beat against each other, so the
  width pulses — roughly 12 Hz at the default, slower and deeper on bass. This
  is a known open question and is the main thing the module is being listened
  to for.

## Building

The two display faces are licensed and are not in this repository. Point your
working copy at them once:

    scripts/set-font-dir.sh <path-to-your-font-folder>

See `assets/fonts/README.md` for the full resolution order.

---

Implementation notes, the measurements behind the voicing, and what is still
open are in [`AGENTS.md`](AGENTS.md) and
[`../../testing-notes/dim-1.0-handoff.md`](../../testing-notes/dim-1.0-handoff.md).
