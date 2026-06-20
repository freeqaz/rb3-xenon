# W10 — deferred-ports: re-derive 4 wave-9 game-port TUs onto main@9037

**Date:** 2026-06-20
**Mode:** DISCOVER/PLANNER (Opus), READ-ONLY in main.
**Baseline:** main @ d910dd9, **9037 / 65543 matched** (fixed for all agents).
**Area:** the 4 game-port TUs wave-9 *deferred* (cascading conflicts on its old
pre-keystone base). Re-derived FRESH onto main@9037 here.
**Verdict:** **ALL 4 STILL HAVE VALUE on 9037** — emit all 4 as self-contained
work-items. No foundational lever found (the AppMini UILabel decl + the
RB3_HASH_SYMBOL guard are established, layout-neutral patterns, not keystones).

---

## Why these were deferred and why they re-derive (not cherry-pick)

The 4 branches all diverged from a **pre-keystone base** (812e1df / b138024b,
8314 matched) and carry the entire wave-9 churn as stale diff. **The branches
must NOT be merged/cherry-picked wholesale.** Concrete proof of corruption if
merged:

- `w9-songstatusmgr-...`'s diff to `BandSongMgr.h` **REVERTS** BandSongMgr's
  hash_map members back to `std::map` and renames a vtable slot
  (`WriteCachedMetadataToStream` → `WriteCachedMetadataFromStream`) — that is the
  old base where BandSongMgr.cpp was being *deleted*. On main@9037 BandSongMgr
  already has the hash_map layout landed; the branch diff would break it.
- The branches also delete `StorePackedMetadata.h` (96-line version, now feeding
  StoreMainPanel) and touch `Object.h` / `ObjMacros.h` / `UIComponent.h` /
  `target_symbol_map.json` (5830–25150-line re-serialization) — ALL stale.

**Re-derivation rule (every work-item):** extract ONLY the TU-specific files
(the `.cpp`, its own `.h`, the new support headers it adds, the splits stanza,
the objects.json wire line, and ONLY the in-range map keys) and apply them
fresh. Discard every shared-header / BandSongMgr / map-wide change.

The keystone's reveal cascade did **NOT** pre-claim any of these 4 spans —
verified: 0 already-keyed VAs in the SongStatusMgr/StoreMenuPanel spans, 1 ICF
STL COMDAT each in LicenseMgr/AppMini (already counted, must NOT re-key). So the
wave-9 deltas hold on 9037.

---

## Ground-truth (auto_03_82260000_text.obj retail symbol-by-VA + main splits.txt)

Region map at the 4 spans (current main pins, sorted):

```
0x825B67B0–0x825B6804  (lower neighbour of SongStatusMgr)
0x825B8670–0x825B86A0  MoggClip.cpp  ← DEAD ORPHAN sliver (no obj), EVICT
0x825BB5B8–...         (upper neighbour of SongStatusMgr)
...
0x8261E020–0x8261FE68  StoreMainPanel.cpp   (lower neighbour of StoreMenuPanel)
0x82626F80–0x82627200  NameGenerator.cpp    (upper neighbour of StoreMenuPanel)
...
0x82630988–0x82632040  SongUpgradeMgr.cpp   ← NEW on 9037; bounds AppMini top + LicenseMgr bottom
0x82632F00–0x826340F0  Instarank.cpp        (upper neighbour of LicenseMgr)
```

### Pin-collision corrections forced by main@9037 (the wave-9 lesson #3 trap)

1. **AppMini text END must drop 0x82630A08 → 0x82630988.** The branch span
   `[0x8262F530, 0x82630A08)` overlaps SongUpgradeMgr (now pinned `[0x82630988,
   0x82632040)`). 0x82630988 = `fn_82630988` = SongUpgradeMgr's first fn (COFF
   confirms: `fn_8263095C, fn_82630988, fn_82630A08`). Re-bound AppMini to
   `[0x8262F530, 0x82630988)`.
