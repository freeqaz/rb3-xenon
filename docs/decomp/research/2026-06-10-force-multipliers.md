# Force-multiplier lever hunt — 2026-06-10 (read-only research handoff)

**Baseline:** main @ 20590dd, fresh `build/45410914/report.json`, **6851/65544 matched**
(`match_percent_normalized == 100`). All percentages below are *report normalized* (the
official metric), all diffs re-verified live against the existing full-build objects
(no rebuilds, no edits).

**Method:** fresh `true_progress` worklist 85–100 (956 HAS_REAL fns), fresh
`wall_classify` routing, fresh `member_delta_finder2` + `inline_policy_finder` runs,
plus a **custom vcall-slot sweep** over all 353 *named* 85–99.9 fns (pattern:
`diff_arg` on `lwz rA, IMM(rB)` where `rB` was loaded from `0x0(rX)` within 4 insns
and `mtctr rA` follows — i.e. vtable-slot loads feeding indirect calls). The sweep
found **36 slot-delta hits the classifier missed entirely** (artifacts in
`~/tmp/fm-research/`: `worklist_85_100.json`, `routed_85_100.json`, `mdf2_85_100.json`,
`ipf_85_100.json`, `vtable_sweep.json`).

**Precedent commits studied:** `30a4ae8` (Rnd 8 virtuals, +10), `dc080dd` (FileMerger
`Merger::filler`, +7), `9150f3c` (TexRenderer rev12/13, +6), `2d82a94` (PostProc
rev-skew, +2), `60eabed` (ColorPalette pad, +1). The two big levers below are exact
repeats of the `30a4ae8` shape (DC3-added virtual → slot shift in callers → gate
`virtual` behind `HX_NATIVE`), localized the same way (machine-code slot anchors in
callers, not source diffing).

---

## Pool-state re-confirmation (task step 2)

- **inline_policy_finder** (band 85–100, 353 scanned): **33 candidates, every one n=1.**
  STILL TAPPED — verdict unchanged from wave close. Only "actionable=YES" rows are
  single-caller DECL-only/INLINE flips (UISlider::Init, MakeShortAng,
  FileStream::DeleteChecksum, a `Normalize` outline) — each ≤+1, not force-multipliers.
- **member_delta_finder2** (`--tp` fresh, HAS_REAL): **8 class candidates,
  MEMBER_DELTA=0 actionable / SIZED_VECTOR=1 / VBASE_WALL=6 / UNKNOWN=1.** The
  "SIZED_VECTOR VocalPlayer −4 @0x278 ×5 fns" + "VBASE Player −4 @0x260 ×4 fns" rows
  are actually ONE shared root cause (Lever 4 below). The tool did **not** error;
  default bucket now correctly HAS_REAL.
- **wall_classify** on the fresh 956-fn worklist routes: DEFER_DEEP 560 / DEFER_VBASE
  182 / PERMUTE 94 / UNKNOWN 73 / MEMBER_DELTA_CANDIDATE 21 / AT_LIMIT 14 /
  INLINE_POLICY 12, and **0 VTABLE_DIVERGENCE** — which is wrong, see Tooling gaps.

---

## LEVER 1 — RndAnimatable: drop DC3-added `virtual OnListFlowLabels` (est +5 direct, +5–10 w/ cascade; HIGH confidence)

**Class:** `RndAnimatable` (`src/system/rndobj/Anim.h:58`):
`virtual DataNode OnListFlowLabels(DataArray *) { return 0; }` — declared **last** in
the own-vtable slice. Present in DC3 (`../dc3-decomp/src/system/rndobj/Anim.h:58`),
**absent from rb3-Wii** (`../rb3/src/system/rndobj/Anim.h` ends the virtual list at
`ListAnimChildren`). DC3 added it for the expanded Flow integration.

**Machine-code slot anchors** (callee primary vtable = RndAnimatable own slice +
derived own slice; `EventTrigger::Trigger` is the first EventTrigger-own slot):

