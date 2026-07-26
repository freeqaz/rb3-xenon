# Scatter-include inlining collapse — scanner, measured pool size, and a control group

**Lane P, 2026-07-26.** Worktree `~/tmp/wt-laneP-scatter`, branch `laneP-scatter`,
base `e7662cdb` (28,238 strict).

Follow-up to `docs/plans/funclet-cascade-lever-2026-07-25.md` §15, which named this
shape, fixed the canonical instance, and estimated the pool at **"234 sites"** —
flagging *"highest-yield follow-up from this lane — build the scanner."*

The scanner now exists. **The 234 figure was a count of scatter-includes, not of
defects.** Measured against a control group, the real pool is ~11 candidate
parents binary-wide, of which ~6 are attributable to the mechanism.

---

## 1. The mechanism (recap)

`H.cpp` contains `#include "some/Guest.cpp"` because retail's linker scattered
Guest's COMDATs into H's pinned `.text` range. Side effect: the two files become
one translation unit, so MSVC can inline across them. Retail could not — it built
them as separate TUs. Two things follow:

1. `/Ob2` **inlines** the callee (the `bl` disappears).
2. The now-local callee is provably nothrow, so MSVC **deletes the caller's EH
   cleanup funclets outright**, along with the EH-state spills that make up the
   frame delta.

Retail therefore *has* funclets we never emit. `__declspec(noinline)` stops (1)
but not (2). The one fix proven so far is relocating the callee's body to a third
TU (`src/system/obj/DataUtl.cpp`, hosting `DataNode`'s copy ctor out of
`DataNode.cpp` — `DataArray::Insert` 31.7 → 100).

## 2. The scanner — `scripts/harvest/scatter_inline_collapse_scan.py`

```
venv/bin/python scripts/harvest/scatter_inline_collapse_scan.py \
    --repo . --all-owners --json ~/tmp/laneP/collapse.json --top 20
```

Needs a built tree (`build/45410914/report.json` + `build/45410914/src`) and
`orig/45410914/band.exe`. Runs in ~1 min; imports its PE/EH/COFF primitives from
`scripts/harvest/funclet_cascade_rank.py` rather than duplicating them.

### 2.1 The load-bearing discovery: base-side funclets are exactly attributable

The 2026-07-25 tooling could only see the *target* side of the funclet ledger.
The base side turns out to be exact too, straight out of our own COFF:

> MSVC emits, per EH function, a COMDAT data symbol **`__unwindtable$<mangled>`**
> whose section relocations name that function's **`__unwind$N`** / **`__catch$N`**
> funclets.

So `base_funclets(sym)` = count of `__unwind$`/`__catch$` relocations out of
`__unwindtable$<sym>`'s section — no disassembly, no objdiff, no naming heuristic.
Verified against `DataArray.cpp`'s post-fix state: `Insert` → exactly the 3
`__unwind$158723/4/5` it should have, `Resize` → 1, `BandSongMetadata::Handle` → 1.

**A missing `__unwindtable$` symbol means MSVC deleted the EH data entirely** —
which is precisely what the collapse does, so absence is scored as 0, not as
"unknown".

### 2.2 What it joins

| side | source | how |
|---|---|---|
| target funclets/parent | `orig/45410914/band.exe` `.pdata` + `_s_FuncInfo` | `funclet_cascade_rank.parse_eh`, screened to r12-frame funclets |
| base funclets/parent | `build/45410914/src/**/*.obj` | `__unwindtable$` relocations (§2.1) |
| target `bl` count | `band.exe` `.text` | decode, opcode 18 with LK |
| base `bl` count | our COFF section bytes | same |
| VA → name | `scripts/target_symbol_map.json` | |
| VA → unit | `config/45410914/splits.txt` | basename-keyed; disambiguated by which host obj actually defines the symbol |
| unit → source | `report.json` `metadata.source_path` | exact |
| guest ownership | the guest's OWN compiled obj symbol set (170/181 guests are separately wired), else class-scope regex on the guest source | |

### 2.3 The SHARP fingerprint

§15's mechanism predicts **both halves at once**. The scanner therefore
distinguishes:

* **broad** — `base_funclets < target_funclets`. Noisy: any body divergence that
  changes EH state count lands here.
* **SHARP** — `base_funclets == 0` **and** target has **more** `bl` than we emit.
  Both halves of the §15 prediction: every funclet deleted *and* a call inlined
  away. This is the real fingerprint.

