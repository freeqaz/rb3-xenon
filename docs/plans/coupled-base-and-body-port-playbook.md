# Coupled-base & Body-port matching — playbook (2026-06-05)

> **CORRECTION 2026-08-17 (task #114) — the `~fns` fan-out column in the work
> order below is INFLATED; the `net` results are NOT. Provenance, bannered not
> rewritten.**
>
> The `~fns` estimates are inherited from `layout_fix_rank.py`, whose `addi`
> branch spelled every base register `rr1`/`rr12`/`rr31` (`'r' + m.group(2)`
> where group 2 already carried its `r`), so no `addi/subi` off the stack
> pointer, the r31 frame alias, or the r12 funclet establisher could ever reach
> the `STACK_REGS` stack bucket. Measured paired over 2128 near-miss functions,
> the as-shipped tool put **100.0% of the 1385 offset rows it parsed into the
> struct bucket and 0 into stack**; corrected the split is 31.2% / 68.8%.
> Direction: inflation of struct-layout evidence. Detail and the re-measurement
> are in `plans/structural-readiness-2026-06-03.md`'s banner; run of record
> `<decomp-bench>/archive/runs/rb3x-layout-fix-rank-rerank-2026-08-17/`.
>
> **The Pass 1 `net` column is unaffected** — those are whole-binary
> `ab_measure.py` build outcomes (4094 → 4136), not tool output, and they stand
> as measured.
>
> Read the two columns together and this page is corroborating evidence for the
> defect rather than a victim of it: the two items whose tool-estimated fan-out
> most overstated their yield — **Flow collections (~9 → 0, RETIRED)** and
> **PanelDir mComponents (~6 → 0, held)** — are exactly the two this doc then
> independently root-caused as a mis-diagnosis and a backwards direction (§"Flow
> collections … was a MIS-DIAGNOSIS", §"PanelDir readiness T2 had the direction
> BACKWARDS"). The items that paid (Char3D, AccomplishmentProgress,
> MicManagerXbox, UIManager) were each pinned from **hand-read header offsets
> and ctor asm**, not from the ranking. Treat the ranking as having contributed
> the two dead ends and none of the five wins.

The structural-readiness audit (`docs/plans/structural-readiness-2026-06-03.md`)
proved the foundational base-class cascade is already landed and the remaining
engine/game layout near-misses fall into exactly two hard classes. This doc is the
reference for *what each class is, why it's hard, and how to attack it* — manual and
automated — plus the agreed work order.

---

## The two task classes

| | **Coupled-base** | **Body-port** |
|---|---|---|
| Root cause | a base / widely-embedded struct's *size or member set* is wrong; fixing it shifts every class that inherits or contains it | a *function body* in our source implements a different algorithm than retail (DC3 is a newer engine version, or RB3 game logic still needs porting); the layout is *downstream* of the algorithm |
| Fix locus | one header edit, **family-wide** blast radius | rewrite C++ bodies to retail's algorithm |
| Why hard | correct in isolation but **regresses siblings** that were already matching at a *compensating* wrong offset | genuine RE: you must read what retail *does* before you know what fields exist |
| Net arbiter | the **build** (net = sum over the whole family) | objdiff per function, iteratively |
| Risk | high (silent sibling regressions) | low/isolated (one subsystem) |

---

## Coupled-base

### Mechanics
MSVC `/O1` lays a derived class out as `[base sub-objects][own members][virtual bases
at tail]`. Change a base size by N and **every** derived class's own members shift by
N and the vbase region moves. Some derived classes carried a *second, compensating*
bug that put their fields at the retail offset *despite* the wrong base size — fixing
the base **un-compensates** them, so they regress while the genuinely-broken ones
improve. You cannot reason your way to "net positive"; you must measure and then chase
the compensating bugs in the regressors until the whole family moves together.

**Proof (this campaign):** removing UIPanel's unused `mPanelId` (−4) gave CreditsPanel
+5 (UIPanel as *secondary* base, genuinely +4) but −13 across *primary*-base panels
that were already correct → net −9. PanelDir (base of ~8 classes) and the Flow
`ObjPtrVec/ObjPtrList/ObjVector` collections (embedded across half the engine) are the
same shape with larger blast radius.

### Recipe (the "land the whole family together" pattern — how String 0xc +94 / Rnd MI +85 succeeded)
1. **Pin the exact retail layout.** Ghidra-decompile the base's ctor/dtor + a few
   derived ctors; read absolute field offsets from the machine code. Cross-check the
   field list vs DC3 + rb3-Wii. objdiff `[off:±N]` gives direction + magnitude. Do not
   guess the delta or the field.
2. **Enumerate the whole family.** `tools/layout_family.py <Base>` lists every derived
   + embedding class and its current match state.
3. **Classify each affected class:** will-improve (near-miss at +Δ) vs will-regress
   (already matching → has a compensating bug). Diagnose each regressor's compensation.
4. **Bundle** the base edit + every compensating edit; A/B the **whole family**, not
   just the base unit.
5. **Converge:** build → per-unit regression diff (`tools/ab_measure.py`) → fix new
   regressors → rebuild → repeat until net-positive, else revert.

### Manual vs automation
- **Automatable:** family enumeration (`layout_family.py`), the build/measure loop
  (`ab_measure.py`), per-unit regression diffing, the net-positive-or-revert gate.
- **The converge loop is an ultracode "loop-until-net-positive":** apply base edit in a
  worktree → build → diff to find regressors → fan one agent per regressor to find+fix
  its compensation → rebuild → repeat. Orchestration fully automated.
- **Irreducibly manual/agent:** pinning the true layout (Ghidra interpretation) and
  diagnosing each compensating bug.
- **Safety:** the build gate makes every attempt free — you cannot make main worse;
  revert on net-negative.

---

## Body-port

### Mechanics
The header is wrong because a **function body** implements a different algorithm than
retail, and the algorithm *determines* the layout (which fields exist, in what order,
what a loader reads). DC3 (our engine source) is often a *newer* version; RB3 game
code lives in rb3-Wii (MWCC, needs Wii→360 porting; DC3 lacks it).

**Examples:** Char3D — DC3 added a `WorldCrowd3DCharHandle*` + a whole handle-object
subsystem; RB3 retail stores a plain `int` instance-index in that slot (see
`Crowd.cpp:754/842` casting `mHandle` to int). The field difference (ptr vs int →
sizeof 0x54 vs 0x50) is downstream of the algorithm; port the subsystem to the
int-index model and the ~20 vector near-misses flip. UIManager — RB3 has a `std::list
mResources` + DTA resource loader (`fn_827E0448`) DC3 lacks.

### Recipe (the repeatable per-function loop)
1. **Pair the oracle:** game → rb3-Wii (`/rb3wii-pair`); engine → DC3 (`/dc3-pair`),
   but treat DC3 as a false friend when newer.
2. **Read retail's behavior:** `/ghidra-decompile` the target + its ctor for control
   flow + field offsets.
3. **Reconstruct layout from the body:** `/struct-info`, `/recon`.
4. **Port the body** to retail's algorithm using the rb3-Wii skeleton, merging
   RB3-vs-Wii + Wii→360 differences.
5. **Match iteratively:** objdiff → `/compare-asm`, `/stack-layout` → `/permute` for
   the codegen last-10%.

### Manual vs automation
- **Automatable scaffolding:** `/rb3wii-pair`/`/dc3-pair` pull oracle source;
  `tools/fingerprint_pipeline.py` scaffolds pinned game TUs; split pins from the oracle.
- **Automatable measurement:** `/recon`, objdiff, `/unicorn-query` (behavioral
  verdict via the native build).
- **Automatable polish:** `/permute` (decomp-synth) closes the last few % once body +
  layout are right. (Gate *commits* by risk class — decomp-synth commits
  plausible-but-wrong variants at non-100%.)
- **Irreducibly manual:** the algorithm reconstruction — e.g. deciding "RB3 uses an int
  index, not a handle object." No tool infers that; read the binary and reason.
- This is the most manual class, but each step is tool-assisted; best run as a per-TU
  pipeline agent in its own worktree.

---

## Agreed work order (by EV = confidence × fan-out ÷ risk)

Body-port first (independent, lower risk, highest-confidence cascades), coupled-base
second (broader payoff but converge-loop + regression risk). Within that:

| # | item | class | ~fns | why here |
|---|------|-------|-----:|----------|
| 1 | **DataFile::ParseArray** | body (.cpp) | small | easiest: split one merged global into two file-statics; bounded |
| 2 | **Char3D mHandle → int index** | body (subsystem) | ~20 | biggest confirmed cascade; clear target (int-index) |
| 3 | **MicManagerXbox +8** | coupled (self-contained) | ~3 | low risk — MicManagerXbox is no class's base |
| 4 | **AccomplishmentProgress / Manager** | body (field recon) | ~17 | medium; Ghidra ctor → missing 4B / 40B fields |
| 5 | **Flow ObjPtr-collection sizes** | coupled (broad) | ~9 | broad cascade if cracked; pin ObjPtrVec/List/Vector sizes first |
| 6 | **PanelDir mComponents −8** | coupled (base of ~8) | ~6 | converge-loop; root (mFlows 0 bytes) unresolved |
| 7 | **UIManager body port** | body (full UI.cpp) | ~5 | most effort/fn; do last |

Each attempt: A/B in a dedicated `scripts/setup_worktree.sh` worktree, measure with
`tools/ab_measure.py`, land on main one-at-a-time only if net-positive. Coupled-base
items measure the **whole family**; body-port items measure the target unit + permute.

---

## Pass 1 results (2026-06-05) — structural-grind-pass workflow

7 parallel agents over the 7 targets. **Landed +42 (4094 -> 4136), zero regressions,
5 commits** (`4026e04`, `1a7815c`, `fa49543`, `00e2355`, `186c193`):

| item | result | net | how |
|------|--------|----:|-----|
| Char3D mHandle->int | **LANDED** | **+28** | dropped dc3's handle-object; RB3 stores int index; Char3D 0x54->0x50, vector cascade flipped (Crowd 89->117) |
| AccomplishmentProgress | **LANDED** | **+8** | added the missing 4-byte int@0x50 before mGamerAwardStatusList |
| DataFile::ParseArray | **LANDED** | **+2** | merged gArray/gNode into one static struct (shared base reg; bare `static` reorders BSS) |
| MicManagerXbox | **LANDED** | **+2** | vector@0x28, new int@0x20 + mMicsChanged@0x24 moved before it (pinned from ctor asm) |
| UIManager InitResources | **LANDED** | **+2** | ported RB3's std::list mResources + DTA loader; InitResources 0->100% |
| Flow collections | RETIRED | 0 | **premise was a mis-diagnosis** (see below) |
| PanelDir mFlows | held | 0 | correct + regression-free but flips nothing alone; bundle with UIComponent-vtable fix |

### Three NEW root-caused coupled-base levers (the real next targets)
1. **StlNodeAlloc EBO size — std::map/set node 0x18 (ours) vs 0x1c (retail). HIGHEST EV.**
   `src/system/stlport/stl/_alloc.h` `_STLP_alloc_proxy` uses EBO; our `StlNodeAlloc`
   is EMPTY so EBO folds it to 0 -> proxy 0x10 -> `_Rb_tree` 0x18. Retail's StlNodeAlloc
   carries 4 bytes of state (pool/heap ptr) -> proxy 0x14 -> tree 0x1c. **Every
   node-based container in the binary is +4.** This is the root of AccomplishmentManager's
   "40-byte gap" (= 10 maps x 4B) and ~12 of its fns stuck at 99.9%, and likely MANY
   multi-map game classes. FIX: give StlNodeAlloc a 4-byte member (non-empty). MUST be
   A/B'd WHOLE-BINARY (regresses classes where the map is the last/only member — start
   offset already matches — while fixing every multi-map class). Textbook converge-loop;
   dedicate a session.
