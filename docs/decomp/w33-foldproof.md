# W33-FOLDPROOF — the 42 NAME-BLOCKED rows adjudicated on retail bytes: **zero are fold-blocked, and none can cross**

**2026-08-17.** Lane W33. Baseline re-derived in-worktree before any edit, never
inherited: `matched_functions` **44,508** · `matched_code` **3,767,864 B** ·
**36.507960%** · honest **21,598** · `total_code` **10,320,664** ·
`total_functions` **69,226**. Ruler **`name_check`**, read from `report.json`'s
own `provenance.diff_config` (22 keys, `ppc.calculatePoolRelocations=false`), not
assumed. Base `1fde5496` on `grounded2-restoration`.

The brief sent this lane at W23/W27's surviving stratum — *"42 NAME-BLOCKED rows
/ 50,436 B, needs retail-byte fold proof"* — on the premise that **our body may
already be right while objdiff charges the callee name**, resolving three ways:
a genuine ICF fold (⇒ alias), a wrong map name (⇒ map fix), or a wrong callee in
our source (⇒ the most valuable class of fix we have).

**That premise is false for all 42 rows, and the stratum is drained.** Two
independent findings, either of which alone closes it:

1. **The name charge is NEVER the binding constraint.** **0 of 42** rows have
   relocation names as their only charge. All 42 also carry immediate charges,
   41 carry hard instruction-level diffs, 40 carry register charges. Since
   `matched_code` keys on `fuzzy == 100` **all-or-nothing per row**, no alias, no
   map fix and no fold proof can move **one byte** of the 50,436.
2. **Nothing here is a fold.** **0 of 54** distinct name pairs are proven folds.
   On the sound test the fold hypothesis is refuted **19/19**.

And a third, which corrects the stratum's label at the source: **16 of the 42
rows (16,584 B, 32.9%) are not name-blocked at all** — their only name charges
are register save/restore helpers.

---

## 1. The anti-vacuity gate, run first and passed

The brief named this as the single most likely way the lane fails: a fresh
worktree's reflinked target objs are **pre-renamer**, so every retail mangled
name reads ABSENT until you build — and the resulting unanimous "refuted" agrees
with your prior. Built first, then asserted:

| tree | COFF symbols in target objs | mangled (`?…`) |
|---|---:|---:|
| main | 359,022 | 60,617 |
| this worktree | **359,022** | **60,617** |

Identical. Every negative below is a real negative.

## 2. The split re-derived, not inherited — and it reproduces exactly

| measure | W23 | W27 | **W33 (this lane)** |
|---|---:|---:|---:|
| frame-differing rows | 317 | 316 | **316** |
| NAME-BLOCKED | 41 / 50,076 B | 42 / 50,436 B | **42 / 50,436 B** |
| COLLECTABLE | 19 / 21,636 B | 18 / 21,236 B | **18 / 21,236 B** |

316 = W23's 317 minus `?GeoInit@@`, the one row W23 fixed and landed — the
detector clears exactly where a fix lands. The NAME-BLOCKED split reproduces
W27's to the byte on a third independent run.

## 3. ⛔ 16 of the 42 rows are MISLABELLED — the charge is a register count, not a name

Splitting the 141 charged name sites by what the two symbols actually are:

| class | sites | rows blocked ONLY by this | bytes |
|---|---:|---:|---:|
| `__savegprlr_N` / `__restgprlr_N` / `__savefpr_N` pairs | **65** | **16** | **16,584 B (32.9%)** |
| genuine function-name pairs | 76 | 26 | 33,852 B |

`__savegprlr_22` against `__savegprlr_23` means **retail saves one more
callee-saved register than we do**. That is a register-allocation fact wearing a
symbol name. There is no alias, map or callee lever: measured on retail bytes,
these helpers are **4-byte entries in a save/restore chain and adjacent ones are
NOT identical** (`__savegprlr_22`/`_23`, `__restgprlr_14`/`_15`,
`__savefpr_19`/`_20` — all three pairs differ). **An alias between them would be
a pure fabrication.** Every one of these 16 rows additionally carries 2–57 hard
diffs and up to 216 register charges, so their bodies genuinely diverge; permuter
is OFF by standing directive.

⇒ Same disease W27 named and the mirror image of it. W27: *"COLLECTABLE is
necessary, not sufficient — it describes the CHARGE CLASS, not whether a source
lever exists."* W33: **"NAME-BLOCKED" also describes the charge class, not the
binding constraint** — and here the name is never binding at all.

## 4. ★★★ The decisive number: 0 of 42 rows can cross