A parent with *fewer* funclets while **we** emit *more* calls is ordinary body
divergence pointing the other way — `??0CharIKHand@@IAA@XZ` (81.3%, 9 target
funclets, we emit 6, but `blΔ = −9`) and `??0RndMultiMesh@@IAA@XZ` (92.5%,
`blΔ = −3`) both look like collapse candidates under the broad filter and are
**not**. Do not work them as this lever.

## 3. Coverage census — what the scan can and cannot see

```
eh_parents_total                   6753     # r12-frame funclet parents, whole binary
in_scatter_host_span               2055     # inside a scatter-include host's pinned .text
in_scatter_host_span_funclets      5182
  named (target_symbol_map)        1182
  unnamed                           873     # 2,241 funclets behind them — INVISIBLE to naming
  guest_attributed                  164
  host_owned                       1018
scatter-include edges               239     # 239 resolved, 155 host files
guest symbol sets                   170 exact (own obj) / 11 scope-fallback
```

The 873 unnamed parents are a genuine blind spot for *this* scan. Note that per
the lane-L correction at the head of the 2026-07-25 doc, **naming a parent does
not flip its funclets** — naming only buys observability, which is exactly what
would be needed to extend this scan. That is the one open lead here (§6).

## 4. ★ The measurement, with a control group

The honest question is not "how many scatter-host functions are missing funclets"
but "**do scatter-host functions miss funclets more than everything else**". So the
same computation is run over every pinned unit, split by whether its source
contains a scatter-include, using an identical method on both groups (named EH
parents that our obj actually defines):

| group | parents | target fl | base fl | broad short | rate | **SHARP** | **rate** |
|---|--:|--:|--:|--:|--:|--:|--:|
| scatter-host | 1,056 | 2,709 | 3,317 | 25 | 2.37% | **11** | **1.04%** |
| control | 1,943 | 4,908 | 6,856 | 38 | 1.96% | **9** | **0.46%** |

* **Broad rate: 2.37% vs 1.96%, Fisher two-sided p = 0.51 — no effect.**
  Scatter-includes do *not* cause a general funclet deficit. Anyone ranking work
  off the broad filter is ranking noise.
* **SHARP rate: 1.04% vs 0.46% — 2.25× enrichment, Fisher two-sided p = 0.097.**
  Real-looking, directionally consistent with §15, but **not significant at 0.05**
  on n = 11 vs 9.
* **Excess attributable to the mechanism = 11 − (0.46% × 1,056) ≈ 6 parents.**

### The pool, stated honestly

> **~11 candidate parents binary-wide** (the SHARP list, §5), of which **~6 are
> attributable** to the scatter-include; each blocks itself plus 1–3 funclets, so
> the ceiling is roughly **15–30 functions**.

Not 234 sites, and not a block of functions per site. Only **4 of 239 edges**
carry a guest-attributed instance at all. The mechanism §15 describes is real —
it was demonstrated on `DataArray` — but it is **idiosyncratic, not systematic**.
`DataArray`/`DataNode` was an unusually rich instance (4 functions + 9 funclets
from one edge) and is not representative.

## 5. The SHARP worklist (complete, whole-binary)

| symbol | match% | tgt fl | blΔ | host unit | guest |
|---|--:|--:|--:|---|---|
| `?ConfigPanels@VocalTrackDir@@QAAXXZ` | 0.0 | 3 | +20 | `bandobj/VocalTrackDir.cpp` | host-owned |
| `?insert_unique_noresize@?$hashtable@U?$pair@$$CBHH@…` | 24.8 | 3 | +15 | `meta_band/AccomplishmentProgress.cpp` | host-owned |
| `?allocate@?$StlNodeAlloc@PAVMoveDetector@@…` | 5.0 | 1 | +6 | `hamobj/MoveAsyncDetector.cpp` | host-owned |
| `?_M_fill_insert_aux@?$vector@HV?$StlNodeAlloc@H@…` | 48.0 | 1 | +6 | `meta_band/MusicLibrary.cpp` | host-owned |
| `?OnMsg@MusicLibrary@@QAA?AVDataNode@@ABVPrimaryProfileChangedMsg@@@Z` | 32.8 | 1 | +2 | `meta_band/MusicLibrary.cpp` | host-owned |
| `??$__uninitialized_fill_n@PAUEnvironmentEntry@LightPreset@@…` | 15.8 | 1 | +2 | `world/LightPreset.cpp` | host-owned |
| `??$__uninitialized_copy@PAVDataArrayPtr@@PAV1@@…` | 33.9 | 1 | +2 | `synth/MetaMusic.cpp` | `rndobj/PropAnim.cpp` |
| `??$_Copy_Construct@VDataArrayPtr@@…` | 14.6 | 1 | +1 | `synth/MetaMusic.cpp` | `rndobj/PropAnim.cpp` |
| `??4?$vector@UEnvLightEntry@LightPreset@@…` (operator=) | 26.5 | 1 | +1 | `rndobj/Utl.cpp` | `ui/UIListDir.cpp` |
| `?PreLoad@VocalTrackDir@@UAAXAAVBinStream@@@Z` | 60.5 | 1 | +1 | `bandobj/VocalTrackDir.cpp` | host-owned |
| `??$__destroy_mv_srcs@V?$reverse_iterator@PAV?$ObjVector@…` | 56.3 | 1 | +1 | `char/Character.cpp` | host-owned |

