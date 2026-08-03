# INSTRUMENT DESIGN — how to build a scanner/control/census that can actually FAIL

> **STATUS (2026-08-02):** current. Written by lane DH-1 from the 23-commit
> DC..DG wave (`eda76311`..`1cbcabc8`) and the five lane memories it produced;
> amended same day with rules 14–15, the never-links blind spot (`dce343a1`,
> `362217af`), model-version staleness, and the DH-3 tool promotions.
> This doc is about **instruments**, not levers — for what to *work on*, see
> [playbooks/levers-that-pay.md](playbooks/levers-that-pay.md).
> ⚠ This doc changes no source, map or splits, so it **cannot move the metric**;
> no A/B was run and none is reported. `ab_measure` would correctly refuse it as
> absent-vs-absent.

**Purpose.** Across DC..DG the tree went **43,527 → 43,590 matched** (+63) and
**~149 → 176 units at 100%** (+27), at Δ`total_code` 0 for the attribution work.
Nearly every large step in that range came not from a clever fix but from
**catching an instrument that was wrong in a way that looked like success, or
like a decisive negative** — DC-2 counted its own as the *eighth
structurally-incapable-of-firing instrument of the session*, and three more
landed after it. A wrong lever costs a lane; a wrong *instrument* closes a vein
for everyone, permanently, and leaves no trace that it did. Below: the shapes
those failures take, each with a worked case, then the rules that catch them.

---

## The table of shapes

| # | shape | it looks like | worked example | tell |
|---|---|---|---|---|
| 1 | **Vacuous control** | proof | DE-1: `?Load@StarDisplay@@` at 100% "proves" rev-store addressing — it is a **vbase adjustor thunk with zero rev stores** (`c14bba5c`) | control cannot physically exhibit the phenomenon |
| 2 | **Silently-vacuous scanner** | a decisive negative | DC-2's shape scan: regex wanted one space, asm has two ⇒ **every body parsed empty, 0 hits** (`1ed4b1e8`) | zeros, and no known-positive was asserted first |
| 3 | **One-label classifier** | a finding | DF-2's word-order detector: **43/43 ALTREV_FIRST** — indistinguishable from `return ALTREV` | output has one value across the whole population |
| 4 | **Base-rate error** | a classifier | carve predictor 81.5% applied to DE-3's 32 anon rows; measured **9.4%** (`8e6dfc27`) | rate measured on a population, applied to its complement |
| 5 | **An improvement is a false positive** | a confirmed fix | DF-3's Campaign container swap **improved 73.30 → 89.00** and was still wrong (`dbab6082`) | you adjudicated on the metric, not on retail bytes |
| 6 | **Unsettled measurement** | success, with the sign inverted | DF-2 read **+23 matched / 17 at 100**; settled A/B: **zero at 100**, most worse | number came from anything but `ab_measure.py` |
| 7 | **Stale census** | a property of the tree | DG-2: **2 of 23** candidates already landed *by the lane that produced the JSON* (`1cbcabc8`); and a ceiling of 70 quoted from a **superseded model** where the current one says 81 (`362217af`) | census consumed after anything landed — or a number carried over from an older MODEL |
| 8 | **Structurally blind metric** | a clean 100% | `mpn` masks reloc args ⇒ wrong callee / wrong container reads 100 (five lanes, five waves); and the match build **never links**, so a link break is invisible (`dce343a1` — ⚠ but the *predicted* second instance was FALSE, see §8) | the defect lives in a field the ruler normalizes away — or in a failure class the build cannot witness |

---

## 1. Vacuous control — structurally incapable of exhibiting the phenomenon

**DE-1 (`c14bba5c`).** Asked whether a plain class-static `LOAD_REVS` form
reproduces retail's single-base rev addressing, the lane pointed at
`?Load@StarDisplay@@`, sitting at **100%**, and concluded yes. But
`?Load@StarDisplay@@` is a **vbase adjustor thunk** — 30 instructions forwarding
through vtable slots, containing **no rev stores at all**. Its 100% meant
nothing: it could not have failed, whatever the truth. The real body is
anonymous (`fn_8231CDD0`) and unpaired, so the question had no evidence at all.

