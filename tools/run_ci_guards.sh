#!/usr/bin/env bash
#
# run_ci_guards.sh -- ONE local entry point for the guards that, until now, ran
# ONLY in CI.
#
# WHY THIS EXISTS
# ---------------
# Lane GATE-DISC (2026-09-11) measured the coverage split and it was total: all
# eight CI guards have ZERO `build.ninja` hits, while the six in-graph check
# stamps (map injectivity, target-obj renaming, split currency, ruler agreement,
# obj-patch fixed point, alias-map content) run on every build and are therefore
# exercised in CI as well. So the in-graph half is checked twice and the CI half
# is checked in exactly one place -- a place no local agent ever reaches.
#
# That asymmetry is worst for `tools/icf_alias_finder.py --validate`, the only
# thing defending scripts/symbol_aliases.json. An alias is pure FORGIVENESS
# under the shipped name_check ruler: objdiff consults SymbolEquivalences and
# drops the charge, so an UNPROVEN alias lifts matched_code BY CONSTRUCTION and
# the `none` control cannot catch it (none ignores relocation names, so a
# fabricated alias reads +0 there by construction). It is the one data file in
# this repo where "the score went up" is not evidence of anything. A lane can
# edit it, measure a gain, and land -- and nothing on this machine objects.
#
# This runner does not change any guard. It gives them a caller.
#
# It also picks up the two "is this retired pass STILL redundant?" guards, which
# had NO caller anywhere -- not CI, not build.ninja. `check_objdiff_eh_prefix.py`
# landed with lane W4-E (10bf877d) and was hand-run only, which is the very
# defect this runner exists to fix; `check_objdiff_map_cache.py` is its twin for
# the alias-map cache key. Both assert a property of the PREBUILT objdiff-cli
# binary that ../objdiff/target/release/ resolves to -- a binary shared by
# symlink with ../rb3 and ../dc3-decomp, swapped by hand, pinned by nothing. A
# rollback past the relevant commit silently un-retires the thing they guard,
# and it fails as a WRONG NUMBER rather than an error.
#
# OPT-IN, DELIBERATELY
# --------------------
# NOT wired into the default build, `all_source`, `report.json` or `progress`.
# Two of these are slow (the native gate builds 18 targets; the symbols.txt
# fixpoint guard FORCES a split), and a guard that makes every build slower is a
# guard someone disables. `ninja guards` runs it; nothing depends on it.
#
# THREE OUTCOMES, NOT TWO
# -----------------------
# A guard that "could not run" is not a pass and not a failure, and collapsing
# it into either is how a gate goes quietly vacuous. So:
#
#   PASS     the guard ran and vouches
#   FAIL     the guard ran and objects            -> overall rc=1
#   BLOCKED  the guard could not run / does not vouch for full coverage
#                                                 -> overall rc=2 (if no FAILs)
#
# Every guard is run even after an earlier one fails; failures are summarised at
# the end. One guard's failure must never swallow another's.
#
# PER-GUARD rc SEMANTICS THAT ARE EASY TO GET BACKWARDS (all three are real)
# -------------------------------------------------------------------------
#  * `icf_alias_finder.py --validate` exits 2 [STALE_TREE] on an unsettled tree.
#    That is the gate WORKING (landed a2925eea): the target objs still carry
#    dtk's anonymous `fn_<addr>` symbols, so a verdict would describe the BUILD
#    STATE rather than the alias data. BLOCKED -- build first, then re-run.
#  * `--self-break` POLARITY IS INVERTED FROM INTUITION: exit 0 means THE PROOF
#    SUCCEEDED (the guard detected the planted violation) and exit 1 means the
#    guard is VACUOUS. Both test_icf_fold_safe.py and symbols_fixpoint_guard.py
#    use this convention, and both are must-pass CI steps. Do NOT "fix" it.
#  * `native_build_gate.sh` returns 0/1/2/3, where 3 = ran but does NOT vouch
#    for full coverage. rc=0 ALONE IS NOT ENOUGH: apply the 0-SKIP rule and
#    require `skipped=0` in its own NATIVE_GATE_RESULT line. That rule, not the
#    exit code, is what caught every false green so far (X21, MATCH-A).
#
# Two deliberate differences from the CI spellings, both in the safe direction:
#  * symbols_fixpoint_guard gets NO `--ci` here. `--ci` defaults it to plain
#    `ninja`; the local default is tools/ninja-locked, which serialises against
#    the other agents working on this box. `--ci` also waives the shared-main
#    refusal -- locally we WANT that refusal (it exits 2 -> BLOCKED), because
#    this guard forces a split and a shared main tree is the wrong place for it.
#  * the native gate gets NO NATIVE_GATE_ALLOW_INCOMPLETE=1. CI sets it because
#    milo-native-engine/Dawn cannot exist in that container. Here they can, so
#    an INCOMPLETE run is real information and is reported as BLOCKED.
#
# Usage:
#   tools/run_ci_guards.sh                 # all guards
#   tools/run_ci_guards.sh --fast          # skip the two SLOW ones
#   tools/run_ci_guards.sh --only icf_alias_validate,grep_binary
#   tools/run_ci_guards.sh --skip native_link_gate
#   tools/run_ci_guards.sh --list
#
#   CI_GUARDS_OBJDIFF=/path/to/objdiff-cli tools/run_ci_guards.sh \
#       --only objdiff_map_cache,objdiff_eh_prefix
#                                          # point the two property guards at a
#                                          # DIFFERENT objdiff binary. This is
#                                          # how their red leg is exercised, and
#                                          # how you check a fleet swap BEFORE
#                                          # deploying it: build the candidate
#                                          # somewhere else (never inside
#                                          # ../objdiff -- its target/release IS
#                                          # the deployed path) and point this
#                                          # at it. rc=1 means that candidate
#                                          # would silently un-retire a pass.
#
set -u

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO" || { echo "cannot cd to repo root $REPO" >&2; exit 2; }

