# w3-pins handoff

Worktree: `.claude/worktrees/wt-w3-pins`, branch `w3-pins`.
Commits: `7d06c8d` (tool: `--worklist` flag), `96d030e` (band3 list, pre-existing),
`049003a` (name/micro-pin ~105 sysnet identities), `1e4870c` (revert 9 bad
identities found during verification).

## What was done

1. **band3 list** (`band3_port_worklist.json`, `--all-wired`): pre-existing work
   from an earlier interrupted session (commit `96d030e`, 3 names). Verified
   consistent, left as-is.
2. **Tool extension**: added `--worklist PATH` to `tools/band3_worklist_pin.py`
   (default `band3_port_worklist.json`), minimal/backward-compatible, so it can
   consume `sysnet_port_worklist.json`-schema files. Committed `7d06c8d`.
3. **sysnet list**: dry-ran `--worklist sysnet_port_worklist_filtered.json`
   (a 474-entry pre-filtered derivative of the 516-entry unfiltered sysnet list,
   found already in the worktree, presumably from the same earlier interrupted
   session — excludes the planner's named collision TUs). Applied, landing:
   - ~40 new `.text` micro-pin lines in `splits.txt` across ~35 TUs (functions
     that were in an unowned gap of an already-wired TU).
   - 67 auto-completed `.pdata` lines (the project's split/build tooling
     auto-completes RUNTIME_FUNCTION metadata when a new `.text` split is added
     for a function that has one — not something the pin tool does directly).
   - 105 new `"0xADDR": "<MSVC mangled>"` entries in `target_symbol_map.json`
     (ADD-ONLY).
   - One real bug found+fixed along the way: `GuitarController.cpp`'s split at
     `0x82777828..0x827778c4` cut through a trailing `except_data_827778c8`
     (8-byte exception-table blob overlapping the last 4 bytes of
     `fn_82777828`'s declared size). Fixed by widening the split end to
     `0x827778c8` to fully absorb the except_data blob.
   - 35 worklist candidates could NOT be auto-named: templates
     (`MakeString<...>`, `__ls<...>` operator<< overloads) and free functions
     that the class::method parser in `gen_game_target_map.py` doesn't handle.
     Left unresolved for a future hand-resolution pass (same as a prior band3
     commit `96d030e` left similar deque-template candidates open).

## Verification sweep (all 104 net new-named symbols)

Built all 65 affected TUs (`tools/ninja-locked`, per-object, never bare
ninja/whole-binary), then ran `bin/objdiff-cli diff -u "default/<TU>"
"<mangled>" --format json -o /tmp/<x>.json` for every one. Full results in
`/tmp/objdiff_results.tsv` (kept in /tmp, not committed).

Applied the lane's stated sanity gate ("target_size within 2x of base_size; if
wildly off, the identity is wrong — revert that one line") plus a
no-diff-data check (`ERROR` / non-function VA). **9 of 104 failed** and were
reverted in `1e4870c` (all NAME-only, no associated `splits.txt` micro-pin, so
this was a pure `target_symbol_map.json` deletion; rebuilt all 9 TUs clean
post-revert):

| addr | identity | issue |
|---|---|---|
| 0x8270c1d0 | `ADSR::Load` | target 248B / base 12B (20.7x), 3.5% match |
| 0x8244e668 | `CharDriver::SetBones` | target 132B / base 8B (16.5x), 5.6% match |
| 0x827ccd80 | `UILabel::Poll` | target 112B / base 4B (28x), 3.2% match |
| 0x827e4c98 | `UISlider::DrawShowing` | target 96B / base 4B (24x), 4.2% match |
| 0x823871f0 | `CharIKScale::CaptureBefore` | 0% match despite 2x-bound sizes |
| 0x8250fc50 | `DateTime::ToDateString` | 0% match, 3.1x size ratio |
| 0x824c1a60 | `SpotlightDrawer::DeSelect` | VA has **no symbols.txt entry at all** — objdiff-cli: "Symbol not found in target" |
| 0x827ed440 | `UIPanel::Draw` | target/base both report **0 bytes** (undiffable) despite symbols.txt declaring size 0x34 there |
| 0x827dc698 | `UITransitionHandler::HasTransitions` | VA resolves to `lbl_827DC698` — a **branch-target label, not a function** |

All 9 were `bsim15-20`/`bsim20-30` (lowest planner confidence tier) —
confirms the sibling-aliasing risk the task flagged for that bucket. Notably,
**most** other `bsim15-20`/`bsim20-30` entries verified fine (many scored
95-99.9% with equal/near-equal sizes — e.g. `BandCrowdMeter::UpdateExcitement`
99.9%, `BandIKEffector::MeasureLengths` 99.3%, several `TrackWidget`/
`MidiReader`/`StreakMeter`/`VorbisReader` methods 99.5-99.9%) — so the raw
a-priori confidence label alone is not a reliable revert signal; the verified
match% + size-ratio gate is what actually separates good identities from bad
ones here.

4 borderline entries **exceed the literal 2x size-ratio bound but were kept**
(none "wildly" off, each has a real 29-47% match consistent with a genuine
unported/diverged implementation rather than a wrong identity, and the naming
tool's exact class+method COFF-symbol match makes sibling-aliasing unlikely):

| addr | identity | match% | ratio |
|---|---|---|---|
| 0x82675148 | `BeatMatchController::RegisterHit` | 47.3% | 2.11x |
| 0x827ca2f0 | `MidiReader::ReadNextEventImpl` | 46.6% | 2.12x |
| 0x827a7de8 | `DataPointMgr::RecordDataPoint` | 35.6% | 2.29x |
| 0x824cfe58 | `WorldCrowd::Draw3DChars` | 29.7% | 2.66x |

These are real measurable fuzzy targets now (the point of NAME-only entries
per the lane spec), just flagging that a future porting pass on these 4
specifically should expect substantial implementation-gap work, not just
codegen-noise cleanup.

## Result: 14 strict-100 pins, 81 fuzzy_kept, 9 dropped

See StructuredOutput for the full itemized lists.

## ICF audit — BLOCKED by infra, not skipped

`tools/icf_alias_check.py` (both `--tu` and `--worktree` modes) requires
`build/45410914/report.json`, which does **not exist in this worktree**
(only 79 of ~2841+ TU objects are built here — the ones this session
compiled — not a full project build). Generating `report.json` requires
`tools/fresh_report.sh` or an equivalent whole-binary build, which the hard
safety rules for this lane explicitly forbid (shared machine, other sessions
building). I did not run it.

**Manual proxy check** using the tool's own documented heuristic (functions
`<= 44 bytes` are ICF-fold/stub-fold candidates) against the 14 strict-100
pins:

| addr | identity | size | confidence_label | risk |
|---|---|---|---|---|
| 0x82756d98 | `MasterAudio::IsLoaded` | 20B | bsim15-20 | **flagged** |
| 0x827aa8f8 | `NetLoader::DetachBuffer` | 40B | bsim15-20 | **flagged** |
| 0x82263ac0 | `PatchLayer::SetPosition` | 36B | bsim15-20 | **flagged** |
| 0x822c4930 | `StreakMeter::NumActiveParts` | 44B | high / ExactInstructionsFunctionHasher | lower risk (strong a-priori identity despite being at the size threshold) |
| 0x827b7cd0 | `TrackDir::SecondsToY` | 12B | bsim15-20 | **flagged** |
| 0x827b7ce0 | `TrackDir::YToSeconds` | 12B | bsim15-20 | **flagged** |

5 of the 6 tiny (<=44B) strict-100 pins are **both** stub-sized **and**
lowest-confidence-tier — the exact pattern the tool's docstring calls out
("THE LESSON") as historically producing fake matches. Unlike the 9 reverted
entries, these did verify as literal 100% byte-identical via a real build+diff
(satisfying the PIN POLICY's stated bar on its own terms), so I did not revert
them — but they should be re-audited with `tools/icf_alias_check.py --tu
<TU>.cpp` the moment a fresh `report.json` exists in this worktree (or a
successor), before treating them as fully honest. The other 8 strict pins are
all real-bodied (72-160 bytes), no flag.

`icf_verdict` in StructuredOutput is `N/A` (accurately reflects "could not
run", not a fabricated pass).

## splits.txt sanity

Re-checked global `.text` range self-overlap after both the apply and the
revert commit: 1004 ranges, 0 overlaps.

## Follow-ups for a future session

- Re-run `tools/icf_alias_check.py --tu {MasterAudio,NetLoader,PatchDir,TrackDir}.cpp`
  once a `report.json` exists, to close out the 5 flagged tiny strict-pins.
- Hand-resolve the 35 unresolved template/operator/free-function worklist
  candidates (`MakeString<...>`, `__ls<...>`, etc.) that the automatic
  class::method parser can't handle.
- The 4 kept borderline-ratio fuzzy targets (`BeatMatchController::RegisterHit`,
  `MidiReader::ReadNextEventImpl`, `DataPointMgr::RecordDataPoint`,
  `WorldCrowd::Draw3DChars`) are real porting-gap targets, not pin-quality
  issues — a source-porting lane (out of scope here) could pick these up.
