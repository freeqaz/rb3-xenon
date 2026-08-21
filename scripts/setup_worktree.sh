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
#   ./tools/ninja-locked          # NO target -- see below
#   bin/objdiff-cli diff -u <unit> <symbol> --format json-pretty -o /dev/stdout
#
# ⛔ Do NOT name a single .obj as the ninja target, and do NOT pass
# `objdiff-cli --build` (which is the same thing: `ninja <base .obj>`). The six
# post-compile patchers hang off the `post-compile` phony, DOWNSTREAM of the
# compile edges, so a single-object build stops one edge short of all of them
# and the fresh compile overwrites the previously-patched bytes -- answering
# from raw compiler output and leaving the object that way for report.json and
# every concurrent lane. Measured on rb3-xenon 2026-08-21: one such build cost
# unit default/BandUI 2.006 pp of matched_code_percent and read
# ?InitPanels@BandUI@@QAAXXZ as 99.7 when it is 100.0. A bare
# `./tools/ninja-locked` on a warm worktree is a no-op in ~24 s, so the advice
# this block used to give bought nothing and cost a correct measurement.
# `python3 scripts/verify_objs_patched.py --verify-manifest` answers whether a
# tree is currently trustworthy.
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
    # Bring main's object cache up to date BEFORE snapshotting it. ff-merge
    # landings advance main's HEAD but do NOT rebuild, so the shared cache goes
    # stale and every worktree would otherwise recompile the newly-landed TUs
    # (cold-cache contention — builds crawl). `ninja-locked all_source` builds
    # the objects only (skips the slow report regen), serializes across
    # concurrent worktree creations, and is a NO-OP once main is current — so
    # only the FIRST creation after a landing pays the small incremental rebuild
    # and every other worktree reflinks an already-warm cache for free. Non-fatal:
    # main may be mid-repair (a common reason to spin up a worktree).
    #
    # ⚠ THIS BUILDS IN THE SHARED MAIN TREE, AND A BUILD RUNS THE SPLIT.
    # `ninja all_source` cannot avoid it: `build/$VERSION/config.json` is an
    # input to the build.ninja generator edge, so ninja brings config.json (=
    # the `split` rule) up to date before ANY target. The split's depfile inputs
    # are orig/default.xex + config/$VERSION/splits.txt + config/$VERSION/
    # symbols.txt — two of which the split itself REWRITES (jeff split.rs
    # split_pdata clears the whole .pdata split set and re-derives one range per
    # .text block). So whenever main's splits.txt carries .text edits whose
    # .pdata has not been derived yet — the normal state right after a
    # splits-touching patch is landed, since lanes land by patch and never
    # rebuild main — this line silently rewrites a TRACKED file in the shared
    # tree, out from under whatever agent owns it.
    #
    # MEASURED (lane CF-9, 2026-08-01):
    #   * settled main: the split rewrites NOTHING — splits.txt/symbols.txt keep
    #     their exact mtimes, git status stays clean. Two worktree creations on a
    #     clean main left main clean. So this is NOT an every-dispatch bug.
    #   * unsettled: after removing 12 .text blocks, one build rewrote
    #     splits.txt (sha1 ee47802b -> 9fbd4133, 10 derived .pdata lines).
    #     Non-idempotency confirmed.
    # Consequence: main goes dirty under config/, which trips the `_changed`
    # gate below, so EVERY worktree created afterwards silently falls back to a
    # full ~1095-object rebuild instead of a 0-compile no-op — and slower
    # worktrees push the fleet toward its <=4-concurrent-build cap.
    #
    # Fix, in two parts, both scoped as narrowly as possible because writing to
    # a shared tree is the hazard we are removing, not one to add more of:
    #   (1) PRE-FLIGHT: if main already has uncommitted config/ changes, some
    #       agent owns that file — do not build in their tree at all. Skipping
    #       costs nothing we had: a dirty config/ already fails the seeding gate,
    #       so those worktrees were going to rebuild either way.
    #   (2) POST-FLIGHT: otherwise config/ was CLEAN when we started, so any
    #       config/ change now is provably OUR side effect and restoring it to
    #       HEAD cannot destroy anyone's work. We still re-verify the file is
    #       byte-identical to what our own build produced before restoring, so a
    #       third party writing inside the window is left alone.
    # RB3_WT_ALLOW_MAIN_MUTATION=1 opts out of the restore (debugging only).
    _refresh_skip=""
    if ! git -C "$MAIN_REPO" diff --quiet -- config/ 2>/dev/null \
       || ! git -C "$MAIN_REPO" diff --quiet --cached -- config/ 2>/dev/null; then
        _refresh_skip="main has uncommitted config/ changes (another agent owns them)"
    fi
    if [ -n "$_refresh_skip" ]; then
        echo "==> SKIPPING main object-cache refresh: $_refresh_skip"
        echo "    (refusing to run the split in the shared tree; worktree rebuilds what's stale)"
    else
        # config/ is clean here, so record HEAD-state hashes of exactly the two
        # tracked files the split rewrites.
        _split_outputs="config/$VERSION/splits.txt config/$VERSION/symbols.txt"
        _pre_hashes="$( cd "$MAIN_REPO" && sha1sum $_split_outputs 2>/dev/null || true )"

        echo "==> Refreshing main's object cache (amortized; no-op if already current)"
        ( cd "$MAIN_REPO" && ./tools/ninja-locked all_source ) >/dev/null 2>&1 \
            || echo "  WARN: main cache refresh failed (non-fatal; worktree will rebuild what's stale)" >&2

        _post_hashes="$( cd "$MAIN_REPO" && sha1sum $_split_outputs 2>/dev/null || true )"
        if [ "$_pre_hashes" != "$_post_hashes" ]; then
            if [ "${RB3_WT_ALLOW_MAIN_MUTATION:-0}" -eq 1 ]; then
                echo "  WARN: the split rewrote main's $_split_outputs; leaving it dirty" >&2
                echo "        (RB3_WT_ALLOW_MAIN_MUTATION=1). Subsequent worktrees will NOT be seeded." >&2
            else
                for _f in $_split_outputs; do
                    # Restore ONLY files that (a) we saw clean, (b) actually changed,
                    # and (c) still hold exactly the bytes our build wrote — so a
                    # concurrent writer in the window is never clobbered.
                    _now="$( cd "$MAIN_REPO" && sha1sum "$_f" 2>/dev/null || true )"
                    case "$_post_hashes" in
                        *"$_now"*) ;;
                        *) echo "  WARN: $_f changed again after our build — leaving it alone" >&2
                           continue ;;
                    esac
                    case "$_pre_hashes" in
                        *"$_now"*) continue ;;   # unchanged by us
                    esac
                    git -C "$MAIN_REPO" checkout -- "$_f" 2>/dev/null \
                        && echo "  restored main's $_f (split-derived churn undone; main stays clean)" \
                        || echo "  WARN: could not restore main's $_f" >&2
                done
            fi
        fi
    fi

    echo "==> build/$VERSION/  (reflink copy — private build dir + WARM object cache)"
    reflink_dir_besteffort "$MAIN_REPO/build/$VERSION" "$WT_BUILD"
    # Critical build inputs must survive the (possibly partial) copy. If a temp
    # obj vanished mid-copy that's fine (ninja recompiles), but obj/ (target
    # split objects) and config.json are required — reflink them individually if
    # the best-effort pass dropped them.
    [ -d "$WT_BUILD/obj" ] || reflink_dir "$MAIN_REPO/build/$VERSION/obj" "$WT_BUILD/obj"
    [ -f "$WT_BUILD/config.json" ] || cp --reflink=auto "$MAIN_REPO/build/$VERSION/config.json" "$WT_BUILD/config.json" 2>/dev/null || true

    # ---- PCH: drop main's reflinked .pch — it bakes MAIN's absolute header paths.
    # The reflinked build/$VERSION/pch/system.pch was created in the MAIN tree and
    # embeds main's absolute include paths as the #pragma-once identity of every
    # header it precompiled. This is INVISIBLE while every PCH-eligible TU is served
    # from objcache, but the moment an engine-header edit forces a REAL /Yu recompile
    # of an eligible TU (dirs: hamobj synth flow gesture meta obj os utl movie),
    # cl.exe consumes this stale .pch and resolves the SAME logical header at BOTH
    # main and worktree paths, so `#pragma once` fails to dedupe -> mass
    # Symbol/Str/File redefinition errors across ~34 TUs, corrupting measurement for
    # every agent that edits a shared engine header.
    #
    # Fix: force ninja to rebuild the PCH IN THIS WORKTREE on first need (baking THIS
    # tree's paths) by deleting the PCH edge's object output (a missing output makes
    # the edge dirty regardless of the seeded .ninja_log). This is cheap and does NOT
    # bust the warm object cache: objcache keys /Yu TUs on a ROOT-INDEPENDENT
    # pch-source-closure digest (objcache key.rs closure_digest — content + repo-root-
    # relative path, NOT the .pch bytes), so eligible TUs stay cache HITS after the
    # rebuild. Cost is a single decomp_pch.cpp compile on the first build that needs
    # the PCH; the dominant GAME workflow (band3/network are NON-eligible dirs) never
    # enters the PCH graph and stays a true 0-compile no-op. The empty placeholder
    # keeps cl.exe OVERWRITING system.pch rather than CREATING it (WIBO_FS_CACHE can't
    # create a new file under the case-insensitive VERSION dir; overwrite is fine).
    if [ -d "$WT_BUILD/pch" ]; then
        rm -f "$WT_BUILD/pch/decomp_pch.obj"
        : > "$WT_BUILD/pch/system.pch"
        echo "==> PCH reset (dropped main-path system.pch/decomp_pch.obj; worktree rebuilds on first need)"
    fi
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

