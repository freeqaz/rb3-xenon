# W16-JE — the NAME_ONLY near-miss rows, adjudicated on retail bytes (2026-10-01)

Lane W16-JE, branch `w16-je`, worktree off main `169512b3e`. Not merged.

## 1. What was asked, and the population as measured

The NAME_ONLY rows of `docs/decomp/next-leverage-queue-2026-09-30.tsv` whose only
charge is a relocation **name** (retail calls name A, we call name B) and that are
still below 100 in `report.json`. For each (retail, ours) name pair, decide on retail
bytes whether it is a real ICF fold or a wrong callee / wrong map name. Then either
admit an alias, or fix the source or the map.

Re-selected on this tree, not inherited:

| measure | value |
|---|---|
| queue NAME_ONLY rows still < 100 (fresh build, graded ruler) | **902 rows / 206,640 B** (matches the brief) |
| re-diffed with `objdiff-cli diff --batch` at the graded ruler | 902 / 902 records, **0 fuzzy disagreements** with `report.json` |
| still NAME_ONLY on this tree | 902 / 902 |
| distinct (retail, ours) name pairs | **798** (766 rows carry exactly one pair) |

Scripts (not committed; working files under `~/tmp/w16je/`): `classify.py` (row → pair
census), `chase_all.py` (runs `tools/icf_pair_adjudicate.py`'s `adjudicate` + `chase`
on every pair in one process), `audit_ph.py` / `verify_ph.py` (placeholder-slot audit,
§3), `rtti.py` / `rtti_map.py` (vtable RTTI reader, §4), `cycproof.py` / `cocallee.py`
(the two-channel cycle proof, §5), `rdis.py` (retail disassembler with map names).

Instrument controls, run first on this tree: `icf_pair_adjudicate.py --chasetest`
PASSED (in-family decoy REFUTED, self-pair controls correct, vacuous decoy REFUTED), and
`--self-break` went red as designed.

## 2. Outcome

| | rows | bytes |
|---|---:|---:|
| queue rows open at start | 902 | 206,640 |
| queue rows open at the end | 544 | 118,772 |
| **queue rows taken to 100** | **358** | **87,868** |

Progress reads (one worktree, full build after each phase, graded ruler — NOT the A/B):

| state | matched fns | matched code B |
|---|---:|---:|
| baseline (main 169512b3e) | 47,391 | 4,830,232 |
| A: 272 clean folds admitted | 47,636 | 4,885,580 |
| B1: 2 map rows nulled, 10 phase-A folds withdrawn | 47,668 | 4,892,372 |
| B2: 11 RTTI renames, CharUpperTwist islands re-homed | 47,693 | 4,899,780 |
| C: 32 cycles broken by two channels | 47,744 | 4,915,704 |
| D1: SetMaxDisplay + 2 swapped name pairs | 47,751 | 4,920,468 |
| D2: `_snprintf` call sites | 47,759 | 4,923,120 |

Each phase was also row-diffed against the previous phase's report: **0 rows fell off
100 and 0 rows went down** at every step. The only "vanished" rows were renamed
`ObjPtrList` destructors, which are present at 100 under their new names.

**Whole-branch A/B** (§8): **+368 fns / +368 honest / +92,888 B (+0.906490 pp)**, 29 units
to 100, 0 rows off 100.

## 3. Phase A — the clean folds, and the hole in `--chase` it exposed

273 pairs chased **PROVEN with 0 CYCLE-ASSUMED** and had no conflict: our spelling not
map-resident elsewhere, not in another group, not withdrawn. 272 were installed (117 new
groups) and 1 was refused by the installer (group address ≠ map address). Every membership
carries an `admitted` record with the chase verdict, sizes, retail body-twin count and
the retail call-site addresses (call site = map address of the charged row + offset of
the charged instruction). 30 are vacuous bodies (getters, `li r3,K; blr`, the empty
`blr`); each of those rests on that retail call-site witness, per the house rule for
empty bodies.

⛔ **`--chase` tolerates a placeholder relocation target on its general path** (so that
it stays a strict superset of flat T1). For a constructor or destructor that slot is the
**vtable**, the one field that names the type; for a template wrapper it is an
**unnamed retail callee**. Every tolerated slot of every admitted pair was therefore
re-checked:

| slot class | count | check | result |
|---|---:|---|---|
| data global (`lbl_` vs `?gChunkAllocator…` etc.) | 470 | one retail address per global | all consistent |
| string literal | 52 | retail string contents | all equal |
| vtable | 6 | retail RTTI (COL → type descriptor) vs our `??_7` class | all equal |
| unnamed retail callee (`fn_…`) | 45 + 9 | chase `fn_X` against our callee | 45 PROVEN, **9 REFUTED** |
| vtable (pairs outside the 6) | 2 | RTTI | **2 mismatched** |

