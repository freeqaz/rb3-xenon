# W16-AH — the 492 `NO_WITNESS_FOLDED_SIDE` rows: refutation by TYPE EXISTENCE, and one map identification

Lane W16-AH, 2026-09-14, branch `w16-ah`, worktree `~/tmp/wt-w16-ah`, based on main `740e40f7`.
Predecessors: W16-AG (`docs/decomp/W16AG_REFUTED_43_WITHDRAWALS_GUARD7_2026-09-14.md`) and
W16-AD (`docs/decomp/W16AD_UNDECIDED_MASKED_RETAIL_FOLD_WITNESS_643_2026-09-14.md`).

**Headline.** 294 alias memberships withdrawn with `withdrawn` records, all at measured Δ0;
one map row added; one membership refuted by an instrument the brief did not anticipate;
`icf_alias_finder.py --validate` **PASS, 0 contradicted**. Lane-internal measures are
**43,319 / 3,990,660 before and after** — this lane buys accuracy, not bytes, and says so.

---

## 1. Reproduced counts, and where the coordinator was wrong

Every figure in the brief was recomputed from `docs/decomp/W16AG_fold_witness_2026-09-14.json`
(643 rows) before anything was built on it.

| figure | brief | measured | verdict |
|---|---|---|---|
| `NO_WITNESS_FOLDED_SIDE` | 496 / 30,864 B | 496 / 30,864 B | ✅ reproduces |
| Accomplishment band (`0x825f3xxx`), W16-AF's | 4 / 724 B | 4 / 724 B | ✅ reproduces |
| remainder, this lane's population | 492 / 30,140 B | 492 / 30,140 B | ✅ reproduces |
| all four EQ/shape flags true on all 492 | yes | yes | ✅ reproduces |
| rows with 1 / 2 `pairs` entries | 478 / 14 | 478 / 14 | ✅ reproduces |
| distinct `c_N` | 58 | **58** over all `pairs`; **45** as `pairs[0]` | ⚠ both true, stated |
| class names spelled in the 58 `c_N` | 23 | **36** | ⛔ **wrong** |
| Route B population ("types all exist") | 22 / 1,796 B | **19 / 1,432 B** on first parse, then **185 / 11,424 B** after the parse defect below was fixed | ⛔ **wrong, twice over** |

⛔ **The "23 class names" figure is wrong in BOTH directions.** The coordinator's regex counted
unresolved mangled back-reference tokens (`0`, `01`, `1`) as class names while missing real ones.
Resolved with `llvm-undname` (`/usr/bin/llvm-undname`), which expands back-references properly,
the count is **36**. ⚠ `llvm-undname` echoes its input before its output; a batched call assuming
two lines per symbol mis-pairs (29 of 58 paired correctly). Call it **once per symbol**.

⚠ **The brief's proposed negative control is VACUOUS and was not used.** It suggests proving RB3
lacks `HamMove` via "no `hamobj/` strings anywhere in retail". Retail carries **ZERO Harmonix
source paths of any kind** — `rndobj`, `bandobj`, `synth`, `src/system` all read 0 — and its 167
`.cpp` strings are entirely Quazal NetZ middleware. A control that cannot fail is not a control.

⚠ **An ASCII class-name token probe was tested and REFUTED as an instrument**: it reads ABSENT for
`RndTex` and `Fader`, which RTTI proves PRESENT. Recorded so nobody re-derives it.

---

## 2. The type-existence criterion, and the controls that make it able to fail

A membership says "our N folded into survivor S at retail X". If `c_N` instantiates a template on
a class RB3 retail does not contain, that instantiation was never in any retail COMDAT, so the
asserted fold cannot have happened. Under `/GR` a polymorphic class's emitted vtable carries a COL
whose `??_R0` holds the `.?AV<T>@@` string, so *descriptor absent ⇒ no vtable emitted ⇒ no instance
constructed*. **THREE halves, all required** (`tools/w16ah_type_existence.py`):

1. the retail type descriptor is absent from `orig/45410914/band.exe`;
2. the type is **polymorphic in our source** (body-scanned for `virtual`, recursing through the
   base clause) — because half (1) speaks only for polymorphic types;
