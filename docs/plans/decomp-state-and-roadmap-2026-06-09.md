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

### gameid-crossval experiment — **CLOSED NEGATIVE** (2026-06-09, calibrated)
Research question was: does cross-arch BSim / anchored-BinDiff re-locate the 25 known
game pins precisely enough to bracket TU spans? **NO — neither signal calibrates.**
- BinDiff (fresh source-built): per-fn precision 0.54 @ conf≥0.97, recall ≤0.023,
  max contiguous correct run = 3 fns. BSim (cross-arch): precision 0.32, recall ≤0.064.
- Root cause = the coverage-stub mirage, now CONFIRMED for BSim: 6112 of 6759
  top-matches to one rb3-Wii class are 32-byte coverage stubs at sim=1.0. The
  distinctive minority (~2-13%/TU) matches; the stub-shaped filler doesn't → no
  contiguity → no spans. Stub-masking/fixed-point BinDiff mods would NOT fix this
  (the negative is structural, not matcher tuning) — do not build them.
- **Salvage (the useful product): `docs/decomp/gameid/crossval_agree.json`** — 146
  BinDiff∩BSim cross-validated per-fn game-code identity hints @ 0.95 precision
  (93 not yet pinned). Usable as per-function LABELS for manual matching / as an
  fn_resolver evidence tier; cannot bracket spans. Full verdict + repro commands:
  `docs/decomp/gameid/VERDICT.json`; bulk artifacts remain at `~/tmp/gameid/`.
- Infra fixed during the run: Ghidra dist now extracted persistently
  (`../ghidra/build/ghidra` → `ghidra-dist/ghidra_12.2_DEV`, no longer via /tmp);
  Ghidra MCP 8002 Ready.

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
5. ~~Re-run inline_policy_finder with fn_resolver~~ — DONE. Resolver integration landed
   (`tools/inline_policy_finder.py`). **VERDICT: inline-policy lever TAPPED for the
   current near-miss pool** — all [90,100) candidates are n=1 (single caller); the
   historic wins (DataArray::Node +37, Str +6) were n=20+ clustered. MakeShortAng
   inline test = NET −1, reverted. Candidate lists archived at
   `~/tmp/forcemult/inline_candidates_v2*.json`. Re-check only after the pool refills.
6. ~~data-symbol renaming (LBL_RENAME)~~ — **REFUTED** (verify-first gate, full
   probe of all 771 [99.9,100) fns + empirical rebuild). LBL_ONLY = **0%**; true
   composition: **42.8% HAS_REAL** (genuine STRUCT_OFF/REG/OPCODE work), **36.6%
   EH-funclet `bl lbl_<frameless dtor>` + frame-recon** (= the already-refuted
   funclet wall; mapping the dtors resolved the bl diffs in all 282 and flipped 0 —
   the `subi r31,r12` frame residual independently blocks every one), 13.9% anon fn_
   callee, 5.1% named mismatch. Genuine data labels (44 fns) are all `__real@` float
   pools / local statics co-occurring with real codegen diffs — never the sole
   blocker. **Do NOT extend the renamer to data symbols (yield ~0).** Root cause of
   the false hypothesis: true_progress's NAME_RELOC class conflates bl-lbl/code,
   data-lbl, anon-fn, named-static (tool gap; a lbl_probe.py classifier existed in
   the removed worktree — rebuild if the question recurs).
7. Other gaps from the wave: UIComponent vtable +4 slot (5 confirmed fns; known wall,
   docs/plans/ui-base-layout-reconstruction.md); n=3 unnamed shared ctor body
   (CharBoneOffset/CharIKFoot) needs Ghidra+DC3 pair to identify.
8. NEW tool ideas from the sized-vector refutation: (a) vector-layout classifier via
   objdiff symbol-name arity (2- vs 3-param `?$vector@`) — answers layout questions
   without a build; (b) `--force-fresh` full-report flag (partial-rebuild reports mix
   old/new objects and mislead per-unit A/B).
9. DxRnd MEMBER_DELTA — DONE (+1, `5770d95`+`22b5148`, 6577): mColorRampTex @0x394 +
   DoPostProcess GPR literals; Present residual = boolean-negation unfixable class.

## 2026-06-09 (evening session) — playbooks + tool-gap closure + 2 more vein verdicts

Theme: **codify the subagent formulas** (docs/decomp/playbooks/) + close the tooling
backlog. 6 agents, all landed. Matched count unchanged at **6577** (verified honest by
the new fresh_report.sh: 6542 raw fuzzy==100 + 35 FP-anchor-normalized).

**Playbooks established (`docs/decomp/playbooks/`)** — the in-repo subagent formulas:
- `bodyport-wave.md` (`178a758`) — the body-port wave recipe distilled from the
  +1500-scale campaigns (provenance, wall-defer list, per-fn loop, measurement
  honesty, landing protocol, hard rules).
- `hasreal-grind.md` (`008e53c`, from the grind pilot) — metric trap (§0), 8-signature
  wall-recognition checklist (§3a–3h) with exact asm, offset root-causing tree,
  time-boxing. **Pilot verdict: 0/12 hand-examined named near-misses were clean
  source fixes** — the pool is walls+permuter-class; do NOT swarm it raw.
  Also: UtilDrawAxes is ALREADY MATCHED (vtable-slot reloc normalized away;
  `run_objdiff` live % ≠ report `match_percent_normalized` — always rank by the
  report metric).

**Tools landed:**
- `b47025f` true_progress: NAME_RELOC split into BL_LBL_FUNCLET / ANON_FN_CALLEE /
  NAMED_MISMATCH / DATA_LBL + HAS_REAL bucket + `--worklist`. Reproduces the 771-fn
  probe exactly (330/282/107/39/0). Worklist: ~/tmp/hasreal_worklist.json (bands on
  normalized %, so free of the §0 trap).
- `b935d9e` fn_resolver T3b gameid_crossval tier: +69 new best-identities, 24
  agreements, 6 conflicts (named wins; ~5% FP tail as expected).
- `f82ed7f` fresh_report.sh + vector_arity.py (861 arity-2 / 0 arity-3 binary-wide —
  sized-vector refutation now a one-command check) + asm_listing obj-path fix
  (`default/` → `src/`; broke /FAs for ALL units) + struct_db refreshed (current
  headers; MCP lookup_struct_offset serves it directly).
- `91b50c9`+followup wall_classify.py — auto-tags the 330-fn worklist with the
  playbook §3 signatures (11/11 ground-truth validation). FIRST EXECUTION taught it
  a new wall class: the headline "Rnd +4@0x54 cluster (44 fns)" was a **funclet
  address-pairing artifact** (r31 from `subi r31,r12` = frame slot, not `this`;
  dtk paired structurally different fns — body sizes 0x78-vs-0x54, `new` sizes
  0x1a4-vs-0x20c). Playbook gained §3i + §4 "gate zero"; classifier gained
  FUNCLET_PAIRING detection (direct-vs-indirect frame-reg access distinguishes
  artifact from real). **Corrected routing of the 330: MEMBER_DELTA 69 (the honest
  residue), FUNCLET_PAIRING 120, DEFER_VBASE 107, PERMUTE 9, UNKNOWN 19, other 6.**
  Output: ~/tmp/hasreal_routed.json.

**Banked deferred lever — CameraShot +25:** real uniform +4 member delta (retail has
a 4-byte member at ~+0x40 we lack; shifts a 9-member ObjPtrList<RndDrawable> chain
+0x44..0xdc stride 0x14). Owning class = vtable `0x82077150` (copy-ctor fn_824AB3E0);
offsets do NOT match the CamShot header → distinct/sub-object layout, needs Ghidra
reconstruction. Handoff: ~/tmp/rnd54_findings.jsonl. No guessing into shared headers.
Other surviving high-conf MEMBER_DELTA: ??_G deleting-dtor adjustor-delta family
(CharInterest −68, Waypoint +592, HamCharacter +96, RndShockwave +724, …) and
ChunkStream +2084 (size-divergence class).

**Vein verdict: unwired-DC3-engine residual = DRAINED** (`c13507f`,
tools/dc3_residual_rank.py + docs/decomp/dc3-residual/ranked.json). 115 unwired
engine TUs; content-match ceiling 57 fns total (2–10%/TU). The one real contiguous
cluster (PlatformMgr_Xbox, 109 fns) diverged too far to wire (DC3 SmartGlass/store
era; C1083 + 282-vs-109 fns). 3 "clusters" were trivial-collision noise (the ranker's
promiscuity+contiguity filter catches these). Roadmap queue item 3 CLOSED.

**New tool gaps (backlog):**
1. struct_db parser ingests both `#ifdef HX_NATIVE` branches (last-write-wins
   fragility).
2. `global_fuzzy_pairs.json` and `dc3_content_match.py` source from DIFFERENT DC3
   obj dirs and disagree on coverage — unify to one DC3 obj source.
3. dc3_content_match default obj dir non-recursive (misses the 811 per-TU system/
   objs).
4. Ghidra MCP needs access serialization (single-process; ClosedException under
   concurrent agents).
5. true_progress worklist `address` field is section-offset decimal — parse the
   `fn_<hex>` from sym for load addresses.

**Where the next +N comes from (post-pilot view):** (a) wall_classify's
MEMBER_DELTA_CANDIDATE + UNKNOWN residue = the real grind pool (likely far smaller
than 330); (b) PERMUTE-routed bucket via /permute (sanctioned, no hand-edits);
(c) body-port waves per the playbook (still the standing campaign — the named
40–95% pool, NOT the near-miss pool); (d) reveal refill after each wave.

## Key refs
- Memory: `feedback_fuzzy_gap_needs_permuter`, `project_game_code_instrumentation`,
  `project_lto_icf_investigation`, `project_scope_map`, `project_engine_split_relocation`.
- Recon artifacts: `~/tmp/recon/` (funclet_classification.json, ghidra-caps/bindiff-vs-rust
  findings, spatial_pin_probe + stub detector in common.py).
- Workflows: `.claude/workflows/{engine-easy-wins,gameid-crossval,permuter-sweep-fresh,
  saverev-sweep,ultracode-levers}.js`.

---

# Session addendum 2026-06-09/10 (late night): first full playbook-driven wave

Coordinator-orchestrated wave of 9 Opus/Sonnet agents executing the playbooks.
**6577 → 6586 matched (+9, zero regressions), 9 commits (5b6b7a6..dc080dd), plus
two major lever discoveries and three permanent refutations.** Operating model
confirmed: coordinator selects/lands/verifies only; ALL implementation (even small
tool fixes) delegated to agents (user directive, memory `feedback_coordinator_role`).

## Matches landed (+9)
- **FileMerger +7 cascade** (`dc080dd`): dropped DC3-added `Merger::filler`
  Symbol@0x4 (rb3-Wii confirms retail lacks it) — cascaded across FileMerger +
  BandWardrobe. The "DC3-added member RB3 lacks" pattern strikes again.
- **ColorPalette +1** (`60eabed`): `ColorSet` padded 0x20→0x44 (retail-only
  trailing non-serialized storage; DC3/rb3-Wii shrank it).
- **Mesh +1** (`b766775`): `CompressedVertex_Xbox.mPosX/Y/Z` were `int`, retail+DC3
  type them `float` (+rest `unsigned int`) — wrong types routed `BinStream<<` to the
  int writer. Plus `817b8a5` BSPFace::Update 80→94.6 (oracle-faithful, residual is
  FP-ordering) and `f97da90` SetBloomBlurWeights PShaderConstant base **0x2f**
  (retail; DC3 moved it to 0x9a) — banked, flips +1 with a future .rdata
  array-layout fix.

## REFUTED: CameraShot "+25" banked lever → VBASE_WALL (`fe0aaaa`)
Ghidra RTTI proof: retail `CamShot` is the **DC3-style virtual-base MI layout**
(base-class descriptor vbase displacement 0x1a0; RndAnimatable + RndTransformable
both deriving `virtual Hmx::Object`). The +4 IS DC3's `mShotStartedPending` —
retail DOES carry it — but our header is flat single-inheritance, and re-adding the
member is **verified net −3** (+7 funclet fns, −10 most-derived reads; two
incompatible base anchors). Only a full MI/vbase reconstruction wins this (deep,
out of scope). The handoff's "vtable 0x82077150 / fn_824AB3E0" attribution was a
sub-object red herring; real owner = `~CamShot` fn_824B29B8. Header comment now
records the wall. **New gate-zero rule: frame-recovered `this` that is then
vbase-adjusted (`subi 0x1a0`) = VBASE_WALL, not MEMBER_DELTA.**

## Grind-wave calibration: MEMBER_DELTA route was ~87% polluted
48 non-CameraShot candidates ground per playbook → 2 real (the +8 above), 42 walls:
24 FUNCLET_PAIRING (every size-44 `fn_` __unwind funclet; divergent `bl` callee =
the tell), 10 VBASE (the ??_G deleting-dtor adjustor family — non-uniform per-class
deltas, virtual inheritance, NOT member-addable; kills that queue item), 5
**VTABLE_DIVERGENCE** (new class: vcall slot-load deltas — DC3 added 8 virtuals to
Rnd, 20 to RndEnviron, that retail lacks), 3 PERMUTE (ChunkStream "+2084" was an
offset-SWAP misparse; real member deltas are uniform SAME-SIGN), 1 DC3_REV_MEMBER
(Mat_NG +0x3c — material-chain rev12/13 block, TexRenderer/PostProc precedent),
1 no-oracle (Singer/VocalPlayer mVocalParts). Ground truth:
/tmp/mdgrind_abandoned.jsonl; corrections: ~/tmp/mdgrind_wall_classify_corrections.md.

## Body-port wave verdicts (pool nearly dry at the top)
- **rndobj/Utl + LightPreset: net 0, unit at-limit.** All [40,95) named fns are
  permuter-class regalloc, §3i structural mispairs (LegacyLoadP9, Save@Keyframe),
  codegen-shape (EnvironmentEntry op!=), or split artifacts (PreMultiplyAlpha
  truncated at 32B). Wave agent independently re-derived the **gRev/BinStreamRev
  conversion** idea → already REFUTED 2026-06-07 (only 33 TUs use DC3-style
  BinStreamRev; their near-misses are walls). Playbook defer-list updated so waves
  stop re-proposing it.
- **Geo/Mesh/MemTracker: +1.** Geo = FP-scheduling permuter-class throughout.
  MemTracker = ONE shared wall (all 6 MemDiffEntry heap fns inline `operator<`
  whose mSizeDiff tiebreaker compiles to the overflow-safe signed-compare idiom —
  boolean-negation class; NOTE our tiebreaker is CORRECT for RB3, DC3's simpler
  form is the wrong oracle there).

## Tooling landed
- `40deb7d` **DC3 obj-source unified**: canonical = `.dc3_text_scratch/named/obj`
  (retail TARGET tree; the compiled-port tree substitutes dc3-decomp's own
  unmatched ports as "truth" — 20 content diffs, all in <92%-matched units). New
  shared tools/dc3_obj_source.py; deterministic ICF tie-break (352 contradictory
  identities → 0, was nondeterministic run-to-run); non-real-name filter
  (`merged_*`/`__unwind`). **Data debt: live global_fuzzy_pairs.json holds dead
  `merged_*` top-identities + 746 dup rows — regenerate (`global_fuzzy_index.py 64
  0.85`) at wave end. T4 fuzzy labels = ICF-ambiguous, never authoritative.**
- `bf0d039` **struct_db gated members**: guard/guard_kind columns (45 real gated
  members, 9 files: HX_NATIVE forks, RB3_RBTREE_0x1C, RB3_HAS_HUE_CONVERGE,
  MILO_DEBUG); lookup returns all variants tagged, retail leads; MCP reader
  guard-aware. DB regen deferred to wave end: `python3 tools/struct_db.py build src/`.
- `5b6b7a6` **true_progress load_addr**: report `address` is NOT base+offset
  recoverable (dtk drops alignment padding from cumulative offsets!) — resolve via
  fn_ self-encoding + inverted target_symbol_map gated by splits ranges (99.2%).
- `tools/fresh_report.sh` OOM: full-parallelism builds get code-137 killed on fresh
  CoW worktrees (32 cores × MSVC-under-wibo); use `ninja -j 12` (fix in flight).

## Worktree pool cleaned
9 stale prior-session worktrees (fa-*, fm-*, xfer-recover) triaged read-only:
all fully harvested (0 ahead-commits; uncommitted files byte-identical to main or
strictly older) → removed. `rb3-sizedvec` KEPT (sized-vector refutation repro copy).

## In flight at time of writing
- **flowback agent**: wall_classify gains divergent-bl/same-sign/vtable-slot/vbase
  gates (validate vs original 11 + the 48-fn confusion matrix) → regenerates
  ~/tmp/hasreal_routed_v2.json; playbook §3/§4 updates; fresh_report.sh -j cap.
- **vtable-lever agent**: slot-by-slot Rnd/RndEnviron RB3-vs-DC3 vtable comparison
  (dump_vtable.py + Ghidra + ham_xbox_r.map), drop DC3-added virtuals if A/B clean.
- Queued: Mat_NG +0x3c rev-member lever; struct_db + global_fuzzy_pairs regen;
  reveal refill sweep (incl. +2 byte-exact MeshAnim anons 0x8245BC78/0x8245DA30).

## VTABLE_DIVERGENCE lever EXECUTED: Rnd +10 @100 (`30a4ae8`, 6586→6596)
Slot-by-slot localization WITHOUT a pinned retail Rnd vtable: used accumulated
slot-offset deltas in CALLERS as anchors (ScreenDump +0 → DrawRect +1 → GetFrameID
+5 → SetShadowMap +6 → DoWorldEnd +8). The 8 DC3-era virtuals retail lacks:
`ScreenDumpUnique` (slot29), `GetSync`/`NumDrawPasses`/`BeginDrawPass`/`EndDrawPass`
(37–40), `ShouldDrawPanel` (42), `Push`/`PopClipPlanesInternal` (60/61); rb3-Wii
confirms none are virtual. Fix idiom: `RND_DC3_VIRTUAL` macro gates `virtual` behind
HX_NATIVE (native keeps dispatch, matching build drops slots) — same as the existing
ClearDepthForOverlay gate at rndobj/Rnd.h:170. Cascade: EndWorld, DrawStringScreen,
DrawRectScreen, RndFlare::SetPointTest, RndShadowMap::EndShadow,
SpotlightDrawer::UpdateBoxMap, 3 Utl draw helpers, CalibrationPanel::Exit.
NOTE: earlier "Clear is the first insertion" hypothesis DISPROVEN (ScreenDump +0
anchor); machine-code anchors beat source diffing for vtable work.
**Deferred from this lever (different root causes):** RndEnviron 20-slot delta =
secondary RndTransformable-base vtable under VIRTUAL inheritance (the foundational
RndHighlightable/RndTransformable vbase wall, §3a — not droppable virtuals);
TrainerGemTab::DrawStartFinish = separate 15-slot UILabel/UIComponent-MI delta
(the documented UI base-layout wall, docs/plans/ui-base-layout-reconstruction.md).

