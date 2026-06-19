# Wave-8 Character.cpp pin-relocation audit (adversarial honesty gate)

**Date:** 2026-06-19
**Auditor:** honesty-gate subagent (read-only on main; A/B in candidate's worktree)
**Branch under review:** `38a773a` ("Character: relocate .text pin from dead sliver to real cluster (+45 @100%)")
**Worktree:** `/home/free/code/milohax/wt-w8-character-relocate-pin`
**Change:** splits.txt-only (2 lines). Relocate `Character.cpp` `.text` pin from the
dead 0x48-byte sliver `[0x822911F0,0x82291238)` (mf=0) to the cluster
`[0x8235B1D0,0x8235F180)` (0x3FB0 bytes) + dtk-derived `.pdata [0x821F9790,0x821F9B08)`
(888 bytes / 111 entries).

## VERDICT: **HONEST — LAND +45.** All 45 newly-matched fns are own-TU Character.obj bodies. Zero foreign. Longest contiguous foreign run = 0.

The risk this audit was charged with (the inverse Waypoint case: a +N where the new
fns are FOREIGN COMDAT folds pinned by VA) is **refuted by ground truth**: every
matched function is defined in OUR compiled `Character.obj`, which a TU only emits for
its own functions + its own template instantiations.

---

## 1. Clean A/B (the decisive measurement)

Built the candidate's full whole-binary report in its worktree on identical machinery,
re-run once to clear the splits-only freshness FP (stable):

| State | splits Character `.text` | total `matched_functions` |
|---|---|---|
| Baseline (parent `da8258f` = main HEAD, dead sliver) | `[0x822911F0,0x82291238)` | **8234** |
| Candidate (`38a773a`, cluster) | `[0x8235B1D0,0x8235F180)` | **8279** (stable on re-run) |

**NET DELTA = +45.** Per-unit diff (candidate report vs main report.json): **ONLY
`default/Character 0 -> 45`. ZERO regressions, zero cross-unit movement.** Sum of all
per-unit deltas = +45 = headline net. The old sliver was a dead pin (mf=0), so unit +45
== binary +45.

## 2. Ownership ground truth — every matched fn is emitted by OUR compiled Character.obj

Parsed `build/45410914/obj/Character.obj` COFF symbol table (our compiled product). It
defines **120 function symbols** (30 named: 9 `@Character@@` methods, 10
`CharPollableSorter`, 2 `Lod@Character` BinStream serializers, + the foreign-named
template singletons below).

Cross-referenced the report's per-function list: **all 45 matched fns AND all 75
unmatched fns are defined in our Character.obj.** `truly foreign (not in our obj) = 0`.

A TU's compiled `.obj` contains only its own functions plus its own COMDAT template
instantiations. The retail linker ICF-folds identical COMDATs across TUs and one copy
"wins" the VA, but our obj independently emitting the byte-identical body proves the
function is genuinely Character's own instantiation. **There is no foreign body matched
by VA attribution.**

### The 45 matched, classified
- **31 anonymous `fn_<va>`** — own-TU bodies (STL/funclet/helper), all defined in our obj.
- **12 named Character-own** — `ForceBlink`/`EnableBlinks`/`SetFocusInterest`/
  `SetInterestFilterFlags`/`Teleport`/`AddedObject`/`SetInterestObjects` (Character methods),
  `ChangedByRecurse@CharPollableSorter` + `Dep@CharPollableSorter` ctor + the `Dep`-typed
  sort machinery (`__push_heap`/`__adjust_heap`/`__make_heap`/`__insertion_sort`/
  `__final_insertion_sort`/`sort_heap`/`__unguarded_partition`/`__linear_insert`),
  `??6...Lod@Character` + `??$?6ULod@Character` (Lod serialization).
- **2 foreign-NAMED ICF singletons** — `?CalcDistTo@RndCam@@...` and
  `?CalcScreenHeight@RndCam@@...`. BOTH are emitted by our Character.obj (Character
  inlines RndCam math in ComputeScreenSize). Spot-checked via objdiff: CalcDistTo =
  100% normalized, **31 insns all-equal** (a real instantiation, not a degenerate stub).
  Own instantiation, honestly matched.

## 3. DC3 corroboration — the cluster IS Character.obj's own TU

DC3 `ham_xbox_r.map` has a real contiguous `char:Character.obj` TU. Its `.text` window
`0x8235B000..0x8235F200` is **101 `char:Character.obj` symbols** (the dominant owner),
and critically: **the `CharPollableSorter::Dep` STL templates are `char:Character.obj`
in DC3** (~50 entries, mostly `f i` ICF-folded). Character holds a `CharPollableSorter`
member; the `Dep`-sort heap machinery is Character's own instantiation — the textbook
"own STL bracketed by own named methods = OK" case. The candidate's "CharPollableSorter
templates attributed to Character" is honest (the "184" figure in the commit message is
a miscount; ownership is correct).

DC3's Character.obj `.text` even extends to DC3 VA `0x8235e094` (own `__unwind$`
funclets) — at near-identical VA to the RB3 cluster, because Character.obj sits at the
same VA in both binaries.

## 4. Longest contiguous FOREIGN run

**Zero.** No matched fn is foreign (all 45 in our obj). No unmatched fn is foreign
either (all 75 in our obj). The 75 unmatched (59 anon + 16 named) are porting-incomplete
Character-own bodies reading <100% — own STL/methods, not foreign folds. There is **no
>=8-contiguous run of foreign fns anywhere** in the unit. The 2 foreign-NAMED matched
(RndCam) are isolated singletons, each its own instantiation.

## 5. PDATA / mechanics

`.pdata [0x821F9790,0x821F9B08)` (888 B / 111 entries) was dtk-auto-derived for the
0x3FB0-byte `.text` cluster (not hand-fudged). Splits-only change; no header/body edits;
no cross-TU regression vector.

## Honesty-gate disposition

- matched > 0 ✓ (+45)
- no >=8-contiguous FOREIGN fn run ✓ (longest foreign run = 0; 0 foreign bodies in unit)
- clear majority (in fact ALL 45) are the pinned TU's own instantiations ✓
- clean same-tree A/B, zero regressions, headline net == intended unit gain ✓ (Character 0->45 only)
- DC3 map corroborates a contiguous Character.obj TU incl. the CharPollableSorter templates ✓

**LAND +45 (HONEST).**
