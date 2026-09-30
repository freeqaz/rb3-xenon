# W16-GP: TrackWidgetImp Sort() cluster — findings, results, open items

Lane W16-GP, worktree `/home/free/tmp/wt-w16-gp`, branch `w16-gp`, base
`4af5f32f0`. Input: frozen scout report `/home/free/tmp/w16gp_report_frozen.md`
(8 numbered "specific goals for the implementer"). Every prediction was
written to `docs/decomp/W16GP_PREDICTIONS_2026-09-30.md` and committed before
the corresponding change, then measured with `tools/ab_measure.py`. Full
prediction/measurement detail (evidence, disassembly, COFF symbol dumps,
splits.txt reasoning) lives in that log; this document is the summary +
verdict layer for the reviewer.

Commits (base `4af5f32f0` → tip), oldest first:

```
a655652e9  docs(W16-GP): prediction for Step 1 relabel, plus a re-scoping finding
a9f34adf7  target_symbol_map: fix 0x827e3c38/0x827e3e10 _S_sort<T> relabel
589c04877  predict(W16GP step 2): MeshInstance Sort() needs a splits.txt boundary move too
7d75f4e65  W16-GP step 2a: re-home TrackWidgetImp<MeshInstance>::Sort under TrackWidget.cpp
a520c6659  splits: accept dtk's derived .pdata correction for the step 2a boundary move
11ac30739  docs(W16-GP): record Step 2a measured result -- hit, +1 fn / +64 B as predicted
822367834  W16-GP step 3a: port TrackWidgetImp.cpp, adapt MultiMeshWidgetImp for InstanceList
b773c1760  splits+map: carve TrackWidgetImp.cpp Sort overrides out of VocalTrack/MidiParser
f18a89517  splits: accept dtk's .pdata re-derivation for the TrackWidgetImp.cpp carve
5dc8e8d68  docs: record Step 3b measured result and VocalTrack reattribution analysis
```

## Report findings: held vs. not held

| Goal | Report claim | Held? | Notes |
|---|---|---|---|
| 1 | `0x827e3c38`/`0x827e3e10` mislabeled `_S_sort<T>` | **Held** | Relabel confirmed against retail bytes; applied. |
| 2 (MeshInstance half, `0x827e48c0`) | `Sort@TrackWidgetImp<MeshInstance>` unnamed | **Held**, needed more than the report said | Report implied a map-only fix; the address actually sat inside `MidiParser.cpp:`'s splits.txt block, so a **splits.txt boundary move** was also required before the map entry could pair against the right unit. |
| 2 (TextInstance half, `0x827e6470`) | same class of fix, needs `TrackWidgetImp.cpp` to exist first | **Held** | Confirmed no compiled body existed anywhere in the tree pre-Step-3; genuinely gated on porting the file, matching the report's own framing. |
| 3 (`0x827e63f8`, `MultiMeshWidgetImp::Sort`) | needs `TrackWidgetImp.cpp` | **Held** | Same as above; non-template override, ground-truth spelling pulled from the compiled `.obj`, not trusted from the report. |
| 4 (thunks `0x827e5c48`/`0x827e5d78`) | need renaming to `TextInstance` list dtor spellings | **REFUTED** | Both addresses were already correctly named as `MidiParser*` list/`_List_base` destructors (an unrelated internal registry). Renaming them per the report would have broken an already-correct mapping. No action taken. |
| 5–7 | port `TrackWidgetImp.cpp` from rb3-Wii oracle | **Held** | File ported, compiles, declared `NonMatching` in `objects.json`. |
| 8 (optional `RndMultiMesh` class-layout check) | — | **Not pursued** | Optional per the report; not required for the required steps; deprioritized in favor of closing the measured steps cleanly. |

