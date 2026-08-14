# TEMPLATE-1's (C) class is mostly FOLDS, and its prescribed action cannot pay

**Lane CVEIN-1, 2026-08-14.** Measured at `34b0a56a`, ruler `name_check`,
baseline `matched_functions 44,361 / matched_code 3,624,188 / code% 35.115840 /
total_code 10,320,664`.

**Verdict: `C_PROVEN_CALLEE` is NOT a "fix our source" vein. Its defining
action — change the template argument in our source — CANNOT collect its bytes
for most of the class, because our source spelling is already RIGHT. The charge
is a relocation NAME, and retail folded the callee into another instantiation.
The collecting action is an ALIAS.** Harvested **+3,220 B / 23 rows** that way.

## The census reproduces; the interpretation does not

Re-running `tools/tmplscan.py` on the current tree reproduces TEMPLATE-1's
figures exactly — 105 pairs / 121 sites / 117 rows / 18,240 B — so this is a
correction to the *reading*, not to the measurement. (ONMSG-1's +12,444 B landed
in between and did not touch this stratum.)

### 18,240 B was never the collectable number

| quantity | value |
|---|---:|
| census, per-pair bytes summed | 117 rows / **18,240 B** |
| **union of rows** touched by ≥1 C pair (pairs share rows) | 106 rows / **15,752 B** |
| rows FULLY covered by C pairs (can cross at all) | 73 rows / **9,768 B** |
| rows at 1-of-N charged sites — uncollectable via this class | 33 rows / **5,984 B** |

The per-pair `bytes` column double-counts rows charged by more than one pair, so
the headline overstates the row union by 2,488 B before any adjudication.

## Why the class is misread: `fold == DIFFERENT` is absence of proof

TEMPLATE-1 was careful that `mapname == REFUTED` is "merely ABSENCE of proof"
because our tree only matches ~35%. **The same asymmetry applies to the other
leg of the gate and was not carried over.** `fold == DIFFERENT` asks *"do OUR two
bodies differ?"* and treats YES as proof that retail kept both. But our two
bodies can differ because **our own instantiation of one of them diverges** —
for reasons that have nothing to do with the template argument.

Worked example, `?Draw@TrainerGemTab@@QAAXH@Z` (656 B, the class's second-largest
row). Retail's `Draw` calls an address the map names
`vector<RndLine::Point>::_M_erase`; we call `vector<TrainerGemTab::ExtraTail>::_M_erase`.
The gate reports `LEN_DIFF` (96 B vs 88 B) + `mapname CORROBORATED` ⇒ (C).

Adjudicated on retail bytes, every step of that reading fails:

* **Retail's own `DrawExtraTails` uses stride `0x48`** (`addi r26,0,72`,
  `addi r29,r29,72`) ⇒ retail's `ExtraTail` is **72 bytes = `sizeof(RndLine::Point)`**.
  Our build matches that row at **100.0000%**, so our size is right too.
  ⚠ The header comment `// size 0x38` is simply wrong — headers lie.
* The 96 B / 88 B difference is **an inline decision, not a size**: Point's
  `_M_erase` inlined the `memcpy` loop; ours calls `__copy<ExtraTail*>`
  out-of-line. Same algorithm, same element size.
* **Retail's fan-in proves the fold outright**: address `0x8247b020` is called by
  **both** `vector<RndLine::Point>::resize` and `TrainerGemTab::Draw`. One
  function cannot be two instantiations — internal inconsistency.

⇒ (A), not (C). And **no source edit could have collected it**: our source
correctly erases a `vector<ExtraTail>`; the name differs only because retail
folded and the map named the survivor `Point`.

⚠ The 5.8% FP calibration could not have caught this. It was scored against T1
alias pairs — pairs where our build **already folds correctly by construction** —
so it never sampled the population where our own codegen breaks the fold. Same
shape as `project_control_stratum_mismatch_2026-08-13.md`: a control drawn from
the wrong stratum cannot fail.

## Splitting the class on a cheap, hard discriminator

**If our spelling has its own address in the map, retail kept both ⇒ no fold.**

