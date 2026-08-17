# The decomp copy-paste "twin body" class is NOT systematic — lane W8-TWINPORT, 2026-08-17

**Question asked:** lane W2-ENGINE found `BandDirector::OnGetCatList` and
`OnCopyCats` byte-identical in our tree — a copy-paste whose real body was never
ported, invisible to the metric because *the copy scores high BECAUSE it is a
copy*. Is that systematic, or a one-off?

**Answer: effectively a one-off.** After adjudication the hidden class contains
**exactly one** real instance — the one already known — and it is now ported and
validated on retail bytes. **The next lane should not re-hunt this vein.**

**But the sweep is not worthless — it turned out to be a MAP-DEFECT detector.**
Every other flagged group adjudicated to a `target_symbol_map.json`
misidentification or to inlining noise, and chasing them produced a landed
+184 B map fix and two more well-evidenced map defects (below).

---

## The instrument

`tools/twin_body_sweep.py`. Group our compiled symbols by **relocation-normalized
body identity** (reusing `icf_alias_build.collect`, the established reader —
⛔ raw `memcmp` is silently vacuous here, `bl` displacements differ by address),
then ask what retail has at the corresponding addresses.

| arm | condition | why |
|---|---|---|
| **A** | ≥2 twins map-resident | decisive (compare retail bodies) but **NOT hidden** — objdiff already charges the copy against its own retail address |
| **B** | exactly 1 map-resident | **THE HIDDEN CLASS** — the unmapped twin is *unpaired*, so it carries no penalty at all |
| **C** | 0 map-resident | no retail evidence either side; unadjudicable |

ARM B conflates *retail ICF-folded them* (correct) with *the twin is unported*
(defect). Discriminator, generalised from W2's manual adjudication: over callers
present on **both** sides, compare slots-into-group (ours) against
slots-to-survivor (retail). Fewer retail slots ⇒ some site went elsewhere ⇒ not
folded. Compares relocation **target names** only; no address arithmetic.

### It was calibrated to both fire and fail

* **POSITIVE** — reproduced the known defect mechanically: `BandDirector::Handle`
  has **2 slots into the group on our side, 1 to the survivor on retail's**.
* **NEGATIVE** — **19/21** T1-proven folds from `symbol_aliases.json` classified
  `FOLD`, not flagged. The 2 that flag are template twins
  (`vector<Symbol>::erase` vs `<TrackType>`), which the same-class/different-method
  "copy-paste shape" filter excludes by construction.

An uncalibrated sweep returning "nothing found" was the outcome to fear here — it
agrees with the null and closes the vein permanently.

---

## Population and result

2,516 groups at ≥32 B; **only 19.5% are adjudicable at all** (the rest have no
retail evidence on one or both sides — that is a coverage bound, not a clearance).
Restricting to the copy-paste shape (**same class, different method names** — a
decomp copy-paste, as opposed to a template twin) gives 102 groups.

**All three ARM-B copy-paste hits, adjudicated:**

