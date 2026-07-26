# RTTI-via-EH, and why BIG-FAMILY should stay unfunded (lane M, 2026-07-26)

Branch `laneM-rtti`, from `d83ca54f` (**27,629 strict**).
Sibling branches `laneM-tuloc`, `laneM-match`.

Predecessor: `docs/plans/identification-discriminators-2026-07-25.md` (lane K).

## What this lane was asked to do

Lane K closed with two open items:

1. **The last unexplored content channel.** 29 % of the identification residue is
   exception-flagged, so the word before the entry point reaches an
   `__ehhandler$` whose scope table reaches **RTTI type-descriptor strings** —
   *referenced content*, not a property of the function's own bytes, and
   therefore not killed by the structural argument that killed prolog shape.
2. **The 3,843 records refused as BIG-FAMILY.** Caller-side inversion scores
   99.09 % held-out at sibling-family ≤ 16 but only 96.65 % at 17+, so the large
   families were left on the table as the biggest reachable pool in the tree.

## Result in one line

**Channel 1 is empty — measured three independent ways.  And the BIG-FAMILY
pool is not 3,843 homings waiting to be won: the whole MULTI residue has a hard
ceiling of 1,695, because retail ICF-folded the swarms and the map is 1:1.**

The second finding is the more important one, and it is not a precision
argument. It says the pool was mis-sized by every lane that has looked at it,
including the brief for this one.

---

## Channel 1 — RTTI via EH scope tables: DEAD

Tool: **`scripts/harvest/eh_rtti_probe.py`** (committed, `--census` /
`--variation` / `--dump VA`).

### The chain decodes correctly

It is worth stating that the mechanism is real and was fully implemented, so
that the negative is a measurement and not a failure to build the thing:

* `.pdata` `RUNTIME_FUNCTION`, big-endian payload: `BeginAddress` (already a
  full VA on this image, **not** an RVA needing `+base` — the RVA reading scores
  0/57,792 inside `.text`, the VA reading scores 57,627/57,792); bitfield with
  PrologLen bits 0-7, FuncLen bits 8-29, ThirtyTwoBit bit 30, ExceptionFlag
  bit 31.
* The two big-endian words at `BeginAddress-8` are `{handler, handlerData}`.
  Confirmed empirically: **9,082 of 9,145** exception-flagged functions share the
  single handler routine `0x82829530` (the `__CxxFrameHandler3` equivalent), 59
  more share `0x82c4c01c`.
* `handlerData` → `_s_FuncInfo`, magic `0x19930522` valid for **9,082 / 9,145
  (99.3 %)**; the 9-`uint32` layout reads out sane values throughout.
* `_s_TryBlockMapEntry` (20 B) and `_s_HandlerType` (16 B) decode without
  structural error.

A worked example (`--dump 0x8229ee78`):

    handler      = 0x82829530
    handler_data = 0x82017d40
    magic        = 0x19930522
    max_state=1 n_try=0 eh_flags=1
    unwind actions: ['0x8229eec8']

### …and it reaches nothing

`--census` over the whole image:

    eh_flagged                 9145
    status/ok                  9082      (bad-magic 62, no-handler-data 1)
    ntry_0                     8008      <- no try block at all
    ntry_1                     1074
    catch-handler shapes:  adj=0x40 p_type=0   x1074
    type-name set size:    0 names: 9082 functions
    distinct name-sets:    1  (the empty set, shared by all 9082)

**88.2 % of exception-flagged functions have no try block**, and every one of
the 1,074 that does carries a single handler with `adjectives = 0x40,
pType = 0` — the compiler-synthesized `catch(...) { destroy locals; rethrow; }`
cleanup that `/EHsc` emits for object lifetime. **`pType` is non-zero in exactly
zero handler entries in the entire image**, so the `TypeDescriptor` walk never
has anything to read.

### Our side agrees, and explains why

Independent extraction from our 914 compiled objects (440,538 function symbols):

