# W10 — hashmap-clusteralpha: SongStatusMgr (cluster-α) — ALREADY-BUILT branch + pin-extension

**Date:** 2026-06-20  **Mode:** DISCOVER/PLANNER (Opus, wave 10), READ-ONLY in main.
**Baseline:** main @ **d910dd9 = 9037 / 65543** matched (fixed for this wave).
**Area:** `hashmap-clusteralpha` — the `hash_map<int,short>@this+0x38` cluster,
23 contiguous accessors in `[0x825B86A0,0x825C10D8)`.
**Verdict:** **REAL_ACTIONABLE.** Owner = **`band3/meta_band/SongStatusMgr.cpp`**
(SongStatus / SongStatusCacheMgr / SongStatusMgr classes). The full
port+wire+pin+convert work was **already executed in wave-9** on the branch
`w9-songstatusmgr-base-rebase-plus-getpossiblestars-reveal` (3 commits, **+45 vs
its base 812e1df**) but **was NEVER merged to main** — main @ d910dd9 still has the
old rb3-Wii `mLookups[1000]` layout in `SongStatusMgr.h` and the dead MoggClip
orphan pin. This wave's work-item is: **re-apply that branch onto main@9037 AND
extend the pin** to the TU's true bounds (the branch under-pinned both ends).

---

## Ground truth (COFF auto_03 by VA + auto_00 rdata strings + Ghidra @8002)

### The cluster region map at main @ d910dd9 (`.text`)
```
Pose.cpp                   0x825B67B0 – 0x825B6804   PINNED
[MusicLibraryNetSetlists]  0x825B6808 – 0x825B7F60   UNPINNED (a DIFFERENT TU — owner-id'd, NOT ours)
SongStatus head            0x825B7F60 – 0x825B8058   UNPINNED  <- HEAD EXTENSION (5 fns, SongStatus inner-class)
MoggClip.cpp (orphan)      0x825B8670 – 0x825B86A0   PINNED, DEAD (not in objects.json) -> EVICT
SongStatusMgr core         0x825B8058 – 0x825BA440   UNPINNED  <- branch pinned exactly this
SongStatusMgr tail         0x825BA440 – 0x825BADD0   UNPINNED  <- TAIL EXTENSION (~13 fns, still SongStatusMgr)
[AppLabel / sub-class TU]   0x825BADD0 – 0x825BB090   UNPINNED  <- DEFER (foreign vtable ctors PTR_Function_82527358)
[AppLabel.cpp]             0x825BB090 – 0x825BB5B8   UNPINNED (L2-id'd, separate item)
ViewSetting.cpp            0x825BB5B8 – 0x825BD5F0   PINNED (landed w9)
CriticalUserListener.cpp   0x825BD5F0 – 0x825BDF28   PINNED (landed w9)
OvershellSlot.cpp          0x825C10D8 – 0x825C3A44   PINNED
```

### Owner = SongStatusMgr (decisive, multiple proofs)
- **Cluster accessors use `this+0x38` hash_map exclusively.** fn_825B8738 (Ghidra):
  `FUN_82552cd0(this+0x38, &key)` → node; reads `*(short*)(*(int*)(node+8)+0xc)`.
  Value@node+8 is a `SongStatus*`; short@SongStatus+0xc = `mBandScoreInstrumentMask`.
  `lbl_82552CD0` = STLport `hashtable<int,V>::find`. NO Symbol-key (82543F88)
  callers in range → pure `hash_map<int, SongStatus*>`. Branch confirmed 43×
  `addi rX,r3,0x38; bl lbl_82552CD0` in [0x825B8058,0x825BA440).
- **SongStatus fields confirmed via SaveFixed/Clear.** fn_825B83D8 (SaveFixed)
  writes +0x8(int songID) +0xc(short instrMask) +0xe(byte review) +0x10(int
  lastPlayed) +0x14(int playCount) then arrays @+0x34/+0x74 — matches
  `SongStatusMgr.h` SongStatus layout. fn_825B7F60 = `SongStatus::Clear()` (oracle
  line 61): zeroes the same fields + the i==3||i==4 special-case array loop.
- **Mgr fields confirmed.** fn_825BA680 reads SongStatusMgr +0xd8 (`mUpdatingStatus`),
  +0x30 (`mSongMgr`), +0x3c (hashtable data list head, walked `puVar1=*puVar1`),
  +0xdc/+0xe0 — matches the branch's re-layout (map@0x38, mUpdatingStatus@0xd8).
