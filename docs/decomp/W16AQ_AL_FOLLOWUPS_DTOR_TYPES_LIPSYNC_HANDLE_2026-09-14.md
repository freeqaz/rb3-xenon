# W16-AQ — W16-AL's filed follow-ups: dtor container types, CharLipSync::Handle, OvershellPanel, splits re-home

Lane: **W16-AQ** · worktree `~/tmp/wt-w16-aq` · branch `w16-aq` · base main `73f16555`
Ruler: objdiff 4.2.9, `functionRelocDiffs=name_check` (read from `report.json` `provenance.diff_config`).

## Lane-internal measures

| | matched_functions | matched_code | matched_code_percent |
|---|---:|---:|---:|
| baseline (`73f16555`) | 43,442 | 4,022,704 B | 39.2612 % |
| final | **43,449** | **4,024,952 B** | **39.28314 %** |
| delta | **+7** | **+2,248 B** | +0.02194 pp |

`total_code` 10,246,004 (read from `report.json`, not inherited). Every figure below is a
set-diff of the `fuzzy==100` row set via `tools/rowset_snapshot.py`, run inside this worktree.

| item | commit(s) | predicted | measured |
|---|---|---|---|
| 1 dtor container types | `0dc8f340`, `47428c04`, `af101e08` | +2 rows / +280 B | **+5 fns / +700 B** |
| 2 `CharLipSync::Handle` | `720d3815` | +1 fn / +164 B | **+1 fn / +164 B** |
| 3 `ResolvePartWaitStates` | `a1156aa5` | +0 fns / +1,356 B | **+0 fns / +1,356 B** |
| 4 splits re-home | `280791e9` | +1 fn / +28 B | **+1 fn / +28 B** |
| 5 `ObjPtrList`/`ObjPtrVec` | — | report only | report only, below |

Cumulative crossed-in rows at the end of the lane (12 rows, 2,344 B in; 1 row, 124 B out):

```
+ 1356 B  default/OvershellPanel::?ResolvePartWaitStates@OvershellPanel@@QAAXXZ
+  192 B  default/TourProgress::??1TourProgress@@UAA@XZ
+  164 B  default/CharLipSync::?Handle@CharLipSync@@UAA?AVDataNode@@PAVDataArray@@_N@Z
+  124 B  default/TourProgress::??$?6VSymbol@@H@@...hash_map...   (operator<<)
+  124 B  default/TourProgress::??$?5VSymbol@@H@@...hash_map...   (operator>>)
+   88 B  default/band3/meta_band/NameGenerator::??1NameGenerator@@UAA@XZ
+   60 B  default/TourProgress::?SetTourMostStarsMap@...hash_map...
+   60 B  default/TourProgress::?SetToursPlayedMap@...hash_map...
+   44 B  default/TourProgress::fn_82362F0C / fn_82362F38 / fn_823642C8 / fn_823642F4
+   28 B  default/band3/game/BandUserMgr::?GetBandUser@BandUserMgr@@SAPAVBandUser@@PAVUser@@@Z
-  124 B  default/TourProgress::??$?5VSymbol@@H@@...std::map...   (the item-1 rename; net 0)
```

The single FELL OUT row is item 1's own rename — the same function under its old `std::map`
spelling — so it is net 0, not a regression.

## Item 1 — the two dtor rows: AL's claim VERIFIED, and the fix was a source type change

AL's "container-type source divergence" claim was tested **both ways** as the brief required, and
it is correct. Three independent retail-byte channels, none of them the map:

1. Our build emits the `0x826100f8` survivor spelling in `default/CharacterCreatorPanel` at 104 B,
   fuzzy 100.0, byte-identical to retail's `.pdata` extent including relocation names ⇒ the address
   really is a **hashtable** dtor. An `_Rb_tree` has no bucket vector to free.
