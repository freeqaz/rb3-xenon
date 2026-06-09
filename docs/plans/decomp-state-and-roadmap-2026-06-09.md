# rb3-xenon decomp — state, lever landscape & research roadmap (2026-06-09)

**Operating model:** Claude coordinates + manages context; implementation is delegated
to **workflows** (`.claude/workflows/*.js`, run via the Workflow tool). Tooling is the
product — every manual/agent pass must report tooling gaps + ideas. This is a research
feedback loop: form a hypothesis → build/run a tool → measure → record verdict.

## Current state (main `ce16bfa`)
- **6568 / 65548 functions matched** (4.93% of whole binary; 6.88% fuzzy).
  Oracle-backed tier ~8.4% matched. Session moved 6386→6568 (+182).
- Honest near-miss pool `[90,100)%` after the true_progress classifier fix
  (`50faab6`): **FRAME_ONLY 445** (funclet-gated, downstream metric), **STRUCT_WORK
  667** (real layout pool), **CODEGEN_WORK 217** (permuter-class), RECOVERABLE/CLEAN ~14.

## Lever landscape — what's TAPPED vs LIVE (hypotheses tested this campaign)

### Refuted / dead (do NOT re-attempt — proven negatives)
- **"Retail is an LTO/LTCG build hiding matches"** — REFUTED (4 signatures). No whole-
  program opt; TUs are contiguous. The asymmetry vs dc3 is the missing **map**, not opts.
- **Funclet "+615 tooling lever"** — REFUTED. FRAME_ONLY = parent-gated funclets (resolve
  free as parents match) + misclassified real work. 0 pure-name-reloc. jeff is NOT buggy
  (clamp/prune already fixed it). objdiff bl-by-name fix flips ~0. (`feedback_fuzzy_gap`.)
- **gameport via TU-cluster pinning** — DEAD for scattered band3 TUs (10/10 negative).
  Coverage-stub mirage + scattered fns; no contiguous cluster to pin.
  (`project_game_code_instrumentation`.)
- **Spatial bracketing on the rb3-Wii byte-oracle** — DEAD (114/116 known pins have 0
  oracle self-agreement; produces confident-WRONG spans).
- **Permuter on hard regalloc residuals** — ~0 even with m2c/BSF installed.
- **Struct-layout grind on top engine units** — largely EXHAUSTED (headers already
  correct; engine-easy-wins 14 agents → only the Str force-multiplier).
- **Sized-vector STLport gap** — REFUTED 2026-06-09 (calibrated worktree A/B, see
  §"sized-vector experiment VERDICT" below). Retail RB3-360 uses the classic 12-byte
  3-pointer `std::vector`; tree-wide sized flip = **NET −504** (−503/+2). Decisive:
  223 retail `?$vector@` symbols match 100% at baseline in the 2-param (ptrs) mangled
  form, not the 3-param sized form. VocalPlayer "+4" is ordinary member/base layout,
  NOT vector sizing. Do not re-attempt.

### Live levers (yield this session)
- **Engine force-multipliers** = "DC3 (newer) diverged from RB3" patterns. Three sub-types,
  each a header/cpp fix that flips MANY callers:
  1. **Inline-policy divergence** — DC3 inlined what retail kept out-of-line (or vice
     versa). `Str::operator==/!=(const String&)` out-of-line → **+6** (`ce16bfa`); the
     `DataArray::Node` keystone earlier. **HIGH EV, repeatable, under-mined.**
  2. **DC3-dropped/added member** — re-add/remove a member to match retail layout:
     `CharSleeve/CharIKSliderMidi mMe` +3, `Gem::Tail` pad +1, postproc pad +1.
  3. **Ground-truth data constants** — save/load revs: `PostProcer SAVE_REVS(1,0)→(0,0)`
     +1 (1-off, not repeatable — only instance found).
- **DC3 byte-identical fingerprint transfer / reveal** — drained this session (+161 early)
  but REFILLS as the swarm body-ports (reveal cascade).

### In-flight experiment (workflow `gameid-crossval`, run `wf_3505a3ec-069`)
- **Game-ID = improve the per-fn LABEL** (the bottleneck), then bracket TU spans.
  Two compiler-robust signals, CALIBRATED on the 25 known game pins, cross-validated:
  - **BinDiff (modified)** — source at `../bindiff` (buildable; `../binexport` cloned).
    Mods: mask 32/40-byte coverage stubs + seed the 627 pins as **fixed points** so the
    call-graph + string-ref passes propagate. Stock bindiff already produced the noisy
    `unified_id_rb3wii.json`; the mods are the untested lift.
  - **BSim** — Ghidra decompiler p-code similarity (compiler-robust; same PPC family).