| | pairs | bytes |
|---|---:|---:|
| our spelling **IS** map-resident ⇒ NOT a fold (genuine defect / map error) | 43 | 6,812 |
| our spelling **NOT** map-resident ⇒ fold candidate | 62 | 11,428 |
| …of which internal inconsistency is demonstrable | 55 | 10,252 |
| …already touching `symbol_aliases.json` yet still charged (ONMSG-1's incomplete-group class) | 30 | 4,356 |

## What was harvested

38 groups / 40 folded spellings installed for the **fresh** subset (survivor
map-resident, folded spelling has no retail address, neither already grouped,
internal inconsistency demonstrable).

* **Pre-registered: +3,328 B / 24 rows, Δmatched_functions 0.**
* **Measured: +3,220 B / 23 rows crossed, 0 fell off, Δmatched_functions +0,
  Δcode% +0.031200pp.**
* The −108 B miss is **one fully-attributed row**:
  `_M_fill_insert@vector<LightPreset::SpotlightDrawerEntry>` went
  **99.6296 → 99.81481** — it improved but carried a *second* charged site the
  census under-counted. Mispriced, not failed. No compensating errors.

### Evidence tier, stated honestly

Every group carries the **map-naming inconsistency** witness: the map names the
CALLING instantiation with our template argument and the CALLEE with a different
one, and the calling row matches at `mpn == 100`, so its identity is corroborated
by bytes. A template instantiated for T2 cannot call a helper instantiated for T1.

**21 of 38 carry a second, independent witness** — heterogeneous retail fan-in on
one address (e.g. one `__adjust_heap` address called by both
`__make_heap<EventEntry,MaxSort>` and
`__partial_sort<PrefabChar*,SortPrefabByPortraitFileName>`). The remaining 17
rest on the map-naming inconsistency alone. Each group records `retail_fanin`,
`retail_callers` and `witness` so the split is auditable.

⛔ **Disclosure, and it is the uncomfortable part: for all 38 groups our compiled
body for the folded spelling is NOT byte-identical to our body for the survivor
spelling — that is true BY CONSTRUCTION, since `fold == DIFFERENT` is what put
them in class (C).** The fold claim therefore rests entirely on retail-side
evidence; our build does not reproduce it. That is defensible here because the
divergence is demonstrably in OUR instantiation (TrainerGemTab: an inline
decision, with retail's own stride and fan-in proving the fold), and because the
alias forgives only the **call-site name** — the folded spelling has no retail
address, so its body is not a scored row and no scored defect is being hidden.
But it does mean each group is one retail-side witness away from being wrong, and
a reviewer should read `retail_fanin` / `witness` before trusting one.
⇒ The 17 single-witness groups are the ones to re-audit first if any of this is
ever questioned.

⚠ **`ab_measure` fired `ALIAS_SUSPECT` and that is EXPECTED, not a clearance
failure**: this is a map-only patch, so `none` is flat (+0 B) while `name_check`
moves (+3,220 B) — the documented signature. The `none` control is
**structurally incapable** of adjudicating an alias; only the retail-byte
evidence above can, which is why it is recorded per group rather than inferred
from the delta.

## What was NOT done

* **The 43 pairs / 6,812 B where our spelling is map-resident were not touched.**
  Retail kept both bodies there, so they are genuine wrong-callee or map defects —
  the real (C1) vein, and the only part of this class a source edit could pay for.
  They need per-row retail-byte adjudication, not aliases.
* **The 30 pairs / 4,356 B already touching `symbol_aliases.json` were left
  alone.** They are ONMSG-1's incomplete-group class, but several have the shape
  `RN=survivor, ON=survivor-of-another-group`, which is a claim that two
  *distinct* retail addresses fold — that needs the group-merge question settled
  by the map owner, not a unilateral merge by this lane.
* No source was edited, so `src/` is untouched and the native gate does not apply.

## Instrument note for whoever works InterstitialMgr next

The `hashtable<Symbol,DataArray*>` family handed over by ONMSG-1 was examined and
**not harvested**. Every uncrossed row in the unit sits at `mpn == 100`, i.e. the
whole ~3.1 kB residual is relocation-name charges — but the biggest apparent
lever is a mirage:

⛔ **A naive target-vs-ours relocation-name diff OVERCOUNTS the charged set,
because it applies neither forgiveness rule.** Such a diff reports 41 pairs / 70
sites for this unit. Its top two entries are both already free:
`lbl_*`/`fn_*` targets are **placeholders**, which `name_check` forgives
(`is_placeholder_symbol_name`), and its largest real-looking entry —
`DataNode::Int` (retail) vs `DataNode::Array` (ours), 6 sites — is **already an
installed alias** (`DataNodeAssertOnlyAccessor`, `0x8274B0F8`).

That pair is worth keeping as a worked example of the fold criterion even so: our
compiler emits the two spellings **byte-identical (36 B) AND
relocation-identical** (both bodies are just a call to `DataNode::Evaluate`,
because `MILO_ASSERT` compiles out), and only `Int` has a retail address. That is
CD-7's ICF criterion met exactly — and note it is **not** a template-argument
case, so `tmplscan` structurally cannot see it.

⇒ **Take the charged-site list from `report.json`, never from a hand-rolled
reloc diff.** The hand-rolled version is shaped like a large fresh vein and is
mostly already-forgiven sites.

---

# RE-AUDIT of the 17 single-witness groups — lane ALIASAUDIT-2, 2026-08-14

CVEIN-1 closed by naming its own weak point: *"the 17 single-witness groups are
the ones to re-audit first if any of this is ever questioned."* This is that
re-audit. **Verdict: 4 KEEP, 13 WITHDRAW.**

## What survived audit, and what did not

| CVEIN-1 claim | re-audit result |
|---|---|
| `retail_fanin == 1` on all 17 | ✅ **reproduced independently** (own `bl` scan of all 10.3 MB of `.text`, resolving callers through `.pdata`) |
| survivor addresses are trustworthy names | ✅ **0 of 38** sit in the map's own `_icf_arbitrary` / `_bijection_arbitrary` / `_denylist` non-evidence lists |
| the `Draw@TrainerGemTab` worked example | ✅ **all four assertions verified independently** (see below) |
| *"for all 38 groups our folded body is NOT byte-identical to the survivor — true BY CONSTRUCTION"* | ⛔ **FALSE.** For **7 of the 17** our folded body is byte-identical to retail@A on every **non-relocated** word |
| *"the folded spelling has no retail address of its own"* | ⛔ **FALSE for group `CVEIN1_sort` @ `0x8280d3e0`** |

⛔⛔ **The `fold == DIFFERENT` gate compared RAW bodies, and raw body comparison
is the instrument CLAUDE.md already documents as silently vacuous.** Two
COMDATs that differ only in *relocation targets* read as "different" to a raw
comparison, which is precisely CD-7's *shape-identical* class. So the gate's
"by construction" disclosure was not a property of the class at all — it was an
artifact of the comparator. Re-run reloc-normalized, 7 of 17 flip.

⛔ **And the rival-address exclusion compared mangled names EXACTLY**, so an
anonymous-namespace hash difference walked straight through it:
`sort<UIListWidget*, WidgetDrawSort@?A0xd29bb4b7>` (ours) vs
`…@?A0x530db9db` (map) is the *same instantiation*, and it already lives at
`0x826715e8`. That group belonged in the 43 pairs CVEIN-1 explicitly declined
to touch ("our spelling IS map-resident ⇒ not a fold").

## The standard applied

A group is KEPT only on all three legs. Leg 2 is the independent witness; leg 1
alone is CD-7's shape-identical class, which **survives 3,967 times** and
therefore proves nothing on its own.

1. **Our build reproduces the fold** — our compiled body for the folded spelling
   is byte-identical to retail@A on every non-relocated word.
2. **An independent retail-byte witness that the RELOCATIONS coincide** —
   heterogeneous retail fan-in on a *callee* address, i.e. one address reached
   from two type-distinct instantiation families. Same argument as the 21, one
   level down.
3. **No unresolved contradiction** — no rival address holding the same
   instantiation, no name-injectivity violation.

## KEEP (4)

| group | A | second witness |
|---|---|---|
| `_M_erase@vector<CharHair::Strand>` ← `vector<Flow::DynamicPropertyEntry>` | `0x823aab50` | **both** callees: `Strand::operator=` `0x823aa650` ← `_M_erase@vector<DynamicPropertyEntry>` `0x823aad78`; `Strand::~Strand` `0x823a8c40` ← `_Destroy_Range<DynamicPropertyEntry*>` `0x823a94f0` + `PropSync<DynamicPropertyEntry>` `0x823aae58` |
| `_M_erase@vector<OutfitConfig::MatSwap>` ← `vector<TransformArea>` | `0x822a8c40` | `MatSwap::operator=` `0x822a7d10` ← `_M_fill_insert_aux@vector<TransformArea>` `0x822a8cb8` (×3) + `_M_erase@vector<TransformArea>` `0x822a92a0` |
| `__median<UIListWidget*,WidgetDrawSort>` ← `<UILabel*,LabelSort>` | `0x8280aed8` | all **5** comparator relocs resolve to `0x8280ad68` `??RWidgetDrawSort`, whose 12-site fan-in includes **4 LabelSort-named** helpers |
| `__unguarded_partition` (same pair) | `0x8280c480` | same, both relocs → `0x8280ad68` |

★ The UI pair is mechanistically complete: once `LabelSort::operator()` and
`WidgetDrawSort::operator()` share one address, every `<T*, Compare>` helper
over 4-byte pointer elements becomes **reloc-identical** — exactly CD-7's
folding criterion — which is why `__median`/`__unguarded_partition` fold while
`sort`/`__introsort_loop` (which call *other* helpers) do not.

## WITHDRAW (13)

* **8 on size alone** — different-size COMDATs cannot fold:
  `__median` `0x82554118` (184 vs 188) · `__unguarded_partition` `0x82554600`
  (128 vs 124) · `__median` `0x826978b0` (164 vs 152, **and A is not even a
  `.pdata` BeginAddress**) · `erase` `0x82328c90` (96 vs 108) · `_M_erase`
  `0x822a4f58` (100 vs 96) · `_M_erase` `0x822a6e60` (104 vs 100) ·
  `__linear_insert` `0x826712c0` (108 vs 124) · `__partial_sort` `0x82348420`
  (152 vs 204).
* **2 on differing code at equal size** — `__linear_insert` `0x82554680`
  (8 of 27 words differ) · `__partial_sort` `0x82554d10` (18 of 39).
* **1 on a rival address** — `sort` `0x8280d3e0`: our folded spelling already
  lives at `0x826715e8`, and our body's two callees
  (`__introsort_loop`/`__final_insertion_sort<UIListWidget,WidgetDrawSort>`)
  resolve to `0x82349770`/`0x82671390`, **not** to the `0x8280d2c0`/`0x8280cd58`
  that retail@A actually calls. Retail kept both chains.
* **2 for no obtainable second witness** — `_M_insert@hashtable` `0x825afb38`
  and `erase@list<OldMMInst>` `0x824e0ea0`. Both reproduce the fold on our side
  and have no rival, but **every callee in their closure has retail fan-in of
  exactly 1**, so no heterogeneous-fan-in witness exists anywhere to obtain.
  These are withdrawn as *unproven*, not as *refuted* — they are the two most
  likely to be re-instatable if a different instrument is found.

⚠ **`_M_erase` has 2-arg and 3-arg overloads, and they are different functions.**
A screen keyed on the class template arguments alone reports the 3-arg sibling
as a "rival" for the 2-arg group. Three of five rival hits were this (or a
`StlNodeAlloc` vs `TransformListAlloc` allocator difference) and were adjudicated
away. Conversely this is exactly the hazard that motivated the re-audit — an
alias that forgives *our use of the wrong overload* — so the screen is worth
keeping and reading, not tightening into silence.

⚠ **A map name can be refuted by its own fan-in, and my reloc-resolution test
inherits the map's errors.** Group `0x822a8c40` first read REFUTED because the
map places `TransformArea::operator=` at `0x822a96a8`. But `0x822a96a8`'s entire
fan-in is `vector<OldColorOption>`, so that row is itself a map defect — not a
rival. **Adjudicate every MISMATCH on fan-in before believing it.**

## Scope, and what was NOT done

* **The 21 dual-witness groups were NOT re-adjudicated** — their witness sits at
  A itself and does not depend on our bodies. They were given the two cheap
  screens that caught defects here: **0 true rivals**, and their bodies show the
  same divergence pattern, which is expected and is CVEIN-1's own point.
* `scripts/symbol_aliases.json` is the only functional change; `src/` is
  untouched, so **the native gate does not apply** (stated, not skipped).
* `icf_alias_finder.py --validate`: **PASS, 1518 groups, 0 contradicted.**

## The `Draw@TrainerGemTab` reframing HOLDS — independently verified

All four assertions verified, one of them stronger than claimed: `0x48` is the
**divisor over `end - begin`**, so it is unambiguously `sizeof(element)`, not an
incidental constant; the element is read at `+0x40`/`+0x44`, impossible in a
0x38-byte struct. Fan-in on `0x8247b020` reproduces exactly (2 sites, 2 callers).
Two refinements worth carrying:

* **The header comment defect is wider than reported** — all three `ExtraTail`
  comments are stale by +0x10, because `Transform` is **64 B**, not 0x30. It is
  a *comment* defect only: the declaration compiles to the correct 72.
* **`72 == sizeof(RndLine::Point)` is a NECESSARY CONDITION, not evidence.** It
  shows our size is right; it does not carry the fold. The fan-in does.
* ⚠ **Which spelling "survived" is conventional and unprovable from bytes** —
  ICF folds identical bodies, so the bytes are equally consistent with either
  name. What is proven is the load-bearing part: **one address serves two
  instantiations.**
