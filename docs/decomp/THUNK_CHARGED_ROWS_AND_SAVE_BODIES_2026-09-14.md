# W16-N — the 9 charged thunk rows, the two Save bodies, two proven namings

Lane W16-N, 2026-09-14, branch `w16-n` off main `f6102042`.
Input: W16-K's `docs/decomp/THUNK_FALSE100_CENSUS_2026-09-14.md` (§8 worklist, §9 Save blocker).

## 0. Whole-binary, pre → post (all from `build/45410914/report.json`, every build rc=0)

| measure | baseline `f6102042` | final | Δ |
|---|---:|---:|---:|
| `matched_functions` | 43,205 | **43,210** | **+5** |
| `matched_code` (B)  | 3,936,700 | **3,937,416** | **+716** |
| `total_code` (B)    | 10,245,956 | 10,245,956 | 0 |

Every step was priced by set-diff of the `fuzzy==100` row set
(`tools/rowset_snapshot.py`), never by subtracting absolutes.

## 1. The "9 charged thunk rows" — per-row verdict

⚠ **The briefed framing is wrong in two ways, and testing it literally is what
found that.** (a) `ClassName@ReviewDisplay` is **not** charged — it reads
`fuzzy 100.0`. (b) Only **4 of the 9** are in the audit's INCONSISTENT bucket;
the other 5 are IRREDUCIBLE fold-hub rows. So the true ceiling for item 1 is
**+8 functions / +112 B over 8 rows**, not 9 rows.

| # | thunk row | named destination | verdict | action |
|---|---|---|---|---|
| A | `PreLoad@UIComponent` | `Highlight` | **THUNK ROW WRONG** | renamed `0x82319c20` → `?Highlight@UIComponent@@$4PPPPPPPM@3AAXXZ` (`98cbc7ed`) |
| B | `ClassName@ReviewDisplay` | a `ForceEmit_*` | **METRIC-FITTING — not actioned** | see §1.1 |
| C | `IsLocal@LocalUser` | `IsDirPtr@ObjDirPtr` | **IRREDUCIBLE** (ICF fold hub) | none |
| D | `GetRemoteUser@RemoteUser` | `GetLocalUser` | **IRREDUCIBLE** | none |
| E | `CanSaveData@NullLocalBandUser` | `GetCrowdMeter` | **IRREDUCIBLE** | none |
| F | `UserName@NullLocalBandUser` | `ContentPattern` | **IRREDUCIBLE** | none |
| G | `SyncProperty@BandUser` | `GetCrowdMeter` | **IRREDUCIBLE**, destination proven wrong two ways | none |
| H | `Load@RndCam` | `Replace@RndParticleSys` | **THREE-ROW ROTATION** | CubeTex 3-cycle repaired (`2482b190`) |
| I | `SetTypeDef@UIPanel` | `Highlight@RndDir` | **THUNK ROW WRONG** | renamed `0x82808f88`, `0x82808fd8` (`98cbc7ed`) |

**Measured:** `2482b190` (the H rotation) = **+1 fn / +16 B, predicted exactly**.
`98cbc7ed` (A and I, three renames) = **Δ0 bytes exactly**, fuzzy −0.000307 pp —
pre-registered as accuracy-only and it landed accuracy-only.

Evidence for C–G being irreducible: our COMDATs are byte-identical to retail and
relocation-free; each *destination* name pairs at 100% with a same-named COMDAT
in its own unit, so the destination names are right; the thunk names are
corroborated by their vtable slots; and the four thunk-side base spellings have
**no retail address at all**, so a rename would make the row permanently 0%
(CLAUDE.md map/name economics). Row G is additionally proven wrong two ways —
the mangled displacement token spells −220 where the bytes adjust by 4, and the
named method's body is 200 B.

### 1.1 The `ForceEmit_*` destination — adjudicated explicitly

`ClassName@ReviewDisplay` branches to `ForceEmit_ReviewDisplay_StaticClassName`,
which is **our own scaffolding** (`src/system/bandobj/StarDisplay.cpp:281`) and
the map's only `ForceEmit` entry. 60 B currently read as matched (a 48 B row in
`default/StarDisplay` plus the 12 B thunk) **rest on a name retail cannot
contain.** That is exactly the metric-fitting class CLAUDE.md warns about.

The correct name is `?ClassName@ReviewDisplay@@UBA?AVSymbol@@XZ`. It is **not
repaired here** because repairing it *unpairs both rows* (−60 B) and also needs
a pin move into `ReviewDisplay.cpp` — i.e. it is a splits lane, not a naming
one. Recorded so it is not re-counted as a win by a later census.

### 1.2 A refuted hypothesis, kept because it changed the outcome