3. **rb3-Wii declares it in 0 files and DC3 in ≥1** — RB3's own dev decomp, names intact, is an
   instrument on RB3's *source*, independent of retail bytes.

**Positive controls (must read PRESENT):** `Spotlight`, `RndTex`, `ObjectDir`, `EventTrigger`,
`NoteVoiceInst`, `Fader`, `Hmx::Object`. **Decline controls (must be DECLINED):** `BandPatchMesh`,
`HighlightObject` — RTTI-absent but non-polymorphic and present in rb3-Wii, i.e. exactly the case
half (2) exists to resolve. **Template channel control:** `ObjPtrList`. The tool refuses to emit a
selection if a positive control fails, if the decline controls stop declining, if the template
channel is vacuous, or if 0 or all classes qualify.

⛔ **A near-false-refutation, caught only by a pre-registered row count.** The first version probed
TEMPLATES by bare name, which is structurally satisfied for every template. It selected **296 rows
where ~210 were pre-registered**, and was about to refute rows over `ObjPtrVec<Spotlight>` and
`ObjPtrVec<RndTex>` — retail-PRESENT element types. Templates are never spelled by bare name;
retail spells `.?AV?$ObjPtrVec@VSpotlight@@VObjectDir@@@@`, so they must be probed by
**instantiation prefix `?$Name@`**. Fixed, with `ObjPtrList` (45 hits vs `ObjPtrVec`'s 0, same
`ObjRefOwner` base) as the control proving the template channel can return PRESENT.

⛔ **A vtable-based polymorphism probe was tried and DISCARDED.** Scanning our 1,213 objs for
`??_7<T>@@6B@` read 0 for `Spotlight` and `ObjectDir`: it measures "we compile the TU that emits
that vtable", not polymorphism. Replaced with the source-level check.

### 2.1 The control that FIRED — half (3) is load-bearing, not corroborative

`Quazal::Job` is **polymorphic** (`virtual ~Job()`, `virtual void DecoratedExecute()`), is declared
in rb3-Wii, and is NetZ middleware retail unquestionably links (the `/Od` region at `0x82A6D168`,
5,782 functions; `Quazal@@` occurs 27 times) — yet **`.?AVJob@Quazal@@` occurs 0 times in retail**.

⇒ **Halves (1)+(2) alone would have FALSELY REFUTED `??1Job@Quazal@@UAA@XZ`.** Only half (3)
declined it. Every row withdrawn by this lane passed all three halves, so nothing applied is
impeached — but no future lane should treat descriptor-absence as sufficient.

### 2.2 The mirror-image hazard — an rb3-Wii zero can be a WII PORT artifact

Half (3)'s oracle is a **Wii** decomp. For console-specific middleware its zero says nothing about
RB3-360. Tested on retail bytes and the test **failed in the direction that matters**:

| subsystem | retail witness | reading |
|---|---|---|
| Bink | `bink`/`Bink`/`BINK` ×6, `BinkTextures.cpp`, `"Error reading Bink header."`, `"Not a Bink file."` | **PRESENT** |
| Kinect / NUI | `Kinect`, `NuiSkeleton`, `NuiImage`, `NUIAPI`, `xnui`, `nui` — **all 0** | **ABSENT** |

⇒ `BinkMovieImpl`, `MovieImpl` and `FxSendBitCrush360` (`.?AVFxSend@@` reads 1) are **deliberately
NOT refuted** despite rb3-Wii declaring none of them. The same probe says `gesture/` (Kinect) and
`hamobj/` are genuinely absent, so those two subsystems — and only those — carry a
retail-byte absence witness of their own.

---

## 3. Route A — what was applied, in three classes

