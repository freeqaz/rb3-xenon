# W3-A — Wiring window #3: wire objcache into main's msvc rules, then deploy the staged wibo

Model: **opus**. Wave 3. The final main-wiring window. Prerequisites (verify each before
starting; if any missing, report and stop):
- Window #1 committed (msvc rule = `WIBO_FS_CACHE=1 WIBO_REWRITE_SHOWINCLUDES=1
  /home/free/code/milohax/wibo/build/release/wibo ...`, no pipe, no download edge).
- Window #2 committed (PCH rules + ~471 msvc_pch edges in build.ninja).
- W2-B passed ALL 8 gate items and delivered `~/tmp/objcache_main_wiring.patch` (the exact
  tools/project.py diff) + a populated shared cache at ~/.cache/rb3-objcache.
- W2-C left a verified staged wibo at /home/free/code/milohax/wibo/build/staging/wibo.

## Hard rules

- NEVER `git stash` / `git checkout <file>` / `git restore` / `git reset --hard` in main.
- Build ONLY via `./tools/ninja-locked`, tee to `~/tmp/rb3_build_w3a_*.log`.
- Commits pathspec-only; trailer `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`.
- MSVC objs: 4-byte COFF timestamp at offset 4 differs across recompiles; cache HITS serve
  timestamp-ZEROED objs; cross-root hits differ additionally in the one embedded /Fo path
  string. Comparators must model all three.
- ABORT DISCIPLINE: any gate failure → `objcache off` (config-file toggle — instantly
  reverts behavior to passthrough WITHOUT a re-log, by design) → diagnose in a worktree,
  never on main. Only `git revert` the wiring commit if the passthrough itself misbehaves.

## Phase A — wire the cache (one re-log)

1. Preflight: `git status --short` (tools/project.py must be unmodified), snapshot
   matched_functions (M0) from build/45410914/report.json, record `git log --oneline -1`.
2. Land the crate resolver + rule prefix: `git apply --check ~/tmp/objcache_main_wiring.patch
   && git apply ...`. The patch (from W2-B) prefixes the msvc/msvc_pch/msvc_pch_create
   commands with `objcache exec --fo $out -- ` and resolves the binary path
   absolute via the `_find_local_fork`-style resolver (upward walk → RB3_OBJCACHE_DIR →
   baked default /home/free/code/milohax/objcache/target/release/objcache). Existence-gate in
   configure.py: hard-fail with a build-it message if the binary is missing (mirrors the
   wibo wrapper gate) — if W2-B's patch lacks this, add it.
3. `python3 configure.py` → inspect build.ninja: all three rules carry the prefix; deps=msvc
   + msvc_deps_prefix unchanged; the W1-C `/Yc`//`/Yu` asserts still pass.
4. IMPORTANT — first settle with cache ON but expect mostly MISSES: main's tree state almost
   certainly differs from W2-B's worktree commit (agents landed matches since). That is fine —
   the settle populates the cache for main's state. If the box is loaded, announce/queue:
   this recompiles all ~745 objs once (command-string change).
   `./tools/ninja-locked 2>&1 | tee ~/tmp/rb3_build_w3a_settle.log`
5. Gates (on main):
   - Second `./tools/ninja-locked` → 0 recompiles.
   - **Report gate:** matched_functions == M0 (modulo documented concurrent landings — same
     reasoning discipline as prior windows).
   - **Hit-path proof:** `rm -rf build/45410914/src && ./tools/ninja-locked` → near-100% hits
     (`objcache stats`), fast compile phase (record wall time), report again == M0. This is
     the payoff scenario — record before/after numbers for the campaign report.
   - **Byte gate on 3 units:** hit obj vs a passthrough control (`OBJCACHE=off` recompile of
     the same unit to a temp /Fo) → identical except timestamp bytes 5-8.
   - **Deps gate:** `ninja -t deps <unit>.obj | grep -c '^    /'` → 0 absolute src paths
     (root-relative normalization active — this unblocks W3-B); positive/negative touch
     controls as in window #1.
6. Update CLAUDE.md: objcache section (what it is, repo path, manual
   `cargo build --release` after source edits, `objcache on|off|stats|gc`, kill switch =
   `objcache off` or OBJCACHE=off env, cache at ~/.cache/rb3-objcache, passthrough-on-failure
   semantics, "cold A/B baselines are now near-free").
7. Commit: `git commit -o tools/project.py configure.py CLAUDE.md -m "build: objcache — shared MSVC object cache on all msvc rules ..."`
   with measured numbers + the trailer.

## Phase B — deploy the staged wibo (no re-log; the binary is not a ninja input)

1. Pick a QUIET moment (no ninja-locked holder: check `pgrep -af ninja` and the lock the
   script uses). The swap is atomic and invisible to ninja; in-flight compiles keep their
   already-mmapped old binary — but don't tempt fate mid-build.
2. ```bash
   sha256sum /home/free/code/milohax/wibo/build/staging/wibo
   mv /home/free/code/milohax/wibo/build/release/wibo ~/tmp/wibo_release_prev
   cp /home/free/code/milohax/wibo/build/staging/wibo /home/free/code/milohax/wibo/build/release/wibo.new
   mv /home/free/code/milohax/wibo/build/release/wibo.new /home/free/code/milohax/wibo/build/release/wibo
   /home/free/code/milohax/wibo/build/release/wibo --version    # merge-descendant version string
   ```
3. Post-deploy gates: because ninja won't recompile anything on its own, force a probe —
   recompile 3 units with `OBJCACHE=off` (touch their .cpp; control objs) and byte-compare
   against pre-deploy controls (timestamp-only deltas). Then a full
   `rm -rf build/45410914/src && ./tools/ninja-locked` (all-hits, fast) + report == M0.
   `WIBO_FS_CACHE_STATS=1` probe prints BOTH reporter lines (the W1-D tripwire is live).
4. Note in CLAUDE.md (same commit as Phase A or a tiny follow-up with pathspec CLAUDE.md):
   live wibo now includes the residual-perf merge; rebuilds of ../wibo must go through a
   staging path + byte gate before replacing build/release/wibo (this file is invoked by
   every msvc compile in main AND all worktrees).

## Acceptance criteria

1. All Phase A gates green; the "rm -rf src → all-hits rebuild" wall-time recorded (expect
   compile phase in seconds).
2. Deps uniformly root-relative on main (the W3-B enabler) — state this explicitly in your
   report, W3-B polls for it.
3. Staged wibo deployed, tripwire live, byte gates green, previous binary preserved at
   ~/tmp/wibo_release_prev.
4. CLAUDE.md accurate for: objcache, wibo staging discipline, PCH (from W2-A), fork-rebuild
   reality.
5. Pathspec-only commits; other agents' WIP untouched.

## Rollback

- Cache misbehaving: `objcache off` (no re-log, instant passthrough). Structural revert:
  `git revert <wiring commit>` + reconfigure + settle.
- wibo deploy: `mv ~/tmp/wibo_release_prev /home/free/code/milohax/wibo/build/release/wibo`
  (atomic, instant; no ninja consequences).
