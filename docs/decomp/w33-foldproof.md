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

### Self-validation — the partition reconciles exactly, zero rows dropped

| quantity | genuine-name | save/restore | sum | expected |
|---|---:|---:|---:|---:|
| rows | 26 | 16 | **42** | 42 |
| bytes | 33,852 | 16,584 | **50,436 B** | 50,436 B |
| charged sites | 76 | 65 | **141** | 141 |
| distinct pairs | 54 | 52 | **106** | 106 |

and the fold adjudication covers all 54 genuine pairs
(20 `BOTH_MAPPED` + 33 `OURS_UNMAPPED` + 1 `NEITHER_MAPPED`).

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

⇒ **the map name is wrong** — the same adjudication shape MPNGAP-1 used to kill
`Handle@GemPlayer`: retail's callee's *shape* contradicts the name the map gives
it. The address is **`0x82682668`**.

⚠ **Stated to exactly the strength the evidence supports, and no further.** What
is proven is the NEGATIVE: a body that calls `__RTDynamicCast` and references two
RTTI type descriptors is a `dynamic_cast` thunk, and **an atexit destructor for a
static is not that**. It is *consistent* with our `?GetBandUser@BandUserMgr@@`,
but that match rests on placeholder-vs-named RTTI operands — which §5 just
established is **`UNDECIDED`, not proof**. Claiming the positive identification
here would be the very error §5 documents, one paragraph later.

⛔ **And renaming it would be actively harmful, which is why it was not done.**
The row lives in `default/CharLipSync`, and our `CharLipSync.obj` does **not**
define `GetBandUser` (our `BandUserMgr.obj` does). objdiff pairs by NAME, so
moving that name into CharLipSync's target obj would leave a base obj that cannot
define it ⇒ the row goes **permanently 0%**. This is precisely the documented
trap: *proving a name wrong ≠ renaming is safe.* The row is 28 B and its charging
row (`?OnMsg@OvershellSlot@@`, 1,396 B) carries 43 immediate charges, so per §4
the entire upside is **0 B** either way.
⚠ Note also that `??__FsLoadedFile@@YAXXZ` is defined in **three** of our objs
(`BandCamShot`, `CharLipSync`, `SkeletonClip`) — `??__F` names are per-TU
compiler-generated and collide freely, so pinning one to a single address is
fragile by construction.

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
* **Landed none of §9's six adjudications, deliberately.** Every one collects
  **0 B** by §4, so none can be justified on score; each needs its own settled
  whole-binary A/B (and, for the source ones, `native_build_gate.sh`) to land
  safely. Two carry specific measured hazards: **A and E are `inline`-removal
  levers**, whose blast radius is every TU that includes the header and which
  `/Ob2` may re-inline anyway (the sure lever is `__declspec(noinline)`); **D is
  a map rename**, the edit class where un-pairing is 80.5% of the delta. Landing
  four unmeasured edits at the end of a long lane to book accuracy credit would
  be exactly the failure mode this campaign keeps recording. They are queued
  below with their evidence so the next lane can price them **one per A/B run**.
* **Did not resolve the 33 `OURS_UNMAPPED` pairs.** Deciding them requires
  resolving retail's anonymous `lbl_<addr>` operands to their data content
  (vtable / RTTI identity). That is a real instrument and it does not exist yet;
  it is the only way this class could ever be settled.

## 9. The six strongest pairs adjudicated individually — 4 SOURCE_BUG, 1 MAP_ERROR, 0 folds

Because §5 refuted the fold for every `BOTH_MAPPED` pair, each of these is
MAP_ERROR or SOURCE_BUG by elimination, and each was then decided on retail
bytes. ⚠ **All of them collect 0 B** (§4) — they are **accuracy** findings, and
they are queued, not landed (§10). Map names checked against retail bytes for all
six: **only D is wrong.**

