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
    local src="$1" dst="$2" tries="${3:-4}" i
    mkdir -p "$(dirname "$dst")"
    # Retry transient failures: when $src is a shared build dir being written by
    # concurrent builds, `cp -a` can abort with "file changed as we read it" or a
    # vanished temp .obj ("cannot stat ...: No such file or directory"). A retry
    # after a short pause usually lands in a quiet window.
    for ((i=1; i<=tries; i++)); do
        rm -rf "$dst"
        if cp -a --reflink=auto "$src" "$dst" 2>/dev/null; then
            return 0
        fi
        sleep $((i))
    done
    return 1
}

# Best-effort reflink for a REGENERABLE cache (the build dir): tolerate a partial
# copy — a few temp objs that vanished mid-copy under concurrent writes just get
# recompiled by ninja. Fails only if the copy produced nothing.
reflink_dir_besteffort() {
    local src="$1" dst="$2" i
    mkdir -p "$(dirname "$dst")"
    rm -rf "$dst"
    for i in 1 2 3 4; do
        if cp -a --reflink=auto "$src" "$dst" 2>/dev/null; then return 0; fi
        # partial copy left in place is fine; retry to fill in more, then accept
        sleep "$i"
    done
    # accept a partial cache as long as SOMETHING copied
    [ -d "$dst" ] && return 0
    return 1
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
    reflink_dir_besteffort "$MAIN_REPO/build/$VERSION" "$WT_BUILD"
    # Critical build inputs must survive the (possibly partial) copy. If a temp
    # obj vanished mid-copy that's fine (ninja recompiles), but obj/ (target
    # split objects) and config.json are required — reflink them individually if
    # the best-effort pass dropped them.
    [ -d "$WT_BUILD/obj" ] || reflink_dir "$MAIN_REPO/build/$VERSION/obj" "$WT_BUILD/obj"
    [ -f "$WT_BUILD/config.json" ] || cp --reflink=auto "$MAIN_REPO/build/$VERSION/config.json" "$WT_BUILD/config.json" 2>/dev/null || true
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

# ---- warm-cache validation : make the reflinked object cache VALID under ninja
# `git worktree add` stamps every checked-out source with a fresh (now) mtime,
# and the toolchain symlink build/compilers resolves to main's dir (recent
# mtime, a shared implicit input to every compile). Ninja is mtime-only (no
# content hashing), so it treats every reflinked object as stale vs those
# inputs and does a FULL REBUILD in the priming step below — 10+ worktrees per
# workflow => 10+ redundant full rebuilds + machine saturation.
#
# A fresh worktree is byte-identical to its base ($BASE_REF) — that's exactly
# what produced the reflinked objects — so the whole reflinked cache IS current.
# Bump every build output newer than all of its inputs (sources, headers,
# toolchain) so ninja sees the cache as up-to-date and the prime (and the
# agent's first build) become no-ops; a later source EDIT gets a newer mtime and
# rebuilds normally. If the worktree DIFFERS from its base (branch reuse, or main
# has local mods) we skip this and let ninja rebuild — correctness over speed.
if [ "$WARM_CACHE" -eq 1 ]; then
    # Only BUILD INPUTS matter for cache validity — a compiled source/header
    # (src/) or a split/objects/symbols config (config/). Dirty scripts, docs,
    # or tooling in main don't make any reflinked object stale, so exclude them.
    # NB: `grep -c` exits 1 when the count is 0 -- WITHOUT the `|| true` that
    # non-zero exit propagates through the command substitution and (under
    # `set -e`) aborts the whole script whenever main has no dirty src/config
    # files (e.g. only docs/tooling changed). The `|| true` keeps the count (0).
    _changed="$( { git -C "$MAIN_REPO" diff --name-only 2>/dev/null;
                   git -C "$MAIN_REPO" diff --name-only --cached 2>/dev/null;
                   git -C "$WORKTREE_PATH" diff --name-only "$BASE_REF" 2>/dev/null; } \
                 | grep -cE '^(src/|config/)' || true )"
    if [ "$_changed" -eq 0 ]; then
        echo "==> Validating warm object cache (worktree == $BASE_REF; marking outputs current)"
        find "$WT_BUILD" -type f -exec touch {} + 2>/dev/null || true
        echo "  reflinked cache marked current — prime + agent's first build are no-ops"
    else
        echo "==> Warm cache NOT validated: worktree differs from $BASE_REF ($_changed path(s)); first build will rebuild"
    fi
fi

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

