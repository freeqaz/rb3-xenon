# W16-GQ — predictions, written and committed BEFORE measuring (2026-09-30)

Per house convention: predict, commit, then measure. These numbers come from
`/home/free/tmp/w16gq_nonfcnp_full_table.json` (the scout's static per-pair
adjudication, keyed on `ours` mangled spelling), summed over the 10 pairs
actually being installed this wave. They are explicitly **NOT** a measured
A/B — the scout report says so directly ("the `full_bytes`/`full_rows`
figures in this report are static-adjudication estimates ... not a measured
A/B" and "un-pairing/re-pairing effects and cascade effects are not linearly
additive and must be measured, not summed"). Recording them anyway, as an
upper-bound sanity check to compare the real A/B against.

## Pairs being installed this wave (10 of 15 candidates)

| pair | ours (short) | target group | full_rows | full_bytes |
|---|---|---|---:|---:|
| #2 | `~vector<ObjPtr<UIColor>>` | 0x822d8cc0 (existing) | 8 | 876 |
| #7 | `~vector<FilePath>` | 0x822d8cc0 (existing) | 3 | 560 |
| #8 | `~vector<ObjPtr<EventTrigger>>` | 0x822d8cc0 (existing) | 4 | 492 |
| #13 | `~vector<ObjPtr<RndPropAnim>>` | 0x822d8cc0 (existing) | 8 | 368 |
| #9 | `_Copy_Construct<Gem>` | 0x82b9b590 (existing) | 2 | 444 |
| #4 | `insert<list<PanelRef>>` | 0x823d6fa8 (new) | 1 | 736 |
| #6 | `insert<list<AwardEntry>>` | 0x823d6fa8 (new) | 1 | 608 |
| #15 | `insert<list<Sink@MsgSource>>` | 0x823d6fa8 (new) | 1 | 340 |
| #5 | `__destroy_range<TrackChannels>` | 0x827a37b0 (new) | 5 | 608 |
| #10 | `insert_after<slist<pair<int,SongStatus*>>>` | 0x82362b20 (new) | 2 | 420 |

**Sum: 35 full_rows / 5,452 full_bytes.**

## Declined / deferred (not in this wave, no byte prediction)

- **#1, #11** (would-be new group at `0x827eb0b8`) — DECLINED. Both pairs'
  `--chase` proof includes exactly 1 `CYCLE-ASSUMED` entry, which fails the
  literal "zero on all three counts" install gate given for this task, even
  though the underlying relationship is independently corroborated by a
  `SLOT-FOLD-OK` elsewhere in the same log. Flagged for the reviewer as a
  judgment call — see the final adjudication doc.
- **#3, #12** — DECLINED per the report's own co-withdrawal contamination
  finding (both spellings already sit in an 86-member/co-withdrawn
  `FABRICATED_CLOSURE_NOT_PARTITION` roster at overlapping addresses).
  `tools/alias_restore_fcnp_membership.py` is not used.
- **#14** — DEFERRED. Its survivor address sits in a range another lane is
  currently relabelling; not touched this wave.

## Predictions

1. **`matched_code` (whole-binary, forward leg):** predict **up to +5,452 B**,
   but expect the real number to be **lower**, because (a) the static table
   prices each row assuming this fix is its *sole* remaining penalty, which
   the table's own full/partial split tries to guarantee but cannot fully
   verify without a live build, and (b) CLAUDE.md's map economics findings
   are explicit that un-pairing/re-pairing and cascade effects are not
   additive. A miss here is expected and will be recorded, not treated as an
   error.
2. **`matched_functions`:** predict **up to +35** (one per `full_rows` entry),
   same caveat as above — likely an overestimate.
3. **`masked_equal_functions`:** predict **Δ0**. These are genuine named
   relocation-target forgiveness (real ICF-fold membership), not masked-equal
   manufacturing, so this counter should not move either direction.
4. **Re-split:** predict the tool reports a forced re-split on both legs,
   since this is a `symbol_aliases.json` (map-adjacent) change consumed by
   the alias/equivalence pipeline, not a `splits.txt` edit — TBD whether
   `ab_measure.py` treats `symbol_aliases.json` edits as requiring a re-split
   at all; recording actual tool behavior rather than assuming.
5. **Reverse leg:** predict the mirror image of (1)-(3), i.e. approximately
   the negative of whatever the forward leg measures, since this is a single
   coherent patch applied then reverted.
6. **Validator (`icf_alias_finder.py --validate`):** predict **PASS, 0
   contradicted**, both before and after — these are ADD-only edits to
   `folded[]` (three new groups, two existing groups grown), no removals, no
   re-targeting of any existing survivor/folded spelling.
7. **Native build gate:** predict **PASS 18/18, 0 SKIPs** — this change only
   touches `scripts/symbol_aliases.json`, a post-link/objdiff-side artifact
   never referenced by `native/`, so it should be gate-neutral.

All of these will be checked against the actual tool output below/in the
final adjudication doc, misses included.
