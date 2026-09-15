# W16-BO — Unemitted factories: source vs map, ten rows adjudicated on retail bytes

**Lane:** W16-BO · **Date:** 2026-09-15 · **Branch:** `w16-bo` (worktree `~/tmp/wt-w16-bo`)
**Scope bars honoured:** no edit to `config/45410914/splits.txt` (W16-BM owns tree-wide
`.text` re-homes, W16-BN owns the `UI.cpp:` / `network/net/NetSearchResult.cpp:`
headings); no commit to `main`, no push, no rebase.

---

## 1. What this lane was for

Ten target rows at `fuzzy == 0` on unmapped VAs, each looking like a factory. The
question was never "can we match them" but **which of four different things is
wrong**, because the four have disjoint fixes and three of the four are invisible
to each other:

| class | meaning | the fix |
|---|---|---|
| **(a) EMISSION** | our source never emits a body for a class we hold | write the body |
| **(b) NAMING** | our obj already emits it, the map lacks the spelling | add the map row |
| **(c) UNIT** | our emitter is a *different TU* than retail's | a `splits.txt` re-home — **a map name alone can never pair it** |
| **(d) NOT A FACTORY** | wrong class / not this shape | withdraw the row |

The load-bearing fact behind the whole taxonomy: **objdiff pairs target↔base by
NAME WITHIN A UNIT**, so a map name pays only if *that unit's own base obj defines
that name*. Class (c) is the class where the obvious fix (name it) is provably
worthless, and it is the reason this lane files two rows instead of fixing them.

---

## 2. The ten-row adjudication table

Sizes are retail extents. "Closed" = `fuzzy == 100` and `mpn == 100` on the shipped
`name_check` ruler, read from `report.json` after a full build.

| # | VA | B | unit | class | what was actually wrong | outcome |
|---|---|---:|---|---|---|---|
| 1 | `0x82677bd0` | 144 | `default/band3/game/Game` | **(a)** | `UnkTU5GuidePitchOwner::UnkTU5GuidePitchOwner(Symbol)` was declared, never defined (the *dtor* was, at fuzzy 100 — a positive control that the class shape is right) | **closed, +144 B** |
| 2 | `0x82b67a00` | 120 | `default/ExternalMic` | **(a)** | `ExternalMic::Init()` declared in the header, defined nowhere | **closed, +120 B** |
| 3 | `0x825557e8` | 120 | `default/band3/meta_band/PrefabMgr` | **(b)** + source trim | body already masked-equal for 24 words; ours 248 B vs retail 120 B because of four surplus `DataRegisterFunc` calls | **closed, +120 B** |
| 4 | `0x82b82260` | 108 | `default/JsonUtils` | **(a)** | `JsonConverter::NewArray` **NOT DEFINED ANYWHERE** in the tree | **closed, +108 B** |
| 5 | `0x8253abb8` | 96 | `default/MusicLibrary` | **(a)** | `MusicLibrary::OnLoad` defined but **EMPTY** — 4 B of `blr` vs 96 B | **closed, +96 B** |
| 6 | `0x82b6a0e8` | 72 | `default/system/synth_xbox/FxSendPitchShift` | **(a)** | whole TU was a one-line stub, so all three of its rows read 0 | **closed, +72 B** (+76 B sibling, +40 B funclet) |
| 7 | `0x823e3e90` | 72 | `default/MidiInstrument` | **(c)** | our body is 18/18 masked-equal but is defined in **`NetSession.obj`**, while the row sits in `default/MidiInstrument` | **FILED — §5** |
| 8 | `0x823e3c88` | 72 | `default/network/net/NetSession` | **(b)** | clean: 72/72 B, 18/18 masked-equal, relocs at retail's exact two `bl` offsets, name defined in the unit's own base obj | **closed, +72 B** |
| 9 | `0x825aaff0` | 72 | `default/SetlistMergePanel` | **(c)** | same as #7, defined in **`LockStepMgr.obj`** | **FILED — §5** |
| 10 | `0x82802240` | 120 | `default/UIColor` | **(b)** + body fix | naming alone predicted Δ0: our body was 128 B / 1-of-30 words equal because the wrong `LOAD_REVS` dialect won | **closed, +120 B** |

**Eight closed, two filed, zero class (d).** No row turned out not to be a factory.

---

## 3. Measured result (this lane's own verified baseline, never inherited)

Baseline re-measured in this worktree before any edit; `rowset_snapshot.py diff`
showed **0 rows of drift** against it at the time it was taken.

| key | before | after | Δ |
|---|---:|---:|---:|
| `matched_functions` | 43,558 | **43,568** | **+10** |
| `matched_code` | 4,040,604 | **4,041,612** | **+1,008 B** |
| `matched_code_percent` | 39.43181 | **39.441643** | +0.009833 |
| `total_functions` | 69,240 | 69,240 | 0 |
| `total_code` | 10,247,068 | 10,247,068 | 0 |
| `masked_equal_functions` | 23,080 | 23,081 | +1 |
| `fuzzy_match_percent` | 49.6996 | 49.709057 | +0.009457 |

