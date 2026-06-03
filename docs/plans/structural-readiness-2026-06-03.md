# Structural Readiness — base-class/struct layout wall (2026-06-03)

**Purpose:** get the engine/game layout *ready* for the per-function matching grind.
This is the output of a verified ultracode campaign: an empirical fan-out map
(`tools/layout_fix_rank.py`) + 6 Opus mapping agents + 6 Sonnet adversarial
verifiers + empirical build-tests. It separates the **real** struct-layout bugs
from measurement noise, ranks them, and says exactly what each one needs.

**Bottom line:** the foundational base-class cascade was *already landed* in prior
sessions (String 0xc, ObjPtr-family 0xc, Rnd MI, ObjectDir +4, UIPanel
mFinalDrawPassFlag, Hmx::Object). What remains is **not** a clean header cascade —
every remaining real bug is **coupled-base** (regresses sibling classes) or
**body-port** class. There are **no clean bounded header-only wins left.** The grind
session should expect per-class/per-fn work, guided by the verified targets below.

Baseline at authoring: **4094** matched (`634297c`).

---

## 1. The big methodological correction (READ THIS FIRST)

The raw fan-out (97% of 1064 near-misses look "offset-class") was **dominated by a
systematic false-positive**. `layout_fix_rank.py` v1 parsed MSVC EH-cleanup-funclet
frame reconstruction — `subi r31, r12, FRAMESIZE` — as a struct field read (base
register `r12` was not in the stack-reg set). When a funclet's parent frame size
differs between target and our build, the immediate delta clusters *coherently by
parent function*, masquerading as a shared struct-field offset.

**Every large cluster in the v1 map was this noise:** BandDirector +32 (24 fns),
Part −64 (19), Rnd −80 (13), MidiParser/WaveFile −48 (23), DataFile −16 (18),
DataFunc, StorePanel, SongDB, RhythmBattle, EventTrigger, NetSync, CharBone*,
HamCamTransform, Shockwave (+724 = ICF mispair), CacheXbox (+2048 = mispair). All
**CONFIRMED not layout bugs** by the verifiers. **Do not chase these.**

The tool is now fixed (`STACK_REGS` includes `r12`; an `addi/subi` writing `r1/r31`
is treated as frame-establish, not a field read). Re-running gives the **true**
struct-delta surface — see `~/tmp/layout_fix_rank_v2.json`. It is ~150 fns, not
~700, and it agrees with the verifiers.

---

## 2. The true struct-delta clusters (corrected tool)

| delta | fns | units | what it is |
|------:|----:|-------|------------|
| −4 | 30 | Crowd(20), MoviePanel(6), VocalPlayer(2), CreditsPanel(2) | Char3D mHandle (Crowd) + UIPanel-base panel family |
| −96/−44/−32/+40 | ~12 | FlowIf, FlowNode, FlowQueueable, FlowSound | ObjPtr-collection sizes too large (coupled) |
| −8 | 6 | PanelDir(4), Overlay(2) | a missing 8 bytes between PanelDir mTriggers and mComponents |
| +4 | 4 | CharHair | shared ObjPtr/ObjRef internal-offset (uncertain) |
| +8 | 3 | Mic (MicManagerXbox) | ChatBuffer vector +8 (self-contained) |
| +36/+52/+60/+12 | ~10 | Dancer, CharNeckTwist, HamCharacter, CharSleeve | virtual-base subobject sizes (engine MI tails) |

---

## 3. Verified real targets — ranked, with what each needs

### T1. WorldCrowd::CharData::Char3D — `mHandle` (BODY PORT, ~20 fns) ⭐ biggest real lever
- **Confirmed:** retail `Char3D` sizeof **0x50** (Transform@0x0, int mIdx@0x40,
  mColors@0x44, **no mHandle**); ours is **0x54** (mHandle@0x50). The −4 cascades
  across ~20 `std::vector<Char3D>` template fns (wrong element stride 0x54 vs 0x50).