`matched_code` is all-or-nothing per row, so a row collects its bytes only when
**every** charged site closes. Charge profile of all 42:

| | rows |
|---|---:|
| rows whose ONLY charges are relocation names | **0** |
| rows also carrying immediate charges | **42** |
| rows also carrying hard (insert/delete/replace) diffs | **41** |
| rows also carrying register charges (permuter OFF) | **40** |

**Rows whose only charges are names: 0 rows / 0 B.** Confirmed against an
independent instrument — `report.json` gives `?SyncObjects@BandCrowdMeter@@`
`fuzzy=99.771736`, matching the classifier's 99.772.

⇒ **The entire 50,436 B is unreachable by this lane's tools *by construction*,
before any fold question is asked.** Proving every fold in the stratum, landing
every map fix, and fabricating every alias would move **+0 B**.

## 5. The fold question, adjudicated on retail bytes — 0 of 54 proven

`tools/w33_fold_adjudicate.py`. The instrument compares **retail's body for A**
(from the dtk-split target obj — retail machine code with map names applied)
against **our compiled body for B**, relocation-normalized **with relocation
target names compared, never masked**.

### The control failed first, in the direction that matters

A comparator that only ever returns one verdict proves nothing, so it was run on
known answers before any data: 5 pairs of a 100%-matching function **against
itself** (must read IDENTICAL) and 4 pairs of **unrelated** functions (must read
DIFFERENT). **4 of 5 positives failed**, and the failure diagnosed two defects:

* **The two legs were not the same measurement.** Our MSVC `/Gy` COMDAT section
  holds the parent body **plus its EH funclets**, while the dtk target extent
  stops at the next symbol. The size deltas came out as **multiples of 40** —
  exactly W23's funclet size. This is lane STLPORT-1's "+8 B" artifact in mirror
  image, and it is why *both* legs now derive the extent by the same rule.
