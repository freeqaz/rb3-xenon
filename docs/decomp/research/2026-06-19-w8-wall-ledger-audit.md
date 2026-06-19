# WALL ledger adversarial audit (C5–C9) — 2026-06-19 (wave-8)

**Mode:** adversarial verifier (Waypoint lesson). Baseline main `da8258f`, 8234 matched.
**Method:** falsify each "wall/deferred" verdict with COFF (`auto_03_82260000_text.obj`),
DC3 `ham_xbox_r.map`, current `report.json`, live objdiff, and the per-item dossiers.

## Headline verdicts

| item | prior ledger verdict | this audit | actionable now? |
|---|---|---|---|
| **C5 Object ATTRIBUTION_ORPHAN** | "pairing-layer fix, not a port; UAA vs QAA" | **VERDICT MISLABELED** — it is port-bound + structurally interleave-capped. The dedicated refutation doc (2026-06-11-object-dirloader-boundary-refutation.md) is **CORRECT**; the C5 *roadmap one-liner* oversimplified it. No free pin/map win. | NO (port-bound, capped) |
| **C6 OvershellSlot 8-byte shift** | "layout wall, no oracle" | **PARTIALLY CRACKABLE** — live `UpdateState` shows a **dominant +8 member-shift (18 instrs, 50%)** = a real, addressable layout delta; the enum-constant logic divergence is a separate body-port. Header currently says mSessionMgr@0x38, not 0x44 (ledger stale). | PARTIAL (layout sub-fix landable; body residue deferred) |
| **C7 Mat_NG scrambled layout** | "scrambled, defer multi-session" | **CONFIRMED WALL, but reconstruction is HALF-DONE** — matng-deferral.md has the full 34-delta retail offset table. auto-rdata vtable-dump does NOT apply (member-field, not vtable). | NO single-commit; multi-session reconstruction is specced |
| **C8 Player base-chain −4 vbase-MI wall** | "vbase-MI prefix wall (refuted unk260)" | **REFUTATION_WRONG / STALE LEDGER** — the Player +4 was **already SOLVED and LANDED** (`e64628e` SongPos 0x18→0x14 drop mPhrase, +17). C8 is a dead ledger entry. Residual §6.3 Band-head-+4 / Game-head-+4 are the only still-open Player-area deltas. | C8-core DONE; Band/Game-head = fresh recon |
| **C9 CamShotFrame funclet+ObjPtr-dtor** | "deferred funclet/inline-policy" | **dtor IS a deferred funclet class (confirmed)** — BUT the unit carries unrelated tractable body-ports (CamShotCrowd::Save 33%, CamShot::GetDurationSeconds 22.9%) that are NOT the C9 wall. | C9-dtor NO; sibling body-ports YES |

---

## C5 — Object ATTRIBUTION_ORPHAN: verdict mislabeled, refutation is sound

**Ground truth (COFF + map + DC3):**
- The three named orphans live at `InitObject@0x82733668`, `SaveType@0x82735B40`,
  `HandleProperty@0x827363D8` — and all 9 Object-class own-methods span
  `0x82733668..0x82738458` (DataDir/SaveType/Save/HandleType/HandleProperty/
  PropertyClear/RegisterFactory/Load + ~Object/ctor).
- They are physically **inside DirLoader.cpp's pin** `0x8272FF10..0x82737FE8`, NOT in
  Object.cpp's tiny `0x82737FE8..0x82738160` (0x178) sliver. That is why objdiff resolves
  `Object::InitObject` against **DirLoader.obj** and reads "Stub (High), 61 insert" — the
  target symbol exists, DirLoader.obj has no base body. This is **NOT** a "UAA vs QAA"
  mangling problem (the roadmap one-liner is wrong on mechanism).
- DC3 `ham_xbox_r.map` confirms a contiguous `obj:Object.obj` TU in DC3
  (`SetNote@825abb80 .. Load@825afd30`). So a real Object TU exists.

