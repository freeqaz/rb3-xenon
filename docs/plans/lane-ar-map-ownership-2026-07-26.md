# laneAR — map ownership round 2 (2026-07-26)

Lane start: main `02c72524`, worktree baseline **36,705** by two builds.
Landed as `f9638cc9`: **37,063 → 37,123 = +60, 0 losses**, measured against
current main after full reconciliation with four concurrent lanes.

Worklist inherited from `docs/plans/lane-ao-map-ownership-2026-07-26.md` §7.
Four subagents: argreg adjudication, arbitrary-set confirmation, laneAO's splits
requests, and the joint map+splits seam.

## Headline: two of the four inherited items were REFUTED, and the yield came from a channel nobody had run

| inherited item | outcome |
|---|---|
| 27 argreg-proven mispairs | adjudicated: **3 repoints (+3), 5 deletes (+0), 3 splits-requests, 16 refused** |
| 48 "untagged-arbitrary" — bulk-tag them | ★**REFUTED 48 of 48.** Do not tag. They are plain mispairs |
| 6 splits requests | 10 adjacent ones converted **10/10 (+10)**; laneAO's own 6 handed to a lane that must re-measure |
| `--include-free` fixpoint | ★**reached, and it is 0** — in three flag configurations |
| *(not on the worklist)* | ★**content disambiguation: +50** |

## 1. `--include-free` reaches a fixpoint, and the fixpoint is zero

`map_displace_round.py` against a **fresh whole-tree homing scan** (1024 TUs,
current obj state), with `--pays-only --strict-guard`:

| configuration | applicable displacements |
|---|--:|
| plain | **0** |
| `--include-free` | **0** (4 `T0_FREE_VA`, all span-refused) |
| `--include-free --break-ties` | 1 — and that 1 is `0x82553fc8 ?Terminate@RndMat@@SAXXZ` |

★The single survivor is the entry the map's own `_denylist_comment` calls a
**"permanent oscillator"**: it was argreg-refuted long ago, but its *bytes* still
match, so the tool re-proposes it on every run. laneAO filtered it by hand; so
did this lane. **`map_displace_round.py` now consults `_denylist`**, which turns
the answer into an honest 0 without hand-filtering. `--break-ties` is worth
running regardless — it broke **61 ICF ties spatially** — but every one it broke
landed on a VA whose holder already reads strict-100, and was correctly refused.

★**The refusal reason is the finding.** Every surviving candidate died on
`span_predictor`, not on evidence: **12 WRONG-UNIT + 1 UNPINNED**, each a name
whose reloc-masked bytes are *uniquely* identical at exactly one VA in the whole
11.8 MB image. That is proof-grade identity that cannot score because the VA
sits in the pinned span of a unit whose obj does not define the name. The
byte-identity map channel is drained; **its entire residue has converted into
the joint map+splits seam.**

★Three of the 13 were laneAO §5 splits requests (`?Init@SongPreview@@`,
`?OnPassthrough@Synth@@`, `?Save@FlowWhile@@`) — **independently rediscovered
from byte identity alone**, a different starting point from laneAO's. Second
instance of this cross-validation; as before it is *also* an accounting trap, so
they were routed to one owner, not two.

## 2. The channel that paid: content disambiguation (+50 of the +60)

`map_displace_round.py` accepts a claimant only when its hit set is **exactly
one VA**. Every multi-hit (ICF-tied) name is therefore invisible to it — and
that is most of them. `multi_content_disambiguate.py` **proposals mode** ranks
those by *which callee symbols the retail code at each candidate VA actually
relocates to*. Orthogonal evidence, and nobody had run it this cycle.

Funnel: **650 proposals → 556** free-VA & name-unmapped **→ 284 PAYS records →
81 distinct VAs**, every one uncontested 1:1 (the 284/81 ratio is one (name,va)
pair seen from several COMDAT TUs, not a contest).