* **Placeholder relocations.** Retail spells a float constant `lbl_8210C7F0`
  where we spell it `__real@3f8020c5` — the same datum. objdiff forgives
  placeholder targets (W19's rule); the comparator didn't, so a function scoring
  `fuzzy == 100` read SHAPE_ONLY.

After the fixes: **5/5 positives and 4/4 negatives correct**, the last positive
explained — its single differing name pair (`??2CriticalSection@@SAPAXI@Z` vs
`??2@YAPAXI@Z`) is **already forgiven by a proven alias group**
(`operator_new_alloc_thunk` @`0x827bd2f0`). Existing alias coverage is
**reported, never silently applied** — these groups are the thing under audit,
and applying them would let a fabricated alias validate itself.

### ⛔⛔ Then the fixed instrument produced a FALSE `IDENTICAL`

It reported `??1ObjRefConcrete<FlowLabel>` ≡ our `<EventTrigger>`, "bodies and
all 9 relocation target names equal". **It is not a fold.** A retail-vs-retail
control caught it: retail's own two dtors differ at exactly one relocation —
`lbl_8201BA34` vs `lbl_8202158C`, the **per-type vtable pointer**. Our side
spells that operand `??_7ObjRefConcrete<EventTrigger>@@6B@`; retail spells it
anonymously; the comparator forgave it as a placeholder.

⇒ **Placeholder forgiveness is correct for SCORING and unsound for FOLD PROOF.**
objdiff forgives a placeholder because it cannot be spelled wrong. But a fold
proof asks whether two bodies are the *same code*, and **the datum that
distinguishes two template instantiations is frequently ANONYMOUS** — a vtable
or RTTI pointer. Forgiving it makes provably different functions read identical.
This is the masked-`bl` trap moved from a call target to a data pointer, and it
is now encoded in the tool: a placeholder-vs-named relocation returns
**`UNDECIDED`, never proof.**

### The sound test, and its unanimous answer

**Retail-vs-retail** comparison is the sound ICF test: both sides use one naming
scheme, so anonymous labels are directly comparable, and both extents are derived
identically — the one-sided-reader hazard cannot apply by construction.

Run on all 20 `BOTH_MAPPED_DIFF_ADDR` pairs (both names resolve to *different*
retail addresses in `target_symbol_map.json`, so retail holds both functions
separately):

| result | pairs |
|---|---:|
| **NO FOLD** — size differs | 14 |
| **NO FOLD** — relocation targets differ | 3 |
| **NO FOLD** — body differs | 2 |
| not testable (symbol absent) | 1 |
| **FOLD_CONSISTENT** | **0** |

**19 of 19 testable pairs refute the fold.** For the remaining 33
`OURS_UNMAPPED` pairs the fold is **UNDECIDABLE by body comparison** — retail's
distinguishing data references are anonymous, and 15 of them are `SHAPE_ONLY`
(shape-identical, relocation-target-different), which per CD-7 is *precisely the
population MSVC does NOT fold*: folding requires identity **including
relocations**. `_List_base<T>::clear` survives at 42 addresses for this exact
reason.

**Total proven folds in the stratum: 0 of 54 pairs.**

## 6. ✅ One real defect proven on retail bytes: a MAP error

`??__FsLoadedFile@@YAXXZ` (charged in `?OnMsg@OvershellSlot@@`) names an address
whose retail body is a **28-byte thunk that calls `__RTDynamicCast`** and
references **two RTTI type descriptors**. `??__F…` is an **atexit destructor
thunk for a static** — a function that cannot possibly have that body. Our side
spells the same site `?GetBandUser@BandUserMgr@@` (`User` → `BandUser`
`dynamic_cast`, RTTI `??_R0?AVBandUser@@@8` / `??_R0?AVUser@@@8`), whose shape
matches retail's exactly.

⇒ **the map name is wrong and our source is right** — the same adjudication
shape MPNGAP-1 used to kill `Handle@GemPlayer`. **Not fixed here**: the row it
sits on (`?OnMsg@OvershellSlot@@`, 1,396 B) also carries 2 hard diffs and **43
immediate charges**, so per §4 correcting it collects **0 B**, and a map rename
is the edit class with the worst measured risk/reward (un-pairing is 80.5% of a
map edit's delta). Recorded for a lane that can price it against the cascade
channel.

## 7. The one row that is *nearly* actionable, and why it still isn't

`?SyncObjects@BandCrowdMeter@@` — 1,344 B, `fuzzy` 99.772, **zero hard diffs,
zero register charges**; the only near-crossing row in the stratum. Fully
diagnosed:

* Retail reuses **one** 16-byte stack temp at `r31+88` across all five
  iterations; we allocate **five distinct slots** (88/104/120/136/152). That is
  exactly the frame delta: 288 − 224 = 64 B = 4 surplus slots × 16 B.
* Its 10 name charges are 5× the `ObjRefConcrete` dtor (**refuted as a fold**,
  §5) and 5× a `push_back` pair (`SHAPE_ONLY`, undecidable).

So crossing it needs **all three** of: a stack-temp source fix, an alias that is
now refuted, and an alias that is unprovable. ⚠ And the source lever is the one
**W23 §4 already refuted** on this exact shape — `(void)FilePath(fp);` and a
brace-less `else` both compiled **byte-identical**, recorded in
`src/system/synth/SampleData.cpp` so nobody retries it. Not retried.

## 8. What this lane deliberately did NOT do

* **Landed no alias.** 0 of 54 pairs are proven, and §4 shows an alias would buy
  **0 B** even if proven. Adding one would lift `name_check` while `none` stayed
  flat — the **ALIAS_SUSPECT** signature, which on a map-only patch is the
  integrity hazard, not a win.
* **Ran no whole-binary A/B.** With no candidate edit, an A/B would price
  nothing. `ab_measure` would have been the tool if any edit had survived §4.
* **Withdrew nothing and pruned nothing.** Classes forgiving 0 today become live
  as porting advances; a prior prune cost +94,616 B to reverse.
* **Did not fix the `??__FsLoadedFile` map row** — §6.
* **Did not resolve the 33 `OURS_UNMAPPED` pairs.** Deciding them requires
  resolving retail's anonymous `lbl_<addr>` operands to their data content
  (vtable / RTTI identity). That is a real instrument and it does not exist yet;
  it is the only way this class could ever be settled.

## 9. For the next lane

* ⛔ **Do not re-price this stratum off "50,436 B".** The collectable content is
  **0 B** and the reason is structural, not effort-shaped.
* ⛔ **`NAME-BLOCKED` is a charge-class label, not a diagnosis** — like
  `COLLECTABLE` (W27), `REGISTER_SWAP`, and objdiff's `AT_LIMIT`. Three lanes
  have now each independently found a confident label restating its own input.
* ★ **The frame queue's remaining value, if any, is the `UNTRIAGED` tail** (257
  rows / 107,124 B, untouched by W23/W27/W33) — but price it with §4's test
  *first*: rank by *"are names the only charge?"*, not by prize.
* ★ **`tools/w33_fold_adjudicate.py` is reusable**, with its control. Run
  `--pairs` with the CTRL+/CTRL− fixture before believing any run: **it produced
  a false IDENTICAL once, and only a control that could fail caught it.**