⚠ **And it is not a one-off — it recurs at population scale.** DF-2 swept
`?Load@Cls@@`-prefixed rows and found **76 of 77** apparent "already at 100% on
the DC3 dialect" rows were adjustor thunks. The same vacuity that cost DE-1 one
control would have supplied DF-2 an entire fabricated population.

⇒ **Before a control's PASS is worth anything, show the control could have
FAILED.** For a code control that means: does the body physically contain the
construct under test? Thunks, funclets, getters and 8-byte stubs almost never do.

## 2. Silently-vacuous scanner — zeros shaped exactly like decisive negatives

Three instances in this wave alone, all shaped like an answer:

- **DC-2 (`1ed4b1e8`).** The first shape scanner for the ObjPtr ctor store order
  returned **zero hits** because its regex expected one space where the asm has
  two, so **every body parsed empty**. Caught only by asserting a known-positive
  FIRST; the committed scan now **hard-asserts** it. Re-run correctly: **124
  sites in retail's order, ZERO in ours** — the finding that justified going
  ungated and paid **+13**.
- **DE-1 (`c14bba5c`).** The rev-idiom scanner v1 reported "NO rev idiom" for EQ
  and BitCrush, nearly written up as a per-unit divergence. Its regex demanded a
  literal `sth rX, 0x0(rY)`; retail renders the same store as
  `sth r10, lbl_82E03D34@l(r29)`, and the control passed only because Chorus
  happened to use the `0x0` rendering. ★★ **A control passing on one asm
  rendering does not validate another.** All 7 units were in fact identical.
- **DG-4.** A span-location census asked "which container symbols are *located*
  in the span" and read empty on a known positive. The lane **discarded the tool
  rather than reading its zeros**, noting `LessonMgr` had "passed" only
  *vacuously* — empty is its expected answer either way. The working question was
  what the unit **calls**.

Plus the standing house case: **`grep` in an agent's shell is binary-blind**
(ugrep `-I` via a shell function) and yields only false negatives on `.obj` /
`band.exe`. See `tools/grep_binary_guard.py`.

⇒ **A zero is a claim about the world only if the instrument has been shown to
produce a non-zero on a case you already know the answer to.**

## 3. One-label classifier — indistinguishable from a constant function

**DF-2.** The word-order detector reported **43/43 `ALTREV_FIRST`** across the
target population. That output is consistent with a correct detector *and* with
`def classify(x): return ALTREV_FIRST`, and nothing in the run separates them.

The cross-check that established discrimination: run it on **DE-1's 9 known
`REV_FIRST` bodies** — it returned `REV_FIRST` **9/9**. Only then was the 43/43
a finding, and it was **load-bearing in the opposite direction from the
prior**: every DF-2 target is altRev-first, the **opposite** of DE-1's struct, so
copying DE-1's field order would have been wrong every single time.

⇒ **A uniform output is not a result until the instrument has produced the other
label on a known-opposite case.**

## 4. Base-rate error — a population rate applied to its own complement

- **DE-3 (`8e6dfc27`).** The carve predictor says anon rows **≤100 B pay 81.5%**,
  **>100 B pay 0.12%**; applied to DE-3's 32 anon candidates it predicts a rich
  vein. Measured: **3 shipped / 32 = 9.4%** — an **~9× over-prediction**. Why:
  the 81.5% is paid by **funclets auto-pairing on bytes** (fork
  `pair_funclets_by_bytes`), and these 32 are **by construction the complement** —
  precisely the residue byte-pairing already failed on. In the wild: `StreamNull`
  has 8 unnamed target fns, only 1 below 100; the other 7 carry the MSVC X360
  funclet prologue and paired automatically. ⚠ Note the direction — carve lanes
  historically **under**-predict, so the reflex correction was also wrong here.
- **DG-2 (`1cbcabc8`) — the positional null.** "The blocker sits at a pin tail"
  was DF-4's core boundary-move filter. Null: **53.0% of the 66 one-away
  anon-blocked units** have the blocker at a pin tail vs **42.3% expected by
  chance** = **1.25× enrichment** — and **12 of the 66** are single-function
  units where "tail" is guaranteed and carries zero information. The predicate is
  consistent with *both* over-reach *and* a unit's last function merely lacking a
  map row.
- The refuted **`name_check` fold-alias model** is the same failure at **1.95×**,
  used as a deterministic classifier.