One incidental finding beyond the 8 goals: the compiled `TrackWidgetImp.obj`
also contains `?Sort@?$TrackWidgetImp@UInstance@RndMultiMesh@@@@UAAXXZ` (the
base template's own instantiation, reachable via `ImmediateWidgetImp`). The
report gives no confirmed retail address for it and none of goals 2–4
reference it — left unmapped, flagged here as an open item rather than
guessed at.

## Predicted vs. measured, per step

All measurements via `tools/ab_measure.py` against the whole-binary
`name_check` ruler (project default), with a `none`-ruler control run
alongside each to distinguish "moves real code" from "map-only alias
forgiveness."

**Step 1 — relabel `0x827e3c38`/`0x827e3e10`.**
Predicted ~2 functions / ~800 B (assumed both addresses were themselves
newly-pairable function bodies). Measured: **+1 fn / +64 B**
(`~/tmp/w16gp_step1_ab.log`). `none`-ruler control: **+0 B** — this really is
a pure map-alias repair (correcting an existing wrong name), not code
becoming newly pairable, which is why `none` didn't move.
**Verdict: partial miss on magnitude and mechanism, hit on direction.** The
actual win came from the *renaming* fixing which callers could resolve
against the corrected name, not from either address independently becoming a
new 100%-matched body.

**Step 2a — MeshInstance half of goal 2 (`0x827e48c0`).**
Predicted +1 fn / +64 B, contingent on discovering (before applying) that a
splits.txt boundary move was also required, since the address sat inside
`MidiParser.cpp:`'s block, not `TrackWidget.cpp:`'s. Measured
(`~/tmp/w16gp_step2_ab.log`): **+1 fn / +64 B exactly**, `none`-ruler control
+64 B too (labeled NOT_APPLICABLE — expected, since this genuinely moves a
pairable body into the right unit rather than aliasing a name).
**Verdict: exact hit.**

**Step 3a — port `TrackWidgetImp.cpp`.**
No numeric prediction was made for this step in isolation (it's scaffolding
— the file compiles but nothing was re-pinned into it yet, so it could not
move the whole-binary metric by itself). Confirmed the file compiles clean
and is declared `NonMatching`.

**Step 3b — TextInstance half of goal 2 + goal 3 (map + splits.txt carve
into the new `TrackWidgetImp.cpp:` heading).**
Predicted ~+2 fns / ~+128 B (assumed both `Sort@TrackWidgetImp<TextInstance>`,
64 B, and `Sort@MultiMeshWidgetImp`, 116 B, would reach 100% once pairable).
Measured (`~/tmp/w16gp_step3b_ab.log`): **+1 fn / +64 B** net whole-binary,
alongside a per-unit line that needed investigation: `default/TrackWidgetImp`
+2 (0→2), `default/VocalTrack` **−1** (179→178).

Investigated per the task's "any unexplained regression must be investigated
before continuing" rule, even though the whole-binary number was net
positive. Root cause (full arithmetic in the predictions doc): the moved 464 B
`.text` block held **two** dtk target functions, not one — a 424 B near-miss
(`_S_sort<TextInstance>`, still not at 100%, fuzzy 99.67%) and a 40 B function
(`fn_827E5F30`) that was **already 100% matched** while mis-attributed to
`VocalTrack`'s unit tally. After the move, `fn_827E5F30` is still 100%
matched, just now correctly counted under `TrackWidgetImp`. So:

- VocalTrack −1 = the reattributed row leaving VocalTrack's count (no real
  loss — same bytes, same match status, different unit).
- TrackWidgetImp +1 = that same row re-entering TrackWidgetImp's count (nets
  to 0 across the two units).
- TrackWidgetImp +1 = the genuinely new match, `Sort@TrackWidgetImp<TextInstance>`
  (64 B), which alone accounts for the measured `Δcode_bytes=+64`.

Net: −1 + 1 + 1 = +1, matching the measured whole-binary `Δmatched=+1`
exactly, with the byte delta accounted for with zero residue.
**Verdict: partial hit** — direction and one of the two predicted addresses
landed exactly; `Sort@MultiMeshWidgetImp` (116 B, fuzzy 99.83%) and the
relocated `_S_sort<TextInstance>` (424 B, fuzzy 99.67%) are now correctly
paired and adjudicable for the first time, but remain near-misses requiring
source-level work, not map/splits work — explicitly out of scope for this
step and left as an open item below. The "VocalTrack regression" is closed:
it is a benign accounting artifact of a two-function block move, not a
defect, and required no reversion.

