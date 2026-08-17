# W36-INSTRUMENT — a guard that had never fired, and a pricer that priced the wrong body

**2026-08-17, branch `w36-instrument`, worktree `~/tmp/wt-w36instrument`, from
`1fde5496` on `grounded2-restoration`.** Work is instrument correctness; no
bytes were sought and none were taken. Ruler `name_check`, resolved at runtime
from `report.json`'s own `provenance.diff_config`.

**Headline: the project's ONLY admission gate into the COMPLETE verdict has
never once rejected anything, and read as a working guard the whole time.**
That outranks the pricer fix, because it is the shape that closes veins.

Protocol note: the worktree was **built before any name-keyed analysis**, and
the renamer was asserted live (73 mangled `BandCamShot` names present in the
target obj, a fabricated name absent, `symbols.txt` line count equal to main's
225,965). A pre-renamer tree would have read every retail name as ABSENT and
agreed with every negative I was about to form.

---

## JOB 1 — `cascade_price`'s local-row charges were priced against the PRE-SWAP body

W29 found this, **quantified nothing**, and deliberately left it unfixed because
the one cell it hand-adjudicated came out in the *tool's* favour. It asked for a
fixture before anyone "corrected" it. This is that fixture.

### The defect is real, and it is 1,948 B on an 11-row edit

Measured as a PRE/POST pair on **one tree that already carries W29's source
fix**, so the map rename is the only variable:

| leg | `matched_code` |
|---|---:|
| PRE (map reverted to Ham) | 3,765,500 |
| POST (map as landed, Band) | 3,767,864 |
| **Δ** | **+2,364** |

Row-level closure: **+3,684 / −1,320 = +2,364 exactly**, nothing unattributed.
It decomposes into **local +1,552** and **cascade +812**.

| channel | old tool | truth |
|---|---:|---:|
| cascade | **+812** | **+812** ✅ exactly right |
| local | −396 | **+1,552** |

**100% of the error is in the local channel.** The tool predicted "no movement"
for four rows that crossed (+1,692 +140 +172 +12) and missed a −68 fall.

★ Two independent lanes' arithmetic closes on the same rows: drop the 1,692 B
`PropSync` (which only crosses once the source fix is in) and the remainder is
**+672** — W29's separately-measured map-only leg, to the byte.

### Mechanism

objdiff pairs target↔base **by name**. In the scatter-include case
(`BandCamShot.cpp` `#include`s `hamobj/HamCamShot.cpp`) **one obj defines both
spellings**, so a rename does not create a pairing — it changes **which of our
bodies objdiff compares**. Measured on the 11 rows:

* all **11 base bodies are BYTE-IDENTICAL** between spellings;
* **8 of 11 carry DIFFERENT RELOCATION NAMES**, and `name_check` charges on
  exactly those.

`?GetNumShots@HamCamShot` is charged `T=?ListNextShots@BandCamShot` vs
`B=?ListNextShots@HamCamShot`. The **target** spelling is already Band, so it is
not in the rename dict, so the old model carried the charge forward as
`PERSISTS` — while post-rename the Band body spells that callee Band and the
charge **clears**.

⚠ **My first structural probe was itself vacuous** and said `False` on all 11.
Relocations are `(offset, name, type)` tuples and I extracted `r[-1]` — the
*type* int — so I was comparing type sets, not names. It was caught only because
the answer contradicted a prediction I had written down first. *Instrument your
instrument* applies to the audit as much as to the audited.

### Fix, and why it is not a guess

Byte-identical bodies make the two relocation lists positionally aligned, which
yields a **base-side** rename `{old_reloc_name → new_reloc_name}` applied to
`bsym` before every comparison. Every precondition is asserted; a failure
returns `SWAP_NOMAP` and marks the row **UNRELIABLE** — never a silently-empty
map, which would be indistinguishable from "no swap".

The two rows that *fall* are the interesting confirmation: both charge on an
**ICF-survivor callee whose alias group folds the Ham spelling but not the Band
one**, so the swap introduces the disagreement. The corrected model predicts
both, including the −68 the old tool missed.

### Controls

```
validate-swap                PASS  18/18 rows exact; local −1,552, cascade −812,
                                   total −2,364 == the measured A/B
validate-swap --self-break   RED   6 local rows fail, cascade UNCHANGED
vacuity guard                REFUSED (exit 2) on an injected unreachable row
validate  (W17, frozen)      PASS  4/4, −580 → the change is a genuine no-op
                                   for ordinary (non-swap) renames
selftest  (W29 size check)   PASS  0 FP / 19,100 green, fires 557/557
```

★ The self-break leaving the **cascade channel exactly right** while reddening
only the local rows is the cleanest available proof that the defect and the fix
are confined to one channel.

★ Five of the eleven local rows are expected **zero and stay zero**: they *wash*
(the Ham row vanishes at 100, a Band row appears at 100). They are this
fixture's nulls — a model that fires on them is wrong in the other direction.

