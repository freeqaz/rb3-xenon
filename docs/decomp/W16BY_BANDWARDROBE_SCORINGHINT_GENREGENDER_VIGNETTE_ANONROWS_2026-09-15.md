# W16-BY — BandWardrobe: FindBestScoringHint, ValidGenreGender, OnEnterVignette, and the 12 anonymous rows

Lane W16-BY, 2026-09-15, branch `w16-by`, worktree `/home/free/tmp/wt-w16-by`.
Permuter OFF throughout. Every measurement below is a FULL `./tools/ninja-locked`
build followed by a read of `build/45410914/report.json` on the shipped
`name_check` ruler (`provenance.diff_config`, 22 keys) — never an MCP
`run_objdiff` number, which builds one `.obj` and skips the six obj patchers.

## 1. Result

| | matched_functions | matched_code | matched_code_percent | fuzzy_match_percent |
|---|---:|---:|---:|---:|
| before (lane baseline) | 43,678 | 4,066,968 | 39.68909 | 49.831375 |
| after  | 43,680 | 4,067,836 | 39.69756 | 49.847680 |
| **delta** | **+2** | **+868 B** | +0.00847 | +0.016305 |

`total_code` 10,247,068 · `total_functions` 69,240 · `masked_equal_functions`
23,111 (unchanged). Unit `default/BandWardrobe`: 236 -> 239 matched of 270,
21,188 -> 22,056 B, `matched_code_percent` 65.62723.

Rowset diff vs `~/tmp/rows_w16bu_main.json` (`tools/rowset_snapshot.py diff`,
run inside the worktree):

```
CROSSED IN : 2 rows, 868 B
   +    828 B  default/BandWardrobe::?FindBestScoringHint@BandWardrobe@@QAAHPAVSymbol@@PAVSlotInfo@1@AAH@Z
   +     40 B  default/BandWardrobe::fn_8232D8A4
FELL OUT   : 0 rows, 0 B
NET bytes  : +868
```

`fn_8232D8A4` is a 40 B EH funclet that re-paired by byte signature in the
larger pool; it is disclosure, not source work. **Nothing fell out.**

`ValidGenreGender` accounts for the second matched *function* and zero bytes —
it sits at `mpn` 100.0 / `fuzzy` 99.89, which is the documented split: functions
are counted on `mpn`, bytes follow `fuzzy`.

## 2. FindBestScoringHint — 55.96 -> 100.0 (+828 B)

**The brief's hypothesis was wrong and I surfaced it before acting.** It said our
extra `static Symbol done` was the culprit. Retail has **six** function-local
statics and we had one; our base body was 552 B against a 828 B target. We were
*missing* code, not carrying extra.

Fix, three parts:

1. Six function-local `static Symbol`s — `done`, `mic`, `guitar`, `drum`,
   `bass`, `keyboard` — taking MSVC multi-static guard bits 0x1..0x20. The
   strings were read out of `orig/45410914/band.exe`. The last five are
   constructed and never read; they are kept because `Symbol`'s ctor interns,
   which is the observable effect retail is paying for.
2. A seventh function-local `static Message get_customize_slot_msg(Symbol(
   "get_customize_slot"))` inside the customize branch — guard bit 0x40 plus an
   `atexit` registration, because it has a destructor.
   (`src/system/utl/Messages.h:76` declares `extern Message
   get_customize_slot_msg;` with no definition anywhere in the tree; the
   function-local static shadows that unresolved external.)
3. Replacing the temporary `int _tmp0 = HandleType(msg).Int();` with a named
   `DataNode node = HandleType(msg); outSlot = node.Int();`. Retail
   rematerializes `addi r3, r31, 0x68` before `bl Int` and runs the `DataNode`
   destructor *after* both stores — a temp-lifetime difference, not a register
   one.

★ Two `REGISTER_SWAP` charges were present at the start and **dissolved on their
own** once (3) landed. That is the twelfth recorded instance of the rule that a
register-swap label is a symptom, not a diagnosis; it should never have been
read as permuter-bound.