**Why it is still NOT a free pin/map win (the refutation doc is correct):**
The RB3 region `0x82733668..0x8273849C` is a **dense 3-way COMDAT interleave**: VA-ordered
named-class map = **9 OBJ methods among 129 anon + 4 Utl.cpp free-fns + 2 DirLoader STL +
6 EH funclets**. The 9 Object methods are isolated islands; the longest contiguous
foreign run (`0x827371d8..0x82737fbc`, ~50 anon incl. the funclet block) ≫ 8. A measured
boundary move (prior worktree `objdir-boundary`) gave **+17 nominal but FAILED the honesty
gate** (15-fn foreign run; only 3 of 54 newly-attributed were genuine Object methods).
- The interleaved foreigners are **`src/system/obj/Utl.cpp`** free functions
  (`MakeFileList`, `IsASubclass`, `ListSuperClasses`, `SubDirHashUsed`, `GetPropertyVal`,
  `ReserveToFit`, `MakeFileListFullPath`, `IsContextUsed`) — that TU is **UNWIRED**
  (objects.json only wires `rndobj/Utl.cpp`). In DC3 these are a contiguous `Utl.obj`
  (0x82599b88..); in RB3 the linker scattered them through DirLoader/Object.

**Conclusion:** C5 is **port-bound, not pairing-bound**. InitObject/SaveType/Save/
HandleType/HandleProperty all read 0% *only because they're in the wrong unit's target
split* — they can't even be measured/ported without first relocating the pin, and the
relocation can't pass the honesty gate due to the interleave. The only honest levers are
(a) **wire obj/Utl.cpp** (separate candidate, harvests the ~12 free-fns — but they too are
scattered, so likely also gate-failing), or (b) port the bodies AND accept they stay
sliver-attributed. **Net: do NOT re-attempt the boundary move; do NOT bulk-delete the
DirLoader Object map entries (real-body names). No free match here.**

---

## C6 — OvershellSlot: a real +8 member-shift sits under the body divergence

`default/OvershellSlot` = 13/19 (68%). Header `OvershellSlot.h` currently declares
`mSessionMgr // 0x38` (ledger says "ours 0x44" — STALE; the header was never edited since
scaffold `8b28623`, so the "0x44" in the roadmap referenced a since-revised state or the
wrong member).

Live `run_diff_inspect ?UpdateState@OvershellSlot@@QAAXXZ --offsets` (59.4%):
- **Dominant delta = +8, 18 instructions (50%)** — our source reads members 8 bytes
  HIGHER than retail across the board → a genuine layout delta (2 excess members/8 bytes
  somewhere below the touched region).
- Outlier `idx 395: TGT lwz r3, 0xb4(r29) vs SRC lwz r3, 0x44(r29)` and
  `idx 433: TGT lwz r3, 0x38(r29)` — mSessionMgr region.
- **BUT** also genuine logic/enum divergence: `idx 393 cmpwi 0x1e vs 0xc9`,
  `idx 416 0x13 vs 0xcc`, `idx 437 li 0x14 vs 0xce`, `idx 518 0x37 vs 0xcb` — state-ID
  constants compared against entirely different values + different control flow. This is
  the roadmap's "drop go_to_wiiprofilecreator / TheServer blocks" body divergence.

**Verdict:** the **+8 shift is an addressable struct-layout lever** (find the 2 excess
members between mState and the touched region; remove/relocate to drop everyone 8 bytes).
Closing it alone won't flip UpdateState to 100 (enum-constant body divergence remains), but
it is the prerequisite that unblocks UpdateView (76.2%) and the simpler members, and is the
honest first half. The body residue (ShowState 28%, different RTDynamicCast path) stays a
no-oracle wall. **Tractable: layout sub-fix; defer body.**

---

## C7 — Mat_NG: confirmed scramble, reconstruction half-specced, auto-rdata doesn't help

