# W16-GD — one fold installed (+1 fn / +1,608 B / Δhonest +1), one REFUSED with the blocker named

Date: 2026-09-16 · branch `w16-gd` · worktree off main `7718d60c`
Patch class: **map-only** (`scripts/symbol_aliases.json`, 5 insertions / 2 deletions).

Two targets were briefed. **One installed, one deliberately NOT installed.**
The negative is the more useful half of this document.

---

## 1. INSTALLED — `push_back<TourDescEntry*>` into group 10

Survivor `?push_back@?$vector@PAVChatReceiver@@…@Z` @ `0x82b5f808` (group idx 10),
folded member added: `?push_back@?$vector@PAVTourDescEntry@@…@Z`. 11 → 12 members.

### Headline

| measure | leg A | leg B | Δ | predicted |
|---|---:|---:|---:|---:|
| `matched_functions` | 44,136 | 44,137 | **+1** | +1 ✓ |
| `matched_code` | 4,166,060 | 4,167,668 | **+1,608 B** | +1,608 ✓ |
| `masked_equal_functions` | 23,323 | 23,323 | +0 | +0 ✓ |
| **honest** (`matched − masked_equal`) | 20,813 | 20,814 | **+1** | +1 ✓ |
| `matched_code_percent` | 40.656116 | 40.671810 | +0.015694 pp | 40.671810 ✓ |
| `fuzzy_match_percent` | 50.494835 | 50.494840 | +5.0e−6 pp | 50.494837 ✗ **MISS** |

One crossing row, `masked_equal` **False** ⇒ the honest floor moves:

| row | size | fuzzy | mpn |
|---|---:|---|---|
| `?Configure@TourDesc@@UAAXPAVDataArray@@@Z` | 1,608 B | 99.987564 → **100.0** | 99.987564 → **100.0** |

Units at 100 (mpn) **194 → 195**, exactly **1 gained** (`default/band3/tour/TourDesc`
59/60 → 60/60, class `MATCHED_ROSE`), **0 lost, 0 unit regressions on either ruler**.
Leg A reproduced my own pre-patch baseline to the last digit; 0 recompiles on both legs.

### The pre-registered miss, stated plainly

**P6 (`fuzzy_match_percent`) MISSED.** I predicted +0.000002 pp (→ 50.494837) by
modelling the aggregate as a size-weighted mean of per-row fuzzy:
`1608 × (100 − 99.987564) / 10,247,068 = 2.0e−6`. Measured **+5.0e−6** (→ 50.494840),
2.5× my figure. I had flagged this prediction as low-confidence on the last digit
and it deserved that flag: the aggregate is evidently **not** a plain size-weighted
mean of the per-row percentages. Every other pre-registered key hit exactly. Recording
it because a pre-registration whose misses get quietly dropped is not a pre-registration.

### Why this was a stronger prior than a fresh claim

This was a **gap in already-installed coverage**, not a new assertion. The same
`vector<TourDescEntry*>` instantiation already had its siblings folded — `erase` in
group 3, **both** `_Vector_base<PAVTourDescEntry>` and `vector<PAVTourDescEntry>`
ctors in group 46 — and only `push_back` was missing from group 10. Verified in-tree,
not inherited.

### The adjudication (this is the clearance — the metric is not)

* **FLAT T1 REFUTED, which is the EXPECTED reading.** Both COMDATs are 112 B with
  identical masked bodies; relocation *targets* differ because each names its own
  per-`T` callees. `/OPT:ICF` is iterative, so a literal-name comparator calls the
  fold signature a template-twin refutation.
* **CHASED T1 PROVEN.** Every differing slot bottoms out in the already-landed
  allocator chain `_M_insert_overflow` → `MemOrPoolAlloc`/`MemOrPoolAllocSTL` →
  `PoolAlloc` → `operator new`, leaf pair `??2CriticalSection@@SAPAXI@Z` /
  `??2ChunkAllocator@@SAPAXI@Z` reported **VACUOUS-BUT-IDENTICAL** — byte- *and*
  relocation-name-identical, i.e. `/OPT:ICF`'s own folding condition with nothing masked.
