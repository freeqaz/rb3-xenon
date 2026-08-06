# Relocation-name audit — alignment with decomp-synth, 2026-08-06 (lane WS-4)

> **STATUS: current.** Sibling of [`RULER_CHANGE_2026-08-02.md`](RULER_CHANGE_2026-08-02.md).
> This lane **aligned and fed**; it invented nothing. Everything below was run,
> not read, except where it says otherwise.

decomp-synth ran an independent audit of `objdiff`'s default
`functionRelocDiffs=none` (`decomp-synth/docs/reloc-name-blindness.md`) and
concluded the ruler credits a `bl` to a *different callee* as EQUAL. That is not
news here — this repo has run the `name_check` ruler for a week and censused the
class across lanes CV-4, CW-2, CX-*, CY-4. What the audit **supplied** is a
classification of all 12,679 (T, B) disagreements this tree produces at
`name_check`, with a body verdict attached to every one. This doc records what
that is worth, what it is not, and what landed.

## 0. Quote a ruler AND a tree state next to any absolute

Two things make a bare absolute meaningless here.

**Absolutes are tree-sensitive, and the ruler is not why.** Re-measured at the
pinned ruler, `matched_code_percent` read `40.755680 @none` / `28.866774
@name_check` where the audit doc records `41.0118` / `29.0281` — a ~0.26 pp drift
caused by one object (`build/45410914/src/system/char/CharIKFingers.obj`) being
rebuilt in between. Every *relational* claim held exactly.

**Version strings cannot identify the ruler.** `~/.local/bin/objdiff-cli` and the
fork build both report `objdiff-cli 4.2.3`; only the fork supports `name_check`.
Pin by sha256.

| | value |
|---|---|
| ruler | `bin/objdiff-cli` → `../objdiff/target/release/objdiff-cli`, sha256 `ca2be75232767f53…` |
| ruler, before first measurement / after last | `ca2be7523276…` / `ca2be7523276…` — **unchanged**, and `ab_measure.py` re-checked it across both legs and would have REFUSED on a swap |
| tree | `a236686e` for the audit; `e8fe5238`–`fc4d981a` for this lane's A/B |
| `matched_code_percent` **@none** | `40.755680` |
| `matched_code_percent` **@name_check** | `28.866774` |
| `matched_functions` @none / @name_check | `44,234` / `44,231` |
| `fuzzy_match_percent` @none / @name_check | `46.634304` / `46.634370` |

## 1. The honest band: **[0.23, 4.00] pp**

The raw `name_check` flip withdraws **11.89 pp** of `matched_code` (40.7557 →
28.8668). Almost none of that is defect.

| body verdict over the 12,679 charged pairs | pairs | share |
|---|---:|---:|
| **FOLD** — identical machine code | 7,048 | 55.6% |
| **FOLD (via thunk)** | 834 | 6.6% |
| GENUINE — different size | 3,169 | 25.0% |
| GENUINE — same size, different code | 187 | 1.5% |
| non-call / one-sided (data relocs) | 1,000 | 7.9% |
| unresolved | 441 | 3.5% |

**62.2% benign, 26.5% genuine.** Rolled up to functions the GENUINE stratum is
**2,863 fns / 422,804 B ≈ 4.00 pp** — the **upper** bound. The **lower** bound is
this repo's own adjudicated figure over the whole at-100 charged population:
**298 fns / 25,920 B / 0.2425 pp** (lane CW-2, `34017f74`), which superseded
CV-4's *estimate* of 353 fns / 24,836 B / 0.23 pp (`34b44dd6`).

> **`matched_code` is overstated by between 0.23 pp and 4.00 pp @none against a
> reported 40.7557 pp — 0.6% to 9.8% relative.** Not the ~29% a raw `name_check`
> flip implies, and not the ~41% the NameCheck commit message implies.

The band is wide because a body difference has two causes and no body test can
separate them: **our source calls the wrong function**, or **the retail symbol
map mis-attributes the name at the destination VA**. This repo's entire
`map(...)` lane series exists to repair the latter. ⇒ **GENUINE is an upper bound
on defects, never a defect count.**

Do not quote `35.51% → 20.96%` from the NameCheck commit; the relative drop does
not reproduce, most plausibly because `icf_aliases.map` has grown since.

## 2. The instrument CV-4 named was built **twice**, and neither supersedes the other

