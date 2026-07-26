# Identification flywheel — round 5, driven to a fixed point (2026-07-26)

Lane `laneO-homing5`. Branch base `d83ca54f` (27,629), then merged main
`098f84a8` (27,816) mid-lane. **Lane result: 27,816 → 27,896 = +80 strict,
0 true losses at every step.** Six iterations, four of them exact fixed points.

Sibling sub-lanes run from here, each in its own worktree, reported separately
and integrated by measurement rather than by merge: `laneO-eh` (C++ EH content
channel — closed, +3), `laneO-rot` (map rotation repair — +28 on its own base,
11 of its assignments integrated here), `laneO-bigfam` (caller-side × content
conjunction on big families — refused on reach, +3), `laneO-wrongunit`
(WRONG-UNIT scatter-include).

The lane's question was not "can we harvest more" but "is the flywheel still
worth spinning". It is, but barely, and the number that matters is the yield
*curve*, not the total.

## Yield curve — this is the deliverable

| iteration | trigger | pins | reveals | caller-side inserts | strict delta |
|---|---|---:|---:|---:|---:|
| 1 | today's body work (SaveLoadManager, NextSongPanel, BandCharacter, AccomplishmentManager, Accomplishment, GameMode, 22-TU local-static sweep, ~380 new splits ranges) | 5 | 15 | 0 | **+20** |
| 2 | map grew by 21 | 0 | 0 | 0 | **0 — exact fixed point** |
| 3 | merged main: +187 strict of sibling body work | 0 | 7 | 1 | **+8** |
| 4 | map grew by 8 | 0 | 0 | 0 | **0 — exact fixed point** |
| – | integrate `laneO-rot`'s genuinely-additional content-resolved assignments | – | 11 | – | **+11** |
| 5 | map grew by 11 | 0 | 0 | 0 | **0 — exact fixed point** |
| – | integrate `laneO-wrongunit`'s scatter-includes + mis-pin repairs | 4 repins | 18 | – | **+41** |
| 6 | **a real source-change wave** (7 scatter-includes) | 0 | 0 | 0 | **0 — exact fixed point** |

Two iterations converge. Iteration 2 and 4 reproduced their predecessors'
verdict censuses *exactly* (62/7/4/51), which is the honest signature of a fixed
point rather than a thin harvest.

**Refill rate: ~+8 strict per ~190 strict of body work, ≈4%.** Round 4 was
+1,108 in one sweep. The flywheel is not dead, but it no longer pays for a
dedicated lane — it pays as a *cheap tail* bolted onto body-port waves. The
whole scan is 36 s wall (914 objs, 16-way), so the right operating model is:
run `homing_scan_all.sh` + `homing_gen4.py --reveal-frag` at the end of every
body-port wave and take the handful of reveals, rather than convening a lane.

**Iteration 6 is the decisive datum.** It followed a genuine *source*-change
wave — 7 scatter-includes and 4 splits repairs, +41 strict — which is exactly
the trigger condition round 4 named as the thing that refills the pool. It
returned **zero**: 0 pins, 0 reveals, 0 applicable PAYS. So the residue is not
waiting on fresh objects; it is structurally refused (see the BIG-FAMILY mirage
and the ambiguity census below). Scatter-include waves in particular re-emit
COMDATs we already knew, so they refill the *score* without refilling the
*identification pool*.

**The identification flywheel is drained at this map state.** It should be
re-run only after a wave that adds genuinely new bodies, and then only as a
36-second tail.

### Corollary that was worth proving

New *pins* do not refill the scan. `homing_scan.py` compares **our compiled
objs** against `band.exe`'s `.pdata`; target objs carved by dtk are not inputs.
Iterations 2 and 4 are the controlled demonstration: the map and splits changed,
our objects did not, and the yield was exactly zero. **Only a source/object
change refills the pool** — round 4's closing note is confirmed, not merely
repeated.

## Census (914 objs, iteration 1)

| class | occurrences | distinct retail VAs |
|---|---:|---:|
| NOMATCH | 101,864 | – |
| ALL-MAPPED | 36,747 | 9,446 |
| MULTI | 26,223 | 8,555 |
| UNIQUE-ICF | 3,924 | 772 |
| UNIQUE | 2,192 | **217** |

`gen4` drops over the 217: 128 already-covered, 63 name-ambiguous, 19
name-already-in-map, 1 no-unit → 6 pinnable.

