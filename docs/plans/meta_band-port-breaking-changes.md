# meta_band Port — Breaking Changes Tracking

Track intentional "break for correctness" decisions made during the band3/meta_band
porting session (2026-05-28). Each entry describes what was changed, why, and what
future work is needed to restore correctness.

---

## 1. `src/system/meta/StorePanel.h` — `MakeNewOffer` made non-pure

**Change:** `virtual StoreOffer *MakeNewOffer(DataArray *) = 0;`
→ `virtual StoreOffer *MakeNewOffer(DataArray *) { return 0; }`

**Why:** The rb3-xenon `StorePanel.h` was ported from DC3, which uses
`MakeNewOffer(DataArray *)` as the signature. The actual RB3 binary has a
different signature: `MakeNewOffer(const StorePackedOfferBase *, bool)` (visible
in rb3-Wii `src/system/meta/StorePanel.h`). `BandStorePanel` overrides the
StorePackedOfferBase version but NOT the DataArray version, making it abstract
under the DC3-derived header, which breaks compilation of BandStoreUIPanel.cpp.

**Root cause:** Our `StorePanel.h` is DC3-based but should be RB3-specific. The
RB3 and DC3 store architectures diverged: RB3 uses `StorePackedOfferBase` from
`system/meta/StorePackedMetadata.h`; DC3 uses `DataArray *` directly.

**Future work:**
- Port `StorePanel.h` from rb3-Wii fully: replace the `DataArray *` signature
  with `const StorePackedOfferBase *, bool`. This requires also porting:
  - `src/system/meta/StorePackedMetadata.h` (defines `StorePackedOfferBase`)
  - `src/system/meta/StoreOffer.h` (to add `StorePackedOfferBase` class — present
    in rb3-Wii at line 88 but absent from the DC3-based version in rb3-xenon)
- Once the correct signature is in place, remove the `{ return 0; }` stub.
- All subclasses of `StorePanel` in `src/band3/` should already have the correct
  override once the base is fixed.

**Impact:** Any existing engine code that calls `StorePanel::MakeNewOffer(DataArray *)`
will silently get a nullptr. Since no engine code is linked/run yet in matching
builds, this is safe for now.

---

## 2. `src/network/Platform/MutexPrimitive.h` — OSMutex replaced with void*

**Change:** Removed `#include <revolution/OS.h>` and changed
`OSMutex *m_hMutex;` → `void *m_hMutex; // Xbox: RTL_CRITICAL_SECTION *`

**Why:** The Wii-targeting `revolution/OS.h` does not exist on the Xbox 360
toolchain. `MutexPrimitive.h` is pulled into the build via:
`UploadErrorMgr.h` → `SessionMgr.h` → `BandMachineMgr.h` →
`network/net/NetSession.h` → `Platform/Time.h` → `Platform/RefCountedObject.h` →
`Platform/CriticalSection.h` → `Platform/MutexPrimitive.h`.

**Root cause:** The `src/network/` headers were ported from rb3-Wii directly
without substituting Wii OS primitives.

**Future work:**
- When `MutexPrimitive.cpp` is added to the build, change `void *m_hMutex` back
  to `RTL_CRITICAL_SECTION *m_hMutex` and add `#include "xdk/XBOXKRNL.h"`.
- The constructor/destructor/Enter/Leave implementation needs porting from
  rb3-Wii (OSInitMutex/OSLockMutex/OSUnlockMutex) to Xbox equivalents
  (RtlInitializeCriticalSection/RtlEnterCriticalSection/RtlLeaveCriticalSection).
- Similarly, `Platform/ObjectThread.h` (not currently in our build path) includes
  `revolution/os/OSAlarm.h` and `revolution/os/OSThread.h` — needs porting when
  ObjectThread.cpp is wired in.

**Impact:** Struct layout is preserved (pointer size unchanged). No behavioral
impact until MutexPrimitive.cpp is linked.

---

## 3. `src/system/bandobj/PatchDir.h` — DECLARE_REVS reordered

**Change:** Moved `DECLARE_REVS;` before `GetCurrentRev()` so that `gRev`
(which DECLARE_REVS declares as a static member) is in scope.

**Why:** The previous agent placed `DECLARE_REVS` after `GetCurrentRev()`,
causing MSVC C2065 "undeclared identifier 'gRev'" and C4430 "missing type
specifier" errors.

**Impact:** None — pure reordering of declarations within the class body.
No future work needed.

---

## Files NOT Ported (deferred)

### `BandStoreUIPanel.cpp` — EXCLUDED from objects.json (too many missing headers)

**Why:** Has a massive include chain:
`BandStoreUIPanel.cpp` → `InputMgr.h` → `SessionMgr.h` → `net/NetSession.h` →
`game/Game.h` → `game/Player.h` → `bandobj/BandTrack.h` (stub created) →
`game/SongDB.h` → `beatmatch/DrumMap.h` (not yet in rb3-xenon) → and more.

The include cascade hits ~15 missing headers before compilation can succeed.
These are deep beatmatch/game-engine headers that need proper full-class bodies,
not stubs, because they are used as full types in the call graph.

**Side effects of partial fixes made:**
- `bandobj/BandTrack.h` — minimal stub created at `src/system/bandobj/BandTrack.h`.
  This is a **correctness break**: the real BandTrack is a complex class inheriting
  from multiple engine objects. Future work: port the real class when the beatmatch
  + bandobj cluster is targeted.
- `MsgSource` class added to `src/system/obj/Msg.h` — needed by InputMgr.h,
  BandStorePanel.h, etc. This is a **new addition** from rb3-Wii, should be correct.
- `utl/VectorSizeDefs.h` created at `src/system/utl/VectorSizeDefs.h` — correct port.
- `DWCProfanityResultMsg` added to `src/system/os/PlatformMgr.h` — Wii DWC message
  never sent on X360; declaration is needed to satisfy `Jobs_Wii.h`. Correct for our
  purposes.
- `StorePackedOfferBase` forward-declared in `BandStorePanel.h` — correct, the type
  needs a full definition from `meta/StorePackedMetadata.h` when that header is ported.

**To re-enable:** Once `beatmatch/DrumMap.h` and all other game-layer beatmatch
headers are ported, add back to objects.json:
```json
"band3/meta_band/BandStoreUIPanel.cpp": "NonMatching"
```

The 10 other files (AccomplishmentCategory, AccomplishmentConditional,
AccomplishmentGroup, AccomplishmentTrainerListConditional, CampaignKey,
CampaignLevel, CymbalSelectionProvider, LicenseMgr, StandIn, UploadErrorMgr)
are all wired in objects.json and compile cleanly.