- RTTI `.?AVSongStatusMgr@@` present in binary (L1).

### MusicLibraryNetSetlists owns the head BELOW 0x825B7F60 (do NOT pin)
- The big fn_825B71A0 references rdata vtable @0x820B0BDC followed by strings
  `owner_guid` / `valid_instr` / `s_id%03i` / `art_url` / `seconds_left` /
  `s_name%03i` → these resolve ONLY to `../rb3/src/band3/meta_band/MusicLibraryNetSetlists.cpp`.
- fn_825B7C90 / fn_825B7D88 read +0x2c/+0x38/+0x44/+0x5c/+0x60 and call BinStream/
  net ops (FUN_82725950/82732f68) — MusicLibraryNetSetlists methods. fn_825B7F10
  (`if (param_2&1) free`) is its deleting-dtor thunk. **Boundary is exact at
  0x825B7F60** (Clear directly follows the dtor thunk). MusicLibraryNetSetlists is
  itself unwired/unpinned — a SEPARATE future port item (the entire 0x825B6808–
  0x825B7F60 head, ~74 fns incl. the big fn_825B71A0). Emitted as frontier.

### Tail boundary at 0x825BADD0 (the attribution edge)
- [0x825BA440, 0x825BADD0): fn_825BA440 (reads SongStatus +0x8/+0x14/+0x18, calls
  cluster fn_825B9ED0), fn_825BA680 (mgr +0xd8/+0x3c iterate), fn_825BA990, plus
  SongStatusMgr's own anonymous static-Symbol init guards (`DAT_82dce3xx` flag
  clearers fn_825BA8FC/91C/AA30/AC20/…) and a dtor-thunk (fn_825BAD80) — all
  own-TU (boilerplate bracketed by named SongStatusMgr code, honesty-gate OK).
- **0x825BADD0+ is a DIFFERENT TU.** fn_825BADD0 sets vtable
  `PTR_Function_82527358_8208e6ac`; fn_825BAEC0 sets vtable `_820b12ac` whose slots
  point to 0x825BB1B0/0x825BB1E8/0x825BB220 (AppLabel region) AND **0x825BB9A0
  (INSIDE the ViewSetting pin!)**. These are AppLabel/sub-class ctors, NOT
  SongStatusMgr. **Pin must stop at 0x825BADD0.** (This is the wave-9-lesson-#3
  attribution edge — the tail vtable-ctors are foreign.)

### MoggClip orphan eviction is HONEST
- `MoggClip.cpp` pinned `0x825B8670–0x825B86A0` (0x30, 1 fn) but **absent from
  objects.json** (no compiled obj → mf=0 dead orphan). fn_825B8670 calls the
  cluster's own fn_825B8058 and computes `min(x*5,15000)` — it's SongStatusMgr
  code, not MoggClip. Textbook `requires_sliver_eviction`. `MoggClipMap.cpp` (the
  real, wired synth unit @0x8270D2D8) is untouched.

---

## The already-built branch (reference implementation — RE-APPLY, don't re-derive)

`w9-songstatusmgr-base-rebase-plus-getpossiblestars-reveal` (tip 3615a68, base
**812e1df** = wave-8 close 8314). Three commits, **+45** total:
1. **6dfc502 (+34):** wire `band3/meta_band/SongStatusMgr.cpp` NonMatching; full
   958-line port; **SongStatusMgr.h re-layout** — delete Wii `SongStatusCacheMgr/
   SongStatusLookup mLookups[1000]`, add `std::hash_map<int,SongStatus*>
   mSongStatusMap @0x38` (sizeof 0x1c, members chain to 0x54); pin
   `.text 0x825B8058–0x825BA440 / .pdata 0x8221C470–0x8221C690`, **evict MoggClip
   orphan**; +34 VA-keyed map entries. `hash<int>` is STLport-builtin (no ODR
   guard needed — same as SongMgr/FixedSizeSaveableStream).