**~70% of the UNIQUE and UNIQUE-ICF pools are EH funclets by name** (1,525 of
2,192; 2,761 of 3,924). Only ~30% is real material. This is the same
contamination that made the `MINSZ=16` experiment a net regression, and it means
the headline pool sizes overstate the reachable material by ~3x. Refused pools
restated as *non-funclet, non-STL named* records: MULTI 10,662 (not 26,223) and
UNIQUE-ICF 1,010 (not 3,924).

Where the refill came from: the TUs changed today contribute 143 of the 2,192
UNIQUE occurrences (~6.5%), concentrated in `AccomplishmentManager` (45),
`NetSession` (18), `VocalTrackDir` (15), `MetaPanel` (14). The other ~93.5% is
pre-existing residue that the drops correctly reject.

## Precision, measured held-out this round

* **caller-side inversion, sibling-family ≤16: 99.25% (662/667).** Family 1
  41/41, 2-4 348/351, 5-16 273/275. Confirms lane K's 99.09% at a newer map
  state. Still the only funded resolver.
* **content resolver (`multi_content_disambiguate --validate`): 74.13%
  combined** (RESOLVED-STRONG 66.96%, RESOLVED-SYM 77.45%); map-free `--no-sym`
  **66.77%, and every one of the 639 misses is `MISS/TRUTH-CONFLICT`.**

### That second figure is a MEASUREMENT ARTIFACT — do not quote it

`--validate` scores **one record per obj that instantiates a COMDAT**, so a
single wrong map entry for a ubiquitous symbol such as `StaticClassName`
manufactures *hundreds* of "misses" from one mistake. Sibling lane `laneO-rot`
collapsed its 461 miss records to **19 distinct symbols** and re-scored **by
name: 454 HIT / 19 MISS = 95.98%** (`/home/free/tmp/laneO-rot/valname.py`).

**The map is wrong; the content resolver is right.** Settled three ways:

1. the per-name re-score above;
2. direct proof — `?StaticClassName@TexMovie@@` is mapped to `0x823ec398`, whose
   body interns the literal `"user_login"`, while the content-desired
   `0x82742b90` interns `"TexMovie"`. That entry reads a false 100% today;
3. blind cross-validation — of the 94 string-verified assignments in main
   `a380ed69` (an entirely different channel: PE strings vs class tokens scanned
   from src/rb3-Wii/dc3), **74 were independently derived by the content
   resolver, 74/74 identical, 0 disagreements.**

**Any future lane quoting a `--validate` precision MUST use a per-name
denominator.** The per-record denominator systematically over-weights the
largest COMDAT families, which is exactly the population every hard
identification question is about.

Corollary, measured by `laneO-rot`: **291 of ~27,644 strict matches are
content-contradicted mispairs** — byte-identical modulo relocations, so
objdiff's normalized diff reads a clean 100% while the VA's own interned string
proves it is a different function. **~1% of the score is dishonest.**

## What the caller-side channel actually produces now

At every fixed point: 62-63 resolutions → **7-8 PAYS, 4 UNPINNED, 51
WRONG-UNIT**. Of the PAYS+UNPINNED, all but one **collide**: the name is already
in the map at a *different* VA, and that VA is not in the name's byte-identical
hit set — i.e. the incumbent entry is provably wrong.

**So the caller-side channel has crossed over from an insert engine into a
mispair-detection engine.** Only 1 of 12 was a genuine insert. This is a
qualitative change in what the tool is for, and the 11 repair leads were handed
to `laneO-rot` (rotation repair) rather than forced through
`tu5_map_apply_fragment.py`, which rightly asserts on the collision.

Independently, `gen4` rejected 19 plain-UNIQUE VAs with `drop_name_in_map` —
the same signature reached from a *different* evidence channel (reloc-masked
byte identity rather than caller relocations). Names where both channels agree
are the highest-confidence repair tier available.

### `span_predictor.py` has a recall bug — patch it before the next wave

`matches(tu, header)` asks whether the owning unit equals **the TU the scan
record came from**. But objdiff pairs whenever the owning unit's obj *defines*
the symbol, and a COMDAT is defined in every obj that instantiates it. So a
proposal is labelled `WRONG-UNIT` merely because the scan surfaced it from some
other TU that also instantiates the same COMDAT.

