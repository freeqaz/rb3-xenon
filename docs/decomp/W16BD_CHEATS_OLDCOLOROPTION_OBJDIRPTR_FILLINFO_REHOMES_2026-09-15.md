# W16-BD — Cheats / OldColorOption / ObjDirPtr / FillInfo re-homes

**Lane:** W16-BD · **Date:** 2026-09-15 · **Branch:** `w16-bd` (off `main`) ·
**Worktree:** `~/tmp/wt-w16-bd`

**Ruler, read from `report.json`'s own `provenance` (never inferred):**
`functionRelocDiffs=name_check`, `ppc.calculatePoolRelocations=false`,
objdiff `tool_commit a5f0ea903ec1`, `tool_binary_hash 5a51cd51fe0a353f` —
identical on the baseline build and on every measurement below, so all deltas
in this document compose on one ruler.

## Lane-internal totals

| | `matched_functions` | `matched_code` | `matched_code_percent` |
|---|---:|---:|---:|
| baseline (build 0, reproduced the brief exactly) | 43,506 | 4,036,280 B | 39.3937 % |
| after the lane | **43,510** | **4,037,196 B** | **39.40264 %** |
| **delta** | **+4** | **+916 B** | **+0.00894 pp** |

`total_code` 10,246,004 and `total_functions` 69,216 are **unchanged throughout**
— no pin in this lane moved the denominator.

Every per-item figure below is a **set-diff of the `fuzzy == 100` rowset**
(`tools/rowset_snapshot.py save` / `diff`), taken across a **full**
`./tools/ninja-locked`, never from a single-obj build. Deltas sum exactly:
`+1 +0 +1 +1 +0 +1 +1 −1 = +4` functions and
`340 + 0 + 124 + 108 + 212 + 88 + 76 − 32 = 916` bytes.

## Commits

| sha | item |
|---|---|
| `2acb4d92` | 1 — CheatsManager ctor re-home |
| `cf8d485b` | 1b — drop the dead GlitchFinder scatter-include |
| `f9ecf043` | 2 — `0x822ab0b0` is `_M_fill_insert<OldColorOption>`, block SPLIT |
| `19ca9e99` | 2 — residual alias (DistEntry / OldColorOption `_M_insert_overflow_aux`) |
| `6ffce478` | 3 — `0x82817ae8` is `ObjDirPtr<HamListRibbon>::operator=`, re-home |
| `f76632d3` | 3 — repair alias, **and** the withdrawal it forced |
| `30687080` | 4 — `fn_8278C2F8` is `??_GFillInfo@@UAAPAXI@Z`, block SPLIT |
| `e5577def` | 5 — App::App's eight EH funclets (W16-BF's filed proposal) |

Branch tip: `e5577def` (plus this document).

---

## Item 1 — `??0CheatsManager@@QAA@XZ` re-homed StringTable.cpp → Cheats.cpp

**Predicted +1 fn / +340 B. Measured +1 fn / +340 B.**

```
StringTable.cpp:
-	.text       start:0x827C2780 end:0x827C28D4
 	.text       start:0x827C8C88 end:0x827C97C8
Cheats.cpp:
-	.text       start:0x827C1A84 end:0x827C2780
-	.text       start:0x827C28D4 end:0x827C3340
+	.text       start:0x827C1A84 end:0x827C3340
```

CROSSED IN 1 row / 340 B — `default/Cheats::??0CheatsManager@@QAA@XZ`.
FELL OUT 0.

The 340 B sat between two Cheats blocks and was pinned to StringTable, whose base
obj cannot define the name; objdiff pairs target↔base **by name**, so the row read
0 however correct the source. The move merges the two adjacent Cheats blocks.
dtk corroborated independently by re-deriving `.pdata`: Cheats' `0x82242028–
0x822420D8` + `0x822420E0–0x822421B8` became `0x82242028–0x822421B8`, and
StringTable's lone `0x822420D8–0x822420E0` record disappeared.

## Item 1b — the GlitchFinder scatter-include (the lane's only `src/` edit)

**Predicted Δ0. Measured Δ0** (0 rows crossed, 0 fell out).

`src/system/utl/StringTable.cpp:103`'s `#include "utl/GlitchFinder.cpp"` existed to
pair the 340 B at `0x827C2780`. That address is CheatsManager's ctor and is now
homed in Cheats.cpp, and GlitchFinder is absent from retail entirely, so the
include paired with nothing. Replaced with a comment; the `#define gRev …` /
`#undef` scaffolding is left in place (now an empty block) because the lane's
`src/` bar is this one line.