2. Retail `??1TourProgress` (`0x82362d70`) calls `0x826100f8` **twice**; `??1NameGenerator`
   (`0x82643958`) calls `0x8260ffd8` then `0x826100f8`; and the hashtable dtor's own body begins
   with `bl 0x8260ffd8`, identifying that 96 B function as `hashtable::clear()`.
3. Both `TourProgress` setters target-call `??4?$hashtable@...::operator=` where we called
   `??4?$_Rb_tree@...`.

So the members are `std::hash_map`, not `std::map`. Fixed in source
(`TourProgress.h/.cpp`, `NameGenerator.h/.cpp`, `Tour.cpp`), with the house
`RB3_HASH_SYMBOL_DEFINED`-guarded `hash<Symbol>` specialisation.

**Counter-evidence disarmed.** The sibling 100% rows constrain nothing: every container-touching
call in them is out-of-line to an **anonymous** retail target (`fn_827B0E78` = `operator[]`,
`fn_8256A220` = `GetNameList`), and `name_check` *forgives* placeholder target names. Those 100%
readings were placeholder forgiveness, not agreement — the documented trap.

The map repair was **mandatory, not opportunistic**: the intermediate source-only build measured
`SyncLoad` (220 B) + `operator>>` (124 B) falling out, −344 B, because the renamed instantiation
un-paired its rows. Four map rows were repaired. A bonus fell out of it: `0x823629b0` was
anonymous at 0% (124 B) and is `operator<<(BinStream&, const hash_map&)`; naming it paired it and
it matched.

Item 1(b), the fold, was proven rather than assumed: retail's setters both `bl 0x825af110`, whose
80 B body is byte-identical (fuzzy 100) to our `<Symbol,DataArray*>` `operator=` in
`InterstitialMgr`; our two `operator=` COMDATs are both 80 B with **identical raw bytes**, differing
only in two relocations to the per-`T` `clear`/`_M_copy_from`; our two `clear` COMDATs are 96 B
raw-identical and retail's single `clear` at `0x8260ffd8` is exactly 96 B. Predicted +2/+120,
measured exactly +2/+120 (alias group 1645).

**A live ODR hazard was found and deliberately NOT resolved.** Adding `hash_map` stream operators
to `BinStream.h` fails to compile (C2995) because two pre-existing TU-local copies already exist
**and they differ from each other**: `SongMgr.cpp` has no `map.clear()` and loops
`for (; size != 0; size--)`, while `BandSongMetadata.cpp` has `map.clear()` and loops
`while (size-- != 0)`. Which body wins for existing consumers is currently arbitrary. I added a
third TU-local copy in `TourProgress.cpp` in the SongMgr (no-clear) form, because `SyncLoad` clears
explicitly and the 100%-matching `std::map` instantiation had no clear. Fixing the hazard properly
would change which body wins for existing consumers — not this lane's change.

## Item 2 — `CharLipSync::Handle`: retail has NO cases, and the brief's framing was wrong

**Predicted +1 fn / +164 B; measured exactly that** — but only after a second defect was fixed.

The brief called our 412 B `Handle` a "DC3-newer over-implementation". That is **wrong and worth
recording**: rb3-Wii (`../rb3/src/system/char/CharLipSync.cpp:281`) and DC3 **both** carry
`HANDLE(parse, OnParse)` and `HANDLE(parse_array, OnParseArray)`. The two oracles agree with each
other and both disagree with retail. This is a retail-vs-oracle divergence, not oracle drift.

Retail bytes settle it. `0x823d3918` (164 B) is the bare Milo forwarder: `_msg->Sym(1)`,
immediately `bl ?Handle@Object@Hmx@@`, then the `END_HANDLERS` tail
(`cmpwi r11,6` / `PathName` / `DataNode(kDataUnhandled)`). There is no Symbol compare anywhere,
so there are zero message cases.

