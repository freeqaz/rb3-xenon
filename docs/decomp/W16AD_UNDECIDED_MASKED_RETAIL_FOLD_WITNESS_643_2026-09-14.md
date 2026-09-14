# W16-AD — the RETAIL-SIDE FOLD WITNESS, mechanised over all 643 `UNDECIDED_MASKED` alias memberships

**Lane:** W16-AD (opus) · **Branch:** `w16-ad` · **Date:** 2026-09-14
**Predecessor:** W16-AA, `docs/decomp/W16AA_UNDECIDED_MASKED_TOP10_LISTNODE_0x823c3960_2026-09-14.md`
**Artifacts:** `docs/decomp/W16AD_fold_witness_2026-09-14.json` (643-row table),
`docs/decomp/W16AD_withdrawal_prediction_2026-09-14.json`,
`docs/decomp/W16AD_withdrawn_20_2026-09-14.json`

---

## Headline

W16-AA adjudicated the **size-ranked top 10** of the `UNDECIDED_MASKED` class by
hand and found **zero wrong callees**, concluding the class is mostly an
instrument *reporting* gap rather than an accuracy exposure. This lane built the
instrument that settled those hand cases and ran it over **all 643 memberships /
41,360 B**.

**The bulk of W16-AA's reading survives at population scale, and its headline
does not.**

