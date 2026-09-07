# modules/

One folder per module. A module is the unit the suite is built from: the
standalone product and the rack both host the same thing.

```
modules/<id>/
  params.h                 ids (kXxx), enum Index, kVersionHint/kStateVersion/
                           kSchemaVersion, specs()
  dsp/                     a ModuleDsp; JUCE-free; setParams(values by Index)
  panel/<Name>Panel.h/.cpp a ModulePanel; builds controls from context.params
  presets/FactoryPresets.h factory() -> vector<FactoryPreset>, Init first
  Module.h/.cpp            module() -> const ModuleDef&
```

## Adding a module

1. Pick a permanent lowercase `id` and a design width (multiple of 20,
   >= 160). Allocate the product's plugin code and bundle id in
   `products/AGENTS.md` at the same time.
2. Write `params.h`. Order is permanent from the first release; put the
   controls a user reaches for first at the top. Keep it under 32
   parameters or it will not fit a rack slot.
3. Write the DSP against `core/dsp/ModuleDsp.h`. It reads `v[Index::x]`
   in `setParams`, which is called before `prepare` and before every
   `process`. Report latency from `latencyForParams`.
4. Write `tests/dsp/<Id>DspTests.cpp` for the arithmetic, and register it in
   `tests/CMakeLists.txt` under `bmo_add_dsp_tool`.
5. Write the panel over `ui::ModulePanel`. Height is fixed at
   `kContentHeight` (688); lay out at design size in `resized()`. Use
   `PlainKnob`, `ConcentricBand`, `SwitchButton`, `OutputMeter` and the
   rule helpers; do not draw text with an outline.

   **What a switch lights up in** is not a free choice:

   | switch | colour |
   |---|---|
   | the module's bypass | the module's accent |
   | **polarity** | **`tokens().polarity`, always** |
   | anything else | `tokens().switchAlt` |

   Polarity is the strict one. It means the same thing on every panel and
   is hunted for by sight rather than read, so it has to look identical
   everywhere; it spent three releases wearing each module's own accent
   before that was fixed. A module whose colour depends on its own state
   rather than on which module it is -- BMO Opto -- sets these at runtime
   instead, but follows the same table.

   Do not write a hex in a panel. If you need "the accent, but legible",
   that is `ui::accentTextOn`; for ink on a filled control it is
   `ui::onAccentOf`. Both derive against the current plate, which is what
   lets a theme change reach your module without it knowing.
6. Write factory presets. Init is index 0 and must be all defaults.
   Every preset should come out at the level it went in; the plugin tests
   check that.
7. `Module.cpp`: fill in a `ModuleDef` with the accent colour and the two
   factories. Add the module to `modules/CMakeLists.txt` with
   `bmo_add_module`.
8. Add a product under `products/<id>/` (three files, copy an existing
   one), register the module in `products/rack/Registry.cpp`, and link the
   new `bmo_<id>` into the rack, the snapshot tool and `rack_tests`.
9. Write `tests/plugin/<Id>Tests.cpp` with the golden schema table, and add
   the module's bank to `kBanks` in `RackTests.cpp`.
10. `scripts/build.sh --snapshots` and look at the panel, standalone and in
    the rack.

## Changing a module

- Adding a parameter: append to `specs()`, bump `kVersionHint`, give it a
  default that leaves old sessions sounding the same, extend `enum Index`,
  the golden schema table and the rack bank table.
- Changing the sound: before 1.0 it is free; after, it is a new product.
  Either way the DSP tests say what the numbers are, and they change with
  the code, deliberately.
