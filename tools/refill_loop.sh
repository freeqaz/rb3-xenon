#!/usr/bin/env bash
# refill_loop.sh — one-command driver for the post-wave REFILL SWEEP.
#
# Wraps the hand loop that the 2026-06-09 refill agent ran (+255 matches, 0
# regressions) into a single repeatable, honesty-gated driver:
#
#   baseline fresh report
#   loop (cap MAX_ITERS):
#     1. pin_identified.py --apply           extend under-pinned units to cover
#                                            byte-exact identified addrs
#     2. RENAMER REFRESH                      rm target_symbol_renames.stamp +
#                                            touch config.yml  (MANDATORY: without
#                                            it new map entries read +0)
#     3. build                                ./tools/ninja-locked
#     4. reveal_sweep -> safe_name_merge gate -> merge into target_symbol_map.json
#     5. RENAMER REFRESH + build              so the merged names take effect
#     -> stop when a reveal wave adds 0 safe names (the cascade is drained)
#   final fresh report
#   per-unit A/B (tools/ab_measure.py): net delta + gained/dropped units
#   EXIT NONZERO if ANY unit dropped matches (so callers can gate landings).
#
# The per-unit A/B reuses the existing tools/ab_measure.py helper (it already
# diffs two report.json by matched_functions per unit) -- no new diff helper.
#
# CONSTRAINTS (load-bearing, see CLAUDE.md):
#   * NO mtime guard is added anywhere around config.json / the split rule. The
#     config.json mtime advancing is what re-runs the target-symbol-renamer.
#   * The renamer-refresh recipe is EXACTLY:
#       rm -f build/45410914/target_symbol_renames.stamp
#       touch config/45410914/config.yml
#     Do not "optimize" it.
#   * Honor NINJA_JOBS (default 12); everything tee'd to /tmp/refill_loop.log.
#
# Run this FROM A WORKTREE (never the shared main tree): it mutates splits.txt
# and scripts/target_symbol_map.json. See scripts/setup_worktree.sh.
#
# Usage:
#   tools/refill_loop.sh [--map FILE] [--max-iters N] [--dry-run]
#   NINJA_JOBS=8 tools/refill_loop.sh
#
#   --map FILE      addr->owner identification map for pin_identified.py
#                   (default: global_fuzzy_pairs.json in repo root, if present).
#                   If absent, the pin_identified step is SKIPPED (reveal still
#                   runs -- a pure reveal refill is valid).
#   --max-iters N   cap on loop iterations (default 5).
#   --dry-run       print every command the driver would run, run nothing.
#
# Exit codes:
#   0   completed; net delta >= 0 AND no unit dropped matches.
#   1   a unit DROPPED matches (regression) -- caller should NOT land.
#   2   a build failed, or a required tool/file was missing.

set -euo pipefail

REPO="$(cd "$(dirname "$0")/.." && pwd)"
LOG="/tmp/refill_loop.log"

MAP="$REPO/global_fuzzy_pairs.json"
MAX_ITERS=5
DRY_RUN=0

while [ $# -gt 0 ]; do
    case "$1" in
        --map)       MAP="$2"; shift 2 ;;
        --map=*)     MAP="${1#*=}"; shift ;;
        --max-iters) MAX_ITERS="$2"; shift 2 ;;
        --max-iters=*) MAX_ITERS="${1#*=}"; shift ;;
        --dry-run)   DRY_RUN=1; shift ;;
        -h|--help)   sed -n '2,48p' "$0"; exit 0 ;;
        *) echo "Unknown argument: $1" >&2; exit 2 ;;
    esac
done

NINJA_JOBS="${NINJA_JOBS:-12}"
JOBS_ARG=()
if [ "$NINJA_JOBS" != "0" ]; then
    JOBS_ARG=(-j "$NINJA_JOBS")
fi

NINJA="$REPO/tools/ninja-locked"
SPLITS="$REPO/config/45410914/splits.txt"
CONFIG_YML="$REPO/config/45410914/config.yml"
STAMP="$REPO/build/45410914/target_symbol_renames.stamp"
TSM="$REPO/scripts/target_symbol_map.json"
REPORT="$REPO/build/45410914/report.json"

WORK="${TMPDIR:-/tmp}/refill_loop.$$"

# ── logging: start fresh, tee everything ──────────────────────────────────────
: > "$LOG"
log()  { echo "$@" | tee -a "$LOG"; }
hr()   { log "────────────────────────────────────────────────────────────────"; }

# run CMD..., echoing it; in --dry-run only echo. Output tee'd to the log.
run() {
    log "+ $*"
    if [ "$DRY_RUN" -eq 1 ]; then
        return 0
    fi
    "$@" 2>&1 | tee -a "$LOG"
    return "${PIPESTATUS[0]}"
}

renamer_refresh() {
    # EXACT recipe -- do not change (see header / CLAUDE.md).
    run rm -f "$STAMP"
    run touch "$CONFIG_YML"
}

build() {
    if ! run "$NINJA" "${JOBS_ARG[@]}"; then
        log "ERROR: build failed (see $LOG)."
        exit 2
    fi
}

log "refill_loop.sh: starting (log: $LOG)"
log "  repo       : $REPO"
log "  ninja jobs : ${JOBS_ARG[*]:-(default)}"
log "  map        : $MAP $( [ -f "$MAP" ] || echo '(absent -> pin step skipped)')"
log "  max iters  : $MAX_ITERS"
log "  dry-run    : $DRY_RUN"

if [ "$DRY_RUN" -eq 0 ]; then
    mkdir -p "$WORK"
fi
BASELINE="$WORK/baseline.report.json"

