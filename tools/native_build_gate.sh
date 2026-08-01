#!/usr/bin/env bash
# Pre-landing gate: does the NATIVE build still compile+link — ALL of it?
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
# ---------------------------------------------------------------------------
# WHAT WAS WRONG WITH THIS GATE (fixed 2026-08-01, lane X2/GATE)
# ---------------------------------------------------------------------------
# The gate could not fail in the way that mattered. Three defects, all measured:
#
#  1. It counted `find build -executable -name 'rb3-*' | wc -l` -- binaries that
#     EXIST, not targets THIS RUN vouches for. X1 measured a tree where 8
#     binaries dated `Jul 31 19:09` were counted as "linked" although nothing
#     had linked for days.
#  2. That count was PRINTED AND NEVER ASSERTED. The only FAIL conditions were
#     `build_rc != 0` and `errs != 0`, so a tree that compiled NOTHING AT ALL
#     still reported "15 linked target(s)" and PASS.
#  3. It ran plain `cmake --build`, i.e. ninja's default `-k1`, stopping at the
#     first failing TU and concealing every independent breakage behind it. It
#     took X1 four fix-and-rerun cycles to find four separate defects.
#
# (1)+(2) were a deliberate trade recorded in the old comment: a log-derived
# count read 0 on incremental builds and "looked like catastrophic failure", so
# a false NEGATIVE was swapped for a false POSITIVE -- the wrong direction for a
# gate, and precisely why a 199-file matching wave killed the native build
# unnoticed.
#
# HOW THE FIX WORKS -- four sets, compared:
#
#   MANIFEST    KNOWN_TARGETS below. A committed floor. Deleting a target's
#               declaration cannot lower the bar silently; it FAILs until the
#               manifest is edited in the same commit.
#   DECLARED    parsed live from native/CMakeLists.txt (`rb3_add_executable(` /
#               `add_executable(`), so a NEWLY ADDED target is held to the same
#               standard immediately, with no manifest edit needed.
#   EXPECTED    = MANIFEST u DECLARED. Monotone by construction: additions raise
#               the bar at once, deletions never lower it.
#   ACTUAL      per target, interrogated from the build system itself, NOT from
#               the filesystem:
#                 `ninja -t targets all` -> which targets are CONFIGURED, i.e.
#                                           have a PRODUCING EDGE (instant)
#                 real `ninja <t>`       -> is it UP-TO-DATE? ("no work to do",
#                                           measured 0.034 s on a no-op)
#               (NOT `ninja -t query` and NOT `ninja -n` -- both are vacuous
#                here; see the two warnings below before changing either probe.)
#               Freshness is thus a POSITIVE STATEMENT BY NINJA that the binary
#               is newer than every input in its dependency graph -- which a
#               stale Jul-31 binary on a changed tree cannot satisfy -- while an
#               incremental no-op on a healthy tree still passes, so fixing
#               defect (1) does NOT reintroduce the false negative the old
#               comment was trading against. mtime-vs-run-marker additionally
#               labels RELINKED vs UP-TO-DATE, so the operator can see which.
#
#               !! `ninja -n` (DRY run) is NOT usable here, in EITHER of its two
#               tempting forms, and both were measured on 2026-08-01:
#
#               (a) as a whole-build staleness count, it makes the gate
#                   INCAPABLE OF PASSING -- the exact mirror of the "cannot
#                   fail" defect this gate exists to prevent. This actually
#                   landed on main as `6c2187fe` and was reverted in `636f59b3`.
#                   native/CMakeLists.txt installs file(GLOB ... CONFIGURE_DEPENDS
#                   ...) on 8 directories, adding a glob-recheck edge that A DRY
#                   RUN CANNOT EXECUTE, so ninja always plans the CMake re-run:
#                       $ ninja -n              -> [0/2] Re-checking globbed dirs
#                                                  [1/2] Re-running CMake...
#                       $ ninja                 -> ninja: no work to do.
#                       $ ninja -n              -> STILL 2, after two clean no-ops
#                   `stale` was therefore a CONSTANT 2 on every healthy tree.
#
#               (b) as a per-target probe, it is vacuous in the other direction:
#                   it returns rc=0 and "work to do" for EVERY target, including
#                   one that DOES NOT EXIST AT ALL.
#
#               The real (non-dry) probe costs 0.034 s on a no-op, so there is
#               nothing to buy by faking it.
#
# Every EXPECTED target must end OK, or be SKIPPED with a reason that is
# independently verified against the environment (see conditional_reason).
# Anything else is a FAIL that NAMES THE TARGET.
#
# The build runs with `-k 0` and the report gives the DISTINCT error set plus
# every failed ninja edge and the targets they belong to, so independent
# breakages are all visible in one run.
#
# USAGE
#     tools/native_build_gate.sh [project_dir] [--strict]
#
#     --strict / NATIVE_GATE_STRICT=1
#         SKIPPED targets FAIL too. Use on a machine that has the full deps
#         tree (i.e. repo main), where a skip means the environment moved.
#     NATIVE_GATE_ONLY="rb3-dta rb3-ark"
#         Build+assert only these targets. The verdict can then NEVER be a bare
#         "PASS" -- it is "PASS (PARTIAL: n of N)" and lists what was omitted --
#         so a subset run cannot be mistaken for full coverage.
#
# Exit 0 = native still builds. Exit 1 = it does not; diagnostics are printed
# and the log path is reported. Exit 2 = the gate could not run at all.

