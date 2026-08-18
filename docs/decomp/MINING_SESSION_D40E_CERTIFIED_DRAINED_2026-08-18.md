> **STATUS (2026-08-18): CURRENT — CLOSURE RECORD.** Session
> `d40e3b86-b15a-4cc0-987b-0b7af342590d` (2026-07-24 → 07-29) is **CERTIFIED
> DRAINED**: mined by 8 sonnet agents + 1 fable reviewer, **zero lost findings**.
> **Do not re-mine it.** Every numeric absolute in that session predates the
> 2026-08-12 `none`→`name_check` ruler flip and is incomparable to today.
> ⛔ The 13/13 false "lost finding" positives were caused by a **search scope that
> omitted `docs/plans/`** — see §3a and `hub_agent_ops.md`.

# ROADMAP — mining operation d40e (July 24-29 session), reviewed 2026-08-18

Reviewer: skeptic pass over 8 miner slice reports in `/home/free/tmp/mine-d40e/findings/`.
Every "genuinely lost" claim was independently re-verified against the repo record
(including `docs/plans/`, which the miners did not search) before being accepted or killed.

## 1. HEADLINE

**The July session's findings are already fully recorded — zero lost insights survived
review.** The mined session (`d40e3b86…`, 2026-07-24 → 07-29) is the same session that
wrote most of the current memory files AND a dense set of dated lane write-ups under
`docs/plans/` (`funclet-cascade-lever-2026-07-25.md`, `lane-ag-deep-body-ports-2026-07-26.md`,
`lane-ah-layout-oracle-2026-07-26.md`, `decomp-state-2026-07-19.md` — updated through the
session, `wii-oracle-tu-location-2026-07-29.md`, `tu-pin-wave-2026-07-29.md`). Of the ~13
findings the miners flagged as genuinely absent from the record, **all 13 were located in
the written record, most verbatim** — the miners searched memory + `docs/decomp/` +
`CLAUDE.md` but 7 of 8 slices never searched `docs/plans/` (grep confirms: only slice1
mentions that directory at all). The one durable output of this operation is a
**mining-process correction** (search scope) plus two or three optional one-line
point-of-use promotions. This is a legitimate result: it certifies the session as
fully drained and stops anyone re-mining it.

## 2. SURVIVED REVIEW

**No recovered findings.** Nothing the miners flagged is simultaneously (a) absent from
the record, (b) still true, and (c) worth acting on. What survives is meta-level:

