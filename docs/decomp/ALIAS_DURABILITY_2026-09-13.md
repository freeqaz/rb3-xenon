# Making alias withdrawals durable — lane W8-A, 2026-09-13

Branch `w8-alias-durability`, worktree `~/tmp/wt-w8-a`, based on main `2fc2552a`
(`git merge-base --is-ancestor 2fc2552a HEAD` asserted before the first edit).

This is an **integrity** lane, not a byte lever. Nothing here is intended to move
the metric, and the alias set is deliberately left untouched — see §7.

Baseline, after this lane's **first full build** (mandatory: a reflinked
worktree's target objs are pre-renamer, so every mangled-name lookup reads
"absent" until the renamer's pre-compile step has run, and any negative taken
before that is vacuous):

```
./tools/ninja-locked                                  EXIT=0
python3 scripts/verify_objs_patched.py --verify-manifest
  [patch-state] OK: 1205 decomp, 3085 target objects match
  tree_sha256=f2a641a7bb0e8929                        rc=0
```

## 0. Headline

`tools/icf_alias_build.py` now reads the withdrawal ledger as a **denylist**, so
a regeneration can no longer silently re-fabricate an alias a human already
withdrew. Measured on this tree, with a census taken the same hour:

| | groups | live memberships | **withdrawn memberships re-emitted as LIVE** |
|---|---:|---:|---:|
| unguarded regeneration (pre-W8-A behaviour) | 1,002 | 4,884 | **110** |
| guarded regeneration (this lane) | 942 | 4,775 | **0** |

Of the 110: **72** were laundered forward out of the shipped file's own live set
by `--merge`, and **38 were newly re-fabricated by generation**. Fourteen of
them are the `FABRICATED_CLOSURE_NOT_PARTITION` class — the one
`scripts/symbol_aliases.json`'s own `_comment` warns about in as many words.

Three corrections to the brief and to the prior record, all measured before
anything was built on them:

1. **The generator is `tools/icf_alias_build.py`, not `scripts/`.** The briefed
   path does not exist. The `grep -c withdrawn` → 0 claim is **correct on the
   corrected path**.
2. **"563 withdrawals" is the wrong unit and slightly the wrong count.** There
   are **564** groups carrying a `withdrawn` key and **10,058 withdrawal
   records** inside them (10,056 dicts + 2 bare strings), over 10,057 distinct
   `(survivor, spelling)` pairs. Withdrawals are per-MEMBERSHIP, not per-group.
3. **W7-D's prescribed `(address, spelling)` key is not sufficient on its own.**
   51 of the 1,595 groups have `address: null` and would collapse into a single
   `None` bucket. §2 gives the key actually used and why it is a union.

## 1. What was actually broken

`--merge` is additive by construction: it carries hand-verified groups FORWARD
and has no suppression list. Nothing anywhere consulted `withdrawn`. So the
whole adjudication ledger was durable only for as long as nobody re-ran the
generator.

The direction of the failure is what makes it worth a lane. An alias is **pure
forgiveness** under the shipped `name_check` ruler, so a re-fabricated one lifts
`matched_code` **by construction**, with no byte evidence behind it. A
regeneration would therefore not present as a regression somebody chases — it
presents as a **silent gain**, which is the direction this project's standing
directive calls worse than a lower score.

And it is not hypothetical. The artifact says so about itself
(`scripts/symbol_aliases.json`, `_comment` [91]):

> 2026-08-19, lane ALIAS-CONSOLIDATION: 357 groups in 43 clusters had their
> folded lists withdrawn (9,395 memberships). … The generator emitted a closure
> where it owed a partition; **until that is fixed a regeneration grows them
> back.**

★ **But the in-file warning badly overstates the magnitude on this tree, and
that is worth writing down.** Only **14** of those 9,395 memberships actually
regenerate here (§3). The reason is that the census enumerator proposes only
pairs it OBSERVED as name-mismatch charges, and most of the 9,395 were never
observed — they were manufactured at the survivor-keyed UNION step, where a star
of separately adjudicated pairs ships as a clique. So the sentence is
directionally right and ~670× off as a size. A lane funded off "9,395 at risk"
would have been funded off a number nobody had measured.

