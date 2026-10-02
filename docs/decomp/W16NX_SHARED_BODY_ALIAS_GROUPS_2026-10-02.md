# W16-NX: shared-body names as alias groups, and a second `--chasetest` decoy (2026-10-02)

**Branch** `w16-nx`, off main `1bbe30628`, rebased onto `c42c979f8` (W16-NW) before the final A/B. Ruler `name_check` (graded;
`report.json` `provenance.diff_config`). Permuter not run. No compile flag, PCH or shared header touched.
Scratch: `~/tmp/w16nx/`.

The brief had two parts. First, name the seven shared-body addresses W16-NU dropped
(`W16NU_IDENTIFICATION_MAP_DEFECT_TAIL_2026-10-02.md` §4.2). Each becomes an alias group whose folded
spellings are proven one by one with `icf_pair_adjudicate.py --chase` and a retail call-site witness, so a
caller stays forgiven only where the fold is real. Second, give `--chasetest` a second fixture for the class
`0x822e4fd8` anchored, so that row's name can land.

## 1. Whole-branch A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-nx-ab --patch ~/tmp/w16nx/ab_final.patch`
(`git diff main..w16-nx -- . ':!docs'`; kinds map + source + splits; `symbols.txt` untouched). Fresh worktree
at main `1bbe30628`. Run dir `~/tmp/wt-w16-nx-ab/.ab_measure_runs/20261002-193524-ab_final-2447904/`.

```
leg A: matched=51559 masked=24645 honest=26914 code%=55.002730  (recompiles: 0, settled)
leg B: matched=51567 masked=24645 honest=26922 code%=55.009950  (recompiles: 1, split=1, patch_steps=7, settle iterations: 2)
split fixed point: leg A converged after 0 extra re-split(s), leg B after 0
Δmatched=+8  Δmasked_equal=+0  Δhonest=+8  Δcode%=+0.007220pp  Δcode_bytes=+740
units at 100% [mpn]: 490 -> 490
[control none] Δmatched_code=+736 B (NOT_APPLICABLE: patch carries source and splits)
```

