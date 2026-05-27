#!/usr/bin/env bash
#
# Sets up a git worktree with a fully working build system.
#
# Usage:
#   scripts/setup_worktree.sh [path] [branch-name] [base-ref]
#
# Arguments:
#   path       - Where to create the worktree (default: /tmp/claude/worktree-<timestamp>)
#   branch     - Branch name for the worktree (default: wt-<dirname>)
#   base-ref   - Git ref to branch from (default: current HEAD, i.e. whatever is checked out)
#
# Examples:
#   scripts/setup_worktree.sh /tmp/claude/my-feature my-feature
#   scripts/setup_worktree.sh /tmp/claude/test test-branch dev
#   scripts/setup_worktree.sh                         # auto-generates path & branch from HEAD
#
# What this does:
#   1. Creates a git worktree from the current HEAD (or specified ref)
#   2. Symlinks clangd config, orig/, and bin/objdiff-cli
#   3. Re-runs configure.py with absolute tool paths so build.ninja works
#   4. Symlinks shared build artifacts (compilers, target objects, etc.)
#
# IMPORTANT: The worktree branches from the currently checked-out commit by
# default, NOT from 'main'. This ensures worktrees have the latest code from
# whatever branch you're working on.
#
# After setup, you can build normally from the worktree:
#   cd /tmp/claude/my-feature && ninja build/45410914/src/system/flow/FlowCommand.obj
#
# The MCP orchestrator tools also work:
#   run_objdiff(symbol, project_dir="/tmp/claude/my-feature")

set -euo pipefail

MAIN_REPO="$(cd "$(dirname "$0")/.." && pwd)"
WORKTREE_PATH="${1:-/tmp/claude/worktree-$(date +%s)}"
BRANCH="${2:-wt-$(basename "$WORKTREE_PATH")}"
BASE_REF="${3:-HEAD}"

# Resolve the base ref to a concrete commit for clarity
BASE_COMMIT="$(git -C "$MAIN_REPO" rev-parse --short "$BASE_REF" 2>/dev/null)" || {
    echo "ERROR: Cannot resolve ref '$BASE_REF'" >&2
    exit 1
}
BASE_BRANCH="$(git -C "$MAIN_REPO" rev-parse --abbrev-ref HEAD 2>/dev/null || echo "detached")"

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

echo "==> Creating worktree at $WORKTREE_PATH"
echo "    Branch: $BRANCH"
echo "    Base:   $BASE_REF ($BASE_COMMIT, on $BASE_BRANCH)"
git -C "$MAIN_REPO" worktree add "$WORKTREE_PATH" -b "$BRANCH" "$BASE_REF"

echo "==> Symlinking clangd config"
ln -sf "$MAIN_REPO/compile_commands.json" "$WORKTREE_PATH/"
if [ -f "$MAIN_REPO/.clangd" ]; then
    ln -sf "$MAIN_REPO/.clangd" "$WORKTREE_PATH/"
fi

echo "==> Symlinking orig/ (target binary)"
# orig/ is gitignored, so the worktree gets an empty dir with .gitkeep
# Remove the empty dir and symlink the real one
rm -rf "$WORKTREE_PATH/orig"
ln -sf "$MAIN_REPO/orig" "$WORKTREE_PATH/orig"

echo "==> Symlinking bin/objdiff-cli"
if [ -f "$MAIN_REPO/bin/objdiff-cli" ]; then
    ln -sf "$MAIN_REPO/bin/objdiff-cli" "$WORKTREE_PATH/bin/objdiff-cli"
fi

echo "==> Symlinking shared build artifacts"
WT_BUILD="$WORKTREE_PATH/build/45410914"
MAIN_BUILD="$MAIN_REPO/build/45410914"
mkdir -p "$WT_BUILD" "$WT_BUILD/pch"
# Pre-create empty PCH file — WIBO_FS_CACHE=1 breaks creating new files in
# case-insensitive path components (45410914). cl.exe can overwrite existing files fine.
touch "$WT_BUILD/pch/system.pch"

# Target objects (original binary, never changes)
ln -sf "$MAIN_BUILD/obj" "$WT_BUILD/obj"

# Pre-split config — MUST exist BEFORE configure.py runs.
# configure.py generates a two-phase build.ninja: phase 1 only has the xex
# split rule, phase 2 (triggered by ninja generator re-run) has all compile
# rules. By providing config.json first, configure.py sees it immediately
# and generates the full build.ninja in one pass.
#
# COPY instead of symlink: the build.ninja "split" rule writes to config.json,
# so a symlink would corrupt the main repo's file. Concurrent worktrees would
# also race on the shared file. A copy (447KB) is safe for parallel use.
if [ -f "$MAIN_BUILD/config.json" ]; then
    cp "$MAIN_BUILD/config.json" "$WT_BUILD/config.json"
fi

# Downloaded tools (compilers, binutils, sjiswrap)
for dir in compilers binutils tools; do
    if [ -e "$MAIN_REPO/build/$dir" ]; then
        ln -sf "$MAIN_REPO/build/$dir" "$WORKTREE_PATH/build/$dir"
    fi
done

echo "==> Symlinking Python venv"
if [ -d "$MAIN_REPO/venv" ]; then
    ln -sf "$MAIN_REPO/venv" "$WORKTREE_PATH/venv"
fi

echo "==> Running configure.py with absolute tool paths"
(
    cd "$WORKTREE_PATH"
    python3 configure.py \
        --dtk "$DTK_PATH" \
        --objdiff "$OBJDIFF_PATH" \
        --wrapper "$WIBO_PATH"
)

echo ""
echo "Worktree ready at: $WORKTREE_PATH"
echo "Branch: $BRANCH (from $BASE_COMMIT on $BASE_BRANCH)"
echo ""
echo "Usage with MCP orchestrator:"
echo "  run_objdiff(symbol, project_dir=\"$WORKTREE_PATH\")"
