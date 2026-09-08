# Render inspection harness

Answers the questions a look at a panel raises but cannot settle: what colour
is that exactly, is that one shape or three, did this refactor move a pixel.

Written in C# and **deliberately not wired into `tools/CMakeLists.txt`**, for
the same reasons as `tools/measure/renders` — see its README. A tool for
looking at renders should never be able to fail a plugin build.

These existed as throwaway PowerShell in a session scratchpad and died with
it, which cost the next session half an hour to rebuild. This is them written
down.

## Running it

    csc -out:Inspect.exe -r:System.Drawing.dll Inspect.cs   # Framework64/v4.0.30319/csc.exe
    ./Inspect.exe <mode> ...

| mode | what it does |
|---|---|
| `scan`  | run-length along one row or column: start, end, width, exact hex |
| `hist`  | exact-colour histogram over a box, with L\* per colour |
| `crop`  | crop and magnify, nearest-neighbour |
| `sheet` | several renders side by side with labels |
| `hash`  | SHA-256 of the pixels, for before/after on a refactor |
| `ratio` | contrast and L\* between colours, every pair both ways |

`ratio` takes either a hex or a pixel out of a render:

    ./Inspect.exe ratio "#8d8d98" snapshots/rack.png:260,364

## Why each one exists

Each mode is the answer to a specific way of being wrong, all of which
happened. `testing-notes/ui-editor-handoff.md` §6 is the long version.

- **`scan` is the one that matters most.** Squinting at a magnified crop
  cannot tell you two inks are the same to within 1.01:1. The dark band
  selector looked like a ring; scanned, rows 355-376 of `ring-dark.png` come
  back `#8d8d98`, `#8e8e93`, `#8d8d98` running together as a 22 px slab,
  because the hairlines and the face were the same colour. No amount of
  looking produces that number.
- **`hist` replaces "the darkest pixel in that box".** Two inks 128 apart in
  hex can be a third of an L\* apart: `#497450` and `#317290` are 44.9 and
  45.2, which is 1.01:1. Darkest-pixel is a coin flip between a caption and
  the rule beside it. Count exact matches instead.
- **`crop` never smooths.** Interpolation would invent colours that are not in
  the render, which is the one thing an inspection tool must not do.
- **`sheet` is the cheap answer to "is this the same as that"**, and to
  showing candidate layouts rather than describing them. Its ground is
  deliberately neither plate, so it cannot be mistaken for part of a render.
- **`hash` settles a refactor instead of arguing it.** Render every panel,
  refactor, render again, compare. Byte-identical or it was not a refactor.
  It hashes pixels rather than file bytes, because a PNG encoder is free to
  vary everything around them.
- **`ratio` prints L\* beside the contrast** because below about L\* 20 the
  ratio stops discriminating: from the dark plate the most contrast available
  by going darker, all the way to black, is 1.29:1. The dark set's structural
  greys are spaced by lightness for that reason.

## Things it cost a wrong answer to learn

- **`crop` and `sheet` refuse to overwrite.** A stale read of a path that had
  only been re-rendered in the reader's head cost a full debugging cycle
  chasing a bug that did not exist. Render to a unique filename; the tool now
  enforces it rather than trusting you to remember.
- **A ratio without a named ground is not a measurement.** "The marker is
  4.67:1" was true and useless. `ratio` prints every pair both ways round so
  the ground is always on the line with the number.
- **Both appearances, every time.** Half the faults on the `ui-editor` branch
  existed in one only. A change measured on the pale plate has not been
  checked.
- **Check what the baseline actually is.** Two "regressions" were a baseline
  with different switch states, and a baseline that predated an intervening
  commit. `hash` is faster than re-deriving that by eye.
- An unknown mode is refused rather than quietly doing nothing, the way
  `tools/snapshot`'s `realValueFor` now refuses a mistyped choice name. A
  no-op that prints nothing reads as an answer.
