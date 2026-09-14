# The `$4`/`$2` vtordisp-thunk FALSE-100 census (lane W16-K, 2026-09-14)

**Provenance.** All figures measured in worktree `/home/free/tmp/wt-w16-k`,
branch `w16-k`, from main tip `36b28258`. Every number comes from a full
`./tools/ninja-locked` that returned **rc=0**, read out of
`build/45410914/report.json` with every numeric `int()`-coerced. Ruler is the
shipped default, read from the report's own `provenance.diff_config`:
**`functionRelocDiffs=name_check`**. Row-level pricing is by set-diff of the
`fuzzy == 100` row set (`tools/rowset_snapshot.py`), never by aggregate alone.

---

## 0. Whole-binary MEASURES, before and after the lane

| measure | before | after | Δ |
|---|---:|---:|---:|
| `matched_functions` | 43,191 | **43,196** | **+5** |
| `matched_code` | 3,932,392 | **3,932,456** | **+64 B** |
| `total_code` | 10,245,956 | 10,245,956 | 0 |
| `total_functions` | 69,217 | 69,217 | 0 |
| `matched_code_percent` | 38.379944 | 38.380566 | +0.000622 |
| `fuzzy_match_percent` | 49.318207 | 49.318214 | +0.000007 |

Rowset set-diff against the baseline snapshot: **5 rows crossed in (+64 B), 0
fell out.** Nothing regressed anywhere in the binary.

---

## 1. The mechanism

A 12–16 byte adjustor thunk has the same instruction stream for every thunk in
the program:

```
lwz   r11,-4(rN)          ; 8163fffc / 8164fffc
subf  rN,r11,rN           ; 7c6b1850 / 7c8b2050
[addi rN,rN,-M]           ; optional, M = the adjustment
b     BODY                ; the ONLY identifying information in the symbol
```

Its entire identity is the `b` relocation. `name_check` **forgives a
relocation whose target carries a placeholder name** (`fn_`, `lbl_`, … — see
`is_placeholder_symbol_name` in objdiff-core `diff/code.rs`). So:

> **A thunk row whose retail branch target is unnamed reads `fuzzy == 100`
> regardless of whether our thunk targets the right function.**

That is the false-100. The same wrong name pointing at an *identified*
function is **charged** instead. The asymmetry is the whole story of this lane,
and §5 shows both halves of it inside a single defect.

## 2. The instrument — `tools/thunk_false100_census.py`

It occupies the cell no existing tool reaches: **named thunk, UNNAMED target.**
(`thunk_target_audit.py` handles named targets; `unnamed_thunk_census.py`
handles unnamed thunks.) It imports validated machinery rather than
reimplementing it — `pdata_extent`, `fold_thunk_gate.mask_word`,
`comdat_bytes.comdats`.

Design points that were forced by defects caught during construction, each of
which had produced a wrong verdict first:

- **`FALSE_100` requires a provable *identity* difference.** The first version
  called `BandSwatch::Save` false on `size 4 vs 80` — but our 4 B body is an
  unwritten stub, which proves *our body is missing*, not that retail's target
  is not `Save`. Size/byte differences now yield `UNDECIDED` with a reason.
- **Descriptors, not chasing.** An 8 B adjustor has no `.pdata` record, so a
  recursive walker steps straight past the function that *is* the answer. Four
  rows were wrongly called FALSE this way. Only a bare `b` (identity) may be
  followed; an adjustor changes `this` and therefore terminates the walk.
- **Shape-matched `addi`**, never a constant word — the adjustment varies per
  class.
- **Unnamed finals are byte-resolved** before being called a disagreement.
  Five rows had *matching* displacements and differed only because retail's
  final was printed as a raw int; that is missing evidence, not disagreement.

`--selftest` pins two fixtures to **retail addresses** (immutable `band.exe`),
so repairing a map row cannot rot them — verified live: the fixtures survived
both map edits in this lane untouched. It carries **two sabotage legs**, and
both are demonstrated to flip the verdict:

| leg | effect | required |
|---|---|---|
| disable the dtor-shape detector | dtor fixture `FALSE_100` → `UNDECIDED` | must NOT be FALSE_100 |
| blind the descriptor comparison | `UnisonIcon::Copy` `TRUE` → `FALSE_100` | must NOT be TRUE |

`SELFTEST PASS` is therefore earned, not asserted.

## 3. Census result

**325 named thunk rows at `fuzzy == 100` whose retail branch target is
UNNAMED — 3,812 B of forgiven stratum, sized here for the first time.**

Final state (after this lane's two repairs):

| bucket | rows | bytes |
|---|---:|---:|
| **FALSE_100** | **3** | **36** |
| TRUE | 24 | 268 |
| UNDECIDED | 298 | 3,508 |

Evidence breakdown:

| bucket | evidence | rows | bytes |
|---|---|---:|---:|
| FALSE_100 | `OURS_BODY_RETAIL_ADJ` | 2 | 24 |
| FALSE_100 | `RETAIL_IS_DELETING_DTOR` | 1 | 12 |
| TRUE | `BYTES_AGREE` | 20 | 220 |
| TRUE | `ADJ_AGREE` | 4 | 48 |
| UNDECIDED | `SIZE_DIFFERS` | 202 | 2,504 |
| UNDECIDED | `NO_OUR_BODY` | 56 | 556 |
| UNDECIDED | `BYTES_DIFFER` | 14 | 168 |
| UNDECIDED | `OUR_BODY_STUB` | 13 | 156 |
| UNDECIDED | `NO_RETAIL_EXTENT` | 8 | 64 |
| UNDECIDED | `ADJ_FINAL_UNRESOLVED` | 5 | 60 |

At entry the census read **4 FALSE_100 / 52 B**; one was repaired in this lane
(§5.1), leaving 3.

⚠ **The proven-wrong population is NOT 3 rows.** `UNDECIDED` is "this lane's
per-row evidence cannot decide", not "probably fine" — §5.2 repaired a row that
sat in `UNDECIDED` and was provably wrong. See §7.

---

## 4. The three surviving FALSE_100 rows — and why each is unfixable *here*

All three have an adjustor token that **agrees with retail's bytes**; what is
wrong in each case is the `class::method` half of the spelling. And in all
three the method the row names has **no identified retail address anywhere in
the map**, so there is nothing to re-home to.

### 4.1 `?Save@BandSwatch@@$4PPPPPPPM@A@AAXAAVBinStream@@@Z` @ `0x822AE7A0` (12 B, `default/BandSwatch`)

Retail bytes: `8163fffc 7c6b1850 480009d0` — vtordisp, adjustment 0 (`A@` = 0,
correct). Destination `0x822AF178` is an **80 B scalar deleting destructor**:
`addi r31,r3,-0xa4`; `bl 0x822AECB0`; `clrlwi. r11,r30,31` (the bit-0 test);
`bl 0x827BC430` (a map-named `MemFree`); returns `this`. Both a bit-0 test
**and** a `bl` reaching a named `MemFree` are required — either alone
over-fires.

**Not repairable.** The correct spelling would be a deleting-dtor thunk
`??_EBandSwatch@@$4PPPPPPPM@A@AAPAXI@Z`. `BandSwatch.obj` emits
`??_EBandSwatch@@$4PPPPPPPM@CFI@AAPAXI@Z` at **16 B** (it has an `addi`), where
retail's is **12 B with adjustment 0** — our class layout gives a non-zero
adjustment where retail has none, so the spelling we would need is not one our
compiler produces. Every other 12 B `??_E…$4…A@` in that obj belongs to a
different class (`DxMesh`, `RndAnimatable`, `RndTexRenderer`, `DxTexRenderer`).

### 4.2 `?PreSave@WorldInstance@@$4PPPPPPPM@A@AAXAAVBinStream@@@Z` @ `0x824EB260` (12 B, `default/Instance`)

Retail: `8163fffc 7c6b1850 4bfff030`, adjustment 0. Destination `0x824EA298`
is an adjustor of **20** that resolves to
`?Replace@RndDir@@UAAXPAVObjRef@@PAVObject@Hmx@@@Z`. Ours is a 4 B `blr` body.

**Not repairable.** Retail splits the adjustment into **two hops** (a 12 B
disp-0 thunk → a 20-adjustor → the body). Our compiler emits that as **one 16 B
thunk**: `Instance.obj` has `?Replace@RndDir@@$4PPPPPPPM@BE@…` at 16 B, where
`BE@` = 20 — the right adjustment, the wrong geometry. A 16 B COMDAT cannot
match a 12 B target row; renaming would un-match it. The intermediate that
retail references is not a symbol our build produces.

### 4.3 `?SetTypeDef@PreloadPanel@@$4PPPPPPPM@A@AAXPAVDataArray@@@Z` @ `0x827B4258` (12 B, `default/PreloadPanel`)

Retail: `8163fffc 7c6b1850 4bfff480`, adjustment 0. Destination `0x827B36E0` is
an adjustor of **60** resolving to `?SetTypeDef@UIPanel@@UAAXPAVDataArray@@@Z`.
Ours is our own real 284 B `PreloadPanel::SetTypeDef` body.
`?SetTypeDef@PreloadPanel@@UAAX…` is **absent from the map entirely** — retail's
`PreloadPanel` does not appear to override `SetTypeDef` at all.

**Deliberately NOT repaired, though a pairing spelling exists.**
`PreloadPanel.obj` emits `?SetTypeDef@UIPanel@@$4PPPPPPPM@A@AAXPAVDataArray@@@Z`
at 12 B with disp 0 — geometry identical to retail's row — so renaming the row
to it *would* pair and *would* read 100. But retail reaches `UIPanel::SetTypeDef`
through a further 60-adjustor that our build does not emit, so I cannot prove
the thunk's symbol names the final method rather than the intermediate. That
makes the rename **swapping one unproven name for another**, which the forgiven
stratum rewards with a false 100 either way. Per the standing rule — a
placeholder is forgiven, a wrong name is charged — an unproven rename here buys
no bytes and costs integrity. **Left as-is, with the evidence recorded.**

---

## 5. Fixes landed — predicted vs measured

Both are the same defect class, found only because the census forced the
question "what does this thunk *actually* branch to?".

### 5.1 The `CDA@` transposition in `TrackDir` — `39ebb841`

`0x827DF4E0` and `0x827DF530` are both 16 B vtordisp thunks with byte-identical
adjustment `3863fdd0` (`addi r3,r3,-560`; the `CDA@` token spells exactly 560:
C=2, D=3, A=0 → 0x230). **Their names were swapped.**

`0x82403728` is proven to be `?Export@RndDir@@UAAXPAVDataArray@@_N@Z` off
**retail's own branch layout**, not off any name we supply: 23 named map rows
branch to it, **20 of them independently spelled
`?Export@RndDir@@$4PPPPPPPM@<disp>@…`** at 20 different addresses across many
units, while `?Export@RndDir@@UAAX…` appears nowhere in the map.
`RndDir::Replace` is separately pinned at `0x82402F68` by a 22-fold convergence
of `?Replace@RndDir@@$4*` rows.

| address | branched to | was named | now named |
|---|---|---|---|
| `0x827DF4E0` | `0x82402F68` = Replace | Export | **Replace** |
| `0x827DF530` | `0x82403728` = Export | Replace | **Export** |

Control: the third member of the same `CDA@` family,
`?Highlight@RndDir@@$4…CDA@` at `0x827DF4F0`, targets the correctly-named
`?Highlight@RndDir@@UAAXXZ` — so this is a two-row transposition, not a
family-wide mis-spelling.

**The payoff is asymmetric, and the asymmetry IS the mechanism:**

| row | destination named? | fuzzy before |
|---|---|---|
| `0x827DF4E0` (named Export) | **yes** (Replace) → CHARGED | **98.75** |
| `0x827DF530` (named Replace) | no → forgiven | **100.0 (FALSE)** |

> **Predicted +16 B / +1 function. Measured +16 B / +1 function.**
> Rowset: 1 row crossed in, 0 fell out.

### 5.2 The 5-cycle rotation in `UIFontImporter` — `2ef95885`

Five 12 B vtordisp rows rotated against their actual branch destinations:

| address | actually branches to | was named | now named |
|---|---|---|---|
| `0x82819D70` | `SetType` | Copy | **SetType** |
| `0x82819D80` | `Save` | Load | **Save** |
| `0x8281C540` | `SyncProperty` | SetType | **SyncProperty** |
| `0x8281C550` | `Load` | SyncProperty | **Load** |
| `0x8281ACE8` | `0x8281A070` (unnamed) | Save | **Copy** |

Four are directly provable (destination already named, disagrees with the row).
The fifth closes by elimination **on a closed system**: `?Copy@UIFontImporter@@UAAX…`
is the only one of the five base methods absent from the map, `0x8281A070` is
the only unnamed destination, exactly one row branches to it, and `Copy` is the
only spelling left once the other four are placed.

Controls in the same class, already correct and untouched: `ClassName`
(`0x82819BE8`), `Handle` (`0x8281D068`), `??_E` → `??_G` (`0x82819BF8`).
Guarded mechanically against the duplicate-key phantom: no `UIFontImporter`
spelling occurs at two addresses after the edit. Our obj emits all five `$4`
spellings, so every renamed row still pairs.

> **Predicted +48 B / +4 functions. Measured +48 B / +4 functions.**

⚠ **Reading the set-diff correctly:** the rowset keys on **name**, so "which
named row crossed" is not "which address was repaired". `?Save` was already at
100 at its old address (forgiven) and is at 100 at its new one, so it never
appears in the diff; `?Copy` crossed instead. The aggregate is the
pre-registered figure either way.

---

## 6. Item 2 — `0x822AF178` re-homed out of `CharInterest.cpp` — `4fc47008`

**What it is:** an 80 B scalar deleting destructor (`addi r31,r3,-0xa4`;
`bl 0x822AECB0`; `clrlwi. r11,r30,31`; `bl 0x827BC430` = `MemFree`; returns
`this`). It had been pinned as a **standalone 80 B island** to `CharInterest.cpp`,
whose other 16 blocks all lie in `0x823BD400`–`0x823BEE90` — **1.1 MB away**.
Both the thunk that names it (`0x822AE7A0`) and the member dtor it calls
(`0x822AECB0`) lie inside BandSwatch's block `0x822ADED8`–`0x822AF178`, and it
is the first byte past that block's end. That is a mis-pin, not a membership.

**Pre-registered Δ0 on every key**, on this reasoning: the row is the
placeholder `fn_822AF178` at fuzzy 0, and objdiff pairs by NAME, so an unnamed
row cannot pair with a COMDAT in *either* unit. PINHOME-1's warning that
re-homing is **not** metric-neutral applies to rows that *can* pair; this one
structurally cannot.

> **Measured: Δ0 on `matched_functions`, `matched_code`, `total_code`,
> `matched_code_percent`, `fuzzy_match_percent` — and an EMPTY rowset set-diff
> in both directions**, so the flat total is not a compensating pair.

**Applied-not-inert control** (the load-bearing half, because Δ0 is also what an
inert edit looks like): `fn_822AF178` now appears in `default/BandSwatch` (113
rows) and is absent from `default/CharInterest` (34 rows).

The `.pdata` lines in that commit are **dtk's own re-derivation, not hand
edits**. The first build tripped the split-guard (`THE SPLIT REWROTE ITS OWN
INPUT`, rc=1) — expected for a `.text` move, since dtk re-derives every `.pdata`
range from the `.text` splits and rewrites the file. `report.json` was left
**stale and was not read**. Recovery was one more build, which returned rc=0
exactly as the guard's message says.

### The two W16-H thunks — honest end state

Both `?Save@BandSwatch@@$4…` and `?PreSave@WorldInstance@@$4…` are **proven-wrong
names with no available correct spelling** (§4.1, §4.2). Re-confirmed with the
census instrument rather than inherited from W16-H.

**Recommendation: do NOT null them.** Nulling costs **−24 B** and destroys the
only pointer to the right answer — the row is the record of *where the question
is*. A wrong name that points at an unidentified function is forgiven by the
ruler and costs nothing today; the honest fix is to identify the target, which
is a different lane's work. Recorded here so the next lane does not re-derive it.

---

## 7. What this census CANNOT see — a measured limitation

`0x8281ACE8` (§5.2) sat in **`UNDECIDED`, not `FALSE_100`** — the tool could not
prove it wrong on its own per-row evidence — **yet it was provably wrong**, and
the cycle/neighbourhood argument found it.

⇒ **The `UNDECIDED` bucket (298 rows / 3,508 B) contains real false-100s that
per-row evidence cannot reach.** Do not read `FALSE_100 = 3` as "only 3 rows are
wrong"; read it as "3 rows are wrong *by single-row evidence alone*".

The stronger instrument, demonstrated twice in this lane, is **structural**:

1. **Convergence.** Many independently-named thunk rows branching to one address
   identify that address off retail's layout (20-fold for `RndDir::Export`).
2. **Neighbourhood closure.** Within one class's vtable block, resolve every
   thunk's destination and check the assignment is a bijection. A rotation or
   transposition shows up immediately and is repairable as a unit.

A further structural finding: **the two halves of a transposition land in
different tools' blind spots** — one half is charged (visible to
`thunk_target_audit.py`), the other is a forgiven false-100 (visible only to
this census). Neither tool alone can diagnose it, which is why a tree-wide
sweep for *pairs where both halves are charged* returns **0**.

