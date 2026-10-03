# W16-OS: stale alias survivors relabelled to the map name, every membership re-chased, drift gated at build time (2026-10-03)

Branch `w16os`, started off main `4466f2a76`, rebased onto `076f4f760` (W16-OR merge) with no conflicts.
Ruler `name_check` (graded). No `src/` file changed. No map row changed.

## 1. Result

| | |
|---|---|
| drifted placed groups found (main `4466f2a76`) | **186** of 2,064 |
| relabelled to the applied map name / its `fn_` placeholder | 181 / 5 |
| memberships re-chased against the body at the address | **275** (87 folded, 186 old labels, 1 absorbed) + 2 re-homed |
| PROVEN / REFUTED / UNDECIDABLE | 41 / 125 / 109 (+2 re-homed, PROVEN) |
| memberships withdrawn (with a record) | 25 folded + 37 old labels |
| memberships re-homed to the address they prove at | 2 |
| address-less partition classes absorbed | 2 |
| drift after the relabel | **0** (build edge and `--validate`) |

Whole-binary A/B, rebased branch against main `076f4f760`:

```
python3 tools/ab_measure.py --worktree ~/tmp/wt-w16os-ab2 --patch <git diff 076f4f760..w16os>
run dir ~/tmp/wt-w16os-ab2/.ab_measure_runs/20261003-044323-branch2-2722599, rc=0, kinds [configgen, map]
leg A: matched=53267 masked=25144 honest=28123 code%=57.381924  (recompiles: 0, settled)
leg B: matched=53269 masked=25144 honest=28125 code%=57.384968  (split=1, settle iterations: 2)
Δmatched=+2  Δmasked_equal=+0  Δhonest=+2  Δcode%=+0.003044pp  Δcode_bytes=+312  Δfuzzy=+0.000013pp
units at 100% [mpn]: 524 -> 524   [control none] Δmatched_code=+0 B
```

**The prediction, written before the first run, was Δmatched in [−3, +8], |Δbytes| < 3 kB, and a few rows down
at most.** The first run (pre-rebase, leg A = `4466f2a76`) measured +2 / +2 / +312 B. Before the rebased run I
predicted the same numbers and the same rows. That is what it measured.

### 1.1 Every row that moved, and why

A per-row diff of the two archived leg reports shows 7 changed rows, 0 rows that exist in only one leg, and the
same 7 rows in both runs.

| row | before → after (fuzzy / mpn) | B | membership that moved it |
|---|---|---:|---|
| BandDirector `__uninitialized_fill_n<CameraManager::PropertyFilter>` | 99.79 / 99.79 → **100 / 100** | 96 | `_Param_Construct<PropertyFilter>` PROVEN at `0x82290ed0`, 60 B = 60 B, 0 cycle-assumed. The map name there, `_Copy_Construct<PropertyFilter>`, was missing from the bucket |
| BandDirector `__uninitialized_copy<PropertyFilter>` | 99.79 / 99.79 → **100 / 100** | 96 | same |
| BandCamShot EH funclets `fn_822B60A4`, `fn_822B6764`, `fn_822B70A4` | 99.5 / 100 → **100 / 100** | 3 × 40 | old label `_Destroy<HamCamShot::Target>` PROVEN at `0x822b1cd8` (8 B = 8 B). Our BandCamShot.obj still references that DC3-named spelling, and retail names the address `_Destroy<BandCamShot::Target>` |
| **down:** Waypoint `vector<ObjPtr<SeqInst>>::resize` | 99.84 → 99.69 (both rulers) | 128 | old label `_M_fill_insert<ObjPtr<SeqInst>>` **withdrawn** at `0x823dc368`. CALLEE-TWIN-DISTINCT: retail's body calls `ObjOwnerPtr<Waypoint>`'s copy ctor, ours calls `ObjPtr<SeqInst>`'s, and our `ObjPtr<SeqInst>` copy ctor is PROVEN at its own retail address. Retail holds both bodies, and they differ |
| **down:** FlowCommand `list<DataNode>::_M_create_node` | 99.94 → 99.67 (both rulers) | 72 | old label `_Copy_Construct<DataNode>` **withdrawn** at `0x8251e928`. CALLEE-TWIN-DISTINCT: retail calls `ContentMgr::CallbackFile`'s copy ctor, ours calls `DataNode`'s, which is PROVEN at its own retail address |