| class | rows | bytes | evidence |
|---|---:|---:|---|
| `TYPE_ABSENT_FROM_RETAIL_ELEMENT` | 198 | — | `c_N` instantiates a template on a DC3-only polymorphic element type (`Flow`, `FlowLabel`, `FlowNode`, `FlowOutPort`, `HamCharacter`, `HamMove`, `RhythmDetector`, `DepthBuffer3D`, `HamSupereasyData`) |
| `TYPE_ABSENT_FROM_RETAIL_CONTAINER` | 84 | — | element type is retail-PRESENT; only the **container** `ObjPtrVec` is absent |
| **wave 1 total** | **282** | **17,040** | commit `7533b3ae`, 15 batches of ≤20 |
| `TYPE_ABSENT_FROM_RETAIL_ENCLOSING` | 11 | 836 | the callee's **own enclosing class**, plus a subsystem witness (§2.2) — commit `f1dced71` |
| `FOLD_REFUTED_BY_CALLER_BRANCH` | 1 | 108 | §5 — commit `b41b38bf` |
| **TOTAL WITHDRAWN** | **294** | **17,984** | |

**`ObjPtrVec` absence is established from three independent angles**, and kept as its own evidence
tier rather than blended with the element tier: retail RTTI **0** against sibling `ObjPtrList`'s
**45**; rb3-Wii spells `ObjPtrList` **338** times and `ObjPtrVec` **never**; our map carries **6**
`ObjPtrVec` rows against **152** `ObjPtrList`.

⚠ **14 rows / 840 B were HELD, not applied** (`docs/decomp/W16AH_held_rows_2026-09-14.json`).
Their group's **survivor is itself an `ObjPtrVec` spelling**, so the container argument impeaches
the survivor — i.e. the map row naming that address — as well as the member. Withdrawing a member
while leaving the group resting on a name the same argument condemns would assert a different wrong
thing. That is a **map identification question, not an alias one**. The guard is deliberately scoped
to the CONTAINER class: an over-broad first version would have blocked 28 ELEMENT rows, and an
over-cautious correction is still a defect.

### 3.1 Wave 2 — the parse gap, and why it yielded 11 rows and not 40

Wave 1's class regex matched `class X`/`struct X` **inside the mangled signature**, i.e. template
arguments only. It never saw the **enclosing class of the callee**, so `??1FreestyleMove@@UAA@XZ`
and `??1LayerArray@HamDriver@@UAA@XZ` fell into "all types present in retail" **vacuously**.
Closing that gap reached 40 candidate rows in `hamobj`/`gesture`; **29 declined**:

- `MoveReplacer` (15 rows) and `HamMove::LocalizedName` (14 rows) are `hamobj` types that are
  **non-polymorphic** (`virtual=0`) ⇒ half (2) declines them; they stay.
- `Quazal::Job`, `BinkMovieImpl`, `FxSendBitCrush360`, `KerningTable`, `FormatString`,
  `BandPatchMesh`, `HighlightObject`, `CharTransDraw`, `StarDisplay` decline on §2.1/§2.2.

The 11 that qualified are all destructor thunks of DC3-only polymorphic classes — `FreestyleMove`,
`HamGameData`, `HamList`, `HamPartyJumpData`, `HamVisDir`, `SuperEasyRemixer`,
`HamDriver::LayerArray`, `HamDriver::LayerClip`, `SkeletonDir`, `NavigationSkeletonDir`,
`StreamRecorder` — all in one group, survivor `??_GAutomator@@UAAPAXI@Z` at `0x823f5270`.

---

## 4. Predicted vs measured, per wave

Prediction by `tools/w16ag_predict_withdrawal.py`, keyed on **`(survivor, address)`** — never the
census `gi`, which W16-AD §4.2 measured wrong for 4,785 of 5,315 rows, and which was wrong again
here (wave 2 reported `gi=770` for true group index **768**; Route B `gi=1158` for **1156**).
`tools/w16ad_predict_withdrawal.py` is documented dead and was **not used**.

| wave | rows | bytes | predicted Δ | measured Δfns | measured Δbytes | set-diff | commit |
|---|---:|---:|---|---:|---:|---|---|
| 1 (15 batches ≤20) | 282 | 17,040 | 0 B / 0 rows | **0** | **0** | 0 in / 0 out | `7533b3ae` |
| 2 (1 batch) | 11 | 836 | 0 B / 0 rows | **0** | **0** | 0 in / 0 out | `f1dced71` |
| Route B map row | — | — | 0 B / 0 rows | **0** | **0** | 0 in / 0 out | `58d3e712` |
| Route B withdrawal | 1 | 108 | 0 B / 0 rows | **0** | **0** | 0 in / 0 out | `b41b38bf` |