- **Why not bounded:** `mHandle` is *genuinely used* in `src/system/world/Crowd.cpp`
  (10 sites: creates `WorldCrowd3DCharHandle` objects, RELEASE, comparisons) — and
  crucially **lines 754/842 cast `mHandle` to `int`**. This is a dc3-vs-RB3
  divergence: dc3 added a handle-*object* system; **RB3 retail stores an int
  instance-index in that slot**. Matching requires porting the Crowd 3D-char
  subsystem to RB3's int-index model (drop `WorldCrowd3DCharHandle`, use int).
- **Grind recipe:** port `WorldCrowd` 3D-char management from rb3-Wii
  (`../rb3/src/.../Crowd.cpp`) — replace `mHandle` (ptr) with the int index; remove
  `WorldCrowd3DCharHandle`; then Char3D shrinks 0x54→0x50 and the 20 vector fns flip.

### T2. PanelDir mComponents −8 (COUPLED base, ~6 fns)
- **Confirmed:** `mComponents` at **0x1f8 (ours) vs 0x200 (retail)** [off:+8] across
  Entering/Exiting/RemovingObject. `mTriggers@0x1f0` **matches both builds** → the
  RndDir base is correct; the 8-byte deficit is **PanelDir-local**, between
  mTriggers and mComponents (where `mFlows std::list<Flow*>` lives). Effectively our
  `mTriggers`+`mFlows` occupy 0x8 where retail uses 0x10.
- **Why not (yet) bounded:** the header annotations (0x218/0x220/0x228) are **stale**
  vs the compiled reality, and PanelDir is the base of ~8 classes (WorldDir, TrackDir,
  SkeletonDir, TrackPanelDirBase, …). A blind +8 risks regressing siblings exactly
  like the UIPanel test (§4). **Root unresolved:** why does `std::list<Flow*> mFlows`
  contribute 0 bytes? (STLport `std::list` sentinel size, or a list collapse.)
- **Grind recipe:** Ghidra-decompile a PanelDir ctor (e.g. `PanelDir::PanelDir`) to
  read the absolute mTriggers/mFlows/mComponents offsets; determine the real
  STLport `std::list` size in this build; if the 8 bytes is a genuinely-missing
  member, add it and A/B (check ALL PanelDir-derived units, not just PanelDir).

### T3. AccomplishmentProgress / AccomplishmentManager (BODY PORT, ~17 fns)
- **AccomplishmentProgress (confirmed):** a 4-byte field is missing before
  `mGamerAwardStatusList` — retail @0x54, ours @0x50; everything after shifts +4.
- **AccomplishmentManager (confirmed):** 40-byte gap — `mGoalAcquisitionInfos`
  retail @0x170 vs ours @0x148, `mGoalProgressionInfos` @0x17c vs @0x154.
- **Why not bounded:** the missing fields are unidentified (need Ghidra ctor RE);
  rb3-Wii game oracle (`../rb3/src/band3/meta_band/Accomplishment*`) shows a
  different field set (MWCC). Body/field reconstruction job.
- **Grind recipe:** Ghidra `AccomplishmentManager`/`Progress` ctor → enumerate the
  member stores → reconcile with rb3-Wii field list → add the missing members.

### T4. UIManager virtual-base body port (BODY PORT, ~5 fns) — already specced
- Header virtual-base migration is applied; the **body** diverges (RB3 has
  `std::list mResources@0x34` + a DTA resource loader `fn_827E0448` that dc3 lacks).
- **Grind recipe:** full UI.cpp port — see
  `docs/plans/ui-base-layout-reconstruction.md` + the UIManager section of the
  `project-engine-baseclass-layout-wall` memory. Merge binary + rb3-Wii
  `../rb3/src/system/ui/UI.cpp` (dc3 is a FALSE FRIEND here).