## 3. ValidGenreGender — 50.28 -> 99.89 fuzzy / mpn 100.0 (+1 function, 0 bytes)

Two changes:

- `MILO_ASSERT(PowerOf2(flags & 0xF8000), 0x3C9);` in place of an
  `if (!PowerOf2(...)) MILO_FAIL("%s has bad focus flags", PathName(shot));`.
  `MILO_ASSERT(cond,line)` is `((void)(cond))` and a pure condition is
  eliminated entirely (19 instructions), whereas `MILO_FAIL(...)` is
  `((void)(__VA_ARGS__))` and **does** evaluate its arguments — which is why
  `PathName` survives at other sites in this file and must not be "cleaned up".
- `static` on the file-scope `GetGenreGenderFlags`, which makes MSVC inline it
  (26 instructions) exactly as retail does.

A prediction miss worth keeping: I predicted base 344 -> 268 B and measured 292.
The cause was six surviving prologue-form instructions (`__savegprlr_27` against
inline `std`/`ld` pairs), which resolved with the second edit. Final base size
372 B = the target exactly.

**Residual (1 charge):** one commutative `and.` operand order. Three source
spellings were compiled and all three are byte-identical, so the operand order
is not reachable from the source. This is the last 0.11%.

## 4. OnEnterVignette — 89.40 -> 91.95 (headline-neutral)

- `LOADMGR_EDITMODE` (the house pattern at `src/system/utl/Loader.h:126-137`,
  `#if defined(MILO_DEBUG) && defined(HX_NATIVE)`) in place of a bare
  `TheLoadMgr.EditMode()`. Retail's body contains **zero `lbz`**, so it has no
  `EditMode` branch at all; our excess was exactly 72 B. Predicted 18
  instructions removed, measured 19 (76 B) — narrated at the time.
- `SlotInfo info[4]` declared before the `static Message msg`, matching retail's
  stack-slot order.

**Residue is regalloc**: 98 of 129 charges are register numbering arising from
`__savegprlr_14` against `__savegprlr_15` (retail spills where we do not and
holds one extra callee-saved value). Permuter-class; permuter is OFF.

## 5. The 12 anonymous rows — identification

I derived the unpaired-symbol list myself rather than trusting the brief's, by
set-diffing the COFF symbol tables of our compiled
`build/45410914/src/system/bandobj/BandWardrobe.obj` against the target
`build/45410914/obj/BandWardrobe.obj` (external function symbols: `section > 0`,
`type 0x20`, `class 2`). Raw diff = 799 symbols, dominated by STL COMDATs that
live in every TU; narrowed to unit-owned scope it is **22**. The brief's list was
shorter, as it warned it might be.

Sanity asserted before trusting any negative: the target obj carries 112 mangled
names of 270 function symbols, i.e. the pre-compile renamer has run. A reflinked
worktree's target objs are pre-renamer and every retail name reads "absent".

Retail bodies were read from the dtk split keyed on the **`.fn` symbol**. ⚠ The
`.s` address column is synthetic for this multi-block unit and renders four of
these twelve bodies at the wrong addresses (`fn_8232E6D0`'s body starts at
`8232E658`, `fn_8232DE80` at `8232DE08`, `fn_82332298` at `82332220`,
`fn_8232FE00` at `8232FD88`). Symbolic branch targets are unaffected and are what
the geometry findings below rest on.

### 5.1 Mapped — six rows, each defensible on bytes

