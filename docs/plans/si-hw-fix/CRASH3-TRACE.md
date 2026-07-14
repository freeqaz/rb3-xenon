# CRASH #3 root-cause trace — null smasher-plate dir on 2-same-instrument load (2026-07-14)

Fault: `Iar=0x82B998D4` (`lbz r11, 0x170(r31)`, r31=NULL), thread 0xf9000000, first-chance,
live console 192.168.8.180 frozen at fault. Registers at capture: `r29=0x45191170` (this),
`r30=0` (loop index = slot 0), `r28=0x8219ED08` (→ image string `"smasher.trans"`, verified),
`r31=0`, `Lr=0x82B998C4`.

## 1. What the crash function is, and the exact field chain

The faulting function `0x82B99878` is **`GemManager::UpdateSlotPositions()`**
(decomp oracle: `rb3/src/band3/bandtrack/GemManager.cpp:1014-1027`; matches the Xbox
body instruction-for-instruction):

```cpp
void GemManager::UpdateSlotPositions() {
    for (int i = 0; i < GetMaxSlots(); i++) {
        RndDir *dir = mNowBar->FindSmasher(i)->Dir();               // r31 <- NULL
        RndTransformable *t = dir->Find<RndTransformable>("smasher.trans", false);
        if (t) tf = t->WorldXfm();                                  // 0x82B998CC
        else   tf = dir->WorldXfm();                                // 0x82B998D4  <== FAULT
        mTrackDir->SetSlotXfm(i, tf);                               // 0x827E0148
    }
    for (int i = mBegin; i < mEnd; i++) mGems[i].UpdateTailPositions(); // 0x82BAB1C0
}
```

Asm ↔ field map (all live-verified via `getmem`):

| Step | Asm | Meaning | Live value |
|---|---|---|---|
| `this` (r29) | — | **GemManager** (non-virtual; no vtable) | `0x45191170` |
| `this->[0x00]` | `lwz r3,0(r29)` @82B99908 | `mTrackDir` (TrackDir*, vtable `0x820276C4`) | `0x44FFA198` |
| `this->[0x04]` → `[0xC]` | `bl 0x822E4460` | `mTrackConfig->GetMaxSlots()` | `0x423C84A8`, count = **5** |
| `this->[0xC8]` | `lwz r3,0xC8(r29)` @82B998A8 | **`mNowBar`** (NowBar*; first member = `mSmashers` vector) | `0x423C82C0` |
| `bl 0x82BAA4D8` | operator[]-style | **`NowBar::FindSmasher(i)`** → `mSmashers[i]` (bounds-checked, else NULL) | begin `0x430750D0`, end `0x430750E4` → 5 GemSmasher* |
| `->[4]` | `lwz r31,4(r3)` @82B998B0 | **`GemSmasher::mDir`** (RndDir*; `+0` = mSlot) | **NULL** |
| `bl 0x8227D418` | Find wrapper | `Find<RndTransformable>("smasher.trans")` → `ObjectDir::FindObject` `0x82750188` (DLL-hooked → returns NULL for null dir) | returns 0 |
| `lbz r11,0x170(r31)` | not-found fallback | `dir->WorldXfm()` dirty-flag read — RndDir's RndTransformable base is at `+0xD4`, dirty byte at `+0x9C` within it → `0xD4+0x9C=0x170` | **FAULT** (r31=0) |

Live smashers (`getmem`): `mSmashers[0]=0x424E0E70 {mSlot=0, mDir=NULL}`,
`[1]=0x45191398 {1, NULL}`, `[2]=0x45191410 {2, NULL}` — **every** GemSmasher of this
track has `mDir == NULL`, not just slot 0. Fault hit on iteration 0 (`r30=0`); every
slot would fault identically.

Single caller of the crash fn: `bl` at `0x82B9DB70` (GemManager track-config/poll path).

## 2. WHY the dir is NULL in the dup-instrument case

All 5 smasher dirs are NULL because **the whole NowBar was constructed with a NULL
`smasherPlateDir`** (`NowBar::NowBar`, NowBar.cpp:18-47): each
`smasherPlateDir->Find<RndDir>(cfg->Str(i+1), true)` ran against a NULL dir; the DLL
FindObject-null-guard turned each into NULL, so `new GemSmasher(i, NULL, ...)` × 5.
(GemSmasher's ctor and the hit/miss paths guard `if (mDir)` / `Null()` — that's why the
load survives until `UpdateSlotPositions`, whose not-found fallback derefs the dir itself.)