# Drop stale ninja LOCK files copied from main (own lock per build dir).
# NOTE: .ninja_log/.ninja_deps are deliberately NOT removed here — they may be
# seeded from main's warm state AFTER configure.py has produced this worktree's
# build.ninja (see the "seed warm ninja state" block below, which can only run
# post-configure because it gates on msvc-rule parity). If any seeding gate
# fails, that block removes them, preserving the old rm-and-rebuild behavior.
rm -f "$WORKTREE_PATH/.ninja_lock" "$WORKTREE_PATH/.ninja-build.lock" 2>/dev/null || true

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
# toolchain) so ninja sees the cache as up-to-date by MTIME; a later source EDIT
# gets a newer mtime and rebuilds normally. If the worktree DIFFERS from its base
# (branch reuse, or main has local mods) we skip this and let ninja rebuild —
# correctness over speed.
#
# SCOPE OF THIS OPTIMIZATION (measured — see worktree-build-tooling-findings):
# The touch makes INCREMENTAL builds warm — a single-obj `ninja <X>.obj` rebuilds
# only X (its deps log is missing, ~1 recompile) and everything else stays cached.
# It does NOT make a full `ninja` (report.json) a no-op: the worktree's compile
# commands carry ABSOLUTE tool paths (configure.py bakes --dtk/--objdiff/--wrapper
# to dodge cargo/download edges) whose command-hash differs from main's
# relative-path .ninja_log, so a full build recompiles all ~727 objs once. That is
# why the prime below is scoped to config.json rather than the bare-ninja default.
if [ "$WARM_CACHE" -eq 1 ]; then
    # Only BUILD INPUTS matter for cache validity — a compiled source/header
    # (src/) or a split/objects/symbols config (config/). Dirty scripts, docs,
    # or tooling in main don't make any reflinked object stale, so exclude them.
    # NB: `grep -c` exits 1 when the count is 0 -- WITHOUT the `|| true` that
    # non-zero exit propagates through the command substitution and (under
    # `set -e`) aborts the whole script whenever main has no dirty src/config
    # files (e.g. only docs/tooling changed). The `|| true` keeps the count (0).
    # The reflinked objects were produced by MAIN at MAIN's HEAD, so the cache
    # is only current for a worktree whose base is that same commit. Comparing
    # the worktree against its OWN base ref is trivially zero and proves
    # nothing — it is the same tree by construction. Basing a worktree on an
    # older ref (a pinned eval substrate, a bisect point) and marking main's
    # newer objects "current" makes ninja skip every stale unit SILENTLY: a
    # scoring run then grades against wrong-warm baselines and prints "no work
    # to do", which is exactly what a correct warm cache prints. Measured on
    # dc3-decomp 2026-08-05 at 286 commits behind main — 7 of the 123 source
    # files a 225-function eval roster spans had changed, and the tree reported
    # no work to do (dc3-decomp `154c365b`). Include main's HEAD-vs-base delta
    # in the same count. No-op when BASE_REF is HEAD, which is the default.
    _changed="$( { git -C "$MAIN_REPO" diff --name-only 2>/dev/null;
                   git -C "$MAIN_REPO" diff --name-only --cached 2>/dev/null;
                   git -C "$WORKTREE_PATH" diff --name-only "$BASE_REF" 2>/dev/null;
                   git -C "$MAIN_REPO" diff --name-only "$BASE_REF" HEAD 2>/dev/null; } \
                 | grep -cE '^(src/|config/)' || true )"
    if [ "$_changed" -eq 0 ]; then
        echo "==> Validating warm object cache (worktree == $BASE_REF; marking outputs current)"
        # Set every tracked source OLDER than the reflinked outputs. The output
        # touch below can't reach tracked INPUTS to non-object targets — notably
        # tools/download_tool.py, which feeds the build/compilers rule. A fresh
        # `git worktree add` stamps sources "now", so download_tool.py ends up
        # newer than the build/compilers toolchain => ninja marks the TOOLCHAIN
        # dirty and cascades a full recompile even for single-obj builds. A fixed
        # old timestamp (not main's, which may itself be recent) guarantees the
        # toolchain isn't rebuilt; a later source EDIT still gets a fresh mtime and
        # rebuilds normally.
        # NB: run FROM the worktree — `git ls-files` prints worktree-relative
        # paths, so touch must resolve them against the worktree, not the setup
        # script's CWD (main), or it silently touches main's copies instead.
        ( cd "$WORKTREE_PATH" && git ls-files -z 2>/dev/null \
            | xargs -0 -r touch -h -d '2020-01-01' 2>/dev/null ) || true
        # DO NOT bump the reflinked build outputs to "now". `cp -a --reflink`
        # already preserved main's exact output mtimes, which is precisely what
        # makes them (a) newer than the 2020-stamped sources above and (b) EQUAL
        # to the per-output mtimes recorded in main's .ninja_deps. The seeding
        # block below copies main's .ninja_deps verbatim, and ninja invalidates
        # a stored dep set when `output->mtime() > deps->mtime` — so touching the
        # objs to "now" (as this step used to) makes every output newer than its
        # recorded deps mtime, marking ALL ~745 edges "stored deps out of date"
        # and re-running them on the first full build (they hit objcache, so it's
        # fast, but it is NOT the 0-compile no-op seeding is meant to produce).
        # In the NON-seeded warm path there is no .ninja_log, so ninja rebuilds a
        # requested obj via "command not in log" regardless of its mtime — i.e.
        # the old "now" touch was a no-op there anyway. Leaving reflink mtimes
        # intact is correct for both paths.
        echo "  reflinked cache marked current (reflink mtimes preserved — seed-compatible)"
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
# These files are gitignored (they are large or machine-generated) and live at the
# repo root or under config/.  Three consecutive batch-2 wave agents had to cp them
# manually; we automate it here.  Each copy is non-fatal: if the source
# doesn't exist yet the worktree is still usable (agents that don't need the
# file won't hit the gap).
#
# config/45410914/scope_map.json is the load-bearing one: WITHOUT it every build's
# decomp dashboard (tools/scope_map.py priority) loses the classification for ~65k
# anonymous fn_8XXXXXXX functions, the per-tier DENOMINATORS collapse to pinned-only
# coverage, and the worktree silently reports INFLATED tier percentages that are not
# comparable to main's.  (scope_map.py now also shouts when it is absent.)
#
# fingerprints.json (15.9 MB, reflink => free on btrfs) is here because its
# absence used to make a CONTROL vacuous rather than merely degrade an output:
# tools/pdata_map_audit.py's `--selftest --sabotage shift` leg -- the anti-vacuity
# check for that whole audit -- [SKIP]ped its only discriminating comparison and
# then printed OK, so the sabotage passed in every worktree (lane P, 2026-08-16).
# That tool no longer DEPENDS on this file (its discriminating controls are now
# intrinsic to the binary, and it exits 2 + prints INCOMPLETE when this is
# missing), but carrying it restores full coverage for free.
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