**11 pairs failed.** 10 had been installed and were removed (one was the installer's
refusal). Examples:
- `MakeString<int,int,int,int>` vs our `MakeString<const char*,u64,const char*,const char*>`.
  Retail calls one unnamed `FormatString::operator<<` four times, so it is a different
  instantiation: a source defect, not a fold.
- `~ObjPtr<SeqInst>` vs our `~ObjPtr<Sequence>`: the retail vtable's RTTI names `SeqInst`.

⇒ **A `--chase` PROVEN is not sufficient on its own for ctor/dtor or template-wrapper
pairs.** The audit is cheap and should be folded into the admission rule. Tool change
**not made**: the gate is shared, and this lane did not want to re-litigate landed groups.

## 4. Phase B — map defects found while adjudicating

**Nulled + denylisted** (rationale appended to `_denylist_comment`):

- `0x8249b200 OggFree` is a **virtual method**: `lis r11; lwz r10,0xcc(r3); cmpwi;
  lfs; stfs f0,8(r3); bnelr; lwz r11,0(r3); lwz r11,0x24(r11)…`, reached only through two
  `.rdata` vtable slots. Our `OggFree` is the 4-byte `b MemFree` free function. With the
  false row gone, `OggFree` is **restored** to the `0x8240ddb0` `b MemFree` thunk group on
  the positive warrant its CF2 withdrawal asked for: byte+reloc identity, destination
  unique image-wide, and 91 call-site witnesses across 28 vorbis rows (6,112 B).
- `0x822d4278 ??2Task` is `blr` + a zero padding word, an empty function. `??2Task`
  (`li r4,0; b MemAlloc`) is admitted to the `0x827bd2f0` allocator-thunk group, which had
  itself recorded this row as a map defect.

**Renamed by retail RTTI.** A ctor or dtor stores its own class's vtable. Reading the COL
of every vtable a body stores, over all 120 ctor/dtor names in the pair set, found
11 wrong rows:

- `ObjPtrList<T>::~ObjPtrList` is a **permutation**, applied atomically:
  0x8232dc20 CharInterest→Hmx::Object, 0x8232c758 Object→ObjectDir,
  0x8232d8d8 ObjectDir→CharInterest, 0x82282c20 CharBoneOffset→RndMesh,
  0x827fa298 RndMesh→UILabel, 0x8271b710/0x8271b798 MoggClipMap↔SfxInst.
- 0x82803540 `EventDialogDismissMsg` ctor → `UITransitionCompleteMsg` ctor (0 → 100).
- 0x8232a2c0 `~CharUpperTwist` → `~DialogDisplay` (stores both DialogDisplay vtables);
  0x8232a130 `??_ECharUpperTwist$2` → `??_EDialogDisplay$4` (its `b` lands on
  `??_GDialogDisplay`).
- 0x8278ded0 `NUISPEECH::CSTree(CHeap*)` → `GameGemList(int)` (0 → 100). The body chases
  PROVEN against our ctor and sits deep inside GameGemList.cpp's contiguous pins, so the
  survivor is named for the TU that holds it. `CSTree` is **not** added as a member, since
  nothing here proves our CSTree COMDAT is this body.

Two of phase A's "folds" were these wrong names wearing an alias (different polymorphic
classes cannot fold, because their vtables differ). Those memberships were removed.

**Re-homed:** CharUpperTwist.cpp's five `.text` islands inside DialogDisplay.cpp's range
(0x8232A0EC–0x8232A4B0) were circular pins created from the two wrong names. They hold
`~DialogDisplay`, its `??_E` thunk, `~DialogDisplay`'s two EH funclets and an unported
`Save`. DialogDisplay is now one contiguous range; `.pdata` was re-derived by the split.

**Swapped pairs** (phase D1), decided by the placeholder slots that distinguish the twins:

- `OnSynchProc` ↔ `OnFileExecRoot`: 0x82517020 references a data global (our
  `OnFileExecRoot` → `gExecRoot`), while 0x825173a0 references `""` (our `OnSynchProc`).
- `SetVoiceAmount` ↔ `SetVoiceProximityFocus`: 0x82b6eb68 tail-calls
  `PitchCorrectedVoice::SetAmount`.

## 5. Phase C — cycles: `_M_fill_insert<Object*>` and its kin

Every CYCLE-ASSUMED leaf in the pair set is a **self-recursive** function:
`_M_fill_insert_aux` 65, `_Rb_tree::_M_erase` 8, `_M_copy` 8, `__introsort_loop` 7.
The chase can never discharge its self-call slot, so the cycle is broken by the
FOLDPROVE-2 method, **with neither channel consulting the self-recursive slot**:

