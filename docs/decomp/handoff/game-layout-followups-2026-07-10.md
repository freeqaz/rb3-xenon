# Game (band3) retail layout — post-base-drop follow-ups (2026-07-10)

The `DiscErrorMgrWii::Callback` base drop landed in `3edcc60` (wave-5 g3-gem,
+8). Independently, a deep recon agent reconstructed the FULL retail `Game`
layout from first principles (RTTI walk + ctor/dtor decompile + 20+ pinned-asm
member witnesses). This doc records what that recon found BEYOND the landed
change, so the remaining Game-unit near-misses have a ready evidence base.

## Retail ground truth (independently verified)

- RTTI: `.?AVGame@@` TypeDescriptor at VA 0x82c4496c; ClassHierarchyDescriptor
  0x821df218, numBaseClasses=4 = {Game, BeatMasterSink(mdisp 0),
  Hmx::Object(mdisp 4), ObjRef(mdisp 4)}. **No Callback base anywhere in the
  retail image** (grep of all 1,387 `?AV` descriptors — no "DiscError" string
  exists).
- Exactly 2 CompleteObjectLocators → 2 vptrs (offset 0 → vftable 0x820da14c,
  offset 4 → 0x820da0f4).
- Retail ctor 0x8265E2E0 / dtor 0x8265D208 (pinned Game.cpp cluster): store
  exactly 2 vptrs; Properties constructed at this+0x2c.
- **Retail sizeof(Game) = 0x164.** Ours after the base drop = 0x168.
- The pre-fix divergence was NOT a uniform −4: it was +4 through mTimeOffset,
  +8 from mTime (base removal also collapses the 4-byte pad before the
  8-aligned Timer), +12 from mLoadState on. The base drop fixed the +4/+8
  zones completely (mProperties..mLastPollMs now byte-exact).

## Follow-up 1 — DiscErrorEnd phantom vtable slot (latent, low priority)

`3edcc60` kept `virtual void DiscErrorEnd();` as a plain Game virtual. With
the base gone, MSVC appends it as a NEW slot at the end of the primary
(BeatMasterSink) vtable — retail's primary Game vtable has no such slot.
Harmless today (Game vtable is unpinned .data, nothing calls it), but it will
bite if/when Game's vtable or any vcall-indexed dispatch gets pinned. Clean
fix (from the recon dossier): delete the declaration (Game.h), the definition
`void Game::DiscErrorEnd() { unk6b = true; }` (Game.cpp:446 region — unk6b
member STAYS, it is retail byte 0x77 and still written at ctor init +
ClearState), and the two `TheDiscErrorMgrWii.{Add,Remove}Callback(this)` calls
(codegen no-ops). The landed change instead widened the stub to `void*` —
works, but leaves the dead virtual.

## Follow-up 2 — the +4 tail zone (mMusicSpeed onward)

Everything from `mMusicSpeed` to the end is still +4 vs retail. Cause: our
`mMuckWithPitch` byte (+pad) before `mMusicSpeed` does not exist there in
retail — retail ctor stores 1.0f at 0xb8 immediately after mLastPollMs@0xb4.
Fixing this (delete or relocate mMuckWithPitch) makes the whole tail
byte-exact and is expected to close: `Game::CanUserPause` (residual =
mDisablePauseMs +4), most of `Game::HandleAudioLoad`, `Game::IsLoaded`
(mLoadState/mTrackerManager +4), `Game::SetGameOver`. Retail tail witnesses:
unkd8 0xdc, unk120 0x124, unk124 0x128, mTrackerManager 0x148, mDisablePauseMs
0x150, unk154 vector 0x158–0x160, sizeof 0x164. Needs one retail witness for
where (or whether) mMuckWithPitch lives before applying — check retail
SetPitchMucker / screensaver handler when located.

## Follow-up 3 — bool-block reorder (0x74–0x79)

Retail bool block 0x74–0x79 has 6 bools; ours has 8 (our unk6c/mPauseTime are
not in retail's block — retail mRealtime@0x78/unk6f@0x79 vs ours 0x7a/0x7b).
Does NOT shift anything (8 bools fit retail's 6+2 pad; mTimeOffset stays
0x7c). Witness: HandleAudioLoad idx42/77 (+2 residual post-base-drop).
Candidate retail slots for the displaced pair: 0x7a/0x7b or 0xbe/0xbf after
unkb9 — or they may simply not exist in retail (dev-only fields; ctor
zero-stores only 6 bool bytes). Needs per-member retail witnesses before the
reorder.

## Follow-up 4 — hygiene

- Refresh stale Wii-era `// 0xHEX` comments in Game.h for
  mProperties(0x2c)..mLastPollMs(0xb4) — now retail-true after the base drop;
  feeds struct_db/lookup_struct_offset. Note members from mMusicSpeed on are
  still +4 pending Follow-up 2.
- `Game::GetScoringTracks` reads 0% at 32 bytes despite iterating
  mAllActivePlayers — smells like a pairing/ICF artifact, worth one look.

Full dossier: recon transcript artifacts (2026-07-10). Related:
`docs/decomp/handoff/round5-header-needs-2026-07-07.md` §1 (superseded by
this doc for Game).