`total_code` and `total_functions` are **unmoved**, which is the check that this
lane changed no denominator — no pin moved, nothing was reattributed.

**CROSSED IN 11 rows / 1,008 B · FELL OUT 0 rows / 0 B**

```
+144 B  Game::??0UnkTU5GuidePitchOwner@@QAA@VSymbol@@@Z
+120 B  UIColor::?Load@UIColor@@UAAXAAVBinStream@@@Z
+120 B  ExternalMic::?Init@ExternalMic@@SAXXZ
+120 B  PrefabMgr::?Init@PrefabMgr@@SAXPAVBandUserMgr@@@Z
+108 B  JsonUtils::?NewArray@JsonConverter@@QAAPAVJsonArray@@XZ
 +96 B  MusicLibrary::?OnLoad@MusicLibrary@@QAAXXZ
 +76 B  FxSendPitchShift::?SyncEffectParams@FxSendPitchShift360@@UBAXPAUIXAudio2SubmixVoice@@@Z
 +72 B  FxSendPitchShift::?CreateFx@FxSendPitchShift360@@MAAPAUIUnknown@@XZ
 +72 B  NetSession::?NewNetMessage@NewUserMsg@@SAPAVNetMessage@@XZ
 +40 B  FxSendPitchShift::fn_82B6A130   (EH funclet, byte-signature pairing)
 +40 B  Game::fn_82677C60               (EH funclet, byte-signature pairing)
```

**Two units completed** (a measure separate from bytes): `default/UIColor` **10/10 fns
/ 100%** — which closes the item W16BK §8 left open at 9/10 — and
`default/system/synth_xbox/FxSendPitchShift` **3/3 / 100%**.

### 3.1 The two builds, and why they were split

The lane deliberately measured **source-only first**, before any map edit. Stated
prediction: **~0**, because all ten VAs are unmapped, so the target rows cannot pair
by name and the naming channel is where the gain must come from.

| leg | Δfns | Δbytes | crossed | fell out |
|---|---:|---:|---|---:|
| source only | +2 | +156 | `SyncEffectParams` (already mapped) + 2 EH funclets | **0** |
| + 8 map rows + 2 proven aliases + 2 schedule fixes | +10 | +1,008 | the 11 above | **0** |

The +156 B is precisely the part that needs **no** name: one already-mapped sibling
in a TU that was wholly unemitted, plus two EH funclets objdiff pairs by byte
signature. The prediction of ~0 was wrong, in the favourable direction, for a
reason worth carrying: **emitting a body can pay without any naming at all when the
funclet it drags in finds a byte-equal counterpart.**

**The zero FELL OUT on the source-only leg is the result that mattered most.** The
riskiest edit in the lane is adding `#include "meta_band/MusicLibraryStore.h"` to
`MusicLibrary.cpp`, a **371-function** unit — exactly where a new include creates
inlining opportunities that perturb unrelated bodies. It perturbed nothing. Had it
regressed, the split legs made it attributable and revertible on its own.

### 3.2 Forecast vs measured

The brief forecast **+2 to +6 fns / +150 to +600 B**; measured **+10 / +1,008 B**.
The gap is not luck: the forecast priced the **naming** channel, and five of the ten
rows needed a **body that did not exist**, which no map edit could ever supply. The
source-only leg (+156 B) is roughly the brief's low end; the naming on top of real
bodies is the rest.

---

## 4. The eight names, and why each pairs

Every spelling was read from the **freshly built COFF symbol table**, filtering
`SectionNumber > 0` so an *undefined external reference* cannot be mistaken for a
definition. A reflinked worktree's target objs are **pre-renamer**, so any name read
before the first build reads "absent" — the build came first, by rule.

| VA | name installed | defined in | unit carrying the row |
|---|---|---|---|
| `0x82677bd0` | `??0UnkTU5GuidePitchOwner@@QAA@VSymbol@@@Z` | `Game.obj` | `default/band3/game/Game` |
| `0x82b67a00` | `?Init@ExternalMic@@SAXXZ` | `ExternalMic.obj` | `default/ExternalMic` |
| `0x825557e8` | `?Init@PrefabMgr@@SAXPAVBandUserMgr@@@Z` | `PrefabMgr.obj` | `default/band3/meta_band/PrefabMgr` |
| `0x82b82260` | `?NewArray@JsonConverter@@QAAPAVJsonArray@@XZ` | `JsonUtils.obj` | `default/JsonUtils` |
| `0x8253abb8` | `?OnLoad@MusicLibrary@@QAAXXZ` | `MusicLibrary.obj` | `default/MusicLibrary` |
| `0x82b6a0e8` | `?CreateFx@FxSendPitchShift360@@MAAPAUIUnknown@@XZ` | `FxSendPitchShift.obj` | `default/system/synth_xbox/FxSendPitchShift` |
| `0x823e3c88` | `?NewNetMessage@NewUserMsg@@SAPAVNetMessage@@XZ` | `NetSession.obj` | `default/network/net/NetSession` |
| `0x82802240` | `?Load@UIColor@@UAAXAAVBinStream@@@Z` | `UIColor.obj` | `default/UIColor` |