- **Open research question (the gate):** does cross-arch BSim / anchored-BinDiff actually
  re-locate the 25 known pins (precision/recall)? If neither calibrates, game-ID via these
  signals is a negative result. If yes: ~15-40 game TUs / 300-1000 fns become *locatable*
  (locating ≠ matching — matching is the downstream per-fn grind).
- **Caveat in play:** RB3Xenon Ghidra project is locked by an in-progress import; the
  workflow degrades to rb3-Wii-side artifacts + reports the target side blocked if so.

## Tooling — built/fixed vs gaps
**Built/fixed this campaign:**
- `m2c` + `BSF` permuter backends installed+wired (`9763683`): `tools/objdiff_to_m2c.py`,
  `tools/compiler_trace/`, decomp-synth.json. (Validated; permuter still hit-or-miss.)
- `tools/true_progress.py` FRAME_RECON classifier fixed (`50faab6`) — honest buckets.
- Game-ID pipeline (`.claude/workflows/gameid-crossval.js`) + bindiff/binexport build path.

**Tooling GAPS / ideas to explore (the feedback-loop backlog):**
1. **Force-multiplier finder** (HIGH EV) — a tool that scans engine near-misses for the
   3 "DC3-diverged" patterns (inline-policy flips, dropped/added members via uniform
   member-offset deltas, save-rev constants) and ranks them, instead of per-unit agent
   recon. The Str/CharSleeve/Gem wins were all found by hand; automate the detection.
   Signal sources: objdiff uniform-offset-delta detection + cross-ref DC3 header inline
   bodies vs retail out-of-line `bl` (the Str pattern is mechanically detectable).
2. **Inline-policy diff** — diff DC3 headers' inline method bodies against retail's
   out-of-line/inline choice per function (detectable: target `bl <fn>` where our build
   inlined the body). This is sub-type 1 above, the highest-yield repeatable lever.
3. **Reveal-cascade refill monitor** — re-run reveal/relocate/pin as the swarm lands
   body-ports (it refills); currently manual.
4. **Game-ID tool** (gated on the calibration verdict) — if BSim/bindiff-mod calibrates,
   productionize it (`tools/`) as the standing game-TU locator.
5. **Permuter is not the bottleneck** — CODEGEN_WORK 217 is genuinely hard; deprioritize.

## Prioritized next experiments (workflow queue)
1. **Integrate `gameid-crossval` results** — pin any HIGH-confidence cross-validated game
   TUs; record the calibration verdict (science: did the signals work?).