---

## 8. Flagged, NOT actioned — worklist with evidence

- **`0x82403728` is `?Export@RndDir@@UAAXPAVDataArray@@_N@Z`** (20-fold
  convergence, §5.1) and **`0x823F94E8` is `?Replace@RndTransformable@@UAAX…`**
  (24-fold: all 24 `?Replace@RndTransformable@@$4*` rows branch there, and the
  name is absent from the map). Naming either converts many forgiven sites into
  checked ones — per standing economics that is **a bet paying in bug exposure,
  not bytes**, with a possible negative sign. Not taken in this lane.
- **9 charged thunk rows whose named destination disagrees with the row's
  method**, tree-wide, beyond the ones fixed here. **Deliberately untouched
  because they cluster in `LocalUser` / `RemoteUser` / `NullLocalBandUser` /
  `BandUser`, which is lane W16-L's live surface** — editing them would collide.
  They are: `PreLoad@UIComponent`→Highlight, `ClassName@ReviewDisplay`→a
  `ForceEmit_*`, `IsLocal@LocalUser`→`IsDirPtr@ObjDirPtr`,
  `GetRemoteUser@RemoteUser`→`GetLocalUser`, `CanSaveData@NullLocalBandUser`→
  `GetCrowdMeter`, `UserName@NullLocalBandUser`→`ContentPattern`,
  `SyncProperty@BandUser`→`GetCrowdMeter`, `Load@RndCam`→
  `Replace@RndParticleSys`, `SetTypeDef@UIPanel`→`Highlight@RndDir`. Several
  look like the *destination's* map name is wrong rather than the thunk's.