⇒ **Before funding a vein, measure the untreated population.** DG-4's container
work states the rule cleanly: **20.0% of container-calling units (43/215) are
HASHTABLE-only**, so "this unit calls `hash_map`" fires on 1 in 5 units by
default — independently confirming DF-3's measured **~75% false-positive rate**.

## 5. An improvement can be a false positive

**DF-3 (`dbab6082`).** `Campaign` was called a container-member-type defect
*first*, on a "confounder-immune counting argument" (3 `hash_map` ctors vs 2
declared members + matching value type) — **the argument was itself a
confounder**. Under the swap the ctor **improved 73.30 → 89.00**, and that
improvement was the evidence. It was wrong: `Campaign`'s 472-byte
`_Rb_tree<Symbol,pair<const Symbol,Symbol>>::insert_unique` **matches retail at
100%**, so the swap was a fabrication that happened to shuffle codegen
favourably. Only **retail bytes** refuted it. `BandProfile` looked positive on
the same reasoning and measured **−73**.

⇒ **A local percentage rise is not adjudication.** The wave's whole adjudication
stack — retail RTTI COLs, retail `bl` targets, xrefs, vtable slots, byte
conservation — exists because the metric cannot referee identity. See also
`project_metric_blind_to_attribution_2026-07-30` (64 deliberately wrong names
measured identical).

## 6. Unsettled measurement reads as success — with the sign inverted

**DF-2 (nothing landed; the lane is the refutation).** Incremental `report.json`
reads taken **inside the worktree during apply/revert cycles** claimed
**+23 matched and 17 bodies at 100%**. The settled whole-binary A/B showed
**zero bodies at 100** and most **worse**: `CharPosConstraint` **82.25 → 34.48**,
`RndLight` **80.44 → 37.10**. Cause: dirty-obj contamination.

⇒ ★★★ **An unsettled in-worktree read is not a weak measurement, it is a WRONG
one, and it is shaped exactly like success.** It is the same family as
"deltas compose, absolutes do not", but sharper — here the **sign** was wrong,
not the magnitude. **Only `tools/ab_measure.py` output is quotable** (settle-to-
zero-work, `report.json` + `report.cache` wiped per read, ruler sha256 pinned
across legs, strict key reads, refusal on any precondition failure).

## 7. Stale census — a snapshot, never a property

**DF-4 (`5f7bf5b4`)** shipped the reachable-ceiling census (1,024 units) and a
23-row boundary-move shortlist. Both aged immediately:

- **2 of the 23** boundary-move candidates were **already landed by DF-4 itself**
  — the JSON predates its own commit (`1cbcabc8`).
- **3 of DG-1's 65** "one map row from 100%" units were **already at 100%**
  (Biquad, Server, TrackerSource), fixed by DF-4's own boundary moves after the
  census was computed (`bce10a25`). Net: DG-1 shipped **3 of 65 = 4.6%**, so the
  headline was **~21× optimistic**.
- The bucket **label** was also wrong for **31 of the 65**: `MAP_ONLY` was read
  as "no source lane can ever finish these", but for 31 our source *does* contain
  the method and only the body diverges. Read `MAP_ONLY` as "the leftover is
  anon", not "unreachable by source."

⇒ **Re-derive a census at HEAD before consuming it, and stamp every census with
the commit it was computed at.** ★ Stronger, and now the house requirement: **a
census that ships as a static JSON list must either be regenerable on demand or
carry a staleness refusal.** DF-4's aged **inside a single wave** — 2 of its 23
candidates were fixed by its own landing — so "recompute it if it looks old" is
not a workable discipline; the artifact has to refuse. `tools/reachable_ceiling.py`
is that census made executable (staleness refusal exit 3).
⚠ And note the census's own stated limit: it **cannot detect a bogus pin** —
`HamDriver`, `FilterQueue` and `SkeletonDir` all sit in COMPLETABLE and are
exactly the metric-fitting cases already rejected.

### ★ The other staleness axis: MODEL VERSION, not time

