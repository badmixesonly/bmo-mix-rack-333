# BMO Saturator — voicing bell, listening retest

**Do this on the next build.** It is the one thing on this fork that has
changed in the audio and has not been heard here.

Build under test: anything from `main` at or after `586a7b9`. This is a "what
to listen for" note, not a build guide.

## What changed, and why it needs ears

Kevin's `51a263b`, *Drop the voicing bell's soft-limit in favour of Frosty's
Q/gain fit*, in `modules/sat/dsp/Filters.h` — 30 lines out, 7 in. It reached
this fork on 8 Sep, in the merge that brought `main` level with
`kevkloud/bmo-mix-rack` before the UI pass went back as a pull request.

It has never been auditioned on this machine. The whole of the `ui-editor`
branch was UI, tests and docs, so nothing before it moved the signal at all,
and every "no DSP file touched" claim on that branch is about that branch —
not about this merge.

**The automated tests do not settle it.** All ten suites pass with it in, but
the sat suites assert level and schema within tolerances; none of them
describes a voicing bell's character. A soft-limit coming out is exactly the
kind of change that passes a level test and sounds different.

## The trap, before you measure anything

**0.4.0's reference render was bounced with AUTO on.** The asymmetry figures
taken from it are void and cannot be used to judge this — see
[Saturator sheen is a bell shape] in the palette/measurement notes and the
0.4.0 render notes. The band tilt from that bounce survives; the asymmetry
numbers do not.

So: **render fresh, with AUTO off**, before comparing anything numerically.
`tools/measure/renders` in `band` mode is the tool, and its README carries the
two things it costs a wrong answer to forget — bin each file at its own sample
rate, and walk the RIFF chunk list.

## What to listen for

- **Drive sweep, full range.** The soft-limit was what held the bell in check
  at the top. Does anything get harsh, spiky or unstable where it used to
  round off? That is the first thing this change could have cost.
- **TONE across its range**, since TONE *is* the high end on this module —
  the error against the target was a too-wide 7 kHz bell, not a shelf level.
  Does the fit sit where the tilt used to?
- **MIX at partial settings.** A limiter coming out of the wet path changes
  what parallel blending does; check it does not now stack oddly against dry.
- **Against a reference.** A vocal and a drum bus, at -18 dBFS RMS per track,
  which is where this suite's material sits. Not at -6: that is a mix-bus
  level, not a track level.

## If it is wrong

The change is one commit and reverts cleanly — nothing on this fork depends on
it. But it is Kevin's call as much as Frosty's, so raise it rather than
reverting it here: it came from upstream, and the two repositories are already
one conversation short of agreeing on where work should live.
