#!/bin/bash
#
# setup_worktree.sh — create a buildable + diffable git worktree, cheaply (CoW).
#
# A naive `git worktree add` produces an UNBUILDABLE tree here: the big build
# inputs/outputs are gitignored (build/, orig/*, build.ninja, objdiff.json),
# so a fresh worktree has no target binary, no toolchain, no generated
# build.ninja, and a cold object cache. This script fixes that in seconds using
# btrfs/xfs copy-on-write reflinks (with graceful fall-back to full copies on
# non-CoW filesystems like tmpfs/ext4). The rb3-xenon repo lives on btrfs, so
# reflinks are the normal path.
#
# Usage:
#   scripts/setup_worktree.sh [path] [branch-name] [base-ref] [--cold-cache]
#
# Arguments:
#   path       - Where to create the worktree (default: .claude/worktrees/wt-<timestamp>)
#   branch     - Branch name for the worktree (default: wt-<basename of path>)
#   base-ref   - Git ref to branch from (default: current HEAD)
#   --cold-cache - Do NOT warm-start the object cache. Use for a guaranteed-clean A/B
#                  test or if a warm cache triggers a full rebuild on your setup.
#
# Examples:
#   scripts/setup_worktree.sh .claude/worktrees/my-feature my-feature
#   scripts/setup_worktree.sh .claude/worktrees/test test-branch dev
#   scripts/setup_worktree.sh                                # auto-generates path/branch
#   scripts/setup_worktree.sh .claude/worktrees/perf perf --cold-cache
#
# What gets shared, and WHY symlink vs reflink-copy per directory
# ----------------------------------------------------------------
# The rule: anything the BUILD WRITES TO must be a real (reflinked) copy, never
# a symlink into the main tree — a symlink would let this worktree's build
# corrupt the shared main build dir (catastrophic with a permuter/agent fleet
# running). Anything only READ can be a symlink (cheapest).
#
#   orig/                    reflink copy   read-only, but reflink is free on
#                                           CoW and avoids any symlink edge case
#   build/compilers/         symlink        read-only X360 toolchain
#   build/binutils/          symlink        read-only toolchain
#   build/45410914/          reflink copy   THE build dir. The `split` rule
#                                           regenerates config.json + obj/ INTO
#                                           this dir, and every compiled .obj
#                                           lands in src/ here. Must be a private
#                                           real copy. Reflinking it also
#                                           warm-starts the object cache for
#                                           fast incremental builds.
#
# Tools (dtk / objdiff-cli / wibo) are NOT taken from build/tools/ — rb3-xenon
# builds the jeff dtk fork from source via cargo, so build/tools/dtk can be a
# stale copy and a cargo build edge would re-fire the manifest-dirty loop in
# every worktree. Instead we point configure.py at the prebuilt sibling-repo
# binaries (../jeff, ../objdiff, ../wibo) — the same resolution the orchestrator
# worktree pool uses — so configure.py emits a binary reference with no cargo
# edge.
#
# After setup:
#   cd <worktree>
#   ./tools/ninja-locked build/45410914/src/system/flow/FlowCommand.obj
#   bin/objdiff-cli diff -u <unit> <symbol> --format json-pretty -o /dev/stdout
#
# Or via the MCP orchestrator:
#   run_objdiff(symbol, project_dir="<worktree>")
#
# Prerequisite: the main repo must have been built once (build/compilers/
# populated by configure.py's download step) and the sibling tool repos
# (../jeff, ../objdiff, ../wibo) must have been built.

set -euo pipefail

MAIN_REPO="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="45410914"

# ---- args (positional path / branch / base-ref + --cold-cache flag) ---------
POSITIONAL=()
WARM_CACHE=1
for arg in "$@"; do
    case "$arg" in
        --cold-cache) WARM_CACHE=0 ;;
        *) POSITIONAL+=("$arg") ;;
    esac
done

WORKTREE_PATH="${POSITIONAL[0]:-$MAIN_REPO/.claude/worktrees/wt-$(date +%s)}"
BRANCH="${POSITIONAL[1]:-wt-$(basename "$WORKTREE_PATH")}"
BASE_REF="${POSITIONAL[2]:-HEAD}"

# Resolve the base ref to a concrete commit for clarity
BASE_COMMIT="$(git -C "$MAIN_REPO" rev-parse --short "$BASE_REF" 2>/dev/null)" || {
    echo "ERROR: Cannot resolve ref '$BASE_REF'" >&2
    exit 1
}
BASE_BRANCH="$(git -C "$MAIN_REPO" rev-parse --abbrev-ref HEAD 2>/dev/null || echo "detached")"