Guards armed: `_denylist`, vendor band, holder-already-100,
name-already-mapped-elsewhere, span `PAYS`-only, and the "never name an
already-matched anonymous funclet" −13 rule — **which fired 0 times** (all 284
anonymous targets read <100).

★**It is a flywheel and it converges in two cycles.** Each landed anchor is new
callee evidence, so re-running refills: PAYS went **81 → 7 → 0**. Round 3 is a
measured fixpoint. Round-1 measured +82/0 losses, round-2 +7/7-of-7-paid.

★**The `dynamic_init` ±2 noise pair was caught red-handed.** The two extra gains
in round 1 were exactly `?Disable@CamShot@@QAAX_NH@Z` and the CharLipSync
`__fill_n<_Bit_iter>` — and in round 2 the *same pair* appeared as the two
losses. It came in and went back out, netting zero across the waves. Fourth and
fifth independent observations. Do not chase it; give both legs the same builds.

## 3. ★The reconciliation that halved the number — and why the number is still right

Main advanced **four times** mid-lane (36,707 → 37,065) including laneAQ's
identity funnel (+276, 432 names). Re-checking the 88 landed entries against the
new main:

| | n |
|---|--:|
| laneAQ landed **character-for-character identical** | **25** |
| **contradicted** (same VA, different name) | 14 |
| name claimed at another VA | 1 |
| **net-new** | **48** |

So the honest contribution is **48, not 88** — the +87 measured at the old base
was real *then* and double-counting *now*. **Two lanes on one evidence channel
must measure the union, never the sum.**

★The contradictions are not simply errors. laneAQ and this lane assigned
`??0RemoteUserUpdatedMsg` / `??0RemovingRemoteUserMsg` to `0x823e1e20` /
`0x823e2020` in **exactly swapped order — and both measured a gain**, because
with masked-identical twins either assignment scores. That is
`_bijection_arbitrary` observed empirically rather than asserted. Deferring to
the lane that landed first costs nothing and keeps the map single-valued.

## 4. The 27 argreg mispairs: 3 repoints, 5 deletes, and a priced doctrine

Argreg proves **wrongness**, not the right answer. Adjudicating all 27 gave
3 repointable / 5 deletable / 3 splits-blocked / **16 refused**.

Repoints, **+3 with 0 losses — all three landed, including the one flagged
medium-risk**:

* `0x82b71e10` `??2RateTransposer::operator new` → `?enableAAFilter@RateTransposer@soundtouch@@QAAXH@Z`
  (rel 0/8, zero masking, `stw r4,0x68(r3)`; soundtouch's source line is
  literally `bUseAAFilter = newMode`). Also vacates `0x82b70e78` — a combined
  name+VA repoint.
* `0x8265df00` `?swap@vector<_Slist_node_base*>` → `?_M_before_begin@hashtable<…>`
  (rel 0/132; 26 instantiations share the bytes, but `SongSortByRank.obj`
  defines exactly one, unmapped, span PAYS).
* `0x82754a48` `?Main@ObjectDir@@` → `?SetCacheMode@DirLoader@@SAX_N@Z`. The
  body *stores* r3 as a byte to a global; `Main()` takes no argument and would
  load. ★At 12 bytes with 2 of 3 instructions relocated **byte identity is
  vacuous** (3,348 obj-hits / 556 names) — this rested on the argreg proof plus
  span ownership alone, and it paid.

