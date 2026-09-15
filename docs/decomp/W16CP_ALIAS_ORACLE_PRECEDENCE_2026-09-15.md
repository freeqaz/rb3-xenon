# W16-CP — which oracle wins when map-resolved retail bytes and our-build COMDAT
# congruence disagree about an ICF fold

**Date:** 2026-09-15 · **Lane:** W16-CP · **Branch:** `w16-cp` · **Base:** `7ec1e249`
**Tree:** built and patched before any name lookup —
`./tools/ninja-locked` EXIT=0, `verify_objs_patched.py --verify-manifest`
`[patch-state] OK: 1219 decomp, 3105 target objects match`,
`tree_sha256=9950e900ef7658de`. Ruler: `functionRelocDiffs=name_check`
(objdiff 4.2.9), read from `report.json`'s own `provenance`, not assumed.

**Nothing was installed, withdrawn, or otherwise changed in
`scripts/symbol_aliases.json`.** This lane is an adjudication-rule lane; its
deliverable is the rule and the per-membership verdicts. The ledger edit is
specified here for whoever takes it, with the sanctioned override schema.

---

## 0. Verdict in one line

**Retail bytes are authoritative, and the mechanism is that ICF happened in
RETAIL's link — but the precedence holds only at the FLAT layer.** A retail-byte
proof that is itself closed under the fold class being tested (`L2_RECURSIVE`)
does **not** outrank a recorded withdrawal. The disputed membership is **flat
`L1_T1`**, so its withdrawal is **refuted**. The other four charged pairs are
`L2_RECURSIVE` and are **not** admitted on this lane's evidence.

⚠ **And the rule was already in the tree.** Lane **W9-D** (`c6711597`,
2026-09-13, `docs/decomp/ALIAS_HYGIENE_2026-09-13.md`) adjudicated the *same*
predicate, from the *same* lane, in the *same* withdrawal class, and reversed
**65 of 74** withdrawals on exactly this reasoning. W16-CN escalated a question
whose governing precedent was three commits of `git log --grep` away. That is
the standing directive *"⛔ READ THE IN-TREE RECORD FIRST"* firing again, and it
is the cheapest lesson in this document.

---

## 1. The two oracles, stated precisely