| pair | verdict | evidence |
|---|---|---|
| `BandDirector::OnGetCatList` / `OnCopyCats` | **REAL DEFECT** | retail's `Handle` calls **two distinct addresses**; ported, see below |
| `SongStatusMgr::GetScore` / `GetHighScore` | **FALSE POSITIVE — mis-mapped CALLER** | `BandProfile::GetSongHighScore` was itself misnamed; fixing the map flipped the group `DEFECT → FOLD` |
| `DataNode::Array` / `Int` | **FALSE POSITIVE — inlining noise** | 954 shared callers, 1976 vs 1953 slots, disagreements in **both** directions; the pair is a *proven fold* (identical COMDATs **incl. relocations** = `/OPT:ICF`'s own condition) |

After the fix and the new `NOISE_BIDIRECTIONAL` guard, **ARM B copy-paste
`DEFECT` = 0.**

---

## What was ported, and how retail corrected it

`BandDirector::OnGetCatList` — the real body (build a filtered `DataArray` of the
symbols whose `RemapCat` is identity), from the rb3-Wii oracle.

Retail's `Handle` calls `fn_822905D0` where we call `OnGetCatList` and
`?OnCopyCats@` at the other slot ⇒ **two distinct addresses ⇒ not folded ⇒ the
defect is real**, and `0x822905D0` **is** `BandDirector::OnGetCatList`. Against it:

```
retail 360 B / ours 360 B      masked body IDENTICAL
15 relocation slots vs 15, agreeing in 13
```

Both differences are provably benign: `Int` vs `Array@DataNode` (the proven ICF
fold above) and `PoolAlloc` 5-arg vs 2-arg (the pre-existing global `MemAlloc`
file/line divergence).

★★ **The oracle's "redundant" tail was load-bearing and my tidying was the bug.**
rb3-Wii ends `DataNode ret(...); arr->Release(); return DataNode(arr, kDataArray);`
— constructing `ret` and seemingly ignoring it. Read as a Wii-decomp artifact and
normalized to `return ret;`, it compiles to **324 B**. Retail's relocation
sequence is `??0DataNode, Release, ??0DataNode, Release` — *two* constructions,
the second `Release` being `~ret` inlined. Restoring the oracle verbatim moved
**324 → 360 == retail exactly.** The familiar warning is "the oracle records
intent, not the MSVC spelling"; **this is the converse, and it bites just as hard.**

---

## Failed predictions (both wrong, both instructive)

| change | predicted | measured |
|---|---|---|
| map fix (3 BandProfile thunk rows) | +24 B | **+184 B** |
| source port | Δ0 / Δ0 | **+2 matched / +144 B** |

* **Map fix:** I priced the renamed **rows** only. Under `name_check` a wrong map
  name also charges every **caller** that relocates against it, so correcting one
  name pays at the row *and* at its call sites. ⇒ price from `report.json`'s
  charged-site list, never from the size of the patch.
* **Port:** I reasoned "unmapped ⇒ unpaired ⇒ metric-inert". `masked_equal` rose
  by exactly +2 — `matched_code` also pairs bodies through the **funclet
  byte-signature channel, which needs no map name**. ⇒ **"unmapped ⇒ metric-inert"
  is FALSE**, and a body-accuracy fix can pay with no naming at all.

---

## Landed

* `tools/twin_body_sweep.py` — the calibrated instrument + `NOISE_BIDIRECTIONAL`.
* `fix(map)` BandProfile lesson-section thunks (3 rows), **+184 B**, Δfn 0.
  `IsProBass`/`IsProKeyboard@BandProfile` had been mapped **nowhere**.
* `fix(BandDirector)` the real `OnGetCatList`, **+2 fns / +144 B**.
* Native gate **PASS 18/18, 0 SKIPs, rc=0** with the port in place.

## NOT done — well-evidenced, left for a map lane

Both from an ARM-A sub-investigation; **read-only, no A/B was run on either**, and
both need a coordinated map + `splits.txt` + `symbol_aliases.json` edit that this
lane deliberately did not half-apply.

1. **`0x827b7d88` is `?Unload@StoreArtLoaderPanel@@UAAXXZ`, not
   `ProfileMgr::HandleProfileLoadComplete`.** Retail's body there calls
   `ClearArt@StoreArtLoaderPanel` and `Unload@UIPanel`; `ProfileMgr` has neither
   and does not derive from `UIPanel`. Our `ProfileMgr` twins are a *faithful*
   port (rb3-Wii has them identical too) and retail folded them at `0x825490e8`.
   ⚠ `symbol_aliases.json` carries a **T1 group asserting a fold that does not
   exist** — the byte evidence is genuine, the inference from it is wrong. The
   `splits.txt` pin is **circular**: a lone 64 B `ProfileMgr.cpp` block exists
   *because* the map claimed that name.
2. **The `ByteCode`/`StaticByteCode` family is a bijection artifact.** The virtual
   and static forms compile to the *same* folded body (no `this`/prologue
   difference at all); the sweep was seeing two different **message classes**.
   Decoding the string operand out of retail `.text` shows e.g.
   `?ByteCode@AccomplishmentEarnedMsg@@` @`0x82690888` points at
   **"ResumeNoScoreGameMsg"**. A 53-body census finds 53 distinct strings, zero
   duplicates. `target_symbol_map.json`'s own `_bijection_arbitrary_comment` says
   which name belongs on which VA is not established; **8 of 51 land on another
   class's survivor**, of which only 3 are flagged. Exposure: ~8 `matched_functions`
   and ~420 `matched_code` bytes credited to rows on the wrong address, **6 of 7
   scoring a clean mpn 100 / fuzzy 100** because the string operand is a
   `lbl_*` placeholder `name_check` forgives.

## Traps for the next lane

* ⛔ **`collect()` over our objs is MULTI-DEFINER.** Scatter-includes
  (`#include "Foo.cpp"`) put one function in many objs — `BandDirector.cpp` lands
  in **15** — and a glob keeps whichever definer sorts **last**. Rebuild the whole
  tree before reading our side, or a stale sibling silently shows you the pre-edit
  body. This lane briefly read a successful port as a no-op.
* ⛔ **Do not edit the worktree while `ab_measure` is running.** It restores the
  tree on *every* exit path, and that restore reverted a tool edit made mid-run.
  The revert is silent and the file simply reads as if you never changed it.
* ⚠ **A `DEFECT` verdict is a suspicion about a NEIGHBOURHOOD, not a proof about
  the pair** — 2 of 3 hits were caused by something other than the pair. Read the
  caller's retail relocation targets before believing it.
* ⚠ **`--selftest`'s positive control now FAILS**, because the defect it points at
  is fixed. That is the fix working. Re-point it at a fresh instance or run it
  against the parent commit; do **not** loosen the grouping to "repair" it.