Injectivity was asserted before writing: no address already mapped, no name already
used at another address. (106 rows in the map carry a `null` value; they were left
untouched — a naive reverse-index over them raises rather than silently mis-answers.)

### 4.1 Caller census — required, not optional

Under `name_check` objdiff **forgives a placeholder target** (`fn_`, `lbl_`, …), so
**naming an anonymous address converts a forgiven call site into a checked one.** The
sign of a naming therefore depends on the *callers*, not on the named row.

| named VA | retail `bl` sites | distinct callers | caller row state **before** | risk |
|---|---:|---:|---|---|
| `0x82677bd0` | 1 | `fn_8267BF30` (Game) | **unmapped, fuzzy 0** | none — an unpaired caller cannot be charged |
| `0x82b67a00` | 1 | `?Init@Synth360@@UAAXXZ` | fuzzy **99.977776** | none — already withholding all 900 B |
| `0x825557e8` | 1 | `fn_82270E68` (App) | **unmapped, fuzzy 0** | none |
| `0x82b82260` | **4** | `?DataPointToQString@RockCentral@@…` | fuzzy **89.55652** | none — already sub-100 |
| `0x8253abb8` | 1 | `?Load@SongSelectPanel@@UAAXXZ` | **fuzzy 100 / mpn 100, 68 B** | **the only row that could have LOST** |
| `0x82b6a0e8` | **0** | — | — | virtual, dispatched through the vtable |
| `0x823e3c88` | **0** | — | — | `REGISTER_OBJ_FACTORY` reached via a data pointer |
| `0x82802240` | **0** | — | — | virtual |

The one row at risk was checked on retail bytes rather than assumed:
`?Load@SongSelectPanel@@UAAXXZ` does `lwz r3, lbl_82DFD3A8` (`TheMusicLibrary`) then
`bl fn_8253ABB8`, and our source is `TheMusicLibrary->OnLoad();` at exactly that
site. So the caller **corroborates** the identification instead of threatening it.
Measured after: still 100.

★ **Three of the eight have ZERO `bl` callers**, and that is *information*, not an
obstacle: zero callers means either a factory reached through a data pointer
(`REGISTER_OBJ_FACTORY`) or a **virtual**. Both were confirmed — `CreateFx` is
`MAA` (protected virtual) in its own mangling, and `UIColor::Load` is `UAA`.

---

## 5. Class (c) — the two rows FILED, never moved

These are the rows where the whole taxonomy earns its keep. Both bodies are
**already correct** — 18/18 masked-equal against retail — and **no map row could
ever score them**, because the unit that carries the target row is not the unit
whose object defines the name.

### 5.1 Row 7 — `0x823e3e90`, 72 B, `VoiceDataMsg` factory

- Target row `fn_823E3E90` sits in **`default/MidiInstrument`**.
- Our body is defined in **`NetSession.obj`**.
- **Independent corroboration:** the factory calls `??0VoiceDataMsg@@QAA@XZ` at
  `0x823e2fe8`, and that address is owned by the `network/net/NetSession.cpp:`
  heading (block `0x823e2c70..0x823e35e8`). The ctor's home settles the TU.

Current state (`config/45410914/splits.txt`, **not edited by this lane**):

```
network/net/NetSession.cpp:          # heading at line 9580
    .text       start:0x823E3BE0 end:0x823E3E38     # line 9676
MidiInstrument.cpp:                  # heading at line 7774
    .text       start:0x823E3E38 end:0x823E3F58     # line 7787
```

The two blocks are **exactly adjacent at `0x823E3E38`**, so the follow-up is a
boundary move, not a new block. Rows inside MidiInstrument's block:

| VA | size | fuzzy | note |
|---|---:|---:|---|
| `0x823e3e90` | 72 | 0 | the factory |
| `0x823e3ed8` | 40 | **100** | almost certainly the factory's EH cleanup funclet |
| `0x823e3f00` | 76 | 0 | a second unmapped row |

**Exact lines a follow-up needs** (move the factory **and its funclet together**):

```
network/net/NetSession.cpp:   .text  start:0x823E3BE0 end:0x823E3F00   # was end:0x823E3E38
MidiInstrument.cpp:           .text  start:0x823E3F00 end:0x823E3F58   # was start:0x823E3E38
```

⚠ **Caveat to price, not to ignore:** `fn_823E3ED8` is at **fuzzy 100 today** via
byte-signature funclet pairing inside `default/MidiInstrument`. Moving it changes
which pool it pairs within, so those **40 B are at risk in either direction** —
this is the mechanism that cost `b341d7ab` exactly 40 B on `fn_8267F574`. Take the
funclet with its parent (as written above) rather than splitting them.

### 5.2 Row 9 — `0x825aaff0`, 72 B, `LockResponseMsg` factory