# ---- tool sanity : prebuilt binaries from the sibling tool repos ------------
# rb3-xenon's canonical tools are the freeqaz forks built into sibling repos.
# (configure.py's own --dtk default is the ../jeff *source dir*, which would
# trigger a cargo build; passing the prebuilt binary instead avoids that.)
TOOL_DIR="$(cd "$MAIN_REPO/.." && pwd)"
DTK="$TOOL_DIR/jeff/target/release/dtk"
# Prefer the objdiff-cli that bin/objdiff-cli already points at (its symlink
# target is the canonical built fork); fall back to the sibling path.
OBJDIFF="$(readlink -f "$MAIN_REPO/bin/objdiff-cli" 2>/dev/null || echo "$TOOL_DIR/objdiff/target/release/objdiff-cli")"
WIBO="$TOOL_DIR/wibo/build/release/wibo"
COMPILERS="$MAIN_REPO/build/compilers"
for t in "$DTK" "$OBJDIFF" "$WIBO"; do
    [ -e "$t" ] || {
        echo "ERROR: required tool missing: $t" >&2
        echo "  (build the sibling tool repos: ../jeff, ../objdiff, ../wibo)" >&2
        exit 1
    }
done
[ -d "$COMPILERS" ] || {
    echo "ERROR: compilers dir missing: $COMPILERS" >&2
    echo "  (run configure.py + a full build in the main repo at least once first)" >&2
    exit 1
}

# ---- reflink helper ---------------------------------------------------------
# Reflink-copy a directory tree (CoW). Falls back to a normal copy if the
# filesystem doesn't support reflinks (cp --reflink=auto handles that
# transparently), but we warn so the operator knows the "instant + free"
# property was lost.
reflink_dir() {
    local src="$1" dst="$2"
    rm -rf "$dst"
    mkdir -p "$(dirname "$dst")"
    cp -a --reflink=auto "$src" "$dst"
}

# Warn if the destination isn't on a reflink-capable fs (script still works,
# just slow because cp falls back to full copies).
DEST_FSTYPE="$(findmnt -no FSTYPE --target "$(dirname "$WORKTREE_PATH")" 2>/dev/null || echo unknown)"
case "$DEST_FSTYPE" in
    btrfs|xfs|zfs) : ;;
    *) echo "WARN: $(dirname "$WORKTREE_PATH") is on '$DEST_FSTYPE'; reflinks may be unavailable — copies will be full (slow, space-hungry)." >&2 ;;
esac

# ---- worktree (idempotent) --------------------------------------------------
if [ -e "$WORKTREE_PATH/.git" ]; then
    echo "==> Worktree already exists at $WORKTREE_PATH (reconfiguring in place)"
else
    echo "==> Creating worktree at $WORKTREE_PATH"
    echo "    branch=$BRANCH  base=$BASE_REF ($BASE_COMMIT, on $BASE_BRANCH)"
    if git -C "$MAIN_REPO" show-ref --verify --quiet "refs/heads/$BRANCH"; then
        git -C "$MAIN_REPO" worktree add "$WORKTREE_PATH" "$BRANCH"
    else
        git -C "$MAIN_REPO" worktree add "$WORKTREE_PATH" -b "$BRANCH" "$BASE_REF"
    fi
fi

# ---- orig/ : reflink copy (read-only, but free on CoW) ----------------------
echo "==> orig/  (reflink copy — target binaries)"
reflink_dir "$MAIN_REPO/orig" "$WORKTREE_PATH/orig"

# ---- build/compilers, build/binutils : symlinks (read-only toolchain) -------
# NB: build/tools is deliberately NOT symlinked — tools come from the sibling
# repos (see tool sanity above), and symlinking build/tools would expose main's
# cargo target dir (build/tools/release) to a stray worktree write.
mkdir -p "$WORKTREE_PATH/build"
for d in compilers binutils; do
    if [ -e "$MAIN_REPO/build/$d" ]; then
        echo "==> build/$d  (symlink — read-only toolchain)"
        rm -rf "$WORKTREE_PATH/build/$d"
        ln -s "$MAIN_REPO/build/$d" "$WORKTREE_PATH/build/$d"
    fi
done

# ---- build/<VERSION>/ : reflink copy (build WRITES here; warm cache) --------
WT_BUILD="$WORKTREE_PATH/build/$VERSION"
if [ "$WARM_CACHE" -eq 1 ]; then
    echo "==> build/$VERSION/  (reflink copy — private build dir + WARM object cache)"
    reflink_dir "$MAIN_REPO/build/$VERSION" "$WT_BUILD"
else
    echo "==> build/$VERSION/  (cold: copying only obj/ + config.json, no object cache)"
    rm -rf "$WT_BUILD"
    mkdir -p "$WT_BUILD/pch"
    # obj/ (target objects from split) and config.json are inputs the build
    # needs even with a cold src/ cache. Reflink them so split/diff work.
    [ -d "$MAIN_REPO/build/$VERSION/obj" ] && reflink_dir "$MAIN_REPO/build/$VERSION/obj" "$WT_BUILD/obj"
    [ -f "$MAIN_REPO/build/$VERSION/config.json" ] && cp --reflink=auto "$MAIN_REPO/build/$VERSION/config.json" "$WT_BUILD/config.json"
    # Pre-create the PCH file. WIBO_FS_CACHE breaks *creating* new files in
    # case-insensitive path components (the 45410914 dir); cl.exe can overwrite
    # an existing file fine. The warm path inherits pch/ from the reflink, so
    # this only matters for cold-cache.
    touch "$WT_BUILD/pch/system.pch"
fi