1. **Masked-body uniqueness of every leaf.** A whole-image census (all 3,120 dtk target
   objects) of retail bodies with the leaf's masked bytes AND relocation shape returns
   exactly one address, the survivor's leaf. So if retail instantiated our leaf, it is
   there. Every non-self slot also chases PROVEN.
2. **A retail witness naming our element type**, in one of two forms:
   - **heterogeneous fan-in**: a retail caller of the survivor chain whose own name carries
     `StlNodeAlloc<T>`. For example, `operator>>`, `LoadStd` and `PropSync` for
     `vector<int>` all call `_M_fill_insert@0x82272a60`, which the map names for `Object*`.
   - **co-callee**: the charged retail caller also calls another T-named function. For
     example, `SongParser::HandleRollEnd` also calls `erase<vector<unsigned int>>`.

Admitted: **30 pairs on fan-in + 2 on co-callee**, each with its leaf membership, all
passing the §3 slot audit. **Restored:**

- `_M_fill_insert<unsigned int>` at 0x82272a60. W17-BPM3 withdrew it as
  CHASE_CYCLE_ASSUMED and named exactly these two channels as the cure.
- `aux<VocalPhrase>` at 0x82781160. ALIAS-2's "396 vs 392 B" premise was the pre-STLPORT-1
  COMDAT-reader artifact; both are 396 B today.

`_M_fill_insert<int>` (14 rows, 5,296 B), the largest pair in the brief's family, is in
the fan-in set.

**Parked, deliberately:** 35 pairs where channel 1 holds but **no retail name witnesses
our element type**. This includes `_M_fill_insert<Mic*>` (2 rows / 1,740 B), the Object*
family's remaining member: retail callers of 0x82272a60 name `int`, `Object*`,
`RndDrawable*`, `RndPollable*`, `LocalSavedSetlist*`, `TourCharLocal*`, but never `Mic*`.
For 4-byte integer-class elements the codegen is identical, so without a type witness
"our member is `vector<Mic*>`" and "retail's was some other pointer vector" cannot be
told apart from bytes. 7 more pairs conflict with an existing map row or group.

## 6. Phase D — source fixes

- **`UIListState::SetMaxDisplay`** (3 UIList rows, incl. `SyncProperty` 2,664 B). Retail
  0x8280e248 is `stw r4,0x18(r3); blr`: the edit-mode clamp is compiled out. It is now
  spelled with the house `LOADMGR_EDITMODE` switch (native keeps the clamp). Row renamed
  from `Voice::SetStartSamp` to `SetMaxDisplay` (the TU that holds it). `Voice::SetStartSamp`,
  byte-identical with no relocation, joins as a fold, witnessed by retail
  `StreamReceiver360::SetSlipOffset` calling 0x8280e248 where we call it.
- **`_snprintf`** (8 rows). Retail calls the CRT `_snprintf` (0x8282d540) where our source
  calls the DC3-era `Hx_snprintf`, which has no retail address (or plain `snprintf` in
  RockCentral). Fixed in TextStream, `LocalizeSeparatedInt(int)`, the retail
  `GrindArray`, and `RockCentral::SyncAvailableSongs`. Native keeps `Hx_snprintf` under
  `HX_NATIVE`.

## 7. Found but NOT fixed (each needs more than a name decision)

