# Wave-6 Waypoint.cpp pin-relocation audit (adversarial honesty gate)

**Date:** 2026-06-16
**Auditor:** honesty-gate subagent (read-only on main; A/B in worktree)
**Branch under review:** `w6-pinaudit2` @ `ad96ec7`
**Worktree:** `/home/free/code/milohax/wt-w6-pinaudit2`
**Change:** splits.txt-only (2 lines). Relocate `Waypoint.cpp` `.text` pin from the
dead 0x50-byte ICF sliver `[0x822C8CA8,0x822C8CF8)` to the cluster
`[0x823C7CC8,0x823CA668)` (+ dtk-derived `.pdata [0x821FF580,0x821FF7D8)`).

## VERDICT: **LAND +31** (zero binary-wide regressions). The wave-3 refutation is STALE / was MISATTRIBUTED.

The wave-3 "DISHONEST ATTRIBUTION" verdict on "Waypoint +31" does **not** hold against
this cluster. Its two load-bearing claims are both refuted below by the DC3 leaked map
and the dtk/objdiff byte-level pairing.

---

## 1. Clean A/B (the decisive measurement)

Built BOTH the branch parent and the proposal in the SAME worktree on identical machinery
(the only honest baseline — the *main* `report.json` is volatile, being rebuilt by
concurrent agents; an early cross-read against a transient main snapshot produced a phantom
"SongMgr -7" that VANISHED under a proper same-tree A/B):

| State | splits Waypoint `.text` | total `matched_functions` |
|---|---|---|
| Baseline (parent `62dd882`, dead sliver) | `[0x822C8CA8,0x822C8CF8)` | **8173** |
| Proposal (`ad96ec7`, cluster) | `[0x823C7CC8,0x823CA668)` | **8204** |

**NET DELTA = +31. Per-unit diff: ONLY `default/Waypoint 0 -> 31`. ZERO regressions.**
The old sliver contributed 0 (dead pin), so unit +31 == binary +31.

## 2. The cluster IS a real contiguous Waypoint TU (DC3 corroboration — REVERSES wave-3)

Wave-3 claimed: *"DC3 ham_xbox_r.map corroborates Waypoint is scattered across
Character.obj/CharServoBone.obj/ClipCollide.obj — NOT a contiguous TU."* **This is false.**

DC3's `ham_xbox_r.map` has a real, contiguous `char:Waypoint.obj` TU. Its `.text` method
cluster runs DC3 `0x823CBAC0` (StaticClassName) → `0x823CEF60` (`??_GWaypoint` sdtor),
in the SAME method order as the RB3 pin:
`ShapeDeltaBox → ShapeDeltaAng → FindNearest → OnWaypoint{Find,Nearest,Last} → Constrain
→ ShapeDelta×2 → Handle → Save → Terminate → ... → Copy → SyncProperty → Load → Init`.
RB3 `0x823C7CC8` ≈ DC3 `0x823CBAC0` (constant ~0x4000 TU-offset). Interleaved within it are
**Waypoint's OWN** template instantiations: `vector<Node<ObjPtrVec<Waypoint>>>`,
`list<Waypoint*>::erase`, `StlNodeAlloc<...Waypoint...>`, `PropSync<Waypoint>`, `Rand::Int`,
`DataArray::Obj<RndTransformable>`. These are the textbook *"own STL/templates bracketed by
own named methods = OK"* honesty-gate case.

What IS scattered (the kernel of truth wave-3 over-read): `ObjPtr<Waypoint>` /
`ObjRefConcrete<Waypoint>` / `ObjPtrList<Waypoint>` COMDATs (folded into
CharServoBone.obj/ClipCollide.obj/CharacterTest.obj), and the `valid_waypoint`/`list_waypoints`
**string literals** (used by ClipCollide). Those are *uses of* Waypoint, not Waypoint's own
methods — they live outside the pinned `.text` range. The named FileMergerOrganizer.obj
methods that abut the cluster sit at DC3 `0x823CB184..0x823CBA74`, i.e. **below**
StaticClassName (`0x823CBAC0`) — and the RB3 pin starts AFTER, at ShapeDeltaBox
(`0x823C7CC8`). FileMergerOrganizer is NOT in the pinned range.

