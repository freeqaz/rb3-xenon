# Lane W3-E — header-AND-map cancelling vtable defects, and L6's uncrossed heads

**Date** 2026-09-11 · **Branch** `w3-vtable-cancel` (worktree `~/tmp/wt-w3-e`, from
main `814e3a60`; rebased onto main `fc680e42` before landing (main moved three times during the lane: `814e3a60` → `3b1c7091` → `fc680e42`)) · **Ruler**
`functionRelocDiffs=name_check` (graded), resolved from `report.json`
`provenance.diff_config`, never hardcoded · every A/B below is
`tools/ab_measure.py --worktree ~/tmp/wt-w3-e --from-dirty`, one change per run,
both legs settled in-run.

## Result in one table

| # | change | kind | pre-registered | measured | commit |
|---|---|---|---|---|---|
| 0 | `tools/vtable_inherit_sweep.py` — the oracle | tool | n/a | control fires on pre-L5 tree, silent on current | `803b52ef`, `3015135b` |
| 1 | `GemPlayer::NoteOn`/`NoteOff` — **map** transposed, header right | map ×3 | +2 / +176 B | **+3 / +204 B** (the +1/+28 B is `DI::NoteOn` pairing in TrainerPanel, which I had wrongly called unpinned) | `687cc9da` |
| 2 | `LocalBandUser` User-vbase table — 5 fold-survivor misnames + 1 misnamed body; header right | map ×6 | +2..+3 / +32 B | **+1 / +32 B** (bytes exact; `mpn` does *not* forgive the hub-name charges I assumed it would) | `2967773e` |
| 3 | `ObjectDir::SetName` — DC3-only virtual forwarder removed from `obj/Dir.h`; retail has no such slot | header | Δ0 on every ruler, 29+ vtables corrected | **Δ0 exactly** (42,443 → 42,443; 37.295963 → 37.295963; fuzzy identical; 956 TUs recompiled) | `a0f82b0d` |
| 4 | `InlineHelp::SetTypeDef` / `LabelShrinkWrapper::SetTypeDef` — DC3-only overrides removed; the map rows named after them re-identified on retail bytes (`Copy`/`PreLoad` thunks, a dtor thunk) + 2 bodies named | header ×2 + cpp ×2 + map ×5 | Δ0 / Δ0 | **−2 / −24 B** — the body naming exposed a THIRD off-by-one chain (row 4b) | `5d9a4aa0` |
| 4b | InlineHelp unit `$4@A@` thunk chain: `PreLoad`→Copy body, `Copy`→dtor, `SyncProperty@RndTransformable`→PreLoad body — three renames | map ×3 | +1 / +12 B (the PreLoad thunk is stuck in MoveAsyncDetector's mis-pin) | **+1 / +12 B exactly** (42,441 → 42,442) | `bf9a9996` |

**Branch total, composed from the five in-run deltas: +3 +1 +0 −2 +1 = +3 matched_functions / +204 +32 +0 −24 +12 = +224 B**, plus the tool. Every delta above is a same-ruler in-run A/B; absolutes are not composed.

**Zero genuine header-AND-map cancelling *transpositions* remain in the
population the oracle can see.** L5's TrackWatcherImpl pair was, on this tree,
the only one; what the new instrument found instead were (a) one-sided map
transpositions hiding inside "WRONG_CALLEE" noise at 99.77 %, (b) fold-survivor
naming defects, and (c) a *different* cancelling class — **DC3-only virtual
overrides** whose our-side thunk symbols the map had been named after, so both
sides agreed on a slot retail does not have.

---

## 1. What each existing tool actually compares (the brief's premise, tested)

The brief said the four tools "compare our header against the map, which agree by
construction" and so could not see the L5 pair. **Tested literally, that is not
the mechanism, and it matters for what to build.**

| tool | retail side | our side | cell of the matrix | could it see TrackWatcherImpl 6/18? |
|---|---|---|---|---|
| `scripts/dump_vtable.py` | — | `??_7X@@6B@` relocs in our COFF | ours only (a dumper) | no — one side |
| `tools/vtable_claim_audit.py` | `.rdata` COL + `.pdata` interiority | — | "is this address a vtable at all" | no — not a slot comparator |
| `tools/vtable_side_by_side.py` | retail `.rdata` slots → map name of the slot **body** | our `??_7` relocs | retail-body-name × ours, **no verdict** | **yes — prints it** (slot 18 `SetAutoplayError` vs `Restart`), and nobody adjudicates 43 rows by hand |
| `tools/vtable_order_sweep.py` | same as side-by-side | same | retail-body-name × ours, with verdict | **held the evidence and withheld it**: TrackWatcherImpl read `UNRESOLVED, covered 1, folded_across 42` |

The reason the sweep withheld it is one line in `icf_fold_safe.fold_counts`:
*"slot address → number of DISTINCT vtables it appears in; count > 1 is folded
ACROSS vtables and is uncomparable."* A base-class method's address appears in
**every derived class's vtable by construction**. TrackWatcherImpl has eight
derived tables, so 42 of its 43 slots read `x8` and were filed as "the ICF-fold
wall" in waves 6–9 (`VTABLE_SLOT_COUNT_FIXES_2026-08-20.md` §12c, §13, §14).
L5's map fix touched only four forwarding-**thunk** names; the slot **bodies**
were named correctly all along, so the comparison the sweep already performs
would have charged slot 18 — had it been allowed to look.

⇒ **The blind spot is INHERITANCE CONFLATED WITH ICF, not header-vs-map.**
Baseline on this tree: **5,144 slots charged, 21,132 excluded as
`folded_across`** — four times the compared population.

## 2. The oracle: `tools/vtable_inherit_sweep.py`

Discriminator: an address `w` held by N retail tables is an ICF fold only if
some holder's class is **outside the RTTI hierarchy of the class that owns the
map name for `w`** (`retail_rtti.bases_of_col`, the decoder the sweep already
uses). If every holder descends from the owner, `w` is one method inherited N
times and its identity is intact. The empty-body hub `0x826c3888` (x1433) is
held by hundreds of unrelated hierarchies and stays incomparable;
`0x827947b0` (`SetAutoplayError@TrackWatcherImpl`, x8) is held only by
TrackWatcherImpl descendants and becomes comparable.

Everything else is the sweep's, reused not re-derived: RTTI-identified tables,
`.pdata`-bounded slot reads, `our_vtable_by_offset` join, the
`AMBIGUOUS_MULTI_VTABLE` refusal, thunk-twin soft marks, and the `Slot` type
that **raises** on a poisoned compare. Two things it adds beyond the
discriminator, both learned from its own false positives (§2b):

* **within-table duplication is a property of the ADDRESS, not of one table** —
  the max multiplicity over every table holding `w`;
* a same-method / different-owner pair (an override ICF-folded onto its base) is
  **withheld** as `same_method_other_owner`, never charged; `??<op>` names with
  a non-virtual access letter are soft-marked.

### 2a. Controls (all executed, not argued)

| control | expectation | measured |
|---|---|---|
| current tree, `--class TrackWatcherImpl` | SAME, slot 18 covered as inherited | **SAME, covered 26 (25 inherited), 17 excluded** (the hub slots) |
| scratch worktree `~/tmp/wt-w3-e-ctl` with `e1ec0647`'s header+map reverse-applied, **fully rebuilt** | slot 18 charged | **SET_DIFFER, `[18] inherited occ=8 retail 0x827947b0 ?SetAutoplayError@TrackWatcherImpl@@ / ours ?Restart@TrackWatcherImpl@@`** |
| a class known right | SAME | 1,144 SAME classes carry ≥1 inherited slot; e.g. `Track` 38 covered / 36 inherited, `GemPlayer` primary table 76 / 70 |
| `--selftest` | 15 checks incl. "the OLD rule would have excluded the inherited slot" and the UIListProvider shape asserted **both** ways (excluded with `within_max`, charged without) | OK |

Slot 6 (`Restart`) is the hub and stays incomparable — a two-slot transposition
leaves one half visible, which is enough.

### 2b. Whole-tree numbers

| | plain sweep | inherit sweep v1 | inherit sweep v2 (shipped) |
|---|---:|---:|---:|
| slots charged on | 5,144 | 12,587 (7,444 inherited) | **12,555 (7,411 inherited)** |
| `folded_across` excluded | 21,132 | 12,642 | 2,881 (+10,209 `folded_within`, relabelled) |
| SAME / SET_DIFFER / PERMUTED / UNRESOLVED | 977 / 1 / 0 / 1,242 | 1,395 / 43 / 2 / 780 | **1,404 / 33 / 0 / 783** |
| classes with charged mismatches | 1 | 54 (49 newly visible) | **41 (36 newly visible)** |

v1 → v2 is the false-positive repair: v1 charged five `UIListProvider`
descendants at slot 16 because the base table holds `0x822ad928` in slots 15
AND 16 (`ComponentStateOverride` and `ElementStateOverride` are both
`return s;`, folded; the map names the survivor `Component…`), and a derived
class that overrides 15 leaves the survivor **once** in its own table. A
per-table `folded_within` check passed it. **My own instrument manufactured a
defect on its first run; the selftest now asserts that shape both ways.**

The 41 charged classes reduce to **12 families** (`~/tmp/w3e_inherit_sweep_v2.json`):

| family | classes | shape | verdict |
|---|---:|---|---|
| `ObjectDir` slot 16: retail `Hmx::Object::SetName` (0x8275a5c0, x584) vs our `ObjectDir::SetName` vtordisp thunk | **29** | we override, retail does not | **HEADER — fixed (§3.3)** |
| `GemPlayer` 23/24 (v1 only) | 2 | pure PERMUTED | **MAP — fixed (§3.1)** |
| `LocalBandUser`/`NullLocalBandUser` 21/23 (v1 only) | 2 | `$4`/`$R4` thunk names | **MAP — fixed (§3.2)** |
| `AppInlineHelp` / `LabelShrinkWrapper` slot 15: retail `UIComponent::SetTypeDef $4` thunk vs our `InlineHelp`/`LabelShrinkWrapper::SetTypeDef` thunk | 2 | we override, retail does not | **HEADER+MAP — fixed (§3.4)** |
| secondary-table slot 0: retail `Load`/`PreLoad`/`Replace`/`Print` `$4` thunk vs ours `Highlight` (`RndDrawable`, `SpotlightEnder`, `CharCollide`, `RndDir`, `WorldReflection`, `MiniLeaderboardDisplay`) | 6 | `covered=1`, UNRESOLVED; the join picked a different secondary table | not adjudicated — thunk-twin / join class, handoff |
| `BandSong` / `RemoteBandUser` slot 0: `??_GSong $4` vs `??_GBandSong`, `??_GRemoteUser $4` vs `??_GRemoteBandUser` | 2 | deleting-dtor thunk naming | map-name class, handoff |
| XAPO (`CXAPOBase::GetRegistrationProperties`) | 2 | vendor | out of scope |

## 3. Candidates adjudicated on retail bytes

### 3.1 `GemPlayer::NoteOn` / `NoteOff` — MAP transposed, header correct (+3 / +204 B)

The only pure PERMUTED class in the tree: slots 23/24 of the BeatMatchSink
sub-object table, inherited into `RealGuitarGemPlayer`. Both rows had sat at
**99.77 %** with one `WRONG_CALLEE` charge each — visible for months as callee
noise.

* `0x826ccb00`: `lwz r11,0(r3); li r7,-1; lwz r3,0x10(r3); li r6,-1; clrlwi r5,r11,24; clrlwi r4,r4,24; b 0x82714730` — four args, two `-1`s, `mVolume` (offset 0) in r5 = `mInstrument->PressNote(note, mVolume, -1, -1)` = **`DirectInstrument::NoteOn`**. The map spelled it `??0VRegTable@XGRAPHICS@@` — a mis-name; it sits contiguously before `NoteOff` (0x826ccb20) and `PlayNote` (0x826ccb30).
* `0x826ccb20`: `lwz r3,0x10(r3); clrlwi r4,r4,24; b 0x827141d8` = `mInstrument->ReleaseNote(note)` = **`NoteOff`** (map right).
* retail slot 23 body `0x826bc180` calls `0x826ccb00` ⇒ it **is** `GemPlayer::NoteOn`; slot 24 body `0x826bc1d8` calls `0x826ccb20` ⇒ `NoteOff`. Our header declares `NoteOn, NoteOff` — matches retail. **Map swapped.**
* Blast radius enumerated by scanning every `bl` in `.text`: each DI address has exactly **one** caller (the GemPlayer body); neither is in `symbol_aliases.json`.

Pre-registered +2 / +176 B; measured **+3 / +204 B** (42,439 → 42,442; 37.293660 → 37.295650 %; 0 regressions). The +1 / +28 B is `?NoteOn@DirectInstrument@@QAAXH@Z` itself pairing at 100 in `default/band3/game/TrainerPanel` — I had claimed DirectInstrument was unpinned from a unit-name grep; it is not. `none` control +28 B (the new pairing); the +176 B is `name_check`-only — the wrong-callee-fix signature, source-backed.

### 3.2 `LocalBandUser` User-vbase table — five fold-survivor misnames, one misnamed body (+1 / +32 B)

Every disputed thunk decoded to its real body:

| slot | retail thunk | lands on | is | map said |
|---|---|---|---|---|
| 21 | `0x8268e468` (12 B `$4`) | `0x8268dd50`: frame, vbase hop, calls `User::Reset` + `_Rb_tree<Symbol…>::clear` | **`LocalBandUser::Reset`** (rb3-Wii: `BandUser::Reset(); LocalUser::Reset(); mShownIntrosSet.clear()`) | `GetLocalBandUser` (ours scored 5.38 % against it) |
| 22 | `0x8268e448` (32 B `$R4`) | `SyncSave@BandUser` | SyncSave thunk | unnamed |
| 23 | `0x8268e3d8` | `0x82533618` = `li r3,1; blr` | **`IsLocal`** `{return true}` | `GetRemoteUser` (returns NULL) |
| 26/27 | `0x8268e378` (one address, two slots) | `0x823591e8` = `li r3,0; blr` | **`GetRemoteUser` const/non-const pair** — a real `GetLocalUser const` thunk would have folded onto 24/25's `0x8268e428` (`addi r3,r3,-0x10; blr`) | `GetLocalUser const` |
| 28 | `0x8268e3f8` (32 B `$R4`, same shape as 22) | `0x823591e8` | `IsNullUser@BandUser` `{false}` | unnamed |

Our `User.h`/`BandUser.h` order is retail's (the MSVC reverse-overload rule
places the const overload first at 24/26, which is what both sides show).
Pre-registered +2..+3 / +32 B; measured **+1 / +32 B** — the `SyncSave` thunk is
the whole delta; the `Reset` body paired at **99.81 on both rulers** (its one
charge is the `clear<>` template spelling and `mpn` does *not* forgive it — my
assumption was wrong); the four `$R4` renames netted exactly 0 as two pairings
moved. `none` control +136 B = 32 + 104: the `Reset` body is
instruction-identical.

