# W16-TV: the two alias-proof gaps closed, a callee-rename re-proof check, and the 74 refuted memberships adjudicated

**Date:** 2026-10-07 · **Lane:** W16-TV · **Branch:** `w16-tv` off main `be41a0308` · **Worktree:** `~/tmp/wt-w16tv`

W16-TT (`docs/decomp/W16TT_FOLD_LEADS_ON_RETAIL_2026-10-07.md` §2–§3) found two ways a wrong alias membership got recorded
as PROVEN, and left 74 installed memberships that the shipped checker itself refutes. This lane closes both gaps in the
tools, adds a build check that re-proves a group when a callee's map name changes, shows each fix failing a planted bad
membership, and withdraws all 74 on retail-byte evidence.

## 0. Results

| item | result |
|---|---|
| gap 1: same-name slot accepted unread | **closed** in `icf_pair_adjudicate` (chase, `_slots_agree`, vacuous branch) and, through a chase confirmation, in flat T1 |
| gap 2: flat T1 tolerates an unnamed retail callee when ours is unmapped | **closed** in `icf_alias_build.relocs_agree` (all callers), plus the chase confirmation |
| gap 3: no re-proof when a callee's map name changes | **new check** `tools/alias_callee_name_drift.py`, wired as build edge `CHECK ALIAS CALLEE NAMES VS MAP` |
| planted bad memberships | 771 (09-15 map state) and 1201 (unnamed callee): **PROVEN under the old policies, not PROVEN under the fixes**. 771 timeline: survivor-drift check PASSes it, new check flags it, re-proof refutes |
| installed memberships moved by the fixes | **0 of 6,355** (one apparent flip was a memo artifact, §2.4) |
| the 74 | **74 withdrawn, 0 kept**, each with a record (§4) |
| `ab_measure` on the 74 withdrawals | **+0 fns / +0 B / Δfuzzy +0.000000** (predicted exactly 0) |
| installed memberships reading REFUTED after the lane | **0** (6,281: 5,542 PROVEN, 739 UNDECIDABLE) |

Commits: `f27b3a733` (tool fixes, plants), `ac027140a` (74 withdrawals), `bbe9dba1e` (snapshot + build edge + test), and this doc.

## 1. Gap 1: a same-name slot is read, not trusted

**Before.** `_slots_agree` did `if rn == on: continue`. `chase` returned True for `survivor == our_name and depth > 0`, and
`relocs_agree` (flat T1) also skips equal names. The retail name is the map's, so the slot was only as good as the map's
identification. Group 771 passed this way: the map had our spelling on a twin body.

**Now** (`icf_pair_adjudicate._samename_ok`). When the callee is present and non-vacuous on both sides, retail's body named
N is chased against our N:

- **The read holds:** OK (`SAMENAME-READ-OK`).
- **The read fails and our N is PROVEN elsewhere:** refuted (`SAMENAME-CONTRADICTED`). "Elsewhere" means `locate_retail`
  gives a clean chase at another retail body.
- **The read fails and our N is located nowhere:** accepted and traced `SAMENAME-UNVERIFIED`. A byte difference alone means
  our port may be imperfect, not that the map is wrong. This is W16-TT's negative control `?Release@Object@Hmx` and the same
  rule W16-JG uses for an undischarged callee.
- **The callee is absent or vacuous on either side:** nothing to read, accepted.

The rule is the one W16-TT's audit validated with both controls; it moved from a monkeypatch into the tool. The same check
covers the vacuous identical branch: an 8-byte thunk's destination is its whole content.

`adjudicate()` now confirms every flat-T1 pass with the chase. A failed confirmation reads REFUTED when the trace has a
positive contradiction, and UNDECIDABLE otherwise. The generator's T1 tier is confirmed the same way
(`reject_T1_CHASE_CONFIRM_FAILED`).

Residual name-only evidence, measured: **886** PROVEN memberships carry at least one `SAMENAME-UNVERIFIED` slot. Across the
file there were 22,910 reads OK, 7,196 unverified and 80,525 uncheckable (a data global or vacuous body). Those 886 are proven
on bytes everywhere except slots where our callee port differs and no contradiction exists. That is the honest bound on what
"PROVEN" still takes from the map.

## 2. Gap 2: an unnamed retail callee fails flat T1

**Before.** CD-9 refused a retail `fn_<B>` slot only when our callee is map-resident, and otherwise tolerated it. That is how
W16-CU's ten 09-15 restorations (groups 723 ×5, 875, 1201 ×3, 1223) read flat PROVEN.

