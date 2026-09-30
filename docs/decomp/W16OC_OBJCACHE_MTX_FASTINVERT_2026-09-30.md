# W16-OC — the "objcache served a 232 B FastInvert" report (2026-09-30)

**Verdict: objcache is NOT at fault.** The 232 B vs 248 B pair is two different
*source versions* of `mtx.cpp`. The wrong object reached W16-IB's tree through
`scripts/setup_worktree.sh`'s warm-cache validation, not through a cache hit.
Main's current object is correct.

Source report: W16-IB §7 (`docs/decomp/W16IB_ANON_GAME_ROWS_IDENTIFIED_2026-09-30.md`
on `w16-ib`) — "identical source, headers, dependency list and command compiles to
`FastInvert` = 248 B with `OBJCACHE=off` … one tree had been served a cached 232 B object."

## 1. What each object is (measured)

All sizes are the `?FastInvert@@YAXABVMatrix3@Hmx@@AAV12@@Z` COMDAT, read with
`tools/comdat_bytes.py`. Uncached compiles used the exact ninja `msvc` command with
`OBJCACHE=off`, run from worktree `~/tmp/wt-w16-oc` (at `e9cf2796b`).

| object | mtx.cpp blake3 | FastInvert |
|---|---|---|
| main `build/45410914/src/system/math/mtx.obj` (main tip `1a679219f`) | `cac7165ab946…` | **232 B** |
| uncached compile of main's `mtx.cpp` | `cac7165ab946…` | **232 B**, byte- and reloc-identical to main's |
| uncached compile of `72f4922b2^:mtx.cpp` (pre-fix) | `7a76333f1626…` | **248 B** |

Commit `72f4922b2` (W16-HY-M, "FastInvert — three unguarded 1/Dot divides; 30.53 → 99.50")
is the whole difference: it replaced the three zero-guarded divides (248 B) with retail's
unguarded ones (232 B). The "scores 99.5" W16-IB saw is that commit's own figure.
`mtx.cpp` is unchanged between `e9cf2796b` and main's tip `1a679219f`.

**Main holds the correct object.**

## 2. The cache is consistent on this input

objcache keys on compiler identity + cflags + a sorted `(path, blake3)` closure of the
source plus every `/showIncludes` header (`src/key.rs`), and it re-hashes the closure on
every lookup (`store::find_hit`). All **252** cached objects with `source=src/system/math/mtx.cpp`
were read from `~/.cache/rb3-objcache` (read-only). Each was grouped by the `mtx.cpp`
hash in its manifest record:

- 8 distinct `mtx.cpp` hashes; **every hash maps to exactly one FastInvert size**.
- 232 B occurs **only** under `cac7165…` (post-fix, 6 objects, first stored 22:01:39).
- The pre-fix hash `7a76333…` maps only to 248 B (92 objects).
- There is one cflags string across all 252 entries.

No hash yields both sizes, so there is no trace of an under-keyed hit. The
general direct-mode blind spot, a *new* header created earlier on the `/I` path that
would shadow a recorded one, is not keyed. It is theoretical here, because the closure
shown in this incident is unchanged.

## 3. How a 232 B object got into a pre-fix tree

Timeline (from the reflogs):

