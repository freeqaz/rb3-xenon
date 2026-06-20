# W12 — SaveLoadManager (band3/meta_band) — DISCOVER dossier

Date: 2026-06-20
Mode: DISCOVER/PLANNER (read-only main, baseline 9301 @ d2d3e53)
TU: `band3/meta_band/SaveLoadManager.cpp` — unpinned, in the
MusicLibrary→AccomplishmentManager gap.

## VERDICT: DEFER (port-gated + boundary-gated; not independently landable now)

SaveLoadManager **is** compiled into the retail XEX and located, but it is NOT a
clean self-contained port-then-pin target this wave. Two hard ground-truth
blockers:

1. **Oracle is the wrong platform.** The only game oracle
   (`../rb3/src/band3/meta_band/SaveLoadManager.cpp`) is the **Wii** build. It is
   built entirely on `MemcardMgr_Wii.h` / `WiiProfileMgr.h` /
   `SaveMemcardAction`/`LoadMemcardAction`, and its state machine literally
   contains `MILO_FAIL("SelectDevice not supported on the Wii.\n")` (states 0x5b,
   0x61). On Xbox 360 device selection **is** supported, so the retail Poll /
   SetState / OnMsg(*) state machines diverge structurally from the Wii bodies.
   The DC3 twin (`dc3-decomp/.../meta_ham/SaveLoadManager.cpp`, 2029 lines, uses
   `MemcardMgr.h` not `_Wii`) is a *different game's* storage manager — closer
   platform, but not a byte oracle for RB3's states/symbols.