Note the **direction**: 8 of 11 are *host-owned*, i.e. a **guest** function is
being inlined **into a host** function — the mirror of the `DataArray` case §15
describes. Any fix must handle both directions.

Two `DataArrayPtr` entries and two `LightPreset` entry-type entries suggest two
shared root causes rather than four independent ones.

**Caveat before funding any of these**: STL template-instantiation swarm members
have a measured ~0/6 historical flip rate and are a common home for symbol-map
mispairs (tell: target element-size immediates or argument count incompatible
with the mapped type). 7 of the 11 are STL templates. Diagnose before investing.

## 5b. ★★ The precision filter — and why the SHARP list is a mirage

The SHARP fingerprint is **necessary but not sufficient**. It fires identically on
ordinary **header-inline-policy divergence**: a class-template member defined
in-class in a header is inlined in *every* TU, scatter-include or not, and takes
the caller's funclets with it exactly the same way. That is a different, already
known lever family — nothing to do with this one.

The scanner now discriminates by asking **where the inlined-away callee is
defined** (`§4b precision filter`). For each SHARP parent it diffs the target
obj's call relocations against ours to get the target-only callees, then:

* callee **not defined in our host TU at all** ⇒ we could not possibly have
  inlined it; the missing `bl` is **body divergence**, not inlining policy.
* callee defined in objs **outside** the host and its guests ⇒ **header
  inline/template COMDAT**, emitted everywhere it is used ⇒ not scatter-related.
* callee defined **only** in the host obj and/or its guests' objs ⇒ genuinely
  entered the TU via the scatter-include ⇒ **scatter-attributable**.

Verdict on all 11 SHARP parents:

| class | n | example |
|---|--:|---|
| HEADER-INLINE (not scatter) | 4 | `ConfigPanels` ← `??0FilePath@@QAA@PBD@Z`, defined in **76** objs |
| NOT-IN-TU (body divergence) | 3 | `MusicLibrary::OnMsg` ← `SessionMgr::GetLeaderUser`, defined only in `SessionMgr.obj`, which `MusicLibrary.cpp` does not include |
| SCATTER-ATTRIBUTABLE | 2 | `__uninitialized_copy<DataArrayPtr*>` ← `_Copy_Construct<DataArrayPtr>` |
| UNKNOWN (callees unresolved) | 2 | |

And **both** "scatter-attributable" rows are themselves doubtful:

* `?_M_fill_insert_aux@?$vector@H…` ← `MusicLibrary::GetCurrentSortName`. A
  `vector<int>` fill-insert does not call `GetCurrentSortName`; this is the
  classic **symbol-map mispair** tell, not a lever instance.
* `__uninitialized_copy<DataArrayPtr*>` ← `_Copy_Construct<DataArrayPtr>`. Both
  are header templates; the test passes only because just two TUs (the host and
  its guest) instantiate them, so "not defined outside host+guests" is
  **inconclusive** here rather than positive evidence.

> **Net: zero confirmed scatter-include collapse instances remain in the binary
> beyond the already-fixed `DataArray`/`DataNode` case. The lever is drained.**

This also explains the weak §4 enrichment: most SHARP hits are header-inline
noise, which is equally common in control units, so the 2.25× ratio is measuring
mostly the same population on both sides.

## 5c. ★★ The construct question — ANSWERED