Identification rests on **RTTI**, not on the spatial/unit-ownership argument CLAUDE.md rates at
~66% precision: `tools/retail_rtti.py owner 0x823d3918` returns 16 classes holding that address at
vtable **slot 6**, including `.?AVCharLipSync@@` (vtable `0x8205288c`). Control:
`owner 0x8275bd78` (`?Handle@Object@Hmx@@`) returns `.?AVObject@Hmx@@` slot 6, so slot 6 really is
`Handle`; `--selftest` passes 8/8.

Removing the two `HANDLE` lines is clean because this TU does not define
`RB3_HANDLE_LOCAL_STATIC`, so `HANDLE` compares against **global** Symbols and pulls in no local
statics, no guard words, no `??__E`/`??__F`. The source-only control build measured **exactly zero
collateral** (set-diff unchanged) and took the COMDAT **604 → 212 B** = retail's 164 B body + the
40 B EH funclet + an 8 B EH prefix.

### The naming trap, realised and then repaired

Naming the map row paid +164 B and cost −164 B in the same build: `?Handle@UIListSlot@@` fell out.
That is the documented `name_check` economics, not a surprise — naming an anonymous address
converts placeholder-**forgiven** call sites into **checked** ones. Retail's
`?Handle@UIListSlot@@` (`0x828152f0`) does `bl 0x823d3918` for its
`HANDLE_SUPERCLASS(UIListWidget)`; that site was free while the address was anonymous.

The cause was a **survivor misidentification already in the tree**. Group 478 sits at
`0x823d3918` with 20 folded `Handle` spellings but carried the survivor
`??$__uninitialized_copy@PAVString@@...`, chosen by `icf_alias_build.py` while the address was
anonymous and therefore never tested against a target name. Refuted three ways: that spelling
compiles to **168 B** in all 20 TUs that emit it (the case-less `Handle` COMDAT is 212 B); it is
absent from `target_symbol_map.json` at **every** address; and the retail body is a `Handle`, not a
String copy loop. The old survivor is recorded in `withdrawn` — never silently dropped — and all 20
folded memberships are preserved.

### Proving the fold rather than assuming it

The brief forbids adding a membership to forgive a real divergence, so the rival hypothesis —
*"those 16 classes never override `Handle`, they simply inherit it"* — had to be killed. It is
refuted by RTTI: **14 of the 16 derive directly from `Hmx::Object`** (`numBaseClasses=3`, bases
self / `Object@Hmx` / `ObjRef`; `PerformanceData` adds `FixedSizeSaveable`), and `Hmx::Object`'s own
slot 6 is `0x8275bd78`. Inheriting would have left `0x8275bd78` in the slot. So 14 distinct
overriding definitions collapsed onto **one** address, which under `/OPT:ICF` means identity
including relocations.

Corroborated on our own bytes: `?Handle@CharLipSync@@` / `@UIListWidget@@` / `@UIColor@@` /
`@UIGuide@@` compile to 212 B bodies that are **raw-byte identical** with all 8 real callee
relocations identical (`?Sym@DataNode@@`, `?Handle@Object@Hmx@@`, `??0DataNode@@`,
`?Release@DataArray@@`, `?PathName@@`, `??1DataNode@@`, `__savegprlr_26`, `__restgprlr_26`). The
**only** differing relocation is the per-function `__ehfuncinfo$` descriptor, itself an
identical-content COMDAT that folds. A case-less
`BEGIN_HANDLERS`/`HANDLE_SUPERCLASS(Hmx::Object)`/`END_HANDLERS` body mentions its class nowhere,
which is exactly why every such `Handle` folds.

**Deliberately NOT added:** `?Handle@DxCubeTex@@` and `?Handle@NgFur@@`. Both hold `0x823d3918` at
slot 6, but their bases (`RndCubeTex`, `RndFur`) are themselves fold members, so their slot 6 could
be plain inheritance rather than a folded override. Unprovable by this method, so excluded.

## Item 3 — `ResolvePartWaitStates`: a real semantic divergence, not "relocation noise"

**Predicted +1,356 B / +0 fns; measured exactly that.** The row was fuzzy 99.98525 with `mpn`
already 100, so `matched_functions` is structurally incapable of registering this fix — a
bytes-only win by construction.

