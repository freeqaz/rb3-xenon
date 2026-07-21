# DEFER handoff — StorePreviewMgr 0x58→0x60 layout (batch-6 lever #3)

2026-07-10 foundational-levers wave (levers branch). Verdict: **DEFER** — retail
ground truth fully established, but the fix is unmeasurable/net-≤0 today.
No edits made; baseline 20080 stands.

## Retail ground truth (CONFIRMED, Ghidra :8002 default_tu5.xex-c5a170)

- Retail `new StorePreviewMgr` size = **0x60** (StorePanel::Load idx 61:
  `li r3, 0x60` vs our `li r3, 0x58`).
- Retail ctor `fn_827B1FC8` takes a hidden **int arg = 1** — the MSVC
  virtual-base "most-derived flag" (`li r4, 0x1` before the ctor bl).
- Lineage: retail `StorePreviewMgr : public MsgSource`, with
  `MsgSource : public virtual Hmx::Object`; the Hmx::Object **virtual base sits
  at +0x38** (0x38 + 0x28 = 0x60). Our header (and DC3's) use direct
  `: public Hmx::Object` — DC3 refactored the MsgSource base away.
- The "2 extra members / +8" seed framing is actually virtual-inheritance
  overhead (MsgSource vbptr+lists+mExporting = 0x18 head) minus a SMALLER
  member region. Retail is the simpler pre-TexMovie variant (retail
  PlayCurrentPreview fn_827B19B8 has no TexMovie/attenuation logic).
- mNetCacheLoader@0x28 and mDownloadQueue@0x2c are identical in both layouts —
  which is why the 3 pinned fns already match.

## Why DEFER

1. The unit's only 3 pinned target fns (IsDownloadingFile,
   AllowPreviewDownload, AddToDownloadQueue) are **already 100%** (the 99.9%
   was reloc/ICF noise). Ctor/dtor/Poll/SetCurrentPreviewFile/
   PlayCurrentPreview/GetLastFailure are NOT in target_symbol_map → a layout
   fix registers zero strict.
2. The only mapped consumer, StorePanel::Load (89.2%), is capped by the
   UNRELATED Profile::GetPadNum vbase-dispatch divergence (lever #1) — best
   case here is a fuzzy bump ~89→~93, never a strict flip. The other caller
   (MusicLibraryStore ctor) is unmapped.
3. Hitting 0x60 forces a full retail-form port: drop DC3-added members
   (mTexMovie/mAttenuation/mLoopForever/mLastFailType/mHasFailure), re-base on
   MsgSource, rewrite the cpp bodies + callers — all unmapped, unmeasurable.
4. Keeping current members + MsgSource base = 0x70 (not 0x60) and shifts
   mNetCacheLoader off 0x28 → would REGRESS the 3 currently-100% fns.

## Revisit recipe

1. First pin StorePreviewMgr ctor (0x827b1fc8) + Poll + SetCurrentPreviewFile
   in target_symbol_map so the layout becomes measurable.
2. Then port to the simpler retail form (MsgSource base, no TexMovie block).
3. StorePanel::Load + MusicLibraryStore::ClearPreview are genuine body-port
   candidates, but their blockers are Profile::GetPadNum (lever #1) and
   preview-clear logic — not this layout.

## Cross-lever corroboration

Retail manager classes are **MsgSource-lineage** (virtual Hmx::Object base) —
independent confirmation of the lever-#2 PlatformMgr +4 hypothesis (retail
PlatformMgr : MsgSource, not : Hmx::Object).
