# W16-GG — `chase()`'s vacuous branch now accepts a PROVEN destination, not only an identical one

Date: 2026-09-16 · branch `w16-gg` · worktree off main `6159dfd5`
Patch class: **tool-only** (`tools/icf_pair_adjudicate.py`). **No alias installed. Zero bytes banked, by construction.**

This lane exists because lane **W16-GD** found the blocker and refused to fix it:
GD would have collected the payout, and the house rule is *an instrument must not
be relaxed by the lane that collects the payout*. GD's record is
`docs/decomp/W16GD_PUSHBACK_FOLD_AND_SSORT_NEGATIVE_2026-09-16.md` §2.

---

## 1. The change, and why it is the minimal one

`chase()` is recursive T1: it verifies a fold by walking *retail's own* relocation
slots and recursing on differing callee names. Its **vacuous branch** guards bodies
that are under 4 words or over half masked, where a masked-byte comparison is
non-discriminating. That branch admitted a pair only when the masked bodies **and
the full relocation list including target names** were literally equal
(`VACUOUS-BUT-IDENTICAL`), and it never recursed.

**The gap.** A 4-byte tail-call thunk (`b <target>`) has masked body `0x00000000`
because the whole instruction *is* the relocated field. Its destination is therefore
the entire information content. Literal name equality pins that destination — but so
does a **recursively proven fold of the two destinations**, because `/OPT:ICF` is a
**fixed point**: if retail folded X and Y, then `b X` and `b Y` resolve to the *same*
address and the two thunks satisfy the folding condition themselves. Refusing that is
not strictness; it refuses a fold **because the linker folded iteratively**.

**What changed, exactly.** The relocation-slot loop is factored into one comparator,
`_slots_agree(...)`, with **two callers that differ by a single named boolean**:

| caller | `tolerate_placeholders` | why |
|---|---|---|
| general (non-vacuous) path | **True** | unchanged; removing it regressed a landed positive control (the CD-9 note) |
| vacuous path | **False** | in a vacuous body a tolerated slot means **nothing was compared** |

The vacuous branch then reads: masked bodies equal **and** (relocation list literally
equal — the old fast path, short-circuited first, so no previously admitted pair
changes — **or** every differing slot's destinations recursively proven, with
placeholders **refused**).

**The resulting branch is strictly WIDER than its old self and strictly NARROWER than
the general path.** The invariant it maintains is unchanged: *every masked field is
accounted for, and every unmasked byte is compared.* The relaxation adds one way to
account for a destination (a proof) alongside the existing one (equality).

⚠ **Strictness was RAISED on the placeholder axis, deliberately.** Sizing that hazard
on this tree: retail vacuous single-relocation thunks against ours of the same masked
body and relocation shape form **2,289,106 shape-compatible cross-pairs**. For this
class the destination does **100%** of the discriminating — tolerate it and the
comparator admits two million pairs.

---

## 2. Controls

### 2.1 The four pre-existing `--chasetest` controls still discriminate

| control | expected | measured |
|---|---|---|
| IN-FAMILY DECOY `fn_827B0E78` | REFUTED | **REFUTED** |
| FLAT-T1 GROUP (`SetObjConcrete<BandCharacter>` ↔ `<BandCamShot>`) | PROVEN | **PROVEN** |
| SELF-PAIR NEGATIVE (`CheckAwesomesCondition`, our COMDAT contradicts retail) | REFUTED | **REFUTED** |
| SELF-PAIR POSITIVE (`ADSRImpl::Save`, byte+reloc identical) | PROVEN | **PROVEN** |

Verdicts are **identical to the pre-change baseline** captured in this same worktree.
`--selftest` (the older flat-T1 mode) also still passes both directions.

### 2.2 The NEW control, and the reason it was mandatory

Two controls were added, both selected **from the live tree in sorted order** (not
hardcoded, so they survive map repairs — the W16-AE self-pair pattern). Both have the
*same shape*: masked bodies equal, one relocation each at the same offset and type,
both destination names real and **different**. They differ only in whether the
destinations are a fold:

| control | construction | expected | measured |
|---|---|---|---|
| **VACUOUS DECOY** | destination bodies **DIFFER** while being the **same size** (so a size test cannot stand in for the byte test) | REFUTED | **REFUTED** |
| VACUOUS FOLD | destination bodies byte-identical **and** their own relocations literally agree (flat T1) | PROVEN | **PROVEN** |

Both fire through the intended mechanism, verified in the trace rather than assumed:
the decoy refuses via `BYTES-DIFFER` → `SLOT-REFUTED` → `VACUOUS` (i.e. *because of*
the destination check), and the fold passes via the new `VACUOUS-DESTINATION-FOLD-PROVEN`
label, which exists only in the new branch.

### 2.3 ★ The decoy was WATCHED FAILING — `--self-break`

