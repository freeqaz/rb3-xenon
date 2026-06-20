# W9 L2 — cluster-alpha @0x825B8738 OWNER ID: **SongStatusMgr** (CONFIRMED)

**Date:** 2026-06-20
**Mode:** ADVERSARIAL DISCOVER/PLANNER (Opus, layer 2), read-only in main @ 812e1df (8314 matched).
**Frontier:** `w9-cluster-alpha-825b8-owner-id` (kind=scout, est +18).
**Verdict:** **REAL_ACTIONABLE.** Owner identified beyond doubt via COFF + Ghidra +
DC3 Rosetta. The int-key `hash_map@this+0x38` cluster-alpha is **`SongStatusMgr`**
(band3/meta_band) — a per-song score/stars/status manager. DC3 has the byte-identical
twin already **Matching** (`lazer/meta_ham/SongStatusMgr.cpp`), the strongest possible
oracle. Caveat: this is a port-then-convert that also EVICTS a mis-pinned MoggClip
sliver; medium risk (RB3 `SongStatus` struct contents differ from both oracles).

---

## Owner identification — ground truth chain

All from `auto_03_82260000_text.obj` (authoritative retail COFF) + Ghidra MCP (8002).

### Structural fingerprint (COFF + Ghidra decompile)
- **23 accessor methods** 0x825B8738–0x825B9ED0, each: `addi rX,r3,0x38; bl lbl_82552CD0`
  (int-key find COMDAT), NULL-miss → 0, value `*(node+8)` (the hash_map slist tell;
  std::map would be node+0x14). Confirms `this+0x38 = hash_map<int, SongStatus*>`.
- `this+0x34` = `SongMgr*` (calls `fn_82783CD8` = `BandSongMgr::HasSong`, virtual
  slot +0x40 = `Data(int)`, +0x5c = name lookup). `this+0x3c` = the map/record list.
- fn_825B8828 (`GetHighScore`): value->`mHighScores[(ty+0x12)<<2]` ⇒ array @value+0x48.
- fn_825B95B8 (`GetSongReview`): value->byte@+0xe = `mReview`.
- fn_825B8918 (`GetBestStars`): `for(i=diff;i<4;i++) max(GetStars(...))` — verbatim.
- fn_825B8FB0 / fn_825B9C68 (`CalculateTotalScore` / min-LastPlayed select):
  iterate all songs, `mSongMgr->HasSong`, `dynamic_cast<BandSongMetadata*>(Data())`
  (RTTI `.?AVBandSongMetadata` typedesc @0x82C424E0), compare `SourceSym()`, cap
  `5*stars`/`15000 pts`. The `*5; min(.,15000)` helper is fn_825B8058.

### Value struct = RB3 `SongStatus` (Wii oracle layout)
`mBandScoreInstrumentMask` (short) @0xc, `mReview` (uchar) @0xe, `mLastPlayed` (int)
@0x10 (the min-select field), `mHighScores[11]` @0x48 (= `(idx+0x12)<<2`). Exact match
to `src/band3/meta_band/SongStatusMgr.h` (rb3-Wii oracle) `class SongStatus`.

### Method-set = `SongStatusMgr` (oracle has all 62 methods, 41 in DC3)
GetHighScore / GetScore / GetStars / GetBestStars / GetSongReview /
GetBandInstrumentMask / CalculateTotalScore / GetPossibleStars … all present, shapes
match the 23+ accessors.

### DC3 ROSETTA (decisive — same compiler/flags, already MATCHING)
`~/code/milohax/dc3-decomp/src/lazer/meta_ham/SongStatusMgr.h`:
```
HamSongMgr *mSongMgr;                       // 0x38
std::map<int, SongStatus> mSongStatusMap;   // 0x3c
class SongStatusMgr : public Hmx::Object, public FixedSizeSaveable, public ContentMgr::Callback
```
`config/373307D9/objects.json` → `"lazer/meta_ham/SongStatusMgr.cpp": "Matching"`,
pinned in DC3 splits.txt. So this exact class, same toolchain, ALREADY matched 100%
in DC3 with `mSongMgr` immediately before a `map<int,SongStatus>`. DC3 layout
(`mSongMgr@0x38, map@0x3c`) is the closest existing twin to retail RB3
(`SongMgr@0x34, hash_map@0x38, list@0x3c`).

**The hash_map conversion premise CONFIRMED:** DC3 uses `std::map<int,SongStatus>`
(value@node+0x14); retail RB3 uses a **hash_map** (value@node+0x8). Same class,
container swapped — exactly the W8 hash_map vein signature.

---

## Pin span (method-confirmed)

Core SongStatusMgr `.text`: **0x825B8058 .. ~0x825B9F80** (23 find-accessors
0x825B8738–0x825B9ED0 + helpers + Save/Load/Handle funclets up to ~0x825BA800).
The dtor/ctor/Handle/SaveFixed exception funclets (fn_825B9CF8 0x110 w/ handlers,
fn_825B9ED0, fn_825B9A18 0x1E0) extend the tail; the EXACT upper bound must be
re-derived after wiring (pin a conservative core first, extend to next pin).

