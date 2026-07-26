# laneAN — the "84-byte cap" in objdiff funclet byte-signature pairing

Worker A. All numbers **measured** on 2026-07-26 unless tagged INFERRED.
Tooling: `docs/plans/laneAN/capscan.rs` (an objdiff-cli example that calls
objdiff-core's own COFF reader, so symbol sizes are byte-for-byte what the diff
sees) and `docs/plans/laneAN/objdiff-cap-lift.patch` (env-gated lift of the
base-side gate, applied to a **private copy** at `/home/free/tmp/objdiff-laneAN`
— the fleet binary `/home/free/code/milohax/objdiff/target/release/objdiff-cli`
was never rebuilt or swapped).

Code under test: `objdiff-core/src/diff/mod.rs`, `pair_funclets_by_bytes`
(~L1410) gated by `is_funclet_like` (~L815) on **both** candidate lists.

---

## 1. The cap is emergent, and it is a size-SET constraint, not a threshold

Base-side (our compiled objs) code symbols, over the **883 objs actually wired
into `objdiff.json`** (878 of which contain code symbols; 1,024 exist on disk,
141 are unwired and can never participate in a diff):

| pool | count | max size | >84 B |
|---|---|---|---|
| all code symbols | 441,850 | 15,844 | 84,890 |
| `is_funclet_like` symbols | 73,182 | **484** | **65** |
| ├ `__unwind$N` | 67,703 | 68 | 0 |
| ├ `__catch$N` | 3,579 | 104 | 25 |
| ├ `??__F*` | 1,510 | 68 | 0 |
| └ `??__E*` | 390 | **484** | 40 |

Largest funclet-like base symbol: `??__EgPropPaths@@YAXXZ` (484 B, `Object.obj`).
99.91 % of the pool is ≤ 68 B.

So **"no function larger than 84 B can score" is not literally true** — but it is
true in spirit and the real constraint is stronger than a threshold: passes 1/2/2b
need *exact* masked-byte equality and pass 3 needs *equal size*, so a target
function of size N can only score if its own unit's base obj contains a
funclet-shaped symbol of **exactly** size N. The realized size set is
`{12,16,…,68}` ∪ a thin `__catch$`/`??__E` tail
`{72,76,80,84,88,92,96,104,108,116,124,132,140,148,164,192,252,260,268,484}`.
The 65 symbols above 84 B are all `??__E` (40) or `__catch$` (25) — and `??__E`
only entered the pool in commit `e5987fb`, which added `??__E`/`??__F` **by name
with no size test**, silently raising the base-side max from 104 B to 484 B.

**Sizing correctness (the `$M<N>` gotcha).** Confirmed real: `Object.obj` carries
604 class-6 (`IMAGE_SYM_CLASS_LABEL`) `$M<N>` symbols, e.g. `$M41439` at +20
inside the 92-byte `??_GMessage@@UAAPAXI@Z`. A naive "size = next symbol's
address" rule would size that function 20 B. objdiff does **not** have the bug:
`infer_symbol_sizes` (`obj/read.rs:295`) skips to the next `Function`/`Object`
symbol when sizing a function, so labels are stepped over — measured: 0 `$M*`
symbols appear as sized code symbols and `??_GMessage` is sized 92. My scan uses
objdiff's reader directly, so these numbers carry no independent sizing risk.

## 2. Is the base-side gate load-bearing or incidental?

**Provenance.** Introduced by `b01e3ef` *"diff: pair MSVC EH funclets by masked
byte signature"* (2026-05-27), which applied the same predicate to both sides.
Neither the commit message nor any comment justifies the *base-side* restriction
specifically; the doc comment on `is_funclet_like` (L807-814) only enumerates
which names participate and explains the `fn_`/`??__E`/`??__F` entries. Later:
`18401be` locked the behaviour in tests, `e5987fb` widened the name set.
Verdict: **incidental to the algorithm, load-bearing for honesty.** Nothing
breaks mechanically if it is lifted — it is the only thing confining a
name-blind byte-equality fallback to compiler-generated code where identity is
not asserted.

**Which passes get dangerous, measured (precision = agreement with the
independent name for that VA in `scripts/target_symbol_map.json`, excluding the
1,232 VAs the map itself flags `_bijection_arbitrary`/`_icf_arbitrary`):**