### S1 (rank 1) — Mining/verification passes MUST search `docs/plans/` (and data artifacts)
- **Claim:** in this operation, 13 of 13 "genuinely lost" claims were found already
  recorded, ~10 of them in `docs/plans/*.md` specifically — often verbatim to the phrase
  (e.g. "transitive re-pairing", "BANKABLE is a lower bound", "A rb3-Wii chain difference
  is not evidence of a defect", "A resumed fixer commits *behind* your merge").
- **Why still true:** structural — `docs/plans/` is where lane write-ups land, and
  CLAUDE.md itself calls them "dated records". A search scope that excludes them will
  manufacture false "lost finding" positives every time.
- **Where to write it:** this file is the primary record; optionally one line in
  `hub_agent_ops.md` ("when auditing whether a finding is recorded, grep docs/plans/ and
  docs/decomp/history/ too — a 2026-08-18 mining pass got 13/13 false lost-positives from
  skipping them").
- **What it saves:** the next mining/verification operation over any 2026-07 session
  (they share the same write-up discipline) from re-reporting recorded material as lost.

### S2 (rank 2, optional, cheap) — two point-of-use discoverability promotions
Real knowledge, already recorded, but recorded ONLY in dated July plan files where
nobody working live will see it. Each is a one-line edit:
1. **`scripts/harvest/class_layout_report.py` docstring:** add "compile prints nothing
   until the TU finishes — ~10 min (BandSongMgr) to ~50 min under fleet load; a short
   timeout is indistinguishable from CLASS_ABSENT" (recorded at
   `docs/plans/lane-ah-layout-oracle-2026-07-26.md:263-270`; the docstring's current
   three-label contract fixes a *different* false-zero and says nothing about latency —
   verified 2026-08-18).
2. **`hub_agent_ops.md`:** the resumed-fixer trap ("a branch reported final and merged
   can be resumed and committed *behind* the merge; check `git merge-base --is-ancestor`
   before treating it as landed" — `lane-ah-layout-oracle:273`,
   `lane-ao-map-ownership-2026-07-26.md:16-19`) and the pgrep cross-lane trap ("a bare
   `pgrep -f <shared tool>` counts other lanes' builds; match `/proc/<pid>/cwd` against
   your worktree" — `decomp-state-2026-07-19.md:2055`). The hub currently carries
   neither (verified by grep 2026-08-18).

These are NOT recovered findings — they are filing improvements. Skipping them costs
little; the record exists and grep finds it.

## 3. REJECTED

### 3a. Claimed lost, actually recorded (the big group — miner search-scope artifact)

| # | slice | claim | where it is recorded |
|---|---|---|---|
| 1 | 2 | regswap⇒at_limit two-part gate (size delta zero AND no insert/delete/diff-op) | `docs/plans/funclet-cascade-lever-2026-07-25.md:1169-1171` ("Revised rule… requires both"), `lane-ag-deep-body-ports-2026-07-26.md:190` — also **superseded** by the stronger 08-14 rule (pure regalloc cannot be in mpn<100 by construction; never defer on a REGISTER_SWAP label) |
| 2 | 2 | "0 BANKABLE is a lower bound; identification pays as worklist generation" | `funclet-cascade-lever-2026-07-25.md:757-778, 1348` verbatim — and superseded in substance by the 08-17 map/name economics (naming pays in bug exposure, not bytes) |
| 3 | 3 | SAVE_SUPERCLASS 1,349-site audit + ColorPalette/SpotlightDrawer negative control | `funclet-cascade-lever-2026-07-25.md:1447-1456` verbatim ("we match retail **because** it is there… A rb3-Wii chain difference is not evidence of a defect"), `identical-pct-cluster-scan-2026-07-26.md:248,365` |
| 4 | 3 | byte-loop has 3 shapes (DELTA 127 · XOR 95 · UPDATE 49), discriminator blind spot, 24/24 `mr.`/`cmplwi` near-miss | `lane-ag-deep-body-ports-2026-07-26.md` §5.4 lines 355-439, verbatim including all counts and the wall reclassification |
| 5 | 4 | class_layout_report ~50-min latency / timeout ≠ CLASS_ABSENT | `lane-ah-layout-oracle-2026-07-26.md:263-270` (residual: not in the tool docstring — see S2.1) |
| 6 | 4 | resumed-fixer trap + ancestry re-check | `lane-ah-layout-oracle-2026-07-26.md:273`, `lane-ao-map-ownership-2026-07-26.md:16-19`, `branch-audit-2026-07-29.md:28` (residual: not in hub_agent_ops — see S2.2) |
| 7 | 5 | cross-unit byte-twin moves structurally unsafe; capacity filter can't model transitive re-pairing | `decomp-state-2026-07-19.md:1951-1994` — full write-up incl. "greedy pairing displaces the incumbent", the 600→87 funnel, −10/−28 result, and "⛔ DO NOT FUND the 716-function pool" |
| 8 | 5 | absence from rb3-Wii ≠ absence from RB3-360 retail (42-row downgrade) | `decomp-state-2026-07-19.md:2042-2045` verbatim ("★★ ABSENCE FROM ../rb3 DOES NOT PROVE ABSENCE…", incl. the exact downgrade wording) |
| 9 | 6 | bare `pgrep -f "ninja-locked"` counts other lanes' builds; use `/proc/<pid>/cwd` | `decomp-state-2026-07-19.md:2055` (see S2.2) |
| 10 | 6 | harness task reaping truncates long uncached builds; `setsid nohup` + report.json mtime | `decomp-state-2026-07-19.md:2059-2061`; refined further in memory `project_nearmiss_codegen_wall2.md:95` ("setsid alone does NOT…") |
| 11 | 7 | BD phantom pin (`UIProxy` ⊂ `SongDifficultyDisplay.cpp`), over-broad pins, `located_spans.json` "unclaimed worklist" | `wii-oracle-tu-location-2026-07-29.md:42,105,175,215,233`; the file is committed at `scripts/harvest/tu_locate/located_spans.json` and was CONSUMED the same day by `tu-pin-wave-2026-07-29.md` — an entire doc auditing it, including its confidence-ladder and snapped-bounds defects. Not unclaimed. |
| 12 | 8 | 3-way JSON map merge needs git stage blobs | institutionalized as `scripts/harvest/resolve_json_union.py` + `docs/decomp/TOOLING.md:421` (miner self-killed; concur) |
| 13 | 1 | "de-mirage screen" / "4-signal audit" / "anchored-BinDiff" possibly lost under naming drift | all three exist under those names: `batch5-ranked-2026-07-24.md` (de-mirage, ×3), memory `feedback_scope_native_port_2026-07-24.md:110` ("4-signal" mispair-audit method), `project_bindiff_spike_2026-07-20.md` + `docs/decomp/gameid/VERDICT.json` (anchored BinDiff). Miner's grep failed on its own terms, cause unknown (possibly quoting). |

### 3b. Superseded (correctly flagged by miners themselves — kept here so nobody resurrects them)
- **"Bulk-conversion law" (convert whole TUs at once):** REFUTED 2026-07-27 by a 3-leg
  control; per-function conversion is positive and additive; convert wholly only when
  statics share a guard word (`project_localstatic_symbol_lever.md`). Slice 5 correctly
  flagged that a naive read of the transcript would resurrect the refuted rule.
- **Lane BF "funclet cascade = best-value unfinished work, only 17 TUs":** refuted
  same-day by laneBK (qualifying population 6, not hundreds; the flag was already on
  125 TUs; `project_localstatic_frame_cascade_2026-07-29.md`, tagged REFUTED-IN-PART).
- **All July funclet-pool sizes** (16,821 → 2,750 → 2,720): every one is a superseded
  snapshot; the ceiling/pool numbers move both ways and must be re-measured, never
  inherited (standing CLAUDE.md rule).

### 3c. Ruler-stale (dead as numbers, even where the mechanism survives)
Every byte/percent/count absolute in the July transcript predates BOTH the 2026-08-02
disclosure flip and the 2026-08-12 `none`→`name_check` ruler flip (~817 kB / 7.9 pp on
an unchanged binary), and most predate the reachable-ceiling re-measurements. Examples
the miners correctly refused to pass through: +32/+56/+145 funclet flips, 37,619→38,819,
−23/−10/−28 move deltas, 42-row counts, 5.2%/2.7% divergence bands, "+560 farmable".
None of these is re-derivable as stated; all mechanisms they decorated are recorded.

### 3d. Real but worthless (no action would ever be taken on them)
- **progress_today midnight-baseline zeroing:** the fix is live code
  (`tools/scope_map.py:1909` writes `progress_history.jsonl`, actively appending as of
  2026-08-18 03:04). Documenting a fixed bug's pre-fix behaviour buys nothing.
- **The `UnisonIcon`/`MoveGraph` 53-VA merge-collision war story (slice 8):** tactical
  detail of a hand-merge failure mode that `resolve_json_union.py` now prevents; the
  tool's existence is the record that matters.

### 3e. Miner artifacts
- Slice 3's framing "this finding fell through a hand-off crack" (byte-loop orphaned
  subagent) was wrong — the relay to the lane lead demonstrably landed, in the lead's
  own write-up (`lane-ag-deep-body-ports` §5.4). The transcript's "no artifact shows it
  landed" was true only within the miner's search scope.

## 4. RECOMMENDED ACTIONS (ordered)

1. **Write down (do now, minutes):** keep this ROADMAP.md as the closure record for
   session d40e — the session is CERTIFIED DRAINED; do not re-mine it. Add the S1
   search-scope line to `hub_agent_ops.md` (or wherever the next mining op's
   instructions are drafted): *cross-check scope must include `docs/plans/`,
   `docs/decomp/history/`, and committed .json/.tsv artifacts, not just memory +
   docs/decomp + CLAUDE.md.*
2. **Write down (optional, minutes):** the two S2 point-of-use promotions (docstring
   latency line; hub_agent_ops resumed-fixer + pgrep-cwd lines). Low value, near-zero
   cost, consistent with the project's hub discipline.
3. **Do not fund:** re-mining this session or its neighbours from 2026-07 with the same
   method — the July sessions wrote their own record in real time (slices 1, 2, 5, 7, 8
   all independently observed this) and the yield here was zero.
4. **Do not fund:** anything shaped like the transcript's July levers — byte-loop DELTA
   sites (walled, priced +1/site if the `cmplwi` form is ever found), cross-unit
   byte-twin moves (structurally refuted), funclet-cascade generalization (refuted),
   the `located_spans` pin worklist as a score play (pin additions are metric-neutral
   reattribution; identification is measured at ~0.2% of total_code). These are all
   drained veins with the drain already recorded.
5. **No source-grind items proposed** — consistent with the current campaign state
   (SOURCE_LEVER ≈ 0.59% of gap, ~43% identification-blocked, ~36% out-of-scope XDK,
   permuter OFF). Nothing in the July transcript changes that arithmetic.

## 5. COVERAGE

- **All 8 slice files present**, spanning 2026-07-24T17:34 → 2026-07-29T13:16. No
  missing-miner gap. Slice 6 is the thinnest (2.5 days, 3 findings) but self-reports
  its cross-checks; residual under-mining risk there is the largest, though the zero
  yield everywhere else makes a hidden gem unlikely.
- **Timeline seam:** slice 3 ends ~07-26T09:23, slice 4 begins 07-26T16:23 — a ~7 h
  window whose coverage depends on how the slicer cut turns; if anything was skipped it
  is mid-day 07-26, which is also the window best covered by the lane docs
  (`lane-ah`/`lane-ao`/`lane-ar`/`lane-as`/`lane-av`, all dated 07-26).
- **Systematic miner blind spot (the operation's main lesson):** 7 of 8 slices searched
  memory + `docs/decomp/` + `CLAUDE.md` only. Every false "lost" positive died in
  `docs/plans/`. Conversely their already-recorded verdicts were reliable — every one I
  spot-checked held.
- **What this operation could not see:** hidden chain-of-thought (API-redacted; only
  narration survives, per the project's own training-harvest record), and any finding
  the July session never narrated. Also: whether the slices' *self-reported*
  "confirmed already recorded" lists are exhaustively correct — I spot-checked several
  (overlap_check, reloc-correspondence, objcache PCH sidecar, SANDWICH over-carve) and
  found no errors, but did not re-verify all ~40 such entries line by line.