| address | size | name | discriminating evidence |
|---|---:|---|---|
| `0x8232b7c0` | 504 | `AddDircut(BandCharacter*,BandCamShot*,Symbol,int)` | `GetAnimInstrument` -> `GetInstrumentFromSym` -> `GetGroupArray`, the `Sym(0)`/`Int(1)` group loop, `PathName(shot)`, `gGenres`+`"female"` (an inlined `GetGenreGenderFlags`), `FlagString` x2. 504 B on both sides. |
| `0x8232c450` | 232 | `OnGetMatchingDude` | sret `DataNode` written with type tag 4 (object); loops the 4 main chars, skips the source char, compares `GetAnimInstrument` of each. |
| `0x8232e6d0` | 224 | `Merger(const Merger&)` | three `String` default-ctors + an ObjRef vtable init, then tail-calls `0x8232de80`. |
| `0x8232de80` | 160 | `Merger::operator=` | three `String::operator=`, `ObjRefConcrete<BandCharacter>::SetObjConcrete`, byte/int field copies, returns `this`. |
| `0x8232ba78` | 152 | `InstrumentMatch` | six arguments in exactly the mangled order (`int*`, `const SlotInfo*` strided `0x10` over 4 slots, `int`, three `int&` outs), calls `GetInstrumentFromSym`. |
| `0x8232fe00` | 108 | `NewObject` | `StaticClassName@BandWardrobe` -> `MemAlloc(0xE0)` -> `??0BandWardrobe` -> `Hmx::Object` base adjust. Nothing else in the TU allocates `sizeof(BandWardrobe)`. |

`0x8232de80` and `0x8232e6d0` are corroborated by a third, independent number:
the `0x64` element stride that `0x82332298` advances a vector pointer by is
`sizeof(FileMerger::Merger)`, consistent across all three bodies.

**Measured effect of the map edit, full build:** `matched_functions` 43,680 and
`matched_code` 4,067,836 **both unchanged**; whole-binary `fuzzy_match_percent`
49.836918 -> 49.847680. That is exactly the documented economics — `matched_code`
is all-or-nothing at `fuzzy == 100`, so pairing a 0% row buys accuracy, call-site
checkability and bug exposure, never bytes. The six rows moved
0% -> 53.55 / 77.55 / 80.79 / 84.87 / 86.44 / 95.00, **which is itself the
confirmation of each identification**: a wrong name diffs unrelated code and
reads near zero.

`symbols.txt` was already a fixed point (md5 unchanged across the forced
re-split), as expected for a map-only edit.

### 5.2 NOT mapped — five rows that are not functions (dtk mis-carve)

Two real functions were carved into five rows. The evidence is **symbolic branch
targets**, which the synthetic address column cannot corrupt:

- `0x8232ae70` (24) + `0x8232ae88` (56) + `0x8232aec0` (8) = **88 B, one
  function** = `FindTarget(Symbol, const TargetNames&)` (our symbol is 88 B).
  `fn_8232AE88` is entered by `bdnzf lt, fn_8232AE88` **from inside its own
  body**, and `fn_8232AEC0` by `beq cr6, fn_8232AEC0` — plain branches, not
  `bl`. Body: reject `r4` against a global Symbol, then scan 4 names, then
  `r3 = *(r3 + (idx + 0x19) * 4)`.
- `0x8232afb0` (120) + `0x8232b028` (12) = **132 B, one function** =
  `MostImportantHuman` (our symbol is 132 B). `fn_8232B028` ends
  `bdnz .L_8232AFCC`, branching backwards into `fn_8232AFB0`'s body.

Naming a fragment would assert something false and would pair 24 B of target
against 88 B of ours. **These need a splits/carve fix, not a map entry.**

### 5.3 NOT mapped — one row with an unresolved spelling

`0x82332298` (116 B) is `vector<FileMerger::Merger>::push_back` by shape:
`_M_finish` vs `_M_end_of_storage`, `_Param_Construct<Merger>`, advance `0x64`,
else `_M_insert_overflow_aux<vector<Merger>>`. But our obj emits **no**
`?push_back@?$vector@UMerger@FileMerger@@...` — only
`?push_back@?$ObjVector@UMerger@FileMerger@@@@QAAXABUMerger@FileMerger@@@Z` at
96 B. Mapping a spelling we do not define pins the row at 0% permanently.

**Rejected candidate, with the discriminating byte:** `0x82332298` is **not**
`OnEnableDebugInterests`, despite matching its compiled size of 116 B exactly.
It carries the `0x64` vector stride and **no `DataNode`/`DataArray` traffic
whatsoever**, which a `?AVDataNode@@` return type requires. Size coincidence
only.

