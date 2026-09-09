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

# Resolve our own directory with parameter expansion rather than dirname. The
# cut-down git bundled inside GitHub Desktop ships bash but no coreutils, and
# there `$(dirname "$0")` came back empty, so this turned into `cd /..` and
# every path below resolved against the root of the MSYS tree instead of the
# repository. It did not fail: a missing command inside a command substitution
# does not trip set -e when the substitution is an argument, so the script went
# on to write its marker into git's own install directory and report success.
self_dir=${0%/*}
if [[ $self_dir == "$0" ]]; then
    self_dir=.
fi
cd "$self_dir/.."

readonly MARKER=.bmo-fontdir
readonly FACES=(TG-MinervaBlack-Black.otf TG-Blender.otf)

# CMake cannot read an MSYS/Cygwin path like /c/Users/... , so record a real
# Windows path when we are on Git Bash. Elsewhere the plain path is correct.
#
# cygpath is the right tool for that and is present in a full Git for Windows,
# but not in the git inside GitHub Desktop -- which is the only git on a
# machine nobody has installed Git for Windows on. The old fallback there
# printed the MSYS path unchanged, so the marker was written with a path CMake
# cannot read and the next configure stopped on a font it could not find,
# naming the folder that does hold it. Do the conversion ourselves, and refuse
# rather than record something we know will not resolve.
to_cmake_path() {
    if command -v cygpath >/dev/null 2>&1; then
        cygpath -m "$1"
        return
    fi

    case "$1" in
        /?/*)
            # /c/Users/you/Fonts -> C:/Users/you/Fonts. ${drive^^} is bash's
            # own uppercase, so this needs no tr either.
            local drive=${1:1:1}
            printf '%s:/%s' "${drive^^}" "${1:3}"
            ;;
        /*)
            if [[ ${OSTYPE:-} == msys* || ${OSTYPE:-} == cygwin* ]]; then
                echo "Cannot record $1 in a form CMake can read." >&2
                echo >&2
                echo "It is an MSYS path and cygpath is not installed, so it" >&2
                echo "cannot be turned into a Windows one. Either install Git" >&2
                echo "for Windows, which carries cygpath, or pass the folder" >&2
                echo "as a Windows path: scripts/set-font-dir.sh C:/path/to/fonts" >&2
                return 1
            fi
            printf '%s' "$1"
            ;;
        *)
            printf '%s' "$1"
            ;;
    esac
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

# Convert before opening the marker, not into it. A redirection truncates its
# file before the command on the left runs, so converting straight into the
# marker would leave an empty one behind on any failure -- and an empty marker
# is worse than no marker, because CMake reads it and looks for the faces in
# the repository root rather than falling back to assets/fonts/.
cmake_dir=$(to_cmake_path "$dir")
printf '%s\n' "$cmake_dir" > "$MARKER"

echo "Font folder recorded in $MARKER:"
echo "    $(<"$MARKER")"
echo
echo "$MARKER is gitignored. Every build in this working copy now reads the"
echo "faces from there; re-run CMake configure if a build tree already exists."