2. **330eff5 (+10):** reveal 10 byte-exact methods (GetTotalSongs 0x825B8058,
   ??0SongStatus 0x825B81D0, SongStatusData::SaveToStream 0x825B8280,
   SongStatus::SaveFixed 0x825B83D8, SongStatus::LoadFixed 0x825B8540,
   GetBestSongStatusFlag 0x825B98C0, GetSongStatusFlag 0x825B9638,
   UpdateCachedTotalScore 0x825B9928, Clear 0x825B9E30, ??_E thunk 0x825BA438) +
   **fix the retail STAR CAP 5000→15000** (0x1388→0x3A98) in GetTotalBestStars
   (line 678), CalculateTotalStars (699), GetPossibleStars (709) —
   fn_825B8670/8FB0/9098 all emit `li 0x3A98`. (CalculateTotalScore's 2e9 score
   cap is unrelated, left alone.)
3. **3615a68 (+1):** reveal GetPossibleStars byte-exact.

**Applies cleanly to main@9037:** `SongStatusMgr.h` is byte-identical between
812e1df and d910dd9; the splits region [0x825B6804,0x825BB5B8) and these source
files are untouched by wave-9's Handle keystone. The re-apply is a content-level
cherry of the 5 changed files (objects.json +1 line, splits.txt MoggClip→
SongStatusMgr, target_symbol_map.json +55 net keys, SongStatusMgr.cpp new,
SongStatusMgr.h re-layout).

---

## Work item — see DISCOVER schema (WI-α)

Re-apply the branch onto main@9037, then **extend the pin** to
`.text [0x825B7F60, 0x825BADD0)` (head +5 SongStatus fns: Clear/SizeFixed/3
array-accessors @0x825B7F60–0x825B8058; tail +~13 SongStatusMgr fns/guards
@0x825BA440–0x825BADD0), add their reveal map entries, A/B.

**Pin-overlap self-check (PASSED):** [0x825B7F60,0x825BADD0) — nearest below
Pose (ends 0x825B6804), nearest above ViewSetting (starts 0x825BB5B8), no overlap
after MoggClip eviction. `.pdata` auto-derives into the free gap
[Pose.end 0x8221C2E0, ViewSetting.start 0x8221C7A0) — wider than the branch's
0x8221C470–0x8221C690 but still inside the free window (only the MoggClip 8-byte
stub 0x8221C4B0–0x8221C4B8 lives there, evicted with the orphan).

**Expected:** **+45 (branch) +~6–18 (extension)** ≈ **+51..63**. Conservative
floor +45 (the branch is proven). The extension fns are byte-exact-likely
(trivial accessors + static guards) once ported+revealed, but a few may be
permuter-class — count empirically.

---

## Assessment of the other two area leads (per task)

- **SongMgr-family hashmap cluster 8255f (L4 = BandSongMgr.cpp):** **ALREADY
  LANDED on main** — `band3/meta_band/BandSongMgr.cpp` is wired (objects.json:707)
  and pinned `.text 0x8255DE88–0x82563500` (splits.txt:2483). No work-item; the
  L4 frontier is closed. (Its adjacent SongSort leads also landed: SongSortMgr.cpp
  pinned splits.txt:1322.)
- **FlowManager-CuePoint-Panel TU 825BC (L2):** the **ViewSetting.cpp** and
  **CriticalUserListener.cpp** TUs the L2 dossier identified in that gap are
  **ALREADY LANDED on main** (pinned at 0x825BB5B8 and 0x825BD5F0). The "CuePoint
  sort"/"MainMenuPanel" labels were refuted by L2 (ICF-folded WaveFile sort;
  DeleteDownloadedArts orphan sliver). Remaining unwired in that neighborhood:
  **AppLabel.cpp** (0x825BB090–0x825BB5B8) and **PrefabMgr.cpp** (~0x825BE7A8) —
  emitted as frontier, separate items, not part of cluster-α.

---

## Bottom line
Cluster-α = SongStatusMgr.cpp. The hard work (port + hash_map@0x38 re-layout +
MoggClip eviction + star-cap fix + reveals = +45) is DONE on an unmerged wave-9
branch that re-applies cleanly to main@9037. Single self-contained work-item:
re-apply + extend the pin to the TU's true bounds [0x825B7F60,0x825BADD0],
stopping before the foreign vtable-ctors at 0x825BADD0. Empty of foundational
levers (the hash_map vein + Handle keystone are settled; no shared-header change
needed). attribution_risk=true (pin/relocation/sliver-eviction/empirical tail
bound).