| | Oracle A | Oracle B |
|---|---|---|
| name | map-resolved destinations | our-side COMDAT congruence |
| instrument | `tools/comdat_fold_gate.py` Gate 1 `compare()` | ALIAS-REPAIR 2026-08-19 partition sweep (`decomp-synth tools/il_witness/resolved_operands.py`) |
| asks | are RETAIL's bytes at the survivor address byte-identical to OUR compiled body, modulo relocated fields, with branch destinations resolved and **target names compared**? | do the group's members agree on their **resolved operands**, *"resolved through the ICF congruence over our own build"* (quoted from the group's own `repair.why`)? |
| on this row | **PASS** | **WITHDRAWN**, class `UNDER_PARTITIONED_ICF_CLOSURE`, disposition `dropped_singleton` |

**Oracle B is measuring the wrong binary.** `/OPT:ICF` folded COMDATs in
**retail's** link. Whether *our* build's ICF congruence puts two instantiations
in one class is a fact about our linker's input, and it is neither necessary nor
sufficient for retail having folded them. Oracle A reads the artifact the
question is actually about. This is not a new finding — W9-D wrote it down —
but it is restated here because W16-CN's escalation shows it had not reached
the lanes that needed it.

★ The asymmetry is **not** "newer instrument wins", and it is not "the gate is
better written". It is that one oracle's evidence is *about retail* and the
other's is *about us*.

---

## 2. Why this row was not already covered by W9-D — a structural point worth keeping

W9-D's population was the **74 live-and-withdrawn** memberships: spellings
appearing in a group's `folded[]` **and** carrying a `withdrawn` record, an
internally contradictory state that the regeneration guard would have resolved
destructively. The disputed spelling here is **withdrawn and live nowhere**
(measured: it is not a `survivor`, not in any `folded[]`, has no `restored`
record). It was **outside W9-D's population by construction.**

⇒ **W9-D's rule applies to this row; W9-D's adjudication does not.** A lane that
reads "W9-D fixed the ALIAS-REPAIR withdrawals" and stops will conclude the
opposite of the truth. The 74 were the *self-contradictory* subset, not the
*wrong* subset.

Ledger shape today, measured:

```
groups                                          1,657
withdrawal records                             10,486   (10,484 dict, 2 bare strings)
   ALIAS-CONSOLIDATION 2026-08-19                9,393
   ALIAS-REPAIR        2026-08-19                  456
UNDER_PARTITIONED_ICF_CLOSURE records             456   over 138 groups
   spelling LIVE somewhere                        152
   spelling live NOWHERE (withdrawn-only)         304
   dispositions: repartitioned 193 / dropped_singleton 263
restored records                                   73   (W9-D 65, GROUNDED-2 6, W16-Y 2)
live-AND-withdrawn at one group, today                3   (W9-D drove this 74 -> 0)
```

---

## 3. The decisive experiment — and the prediction it broke

Re-running Oracle A and reporting that it still says PASS would be worthless:
*a fresh instrument agreeing with the hypothesis that motivated running it is
the weakest possible evidence.* The question that discriminates is **which
evidence layer** each pass rests on, which is W9-D's own split:

* **`L1_T1`** — flat retail-byte identity, relocation **target names** compared,
  non-circular. W9-D reversed 65 withdrawals on this layer.
* **`L2_RECURSIVE`** — *"ICF fixpoint via chase"*: name equality closed under the
  very fold class under test. W9-D **enacted** 4 withdrawals despite a `PROVEN`
  label, because a fixpoint cannot outrank a recorded refutation.

**The test:** re-run `compare()` with the installed-alias map **emptied**. If the
pass survives, it is flat. `/home/free/tmp/w16cp_layer.py`.

> **I predicted the disputed membership would be the circular one.** It is the
> exact opposite, and that failed prediction is the most useful line in this
> lane: W16-CN's three "all PASS" dtor pairs are **not** of equal strength, and
> the one under dispute is the **only** flat one.

| charges | retail calls (S) | we call (F) | with aliases | FLAT (alias map empty) | layer |
|---:|---|---|---|---|---|
| dtor ×2 | `~vector<MoveReplacer>` @`0x822ec870` | `~vector<pair<ObjPtr<ET>,ObjPtr<ET>>>` | PASS | **PASS** | **`L1_T1`** ← disputed |
| dtor ×1 | `~vector<String>` @`0x822d8cc0` | `~vector<ObjPtr<EventTrigger>>` | PASS | **FAIL** | `L2`-like |
| dtor ×4 | `~vector<String>` @`0x822d8cc0` | `~vector<ObjPtr<RndPropAnim>>` | PASS | **FAIL** | `L2`-like |
| ctor ×2 | `make_pair<String,String>` @`0x822e7538` | `make_pair<ObjPtr<ET>,ObjPtr<ET>>` | FAIL | FAIL | unresolvable dest |
| ctor ×2 | `push_back<vector<SongPattern>>` @`0x822ecbc8` | `push_back<vector<pair<…>>>` | FAIL | FAIL | different callee |

The two `L2`-like passes are mediated by group[670] @`0x82710808`
(`__destroy_range_aux<reverse_iterator<ObjDirPtr<ObjectDir>>>`, 19 folded, T1,
**0 withdrawn, no repair record**) — i.e. the mediator is itself clean and is a
*different* template family. So the mediation is arguably compositional rather
than self-referential. **That nuance does not rescue them here**, and the
distinction is recorded in §7 as unsettled.

### 3.1 Independent cross-check — a different code path, same partition

`tools/alias_forgiveness_audit.Sides.verdict()`, the layered L1..L5 retail-byte
adjudicator **W9-D itself used**, run on the same five pairs. Different chase
logic, different reader, same tree:

```
ANTI-VACUITY: target bodies 69,432 (mangled 27,603) | our bodies 95,487
control: a known-live membership -> L1_T1          (expect a PROVEN layer)  OK
control: a nonsense symbol pair  -> NEEDS_SOURCE   (expect NEEDS_SOURCE)    OK

  dtor x2   L1_T1         flat T1                  <== DISPUTED
  dtor x1   L2_RECURSIVE  ICF fixpoint via chase
  dtor x4   L2_RECURSIVE  ICF fixpoint via chase
  ctor x2   L2_RECURSIVE  ICF fixpoint via chase
  ctor x2   L2_RECURSIVE  ICF fixpoint via chase
```

**Two instruments with no shared adjudication code partition the same five pairs
identically**, and both controls discriminate. That is the evidence that carries
this lane, not the PASS itself.

⚠ Note a correction to W16-CN in passing: it called the two ctor pairs
"unresolvable" and "chained fold". `Sides` labels both `L2_RECURSIVE`, i.e.
inside its `PROVEN` set. **The label `PROVEN` is not the adjudication** — the
layer is. This is the same trap W9-D named when it split 69 `PROVEN` into 65
reversible and 4 not.

### 3.2 Retail-byte anchor — the survivor address is not merely a map name

Both oracles anchor on `addr(S)` resolved through `target_symbol_map.json`, so a
wrong map row would move the comparison to the wrong bytes. Checked directly
against retail: `??1GemTrackDir@@UAA@XZ` @ `0x822ee768`, 182 words, branch
destinations enumerated from the bytes:

```
0x822ec870  offsets [508, 516]   <== the disputed survivor, EXACTLY the x2 charges
0x822d8cc0  offsets [500, 524, 532, 540, 548]   (5 sites = the x1 + x4 charges)
```

Retail's own instruction stream branches to `0x822ec870` twice. The address is
established by retail bytes; the map supplies only the *name*, and the T1 test
compares **bodies**, not names. The anchor is sound.

---

## 4. Per-membership verdicts

| membership | verdict |
|---|---|
| `~vector<pair<ObjPtr<EventTrigger>,ObjPtr<EventTrigger>>>` @ `0x822ec870` | **WITHDRAWAL REFUTED — admit.** Flat `L1_T1` on two independent instruments; retail-byte anchored; withdrawn by a predicate that reasons about our build. |
| `~vector<ObjPtr<EventTrigger>>` @ `0x822d8cc0` | **NOT ADMITTED on this lane's evidence.** `L2_RECURSIVE` only. Not under any withdrawal — simply never proposed. |
| `~vector<ObjPtr<RndPropAnim>>` @ `0x822d8cc0` | **NOT ADMITTED**, same. |
| `make_pair<ObjPtr<ET>,ObjPtr<ET>>` @ `0x822e7538` | **IRREDUCIBLE today.** Retail's branch at `+0x4` targets `0x82829258`, which the map does not name. Needs a map identification (MAPID-1 shape), which pays in bug exposure, not bytes. |
| `push_back<vector<pair<…>>>` @ `0x822ecbc8` | **NOT ADMITTED.** Retail calls `_Copy_Construct<SongPattern>`, we call `_Copy_Construct<pair<…>>`; a chained fold that needs that pair established first. |

## 4.1 ⛔ The 728 B is NOT collectable, and W16-CN's corrected reason is also wrong

W16-CN first claimed 728 B collectable, then retracted it as *"blocked by a
policy conflict, 2 of 3 pairs cleanly T1, the third contested."* **Both halves of
that are wrong.** Measured on the rendered map objdiff actually consumes
(`build/45410914/icf_aliases.map`, uppercase-hex keyed):

```
bucket 822EC870 : 1 symbol   -- the survivor ~vector<MoveReplacer> ALONE
                               (a one-symbol bucket is not an equivalence)
bucket 822D8CC0 : 2 symbols  -- ~vector<String> + ~vector<ObjDirPtr<ObjectDir>>
```

**None of the dtor's three pairs is an installed equivalence.** `matched_code` is
all-or-nothing per row, and the row carries 7 charges over those 3 pairs:

```
default/GemTrackDir  ??1GemTrackDir@@UAA@XZ   size=728   mpn=fuzzy=99.80769
default/GemTrackDir  ??0GemTrackDir@@QAA@XZ   size=2548  mpn=fuzzy=99.968605
```

⇒ Admitting the disputed membership **alone** leaves 5 charges over 2 pairs and
the row still does not cross: **+0 B on this row.** Collecting the 728 B
additionally requires both `0x822d8cc0` pairs, which rest on `L2_RECURSIVE`.
**0 of 3,276 B remains collectable** — the same bottom line W16-CN reached, for a
materially different and better-characterised reason. Admit the membership
because it is **true**, not because it pays.

★ This is the standing rule demonstrating itself: *a row's headline prize
computed from a charge count can be uncollectable in principle.* Price from the
installed-equivalence state, not from the number of passing pairs.

---

## 5. THE RULE (for the next lane facing this conflict)

1. **Retail bytes outrank an our-build predicate.** An ICF claim is a claim about
   retail's link. Any oracle whose evidence is our compiler's or our linker's
   congruence is not evidence about retail and loses on contact with a
   retail-byte proof.
2. **But precedence is layered, and the layer is the adjudication — never the
   label.** Only **flat `L1_T1`** (byte identity, relocation *target names*
   compared, alias map not consulted) outranks a recorded withdrawal.
   `L2_RECURSIVE` / "ICF fixpoint via chase" does **not**, because it is closed
   under the class being tested. `PROVEN` is a set that contains both; do not
   adjudicate on it.
3. **The test that separates them is cheap and mandatory: re-run the gate with
   the installed-alias map emptied.** Survives ⇒ flat. Collapses ⇒ circular.
   Report both legs.
4. **Cross-check on a second instrument with no shared adjudication code**
   (`comdat_fold_gate.compare()` vs `alias_forgiveness_audit.Sides.verdict()`),
   and **assert both a positive and a negative control** before believing either.
5. **Anchor the survivor address on retail bytes** — enumerate the caller's
   branch destinations — so the verdict does not silently rest on one map row.
6. **A `withdrawn` record is evidence, not authority.** Read its `lane`,
   `class` and `repair.why`, and check whether that *mechanism* has since been
   refuted (STLPORT-1 `ff832b50` / GROUNDED-2 `6e13ee3f` for the size class;
   **W9-D `c6711597` for the our-build-congruence class**). Equally: a withdrawal
   whose mechanism survives scrutiny **stands**, and 26 of them here do.
7. **Never install by preferring the newer run.** Re-admission goes through
   `tools/alias_withdrawals.py:load_overrides`, which requires `spelling`, one of
   `survivor`/`address`, a non-empty `reason`, and `overrides_class` **equal to
   the ledger record's class** — a blind override is impossible by construction,
   and there is deliberately no allow-all.

### 5.1 The vein this rule opens — sized, not extrapolated

All 456 `UNDER_PARTITIONED_ICF_CLOSURE` withdrawals adjudicated on retail bytes
(`Sides.verdict`, anti-vacuity gate passed, full run,
`~/tmp/w16cp_underpart_adjudicated.json`):

| layer | count | of which live nowhere |
|---|---:|---:|
| **`L1_T1`** (flat) | **385** | **246** |
| `CONTRADICTED` | 26 | 26 |
| `NEEDS_SOURCE` | 29 | 25 |
| `L2_RECURSIVE` | 8 | 1 |
| `NEEDS_MAP_ID` | 7 | 5 |
| `L5_INCONSISTENCY` | 1 | 1 |

**84.4% of this sweep's withdrawals adjudicate flat `L1_T1`** — a near-replay of
W9-D's 65-of-74 (87.8%) on a **disjoint** population, which is corroboration
rather than repetition. **246 memberships are flat `L1_T1` and live nowhere**:
the reversible vein, of which this lane's disputed row is one.

⛔ **This lane does NOT price that vein and nobody should multiply it out.** W9-D
measured its 65 reversals as protecting 45,712 B, but those were *already live*
— reversing them was Δ0 and protected existing forgiveness. These 246 are **not
live**, so admitting them **adds** to `folded[]`, changes the rendered map, and
has a **real, unmeasured delta in both directions** (it can also expose
wrong-callee bugs the way MAPID-1 did). W9-D's own §9.3 correction applies:
*a figure obtained by arithmetic on two other figures is not a measurement.*
Price with `tools/alias_group_ablate.py`, per membership, before installing.

---

## 6. Ledger edit specified but NOT made

For whoever installs it — the disputed membership only:

```json
[{"address": "0x822ec870",
  "spelling": "??1?$vector@U?$pair@V?$ObjPtr@VEventTrigger@@@@V1@@stlpmtx_std@@V?$StlNodeAlloc@U?$pair@V?$ObjPtr@VEventTrigger@@@@V1@@stlpmtx_std@@@2@@stlpmtx_std@@QAA@XZ",
  "overrides_class": "UNDER_PARTITIONED_ICF_CLOSURE",
  "reason": "W16-CP: flat L1_T1 on two independent instruments with the alias map emptied; retail's ??1GemTrackDir branches to 0x822ec870 at +508/+516 in its own bytes. Withdrawn by the ALIAS-REPAIR 2026-08-19 predicate, which resolves operands through the ICF congruence over OUR build -- the wrong binary, refuted for this class by lane W9-D (c6711597)."}]
```

Follow the **GROUNDED-2 / W9-D convention**, verified against the file by both
lanes: restoring means **moving** the record from `withdrawn[]` into `restored[]`
with its original text under `superseded_records`, **never deleting it** and
never merely annotating beside it — the guard reads `withdrawn`, so a record left
in place keeps denying (`0x824afa78` is the standing proof).

---

## 7. What this lane did NOT settle

1. **The two `0x822d8cc0` pairs are unresolved, not refused.** Their mediator
   (group[670]) is a clean, independent T1 group in a *different* template
   family, so "alias-mediated" may be **compositional** rather than circular —
   a distinction `Sides` does not draw, since it labels any chase-resolved pass
   `L2_RECURSIVE`. **Whether a chase through an independently-proven,
   non-overlapping fold class should count as flat is an open question of
   general importance** (it governs the 8 `L2_RECURSIVE` rows above and every
   future chase). This lane deliberately did not widen the gate to find out.
2. **The 246-membership vein is unpriced.** Sized only. No A/B, no ablation.
3. **`0x82829258` is still unnamed**, so the ctor's `make_pair` pair is
   irreducible today. Unchanged from W16-CN.
4. **The 26 `CONTRADICTED` and 29 `NEEDS_SOURCE` withdrawals were not
   individually inspected.** They are counted, not adjudicated; W9-D's warning
   that *a tool's confident "contradicted" is the claim most worth auditing*
   applies to them as much as to anything here.
5. **No ledger, map, source or metric change was made**, so no A/B was owed and
   none was run. `report.json` is unmoved: `matched_code 4,109,688 /
   40.10599% / matched_functions 43,915`.
6. **The 9,393 `FABRICATED_CLOSURE_NOT_PARTITION` withdrawals** (from
   ALIAS-CONSOLIDATION 2026-08-19, 20× this lane's class) were not examined at
   all. Whether the same precedence rule governs them is **unknown** and should
   not be assumed from this lane.

---

## 8. Instrument hazards met in this lane, recorded because they are cheap to repeat

* ⛔ **A lowercase grep of `icf_aliases.map` for `822ec870` returns nothing** —
  the map writes uppercase hex (`822EC870`). The first pass produced a clean,
  decisive, **false** negative on the exact question the lane turned on. Same
  family as the binary-blind `grep` shim: *emptiness shaped like a result.*
* ⛔ **`symbol_aliases.json`'s `withdrawn[]` is not homogeneous** — 10,484 dicts
  and **2 bare strings**. A naive `w.get('spelling')` raises `AttributeError`
  mid-census; a naive `w['class']` would have silently skewed one.
* ⛔ **`report.json`'s `provenance` is a dict but `diff_config` is a LIST of
  `"key=value"` strings**, and `fuzzy_match_percent` is **not** a function-level
  key in this schema — function rows carry `match_percent_normalized`, with
  metadata holding only `demangled_name`. Protobuf defaults are omitted; coerce
  and check the shape before filtering.
* ★ **The worktree's first `verify-manifest` said a build was owed** and the
  build was run before any mangled-name lookup. A reflinked tree's target objs
  are pre-renamer, and every verdict here would otherwise have read `absent` —
  the vacuity that *agrees with whatever you expected*.