Run as a controlled experiment: revert the `DataUtl.cpp` hosting workaround to
reproduce the defect (`DataArray::Insert` 100.00 → 27.99), then test constructs
against a known-correct answer.

| construct | `Insert` % | `bl` back? | funclets back? |
|---|--:|---|---|
| pristine (ctor hosted in `DataUtl.cpp`) | **100.00** | yes | yes |
| control (ctor back in `DataNode.cpp`) | 27.99 | no | no |
| `#include` moved to **top** of the TU | 27.99 | no | no |
| `throw(...)` on the definition | 27.99 | no | no |
| `throw(...)` on the declaration in `Data.h` | 27.99 | no | no |
| `#pragma auto_inline(off)` around the include | 27.99 | no | no |
| `__declspec(noinline)` on the ctor | 61.70 | **yes** | no |
| `#pragma auto_inline(off)` around the body | 61.70 | yes | no |
| `#pragma inline_depth(0)` around the body | 61.92 | yes | no |
| opaque `extern` throwing edge in the ctor | **100.00** | yes | **yes** |
| **`#pragma optimize("g", off)` around the body** | **100.00** | yes | **yes** |

**Winner: `#pragma optimize("g", off)` / `#pragma optimize("", on)` around the
callee's definition.** It defeats the nothrow deduction *in place* — no function
relocation needed — and also restores `InsertNodes`/`Resize`/`Remove` to 100.00
and the TU's funclet count 118 → 131. Whole-binary: **28,238, 0 gained 0 lost** —
exactly equivalent to the `DataUtl` hosting workaround, i.e. a drop-in
replacement for the non-generalisable fix.

Findings that matter more than the winner:

* **The inliner and the nothrow analysis are independent.** Everything that only
  suppresses inlining lands on the same **61.7 plateau** — the `bl` returns, the
  funclets stay deleted. This confirms and sharpens §15's `__declspec(noinline)`
  note: *suppressing inlining is not merely insufficient, it is the wrong axis.*
* **Include-at-top is a clean NEGATIVE** — output byte-identical to the control.
  MSVC defers codegen to end-of-TU, so a body defined *after* its call site
  inlines identically. This kills the cheapest hypothesis; do not retry it.
* **`throw(...)` is inert** on both declaration and definition. MSVC's /EHsc
  nothrow deduction is a **body analysis**, not spec-driven.
* The opaque-throwing-edge candidate also reaching 100.00 **proves the gate is
  "can this callee throw", not "is it inlined"**.
* **Applicability caveat:** `optimize("g", off)` changes the guarded function's
  *own* codegen. It is free only when the callee has no `target_symbol_map.json`
  entry of its own. For a callee that *is* a tracked target, third-TU hosting
  remains the answer.

Left uncommitted on branch `laneP-nothrow` (`d328d0ac`) — it is whole-binary
neutral and, given §5b, has **no remaining application**, so landing it would be
pure churn. It is recorded here for the construct knowledge, not the diff.

## 6. What remains

* **Nothing, on this lever.** The construct question is answered (§5c) and the
  pool is empty (§5b). **De-fund it.** `#pragma optimize("g", off)` is now a
  known, cheap tool if the shape ever reappears — that is the durable output.
* **The header-inline population is the real vein next door.** 4 of the 11 SHARP
  parents (and the 9 SHARP hits in the *control* group, which by construction
  have no scatter-include) are ordinary DC3-inlined-vs-retail-out-of-line
  divergence. `??0FilePath@@QAA@PBD@Z` (76 objs), `ObjRefConcrete<…>::~` (93 objs)
  and `ObjRefConcrete<…>::SetObjConcrete` (70 objs) are inlined by us everywhere
  and called out-of-line by retail — a **force-multiplier shape** with far more
  instances than this lane's. The scanner's `§4b` classifier already separates
  them out; feeding the HEADER-INLINE bucket to the inline-policy lever is the
  natural follow-on.
* **873 unnamed parents / 2,241 funclets in scatter-host spans** are invisible to
  the scan because SHARP needs a name to look our symbol up. Extending attribution
  by reloc-masked byte identity (the `homing_scan.py` primitive) rather than by
  name would close this — the only genuinely unexplored extension.
* **Do not re-rank off the broad filter** (§4) — it is statistically flat, and
  `CharIKHand` / `RndMultiMesh` are its false positives (§2.3).
