# DEFER handoff — Profile::GetPadNum "missing-virtual" (batch-6 lever #1)

2026-07-10 foundational-levers wave (levers branch). Verdict: **DEFER — do NOT
land; the missing-virtual hypothesis is DISPROVEN** with whole-binary numbers.
Exploratory 1-line edit committed for the record on branch `lever1`
(038abb02, worktree /home/free/tmp/wt-lever1); NET **-6**, 0 gained.

## Ground truth: retail Profile::GetPadNum is NON-virtual

- Retail Profile vftable pair read directly from default_tu5.xex:
  - Object-rooted vftable @ **0x82112148**: 22 slots, ends `FindPathName@0x54`
    — GetPadNum ABSENT.
  - FixedSizeSaveable-rooted vftable @ **0x821121a0**: byte-identical to ours
    (10 slots: ~Profile, SaveFixed/LoadFixed, HasCheated, IsUnsaved,
    SaveLoadComplete, HasSomethingToUpload, DeleteAll, PreLoad@0x24).
- rb3-Wii oracle declares `GetPadNum() const` non-virtual; DC3's Profile.h is
  byte-identical to ours.
- **Decisive:** 6 fns matched 100% WITH our direct
  `bl ?GetPadNum@Profile@@QBAHXZ` — MemcardMgr_Xbox::On{LoadGame, SaveGame,
  DeleteSaves, CheckForSaveContainer, Msg} + RockCentral::RecordBattleScore —
  and ALL regress when GetPadNum is made virtual. Retail direct-calls it there.

## Measured A/B (2 runs, identical)

`virtual int GetPadNum() const;` → 20080 → **20074 (NET -6)**, 0 gained,
6 strict regressions (list above) + fuzzy drop RockCentral::UpdateFriendList
99.4→96.4. StorePanel::Load rose 89.2→93.3 only (MSVC appends the new virtual
to the FixedSizeSaveable vftable slot 0x28, NOT retail's vbase slot 0x64 —
wrong mechanism, no strict flip).

## What the StorePanel slot-0x64 sequence actually is

The 6-insn vbtable→vbase→vtable-slot-0x64 dispatch in StorePanel::Load /
EnumerateOffers is a **StorePanel source-reconstruction divergence**, not a
Profile-vtable property: our `StorePanel::StoreProfile()` is a
`return nullptr;` stub, so the static type/dispatch context differs from
retail (retail's Object-rooted Profile vftable tops out at 0x54, so the 0x64
slot belongs to a LARGER derived vftable — i.e. retail dispatches on a
derived/differently-typed object, plausibly BandProfile through
StoreProfile()'s real return). Fixing it = per-function StorePanel body/type
reconstruction, NOT a Profile.h lever. Reaching vbase+0x64 via headers would
require adding a virtual to Hmx::Object (sacred, DC3-shared, catastrophic
blast radius) — ruled out.

## Standing guidance

- Remove Profile::GetPadNum from missing-virtual scanner candidates (it will
  keep FP-ing on the StorePanel sites).
- StorePanel::Load / EnumerateOffers near-misses route to a StorePanel-specific
  body/type reconstruction (what does retail StoreProfile() return?), together
  with the StorePreviewMgr retail-form port (see
  storepreviewmgr-0x60-DEFER.md).
