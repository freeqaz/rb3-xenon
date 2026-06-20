# W9 L5 — songstatusmgr-hashmap-pin-evict: VERIFIED REAL, worktree LAND-READY

**Date:** 2026-06-20
**Mode:** ADVERSARIAL DISCOVER/PLANNER (Opus, layer 5), READ-ONLY in main.
**Baseline:** main @ 812e1df, 8314 / 65544 matched (fixed for all agents).
**Frontier item:** `songstatusmgr-hashmap-pin-evict` (kind=scout, est +24).
**Verdict:** **REAL_ACTIONABLE.** The worktree
`wt-w9-songstatusmgr-port-convert-pin-evict@c1fd97e` already executed the full
recipe and its report reads **8348 (+34 vs main)**. Land it (after the standard
fresh whole-binary A/B). All ground-truth tells confirm the work is honest.

---

## What the worktree did (verified by diff + COFF ground truth)

Commit c1fd97e = "SongStatusMgr: hash_map<int,SongStatus*> re-layout +
find-accessor port (+34 @100%)". Diff vs main@812e1df:

- **objects.json**: `+ "band3/meta_band/SongStatusMgr.cpp": "NonMatching"` (wired).
- **splits.txt**: REPLACED the orphan `MoggClip.cpp` sliver pin
  `.text 0x825B8670–0x825B86A0` (0x30 = 1 fn) with
  `SongStatusMgr.cpp .text 0x825B8058–0x825BA440` (~76 fns); `.pdata`
  backfilled to `0x8221C470–0x8221C690` (real range, not the old 8-byte stub).
- **SongStatusMgr.h**: deleted the rb3-Wii `SongStatusCacheMgr`/`SongStatusLookup
  mLookups[1000]` linear-scan model; SongStatusMgr now multiply-inherits
  `Hmx::Object, FixedSizeSaveable, ContentMgr::Callback`; the song index is
  `std::hash_map<int, SongStatus*> mSongStatusMap` **at offset 0x38** (mSongMgr
  0x34, cached arrays 0x54/0x80/0xac, mUpdatingStatus 0xd8 …).
- **SongStatusMgr.cpp**: full port (958 lines) using
  `mSongStatusMap.find()/begin()/end()/[]/clear()`; no local `hash<int>` spec
  (STLport `<stl/_hash_fun.h>` supplies it — same as the already-landed
  SongMgr/FixedSizeSaveableStream int-key TUs, so no ODR guard needed).
- **target_symbol_map.json**: **exactly +34 new VA keys, 0 removed** (all in
  0x825B8xxx–0x825BAxxx). The 5678-line raw diff is JSON re-serialization noise;
  the semantic change is the 34 reveal entries. +34 keys == +34 matched.

Build artifacts present in the worktree: `build/45410914/obj/SongStatusMgr.obj`
(dtk target) **and** `build/45410914/src/band3/meta_band/SongStatusMgr.obj` (our
compiled) — both sides exist, so objdiff paired them and report.json is current.

---

## Ground-truth confirmations (auto_03_82260000_text.obj, authoritative retail COFF)

1. **Owner = SongStatusMgr, layout map@0x38.** Of the ~43 calls to the int-key
   find COMDAT `lbl_82552CD0` inside the pin `[0x825B8058, 0x825BA440)`, **every
   single one** decodes `addi rX, r3, 0x38; bl lbl_82552CD0`. The container is
   `this+0x38` with zero exceptions — exactly the worktree's `mSongStatusMap`
   offset. No Symbol-key (82543F88) callers in range → pure `hash_map<int,…>`.
   (Frontier item's "+0x54/+0x68/+0x74/+0x8c" offsets are WRONG; the real and
   only container offset is 0x38.)
2. **MoggClip eviction is HONEST.** `MoggClip.cpp` is pinned in splits.txt but is
   **absent from objects.json** (no compiled obj) — a dead orphan sliver (mf=0)
   sitting mid-cluster at 0x825B8670 (offset 0x618 into the SongStatusMgr TU).
   This is the textbook `requires_sliver_eviction` pattern (dead displaced sliver
   squatting in a real owner's TU). `MoggClipMap.cpp` (a *different*, real, wired
   unit at 0x8270D2D8) is untouched.
3. **Honesty gate passes.** The 37 non-100 fns in the unit are SongStatusMgr's
   own unported bodies bracketed by named SongStatusMgr methods
   (`?IsSongPlayed@SongStatusMgr@@`, `?GetAwesomes@SongStatusMgr@@`,
   `?SetProGuitarSongLessonComplete@SongStatusMgr@@`). The rb3-Wii oracle has 66
   `SongStatusMgr::` methods — consistent with the ~71-fn TU. No foreign fn_@0%
   run: the 0% functions are own-TU accessor/save/load bodies, not a foreign TU.

---

## Frontier lead corrections (FALSIFIED specifics; conclusion still REAL)

- **`FUN_82803f30` is NOT a getter.** 0x82803F30 = `__savegprlr_26`, a CRT
  register-save prologue helper. Ghidra auto-named the spill thunk `FUN_82803f30`.
  It is in a different region (0x828xxxx) entirely, not in the SongStatusMgr TU,
  and is irrelevant to landing — the worktree produced +34 without resolving any
  singleton getter. The real `TheSongStatusMgr` accessor (if one exists) is a
  separate, non-blocking Ghidra trace.
- **SongSortMgr is a DIFFERENT TU**, not part of this item. It was ported in its
  own worktree `wt-w9-songsortmgr-port-then-pin@8d4d72c` (report 8392, +78). Do
  not conflate; this item is SongStatusMgr only.
- **est +24 was low**; actual +34 (L1 dossier predicted +20–40 — in range).

---

## Pin-extension frontier (adjacent work, NOT included in this land)

The worktree ended the pin at **0x825BA440** (conservative: where int-find users
stop). Above it, `[0x825BA440, 0x825BB090)` (~13 fns) reads **neither this+0x38
nor lbl_82552CD0** — it does not use the hash_map, so it is NOT obviously
SongStatusMgr tail (could be SaveFixed/LoadFixed map-iterate orchestrators that
use begin/end, OR AppLabel head). The L2 dossier
(2026-06-20-w9-L2-flowmanager-cuepoint-panel-tu-825BC.md) places **AppLabel.cpp**
starting ~0x825BB090, then ViewSetting/CriticalUserListener/MainMenuPanel-sliver/
PrefabMgr up to OvershellSlot@0x825C10D8. So the `[0x825BA440, 0x825BB090)` band
is an unresolved seam — a possible SongStatusMgr pin-extension (+a few fns) once
the worktree lands and objdiff can A/B the extension empirically. Flagged as a
discovered frontier, not part of this self-contained land.

---

## Bottom line

`songstatusmgr-hashmap-pin-evict` is **REAL and already done** in
`wt-w9-songstatusmgr-port-convert-pin-evict@c1fd97e` (8348, +34). The conversion
(`hash_map<int,SongStatus*>@0x38` replacing Wii `mLookups[1000]`), the MoggClip
orphan-sliver eviction, the wire, the pin, and the 34 reveal map-entries are all
ground-truth-correct and honesty-gate-clean. ACTION: standard fresh whole-binary
A/B then land. Adjacent: the 0x825BA440→0x825BB090 pin-extension seam.