# ⛔ DO NOT re-add `unified_id_rb3wii.json` or `global_fuzzy_pairs.json` here.
# Both are TU0-era address indices (built May/Jun 2026, BEFORE the 2026-07-15
# TU5 flip). Their rb3-side addresses are keyed to a DIFFERENT build of the
# binary and now carry ZERO signal: measured against TU5's 69,209 `.text`
# function starts they resolve at 4.27% / 2.15%, versus a random-offset null of
# ~2.9% / ~3.9% — i.e. indistinguishable from noise, and for several shift
# offsets an ARBITRARY shift scores HIGHER than the true one. (Positive control:
# the live `scripts/target_symbol_map.json` resolves at 99.79% on the same test,
# so the test does detect a live index.) Audit any candidate with
# `tools/index_liveness_audit.py` before adding it.
#
# Copying them fleet-wide was actively harmful: they parse fine, so consumers
# silently degrade instead of failing, and lane BW-2 lost time trying to use the
# oracle as an attribution instrument before discovering it was dead. Neither is
# a build input (0 references in build.ninja / configure.py / tools/project.py),
# so dropping them cannot affect any build. Leaving them absent also makes
# objdiff's `--global-byte-eq` fail LOUDLY in a worktree (its oracle loader
# hard-fails on a missing file) rather than silently promoting nothing.
echo "==> Copying gitignored analysis inputs (non-fatal if absent)"
for analysis_file in \
        struct_db.sqlite \
        fingerprints.json \
        config/45410914/scope_map.json; do
    # Try MAIN_REPO first, then PRIMARY_REPO fallback.
    src="$MAIN_REPO/$analysis_file"
    [ -e "$src" ] || src="$PRIMARY_REPO/$analysis_file"
    dst="$WORKTREE_PATH/$analysis_file"
    # entries may be repo-root-relative PATHS, not just basenames
    mkdir -p "$(dirname "$dst")" 2>/dev/null || true
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