**Now.** With `relocs_agree(strict=True)`, a retail `fn_` slot against a real name of ours fails, whether or not ours is
mapped (`UNNAMED_CALLEE_POLICY = "refuse"`). The pair can still be proven by the chase, whose W16-JG discharge reads `fn_<B>`'s
bytes against our callee (`CALLEE-CHASED`), but never by a literal-name comparator. `survivor_self_check` is a *refusal* gate,
so it keeps the old tolerance explicitly: an undecided slot is not a contradiction.

### 2.1 Planted bad memberships (`tools/test_alias_proof_gaps.py`, needs a built tree)

| plant | policies | flat T1 | verdict |
|---|---|---|---|
| 771 in the 09-15 map state (0x82773E70 carries our spelling) | pre-W16-TV | PROVEN | **PROVEN** |
| | same-name trust, flat confirmed | PROVEN | PROVEN (so the read is what does the work) |
| | shipped | REFUTED (`SAMENAME-CONTRADICTED`) | **REFUTED** |
| 771 on today's map | shipped | REFUTED | REFUTED (W16-TT's finding) |
| 1201 in the 09-15 map state (0x827427E8 unnamed; our callee unmapped, so CD-9 alone misses it) | pre-W16-TV | PROVEN | **PROVEN** |
| | only the unnamed-callee refusal | REFUTED | **REFUTED** |
| | shipped | REFUTED | REFUTED |
| sound: W16-JE `SetObjConcrete<UILabel>` | shipped | PROVEN | **PROVEN** |

`TEST PASS`. `tools/alias_samename_slot_audit.py --selftest` now uses the same plants: `PASS`.

### 2.2 Existing controls

`icf_pair_adjudicate.py --chasetest` still passes all 23 controls. Every `--self-break*` mode still exits 0, so each decoy
goes red and the rest stay green. Five decoys that flat T1 itself used to read PROVEN now fail flat T1 too, and every positive
control is unchanged:

| decoy | flat T1 before → after |
|---|---|
| SLOT DECOY W16-JE vtable pair | PROVEN → REFUTED |
| SLOT DECOY CALLEE_LOCATED_ELSEWHERE | PROVEN → REFUTED |
| SLOT DECOY VTABLE_OF_CLASS_ELSEWHERE | PROVEN → REFUTED |
| SLOT DECOY VTABLE_RTTI_DIFFERS | PROVEN → REFUTED |
| RENAME DECOY | PROVEN → UNDECIDABLE |

### 2.3 Generator (T1 only, `--tiers 1`, scratch output)

The old policies emit 227 memberships and the new ones 226. The one dropped is `__adjust_heap<RndTransformable*>` into
`__adjust_heap<RndPollable*>` (installed, group 205). Flat T1 now refuses its unnamed slot `fn_822BB3C8`. The installed
membership stays PROVEN, because the chase discharges that slot on bytes: retail `fn_822BB3C8` is our
`__push_heap<RndTransformable*>`. The generator has no chase path for flat-refused pairs, so it is now strictly more
conservative.

### 2.4 Population: no installed membership moves, after one false flip

The first whole-file run read **one** membership PROVEN → REFUTED (group 1629, `list<Symbol>::_S_sort`) with an *empty*
trace. A fresh memo reads it PROVEN under both policies. Cause: `chase` memoizes a failed pair at whatever depth it was first
reached, **including a `DEPTH-CAP` failure**. A later membership that meets the pair at a shallower depth inherits the failure.
The same-name reads recurse deeper, so they expose this.

W16-TT's audit also shared one memo across the file. `membership_verdict` (the one verdict function the audit, the snapshot
and the re-proof now share) therefore uses one memo per membership, which costs 22 s for the file.

With that fix: **6,355 memberships, 0 change verdict, 74 REFUTED, the same 74 W16-TT listed.** W16-TT's base read 4,519
PROVEN, 74 REFUTED and 1,762 UNDECIDABLE. This run reads 5,542 / 74 / 739, because `membership_verdict` also chases pairs flat
T1 calls UNDECIDABLE (vacuous bodies), as the `--chase` CLI does. W16-TT's post-edit control quoted 4,525 PROVEN, and HEAD gave
4,519 with its own tool. I did not chase that 6-row gap; a shared-memo artifact of the same kind is the obvious suspect.

## 3. Gap 3: re-prove a group when a callee's map name changes

`tools/alias_survivor_drift.py` (W16-OS) pins the survivor's name. Nothing pinned the names the proof reads. That is why
771 kept its record after W16-SG renamed 0x82773E70, and why 723/875/1201/1223 kept theirs after the map named their callees
on 09-30.

**`scripts/alias_callee_names.json`** stores, per placed group with a folded member:

