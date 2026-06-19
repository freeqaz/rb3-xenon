# W8 — UIComponent reconstruction (C1/C2/C3): adversarial audit

**Date:** 2026-06-19. **Baseline:** main @da8258f, report.json fresh, **8234 matched**.
**Mode:** PLANNER / ADVERSARIAL VERIFIER, read-only in main.
**Task:** falsify the "C1 UIComponent base-layout reconstruction is THE key gated
lever (open), unblocks C2 banked rnddrawable-devirt +4+cascade and C3 finishers"
claim from the CONSOLIDATED OPEN BACKLOG (roadmap §C1/C2), and emit a cold-executable
plan for the next wave.

---

## HEADLINE VERDICT: REFUTATION_WRONG — C1 and C2 are ALREADY LANDED and DELIVERED

The backlog framing is **STALE / FACTUALLY WRONG** (the inverse of the Waypoint
trap: a prior verdict says "open/gated" when ground truth says "done"). The
"reconstruction" is not pending — it shipped two commits ago.

### Ground-truth proof (git + header source + report.json)

1. **Git history** (`git log --oneline -- src/system/ui/UIComponent.h src/system/rndobj/Draw.h`):
   - `f4f4d13` — **"ui: UIComponent virtual reconstruction + banked RndDrawable
     devirt — +6 @6948"**. This commit composed C1 (own-virtuals) AND C2
     (rnddrawable-devirt) IN THE SAME COMMIT, exactly as the wave-3 dossier
     (`2026-06-11-uicomponent-virtuals.md` §"The two levers MUST land in the same
     commit") prescribed.
   - `0b7c656` — **"UIComponent: tail-byte layout fix + 5 ports + 7 reveals
     (+11 @100%)"** = C3 phase-A (the bp4-uicomp dossier `2026-06-11-bp4-uicomp.md`).

2. **`src/system/ui/UIComponent.h` (current main) ALREADY HAS** the full C1 fix:
   - 8 own-virtuals at slots 0x30–0x4c in the verified retail order: ResourceCopy,
     SetState, StateSym, Entering, Exiting, CanHaveFocus, CopyMembers, Update
     (lines 66–73).
   - `UICOMP_DC3_VIRTUAL` gate on `OldResourcePreload` (lines 97–102), and on ALL
     12 derived overrides: `grep -rn UICOMP_DC3_VIRTUAL src/ --include=*.h` →
     UIList, UISlider, UILabel, LabelShrinkWrapper, InlineHelp + hamobj
     {MeterDisplay, HamNavList, MiniLeaderboardDisplay, SongDifficultyDisplay,
     StarsDisplay}. The §2.1 "MANDATORY companion edits" are all present.
   - Member layout corrected: `int mSelected; // 0x104` precedes
     `UIResource *mResource; // 0x108` (the §1.4 swap), `State mState; // 0xe0`,
     tail `bool mLoading; // 0x13c` / `mSelectCancelled; // 0x13d`.

3. **`src/system/rndobj/Draw.h` (current main) ALREADY HAS** the C2 devirt:
   `DRAW_DC3_VIRTUAL void Draw();` + `DRAW_DC3_VIRTUAL void DrawShadow(...)`
   (lines 81/83) with the `#ifdef HX_NATIVE` gate (lines 36–40). The 5 subclass
   overrides are gated too: `grep -rn DRAW_DC3_VIRTUAL src/ --include=*.h` →
   Character.h:81 (DrawShadow), CharClipSet.h:36, HamCharacter.h:54, Env.h:28,
   Group.h:36. The "banked patch" `docs/decomp/handoff/rnddrawable-devirt-banked.patch`
   is a SPENT artifact identical to what's already in-tree.

### The predicted wins ACTUALLY LANDED (report.json `match_percent_normalized`)

| function | wave-3 dossier prediction | CURRENT (main @da8258f) |
|---|---|---|
| `?DrawShowing@RndLine@@UAAXXZ` | banked +4 anchor, 99.98→100 | **100.0** ✓ |
| `?Entering@PanelDir@@UBA_NXZ` | wall-flip +1, 99.97→100 | **100.0** ✓ |
| `?Exiting@PanelDir@@UBA_NXZ` | wall-flip +1, 99.97→100 | **100.0** ✓ |
| 8 UIComponent own-virtuals (ResourceCopy/SetState/StateSym/Exiting/Enter/CopyMembers/MockSelect/UpdateResource) | reveals | **all 100.0** ✓ |

Regression sanity (watch-list units healthy, no MI-slot corruption): ScrollSelect
18/24, UIScreen 38/51, PanelDir 33/52, UISlider 19/50, InlineHelp 29/38,
SpotlightDrawer 50/87, Group 32/69, CharClipSet 43/88, MoviePanel 31/75 — all
matched-bodies present, none zeroed.

**Conclusion:** there is NO "reconstruction" left to do. The vtable is recovered,
the layout is correct, the levers are composed and net-positive in-tree. The
backlog C1/C2 entries should be moved to CLOSED/SPENT. The roadmap "Top of queue
now" item #2 ("C1 — the key gated lever") is a phantom.

---

## What ACTUALLY remains (the honest residual frontier)

`default/UIComponent` = **55/102 matched** (47 below-100). The residual is NOT
layout/vtable — it is **body-port near-misses + a funclet wall + unmapped/unported
bodies**. Breakdown:

### Group 1 — named-method body-port near-misses (3 fns, the real "C3")
All ported, all bodies present, residual is logic/regalloc — NOT a wall, NOT
gated on any layout fix:
- `?Update@UIComponent@@UAAXXZ` **68.8%** (304 instr): REGISTER_SWAP 48 instr/7
  pairs (r10↔r11 dominant) + 3 control-flow (lwz↔b) + 1 commutative + 1
  OFFSET_SWAP (0x54,0x5c) + 79 deletes (inlined MILO_FAIL/Find<T> helper
  divergence). Deep permuter-class; the offset-swap (0x54,0x5c) is a decl-reorder
  candidate. LOW confidence to hand-finish; permuter-or-defer.
- `?ResourceFileUpdated@UIComponent@@QAAX_N@Z` **88.84%** (58 instr): target calls
  `??0String@@QAA@ABV0@@Z` (String copy-ctor), `fn_822606F0` (ObjDirPtr assign),
  `fn_822608A8`, `fn_825031C0`, `fn_827CD1E8`; OUR base calls
  `MakeString<PBD,String>`, `ObjDirPtr::op=`, `FileRoot`, `LoadFile`. The diff is
  a HELPER-CALL-PATTERN divergence at idx 14-27 (2 insert / 3 delete around the
  path build). MEDIUM confidence — oracle-guided body re-shape (rb3-Wii
  `UIComponent.cpp:385`). Our cpp body (lines 205-217) builds the path via
  `MakeString("%s/%s.milo", mResourcePath.c_str(), mResourceName)` + explicit
  `FileRoot()` + `mResourceDir.LoadFile(...)`; retail apparently uses a String
  temp + a different ObjDirPtr-assign helper. Re-recon the exact target call seq.
- `?GetResourcesPath@UIComponent@@QAAPBDXZ` **73.55%** (118 instr): 52 replace +
  14 control-flow (bl↔lwz) + r28↔r29 regswap. The `std::vector<Symbol>` +
  `SystemConfig`/`ListSuperClasses` loop diverges (the §B "may stall — drop if
  self-contained" caveat). MEDIUM-LOW; permuter-or-defer.

### Group 2 — FUNCLET_WALL (11 fns @ 92.5–99.9), gated on Handle matching
`fn_827D9D44, fn_827D9D64` (92.5), `fn_827D9D84/DAC/DD4/DFC, fn_827DA854, A8A8,
DAEA8` (99.8–99.9). Per the bp4-uicomp dossier §D (re-confirmed): these are EH
funclets of `fn_827D9928` (Handle/BEGIN_HANDLERS, 956B, currently 0%); their lone
diff is the parent frame-reconstruct `subi r31, r12, 0xd0` (target) vs `0x90`
(ours) — retail's Handle frame is 0x40 larger (more HANDLE entries). **They flip
AUTOMATICALLY iff Handle is matched.** Do NOT hand-grind the funclets.

### Group 3 — unmapped / unported bodies (34 fns @ 0%)
The dtor `fn_827DABC0`, ctor `fn_827DA5B0`, Handle `fn_827D9928`, SyncProperty
`fn_827DB5D0`, PreLoad `fn_827DB5E0`, `fn_827DB6E8` (PostLoad/Save?), the
ObjPtr-dtor `fn_827D94D8`, the Object-vbase thunks `fn_827DA9B0..ABA8`, and vbase
PreLoad/PostLoad thunks `fn_827DBD90/BDA0`. These are anonymous on the target
(no map entry) so they read 0% even if our compiled body is byte-exact. Two
sub-classes:
- **Reveal candidates** (body defined in our cpp, just no map entry): the dtor
  `fn_827DABC0` is the prime one — bp4-uicomp §C listed it as "bonus, 284B,
  plausible after the ctor edit; don't count" but never verified. Cheap to test:
  add a map entry, A/B; if it reads 100 it's a free +1 (+ the ObjPtr-dtor
  `fn_827D94D8` and thunks may cascade).
- **Real ports** (no body in our cpp): Handle `fn_827D9928`, SyncProperty,
  PreLoad/PostLoad/Save — these need the rev-2-vs-rev-3 Load/Save reconcile noted
  in bp4-uicomp §D BODY_DEEP. Deeper; defer until Handle is scoped (it also
  unblocks the 11 funclets).

---

## Falsification attempts that FAILED to overturn "C1/C2 done" (i.e. it really is done)
- Checked whether the own-virtuals landed WITHOUT the devirt (would mean
  mis-aligned slots, hidden regression): NO — both are in `f4f4d13`, and
  DrawShowing@RndLine + PanelDir Entering/Exiting are all 100, which is only
  possible if BOTH the +8 (own-virtuals) and −8 (devirt) shifts are present and
  cancel. The arithmetic the wave-3 dossier §1 predicted is satisfied in-tree.
- Checked whether the devirt is only partially applied (some subclass override
  ungated → bogus slot): NO — all 5 (Character/CharClipSet/HamCharacter/Env/Group)
  carry DRAW_DC3_VIRTUAL.
- Checked whether the UICOMP_DC3_VIRTUAL companion edits are incomplete (a missed
  derived OldResourcePreload inserts a slot, regressing that subtree): NO — all 12
  sites from the §2.1 list are gated; InlineHelp (the §2.1 "highest risk")
  is 29/38, not regressed.

---

## Per-work_item evidence index
- WI-1 (ResourceFileUpdated): run_objdiff full_listing above; oracle rb3-Wii
  `src/system/ui/UIComponent.cpp:385-397`; our cpp `src/system/ui/UIComponent.cpp:205-217`.
- WI-2 (dtor/ctor reveal): unit dump shows `fn_827DABC0` (dtor, 284B) +
  `fn_827DA5B0` (ctor, 496B) @0%, no map entry (`grep 827DABC0 scripts/target_symbol_map.json` → none);
  our cpp defines the ctor (line 39) and the implicit `~UIComponent(){}` (header line 47).
- WI-3 (CollideList@RndGroup 91.5%): run_objdiff above — bl↔mr control-flow,
  not a slot diff (slot already aligned by devirt).
- WI-4 (Update / GetResourcesPath): permuter-class; run_objdiff pattern tables above.
- WI-5 (Handle + funclets): bp4-uicomp §D, re-confirmed by the 11-funclet @99.9
  cluster all sharing the `subi r31,r12,0xd0` parent-frame signature.

## Recommendation for the roadmap
Move C1 + C2 to CLOSED/SPENT (landed `f4f4d13`/`0b7c656`, +17 already counted in
the 8234 baseline). Re-file C3 as a small bodyport lane (WI-1/WI-3 actionable;
WI-4 permuter; WI-5 deeper). Net remaining EV in this whole area is modest
(~+2 to +5), not the "+4+cascade key lever" the stale backlog implies — that
cascade ALREADY fired.
