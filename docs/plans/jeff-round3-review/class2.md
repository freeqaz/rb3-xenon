# jeff round-3 review — DOC CLASS 2 ("AddRoll-class .pdata over-splits")

**Verdict up front: doc-Class-2 is FULLY IMPLEMENTED AND LANDED. DONE — no residual action.**
The design doc's own STATUS banner ("not yet implemented") is stale; the pass shipped
2026-07-17. The doc's *central premise* (raw `.pdata` over-split) was refuted and the
implementation is a corrected inverse-of-a670a12 pass on the symbol layer.

---

## (a) Doc-class → commit / memory mapping (disambiguation trap resolved)

| doc class | what it is | jeff commit(s) | jeff "Class-N" | status |
|---|---|---|---|---|
| **Class 1** (terminatorless fragment) | span w/ no hard-flow terminator | `fc5d2af` (read-only **census** tool `census_terminatorless_functions`, env `JEFF_CLASS1_CENSUS`) | jeff "Class-1" | **NO-GO** (census-settled: 64% false-positive, do not build extend pass) |
| **Class 2** (AddRoll over-split) | one body split into ≥2 anon fall-through fragments | **`7e49a38`** pass + **`7f69b9e`** doc-xref | jeff "Class-2" | **LANDED +67** @ rb3-xenon `eb4863cc` |
| **Class 3** (stray func_type==3 except_data) | EH COMDAT mid-function | — | jeff "Class-3" | **NO-GO** (population 0, already solved by `b1bc97c` write-gate) |
| *(none — jeff-only)* | branch-reached over-carve tails (head DOES terminate) | `b50881e` `merge_branch_reached_overcarve_tails` | jeff "**Class-4**" | LANDED +35 @ `f03b9719` |

**Trap resolved:** the memory's "Class-2 +67" **IS** the doc's Class 2 (same AddRoll fixture,
same commit lineage, same `docs/plans/jeff-pdata-boundary-round3.md` reference). The memory's
"Class-4 +35" is a **jeff-only** complement class **not in the doc** — discovered while
building Class-2. So the +102 in `project_jeff_class4_merge_2026-07-17` = **+67 (doc Class 2)
+ +35 (jeff-only Class 4)**. Doc classes 1/2/3 map 1:1 onto jeff "Class-1/2/3"; only "Class-4"
is extra.

---

## (b) Premises CONFIRMED / REFUTED with numbers

### Q1 — Does 7e49a38 implement the doc's Class-2 detection+merge?
**CONFIRMED it merges the class, but the DETECTION MECHANISM DIVERGES from the doc (doc premise
was wrong).**

- **Doc proposed:** `merge_pdata_fallthrough_fragments` operating on the **raw `.pdata` entry
  table** — "consecutive pdata entries E1,E2 … merge into one." (doc L204-231)
- **Reality (`merge_fallthrough_leaf_fragments`, `src/cmd/xex.rs:1212`):** operates on the
  **FINAL function-symbol layer, NOT the `.pdata` table.** The 2026-07-16 census proved a
  literal raw-`.pdata` predicate finds **0 candidates** — the AddRoll over-split lives in
  `.pdata` **GAPS** as fragments that jeff's OWN a670a12 leaf-synthesis over-carved. So the
  pass is the **exact inverse of a670a12** on the same symbol layer.
