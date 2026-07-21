# LANDABLE — PlatformMgr MsgSource re-base (+4 strict, 0 lost) — batch-6 lever #2

2026-07-21 foundational-levers wave. Verdict: **LANDABLE**. Edit: ONE file,
`src/system/os/PlatformMgr.h` (+50/−36). Worker commit `d296bf53` (branch
`lever2`, worktree /home/free/tmp/wt-lever2); cherry-picked and independently
re-verified on branch `levers` as `2cd25fc4` (worktree /home/free/tmp/wt-levers).

## Ground truth (Ghidra default_tu5.xex-c5a170)

Retail PlatformMgr is **`: public MsgSource, public ContentMgr::Callback`**
(rb3-Wii lineage), NOT DC3's flat `: public Hmx::Object`. MSVC hoists the
vftable-carrying Callback base to primary:
`Callback vfptr@0x0 | MsgSource@0x4 (vbptr@0x4, mSinks@0x8, mEventSinks@0x10,
mExporting@0x18) | members@0x1c | virtual Hmx::Object at tail`.
The `addi r3, r29, 0x4` receiver at ~308 `ThePlatformMgr.AddSink/RemoveSink`
call sites is the MsgSource-subobject this-adjust (retail AddSink =
fn_82767BA0, does the vbtable walk `*(vbptr)+4`). Anchors all consistent:
mSigninMask@0x1c, mConnected@0x26, mScreenSaver@0x2c.

## The 2026-07-18 DO-NOT note is REFUTED (for this divergence)

That note was right that `ThePlatformMgr.<field>` global-reloc addends are
normalized away — but the base-subobject **this-adjust is a real,
un-normalized instruction** at every AddSink/RemoveSink site. The forbidden
re-anchor measures NET **+4 with zero strict losses**. The header's LAYOUT
NOTE was rewritten in the patch to supersede the old one.

## A/B evidence (twice by the worker, twice more independently by the lead — all four runs identical)

- Baseline 20080 → **20084 (NET +4, 4 gained, 0 strict regressed)**.
- Gained: `BandUI::Init`, `BandUI::Terminate`, `??1Campaign`,
  `ConnectionStatusPanel::CheckForLostConnection`.
- Fuzzy movement is confined to 4 **anonymous unmapped fn_** (no named/mapped
  fn moved): fn_82588ACC 99.9→0, fn_82588A7C / fn_82588A2C 99.9→93.9 — all
  three are 40-byte SessionMgr EH unwind funclets (Ghidra: `in_r12 -
  0x98/0xa0/0xa8` dtor thunks) whose objdiff pairing slipped (the known
  funclet over-subscription artifact); fn_82516410 80→0 is a 12-byte
  `return &lbl_82cc9f90` accessor that had been heuristically 80%-paired.
  Accepted funclet-echo slip class — not real regressions.
- Logs: `~/tmp/lever2_edit2_build.log`, `~/tmp/lever2_edit2_run2.log`;
  baselines `~/tmp/BASE_lever2.json`, `~/tmp/BASE_levers.json`.

## Landing notes for the coordinator

- Patch = the single PlatformMgr.h commit (2cd25fc4 on `levers`, or d296bf53
  on `lever2`). No .cpp edits needed (PlatformMgr_Xbox.cpp still uses the
  parked DC3 XSocial members; everything compiles untouched).
- PlatformMgr.h is widely included → land with a FULL rebuild verify, and
  expect the 4 funclet-echo slips above in the regression-lock output (they
  are anonymous/unmapped; whitelist if the lock flags them).
- Cross-lever corroboration: lever #3 independently found retail
  StorePreviewMgr is also MsgSource-lineage (virtual Object base) — retail
  manager classes kept the Wii MsgSource lineage that DC3 later flattened.
  When a DC3-lineage manager singleton shows a +4/+N receiver adjust, check
  MsgSource re-base FIRST.