| pass | lifted pairs | audited | precision |
|---|---|---|---|
| 1 — exact, unique on both sides | 12,634 | 12,166 | **98.2 %** |
| 2 — exact, ambiguous greedy zip | 5,430 | 3,434 | **36.3 %** |
| 2b — exact, many-to-one overflow | 203 | 90 | **43.3 %** |
| 3 — same-size fuzzy ≥50 % | 2,010 | 949 | **66.7 %** |

Precision by size for the same population: >44 B 86.4 %, >84 B 89.1 %,
>128 B 94.2 %, >256 B 95.5 %, >512 B 99.5 % — but note that within pass 1 alone
size barely matters (98.2 % at >44 B, 98.3 % at >84 B, 98.9 % at >128 B). The
size correlation is mostly *composition*: small symbols are disproportionately
pass-2 ambiguous.

**So the intuition inverts.** Large sizes make the *pairing* more trustworthy,
not less — a 5,036-byte instruction-exact hit (`fn_82BB8648` ↔
`rijndael_ecb_decrypt`) is not a coincidence. What is dangerous is **ambiguity**
(pass 2/2b), which is heaviest at small sizes, plus pass 3's fuzzy matching
(which does not produce strict matches anyway: 0 of its 1,088 pairs on current
objs reached 100 %).

**Interaction with `function_reloc_diffs: None`** (`objdiff-cli/src/cmd/report.rs:381`):
severe, but not in the way expected. Requiring reloc-descriptor equality
(`named_symbol_signature`) as an honesty gate **does not work on this target**:
only 1,896 of 20,277 lifted pairs (9.3 %) have equal reloc descriptors, because
the target side's reloc targets are dtk's anonymous `fn_<VA>` names — they can
never equal our mangled base names except for externally-fixed symbols
(`__savegprlr_*`, `__CxxFrameHandler`, …). And its precision is only 91.6 %,
*worse* than plain pass-1 uniqueness. Measured A/B: `lifted-strict` yields only
+1,692 strict on the HEAD-config objs vs +18,456 unrestricted. Reloc equality is
therefore the wrong gate here; **uniqueness is the right one.**

The masked-bytes rule also over-zeroes relative to the diff: `funclet_signature`
blanks the whole 4-byte instruction word at each reloc, including register
fields, so masked equality is *weaker* than 100 % diff equality (INFERRED, but
consistent with gains ≈ sim-100 pairs ±1 %).

## 3. Static estimate (computed before any build)

Target-side anonymous `fn_<8hex>` symbols whose masked signature exactly equals
some base code symbol in their **own unit's** base obj:

- 20,397 total; 19,051 already have a funclet-shaped base partner (eligible today)
- **1,346 newly eligible** if the base-side gate is lifted; **334 of those >84 B**
- newly-eligible max size 472 B; only 20 of the 334 have reloc-descriptor equality
- newly-eligible size histogram: ≤44 B 759, 44-84 B 233, 84-128 B 216,
  128-256 B 88, 256-512 B 50

This estimate was run against **main's** target objs. It **undercounts by ~14×**
against the worktree A/B below, and the reason is itself a finding: main's
`config/45410914/symbols.txt` + `scripts/target_symbol_map.json` are ahead of
`HEAD`, so main's freshly re-split target objs already carry mangled names where
HEAD's do not (e.g. `default/aes` has 3 code symbols in main and a 5,036-byte
`fn_82BB8648` at HEAD). **Anything that measures the anonymous-`fn_` pool is
sensitive to how recently the target objs were re-split.**

## 4. Measured A/B (strict = `match_percent_normalized == 100.0`)

Same splits, same objs within each table; separate `-o` path per leg so each has
its own `report.cache`; the patched binary with the gate off is **byte-identical
in output** to the stock fleet binary (`md5sum repA.json == repB.json`) — a clean
control.

**(a) `/home/free/tmp/wt-laneAN-cap`, objs re-split from `HEAD` config**
(baseline 19,414 matched / 69,410 total):

