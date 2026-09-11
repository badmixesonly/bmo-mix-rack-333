# BMO Tune RT -- handoff, 2026-09-10, AURORA

Everything a cold start needs. Read this, then `modules/tune/AGENTS.md`.

**The DSP is done and measured. Nothing has been heard in a DAW, and there is
no plugin yet.** The next real step is a listening pass on renders from the
CLI, then the JUCE wrapper.

## 1. Where it is

- Repository: `C:\Users\thesp\OneDrive\Documents\REPO\bmo-tune-rt`, a sibling
  of `bmo-mix-rack-333`. Local git only -- **no remote, nothing pushed**, by
  Frosty's decision on 2026-09-10. Pushes wait for Frosty's approval.
- Branch `main`, eight commits, each a coherent stage with its evidence in the
  message.
- It builds against the rack checkout next to it (`BMO_RACK_DIR`), for
  `core/dsp/ModuleDsp.h` and `core/state/ParamSpec.h`. Nothing in the rack was
  changed.
- Licence: the rack's `LICENSE` (AGPL-3.0), copied verbatim from Kevin's
  `main`. **Kevin's README says "MIT, see LICENSE"**, which contradicts the file
  it points at; this repository's README says AGPL-3.0 to match its file. That
  line is Kevin's to settle.

## 2. Decisions taken with Frosty, 2026-09-10

| Question (spec Part IV) | Decision |
|---|---|
| Where it lives | new sibling repo, reusing the rack's shared code |
| Push target | local only for now |
| Pitch range floor (Q3) | 80 Hz default and Auto; 55 Hz in Bass and Instrument |
| Latency contract (Q4) | **Live** default; Studio as the option |
| Flex (§4.3a) | an ordinary parameter, default 0; not the plugin's focus -- hard tuning and retune speed are |
| Licence | follow Kevin's main |

The two patents the spec flagged for Flex are Smule's karaoke patents, not
Antares' -- checked at Google Patents. The research digest itself is not on
AURORA, so the spec's citation could not be traced back to it.

## 3. Build and check

```
bash scripts/build.sh              # Release, eight suites, ~30 s
bash scripts/build.sh --corpus     # + 72-item corpus, ~15 s
build/tools/Release/bmo-tune-latency --range all
build/tools/Release/bmo-tune-bench --quick
```

AURORA has CMake 4.4 and MSVC 2022. **No clang and no Python**: every tool is
C++, and rtsan/TSan/UBSan are wired for a clang toolchain but not run.

## 4. Next steps, in order

1. **Listen.** Render real vocals through `bmo-tune-cli` with both engines at
   retune 0, 20 and 50, and the vibrato-0 warble case (§5 below). Record what
   was heard, with settings and the machine, before proposing any change.
2. **Decide vibrato 0's note decision** (§5) on what was heard.
3. **The JUCE wrapper.** It needs MIDI in, which the rack's
   `SingleModuleProcessor` does not take (`acceptsMidi() == false`), so it is a
   processor of its own built from the rack's `core/state` and `core/ui`, with
   `TuneDsp` as its engine. Allocate the plugin code, bundle id, preset
   extension and accent in the rack's `products/AGENTS.md` table first.
4. **pluginval, host matrix, Ableton pass** -- Frosty's, on the wrapper build.
5. **Real corpora** (PTDB-TUG etc.) need downloading; ask first.

## 5. Open questions

- **Vibrato 0 warbles across a note boundary** -- deliberate, asserted, and a
  listening call. See `modules/tune/AGENTS.md`.
- **The LPC formant stage** is out of the signal path, with the reasons and
  what would bring it back recorded in `modules/tune/AGENTS.md`. PSOLA alone
  meets the formant gate.
- **192 kHz / 32-sample p99** is lumpy (~30 % of a block) because one
  refinement lands in one block. Fine at 48 kHz.
- **Kevin's README MIT line** vs his AGPL `LICENSE`.

## 6. Method lessons from this session

- **The spec's numbers were measured, not trusted, and several were wrong**:
  the 16-tap kernel's error, the flex curve's smoothness, a period of PSOLA
  lookahead, and the patents' owner. Each correction is in the code with its
  measurement, where the next person will look.
- **The corpus found what the unit tests could not.** A +1200-cent glitch at
  note ends came out of scoring 72 items, not from any test written for it.
  Run the corpus after any DSP change.
- **Measurement tools have bugs of their own.** The latency rig first picked
  arbitrary cross-correlation peaks on a periodic tone, then measured a
  detector bias as latency; the formant ruler first could not see past 10 %.
  Each fault is recorded at the point of use so it is not reintroduced.
