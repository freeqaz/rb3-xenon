# W16-GQ: FCNP-nonmember ICF fold install — 10 of 15 candidate pairs

Lane W16-GQ, worktree `/home/free/tmp/wt-w16-gq`, branch `w16-gq`, started at
`31d347ed5`. Implements goals 1/3/4/6 of the scout report
`/home/free/tmp/w16gq_report_frozen.md` against `scripts/symbol_aliases.json`
only. Final worktree state: `2ca241bf7`, tree clean, no uncommitted changes.

**Bottom line: 10 of 15 candidate pairs installed across 5 group edits (2
existing groups grown, 3 new groups created); 5 declined (2 for a literal
gate-threshold judgment call flagged below, 2 for pre-existing co-withdrawal
contamination, 1 deferred to another lane). Measured whole-binary A/B:
+17 matched functions / +6,220 B / +0.060703 pp, `masked_equal_functions`
unchanged. Reverse leg reproduces the exact negation. Validator PASS, 0
contradicted, before/after. Native gate PASS 18/18.**

## Per-pair table

All counts are from `python3 tools/icf_pair_adjudicate.py --chase` run in this
worktree, logs at `/home/free/tmp/w16gq/pairN_chase.log`. "Disqualifiers" are
the three counters the install gate keys on: `SLOT-REFUTED`, `BYTES-DIFFER`,
`CYCLE-ASSUMED` — the gate is "install only if all three are exactly 0."
"Placeholder relocs" = case-insensitive `placeholder` hits in the chase log,
i.e. whether any part of the proof rests on a placeholder-name tolerance
rather than a real named relocation. FAMILY = pigeonhole line, "N of ours →
M retail address(es)".

| # | verdict | SLOT-REFUTED | BYTES-DIFFER | CYCLE-ASSUMED | placeholder relocs | FAMILY | disposition |
|---|---|---|---|---|---|---|---|
| 2 | CHASED T1: PROVEN | 0 | 0 | 0 | 0 | 20→1 | **installed** — added to `0x822d8cc0` |
| 7 | CHASED T1: PROVEN | 0 | 0 | 0 | 0 | 20→1 | **installed** — added to `0x822d8cc0` |
| 8 | CHASED T1: PROVEN | 0 | 0 | 0 | 0 | 20→1 | **installed** — added to `0x822d8cc0` |
| 13 | CHASED T1: PROVEN | 0 | 0 | 0 | 0 | 20→1 | **installed** — added to `0x822d8cc0` |
| 9 | CHASED T1: PROVEN | 0 | 0 | 0 | 0 | 2→1 | **installed** — added to `0x82b9b590`; disassembly-confirmed |
| 4 | CHASED T1: PROVEN | 0 | 0 | 0 | 0 | 18→1 | **installed** — new group `0x823d6fa8` |
| 6 | CHASED T1: PROVEN | 0 | 0 | 0 | 0 | 18→1 | **installed** — new group `0x823d6fa8` |
| 15 | CHASED T1: PROVEN | 0 | 0 | 0 | 0 | 18→1 | **installed** — new group `0x823d6fa8` |
| 5 | CHASED T1: PROVEN | 0 | 0 | 0 | 0 | 1→1 | **installed** — new group `0x827a37b0`; disassembly-confirmed |
| 10 | CHASED T1: PROVEN | 0 | 0 | 0 | 0 | 26→1 | **installed** — new group `0x82362b20` |
| 1 | CHASED T1: PROVEN | 0 | 0 | **1** | 0 | 9→1 | **declined** — see judgment call below |
| 11 | CHASED T1: PROVEN | 0 | 0 | **1** | 0 | 9→1 | **declined** — see judgment call below |
| 3 | n/a (not re-chased) | — | — | — | — | — | **declined** — co-withdrawal contamination (86-member `FABRICATED_CLOSURE_NOT_PARTITION` roster, recurs across 10 group addresses) |
| 12 | n/a (not re-chased) | — | — | — | — | — | **declined** — co-withdrawal contamination (`ObjRefConcrete<T>` equivalence class), despite being the strongest-looking FLAT-T1-PROVEN candidate and the sole FAMILY mismatch |
| 14 | n/a (not re-chased) | — | — | — | — | — | **deferred** — survivor address `0x827e16a0` sits in a range another lane is currently relabelling; scout report recommended ADMIT, this is a scheduling deferral, not a technical decline |

Zero of the 10 installed pairs' proofs rest on placeholder-name tolerance —
every relocation consulted in every chase log resolved to a real mangled
name on both sides.

### Judgment call flagged for reviewer: pairs #1, #11

