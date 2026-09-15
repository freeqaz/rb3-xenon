# W16-AZ — PhraseAnalyzer's two holes, SetTheTempoMap re-home, the ClipDistMap block, and a T1 fold at 0x826cb590

**Lane:** W16-AZ · **Date:** 2026-09-15 · **Branch:** `w16-az`, dispatch tip `9f7c571a0204`, baseline parent `8ecff1e77fdf`
**Brief:** `~/tmp/brief_w16az.md` · **Scope:** `config/45410914/splits.txt` (own headings only), `scripts/target_symbol_map.json`, `scripts/symbol_aliases.json`. **No `src/` edit** — every one of the four items adjudicated to a splits / map / alias defect, never a source one.
**Ruler:** `name_check`, objdiff 4.2.9 `5a51cd51fe0a353f`, `report.json` provenance read on every build. Every delta below is a **set-diff of the `fuzzy == 100` row set** (`tools/rowset_snapshot.py`, keys `unit::name`), not a headline subtraction.

---

## 0. Headline

| item | predicted | measured | commit |
|---|---|---|---|
| 1 — PhraseAnalyzer's two holes (`0x8278B7F0–B838`, `0x8278BB98–BD68`) | +2 fns / +484 B | **+3 fns / +620 B** (4 in, 1 out) — missed by one caller row | `547fe0ae` |
| 2 — `0x827d2500 SetTheTempoMap` SongInfoCopy → TempoMap | +1 / +12 B | **+1 / +12 B**, exact | `e9b3ad2c` |
| 3 — `0x822abd60` + the ClipDistMap block → OutfitConfig | +1 / +112 B, "possibly +40 B" | **+2 / +152 B** (112 + the 40 B funclet) | `3e87a7cd` |
| 4 — `0x826cb590` `push_back` fold (AddInfo's one `diff_arg`) | +1 / +100 B | **+1 / +100 B**, exact | `acb08c89` |

**Lane total: 43,489 / 4,033,120 B → 43,496 / 4,034,004 B = **+7 functions / +884 B** (+0.00863 pp = 884 / 10,246,004). 8 rows crossed in, 1 row fell out — and that one is the 44 B funclet `fn_8278BD3C` re-attributed from `FileChecksum` to `PhraseAnalyzer` in Item 1, not a lost match: no row that was matched at dispatch is unmatched now.** `total_code` 10,246,004 unchanged throughout (no pin added or removed, only moved).

Baseline provenance: main `8ecff1e77fdf`, `report.json` measures 43,489 / 4,033,120 / 39.3629 %, snapshot `~/tmp/rows_w16az_base.json` (40,827 rows at fuzzy==100) taken after two settle builds (builds 0–1, rc=0, zero work on the second).

---

## 1. Item 1 — PhraseAnalyzer's two holes (`547fe0ae`)

**Briefed figure tested literally first.** AY reported PhraseAnalyzer "not pinned". False: `system/beatmatch/PhraseAnalyzer.cpp:` was pinned with **17** `.text` blocks at dispatch (20 after this item). What AY saw was two *gaps* inside its span that neighbouring headings had swallowed: `0x8278B7F0–B838` sat under `HamMove.cpp:` and `0x8278BB98–BD3C` + `0x8278BD3C–BD68` under `FileChecksum.cpp:`. Retail bodies there are `vector<PhraseData>` machinery (`__uninitialized_copy`, `_M_insert_overflow_aux`, a funclet) that only `PhraseAnalyzer.obj` defines.

**Change:** three `.text` lines moved into `PhraseAnalyzer.cpp:`; map rows `0x8278b7f0` → `??$__uninitialized_copy@PAVPhraseData@PhraseAnalyzer@@…` and `0x8278bb98` → `?_M_insert_overflow_aux@?$vector@VPhraseData@PhraseAnalyzer@@…`; alias group `0x8278b7f0` reoriented (survivor := PhraseData spelling, `folded: []`, former survivor under `withdrawn` class `SURVIVOR_SIZE_MISMATCH`). Validator PASS, 0 contradicted.

**Build-2 split-guard note.** Moving `.text` blocks whose bodies carry unwind records makes dtk re-derive their `.pdata` lines (`0x8223EE98–EEA0`, `EEA0–EEA8` into PhraseAnalyzer), which trips `[split-guard] THE SPLIT REWROTE ITS OWN INPUT` once — **rc=1, `report.json` STALE**. Build 3 (retry, no further edit) was the fixed point. ⚠ The harness notification for build 2 said "exit code 0" — that is the shell wrapper's; the `BUILD rc=1` line in the log is the truth. Item 2's 12 B leaf has no unwind record, so no `.pdata` moved and no guard trip (build 4 clean first time).

**Predicted vs measured (set-diff base → after1):**

| row | size | |
|---|---|---|
| `PhraseAnalyzer::??$__uninitialized_copy@PAVPhraseData…` | 72 B | IN, predicted |
| `PhraseAnalyzer::?_M_insert_overflow_aux@?$vector@VPhraseData…` | 412 B | IN, predicted |
| `PhraseAnalyzer::fn_8278BD3C` (funclet) | 44 B | IN (re-attributed) |
| `FileChecksum::fn_8278BD3C` | 44 B | OUT (same funclet, old home) |
| `PhraseAnalyzer::?push_back@?$vector@VPhraseData…` | 136 B | **IN, NOT predicted** |

Net **+3 fns / +620 B** vs predicted +2 / +484. The miss is the `push_back<PhraseData>` caller: its only charged site was the `bl` to `0x8278bb98`, which had been named with a spelling our obj cannot define; naming it correctly forgave the caller too. **Lesson: price a rename by its callers, not just the renamed rows** — a wrong existing name is financed by its callers (CLAUDE.md map/name economics (a)), and repairing it collects from them.

**Alias mis-orientation pattern (seen twice this lane, §3 again):** a group whose *survivor* spelling has 0 definers and 0 referencers on our side cannot have been the body T1 was measured against — T1 compares retail bytes to *our compiled COMDAT*, so the witness was necessarily the `folded` spelling. Reorient, don't prune.

---

## 2. Item 2 — `0x827d2500 SetTheTempoMap` (`e9b3ad2c`)

Briefed: the 12 B leaf `?SetTheTempoMap@@YAXPAVTempoMap@@@Z` sits inside `SongInfoCopy.cpp`'s third block (`0x827D1EF0–2510`) while only `TempoMap.obj` defines it. Verified on COFF (build first; 1,215 objs) and on `splits.txt`. Change: `SongInfoCopy.cpp` block end `0x827D2510 → 0x827D2500`, `TempoMap.cpp` block start `0x827D2510 → 0x827D2500`. Predicted +1 / +12 B, FELL OUT 0; **measured +1 / +12 B** (`default/TempoMap::?SetTheTempoMap@@YAXPAVTempoMap@@@Z`), FELL OUT 0. Not a boundary-drain: SongInfoCopy keeps its block.

---

## 3. Item 3 — `0x822abd60` and the ClipDistMap block (`3e87a7cd`)

### 3.1 Adjudication

| question | answer | evidence |
|---|---|---|
| Is `0x822abd60` inside ClipDistMap's span or a hole? | **Inside** `ClipDistMap.cpp:` first block `.text 0x822ABCE0–0x822ABE70` | `splits.txt` at dispatch |
| Provenance of that block | **CJ-1 `2e7e0b10`** extended an earlier `–0x822ABD5C` pin to `–0x822ABE70` (a blind gap-fill); the map row came from the **TU5 regen `a320bc12`** | `git log -S` on the line / the row |
| Which of OUR objs defines `vector<BandPatchMesh>::_M_fill_insert`? | `OutfitConfig.obj` **and** `Gem.obj` (both 112 B == retail); `ClipDistMap.obj` defines nothing BandPatchMesh-related | COFF symbol tables, post-build |
| Which unit owns the block? | **`OutfitConfig.cpp`** — its own blocks flank it on both sides (`0x822ABA10–A1C` before, `0x822ABE70–BFB8` after); the block's other bodies are OutfitConfig-shaped (`fn_822ABDD8` calls `OutfitConfig::StaticClassName` + `MemAlloc(252)`; `fn_822ABCE0` is a 124 B `vector<T>::resize` over 16 B elements; `fn_822ABE48` is a funclet). Gem merely includes the header and lives elsewhere. | `ClipDistMap.s` keyed on `.fn`, map |
| Is the existing alias group at `0x822abd60` sound? | **Mis-oriented.** Survivor spelling (`ObjPtrVec<RndDrawable>::Node _M_fill_insert`) has **0 definers / 0 referencers** in 1,215 objs; all 12 `ObjPtrVec<X>::Node _M_fill_insert` instantiations we hold are **108 B** vs retail 112 B | COFF census |
| T1 re-measured | retail `fn_822ABD60` vs our OutfitConfig.obj BandPatchMesh body: **28/28 words equal, 23/28 fully unmasked**, bl displacements masked | inline masked compare |

The brief's rule applied: *"if retail places it where a unit whose obj defines that spelling can own it, re-home + rename in one commit"* — it does, so one commit: `.text 0x822ABCE0–0x822ABE70` moved `ClipDistMap.cpp:` → `OutfitConfig.cpp:` (address order); map `0x822abd60` → `?_M_fill_insert@?$vector@VBandPatchMesh@@…IABV3@@Z`; alias group reoriented with the ObjPtrVec spelling under `withdrawn` (`SURVIVOR_SIZE_MISMATCH`, lane `W16-AZ 2026-09-15`). Nothing pruned. Validator PASS: 1399 map-consistent / 247 tolerated / 1 exempt / **0 contradicted** / 1647 total.

### 3.2 Predicted vs measured (set-diff after2 → after3)

Build 5 rc=1 (split-guard: `.pdata 0x821F52A0–52C0` re-derived alongside the moved `.text`), build 6 rc=0 fixed point, 0 guard lines.

| row | size | |
|---|---|---|
| `default/OutfitConfig::?_M_fill_insert@?$vector@VBandPatchMesh…` | 112 B | IN, predicted |
| `default/OutfitConfig::fn_822ABE48` | 40 B | IN — flagged "possibly +40 B if the funclet re-pairs"; it did |

**+2 fns / +152 B, FELL OUT 0.** `default/ClipDistMap` 4/16 → 4/12 (5 blocks remain, not drained). `default/OutfitConfig` 178/239.

### 3.3 Left anonymous, with the evidence that would change it

- **`fn_822ABCE0` (124 B resize): two-way TIE.** `resize<OldColorOption>` and `resize<Overlay@OutfitConfig>` both read **31/31** against retail; `resize<Piece@Piercing>` fails 3 immediates (shift count ⇒ element size); `resize<BandPatchMesh>` is 128 B. The tied pair differ only in masked `bl` targets, and the retail callees are themselves fold survivors (`0x822aacd8` named `_M_erase<OldColorOption>`, `0x822ab0b0` named `_M_fill_insert<TransformCrowd>`). Naming a NEW address is a bet with zero call-site upside; a 50/50 bet is not taken. **Would change it:** retail-byte identification of `0x822ab0b0`'s true family, which would break the tie by relocation.
- **`fn_822ABDD8` (112 B): OutfitConfig `New`-shaped** (`StaticClassName` + `MemAlloc(0xfc)` + a ctor `bl` into HamCamTransform's block at `0x822AB3E0`), but our `?NewObject@OutfitConfig@@SAPAVObject@Hmx@@XZ` is 148 B. **Would change it:** an `OutfitConfig` allocation helper of exactly 112 B in our objs, or a retail-byte match against another `New`-family body.

### 3.4 PROPOSAL (not my heading — no edit made): `HamCamTransform.cpp`'s `0x822AB0B0–380` and `0x822AB3D8–838`

Retail `fn_822AB128` (328 B) is **82/82 words equal** to the leading 328 B of our `_M_insert_overflow_aux<BandPatchMesh>` (OutfitConfig.obj, 396 B COMDAT span — the trailing 68 B are almost certainly this function's EH funclets billed into the span, the one-sided reader artifact CLAUDE.md records). `fn_822AB3E0` is the ctor `fn_822ABDD8` calls after `MemAlloc(252)`. Both blocks sit between OutfitConfig's `0x822ABA10` and Gem's `0x822AAA08–B0B0` — i.e. more OutfitConfig / BandPatchMesh territory pinned under `HamCamTransform.cpp` (whose `.s` prints these bodies with synthetic `0x82276xxx` addresses — key on `.fn`, never the column). Suggested owner action: masked-compare every body in those two blocks against OutfitConfig.obj's remaining BandPatchMesh family (`_M_insert_overflow` 48 B, `_M_fill_insert_aux` 440 B, `resize<vector<BandPatchMesh>>` 128 B, `resize<ObjVector<BandPatchMesh>>` 116 B, `??0OutfitConfig@@QAA@XZ` 1016 B), then re-home + rename the proven ones. Also note the map names `0x822ab0b0` `_M_fill_insert<TransformCrowd>` — a spelling worth a definer check before anyone relies on it.

---

## 4. Item 4 — `0x826cb590` `push_back` fold (`acb08c89`)

**Briefed:** "other PhraseAnalyzer sub-100 rows explained by halfword-vs-word / fold spelling". Two named sub-100 rows remained in `default/system/beatmatch/PhraseAnalyzer` after Item 1: `?AddInfo@…` (100 B, fuzzy 99.8) and `??0?$vector@URawPhrase…` (120 B, 99.83). Both have **zero instruction-byte differences** and exactly one `diff_arg` each — the relocation-name class, so a source edit cannot move either; only a map/alias fact can.

**AddInfo's single charge:** `bl ?push_back@?$vector@VSongSection@@…` (retail, `0x826cb590`) vs our `?push_back@?$vector@URawPhrase@@…`. Source check first: PhraseAnalyzer holds a `vector<RawPhrase>` and calls `push_back` on it — our spelling is correct. The map names `0x826cb590` after SongLayout's caller (its `.text` block `0x826CB590–0x826CB610`), so this is a fold, if the two instantiations really are byte-identical. No existing alias group mentioned either spelling (checked with a tightened filter after a loose `'URawPhrase' in name` filter false-hit the giant `_Vector_base` ctor group at `0x826b8b28`).

**T1, measured by hand** (masked word compare mirroring `tools/icf_alias_build.py`; COFF reloc types 3/6 mask `0x03fffffc`, 5/7 mask `0xfffc`, `bl`/`b` displacements masked unconditionally on the retail side):

| retail `fn_826CB590` (128 B) vs | words equal | fully unmasked |
|---|---|---|
| our `push_back<RawPhrase>` (PhraseAnalyzer.obj, 128 B) | **32/32** | 27/32 |
| our `push_back<SongSection>` (SongLayout.obj, 128 B) — control | **32/32** | 27/32 |

Both COMDATs have identical relocation shape (`memcpy` at +48, `_M_insert_overflow_aux<T>` at +104), both element types are 16 B, both 128 B. Retail `fn_826CB590` calls `memcpy` (`0x8282a900`) and `fn_82787ED0`. ⇒ a genuine fold; the group orientation is survivor = SongSection (map-resident at the address), folded = RawPhrase.

**Change:** one group appended to `scripts/symbol_aliases.json` (1647 → 1648; +9 lines, no reformat; evidence string records the compare). Nothing withdrawn, nothing pruned. `touch config.yml`, build 7 rc=0, **0 split-guard lines** (alias edit only — no `.text` moved, as predicted).

**Predicted (on record before build 7):** +1 fn / +100 B, CROSSED IN `PhraseAnalyzer::?AddInfo@…`, FELL OUT 0; `default/SongLayout::push_back<SongSection>` unchanged at 99.84 (its charge is the *callee* name at `0x82787ed0`, untouched).
**Measured (set-diff after3 → after4):** CROSSED IN 1 row / 100 B = exactly that row; FELL OUT 0; 43,495 / 4,033,904 → **43,496 / 4,034,004 B** (39.37051 → 39.371483 %); SongLayout's row still **99.84375**. Exact. Validator PASS: 1400 map-consistent / 247 tolerated / 1 exempt / 0 contradicted / 1648.

**The other row — hand-off, not touched:** `??0?$vector@URawPhrase…` (120 B) is charged on `bl ??$?0H@?$StlNodeAlloc@V?$_List_node@H@…` vs our `?get_allocator@?$vector@URawPhrase…` — the `StlNodeAlloc<_List_node<int>>` ctor `blr` fold, which the concurrency bars assign to W16-BA's alias group. See §6.

---

## 5. Gates (in order, in the worktree, after the final build)

All run in `/home/free/tmp/wt-w16-az` on the build-7 tree (`BUILD rc=0`, read from the log line, not the wrapper's exit code):

| gate | result |
|---|---|
| `./tools/ninja-locked` (build 7) | `BUILD rc=0`, 0 split-guard lines |
| `python3 scripts/verify_ruler_agreement.py --check` | rc=0 — `OK: both objdiff-cli entry points resolve the same ruler.` |
| `python3 scripts/verify_objs_patched.py --verify-manifest` | rc=0 — `[patch-state] OK: 1215 decomp, 3114 target objects match 2026-09-15T01:00:13Z (tree_sha256=5f908626a7dafbfe)` |
| `python3 tools/icf_alias_finder.py --validate` | rc=0 — `VALIDATE: PASS -- 1400 map-consistent, 247 tolerated (enumerated above), 0 contradicted, 1648 total` |
| `tools/native_build_gate.sh` (run 1, for this line; re-run as the lane's LAST action, line repeated in the final message) | `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0` (log `~/tmp/w16az_native_gate_1.log`) |

No `src/` file was edited in this lane; the native gate is run because the rule is "the gate run must be the lane's last action", not because a source change could have reached the native link.

---

## 6. Side observations

- **dtk 4 B gaps:** `PhraseAnalyzer.s` renders `fn_8278B738` as starting at `0x8278B734`, and `TempoMap.s`/`SongInfoCopy.s` render `fn_827D2528` at `0x827D2524` — each is a 4 B gap between a block end and the next function's true start, absorbed into the following symbol. Cosmetic in `.s`, but any tool that trusts `.fn` start addresses from those files is 4 B off there.
- **`.s` address column is synthetic for multi-block units** (seen live: `HamCamTransform.s` prints `fn_822AB128`'s body at `0x82276xxx`; `SongLayout.s` prints `fn_826CB590` at `0x822A9Exx`). Every retail-byte compare in this lane keyed on `.fn fn_<addr>`.
- **`0x82787ed0`** (retail-named `_M_insert_overflow_aux<pair<int,int>>`) is the `bl` target of `fn_826CB590`, and is therefore the one `diff_arg` holding `default/SongLayout::push_back<SongSection>` (128 B) at 99.84. No alias group exists there. Not this lane's heading; a T1 compare of retail `fn_82787ED0` against our `_M_insert_overflow_aux<SongSection>` (SongLayout.obj) would settle it — note `pair<int,int>` is 8 B while SongSection is 16 B, so the retail *name* there deserves the same definer/size check that killed the two survivors above.
- **`default/system/beatmatch/PhraseAnalyzer::??0?$vector@URawPhrase@@…` (120 B, 99.83):** its one `diff_arg` is `bl ??$?0H@?$StlNodeAlloc@V?$_List_node@H@…` (retail) vs our `get_allocator<RawPhrase>` — the **`StlNodeAlloc<_List_node<int>>` ctor `blr` fold, which is W16-BA's alias group by the concurrency bars**. Hand-off: our `?get_allocator@?$vector@URawPhrase@@…` spelling is a candidate membership there.
- **`push_back<PhraseData>` crossing in Item 1** is the cleanest demonstration this lane has of "a wrong map name is financed by its callers": fixing the callee's name paid +136 B on a row nobody edited.

---

## 7. NOT done, and why

| item | reason |
|---|---|
| Name `fn_822ABCE0`, `fn_822ABDD8` | §3.3 — tie / size mismatch; a NEW name is a bet with no call-site upside |
| Re-home HamCamTransform's `0x822AB0B0–380`, `0x822AB3D8–838` | not my heading — §3.4 proposal |
| Alias membership for `get_allocator<RawPhrase>` | belongs to W16-BA's `StlNodeAlloc<_List_node<int>>` ctor group — hand-off in §6 |
| `0x82787ed0` (SongLayout's 99.84 row) | not my heading; evidence path recorded in §6 |
| PhraseAnalyzer's six anonymous `fn_` rows (`fn_8278B3D8` 92 B, `fn_8278B678` 88 B, `fn_8278C160` 408 B, `fn_8278C2F8` 76 B, `fn_8278B638` 52 B, `fn_8278B66C` 8 B) | outside the brief's Item 4 scope (halfword/fold spelling on *named* rows); not investigated |
| A `none`-ruler control | per brief: `matched_functions` is not ruler-invariant on 4.2.9 — a `none` leg would measure the ruler, not the change |

---

## 8. Builds and artifacts

| build | log | rc | purpose |
|---|---|---|---|
| 0, 1 | `~/tmp/rb3_build_w16az_0.log`, `_1.log` | 0, 0 | settle to zero work; baseline snapshot |
| 2 | `_2.log` | **1** | Item 1 first pass — split-guard (`.pdata` re-derive) |
| 3 | `_3.log` | 0 | Item 1 fixed point → `after1` |
| 4 | `_4.log` | 0 | Item 2 → `after2` |
| 5 | `_5.log` | **1** | Item 3 first pass — split-guard |
| 6 | `_6.log` | 0 | Item 3 fixed point → `after3` |
| 7 | `_7.log` | 0 | Item 4 → `after4` |

Snapshots: `~/tmp/rows_w16az_{base,after1,after2,after3,after4}.json`. Validator logs: `~/tmp/w16az_validate_{1,2,3}.log`.
