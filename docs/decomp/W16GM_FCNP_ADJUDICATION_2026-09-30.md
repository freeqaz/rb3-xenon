# W16-GM: adjudicating the priced FCNP spellings on retail bytes

**Worktree `~/tmp/wt-w16-gm`, branch `w16-gm`, forked from main `8af79551`.**
Phase 1 (prices, method, the 738→42 correction): `W16GM_FCNP_SPELLING_PRICES_2026-09-30.md`.
Predictions, written before any measurement: `W16GM_PREDICTIONS_2026-09-30.md`.
Chase log: `~/tmp/w16gm_chase.log`; per-pair verdict table `~/tmp/w16gm_chase_verdicts.json`.

## 1. Instrument gates — run in THIS tree before any verdict

| gate | result |
|---|---|
| `icf_pair_adjudicate.py --selftest` | **PASS** — positive control PROVEN, negative REFUTED, rc=0 |
| `--chasetest` | all controls as specified; **in-family decoy REFUTED** (`SLOT-REFUTED` at `_M_find<Symbol>`), rc=0 |
| `--self-break` | `self-break OK -- the VACUOUS DECOY went RED with the destination proof removed, so the control discriminates.` rc=0 |
| `icf_alias_finder.py --validate` (baseline) | first run **REFUSED (exit 2)**: the reflinked patch manifest still listed main's untracked 0-byte `src/system/os/MasterAudio.cpp`; after a settle build (381 TUs recompiled, every `report.json` measure byte-identical) → **PASS 1,408 map-consistent / 250 tolerated / 0 contradicted / 1,659 total** |
| `alias_withdrawal_audit.py` (baseline) | live-and-withdrawn **4** (3 `SURVIVOR_SIZE_MISMATCH` + 1 FCNP) |

## 2. One chase run, 33 pairs: 29 candidates + 4 in-family decoys

`--pairs ~/tmp/w16gm_chase_pairs.json --chase`, plus `--family` on the
`Object*`/`RndMat*` pair. Labels counted over the **whole** log: `CYCLE-ASSUMED` 0 ·
`SLOT-REFUTED` 4 · `BYTES-DIFFER` 4 · `SLOT-FOLD-OK` 88 · `VACUOUS-BUT-IDENTICAL` 22 ·
`UNDECIDABLE` 0 — and the 4+4 refutation labels all belong to the four decoys.

| class | pairs | FLAT T1 | CHASED T1 | notes |
|---|---:|---|---|---|
| `list<T>::insert` → survivor `list<Hmx::Object*>::insert` @ `0x823d14c0` | 23 | REFUTED (template twin — callee **names** differ, as they must) | **PROVEN**, each with 4 `SLOT-FOLD-OK` + 1 `VACUOUS-BUT-IDENTICAL` | chain `_M_create_node<list<Dep*>>` → `MemOrPoolAlloc`/`STL` → `PoolAlloc` 5-/2-arg → operator-new thunk (full byte equality incl. relocation names). Includes the enum (`TrackType`) and `int` elements. |
| `list<MsgSource::EventSink>::insert` → `list<MsgSinks::EventSink>::insert` @ `0x82767518` | 1 | REFUTED | **REFUTED** — `MAPPED-VS-PLACEHOLDER fn_827674A0` | see §3 decline |
| non-`0x823d14c0` husk groups (`~ObjRefConcrete<RndPartLauncher>`, `vector<ObjPtr<RndTex>>::_M_fill_insert`, `operator>>` `ObjOwnerPtr<Waypoint>`, `operator<<` `list<Layer>`, `~ObjPtrList<UILabel>`, `list<BitmapOverride>::insert`, `list<OldMatOption>` copy-ctor) | 7 | **PROVEN** (strict relocation-name equality, nothing masked) | PROVEN | two of the seven groups carry an `address` field naming a different body than the survivor's map row — see §3 |
| **DECOYS** — `list<BandCharacter::BoneState>`, `list<ScriptTask::Var>`, `list<PanelRef>`, `list<AwardEntry>` → the same `Object*` survivor | 4 | REFUTED | **REFUTED** at the `_M_create_node` slot: `BYTES-DIFFER` + `SLOT-REFUTED` | the discrimination evidence: same 100-byte body, same survivor, struct element ⇒ refused |

`--family`: **53 of our spellings → exactly 1 retail address**, `our_slot0_matches_retail True`,
47 of 48 retail body-twins excluded by the slot-0 discriminator — GK's pigeonhole
reproduces, and the enum / `int` / `unsigned` inserts are inside the 53.

**Map residency (criterion 2), measured for all 29 candidates:** 28 are resident
nowhere in `target_symbol_map.json` while their survivor is resident at the group
address; the one exception is `list<MsgSource::EventSink>::insert`, map-resident
at its own 100-byte body `0x82749630` (the survivor's body at `0x82767518` is also
`0x64`).

## 3. Decisions

### Batch 1 — RESTORED, six commits, one spelling each (all `0x823d14c0`)

| spelling | price | evidence (all in one run) |
|---|---:|---|
| `list<TrackType>::insert` | 1,632 B / 1 row | CHASED PROVEN 0/0/0; not map-resident; decoys refuted. Enum = 4 B `stw` copy ⇒ byte-identical to `list<T*>`; nothing byte-distinguishable could hide behind the alias. |
| `list<RndMat*>::insert` | 1,488 B / 7 rows / 7 units | same; the seven `::Mats(list<RndMat*>&)` overrides — retail's OWN caller names carry `list<RndMat*>` in their mangling, so the element type is not in doubt |
| `list<SortNode*>::insert` | 1,108 B / 4 rows | same |
| `list<FileMerger::Merger*>::insert` | 1,056 B / 3 rows | same |
| `list<Triangle*>::insert` | 1,040 B / 1 row | same |
| `list<RndMesh*>::insert` | 940 B / 2 rows | same |