## Mat_NG DC3_REV_MEMBER lever: DEFERRED — retail material layout is SCRAMBLED, not block-shifted (`424412b`)
The SetupShader +0x3c "clean delta" is only the tail facet. Ground truth =
`SetRegularShaderConst` (34 member touches): retail RndMat/BaseMaterial is
**reordered AND bool-repacked** vs our DC3-derived headers — 34 this-relative
deltas with OPPOSITE signs (−188..+120). Retail packs bool flags low/tight
(0x44/0x54/0xc2/0xc3, like rb3-Wii's packed-bitfield block); DC3 scattered them
as bytes high in the class. NOT fixable by gating a 0x3c block in widely-shared
Mat.h. Needs a dedicated multi-session retail-layout reconstruction validated
against SetRegularShaderConst until all deltas zero. Full RETAIL↔OURS offset
table: docs/decomp/matng-deferral.md (+ evidence rows matng-abandoned.jsonl).
Header `// 0xNN` comments in Mat.h are STALE (mDirty says 0x228, compiles 0x188).

## Refill sweep: +255 (6596→6851, 0 regressions) — the compounding loop delivers again
(`e6769d3`+`36d5599`) pin_identified extended 33 under-pinned units (+50 jaccard=1.0
byte-exact addrs; top: BandCamShot +50, CharHair +37, CharDriver +27, Tex +25,
Flow +15) and two reveal waves added 39 safe map names (35→11 + 54→28 after gating;
3rd wave = 0, cascade drained at this state). Honesty gates verified: per-unit A/B
27 gained / 0 dropped; denominator FELL (65548→65544); flagged anon-zero runs all
pre-existing; the 2 MeshAnim seeds were correctly REJECTED (name_collision_tsm —
their mangled names live at other addrs).
**Pool verdicts after refill:** inline_policy STILL TAPPED (17 candidates in
[90,100), all n=1; the n≥2 clusters in [40,90) are ObjPtr/Symbol template ctors in
forbidden hot headers). member_delta_finder2: 0 actionable (1 sized-vector DRAINED
class, 6 vbase-wall, 1 low-confidence n=1).
**Tool gaps:** member_delta_finder2 default `--bucket STRUCT_WORK` scans 0 fns
(true_progress emits `HAS_REAL`) — align default; the pin→build→reveal→gate→merge
loop needs a single driver (`tools/refill_loop.sh`) with the honesty A/B built in.

## Wave-close tooling (final round)
- `6862880` global_fuzzy_index residuals: `jumptable_` filter (394 rows; dtk
  jump-table blocks saturate masked-jaccard as shingle-subsets), RB3 ICF-input
  dedup (92,596 fn entries / 50,517 unique addrs → 2,100 identical output rows),
  size-ratio gate [0.33,3.0] at emit (fn_resolver T4 has no size awareness).
  Artifact regenerated + validated: 2000 rows, 0 non-real / 0 dups / 0 bad-ratio,
  1781 strong (j≥0.97 same-size).
- `ebff84b` `tools/refill_loop.sh`: the +255 manual loop is now one command with
  the honesty A/B built in (exits 1 on any dropped unit; reuses tools/ab_measure.py).
  member_delta_finder2 default bucket STRUCT_WORK→HAS_REAL (the silent-0 bug) +
  loud 0-row warning. Playbook §9 updated with the driver one-liner.
- struct_db + global_fuzzy_pairs both regenerated on the fixed tools (tasks 10/11).

## Wave-close verification (main, fresh full build): **6851 / 65544 matched** ✓
Independent main-tree fresh_report.sh confirms the composed wave result exactly
(raw fuzzy==100 is 6816; +35 FP-anchor-normalized = the known objdiff-fork gap,
unchanged). Session: 6577 → 6851 (+274), 0 regressions, 23 commits.

---

# ADDENDUM 2026-06-10 — wave 2 (research-first model)

**Operating model update (user directive):** Fable subagents run research as the
first phase of every wave; Opus implements; Sonnet does mechanical work. Docs are
the handoff artifacts between phases. Permuter DEPRIORITIZED — targeted
single-fn/unit subagent only, and only when blocking other work (PERMUTE bucket
~14 fns stays parked).

## Wave-2 research (3 Fable agents, `7767627`) — docs/decomp/research/2026-06-10-*.md
- **bodyport-pool.md**: 261 named fns @40–95 across 146 units post-exclusions.
  Launch list: BinStream (DC3 AutoGlitchReport RAII = the divergence; retail body
  is the rb3-Wii form — HIGH), Rnd 5 fns (layout pre-fixed; DrawTimers needs the
  rb3-Wii body not DC3's rewrite), Gem 3 fns + GemManager (top unported
  fingerprint candidate) + GuitarController::Handle, OvershellSlot/VocalTrackDir.
  DO-NOT-LAUNCH: SHA1/Quazal-MD5/FFT (regalloc-saturated), vorbis mapping0/psy
  (library-version delta).
- **routed-residue.md**: of 27 MEMBER_DELTA+UNKNOWN routed fns, 8 already matched,
  19 live → 6 actionable. HEADLINE: **default/MidiInstrument is unit-level
  MIS-PINNED** (pinned range 0x822B0C60–0x822B3D28 is really the BandIKEffector/
  CharSignalApplier TU — vector elem stride 0x1c = BoneOp; true MidiInstrument
  cluster sits unmatched in auto_03_826F42A8_text, ~169 fns). Levers: RndTex::unk2c
  drop (+4 uniform, rb3-Wii lacks it), WorldCrowd mCharForceLod+unkd0 gate (+8
  delta @0x90→0x98), AccomplishmentProgress RB3_RBTREE_0x1C, CamShot
  _Destroy_Range map-label SWAP (mirrored ±232) + 2 dtor reveals, Synth −0xc
  (needs bracketing). Parked w/ evidence: RndEnvironTracker (20-slot wall,
  misrouted), Shader::Select (→ Mat_NG scramble; added as matng validation fn),
  Locale (DATA_LBL tooling class), yylex (flex DFA version delta). +5 new
  classifier patterns for wall_classify v3 documented in-doc.
- **force-multipliers.md**: custom vcall-slot sweep found **36 vtable-slot-delta
  sites wall_classify entirely missed** (its vtable gate reported 0 — tooling gap).
  Levers: RndAnimatable::OnListFlowLabels DC3-only virtual (drop → fixes uniform
  +4 on EventTrigger::Trigger, 5 anchors ≥99.96 + 42-site cascade),
  PropKeys::RemoveRange DC3-only virtual (8 slots uniformly +4 in
  PropAnim::ValueFromIndex bracket insertion @0x28), retail RndDrawable::Draw is
  NON-virtual + no DrawShadow (direct-bl proof in RndDir::DrawShowing; UIComponent
  +4 interplay must be A/B'd), Player base-chain plain-this −4 spanning
  Player/VocalPlayer/Singer ~10 fns (retail mVocalParts@0x390 confirmed),
  OnClearColor = target_symbol_map off-by-one (3 addrs, not a member delta),
  Rnd::Terminate emits DC3-added DOFProc::Terminate call. Re-confirmed:
  inline-policy tapped (all n=1), mdf2 0 actionable (rows collapse into the
  Player-chain lever).

## Wave-2 implementation batch 1 (launched, wf_9bb5eea6-0ab)
5 worktree agents: mech-map (Sonnet: OnClearColor remap + CamShot swap/reveals +
AccomplishmentProgress flag), lever-anim (OnListFlowLabels + RemoveRange),
lever-tex-crowd (Tex unk2c + Crowd gate), reloc-midi (verify-first re-pin),
bodyport-binstream-rnd (+ DOFProc gate). Held for batch 2: Draw devirt (risk),
Player-chain bracket, Gem/GemManager, OvershellSlot/VocalTrackDir, Synth bracket,
PlatformMgr head. Per-lever A/B + worktree-branch commits; coordinator lands.

## Wave-2 batch 1 LANDED + verified: **6880 / 65544** (+29, 0 regressions, 8 commits)
Composed fresh build on main confirms the independent per-lever A/Bs compose
exactly (6851+29; raw 6816→6845 in lockstep; FP-anchor gap still 35).
- `4d3dddf` WorldCrowd drop DC3 mCharForceLod+unkd0 (+9 — cascade across
  CamShotCrowd template/accessor family; dossier est was +1..+4) + `1d855ee`
  RndTex drop DC3 unk2c CRC (+8; 84-file fan-out, zero regressions). Gate idiom:
  `#ifdef RB3_WORLDCROWD_DC3_REV` / `RB3_RNDTEX_DC3_CRC` (off=retail).
- `332a0b7` Anim.h OnListFlowLabels DC3-only virtual gated (+4: LightPreset::
  StartAnim + 3 VocalTrackDir; slot 0x28→0x24) + `c8cbb32` PropKeys::RemoveRange
  gated (+1: ValueFromIndex; 8 slots uniformly +4 bracketed insertion @0x28).
- `b647c21` OnClearColor map off-by-one (+3) + `2c44ba5` CamShot _Destroy_Range
  label swap + ??1CamShotFrame/??1CamShotCrowd reveals (+2; dtors at 44%/88% =
  follow-up: CamShotFrame mFocalTarget retail 0xf4 vs ours 0xfc — 8-byte
  upstream member to find).
- `7e3f14f`+`c9dfbba` BinStream/Rnd body-ports (+2: Rnd::Terminate DOFProc gate,
  WordWrap rb3-Wii form; BinStream Read 49.9→97.7 / Write 87.2→98.4 fuzzy).

**REFUTED/walls from batch 1 (do not re-attempt as-is):**
- AccomplishmentProgress `/DRB3_RBTREE_0x1C` alone = **−14** (reverted): the
  stair-step is COMPOUND — a 4-byte member deficit BEFORE the first rbtree
  (retail first tree @0x614 vs ours 0x610) AND the 0x18→0x1c split. Find the
  member first, then flag. wall_classify gap: neighbor-bracket the first-tree
  start address to detect compound cases.
- MidiInstrument naive re-pin = net −2 (identity 100% CONFIRMED: pinned range is
  the BandIKEffector TU — Constraint stride 0x1c proof; true cluster
  0x826F5528–0x826F6C60 proven by re-pin diff: MakeNoteInst 15.7→99.97). Vacated
  range loses ~19 accidental funclet folds. LANDABLE PATH (est +10–25):
  (1) wire BandIKEffector.cpp from ../rb3 (needs RndHighlightable.h port + MWCC
  paired-singles Multiply asm replaced), (2) apply
  docs/decomp/handoff/midiinstrument-repin.patch, (3) close MidiInstrument
  near-misses: layout excess +0xc before mFaders (suspect ObjPtrList width),
  4-arg debug PoolAlloc, SampleZone +4 tail int (also lifts SampleZone.cpp).
- BinStream Read/Write final residual = single extra `clrlwi` byte-mask =
  COMPILER-VERSION codegen artifact (permuter 0; source-unfixable). WriteEndian
  = jeff funclet mis-nest wall (machine code matches, caps 61%). Rnd
  CreateDefaults = inline-policy (New<RndEnviron> inlined in retail only);
  DrawTimers = frame-pointer/EH-scope divergence (dossier's rb3-Wii-port advice
  was WRONG — target uses the DC3 form); UpdateRate = retail-specific
  Symbol-typed source neither oracle has. All defer.
- Tooling: diff_inspect --diagnose "Match estimate" is positional/UNnormalized —
  it disagrees wildly with report match_percent_normalized (Tex::Print ~22% vs
  real 99.9→100). NEVER judge a lever by diagnose's headline percent.
- target_symbol_map consistency linter idea (from reloc-midi): a range with
  named symbols from >1 unrelated class family = mis-pin flag; would have
  caught MidiInstrument automatically.

## Wave-2 batch 2 LANDED + verified: **6932 / 65544** (+52, 0 regressions, 8 commits)
Composed fresh build confirms exact composition (6880+52; raw 6845→6897 lockstep;
gap 35 unchanged; units 1582→1585 = BandIKEffector wired + MidiInstrument re-pinned).
- `8c8face`..`b7bd316` **BandIKEffector campaign +30** (the reloc-midi 3-step
  handoff EXECUTED): BandIKEffector.cpp ported from rb3-Wii owns the formerly
  mis-pinned range (29/94), MidiInstrument re-pinned to true span (17/56), ADSR
  Ps2ADSR mPacked tail member (+2 — DC3 dropped it, rb3-Wii has it; proven by
  memcpy 0x28-vs-0x24), MidiInstrument drop DC3 SynthPollable base (+1, the
  +0xc layout). PORT LESSONS in agent notes: RndHighlightable is OUR
  rndobj/Highlight.h (no port needed); MWCC Multiply already __MWERKS__-gated;
  ObjMacros.h vs Object.h REVS-macro arity trap (expand DECLARE_REVS inline);
  retail access-specifier mangling (I vs Q) makes public-vs-protected a
  false-0 — check map access letters when a port reads 0%.
- `e4180d4`+`1e295f6` **Gem +8 / GuitarController +4**: dossier root cause was
  WRONG (not missing logic) — retail uses FUNCTION-LOCAL `static Symbol x("x")`
  lazy-init guard blocks (NEW GENERAL GAME LEVER: any TU referencing
  Symbols-header externs where retail used file-local statics shows
  ??0Symbol+guard-ori delete blocks); BEGIN_HANDLERS MILO_DEBUG-off per-TU
  override (macros.h:3 force-defines MILO_DEBUG tree-wide → every BEGIN_HANDLERS
  emits a MessageTimer block retail lacks — becomes a force-multiplier as more
  Handle-bearing TUs get pinned; global flip = dedicated wave, layout-coupled);
  MILO_WARN (void)(args) per-TU now validated on 3 units = documented idiom.
- `1da6d01` **refill +9** (Tex +3, Crowd +3, TrackPanelDir +3 — batch-1 cascade
  reveals; iter-2 drained). Pools: inline-policy STILL TAPPED (17, all n=1);
  member-delta 0 actionable (1 SIZED_VECTOR VocalPlayer −4@0x278 = wall, our
  STLport has NO sized-vector impl; 5 VBASE; 1 UNKNOWN FreestyleMotionFilter
  −36@0x10).
- `89d87d7` **Synth −0xc +1** (compound: DC3 unk5c list + mZombieInsts position
  + ObjDirPtr 8-vs-0xc width pad; gate RB3_SYNTH_DC3_LISTS default-off).

**BANKED / REFUTED from batch 2:**
- **RndDrawable Draw devirt + DrawShadow drop = BANKED net-0** (patch:
  docs/decomp/handoff/rnddrawable-devirt-banked.patch, applies cleanly).
  Architecture PROVEN (retail Draw is non-virtual: direct `bl fn_823F3A80`
  cull-wrapper in RndDir::DrawShowing; CollideList +8 = two slots). +4 gained
  exactly offset by 4 UIComponent-MI losses (uniform −8 = retail UIComponent
  keeps exactly 2 of rb3-Wii's 4 own-virtuals we lack; which 2 = the
  ui-base-layout-reconstruction effort). DO NOT split the lever (DrawShadow
  alone = −2). Becomes +4+cascade once UIComponent is reconstructed.
- **VocalTrackDir PreLoad/Deploy/TutorialReset = target_symbol_map ICF
  MIS-PINS, not body-ports** (PreLoad pinned onto except_data 0x8; Deploy onto a
  "slider.sld" fn; TutorialReset onto a static-Symbol fn). The research
  dossier's percents compared our source against WRONG retail bytes. TrackReset
  99.989 = ObjectDir-vbase vtable single-slot wall (needs retail vtable order —
  COFF split objs carry no ??_7; Ghidra or caller-anchor reconstruction).
- **OvershellSlot = layout-reconstruction wall**: real retail logic divergence
  (fully DECODED in agent evidence: drop go_to_wiiprofilecreator
  HasTransitionEvent halves + TheServer/IsPrimaryProfileCritical blocks;
  enter/exit_msg as function-local statics) sits UNDER an 8-byte member shift
  (mSessionMgr retail 0x3c vs ours 0x44; the 2 enum members between mState and
  mUserNameLabel). rb3-Wii header is byte-identical to ours = WRONG for
  retail-360; no oracle. Multi-session (cf. Mat_NG). ShowState retail body is a
  DIFFERENT function (DataArray/RTDynamicCast path) — no oracle, skip.
- **Player base-chain −4 = vbase-MI wall** (dossier's unk260 vector hypothesis
  refuted — our STLport vector is unconditionally 12B, no sized-vector branch;
  the +4 sits in the vbase prefix below 0x260; header comments wholesale stale).
- **CamShotFrame −0x8 premise REFUTED**: compiled mFocalTarget is ALREADY 0xf4
  (CameraShot.h `// 0xfc` comment is stale); the 44% dtor = funclet frame +
  ObjPtr-dtor inline-policy, both deferred classes.
- **GemManager scaffold REFUSED correctly**: fingerprint span = 8.8 MB scatter
  (oracle fns at 0.01–0.62 confidence across 9 MB, no contiguous cluster).

**TOOLING QUEUE (multi-agent confirmed, next batch):**
1. target_symbol_map consistency LINTER (3rd independent confirmation:
   MidiInstrument, VocalTrackDir×3, GemManager) + regenerate map names for the
   re-pinned ranges + purge stale MidiInstrument/SampleZone entries now inside
   BandIKEffector (harmless 0% noise but pollutes unit fuzzy).
2. PoolAlloc ICF-merged-symbol ALIASING (highest-value per bandik agent):
   retail folded 2-arg POOL_OVERLOAD operator-new into the 5-arg debug
   ?PoolAlloc@@YAPAXHHPBDH0@Z; our byte-identical `bl` reads [sym] mismatch.
   Caps MakeNoteInst 97.1; likely binary-wide POOL_OVERLOAD sites. Renamer/
   objdiff alias, NOT source (forcing 5-arg form would load r5-r7 and regress).
3. static-Symbol-guard FINDER (the Gem +8 pattern, generalizable worklist).
4. setup_worktree.sh: reflink unified_id_rb3wii.json + struct_db.sqlite +
   global_fuzzy_pairs.json (3 agents hit the gap); fresh_report.sh warn when
   count diverges >10 from pre-build report (the 6845-vs-6880 measurement trap).

## Wave-2 batch 3 (tooling) LANDED: 7 commits, count stable 6932 ✓
- `622f556`+`8c45119`+`1c36699` **tools/map_lint.py** (4 checks) + purged 12 stale
  BandIKEffector orphan map entries (A/B 6932→6932). `obj_orphan` = the
  zero-FP canonical cleanup gate (map name not defined by the unit's compiled
  obj can never pair); whole-map 1092 orphans = future vein but PER-UNIT GATED
  (don't remove names that will pair once source lands). `class_mixing` has a
  known FP class (legit member-type instantiations). VERDICT CORRECTION:
  VocalTrackDir::PreLoad map VA IS a real fn — the batch-2 "except_data mis-pin"
  was a recon mis-read; bodies genuinely diverge (still not body-portable, but
  the MAP is correct). gen_game_target_map regen for re-pinned ranges = N/A
  (BandIKEffector/MidiInstrument are ENGINE TUs, outside the rb3-Wii game
  oracle; batch-2's manual names are complete).
- `9267730` **ICF alias machinery** + the core REFUTATION: the authoritative
  report metric NEVER penalized [sym] reloc-name mismatches
  (report path sets function_reloc_diffs=None → reloc_eq returns true before
  name compare; MCP run_objdiff passes functionRelocDiffs=none too). The
  bandik "highest-value lever" premise came from bare strict-config objdiff-cli
  diff. Landed anyway as scripts/symbol_aliases.json (2 PROVEN folds: PoolAlloc
  5-arg ← 2-arg @0x827960D8; MemOrPoolAlloc + STL variant @0x82798250) rendered
  to a synthetic MSVC map (gen_symbol_alias_map.py) wired via
  ProjectConfig.map_file → objdiff symbol_equivalences; icf_alias_finder.py
  --validate/--scan/--report. Pre-neutralizes alias residue in strict dev
  diffs; 0 pure-alias victims in the current pool. KNOWN GAP: objdiff report
  cache hash OMITS map_file content — rm build/45410914/report.cache after
  editing aliases (one-line fork fix possible).
- `efb2046` **tools/static_symbol_finder.py** + worklist doc
  (docs/decomp/research/2026-06-10-static-symbol-worklist.md). Pool at current
  pin coverage: 3 candidates — Player::SetEnergy 56.71% CLEAN one-way (+1 EV,
  send_update_energy_msg Messages4.h:6 extern vs retail function-local static),
  OvershellSlot UpdateView/UpdateState correctly flagged two-sided WALLS
  (agrees w/ batch-2 verdict). MESSAGE_TIMER (BEGIN_HANDLERS) pool EMPTY —
  drained by batch 2. Lever re-arms as more game TUs get pinned.
- `212488a`+`7d816b4` **infra**: setup_worktree.sh auto-copies
  global_fuzzy_pairs.json/unified_id_rb3wii.json/struct_db.sqlite (4×-confirmed
  gap CLOSED); fresh_report.sh warns on >10 no-source-change divergence (the
  −34 mis-score trap) and prints measures.matched_functions beside the raw
  count.

## Wave-2 CLOSE: **6932 / 65544** (6851 → 6932, +81, 0 regressions, ~28 commits ..8bb12f0)
Final micro-action `8bb12f0`: Player::SetEnergy static-Message port (56.71→99.93;
retail uses a function-local `static Message send_update_energy(Symbol(...))`,
NOT the Messages4.h extern — the corrected form vs first-attempt static Symbol
matters: the Message ctor + ??__F atexit dtor thunk must pair). Net 0 count;
residue = the Player +4 member wall, now bracketed by TWO independent
observations (batch-2: all reads ≥0x260 shifted, mUser 0x260/0x264; SetEnergy:
all fields ≥0x2a0 shifted, `unk2a0` prime candidate). Fixing it = dedicated
member_delta campaign touching ~100 Player.cpp fns + cross-TU (est +7-10 per the
force-mult dossier) — the highest-value SINGLE deferred lever now on the books.

### Where the next +N comes from (post-wave-2 queue, in EV order)
1. **Player.h +4 layout reconstruction** (+7-10 + frees SetEnergy & VocalPlayer/
   Singer chain; two bracket observations on file; needs careful cross-TU A/B).
2. **UIComponent 2-missing-virtuals reconstruction** (unlocks the banked
   Draw-devirt +4 + cascade; docs/decomp/handoff/rnddrawable-devirt-banked.patch).
3. **obj_orphan map cleanup vein** (1092 entries, per-unit gated via
   tools/map_lint.py --check obj_orphan; only stable-source units).
4. **AccomplishmentProgress compound fix** (find the 4-byte pre-rbtree member,
   THEN RB3_RBTREE_0x1C). [SUPERSEDED by wave-3 research — see below]
5. **OvershellSlot layout reconstruction** (retail logic fully decoded+banked;
   multi-session). 6. Mat_NG (multi-session, matng-deferral.md). 7. New research
   wave once these drain — the 40-95 bodyport pool was largely executed or
   refuted this wave.

---

# WAVE 3 (2026-06-11) — research-first model, round 2

## Research phase: 4 dossiers (docs/decomp/research/2026-06-11-*.md), 3 MAJOR CORRECTIONS to prior verdicts

- **Player +4 SOLVED** (`player-plus4-layout.md`): NOT a vbase-MI wall. The two
  bracket observations (≥0x260, ≥0x2a0) were both loose lower bounds; true
  onset = **Performer+0x224**, ours = retail + 4. Cause: our SongPos.h is
  DC3's (0x18, has `int mPhrase`); retail RB3-360 SongPos = 0x14, NO mPhrase,
  identical to rb3-Wii incl. the mTotalBeat-skipping default-ctor quirk
  (retail Performer ctor fn_8267F0F0 reproduces it). Player.h needs NO edit;
  fix = SongPos.h gate (`SONGPOS_DC3_PHRASE`) + HamSongData.cpp 6→5-arg ctor.
  Retail Player ctor located @0x82688E40 (outside pinned range). Est +8–13.
  BONUS LEVERS PARKED in dossier: **Band head +4** (band+0x90 vs 0x94) and
  **Game head +4** (TheGame+0x3d vs 0x41) — independent DC3-delta levers in
  uncompiled TUs, baked into compiled readers.
- **UIComponent vtable RECOVERED** (`uicomponent-virtuals.md`): the "keeps
  exactly 2 of 4" framing was the NET (+2 slots), not the set. Retail keeps
  **3 of 4** Wii own-virtuals (ResourceCopy +0x30, CopyMembers +0x48, Update
  +0x4c; SetTypeDef is slot-neutral) and **LACKS our DC3-only
  OldResourcePreload slot**. Primary evidence: raw vtable words dumped from
  `auto_00_82000400_rdata.obj` @0x8211D4A4 (20 slots; RndDrawable slice = 12,
  no Draw/DrawShadow — confirms devirt). **The auto-rdata-obj dump technique
  unlocks every "retail vtable order unknown" wall** (e.g. VocalTrackDir::
  TrackReset). Also found: mSelected@0x104/mResource@0x108 SWAPPED in our
  header. Traps: both levers (header + banked devirt patch) must land
  together; 10 derived OldResourcePreload overrides gated in same edit.
  Est +6–8 immediate + re-pin/porting follow-ups.
- **AccomplishmentProgress REFRAMED** (`accomplishmentprogress-compound.md`):
  NO missing game member. Retail's TU has sizeof(map)=0x1c AND sizeof(set)=
  0x18 in the SAME TU → the queue's "member + THEN RB3_RBTREE_0x1C" plan was
  wrong (flag grows sets too = the −14). Fix = new **RB3_MAP_0x1C** gate
  (map/multimap-only pad), ungate unk50, per-TU cflag, +12 map entries.
  Refines the rbtree-ODR story: unk50 was a compensation hack. FOLLOW-UP
  VEIN: re-try previously-regressing rbtree TUs with the map-only flag.
- **obj_orphan worklist** (`obj-orphan-worklist.md`): 1103 orphans → 911
  CLEANUP-SAFE across 157 units (hygiene, +0), 92 DO-NOT-TOUCH, and **9
  INVESTIGATE mis-pin cases** — top two are real +N candidates of the
  BandIKEffector class: **MidiParser** range swallows 5 MidiParserMgr methods
  (0x827C5E38–0x827C62D0, last 0x498 bytes), **AsyncFileHolmes** over-extends
  0x6730 bytes into MusicLibrary (12 methods, 0x82527920–0x825285D0).

## Implementation phase
- **AccomplishmentProgress RB3_MAP_0x1C: LANDED +10 (6932→6942), VERIFIED on
  fresh main build** (a811f7f, merged ff-only; fresh_report on main:
  measures.matched_functions = 6942, raw 6907 = known 35 FP-anchor gap).
  +4 layout (dtor + 3 funclets) +6 pairing entries (4 Set*, HasNewReward
  VignetteFestival incl. fixing wrong ?IsRest@HamMove entry @0x825776D8,
  SendHardCoreStatusUpdate). Agent correctly REFUSED 2 dossier-guessed
  entries (0x82577978 = stats-accumulator not Clear; 0x8257A078 =
  global-this init/reset not FakeFill — both left unmapped, identities
  partially decoded in agent report). Residual near-misses for a future
  bodyport batch: 4 Get* @76.85 (out-of-line find vs our inlined _M_find),
  IsUploadDirty 71.4, ClearNewRewardVignetteFestival 11.0.
- **SongPos +4: LANDED +17 (6942→6959), zero regressions** (e64628e).
  Main lever +13 (VocalPlayer +11 incl. 5 auto-revealed already-paired anon
  fns — no refill needed; Player +1, Singer +1); micro-fix GetBandTrack
  ptr-null `Track* t = ...; if (t != nullptr)` cmpwi→cmplwi +3 (inlines into
  Rollback/SetMultiplierActive); micro-fix GetEnabledStateAt explicit
  two-equality compare +1. 12 named flips incl. SetEnergy/GetFrameMatchType/
  ChangeDifficulty. No compiled mPhrase user existed (BeatClock.cpp unwired).
  Risk surfaces (Task/MasterAudio/TrackPanelDir/Stats/…) all unchanged.
- **UIComponent virtuals + banked devirt: LANDED +6 (→6948 vs its baseline),
  zero regressions** (f4f4d13, 18 files). ResourceCopy/CopyMembers/Update
  added in Wii order (Update = phase-A stub); OldResourcePreload gated behind
  new `UICOMP_DC3_VIRTUAL` (+ all 10 derived overrides); mSelected/mResource
  swap fixed; rnddrawable-devirt-banked.patch applied clean. Flips: PanelDir
  Entering/Exiting +2, RndLine::DrawShowing +1, TrainerGemTab +2,
  BandCharacter +1 (cascade landed differently than predicted named list —
  UIListSubList::Draw did NOT flip). Refill: none needed (no
  unpaired-byte-exact gap). NOW UNBLOCKED: UIComponent-TU re-pin (+5–9),
  GuitarController/UIGuide/UIEvent porting veins.
- **Composed verify on main: 6965 EXACT (6942+17+6, zero composition loss),
  stable on re-run.** (The fresh_report warm-cache warning fired on the first
  post-merge run because the source delta arrived via merge with a clean
  tree — benign here, cleared by the prescribed re-run.)
- **RB3_MAP_0x1C retry vein: CLOSED EMPTY** (7c24a93,
  `2026-06-11-map0x1c-sweep.md`). Structural reason: map-only differs from
  all-tree ONLY in TUs with BOTH map+set members; pinned such TUs =
  AccomplishmentProgress (done) + 3 one-fn unported scaffolds (SongMgr/
  HamDirector/DanceRemixer — re-arm only if their source lands). 12 TUs A/B:
  VocalTrackDir −8 (genuine 0x18 cohort), 11 × +0. The 2 stair-step
  fingerprint candidates were false (CharDriver = upstream ObjPtr chain;
  Gem = stack-frame, set-only TU).
- **Band/Game head +4: REFUTED** (e034445, `2026-06-11-bandgame-head4.md`).
  Both deltas REAL (ours = retail+4: Band mCommonPhraseCapturer 0x94 vs
  0x90; Game mProperties 0x30 vs 0x2c) but ZERO measured value — 0 genuine
  readers among all 165 measured near-misses; the +4 is baked only into the
  UNPINNED retail Player ctor (0x82688E40) + anon unpaired readers. NOT a
  header fix: Band.h/Game.h byte-identical to Wii oracle, no DC3 class;
  it's a PER-TU ODR DIVERGENCE — GemTrack.cpp compiles Game @ retail layout
  while Player.cpp compiles the SAME header +4 (root cause unknown; SongPos/
  vector/sized-vector ruled out). Re-arms only if the Player ctor gets
  pinned AND the per-TU divergence is root-caused. ⚠ open mystery worth a
  future research slot: same header, two layouts, same flags.
- **Split mis-pin fixes: LANDED +24, composed verify 6989 EXACT
  (6965+24), stable on re-run** (386fe70 +5 MidiParser/MidiParserMgr
  boundary 0x827C62D0→0x827C5E38; 5f05b23 +19 AsyncFileHolmes/MusicLibrary
  boundary 0x82528C50→0x82527920 — AFH is a 44-line TU that was over-pinned
  by ~0x6A80 bytes; +19 = 12 named MusicLibrary methods + 7 TU-anon fns).
  Pure splits.txt shared-boundary moves, .text+.pdata edited together
  (pdata-catch-22-safe), no map/source edits needed (map is VA-keyed).
  obj_orphan counts dropped accordingly (validates the map_lint
  INVESTIGATE gate as a +N detector, 2-for-2 this wave).
- **NEW FOLLOW-ON LEVER (queued): AsyncFileHolmes HEAD** (0x82522248–
  0x82527920) still contains several un-split foreign TUs (MetaPerformer,
  DOFProc, StartTransitionMsg/CurrentScreenChangedMsg message classes,
  RndSoftParticleBuffer) — splitting those out = a bigger future re-pin
  campaign; the 5 residual AFH orphans map to them.
- **UIComponent-TU re-pin: LANDED +38 (6989→7027 EXACT, stable re-run,
  zero regressions)** (57910ac) — blew past the +5–9 estimate. Root cause:
  UIComponent.cpp was pinned to a single ICF-displaced one-off
  (?StaticClassName@ at 0x823D9AE0, 1 fn) while the REAL retail TU cluster
  sat unsplit in the auto_03 blob. New pin .text 0x827D8DC8–0x827DBDB0
  (+ dtk-derived pdata). Unit 1→39 matched (3 named + 36 anon byte-exact
  auto-pairs). Boundary care: excluded the foreign ICF'd OptionsPanel guard
  pair at head AND retail's ??__E dynamic-init thunks at tail (our build
  inlines them — 0 ??__E in compiled obj, can never pair).
  **PATTERN INSIGHT: re-pins went 3-for-3 ABOVE estimate this wave**
  (mispin +24 vs "investigate", re-pin +38 vs +5–9). Mis-pinned/under-pinned
  wired TUs are systematically undervalued — the next research wave should
  hunt "pinned-to-a-sliver" TUs (tiny pinned range + large matched-source
  obj) binary-wide.
- **ObjectDir-vbase vtable wall: ROOT-CAUSED, fix BANKED net-0**
  (c426ad4 research + 7583d04 patch docs/decomp/handoff/
  objectdir-vbase-banked.patch — DO NOT land alone). The rdata-obj
  slot-dump technique VALIDATED on its second wall: recovered the full
  retail ObjectDir-vbase slot table (anchored via VocalTrackDir::
  SyncObjects override 0x822E7D50 → vtable 0x82029D64; SyncObjects
  universally +0xc across all 28 retail ObjectDir-vbase vtables). TWO
  coordinated base-class bugs: (1) GetExposedProperties = DC3-only virtual
  retail lacks → gate DIR_DC3_VIRTUAL; (2) AllowsInlineProxy IS virtual in
  retail (+0x14), DC3 demoted it (BandCharacter already overrides). The
  bugs cancel past InlineSubDirType — explains why some vcalls matched by
  coincidence. Fix proven byte-exact on TrackReset + CharClipSet::
  SyncProperty + resolves PanelDir::RemovingObject @99.978, but a 40-byte
  CharClipSet DataNode-dtor unwind funclet (fn_823C2044) drops 100→94
  (frame-layout/permuter class) → net 0. Becomes +2-3 when the funclet
  realigns or in a coordinated campaign. Secondary scan: the 99.88 ??_G
  vector-deleting-dtor cluster + 99.6 40-byte fn_ cluster = funclet noise,
  NOT vtable walls; generalized single-slot-wall detector documented.
## Sliver-pin vein (hunt dossier d922dad: ceiling +145–280; realizing ~3× estimates)
- **Batch 1 LANDED +171, verify 7198 EXACT (7027→7198), stable, zero
  regressions**: UIList re-pin **+80** (d2eb460; 0x58 ICF-StaticClassName
  sliver → real cluster 0x827D2900–0x827D8D48 abutting UIComponent, unit
  1→81; excluded same OptionsPanel guard pair at tail); CharEyes **+68**
  (7a77b4c; → 0x82370FA0–0x82377048, unit 1→68); CameraManager **+23**
  (8d7898b; → 0x824A6D08–0x824A83C0, unit 0→23). All honesty-gated.
  Units count 1585→1581 (blob re-segmentation, denominator unchanged).
- **Object/DirLoader/Dir triple: REFUTED by honesty gate** (fe603cc,
  `2026-06-11-object-dirloader-boundary-refutation.md`) — boundary move
  measured +17 but 51/54 newly-attributed fns were FOREIGN (unwired
  obj/Utl.cpp free fns + DirLoader STL + 32-fn funclet farm; 15-contiguous
  foreign run ≫ gate). Region is a THREE-WAY COMDAT INTERLEAVE — the /O1
  TU-contiguity premise FAILS there; no honest pin exists. Reverted clean.
  REAL levers recorded: per-fn body-port of 5–6 genuine Object methods
  (COFF-proven real bodies, pin-independent); optionally wire obj/Utl.cpp.
  ⚠ REVISES orphan-doc: do NOT bulk-delete DirLoader's Object/Symbol/
  DataNode orphan names (real-body names, useful after porting).
- **UIListDir region: MOSTLY REFUTED, +3 kept** (2fad988, UIListState pin
  extended to full TU cluster 0x827E8A50–0x827EA3B8, unit 16→19; ctor was
  dtk-funclet-split 8 bytes off its VA map entry — renamer near-miss class).
  UILabel REFUTED: only ONE real UILabel method exists in the whole retail
  map; the "64-fn cluster" was UIListDir's ULabelSort STL instantiations +
  ICF/inline dissolution — **oracle-obj-count ≠ retail-cluster-size; the
  hunt dossier's EV column systematically overestimates wherever retail
  inlined/ICF'd a class** (2nd confirmation of the Object/DirLoader
  pitfall). UIListWidget REFUTED: 7 methods in a many-TU COMDAT interleave.
## Sliver-vein EXHAUST workflow (ultracode, 42 agents): **+430 LANDED, composed verify 7631 EXACT (7201+430), stable; ZERO regressions**
Structure: recon gate (12 sonnet, named-count EV rule — killed 4 NOGOs
pre-build) → tier-1 Opus implements (chunks of 4, worktree A/B + honesty
gate each) → 4 mechanical sonnet batches → pin_audit.py tool (opus,
validated vs all wave-3 wins+refutations) → tool-fed round 2 (9 found,
8 passed recon, 8 kept). 17 levers kept, 3 implementation refutations,
4 recon NOGOs. All merged ff-only, zero rebase conflicts, zero splits
overlaps (1232 ranges).
**KEPT (17):** SongDB +32 (old pin was ICF-alias scatter graveyard; net
exact); TexBlender/AO carve +39 (AO unit 0→67; TexBlender −28 were
double-counted AO fns); SongMgr +14 (Option-A relocate; 10 named methods
@99.8-99.97 = bodyport pool); Spotlight-triple +4; CharEyeDartRuleset/
CharInterest +7; extensions batch +60 (Stats/FileMerger/CharHair/Song/
MusicLibrary/ButtonHolder/CharClip); small-relocs batch +89 (HeldButton
Panel +32!, UIListSlot +19, CharMeshHide +17, CharIKFingers +12,
UITrigger +9, UIColor/LightPresetManager/SongMetadata; CharIKHand →
deferred to r2); micro-trims +7 (Task/DataNode +2, UI/PanelDir +3,
keygen/ByteGrinder +2); blind-extensions +87 (**Player +35** — unlocked
by this wave's own SongPos fix, VocalPlayer +27, BandWardrobe +25;
VocalTrackDir reverted); round-2 from pin_audit: CharBoneDir +23,
PostProc +16, PropAnim +13 (evicted MessageTimer sliver), CharIKHead +12,
CalibrationPanel +11, SoundTouch +9 (absorbed DirectionGestureFilter
sliver), Bitmap +4, RenderState +3.
**TOOL LANDED:** tools/pin_audit.py (da11049) + ranked worklist JSON
(docs/decomp/research/2026-06-11-pin-audit-worklist.json). Validation:
reproduces UIList/CharEyes/CameraManager exactly on pre-wave splits;
filters/flags all known refutations; extra guards beyond spec (namespace
over-claim, crosses-stub-farm, obj-defines pairing-readiness). Current
state: 42 cands / 41 filtered / 118 deferred. RE-RUN after each landing
wave (adjacency changes).
**REFUTED (don't re-attempt, full evidence in workflow notes + temp
/tmp/sliver_workflow_notes.md → key reasons here):** Waypoint relocate
(superficial +31 = COMDAT-fold interleave, 18-fn foreign run, fuzzy-pairs
ID'd Dir.obj/headdetect content; DC3 map corroborates Waypoint is
template-scattered); UIGuide/LNT (boundaries asm-correct but net −1:
UIGuide's over-pin currently harvests 2 CRT guard thunks the trim
orphans + LNT sliver loses 1); Mic/FxSendDistortion (retail compiled
FxSendDistortion into Mic's TU — single-TU compilation proven by shared
vtable stores; attribution follows compiled obj, no honest boundary);
VocalTrack+Gem NOGO (obj .text sizes == pin sizes EXACTLY both sides —
the 22 'missing' named methods are UNPORTED source, extension = pure 0%
dilution; FontBase sliver also blocks; dossier delta arithmetically
inconsistent); MemHeap/Str NOGO (dossier premise wrong: both units
currently 0/62+0/48 = source/struct wall-gated, not pin-gated);
Character+TypeProps NOGO (FlowEventListener pinned INSIDE proposed range
358 commits ago + region is Object.cpp-family interleave);
StreamNull/MoggClipMap NOGO (MoggClipMap.obj 0xB30 vs 12.8KB multi-TU
scatter range) — BUT its step-B MidiSynth/StreamNull trim [0x826FBD28,
0x826FBD98) was assessed GO (+2, IsFinished/Resync ported) and never
executed: cheap leftover for a micro-pass.

## Sliver-vein FINAL TAIL: **+134, verify 7765 EXACT-stable** (sv-tail ..5a34115, 6 of 6 KEPT, 0 refuted)
FileMerger +54 (lo-extend to full cluster; blocker was the HamPlayerData
1-fn 47% sliver interleaving it — evicted, this-unit-is-the-sliver);
Stats +33 (hi-extend; evicted dead ClipDistMap + CamAnim slivers);
CameraManager upper sub-cluster +23 (hi-extend over 3 evicted group
slivers); MusicLibrary +15 (hi-extend; ErrorNode squatter had ZERO named
methods anywhere — parked); LightPresetManager +6 (relocate from
ObjDirItr ICF sliver); MidiSynth/StreamNull trim +2 (IsFinished/Resync
already in source). DURABLE PATTERN: `requires_sliver_eviction` ≈ a dead
displaced mf=0 sliver squatting in a real owner's TU; evicting it (when
this-unit-IS-the-sliver) is the clean play. FOLLOW-ON: evicted
RhythmDetectorGroup/CharClipGroup/CharPollGroup have real clusters @
0x8257c728/0x822f8ea0/0x8239ce90 (final mini-pass in flight).
**Groups mini-pass (91a7abc): CharPollGroup fresh-pin +20** (real cluster
0x8239C8D0–0x8239E1F4 in the CharWeightable→CharIKFingers gap);
CharClipGroup + RhythmDetectorGroup REFUTED (the tail agent's cited
addresses were single ICF one-offs — an ObjDirItr operator++ our obj
doesn't define, and a ??_E vector-dtor thunk in a UIEventMgr/
Accomplishment thunk run; both classes' real methods are COMDAT-scattered
binary-wide, no contiguous cluster exists).
**LANE VERDICT: the sliver-pin lane is DRY.** Every honest single-
.text-range candidate is landed; what remains in pin_audit output is
known-refuted residue, port-gated work (VocalTrack-class: port THEN
extend), or obj_defines_none_yet mirages. Re-run pin_audit when new
source lands (porting creates new pairing-ready clusters).

# WAVE-3 CLOSE: **7785 / 65545** (6932 → 7785, **+853**, 0 regressions, all composed verifies EXACT)
Ledger: research dossiers ×5 + accprog +10, songpos +17, uicomp-virtuals
+6, mispins +24, uicomp-repin +38, uilist +80, chareyes+cameramanager +91,
uiliststate +3, sliver-exhaust workflow +430. Banked: ObjectDir-vbase
two-virtual fix (net-0, funclet-coupled). New durable assets: pin_audit.py,
rdata-obj vtable-dump technique (2-for-2), RB3_MAP_0x1C / SONGPOS_DC3_PHRASE
/ UICOMP_DC3_VIRTUAL gates, map_lint INVESTIGATE→fix pipeline (2-for-2).

### Post-wave-3 queue (EV order)
1. **Re-run pin_audit.py on the new state** (adjacency changed; 118
   deferred + 41 filtered re-rankable) → next re-pin round.
2. **Bodyport pool REFILLED by the re-pins**: SongMgr 10 named @99.8+,
   SongDB named near-misses, AccProg 4×Get* @76.85, Object.cpp 5-6
   genuine methods (COFF-proven, pin-independent), VocalTrack 22 unported
   named methods (port-then-extend), UIComponent::Update real body
   (phase-A stub today).
3. MidiSynth/StreamNull +2 trim (assessed GO, unexecuted).
   [DONE — landed in the wave-3 tail; StreamNull pin starts 0x826FBD28.]
4. Banked ObjectDir-vbase patch (+2-3 when CharClipSet funclet realigns).
5. AsyncFileHolmes-head identification (fingerprint/Ghidra, not boundary).
6. Synth-belt pin+port campaign (0x826DE000–0x82909000 region, identified
   but uncompiled). 7. obj_orphan purge (re-gate per refutations).
8. OvershellSlot, Mat_NG (multi-session walls).

# WAVE-4 CLOSE (2026-06-11): **7866 / 65545** (7785 → 7866, **+81**, 0 regressions, all composed verifies EXACT)

Bodyport-pool wave (queue item 2), run as one ultracode workflow
(`bodyport-wave4`, wf_10f83482-5c2, 11 agents): Fable recon-gate on 5 lanes
(read-only, main repo, dossiers `docs/decomp/research/2026-06-11-bp4-*.md`,
all 5 GO) → Opus implementation in worktrees chunked 3-wide → coordinator
landed 4 branches sequentially (rebase + ff-only), composed fresh verify
after each: 7831 → 7846 → 7855 → 7866, every count EXACT and stable on
re-run.

### Landed (+81)
- **VocalTrack +46** (87f7869, splits-only): extension lo 0x82B727B8 →
  0x82B6D688 + FontBase 0x54 dead-sliver eviction (mf=0, unit-IS-the-sliver;
  fn_82B6E8E0 is VocalTrack's own ~_Deque_base ICF twin). **The wave-3
  "port-then-extend / 22 unported methods" refutation was a MEASUREMENT
  ERROR**: VocalTrack.cpp already defines every method in the range and the
  compiled obj .text is 0x12628 COMDAT bytes (not the claimed 0x7AE8 ==
  pin-size). 23 named + ~23 anon STL twins flipped 0→100; honesty gate
  passed (longest anon run 12, bracketed by own named lyric/marker helpers).
  LESSON: re-verify a refutation's load-bearing measurement before trusting
  it across waves.
- **SongMgr +15** (dba3312): real base is **MsgSource (+ ContentMgr::
  Callback), not Hmx::Object** — proven by rdata vtable dump (technique now
  3-for-3): RTTI COL @0x821d89ec attr=0x3 (MI+VI), 5 bases; own-virtual
  prefix starts slot 14/+0x38 (Init); retail lacks DC3's
  AlternateSongDir/ContentTitleDiscovered virtuals (gated). All 9 @99.8x
  named + 6 anon helpers → 100. Files: SongMgr.{h,cpp}, ContentMgr.h,
  objects.json.
- **UIComponent +11** (0b7c656): retail tail has TWO flag bytes (mLoading
  0x13c, mSelectCancelled 0x13d, NO mMockSelect) — proven from 3 target asm
  fns; struct size unchanged 0x140, no derived cascade. FinishSelecting
  99.98→100, MockSelect+UpdateResource ported, 7 wave-3 phase-A bodies
  revealed (12 map entries). Update improved 0→69.8, ResourceFileUpdated
  0→88.8 (remaining near-misses).
- **AccomplishmentProgress +9** (e49d006): ⭐ **hash_map discovery — the
  five "0x1c std::map" progress maps are genuinely STLport `hash_map`;
  RB3_MAP_0x1C on this TU was a compensating impostor** (the "dead pad word"
  = hashtable's float _M_max_load_factor @0x18; sizeof naturally 0x1c).
  Retail's out-of-line find (sret, value@node+0x8 slist walk) ==
  _hashtable.h verbatim. Added hash<Symbol> = (size_t)Str(). Gate removed
  from objects.json:681. Plus GamerAwardStatus 360 tail (XOVERLAPPED @0x1c,
  sizeof 0x14→0x38, ERROR_IO_PENDING dtor cancel). 4×Get* 78.85→100,
  GetCurrentValue reveal, 3 GamerAwardStatus fns, LoadStdPtr.
  **FOLLOW-UP LEVER: other 0x1c-gated map TUs whose fns still don't match
  should be re-tried as hash_map.**

### Not landed (honest)
- **Object at-limit** (worktree reverted, net 0): dtor body restructure was
  correct (63.3→97.06, all 5 recon deltas + mNote@0x14-not-mName pool-free)
  but final 7 mismatches = register-allocation SPILL cascade (retail spills
  &mRefs to 0x50(r31) twice; ours keeps callee-save) — permuter exhausted
  134 candidates, wall. Ctor 74.58 unchanged. InitObject/SaveType/Save/
  HandleProperty = ATTRIBUTION_ORPHANs (bodies already == DC3 text but
  per-unit pairing can't register them inside DirLoader's unit; map carries
  DC3 UAA vs our QAA for InitObject). Port = +0 until a pairing-layer fix.
- **vbase-recompose net-0 re-confirmed** (haiku mechanical retest; TrackReset
  +1 / CharClipSet fn_823C2044 −1 balance holds post-wave-3). Fable
  deep-dive on the coupling mechanism in flight → dossier
  `2026-06-11-bp4-vbase-deep.md`.
- Stale queue items resolved: StreamNull trim already landed (wave-3 tail);
  SongDB named-near-miss pool is EMPTY (only STL residue <100).

### New walls catalogued (dossiers have asm evidence)
- AccProg IsUploadDirty @71.43 = TARGET_BOUNDARY dtk size-divergence (target
  symbol has a foreign 16-byte sret accessor glued on; jeff-side fix, not
  source). UIComponent Handle-parent funclet family (fn_827D9D44..E24 @
  92.5-99.9) flips iff Handle (fn_827D9928, retail frame +0x40) ever
  matches. VocalTrack UpdateScrolling @52.34 sz8948 = body-divergence wall
  (1015 I/D, retail-rederive only — DEFER).

### Refill sweep + vbase verdict (same session, post-close)

- **refill_loop.sh sweep LANDED +172** (72faa36; queue item 1 executed
  immediately): 7866 → **8038**, verified EXACT in main fresh+re-run.
  2 iterations (143 safe names, then dry); 46 units gained / 0 dropped.
  Top: FileMerger +19, VocalPlayer +17, Player +12, CharIKFingers +11,
  AccProg +11, BandIKEffector +9, CharMeshHide +7, SongDB +7. Pool
  re-checks: inline_policy_finder still TAPPED (22 candidates, all n=1);
  member_delta_finder2 over new near-misses = 5 known VBASE_WALLs + 1
  UNKNOWN, zero MEMBER_DELTA/SIZED_VECTOR — no struct leverage refilled.
- **vbase deep-dive SOLVED the net-0 mechanism** (Fable, dossier
  `2026-06-11-bp4-vbase-deep.md`, commit 24706f1): fn_823C2044 is retail
  `??0CharLipSync`'s member-unwind funclet (the CharClipSet pin spans the
  CharClipSet/CharLipSync TU boundary; wired CharLipSync pin at 0x822CADA8
  is a displaced sliver). Its pre-patch "100%" was an ARTIFACT funded by
  the bug itself: the DC3-only GetExposedProperties virtual forced a
  `??0DataArrayPtr` COMDAT whose `__unwind$72600` objdiff masked-paired the
  funclet against; retail's CharClipSet TU has no DataArrayPtr ctor at all.
  So the patch's −1 is artifact-removal, not regression → patch is honest
  net-0; **PARK standalone permanently**. Binary-wide delta is exactly 4
  fns; PanelDir::RemovingObject claim in the old banked-doc is STALE
  (99.978 both sides). Bonus finding: retail CharLipSync has
  `ObjPtr<RndPropAnim> mPropAnim` @0x28 that DC3's header dropped —
  restored + verified 0-delta, **banked** as
  `docs/decomp/handoff/charlipsync-mpropanim-prereq-banked.patch`.
  Remaining gate to the +1: ObjPtr-ctor inline-policy wall (retail inlines
  to 3 stores). The honest composition = **CharLipSync re-pin+port campaign**
  (~29 named-method EV per DC3 map; 7-step plan with boundary
  B ∈ (0x823C0CD8, 0x823C11E8], shared-boundary re-pin coordinates, and the
  CharSleeve under-pin sibling fix, all in the dossier).

### Post-wave-4 queue v2 (EV order, after refill)
1. **CharLipSync re-pin+port campaign** (~29 EV; plan + banked prereq ready,
   apply both handoff patches together — vbase patch composes there).
2. **hash_map re-try on remaining 0x1c-gated/0x18-deficit map TUs** (AccProg
   precedent; check rbtree_blast.py-era TUs whose fns still read <100).
3. Re-run pin_audit.py (source landed in SongMgr/AccProg/UIComponent;
   VocalTrack + refill pins moved).
4. Synth-belt pin+port campaign; AFH-head identification; obj_orphan purge.
5. UIComponent::Update 69.8 / ResourceFileUpdated 88.8 finishers; SongMgr
   bonus reveals (recon step 7 list: 0x82784040 ContentName(int) etc.).
6. OvershellSlot, Mat_NG (multi-session walls).

**SESSION TOTAL 2026-06-11 (waves 3+4+refill): 6932 → 8038 (+1106, zero
regressions, every composed verify EXACT).**

# WAVE-5 CLOSE (2026-06-16): **8147 / 65543** (8038 → 8147, **+109**, 0 regressions, all composed verifies EXACT 8073→8129→8146→8147)

Queue-v2 wave, one ultracode workflow (wave5-queue-v2, wf_401e1d45-d16,
7 agents): Sonnet scope-gate on 3 under-explored lanes (all GO) → Opus
implementation in worktrees (CharLipSync straight-to-Opus, no recon) →
coordinator landed 4 branches sequentially (rebase + ff-only; one
target_symbol_map.json union conflict resolved charlipsync+hashmap). NO lane
blocked → Fable escalation never fired (and Fable went unavailable
mid-session — escalation rerouted to Opus on-disk as a safety net, unused).

### Landed (+109)
- **CharLipSync campaign +35** (ef3a625, +prereq 967704f): executed the
  vbase-deep dossier's 7-step plan. Applied BOTH banked patches (mPropAnim
  prereq 0-delta + ObjectDir-vbase net-0-alone) → composed cleanly. Re-pinned
  the displaced CharLipSync sliver (0x822CADA8, 4/19 generic-STL) to its real
  cluster .text [0x823C0CD8,0x823C3FF0) (shared-boundary with CharClipSet),
  +18; reveal_sweep +3; **ALSO fixed the CharSleeve→CharClipSet under-pin
  sibling** (CharClipSet TU really starts 0x823BE0E8=ResetEditorState, was
  pinned from 0x823BEA70 leaving ~9 fns squatting in CharSleeve's pin), +13;
  TrackReset@VocalTrackDir 99.989→100 (the vbase patch composing as predicted
  — the artifact-funded −1 is gone because the funclet now lands in its real
  owner). The vbase patch is no longer "park standalone" — it LANDED here.
- **PropKeys +48 + rnddx9/Rnd +8 = +56** (59a5fa4, b08a210, splits/objects
  only): pin_audit was NOT dry after all on the new state. PropKeys had a
  SECOND displaced .text cluster [0x8240EB60,0x824137A0) (Keys<T> template +
  property code) in the auto-blob — pinned as a second range on the existing
  split, no new map entries (dtk auto-pairs by VA). And wired a brand-new TU:
  system/rnddx9/Rnd.cpp (DxRnd D3D9 renderer) compiled + pinned [0x8270DBB0,..)
  for +8 first matches.
- **AccomplishmentManager hash_map +17** (6dc368d): the AccProg discovery
  scaled — all 12 std::map<Symbol,X> members are genuinely STLport hash_map
  (find inlines the same fn_82543F88 hashtable-find COMDAT, called 34× in the
  unit; RB3_RBTREE_0x1C was the compensating impostor). Switched members +
  return types + iterator decls (AM.{h,cpp}, AccomplishmentPanel.{h,cpp},
  AccomplishmentProgress.{h,cpp}); dropped the gate; hash<Symbol> guarded
  RB3_HASH_SYMBOL_DEFINED so it coexists with AccProg's. 12 named Get/Has +
  2 funclets.
- **SongMgr SaveWrite +1** (15520e1): retail compiled `_MemAllocTemp` with
  MemTrack debug instrumentation STRIPPED (2-arg, no __FILE__/__LINE__/name) —
  same lever as the landed MemAlloc/MemFree macros. Added 2-arg overload +
  macro rewriting all 26 inherited 5-arg sites in MemMgr.h. (Honest
  correction: the other 3 SongMgr "reveals" — ContentName(int)/GetSongsInContent/
  CacheSongData — are real near-misses 68.9-96.5%, NOT byte-exact stubs; not
  landed.)

### New levers / facts this wave
- **hash_map vein is BROAD** (now 2-for-2: AccProg, AccomplishmentManager).
  The tell: find() inlines fn_82543F88 (sret r3, &container r4, &key r5;
  NULL-miss; value@node+0x8). Any 0x1c/0x18-gated map TU whose find-using fns
  read <100 is a candidate — switch member type to hash_map + hash<K> spec +
  drop the gate. STILL OPEN: CharClip.cpp (/DRB3_RBTREE_0x1C, objects.json:210)
  was NOT swept this wave — check next. SongMgr partial: 2 of its members
  (mSongIDsInContent@0x70, unkmap5@0xa8) are hash_map (7 fn_82543F88 calls) but
  the surgical 2-member swap was deferred to keep the wave clean — TODO.
- pin_audit re-run found honest candidates (PropKeys 2nd cluster + rnddx9);
  re-run again now that 4 more units changed.

### Post-wave-5 queue (EV order)
1. **refill_loop.sh sweep** — DONE, **+26** (391ff48; 8147→8173, 2 iters,
   5 units: AccomplishmentManager +11, PropKeys +8, rnddx9/Rnd +4,
   ChunkStream +2, Mesh +1; 0 drops). Pool re-checks unchanged:
   inline_policy still TAPPED (n=1), member_delta = 5 VBASE_WALLs + 1 UNKNOWN
   (FreestyleMotionFilter −36@0x10) — no struct leverage.
2. **hash_map sweep round 2**: CharClip.cpp + SongMgr 2-member surgical swap +
   any other gated map TU with find-using <100 fns (binary-wide fn_82543F88
   caller scan).
3. Re-run pin_audit.py (units changed; PropKeys/rnddx9/CharLipSync moved).
4. UIComponent::Update 69.8 / ResourceFileUpdated 88.8 finishers; SongMgr
   ContentName(int)/GetSongsInContent/CacheSongData near-misses (68.9-96.5).
5. Synth-belt pin+port campaign; AFH-head identification; obj_orphan purge.
6. OvershellSlot, Mat_NG (multi-session walls).

**SESSION TOTAL (waves 3+4+refill+5+refill): 6932 → 8173 (+1241, zero
regressions, every composed verify EXACT).**

# WAVE-6 CLOSE (2026-06-16): **8220 / 65543** (8173 → 8220, **+47**, 0 regressions, all composed verifies EXACT 8180→8189→8220)

Queue-v2 round 2, one ultracode workflow (wave6-hashmap-pins,
wf_a058cca0-75e, 5 agents): Sonnet scope 2 lanes (hashmap2 + pinaudit2, both
GO) → Opus impl 3 lanes (+ finishers2 straight-to-Opus) → coordinator landed
sequentially. NO lane blocked → Opus-escalation (Fable replacement) unused.

### Landed (+47)
- **SongMgr hash_map (finishers2 +7 then refill +9 = +16)** (f0829ef,
  23afe4a): ⭐ **all FIVE SongMgr "std::map" members are STLport hash_map** —
  not just the 2 the hashmap2 scout found. The int-key maps
  (mUncachedSongMetadata@0x34, mCachedSongMetadata@0x54, mContentUsedForSong
  @0x8c) use a SECOND find COMDAT (lbl_82552CD0) that the fn_82543F88-only
  scan missed; ContentName(int)'s node-value-@0x8-vs-@0x14 tell proved it.
  Converted all 5 + dropped RB3_MAP_0x1C + TU-local hash_map BinStream
  operator<<//>> (NOT in the broadly-included BinStream.h). Got
  ContentName(int)/CacheSongData/GetSongsInContent +7, then refill revealed
  9 more SongMgr accessors (ContentNameRoot/NumSongsInContent/ContentUnmounted/
  ContentStarted/ClearCachedContent/ClearFromCache/IsContentUsedForSong/Data +
  STL helper). **The parallel hashmap2 lane (2-member, +6) was SUPERSEDED**;
  its dossier retained.
- **Waypoint relocation +31** (d087a94, splits-only): relocated the dead 0x50
  ICF sliver [0x822C8CA8,..) to the real TU cluster [0x823C7CC8,0x823CA668).
  ⭐ **THIS REVERSES THE CANONICAL "Waypoint +31 = dishonest" REFUTATION.**
  Adversarial Opus audit (dossier 2026-06-16-w6-waypoint-audit.md): 25 anon
  fns = 25 own / 0 foreign. DECISIVE: DC3 ham_xbox_r.map shows a CONTIGUOUS
  char:Waypoint.obj TU (0x823CBAC0..0x823CEF60, same method order, ~0x4000
  offset). The wave-3 "18-fn foreign run at 0x823C8A48" was a MISREAD of ICF
  address-aliases — 0x823C8A48 IS Waypoint::Save. Composed verify on live main
  = exactly +31, all in Waypoint, zero regressions (arithmetic-proven honest).
  6 named methods @100 + 25 own funclets/registry-bit-ops; Save 99.9/Copy 99.8
  are porting-incomplete tails (follow-up).

### Refuted / not landed
- hashmap2 SongMgr conversion (subset, superseded by finishers2's superset).
- pin_audit2 also re-confirmed TypeProps/Character BLOCKED (FlowEventListener
  0x827418D0 inside proposed range) and the PropKeys w6 "candidate" = FALSE
  POSITIVE (inside the wave-5 2nd pin already).
- finishers2's UIComponent ?Update@ (69.84) + ?ResourceFileUpdated@ (88.84) =
  WALLS (not attempted / reveal-blocked), DEFER.

### ⚠ REFUTATION-LIST CORRECTION (important for future sessions)
"Waypoint relocate (COMDAT template-scatter)" and "Waypoint +31 = DISHONEST
ATTRIBUTION, reverted" are **WRONG** — they rested on a misread DC3 map +
misread ICF aliases on the DEAD sliver. The real Waypoint TU is contiguous and
the +31 is honest (LANDED d087a94). Re-tag accordingly.

### Post-wave-6 queue (EV order)
1. **hash_map vein round 3**: binary-wide fn_82543F88 + lbl_82552CD0 (the
   int-key find COMDAT) DUAL caller scan — the 5-member SongMgr finding means
   the earlier scans under-counted; re-scan for both COMDATs. The big
   auto_03_82272EB4 blob (76 fn_82543F88 calls) = future pin target.
2. pin_audit round 3 (Waypoint method tail Save/Copy; other survivors).
3. UIComponent Update/ResourceFileUpdated walls (need real port or defer).
4. Synth-belt pin+port; AFH-head; obj_orphan purge.
5. OvershellSlot, Mat_NG.

**SESSION TOTAL (waves 3+4+5+6, 2026-06-11 & -16): 6932 → 8220 (+1288, zero
regressions, every composed verify EXACT).**

# ============================================================
# CONSOLIDATED OPEN BACKLOG (as of 2026-06-19) — THE authoritative TODO
# ============================================================
# Supersedes the scattered per-wave "Post-wave-N queue" lists above. Anything
# not here is either DONE or in the CLOSED/SPENT ledger at the bottom. Re-rank
# after each wave. Current state: main @ d6e1435, 8220/65543 matched.

## A. ACTIVE LEVERS (have a proven method, ready to execute — highest EV)
A1. **hash_map vein — RE-OPENED, ACTIVE** (wave-8 PROVED "exhausted" WRONG;
    REFUTATION_WRONG, dossier 2026-06-19-w8-hashmap-exhaustion.md). The wave-7
    "exhausted" verdict was a SCOPING ARTIFACT — it scanned only 2 known COMDAT
    addresses against 5 pre-pinned blob labels. GROUND TRUTH (auto_03 COFF,
    411k relocs): there is a **THIRD 128-byte find-COMDAT at 82B23238** (never
    scanned), plus insert/operator[]/rehash COMDATs; **75 fn_82543F88 callers
    (40 in UNPINNED/unconverted units) + 43 lbl_82552CD0 callers (35 unpinned)**.
    Find-only scans are ALSO structurally blind to iterate-only members (AccProg
    ::Poll). Converted units have residuals too. REMAINING WORK (next wave):
    **cluster-alpha [0x825B86A0,0x825C10D8)** = single-TU hash_map<int,short>@
    this+0x38, 23 accessors, +20 (the w7 doc itself flagged this then DROPPED it);
    **BandSongMgr port-then-pin** [~0x82631350,~0x82632C54], +16 (BandSongMgr.cpp
    UNWIRED — genuine port-then-pin, NOT the "A6 mirage"); **datautl 3DSound
    symbol cluster** +5; **82B23238 third-COMDAT scout**. Landed this wave:
    accprog-iterate +2, fsss GetID +1. Method: 2026-06-16-w6-hashmap2.md.
A2. **pin_audit — mostly DRY** (round 3 run wave-7). Sliver-hunt is now 6-for-6
    (UIComponent +38, CharLipSync, Waypoint +31, Part +11). Live survivor:
    **A2a Character.cpp relocation (+9, RECON-GATED)** — relocate the dead 0x48
    sliver [0x822911F0..0x82291238) to the real cluster [0x8235B1D0..0x8235F180)
    (7 named Character methods: ForceBlink/EnableBlinks/SetFocusInterest/
    SetInterestFilterFlags/Teleport/AddedObject/SetInterestObjects, all in
    Character.obj). GATE: Ghidra-decompile fn_8235DC48 (1344B) + fn_8235E300
    (1196B) in the 15-fn sub-gap [0x8235DBD8..0x8235E9A0] — if both are
    CharPollableSorter sort bodies (own-TU), pin is clean +9; if foreign COMDATs,
    refute. (w6's "depends on TypeProps eviction" was WRONG — Character cluster
    is ~5MB from TypeProps.) Dossier: 2026-06-19-w7-pinaudit3.md. Everything else
    re-confirmed refuted/blocked/FP (TypeProps FlowEventListener-inside,
    UIGuide/LNT net-1, InlineHelp inside CrowdAudio, PropKeys-FP).
A3. **refill_loop.sh** — standing compounding step; run in a worktree after ANY
    source-landing wave (`NINJA_JOBS=12 tools/refill_loop.sh --map
    global_fuzzy_pairs.json`). Gave +172/+26/+9 across this session.
A4. **AsyncFileHolmes HEAD split** (0x82522248–0x82527920): still contains
    un-split foreign TUs (MetaPerformer, DOFProc, StartTransitionMsg/
    CurrentScreenChangedMsg message classes, RndSoftParticleBuffer). Split them
    into their own pinned TUs → re-pin campaign; the 5 residual AFH orphans map
    to them. Bigger multi-TU effort.
A5. **Synth-belt: SCOPE first.** 0x826DE000–0x82909000 already has ~169 pins —
    NOT virgin. Needs a gap-scan (tools/scope_map.py / pin_audit) to find which
    synth TUs are still unpinned/uncompiled before any pin+port. Don't assume
    it's empty.
A6. **blob-82627200 port-then-pin (LOW priority, big).** Wave-7 DISPROVED the
    other 4 "hash_map blobs" (mirages — already-converted or std::map). Only
    this one is real unpinned work: gap [0x82627200, 0x82635720), 0xE520 bytes,
    ~593 anon fns, MULTI-CLASS (GamePanel sliver + others) — a port-then-pin
    campaign, not a clean pin. Needs COFF owner-split first. Defer unless a
    fingerprint pass shows a tight single-TU sub-cluster.

## B. NEAR-MISS TAILS (small, specific, low-risk)
B1. **Waypoint Save/Copy — DONE** (+7, wave-8): mConnections was
    ObjVector<ObjOwnerPtr<Waypoint>> (rb3-Wii oracle authoritative — DC3
    false-friend had ObjPtrVec); header 0xdc→0xd0 + 1-arg ctor. Landed d3c6e4f.
B2. **FreestyleMotionFilter::Deactivate (99.33) — CONFIRMED WALL** (wave-8
    falsified it as fixable): retail is a 12-byte stub writing 1 @offset 0x10,
    NO oracle, no triangulation — unmatchable without a base-class change. DEFER
    permanently unless a base-layout oracle appears.
B3. **SongMgr — TRULY EXHAUSTED** (wave-8: all 27 named methods at 100.0).

## C. WALLS / GATED (need a prerequisite or deep reconstruction)
C1. **UIComponent base-layout reconstruction — DONE/CLOSED** (wave-8 proved it
    ALREADY LANDED: commit f4f4d13 "+6", the full 8 own-virtuals at slots
    0x30-0x4c in verified retail order + UICOMP_DC3_VIRTUAL on all 12 derived
    sites). The "+4+cascade key gated lever (open)" framing was STALE — the
    cascade already fired (DrawShowing@RndLine/PanelDir Entering/Exiting all 100).
C2. **rnddrawable-devirt — DONE/CLOSED** (already in-tree: Draw.h has
    DRAW_DC3_VIRTUAL on Draw/DrawShadow + all 5 subclass overrides; landed with
    C1 in f4f4d13). The banked patch was a SPENT artifact identical to in-tree
    code — DELETED from docs/decomp/handoff/.
C3. **UIComponent ?ResourceFileUpdated@ — DONE** (+23, wave-8): root cause was
    NOT a body port but the GLOBAL MakeString.h template convention (DC3
    by-const-ref → rb3-Wii by-value) — landed 36b9817, broad win. ?Update@
    (68.8, 304 instr) + ?GetResourcesPath@ (73.55) remain = PERMUTER-class
    (regalloc), defer. ?Handle@ port (fn_827D9928, 956B, 0%) = +12 incl. funclet
    unblock — NEXT-WAVE candidate (not a wall).
C4. **VocalTrack ?UpdateScrolling@ (52.34, sz 8948)** — body-divergence wall
    (1015 I/D, retail-rederive only). DEFER.
C5. **Object.cpp** — dtor at 97.06 (regalloc spill-cascade, permuter exhausted
    134 candidates); InitObject/SaveType/Save/HandleProperty = ATTRIBUTION_ORPHANs
    (bodies == DC3 but per-unit pairing can't register them inside DirLoader's
    unit — needs a pairing-layer fix, not a port).
C6. **OvershellSlot** — layout-reconstruction wall (mSessionMgr retail 0x3c vs
    ours 0x44; rb3-Wii header byte-identical = wrong for retail-360, NO oracle).
    Multi-session. Logic divergence fully DECODED in batch-2 agent evidence.
C7. **Mat_NG** — retail material layout SCRAMBLED (not block-shifted); DC3_REV_MEMBER
    lever DEFERRED (424412b). Multi-session.
C8. **Player base-chain −4 — DONE/CLOSED** (wave-8: ALREADY SOLVED+landed —
    commit e64628e, SongPos 0x18→0x14 drop DC3 mPhrase, +17. The ledger entry
    was DEAD/contradicted; not a wall).
C9. **CamShotFrame** — funclet frame + ObjPtr-dtor inline-policy, both deferred
    classes (mFocalTarget already 0xf4, the // 0xfc comment was stale).

## D. TOOLING / JEFF-SIDE
D1. **AccProg ?IsUploadDirty@ (71.43)** = dtk TARGET_BOUNDARY divergence (target
    symbol has a foreign 16-byte sret accessor glued on — a jeff-side .pdata/
    boundary fix, NOT a source fix). Belongs to the jeff fork backlog.
D2. **target_symbol_map consistency linter / stale-entry purge** — map_lint
    exists; verify the purge of stale MidiInstrument/SampleZone entries now
    inside BandIKEffector's range is done (harmless 0% noise, pollutes unit fuzzy).

## E. CLOSED / SPENT this session (DO NOT re-attempt — record to prevent churn)
- **Waypoint relocation +31** — LANDED honest (d087a94); the old "DISHONEST,
  reverted" refutation is OVERTURNED (audit: 2026-06-16-w6-waypoint-audit.md).
- **CharLipSync re-pin+port campaign** — DONE (wave-5, +35); composed the
  ObjectDir-vbase patch; TrackReset@VocalTrackDir now 100.
- **SongMgr** — all 5 map members → hash_map DONE (wave-6, +16 with refill).
- **hash_map**: CharClip = genuine rbtree (don't touch); PhysicsVolume/DataFunc
  = not members (eliminated).
- **Banked patches SPENT**: objectdir-vbase + charlipsync-mpropanim-prereq
  (consumed wave-5), midiinstrument-repin (already in splits.txt from a prior
  session). Removed from docs/decomp/handoff/. Only rnddrawable-devirt remains
  (→ C2, stale/gated).
- StreamNull/MidiSynth trim, MsgSource SongMgr base, AccProg hash_map,
  UIComponent tail-bytes — all DONE earlier this session.

# ============================================================
# WAVE-7 CLOSE (2026-06-19): **8234 / 65543** (8220 → 8234, +14, 0 regressions)
# ============================================================
Tooling-first ultracode workflow (wave7-tooling-then-pins, wf_2a15a46b-5b4,
5 agents): 3 focused Sonnet scouts (pin_audit3, hashmap-blobs, hashmap-thin) →
13 candidates, only 2 ACTIONABLE → 2 focused Opus impl agents → both LANDED.
NO blocked lanes. Composed verifies EXACT (8231 → 8234).
- **Part dual-range +11** (9908b26, splits-only): second .text range for the
  RndParticleSys cluster [0x8243B7D8..0x8243C858), bypassing the Accomplishment
  stub farm (in the gap between range 1 and range 2) — the PropKeys dual-range
  pattern. 11 RndParticleSys methods, perfect attribution.
- **FixedSizeSaveableStream hash_map +3** (5596790): both symbol-table members
  (m_mapSymbolToID@0x30, m_mapIDToSymbol@0x4c) were std::hash_map — vein's 4th
  success. GetSymbol/InitializeTable/SetSymbolID 0→100.
HIGH-VALUE NEGATIVE RESULTS (the tooling phase's main payoff — saved a wave of
dead-end impl): hash_map vein EXHAUSTED (A1; the auto_03 blob "mass" was
mirages); pin_audit mostly DRY (A2; Part was the only clean win, Character +9 is
recon-gated A2a). Backlog A1/A2/A6 updated accordingly.

**SESSION TOTAL (waves 3–7, 2026-06-11/16/19): 6932 → 8234 (+1302, zero
regressions, every composed verify EXACT).**

### Top of queue now (post-wave-7)
1. **A2a Character.cpp relocation +9** — RECON-GATED: Ghidra fn_8235DC48 +
   fn_8235E300 (own sort bodies vs foreign) → then pin [0x8235B1D0..0x8235F180).
2. **C1 UIComponent base-layout reconstruction** — the key gated lever
   (unblocks C2 rnddrawable-devirt +4+cascade, C3 finishers). Multi-step;
   docs/plans/ui-base-layout-reconstruction.md.
3. **B-tier near-misses**: Waypoint Save/Copy tails, FreestyleMotionFilter
   −36@0x10. Small.
4. **A4 AsyncFileHolmes HEAD split** — bigger re-pin campaign.
Active veins (sliver-repin, hash_map) are now largely worked out; the frontier
is shifting to gated reconstruction work (C1) + recon-gated re-pins (A2a) +
bigger pin campaigns (A4/A6). Cost-per-match rising — expect smaller waves.
[NOTE: wave-8 OVERTURNED the "largely worked out" + "smaller waves" framing —
see WAVE-8 CLOSE below.]

# ============================================================
# WAVE-8 CLOSE (2026-06-19): **8314 / 65544** (8234 → 8314, +80, 0 regressions)
# ============================================================
All-Opus DYNAMIC workflow (wave8-opus-verify-and-advance, wf_beb07c14-625,
23 agents): 5 Opus PLANNERS adversarially FALSIFIED every "exhausted/refuted/
done" claim AND emitted executable work-items (planner-driven dynamic waves) →
10 Opus impl → 2 adversarial honesty-audits of attribution-risk pins → 6
follow-ups → escalation. Composed verifies EXACT (8279→8302→8314).

### THE BIG STORY: the adversarial pass caught FOUR wrong claims (the Waypoint
### lesson, generalized). This is why we re-verify our own verdicts.
1. **hash_map "EXHAUSTED" = WRONG** → A1 RE-OPENED (3rd find-COMDAT 82B23238,
   75+ unconverted callers, cluster-alpha +20 / BandSongMgr +16 / datautl +5
   remaining). The w7 "exhausted" was a 2-COMDAT scoping artifact.
2. **C1 UIComponent reconstruction "the key OPEN gated lever" = WRONG** →
   already landed f4f4d13; cascade already fired. CLOSED.
3. **C2 rnddrawable-devirt "banked, not landed" = WRONG** → already in-tree;
   the banked patch was spent, DELETED.
4. **C8 Player base-chain −4 "vbase-MI wall" = WRONG** → already solved e64628e.
   CLOSED.
Plus genuine confirmations (the refutations that HELD, re-verified with fresh
COFF/DC3-map/Ghidra): InlineHelp, TypeProps, Mic/FxSend, Band/Game head,
AsyncFileHolmes interleave walls; C5 Object port-bound; B2 FreestyleMotionFilter
12-byte-stub wall; B3 SongMgr truly exhausted. Refutations aren't all wrong —
but they MUST be re-checked, and ~30% were stale.

### Landed (+80)
- **Character relocation +45** (38a773a, splits-only): dead 0x48 sliver →
  real cluster [0x8235B1D0,0x8235F180). Recon-then-pin (Ghidra-confirmed the
  15-fn sub-gap = own CharPollableSorter sort bodies), then ADVERSARIAL
  honesty-audited HONEST (45 own / 0 foreign). Far above the +9 estimate.
- **MakeString.h by-value +23** (36b9817): GLOBAL header convention fix (DC3
  by-const-ref template params → rb3-Wii by-value) — a broad inlining-keystone
  win surfaced while chasing UIComponent::ResourceFileUpdated.
- **Waypoint Save/Copy +7** (d3c6e4f), **AccProg iterate-bodyports +2**,
  **fsss GetID pin-extend +1** (audited HONEST), **RndGroup CollideList +1**
  (mShowing→mDrawing loop-gate fix), **AccProg IsUploadDirty +1** (config/
  symbols.txt boundary fix, NOT jeff-side as D1 claimed — D1 corrected).

### Process win: 2-stage honesty defense held
Both attribution-risk pins (Character, fsss) passed an independent adversarial
own-vs-foreign audit BEFORE landing, then composed-verified EXACT on main. The
non-landable results (C6 OvershellSlot, C9 CamShot, several reveals) all
refuted cleanly on contact = valid net-0, zero false-positive attribution.

### NEXT WAVE (re-opened + surfaced, EV order)
1. **hash_map re-opened vein**: cluster-alpha +20 (port-then-pin, identify
   owner), BandSongMgr +16 (port BandSongMgr.cpp from rb3-Wii + wire + pin),
   datautl +5, 82B23238 third-COMDAT scout.
2. **UIComponent ?Handle@ port +12** (fn_827D9928, 956B, BEGIN_HANDLERS —
   unblocks its funclet family).
3. **A2a Character deepen** (the relocated TU has near-miss tails to finish).
4. **Mat_NG reconstruction +8** (C7, multi-session, RB3_MATNG_LAYOUT plan in
   the wall-ledger dossier).
5. D2 map_lint stale-purge; A4 AsyncFileHolmes (confirmed interleave — bigger).

**SESSION TOTAL (waves 3–8): 6932 → 8314 (+1382, zero regressions, every
composed verify EXACT).**

# ============================================================
# WAVE-9 CLOSE (2026-06-20): **9037 / 65543** (8314 → 9037, +723, build green)
# ============================================================
DEEP 10-LAYER DYNAMIC OPUS LOOP (wave9-deep-dynamic-loop, wf_87949d74-f91,
**152 agents, ~9.5h**): each layer fanned out (discover→execute) + fanned in
(honesty-audit→reduce→regenerate frontier), tasks generated on-the-fly. It ran
all 10 layers (vein stayed RICH) and produced 48 "land-ready" results nominally
summing to **+3066** — but that was **~4.5× INFLATED by double-counting** (see
LESSON). Real coordinator-de-duplicated, composed-verified gain = **+723**.

### ⭐ THE KEYSTONE (+217, the dominant lever)
**MILO_MESSAGE_TIMERS Handle-macro gate** (3b86e9a): retail RB3 compiled every
Milo object's `::Handle` with the MessageTimer profiling instrumentation OFF
(proven byte-exact on GuitarController). Gated the timer behind a new
MILO_MESSAGE_TIMERS macro (undefined = retail shape) in Object.h/ObjMacros.h +
HANDLE_CHECK comma-form (PathName vcall survives) + END_HANDLERS PathName tail.
This is a BINARY-WIDE retail-stripping lever (like debug-output stripping but
Handle is everywhere) — +217 spread across 10+ unit families (UI/Rnd/Char/
Dancer/Quest/ArkFile/Instance), honesty-confirmed. Native port keeps real
timers via HX_NATIVE. DURABLE: any future Handle near-miss is already covered.

### Game-port TUs landed (real composed incrementals, +423)
Sequence +111 (wire+pin core .text, evict ICF sliver), SongSortMgr +78,
BandSongMgr +63 (std::map→hash_map), SongUpgradeMgr +41, SongSelectPanel +30,
Instarank +28, StoreMainPanel +28, ViewSetting +22, MidiParser::Handle +16,
CriticalUserListener +9, StreamPlayer/ConnStatusPanel +9, FSSS residuals +2.
All band3/meta_band (or engine) TUs ported from rb3-Wii, wired+pinned, A/B'd
individually on the growing keystone base.

### Refill +83 (557622e)
Post-keystone reveal harvest: 51 map entries for Handle funclets/message-
handlers binary-wide (BandDirector/Character/Waypoint/UIComponent/Joypad/Seq
family/StoreMainPanel/…) + Joypad.cpp pin extension. The keystone's cascade.

### Deferred (re-derivable next session — branches kept, dossiers in research/)
SongStatusMgr ~+45 (cascading multi-commit map conflicts; branch
w9-songstatusmgr-base-rebase-plus-getpossiblestars-reveal), AppMiniLeaderboard
~+20, LicenseMgr ~+27, StoreMenuPanel ~+8. RndParticleSys-port-dc3 = DISCARDED
(it was a keystone Handle-macro variant, superseded, not a real TU port).

### Build-break incident (fixed, 94b68b4)
Two independently-developed adjacent ports (CriticalUserListener + ViewSetting,
both ~0x825BDxxx) DISAGREED on their shared .text/.pdata boundary → splits
overlap broke the build. Fixed: CUL .text start 0x825BD484→0x825BD5F0
(ViewSetting owns up to 0x825BD5F0). LESSON: independently-developed adjacent
pins can collide — run the splits overlap-checker before landing a batch.

### ⭐⭐ DEEP-LOOP PROCESS LESSON (critical for future deep loops)
A 10-layer loop measuring every work-item vs a FIXED baseline **double-counts
foundational levers**: the Handle keystone got independently re-derived ~12×
(each layer's agents kept finding it + measuring it +130/+196/+217 against
8314), and every dependent work-item bundled it, inflating nominal +3066 →
real +723. The rebase-auto-drop of duplicate commits + the synthesizer's
de-dup guide + per-landing composed verify were ESSENTIAL to recover signal.
MITIGATIONS next time: (a) keep deep loops to INDEPENDENT work only (the
game-port TUs composed cleanly; the Handle-family swarm didn't); (b) have the
reducer maintain a "virtually-applied" set and tell later layers to measure
INCREMENTALLY on it, not vs fixed baseline; (c) land the foundational keystone
FIRST (as its own short wave) THEN fan out dependents. The +723 is real and
large, but the loop burned ~152 agents/~15M tokens for it — efficiency was
poor; a keystone-first + independent-fan-out structure would get the same +723
for ~1/4 the cost.

### Backlog deltas from wave-9
- UIComponent ?Handle@ / the whole Handle family (was C3/A-tier) = DONE
  (keystone). hash_map vein: BandSongMgr/SongUpgradeMgr/SongSortMgr converted
  (SongStatusMgr deferred). Many band3/meta_band TUs now wired+pinned.
- NEXT: land the 4 deferred ports (clean re-derivation), then re-run pin_audit
  (lots of new ported source = new sliver candidates), refill again, and the
  remaining hash_map cluster-alpha [0x825B86A0,0x825C10D8) (still unowned).

**SESSION TOTAL (waves 3–9): 6932 → 9037 (+2105, build green; every landed
lever composed-verified EXACT; one build-break caught+fixed).**

# ============================================================
# WAVE-10 CLOSE (2026-06-20): **9155 / 65546** (9037 → 9155, +118, build green)
# ============================================================
LESSON-APPLIED single-pass INDEPENDENT-fanout wave (wave10-independent-fanout,
wf_9b33e64e-5b4, 24 agents): explicitly NOT a deep loop — keystone-done, so
independent game-TU ports fan out without double-counting. Discover (4 Opus
lanes) → execute (self-contained) → honesty-audit (all 9 HONEST) → reduce.
PROVED THE LESSON: the reducer de-duped same-TU variants (the discover lanes
overlapped, spawning SongStatusMgr ×3 / LicenseMgr ×2 / AppMini ×2) down to the
5 REAL winners (+118), and the pre-build splits overlap-check came back clean
(0 overlaps) — NO build-break this time (vs wave-9). Zero foundational levers
bundled (flag_foundational machinery unused — none found).

### Landed (+118, 5 independent meta_band game-TU ports, all audited HONEST)
- **SongStatusMgr +49** (port + retail hash_map<int,SongStatus*>@0x38 cache
  re-layout replacing Wii SongStatusCacheMgr[1000] + 15000 star-cap + evict
  dead MoggClip orphan sliver). LAND-FIRST anchor. ⚠ picked port-then-pin; the
  `clusteralpha-reapply-extend` sibling REGENERATED target_symbol_map wholesale
  (+12679 entries, whole-binary re-pair) = would-be poison, correctly rejected.
- **LicenseMgr +27** (reconstruct content-cache layout: hash_map<Symbol,
  vector<Symbol>>@0x1c + set<Symbol>@0x4 + dirty bool@0x38; pin between
  SongUpgradeMgr & Instarank). 4 residual fns are jeff-asm-misnest-truncation
  (→ tooling queue).
- **AppMiniLeaderboardDisplay +19**, **StoreMenuPanel +14**, **VoiceoverPanel
  +9** (evict dead Cam.cpp sliper pinned inside its cluster). All
  port+wire+pin+map self-contained.

### Process: lessons held
2-stage honesty defense (per-item overlap self-check + own-vs-foreign audit
before land) + reducer de-dup + binary-wide overlap check before build = clean
land, no incident. Efficiency far better than wave-9's deep loop (24 agents for
the same shape of win vs 152).

### NEXT WAVE (reducer next_frontier — the meta_band belt is still RICH; each
### landing opens adjacent seams; all INDEPENDENT port-then-pin, no keystone)
1. MusicLibraryNetSetlists port+pin (head gap below SongStatusMgr) ~+15
2. AppLabel.cpp body [0x825BB090,0x825BB5B8) ~+12 (boundary: starts 0x825BB090
   NOT 0x825BADD0 — SongStatusMgr absorbed up to 0x825BB090)
3. engine MiniLeaderboardDisplay.cpp pin [0x8262E974,0x8262F530) ~+10
4. PrefabMgr.cpp unwired TU ~0x825BE7A8 ~+10
5. VoiceoverPanel megacluster [0x825FC080,0x8261AAF0) boundary-derive scout
   (~12 interleaved panels, ~+15 first batch + tail)
6. Campaign.cpp ~0x82590910 boundary recon ~+12
7. re-run pin_audit (multi-range fix first) after these land
COORDINATOR-QUEUE (parallel, non-blocking): jeff asm-misnest truncation fix
(../jeff src/cmd/xex.rs — LicenseMgr +4 + binary-wide tail); pin_audit.py
multi-range candidate fix.

**SESSION TOTAL (waves 3–10): 6932 → 9155 (+2223, build green; every landed
lever composed-verified EXACT).**

# ============================================================
# WAVE-10.5 refill + WAVE-11 CLOSE (2026-06-20): **9301 / 65546** (9155 → 9301, +146)
# ============================================================
- refill +4 (053d2d0): SongStatus::SaveFixed / StoreMenuPanel reveals (game-TU
  pins cascade less than the keystone).
- **WAVE-11 +142** (wave11-metaband-belt, wf_15cb30a1-abc, 15 agents): one
  discover lane per distinct TU (TU-dedup structural — no same-TU overlap to
  untangle). 4 meta_band belt port-then-pin TUs, all audited HONEST + splits-
  clean: **Campaign +58** (port band3/meta_band/Campaign.cpp), **VoiceoverPanel
  megacluster +41** (scout-then-port the first clean sub-TU = EditSetlistPanel.cpp,
  rest deferred to frontier), **PrefabMgr +24**, **MusicLibraryNetSetlists +19**
  (identified by UNIQUE string fingerprint s_id%03i/s_name%03i → exactly one
  oracle file; head gap below SongStatusMgr). Pre-build splits overlap-check
  clean; no build-break.
  - WORKFLOW BUG NOTED: the audit-clearing key-match dropped Campaign (+58) from
    the reducer's "cleared" set because the auditor keyed itself "w11-Campaign"
    vs impl key "Campaign" — coordinator caught it (Campaign WAS landable+honest)
    and landed it manually. NEXT WAVE: key audits by impl.key exactly.
- DOC CHANGE: the "never touch math/Color.h/math/Utl.h/shared headers" rule is
  now recorded as a **CAUTION, not a ban** (user-confirmed) — codebase-wide
  changes are fine when principled + composed-verified net-positive (Handle
  keystone, MakeString were exactly this). playbooks/bodyport-wave.md §10 updated.

### NEXT WAVE (wave-11 reducer frontier — meta_band belt HEALTHY, all INDEPENDENT
### port-then-pin, oracle-HAVE-in-rb3-Wii unwired TUs)
1. MainHubPanel.cpp ~+25, 2. setlist-family (SavedSetlist/SongSortNode/SongSort,
coupled to MusicLibraryNetSetlists.h) ~+24, 3. ManageBandPanel.cpp ~+18,
4. PatchPanel-deepen+PatchSelectPanel ~+15, 5. SaveLoadManager.cpp ~+12,
6. AppLabel real cluster (rb3-Wii 121 fns, NOT the refuted 0x825BB090 sliver
which is a ViewSetting music_library_upsell subclass — reconstruct-from-disasm,
deferred) ~+12. TWO BIG UN-BISECTED GAPS need a dedicated bisection sub-wave
(auto_03 string-fingerprint): [0x825BDF28,0x825C10D8) ~110 fns +
[0x825C3A44,0x825D0EF0) ~565 fns. Then re-run pin_audit.

**SESSION TOTAL (waves 3–11): 6932 → 9301 (+2369, build green; every landed
lever composed-verified EXACT).**

# WAVE-12 CLOSE (2026-06-20): **9404 / 65546** (9310 → 9404, +94, build green)
wave12-metaband-belt-2 (15 agents, audit-key bug FIXED via TU-keyed clearing): 3
meta_band ports, all audited HONEST+splits-clean — ManageBandPanel +62,
Award.cpp +23 (the bisection-scout lane PORTED the first clean sub-TU out of the
big [0x825C3A44,0x825D0EF0) gap), PatchSelectPanel +9. DEFERRED with evidence:
MainHubPanel (scattered ~9MB, NO contiguous span — documented big-scattered-TU
negative, do NOT span-pin; its 44/45 fns are ICF aliases = reveal-sweep territory
under foreign pins, never its own pin). SavedSetlist +0 (needs additive
FixedSizeSaveable.h 4-arg SaveStd/LoadStd vector template overloads — flagged
foundational, gate on composed A/B; didn't land this pass).
NEXT (wave-12 reducer, belt MATURING — character shifting to scatter/relocate/
dependency-chain): the two big gaps still need full string-fingerprint bisection
([0x825BDF28,0x825C10D8)~110fn + rest of [0x825C3A44,0x825D0EF0)~565fn, Award
carved a piece); AccomplishmentPlayerConditional/SongConditional sliver-evict in
GAP B; ProfileMgr→SaveLoadManager + NavListSortNode→SongSort dependency chains;
SavedSetlist (retry with the header overload). Re-run pin_audit (each port adds
sliver candidates). Cost-per-match rising (panels scatter, oracle thinning).

**SESSION TOTAL (waves 3–12): 6932 → 9404 (+2472, build green; every landed
lever composed-verified EXACT).**

# ============================================================
# WAVE-13 CLOSE (2026-06-20): **9454 / 65552** (9404 → 9454, +50, build green)
# ============================================================
wave13-belt-bisection (wf_6e9aaa4c-105, 14 agents; single-pass INDEPENDENT
fan-out, one TU per discover lane). 3 land-ready lanes, all audited HONEST +
splits-clean, composed-verified EXACT (run1=run2=9454, no divergence):
- **SavedSetlist-retry +33** — ported band3/meta_band/SavedSetlist.cpp, the W12
  deferral RESOLVED (byte-exact pair via content-match; the FixedSizeSaveable
  overload worry didn't block the contiguous cluster). .text [0x82590C70,
  0x82592270), abuts Campaign.cpp end exactly at 0x82590C70. Largest gain.
- **gapA / CharData.cpp +14** — the GAP A [0x825BDF28,0x825C10D8) bisection-scout
  lane proved the gap is exactly TWO sub-TUs: (1) CharData.cpp [0x825BDF28,
  0x825BEBD8) 36 fns UNWIRED = ported here; (2) OvershellSlot.cpp HEAD
  [0x825BEBD8,0x825C10D8) 74 fns WIRED-but-under-pinned = frontier. Boundary
  proven by string fingerprints (prefab_* end the lower, msg_duration/state_handlers
  begin the upper) + IsQuitToken@0x825BFB08 byte-matching the compiled
  OvershellSlot.obj. CharData now ABUTS CriticalUserListener below and unblocks
  the OvershellSlot-head extension above.
- **gapB / AccomplishmentSetlist +3** — RELOCATE (Waypoint pattern): dead sliver
  pin [0x8243F220,0x8243F330) (0/8 matched) → real cluster [0x825CBC58,0x825CC010).
  Splits-relocate + 5 ADD-only map entries, NO .cpp change (already wired+compiling).
  Proved the GAP B vein: the Accomplishment*Conditional family is mostly
  DEAD-SLIVER pins whose real clusters sit contiguously in [0x825cc010,0x825d0ef0).
- SongSortNode = HONEST NEGATIVE (+0): its 53 methods are ICF-scattered binary-wide
  (0x8226f…–0x82b4af…), NOT a contiguous cluster — the proposed 0x826438C0 span is
  empty. Classic Waypoint/Object scatter. Lever (deferred) = per-fn identity transfer
  across the scattered owning TUs (multi-TU, not self-contained). Fold with SongSort.cpp.
- SaveLoadManager = FOUNDATIONAL-FLAGGED (not landed): match ceiling gated by
  ProfileMgr.cpp (TheProfileMgr.* calls need ProfileMgr's interface+layout first).
  Sequence the keystone chain ProfileMgr → BandProfile → SaveLoadManager; do NOT bundle.

TOOLING: the coordinator harvest/land helpers were promoted /tmp → **scripts/harvest/**
(land.sh + resolve_json_union.py + resolve_splits_union.py + README), generalized to
derive repo-root/script-dir and accept a worktree-path OR branch-name. SOP doc updated.

### NEXT WAVE (wave-13 reducer frontier — gapB conditional RELOCATE belt is the
### single richest near-term seam, ~+60 of mostly-MECHANICAL relocates, no body work
### for already-matching fns; all wired+compiling, just splits-relocate + map-add):
1. **AccomplishmentPlayerConditional relocate(+bodyport) +20** — dead sliver
   @0x8243F178 (0/5) → real cluster [0x825ccbe0,0x825ce5a8) 58 fns (best_score/
   career_fills/total_bre_hits anchors); some bodies diverge (Configure 0x130 vs
   0xD8) so couple relocate with a small body-port.
2. **gapB-conditionals fresh-pin batch +25** — OneShot [0x825cc010,0x825cc220),
   TrainerConditional/TrainerCategory/TrainerList, SongListConditional,
   DiscSongConditional — unpinned-no-sliver wired+compiling conditionals filling
   GAP B around the relocated AccomplishmentSetlist.
3. **overshellslot-head-extension +10** — extend the existing OvershellSlot .text
   pin DOWN from 0x825C10D8 to 0x825BEBD8 (now abuts CharData); gen_game_target_map
   --tu OvershellSlot.cpp --apply for the head addrs; body-port divergent head fns.
4. **LockStepMgr +20** — upper neighbour to SavedSetlist [0x82592270,~0x82595540)
   ~136 anon fns, unpinned+unwired net-message TU (releasing_lock_step/
   BasicStartLockMsg/EndLockMsg/LockResponseMsg). Port-then-pin.
5. Smaller Acc* relocates: AccomplishmentSongFilterConditional [0x825cf390,0x825cf8f8),
   AccomplishmentSongConditional [0x825cc220,0x825ccbe0) (modest), AccomplishmentCategory
   tail [0x825d0e50,0x825d0ef0) + Award 0xD00→0xE50 tail-extend.
6. KEYSTONE CHAIN (own short wave, sequence don't bundle): ProfileMgr.cpp +20 →
   BandProfile.cpp +12 → SaveLoadManager; + saveload-sibling-cluster [0x8252E6B0,
   0x82532068) +15 (vtable 0x8209002C). SongSort.cpp co-located with SongSortNode.
7. Region B1 [0x825C3A44,0x825CB590) (~30KB OvershellSlot strings) needs a decision:
   OvershellSlot unpinned tail (extension) vs a coupled provider TU. Re-run pin_audit.

**SESSION TOTAL (waves 3–13): 6932 → 9454 (+2522, build green; every landed
lever composed-verified EXACT).**

# ============================================================
# WAVE-14 CLOSE (2026-06-20): **9477 / 65554** (9454 → 9477, +23, build green)
# ============================================================
wave14-gapb-relocate-belt (wf_ff7c3d73-07b, 8 agents: 3 execute + 1 read-only
scout + audits + reduce). composed-verified EXACT (run1=run2=9477).

LANDED (the ONLY honest lane):
- **gapb-belt +23** (one-owner of the contiguous GAP B tiling [0x825cc010,0x825d0ef0)).
  3 dead-sliver RELOCATES → real clusters (AccomplishmentCategory tail @0x8243EF98→
  [0x825D0E50,0x825D0EF0); AccomplishmentPlayerConditional @0x8243F178→[0x825CCBE0,
  0x825CE5A8); AccomplishmentSongFilterConditional @0x8243F378→[0x825CF390,0x825CF8F8))
  + Award.cpp own-tail extend (.text 0xD00→0xE50, +2) + 6 fresh-pins (OneShot,
  Trainer/TrainerCategory/TrainerList Conditional, SongList(+0), DiscSong). Audited
  HONEST (longest foreign run = 0; PlayerConditional fns ref best_hopos_percent/
  career_fills = conclusively own; byte-unique own funclets; BinDiff hits onto them =
  ICF address-alias FPs). Agent honestly REVERTED a net-negative relocate
  (AccomplishmentSongConditional → [0x825CC220,0x825CCBE0) gains 0 while losing 3
  sliver trivials; left unpinned). map +5 ADD-only.

⭐⭐ HONESTY AUDIT CAUGHT +57 OF ICF-ALIAS INFLATION — two span-pins REFUTED, NOT landed:
- **OvershellSlot-head +18 = REFUTED.** The "head" [0x825BEBD8,0x825C10D8) is a MIXED
  multi-TU blob, NOT a clean OvershellSlot cluster: the rb3-Wii BinDiff oracle maps the
  confident named hits to FIVE TUs (MetaPerformer/SaveLoadManager/BandMemberProvider/
  ClosetPanel + one OvershellSlot hit itself at 0%); the 15 "matches" are all sim-0.00
  ICF-foldable trivia. The +18 is swept-in foreign code. ⚠ This OVERTURNS the wave-13
  gapA frontier item "OvershellSlot owns the head": IsQuitToken@0x825BFB08 byte-matching
  the compiled obj was REAL but only proves a FEW OvershellSlot fns are interleaved
  there — the head is mixed. DO NOT span-pin it.
- **LockStepMgr +39 = REFUTED.** [0x82592270,0x82595540) is Quazal/network FOREIGN code
  (Quazal::DuplicationSpace/DORef/StationURL, AccountManagementProtocolDDL, BudgetScreen,
  NetSync, OvershellPanel). LockStepMgr is ICF-scattered binary-wide (Init@0x82598d80,
  StartLock@0x82b7f3b0, OnMsg@0x8253d040 … NOT ONE method in the span) — same negative
  as SongSortNode/MainHubPanel. The +39 are all ≤44B ICF stubs with no LockStepMgr
  identity. The fresh port compiled fine but is useless without per-fn identity work.
  ⚠ OVERTURNS the wave-13 "LockStepMgr +20" frontier item. DO NOT span-pin.

⭐ LESSONS (this wave's main payoff):
- **byte-match ≠ ownership when ICF folding is in play.** Trivial ≤44B stubs (??_E
  deleting-dtor thunks, one-line getters) fold byte-identically across unrelated TUs, so
  "our compiled obj byte-matches N fns in this span" does NOT prove the span is our TU.
  The decisive test is the BinDiff oracle's named-hit attribution + fn SIZE (real method
  bodies, not stubs) + a >=8-contiguous-foreign-0% check.
- **string presence ≠ contiguous ownership** in a mixed/scattered blob (OvershellSlot
  head had OvershellSlot-only strings yet was a 5-TU blob). Gate "extend on strings"
  claims on a BinDiff own-vs-foreign pass.
- **splits-clean ≠ honest.** The reducer's landing_guide ranked LockStepMgr #1 on
  "clean splits / zero collision" — but that's orthogonal to attribution. The AUDIT is
  the gate; trust it over the EV ranking.
- gapb-belt over-estimated (+23 not the briefed ~+45-55): the big conditional TUs
  (Player 58 / DiscSong 31 / SongConditional 24) are content-divergent STL (non-inlined
  vector/set<Symbol> instantiations whose mangled names have no in-cluster oracle entry);
  only reveal-byte-exact fns + clean small relocates register. The cheap relocate vein is
  bottoming out — remaining GAP B value is BODY-PORT work.

### WAVE-15 FRONTIER (scout-corrected; cheap span-pin vein THINNING, frontier
### bifurcating into (a) genuine-contiguous ports + (b) body-ports):
1. **ProfileMgr.cpp port-then-pin** @[0x82534D38,0x825395B8) (~162 fns / 18960B, 12
   substantial >=200B fns) — a REAL contiguous cluster (fn_82534D38 refs `profile_mgr`
   @0x82091630). Wave-13's SaveLoadManager scout WRONGLY swallowed this into its span;
   ProfileMgr is the lower portion, SaveLoadManager the upper. The keystone that unblocks
   SaveLoadManager. Oracle ../rb3/src/band3/meta_band/ProfileMgr.cpp. EV ~+20-30.
2. **saveload-sibling = MusicLibrary-family panel** [0x8252E6B0,0x82532068) (vtable
   0x8209002C; strings qp_party_shuffle/can_headers_be_selected/get_back_screen/sort/
   profile_pre_delete_msg → MusicLibrary.cpp family). Identify exact class via vtable
   dump + wire+pin from rb3-Wii. EV ~+15.
3. **GAP B conditional BODY-PORT sub-wave** — clusters now pinned+confirmed but ~150 fns
   at 0% need body-ports. PlayerConditional Configure 0x130-vs-0xD8 = a struct-layout
   lever (condition/tracker vector member-count delta) LIKELY CASCADING across the family.
   Port Configure/IsFulfilled/Inq* from ../rb3/src/band3/meta_band/. EV ~+30-50 if the
   struct fix cascades. This is the real remaining GAP B value.
4. **Region B1 [0x825C3A44,0x825CB590)** — scout says OvershellSlot's own unpinned tail
   (strings swap_user/p_providers/setup_providers @ OvershellSlot.cpp line nums). ⚠ GATE
   on a BinDiff own-vs-foreign pass FIRST (the head burned us on identical string
   reasoning); only extend if BinDiff confirms a contiguous OvershellSlot run.
5. DEFERRED — ICF-scattered, per-fn identity-transfer ONLY (no contiguous span; do NOT
   span-pin): BandProfile (104 fns 0x822639F0..0x82BD66B0), SongSort (14 fns), SongSortNode
   (60 fns), LockStepMgr. Same class as MainHubPanel.
6. Re-run pin_audit.py on the post-wave-14 state (new pinned source = new sliver candidates).

**SESSION TOTAL (waves 3–14): 6932 → 9477 (+2545, build green; every landed lever
composed-verified EXACT; honesty audit refuted +57 ICF-alias inflation this wave).**

# ============================================================
# WAVE-15 CLOSE + IDENTITY-TRANSFER MILESTONE (2026-06-21):
#   **9552 / 65564** (9477 → 9535 wave-15 +58, → 9552 idtransfer +17; build green)
# ============================================================

## WAVE 15 (matching) — gapb-bodyport +58 (wf_66a1ebb1-2cd)
- **gapb-bodyport +58** (9477→9535, composed-verified, ZERO regressions). PURE BODY-PORT
  (splits.txt byte-identical). ⭐ STRUCT LEVER (cascaded across the AccomplishmentConditional
  family as predicted): retail-360 stores conditions in **std::list<AccomplishmentCondition>**
  (sentinel _M_node at this+0x90), NOT the std::vector the rb3-Wii DEV oracle was converted to.
  Fix = vector→std::list in AccomplishmentConditional.h + a **+0x10 base tail pad on the shared
  Accomplishment.h** (retail places the derived list at 0x90 vs our base-sum 0x80). Cascaded:
  PlayerConditional 1/58→55/58 (Configure 62→100, IsFulfilled 79→100, InqBestProgressValues→100)
  across OneShot/SongConditional/LessonDiscSong/LessonSongList. Shared-header change → gated on
  whole-binary composed A/B (soft-rule), held +58/0. Commit landed.
- profilemgr **self-refuted/DEFERRED** (NOT a span-pin failure — ownership PROVEN): span
  [0x82534D38,0x825395B8) is genuinely ProfileMgr.cpp, contiguous + overlap-clean (the wave-13
  SaveLoadManager scout over-swallowed it; ProfileMgr = lower portion). Blocked on (A) gen_game_target_map
  produced 0 oracle pairs → needs HAND-BUILT target-map entries; (B) Wii-divergent substantial
  methods (Rnd::SetOverscan/ConfigureRenderMode, WiiFriendMgr, Mic APIs) need per-method 360
  reconstruction. Multi-hour; deferred with the span pre-verified.
- saveload-sib **self-refuted/DEFERRED + IDENTIFIED**: the cluster [0x8252E6B0,0x82532068) is
  **MusicLibrary.cpp's 2nd .text cluster** (vtable 0x8209002C 4-base shape matches
  `MusicLibrary : UIListProvider, Hmx::Object, ContentMgr::Callback, Synchronizable`; ctor
  allocates ViewSettingsProvider/SetlistProvider/MusicLibraryNetSetlists/SetlistScoresProvider).
  A dual-range pin scored +57 but the lane's OWN ICF-alias self-check REFUTED it (56/57 are
  ≤44B stub folds, 1 foreign ProfileChangedMsg) → REQUEUE as a real port-then-pin of range-2.
- scout: **B1 [0x825C3A44,0x825CB590) = REFUTED-mixed-blob** (87% ≤40B stub farm, 49 oracle hits
  across 30 TUs, ZERO OvershellSlot) — the BinDiff gate correctly killed the wave-14 string-based
  "OvershellSlot tail" premise. SaveLoadManager-upper also refuted (scatters). pin_audit re-ran DRY.

⭐ STRATEGIC (the wave's headline): **contiguous-port inventory is near-exhausted for the
meta_band priority tier.** The belt has MATURED to the SCATTER PHASE — OvershellSlot/ProfileMgr/
SaveLoadManager/MusicLibrary-2/MainHubPanel are all ICF-folded binary-wide. What still produces
matches: (a) **BODY-PORTS with a real oracle + struct lever** (the +58), and (b) **identity-transfer
for scattered TUs** (below).

⭐ LESSONS:
- The ICF-alias **self-check works** — saveload-sib refuted its OWN +57 before the audit ran, and
  the B1 BinDiff gate refuted a string-based premise. Honesty is now self-enforcing in the lanes.
- Workflow filter bug (coordinator caught, like wave-11 Campaign): the auditor set
  `stub_dominated=true` on the +58 body-port (a span-pin concept misapplied — its own verdict said
  "HONEST/LANDABLE"), which dropped the wave's only winner from `cleared`. NEXT: gate stub_dominated
  on span-pins ONLY, never body-ports.

## IDENTITY-TRANSFER TOOLING MILESTONE — tool PROVEN + RockCentral +17 (wf_f290fa44-aae)
- Built **`tools/identity_transfer.py`** (410 LOC) + the full mechanism spec
  (**docs/decomp/identity-transfer.md**). Multi-range micro-pinning works for arbitrary N
  (verified against jeff `apply_splits` RAW push / `split_obj` name-grouping / `create_gap_splits`
  auto-gap-ownership). Each scattered method → one `.text [VA,VA+pdata_len)` micro-range under the
  TU header; all merge into one TU obj; oracle-named; objdiff pairs. **Feasible with CURRENT tools,
  NO fork** for case-A.
- **RockCentral.cpp +17** (9535→9552, composed-verified run1=run2, 0 regressions): the tool
  classified 129 bodies → 104 case-A / 25 case-B (SKIPPED to worklist), coalesced 102 into 80
  `.text` micro-ranges, dtk auto-backfilled 77 `.pdata` entries, +17 all genuine RockCentral bodies.
  (RockCentral chosen over BandProfile for a zero-port-cost mechanism proof — already wired at 96%.)
- **case-A** (method in unowned auto_ blob) = works now. **case-B** (method inside a foreign pin) =
  deferred (objdiff pairs within-unit); an optional objdiff "global byte-equality" fork would unlock it.
- ⭐ This UNBLOCKS THE SCATTER PHASE: BandProfile (70 case-A), SongSortNode, LockStepMgr, MainHubPanel
  — gated only on porting each TU's MWCC source so the obj defines the methods, NOT on the mechanism.

### WAVE-16 FRONTIER (frontier now = body-ports + identity-transfer; span-pins spent):
1. **Identity-transfer generalization**: port BandProfile.cpp source → `identity_transfer --tu
   BandProfile.cpp --apply` (70 case-A methods). Then SongSortNode/LockStepMgr/MainHubPanel. Honesty:
   per-unit A/B, confirm newly-100 are real bodies not ≤44B stub folds (STL-heavy clusters = ICF risk).
2. **MusicLibrary.cpp range-2 port-then-pin** [0x8252E6B0,0x82532068) from rb3-Wii (123 named methods;
   ctor/OnEnter/InitData-already-100 confirm source alignment) — real per-method porting, not splits-only.
3. **ProfileMgr.cpp** [0x82534D38,0x825395B8) (span pre-verified): hand-build target-map entries +
   reconstruct ~6 Wii-divergent methods (clean getters cheap).
4. **More struct-lever BODY-PORTS** in already-pinned-but-0% clusters (the AccomplishmentConditional
   std::list win is the template — look for other vector-vs-list / member-layout divergences).
5. Generalize the ICF-alias name-set-diff self-check into the wave SOP (saveload-sib's follow-up).
6. (Later/optional) objdiff global byte-equality fork → unlocks identity-transfer case-B methods.

**SESSION TOTAL (waves 3–15 + idtransfer): 6932 → 9552 (+2620, build green; every landed lever
composed-verified EXACT; honesty self-checks refuted span-pin ICF-alias inflation throughout).**

# ============================================================
# WAVE-16 CLOSE (2026-06-21): **9558 / 65564** (UNCHANGED — 3 honest self-refutes, +0 landed)
# ============================================================
wave16-identity-transfer-sweep (wf_11185ca4-a07). A PRODUCTIVE NEGATIVE wave — redrew the
frontier. Nothing landable; all three execute lanes self-refuted honestly:
- **idt-sweep +0**: the "RockCentral +17 for free" pattern does NOT generalize. ~90 of 94 wired
  game TUs ALREADY carry a contiguous span pin; appending identity-transfer micro-pins to a
  span-pinned TU mints duplicate mangled-name target fragments that STEAL objdiff pairing from
  the already-matching real method → **net regressions** (−14 across VocalTrack/GemTrack/SongDB/
  VocalPlayer/Player). The 4 unpinned wired TUs (SongRecord/OvershellSlotState/SongSortByRank/
  CampaignLevel) yielded 0 real byte-matches. ⭐ identity_transfer.py needs two fixes: (1) DETECT
  a TU that already has a span pin → skip/filter colliding micro-pins; (2) the dry-run "named
  bodies" metric is NON-PREDICTIVE (counts already-matched methods) → a truthful estimator must
  diff against the current whole-binary 100-set and count only NOT-yet-matched real-bodied (>44B,
  sim≥0.5) methods.
- **BandProfile +0**: mechanism worked END-TO-END (ported 1013-line MWCC→MSVC, obj defines 115
  symbols, carved 64 micro-ranges, named 23) but **ZERO reached 100%** (best fuzzy 47.8%, ctor
  1.7%). ⭐ NEW WALL — **ported-body-divergence**: ported MWCC→MSVC bodies diverge from retail
  everywhere; + oracle VA mis-attribution on tiny ICF stubs (oracle maps accessors to WRONG VAs).
  Port branch **w16-bandprofile @ ec65595 KEPT** for re-derivation under a different body-port strategy.
- **musiclib-r2 +0**: the range-2 dual-range pin scores +57 but it's the **FAKE ICF-stub fold**
  (113/125 range-2 fns are ≤44B stubs; only 1 — MusicLibraryTask::operator= @0x8252E998 156B —
  uniquely byte-matches our obj). Same shape as the wave-15 +57. The new `icf_alias_check.py` flags it.

## TOOLING DELIVERABLE (lesson → automation)
- **`tools/icf_alias_check.py`** (committed 23bb6ee, wired into the SOP audit step 1d95113): automates
  the ICF-alias inflation gate (waves 14/15/16's recurring +57 fake-match shape). `--worktree` newly-
  matched-diff mode = exit 1 on stub-fold-dominated gains. byte-match ≠ ownership under ICF folding.

## VEIN STATUS — cheap matching levers DRY; two real levers remain
- **Contiguous-port-then-pin: EXHAUSTED.** **identity-transfer: THIN** (RockCentral was a special
  case — wired+96%+unowned-blob+no-competing-span; that combination is rare). Relocate/sliver: spent.
- ⭐ **(1) STRUCT-LEVER BODY-PORTS** = the best remaining MATCHING lever. The AccomplishmentConditional
  std::list +58 is the template: a single wrong this-relative member offset / vector-vs-list / base-size
  delta that CASCADES across a class family. Hunt near-miss clusters for the single-member tell.
- ⭐ **(2) objdiff GLOBAL BYTE-EQUALITY FORK** = the highest-CEILING lever (+100 potential binary-wide):
  unlocks identity-transfer **case-B** (methods physically inside a foreign pin, which objdiff can't
  pair within-unit today). A freeqaz objdiff-core fork — a global second pass matching an unmatched
  named target fn against a byte-identical base symbol in ANY unit's obj (report.rs reads all base objs).
  Substantial Rust fork; the "build new tooling" path.

### WAVE-17 PLAN (WIDER — 2× threads, two concurrent ultracode workflows):
A. **Matching (wide struct-lever body-port hunt)** — many lanes, each scanning a different already-
   pinned near-miss cluster family for a cascading member-offset divergence + porting the fix.
B. **Tooling (objdiff global-byte-equality fork)** — Understand→Design→Prototype the case-B unlock +
   the identity_transfer.py collision-safety + truthful-estimator fixes.

**SESSION TOTAL (waves 3–16): 6932 → 9558 (+2626; wave-16 +0 = honest negatives that redrew the
frontier; tooling: icf_alias_check.py landed).**

# ============================================================
# WAVE-17 CLOSE (2026-06-21): **9598 / 65564** (9558 → 9598, +40 matching; build green)
#   + WAVE-17B tooling (identity_transfer fixes landed, objdiff case-B fork BANKED)
# ============================================================
WIDE 2-track wave (user: "double the threads"). 17A = 8-lane struct-lever body-port hunt
(wf_62bf7d30); 17B = objdiff case-B fork + identity_transfer fixes (wf_547dda28).

## 17A MATCHING +40 (4 honest lanes; 4 families flat AT-LIMIT)
- **uicomponent +14** (struct lever): UIList::mListDir is a raw UIListDir* (4B) where DC3 has a
  fatter member → uniform +0xC this-relative member-offset shift. UIList.h/.cpp only, clean.
- **metaband +14** (struct lever): OnlineID 0x10 size fix (XUID-based 360 OnlineID 8B larger) +
  UIListProvider non-virtual tail; the one GENUINELY CASCADING lever (leaderboard family). Also
  SongUpgradeMgr/SongSelectPanel body-ports. (Touches OnlineID.h, UIListProvider.h.)
- **object +1** (struct lever): dropped the DC3-added `int indent` from Data{Node,Array}::Print
  (4→3 args, RB3-retail sig). Wide-touch (Data.h + 13 callers) but zero regressions.
- songmgr +2 DROPPED at landing (its SongUpgradeMgr work overlapped metaband's 6ee483f — the
  double-count; took metaband's superset to avoid a .cpp conflict).
- ⭐ The 3 landed levers CASCADE FURTHER IN COMBINATION: individually +14/+14/+1 but the combined
  composed-verify = **+40** (shared UI-list / OnlineID / Data headers compound). composed run1=run2=9598.
- AT-LIMIT families (net 0, all walls): **vocaltrack** (VocalTrackDir = retail-vs-Wii-DEV version
  divergence — wrong oracle, needs Ghidra reconstruction), **player** (member-base/vbase/regalloc),
  **gemtrack** (see follow-ups), **character** (CharHair struct lever found but needs full TU revert).

## 17B TOOLING
- **identity_transfer.py FIXED + landed** (6b138ec): hard-skips span-pinned TUs (the wave-16 −14
  collision root — appending micro-pins to a span-pinned TU steals pairing) + truthful `--estimate`
  vs the current 100-set (the dry-run "named bodies" metric was non-predictive).
- **objdiff case-B fork BANKED** (doc docs/decomp/handoff/objdiff-caseb-fork-banked.md, a116553):
  branch `caseb-global-byteeq @ b1c92be` in ../objdiff, built ISOLATED to /tmp (shared binary
  untouched). Global byte-equality pass in the report driver (NOT diff_objs); honest gate =
  masked-bytes + reloc-target-NAME equality + REQUIRED oracle (sim≥0.5, attributes to unit).
  Honest unlock now = +0 (do-no-harm, off-by-default); +150–220 ceiling gated upstream on porting
  scattered TUs. Full integration checklist in the doc. (Captured ~362 lines of pre-existing WIP.)

⭐ VEIN STATUS — APPROACHING STUCK on the CURRENTLY-MINED families (meta_band belt + core engine
near-misses), NOT globally stuck. Struct levers are now isolated single-class fixes (not wave-9
keystones); 4/8 families flat; body-port tail thinning to permuter/ICF-class. The struct-lever
hunt's remaining value is in NEWLY-PINNED units → PIN FRESH INVENTORY before the next struct-lever wave.

### WAVE-18 FRONTIER (PIVOT to fresh inventory + cheap reveals + orthogonal tooling; WIDE 2-track):
A. MATCHING/FRESH-INVENTORY (wide): (1) **gemtrack map-augmentation** — ~53 GemTrack methods are
   byte-identical to the oracle but UNPAIRED (missing target_symbol_map entries) → `gen_game_target_map.py
   --tu GemTrack.cpp` + reveal = cheap; (2) **map-aug SWEEP** generalize that across wired game TUs;
   (3) **CharHair full-revert** struct lever (DC3-newer → rb3-Wii form, −8 cascade, 4 asm anchors);
   (4) **SongStatusMgr** index-loop body-port; (5) **pin_audit refresh** (new re-pins from all
   wave-13..17 pins); (6) **belt-gap bisection** + new un-pinned contiguous game-TU pins (BinDiff-
   confirmed) — fresh inventory the struct-lever hunt can then mine.
B. TOOLING (orthogonal unstick): **jeff funclet-truncation fix** — the recurring dtk asm-misnest
   bug (GemTrack::See, Award ctor, LicenseMgr) truncates a function at a premature .endfn → false
   0% + lost binary tail. ../jeff src/cmd/xex.rs. Build ISOLATED, validate do-no-harm before integrating.

**SESSION TOTAL (waves 3–17): 6932 → 9598 (+2666; wave-17 +40 matching + tooling: identity_transfer
fixes, objdiff case-B fork banked, icf_alias_check.py).**

## WAVE-17 REFILL ADDENDUM (main @0cd9116): **9617** (9598 → 9617, +19)
The OnlineID 0x10 struct-lever CASCADED through reveal_sweep (refill was NOT a no-op this time,
unlike the belt-pin waves): +19 = UIList +10 (ChildList/Poll/SelectedSym/SetSelected/OnScroll…),
ViewSettingsProvider +7 (RefreshAllSettings/Mat/Text/SelectSetting…), MusicLibrary +2. Confirms
struct-lever body-ports refill the reveal pool (vs relocate/belt pins which cascade ~0).
**WAVE-17 TOTAL = +59 (9558 → 9617). SESSION (waves 3–17): 6932 → 9617 (+2685).**

# ============================================================
# WAVE-18 CLOSE (2026-06-21): **9739 / 65547** (9617 → 9739, +122; build green)
#   18A matching +14 (charhair) + 18B TRUNCATION FIX +108
# ============================================================
WIDE 2-track. 18A = 8-lane fresh-inventory pivot (wf_f4ad9d22); 18B = jeff funclet-
truncation fix (wf_6b96aa50).

## 18A FRESH-INVENTORY PIVOT: +14 (charhair only; 7/8 lanes refuted entire classes)
- **charhair-revert +14** (DC3-drift STRUCT LEVER): CharHair.cpp/.h in our tree were the
  DC3-newer (rev-13) revision (extra mWind/mFlat/mWindObj members); RB3-retail is rev-11.
  Reverting to the rb3-Wii form fixed a −8 this-relative member cascade across the CharHair
  cluster. SAME mechanism class as wave-17 OnlineID/UIList/Data. Landed via cherry-pick (a6e4fb9).
- ⭐ DECISIVE NEGATIVES (7 lanes closed candidate CLASSES): **fresh-inventory contiguous-TU
  ports EXHAUSTED** (belt-gap-bisect + 2 fresh-TU lanes agree: no unwired band3 TU forms a
  contiguous oracle-backed span — all remaining unpinned game code is ICF-scattered);
  **map-augmentation NOT a broad vein** (72-TU exhaustive scan: gemtrack's "byte-identical
  unpaired" claim is FALSE — they're divergent near-misses, not free reveals; one-off at best);
  **pin_audit DRY** (16 candidates all fail own-vs-foreign). The ONLY productive matching vein
  is **DC3-vs-RB3 version-drift struct levers** (revert DC3-newer members RB3-retail lacks).

## 18B ⭐⭐ TRUNCATION FIX +108 (biggest single lever since the Handle keystone)
- ROOT CAUSE (not a live dtk bug): a STALE committed **symbols.txt** cache. dtk's CFA derives
  the correct length from .pdata, but `Symbols::add(replace=false)` kept the existing stale
  `size:0x28` for same-named auto `fn_` symbols, and clamp only SHRINKS. 1207 functions carried
  a truncated size frozen by an older jeff → each split into a 40-byte stub + an orphan tail
  objdiff couldn't pair (false 0%); some truncation stubs even FALSELY byte-matched (ICF-stub class).
- FIX (data-level, **stock-dtk-compatible**): regenerated symbols.txt via the
  `grow_undersized_function_symbols` pass (jeff branch `fix-funclet-truncation @39e482f`,
  do-no-harm validated: 1207 grow pairs, 0 shrinks, function set identical, 0 overlaps) forcing
  each size to its pdata length, + 5 splits.txt .text-end extensions (Rand/JoypadClient/HDCache/
  SongPreview/FlowSetProperty, pins authored around the old truncated sizes). Committed 548fbf9.
- Honest +108 = full-body restorations now byte-matching (GemTrack::See 0%→100%, Award ctor
  0x28→0x8C, RndPropAnim::ValueFromFrame, Sequence::OnPlay, LicenseMgr::ContentLoaded…). The fix
  REDUCES stub-fold risk (eliminates truncation stubs). Stable run1=run2=9739; stock dtk + the
  committed symbols.txt also yields 9739 (the committed artifact is self-sufficient — the fixed
  dtk is kept locally as a future-truncation safety net only).
- ⭐ REFILLS THE FRONTIER: 1875 functions were truncated (649 in pinned ranges); only ~108 now
  match — the rest are now FULL-BODIED NEAR-MISSES previously hidden by truncation = a fresh
  body-port pool. So we are NOT stuck — the truncation fix opened new inventory.
- BUILD INCIDENT (recovered): an 18B reader's grown-size analysis leaked into main's gitignored
  `function_analysis/` cache, corrupting dtk symbol-sizing → a phantom "Rand.cpp ends within
  symbol" split abort. Cleared by removing the stray cache. (Lesson: read-only analysis lanes
  must not write analysis caches into the MAIN tree.)

### WAVE-19 FRONTIER:
1. **Truncation-refilled body-port pool** — the ~541 now-visible full-bodied near-misses in pinned
   ranges (1875 truncated − ~108 matched − unpinned). Many are real methods (RndPropAnim/Geo/
   Sequence/LicenseMgr…) that were hidden; objdiff + port the divergent ones. NEW cheap-ish inventory.
2. **DC3-drift struct-lever hunt** (the proven matching vein): more classes where our DC3-sourced
   tree carries later-revision members RB3-retail lacks (CharHair/OnlineID/UIList/Data pattern).
3. gemtrack permuter near-misses (NextKickNoteMs 99.97%, SetEnableSlot 99%, …) — low-EV permuter pass.
4. objdiff case-B fork (banked) + the now-integrated truncation fix together unstick more.

**SESSION TOTAL (waves 3–18): 6932 → 9739 (+2807). Wave-18 +122 (charhair +14, truncation fix +108).**

## WAVE-18 REFILL ADDENDUM (main @44de4c1): **9772** (9739 → 9772, +33)
The truncation fix cascaded a strong reveal wave: +33 = 33 restored full-bodied functions now
byte-exact + named via pin_identified/reveal_sweep (map-only, ADD-only, 0 regressions). Confirms
the truncation fix REFILLS the reveal pool, not just the body-port pool.
**WAVE-18 TOTAL = +155 (9617 → 9772). SESSION (waves 3–18): 6932 → 9772 (+2840).**

# ============================================================
# WAVE-19 CLOSE (2026-06-21): **9788 / 65547** (9772 → 9788, +16; build green)
#   truncation-refilled body-port harvest + DC3-drift struct-lever hunt (8 wide lanes)
# ============================================================
5 lanes landed (composed run1=run2=9788, +16 = clean sum, no cascade surprises), 3 refuted:
- **trunc-sequence +6**: MidiInstrument::Save (missing mFaders.Save line + Faders.h decl),
  DelayEffect::SetParameter (clamp-order), Mic.cpp. synth family truncation near-misses.
- **trunc-rndobj +4** ⭐ INLINE-POLICY FORCE-MULTIPLIER: RndAnimatable::SetFrame was defined
  OUT-OF-LINE in Anim.cpp with the DC3-newer body (mFrame!=frame guard + BroadcastPropertyChange);
  retail RB3 + rb3-Wii define it INLINE (`{mFrame=frame;}`). Without LTCG, cross-TU callers
  (Gen/PropAnim/Group/Part) emitted `bl SetFrame` where retail inlines `stfs`. Moving it inline
  into Anim.h cascaded +4 across the family. SAME CLASS as CharHair/OnlineID DC3-drift but on
  INLINE-POLICY not member-offset.
- **dc3drift-engine +3**, **trunc-obj-engine +2** (ctor methods), **trunc-math-geo +1**
  (Rand::Gaussian: Float(float,float) is __declspec(noinline), retail inlines the [-1,1] map →
  rewrote Float(-1,1) as Float()*2-1 = another inline-policy fix).
- REFUTED: trunc-metaband (struct lever found, composed net 0), trunc-bandtrack (permuter-class:
  GemTrack Poll/CheckShifts FP-regalloc), dc3drift-game (struct lever found, net 0).

⭐ NEW VEIN — **INLINE-POLICY FORCE-MULTIPLIER**: a method DC3 defines OUT-OF-LINE (often with a
DC3-added guard/broadcast body) that retail RB3 + rb3-Wii define INLINE in the header. Without
LTCG, every cross-TU caller emits a `bl` where retail inlines the body → fixing ONE header inline
cascades to ALL callers. The only force-multiplier still firing. QUEUED: MemDoTempAllocations
(MemMgr.h, frontier rank 1) + a binary-wide hunt for the pattern.

⭐ VEIN STATUS — THINNING, cost-per-match RISING. The truncation fix refilled the body-port pool
and wave-19 harvested ~50-60% (+13); the remainder (~13) is dominated by PERMUTER-CLASS residue
(FP regalloc cascades, commutative-operand swaps, char-signedness codegen) that does NOT respond
to source reordering — needs the permuter tool, low EV. The remaining productive levers are: (a)
the inline-policy force-multiplier hunt, (b) a permuter sweep on the accumulated >=97% near-misses,
(c) the HARD frontier (objdiff case-B fork [banked] + scattered-TU ports w/ body-divergence wall).

### WAVE-20 FRONTIER:
1. **INLINE-POLICY FORCE-MULTIPLIER HUNT** (binary-wide): find methods DC3 defines out-of-line that
   retail/rb3-Wii inline (the SetFrame/Gaussian pattern) — each cascades to all cross-TU callers.
   Start MemDoTempAllocations (MemMgr.h); scan base-class headers (Rnd*/Obj/Char/UI/math value types).
2. **PERMUTER SWEEP** on the accumulated >=97% near-misses (math/geo Multiply/OnSide/CheckBSPTree,
   SHA1 Update/Final/ReportHash, RndGenerator::SetFrame commutative-fadds, gemtrack NextKickNoteMs
   99.97%/SetEnableSlot 99%) — automated `decomp_synth` permuter, harvest whatever converges.
3. Remaining truncation near-misses + DC3-drift residue (lower EV).

**SESSION TOTAL (waves 3–19): 6932 → 9788 (+2856). Wave-19 +16.**