| caller fn | unit | report % | tgt slot | our slot |
|---|---|---|---|---|
| `?PlayIntro@VocalTrackDir@@UAAXXZ` (×2 sites) | default/VocalTrackDir | 99.96 | 0x24 | 0x28 |
| `?SpotlightPhraseSuccess@VocalTrackDir@@UAAXXZ` | default/VocalTrackDir | 99.97 | 0x24 | 0x28 |
| `?CanChat@VocalTrackDir@@QAAX_N@Z` | default/VocalTrackDir | 99.96 | 0x24 | 0x28 |
| `?TrackReset@VocalTrackDir@@UAAXXZ` | default/VocalTrackDir | 99.98 | 0x24 | 0x28 |
| `?StartAnim@LightPreset@@UAAXXZ` | default/LightPreset | 99.99 | 0x24 | 0x28 |

All five call `EventTrigger::Trigger()` through a plain object pointer (e.g.
`LightPreset::StartAnim` at `src/system/world/LightPreset.cpp:521-523`:
`(*it)->Trigger()`); **no vbase adjust on the path** — the `this` is the list element
directly. Slot arithmetic: RndAnimatable own virtuals ours = Loop, StartAnim, EndAnim,
SetFrame, StartFrame, EndFrame, AnimTarget, SetKey, ListAnimChildren,
OnListFlowLabels = **10 slots → Trigger @0x28**; retail = 0x24 → **9 slots**. The only
member of the list absent from rb3-Wii is OnListFlowLabels.

**Why low-risk:** it is declared LAST in the slice, so dropping it changes **no**
RndAnimatable-own slot (StartAnim/SetFrame/... callers keep their offsets); only
*derived-class own slices* shift −4 — and any currently-matched fn vcalling a derived
slot would already disagree with retail if retail lacked the slot, so by construction
the matched set can't regress on slot offsets (same self-consistency argument that
held for the Rnd +10 lever). Note `RndGroup : public RndAnimatable, ...`
(primary base) — RndGroup-own slots shift too.

**Proposed edit** (same idiom as `RND_DC3_VIRTUAL`, rndobj/Rnd.h:28-36):
in `src/system/rndobj/Anim.h` gate the `virtual` keyword on OnListFlowLabels behind
`HX_NATIVE`; keep the method (it is HANDLE'd: `src/system/rndobj/Anim.cpp:35`
`HANDLE(list_flow_labels, OnListFlowLabels)` — the macro calls it directly, no vtable
needed). `RndPropAnim::OnListFlowLabels` (`src/system/rndobj/PropAnim.cpp:723`)
becomes a shadowing non-virtual — gate its declaration's `virtual` the same way.

**Cascade pool:** 42 `->Trigger();` sites across `src/system` + `src/band3`
(StreakMeter, TrackPanelDir, GemTrack, EventAnim, LabelNumberTicker, Splash,
PropAnim, EventTrigger.cpp itself …) — callers below 85% / anon will move; run the
refill loop after landing. **Decisive pre-check for the agent:** confirm one retail
vcall at slot 0x20 (`ListAnimChildren`) or lower matches ours (brackets the insertion
at exactly slot 9).

---

## LEVER 2 — RndDrawable: devirtualize `Draw()` + drop DC3 `DrawShadow` (est +3 direct, +4–8 w/ cascade + unblocks body-ports; MED-HIGH confidence)

**Class:** `RndDrawable` (`src/system/rndobj/Draw.h`). Two independent anchors prove
retail's own-vtable slice has **two fewer pre-CollideList slots** than ours:

| caller fn | unit | report % | call (source) | tgt slot | our slot |
|---|---|---|---|---|---|
| `?DrawShowing@RndLine@@UAAXXZ` | default/Line | 99.98 | `mMesh->DrawShowing()` (Line.cpp) | 0x14 | 0x18 |
| `?DrawMeshVec@SpotlightDrawer@@MAAX…` | default/SpotlightDrawer | 99.95 | mesh `DrawShowing` | 0x14 | 0x18 |
| `?RenderSheet@NgSpotlightDrawer@@IAAXPAVSpotlight@@@Z` | default/SpotlightDrawer_NG | 99.96 | mesh `DrawShowing` | 0x14 | 0x18 |
| `?CollideList@RndGroup@@UAAX…` | default/Group | 92.46 | `(*it)->CollideList(seg, colls)` (Group.cpp) | 0x24 | 0x2c |

