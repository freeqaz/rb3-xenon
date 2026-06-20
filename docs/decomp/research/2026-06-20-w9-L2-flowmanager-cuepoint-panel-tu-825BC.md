# W9 L2 dossier — "flowmanager-cuepoint-panel-tu-825BC" frontier (REFUTED-as-stated, REAL-as-cluster)

Date: 2026-06-20. Mode: adversarial discover/planner (read-only in main @8314).
Range investigated: the 0x825BB090–0x825C10D8 unpinned gap (between MoggClip/Pose
slivers below and the OvershellSlot pin at 0x825C10D8).

## Verdict on the frontier hypothesis

The frontier item ("SECOND TU in the gap … a MainMenuPanel/download-art panel TU
with embedded CuePoint anon-ns sort, est +15") is **WRONG on every specific**, but
the underlying region IS real, unwired, oracle-backed game code. Specifically:

1. **"CuePoint sort_heap" is a MIS-LABEL.** fn_825BC7E8 is a `sort_heap`/`__adjust_heap`
   template body. Ghidra's name-import tagged it
   `__adjust_heap_PAUCuePoint__A0x81ddebd1__...` — but `?A0x81ddebd1` is the anon-ns
   hash of **WaveFile.cpp** (`utl:WaveFile.obj` in the DC3 map), and WaveFile is
   already pinned at 0x827ADAA0–0x827AEB84, FAR from here. This region's sort is
   **ICF-folded** with WaveFile's CuePoint sort (identical machine code → the
   name-import borrowed WaveFile's symbol). The REAL sort here is
   `FilterViewSetting::SetFilterData` sorting `Filter` structs via
   `CompareFilters` (rb3-Wii `ViewSetting.cpp:212`, `AlphaKeyStrCmp`). There is no
   CuePoint class anywhere in rb3-Wii or DC3 game code; CuePoint exists ONLY in
   WaveFile.cpp / midi constants.

2. **"MainMenuPanel TU" is WRONG.** The only MainMenuPanel symbol in this gap is the
   target_symbol_map orphan `0x825BE388 -> ?DeleteDownloadedArts@MainMenuPanel@@AAAXXZ`
   (verified in scripts/target_symbol_map.json). The bulk of MainMenuPanel's COMDATs
   sit at 0x827A8xxx (3 map entries there). So 0x825BE388 is an ICF/COMDAT **sliver**,
   not the TU. Its body (Ghidra) deletes objects at this+0x1c/+0x20 = the
   mDlcImage/mUtilityImage loader deletes — a tiny generic deleting helper that
   linker-placed here.

3. **It is NOT one TU; it is a CLUSTER of ≥4 distinct unwired RB3 game TUs.** String
   anchors (resolved from auto_00_82000400_rdata.obj) prove the boundaries:

| approx span | owner TU (rb3-Wii oracle) | distinctive strings (rdata) | status |
|---|---|---|---|
| 0x825BB090–~0x825BB5B8 (~19 fns) | **AppLabel.cpp** (band3/meta_band) | `music_library_upsell_on/off`, `name`; fn_825BB408 = SetViewSetting/SyncProperty shape | unwired |
| **0x825BB5B8–~0x825BD5F0 (~70 fns)** | **ViewSetting.cpp** (band3/meta_band) | `filter_setting_genres/decades/difficulties`, `filter_none`, `bg_even.mat`, `bg_odd.mat`, `header.mat`, `disabled.color`, `options`, `filters`, `select_setting`, `set_to_setting_options` | **unwired (DOMINANT actionable)** |
| ~0x825BD5F0–~0x825BDF28 (~22 fns) | CriticalUserListener.cpp (band3/meta_band) | `critical_user_listener`, `critical_user_drop_out`, `clear/set/get_critical_user` | unwired (small, clean) |
| ~0x825BDF28–0x825BE7A8 (~23 fns) | MainMenuPanel sliver + CriticalUser tail / STL | (few strings; 0x825BE388 = DeleteDownloadedArts orphan) | murky — DEFER |
| 0x825BE7A8–0x825C10D8 (~87 fns) | PrefabMgr.cpp + **OvershellSlot upper** | `prefab_mgr`, `prefab_portrait_path_prefix/suffix` (PrefabMgr); `skip_choose_part`, `forced_part` (OvershellSlot) | mixed; OvershellSlot ALREADY WIRED at 0x825C10D8 (extend-up candidate) |

Confirmed AppLabel.cpp owns the upsell-region (rb3-Wii AppLabel.cpp references both
`music_library_upsell` and `SetViewSetting`); also unwired. AppLabel→ViewSetting
boundary is in 0x825BB4DC–0x825BB5B8 (the string-less functions there) — implementer
must confirm per-function. AppLabel itself is a separate portable work-item.

Note: the gap is a spatial neighborhood of `src/band3/meta_band/*` panel/provider TUs,
all RB3-specific (NOT in DC3 game code) with rb3-Wii sources. The 360-retail linker
packed several short TUs here. The frontier conflated 3-4 of them into one
"MainMenuPanel panel TU."

## Ground-truth method (reproducible)

- COFF symbol dump by VA from `auto_03_82260000_text.obj` (single .text section,
  little-endian COFF header, machine 0x01F2; symbol value = VA − 0x82260000; names
  encode VA as `fn_<VA>`). Script: /tmp/coffsym.py (in this session).
- String resolution from `auto_00_82000400_rdata.obj` (.rdata section; VA→file
  offset = VA − 0x82000400). Script: /tmp/rdstr.py.
- Per-function string anchors via `tools/ghidra/ghidra-decompile.py 0x<VA>` (Ghidra
  MCP @8002 up), grepping `0xffffffff820xxxxx` immediate operands = rdata string xrefs.

## Cross-binary oracle status

- ViewSetting.cpp: rb3-Wii `src/band3/meta_band/ViewSetting.cpp` (518 lines, 69
  `::` methods), `.h` 168 lines. Bases: `UIListProvider` (engine, wired),
  `Hmx::Object`. ALL header deps resolve in rb3-xenon tree (UIListProvider,
  SongSortMgr, BandLabel, Sorting, MusicLibrary, RockCentral all present). NOT in
  DC3 game code (DC3 false-friend N/A — pure rb3-Wii port).
- CriticalUserListener.cpp: rb3-Wii (74 lines). Deps: SessionMgr, BandUser,
  UIEventMgr, User — all present. Small, clean port.
- PrefabMgr.cpp: rb3-Wii (344 lines). The 0x825BE7A8 cluster.
- None of ViewSetting / CriticalUserListener / PrefabMgr appear in objects.json or
  splits.txt (fully unwired). OvershellSlot IS wired (0x825C10D8 pin, 2076-line src).

## Attribution / honesty cautions

- ALL pins here are attribution_risk=true: spans must be bounded by ground-truth
  string anchors per function, NOT by oracle fn count (oracle over-counts vs
  retail COMDAT interleave). The 70/22/87 fn counts above include STL helpers,
  inlined COMDATs, and possibly ICF slivers from foreign TUs (e.g. the
  DeleteDownloadedArts orphan). Each work-item MUST do per-function honesty audit
  (no ≥8-contiguous FOREIGN fn_@0% run) and pin only the contiguous own-TU body.
- The upsell-helper top region (0x825BB090–0x825BB5B8) and the murky
  0x825BDF28–0x825BE7A8 region are DEFERRED — owner not nailed to a single
  buildable TU. Re-drill before pinning.

## Adjacency / coordination

- cluster-alpha sits BELOW at 0x825B8738 and DOES use the int-key hashtable-find
  COMDAT `lbl_82552CD0` (confirmed: fn_825B8738 calls FUN_82552cd0) — that is the
  separate hash_map vein, NOT this gap. No shared boundary issue (cluster-alpha is
  below the MoggClip/Pose slivers at 0x825B86xx).
- OvershellSlot (wired) is the UPPER bound (0x825C10D8). The PrefabMgr/OvershellSlot
  mixed region (0x825BE7A8–0x825C10D8) is an extend-up candidate for the existing
  OvershellSlot pin — separate frontier.
