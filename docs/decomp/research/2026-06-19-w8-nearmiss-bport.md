# Wave-8 B-tier near-miss body-port assessment (adversarial planner)

**Date:** 2026-06-19
**Mode:** PLANNER / ADVERSARIAL VERIFIER (Opus), read-only on main `da8258f`, baseline **8234** matched.
**Area:** nearmiss-bport — B1 Waypoint Save/Copy, B2 FreestyleMotionFilter, C3 UIComponent Update/ResourceFileUpdated, B3 SongMgr residual.
**Method:** objdiff full-listing + Ghidra decompile of the *retail* bodies + COFF symbol-table ground truth + rb3-Wii/DC3 oracle cross-check. Applied the Waypoint lesson: every "wall/exhausted" was treated as a hypothesis to falsify.

## Headline verdicts
- **B1 Waypoint Save (99.85) + Copy (99.79) — ACTIONABLE, +2, REFUTATION-WRONG (DC3 false-friend).** The residual is a single **member-type divergence**: retail's `mConnections` is `ObjVector<ObjOwnerPtr<Waypoint> >` (0x10 bytes, element 0xc), NOT our DC3-derived `ObjPtrVec<Waypoint>` (0x1c). rb3-Wii oracle (the GAME-side header) has it RIGHT (`ObjVector<ObjOwnerPtr<Waypoint> > mConnections; // 0xac`); DC3's newer engine changed it to ObjPtrVec and we copied DC3. Clean port. **This is the high-value outcome the brief asked for: a prior implicit "engine⟶DC3" assumption is wrong; the rb3-Wii game oracle is authoritative here.**
- **B2 FreestyleMotionFilter::Deactivate (99.33) — WALL / stub mirage, DEFER.** Retail body is a 12-byte stub `*(byte*)(this+0x10)=1` (proven via Ghidra + COFF). Writing **1** (not our `mIsActive=false`) at offset **0x10** (inside the Hmx::Object 0x28 base region). Only ONE FreestyleMotionFilter symbol exists in the whole binary (this stub) — no ctor/Activate/Clear to triangulate the real layout. Unmatchable without a base-class change (impossible for one stub). DC3 is a false friend (identical-to-ours `=false`); no rb3-Wii oracle (Kinect-only). The "−36@0x10 member_delta" backlog note = decimal(0x34−0x10)=0x24=36; it is a stub, not a recoverable layout delta.
- **C3 UIComponent ResourceFileUpdated (88.84) + Update (68.8) — GATED on C1, DEFER (not standalone walls).** ResourceFileUpdated has a genuine portable body divergence (retail uses a String-arg `MakeString` overload + an extra `String` copy + a different `mResourceName` offset 0x138-vs-0x118) BUT it is entangled with the UIComponent member layout (0x118/0x138 String slots) which is the C1 reconstruction. Update (68.8%, 79 deletes + r10↔r11 regalloc cascade) is a deep body+layout wall. Both un-actionable until C1 (`docs/plans/ui-base-layout-reconstruction.md`) lands; coordinate with the C1 planner.
- **B3 SongMgr — TRULY EXHAUSTED.** All 27 named SongMgr methods are 100.0% after the all-5 hash_map conversion. `ContentDiscovered`/`ContentMounted` do not exist as named functions anywhere in report.json (eliminated/inlined). The only sub-100 in the unit is anon `fn_82784D88` (99.8, no map entry — an unported STL helper, not a SongMgr member). No work_item.

---

## B1 — Waypoint Save/Copy (DECISIVE EVIDENCE)

### The diffs
`Save` (?Save@Waypoint@@UAAXAAVBinStream@@@Z): 10/67 diff_arg, all a **uniform +0xC offset shift** + ONE call swap:
- idx 30: target `bl fn_823C8790` vs base `bl ??$?6VWaypoint@@@@YAAAVBinStream@@AAV0@ABV?$ObjPtrVec@VWaypoint@@VObjectDir@@@@@Z`
- idx 13/17/19/22/31/37/43/49/55: every member offset is 0xC smaller in target (e.g. vbase `-0xe0` target vs `-0xec` base).

`Copy` (?Copy@Waypoint@@...): 9/43 diff_arg, same +0xC shift + ONE call swap:
- idx 30: target `bl ??4?$ObjVector@VConstraint@BandIKEffector@@@@QAAXABV0@@Z` vs base `bl ??4?$ObjPtrVec@VWaypoint@@VObjectDir@@@@QAAXABV0@@Z`
- idx 10: target `bl fn_82735C58` vs base `bl ?Copy@Object@Hmx@@...` (Object::Copy ICF-folded into fn_82735C58, naming noise).