## 2. The mechanism

`tools/alias_withdrawals.py` (new) loads the ledger; `tools/icf_alias_build.py`
consults it. A membership is denied if it matches **either**

    (survivor, spelling)   or   (address, spelling)

**Both keys are load-bearing, and neither alone is sufficient:**

* `address` alone — W7-D's H1 — misses the **51 groups with `address: null`**,
  which would collapse into one `None` bucket. (None of those 51 carries a
  withdrawal *today*; that is luck, not an invariant, and it is not a property a
  guard should depend on.)
* `survivor` alone misses any withdrawal whose stated remedy was a map repair
  that **renamed the survivor at that address** — exactly the
  `SURVIVOR_SPELLING_CORRECTED_BY_MAP_REPAIR`, `SURVIVOR_MISNAMED` and
  `SURVIVOR_NAME_INHERITED_FROM_MAP` classes. The address outlives the name.

Measured safe against over-blocking: **`survivor` is unique across all 1,595
groups** (1,595 distinct) and non-null `address` is unique across the 1,544 that
have one, so neither key aliases two groups together.

Four further design points, each chosen against a specific failure mode:

1. **The generation gate is deliberately the LAST gate, immediately before
   `ACCEPT`.** Placed first it would also absorb pairs that some other gate would
   have rejected anyway, and the `reject_withdrawn` census bucket would overstate
   what the ledger is holding back. Where it sits, the bucket means exactly
   *"passed every evidence gate, and was stopped only by a human's prior
   adjudication"* — which is the number worth reporting.
2. **The `--merge` carry-forward is gated too**, for a different reason: the
   shipped file already contains 74 memberships that are simultaneously live and
   withdrawn (§4), and without this `--merge` would launder that pre-existing
   inconsistency forward on every run.
3. **`--withdrawals` defaults to the shipped file and is read INDEPENDENTLY of
   `--merge`.** A run without `--merge` must still be protected, or the guard is
   one forgotten flag away from vacuous.
4. **Anti-vacuity: a ledger that is missing, unparseable, or under 5,000 records
   REFUSES the run.** "0 suppressed" is precisely what a vacuous load looks like,
   and a denylist that silently loads nothing is worse than none because it reads
   as a clean run. Same reasoning as `scripts/obj_pairing.py`'s
   `DEFAULT_MIN_DECLARED`.

**Override.** There is deliberately **no blanket allow-all**. An override is
per-membership, in a JSON file, and must NAME the record it overrides: its
`overrides_class` must equal the ledger record's `class`, and it must carry a
non-empty `reason`. A wrong class is refused, so you cannot re-admit a
membership without having read the record that withdrew it. `--no-withdrawal-guard`
exists only to measure the guard's own effect and prints a "do not land this
file" banner on stderr.

## 3. The exposure audit — 110, with its provenance

`tools/alias_withdrawal_audit.py` (new) answers the sizing question. It scores
the ledger against a candidate regeneration, so the number is reproducible
rather than asserted.

```
python3 tools/icf_site_census.py   --out ~/tmp/w8a/sites_fresh.json
python3 tools/icf_fold_evidence.py --out ~/tmp/w8a/evidence_fresh.json
python3 tools/icf_alias_build.py --enumerate census \
    --sites ~/tmp/w8a/sites_fresh.json --evidence ~/tmp/w8a/evidence_fresh.json \
    --merge scripts/symbol_aliases.json --no-withdrawal-guard \
    --out ~/tmp/w8a/red_flag.json
python3 tools/alias_withdrawal_audit.py --candidate ~/tmp/w8a/red_flag.json
```