+2 fns and +192 B come from the two BandDirector rows; the other +120 B comes from the three funclets, which were
already at mpn 100. **The two down rows were below 100 before and after, so they cost 0 functions and 0 bytes.** Each
was a call site the stale bucket forgave against a function retail does not call there. The alias was hiding a
wrong-callee charge, and the charge is now visible.

The `none` control is flat. On an alias change, a flat `none` with a gain on the graded ruler is also what a
fabricated alias looks like (CLAUDE.md). Here the gain is accounted for row by row, and each row traces to a
membership chased PROVEN on retail bytes.

## 2. The defect

`gen_symbol_alias_map.py` renders each placed group as one objdiff bucket at `address`: `[survivor, *folded]`. The
target objs spell that address with `target_symbol_map.json`'s name. When `survivor` is not that name:

- **The survivor is unmapped** (the address was renamed since): the bucket holds no name retail uses there, so
  every folded member forgives nothing.
- **The survivor is the map name of another address:** retail call sites to *that* address are forgiven against
  this group's folded members, which means forgiveness against the wrong function.
- **Any tool that chases a group by its `survivor` field reads the wrong retail body.** The body is either
  `MISSING(retail)`, which W16-OM's census counted as REFUTED 321 times, or the stale label's body at the other
  address.

The second and third cases happened together at `0x827d5bb0`. W16-NK recorded `_M_allocate_and_copy<StreakInfo>`
there as "CHASED T1 PROVEN". That proof was against the stale label `_M_allocate_and_copy<Vector2>`, whose map body
is at **`0x824f18c8`**. Against the body at `0x827d5bb0` the member is BYTES-DIFFER, and against `0x824f18c8` it is
PROVEN, 100 B = 100 B, 0 cycle-assumed. It is re-homed there.

Nothing compared the two files, so every map rename at a grouped address could leave a stale label behind.
`icf_alias_finder --validate` could not see the problem: its named-member test passes a stale label, because the
target objs never mention that label.

## 3. Method

### 3.1 Population

The population is every placed group whose survivor ≠ the **applied** map name at its address. The applied name
comes from `obj_target_symbol_renamer.load_address_map`, so null rows and `_denylist` rows name nothing. Where the
map names nothing, the survivor must be the address's own `fn_`/`lbl_` placeholder.

- That gives 186 at `4466f2a76`. An independent dict-based classifier reached the same 186 from the raw JSON.
- The 34 groups whose placeholder survivor names its own unnamed address are consistent and were left alone.
- W16-OA's 228 was measured at `d792b486f`; lanes since then relabelled some groups.

### 3.2 Verdicts (`tools/alias_survivor_relabel.py`)

