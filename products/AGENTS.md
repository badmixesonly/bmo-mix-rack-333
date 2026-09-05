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
| BMO Mix Rack | -- | `Brck` | `com.lt3audio.bmomixrack` | `.bmorack` |

Manufacturer code `LT3a`, company "LT3 Audio", preset root `LT3 Audio/`.
BMO EQ keeps FrostyEQ's code and bundle id on purpose: that is what makes
existing sessions open.

Reserved for later products (not built, do not reuse): `Bfet` FET comp,
`Bopt` opto comp, `Bdyn` dynamics, `Bdes` de-esser, `Bovr` overdrive,
`Bcmp` compressor, `Bdly` delay, `Brvb` reverb.

## Rules

- A product never contains DSP or UI. If you are writing either here, it
  belongs in `modules/` or `core/`.
- The rack's registry order is the order in its add menu; put the most
  used first.
- Rack presets (`rackPresets()`) name modules by id and set values by
  parameter id, in real units. They are applied through the same path as
  saved state, so anything a preset can express a session can restore.
