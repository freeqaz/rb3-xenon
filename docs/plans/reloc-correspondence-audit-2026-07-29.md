# laneBH — reloc-correspondence audit: how much of the strict count is REPRODUCTION vs SHAPE (2026-07-29)

Inputs: `docs/plans/gapfill-pricing-and-nearmiss-open-2026-07-29.md` (laneBE),
commit `2e59f8b1` (laneTIGHTGAP, landed +109), `scripts/harvest/live_units.py`,
`scripts/harvest/eh_ground_truth.py`.
Worktree `/home/free/tmp/wt-laneBH-1` (branch `laneBH-1`, from main `493eb60c`),
full `./tools/ninja-locked` run before any obj-derived measurement.
Baseline in-worktree: **39,520** `matched_functions` (main reads 39,522; the
2-function delta is the same one laneBE saw and does not affect any ratio here).
Tool: `scripts/harvest/reloc_correspondence.py`. Scratch: `/home/free/tmp/laneBH_*`.

## 0. TL;DR

1. **The count is substantially sound.** Of 39,520 functions at 100.0
   `match_percent_normalized`, reported as a **band** between the permissive and
   the conservative reading of the weakest oracle (§3.5):

   | | permissive | **conservative** |
   |---|--:|--:|
   | **positively evidenced** (corresponding + no-relocs) | **65.5%** | **43.8%** |
   | **DIVERGENT** (≥1 reloc proven to point elsewhere) | **5.2%** | **2.7%** |
   | UNDECIDABLE | 27.7% | 51.8% |
   | unpaired | 1.6% | 1.6% |

   Both ends say the same thing: **divergence is a few percent, not a
   substantial fraction**, and the residue is *unobservable*, not suspect.
2. ★ **Raw-diff was the wrong instrument and laneBE's "0 of 528 raw-safe" figure
   should not be read as "0 of 528 real".** Rebuilt on symbolic correspondence,
   the same population splits three ways, and the *tree-wide* divergence rate is
   5.2%, not 100%.
3. ★★ **`orig/45410914/band.exe` is the decompressed retail PE and it is a
   ground-truth oracle nobody in this project has been using.** It turns "does
   our symbol correspond to retail VA V" from an inference about
   `target_symbol_map.json` into a byte comparison. It is what makes this audit
   possible and it should be wired into other tools.
4. **laneTIGHTGAP's landed +109: 29.0% corresponding / 29.9% divergent / 39.3%
   undecidable — a 5.7× worse divergence rate than the tree** (conservative
   reading: 15.9 / 19.6 / 62.6, **7.3×** — the verdict is invariant to oracle
   strictness). Its 105 pairable credits rest on only **64 distinct base
   symbols**. Recommendation: **STAND (do not revert), reclassify in the ledger,
   and gate the channel going forward.** Reasoning in §4.
5. **By-product worth more than the audit: 109 named bodies ≥128 B carry
   content-proven wrong constants/strings** — real behavioural bugs that
   normalized diff structurally cannot see. Examples: `RndDrawable::Save` writes
   revision **4** where retail writes **3**; `AAFilter::calculateCoeffs` uses
   **π** where retail uses **2π**; `DirLoader::FixClassName` has
   TexRenderer↔CompositeTexture and View↔Group **swapped**;
   `RndAmbientOcclusion::BuildTrees` has `kQualityLUT = {256,1024,0,2}` where
   retail has `{300,150,2,0}` and FLT_MAX/-FLT_MAX inverted. Full list:
   `/home/free/tmp/laneBH_realbugs.json`. **This is a fresh, ranked, oracle-free
   near-miss queue that no existing scanner produces.**

## 1. The instrument

`scripts/harvest/reloc_correspondence.py` (per-symbol + `--census`). Full method
and limits are in its docstring; the short version:

For every function at 100.0 normalized, take its relocation list from the dtk
target obj and from the paired base symbol in our compiled obj, align by
(offset, type), and decide each slot with four oracles in strength order:

| # | oracle | what it compares | slots decided |
|--:|---|---|--:|
| 0 | **name identity** | the renamer's mangled name at that retail VA == our symbol's name | 55,721 |
| 1 | **content** | our symbol's bytes vs **`band.exe` bytes at the target VA**, masked at our own relocation offsets | 21,600 |
| 2 | **consistency** | the modal target VA that ≥2 **distinct functions** independently bind this base symbol to | 53,863 |
| 3 | **map** | `target_symbol_map.json` / `symbols.txt` name→VA | 197 |
| — | undecided | none of the above speaks | 45,908 |

