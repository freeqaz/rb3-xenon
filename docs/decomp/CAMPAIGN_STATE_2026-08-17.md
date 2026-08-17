# Campaign state — 2026-08-17 (third edition)

> **STATUS (2026-08-17): CURRENT — this edition replaces
> `CAMPAIGN_STATE_2026-08-14.md` wholesale** (the precedent CONSOLIDATE-1 set).
> Every byte absolute below is on the **shipped `name_check` ruler** unless
> tagged `@none`. Written by the coordinator from two Opus analysis lanes
> (GAPMAP-1: gap decomposition; DOCAUDIT-1: history + docs audit), both run
> 2026-08-17 with **all decomp lanes paused**.

## 0. SESSION CLOSE

### 0.7 ROUND EIGHT — W20/W21/W22 ← **LATEST**

**44,503 fns / 3,756,568 B = 36.398511%**, honest **21,593**. **Day total
+59 fns / +32,864 B**; **59.55% of the 61.121% reachable ceiling.** All three
exact.

| lane | predicted | measured | |
|---|---:|---:|---|
| W22-FRAME | +11 / +5,296 | **+11 / +5,296** | ✓ *largest of the day* |
| W21-CARVE | +4 / +1,008 | **+4 / +1,008** | ✓ cascade **exactly 0** |
| W20-CASCADE | 0 / 0 | **0 / 0** | ✓ *by design — priced, then declined* |

★★★★★ **THE CALLER CASCADE IS ONLY 19.5% OF A MAP EDIT'S DELTA. THE DOMINANT
TERM IS UN-PAIRING (80.5%), AND IT HAD NO NAME.** W20's round trip
pre-registered −2,180 and measured −2,976: **−580 B / 4 rows** was the cascade
(**predicted exactly**), **−2,396 B / 10 rows** were rows that **VANISHED FROM
THE REPORT** because the pinned obj cannot define the spelling — **W9's −180 B
mode at scale**, which we had filed as a rare edge case. ⇒ We spent three lanes
diagnosing the minority term.
⛔⛔ **"A pure rename moves `none` by 0" is FALSE when the edit changes
PAIRING** — W20 missed `none` by −2,520 while still calling its edit a pure
rename, and **`ab_measure`'s own control had already said `REAL_PAIRING`.**
★ The pricer had **5 rows right BY THE WRONG MECHANISM**; they are now excluded
from the point estimate and pairing is reported as an explicit **bound**.
*A right number from a wrong mechanism is a coincidence waiting to mislead.*

★★★★ **CALLER-SPELLING DISPERSION ⇒ CASCADE STRUCTURALLY ZERO** — the new
discriminator between a repairable wrong name and an arbitrary ICF survivor. It
killed **both** open leads: `0x822dea78` (7 distinct our-side spellings, +0 vs
80 B blocked — **W17's refusal quantified**) and `0x8233bea0` (+0, its only
caller itself unpaired). ★ W21 then landed that carve for **+1,008 B with
cascade measured at exactly 0** — **two lanes, two instruments, same answer.**

★★★★ **A KNOWABLE NAME CAN BE METRIC-NEGATIVE, AND PRICEABLE.** W21 **refused**
to name `0x8233cb58`: three callers sit at `fuzzy` 100 **today** (1,212 B)
*precisely because* the placeholder is forgiven, and our `PerfectSectionTracker`
legitimately spells `<TrackType,bool>`. ⇒ **not "we don't know the name" but
"naming it costs 620 B of CORRECT code, because the grader cannot represent a
fold."**
★★★ **BLOCK the cascade structurally rather than estimate it** — W21 is the
first of four consecutive lanes to get it right, and did so by arranging the
edit so there was nothing to estimate. ⚠ The brief undercounted the chain:
**FOUR members, not three** — a three-way carve would have **stranded a member**.

★★★★ **W19's IN-SOURCE MECHANISM NOTE WAS WRONG, AND IT MATTERED.** It blamed
`/Ob2` deleting the temp and prescribed `auto_inline(off)`; measured, **with**
the pragma the temp *is* constructed and the call *is* out-of-line on both
sides. The real cause is **escape / memory-effect analysis** — an empty body let
MSVC kill a dead vptr store, forward `r27`, and **overlay the temp onto shared
scratch** instead of giving it 8 private bytes. Retail keeps **four** 8-byte
message temps; we kept three. That is the 0x10. ★ W22 **proved** W19's
load-bearing assumption instead of inheriting it: **all 212 `r28`↔`r29` charges
dissolve with the frame** — `REGISTER_SWAP`-is-a-symptom holding at 212 sites.

⛔⛔ **A FIFTH BUILD-PROBE VACUITY: `ninja <one .obj>` IS NOT A MEASUREMENT.**
The **six post-compile obj patchers never run**, so a one-obj rebuild plus a
hand-run objdiff **MANUFACTURED A PHANTOM 396 B REGRESSION** on a row that is
**98/98 instructions equal** and loses only to 8 B of COMDAT alignment padding.
**The patchers are part of the ruler.** W22's whole prediction miss was that
phantom — in the direction that would have made it revert real work.
★ Sibling: **a probe can buy the frame and MISS THE SCHEDULE** (the probe
reached `-0xf0` but left an 8-instruction residual; the **faithful** body scored
100.000) ⇒ **do not read "permuter-class" off a probe.**

⛔ **`Poll@VocalPlayer`'s 3,388 B is UNREACHABLE REGARDLESS OF BODY QUALITY** —
8 of its 10 name charges are ICF fold-survivor names **where our source is
CORRECT**. It needs *proven aliases*, which **gates** the work rather than
enabling it. And *"Poll has a missing local at `0x78`"* is **refuted in-source**:
shared scratch, retail's 5 extra stores all overwritten before any read — **dead
stores retail failed to eliminate, the same class W19 refuted on
`Poll@VocalTrack`, now found twice.**

⚠ **`arity_screen.py`'s parser failure was TOTAL, not partial** — over 30,059
map names the fixed parser demangles **27,931** vs the old **1,063**, and **all
1,063 of the old parser's non-`None` answers were CORRUPT.** Fixture requires the
legacy parser to **fail**, exiting 2 as `VACUOUS` if it ever passes.
⚠ **Grep `symbol_aliases.json` BEFORE believing a relocation-name find** — W21
"discovered" `0x827bd208`, counted 403 call sites, and it had been solved since
2026-08-13 with the identical argument.
★ **The untested-by-construction class is cut 34 → 4** (`rbtree_body_anchor.py`,
selftest 156/202 sabotages flagged; the surviving 22.8% are **ICF twins, reported
as the blind spot**). ⚠ It is **34, not W17's 35** — *the map moved under W17's
own renames.*

### 0.6 ROUND SEVEN — W17/W18/W19

**44,488 fns / 3,750,264 B = 36.337429%**, honest **21,588**, units at 100%
**254 → 255**. **Day total +44 fns / +26,560 B**; **59.45% of the 61.121%
reachable ceiling.** All three exact.

| lane | predicted | measured | |
|---|---:|---:|---|
| W19-VOCAL | 0 / +732 | **0 / +732** | ✓ (4 of 4 legs) |
| W18-SYMPAIR | 0 / +148 | **0 / +148** | ✓ |
| W17-FAMILYSWEEP | +3 / +1,652 | **+3 / +1,652** | ✓ *(headline; components missed — see below)* |

