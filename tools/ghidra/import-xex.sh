#!/bin/bash
# Import RB3 XEX into Ghidra with full analysis.
#
# Usage: ./tools/ghidra/import-xex.sh [project-name]
#
# Creates a Ghidra project with:
# - Full analysis (disassembly, strings, references)
#
# Note: Unlike DC3, rb3-xenon has no leaked .map file (ham_xbox_r.map).
# For function identification, use tools/fingerprint_match.py instead.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
MILOHAX_DIR="$(cd "$PROJECT_DIR/.." && pwd)"
GHIDRA_INSTALL_DIR="${GHIDRA_INSTALL_DIR:-$MILOHAX_DIR/ghidra/build/ghidra}"
GHIDRA="$GHIDRA_INSTALL_DIR/support/analyzeHeadless"

XEX="$PROJECT_DIR/orig/45410914/default.xex"
PROJECT_LOC="$PROJECT_DIR/ghidra_projects"
PROJECT_NAME="${1:-RB3Xenon}"

# Check prerequisites
if [[ ! -x "$GHIDRA" ]]; then
    echo "Error: Ghidra not found at $GHIDRA"
    echo "Set GHIDRA_INSTALL_DIR or build the VMX128 fork: cd ../ghidra && gradle buildGhidra"
    exit 1
fi

if [[ ! -f "$XEX" ]]; then
    echo "Error: XEX not found at $XEX"
    exit 1
fi

# Create project directory
mkdir -p "$PROJECT_LOC"

echo "=== RB3 Xenon Ghidra Import ==="
echo "XEX: $XEX"
echo "Project: $PROJECT_LOC/$PROJECT_NAME"
echo ""

# Check if project exists
if [[ -f "$PROJECT_LOC/$PROJECT_NAME.gpr" ]]; then
    echo "Project already exists. Delete it? [y/N]"
    read -r response
    if [[ "$response" =~ ^[Yy]$ ]]; then
        rm -rf "$PROJECT_LOC/$PROJECT_NAME.gpr" "$PROJECT_LOC/$PROJECT_NAME.rep"
    else
        echo "Aborting."
        exit 1
    fi
fi

# Import XEX with full analysis (single-pass — no map file for RB3)
echo "Importing XEX with full analysis..."
echo "  (This may take several minutes)"
"$GHIDRA" "$PROJECT_LOC" "$PROJECT_NAME" \
    -import "$XEX" \
    -log /tmp/ghidra-rb3xenon-import.log \
    2>&1 | grep -E "(INFO|ERROR|WARN)" | tail -20

echo ""
echo "=== Import Complete ==="
echo "Project: $PROJECT_LOC/$PROJECT_NAME.gpr"
echo ""
echo "To open in Ghidra GUI:"
echo "  ghidra &"
echo "  File -> Open Project -> $PROJECT_LOC/$PROJECT_NAME.gpr"
echo ""
echo "To start the MCP service:"
echo "  ./tools/ghidra/pyghidra-service.sh start"
echo ""
echo "To search strings from CLI:"
echo "  $GHIDRA $PROJECT_LOC $PROJECT_NAME -process default.xex \\"
echo "    -scriptPath $SCRIPT_DIR -postScript SearchString.java \"pattern\" -noanalysis"
