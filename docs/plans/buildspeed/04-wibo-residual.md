# W1-D — wibo fork residual perf: readlink storm, scoped negative-stat cache, stats reporter

Model: **sonnet**. Wave 1. Work on a BRANCH in a wibo worktree — never wibo main, never
deploy. W2-C merges + stages; W3-A deploys.

## Hard rules

- NEVER `git stash` / `git checkout <file>` / `git restore` / `git reset --hard` in any main
  repo. The wibo main checkout is /home/free/code/milohax/wibo (HEAD 6a7c37e) — another task
  (W1-A) is rebuilding its release binary from that exact HEAD **concurrently**. You must not
  move HEAD or dirty that tree. Work in a git worktree:
  ```bash
  cd /home/free/code/milohax/wibo
  git worktree add ~/tmp/wt-wibo-residual -b perf-residual
  cd ~/tmp/wt-wibo-residual
  cmake --preset release-worktree 2>/dev/null || cmake -S . -B build-res -G Ninja -DCMAKE_BUILD_TYPE=Release $(grep -o '"WIBO_ENABLE_[A-Z_]*": "ON"' CMakePresets.json >/dev/null && echo -DWIBO_ENABLE_LIBURING=ON)
  ```
  (If the `release` preset hardcodes binaryDir `${sourceDir}/build/release` it will build
  inside YOUR worktree's build/release — that is fine and isolated. Just never touch
  /home/free/code/milohax/wibo/build/release/wibo.)
- wibo commit style: plain, concise, no Co-Authored-By trailer (match `git log` style).
- All scratch/logs under `~/tmp`, never `/tmp`.
- MSVC objs are NEVER bit-stable across recompiles (COFF TimeDateStamp, 4 bytes at offset 4).
  Your byte-verification MUST include a same-settings control pair.

## Context

The fork (freeqaz/wibo, this repo) implements WIBO_FS_CACHE — path-resolution memoization
that cut syscalls/TU from 89,643 to 10,784. Profiling (strace, on
rb3-xenon src/band3/meta_band/Instarank.cpp with cache ON) shows the residual top sinks:

1. **readlink storm — 2,283/TU, ALL failing (EINVAL, not-a-symlink).** Cause:
   `std::filesystem::weakly_canonical` resolving each path component. Call sites:
   `src/files.cpp:872` (canonicalPath) and `dll/kernel32/fileapi.cpp` resolvedPath
   (~lines 340-356). The decomp source trees contain no symlinks (only the toolchain dir,
   resolved once at startup), so `weakly_canonical` can become `lexically_normal` (plus at
   most one lstat if you want belt-and-suspenders).
2. **negative-stat storm — ~1,510 newfstatat ENOENT/TU.** The exists-cache
   (`src/files.cpp:301-308`) deliberately caches only positives — the comment documents why:
   output files (/Fo obj, *.pch) get CREATED mid-process and a cached negative would break
   that. Every include-search miss therefore re-stats. Fix: a negative-exists cache **scoped
   by path prefix to the read-only include roots** (the /I dirs: src/, src/system/,
   src/system/stlport/, src/xdk/…, plus the compiler dir), never caching negatives for paths
   under the build output tree or matching `*.pch`/`*.obj`. A wrongly-cached negative fails
   LOUD (cl.exe C1083 cannot-open-include) — still, scope conservatively.
3. **dead stats reporter:** `reportFilesCacheStats()` defined at `src/files.cpp:959`,
   declared in `files.h:41`, NEVER called — so WIBO_FS_CACHE_STATS only prints the
   fileapi.cpp:648 layer. Wire it into the same process-exit path fileapi's reporter uses.
   This is the regression tripwire that would have caught the stock-binary clobber instantly.
4. **getcwd churn:** `std::filesystem::current_path()` ~820×/TU
   (src/modules.cpp:431, dll/kernel32/winbase.cpp:244/285) though cwd never changes —
   memoize once (~0.7 ms/TU).

Expected impact: ~3,800 of 10,784 residual syscalls/TU (~35%); small wall-clock alone
(~0.1-0.2 s/745-TU build at 32-wide) but real kernel dcache/inode contention relief in the
high-load regime where the fleet actually builds. Line numbers are from HEAD 6a7c37e —
re-locate by content if drifted.

## Steps

1. Read the current implementations (files named above) until you can state why each cache
   is safe. Note exactly which paths the positive exists-cache refuses to cache and mirror
   that exclusion set for negatives.
