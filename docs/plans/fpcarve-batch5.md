# Fingerprint carve — BATCH 5 outcome (2026-07-21)

Author: batch-5 carve foreman. Consumed the batch-5 seeds of
`docs/plans/fpcarve-batch4.md` (251927f0). **Baseline 20,007 → final 20,080
(= +73), 6 landings + 2 foreman map fixups, 0 lost.**

## Landings

| # | candidate (seed) | span(s) / files | delta | commits | notes |
|---|------------------|-----------------|-------|---------|-------|
| 1 | TrackerSource.cpp carve (#2 tracker tail) | 826E29E0..826E34D0 | +22 | 4ee937e6 | seed span 826E3804 was OVER-CARVED — tail ≥826E34D0 is a separate NavListNode-family TU; fixed pre-existing mispair at 0x826e3010 (was DetermineHeaderSymbolForSong, is GetIDFromInstrument) |
| 2 | UserMgr::unk2c removal (#4 BandUserMgr drift) | UserMgr.h/.cpp, Joypad_Xinput.cpp | +2 | 2f56a157 | drift direction INVERTED vs seed: we were 4 HIGH (DC3-added bool at Object-end 0x28); flips SessionMgr ctor + JoypadResetXboxPC; 53 includers, only 2 units changed, 0 lost |
| 3 | StorePanel bodyport (#5) | StorePanel.cpp only | +0 strict (fuzzy: UpdateOffers 30→84, Load 82→89.2) | f1348ab8, e9fe4523 | EnumProduct.mOfferID truly at +0x10 (String=0xc, u64 8-aligns); StoreOffer = 3 embedded StorePurchaseable @0/0x40/0x80 |
| 4 | NetSession.cpp carve (#9) | 823E6F60..823E9158 | +40 | ba545557, 19cf9111 (+fixups db0daafb, dc8bf767) | biggest win; 40 fns @100; NOT Quazal-blocked (compile-and-diff, missing Quazal .cpps irrelevant); net/ is plain /O1 (the /Od caveat applies to Platform/ only); removed stale __unwind$176976 at 0x823e8cc8 (real fn mismapped as funclet) |
| 5 | StreakTracker overflow repin (#3) | StreakTracker .text ext to 826E03D8; POT starts 826E03D8 | +9 | 7a3221aa, 78f11d7f | true boundary 0x826E03D8 not batch-4's 03D0; layout force-mult: removed 3 spurious members (map 0x7c→0x70) flipped 3 fns; POT lost NOTHING (its 20 own matches all ≥03D8) |
| 6 | NextSongPanel parent map (#7) | 1 map entry | +0 strict (parent 0→80.6% fuzzy) | 959663b8/fea88185 | see verdict below |

Non-landings:
- **Seed #1 Leaderboard::ShowGamercard = MISIDENTIFICATION.** fn_826733F0 is
  `PlayerLeaderboard::OnSelectRow` — already inside PlayerLeaderboards.cpp's
  pin, unit fully harvested (11/11). Retail body is a full 436-byte
  gamercard-privilege fn where the Wii oracle is a 3-line dev stub
  (game-code-instrumentation inversion); blocked on anon cross-TU helper
  fn_8251C960 (XPrivilegeCheck). Net 0, honest stop.
- **Seed #6 SessionMgr mediums = regalloc walls.** OnMsg(NewRemoteUserMsg)
  source is IDENTICAL to oracle; divergence is register-cascade + frame Δ+0x10
  from inlining. Permuter-class, permuter OFF. Deferred.
- **Seed #8 FingerShape**: not dispatched (flagged low priority; pin already
  TU5-correct; structural correlation, no strings).

## Seed verdict census

9 seeds: 5 productive (+73), 1 misidentified (#1), 1 wall-blocked (#6),
1 dead-vein diagnosis (#7), 1 skipped (#8). MisID rate holding at ~1/batch.

## NextSongPanel 238-near-miss verdict: DEAD as a lever

The "238 anonymous STL instantiations" premise was wrong: they are 238
**EH cleanup funclets, all exactly 40 bytes**, 230 of them children of ONE
12KB parent (`NextSongPanel::CountOrCreateExpandedDetails`, fn_82645320).
Sole divergence = the `subi r31, r12, <frame>` immediate: our parent frame is
0x860 vs retail 0x880 (9 TGT_ONLY spill slots, 502-insn regswap cascade).
No STL header idiom, no mispair. Only path: a faithful body-port of the parent
that happens to land MSVC's regalloc → would cascade +230 strict as a
byproduct. High-value/high-risk single target, NOT a wave.

## Friction census (foreman traps — READ BEFORE BATCH 6)

1. **`resolve_json_union` RESURRECTS DELETIONS — hit 3 times this batch.**
   The land.sh union resolver keeps any key present on either side, so:
   (a) a worker's map DELETION (stale __unwind$176976) was undone by the
   rebase union — main lost 1 match until re-deleted; (b) it was undone AGAIN
   by the next landing whose branch predated the fix; (c) on a genuine
   conflict it "kept theirs" = the branch's STALE pre-rebase value for
   0x826e3010, which would have reverted landing-1's mispair fix.
   **SOP until tooling is fixed:** after every land.sh run, grep the rebased
   map for every key that any prior landing in the batch deleted or re-pointed,
   and re-verify against main's intended value. Proper fix: make the resolver
   3-way (respect deletions/changes relative to merge-base).
2. **size_order_automap WEAK-tier mis-sizing**: it guessed `GetPlayerCount`
   (64 vs 56B) as a `??_G`; thunks/dtors needed manual vtable-read ID. WEAK
   stays never-emit; expect 1-2 manual IDs per small TU.
3. Post-landing main full builds continue to re-baseline slightly vs worker
   worktree measurements (NetSession worker's in-tree +40 landed as +39 until
   the resurrected map entry was re-deleted — the loss was trap #1, not noise).

## Transferable cracks (new this batch)

- **cmpw operand order** follows source operand order (`pPlayer->GetTrackType() == ty`).
- **Bool normalize idiom**: explicit `? 1 : 0` → `clrlwi;subic;subfe`.
- **Branch polarity**: flip `if (x)` vs `if (!x)` to match retail's guard direction (UpdateOffers mShowTestOffers).
- Dropping a `bool matched` flag in favor of re-testing `it != end` frees a callee-saved reg.
- DC3-added members strike again (UserMgr::unk2c) — when a shared engine class
  drifts +4, grep DC3 for a member rb3-Wii lacks before padding.

## Batch-6 seeds

1. **NavListNode-family TU [0x826E34D0, ~0x826E3808)** — ~6 small fns,
   vtables 0x820f1400/0x820f1418. NO oracle (rb3-Wii lacks it; DC3's
   lazer/meta_ham version is DC-specific). From-scratch Ghidra RE carve;
   existing `??_GNavListNode` map entry at 0x826e37b8 is legitimate.
2. **CountOrCreateExpandedDetails body-port** (fn_82645320, 12KB, now 80.6%
   fuzzy) — the +230-funclet cascade prize. 477 delete-insns suggest missing
   branches vs oracle. High-risk, single-fn, rank accordingly.
3. **Profile::GetPadNum virtual-base dispatch** — retail calls it virtually
   through a Profile vbase subobject; ours is non-virtual direct bl. Blocks
   StorePanel Load tail + EnumerateOffers and plausibly every GetPadNum
   callsite. Foundational header change, needs gated whole-binary A/B.
   (Candidate for the missing-virtual force-multiplier scanner.)
4. **PlatformMgr Object-base offset** — retail AddSink receiver is
   ThePlatformMgr+4, ours +0. Layout/inheritance fix, gated A/B.
5. **StorePreviewMgr size 0x58→0x60 + int-arg ctor** — unknown 2 members;
   ripples to MusicLibraryStore/Lit_NG.
6. **NetSession residuals**: Handle dispatcher 57% (BEGIN_HANDLERS macro
   wall), ctor 92% regswap, 10× `??__F` guard dtors 92.5% (objdiff funclet
   over-subscription — tooling, not source).
7. **StreakTracker residuals**: LocalEndStreak 26%, ConfigureTrackerSpecificData
   75%, Poll_ — structural static-Symbol-guard + FPR cascades.
8. **FingerShape** (carried, still low priority).

## Vein health — honest flag for the coordinator

Per-batch strict yield: batch-2 +351 → batch-3 +212 → batch-4 +221 →
**batch-5 +73**. The named-seed vein is THINNING hard: batch-5's yield was
2 carves (TrackerSource, NetSession = +62 of the +73) and the residual seeds
are predominantly walls (regalloc, dispatcher macros, funclet tooling) or
foundational header gambits, not clean gap-carves. Batch-4's seed list was the
first with a misidentified #1 seed AND a dead #7 lever; batch-5 confirmed both.
**Recommendation: identification must go back to first principles over the
remaining anon mass** — fp2_span maximal-run census + Ghidra string-family
sub-splits over the unpinned .text regions (the batch-2 method), rather than
milking the seed tail. The three foundational levers (#3/#4 above + the
missing-virtual scanner from memory) are the other credible volume source.