| config | matched | Δ | lost | gains >84 B | gains ≤44 B |
|---|---|---|---|---|---|
| lifted, all passes | 37,870 | **+18,456** | 0 | 10,706 | 4,161 |
| lifted + reloc-descriptor-strict | 21,106 | +1,692 | 0 | 448 | 962 |
| lifted + pass-1-only | 32,045 | +12,631 | 0 | 8,733 | 1,707 |
| lifted + pass-1-only + min 48 B | 30,338 | +10,924 | 0 | 8,733 | 0 |
| lifted + pass-1-only + min 88 B | 28,147 | +8,733 | 0 | 8,733 | 0 |

**(b) main repo's current objs (read-only run, baseline 36,069 / 69,405):**

| config | matched | Δ | lost | gains >84 B | gains ≤44 B |
|---|---|---|---|---|---|
| lifted, all passes | 37,300 | **+1,231** | 0 | 318 | 699 |
| lifted + pass-1-only | 36,534 | **+465** | 0 | 131 | 265 |

Pass split of the 1,231 current-obj gains: pass 1 = 465, pass 2 = 699,
pass 2b = 18, pass 3 = 0 (1,088 fuzzy pairs formed, none reached 100 %).
Every leg lost **0** functions.

Applying the measured per-pass precision to the current-obj gains: ≈ 719 of the
1,231 (58 %) would be true identities and ≈ 512 (42 %) attribution-arbitrary
(INFERRED — extrapolation of §2's rates).

**Table (a) vs (b) is the headline honesty fact:** on HEAD's objs the lift is
worth +18,456; on main's *current* objs, after the laneAK/AM/AL name-side waves
have already harvested the same population through
`symbols.txt`/`target_symbol_map.json`, it is worth +1,231. **The lift is
overwhelmingly a duplicate of work the project already does on the name side —
but does it silently, with no manifest.**

Caveat on the precision audit: `target_symbol_map.json` is not a fully
independent oracle. Entries created by within-unit byte-identity "reveal" agree
with a pass-1 byte pairing by construction, so 98.2 % is an upper bound. The
*disagreement* rates (pass 2 at 63.7 %) are the load-bearing half and are not
subject to that circularity.

## 5. Judgement — do not lift the gate as-is

Recommendation: **do not lift it in the shared objdiff.** Reasons, in order:

1. **The yield today is +1,231 (+3.4 %), not +18,456.** The large number is an
   artifact of stale target objs; the name-side pipeline already captures this
   population, and does so *with an audit trail* (`_bijection_arbitrary`,
   `_icf_arbitrary`, `_splits_fill_unresolved` manifests in
   `target_symbol_map.json`). A gate lift would re-harvest the same functions
   invisibly, and the `_bijection_arbitrary` doctrine would be lost.
2. **58 % of the yield arrives through the ambiguous passes**, whose measured
   precision is 36-43 %. Those pairings are byte-true modulo relocations but
   their *identity* is a coin flip — exactly what the project has repeatedly
   decided must be disclosed rather than absorbed into the score.
3. **`function_reloc_diffs: None` makes it unauditable after the fact**: two
   symbols with identical instruction shape and different callees/strings score
   a true 100 %, and reloc-descriptor equality cannot be used as a corrective
   because target-side reloc names are anonymous (only 9.3 % ever agree).

**If it is lifted anyway**, the gate that the data supports is: base-side accepts
any Code symbol, **pass 1 only** (unique on both sides, `right_lifted_in`
excluded from passes 2, 2b and 3) and **minimum size 48 B**. That configuration
measures 98.2 % precision, 0 losses, and +465 on current objs (+10,924 on stale
objs). It should additionally (a) keep `masked_pairing: true` so every such pair
is disclosed as `masked_equal_symbol`, and (b) emit the pair list so the VAs land
in a manifest the way the name-side waves do. Do **not** ship the unrestricted
lift, the fuzzy pass 3 (0 strict gains, 1,088 spurious pairings), or
reloc-descriptor-strict mode (wrong predicate for this target).

`docs/plans/laneAN/objdiff-cap-lift.patch` implements all of this behind
`OBJDIFF_FUNCLET_GATE` / `_P1ONLY` / `_MIN` / `_NOFUZZY` / `_DEBUG`, defaulting
to **stock behaviour when unset** — it is safe to apply to `../objdiff` without
changing any fleet result, but it has deliberately NOT been applied there.