- `?ClassName@ReviewDisplay@@$4…` targets `?ForceEmit_ReviewDisplay_StaticClassName@@YA…`
  — a `ForceEmit_*` symbol, the metric-fitting class CLAUDE.md warns about.
  Worth a look by whoever owns that area.

---

## 9. Item 1 — the two `Save` bodies: **NOT DONE**, with the blocker named

`BandList::Save` (388 B @ `0x8233C4E0`) and `EventAnim::Save` (172 B @
`0x824C9540`) were **not written**. This is a re-pricing of the handoff, not a
failure to find the addresses — the splits carve and the map entry are computed
exactly below and are *not* the blocker.

### The blocker

**`bs << ObjList<T>` does not exist in our tree.** Verified with a control:
`operator>>(BinStreamRev&, ObjList<T>&)` and `operator>>(BinStream&, ObjList<T>&)`
are at `src/system/obj/Object.h:2497` and `:2511` (grep rc=0, 2 hits); there is
**no `operator<<` for `ObjList<T>` anywhere in `src/`** (grep rc=1). An earlier
COMDAT probe of mine returned 0 for *both* operators — i.e. it could not have
succeeded — and was re-run with a proper control before being believed.

Writing either `Save` therefore requires adding an `operator<<` template to
`src/system/obj/Object.h`, which is a **PCH input** (`decomp_pch.h` = Object.h +
Debug.h, "codegen-load-bearing — keep it sacred"), cascading ~281 TUs, plus
element-level `operator<<` for `EventAnim::EventCall` and `KeyFrame`. That is a
materially different risk class from W16-H's two bodies, which used only
existing operators. **It re-prices the task from "carve + map entry + body" to
"requires a new streaming template in a PCH header", and it needs the native
gate and a full A/B in its own right.**