# ---- safety assertion : configure.py must have produced build.ninja ---------
# A SIGPIPE-class abort (or any silent configure failure) that leaves no
# build.ninja produces an UNBUILDABLE worktree: every build dies with
# `ninja: error: loading 'build.ninja'`, workflows spin on the lock, and stale
# reports read as false NET +0. Fail LOUD here instead of downstream. (The
# git-worktree-list SIGPIPE that caused exactly this was fixed in 8d8d257; this
# assertion is the backstop so the failure mode can never again be silent.)
if [ ! -f "$WORKTREE_PATH/build.ninja" ]; then
    echo "FATAL: configure.py did not produce $WORKTREE_PATH/build.ninja." >&2
    echo "       The worktree is unbuildable; refusing to hand back a broken tree." >&2
    exit 1
fi

# ---- seed warm ninja state (.ninja_log/.ninja_deps) from main ---------------
# A fresh worktree has NO ninja log, so ninja treats every output as never-built
# and a full `ninja` recompiles all ~745 objs even though the reflinked object
# cache is byte-valid. When ALL of the following hold, main's .ninja_log +
# .ninja_deps are a CORRECT warm state for this worktree, so seeding them makes
# the first full build a ~0-compile no-op:
#   a) WARM_CACHE==1 AND main/worktree is clean (no src/|config/ diffs — the
#      existing `_changed` gate) AND main has no uncommitted configure.py /
#      tools/project.py diffs (a dirty wiring file means main's .ninja_log may
#      encode commands that differ from what this worktree's regenerated
#      build.ninja would run);
#   b) rule parity: the msvc/msvc_pch/msvc_pch_create rule blocks are
#      byte-identical between main and worktree build.ninja (same absolute tool
#      paths => identical command strings => identical command hashes);
#   c) main's deps are uniformly repo-root-relative: `ninja -t deps` on 3 probe
#      objs (incl. PCH-eligible units) yields ZERO absolute (`^    /`) dep lines
#      — an absolute dep would pin the copied .ninja_deps to MAIN's files, so a
#      worktree header edit to those paths would be silently missed;
#   d) main's .ninja_log and .ninja_deps both exist and are non-empty.
# Any gate failing => remove the two files (old rm-and-rebuild behavior) and
# print EXACTLY which gate failed (observability > silent fallback). The
# --cold-cache path (WARM_CACHE==0) always lands in the else branch, so cold
# baselines stay honestly cold.
_seed_ok=1
_seed_reason=""
if [ "$WARM_CACHE" -ne 1 ]; then
    _seed_ok=0
    _seed_reason="cold-cache (--cold-cache): warm seeding disabled by design"