`smasherPlateDir` comes from `TrackDir::SmasherPlate()` = `GemTrackDir::mSmasherPlate`,
populated by this chain (Xbox VAs located by string/struct-pattern match against the real
console image `/tmp/rb3dx/default.base`, and matching the decomp source exactly):

- **`GemTrackDir::SetupSmasherPlate` = `0x822EADE0`** — called from
  `TrackPanelDir::AddTrack` when a slot is assigned an instrument
  (`TrackPanelDir.cpp:196`). Reads `mTrackInstrument` at GemTrackDir`+0x10`, calls ↓,
  stores result into `mSmasherPlate` ObjPtr at `+0x290` (raw ptr `+0x298`). **If the
  result is NULL it silently skips all setup** (`beq 0x822EB1AC`) — no crash, no log.
- **`GemTrackResourceManager::GetFreeSmasherPlate(TrackInstrument)` = `0x82356290`**:
  ```
  82356290  cmpwi cr6, r4, -2        ; inst == kInstPending(-2) -> return 0
  82356298+ loop over vector at mgr+0x34..+0x38, stride 0x14:
            info+0x0C == inst  &&  byte info+0x10 (mInUse) == 0
              -> mInUse=1; return *(info+0x08)      ; the RndDir* plate
  823562E8  li r3, 0 ; blr           ; pool exhausted -> NULL (retail strips the MILO_WARN)
  ```
- **`GemTrackResourceManager::InitSmasherPlates` = `0x82356A00`** (ctor `0x82356D40`,
  vtable `0x8203BF34`): builds the pool with **exactly ONE plate per instrument** —
  7 entries: `smasher_plate_guitar` (string @`0x8203C0F0`), `_bass`, `_keys`, `_drum`,
  `_real_guitar`, `_real_bass`, `_real_keys` — found in the shared track milo scene.

**The dup-instrument hole:** the pool is keyed by *instrument*, not by player/slot, and
holds one plate each. The first track on an instrument claims the plate (`mInUse=1`);
the **second track on the SAME instrument finds no `(inst, !mInUse)` entry and gets
NULL**. The base game can never reach this (retail enforces one player per instrument);
the SI feature's Layers A-C legalize the duplicate claim but populate nothing here —
`GemTrackDir::mSmasherPlate` for the 2nd track simply stays NULL. That NULL is what the
game "normally puts there": a real per-instrument `RndDir` smasher plate from the track
resource scene.

## 3. Crash #2 vs crash #3 — same null dir?

