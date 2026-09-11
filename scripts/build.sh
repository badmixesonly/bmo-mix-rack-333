#!/usr/bin/env bash
# Developer build: configure (once), build everything, run the tests. The same
# shape as BMO Mix Rack's scripts/build.sh, and for the same reasons.
#
#   scripts/build.sh            DSP only: Release build + ctest, no JUCE
#   scripts/build.sh --plugin   the plugin too, in build-plugin/: VST3 and
#                               Standalone, the panel and host checks, and
#                               panel snapshots in snapshots/
#   scripts/build.sh --corpus   the DSP build, then score the synthetic corpus
#
# Release rather than the rack's Debug: the tests here push ten million
# samples through the detector and render whole corpora, and an MSVC Debug
# build of that takes minutes rather than seconds. A Release build is also
# never copied into the system plugin folder -- install it by hand.
#
# The plugin needs JUCE (the rack's own submodule, libs/bmo-mix-rack/libs/JUCE)
# and the licensed fonts (.bmo-fontdir); CMake says how to get either.

set -euo pipefail
cd "$(dirname "$0")/.."

config=Release
mode=${1:-}

if [[ $mode == --plugin ]]; then
    dir=build-plugin
    args=(-DCMAKE_BUILD_TYPE="$config" -DBMO_DSP_ONLY=OFF)
else
    dir=build
    args=(-DCMAKE_BUILD_TYPE="$config")
fi

if [[ ! -f $dir/CMakeCache.txt ]]; then
    [[ $OSTYPE == darwin* ]] && args+=(-DCMAKE_OSX_ARCHITECTURES=arm64)
    cmake -B "$dir" "${args[@]}"
fi

# Multi-config generators (Visual Studio, Xcode) want the configuration named
# on every command; the cache says which kind this build directory is.
if grep -q '^CMAKE_CONFIGURATION_TYPES:' "$dir/CMakeCache.txt"; then
    build_args=(--config "$config")
    bin=$config/
else
    build_args=()
    bin=
fi

cmake --build "$dir" --parallel "${build_args[@]}"
ctest --test-dir "$dir" -C "$config" --output-on-failure

if [[ $mode == --plugin ]]; then
    snap="$dir/tools/${bin}bmo-tune-snapshot"
    mkdir -p snapshots
    for look in dark light; do
        "$snap" "snapshots/hybrid-$look.png"  appearance=$look engine=Hybrid key=A scale=Minor retune=14 vibrato=45 glide=30
        "$snap" "snapshots/classic-$look.png" appearance=$look engine=Classic key=Bb scale=Major note_d=0
    done
    echo "VST3: $dir/products/tune/BmoTuneRT_artefacts/${bin}VST3/BMO Tune RT.vst3"
fi

if [[ $mode == --corpus ]]; then
    scripts/score-corpus.sh
fi