| time (UTC) | event |
|---|---|
| 22:34:59 | main → `74ee485ef` |
| 22:37:14 | `w16-ib` created from HEAD = `74ee485ef` (setup validated correctly: base == main HEAD) |
| 22:41:24 | main → `e9cf2796b` (W16-HY merge, **brings the FastInvert fix**) |
| 22:41:40 | a 232 B `mtx.obj` is stored to the cache (main's post-merge build) |
| 22:49:12 | `w16-ib-base` "Created from 74ee485ef"; `~/tmp/wt-w16-ib-ab` set up on it |
| 22:49:59 | `ab0` build there: 381 compiles (PCH cascade), **mtx.obj not among them** |
| 22:55:38 | `ab2`: `[5/14] MSVC …/mtx.obj`, the first recompile (→ 248 B) |

`wt-w16-ib-ab`'s `mtx.cpp` still carries mtime **2020-01-01** and the pre-fix content. That
stamp is written only when setup's warm-cache guard passes. From 22:49 to 22:55, that tree
therefore paired **pre-fix source** with **main's reflinked post-fix object**, and ninja
considered it current.

### Root cause: `setup_worktree.sh`'s guard never compares the worktree's commit to main's

```sh
_changed = main dirty  +  main --cached
         + git -C "$WORKTREE_PATH" diff "$BASE_REF"     # HEAD here = the WORKTREE's HEAD
         + git -C "$MAIN_REPO"     diff "$BASE_REF" HEAD # HEAD here = MAIN's HEAD
```

The reflinked objects were built at **main's HEAD**. The worktree is checked out at
`BASE_REF` only when the branch is new. When `$BRANCH` already exists, setup runs
`git worktree add "$WORKTREE_PATH" "$BRANCH"` and gets the branch's own tip. The same
happens when the path already exists ("reconfiguring in place"). With the default
`BASE_REF=HEAD`, every term is then trivially 0. Setup stamps every tracked file to 2020,
older than main's objects, **and** seeds main's `.ninja_log`/`.ninja_deps`, whose gate
reuses the same `_changed` (line ~604). Every object whose source changed between the two
commits is then silently stale until something touches it.

**Reproduced** (probe, since removed): `git branch w16-oc-probe 74ee485ef`, then
`scripts/setup_worktree.sh ~/tmp/wt-w16-oc-probe w16-oc-probe` with main at `1a679219f`:

- setup prints `Validating warm object cache (worktree == HEAD; marking outputs current)`;
- `mtx.cpp` has the pre-fix hash `7a76333…`, mtime 2020-01-01, and the reflinked `mtx.obj` holds a **232 B** FastInvert;
- `ninja -d explain -n …/mtx.obj` names only the `always`/split-stamp edges, **not mtx.obj**.
  The control after `touch mtx.cpp` reports "output … older than most recent input" and schedules the compile, so the query discriminates.

One guard **did** fire in the probe: `verify_split_current.py` (content-keyed) refused,
because splits.txt differs between `74ee485ef` and `1a679219f`. For W16-IB's real pair
(`74ee485ef` → `e9cf2796b`) `config/` changed **0** files and `src/` **66**, so that
guard passes, and **no** content check covers the compiled objects.

**Exposure for that pair:** from `ninja -t deps` plus the changed `.cpp`s, **523** compiled
objects have a closure that includes one of the 66 files. 132 of them are PCH-eligible
(rebuilt by the PCH-drop cascade) and **391 are not**. This is an upper bound on objects
that actually differ.

**W16-IB's A/B is not affected by this.** `ab3` (finished 23:16:46, three seconds before
the `ab_measure` run 20260930-231649) recompiled 1,226 objects, **including all 523**. So
leg A inherited no stale main object. The §7 per-row caution was nonetheless right.

## 4. Fix (applied on `w16-oc`, `scripts/setup_worktree.sh`)