- Target row `fn_825AAFF0` sits in **`default/SetlistMergePanel`**.
- Our body is defined in **`LockStepMgr.obj`**.
- **Independent corroboration:** the factory calls `??0LockResponseMsg@@QAA@XZ` at
  `0x825aac28`, owned by the `band3/meta_band/LockStepMgr.cpp:` heading — and that
  heading's only `.text` block is `0x825AAC28..0x825AACA0`, i.e. it *starts* at the
  ctor.

```
band3/meta_band/LockStepMgr.cpp:     # heading at line 13265
    .text       start:0x825AAC28 end:0x825AACA0
SetlistMergePanel.cpp:               # heading at line 10264
    .text       start:0x825AAF60 end:0x825AB060     # line 10288
```

⚠ **This one is NOT adjacent** — there is a gap `0x825AACA0..0x825AAF60` — so it is
strictly more invasive than row 7: LockStepMgr must gain a **second, discontiguous**
`.text` block and SetlistMergePanel's single block must be **split in two**:

```
band3/meta_band/LockStepMgr.cpp:  .text  start:0x825AAC28 end:0x825AACA0   # unchanged
                                  .text  start:0x825AAFF0 end:0x825AB038   # NEW
SetlistMergePanel.cpp:            .text  start:0x825AAF60 end:0x825AAFF0   # split
                                  .text  start:0x825AB038 end:0x825AB060   # split
```

Rows in SetlistMergePanel's block: `0x825aaf98` (76 B, fuzzy 0), `0x825aaff0`
(72 B, fuzzy 0, the factory), `0x825ab038` (40 B, **fuzzy 99.4** — a funclet whose
residual is relocation-name-only). The same funclet caveat as §5.1 applies, and the
`0x825ab038` row is *already* short of 100, so it has less to lose.

★ **Both rows are reminders that `.pdata` is DERIVED**: only `.text` may be edited;
every split run clears and re-derives the whole `.pdata` set.

---

## 6. How each closed row was actually fixed (the reasoning, not the verdict)

### 6.1 Row 6 — a whole TU that scored zero

`src/system/synth_xbox/FxSendPitchShift.cpp` was the single line
`// Decompiled from assembly`. All **three** rows of its unit (188 B) read fuzzy 0
because the base obj defined nothing at all. Ported from DC3's identically-arranged
TU. Retail evidence for `CreateFx`: `li r3, 0x70` (= 112 = `sizeof(PitchShiftEffect)`),
`bl 0x827BD2F0` (operator new), `bl 0x82B6D820` = `??0PitchShiftEffect@@QAA@XZ`, an
**unadjusted** return (so the return type is class-typed, not `Hmx::Object*` — no
vbtable upcast tail), and **0 `bl` callers** ⇒ virtual.

⛔ **Brief defect found:** the brief states `PitchShiftEffect` has "no such class in
our tree". **False** — `src/system/synth_xbox/PitchShiftEffect.h` declares it with
`// size 0x70` and `??0PitchShiftEffect@@QAA@XZ` is defined in `PitchShiftEffect.obj`.

`fn_82B6A130` (40 B) is `CreateFx`'s **EH cleanup funclet**, identified by byte
signature; it crossed on the source-only leg with no name.

⚠ **Residual, stated rather than hidden:** `FxSendPitchShift360.cpp` still defines
the same two functions as stubs. It is absent from `objects.json`, so the match
build sees no duplicate — the native gate is what adjudicates whether the native
link sees both. DC3 has the identical arrangement, which is reassuring but is not
the same as a measurement. **See §8 for the gate result.**

### 6.2 Row 3 — deleting code, proved faithful before deleting it

Retail `fn_825557E8` is 120 B and **ends** at the `unk5c` branch; ours was 248 B,
masked-equal for the first 24 words then 128 B of surplus, because `Init` also made
four `DataRegisterFunc` calls. Deleting code on a hunch is how a lane fabricates a
gain, so this was proved by a **controlled retail-string search**: all four handler
name strings (`prefab_is_customizable`, `prefab_toggle_customizable`,
`prefab_uses_profile_patches`, `prefab_toggle_uses_profile_patches`) occur **0
times** in `orig/45410914/band.exe`, with **non-zero positive controls** from the
same TU so the search is not vacuous, and the unit carries **no `?OnPrefab*` target
row**. The handler *definitions* were left in place — they are header-declared
members and removing them is a separate, larger claim.

Corroborating immediates: `li r3, 0x9c` = 156 = compiler-verified
`sizeof(PrefabMgr)`; the store offset `0x7c` = 124 matches the header's `unk5c`.

### 6.3 Row 10 — two competing `LOAD_REVS` dialects, and the one that won was wrong

