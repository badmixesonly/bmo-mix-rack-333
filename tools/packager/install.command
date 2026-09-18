#!/usr/bin/env bash
# Install this tester build on macOS. Double-click it, or run it from a
# terminal in the unzipped folder.
#
# Three things, in this order:
#   1. remove bundles a previous build installed under a name this one no
#      longer uses (superseded.txt), so the DAW does not list both;
#   2. copy VST3 and AU into the user's plug-in folders;
#   3. clear the macOS quarantine flag on exactly what was installed.
#
# Step 3 is why "BMO CEQ is damaged and can't be opened" happens: these builds
# are not signed or notarised, so Gatekeeper quarantines anything unzipped from
# a download. Clearing it by the names actually installed, rather than by a
# glob, is deliberate -- the old README globbed BMO* and silently missed
# LTV Comp, the one bundle whose name does not start with BMO.

set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
vst3_dir="$HOME/Library/Audio/Plug-Ins/VST3"
au_dir="$HOME/Library/Audio/Plug-Ins/Components"

[ -d "$here/VST3" ] || { echo "No VST3 folder beside this script. Run it from the unzipped package."; exit 1; }

# What this build ships, by base name. Used to install, to clear quarantine,
# and as the guard on removal below.
shipping=()
while IFS= read -r n; do shipping+=("$n"); done \
    < <(find "$here/VST3" -maxdepth 1 -type d -name '*.vst3' | sed 's#.*/##; s#\.vst3$##' | sort)

echo "Installing ${#shipping[@]} plugins."
echo

# -- 1. superseded ---------------------------------------------------------
if [ -f "$here/superseded.txt" ]; then
    removed=0
    while IFS='|' read -r old replacement _rest; do
        # tr -d '\015' -- superseded.txt is pinned to LF in .gitattributes, but
        # strip any stray CR anyway so a hand-edited list cannot break the guard.
        old=$(printf %s "$old" | tr -d '\015' | sed 's/^ *//; s/ *$//')
        replacement=$(printf %s "$replacement" | tr -d '\015' | sed 's/^ *//; s/ *$//')
        [ -z "$old" ] && continue
        case "$old" in \#*) continue ;; esac

        # Never remove something this build also ships under that name.
        for s in "${shipping[@]}"; do
            [ "$s" = "$old" ] && { old=""; break; }
        done
        [ -z "$old" ] && continue

        for victim in "$vst3_dir/$old.vst3" "$au_dir/$old.component"; do
            if [ -e "$victim" ]; then
                [ "$removed" -eq 0 ] && echo "Removing superseded bundles"
                echo "  - $(basename "$victim")   (now $replacement)"
                rm -rf "$victim"
                removed=$((removed + 1))
            fi
        done
    done < "$here/superseded.txt"
    [ "$removed" -gt 0 ] && echo
fi

# -- 2. copy ---------------------------------------------------------------
# Each bundle is removed before it is copied. BSD cp -R -- which is what runs
# on macOS -- copies a directory INTO an existing directory of the same name,
# so upgrading in place without this leaves BMO Opto.vst3/BMO Opto.vst3 and a
# plugin the DAW can no longer load. GNU cp does not, so the bug does not
# reproduce when this script is tested under Git Bash on Windows.
install_bundle() {
    target="$2/$(basename "$1")"
    rm -rf "$target"
    cp -R "$1" "$2/"
}

mkdir -p "$vst3_dir"
echo "Installing -> $vst3_dir"
for b in "$here/VST3/"*.vst3; do install_bundle "$b" "$vst3_dir"; done

if [ -d "$here/AU" ] && compgen -G "$here/AU/*.component" > /dev/null; then
    mkdir -p "$au_dir"
    echo "Installing -> $au_dir"
    for b in "$here/AU/"*.component; do install_bundle "$b" "$au_dir"; done
fi
echo
# -- 3. quarantine ---------------------------------------------------------
echo "Clearing the quarantine flag on what was just installed"
for name in "${shipping[@]}"; do
    [ -e "$vst3_dir/$name.vst3" ] && xattr -dr com.apple.quarantine "$vst3_dir/$name.vst3" 2>/dev/null || true
    [ -e "$au_dir/$name.component" ] && xattr -dr com.apple.quarantine "$au_dir/$name.component" 2>/dev/null || true
done

echo
echo "Done. Rescan plugins in your DAW."
echo
echo "Standalone apps were not installed; they are in Standalone/ if you want them."