I predicted `/OPT:ICF` folds all identical relocation-free trivial bodies, which
would have licensed 4 aliases. **Refuted by measurement:** retail has 3 distinct
named addresses with `li r3,1; blr` and 5 with `li r3,0; blr`. This stopped 4
unproven aliases being installed. Separately I predicted all fold-hub
destinations were trivial bodies — wrong for 2 of 4 (`0x822ad870`, `0x82279708`
are 8-byte adjustors forwarding to `RndDrawable::Highlight`), which is why
`.pdata` extent returns None for them: an 8-byte leaf stub has no unwind record.

## 2. The two Save bodies — W16-K's blocker is REFUTED

W16-K recorded the blocker as "`bs << ObjList<T>` does not exist in our tree"
and proposed porting an operator into `obj/Object.h`, a **PCH input with a
~281-TU cascade**. That work is unnecessary, and the oracle never had such an
operator either (`../rb3/src/system/obj/ObjList.h` and
`../dc3-decomp/src/system/obj/Object.h` both carry only `operator>>`).

**Retail settles it directly.** `fn_824C93C0` is already map-named
`??$?6UTarget@HamCamShot@@…ABV?$list@UTarget@HamCamShot@@…@Z` — retail writes
`ObjList<T>` through the *generic*
`operator<<(BinStream&, const stlpmtx_std::list<T,Alloc>&)`, which we already
have at `src/system/utl/BinStream.h:367`. `ObjList<T>` derives from
`std::list<T>`, so derived-to-base deduction picks it up unchanged. Likewise
`ObjPtr<T>` derives from `ObjRefConcrete<T>`, whose `operator<<` is already at
`src/system/obj/ObjPtr_p.h:193`. **No header edit, no PCH cascade.**

The asymmetry W16-K saw is real but one-sided: `operator>>` needs an
ObjList-specific overload because `ObjList::resize` passes `T(mOwner)`; the
write side never needs the owner.

### 2.1 The decoding key: `r31` is the virtual-base pointer

Both bodies address every member through a **negative** displacement, which
looks impossible until you notice `r31` is not `this` — it is the address of the
`Hmx::Object` **virtual base subobject**. Confirmed by the compiler layout, not
guessed:

* `EventAnim`: vbase at `+0x40`, `sizeof` `0x68` ⇒ `r31 = this+0x40`, and
  `-0x18/-0x30/-0xc/-0x24` resolve to `mKeys 0x28 / mStart 0x10 /
  mResetStart 0x34 / mEnd 0x1c` exactly.
* `BandList`: vbase at `0x358`, so `subi r3,r31,0x118` = `this+0x240` = UIList's
  **own** vbase offset — that is the `SAVE_SUPERCLASS(UIList)` call, *not* a
  member access. All 16 writes then resolve to named members.

This convention is MSVC's own and our compiler reproduces it; nothing in the
source models it.

### 2.2 What was actually missing

Retail's element serializers are ours and were absent from the tree — the
briefed "+1/+388 B and +1/+172 B" undercounts the work by three functions:

| retail addr | what it is | size |
|---|---|---:|
| `fn_8233C4E0` | `BandList::Save` | 388 B |
| `fn_824C9540` | `EventAnim::Save` | 172 B |
| `fn_8233BA58` | `HighlightObject::Save` (a **member** call in retail's vector loop) | 136 B |
| `fn_824C94D0` | `operator<<` for `list<KeyFrame>` | 108 B |
| `fn_8233C260` | `operator<<` for `vector<HighlightObject>` | 104 B |
| `fn_824C9478` | `operator<<(BinStream&, const EventAnim::KeyFrame&)` | 88 B |
| `fn_824C8F68` | `operator<<(BinStream&, const EventAnim::EventCall&)` | 72 B |

### 2.3 Predicted vs measured

**Leg 1 — source only (`02c105d5`).** Predicted **Δ0**, because all seven retail
addresses were still anonymous so objdiff cannot pair them by name. Measured
**Δ0 exactly** (43,206 / 3,936,716). ⚠ This leg is unmeasurable by design — an
unpaired row shows no raw diff either — so its only products are "it compiles"
and the exact mangled spellings, read out of **COFF after a build** (reflinked
objs are pre-renamer), never hand-mangled.

**Leg 2 — 6 carves + 7 names (`d698a161`).** Predicted **+7 fns / +808 B**;
measured **+4 fns / +700 B**.

* CROSSED, 5 rows / **808 B — the crossing set is byte-exact as predicted**:
  BandList::Save 388, HighlightObject::Save 136, `list<KeyFrame>` op<< 108,
  `vector<HighlightObject>` op<< 104, EventCall op<< 72.
* **MISS 1** — `EventAnim::Save` (172 B) and the KeyFrame `op<<` (88 B) did not
  cross. `EventAnim::Save` reads **41/43 instructions EQUAL**; its only two
  charges are `bl` relocation *names* pointing at `fn_824C93C0`, the ICF fold
  survivor the map spells `HamCamShot::Target`. The body is right; the name is
  arbitrary. ⚠ I also predicted these would still take `mpn` 100 — **wrong, and
  worth recording: a `[sym]` relocation arg is a NON-immediate arg diff, so it
  is charged on BOTH rulers, not only on fuzzy.**
* **MISS 2** — one row **FELL OUT**: BandCamShot's `list<HamCamShot::Target>`
  writer, −108 B. This is the standing naming economics firing exactly:
  `fn_824C8F68` was anonymous, so `name_check` **forgave** it at that call site;
  naming it converted a forgiven site into a checked one. **Kept** — it is bug
  exposure, and the 100% reading was resting on the fold being invisible.

The `.pdata` re-derivation is expected and is recorded here so the next lane
does not mistake it for a failure: the six `.text` carves made dtk re-derive 4
`.pdata` ranges and rewrite `splits.txt`, which fails the build once via the
split-guard. `.pdata` is derived output; the retry is a fixed point.

⚠ The EventAnim boundary is **`0x824C95F0`, not W16-K's proposed `0x824C95F8`** —
the 8 bytes at `0x824C95F0` are the EH prefix of EventTrigger's `fn_824C95F8`,
and handing them to EventAnim would split a COMDAT across units.

## 3. The two namings — sign, and every falling row

Pre-registered: small positive at best, real chance of net negative, because
**22** `?Export@RndDir@@$4*` and **24** `?Replace@RndTransformable@@$4*` map rows
branch to these two addresses and naming converts each forgiven placeholder site
into a checked one.

**Measured (`f10bb44e`): Δ0 EXACTLY — 0 rows crossed, 0 rows fell out.** There
are no falling rows to adjudicate and nothing to revert.

That zero is the result, not the absence of one: **46 call sites went from
forgiven to checked and produced zero new charges.** A wrong name at either
address would have charged every one of those thunks, so W16-K's 20-fold and
24-fold convergences are now confirmed **by measurement**, not only by counting.

`fuzzy_match_percent` rose **+0.001568 pp** — partial credit from two rows that
were previously UNPAIRED and now pair:

* `?Replace@RndTransformable@@UAAXPAVObjRef@@PAVObject@Hmx@@@Z` pairs in unit
  `Trans` at **16.9%**, and immediately **exposes a real source divergence**: our
  body emits `??_R0?AVRndTransformable@@` / `??_R0?AVObject@Hmx@@` RTTI
  descriptors for a `dynamic_cast` retail does not perform, plus a `beq`/`bne`
  polarity inversion. That defect was invisible while the row was unpaired.
* `0x82403728` is inside **`Anim.cpp`'s** `.text` pin, so `RndDir::Export` is not
  that unit's to define and the row cannot pair. The name is still the correct
  identification and costs nothing; re-homing it is a splits question.

## 4. Commits

| commit | what |
|---|---|
| `2482b190` | map: CubeTex 3-cycle thunk rotation — **+1 fn / +16 B**, predicted exactly |
| `98cbc7ed` | map: three proven thunk renames — accuracy-only, **Δ0 B** as predicted |
| `02c105d5` | src: `EventAnim::Save` + `BandList::Save` bodies — blocker refuted, **Δ0** |
| `d698a161` | map+splits: carve and name both Saves — **+4 fns / +700 B** |
| `f10bb44e` | map: name `Export@RndDir` and `Replace@RndTransformable` — **Δ0**, 46 sites confirmed |

## 5. NOT done, with reasons

1. **The `fn_824C93C0` fold alias — 368 B behind one decision** (the withheld
   172 + 88 B, plus restoring the 108 B that fell out). MAPID-1's own withdrawal
   test **passes here in the positive direction**: it withdrew spellings whose
   element serializer is map-resident at an address retail's list writer does
   *not* call, and `EventCall`'s element serializer **is** `fn_824C8F68` —
   exactly what retail's `fn_824C93C0` calls at `0x50`. Not installed because an
   alias lifts the score **by construction**, and this lane did not run a
   mechanical T1 byte-identity verification of the *HamCamShot half* of the fold.
   That verification is the single remaining step.
2. **`ClassName@ReviewDisplay` / the `ForceEmit_*` row** — §1.1. Needs a pin move
   into `ReviewDisplay.cpp`; costs −60 B on the way.
3. **`RndTransformable::Replace`'s exposed `dynamic_cast` divergence** — a source
   fix, newly visible, not attempted here.
4. **Re-homing `0x82403728` out of `Anim.cpp`** — splits work, not naming work.
5. **Rows C–G** — adjudicated IRREDUCIBLE, see §1. No rename is safe: the
   thunk-side base spellings have no retail address, so renaming makes the rows
   permanently 0%.

## 6. Native gate

`src/` changed (four files), so the gate is required. Run last, from the
worktree, after every other action:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```