⚠ **The replacement comment was rewritten before commit.** My first version
contained the literal token `#include "utl/GlitchFinder.cpp"` inside the comment —
the same class of defect as commit `6c087cbd`, where prose containing a directive
token desynchronised `ScatterIncludes.cmake` and broke the native link. The
shipped comment contains **no directive token**, and directive depth was
re-verified balanced (0) rather than trusting the line-anchoring fix. This is why
the native gate is this lane's last action.

## Item 2 — `0x822ab0b0` is `_M_fill_insert<OldColorOption>`, and the block is SPLIT

**Pre-registered +2 fns / +232 B (BC §3.3). Measured for the splits+map change:
+1 fn / +124 B.** The shortfall was diagnosed, not absorbed, and the remaining
+108 B was then collected by an adjudicated alias — reaching BC's full +232 B.

```
HamCamTransform.cpp:
-	.text       start:0x822AB0B0 end:0x822AB380
+	.text       start:0x822AB128 end:0x822AB380
OutfitConfig.cpp:
+	.text       start:0x822AB0B0 end:0x822AB128
```

Map row `0x822ab0b0`: `_M_fill_insert<vector<TransformCrowd>>` →
`?_M_fill_insert@?$vector@VOldColorOption@@V?$StlNodeAlloc@VOldColorOption@@@stlpmtx_std@@@stlpmtx_std@@AAAXPAVOldColorOption@@IABV3@@Z`
(spelling read from OutfitConfig.obj's COFF, not typed by hand).

**Deviation from the brief, deliberate:** BC asked for a whole-block move of
splits line 3303. The block holds **7 rows, 2 of them at mpn 100**, so moving it
whole would have carried matched rows out of a unit whose base obj defines them.
A SPLIT of just the 108 B row is better on **both** accuracy and metric.

**Why the prediction was half wrong:** the 108 B row became pairable
(fuzzy 99.62963 → 99.81481) but retained **one** unenumerated charged site — a
`diff_arg`, retail `bl _M_insert_overflow_aux<DistEntry>` against our
`<OldColorOption>`. `matched_code` is all-or-nothing per row, so a row one charge
short pays nothing. This is RESIDUAL-1 exactly: price from the graded charged-site
list, never from a mismatch or instruction-equality count.

`19ca9e99` installs that fold as an alias group (`_M_insert_overflow_aux`
@ `0x822aa578`, survivor the `DistEntry` spelling), adjudicated with
`icf_pair_adjudicate.py` after running its `--selftest` and `--chasetest`
controls, with the weak half of the evidence stated in the group's `evidence`
string. **Measured +1 fn / +108 B**; validator PASS afterwards (1650 → 1651
groups).

## Item 3 — `0x82817ae8` is `ObjDirPtr<HamListRibbon>::operator=`

**Measured for the splits+map change: +0 fns / +212 B** (+300 crossed in,
−88 fell out), then **+1 fn / +88 B** for the repair alias. Net **+1 fn / +300 B**.

```
HamNavList.cpp:
-	.text       start:0x82817A68 end:0x82817AE8
+	.text       start:0x82817A68 end:0x82817C20
UILabel.cpp:
-	.text       start:0x82817AE8 end:0x82817C20
```

Map row `0x82817ae8`: `null` → `??4?$ObjDirPtr@VHamListRibbon@@@@QAAAAV0@PAVHamListRibbon@@@Z`
(asserted free elsewhere in the map first). Identity established by **asymmetry**,
not by a single PROVEN: FLAT-PROVEN for `<HamListRibbon>` while both rivals were
FLAT-REFUTED, and only chased-PROVEN through a slot resolving to HamListRibbon's
own `PostLoad`.

**The −88 B was not noise and is fully attributed.**
`default/UIFontImporter::??1?$ObjDirPtr@VUILabelDir@@@@UAA@XZ` fell out at
instruction 12 of 22, a `diff_arg`: retail's own UILabelDir ObjDirPtr destructor
calls `0x82817ae8`, so the two `operator=` instantiations folded and the name I
installed is the survivor's arbitrary spelling. That is MAPID-1's economics
running exactly as documented — **naming an anonymous address is a bet that pays
in bug exposure, not bytes**. Repaired with alias group `$ObjDirPtr_assign`
@ `0x82817ae8` (survivor `<HamListRibbon>`, folded `<UILabelDir>`): +1 fn / +88 B.

### The contradiction that repair exposed, and the withdrawal it forced

Installing that group turned `icf_alias_finder.py --validate` **FAIL — 1
CONTRADICTED**. The flagged group was **not** mine: a pre-existing group
`4?$ObjDirPtr` **also at `0x82817ae8`**, survivor
`??4?$ObjDirPtr@VObjectDir@@@@…`, folded `<HamListRibbon>`. Once my map row named
`0x82817ae8`, that group had two **map-resident** members — the validator's only
fatal class.

Adjudicated on retail bytes, and the group is **REFUTED**:

* `ObjDirPtr<ObjectDir>::operator=` is `0x82270690` (defined in RhythmDetector.obj);
  `ObjDirPtr<HamListRibbon>::operator=` is `0x82817ae8` (HamNavList.obj).
* Both are 300 B and **reloc-masked byte-identical — 0 differing words of 75**,
  which is precisely why the T1 generator proposed a fold.
* Their relocation at **+0x48 differs and IS the discriminator**: `0x82270690`
  tail-calls `0x82270340` `PostLoad<ObjectDir>`, `0x82817ae8` tail-calls
  `0x82817a68` `PostLoad<HamListRibbon>` — two distinct map-named addresses.
  `/OPT:ICF` folds only COMDATs identical **including relocations**, so a differing
  `bl` target is proof they were never folded.
* The fold claim was an **artifact of the old map name**: until W16-BA nulled it,
  the ObjectDir spelling sat on `0x82817ae8`, so the group's address described
  HamListRibbon's body while carrying ObjectDir's name.

Repair (`f76632d3`): the membership is **withdrawn** — `folded: []` plus a
`withdrawn` record, nothing pruned, per the standing rule that a removal without
one is a clobber — and the group's `address` is corrected `0x82817ae8` →
`0x82270690`.

⚠ **The address correction is not cosmetic, and this is the transferable lesson.**
`tools/gen_symbol_alias_map.py` buckets equivalences **BY ADDRESS** and
objdiff's `reloc_eq` equates a whole bucket. Leaving the withdrawn group on
`0x82817ae8` would have merged ObjectDir into the new
`{HamListRibbon, UILabelDir}` bucket and kept forgiving the very charge being
withdrawn — the withdrawal would have been **prose only**. A withdrawal that
leaves the group's address colliding with a live group is not a withdrawal.

The prior `restore`/`restored` records (RELOC-RECONCILE 2026-08-20, W9-D
2026-09-13) are left intact: their observation — our compiled HamListRibbon body
matches exactly one retail `.pdata` start, `0x82817ae8` — is **correct**, and is
now the evidence that `0x82817ae8` is HamListRibbon's **own** body rather than a
fold survivor.

**Pre-registered Δ0 for the withdrawal; measured Δ0** (CROSSED IN 0 / FELL OUT 0,
43,510 → 43,510, 4,037,152 → 4,037,152) — with both addresses correctly named our
source spells its own `T`, so the ObjectDir forgiveness was inert.

## Item 4 — `fn_8278C2F8` is `??_GFillInfo@@UAAPAXI@Z`, SPLIT into SongData.cpp

**Predicted +1 fn / +76 B. Measured +1 fn / +76 B.**

```
system/beatmatch/PhraseAnalyzer.cpp:
-	.text       start:0x8278C0C0 end:0x8278C348
+	.text       start:0x8278C0C0 end:0x8278C2F8
SongData.cpp:
+	.text       start:0x8278C2F8 end:0x8278C348
map: + "0x8278c2f8": "??_GFillInfo@@UAAPAXI@Z"
```

CROSSED IN 1 row / 76 B — `default/SongData::??_GFillInfo@@UAAPAXI@Z`.
FELL OUT 0.

The row read fuzzy 0 / mpn 0 because PhraseAnalyzer.obj cannot define the name;
our build defines it in exactly one compiled object, `SongData.obj`. A SPLIT, not
a move: the block also holds `SetPhraseIDs` (160 B) and `Analyze` (408 B), both at
fuzzy 100. The 4 bytes `0x8278C344–0x8278C348` travel with the function
deliberately — inter-function padding owned by no COMDAT, carrying no function row
and not in `total_code`, and `0x8278C348` begins another unit's block, so this
keeps both units contiguous instead of stranding a 4-byte block.

The briefed reloc divergence (ours `??3@YAXPAX@Z` vs retail
`??3BinStream@@SAXPAX@Z`) needed no work: it is already inside the 205-member
`CONTRADICTION_EXEMPT` group at `0x8240ddb0`.

dtk corroborated the boundary by moving **exactly one unwind record**:
PhraseAnalyzer's `0x8223EEE0–0x8223EEF8` shrank to `–0x8223EEF0`, SongData gained
`0x8223EEF0–0x8223EEF8`.

## Item 5 — App::App's eight EH funclets (filed by W16-BF)

**W16-BF pre-registered −1 fn / −32 B. Measured −1 fn / −32 B.**

```
RhythmDetector.cpp:
-	.text       start:0x822715B0 end:0x82271748
+	.text       start:0x822716E8 end:0x82271748
App.cpp:
-	.text       start:0x82270E68 end:0x822715B0
+	.text       start:0x82270E68 end:0x822716E8
```

FELL OUT 1 row / 32 B — `default/RhythmDetector::fn_822715D8`. CROSSED IN 0.

Applied only after re-verifying the extents myself on retail bytes, as the handoff
required. From `orig/45410914/band.exe`, PE-mapped, big-endian: the 8-byte EH
prefix at `0x82270E60` is `82829530 82000de0`, so the function at `0x82270E68`
(App.cpp's single `.text` block) has FuncInfo at `.rdata:0x82000DE0` — magic
`0x19930522`, maxState 8, pUnwindMap `0x82000da0` — whose eight UnwindMap actions
are `0x822715b0 / 5d8 / 5f8 / 620 / 648 / 670 / 698 / 6c0`, sizes
40/32/40/40/40/40/40/40 = **312 B**, ending at `0x822716e8`. The moved range is
App::App's funclet set to the byte.

⚠ The geometry differs from the handoff summary: RhythmDetector's block ran to
`0x82271748`, not `0x822716E8`, so this is a partial **split** of that block, and
because App.cpp's block already ended at `0x822715B0` the range **merges** into it.

The row that falls out **is one of the eight funclets** — the 32 B one — which had
been pairing by byte signature inside RhythmDetector's pool (objdiff pairs funclets
by signature, not identity) and does not re-pair in App's pool. That is the whole
trade, and it is the standing directive's shape: a code% drop from a truer
attribution is a win.

## Gates (in the brief's order, native LAST)

```
full build                                                 rc=0
python3 scripts/verify_ruler_agreement.py --check           rc=0
  OK: both objdiff-cli entry points resolve the same ruler.
python3 scripts/verify_objs_patched.py --verify-manifest     rc=0
  [patch-state] OK: 1215 decomp, 3114 target objects match (tree_sha256=66ca343d8ef0fe3e)
python3 tools/icf_alias_finder.py --validate                 rc=0
  VALIDATE: PASS -- 1404 map-consistent, 247 tolerated, 0 contradicted, 1652 total
tools/native_build_gate.sh                                   rc=0
```

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

`skipped=0`, so this is full coverage, not the `PASS (INCOMPLETE: …)` shape. Only
this document was committed after the gate run — a markdown file cannot reach the
native link.

## NOT done, and why

* **Splits block 3304 was NOT moved** (item 2). The masked-compare gave **no
  positive OutfitConfig evidence for any of its 12 bodies**, and the block holds
  44 matched bytes and 3 mpn-100 rows that a move would put in a unit that cannot
  define them. Moving it would have been a metric loss bought with no accuracy.
* **`0x822ab128` NOT named.** AZ §3.4 read it as `_M_insert_overflow_aux<BandPatchMesh>`
  (82/82 words); a chased T1 adjudication **REFUTES** that — retail's
  copy-construct slots resolve to `??0TransformArea@@QAA@ABV0@@Z` @ `0x8234c410`
  and `_Copy_Construct<TransformArea>`. Our `<TransformArea>` spelling is **also**
  refuted (masked bodies differ). **Filed as an open contradiction:** the map
  already places `_M_insert_overflow_aux<TransformArea>` at `0x822a84d8` (99.939),
  so if `0x822ab128` is that instantiation, one of the two names is wrong. Naming
  it on a refuted hypothesis would be a fabricated map row.
* **`0x822ab3e0` NOT named** — 588 B, no masked twins, and AZ's ctor guess is
  inconsistent with our 1,016 B `??0OutfitConfig`.
* **`<HamScrollSpeedIndicator>` deliberately NOT added** as an alias membership:
  chased-PROVEN but **unwitnessed** — no retail caller, no charged site. An alias
  is pure forgiveness; installing an unwitnessed one lifts the score by
  construction and buys nothing real.
* **`0x822ab2b8`'s `_Destroy<DynamicPropertyEntry@Flow>` row** is now unpairable in
  OutfitConfig (4 B, mpn 95). No measurable loss, and the address is outside this
  lane's map bar.
* **No `symbols.txt` edit, no `.pdata` hand-edit** anywhere in this lane — every
  `.pdata` change in the diff is dtk's own re-derivation.
* **W16-BF's `EventTrigger.cpp` / `EventAnim.cpp` headings were not touched**, per
  the concurrency bar; BF owns them on its own branch.