`laneO-rot` re-classified against every TU whose obj defines the symbol and
converted **10 of this lane's 51 `WRONG-UNIT` caller-side proposals into `PAYS`:
+10 strict, 0 LOST, 100% conversion.**

`laneO-bigfam` then showed the tool is wrong in **both** directions, which
matters more: `?Type@HamLabelCountDoneMsg@@` was called `WRONG-UNIT` and
converted (the recall bug), but `?StaticClassName@NgLight@@` was called `PAYS`
while a straight "any obj that defines the symbol" reimplementation called it
`NONE` — and it converted too. **So the define-set fix is necessary but not
sufficient.** The published 126/126 lifetime record is a *precision* claim about
its `PAYS` tier only; it says nothing about the tier boundaries. Until this is
fixed and re-validated, **do not trust either polarity** — treat `WRONG-UNIT`
as "unproven", not "refused". Fixing it properly is the highest-leverage single
change available to the identification stack, because it gates every wave.

## Refused, with the numbers to justify the refusal

| pool | size | why refused |
|---|---:|---|
| BIG-FAMILY (caller-side, family 17+) | 11,571 → **really 218** | see "the BIG-FAMILY mirage" below |
| MULTI / UNIQUE-ICF (content) | 10,662 named + 1,010 | resolver at 74% / 67%, inseparable from map error |
| NO-ANCHOR | 2,800 | no caller of these is itself homed; shrinks as the map grows |
| name-ambiguous | 134 VAs | see below |
| SHARED-VA | 225 | retail ICF-folded two instantiations onto one address; the map is 1:1 by construction, so this is structurally unreachable |
| DISAGREE | 33 | anchors point at different VAs |

### The BIG-FAMILY mirage — the largest "reachable pool" is 98% illusory

Lane K's headline residue was `BIG-FAMILY`: 11,547 candidates refused by the
sibling-family cap at a measured 96.65%. Sibling lane `laneO-bigfam` found the
number is an artifact of evaluation order: **`resolve()` tests the family cap
*before* the anchor lookup, so the `BIG-FAMILY` verdict structurally masks
`NO-ANCHOR`.** Re-running the fixed point uncapped:

| verdict | `--max-family 16` | `--max-family 0` |
|---|---:|---:|
| BIG-FAMILY | 11,586 | – |
| NO-ANCHOR | 2,797 | **12,597** |
| SHARED-VA | 225 | 1,044 |
| NOT-IN-HITS | 249 | 524 |
| DISAGREE | 33 | 90 |
| RESOLVED | 63 | **281** |