★ The fixture's expectations are frozen from the **measured** A/B, never from
the tool's output, so it can convict the tool. Its population is the frozen 11
addresses in the fixture file, never derived from the model under test.

---

## JOB 2 — the audit

### ⛔⛔ THE `COMPLETE` ADMISSION GATE HAD NEVER FIRED (`scripts/orchestrator/mcp_server.py`)

Proved by execution, not by reading — running the guard's exact argv:

```
$ ./bin/objdiff-cli diff -p . '??0BeatMatchSink@@QAA@XZ' -c functionRelocDiffs=name_check
# Diff: public: __cdecl BeatMatchSink::BeatMatchSink(void)
- **Base Size**: 16 bytes
$ stdout.find("{")  ->  -1
```

objdiff-cli's default format is **markdown**. The guard omitted `-f json` (every
other objdiff call in the same file passes it), so `stdout.find("{")` was −1 on
every call; there was **no `else`**, so execution fell through and recorded
COMPLETE. A third fail-open sat underneath:
`except Exception: pass  # If check fails, allow the report through`.

Corroborated two independent ways:

* the guard is the **only** writer of `is_stub = 1` in the tree, and the column
  reads **0 on all 86,675 rows** of `decomp.db`;
* `scripts/reset_false_complete.py` exists **solely** to undo *"false COMPLETE
  functions caused by base_size=0"* at scale — the damage is on record.

⇒ `query_functions(is_stub=True)`, an advertised MCP parameter, structurally
returns **0 rows** and reads as a drained vein.

**Fixed**: add `-f json`; add an `else` that refuses; turn the bare `except` into
a refusal. The gate now **fails closed**.

```
complete_guard_selftest.py                PASS  rejects 2/2 discovered stubs,
                                                admits 2/2 real functions, and
                                                writes is_stub=1 for exactly 2
                                                rows — the first time that
                                                column has ever been written
complete_guard_selftest.py --self-break   RED
over-block smoke test                     0 / 25 genuinely-matching (fuzzy==100)
                                                functions wrongly refused
```

The selftest drives the **real** `_report_result` coroutine against a **copy** of
`decomp.db`. Testing a re-implementation is how the original defect survived.

★★ **The self-break reproduced W29's hazard verbatim, in a different tool.**
Spelled the obvious way — patch `subprocess` first, then discover controls — the
break also disabled the **discovery** probe (which needs `-f json` too), both
control populations came back `0 -> []`, and the run tripped its own **vacuity
refusal instead of producing a red**. The break is now applied *after* discovery
and the ordering is commented as load-bearing. General rule: **a negative
control must not be able to poison its own population.**

★ **Note the shape of the red, which is the repair's signature.** The repaired
guard fails **closed**, so a broken probe makes it over-BLOCK — loud. The
original failed **open**, so the same broken probe made it over-ADMIT — silent.
That asymmetry is why it survived for months.

⚠ **Latent hazard, recorded not fixed:** the whole guard sits inside
`if symbol and self.record_attempts:`, so `--no-record-attempts` disables the
COMPLETE gate outright. Nothing in the tree passes that flag today. (I hit this
myself — my first selftest constructed the server with `record_attempts=False`
and every control "passed" admission for a reason unrelated to the guard.)

### ⛔ The ungated `fuzzy == mpn` certificate is still live in `tools/w25_scope.py`

True as stated, vacuous as applied. Measured whole-binary **before** the fix:

| | rows | bytes |
|---|---:|---:|
| ungated PURE | 22,687 | 5,245,780 |
| of which `fuzzy == 0` (0 == 0) | **22,090** | **5,196,904** |
| genuine (fuzzy > 0, named) | 159 | 30,752 |

**97.4% by rows / 99.1% by bytes is the trivial case.** On the tool's own default
scope the bucket goes **319 → 13 rows / 596 B**, reproducing the briefed "309 of
319". Now gated on `fuzzy > 0`, with the vacuous class **split out and labelled**
rather than deleted — so the number stays visible instead of vanishing into a
smaller headline.

### ✅ `coff_bodies_ext.py` — the extent artifact WAS fixed in code, twice