**Result: 110 withdrawn memberships re-emitted as live** — 72 carried out of the
shipped file's own live set by `--merge`, **38 newly re-fabricated by
generation**:

| withdrawal class | re-emitted |
|---|---:|
| `UNDER_PARTITIONED_ICF_CLOSURE` | 91 |
| `FABRICATED_CLOSURE_NOT_PARTITION` | 14 |
| `SURVIVOR_SIZE_MISMATCH` | 3 |
| `<none>` | 2 |
| **total** | **110** |

⚠ **THE NUMBER IS A PROPERTY OF THE INPUTS, NOT A CONSTANT — do not inherit
it.** The candidate supply is a census snapshot of this tree's objects, so it
moves with the tree, the map, and the freshness of the census. Measured on **one
tree, one hour apart**:

| census | groups | re-emitted | of which newly fabricated |
|---|---:|---:|---:|
| archived 2026-07-31 inputs (`~/tmp/cd9_*.json`) | 928 | **81** | 9 |
| fresh 2026-09-13 census, this tree | 1,002 | **110** | 38 |

The stale inputs understate by 26%, and understate the *newly fabricated* half
by 4×. This is the same rule the campaign applies to `total_code` and to the
reachable ceiling: **re-measure, never inherit**. The provenance for the number
quoted above is: base `2fc2552a`, 1,205 compiled objs / 3,085 target objs,
census and evidence generated 2026-09-13 on this worktree after a full build.

## 4. What the ledger currently holds — and a pre-existing inconsistency

```
groups                 1,595  (564 carry >=1 withdrawal)
withdrawal records    10,058  over 10,057 distinct (survivor, spelling)
```

★ **74 memberships are simultaneously LIVE in a group's `folded` list and NAMED
in a `withdrawn` record at that same group.** This predates the lane and is not
something the guard created:

| class | count |
|---|---:|
| `UNDER_PARTITIONED_ICF_CLOSURE` | 70 |
| `SURVIVOR_SIZE_MISMATCH` | 3 |
| `<none>` | 1 |

**10 of the 74 are also the SURVIVOR of another group** — i.e. the same spelling
is at once a survivor and a folded member elsewhere, which violates
`icf_alias_build.py`'s own stated invariant (*"drop any group whose survivor
collides with another group's folded name"*). The shipped file can hold that
state because the repartition that created it was applied by a different tool,
not by the generator.

I did **not** resolve these (§7). The guard's carry-forward gate stops `--merge`
from laundering them forward into a *future* regeneration, but it does not edit
the committed file, and the committed file's live set is what the build consumes
today. They are filed as handoff H2.

## 5. Proving the guard can fail

A green guard run proves nothing on its own. Both directions were executed on
the real generator, same tree, same inputs.

### 5.1 GREEN — the guard on

```
withdrawal guard: 10058 record(s) from scripts/symbol_aliases.json
...
withdrawal guard: 103 membership(s) SUPPRESSED at generation (would otherwise
have been ACCEPTed), 72 SUPPRESSED at --merge carry-forward, 0 re-admitted by
explicit override
     generation     UNDER_PARTITIONED_ICF_CLOSURE                   84
     generation     FABRICATED_CLOSURE_NOT_PARTITION                14
     generation     SURVIVOR_SIZE_MISMATCH                           3
     generation     <none>                                           2
     carry-forward  UNDER_PARTITIONED_ICF_CLOSURE                   68
     carry-forward  SURVIVOR_SIZE_MISMATCH                           3
     carry-forward  <none>                                           1
```

Audited output: **0** withdrawn memberships live, against 110 unguarded.

### 5.2 RED — the flag opt-out, and a byte-level control on the no-op claim

`--no-withdrawal-guard` restores the old behaviour exactly. The resulting file
is **byte-identical to the pre-change generator's output**:

```
93b5bf17…c03ecc  baseline_fresh.json   (generator BEFORE this lane's commit)
93b5bf17…c03ecc  red_flag.json         (generator AFTER, --no-withdrawal-guard)
6f5ea01f…30b6fb  guarded_fresh.json    (generator AFTER, guard on)
```

