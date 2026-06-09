# Playbook: body-port wave (oracle → 100% port campaign)

**Audience:** a fresh subagent (or orchestrator) running a body-port wave. This is the
distilled formula from the 2026-05-30 → 2026-06-07 campaigns (+1500-scale across waves,
0 net regressions when this recipe was followed). The most-evolved executable form is
`.claude/workflows/bodyport7.js`; this doc is the rationale + the parts a workflow
prompt can't hold.

**What a body-port is:** a near-miss function whose diff is **logic divergence** —
different/missing instructions, wrong callee, wrong constant — fixed by porting the
real body from an oracle repo. It is NOT regalloc noise, funclet noise, or layout
deltas (those have their own playbooks/verdicts; see the defer list).

**Why waves matter beyond their own +N:** each wave refills two downstream pools:
the **reveal cascade** (newly byte-exact anonymous fns that need only a symbol-map
entry — `tools/reveal_sweep.py`) and the **inline-policy candidate pool**
(`tools/inline_policy_finder.py` — declared TAPPED at n=1 on 2026-06-09; re-check
after every wave).

## 1. Provenance (non-negotiable)
- **Game code (`src/band3/`, `src/network/`)** ← `../rb3` (rb3-Wii DEV decomp). Named
  fns + MILO_ASSERT paths. Needs MWCC→MSVC porting.
- **Engine (`src/system/`)** ← `../dc3-decomp` (same compiler, same flags, named via
  leaked map). **DC3 is NEWER** — when a fn won't match, suspect a DC3-added/changed
  member or inline-policy flip and cross-check rb3-Wii for the RB3-era intent.
- DC3 is a **false friend for game code**; rb3-Wii is the only game oracle.

## 2. Target selection
- Pool: **named (non-`fn_`) functions at 40–95%** in `build/45410914/report.json`
  (key: `match_percent_normalized` per function). Below 40% is usually a wrong-pair
  or unported TU; above 95% is usually the near-miss pool (different playbook:
  `hasreal-grind.md`).
- Rank units: `python3 tools/fingerprint_pipeline.py candidates --min-fns 4` (unported
  game TUs by oracle coverage + source presence); `tools/true_progress.py --worklist`
  for the classified near-miss pool.
- 1–2 TUs per agent. Compact panel/UI/data-mgr TUs (~0x1000–0x3000 .text) are the
  sweet spot. Big player TUs (GemPlayer/VocalPart/BandUser) scatter across the whole
  binary — per-FUNCTION work only, never span-pinning (verified negative, 2026-06-01).

## 3. Wall recognition — classify BEFORE porting, defer fast
Run `scripts/analysis/diff_inspect.py` (modes `diagnose`/`mismatches`) on each
candidate. **Defer without forcing** (note it, move on):
- **Regalloc/scheduling**: same opcodes, registers swapped → permuter-class, ~0 EV.
- **Funclet wall**: `subi r31, r12, <frame>` frame-recon + `bl lbl_<addr>` residuals →
  parent-gated, resolves free when parents match.
- **Boolean negation**: target `subic`/`subfe` vs our `extrwi` → documented
  unfixable-compiler class (`docs/decomp/patterns/INDEX.md`).
- **Coverage-stub mirage** (game code): target body is a uniform 32/40-byte breadcrumb
  stub with no `r3` use — retail STUBBED the accessor; porting the real body can never
  match. Recognize by size + shape, not by oracle similarity.
- **vbase/coupled-base hierarchy** (`??_8` vbtable, adjustor thunks) → known deep wall.
- **gRev/BinStreamRev Load-architecture conversion = REFUTED lever** (2026-06-07 campaign):
  only ~33 TUs use DC3-style `BinStreamRev`, and their near-misses are walls, not body-ports.
  The `LightPreset` conversion stalls at `operator>>` argument-passing and never matches.
  **Do not re-propose converting RB3's Load path to DC3's `gRev`/`BinStreamRev` form.**
- If EVERYTHING in a unit is a wall: report net 0 + "unit at-limit". That is a valid,
  useful result — record it so nobody re-attempts.

## 4. Worktree setup (every agent, every time)
```bash
cd /home/free/code/milohax/rb3-xenon
scripts/setup_worktree.sh /home/free/code/milohax/wt-<key> <branch-key>   # btrfs CoW; warm cache
cd /home/free/code/milohax/wt-<key>
```
- Bare `git worktree add` is UNBUILDABLE (toolchain/build inputs gitignored).
- If you must re-run `configure.py`, current main resolves the forked tools correctly;
  if it ever grabs dtk 0.3.0, pass explicit `--dtk /home/free/code/milohax/jeff/target/release/dtk
  --objdiff /home/free/code/milohax/objdiff/target/release/objdiff-cli
  --wrapper /home/free/code/milohax/wibo/build/release/wibo` (the `/tmp`-worktree trap).