Function verdict: DIVERGENT if any slot disagrees; else UNRESOLVABLE if any slot
is undecided; else CORRESPONDING; NO_RELOCS if there are none.

Pairing reproduces objdiff's own rules (name pairing, then
`pair_funclets_by_bytes` masked-signature pairing, then the looser union-masked
equality that its pass-3 fallback can land on) and, when several base symbols are
admissible, takes the **most favourable** verdict — so a DIVERGENT reading is
never an artifact of objdiff's arbitrary tie-break.

### Four instrument defects found and fixed during calibration

Each one moved the headline by several points; they are recorded because any
re-implementation will hit them.

| defect | symptom | fix | effect |
|---|---|---|--:|
| **base funclet sizes** | MSVC packs many `__unwind$NNN` into ONE `.text` section; "rest of section" as the size makes a 40 B funclet look 600 B and destroys every signature comparison | derive size as the gap to the next symbol | 16 spurious NO_BASE_PAIR → 0 |
| **ICF over-rejection** | `target_symbol_map.json` keeps ONE name per folded VA; every `lwz r3,N(r3); blr` accessor and same-shape template read DIVERGENT | content oracle (folded bytes ARE our bytes ⇒ CORRESPOND), plus ICF-capability suppression on the map path | 4,778 → 1,361 raw divergence claims |
| **content test on unmatched callees** | a byte difference at the callee's address means "we have not ported the callee", not "the pointer is wrong" | apply the code-content test only when the callee is itself a 100% match | false 20.4% → 5.5% |
| ★ **self-corroborating consistency** | a guard word referenced 3× inside ONE 88 B accessor counted as 3 independent observations — a tautology | count support over **distinct functions**, ≥2 required | CORRESPONDING 73.1% → **61.3%** |
| ★ **over-subscribed consistency support** | the "distinct functions" backing a binding can themselves all be `fn_<VA>` funclets that reached 100% by the same many-to-one masked pairing — 33 of them "supported" one `DirLoader` local guard | `--strict-consistency`: require ≥2 **named, non-funclet** supporters | CORRESPONDING 61.3% → **39.6%** (reported as a band, §3.5) |
| **vacuous zero comparison** | an all-zero data object matches the image at almost any address | refuse the content oracle when our bytes are all zero | 1,977 slots un-certified |

## 2. Controls — and what would have falsified the classifier

### 2.1 Falsification control (the anti-tautology test)

**What would have falsified it:** if functions it calls CORRESPONDING kept that
verdict after their pointers were rewritten to point somewhere else, the tool
would be measuring nothing. `scripts/harvest/reloc_correspondence_selftest.py` does
exactly that: it takes 400 CORRESPONDING functions and rewrites **every** target
relocation to the anonymous label δ bytes further on, which breaks all four
oracles simultaneously.

| δ | DIVERGENT | UNRESOLVABLE | still CORRESPONDING |
|---|--:|--:|--:|
| 0x40 | 306 | 75 | **19 (4.8%)** |
| 0x2000 | 306 | 75 | 19 (4.8%) |

**95.2% flip away from CORRESPONDING, at both displacements.** The residual is the intrinsic floor —
displacing a pointer inside a run of byte-identical `__unwind$` funclets or
repeated 4-byte constants lands on a genuine twin. The test is not a tautology:
it produced the falsifying verdict, in bulk.

### 2.2 Positive control — laneBODYPORT `559645e9` hand-ports

23 functions hand-ported from the oracle to 100% in the most recent body-port
landing (Profile/WorldCrowd `Handle`, Rnd{Drawable,Flare,Generator,TransAnim,
CubeTex} `Save`, PanelDir/NgSpotlightDrawer `SetType`, UISlider/UIList
`Handle`/`SyncProperty`, plus their thunks).

**12 CORRESPONDING, 8 UNRESOLVABLE, 3 DIVERGENT.** Every one of the 3 was
opened by hand:

* `RndDrawable::Save` — our `gSaveRev_RndDrawable` = **4**, retail's word at that
  VA = **3**. Not a tool error: a **real** value divergence that the
  SAVE-REVISION-FROM-`.data` lever cannot see, because the load is
  reloc-masked. (The lever got the *shape* right and left the *value*
  unverified.)