**Prediction, written before the run:** +8 / +0 / +8 / +740 B, from the lane's progress read on its own
worktree (diffed row by row against main's `report.json`, which carried the identical leg-A measures).
**Measured exactly.**

**Re-measured after main moved to `c42c979f8`** (W16-NW: StringConversion, Quazal `symbols.txt`
re-carve, disjoint map rows). The rebase was clean. Same A/B worktree advanced to `c42c979f8`, run dir
`~/tmp/wt-w16-nx-ab/.ab_measure_runs/20261002-194121-ab_final2-2485657/`:

```
leg A: matched=51582 masked=24645 honest=26937 code%=55.040930  (recompiles: 0, settled)
leg B: matched=51590 masked=24645 honest=26945 code%=55.048150  (split fixed point both legs)
Δmatched=+8  Δmasked_equal=+0  Δhonest=+8  Δcode_bytes=+740   rows: 2 up to 100, 1 down, 24 renamed keys
```

Prediction (an unchanged delta) **measured exactly**. On the rebased tree `--chasetest` (rc=0),
`--self-break-slots` (rc=0) and `--validate` (PASS, 0 contradicted) were re-run.

**Row level** (archived legs, identical on both runs): 2 rows cross to 100 (the CharBlendBone unwind funclets `fn_823C4394` /
`fn_823C4680`), **1 row goes down** (§5), and 12 keys are renamed. Every renamed key is at fuzzy and mpn 100
in leg B. Three keys at 100 vanish (`HasLesson@LessonMgr`, `_Destroy<ConstraintSystem>`,
`splice<BandCamShot::Target>`). Each was a phantom pairing whose address now carries its true name, at 100.
The LessonMgr "unit regression" (37 → 36) is the `HasLesson` row leaving that unit as `AssetMgr::HasAsset`.

## 2. Measured first: naming alone

The seven names plus `0x822e4fd8`, applied with no alias work: **−49 fns / −11,048 B** (progress read).
This reproduces W16-NU's failed first cut (−52 rows / −11,452 B with its other names).

## 3. The alias groups

A new scanner (`~/tmp/w16nx/landers.py`) runs the other way from W16-NU's `sites.py`. It finds every retail
function that relocates to the named address, diffs it with objdiff, and records the spelling our paired
caller uses at that site. On the named tree that produced **41 (survivor, spelling) pairs**, each run with
`icf_pair_adjudicate.py --chase --size --pairs`:

| address | survivor (map name) | spellings landing | PROVEN | REFUTED |
|---|---|---|---|---|
| `0x822a1520` | `vector<int>::_M_erase` | 33 (4-byte-element vectors) | **33** (flat T1: 1 relocation, names equal) | 0 |
| `0x8256a220` | `AssetMgr::GetAsset` | 3 | **2** (`GetScene`, `GetNameList`) | `GetLesson` |
| `0x82728d08` | `__uninitialized_copy<SampleMarker>` | 1 | **1** (`TypeCreatorPair`) | 0 |
| `0x823dcd38` | `ObjVector<ObjOwnerPtr<Waypoint>>::resize` | 1 | 0 | `resize<Constraint>` |
| `0x823c3828` | `list<ConstraintSystem>::clear` | 1 | 0 | `~ObjPtr<RndTransformable>` |
| `0x824cd9a8` | `list<PresetOverride>::clear` | 1 | 0 | `clear<BandCamShot::Target>` |
| `0x8259f6e0` | `slist<pair<Symbol,String>>::_M_create_node` | 1 | 0 | `create_node<pair<Symbol,float>>` |
| `0x822e4fd8` | `pair<ObjPtr<EventTrigger>>` copy ctor | 0 | n/a | n/a |

Every admitted spelling passed four checks:

- **chase:** PROVEN with 0 CYCLE-ASSUMED.
- **uniqueness:** the survivor is the only retail masked-body twin the spelling chases PROVEN against,
  checked over the full twin set.
- **witness:** a retail `bl <survivor>` decoded in `band.exe` inside a retail caller whose paired site calls
  the spelling.
- **spelling side:** every paired call site of the spelling lands at the survivor (`sites.py`, 36/36).

None was map-resident or in any other group. All five REFUTED pairs are `BYTES-DIFFER`: our callee compiles to a different body than
retail calls there, so those charges are honest, and §4 shows four of the five came from mislabelled caller
rows.

**One level down**, the newly named rows each had one charged callee. Three more spellings were proven the
same way and admitted:

- `_M_find<pair<Symbol,Asset*>>` into the existing `0x82557770` group.
- `_Copy_Construct<pair<const Symbol,String>>` into `0x82743a90`.
- `vector<ObjOwnerPtr<Waypoint>>::resize` into a new group at `0x823dcca0`.

The last spelling was withdrawn from two groups on 2026-08-19 as `FABRICATED_CLOSURE_NOT_PARTITION`, i.e. it
sat in two groups at once. Of its **22** retail masked-body twins it chases PROVEN against exactly one
(`0x823dcca0`), and the single retail call site lands there. A pre-existing charge at `0x82557770`
(`_M_find` with `PatchDir::SymbolHash`) chases `BYTES-DIFFER` and stays charged; ALIAS-2 had already
withdrawn it.

## 4. Map defects the naming exposed

Under a placeholder callee, a caller row pairs on shape alone. Once the callee was named, four rows showed
the caller's own label was wrong. Each was settled on its retail callers:

| address | map said | it is | retail evidence | action |
|---|---|---|---|---|
| `0x8256a268` | `LessonMgr::HasLesson` | `AssetMgr::HasAsset` | body is `bl GetAsset; != 0`; all 8 retail callers are sites where we call `HasAsset@AssetMgr` | renamed; block re-homed LessonMgr → AssetMgr (flanked by AssetMgr on both sides); group `0x8256a268` relabelled |
| `0x823c3ac0` | `_Destroy<ConstraintSystem>` | `~ObjList<ConstraintSystem>` | 4-byte `b clear<ConstraintSystem>`; its 3 callers are CharBlendBone unwind funclets doing `addi r3,r11,8` (`mTargets` at +0x8); our byte-identical paired funclet calls `~ObjList<ConstraintSystem>` | renamed |
| `0x824d0618` | `list<BandCamShot::Target>` splice | `list<WorldDir::PresetOverride>` splice | one caller, `list<PresetOverride>::operator=`; callee frees 0x20-byte nodes with the element dtor at node+8, where our `list<Target>::clear` destroys at node+0x58 and frees 0x6c | renamed; `Shockwave.cpp`'s laneAE helper now emits the PresetOverride COMDAT instead of the refuted Target one; group relabelled |
| `0x823dcfb0` | `ObjVector<Constraint>::operator=` | `ObjVector<ObjOwnerPtr<Waypoint>>::operator=` | one caller, `Waypoint::Copy`; calls `0x823dcd38` and `vector<ObjOwnerPtr<Waypoint>>::operator=` | renamed; single-row block re-homed MeshAnim → Waypoint (flanked by Waypoint on both sides); group relabelled |

Groups `0x8256a268`, `0x824d0618` and `0x823dcfb0` already existed with the wrong label as survivor and the
true spelling folded. That's the shape W16-NU's §3 found, where the group had found the identity but the map
had not followed. Each old label was chased against the new survivor and REFUTED (`BYTES-DIFFER`). It keeps
a `withdrawn` record (class `FORMER_SURVIVOR_LABEL_NOT_A_FOLD`), and the group keeps a `relabelled` record.
Nothing was pruned.

## 5. The row that went down

`default/BandSongMetadata` `slist<pair<const Symbol,float>>::insert_after` (`0x8259f7e0`, 88 B):
100 → 99.77.

Retail `0x8259f7e0` calls `0x8259f6e0`, whose body copy-constructs a **String** value: its one callee is
`_Copy_Construct<pair<void* const,String>>`, and `create_node<pair<Symbol,String>>` chases PROVEN against it
uniquely. Our `insert_after<pair<Symbol,float>>` calls `create_node<pair<Symbol,float>>`, which copies 8
bytes trivially, so the chase is `BYTES-DIFFER`. Retail's callers of `0x8259f7e0` carry map labels for the
`<Symbol,float>` and `<Symbol,Asset*>` hash tables. A String-copying `insert_after` cannot be either, so at
least one label in this Symbol-keyed hashtable insert chain is wrong. The chain was not untangled here (§8).
The charge is a real exposure and was kept.

## 6. `--chasetest`: the second fixture

With `0x822e4fd8` named, `--chasetest` **REFUSED** ("no lax-PROVEN slot decoy for
PLACEHOLDER_SLOT_MAPPED_VS_PLACEHOLDER"), as W16-NU recorded. The class's only live decoy was W16-JG's
record on group `0x822e5040`, and it was lax-PROVEN only because its callee slot was the placeholder
`fn_822E4FD8`. Of the 10 W16-JG records of this class, the other 9 had already drifted out of the decoy
shape (probed one by one: all lax-REFUTED).

A decoy for this class must be **lax-PROVEN and discharge-REFUTED**. A top-level `MAPPED-VS-PLACEHOLDER` is
refused before the slot policy is consulted, so that shape fails under lax as well. The class's working shape
is a tolerated top-level placeholder slot whose discharge chase meets a map-resident callee of ours one level
down. A scan of the live tree (`~/tmp/w16nx/decoyscan.py`) covered every (map-resident retail survivor, our
spelling) pair with equal masked bodies, an equal relocation shape, and at least one retail-placeholder vs
unmapped-ours slot: **70,405 pairs**, of which 50,367 are lax-PROVEN and discharge-REFUTED. **Exactly one**
has `MAPPED-VS-PLACEHOLDER` in its discharge trace:

- survivor `hashtable<pair<Symbol,vector<Symbol>>>::insert_unique_noresize` (`0x8264ed88`)
- ours `hashtable<pair<Symbol,String>>::insert_unique_noresize`
- discharge: retail `fn_8264EC08` vs our `create_node<pair<Symbol,String>>`, map-resident at `0x8259f6e0`
  (named in this lane) ⇒ `MAPPED-VS-PLACEHOLDER`; then `SLOT-CONTRADICTED:CALLEE-LOCATED-ELSEWHERE`. That
  is positive evidence, not merely unproven.

It is recorded as a refused membership in a survivor-only group at `0x8264ed88`. `slot_controls` now also
takes a later lane's record of a class W16-JG defined, tried after W16-JG's own records. Every class that
still has a W16-JG decoy keeps it, and the class set still comes only from W16-JG.

Side observation: naming `0x822e4fd8` also retired the first `CALLEE_LOCATED_ELSEWHERE` decoy
(`_Param_Construct<FlowMathOp>` vs `_Copy_Construct<pair<ObjPtr<EventTrigger>>>`). That class fell back to
another W16-JG record (`0x827fd9c8`) by itself, because it has about 40.

## 7. Gates

On the final code (full `./tools/ninja-locked` after a forced re-split, which leaves the tree a fixed point
of the post-compile passes):

- `icf_pair_adjudicate.py --chasetest`: rc=0, "selftest PASSED". `--self-break`, `--self-break-slots` (all 6
  slot decoys red, no other control moved), `--self-break-tailpad`, `--self-break-rename`,
  `--self-break-overcarve`, and `--self-break-size eh|tol|onesided|firstdef`: all rc=0.
- `icf_alias_finder.py --validate`: **PASS**, 1799 map-consistent / 309 tolerated / **0 contradicted** /
  2109 groups.
- `map_name_injectivity.py`: OK, 33,836 applied rows, injective (+1 enumerated exception).
- `tools/test_alias_group_key.py`, `test_icf_alias_{join_guard,no_ourbuild_gate,survivor_gate,withdrawal_guard}.py`:
  all rc=0.
- `alias_placeholder_slot_audit.py` (dry): all 63 memberships in the six groups this lane added to are
  **CLEAN**.
- No added `src/` line cites another decomp. No commit carries a co-author line.
- `tools/native_build_gate.sh`, run last: see the commit that lands this doc.

## 8. Not done

- The Symbol-keyed hashtable insert chain (`0x8259f7e0` and its callers, §5) was not relabelled. That
  needs the chain's retail callers walked one level further up.
- `fn_824D06B8` / `fn_824D0758` in the Shockwave block are almost certainly the BitmapOverride and
  MatOverride splices (they sit beside the PresetOverride one, and the block is all WorldDir code). They are
  unpaired and were left unnamed.
- No alias was installed without a chase, a uniqueness check and a decoded retail `bl`.
- The permuter was not run, and no compile flag or shared header was touched. The only source edit is
  `rndobj/Shockwave.cpp`.
