#!/usr/bin/env bash
# Pre-landing gate: does the NATIVE build still compile+link?
#
# WHY THIS EXISTS
# ---------------
# The X360 match build NEVER LINKS. It compiles TUs and diffs objects, so an
# entire class of defect is STRUCTURALLY INVISIBLE there: ODR violations,
# duplicate symbols, missing definitions, and any construct that is well-formed
# only under ILP32 / only with an Xbox-only overload in scope.
#
# The native build (native/, x86_64 clang) is the ONLY build in this project that
# links. It is therefore the only instrument that can see that class.
#
# This has now bitten twice. Most recently `b2958f2d` landed, into SHARED
# src/system/rndobj/Bitmap.h:
#
#     static void *operator new(unsigned int s) { return (MemAlloc)(s, 0); }
#
# Both halves are Xbox-only BY CONSTRUCTION: the 2-arg `MemAlloc(int,int)` is
# declared under `#ifndef HX_NATIVE` (utl/MemMgr.h:154-157), and `size_t ==
# unsigned int` only on ILP32. On x86_64 LP64 `size_t` is `unsigned long`, so the
# overload is ill-formed AND the callee does not exist. Result: native build
# rc=1, 15 error lines, ALL 15 TARGETS DEAD -- and main shipped it, because
# nothing in the X360 pipeline can fail on it.
#
# RULE: run this before landing ANY change that touches shared `src/`.
# A change confined to `native/` cannot affect the X360 build; a change to
# `src/` can affect BOTH, and needs this gate AND a whole-binary A/B.
#
# USAGE
#     tools/native_build_gate.sh [project_dir]      # default: repo root
#
# Exit 0 = native still builds. Exit 1 = it does not; the diagnostics are printed
# and the log path is reported.

set -uo pipefail

DIR="${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
LOG="${TMPDIR:-$HOME/tmp}/native_gate_$(basename "$DIR").log"
mkdir -p "$(dirname "$LOG")"

if [ ! -d "$DIR/native" ]; then
    echo "native_build_gate: no native/ under $DIR" >&2
    exit 2
fi

cd "$DIR/native" || exit 2

cmake -S . -B build -G Ninja \
      -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ > "$LOG" 2>&1
cfg_rc=$?
if [ $cfg_rc -ne 0 ]; then
    echo "NATIVE GATE: FAIL (cmake configure rc=$cfg_rc)"
    tail -20 "$LOG"
    echo "log: $LOG"
    exit 1
fi

cmake --build build >> "$LOG" 2>&1
build_rc=$?

errs=$(grep -c "error:" "$LOG")
warns=$(grep -c "warning:" "$LOG")
# Count the binaries that EXIST, not the "Linking" lines in the log: on an
# incremental build ninja relinks nothing and a log-derived count reads 0, which
# looks like catastrophic failure on a perfectly good tree. (Caught by this
# script's own positive control doing exactly that.)
targets=$(find build -maxdepth 1 -type f -executable -name 'rb3-*' 2>/dev/null | wc -l)

if [ $build_rc -ne 0 ] || [ "$errs" -ne 0 ]; then
    echo "NATIVE GATE: FAIL  (build rc=$build_rc, $errs error line(s))"
    echo "--- first diagnostics ---"
    grep "error:" "$LOG" | head -8
    echo "log: $LOG"
    exit 1
fi

# The warning policy is -Wno-everything + explicit -Werror= opt-ins, so a
# non-zero warning count here means something got past an opt-in that should
# have been an error, or a new -W was added without -Werror=. Report, don't fail.
echo "NATIVE GATE: PASS  (rc=0, $errs errors, $warns warnings, $targets linked target(s))"
[ "$warns" -ne 0 ] && echo "  note: $warns warning(s) -- policy expects 0; check the opt-in list in native/CMakeLists.txt"
echo "log: $LOG"
exit 0
