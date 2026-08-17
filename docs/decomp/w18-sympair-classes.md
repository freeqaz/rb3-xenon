# W18-SYMPAIR — the two unexamined sympair classes, adjudicated on retail bytes

**2026-08-17, branch `w18-sympair`, baseline `a62fd3d9`.** Baseline reproduced
**exactly** as briefed: **44,485 fns / 3,747,732 B / 36.312897% / honest 21,585 /
total_code 10,320,664 / total_functions 69,226**, ruler `name_check` (read from
`report.json`'s own `provenance.diff_config`, not assumed).

**Shipped: +148 B / Δfns 0 / Δhonest 0**, predicted exactly (`5eb0333b`).
Everything else is **refused with the reason recorded**, which is the point of
the lane.

W7-SYMPAIR triaged its queue and left two classes untouched:
`MIXED/UNKNOWN` 76,060 B and `ALL_OURS_UNMAPPED` 158,300 B. Re-measured here at
`a62fd3d9` they are **75,520 B (411 rows)** and **157,084 B (720 rows)** — W7's
figures held within 1%, the shrinkage attributable to W12/W14/W15 landing.
Combined: **1,131 rows / 232,604 B.**

---

## Headline: 0.34% of the slice is source-work

| mechanism | rows | bytes | share |
|---|---:|---:|---:|
| **A. PROVEN FOLD** — our callee's body **IS** retail's code at that address | 564 | **139,464** | **59.96%** |
| **B1.** our callee's code is **ABSENT from retail entirely** | 288 | 62,820 | 27.01% |
| **C.** we compile no body for the callee — no evidence obtainable | 99 | 16,924 | 7.28% |
| B2. callee **present in retail but unnamed** (identification gap) | 39 | 4,592 | 1.97% |
| B3. unsearchable (first word relocated / too small) | 17 | 4,184 | 1.80% |
| **D1. the ROW's map name is REFUTED** by our own call | 80 | 3,268 | 1.40% |
| **D2. genuine SOURCE defect — our source chose, and chose wrong** | 25 | **792** | **0.34%** |
| D4. retail's name at the target address is not body-confirmed either | 11 | 328 | 0.14% |
| B4. ambiguous (callee bytes match >8 retail addresses) | 6 | 224 | 0.10% |
| D3. our call unrelated to the row | 2 | 8 | 0.00% |
| **TOTAL** | **1,131** | **232,604** | 100.00% |

Rows and bytes **sum exactly** to the two classes; the tool asserts it and
refuses otherwise. The roll-up is per-ROW and takes the **worst** pair, because
`matched_code` is all-or-nothing per row.

⇒ **Reachable by source work: 792 B, 0.34%** (952 B before this lane spent 148 of
it). Reachable by map/pin work: D1+D4+B2 = **8,188 B, 3.52%**, and D1 proves
existence without proving assignment. Everything else — **93%** — is a proven
fold, a callee that is not in retail, or a callee we cannot even test.

★ This is **W5-CEILING's lesson running again at 100× the ratio**: W5 predicted
20–45 kB and measured 6,984 B. A 232 kB slice bought 148 B.

---

## 1. What is actually in them (by mechanism, not by size)

### A. PROVEN FOLD — 139,464 B, 59.96% — NOT source-reachable

Our compiled callee is byte-identical to retail's code at the address retail's
call targets, modulo relocated words. Retail folded the two spellings; the map
can spell only one. **No source mutation can close these** — the only mechanism
that reaches them is `scripts/symbol_aliases.json`, and expanding alias
forgiveness is a **policy decision this lane does not own**: ~22% of everything
counted as `matched_code` already rests on it (818,416 B / 7.93 pp, lane
ALIAS-2), and `scripts/icf_alias_groups.json`'s 1,407 ungated groups are ungated
deliberately.

⚠ **32.5% / 22.3% of the two classes' IDENTICAL bytes rest on ≤2 compared
words** (`evidence=VACUOUS` in the TSV): the callee is a tiny thunk that is
almost entirely relocation, so "identical" is nearly no evidence. This is
GROUNDED-1's already-known irreducible thunk stratum arriving from a different
direction — *which name the call site meant was destroyed by ICF itself*.

### B1. The callee is not in retail at all — 62,820 B, 27.01%

Our callee's bytes match **no** retail `.pdata` function start. We emit a
function RB3 does not have: inlined there, or a DC3-newer divergence. Not
cheaply reachable; it is body-port work on the *callee*, not a naming fix.

### C. No evidence obtainable — 16,924 B, 7.28%

We compile no COMDAT for the callee at all (imports, data symbols, TUs we do not
build). Nothing to test either way. Reported separately rather than folded into
a "not adjudicated" bucket, because *missing evidence* and *negative evidence*
are different findings.

### D. Real, adjudicated defects — 4,396 B, 1.89%

The only class where retail bytes say a defect exists, split by **whether our
source had freedom at the call edge**:

* **D1 (3,268 B) — the ROW's name is refuted, not our source.**
  `vector<T>::_M_fill_insert_aux` calls `T::T(const T&)` **by template
  construction**; a thunk branches to its own method. We cannot write anything
  else, so retail's row calling a *different* T's ctor means the address is not
  the instantiation the map named. Worked example: retail's
  `?_M_fill_insert_aux@?$vector@VMeshAO@OutfitConfig@@…` calls
  `??0Patch@BandCharDesc@@`, so that address is `vector<Patch>`'s, not
  `vector<MeshAO>`'s. **This is W7's FileCache anchor #1 reused, and it inherits
  W7's limit exactly: it proves the name is WRONG, never what is RIGHT.**
* **D2 (792 B) — genuine source defect.** Neither candidate is entailed by the
  row's own name, so our source really did choose. This is the only
  source-reachable class in the whole slice.
* D4 (328 B) — retail's own name at the target address is not body-confirmed
  either, so the charge is a map problem and our source is not implicated.

---

## 2. How much carries an identity hypothesis? — the W5-CEILING test, applied

Two answers, because "identity hypothesis" has a weak and a strong reading.

**Strong (a body-level verdict exists): 62.7%** — classes A + D, 143,860 B. For
these we can say *on retail bytes* whether our callee is the code retail calls.

**Weak (any adjudicable hypothesis at all): 37.3% have NONE** — B1+B3+B4+C =
84,152 B, where the callee is absent from retail, untestable, or ambiguous.

And the number that actually prices work — **the fraction carrying an identity
hypothesis that source work can act on — is 0.34%.**

⛔ **The name-similarity axis, tried first, does NOT carry an identity
hypothesis, and a control is what showed it.** Classifying each pair by the
relationship between the two demangled names looked promising until the same
classifier was run over populations of **known** folds:

| population | n | SAME_NAME | TEMPLATE_ARGS_DIFFER | SAME_METHOD_OTHER_CLASS | SAME_CLASS_OTHER_METHOD | DISJOINT |
|---|---:|---:|---:|---:|---:|---:|
| P1 applied aliases (proven folds) | 15,186 | 13.9% | **65.5%** | 2.3% | 7.4% | 10.4% |
| P2 ungated fold candidates | 7,917 | 8.9% | 32.9% | 4.0% | 5.3% | 48.6% |
| **P3 in-queue `FOLD_FANIN` pairs** | 801 | 2.2% | **55.3%** | **0.4%** | 2.6% | 39.3% |
| QU queue `UNKNOWN` pairs | 346 | 0.6% | 36.1% | **15.9%** | 19.1% | 27.2% |
| QO queue `OURS_UNMAPPED` pairs | 624 | 4.5% | 38.3% | 8.3% | 13.9% | 31.6% |

`TEMPLATE_ARGS_DIFFER` is *what a proven fold looks like* (65.5% / 55.3%), so
finding it in the queue says "fold", not "defect" — the shape that covers ~48%
of `MIXED/UNKNOWN` is the **least** informative one. `SAME_METHOD_OTHER_CLASS`
is depleted **~40×** in the in-queue fold control (0.4%) versus `UNKNOWN`
(15.9%), which *is* a discriminating shape — but following it produced a long
tail of **92 distinct class pairs**, overwhelmingly unrelated classes with
same-shaped methods. Its one systematic member is the **DC3-vs-RB3 spelling**
`BandCamShot::Target` ← `HamCamShot::Target` (2,192 B), i.e. **our map spells one
class two ways**; left unopened, see §4.

★ **The transferable point: a name-relationship census cannot price this
material, and you only find that out by running the classifier on a known-fold
population first.** Without P3 the 48% `TEMPLATE_ARGS_DIFFER` bucket reads like
a lead.

---

## 3. Proved vs refused

### Proved and landed — `5eb0333b`, +148 B, predicted exactly

`?UpdateVolume@SfxInst@@UAAXXZ` (148 B, `fuzzy` 99.86487, **`mpn` already 100**,
so earning zero bytes) had **exactly one charged site** and it was the callee
name. Retail calls `MoggClip::SetVolume`; we called `MoggClip::SetControllerVolume`.

Adjudicated on bytes, both legs independent: our compiled `?SetVolume@MoggClip@@`
is byte-identical to retail at the targeted address, **and** our
`?SetControllerVolume@` is byte-identical to retail at *its own* mapped address.
So both functions exist, both are correctly identified, and we called the wrong
one — not a fold, not a map defect. Corroborated by the DC3 oracle
(`dc3-decomp/src/system/synth/Sfx.cpp:78` writes `clip->SetVolume(...)`).

⚠ **The obvious edit made it WORSE and the negative result is the useful part.**
Plain `clip->SetVolume(...)` took the row from 37 instructions / 1 charge to
**40 instructions / 12 charges**, because `MoggClip::SetVolume` is `virtual`, so
MSVC emits a vcall where retail has a direct `bl`. The qualified
`clip->MoggClip::SetVolume(...)` reproduces retail exactly (37 instructions, 0
charges) — and that is the **established idiom in this very TU**: `SfxInst::Stop`
four functions above already writes `clip->MoggClip::Stop()`.

Measured: **Δcode +148 B, Δmatched 0, Δhonest 0**, 36.312897 → 36.314330.
`none` control **flat at +0** with graded +148: with `source` in the patch that
is the *wrong-callee-fix* signature, not the alias-suspect one (`ab_measure`
reported the alias check `NOT_APPLICABLE`, correctly).
`NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`

### Refused, with reasons

* **A, 139,464 B — refused as out of scope, not as unreachable.** It is a proven
  fold class and the mechanism that collects it is alias forgiveness, which is a
  policy call the coordinator owns. Adding ~139 kB of new alias groups would
  move the headline by ~1.35 pp while adding to the 22% of `matched_code` that
  already rests on forgiveness. **The evidence needed to do it safely is
  already computed and shipped in the TSV** (`A_PROVEN_FOLD` rows carry the
  target address and an `evidence` column) — but ~27% of those bytes are
  `VACUOUS`-evidence thunks and should not be admitted on this instrument alone.
* **D1, 3,268 B — refused on W7's exact ground.** The template/thunk constraint
  proves the row's map name is wrong; it does not say what the right name is,
  and renaming requires the destination obj to be able to define the
  replacement (W9's −180 B failure mode; W15's re-home-not-rename rule). 80 rows
  is a real queue for a lane that can do per-case assignment.
* **B1, 62,820 B — refused as mispriced-as-naming.** It is callee body-port
  work wearing a naming costume.
* **The `; reg` contamination — measured, not assumed: 1 row / 1,652 B.** See §4.

---

## 4. Instrument findings

1. **`tools/sympair_queue.py` counts a charge as a pure symbol pair iff the
   joined detail string *starts with* `SYM`** — so an instruction differing in
   both a symbol and a register is classified by **argument order**. Measured
   over all 2,526 swept rows: **42 charges / 20 rows** mix `register+symbol`,
   and exactly **1 row / 1,652 B** (`?RGGetChordName@@`, `default/RGUtl`) is
   called crossable while carrying a register diff it cannot fix by naming.
   0.31% of W7's 534,248 B headline, and **outside both classes examined here**
   — a real correction, deliberately reported small rather than talked up.
2. ⛔ **`llvm-undname` emits blank-line-delimited records, and a name it rejects
   yields a ONE-line record.** The natural "two non-blank lines = one pair"
   parser desyncs at the first rejection and mislabels every name after it. Here
   it produced a confident **100% UNDEMANGLABLE** on 1,624 names that had
   demangled fine. **The same bug is live in `tools/arity_screen.py:810`
   `demangle_batch`** — not fixed here (out of lane scope) but recorded. Parse
   records, and assert `missing == rejected`.
3. **An entailment test on mangled type tokens is vacuous for constructors.**
   `[VU](\w+)@` finds nothing in `??0MeshAO@OutfitConfig@@`, i.e. it silently
   fails on exactly the template-callee case it exists to catch, and every such
   site falls through to "source defect". Use demangled identifiers — and only
   the ones that **distinguish** the two candidates, since shared identifiers
   (`ObjDirItr` in both instantiations) make both sides read "entailed", which
   is the vacuous outcome in the other direction.
4. ★ **The actionable class shrank monotonically as the classifier got more
   careful: 3,308 → 1,484 → 1,016 → 792 B.** That trajectory is itself the
   finding — the class was largely an artifact of imprecise classification, and
   a lane that stopped at the first number would have briefed a 4× overstatement.

### A hypothesis that was refuted, and should not be re-run

**MAP_COINFLIP: 0 of 141 sites.** The `AccomplishmentCategoryCmp` ↔
`AccomplishmentGroupCmp` transposition and the `UIPanel::Enter`/`UIPanel::Draw`
pairs *look* like the map's `_bijection_arbitrary` picking arbitrarily between
byte-indistinguishable methods. Tested directly — compare retail@`at` against
retail@`ao` under the same relocation mask — and **retail's two functions differ
in every case**. The arbitrary-bijection explanation does **not** extend from
W7's `ALL_RECIPROCAL` class into these two.

---

## 5. What I did NOT do

* **No alias group was added** (§3). The 139 kB is sized and its evidence is
  shipped; the decision is not this lane's.
* **No map or pin edit.** Every D1/D4 row proves *existence* of a defect and not
  *assignment*, and no re-split was run, so nothing here is inert-but-claimed.
* **`FOLD_FANIN` (285,024 B) and `ALL_RECIPROCAL` (16,620 B) were not
  re-opened** — W7 triaged them and W12/W14/W15 drained the family class.
* **`tools/arity_screen.py`'s demangle desync was not fixed**, only recorded.
* **B2 (4,592 B, callee present but unnamed) was not pursued.** Naming an
  anonymous address has zero byte upside by construction (`name_check` already
  forgives placeholder targets); its payout is bug exposure (MAPID-1), and at
  4.6 kB it does not justify the identification work here.
* The **`BandCamShot`/`HamCamShot` map-spelling split (2,192 B)** is identified
  but not opened: it needs the RTTI-presence anchor (W14's `.?AVMoveMgr@@`
  method) and a decision about which spelling RB3 retail actually used, which is
  a map-wide consistency question rather than a per-row one.

---

## Reproducing

```bash
python3 tools/sympair_queue.py --project-dir <wt> --out-queue /tmp/q.tsv   # the queue
python3 tools/sympair_adjudicate.py --project-dir <wt> --queue /tmp/q.tsv  # this lane
python3 tools/sympair_adjudicate.py --project-dir <wt> --controls-only     # controls only
```

`sympair_adjudicate.py` **refuses (exit 2) if its own instrument fails to
discriminate** — the positive control on declared folds must exceed 90%
IDENTICAL (measured **99.9%**, 14,426/14,436) and the random-pairing null must
stay under 5% (measured **0.07%**, 1/1,500). The existence search carries its own
recall control (**96.8%**, 240/248 known-present callees recovered without being
told the address). Per-row output: `docs/decomp/sympair-adjudicated-W18.tsv`.