Lifting the cap entirely buys **218 extra proposals at 96.74%** (which
independently reproduces lane K's 96.65%), not 11,547. About **9,800 of those
names have no homed caller at all** and would be refused with or without the
cap. **No discriminator, and no conjunction of discriminators, can rescue a
candidate with zero anchors.** Do not send another lane at big families.

That lane also measured the hypothesis this project had not tested: the
**conjunction** of caller-side inversion and map-free content evidence, both
resolving and agreeing. It clears the bar handsomely — **99.57% pessimistic /
100% optimistic** (233/234), vs 97.30% caller-side alone and 99.28% content
alone; the two channels resolved 235 times and disagreed **once**. The
mutual-exclusion requirement is what buys it: the weak "content merely
corroborates" form is only 98.46% and does not clear the bar. But its **reach
over family-17+ is 5 proposals**, because 206 of the 218 uncapped candidates are
content `NO-EVIDENCE`. Funded on precision, refused on reach; +3 landed.

**The binding constraint on identification is anchor coverage, not any
discriminator.** Anchors are produced by strict matches, so body-port and pin
waves feed this channel better than any new probe ever will. That is the second
independent reason (with the ~4% refill rate above) that identification should
now ride on body-port waves rather than run as its own lane.

### The name-ambiguous pool is genuinely dead — quantified

134 distinct UNIQUE VAs are claimed by ≥2 *different* mangled names (63
uncovered + 71 already covered by a span). Breakdown:

* **88** involve at least one EH funclet name → naming an already-matched
  funclet is a measured regression (−13), so these are poison.
* **41** are pure STL/template twin ties (`_M_clear` /
  `_M_clear_after_move` over `vector<MatSwap>` vs `vector<TransformArea>`, etc.)
  with no discriminator on either side.
* **4** are soluble by the one principled tie-break available — excluding the
  DC3-only `hamobj` candidate (`BandList::Load` vs `HamCharacter::Load`,
  `CharData::Handle` vs `HamIKSkeleton::Handle`, `OldMatOption::~` vs
  `HamListRibbon::ScrollAnims::~`, `CharWeightable` vs `HamIKSkeleton` scalar
  deleting dtor). `hamobj` has **zero** units in `report.json` — no pins, so it
  can never pair.
* **1** partial.

A reach of 4 does not justify standing up a new tie-break mechanism, and the
mechanism would be dangerous: because candidates are byte-identical, **picking
the pinned candidate buys a guaranteed +1 whether or not it is correct**. That
is exactly the dishonest number this project refuses. Ties refused and counted:
**134**.

## Byproduct: the NOT-IN-HITS worklist is thin

`caller_side_invert.py` emits 243-249 `NOT-IN-HITS` records — a homed caller
pinpoints the retail address but our body is not byte-identical there, i.e. an
identification with a known target and a known gap. Enriched with retail
`.pdata` sizes and current match%, deduped to **83 distinct records, of which
only 15 are non-funclet**. Pin status of those 15: 8 own-unit, 4 wrong-unit, 3
unpinned; 186 of 249 have a **zero** size delta (pure codegen divergence).
Artifact: `~/tmp/laneO/nonbyte_worklist.json`. This is a real body-port lead
list but it is a dozen functions, not a campaign.

## The C++ EH content channel — measured and CLOSED

Lane K left one unexplored content channel: for exception-flagged functions, the
chain `handler → scope table → type descriptor → RTTI class-name string`, which
would be a key existing on *both* sides and not computed from the compared
bytes. `laneO-eh` built the decoder and closed it. **The RTTI sub-channel does
not exist in this binary:**

```
exception-flagged           9145      C++ EH (0x19930522)  9082 / SEH 63
  nTryBlocks == 0           8008      unwind-only, no catch at all
  nTryBlocks == 1           1074      every one a single catch(...)
  with a TYPED catch           0      ZERO TypeDescriptors reachable
distinct catch type names      1      "<catch(...)>"
```

RB3 is built `/EHsc`, so the compiler emits only synthesized `catch(...)`
(`nCatches==1`, `adjectives==0x40`, `pType==0`) — the chain terminates at the
third arrow. Two premises in lane K's note are also false for this toolchain:
there are **no `__ehhandler$` symbols** (an x86-only construct; X360 stores
`[VA-8]=handler, [VA-4]=FuncInfo*` inline before the entry point), and our
funclets are `__unwind$<opaque-int>` with no parent name — association is by
COMDAT section membership.

The EH shape probe over 33,717 hit sets confirms lane K's structural argument
one level down: `eh_present`/`eh_cxx`/`eh_handler`/`eh_types` vary in **1** set.
The single survivor is *what the unwind funclets reference* (varies in 1,388,
isolates the truth in 293 of 665) — admissible only because funclet bodies lie
outside the parent's compared byte range. Held-out **96.15%**, or **100%
(37/37)** with a new `positive_exclusion` guard; reach collapses to **3 distinct
retail VAs**. Landed +3 and closed.

That guard is a real scoring bug worth propagating: all 4 misses had a rival
resolving to an **unmapped** VA, which the acceptance rule counted as exclusion.
Retail keeps un-folded duplicates and `target_symbol_map` is 1:1, so "unmapped"
is *absence of evidence*; counting it as conflict biases every pick toward
whichever copy the map happened to name. **The same check should be made in
`multi_content_disambiguate.evaluate()`'s `symva` branch.** Reusable byproducts:
`EHDecoder` (a complete X360 MSVC C++ EH reader) and `our_funclets()` in
`scripts/harvest/eh_content_resolve.py` on branch `laneO-eh`.

## Dead ends confirmed, do not rebuild

Everything lane K killed stays killed: `.pdata` prolog shape (varies in 18 of
33,714 hit sets), call-graph shape / out-degree / leaf-ness / frame size (**any
probe computed from the function's own bytes cannot separate a hit set —
candidates are byte-identical by construction**), neighbourhood fingerprints (no
corresponding key on our side).

`MINSZ=32` and `HOMING_FUNCLETS` off remain correct; the ~70% funclet share of
the UNIQUE pools measured here is the mechanism behind that earlier result.

## A wrong splits pin manufactures false matches — and it is in the score today

`laneO-wrongunit`'s `BeatMatcher.cpp` repair reported **LOST = 8**, and every
one was the *same* anonymous `fn_` symbol re-homing 1:1 to the unit that
actually owns it. Unit-agnostic true losses: **0**. Those eight had been reading
100% under `BeatMatcher` purely because **objdiff pairs anonymous functions
positionally**, and `BeatMatcher`'s range wrongly bridged two of `DrumMixDB`'s
own ranges.

This is a second, independent mechanism (alongside the ~291 content-contradicted
mispairs) by which the score contains matches that are not what they claim to
be. **Every A/B in this lane was therefore re-checked unit-agnostically**:
compare the set of matching *function names*, not `(unit, name)` pairs, before
calling anything a regression. A unit-qualified LOST that is really a re-home is
a *correction*, not a loss — and treating it as a loss would have caused us to
revert a +41 wave.

Bad ranges found and *not* repaired (each emits ~0 of its mapped symbols from
the pinned owner) — a ready worklist: `band3/game/Player.cpp`
0x826A9CF0-0x826AAF78 (0/6, all GemTrainerPanel), `ColorPalette.cpp`
0x826AAF78-0x826AB314, `DataFile.cpp` 0x8276C6CC-0x8276D858 (0/6),
`Morph.cpp` 0x8238A450-0x8238ABA4 (0/2, neighbours all CharEyes),
`UploadErrorMgr.cpp` 0x82642450-0x82643310 (0/2, holds
`SetlistToStorePanel::Enter/Unload`), `IdentityInfo.cpp` 0x825C2300-0x825C23AC,
`AccomplishmentProgress.cpp` tail of 0x82592180-0x82592D18 (is UIEventMgr),
`DepthBuffer3D.cpp` 0x826F7E50-0x826F9A04 (straddles `band3/game/Singer`).

## Trust audit — before and after, both ways

`multi_content_disambiguate.py --trust-audit` run twice over the same iteration-5
scan, once with main `098f84a8`'s map and once with this branch's final map:

| | names checked | corroborated | **CONTRADICTED** |
|---|---:|---:|---:|
| before (main `098f84a8`) | 10,848 | 2,349 | **297** |
| after (this branch, +39) | 10,888 | 2,364 | **297** |

The contradicted **sets are identical**, not merely equal in size: 0 new, 0
fixed. 40 further names became content-checkable and 15 more corroborated. So
this lane's +39 is free of new map corruption — which is the check that matters,
because a mispaired ICF twin reads a clean 100% and would otherwise be
indistinguishable from a real match.

Re-run once more after integrating `laneO-wrongunit` (final +80 state): 10,906
names checked, 2,364 corroborated, **298 contradicted — exactly 1 new**:

> `??0AccomplishmentPlayerConditional@@QAA@PAVDataArray@@H@Z` @ `0x825e80d0`

**Declared rather than hidden.** It was not in main's map at all and is not a
pre-existing error we merely exposed — it is one of the 18 map entries inherited
from `laneO-wrongunit`. Its hit set is a clean two-element ICF twin pair,
`{0x825e55a0, 0x825e80d0}`, **both now mapped**, and map-free content says this
name does not belong at `0x825e80d0`. So it is a coin-flip that landed wrong and
reads a false 100% either way.

It is deliberately **not** removed: deleting it would cost a strict match without
establishing truth, because the sibling VA is occupied too. The correct fix is a
*rotation* (swap the two names), which is `map_rotation_repair.py`'s machinery,
not a fragment apply. Handed to that vein. Honest scoreboard for this lane:
**+80 strict, 1 known-contradicted entry among them.**

(`laneO-rot`, working the repair side on its own base, moved its audit 385 → 340
with 0 new. The 297 here is the same population measured against a newer map
that had already absorbed `a380ed69`'s string-verified repairs.)

## Verification discipline used

Every wave: `touch config/45410914/config.yml`, `rm -f
build/45410914/report.cache`, full `./tools/ninja-locked`, diff against a
baseline pickle, **NEW and LOST reported separately**. LOST was 0 at every step.
`overlap_check.py` after every pin wave (.text 3,980 ranges / .pdata 3,757
ranges, 0 overlaps). Every wave-A pin span was independently audited against the
retail `.pdata` table and each covers **exactly one** retail function — no
over-pinning.