The fix goes in `scripts/setup_worktree.sh`; objcache itself needs nothing. It adds the missing
comparison, "the worktree's checked-out commit vs the commit that built the reflinked
objects (main's HEAD)". It also adds the worktree's own uncommitted work (tracked diffs
including staged, plus untracked `src/`/`config/` files) and fails closed if the worktree's
HEAD cannot be resolved. The core of the change, as first proposed:

```diff
     _changed="$( { git -C "$MAIN_REPO" diff --name-only 2>/dev/null;
                    git -C "$MAIN_REPO" diff --name-only --cached 2>/dev/null;
                    git -C "$WORKTREE_PATH" diff --name-only "$BASE_REF" 2>/dev/null;
-                   git -C "$MAIN_REPO" diff --name-only "$BASE_REF" HEAD 2>/dev/null; } \
+                   git -C "$MAIN_REPO" diff --name-only "$BASE_REF" HEAD 2>/dev/null;
+                   # the reflinked objs were built at MAIN's HEAD; the worktree may be at
+                   # an existing branch's tip (worktree add <path> <branch> ignores BASE_REF)
+                   git -C "$MAIN_REPO" diff --name-only \
+                       "$(git -C "$WORKTREE_PATH" rev-parse HEAD)" HEAD 2>/dev/null;
+                   git -C "$WORKTREE_PATH" diff --name-only HEAD 2>/dev/null; } \
                  | grep -cE '^(src/|config/)' || true )"
```

### Verified on real runs

**Harness.** The script derives `MAIN_REPO` from its own path, so the fixed copy in
`~/tmp/wt-w16-oc` treats that worktree as "main". It was rebased onto `1a679219f` and
fully built and settled first (389 compiles on the first build, **0** on the second, tree
clean), so its `build/` reflects its HEAD. The real main was not touched.
Fixtures were `oc-stale`, which is HEAD with only `mtx.cpp` reverted to pre-`72f4922b2`
(a `src`-only difference, the incident's shape), `oc-probe` at `74ee485ef`, and `oc-fresh`, a
new branch at HEAD. Each build is a full `./tools/ninja-locked`.

| case | script | guard | seeding | first build | mtx.obj FastInvert |
|---|---|---|---|---|---|
| control: `oc-stale` (existing branch) | **unfixed** | `Validating` ⛔ | `Seeded` ⛔ | rc 0, 381 MSVC edges, **mtx not compiled** | **232 B beside pre-fix source** ⛔ |
| 1: `oc-stale` (existing branch) | fixed | **NOT validated** (1 path) | **NOT seeded** | rc 0, 1,226 edges, mtx compiled | **248 B** (correct for its source) |
| 2: probe, `oc-probe` at `74ee485ef` | fixed | **NOT validated** (73 paths) | **NOT seeded** | not built¹; 0 files 2020-stamped, `ninja -d explain` schedules mtx.obj | n/a |
| 3: fresh `oc-fresh` at HEAD | fixed | `Validating` (`f2494e685 == main HEAD`) | `Seeded` | rc 0, 41 s; **0 non-PCH compiles**; second build 0 edges | n/a |

¹ Case 2 needs `RB3_ALLOW_UNRESOLVED_SPLITS=1` for setup's configure step and would stop
at `verify_split_current.py` in a build. Both failures are unrelated, pre-existing
consequences of reflinking `build/` from a main whose `config/` differs, and the split guard
is correct to refuse.

The seeding gate follows because it reuses `_changed`: it seeds in case 3 and refuses in
cases 1 and 2. Its refusal message is reworded to match.

⚠ **Case 3's first build is not literally "0 compiles".** All **381** MSVC edges are the
PCH-reset cascade (setup deliberately drops main's `system.pch`, so the `/Yc` edge and every
`/Yu` TU re-run and are served by objcache). The unfixed control also shows exactly 381, so
this is pre-existing, and CLAUDE.md's "true 0-compile no-op" holds only outside the 9 PCH
dirs. That cache serving is inferred from the 41 s wall time and was not separately measured.

Other options considered but not implemented: stamp to 2020 only files that are
**byte-identical to main's working copy**, so only their dependents rebuild. Seeding
would still need the all-or-nothing gate.

**Correction to this doc's first version.** It listed "main's `build/` may lag main's HEAD
after a merge" as an unaddressed residual. Setup already runs `ninja-locked post-compile` in
main before reflinking ("Refreshing main's object cache"). That skips only when main has
uncommitted `config/` changes, and in that case the `_changed` gate refuses anyway. The
remaining window is a refresh that **fails** (logged as a non-fatal WARN), which this change
does not cover.

⚠ dc3-decomp's `scripts/setup_worktree.sh` has the identical guard shape (lines 240, 389–390).
It is not touched here.

## 5. Not done

- No change to objcache source, its live binary, or `~/.cache/rb3-objcache` (read-only scan).
- No change to main or to dc3-decomp. The `setup_worktree.sh` fix lives on `w16-oc` for the
  coordinator to merge.
- The shadowing blind spot in §2 was not probed.