## 3. The "18-fn foreign run at 0x823C8A48..0x823C9D58" was ICF aliases mis-read as foreign TUs

`0x823C8A48` is `?Save@Waypoint@@UAAXAAVBinStream@@@Z` per `target_symbol_map.json` AND per
the asm body (it streams Waypoint members). The DC3 map shows the "FileMerger / HamNavList /
InlineHelp / Watcher / CameraShot / Sequence" symbols wave-3 listed are **ICF address-aliases
at the same VA as Waypoint's own functions** (e.g. at DC3 `0x823CC828`, six names —
`??_E?$ObjPtr@VWaypoint@@`, `??_E?$ObjRefConcrete@VWaypoint@@`, `??_ENode@?$ObjPtrVec@VWaypoint@@`
… — share one byte sequence; the linker picked Waypoint's copy primary). They are the SAME
bytes wearing alias names, not separately-placed interleaved functions. The honesty gate's
"foreign-NAMED @<100 run" misses them precisely because, after ICF, they read 100% — but
they are not foreign *bodies*, they are Waypoint's own bodies.

## 4. Per-address true-owner table — all 25 matched anon fns

Every matched anon fn is either (a) an MSVC X360 **funclet** (exception-unwind cleanup
handler — `subi r31,r12,0xNN; mflr r12; stw r12,-8(r1); stwu r1,-0x60(r1)` frame-reconstruct
prologue, then one cleanup `bl` on a parent-frame local, then `blr`), or (b) a trivial
bit-manip helper on Waypoint's own static registry global (`lbl_82C8E4D8+0x15C/+0x17C/+0x19C`
= Waypoint TypeProp/registration state). All physically inside the DC3-corroborated
Waypoint.obj TU, bracketed by Waypoint's own named methods. **0 foreign.**

| RB3 addr | kind | body summary | owner |
|---|---|---|---|
| 0x823C7FA4 | funclet | `lwz r3,0x84(r31); bl <dtor>` | Waypoint (own) |
| 0x823C80A0 | funclet | `lwz r3,0x84(r31); bl <dtor>` | Waypoint (own) |
| 0x823C81F4 | funclet | `lwz r4,0x84;lwz r3,0x50; bl` | Waypoint (own) |
| 0x823C873C | funclet | `addi r3,r31,0x58; bl DataArray::Release-ish` | Waypoint (own) |
| 0x823C8764 | funclet | `addi r3,r31,0x..; bl Release-ish` | Waypoint (own) |
| 0x823C88CC | funclet | `addi r3,r31,0x50; bl Release-ish` | Waypoint (own) |
| 0x823C8968 | funclet | destroy_range on parent vec + `_CxxThrowException` rethrow | Waypoint (own) |
| 0x823C8A10 | funclet | destroy_range on parent vec + rethrow | Waypoint (own) |
| 0x823C9594 | helper | clrrwi bit-clear on `lbl_82C8E4D8+0x17C` (Waypoint static) | Waypoint (own) |
| 0x823C9818 | funclet | `lwz r11,0x94(r31); addi r3,r11,0xb8; bl` (member dtor) | Waypoint (own) |
| 0x823C9844 | funclet | `lwz r11,0x94(r31); addi r3,r11,0xd0; bl` (member dtor) | Waypoint (own) |
| 0x823C9BC8 | funclet | `addi r3,r31,0x58; bl Release-ish` | Waypoint (own) |
| 0x823C9BF0 | funclet | `addi r3,r31,0x60; bl <dtor>` | Waypoint (own) |
| 0x823C9C18 | funclet | `addi r3,r31,0x60; bl fn_823C7F30` (INTERNAL Waypoint ctor) | Waypoint (own) |
| 0x823CA0E4 | helper | clrrwi bit-clear on `lbl_82C8E4D8+0x19C` (Waypoint static) | Waypoint (own) |
| 0x823CA104 | helper | bit-op on `+0x19C` | Waypoint (own) |
| 0x823CA124 | helper | bit-op on `+0x19C` | Waypoint (own) |
| 0x823CA144 | helper | rlwinm bit-clear on `+0x19C` | Waypoint (own) |
| 0x823CA164 | funclet | `addi r3,r31,0x58; bl Release-ish` | Waypoint (own) |
| 0x823CA18C | helper | bit-op on `+0x19C` | Waypoint (own) |
| 0x823CA1AC | helper | bit-op on `+0x19C` | Waypoint (own) |
| 0x823CA1CC | funclet | `addi r3,r31,0x60; bl Release-ish` | Waypoint (own) |
| 0x823CA1F4 | helper | rlwinm bit-clear on `+0x19C` | Waypoint (own) |
| 0x823CA304 | funclet | `addi r3,r31,0x50; bl <dtor>` | Waypoint (own) |
| 0x823CA32C | funclet | `addi r3,r31,0x50; bl fn_823C7F30` (INTERNAL Waypoint ctor) | Waypoint (own) |

