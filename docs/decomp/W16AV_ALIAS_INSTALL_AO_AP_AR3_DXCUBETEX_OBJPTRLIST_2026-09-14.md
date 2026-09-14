# W16-AV — installing AO / AP / AR-3, and refusing two of them

Lane **W16-AV**, branch `w16-av`, based on main `6c35e385` (= `11506937` + docs).
Ruler `name_check`, objdiff 4.2.9 `5a51cd51fe0a353f`, `total_code` 10,246,004.

| | matched_functions | matched_code | matched_code_percent |
|---|---:|---:|---:|
| baseline (`11506937`) | 43,470 | 4,028,008 B | 39.312965 % |
| **this branch** | **43,477** | **4,030,792 B** | **39.340134 %** |
| Δ | **+7** | **+2,784 B** | +0.027169 pp |

Zero rows fell out of the `fuzzy==100` set across all four landed changes (set-diff, every step).

## Scoreboard

| item | verdict | predicted | measured |
|---|---|---|---|
| 1 AO `0x8246af50` resize | **INSTALLED** (+ a second group the proposal missed) | +2 / +244 B | **+3 / +664 B** |
| 2 AP-2 | already installed before this lane — **re-proven**, not re-installed | — | — |
| 2 AP-3 | **REFUTED as an alias**; replaced by a map repair | +2 / +132 B | **+2 / +132 B** |
| 3 AP-1 | **INSTALLED** (FT-EMPTY, positional) | +1 / +1,812 B | **+1 / +1,812 B** |
| 4 AR-3 rotation | **INSTALLED**, and AR-3's third name identified | +1 / +176 B | **+1 / +176 B** |
| 5 `Handle@DxCubeTex` / `Handle@NgFur` | **REFUSED — vacuous** | — | not installed |
| 6 `ObjPtrList` per-site | **REFUTED — the repair does not exist** | — | not installed, per "report and stop" |

Three of the four installed items measured **exactly** as predicted. The one that did not
(item 1) is discussed below, because the mis-prediction is the useful part.

## Briefed figures that did not survive literal testing

Recorded first, because every one of them was load-bearing for some proposal:

| briefed | measured |
|---|---|
| AO: the two COMDATs are byte-identical **including relocations** | **FALSE** — reloc target names differ at 2 sites |
| AP-2 needs installing | **already installed** |
| groups `0x8246af50` / `0x826ccb48` exist | **neither exists** |
| AP-3's survivor is `_Destroy<Grammar>` at `0x826ccb48` | **wrong** — the map row was mis-identified |
| item 6: the two map rows are `null` | **absent from the map entirely** (the map has 102 null rows; not these) |
| item 5: "identical vtable pointers ⇒ no override was emitted" | **circular under ICF** — a *folded* override also yields identical pointers |

## Item 1 — AO `0x8246af50` (INSTALLED, conclusion right, evidence wrong)

AO's conclusion is correct and its stated evidence is not. The two COMDATs are **not**
reloc-identical as compiled: `list<Transform>::resize` and
`list<RndMultiMesh::Instance>::resize` differ at two relocation targets, because each calls
its own `T`-specific `erase` and `insert`. Under `/OPT:ICF` that is exactly the shape that
does **not** fold.

What actually happens is **transitive folding**, which ICF reaches because it iterates to a
fixed point: the `erase` and `insert` families fold first (`Instance` has exactly one member,
`Transform mXfm` — `MultiMesh.h:94-107` — so the two instantiations generate identical code),
and *only then* do the two `resize` bodies become reloc-identical and fold in a later pass.
Proven on retail bytes with the relocated fields masked and every relocated branch destination
resolved through the map by name — not on map absence, which measures identification coverage
(~1.95x enrichment), not folding.

⇒ Installing AO's group alone would have been a half-fix: the `resize` row's own charge sits at
its **`insert`** call site. I installed both:

- `0x8246af50` — `resize@list<Transform>` (survivor) + `resize@list<RndMultiMesh::Instance>` — **T1**
- `0x824e0f00` — the corresponding `insert` family, **lane-discovered**, not in the proposal — **T1**

