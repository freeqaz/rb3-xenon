# W9 L4 — songmgr-family-hashmap-cluster-8255f (BandSongMgr)

**Date:** 2026-06-20  **Mode:** adversarial DISCOVER/PLANNER (read-only in main @8314)
**Verdict:** REAL_ACTIONABLE. The frontier item is the **UNWIRED `band3/meta_band/BandSongMgr.cpp` TU**, confirmed by oracle ground-truth. The hash_map vein is present and the FIX recipe applies (with one simplification — `hash<int>` is already built-in).

## Ground truth established

### The find COMDAT (`FUN_82552cd0` = `lbl_82552CD0`, the int-key hash find)
Decompiled (Ghidra): computes `key % bucket_count` (`(end-begin>>2)-1`), walks a bucket chain comparing `node[1] == key` (key @ node+4), returns node or NULL. **This is `std::hash_map<int,V>::find`, NOT `_Rb_tree::_M_find`.** Callers read value @ `*(node+8)`. Matches the documented int-key find-COMDAT in the hash_map vein notes.

### fn_8255F858 = `BandSongMgr::GetShortNameFromSongID(int, bool)` (oracle BandSongMgr.cpp:206)
```
piVar1 = FUN_82552cd0(this+0xd4, &songID);   // mSongNameLookup.find(songID)
if (*piVar1==0) {
  piVar1 = FUN_82552cd0(this+0x10c, &songID); // mExtraSongIDMap.find(songID)
  if (*piVar1==0) Symbol(param1, PTR_DAT_82c411b0);  // gNullStr fallback
  else *param1 = *(piVar1+8);                 // value @ node+8 (hash_map)
} else *param1 = *(piVar1+8);
```
Exactly the oracle's two `find()`+fallback. **0xd4 = `mSongNameLookup` (std::map<int,Symbol> in Wii oracle @0xc4), 0x10c = `mExtraSongIDMap` (Wii @0xf4).** The Wii→Xbox delta reconciles with 0x1c-sized maps over the larger Xbox SongMgr base: 0xc4→0xd4, 0xd4+0x1c=0xf0 (mSongIDLookup Symbol-key), 0xf0+0x1c=0x10c (mExtraSongIDMap). **Retail compiled these `std::map` members as `std::hash_map`** (the int-hash find proves it).

### fn_8255DF90 = `BandSongMgr::IsInExclusionList(const char*, int)` (oracle BandSongMgr.cpp:568)
Iterates the file-static `exclusionList[]` table (4 × 8-byte `{const char* name; int songID;}`): `{ "danicalifornia", 8 }, { "blackholesun", 3 }, { "hierkommtalex", 1005106 }, { "rockandrollstar", 1005109 }`. Decomp matches byte-for-byte: `if (param_3 == ppuVar4[1]) return 1;` (songID==entry->songID) else strcmp; `uVar3 += 8; if (0x1f < uVar3) return 0;` (4 entries). The string `danicalifornia` lives at 0x8209c190; pointer table `PTR_s_danicalifornia_8209c1b4`. **Unambiguous BandSongMgr fingerprint.**

### Other cluster anchors
- fn_82561530 — AddSongs/AddSongData path: builds the **0x138-byte** record (`FUN_82709ee0(0x138)` = `BandSongMetadata`/`SongData` alloc), calls `FUN_82783aa8` (SongMgr base, pinned 0x82783A00), 0xd4/0x10c lookups, `(**(*piVar6+0x6c))` vcall.
- fn_82561180 — int-key `find` returning value@+8 with a NULL→insert-default fallback (a `map::operator[]`-style accessor).
- fn_8255DF30/DF40/DF50/DF58 — small SongMgr-derived accessors (return const string ptrs `ContentDir`/`ContentPattern`; fn_8255DF58 calls pinned base `Function_82783A00`). **These mark the BandSongMgr TU start (~0x8255DF30).**

## TU identity & wiring
- **`band3/meta_band/BandSongMgr.cpp` is UNWIRED**: not in `config/45410914/objects.json`, not in `config/45410914/splits.txt`. Confirmed unpinned.
- Oracle: `../rb3/src/band3/meta_band/BandSongMgr.{h,cpp}` (116 + 999 LOC, ~57 named methods) and `../rb3/src/band3/meta_band/BandSongMetadata.h` (the 0x138 record). rb3-Wii is the authoritative game-code oracle.
- Base `SongMgr` (oracle `../rb3/src/system/meta/SongMgr.h`) is the pinned base at **0x82783A00** (`FUN_82783aa8`). `extern BandSongMgr &TheSongMgr;`