**Same underlying null object (the 2nd same-instrument track's missing smasher plate);
different consumer.** Evidence:

- Crash #2 (`Iar=0x8274E584`, arg `r3=0x8`) is **inside `ObjectDir::FindObject(NULL, …)`
  itself**: `0x8274FC58` (FindObject's exact-find inner, called from FindObject+0x2C —
  return addr `0x827501B8` was on crash #2's stack) computes `addi r31, r3, 8`
  (= `dir->mHashTable` at dir+8) and calls the hash-find `0x8274E570`, which does
  `lwz r11, 0(r3)` → with dir=NULL that's a read at `0x8`. Exactly the fault the
  FindObject null-guard hook now intercepts.
- Crash #2's other stack frames are all track-setup config machinery: `0x8268AE90/0x8277B490`
  is the cached-`SystemConfig("TRACK_SYMBOLS")` per-track-type symbol resolver (string
  verified at `0x8210C764`) — the helper behind `trackConfig.Type()`, which
  `NowBar::NowBar` calls on line 25 immediately before its 5 × `Find<RndDir>` on the NULL
  plate; `0x82760628` (frame `0x827606C4`) is a resolve-names-from-config-array helper.
- Chronology confirms it: with the hook installed, those same NowBar `Find(NULL, …)`
  calls now *succeed as not-found* (that is precisely how all 5 GemSmashers got
  `mDir=NULL`), and the load proceeds to the first consumer whose **not-found fallback
  dereferences the dir itself** — `UpdateSlotPositions` → crash #3.
- The earlier attribution of crash #2 to `DataNodeGetObjHook`'s type-guard
  (CRASH-2same-instrument doc) is likely a stale-stack artifact: the faulting call is a
  compiled-C++ FindObject on a null dir, not a node-type issue; the DTA
  `get_smasher_plate` handler (`GemTrackDir.cpp:1368`) is an equivalent DTA-side route to
  the same NULL plate.

## 4. Recommended fix (DLL-hook layer)

**(a) Populate upstream — hook `GemTrackResourceManager::GetFreeSmasherPlate`
(`0x82356290`).** This is the single choke point; fixing it cures crash #2's family,
crash #3, and every downstream smasher consumer (hit/burn triggers, slot transforms,
key-lane groups) in one place.

```c
/* ports_xbox360.h */
#define PORT_GETFREESMASHERPLATE 0x82356290 /* GemTrackResourceManager::GetFreeSmasherPlate */

/* SameInstrumentHooks.c — offsets verified against retail asm @0x82356290:
   vector<SmasherPlateInfo> at mgr+0x34/+0x38, stride 0x14,
   info: +0x08 plate (ObjPtr raw), +0x0C TrackInstrument, +0x10 mInUse (byte) */
RndDir *GetFreeSmasherPlateHook(void *mgr, int inst) {
    RndDir *plate = GetFreeSmasherPlate(mgr, inst);      /* trampoline to original */
    if (plate != NULL || !SI_ENABLED || inst == -2 /*kInstPending*/)
        return plate;
    /* pool exhausted: 2nd+ player on the same instrument -> SHARE the in-use plate */
    char *p   = *(char **)((char *)mgr + 0x34);
    char *end = *(char **)((char *)mgr + 0x38);
    for (; p < end; p += 0x14)
        if (*(int *)(p + 0x0C) == inst)
            return *(RndDir **)(p + 0x08);   /* leave mInUse accounting untouched */
    return NULL;
}
```

Sharing is sound here: `SetupSmasherPlate` does **not** reparent the plate's transform
(only `RndGroup::AddObject`, a draw-list insert), so `UpdateSlotPositions` reads the same
lane world-transforms either way — identical lane layouts are exactly correct for two
tracks of one instrument. Known v1 costs, both acceptable and non-crashing:
  - cosmetic hit-FX cross-talk (both players' hit flames animate the one shared plate —
    a full fix would clone the plate RndDir, out of DLL scope);
  - `ReleaseSmasherPlate` double-release on teardown clears `mInUse` early / re-matches
    once (retail asserts stripped; plate object remains valid — harmless).

**(b) Keep the `ObjectDir::FindObject` null-guard hook (`0x82750188`)** as defense in
depth — returning NULL for a null dir matches retail's "not found" semantics and converts
hard faults into recoverable paths. But crash #3 proves it is **not sufficient alone**:
`Find(name, false)` callers' not-found fallbacks may dereference the dir itself
(`dir->WorldXfm()` here). It is a net, not the fix.

**(c) Do NOT fix by guarding `GemManager::UpdateSlotPositions` (`0x82B99878`) alone** —
that suppresses crash #3 but leaves the 2nd track with no slot transforms (gems
mispositioned), dead hit FX, and more unguarded null-dir consumers waiting downstream
(e.g. `GemManager.cpp:945`'s `mTrackDir->SmasherPlate()` use, DTA `get_smasher_plate`).
Whack-a-mole; the upstream populate in (a) is the proper fix.

## Appendix — key VAs (real console image, base 0x82000000)

| VA | What |
|---|---|
| `0x82B99878` | `GemManager::UpdateSlotPositions` (crash #3 fn; fault @+0x5C) |
| `0x82B9DB70` | its only caller (`bl`) |
| `0x82BAA4D8` | `NowBar::FindSmasher` (bounds-checked `mSmashers[i]`) |
| `0x8227D418` | `Find<RndTransformable>` wrapper (FindObject + checked cast) |
| `0x82750188` | `ObjectDir::FindObject` (DLL-hooked; PORT_OBJECTDIRFINDOBJECT) |
| `0x8274FC58` / `0x8274E570` | FindObject inner exact-find / hash-find (crash #2 fault path, `dir+8`) |
| `0x823F7A80` | `RndTransformable::ComputeWorldXfm` (dirty path) |
| `0x827E0148` | `TrackDir::SetSlotXfm` (vector of 0x40-byte Transforms at TrackDir+0x260) |
| `0x822EADE0` | `GemTrackDir::SetupSmasherPlate` (mSmasherPlate ObjPtr @+0x290, raw @+0x298; mTrackInstrument @+0x10) |
| `0x82356290` | `GemTrackResourceManager::GetFreeSmasherPlate` ← **hook here** |
| `0x82356A00` / `0x82356D40` | `InitSmasherPlates` (7 single plates) / GTRM ctor (vtable `0x8203BF34`) |
| `0x8219ED08` | `"smasher.trans"` string |
| `0x8203C0F0` | `"smasher_plate_guitar"` string |

Live objects (this capture): GemManager `0x45191170`, mTrackDir `0x44FFA198`
(vtable `0x820276C4`), mNowBar `0x423C82C0`, mSmashers begin `0x430750D0` (5 entries),
smasher[0] `0x424E0E70` — `mDir=NULL` for all smashers.

---

## Hardware-test outcome — fix shipped & verified boot (2026-07-14)

The plate-share fix from (a) was implemented in the RB3Enhanced DLL and deployed to the
devkit. **Crash #3 is resolved: the 2-same-instrument song now loads and reaches gameplay.**

**Implementation (RB3Enhanced, XDK-free OSS build):**
- `include/ports_xbox360.h` — `PORT_GETFREESMASHERPLATE 0x82356290`
- `source/_functions.c` — `RB3E_STUB(GetFreeSmasherPlateOrig)` (call-original trampoline)
- `source/SameInstrumentHooks.c` — `GetFreeSmasherPlateHook(mgr, inst)`: calls the original;
  on NULL return + `SI_ENABLED`, walks the plate pool (`mgr+0x34..0x38`, stride `0x14`,
  `entry.instrument`@`+0x0C`, `entry.plate`@`+0x08`) and returns the instrument's existing
  in-use plate. Wired as a 5th `SI_HOOK` in `InitSameInstrument()`.
- Build: 51/51 compiled, link rc=0. Map VAs: `GetFreeSmasherPlateHook` `0x840196b8`,
  `GetFreeSmasherPlateOrig` `0x84012de8`.
- Pack/deploy: `pack-si-dll.sh --deploy` → 57344-byte DLL, sha `4e9d6b7c1f97cafa`,
  xexlint PASS (0 reject), on-drive sha matches; DLL confirmed mapped at base `0x84000000`
  alongside `default.xex` after launch.

The `ObjectDir::FindObject` null-guard hook (b) was kept as-is (defense in depth).

### Live-observed Phase-2 limitations (NOT crash regressions — pre-existing v1 design gaps)

First real 2-same-instrument playtest (two guitars, one Medium + one Expert) surfaced two
behaviors, both already documented as Phase-2 items in
`../rb3enhanced-same-instrument-patch.md` (§7, collision table L428-433, test-ladder step 10):

1. **Both players see the Expert note chart** (scroll speeds differ correctly, but the gems
   are the same difficulty). Root cause is the shared-difficulty v1 limitation:
   `ProcessConfigHook` collapses both players onto one `mTrackNum` and does last-writer-wins
   on `mTrackDiffs[num]` (SameInstrumentHooks.c:271); `RecalcGemListHook` then indexes the
   gem-list clone by that shared value (`SongDataTrackDiff`, line 430) → both pull the same
   (last-written) difficulty. Scroll speed differs because it comes from a separate per-player
   subsystem, not `mTrackDiffs`. This is exactly the "degrades gracefully" case the test
   ladder predicts for mismatched difficulties. Fix (Phase-2, ~1 line): the clone already
   holds every difficulty, so index `RecalcGemListHook` by *this watcher's own player
   difficulty* instead of the shared track difficulty.

2. **Inputs are shared / note-stealing** — a hit on one controller registers for both
   players. The Center-layer gem-clone in `RecalcGemListHook` was meant to prevent exactly
   this by giving each watcher a private `mGemList`, so full input-sharing implies the clone
   is **not engaging** for this config. Most likely cause: sharing `mTrackNum` (Layer C)
   makes the game create a **single** `TrackWatcherImpl` for the track rather than one per
   player, so `claim->claims` never reaches 2 and no clone is ever made → one watcher, one
   gem list, one input target (which would unify both symptoms #1 and #2 under one root
   cause). **Open Phase-2 trace question:** for a shared `mTrackNum`, does RB3 instantiate
   one `TrackWatcherImpl` or two? That single fact decides whether the fix is per-watcher
   difficulty selection (if two watchers exist) or a deeper per-player watcher/input-routing
   change (if only one). The paused live session (frozen at song load) is the vehicle for
   this trace: read the live `TrackWatcherImpl` count and their `mTrackNum`/`mGemList`
   pointers via XBDM.

**Status:** crash-fix objective COMPLETE. Items #1/#2 are Phase-2 feature work with their own
trace/design, out of scope for this crash fix.