## 6. Near-crossers — priced, all five declined

Priced from `report.json`'s charged-site list, per the rule that a mismatch
*count* is the wrong instrument. **The verdict is uniform: not one of the five
has a charge that names a source construct.**

| row | size | fuzzy | charges | verdict |
|---|---:|---:|---|---|
| `SetVenueDir` | 244 | 96.72 | 2 | `mr r3, r25` moved two positions among three independent argument moves before a call. Scheduling. |
| `OnEnterCloset` | 356 | 99.47 | 9 | all nine are one r23<->r24 register-numbering swap (incl. two adjacent stores reordered). Regalloc. |
| `LoadMainCharacters` | 1,776 | 99.79 | 17 | 15 register-numbering/adjacent-load scheduling; only `[250]`/`[254]` name anything structural (a `+0x18` offset). Cannot cross without the other 15. |
| `OnSelectExtras` | 920 | 99.96 | 2 | both are ICF fold-alias relocation *names*, and one is refused by construction — see below. |
| `SelectExtra` | 388 | 99.95 | 1 | one ICF fold-alias relocation name. Alias-lane work, not source work. |

### 6.1 The `SetVenueDir` MakeString finding is a fold, not a defect

The call diff shows target `MakeString<const char*, const char*>` against our
`MakeString<const char*, int>`. Retail's format strings read out of `band.exe`
are `"crowd_%s%02d"` and `"%s_base"` — **byte-identical to ours**. Both
instantiations pass a 32-bit value identically, so the COMDATs folded and retail
kept the other spelling. It carries **no charge** (diff score 200 = the two `mr`
only). Do not re-open this as a source defect.

### 6.2 The alias lever is 388 B, not 1,308 B

`SelectExtra` and `OnSelectExtras` both charge
`?insert@?$list@VSymbol@@...` against retail's
`?insert@?$list@PAVObject@Hmx@@...`. `scripts/symbol_aliases.json` **group
1485** is exactly that survivor, at `0x823d14c0`, already carrying
`list<void(*)()>`, `list<char*>`, `list<Dep*>` and `list<CharClip*>` as folded
members — all pointer element types. Adding `list<Symbol>` is a runnable CF2
question.

★ **But `OnSelectExtras` cannot be collected this way, and this is why you price
before you brief.** Its second charge is `_S_sort<Symbol>` against retail's
`_S_sort<unsigned int>`, and `scripts/target_symbol_map.json` places
`_S_sort<Symbol>` at `0x827e5d88` and `_S_sort<unsigned int>` at `0x824e1858` —
**two distinct addresses, so they did not fold**, and an alias between symbols
the map places at distinct addresses is a *fatal fabricated-alias refusal* by
construction. Since `matched_code` requires `fuzzy == 100`, closing only the
`insert` charge buys `OnSelectExtras` nothing. **Realisable prize: `SelectExtra`,
388 B.**

⚠ A trap for whoever takes it: `?insert@?$list@HV...` (`list<int>`) appears as
`withdrawn` from group 1485 and nine sibling groups. Its class is
`FABRICATED_CLOSURE_NOT_PARTITION` — a bookkeeping withdrawal against a
fabricated closure, **not** evidence that a by-value element type cannot fold.
Group 1485's own `restored` record shows lane W16-Y restoring a member on exactly
that basis, with the note that a CF2 retail-byte re-derivation *outranks a
closure-shaped withdrawal*. So the `list<int>` withdrawal must not be read as a
precedent against `list<Symbol>`.

## 7. Rows left unfixed, and the exact evidence that would change each verdict

