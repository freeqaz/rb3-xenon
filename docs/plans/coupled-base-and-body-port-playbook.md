# Coupled-base & Body-port matching — playbook (2026-06-05)

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

## Refs
`docs/plans/structural-readiness-2026-06-03.md` (verified target evidence + DROP list),
`docs/plans/engine-baseclass-layout-bugs.md`, `docs/plans/objptr-family-relayout-migration.md`,
`docs/plans/ui-base-layout-reconstruction.md`; tools `layout_fix_rank.py`,
`ab_measure.py`, `layout_family.py`; memories `project-engine-baseclass-layout-wall`,
`project-game-port-workflow`, `project-permuter-correctness-model`.
