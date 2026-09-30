# W16-GR predictions — group 10 push_back<T*> batch (2026-09-30)

Written and committed **before** running `ab_measure.py`, per the task's
standing "predict before you measure" discipline. Source: frozen scout report
`/home/free/tmp/w16gr_report_frozen.md`, independently re-verified this lane
against `tools/icf_pair_adjudicate.py` (selftest/chasetest/self-break all
passed) and against a fresh `report.json` read at full float precision.

## What was installed

All 60 scout-proposed `push_back<T*>` spellings (survivor
`?push_back@?$vector@PAVChatReceiver@@V?$StlNodeAlloc@PAVChatReceiver@@@stlpmtx_std@@@stlpmtx_std@@QAAXABQAVChatReceiver@@@Z`,
group 10, address `0x82b5f808`) are now in `scripts/symbol_aliases.json`
group 10's `folded[]` list (72 entries: 12 pre-existing + 60 new), landed
across 6 commits of exactly 10 spellings each
(`4c2c26270` .. `b4afc6d9b`). 0 of the 60 candidates were rejected — all
passed CHASED T1 = PROVEN with zero `CYCLE-ASSUMED` / `SLOT-REFUTED` /
`BYTES-DIFFER` occurrences and a non-zero named-relocation tally, matching the
scout's own tallies exactly (60/60). 3 non-pointer controls
(`vector<float>`/`M`, `vector<Extent>`, `vector<Gem>`) correctly REFUTED at
both flat and chased T1 and were NOT installed.

## Predicted whole-binary delta

- **Δ matched functions: +85**
- **Δ matched-code bytes: +27,572**
- Source: Σ `report.json` `size` for the 85 pure rows (§2/§3 of the scout
  report) that are currently 0%-credited and become fully coverable once all
  60 spellings are admitted (no row needs a spelling outside the 60).
- Re-confirmed this lane at full float precision, not the rounded display: the
  maximum `match_percent_normalized`/`fuzzy_match_percent` value across all 85
  target rows, pre-fix, is **99.98984**
  (`?ParseDataResultsIntoSetlists@MusicLibraryNetSetlists@@QAAX_N@Z`). A
  `round(x,1)` histogram shows 12 rows (mpn) / 11 rows (fuzzy) displaying as
  "100.0", which is purely a rounding artifact of values in the 99.94–99.99
  range — **zero of the 85 rows are genuinely at 100% already**, so the
  +85/+27,572 prediction is not inflated by rows that would cross regardless
  of this change.

## `masked_equal` — explicit correction applied

The scout report's goal 2 (§7.2) states the 85 rows will "read
`masked_equal = true`" after the fix, i.e. characterizes the mechanism as
ICF/alias forgiveness rather than an honest source match. **That
characterization of the *mechanism* (alias forgiveness, not a real source
match) is accepted**, but the specific claim that these rows will show up as
`masked_equal = true` in `report.json` is corrected here before measuring:
on this project, `masked_equal_functions` / the per-row `masked_equal` flag
mark **funclet byte-signature pairings** specifically (objdiff's pass-2b
over-subscription disclosure), not general alias-table forgiveness. A row
that crosses because its callee now resolves through `symbol_aliases.json`
does **not** necessarily set that flag.

**Prediction: `masked_equal_functions` Δ = 0** from this change. If it moves
by any nonzero amount in the forward-leg measurement, that is a surprise to
be called out explicitly in the deliverable doc, not silently folded into the
headline numbers.

## Expected classification / alias-map behavior

- `ab_measure.py`'s forward leg is expected to classify this patch
  `ALIAS_SUSPECT` (map-only patch, no `src/` changes) — this is the *expected*
  label for a `symbol_aliases.json`-only change, not a red flag.
- `tools/icf_alias_finder.py --validate` is expected to PASS with 0
  contradicted groups, both before and after the edit (group 10's existing 12
  memberships are untouched — only appended to — and the new 60 share the
  identical proof chain already used for the pre-existing 12, per the file's
  cumulative evidence string).
- Every pre-existing alias membership on main (all 1660 groups) is expected
  to still be present verbatim after the edit — only group 10's `folded[]`,
  `evidence`, `lane`, and `added_*` key changed; confirmed via
  `git diff 07f080afc HEAD -- scripts/symbol_aliases.json` producing exactly
  one contiguous hunk touching only group 10's region of the file.

## The 4 multi-spelling rows (scout §5) — verified present, mapped to batches

4 of the 85 pure rows need 2–3 distinct spellings from the 60-candidate list
before they fully cross. All needed spellings are confirmed installed,
though a few land in different batches/commits — this does not matter for
measurement since `ab_measure` runs after all 6 commits are applied to the
worktree:

| row | spellings needed | batch(es) landed |
|---|---|---|
| `?RebuildKeyCheatsForMode@CheatsManager@@QAAXXZ` | `push_back<KeyCheat*>`, `push_back<QuickJoyCheat*>` | both batch 3 |
| `??0PrefabMgr@@QAA@XZ` | `push_back<BandCharDesc*>`, `push_back<CharCreatorPrefab*>` (nested in `PrefabMgr`), `push_back<PrefabChar*>` | all batch 1 |
| `?Update@PlayerDiffIcon@@UAAXXZ` | `push_back<BandLabel*>`, `push_back<RndMesh*>` | BandLabel batch 2, RndMesh batch 1 |
| `?Update@MicInputArrow@@UAAXXZ` | `push_back<EventTrigger*>`, `push_back<RndAnimatable*>` | both batch 1 |

The remaining 81 pure rows need exactly one spelling each. These 4 rows will
be checked individually in the post-measurement deliverable to confirm they
actually cross (the scout flagged them as "most likely to show a partial
cross if any one spelling in the batch is rejected" — moot here since 0
spellings were rejected, but still worth the direct confirmation per the
task's Step 5 instruction).

## Explicitly out of scope for this prediction

- **25 mixed rows / 12,392 bytes** (scout §6) need a group-10 spelling **plus**
  at least one unrelated fix and will NOT fully cross from this change alone
  — not counted in the +27,572 prediction. `?Init@ByteGrinder@@QAAXXZ` is the
  representative case (needs `push_back<DataNode(*)(DataArray*)>`, which IS
  among the 60, **plus** a separate, unrelated op40/op58 naming-mismatch fix)
  — flagged for follow-up, not this batch's credit.
- **The ">8 charges" population (scout §4)**: the brief said 9 rows, this
  lane's own prior in-context estimate (pre-compaction) said ~32, and the
  scout's fresh from-scratch recomputation this session says 2
  (`?Init@ByteGrinder@@QAAXXZ` at 66 charges, `??0FingerShape@@QAA@PAVRndDir@@@Z`
  at 20 charges, both 0 placeholder). This is an unreconciled discrepancy
  about a **different, non-overlapping question** (charge-count outliers, not
  row-crossing count) and does not affect the +85/+27,572 prediction above.
  It will be independently re-derived a third time in the deliverable doc.