* **Read off retail's OWN relocation, not searched for.** Retail's `Configure` carries
  a `bl` at file offset **0x618** whose relocation *names this survivor*, opposite our
  `bl` naming the folded spelling. There is no best-candidate step, so there is no coin
  flip to lose — the `--chase` safety property.
* **Second witness, sharing no arithmetic with the recursion.** `--family` partitions by
  (masked body, slot-0 callee) and collapses **142** of our `push_back<T>` spellings onto
  **one** retail address. The discriminator is not a blur: it **excludes** retail's second
  masked-body twin `?push_back@?$vector@VSymbol@@…@Z` @ `0x822d16f8` (W16-FZ's separately
  landed group), i.e. it *separates* the two retail survivors instead of merging them.
* **Instrument control.** `--chasetest` passed all four controls in this worktree —
  in-family decoy `fn_827B0E78` correctly **REFUTED**, flat-T1 group **PROVEN**,
  self-pair negative **REFUTED**, self-pair positive **PROVEN**. The tool printed
  *"selftest PASSED — the instrument can both pass and fail"*. The PROVEN verdict is a
  measurement, not the detector restating its input.

### Anti-vacuity proof for my own instrument

1. **Post-renamer tree asserted, not assumed.** `collect` saw **3,112 target objs →
   69,432 symbols**. A reflinked worktree carries target objs *without* the pre-compile
   renamer's effect, so every retail mangled name would read "absent" and the adjudicator
   would return `UNDECIDABLE: survivor absent` — a vacuity that **agrees with a refuted
   prior**. I built the worktree fully before adjudicating anything.
2. **Charge-count self-validation against the ruler.** An independent slot census of
   retail-vs-our `Configure` finds **118 name-differing relocation slots of 277**, of
   which only **3 are non-placeholder**, and **2 of those 3 are already forgiven**
   (`?Int`/`?Array` via group 1391 `DataNodeAssertOnlyAccessor`; `??2CriticalSection`/`??2`
   via group 1546 `operator_new_alloc_thunk`). That leaves **exactly one** charged site —
   which reproduces objdiff's `diff_score` of **5 / 40200** exactly (one `diff_arg` = 5).
   My census and the scoring path agree without sharing code.
3. **Block priced, not row.** W16-FZ's correction #1 was to census *our* objs for callers
   of the folded spelling. Done: **exactly one** function references it, at **one** site —
   `TourDesc::Configure` itself. Here the block genuinely *is* the row, so +1/+1,608 was
   the right prediction rather than an under-count.

### A comparison-that-could-not-fire, caught

My first pairwise slot comparator read the reloc tuple's **last** element as the symbol
name. The tuple is `(offset, name, type)`, so I was comparing the **type integer**, which
is equal on both sides by construction — it reported a clean, decisive **"all 22 slots
SAME"**, flatly contradicting the flat-T1 refutation. Caught only because it disagreed
with a verdict I already had. Re-run keyed on index 1 **with a control** (12 slots must
read SAME for the comparator to be live), it gave 10 differing slots. Same family as
W16-FZ's trap #2 and the house `grep`/`all([])` traps: *a screen that cannot fire returns
a confident answer*.

### Liveness instruments (gated on the right pipeline)

| instrument | leg A | leg B |
|---|---|---|
| `Loaded … ICF equivalence entries` (read from the **report** run that produced the score) | 6,104 | **6,105** (+1) |
| rendered `icf_aliases.map` symbol lines | 6,920 | **6,921** (+1) |
| rendered groups | 1,608 | **1,608** (unchanged — no new group) |
| `icf_alias_finder --validate` | PASS, 0 CONTRADICTED | PASS, **0 CONTRADICTED** |
| bucket counts (OK / tolerated / total) | 1,408 / 250 / 1,659 | **unchanged** 1,408 / 250 / 1,659 |
| `member spellings looked up` | 7,142 | **7,143** (+1) |

**The +1 counter model is confirmed against W16-FZ's +2.** Adding a membership to an
**existing** group contributes **one** previously-absent name; creating a **new** group
contributes two. Both readings are now measured, on consecutive days, on the same counter.