W16-AK left this charge undiagnosed and said so honestly: 2 of the row's 3 charges were `push_back`
fold-aliases it could not install, so the third paid it nothing. Those aliases are installed now,
which made the branch-dest the only thing between this row and 1,356 B.

`diff_inspect --mode diagnose` calls it *"Branch destination diffs: 1 (address relocation noise)"*.
**That label is wrong**, and it is why the charge sat unexamined: a branch destination *inside* a
function is control flow, not a relocation. Normalising both instruction streams to
function-relative offsets (target base `0x416c`, base `0x1b708`):

```
[150-152] state != kState_ChoosePartWait -> +0x298 on BOTH sides (push_back)
[153-160] RepresentSamePart(user->GetTrackType(), other->GetTrackType())
[161]     result false -> retail +0x2b8 (loop increment)
                       -> ours   +0x298 (push_back)
```

The only path that differs is `state == kState_ChoosePartWait && !RepresentSamePart`: retail skips
`priorityUsers.push_back(other)` **and** the `allWaiting` update and continues the loop; we
performed the push_back. That is behavioural, not layout.

The **asymmetry is the proof**: with the rb3-Wii oracle's single `&&`, both ways of failing the
condition jump to the same block, so the state-check failure and the `RepresentSamePart` failure
could not target different addresses. Retail must therefore nest the tests and `continue` out of
the inner one. The oracle carries the `&&` form, so this is the same retail-vs-oracle class as
item 2.

## Item 4 — splits re-home: a MOVE, not an extension

**Predicted +1 fn / +28 B; measured exactly that**, no fallout. Re-homing an already-pinned address
is not metric-neutral (PINHOME-1), so it was priced by set-diff rather than assumed free.

The 28 B at `0x82682668` was pinned to `CharLipSync.cpp` but the body is a tail
`b __RTDynamicCast` with two RTTI descriptors — exactly what
`BandUser *BandUserMgr::GetBandUser(User *u) { return dynamic_cast<BandUser*>(u); }`
(`BandUserMgr.cpp:87`) compiles to. The map row was already correct, so the row read 0% purely
because objdiff pairs target↔base **by name within a unit** and `default/CharLipSync`'s base obj
cannot define that symbol.

The brief asked what lies in `0x82682684`–`0x826826A8` first, and the answer is why this is a move
rather than an extension: `pdata_extent` reports **one 80 B unwind record covering
`0x82682618`–`0x826826d0`**, i.e. a whole run of `__RTDynamicCast` thunks. The gap holds 4 B of
padding, a **complete second 28 B thunk** at `0x82682688`–`0x826826A4` (different RTTI descriptors:
`0x82c72528`/`0x82c6e6b0` vs our `0x82c6da58`/`0x82c6e6c8`), and 4 B more padding. Extending
`BandUserMgr`'s `.text` back to `0x82682668` would have absorbed a function I have **not**
identified.

Only `.text` lines were touched. The re-split left `.pdata` unchanged, which is consistent rather
than suspicious: the covering record starts at `0x82682618`, outside both units' `.text` ranges, so
it was never attributed to `CharLipSync`. `symbols.txt` came back unchanged ⇒ split at a fixed point.

## Item 5 — `ObjPtrList` vs `ObjPtrVec` (REPORT ONLY, nothing installed)

Briefed figures tested literally and **confirmed on retail bytes**: `ObjPtrList` occurs **45** times
in `orig/45410914/band.exe`, `ObjPtrVec` **0**.

But the briefed *framing* — "our build emits 0 `ObjPtrList` `_Copy_Construct` rows because our
source spells `ObjPtrVec`" — is only half right, and the half that is wrong matters:

- It is true **for `_Copy_Construct` specifically**: 0 `ObjPtrList` `_Copy_Construct` symbols vs
  **165** `ObjPtrVec` ones.
