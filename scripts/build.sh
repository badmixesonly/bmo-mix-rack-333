#!/usr/bin/env bash
# Developer build: configure (once), build everything, run the tests.
#
#   scripts/build.sh            build + test (Debug, arm64, installs plugins
#                               into ~/Library/Audio/Plug-Ins on macOS)
#   scripts/build.sh --snapshots  also render every panel into snapshots/
#
# Live holds plugin bundles open while it runs, so the copy-after-build step
# can fail quietly and leave you auditioning a stale binary. Quit Live first.

set -euo pipefail
cd "$(dirname "$0")/.."

if [[ ! -f build/CMakeCache.txt ]]; then
    cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES=arm64
fi

cmake --build build --parallel
ctest --test-dir build --output-on-failure

if [[ ${1:-} == --snapshots ]]; then
    mkdir -p snapshots
    ./build/tools/snapshot eq   snapshots/eq.png
    ./build/tools/snapshot sat  snapshots/sat.png
    ./build/tools/snapshot util snapshots/util.png
    ./build/tools/snapshot rack snapshots/rack.png chain=util,eq,sat
fi
