#!/usr/bin/env bash
# Developer build: configure (once), build everything, run the tests. The same
# shape as BMO Mix Rack's scripts/build.sh, and for the same reasons.
#
#   scripts/build.sh            Release build + ctest
#   scripts/build.sh --corpus   then score the synthetic corpus
#
# Release rather than the rack's Debug: the tests here push ten million
# samples through the detector and render whole corpora, and an MSVC Debug
# build of that takes minutes rather than seconds.

set -euo pipefail
cd "$(dirname "$0")/.."

config=Release

if [[ ! -f build/CMakeCache.txt ]]; then
    args=(-DCMAKE_BUILD_TYPE="$config")
    [[ $OSTYPE == darwin* ]] && args+=(-DCMAKE_OSX_ARCHITECTURES=arm64)
    cmake -B build "${args[@]}"
fi

# Multi-config generators (Visual Studio, Xcode) want the configuration named
# on every command; the cache says which kind this build directory is.
if grep -q '^CMAKE_CONFIGURATION_TYPES:' build/CMakeCache.txt; then
    build_args=(--config "$config")
else
    build_args=()
fi

cmake --build build --parallel "${build_args[@]}"
ctest --test-dir build -C "$config" --output-on-failure

if [[ ${1:-} == --corpus ]]; then
    scripts/score-corpus.sh
fi
