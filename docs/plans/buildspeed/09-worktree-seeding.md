# W3-B — Worktree warm-state seeding: .ninja_log/.ninja_deps in setup_worktree.sh, 0-compile verify

Model: **sonnet**. Wave 3. **Start only after W3-A reports success** (the orchestrator should
sequence this; you ALSO self-gate — see Preconditions). Completes the half-landed "L2
fully-warm worktrees" work (session tasks #8/#9/#10 lineage).

## Hard rules

- NEVER `git stash` / `git checkout <file>` / `git restore` / `git reset --hard` in main.
- Worktrees + logs under `~/tmp` (never `/tmp`). Build via each tree's `./tools/ninja-locked`,
  tee to `~/tmp/rb3_seed_*.log`.
- Commit pathspec-only (`git commit -o scripts/setup_worktree.sh CLAUDE.md`); trailer
  `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`.

## Context

Goal: a fresh worktree's FIRST full `ninja` should be a 0-compile no-op. Three things
historically prevented it; the first two are now fixed:

1. ~~Command-hash mismatch~~: worktrees bake absolute tool paths (--dtk/--objdiff/--wrapper,
   and now objcache) via `scripts/setup_worktree.sh` → configure.py. Since windows #1-#3,
   MAIN's build.ninja uses the SAME absolute paths (jeff/objdiff prebuilt preference, wibo
   wrapper default, objcache resolver) → main and worktree msvc command strings are
   byte-identical. Verifiable: `cmp <(main build.ninja msvc rule) <(worktree's)`.
2. ~~Deps portability~~: main's .ninja_deps used to hold MIXED absolute+relative source paths
   (e.g. MasterAudio.obj: 41 absolute /home/free/.../rb3-xenon/src/... + 221 relative) —
   copying it into a worktree would pin those deps to MAIN's files (worktree header edits
   silently missed). Since window #3, objcache normalizes every stored/replayed
   `Note: including file:` line to repo-root-relative, and W3-A's settle re-logged all TUs —
   main's deps should now be uniformly relative. YOU MUST VERIFY, not assume.
3. **Missing ninja state** (this task): scripts/setup_worktree.sh currently `rm -f`s
   `.ninja_log`/`.ninja_deps` in the fresh worktree (~line 235: "Drop stale ninja state
   copied from main") — ninja treats every output as having no log entry → dirty → full
   recompile. The script already restamps mtimes so inputs predate outputs (the
   "warm-cache validation" block, ~lines 238-300) and already gates on main-clean
   (`_changed` count of src/|config/ diffs). You add: seed the two files from main when safe.

Read the whole script first (/home/free/code/milohax/rb3-xenon/scripts/setup_worktree.sh,
~500 lines); the flow is: reflink build dir → rm ninja state (line ~235) → warm-cache mtime
restamp (~238+) → configure.py with absolute tool paths (~358-367) → build.ninja assertion
(~376+) → prime ninja state (~460+). Line numbers drift — anchor on the quoted comments.

## Preconditions (self-gate; poll until true or report blocked after ~2h)

In /home/free/code/milohax/rb3-xenon:
```bash
grep -q "objcache" build.ninja                               # window #3 wired
ninja -t deps build/45410914/src/system/beatmatch/MasterAudio.obj | grep -c '^    /'   # == 0
git log --oneline -5   # W3-A's commit present; git status shows no in-flight wiring edits
```
If the deps check shows absolute paths, W3-A's re-log hasn't fully regenerated deps —
do not seed; report blocked.

## Steps

1. Edit scripts/setup_worktree.sh:
   - Replace the unconditional `rm -f "$WORKTREE_PATH/.ninja_log" "$WORKTREE_PATH/.ninja_deps" ...`
     (keep removing `.ninja_lock`/`.ninja-build.lock` unconditionally) with seeding logic that
     runs AFTER configure.py has produced the worktree's build.ninja (move it below the
     configure step — order matters, parity can only be checked post-configure):
     ```
     seed only if ALL hold:
       a) WARM_CACHE == 1 and the existing "$_changed == 0" main-clean gate passed
          (reuse/extend that computation — main must have no src/|config/ diffs vs BASE_REF,
          and additionally no tools/project.py or configure.py diffs: a dirty wiring file
          means main's .ninja_log may not match what this worktree's build.ninja would run);
       b) the worktree's build.ninja msvc rule block is byte-identical to main's:
          extract both (awk from 'rule msvc' through the blank line after 'deps = msvc',
          for msvc, msvc_pch, msvc_pch_create) and cmp;
       c) main's .ninja_deps has zero absolute-src dep paths:
          (cd "$MAIN_REPO" && ninja -t deps <probe obj> | grep -c '^    /') == 0
          — probe 3 objs incl. one msvc_pch unit;
       d) main's .ninja_log and .ninja_deps exist and are non-empty.
     then: cp --reflink=auto "$MAIN_REPO/.ninja_log" "$WORKTREE_PATH/.ninja_log"
           cp --reflink=auto "$MAIN_REPO/.ninja_deps" "$WORKTREE_PATH/.ninja_deps"
     else: keep current behavior (no seed) and echo WHY (which gate failed) — observability
           matters; silent fallback hides regressions.
     ```
   - The existing mtime restamp must still run (checked-out sources are stamped 'now' by
     `git worktree add`; ninja's log stores recorded mtimes — the restamp makes
     outputs-newer-than-inputs hold; keep its ordering relative to the seed: restamp AFTER
     seeding is fine since restat compares filesystem mtimes at build time).
   - Do not touch the `--cold-cache` path (baselines must stay honestly cold; with objcache
     they are cheap anyway).