⚠ **`IsNullUser` reads 12 B at 0 %** because `config/45410914/symbols.txt:169983`
carries `fn_8268E3F8 … size:0xC` for a 0x20-byte thunk (its twin `fn_8268E448`
is 0x20). I first misread that 0 % as a `$4`-vs-`$R4` declaring-class
difference; the raw bytes refuted it. **Handoff to a splits lane** — this lane
does not edit `symbols.txt`.

### 3.3 `ObjectDir::SetName` — a DC3-only virtual on 29+ vtables (header)

Retail: all 584 Object-derived tables hold `Hmx::Object::SetName` (`0x8275a5c0`)
at Object slot 16, including every ObjectDir descendant; the retail map has
**zero** `SetName@ObjectDir` symbols. rb3-Wii's `Dir.h` declares no override.
DC3's `Dir.h` declares `virtual void SetName(const char*, ObjectDir*) {
Hmx::Object::SetName(name, dir); }` — a pure forwarder — and DC3's own map has 20
symbols for it; ours is a verbatim DC3 copy. ⇒ our ObjectDir-descendant vtables
carried a vtordisp thunk in a slot where retail has the base method: 29 charged
classes, one mechanism. Removed. Behaviour-neutral (the forwarder did nothing),
`.rdata` vtables now match retail's shape. Pre-registered **Δ0 on every ruler**
(full-tree recompile; `Dir.h` is everywhere). Measured **Δ0 exactly**: 42,443 → 42,443 / 37.295963 → 37.295963 / fuzzy 48.960990 → 48.960990, 956 recompiles, 0 units moved (`a0f82b0d`).

### 3.4 `InlineHelp::SetTypeDef` / `LabelShrinkWrapper::SetTypeDef` — DC3-only overrides that were a cancelling pair in miniature (header + map)

Retail slot 15 of `AppInlineHelp` (`0x823258f0`) and `LabelShrinkWrapper`
(`0x8231c7d8`) are `$4` thunks that branch to **`UIComponent::SetTypeDef`
(`0x827fe658`)** — retail does not override. rb3-Wii declares no override on
either class; DC3 does (`InlineHelp.h:44`, `LabelShrinkWrapper.h:21`), and ours
copied it: `{ Hmx::Object::SetTypeDef(d); Update(); }` — which **skips
`UIComponent::SetTypeDef`** (500 B of component setup) in the native port. A
real behavioural divergence.

The metric side is L5's shape at 16 B, and decoding it turned up two more
off-by-one-virtual map rows in the same table. AppInlineHelp's Object-subobject
table (`vt=0x820be6cc`), slot by slot against Object's layout:

| slot | Object virtual | retail | branches to | is | map said |
|---|---|---|---|---|---|
| 9 | `Copy` | `0x82602840` `$4@3` | `0x82313f00` — `__RTDynamicCast`, branch, `UIComponent::Copy` (the `CREATE_COPY`/`COPY_SUPERCLASS` shape) | **`InlineHelp::Copy` thunk** | `SetTypeDef@InlineHelp $4@3` (16 B, "100 %" — target was an unnamed placeholder, forgiven) |
| 15 | `SetTypeDef` | `0x823258f0` `$4@EA@` (x2, shared with PlayerDiffIcon) | `0x827fe658` = `UIComponent::SetTypeDef` | **no InlineHelp override in retail** | (correct) |
| 18 | `PreLoad` | `0x82602800` `$4@3` | `0x823179a8` — three `ReadEndian`, two `>>bool`, the `ActionElement` vector `>>`, a `UIColor` ref load, `UIComponent::PreLoad` | **`InlineHelp::PreLoad` thunk** | `Copy@InlineHelp $4@3` (16 B, "100 %" — same forgiveness) |

And `0x82826e28` (map: `SetTypeDef@LabelShrinkWrapper $4@A@`, 12 B at 98.3 %)
branches to `??_GLabelShrinkWrapper` — it is the scalar deleting-dtor thunk;
our object emits only the `??_E` twin, so it is honestly unpaired after the
rename.

Map edits, all on the bytes above: `0x82602840` → `?Copy@InlineHelp@@$4…@3`,
`0x82602800` → `?PreLoad@InlineHelp@@$4…@3`, `0x82313f00` →
`?Copy@InlineHelp@@UAAX…`, `0x823179a8` → `?PreLoad@InlineHelp@@UAAX…`,
`0x82826e28` → `??_GLabelShrinkWrapper@@$4…`. Injectivity checked (no duplicate
names). Pre-registered **Δ0 / Δ0**: the two AppInlineHelp thunk rows trade one
100 pairing for another (targets now named on both sides), the 98.3 % row was
never matched, and both bodies sit in **foreign pinned ranges** —
`0x82313f00` inside `CrowdAudio.cpp` (`0x82313E28–0x8231433C`) and
`0x823179a8` inside `MoveAsyncDetector.cpp` (`0x823179A4–0x82317AA0`) — so
they cannot pair until a splits lane re-homes them (**handoff**: both ranges
contain `InlineHelp` code). Measured **−2 / −24 B** (42,443 → 42,441): per row,
the AppInlineHelp swap netted 0 and the LabelShrinkWrapper row was unmatched
both ways, exactly as predicted — but two 12-byte `$4@A@` thunks fell from 100
to 98.33: `?PreLoad@InlineHelp@@$4…@A@` (InlineHelp unit) and
`?SyncProperty@RndTransformable@@$4…@A@` (MoveAsyncDetector range). Decoding
them found a **third off-by-one chain** in the InlineHelp unit's own thunk row:

| retail | map said | branches to | is |
|---|---|---|---|
| `0x82316418` | `PreLoad $4@A@` | `0x82313f00` (Copy body) | **Copy** thunk |
| `0x82316428` | `Copy $4@A@` | `0x82316c70` = `??_GInlineHelp` | **deleting-dtor** thunk |
| `0x82317a88` | `SyncProperty@RndTransformable $4@A@` | `0x823179a8` (PreLoad body) | **PreLoad** thunk |

dtor → Copy → PreLoad, each one virtual late, all reading 100 % while their
targets were unnamed placeholders. Naming the bodies removed the forgiveness;
the −24 B is that, not a regression in anything real. Repaired as a separate
measured change (row 4b): pre-registered **+1 / +12 B** — only the Copy thunk
can re-pair; the PreLoad thunk's row lives in MoveAsyncDetector's range whose
base object cannot define it, and our object emits no `??_G…$4` dtor thunk.
Measured **+1 / +12 B exactly** (42,441 → 42,442; 37.295730 → 37.295845; `none` −24 B = the forgiven credit on the two misnamed rows removed) — `bf9a9996`. Net of rows 4 + 4b against the tree before them: **−1 / −12 B**, the 12 B being the PreLoad thunk held hostage by the MoveAsyncDetector mis-pin.

### 3.5 Not edited, with reasons

* **`UIListProvider` slot 16** — my v1 false positive (§2b). Both oracles and
  retail agree on the order; the charge was a cross-table fold artefact.
* **`RndLight` slot 2** (`SetLightType@DxLight` vs `@RndLight`, occ 3) — an
  override folded onto its base; now withheld by the backref-safe
  same-method rule.
* **`String` slot 1** (`??YString@@QAA…` = non-virtual `operator+=(Symbol)`
  sitting in a vtable slot) — a map-spelling defect of the `map_audit` class;
  now soft-marked, not a source question.
* The secondary-table slot-0 and deleting-dtor families in §2b — thunk-twin /
  join noise; listed for a map lane, not adjudicated here.

## 4. L6's uncrossed heads — what was tried, what was not

Row numbers are L6's (`STRUCT_HEADS_2026-09-10.md`). Row 1 (`CustomizePanel::Handle`,
six recorded negatives) was not opened, per brief. Probes ran in the scratch
worktree on full `tools/ninja-locked` builds with `report.json` reads.

| row | function | L6 status | this lane | pre-registered | measured |
|---|---|---|---|---|---|
| 7 | `NgSpotlightDrawer::RenderScene` (588 B), `srawi.` vs `clrrwi.` at [14] | 2 probes negative | **v1** `int numLights = sLights.size(); if (numLights > 0 && …)` | crosses only if MSVC keeps `srawi.`+`beq` for a signed `>0` | **WORSE**: keeps `clrrwi.` and adds `beq→ble` (2 charged vs 1) |
| 7 | 〃 | 〃 | **v2** DC3's structure: `if (numLights == 0) { ClearPostProc(); } else if (…) {…} else { ClearPostProc(); }` | crosses if the two arms tail-merge and the peephole is suppressed | **INERT** (arms merged, still `clrrwi.`; 99.59) |
| 16 | `BandCamShot::Target::operator=` (248 B), first-flag `rlwimi` direction | not probed | **probe A** `unsigned char mTeleport : 1` (first flag only) | +248 B if the first-field direction is a type effect; 0 other rows move | **WORSE**: target inert, `operator>>(BinStream&,Target&)` 1,180 B 100→98.54 and `PropSync<Target>` 1,692 B 100→99.50 regress (unit bytes-at-100 20,480 → 17,608). The readers prove the field is `bool`. |
| 11 | `MetaPerformer::SyncSave` (380 B), `addi r4,r31,0x58` vs `mr r4,r3` | 2 probes WORSE | not re-probed | — | the only untried spellings change callees (`c_str()`) or copy-construct `mSetlistTitle`, which L6's uncast-ternary probe already measured at 83.5; no mechanism identified that yields the recomputed address without a copy |
| 3, 17 | `Find(…, false)`-site scheduling | `false`→`0` inert | not re-probed | — | L6 named it a compiler-internal class keyed on the zero immediate; nothing in the byte evidence disagrees |
| 6, 9 | dead `this` home in an inlined member | 1 inert / not probed | not re-probed | — | documented W37 class, no source lever known |
| 13 | TU-local float pool | documented wall | not re-probed | — | — |

Reading of row 7 after four negatives: r11 is dead after `srawi.` on both sides
(clobbered by `lbz r11` at [16]) and both branch `beq`, so retail simply did not
apply the shift-then-test-zero → mask peephole ours gets under every spelling
tried, including a signed relational. It is not a source-construct question.

## 5. Bugs found, one line each

1. **`icf_fold_safe.fold_counts` conflates inheritance with ICF** — any address in >1 retail table is "folded"; 21,132 slots withheld, 7,411 of them plain inheritance. Fixed by the new tool, not by editing the module (its selftests and callers are unchanged).
2. **`GemPlayer::NoteOn`/`NoteOff` map names transposed**, and `0x826ccb00` (`DI::NoteOn`) misnamed as an XGRAPHICS ctor — the vtable-order sweep's real prize was a map bug that read as two `WRONG_CALLEE` rows.
3. **Five `LocalBandUser` thunk names were fold-survivor spellings**, one of them impossible on its face (`GetLocalUser const` at an address a real one could not occupy).
4. **`ObjectDir::SetName` is DC3-only** — 29+ of our vtables carried a slot retail does not have.
5. **`InlineHelp`/`LabelShrinkWrapper::SetTypeDef` are DC3-only and skip `UIComponent::SetTypeDef`** — a native behavioural bug, held at 100 % by two map rows named after our own wrong symbols.
6. **`symbols.txt` extent defect** `fn_8268E3F8 size:0xC` (true 0x20) — handoff. **Two mis-pins**: `InlineHelp::Copy` (`0x82313f00`) sits in `CrowdAudio.cpp`'s range and `InlineHelp::PreLoad` (`0x823179a8`) in `MoveAsyncDetector.cpp`'s — handoff to a splits lane.
7. **Two AppInlineHelp map rows were each one virtual off** (`Copy`↔`SetTypeDef`, `PreLoad`↔`Copy`) and read 100 % because their branch targets were unnamed placeholders `name_check` forgives.
8. Two of my own: the v1 within-table fold check (§2b), and a "DirectInstrument is unpinned" claim from a unit-name grep (§3.1). Also: the split `.s` files spell addresses in **uppercase hex**; every lowercase grep of mine was a silent vacuous negative until I noticed.

## 6. What I did NOT do

* Did not touch `symbols.txt`, `splits.txt`, or `icf_fold_safe.py`.
* Did not adjudicate the secondary-table slot-0 or deleting-dtor families (§2b) — thunk-twin / join class; a map lane's worklist.
* Did not open `CustomizePanel::Handle` (row 1), or re-probe L6 rows 3/6/9/11/13/17 beyond reading the byte evidence against L6's named mechanism.
* Did not evaluate DC3's `ham_xbox_r.map` as a slot-order oracle beyond provenance greps — retail's own `.rdata`+RTTI turned out to be the independent oracle, and DC3 is newer (it is exactly where the SetName/SetTypeDef divergences come from).
* Did not run the permuter (directive: deferred).

## 7. Landing checklist

* rebased onto main `fc680e42` (clean, 7/7); `python3 tools/icf_alias_finder.py --validate`: **PASS — 1357 map-consistent, 232 tolerated, 0 contradicted, 1591 total** (exit 0); build's `CHECK TARGET OBJS RENAMED`: 25,478 / 28,978 map names present (87.9 %, floor 40 %); map injectivity clean (the two pre-existing duplicates are denylisted, not mine)
* `python3 scripts/verify_objs_patched.py --verify-manifest`: `[patch-state] OK: 1205 decomp, 3088 target objects match` — **exit code 0**
* rebased tree headline (`report.json`, `name_check`): **42,435 / 3,822,584 B / 37.308224 %**; main's ledger at `fc680e42` reads 42,432 / 3,822,360 B / 37.306034 % ⇒ **+3 / +224 B, equal to the composed in-run total** — an independent consistency check, not a measurement
* oracle re-run on the rebased tree: TrackWatcherImpl SAME (26 covered); GemPlayer all three tables SAME (the 23/24 table: 20 covered, 16 inherited)
* native gate (run LAST on the rebased tree, after every `src/` edit; log `~/tmp/rb3_native_gate_w3e.log`):

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

This document was committed AFTER the gate run as a docs-only change (no `src/` content).