**Own-vs-foreign split of the 25 anon fns: 25 own / 0 foreign.**
Plus the 6 named are unambiguously Waypoint's own. Total 31 = 31 own.

Corroboration: our compiled `Waypoint.obj` emits only 13 NAMED symbols — 9 genuinely
Waypoint (ShapeDeltaBox, ShapeDeltaAng, OnWaypointLast, Constrain, ShapeDelta×2, Save, Init,
Copy) + 4 of Waypoint's OWN dependent templates (`vector<ObjPtr<SeqInst>>::_M_fill_insert`/
`::resize`, `ObjVector<Constraint@BandIKEffector>::operator=`, `RndLine::~RndLine` — Waypoint
holds SeqInst sequences, BandIKEffector constraints, and an RndLine). All FOUR of those
foreign-*named* templates currently read **0%** (unported), are **scattered/non-contiguous**,
and are Waypoint's own instantiations — not folded foreign bodies attributed to the pin.

Spot-checked via objdiff (normalized, in worktree): `fn_823CA0E4` 100% (8 insns all equal),
`fn_823C8968` 100% (14 insns all equal) — honest byte-exact pairing to our compiled
`Waypoint.obj`, not an ICF mis-pair.

## 5. Longest contiguous FOREIGN run attributed to Waypoint

**Zero.** No matched fn in the unit is foreign. The longest contiguous *below-100%* run
(`fn_823C8B58 .. fn_823C9600`, ~14 fns) is UNMATCHED Waypoint-own `vector<Node<ObjPtrVec<
Waypoint>>>` machinery (DC3: `_M_insert_overflow`, `__uninitialized_copy`, `reserve`,
`push_back`, all `char:Waypoint.obj`) — porting-incomplete, not foreign. There is **no
>=8-contiguous run of FOREIGN fns (matched or not)** attributed to Waypoint.

## Honesty-gate disposition

- matched > 0 ✓ (31)
- no >=8-contiguous FOREIGN fn run ✓ (the long below-100 run is Waypoint's own STL, bracketed by own named methods)
- clean same-tree A/B, zero regressions ✓ (+31, only Waypoint changes)
- DC3 map proves the cluster is a real contiguous Waypoint.obj TU ✓

**LAND +31.** Record correction: the wave-3 "Waypoint relocate (COMDAT template-scatter,
DC3 map corroborates) — REFUTED/DISHONEST" verdict was about the dead sliver and rested on
(a) reading ICF address-aliases as separate foreign TUs and (b) a misreading of the DC3 map
(the scatter is of `ObjPtr<Waypoint>` *uses* + string literals, while Waypoint's own methods
are a contiguous `char:Waypoint.obj`). It does not apply to `[0x823C7CC8,0x823CA668)`.
The MEMORY.md "Waypoint +31 ... reverted" example should be re-tagged: the +31 here is HONEST.
