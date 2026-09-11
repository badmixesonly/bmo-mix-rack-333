# BMO Tune RT -- the DSP

What the plugin does to a voice, in plain terms. `AGENTS.md` beside this file
has the engineering detail and the evidence for every number here.

## What it listens for

The detector finds the pitch of a single voice from about 80 Hz to 1.4 kHz
(the Auto range; Bass and Instrument reach down to 55 Hz). It locks onto a
new note in about two and a half cycles of that note -- 5 ms on an A4, 11 ms
on an A3 -- and tracks it to a small fraction of a cent. Breaths, consonants
and silence are left alone.

## What it does about it

It pulls the voice to the nearest allowed note -- chromatic by default, or a
key with a major or minor scale, with any note switched out -- as fast as
Retune Speed says. At 0, the default, the snap is immediate: that is the
sound this plugin is built for.

- **Vibrato** 0 % flattens the singer's vibrato onto the note; 100 % keeps it
  and only corrects the note it is centred on; 150 % exaggerates it.
- **Flex** leaves small deviations alone. Off by default.

## How it moves the pitch

It reads the voice faster or slower, repeating or dropping whole cycles to
keep up. Formants move with the pitch, which is the bright, slightly
synthetic quality the classic hard-tune sound has.

There was a second engine, Hybrid, which kept the formants where they were.
Classic sounded better in Ableton (2026-09-11), so it is the only one; Hybrid
is kept for a possible non-real-time tuner --
`testing-notes/nrt-tune-handoff-2026-09-11.md`.

## Latency

It reports zero to the host and costs 0.4 ms while it is not correcting.
While it corrects, it runs up to one cycle of the note later -- the same
contract as Waves Tune Real-Time. That is the only mode: it is what a
singer monitoring through the plugin needs.

## CPU

About 0.9 % of one core at 48 kHz with 128-sample buffers, on AURORA, the
laptop this was built on.
