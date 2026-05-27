#!/usr/bin/env bash
#
# Configures an EXISTING git worktree with a fully working build system.
# Use this for worktrees created by external tools (e.g., Claude Code's
# built-in worktree isolation) that don't go through setup_worktree.sh.
#
# Usage:
#   scripts/configure_existing_worktree.sh <worktree-path>
#
# If no path is given, uses the current working directory.
#
# This script is idempotent — safe to run multiple times on the same worktree.

set -euo pipefail

MAIN_REPO="$(cd "$(dirname "$0")/.." && pwd)"
WORKTREE_PATH="${1:-$(pwd)}"

# Resolve to absolute path
WORKTREE_PATH="$(cd "$WORKTREE_PATH" && pwd)"

# Sanity check: must be a git worktree (has .git file, not .git directory)
if [ ! -f "$WORKTREE_PATH/.git" ] && [ ! -d "$WORKTREE_PATH/.git" ]; then
    echo "ERROR: $WORKTREE_PATH does not appear to be a git worktree or repository" >&2
    exit 1
fi

# Resolve tool paths (relative to main repo's parent)
TOOL_DIR="$(cd "$MAIN_REPO/.." && pwd)"
DTK_PATH="$TOOL_DIR/jeff/target/release/dtk"
OBJDIFF_PATH="$TOOL_DIR/objdiff/target/release/objdiff-cli"
WIBO_PATH="$TOOL_DIR/wibo/build/release/wibo"

# Verify tools exist
for tool in "$DTK_PATH" "$OBJDIFF_PATH" "$WIBO_PATH"; do
    if [ ! -f "$tool" ]; then
        echo "ERROR: Required tool not found: $tool" >&2
        exit 1
    fi
done

echo "==> Configuring worktree at $WORKTREE_PATH"

echo "    Symlinking clangd config"
if [ -f "$MAIN_REPO/compile_commands.json" ]; then
    ln -sf "$MAIN_REPO/compile_commands.json" "$WORKTREE_PATH/"
fi
if [ -f "$MAIN_REPO/.clangd" ]; then
    ln -sf "$MAIN_REPO/.clangd" "$WORKTREE_PATH/"
fi

echo "    Symlinking orig/ (target binary)"
rm -rf "$WORKTREE_PATH/orig"
ln -sf "$MAIN_REPO/orig" "$WORKTREE_PATH/orig"

echo "    Symlinking bin/objdiff-cli"
mkdir -p "$WORKTREE_PATH/bin"
if [ -f "$MAIN_REPO/bin/objdiff-cli" ]; then
    ln -sf "$MAIN_REPO/bin/objdiff-cli" "$WORKTREE_PATH/bin/objdiff-cli"
fi

echo "    Setting up build artifacts"
WT_BUILD="$WORKTREE_PATH/build/45410914"
MAIN_BUILD="$MAIN_REPO/build/45410914"
mkdir -p "$WT_BUILD" "$WT_BUILD/pch"
touch "$WT_BUILD/pch/system.pch"

# Target objects (original binary, never changes)
ln -sf "$MAIN_BUILD/obj" "$WT_BUILD/obj"

# Pre-split config (COPY, not symlink — see setup_worktree.sh for rationale)
if [ -f "$MAIN_BUILD/config.json" ]; then
    cp "$MAIN_BUILD/config.json" "$WT_BUILD/config.json"
fi

# Downloaded tools (compilers, binutils, sjiswrap)
for dir in compilers binutils tools; do
    if [ -e "$MAIN_REPO/build/$dir" ]; then
        ln -sf "$MAIN_REPO/build/$dir" "$WORKTREE_PATH/build/$dir"
    fi
done

echo "    Symlinking Python venv"
if [ -d "$MAIN_REPO/venv" ]; then
    ln -sf "$MAIN_REPO/venv" "$WORKTREE_PATH/venv"
fi

echo "    Running configure.py with absolute tool paths"
(
    cd "$WORKTREE_PATH"
    python3 configure.py \
        --dtk "$DTK_PATH" \
        --objdiff "$OBJDIFF_PATH" \
        --wrapper "$WIBO_PATH"
)

echo ""
echo "Worktree configured: $WORKTREE_PATH"
echo ""
echo "Usage with MCP orchestrator:"
echo "  run_objdiff(symbol, project_dir=\"$WORKTREE_PATH\")"