Our slice: UpdateSphere 0, GetDistanceToPlane 1, MakeWorldSphere 2, CamOverride 3,
Mats 4, **Draw 5 (0x14)**, DrawShowing 6 (0x18), DrawShadow 7, ListDrawChildren 8,
CollideShowing 9, CollidePlane 10, CollideList 11 (0x2c). Retail: DrawShowing=5
(0x14), CollideList=9 (0x24) → −1 in [0..5], −1 more in [6..10].

**Which two:**
1. **`Draw()` is NON-virtual in retail.** Smoking gun: retail
   `?DrawShowing@RndDir@@UAAXXZ` (target-only, 0% — we don't compile it; unit
   default/Anim) iterates draw children and emits **`bl fn_823F3A80` directly**
   (direct call, r3 = child) where the source (`(*it)->Draw()`, cf. our
   `src/system/rndobj/Group.cpp:219`) would emit a vcall if Draw were virtual. RB3-era
   Milo `Draw()` is the non-virtual cull-wrapper that calls virtual `DrawShowing()`;
   DC3 made it virtual. (rb3-Wii's header says `virtual void Draw()` — the machine
   code overrides the header here; Wii is the dev build / different platform branch.)
2. **`DrawShadow(const Transform&, float)` is DC3-only** (`Draw.h:54`,
   `../dc3-decomp/.../Draw.h:54`). rb3-Wii has `DrawShowingBudget(float)` in that
   region instead; retail-360 appears to have **neither** (otherwise CollideList
   would read 0x28, not 0x24).

**Proposed edit:** `RND_DC3_VIRTUAL`-style gate on `Draw()` and `DrawShadow()` in
`src/system/rndobj/Draw.h` (+ gate the `virtual` on any overrides:
grep `::Draw()` / `DrawShadow` overrides — e.g. RndDir, RndGroup, RndMovie?, UI
classes). Keep both methods callable non-virtually.

**Interaction warning (the one real risk):** `UIComponent : public RndDrawable, …`
(primary base) — UI sites currently mismatch by **+4** (PanelDir::Entering tgt 0x3c /
ours 0x40; UIListSubList::Draw 0x34/0x38; Splash::Draw 0x114/0x118). Dropping 2
RndDrawable slots turns that +4 into **−4**, i.e. retail UIComponent has ~1 own
virtual we lack (rb3-Wii UIComponent has SetTypeDef / ResourceCopy / CopyMembers /
Update that our DC3-derived header dropped — `../rb3/src/system/ui/UIComponent.h:55-68`).
The whole-binary A/B will surface this; if UI units regress, pair the lever with
adding ONE retail UIComponent virtual (likely `Update` or `ResourceCopy`, rb3-Wii
order) — but do NOT attempt the full UI MI reconstruction (refuted quick-fix;
see docs/plans/ui-base-layout-reconstruction.md — the anchors above are fresh input
for that doc). If pairing is needed, that's still a 2-file gate, not a reconstruction.

**Extra upside:** with non-virtual `Draw()`, currently-impossible body-ports compile
correctly (our `(*it)->Draw()` emits a vcall where retail has `bl` — e.g.
`RndDir::DrawShowing` target @0% in default/Anim becomes portable).

---

## LEVER 3 — PropKeys: drop DC3-added `virtual RemoveRange(float,float)` (est +1–3; HIGH confidence)

**Class:** `PropKeys : public ObjRefOwner` (`src/system/rndobj/PropKeys.h:114`) — NO
virtual inheritance anywhere in the chain (clean). `virtual int RemoveRange(float,
float)` is at own-slot 10 (0x28); present in DC3 (`PropKeys.h:114` + per-Keys
overrides at :306/:353/:401/:452), **completely absent from rb3-Wii** (zero grep hits
in `../rb3/src`).

**Anchor:** `?ValueFromIndex@RndPropAnim@@QAA_NPAVPropKeys@@HPAVDataNode@@@Z`
(default/PropAnim, **99.97**, size 1156) — **8 distinct PropKeys vcall slots all
uniformly +4** (tgt 0x2c→0x58 vs ours 0x30→0x5c; the As\*Keys/\*At family), and **no
shifted slot below 0x2c** → insertion bracketed at exactly slot ≤10 = RemoveRange
(slots 0–9: dtor?, RefOwner, Replace, StartFrame, EndFrame, FrameFromIndex, SetFrame,
CloneKey, SetKey, RemoveKey).

