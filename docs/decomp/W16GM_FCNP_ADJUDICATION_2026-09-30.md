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

### Batch 2 — 22 memberships RESTORED, then FIVE REVERTED (see §5)

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


**Per-spelling attribution (from the run's archived `legA/legB_report.json.gz`):**
18 rows crossed, 0 fell, 0 `masked_equal`, 0 unattributed — each of the six
spellings measured EXACTLY its phase-1 price (table in §6).

## 5. Batch 2 — 22 memberships, then a negative result

Pre-registered in `W16GM_PREDICTIONS_2026-09-30.md` §"Batch 2" *before* the run.

FORWARD (`--from-dirty`, 22 memberships; run `20260930-013804`):

| | predicted | measured |
|---|---|---|
| Δ`matched_code` | +6,652 B | **+6,652 B** ✅ exact (31 rows crossed, 0 fell, 0 unattributed) |
| Δ`matched_code_percent` | +0.064916 pp | **+0.064915 pp** |
| Δ`matched_functions` | **+31** | **+30** ⛔ MISS |
| Δ`masked_equal_functions` | **+1** | **+0** ⛔ MISS |
| set row `PanelDir::SyncObjects` (336 B) crosses | yes | **yes** — `default/PanelDir` reached 100 % on both rulers (units 196→197 / 173→174) |
| `ALIAS_SUSPECT` | fires | **fired** (`none` Δ+0 B, default +6,652 B) |
| alias map symbol lines | 6,930→6,952 (+22) | **6,952** ✅ |
| `Loaded N ICF equivalence entries` | 6114→**6136** (+22) | **6141** (+27) ⛔ MISS |
| split / recompiles | forced / 0 | `split=1 renamer_patched=1833`, 0 recompiles, both legs settled |

REVERT (`--patch` of the reversed 22-commit diff): **−30 / −6,652 B / masked +0**,
exact mirror; `[control none] FLAT: none UNMOVED and default not up`, `ALIAS_SUSPECT`
silent — the direction asymmetry again.

**The two misses have one cause, and I predicted the wrong side of a shape I had
named.** The 44 B row `fn_827FD814` (`~ObjPtrList<UILabel>`'s second row) was
already `mpn == 100` / `masked_equal == True` at fuzzy 99.5 — GK's T3 shape. Crossing
`fuzzy` therefore moved **bytes only**: +44 B, +0 functions, +0 `masked_equal`. So
Δfunctions is 31 rows − 1 already-counted = **+30**, and `masked_equal_functions`
(which counts only inside `mpn == 100`) was already counting it. The predictions
file flagged exactly this DB-4 two-rulers mechanism as the possible miss mode and
then predicted +1 anyway; recorded as a miss, not adjusted.
**The third miss** (+27 not +22 entries) is bookkeeping I did not model: the five
husk groups went from `folded: []` (emitting nothing) to one member each, and a
*new* equivalence class costs two entries (survivor + member): 17 + 5×2 = 27. The
symbol-line count (+22) was right because it counts folded spellings.

### 5a. The five husk-group memberships are REVERTED (one commit, negative result)

The coordinator's mid-lane rule: any spelling admitted on a group-level argument
rather than its own evidence is split out. Checking the reloc tallies behind the
five `FLAT T1 PROVEN` verdicts (33-pair log) and running the **in-group
discrimination** chase — every remaining FCNP-withdrawn spelling of each touched
group against its survivor, 117 pairs, `~/tmp/w16gm_ingroup_chase.log` — showed:

| group | restored spelling | relocs (of which `tolerated_placeholder`) | slots the chase examined | other FCNP members of the group that ALSO prove |
|---|---|---:|---:|---|
| `0x822a4fb8` | `vector<ObjPtr<RndTex>>::_M_fill_insert` | 2 (**2**) | **0** | **12 / 12** — incl. `vector<GemInProgress>`, a struct element whose retail caller pairs to a *different* `_M_fill_insert` address |
| `0x827fa298` | `~ObjPtrList<UILabel>` | 9 (**5**) | 0 | **14 / 14** |
| `0x822a7f60` | `list<OldMatOption>` copy-ctor | 3 (1) | 0 | **4 / 4** |
| `0x82327878` | `operator<<(list<Layer>)` | 4 (1) | 0 | **1 / 1** |
| `0x823dcde0` | `operator>>(ObjOwnerPtr<Waypoint>)` | 5 (2) | 0 | sole member — untestable |

"FLAT PROVEN strict" there means *masked bytes identical and every relocation
either name-equal or a retail `fn_` placeholder* — and for `_M_fill_insert` **both**
relocations were placeholders, so nothing was name-checked at all and the chase had
no named slot to descend into. With retail keeping 24 / 47 / 11 / 12 body-twins of
those shapes, a comparator that accepts every family member cannot say which twin
our spelling folded into; that is GK's criterion 3 failing outright. **Reverted:
1,080 B / 6 rows**, commit "REVERT the five husk-group restorations". The evidence
stands on record; the unlock is a **map lane naming the `fn_` callees** at those
five bodies, after which the chase can discriminate.

By contrast the `0x823d14c0` evidence discriminates hard, per spelling: **44 of the
58 non-restored FCNP members REFUTE** — including `list<float>::insert`, a 4-byte
element refused because its node copy is `stfs`, not `stw` — while every restored
pointer / `int` / enum member proves with **four named `SLOT-FOLD-OK` frames** and
zero placeholder tolerance. The 14 other members that also prove are all
`list<T*>` with **no charged site anywhere** (price 0); left withdrawn, evidence in
the log, for the same "positive warrant" reason as GK's T3.

## 6. Per-spelling evidence table (every spelling this lane restored, kept or reverted)

Columns: FLAT / CHASED T1 verdicts; `CA/SR/BD` = `CYCLE-ASSUMED` / `SLOT-REFUTED` /
`BYTES-DIFFER` counts on that pair; slots = `SLOT-FOLD-OK` + `VACUOUS-BUT-IDENTICAL`
frames; relocs = `n_relocs` (placeholder-tolerated); map-resident elsewhere = our
spelling has its own row in `target_symbol_map.json` (all: **no**; survivor resident
at the group address only); in-group = the 117-pair discrimination run; pred/meas =
phase-1 price vs the row-attributed contribution in the A/B's leg B.

| batch | group | our spelling | FLAT | CHASED | CA/SR/BD | slots FOK+VBI | relocs (placeholder-tolerated) | map-resident elsewhere | in-group discrimination | pred B/rows | meas B/rows |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | `0x823d14c0` | `W4TrackType@@` | REFUTED | **PROVEN** | 0/0/0 | 4+1 | — (0) | no | 44 of 58 non-restored members REFUTED (incl. list<float>, 4 B); 23/23 restored PROVEN | 1632/1 | 1632/1 |
| 1 | `0x823d14c0` | `PAVRndMat@@` | REFUTED | **PROVEN** | 0/0/0 | 4+1 | — (0) | no | 44 of 58 non-restored members REFUTED (incl. list<float>, 4 B); 23/23 restored PROVEN | 1488/7 | 1488/7 |
| 1 | `0x823d14c0` | `PAVSortNode@@` | REFUTED | **PROVEN** | 0/0/0 | 4+1 | — (0) | no | 44 of 58 non-restored members REFUTED (incl. list<float>, 4 B); 23/23 restored PROVEN | 1108/4 | 1108/4 |
| 1 | `0x823d14c0` | `PAUMerger@FileMerger@@` | REFUTED | **PROVEN** | 0/0/0 | 4+1 | — (0) | no | 44 of 58 non-restored members REFUTED (incl. list<float>, 4 B); 23/23 restored PROVEN | 1056/3 | 1056/3 |
| 1 | `0x823d14c0` | `PAVTriangle@@` | REFUTED | **PROVEN** | 0/0/0 | 4+1 | — (0) | no | 44 of 58 non-restored members REFUTED (incl. list<float>, 4 B); 23/23 restored PROVEN | 1040/1 | 1040/1 |
| 1 | `0x823d14c0` | `PAVRndMesh@@` | REFUTED | **PROVEN** | 0/0/0 | 4+1 | — (0) | no | 44 of 58 non-restored members REFUTED (incl. list<float>, 4 B); 23/23 restored PROVEN | 940/2 | 940/2 |
| 2 | `0x823d14c0` | `PAVRndPollable@@` | REFUTED | **PROVEN** | 0/0/0 | 4+1 | — (0) | no | 44 of 58 non-restored members REFUTED (incl. list<float>, 4 B); 23/23 restored PROVEN | 904/7 | 904/7 |
| 2 | `0x823d14c0` | `PAVPropKeys@@` | REFUTED | **PROVEN** | 0/0/0 | 4+1 | — (0) | no | 44 of 58 non-restored members REFUTED (incl. list<float>, 4 B); 23/23 restored PROVEN | 780/1 | 780/1 |
| 2 | `0x823d14c0` | `PAVVorbisReader@@` | REFUTED | **PROVEN** | 0/0/0 | 4+1 | — (0) | no | 44 of 58 non-restored members REFUTED (incl. list<float>, 4 B); 23/23 restored PROVEN | 508/1 | 508/1 |
| 2 | `0x823d14c0` | `PAVDataArray@@` | REFUTED | **PROVEN** | 0/0/0 | 4+1 | — (0) | no | 44 of 58 non-restored members REFUTED (incl. list<float>, 4 B); 23/23 restored PROVEN | 464/1 | 464/1 |
| 2 | `0x823d14c0` | `PAVUIResource@@` | REFUTED | **PROVEN** | 0/0/0 | 4+1 | — (0) | no | 44 of 58 non-restored members REFUTED (incl. list<float>, 4 B); 23/23 restored PROVEN | 428/1 | 428/1 |
| 2 | `0x823d14c0` | `H` | REFUTED | **PROVEN** | 0/0/0 | 4+1 | — (0) | no | 44 of 58 non-restored members REFUTED (incl. list<float>, 4 B); 23/23 restored PROVEN | 400/3 | 400/3 |
| 2 | `0x823d14c0` | `PAVWaypoint@@` | REFUTED | **PROVEN** | 0/0/0 | 4+1 | — (0) | no | 44 of 58 non-restored members REFUTED (incl. list<float>, 4 B); 23/23 restored PROVEN | 400/1 | 400/1 |
| 2 | `0x823d14c0` | `PAVGamerAwardStatus@@` | REFUTED | **PROVEN** | 0/0/0 | 4+1 | — (0) | no | 44 of 58 non-restored members REFUTED (incl. list<float>, 4 B); 23/23 restored PROVEN | 364/2 | 364/2 |
| 2 | `0x823d14c0` | `PAVRndDir@@` | REFUTED | **PROVEN** | 0/0/0 | 4+1 | — (0) | no | 44 of 58 non-restored members REFUTED (incl. list<float>, 4 B); 23/23 restored PROVEN | 296/1 | 296/1 |
| 2 | `0x823d14c0` | `PAVJob@@` | REFUTED | **PROVEN** | 0/0/0 | 4+1 | — (0) | no | 44 of 58 non-restored members REFUTED (incl. list<float>, 4 B); 23/23 restored PROVEN | 156/1 | 156/1 |
| 2 | `0x823d14c0` | `PAVPassiveMessage@@` | REFUTED | **PROVEN** | 0/0/0 | 4+1 | — (0) | no | 44 of 58 non-restored members REFUTED (incl. list<float>, 4 B); 23/23 restored PROVEN | 132/1 | 132/1 |
| 2 | `0x823d14c0` | `PAVRndGroup@@` | REFUTED | **PROVEN** | 0/0/0 | 4+1 | — (0) | no | 44 of 58 non-restored members REFUTED (incl. list<float>, 4 B); 23/23 restored PROVEN | 124/1 | 124/1 |
| 2 | `0x823d14c0` | `PAVSynthPollable@@` | REFUTED | **PROVEN** | 0/0/0 | 4+1 | — (0) | no | 44 of 58 non-restored members REFUTED (incl. list<float>, 4 B); 23/23 restored PROVEN | 116/1 | 116/1 |
| 2 | `0x823d14c0` | `PAVVoice@@` | REFUTED | **PROVEN** | 0/0/0 | 4+1 | — (0) | no | 44 of 58 non-restored members REFUTED (incl. list<float>, 4 B); 23/23 restored PROVEN | 108/1 | 108/1 |
| 2 | `0x823d14c0` | `PAVCallback@ContentMgr@@` | REFUTED | **PROVEN** | 0/0/0 | 4+1 | — (0) | no | 44 of 58 non-restored members REFUTED (incl. list<float>, 4 B); 23/23 restored PROVEN | 56/1 | 56/1 |
| 2 | `0x823d14c0` | `PAVUIComponent@@` | REFUTED | **PROVEN** | 0/0/0 | 4+1 | — (0) | no | 44 of 58 non-restored members REFUTED (incl. list<float>, 4 B); 23/23 restored PROVEN | 0/0 | 0/0 |
| 2 | `0x823d14c0` | `PAVUITrigger@@` | REFUTED | **PROVEN** | 0/0/0 | 4+1 | — (0) | no | 44 of 58 non-restored members REFUTED (incl. list<float>, 4 B); 23/23 restored PROVEN | 0/0 | 0/0 |
| 2 | `0x822a4fb8` | `?_M_fill_insert@?$vector@V?$ObjPtr@VRndTex@@@@V?$StlNod` | PROVEN | **PROVEN** | 0/0/0 | 0+0 | 2 (2) | no | 12/12 other members PROVEN — NON-discriminating | 368/1 | 368/1 |
| 2 | `0x823dcde0` | `??$?5V?$ObjOwnerPtr@VWaypoint@@@@@@YAAAVBinStream@@AAV0` | PROVEN | **PROVEN** | 0/0/0 | 0+0 | 5 (2) | no | sole FCNP member | 356/1 | 356/1 |
| 2 | `0x82327878` | `VLayer@LayerDir@@` | PROVEN | **PROVEN** | 0/0/0 | 0+0 | 4 (1) | no | 1/1 other PROVEN — NON-discriminating | 152/1 | 152/1 |
| 2 | `0x827fa298` | `??1?$ObjPtrList@VUILabel@@VObjectDir@@@@UAA@XZ` | PROVEN | **PROVEN** | 0/0/0 | 0+0 | 9 (5) | no | 14/14 other PROVEN — NON-discriminating | 120/2 | 120/2 |
| 2 | `0x822a7f60` | `VOldMatOption@@` | PROVEN | **PROVEN** | 0/0/0 | 0+0 | 3 (1) | no | 4/4 other PROVEN — NON-discriminating | 84/1 | 84/1 |

Rows 24–28 (batch 2, non-`0x823d14c0`) are the five **REVERTED** in §5a.

## 7. Final tree state and gates

Full `./tools/ninja-locked` after the revert commit, `report.json` read by key:

| | baseline `8af79551` | **final `w16-gm`** | Δ | predicted Δ (23 memberships) |
|---|---|---|---|---|
| `matched_functions` | 44,154 | **44,197** | **+43** | +43 ✅ |
| `matched_code` | 4,177,988 | **4,190,824** | **+12,836 B** | +12,836 ✅ (7,264 + 5,572) |
| `matched_code_percent` | 40.772522 | **40.897785** | +0.125263 pp | |
| `masked_equal_functions` | 23,323 | **23,323** | **0** ✅ | 0 |
| `total_code` / `total_functions` | 10,247,068 / 69,240 | same | 0 / 0 | |
| ICF alias map symbol lines | 6,924 | **6,947** | +23 | +23 |
| `Loaded N ICF equivalence entries` | 6108 | **6131** | +23 | +23 |
| `icf_alias_finder.py --validate` | PASS 1,408 / 250 / 0 / 1,659 | **PASS 1,408 / 250 / 0 / 1,659** | unchanged | |
| `alias_withdrawal_audit.py` live-and-withdrawn | 4 | **4** | unchanged | |
| group `0x823d14c0` | folded 7 / withdrawn 81 / restored 5 | **folded 30 / withdrawn 58 / restored 28** | | |

`masked_equal_functions` held at Δ0 across batch 1, across the landed set, and
across the final tree; the only row that would have moved it (the 44 B funclet) is
in the reverted five. Both A/B directions for both batches are in §4 / §5.

Native gate: see the `NATIVE_GATE_RESULT` line in the lane's final report (run
last, after this commit).

## 8. What this lane deliberately did NOT do

- No source edits; no map edits. Map-lane filings: (i) the two `EventSink` insert
  bodies `0x82749630` / `0x82767518` + naming `fn_827674A0`; (ii) the `fn_` callees
  at the five husk-group bodies above; (iii) group records `0x822b97c8` and
  `0x824ce130`, whose `address` disagrees with their survivor's map row (508 B).
- No restoration on a chase result alone: `list<MsgSource::EventSink>` declined
  (REFUTED + map-resident), and five memberships that *passed* the chase were
  reverted once the reloc tallies showed the pass was placeholder-shaped.
- No uniqueness probe of the five husk spellings against other retail body-twins
  at other addresses — the in-group result already fails criterion 3, so the probe
  could only have confirmed the decline; budget went to the revert and rebuild.
- The 14 proven-but-priceless `list<T*>::insert` members of `0x823d14c0` are left
  withdrawn (Δ0 today; a future caller would make them live — same rule as the
  `STALE_SPELLING` classes).
- The 122-site / 107-row "FCNP spelling vs non-member retail name" population
  (12,712 B fully clearable) is filed, not adjudicated — new claims, not
  restorations, several pairing element types of different sizes.
- `?PathCompare@@` untouched (commutative operand order; permuter OFF).
