# UI editor — handoff

Where the `ui-editor` branch starts from, what is proven to work, what is
still open, and the traps. Written for someone picking this up cold in a
fresh session.

Everything under "Verified" below was actually run on this machine on
2026-09-07 and the output read. Everything under "Not verified" was not —
the distinction is kept deliberately, because until today every claim about
how these panels *look* was arithmetic off the source rather than something
seen.

---

## 1. Where the branch starts

`ui-editor` is branched from `main` at `6fdf8d9`, which is current for the
first time: it now carries both the Opto line of work and the font change.

| | |
|---|---|
| `main` | `6fdf8d9` — merge of PR #2 (fonts) on top of PR #1 (Opto) |
| Opto | 25 commits, merged as a merge commit so the measurement history survives |
| Fonts | `BMO_FONT_DIR` resolution, `scripts/set-font-dir.sh` |
| `snapshot` | links `bmo_eq bmo_sat bmo_util bmo_opto` and the rack registry |

Before this, `main` was a single commit with no Opto at all. Anything that
branched from the old `main` is missing the module, the product, the
measurement harness and the presets.

**Do not delete the `add-bmo-opto` branch.** It is the head of an open PR
against Kevin's original repository — `kevkloud/bmo-mix-rack#1`, still OPEN
and MERGEABLE. Deleting the branch closes his PR. Merging into this fork's
`main` deliberately did not touch it, and the merges here used no
`--delete-branch` for that reason.

---

## 2. Verified: the build works, headlessly, on Windows

This is new. Both former blockers are gone.

- **cmake** 4.4.3, installed via `winget install --id Kitware.CMake`, at
  `C:\Program Files\CMake\bin` on the machine PATH. VS 2022 Community
  supplies MSVC (14.38 / 14.44) and the "Visual Studio 17 2022" generator.
  The VS "C++ CMake tools" component is *not* installed, which is why
  nothing was found before.
- **Fonts** resolve from outside the repository (section 3).
- **`tools/snapshot`** builds and renders with no display attached, exit 0.

Recipe, from a clean checkout:

    cmake -S . -B build
    cmake --build build --target snapshot --config Debug --parallel
    ./build/tools/Debug/snapshot.exe opto snapshots/opto.png
    ./build/tools/Debug/snapshot.exe rack snapshots/rack.png chain=util,eq,sat,opto

A correct configure prints the font folder it chose:

    -- BMO fonts: C:/Users/stefr/Documents/FONTS (.bmo-fontdir)

Renders come out at 2x. Observed sizes: opto 440x1480, eq 560x1480,
sat 520x1480, util 320x1480, rack 1920x1480.

Also verified: all four DSP suites pass (`ctest -C Debug`), and `BmoAssets`
compiles with both faces embedded at 35008 and 36616 bytes, matching the
source files exactly.

### Two things that will bite

**`scripts/build.sh --snapshots` is broken on Windows.** Lines 23-27 call
`./build/tools/snapshot`, but the multi-config VS generator puts it at
`build/tools/Debug/snapshot.exe`. The script is fine on macOS. Making those
paths config-aware is a good first commit on this branch.

**`snapshots/` is gitignored**, so renders never travel with a branch. Re-run
the tool rather than looking for committed PNGs.

---

## 3. Fonts: the trap for a second session

The two display faces are licensed to Frosty and Kevin as individuals, not to
the project, and this repository is public. They are never committed. CMake
resolves the directory, first hit wins:

1. `-DBMO_FONT_DIR=<path>` — one-off; sticks in that build cache
2. `$BMO_FONT_DIR` — a shell or CI environment
3. `.bmo-fontdir` — per-working-copy constant, gitignored
4. `assets/fonts/` — what CI decodes from secrets

On this machine the faces are at `C:\Users\stefr\Documents\FONTS` and
`.bmo-fontdir` already points there.

**The trap:** `.bmo-fontdir` is a file in the working tree and is not in git.
A second worktree, a fresh clone, or a Claude Code isolation worktree does
**not** get it — the existing worktree under `.claude/worktrees/` was checked
and does not have it. The build then falls back to an empty `assets/fonts/`
and stops with a fatal error naming the missing file.

Two fixes, either is fine:

    scripts/set-font-dir.sh /c/Users/stefr/Documents/FONTS   # per working copy
    setx BMO_FONT_DIR "C:/Users/stefr/Documents/FONTS"       # machine-wide, all clones

The env var is the better one for a multi-session workflow: it covers every
clone and worktree at once. It only affects shells started afterwards.

`.bmo-fontdir` is also listed in `.git/info/exclude` on this clone, so it
stays ignored on branches that predate the change. That file is local and
does not travel — a fresh clone needs the `.gitignore` rule, which `main`
now has.

A **cloud** session cannot build the UI at all: it clones fresh and has no
fonts, and they are not in the repository by design. Only GitHub Actions can,
via the `FONT_TG_*` secrets. This workflow is local-only, and that is the
licence working as intended, not a gap to close.

---

## 4. What the first render actually shows

The Opto panel was rendered and looked at. The crowding that had been
suspected from the source is real, and worse than the arithmetic suggested:

- **The VU scale collides.** `-10 -7 -5 -3` overlap into near-illegibility.
  The `0` and `3` sit on a visibly different baseline from the rest.
- **The needle crosses the `-20` label**, which is itself partly overdrawn.
- **Tick marks do not line up with their numbers.**
- **Three large dead bands**: TELE to COMP, meter to MAKEUP, and below LINK.
- **`COLOR` is clipped** at the bottom edge of the panel.

Sources: `modules/opto/panel/OptoPanel.cpp` for the panel, `core/ui/Controls.cpp`
for the meter drawing, `core/ui/Tokens.h` for the spacing and colour tokens.

This is the first direct evidence about layout in this project. Prefer
rendering over reasoning from here on: the tool is fast and the source is
misleading about the result.

---

## 5. Open: the direction has not been chosen

`core/ui/Tokens.h` is **untouched** and should stay that way until the
direction is settled. The three options on the table, unchanged:

- **Repair** — fix the collisions and clipping, change nothing else.
- **Re-value** — keep the art direction, re-derive the spacing and type
  scale so the dead bands and the crowding both go away.
- **Re-art-direct** — treat the panel as a fresh design problem.

This is Frosty's call and was explicitly reserved for him. The renders in
section 4 are new evidence and worth re-deciding against.

Also unanswered: whether the editor targets the whole rack or Opto first.

---

## 6. Not verified

Stated plainly so nobody inherits it as fact:

- The snapshot tool has only been run in Debug. Release is untested.
- Nothing has been checked against a real DAW render at 1x; all observations
  above are from the 2x PNGs.
- No plugin target (VST3 / Standalone) has been built on this machine — only
  `snapshot`, `BmoAssets` and the DSP tests. A full plugin build may surface
  its own problems.
- `scripts/build.sh --snapshots` has never completed on Windows, for the path
  reason in section 2.
