#!/bin/bash
# run_apply_symbols.sh — import our known mangled symbols into the RB3 Ghidra
# project by renaming the anonymous fn_<addr>/FUN_<addr> functions.
#
# Pipeline:
#   1. (re)generate the symbol map        (build_symbol_map.py)  unless --no-regen
#   2. stop the :8002 pyghidra-mcp service (exclusive project access)
#   3. analyzeHeadless ... -postScript apply_symbols.py -noanalysis
#   4. restart the :8002 service
#
# This ONLY touches the rb3-xenon service on port 8002. dc3 (8000) and rb3-Wii
# (8001) are never touched.
#
# Re-runnable: apply_symbols.py is idempotent. The whole rename is reproducible
# after a Ghidra re-import (tools/ghidra/import-xex.sh) by re-running this.
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
MILOHAX_DIR="$(cd "$PROJECT_DIR/.." && pwd)"

# Project location = the directory that directly contains <name>.gpr.
PROJECT_LOC="$PROJECT_DIR/ghidra_projects/RB3Xenon/RB3Xenon"
PROJECT_NAME="RB3Xenon"
MAP_JSON="$SCRIPT_DIR/rb3_symbol_map.json"

export JAVA_HOME="${JAVA_HOME:-/usr/lib/jvm/java-17-openjdk}"
export GHIDRA_INSTALL_DIR="${GHIDRA_INSTALL_DIR:-$MILOHAX_DIR/ghidra/build/ghidra}"
# Standalone PyGhidra (Python 3 + JPype) — run via the pyghidra-mcp venv that the
# service uses, so JPype/pyghidra match the Ghidra build.
PYGHIDRA_MCP="$MILOHAX_DIR/pyghidra-mcp"

REGEN=1
MIN_PCT=80
PY_ARGS=()
for arg in "$@"; do
    case "$arg" in
        --no-regen) REGEN=0 ;;
        --min-percent=*) MIN_PCT="${arg#*=}" ;;
        --dry-run) PY_ARGS+=("--dry-run") ;;
        *) echo "Unknown arg: $arg" >&2; exit 2 ;;
    esac
done

if [[ $REGEN -eq 1 ]]; then
    echo "==> Regenerating symbol map (min-percent=$MIN_PCT)"
    python3 "$SCRIPT_DIR/build_symbol_map.py" --min-percent "$MIN_PCT" --out "$MAP_JSON"
fi

if [[ ! -f "$MAP_JSON" ]]; then
    echo "ERROR: map not found: $MAP_JSON" >&2
    exit 1
fi

echo "==> Stopping :8002 pyghidra-mcp service (exclusive project access)"
"$SCRIPT_DIR/pyghidra-service.sh" stop || true
# Clear any stale project locks
rm -f "$PROJECT_LOC"/*.lock* 2>/dev/null || true

echo "==> Running PyGhidra rename (apply_symbols.py)"
LOG=/tmp/claude/ghidra-rb3xenon-apply.log
mkdir -p /tmp/claude 2>/dev/null || true
set +e
uv run --python 3.10 --project "$PYGHIDRA_MCP" python "$SCRIPT_DIR/apply_symbols.py" \
    --project-location "$PROJECT_LOC" \
    --project-name "$PROJECT_NAME" \
    --map "$MAP_JSON" \
    "${PY_ARGS[@]}" \
    2>&1 | tee "$LOG"
RC=${PIPESTATUS[0]}
set -e

echo "==> Restarting :8002 pyghidra-mcp service"
"$SCRIPT_DIR/pyghidra-service.sh" start || true

echo "==> apply_symbols summary line:"
grep "\[apply_symbols\] DONE" "$LOG" || echo "(no DONE line — check $LOG)"
exit $RC