- its survivor;
- the retail callee addresses its survivor reaches in ≤3 relocation steps;
- the `membership_verdict` of each folded member;
- one shared address → applied-map-name table.

Depth 3 covers 2,972 distinct addresses, 8% of the 36,249-row map (depth 1: 1,056; depth 4: 3,614). It is a stated bound,
not the chase's full depth. Recorded at this lane: 1,442 groups, 6,140 memberships (5,525 PROVEN, 615 UNDECIDABLE,
0 regressed). The 141 memberships in address-less groups are exempt, as in the survivor check.

**`--check`** needs no build. A group is stale when it is unrecorded, its survivor changed, a folded member has no recorded
proof, or the applied name at any recorded callee address differs. It is wired as an `always` edge gating REPORT and
`progress`, like the survivor check.

**`--reprove [--write]`** needs a built tree. It re-proves every membership of each stale group. A member that was PROVEN and
no longer is, or any member reading REFUTED, is a regression: it is printed and **not recorded**, so the build stays red until
the membership is withdrawn with a record. The check cannot launder a refutation.

**Shown to fail.**

- `--selftest` covers 9 frozen fixtures (rename, newly named, newly unnamed, unrecorded, new member, survivor change, and a
  withdrawn member not stale) plus the **771 timeline**. The timeline was recorded PROVEN with the 09-15 comparators and map,
  over 15 callees including 0x82773e70. With today's map, the survivor-drift check **PASSes it (blind)**, this check flags
  `callee 0x82773e70 renamed`, and the re-proof reads **REFUTED**, regression reported. `SELFTEST: PASS`.
- Build sabotage: one recorded callee name planted stale in the snapshot gave a full build `rc=1` at
  `alias_callee_name_drift_checked.stamp`, naming the 2 groups whose proofs read that address. Restored, the build was
  `rc=0`.
- `tools/test_alias_callee_name_drift.py` (script arm of `scripts/test_tools.py`) runs the fixtures plus two live-snapshot
  mutations, a renamed callee and an unproven admission, and requires both to go red. 4 passed. As first written, the file
  ran **zero** tests when invoked as a script and exited 0, the W16-OA vacuity; it now carries the `__main__` pytest block.

**Cost for other lanes.** A map edit that renames an address in some group's 3-step neighbourhood, or any alias admission,
now turns the build red until `tools/alias_callee_name_drift.py --reprove --write` is run on the built tree and its snapshot
change is committed with the edit.

## 4. The 74: all withdrawn

Per membership I collected:

- the shipped verdict with a fresh memo, and the trace's leaf failure;
- `locate_retail` for our folded body and for our leaf callee;
- the leaf sizes on each side;
- how many groups claim the spelling;
- a **call-site census**: our callers of the spelling that pair with a retail body by name, and what retail calls in that
  relocation slot;
- for DC3-named types, whether the class exists in retail. The test is a Python scan of `band.exe` for `.?AV<name>@@` RTTI
  and the class-name string. The control: every survivor-side polymorphic class (`BandCamShot`, `CharLipSync`,
  `GemTrackDir`, `FileMerger`, `CharHair`, `SpotlightDrawer`, `RndTexBlendController`) is found.

**No membership has any support.** Across the 74, **0** paired retail call sites call the survivor, and one contradicts it
(G971, below). Every other caller of ours has no retail counterpart by name. So I predicted withdrawal would measure exactly 0,
and it did. Each record names its strongest evidence, in this priority:

