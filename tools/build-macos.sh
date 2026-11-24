#!/usr/bin/env bash
# Build the Xbox 360 native app and its asset-free Metal smoke test.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
dawn_dir="${DAWN_DIR:-$root/../dawn-prebuilt/lib/cmake/Dawn}"
engine_dir="${MILO_ENGINE_PATH:-$root/../milo-native-engine-rb3-xenon}"
build_dir="${BUILD_DIR:-$root/native/build-macos}"

if [[ ! -d "$engine_dir" && -z "${MILO_ENGINE_PATH:-}" ]]; then
    engine_dir="$root/../milo-native-engine"
fi
if [[ -f "$dawn_dir/lib/cmake/Dawn/DawnConfig.cmake" ]]; then
    dawn_dir="$dawn_dir/lib/cmake/Dawn"
fi
if [[ ! -f "$engine_dir/CMakeLists.txt" || ! -f "$dawn_dir/DawnConfig.cmake" ]]; then
    printf 'Set MILO_ENGINE_PATH to the pinned engine checkout and DAWN_DIR to the Metal Dawn install.\n' >&2
    exit 2
fi

cmake -S "$root/native" -B "$build_dir" -G Ninja \
    -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
    -DMILO_ENGINE_PATH="$engine_dir" -DDawn_DIR="$dawn_dir"
cmake --build "$build_dir" --target rb3-app rb3-frame --parallel "${JOBS:-$(sysctl -n hw.logicalcpu)}"
