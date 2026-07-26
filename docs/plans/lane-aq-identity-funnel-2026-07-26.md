# Lane AQ — the >68 B anonymous pool: identity funnel (2026-07-26)

Worktree `~/tmp/wt-laneAQ-1`, branch `laneAQ-1`, base `9b2d2737` on main.
Baseline **36,659** strict (`match_percent_normalized == 100.0` in
`build/45410914/report.json`), reproduced in-worktree before any edit.

Predecessors this lane was funded on:
`docs/plans/lane-al-autocarve-2026-07-26.md` (which sized the pool at ~2,890),
`docs/plans/laneAN/objdiff-84byte-cap.md`,
`docs/plans/identical-pct-cluster-scan-2026-07-26.md`.

---

## 1. ★ The pool re-derives to 6,542, not 2,890 — and its centre of mass moved

laneAL defined the pool as *"anonymous entries above 68 bytes in `auto_03_*`
carve spans"* and measured **~2,890** at 32,182 strict. Re-derived at HEAD
(`scripts/harvest/autocarve_funnel.py --worktree .`), that same definition now
gives **1,487** — laneAL's own interior-hole sweep and the laneAC/AD/AM waves
consumed roughly half of it by *claiming the address*.

But the address-claiming did not make those functions matchable; it moved them
from a synthetic `auto_03_*` unit into a real pinned unit, where they are still
anonymous and still unpaired. The honest pool is therefore **every anonymous
`fn_8XXXXXXX` target function, size > 68 B, outside the vendor window
`0x82800000..0x82D00000`, not at strict 100** — regardless of which unit holds
it:

