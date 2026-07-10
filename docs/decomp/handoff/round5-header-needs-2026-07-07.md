# Round-5 harvest — header-need follow-ups (2026-07-07)

The round-5 near-miss wave (playbook: `docs/decomp/playbooks/nearmiss-harvest.md`)
landed its .cpp wins cleanly; per lane discipline, header-level fixes were
*reported, not applied*. Each item below is a *single* shared-header change to be
done as its own whole-binary-A/B-gated task. Listed by expected value.

> **STATUS (2026-07-10):** all four executed after a 5-agent deep-recon pass.
> #1 Game.h LANDED independently by wave-5 (`3edcc60`) — deeper findings +
> remaining Game drift in `game-layout-followups-2026-07-10.md`. #3
> StandardStream and #4 Joypad LANDED together (`b5b28fe`, +7 strict, A/B ×2
> clean) — NOTE the recon **refuted** #3's "marker region" hypothesis (both
> missing words sit at 0x160/0x164; the marker region already matched) and
> **corrected** #4 (our stride was right only via two canceling errors; fix =
> bool repack **plus** restoring dc3's 2 tail ints to hold sizeof 0xd4). #2
> Data.h in flight.

## 1. Game.h — drop the Wii-only `DiscErrorMgrWii::Callback` base (BIG cascade)

**Claim:** retail-360 `Game` has `mProperties` at **0x2c**; ours compiles it at
0x30 — uniform −4 on all `Game::Properties` accesses. Prime suspect: the third
base `DiscErrorMgrWii::Callback` (vptr, 4 bytes, `src/system/os/DiscErrorMgr_Wii.h`)
is rb3-Wii drift retail lacks (4 + sizeof(Hmx::Object)=0x28 = 0x2c exactly).

**Evidence (two independent witnesses, lane C):**
- `SongDB::GetPhraseExtents` residual: 2× `lbz 0x2c` (InTrainer) vs our 0x30 —
  otherwise normalized-clean after the .cpp win (landed at ~99.6).
- `GemManager::IsSpotlightGem`: `lbz 0x2f` (AllowOverdrivePhrases) vs our 0x33,
  plus `lbz 0x2c` vs 0x30 — otherwise clean (landed at ~98.7).

**Blast radius:** removing the base also removes the `DiscErrorEnd()` virtual /
vtable slot — audit every `DiscErrorMgrWii` reference and Game vtable user.
Payoff: converts the two functions above to strict AND likely lifts every other
Game::Properties-touching near-miss binary-wide.

## 2. Data.h — `SortNodes(int)` → `SortNodes()` (known +6 cascade)

DC3 added the int param; rb3-Wii `Data.h:477` is no-arg. Sole residual in
`CharBoneDir::GetContextFlags` (99.38) is one extra `li r4,0x0` before the call.
Call sites to sync: `DataArray.cpp:452` (definition), `DataFunc.cpp:1306`,
`Utl.cpp:300`, `CharBoneDir.cpp:191`. This is the same finding as the 07-01
bodyport recon ("+6 cascade, shared header") — now with a second witness.

Related, same file, from round 4: `Execute(bool fail=true)` → Wii-era
`Execute()` unblocks `AppChild::Poll` (97.9). Consider doing both Data.h edits
in ONE gated A/B (audit `Execute(false)` call sites first).

## 3. StandardStream.h — +8 layout (mAccumulatedLoopbacks + 4B in marker region)

Retail `StandardStream` is 8 bytes bigger before `mPollingEnabled` (0x168 vs
our 0x160). One known piece: **`float mAccumulatedLoopbacks` at retail 0x164**
(rb3-Wii `StandardStream.h:153-154`). Witness: `StandardStream::Init` (99.4)
residual = missing `stfs f30(0.0f), 0x164` + `lbz 0x168 vs 0x160`. The other
+4 is in the 0xf4–0x160 marker region (Wii `Marker` is 0x14 vs our 0x10; Wii
has no `mJumpInstances`) — needs its own recon before applying. Companion .cpp
edit once landed: `mAccumulatedLoopbacks = 0.0f;` after `ClearLoopMarkers()`
in `Init`.

## 4. Joypad.h — `JoypadData::mType` 0x74 → 0x6c (stride 0xd4 confirmed)

Retail reads `mType` at **0x6c**, element stride 0xd4 (retail Joypad.s:
`mulli r10,r3,0xd4; lwz r11,0x6c(r10)` vs JoypadType constants; retail never
touches 0x74). Ours has 8 extra bytes in the 0x5c–0x70 mask-block region.
Witness: `UsbMidiGuitar::Poll` (banked at 99.9). Affects every
JoypadData-touching TU — full A/B mandatory. Same TU also wants retail
ProGuitarData bitfield members for the pgRaw[0xc]/[0xb] extract order (separate,
smaller finding).

## Also banked this round (not header-gated, permuter-class residuals)

- `Synapse::ProcessInPlace` — closed to strict in the composed A/B.
- `CharIKFingers::CalculateHandDest` 95.8 → 99.7 (3-cycle FPR result rotation).
- `UsbMidiGuitar::Poll` 99.16 → 99.9 (see #4).

Wall verdicts from this round are registered in
`scripts/harvest/nearmiss_verdicts.json` (auto-excluded from future pools).