The audit doc says CV-4 "explicitly left open the cross-binary test … that was
not built". **That reading is four days stale.** Lane CW-2 built it on
2026-08-02 as [`tools/xbin_adjudicate.py`](../../tools/xbin_adjudicate.py)
(`cc653da0`, `34017f74`); the audit was reading CV-4's commit and not CW-2's.

| | `tools/xbin_adjudicate.py` (CW-2, here) | `probe_icf_foldtest.py` (decomp-synth) |
|---|---|---|
| retail side | dtk split objs, **plus** all 57,733 `.pdata` bodies scanned by CONTENT (ch2, map-independent) | PE image bytes at `addr(T)` |
| masking | **exactly the relocated field**, from OUR OWN reloc table; REL24 keeps `opcode\|AA\|LK`; an unknown reloc type is **REFUSED** | **every D-form displacement and every branch displacement, unconditionally** |
| reloc target names | compared | **never read** |
| thunks | not chased | one level, either side |
| anti-vacuity guard | ≥4 words AND relocated < 50% of body | **none** |
| `UNKNOWN` | explicit, never a fallthrough | only "not in map" / "not compiled" |
| calibration | ch1 positive control 96.67%/15,509; length-conditioned null 4.24% ⇒ 22.8×; ch2 recall 79.5%, random-offset null 1.1–2.2% | none published |

⇒ **The decomp-synth probe is a supply instrument, not an adjudicator.** Its
comparator discards immediates that `xbin_adjudicate` deliberately preserves
*because discarding them manufactures benign verdicts* — the direction this
project's standing directive calls worse than a lower metric — and it never reads
relocation target names, which is precisely the template-twin hole
`relocs_agree` exists to close (`vector<Foo>::erase` and `vector<Bar>::erase`
have identical machine bytes and differ **only** in the destructor they call).

**Measured corroboration.** Of its FOLD verdicts that clear
`icf_alias_build.py`'s hard gates, this tree's strict T1/T2/T3 adjudicators
accept **473 of 2,043 = 23.2%**. **525** of the rejects are
`reject_RELOC_TARGETS_DIFFER` — identical masked bytes, *different callee names*.
Treating the external verdict as a fourth evidence tier would have landed all
2,043 and silently manufactured 1,570 aliases.

⚠ Corollary for reading its *decide rate*: on the class-(b) **shape** (our callee
absent from `target_symbol_map.json`) the probe reaches a FOLD/GENUINE verdict on
**7,172 of 8,353 records = 85.9%**, where CW-2 + CY-4 reach one on 29.7% of class
(b). **That gap is the masker, not better evidence.** A DECIDED from a coarse
comparator is worth less than an UNKNOWN from a strict one.

### CV-4's "(b) backlog" residual, kept current

| lane | commit | class (b) | still UNKNOWN |
|---|---|---|---|
| CV-4 | `34b44dd6` | 1,223 fns / 159,276 B | all of it ("unadjudicable") |
| CW-2 | `cc653da0`,`34017f74` | 1,174 fns / 151,896 B | **1,018 fns / 129,544 B** (86.7%) |
| CY-4 | `57550c2b` | 1,177 fns / 151,932 B | **717 fns / 115,568 B** |

**717 fns / 115,568 B is the current residual** = 60.9% of (b)'s functions but
**76.1% of its bytes** — CY-4's decided rows are tiny adjustor thunks (179 alias
functions averaging 11.4 B), so the byte-weighted residue barely moved. Quote the
byte figure.

⚠ The 8,353 above is the class-(b) *shape* re-derived from the audit's own
population by "B absent from the symbol map". **It is not a join against CV-4's
actual class-(b) row set**, which is not a committed artifact. Doing that join is
the cheap next step and would let CY-4's 717-fn block be attacked directly. **Not
done in this lane.**

## 3. What landed — the FOLD residue

The 7,882 FOLD / FOLD-via-thunk records entered
`tools/icf_alias_build.py --xfold` as **candidate pairs only**, adjudicated by
the unchanged T1/T2/T3 tiers behind every existing hard gate (exactly one
map-resident survivor per group; every folded spelling referenced by ≥1 built
obj; the survivor and only the survivor named in the target objs; observed as
census noise).

```
272 groups /  512 aliases   before
521 groups / 1001 aliases   after      (+489 pairs, +453 attributable to --xfold)
```

