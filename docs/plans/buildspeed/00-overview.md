# Build-Speed Round 2 — Campaign Overview

Date: 2026-07-02. Planner: Fable. Repo: /home/free/code/milohax/rb3-xenon (main branch).
Research inputs: /home/free/tmp/wf_round2_results.json (agents WIBO-AUDIT, MSVC-CACHE,
SHOWINC, PCH-PLAN, SHOWINC-v2 — measured numbers, spot-verified 2026-07-02 by the planner).

## The situation (verified ground truth, re-checked this session)

1. **P0 regression:** `build/tools/wibo` on main is VANILLA upstream decompals/wibo 1.0.1.
   Verified: `strings build/tools/wibo | grep WIBO_` shows only WIBO_DEBUG/WIBO_DEBUG_HEAP/
   WIBO_DEBUG_INDENT/WIBO_PATH — **no WIBO_FS_CACHE**. `build/tools/wibo --version` =
   `wibo 1.0.1 (Linux x86_64)`. The `WIBO_FS_CACHE=1` in the msvc ninja rule (commit 9b938ea)
   is a **no-op on main today**. Cause: `configure.py` line ~266 `config.wrapper = args.wrapper`
   has no default → `use_wibo()` (tools/project.py:274-280) returns True → ninja download edge
   (build.ninja:24-26, tag 1.0.1) re-downloads stock and clobbers any manually-placed fork binary.
   The fork at /home/free/code/milohax/wibo/build/release/wibo (verified: reports
   `wibo 1.0.1-9-g6a7c37e`, has WIBO_FS_CACHE / WIBO_FS_CACHE_STATS / WIBO_PATH_MAP /
   WIBO_REWRITE_SHOWINCLUDES / WIBO_COMPUTER_NAME) is what **worktrees already use** —
   scripts/setup_worktree.sh:103 passes `--wrapper "$TOOL_DIR/wibo/build/release/wibo"`
   (TOOL_DIR = /home/free/code/milohax, script line 98). So main is slow AND main's msvc
   command string differs from every worktree's (relative `build/tools/wibo` vs absolute fork
   path) — which is also what forces every fresh worktree to recompile all ~745 objs once.

2. **Measured wins on the table** (from wf_round2_results.json, fork binary):
   syscalls/TU 89,643 → 10,784; at 32-wide 389 → 25 ms/TU (15.6x); ~270 s saved per full
   ~745-TU build. Fork-cache obj vs stock obj = **0 differing bytes** (matching-safe).

3. **L2 (fully-warm worktrees) is half-landed:** main's build.ninja already references
   /home/free/code/milohax/jeff/target/release/dtk and .../objdiff/target/release/objdiff-cli
   (no cargo edges); configure.py has an **uncommitted** in-flight diff making
   `_find_local_fork` prefer prebuilt release binaries in both resolution paths (byte-identical
   build.ninja in main and worktrees — load-bearing). Not done: wrapper parity (this campaign's
   window #1), .ninja_log/.ninja_deps seeding in setup_worktree.sh (which currently `rm -f`s
   them at ~line 235), fresh-worktree 0-compile verification, the commit itself, CLAUDE.md.