| class | n | evidence |
|---|---:|---|
| `FOLDED_TYPE_ABSENT_FROM_RETAIL` | 51 | DC3 types: `HamCamShot`, `HamMove` (+ `LocalizedName`, `MoveReplacer`, `MoveParent`), `CharSignalApplier::BoneOp`, `ObjPtrVec<Flow…/HamCharacter/HamMove/RhythmDetector/Object/RndTex>` nodes, `Flow::DynamicPropertyEntry`, `MoveDetector`, `MsgSinks`. The polymorphic ones have no RTTI and no class-name string in `band.exe`. `ObjPtrVec` and `MsgSinks` are not in the RB3 engine: its `Msg.h` has `MsgSource::Sink`, and ours holds an `ObjOwnerPtr` where retail's callee copies an `ObjPtr`. Retail holds no instantiation to fold. |
| `FOLDED_BODY_LOCATED_ELSEWHERE_ON_RETAIL` | 12 | our body is PROVEN at another retail address (the address in the table) |
| `LEAF_CALLEE_SIZE_DIFFERS_ON_RETAIL` | 8 | the survivor's callee and ours differ in size, so they cannot be one COMDAT. This includes `pair<String,String>`, claimed by **5** groups whose copy ctors are 84/120/120/172/224 B against our 56 B |
| `BODY_SIZE_DIFFERS_ON_RETAIL` | 1 | `NetLoaderRef::NetLoaderRef` 64 B against `File::Filename` 48 B |
| `LEAF_CALLEE_LOCATED_ELSEWHERE_ON_RETAIL` | 1 | our `vector<short>` copy ctor is proven at `0x82bb3430` (next to W16-TT's `vector<short>` chain) |
| `LEAF_CALLEE_BYTES_DIFFER_NO_SUPPORT` | 1 | `FixedString::contains`: same-size `find` leaves differ, nothing located, no callers of ours |

The pigeonhole holds across the generator-T1 family. `_Copy_Construct<HamMove::LocalizedName>`,
`_Param_Construct<MoveReplacer>` and `_Param_Construct<set<MoveParent*>>` are each claimed by **6** groups at 6 retail
addresses, but one COMDAT has one address. Each of those survivors calls a different copy constructor.

**None kept.** The candidates I looked at hardest were G705 and G443 (`MsgSinks::Sink` / `EventSink`). There our source,
not the map, could have been the wrong side. They fall to the type test: `MsgSinks` is DC3's message system, which RB3's engine
does not have.

**Measured** (`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16tv --from-dirty --label w16tv-withdraw74`, kind `map`, both
legs at the split fixed point, objdiff `sha256:c1b7d952`, run `20261007-133306-w16tv-withdraw74-2797070`):

| | leg A | leg B | Δ |
|---|---|---|---|
| matched functions | 54,939 | 54,939 | **+0** |
| code % | 59.312576 | 59.312576 | **+0 B** |
| fuzzy | 64.316605 | 64.316605 | **+0.000000** |
| `none` control | 54,995 / 59.680496 | same | flat |

### 4.1 Leads (not acted on; naming is a bet)

- **G971:** our `vector<Key<vector<Vector2>>>::_M_erase` is PROVEN at the unnamed `0x82442E68`, which is exactly where
  retail's paired `resize` calls. That address is the body's real home.
- **G846:** our `~vector<ColorSet>` / `~_Vector_base<ColorSet>` are PROVEN at the unnamed `0x824DFA50`.
- Others with a located body at a named address: G95 → `0x824f9020`, G546 → `0x826d0da0`, `RangeSection` ×6 →
  `0x827fb758`, G1688 → `0x824f18c8`. Some of these may be admissions to the group at that address. I did not adjudicate
  those admissions.

### 4.2 Per-membership verdicts

| # | group @ address | folded spelling (truncated) | verdict | class | key evidence |
|---:|---|---|---|---|---|
| 1 | 93 @ `0x822b2e20` | `?TeleportTarget@HamCamShot@@QAAXPAVRndTransformable@@ABVTransform@@_N@Z` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf is a vacuous local-static slot |
| 2 | 95 @ `0x824d44d8` | `??$__uninitialized_fill_n@PAUShaderMacro@@IU1@@stlpmtx_std@@YAPAUShaderMacro@@PAU1@IABU1@A` | WITHDRAW | BODY-ELSEWHERE | ours proven at 0x824f9020 |
| 3 | 111 @ `0x823d3ac8` | `??$_Copy_Construct@U?$pair@$$CBVString@@V1@@stlpmtx_std@@@stlpmtx_std@@YAXPAU?$pair@$$CBVS` | WITHDRAW | LEAF-SIZE | leaf 84 B retail vs 56 B ours; claimed by 5 groups |
| 4 | 111 @ `0x823d3ac8` | `??$_Copy_Construct@ULocalizedName@HamMove@@@stlpmtx_std@@YAXPAULocalizedName@HamMove@@ABU1` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 84/84 B; claimed by 6 groups |
| 5 | 111 @ `0x823d3ac8` | `??$_Copy_Construct@UMoveReplacer@@@stlpmtx_std@@YAXPAUMoveReplacer@@ABU1@@Z` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 84/84 B |
| 6 | 111 @ `0x823d3ac8` | `??$_Param_Construct@UMoveReplacer@@U1@@stlpmtx_std@@YAXPAUMoveReplacer@@ABU1@@Z` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 84/84 B; claimed by 6 groups |
| 7 | 111 @ `0x823d3ac8` | `??$_Param_Construct@V?$set@PBVMoveParent@@U?$less@PBVMoveParent@@@stlpmtx_std@@V?$StlNodeA` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 84/240 B; claimed by 6 groups |
| 8 | 322 @ `0x827a3fa0` | `?push_back@?$vector@UNode@?$ObjPtrVec@VFlow@@VObjectDir@@@@V?$StlNodeAlloc@UNode@?$ObjPtrV` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 80/60 B |
| 9 | 322 @ `0x827a3fa0` | `?push_back@?$vector@UNode@?$ObjPtrVec@VFlowNode@@VObjectDir@@@@V?$StlNodeAlloc@UNode@?$Obj` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 80/60 B |
| 10 | 322 @ `0x827a3fa0` | `?push_back@?$vector@UNode@?$ObjPtrVec@VObject@Hmx@@VObjectDir@@@@V?$StlNodeAlloc@UNode@?$O` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 80/60 B |
| 11 | 322 @ `0x827a3fa0` | `?push_back@?$vector@UNode@?$ObjPtrVec@VRhythmDetector@@VObjectDir@@@@V?$StlNodeAlloc@UNode` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 80/60 B |
| 12 | 416 @ `0x8242c568` | `??$sort@PAPAUEventEntry@@UMaxSort@@@stlpmtx_std@@YAXPAPAUEventEntry@@0UMaxSort@@@Z` | WITHDRAW | LEAF-SIZE | leaf 208 B retail vs 188 B ours |
| 13 | 443 @ `0x8230df20` | `??$_Copy_Construct@UEventSink@MsgSinks@@@stlpmtx_std@@YAXPAUEventSink@MsgSinks@@ABU12@@Z` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 92/92 B |
| 14 | 546 @ `0x8266c440` | `??1?$vector@U?$pair@PAVRndTexBlendController@@M@stlpmtx_std@@V?$StlNodeAlloc@U?$pair@PAVRn` | WITHDRAW | BODY-ELSEWHERE | ours proven at 0x826d0da0 |
| 15 | 564 @ `0x82346e50` | `??$_Copy_Construct@ULocalizedName@HamMove@@@stlpmtx_std@@YAXPAULocalizedName@HamMove@@ABU1` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 56/84 B; claimed by 6 groups |
| 16 | 564 @ `0x82346e50` | `??$_Copy_Construct@UNode@?$ObjPtrVec@VFlow@@VObjectDir@@@@@stlpmtx_std@@YAXPAUNode@?$ObjPt` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 56/144 B |
| 17 | 564 @ `0x82346e50` | `??$_Copy_Construct@UNode@?$ObjPtrVec@VFlowLabel@@VObjectDir@@@@@stlpmtx_std@@YAXPAUNode@?$` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 56/144 B |
| 18 | 564 @ `0x82346e50` | `??$_Copy_Construct@UNode@?$ObjPtrVec@VFlowNode@@VObjectDir@@@@@stlpmtx_std@@YAXPAUNode@?$O` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 56/144 B |
| 19 | 564 @ `0x82346e50` | `??$_Copy_Construct@UNode@?$ObjPtrVec@VFlowOutPort@@VObjectDir@@@@@stlpmtx_std@@YAXPAUNode@` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 56/144 B |
| 20 | 564 @ `0x82346e50` | `??$_Copy_Construct@UNode@?$ObjPtrVec@VHamCharacter@@VObjectDir@@@@@stlpmtx_std@@YAXPAUNode` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 56/144 B |
| 21 | 564 @ `0x82346e50` | `??$_Copy_Construct@UNode@?$ObjPtrVec@VHamMove@@VObjectDir@@@@@stlpmtx_std@@YAXPAUNode@?$Ob` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 56/144 B |
| 22 | 564 @ `0x82346e50` | `??$_Copy_Construct@UNode@?$ObjPtrVec@VObject@Hmx@@VObjectDir@@@@@stlpmtx_std@@YAXPAUNode@?` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 56/128 B |
| 23 | 564 @ `0x82346e50` | `??$_Copy_Construct@UNode@?$ObjPtrVec@VRhythmDetector@@VObjectDir@@@@@stlpmtx_std@@YAXPAUNo` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 56/144 B |
| 24 | 564 @ `0x82346e50` | `??$_Copy_Construct@UNode@?$ObjPtrVec@VRndTex@@VObjectDir@@@@@stlpmtx_std@@YAXPAUNode@?$Obj` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 56/128 B |
| 25 | 564 @ `0x82346e50` | `??$_Copy_Construct@V?$vector@VRangeSection@@V?$StlNodeAlloc@VRangeSection@@@stlpmtx_std@@@` | WITHDRAW | BODY-ELSEWHERE | ours proven at 0x827fb758; claimed by 3 groups |
| 26 | 564 @ `0x82346e50` | `??$_Param_Construct@UBoneOp@CharSignalApplier@@U12@@stlpmtx_std@@YAXPAUBoneOp@CharSignalAp` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 56/84 B; claimed by 3 groups |
| 27 | 564 @ `0x82346e50` | `??$_Param_Construct@UMoveReplacer@@U1@@stlpmtx_std@@YAXPAUMoveReplacer@@ABU1@@Z` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 56/84 B; claimed by 6 groups |
| 28 | 564 @ `0x82346e50` | `??$_Param_Construct@UNode@?$ObjPtrVec@VFlow@@VObjectDir@@@@U12@@stlpmtx_std@@YAXPAUNode@?$` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 56/144 B |
| 29 | 564 @ `0x82346e50` | `??$_Param_Construct@UNode@?$ObjPtrVec@VFlowLabel@@VObjectDir@@@@U12@@stlpmtx_std@@YAXPAUNo` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 56/144 B |
| 30 | 564 @ `0x82346e50` | `??$_Param_Construct@UNode@?$ObjPtrVec@VFlowNode@@VObjectDir@@@@U12@@stlpmtx_std@@YAXPAUNod` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 56/144 B |
| 31 | 564 @ `0x82346e50` | `??$_Param_Construct@UNode@?$ObjPtrVec@VFlowOutPort@@VObjectDir@@@@U12@@stlpmtx_std@@YAXPAU` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 56/144 B |
| 32 | 564 @ `0x82346e50` | `??$_Param_Construct@UNode@?$ObjPtrVec@VHamCharacter@@VObjectDir@@@@U12@@stlpmtx_std@@YAXPA` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 56/144 B |
| 33 | 564 @ `0x82346e50` | `??$_Param_Construct@UNode@?$ObjPtrVec@VHamMove@@VObjectDir@@@@U12@@stlpmtx_std@@YAXPAUNode` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 56/144 B |
| 34 | 564 @ `0x82346e50` | `??$_Param_Construct@UNode@?$ObjPtrVec@VObject@Hmx@@VObjectDir@@@@U12@@stlpmtx_std@@YAXPAUN` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 56/128 B |
| 35 | 564 @ `0x82346e50` | `??$_Param_Construct@UNode@?$ObjPtrVec@VRhythmDetector@@VObjectDir@@@@U12@@stlpmtx_std@@YAX` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 56/144 B |
| 36 | 564 @ `0x82346e50` | `??$_Param_Construct@UNode@?$ObjPtrVec@VRndTex@@VObjectDir@@@@U12@@stlpmtx_std@@YAXPAUNode@` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 56/128 B |
| 37 | 564 @ `0x82346e50` | `??$_Param_Construct@V?$set@PBVMoveParent@@U?$less@PBVMoveParent@@@stlpmtx_std@@V?$StlNodeA` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 56/240 B; claimed by 6 groups |
| 38 | 564 @ `0x82346e50` | `??$_Param_Construct@V?$vector@VRangeSection@@V?$StlNodeAlloc@VRangeSection@@@stlpmtx_std@@` | WITHDRAW | BODY-ELSEWHERE | ours proven at 0x827fb758; claimed by 3 groups |
| 39 | 581 @ `0x823048e0` | `??$_Copy_Construct@U?$pair@$$CBVString@@V1@@stlpmtx_std@@@stlpmtx_std@@YAXPAU?$pair@$$CBVS` | WITHDRAW | LEAF-SIZE | leaf 120 B retail vs 56 B ours; claimed by 5 groups |
| 40 | 581 @ `0x823048e0` | `??$_Copy_Construct@ULocalizedName@HamMove@@@stlpmtx_std@@YAXPAULocalizedName@HamMove@@ABU1` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 120/84 B; claimed by 6 groups |
| 41 | 581 @ `0x823048e0` | `??$_Copy_Construct@V?$vector@VRangeSection@@V?$StlNodeAlloc@VRangeSection@@@stlpmtx_std@@@` | WITHDRAW | BODY-ELSEWHERE | ours proven at 0x827fb758; claimed by 3 groups |
| 42 | 581 @ `0x823048e0` | `??$_Param_Construct@UBoneOp@CharSignalApplier@@U12@@stlpmtx_std@@YAXPAUBoneOp@CharSignalAp` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 120/84 B; claimed by 3 groups |
| 43 | 581 @ `0x823048e0` | `??$_Param_Construct@UMoveReplacer@@U1@@stlpmtx_std@@YAXPAUMoveReplacer@@ABU1@@Z` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 120/84 B; claimed by 6 groups |
| 44 | 581 @ `0x823048e0` | `??$_Param_Construct@V?$set@PBVMoveParent@@U?$less@PBVMoveParent@@@stlpmtx_std@@V?$StlNodeA` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 120/240 B; claimed by 6 groups |
| 45 | 581 @ `0x823048e0` | `??$_Param_Construct@V?$vector@VRangeSection@@V?$StlNodeAlloc@VRangeSection@@@stlpmtx_std@@` | WITHDRAW | BODY-ELSEWHERE | ours proven at 0x827fb758; claimed by 3 groups |
| 46 | 601 @ `0x8237b938` | `??$_Copy_Construct@U?$pair@$$CBVString@@V1@@stlpmtx_std@@@stlpmtx_std@@YAXPAU?$pair@$$CBVS` | WITHDRAW | LEAF-SIZE | leaf 120 B retail vs 56 B ours; claimed by 5 groups |
| 47 | 601 @ `0x8237b938` | `??$_Copy_Construct@ULocalizedName@HamMove@@@stlpmtx_std@@YAXPAULocalizedName@HamMove@@ABU1` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 120/84 B; claimed by 6 groups |
| 48 | 601 @ `0x8237b938` | `??$_Param_Construct@UMoveReplacer@@U1@@stlpmtx_std@@YAXPAUMoveReplacer@@ABU1@@Z` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 120/84 B; claimed by 6 groups |
| 49 | 601 @ `0x8237b938` | `??$_Param_Construct@V?$set@PBVMoveParent@@U?$less@PBVMoveParent@@@stlpmtx_std@@V?$StlNodeA` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 120/240 B; claimed by 6 groups |
| 50 | 623 @ `0x8266c470` | `??$_Copy_Construct@U?$pair@$$CBVString@@V1@@stlpmtx_std@@@stlpmtx_std@@YAXPAU?$pair@$$CBVS` | WITHDRAW | LEAF-SIZE | leaf 172 B retail vs 56 B ours; claimed by 5 groups |
| 51 | 623 @ `0x8266c470` | `??$_Copy_Construct@ULocalizedName@HamMove@@@stlpmtx_std@@YAXPAULocalizedName@HamMove@@ABU1` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 172/84 B; claimed by 6 groups |
| 52 | 623 @ `0x8266c470` | `??$_Copy_Construct@V?$vector@VRangeSection@@V?$StlNodeAlloc@VRangeSection@@@stlpmtx_std@@@` | WITHDRAW | BODY-ELSEWHERE | ours proven at 0x827fb758; claimed by 3 groups |
| 53 | 623 @ `0x8266c470` | `??$_Param_Construct@UBoneOp@CharSignalApplier@@U12@@stlpmtx_std@@YAXPAUBoneOp@CharSignalAp` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 172/84 B; claimed by 3 groups |
| 54 | 623 @ `0x8266c470` | `??$_Param_Construct@UMoveReplacer@@U1@@stlpmtx_std@@YAXPAUMoveReplacer@@ABU1@@Z` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 172/84 B; claimed by 6 groups |
| 55 | 623 @ `0x8266c470` | `??$_Param_Construct@V?$set@PBVMoveParent@@U?$less@PBVMoveParent@@@stlpmtx_std@@V?$StlNodeA` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 172/240 B; claimed by 6 groups |
| 56 | 623 @ `0x8266c470` | `??$_Param_Construct@V?$vector@VRangeSection@@V?$StlNodeAlloc@VRangeSection@@@stlpmtx_std@@` | WITHDRAW | BODY-ELSEWHERE | ours proven at 0x827fb758; claimed by 3 groups |
| 57 | 638 @ `0x8232fb50` | `??$_Copy_Construct@U?$pair@$$CBVString@@V1@@stlpmtx_std@@@stlpmtx_std@@YAXPAU?$pair@$$CBVS` | WITHDRAW | LEAF-SIZE | leaf 224 B retail vs 56 B ours; claimed by 5 groups |
| 58 | 638 @ `0x8232fb50` | `??$_Copy_Construct@ULocalizedName@HamMove@@@stlpmtx_std@@YAXPAULocalizedName@HamMove@@ABU1` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 224/84 B; claimed by 6 groups |
| 59 | 638 @ `0x8232fb50` | `??$_Param_Construct@UMoveReplacer@@U1@@stlpmtx_std@@YAXPAUMoveReplacer@@ABU1@@Z` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 224/84 B; claimed by 6 groups |
| 60 | 638 @ `0x8232fb50` | `??$_Param_Construct@V?$set@PBVMoveParent@@U?$less@PBVMoveParent@@@stlpmtx_std@@V?$StlNodeA` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 224/240 B; claimed by 6 groups |
| 61 | 705 @ `0x822c9048` | `??$_Copy_Construct@USink@MsgSinks@@@stlpmtx_std@@YAXPAUSink@MsgSinks@@ABU12@@Z` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 104/104 B |
| 62 | 846 @ `0x8271a818` | `??1?$_Vector_base@UColorSet@@V?$StlNodeAlloc@UColorSet@@@stlpmtx_std@@@stlpmtx_std@@QAA@XZ` | WITHDRAW | BODY-ELSEWHERE | ours proven at fn_824DFA50 |
| 63 | 846 @ `0x8271a818` | `??1?$vector@UColorSet@@V?$StlNodeAlloc@UColorSet@@@stlpmtx_std@@@stlpmtx_std@@QAA@XZ` | WITHDRAW | BODY-ELSEWHERE | ours proven at fn_824DFA50 |
| 64 | 949 @ `0x826fe590` | `??$_Param_Construct@UCheat@CheatProvider@@U12@@stlpmtx_std@@YAXPAUCheat@CheatProvider@@ABU` | WITHDRAW | LEAF-SIZE | leaf 108 B retail vs 64 B ours |
| 65 | 957 @ `0x82516ad0` | `??0NetLoaderRef@@QAA@XZ` | WITHDRAW | BODY-SIZE | body 48 B retail vs 64 B ours |
| 66 | 971 @ `0x82442df8` | `?_M_erase@?$vector@V?$Key@V?$vector@VVector2@@V?$StlNodeAlloc@VVector2@@@stlpmtx_std@@@stl` | WITHDRAW | BODY-ELSEWHERE | ours proven at fn_82442E68; retail caller calls fn_82442E68 |
| 67 | 1056 @ `0x823aade0` | `?_M_fill_insert@?$vector@UDynamicPropertyEntry@Flow@@V?$StlNodeAlloc@UDynamicPropertyEntry` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 144/128 B |
| 68 | 1111 @ `0x824adbd0` | `??$_Copy_Construct@UEntry@LocalePanel@@@stlpmtx_std@@YAXPAUEntry@LocalePanel@@ABU12@@Z` | WITHDRAW | LEAF-SIZE | leaf 140 B retail vs 76 B ours |
| 69 | 1113 @ `0x827be190` | `?contains@FixedString@@QBA_NPBD@Z` | WITHDRAW | LEAF-BYTES | leaf 76/76 B, bytes differ, not located |
| 70 | 1405 @ `0x822b4eb0` | `?GetTotalDurationSeconds@HamCamShot@@QAAMXZ` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 200/200 B |
| 71 | 1426 @ `0x822b4df0` | `?GetNumShots@HamCamShot@@QAAHXZ` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 200/200 B |
| 72 | 1561 @ `0x82772b08` | `??$_Param_Construct@V?$vector@FV?$StlNodeAlloc@F@stlpmtx_std@@@stlpmtx_std@@V12@@stlpmtx_s` | WITHDRAW | LEAF-ELSEWHERE | our leaf callee proven at 0x82bb3430 |
| 73 | 1688 @ `0x82772208` | `??$_M_allocate_and_copy@PBUXUSER_ACHIEVEMENT@@@?$vector@UXUSER_ACHIEVEMENT@@V?$StlNodeAllo` | WITHDRAW | BODY-ELSEWHERE | ours proven at 0x824f18c8 |
| 74 | 1809 @ `0x82519f38` | `??$sort@PAPAVMoveDetector@@UMoveDetectorCmp@@@stlpmtx_std@@YAXPAPAVMoveDetector@@0UMoveDet` | WITHDRAW | TYPE-ABSENT | DC3 type; leaf 252/252 B |

The full evidence string for each membership is in its `withdrawn[]` record (`lane: "W16-TV 2026-10-07"`) in
`scripts/symbol_aliases.json`.

## 5. Not done

- The §4.1 leads (two unnamed homes, located bodies that might be admissions elsewhere).
- Other probe tools that call `relocs_agree` directly (`icf_relocname_census`, `foldprove2_cheapkill`,
  `incomplete_group_*`, `nogroup_census`, `scripts/icf_alias_fixpoint.py`, …) inherit the gap-2 refusal. They do **not**
  get the same-name read, because they bypass `adjudicate`. `relocs_agree`'s docstring now says it is necessary, not
  sufficient, for a proof.
- The 886 PROVEN memberships resting on a `SAMENAME-UNVERIFIED` slot (§1) are a bound, not a finding. They need source work
  on the callee, not alias work.
- The native gate was not run: no `src/` change; `tools/project.py` only changes the match build's ninja graph.
- Not merged, not pushed.