set -uo pipefail

# --------------------------------------------------------------- MANIFEST ---
# The floor. If you add or remove a target in native/CMakeLists.txt, update
# this list IN THE SAME COMMIT -- that is the point: the change becomes a
# reviewable line in the diff instead of a silently lowered bar.
KNOWN_TARGETS=(
    rb3-dta   rb3-song    rb3-midi    rb3-gem     rb3-hit
    rb3-score rb3-score2  rb3-score3  rb3-score4
    rb3-vocal rb3-vocal2  rb3-harmony rb3-crowd
    rb3-save  rb3-ark
    rb3-frame rb3-milo    rb3-render
)

STRICT="${NATIVE_GATE_STRICT:-0}"
DIR=""
for arg in "$@"; do
    case "$arg" in
        --strict) STRICT=1 ;;
        -*)       echo "native_build_gate: unknown option $arg" >&2; exit 2 ;;
        *)        DIR="$arg" ;;
    esac
done
[ -n "$DIR" ] || DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# Normalise: a relative arg such as `.` otherwise yields a log named
# `native_gate_..log` and an unreadable "tree:" line.
DIR="$(cd "$DIR" 2>/dev/null && pwd)" || { echo "native_build_gate: no such dir" >&2; exit 2; }

LOG="${TMPDIR:-$HOME/tmp}/native_gate_$(basename "$DIR").log"
mkdir -p "$(dirname "$LOG")"

if [ ! -d "$DIR/native" ]; then
    echo "native_build_gate: no native/ under $DIR" >&2
    exit 2
fi
cd "$DIR/native" || exit 2
BUILD=build
CML=CMakeLists.txt

# `grep` is a shell FUNCTION shimming to `ugrep -I` in some interactive shells
# here; it silently matches NOTHING in a file it thinks is binary (false
# negatives only). Scripts get the real grep, but pin it anyway and pass -a so a
# stray NUL byte in a compiler diagnostic cannot make an error line invisible.
G() { command grep -a "$@"; }

cache_get() {  # $1 = cache variable name -> its value, or empty
    [ -f "$BUILD/CMakeCache.txt" ] || return 0
    sed -n "s/^$1:[^=]*=//p" "$BUILD/CMakeCache.txt" | head -1
}

