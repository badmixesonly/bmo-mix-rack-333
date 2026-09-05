#!/usr/bin/env bash
# Build a Release tree and stage a versioned tester package for this machine.
#
#   scripts/package.sh            -> packages/v<version>/BMO-<version>-macOS-<arch>.zip
#   scripts/package.sh --universal   arm64 + x86_64 (slower)
#
# The Windows package comes from CI (the "BMO-Windows" artifact of the build
# workflow); drop it into the same packages/v<version>/ folder.
#
# packages/ is gitignored: these are for testers, not the repository.

set -euo pipefail
cd "$(dirname "$0")/.."

version=$(sed -n 's/^project(BMOMixRack VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)
major=${version%%.*}
arch=arm64
args=(-DCMAKE_OSX_ARCHITECTURES=arm64)

if [[ ${1:-} == --universal ]]; then
    arch=universal
    args=(-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64")
fi

build=build-release
out=packages/v$major
stage=$out/BMO-$version-macOS-$arch

cmake -B "$build" -DCMAKE_BUILD_TYPE=Release "${args[@]}"
cmake --build "$build" --parallel
ctest --test-dir "$build" --output-on-failure

mkdir -p "$out"
tools/packager/package.sh "$build" "$stage"

( cd "$out" && rm -f "$(basename "$stage").zip" && zip -qry "$(basename "$stage").zip" "$(basename "$stage")" )
echo "wrote $stage.zip"