elif [ "${_changed:-1}" -ne 0 ]; then
    _seed_ok=0
    _seed_reason="main/worktree has src/|config/ diffs vs $BASE_REF (_changed=${_changed:-unset})"
else
    # Gate a (wiring): no uncommitted configure.py / tools/project.py diffs in main.
    _wiring_changed="$( { git -C "$MAIN_REPO" diff --name-only 2>/dev/null;
                          git -C "$MAIN_REPO" diff --name-only --cached 2>/dev/null; } \
                        | grep -cE '^(configure\.py|tools/project\.py)$' || true )"
    if [ "$_wiring_changed" -ne 0 ]; then
        _seed_ok=0
        _seed_reason="main has uncommitted configure.py/tools/project.py diffs ($_wiring_changed) — wiring may differ"
    fi
fi

# Gate b (rule parity): extract the msvc rule blocks from both build.ninja files
# (each block runs from its `rule <name>` header through the blank line that
# terminates it) and require them byte-identical.
if [ "$_seed_ok" -eq 1 ]; then
    _rule_awk='/^rule (msvc|msvc_pch|msvc_pch_create)$/{p=1} p{print} p&&/^$/{p=0}'
    if ! cmp -s \
         <(awk "$_rule_awk" "$MAIN_REPO/build.ninja") \
         <(awk "$_rule_awk" "$WORKTREE_PATH/build.ninja"); then
        _seed_ok=0
        _seed_reason="msvc rule block(s) differ between main and worktree build.ninja"
    fi