# --- Is a target's absence LEGITIMATE? --------------------------------------
# Echo a reason iff the target is conditional AND the condition is independently
# verified to hold RIGHT NOW. Silence => the absence is a defect.
#
# A target that is merely "not there" is NEVER accepted: the reason must be a
# checkable fact about this machine, not a fact about the source -- otherwise
# this function reopens the exact hole the gate exists to close.
conditional_reason() {
    case "$1" in
    rb3-frame)
        # Guarded by RB3X_BUILD_ENGINE, which native/CMakeLists.txt:858-866
        # AUTO-DISABLES when Dawn is absent. Dawn_DIR and MILO_ENGINE_PATH both
        # default RELATIVE to the source tree, so they resolve in the repo but
        # NOT in a worktree -- where rb3-frame silently vanishes at rc=0. That
        # is a real, reproducible false positive for the old gate.
        #
        # !! Do NOT read RB3X_BUILD_ENGINE from CMakeCache.txt to answer this.
        # The auto-disable is a plain `set(RB3X_BUILD_ENGINE OFF)` that shadows
        # the cache entry, so the CACHE STILL READS `ON` ON A TREE WHERE THE
        # ENGINE IS OFF. Verified 2026-08-01 in a Dawn-less worktree:
        # `RB3X_BUILD_ENGINE:BOOL=ON` while rb3-frame was not configured. That
        # instrument is vacuous. The file-existence test below is the same
        # condition CMake itself branches on.
        local dawn; dawn="$(cache_get Dawn_DIR)"
        if [ -n "$dawn" ] && [ ! -f "$dawn/DawnConfig.cmake" ]; then
            echo "RB3X_BUILD_ENGINE auto-disabled -- no DawnConfig.cmake at $dawn"
        fi
        ;;
    rb3-milo)
        # X2. Needs no Dawn and no GPU -- its ONLY engine dependency is the
        # header-only platform/NativeSettings.h that rndobj/Cam.cpp includes
        # under #ifdef HX_NATIVE. So the condition is the ENGINE CHECKOUT, not
        # Dawn, and it is tested the same way CMake tests it: file existence.
        local eng; eng="$(cache_get MILO_ENGINE_PATH)"
        if [ -n "$eng" ] && [ ! -f "$eng/src/platform/NativeSettings.h" ]; then
            echo "no milo-native-engine checkout at $eng (needs platform/NativeSettings.h)"
        fi
        ;;
    rb3-render)
        # X3. Needs BOTH -- it links libmilo-engine.a (so Dawn) and it lives
        # inside the rb3-milo block (so the checkout). Report whichever is
        # actually absent; if neither is, silence, and the target must be there.
        local eng dawn; eng="$(cache_get MILO_ENGINE_PATH)"; dawn="$(cache_get Dawn_DIR)"
        if [ -n "$eng" ] && [ ! -f "$eng/src/platform/NativeSettings.h" ]; then
            echo "no milo-native-engine checkout at $eng"
        elif [ -n "$dawn" ] && [ ! -f "$dawn/DawnConfig.cmake" ]; then
            echo "RB3X_BUILD_ENGINE auto-disabled -- no DawnConfig.cmake at $dawn"
        fi
        ;;
    esac
}

# ------------------------------------------------------------- configure ----
MARKER="$BUILD/.native_gate_run_marker"
cmake -S . -B "$BUILD" -G Ninja \
      -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ > "$LOG" 2>&1
cfg_rc=$?
if [ $cfg_rc -ne 0 ]; then
    echo "NATIVE GATE: FAIL (cmake configure rc=$cfg_rc)"
    tail -20 "$LOG"
    echo "log: $LOG"
    exit 1
fi

NINJA="$(cache_get CMAKE_MAKE_PROGRAM)"; [ -n "$NINJA" ] || NINJA=ninja

# --------------------------------------------------- CONFIGURED target set --
# Enumerate the nodes that have a PRODUCING EDGE in the ninja graph.
# `ninja -t targets all` prints "<output>: <rule>" for exactly those.
#
# !! `ninja -t query <t>` is NOT usable for this -- and the TODO comment left in
# `636f59b3` explicitly recommended it, so this is a correction, not a nitpick.
# It was rejected only AFTER it silently passed a negative control
# (2026-08-01): it returns rc=0 for ANY PATH
# THAT MERELY EXISTS ON DISK, producing "<name>:" with an empty `outputs:` and
# no `input:` line for a target that is not in the graph at all. So it is a
# file-existence test wearing a graph-query costume -- and a stale binary then
# also satisfies the follow-up "no work to do" probe, because a node with no
# inputs is trivially up to date. Both probes fall to the same cause, and they
# fall EXACTLY in the scenario this gate exists for: a dropped target whose old
# binary is still on disk. Tested `-t query` against an absent target with no
# file (it correctly errors) and wrongly concluded it worked; the case that
# matters is an absent target WITH a file.
configured="$("$NINJA" -C "$BUILD" -t targets all 2>/dev/null | sed -n 's/^\([^:]*\): .*/\1/p')"
if [ -z "$configured" ]; then
    # Anti-vacuity self-check: a probe that returns nothing would mark every
    # target absent (or, in an earlier draft, every target fine). Refuse to
    # report rather than emit a verdict the instrument cannot support.
    echo "NATIVE GATE: FAIL (the gate could not enumerate ANY target from the ninja graph --"
    echo "  its own probe is broken, so no verdict is reportable)"
    echo "log: $LOG"
    exit 2
fi
# Exact-line membership, done WITHOUT a pipe on purpose.
# `printf '%s\n' "$configured" | grep -qxF "$1"` is WRONG under `set -o
# pipefail`, and wrong in a data-dependent way that cost a negative control to
# find: grep -q exits at the FIRST match, printf then dies of SIGPIPE, and
# pipefail promotes the writer's death to the pipeline's status -- so a MATCH
# reports FALSE. It only misbehaves when the match is early enough to make grep
# exit before printf finishes, so `rb3-save` (late in a 3102-line list) passed
# while `rb3-dta` (line 171) failed. Same family as `prog | tail; echo $?`.
is_configured() {
    case $'\n'"$configured"$'\n' in
        *$'\n'"$1"$'\n'*) return 0 ;;
        *)                return 1 ;;
    esac
}