2. **All identifying strings are stripped** (the `[[project_debug_output_stripping]]`
   pattern). None of the Wii error strings ("CacheMgr search returned error",
   "SelectDevice not supported on the Wii") NOR the DC3 strings
   ("HandleEventResponse: expected state", "saveloadmgr is not idle", "Bad choice
   index", "SAVESIZE TOTAL") exist in the retail binary. So the giant state-machine
   bodies (Poll ~4096B, SetState ~2628B, GetDialogMsg ~6592B) cannot be verified
   against either oracle by content — they must be reconstructed blind and verified
   only at the instruction level. High risk, low certainty.

3. **Boundary is gated on an unpinned sibling TU (ProfileMgr).** The SaveLoadManager
   cluster is interleaved in a dense multi-TU blob whose lower neighbour is the
   **unpinned** ProfileMgr/global-options TU. Pinning SaveLoadManager cleanly
   requires first establishing the ProfileMgr boundary (see "Boundary" below).
   This violates the SELF-CONTAINED / bound-vs-both-neighbours rule.

Honest deferral with evidence — per task rules, this is a valid outcome.

## Location (ground truth, via fingerprints.json + Ghidra decompile)

Gap: MusicLibrary `.text` end `0x8252E608` → PrefabMgr `.text` start `0x82540840`
(645 unpinned fns; multi-TU blob, NOT one TU).

SaveLoadManager functions identified (addr ← evidence):

| addr        | size  | identity (evidence)                                                        |
|-------------|-------|----------------------------------------------------------------------------|
| 0x825395B8  | 6592  | `GetDialogMsg`/`GetDialogOpt*` — refs `mc_manual_load_confirm/corrupt/...` |
| 0x8253D198  | 4096  | `Poll` — refs `saveload_dialog_event`,`song_info_cache_name`,`global_options_cache_name` |
| 0x8253E698  | 360   | refs `saveload_dialog_event`                                               |
| 0x8253EAF8  | 368   | **ctor** — Ghidra-confirmed: vtable `&PTR_LAB_820943dc`, `SetName("saveload_mgr"@0x82094728)`, `ThePlatformMgr.AddSink(SigninChangedMsg)`, member init matches Wii ctor (mActivated@0x18=0, mInitialLoadNotDone@0x19=1, reserve(4)) |
| 0x8253EF78  | 2576  | **Handle** (BEGIN_HANDLERS) — refs `activate,autoload,autosave,disable_autosave`,... |
| 0x8253FDA8  | 2628  | **SetState** — 106-case switch, recursive SetState (matches Wii state shape) |

Handler-action strings present in retail confirm the TU is compiled in:
`autoload@0x820948d0`, `manual_save@0x820948c4`, `enable_autosave@0x820948b4`,
`get_dialog_msg@0x8209485c`, `is_initial_load_done@0x820947fc`,
`saveload_dialog_event@0x82094584`, `song_info_cache_name@0x8209456c`,
`global_options_cache_name@0x82094550`, `saveload_mgr@0x82094728`.
RTTI: `.?AVSaveLoadManager@@ @0x82c421b0`. Vtable: `0x820943dc`.

SaveLoadManager `.text` extent (approx): **0x825395B8 .. ~0x82540000**
(GetDialogMsg start … past SetState 0x8253FDA8+0xA44=0x825407EC; then two tiny
0x40 fns 0x825407EC/0x82540814, then PrefabMgr at 0x82540840). Lower bound NOT
clean — see Boundary.

## Boundary problem (the gating issue)

Sorted big functions in the upper gap:
- 0x82535A48 (8008B) = **ProfileMgr** global-options Handle —
  refs `force_mic_gain, get_bass_boost, get_dolby, set_background_volume,
  has_primary_profile, relock_songs, set_*_volume`... (NOT SaveLoad; the
  `is_autosave_enabled` marker is a shared dialog symbol, a false-positive).
- 0x82534D38 (40B) refs `profile_mgr` → ProfileMgr accessor.
- 0x825395B8 (6592B) = SaveLoad GetDialogMsg (first confident SaveLoad fn).

So order is: …ProfileMgr(0x82535A48)… | SaveLoad(0x825395B8 … 0x8253FFFF) |
PrefabMgr(0x82540840). The ProfileMgr→SaveLoad transition between 0x82535A48+0x1F48
(=0x82537990, start of a 32B-thunk run) and 0x825395B8 is plausible but
**unverified**, and ProfileMgr itself is unpinned (`grep ProfileMgr splits.txt` =
none). A pin starting at 0x825395B8 risks (a) clipping SaveLoad fns that the
compiler emitted *below* GetDialogMsg interleaved with ProfileMgr, and (b) leaving
ProfileMgr as a 0-matched foreign run inside any over-broad pin → fails the
≥8-contiguous-FOREIGN-fn honesty gate.

## What WOULD make this landable (sequenced prerequisites)

1. **Pin+identify ProfileMgr first** (the lower sibling). Establishes the clean
   lower boundary for SaveLoadManager and is itself a larger EV target (8008B
   Handle + global-options accessors). Recommended as the keystone before SaveLoad.
2. **Port from DC3 twin, not Wii**, for the Xbox state machine — but treat DC3 as a
   structural scaffold only (different states/symbols); reconstruct RB3's exact
   state enum + symbol set from the retail switch (0x8253FDA8) instruction-by-
   instruction. The `os/Memcard.h`/`utl/Cache.h`/`CacheMgr` plumbing is the SAME
   on 360 (DC3 uses it), so the cache-path states (0x14-0x41) likely match DC3
   closely; the device-selection states (Wii-disabled) follow DC3/360.
3. **Verify at instruction level only** — strings stripped, so objdiff %/regalloc
   is the sole signal. Expect this to be a multi-session per-fn grind, not a one-
   worktree port.

## Expected delta

If the full TU were matched: the trivial accessors (IsInitialLoadDone, IsIdle,
GetDialogFocusOption, Activate, HandleEventResponseStart, GetProfile, Start,
Finish, AutoSave, AutoLoad, Init, ManualDelete, GetDialogOpt3, the 6 OnMsg are
medium) — realistically **+8 to +12** trivial/small fns are tractable once the
boundary is pinned; the four giant state-machine fns (Poll/SetState/Handle/
GetDialogMsg ≈ 15.9KB) are blind-reconstruction grind, not first-wave.

Given the boundary + oracle blockers, **expected_delta for THIS wave = 0** (defer).
Realistic post-prerequisite delta ≈ +10.

## Frontier handed off

- **ProfileMgr** (band3/meta_band, unpinned) — the lower sibling; pin+identify
  first. 8008B Handle @0x82535A48 + global-options accessors; likely +20-40 and it
  unblocks SaveLoadManager's lower boundary. Oracle: rb3-Wii `meta/Profile*` /
  `meta_band/ProfileMgr` + DC3 `meta_ham/ProfileMgr` (96.6% in DC3).
- **SaveLoadManager (this TU)** — re-attempt AFTER ProfileMgr is pinned, porting the
  cache-path states from the DC3 360 twin and reconstructing the RB3 state enum
  from the retail SetState switch.