**Predicted +2 / +244 B, measured +3 / +664 B.** I did not bank the surplus until I had
mechanically identified the extra row: `?Save@WorldCrowd@@` (420 B), whose retail body at
`Save+248` branches to `0x824e0f00`. AO's own proposal accounts for **+1 / +100 B**; the
`insert` group I added accounts for **+2 / +564 B**.

**Root cause of the mis-prediction, stated plainly:** I enumerated beneficiaries only inside the
immediate `list<T>` family instead of across all call sites. Corrected method for the rest of the
lane — enumerate undefined-external references across *all* objs before predicting. Items 3 and 4
then measured exactly.

Commit `5134ce8a`.

## Item 2 — AP-2 re-proven, AP-3 REFUTED

**AP-2** (`0x82823340`, `IsLoaded@ObjDirPtr<ObjectDir>`) was **already installed** before this
lane. Re-proven, not re-installed.

**AP-3 is refuted on retail bytes.** The claim was that the 8-byte thunk
`?IsLoaded@DirectInstrument@@QAA_NXZ` (`addi r3,r3,4; b 0x82823340`) folds with the survivor
`??$_Destroy@UGrammar@SpeechMgr@@...` at `0x826ccb48`. Two 8-byte thunks fold only if their
branch destinations agree, and these do not:

- `_Destroy<Grammar>` tail-calls `String::~String` at `0x827bdf38`
- `DirectInstrument::IsLoaded` tail-calls `ObjDirPtr<ObjectDir>::IsLoaded` at `0x82823340`

Retail `0x826ccb48` branches to **`0x82823340`**, and decodes as a boolean predicate, not a
String destructor. Five other retail thunks do carry the `_Destroy<Grammar>` shape. objdiff
independently labels the charge `WRONG_CALLEE`.

⇒ the **map** was wrong, not the source. Repaired `0x826ccb48` to
`?IsLoaded@DirectInstrument@@QAA_NXZ` (`DirectInstrument.cpp:21-22` is literally
`return mDir.IsLoaded();`, and `mDir` sits at offset 4 — which is the `addi r3,r3,4`).
Safety checked before landing: 1 caller, no undefined-external reference to the old name
anywhere, `TrainerPanel.obj` defines the new name, and the map stays injective.

**Predicted +2 / +132 B, measured exactly +2 / +132 B.** Commit `71282df4`.

⚠ Note for W16-AU: `0x826ccb48` sits inside the span being re-homed to `DirectInstrument.cpp`.
This is a **map** row, address-keyed, so it survives the move — but it is now a *named* row, so
the unit it lands in must define that name or it reads 0%. `TrainerPanel.obj` defines it today.

## Item 3 — AP-1 `0x826c3888` (INSTALLED, FT-EMPTY)

`?PrintStats@HitTracker@@QBAXXZ` is a bare `blr`: its body is entirely `MILO_LOG` loops
(`HitTracker.cpp:41-60`), which compile out. Retail `0x826c3888` is exactly 4 bytes, `blr`.

The byte half of the proof is therefore **vacuous** — a relocation-free 4-byte body carries no
information, so any `blr` in the binary would "match". This is **FT-EMPTY**, and it needs a
**positional** witness, which I re-derived rather than inherited: retail's `Handle@GamePanel`
contains exactly two `lwz r3,-16(r27)`-preceded `bl` sites, at `+0x460` (the charged one) and
`+0x4a8` (which resolves to `?Reset@HitTracker@@`, an already-agreed name). The source seals it —
`GamePanel.cpp:666-667` is `HANDLE_ACTION(print_hit_stats, mHitTracker->PrintStats())` immediately
followed by `HANDLE_ACTION(clear_hit_stats, mHitTracker->Reset())`, in that order.

Blast radius measured before landing: exactly 1 referencing obj.

**Predicted +1 / +1,812 B, measured exactly +1 / +1,812 B** (`?Handle@GamePanel@@`, 1,812 B,
99.9890 -> 100). Commit `13a38546`.

*(I did not edit `src/band3/game/GamePanel.*` — W16-AT owns it. Read only.)*