Every wave measured on a **full `./tools/ninja-locked` (rc=0)** after `touch config/45410914/config.yml`,
with `tools/rowset_snapshot.py` (the `fuzzy==100` byte ledger) **and**
`tools/w16ag_rowfuzzy_snapshot.py` (every row's size/fuzzy/mpn, so a fractional `diff_arg` move
would be disclosed). Wave 2 and Route B both recorded **0 rows MOVED**.

### 4.1 Reading Δ0 honestly — the two zero shapes, and proof the channel is live

⚠ **A Δ0 is only meaningful if the edit reached the ruler and the ruler can move.** Both were shown:

- **The edit reached the ruler.** `build/45410914/icf_aliases.map` went **7,964 → 7,953 lines =
  exactly −11** for wave 2 (wave 1: exactly −282). ⚠ Two readings that *looked* like inertness were
  both wrong: the build printing `unchanged icf_aliases.map` refers to the stamp, and the withdrawn
  spellings remaining greppable in the map is **ALIAS-CONSOLIDATION** — a spelling is a folded
  member of 12–16 groups at distinct addresses, so removing one membership leaves it present via
  siblings. Rendering the map from the saved pre-edit JSON proved the delta exactly.
- **The ruler can move.** A reverted control — withdrawing `MakeString<JoinResponseError,int>` at
  `0x8229d148` — predicted **−92 B / −1 row** and measured **exactly that**, naming
  `SessionMessages::?Print@JoinResponseMsg@@UBAXAAVTextStream@@@Z`. Baseline restored afterwards.

⚠ **The two zero shapes are NOT equivalent, and are labelled per wave.** The predictor's vacuity
check distinguishes them:

| wave | call sites examined | shape |
|---|---:|---|
| wave 2 | **0** | weak zero — *no sites exist at all* |
| Route B withdrawal | **1** (in a report row, retail names N at it, 0 charged) | **strong zero** |

---

## 5. Route B — one map identification, and the instrument it exposed

Answered **systematically over all 22 residual `c_N` (185 rows / 11,424 B)** rather than row by row,
because the population splits cleanly and the split *is* the result:

| class | rows | why a map row cannot settle it |
|---|---:|---|
| `HAS_NAMED` | 70 | a map-named caller **already exists** and compares **NE** (`?_M_insert_overflow_aux@vector<map<Symbol,…>>` @`0x8235fba8`; `?_M_fill_insert_aux@vector<BandPatchMesh>` @`0x822aad50`). Nothing is unnamed — our body simply is not retail's, so the offset correspondence is unavailable. |
| `UNPAIRED` | 115 | no caller named at all; every caller is an STLport template helper (`_Copy_Construct`, `_Param_Construct`, `_M_fill_insert_aux`) with no report row and no map address. |

One identification was nevertheless available, and was made:

```
0x823c8cf0  ->  ??_DCharTransDraw@@QAAXXZ
```

Evidence converging from two independent directions: (a) our `??_DCharTransDraw` body compares
**EQ** to retail's 108 bytes there under `retail_compare`, and `0x823c8cf0` **is** a `.pdata`
BeginAddress; (b) retail's `??_GCharTransDraw@@UAAPAXI@Z`, map-named at `0x823c8fc0`, has exactly
**two** branch sites — `+0x20 → 0x823c8cf0` and `+0x30 → ?MemFree@@YAXPAX@Z` — while our `??_G`
calls `??_D` at **+32 (0x20)** and `??3CharTransDraw@@SAXPAX@Z` at **+48 (0x30)**, the second site
corroborating the correspondence independently (class `operator delete` ↔ `MemFree`). The two rival
EQ spellings are mapped **elsewhere** and score `fuzzy==100` today — `??_DScreenshot` at
`0x82824a58`, `??_DRndMultiMesh` at `0x82469fc8` — so neither competes for this address.

**Witness re-sweep** (`tools/w16ad_fold_witness.py --sweep`, run **UNCHANGED**):