`icf_alias_finder --validate`: **262 OK / 10 failing → 511 OK / 10 failing**, and
the FAIL set is **line-for-line identical**. ⚠ Those 10 pre-date this lane and
come from carried hand-verified groups, so `--validate` FAILs at HEAD and **is
not usable as a pass/fail gate** until they are adjudicated. It is usable as a
set comparison.

### The A/B — this is the deliverable

`tools/ab_measure.py`, ruler sha256 pinned across both legs.

| key | leg A | leg B | Δ |
|---|---:|---:|---:|
| **`matched_functions` @none** | 44,234 | 44,234 | **0** |
| **`masked_equal_functions` @none** | 22,864 | 22,864 | **0** |
| **`matched_code` @none** | 4,339,052 | 4,339,052 | **0 B** |
| **`matched_code_percent` @none** | 40.755680 | 40.755680 | **0.000000** |
| **`fuzzy_match_percent` @none** | 46.634304 | 46.634304 | **0.000000** |
| units at 100 (mpn / fuzzy) | 254 / 229 | 254 / 229 | **0 / 0** |
| units improved / regressed | — | — | **0 / 0** |
| `matched_code_percent` **@name_check** | 28.866774 | 29.679888 | **+0.813114 pp** |
| `matched_code` **@name_check** | 3,073,300 | 3,159,868 | **+86,568 B** |
| `matched_functions` @name_check | 44,231 | 44,231 | 0 |

**A default-key delta of exactly 0 is the invariant, not the result** — an alias
map feeds `symbol_equivalences`, which `reloc_eq` never consults at
`functionRelocDiffs=none`. Any nonzero default-key delta would have been a bug in
the feed. `+0.813114 pp` is 16× `ab_measure`'s stated ~0.05 pp `name_check`
build-noise floor, and reproduces `92e3951a`'s shape exactly (*"+1.72pp
name_check, default EXACTLY 0"*).

## 4. What landed — the GENUINE residue

[`relocname-genuine-worklist-WS4.tsv`](relocname-genuine-worklist-WS4.tsv) —
3,004 rows = **3,356 charged call sites over 1,506 distinct (T,B) pairs, 2,863
enclosing functions, 561 units**, each carrying the body-test verdict and the
(disagreeing) symbol-map verdict. Head-weighted: the largest single row is 501
sites, retail `??2CriticalSection@@SAPAXI@Z` against our `??2@YAPAXI@Z`.

Regenerable — `tools/relocname_genuine_worklist.py --out …`, `--check` re-cuts
and diffs. It REFUSES on an input sha256 mismatch, on a zero-row cut, and on a
collapsed COFF read (all three of which otherwise read exactly like a decisive
negative).

**Nothing from it has been applied. `git diff` over `src/` is empty, and that is
an acceptance criterion of this lane.** GENUINE is an upper bound; adjudicate a
row with `tools/xbin_adjudicate.py` or `tools/at100_adjudicate.py`, never from
the symbol map, and remember the upstream comparator over-produces FOLD so this
residue is if anything **short**.

## 5. Not done

- The join between the audit's pairs and CV-4's actual class-(b) row set (§2).
- Re-running `icf_site_census.py` / `icf_fold_evidence.py` at HEAD — the
  candidate enumerator still reads the 2026-07-31 `cd9_*` artifacts. That aged
  census is why regeneration proposed 8 landed aliases nowhere; `--merge` now
  carries a **never-adjudicated** member forward and drops a **refuted** one, so
  the failure is contained, but the inputs should be re-cut.
- Any map or source repair. This lane changed no `src/`, no
  `target_symbol_map.json`, no splits.
- `objdiff.json` is untouched — the default ruler stays `none` for every
  contributor, deliberately: the search gradient must stay reloc-blind or every
  ICF fold becomes an unreachable penalty the optimizer chases.

## See also

- [`RULER_CHANGE_2026-08-02.md`](RULER_CHANGE_2026-08-02.md) — the
  `masked_equal_functions` disclosure flip and the same-ruler guard.
- [`INSTRUMENT_DESIGN.md`](INSTRUMENT_DESIGN.md) §8 — the structurally-blind-metric
  shape this whole class belongs to.
- `tools/xbin_adjudicate.py` — the full instrument comparison and the (b) chain.
- `decomp-synth/docs/reloc-name-blindness.md`;
  `decomp-bench/archive/harvest/relocname-audit-2026-08-06/` (decomp-bench
  `6cc3caa6`) — the audit and its archived classification.