- Scaffold game TUs: `python3 tools/fingerprint_pipeline.py scaffold <tu>` + add the
  `objects.json` entry, then regenerate `build.ninja`.

## 5. The per-function loop
1. `./tools/ninja-locked 2>&1 | tee /tmp/<key>_build.log` (ALWAYS tee; the
   `0x8229D660` overlap WARN is expected noise).
2. Diff one fn (`diff_inspect` or `bin/objdiff-cli`), form a SOURCE-LEVEL hypothesis
   (wrong callee → check oracle body; wrong constant → check enums/save-revs; missing
   block → un-ported branch).
3. Edit → rebuild → re-measure that fn AND the whole unit. Iterate to 100%.
4. When fns go byte-exact but read 0%/anon: reveal pass —
   `rm build/45410914/target_symbol_renames.stamp && touch config/45410914/config.yml`
   then rebuild, and run `tools/reveal_sweep.py` (+ gate + merge) for symbol-map entries.
5. Suppressed side-effect args in stripped assert paths: the per-TU
   `MILO_ASSERT/MILO_NOTIFY (void)(args)` override pattern (see Debug.h gates).

## 6. Measurement honesty (the part that keeps waves regression-free)
- Capture the worktree's **whole-binary matched count BEFORE any edit** (baseline).
- After: net = after − baseline, tree-wide, from a **fresh full report** — partial
  rebuilds mix old/new objects and lie about per-unit A/B (use `tools/fresh_report.sh`).
- `landable = net_delta > 0 AND no unexplained regressions`. A header edit that gains
  +5 in your unit and silently drops 6 elsewhere is a NET LOSS — header changes are
  the #1 cross-TU regression source.

## 7. Agent deliverable (structured, orchestrator-landable)
Return exactly (the bodyport7 schema): `key`, `ported[] {fn, before, after}`,
`net_delta`, `regressions[] {unit, delta}`, `landable`, `patch_path` (a
`git diff HEAD > ~/tmp/<wave>/<key>.patch` from the worktree), `files_changed[]`,
`notes`. NEVER commit to main; NEVER push; leave the worktree in place for harvest.

## 8. Landing (orchestrator only)
1. Per-TU path-limited commit on the worktree branch (exclude gitignored measurement
   files: `unified_id_rb3wii.json`, `candidate_spans`, `game_splits.*`).
2. `git rebase <main-HEAD>` the branch; UNION shared-file edits across agents
   (`objects.json`, `splits.txt`, `scripts/target_symbol_map.json`, shared headers).
3. Rebuild IN THE WORKTREE; verify: build green, new fns hold, prior matches survive,
   whole-binary count ≥ sum of expected deltas.
4. `git merge --ff-only` into main (main HEAD unchanged + touched files clean in main's
   tree first). Path-limited; never amend (shared-index race); don't push.
5. Extracting blobs by hand: SEQUENTIAL `git show <branch>:<path> > <path>` statements —
   never a heredoc-fed `while read` loop (it truncated 17 files to 0 bytes once).

## 9. Post-wave refill sweep (the compounding step)
After landing: `tools/reveal_sweep.py` → `tools/pin_identified.py`/relocate tools →
re-run `tools/inline_policy_finder.py` (does the pool have n≥20 clusters now?) →
`tools/member_delta_finder2.py` over the new near-misses. Update
`docs/plans/decomp-state-and-roadmap-*.md` with the wave verdict.

## 10. Hard rules (each one cost a real incident)
- NEVER touch hot widely-included headers (`math/Color.h`, `math/Utl.h`) — shifts
  MSVC inlining binary-wide. New decls go in NEW headers; engine-class members
  tail-append only, gated (`#if RB3_…` / `// 0xNNN (retail only; removed in DC3)`).
- Per-TU divergences gate via `objects.json` `extra_cflags` defines (the
  `RB3_RBTREE_0x1C` ODR-split pattern), not global flags.
- `config.json` mtime is load-bearing (re-runs the target-symbol-renamer). No mtime
  guards on the `split` rule.
- No `git stash`/`checkout`/`restore`/`reset` of files in the main repo, ever.
- Model choice: **Opus for from-scratch ports** (Sonnet verifiably fails them and
  invents wrong "unfixable" conclusions), Sonnet for reconciles/triage.
- Killed/rate-limited workflow? Harvest from disk: check every worktree for branch
  commits AND uncommitted work; a worktree `report.json` above baseline = real gain
  worth landing. SIGKILL orphaned permuter parents first, sweep children by
  `/proc/PID/cwd`.
