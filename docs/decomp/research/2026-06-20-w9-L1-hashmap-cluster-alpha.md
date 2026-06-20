# W9 L1 — hashmap-cluster-alpha: ADVERSARIAL DRILL → owner IDENTIFIED

**Date:** 2026-06-20
**Mode:** ADVERSARIAL DISCOVER/PLANNER (Opus, layer 1), READ-ONLY in main repo.
**Baseline:** main @ 812e1df, 8314 / 65545 matched (fixed for all agents).
**Frontier item:** `hashmap-cluster-alpha` (kind=port-then-pin, est +20).
**Verdict:** **REAL_ACTIONABLE.** Owner identified with ground truth =
**`SongStatusMgr.cpp`** (3 classes: `SongStatus` / `SongStatusCacheMgr` /
`SongStatusMgr`), an **UNWIRED, UNPINNED** RB3 game TU. The w8 doc's
"CuePoint/FlowManager/MainMenuPanel neighbors" guess was wrong — those anchors
are in a *different, later* TU further up the same 0x8A38 gap.

---

## Ground truth (COFF `auto_03_82260000_text.obj` + `auto_00_82000400_rdata.obj` + Ghidra)

### The gap structure (the sliver-pin pattern)
- **Pose.cpp** pin ends `0x825B6804`.
- gap `0x825B6804 → 0x825B8670` (0x1E6C) — **start of the SongStatusMgr TU**
  (`SongStatus` ctor/dtor/Save/Load methods: fn_825B7C0C, fn_825B7D88,
  fn_825B83D8=SaveFixed, fn_825B8540=LoadFixed, vtables lbl_820B0D50/0DD8/0E20…).
- **MoggClip.cpp** pin = `0x825B8670 – 0x825B86A0` (**only 0x30 bytes = 1 fn**) —
  a **SLIVER mis-pinned INSIDE the SongStatusMgr TU**. (MoggClip's real code is
  the small ICF-displaced sliver; the SongStatusMgr cluster surrounds it.)
- gap `0x825B86A0 → 0x825C10D8` (0x8A38) — **cluster α**: the 23 hash_map
  accessors + the SaveFixed/LoadFixed orchestrators (fn_825BA270, fn_825BA440)
  + the rest of SongStatusMgr, THEN a foreign TU (CuePoint anon-ns sort_heap
  @0x825BC7E8, the panel/FlowManager-named code @0x825BCxxx-0x825BExxx).
- **OvershellSlot.cpp** pin starts `0x825C10D8`.

So the SongStatusMgr TU candidate span is **[0x825B6804, ≈0x825BB090)** (~0x488C
bytes, **168 functions**), with MoggClip's 0x30 sliver embedded. The exact end
is TBD by the worktree (extend until the first foreign fn_@0% run — the CuePoint
anon-ns is the obvious upper wall, lbl_820B0FF0/0FB0 vtables are the last
SongStatus-family ones).

### The 23-accessor int-key hash_map cluster (0x825B8738 → 0x825B9ED0)
23 distinct callers of int-key find-COMDAT `lbl_82552CD0`, **all** decoding
`addi rX, r3, 0x38; bl lbl_82552CD0`. NO Symbol-key (82543F88) callers in the
region → pure `hash_map<int, …>` class.

`lbl_82552CD0` decompiled = STLport `hashtable<int,…>::find`: `hash = key`,
`bucket = key % ((cont+0xc - cont+0x8)>>2)`, slist-walk comparing `node[1]==key`,
NULL on miss, value pointer at `node+0x8`. The accessors read
`*(short*)(*(int*)(node+8) + 0xc)` → **value is a pointer; the map is
`hash_map<int, SongStatus*>` (songID → SongStatus*)** used as the cache index.

### Why this is SongStatusMgr (semantic match, not just spatial)
- fn_825B8918 = `for(i=diff; i<4; i++) max(GetX(this,ty,i))` = **EXACT**
  `SongStatusMgr::GetBestStars/GetBestAccuracy/…` (rb3-Wii
  `SongStatusMgr.cpp:594` — `for (int i = diff; i < 4; i++)`).
- fn_825B8898 = `GetStars(this, idx, ty, diff)` → `GetSongStatus(idx)->GetStars`.
- fn_825BA270 / fn_825B96E0 = Save/Load iterating **11** entries (= 11
  ScoreTypes, matches `mHighScores[11]`/`mSongData[11][4]`) with RB3 save-rev
  gates (`< 0x8f / 0x92 / 0x93`, `0x1da`) — `SongStatus::SaveFixed/LoadFixed`.
- fn_825B83D8/fn_825B8540 = BinStream write/read of `+0x8(int) +0xc(short)
  +0xe(byte) +0x10(int) +0x14(int)` then arrays — `SongStatus` fields
  (mSongID@0x8, mBandScoreInstrumentMask@0xc, mReview@0xe, mLastPlayed@0x10,
  mPlayCount@0x14 — matches `SongStatusMgr.h` exactly).
- RTTI type-descriptor string **`.?AVSongStatusMgr@@` @ 0x82c43134** confirmed
  present in the binary (Ghidra strings).

