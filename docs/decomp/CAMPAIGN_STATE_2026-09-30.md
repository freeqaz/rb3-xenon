# CAMPAIGN STATE — 2026-09-30 (fourth edition)

> **SUPERSEDED** for numbers and priorities by
> [CAMPAIGN_STATE_2026-10-02.md](CAMPAIGN_STATE_2026-10-02.md) (ceiling 61.745%,
> 88.36% of it). Kept as the dated record of what moved +499,860 B on 09-30.

> Replaces [CAMPAIGN_STATE_2026-08-17.md](CAMPAIGN_STATE_2026-08-17.md) as the
> current-state doc. That edition's partition and arc history stay valid as a
> dated record; its **strategic claim "there is no big lever left" is refuted
> by §2** (+499,860 B in one day, from breadth and shared-template shape rather
> than from any single large lever).

Measured on main's live `report.json` at **`61bb82227`** (built 23:54 UTC; the
tip merge `61bb82227` is tooling-only, so this equals `1a679219f`), ruler read
from `provenance`: `name_check`, objdiff 4.2.9. **W16-HZ (`a95d5c525`, +136 fns
/ +20,336 B, engine anon round 3) landed after this census** — the ENGINE
ANON_0 pool below is already ~20 kB smaller on main.

## 1. Headline

| | morning (W17-TARGET baseline) | `61bb82227` | Δ |
|---|---:|---:|---:|
| `matched_functions` | 44,154 | 46,403 | **+2,249** |
| `matched_code` | 4,177,988 | 4,677,848 | **+499,860 B** |
| `matched_code_percent` | 40.772522 | 45.650580 | **+4.878 pp** |
| `fuzzy_match_percent` | 50.497726 | 54.578014 | **+4.080 pp** |
| `masked_equal_functions` | 23,323 | 23,689 | +366 |
| honest (`matched − masked`) | 20,831 | 22,714 | **+1,883** |
| `total_functions` / `total_code` | 69,240 / 10,247,068 | 69,153 / 10,247,072 | −87 rows (carve/re-home fixes) / +4 B |

The morning baseline is the W17-TARGET brief's (priced at old-lineage
`8af79551`, which the 09-30 history rewrite replaced — it is not an ancestor
of main any more; the numbers stand).

**Reachable ceiling, re-measured** (`tools/ceiling_recompute.py`, same rules as
09-16): PAIRABLE 6,484,424 B = 63.281% − scaffold shells 178,664 B (103 units)
= **6,305,760 B = 61.537%**. `matched_code` = **74.18% of the ceiling** (09-16:
65.64%). Gap to ceiling **1,627,912 B**. The ceiling moved +0.002 pp; the share
moved +8.5 pp — again the campaign moved, the denominator did not.

## 2. What moved it — today's 100 first-parent merges

Subject-line deltas sum to **+491,668 B = 98.4%** of the measured +499,860 B
(the rest is merges whose subject carries no byte figure, e.g. W16-GZ +4,392).
Classified by subject keyword; each lane's own doc has the per-row record.

| class | merges | fns | bytes | share | lead lanes |
|---|---:|---:|---:|---:|---|
| **A. anonymous rows named + drifted bodies repaired** | 12 | 950 | **160,784** | 32.7% | W16-HK 35,432 · HO 33,744 · HN 28,228 · HR 19,088 · HS 17,448 · IB 8,416 · HG 8,132 · PRAC 8,152 |
| **B. alias / ICF fold adjudication on retail bytes** | 7 | 324 | **103,052** | 21.0% | W16-HA 37,904 · GR 22,088 · HC 20,808 · GM 12,836 · GQ 6,220 · GV 3,196 |
| **C. shared templates to retail's shape** | 11 | 241 | **87,840** | 17.9% | W17-OPTR 53,792 · TMPL2 13,092 · OPLNAME 5,760 · HP 4,092 · BSR 3,544 · OWN 3,180 |
| D. 1–90% band breadth sweeps | 4 | 278 | 51,288 | 10.4% | W16-HX 27,344 · HY 12,608 · HE 6,740 · HM 4,596 |
| E. re-homing mis-pinned blocks | 13 | 317 | 42,932 | 8.7% | W17-HCT 7,792 · BPM 5,596 · PIN2 4,916 · HU 4,688 · MVS 4,652 |
| F. single-row ports, retail-byte name repairs, tooling | 53 | 117 | 45,772 | 9.3% | W17-F2 6,160 · SHD 4,644 · F3 3,584 · CLIP 3,308 · F4b 2,596 |