(The `ObjVector<Constraint@BandIKEffector>::operator=` NAME is an ICF alias — see below; the real element size is 0xc, proven by the helper's `/0xc` divide.)

### Retail ground truth (Ghidra)
- `Waypoint::Save` @0x823C8A48 calls `fn_823C8790(binstream, this-0x14)`.
- `fn_823C8790` = `operator<<(BinStream&, ObjVector<ObjOwnerPtr<Waypoint>>&)`:
  `local[0] = (end - begin)/0xc; WriteEndian(count,4); for (p=begin; p!=end; p+=0xc) Function_822B75B0(bs,p);`
  → **element size 0xc** = `ObjOwnerPtr<Waypoint>`; `Function_822B75B0` reads `p+8` (mObject) and writes its name (the ObjPtr-name streamer). Textbook `operator<<(BinStream&, ObjVector<T>&)`.
- `Waypoint::Copy` @0x823CA598 copies `mConnections` via `Function_823CA528(this-0x14, src+0xd0)`:
  `if (a!=b) { resize_helper(a, (b[1]-*b)/0xc); copy_helper(a,b); }` → **`ObjVector<T>::operator=`** with 0xc-byte elements. Confirms the member is ObjVector, element 0xc.

### Oracle (the refutation)
- **rb3-Wii** `../rb3/src/system/char/Waypoint.h:62`: `ObjVector<ObjOwnerPtr<Waypoint> > mConnections; // 0xac` — CORRECT for retail.
- **DC3** `../dc3-decomp/src/system/char/Waypoint.h`: `ObjPtrVec<Waypoint> mConnections; // 0xdc` — byte-identical to OURS, and WRONG for retail (DC3-newer divergence).
- Our `src/system/char/Waypoint.h:74`: `ObjPtrVec<Waypoint> mConnections; // 0xdc` (copied from DC3).
- Sizes: `ObjPtrVec` total **0x1c** (Object.h:480 comment); `ObjVector<ObjOwnerPtr<...>>` = std::vector(0xc)+mOwner(0x4) = **0x10**. Delta **0xC** == the observed shift exactly.

### Why it is portable (no new infra)
- `ObjVector<ObjOwnerPtr<...>>` is already a proven working pattern in our tree: `src/system/char/CharBonesMeshes.h:29` `ObjVector<ObjOwnerPtr<RndTransformable> > mMeshes;` (same "Retail X360: ObjVector<ObjOwnerPtr<...>>" annotation).
- `ObjVector` lives in `obj/Object.h` (already included by Waypoint.h). Ctor is `ObjVector(Hmx::Object*)` (1-arg) — matches rb3-Wii `mConnections(this)`.
- Per-element `operator<<(BinStream&, const ObjOwnerPtr<T1>&)` EXISTS (Object.h:461). `PropSync(ObjVector<T>&,...)` EXISTS (PropSync_p.h:363) so `SYNC_PROP(connections,...)` still compiles. `operator>>(BinStream&, ObjVector<T>&)` EXISTS (Object.h:1816) so `d >> mConnections` (Load) compiles.
- ⚠ GAP: there is **no `operator<<(BinStream&, ObjVector<T>&)`** in Object.h (only `operator>>`). The impl agent must ADD it (mirror of the operator>> at Object.h:1816 — write `count` then iterate per-element `bs << *it`). This is what retail's `fn_823C8790` is. Add it in Object.h next to the `operator>>` templates, OR TU-locally in Waypoint.cpp (prefer Object.h for parity with how operator>> is shared). Without it, `bs << mConnections` won't compile once the type flips.

### Cold-exec plan (B1)
1. `src/system/char/Waypoint.h:74`: `ObjPtrVec<Waypoint> mConnections; // 0xdc` → `ObjVector<ObjOwnerPtr<Waypoint> > mConnections; // 0xd0`.
2. `src/system/char/Waypoint.cpp:18-19` ctor init-list: `mConnections(this, (EraseMode)1)` → `mConnections(this)`.
3. If absent, add `template<class T> BinStream &operator<<(BinStream &bs, const ObjVector<T> &vec)` in `src/system/obj/Object.h` next to the `operator>>` at ~1816 (write `vec.size()` then loop `bs << vec[i]`). Verify it produces the same shape as retail `fn_823C8790` (count then per-element).
4. Build, objdiff Save + Copy → expect both 100. Also re-check `?Load@Waypoint@@...` and the ctor/dtor didn't regress (they should improve or stay).
5. **HEADER EDIT — full-binary A/B mandatory.** sizeof(Waypoint) shrinks 0xC. Blast radius: nearly all Waypoint uses are `Waypoint*` (pointers, sizeof-immune). `ClipCollide.h:70` reference is a doc comment. Confirm A/B nets >= +2 with no foreign regressions; revert if any cross-TU regression appears.

Expected delta **+2** (Save, Copy); possible small bonus from the connections-machinery anon funclets/templates in the Waypoint TU re-pairing.

---

## B2 — FreestyleMotionFilter::Deactivate (WALL, evidence)

- objdiff: idx0 `li r11,0x1`(target) vs `li r11,0x0`(base); idx1 `stb r11,0x10,r3`(target) vs `stb r11,0x34,r3`(base). 12-byte fn.
- Ghidra @0x827AA978 (COFF-authoritative symbol `?Deactivate@FreestyleMotionFilter@@QAAXXZ`): body = `*(byte*)(this+0x10)=1; return;`. NOT ICF-folded (icf_aliases has 0 entries; only 1 symbol at the VA).
- Layout impossibility: Hmx::Object base is **0x28** on retail X360 (Object.h ctor-derived comment). A normal derived member cannot live at 0x10 (that's `mTypeDef`'s slot). So retail's FMF is NOT a normal 0x28-Object subclass here, OR this is a retail coverage/instrumentation stub (cf. memory `project_game_code_instrumentation` — retail stubbed trivial accessors into uniform breadcrumb bodies). Writing `=true` in a method named `Deactivate` corroborates "stub, not real logic".
- Oracles dead: DC3 `Deactivate(){mIsActive=false;}` (identical-to-ours, false friend); no rb3-Wii (Kinect/gesture not in Wii build). Only ONE FMF symbol binary-wide (Ghidra symbol search) → cannot triangulate the real layout.
- **Verdict: DEFER as stub-mirage/base-layout wall.** Matching it needs the member at 0x10 + value 1, achievable only via a base-class change with huge blast radius for a single +1 stub — not worth it and not safely reconstructable. Do not re-attempt as a "member_delta" lever.

---

## C3 — UIComponent ResourceFileUpdated / Update (GATED on C1)

### ResourceFileUpdated (88.84) — portable body divergence BUT layout-entangled
objdiff (224B target vs 220B base; 6 diff_arg + 2 replace + 3 delete + 2 insert):
- Stack frame 0xa0(target) vs 0x90(base) — retail builds an **extra String** (idx14-16 deleted from base: `addi r4,r30,0x118; addi r3,r31,0x60; bl ??0String@@QAA@ABV0@@Z`).
- MakeString path: target calls `fn_827CD1E8` with a **String** arg (`lwz r4,0x138,r30`) + literal `"%s/%s.milo"`; base calls the `MakeString<PBD,String>` template passing `mResourceName` as `const char*`. → retail uses a String-arg MakeString overload, and `mResourceName` is at **0x138** in retail vs **0x118** in our header (UIComponent.h:116). There is another String at 0x118 retail copies first.
- These offsets ARE the UIComponent member layout that C1 reconstructs. Porting RFU bodies alone won't match until 0x118/0x130/0x138 String slots are correct.

### Update (68.8) — deep wall
51 diff_arg, 79 delete, r10↔r11 regalloc cascade (REGISTER_SWAP 48 insns/7 pairs), 3 control-flow replacements. Body + layout divergence; permuter-class at best, and gated on C1.

**Verdict: DEFER both, gated on C1** (`docs/plans/ui-base-layout-reconstruction.md` — layout is reverse-engineered but standalone-EV is 0; it's a 3-step foundation). ResourceFileUpdated becomes a tractable body-port (String-overload MakeString + extra String copy) the moment C1 lands; flag it as the first RFU follow-up. Coordinate with the C1/UIComponent-reconstruction planner. Wave-7's "wall" tag holds, refined: RFU = port-after-C1, Update = deep wall.

---

## B3 — SongMgr (EXHAUSTED)
All 27 named methods 100.0% (verified via report.json filter excluding fn_/lbl_). `ContentDiscovered`/`ContentMounted` absent binary-wide. Sole sub-100 in unit = anon `fn_82784D88` (99.8, no target_symbol_map entry → STL helper, not a member). No actionable named residual. No work_item.

---

## Honesty / attribution notes
- B1 is a body/type fix (member type flip), not a pin/relocation — attribution_risk is LOW, but the HEADER EDIT mandates a full-binary A/B (sizeof change). Marked attribution_risk=false (no pin), but the A/B is non-negotiable.
- B2/C3/B3 produce no landable items this wave.