## Item 4 — AR-3 rotation (INSTALLED, map-only) + the name AR-3 left unidentified

A clean 3-cycle over the `EventTrigger` `_M_splice_insert_dispatch` instantiations, derived from
the **caller** channel (which caller reaches which body), not from spelling:

| address | was | now |
|---|---|---|
| `0x824a2190` | `null` | `Anim` instantiation |
| `0x824a2230` | `Anim` | `ProxyCall` instantiation |
| `0x824c9878` | `ProxyCall` | **`EventCall@EventAnim`** |

The third name is the lane's own work — AR-3 filed the rotation but never said what
`0x824c9878` actually is. I read it out of `EventAnim.obj`'s COFF symbol table after building
(a fresh worktree's reflinked objs are pre-renamer; I asserted 80,880 mangled names among
495,647 total before trusting anything, and that 495,647 was later independently corroborated by
`verify_objs_patched.py`).

Injectivity verified across the **whole** map, not just the cluster: 2 pre-existing duplicate
names, unchanged by the rotation, no new ones.

**Predicted +1 / +176 B, measured exactly +1 / +176 B.** Commit `54c8ea9d`.

**Honest cost, recorded rather than hidden:** `0x824c9878` is now *unpairable* until the
`0x824c9xxx` cluster is re-homed to `EventAnim.cpp` (W16-AU's splits territory — I did not touch
`splits.txt`). The byte cost of that is **zero**, because the row was already below 100; it is a
correctness improvement that will pay when the re-home lands, not a regression.

## Item 5 — `?Handle@DxCubeTex@@` / `?Handle@NgFur@@`: REFUSED (vacuous)

AQ deferred these as "unprovable". The refusal is firmer than that, and on a different ground:
**the membership would be vacuous.**

An alias forgives a relocation-name charge only where *our* side emits or references the folded
spelling. Neither spelling exists anywhere on our side:

- **No `?Handle@DxCubeTex@@*` or `?Handle@NgFur@@*` symbol exists in any of our 1,215 compiled
  objs** — not as a definition, not as a reference (COFF symbol-table scan, post-build).
- Both are absent from `target_symbol_map.json`.

⇒ adding them to group 478 could not forgive a charge that does not exist. It would inflate the
group's membership count and nothing else — precisely the "unproven alias lifts the score by
construction" hazard, except here it would not even lift it.

Source agrees, which is why the symbols are absent: **neither class declares an override.**
`src/system/rnddx9/CubeTex.h:6` — `class DxCubeTex : public RndCubeTex` declares
`~DxCubeTex, Select, Reset, Sync`, no `Handle`; the `virtual DataNode Handle(DataArray*, bool)`
is on the base, `src/system/rndobj/CubeTex.h:42`. `src/system/rndobj/Fur_NG.h:8` —
`class NgFur : public RndFur` declares `Prep, Shell` and a ctor, no `Handle`; the virtual is on
`RndFur` (`Fur.h:12,16`).

⚠ **The brief's suggested test is circular and must not be re-filed.** It proposed comparing
`??_7DxCubeTex@@6B@` slot 6 against the base's slot 6, "if identical pointers, no override was
emitted". Under ICF a *folded* override produces identical pointers too — the test cannot
distinguish the two cases it exists to distinguish. The vacuity argument above is independent of
it.

## Item 6 — `ObjPtrList` per-site repairs: REFUTED, reported and stopped

The brief says "if it is not a one-line change per site, report and stop". It is not a one-line
change, and the reason is that **the premise is wrong three separate ways.**

**(a) The two briefed addresses are not `ObjPtrVec`/`ObjPtrList` instantiations at all.** Both
decode as `ObjPtr<T>` **constructors**. Each stores an `.rdata` vtable pointer into `0(this)`,
copies the 12-byte `{vptr, owner, ptr}` layout out of `r4`, and calls
`?AddRef@Object@Hmx@@QAAXPAVObjRefOwner@@@Z` at `0x8275bd08`. The vtables identify `T`
unambiguously:

| address | vtable | slot 0 | slot 8 | ⇒ |
|---|---|---|---|---|
| `0x82706068` | `0x820f70a8` | `??_G?$ObjPtr@VSeqInst@@@@` | `?Replace@?$ObjPtr@VSeqInst@@@@` | `ObjPtr<SeqInst>` ctor |
| `0x8229dc70` | `0x82017a34` | `??_G?$ObjPtr@VRndTransformable@@@@` | `?Replace@?$ObjPtr@VRndTransformable@@@@` | `ObjPtr<RndTransformable>` ctor |

(`0x82706068` is a *different* `ObjPtr<SeqInst>` ctor from the already-named
`??0?$ObjPtr@VSeqInst@@@@QAA@ABV0@@Z` at `0x823daab8`: it passes `this` as the `ObjRefOwner` and
needs no `this`-adjustor, where `0x823daab8` loads the stored owner and adjusts. They therefore
do not fold, which is why both exist.)

No `ObjPtrVec<T>` -> `ObjPtrList<T>` member-declaration change can affect either body.

**(b) Where the briefed addresses came from.** AQ's item 5 says "six map rows name `ObjPtrVec`;
three are at 100%, one at 99.67, two at 0%", then names `0x82706068` / `0x8229dc70` as the
targets. Those two addresses are **not among the six**. The nearest is `0x82706100` =
`_Copy_Construct<ObjPtrVec<RndGroup>::Node>`, which is **0x98 bytes after** `0x82706068`. It is a
transposition to a neighbouring address, and it propagated into this lane's brief unchallenged.

**(c) The rows that *are* at 0% cannot be fixed by a spelling change either.** The real two:

| address | symbol | size | pinned unit | fuzzy |
|---|---|---:|---|---:|
| `0x8278b7f0` | `__uninitialized_copy<ObjPtrVec<Hmx::Object>::Node*>` | 72 B | `default/HamMove` | 0.0 |
| `0x822abd60` | `_M_fill_insert<vector<ObjPtrVec<RndDrawable>::Node>>` | 112 B | `default/ClipDistMap` | 0.0 |

COFF scan over all 1,215 compiled objs (3,033,976 symbols):

- `0x8278b7f0`'s symbol **is defined by `HamMove.obj`** — the very unit it is pinned to — plus 4
  others. So it already pairs, and its 0% is a **real code divergence**, not a naming or pairing
  failure. Renaming it to the `ObjPtrList` spelling would **unpair** it (no obj defines that
  spelling) and strand it at 0% permanently. Strictly worse.
- `0x822abd60`'s symbol is defined by **no obj at all**, under **either** spelling. The
  instantiation simply does not exist in our build, so no declaration change can conjure it.

⇒ There is no one-line per-site repair here. **Nothing installed; no `src/` file touched.**
AQ's underlying observation still stands (retail has one template, named `ObjPtrList`; we carry a
duplicate `ObjPtrVec`) — but the two addresses attached to it were wrong, and the two correct
addresses are not spelling problems. A future lane should start from `0x8278b7f0` as an ordinary
**divergence** row (72 B, already pairing) and treat `0x822abd60` as an **absent instantiation**,
not as a rename.

## NOT done, and why

1. **Item 5 not installed** — refused as vacuous (neither spelling exists on our side). Refutation
   above; it is specific enough that it should not be re-filed.
2. **Item 6 not installed** — refuted; the brief's own "report and stop" clause applies. The
   corrected targets are recorded above for whoever picks it up.
3. **`0x824c9878` left unpairable** — it needs the `0x824c9xxx` cluster re-homed to
   `EventAnim.cpp`, which is `splits.txt`, owned by W16-AU. Zero byte cost today.
4. **`src/` untouched** — the only sanctioned `src/` edit was item 6's declaration change, and
   item 6 was refuted before any edit was justified.
5. **`GamePanel.*`, `splits.txt`, `objects.json` untouched** — owned by W16-AT / W16-AU.
6. **AO's `0x8246af50` `.xdata` equality asserted only via extent equality**, not by decoding the
   unwind records field by field. The fold is proven by the reloc-masked body comparison plus the
   call-site branch evidence; the `.xdata` half rests on both COMDATs having equal retail extent.