Both show exactly 1 `CYCLE-ASSUMED` entry in an otherwise-clean chase log (12
`SLOT-FOLD-OK`, 0 `SLOT-REFUTED`, 0 `BYTES-DIFFER`). Per the task's literal
"zero on all three counts" install gate, I **declined** both — so the planned
6th group at `0x827eb0b8` (`_M_fill_insert<vector<Note@MidiParser>>` survivor,
folding `_M_fill_insert<vector<GemInProgress>>` and
`_M_fill_insert<vector<Key<Vector2>>>`) is **not created this wave**.

I flag this because the literal reading may be overly conservative:
`chase()`'s own docstring documents `CYCLE-ASSUMED` as a legitimate,
non-silent, coinductively-justified mechanism (accepting a self-referential
proof step under the standard coinduction argument, not a hidden gap), and the
same underlying relationship is independently and fully corroborated
elsewhere in the very same log via `SLOT-FOLD-OK` with no cycle needed. So the
one `CYCLE-ASSUMED` looks like a proof-search artifact rather than a real gap
in coverage. I still declined both, on the theory that "zero on all three
counts" was written as a bright-line rule precisely so implementers don't
make exactly this kind of judgment call — but the reviewer should decide
whether `0x827eb0b8` should be created after all.

### Disassembly confirmation: pairs #5, #9

Per the scout report's caution (top-level template name mismatch: pair #5 is
`__destroy_mv_srcs<T>` survivor folding a `__destroy_range<T>` spelling; pair
#9 is `_Param_Construct<T>` survivor folding a `_Copy_Construct<T>` spelling —
different STLport helper template names, which is a plausible ICF-fold shape
but was flagged for manual verification rather than trusting the chase
adjudicator alone), both bodies were pulled from `orig/45410914/band.exe` and
from our own compiled `.obj`s (via `tools/va_disasm.py` and
`/home/free/tmp/w16gq/disasm_ours.py`, the latter reusing
`icf_alias_build.function_bodies_ext` to extract raw unmasked bytes and
`capstone` to disassemble) and compared instruction-by-instruction. Both
pairs: **instruction-identical except for one relocated `bl` target each** —
exactly the ICF-fold signature (same generated code, different callee
address baked in by which instantiation the linker kept). No further
concerns.

### Pairs #3, #12: declined for co-withdrawal contamination