★★★★★ **THE CHARGED-SITE CENSUS WAS COUNTING REGISTER DIFFS AS NAME CHARGES.**
objdiff's `reloc_eq` **forgives placeholder target names**
(`fn_`/`lbl_`/`jumptable_`/…), so a site whose only differing argument is a
placeholder **never reaches `diff_arg`**. ⇒ **an `arg:{Register,Symbol}` diff is
charged BY THE REGISTER; only a BARE `arg:{Symbol}` is a real name charge.**
Naive counting reads **138** name charges on `Handle@VocalPlayer` and files it
**fold-alias-walled and unreachable by source work** — **the true count is
ZERO.** That is the difference between a closed vein and an open row, produced
by an instrument nobody had questioned. Tooled as `tools/w19_charge_census.py`.

★★★ **THE CALLER-CASCADE UNDERESTIMATE IS NOW THREE-FOR-THREE** (W8, W9, W17),
always in the same direction. W17 makes the mechanism unmistakable: graded
predicted **+1,072** / measured **+1,652**, while the **`none` control predicted
+792 and measured +792 EXACTLY** — localising the whole **+580** to four rows
**not in the patch**. ⇒ *"remember to include call sites"* **has failed as a
rule**; the cascade must be **enumerated and priced as its own line item**
(dispatched as W20). Attribution was **scored, not asserted**: row-level net
== headline.
★★★★ **And the cascade is the DISCRIMINATOR, not just yield** — W17 nearly
re-homed a family off an **ICF survivor**, whose name is *arbitrary* (W7's
fixed-point problem). The cascade refuted it, and the metric then **confirmed**
the true assignment through `SupportChar@RndText`, a row the lane never touched.

⛔ **THE TWO UNEXAMINED SYMPAIR CLASSES ARE 93% UNREACHABLE.** 232,604 B →
**792 B of source work (0.34%)**, and W18 hand-checked the survivors and reports
the true figure is **below** that. Breakdown: **PROVEN FOLD 59.96%** · **callee
ABSENT FROM RETAIL 27.01%** · no compiled body 7.28% · unnamed/ambiguous 3.87% ·
row-name refuted 1.40%. A 232 kB slice bought **148 B** —
[[ceiling-raising-is-futile]] at ~100× the ratio.
⛔ **`TEMPLATE_ARGS_DIFFER` is what a PROVEN FOLD LOOKS LIKE** (65.5% of applied
aliases, 55.3% of the in-queue control) ⇒ the shape covering ~48% of
`MIXED/UNKNOWN` is **the least informative feature available, not a lead** — and
**only a control showed that.**
★ The actionable class shrank **3,308 → 1,484 → 1,016 → 792 B** as the
classifier got more careful. **Stopping at the first number would have briefed a
4× overstatement.**