2. **UIComponent vtable +1 slot (+4).** UIComponent's vtable has one extra 4-byte slot
   before Entering()/Exiting(): retail dispatches `lwz r11,0x3c(r11)` vs ours `0x40`.
   Blocks the whole PanelDir family. Bundle with the (verified-correct) PanelDir mFlows
   removal (`~/tmp/grind/paneldir.patch`) -> Entering/Exiting/Exit flip (+3). UIComponent
   blast radius -> coupled-base, A/B the family.
3. **RndCam.h field layout (+176 skew).** UIManager::Init writes cam fields at
   target 0x114/0x118/0x11c vs ours 0x64/0x68/0x6c (+0xB0). An RndCam.h layout fix with
   broad blast radius; gates UIManager::Init (87.6%) + other RndCam users.

### Retired / corrected
- **Flow collections (readiness T5) was a MIS-DIAGNOSIS.** ObjPtrVec=0x1c, ObjPtrList=0x14,
  ObjVector=0x10 are all CORRECT (match DC3 + retail member spans). The "FlowNode 0x64 vs
  0x38" figures were the funclet/ICF false-positives readiness s1 warns about. The genuine
  FlowNode delta is only +4, from String 0xc (ours, with mCap) vs DC3 String 0x8
  (capacity-in-buffer). **OPEN QUESTION:** our String=0xc is a deliberately LANDED RB3 fix
  (+94); if RB3 retail genuinely uses 0xc (mCap member) while DC3 uses 0x8, then our FlowNode
  0x64 is RB3-CORRECT and Flow is already done modulo funclet noise. Re-pin against RB3
  Ghidra (String ctor fn_82798E18 / resize fn_82798E68), NOT DC3, before any action.
