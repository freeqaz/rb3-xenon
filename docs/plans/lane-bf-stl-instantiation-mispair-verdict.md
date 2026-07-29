# laneBF W6 — CLASS VERDICT: the STL-instantiation near-miss band is MAP MISPAIR

**Date:** 2026-07-29 · **Branch:** `laneBF-6` · **Worktree:** `~/tmp/wt-laneBF-6`
**Scope:** the ~200 STL-template instantiations sitting at 99.9x% in the 96–100%
named band (clusters `_M_insert_overflow_aux`, `_M_fill_insert`,
`__uninitialized_fill_n`, `__uninitialized_copy`, `_M_allocate_and_copy`,
`_Destroy_Range`, `resize`, `NewObject`, …; see
`scripts/harvest/identical_pct_cluster_scan.py --axis score_shape`).

## Verdict

**The class is MAP MISPAIR, essentially in full. Zero members were shown to be a
genuine element-`sizeof`/layout divergence. Do not fund source work on it.**

The measured split (see Evidence):

| bucket | n | share |
|---|---|---|
| proven MISPAIR (target COMDAT belongs to a *different* instantiation) | 20 / 24 *decidable* stride cases | 83% |
| proven GENUINE layout bug (our struct is wrong) | **0 / 24** | **0%** |
| inconclusive (family-key collision) | 4 | 17% |
| undecidable in-place (no 100% family anchor to compare against) | 37 | — |
| independent twin-body proof over the STL near-miss pool | 47 / 118 | 40% (lower bound) |
| hand-adjudicated spot checks, all MISPAIR | 11 / 11 | 100% |

Only **one** counter-example surfaced in the whole sweep:
`?_M_fill_insert_aux@vector<SpotMeshEntry@SpotlightDrawer>` (`default/SpotlightDrawer_NG`,
97.12%) — correctly paired (same instantiation both sides, both stride 0x50);
its residual is an r25↔r26 regalloc swap plus a `divw.`/`divwu.` scheduling
difference. That is the permuter/regalloc class, not this class.

## Why the "negative control" warning resolves the same way

The cluster scan warned that 66 `_M_insert_overflow_aux` and 1044 `??0`
instantiations **already read 100.0** and could be broken by a layout change.
They are, in fact, **mispairs too** — merely *size-coincident* ones. In
`default/HamCamTransform` the target obj's real content is retail
`OutfitConfig.cpp`'s STL family; `sizeof(TransformArea) == sizeof(MatSwap@OutfitConfig) == 0x70`,
so every `TransformArea` helper folds byte-identically onto the `MatSwap` COMDAT
and scores 100. `NavItem` (0x28) vs `Piercing@OutfitConfig` (0x60) does not
coincide, so it scores 99.9x. **Same defect, different visibility.** Any struct
change made to chase a stride would corrupt a correct struct *and* break the
size-coincident 100s. Confirmed: **no source change is proposed, none should be.**

## Evidence

### 1. Self-contained proof — intra-family stride contradiction (new tool)

`scripts/harvest/family_stride_proof.py <project_dir>`

Within one target obj, every helper of the same `vector<T>` family must stride by
`sizeof(T)`. If a **100%-matching, byte-identical** member of family `T` strides
by `S` in the *target*, and a near-miss member of family `T` strides by `S' != S`
in the *target*, the near-miss target COMDAT cannot belong to family `T`. No
oracle, no struct DB, no assumption about our headers.

Canonical case — `default/HamCamTransform`, family `vector<DistEntry>`:

* `?resize@vector<DistEntry>` — target obj bytes **identical** to ours — `srawi 5`, `slwi 5` ⇒ stride **0x20**
* `??1vector<DistEntry>` — target obj bytes **identical** to ours — `srawi 5` ⇒ stride **0x20**
* `?_M_insert_overflow_aux@vector<DistEntry>` — 99.864% — target `srawi 4`, `slwi 4`, `addi 0x10` ⇒ stride **0x10**

Global result: **20 PROVEN MISPAIR, 0 GENUINE LAYOUT BUG.** The tool reports
both directions; the "our struct is wrong" bucket came back empty.

### 2. Independent proof — masked-body twin identity (new tool)

`scripts/harvest/stl_mispair_twin_scan.py <project_dir>` (uses
`scripts/harvest/coff_func_bodies.py`) masks relocation words and asks whether the
*target* body is byte-identical to one of **our** instantiations under a
different name. 47 of 118 STL near-misses positively identify a foreign
instantiation. (This is a lower bound: it only fires when we happen to compile
the true instantiation somewhere in the tree.)

### 3. Raw-mode callee identity, per case

`run_diff_inspect(..., diff_mode="raw")` exposes the relocation targets that
normalized mode hides. Representative:

| function | target really is | proof |
|---|---|---|
| `__uninitialized_fill_n<DistEntry*>` | `<Key<RndMatAnim::TexPtr>*>` | calls `_Copy_Construct<Key<TexPtr>>`; `sizeof(Key<TexPtr>)==0x10` == target stride |
| `__uninitialized_fill_n<NavItem*>` | a 0x60 element (`Piercing@OutfitConfig`) | stride 0x60; compiler `sizeof(NavItem)==0x28` |
| `_Destroy_Range<NavItem*>` | an `ObjVector<Lod@Character>` range | calls `__destroy_mv_srcs<reverse_iterator<ObjVector<Lod>*>>` |
| `?resize@ObjVector<EyeDesc@CharEyes>` | `ObjVector<OldMatOption>::resize` | calls `??0OldMatOption`, `??1OldMatOption`, `resize@list<OldMMInst>` |
| `?NewObject@FlowNode@@` | `BandCharDesc::NewObject` | `li r3,0x260`, `??0BandCharDesc`, `StaticClassName@BandCharDesc` |
| `?PropSync@@(TransformCrowd&)` | a `{Symbol@0x0, list<EventSinkElem>@0x4}` class | target syncs `PropSync<Symbol>` then `PropSync<list<EventSinkElem@MsgSource>>` |

### 4. Calibration of the weak signal (do not use alone)

A "target callee names a foreign type" heuristic fires on **415 / 631 (66%)** of
the 96–100% named band — but it also fires on **8564 / 18183 (47%)** of functions
that read **100.0**, because ICF gives folded callees an arbitrary representative
name. Enrichment is only ~2.2×; it is a *screen*, not a proof. Use
`family_stride_proof.py` (§1) or the twin test (§2) for verdicts.

## Routing consequence

This class belongs to the **splits / `target_symbol_map.json` attribution
channel**, not the decomp channel. The defect is COMDAT scatter: a pinned unit's
`.text` range contains a neighbouring retail TU's STL COMDATs, and the map labels
them with our TU's instantiation names.

Repointing via `scripts/harvest/invcorr_mispair_repoint.py` is *not* worth it
here: the correctly-named instantiation is almost never one we compile in the
same unit, so a repoint converts a 99.9%-but-unmatched function into an unpaired
one — 0 strict gain, fuzzy loss. Per the attribution census, never add a map
entry for a VA outside that unit's `splits.txt` range.

## Files

* `scripts/harvest/family_stride_proof.py` — the decisive, self-contained classifier
* `scripts/harvest/stl_mispair_twin_scan.py` — masked-body twin identification
* `scripts/harvest/coff_func_bodies.py` — per-function COFF body + reloc-name extraction (shared)