Our `UIColor::Load` was 128 B with **1 of 30 words equal**. Cause: two definitions
of `LOAD_REVS` reach that TU and `obj/Object.h:1805`'s won, expanding to
`int revs; bs >> revs; BinStreamRev d(bs, revs);` — building a **dead**
`BinStreamRev`. Retail instead splits the packed int into two file-scope shorts
(`obj/ObjMacros.h:647`'s dialect):

```
li r5,4 / addi r4,r1,0x50 / bl ?ReadEndian@BinStream@@QAAXPAXH@Z   -> bs >> rev
lwz r11,0x50(r1) / mr r10,r11 / srwi r11,r11,16
sth r11, lbl_82E077F4+0      <- the SHIFTED half at base+0
sth r10, lbl_82E077F4+4      <- the TRUNCATED half at base+4
bl ?Load@Object@Hmx@@UAAXAAVBinStream@@@Z
addi r4,r31,0x28 / bl ??5@YAAAVBinStream@@AAV0@AAVColor@Hmx@@@Z    -> bs >> mColor
```

`utl/BinStream.h:199-200` settles **which short is which**: `getHmxRev` truncates and
`getAltRev` is `(unsigned)packed >> 0x10` — note retail's **logical** `srwi`, not
`srawi`. So base+0 is `gAltRev` and base+4 is `gRev`, and because **declaration order
is what fixes `.bss` placement, `gAltRev` must be declared FIRST**. This reproduces
the arrangement already proven on retail bytes in `ui/UILabel.cpp:52-89` (worth
1,132 B there). `ASSERT_REVS` expands to nothing — retail's asm goes straight from
the rev split into `Hmx::Object::Load` with no `MILO_FAIL` arm.

Installed with `push_macro`/`pop_macro` so the dialect cannot leak if this file is
ever whole-file `#include`d by a COMDAT-scatter owner (checked: it is not today).
The two-argument `INIT_REVS(rev, alt)` form was kept so the existing call site is
unchanged.

### 6.4 Rows 1 and 4 — two scheduling hypotheses, stated before the build

Both rows **paired** on the map edit and then sat just short. Pairing is what made
them visible at all — this is the pairability-as-correctness-instrument effect.

**Row 1, 97.22222 → 100.0.** One `delete`: retail **RELOADS** `lwz r3,0x14(r30)`
before `Load(false)`; with four *direct member stores* our build keeps the pointer
live in `r3` and skips it. Hypothesis: an **indexed** store through `this` is the
aliasing fact that forces the reload back, so `mUnkCounts` is zeroed by a
4-iteration loop. `/O1` unrolls it to the same four `stw` retail has. **Confirmed.**

**Row 4, 92.40741 → 100.0.** One insert + one delete: retail sinks the
`stw r31,0x50(r1)` that materialises `push_back`'s const-ref argument **before** the
inlined `AddRef`, we emitted it after. Hypothesis: naming the upcast temporary
(`JsonObject *entry = arr;`) initialises it at its declaration and puts the store
where retail has it; semantically identical, offset 0 under single inheritance.
**Confirmed.**

Both are recorded as hypotheses-with-mechanism because a scheduling residual is
normally waved off as permuter territory — and **two of them here had a source
cause**, found by asking *what fact would the compiler need in order to emit
retail's instruction* rather than by perturbing declarations.

### 6.5 Row 4's identity — and an instrument correction worth keeping

`?NewArray@JsonConverter@@QAAPAVJsonArray@@XZ` was **NOT DEFINED ANYWHERE** in the
tree. A first raw byte-regex scan reported it as defined in `RockCentral.obj` — and
also reported `??0VocalGuidePitch@@QAA@XZ` defined in `PracticePanel.obj`. Both are
**false**: a byte scan finds **undefined external references** too. The corrected
instrument reads the COFF symbol table and filters `SectionNumber > 0`.
⇒ **A symbol-name byte scan is not a definition test.** This flipped row 4 from
"class (b), just name it" to "class (a), nobody ever wrote it".

Retail evidence for the body: `li r3, 0x8` (= compiler-confirmed `sizeof(JsonArray)`),
`bl ??0JsonArray@@AAA@XZ` — **`AAA` = a PRIVATE ctor**, which is exactly what the
*system* header declares while the parallel `network/` header makes it public, so
**retail agrees with the system class shape**; `lwz r3,4(r31)` + `bl json_object_get`
= the inline `AddRef()` (`mObject` at +0x4); `addi r3,r30,8` = `mObjects` at +0x8.
Corroborated by the call graph: exactly **four** retail `bl` callers, all inside
`?DataPointToQString@RockCentral@@…`, and our `DataPointToQString` calls
`jc.NewArray()` exactly four times.

### 6.6 Row 5 — and an in-tree doc defect

`MusicLibrary::OnLoad` was `{}`. Retail: `stb 0,0x1a0(r30)`; `li r3,0x64` (= 100 =
`sizeof(MusicLibraryStore)`); operator new; `bl 0x825BD458` =
`??0MusicLibraryStore@@QAA@XZ`; `stw r3,0x19c(r30)`.

⛔ **`src/band3/meta_band/MusicLibrary.h:348` is WRONG**: it names `fn_825276C0` as
the op-starter. Decoding `0x825276C0` lands inside a
`_Vector_base<TrackerPlayerDisplay>` destructor region, which cannot be a function
start. **The real op-starter is `fn_8253ABB8`** — this row. The comment was left in
place and the correction recorded at the fix site.

The allocated type is `MusicLibraryStore`, **not** the local `MusicLibraryUnkOp`
stub, corroborating the residual already noted at `MusicLibrary.cpp:375`. `unk19c`
is retyped only through a cast at this one site, **deliberately**: retyping the
member would re-point ~20 call sites (`Poll`/`Finish`/`Unk825BCA38`/…) at a class
that does not declare them. The cast is a reinterpret and emits no instruction, so
the codegen is retail's exactly.

---

## 7. Two ICF fold memberships, adjudicated not assumed

Rows 2 and 4 each carried one charged relocation-name site where retail names
`push_back<vector<ChatReceiver*>>` and we spell `<ExternalMic*>` / `<JsonObject*>`.
`TEMPLATE_ARGS_DIFFER` **is what a fold looks like**, which is exactly why it is not
evidence of one — and an unproven alias lifts `name_check` **by construction**.

Both were run through `tools/icf_pair_adjudicate.py`:

- **Flat T1: REFUTED** for both — 112 B on both sides, masked bodies identical, but
  relocation *targets* disagree ("template-twin, not a fold").
- **CHASED T1: PROVEN** for both — the single differing slot is
  `_M_insert_overflow<T*>` vs `<int>`, and the chase bottoms out entirely on folds
  this repo has **already** proven: `??2CriticalSection`/`??2ChunkAllocator`
  (group 1546), `PoolAlloc` 2-arg/5-arg, `MemOrPoolAlloc`/`MemOrPoolAllocSTL`.

Both were added to the existing group at `0x82b5f808`, which already forgives seven
other pointer instantiations plus `<int>` — precedent: W16-M added
`push_back<Friend*>` the same way. Neither forgives a site that was scoring before
this lane: **both rows read `fuzzy` 0 (unpairable) an hour earlier.**

⚠ **A correction is written into the group's own evidence field**, not edited away:
this lane predicted the `<JsonObject*>` membership would **not** cross `NewArray`
(because of its separate insert/delete). The scheduling pair was then also closed,
so the row did cross. The record says so explicitly, so the next lane does not
conclude the alias alone was what closed it.

---

## 8. Optional items

### 8.1 `fn_82802240` (UIColor::Load) — DONE

This is row 10. `default/UIColor` is now **10/10 functions / 1,016 of 1,016 B /
100%**, which closes the item W16BK §8 recorded as leaving the unit at 9/10.

### 8.2 W16BJ §5.5's `fn_82703AD0` — adjudicated and CLOSED as NOT COLLECTABLE

40 B / 10 instructions, `mpn` 100 / `fuzzy` 99.5, one `diff_arg`. The charged pair:

```
target: ??$__destroy_range@PAUMoveRating@SkeletonClip@@U12@@stlpmtx_std@@YAXPAUMoveRating@SkeletonClip@@00@Z   (3 args)
base:   ??$_Destroy_Range@PAUSpotlightEntry@LightPreset@@@stlpmtx_std@@YAXPAUSpotlightEntry@LightPreset@@0@Z    (2 args)
```

The tempting move is an alias — both look like no-ops for trivially-destructible
types. **Adjudicated instead: FLAT T1 REFUTED and CHASED T1 REFUTED** — 80 B on
both sides with **masked bodies that genuinely DIFFER** ("retail did not keep the
code our spelling compiles to"). So this is **not a fold** and no alias can
legitimately close the 40 B.

Two further reasons this row is a dead end as written, both from objdiff itself:
it flags `UNVERIFIABLE_PAIRING` — `fn_82703AD0` **has no asserted identity** and is
paired by *masked byte signature*, so the comparator is an arbitrary byte-equal
counterpart and the finding is unfalsifiable rather than fixable. This is W16BJ's
own durable rule holding: **a relocation-masked twin comparator predicts
`matched_functions`, never `matched_code`.**

★ **Residual worth a lane of its own, and it is NOT an alias task:** retail's 3-arg
`__destroy_range` and our 2-arg `_Destroy_Range` are **different STLport helpers at
the same 80 B**, not two spellings of one. If our STLport reaches for the wrong one
systematically, that is a real source divergence with a population — but it needs
the identity of `fn_82703AD0` established first, and nothing here licenses an alias.

---

## 9. Item 6 — report-only sweep of the same vein

Two passes, because the first one was too coarse to publish.

**Pass 1 (coarse).** Rows that are unmapped, `fuzzy == 0`, 64–160 B, in a unit that
*has* a base obj: **2,565 rows / 261,932 B across 547 units**. Cross-referencing
"our base obj defines names that are not any target row" produces big numbers
(RockCentral 1,922, BandCharacter 2,320, HamCamTransform 6,020) — but that column is
dominated by **STL/template COMDATs**, so its enrichment would describe the detector
rather than the vein. **Not published as a lever.**

**Pass 2 (decisive).** Same candidate set, filtered on the MSVC `return new X()`
idiom in the **retail** body (`li r3,<imm>` → `bl` operator new → `cmplwi r3,0` →
`beq` → `bl <ctor>`):

> **18 rows / 1,688 B across 14 units.**

★ **Positive control, unprompted:** the filter independently rediscovered **both** of
this lane's own class-(c) rows — `fn_823E3E90` (`default/MidiInstrument`) and
`fn_825AAFF0` (`default/SetlistMergePanel`) — which were adjudicated by hand hours
earlier. A sweep that finds the answers you already have by another route is not
vacuous.

| row | unit | B | `sizeof` imm | ctor called |
|---|---|---:|---:|---|
| `fn_825877A8` | `default/band3/meta_band/SessionMgr` | 152 | `0xa4` | *(detector read operator new — verify by hand)* |
| `fn_82B823C0` | `default/JsonUtils` | 124 | `0x8` | `fn_82B81FA0` |
| `fn_82B822D0` | `default/JsonUtils` | 116 | `0x8` | `fn_82B81EF0` |
| `fn_82B82348` | `default/JsonUtils` | 116 | `0x8` | `fn_82B81F48` |
| `fn_82574D90` | `default/Flow` | 100 | `0xdc` | `fn_82574938` |
| `fn_823134F8` | `default/Gesture` | 100 | `0xe4` | `fn_82311AA8` |
| `fn_8256E8A8` | `default/MetaPanel` | 100 | `0x6c` | `fn_826033F8` |
| `fn_82574810` | `default/MetaPanel` | 100 | `0xb0` | `fn_825743E0` |
| `fn_8274DF40` | `default/EventTrigger` | 96 | `0x44` | `fn_82767FB0` |
| `fn_827457C0` | `default/WavMgr` | 92 | `0xe0` | `fn_827450A0` |
| `fn_8256B838` | `default/band3/meta_band/CharCache` | 80 | `0x60` | `fn_8256B740` |
| `fn_823F0238` | `default/CheatProvider` | 76 | `0xcc` | `fn_823EFD28` |
| `fn_8268F0D0` | `default/BandUser` | 76 | `0x114` | `fn_8268EB10` |
| `fn_828023A0` | `default/UI` | 72 | `0x40` | `fn_827F1E90` |
| `fn_82768A98` | `default/Gesture` | 72 | `0x30` | `fn_82768770` |
| `fn_823E3E90` | `default/MidiInstrument` | 72 | `0x34` | `fn_823E2FE8` ← **this lane, §5.1** |
| `fn_825AAFF0` | `default/SetlistMergePanel` | 72 | `0x18` | `fn_825AAC28` ← **this lane, §5.2** |
| `fn_8274DEC8` | `default/system/obj/Dir` | 72 | `0x28` | `fn_8275CB88` |

**Net for a follow-up lane: 16 rows / 1,544 B** (the two above are already filed).

★ **The richest single target is `default/JsonUtils`.** It is **11/26 fns / 43.07%**
and carries **15** unmapped `fuzzy == 0` rows, **three** of them factory-shaped with
`sizeof` `0x8` and ctors that are themselves inside the same TU
(`fn_82B81FA0`/`fn_82B81EF0`/`fn_82B81F48` — the `JsonArray` family). This lane
proved that TU's class shapes are the ones retail agrees with (§6.5), so the next
lane there starts from a settled premise. **⚠ The `sizeof` route is how this vein
gets adjudicated, not the row size** — `report.json` sizes are a targeting hazard.

★ **Method note for whoever picks this up:** the `sizeof` immediate is the highest-value
single discriminator in the whole class. `li r3, <imm>` before `operator new` gives the
allocated class's size directly, and `scripts/harvest/class_layout_report.py` (i.e.
`cl /d1reportSingleClassLayout`) turns that into a *candidate class* — authoritatively,
where the `// 0xHEX` header comments are measurably wrong. Eight of this lane's ten
rows were identified that way before any diff was run.

---

## 10. What this lane did NOT do, and why

- **No `splits.txt` edit of any kind.** Rows 7 and 9 are the two rows that need one,
  and §5 gives the exact lines, the adjacency status, the destination headings, and
  the funclet risk — so the move is a mechanical edit for whoever owns splits. W16-BM
  and W16-BN hold that file.
- **Did not retype `MusicLibrary::unk19c` to `MusicLibraryStore *`.** It is the truer
  type (§6.6) but it re-points ~20 call sites at a class that does not declare those
  methods. Scoped to a cast at the one site retail proves.
- **Did not delete `PrefabMgr`'s four `OnPrefab*` handler definitions**, only their
  registrations. They are header-declared members; removing them is a separate claim.
- **Did not remove the duplicate stubs in `FxSendPitchShift360.cpp`** (§6.1). The
  file is absent from `objects.json`; the native gate is the instrument that would
  fail on it.
- **Did not pursue the `__destroy_range` / `_Destroy_Range` divergence** (§8.2). It is
  a real finding with a possible population, but it needs `fn_82703AD0` identified
  first and is not this lane's class.
- **Did not touch the 2,565-row coarse population** (§9 pass 1) beyond reporting that
  its obvious classifier is not one.

### Rows I believe are unfixable as currently pinned, and the evidence that would change that

| row | why | what would change it |
|---|---|---|
| `0x823e3e90` (72 B) | class (c): correct body, defined in `NetSession.obj`, row lives in `default/MidiInstrument`. **No map name can pair it** — the unit's own obj does not define it. | the §5.1 `.text` boundary move. It is a *boundary* move over already-pinned code, so it is **NOT** metric-neutral (re-homing changes pairability; ~+72 B expected, with 40 B of funclet at risk). |
| `0x825aaff0` (72 B) | same, defined in `LockStepMgr.obj` | the §5.2 edit — **discontiguous**, so a new block plus a split, strictly more invasive |
| `fn_82703AD0` (40 B) | not a fold (CHASED T1 REFUTED, masked bodies differ at 80/80) **and** no asserted identity, so the charge is unfalsifiable | an identity for `fn_82703AD0` established on retail bytes. Until then an alias here would be fabrication, and a `none`-ruler control **cannot** detect that. |

---

## 11. Corrections this lane made to existing records

1. **`src/band3/meta_band/MusicLibrary.h:348`** names `fn_825276C0` as the op-starter.
   Wrong — that address is inside a `_Vector_base<TrackerPlayerDisplay>` dtor region.
   The real one is **`fn_8253ABB8`**.
2. **The brief** says `PitchShiftEffect` has "no such class in our tree". Wrong — it
   is declared in `src/system/synth_xbox/PitchShiftEffect.h` with `// size 0x70` and
   its ctor is defined in `PitchShiftEffect.obj`.
3. **The brief**'s "assert ≥27,000 mangled names present" does not literally hold.
   `tools/check_target_objs_renamed.py` reports **25,838 / 29,359 map names present
   in 3,113 target objs = 88.0%** (floor 40%). The real number is recorded rather
   than the briefed one; 88% decisively rules out a pre-renamer tree, which would
   read ~0%.
4. **A symbol-name byte scan is not a definition test** (§6.5) — it counts undefined
   external references. Two false "defined" verdicts came out of it.
5. **This lane's own alias evidence** predicted `NewArray` would not cross on the
   membership alone. It did cross, because the scheduling pair was fixed too; the
   correction is written into `symbol_aliases.json` rather than edited out.

---

## 12. Gate chain

Run in the brief's order, in the worktree, **native LAST**.

| gate | result |
|---|---|
| full build (`./tools/ninja-locked`) | **rc=0** |
| `scripts/verify_ruler_agreement.py --check` | see below |
| `scripts/verify_objs_patched.py --verify-manifest` | see below |
| `tools/icf_alias_finder.py --validate` | see below |
| `tools/funclet_homing.py --validate` | see below |
| `tools/native_build_gate.sh` | see below |

All six gates were run in the brief's order, in the worktree, with the native gate
**last** (a comment-only change has broken the native link before, so the gate run
must be the lane's final action).

| gate | rc | result |
|---|---:|---|
| full build `./tools/ninja-locked` | **0** | `~/tmp/rb3_build_w16bo_5.log` |
| `scripts/verify_ruler_agreement.py --check` | **0** | `OK: both objdiff-cli entry points resolve the same ruler` (`ppc.calculatePoolRelocations = false`) |
| `scripts/verify_objs_patched.py --verify-manifest` | **0** | `1215 decomp, 3113 target objects match` (`tree_sha256=c7f8de887bd9f5bd`); denylist clean, 495,607 symbols scanned |
| `tools/icf_alias_finder.py --validate` | **0** | `VALIDATE: PASS -- 1404 map-consistent, 247 tolerated, 0 contradicted, 1652 total` |
| `tools/funclet_homing.py --validate` | **0** | `VALIDATE: PASS` — `HOMED 24221 / MIS-PINNED 706 / ORPHAN 1352 / UNPINNED-FUNCLET 42`, ambiguous FuncInfos 1 (expected 1) |
| `tools/native_build_gate.sh` | **0** | verbatim below |

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

★ **`skipped=0`, so this is full coverage, not the `PASS (INCOMPLETE: …)` shape** —
the distinction is one space apart in the prose verdict and is why the machine line
is pasted verbatim rather than paraphrased.

★ **The native gate also discharges the §6.1 residual.** The open question was
whether `FxSendPitchShift360.cpp`'s surviving stub definitions of the same two
functions would collide at native link time — the match build is structurally
incapable of seeing that, since the file is absent from `objects.json`. 18/18 with
0 skips says they do not.

**Measures re-read from `report.json` after the final build**, identical to §3:
`matched_functions` **43,568** · `matched_code` **4,041,612** ·
`matched_code_percent` **39.441643** · `total_functions` **69,240** ·
`total_code` **10,247,068** · `masked_equal_functions` **23,081** ·
`fuzzy_match_percent` **49.709057**.