| # | pair | verdict | decisive retail-byte evidence |
|---|---|---|---|
| **A** | `PrintTick@VocalNoteList` vs `TickFormat` | **SOURCE_BUG** (inline policy) | `0x827809e0` tail-calls `0x827d1018` — it *is* `PrintTick(int){return TickFormat(...);}`. Retail makes 8 out-of-line calls across 4 fns; we make **0** because `VocalNoteList.cpp:12` marks it `inline`. |
| **B** | `substr(II)` vs `substr(I)` | **SOURCE_BUG** | `0x827be7e0` uses `r5` **and** `r6` (`strncpy` len=`r6`); `0x827be7a8` never touches `r6`. Retail 3×two-arg/0×one-arg; ours 2/1. |
| **C** | `String(const char*)` vs `operator=` | **MIXED — 2 of 4 flagged rows are NOT defects** | ⚠ see below |
| **D** | `PastFinalNote` vs `AtFirstPhrase` | **MAP_ERROR — the two names are TRANSPOSED; our source is right** | three independent instruments, below |
| **E** | `SetClipType@CharDriver` vs `SyncInternalBones` | **SOURCE_BUG** (inline policy) | retail's site loads `r4` then calls `0x8237a9e0`, which stores `r4` to `this+0x7c` — it takes an argument, so both map names are right. Our call site is **already correct**; `SetClipType` is defined inline in `CharDriver.h:91`. |
| **F** | `Find<Object>@ObjectDir` vs `FindObject` | **SOURCE_BUG** | `0x82270438` **calls** `0x82750188` at `+0x18` after `li r5,0` — template wrapper → implementation. Two genuinely distinct functions. |

★ **A refutes the hypothesis it was sent to test.** The "3+1+1 criss-cross"
across three retail names looked exactly like W31's SongDB **name permutation**.
It is not: relocation counts in `NotesDone` are retail **4× `PrintTick` / 0×
`TickFormat`** against ours **0 / 4**, with `SongFullPath` **6 vs 6** — one clean
4-vs-4 substitution smeared by objdiff alignment across the interleaved
`SongFullPath` calls. **The map is correct; the shape was an artifact of
alignment.**

★★ **D is the one real map defect, and it is proven three ways** — the strongest
single result of the adjudication:
1. **Vtable membership.** `0x826e5ae8` (map: *non-virtual* `QBA AtFirstPhrase`)
   sits in a `.rdata` code-pointer table at `0x820f1a34`; `0x826e5688` (map:
   *virtual* `UBA PastFinalNote`) has **zero** word references binary-wide. **A
   non-virtual method cannot occupy a vtable slot.**
2. **Slot alignment.** That table is VocalPlayer's vtable; our vtable has
   `GetStarRating` → `PastFinalNote` at Δ+0x10, and retail has `GetStarRating` at
   `0x820f1a24` → the mystery address at `0x820f1a34`, followed by the same six
   names in order.
3. **Field semantics.** The two bodies differ by one instruction: `0x826e5688`
   reads `+0` (begin) ⇒ "at first"; `0x826e5ae8` reads `+4` (end) ⇒ "past final".

⇒ the map has them **swapped**, and `VocalTrack::RebuildHUD` calling
`AtFirstPhrase` is correct.

⚠⚠ **C is the cautionary one, and it is why "grep the alias file first" is a
rule.** Two of the four rows flagged there (`PlatformMgr::SetRegion`,
`AccomplishmentProvider::Mat`) charge `??0String@@QAA@PBD@Z` against our
`??0String@@QAA@VSymbol@@@Z` — **already forgiven by ICF alias group 34**, as is
`ToUpper@FixedString`/`ToUpper@String` by **group 150**. Fixing those would have
been chasing forgiven noise. **Verdict: not defects.** Of the genuine remainder,
`File.cpp RecursePatternInternal`'s direction is known but the exact line is
**UNDECIDED**, and `PreloadPanel::Load` is a real missing statement whose exact
form is unpinned — with the element-type conclusion **explicitly withheld**
(retail also references `vector<Symbol>::erase`, so its `vector<String>`
`push_back` is a different container).

★ **B's root cause is the documented provenance trap, exactly as CLAUDE.md
predicts.** dc3 (which is *newer* than RB3) uses the one-arg `substr` at
`File.cpp:688`; the RB3-era rb3-Wii oracle uses the two-arg form at
`File.cpp:599`; **we copied dc3**, and retail agrees with rb3-Wii. Same shape as
W27's `MidiReader` MILO_WARN/NOTIFY finding.

