# products/

The thin layer that turns a module into a plugin. Each folder is three
files: `Product.h` (name, preset folder, factory function), `main.cpp`
(`createPluginFilter`), and a `CMakeLists.txt` calling `bmo_add_plugin`.
The rack adds `Registry.cpp`, the list of modules it can host, and its
chain presets in `Product.h`.

## The identity table

Permanent. Allocate here before the first build of anything new.

| Product | Module id | Plugin code | Bundle id | Presets |
|---|---|---|---|---|
| BMO EQ | `eq` | `Fsty` | `com.lt3audio.frostyeq` | `.bmoeq` (reads `.frostyeq`) |
| BMO Saturator | `sat` | `Bsat` | `com.lt3audio.bmosaturator` | `.bmosat` |
| BMO Util | `util` | `Butl` | `com.lt3audio.bmoutil` | `.bmoutil` |
| BMO Opto | `opto` | `Bopt` | `com.lt3audio.bmoopto` | `.bmoopto` |
| BMO Dimension | `dim` | `Bdim` | `com.lt3audio.bmodimension` | `.bmodim` |
| BMO Mix Rack | -- | `Brck` | `com.lt3audio.bmomixrack` | `.bmorack` |

Manufacturer code `LT3a`, company "LT3 Audio", preset root `LT3 Audio/`.
BMO EQ keeps FrostyEQ's code and bundle id on purpose: that is what makes
existing sessions open.

BMO Opto is a two-knob opto-style leveling compressor (CRUSH, LEVEL): a
feedback-topology detector (the sidechain reads the signal after gain
reduction, as on a real opto cell, not before it) with a program-dependent
release time -- the harder and longer it has been driven, the slower it lets
go -- and a knee that hardens as CRUSH increases. See `modules/opto/AGENTS`
notes at the top of `modules/opto/dsp/DspCore.h` for the model.

BMO Dimension is a stereo imager in three stages, all of which process the
**side signal only**: a detune stage that manufactures side content from a
mono source, a modulated all-pass that decorrelates it, and an S1-style
imager that scales and steers it. Because `L + R = 2M`, a side-only chain
cancels in the mono sum by construction rather than by testing -- which is
the reason the topology is arranged that way. The detune stage is
bypassable, so one module covers a mono vocal and an already-wide bus.

It takes the lavender `#d4a4ff` that BMO Opto carried until 0.2.2 and gave
up when its panel went greyscale. Nothing else uses it; see
`modules/opto/Module.cpp` for why it was free.

Reserved for later products (not built, do not reuse): `Bpar` parametric EQ
(BMO Parametric, `com.lt3audio.bmoparametric`, `.bmopar`), `Bfet` FET comp,
`Bdyn` dynamics, `Bdes` de-esser, `Bovr` overdrive,
`Bcmp` compressor, `Bdly` delay, `Brvb` reverb.

**BMO Parametric needs its line against BMO EQ drawn before its first
build, not after.** BMO EQ is already an EQ -- three fixed bands and two
filters, reached for by character. A second EQ that does not say in one
sentence why it is not that one will compete with it for the same slot in a
user's head, and the two will get the same automation lanes opened by
mistake. Surgical against character is a real distinction; write it here
first.

## Accents

Permanent, and allocated here for the same reason plugin codes are. The
Palette Book is a measurement write-up, not a registry, and it has drifted
from the code twice -- it still lists `#d4a4ff` against BMO Opto, which gave
that colour up in 0.2.2. Reading it as an allocation list is what nearly
cost BMO Dimension the lavender. This table is the allocation; that document
is the evidence.

Contrast is quoted against both plates that ship: `#2e2e32` dark and
`#efefef` pale. Hue is there because separation from the *other* accents is
the constraint that actually binds -- there are more legible colours than
there are distinguishable ones.

| Module | Accent | Hue | on `#2e2e32` | on `#efefef` |
|---|---|---|---|---|
| BMO EQ | `#f08cb4` | 336.0° | 5.87 | 2.00 |
| BMO Saturator | `#efa552` | 31.7° | 6.55 | 1.80 |
| BMO Util | `#7fc98a` | 128.9° | 6.84 | 1.72 |
| BMO Opto | none -- `tokens().neutral` `#ababab` | -- | -- | -- |
| BMO Dimension | `#d4a4ff` | 271.6° | 6.80 | 1.73 |
| *(not an accent)* utility azure `#4fb8e8` | | 198.8° | 6.02 | -- |
| **reserved** -- BMO Parametric | `#5ecfc0` teal | 172.0° | **7.19** | 1.64 |

BMO Opto has no accent and is not holding one: its panel went greyscale in
0.2.2 so that the only colour on it could mean "engaged". Its red `#e0685a`
and amber `#e0b040` are **states, not an accent** -- they never touch a cap,
a caption or the header bar.

The azure is not a module accent and is not available as one. It is the
utility-knob colour and appears on every panel in the suite, which is
exactly what makes it the hue everything else has to stay away from.

**The teal reservation carries a known objection, recorded so it is not
rediscovered as new.** It was the candidate for BMO Dimension and lost to
the lavender on separation: teal sits **26.8° from the utility azure**, and
the azure is on every panel including whichever one takes the teal. Lavender
had 64.4° to its nearest neighbour. Teal is the best of a thin remaining
field rather than a good hue, and it wins on the dark plate -- 7.19:1 is the
highest in the table. Also worth weighing before it is spent: at 172° it is
about as far from BMO EQ's 336° as two colours get, which reads as
*unrelated*, and a parametric EQ may want to read as EQ's sibling instead.

If the teal is passed over, the field measured for Dimension was
periwinkle `#8fa4ff` (228.8°, 29.9° from azure, 5.74 dark), gold `#e8c95a`
(46.9°, 15.2° from the Saturator, 8.33 dark) and cyan `#63d3e8` (189.5°,
9.3° from azure, 7.74 dark). None of them beats teal on separation.

## Rules

- A product never contains DSP or UI. If you are writing either here, it
  belongs in `modules/` or `core/`.
- The rack's registry order is the order in its add menu; put the most
  used first.
- Rack presets (`rackPresets()`) name modules by id and set values by
  parameter id, in real units. They are applied through the same path as
  saved state, so anything a preset can express a session can restore.