VERSION="${VERSION:-45410914}"
LOGDIR="${CI_GUARDS_LOGDIR:-$HOME/tmp/ci-guards-$(date +%Y%m%d-%H%M%S)-$$}"
ONLY=""; SKIP=""; FAST=0; LIST=0

while [ $# -gt 0 ]; do
    case "$1" in
        --only)    ONLY="${2:-}"; shift 2 ;;
        --skip)    SKIP="${2:-}"; shift 2 ;;
        --version) VERSION="${2:-}"; shift 2 ;;
        --logdir)  LOGDIR="${2:-}"; shift 2 ;;
        --fast)    FAST=1; shift ;;
        --list)    LIST=1; shift ;;
        -h|--help) sed -n '2,80p' "$0"; exit 0 ;;
        *) echo "unknown option: $1 (try --help)" >&2; exit 2 ;;
    esac
done

# ---- registry ---------------------------------------------------------------
# mode: plain     rc 0 => PASS, else FAIL
#       refusable rc 0 => PASS, rc 2 => BLOCKED ("could not run"), else FAIL
#       selfbreak rc 0 => PASS *because the proof succeeded*, 1 => FAIL
#                 (guard is vacuous), 2 => BLOCKED.  See the polarity note above.
#       property  rc 0 => PASS, rc 5 => BLOCKED (VACUOUS: the probe proved
#                 nothing, which is NOT a pass), else FAIL.  The convention used
#                 by the "is this retired pass still redundant?" guards.
#       native    parse NATIVE_GATE_RESULT; require verdict=PASS AND skipped=0
declare -a G_ID G_MODE G_SPEED G_DESC G_CMD
add() { G_ID+=("$1"); G_MODE+=("$2"); G_SPEED+=("$3"); G_DESC+=("$4"); G_CMD+=("$5"); }

