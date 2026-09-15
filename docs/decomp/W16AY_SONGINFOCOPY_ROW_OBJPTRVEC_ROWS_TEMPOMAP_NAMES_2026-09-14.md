# W16-AY — SongInfoCopy row, two ObjPtrVec rows, three TempoMap-area names

Lane W16-AY, worktree `~/tmp/wt-w16-ay`, branch `w16-ay`, based on main `ba734115`.
Ruler `name_check` (graded), objdiff 4.2.9 `5a51cd51fe0a353f`.

**Lane total: +1 function / +144 B.** `matched_functions` 43,485 → 43,486;
`matched_code` 4,032,352 → 4,032,496 B; `matched_code_percent` 39.355362 → 39.356770.
`fuzzy_match_percent` 49.661087 → 49.661020 (−0.000067 pp — disclosed withdrawn
false credit, §1). Every figure by set-diff of the `fuzzy==100` row set
(`tools/rowset_snapshot.py`) run inside this worktree, never by inheriting a number.

## 0. Preconditions actually tested (not assumed)

- The briefed baseline was verified **literally** before any work: the worktree's
  `report.json` read 43,485 / 4,032,352 B / 39.355362 % / `total_code` 10,246,004,
  objdiff 4.2.9 `5a51cd51fe0a353f`, ruler `name_check` — matching the brief exactly.
- **Anti-vacuity guard for every COFF claim in this document.** A fresh worktree's
  reflinked target objs are pre-renamer, so a symbol-name negative would be silently
  false. After a full build I asserted **80,899 mangled names among 495,651 symbols**
  over 3,114 target objects. This was later corroborated independently by
  `verify_objs_patched.py --verify-manifest`, which reports the same **495,651
  symbols / 3,114 target objects**. Every "no obj defines X" below is therefore a
  real negative.

## 1. `0x827d2500` — mis-named `_M_throw_length_error`; it is `SetTheTempoMap` (+1 / +144 B)

Commit `c798525b`.

**The brief (and W16-AT's prose) said "reads `TheTempoMap`", predicting a getter/thunk.
Read literally, the body is a STORE:**

```
lis  r11, lbl_82C78F5C@ha
stw  r3,  lbl_82C78F5C@l(r11)
blr
```

`TheTempoMap = arg` — a **setter**, not a reader. Our `src/system/utl/TempoMap.cpp`
already carries it verbatim (`void SetTheTempoMap(TempoMap *tmap)`), immediately
before `ResetTheTempoMap` at the adjacent `0x827d2510`.

Exact spelling from the built COFF tables (never a header):
`?SetTheTempoMap@@YAXPAVTempoMap@@@Z`, **defined by `TempoMap.obj` alone**.

**The payout is the caller channel, not the row.** Exactly one retail caller exists,
keyed on `.fn` and never on the synthetic address column: `fn_82788518` =
`?OnAcceptMaps@SongParser@@UAA_NPAVTempoMap@@PAVMeasureMap@@@Z` (144 B). Its
charged-site list held exactly **one** `diff_arg`: target
`?_M_throw_length_error@…` vs base `?SetTheTempoMap@@YAXPAVTempoMap@@@Z`. The
rb3-Wii oracle confirms `OnAcceptMaps` calls `SetTheTempoMap(mTempoMap)`. This is the
documented *"repairing a WRONG existing map name PAYS"* case — the wrong name was
being financed by its caller.

| | predicted | measured |
|---|---|---|
| `OnAcceptMaps` crosses 99.8611 → 100 | +1 fn / +144 B | **+1 fn / +144 B, exact** |

Set-diff: `OnAcceptMaps` crossed in, **0 rows fell out**. The edit was live — the
re-split ran and the renamer patched 1,838 files / 86,188 renames.

**Disclosed cost, not hidden.** The `0x827d2500` row itself goes **58.33 → 0**. It had
been pairing against `SongInfoCopy.obj`'s STLport `_M_throw_length_error` COMDAT — a
false pairing paying partial credit for the wrong function. `SongInfoCopy.obj` cannot
define the TempoMap spelling, so the truthful name un-pairs it. Neither 58.33 nor 0 is
100, so no headline measure moves; the −0.000067 pp of `fuzzy_match_percent` is exactly
that withdrawn false credit.

**Root cause is a splits pin, not the map.** `SongInfoCopy.cpp`'s `.text` runs to
`0x827D2510` and `TempoMap.cpp` starts there, so `SetTheTempoMap` is homed one function
short of its real TU. **Re-homing `0x827D2500` into `TempoMap.cpp` is worth a further
+1 / +12 B.** Not done — `splits.txt` is barred to this lane (W16-AX owns it).

Injectivity checked over the whole map: no address carried the new spelling, and
`0x827d2500` was the **sole** holder of the old one, so target-side references to the
real `_M_throw_length_error` fall back to forgiven placeholders. 2 pre-existing
duplicate names, unchanged.

## 2. The two `ObjPtrVec` rows — the brief's premise is REFUTED on both

**Nothing installed, no `src/` file touched.** Both rows are **misidentifications**, not
the divergence / missing-instantiation the brief described. Neither can pair under its
current pin at any naming, so neither is worth a map edit today.

### 2(a) `0x8278b7f0` (72 B, `default/HamMove`, fuzzy 0, mpn 1.944) — NOT a HamMove divergence

The brief: *"the symbol IS defined by `HamMove.obj` yet reads 0: a real divergence …
if the fix is in `src/system/hamobj/HamMove.cpp` or the header it instantiates from,
make it and price it."* **The fix is in neither.**

Retail's body (raw bytes read from the `.s`, not objdiff's rendering):