A control nobody has watched fail is an assumption. `--self-break` runs `--chasetest`
with the vacuous branch's **destination proof removed** (relocation *shape* still
checked) — i.e. "the relaxation with its recursion deleted" — and **exits 0 only if the
decoy goes red**:

```
=== VACUOUS DECOY, destinations are NOT a fold (expect REFUTED)
  CHASED T1: PROVEN
  ** CONTROL FAILED: wanted REFUTED **
self-break OK -- the VACUOUS DECOY went RED with the destination proof removed,
so the control discriminates.
```

★★★ **Only that ONE control flipped; the other five were unaffected.** That is the
finding, not a footnote: **the four pre-existing controls are structurally blind to the
vacuous branch's destination handling**, so a permissive relaxation would have shipped
green under them. GD predicted exactly this (*"without that, the relaxation is untested
in exactly the direction it widens"*) and was right.

### 2.4 Selectivity — the decoy as a number, against a null that can fail

4,000 **random** shape-compatible vacuous cross-pairs with differing destinations
(seeded, reproducible):

| branch | admitted |
|---|---:|
| relaxed branch (shipped) | **22 / 4,000 = 0.550%** |
| `--self-break` (destination proof removed) | **3,489 / 4,000 = 87.225%** |

A **158× separation**: the destination proof does essentially all the work, and the
relaxation did not dissolve the discriminator. **Integrity check on my own
implementation: all 22 admitted pairs have byte-equal destinations, 0 violations** —
e.g. `ObjOwnerPtr<Object>::SetOwnerObj` ↔ `ObjOwnerPtr<RndTex>::SetOwnerObj`, 104 B
both sides, a textbook template fold.

⚠ **Scope of that null, stated honestly:** it is a *random* population, so it bounds
permissiveness against unrelated code. The *in-family* near-miss case is what the
DECOY control covers specifically, and that is why both exist.

### 2.5 Regression control over every installed membership

Old `chase` (from `HEAD`) and new `chase` run over **all 4,838 installed
(survivor, folded) pairs** that resolve on both sides:

| | count |
|---|---:|
| unchanged verdict | 4,779 |
| REFUTED → PROVEN (the widening) | **59** |
| **PROVEN → REFUTED** | **0** ✅ |

**0 narrowings confirms the strict-widening claim empirically rather than by
assertion** — nothing already landed is re-litigated. The 59 are already-installed
memberships whose evidential standing moves *unproven → proven* at **zero metric
cost** (they were already forgiven).

### 2.6 Alias integrity

`python3 tools/icf_alias_finder.py --validate` → **`VALIDATE: PASS — 1408
map-consistent, 250 tolerated, 0 contradicted, 1659 total`**, rc=0. Identical to
W16-GD's reading (1,408 / 250 / 1,659). **Nothing pruned**; `STALE_SPELLING` (88) and
`UNWITNESSED` (101) untouched — a prior prune (`a745039e`) cost **+94,616 B** to reverse.

---

## 3. Whole-binary measurement — Δ0, and it is STRUCTURAL

**Pre-registered as zero, and zero is what the change is *for*.** Not merely expected:
`chase()` is **unreachable from the build**. Every importer
(`alias_forgiveness_audit`, `icf_relocname_census`, `incomplete_group_adjudicate`,
`incomplete_group_control`) is an offline analysis tool, and
`command grep -c icf_pair_adjudicate build.ninja` = **0**. The ninja alias chain is
`gen_symbol_alias_map.py` ← `scripts/symbol_aliases.json`, which never imports it.
`git status` shows exactly one modified file, the `.py`.

Ruler from `report.json` `provenance`: **`functionRelocDiffs=name_check`**,
`ppc.calculatePoolRelocations=false`; objdiff `tool_commit a5f0ea903ec1`.

⚠ **The first comparison I ran was VACUOUS and I discarded it.** The post-edit build
did 0 compiles *and did not re-run the REPORT edge*, so `report.json` was the same
file — comparing it to itself proves nothing. Leg B was re-taken after
`rm -f build/45410914/report.json build/45410914/report.cache`, forcing a genuine
recomputation (`[5/6] REPORT`, `Loaded 6106 ICF equivalence entries`).

| key | leg A (pre-edit) | leg B (recomputed, post-edit) | Δ |
|---|---:|---:|---:|
| `matched_functions` | 44,138 | 44,138 | **0** |
| `matched_code` | 4,168,232 | 4,168,232 | **0** |
| `masked_equal_functions` | 23,323 | 23,323 | **0** |
| `matched_code_percent` | 40.677315 | 40.677315 | **0** |
| `fuzzy_match_percent` | 50.4972 | 50.4972 | **0** |
| `total_code` / `total_functions` | 10,247,068 / 69,240 | 10,247,068 / 69,240 | **0** |

**All 13 measure keys compared key-by-key; keys differing: NONE.**

---

## 4. FILED — the `CopyTypeProperties` verdict under the repaired instrument

**`?CopyTypeProperties@@YAXPAVObject@Hmx@@0@Z`, 1,472 B, unit `default/system/obj/Utl`,
4 `diff_arg` charges.**

Pair: survivor `??$_S_sort@IV?$StlNodeAlloc@I@…` (retail, "unsigned int") ↔ ours
`??$_S_sort@VSymbol@@V?$StlNodeAlloc@VSymbol@@…`.

| tier | verdict |
|---|---|
| FLAT T1 | **REFUTED** — *"masked bodies match but relocation TARGETS disagree"* (the expected reading; `/OPT:ICF` is iterative) |
| CHASED T1, **before** this lane | **REFUTED** — died at `VACUOUS` on the thunk pair |
| CHASED T1, **after** this lane | ✅ **PROVEN** |

Evidence, read off retail's own relocations (`chase` never searches):

* Sizes equal **424 / 424**; masked bodies identical; 22 relocation slots, **12 read SAME**.
* Slots **11, 12, 14** → `list<BSPFace>::swap` ↔ `list<Symbol>::swap` — `SLOT-FOLD-OK`.
* Slot **20** → `_List_base<SynthPollable*>::clear` ↔ `_List_base<Symbol>::clear` —
  **FLAT-T1 PROVEN standalone**, 88 B both sides, `retail_bodytwins: 1` (no coin flip).
* Slots **1, 5, 15, 17** — the former blocker — retail
  `??$__destroy_aux@UEntry@LocalePanel@@` ↔ our `??1?$list@VSymbol@@`, each **4 B**,
  masked body `0x00000000`, **one type-6 relocation at offset 0**, destinations
  `_List_base<SynthPollable*>::clear` and `_List_base<Symbol>::clear` — i.e. **the pair
  already proven at slot 20**. All four now resolve `VACUOUS-DESTINATION-FOLD-PROVEN`.
* Slots **3, 7** — retail `lbl_8243F3C0` — placeholder, tolerated by the general path
  (and `name_check` forgives placeholder targets anyway).
* ★ **0 `CYCLE-ASSUMED` frames in the entire trace** — the proof descends by direct
  byte/relocation comparison, with no coinductive assumption anywhere.

**The source-defect hypothesis stays REFUTED** (GD's finding, re-confirmed): retail's
body is *named* `_S_sort<unsigned int>` but calls callees named for **BSPFace**,
**SynthPollable\*** and **LocalePanel::Entry** — three different `T` in one function,
which is the fold-arbitrary-survivor signature, not a container mismatch.

### ⛔ NOT INSTALLED — deliberately

The alias is **not** installed and the **1,472 B is not collected**. That is the point
of splitting the work: the relaxation decision stays uncontaminated by the scoreboard
of the lane that made it. `scripts/symbol_aliases.json` is **untouched**.

**For the lane that installs it** — prize **+1 function / +1,472 B**, `masked_equal`
**False** ⇒ Δhonest **+1**. Price the **block, not the row**: census *our* objs for
callers of the folded spelling before predicting the delta. And note the standing
integrity position — `ab_measure`'s `none` control **cannot** clear an alias (a
fabricated one lifts `name_check` by construction); the clearance is the retail-byte
adjudication above, and `VALIDATE: PASS` / `OK (MAP-CONSISTENT)` is **not** a fold proof.

---

## 5. Contradicting the brief / corrections

* **The brief said "Expected whole-binary measurement: Δ0 … If you measure a NON-zero
  delta, STOP — it means the relaxation reached installed groups."** The premise is
  sound but the mechanism is stronger than stated: **the relaxation cannot reach the
  ruler at all**, because `chase()` is not invoked by any ninja edge. A non-zero delta
  here would have indicated a *build* problem, not a relaxation that reached installed
  groups. Separately, the relaxation **does** reach installed groups — 59 of them — and
  that is still Δ0, because those memberships were already installed and already
  forgiven. **"Reached installed groups" and "moved the metric" are independent.**
* **GD's step 2 said the in-family decoy "should be unaffected — *verify, do not
  assume*".** Verified: unaffected, and `--self-break` shows *why* — it fails at
  `BYTES-DIFFER` in the general path, nowhere near the vacuous branch.
* **The first whole-binary comparison I ran was vacuous** (REPORT edge did not re-run;
  see §3). Recorded because a comparison of a file against itself returns a clean,
  decisive-looking Δ0 — the house failure family.

## 6. Deliberately not done

* **Did not install the `_S_sort` alias** and did not touch `scripts/symbol_aliases.json`.
* **Did not adjudicate or install any of the 59** installed memberships that newly prove
  — they are already installed; re-stating their evidence banks nothing.
* **Did not prune anything.**
* **Did not widen the general (non-vacuous) path's placeholder tolerance**, which the
  CD-9 note records as load-bearing.
* **Did not change memoisation semantics** — the vacuous branch still does not memoise,
  exactly as before, to keep the diff minimal.