add grep_binary            plain     fast \
    "binary-scan methods can still FIND things in binaries (a blind grep yields only false negatives)" \
    "python3 tools/grep_binary_guard.py -v"
add source_category_self   plain     fast \
    "progress_category classifier still DISCRIMINATES (23 known answers, 9 negatives)" \
    "python3 tools/source_category.py selftest"
add source_category_audit  plain     fast \
    "every declared object's category agrees with its source PATH" \
    "python3 tools/source_category.py audit"
add scope_map_selftest     plain     fast \
    "scope-cache gate fails stale/dead fixtures AND passes the correct one (the control is half the evidence)" \
    "python3 tools/scope_map.py selftest"
add reloc_symidx           plain     fast \
    "COFF relocation SymbolTableIndex resolves through AUX records (only 20.99% resolve without the correction)" \
    "python3 tools/reloc_symidx_guard.py -v"
add icf_fold_safe          plain     fast \
    "ICF fold-poisoning: the unsafe name comparison RAISES instead of conflating folded with wrong" \
    "python3 tools/test_icf_fold_safe.py"
add icf_fold_safe_break    selfbreak fast \
    "...and prove THAT guard can fail (polarity: rc=0 == the proof SUCCEEDED)" \
    "python3 tools/test_icf_fold_safe.py --self-break"
add icf_alias_validate     refusable fast \
    "every ICF alias group in scripts/symbol_aliases.json is grounded (the ONLY defence of that file)" \
    "python3 tools/icf_alias_finder.py --validate"
add objdiff_map_cache      property  fast \
    "the consumed objdiff keys its report cache on the ALIAS MAP (~25s; guards the deleted icf_aliases_cache_purged edge)" \
    "python3 tools/check_objdiff_map_cache.py ${CI_GUARDS_OBJDIFF:+--objdiff $CI_GUARDS_OBJDIFF}"
add objdiff_eh_prefix      property  fast \
    "the consumed objdiff bounds the MSVC EH funclet prefix itself (guards the retired obj_eh_boundary_patcher)" \
    "python3 tools/check_objdiff_eh_prefix.py ${CI_GUARDS_OBJDIFF:+--objdiff $CI_GUARDS_OBJDIFF}"
add symbols_fixpoint       refusable SLOW \
    "config/<v>/symbols.txt is at dtk's FIXED POINT (forces its own re-split, ~10s+)" \
    "python3 tools/symbols_fixpoint_guard.py -v --version $VERSION"
add symbols_fixpoint_break selfbreak SLOW \
    "...and prove THAT guard can fail (polarity: rc=0 == the proof SUCCEEDED)" \
    "python3 tools/symbols_fixpoint_guard.py -v --self-break --version $VERSION"
add native_link_gate       native    SLOW \
    "the native build LINKS -- the only instrument here that can see ODR/undefined-symbol breaks" \
    "./tools/native_build_gate.sh ."

in_csv() { case ",$2," in *",$1,"*) return 0 ;; *) return 1 ;; esac; }

if [ "$LIST" -eq 1 ]; then
    printf "%-26s %-10s %-5s %s\n" ID MODE SPEED DESCRIPTION
    for i in "${!G_ID[@]}"; do
        printf "%-26s %-10s %-5s %s\n" "${G_ID[$i]}" "${G_MODE[$i]}" "${G_SPEED[$i]}" "${G_DESC[$i]}"
    done
    exit 0
fi

mkdir -p "$LOGDIR" || { echo "cannot create logdir $LOGDIR" >&2; exit 2; }

echo "=== CI GUARDS ==="
echo "repo:    $REPO"
echo "version: $VERSION"
echo "logs:    $LOGDIR"
echo