# --------------------------------------------------- EXPECTED target set ----
# DECLARED: parse the source of truth. Comments are stripped first, so a
# commented-out declaration does not count.
declared=()
while IFS= read -r t; do [ -n "$t" ] && declared+=("$t"); done < <(
    awk '{ line = $0; sub(/#.*/, "", line)
           if (match(line, /(^|[^A-Za-z0-9_])(rb3_)?add_executable[ \t]*\([ \t]*[A-Za-z0-9_.+-]+/)) {
               s = substr(line, RSTART, RLENGTH); sub(/.*\([ \t]*/, "", s); print s } }' "$CML" | sort -u)

is_in() { local n="$1"; shift; local x; for x in "$@"; do [ "$x" = "$n" ] && return 0; done; return 1; }

# EXPECTED = MANIFEST u DECLARED (manifest order first, for stable output).
expected=("${KNOWN_TARGETS[@]}")
new_targets=()
for t in ${declared[@]+"${declared[@]}"}; do
    if ! is_in "$t" "${KNOWN_TARGETS[@]}"; then expected+=("$t"); new_targets+=("$t"); fi
done
dropped=()
for t in "${KNOWN_TARGETS[@]}"; do
    is_in "$t" ${declared[@]+"${declared[@]}"} || dropped+=("$t")
done

# Optional subset. Can only ever produce a self-labelled PARTIAL verdict.
partial=0; omitted=()
if [ -n "${NATIVE_GATE_ONLY:-}" ]; then
    read -r -a only <<< "$NATIVE_GATE_ONLY"
    for t in "${expected[@]}"; do is_in "$t" "${only[@]}" || omitted+=("$t"); done
    expected=("${only[@]}")
    partial=1
fi

# ------------------------------------------------------------- build -------
# -k 0: keep going after failures, so INDEPENDENT breakages are all reported in
# one run instead of the first one masking the rest.
: > "$MARKER"
if [ $partial -eq 1 ]; then
    # Only ask ninja to build names it actually knows: a requested-but-absent
    # target must be adjudicated below (and FAIL), not abort the build here.
    buildable=()
    for t in "${expected[@]}"; do
        is_configured "$t" && buildable+=("$t")
    done
    if [ ${#buildable[@]} -gt 0 ]; then
        cmake --build "$BUILD" --target "${buildable[@]}" -- -k 0 >> "$LOG" 2>&1
        build_rc=$?
    else
        build_rc=0
    fi
else
    cmake --build "$BUILD" -- -k 0 >> "$LOG" 2>&1
    build_rc=$?
fi

errs=$(G -c "error:" "$LOG");            errs=${errs:-0}
warns=$(G -c "warning:" "$LOG");         warns=${warns:-0}
failed_edges=$(G -c "^FAILED: " "$LOG"); failed_edges=${failed_edges:-0}
distinct_of() { G "error:" "$LOG" | sed 's#^.*/\([^/]*:[0-9]*:[0-9]*: error:\)#\1#' | sort -u; }
distinct_errs=$(distinct_of | wc -l)

# Attribute failed ninja edges to targets: CMakeFiles/<target>.dir/... for
# compiles, the bare output name for links.
failed_targets=$(G "^FAILED: " "$LOG" \
    | sed -n 's#.*CMakeFiles/\([^/]*\)\.dir/.*#\1#p' | sort -u)

# ------------------------------------------------- per-target adjudication --
ok=(); relinked=(); skipped_lines=(); fail_lines=()
for t in "${expected[@]}"; do
    if ! is_configured "$t"; then
        # NOT CONFIGURED. Legitimate only with a verified environmental reason.
        # NB this is a graph-edge test, NOT a file test: a leftover binary from
        # an earlier build does NOT make a dropped target look present.
        reason="$(conditional_reason "$t")"
        if [ -n "$reason" ]; then
            skipped_lines+=("  SKIPPED   $t -- $reason")
        elif is_in "$t" ${dropped[@]+"${dropped[@]}"}; then
            fail_lines+=("  DROPPED   $t -- declaration REMOVED from native/$CML (held by the manifest floor). If deliberate, delete it from KNOWN_TARGETS in this script, in the same commit.")
        else
            fail_lines+=("  MISSING   $t -- declared in native/$CML but NOT CONFIGURED, and no verified conditional reason")
        fi
        continue
    fi
    if [ ! -x "$BUILD/$t" ]; then
        why=""
        is_in "$t" $failed_targets && why=" -- failed edges are attributed to it in the log"
        fail_lines+=("  NOBINARY  $t -- configured, but no executable was produced$why")
        continue
    fi
    # Configured + a binary exists. Is it THIS run's? Ask ninja, not the mtime:
    # "no work to do" is a positive statement that the binary is newer than
    # every input in its graph. A stale binary on a changed tree cannot get it.
    probe="$("$NINJA" -C "$BUILD" "$t" 2>&1)"; prc=$?
    # Substring test in-shell, NOT `printf | grep -q` -- see is_configured().
    if [ $prc -ne 0 ] || [[ "$probe" != *"no work to do"* ]]; then
        fail_lines+=("  STALE     $t -- a binary exists but ninja does not consider it up to date (rc=$prc). It is NOT attributable to this run.")
        continue
    fi
    if [ "$BUILD/$t" -nt "$MARKER" ]; then relinked+=("$t"); else ok+=("$t"); fi
done

# ------------------------------------------------------------- report ------
echo "=== NATIVE BUILD GATE ==="
echo "tree:      $DIR"
echo "manifest:  ${#KNOWN_TARGETS[@]} known target(s)"
echo "declared:  ${#declared[@]} parsed from native/$CML"
echo "expected:  ${#expected[@]} (manifest u declared)$( [ $partial -eq 1 ] && echo "  [SUBSET REQUESTED]")"
echo "build:     rc=$build_rc, $errs error line(s) / $distinct_errs distinct, $failed_edges failed edge(s), $warns warning(s)"
[ ${#new_targets[@]} -gt 0 ] && echo "note:      target(s) declared but absent from this script's manifest, held to the same standard anyway: ${new_targets[*]} (add them to KNOWN_TARGETS)"
echo
echo "targets:"
for t in ${relinked[@]+"${relinked[@]}"}; do echo "  OK        $t -- relinked this run"; done
for t in ${ok[@]+"${ok[@]}"};             do echo "  OK        $t -- up to date (ninja: no work to do)"; done
for l in ${skipped_lines[@]+"${skipped_lines[@]}"}; do echo "$l"; done
for l in ${fail_lines[@]+"${fail_lines[@]}"};       do echo "$l"; done
[ $partial -eq 1 ] && [ ${#omitted[@]} -gt 0 ] && echo "  OMITTED   ${omitted[*]}"
echo

n_ok=$(( ${#ok[@]} + ${#relinked[@]} ))
n_skip=${#skipped_lines[@]}
n_bad=${#fail_lines[@]}
if [ $STRICT -eq 1 ] && [ $n_skip -gt 0 ]; then
    for l in "${skipped_lines[@]}"; do echo "  STRICT-FAIL${l#  SKIPPED}"; done
    n_bad=$(( n_bad + n_skip ))
fi

if [ $build_rc -ne 0 ] || [ "$errs" -ne 0 ] || [ $n_bad -ne 0 ]; then
    verdict="NATIVE GATE: FAIL  (build rc=$build_rc, $errs error line(s), $n_bad target defect(s), $n_ok/${#expected[@]} target(s) good"
    [ $n_skip -gt 0 ] && verdict="$verdict, $n_skip skipped"
    echo "$verdict)"
    if [ "$errs" -ne 0 ]; then
        echo "--- distinct diagnostics ($distinct_errs) ---"
        distinct_of | head -20
        echo "--- $failed_edges failed edge(s) across target(s): $(echo $failed_targets | tr '\n' ' ')---"
    fi
    echo "log: $LOG"
    exit 1
fi

# The warning policy is -Wno-everything + explicit -Werror= opt-ins, so a
# non-zero warning count here means something got past an opt-in that should
# have been an error, or a new -W was added without -Werror=. Report, don't fail.
if [ $partial -eq 1 ]; then
    echo "NATIVE GATE: PASS (PARTIAL: $n_ok of ${#KNOWN_TARGETS[@]} known target(s) requested) -- NOT full coverage"
else
    echo "NATIVE GATE: PASS  (rc=0, $errs errors, $warns warnings, $n_ok/${#expected[@]} target(s) verified$( [ $n_skip -gt 0 ] && echo ", $n_skip skipped"))"
fi
[ "$warns" -ne 0 ] && echo "  note: $warns warning(s) -- policy expects 0; check the opt-in list in native/CMakeLists.txt"
echo "log: $LOG"
exit 0