⚠ **Semantics, stated honestly:** A, E and F are objdiff-"wrong callee" but
**behaviourally equivalent** — they are inline-policy and call-spelling
differences, not logic bugs. B is equivalent in effect (a two-arg `substr` with
an oversized length takes the same tail path). **The only true behavioural
divergence candidate is C's `PreloadPanel::Load`**, a missing statement.

⚠ **Instrument control worth reusing:** the VA reader was validated against
`band.exe` bytes with relocations masked **before** use — and its first PE parse
was off by one section-table field, producing *plausible but wrong*
disassembly. The control caught it. Plausible-looking disassembly is not
self-validating.

## 10. For the next lane

* ⛔ **Do not re-price this stratum off "50,436 B".** The collectable content is
  **0 B** and the reason is structural, not effort-shaped.
* ⛔ **`NAME-BLOCKED` is a charge-class label, not a diagnosis** — like
  `COLLECTABLE` (W27), `REGISTER_SWAP`, and objdiff's `AT_LIMIT`. Three lanes
  have now each independently found a confident label restating its own input.
* ⛔⛔ **THE `UNTRIAGED` TAIL IS THE SAME — DO NOT OPEN IT AS A NAME LANE.** §4's
  test was run over the **whole** frame queue, not just the top-60 window
  (178 rows at `fuzzy ≥ 50`, `tools/w23_collectable.py --min-fuzzy 50 --top 320`):

  | verdict | rows | bytes |
  |---|---:|---:|
  | NAME-BLOCKED | 108 | 88,340 B |
  | COLLECTABLE | 47 | 30,916 B |
  | NO-FRAME-SITE | 23 | 9,144 B |

  **Rows whose only charges are genuine relocation names: 0 rows / 0 B** —
  binary-wide across the queue, not merely in the head. And **50 rows /
  32,880 B** are blocked *only* by save/restore helpers (§3's class), i.e. 37%
  of the NAME-BLOCKED population by row count is a register count, not a name.
  ⇒ the §4 result is **structural, not a property of the window W23 happened to
  triage.**
* ★ **`tools/w33_fold_adjudicate.py` is reusable**, with its control. Run
  `--pairs` with the CTRL+/CTRL− fixture before believing any run: **it produced
  a false IDENTICAL once, and only a control that could fail caught it.**

### Queued accuracy fixes from §9 — one per A/B run, none of them worth bytes

| # | edit | risk |
|---|---|---|
| **B** | `src/system/os/File.cpp:771` — `pttn.substr((unsigned)forwardPos)` → `pttn.substr((unsigned)forwardPos, (unsigned)(pttnLen+1)-forwardPos)` (rb3-Wii's spelling; reproduces retail's 3/0) | **lowest — one line, oracle-backed** |
| **F** | `src/system/bandobj/BandCharacter.cpp` — `FindObject(...)` → `Find<Hmx::Object>(...)` at **2739**, **2753**, **2883**, **2904**; **2899-2901** `dynamic_cast<RndTransformable*>(...FindObject(...))` → `Find<RndTransformable>(...)`. Independent check: `__RTDynamicCast` count 4→3 | low, but 5 sites |
| **D** | `scripts/target_symbol_map.json` — swap `0x826e5688` → `?AtFirstPhrase@VocalPlayer@@QBA_NXZ` and `0x826e5ae8` → `?PastFinalNote@VocalPlayer@@UBA_NXZ` | **map rename — un-pairing risk; force a re-split** |
| **A** | `src/system/beatmatch/VocalNoteList.cpp:12` — drop `inline` on `PrintTick` | `/Ob2` may re-inline; header blast radius |
| **E** | `src/system/char/CharDriver.h:91-96` — move `SetClipType` out-of-line into `CharDriver.cpp` (`SetApply` just below has the identical shape) | same as A |
| **C** | `PreloadPanel::Load` — a missing `String` construction + `push_back`; exact form **unpinned** | ⛔ do not guess the container element type — explicitly withheld |