2. **Build + run the force-multiplier finder** (gap #1/#2) → feed an `engine-forcemult`
   workflow targeting inline-policy + dropped-member flips across ALL engine units (not
   just the top STRUCT_WORK ones — Str came from DirLoader recon, not a top unit).
3. **DC3-source wiring residual** — vet the ~remaining real (non-vendor, non-Kinect)
   unwired engine `.cpp` for byte-identical clusters (UIListProvider was +9).
4. **Reveal refill** after each body-port wave.

## 2026-06-09 force-multiplier wave results (main `9150f3c`, 6576, +190 session)
Built+validated `tools/inline_policy_finder.py` + `tools/member_delta_finder.py` (`a8716c7`),
ran an apply wave → **+8 landed** (`9150f3c`):
- **NgPostProc +2** — RndPostProc carried 4 DC3-only floats (hue/blend, rev skew) RB3 lacks;
  gated behind `RB3_HAS_HUE_CONVERGE`. (member-delta finder hit.)
- **RndTexRenderer +6** — scaffolded from DC3 rev-13; retail is rev-11. Dropped mEnviron/
  mClearBuffer/mClearColor + restored bool order. (member-delta finder hit.)

### sized-vector experiment VERDICT (2026-06-09, calibrated A/B): **REFUTED**
Hypothesis was: rb3-Wii's `_STLP_USE_SIZED_VECTOR` (8-byte vector) vs our 12-byte =
tree-wide +4 force-multiplier. Ported the full machinery (rb3-Wii `_vector_sized.{h,c}`,
`_vector_ptrs.{h,c}`, dispatch wrapper, `RB3_PTRS_VECTOR` per-TU gate à la
RB3_RBTREE_0x1C) into worktree `/home/free/code/milohax/rb3-sizedvec`; sanity gate
passed (`sizeof(vector<int>)==8`). Whole-binary A/B: baseline 6576 → sized 6072,
**NET −504** (−503 regressed / +2 improved, 77 units, no unit meaningfully gained).
Decisive evidence beyond the number:
1. 223 retail `std::vector` member fns match **100% at baseline** with 2-param ptrs
   mangling (`?$vector@T V?$StlNodeAlloc@...`), not the 3-param sized form.
2. Isolated control: AccomplishmentManager `_M_erase` 100%→0%→100% flipping just that
   TU's vector layout.
3. VocalPlayer (the poster child) *regresses* under sized; its +4s are ordinary
   dropped/added-member or base-class layout. The 8-byte-stride annotation evidence
   was an artifact of adjacent small members.
MSVC breakers found mid-port (synth_xbox explicit-allocator forms, explicit
`template class vector<>` instantiations, rndobj `_M_finish` internal pokes) were
themselves corroborating evidence retail compiled ptrs. Worktree left in place
(`#if 0`'d lever + refutation comment) for reproducibility; removable.

### Walls confirmed (not levers): GameMode/Player = vbase/coupled-base hierarchy
(PropertyEventProvider→MsgSource differs RB3 vs DC3); ??_8 vbtable + ??_E/D/G adjustor
thunks. Deep multi-TU reconstruction, no clean oracle. Defer.

### Tooling gaps from this wave (feedback-loop backlog, prioritized)
1. **member_delta_finder v2** — must CLASSIFY (not flag as member-pad): (a) sized-vector
   STLport gaps (detect VECTOR_SIZE_SMALL/LARGE members + the +4/vector signature),
   (b) vbase displacement (??_8 vbtable + adjustor-thunk diffs = coupled-base wall).
   Both produced false candidates this wave (VocalPlayer, GameMode).
2. **fn_<addr> → identity resolver** for anonymous bl callees (unblocks inline tail + game-ID).
3. **struct_db / lookup_struct_offset is STALE** — reports old ObjPtr-size / rb3-Wii offsets
   (misled RndTexRenderer + Player). Refresh from current headers; reconcile with retail.
4. **Ghidra MCP (port 8002) was DOWN** all wave (3 agents blocked on struct cross-checks) —
   restart `tools/ghidra/pyghidra-service.sh` after the gameid Ghidra agent releases RB3Xenon
   (stale RB3Xenon.lock from Jun 7 to clean). Until then agents fall back to objdiff asm.
5. diff_inspect asm_listing uses wrong obj path for game units (`default/band3/...` prefix).

### Updated next-experiment queue (post-wave)
1. ~~sized-vector STLport experiment~~ — DONE, **REFUTED** (see verdict above).
2. ~~member_delta_finder v2~~ — DONE, landed `d3fc934` (validation 3/3; STRUCT_WORK
   sweep → 7 candidates: DxRnd MEMBER_DELTA actionable [agent in flight], 4 vbase
   walls defer, VocalPlayer reclassified, 1 unknown).
3. ~~fn_-resolver tool~~ — DONE, landed `013b7ae` (`tools/fn_resolver.py`; 4,146 named
   + 5,932 strong ≥0.70-conf identities; 3/5 inline-policy unknowns resolved).
4. Integrate `gameid-crossval` (rb3-Wii artifacts at `~/tmp/gameid/`; target side was
   blocked on the stale RB3Xenon.lock — lock cleared, Ghidra MCP 8002 restarted).
5. **Re-run `tools/inline_policy_finder.py` with fn_resolver wired in** — the inline
   tail is now resolvable; the Str pattern remains the highest-yield repeatable lever.
6. NEW tool ideas from the sized-vector refutation: (a) vector-layout classifier via
   objdiff symbol-name arity (2- vs 3-param `?$vector@`) — answers layout questions
   without a build; (b) `--force-fresh` full-report flag (partial-rebuild reports mix
   old/new objects and mislead per-unit A/B).

## Key refs
- Memory: `feedback_fuzzy_gap_needs_permuter`, `project_game_code_instrumentation`,
  `project_lto_icf_investigation`, `project_scope_map`, `project_engine_split_relocation`.
- Recon artifacts: `~/tmp/recon/` (funclet_classification.json, ghidra-caps/bindiff-vs-rust
  findings, spatial_pin_probe + stub detector in common.py).
- Workflows: `.claude/workflows/{engine-easy-wins,gameid-crossval,permuter-sweep-fresh,
  saverev-sweep,ultracode-levers}.js`.
