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
Retune Speed says. At 0, the default, the snap is immediate: that is the sound this plugin
is built for.

- **Vibrato** 0 % flattens the singer's vibrato onto the note; 100 % keeps it
  and only corrects the note it is centred on; 150 % exaggerates it.
- **Glide** (Hybrid only) slides between notes instead of jumping.
- **Flex** leaves small deviations alone. Off by default.

## Two engines

- **Classic** reads the voice faster or slower, repeating or dropping whole
  cycles to keep up. Formants move with the pitch, which is the bright,
  slightly synthetic quality the classic hard-tune sound has.
- **Hybrid** rebuilds the voice from overlapping single-cycle grains placed at
  the new pitch. Formants stay where the singer put them, and **Formant
  Shift** can move them independently.

Both cost the same latency, so switching engines never moves the track.

## Latency

- **Live** (default): reports zero to the host and costs 0.4 ms while it is
  not correcting. While it corrects, it runs up to one cycle of the note
  later -- the same contract as Waves Tune Real-Time.
- **Studio**: reports a fixed delay for the chosen pitch range (9.2 ms for
  Auto at 48 kHz) so the host lines the track up exactly.

## CPU

About 1 % of one core at 48 kHz with 128-sample buffers, either engine, on
the laptop this was built on.