| verdict | before (W16-AG) | after | Δ |
|---|---|---|---|
| `NO_WITNESS_FOLDED_SIDE` | 496 / 30,864 | **495 / 30,756** | **−1 row / −108 B** |
| `WITNESS_INCONCLUSIVE` | 52 / 3,512 | **53 / 3,620** | **+1 row / +108 B** |
| `WITNESS_REFUTED` | 63 / 3,828 | 63 / 3,828 | 0 |
| `WITNESS_CONFIRMED` | 23 / 1,944 | 23 / 1,944 | 0 |
| `CHANNEL_B` / `WITNESS_PARTIAL` / `ALREADY_NAMED` | 4 / 4 / 1 | 4 / 4 / 1 | 0 |

**Exactly one row moved, and it is the predicted one.** The map row worked mechanically: the
witness reached the **STRONG** tier on both sides, decoding `??1CharTransDraw@@UAA@XZ` to
`0x823c8a88` via the newly-named caller at +32, and `??1Screenshot@@UAA@XZ` to `0x82824788`. The
addresses **differ**, which would refute the membership — **and it was not refuted**, because
`a_is_pdata_begin` is **FALSE**: `0x823c8a88` is 20 bytes and is not a function start, so it is a
thunk or funclet. Verdict `INCONCLUSIVE_NOT_BEGIN`. W16-AD built that guard exactly so a decoded
difference cannot become a withdrawal on its own, and here it is what stands between a plausible
story and a clobber.

⇒ **Route B's honest score: 1 row left `NO_WITNESS_FOLDED_SIDE`, 0 rows reached `WITNESS_CONFIRMED`
or `WITNESS_REFUTED`, 0 withdrawals follow from the witness.** No name was guessed.

### 5.1 The map row refuted the membership by a DIFFERENT instrument

Adding the row turned `icf_alias_finder.py --validate` **FATAL**:

```
FAIL [_DScreenshot @ 0x82824a58]: target objs name 2 members:
     ['??_DScreenshot@@QAAXXZ', '??_DCharTransDraw@@QAAXXZ']
```

Not a tooling artifact, and not a reason to revert. A symbol cannot both live at `0x823c8cf0` and
be folded into a survivor at `0x82824a58`. The membership is the wrong one, and **the argument does
not depend on the identification at all**: retail's `??_GCharTransDraw` branches at +0x20 to
`0x823c8cf0`, **not** to the survivor — had the fold happened, CharTransDraw's own vector-deleting
destructor would call the survivor. Whatever `0x823c8cf0` is named, it is not `0x82824a58`.

⚠ **Why the membership existed** — recorded so nobody re-derives it: `??_DCharTransDraw`,
`??_DScreenshot` and `??_DRndMultiMesh` are byte-indistinguishable under `retail_compare` precisely
because their **one** discriminating `bl` (each calls its *own* `??1`) targets an address the map
does not name. That is the `UNDECIDED_MASKED` blind spot the fold witness was built for — **not
evidence of folding**. Retail kept three copies.

Withdrawn on the **caller-branch** instrument, group kept with `folded: []` plus a `withdrawn`
record. Validator returns to **PASS**.

---

## 6. Per-`c_N` disposition

Over the 492 rows. `c_N` taken as `pairs[0]` (45 distinct; **58** counting all `pairs` entries —
the 14 two-pair rows account for the difference). "callers paired/unpaired" counts distinct callers
of `c_N` in our objs that are / are not rows in `report.json`.