# ---- gitignored analysis inputs : reflink-copy so agents don't have to hand-copy
# These files live at the repo root and are gitignored (they are large or
# machine-generated). Three consecutive batch-2 wave agents had to cp them
# manually; we automate it here.  Each copy is non-fatal: if the source
# doesn't exist yet the worktree is still usable (agents that don't need the
# file won't hit the gap).
#
# Source resolution: prefer MAIN_REPO (= the repo containing this script).
# Fallback: if MAIN_REPO is itself a worktree (e.g. wt-infra), scan
# `git worktree list` for the primary tree (first entry = [main] branch or
# whichever tree registered the worktree) so the files are always found
# regardless of which worktree the caller is in.
PRIMARY_REPO="$MAIN_REPO"
if ! git -C "$MAIN_REPO" rev-parse --git-common-dir >/dev/null 2>&1; then
    true  # no-op; MAIN_REPO is still the best we have
else
    # git --git-common-dir points to the shared .git in the main worktree.
    # git worktree list's first line is always the main (linked) worktree path.
    # NB: awk must NOT `exit` on the first match — under `set -o pipefail` an early
    # exit closes the pipe while `git` is still writing (a race that only trips
    # under load / many worktrees), so `git` dies with SIGPIPE(141) and `set -e`
    # aborts the whole script right before configure.py + prime (leaving an
    # unbuildable worktree with no build.ninja). Print the first match but keep
    # reading to EOF so `git` always finishes cleanly.
    _primary="$(git -C "$MAIN_REPO" worktree list --porcelain 2>/dev/null \
                | awk '/^worktree / {if (!seen++) print $2}')"
    [ -n "$_primary" ] && [ -d "$_primary" ] && PRIMARY_REPO="$_primary"
fi

echo "==> Copying gitignored analysis inputs (non-fatal if absent)"
for analysis_file in \
        global_fuzzy_pairs.json \
        unified_id_rb3wii.json \
        struct_db.sqlite; do
    # Try MAIN_REPO first, then PRIMARY_REPO fallback.
    src="$MAIN_REPO/$analysis_file"
    [ -e "$src" ] || src="$PRIMARY_REPO/$analysis_file"
    dst="$WORKTREE_PATH/$analysis_file"
    if [ -e "$src" ]; then
        cp --reflink=auto "$src" "$dst" 2>/dev/null \
            && echo "  copied $analysis_file (from $(dirname "$src"))" \
            || echo "  WARN: could not copy $analysis_file (non-fatal)" >&2
    else
        echo "  skip $analysis_file (not present in main repo yet)"
    fi
done

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

# ---- neutralize the build/compilers download edge (offline worktree) --------
# build.ninja has a `build/compilers: download_tool | tools/download_tool.py`
# edge that (re)downloads the MSVC X360 toolchain from files.decomp.dev. In a
# fresh worktree ninja has no .ninja_log entry for it, so it ALWAYS re-runs the
# edge (this is a missing-log-entry issue, not mtime — touching does not help).
# But the toolchain is already symlinked above (build/compilers -> main), and
# this environment has no cert path to the download host (the request dies with
# `SSL: CERTIFICATE_VERIFY_FAILED`, and download_tool.py's certifi retry also
# fails). Result without this step: the prime below and EVERY subsequent build
# fail at the download edge, silently blocking all matching work in the worktree
# (stale report.json => false NET +0). Fix: patch the worktree's copy of
# download_tool.py to no-op when the requested output already exists (which it
# always does here — it is symlinked). This dirties the worktree's
# download_tool.py; that is expected and the land/verify-stage tooling already
# excludes it from commits.
DL_TOOL="$WORKTREE_PATH/tools/download_tool.py"
if [ -f "$DL_TOOL" ]; then
    python3 - "$DL_TOOL" <<'PYEOF'
import sys
p = sys.argv[1]
s = open(p).read()
if "already present; skipping download" in s:
    sys.exit(0)  # already patched (idempotent)
old = "    url = TOOLS[args.tool](args.tag)\n    output = Path(args.output)\n"
new = (
    "    output = Path(args.output)\n"
    "    # setup_worktree offline short-circuit: skip the network download when the\n"
    "    # tool is already present (e.g. build/compilers symlinked from the main\n"
    "    # tree). This env has no cert path to the download host, and re-downloading\n"
    "    # an already-present toolchain is unnecessary.\n"
    "    if output.exists() and (not output.is_dir() or any(output.iterdir())):\n"
    "        print(f\"{output} already present; skipping download\")\n"
    "        return\n"
    "    url = TOOLS[args.tool](args.tag)\n"
)
if old in s:
    open(p, "w").write(s.replace(old, new, 1))
    print("==> Neutralized build/compilers download edge (download_tool.py offline short-circuit)")
else:
    sys.stderr.write("WARN: could not patch download_tool.py (unexpected layout); "
                     "worktree builds may fail at the build/compilers download edge.\n")
PYEOF
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
