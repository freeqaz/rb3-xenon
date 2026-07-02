# W2-B — objcache integration dry-run: worktree wiring, gates, PCH keying, concurrency stress

Model: **opus**. Wave 2. Uses W1-B's crate. Touches ONLY worktrees + the objcache repo —
NEVER rb3-xenon main's wiring (that is W3-A). Your deliverables: the chosen PCH key strategy
implemented in the crate, a fully-passing verification run, a multi-worktree stress result,
and the EXACT tools/project.py diff W3-A will apply to main.

## Hard rules

- NEVER `git stash` / `git checkout <file>` / `git restore` / `git reset --hard` in any main
  repo. Worktrees + logs under `~/tmp` only (never `/tmp`).
- Worktree builds via the worktree's `./tools/ninja-locked`, tee to `~/tmp/objcache_int_*.log`.
- MSVC objs are NEVER bit-stable across recompiles (COFF TimeDateStamp at offset 4, 4 bytes) —
  every byte comparison needs a same-settings control. Additionally: a cache HIT serves a
  timestamp-ZEROED obj (by design), and a cross-root hit differs in exactly one embedded
  /Fo path string — your comparators must model both.
- objcache repo commits: concise imperative style (repo created in W1-B).

## Context

W1-B built `/home/free/code/milohax/objcache` → `target/release/objcache`: a direct-mode
(dep-closure-hashing) object cache with a `objcache exec --fo $out -- <cmd>` CLI, blake3 keys
(compiler DLLs + cflags + source + validated closure), content-addressed store at
`~/.cache/rb3-objcache/` with atomic rename publish, reflink fetch, normalized (root-relative)
`Note: including file:` dep capture/replay for ninja `deps=msvc`, timestamp zeroing on store,
passthrough-on-any-failure, config-file toggle (NOT in the command string), and PCH argv
detection that currently passes `/Yc`//`/Yu` compiles through uncached. Read its DESIGN.md and
W1-B's report first.

By the time you run, main should have: window #1 (fork wibo + WIBO_REWRITE_SHOWINCLUDES, no
pipe) and window #2 (PCH: rules msvc_pch_create//Yc, msvc_pch//Yu, ~471 eligible TUs,
build/45410914/pch/system.pch). Verify both: `grep -c '/Yu"decomp_pch.h"' build.ninja` ≥1 in
main and no `transform_dep.py` in the msvc rule. If W2-A has not landed yet, create your
worktree from main and apply W1-C's PCH patch inside the worktree — the point is to test
against the FINAL rule shape.

## Part 1 — wire objcache into a worktree's build

```bash
cd /home/free/code/milohax/rb3-xenon
scripts/setup_worktree.sh ~/tmp/wt-objint objcache-int
```

In ~/tmp/wt-objint, edit tools/project.py (worktree copy): prefix the three msvc-family rule
commands with the cache. Proposed shape (this exact diff, refined, is your deliverable for
W3-A):

- `msvc` rule: `WIBO_FS_CACHE=1 WIBO_REWRITE_SHOWINCLUDES=1 {wrapper}{cl} ...` becomes
  `WIBO_FS_CACHE=1 WIBO_REWRITE_SHOWINCLUDES=1 /home/free/code/milohax/objcache/target/release/objcache exec --fo $out -- {wrapper}{cl} $cflags /showIncludes /Fo$out $in`
  (env prefix applies to objcache; the crate propagates env to the child — that was a W1-B
  requirement, verify it).
- The objcache binary path must come from the same resolver family configure.py uses for
  jeff/objdiff (`_find_local_fork`-style: upward walk → env `RB3_OBJCACHE_DIR` → baked
  absolute default), resolved ABSOLUTE so main and worktrees emit identical command strings.
  For the worktree experiment you may hardcode the absolute path; the delivered diff must use
  the resolver.
- `msvc_pch` and `msvc_pch_create` rules: same prefix. Note W1-C added an assert on the
  replace anchor — your prefix must not break it (prefix AFTER the .replace() calls compute,
  i.e. modify `msvc_cmd` before the rule definitions but after... simplest: wrap at the
  `msvc_cmd = f"WIBO_FS_CACHE=1 ..."` assignment so all three rules inherit the prefix and the
  `.replace()` anchors still match — verify `/Yc`/`/Yu` asserts still pass on reconfigure).

Then `python3 configure.py <same args as the worktree's configure_args>` and inspect
build.ninja: all three rules carry the objcache prefix; deps=msvc unchanged;
msvc_deps_prefix unchanged.

## Part 2 — PCH key strategy (decide by experiment, then implement in the crate)

Problem: under `/Yu`, cl restores state from system.pch and does NOT emit the pch-covered
headers in /showIncludes → the closure under-keys msvc_pch TUs. Candidate strategies:
(a) mix `blake3(system.pch bytes)` into the key;
(b) mix in the pch-SOURCE closure recorded from the `/Yc` compile (keyed by decomp_pch.h
    content + cflags).