| rows | bytes | disposition | callers (paired/unpaired) | `c_N` |
|---:|---:|---|---|---|
| 42 | 2520 | HELD 6, WITHDRAWN-w1 36 | 0/6 | `??0Node@?$ObjPtrVec@VSpotlight@@VObjectDir@@@@QAA@ABU01@@Z` |
| 28 | 1680 | STAY 28 | 0/2 | `??0?$pair@V?$ObjPtr@VEventTrigger@@@@V1@@stlpmtx_std@@QAA@ABU01@@Z` |
| 28 | 1680 | WITHDRAWN-w1 28 | 0/3 | `??0Node@?$ObjPtrVec@VFlow@@VObjectDir@@@@QAA@ABU01@@Z` |
| 28 | 1680 | WITHDRAWN-w1 28 | 0/3 | `??0Node@?$ObjPtrVec@VFlowLabel@@VObjectDir@@@@QAA@ABU01@@Z` |
| 28 | 1680 | WITHDRAWN-w1 28 | 0/3 | `??0Node@?$ObjPtrVec@VFlowNode@@VObjectDir@@@@QAA@ABU01@@Z` |
| 28 | 1680 | WITHDRAWN-w1 28 | 0/3 | `??0Node@?$ObjPtrVec@VFlowOutPort@@VObjectDir@@@@QAA@ABU01@@Z` |
| 28 | 1680 | WITHDRAWN-w1 28 | 0/3 | `??0Node@?$ObjPtrVec@VHamCharacter@@VObjectDir@@@@QAA@ABU01@@Z` |
| 28 | 1680 | WITHDRAWN-w1 28 | 0/3 | `??0Node@?$ObjPtrVec@VHamMove@@VObjectDir@@@@QAA@ABU01@@Z` |
| 28 | 1680 | HELD 4, WITHDRAWN-w1 24 | 0/3 | `??0Node@?$ObjPtrVec@VObject@Hmx@@VObjectDir@@@@QAA@ABU01@@Z` |
| 28 | 1680 | WITHDRAWN-w1 28 | 0/3 | `??0Node@?$ObjPtrVec@VRhythmDetector@@VObjectDir@@@@QAA@ABU01@@Z` |
| 28 | 1680 | HELD 4, WITHDRAWN-w1 24 | 0/3 | `??0Node@?$ObjPtrVec@VRndTex@@VObjectDir@@@@QAA@ABU01@@Z` |
| 28 | 1680 | STAY 28 | 1/4 | `??0?$_Rb_tree@VSymbol@@U?$less@VSymbol@@@stlpmtx_std@@U?$pair@$$CBVSymbol@` |
| 28 | 1680 | STAY 28 | 1/2 | `??0BandPatchMesh@@QAA@ABV0@@Z` |
| 28 | 1680 | STAY 28 | 0/3 | `??0HighlightObject@@QAA@ABV0@@Z` |
| 15 | 900 | STAY 15 | 0/2 | `??0MoveReplacer@@QAA@ABU0@@Z` |
| 14 | 840 | STAY 14 | 0/1 | `??0?$pair@$$CBVString@@V1@@stlpmtx_std@@QAA@ABU01@@Z` |
| 14 | 840 | STAY 14 | 0/3 | `??0LocalizedName@HamMove@@QAA@ABU01@@Z` |
| 14 | 840 | STAY 14 | 2/4 | `??0?$_Rb_tree@HU?$less@H@stlpmtx_std@@U?$pair@$$CBHM@2@U?$_Select1st@U?$pa` |
| 3 | 284 | STAY 3 | 0/10 | `??6FormatString@@QAAAAV0@I@Z` |
| 1 | 72 | STAY 1 | 0/3 | `??6FormatString@@QAAAAV0@_K@Z` |
| 1 | 92 | STAY 1 | 0/4 | `??6FormatString@@QAAAAV0@J@Z` |
| 1 | 96 | STAY 1 | 0/6 | `?Unlink@?$ObjPtrList@VNoteVoiceInst@@VObjectDir@@@@AAAPAUNode@1@PAU21@@Z` |
| 1 | 80 | STAY 1 | 0/1 | `?_M_erase@?$_Rb_tree@IU?$less@I@stlpmtx_std@@U?$pair@$$CBIVMeshInfo@RndTex` |
| 1 | 80 | STAY 1 | 0/1 | `?_M_erase@?$_Rb_tree@PAXU?$less@PAX@stlpmtx_std@@U?$pair@QAXVString@@@2@U?` |
| 1 | 80 | STAY 1 | 0/1 | `?_M_erase@?$_Rb_tree@VSymbol@@U?$less@VSymbol@@@stlpmtx_std@@U?$pair@$$CBV` |
| 1 | 88 | STAY 1 | 0/2 | `??1Job@Quazal@@UAA@XZ` |
| 1 | 60 | STAY 1 | 0/4 | `??0?$ObjOwnerPtr@VObject@Hmx@@@@QAA@ABV0@@Z` |
| 1 | 120 | WITHDRAWN-w1 1 | 0/2 | `?Advance@?$ObjDirItr@VDepthBuffer3D@@@@AAAXXZ` |
| 1 | 120 | WITHDRAWN-w1 1 | 0/1 | `?Advance@?$ObjDirItr@VHamSupereasyData@@@@AAAXXZ` |
| 1 | 76 | STAY 1 | 0/1 | `??1BinkMovieImpl@@UAA@XZ` |
| 1 | 76 | WITHDRAWN-w2 1 | 0/3 | `??1FreestyleMove@@UAA@XZ` |
| 1 | 76 | STAY 1 | 0/1 | `??1FxSendBitCrush360@@UAA@XZ` |
| 1 | 76 | WITHDRAWN-w2 1 | 0/1 | `??1HamGameData@@UAA@XZ` |
| 1 | 76 | WITHDRAWN-w2 1 | 0/1 | `??_DHamList@@QAAXXZ` |
| 1 | 76 | WITHDRAWN-w2 1 | 0/1 | `??1HamPartyJumpData@@UAA@XZ` |
| 1 | 76 | WITHDRAWN-w2 1 | 0/1 | `??_DHamVisDir@@QAAXXZ` |
| 1 | 80 | STAY 1 | 0/3 | `??1KerningTable@@QAA@XZ` |
| 1 | 76 | WITHDRAWN-w2 1 | 0/2 | `??1LayerArray@HamDriver@@UAA@XZ` |
| 1 | 76 | WITHDRAWN-w2 1 | 0/1 | `??1LayerClip@HamDriver@@UAA@XZ` |
| 1 | 76 | WITHDRAWN-w2 1 | 0/1 | `??_DNavigationSkeletonDir@@QAAXXZ` |
| 1 | 76 | WITHDRAWN-w2 1 | 0/1 | `??_DSkeletonDir@@QAAXXZ` |
| 1 | 76 | WITHDRAWN-w2 1 | 0/1 | `??_DStreamRecorder@@QAAXXZ` |
| 1 | 76 | WITHDRAWN-w2 1 | 0/1 | `??1SuperEasyRemixer@@UAA@XZ` |
| 1 | 108 | STAY 1 | 1/0 | `??1CharTransDraw@@UAA@XZ` |
| 1 | 12 | STAY 1 | 0/1 | `?PostLoad@StarDisplay@@UAAXAAVBinStream@@@Z` |

