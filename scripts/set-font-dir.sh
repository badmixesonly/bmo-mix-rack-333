#!/usr/bin/env bash
# Point this working copy at the folder where you keep the licensed .otf files.
#
#   scripts/set-font-dir.sh <path>   record that folder for every build here
#   scripts/set-font-dir.sh          show what is recorded now
#   scripts/set-font-dir.sh --clear  forget it
#
# The two display faces are licensed to Frosty and Kevin as individuals, not
# to this project, and this repository is public. They are therefore never
# committed. Instead of copying them in, this records *where they already
# live* in .bmo-fontdir, which is gitignored. The files stay put; the build
# reads them from there.
#
# Your font folder is yours. Nothing in this repository writes to it, moves
# anything in it, or cleans it: the only reason to open it is to add a newly
# licensed face for a future build.

set -euo pipefail
cd "$(dirname "$0")/.."

readonly MARKER=.bmo-fontdir
readonly FACES=(TG-MinervaBlack-Black.otf TG-Blender.otf)

# CMake cannot read an MSYS/Cygwin path like /c/Users/... , so record a real
# Windows path when we are on Git Bash. Elsewhere the plain path is correct.
to_cmake_path() {
    if command -v cygpath >/dev/null 2>&1; then
        cygpath -m "$1"
    else
        printf '%s' "$1"
    fi
}

case "${1:-}" in
    "")
        if [[ -f $MARKER ]]; then
            dir=$(<"$MARKER")
            echo "Font folder: $dir"
            for face in "${FACES[@]}"; do
                if [[ -f "$dir/$face" ]]; then
                    echo "  ok      $face"
                else
                    echo "  MISSING $face"
                fi
            done
        else
            echo "No font folder recorded; builds fall back to assets/fonts/."
            echo "Set one with: scripts/set-font-dir.sh <path>"
        fi
        exit 0
        ;;
    --clear)
        rm -f "$MARKER"
        echo "Forgot the recorded font folder. Builds fall back to assets/fonts/."
        exit 0
        ;;
    -h|--help)
        sed -n '2,18p' "$0" | sed 's/^# \{0,1\}//'
        exit 0
        ;;
esac

dir=$1

[[ -d $dir ]] || { echo "Not a directory: $dir" >&2; exit 1; }

dir=$(cd "$dir" && pwd)

missing=()
for face in "${FACES[@]}"; do
    [[ -f "$dir/$face" ]] || missing+=("$face")
done

if (( ${#missing[@]} )); then
    echo "That folder is missing ${#missing[@]} of the ${#FACES[@]} faces this build needs:" >&2
    printf '    %s\n' "${missing[@]}" >&2
    echo >&2
    echo "Looked in: $dir" >&2
    echo "Nothing was recorded. Point this at the folder holding both .otf files." >&2
    exit 1
fi

# Keeping them inside the working tree is what we are trying to avoid: one
# forced add, or one clone by someone else, and the licence is broken.
repo=$(pwd)
if [[ $dir == "$repo"/* || $dir == "$repo" ]]; then
    echo "Warning: $dir is inside this repository." >&2
    echo "         Keep the licensed faces outside it, so no git operation" >&2
    echo "         can ever pick them up. Recording it anyway." >&2
fi

to_cmake_path "$dir" > "$MARKER"
printf '\n' >> "$MARKER"

echo "Font folder recorded in $MARKER:"
echo "    $(<"$MARKER")"
echo
echo "$MARKER is gitignored. Every build in this working copy now reads the"
echo "faces from there; re-run CMake configure if a build tree already exists."
