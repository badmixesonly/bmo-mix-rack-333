# BMO Mix Rack

**BMO (Bad Mixes Only)** is a set of mixing plugins by **LT3a**. This
repository holds the whole suite: three modules today, one rack that chains
them, and the shared code that makes a new module a few files rather than a
new plugin.

| Product | What it is | Width |
|---|---|---|
| **BMO EQ** | FrostyEQ, renamed: a Neve-style three-band EQ with low cut, oversampled | 280 |
| **BMO Saturator** | Drive, tone and blend, with auto-gain | 260 |
| **BMO Util** | Gain, pan, width, polarity, mono | 160 |
| **BMO Mix Rack** | Up to eight of the above in series, re-orderable, with chain presets | as wide as its modules |
| **BMO Tune RT** | A low-latency monophonic pitch corrector. In this repository, **not in the rack** | 360 |

Every product ships as VST3 and Standalone on macOS and Windows, and AU on
macOS. Nothing is signed or notarised yet: this is a tester build.

BMO Tune RT is built here and shares `core/`, but it is in no rack chain.
Either side builds without the other: `-DBMO_BUILD_TUNE=OFF` for the rack
alone, `-DBMO_BUILD_RACK=OFF` for Tune alone. See
[`modules/tune/AGENTS.md`](modules/tune/AGENTS.md).

## Building

```
git clone --recursive https://github.com/kevkloud/bmo-mix-rack
cp <the two .otf files> assets/fonts/        # see assets/fonts/README.md
scripts/build.sh                              # Debug, arm64, runs the tests
scripts/build.sh --snapshots                  # also renders every panel to snapshots/
scripts/package.sh                            # Release zip in packages/v1/
```

Requirements: CMake 3.22+, a C++20 compiler (Xcode 15 / MSVC 2022), and the
two licensed typefaces. Without JUCE at all:

```
cmake -B build-dsp -DBMO_DSP_ONLY=ON && cmake --build build-dsp && ctest --test-dir build-dsp
```

builds and runs the DSP tests and the measurement harnesses, which is what
CI's fast job does on a bare Linux container.

## Layout

```
core/       shared code: dsp/ (JUCE-free), state/, ui/, product/, rack/
modules/    one folder per module: params.h, dsp/, panel/, presets/, Module.cpp
products/   one thin CMakeLists + Product.h per plugin, and the rack's registry
tools/      measure/ (offline DSP harnesses), snapshot/ (renders a panel), packager/
            tune/ (BMO Tune RT's own harnesses and its own snapshot)
tests/      dsp/ (JUCE-free) and plugin/ (schema, state, presets, rack)
            dsp/tune/ and plugin/tune/ are BMO Tune RT's, named tune_* in ctest
design/     tune/ panel studies
libs/JUCE   submodule, pinned
```

`AGENTS.md` at the root and in `core/`, `modules/` and `products/` explain the
rules for working in each. **Adding a module** is described in
`modules/AGENTS.md`.

## The contract a session relies on

Parameter IDs, their order, ranges and defaults, plugin codes, bundle IDs and
the manufacturer code `LT3a` are all permanent once a session has been saved
with them. `tests/plugin/*Tests.cpp` write each product's schema out in full
and fail the build if it drifts. The rack's 8 x 32 generic parameter grid
(`slot1_p01` ... `slot8_p32`) maps to each module in spec order, and
`RackTests.cpp` pins that mapping per module. A module may have more than 32
parameters. The ones past the 32nd are saved with the rack and work on its
panel, but they have no host lane, so a rack cannot automate them.

## Presets and themes

Presets live under `~/Library/Audio/Presets/LT3 Audio/<Product>/` on macOS
and `%APPDATA%\LT3 Audio\<Product>\Presets\` on Windows. BMO EQ migrates a
FrostyEQ preset folder on first run.

A theme is a flat JSON file of token name to hex colour at
`LT3 Audio/Themes/Default.json`; every open editor re-reads it once a second.
The token names are the fields of `core/ui/Tokens.h`.

## Licence

MIT, see `LICENSE`. The typefaces are not part of the licence and are not in
the repository.