Experiments to run first:
1. Build the pch twice in the same worktree (delete + rebuild): `cmp` the two system.pch —
   is it byte-stable modulo nothing/timestamps? Where do they differ?
2. Build system.pch in a SECOND worktree at a different path: `cmp` across worktrees — does
   the pch embed absolute paths?
3. Check what /showIncludes emits for a `/Yu` TU (count dep lines vs the same TU compiled
   plain) — confirms the under-keying premise and tells you what ninja deps look like for
   eligible TUs (record this for W3-B's benefit).

Decision rule: if system.pch is byte-stable within a root AND across roots → strategy (a)
(simplest, correct by construction). If it differs across roots (embedded paths) or across
rebuilds (timestamps) → strategy (b) (semantically keyed by pch inputs). Either way the
`/Yc` create compile itself stays UNCACHED (it is one edge, ~1 s, multi-output .pch+.obj —
not worth the complexity; document this).

Implement the chosen strategy in the crate + unit tests. The failure mode to test explicitly:
edit src/system/obj/Object.h (pch input) in the worktree → the pch rebuilds → every eligible
TU's key MUST change (no stale hits). Revert the edit after.

## Part 3 — verification protocol (the full dress rehearsal of W3-A's gates)

All in ~/tmp/wt-objint unless stated. Baseline first: with the cache DISABLED
(`objcache off`), full build, save report.json measures + a copy of 5 reference objs
(2 plain-msvc units, 2 msvc_pch units, 1 with .rdata/.data sections).

1. **Populate:** `objcache on`, wipe the cache dir, force a full recompile
   (`ninja -t restat` won't do it — touch a shared header? No: cleanest is
   `rm -rf build/45410914/src && ./tools/ninja-locked`), confirm 100% misses
   (`objcache stats`), build completes, report equals baseline.
2. **Hit correctness (same root):** `rm -rf build/45410914/src`, rebuild → expect ~100% hits
   (the /Yc edge + non-msvc edges excepted). For the 5 reference units: `cmp -l` hit obj vs
   baseline obj → differences ONLY in bytes 5-8 (the zeroed timestamp). objdiff match% per
   function unchanged. Full report: **matched_functions EXACTLY equal** to baseline.
3. **Deps replay correctness:** `ninja -t deps <unit>.obj` after a hit → VALID, non-empty,
   all paths root-relative (this is the W3-B enabler — record a sample). Positive control:
   touch a transitive header → unit recompiles (as a cache hit or miss depending on whether
   the closure changed — a content-identical touch is a HIT but ninja must still re-run the
   edge). Negative control: touch an unrelated file → no re-run.
4. **Closure-change correctness:** actually EDIT a header (add a comment line) → dependent
   TUs re-run as MISSES (closure hash changed), new entries stored; revert the edit →
   dependents re-run as HITS (old manifest closure revalidates). This proves the manifest
   handles multiple closures per source.
5. **Cross-root sharing:** create a second worktree ~/tmp/wt-objint2 (same wiring), same
   commit. Cold build there with the populated cache → expect ~100% hits. For a reference
   unit: the fetched obj vs worktree-1's obj differ in AT MOST the one embedded /Fo path
   string (+ nothing else) — verify with a masked compare (locate the path string, mask both,
   cmp). objdiff + report in worktree-2: matched_functions equal to worktree-1's.
6. **Concurrency stress (hard gate):** from a WIPED cache, launch simultaneous full builds in
   BOTH worktrees (each `./tools/ninja-locked` is -j default ≈ nproc; that's 2×32 jobs racing
   on identical keys). Both must complete; reports equal baseline; then verify store
   integrity: `objcache verify`-style sweep or re-hash every stored obj against its key
   (write a small loop; the crate's paranoid mode helps). Repeat once with 3 worktrees +
   main-shaped load if the box allows. Also kill -9 one build mid-flight and confirm the
   cache has no torn entries and the next build completes clean.
7. **Failure-path:** `chmod -R a-w ~/.cache/rb3-objcache` → build still succeeds (all
   passthrough); restore permissions. `OBJCACHE=off ./tools/ninja-locked` → passthrough.
8. **Perf numbers (report these):** hit-path p50/p99 per TU (from objcache stats/timing),
   compile-phase wall for the all-hits full build vs the all-miss build vs the no-cache
   baseline.

## Deliverables

1. Crate updated (PCH keying + anything the dress rehearsal shook out), committed in
   /home/free/code/milohax/objcache, release binary rebuilt.
2. The exact, final tools/project.py diff for main (attach as a patch file under
   ~/tmp/objcache_main_wiring.patch AND inline in your report) including the resolver for the
   objcache binary path.
3. Gate evidence for all 8 protocol items + the PCH key decision writeup (which strategy,
   what the experiments showed).
4. Cleanup: remove the worktrees (`git worktree remove --force ...` from main), leave the
   populated cache in place (it seeds W3-A's landing).

## Rollback

Nothing on main to roll back. `objcache off` disables globally; wiping
~/.cache/rb3-objcache resets state.