fi

# Gate c (relative deps): 3 probe objs incl. PCH-eligible units must have zero
# absolute src dep paths in main's .ninja_deps.
if [ "$_seed_ok" -eq 1 ]; then
    _abs_deps=0
    for _probe in \
        "build/$VERSION/src/system/beatmatch/MasterAudio.obj" \
        "build/$VERSION/src/system/os/Debug.obj" \
        "build/$VERSION/src/system/obj/Object.obj"; do
        _n="$( ( cd "$MAIN_REPO" && ninja -t deps "$_probe" 2>/dev/null ) \
               | grep -c '^    /' || true )"
        _abs_deps=$(( _abs_deps + _n ))
    done
    if [ "$_abs_deps" -ne 0 ]; then
        _seed_ok=0
        _seed_reason="main's .ninja_deps has $_abs_deps absolute src dep path(s) — not portable to a worktree"
    fi
fi

# Gate d (non-empty state): main's .ninja_log and .ninja_deps exist + non-empty.
if [ "$_seed_ok" -eq 1 ]; then
    if [ ! -s "$MAIN_REPO/.ninja_log" ] || [ ! -s "$MAIN_REPO/.ninja_deps" ]; then
        _seed_ok=0
        _seed_reason="main's .ninja_log/.ninja_deps missing or empty"
    fi