matng-deferral.md already reconstructed the **full 34-delta retail↔ours offset table** from
`SetRegularShaderConst` (deltas −188..+120, opposite signs; bool flags packed LOW at retail
0x44/0x54/0xc2/0xc3 vs scattered HIGH in our DC3 headers). Root cause: DC3 (newer) refactored
BaseMaterial — expanded packed bool flags to individual bytes and reordered members; retail
RB3 predates it (matches rb3-Wii's packed-bitfield grouping).

**The brief's hypothesis (auto-rdata vtable-dump recovers the order) is FALSIFIED** — this
is a member-FIELD reorder + bool-bitpack, not a vtable-slot-order problem. A vtable dump
recovers nothing here.

**Crackable-with-effort, not a true wall:** the reconstruction table IS the spec. An impl
agent can repack/reorder BaseMaterial/Mat per the table behind an `RB3_MATNG_LAYOUT` define,
validating against SetRegularShaderConst until all 34 deltas → 0, then whole-binary A/B.
Risk: BaseMaterial.h/Mat.h are widely included → must gate per-TU and full A/B. Multi-session.

---

## C8 — REFUTATION_WRONG (highest-value): Player +4 was already SOLVED + LANDED

The roadmap WALL ledger C8 ("Player base-chain −4 = vbase-MI prefix wall") is a **STALE,
CONTRADICTED entry**. The dedicated dossier `2026-06-11-player-plus4-layout.md` PROVED (5
independent legs: retail Performer ctor zero-store incl. skipped-mTotalBeat quirk;
mQuarantined@0x230 vs ours 0x234; MsgSource@0x240 / vbase@0x300 vs ours +4; 30+ uniform +4
pairs; rb3-Wii header agreement) that it is **NOT a vbase-MI wall** — it's a single
DC3-added member `int mPhrase` in `src/system/utl/SongPos.h` (DC3 size 0x18 → retail 0x14).

**It was LANDED:** `git log src/system/utl/SongPos.h` → `e64628e "lever: SongPos 0x18->0x14
drop DC3 mPhrase — Player+4 RESOLVED, +17 @100% (6942->6959)"`. SongPos.h now gates mPhrase
behind `SONGPOS_DC3_PHRASE`; HamSongData.cpp:52 is the 5-arg ctor. The §6.1 cmplwi residue
is ALSO closed: live `GetBandTrack` = **100%** (reads 0x260 + cmplwi correctly).

**Still-open Player-area residues** (the dossier's §6.3, never the "wall"):
- **Band head +4** — retail reads `Band::mCommonPhraseCapturer` at band+0x90; ours bakes
  band+0x94. A separate DC3-vs-RB3 delta in Band.h's first members (Band.cpp uncompiled;
  affects TUs inlining Band accessors: Player.cpp:406/518/554, GemTrack).
- **Game head +4** — retail reads `TheGame->mProperties.mEnableOverdrive` at Game+0x3d;
  ours bakes Game+0x41. Separate delta in Game's first 0x24 bytes (Game.cpp uncompiled).
These are fresh recon targets, not the C8 wall. **Update the ledger: strike C8.**

---

## C9 — CamShotFrame dtor: funclet wall confirmed, but sibling body-ports are open

`default/CameraShot` = 144/248. The C9 dtor (mFocalTarget@0xf4 — already correct;
GetCurrentTargetPosition@0xf4 reads 99.8%) is genuinely a deferred funclet-frame +
ObjPtr-dtor-inline-policy class — CONFIRMED, leave it.

But the unit has **unrelated tractable near-misses that are NOT the C9 wall**:
`CamShotCrowd::Save` 33.3% and `CamShot::GetDurationSeconds` 22.9% (low enough to be real
body divergence, oracle-portable), `CamShotCrowd::CamShotCrowd` 56.9%. These are
body-port candidates from the DC3/rb3-Wii CameraShot oracle, independent of the dtor.

---

## Ledger corrections recommended

1. **Strike C8** entirely (resolved + landed `e64628e`); replace with a "Band-head-+4 /
   Game-head-+4 fresh recon" item if tracking is desired.
2. **Reword C5** mechanism: not "UAA vs QAA pairing-layer fix" — it is port-bound +
   COMDAT-interleave-capped (no free pin/map win; the boundary move is gate-failing and
   already refuted/measured).
3. **Reword C6**: it is *partially* crackable (a real +8 member-shift sub-lever), not a
   monolithic wall.
4. **C7**: keep deferred but note the reconstruction table exists (matng-deferral.md) and
   auto-rdata vtable-dump is the wrong tool.
5. **C9**: split — dtor stays deferred; sibling body-ports (CamShotCrowd::Save,
   GetDurationSeconds) are a fresh bodyport item.
