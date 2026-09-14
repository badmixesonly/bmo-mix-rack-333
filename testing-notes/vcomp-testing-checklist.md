# BMO Vcomp — listening checklist

Build under test: `integration`, which carries BMO Vcomp (`dc59c0c` on
`add-bmo-vcomp`) alongside BMO DEQ and BMO Tune RT. Built on **AURORA**.
One round of listening covers all three.

**Nothing in this module has been heard on programme material.** Every
number in it is a round number at a shape — the curve, the ARC scales,
the gate's ratio, all eight presets. So this is not a regression pass
looking for faults; it is the pass that decides what the module sounds
like. Disagreeing with a number here is the point, not a bug report.

Offline renders and the numbers behind them are in
`packages/vcomp-listening/` (local, gitignored). Regenerate with
`measure_vcomp presets|gate|bands|curve --outdir <dir>`.

## 1. Does it work at all

- Loads standalone and in the rack, no crashes, clicks or silence.
- AMOUNT at 0 is a **wire** — A/B against bypass should be indetectable
  at any input level. If it is not, that is a real DSP fault, not taste.
- AMOUNT audibly does something at every part of its travel, including
  the first quarter. The first build was dead below about 25% and that is
  exactly the fault to listen for again.

## 2. AMOUNT — the one that matters

The claim is that AMOUNT buys **density, not level**: turning it up
should make the vocal sit further forward without getting louder.

- Sweep it slowly on a dry lead vocal. Does the loudness stay put while
  the *character* changes?
- Does the top of the knob sound like a different, more aggressive
  compressor than the bottom — or just like the same one working harder?
  Three things move together (threshold, knee, ratio 1:1→8:1) precisely
  so that it should be the former.
- Where on the dial does it stop being useful and start being an effect?
  That number decides whether the sweep ranges are right.

## 3. The gate

It is there because the makeup lifts room tone as much as it lifts the
voice. Drag the handle on the IN meter.

- Set it against a real noisy take — bleed, breaths, room. Does it clean
  up the silences without you hearing it work?
- **Does it ever bite the front of a word?** This is the expensive
  failure and the one to hunt for. Offline it moves the first 20 ms by at
  most 0.06 dB at every threshold, but a synthetic phrase is not a singer.
- Is 3:1 decisive enough? At a threshold 8 dB above the noise it shuts by
  16 dB. On a bleedy track that may be too polite — say so and it changes.
- Does putting the threshold *on the meter* work as an interaction, or
  would you rather have a knob? This is the first control in the suite
  that is neither knob nor switch.

## 4. ARC, and the timing controls

- With COMPLEX off, ARC is always on. Does the release feel like it is
  paying attention — quick after a consonant, slower after a long held
  line? That difference is the whole of it.
- Turn COMPLEX on and switch ARC off (preset **Manual**). Is the
  difference audible, and is ARC the better default?
- Standard mode runs a fixed 5 ms attack that a standard-mode user cannot
  change. Is 5 ms right for a vocal, or does it want to be faster
  (denser, more RVox) or slower (more transient through)?
- Turning COMPLEX on with untouched knobs is supposed to be **silent**.
  Verify by ear: flip it back and forth on a held note.

## 5. LOW THRU / HIGH THRU

These split bands out of the compressor's reach — not a sidechain filter.

- **LOW THRU** on a chesty male vocal: does the weight stay while the
  midrange levels? Preset **Keep The Chest**.
- **HIGH THRU** on a bright/sibilant source: does the air stay open over
  a held-down body? Preset **Keep The Air**.
- Sweep each one through its range with AMOUNT high. Anywhere it sounds
  hollow, phasey or like an EQ rather than like less compression is worth
  flagging — the crossover measures flat to 0.00 dB, so anything heard
  there is the *band split concept* not suiting the source, which is a
  real finding.
- Is SC HPF still earning its place next to LOW THRU, or do the two feel
  redundant in use? They do different things on paper.

## 6. The meters

- Three bars, IN / GR / OUT. Is GR growing **right to left** readable, or
  does it fight the two bars above and below it that grow left to right?
- Are the ticks (every 12 dB) enough to set the gate against?
- Does GR in azure read as "working" rather than "in trouble"? It is
  deliberately not the amber/red of the level bars.
- Is the meter block big enough? It replaced Opto's needle VU and is
  considerably shorter.

## 7. Presets

All eight are AMOUNT positions with names, not ear-tuned settings. None
of them set OUTPUT or GATE.

- Do **Lift / Forward / In Front** actually read as three useful
  settings, or as one setting at three depths?
- Anything obviously louder or quieter than the others? They are
  level-matched by construction rather than by hand, so a preset that
  sounds off means the *auto makeup* is off, not the preset.
- **Watch for clipping.** `report-presets.txt` shows In Front and Keep
  The Chest peaking above 0 dBFS on a −18 dBFS RMS source. Full makeup
  with no limiter does that. If you hear it, that is the argument for
  building the limiter RVox has and this does not.

## 8. Against the references

The point of the module. `measure_vcomp gen voice --out x.wav` writes the
harness's own source so the *same file* can go through both.

- **RVox**: is BMO's one knob as immediately useful? Is it as dense at
  the top?
- **RComp**: does ARC hold up against the real thing?
- **DC1A**: is BMO as easy to be right with?

## 9. Naming (lowest priority)

"AMOUNT", "LOW THRU", "HIGH THRU", "SC HPF", "COMPLEX" and the product
name itself are all first drafts. The accent (`#a2a8ff` periwinkle) has
not been signed off either — see `products/AGENTS.md`.

---

Feed back whatever you notice, however informally. The numbers are cheap
to change right now and expensive after the first release.