```
A1 4B 00 00  lhz r10, 0x0(r11)   B1 45 00 00  sth r10, 0x0(r5)
A1 4B 00 02  lhz r10, 0x2(r11)   B1 45 00 02  sth r10, 0x2(r5)
A1 4B 00 04  lhz r10, 0x4(r11)   B1 45 00 04  sth r10, 0x4(r5)
A1 4B 00 06  lhz r10, 0x6(r11)   B1 45 00 06  sth r10, 0x6(r5)
39 6B 00 08  addi r11, r11, 0x8  38 A5 00 08  addi r5, r5, 0x8
```

An **8-byte element copied as four HALFWORDS** ⇒ **alignment 2**. That is the decisive
fact: *nothing holding an object pointer can be 2-aligned*, so these bytes cannot be an
`ObjPtrVec<T>::Node` copy **under any layout** — the hypothesis "our `Node` layout is
wrong" is refuted by alignment alone, not by our header.

`PhraseAnalyzer::PhraseData` is `{short unk0, short unk2, short unk4, bool mUnison}` =
**8 bytes, alignment 2** — four halfword moves at offsets 0/2/4/6, stride 8. Exact.

This was reached independently and *then* found to agree with the in-tree record:
`scripts/symbol_aliases.json` already carries a group at `0x8278b7f0` whose **T1**
evidence is "retail bytes at the survivor address are byte-identical to **our compiled
body for the folded spelling**" — the folded spelling being
`__uninitialized_copy<PhraseAnalyzer::PhraseData*>`. T1 verified the *folded* name, never
the survivor; the survivor is simply the map's current (wrong) name.

⚠ Our `ObjPtrVec<T>::Node` body is 96 B, polymorphic, and calls `_Param_Construct` —
**not** byte-identical to these 72 B, so retail could not have folded the two either
(MSVC folds only COMDATs identical including relocations). The group is therefore
recording *a map error as if it were a fold*. `tools/icf_alias_finder.py --validate`
reports it **map-consistent, 0 contradicted** — a live instance of the standing
doctrine that **MAP-CONSISTENT is not PROVEN**: the validator checks that the survivor
is the map name, which is exactly the thing that is wrong here.

**The same misidentification repeats one level up, and it is measurable.** The row
`0x8278bb98` = `_M_insert_overflow_aux<vector<ChecksumData>>` (412 B, **93.641**) charges:

| idx | target | base |
|---|---|---|
| 43/44 | `lhz/sth 0x0` | `lwz/stw 0x0` |
| 45/46 | `lhz/sth 0x2` | `lwz/stw 0x4` |
| 47–50 | `lhz/sth 0x4`, `lhz/sth 0x6` | *(deleted)* |

plus 3 `diff_arg` `bl` sites naming 2 different callees. That is precisely an 8-byte
payload at **alignment 2** (`PhraseData`) versus **alignment 4** (`ChecksumData`, two
pointers) inlined into the same template. ⇒ **`0x8278bb98` is the `vector<PhraseData>`
insert, not the `vector<ChecksumData>` one.**

**Why no map edit was made.** `PhraseAnalyzer.cpp` is **not pinned at all**, and the
PhraseData spelling is defined by `PhraseAnalyzer.obj` **only** — never by `HamMove.obj`
or `FileChecksum.obj`. Renaming today would leave `0x8278b7f0` at 0 (no change) and
would drop `0x8278bb98` from 93.641 to 0 (un-paired), buying nothing on either headline
measure while losing fuzzy credit. The enabling step is a **splits** change, which this
lane is barred from.