# Drop stale ninja state copied from main (own lock + logs per build dir).
rm -f "$WORKTREE_PATH/.ninja_log" "$WORKTREE_PATH/.ninja_deps" \
      "$WORKTREE_PATH/.ninja_lock" "$WORKTREE_PATH/.ninja-build.lock" 2>/dev/null || true

# ---- clangd config + bin/objdiff-cli + venv : read-only symlinks ------------
echo "==> Symlinking clangd config"
[ -e "$MAIN_REPO/compile_commands.json" ] && ln -sfn "$MAIN_REPO/compile_commands.json" "$WORKTREE_PATH/compile_commands.json"
[ -e "$MAIN_REPO/.clangd" ] && ln -sfn "$MAIN_REPO/.clangd" "$WORKTREE_PATH/.clangd"

echo "==> Symlinking bin/objdiff-cli"
mkdir -p "$WORKTREE_PATH/bin"
ln -sfn "$MAIN_REPO/bin/objdiff-cli" "$WORKTREE_PATH/bin/objdiff-cli"

if [ -d "$MAIN_REPO/venv" ]; then
    echo "==> Symlinking Python venv"
    ln -sfn "$MAIN_REPO/venv" "$WORKTREE_PATH/venv"
fi

# ---- configure.py : bake absolute tool paths into this worktree's build.ninja
# rb3-xenon's configure.py takes --wrapper (NOT --wibo). Passing the prebuilt
# dtk binary avoids the cargo build edge (and the manifest-dirty loop).
echo "==> Running configure.py with absolute tool paths"
(
    cd "$WORKTREE_PATH"
    python3 configure.py \
        --dtk "$DTK" \
        --objdiff "$OBJDIFF" \
        --wrapper "$WIBO"
)

# ---- safety assertion : worktree build/ must be its own real dir ------------
if [ -L "$WT_BUILD" ]; then
    echo "FATAL: $WT_BUILD is a symlink — the build would corrupt the main tree. Aborting." >&2
    exit 1
fi

# ---- prune zero-byte orphan .obj files --------------------------------------
# The main build dir can accumulate zero-byte orphan objects with no ninja rule.
# Reflinking them into the worktree can confuse the post-build .obj patchers
# (they struct.unpack_from a COFF header that isn't there). Delete them here to
# keep the worktree clean and avoid the warning spam.
if [ -d "$WT_BUILD/src" ]; then
    pruned=$(find "$WT_BUILD/src" -name '*.obj' -type f -size 0 -print -delete 2>/dev/null | wc -l)
    [ "$pruned" -gt 0 ] && echo "==> Pruned $pruned zero-byte orphan .obj file(s)"
fi

# ---- prime ninja state : trigger SPLIT + configure.py regeneration ----------
# Without this, the worktree's first `ninja -t commands <obj>` query (used by
# the permuter, MCP orchestrator, and objdiff scripts) can return commands
# derived from a not-yet-fully-consistent build.ninja, leading to baseline
# match% returning 0.00% on the first invocation of every function in the unit.
# Running ninja once here re-runs SPLIT (regenerates config.json from config.yml)
# and the configure.py edge inside build.ninja, leaving the build graph fully
# consistent. With the warm reflinked object cache, this is a near-no-op rebuild
# but updates `.ninja_log` and `.ninja_deps` so subsequent queries are
# deterministic. Use ninja-locked (per CLAUDE.md, never bare ninja) so the
# worktree's own .ninja-build.lock serializes concurrent builds.
#
# NOTE (rb3-xenon divergence from DC3): a failed prime is a loud WARNING, not
# fatal. The main tree is frequently mid-repair (the whole point of spinning up
# a worktree is often to FIX the broken build), so a prime failure must not
# block worktree creation. The full error is printed so the operator sees it,
# but the worktree is left fully configured and usable.
echo "==> Priming ninja state (regenerates config.json + warms .ninja_log)"
(
    cd "$WORKTREE_PATH"
    NINJA="./tools/ninja-locked"
    [ -x "$NINJA" ] || NINJA="ninja"
    prime_log="$(mktemp)"
    if "$NINJA" >"$prime_log" 2>&1; then
        tail -5 "$prime_log"
    else
        echo "WARN: ninja prime failed (the main tree may not build yet)." >&2
        echo "      The worktree is still configured and usable; fix the build inside it." >&2
        echo "      ---- prime output ----" >&2
        cat "$prime_log" >&2
        echo "      ----------------------" >&2
    fi
    rm -f "$prime_log"
)

echo ""
echo "Worktree ready:  $WORKTREE_PATH"
echo "  branch:        $BRANCH  (from $BASE_COMMIT on $BASE_BRANCH)"
echo ""
echo "Next:"
echo "  cd $WORKTREE_PATH"
echo "  ./tools/ninja-locked build/$VERSION/src/<File>.obj   # warm cache = fast"
echo ""
echo "Usage with MCP orchestrator:"
echo "  run_objdiff(symbol, project_dir=\"$WORKTREE_PATH\")"
echo ""
echo "Remove when done:  git -C $MAIN_REPO worktree remove --force $WORKTREE_PATH"