⇒ **The paired-caller column is 0 for every `c_N` in the population.** The brief anticipated that a
paired caller spelling a DC3-only instantiation would be a "DC3 leak in RB3 source that the alias
is hiding" — the bug-exposure payout. **That population is empty here**: every caller of every
`c_N` is unpaired, so these memberships forgive **0 bytes today**. That is exactly why every wave
measured Δ0, and it is a finding, not an absence of one — the value of the withdrawals is that the
map stops asserting folds that cannot have happened, before those TUs become pairable.

---

## 7. Gates

All run in the worktree, in order, after the last source edit.

```
BUILD rc=0
RULER rc=0    OK: both objdiff-cli entry points resolve the same ruler.
PATCHED rc=0  [patch-state] OK: 1213 decomp, 3115 target objects match (tree_sha256=f1b36781cd6a6090)
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

`python3 tools/icf_alias_finder.py --validate` — **rc=0**:

```
COVERAGE: 1634 groups classified (1634/1634 reached, 6583 member spellings looked up)
  OK (MAP-CONSISTENT)            1387
  TOLERATED PLACEHOLDER_SURVIVOR   34
  TOLERATED STALE_SPELLING         85
  TOLERATED SURVIVOR_MISLABELED    28
  TOLERATED UNWITNESSED            99
  CONTRADICTION_EXEMPT              1
  CONTRADICTED (FATAL)              0