### The carves, computed and ready

- **`BandList::Save`** — CharSignalApplier's block `0x8233C480`–`0x8233C668`
  holds exactly two functions: a 96 B STL helper at `0x8233C480`
  (`??$__destroy_range_aux@V?$reverse_iterator@PAUBoneOp@CharSignalApplier@@…`)
  and the unnamed 388 B at `0x8233C4E0` (ends `0x8233C664`). Two-line carve:
  CharSignalApplier `end:0x8233C4E0`; BandList's `start:0x8233C668` →
  `start:0x8233C4E0`, merging with its existing `…–0x8233C850`.
  Source stub: `SAVE_OBJ(BandList, 0x72)` at `src/system/bandobj/BandList.cpp:58`.
- **`EventAnim::Save`** — EventTrigger's block `0x824C94D0`–`0x824C97D8` holds 5
  unnamed functions; `0x824C9540` (len 172) ends `0x824C95EC`. Three-way split:
  EventTrigger keeps `0x824C94D0`–`0x824C9540` **and** `0x824C95F8`–`0x824C97D8`;
  EventAnim gets a new block `0x824C9540`–`0x824C95F8`.
  Source stub: `SAVE_OBJ(EventAnim, 0x7F)` at `src/system/world/EventAnim.cpp:77`.