4. **Deps portability blocker for seeding** (planner-verified): `ninja -t deps` on main shows
   MIXED path forms — e.g. MasterAudio.obj has 262 deps: 41 absolute
   (`/home/free/code/milohax/rb3-xenon/src/...`) + 221 relative. Absolute entries pinned to
   main's tree make a copied .ninja_deps **incorrect** in a worktree (worktree header edits to
   those files would not retrigger builds). Resolution in this campaign: the Rust object cache
   normalizes all stored/replayed dep lines to repo-root-relative, and its landing (window #3)
   forces one full re-log that regenerates main's deps uniformly relative. Seeding lands after.

## Campaign shape — three waves, three main-wiring windows

A "main-wiring window" = a task that changes rb3-xenon main's build wiring
(configure.py / tools/project.py / reconfigure / settle build). **At most one per wave.**
Any msvc-rule command-string change forces a one-time full ~745-obj re-log; window #1 makes
that cheap forever after (FS_CACHE active → compile phase ~19 s at 32-wide instead of ~5 min).

### Wave 1 (concurrent)
| id | task | model | mutates |
|----|------|-------|---------|
| W1-A | Wiring window #1: fork wibo deployed + wrapper default + pipe removal + commit L2 configure.py | opus | rb3-xenon main + wibo rebuild |
| W1-B | Rust object-cache crate (`objcache`) — core implementation, standalone | opus | new sibling repo only |
| W1-C | PCH port: apply + fully verify in a ~/tmp worktree; deliver a ready patch | opus | worktree only |
| W1-D | wibo residual perf (readlink storm, negative-stat cache, stats reporter) on a branch | sonnet | wibo fork branch only |

### Wave 2 (concurrent; W2-A is the only main-wiring task)
| id | task | model | mutates |
|----|------|-------|---------|
| W2-A | Wiring window #2: land the verified PCH patch on main | opus | rb3-xenon main |
| W2-B | objcache integration dry-run: wire into a PCH worktree, byte/deps/report gates, PCH key decision, multi-worktree concurrency stress; produce the exact main diff | opus | worktrees + objcache repo |
| W2-C | Merge W1-D branch into wibo fork main; build to a STAGING path; re-run byte gate | sonnet | wibo repo only (staging binary; does NOT touch build/release/wibo) |

### Wave 3 (W3-B starts after W3-A reports success — see sequencing)
| id | task | model | mutates |
|----|------|-------|---------|
| W3-A | Wiring window #3: wire objcache into main's msvc rules + full verification protocol; then atomically deploy the staged wibo binary; final CLAUDE.md | opus | rb3-xenon main + build/release/wibo |
| W3-B | Worktree warm-state seeding (.ninja_log/.ninja_deps) in setup_worktree.sh + 0-compile verification | sonnet | setup_worktree.sh + throwaway worktrees |

## Measurement baseline (record fresh numbers at each window — these drift)

| metric | value | source / how to re-measure |
|---|---|---|
| matched_functions | 10,906 (report.json of 2026-07-02 04:28) | `python3 -c "import json;print(json.load(open('build/45410914/report.json'))['measures']['matched_functions'])"` — **moves as other agents land; each gate must snapshot immediately before its own change** |
| fuzzy_match_percent | 11.512729 | same report |
| TU count (msvc objs) | ~745 | `grep -c ': msvc' build.ninja` (approx) |
| per-TU compile, stock wibo, 32-wide | 389 ms | wf_round2_results WIBO-AUDIT |
| per-TU compile, fork+FS_CACHE, 32-wide | 25 ms | same (15.6x) |
| pipe (transform_dep.py) cost | ~29 ms/TU | SHOWINC-v2 (10-run paired) |
| PCH saving estimate | ~0.10-0.20 s/TU × ~471 eligible TUs | PCH-PLAN (must measure in W1-C step 0) |
| objcache hit target | ~5 ms (Rust) vs 0.56-3.56 s compile | MSVC-CACHE |
| obj nondeterminism | exactly COFF FileHeader TimeDateStamp, 4 bytes at offset 4 | MSVC-CACHE (control: 2 recompiles, `cmp -l` = 1 byte) |
| obj absolute-path content | exactly one string: the /Fo output path (`Z:\home\free\...\<unit>.obj`) | MSVC-CACHE |

## Risk register

| risk | severity | mitigation |
|---|---|---|
| Download edge re-clobbers the fork binary again | high (it already happened once) | Window #1 sets config.wrapper → use_wibo() False → the download edge is **not emitted at all** (tools/project.py:624-637). Plus configure.py hard-fails if the resolved wrapper binary lacks the `WIBO_REWRITE_SHOWINCLUDES` feature bytes. |
| REWRITE_SHOWINCLUDES enabled while a stock binary runs → raw Windows paths corrupt ninja deps | high | Same hard gate; pipe removal and wrapper default land in ONE commit (W1-A), never separately. |
| PCH changes codegen (pragma leak / /FI instantiation) | high | W1-C 3-gate protocol: .text byte gate on ≥5 objs, objdiff per-function equality, whole-binary matched_functions **equality** (not ≥). Instant disable: empty `pch_eligible_dirs` + reconfigure. |
| Cache serves a stale/wrong obj | critical | Direct-mode key covers compiler-DLL identity + full cflags + source + validated closure hashes; manifest validation on every hit; PCH TUs get pch identity in the key (W2-B decision); any anomaly → passthrough to real compile. Full verification protocol in W2-B/W3-A incl. report gate. |
| Concurrent agents disrupted by a wiring window | medium | ninja-locked serializes builds; land when queue is quiet; windows are ~1 settle each and post-window builds are FASTER for everyone. Snapshot matched_functions immediately before/after within the window. |
| Torn/corrupt cache entries under -j32 × many worktrees | high | Atomic publish (write temp + rename on same fs), content-hash self-check on fetch, flock-free hit path, W2-B stress test is a hard gate. |
| wibo residual changes (negative-stat cache) return stale-missing for a mid-build-created file | medium | Cache negatives ONLY under read-only include roots; never /Fo outputs or *.pch (the reason the positive cache never cached negatives). Failure mode is a LOUD C1083, not silent miscompile. Byte gate before merge (W1-D) and again on the staged binary (W2-C). |
| matched_functions baseline drift from unrelated landings mid-gate | medium | Gates compare before/after within the same quiet window on the same commit, not against a stored absolute number. |
| Seeded .ninja_deps points at main's absolute paths | high (silent staleness) | W3-B gates seeding on `ninja -t deps` in main containing zero absolute src paths (true only after window #3's re-log with objcache dep normalization). If the gate fails, seeding is skipped — current behavior preserved. |