A number can be stale without a single commit landing. **A ceiling quoted from a
superseded model is indistinguishable from a measured one.** Live instance from
this wave: two lanes were briefed that DG-3 "beat the reachable-ceiling census"
— `default/Font` reaching **72/109 against a predicted 70** (`362217af`). DH-4
re-derived it with the tool instead of taking the report: the **70** came from
the **superseded two-way anon model** (DF-1's `named 32/38, anon 32/71 ⇒
reachable 70/109`, `369273db`), and the corrected **three-way** model — anon
classifies three ways, not two, since **1,696 rows sit strictly between 0 and
100** — gives a ceiling of **81**, inside which 72 sits unremarkably. *(DH-4's
re-derivation; not re-run here.)* ⇒ **The propagated claim was true of the brief
and false of the tool.**

★ And DG-3 had already named a second mechanism pushing the same way: the static
model **held the anon pool constant**, but the layout fix moved it **32 → 37**,
because fixing layout lets funclet byte-pairing find more anon rows. A ceiling
computed from a snapshot therefore *understates* what a layout fix can reach.

⇒ **Re-derive an inherited number with the CURRENT instrument before letting it
steer funding — the model version is part of the number's provenance.** Quote a
ceiling as "81 (three-way model, `tools/reachable_ceiling.py`, at `<commit>`)",
never as a bare integer.

## 8. Structurally blind metric — the ruler normalizes the defect away

`match_percent_normalized` is computed with `functionRelocDiffs=none`:
`reloc_eq` returns true **regardless of target name**. A **wrong callee** and a
**wrong container** therefore read a clean **100**. This bit five lanes in five
waves:

- **DE-2 (`d7a9775a`).** `TourWeightManager::ConfigureQuestWeightData` **read
  100% the whole time** while calling the wrong container's `operator[]`.
  Believing that 100% would have closed the vein.
- **DD-2 (`81d23046`) corrects DC-3.** LayerDir's "retail streams `list<Symbol>`
  but our member is `list<Layer>` ⇒ wrong member type" was a **misdiagnosis** —
  the callee name is a **relocation argument** and cannot witness a type
  mismatch; retail's writer had ICF-folded with `list<Symbol>`'s. No member-type
  change was needed. Same artifact as `BandIKEffector` calling
  `?Save@FlowValueCase@@` for `CharWeightable` and matching 100% untouched.
- **DF-3 (`dbab6082`), fifth confirmation.** `SetPropertyValue` reads 100%
  **before and after** the fix.
- **DC-4 (`dcd456f6`)** sized the class exactly: 219 rows / 101,996 B /
  0.954244pp of ruler disagreement, of which **14 rows / 1,152 B are counted in
  `matched_functions` WITH A GENUINELY WRONG CONSTANT** — PPC shift/`rlwinm`
  fields render Opaque and normalize away (`Character` `fn_822A49EC`
  `slwi r3,r11,5` vs `,4`: a ×32-vs-×16 struct stride).

⇒ Two operational consequences, both counter-intuitive and both house policy:
1. **Expect a correct fix in this class to measure Δmatched 0 — and land it
   anyway.** DF-3 landed `TourPropertyCollection` and `NextSongPanel` at Δ0 on
   verified-correct member types (obj confirmed emitting
   `hash_map@VSymbol@@M`, zero regressions).
2. **A metric that hides a real bug is worse than a lower metric.** DC-1
   (`eda76311`) deleted 39 proven-false map rows at **−0.014467pp fuzzy** and
   Δ0 on both headline rulers, precisely because the fuzzy was credit never
   earned.

### ★ The same blindness one level down: the match build NEVER LINKS

The ruler is not the only instrument with a structural blind spot — so is the
**build**. The X360 match build compiles TUs and diffs objects; it never links,
so **an undefined symbol is invisible to every match-side instrument** (objdiff,
`report.json`, `ab_measure`, unit percentages all read perfectly clean). Twice in
this session a landed decomp change broke the native build this way:

- **`dce343a1`.** DD-2's `RndEnvAnim::Save` (`81d23046`) shipped `bs << mKeysOwner`,
  the tree's third `ObjOwnerPtr` save site. `operator<<(BinStream&, const
  ObjOwnerPtr<T1>&)` is **declared** (`obj/Object.h:760-761`) with its definition
  **commented out** (`ObjPtr_p.h:485-486`); being an exact match it **beats the
  base-class overload that IS defined**, so the call compiles clean and fails only
  at link. It reached main and left `rb3-milo`/`rb3-render` NOBINARY.
- **`362217af` — ⛔ THE SECOND INSTANCE NEVER HAPPENED, and correcting it is the
  more useful lesson.** DG-3 predicted the same breakage in advance (*"a link
  break is EXPECTED … `native_link_glue.cpp` instantiates `operator<<` for
  `ObjPtrList`/`ObjPtrVec`/`ObjOwnerPtr`/`ObjDirPtr` but NOT plain `ObjPtr<T>`"*),
  a coordinator propagated it, and `c833a0fe` landed a fix for it. **Lane DJ-4
  ran the gate: PASS 18/18 — and then removed `c833a0fe`'s two instantiations
  and it still linked 18/18.** The fix is **inert dead code**; the break was
  never real.
  ★ **The reasoning had inverted a load-bearing detail.** `bs << mMat` binds to
  `operator<<(BinStream&, const ObjRefConcrete<T1,ObjectDir>&)`, which is
  **defined** at `ObjPtr_p.h:156` and implicitly instantiated. The exact-match
  `ObjPtr<T1>` declaration at `Object.h:686` is **commented out**, so it never
  enters overload resolution — **that is precisely what makes plain `ObjPtr`
  SAFE.** `ObjOwnerPtr` broke for the **opposite** reason: its declaration
  (`Object.h:761`) is **live** while its *definition* is commented out, so the
  exact match wins and has no body. Same two files, same commented-out lines,
  **opposite consequence** — and the near-identical surface is exactly why the
  prediction felt sound.

⇒ **Know which failure classes your build structurally cannot witness, and name
the instrument that can.** Here that instrument is **`tools/native_build_gate.sh`**
— cheap, and the only thing standing between a match-positive change and a
silently broken native tree.
⚠ **The recurrence signature is NARROWER than first recorded.** Not "a new
`bs <<` over a smart-pointer member" — that over-fires, and did. It is:
**a `bs <<` over a smart pointer whose exact-match `operator<<` is *DECLARED*
(definition commented out or absent).** A commented-out *declaration* is safe;
a live declaration with no definition is the trap.
★★ And note how the gate was kept honest: in a bare worktree `rb3-milo` and
`rb3-render` **silently skip** unless `MILO_ENGINE_PATH`/`Dawn_DIR` are seeded,
which would have made the whole run **vacuous for exactly the two targets that
matter**. DJ-4 seeded them, *then* proved the gate could fail by removing the
instantiations. A PASS from an unseeded gate is worth nothing.

---

## DESIGN RULES

Each rule with the one-line evidence hook that earned it.

1. **Assert a known-positive BEFORE trusting any zero — and hard-assert it in the
   committed scan.** *DC-2's first shape scanner returned zero because its regex
   wanted one space and the asm has two; the committed version now asserts the
   positive (`1ed4b1e8`).*

2. **A control must be able to FAIL: pair every empty-check with a deliberately
   sabotaged/mispaired leg.** *DD-3's opcode-multiset differ ran **300/300
   identical bodies → empty AND 198/200 deliberately mispaired → non-empty**;
   ★★★ **the empty-check alone is vacuous — a differ hardcoded to return nothing
   passes it** (`5d8fc966`).*

3. **Run the null / untreated population before funding a vein.** *DG-4:
   HASHTABLE-only base rate **20.0% (43/215)**; DG-2's pin-tail predicate
   **1.25×**; DC-4's raw class-naming channel only **1.70×** enriched while
   "names an UNCLAIMED class" is **42.5×** — and a callee-graph channel **failed
   its null and was not used**. Cf. the dead-index census and random-offset
   nulls.*

4. **A one-label output is not believed until the instrument produces the other
   label on a known-opposite case.** *DF-2's 43/43 ALTREV_FIRST became a finding
   only after returning REV_FIRST 9/9 on DE-1's known bodies.*

5. **Predict before measuring; state Δmatched AND Δbytes; use byte-sum
   corroboration BOTH ways.** *DE-1's **+2,212 B equals the sum of the 8 target
   sizes** (516+336+292+292+284+220+144+128) to the byte — confirmation
   (`c14bba5c`). DF-2 predicted **+4,236 B** from 17 bodies reaching 100 and
   measured **−1,344** — **the failing byte-sum was the tell** that the whole
   sweep was wrong.* ⚠ And the two rulers can disagree by construction: DG-1's
   byte-exact row paid **all 560 B** while two arg-only rows paid **+1 matched
   each with +0 bytes** (`bce10a25`), so quote both or you cannot tell a real Δ0
   from a blind one.

6. **Refuse rather than return a misleading zero — the house style.**
   *`ab_measure.py` exits **2 with no delta keys** on any precondition failure;
   `grep_binary_guard.py --self-break` proves it can fail; DC-4's collapsed-read
   refusal was **shown able to fire in both directions** (sabotage → REFUSED with
   no output written, real input → passes) with its **threshold set AFTER
   measuring** (real 20.6% passes / name-desynced 0.0% refuses) (`dcd456f6`).*

7. **Anti-vacuity guard for any masked byte comparison: ≥4 real (non-relocated)
   words AND ≥50% of body, as a HARD REFUSAL.** *DG-1 **retired** six 8–16 B rows
   (StreamNull, HamLabel, Cache, DeJitterPanel, FreestyleMotionFilter,
   StoreSongSortNode) rather than re-funding them: 1–3 real words each against
   1–25 same-size and 3–36 same-class candidates ⇒ **no future evidence at that
   body size can decide them** (`bce10a25`). This is the guard that once
   "matched" a 16 B thunk to `FastCos`.*

8. **Never use a SUFFICIENT test as a NECESSARY one.** *DD-4 (`b206d005`): T1 is
   written as sufficient ("retail kept exactly the code F compiles to ⇒ one
   body") but was used as necessary, and `reject_RETAIL_DIFFER` is a hard
   `continue` that preempts T2/T3. On the 7 PA/PB template twins retail differs
   from **both** our spellings equally (retail 96 B/3 relocs vs both ours
   104 B/5) ⇒ **the test would have rejected the survivor as an alias of
   itself**. Verdict flipped from "withdraw 8" to **RETAIN 7, WITHDRAW 1**.*

9. **Byte identity proves what a function EQUALS, not where it LIVES.** *DD-4:
   three repoints passed a strong masked-byte test (differs from retail at X,
   **equals** retail at Y); the **home-unit gate** killed all three — Y lands in
   `BandStorePanel.cpp` / `Text.cpp` / `Character.cpp`, TUs that would never
   instantiate those templates. The bodies are shape-identical across many `T`
   (8/14/6 retail bodies matched ours), so **"exactly one candidate" was
   elimination in disguise** — ByteGrinder has 3, not 1.*

10. **A confirm rate cannot retire or establish a defect class — only a control
    can size it.** *DF-3's confounder-immune joint scan agreed **5/5** with our
    source and found zero defects **in its coverage**; the class was sized
    instead by adjudicating 8 candidates → 2 real defects (**~75% FP**), and
    closed only by DG-4's exhaustive pass over **46 Symbol-keyed 0x18-container
    members in 29 units → zero**. Cf. the bijection-arbitrary result: a 73.8%
    confirm rate coexisting with **5.41× enrichment** in wrong methods.*

11. **Coupled halves can MULTIPLY — decompose every time, and never read a
    negative half as refutation.** *DF-1 (`369273db`): source-only **−3**,
    map-only **−4**, **combined +1** — an 8-point swing, because 7 map entries
    hard-code `RndFontBase` inside their mangled names and four were at 100%
    (988 B). Same shape at `47907c6f`: the splits half **alone is negative**
    (HitTracker drops off 100 while TrackerSource joins it), coupled with the map
    row it is **+1 matched and +1 unit at Δtotal_code 0**. ⚠ **Neither
    composition nor multiplication is a law** — DD-2 and DD-3 composed **exactly**
    (43,556+12; 38.9428+0.025821; 45.99719+0.025501) on disjoint files. Decompose
    and measure; do not assume either.*

12. **Prefer a non-metric, ground-truth definition where one exists.** *DF-4
    re-derived the anon gate as **"a sub-100 anon row is precisely a retail
    address absent from `scripts/target_symbol_map.json`"** and demonstrated it on
    `Pool.s` (four `fn_` rows; three in the map at 100%, `0x827d9640` absent and
    reading 0) — sharper than the inherited "anon can't pair", which DF-2 showed
    is **too strong** anyway (104 of 161 complete units contain an anon row at
    100 via fork byte-signature pairing). The true statement is **"anon residue
    is not reachable BY SOURCE EDITS."***

13. **Report the instrument bugs you caught — they change the numbers you already
    published.** *DF-4 self-caught three, all affecting reported figures; the
    pin-tail test's exact end-equality was defeated by **4 bytes of alignment
    slack** and alone **under-counted the tail class 20 → 35** (`5f7bf5b4`).
    DC-3 found `mech_taxonomy.py`'s positional-vs-raw COFF symbol index skew on
    **2,561 of 2,563 symbols**, which had made "class A is EMPTY: 0 of 1,211" a
    false negative (`ebca36e8`) — the **third** distinct instance of a COFF
    symbol-indexing assumption failing silently.*

14. **Name a bucket after WHAT WAS MEASURED, never after an inferred remedy.**
    *DF-4's census labelled a bucket `MAP_ONLY` and glossed it "no source lane
    can EVER finish these" (`5f7bf5b4`). The **measured fact** was only "the
    leftover rows are anon"; the **remedy inference** — "therefore only a map row
    can fix it" — was false for roughly half the bucket, and the label is what
    lanes read. DG-1 found **31 of its 65** one-away entries had the method in
    our source with a diverging body (`bce10a25`), and DH-3's fresh run of the
    promoted census tool reaches the same conclusion from the other direction:
    **25 of 50 anon-blocked one-aways are SAMECLASS_DIFFSIZE source work, and
    `MAP_FIXABLE_CANDIDATE` = 0 on the current tree** (DH-3's measurement, not
    re-run here). The bucket is now `ANON_BLOCKED`, carrying the measured-fact
    legend. ⇒ A remedy is a hypothesis about a bucket; the bucket name is read as
    a verdict, and a wrong one **routes budget away from work that would have
    paid**.*

15. **Re-derive an inherited number with the current instrument before it steers
    funding — model version and range are part of provenance.** *A ceiling quoted
    from a superseded model is indistinguishable from a measured one: DG-3's
    "beat the ceiling, 72 vs 70" was true of the brief and false of the tool —
    the 70 was the two-way anon model, the three-way gives 81 (shape 7 above).
    Carry the model, the range and the commit with the number, never a bare
    integer.*
    ★ **Lived instance, and note where the number came from.** This doc's own
    brief headlined **~+1,600 matched** for waves DC..DG. Tallying every landed Δ
    in the supplied 23-commit range gives **+63** (43,527 → 43,590). Both figures
    are real: **+1,646** is the *whole-session* total (41,952 → 43,598) spanning
    ~eight earlier waves whose commits were **outside the evidence set**, and it
    was mislabelled onto the shorter range — the coordinator's own correction.
    ⇒ **A figure whose citation cannot support it is the fabricated-baseline
    hazard** (20 workflows once hardcoded fake baselines ⇒ +34k phantom deltas),
    and **authority is not provenance**: the number arrived from the coordinator,
    in a brief, and was still wrong. Two lanes independently declined to carry it.
    **Check the citation, not the sender.**

---

## Fixtures available in this repo

Use these rather than hand-rolling; each already embodies a rule above.

- **`tools/grep_binary_guard.py`** — builds its own binary fixture so it can
  never skip, reconstructs the shell shim in a subshell to test the *actual*
  risk, and **`--self-break`** proves the guard can fail. The reference
  implementation of rule 6.
- **`tools/ab_measure.py`** — the whole-binary A/B protocol as a tool: settle-to-
  zero-work, `report.json`+`report.cache` wiped per read, exact-key parsing,
  forced re-split for map/splits patches, absent-vs-absent detection, ruler
  sha256 pinned across legs. **Exit 2 = REFUSED, no delta keys published.**
  **`--selftest`** drives every refusal branch without building, and is itself
  validated to FAIL under a sabotaged log classifier. Rules 1, 2, 6.
- **`tools/symbols_fixpoint_guard.py`** — `assert_plausible()` is a compact model
  of the vacuity trap ("empty == empty reads as CONFIRMED; refuse to compare
  anything we have not proven we actually read"), plus `perturb_refragment()`, a
  byte-conserving deliberate perturbation to prove the comparison can fire.
- **`tools/masked_byte_identity.py`** — the masked byte comparator with the
  **anti-vacuity guard as a hard refusal** (rule 7: ≥4 real non-relocated words
  AND ≥50% of body). Report coverage with every verdict — DD-1's stride audit
  carries the same guard and passed at 67–85% of body.
  ⚠⚠ **Passing the guard is NOT identification.** On compiler-generated
  destructors the bodies are *shape-identical*, and DJ-4 measured this tool
  returning **7 exact candidates for one 76-byte body**. Byte identity is
  necessary and nowhere near sufficient — **the callee discriminates**, and you
  must resolve `bl` targets rather than mask them: DJ-4's first comparator
  masked branch displacements and read **9 of 14 as IDENTICAL**, where resolving
  them showed **18 of 51 rows name a class their own callee contradicts.**
  *Provenance: promoted from `~/tmp/laneDE3/identity.py`.*
- **`tools/retail_rtti.py`** — section-aware retail PE/RTTI resolver
  (vtable VA → COL → TypeDescriptor → class). Section-aware is the whole point:
  the `va-0x82000000` shortcut is exact for only **3 of 12 sections**
  (`.rdata`, `.pdata`, `BINKCONS`) — `.text` skews **−0xb200** and RTTI
  TypeDescriptors live in `.data` (**−0x12400**). Measured by `--sections`.
  *Provenance: `~/tmp/laneDE3/sweep.py`, `evidence.py`.*
- **`tools/gated_map_write.py`** — map writer gated with `object_pairs_hook`
  (duplicate-KEY detection) doing line-targeted textual insertion that preserves
  the load-bearing 1-space indent, **never `json.dump`**.
  *Provenance: `~/tmp/laneDE3/apply_map.py`.*
- **`tools/reachable_ceiling.py`** — the reachable-ceiling census as an
  executable artifact rather than a JSON that rots: **staleness refusal (exit 3)**,
  **collapsed-join refusal (exit 4)**, and a sabotage-tested consistency control.
  Rules 7 (shape) and 14 made runnable.
- **`tools/native_build_gate.sh`** — the instrument for the failure class the
  match build **structurally cannot witness** (link breakage). Run it before
  landing anything that adds a serialization call site.
- **`tools/gate_liveness.py --selftest`** — the reference implementation of a
  **versioned comparator migration**, and of a regression fixture that pins a
  *specific* bug. It ships **two** comparators (`--comparator symbol|positional`)
  with a printed banner naming which produced a number, so a superseded figure
  stays reproducible instead of silently changing meaning. Three synthetic COFF
  pairs are built **in memory** (no toolchain, no filesystem ⇒ can never skip),
  and the legacy comparator is asserted to **STAY WRONG** in both directions —
  false LIVE on an inert pair, false `owned=0` on a live one — so a later
  "cleanup" fails loudly. Proven able to fail under three sabotages.
  ★ Its migration also supplies the cleanest instance of a general hazard:
  **a coarse label can be robust to a defect that destroys the magnitude it is
  computed from.** `owned > 0` gave **0 label flips in 250** while **202 of 250**
  `owned` totals were wrong (median 32.7%, max 1062%) — which is precisely why
  three lanes' controls passed over it. If your instrument reports a label *and*
  a number, validate them **separately**; agreement on the label is not evidence
  about the number.

- **Known-positive RTTI controls** — a section-aware big-endian PE/RTTI decoder
  must reproduce these before its *absences* are believed: **`RndText` decodes to
  9 bases** across four COLs (0x0/0x24/0x194/0x1c0) with correct virtual-base
  PMDs, and **`RndTransformable` resolves 10 refs** (`Object@Hmx` 17). Both exist
  because the naive `va-0x82000000` shortcut — exact for only 3 of 12 sections —
  returned a **false zero** for `RndTransformable`, a class that certainly has
  RTTI (`de044702`, `369273db`). Also: RTTI adjudication reproducing DG-2's
  published vtable VAs exactly (`0x82198C74` → FIFOSampleBuffer, `0x82059CB4` →
  XboxJob), and the COL count **2,220**, matching CLAUDE.md's independent count
  and **not fitted**.

## See also

- [playbooks/levers-that-pay.md](playbooks/levers-that-pay.md) — what to work on;
  deliberately not duplicated here.
- [RULER_CHANGE_2026-08-02.md](RULER_CHANGE_2026-08-02.md) — why any `honest`
  figure before 2026-08-02 is stale by ~21.5k and Δhonest does not compose.
- `../../CLAUDE.md` — the `/GR`, ICF and obj-byte-compare cases: three
  instruments *structurally incapable* of settling the question they were aimed
  at.