⚠ **`renamer_patched` — a refinement to W16-FZ's coordinator note, not a contradiction.**
That note records F5 reading *"0 files patched"* and concludes an alias-only edit never
traverses the renamer. **Under `ab_measure` my run reads `renamer_patched: 1833`, on both
legs**, and `ab_measure` classifies `scripts/symbol_aliases.json` as kind `map` and would
have **hard-refused** on `rp == 0`. Both are correct: `ab_measure` *forces* a re-split
(rm the renamer stamp + `touch config.yml`), which re-runs the renamer over the freshly
split target objs, so it patches ~1,833 files regardless of whether the alias map changed.
A *plain* build with a current stamp patches 0. ⇒ **the number measures the forced
re-split, not the alias edit** — it is alive as a build-state gate and **vacuous as an
alias-liveness gate** in either direction. The gates that actually sit on the scoring path
are the equivalence-entry counter and the rendered map line count.

### The integrity position, stated without spin

`ab_measure` fired, exactly as pre-registered:

```
[control none] ALIAS_SUSPECT: default ruler UP (+1608 B) while `none` is FLAT
on a map-only patch — the FABRICATED-ALIAS shape.
```

`delta_matched_code_none` = **0**, `delta_matched_code_default` = **1608**. **This is
neither a finding against the lane nor, had it been absent, a clearance.** An alias lifts
`name_check` *by construction*; a fabricated alias produces a bit-identical shape. The
metric and the `none` control are **structurally incapable** of adjudicating this edit and
are offered as evidence for nothing. The clearance is the retail-byte adjudication above.

⚠ Equally: the group sits in **`OK (MAP-CONSISTENT)`**, and its bucket **did not move**
(1,408 before and after). That bucket was renamed from `OK (grounded)` precisely because
it asserts *map-consistency*, **not** proof of folding. `VALIDATE: PASS` is not a
clearance. (W16-FZ's group landed in `TOLERATED`; mine lands one bucket up. Neither is
proof.)

---

## 2. NOT INSTALLED — `_S_sort<unsigned int>` ↔ `_S_sort<Symbol>`

Target: `?CopyTypeProperties@@YAXPAVObject@Hmx@@0@Z`, **1,472 B**, unit
`default/system/obj/Utl` (the brief said `default/Utl`), fuzzy 99.945656,
`diff_score` **20 / 36800** ⇒ exactly **4** `diff_arg` charges, all the same callee pair.

**`CHASED T1: REFUTED`. Not installed. Δ0 on every key, because no edit was made.**

### The source-defect hypothesis is REFUTED — we are not sorting the wrong container

The brief correctly flagged that if retail genuinely sorts a `list<unsigned int>` where we
build a `list<Symbol>`, that is a **source** defect and an alias would be *concealing* it —
the standing counter-example where 8 memberships forgave our use of the wrong overload.

It is not that. Retail's body is *named* `_S_sort<unsigned int>` but its callees are named
for **`BSPFace`**, **`SynthPollable*`** and **`LocalePanel::Entry`** — **three different
`T` in one function.** A genuine `_S_sort<unsigned int>` instantiation would call
`unsigned int`-named callees. So retail's name here is a fold-arbitrary survivor spelling,
exactly the signature `chase()`'s own docstring describes. Corroborating: both sides are
**424 B** (equal size, so the wrong-overload size tell is absent), and the `less<>`
comparison is **inlined** — no relocation slot — hence inside the masked body, which
compares **identical**. `Symbol` is a single `const char*` (`src/system/utl/Symbol.h:13`)
compared with `<` on the pointer (line 25), which is the same `cmplw` an unsigned-int
`less<I>` emits.

### Slot census — 10 of 22 relocation slots differ by name

| slots | retail | ours | status |
|---|---|---|---|
| 11, 12, 14 | `list<BSPFace>::swap` (204 B) | `list<Symbol>::swap` (204 B) | **PROVEN** (flat + chased T1, `retail_bodytwins: 1`) |
| 20 | `_List_base<SynthPollable*>::clear` (88 B) | `_List_base<Symbol>::clear` (88 B) | **PROVEN** (flat + chased T1, `retail_bodytwins: 1`) |
| 3, 7 | `lbl_8243F3C0` | `??_F…list<Symbol>` (32 B) | placeholder ⇒ tolerated by `chase`, and `name_check` forgives placeholder targets |
| **1, 5, 15, 17** | `__destroy_aux<LocalePanel::Entry>` (4 B) | `~list<Symbol>` (4 B) | **the sole blocker** |