That sha equality is the control on the claim "this change is inert when
disabled": the guard is provably the *entire* difference, and the change
introduces no incidental drift.

### 5.3 RED — deliberate code sabotage

`tools/sabotage_withdrawal_guard.py` (new) mutates one load-bearing part of the
guard at a time, re-runs the generator, and requires it to regrow withdrawn
memberships. It asserts each mutation actually changed the file's bytes (the
self-substituting-regex failure mode this campaign has already hit), treats a
missing anchor as a hard error rather than a silent skip, and restores the tree
with `git diff --exit-code` verification on every exit path including exception.

```
=== CONTROL: guard intact ===
  rc=0  withdrawn-live=0  (expect 0)   PASS
```

| defect | what it neuters | result |
|---|---|---|
| `lookup_disabled` | `Ledger.lookup` returns `None` unconditionally | **RED** — withdrawn-live **110** |
| `generation_gate_removed` | the gate before `ACCEPT` | **RED** — withdrawn-live **103** |
| `carry_gate_removed` | `_wd_denied` in the `--merge` path | **RED** — withdrawn-live **55** |
| `vacuity_floor_removed` | the `MIN_RECORDS` floor | **RED** — an empty ledger no longer refuses (`rc=0`, no `REFUSING`) |

`All 4 defects produced a failure`, `EXIT=0`. Note `lookup_disabled` reproduces
**exactly 110**, the unguarded baseline, to the unit — the guard's total effect
and the measured exposure are the same number arrived at two ways.

The 103/55 split is informative rather than incidental: **neither gate alone is
sufficient.** The generation gate carries the newly-fabricated pairs; the
carry-forward gate carries the ones `--merge` would launder out of the shipped
file's own live set. They do not sum to 110 because suppressing at generation
changes the group structure `--merge` then reconciles against.

### 5.4 ⚠ Two probes came back GREEN, and that is a MEASUREMENT, not a broken control

The first version of this control also disabled each denylist key
*individually*, and both came back GREEN. The honest reading is not "rig them
until they go red" — it is that **the two keys are currently REDUNDANT**:

```
of the 110 re-emitted withdrawn memberships:
  caught by BOTH keys      : 110
  caught by survivor ONLY  : 0
  caught by address  ONLY  : 0
```

Every exposed membership is matched by both keys, and `lookup` falls through
from one to the other, so removing either changes nothing. The union is kept
deliberately — each key covers a failure mode the other structurally cannot (51
groups have `address: null`; the map-repair classes rename the survivor at a
fixed address) — but *"kept for a case that has not happened yet"* is the honest
description, and it is **not** currently load-bearing.

They were therefore moved out of `DEFECTS` (which must go RED) into
`REDUNDANCY_PROBES`, which report the overlap instead of asserting it. A control
rewritten until it passes is not a control; a control that reports an
uncomfortable GREEN is doing its job.


## 6. The map-coupled class — 25 records, 0 regenerating today

W7-D observed that for `0x823c8908` regeneration is blocked by the **map fix**,
not by the withdrawal: the differing words sit at unrelocated COMDAT offsets, so
the generator's "modulo relocated fields" masking cannot hide them once the map
row is right. W7-C proved the general shape — an alias defect and a map defect
can be **one error with two symptoms**.

`tools/alias_withdrawal_audit.py --map-coupled` lists the withdrawals whose
stated cause is a map identification rather than a property of the two bodies:
**25 records** across 8 classes. It also reports, per record, whether
`target_symbol_map.json` still names the group's survivor at that address.