**Cumulative whole-binary effect of all measured steps**: +3 matched
functions / +192 B total (Steps 1, 2a, 3b each contributing +1 fn / +64 B),
each independently confirmed net-positive with no unexplained residual after
investigation.

## Native build gate

Final required action, run against the worktree at commit `5dc8e8d68`
(tip, after all source/splits/map/doc commits):

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

Full output archived at `~/tmp/w16gp_native_gate.log`. 18/18 targets
verified, 0 skipped, 0 partial, 0 failed — a clean pass, not an
`INCOMPLETE`/`PARTIAL` result that happens to also print PASS.

## Open questions for the reviewer

1. **`_S_sort<TextInstance>` (424 B, fuzzy 99.67%) and
   `Sort@MultiMeshWidgetImp` (116 B, fuzzy 99.83%)** are now correctly paired
   in `TrackWidgetImp.cpp`'s unit but not at 100%. These are genuine
   source-level near-misses (register allocation / instruction-selection
   differences, not identified in detail this session) — worth a follow-up
   lane with `run_diff_inspect diagnose` on a full build.
2. **`Sort@TrackWidgetImp<RndMultiMesh::Instance>`** (the base template's own
   instantiation, present in the compiled object via `ImmediateWidgetImp`)
   has no confirmed retail address in the report and was left unmapped.
   Worth checking whether it corresponds to one of the report's ~115
   unmapped functions (goal 6) in a future pass.
3. **Goal 6** (~115 unmapped functions in the broader report) and **the full
   Q4 splits.txt reorganization** the report also gestures at were both
   deliberately treated as out of scope for this lane — they are much larger
   in surface area than the 8 numbered goals this lane was assigned to
   execute, and mixing them in would have diluted the evidence trail for the
   steps that were done. Left as-is for a dedicated future lane.
4. Step 1's mechanism mismatch (predicted "two newly-pairable bodies",
   measured "one repaired alias, `none`-ruler flat") is worth double-checking
   against `symbol_aliases.json`/ICF-fold considerations if a future lane
   revisits the `0x827e3c38`/`0x827e3e10` pair — I did not find evidence of a
   fabricated alias (the `none` control moving 0 is consistent with a correct
   repair, not a forgiveness artifact), but I did not exhaustively rule out
   every alternative mechanism for the +64 B figure.

## What was not done

- Goal 4's report-hypothesized rename was **not applied** — it was checked
  against `target_symbol_map.json` first and found to already be correct
  under a different, unrelated identity (`MidiParser*` list destructors, not
  `TextInstance`). Applying it would have been a regression; documented and
  skipped instead.
- Goal 8 (optional `RndMultiMesh` class-layout verification via
  `lookup_struct_offset`/`class_layout_report.py`) was not run — explicitly
  optional in the report and not required by the task's numbered steps.
- Goal 6 (~115 unmapped functions) and the report's broader Q4 splits
  reorganization were not attempted — out of scope for this lane by design
  (see Open Questions #3).
- The two near-miss functions surfaced by Step 3b
  (`_S_sort<TextInstance>` and `Sort@MultiMeshWidgetImp`) were **not**
  pushed to 100% — that is source-level decomp work, not the map/splits
  scope this lane was executing, and is flagged for a follow-up lane instead.
- No commits were made to `main`, nothing was pushed, and none of the
  explicitly off-limits source files
  (`MemMgr.*`, `MemHeap.cpp`, `Memory_Xbox.cpp`, `PoolAlloc.cpp`,
  `VocalTrack.cpp`, `VocalPlayer.cpp`, `SaveLoadManager.cpp`,
  `MusicLibrary.cpp`, `StoreMenuPanel.cpp`) were edited — the one
  `VocalTrack.cpp`-adjacent change made was the explicitly pre-granted
  splits.txt boundary move on its `0x827e5d88`–`0x827e5f58` `.text` block,
  which does not touch the source file itself.