**Proposed edit:** gate `virtual` on RemoveRange in PropKeys + the four Keys<>
overrides behind HX_NATIVE; `RndPropAnim::RemoveRange` (`src/system/rndobj/PropAnim.cpp:491`,
caller at :77) keeps working via direct calls (it's also DC3-only —
no rb3-Wii equivalent — so consider gating the `remove_range` handler entry too if
the PropAnim Handle near-miss complains). Other beneficiaries: PropAnim
`?AdvanceFrame` 87.22 / `?FindKeys` 98.12 may have additional diffs; PropKeys unit
has 34 unmatched anons that the reveal refill can collect after the slot fix.

---

## LEVER 4 — Player base-chain −4 (Player + VocalPlayer + Singer ≈ 10 fns; MEDIUM confidence, needs bracketing)

**Pattern:** our offsets are uniformly **+4 above retail** for everything ≥ ~0x260 in
`Player` (and therefore in `VocalPlayer`/`Singer`-visible accesses too):

- `?GetBandTrack@Player@@QBAPAVBandTrack@@XZ` (band3/game/Player): retail
  `lwz r11, 0x260(r3)` vs ours `0x264` — **plain `this`, NO vbase adjust** (verified
  in the live listing; this is NOT the CameraShot frame-recovered/vbase-adjusted
  shape). Also has an independent `cmplwi` vs `cmpwi` diff (member typed int vs
  pointer somewhere — fix opportunistically).
- `?GetEnabledStateAt@Player@@QBA?AW4EnabledState@@M@Z`: +4 at 0x280/0x288/0x290/0x294
  (ours 0x284/0x28c/0x294/0x298).
- `?SetMultiplierActive@Player@@UAAX_N@Z`, `?Rollback@Player@@UAAXMM@Z`: +4 @~0x260.
- VocalPlayer (5 fns: `Rollback` 89.88, `OnGameOver` 99.91, `ChangeDifficulty` 99.84,
  `GetSpotlightPhraseID` 99.91, `GetNextPhraseMarker` 99.67): −4 evidence at
  0x384/0x390/0x278 (mdf2 row 1).
- `?GetFrameMatchType@Singer@@QAAHXZ` 99.91: retail reads
  `VocalPlayer+0x390` (= mVocalParts; retail asm at fn_826D99B0:
  `lwz r10, 0x0(r3); lwz r10, 0x390(r10)`), ours compiles 0x394 region.

**Root cause hypothesis:** ONE 4-byte over-size in our `Player` layout between
0x228 (`mParams`) and 0x260. `src/band3/game/Player.h:207-240` is textually identical
to `../rb3/src/band3/game/Player.h` — so the divergence is a *type-size mapping* in
our 360 port of that block (suspects: `std::vector<Extent VECTOR_SIZE_SMALL> unk260`
— `VECTOR_SIZE_SMALL` expands to `, unsigned short` on Wii (8-byte sized vector) but
empty/0xc in our matching branch (`src/system/utl/VectorSizeDefs.h:17,24`); or
`String mPlayerName @0x23c`; or padding after `bool mIsInCoda`). NOTE: retail mangled
names are 0 arity-3 vectors binary-wide (tools/vector_arity.py), so if unk260 is the
culprit retail's member there is an 8-byte *non-std::vector* struct — replace with a
gated 8-byte placeholder, don't touch STLport (sized-vector tree-wide is REFUTED −504).

**Recipe for the agent:** bracket the boundary — find matched(100%) Player fns
touching 0x228..0x25c (offsets agree = below boundary) vs the shifted ≥0x260 set;
shrink exactly one member at the boundary by 4 (gated `// (DC3 port artifact)`
comment); whole-binary A/B. Watch the `Performer` vbase (pilot tagged
SetMultiplierActive VBASE) — but the plain-`this` access pattern above says the delta
is in Player's own block, not vbase displacement. If A/B nets negative à la CameraShot
(`fe0aaaa`, −3), revert and record.

---

## LEVER 5 — target_symbol_map off-by-one repairs (Rnd OnClearColor family; +2–3 trivial; HIGH confidence)