**ATTRIBUTION RISK / SLIVER EVICTION (load-bearing):**
- **`MoggClip.cpp` pin @0x825B8670–0x825B86A0 (size 0x30) is a MIS-PIN** — that fn is
  `Function_825B8058()*5; cap 15000` = a SongStatusMgr `GetPossibleStars`-style helper,
  NOT MoggClip. It sits INSIDE the SongStatusMgr cluster. Porting SongStatusMgr REQUIRES
  evicting this sliver (roadmap `requires_sliver_eviction` pattern: this-unit-IS-the-sliver).
- `Pose.cpp` pin @0x825B67B0–0x825B6804 is just below; verify it's not also SongStatusMgr
  before extending the lower bound past 0x825B8058.
- Neighbors: SongUpgradeMgr (map `mUpgradeData@0x1c`, distinct) below per frontier;
  CuePoint sort_heap @0x825BC7E8 / MainMenuPanel::DeleteDownloadedArts @0x825BE388 /
  OvershellSlot pin @0x825C10D8 above.

---

## Port recipe (self-contained, one worktree)

1. **Source:** start from rb3-Wii `src/band3/meta_band/SongStatusMgr.{h,cpp}` (already
   in OUR tree — the .h exists; .cpp must be copied from rb3-Wii). It has the RB3
   `SongStatus` struct + the correct accessor LOGIC (GetStars/GetBestStars/
   CalculateTotalScore verbatim).
2. **Re-layout the manager to retail (DC3-guided):** retail is NOT the Wii dev
   `mCacheMgr` array (`mLookups[1000]` + linear search). Replace it with a single
   `std::hash_map<int, SongStatus*>` member at the offset matching retail (map@0x38,
   SongMgr*@0x34 — i.e. SongMgr ptr immediately before the map, mirroring DC3's
   `mSongMgr@0x38; map@0x3c`). Rewrite `HasSongStatus`/`GetSongStatus`/`AccessSongStatus`
   as hash_map `find()` (NULL-miss → matches the inline find-COMDAT in every accessor).
   Drop the `SongStatusCacheMgr` array indirection.
3. **hash<int> spec + #define guard** (per the hash_map vein FIX recipe): unique guard,
   value@node+8.
4. **Wire** `SongStatusMgr.cpp` as `NonMatching` in objects.json; **pin** the core
   `.text` [0x825B8058, ~0x825B9F80] in splits.txt; `gen_game_target_map` entries.
5. **EVICT the MoggClip sliver** (0x825B8670 pin) — it's SongStatusMgr code; re-pin
   MoggClip to its real cluster or drop the sliver.
6. `touch config.yml && fresh_report.sh` A/B vs main@8314; re-run for splits-only FP.
   Honesty gate: net≥+1, no ≥8 foreign fn_@0% run, gains==intended.

**Risk:** MEDIUM. The MANAGER structure is DC3-proven, but RB3's `SongStatus` value
struct (mHighScores[11], mReview, mBandScoreInstrumentMask, the 11×4 SongStatusData)
differs from BOTH DC3 (Dance Central data) and must come from the Wii oracle, then be
verified byte-exact against retail value-offsets (already cross-checked: @0xc/0xe/0x10/0x48).
The base MI (`Hmx::Object + FixedSizeSaveable + ContentMgr::Callback`) drives the
vtable/funclet layout — get the inheritance order from DC3.

Expected: +12–20 (23 find-accessors + helpers; some Save/Load/Handle funclets may be
permuter-class). Frontier est +18 is reasonable for the accessor mass alone.

---

## Adjacent leads discovered (seed later layers)

- **MoggClip sliver-mispin audit** (0x825B8670): MoggClip.cpp is pinned to a 0x30 sliver
  that is SongStatusMgr code. Run `pin_audit.py` on this region post-SongStatusMgr-land;
  the whole [Pose 0x825B67B0 → SongStatusMgr 0x825B8058] gap needs a sliver sweep.
- **DC3-twin port multiplier:** DC3 has `SongStatusMgr.cpp` MATCHING — the same DC3
  Rosetta likely covers other RB3 meta_band singletons. Cross-index DC3 `meta_ham/*`
  Matching files against RB3 anon clusters (BandSongMgr=β already known; check
  SongSortMgr, LicenseMgr, ContentMgr::Callback users).
- **SongStatus FixedSizeSaveable Save/LoadFixed** (fn_825B9CF8 w/ handlers,
  fn_825B9A18 0x1E0): the serialization funclets are the same SAVE_REVS-style
  rev-constant risk as PostProcer (roadmap saverev pattern) — separate body-port,
  cross-check DC3 `SaveStd/LoadStd(..,0xD48,0x83)` rev constants vs RB3.
