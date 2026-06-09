#!/usr/bin/env bash
# fresh_report.sh — Generate a guaranteed-fresh, full build/45410914/report.json.
#
# Problem: A partial rebuild (e.g. only some .obj files changed) can leave
# report.json with stale data if ninja's mtime tracking decided it didn't
# need regenerating.  Per-unit A/B measurements get misled by mixed-vintage
# objects.
#
# Solution:
#   1. Build all compiled .obj targets (all_source) to ensure every object is
#      up-to-date with the current source.
#   2. Delete report.json to force unconditional regeneration.
#   3. Rebuild report.json via ninja (runs objdiff-cli report generate).
#   4. Verify freshness: report.json mtime is newer than every .obj file.
#
# IMPORTANT: this script intentionally does NOT touch the split rule or
# config.json mtime (the config.json mtime is load-bearing for the
# target-symbol-renamer; see CLAUDE.md).  It only forces the report step.
#
# Parallelism cap: at full parallelism on 32 cores, wave agents running this on
# fresh btrfs-CoW worktrees hit code-137 OOM kills under wibo/MSVC (each cl.exe
# under wibo is memory-heavy).  We cap ninja's job count: honor $NINJA_JOBS if set,
# else default to 12 (both -j 12 and -j 4 were verified safe).  Set NINJA_JOBS=0 to
# pass no -j (use ninja's default).
#
# Usage:
#   ./tools/fresh_report.sh [--dry-run] [--no-verify]
#       --dry-run    Print what would be done but do not execute.
#       --no-verify  Skip the post-build freshness verification.
#   NINJA_JOBS=N ./tools/fresh_report.sh   # override the parallelism cap
#
# Output is tee'd to /tmp/rb3_build_fresh_report.log.

set -euo pipefail

REPO="$(cd "$(dirname "$0")/.." && pwd)"
LOG="/tmp/rb3_build_fresh_report.log"
DRY_RUN=0
NO_VERIFY=0

for arg in "$@"; do
    case "$arg" in
        --dry-run)   DRY_RUN=1  ;;
        --no-verify) NO_VERIFY=1 ;;
        *) echo "Unknown argument: $arg" >&2; exit 1 ;;
    esac
done

REPORT="$REPO/build/45410914/report.json"
NINJA="$REPO/tools/ninja-locked"

# Parallelism cap (see header).  NINJA_JOBS overrides; 0 means "no -j".
NINJA_JOBS="${NINJA_JOBS:-12}"
JOBS_ARG=()
if [ "$NINJA_JOBS" != "0" ]; then
    JOBS_ARG=(-j "$NINJA_JOBS")
fi

echo "fresh_report.sh: starting (log: $LOG)" | tee -a "$LOG"
echo "  parallelism: ninja ${JOBS_ARG[*]:-(default)}" | tee -a "$LOG"
echo "  repo:   $REPO" | tee -a "$LOG"
echo "  report: $REPORT" | tee -a "$LOG"

# ── Step 1: build all compiled objects ────────────────────────────────────────
echo "" | tee -a "$LOG"
echo "Step 1: ensuring all_source is up-to-date..." | tee -a "$LOG"
if [ "$DRY_RUN" -eq 1 ]; then
    echo "  [dry-run] would run: $NINJA ${JOBS_ARG[*]} all_source" | tee -a "$LOG"
else
    "$NINJA" "${JOBS_ARG[@]}" all_source 2>&1 | tee -a "$LOG"
    echo "  all_source OK" | tee -a "$LOG"
fi

# ── Step 2: delete report.json to force regeneration ─────────────────────────
echo "" | tee -a "$LOG"
echo "Step 2: removing stale report.json..." | tee -a "$LOG"
if [ "$DRY_RUN" -eq 1 ]; then
    echo "  [dry-run] would rm -f $REPORT" | tee -a "$LOG"
else
    rm -f "$REPORT"
    echo "  report.json deleted" | tee -a "$LOG"
fi

# ── Step 3: regenerate report.json ────────────────────────────────────────────
echo "" | tee -a "$LOG"
echo "Step 3: regenerating report.json..." | tee -a "$LOG"
if [ "$DRY_RUN" -eq 1 ]; then
    echo "  [dry-run] would run: $NINJA ${JOBS_ARG[*]} build/45410914/report.json" | tee -a "$LOG"
else
    "$NINJA" "${JOBS_ARG[@]}" build/45410914/report.json 2>&1 | tee -a "$LOG"
    if [ ! -f "$REPORT" ]; then
        echo "ERROR: report.json was not created by ninja" | tee -a "$LOG"
        exit 1
    fi
    echo "  report.json regenerated OK ($(wc -c < "$REPORT") bytes)" | tee -a "$LOG"
fi

# ── Step 4: verify freshness ──────────────────────────────────────────────────
if [ "$DRY_RUN" -eq 1 ] || [ "$NO_VERIFY" -eq 1 ]; then
    echo "" | tee -a "$LOG"
    echo "Step 4: skipping verification (dry-run or --no-verify)" | tee -a "$LOG"
else
    echo "" | tee -a "$LOG"
    echo "Step 4: verifying freshness..." | tee -a "$LOG"

    report_mtime=$(stat -c %Y "$REPORT")
    stale_count=0
    stale_examples=()

    # Check all compiled .obj files under build/45410914/src/
    while IFS= read -r obj; do
        obj_mtime=$(stat -c %Y "$obj" 2>/dev/null || echo 0)
        if [ "$obj_mtime" -gt "$report_mtime" ]; then
            stale_count=$((stale_count + 1))
            if [ ${#stale_examples[@]} -lt 5 ]; then
                stale_examples+=("$obj")
            fi
        fi
    done < <(find "$REPO/build/45410914/src" -name "*.obj" -type f 2>/dev/null)

    if [ "$stale_count" -eq 0 ]; then
        echo "  Freshness OK: report.json is newer than all .obj files." | tee -a "$LOG"
    else
        echo "  WARNING: $stale_count .obj file(s) are newer than report.json:" | tee -a "$LOG"
        for ex in "${stale_examples[@]}"; do
            echo "    $ex" | tee -a "$LOG"
        done
        echo "  This may indicate a concurrent build modified objects after" | tee -a "$LOG"
        echo "  report generation.  Re-run fresh_report.sh to retry." | tee -a "$LOG"
        # Non-fatal: warn but don't fail (a concurrent agent may have just compiled)
    fi
fi

echo "" | tee -a "$LOG"
echo "fresh_report.sh: done.  Report: $REPORT" | tee -a "$LOG"
if [ -f "$REPORT" ]; then
    python3 - "$REPORT" 2>/dev/null <<'EOF' | tee -a "$LOG" || true
import json, sys
with open(sys.argv[1]) as f:
    r = json.load(f)
units = r.get("units", [])
fns = sum(len(u.get("functions", [])) for u in units)
matched = sum(1 for u in units for fn in u.get("functions", [])
              if fn.get("fuzzy_match_percent", 0) == 100.0)
print(f"  Units: {len(units)}  Functions: {fns}  Matched@100%: {matched}")
EOF
fi