★★★★ **THE REFERENCE YOU BIND IS ITSELF CODEGEN.** W19 proved *without the
oracle* that MSVC emits commutative `a + b` as `fadds fD, fb, fa` — **reversed**
(`UpdateLyricZ`'s two identical halves get *different* operand orders in retail).
But **getting the arithmetic right was NOT sufficient**: the obvious
`Transform &xfm = …` form flipped the `fadds` correctly and **still scored
worse** (99.945 → **99.399**), costing an `addi` and flipping an untouched half.
A **`float&`** gives **100.000**.

⛔ **Three spellings refuted and documented IN-SOURCE so they are not retried:**
the FP operand reversal **does not generalise to integer indexed loads**
(`lwzx` is canonicalised independently of source order — and BF-3's earlier test
was **vacuous**, it *is* `operator[]`); an explicit `!` on an already-`bool`
makes MSVC negate **arithmetically** (**97.617 → 95.195**; our code is one
instruction *shorter* than retail's — retail failing to coalesce); and a virtual
`clip->SetVolume(...)` measured **worse** where the qualified
`clip->MoggClip::SetVolume(...)` is exact, and was **already the idiom four
functions above in the same TU**.

⚠ **Two live instrument defects recorded, one still UNFIXED:**
`tools/sympair_queue.py` classifies a pure symbol pair iff the detail string
**starts with `SYM`**, i.e. **by argument order** (blast radius measured: 1 row /
1,652 B wrongly called crossable); and **`llvm-undname` emits blank-line
records where a REJECTED name yields a ONE-LINE record**, desyncing the natural
parser and producing a confident *"100% UNDEMANGLABLE"* on **1,624 names that
demangle fine** — ⚠ **that bug is still live in `tools/arity_screen.py:810`**
(handed to W20).

⛔ **W17's sweep is NOT a clearance and says so**: **35 of 86 components reach no
builder and no eraser** (`_M_find`, `swap`, `begin`, bare `_M_erase`) and are
**untested BY CONSTRUCTION** — a clean sweep clears **51, not 86**. Handed to
W21 with the `map<int,bool>` chain, which W17 raised from *existence-proved* to
**assignment-proved** but deliberately did not ship (it needs a three-way carve).

★ **Two corrections to lanes that landed the same day**: W12's `CharInfo`
hand-back is confirmed but its `src/**` premise was wrong (`CharInfo` was
**already 16 B**, so the header fix handed onward was never needed); and W15's
*"sole caller of `0x8233c668` is `map<int,int>::operator[]`"* is **wrong** — its
conclusion survives, its stated evidence did not.

### 0.5 ROUNDS FIVE–SIX — W11b/W12/W13/W14/W15/W16

**44,485 fns / 3,747,732 B = 36.312896%**, honest **21,585**,
`total_functions` **69,226**. **Day total +41 fns / +24,028 B**;
**59.41% of the 61.121% reachable ceiling.**

| lane | predicted | measured | |
|---|---:|---:|---|
| W11b-CUSTOMIZE | +1 / +132 | **+1 / +132** | ✓ |
| W12-MAPLEADS | +2 / +632 | **+2 / +632** | ✓ |
| W14-FAMILY | +3 / +1,540 | **+3 / +1,540** | ✓ |
| W13-CHARINFO | −1 / −92 | **−1 / −92** | ✓ *(deliberate)* |
| W15-UNLOCK | 0 / +1,156 | **0 / +1,156** | ✓ |
| W16-HEADERTRUTH | 0 / 0 | **0 / 0** | ✓ |

★★★★★ **THE FAMILY DEFECT IS THE STRONGEST LEVER FOUND — FOUR INSTANCES IN
ONE DAY.** A wrong `_Rb_tree` name is invisible to `name_check` when the
whole family carries it consistently: intra-family relocation names agree,
members sit at `fuzzy` 100, and only the boundary edge is charged.
**Flagged row alone = −696 B; the family = +632 B.** The screen flags a
member; the fix is the component.

★★★★ **W15: THE BLOCKER W14 REFUSED TO GUESS WAS A PHANTOM.** `0x8235c610`
is not a function — it is the interior (+8) of the 104-byte leaf at
`0x8235c608`, which the map **already named correctly**. dtk carved one
function into an 8 B head and a 96 B tail and the tail took the name. Every
symptom follows (nothing can call mid-function; a mid-function slice scores
~0). **The row was DELETED, not renamed.** Independent corroboration nobody
solicited: `total_functions` fell **69,227 → 69,226**, and alias group 386
had already recorded *"different body SIZE (8 vs 104)"*.
⇒ **When an identification is "missing", first ask whether the thing needing
a name EXISTS.**

⛔⛔ **W16 REFUTED THE COORDINATOR'S PREMISE MECHANICALLY: THERE IS NO
".rdata SWITCH JUMP TABLE READ AS A VTABLE" FAILURE MODE ON THIS TARGET.**
MSVC X360 emits switch tables as compact **byte-offset** tables containing
no code addresses — 208 harvested via `bctr` dispatch sites, **0** with
pointer shape. The confusable structure is the **MSVC C++ EH IP-to-state
map** (`{void *pc; int state;}[]`), whose `pc` fields are interior to one
function. Both of W13's falses are that.
★ **6 of 7 corrections are REPAIRS, NOT WITHDRAWALS** — the substance was
right and the address wrong. `UIComponent.h`'s "20 slots" is **TRUE**
(`0x82122c84`, 20 slots / 0x50 B); W13's withdrawal was *more cautious than
the evidence required* **and left the false address still asserted on line
69**. `ContentMgr.h` had **inherited** `SongMgr.h`'s false address —
header-to-header propagation is the disease.
⇒ **An over-cautious correction is still a defect, and a lane auditing the
lane before it is worth funding.**

★★ **THREE BRIEFED FIGURES OF MINE WERE WRONG AND EACH WAS REFUTED**: the
switch-table premise; a "9-item" interior queue that measures **174**
(0/174 are `bl` targets vs a 36.3% control that fires); and a
"97-instance `SyncProperty` lever" that is a **historical landed result**,
not a live claim — its check runs and returns `DIFF=0` over **4 adjudicable
rows**, so it is not a lever, and saying so is the finding.

⛔ **HEADER COMMENTS REMAIN A SYSTEMIC HAZARD, RATE UNQUOTABLE.** 7 of 17
live claims false = 41.2%, Wilson CI **[21.6%, 64.0%]** — consistent with
W13's 3/10 and **still far too wide to quote as a point rate**. Both lanes
refused to publish a point estimate; that restraint is the correct output.
★ **W13's headline: the header's cited "retail" `li r3,0x28` was OUR OWN
CODEGEN** — exactly what our wrong 20-byte `CharInfo` compiles to. The
comment reasoned from the output it existed to justify. `CharInfo` is
**16 B**, compiler-confirmed.

⚠ **Two coordinator process failures worth keeping:** I relayed W12's cited
address `0x824730e8`, which is wrong (it is `RndFont3d`'s
`map<u16,CharInfo*>` in `Font3d.s`) — **W12's conclusion was right and its
evidence was not**; and I briefed W13 a **stale baseline**, which it
re-measured as instructed. Also: a merge-message backtick was
shell-evaluated and silently ate a word — **write merge messages to a file,
never inline.**

### 0.4 ROUND FOUR — W9-FALSECREDIT + W10-BODYPORT

**44,480 fns / 3,744,364 B = 36.280262%**, honest **21,580**, 254 units at 100%.
**Day total: +36 fns / +20,660 B across FOURTEEN lanes, every one composing to
the byte.** `main` was reconciled into this branch and measured **exactly
metric-neutral** (+0/+0).

★★★★★ **A WRONG MAP NAME IS NOT A FREE +N — IT IS A LOCAL +N FINANCED BY A
CHARGE ON EVERY CALLER THAT RELOCATES AGAINST IT.** W9 was briefed to expect
net-NEGATIVE bytes and measured **+320**. The touched rows netted **exactly
zero** (232 B of false credit out of BandList, the identical 232 B of honest
credit into Accomplishment); **all +268 B came from three callers the lane never
touched.** ⇒ **"Accuracy work costs bytes" is TOO CRUDE** — true for
*withdrawing an unproven alias*, **false for repairing a wrong name**.

★★★★ **THE CASCADE IS EVIDENCE, NOT JUST YIELD.** Several signed-4-byte `set`
instantiations were candidates; the +268 B arrives **only** under
`set<ScoreType>` — pick `set<int>` or `set<TrackType>` and those three untouched
rows **do not cross**. **A caller cascade discriminates between rival
assignments.**

⛔⛔ **PROVING A NAME IS WRONG DOES NOT MAKE RENAMING IT SAFE.** Four ByteCode
rows with equally proven identity were **refused**: the **pinned unit's obj
cannot define the replacement**, so renaming trades a false credit for a
**permanently 0% row** (−180 B / −3 fns). One had been briefed as *"costs
literally nothing."* **Check the base obj can define the name first.**

★★★★ **AN ALIAS'S EVIDENCE CAN BE TRUE WHILE ITS INFERENCE IS FALSE.** "Retail
at A is byte-identical to our symbol B" is equally consistent with **folded** and
with **"A IS B and the map is wrong"**; `icf_alias_build.py` cannot separate
them. Here the second held, and **the forgiveness measured WORSE than the fact —
99.23 aliased vs 100.0 corrected.**

⚠ **PRICED FROM THE PATCH TWICE IN ONE DAY** (W9 ~0→+268, W8 +24→+184) ⇒ **price
from the charged-site list INCLUDING CALL SITES.**

★★★ **THE GAME LAYER'S *PURE* SOURCE SURFACE IS ~3,656 B — 15× SMALLER THAN THE
RAW DIVERGENCE FIGURE** (W10). Named `mpn<100` rows in `src/band3/` +
`src/network/` are 237 rows / 137,332 B, but split by whether every charge is
instruction-level: **PURE 34 rows / 8,692 B** vs **MIXED 203 rows / 128,640 B**,
and **58% of the pure mass is the single already-drained CustomizePanel row.**
Independent corroboration that **the gap is mostly not source work.**

★★ **A DROPPED STATEMENT LEAVES A HOLE IN THE `MILO_ASSERT` LINE NUMBERING** even
when the source text matches the oracle byte-for-byte. W10 crossed
`NetSession::AddLocalUser` by spotting `0x2F9, 0x2FA, ⟨gap⟩, 0x2FC` and restoring
the assert at `0x2FB` — `MILO_ASSERT` is `((void)(cond))` here, so it still
**evaluates**, reproducing retail's discarded call. Reusable oracle.

★★ **ONE MISPLACED DECLARATION CAUSED TWO DIVERGENCES.** `UpdateOnlineID` was
declared on the virtual base `User` instead of `LocalUser` — a header comment
claimed this "matches rb3-Wii" and **it was simply false**. Retail agrees
independently: it calls with `mr r3, r30` (raw pointer) where a vbase member
needs the 4-instruction adjust — and that adjust also **proved the pointer
non-null**, making MSVC elide a null check retail keeps. *(Second header comment
today asserting an untrue fact; cf. the `Rnd.h` vtable claim W7 retracted.)*

⛔ **`run_objdiff`'s OFFSET RESOLVER LABELS r31 FRAME OFFSETS AS STRUCT MEMBERS**,
complete with "wrong field?" hints, while every *object* access already matched —
W10 nearly rewrote a header for nothing. **The documented r31 coin-flip, live.**
Adjudicate layout with `scripts/harvest/class_layout_report.py` (which asks the
**compiler**), never the `// 0xHEX` comments or the resolver.

★ **`CustomizePanel::Handle` RE-PRICED: RESIDUAL-2's "buys zero bytes" IS STALE.**
(a) is closed and both aliases landed ⇒ **5,036 B behind exactly one
instruction**, the best size-per-fix in the game layer. Lane W11 re-opened it on
that changed price.

⚠ **THE ORACLE'S LITERAL FORM CAN BE WORSE**: on `NetSession::Poll` the oracle's
nested-`if` scored **87.1%** where our own `&&`/`||` spelling was already better —
a direct counter-example to W8's "restore it verbatim" result. **Both directions
are real; neither is a default. Let the bytes decide.**

### 0.3 ROUND THREE — W7-SYMPAIR + W8-TWINPORT

**+2 fns / +328 B** (all of it W8; W7 measured **Δ exactly 0** and landed anyway).
Twelve consecutive lanes have now composed to the byte.

★★★★★ **THE SYMPAIR QUEUE IS LARGE AND MOSTLY UNREACHABLE — AND THE TRIAGE IS
THE DELIVERABLE.** `tools/sympair_queue.py` (promoted from W2-ENGINE's scratch,
which would otherwise have died in `~/tmp`) finds every row whose **only**
charges are relocation-symbol pairs, i.e. realizable by naming alone. Whole
binary: **2,340 rows / 536,528 B**. But:

| class | rows | bytes | share |
|---|---:|---:|---:|
| `FOLD_FANIN` — ≥2 of our fns hit ONE target address | 1,099 | 285,548 | **53.22% IRREDUCIBLE** |
| `ALL_OURS_UNMAPPED` — callee has no retail address | 724 | 158,300 | 29.50% |
| `MIXED/UNKNOWN` | 415 | 76,060 | 14.18% |
| `ALL_RECIPROCAL` — transposed map bijection | 102 | 16,620 | **3.10% fixable** |

⇒ **Only ~3% carries a fixable-naming signature.** The big prizes are folds:
`BandUI::Handle` (3,564 B) calls **one** address where our source calls three
distinct `OnMsg` overloads. **A raw "536 kB crossable" figure would have been
badly misleading** — do not quote it without the triage.
★ Self-validates against a **frozen fixture** (23 rows / 41,088 B, W2's figure,
bit-for-bit) and the gate is **proven able to fail** (`--selftest --mutate` →
exit 1). The fixture is frozen because W2's worktree was already removed and it
is **not regenerable**.

★★★★ **A FOLD CANNOT TRANSPOSE ACROSS TWO DISTINCT ADDRESSES** — a fold maps N
names onto ONE address. So reciprocal pairs are **provably not folds**: 54 found.
⛔ **The map's own `_bijection_arbitrary` list UNDER-COUNTS** — only **11 of 54**
sit on a flagged address, and only 8.03% of queue bytes touch the flagged
population. **43 transpositions are in the UNFLAGGED map.**

★★★★★ **A WRONG MAP NAME PROPAGATES INTO SOURCE.** `Rnd.h` carried a deliberate,
confident comment reasoning **from** a transposed map name, elaborated into a
load-bearing **vtable slot-ordering decision**. Settled with an **anchor OUTSIDE
the map** (the `.rdata` dispatch strings in `Rnd::Handle`): `'screen_dump'` →
`bl 0x82413098` (`+0x6C`), `'screen_dump_unique'` → `bl 0x824130f0` (`+0x70`),
bodies otherwise identical ⇒ **ScreenDump is the LOWER slot, declared FIRST.**
Header claim retracted in place with its refutation.
⇒ **A map defect is NOT contained by the map.** Δ measured exactly 0, and the
mechanism was verified so Δ0 could not silently mean "nothing happened"
(`Rnd::Handle` 3 charges → 1, survivor = the predicted `FOLD_FANIN` pair).

★★★★ **RECIPROCITY PROVES A MAP DEFECT EXISTS WITHOUT PROVING WHICH SIDE IS
WRONG** — existence and assignment are separate claims. W7 **deliberately shipped
no swap** on the `FileCache` STL-sort cluster: it is *provably* self-contradictory
(`sort<T,Cmp>` must call `__introsort_loop<T,Cmp>`; the map has the two sorts
pointing at each other's callees), but body-identity, call-edge tokens and TU
spatial grouping give **three mutually inconsistent answers**, and ⛔ **the token
check is CONTAMINATED BY CONSTRUCTION** — the callee names it trusts come from
the same suspect map, making it a **fixed-point problem, not a proof**. A swap
would have paid +224 B and been a guess.

★★★ **THE COPY-PASTE TWIN CLASS IS NOT SYSTEMATIC — one instance, now closed**
(W8). The structurally important part: **ARM A (both twins map-resident) is NOT
the hidden class** — objdiff already charges the copy against its own retail
address. **The hidden class is ARM B: one twin UNPAIRED, carrying no penalty at
all.** Positive control passes (reproduced the known defect mechanically);
negative control passes (19/21 T1 folds classified FOLD). ARM-B copy-paste
defects are now **zero**. ⇒ The tool survives with a **changed purpose: it is a
poor copy-paste detector and a good MAP-DEFECT detector** — every non-known hit
was a `target_symbol_map.json` misidentification.

★★★ **RETAIL CORRECTED THE PORT — the LITERAL ORACLE SPELLING WAS LOAD-BEARING.**
The oracle's apparently-redundant `DataNode ret(...); arr->Release(); return
DataNode(arr, kDataArray);` reads like a Wii artifact; tidying it to `return ret;`
gives **324 B**. Retail's relocation sequence has **two** `DataNode`
constructions — restoring the oracle **verbatim** moved **324 → 360 == retail
exactly**. ⇒ The standing rule "the oracle records intent, not the spelling" has
a **live converse**; neither direction is a safe default.

⛔ **TWO PRICING RULES CORRECTED BY MEASURED PREDICTION FAILURES** (W8 predicted
+24 B and measured **+184**; predicted inert and measured **+2 fns / +144 B**):
1. **Price from the charged-site list, not from the PATCH** — a wrong name also
   charges every **CALLER**.
2. ⛔ **"unmapped ⇒ metric-inert" IS FALSE** — `masked_equal` rose +2 because
   `matched_code` pairs bodies through the **funclet byte-signature channel,
   which needs NO map name.**

⚠ **Do not edit a worktree while `ab_measure` runs** — its restore-on-every-exit
path (correct, newly installed by TOOL-AB) silently reverted a mid-run tool edit.
⚠ **`collect()` over our objs is MULTI-DEFINER**: scatter-includes put
`BandDirector.cpp` in **15** objs and a glob keeps whichever sorts last — a
successful port briefly read as a no-op.

### 0.2 ROUND TWO — four more lanes, composition exact again

**Measured after a FORCED re-split** (map + alias files both changed, so a bare
`ninja` would have under-reported — see the round-one warning below):

| key | value |
|---|---:|
| `matched_functions` | **44,476 / 69,227** |
| `matched_code` | **3,743,844 / 10,320,664 B = 36.275223%** |
| honest floor | **21,578** |
| fuzzy | 48.616750 |
| **% of the 61.121% reachable ceiling** | **59.35%** |

**Round-two delta: +3 functions / +15,496 B**, composing EXACTLY with the four
independent A/Bs — W2-ENGINE +12,780 · W1b-GAME +2,892 · W5-CEILING +460 ·
W4b-DUALWIT −636.

**Then W6-ACCURACY landed on top: +1 fn / −128 B, composing exactly**, giving
**44,477 / 3,743,716 B = 36.273983%**, honest **21,579**, **254 units at 100%**.
**Day total across ten lanes: +33 fns / +20,012 B.**

★★★ **A PIN DEFECT'S SYMPTOM WAS RIGHT WHILE MY DIAGNOSIS WAS WRONG.** I briefed
`default/lsp` as mis-pinned because "the pin starts exactly where `lookup.c`'s
`vorbis_fromdBlook` is mapped." That clause is **literally true and
diagnostically worthless** — **`lsp.c:59` does `#include "lookup.c"`**, so that
function genuinely *is* lsp.c's TU and matches 100%. **The start was correct; the
TAIL was foreign** — 1,872 B of `memchr`, a CRT errno stub, an `NtCancelTimer`
wrapper and a run of `??__E`/atexit thunks, with the old end landing **mid-`??__E`
block**. Fixed by truncation: rows 37 → 2, units 253 → **254**, Δbytes 0.
⇒ **A reproducing symptom is not evidence for its mechanism** — same shape as the
2026-08-16 "count right, cause wrong" finding.

★★★★ **A NAME REPAIR HAS A DISTINCTIVE TWO-RULER SIGNATURE — USE IT.**
`0x8235c328` is `set<MoveDetector*>::_M_create_node`, **not**
`map<int,float>` — a whole-image census found only **19** surviving
`_M_create_node` bodies and exactly one whose value_type size contradicts its
allocation (`li r3,0x14` = 4-byte value_type; `pair<const int,float>` is 8).
The alias group also had **survivor/folded INVERTED**. Predicted **+76 − 204 =
−128 B**; measured **−128 B exactly, Δmatched +1**, and the **`none` control read
+76 B** — independently splitting the result into **+76 B of real code gained**
and **−204 B of forgiveness that rested on a false name**. Land the negative.

⛔ **TWO `fuzzy == 100` ROWS ARE FALSE CREDITS ON THAT SAME WRONG NAME** —
`_M_insert@map<int,float>` (BandList, 204 B) and `insert_unique@map<int,float>`
(232 B) both read **100** while their retail bodies call the **0x14 SET**
builder, impossible for a real map. **The at-100% defect class again.** Needs its
own pass.

✅ **A CHANGE DELIBERATELY NOT MADE IS A RESULT.** The `LabelSort` "retail sorts
by draw order" reading is **TYPE-IMPOSSIBLE** — `UILabel : public UIComponent`
has no `DrawOrder()`, and retail's comparator takes `const UIListWidget*`.
Rewriting our `LabelSort` would have been a **regression**. All ten
`LabelSort`-named entries bottom out in `WidgetDrawSort::operator()`; the family
is closed and never reaches a `stricmp` comparator. The lane ran **the control
W4b lacked** — **341** `lwz +0x164` (`mTextToken`) loads exist in `.text`, so the
instrument *can* fire, and **zero** feed a `bl stricmp`. Stays **UNPROVEN**, no
source change, and the ten wrong names are deliberately left alone (already
neutralised by proven aliases at 99.8–100%; renaming risks another −204 with no
accuracy gain).

★★★★★ **THE CEILING IS EFFECTIVELY FIXED — AND RAISING IT DOES NOT CLOSE THE
GAP.** W5-CEILING walked the FULL cost chain (identify → pin → wire → compile →
pair → match) on the most favourable unit in the tree and measured
**Δgap-to-ceiling EXACTLY 0**: the work raises the target and collects it in the
same step. Rate: **~1.5 h of expert time bought 460 B**; total available
≈1.6 kB genuinely-ours, ≤8.4 kB as a loose upper bound — against a **2,579,704 B
gap that already exists BELOW the ceiling**. ⇒ **Grinding under the ceiling
dominates by two orders of magnitude. Do not re-fund `auto_*` pin+wire.**
⚠ The briefed "8.9% portable slice" re-measured at 10.2%, but **96.0% of it has
NO identity hypothesis at all** (flanked by two *different* units); the part that
does was predicted at 20–45 kB and **measured 6,984 B**. Its #1 item is code
`f1874de3` deliberately un-attributed *for accuracy*.

★★★★ **THE YIELD WAS IN METADATA, NOT SOURCE — AGAIN.** W2-ENGINE was briefed at
the engine's 387,776 B divergence pool and found its **+12,780 B in the arg-only
class this doc twice calls "drained"**, via two wrong **map names** and **zero
source edits**. Three separate lanes today opened on a suspected source defect
and each **refuted its own hypothesis**: the source was right and the metadata
wrong. ⇒ **The near-term vein is map/pin accuracy, not porting.**

⛔ **AN "EQUAL" VERDICT ON A PLACEHOLDER TARGET CARRIES NO INFORMATION.** objdiff
called a string relocation *equal* while its target was the placeholder
`lbl_82016FB4`; `name_check` forgives placeholders **by construction**. The lane
had to read the bytes, which said `"copy_cats"`. Same disease as reading a 100%
row as evidence a callee is right.

★★★ **A REAL DEFECT CLASS THE METRIC CANNOT SEE, FOUND BY ACCIDENT.**
`BandDirector::OnGetCatList` and `OnCopyCats` have **byte-identical SOURCE
bodies** — a decomp copy-paste. The body is `copy_cats` semantics, so
`OnGetCatList`'s real body is **unported** — yet it pairs at **99.96% BECAUSE it
was copied from its twin.** No amount of residue grinding can surface this.
(Lane W8-TWINPORT dispatched to test whether it is systematic.)

⚠ **THE `cntlzw`/`extrwi.` BOOL-MATERIALIZATION LEVER IS PROVEN BUT THE VEIN IS
THIN** — W1b swept the top 90 named game near-misses for the signature and found
**0 hits**. The lever itself is settled: `& 1` **at the call site** (a semantic
no-op that forces a value context), and the **helper body is MEASURABLY INERT
across 10 structurally distinct forms**, including defining it inline in the
header. The prior in-file note saying "attack the helper itself" is **refuted and
corrected in source**. Do not fund a sweep.

⚠ **W4b: three of six alias withdrawals differ at EQUAL SIZE**, where a size test
is structurally blind — the within-build **content** test is what found them.
Its gate was **shown able to fail first** (fabricated alias → `CONTRADICTED
(FATAL)` → restored byte-for-byte). Nothing pruned; all groups kept.

⚠⚠ **COORDINATION DEFECT, RECORDED SO IT IS NOT REPEATED:** this whole session's
work landed on branch **`grounded2-restoration`, NOT `main`** — the shared tree
was checked out there on 08-16 and the environment banner still said "Current
branch: main", which the coordinator took at face value for nine lanes.
Concurrently another agent advanced `refs/heads/main` by **46 commits** in its
own job worktree. Nothing was lost and only **two files overlap**
(`CLAUDE.md`, `docs/decomp/TOOLING.md`), but ⇒ **verify the checked-out branch
with `git branch --show-current` before landing, never from the session banner.**

### 0.1 ROUND ONE — five lanes landed, composition verified exactly

**Measured at `3a524480` after a FORCED re-split** (renamer stamp removed +
`config.yml` touched; renamer patched 1,824 files):

| key | value |
|---|---:|
| `matched_functions` | **44,473 / 69,227** |
| `matched_code` | **3,728,348 / 10,320,664 B = 36.125079%** |
| honest floor | **21,575** |
| fuzzy | 48.612083 |

**Session delta: +29 functions / +4,644 B**, and it **composes EXACTLY** with
the five lanes measured independently in their own worktrees —
W3-IDENT +26/+1,976 · W0-ALLOC +0/+1,736 · W0-SIZEOF +2/+948 ·
W1-GAME +1/+296 · W4-ALIAS +0/−312. Five separate A/Bs, one whole-binary
confirmation, no inflation.

⚠⚠ **A MAP-ONLY MERGE IS INERT IN MAIN'S REPORT UNTIL YOU FORCE A RE-SPLIT.**
The coordinator's first post-merge read was `+26 / +3,712 B` — exactly W3 +
W0-ALLOC, with W0-SIZEOF's contribution **entirely absent** — because a bare
`ninja build/45410914/report.json` does not re-run SPLIT/renamer.
**It was caught ONLY because the composition failed by exactly one lane's
figure.** `ab_measure` does this automatically; a hand read does not.

★★★★ **EVERY ONE OF THE FIVE LANES REFUTED ITS OWN BRIEF, ALL IN THE
INFORMATIVE DIRECTION**: six allocator sites were **seven**; a two-cycle was a
**three-cycle**; an "unexecuted" pipeline had **run three times**; a "cascade"
was **thirteen independent bugs**; and "alias residue" was an **install queue
of the opposite sign**. ⇒ **The literal-testing rule is not ceremony — it fired
5 for 5.**

## 1. Headline at the start of the session (HEAD `6e13ee3f`)

| key | value | provenance |
|---|---:|---|
| `matched_functions` | **44,444 / 69,227** | `report.json` regenerated at HEAD (the prior on-disk report was one merge stale — it predated laneR's +204 B/+1 fn and GROUNDED-2's +1,728 B; composition checks exactly: 3,721,772 + 204 + 1,728 = 3,723,704) |
| `matched_code` | **3,723,704 / 10,320,664 B = 36.080082%** | same |
| honest floor | **21,546** (= matched − masked_equal 22,898) | same |
| fuzzy (whole-binary) | 48.591938 | same |
| ruler | `name_check` (self-declared in `provenance.diff_config`) | same |

Per-category (dashboard `progress_categories`; the five categories cover
8,594,604 B — the remaining 1,726,060 B is exactly the unattributed `auto_*`
class, which carries no category by construction):

| category | total_code | matched_code | code% | wmean fuzzy |
|---|---:|---:|---:|---:|
| game | 2,114,248 | 1,356,216 | 64.15% | 82.47 |
| engine | 3,995,832 | 2,239,228 | 56.04% | 78.19 |
| thirdparty | 105,740 | 94,352 | 89.23% | 93.77 |
| network | 269,640 | 31,496 | 11.68% | 17.41 |
| sdk | 2,109,144 | 480 | 0.02% | 0.03 |

*(Category rows are from the 06:49 report snapshot; the +1,932 B HEAD delta
lands in game/engine rows and does not change any percentage by more than
0.09 pp.)*

## 2. The partition — where every byte of the 10.32 MB stands

Computed by lane GAPMAP-1 (2026-08-17) from `report.json` joined to
`objdiff.json` + per-unit COFF symbol counts. **Self-validated four ways**:
rows sum to `total_functions`, bytes to `total_code`, Σsize(fuzzy==100) ==
`matched_code`, count(mpn==100) == `matched_functions`. Computed at the 06:49
snapshot (1,932 B / 1 fn behind HEAD — immaterial to every share below).

| # | class | rows | bytes | % total_code |
|---|---|---:|---:|---:|
| 1 | **MATCHED** (`fuzzy==100`) | 38,649 | 3,721,772 | **36.06%** |
| 2a | UNPAIRABLE — no source (229/230 units `xdk/*`, out of scope) | 4,454 | 2,106,356 | 20.41% |
| 2b | UNPAIRABLE — `auto_*` unattributed (identification) | 10,101 | 1,726,060 | 16.72% |
| 2c | UNPAIRABLE — map-scaffold units (base obj ≤2 syms) | 914 | 180,196 | 1.75% |
| 3a | PAIRABLE, arg-only (`mpn==100`, `fuzzy<100`) — **drained** | 5,794 | 797,144 | 7.72% |
| 3b | PAIRABLE, `fuzzy∈[95,100)`, `mpn<100` | 1,634 | 253,352 | 2.46% |
| 3c | PAIRABLE, `fuzzy∈(0,95)`, `mpn<100` | 959 | 337,064 | 3.27% |
| 3d | PAIRABLE, `fuzzy==0` (96% of bytes are ANONYMOUS targets ⇒ identification, not unported code) | 6,722 | 1,198,720 | 11.62% |
| | **TOTAL** | **69,227** | **10,320,664** | **100.00%** |

The 2c threshold is a genuine cliff, not a fitted cut (≤1 sym: 0 units; ≤2:
105; ≤3: 109), and reproduces AUTOID-1's 08-13 figure to the byte.

## 3. Reachable ceiling — 61.12%, and we stand at 59.0% of it

```
PAIRABLE bytes (unit has a base obj)          6,488,248 = 62.867%  raw
  − map-scaffold shells                         180,196
= CORRECTED reachable ceiling                 6,308,052 = 61.121%
matched_code 3,723,704  =  59.03% of the corrected reachable surface
gap-to-corrected-ceiling                      2,584,348 B
```

**What moved since 08-13/08-14:** ceiling fell 63.10% → 62.87% raw, fully
attributed — PAIRABLE lost exactly the 24,276 B that `auto_*` gained (a pin
reattribution, not a regression); no-source and scaffold classes byte-identical
across all three measurements. Ceiling vs 08-14 is flat (−1,304 B). ⛔ Standing
rule unchanged: **the ceiling moves both ways — re-measure it, never inherit.**

## 4. The gap, replayed on the 08-14 five-class scheme — shares stable to ~1 pp

| class (over the 2,766,476 B pairable gap) | bytes | % gap |
|---|---:|---:|
| anon `fuzzy==0` — **IDENTIFICATION**, unpairable at any source quality | 1,328,620 | **48.0%** |
| arg-only / reloc-name — **DRAINED** (~91% irreducible fold/map noise) | 797,144 | **28.8%** |
| named partial — **DIVERGENCE** (the real write surface) | 528,996 | **19.1%** |
| anon partial | 61,420 | 2.2% |
| named `fuzzy==0` (no body/stub) | 50,296 | 1.8% |

Divergence by category: **engine 387,776 B · game 126,520 B · network
9,828 B · thirdparty 4,872 B.** Within it, **93.6% of bytes are ARG-GATED**
(closing the instruction mismatches buys `mpn=100` / +1 fn and **zero bytes**
until register/branch/reloc-name charges also resolve); the clean
pure-source-collects-everything surface is **33,788 B (0.33 pp)** — and
5,036 B of that is the drained `CustomizePanel::Handle`. Structural bound
unchanged: pure regalloc rows cannot appear in `mpn<100`, so the permuter
floor inside divergence is 8.1% (permuter OFF by user directive).

⚠ Audit note from the replay: the 08-14 record's own five rows sum to
2,761,960 B against its stated 2,762,688 B gap — a 728 B internal leak in
that memory. Shares unaffected; today's replay has zero residue.

## 5. Alias-mechanism exposure — hold this number next to the headline

`scripts/symbol_aliases.json` at HEAD: **1,528 groups / 15,196 folded
memberships** (= MAPID-1's 15,190 + GROUNDED-2's 6 restorations, exact); 29
groups carry withdrawals, 6 restorations, nothing pruned. Magnitude by
ablation (ALIAS-2, `64088f62`): **818,416 B / 7.93 pp** — **~22% of everything
we count as matched rests on this mechanism.**
⚠ **Re-ablated 2026-08-17 (W4-ALIAS) at −810,540 B / −7.853563 pp** — the
mechanism did not shrink, **the tree moved under it** (source work replaces
forgiven bytes with earned ones). ⇒ **Deltas compose; absolutes do not.
Re-ablate before quoting this, exactly as with `total_code`.** Evidence split: PROVEN 92.73% ·
NEEDS_SOURCE 1.96% · CONTRADICTED 1.78% · NEEDS_MAP_ID **0.00% (drained to
zero by MAPID-1)**. **129,360 B is irreducible by construction** —
relocation-free thunks where ICF destroyed which name the site meant. The 11%
"unattributable" class from GROUNDED-1 was adjudicated a **census blind spot**:
0 of 1,894 rows depend on a non-proven membership.

## 6. What the last fortnight did (08-01 → 08-16, ~130 lanes)

Full narrative in DOCAUDIT-1's audit; one line per arc:

1. **Pin/splits hygiene** — ~1.7 MB vendor pinned; `total_code` de-inflated
   (one 204 B fn had been billed 210,136 B). Drained.
2. **Source-lever grind** — rev-statics +13, container types, `/fp:fast`
   paren barrier; `MILO_WARN` refuted thin; cheap-model lane closed by user.
   Drained.
3. **Ruler flip to `name_check` (08-12)** — the pivot: −817 kB / −7.9 pp from
   the ruler alone, `matched_functions` bit-identical; two tools found on the
   wrong ruler in opposite directions, both now resolve it at runtime. See
   `RULER_CHANGE_name_check_2026-08-12.md`.
4. **Map-defect campaign** — the period's best byte source: DC3-signature-on-
   RB3-address class (+5,392, +5,976, +5,236 B rows…), transposed pairs,
   splits re-homing. Concentrated in game; engine thin; network zero.
5. **Alias-ledger adjudication** — sized (818 kB), tiered, 80 fabricated
   memberships withdrawn (−10,916 B predicted exactly), `NEEDS_MAP_ID`
   drained (−1,656 B, exposing 6 real wrong-callee bugs), the "+8 B STLport
   source bug" refuted as our own COFF reader, 6 folds restored (+1,728 B).
   Deliberately net-negative: accuracy over headline.
6. **Body-port waves** — INSDEL-1..5/SRCARG/FAMILY/STORE-2: real fixes
   (comparator inversion +1,804 B, `EnumerateOffers` port +692 B…); the class
   is **bounded at ~9.6 kB actionable**, not the 440 kB it opened as.
7. **Measurement epistemics** — `ab_measure` tree-restore matrix fixed (8/12
   exit paths corrupted the tree), split fixed-point iteration added, native
   gate audited sound, ~35 instrument failures catalogued. Arguably the
   period's real product.
8. **DB integrity** — `IDENTITY_UNESTABLISHED` verdict wired into all 13 work
   selectors so a row we can't vouch is *the* function is never offered again.

Net 08-14→08-16: **+40 functions / −1,856 B** — the sign is the directive
working, not a regression.

## 7. ROADMAP — the honest paths from 59.0%-of-reachable

**Framing first: "fully matching" cannot mean 100% of the XEX.** 20.4% is
Microsoft vendor source we will never write (out of scope by standing
directive) and ~1.75% is map scaffolding. The meaningful end-states, in the
order the standing directives rank them:

**W0 — Correctness debt (small, behavioural, fund first).** The six
allocator-spelling divergences MAPID-1 exposed (`MemAlloc` / `_MemAlloc` /
`_MemAllocTemp` — the temp allocator is a *different allocator*, so these are
real bugs, near-zero bytes). Plus the `RndBone`↔`FilePath` swapped-`sizeof`
map mis-assignment and 21 sibling per-instantiation `sizeof` divergences.
~1–2 lanes.

**W1 — Game-layer divergence (the priority layer, per directive).**
126,520 B named divergence + 50,296 B named-no-body across band3/network.
Rank by size-if-it-crosses at fuzzy≥95 but **price from `report.json`'s
charged-site list, never a mismatch count** — 19 of the top 20 are arg-gated.
Expect many fixes to land as Δbytes 0 / +1 fn (mpn-only) — land them anyway;
`mpn==100` rows can still hide wrong callees. Sustainable yield: single-digit
kB per lane.

**W2 — Engine divergence (387,776 B, secondary per game-first directive).**
Same discipline. DC3-verbatim units cannot be adjudicated by source diff
(DC3 is newer); adjudicate on retail bytes.

**W3 — Identification at scale.** ⛔⛔ **THIS ENTRY WAS WRONG ON EVERY LOAD-
BEARING CLAIM AND IS RETRACTED — see
`W3_IDENTIFICATION_ADJUDICATED_2026-08-17.md` (lane W3-IDENT, merged
`6b519b3d`). The original text is preserved below the correction because it
is the second time a lane was funded off it.**

✅ **What is actually true:**
1. **The Ghidra+BinDiff route is NOT unexecuted — it RAN THREE TIMES and
   landed ~549 map names** (`e5293a8c` +286, `aa86fb41` +263, `5ff856bc`
   repoints), precision 89.8% → 96.4% → **98.6%**.
2. **It was KILLED ON MEASUREMENT, not abandoned** (lane DL-1, `254e80bd`):
   scoping to the correct DC3 `.obj` lifts top-1 46.8% → 80.7% with a
   sabotage leg at **0.0%** (a control that could fail), but the decoy null
   has **p95 = 1.000 ⇒ NO THRESHOLD EXISTS.** DL-1's verdict is structural:
   **obtaining a location prior IS WHAT PINNING A TU MEANS**, so the method
   is **circular for exactly the unpinned population it keeps being proposed
   for.** Also: DC3-BinDiff names only shared *engine* code (**9%** game
   yield), and `bindiff_match.json` is TU0-era **address-dead** (3.13% valid
   on TU5).
3. ⛔ **"W3 is the only ceiling-mover" is FALSE.** W3-IDENT's wave measured
   **Δceiling exactly 0, by construction** — named rows sit in units that
   *already* have base objs, so naming is a **pairing** play, not a
   pairability one. The only ceiling-*raising* route is `auto_*` pin+wire,
   which is 8.9% attributable-and-portable and dominated by 7-line Quazal
   scaffolds ⇒ **the ceiling is much closer to fixed than this doc assumed.**
4. **The XEX-loader conflict is resolved and NEITHER doc was right**: the
   service does not use `/opt/ghidra`; it uses the **VMX128 fork at Ghidra
   12.2, which already has `XEXLoaderWV` installed.** The real gap is
   **BinExport**, and per (2) it is not worth closing.
5. ⛔ **ROOT CAUSE, structural — `tools/bindiff_match.json` is GITIGNORED**:
   3.7 MB on disk with **zero git history**, indistinguishable from
   un-acted-on scratch. **A gitignored artifact is an invisible
   institutional memory.** Pointer committed at
   `tools/BINDIFF_MATCH_POINTER.md`.

✅ **What remains fundable here:** the **body-identity channel**
(`tools/ident_body_channel.py`), whose whole reachable surface is **234 rows
/ 21,224 B = 0.206% of `total_code`** — real, measured, and small. W3-IDENT
took 26 of them (+26 fns / +1,976 B, predicted +20…+26 / +1,900…+2,372).
⚠ Naming under `name_check` remains a bet whose payout is **bug exposure,
not bytes** — that wave surfaced `HamBattleData`'s `operator<<` for
`list<BattleStep>` dropping 100.0 → 99.791664 with `mpn` unmoved, a defect
the placeholder forgiveness had been hiding.

*(Retracted original text: "the only route to the big class … standing plan,
never executed (XEXLoaderWV needs a Ghidra 12.1 rebuild)".)*

**W4 — Alias ledger.** ⛔ **THE "TEMPLATE-1 = ~98 kB OF ALIAS RESIDUE" FRAMING
IS WRONG BY SIGN — corrected by lane W4-ALIAS (`3a524480`).** The *figure* is
right (98,032 B over 435 rows / 332 pairs, reproducing the 08-16 census
exactly — one of only two briefed counts to survive literal testing this
session). But **only 1,864 B of it is currently COUNTED AS MATCHED: 98.1% is
UNCOLLECTED POTENTIAL, not forgiven bytes**, and **592 of 631 pairs forgive
zero because they are not in the map at all.** ⇒ **TEMPLATE-1 is an INSTALL
queue, and its own census already ruled both open classes non-levers**
(`UNDECIDABLE` = "settled by pinning"; `BC_DIFFERENT_UNRESOLVED` = "downstream
of ordinary matching progress"). **Do not re-brief the 98 kB as alias work.**
✅ The demangled→mangled join now **exists and is validated**:
`tools/tmpl_demangle_join.py`, 631/631 pairs joined both sides, negative
control passes, 5 anonymous-namespace ambiguities reported rather than
silently resolved.
★ **Live lead instead:** the **21 dual-witness groups ALIASAUDIT-2 declined to
re-adjudicate are NOT clean** — 3 of them withdrew here at −312 B (predicted
exactly), each forced by the template argument, **two at EQUAL SIZE so no size
test could have found them**. A fan-in witness is evidence about the *callee*
address and **does not establish that the parent bodies fold**; the other **18
have never had the within-build content test applied.**
Still open and unchanged: 76 withheld contradictions (`verdict()`'s
`CONTRADICTED` is a **fallback, not a refutation**); the 129,360 B irreducible
thunk floor is **accepted, not work**.

**W5 — Gated / declined (do not fund without new evidence).** Permuter
(user OFF; floor 8.1% of divergence); jeff P1 relaxation for
`DataNode::operator==` (fleet-shared binary, declined 3×); `/TC-/TP` split
and `/EHsc`-`c` residue (metric-only evidence); sdk/xdk source (out of
scope); stubbing no-source units (metric-fitting).

**The strategic statement, unchanged from INSTR-1 and re-confirmed today:
there is no big lever left.** Every large-looking lever was sized and
deflated. From here the campaign is (a) a long, honest grind on W1/W2 in
single-digit-kB increments, (b) the W3 identification infrastructure play,
which is the only thing that moves the *ceiling*, and (c) the native port
(`hub_native.md`: **the user's stated real goal**), for which the matching
metric is instrumental, not terminal.

## 8. Top-20 crossing candidates (fuzzy≥95, mpn<100), with the trap labelled

See GAPMAP-1's table in the transcript / `docs/INDEX.md` pointer. Headline
rows: `CharacterCreatorPanel::Handle` 5,164 B (~209 est arg sites — likely a
register cascade downstream of one defect), `VocalPlayer::Handle` 4,936 B
(~224), `Spotlight::SyncProperty` 4,728 B (~103), `BandDirector::OnFileLoaded`
3,816 B (**~2 sites — RESIDUAL-1 shape, independent charges**),
`SaveLoadManager::SetState` 4,096 B. ⚠ `CustomizePanel::Handle` (5,036 B)
reads CLEAN and is **DRAINED** (RESIDUAL-2: 9 shapes inert + 3 worse) — do
not re-brief.

⛔⛔ **THE "LARGE COUNTS PROBABLY DISSOLVE WITH THE REAL DEFECT" READING IS
REFUTED — MEASURED, NOT ARGUED** (lane W1-GAME, merged `cb03ef87`). It was
pre-registered on `SaveLoadManager::SetState` (~114 est sites) and the
composition came back **110 `diff_arg`, ZERO unexplained**: **105 register
swaps across 28 DISTINCT pairs** (dominant pair only 29%, spanning idx
6→1022), **25 insert/delete in 13 SEPARATE clusters**, 4 real replaces ⇒
**thirteen independent body divergences, not one defect with a cascade.**
Two same-day micro-instances corroborate: FocusTracker's fix closed exactly
the 3 charges **at its site** and left the other 3 untouched at their
original indices; GemPlayer's 3 were all at one site and all closed.

> ★★★★ **CHARGES CLOSE WHERE THEIR CAUSE IS. THEY DO NOT DISSOLVE AT A
> DISTANCE.** ⇒ **Price a 114-site row at ~13 fixes, not at one**, and note
> that a partial fix buys **zero bytes by construction** (`matched_code` is
> all-or-nothing per row). The site estimator is valid as a **SIZE** signal
> and refuted as a **STRUCTURE** signal.

★★★★ **AND THE ORACLE RECORDS INTENT, NOT THE MSVC SPELLING.** W1-GAME's
headline failed prediction: transcribing rb3-Wii's own spelling of a fixed
expression scored **96.9% — WORSE than the 97.6% it started from**. Three
semantically identical spellings scored **96.9 / 97.6 / 100.0**. ⇒ On
bool-materialization shapes, **literal oracle transcription is actively
harmful**; the oracle tells you *what* retail computes, never *how* to spell
it.

## 9. Provenance

- Gap partition, ceiling, top-20: lane GAPMAP-1, 2026-08-17, scripts in
  `~/tmp/gapmap/`, from `report.json` (06:49 snapshot) + `objdiff.json` +
  COFF symbol counts; four exact-sum validations passed.
- History + docs audit: lane DOCAUDIT-1, 2026-08-17, from merge-commit
  bodies 08-01→08-16 + `CAMPAIGN_STATE_2026-08-14.md` + docs sweep.
- HEAD baseline: report regenerated at `6e13ee3f` by the coordinator,
  2026-08-17 (`rm report.json report.cache && ninja <report>`; 3,088 units,
  0 cache hits).
- Alias figures: `docs/decomp/ALIAS_UNPROVEN_REMAINDER_ADJUDICATED_2026-08-16.md`,
  `ALIAS_NEEDS_MAP_ID_DRAINED_2026-08-16.md`, GROUNDED-2 merge `6e13ee3f`.