## Falsifications performed
- **"NOT BandSongMgr (beta 0x82631298)" — the LEAD'S LABEL IS WRONG.** 0x82631298 decompiles to a *small* function using a `std::map<Symbol,int>` `_Rb_tree` find (`FUN_822e58a8`) on `this+0x4` + an int-hash find on `this+0x1c` — a **different, smaller song-mgr layout** (FakeSongMgr-like or a beta/secondary instance), NOT BandSongMgr and NOT the 0x8255F cluster. The real BandSongMgr is the 0x8255F cluster. (Does not change the verdict — strengthens it.)
- **hash<int> "unique #define guard" is UNNECESSARY.** `hash<int>` is already built into STLport (`src/system/stlport/stl/_hash_fun.h:78`). Only `hash<Symbol>` needs a custom spec (already in AccomplishmentProgress.h). BandSongMgr's converted maps are **int-keyed** → no new hash spec, no guard. De-risks the port.
- **hash_map infra already proven in-tree:** `AccomplishmentProgress.h` already uses `std::hash_map<int,int>` (mGigTypeCompletedMap @0x62c) + `std::hash_map<Symbol,int>` — the vein landed there; same STLport `<hash_map>`.

## Bounding (attribution risk — verify empirically during port)
- The gap is one big unpinned span: ContextChecker ends **0x82558EAC** → AccomplishmentProgress starts **0x82577680**. It contains MULTIPLE small TUs.
- 0x82558EE0–~0x8255DE88: dense ~0x50–0x60-byte fns + a **float-comparator** (fn_8255DE88 does double compares via singleton FUN_82803f38) = **SongSort family** (SongSortByX comparators), NOT BandSongMgr.
- BandSongMgr body: **~0x8255DF30 → ~0x825632A8/0x82563400**. fn_82563500 is a *different* class ctor (sets vtable `&PTR_Function_82563C28_8209d07c`, virtual-base layout) = next TU start. The exact lower bound (where SongSort ends / BandSongMgr begins) and the tail (0x825632A8–0x82563500 ctor/dtor/vtable thunks) must be pinned by objdiff feedback (foreign fn_@0% runs reveal mis-attribution). The lead's [0x8255F000, 0x82563400) is a reasonable *dense-body* approximation but UNDERSHOOTS the start (~0x8255DF30) — pin the full BandSongMgr span, not the partial.
- **fn_82563038 caveat:** operates on a DIFFERENT singleton (`FUN_82803f30`, reads +0x54/+0x68/+0x74/+0x8c) — likely **SongStatusMgr/SongSortMgr**, NOT BandSongMgr's `this+0xd4/0x10c`. Treat as a separate TU; do not assume it's in the BandSongMgr pin.

## FIX recipe (per the hash_map vein, self-contained)
1. Scaffold `src/band3/meta_band/BandSongMgr.cpp` + `.h` from rb3-Wii oracle (MWCC→MSVC X360 port).
2. In the ported header: change `mSongNameLookup`, `mExtraSongIDMap` (and probably `mSongIDLookup` Symbol-key) from `std::map<...>` to `std::hash_map<...>` (`#include <hash_map>`). `hash<int>` is built-in; `hash<Symbol>` spec already exists (reuse AccomplishmentProgress.h's, or a shared guarded spec). Returns + iterators in the .cpp follow the member type.
3. Drop the 0x1c-gate "dead pad" concern — the hashtable's `float _M_max_load_factor` occupies that slot.
4. Add to `objects.json` as `NonMatching`; pin the BandSongMgr `.text` span in `splits.txt` (full span ~0x8255DF30→tail; let dtk back-fill `.pdata`).
5. `tools/gen_game_target_map.py` → add `target_symbol_map.json` entries so byte-exact find-using fns pair (GetShortNameFromSongID, IsInExclusionList, accessors, ctor/dtor, vtable thunks).
6. Whole-binary A/B vs main@8314 (VERIFY COMMAND). Honesty gate: keep only if net≥+1, no ≥8-contig foreign fn_@0% run in the pinned range.

## Expected first-wave delta
~57 oracle methods; cluster ~120 fns (incl. thunks/iterators/COMDATs). Trivial accessors + ctor/dtor/vtable thunks + the 2 confirmed (GetShortNameFromSongID, IsInExclusionList) land first. Realistic **+10–20** for a first self-contained landing; deeper methods (AddSongData/AddSongs/GetValidSongs/save-load) are follow-ups.

## Discovered adjacent frontier (seeds for later layers)
1. **SongSort family** (0x82558EE0–~0x8255DE88, between ContextChecker and BandSongMgr): float-comparator fn_8255DE88 + dense small fns. Oracle: `../rb3/src/band3/meta_band/SongSort*.cpp` (SongSortMgr + 9 SongSortByX). All UNWIRED. Likely several easy comparator matches.
2. **SongStatusMgr/SongSortMgr singleton TU** owning fn_82563038's object (`FUN_82803f30`, members +0x54/+0x68/+0x74/+0x8c with int-hash finds) — another hash_map-vein owner; oracle `SongStatusMgr.cpp`/`SongSortMgr.cpp`.
3. **0x82631298 small song-mgr** (`std::map<Symbol,int>` find @+4 + int-hash find @+0x1c): identify (FakeSongMgr? `../rb3` / `../dc3-decomp` `FakeSongMgr.cpp`, already wired NonMatching) — possibly a different beta/secondary SongMgr instance; the find @+0x1c is another int-hash owner.
4. **BandSongMetadata.cpp** (the 0x138-byte record built by fn_82561530) — oracle `../rb3/src/band3/meta_band/BandSongMetadata.{h,cpp}`, UNWIRED; pairs with BandSongMgr.