2. Implement, one commit per item, in this order (each independently revertable):
   a. `weakly_canonical` → `lexically_normal` at the two call sites. Guard: if the input path
      contains a component that IS a symlink (check the toolchain root once at startup),
      fall back to the old path — or simpler, document that both call sites only ever see
      guest paths under the mapped roots which contain no symlinks.
   b. Scoped negative-exists cache. Gate: only cache a negative when the path is under one of
      the include roots captured at first use (derive from the /I args cl was invoked with, or
      a conservative allowlist: any path whose resolved prefix is under the cwd's `src/`
      subtree or the compiler dir) AND the basename does not end in `.obj/.pch/.pdb`.
      Invalidation: none needed within a process IF the gate is right (include roots are
      read-only during a compile); the cache is per-process (static map), same as the others.
   c. Wire `reportFilesCacheStats()` at exit next to fileapi.cpp:648's reporter.
   d. Memoize `current_path()` (initialize once; wibo never chdirs after startup — verify by
      grepping for chdir/current_path setters before assuming).
3. Rebuild release in the worktree.

## Verification (mandatory before you call it done)

Test TU: any real rb3-xenon compile. Use a throwaway rb3-xenon worktree
(`/home/free/code/milohax/rb3-xenon/scripts/setup_worktree.sh ~/tmp/wt-wiboverify wibo-verify`)
or run the raw command from the rb3-xenon repo root WITHOUT writing into build/
(use `/Fo$HOME/tmp/wiboverify/<name>.obj`):

```bash
cd /home/free/code/milohax/rb3-xenon
CMD=(build/compilers/X360/16.00.11886.00/cl.exe /I src/system/stlport /I src/xdk/LIBCMT \
     /I src /I src/system /nologo /wd4355 /wd4164 /c /GR /O1 /Oi /EHsc /TP /showIncludes)
# A: control pair with the UNMODIFIED fork binary (baseline + noise floor)
WIBO_FS_CACHE=1 /home/free/code/milohax/wibo/build/release/wibo "${CMD[@]}" \
  /Fo$HOME/tmp/wiboverify/a1.obj src/system/os/System.cpp > ~/tmp/wiboverify/a1.out
WIBO_FS_CACHE=1 /home/free/code/milohax/wibo/build/release/wibo "${CMD[@]}" \
  /Fo$HOME/tmp/wiboverify/a2.obj src/system/os/System.cpp > ~/tmp/wiboverify/a2.out
# B: your patched binary
WIBO_FS_CACHE=1 ~/tmp/wt-wibo-residual/<your build dir>/wibo "${CMD[@]}" \
  /Fo$HOME/tmp/wiboverify/b1.obj src/system/os/System.cpp > ~/tmp/wiboverify/b1.out
```

1. **Byte gate:** `cmp -l a1.obj a2.obj` → expect ONLY offset-4..8 timestamp byte(s) (that is
   the noise floor). `cmp -l a1.obj b1.obj` → must show the SAME profile (only timestamp
   bytes). Any other differing byte = ABORT, bisect your commits. Repeat on 3 TUs including
   one under src/band3 and one heavy-include one (e.g. src/band3/meta_band/MusicLibrary.cpp).
2. **stdout gate:** `diff <(sort a1.out) <(sort b1.out)` on the `Note: including file:` lines
   → identical (your changes must not alter showIncludes output). Also run once with
   `WIBO_REWRITE_SHOWINCLUDES=1` on both and compare.
3. **Syscall gate:** `strace -c -f` both binaries on the same TU → report readlink and
   newfstatat counts before/after (expect ≈2,283→~0 and ENOENT-stat ≈1,510→~small).
4. **Stats tripwire:** `WIBO_FS_CACHE=1 WIBO_FS_CACHE_STATS=1 <patched wibo> …` → BOTH
   reporter lines print (files.cpp layer + fileapi.cpp layer).
5. **Full-build smoke:** in a throwaway rb3-xenon worktree, point the worktree's build at
   your binary (re-run its `python3 configure.py --wrapper <your binary> …` with the same
   other args from build.ninja's `configure_args`), full `./tools/ninja-locked`, confirm
   completion + no C1083 storm, then run the report and compare matched_functions to the same
   worktree's pre-swap value (must be equal).

## Deliverables

- Branch `perf-residual` in the wibo repo (pushed as a local branch; worktree can be removed),
  one commit per item, each message stating the measured syscall delta.
- A short report: per-item syscall counts before/after, byte-gate evidence (the cmp profiles),
  full-build smoke result. State explicitly: "NOT deployed; W2-C merges, W3-A deploys."

## Rollback

Branch is unmerged — nothing to roll back. If an item fails its gate, drop that commit from
the branch and note it in the report.