2. Verify end-to-end (this is session-task #9's content):
   ```bash
   cd /home/free/code/milohax/rb3-xenon
   scripts/setup_worktree.sh ~/tmp/wt-seedtest seed-test 2>&1 | tee ~/tmp/rb3_seed_create.log
   cd ~/tmp/wt-seedtest
   ./tools/ninja-locked 2>&1 | tee ~/tmp/rb3_seed_firstbuild.log
   grep -c "MSVC \|PCH " ~/tmp/rb3_seed_firstbuild.log      # EXPECT 0 compiles
   ```
   Then correctness controls IN THE WORKTREE:
   - positive: append a comment line to a header with many dependents (e.g.
     src/system/obj/Data.h) → `./tools/ninja-locked` recompiles its dependents (they'll be
     cache MISSES in objcache — fine), then revert the edit (plain edit revert, this is your
     worktree branch) → rebuild (hits);
   - positive-PCH: touch-edit src/system/os/Debug.h → PCH rebuilds + eligible TUs re-run;
     revert;
   - negative: `touch docs/README-equivalent` → 0 recompiles;
   - deps sanity: `ninja -t deps <the edited unit>.obj` in the worktree is VALID and
     root-relative.
   Repeat the creation once more while ANOTHER worktree build runs (herd behavior), and once
   with main deliberately dirtied by a scratch src edit in a THROWAWAY file you create then
   delete (touch src/system/os/__seedgate_probe.h; expect: seeding SKIPPED, script still
   succeeds, worktree builds correctly the slow way; rm the probe file after).
3. Cleanup test worktrees (`git worktree remove --force ...` from main).
4. Update CLAUDE.md's "Git & worktrees" bullet for setup_worktree.sh: fresh worktrees now
   seed main's ninja state when main is clean & parity holds → first full ninja ≈ 0 compiles;
   cold-cache flag unchanged; seeding auto-skips (with a printed reason) when gates fail.
5. Commit: `git commit -o scripts/setup_worktree.sh CLAUDE.md -m "worktree: seed .ninja_log/.ninja_deps for 0-compile fresh worktrees (gated on parity + relative deps + clean main) ..."`
   with the trailer.

## Acceptance criteria

1. Fresh warm worktree first full build = 0 MSVC/PCH compiles (log evidence).
2. Positive/negative/PCH dep controls pass in the seeded worktree.
3. Gate-failure path proven (dirty main → seed skipped with printed reason, build still
   correct).
4. Committed pathspec-only; CLAUDE.md updated.

## Rollback

`git revert` the commit — the script returns to rm-and-rebuild behavior (slower, always
correct). Seeded worktrees are throwaway by nature; no migration.
