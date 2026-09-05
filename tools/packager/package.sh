#!/usr/bin/env bash
# Stage every product's plugin bundles into one folder for testers.
#
#   tools/packager/package.sh <build-dir> <out-dir>
#
# Locates the artefacts rather than hardcoding their paths, which have moved
# under renames before. Produces:
#
#   <out>/VST3/*.vst3          both platforms
#   <out>/AU/*.component       macOS
#   <out>/Standalone/*         .app on macOS, .exe on Windows
#   <out>/LICENSE.txt
#   <out>/README.txt           where to put things
#
# Distribution proper (installers, signing, notarisation) is out of scope
# for now; this is the "unzip and copy" package.

set -euo pipefail

build=${1:?build dir}
out=${2:?out dir}
root=$(cd "$(dirname "$0")/../.." && pwd)

rm -rf "$out"
mkdir -p "$out/VST3" "$out/Standalone"

products=("BMO EQ" "BMO Saturator" "BMO Util" "BMO Mix Rack")
found=0

for name in "${products[@]}"; do
    vst3=$(find "$build" -type d -name "$name.vst3" | head -1)
    if [ -n "$vst3" ]; then cp -R "$vst3" "$out/VST3/"; found=$((found + 1)); fi

    au=$(find "$build" -type d -name "$name.component" | head -1)
    if [ -n "$au" ]; then mkdir -p "$out/AU"; cp -R "$au" "$out/AU/"; fi

    app=$(find "$build" -type d -name "$name.app" | head -1)
    if [ -n "$app" ]; then cp -R "$app" "$out/Standalone/"; fi

    exe=$(find "$build" -type f -name "$name.exe" | head -1)
    if [ -n "$exe" ]; then cp "$exe" "$out/Standalone/"; fi
done

if [ "$found" -eq 0 ]; then
    echo "no VST3 bundles found under $build"
    find "$build" -name '*_artefacts' | head
    exit 1
fi

cp "$root/LICENSE" "$out/LICENSE.txt"

cat > "$out/README.txt" <<'TXT'
BMO (Bad Mixes Only) by LT3a -- tester build

Contents
  VST3/         BMO EQ, BMO Saturator, BMO Util, BMO Mix Rack
  AU/           the same, as Audio Units (macOS only)
  Standalone/   each product as an app, for a quick look without a DAW

Install
  macOS
    VST3  ->  ~/Library/Audio/Plug-Ins/VST3/
    AU    ->  ~/Library/Audio/Plug-Ins/Components/
    These are not signed or notarised yet. If macOS refuses to open
    them, remove the quarantine flag once:
      xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/BMO*.vst3
      xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/BMO*.component
    Then rescan plugins in your DAW.
  Windows
    VST3  ->  C:\Program Files\Common Files\VST3\

FrostyEQ users
  BMO EQ is FrostyEQ renamed; sessions that used FrostyEQ open with
  BMO EQ. Remove the old FrostyEQ.vst3 / FrostyEQ.component so the DAW
  does not show both.

Presets
  ~/Library/Audio/Presets/LT3 Audio/<product>/   (macOS)
  %APPDATA%\LT3 Audio\<product>\Presets\          (Windows)
  A theme file can be dropped at LT3 Audio/Themes/Default.json; see the
  repository README.
TXT

echo "staged into $out:"
find "$out" -maxdepth 2 | sort