- **PanelDir readiness T2 had the direction BACKWARDS:** verified from `build/45410914/asm/
  PanelDir.s` — mTriggers@0x1f0, mComponents@0x1f8 (gap 0x8 = one healthy std::list); RB3
  has NO mFlows. Ours has the spurious mFlows (DC3 false-friend) -> REMOVE it (not add).

### Reusable levers discovered
- **When Ghidra (8002) is saturated by concurrent agents, read the retail algorithm
  DIRECTLY from `build/45410914/asm/<UNIT>.s` (.fn/.endfn blocks)** — same ground truth,
  never times out. This unblocked PanelDir/Mic/Char3D pinning when Ghidra was down all run.
- decomp_synth `temp_elimination` + `statement_reorder` are high-value for static-init/guard
  ordering (found the UIManager InitResources guard-less-Symbol win). Run `--no-apply` first,
  then re-run WITHOUT it to actually write the winning variant.
- dc3 is a FALSE FRIEND for several of these (Char3D handle-object, Mic 0x20 bug, PanelDir
  mFlows, String 0x8); rb3-Wii + the retail asm are the correct oracles for RB3-specific shapes.

## Refs
`docs/plans/structural-readiness-2026-06-03.md` (verified target evidence + DROP list),
`docs/plans/engine-baseclass-layout-bugs.md`, `docs/plans/objptr-family-relayout-migration.md`,
`docs/plans/ui-base-layout-reconstruction.md`; tools `layout_fix_rank.py`,
`ab_measure.py`, `layout_family.py`; memories `project-engine-baseclass-layout-wall`,
`project-game-port-workflow`, `project-permuter-correctness-model`.