* MSVC X360 does **not** emit `__ehhandler$` / `__unwindfunclet$` symbols. It
  emits one packed `.rdata` COMDAT per function holding `__ehfuncinfo$<mangled>`,
  `__unwindtable$<mangled>`, `__tryblocktable$<mangled>`, and
  `__catchsym$<mangled>$N`. A typed catch's `dispType` is a real COFF relocation
  in that same section pointing straight at `??_R0<type>@8` (hop depth 0 in
  100 % of cases; the symbol name alone determines the RTTI string, verified
  byte-for-byte against the type descriptor's `.data`).
* **31,401** of our functions have an `__ehfuncinfo$`. **6** of them reach a
  `??_R0`. All six are `.PAD` (`char*`), from the `MILO_CATCH(const char*)` macro
  in `src/system/os/Debug.h`. They collapse to one name-set, so the signal is
  non-discriminating on our side too.
* 3,548 `__tryblocktable$` / `__catchsym$` entries exist, essentially all
  STLport allocator exception-safety rollback using `catch(...)`, which carries
  no type by design.

The retail/our asymmetry (6 typed catches vs 0) is itself consistent: retail
stripped the MILO debug machinery, so the `MILO_TRY`/`MILO_CATCH` sites compiled
away. See `project_milodbg_drift_vein.md`.

### Even the mechanism, not just the content, is constant within a hit set

The decisive test, mirroring `pdata_shape_probe.py`. `--variation` builds the
*maximal* EH signature — handler routine, `maxState`, `nTryBlocks`, `EHFlags`,
and every catch clause's `(adjectives, type name)` — and asks whether it varies
across the byte-identical candidates of a hit set:

    distinct hit tuples with >=2 candidates: 856
      MULTI/EH/VARIES                    3
      MULTI/EH/constant                103
      MULTI/noEH/constant              188
      (ALL-MAPPED and UNIQUE-ICF sets: constant in all 562)
    hit sets where variation isolates a unique candidate: 3

    per-record (MULTI):  EH/VARIES 7,  EH/constant 7709,  noEH/constant 18507
    => the EH signature varies for 7 of 26,223 MULTI records (0.027 %)

Compare prolog shape: 18 of 33,714 (0.053 %). **The EH channel is not merely
weak, it is subject to the same structural argument** — this binary's EH tables
are derived from the function's own cleanup structure, so functions with
identical bytes have identical tables. RTTI *names* would have escaped that
argument, being referenced content; there simply are none.

The 7,716 exception-flagged MULTI records (29.4 % of the residue, matching lane
K's 8,752 estimate) are therefore reached by this channel: **0**.

**Do not rebuild this channel.** `eh_rtti_probe.py` is committed so the
measurement is reproducible rather than folklore.

---

## The headroom measurement — why BIG-FAMILY is smaller than it looks

Tool: **`scripts/harvest/residue_headroom.py`** (committed).

Every identification lane has sized its opportunity in `homing_scan` *records*.
Two independent deflators apply, and nobody had measured them:

1. **A record is a (name × TU) pair, not a name.** A template instantiated in 40
   TUs contributes 40 MULTI records but is one symbol with one retail home.
2. **Retail ICF-folded the swarms.** A "family" = the set of our names sharing
   one byte-identical hit tuple. In **112 of 294** families there are strictly
   *fewer* retail addresses than our names. Worst observed: 130 names over 4
   addresses.

The map is 1:1 by construction (one VA → one mangled name), so a family absorbs
at most `min(#unhomed names, #free addresses)` new homings **however good the
discriminator is**. Summed over all families at 27,629 strict:

    MULTI records                         26,223
    distinct names                          5,853
    retail addresses in those families      9,228
    ... still unmapped                      6,670
    ... names still unhomed                 4,092
    HEADROOM                            ->  1,695     (6.5 % of the records)

    family regime:  m<n 112 | m==n 61 | m>n 121

Largest families by headroom:

    head  unhomed  freeVA  names  addrs  records  example
     328      794     328    982    577     5246  ??_G?$BloomTextures@$02@NgPostProc@@UAAPAXI@Z
     126      298     126    503    337     2204  ??$?5PAVDataArray@@@BinStreamRev@@...
      80      104      80    128    106      900  ??$Find@VAppInlineHelp@@@ObjectDir@@...
      61      166      61    186     82      888  ??_G?$StackString@$0BAA@@@UAAPAXI@Z
      50       77      50     89     60      784  ??1?$ObjRefConcrete@VBandCamShot@@...
      44      168      44    560    453     3014  ?GetContributionToken@FocusTracker@@...

### What this does to the BIG-FAMILY question

Lane K refused 3,843 names at a measured 96.65 %. Those names are drawn from
this same pool, whose total capacity is 1,695. **Most of the 3,843 have no free
address to be homed to at all.** So the argument against funding BIG-FAMILY is
not primarily the 1-in-30 error rate — it is that the pool does not contain the
prize.

There is a stronger version of the point for the `m<n` families specifically:
when retail folds *k* of our names onto one address, **the truth is not a
function**. There is no correct 1:1 answer to discover, only a choice of which
name to record. No discriminator can be "precise" about a question that has no
unique answer, and a resolver that appears confident there is measuring its own
tie-breaking rule.

This also means the honest ceiling for *any* future identification lane working
this residue is ~1,695 homings, of which only the `span_predictor.py` PAYS tier
converts. Size future proposals against that number.

---

## What actually landed: +32 from the unambiguous classes, 0 LOST

Before touching BIG-FAMILY it was worth re-harvesting what is unambiguous *by
construction*. `homing_scan`'s `UNIQUE` and `UNIQUE-ICF` classes need no
discriminator at all; they had simply not been re-swept since the map last grew.

Selection rule — all five conditions, refuse otherwise:

1. class is `UNIQUE` or `UNIQUE-ICF`;
2. the resolved VA is not already a map key;
3. the name is not already a map value (not homed elsewhere);
4. every record for that name agrees on **one** VA;
5. **that VA is claimed by exactly one name.**

Condition 5 is the one that matters, and it is the same ICF story as above:
1,331 names pass 1-4, but 1,234 of them land on 217 VAs contested by other
names. Refusing every one of those leaves **97 certain names**.

| wave | mechanism | applied | delta | lost |
|---|---|---|---|---|
| 1 | PAYS tier, map entries only | 24 | **+24** | 0 |
| 2 | UNPINNED tier — 5 splits ranges over 4 units + 6 map entries | 6 | **+6** | 0 |
| 3 | scope-filter correction (see below) | 2 | **+2** | 0 |
| | **27,629 -> 27,661** | | **+32** | **0** |

100 % conversion on every tier, consistent with `span_predictor.py`'s perfect
record across lanes.

* **WRONG-UNIT tier (126 proposals): 84 refused, 1 free.** `span_predictor`'s
  rule is that a WRONG-UNIT proposal only pays if the owning unit *also*
  compiles the symbol (scatter-include). Exactly one did. Repointing the other
  84 would cost their existing pairings, so they were refused.
* **Scope-filter correction.** Wave 1 dropped two PAYS entries by excluding the
  whole `0x828-0x82C` address band. The rule is to exclude `auto_03_*` *spans*,
  which happen to live in that band — not the band itself. Both entries belong
  to real named units (`band3/bandtrack/TrackPanel.cpp` and an STLport unit).
  Filter on the owner prefix, not the address.
* **The flywheel is now drained.** Re-scanning at 27,661 (the map grew, so
  previously-ambiguous sets can collapse) yields 65 fresh SAFE names but
  **0 PAYS** and `homing_gen4` emits **0** pins. There is nothing unambiguous
  left in this pool at this state; it will only refill when new source lands.

Trust audit across the whole lane (`multi_content_disambiguate --trust-audit`):
**385 contradicted before, 385 after — zero new**, with corroborated rising
2,268 -> 2,279 and 24 more names becoming content-checkable.

## Channel 2 — the two discriminators funded to attack BIG-FAMILY

First, a correction to the brief's sizing. Re-running `caller_side_invert.py
--validate` at 27,629 (lane K measured at 27,346):

    CALLER-SIDE precision 662/667 = 99.250 %
      1 anchor    530/534 = 99.25 %      sibling-family 1     41/41  = 100.00 %
      2 anchors    94/94  = 100.00 %     sibling-family 2-4  348/351 =  99.15 %
      3+ anchors   38/39  =  97.44 %     sibling-family 5-16 273/275 =  99.27 %
    BIG-FAMILY refused: 1,415      (lane K: 3,843)
    NO-ANCHOR:            521      SHARED-VA 21, NOT-IN-HITS 19, DISAGREE 7

**The BIG-FAMILY pool is 1,415 names, not 3,843** — the map grew and absorbed
most of it. Against a total residue headroom of 1,695, that is the real size of
the prize this lane was sent to win.

Two independent Opus streams, each in its own worktree, each measured held-out
with whole-family hold-out and contested-drop before scoring.

### TU-locality of already-homed neighbours (`laneM-tuloc`)

Tool: **`scripts/harvest/tu_locality_invert.py`** (committed).

Rationale: lane K dismissed neighbourhood evidence because "our COMDAT-per-
function objs have no matching key". That is true of *byte* fingerprints but not
of **TU membership**, which we have on both sides — retail is `/O1` with no LTCG
and no cross-TU reordering, so TU spatial grouping in `.text` is preserved. The
rule: the correct candidate is the one whose already-homed `.pdata` neighbours
belong to the same TU that compiles the symbol.

**It works, and it works best exactly where caller-side inversion fails.**
Held-out, DEV/TEST split by name, contested-drop before scoring, one record per
**name** (not per TU × name):

| channel | TEST overall | fam 1 | 2-4 | 5-16 | 17-99 | 100+ |
|---|---|---|---|---|---|---|
| **TU-locality alone** (K=4, min-same=4, `--pure`, no family cap) | **151/152 = 99.34 %** | 7/7 | 18/19 | 40/40 | **50/50** | **36/36** |
| caller-side alone, cap lifted | 385/396 = 97.22 % | 14/14 | 45/48 | 98/98 | 82/87 | 146/149 |
| strict intersection (both resolve) | **69/69 = 100 %** | 2/2 | 10/10 | 24/24 | 15/15 | 18/18 |
| caller-side + TU-loc confirmation, fam 17+ | 117/118 = 99.15 % | — | — | — | 50/51 | 67/67 |

**So the answer to the lane's second question is yes: famsize 17+ is fundable —
86/86 on TEST — by a discriminator that is *more* precise on large families than
on small ones, the exact inverse of caller-side inversion.** The two channels
never disagreed on any split, so they are genuinely independent evidence. The
confirmation-filter variant the brief hypothesised does work (lifts caller-side
97.22 % → 98.80 %) but is strictly worse than TU-locality alone.

Honesty note kept on the record: 86/86 is a point estimate. Clopper-Pearson
95 % lower bound is 96.6 % at n = 86, 98.3 % pooling DEV+TEST at 172/172.
"100 %" means "no measured error at this n", not proof.

**Reach, not precision, is the binding constraint** — as the headroom
measurement predicted. 74 production proposals = **4.4 % of the 1,695 ceiling**.
`span_predictor` tiers them 27 PAYS / 23 WRONG-UNIT / 24 UNPINNED; 9 of the 27
PAYS name-collide with an existing map entry at a different VA and were left to
the rotation machinery rather than repointed. 18 applied on `laneM-tuloc`, all
18 converted; **2 of them were independently derived by this branch's
byte-identity harvest at the same VA with the same name** — an unplanned
cross-validation of both channels.

Refusals, counted: production 45 RESOLVED / **356 TIE** / 344 RIVAL /
4,234 NO-SIGNAL / 30 DROP-CONTESTED. Funclets are dropped from the neighbour
spine by name (238 of 57,733 `.pdata` entries); unmapped entries stay in the
spine and count as "unknown", consuming window slots.

Two methodology findings worth propagating to every future identification lane:

1. **A DEV/TEST split is load-bearing.** Several grid points read 100 % on DEV
   and fell to 97.8–98.3 % on TEST. Sweeping ~150 operating points and reporting
   the best is reporting selection noise — without the split this lane would have
   claimed 100 % at reach 304.
2. **`merged.json` records are (TU × name) pairs, not names.** Scoring
   per-record roughly doubles *n* and weights precision by template instantiation
   count. `caller_side_invert.py` already dedups by name; anything new must too.
   An initial caller-side reading of 98.39 % was purely this artefact.

Re-run after any wave that homes new functions: reach is gated by how well-mapped
the unhomed neighbourhoods are, so it grows with the map.

### Global family closure by forced elimination (`laneM-match`)

Tool: **`scripts/harvest/family_closure.py`** (committed).

Rationale: every existing resolver decides one function at a time, which is why
big families fail. The family has global structure — a matching — so evidence
pinning `F1..F(n-1)` *forces* `Fn` by elimination.

#### Counting-based closure is dead, permanently

Regime census over 21,903 families with a non-empty hit set:

| famsize | m==n | m>n | m<n |
|---|---|---|---|
| 1 | 8119 | 148 | 0 |
| 2-4 | 297 | 60 | 642 |
| 5-16 | 23 | 30 | 208 |
| 17-99 | 1 | 3 | 93 |
| 100+ | 0 | 0 | **12** |

At famsize 17+, **105 of 109 families are `m<n`**. Rule "naked single by
counting", gated to `m>=n`, therefore reaches **exactly zero** on the pool it was
built for. Forced across the gate it measures 95.59 %.

The deeper reason is the one this lane keeps arriving at: where retail ICF-folds
*k* of our names onto one address, **the truth is not a 1:1 matching**. There is
no correct answer to discover, only a choice of which name to record, and a
resolver that looks confident there is measuring its own tie-breaking rule.
That closes counting-closure permanently rather than contingently.

#### What works instead: span-local uniqueness

Everything inside unit `U`'s pinned `.text` range is `U`'s linked contribution.
If `U`'s span holds exactly **one** still-free family-bodied VA, and our objects
for `U` emit exactly **one** still-free family member, they are the same
function. This is a derivation from *the linker's own partitioning*, not a
count — and it is **robust to folding**: a member whose copy was folded away has
no family VA in its own span, so it is refused with an empty domain rather than
mis-assigned. Folding costs reach, never correctness, which is exactly why it
survives the `m<n` regime that kills counting.

Held-out with a **whole-family hold-out** — hide every map entry naming any
member *and* every entry sitting on any VA of the family's hit set. Leave-one-out
is worthless here: a fully-mapped family leaves exactly one free VA and
elimination "recovers" the held-out name for free, measuring nothing. Production
is strictly better informed, so these are lower bounds:

| famsize | HIT/total | precision |
|---|---|---|
| 1 | 6109/6112 | 99.95 % |
| 2-4 | 410/411 | 99.76 % |
| 5-16 | 225/228 | 98.68 % |
| 17-99 | 149/150 | 99.33 % |
| 100+ | 106/107 | 99.07 % |
| **17+** | **255/257** | **99.22 %** |
| overall | 6999/7008 | 99.87 % |

**The knob that buys the precision is emitter count, not family size.** At
famsize 17+: 99.47 % (1 emitter) / 97.73 % (2-4) / 94.74 % (5-20) / **73.91 %
(21+)**. A single-emitter COMDAT gave the linker no choice of contributing
object; a 30-TU COMDAT could have been linked from any of them, and our per-TU
instantiation set diverges from retail's. Operating point:
`--rule1 --max-emitters 3`.

All 32 of the famsize-17+ picks land in `m<n` families (976, 560, 501, 270, 183,
128, 116 …) — precisely lane K's refused pool.

Refusals, counted: OPEN-2 **3,515** (≥2 feasible VAs — refused, never argmax'd),
OPEN-1 599, R2-UNIT-AMBIGUOUS 534, OPEN-0 533, R1-BLOCKED-REGIME 220,
R2-UNIT-EMPTY 133, R2-SPAN-INCOMPLETE 28, MULTI-EMITTER-FILTERED 25,
CONTENT-CONFLICT 4, contested drops 0.

A first attempt without a map-free content veto introduced exactly **one** new
contradiction (`?StaticClassName@StarDisplay@@`). The veto was added, the wave
reverted, re-derived and re-applied; that class is now refused. Recorded because
it is the only time in this lane that the honesty gate actually caught something.

Reach: 104 forced = **6.1 %** of the 1,695 headroom, 48 applied = 2.8 %.
56 UNPINNED picks at the same measured precision remain, needing splits pins.

### Both channels integrated

| stream | applied here | delta | lost |
|---|---|---|---|
| unambiguous-homing harvest (this branch) | 32 | +32 | 0 |
| TU-locality (`laneM-tuloc`) | 16 of 18 | +16 | 0 |
| family closure (`laneM-match`) | 37 of 48 | +37 | 0 |
| | | **+85** | **0** |

**27,629 -> 27,714**, re-verified on a cache-cleared full build.

The overlaps are the most reassuring number in the lane: of the entries the two
sibling streams proposed, **2 (TU-locality) and 11 (family closure) had already
been derived independently by another channel at the same VA with the same
name, and none of the three channels ever contradicted another.**

---

---

## The BIG-FAMILY pool was never 3,843 — a masking bug in the census

`caller_side_invert.py`'s `resolve()` tested the sibling-family cap **before**
the anchor lookup, so the `BIG-FAMILY` verdict swallowed every other refusal
reason — above all `NO-ANCHOR`. Fixed here: the cap is now tested **last**, so a
record is only reported as "refused for being in a big family" if it *would
otherwise have resolved*.

Corrected census on this lane's fixed-point scan (6,934 targets, at 27,781):

    max-family 16   NO-ANCHOR 4077  ALREADY-HOMED 2075  SHARED-VA 331
                    NOT-IN-HITS 191  BIG-FAMILY  174  RESOLVED  57  DISAGREE 29
    max-family 0    NO-ANCHOR 4077  ALREADY-HOMED 2075  SHARED-VA 331
                    RESOLVED  231    NOT-IN-HITS 191                 DISAGREE 29

**The genuinely capped pool is 174 records** — not 1,415, not 3,843. Lifting the
cap moves those 174 into RESOLVED (57 → 231).

This re-prices the entire channel, and it matches what both of this lane's
discriminators independently found: **the binding constraint is anchor coverage,
not the precision of any test.** Anchors are functions that are already
strict-matched, so this pool grows as a *consequence* of body-port work. Both
TU-locality (86/86 on famsize 17+) and family closure (99.22 % on 17+) cleared
the precision gate comfortably and were then limited to 4.4 % and 6.1 % of the
1,695 headroom respectively. **Do not build a third discriminator for this pool.**

## The string-operand channel: rigorous negative, and 48 fake matches disclosed

Branch `laneM-str` (commit `1f8cc191`), on top of main `fb55bbe7` (27,839).

Applying the `a380ed69` string-operand discriminator to the residue:

* **Precision is excellent.** Leave-one-tree-out, one record per **name** (418
  distinct strings, not 453 VAs), DEV/TEST by `md5(string)%2`, guard chosen on
  DEV only: **TEST 452/452 = 1.0000** with the token-relation guard (DEV 0.9951);
  ungated it is 578/582 = 0.9931. Coverage cost 21.3 %. Every observed failure is
  a subclass twin sharing the classname token.
* **Reach is small.** The census reconciles to exactly 294 families, of which
  **22 are reachable** by a distinguishing string operand — about **150 of the
  1,695 ceiling (8.8 %)**. 1,688 of the headroom sits in no-string families.
  `??_G` swarms are **definitively unreachable, measured not assumed**: 21
  families, 7,314 names over 1,306 addresses, and the largest
  (`??_G?$ObjPtrVec@…`, 577 addresses) contains **no `lis` instruction at all**.
* **And the yield is zero, for an instructive reason.** The full string-certain
  wave (28 repairs + 38 evictions) measured **−57 strict, 0 gained**, and was
  reverted. For every provably-wrong entry, the string-correct class's symbol is
  *not compiled in the unit that owns the VA*: retail scattered class C's COMDAT
  into the span pinned to unit U while we compile class D there. **That is a
  splits/COMDAT-scatter problem no map entry can fix.** **48 of the 27,839 are
  fake matches, and each honest correction costs exactly the fake match it
  removes.** Disclosed rather than applied.
* Two methodology corrections to `a380ed69` fell out of this, each proven by
  measurement: `declared()` returned the first tree's token and stopped, so a
  class declaring different tokens per tree (`MCResultMsg`: `mc_result` in
  `src/`, `memcard_result` in dc3) read as a false MISMATCH (**UNION**, fixes 8);
  and a derived/platform class registers under its *base* classname
  (`FxSendReverb360` → "FxSendReverb"), so the token is a strict substring
  (**DERIVED**). Applying the original method unmodified would have **destroyed a
  correct 11-entry `FxSend*360` block**.
* Structural byproduct: the `.text` order and `.data` static-slot order of the
  453 members are the *identical* permutation (0 descents over 452 pairs), so VA
  runs are TU COMDAT runs. A directory-run interpolation channel on that scores
  TEST 0.950 — corroborating only, refused as too weak to fund alone.

## Evidence-class census of all 294 families (independent second measurement)

A separate read-only agent classified every family by what its members actually
reference at masked relocation slots. It agrees with the string lane's reach
conclusion by a different route, and it prices the *rest* of the residue:

| evidence class | families | distinct names | distinct addrs |
|---|---|---|---|
| CALLEE-ONLY | 172 | 3,658 | 2,372 |
| DATA-ONLY | 80 | 1,334 | **5,024** |
| STRING | 39 | 836 | 1,299 |
| FLOATCONST | 3 | 25 | 398 |
| NONE | 0 | 0 | 0 |

Of the 39 STRING families, only **17 (782 names / 1,126 addrs)** decode to a
literal that actually *varies* across candidates; the other 22 carry a fixed
literal (`"types"`, `"objects"`, `"vector"`) with zero discriminating power. So
the string channel tops out at **~13 % of residue names** — consistent with the
string lane's **8.8 % of the 1,695 headroom**, which is the tighter bound
because most of those names have no free address to be homed to.

Two corroborations worth recording:

* The `StaticClassName`/`Type` swarm is **exactly one family, famsize 560 /
  453 hits** — the 453 matches `a380ed69`'s ground truth precisely. Even this
  best case is not clean by string alone: 418 of 453 candidates decode to a
  distinct class-name string, **35 still collide**.
* The `??_G` scalar-deleting-destructor population is **not one family but ≥20**
  (15 CALLEE-ONLY + 5 DATA-ONLY), including the single largest family in the
  residue (982 names / 577 addrs). They reference the class dtor and
  `operator delete`, sometimes the class vtable — **never a string literal**.
  Two independent methods now agree they are unreachable by strings.

**Correction to `residue_headroom.py`'s own output.** Its "retail addresses
9,228" is a sum over families *with multiplicity*; families share addresses, so
the distinct count is **8,555**. The 1,695 headroom is therefore an **upper
bound** and slightly loose for the same reason. It remains a ceiling, which is
how it was used, but it should not be quoted as an exact reachable count.

**Unfunded forward lead, stated with its caveat.** DATA-ONLY is 22.8 % of names
but **58.7 % of addresses** — these bodies reference their class's own vtable,
and vtable identity *is* content with a counterpart on our side
(`??_7Class@@6B@`; `scripts/harvest/vtable_1anchor.py` / `vtable_global.py`
already exist). But the vtable reference sits at a **masked** slot, so it only
becomes evidence once the retail vtable is itself identified — i.e. it needs
bootstrapping off the corroborated map, exactly like caller-side inversion, and
would therefore be **anchor-limited in the same way every channel in this lane
turned out to be**. It is recorded as a lead, not as a measured result, and it
was deliberately not built.

## Honesty accounting for this lane

* **151 entries applied, zero net new contradictions.** The closing
  `--trust-audit` read 386 against a 385 baseline. The +1 was traced rather than
  netted away — `?GetPlayerContributionString@Tracker@@…` at `0x826d0b80`, from
  the final TU-locality wave — and **evicted**, costing exactly the one fake
  match it removed and returning the audit to 385.
* **A/B verified unit-agnostically as well as pair-wise.** objdiff pairs
  anonymous functions *positionally*, so a wrong splits pin manufactures 100 %
  matches and a re-homed `fn_` address reads as a `(unit, function)` loss without
  being a real regression. The function-name multiset also shows **0 lost, 151
  gained**.
* Ties were refused and counted at every stage, not broken. A tie-break on
  byte-identical candidates buys a *guaranteed fake* +1.

## Final ledger

| wave | mechanism | applied | delta | lost |
|---|---|---|---|---|
| 1 | unambiguous-homing harvest, PAYS tier | 24 | +24 | 0 |
| 2 | UNPINNED tier, 5 splits ranges | 6 | +6 | 0 |
| 3 | scope-filter correction | 2 | +2 | 0 |
| 4 | TU-locality (`laneM-tuloc`) | 16 | +16 | 0 |
| 5 | family closure (`laneM-match`) | 37 | +37 | 0 |
| 6 | family closure, UNPINNED tier, 56 pins | 56 | +55 | 0 |
| 7 | TU-locality at the fixed point, 12 pins | 12 | +12 | 0 |
| 8 | eviction of the one contradicted entry | −1 | −1 | 0 |
| | **27,629 -> 27,780** | | **+151** | **0** |

## Reproducing

```bash
scripts/setup_worktree.sh ~/tmp/wt-laneM-rtti laneM-rtti && cd ~/tmp/wt-laneM-rtti
./tools/ninja-locked
rm -f build/45410914/report.cache && ./tools/ninja-locked   # baseline must read 27,629

# whole-tree reloc-masked byte-identity scan (~36 s at 16-way)
scripts/harvest/homing_scan_all.sh $PWD ~/tmp/laneM/scan 16 20

# channel 1: decode the EH chain and measure it
python3 scripts/harvest/eh_rtti_probe.py --census
python3 scripts/harvest/eh_rtti_probe.py --variation --results ~/tmp/laneM/scan/merged.json
python3 scripts/harvest/eh_rtti_probe.py --dump 0x8229ee78

# the ceiling every future lane should size against
python3 scripts/harvest/residue_headroom.py --results ~/tmp/laneM/scan/merged.json
```