| row / item | current verdict | what would change it |
|---|---|---|
| `ValidGenreGender` last 0.11% | unreachable from source | a source spelling of `flags & 0xF8000` that compiles to the opposite `and.` operand order. Three spellings were compiled and all three were byte-identical; a fourth that differs would settle it. |
| `OnEnterVignette` residue (129 charges) | regalloc | retail uses `__savegprlr_14`, we use `_15`. A source change that makes MSVC hold one more callee-saved value across the same span — not a register hint — would move it. Otherwise permuter. |
| `SetVenueDir` (244 B) | scheduling | a source form in which `dir` is materialised *after* the `false` argument. If `dir` were re-read from a member rather than held in a local across the loop, the `mr` would schedule last. |
| `OnEnterCloset` (356 B) | regalloc | the whole charge set is one r23<->r24 pair; only a change in live-range structure (not declaration order, which is measured inert for register-only swaps) can renumber it. |
| `LoadMainCharacters` (1,776 B) | regalloc + 1 offset | resolve `[250]`/`[254]` first: our `addi r3, r31, 0x6c` against retail's `addi r3, r31, 0x54` is a **+0x18 member-offset delta**. Run `scripts/harvest/class_layout_report.py` on the struct `r31` points at — *ask the compiler, not the `// 0xHEX` comments*. If the layout is confirmed correct, the row is permuter-only. |
| `OnSelectExtras` (920 B) | not collectable | only a retail-byte proof that `_S_sort<Symbol>` and `_S_sort<unsigned int>` are the *same* address — i.e. that one of the two map entries is wrong. Until then the two charges cannot both be closed. |
| `SelectExtra` (388 B) | deferred to the alias lane | `tools/comdat_fold_gate.py` with a one-pair worklist (`target` = the group-1485 survivor, `target_addr` `0x823d14c0`, `base` = `?insert@?$list@VSymbol@@...`). A **CF2 PASS** licenses adding it to group 1485; anything else does not. Note our spelling is absent from `target_symbol_map.json`, so the worklist needs a `base_addr` the gate will accept. |
| `0x82332298` (116 B) | identified, unmappable | whether retail emits `ObjVector<Merger>::push_back` as a separate body. If it does not, the 116 B body is the `vector` spelling and we must emit that symbol before mapping it. |
| `0x8232ae70`/`ae88`/`aec0`, `0x8232afb0`/`b028` (220 B) | dtk mis-carve | a `splits.txt`/`symbols.txt` carve that makes `0x8232ae70` an 88 B function and `0x8232afb0` a 132 B one. Both of our counterpart symbols are already exactly those sizes, so the rows should pair immediately once carved. **This is the single cheapest structural item left in the unit.** |
| `SyncProperty` (2,416 B) | drained, do not re-fund | the `lwz r5,0x60(r3) ... lwzx` shape in `SyncVignetteInterest`/`SyncEnableBlinks`/`ForceBlink`. Permuter-class; the brief already flagged it as a drained lever and this lane did not re-open it. |

## 8. Gate chain

All run in the worktree, in this order, after a full `rc=0` build:

```
BUILD rc=0                                            (~/tmp/rb3_build_w16by_12.log)
scripts/verify_ruler_agreement.py --check      rc=0   both entry points resolve the same ruler
scripts/verify_objs_patched.py --verify-manifest rc=0 1219 decomp + 3105 target objects match
                                                      (tree_sha256=adb046752f6035a5)
tools/icf_alias_finder.py --validate           rc=0   PASS -- 1407 map-consistent, 249 tolerated,
                                                      0 contradicted, 1657 total
tools/funclet_homing.py --validate             rc=0   PASS -- 25052 HOMED, 1226 ORPHAN,
                                                      1 MIS-PINNED, 42 UNPINNED-FUNCLET
```

The native gate is this lane's last action; its `NATIVE_GATE_RESULT` line is
reproduced verbatim in the final report.

## 9. Files touched

- `src/system/bandobj/BandWardrobe.cpp` — tasks 1-3.
- `scripts/target_symbol_map.json` — six entries, task 4.
- this document.

No header, `src/system/obj/Object.h`, ObjPtr/ObjPtrList, `src/system/dsp/`,
VocalTrackDir, GemTrackDir, Tour or BandProfile file was touched.