- It is false as a statement about our source. Our objs contain **13,145** symbols naming
  `ObjPtrList` and **9,122** naming `ObjPtrVec`. Our tree carries **two separate templates**
  (`Object.h:976` `ObjPtrVec`, `Object.h:1182` `ObjPtrList`), both live.
- The map already names **152** rows `ObjPtrList`, of which 151 appear in `report.json`
  (22,984 B) and **131 are already at fuzzy 100**. So the `ObjPtrList` spelling is not missing from
  our build at all — it is missing only at the specific instantiations behind the held memberships.
- Six map rows name `ObjPtrVec`; three are at 100%, one at 99.67, two at 0%. A 100% there does
  **not** prove retail spelled it `ObjPtrVec` — map names are our reconstruction, and the two
  templates generate identical code, which is precisely why the bodies match either way.

⇒ Retail has **one** template, named `ObjPtrList`; we carry a duplicate of it named `ObjPtrVec`.
The repair is **per-site** (change the specific member declarations whose instantiations back
`0x82706068` / `0x8229dc70` from `ObjPtrVec<T>` to `ObjPtrList<T>`), not a global rename of 148
occurrences. Both target addresses are currently **`null`** in the map, so naming them with the
`ObjPtrList` spelling *before* the source change would strand them at 0% — which the brief
forbids. **Nothing installed.**

## NOT done, and why

1. **Item 5 not implemented** — report only, per the brief. The per-site source change is the right
   repair but it needs a lane that can verify each instantiation's layout, and a map rename ahead of
   it strands rows at 0%.
2. **`?Handle@DxCubeTex@@` / `?Handle@NgFur@@` left out of group 478** — their bases are themselves
   fold members, so slot-6 co-location cannot distinguish a folded override from plain inheritance.
   *What would change this:* finding a separate retail address for either class's `Handle`, or
   source evidence that they override it.
3. **The `BinStream.h` `hash_map` ODR hazard left in place** (two divergent TU-local stream-operator
   copies, `SongMgr.cpp` vs `BandSongMetadata.cpp`). Resolving it changes which body wins for
   existing consumers — a different lane's change. Documented at the site.
4. **`OnParse`/`OnParseArray` retained in `CharLipSync.cpp`, now unreferenced.** Retail's `/OPT:REF`
   would have dropped them, but their *absence* is not provable from the image, and removing them
   buys nothing measurable (they are unpaired either way).
5. **The second `__RTDynamicCast` thunk at `0x82682688` left unhomed** — not identified, so not
   absorbed. *What would change this:* resolving its two RTTI descriptors
   (`0x82c72528` / `0x82c6e6b0`) to a class pair.
6. **AO's and AP's filed alias proposals not installed** (`0x8246af50`; `0x826c3888`, `0x82823340`,
   `0x826ccb48`). The brief admits them only if I verify each on retail bytes myself, and the budget
   went to items 1–4 which were all measured positive. `??0Splash` was not added anywhere, as AP
   recorded.
7. **Concurrency bars respected** — no file or address belonging to W16-AO (the ten `list<T>` map
   rows) or W16-AP (`GamePanel.cpp/.h`, its map rows, `Server.h`) was touched;
   `src/network/net/Server.h:26` `GetPlayerID` is untouched and still `int`.

## Gates

Run in the worktree, in the brief's order, native gate last.

| gate | result |
|---|---|
| full build | `rc=0` |
| `scripts/verify_ruler_agreement.py --check` | `rc=0` — both objdiff-cli entry points resolve the same ruler |
| `scripts/verify_objs_patched.py --verify-manifest` | `rc=0` — 1,215 decomp + 3,115 target objects match, `tree_sha256=8a30fe9c4604fa0f` |
| `tools/icf_alias_finder.py --validate` | `rc=0` — PASS, 1,398 map-consistent, 246 tolerated, **0 contradicted**, 1,645 groups |
| `tools/native_build_gate.sh` | see line below |

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```