| | |
|---|---|
| rows where the class really is a reporting gap | **496 / 30,864 B** (`NO_WITNESS_FOLDED_SIDE` — gi=95's situation at scale) |
| rows CONFIRMED on retail bytes | **23 / 1,944 B** |
| ⛔ **rows REFUTED on retail bytes** | **63 / 3,828 B** — the alias forgives a **wrong callee** |

So the class is *predominantly* a reporting gap, exactly as W16-AA said — but
**63 memberships are refuted**, which a size-ranked head of 10 could not see.
20 were withdrawn (the batch cap, chosen by largest forgiven bytes); 43 are
listed for a follow-on lane.

**All 63 refutations carry the strongest available signature**: retail@X's own
call at the discriminating offset lands on `c_S` in **63/63**, so X really is
the survivor while `c_N` demonstrably lives at a different retail address.

---

## 1. The instrument

A membership asserts *"our spelling N folded into survivor S at retail address
X"*. For an `UNDECIDED_MASKED` row retail@X compares EQ to **both** our N and
our S once unnamed relocation destinations are masked, and the one word that
would discriminate is a `b`/`bl` to an unnamed destination: our N calls `c_N`,
our S calls `c_S`, and `c_N != c_S` in our build.

The membership is true **iff retail folded `c_N` and `c_S` too** — one level
down. So:

> Find a **map-named** retail function that, in our build, calls `c_N`. Decode
> retail's branch at the corresponding offset. That is where `c_N` **lives** in
> retail. Do the same for `c_S`.
> *Same address* ⇒ retail folded the callees ⇒ the parent fold is real.
> *Different addresses* ⇒ retail kept them apart ⇒ candidate refutation.

Tool: **`tools/w16ad_fold_witness.py`**. Channel classification is **reused**
from `tools/w16aa_masked_diff.py` rather than rewritten — that file's
census-representative selection is load-bearing, so it was lifted to module
level (`pick_census_rep`, `analyze`) in a refactor **verified byte-identical**
on `w16aa_masked_diff.py 12` before and after.

### 1.1 The validity condition, which is the whole instrument

"Our caller's offset *o*" equals "retail's offset *o*" **only if the two bodies
are the same body**. A witness is therefore admitted as **STRONG** only when the
caller itself compares **EQ** to retail at its mapped address. That single
requirement buys two things at once:

* the offsets correspond, so the decoded `bl` really is the call we mean; and
* the caller's **map row is corroborated by bytes** — so the instrument is not
  resting on a map whose clusters W16-AA measured as internally inconsistent
  (see §5).

A merely size-equal caller is recorded **WEAK** and never decides a row. MEDIUM
index-aligns equal-length `b`/`bl` sequences. **CONFIRMED may rest on MEDIUM;
REFUTED requires STRONG on both sides.** The asymmetry is deliberate: a false
refutation is the costly error.

### 1.2 Refutation guards

Two different unnamed destinations are not automatically two functions. Before
refuting:

* both destinations must be `.pdata` **BeginAddresses** (X360 `.pdata` is
  **BIG-ENDIAN**) — else one is a funclet, thunk tail or EH-prefix mis-carve; and
* the two retail bodies must **not** be identical relocation-normalised — if they
  are, retail simply kept two copies (CD-7's 51-surplus class) and the pair says
  nothing either way.

Failing either yields **INCONCLUSIVE**, never a withdrawal. §6 measures how
often that guard fired and what it cost.

### 1.3 Why this is not the STLPORT-1 hazard

STLPORT-1's defect was a **one-sided reader artifact** (a COMDAT span billing a
successor's EH funclet against a `.pdata` function extent), and CLAUDE.md records
that such an error is **invisible to a two-sided size control** because it
cancels. This lane's refutation contains **no size and no our-side extent**: the
decisive comparison is *two retail addresses decoded by one procedure*. There is
nothing for a one-sided reader error to be one-sided about.

---

## 2. Self-test — run BEFORE the sweep, and it can FAIL

Verbatim (`python3 tools/w16ad_fold_witness.py --selftest`):

```
== SELF-TEST A: reproduce W16-AA's hand verdicts ==
  gi=444   MISS   tool=WITNESS_PARTIAL                  W16-AA=FOLD CONFIRMED
           channels: a_closable=5 b=0 named=0 | pairs=['CONFIRMED', 'NO_WITNESS_N', 'CONFIRMED', 'CONFIRMED', 'CONFIRMED']
  gi=1302  OK     tool=WITNESS_CONFIRMED                W16-AA=FOLD CONFIRMED
           channels: a_closable=2 b=0 named=0 | pairs=['CONFIRMED', 'CONFIRMED']
  gi=1050  MISS   tool=CHANNEL_B                        W16-AA=FOLD CONFIRMED
           channels: a_closable=0 b=2 named=0 | pairs=[]
  gi=730   MISS   tool=NO_WITNESS_FOLDED_SIDE           W16-AA=retail supports cross-T folding
           channels: a_closable=1 b=0 named=0 | pairs=['NO_WITNESS_N']
  gi=730   MISS   tool=NO_WITNESS_FOLDED_SIDE           W16-AA=retail supports cross-T folding
           channels: a_closable=1 b=0 named=0 | pairs=['NO_WITNESS_N']
  gi=95    OK     tool=CHANNEL_B                        W16-AA=NOT a retail fold; sound identification; LEAVE
           channels: a_closable=0 b=7 named=0 | pairs=[]

== SELF-TEST B: POSITIVE control (same callee both sides) ==
  c_N=c_S=??0Symbol@@QAA@PBD@Z -> CONFIRMED

== SELF-TEST C: NEGATIVE control -- the refutation arm MUST fire ==
  c_N=??0Symbol@@QAA@PBD@Z
  c_S=?Release@DataArray@@QAAXXZ
  -> REFUTED  (N@['0x827c0728']  S@['0x82270510'])
  guards: {"a_is_pdata_begin": true, "b_is_pdata_begin": true, "a_size": 140, "b_size": 76, "bodies_relocnorm_identical": false, "verdict": "SEPARATE"}

SELFTEST hand_reproduced=2/6 positive_control=PASS negative_control=FIRED
```

**The refutation arm fires.** A sweep that only ever says CONFIRMED would be the
census's blind spot re-implemented; the constructed negative control
(`??0Symbol@@` @0x827c0728 vs `?Release@DataArray@@` @0x82270510) comes back
**REFUTED** with both guards passing.

### 2.1 ⛔ The 2/6 hand reproduction is an instrument-REACH fact, not a failure

**I did not tune the tool until that table went green.** Doing so would have
destroyed the only evidence that the instrument has a defined reach. Each miss
was investigated and each has a byte-level cause:

| row | tool | cause |
|---|---|---|
| gi=444 | `WITNESS_PARTIAL` | 4 of 5 pairs CONFIRMED; the single gap is W16-AA's **own acknowledged `_K` residual** |
| gi=1050 | `CHANNEL_B` | channel (b) only — **no branch witness is possible**, by construction |
| gi=730 ×2 | `NO_WITNESS_FOLDED_SIDE` | the folded spellings are Ham/DC3 types **retail does not contain** |
| gi=95 | `CHANNEL_B` | **agrees** with W16-AA ("not a retail fold, leave") |

So the instrument reproduces every hand verdict it is *structurally able* to
reach, and declines the rest for a stated reason. gi=95 and gi=1302 are the two
it can reach, and it gets both right.

---

## 3. The 643-row partition (self-validated)

| verdict | rows | bytes |
|---|---:|---:|
| `NO_WITNESS_FOLDED_SIDE` | 496 | 30,864 |
| **`WITNESS_REFUTED`** | **63** | **3,828** |
| `WITNESS_INCONCLUSIVE` | 52 | 3,512 |
| `WITNESS_CONFIRMED` | 23 | 1,944 |
| `WITNESS_PARTIAL` | 4 | 380 |
| `CHANNEL_B` | 4 | 696 |
| `ALREADY_NAMED` | 1 | 136 |
| **total** | **643** | **41,360** |

Rows sum to **643** and bytes to **41,360** exactly, as required.

### 3.1 Independent cross-validation against W16-AA's published population table

W16-AA published the class composition. My sweep computes that population
itself and was **not fitted** to those figures:

| | W16-AA published | W16-AD measured |
|---|---|---|
| channel (b) present | 6 / 892 B | **6 / 892 B** ✅ exact |
| channel (a) only | 636 / 40,332 B | **636 / 40,332 B** ✅ exact |
| already-named | 1 | **1** ✅ exact |

Two rows carry **both** channels, which is the whole of the 638-vs-636 arithmetic
difference between how the two lanes phrase the split. Nothing disagrees.

---

## 4. Item 2 — the withdrawals

**20 memberships withdrawn** (the brief's batch cap), chosen by largest forgiven
bytes, across alias groups **113 (6), 201 (6), 336 (5), 562 (2), 1255 (1)**.
Groups are **kept**; each membership carries a `withdrawn` record with
`spelling`, `lane`, `class = RETAIL_WITNESS_REFUTED`, `disposition`, both witness
addresses and the witnessing callers. **Nothing was pruned** — a membership
removed without a `withdrawn` record is a clobber, and a group emptied of a
membership believed true but not yet live would degrade later (a prior prune cost
+94,616 B to reverse).

**43 refuted rows / 2,580 B were NOT landed** and are listed in
`W16AD_withdrawn_20_2026-09-14.json` under `not_landed_refuted`.

### 4.1 Predicted vs measured — Δ0, pre-registered as Δ0

`tools/w16ad_predict_withdrawal.py` predicted the byte effect **before** the
build: **0 bytes**. Measured by set-diff of the `fuzzy==100` row set on a full
build: **0 bytes, 0 rows**. `matched_functions 43,302` and
`matched_code 3,988,880` unchanged.

⚠ **An all-zero prediction is the exact shape CLAUDE.md warns is a vacuity**, so
it was investigated rather than reported. The finding: a membership only forgives
a site where our relocation names N **and** retail's destination carries a **map
name that is another member of the same group**; an *unnamed* retail destination
is already forgiven as a placeholder by `is_placeholder_symbol_name`. Checked
directly: the 63 spellings have 1–3 call sites each, **all 74 call sites are in
functions that are not report rows at all**, and **none of the 63 spellings is
itself a report row**. The tool now emits `caller_report_status` and
`self_is_report_row` specifically so this zero is **distinguishable from a
vacuity**.

⇒ These 20 withdrawals buy **accuracy, not bytes**. They are landed on merit:
the forgiveness they removed was forgiving a demonstrably wrong callee.

`python3 tools/icf_alias_finder.py --validate` on the built tree: **PASS, 0
CONTRADICTED, 1,634 groups** (unchanged — nothing pruned).

### 4.2 ⛔⛔ A defect found in the census's own keying, which invalidated a first attempt

**The census's `gi` is NOT an index into `aliases['groups']`.** Caught because
the 20-row selection produced impossible arithmetic (`gi=203 folded list has 1,
withdrawing 6 -> -5 remain`) and two groups resolved with **empty** folded lists.

Measured skew: gi=114→113, gi=203→201, gi=1257→1255 — **non-constant**
(+2 on 564 `UNDECIDED_MASKED` rows, +1 on 68, 0 on 11). Over the whole census
only **503 of 5,315 rows have `gi` correct; 4,785 are WRONG** and 27 resolve to
no group at all.

Fixed by keying on **`(survivor, address)`**, which is unique over all 1,634
groups, and *validated* by requiring the folded spelling to be present in the
resolved group: **643/643**.

**`tools/w16s_ablate.py` had the same defect and is fixed the same way.** Its old
behaviour was a **silent no-op** — removal filters by name membership, so a
mis-keyed ablation deletes nothing — which means past mis-keyed ablations
**UNDER-report** what a class forgives rather than deleting the wrong thing. Any
inherited ablation figure keyed on `gi` should be re-measured.

---

## 5. Item 3 — the Accomplishment/Goal algorithm-instantiation cluster

### 5.1 The invariant, and why the masked-body test cannot settle this

An STL algorithm instantiated on comparator `XCmp` calls `??RXCmp@@`. **Our
build satisfies this 24/24 with no exceptions**, so it is a property of the
source, not of a name. Decoding retail's branch at the offset where *our* body
calls the comparator therefore names the comparator retail's function is
instantiated on — and so names the function.

⛔ **`tools/w16aa_adjudicate_dest.py` cannot do this.** Both candidates read
**MASKED-EQ 84 B / 3 relocs at BOTH addresses** — necessarily, because the
comparator `bl` is precisely the relocated word masking removes. The
discriminator has to be the decoded **destination**.

The comparator map rows were **re-verified independently** rather than inherited
from W16-AA. Their retail `.pdata` extents discriminate all three candidates by
size — a test that could have failed and did not:

| retail address | extent | only candidate that fits |
|---|---|---|
| `0x825f7000` | 104 B | `??RAccomplishmentCmp@@` |
| `0x825f6f20` | 112 B | `??RAccomplishmentCategoryCmp@@` |
| — | — | `??RGoalCmp@@` (144 B) fits **neither** |

### 5.2 Adjudication over 35 instantiations (`tools/w16ad_item3_adjudicate.py`)

| verdict | n |
|---|---:|
| **CONSISTENT** (the control) | **14** — GoalAlpaCmp 7/7, AccomplishmentGroupCmp 7/7, **all reading `fuzzy` 100** |
| **INCONSISTENT** | **11** — *all* Accomplishment Cmp/Category |
| `DEST_UNNAMED` | 4 |
| `NOT_IN_MAP` | 6 |

The 14 CONSISTENT rows are the control: **the invariant holds in retail wherever
the names are right**, and those rows are already at 100. The inconsistency is
confined to one family.

### 5.3 ⛔ Why "the map is wrong, therefore fixing it pays" is FALSE here

The cluster is mislabelled **consistently**: a caller row and its callee row are
wrong in the *same direction*, their relocation names agree, and `name_check`
charges nothing. **7 of 16 caller sites in this cluster are in exactly that
state.** Renaming only the callee breaks the cancellation and manufactures a
charge on a row that reads 100 today.

So the edit was **simulated, not argued** — `tools/w16ad_item3_simulate.py`
replays `name_check`'s own question (does our relocation's target *name* equal
the map name of the address retail branches to?) over every site, under the
current map and the proposed map.

**Two defects in the simulation were caught by measurements that could have come
out either way, and both are documented in the tool's source:**

1. **It held the caller's own address fixed.** objdiff pairs target↔base **by
   name**, so after the swap our `<AccomplishmentCmp>` body is compared against
   the *other* retail address. The first draft priced the swap as if only callees
   moved and reported a uniform regression that does not exist.
2. **It charged placeholder destinations.** `name_check` **forgives** them.
   Caught because a 408 B row measurably reading `fuzzy 100` was credited with 2
   charges — impossible.

### 5.4 Result: four proven swaps, predicted exactly

Four of the eleven INCONSISTENT rows form **mutual** swap pairs at addresses whose
names are **both defined by `band3/meta_band/AccomplishmentPanel.obj`**, so the
swap cannot un-pair a row — the hazard that is **80.5% of a map edit's delta**.

| algorithm | addresses swapped |
|---|---|
| `__merge_without_buffer` | `0x825fbd78` ↔ `0x825fb6c8` |
| `__unguarded_linear_insert` | `0x825f7250` ↔ `0x825f71a0` |
| `__upper_bound` | `0x825f76a8` ↔ `0x825f74e8` |
| `merge` | `0x825f82d8` ↔ `0x825f8148` |

**Pre-registered:** +1,088 B from 7 rows crossing, −408 B from one falling, net
**+680 B / +6 rows**.
**Measured** by set-diff of the `fuzzy==100` row set on a full build:
**identical — row for row and byte for byte.**

```
CROSSED IN : 7 rows, 1088 B
FELL OUT   : 1 rows,  408 B   ??$__merge_adaptive@...VAccomplishmentCmp@@...
NET bytes  : +680
  matched_functions  43302 -> 43308     delta +6
  matched_code       3988880 -> 3989560 delta +680
```

**The one regression is landed deliberately.**
`__merge_adaptive<AccomplishmentCmp>` read 100 only because two errors cancelled.
Making it visibly charged is a **correction**; a metric that hides a real bug is
worse than a lower metric.

### 5.5 Handed to a map lane — the remaining 3 INCONSISTENT rows

They are **not a 2-cycle but a 3-way rotation**. The band at `0x825f3xxx` named
`<AccomplishmentCmp>` (`__linear_insert` `0x825f3db8`, `__lower_bound`
`0x825f3480`, `__merge_backward` `0x825f3e38`) calls an **unnamed** comparator at
`0x825f32a8`, and is almost certainly
`CampaignGoalsLeaderboardChoicePanel`'s **`<GoalCmp>`** instantiations — whose map
rows are exactly the **6 `NOT_IN_MAP`** entries. Resolving it requires
`0x825f32a8` identified **first**, and re-homes rows **across units**, which is
**not reattribution and is not metric-neutral**. Out of scope here by the brief's
own rule.

---

## 6. Item 4 — gi=563, and an audit of my own guard

Item 4 asked for the witness on gi=563 (112 B, `__destroy_range_aux` at unnamed
`0x8246ebe0`): CONFIRMED ⇒ nothing to do, REFUTED ⇒ Item 2 rules. **It is
neither.** The witness is STRONG on both sides — `c_N` at `0x8246eb78`, `c_S` at
`0x8246ebe0`, each from a caller comparing EQ to retail — but the guard fires
`INCONCLUSIVE_TWIN`, so **no withdrawal**.

Hand-verified with an independent decoder, and worth recording: **my first
independent check disagreed with the tool**, which was **my own error** — I
compared `S_map_named` (`0x82295fb8`, 96 B) instead of the witness destination.
On the correct pair both are 100 B `.pdata` starts of 25 words differing in
**exactly one word**: a `bl` at `+0x2c` to `~vector<Color>` (`0x82774148`) vs
`~vector<Vector2>` (`0x827740E0`) — the `_List_base<T>::clear` shape CLAUDE.md
describes, per-`T` deallocators.

### 6.1 ⚠ That exposed a property of my own guard, now measured

`guard_pair` tests identity with `masked_body`, which masks branch displacements
**and imm16**. CD-7's 51-surplus class — what the brief's guard is aimed at — is
bodies identical **INCLUDING call targets**. Bodies identical only when call
targets are **ignored** are CD-7's *other* population (3,967 in 1,061 groups),
exactly what `/OPT:ICF` is **expected** to leave alone.

So the guard is conservative in the **safe** direction, but its reach is wider
than advertised. `tools/w16ad_strict_twin_audit.py` measures the cost rather than
hedging about it, over the whole 643-row sweep:

| | |
|---|---:|
| pair-instances landing `INCONCLUSIVE_TWIN` | 8 |
| of those, separating cleanly under the **strict** test | **8** |
| genuine CD-7 51-surplus twins found | **0** |
| distinct rows blocked | **7 / 492 B** — gi=338, 563, 564, 583, 589, 625, 942 |

**No row in this population is a genuine 51-surplus twin**: the guard never
protected against the hazard it was written for, it only ever blocked
shape-identical/reloc-different pairs.

⇒ **The lane's 63 refutations are a LOWER BOUND with a number attached: the
strict ceiling is 70 rows / 4,320 B.** Nothing was withdrawn on that basis —
INCONCLUSIVE is not REFUTED, and the Item 2 batch cap of 20 was already spent.

---

## 7. What I did NOT do, and why

* **Did not withdraw any `NO_WITNESS` row** (496 rows / 30,864 B — 75% of the
  class). Absence of a witness is **absence of evidence, not refutation**.
* **Did not withdraw the 43 remaining refuted rows** (2,580 B) — the brief caps a
  batch at 20. They are listed with their witness addresses.
* **Did not withdraw the 7 guard-blocked rows** (492 B) despite all 7 separating
  under the strict test. §6.
* **Did not prune any alias group.** Every withdrawal keeps its group and records
  a `withdrawn` entry.
* **Did not touch the 3 remaining Accomplishment INCONSISTENT rows** — a 3-way
  rotation needing `0x825f32a8` identified and cross-unit re-homing. §5.5.
* **Did not fix any source line.** No refuted row exposed a wrong callee in *our*
  source that retail names unambiguously — in every one of the 63, our source
  spells a callee that genuinely exists at a distinct retail address; the defect
  is the **alias**, not the source.
* **Did not re-measure inherited ablation figures** invalidated by the `gi`-keying
  defect (§4.2), beyond fixing the tool. Flagged for a follow-on lane.
* **Flagged, not fixed:** the map places `c_S` of gi=563 at `0x82295fb8` (96 B)
  while an EQ caller's decoded branch puts it at `0x8246ebe0` (100 B). Those
  disagree. Adjudicating it needs its own pre-registration.

---

## 8. Commits on `w16-ad`

| sha | contents |
|---|---|
| `74a9c739` | witness tool + behaviour-neutral `w16aa_masked_diff` refactor (verified byte-identical) |
| `4cf6a8a3` | 643-row sweep + withdrawal prediction |
| `0f059717` | 20 withdrawals (`gi`-keying defect fixed; `w16s_ablate.py` fixed with it) |
| `dcf8ab78` | Item 3 — four Accomplishment map-row swaps, +680 B measured exactly as predicted |
| `5983542c` | Item 4 + guard conservatism audit |