Criterion 4 (fold vs wrong instantiation): for a pointer / 4-byte element every
instantiation compiles to the same 100 bytes and the same callee chain, so there is
no byte-visible "wrong instantiation" for the alias to hide; the decoys show that a
struct element is *not* forgiven. Records: `restored[]` entries in group
`0x823d14c0`, written by `tools/alias_restore_fcnp_membership.py`, each carrying
the superseded withdrawal record verbatim.

### DECLINED

- **`list<MsgSource::EventSink>::insert` (424 B + 316 B partial)** — CHASED T1
  **REFUTED** (`MAPPED-VS-PLACEHOLDER`: the survivor's `_M_create_node` is the
  unnamed `fn_827674A0`, so the slot cannot be proven) **and** criterion 2 fails:
  the map places our spelling on its own 100-byte body at `0x82749630`, while the
  group's survivor is the *other* class `MsgSinks::EventSink` at `0x82767518`. Two
  live readings — a map mislabel between two same-shaped bodies, or our
  `PropSync<MsgSource::EventSink>` calling the wrong instantiation — with opposite
  remedies (GK's T1 shape exactly). **Filed for a map lane:** adjudicate the two
  `EventSink` insert rows and name `fn_827674A0`.
- **`~ObjRefConcrete<RndPartLauncher>` @ group `0x822b97c8` (392 B)** and
  **`list<WorldDir::BitmapOverride>::insert` @ group `0x824ce130` (116 B)** — both
  FLAT+CHASED PROVEN *by name*, but the group record's `address` names a body
  (`0x822b97c8`, `0x824ce130`) different from where the map places the survivor
  (`0x822b96a8`, `0x822b55e0`). The alias would work by name, but I will not
  install a membership into a group whose own address field contradicts its
  survivor; that is a group-record repair first. Filed.

### Batch 2 — see §5 (decided after batch 1 measured as predicted)

## 4. Batch 1 measurement (`tools/ab_measure.py`, both directions)

_(filled from the run logs; nothing in the prediction file was edited)_

FORWARD (`--from-dirty`, six memberships added; run `20260930-012943`):

| | predicted | measured |
|---|---|---|
| Δ`matched_code` | +7,264 B | **+7,264 B** ✅ exact |
| Δ`matched_functions` | +18 | **+18** ✅ (Δhonest +18) |
| Δ`masked_equal_functions` | 0 | **+0** ✅ |
| Δ`matched_code_percent` | +0.070889 pp | **+0.070888 pp** (rounding) |
| Δ`total_code` / Δ`total_functions` | 0 / 0 | pairable units 1741→1741, `unit net = +18 vs whole-binary +18` ✅ |
| `ALIAS_SUSPECT` fires | yes | **fired**: `default ruler UP (+7264 B) while none is FLAT on a map-only patch` — `[control none] Δmatched_code=+0 B` |
| ICF alias map: symbol lines / `Loaded N` | +6 / 6108→6114 | **6,924→6,930 symbol lines; `Loaded 6114`** ✅ |
| split forced on both legs | yes | leg A settle `split=1`, leg B `split=1 renamer_patched=1833`, **0 recompiles**, both legs settled (2 iterations) |
| leg A reproduces baseline | — | `matched=44154 masked_equal=23323 honest=20831 code%=40.772522` ✅ to the digit |
| units reaching 100 % | 0 (none priced to completion) | **0 / 0 fell off**, both rulers ✅ |
| Δfuzzy aggregate | — | +0.000050 pp |

REVERT (`--patch` of the reversed batch-1 diff on the committed tree):

| | predicted | measured |
|---|---|---|
| Δ`matched_code` / Δ`matched_functions` / Δ`masked_equal` | −7,264 B / −18 / 0 | **−7,264 B / −18 / +0** ✅ exact mirror; leg A = the committed tree (44,172 / 40.843410), leg B = baseline to the digit |
| `ALIAS_SUSPECT` | silent (revert direction) | **silent**: `[control none] FLAT: none UNMOVED and default not up` — the guard's predicate needs the default ruler going UP, so a revert-only measurement would never see it (GK §4's direction trap, reproduced) |
| split / recompiles | forced, 0 | `split=1 renamer_patched=1833`, 0 recompiles, `symbols.txt` at fixed point after 0 extra re-splits on both legs |

Validator after batch 1: **PASS 1,408 / 250 / 0 / 1,659** (unchanged);
`alias_withdrawal_audit.py` live-and-withdrawn **4** (unchanged).

## 5. Batch 2

_TBD_

## 6. What this lane deliberately did NOT do

- No source edits; no map edits (two map-lane filings above).
- No restoration on a chase result alone: `list<MsgSource::EventSink>` was declined
  even though its price (424 B) ranks 12th, because the map contradicts it.
- No membership installed into a group whose `address` disagrees with its
  survivor's map row (two candidates, 508 B).
- The 122-site / 107-row "FCNP spelling vs non-member retail name" population
  (12,712 B fully clearable) is filed, not adjudicated — those are new claims, not
  restorations, and several pair different-sized element types.
- `?PathCompare@@` untouched (commutative operand order; permuter OFF).
