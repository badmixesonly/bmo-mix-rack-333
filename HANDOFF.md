# Handoff: testing the questionable audio

For a fresh session reviewing BMO Tune RT against **23 audio examples**
Frosty recorded in Ableton on 2026-09-11 -- the "hiccups" heard in the first
build ("it works! its got some hiccups but it works"). Written on **AURORA**,
2026-09-11, at the end of the session that built the plugin.

Read first: `AGENTS.md`, then `modules/tune/AGENTS.md` (invariants, what was
measured, what is open). This file is only about the audio.

> **Update, later on 2026-09-11, on AURORA.** What arrived was a shoot-out
> rather than hiccup examples: two songs through Antares, Waves and 0.1 at
> 0 / 10 / 20. It is analysed in `testing-notes/shootout-2026-09-11.md`;
> true latency and correction lag against both, measured on a synthetic
> reference, in `testing-notes/latency-and-lag-2026-09-11.md`. Three things
> changed as a result: **Retune is now in milliseconds** (`retune_ms`; 0.1's
> `retune` was a unitless knob -- 0.1's knob k was 400 (2^(8k/100) - 1) / 255
> ms, so 10 was 1.2 ms and 36 was 10 ms -- and is retired), **the latency
> rule** (root `AGENTS.md`), and **the open hard-tune work**
> (`tests/dsp/HardTuneTests.cpp --target`). The method below still holds for
> any hiccup example that turns up; read a 0.1 Retune value through that
> formula.

## Where the code is

| | |
|---|---|
| Repository | `bmo-tune-rt`, its own repository. Pushed to Frosty's fork as branches, since it has no remote of its own: `badmixesonly/bmo-mix-rack-333`, branch **`bmo-tune-rt`** (this code) and **`bmo-tune-rt-archive-hybrid-studio`** (the engine that was set aside). Its history is unrelated to the rack's; never merge either branch into the fork's `main`, and never push anything to Kevin's `kevkloud/bmo-mix-rack`. |
| Clone | `git clone -b bmo-tune-rt https://github.com/badmixesonly/bmo-mix-rack-333.git bmo-tune-rt`, then `git submodule update --init libs/bmo-mix-rack` |
| Build | `scripts/build.sh` (DSP, tests, tools -- no JUCE needed for any of the audio work below); `scripts/build.sh --plugin` for the VST3 |

## Read this before listening to anything: which build made the audio

The examples were made with **0.1**, commit `6710e73`, the build installed in
`C:\Program Files\Common Files\VST3` on AURORA. The code on `bmo-tune-rt` is
**newer**: on 2026-09-11 the product went CLASSIC and Live only (`5b46af7`).

So for each example, first sort it:

- **Recorded on Hybrid, or with Studio latency, Glide, or the formant
  controls in play** -- that code is no longer in Tune RT. Note it and set it
  aside for the non-real-time handoff
  (`testing-notes/nrt-tune-handoff-2026-09-11.md`); do not spend time fixing
  it here.
- **Recorded on Classic, Live** -- this is Tune RT today, and the rest of
  this file applies. The CLASSIC signal path did not change between 0.1 and
  now except that Glide was removed (it was already inert on CLASSIC, proved
  bit-exact by the old `ModeTests`), so a Classic example reproduces on
  current code.

To render an example on 0.1 exactly, build the old commit side by side:
`git worktree add ../tune-0.1 6710e73`.

## What Frosty should give for each example

The audio is most useful as a pair: the **dry vocal** that went into the
plugin, and the **plugin's output** as heard. The DSP is deterministic and
bit-identical at any host block size, so a dry file and its settings
reproduce the output exactly -- which turns "it sounded wrong" into a
number. Only the output? Still useful, but the session has to infer the
input.

Put the files in `field-audio/` at the repository root. It is **gitignored**:
the vocals may not be ours to publish, and the fork is public. Never commit
them; a test gets a synthetic stand-in instead (below).

One row per example, in `field-audio/manifest.md` (a copy of this table):

| # | files (dry / out) | engine | latency | key, scale | range | retune | vibrato | flex | notes off | ref A | rate / buffer | track | where (m:ss) | what it sounded like |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | `01-dry.wav` / `01-out.wav` | Classic | Live | A minor | Auto | 0 | 0 | 0 | -- | 440 | 48k / 128 | mono | 0:12, 0:31 | click on the held note |

"What it sounded like" in plain words is the most important column. The
useful vocabulary: *click or pop*, *dropout* (correction cuts out), *wrong
note*, *octave jump*, *warble* (flips between two notes), *late* (the snap
comes after the note starts), *doubling or phasing*, *level dip*, *robotic
where it should not be*, *not robotic where it should be*.

Also worth knowing per example: a mono or stereo track (see "stereo" below),
and whether anything was automated during the take.

## Reproducing one

```
build/tools/Release/bmo-tune-cli field-audio/01-dry.wav field-audio/01-render.wav ^
    --set key=A --set scale=Minor --set retune_ms=0 --set vibrato=0 ^
    --dump-analysis field-audio/01.csv
build/tools/Release/bmo-tune-cli --list        # every parameter id and its choices
```

Choices go by name (`key=Bb`, `scale=Minor`, `range=Bass`); a note switched
out is `note_d=0`. `--block N` sets the host block size, which should change
nothing -- if it does, that is a bug in itself.