⚠ Do not hand-edit `.pdata` for either — it is derived output and dtk re-derives
it from the `.text` split on every run (§6 shows the split-guard enforcing this).

### `EventAnim::Save`, decoded from retail

The rb3-Wii oracle carries only `SAVE_OBJ(EventAnim, 0x7F)`, so retail was read
directly (same situation as W16-H's `OverdriveMeter`):

```cpp
BEGIN_SAVES(EventAnim)
    SAVE_REVS(1, 0)                 // li r11,1 ; bs.Write(&v,4)
    SAVE_SUPERCLASS(Hmx::Object)    // vbtable: lwz r11,-0x3c(r31); lwz r11,4(r11); add; subi 0x3c
    SAVE_SUPERCLASS(RndAnimatable)  // subi r3,r31,0x2c ; bl fn_82400860
    bs << mKeys;                    // subi r4,r31,0x18 ; bl fn_824C94D0
    bs << mStart;                   // subi r4,r31,0x30 ; bl fn_824C93C0
    bs << mResetStart;              // lbz r11,-0xc(r31) ; 1-byte write via fn_827C4F58
    bs << mEnd;                     // subi r4,r31,0x24 ; bl fn_824C93C0
END_SAVES
```

Corroboration: two **distinct** `operator<<` callees (`fn_824C94D0` for `mKeys`
vs `fn_824C93C0` for `mStart`/`mEnd`) match `ObjList<KeyFrame>` vs
`ObjList<EventCall>`; and the field order mirrors our `Load` exactly
(`LOAD_REVS`, `ASSERT_REVS(1,0)`, `LOAD_SUPERCLASS(Hmx::Object)`,
`LOAD_SUPERCLASS(RndAnimatable)`, `bs >> mKeys`, then the rev-gated
`mStart`/`mResetStart`/`mEnd`). Members: `mStart // 0x10`, `mEnd // 0x1c`,
`mKeys // 0x28`, `mResetStart // 0x34`, `mLastFrame // 0x38`.

`BandList::Save` was located and sized but **not decoded instruction by
instruction** — it is blocked on the same missing operator, so decoding it would
not have changed the outcome.