| class | records | map row now differs from the group's survivor |
|---|---:|---:|
| `FIXPOINT_ROOT_DIFFERS` | 16 | 14 |
| `SURVIVOR_SPELLING_CORRECTED_BY_MAP_REPAIR` | 2 | 0 |
| `DISTINCT_ADDRESSES_CANNOT_FOLD` | 2 | 1 |
| `MAP_DEFECT_INVERTED_CONCLUSION` | 1 | 1 |
| `SURVIVOR_MISNAMED` | 1 | 1 |
| `SURVIVOR_NAME_INHERITED_FROM_MAP` | 1 | 1 |
| `WRONG_SURVIVOR_IDENTITY` | 1 | 0 |
| `PIGEONHOLE_DISTINCT_RETAIL_ADDRESS` | 1 | 0 |

"Map row differs" is the **healthy** state for this class: it means the map
repair landed and the group's `survivor` field is the stale, refuted name. The
named singletons, all confirmed repaired:

* `0x823c8908` — group survivor `?SetType@CharBlendBone@@`, **map now
  `?SetType@CharTransDraw@@`** (W7-C's re-home; W7-D's withdrawal).
* `0x82738a48` — group survivor `??_DMeterDisplay@@`, **map now `??_DDxMesh@@`**
  (W6-B).
* `0x827b7d88` — group survivor `?HandleProfileLoadComplete@ProfileMgr@@`, **map
  now `?Unload@StoreArtLoaderPanel@@`** (W9-FALSECREDIT).
* `0x827029d8` — group survivor `?SetJump@StandardStream@@`, **map now
  `?UpdateTimeByFiltering@StandardStream@@`** (S3-ABLATE).

★ **Measured: 0 of the 25 map-coupled withdrawals regenerate today.** None of
their classes appears anywhere in the 110. That **corroborates W7-D's claim
empirically** — the map fixes are doing the blocking. The change this lane makes
is that they are no longer doing it *alone*: before today the map row was the
single point of failure for this class, and a map edit that re-introduced the
old name would have silently re-fabricated the alias with nothing to stop it.

Two oddities in the list, reported and **not** acted on:

* `0x824afa78` `DISTINCT_ADDRESSES_CANNOT_FOLD` — the group's `survivor` and the
  withdrawn `spelling` are **the same symbol**
  (`??E?$ObjDirItr@VSpotlightDrawer@@@@QAAAAV0@XZ`), i.e. a group recorded as
  folding a spelling into itself. Degenerate; harmless today because it is
  withdrawn.
* `0x8233c668` carries **8 separate `FIXPOINT_ROOT_DIFFERS` withdrawals** with
  the same survivor — the single densest map-coupled address in the ledger.

Per the brief, **no map rows were touched in this lane.**

## 7. What this lane did NOT do

* **Did not change the alias set.** `scripts/symbol_aliases.json` is untouched —
  `git diff` against `2fc2552a` shows no change to it. No alias was installed,
  withdrawn, or re-admitted. The Δ is therefore **0 by construction**, not by
  measurement, which is the strongest form available here: there is no edit to
  price. `ab_measure --from-dirty` was deliberately **not** run, because a
  changed-file set of `tools/*.py` + `docs/*.md` contains nothing the build
  consumes, and a measured "Δ0" over an empty treatment would be an
  absent-vs-absent reading — the exact shape `ab_measure` exists to refuse.
* **Did not resolve the 74 live-and-withdrawn memberships** (§4), including the
  10 that break the survivor invariant. Each needs individual adjudication (a
  `repartitioned` record may legitimately describe a spelling that moved and a
  live membership that should have gone with it), and doing it in bulk would be
  exactly the unproven bulk alias edit this lane exists to prevent. Handoff H2.