Not re-run through `--chase` this wave (scout report already identified the
disqualifying issue independently of FLAT/CHASE): both spellings appear in
rosters that a prior lane (2026-08-19 consolidation) explicitly withdrew as
`FABRICATED_CLOSURE_NOT_PARTITION` (#3, recurring across 10 group addresses)
or as part of the `ObjRefConcrete<T>` equivalence-class withdrawal (#12).
Per the scout report's explicit instruction, `tools/alias_restore_fcnp_membership.py`
was **not used** on either — it restores a single spelling's own FCNP record
into the group it names, not a two-spelling pairing check, and using it here
would silently reopen a pairing a prior lane rejected for a documented reason
rather than adjudicating it fresh.

## A/B measurement — both directions

Forward leg: cumulative diff of all 5 group-edit commits
(`7f8e6f6d5`..`2ca241bf7`) applied via `tools/ab_measure.py --patch`.
Reverse leg: `git diff HEAD 9155fbc0e -- scripts/symbol_aliases.json` (the
exact reverse of the forward diff, verified `git apply --check` clean)
applied the same way. Both runs confirmed the tool forced a full re-split on
leg B (`split=1`, renamer patched >1000 files each direction) and settled to
a fixed point (0 extra re-splits, both legs, both directions).

**Forward:**
```
Δmatched=+17  Δmasked_equal=+0  Δhonest=+17  Δcode%=+0.060703pp  Δcode_bytes=+6220
Δfuzzy=+0.000061pp
13 unit(s) improved, sum +17 (mirror image of the reverse-leg regression list below)
units at 100% [mpn ruler]: 198 -> 198 (Δ+0)
[control none] ALIAS_SUSPECT shape: default ruler +6220 B, none ruler +0 B
```

**Reverse** (`~/tmp/w16gq_ab_reverse.log`):
```
leg A: matched=44218 masked=23323 honest=20895 code%=41.074657
leg B: matched=44201 masked=23323 honest=20878 code%=41.013954
Δmatched=-17  Δmasked_equal=+0  Δhonest=-17  Δcode%=-0.060703pp  Δcode_bytes=-6220
Δfuzzy=-0.000061pp
unit REGRESSIONS: 13 unit(s), sum -17
    -3  default/UIListWidget  (71->68)
    -2  default/GemManager  (128->126)
    -2  default/SongStatusMgr  (80->78)
    -1  default/Award, BandCrowdMeter, DataArraySongInfo, GemTrackDir, Msg,
        SongInfoCopy, UIListSlot, UIScreen, UISlider, system/ui/UILabelDir
units at 100% [mpn ruler]: 198 -> 198 (Δ+0; 0 reached, 0 fell off)
units at 100% [all-rows-fuzzy ruler]: 174 -> 174 (Δ+0; 0 reached, 0 fell off)
[control none] Δmatched_code=+0 B Δcode%=+0.000000 (default ruler -6220 B)
[control none] FLAT: 'none' UNMOVED and default not up -- consistent with a
  pure RE-name (reloc_eq makes renaming free on both rulers).
[tree] restored to the pre-run state (1 path(s), verified by re-reading the
  diff AND the untracked set)
```

The reverse leg is the **exact arithmetic negation** of the forward leg on
every reported metric, including all 13 per-unit deltas — internal-consistency
evidence that settle/split-to-fixed-point/report generation behaved
symmetrically in both directions with no order-dependent artifact.
`masked_equal_functions` is unchanged (23323) in both legs, both directions,
confirming the pre-registered prediction that this wave changes only
*charged* relocation-name comparisons, not the folded-body-signature pairing
count.

**On the `[control none] ALIAS_SUSPECT` wording asymmetry**: the forward leg
prints the stronger `ALIAS_SUSPECT` / "FABRICATED-ALIAS shape" wording; the
reverse leg prints the milder `FLAT` wording. The underlying shape is the
exact mirror image in both cases (`none` ruler flat, default ruler moves).
This is the **structurally expected shape for any legitimate pure
name-forgiveness wave** — `none` never charges relocation names to begin
with, so it cannot move regardless of whether the alias is genuine or
fabricated. Per CLAUDE.md's own map-economics findings, this shape is
genuinely ambiguous between "fold-alias" and "wrong-callee-fix" from the
`none`-ruler evidence alone and requires retail-byte adjudication to clear.
That adjudication is exactly what the CHASED T1 PROVEN chase-log analysis
above (0/0/0 on all three disqualifiers, 0 placeholder reliance, across all
10 installed pairs) plus the manual disassembly for #5/#9 already constitute
— so I read the shape alert as fired-and-cleared, not as an open concern, but
flag the raw wording asymmetry for whoever tunes that alert's message
selection logic next, since it is currently inconsistent between forward and
reverse legs of what is definitionally the same underlying change.

## Predicted vs measured

Pre-registered in `docs/decomp/W16GQ_PREDICTIONS_2026-09-30.md` (commit
`9155fbc0e`, before any measurement was run):

| metric | predicted | measured | note |
|---|---|---|---|
| `matched_code` (Δbytes) | up to +5,452 | **+6,220** | miss in the opposite direction than expected — predicted an undershoot, got an overshoot |
| `matched_functions` (Δ) | up to +35 | **+17** | roughly half; direction of miss ("likely overestimate") was correct |
| `masked_equal_functions` (Δ) | 0 | **0** | confirmed exactly, both directions |
| forced re-split both legs | yes | **yes** | leg B always `split=1`, renamer >1000 files patched each time |
| reverse leg ≈ negation of forward | yes | **yes, exactly** | every metric, including all 13 per-unit deltas |
| validator PASS / 0 contradicted, before+after | yes | **yes** | see below |
| native gate PASS 18/18 | yes | **yes** | see below |

## Validator before/after

`python3 tools/icf_alias_finder.py --validate`, run against this worktree:

- **Before** (pre-wave baseline, per inherited prior-session notes):
  PASS, 1408 map-consistent, 251 tolerated, 0 contradicted, 1660 total.
- **After** (final state, `2ca241bf7`, re-confirmed this session):
  ```
  VALIDATE: PASS -- 1411 map-consistent, 251 tolerated (enumerated above),
  0 contradicted, 1663 total
  ```
  Tolerated sub-classes unchanged in every count (34 PLACEHOLDER_SURVIVOR, 89
  STALE_SPELLING, 27 SURVIVOR_MISLABELED, 101 UNWITNESSED); 1
  CONTRADICTION_EXEMPT unchanged (pre-existing, unrelated to this wave — the
  `??3BinStream@@` HMX operator-delete family). Map-consistent count moved
  exactly +3 (1408→1411), matching the 3 newly-created groups
  (`0x823d6fa8`, `0x827a37b0`, `0x82362b20`); the two grown existing groups
  (`0x822d8cc0`, `0x82b9b590`) were already map-consistent and stayed so.

## No main membership lost

Comparing this worktree's final `scripts/symbol_aliases.json` against
`git show main:scripts/symbol_aliases.json` by address-set and per-address
member-set difference (not a raw group-count comparison, since `main` has
independently advanced since I branched at `31d347ed5` via unrelated lanes'
commits to the same file — a raw count comparison would be comparing
different points in time and is not the right instrument here):

- **0 addresses** present in git-main are missing from the worktree.
- **0 addresses** shared between git-main and the worktree have any lost
  member (every folded spelling and survivor present on main is still
  present here).
- **Exactly 3 addresses** exist in the worktree but not in git-main — the 3
  new groups this wave created (`0x823d6fa8`, `0x827a37b0`, `0x82362b20`).

This is the clean, defensible answer to "every membership already on main
still exists in the final file": it does, with no exceptions, and the only
net change relative to main is the intended additions.

## Native build gate

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```
Full log: `~/tmp/w16gq_native_gate.log`. All 18 targets relinked and OK; 0
build errors, 0 linker diagnostics, 0 warnings.

## Deferred / not in scope this wave

- **Pair #14** — scout report recommends ADMIT (new group at `0x827e16a0`,
  `insert<list<MeshInstance>>` survivor folding
  `insert<list<BoneState@BandCharacter>>`), but its survivor address sits in
  a range another lane is currently relabelling. Deferring to avoid a
  collision; this is scheduling, not a technical objection — the underlying
  pairing was not found to have any defect.
- **Pairs #1, #11** — see judgment call above; group at `0x827eb0b8` not
  created this wave pending reviewer decision.
- **Remaining ~29 of 44 candidate pairs** beyond the scout report's top-15 are
  unadjudicated. The co-withdrawal-contamination sweep already covers all 44
  and found no further contamination beyond #3/#12, so that one check would
  not need re-running if this work continues.

## Questions for the reviewer

1. **Pairs #1/#11 (`0x827eb0b8`)**: should this group be created despite the
   single `CYCLE-ASSUMED` entry each, given (a) `chase()`'s own docstring
   treats `CYCLE-ASSUMED` as a legitimate coinductive proof step, not a gap,
   and (b) the same relationship is independently corroborated via
   `SLOT-FOLD-OK` elsewhere in the same log with no cycle required? Or does
   the literal "zero on all three counts" gate exist specifically to prevent
   this kind of case-by-case override, in which case the decline stands?
2. **`ALIAS_SUSPECT` wording asymmetry**: is it worth normalizing
   `ab_measure.py`'s shape-check message so a legitimate pure-forgiveness
   patch doesn't read as more alarming in one diff direction than its exact
   mirror image in the other? (Not blocking — flagged for whoever next tunes
   that message.)
3. **Pair #14 scheduling**: once the other lane's relabelling of
   `0x827e16a0`'s range lands, should pair #14 be picked up as a follow-on
   single-group wave, or folded into a future consolidated pass over the
   remaining 29 unadjudicated pairs?

---

**Summary for the user**: Installed 10 of 15 candidate ICF-fold pairs into
`scripts/symbol_aliases.json` (2 existing groups grown at `0x822d8cc0` and
`0x82b9b590`; 3 new groups created at `0x823d6fa8`, `0x827a37b0`,
`0x82362b20`), each individually committed and each pre-adjudicated via
`icf_pair_adjudicate.py --chase` with a clean `CHASED T1: PROVEN` verdict, 0
`SLOT-REFUTED`/`BYTES-DIFFER`/`CYCLE-ASSUMED`, and 0 placeholder-relocation
reliance; pairs #5 and #9 additionally confirmed by manual disassembly
(instruction-identical except one relocated `bl` target each). Declined
pairs #1/#11 (1 `CYCLE-ASSUMED` each — flagged as a judgment call, see above),
#3/#12 (pre-existing co-withdrawal contamination), and deferred #14 (address
range under another lane's active relabelling). Measured whole-binary A/B in
both directions: forward +17 matched functions / +6,220 B / +0.060703 pp
(`masked_equal_functions` unchanged, 13 units improved, forced re-split
confirmed both legs); reverse is the exact negation. Predictions were
directionally right but missed in size on both counted metrics (bytes came in
above the predicted ceiling, functions at roughly half). Validator: PASS, 0
contradicted, before (1408 consistent/1660 total) and after (1411
consistent/1663 total) — the +3 exactly matches the 3 new groups, and a
direct address/member-set diff against `main` confirms zero memberships were
lost. Native build gate: `NATIVE_GATE_RESULT verdict=PASS expected=18
verified=18 skipped=0 partial=0 failed=0 rc=0`. Full write-up:
`docs/decomp/W16GQ_FCNP_NONMEMBER_ADJUDICATION_2026-09-30.md`.
