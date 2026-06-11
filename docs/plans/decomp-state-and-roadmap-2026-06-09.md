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
- IN FLIGHT: vtable-order walls via auto-rdata-obj slot dump (worktree
  rb3-vtwalls; primary VocalTrackDir::TrackReset @99.989).
- WAVE-3 QUEUE (remaining): pinned-to-a-sliver binary-wide hunt (NEW, high
  EV per the 3-for-3 pattern), AsyncFileHolmes-head foreign-TU split-out,
  obj_orphan 911 cleanup-safe purge (hygiene +0), remaining obj_orphan
  INVESTIGATE cases (UIGuide/LabelNumberTicker, MidiSynth/StreamNull,
  Task/DataNode, UI/PanelDir, VocalTrack/Gem — per worklist dossier),
  OvershellSlot, Mat_NG, fresh research wave.