### THE RETAIL DIVERGENCE (the load-bearing fact)
rb3-Wii `SongStatusCacheMgr` looks up songID via **linear scan over
`mLookups[1000]`** (`GetSongStatusIndex`: `for i<1000 if idx==mLookups[i].mSongID`).
The **retail XEX replaced this with a `hash_map<int, SongStatus*>` index** living
at the +0x38 container. The **existing rb3-xenon `SongStatusMgr.h` still has the
Wii `SongStatusLookup mLookups[1000]` layout (line 196) — it does NOT have the
hash_map**, so a naive port will NOT match. This is the classic game-code
DC3/Wii false-friend: the body oracle (rb3-Wii) is structurally older than retail.
The worktree must reconstruct the retail cache layout from the asm: add a
`std::hash_map<int, SongStatus*>` member (at the offset that lands the find at
this+0x38 for the accessors) and rewrite GetSongStatusIndex/HasSongStatus/
AccessSongStatus to use it.

### Status / assets already present
- **`src/band3/meta_band/SongStatusMgr.h` ALREADY EXISTS** in rb3-xenon (Wii
  layout) — partial head-start; needs the hash_map cache edit.
- **`SongStatusMgr.cpp` does NOT exist** — full port from
  `../rb3/src/band3/meta_band/SongStatusMgr.cpp` (1160 LOC) required.
- NOT in objects.json, NOT in splits.txt.
- Conversion-pattern references (already-converted-to-hash_map + pinned, int-key
  find): **FixedSizeSaveableStream.cpp** (splits L1831), **SongMgr.cpp** (L1242),
  **AccomplishmentProgress.cpp** (L2439). Mirror their `std::hash_map` member +
  `find()` idiom + the unique `hash<int>` spec #define guard.

---

## Work item (self-contained, one worktree, independently landable vs main@8314)

### WI-α1 — Port + wire + pin SongStatusMgr.cpp with the retail hash_map cache
**ALL in one `scripts/setup_worktree.sh` worktree:**
1. Port `../rb3/src/band3/meta_band/SongStatusMgr.cpp` (1160 LOC, MWCC→MSVC X360).
2. **Edit SongStatusMgr.h cache layout to match retail**: replace/augment the
   `mLookups[1000]` linear index with `std::hash_map<int, SongStatus*>` so the
   find-COMDAT lands at the offset the accessors use (this+0x38 for the
   accessor's `this`; verify whether `this` is SongStatusCacheMgr or SongStatusMgr
   by re-deriving the offset from the asm — the accessors take `this`→+0x38
   directly). Rewrite GetSongStatusIndex/HasSongStatus/AccessSongStatus/
   GetSongStatus to use the hash_map. Cross-check rb3-Wii for the *intent*
   (linear scan semantics) but the *layout* from retail asm.
3. Add the unique `hash<int>` specialization with a #define guard (mirror
   FixedSizeSaveableStream/SongMgr to avoid ODR collision).
4. Wire NonMatching in `config/45410914/objects.json`.
5. Pin `.text start:0x825B6804 end:<TU_END>` in `splits.txt` (start = Pose's end;
   end = extend up to the first foreign fn_@0% run — candidate ≈0x825BB090,
   refine empirically; do NOT swallow the CuePoint/panel TU above). `touch
   config.yml && ninja` back-fills `.pdata`.
6. **Handle the MoggClip 0x30 sliver**: it sits INSIDE this span (0x825B8670).
   Either (a) keep MoggClip's sliver pin and pin SongStatusMgr around it (two
   sub-ranges), or (b) if MoggClip's sliver is a dead ICF one-off displaced into
   this TU, evict it (the roadmap `requires_sliver_eviction` pattern). Decide via
   objdiff: if MoggClip's 1 fn at 0x825B8670 is really SongStatusMgr code, fold
   it in; if it's a genuine MoggClip ICF sliver, pin around it.
7. Add target_symbol_map.json + symbols.txt map-entries for the now-byte-exact
   find-using accessors (VA-keyed; re-pins auto-pair).
8. Whole-binary A/B vs main@8314 (the VERIFY COMMAND). Honesty gate: net≥+1, no
   ≥8-contiguous foreign fn_@0% run, headline net == intended unit gain.

**Expected:** +20–40 (23 trivial map accessors + the GetBest* difficulty-loop
wrappers + Save/Load + the SongStatus inner-class methods, most byte-exact once
the cache layout matches). **attribution_risk: TRUE** (pin/relocation + sliver +
multi-class TU; the TU-end bound and MoggClip-sliver disposition are empirical).

---

## Bottom line
Frontier item is REAL. Owner = SongStatusMgr.cpp (unwired RB3 game TU, .h
present with the wrong/Wii cache layout). The "+0x38 hash_map<int,short>" is the
retail-only `hash_map<int, SongStatus*>` song-index cache that replaced Wii's
`mLookups[1000]` linear scan — the precise game-code divergence to reconstruct.
Self-contained port+wire+pin+convert+reveal in one worktree, independently
landable. Adjacent leads (BandSongMgr β, the 825BC-825BE FlowManager/panel TU,
the 3rd find-COMDAT 82B23238) emitted as discovered_frontier.
