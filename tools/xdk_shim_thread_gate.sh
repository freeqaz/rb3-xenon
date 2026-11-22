#!/usr/bin/env bash
# xdk_shim_thread_gate.sh -- gate the thread and handle shims in native/src/xdk_shims.cpp
# under AddressSanitizer. (Lane threadshim, 2026-11-22.)
#
# WHY: CreateThread's start routine wrote its exit code into the handle entry, which a
# CloseHandle made while the thread ran had already freed (docs/decomp/
# MEMCARD_RB3APP_MEMCARD_XBOX_2026-11-22.md, "Pre-existing CreateThread shim hazard"). The
# same table let a CloseHandle free an event under a thread waiting on it, joined one pthread
# from several waiters, and ignored timeouts on thread and mutex waits. None of that shows in
# native_build_gate.sh or native_health.sh: the programs behave until the freed memory is
# reused. Each case in native/src/xdk_shim_thread_gate.cpp is one such sequence.
#
# USAGE
#   tools/xdk_shim_thread_gate.sh [--src PATH] [WORKTREE]
#     --src PATH  compile this xdk_shims.cpp instead of the worktree's (to run the gate
#                 against an older shim, e.g. `git show memcard:native/src/xdk_shims.cpp`)
#
# Compiles xdk_shims.cpp and the gate driver with clang++ -fsanitize=address -O0 -g into
# ~/tmp/xdk_shim_thread_gate/, then runs every case in its own process with a 10 s timeout.
#
# CONTROLS
#   control-join  the ordinary lifecycle; must PASS. A FAIL means the harness is broken.
#   control-asan  a deliberate heap use-after-free; ASan MUST report it. If it does not, the
#                 instrument cannot see the bug class and the run is INVALID.
#
# EXIT  0 every case passed and both controls behaved; 1 a case failed; 2 could not build,
#       or a control misbehaved (the result says nothing about the shims).
# The last line is
#   XDK_SHIM_THREAD_GATE_RESULT verdict=<PASS|FAIL|INVALID|BUILD_FAILED> cases=N pass=N fail=N controls=<ok|broken> rc=N
set -u

SRC=""
WT=""
while [ $# -gt 0 ]; do
    case "$1" in
        --src) SRC="$2"; shift 2 ;;
        -h|--help) sed -n '2,30p' "$0"; exit 0 ;;
        *) WT="$1"; shift ;;
    esac
done
WT="${WT:-$(cd "$(dirname "$0")/.." && pwd)}"
SRC="${SRC:-$WT/native/src/xdk_shims.cpp}"
OUT="$HOME/tmp/xdk_shim_thread_gate"
mkdir -p "$OUT"
BIN="$OUT/gate_$$"

CXXFLAGS=(-std=c++17 -w -O0 -g -fno-omit-frame-pointer -fsanitize=address
          -fms-extensions -fms-compatibility -fms-compatibility-version=19.29
          -fdelayed-template-parsing -DHX_NATIVE=1
          # native/CMakeLists.txt MS_COMPAT_FLAGS/ATOMIC_COMPAT_FLAGS: -fms-compatibility hides these from <atomic>
          -D__GCC_ATOMIC_TEST_AND_SET_TRUEVAL=1 -D__GCC_ATOMIC_BOOL_LOCK_FREE=2 -D__GCC_ATOMIC_CHAR_LOCK_FREE=2
          -D__GCC_ATOMIC_WCHAR_T_LOCK_FREE=2 -D__GCC_ATOMIC_CHAR16_T_LOCK_FREE=2
          -D__GCC_ATOMIC_CHAR32_T_LOCK_FREE=2 -D__GCC_ATOMIC_SHORT_LOCK_FREE=2
          -D__GCC_ATOMIC_INT_LOCK_FREE=2 -D__GCC_ATOMIC_LONG_LOCK_FREE=2
          -D__GCC_ATOMIC_LLONG_LOCK_FREE=2 -D__GCC_ATOMIC_POINTER_LOCK_FREE=2
          -isystem "$WT/native/include" -I"$WT/native/src" -I"$WT/src" -I"$WT/src/system")

echo "shim source: $SRC"
if ! clang++ "${CXXFLAGS[@]}" "$SRC" "$WT/native/src/xdk_shim_thread_gate.cpp" \
        -lpthread -o "$BIN" > "$OUT/build.log" 2>&1; then
    tail -20 "$OUT/build.log"
    echo "XDK_SHIM_THREAD_GATE_RESULT verdict=BUILD_FAILED cases=0 pass=0 fail=0 controls=broken rc=2"
    exit 2
fi

ulimit -d unlimited 2>/dev/null
export ASAN_OPTIONS="halt_on_error=1:detect_leaks=0:abort_on_error=0"

total=0; pass=0; fail=0; controls=ok
for c in $("$BIN" --list); do
    log="$OUT/$c.log"
    timeout 10 "$BIN" "$c" > "$log" 2>&1
    rc=$?
    asan=$(command grep -a -c 'ERROR: AddressSanitizer' "$log")
    if [ "$c" = control-asan ]; then
        if [ "$asan" -ge 1 ]; then
            echo "CONTROL $c ok (ASan reported the deliberate use-after-free)"
        else
            echo "CONTROL $c BROKEN (rc=$rc, no ASan report)"; controls=broken
        fi
        continue
    fi
    total=$((total + 1))
    why=""
    if [ $rc -eq 124 ]; then why="hung (10 s timeout)"
    elif [ "$asan" -ge 1 ]; then
        why="ASan: $(command grep -a -m1 'ERROR: AddressSanitizer' "$log" | sed 's/.*AddressSanitizer: //; s/ on address.*//') at $(command grep -a -m1 -E '#[0-9]+ .* in .*xdk_shims\.cpp' "$log" | sed 's/.* in //; s/ .*\//:/')"
    elif [ $rc -ne 0 ]; then why="$(command grep -a -m1 'FAIL' "$log" || echo "rc=$rc")"
    fi
    if [ -z "$why" ]; then
        pass=$((pass + 1)); echo "PASS $c"
    else
        fail=$((fail + 1)); echo "FAIL $c -- $why"
        [ "$c" = control-join ] && controls=broken
    fi
done
rm -f "$BIN"

if [ "$controls" != ok ]; then verdict=INVALID; rc=2
elif [ $fail -gt 0 ]; then verdict=FAIL; rc=1
else verdict=PASS; rc=0
fi
echo "XDK_SHIM_THREAD_GATE_RESULT verdict=$verdict cases=$total pass=$pass fail=$fail controls=$controls rc=$rc"
exit $rc