What made each class pay:

- **A.** W16-HD's scorer concluded "most of the anon gap is porting" because
  only 1–15% of anonymous rows had their true identity in our pool. **W16-HG
  showed that was a pool-coverage artifact**: a large share of anonymous rows
  are functions we *already define*, whose bodies had drifted far enough that
  no scorer could pair them. Repair the body → the identity becomes provable →
  name it. HK/HN/HO ran that at scale (0 units regressed); HR/HS added caller
  binding for small rows; IB added retail RTTI vtable slots.
- **B.** Each admission went through `icf_pair_adjudicate.py --chase`
  (CHASED T1 PROVEN) with `masked_equal` Δ 0 on every lane — real pairing,
  not disclosure.
- **C.** Retail RTTI has exactly one ref base (`.?AVObjRef@@`) and **no
  `ObjRefConcrete`**; re-basing `ObjOwnerPtr` (W17-OWN) and `ObjPtr`
  (W17-OPTR) on `ObjRef` made every instantiation retail-shaped at once, and
  fixed a real ring bug (six `mOwner`-release dtor specialisations). Retail
  also has **no `BinStreamRev`** (0 × `.?AVBinStreamRev@@`). The method that
  paid repeatedly: fix the shared template → prove the instantiations
  word-identical → name the anonymous instantiation addresses from each body's
  own type descriptor (OPL → OPLNAME).
- **D.** Rank by `size × (100 − fuzzy)`; HX raised 134 of 172 in-band rows, HY
  111 up / 0 down. ~25 behaviour bugs fixed along the way.

## 3. Where the remaining gap sits

Pairable units only, ruler as above. Full row list:
[next-leverage-queue-2026-09-30.tsv](next-leverage-queue-2026-09-30.tsv)
(7,452 rows; `pool`, `charge_class`, `gap_bytes`, retail→ours name pairs).

| pool | rows | bytes (size if crossed) |
|---|---:|---:|
| ANON_0 — anonymous `fn_` rows at fuzzy 0 | 4,785 | 790,956 |
|   … of which **Quazal NetZ** (`src/network/quazal/*`, `ObjDup`, `Core`) | ~1,010 | ~210,000 |
| NAMED_90_100 | 1,891 | 694,100 |
| NAMED_1_90 | 440 | 187,236 (54,291 gap-weighted) |
| NAMED_0 — retail name our object does not define | 281 | 32,780 |
| anonymous rows at 90–100 — **all 2,246 are `masked_equal` EH funclets** | 2,246 | 92,452 |

**Quazal has no oracle**: rb3-Wii's `src/network/` is 178 files / 9,112 lines
of scaffolds, and our Quazal units are 7-line `namespace Quazal {}` shells — the
same class the ceiling's scaffold cut already removes. Treat it like XDK.

NAMED_90_100 split by charge class (every row diffed with `objdiff-cli diff
--batch`, JSONL; fuzzy agrees with `report.json` on 1,886 / 1,891 rows):

| charge class | rows | bytes |
|---|---:|---:|
| STRUCT (insert / delete / opcode) | 507 | 330,340 |
| **NAME_ONLY** (relocation names only) | 1,042 | 219,992 |
| REG_ONLY (permuter class) | 161 | 75,764 |
| IMM (immediates, ± names/regs) | 147 | 43,876 |
| NAME+REG | 34 | 24,128 |

## 4. Next levers, ranked

1. **ANON_0 outside Quazal — ~580 kB, the largest reachable pool.** Run the
   class-A method (drifted body → repair → name; caller binding; RTTI slot) in
   the directories with the most bytes left: engine `bandobj` 552 rows /
   106 kB, `rndobj` 492 / 98 kB, `char` 383 / 69 kB, `rnddx9` 120 / 25 kB,
   `ui` 189 / 24 kB, `synth` 187 / 20 kB; game `meta_band` 326 / 42 kB,
   `game` 142 / 15 kB, `net_band` 73 / 15 kB. Re-measure after W16-HZ first.
   Also apply the class-C template method to anonymous STL / `obj/`
   instantiations (T from the body's own type descriptor).

2. **Inline `operator delete` block — 94 classes / 105 rows, mechanical.**
   In 105 near-miss rows the *only* charge is retail calling `?MemFree@@YAXPAX@Z`
   where we call the class's out-of-line `??3<Class>@@SAXPAX@Z` (8,088 B in
   name-only rows; the rest carry other charges too). The switch already exists
   — `OBJ_MEM_OVERLOAD_INLINE_DEL` in `utl/MemMgr.h:426` (used for RndMat,
   GemTrackDir, VocalTrackDir). **0 of the 94 classes** are also charged
   where retail calls an out-of-line delete. ⚠ The screen saw only sub-100
   rows; rows at 100 that call `??3<Class>` were not checked, and
   `ObjMacros.h:782` says the direction is per class — so A/B each class (or a
   small batch) on retail bytes rather than bulk-flipping. Classes:
   `grep ' ?MemFree@@YAXPAX@Z->??3' next-leverage-queue-2026-09-30.tsv`.