`?OnClearColorR/G@Rnd@@…` (default/Rnd, both 99.80) are NOT member deltas: retail
`Rnd::mClearColor` is at **0x2c exactly like ours**. The retail .s
(`build/45410914/asm/Rnd.s` @0x82400208/0x220/0x238/0x250) shows four consecutive
20-byte accessors reading 0x2c (R), 0x30 (G), 0x34 (B), and the Packed reader using
all three; but `scripts/target_symbol_map.json` names **0x82400220 (reads 0x30=G) as
OnClearColorR** and 0x82400238 (=B) as OnClearColorG — an off-by-one name transfer.
Fix: remap `0x82400208→OnClearColorR`, `0x82400220→OnClearColorG`,
`0x82400238→OnClearColorB` (check what currently claims 0x82400208 / where our
OnClearColorB pairs). Each repaired pairing is a free +1 since the bodies already
byte-match at the right addresses.

**Generalizable sweep:** any MEMBER_DELTA candidate whose *neighbor* target fn
contains our exact immediate is this artifact class. A 20-line checker over
`vtable_sweep.json`/mdf2 outputs + the .s would catch the family.

---

## LEVER 6 — Rnd::Terminate: gate DC3-added `DOFProc::Terminate()` call (+1; HIGH confidence)

`?Terminate@Rnd@@UAAXXZ` (default/Rnd, 97.78): the ONLY structural diff is ours has an
extra `bl ?Terminate@DOFProc@@SAXXZ` between `RndMultiMesh::Terminate` and
`RndMat::Terminate` that retail lacks (clean single `insert`). DOFProc is the NG/DC3
depth-of-field subsystem. Gate the call in `src/system/rndobj/Rnd.cpp` Terminate
behind `HX_NATIVE` (cross-check rb3-Wii Rnd::Terminate has no DOFProc). Remaining
diffs in that fn are name-resolution noise (`except_data_82403FE0` vs
`RndOverlay::Terminate` label artifact) — verify report-normalized flips before
counting it.

---

## LEVER 7 (defer-grade) — PlatformMgr head layout: retail `mGuideShowing` @0x20 vs ours 0x54

`?Poll@JoypadClient@@AAAXXZ` (default/JoypadClient, 94.41): retail
`lbz r11, 0x20(ThePlatformMgr)` vs ours `0x54` for `GuideShowing()`
(`src/system/os/JoypadClient.cpp:170`; inline accessor `PlatformMgr.h:134`). Our
DC3-derived `PlatformMgr` head block (`src/system/os/PlatformMgr.h:53-63`:
XSocial bools @0x2c, `XOVERLAPPED mOverlapped` @0x30, unk4c, masks @0x50/0x54) is
DC3-SmartGlass-era; retail packs `mGuideShowing` right after Hmx::Object (0x1c→0x20).
Only inline accessors bake offsets (GuideShowing/IsConnected/ScreenSaver/SignInMask),
so visible gain is small (+1–3) and reconstruction needs 3–4 retail anchor sites
(Rnd.cpp ScreenSaver sites, signin-mask readers). PlatformMgr_Xbox porting itself is
DRAINED/too-diverged (`c13507f`) — this is header-only, but do it only when an agent
is idle.

---

## Negative/triage results (do NOT chase; recorded to save the next agent's time)

- **MidiInstrument "vector\<SampleZone\> cluster" = WRONG-IDENTITY, not layout.** The
  retail element op= (`fn_822B0EF8`, named-by-map as SampleZone::operator=) is a
  **0x1c struct {ObjPtr@0x0, ObjPtr@0xc (obj at +0x8, non-poly), float@0x18}** that
  calls `fn_827D90F0` (game-address range!) — cannot be SampleZone (0x50, ours==DC3,
  `synth/SampleZone.h`). Retail strides 0x1c vs ours 0x50 across operator= (88.05),
  _M_erase (61), deallocate (4.2), ctor (15.7). The MidiInstrument pin/extension or
  the name transfer for this subrange is wrong → route to a pin-identity audit, not a
  header edit.
- **RndEnvironTracker ctor/dtor (default/BandCharacter ×2 @99.97/99.98)** = the
  documented **RndEnviron +20-slot vbase wall** (retail `lwz r11, 0x54(vptr)` vs ours
  `0x4` feeding bctrl). Refuted lever; do not re-propose. (But see Tooling gaps —
  classifier called it MEMBER_DELTA.)