Every membership is chased with `icf_pair_adjudicate.chase` against the body **at the group's address**. A failed
chase is **not** evidence by itself, because our spelling may be a defective port of the right function (the
adjudicator's own rule: "not proven" is not "refuted"). So REFUTED requires positive retail-byte evidence:

| REFUTED kind | rule | n |
|---|---|---:|
| `SLOT-CONTRADICTED:*` | the adjudicator's positive class: retail RTTI or the type descriptor names another class | 24 |
| `BODY:BYTES-DIFFER` | depth-0 byte difference **and an anchor**: another spelling of ours is PROVEN on the same retail body | 34 |
| `CALLEE-OPERATION-DIFFERS` | anchored, and the differing callee is a different operation (`_M_copy_from` vs `_M_initialize_buckets`, `ShowPartyUI` vs `ShowOfferUI`) | 34 |
| `CALLEE-TWIN-DISTINCT` | anchored, same operation, and **our callee is PROVEN at its own retail address**, so retail holds both callee bodies and they differ | 30 |

Everything else is UNDECIDABLE, and its kind is recorded:

| UNDECIDABLE kind | n |
|---|---:|
| `MISSING(ours)` (we compile no body) | 62 |
| `CALLEE-UNANCHORED` | 32 |
| `VACUOUS` | 4 |
| `VACUOUS-PLACEHOLDER-SLOT` | 7 |
| `SLOT-UNDISCHARGED` | 4 |
| `UNANCHORED` | 1 |
| `MISSING(retail)` | 1 |

Hand checks on the largest refuted cluster:

- **`0x825a07e0` (23 folded hashtable members).** The members are *bucket-count* ctors
  `hashtable(size_t, const hash&, …)` (`QAA@IABU…`). The map name is a *copy* ctor `hashtable(const hashtable&)`
  (`QAA@ABV01@@Z`). Retail's body calls `_M_copy_from` at the slot where the members call `_M_initialize_buckets`,
  and our own copy ctor proves on that body. These are different functions.
- **The PlatformMgr cluster.** Each wrapper calls a different PlatformMgr method in retail.

### 3.3 Dispositions

| role | PROVEN | REFUTED | UNDECIDABLE |
|---|---|---|---|
| folded (87) | 31 kept | 25 withdrawn, **2 re-homed** | 30 kept |
| old label, not a map name elsewhere (119) | 10 folded | 37 withdrawn | 37 folded (carried: the map name was already in the bucket), **35 held as a record** |
| old label that is the map name at another address (67) | none | 58 recorded (a withdrawal record) | 9 recorded |
| absorbed from an address-less class (1) | 1 admitted | none | none |

- **No membership was withdrawn without a REFUTED verdict on retail bytes.** UNDECIDABLE folded members all stay.
- **An UNDECIDABLE old label is folded only where the map name was already in the bucket.** There, folding it
  changes nothing. Elsewhere, folding it would switch on forgiveness that nobody proved, so it is *held*: kept as a
  record in `relabelled.held`, not as a member. Examples of held labels are `Init@BandCamShot` vs
  `Init@BandCharacter` and DC3's `ObjRefConcrete<HamCharacter>` dtor; ALIAS-REPAIR had already shown the first of
  these to differ on resolved operands.
- **An old label that is the map's name at another address is never folded here**, because that would put one
  name at two addresses. It is that address's survivor instead.
  - The PlatformMgr dtors, `~ObjPtr<X>` dtors and similar labels formed **permutation chains**: each group's label
    was the next group's map name. They were relabelled together.
- **Re-homes.** A REFUTED member that is PROVEN, with 0 cycle-assumed, at the stale label's own map address moves
  there:
  - `0x827d5bb0` → `0x824f18c8` (`_M_allocate_and_copy<StreakInfo>`, see §2);
  - `0x824a0608` → `0x824a0af0` (`~_List_base<EventTrigger::Anim>`).

  Both target groups are new and carry `admitted` records with the evidence from both addresses.
- **Absorbed classes.** At `0x8228c878` and `0x823a94f0`, ALIAS-REPAIR (08-19) had partitioned a closure into an
  address-less class whose survivor *is* the map name today. These classes were absorbed (W16-OA's pattern), and
  their members were chased PROVEN at the address (`Register@BandCharacter` 52 B = 52 B; `__destroy_range<Strand>`
  80 B = 80 B).
- **Records kept on each group:**
  - `relabelled` (`from`, `to`, `held`);
  - `rechase_w16os` (role, verdict, kind, disposition and evidence for each membership);
  - `withdrawn` records with classes `STALE_SURVIVOR_RECHASE_REFUTED`, `FORMER_SURVIVOR_LABEL_REFUTED`,
    `FORMER_SURVIVOR_LABEL_NOT_A_FOLD` and `REHOMED_FROM_STALE_SURVIVOR_ADDRESS`;
  - `absorbed`, which holds the whole address-less group;
  - `relabelled_history`, where the group already had a `relabelled`.

  Nothing was pruned. Groups: 2,109 → 2,109 (2 absorbed, 2 opened).
- **Denylist accounting**, asserted by the tool:
  - Keys went from 22,057 to 22,301.
  - **0** keys were lost unexplained.
  - **731** keys were lost, and all are survivor-keyed copies of records that stay in the same group, where they are
    now keyed on the new survivor and, as before, on the address.

### 3.4 Stability after the rebase

W16-OR added 14 map rows. On the rebuilt rebased tree:

- drift = 0;
- **all 275 recorded memberships were re-chased: 0 verdicts changed.**

## 4. The gate (`tools/alias_survivor_drift.py`)

There is one predicate, `find_drift(groups, applied)`, and three consumers:

1. **An in-graph build edge**, `CHECK ALIAS SURVIVORS VS MAP` (`tools/project.py`). It is `always` over both JSON
   files, has a content-addressed stamp, and is an implicit input of `report.json` and of `progress`, like the
   injectivity gate. A failure prints the fix: `python3 tools/alias_survivor_relabel.py --write`.
2. **`icf_alias_finder.py --validate`**. Drift is now FATAL and is reported as `SURVIVOR_DRIFTED` inside
   CONTRADICTED.
3. **`tools/test_alias_survivor_drift.py`**, which is registered in `scripts/test_tools.py`.

Proof that the gate can fail:

- **Build edge.** I injected one drifted survivor (`0x82514d48`) and ran `./tools/ninja-locked`. It returned rc=1
  with `FAILED: … alias_survivor_drift_checked.stamp`. After restoring the survivor, it returned rc=0.
- **Test.** Against main's ledger the test reports **5 failed** / 9 passed. On the branch it reports 14 passed.
  (The `rc=0` my shell printed for the main-ledger run was `tail`'s exit code. The pytest summary line is the
  evidence.)
- **Frozen fixtures** (`--selftest`). These include a healthy population, which must stay green. They also include
  each drift shape:
  - an unmapped survivor;
  - another address's name;
  - a stale placeholder;
  - a real name at an unnamed address;
  - a placeholder for another address.

  They also cover the loader's null and `_denylist` handling.
- **Vacuity guards.** An empty population REFUSES (rc 2). The test also asserts that over 90% of placed groups carry
  a real map name, so the equality is actually exercised.

## 5. Side repair: `--chasetest` was red on main

At `4466f2a76`, `icf_pair_adjudicate.py --chasetest` refused with "dead-blr positive … is no longer a dead-blr
pair". W16-ON's `~BandHeadShaper` carve (`731b6885a`) moved dtk's extent at `0x822AFD68` from 32 to 36 B. That made
W16-OM's rule-B positive control flat-identical, and so vacuous. The carve changed only the extent: rule B reads raw
retail bytes, the census and our body, and none of those moved.

The two dead-blr controls now run in a **pre-carve view**: the live tree with that one extent shortened by exactly
the carved `blr`. The view is admitted only when:

- the live body equals ours byte for byte;
- the live body ends in `blr`;
- no relocation lies in that word.

Every other shape still refuses. `--record-fixtures` skips the view id.

Every chase verdict in this lane depends on this instrument. On the rebased, built tree:

| run | result |
|---|---|
| `--chasetest` | rc=0, "selftest PASSED" |
| `--self-break` / `-slots` / `-tailpad` / `-rename` / `-overcarve` / `-vtvac` / `-deadblr` | each rc=0, its own decoy(s) red, no other control moved (slots: 6/6) |
| `--self-break-size eh / tol / onesided / firstdef` | rc=0, exactly 2 / 3 / 1 / 1 size controls red |

## 6. Other gates (rebased tree `076f4f760` + branch, after a full `./tools/ninja-locked`)

- `icf_alias_finder.py --validate`: **PASS**, 1,889 map-consistent / 220 tolerated / **0 contradicted (of which
  SURVIVOR_DRIFTED 0)** / 2,109 groups.
  - The 5 remaining `SURVIVOR_MISLABELED` rows are all address-less partition classes, which render into no bucket.
  - `--selftest` PASS.
- `map_name_injectivity.py`: OK, 35,156 applied rows, injective (+1 enumerated exception).
- `gen_symbol_alias_map.py --check`: OK.
- `scripts/test_tools.py`: **RESULT: PASS** (new=0, timeout=0, broken=0, script-fail=0) on the pre-rebase branch
  and again on the rebased branch.
- Native gate (§8).

## 7. Found, deliberately not done

- **Where the hashtable bucket-count ctors live.** The 23 bucket-count ctors withdrawn from `0x825a07e0` were never
  forgiving anything: they were in a bucket without the retail name. Their real retail home is not located. A
  `locate_retail` pass over that family is a follow-up.
- **Our BandCamShot source still spells `HamCamShot::Target`** (DC3's name), so the funclets pair only through the
  PROVEN alias. Renaming the type is a source clean-up, not alias work.
- **Four folded spellings that the map places at another address** pre-date this lane, are unchanged and are not
  CONTRADICTED by `--validate`:
  - `__uninitialized_copy<CharBones::Bone>` at `0x8237f158`;
  - `??3@YAXPAX0@Z` and `TrigTableTerminate` at `0x826c3888`;
  - `??3RndLight@@SAXPAX@Z` at `0x8240ddb0`.
- **The 35 held old labels and 32 CALLEE-UNANCHORED memberships** are recorded with their trace kind. Deciding them
  needs either a retail address for our callee or a port of it.
- **`_Copy_Construct<MsgSinks::Sink>` at `0x822c9048`** stays a member (UNDECIDABLE). Retail calls
  `ObjPtr<Object>`'s copy ctor where ours calls `ObjOwnerPtr<Object>`'s. That could be a wrong member type in our
  `Sink`. The address is unnamed, so call sites there are already placeholder-forgiven either way.

## 8. Native gate (run last)

See the final line of this section, added after the run.