VALIDATE: PASS -- 1387 map-consistent, 246 tolerated (enumerated above), 0 contradicted, 1634 total
```

Alias-file bookkeeping: groups **1,634 → 1,634** (none pruned), folded **5,243 → 4,949**,
`withdrawn` records **10,164 → 10,458**. Every removed membership carries a record.

---

## 8. NOT done, and why

1. **The 184 residual `NO_WITNESS_FOLDED_SIDE` rows (11,316 B) are left in place.** §5 shows why:
   70 rows already have a named caller that compares NE (a map row cannot help), and 115 have only
   unpaired STLport template-helper callers. Settling them needs the callers' TUs to become
   pairable, or a `splits.txt` re-home — **not** an alias edit. Re-homing is measured **NOT**
   metric-neutral (`+3 fns / +428 B`, lane PINHOME-1) and is out of this lane's scope.
2. **29 rows (1,740 B) — `MoveReplacer` (15) and `HamMove::LocalizedName` (14) — decline on half
   (2).** They are `hamobj` types, so the *subsystem* argument would reach them, but they are
   non-polymorphic and the retail RTTI probe is uninformative for such types. Extending the
   criterion with a subsystem-only half mid-lane, without a fresh control set, is precisely the
   near-false-refutation hazard of §2 — deliberately not done. **A later lane with a stated,
   falsifiable subsystem-only criterion could reach these 29 rows.**
3. **`BinkMovieImpl`, `MovieImpl`, `FxSendBitCrush360` (3 rows) NOT refuted** — §2.2, the Bink
   subsystem is in retail.
4. **`Quazal::Job` (1 row) NOT refuted** — §2.1, the control that fired.
5. **The 14 HELD `ObjPtrVec`-survivor rows (840 B)** — §3, a map question.
6. **The gi-invalidated ablation re-measurement was NOT done.** Budget went to fixing the
   enclosing-class parse gap (§3.1) and to Route B's systematic answer (§5). No conclusion in this
   document rests on a `gi`-keyed figure; every key used here is `(survivor, address)`.
7. **The 53 `WITNESS_INCONCLUSIVE` rows, the 4-row Accomplishment band (W16-AF's) and every source
   line are untouched.** ⚠ The inconclusive count rose 52 → 53 solely because *this lane's* Route B
   row moved into it; no pre-existing inconclusive row was examined or altered.

---

## 9. Artifacts

| path | contents |
|---|---|
| `tools/w16ah_type_existence.py` | the criterion with its controls |
| `tools/w16ah_build_selection.py` | selection split into APPLY / HOLD with `ah_class` |
| `tools/w16ah_apply_withdrawals.py` | applier — group kept, `(survivor, address)` keyed, cap 20 |
| `docs/decomp/W16AH_apply_selection_2026-09-14.json` | wave 1 selection, 282 rows |
| `docs/decomp/W16AH_held_rows_2026-09-14.json` | 14 held rows |
| `docs/decomp/W16AH_prediction_2026-09-14.json` | pristine-tree prediction, 282-row subset |
| `docs/decomp/W16AH_applied_282_2026-09-14.json` | wave 1 apply manifest |
| `docs/decomp/W16AH_wave2_selection_2026-09-14.json` | wave 2 selection, 11 rows |
| `docs/decomp/W16AH_wave2_applied_2026-09-14.json` | wave 2 apply manifest |
| `docs/decomp/W16AH_fold_witness_resweep_2026-09-14.json` | the re-sweep, 643 rows |

⚠ **Three defects in this lane's own scratch analysis, recorded because each had the shape that
CONFIRMS the hypothesis** — the most dangerous kind:

- `addr.startswith('825f3')` matched nothing (the field carries a `0x` prefix), silently reading the
  Accomplishment band back into the population as 496/30,864 instead of 492/30,140;
- `declfile()` returned the first file matching `class X\b`, which for `SuperEasyRemixer` is a
  **forward declaration** with no body — it read as non-polymorphic and dropped a qualifying row;
- `set -- $spec` in zsh does **not** word-split, so a polymorphism probe read an empty file and
  printed nothing, i.e. "no virtuals" — the refutation-confirming answer. Redone in Python.
  (The same hazard, with `--include=*.h` unquoted, earlier aborted every cross-oracle grep and
  returned a vacuous `0` for **all** classes including the `Spotlight` positive control. Caught only
  because that control read 0.)