| | count |
|---|--:|
| anonymous `fn_` rows in `report.json` | 45,030 |
| ...in scope (outside `0x82800000..0x82D00000`) | 32,680 |
| ...and > 68 B | 6,567 |
| **...and not strict — THE POOL** | **6,542** |
| ├ in an `auto_03_*` carve span (laneAL's view) | 1,487 |
| └ **in an already-PINNED unit** | **5,055** |
| of the pool: reading exactly 0.0 % (unpaired) | 6,537 |
| of the pool: reading 0 < p < 100 (paired, body-divergent) | 5 |
| median size | 168 B |
| total | 1.83 MB |

**The 5,055 is the finding.** laneAL's framing — "these need an identity from
the map lane, then source" — is right, but 77 % of them are no longer an
*attribution* problem at all: their address is claimed, their owning TU is
known, its base obj is compiled and on disk. What is missing is only the
**assignment of a name from that obj's COFF symbol table to that VA**.

Top pinned units by pool demand: `RockCentral` 75, `BandCharacter` 75,
`DataFunc` 65, `AccomplishmentPanel` 63, `UIStats` 59, `rndobj/Rnd` 54,
`TrackWatcherImpl` 51, `TrackPanelDir` 50, `CharacterCreatorPanel` 47,
`Character` 46, `MeshAnim` 45, `Mesh` 44, `PropKeys` 44, `DirLoader` 43,
`game/VocalPlayer` 43.

## 2. It is a SELECTION problem, not a supply problem

Per pinned unit, counting the base obj's real (non-funclet) code symbols > 68 B
whose name does **not** appear in that unit's target obj — i.e. candidate names
with nowhere to go:

| | |
|---|--:|
| pinned units holding pool demand | 538 |
| total demand | 5,055 |
| total unpaired base-symbol supply | 86,534 |
| Σ per-unit `min(demand, supply)` | 5,014 (99.2 % of demand) |
| units with **zero** supply | 3, holding 10 demand |

Supply exceeds demand by **17×**. So no counting or forced-bijection argument
generalises: only in the handful of near-balanced units (`DataFunc` 65 vs 78,
`UIStats` 59 vs 52, `rnddx9/Rnd` 38 vs 23) does cardinality constrain anything.
Everywhere else the channel has to *discriminate*, and the supply set is
dominated by STL instantiations and inlined helpers that never reach `.text`.

Data: `/home/free/tmp/aq_supply.json`.

## 3. ★ RTTI as a vtable anchor: CLOSED, empty

A retail vtable slot is an attractive identity anchor because it is *content*,
not a property of the function's own bytes — so unlike byte-identity homing
(`docs/plans/lane-al-autocarve-2026-07-26.md`: 58.8 % NOMATCH, drained) it
survives body divergence. `scripts/harvest/vtable_global.py` already exploits
this but needs **≥2 already-mapped anchor names** per vtable. The obvious
scaling move is to anchor on **RTTI** instead: MSVC emits a Complete Object
Locator pointer at `vtable[-1]`, and the COL reaches a type descriptor holding
the `.?AV<Class>@@` name — an anchor-free target-vtable → class-name map.

**Measured, and it does not exist for RB3's own classes.** I first concluded
"retail has no RTTI at all" and **that was wrong — retracted here.** The COLs
are present:

* 1,280 vtable runs in `build/45410914/obj/auto_00_82000400_rdata.obj`;
  **955 carry a COL pointer at `word[-4]`** (into `0x821c….0x8220….`), 86 abut
  a preceding vtable, 325 have none. My first pass looked for the COL in the
  wrong address window (`0x82c…`) and reported 0/1,280.
* Sweeping `0x821c0000..0x82200000` for COL-shaped records
  (`sig==0`, `pTypeDescriptor` into `.rdata`, `pClassDescriptor` back into the
  COL region) finds **2,220 COLs / 1,354 distinct type descriptors**.

The chain then dies at the type descriptor:

* **1 of 1,280 runs** resolves to a `.?A[VU]` class-name string — and it is an
  XDK class (`.?AUID3DXInclude@@`). The only other names recoverable anywhere in
  the chain are `DxShader`, `DxShaderBuffer`, `DxShaderInclude` — all D3D static
  library, all compiled into retail from a vendor lib that kept its RTTI.
* 1,415 `.?AV<Class>@@` / `.?AU<Class>@@` strings **do** survive in `.rdata`
  (`0x82c52538..0x82c9b458`, and they are genuinely Milo classes — `FilePath`,
  `String`, `TextStream`, `ObjRef`, `ObjDirPtr<ObjectDir>`), but they are
  **orphans**: sweeping every 32-bit word in the image for a pointer landing at
  `name_va + δ` for δ ∈ [−24, +8) finds a total of **~20 hits across all 1,415
  strings**. Nothing references them.

So retail's RTTI type descriptors were emitted and then had their contents
stripped, leaving live COLs pointing at zeroed records plus an unreferenced
string pool. **The RTTI-anchored identity channel is closed. Do not re-open
it** — this is why `vtable_global.py` had to anchor on already-mapped names.

A corollary worth *not* over-reading: our build's cflags include `/GR`
(`config/45410914/config.json`) and our objs emit thousands of `??_R0..??_R4`
symbols retail does not have. That is inherited from DC3 and CLAUDE.md marks it
as *assumed* for RB3, not verified. It cannot simply be flipped to `/GR-`:
the tree contains **761 `dynamic_cast`/`typeid` uses**, which do not compile
without RTTI. Flagged, not actioned.

## 4. What the vtable channel is still worth: 796 of the pool

Even without RTTI, the retail vtables are a large, addressable slice of the pool:

```
1,280 target vtable runs           13,291 slots
                                    3,445 anonymous fn_ slots
                                    2,007 distinct anonymous VAs
                                    1,076 of those > 68 B (none strict)
                                      796 also in-scope  <-- 12% of the pool
```

And the aligner is the bottleneck, not the evidence. Re-run at HEAD:

| tool | result |
|---|---|
| `vtable_global.py` | 1,969 distinct base vtables → **matched_vt = 40**, ambig 15, candidates 13, gate live **9** |
| `vtable_1anchor.py` | matched_vt = 3, candidates 5, gate live **3** |

**40 of 1,969 base vtables placed.** Widening that is the concrete lever, and it
is dispatched as a separate work item (§7).

## 5. First channel: 12 vtable-anchored names → +7 strict, 0 losses

The 12 gated candidates the two vtable passes produce at HEAD, applied as a
`target_symbol_map.json` fragment (branch `laneAQ-1`, commit `9d2f6fd1`).

| | | |
|---|--:|---|
| `??_EBandList@@WBEE@AAPAXI@Z` | **100.0** | 8 B |
| `??_EBandList@@WBFA@AAPAXI@Z` | **100.0** | 8 B |
| `??_EBandList@@WBFE@AAPAXI@Z` | **100.0** | 8 B |
| `??_EFileMergerOrganizer@@WCI@AAPAXI@Z` | **100.0** | 8 B |
| `?Exit@BandCharacter@@UAAXXZ` | **100.0** | 4 B |
| `?SetPlayback@GameMicManager@@UAAX_N@Z` | 97.14 | 140 B |
| `?FailedLoading@FileMergerOrganizer@@UAAXPAVLoader@@@Z` | 95.00 | 256 B |
| `?SaveFixed@TourProgress@@UBAXAAVFixedSizeSaveableStream@@@Z` | 86.15 | 208 B |
| `?Poll@BandCharacter@@UAAXXZ` | 65.95 | 1000 B |
| `?Poll@WorldDir@@UAAXXZ` | 65.92 | 372 B |
| `?Handle@BandList@@UAA?AVDataNode@@PAVDataArray@@_N@Z` | 61.61 | 688 B |
| `?Enter@RndDir@@UAAXXZ` | 2.35 | 204 B |

**Measured A/B: 36,659 → 36,666 = +7 strict, 0 losses**, stable across three
further identical rebuilds. (I first reported +5 against a contaminated control
— see §6.)

★ **The shape of that table is the lane's central result, in miniature.**
*Every* flip to strict is 4–8 bytes — thunks. **Every member of the actual pool
(>68 B) pairs sub-100.** Identity does not produce matches for this pool; it
produces *scored body-port targets* where there were unpairable 0 %s. Two of
the seven land in the 95–99 band (cheap follow-on source work), three in the
60–70 band, one at 2.35 %.

### 5.1 A free side-effect: two over-covering `.text` pins

Two of the twelve land at a VA pinned to a TU that cannot own them, and both
score low — the signature of an over-covering pin rather than a bad name:

| VA | name | evidence | pinned to |
|---|---|---|---|
| `0x824cca40` | `?Poll@WorldDir@@UAAXXZ` | `??_7WorldDir@@6BRndPollable@@@` **slot 0**, 3 agreeing anchors | `MidiSynth.cpp` `0x824cbfe0..0x824cd730` |
| `0x82404ea8` | `?Enter@RndDir@@UAAXXZ` | `??_7PitchArrow@@6BRndPollable@@@` slot 1 (inherited, not overridden), 3 anchors | `Anim.cpp` `0x8240466c..0x82405974` |

Both names are `Dir.cpp` members and both VAs sit inside a *neighbour's* span —
consistent with `Dir.cpp`'s own `.text` never being pinned and the adjacent
units over-covering it. Handed to the splits owner; **not repaired here**
(splits is a single-owner file). Evidence rows in
`/home/free/tmp/aq_vtg/global_evidence.txt`.

Note this is itself an argument for the vtable channel: it produced a *splits*
defect for free, from content evidence, in a place where no byte-identity or
adjacency heuristic was looking.

## 6. ★ A claim of mine, REFUTED mid-lane: the "±2 rebuild nondeterminism"

I measured baseline 36,659, then reverted the map to byte-identical content and
measured **36,661**, and wrote this up as "re-splitting is nondeterministic by
±2 — match SPLIT cycles, not just builds". **That was wrong.** Three further
identical rebuilds held at exactly the treated value, and the residual +2
vanished the moment I also ran `git checkout -- config/45410914/symbols.txt`
before the control build:

| state | strict |
|---|--:|
| baseline | 36,659 |
| map reverted, `symbols.txt` **not** reverted | 36,661 ← contaminated |
| map reverted **and `symbols.txt` reverted** | **36,659** ← true control |
| fragment applied | 36,666 |
| 3 further identical rebuilds | 36,666 ×3 (exactly stable) |

The cause is the already-recorded trap `symbols.txt` **is both a dtk input and
a regenerated output**: leg B's names persisted into the control through it, so
the control silently retained part of the treatment. My lane's price worker hit
the identical artifact independently and diagnosed it the same way.

> **Rule (a sharpening of the existing one, not a new one): a map A/B must
> revert `config/45410914/symbols.txt` on BOTH legs, not just the map.**
> Otherwise the control is contaminated and the measured delta is understated
> (here, +5 instead of the true +7). Never commit `symbols.txt`.

Plain rebuilds on this tree are **exactly deterministic**. There is no ±2 noise
floor; I invented one and then measured it away.

## 7. Channel results

Three channels were worked in parallel, each in its own worktree, each with a
held-out or control leg. All numbers measured.

### 7.1 Per-unit exact masked-byte equality — the big one, and it **refutes "drained"**

Prior lanes concluded byte-identity homing was drained
(`docs/plans/lane-al-autocarve-2026-07-26.md`: 4 actionable VAs tree-wide). That
measurement was **global**. Measured **per unit**, it is emphatically not
drained — a body that collides tree-wide (`?SetType@X@@`, `PropSync<T>`,
`_M_insert_overflow_aux`, `ObjRefConcrete<T>::Load` — the ICF-prone shapes that
make global homing hopeless) is still **unique inside its own unit**.

Funnel over the 5,055 pinned arm, with the **measured** flip rate per class:

| class | count | measured flip → 100 | median measured % |
|---|--:|--:|--:|
| **EXACT_UNIQUE** (unique masked-byte class in unit) | **124** | **100 %** (115/115) | 100.0 |
| **EXACT_AMBIG** (several candidates, same bytes) | **161** | **100 %** (153/153) | 100.0 |
| WD1 (1 differing masked word) | 44 | 34.3 % | 100.0 |
| WD2 | 72 | 13.1 % | 99.9 |
| WD3 | 58 | 13.9 % | 97.9 |
| WD4+ | 2,356 | 2.5 % | 56.3 |
| NEARSIZE (no same-size candidate; one within ±10 %) | 1,782 | 1.0 % | 68.5 |
| **SOURCE_MISSING** (no same-size *and* no near-size candidate) | **395** | **0 %** (0/249) | 59.6 |
| NO_TARGET_ASM (`resolve_unit` basename artifact, not a real class) | 63 | untested | — |

**Why these sat at 0 % through every prior wave** — `is_funclet_like()` gates
**both** sides of `pair_funclets_by_bytes` (`objdiff-core/src/diff/mod.rs`
:1423,:1438). A target `fn_<8hex>` can only byte-pair with a base symbol that is
*also* `fn_`/`__unwind$N`/`__catch$N`/`??__E`/`??__F`. A real mangled base name
like `?SetType@BandList@@UAAXVSymbol@@@Z` is unreachable by that path. A
`target_symbol_map.json` entry is the **only** channel that exists for them.

**"Ambiguous byte equality is a coin flip" — refuted for scoring, confirmed for
identity.** All 153 EXACT_AMBIG entries read exactly 100.0. laneAN's 36.3 %
pass-2 precision measures *which name is right*, not whether the pair scores:
under `functionRelocDiffs=none` every member of a masked-byte-equal class scores
100 regardless of assignment. Byte-true match, arbitrary name — the existing
`_bijection_arbitrary` doctrine, and it must be disclosed as such.

### 7.2 Order-anchored interval bijection — real, small, one-shot

`scripts/harvest/size_order_automap.py` already implements interval alignment,
but its anchors come from **one source: reloc-masked byte identity** — blind by
construction exactly where this lane aims. `scripts/harvest/order_anchored_bijection.py`
(new, branch `laneAQ-order`) reuses its parsers and adds **name anchors**, a hard
`k == s` forcing rule, a symmetric > 68 B projection of both sequences, and
leave-one-out calibration.

```
5,060  demand (independently reproducing the 5,055)
3,962  (78.3%) inside a BOUNDED gap between two named anchors
  549           in a FORCED gap (k demand == k supply):  k=1 274 | k=2 125 | k>=3 150
3,664           unforced
```

**Raw order-forcing is mostly wrong** and says so: of the 274 k=1 forced, 198
(**72 %**) are size-CONTRADICTORY (>8 B). Base-side oversupply makes `k == s` a
frequent coincidence. Held-out precision (leave-one-out over 6,956 named
interior anchors, 2,252 recovered):

| gate | precision |
|---|--:|
| all forced | 94.7 % |
| k=1 | 97.0 % |
| k=1 + size exact | 99.5 % |
| **k=1 + size + reloc exact** | **99.9 %** |
| shipped tier (any k + size + reloc exact) | **99.8 %** |
| k≥2 | 81.5 % |

**A/B: +37 strict from 62 entries, 0 losses.** The **control leg is what makes
the gate credible**: the 320 candidates the gate *rejects*, applied under the
same protocol, give **+5 strict** and a band of `90-99 ×47 · 50-90 ×151 ·
0-50 ×115` — i.e. 115 junk-band fake identities bought 5 matches. Inside the
gate: 60 % strict hit rate, nothing below 64.5 %, zero junk band.

**Fixpoint in one round** (re-running with the 62 as anchors yields 0). Not a
flywheel; it refills only when other lanes add names or objs move.

### 7.3 Two independent channels, cross-validated

§7.1 (masked-byte equality) and §7.2 (monotone order between name anchors) are
methodologically unrelated. They overlap on **28 addresses**:

* **20 agree exactly** — independent corroboration.
* **8 disagree, and every one is a known ICF-degenerate shape**: `??_G` scalar
  deleting destructors (`ObjPtrList<Object>`↔`ColorKeys`, `ObjPtr<Sequence>`↔
  `BandList`, `BoolKeys`↔`ObjectKeys`, `ChannelData`↔`MasterAudio`) and DataFunc
  byte-twins (`DataCeil`↔`DataFloor`, `DataSubEq`↔`DataDivideEq`).

Both members of every disagreement score 100 either way. **The disagreement set
is exactly the `_bijection_arbitrary` population and nothing else** — which is
about as good a cross-validation as this project can produce.

A third corroboration arrived for free: 18 of the composed fragment's entries
had *already* been derived independently by laneAO between this lane's start and
its landing. **18/18 agreed exactly, 0 contradictions.**

### 7.4 The fuzzy-percentage trap (measured, and it kills a tempting shortcut)

The 249 tested SOURCE_MISSING entries — where by construction **no** same-size
or near-size candidate exists, so the name is definitionally wrong — measured
**mean 53.7 % / median 59.6 % / max 89.95 %**. A deliberately wrong name reads
~55 % because MSVC PPC prologue/epilogue/`__savegprlr` boilerplate is shared
across all functions.

> **Nothing below 90 % is evidence of identity.** Usefully, zero SOURCE_MISSING
> entries reached ≥90, so ≥90 does retain signal.

And the natural proxy fails outright: reloc-masked positional word agreement
predicts the measured percentage with MAE **15.9 pts**, biased low by 10.7 pts,
and **does not predict the flip event at all** —
`?SetFaceType@CharacterCreatorPanel@@QAAXVSymbol@@@Z` was predicted 63.9 % and
read exactly **100.0**. **Only exact masked-byte equality predicts a flip.** Any
lane budgeting from a proxy percentage will be wrong.

## 8. ★ THE IDENTITY FUNNEL — measured counts

```
6,542   the pool (anon fn_, >68 B, in-scope, not strict)
  1,487   in auto_03 carve spans -- no compiled TU, unpairable until pinned;
          against a global index of all 1,024 objs: 28 exact-unique,
          366 exact-ambiguous, 1,093 no match. CONDITIONAL handoff to the
          geometric-attribution lane, NOT an identity deliverable.
  5,055   in already-pinned units  <-- the identity problem proper
     285     EXACT masked-byte equality in unit ......... 100% flip rate
     174     WD1-3 (1-3 differing masked words) ......... 13-34% flip rate
      62     order-forced + size-exact + reloc-exact .... 99.8% held-out precision
     148     retail vtable slot, multi-anchor aligned ... 93-100% held-out precision
   ~4,660    body divergence -- NO map entry can ever help
     395       of which provably SOURCE-MISSING (no candidate of that size at all)
```

**Evidence-backed identities established: 432** (12 vtable-anchored by the old
aligner, 264 exact masked-byte, 33 order-anchored, 141 from the repaired
multi-anchor vtable aligner; de-duplicated). **Hypotheses discarded: ~2,190** —
the relaxed tiers, which buy +57 strict combined at the cost of planting an
arbitrary name on ~1,935 VAs. **Not funded.**

Of the 432, **406 are > 68 B** — genuine pool members, not funclet crumbs.

Percentage bands are given in §9 for the full landed set.

### 8.1 The honest ceiling

**~4,660 of the 5,055 pinned pool (92 %) is body divergence** — supply is not
the constraint (across all 538 units exactly **one**, `Common_Xbox`, is short a
name, by 1). Names are abundant; *correct bodies* are not. This corroborates
laneAL's "source problem wearing an attribution costume", now with a
per-function measurement behind it rather than an inference.

A follow-on source lane buys **~81 genuine 99-100 near-misses**, not the 345 the
raw 90-99 band suggests — only 63 of the 100 measured 99-100 hits came from
evidenced tiers; the rest are argmax guesses sitting at a coincidental score.

### 7.5 The vtable aligner was **buggy**, not saturated — 40 → 318 placed

The "40 of 1,969" ceiling in §4 turned out to be mostly a defect. Failure
histogram over the 1,969 base `??_7` vtables vs 1,281 target runs:

| bucket | count | share |
|---|--:|--:|
| ANCHOR_0 (no mapped anchor) | 125 | 6.3 % |
| ANCHOR_1 (one anchor) | 405 | 20.6 % |
| **NO_RUN_FOR_ANCHOR** | **791** | **40.2 %** |
| NO_CONSISTENT_ALIGN | 593 | 30.1 % |
| AMBIG | 15 | 0.8 % |
| MATCHED, nothing new | 28 | 1.4 % |
| MATCHED with yield | 12 | 0.6 % |

**The 40 % bucket is a bug.** `vtable_global.py:355` seeds candidate slot offsets
from only `first_i = min(anchors)`, so a vtable dies whenever that *one* name is
absent from every run — and **618 of the 791 have another anchor that is
present**. Second defect: out-of-range slots were counted as anchor
*disagreements* (1,145 such slots across 151 otherwise-clean vtables); a
subrange overlap is not a contradiction.

`scripts/harvest/vtable_multianchor.py` fixes both and adds a tier C that
tolerates ICF-explainable disagreements. Placement **40 → 318**. Held-out
precision, 5 × N=500 randomly hidden map entries:

| tier | precision |
|---|--:|
| A — strict, full overlap | **100.0 %** (44/44) |
| B — strict, subrange | 89.5 % (102/114) |
| C — ICF-tolerant | **93.3 %** (265/284) |

Every inspected tier-B/C error is an **ICF fold**, not a misalignment
(`RndAmbientOcclusion`↔`RndTexBlender` `ClassName`, `Parallel`↔`SerialGroupSeq`
`Save`) — both names genuinely live at the folded VA. Tier C beats tier B on
precision *and* has 3× the recall. **Priced honestly: ~6 of the 86 tier-C
entries carry an ICF-twin name** and will resurface as stubbornly-0 % body-port
targets. Not a regression (they were unpairable before), but it is map
pollution and is disclosed here.

**Three non-map anchor types measured and found weak on the 530 vtables with
< 2 anchors — do not re-hunt:** run-length equality places **3/530**;
`??_E`/`??_G` slot-0 + length places **5/530** (474 base vtables have a dtor at
slot 0 and so does nearly every run — no discriminating power); owning-unit
adjacency majority-vote places **30/530**.

**A/B: +19 strict, 0 losses** (conservative — present in both treatment legs,
absent from both baseline legs; the two treatment legs were set-identical).
Channel verdict: **a re-run flywheel, not a one-shot** — every landed name
becomes an anchor. Re-run after each map-growing wave.

## 9. Landed

Composed fragment of **432 entries** across all four channels, de-duplicated
(18 dropped as already-derived-identically by laneAO, 7 as internal overlaps),
applied to `scripts/target_symbol_map.json` at main `02c72524`:

**36,705 → 36,981 = +276 strict, 0 losses.** All 276 gains are exactly fragment
names; **zero gains in units the diff does not touch** — no stale-obj phantom,
no ripple. `symbols.txt` reverted on both legs.

Final band distribution of all 432 named functions:

| band | all | > 68 B |
|---|--:|--:|
| **100** | 276 | 258 |
| 99-100 | 14 | 14 |
| 90-99 | 20 | 20 |
| 50-90 | 73 | 71 |
| 0-50 | 49 | 43 |
| unpaired | **0** | **0** |
| **total** | **432** | **406** |

**406 of the 432 are genuine pool members (> 68 B), and 156 of them are newly
scored rather than newly matched** — unpairable 0 %s converted into measurable
body-port targets, 34 of those already ≥ 90 %. That conversion is the lane's
second product and it does not show up in the strict count at all.

The 34 newly-paired functions at ≥ 90 % are a priced body-port worklist; the 14
at ≥ 99 are the cheapest, led by `MetaPerformer::PopulateSoloPlayerScore`
(99.979), `QuestManager::IsQuestAvailable` (99.975),
`ContextWrapper::SetCallbackObject` (99.964), `PatchDir::CacheRenderedTex`
(99.951) and six `BandProfile` methods.

### 9.1 A note on the ±2 flippers

`default/CameraShot ?Disable@CamShot@@QAAX_NH@Z` and the `default/CharLipSync`
`??$__fill_n@…_Bit_iter…` instantiation **do** flip between otherwise-identical
clean rebuilds — independently reproduced by two workers at two different base
commits (36,705 / 36,707). They are shape-degenerate symbols of exactly the kind
objdiff's ambiguous byte-signature passes zip greedily. This is separate from,
and was masked by, the `symbols.txt` contamination of §6.

> **Practical consequence: never count a delta of ≤ ±2 without also checking
> that every gain is causally attributable to your change.** Every number in
> this document passes that check — the "gains NOT in fragment" count is 0 for
> all four channels.

## 10. Recommendations

* **Do not fund the relaxed tiers.** The control legs price them exactly: +5
  strict for 115 junk-band fake identities (order channel), +52 strict for
  ~1,820 arbitrary names (byte channel). Both inflate the map's identity claims
  to buy a rounding error.
* **The per-unit exact channel is re-runnable and should be re-run after every
  wave that moves objs** — it is per-unit, so it refills whenever a body flips.
  It is *not* the drained global-homing channel.
* **RTTI is closed** (§3). Do not re-open.
* **The vtable aligner was the open lever and is now half-open**: the 40/1,969
  ceiling was a first-anchor-only-seeding bug (§7.5), now 318/1,969. It is a
  **flywheel** — re-run it after every map-growing wave, since each landed name
  becomes an anchor.
* The remaining ~4,660 belong to a source lane, not an identity lane.

## 11. Reproducing

```bash
python3 scripts/harvest/autocarve_funnel.py --worktree .
python3 scripts/harvest/vtable_global.py  . ~/tmp/aq_vtg
python3 scripts/harvest/vtable_1anchor.py . ~/tmp/aq_vt1
python3 scripts/harvest/order_anchored_bijection.py     # branch laneAQ-order
```

Artifacts: `/home/free/tmp/laneAQ/` (per-class fragments + predicted-vs-measured
tables), `/home/free/tmp/laneAQ_funnel.json` (full 5,055-row classification),
`/home/free/tmp/aq_supply.json`, `/home/free/tmp/aq_land_frag.json`.

## 8. Reproducing

```bash
python3 scripts/harvest/autocarve_funnel.py --worktree .
python3 scripts/harvest/vtable_global.py  . ~/tmp/aq_vtg
python3 scripts/harvest/vtable_1anchor.py . ~/tmp/aq_vt1
```
