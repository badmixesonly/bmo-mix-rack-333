# Workflows — what branches off what, and the commands for each

**For the next Claude Code session, on either machine.** Written 2026-09-11 on
**AURORA**. Frosty set the plan; this file is the map, so a session can start
on one piece without re-deriving how the pieces fit.

This file lives on the **fork** (`badmixesonly/bmo-mix-rack-333`), on the
integration branch. It is not for Kevin's repository: it describes how Frosty's
two machines work, not how the suite is built.

---

## The rules that apply to every workflow below

- **Push to the fork only.** `origin` is `badmixesonly/bmo-mix-rack-333`.
  Nothing goes to `kevkloud/bmo-mix-rack` except through Frosty, as a pull
  request he opens. Never push a `frosty-*` branch there yourself.
- **Name the machine** in every note, handoff and commit body that records
  where a build, test, measurement or listening result happened: "on AURORA"
  or "on ICE QUEEN". Root `AGENTS.md` says how to tell which you are on.
- **Ask before pushing, and batch.** A CI round trip is about 22 minutes on
  Windows, and the workflow's concurrency group cancels a running build on the
  same ref.
- **Don't touch another worktree.** `..\bmo-mix-rack-333-deq` and
  `..\bmo-tune-rt` belong to other sessions.
- Frosty decides character, colour, version numbers and anything a listener
  would notice. Offer options with measured trade-offs; don't pick quietly.

## A new worktree needs two things before it builds

Neither travels with a branch, and both fail loudly:

```
scripts/set-font-dir.sh "C:/Users/thesp/OneDrive/Documents/FONTS"   # AURORA's path
git submodule update --init libs/JUCE
```

---

## The order, and why it is this order

```
main (Kevin's, mirrored on the fork)
 └── integration ─────────────────────────────────────────────┐
      ├── add-bmo-deq        DEQ, plus the macOS fix          │  stage 1
      ├── add-bmo-tune       Tune, in the repo not the rack   │  (merged in)
      │
      ├── deq-topology       serial vs parallel: a decision   │  stage 2
      ├── ceq-latency        BMO CEQ's latency work           │  parallel,
      ├── opto-high-gr       behaviour at high reduction      │  DSP only
      │
      └── ui-pass            everything a listener sees       │  stage 3
           ├── dim-controls  Dimension's controls audit       │  stage 4
           ├── sat-voicing   the voicing bell, by ear         │  parallel,
           └── (the listening passes for the above)           │  ears

bmo-tune-work      off integration, parallel to all of it, all the way through
```

**Stage 2 comes before the UI pass, not after it.** Anything that changes
*which controls exist* forces the panel to be laid out twice. Each item below
says whether it can.

**Stage 4 comes after it** for the opposite reason: a listening pass should
happen on the build that ships, or it gets done twice.

**The listening and the DSP measuring are different things.** Measuring needs
no UI and can start at stage 2. Ears want the finished panel.

---

## Dependency audit

What each workflow touches, and what that means. Confirm the "can it change
controls" column when you open the work — it is the question that decides the
order, and it is answered from the notes, not from the code.

| Workflow | Touches | Shares files with | Can it change controls? |
|---|---|---|---|
| `add-bmo-tune` | `modules/tune`, `products/tune`, `tools/tune`, `tests/*/tune`, the four CMakeLists, `products/AGENTS.md` | DEQ, in the CMakeLists and the identity tables | no |
| `add-bmo-deq` | `modules/deq`, `products/deq`, the rack's registry, `RackTests.cpp`, the same CMakeLists | Tune, as above | no — the macOS fix is a test fault |
| `deq-topology` | `modules/deq/dsp`, `tests/dsp/DeqDspTests.cpp`, `testing-notes/deq-topology-listening.md` | nothing else | **yes** — a topology choice can add a control |
| `ceq-latency` | `modules/eq/dsp`, `tests/dsp/EqDspTests.cpp` | nothing else | **yes** — oversampling is a control, and it sets the latency |
| `opto-high-gr` | `modules/opto/dsp`, `tests/dsp/OptoDspTests.cpp` | nothing else | probably not — confirm from `opto-0.2.1-handoff.md` |
| `sat-voicing` | ears, then maybe `modules/sat/dsp/Filters.h` | nothing else | no — it is a retest of Kevin's change |
| `ui-pass` | `core/ui/*`, every `modules/*/panel`, `tests/ui/LayoutTests.cpp`, `tools/snapshot` | **every module at once** | it *is* the control work |
| `dim-controls` | Dimension's panel | the UI pass, heavily | yes — fold it into the pass |
| `bmo-tune-work` | `modules/tune/dsp`, `tools/tune`, `tests/dsp/tune` | nothing in the rack | yes, but only Tune's own |

**Why the UI pass cannot be parallel.** Every panel is built on the same
`core/ui` tokens, controls and look-and-feel, and one layout test walks all of
them. The last pass was 45 commits across every panel. Five branches editing
panels at once means five sets of conflicts in the same files.

**Why the DSP workflows can be.** They touch one module's `dsp/` and its own
test file. No two of them meet, and none of them meets the UI pass.

**Tune is independent of all of it** — different folders, and the build
switches keep it that way — but its *panel* is drawn with the same shared UI
code, so Tune's UI work belongs in the UI pass, not in `bmo-tune-work`.

---

## The commands, per workflow

### Starting any workflow

```
git -C <worktree> fetch origin
git worktree add -b <branch> ../bmo-mix-rack-333-<short> origin/integration
```

Then the two setup lines above, then build.

### Building and testing

