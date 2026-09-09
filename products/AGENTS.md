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

Reserved for later products (not built, do not reuse): `Bfet` FET comp,
`Bdyn` dynamics, `Bdes` de-esser, `Bovr` overdrive,
`Bcmp` compressor, `Bdly` delay, `Brvb` reverb.

## Rules

- A product never contains DSP or UI. If you are writing either here, it
  belongs in `modules/` or `core/`.
- The rack's registry order is the order in its add menu; put the most
  used first.
- Rack presets (`rackPresets()`) name modules by id and set values by
  parameter id, in real units. They are applied through the same path as
  saved state, so anything a preset can express a session can restore.
