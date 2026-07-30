> **STATUS (2026-07-30, laneBT-4):** rescued from the abandoned branch
> `cA2-MainHubPanel` (2026-06-23), which was never merged; the branch's code and
> span pin are correctly dead, but this honest-negative record was worth keeping.
> ⚠ **TU0-era addresses.** Main flipped to the TU5 address space on 2026-07-15, so
> every hex address below is against the OLD address space and cannot be used
> literally. Verified 2026-07-30: main pins `MainHubPanel.cpp` as carved
> per-function ranges (0x825C1E30, 0x8261FBB0–0x8262473C) that do **not** overlap
> the refuted span — i.e. main already follows the "correct path" prescribed at the
> bottom of this doc. The durable, era-independent lesson is the ICF stub-fold
> false-match trap: a foreign-blob span pin can read +70 matched while delivering
> zero real matches.

# MainHubPanel.cpp class-A span-pin — REFUTED (honest negative)

**Date:** 2026-06-23
**Branch:** `cA2-MainHubPanel` (DO NOT MERGE the span pin)
**Verdict:** HONEST NEGATIVE — the "validated OWN span [0x826032A0, 0x82607ED4)"
premise is **refuted by the rb3-Wii BinDiff oracle**. The span is a mixed/foreign
blob, NOT MainHubPanel. real_net_delta = **0**.

## What was attempted
Port-then-pin per the class-A recipe:
1. Ported `rb3/src/band3/meta_band/MainHubPanel.cpp` → `src/band3/meta_band/MainHubPanel.cpp`.
   The header `src/band3/meta_band/MainHubPanel.h` already existed (full, native-era).
   Only one MWCC→MSVC fix needed: `Timer::CyclesToMs(mMessageTimer.mCycles)` (private
   member access) → `mMessageTimer.Ms()` (equivalent inline: `Ms()` *is*
   `CyclesToMs(mCycles)`). Removed two redundant local-path includes. **Compiles clean.**
2. Added `band3/meta_band/MainHubPanel.cpp: NonMatching` to objects.json.
3. Pinned `.text start:0x826032A0 end:0x82607EE0` (end extended from the briefed
   0x82607ED4 to 0x82607EE0 because 0x82607ED4 cut through `except_data_82607ED8`;
   0x82607EE0 is the clean boundary where ManageBandPanel's fn_82607EE0 begins).
   Overlap self-check = 0. dtk auto-derived .pdata [0x822213C8, 0x82221910).

## Why it is refuted (decisive evidence)
The rb3-Wii BinDiff oracle (`unified_id_rb3wii.json`) attributes **42** functions to
`band3/src/meta_band/MainHubPanel.cpp`. **ZERO of them fall in the pinned span.**
They scatter from 0x822ce6d0 to 0x824eb14c (all BELOW the span), at near-random
similarities (MainHubPanel::AdvanceAll sim=0.11, OnMsg(ReleasingLockStep) sim=0.06,
the anon MainHubAdvanceMsg dtor sim=0.02). This is the textbook ICF-scattered TU
shape — and MainHubPanel is **named explicitly** in the wave-loop SOP and MEMORY as
this class: *"never span-pin a TU whose fns scatter across MB (MainHubPanel: 44/45
fns are ICF aliases = reveal-territory under foreign pins)."*

The 19 oracle entries that DO land in the span attribute to a **mixed bag of foreign
TUs**: AccomplishmentManager, TourPerformerLocal, StoreOfferProvider, network DDL
files (FriendsProtocolDDL/GatheringStatsDDL/Statistics), EventDialogPanel,
OvershellPanel, PatchPanel, TourDescPanel. It is a multi-TU blob, not MainHubPanel.

`gen_game_target_map.py --tu MainHubPanel.cpp --apply` consequently produced **0**
entries (no oracle MainHubPanel address is in-span to pair).

## The +70 was ICF-alias stub-fold inflation
Despite 0 map entries, the pinned span nominally added +70 matched (10005→10075).
`tools/icf_alias_check.py` on the newly-matched diff:
- REAL-BODIED: 5 (7.1%); STUB-FOLD: 65 (92.9%); longest contiguous stub run: 61.
- **Every one of the 70 is an anonymous `fn_<addr>`** — not a single named
  MainHubPanel method (`?Enter@MainHubPanel@@`, `?Poll@…`, etc.).
- Largest is 68 bytes; the rest ≤48 bytes (getter/dtor-thunk/vtable-thunk size class).
- Even the 5 "real-bodied" candidates (fn_82603B08/548/518/4E8/4B8) have **NO rb3-Wii
  oracle attribution at all** — they are not identified as any method.
This is exactly the wave-14 LockStepMgr / wave-15/16 fake-match trap: trivial
anonymous functions in a foreign blob byte-folding against our compiled stubs.

**Removing the pin returns matched to 10005 (= baseline)** — proving the entire +70
was the false pin, zero real MainHubPanel matches.

## Disposition
- Span pin **NOT landed** (reverted from splits.txt).
- Ported, compiling `src/band3/meta_band/MainHubPanel.cpp` + the objects.json
  compile-only scaffold **kept** — reusable for the correct approach.
- **Correct path** (per MEMORY locator reframe): MainHubPanel needs the high-confidence
  LOCATOR + per-fn identity-transfer on CONFIRMED VAs (its 42 methods are scattered;
  the BinDiff oracle is near-random for them — identity_transfer on these raw oracle
  VAs would carve WRONG ranges, the wave-16 BandProfile 0/64 outcome). Do NOT span-pin.