Checked because six folds were wrongly withdrawn on it and later restored at
+1,728 B. `ff832b50` fixed the trailing-EH-prefix billing; `d586e8d5` then
corrected the fix's *gate* from a name predicate (`is the successor named
__unwind$/__catch$?`) to the `$EH` **marker**, because an interior prefix can
precede an ordinary function — measured at 3 of 6,395 interior prefixes. Not a
docs-only repair. It is also the reader `cascade_price` depends on, and Job 1's
byte-identical finding across 11 rows is a live corroboration that it is sane.

### ✅ `grep_binary_guard.py` runs and discriminates

`PASS (7 probes, 1 hazard warning)`; `--self-break` → `FAIL (1 check returned a
FALSE NEGATIVE)`. The shim is confirmed **live** in this shell. ⚠ Narrower
weakness, not fixed: only the `real grep -ao` check is actually falsified by the
break — the shim checks recompute their expectation from the swapped needle and
degrade to WARN, and they are skipped entirely where no shell snapshot exists
(i.e. in CI, the one place the guard is automated).

### Reported, not fixed — ranked by "would cause a wrong decision"

A systematic sweep (delegated, then spot-verified) found the same disease
throughout, with one cross-cutting pattern worth stating on its own:

> **Every one of these tools guards rigorously against a MALFORMED input and not
> at all against an ABSENT or EMPTY one.**

1. **`tools/icf_alias_finder.py --validate` passes on an empty alias file — and
   it is a CI gate.** `.get("groups", [])` → nothing classified → `n_bad = 0` →
   PASS. Twelve lines above, it refuses rigorously when the *obj* population
   looks wrong (`mangled < 1000 → return 2`). This guards the file where alias
   forgiveness is worth **~818 kB / 7.93 pp** of `matched_code`. A row floor is
   trivially available (it holds 1,529 groups).
2. **`scripts/harvest/check_regression_lock.py` reports `CLEAN` over an empty
   baseline** — if the `merge_commit` query selects nothing, the comparison loop
   never runs and the landing proceeds. It refuses on a *totally* empty snapshot
   but never checks the *selected* commit's set is populated.
3. **`tools/screen_gate.py` passes over zero screens.** `--only <typo>` silently
   `continue`s past unknown names → `all([])` → *"All screens ARMED… their
   negatives may now be believed."* Reproduced. This is the tool whose own source
   says *"a harness that cannot fail is exactly the bug it exists to prevent."*
4. **`scripts/analysis/ruler.py`'s selftest passes when the ruler is the loud
   FALLBACK** — with no `report.json`, the authoritativeness assertion is
   `[SKIP]`ped and `ok` stays True. The assertion is reachable only when it is
   already guaranteed.
5. **`tools/map_lint.py` prints `CLEAN -- no findings` for a `--unit` typo**, and
   its `load_map` silently drops keys that `icf_alias_finder` accepts (case
   mismatch on the `0x` prefix) — the two readers disagree about the same file.
6. **`scripts/validate_symbols.py`** hardcodes a TU0-era `.text` range and
   reports **2,284 phantom "invalid" entries**, always exiting 1 — a false alarm
   in the opposite direction, and per the standing rule all TU0-era addresses are
   invalid since the TU5 flip.

**Verified NOT defective** (read deeply enough to be sure, so nobody re-audits):
`tools/symbols_fixpoint_guard.py`, `tools/native_build_gate.sh`,
`tools/comdat_fold_gate.py`, `tools/ab_measure.py`, `tools/scope_map.py`,
`tools/gate_liveness.py`, `scripts/harvest/land.sh`, `tools/noobj_census.py`.
⚠ Note `ab_measure`'s `f.get("fuzzy_match_percent", 0.0)` default is **correct**,
not an instance of the disease: 22,157 of 69,226 report rows legitimately omit
that key (protobuf-JSON drops defaults) while 0 omit `match_percent_normalized`.

---

## Measurement discipline

**`ab_measure --from-dirty` REFUSED**, `kinds=['NONE']`, *"patch touches no
build-relevant path — this A/B would measure nothing."* That is the correct
verdict for a tools-only change and is **stronger than a Δ0 reading**, which an
absent-vs-absent comparison cannot earn. The tree was restored and verified.

No `src/**` touched by any commit, so the native gate is not triggered.

## What I did NOT do

* **Did not fix items 1–6 above.** Each is a small patch, but each needs its own
  proven-red control, and shipping six unproven guards would be the disease.
  Item 1 (`icf_alias_finder`) is the one to take next: it is a CI gate over the
  ~7.9 pp forgiveness mechanism.
* **Did not touch the `AT_LIMIT` admission path.** `report_result(status=
  "at_limit")` has no guard written at all — the mirror of the COMPLETE defect,
  but a real guard there needs a definition of "at limit" that objdiff can
  supply, and per CLAUDE.md an `AT_LIMIT` label on a `diff_arg`-only row carries
  no information. That is a design question, not a patch.
* **Did not add a guard for objdiff's r31-relative "wrong field?" hint.**
  Establishing that it misfires needs a measured false-positive rate against
  compiler-verified layouts, which is a lane of its own.
* **Left `grep_binary_guard`'s narrower weakness in place** (above) — the tool's
  headline verdict does discriminate, so the fix is not urgent and I would
  rather report it than half-fix it.

## Reproducing

```bash
python3 tools/cascade_price.py validate-swap --project-dir <wt>
python3 tools/cascade_price.py validate-swap --project-dir <wt> --self-break
python3 tools/cascade_price.py validate      --project-dir <wt>   # W17, 4/4
python3 tools/cascade_price.py selftest      --project-dir <wt>
python3 scripts/orchestrator/complete_guard_selftest.py --project-dir <wt>
python3 scripts/orchestrator/complete_guard_selftest.py --project-dir <wt> --self-break
python3 tools/grep_binary_guard.py ; python3 tools/grep_binary_guard.py --self-break
```