total=0; passed=0; failed=0; blocked=0; skipped=0
# Explicitly empty, not merely declared: `${#arr[@]:-0}` is a BAD SUBSTITUTION in
# bash (caught by actually running the red leg -- the FAILED: line silently never
# printed, which would have hidden exactly what this runner exists to surface).
FAILED_IDS=(); BLOCKED_IDS=()

for i in "${!G_ID[@]}"; do
    id="${G_ID[$i]}"; mode="${G_MODE[$i]}"; speed="${G_SPEED[$i]}"
    desc="${G_DESC[$i]}"; cmd="${G_CMD[$i]}"

    if [ -n "$ONLY" ] && ! in_csv "$id" "$ONLY"; then skipped=$((skipped+1)); continue; fi
    if [ -n "$SKIP" ] && in_csv "$id" "$SKIP"; then
        skipped=$((skipped+1)); echo "--- $id -- SKIPPED (--skip)"; continue
    fi
    if [ "$FAST" -eq 1 ] && [ "$speed" = "SLOW" ]; then
        skipped=$((skipped+1)); echo "--- $id -- SKIPPED (--fast; this one is SLOW)"; continue
    fi

    total=$((total+1))
    log="$LOGDIR/$id.log"
    echo "--- [$total] $id  ($speed)"
    echo "    $desc"
    echo "    \$ $cmd"

    # ⛔ A MISSING GUARD SCRIPT MUST READ "COULD NOT RUN", NEVER "FAILED".
    # `python3 nosuchfile.py` exits 2 -- which is exactly the code the property
    # guards use for "the property no longer holds". This bit THIS LANE: an EH
    # guard absent from the worktree reported a confident rc=2 FAIL in 0s and
    # was very nearly written up as an independent confirmation that a
    # rolled-back objdiff binary had been caught. It was a missing file. A
    # vacuity that AGREES WITH YOUR PRIOR is the hardest kind to catch, so the
    # existence check is mechanical rather than remembered.
    script=""
    if [[ "$cmd" =~ ([^[:space:]]+\.(py|sh)) ]]; then script="${BASH_REMATCH[1]}"; fi
    if [ -n "$script" ] && [ ! -f "$script" ]; then
        blocked=$((blocked+1)); BLOCKED_IDS+=("$id")
        echo "    rc=--  0s  status=BLOCKED"
        echo "    note: guard script not found: $script (not a failure -- this"
        echo "          guard could not run at all; is it on this branch?)"
        echo "CI_GUARD id=$id mode=$mode rc=NA status=BLOCKED secs=0"
        echo
        continue
    fi

    t0=$SECONDS
    eval "$cmd" > "$log" 2>&1
    rc=$?
    secs=$((SECONDS - t0))

    note=""
    case "$mode" in
        plain)
            if [ "$rc" -eq 0 ]; then status=PASS; else status=FAIL; fi ;;
        refusable)
            case "$rc" in
                0) status=PASS ;;
                2) status=BLOCKED; note="exit 2 == REFUSED: the instrument does not have its inputs (not a pass, not a failure)" ;;
                *) status=FAIL ;;
            esac ;;
        selfbreak)
            case "$rc" in
                0) status=PASS; note="rc=0 == THE PROOF SUCCEEDED (guard detected the plant)" ;;
                1) status=FAIL; note="rc=1 == the guard is VACUOUS: it did NOT detect the planted violation" ;;
                2) status=BLOCKED; note="exit 2 == could not run the proof" ;;
                *) status=FAIL ;;
            esac ;;
        property)
            case "$rc" in
                0) status=PASS ;;
                5) status=BLOCKED; note="exit 5 == VACUOUS: the probe proved nothing (not a pass)" ;;
                *) status=FAIL; note="the retired pass's redundancy PROPERTY no longer holds -- restore the pass/edge or move the tool forward" ;;
            esac ;;
        native)
            line="$(grep -E '^NATIVE_GATE_RESULT ' "$log" | tail -1)"
            if [ -z "$line" ]; then
                status=FAIL; note="no NATIVE_GATE_RESULT line -- cannot verify coverage"
            else
                verdict="$(printf '%s' "$line" | sed -n 's/.*verdict=\([A-Z]*\).*/\1/p')"
                nskip="$(printf '%s' "$line" | sed -n 's/.*skipped=\([0-9]*\).*/\1/p')"
                nfail="$(printf '%s' "$line" | sed -n 's/.*failed=\([0-9]*\).*/\1/p')"
                if [ "$verdict" = "FAIL" ] || [ "$rc" -eq 1 ] || [ "${nfail:-0}" -ne 0 ]; then
                    status=FAIL; note="$line"
                elif [ "$rc" -eq 2 ]; then
                    status=BLOCKED; note="UNRUNNABLE (cmake/clang missing?): $line"
                elif [ "${nskip:-1}" -ne 0 ] || [ "$verdict" != "PASS" ] || [ "$rc" -ne 0 ]; then
                    # THE 0-SKIP RULE. rc=0 with SKIPs is indistinguishable from a
                    # full pass to anything reading only the exit code.
                    status=BLOCKED; note="ran but does NOT vouch for full coverage (0-SKIP rule): $line"
                else
                    status=PASS; note="$line"
                fi
            fi ;;
        *) status=FAIL; note="unknown mode $mode" ;;
    esac

    # A guard that TIMED OUT never reached a verdict, so it is "could not run",
    # not "objects". This is not hypothetical here: symbols_fixpoint_guard forces
    # its own re-split through tools/ninja-locked, which SERIALISES against every
    # other agent on this box; with seven concurrent lanes it hit its own 1800s
    # timeout and let subprocess.TimeoutExpired escape as a traceback -> rc=1,
    # indistinguishable from genuine symbols.txt drift. Left as FAIL it would
    # make this runner cry wolf on any busy tree, and a runner people learn to
    # ignore is the disease this one exists to cure.
    if [ "$status" = "FAIL" ] && grep -qE "TimeoutExpired|timed out after" "$log" 2>/dev/null; then
        status=BLOCKED
        note="TIMED OUT -- no verdict reached (the shared ninja lock is held by another agent?). Not a failure; re-run when the tree is quiet."
    fi

    case "$status" in
        PASS)    passed=$((passed+1)) ;;
        FAIL)    failed=$((failed+1)); FAILED_IDS+=("$id") ;;
        BLOCKED) blocked=$((blocked+1)); BLOCKED_IDS+=("$id") ;;
    esac

    echo "    rc=$rc  ${secs}s  status=$status"
    [ -n "$note" ] && echo "    note: $note"
    if [ "$status" != "PASS" ]; then
        echo "    ---- last 12 lines of $log ----"
        tail -12 "$log" | sed 's/^/    | /'
    fi
    # machine-readable, one line per guard
    echo "CI_GUARD id=$id mode=$mode rc=$rc status=$status secs=$secs"
    echo
done

if [ "$failed" -ne 0 ]; then
    verdict=FAIL; rc=1
elif [ "$blocked" -ne 0 ]; then
    verdict=INCOMPLETE; rc=2
else
    verdict=PASS; rc=0
fi

echo "=== SUMMARY ==="
[ "${#FAILED_IDS[@]}"  -gt 0 ] && echo "FAILED:  ${FAILED_IDS[*]}"
[ "${#BLOCKED_IDS[@]}" -gt 0 ] && echo "BLOCKED: ${BLOCKED_IDS[*]} (could not run / does not vouch -- NOT passes)"
echo "logs:    $LOGDIR"
# The one machine-readable surface. Same shape as NATIVE_GATE_RESULT, and for
# the same reason: the prose verdict is easy to relay wrongly.
echo "CI_GUARDS_RESULT verdict=$verdict total=$total passed=$passed failed=$failed blocked=$blocked skipped=$skipped rc=$rc"
exit $rc