# ── sanity: required helpers exist ────────────────────────────────────────────
for t in tools/pin_identified.py tools/reveal_sweep.py tools/safe_name_merge.py \
         tools/fresh_report.sh tools/ab_measure.py; do
    if [ ! -f "$REPO/$t" ]; then
        log "ERROR: missing required helper: $t"
        [ "$DRY_RUN" -eq 1 ] || exit 2
    fi
done

# ── baseline fresh report (frozen for the A/B) ────────────────────────────────
hr
log "BASELINE: fresh full report"
if ! run "$REPO/tools/fresh_report.sh"; then
    log "ERROR: baseline fresh_report.sh failed."
    [ "$DRY_RUN" -eq 1 ] || exit 2
fi
run cp "$REPORT" "$BASELINE"

# ── the refill loop ───────────────────────────────────────────────────────────
iter=0
while [ "$iter" -lt "$MAX_ITERS" ]; do
    iter=$((iter + 1))
    hr
    log "ITERATION $iter / $MAX_ITERS"

    # 1. pin_identified (only if a map is present) ─────────────────────────────
    if [ -f "$MAP" ]; then
        log "step 1: pin_identified --apply (extend under-pinned units)"
        run python3 "$REPO/tools/pin_identified.py" --map "$MAP" \
            --splits "$SPLITS" --apply
    else
        log "step 1: pin_identified SKIPPED (no map at $MAP$( \
            [ "$DRY_RUN" -eq 1 ] && echo ' -- with a map present it would run pin_identified.py --apply'))"
    fi

    # 2. renamer refresh (MANDATORY before measuring) ──────────────────────────
    log "step 2: renamer refresh (rm stamp + touch config.yml)"
    renamer_refresh

    # 3. build ─────────────────────────────────────────────────────────────────
    log "step 3: build"
    build

    # 4. reveal_sweep -> gate -> merge ─────────────────────────────────────────
    log "step 4: reveal_sweep + gate + merge"
    CAND="$WORK/reveal_cand.$iter.json"
    FRAG="$WORK/reveal_frag.$iter.json"
    SAFE="$WORK/reveal_safe.$iter.json"
    run python3 "$REPO/tools/reveal_sweep.py" --out "$CAND" --emit-fragment "$FRAG"

    # gate (writes the safe subset; does NOT merge yet so we can count it)
    run python3 "$REPO/tools/safe_name_merge.py" --gate "$FRAG" \
        --tsm "$TSM" --splits "$SPLITS" --out "$SAFE"

    # count safe names added this wave
    added=0
    if [ "$DRY_RUN" -eq 0 ] && [ -f "$SAFE" ]; then
        added=$(python3 -c '
import json, sys
try:
    d = json.load(open(sys.argv[1]))
    print(len(d) if isinstance(d, dict) else len(d))
except Exception:
    print(0)
' "$SAFE")
    fi
    log "  reveal wave $iter: $added safe name(s)"

    if [ "$DRY_RUN" -eq 1 ]; then
        # In dry-run, show one full pass then a representative final pass.
        log "  [dry-run] would merge safe subset into $TSM, renamer-refresh, rebuild"
        log "  [dry-run] would stop the loop when a wave adds 0 safe names"
        run python3 "$REPO/tools/safe_name_merge.py" --gate "$FRAG" \
            --merge --tsm "$TSM" --splits "$SPLITS"
        renamer_refresh
        log "step 5: build (apply merged names)"
        build
        break
    fi

    if [ "$added" -eq 0 ]; then
        log "  reveal cascade drained (0 safe names) -> stopping loop after iter $iter"
        break
    fi

    # merge the safe subset into the tsm
    run python3 "$REPO/tools/safe_name_merge.py" --gate "$FRAG" \
        --merge --tsm "$TSM" --splits "$SPLITS"

    # 5. renamer refresh + build so merged names take effect ───────────────────
    log "step 5: renamer refresh + build (apply merged names)"
    renamer_refresh
    build
done

if [ "$iter" -ge "$MAX_ITERS" ]; then
    log "NOTE: hit MAX_ITERS=$MAX_ITERS; cascade may not be fully drained."
fi

# ── final fresh report + honesty A/B ──────────────────────────────────────────
hr
log "FINAL: fresh full report"
if ! run "$REPO/tools/fresh_report.sh"; then
    log "ERROR: final fresh_report.sh failed."
    [ "$DRY_RUN" -eq 1 ] || exit 2
fi

hr
log "A/B (per-unit): baseline vs final  (tools/ab_measure.py)"
if [ "$DRY_RUN" -eq 1 ]; then
    log "+ python3 $REPO/tools/ab_measure.py --worktree $REPO --baseline $BASELINE --json"
    log ""
    log "refill_loop.sh: DRY-RUN complete (no commands executed; nothing measured)."
    rm -rf "$WORK" 2>/dev/null || true
    exit 0
fi

AB_JSON="$WORK/ab.json"
python3 "$REPO/tools/ab_measure.py" --worktree "$REPO" \
    --baseline "$BASELINE" --json | tee "$AB_JSON" | tee -a "$LOG"

# parse the A/B verdict
read -r NET NREG < <(python3 -c '
import json, sys
d = json.load(open(sys.argv[1]))
print(d.get("net_delta", 0), d.get("n_regressed_units", 0))
' "$AB_JSON")

hr
log "refill_loop.sh: done."
log "  net matched-function delta : $NET"
log "  units dropping matches     : $NREG"
log "  full log                   : $LOG"
log "  A/B json                   : $AB_JSON"

if [ "$NREG" -gt 0 ]; then
    log "RESULT: REGRESSION -- $NREG unit(s) lost matches. DO NOT land as-is."
    exit 1
fi
log "RESULT: clean (net $NET, 0 units dropped)."
exit 0