fi

if [ "$_seed_ok" -eq 1 ]; then
    cp --reflink=auto "$MAIN_REPO/.ninja_log"  "$WORKTREE_PATH/.ninja_log"
    cp --reflink=auto "$MAIN_REPO/.ninja_deps" "$WORKTREE_PATH/.ninja_deps"
    echo "==> Seeded warm ninja state from main (.ninja_log + .ninja_deps) — first full build should be ~0 compiles"
else
    rm -f "$WORKTREE_PATH/.ninja_log" "$WORKTREE_PATH/.ninja_deps" 2>/dev/null || true
    echo "==> Warm ninja state NOT seeded: $_seed_reason (first build will rebuild)"
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
    # The patch rewrite above stamps download_tool.py "now" — NEWER than the
    # build/compilers output (a symlink whose TARGET, main's toolchain dir, ninja
    # stats). `rule download_tool` has NO restat, and build/compilers is an
    # implicit input to all ~727 compile edges, so a perpetually-newer input
    # means: every ninja run re-fires the (no-op'd) download edge, ninja assumes
    # its output changed, and EVERY downstream compile requested in that run goes
    # dirty — repeated full builds re-pay the 727-obj tax and even repeated
    # single-obj builds recompile every time (the measured 3-10s "warm" builds).
    # Old-stamping the patched file makes the edge mtime-clean after its first
    # (unavoidable, missing-log-entry) run, so the cascade stops permanently.
    touch -d '2020-01-01' "$DL_TOOL" 2>/dev/null || true
    # The patch dirties a TRACKED file in the worktree, and agents doing
    # `git commit -a` / `git add -A` then LEAK it into their branches (two
    # wave-3 lanes were rejected on exactly this). assume-unchanged hides the
    # local patch from status/diff/commit machinery so branch diffs stay clean;
    # the working-copy patch still applies for builds.
    git -C "$WORKTREE_PATH" update-index --assume-unchanged tools/download_tool.py 2>/dev/null || true
fi