* **Did not fix the star→clique union defect** that manufactures the
  `FABRICATED_CLOSURE_NOT_PARTITION` class in the first place. It is diagnosed
  at its emission site and deliberately unfixed (revert `760cb450`: the only
  available predicate asks the fold question of *our* build, and ICF happened in
  *retail's* link). The guard suppresses the symptom; the cause is untouched.
* **Did not touch any map row**, per the brief — including the four addresses in
  §6 whose map/alias coupling is now documented.
* **Did not re-run the alias validator's contradiction analysis as new work** —
  §8 runs it as a gate only.
* **Did not regenerate `scripts/symbol_aliases.json`.** The file's own
  `_comment` says a regeneration is a regression (it is a fixed point, not a
  derived artifact), and this lane produced its candidates in `~/tmp` only.

## 8. Gates

Full build after `touch config/45410914/config.yml`, then every mandated gate:

```
./tools/ninja-locked                                            EXIT=0
python3 scripts/verify_objs_patched.py --verify-manifest
  [patch-state] OK: 1205 decomp, 3085 target objects match
  tree_sha256=f2a641a7bb0e8929                                  rc=0
python3 tools/icf_alias_finder.py --validate
  VALIDATE: PASS -- 1357 map-consistent, 236 tolerated, 0 CONTRADICTED, 1595 total
                                                                rc=0
python3 -m pytest tools/test_icf_alias_withdrawal_guard.py -q   7 passed
python3 tools/sabotage_withdrawal_guard.py …                    EXIT=0 (4/4 RED)
```

`report.json` after the gate build:

```
matched_functions      42,738
matched_code        3,865,216
matched_code_percent  37.724308
fuzzy_match_percent   49.108723
total_code         10,245,956
total_functions        69,219
```

★ **`tree_sha256=f2a641a7bb0e8929` is byte-identical to the value recorded
before the first edit.** The object tree's content manifest did not move, which
is a stronger Δ0 statement than an A/B: nothing the build consumes changed at
all. `git diff 2fc2552a HEAD` touches five files, all under `tools/`, and
**`scripts/symbol_aliases.json` and `scripts/target_symbol_map.json` are both
untouched.**

## 9. Handoffs

1. **H1 — 38 memberships would be newly re-fabricated the moment anyone
   regenerates, and the guard now stops them, but the CAUSE is untouched.** The
   star→clique union at the survivor key is what manufactures
   `FABRICATED_CLOSURE_NOT_PARTITION`, and it is deliberately unfixed (revert
   `760cb450`). The guard is a denylist over a generator that still owes a
   partition. Anyone who fixes the partition should re-measure §3's number
   afterwards; it should collapse.
2. **H2 — 74 memberships are live AND withdrawn in the shipped file; 10 of them
   break the one-survivor-per-group invariant** (§4). These need per-record
   adjudication, not a bulk edit: a `repartitioned` disposition may legitimately
   describe a spelling that moved with a live membership that should have moved
   too. Note the guard already prevents `--merge` from carrying them forward, so
   a future regeneration would silently DROP these 74 — which is the right
   default for an integrity guard but is a **−74-membership event nobody has
   priced**. Price it before the next regeneration, not during one.
3. **H3 — the two denylist keys are currently redundant** (§5.4). If someone
   wants to simplify, the measurement says either key alone suffices *today*.
   The recommendation is to keep both, because the 51 null-address groups and
   the map-repair rename classes are exactly the cases that would break a single
   key, and neither is hypothetical. But the redundancy should be re-measured
   rather than assumed the next time this is touched.
4. **H4 — `0x824afa78` records a group folding a spelling into ITSELF**
   (`??E?$ObjDirItr@VSpotlightDrawer@@@@QAAAAV0@XZ` as both survivor and
   withdrawn spelling). Harmless while withdrawn; it suggests whatever produced
   that record had a degenerate input. Worth a look by whoever owns the
   `DISTINCT_ADDRESSES_CANNOT_FOLD` sweep.
5. **H5 — `0x8233c668` carries 8 `FIXPOINT_ROOT_DIFFERS` withdrawals** under one
   survivor, the densest map-coupled address in the ledger (§6). A map lane, not
   an alias one.

## 10. The one-line lesson

**An adjudication that lives only in an artifact its generator does not read is
not a decision, it is a comment** — and when the artifact is an alias ledger,
the failure mode is a silent metric *gain*, which is the one direction nobody
audits.