**Confirm the reproduction before diagnosing.** Null the render against
Frosty's output file (subtract, sample-aligned; `tools/common/Analysis.h`
has `delayOf` for the alignment). If they do not null, find out why first --
settings, automation, the 0.1-versus-now question, or stereo:

- **Stereo.** The plugin corrects the **left** channel and copies it to the
  right. `bmo-tune-cli` **sums** multichannel input to mono. On a stereo
  source whose channels differ, the two disagree. Split the left channel out
  first, or give the CLI a `--channel` option (a small, worthwhile change).
  And if a stereo vocal is part of the complaint -- "the right side
  vanished", "it went narrow" -- that is the known limitation in
  `testing-notes/tune-0.1-first-build-2026-09-10.md`, not a new fault.

## Reading the analysis

`--dump-analysis` writes one row per detector evaluation and per splice:

| column | meaning |
|---|---|
| `n` | sample index -- seconds = n / rate |
| `evaluated`, `f0`, `clarity` | the detector ran here; its pitch in Hz and its confidence, 0-1 |
| `voiced` | whether it believes a note is sounding |
| `pitch_in` | the pitch the correction used, in semitones from A4 (after jump confirmation) |
| `target`, `note` | the note it is correcting toward |
| `applied_cents` | the correction sent to the engine |
| `ratio`, `lag` | the engine's read rate and its delay behind the input, in samples |
| `splice` | 1 where the engine jumped a whole period to stay in its window |

What to look for, by complaint -- starting points, not conclusions:

| complaint | look at | likely suspects |
|---|---|---|
| click / pop | `splice` rows at that time; `lag` jumping | a splice over material that is not periodic (breath, a voiced consonant), where the half-period crossfade has nothing correlated to cross; the homing fade |
| wrong note, octave jump | `f0` against the real pitch; `note` changes | an octave error the jump confirmation let through (it waits for one agreeing estimate); the note decision; the scale mask |
| warble on a held note | `note` flipping between two neighbours | the **known** vibrato 0 behaviour -- at vibrato 0 the note follows the raw pitch, so a vibrato across a boundary flips (`modules/tune/AGENTS.md`, "Open"). By design for hard tune; whether it is too much is Frosty's call |
| dropout / correction cutting in and out | `voiced` toggling; `clarity` near 0.60-0.85 | the voicing hysteresis (0.85 on, 0.60 off), the -55 dB gate, the stability gate; the 10 ms release |
| late snap at note starts | time from onset to the first `voiced` row near the note | lock takes 2.2-2.6 periods by measurement -- about 11 ms on an A3; the quarter-period attack |
| doubling, phasing, comb | `lag` wandering during a note | the Live window: the read wanders up to a period behind while correcting (1.5 ms at A4, 2.5 ms at A3) -- against a dry copy of the vocal elsewhere in the session, that is a comb filter |
| chipmunk or dark tone on big corrections | -- | CLASSIC moves formants with the pitch by design (spec §5.3). Not a fault |

## Turning a hiccup into a test, then a fix

The house rule, from both repositories: **measure, never judge a render by
eye or ear alone** -- and fix nothing a test does not first fail on.

1. Find the shortest excerpt that shows it, and the analysis rows that
   explain it.
2. Rebuild it synthetically -- `tools/common/Signals.h` (`voice`, `glide`,
   `vibrato`, `step`, noise at an SNR) or a new `bmo-tune-gen` corpus item --
   so the test ships without anyone's vocal in it. If it will not reproduce
   synthetically, that is a finding too: say what the real audio has that
   the generator lacks.
3. Write the failing check in `tests/dsp/` (one `check` per claim, the claim
   as a sentence, the measured number `report`ed), measured with
   `tools/common/Analysis.h` -- never the plugin's own detector.
4. Fix it. Then all of `scripts/build.sh`, `scripts/build.sh --plugin`,
   `scripts/build.sh --corpus`, `bmo-tune-latency --range all` and
   `bmo-tune-bench --quick` must pass, and the corpus scores should not get
   worse anywhere (today: gross pitch error 0.10 % mean, 1.06 % worst).
5. Record it in `modules/tune/AGENTS.md` -- the departure, the number, the
   test that holds it -- and move the item out of "Open".

Frosty listens to every fix before it is called fixed; a passing test
is necessary, not sufficient.

## Rules that still hold

- Name the machine in anything that records where something happened --
  AURORA (laptop, `C:\Users\thesp`) or ICE QUEEN (desktop, `C:\Users\stefr`).
  Never infer ICE QUEEN's state from AURORA's disk.
- The parameter schema is frozen; five ids are retired for good
  (`kRetiredIds`). A fix that needs a new parameter appends one, and argues
  for it in `tests/dsp/SchemaTests.cpp`.
- No FFT in the correction path; nothing allocates after `prepare()`; the
  host is told 0 latency, always.
- Pushes only with Frosty's say, to the fork's `bmo-tune-rt` branch -- never
  to Kevin's repository.
- Downloads (pluginval, real corpora, anything) need Frosty's permission.
- The licensed fonts and the field audio are never committed.
- The installed VST3 on AURORA is still 0.1 until someone copies a new build
  over it.
