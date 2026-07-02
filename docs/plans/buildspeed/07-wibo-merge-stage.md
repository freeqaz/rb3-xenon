# W2-C — Merge the wibo `perf-residual` branch; build a STAGED binary; re-run the byte gate

Model: **sonnet**. Wave 2. wibo repo only. Does NOT touch rb3-xenon, does NOT touch the live
binary at /home/free/code/milohax/wibo/build/release/wibo (main's builds run it — swapping it
is W3-A's job).

## Hard rules

- NEVER `git stash` / `git checkout <file>` / `git restore` / `git reset --hard` in the wibo
  main checkout. Merging on main's branch is allowed (that IS the task) but keep the working
  tree clean otherwise.
- The live binary `/home/free/code/milohax/wibo/build/release/wibo` must remain BYTE-UNCHANGED
  by this task (rb3-xenon main's msvc rule invokes it on every compile; an in-place rebuild
  mid-build is a race). Verify at the end: its mtime/sha256 are the same as at the start.
- Scratch/logs under `~/tmp` (never `/tmp`). wibo commit style: plain, no trailer.
- MSVC objs: only the 4 COFF timestamp bytes at offset 4 may differ across same-settings
  recompiles — byte gates need a control pair.

## Context

W1-D produced branch `perf-residual` in /home/free/code/milohax/wibo (base 6a7c37e):
weakly_canonical→lexically_normal (src/files.cpp:872 + dll/kernel32/fileapi.cpp ~340-356),
a negative-exists cache scoped to read-only include roots (never /Fo outputs or *.pch),
wiring the dead `reportFilesCacheStats()` (src/files.cpp:959) at exit, and a memoized
current_path(). Its report includes byte-gate + syscall evidence. Read that report first;
if W1-D dropped any item, your merge covers only what passed.

Why staging: rb3-xenon main (after wave-1's window #1) resolves the wrapper to the absolute
path above. The binary is NOT a ninja implicit input (rebuilding it does not dirty the
graph), so a swap is invisible to ninja — which is exactly why the swap must be deliberate,
verified, and atomic (W3-A does `mv` into place when main is quiet).

## Steps

1. Preflight:
   ```bash
   cd /home/free/code/milohax/wibo
   git log --oneline -3                      # HEAD must be 6a7c37e or a descendant
   git status --short                        # must be clean (untracked build/ dirs are fine)
   sha256sum build/release/wibo > ~/tmp/wibo_live_before.sha   # the do-not-touch reference
   git log --oneline main..perf-residual 2>/dev/null || git log --oneline HEAD..perf-residual
   ```
2. Review the branch commits (read the diffs; sanity-check the negative-cache gating excludes
   .obj/.pch/.pdb and build-output prefixes). If anything looks unsafe, STOP and report
   rather than merging.
3. Merge: `git merge --no-ff perf-residual -m "merge perf-residual: lexically_normal, scoped negative-stat cache, stats reporter, cwd memoize"`.
4. Build to a STAGING location (never the live preset dir):
   ```bash
   cmake -S . -B build/staging -G Ninja -DCMAKE_BUILD_TYPE=Release \
     $(python3 - <<'EOF'
import json;p=json.load(open('CMakePresets.json'))
base={}
def walk(n,acc):
    for pr in p['configurePresets']:
        if pr['name']==n:
            for i in pr.get('inherits',[]): walk(i,acc)
            acc.update(pr.get('cacheVariables',{}))
walk('release',base)
print(' '.join(f'-D{k}={v}' for k,v in base.items()))
EOF
) 2>&1 | tee ~/tmp/wibo_stage_conf.log
   cmake --build build/staging 2>&1 | tee ~/tmp/wibo_stage_build.log
   ```
   (The python snippet flattens the `release` preset's cacheVariables so the staging build has
   IDENTICAL configuration to the live binary's preset — including WIBO_ENABLE_LIBURING etc.
   If the preset graph is simple enough, read CMakePresets.json and pass the -D flags by hand
   instead; the requirement is: same cache variables as preset `release`, different binaryDir.)
5. Verify the staged binary:
   ```bash
   ./build/staging/wibo --version            # 1.0.1-<n>-g<merge sha> (Linux x86_64)
   strings build/staging/wibo | grep -c WIBO_REWRITE_SHOWINCLUDES   # >=1
   strings build/staging/wibo | grep -c WIBO_FS_CACHE               # >=1
   ```
6. Byte gate with the staged binary (same recipe as W1-D's verification, 3 TUs incl. one
   band3 + one heavy-include): control pair with the LIVE binary → noise floor = timestamp
   bytes only; live vs staged obj → same profile. showIncludes stdout identical (with and
   without WIBO_REWRITE_SHOWINCLUDES=1). Both stats reporters print under
   WIBO_FS_CACHE_STATS=1.
7. Confirm the live binary untouched: `sha256sum -c ~/tmp/wibo_live_before.sha`.

## Deliverables

- wibo main branch contains the merge; staged binary at
  `/home/free/code/milohax/wibo/build/staging/wibo` (leave it there — W3-A deploys it).
- Report: merge sha, staged --version string, byte-gate evidence, stats output, confirmation
  the live binary is untouched.

## Rollback

The merge is unreleased (live binary unchanged). If a gate fails: `git revert -m 1 <merge sha>`
on wibo main, delete build/staging, report which item failed.
