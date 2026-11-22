#!/bin/bash
# ghidra_sync.sh -- bring the live RB3 Ghidra program up to what the repo knows, in one command
# (docs/plans/ghidra-data-sync-2026-11-22.md, phase 5). Re-runnable: every step reconciles against today's
# repo state, so a second run only applies what changed.
#
#   tools/ghidra/ghidra_sync.sh                 # build + census on a COPY; the live service is not touched
#   tools/ghidra/ghidra_sync.sh --swap          # ... then swap the copy in (~1 min outage on :8002)
#   tools/ghidra/ghidra_sync.sh --layouts FILE  # reuse an emitted layout JSON (skip the ~15 min re-emit)
#   tools/ghidra/ghidra_sync.sh --skip-layouts  # phases 1-3 only
#   tools/ghidra/ghidra_sync.sh --program default_tu5.xex-c5a170   # sync the Deluxe program instead
#
# Run it from the MAIN checkout (the Ghidra project lives only there, gitignored) after the repo change you
# want reflected has landed. Steps, each logged under $WORK (default ~/tmp/ghidra-sync/run-<stamp>/):
#   0. reflink-copy ghidra_projects/RB3Xenon/RB3Xenon (1.3 GB, seconds)
#   1. function names   build_full_symbol_map.py + apply_symbols.py (reconciles: renames, replaces, removes
#                       names the map dropped, using the previous run's map kept in the project as --prev-map)
#   2. data names       build_data_symbol_map.py + apply_data_symbols.py
#   3. prototypes       apply_prototypes.py (demangled signatures of every mangled-named function)
#   4. class layouts    emit_class_layouts.py (one /d1reportAllClassLayout compile per TU; needs a configured
#                       tree) + apply_class_layouts.py (fills the class structures `this` points at)
#   5. census           ghidra_sync_census.py over ~/tmp/ghidra-sync/bodies20.txt; writes sync_state.json,
#                       which tools/ghidra/ghidra-status.py prints as its "Sync:" line
#   6. --swap           stop :8002, move the live project aside as RB3Xenon.bak-<stamp>, move the copy in,
#                       move the repo-root cache.db* aside (pyghidra's decompile cache), start, wait for ready.
#                       Tell the other lanes before and after; delete the backup once you trust the result.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$SCRIPT_DIR/../.." && pwd)"
# the main checkout (this script may run from a worktree; Ghidra, pyghidra-mcp and the project hang off main)
MAIN_REPO="$(cd "$(git -C "$REPO" rev-parse --path-format=absolute --git-common-dir)/.." && pwd)"
MILOHAX_DIR="$(cd "$MAIN_REPO/.." && pwd)"
LIVE_HOME="$REPO/ghidra_projects/RB3Xenon/RB3Xenon"
STAMP="$(date +%Y%m%d-%H%M%S)"
WORK="$HOME/tmp/ghidra-sync/run-$STAMP"
PROGRAM="default.xex-d56e7f"
SWAP=0; SKIP_LAYOUTS=0; LAYOUTS=""
BODIES="$HOME/tmp/ghidra-sync/bodies20.txt"
while [[ $# -gt 0 ]]; do
    case "$1" in
        --swap) SWAP=1 ;;
        --skip-layouts) SKIP_LAYOUTS=1 ;;
        --layouts) LAYOUTS="$2"; shift ;;
        --program) PROGRAM="$2"; shift ;;
        --work) WORK="$2"; shift ;;
        --bodies) BODIES="$2"; shift ;;
        *) echo "unknown arg: $1" >&2; exit 2 ;;
    esac
    shift
done
if [[ ! -d "$LIVE_HOME/RB3Xenon.rep" ]]; then
    # a worktree: the project lives only in the main checkout. Building a copy from here is fine;
    # swapping is not (pyghidra-service.sh derives the project path from its own location).
    LIVE_HOME="$MAIN_REPO/ghidra_projects/RB3Xenon/RB3Xenon"
    [[ $SWAP -eq 0 ]] || { echo "--swap: run from the main checkout, not a worktree" >&2; exit 2; }
fi
[[ -d "$LIVE_HOME/RB3Xenon.rep" ]] || { echo "no live project at $LIVE_HOME" >&2; exit 2; }

export GHIDRA_INSTALL_DIR="${GHIDRA_INSTALL_DIR:-$MILOHAX_DIR/ghidra/build/ghidra}"
if [[ -z "${JAVA_HOME:-}" ]]; then
    _jmin=$(sed -n 's/^application.java.min=//p' "$GHIDRA_INSTALL_DIR/Ghidra/application.properties")
    for _jv in $(ls -d /usr/lib/jvm/java-*-openjdk | sed 's/.*java-\([0-9]*\)-openjdk/\1/' | sort -n); do
        if [[ "$_jv" -ge "${_jmin:-21}" ]]; then export JAVA_HOME="/usr/lib/jvm/java-$_jv-openjdk"; break; fi
    done