## What is deliberately deferred (do NOT do in this campaign)

- wibo preloaded-cl fork-server (~44 s CPU/build; large architectural risk — WIBO-AUDIT vector 4).
- wibo cross-process cache persistence / daemon (superseded by the object cache — WIBO-AUDIT vector 3).
- WIBO_PATH_MAP bitwise-portability hardening (option B in MSVC-CACHE): changes obj bytes fleet-wide
  (path-derived ??_C@/__FILE__ hashes); the cache is correct without it (option A: the single
  embedded /Fo path is match-irrelevant). Revisit only with its own whole-binary A/B.
- PCH Phase 2 (band3/meta_band eligibility): low Object.h coverage, unproven on game code
  (PCH-PLAN "Phase 2"); needs its own gated campaign.
- Porting dc3 tools/compiler_trace/ wholesale (reference only; invoker.py cited where useful).

## End-state acceptance (campaign level)

- matched_functions ≥ the value at each window's start (equality required for PCH and cache gates).
- Main runs the fork wibo with FS_CACHE genuinely active (verify: `WIBO_FS_CACHE_STATS=1` on one
  TU prints cache stats; `strings` shows the feature symbols).
- No `build/tools/wibo` download edge in build.ninja.
- Full cold-baseline builds are all-cache-hits (compile phase seconds, not minutes).
- Fresh worktree full `ninja` = 0 MSVC recompiles (seeded warm state).
- CLAUDE.md documents: fork-binary reality (manual rebuild of ../wibo like ../jeff/../objdiff),
  the objcache repo + kill-switch, PCH, and the updated worktree flow.

## Task docs

- 01-wiring-window-1.md (W1-A)
- 02-objcache-crate.md (W1-B)
- 03-pch-verify.md (W1-C)
- 04-wibo-residual.md (W1-D)
- 05-pch-land.md (W2-A)
- 06-objcache-integration.md (W2-B)
- 07-wibo-merge-stage.md (W2-C)
- 08-objcache-wire-and-wibo-deploy.md (W3-A)
- 09-worktree-seeding.md (W3-B)
