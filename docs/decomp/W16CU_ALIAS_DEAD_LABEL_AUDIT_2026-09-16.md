# W16-CU — auditing W16-CP's `dead` column: the label is CORRECT, the reading of it was not

**Date:** 2026-09-16 · **Lane:** W16-CU · **Branch:** `w16-cu` · **Base:** `cd699bb8`
**Tree:** built and patched before any name lookup —
`[patch-state] OK: 1219 decomp, 3105 target objects match`, `tree_sha256=258439756d297603`.
Ruler `functionRelocDiffs=name_check`, objdiff 4.2.9, read from `report.json`'s own
`provenance`. Null control reproduced the baseline to the last digit in 3.05 s.

---

## 0. Verdict in one line

**`dead` is a LEDGER-STATE flag meaning "this spelling is not currently in any
group's `folded[]`" — it never was a claim about bytes.** It is therefore exactly
the *candidate-for-restoration* set, and W16-CT restoring one of those rows and
being paid +184 B is the label **working**, not failing. Nothing about CP's
adjudication needed reopening.

## 1. How the label was actually computed — settled by exact reproduction

The instrument was never found as a script because the column is not a
measurement; it is a one-line predicate over the ledger. Recomputed against
`scripts/symbol_aliases.json` at CP's own base `7ec1e249` and compared to all 456
rows of `~/tmp/w16cp_underpart_adjudicated.json`:

| candidate predicate | agreement |
|---|---:|
| **spelling ∈ some group's `folded[]`** | **456 / 456** ✅ |
| spelling ∈ some group's `folded[]` **or** is some `survivor` | 411 / 456 |
| the group's own `evidence` line "N census sites" > 0 | 307 / 456 |

The 45 rows that separate the first two are `repartitioned` singletons whose
spelling became a **survivor** elsewhere — that is the detail that pins the
predicate, and it is why "live somewhere" in the loose sense is the wrong reading.
CP's own §2 says so in words: *"spelling LIVE somewhere 152 / spelling live
NOWHERE (withdrawn-only) 304"*, which is the artifact's `LIVE` 152 / `dead` 304
split exactly.

⇒ **The hypothesis this lane was dispatched to test — "a liveness census that
enumerates charged sites by ROW NAME and skips `fn_` rows is blind to the
anonymous stratum" — is REFUTED. No census computed this column at all.** The
`fn_`-blindness mechanism is real in general (it is why `tools/alias_group_ablate.py`
exists), but it is not what produced `dead`.

## 2. Where the misreading entered, since that is the reusable lesson

