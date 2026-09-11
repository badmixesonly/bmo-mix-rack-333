# For AURORA — tasks Claude Code can finish without oversight

Written 2026-09-11 on **ICE QUEEN** (Frosty's desktop), for the Claude Code
session on **AURORA** (Frosty's laptop, `C:\Users\thesp`). Frosty assigned
these. They need no ears and no decisions from Frosty or Kevin: each one has
a rule to follow and a way to check it is done.

This file lives on its own fork branch, **`for-aurora`**
(`badmixesonly/bmo-mix-rack-333`), so ticking boxes never touches a working
branch. The fork has GitHub issues switched off, which is why this is a file.

## How to work through it

- **Tick as you go.** Change `- [ ]` to `- [x]`, and add the commit hash and
  "on AURORA" after the item. Commit the tick to `for-aurora` and push it to
  the fork. Do the work itself on the branch the item names, never on
  `for-aurora`.
- **Stay on the fork.** Push to `badmixesonly/bmo-mix-rack-333` only. Do not
  push to Kevin's repo (`kevkloud/bmo-mix-rack`), open PRs there, or touch any
  `frosty-*` branch there. Anything that reaches Kevin's main goes through
  Frosty.
- **Don't do the "not for AURORA" items at the bottom.** They need ears or
  someone's decision.
- **Stop if you're stuck.** If an item turns out to need a judgement this file
  doesn't settle, leave it unticked, write one line under it saying why, and
  move on.
- **Name the machine** in every note you write (`testing-notes/`, `AGENTS.md`,
  commit bodies): "on AURORA".
- **`scripts/build.sh` builds Debug and installs over AURORA's VST3 folder.**
  That's fine for development. Just say so if Frosty is about to listen on
  AURORA.

---

## 1. Name this machine

- [ ] **Create `C:\Users\thesp\.claude\CLAUDE.md`** from the AURORA block in
      the root `AGENTS.md`, section "Which machine you are on". Frosty handing
      you this list is the confirmation that this is the laptop. Machine-local:
      never commit it.

## 2. BMO Tune RT — get it onto the fork properly

BMO Tune RT was built on AURORA on 2026-09-10 and has not been pushed yet. A
copy of it is installed on ICE QUEEN, not from any recorded build.

- [ ] **Branch it on top of `add-bmo-deq`**, not from `main`, and not *on*
      `add-bmo-deq` itself. Call it `add-bmo-tune`. `modules/AGENTS.md`, "Two
      modules being written at once", says why: both modules edit the same
      shared files, and `RackTests.cpp` asserts an exact registry size. One
      stacked build then carries DEQ and Tune for a single round of DAW
      testing.
- [ ] **Claim a plugin code.** Four characters, same shape as the others
      (`B` + three lowercase). It must not be taken — `Fsty` `Bsat` `Butl`
      `Bopt` `Bdim` `Brck` `Bpar` (DEQ) — or reserved — `Bfet` `Bdyn` `Bdes`
      `Bovr` `Bcmp` `Bdly` `Brvb`. `Btun` is free today. **A plugin code is
      permanent once a build ships**, so claim it before the first CI build
      anyone installs.
- [ ] **Bundle id, preset extension, module id.** Follow the pattern in the
      identity table in `products/AGENTS.md`: `com.lt3audio.bmo<name>`,
      `.bmo<id>`, a short lowercase module id. Add the row to that table and
      make `products/<id>/CMakeLists.txt` agree with it.
- [ ] **Accent colour, by the method `products/AGENTS.md` "Accents" uses.**
      Hue separation from every other accent *and* the utility azure is the
      constraint that binds; then contrast on both plates, `#2e2e32` dark and
      `#efefef` pale. The shipped accents all clear 5.8:1 on the dark plate.
      A starting point, to verify, not to copy: the taken hues are 31.7°,
      128.9°, 172.0°, 198.8° (azure), 271.6° and 336.0°. The widest gap is
      31.7°–128.9°, and its middle (~80°, a yellow-green) keeps **~48.6°** from
      its nearest neighbours, which beats anything else left. Also stay clear
      of Opto's state colours, red `#e0685a` and amber `#e0b040`. Add the row
      to the Accents table with hue and both contrast figures, put the colour
      in the module's `ModuleDef`, then **render the panel with
      `tools/snapshot` and look at it** before ticking.
- [ ] **Every shared file a new module touches.** Work down the table in
      `modules/AGENTS.md` under that heading: all ten rows. Stacked on DEQ,
      `RackTests.cpp`'s registry size becomes 7, and its `kBanks` gets Tune's
      parameter list.
- [ ] **`modules/<id>/AGENTS.md` and `README.md`**, per the root `AGENTS.md`
      "Documenting new work", linked from `modules/AGENTS.md`.
- [ ] **Green, then pushed.** Full `ctest` in Release passes on AURORA.
      Push `add-bmo-tune` to the fork and dispatch the build:
      `gh workflow run build --repo badmixesonly/bmo-mix-rack-333 --ref add-bmo-tune`.
      Tick with the run id once Windows, macOS and DSP are green. That run's
      BMO-Windows artifact is what ICE QUEEN installs, by hash, for Frosty to
      test.

## 3. Colours: the `utilGain` placeholder

- [ ] **Recolour `utilGain` in `core/ui/Tokens.h`.** It is `#9c71c3`, hue
      ~275°, and its own comment calls it a placeholder left over from Opto's
      old lavender. It now sits ~3° from Dimension's accent, so in adjacent rack
      slots Util's GAIN knob and Dimension's whole panel read as the same
      purple. **Do this after Tune's accent is picked**, then choose by the same
      separation method against the full set, Tune included. With Tune near
      80°, the widest remaining gap is 198.8°–271.6°, centred ~235°, ~36° from
      the azure and from the lavender. Verify it. Update the comment in
      `Tokens.h` to say the colour was chosen and why, render Util and look
      before ticking. Kevin was asked about this colour in the 2026-09-10
      handoff note; say in the commit body that this answers it, so he sees it
      when it reaches him. Do it on `add-bmo-tune` (it's shared UI, and that
      branch carries the colour work) unless Frosty says otherwise.

## 4. BMO DEQ — the macOS test failure

- [ ] **Fix `deq_dsp`'s "T2 ceiling" failure on macOS.** Fork run 34562668139
      (`add-bmo-deq`, `900efdd`) failed only on macOS, and only this:

      FAIL: T2 ceiling: lowCut f0=0.000000 Q=0.000000 g=0.000000 region 1 -- limit 1.89, got 3.31336
      FAIL: T2 ceiling: lowCut f0=0.000000 Q=0.000000 g=0.000000 region 2 -- limit 1.69, got 2.40415

      It passes on Windows (MSVC) and Linux. **The all-zero band is the lead:**
      either the case's parameters are never set on that path, or the failure
      message prints the wrong fields. Either way something differs on clang —
      uninitialised data, evaluation order, or a platform `std::` difference.
      Find the cause; don't widen the limit. Fix it on `add-bmo-deq` (it's
      DEQ's own test) and push. Tick when a fork run of `add-bmo-deq` is green
      on all three jobs, with its run id. If `add-bmo-tune` is already
      stacked, rebase it onto the fix.

## 5. BMO DEQ — prepare the serial-vs-parallel blind pass

- [ ] **Render the blind set; do not listen and do not open the key.**
      `testing-notes/deq-topology-listening.md` §0: build `measure_deq`
      (DSP-only, Release) and run
      `measure_deq render <source.wav> <out-folder> --blind 7` into a
      **short** output path, for each source. Sources: a vocal with sibilance
      (the Fuji render, if AURORA has it), a full mix, and a kick-heavy loop.
      Ask Frosty for any AURORA doesn't have — a file with no low end can't
      test case 1, and one with no sibilance can't test case 7. Before
      ticking, **check the tool's table**: a dynamic case reading "max GR 0.0"
      never engaged, so its pair tests nothing about dynamics. Note which in
      the tick. Tell Frosty where the files are. The listening itself is his
      (see below).

---

## Not for AURORA — these need ears or a decision

- **The blind listening** in `deq-topology-listening.md` §2, and deciding
  serial vs parallel. That's Frosty's ear.
- **BMO DEQ's Ableton checklist** (`testing-notes/deq-testing-checklist.md`)
  — Frosty, planned for 2026-09-11 on ICE QUEEN.
- **Kevin's open questions** from the 2026-09-10 handoff note: Dimension's
  output stage, goniometer vs correlation meter, the DETUNE switch colour,
  and renaming BMO EQ to CEQ. `utilGain` (§3 above) is the exception: Frosty
  has cleared colours for AURORA.
- **Auditioning BMO Dimension's seven factory presets.**
- **Anything on Kevin's repo.** Frosty moves branches there and opens PRs.