# ---- prime ninja state : trigger SPLIT + configure.py regeneration ----------
# Without this, the worktree's first `ninja -t commands <obj>` query (used by
# the permuter, MCP orchestrator, and objdiff scripts) can return commands
# derived from a not-yet-fully-consistent build.ninja, leading to baseline
# match% returning 0.00% on the first invocation of every function in the unit.
# Priming re-runs SPLIT (regenerates config.json from config.yml) and settles
# the configure.py edge inside build.ninja, leaving the build graph fully
# consistent + `.ninja_log`/`.ninja_deps` initialized so subsequent queries are
# deterministic. Use ninja-locked (per CLAUDE.md, never bare ninja) so the
# worktree's own .ninja-build.lock serializes concurrent builds.
#
# CRITICAL — prime the `config.json` target, NOT the bare-ninja default.
# Bare `ninja` builds the report.json default target, which depends on ALL ~727
# objs. The reflinked cache is byte-valid, but the worktree's compile commands
# carry ABSOLUTE tool paths (--dtk/--objdiff/--wrapper, baked by configure.py
# above to dodge the cargo/download edges) that differ from main's relative-path
# commands recorded in main's .ninja_log. Fresh worktree => no local .ninja_log
# => SPLIT regenerates config.json => the configure generator edge fires =>
# build.ninja reloads => ninja recomputes and now finds every compile edge dirty
# ("deps missing" post-reload) => a full 727-obj rebuild, on EVERY worktree.
# Building `config.json` alone performs the SPLIT + graph-settle (the actual
# determinism goal) with ZERO obj compiles — the single largest wave-throughput
# win. A later full `ninja` (e.g. a verify A/B needing report.json) still does
# that one-time 727 rebuild, but single-obj objdiff agents (the common case) now
# rebuild only the obj they touch. See docs/decomp/handoff/
# worktree-build-tooling-findings-2026-07-01.md for the experiments.
#
# NOTE (rb3-xenon divergence from DC3): a failed prime is a loud WARNING, not
# fatal. The main tree is frequently mid-repair (the whole point of spinning up
# a worktree is often to FIX the broken build), so a prime failure must not
# block worktree creation. The full error is printed so the operator sees it,
# but the worktree is left fully configured and usable.
if [ "${WT_SKIP_PRIME:-0}" -eq 1 ]; then
    echo "==> WT_SKIP_PRIME=1 : skipping ninja prime (worktree configured but not primed)"
else
echo "==> Priming ninja state (SPLIT + config.json — scoped to avoid a full 727-obj rebuild)"
(
    cd "$WORKTREE_PATH"
    NINJA="./tools/ninja-locked"
    [ -x "$NINJA" ] || NINJA="ninja"
    prime_log="$(mktemp)"
    if "$NINJA" "build/$VERSION/config.json" >"$prime_log" 2>&1; then
        tail -5 "$prime_log"
        # The scoped prime above re-runs the SPLIT, which regenerates every target
        # .obj *un-renamed*. The obj_target_symbol_renamer edge does NOT declare
        # those objs as inputs (only the script, target_symbol_map.json and
        # config.json), so whether it re-fires afterwards is decided by an mtime
        # race between the just-written config.json and the reflink-COPIED stamp.
        # When the copied stamp wins, the renames are never applied: target objs
        # keep raw fn_<addr> names, nothing pairs, and the worktree silently
        # measures ~4,731 matched functions LOW (42,963 -> 38,232) with a plain
        # `ninja` unable to repair it. Drop the stamp so the renamer always runs.
        rm -f "build/$VERSION/target_symbol_renames.stamp"
    else
        echo "WARN: ninja prime failed (the main tree may not build yet)." >&2
        echo "      The worktree is still configured and usable; fix the build inside it." >&2
        echo "      ---- prime output ----" >&2
        cat "$prime_log" >&2
        echo "      ----------------------" >&2
    fi
    rm -f "$prime_log"
)
fi

echo ""
echo "Worktree ready:  $WORKTREE_PATH"
echo "  branch:        $BRANCH  (from $BASE_COMMIT on $BASE_BRANCH)"
echo ""
echo "Next:"
echo "  cd $WORKTREE_PATH"
echo "  ./tools/ninja-locked                    # NO target: a single-.obj build"
echo "                                          # skips the post-compile patchers"
echo "  python3 scripts/verify_objs_patched.py --verify-manifest   # is it trustworthy?"
echo ""
echo "Usage with MCP orchestrator:"
echo "  run_objdiff(symbol, project_dir=\"$WORKTREE_PATH\")"
echo ""
echo "Remove when done:  git -C $MAIN_REPO worktree remove --force $WORKTREE_PATH"