| family | rows / B | what retail bytes say | why left |
|---|---|---|---|
| `Keys<T>::Add` / `FindBounds` (KeyLessEq vs KeyGreaterEq) | ~10 / ~5.8 kB | 0x82422c38 is ONE 184-B function with **KeyGreaterEq** semantics (binary search for first `frame >= f`, then walk back over equal frames), mis-carved by dtk into a 176-B row named `KeyLessEq<Color>` + an 8-B tail row named `KeyGreaterEq<Color>`. Its stride is 0x14 (20-B Key, i.e. Color/Quat), so callers mapped as `Keys<Vector3>` (16-B Key) are themselves misnamed. Our `KeyGreaterEq` is 192 B vs retail 184. | needs a `symbols.txt` carve fix (ab_measure refuses symbols.txt patches by design), map renames and a body fix |
| `MakeString<int,int,int,int>` (RndShaderProgram::Cache/CopyErrorShader) | 2 / 2,212 | retail calls one unnamed `FormatString::operator<<` (fn_827C40E8, 124 B) ×4 | none of our 14 `operator<<` overloads matches 124 B (our `int` one is 208 B): a FormatString body divergence |
| `CheckConditionsForSong` | 2 / 2,376 | at the hopos/solo positions retail calls addresses mapped `CheckAwesomes` (128 B) / `CheckDoubleAwesomes`; our `CheckHoposPercent` is 156 B | map identifications look shifted *and* bodies diverge |
| `push_back<pair<VocalPhrase*,VocalPart*>>` vs ours `push_back<ShaderMacro>` | 1 / 3,084 | `__uninitialized_fill_n<pair<int,int>>` vs ours `<ShaderMacro>` BYTES-DIFFER | ShaderMacro copy codegen differs; needs ShaderOptions work |
| `__find<const int*>` vs ours `__find<int*>` | 4 / 2,300 | 176 vs 172 B | retail searches through const iterators; four callers |
| `op40` vs ours `op58` (ByteGrinder::Init) | 1 / 2,084 | bodies differ | op-table identity/order |
| `_Stl_prime<bool>::_S_next_size` (mapped `erase<list<FileCache*>>`) | 6 / 1,240 | 96 vs our 92 B | STLport version difference (already noted by the hash_map lane) |
| `__uninitialized_copy<RangeShift>` vs ours `ObjDirItr<CharClip>::operator++` | 3 / 2,920 | PROVEN but ours is mapped at 0x823d1af0 | needs a dynamic_cast-target (RTTI descriptor) check before any rename |
| `operator>><SongPattern>` vs ours `<MeshAO>` | 1 / 1,120 | §3 audit: unnamed retail callee refutes | source |
| previously-withdrawn memberships (`FABRICATED_CLOSURE_NOT_PARTITION`, `UNDER_PARTITIONED_ICF_CLOSURE`, ...) that chase PROVEN (no cycle) today | 39 pairs / 50 rows / 17,832 B (pair-weighted) | — | each withdrawal needs its own re-adjudication; none was overridden here |

Remaining pairs on the final tree (544 rows / 118,772 B): 250 REFUTED/REFUTED
(60,972 B), 79 UNDECIDABLE/REFUTED (17,700 B), 61+37+7 clean-but-conflicting or
audit-failed, 53 cycles without a type witness.

## 8. Measurement and gates

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-je-ab --patch <git diff main w16-je>`,
fresh worktree at main `169512b3e`, patch kinds map+source+splits (forced re-split on both
legs, both at a `symbols.txt` fixed point after 0 extra splits). Run dir
`~/tmp/wt-w16-je-ab/.ab_measure_runs/20261001-081229-w16je-whole-branch-3352011/`.

| | leg A (main) | leg B (w16-je) | Δ |
|---|---:|---:|---:|
| matched_functions | 47,391 | 47,759 | **+368** |
| masked_equal | 23,822 | 23,822 | +0 |
| honest | 23,569 | 23,937 | **+368** |
| matched_code_percent | 47.137640 | 48.044130 | **+0.906490 pp (+92,888 B)** |
| fuzzy | 56.694084 | 56.697243 | +0.003159 pp |
| units at 100 (mpn) | 270 | 299 | +29, **0 fell off** |
| units at 100 (all-rows-fuzzy) | 232 | 256 | +24, 0 fell off |

`none` control: +196 B (NOT_APPLICABLE as a check -- the patch carries source, so movement
on `none` is expected). The A/B equals the sum of the phase progress reads (+368 / +92,888 B)
exactly.

**Row level, run on the A/B's own archived leg reports:** 407 rows reach fuzzy 100
(+93,056 B), **0 rows go down on either ruler.** The keys that disappear are all renames
or re-homes, and each is present at 100 under its new identity in leg B:
`~ObjPtrList<CharBoneOffset>` (BandCharacter) is now `~ObjPtrList<RndMesh>` at 100;
`~ObjPtrList<RndMesh>` (UIList) is now `~ObjPtrList<UILabel>` at 100; and
`fn_8232A348`/`fn_8232A378` (`~DialogDisplay`'s funclets) moved from CharUpperTwist to
DialogDisplay at 100. That move is also the A/B's one "unit regression"
(CharUpperTwist 23 → 21 matched).

Gates on the final tree:

- `tools/map_name_injectivity.py`: OK (32,159 applied rows, injective).
- `tools/icf_alias_finder.py --validate`: PASS (0 contradicted).
- `scripts/verify_objs_patched.py --verify-manifest`: OK (denylist applied, 7 addresses).
- `tools/native_build_gate.sh`: `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0` (run on the final source state; nothing under `src/` changed after it).

## 9. Deliberately not done

- No change to `tools/icf_pair_adjudicate.py` (the §3 hole is reported, not patched).
- No withdrawal overridden on chase alone. Restorations were made only where the
  withdrawal's own stated cure was supplied (OggFree, `_M_fill_insert<unsigned>`) or its
  premise was measured false (`aux<VocalPhrase>`).
- No `symbols.txt` edits (the `Keys` mis-carve), and no map renames without an RTTI,
  TU-location or retail-callee argument.
- Source comments cite retail bytes only. rb3-Wii was consulted as a hypothesis and is
  not cited as any body's source.
