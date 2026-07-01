# Post-codegen-KILL execution streams (2026-06-30)

Context: the codegen-near-miss investigation ("body-divergence wall #2") was
KILLED as a NEW-codegen-tool effort — see
`docs/decomp/research/2026-06-30-nearmiss-codegen-inventory.md`. But that diagnosis
ALSO produced concrete, executable matching candidate lists routing to the
EXISTING levers (header-struct, body-port). This doc captures those streams so
they persist for cold pickup, and tracks execution. The veins are KNOWN-thinning
(MEMORY: struct-lever isolated-single-class, bodyport mostly-spent) but the owner
asked to push them anyway — harvest what's there, report honest yields, loop to
exhaustion.

Cached inventory the streams draw from (regenerable, read-only):
- `/tmp/claude/nearmiss_inventory.jsonl` (primary class per near-miss)
- `/tmp/claude/unattributed_enriched.jsonl` (instruction-level sub-class)
- regen: `tools/classify_nearmiss_codegen.py` → `tools/enrich_unattributed.py`
  → `tools/split_imm_offset.py`

## DISCIPLINE (non-negotiable — these are MATCHING changes, not diagnosis)
- Each candidate worked in an ISOLATED CoW worktree (`scripts/setup_worktree.sh`).
  NEVER mutate main. Recon-first.
- Whole-binary composed A/B before any claimed win:
  `rm -f build/45410914/*/target_symbol_renames.stamp; touch config/45410914/config.yml;
  tools/fresh_report.sh` — run TWICE (run1==run2 deterministic), 0 unexplained
  regressions. true-100 byte-equal only; NEVER commit a partial.
- `tools/icf_alias_check.py` (no <=44B stub-fold inflation). `tools/fuzzy_progress.py`
  for fuzzy context.
- Coordinator (Opus) keeps selection/verification/landing; agents return patches.
  Land one at a time via `scripts/harvest/land.sh` + wave-loop SOP
  (`docs/decomp/handoff/wave-loop-SOP-2026-06-20.md`). After EVERY land, re-run
  `configure.py` and grep "Missing configuration for <TU>" (cross-agent
  objects.json-drop hazard that silently zeroes a landed wave).
- Sonnet ports are UNRELIABLE (5 prior incidents). Ports need HEAVY coordinator
  gating every wave (icf_alias_check + composed-verify + main-tree leak-check +
  splits overlap-check).

## STREAM 1 — STRUCT-OFFSET cascade clusters (highest EV)
Units with many IMM_OFFSET near-misses sharing a UNIFORM member-offset delta =
DC3/Wii struct drift (a member added/dropped) where ONE header fix cascades
across all access sites. Ranked candidates (delta = base−target, count):

| unit | uniform delta | sites | named | oracle | notes |
|---|---|---:|---:|---|---|
| CharEyes | +16 | 32 | 4 | dc3 (char engine) | strongest single-delta signal |
| CreditsPanel | +4 | 26 | 4 | rb3-Wii (game) | clean uniform +4 |
| GamePanel | +24 | 7 | 0 | rb3-Wii (game) | perfectly uniform, all 7 fns |
| CharIKHead | +4 | 14 | 1 | dc3 (char) | uniform +4 |
| Character | +8 | 6 | 6 | dc3 (char BASE) | base class → may cascade WIDE |
| LightPreset | ±60 | 17/14 | 8 | dc3 | symmetric = member block move/swap |
| CharDriver | +36 / +12 | 13/13 | 5 | dc3 | two deltas — coupled members |
| CameraManager | +36 / +48 | 8/6 | 5 | rb3-Wii/dc3 | multi-delta |
| HamCharacter | −96 | 7 | 1 | rb3-Wii (game) | uniform −96 |

⚠ Direction matters: must oracle-confirm whether retail ADDED a member we lack
(grow our struct) or DROPPED one we have (shrink). MEMORY warns some re-basings
REGRESS (RndMat/RndFont/RndWind are confirmed RB3-360==DC3 — do NOT re-base).
Recon must verify direction via objdiff anchor + oracle header diff BEFORE apply.

## RESULTS (2026-07-01)
- ⭐ **+2 LANDED (main @c336c46)**: FileMerger::Clear + RndParticleSys::CheckBursts
  to strict-100 (whole-binary A/B verified NET +2, 0 regressions). Both DC3-drift
  reverts (arg-count / POD-ctor). **The bodyport recon hypotheses are RELIABLE** —
  unlike the struct-cascade (below).
- ⛔ **CharEyes struct-cascade = NET +0 (DISCARDED)**: the +16 IMM_OFFSET "delta"
  was NOT a removable-member drift. Shrinking CharEyes (mDartOffset #ifdef
  MILO_DEBUG) made near-misses WORSE (EnforceMinimumTargetDistance 99.87→97.37) —
  retail-360's CharEyes HAS that 0x10 (the rb3-Wii MILO_DEBUG guard does not apply
  to retail-360). ⚠ LESSON: IMM_OFFSET "uniform delta" clusters are NOT reliable
  struct-lever candidates — the classifier sees immediate diffs that are a minority
  of the residual, not a clean cascadeable member drift. Prefer the bodyport
  recon's mechanism-level analysis over the classifier's offset-delta heuristic.
- ⏳ SortNodes (+6, Data.h 1-arg→0-arg) building (full recompile).
- STREAM 1 (struct-offset clusters) DE-PRIORITIZED after CharEyes disproof; the
  IMM_OFFSET buckets need per-fn mechanism recon (like Stream 2), not the
  offset-delta heuristic, to avoid false positives.

## STREAM 2 — BODY-divergence per-fn ports (bodyport lever)
Genuine oracle logic/guard/arg divergences (port the real body from the oracle
to strict-100). Best non-STL named candidates:

| fn | unit | pct | oracle |
|---|---|---:|---|
| MusicLibrary::UpdateHeaderData | MusicLibrary | 95.74 | rb3-Wii |
| AppChild::Poll | AppChild | 97.92 | dc3 |
| RndRenderState::Init | RenderState | 94.74 | dc3 |
| FileMerger::Clear | FileMerger | 97.67 | rb3-Wii/dc3 |
| RndParticleSys::CheckBursts | Part | 95.06 | dc3 |
| RndBitmap::SaveBmp | Bitmap | 90.62 | dc3 |
| CharBoneDir::GetContextFlags | CharBoneDir | 99.38 | dc3 |
| EditSetlistPanel::SetEditState | EditSetlistPanel | 93.55 | rb3-Wii (game) |
| FftIpp::~FftIpp | FftIpp | 97.96 | dc3 |
| RndRenderState / RenderState::Init | RenderState | 94.74 | dc3 |

(STL `??$?5...` operator>> template instantiations DEFERRED — body lives in a
shared header, wide-breakage risk, divergence often regalloc not logic.)

## STREAM 3 — classifier mislabel reroute (cheap, residual)
~16–22 near-misses my classifier put in PEEPHOLE/IMM_OFFSET/REGALLOC whose REAL
residual is a struct-size(divw/mulli sizeof) / member-type(lfs vs lwz) delta or a
logic/guard body divergence. Re-route them into Streams 1/2. The validation agents
flagged these; mine `unattributed_enriched.jsonl` for PEEPHOLE entries with
`divw/mulli/lfs` transitions.

## STREAM 4 — dormant FPR permuter drivers (LOW EV, optional)
Wire `fpr_declaration_reorder` + `first_use_reorder` (0 runs, exist in
`../decomp-synth`) into the active scan set; one bounded pass on the 5
REGALLOC_FPR_CALLEE (CheckBSPTree/FastInvert/Rot::MakeScale). Predicted 0–3.
Touches shared `../decomp-synth` → isolate + do-no-harm. Deprioritized.

## EXECUTION LOG
- 2026-06-30: streams identified + documented. Codegen-tool effort KILLED.
  Wave 1 launched: Stream 1 recon→apply on the top struct clusters.
- 2026-07-01: Wave-1 struct-cascade results (⚠ ran concurrent with a heavy owner
  bodyport wave → IO storm load 200+ blocked/rate-limited several lanes):
  - **CharEyes +~32 READY**: edit applied+dual-oracle-confirmed in wt-s1-CharEyes
    (mDartOffset is MILO_DEBUG-only → wrap in #ifdef; our retail carries it = +0x10
    drift; CharEyes-OWN, no cascade; retarget line 912 write to mCurrentDartOffset
    using the existing 626/640 cast idiom). Verification storm-blocked → COORDINATOR
    to A/B + land. Independently sanity-checked read-only: sound.
  - **CharIKHead DEFER**: the +4 is a shared-base (RndPollable/CharWeightable)
    vtordisp/virtual-decl drift = the RndMat/RndFont re-base hazard. High regression
    risk, low EV. Not a clean member edit.
  - **Character / GamePanel / CreditsPanel**: apply agents rate-limited (transient),
    incomplete → re-run recon+apply at LOW concurrency later.
  - Bodyport-recon (read-only) landed 5 PORTABLE_WINs (saved
    ~/tmp/bodyport_recon_results.json): CharBoneDir/SortNodes Data.h 1-arg→0-arg
    (+6 cascade, shared header), FileMerger::Clear bool→no-arg (+2), Part Burst POD
    (+1), EditSetlistPanel VerifyStrings case-4 (+1), MusicLibrary inline-policy
    (+1/2). Plus 4 confirmed DEFER_CODEGEN (AppChild/RenderState/Bitmap/FftIpp) =
    more corroboration of the wall-#2 KILL.
  - ⚠ LESSON: never run my heavy build-wave concurrent with the owner's active
    wave (14 concurrent builds = load-200 storm that starves ALL builds). Switch to
    COORDINATOR-DRIVEN SERIAL landing in ONE worktree (incremental A/B), light
    footprint. Note: clean HEAD now builds to ~10682 (owner landed +18); measure
    NET via in-worktree A/B, not the absolute count.
  - ⚠⚠ CRITICAL ENV UNBLOCK: fresh worktree builds FAIL at the `build/compilers`
    ninja edge — it runs `tools/download_tool.py` which tries to download the MSVC
    toolchain from files.decomp.dev and dies on `SSL: CERTIFICATE_VERIFY_FAILED`
    (no cert path in this env; certifi retry also fails). ninja rebuilds the edge
    because a fresh worktree has no .ninja_log entry (NOT mtime — touching doesn't
    help). This silently blocked the whole struct apply wave (stale-report false
    +0, "frozen priming"). FIX (per-worktree, do NOT commit — this is the
    download_tool.py the verify-stage-wave skill says to EXCLUDE): short-circuit
    download_tool.py `main()` to `return` early when the output already exists
    (setup_worktree symlinks build/compilers from main). Apply this in EVERY
    worktree before building. TODO: bake into setup_worktree.sh (shared-tool change).
  - ⚠ PROCESS-HYGIENE LESSON: `pgrep -f 'wt-s1-CharEyes'` MATCHES MY OWN SHELL
    (pattern in the command line) → `kill` self-terminates the command (exit 144 /
    no output). Never pattern-kill on a string that appears in the kill command;
    kill by explicit PID. Also: when taking over an agent's worktree, its leftover
    verify script keeps building → two drivers fight the ninja lock → stale objs /
    corrupt build dir. Recreate the worktree clean instead of fighting it.