### T5. Flow ObjPtr-collection sizes (COUPLED refactor, ~9 fns)
- **Confirmed too-big:** FlowNode non-vbase 0x38 retail vs 0x64 ours (+0x2c);
  FlowQueueable 0x50 vs 0x70 (+0x20); FlowIf 0x30 vs 0x90 (+0x60); FlowSound 0xcc
  vs 0xa4 (−0x28, too small — FlowPtr<Sound> sizing?).
- **Root:** our `ObjPtrVec`/`ObjPtrList`/`ObjVector` container sizes differ from
  retail. **High coupling** (these are base of much of the engine) → a refactor, not
  a header tweak. Defer to a dedicated session; pin the retail container sizes with
  Ghidra first.

### T6. Smaller / lower-confidence
- **MicManagerXbox** (BOUNDED, self-contained, ~3 fns): ChatBuffer vector retail
  @0x28 vs ours @0x20 → need +8 (2 fields) before it. Exact fields unknown; safe to
  experiment since MicManagerXbox is no class's base.
- **CharHair +4** (~4 fns): attributed to shared ObjPtr/ObjRef internal offset —
  uncertain (obj-core refuted the +16 ObjPtr cluster as funclet noise; re-verify).
- **DataFile::ParseArray** (.cpp body): declare `gArray`/`gNode` as two separate
  file-statics instead of one merged global. Small body change.

---

## 4. Empirical results (what was actually tried)

- **UIPanel `mPanelId` removal — TESTED, net −9, REVERTED.** The verifier rated this
  high-confidence BOUNDED (remove the unused trailing `int mPanelId@0x38`). Build
  A/B: **CreditsPanel +5** (UIPanel as *secondary* base) but **−13 across
  primary-base panels** (CalibrationPanel −3, DeJitterPanel −4, MoviePanel −1,
  QuestFilterPanel −2, GamePanel −2, TrainerPanel −1). The "+4 is uniform" premise is
  wrong: primary-base panels were already correct, so removal broke them. **Lesson:
  even a high-confidence CONFIRMED layout claim can net-negative from coupling the
  static analysis missed (primary vs secondary sub-object). The build is the only
  arbiter. Any base-class layout edit MUST be A/B'd across ALL derived units.**

---

## 5. DROP list (CONFIRMED not-a-bug — do NOT spend time here)

Already-correct or funclet/mispair noise: Hmx::Object, ObjectDir, ObjPtr/ObjRef
(+16 cluster), BandDirector (+32), MidiParser/WaveFile (−48), Part (−64), Rnd (−80),
DataFile/StreakMeter (−16), DataFunc, StorePanel, SongDB, RhythmBattle, EventTrigger,
NetSync, HamCamTransform, CharBone/CharBoneOffset/CharSleeve (stack/regalloc),
RndShockwave (+724 ICF mispair), CacheXbox (+2048 mispair), UITransitionHandler
(funclet mispair), SampleZone (stale annotations only).

---

## 6. Tooling

- `tools/layout_fix_rank.py` — fan-out aggregator. **v2 fixes the funclet FP.**
  `python3 tools/layout_fix_rank.py --lo 80 --hi 100 --out <json>`. Output: per-fn
  struct/stack deltas, per-unit rollup, global delta clusters (keystone candidates).
  Caveat: still cannot distinguish a struct read on r31 when `this` is parked in r31
  (rare under /O1); treat r31-based deltas as suspect.
- `tools/classify_nearmiss.py` — per-fn mismatch-cause classifier (OFFSET/REG/...).
- Verified artifacts: `~/tmp/readiness/{domain}.json` (maps) +
  `{domain}.verdict.json` (adversarial verdicts).

## 7. Refs
`docs/plans/engine-baseclass-layout-bugs.md`,
`docs/plans/objptr-family-relayout-migration.md`,
`docs/plans/ui-base-layout-reconstruction.md`; memories
`project-engine-baseclass-layout-wall`, `project-objptr-relayout-migration`,
`project-engine-split-relocation`.