**Recipe for the lane that owns splits** (predicted, not measured):
1. pin `PhraseAnalyzer.cpp` covering `0x8278B7F0–0x8278B838` (currently a deliberate
   72-byte block inside `HamMove.cpp`'s entry, justified only by the wrong name) and the
   `0x8278BB98` body;
2. rename both map rows to the `PhraseData` spellings;
3. reverse the `symbol_aliases.json` group so the survivor is the spelling whose body
   actually matches, and record a `withdrawn` note for the ObjPtrVec membership rather
   than deleting it.
   Upside if the bodies match: **up to +484 B** (72 + 412). ⚠ Do **not** add the
   ObjPtrVec spelling as a *folded* member of the reversed group — our body is 96 B and
   differs, so that membership would be a fabricated alias.

### 2(b) `0x822abd60` (112 B, `default/ClipDistMap`, fuzzy 0) — absent instantiation, and the name is wrong too

W16-AV's claim verified literally: **no obj defines the symbol under either spelling**
(`ObjPtrVec` or `ObjPtrList`) — confirmed against the 80,899-mangled-name assertion.

Retail's body identifies the element by stride: `li r9, 0x24` + `divw` ⇒ a
**36-byte** element. Our `ObjPtrVec<RndDrawable,ObjectDir>::Node` is `0x10` = 16 bytes,
and `ClipDistMap::Node` is 12 bytes (3 floats) — **so the mapped name is wrong
independently of the pairing question**. The retail body's own callee is
`0x822aad50` = `_M_fill_insert_aux<vector<BandPatchMesh>>`, i.e. the element is
whatever that 36-byte type is.

**The missing instantiation, recorded precisely as the brief asked:**
`ClipDistMap.obj` contains **no `_M_fill_insert` symbol of any instantiation**. It
defines the full `vector<DistEntry>` member set (`begin`, `end`, `size`, `_M_set`,
`_M_is_inside`, `allocate`, `deallocate`, `_Vector_base` dtor, `reverse_iterator`…) but
never a `resize`/`insert` that would instantiate `_M_fill_insert`. So the row cannot
pair against `ClipDistMap.obj` at **any** name, and no source change to
`src/system/char/ClipDistMap.cpp` can conjure it without inventing a call retail's
`ClipDistMap` does not obviously make. Left untouched; adding a call purely to create a
symbol would be metric fitting.

## 3. Three names W16-AT proved but could not install — two installed (Δ0), one refused

Commit `02599941`.

| address | name (exact COFF spelling) | defining obj | scored ref sites |
|---|---|---|---|
| `lbl_82C78F60` | `?gDefaultTempoMap@@3VSimpleTempoMap@@A` | `TempoMap.obj` | 1 |
| `lbl_82E051A0` | `?TheTaskMgr@@3VTaskMgr@@A` | `obj/Task.obj` | 414 across 91 units |

Both identifications re-proved from retail bytes rather than inherited:

- `fn_827D2510` (`ResetTheTempoMap`) does `lis/addi lbl_82C78F60` then `stw`s that
  **address** into `lbl_82C78F5C` (`TheTempoMap`) — exactly our
  `TheTempoMap = &gDefaultTempoMap`. The only other reference is the dynamic
  initialiser in an unpinned `auto_*` unit.
- Every retail site for `lbl_82E051A0` takes the label's **address**
  (`addi r3, r11, lbl@l`), and `Task.s` does `addi r11,r11,lbl@l; lwz r11, 0x28(r11)`
  — a global **object** with members, which is what `3V…A` spells, not a pointer.

**Prediction before measuring: Δ0.** Naming converts 414 forgiven placeholder sites into
checked ones; a correct name is free, a wrong one creates 414 new charges.

**Measured exactly +0 / +0**, all four measures identical to the last digit, 0 rows
crossed in and 0 fell out. ⚠ The Δ0 is a **real** result, not an inert edit: the renamer
went **86,188 → 86,285** symbol renames (+97), so the rows were applied. No bug exposure
was found — which is itself the finding: our spelling of both globals agrees with retail
usage at all 414 scored sites.

### `fn_827C91A0` — NOT named, and the refusal is controlled

W16-AT left it anonymous because "SecondsToTick" is convention-derived from its twin
`SecondsToBeat`. **I found no proof AT did not have.** Every independent channel returns
a negative *with a working positive control*:

| channel | result |
|---|---|
| DC3 named `ham_xbox_r.map` (`lookup_dc3`) | no `SecondsToTick` |
| retail `default.xex` byte scan (Python, not grep) | `seconds_to_tick` **0**; controls `seconds_to_beat` **1**, `ms_to_tick` **1** |
| rb3-Wii `src/` | only unrelated Wii-SDK `OSSecondsToTicks` macros |
| dc3-decomp `src/` | nothing |

The positive controls matter: they prove the xex scan **can** find such a registration
string, so the 0 is a measurement rather than a broken search.

Independently, naming it has **zero byte upside** — under `name_check` the single
`GamePanel::UpdateNowBar` call site's placeholder target is already forgiven — so the
expected value is ≤ 0 plus the risk of installing an invented name. Left anonymous.

**What would change this:** a DC3/rb3-Wii header or map entry declaring a seconds→tick
free function; a retail string registering it as a DataFunc; or any symbol-bearing build
of this TU. Body evidence cannot settle it — objdiff pairs by name, so a matching body
proves the *function*, never the *spelling*.

## 4. `0x826cca78` — the `TrainerPanel` `$4` adjustor thunk: VERIFIED CORRECT

No repair needed; nothing changed. (`TrainerPanel.cpp` is W16-AW's — read only.)

- Row `?Handle@TrainerPanel@@$4PPPPPPPM@A@AA?AVDataNode@@PAVDataArray@@_N@Z`,
  unit `default/band3/game/TrainerPanel`, 12 B, **fuzzy 100.0 / mpn 100.0**.
- Our `TrainerPanel.obj` **does** emit that `$4` spelling (COFF, post-build).
- The `PPPPPPPM@` decode is consistent with the retail bytes, which is what the brief
  asked. Decoding MSVC's mangled number (`A`…`P` = nibbles 0…15): `PPPPPPPM` =
  `0xFFFFFFFC` = **−4**, and `A@` = **0**. Retail:

  ```
  lwz  r11, -0x4(r4)     ; read the vtordisp at this−4      -> matches PPPPPPPM@ = −4
  subf r4,  r11, r4      ; adjust `this`; no extra addi     -> matches A@ = 0
  b    fn_826CC0B0       ; tail call
  ```
- `0x826CC0B0` is `?Handle@TrainerPanel@@UAA?AVDataNode@@PAVDataArray@@_N@Z`, the
  un-adjusted virtual — exactly what an adjustor thunk must jump to.
- `r4` carries `this` because `r3` is the hidden return slot for the by-value
  `DataNode` return, consistent with the `?AVDataNode@@` in the mangling.

## 5. W16-AV Item 4's "third name" — premise STALE, already settled

The brief asked me to settle the name AR-3 left unidentified. **W16-AV already did**
(commit `54c8ea9d`, landed via merge `df358e10`, present in this lane's base
`ba734115`): `0x824c9878` is
`_M_splice_insert_dispatch<list<EventAnim::EventCall>>` today, measured +1 / +176 B.
Nothing to do. Testing the briefed item literally — as the brief's own ⛔ instructs —
is what caught this.

## 6. Gates (in the brief's order, all in this worktree)

1. Full build `rc=0` — `~/tmp/rb3_build_w16ay_3.log` (SPLIT + renamer 1,838 files /
   86,285 renames + REPORT).
2. `python3 scripts/verify_ruler_agreement.py --check` → **rc=0**,
   "both objdiff-cli entry points resolve the same ruler".
3. `python3 scripts/verify_objs_patched.py --verify-manifest` → **rc=0**,
   1,215 decomp + 3,114 target objects, `tree_sha256=c513729712b2a079`.
4. `python3 tools/icf_alias_finder.py --validate` → **rc=0**, `VALIDATE: PASS — 1,399
   map-consistent, 247 tolerated, 0 contradicted, 1,647 total`. (Run for information:
   this lane did **not** touch `scripts/symbol_aliases.json` — 0 diff lines.)
5. `tools/native_build_gate.sh`, run last:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

`NATIVE GATE: PASS  (rc=0, 0 errors, 0 warnings, 18/18 target(s) verified)`.
Only this docs-only commit follows the gate, per the W16-AT precedent (the gate must
post-date every source change, and the doc must carry the line).

## 7. NOT done, and why

- **No `splits.txt` / `objects.json` edit** — barred (W16-AX). This blocks three
  separately-priced items: `0x827D2500` → `TempoMap.cpp` (**+1 / +12 B**, §1), and the
  `PhraseAnalyzer.cpp` pin (**up to +484 B**, §2a).
- **No `src/` file touched at all.** The brief authorised `SongInfoCopy.cpp`,
  `HamMove.cpp` and `ClipDistMap.cpp`; none needed a change, and §2 says why in each
  case. The lane's whole delta is 3 map rows.
- **`scripts/symbol_aliases.json` untouched** — the `0x8278b7f0` group is mis-oriented
  (§2a), but reversing it is only safe together with the pin, and a membership removed
  without a `withdrawn` record is a clobber.
- **`fn_827C91A0` not named** (§3) — controlled negative on four channels.
- **`0x8278bb98` not renamed** (§2a) — proved wrong, but renaming costs 93.641 → 0 and
  buys nothing until `PhraseAnalyzer.cpp` is pinned. Proving a name wrong is not the
  same as renaming being safe.
- **`ClipDistMap.cpp` not given an instantiation** (§2b) — creating a symbol to buy a
  pairable row is metric fitting.
- No `Co-Authored-By` / AI attribution trailer on any commit, regardless of harness
  reminders.