2. **AppMini pdata END must drop 0x82224680 → 0x82224678** (or just let dtk
   auto-derive). Branch pdata `[0x82224548, 0x82224680)` overlaps SongUpgradeMgr
   pdata `[0x82224678, 0x82224818)` by 8 bytes. Bounding the text to 0x82630988
   makes dtk derive pdata ending at 0x82224678.
3. SongStatusMgr / LicenseMgr / StoreMenuPanel pins are **collision-free** on
   9037 (verified against both neighbours, text + pdata). SongStatusMgr's only
   in-range conflict is the MoggClip sliver it explicitly evicts (text
   0x825B8670 + pdata 0x8221C4B0, both inside SongStatusMgr's ranges).

---

## Per-TU summary (details in work-items)

| TU | text span (corrected) | pdata | wire? | expected | risk |
|---|---|---|---|---|---|
| SongStatusMgr | 0x825B8058–0x825BA440 | 0x8221C470–0x8221C690 | new + evict MoggClip | ~+45 | 11 includers (method-only, low) |
| LicenseMgr | 0x82632040–0x82632F00 | 0x82224818–0x82224938 | already wired (stub→real) | ~+26 | pin |
| AppMini | 0x8262F530–**0x82630988** | auto (–0x82224678) | new | ~+18 | pin + UILabel decl |
| StoreMenuPanel | 0x8261FF18–0x826211E8 | 0x82223240–0x82223380 | new | +8 | pin |

Total honest ceiling ≈ **+97** across the 4, independently landable.

---

## SongStatusMgr (~+45) — the include-graph caveat

`SongStatusMgr.h` is included by **11 wired TUs** (AccomplishmentManager,
MusicLibrary, SongRecord, RockCentral, AccomplishmentSong/Disc/Player/List/Filter
Conditional, Utl). VERIFIED SAFE TO RE-LAYOUT:
- All 11 use SongStatusMgr **only through method calls** via
  `BandProfile::GetSongStatusMgr()` (returns `SongStatusMgr*`, out-of-line) — no
  raw `SongStatusMgr` member-offset access.
- The **`SongStatus` per-song struct layout is BYTE-IDENTICAL** between main and
  the branch (0x8 mSongID … 0xa0 mSongData[11][4]) — its accessors inline
  identically.
- `SongStatusCacheMgr` / `SongStatusLookup` / `mLookups` (the Wii model being
  deleted) are referenced **only inside SongStatusMgr.h** — nothing else.
Residual risk: if the keystone newly inlined a SongStatusMgr *accessor* (now
hash_map-backed) into one of the 11, that includer's codegen changes — so the
land MUST whole-binary A/B and confirm those 11 hold. attribution_risk=true.

The hash_map<int,SongStatus*>@0x38 layout is COFF-proven (dossier L5: all 43
int-find call sites in-range decode `addi rX,r3,0x38; bl lbl_82552CD0`).

The +45 = +34 (base port + MoggClip evict + reveal) + +10 (15000 star-cap fix at
GetTotalStars/GetTotalSongs, NOT the Wii 5000) + +1 (fn_825B8670 =
GetPossibleStars reveal, which is the *evicted* MoggClip address, byte-exact once
the star-cap + eviction are in).

---

## Bottom line

Emit all 4 work-items. The dominant correctness gates: (a) extract TU-only files,
discard branch's stale shared-header/BandSongMgr/map churn; (b) re-bound AppMini
to 0x82630988 (text) / auto-pdata; (c) SongStatusMgr whole-binary A/B must hold
its 11 includers; (d) never re-key the ICF STL COMDATs already in main's map
(0x82630880 CharMeshHide::Init, 0x826321B0 _Copy_Construct); (e) StorePackedMetadata.h
is ADDITIVE on 9037 (the 96-line StoreMainPanel version exists — add LoadPage +
StorePage/StorePackedPage, do not replace).
