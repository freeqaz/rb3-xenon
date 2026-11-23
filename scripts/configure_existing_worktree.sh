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
# This script is idempotent — safe to run multiple times on the same worktree
# (since 2026-11-22: before link_dir a second run wrote into the main checkout).

set -euo pipefail

# The main checkout, wherever this copy of the script lives: a worktree's own
# copy would otherwise make MAIN_REPO the worktree and link it to itself.
MAIN_REPO="$(dirname "$(git -C "$(dirname "$0")" rev-parse --path-format=absolute --git-common-dir)")"
WORKTREE_PATH="${1:-$(pwd)}"

# Resolve to absolute path
WORKTREE_PATH="$(cd "$WORKTREE_PATH" && pwd)"

if [ "$WORKTREE_PATH" = "$MAIN_REPO" ]; then
    echo "ERROR: $WORKTREE_PATH is the main checkout; give a worktree path" >&2
    exit 1
fi

# Link $2 -> $1 for a DIRECTORY target. -n: when $2 is already a symlink to a
# directory, replace the link; plain -sf follows it and writes $2/<name> INSIDE
# the target, which on a second run planted build/45410914/obj/obj in the MAIN
# checkout and failed on build/compilers. A real directory at $2 is kept:
# setup_worktree.sh makes some of these as reflinked copies on purpose.
link_dir() {
    if [ -d "$2" ] && [ ! -L "$2" ]; then
        echo "    keeping the real directory $2"
        return 0
    fi
    ln -sfn "$1" "$2"
}

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

# Target objects: a REAL reflinked copy, never a link. The split rewrites obj/
# (dtk regenerates every target obj, then the renamer patches them in place),
# so a linked obj/ made every build here rewrite the MAIN checkout's target
# objs, raw and un-renamed, under every other reader (2026-11-23: three
# ab_measure refusals). scripts/verify_target_image.py now refuses to build
# with an obj/ linked out of the tree; this converts one made by an older copy
# of this script.
if [ -L "$WT_BUILD/obj" ]; then
    echo "    replacing the obj/ symlink with a reflinked copy"
    rm "$WT_BUILD/obj"
fi
if [ -d "$WT_BUILD/obj" ]; then
    echo "    keeping the real directory $WT_BUILD/obj"
elif [ -d "$MAIN_BUILD/obj" ]; then
    cp -a --reflink=auto "$MAIN_BUILD/obj" "$WT_BUILD/obj"
fi

# Pre-split config (COPY, not symlink — see setup_worktree.sh for rationale)
if [ -f "$MAIN_BUILD/config.json" ]; then
    cp "$MAIN_BUILD/config.json" "$WT_BUILD/config.json"
fi

# Downloaded tools (compilers, binutils, sjiswrap)
for dir in compilers binutils tools; do
    if [ -e "$MAIN_REPO/build/$dir" ]; then
        link_dir "$MAIN_REPO/build/$dir" "$WORKTREE_PATH/build/$dir"
    fi
done

echo "    Symlinking Python venv"
if [ -d "$MAIN_REPO/venv" ]; then
    link_dir "$MAIN_REPO/venv" "$WORKTREE_PATH/venv"
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