★**The deletes priced the doctrine.** Retiring 5 argreg-proven-wrong bindings
was measured by reverting the five lines and rebuilding: **+0 strict, 0 losses,
`matched_code` UNCHANGED, whole-binary fuzzy 37.029655 → 37.028950 = −0.0007pp.**
Four are `??__E`/`??__F`, which objdiff's `pair_funclets_by_bytes` re-pairs once
unmapped — **flat `matched_code` is the direct evidence of that**. So "unmapped
beats wrongly-mapped" is far cheaper than the standing caution implies: the
40–96 % fuzzy that deletion appears to discard is almost entirely recovered.
(`0x826cce60` is additionally a Kinect/NUI DC3 identity with no RB3-360
counterpart — the same import-artefact class as laneAO's `??0WiiFriendsScreen`.)

**Negative result worth keeping:** `map_rotation_repair.py analyze` (map-free
content resolver) over the fresh scan resolved 2,700 names and **not one of the
27 is among them**. No REPOINT-VA is derivable from the name side either. The 27
are an honestly-hard residue, exactly as laneAO characterised them.

## 5. ★The "48 untagged-arbitrary" hypothesis is REFUTED 48 of 48

The trust audit reproduces cleanly (`--trust-audit` → 12,166 names checked, 83
contradicted; laneAO's 90/12,127 differs only by map drift). The partition is
identical: **48 multi-candidate + 35 singleton**.

★**laneAO's prescribed confirmation test is a tautology.** "Byte-diff 2–3
`StaticClassName` siblings" passes for all 48 — but it can *never fail*, because
`homing_scan.py` **constructs** the class by reloc-masked byte equality. The
decisive test is whether the **masked slots** resolve to identical content. They
do not: the 88-byte `StaticClassName`/`Type` class has **453 members, each
materialising a distinct legible class-name literal**. Identity is fully
establishable, so the entries are **plain mispairs, not arbitrary** —
`?StaticClassName@BaseMaterial@@` is mapped at a VA whose own reloc slot
materialises `"Movie"`; `CharFeedback`→`"CharBoneOffset"`;
`?Type@SkeletonIdentifiedMsg@@`→`"local_machine_updated"`.

**Independent corroboration, arrived at the same day from the other direction:**
lanePHANTOM (`01a0e9fa`) decoded the same `lis/addi` literals and repointed 31 of
them. Two lanes, two code paths, one conclusion: **repoint by literal, do not
tag.** Also from lanePHANTOM, and consistent with §1 here: *repairing the map
alone costs full price (−30); repairing map + split is nearly free (−2).*

Genuine arbitrariness survives only as a small residue (`DxCam`/`RndCam` both
name themselves `"Cam"`). Honest order: **repoint first, tag the 2–3-way
remainder afterwards.** 3 entries (`DataACos/ASin/ATan`) are audit false
positives — a float-pool decode artefact — and 2 (`AppInlineHelp`,
`NgSpotlightDrawer`) are **source** defects: our `Symbol()` literal carries a
DC3-era prefix retail never had, invisible to objdiff because the string sits
behind a masked reloc.

Tagging *would* have been match-neutral —
`scripts/obj_target_symbol_renamer.py:107` skips any key not starting `0x`
*before* registering a rename — but neutral is not the same as correct.

## 6. The joint map+splits seam: 10/10 converted

The 10 non-overlapping WRONG-UNIT/UNPINNED candidates from §1 all landed,
**+10 with 0 losses**. The seam's record goes 15/15 → **25/25**.

★**Nine of ten were splits mis-attribution, not scatter** — single-function
`.text` islands whose `[start,end)` equals the function's `symbols.txt` extent
exactly, sandwiched by the true claimant's own ranges. **The pin and the map
entry were mutually reinforcing: each alone reads as evidence for the other.**

> ★**CORRECTION (laneAV, 2026-07-26) — "sandwiched" does NOT license a wholesale
> range move.** The shape above is a *single-function island* whose extent equals
> one symbol's `symbols.txt` extent. Read as a general rule it is wrong, and
> laneAV measured the cost: moving a whole sandwiched island **destroyed a named
> match and replaced it with a byte-paired anonymous `fn_`** — net zero while
> looking like progress. ⇒ **A whole-island move can buy a fake match with a real
> one.** Move a range only when every symbol inside it is accounted for; otherwise
> move the single function. Safe composition primitive:
> `scripts/harvest/splits_additive_merge.py` (refuses on any interval overlap —
> `git apply --3way` cannot do this).

★**The discriminator is not the symbol's kind.** The pre-issued heuristic
("STL/`??_D`/`??_G` thunks ⇒ scatter; member functions ⇒ splits") was **wrong**:
`??_DDxLight`, `??_GDxLight` and `__destroy_range<Data*>` are all
compiler-generated/template symbols and all shape (a). The real test is whether
the VA is **already inside the claimant's own pinned range** (⇒ source-side) or
in a **foreign island** (⇒ splits move). Only one of the ten was the former.

★**A COMDAT section size is NOT the function size** — it includes the EH
funclet. Two valid candidates were nearly refused on a bogus mismatch (0xB8 vs
0x88; 0x11C vs 0xC4). The homing scan's `size` field is the true size.

★**`DECOMP_FORCEBLOCK` is a silent no-op under MSVC** — `src/decomp.h:38` gates
it on `__MWERKS__`, so every Wii-oracle forcing block in the tree expands to
nothing for the 360 build. Re-stating `SongDB.cpp`'s block for MSVC paid +1 with
**no splits change at all**. `grep -rln DECOMP_FORCEBLOCK src/` enumerates every
other TU where the oracle already recorded "retail emitted this here" and our
build silently drops it. **Unmined vein.**

## 7. Two tool defects fixed (one of them corrupts the map)

★**`map_rotation_repair.py apply` selected map-entry lines with
`s.startswith('"0x')` — which also matches the bare `"0xVA",` ELEMENTS of the
`_denylist` / `_icf_arbitrary` / `_bijection_arbitrary` (1,207-entry) provenance
arrays.** Applying a 3-entry plan wrote a `"key": "value"` pair *inside* the
`_bijection_arbitrary` array and the map stopped parsing (line 1381 —
`0x82754a48` is also listed there). A `remove` would instead have **silently
deleted an array element**. Neither existing assert can catch this: they build
their view from `json.load(...)` filtered by `isinstance(v, str)`, so the arrays
are invisible to the checker and visible only to the textual writer. Now
requires the colon.

★**`map_displace_round.py` now consults `_denylist`** (§1).

★**`scripts/harvest/land.sh` REFLOWS the whole map.** Its 3-way dict-union
resolve rewrote **1,531 of 1,531 lines**. Content was verified correct
entry-by-entry — and it *did* correctly protect 6 deliberate deletions from
resurrection — but the reflow violates the project's byte-splice invariant and
would collide with every lane holding in-flight map edits. **Compose the two
deltas textually instead.** (Here the rebase was unnecessary anyway: the branch
was already a descendant of main's HEAD.)

## 8. Invariants and method

* Map after landing: **24,608 address lines, 0 duplicate VAs** — checked **on
  the raw lines**, because `json.load` silently keeps the last of a duplicate
  key and hides the problem entirely. `_bijection_arbitrary` 1207 /
  `_icf_arbitrary` 25 / `_denylist` 3 intact.
* Every leg: 2 builds, `rm -f build/45410914/{report.cache,target_symbol_renames.stamp}`
  + `touch config/45410914/config.yml`. **The map is not a ninja input to the
  renamer**; without the stamp removal a map edit silently does nothing and
  reads as a refutation.
* Every A/B **unit-agnostic** — by function name as well as (unit, name).
* `symbols.txt` restored before every commit; never committed.
* The +60 is the **measured union**, not 50 + 10. Collision check first (0 VA,
  0 name, 0 removal collisions), then one measurement of the merged tree.

## 9. Residue — the named worklist for the next lane

1. ★**113 joint map+splits candidates** (49 WRONG-UNIT across 36 owner units +
   64 UNPINNED), in `/home/free/tmp/laneAR_joint_residue.json`. Same shape as
   the 10 that converted 10/10. Biggest owners: `Character.cpp` 5,
   `HamCamTransform.cpp` 4, `AccomplishmentDiscSongConditional.cpp` 3. **This is
   the richest remaining vein this lane found.**
2. **`DECOMP_FORCEBLOCK` sweep** (§6) — every TU where the Wii oracle recorded a
   scatter COMDAT and the MSVC build silently drops it. Free of splits churn.
3. **20 still-live `_content_contradicted` entries** after lanePHANTOM's 31
   (`/home/free/tmp/laneAR_a2_tags.json`, `live_after_inflight_main_edit`) —
   **repoint by decoded literal, do not tag**. Includes 3 `?ByteCode@…Msg@@`
   whose true home is the VA already holding `?StaticByteCode@<same class>@@`
   (a real ICF fold: two names, one VA).
4. **35 singleton-class contradicted** (`/home/free/tmp/laneAR_a2_residue.json`):
   17 FLOAT_POOL_ALIAS (audit-weak), 8 LITERAL_PRESENT_ELSEWHERE (the real
   worklist), 7 LITERAL_ABSENT_FROM_RETAIL (**source** drift — map is right),
   3 STRING_TAIL_MERGE (reader artefact).
5. **3 argreg splits-requests** with exact byte-identity evidence, blocked only
   by the pin: `0x82356f48 → ?SetPlayerLocal@TrackPanelDirBase@@` (rel 0/28),
   `0x826f9878 → ?IsTambourineButton@TambourineManager@@` (rel 0/16),
   `0x8276c958 → ?DebugText@DirUnloader@@` (the VA loads `"UnLoader: %s"`; its
   sibling at `0x8276ad08` loads `"DataLoader: %s"` and is already correct).
6. ★**SOURCE defect: `ObjRefConcrete<T,ObjectDir>::Replace` is missing from the
   tree.** `0x823bdab0` / `0x82314b70` read a false 95.62 % on the wrong
   function. Both retail bodies decode to
   `if (mObj == r4) SetObj(__RTDynamicCast(r5,0,&src,&dst,0))` — exact MSVC
   `__RTDynamicCast` ABI — i.e. a 2-argument `Replace(Object*, Object*)` that our
   411k-symbol COFF index does not contain at all (and our `SetObj` is 100 B vs
   retail's 96 B). Emitting it makes both VAs repointable and retires the false
   credit. *This is the inverse of laneAO §4, where a bad map entry manufactured
   a false source lead: here a bad map entry is HIDING a real one.*
7. `?SetType@RndPropAnim@@UAAXVSymbol@@@Z` @`0x82429540` — the one content
   insert that did not pay; now reads 62.92 % in `default/PropAnim`. Correct
   identity, real body divergence. A source lead, kept deliberately.
8. **249 contested ICF claims** that `--break-ties` could not separate
   spatially, and **375 refused because the holder already reads strict-100**.
   Both correctly refused; the first is the natural population for a genuine
   arbitrary tag once §3's repoint-first order is exhausted.

## 10. laneAO's own 6 splits requests: 4 landed (+4), 2 refuted (`3cdd8929`)

Re-measured against current main, 2 builds per leg, unit-agnostic:
**37,280 → 37,284 = +4, 0 losses**, replicated on the build-1 leg
(37,278 → 37,282, identical gain set). None of the 4 VAs or names existed on
main beforehand — no double-count with the concurrent map lanes.

`?Init@SongPreview@@` (whole-range MOVE, and its `.pdata` record
`0x82240328-0x82240330` verified against `band.exe`'s RUNTIME_FUNCTION table),
`?OnPassthrough@Synth@@` (MOVE to `CheatProvider.cpp`), `?Save@FlowWhile@@`
(CARVE out of `BandLabel.cpp`), `VorbisReader` ctor (pin+map, then source fix).

★**The lever generalised: a pin whose function is unmapped HIDES the source
defect — pin first, then read the residual offset delta as the struct
correction.** Pinning the `VorbisReader` ctor alone measured **+0**: the newly
paired ctor read 99.992 % with exactly one mismatch, `stw r29,0xec(r30)` vs our
`0xc0`, delta −44 = **−0x2C** — confirming laneAO §7's standing `mHdrSize`
prediction *to the byte*. Retail emits three consecutive zero stores at
`0xec`/`0xf0`/`0xf4` = `mHdrSize`/`mHdrBuf`/`mCtrState`, i.e. `mHdrSize`
immediately precedes `mHdrBuf` as in rb3-Wii's declaration order; it shifted
+0x2C with the TU5 tail and only the placeholder blob sits at `0xc0`. Relocating
it *inside* the existing placeholder array keeps `sizeof` byte-identical, so the
blast radius is nil. The pin and the header fix were measured as separate legs
(+0 then +1): **neither pays without the other.**

★**`?OnPassthrough@Synth@@`'s claimant was 5-wide, and COMDAT ordering is a NULL
discriminator** — all five objs emit the identical COMDAT sequence, because the
order is a property of the shared header, not of the obj. What did discriminate
is spatial: `??$_Param_Construct@UCheat@CheatProvider@@…` at `0x826fe590`, a
CheatProvider-only symbol, anchors the head of the retail run, and CheatProvider
already owns the adjacent range on **both** the `.text` and the `.pdata` side.

### Both refusals overturned their premise

**Request 5 (`fn_826C44F8`, "5 un-split accessors") — refuted, and already
matching.** Our `?Handle@GemPlayer@@` is 5,612 B = **0x15EC exactly**, retail has
*one* `.pdata` record for the whole span, and it reads **100.000 %**. The 5
accessors were `/Ob2`-inlined into it and do not exist as symbols in
`GemPlayer.obj`; the retail instruction before each claimed entry point is
mid-stream (`lwz r11,0(r11)`, `b -0x7c`, `rlwinm.`), never `blr` or padding.
*Residue:* the map holds 5 interior-pointing entries — `0x826c48f8`,
`0x826c4908`, `0x826c4910`, `0x826c4958`, `0x826c4970` — provably inert (no
symbol to rename) but exactly the kind of wrong identity that manufactures a
false source lead.

**Request 6 (`CharClip.s`/`HamPlayerData.s` overlap) — refuted; the root cause
is not a dtk over-carve.** `HamPlayerData` appears **zero** times in
`splits.txt`, and `build/45410914/asm/HamPlayerData.s` is dated **Jun 11**,
pre-dating the Jul-15 TU5 flip — it is **TU0-era residue that is no longer
regenerated**. Proof: the stale `.s` claims `.pdata@0x821FB8D8 → fn_8237FBD8 len
0x9C`, but the live `.pdata` at that address holds `fn_0x82310f98 len 0x48`, and
no live `.pdata` record begins at `0x8237FBD8` at all. The live carve is a single
function `[0x8237FBD4, +0x2C)` owned by `CharClip.cpp`, at 100.000 %. laneAO's
*conclusion* (do not repoint) was right; its *reason* was half-right — retail is
`subi r31,r12,0x80` at FBD4 then `mflr r12` at FBD8, so the `mflr` it read as "a
real prologue at FBD8" is the **second instruction of `fn_8237FBD4`**.

★**New trap for the index: stale `build/*/asm/*.s` files for units no longer in
`splits.txt` are silently wrong and are never regenerated.** Any lane reading
carve geometry from a `.s` must check the unit is still pinned and the file
post-dates the TU5 flip. `0x8237fbd8 ?SetCharacter@HamPlayerData@@` is a
surviving TU0-era address (inert today); since
`system/hamobj/HamPlayerData.obj` does define the symbol, its true TU5 VA is
findable with `homing_reverse.py`. **A sweep for other TU0-era survivors is
worth a lane.**
