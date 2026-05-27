#!/bin/bash
# Search for strings (with xrefs) in RB3Xenon Ghidra project — headless wrapper
#
# Usage: ./tools/ghidra/search-string.sh <pattern>
#
# Requires: GHIDRA_INSTALL_DIR set (or VMX128 fork auto-detected),
# and ghidra_projects/RB3Xenon created via import-xex.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
MILOHAX_DIR="$(cd "$PROJECT_DIR/.." && pwd)"

GHIDRA_INSTALL_DIR="${GHIDRA_INSTALL_DIR:-$MILOHAX_DIR/ghidra/build/ghidra}"
GHIDRA="$GHIDRA_INSTALL_DIR/support/analyzeHeadless"

PROJECT_LOC="$PROJECT_DIR/ghidra_projects"
PROJECT_NAME="RB3Xenon"

if [[ $# -eq 0 ]]; then
    echo "Usage: $0 <pattern>"
    echo ""
    echo "Searches defined strings and raw memory, shows function xrefs."
    echo "Examples:"
    echo "  $0 CharBones"
    echo "  $0 \"App.cpp\""
    echo "  $0 milo_assert"
    exit 1
fi

PATTERN="$1"

if [[ ! -x "$GHIDRA" ]]; then
    echo "Error: analyzeHeadless not found at $GHIDRA"
    echo "Set GHIDRA_INSTALL_DIR to your Ghidra installation."
    exit 1
fi

if [[ ! -f "$PROJECT_LOC/$PROJECT_NAME.gpr" ]]; then
    echo "Error: Project not found at $PROJECT_LOC/$PROJECT_NAME.gpr"
    echo "Run import-xex.sh first."
    exit 1
fi

"$GHIDRA" "$PROJECT_LOC" "$PROJECT_NAME" \
    -process default.xex \
    -scriptPath "$SCRIPT_DIR" \
    -postScript SearchString.java "$PATTERN" \
    -noanalysis \
    2>&1 | grep -E "(Searching|===|Found|matching|raw memory|  [0-9a-f]{8}:|    <-)"