(Control: 12 slots read SAME, so the comparator is live.)

### ★ The blocker, named precisely — it is ONE bounded instrument gap, not four

Those four 4-byte bodies are **not** `blr`. Their masked body is `00000000` because the
entire instruction **is** the relocated field: each is a **4-byte tail-call thunk**
(`b <target>`, relocation type 6 at offset 0). Their targets are:

* retail → `?clear@?$_List_base@PAVSynthPollable@@…@Z`
* ours   → `?clear@?$_List_base@VSymbol@@…@Z`

— **which is precisely the pair I independently adjudicated as flat-T1 *and* chased-T1
PROVEN with `retail_bodytwins: 1`** (slot 20, above). So the four failing slots tail-call a
pair that is *already proven to fold*.

`chase()` refuses because its vacuous branch requires the pair be
`VACUOUS-BUT-IDENTICAL` — masked body **and the full relocation list including target
names** literally equal — and it does **not** recurse into a vacuous pair's relocations.
Here the bodies are identical and the relocation *offset and type* agree; only the target
*names* differ, by a fold that is separately proven. All four slots are the same pair, so
**a single relaxation would decide all of them.**

This is the same shape the chase's own comment records for `list<char*>::insert`, which
blocked lanes W8-D and W9-B — with one difference that matters: there the thunk's
relocation named the **same** symbol on both sides, so `VACUOUS-BUT-IDENTICAL` admitted
it. Here it names a **proven fold pair**, which the branch has no way to express.

### Why I did not relax it, and what the next lane must do

The guard is load-bearing and its docstring says so: *"STRICTNESS: full equality is
required, relocation target names INCLUDED. A vacuous pair whose reloc names differ still
REFUSES, so this cannot admit a template twin."* Relaxing it is a change to the
**adjudicating instrument**, not to data — a far larger commitment than this lane was
chartered for, and precisely the move that must never be made by the lane that benefits
from it. Overriding a `REFUTED` verdict with my own reasoning is the failure mode the
integrity brief exists to prevent, so I did not.

**For whoever picks this up** — the change is bounded and gateable:
1. In `chase()`'s vacuous branch, when bodies are identical and every relocation agrees on
   **offset and type**, recurse on differing target names instead of demanding literal
   equality.
2. Re-run `--chasetest` and require **all four** existing controls still discriminate —
   the in-family decoy `fn_827B0E78` must stay **REFUTED** (it fails at `BYTES-DIFFER`,
   not at a vacuous slot, so it should be unaffected — *verify, do not assume*).
3. Add a **new** control that the relaxed branch still refuses a template twin: a vacuous
   thunk pair whose targets are **not** a proven fold must still REFUTE. Without that, the
   relaxation is untested in exactly the direction it widens.
4. Only then re-adjudicate this pair. Prize if it proves: **+1 function / +1,472 B**,
   `masked_equal` **False** ⇒ Δhonest **+1**.

**Do not install this alias on the strength of this document.** Everything above is
consistent with a real fold and none of it is a proof the house instrument accepts.

---

## Deliberately not done

* **Did not install the `_S_sort` alias**, and **did not modify `icf_pair_adjudicate.py`**
  (or any tool) to make it pass.
* **Did not run `tools/alias_apply_withdrawal.py`** — W16-FV found its hardcoded note
  stamps a reversed predicate.
* **Did not add the other 141 `push_back<T>` family spellings** that `--family` collapses
  onto `0x82b5f808`. Each is a separate assertion needing its own adjudication, and our
  objs reference the folded spelling from **only** `TourDesc::Configure`, so they would buy
  **0 bytes** while widening the integrity surface.
* **Did not prune anything.** `STALE_SPELLING` (88) and `UNWITNESSED` (101) untouched;
  both forgive 0 bytes today and become live as porting advances, and a prior prune cost
  **+94,616 B** to reverse.
* **Did not touch** `?Handle@TourProgress@@…@Z` (2,596 B) — out of scope by brief; its two
  charges are `add r4,r4,r9` vs `add r4,r9,r4`, commutative-operand order, permuter class.
* **Did not fix** the other 4 unmatched rows in `default/band3/tour/TourDesc` (now 60/60 —
  actually complete) or anything else in `default/system/obj/Utl`.