fi
export GHIDRA_USER_HOME="$HOME/tmp/ghidra-sync/ghidra_user"
pyg() { uv run --python 3.10 --project "$MILOHAX_DIR/pyghidra-mcp" python "$@"; }
step() { echo "==> [$(date +%H:%M:%S)] $*"; }

mkdir -p "$WORK"
COPY="$WORK/proj/RB3Xenon"
step "0 copy live project -> $COPY"
mkdir -p "$WORK/proj"
cp -a --reflink=always "$LIVE_HOME" "$WORK/proj/"
rm -f "$COPY"/*.lock "$COPY"/*.lock~
PREV="$COPY/sync_last_full_map.json"

step "1 function names (log $WORK/p1.log)"
python3 "$SCRIPT_DIR/build_full_symbol_map.py" --out "$WORK/full_map.json" > "$WORK/p1_build.log" 2>&1
PREV_ARGS=(); [[ -f "$PREV" ]] && PREV_ARGS=(--prev-map "$PREV")
pyg "$SCRIPT_DIR/apply_symbols.py" --project-location "$COPY" --program "$PROGRAM" \
    --map "$WORK/full_map.json" "${PREV_ARGS[@]}" > "$WORK/p1.log" 2>&1
grep "\[apply_symbols\] DONE" "$WORK/p1.log"
cp "$WORK/full_map.json" "$PREV"

step "2 data names (log $WORK/p2.log)"
python3 "$SCRIPT_DIR/build_data_symbol_map.py" --out "$WORK/data_map.json" > "$WORK/p2_build.log" 2>&1
pyg "$SCRIPT_DIR/apply_data_symbols.py" --project-location "$COPY" --program "$PROGRAM" \
    --map "$WORK/data_map.json" > "$WORK/p2.log" 2>&1
grep "\[apply_data\] DONE" "$WORK/p2.log"

step "3 prototypes (log $WORK/p3.log)"
pyg "$SCRIPT_DIR/apply_prototypes.py" --project-location "$COPY" --program "$PROGRAM" > "$WORK/p3.log" 2>&1
grep "\[apply_prototypes\] DONE" "$WORK/p3.log"

if [[ $SKIP_LAYOUTS -eq 0 ]]; then
    if [[ -z "$LAYOUTS" ]]; then
        LAYOUTS="$WORK/layouts.json"
        step "4a emit compiler layouts (log $WORK/p4_emit.log; ~15 min)"
        python3 "$SCRIPT_DIR/emit_class_layouts.py" --out "$LAYOUTS" > "$WORK/p4_emit.log" 2>&1
        grep "\[emit_layouts\] DONE" "$WORK/p4_emit.log"
    fi
    step "4b apply class layouts (log $WORK/p4.log)"
    pyg "$SCRIPT_DIR/apply_class_layouts.py" --project-location "$COPY" --program "$PROGRAM" \
        --layouts "$LAYOUTS" > "$WORK/p4.log" 2>&1
    grep "\[apply_layouts\] DONE" "$WORK/p4.log"
fi

step "5 census (log $WORK/census.log)"
BODY_ARGS=(); [[ -f "$BODIES" ]] && BODY_ARGS=(--bodies "$BODIES")
pyg "$SCRIPT_DIR/ghidra_sync_census.py" --project-location "$COPY" --program "$PROGRAM" \
    "${BODY_ARGS[@]}" --out-dir "$WORK/census" --stamp "$COPY/sync_state.json" > "$WORK/census.log" 2>&1
grep "^\[census\] {\|\[bodies\] totals" "$WORK/census.log"

if [[ $SWAP -eq 0 ]]; then
    step "done; the live project is untouched. Swap with: $0 --swap (or move $COPY in by hand)"
    exit 0
fi

step "6 swap (stop :8002, move, start)"
"$SCRIPT_DIR/pyghidra-service.sh" stop
mv "$LIVE_HOME" "$LIVE_HOME.bak-$STAMP"
mv "$COPY" "$LIVE_HOME"
mkdir -p "$WORK/cachedb-aside"
for f in "$REPO"/cache.db "$REPO"/cache.db-wal "$REPO"/cache.db-shm; do [[ -e "$f" ]] && mv "$f" "$WORK/cachedb-aside/"; done
"$SCRIPT_DIR/pyghidra-service.sh" start
LOGF=/tmp/claude/pyghidra-mcp-rb3xenon.log
if timeout 300 bash -c "until grep -q 'Uvicorn running' '$LOGF'; do sleep 3; done"; then
    step "service ready; backup kept at $LIVE_HOME.bak-$STAMP"
else
    echo "TIMEOUT: service not ready after 300 s; see $LOGF (backup at $LIVE_HOME.bak-$STAMP)" >&2
    exit 3
fi