- **REFUTED doc premise:** "Class 2 = raw `.pdata` over-split (one body, 2+ pdata entries)" is
  **wrong** (commit + `project_jeff_class2_merge_2026-07-17`: "literal predicate over pdata
  entries = 0 candidates").

**Predicate mapping — doc's 4 → implemented 6** (`plan_fallthrough_merge_runs` + guards):

| impl guard | doc predicate | note |
|---|---|---|
| P1 exact adjacency `S1.end==S2.addr` | doc P1 ✓ | identical |
| P2 fall-through (S1 last insn NOT hard-flow-terminator) | doc P2 ✓ | identical |
| P3 zero incoming xrefs (post-`tracker.apply`) **AND anon** (`fn_`/`lbl_`) | doc P3 ✓ (refined: + must be anon) | |
| P4 **neither endpoint in `obj.pdata_funcs`** | ~doc P4 (INVERTED framing) | doc merged *within* pdata; impl keeps pdata partition **sacred** — a670a12 guard-5 preserved |
| **P5 same split unit** (never merge across pinned TU boundary) | **NEW** | prevents dtk "split ends within symbol" BUILD FAILURE |
| **P6 `JEFF_MERGE_PROTECT`** external-protect map | **NEW / load-bearing** | without it over-fires by 2 (`MidiReader::_M_erase`, `BandCharDesc` op-delete) → +65/−2 |

Greedy forward chain-merge (census: 272 multi-frag chains, longest 9). Runs AFTER leaf synthesis
(its fragments are the input), BEFORE prune. Grows S1 to run end; strips absorbed fragments to
`__MERGED_<name>` size-0 `Unknown`/`Stripped` (`xex.rs:1332-1351`) — hence 0 `__MERGED_` lines
survive into committed `symbols.txt` (stripped symbols aren't serialized).

### Q5 — ".pdata is symbol-table-driven, do NOT rewrite .pdata bytes"
**CONFIRMED correct.** Pass touches only `obj.symbols` (grow S1 / strip tails); never
`obj.pdata_funcs` or `except_data`. Commit: "Pure symbol-layer … emitted `.pdata` is unchanged."
Consistent with CLAUDE.md (`.pdata` re-derived from symbol table every split). **Corollary:** the
doc's validation gate "emitted target `.pdata` entry count drops by exactly the merge count"
(L337-338) is **INAPPLICABLE** — the merged fragments were never `.pdata` entries (they lived in
gaps), so `.pdata` output is unchanged, not decremented.

---

## (c) AddRoll CLOSED? — **YES**, hard evidence.

- `config/45410914/symbols.txt`: `fn_826976E0 = .text:0x826976E0; size:0x24` (**36 bytes**);
  the second fragment `fn_826976F0` is **absent** (merged away).
- `build/45410914/report.json` (fresh, Jul 30 00:42): unit `default/band3/game/Stats`,
  `?AddRoll@Stats@@QAAX_N@Z`, **size 36, fuzzy_match_percent = 100.0**.
- **Doc address correction:** doc cited `fn_826791B0`(16B)+`fn_826791C0`(20B) — those were
  **wrong/approximate**; `fn_826791C0` is an unrelated 112B function. The real AddRoll fragments
  were `fn_826976E0`+`fn_826976F0` (per memory), now merged to the single 36B body.

---

## (d) Census size + how many merged

Census already run (2026-07-16 §h; recorded in commit + `project_jeff_class2_merge`). Not re-run
(read-only; recorded numbers authoritative; the env-gated in-tree census tools `fc5d2af` cover
Class-1/Class-4, not Class-2):

- **Family / candidate pairs:** ~**1,789** adjacency+fall-through+zero-xref pairs.
- **Merge groups (runs):** ~**1,414**, of which **272** are multi-fragment chains (longest 9).
- **Fragments absorbed:** **1,727** (total_functions 71,123 → 70,273 at landing; currently 69,367
  after later passes).
- **~1,168** unpinned-gap merges have no immediate match but improve the fingerprint-ID surface.

So the pass merged the whole detected family, not just AddRoll — 1 confirmed match close +
large ID-surface cleanup.

---

## (e) Honest-floor-priced actual gain

- **AddRoll: verifiably REAL.** Single 36B supply-backed body, 100.0 in report, contributes
  `matched_code` — an axis-1 (supply) gain, not a reloc-masked twin.
- **The broader +67 predates the pricing rule** (`Δ(matched − masked_equal)` is from
  `project_honest_floor_2026-07-29`; Class-2 landed 2026-07-17). At landing it was priced by the
  then-authority: `report.json` `match_percent_normalized==100` **set-diff** (+67 add, 0 loss;
  byte-stable over 3 re-splits).
- **No evidence any of the +67 is masked_equal churn.** The mechanism (reuniting real fall-through
  bodies so a whole compiled body pairs against a whole target) does not mint reloc-masked twins.
  The honest-floor audit attributes the current `masked_equal_functions` to a **different**
  population — the ~1,517 pass-2b **surplus funclets** — not to class-2 merged leaf bodies.
  Current `report.json`: `matched_functions=40,882`, `masked_equal_functions=1,509` (3.7%),
  honest-floor proxy `matched − masked_equal = 39,373`.
- **Conclusion:** the +67 moved the honest floor (real supply/identity gains); it was not
  masked_equal inflation. Exact per-landing Δ(matched−masked_equal) was not recorded (rule
  postdates it), but AddRoll is provably real and the confounded bucket is a disjoint population.

---

## (f) Residual Class-2 work — **NONE. DONE.**

- Doc-Class-2 pass landed (`7e49a38`+`7f69b9e`); rb3-xenon `eb4863cc` +67/0; AddRoll closed;
  symbols.txt byte-stable over 3 re-splits; `cargo test` 131 green, clippy clean.
- **Vein DRAINED (post-landing re-hunt, `project_jeff_fleet_binary_unreproducible_2026-07-26`):**
  21,910 named fns → 88 low-nonzero → … → **1 genuine residual case** (`CamShot::Disable`, ~40B),
  7 false positives (7 different failure modes) → "**do not fund another merge pass.**"
- 3+-entry / non-fall-through / EH-adjacent cases the pass excludes are **by design** (P4/P5
  guards) and were confirmed empty or unfixable by census; not residual Class-2.
- Doc Class 1 (terminatorless) and Class 3 (except_data) — **out of this scope** — are both
  separately NO-GO (census-settled).

### Two housekeeping notes (not action items for Class-2)
1. **Fleet-binary drift (FYI):** live `../jeff/target/release/dtk` reports version **`57b52d6`**
   (`dtk 1.9.2`, branch `laneAF-va-fragments`), while jeff git HEAD is `b50881e`. `57b52d6`
   **is a descendant of both `7e49a38` (Class-2) and `b50881e` (Class-4)** plus the scattered-VA
   fix — verified via `git merge-base --is-ancestor`. So the live binary **contains** the
   Class-2 pass; the drift is a benign superset (scattered-VA commit measured match-neutral). This
   is the documented `project_jeff_fleet_binary_unreproducible_2026-07-26` condition, not a
   Class-2 regression.
2. Doc STATUS banner ("2026-07-12: design — not yet implemented") is **stale** for Class 2.