Not with CP. CP used the column correctly and explicitly declined to price the
vein behind it (§5.1: *"This lane does NOT price that vein and nobody should
multiply it out… a real, unmeasured delta in both directions"*).

The misreading entered at **`ba713e02`**, which wrote *"WORTH ZERO BYTES, AND THAT
IS THE POINT… CP's adjudication artifact records the row as [… 'dead' …] —
liveness `dead`, 0 census sites… So re-admission forgives nothing"*, pre-registered
Δ0 on that basis, measured +184 B, and then concluded *"CP's liveness column
reopens"* — attributing a failed prediction to the instrument rather than to the
reading of it. The merge message and the roadmap inherited it.

★ **The durable shape: a ledger-state flag and a pricing claim are different
kinds of statement, and nothing in the artifact's vocabulary distinguishes them.**
`dead`/`LIVE` reads like a verdict about worth. It is a fact about installation
state. When a column's name is a metaphor, check the predicate before pricing on it.

## 3. CP's layer column, independently reproduced

`tools/alias_forgiveness_audit.Sides.verdict` re-run over all 456 rows in a
**different tree** (`w16-cu` @ `cd699bb8`), with anti-vacuity and both controls:

```
ANTI-VACUITY: target bodies 69432 (mangled 27603) | our bodies 95494
CONTROL+ known-live membership -> L1_T1        (expect a PROVEN layer)  OK
CONTROL- nonsense pair         -> NEEDS_SOURCE (expect NEEDS_SOURCE)    OK
REPRODUCTION vs CP: 456/456 agree
L1_T1 385 · NEEDS_SOURCE 29 · CONTRADICTED 26 · L2_RECURSIVE 8 · NEEDS_MAP_ID 7 · L5_INCONSISTENCY 1
```

**456/456.** CP's adjudication is sound and is not what went wrong.

## 4. The vein CP sized but did not price — now PRICED

Population: rows that are flat `L1_T1`, not installed in any `folded[]`, and whose
withdrawal record is still in effect = **245**, over 91 groups.

**3 are SELF-ALIASES** — the spelling is the **survivor of the very group carrying
its own withdrawal** (`ObjDirItr<SpotlightDrawer>` `0x824afa78`,
`ObjDirItr<BandList>` `0x8230b260`, `ObjDirItr<RndGroup>` `0x8243e108`, all
`dropped_singleton`). The name is already at that address in the map, so
"restoring into `folded[]`" is a no-op that would duplicate the survivor in its own
fold list. Measured: each pays 0. **Excluded. 245 → 242 restored.**
⚠ A first installer missed this because it checked only `folded[]` and never
`survivor`, and silently counted them as installs.

**Measured, per membership, by installing ONE at a time and diffing the crossing
set — no name matching anywhere, so an anonymous row is attributed exactly as well
as a named one:**

| folded spelling → survivor | pays | at the row |
|---|---:|---|
| `MakeString<unsigned int>` → `MakeString<const char*>` | **+940** | `RGTrainerPanel::HandleChordLegend` |
| `~PatchPair` → `~MeshPair` (`0x8229e1d8`) | **+404** | `PropSync<PatchPair>` +364 · `fn_8234A424` +40 |
| `MakeString<const char*,const char*,int>` → `MakeString<Symbol,…>` | **+228** | `CharacterCreatorPanel::AddGridThumbnails` |
| `String::contains` → `FixedString::contains` (`0x827be190`) | **+184** | `StringMatchesFilter` |
| `_List_iterator<RndMultiMesh::Instance>` ctor → `…<Voice*>` ctor | **+156** | `MeshAnim::_M_splice_insert_dispatch<…Instance…>` |
| `~vector<RndMorph::Pose>` → `~vector<DrivenPropertyEntry>` | **+132** | `RndMorph::~RndMorph` |

**6 payers of 242. Sum of individual payments = +2,044 B = the all-at-once total
EXACTLY ⇒ no joint dependencies.** 0 memberships caused any row to fall out,
consistent with an alias being pure forgiveness.

★ **Payment lands where each fold structurally predicts**: a `MakeString` overload
at a string-formatting site, `~vector<Pose>` at the destructor of the class holding
a `vector<Pose>` member, `~PatchPair` at `PropSync<PatchPair>`. That is the same
check W16-CT ran on its four thunks, and it is stronger evidence than the total.

## 5. The controls — both discriminate, and one caught itself being vacuous

**Permuted decoy (the load-bearing control).** An alias lifts `name_check` BY
CONSTRUCTION and the `none` ruler is flat there by construction, so neither can
clear a restore. The question a control *can* answer is whether payment is specific
to the adjudicated **pairing**. Both legs install 242 spellings into the **same 88
groups** from the **same spelling population**; only which spelling goes into which
group differs. A decoy built from random unrelated symbols would pay 0 vacuously
(they sit at no charged site); a derangement cannot fail that way.

| leg | map lines | Δcode | Δfns | crossed in | fell out |
|---|---|---:|---:|---:|---:|
| TRUE pairing | 8,383 (+242) | **+2,044 B** | +6 | 7 | 0 |
| PERMUTED decoy | 8,383 (+242) | **+0 B** | +0 | 0 | 0 |

⛔ **The first version of this control was VACUOUS and returned exactly the
prior (+0 B).** It had left the map at 8,386 lines from its own previous run,
then re-installed the same decoys and compared the decoy map against itself. It
was caught only by an anti-vacuity assertion, and the fix is that **each leg now
asserts, inside the measured path, that the rendered map grew by exactly 242
lines** — a leg that never reached the ruler REFUSES instead of reporting a clean
false zero. *A vacuity that agrees with your prior is the hardest kind to catch.*

**Guard discrimination** (W9-D's pattern, a gate that can fail):

```
PRE-change ledger    records=10485 (floor 5000)   of the 245: DENIED=245
POST-change ledger   records=10243 (floor 5000)   of the 245: DENIED=3
```

The 3 still denied are exactly the self-aliases deliberately left withdrawn.

## 6. Landed

242 memberships restored across 88 groups, GROUNDED-2 / W9-D convention: the record
**moves** out of `withdrawn[]` into `restored[]` with its original text verbatim
under `superseded_records`, the spelling joins `folded[]`. Nothing deleted, nothing
merely annotated — the guard reads `withdrawn`, so a record left in place keeps
denying. Deep-compare asserted **only the 88 intended groups changed**.

```
folded 5092 -> 5334 (+242)   withdrawn 10485 -> 10243 (-242)   restored 74 -> 316 (+242)
map 8141 -> 8383 lines
matched_code 4116348 -> 4118392 (+2044 B)   matched_functions 43924 -> 43930 (+6)
code% 40.170982 -> 40.190933                 fuzzy 49.987680 -> 49.987747
CROSSED IN 7 (2044 B)   FELL OUT 0           PREDICTION HELD EXACTLY
```

Why these are installed: **because they are true, not because they pay.** All 242
are flat `L1_T1` — retail-byte identity with relocation target names compared — on
two instruments with no shared adjudication code. Their withdrawal came from the
**ALIAS-REPAIR 2026-08-19** sweep, whose predicate resolves operands *"through the
ICF congruence over our own build"*; `/OPT:ICF` folded COMDATs in **retail's** link,
so that predicate is unsound in this direction. That is the identical defect W9-D
(`c6711597`) found in 65 sibling memberships and reversed. 236 of the 242 pay
nothing at all and are installed anyway.

## 7. What this lane did NOT do

1. **The 26 `CONTRADICTED`, 29 `NEEDS_SOURCE`, 8 `L2_RECURSIVE`, 7 `NEEDS_MAP_ID`
   and 1 `L5_INCONSISTENCY` rows are untouched.** `L2_RECURSIVE` is closed under the
   fold class under test and does not outrank a recorded withdrawal — W9-D, W16-CP
   and this lane all leave it withdrawn. CP's §7.4 warning that the `CONTRADICTED`
   set is *counted, not adjudicated* still stands.
2. **The 3 self-alias records were left in `withdrawn[]`.** They are inert (the name
   is already the survivor) and modifying them buys nothing measurable.
3. **The 9,393 `FABRICATED_CLOSURE_NOT_PARTITION` withdrawals — 20× this class —
   remain unexamined.** Whether the same precedence governs them is unknown and must
   not be assumed from this lane.
4. **No source, splits, map or objdiff change.** The alias ledger is a report-time
   input: `tree_sha256` is unchanged, `[patch-state] OK` before and after.