- **PostProc `UpdateColorModulation` (97.94)** is NOT another rev-member: the diff is
  a CSE/reload (retail re-loads 0x124 into f0; ours reuses) — permuter-class. The
  mdf2 "−76" was a misparse of the 0x124/0x170 line pairing.
- **CameraShot `_Destroy_Range`/`_M_erase` ±232 mirrored pairs** = Crowd↔Frame
  instantiation pairing swap (mirrored signs = OFFSET_SWAP per playbook §4);
  `Load@CamShotCrowd` −8@0x90 is the CamShot vbase wall (refuted `fe0aaaa`). Skip.
- **TrainerGemTab (0x50 vs 0x14), PanelDir ×4 (+4), UIListSubList (+4), Splash
  (+4 @0x114)** = UIComponent/UILabel MI wall anchors — recorded above as input to
  docs/plans/ui-base-layout-reconstruction.md; quick-fix refuted.
- **FreeCamera::Poll ±8 mirrored** = swap, PERMUTE-class.
- **Rnd OnClearColor* are NOT evidence of an Rnd member before mClearColor** — see
  Lever 5; retail layout agrees with our compiled 0x2c (the Rnd.h `// 0x30` comments
  are stale by −4, matching the already-verified overlay/console block).

## Tooling gaps found (report to coordinator)

1. **wall_classify VTABLE gate too narrow:** reported 0 VTABLE_DIVERGENCE on a pool
   where the custom sweep found 36 slot-delta sites in 15 units. It misses (a) vcalls
   on objects loaded from memory (base reg loaded `lwz rB, 0x0(rX)` where rX≠r3 —
   RndEnvironTracker went to MEMBER_DELTA_CANDIDATE with "delta +80"), (b) cross-unit
   clustering by callee class (VocalTrackDir/LightPreset hits were routed
   DEFER_VBASE). The 60-line sweep at `/tmp/claude/vtsweep.py` (saved output
   `~/tmp/fm-research/vtable_sweep.json`) is the fix template.
2. **member_delta_finder2 misroutes vtable-slot deltas** as member deltas
   (RndEnvironTracker "+80@0x84", TrainerGemTab "+60@0x50") — should run the vcall
   check before the uniform-delta check.
3. **target_symbol_map off-by-one family** (Lever 5): name transfer can land on the
   wrong member of a run of ICF-shaped sibling accessors; reveal/gate has no
   neighbor-immediate cross-check.
4. **MidiInstrument subrange identity** (above): content at 0x822B0E00–0x822B3700
   doesn't match the unit's claimed classes; the pin-extension honesty gate
   (contiguous-foreign-run) didn't catch it because the fns pair at high fuzzy %.

## Ranked launch list

| # | target | kind | est gain | confidence | action |
|---|---|---|---|---|---|
| 1 | RndAnimatable `OnListFlowLabels` | vtable gate | +5 direct, +5–10 w/ refill | high | gate `virtual` (HX_NATIVE), A/B, refill |
| 2 | RndDrawable `Draw` devirtualize + `DrawShadow` drop | vtable gate | +3 direct, +4–8 (+unblocks RndDir body-ports) | med-high | gate both, watch UI delta, possibly pair w/ 1 UIComponent virtual |
| 3 | target_symbol_map OnClearColor off-by-one | map repair | +2–3 | high | remap 3 addrs, resplit-renamer refresh |
| 4 | PropKeys `RemoveRange` | vtable gate | +1–3 | high | gate virtual + 4 overrides |
| 5 | Player base-chain −4 | member delta | +4–10 | medium | bracket boundary 0x228–0x260, gated shrink, A/B (CameraShot −3 precedent = revert rule) |
| 6 | Rnd::Terminate DOFProc call | body gate | +1 | high | HX_NATIVE gate one call |
| 7 | PlatformMgr head layout | member reconstruction | +1–3 | low-med | only when idle; header-only, 3–4 anchors needed |

Run order: 1→2 (same-agent, same A/B harness as `30a4ae8`), 3+6 cheap singles, 4, then
5 as its own careful agent. After each landing: `NINJA_JOBS=12 tools/refill_loop.sh
--map global_fuzzy_pairs.json` (playbook §9) — both vtable levers feed the reveal
cascade in EventTrigger/LightPreset/PropKeys-adjacent anon pools.