* `UIList::Handle` — our `"allow_highlight"` vs retail `"set_draw_manually…"` at
  that VA. **Real** (consistent with that commit's own "retail carries fewer
  arms" finding).
* `RndTransAnim::Save$4…` — a **12-byte adjustor thunk**, i.e. the degenerate
  stratum where reloc-masked equality is near-vacuous (a 12 B adjustor body is
  shared by ~1,673 symbols). Correctly flagged, but low information.

**Zero false CORRESPONDING; ≤1 arguable false DIVERGENT in 23 (≤4%).**

### 2.3 Negative control — the `StaticClassName`/`Type()` family

The known shape-only family (one identical 88-byte body, the string operand the
only discriminator). 423 members at 100%:

**395 UNRESOLVABLE (93.4%), 24 DIVERGENT, 3 CORRESPONDING (0.7%).**

99.3% are *not* certified. That is the correct answer — their discriminating
operands are `.bss` guards and `.bss` `Symbol` slots, which have **no bytes in
either image**, so no oracle can decide them. ★ Note this is the *post-fix*
result: before the distinct-function fix these read 400/423 CORRESPONDING, which
is precisely the tautology the fix removed.

### 2.4 Negative control — laneBE's guard-clear channel

laneBE's +560 was never landed, so those functions are not in this census.
The **same mechanism** is present in laneTIGHTGAP's landed set and is shown
directly in §4: in `default/DataNode`, our single `??__EgDataArrayConditional`
is credited against **six** different retail initializers, each constructing a
*different* static object (`lbl_82E03B80`, `lbl_82E050A4`, `lbl_82E054A0`,
`lbl_82E03904`, …). The consistency oracle certifies the modal one and calls the
other five DIVERGENT. That is laneBE's finding, reproduced symbolically and
counted.

## 3. The census

39,520 functions at 100.0 `match_percent_normalized`, 846 live units
(`objdiff.json` via `live_units.py`; no globbing, no stale `auto_03_*`).

| verdict | n | share |
|---|--:|--:|
| **CORRESPONDING** | **24,212** | **61.3%** |
| UNRESOLVABLE | 10,965 | 27.7% |
| **DIVERGENT** | **2,036** | **5.2%** |
| NO_RELOCS (nothing to get wrong) | 1,668 | 4.2% |
| NO_BASE_PAIR | 412 | 1.0% |
| SHAPE_MISMATCH (reloc shapes differ) | 226 | 0.6% |
| NO_TARGET_SYM | 1 | 0.0% |

`SHAPE_MISMATCH` was hand-diagnosed after the census and is **not** evidence of
divergence: dtk emits a `REL14` relocation for an *intra-function conditional
branch* that MSVC resolves internally with no relocation at all (verified on
`RndAnimFilter::ListAnimChildren` and `ObjList<HamCamShot::Target>::operator=`).
The tool now filters self-targeted `REL14`, so a re-run redistributes most of
this bucket into the adjudicated verdicts; the numbers below are the pre-filter
run and therefore *understate* both CORRESPONDING and DIVERGENT by ≤0.6%.
`NO_BASE_PAIR` (412) is where our obj simply has no admissible partner for the
target symbol — objdiff reached 100% through a pass this tool does not
reimplement (mostly the cross-unit global reconcile pass).

**★ Headline decomposition: A = 25,880 corresponding (65.5%) /
B = 2,036 divergent (5.2%) / C = 10,965 undecidable (27.7%) /
638 unpaired (1.6%).**

### 3.1 By size band

| band | CORRESP | DIVERG | UNRESOLV | NO_RELOCS | other | total | div% |
|---|--:|--:|--:|--:|--:|--:|--:|
| ≤16 B (thunks/adjustors) | 1,847 | 337 | 547 | 558 | 2 | 3,291 | 10.2% |
| 17–48 B | 13,664 | 962 | 6,150 | 499 | 276 | 21,551 | 4.5% |
| 49–128 B | 5,374 | 495 | 1,862 | 483 | 259 | 8,473 | 5.8% |
| 129–512 B | 3,168 | 215 | 1,981 | 128 | 101 | 5,593 | 3.8% |
| >512 B | 159 | 27 | 425 | 0 | 1 | 612 | 4.4% |

★ The ≤16 B stratum is the one the mission flagged as near-vacuous, and it is
also the worst: **10.2% divergent, 2.2× the tree rate.** These are adjustor
thunks and 12-byte forwarders whose reloc-masked bodies are shared by >1,600
symbols; their 100% carries essentially no information either way.

### 3.2 By unit tier

| tier | CORRESP | DIVERG | UNRESOLV | NO_RELOCS | total | div% |
|---|--:|--:|--:|--:|--:|--:|
| engine | 14,933 | 1,340 | 6,443 | 1,059 | 24,269 | 5.5% |
| game | 9,000 | 689 | 4,412 | 600 | 14,844 | 4.6% |
| network | 279 | 7 | 110 | 9 | 407 | 1.7% |

The priority tier (game) is **cleaner** than the engine. Nothing here says the
game-code number is inflated relative to the engine's.

### 3.3 Funclet vs body

| | CORRESP | DIVERG | UNRESOLV | NO_RELOCS | other | total |
|---|--:|--:|--:|--:|--:|--:|
| funclet-like (`fn_`, `__unwind$`, `??__E/F`) | 13,543 | 955 | 6,138 | 0 | 314 | 20,950 |
| named body | 10,669 | 1,081 | 4,827 | 1,668 | 325 | 18,570 |

Divergence is **not** concentrated in funclets (4.6% vs 5.8%). The named-body
divergences are the interesting half — they are real source defects (§5), not
attribution noise.

### 3.4 What the divergences are, by pointed-at object

| base symbol class | diverging slots |
|---|--:|
| function / non-literal data symbol | 3,576 |
| string literal `??_C@` | 178 |
| vtable `??_7` | 122 |
| float constant `__real@` / `__xmm@` | 56 |
| static init/dtor thunk `??__E`/`??__F` | 20 |
| static-init guard `??_B` | 11 |

Unit hotspots: `RockCentral` 56, `BandCharDesc` 52, `BandCharacter` 40,
`SessionMgr` 38, `Campaign` 33, `VocalTrackDir` 33, `StreakMeter` 32,
`TrackPanelDir` 32, `SaveLoadManager` 31, `AccomplishmentPanel` 27.

### 3.5 Sensitivity — the conservative bound

The consistency oracle's support count is the weakest link: a binding "supported
by N distinct functions" is only N independent observations if those functions
are themselves independent. They frequently are not — in `DirLoader`, 33
`fn_<VA>` EH funclets all pair against one local guard through exactly the
many-to-one masked pairing this audit is investigating. `--strict-consistency`
demands ≥2 **named, non-funclet** supporters instead, and demotes everything else
to UNDECIDED (in both directions — it also withdraws consistency-based DIVERGENT
verdicts).

| verdict | permissive | conservative | delta |
|---|--:|--:|--:|
| CORRESPONDING | 24,212 (61.3%) | 15,665 (39.6%) | −8,547 |
| UNRESOLVABLE | 10,965 (27.7%) | 20,484 (51.8%) | +9,519 |
| **DIVERGENT** | **2,036 (5.2%)** | **1,064 (2.7%)** | −972 |
| NO_RELOCS | 1,668 (4.2%) | 1,668 (4.2%) | 0 |
| unpaired | 639 (1.6%) | 639 (1.6%) | 0 |

★ The conservative reading is the one to quote if a single number is needed:
**43.8% proven reproduction, 2.7% proven shape, 51.8% unobservable.** The
permissive reading is the one to quote for "how much *could* be wrong":
**at most 5.2%.**

## 4. ★ The adjudication: laneTIGHTGAP's landed +109 (`2e59f8b1`)

Derived without an A/B: the commit's splits.txt diff yields **45 newly-claimed
VA regions / 36,632 B** (matching its own "landed 45 of 104 gaps"); the
currently-100% functions whose retail VA falls inside them number **107** —
which reproduces laneBE's independently-derived "107 of the +109 are in the
in-window tight-gap set" exactly.

| | tightgap-107 | tree baseline | ratio | strict: tightgap | strict: tree | ratio |
|---|--:|--:|--:|--:|--:|--:|
| CORRESPONDING | 31 (29.0%) | 61.3% | 0.47× | 17 (15.9%) | 39.6% | 0.40× |
| **DIVERGENT** | **32 (29.9%)** | **5.2%** | **5.7×** | **21 (19.6%)** | **2.7%** | **7.3×** |
| UNRESOLVABLE | 42 (39.3%) | 27.7% | 1.4× | 67 (62.6%) | 51.8% | 1.2× |
| NO_BASE_PAIR | 2 (1.9%) | 1.0% | — | 2 (1.9%) | 1.0% | — |

The verdict is **invariant to the oracle strictness**: on either reading the
tightgap set is ~6× the tree divergence rate and ~0.4× its correspondence rate.

Divergence evidence: 15 by the content oracle, 16 by consistency, 1 by map.
Divergent units: `DataNode` 12, `Voice` 4, `UsbMidiGuitar` 4, `BlockMgr` 2,
`FilePath` 2, `SkeletonClip` 2, `GemManager` 1, `PropKeys` 1.

**The over-subscription is visible symbolically:** the 105 pairable credits rest
on only **64 distinct base symbols** (1.64×). The worst offenders are exactly
the static-lifecycle thunks — `??__EgDataArrayConditional` credited 6×,
`??__EgLockPendingLists` 6×, `??__FgPendingVoices` 5×,
`??__FgDataArrayConditional` 4×, `??__EgDataVars` 4×, `??__EgPendingVoices` 4×.
Our TU emits ONE of each; retail has many, each initialising a *different*
object. Worked example (`default/BlockMgr`):

```
fn_82C3FC18  CORRESPONDING  base ??__EgReadTime   -> lbl_82DFBB88, calls ??0Timer   ✓
fn_82C3FC60  DIVERGENT      base ??__EgReadTime   -> fn_82C45AD8,  calls atexit     ✗
fn_82C3FC70  DIVERGENT      base ??__EgReadTime   -> fn_82C45B08,  calls atexit     ✗
```

The retail functions at `…C60`/`…C70` register a destructor with `atexit`; ours
constructs a `Timer`. Identical shape, different program.

### Recommendation: **STAND — do not revert `2e59f8b1`. Reclassify and gate.**

Grounds, in order:

1. **Reverting is net-negative on evidence.** It would delete 31 positively
   evidenced credits and 42 undecidable ones to remove 32 unevidenced ones.
2. **Magnitude.** 32 functions is **0.08%** of 39,522. Removing it does not
   change any decision that the headline number feeds.
3. **The splits claim and the scoring claim are different claims.** The commit
   asserts *geometry* — these retail bytes belong inside unit X's span. My
   measurement is about *which base symbol objdiff paired them with*. Reverting
   the split does not correct the pairing; it only removes the credit. The
   defect is in the scoring rule (many-to-one masked pairing), not in the
   splits.
4. **laneTIGHTGAP's own honesty filter worked.** It declined +79 raw matches and
   landed the filtered set; that discipline is why this reads 5.7× the tree rate
   and not 20×.

But the channel is confirmed **farmable** and should be closed, exactly as
laneBE recommended for the +560 sibling. Two concrete actions:

* **Ledger:** record the landed +109 as **≈31 evidenced / ≈42 undecidable /
  ≈32 shape-only**, so no future lane cites it as 109 reproductions.
* **Gate:** make `reloc_correspondence.py --census --va-ranges <claimed>` a
  required pre-landing check for any future gap-absorption or splits-fill
  channel. A channel whose divergence rate materially exceeds the tree's 5.2%
  is buying metric, not program. This is cheap (seconds on a filtered range,
  ~6 min tree-wide) and it is the gate laneBE asked for — but built on symbolic
  correspondence rather than raw diff, so it does not reject correct code.

For reference, laneBE's unlanded **+560** is *entirely* the `??__E`/`??__F` /
EH-funclet / guard-clear population (its own Q2 by-product established there are
zero real bodies in it), i.e. the same class that supplies 100% of the
tightgap-107's divergence. It should be expected to price **worse** than 29.9%,
and the existing CLOSE recommendation stands.

## 5. ★ By-product: 109 content-proven wrong constants/strings in named bodies

The content oracle finds source defects that normalized diff structurally cannot
see, because the wrong value lives in a reloc-masked operand. Ranked by size,
full list in `/home/free/tmp/laneBH_realbugs.json` (regenerate with `--census`
and filter `div_conf == "content"`, non-`fn_`, size ≥ 128):

| size | unit | function | proven wrong |
|--:|---|---|---|
| 2260 | DirLoader | `FixClassName` | `TexRenderer`↔`CompositeTexture`, `View`↔`Group`, `BandFx`/`WorldFx` — **the remap table is permuted** |
| 1040 | AmbientOcclusion | `RndAmbientOcclusion::BuildTrees` | `kQualityLUT` ours `{256,1024,0,2}` vs retail `{300,150,2,0}`; `FLT_MAX`/`-FLT_MAX` **inverted** |
| 3132 | UIList | `UIList::Handle` | `"allow_highlight"` vs retail `"set_draw_manually_controlled"` |
| 3972 | AccomplishmentPanel | `LaunchGoal` | `acc_multiplayersession` / `acc_createsetlist` / `acc_HMXrecommends` **permuted** |
| 536 | BaseMaterial | `BaseMaterial::Handle` | `allowed_next_pass` / `allowed_normal_map` / `is_default` all mis-aimed |
| 472 | AAFilter | `soundtouch::AAFilter::calculateCoeffs` | `__real@400921fb…` (π) where retail has `__real@401921fb…` (2π) |
| 2420 | CameraShot | `PropSync(CamShotFrame)` | `__real@42652ee1` vs retail `42652ee0` (RAD2DEG, 1 ULP) |
| 160 | Draw | `RndDrawable::Save` | `gSaveRev_RndDrawable` = 4, retail = 3 |
| 220 | CubeTex | `RndCubeTex::Save` | `gSaveRev_RndCubeTex` (head word agrees; flagged, extent-guarded) |
| 5932 | Part | `RndParticleSys::SyncProperty` | `motion_parent` and neighbours mis-aimed |
| 6160 | MusicLibrary | `MusicLibrary::Handle` | handler arms `RebuildRestrictedData` / `RemoveLastSongFromSetlist` mis-aimed |

★ These are **100%-matched functions with wrong behaviour**. They are invisible
to every existing near-miss scanner (which only look below 100%). Recommend a
follow-up lane: the `Handle`/`SyncProperty` cases are almost all *arm ordering*
— our `BEGIN_HANDLERS` list is permuted relative to retail — which is one edit
per unit and also corroborates laneBODYPORT's lever #3.

## 6. Does this change what the headline number means?

**Plainly: not much, and not in the direction the brief feared.**

* **43.8–65.5% of the count is positively evidenced** relocation-by-relocation
  against the retail image, depending on how much weight the consistency oracle
  is allowed. That is a floor, not a ceiling — the undecidable residue is
  dominated by `.bss` statics and externs that *have no bytes in either binary*,
  so no instrument can decide them; they are not suspect, they are
  unobservable.
* **2.7–5.2% divergent** is the honest upper bound on "shape, not
  reproduction", and a large part of it is **real source bugs in genuinely
  reproduced code** rather than fake credit (§5).
* Divergence **concentrates in one identifiable stratum**: ≤16 B adjustor/
  forwarder thunks (10.2%) and the `??__E`/`??__F` static-lifecycle thunk
  family. Both are known, both are already the subject of the gap-absorption
  debate, and together they are a small, nameable slice.
* The priority (game) tier is **cleaner** than the engine tier.

So: *"39,522" means roughly "17,000–26,000 functions we can prove we reproduced,
1,060–2,040 we can prove we did not, and 11,000–20,000 the binaries cannot
adjudicate."*
That is a materially sound number. **It does not need to be restated or
discounted.** What it does need is the §4 gate, so that future channels cannot
grow the 5.2% quietly.

## 7. Reproduction

```bash
scripts/setup_worktree.sh ~/tmp/wt-laneBH-1 laneBH-1
cd ~/tmp/wt-laneBH-1 && ./tools/ninja-locked          # MANDATORY: dirty-obj reflink trap

# tree-wide census (~6 min)
python3 scripts/harvest/reloc_correspondence.py --worktree $PWD --census \
        --out ~/tmp/laneBH_census_full.json

# one function, with every relocation slot printed
python3 scripts/harvest/reloc_correspondence.py --worktree $PWD \
        --symbol '?FixClassName@DirLoader@@AAA?AVSymbol@@V2@@Z'

# gate a claimed splits channel (ranges = JSON [[lo,hi],...])
python3 scripts/harvest/reloc_correspondence.py --worktree $PWD --census \
        --va-ranges ranges.json

# falsification control
python3 scripts/harvest/reloc_correspondence_selftest.py --worktree $PWD \
        --census ~/tmp/laneBH_census_full.json --delta 0x40 -n 400
```

Bounds and ablation switches for auditing the tool itself:
`--strict-consistency` (the conservative bound of §3.5), `--no-content` (drop the
`band.exe` oracle), `--no-consistency`, `--no-icf`, `--no-merged-tolerant`.