```
# everything, Release, with the plugins                  (~20 min cold)
cmake -S . -B build && cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure

# DSP only: no JUCE, no plugins                          (~2 min)
cmake -S . -B build-dsp -DBMO_DSP_ONLY=ON
cmake --build build-dsp --config Release --parallel
ctest --test-dir build-dsp -C Release --output-on-failure

# the two sides on their own — what keeps Tune out of the rack
cmake -S . -B build-tune -DBMO_DSP_ONLY=ON -DBMO_BUILD_RACK=OFF
cmake -S . -B build-rack -DBMO_DSP_ONLY=ON -DBMO_BUILD_TUNE=OFF

# Debug, and installs over this machine's VST3 folder — say so before Frosty listens
scripts/build.sh
scripts/build.sh --snapshots       # also renders every rack panel to snapshots/
```

`ctest` names Tune's suites `tune_*`. `tune_hardtune_target` is **disabled on
purpose** — it is the open hard-tune work, and the change that makes it pass
enables it.

### Looking at a panel without a 20-minute build

```
build/tools/Release/snapshot.exe <eq|sat|util|opto|dim|deq|rack> out.png [param=value ...]
build/tools/tune/Release/bmo-tune-snapshot.exe out.png [retune_ms=12 appearance=dark]
```

### Measuring, per module

```
build/tools/Release/measure_eq.exe      # and measure_sat, measure_opto, measure_dim
build/tools/Release/measure_deq.exe     # on add-bmo-deq
build/tools/tune/Release/bmo-tune-latency.exe --range all
build/tools/tune/Release/bmo-tune-ref.exe bmo
build/tools/tune/Release/bmo-tune-field.exe
```

### CI on the fork

CI does not run itself on a feature branch. Dispatch it, and give Frosty the
run id:

```
gh workflow run build.yml --repo badmixesonly/bmo-mix-rack-333 --ref <branch>
gh run list --repo badmixesonly/bmo-mix-rack-333 --limit 5
```

Jobs: **DSP** (Linux, seconds), **Each side alone** (Linux, the two switches),
**macOS** and **Windows** (the plugins, ~20-25 min). The Windows job's
`BMO-Windows` artifact is what ICE QUEEN installs, by hash.

### Finishing a workflow

1. Full `ctest` in Release passes on the machine you are on.
2. Push the branch to the fork, dispatch CI, wait for all four jobs.
3. Write what you measured into `testing-notes/`, naming the machine.
4. Tell Frosty. Merging into `integration`, and anything that reaches Kevin,
   is his.

---

## The workflows, one by one

### `add-bmo-deq` — DEQ, and the macOS failure

Branch exists, three commits on `main`. Fork run 34562668139 failed on macOS
only: `deq_dsp` "T2 ceiling", `lowCut f0=0 Q=0 g=0`, limit 1.89, got 3.31.
Windows and Linux pass. **The all-zero band is the lead** — either the case's
parameters are never set on that path, or the failure message prints the wrong
fields. Find the cause; do not widen the limit. Green on all three jobs, with
the run id, is what done looks like.

### `add-bmo-tune` — Tune in the repository, not in the rack

Done on AURORA, 2026-09-11: history merged, tools and tests relocated, build
switches, identity in the tables. Not yet pushed. What is left is Frosty's
call on the lime accent's 1.29 contrast on the pale plate
(`products/AGENTS.md`), and a CI run.

### `deq-topology` — serial or parallel

`testing-notes/deq-topology-listening.md` §0 renders the blind set; §2 is
Frosty's ear. **Do not open the key.** Render, check the tool's table for a
case that never engaged ("max GR 0.0" tests nothing), tell Frosty where the
files are, and stop.

### `ceq-latency` — BMO CEQ's latency work

BMO EQ is the one module in the suite whose default is not zero-latency: its
oversampling sets `latencyForParams`. **BMO EQ is renamed BMO CEQ** (Frosty,
2026-09-11); the plugin code `Fsty`, the bundle id and both preset extensions
do **not** change, because that is what makes existing sessions open. The
rename touches the display name, the header and the docs, so do it with the UI
pass rather than here.

### `opto-high-gr` — behaviour at high reduction

Start from `testing-notes/opto-0.2.1-handoff.md` and
`opto-testing-checklist.md`. House rule from this module's own history:
**assert absolutes, not comparisons** — a relative release test passed for a
whole release while both modes were broken.

### `sat-voicing` — the voicing bell, by ear

`testing-notes/saturator-voicing-retest.md`. Kevin's `51a263b` changed the
voicing bell and it has never been heard on either machine. Ears, on the
build that ships.

### `ui-pass` — everything a listener sees

The big one, and the one that has to be alone. Read
`docs/ui-workflow-brief.md` and `testing-notes/ui-editor-handoff.md` first —
45 commits of prior art, what was tried and thrown away, and the loop that
makes this cheap. In scope: layout, colour and controls across every rack
module and Tune; Dimension's controls audit; the `utilGain` placeholder
(`core/ui/Tokens.h`, `#9c71c3`, about 3 degrees from Dimension's accent, so in
adjacent slots they read as the same purple); and the BMO CEQ rename. Out of
scope: anything that changes what a control *does*.

### `bmo-tune-work` — Tune's own testing and finish

Runs parallel to all of it. Start from `testing-notes/tune-handoff.md`. Open:
the round-three blind set, the neighbour-note flips at vibrato 0, detector
jumps on scoops, octave-down at creaky phrase ends, voicing dropouts, then the
correction lag. The latency rule (10.62 ms) governs every change.