3. **DC-only directory pin audit — ~51 kB plus accuracy.** Units sourced from
   `src/system/{hamobj,gesture,flow}` hold 81,856 B of pinned retail code, only
   26,256 B matched; 250 anonymous rows / 39,264 B at 0, 11,672 B named < 100.
   RB3-360 has no Kinect/dance code, and W17-HCT (HamCamTransform was
   OutfitConfig's code) and W17-PRAC (230 DC-only classes with no retail RTTI)
   both found real RB3 code under DC3 stand-in names. Largest: `MoveMgr`
   13,236 pinned / 1,824 matched / 56 anon rows; `DepthBuffer3D` 8,468 / 31
   anon. Re-home each block by caller/RTTI/strings.

4. **NAMED_1_90 round 4 — 440 rows / 54 kB gap-weighted.** The sweeps have run
   at ~70–80% raise rates with 0 rows down. Engine `rndobj` 90 rows / 53 kB,
   `bandobj` 58 / 25 kB, `char` 36 / 11 kB; game `meta_band` 45 / 11 kB. Top
   by gap: `CSHA1::Transform` 5,856 @ 55.7, `RndAmbientOcclusion::Tessellate`
   4,796 @ 64.2, `NgSpotlightDrawer::SetupXSection` 2,104 @ 53.4,
   `Spotlight::BuildNGCone` 1,692 @ 66.7, `BandPatchMesh::FindXfm` 1,268 @ 57.0
   (W17-BPM3 has it).

5. **NAME_ONLY near-misses — 1,042 rows / 220 kB.** Largest retail-side
   targets: `vector<Hmx::Object*>::_M_fill_insert` 36 rows / 14,224 B ·
   `vector<ChatReceiver*>::push_back` (group 10) 22 / 8,720 · `hash_map<Symbol,int>`
   ctor 26 / 7,884 · `??3BinStream` 39 / 6,980 · `StlNodeAlloc<_List_node<int>>`
   ctor 31 / 6,328 · `vector<CharPollableSorter::Dep*>::reserve` 17 / 5,968 ·
   `list<Hmx::Object*>::insert` 23 / 4,584. Paid 103 kB today.
   ⚠ **Parked by the coordinator under the 09-30 breadth directive** (W16-HC:
   alias crossings move fuzzy by ~0.0002 pp). Unparking is the user's call; if
   it reopens, every admission still needs CHASED T1 PROVEN.

6. **NAMED_0 — 281 rows / 33 kB**: retail names our objects do not define ⇒
   write the missing bodies (engine `ui` 40 rows, `rndobj` 33, `meta_band` 32).

**Not funded, and why:** STRUCT 330 kB is diffuse, and its biggest rows are
documented walls (`VocalTrack::UpdateScrolling` = codegen-shape wall W16-CW;
`CustomizePanel::Handle` needs two unproven aliases). REG_ONLY 76 kB is
permuter class (deferred by directive). The 92 kB of anonymous rows at 90–100
are `masked_equal` funclet disclosures, not a lever. `auto_*` 1.66 MB and XDK
no-source 2.09 MB lie outside the ceiling.

## 5. Instrument notes

- `objdiff-cli diff --batch` reads symbols on **stdin**; `-f json` emits
  **JSONL** (one record per line, not one JSON document).
- A whole-pool batch run **died with SIGBUS (rc=135) at unit ~200/538** while
  other lanes were building in main — almost certainly an object rewritten
  under a live mmap. Chunks of 150 symbols with one retry all completed.
- Classifier: a charged row is NAME_ONLY when every non-equal instruction is
  `diff_arg` and every differing typed argument is a `Symbol`; this is the
  census behind the W17-TARGET brief's "pure relocation-NAME" class, re-derived.
- Scripts: `~/tmp/gap_census_0930.py`, `~/tmp/classify_n90.py` (not
  committed; the TSV is the artifact).
