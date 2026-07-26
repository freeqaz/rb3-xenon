# Lane AA — working the STRUCTURAL identical-percentage clusters

**2026-07-26.** Worktree `~/tmp/wt-laneAA-clusters`, branch `laneAA-clusters`,
base `0cae0c4f` (**29,653** strict). Decoder inherited from lane Y:
`docs/plans/identical-pct-cluster-scan-2026-07-26.md`.

---

## 1. What the pool actually is at 29,653

The handoff advertised **41 STRUCTURAL clusters / 248 functions** measured at
29,474. Re-running `scripts/harvest/identical_pct_cluster_scan.py` at 29,653
(main advanced +179 in between) gives:

| | clusters | functions |
|---|--:|--:|
| named paired sub-100 pool | — | **1,562** |
| `pct`-axis clusters (>=3 members, >=2 units) | 59 | 367 (23.5%) |
| of those, **STRUCTURAL** | **34** | **179** |

So the structural pool shrank 248 -> 179 purely from other lanes landing work.

### 1.1 ★ 55 of the 179 (31%) are dead on inspection, at zero build cost

Two free screens, both driven off the pre-split target listings in
`build/45410914/asm/` (no objdiff, no compile):

| screen | shape | n |
|---|---|--:|
| **retail coverage stub** | 8 instrs: `stwu r1,-0x60(r1)` / `lis r11,lbl@ha` / `lwz` / `rlwinm r11,r11,0,a,b` / `lis` / `stw` / `addi r1,r1,0x60` / `blr`, r3 never touched | 51 |
| **bare tail-branch** | target body is a 1-instruction `b fn_XXXX` | 4 |

**Independently confirmed by dumping the bodies.** The three largest structural
clusters are *entirely* coverage stubs and are byte-identical to each other
except for the `rlwinm` bit index:

| pct | n | dead | family |
|---|--:|--:|---|
| 17.5 | 24 | 24 | `Accomplishment::GetType` &c. |
| 25.0 | 21 | 20 | `Accomplishment::GetDynamicPrereqsSongs` &c. |
| 37.5 | 7 | 7 | `Accomplishment::GetName` &c. |

> **★ The cluster-size distribution inverts once you screen.** Every structural
> cluster with >=20 members is 100% dead. After the screen the largest live
> cluster is n=9 and the median is n=3. The handoff's "6 clusters >=20 members"
> is not a big-ticket tier — it is the instrumentation tier restated.

This screen should run **before** any triage in future waves. It costs 0.2 s and
removed 31% of the pool.

### 1.2 Of the 124 live functions, only 22 sit in a name-shape-pure cluster

Bucketing each live cluster by the purity of its dominant mangled-name shape:

| purity | clusters | functions |
|---|--:|--:|
| >=60% (plausibly one shared cause) | 8 | **22** |
| <60% (different functions colliding on one percentage) | 24 | 102 |

A cluster of three unrelated function names sharing a percentage is what
coincidence looks like — the percentage encodes only `(S, N)`, and small `S`
over similar `N` collides readily. Purity is not decisive (my one win below was
in a 67%-pure cluster, and a shared *macro* can legitimately cross names), but
it is the right first sort.

---

## 2. Results

### 2.1 ★ FLIP: `SavePlayerStats` — the named sub-object reference local (+2)

**Cluster `pct=99.0`, S=61, N=61, n=3 (2 real + 1 coincidence).** The two-call
rule settled it immediately. `FocusTracker::SavePlayerStats` (`default/FocusTracker`)
and `DeployCountTracker::SavePlayerStats` (`default/DeployCountTracker`) —
unrelated units, different map value types — have a **byte-identical mismatch
profile**:

```
[24] replace : target `addi r28, r3, 0x10`   vs   base `mr r28, r3`
[38] diff_arg: `stfs`  [off:+16]
```

Same instruction, same index, **same direction** => ONE CAUSE.

**Cause.** Retail materialises the address of the `Player::mStats` sub-object
**once** into a callee-save register and then addresses it at offset 0, holding
that pointer across the intervening `map::find` call. Our source kept the outer
`Player*` and addressed the member at its full `+0x10`. Under `/O1` this is
driven purely by whether the C++ source *names* the sub-object in a local
reference.

**★ The negative control settled the fix direction positively — this is the
INVERSE case.** Two same-name siblings already read strict 100.0, and both
already write the reference form:

```cpp
// StreakTracker::SavePlayerStats   (strict 100.0)   Stats &stats = pPlayer->mStats;
// ScoreTracker::SavePlayerStats    (strict 100.0)   Stats &stats = pPlayer->mStats;
// FocusTracker::SavePlayerStats    (99.0)           pPlayer->mStats.unk1c0 = cData->second;
// DeployCountTracker::SavePlayerStats (99.0)        pPlayer->mStats.unk1c0 = cData->second.unk4;
```

Both variants existed in-tree; these two call sites had simply selected the
wrong one. The fix is therefore at the **call sites**, never in a shared header —
exactly what a non-empty sibling-at-100 list demands.

**Measured: 29,653 -> 29,655, 0 regressions.** Commit `df7f5091`.
Files: `src/band3/game/FocusTracker.cpp`, `src/band3/game/DeployCountTracker.cpp`.
The third cluster member (`RockCentral::RedeemToken`) has a different `N` and is
coincidence.

**Family closed.** A third member, `AccuracyTracker::SavePlayerStats`, was
surfaced only by the **`delta_shape`** axis (delta = 0 B; the `pct` axis splits
it away because its `N` differs). Same cause, same one-line fix, **+1**
(commit `c8ac42ee`). The `SavePlayerStats` family is now **7/7 at strict 100.0**
(Focus, Deploy, Accuracy, Streak, Score, PerfectOverdrive, PerfectSection).

> A triage worker had classified `AccuracyTracker` as *"pure
> instruction-scheduling / evaluation-order divergence"*. Measurement says
> otherwise. **Re-verify worker verdicts against your own baseline** — this is the
> second worker verdict this lane overturned by simply running the experiment.

### 2.1b ★ FLIP: the `Localize()` wrong-overload call sites (+9)

Cross-cluster ONE CAUSE spanning `pct=94.44444` and `pct=90.0`.
`src/system/utl/Locale.h:80-83` declares **both** overloads:

```cpp
const char *Localize(Symbol token, bool *success, Locale &locale);
const char *Localize(Symbol token, bool *success);   // the RB3 2-arg form
```

Call sites passed `TheLocale` explicitly and so selected the 3-arg overload;
retail's disassembly makes the clean 2-arg call with no `&TheLocale` load.
**Another instance of the INVERSE case** — both variants in-tree, wrong one
selected. The negative control is same-TU and clean:
`InlineHelp::ActionElement::SetToken` already used the 2-arg form and already
read strict 100.0.

A worker verified 3 call sites; **grepping the tree found 13**, and converting
all 13 gave **+9 with 0 regressions** (`?SetConfig@ActionElement@InlineHelp@@`,
`?SetDateTime@UILabel@@`, plus 7 EH funclets — funclets flip with their parent's
frame). Commit `078f9b2d`. Files: `MeterDisplay.cpp`, `UIListProvider.cpp`,
`InlineHelp.cpp`, `DataFunc.cpp`, `UILabel.cpp`, `LocalePanel.cpp`.

> **The generalisation is worth more than the cluster.** The cluster pointed at 3
> sites; the *cause* covered 13. Once a wrong-overload/wrong-variant cause is
> identified, grep the whole tree for it before measuring.

**Negative results from the same lever, both reverted:**
- `LocalizeSeparatedInt(num, TheLocale)` -> `(num)` at 10 call sites, despite an
  identical two-overload setup in the same header and a header comment asserting
  retail uses the 1-arg form: **delta 0**. The sibling overload does *not* carry
  the same defect.
- `IPP::Add_InPlace` / `Mul_InPlace` commutative operand reorder
  (`f2[i] += f1[i]` -> `f2[i] = f1[i] + f2[i]`), a worker's medium-confidence
  ONE CAUSE: **delta 0**.

### 2.1c Two more measured negatives worth not repeating

- **`RndMesh::VertVector` dtor (`GemRepTemplate::~GemRepTemplate`, 96.13).**
  Target emits `stw` at `+0x60`/`+0x6c` where we emit `sth`; `mCapacity` and
  `unkc` are adjacent `unsigned short`s at `0x8`/`0xa` (`src/system/rndobj/Mesh.h:144`).
  Hypothesis: retail zeroes both and MSVC merges the two `sth`s into one `stw`,
  so adding `unkc = 0;` to `~VertVector()` should reproduce it. **Delta 0.**
  Reverted. (Widening `mCapacity` to 32 bits was *not* attempted — it would
  break the deliberate 0xc packing the header comment documents.)

- **★ Inline-suppression by definition order does not work under `/O1 /Ob2`.**
  `Hmx::Object::HandleProperty` (`default/DirLoader`, 94.0) differs from retail
  by exactly one construct: retail emits `bl ?PathName@@YAPBDPBVObject@Hmx@@@Z`
  where we inline it to `lwz r11,0x0(r29); lwz r11,0x50(r11); mtctr; bctrl` —
  the virtual `FindPathName` dispatch inside `PathName`. `PathName` is defined in
  the same TU (`DirLoader.cpp:1114`) **and is itself at strict 100.0**, so retail
  clearly emitted it out-of-line and called it.
  Hypothesis: MSVC can only inline a body it has already parsed, so moving the
  `PathName` definition *after* `HandleProperty` would force the `bl`.
  **Delta 0, and the diff was byte-for-byte unchanged** — MSVC's `/Ob2` inlines
  TU-wide irrespective of definition order (it is not a one-pass inliner).
  Reverted.
  > **Generalisable:** source-order tricks are not an inlining lever for this
  > compiler. The 9 accompanying `r28<->r29` swaps are a *symptom* of the inline,
  > not an independent regalloc wall — consistent with the lane rule that
  > `regswap => at_limit` is invalid whenever an insert/replace is also present.

### 2.1d ★ The lever generalised: `scripts/harvest/subobject_ref_scan.py` (+1)

The §2.1 cause was productionised into a scanner. **Signature:** a replace /
insert / delete anchor where one side forms `addi rX, rY, K` and the other keeps
`mr rX, rY` (or both form `addi` with different `K`), **corroborated by load/store
rows on `rX` whose immediate delta equals `-K`.** That corroboration is what
separates the lever from ordinary regalloc noise; `r1`-based anchors are tagged
`frame_relative` (stack-layout drift — a different lever).

Over the 963 named sub-100 >=15% pool it ranks 176 anchors across 163 functions,
but **only 21 are object-relative** — the rest are frame drift. So the lever is
real but *thin*, which is the honest calibration.

**`DSP::SynapseAPO::OnSetParameters` — FLIP, 99.90 -> 100.0 (+1).** We spelled
`mParams.bands[i].field` out longhand at all 14 use sites, so MSVC anchored the
induction pointer 4 bytes off retail's (`addi r29, r3, 0x174` vs `0x170`) and
every dependent `lfs`/`lbz` shifted by +4. Introducing `SynapseBand &m` /
`const SynapseBand &t` collapses 11 mismatched rows to 3, and those 3 are
ICF-folded `bl` targets that normalize away. **The negative control is inside the
same function**: the `params`-side anchor already matched, because that side
already used the reference form — only the `this`-side had picked the wrong one.

`GemManager::GemManager` improved but did not flip:
`mTrackDir->Find<RndDir>(...)->LocalXfm().v.y` made MSVC materialise the
`Transform` sub-object (`addi r11, r3, 0xd4` + `lfs 0x50`) where retail keeps the
outer `RndDir*` and loads flat `0x124`. Naming the dir in a local reproduces
retail exactly; the residual is an unrelated ctor store-order pair.

`RGGemMatcher::FretMatchImpl` — anchor corrected, no flip: retail anchors on
`mStringSwings` (+0x18), we anchored on `mStringNonStrum` (+0x30); a
`const float &` binding fixes all 3 anchor rows. Residue is a counter regswap
(a 21-pair decl-order sweep reached 9->7, never 0). Kept as a genuine move toward
retail at **0 delta / 0 regressions**.

Verified by the lane lead against its own baseline: **+1, 0 regressions**
(cherry-picked as `ebec70b3` and `e9c2d425`).

> **★ The candidate universe is closed, and the lever is thin.** Re-running the
> scanner over the *entire* 6,066-function named sub-100 pool
> (`--min-pct 0 --min-size 16`) added **zero** new object-relative candidates.
> Binary-wide only ~18 functions carry the signature, and most are already deep
> body-port targets where the anchor is a rounding error on the diff.
> **Honest hit rate: 5 candidates worked seriously, 1 flipped (20%)** — 2 had
> their anchor genuinely corrected but are blocked by unrelated walls, and 2 are
> codegen states not reachable from *any* C++ form at `/O1` (`MicNull::GetRecentBuf`
> and `RndParticleSys::SetPersistentPool`: MSVC rematerialises the pointer at the
> declaration slot in all 5-6 source forms tried).
>
> **The productive sub-shape is narrower than the signature**: a *loop* over an
> array of sub-objects where retail names the element once per iteration and we
> spelled out `outer.array[i].field` at every use. Both landed wins have exactly
> that fingerprint, and it now appears exhausted.

### 2.2 ★★ CORRECTION to lane Y §7.1: `?SetObj@?$ObjRefConcrete<T>` is a MAP MISPAIR, not `at_limit`

Lane Y published this cluster (`pct=95.625`, S=105, n=5 / 3 units) as
*"ONE CAUSE but `at_limit` — a return-value register-retention artifact that
depends on `SetObjConcrete`'s own codegen"*. **That is wrong.** The target at
those VAs is a different function.

Reading the full target listing for
`?SetObj@?$ObjRefConcrete@VCharClip@@VObjectDir@@@@` (map -> `0x82377478`,
`build/45410914/asm/CharDriver.s:1372`):

```
mr   r31, r3            ; this
mr   r3, r5             ; <-- reads r5
lwz  r11, 0x8(r31)      ; mObject   (offset 0x8)
cmplw cr6, r11, r4
bne  cr6, .L_823774AC   ; if (mObject != r4) return
... __RTDynamicCast(r3=r5, ...) ...
bl   ?SetObjConcrete@?$ObjRefConcrete@VObject@Hmx@@VObjectDir@@@@
```

**The decisive tell is `mr r3, r5`.** `SetObj(Hmx::Object*)` is
`(this=r3, root_obj=r4)` — it has **no r5 parameter**. A function that reads r5
has three arguments. The body is:

```cpp
void Replace(Hmx::Object *from, Hmx::Object *to) {
    if (mObject == from) SetObjConcrete(dynamic_cast<T1 *>(to));
}
```

Corroborated by our own header, `src/system/obj/Object.h:311-340` (the retail,
non-`HX_NATIVE` branch): `mObject` is at **0x8** — matching `lwz r11, 0x8(r31)` —
and the comment states outright *"`Replace(from,to)` is left pure (overridden by
`ObjPtr`/`ObjOwnerPtr`)"*. So `ObjRefConcrete` has no `Replace` of its own and
the map cannot legitimately point a `SetObj` symbol at a `Replace` body.

**Blast radius, and the corroboration that settles it.**
`scripts/target_symbol_map.json` holds **10** `?SetObj@?$ObjRefConcrete@...`
entries and **zero** `?Replace@?$ObjRefConcrete@...` entries. **All 10 SetObj
entries read sub-100** — not one matches:

| VA | pct | unit | instantiation |
|---|--:|---|---|
| `0x8228df88` | 14.69 | `default/BandDirector` | `FileMerger` |
| `0x8228e5a0` | 0.50 | `default/BandDirector` | `RndPostProc` |
| `0x8229d830` | 95.625 | `default/OutfitConfig` | `RndTex` |
| `0x8229da58` | 95.625 | `default/OutfitConfig` | `RndMat` |
| `0x8229dbf0` | 69.83 | `default/OutfitConfig` | `RndTransformable` |
| `0x8229dee8` | 95.625 | `default/OutfitConfig` | `ColorPalette` |
| `0x8229dff0` | 69.83 | `default/OutfitConfig` | `RndTexBlender` |
| `0x8229e2f8` | 69.83 | `default/OutfitConfig` | `RndDir` |
| `0x82314b70` | 95.625 | `default/InlineHelp` | `UIColor` |
| `0x82377478` | 95.625 | `default/CharDriver` | `CharClip` |

A template whose every instantiation is mispaired the same way is a *family*
mispair, worth far more to the map lane than ten scattered singles: the real
`Replace` bodies at these VAs are currently unclaimed by any symbol.

**Not repaired here — map repair is single-owner.** Reported to the map lane.
The 5 cluster members are removed from this lane's live pool as not-source-fixable.

> **Transferable lesson, and it generalises lane Y's own §6 lesson.** Before
> accepting *any* "same shape, residual artifact" verdict, **count the target's
> argument registers.** A target that reads an argument register the symbol's
> signature does not declare is a mispair, full stop — no codegen theory needed.
> This test is free, needs no build, and is decisive where "the bodies look
> similar" is not.

### 2.3 Carried forward from lane Y (verified, not re-derived)

| cluster | S | n | verdict |
|---|--:|--:|---|
| `??4?$ObjVector<T>::operator=` (in `pct=93.333336`) | 160 | 5 of 8 | SHARED MECHANISM — `divw`/`srawi` element `sizeof`, direction flips, map-suspect |
| `?resize@?$vector<T>` (`pct=89.375`) | 340 | 3 | SHARED MECHANISM, map-suspect |
| `?_M_fill_insert@?$vector<T>` (`pct=88.81481`) | 302 | 3 | SHARED MECHANISM, map-suspect |

### 2.4 ★★ CORRECTION: the `??$__uninitialized_copy@` cluster is also a mispair

A triage worker reported `pct=42.857143` (n=3) as this lane's second ONE CAUSE,
with an elaborate theory that our STLport compiles retain
`_STLP_TRY`/`_STLP_UNWIND` exception scaffolding retail lacked, proposing to
define `_STLP_DONT_USE_EXCEPTIONS`. **Refuted before it could be funded**, by the
same argument-register test as §2.2.

`__uninitialized_copy(first, last, result, const __false_type&)` is a **free
function template taking four arguments**. Both mapped targets
(`0x822873d0` / `default/BandCharacter`, `0x823a40f0` / `default/Morph`) read
**only r3**, load `0x4(r3)` and `0x8(r3)`, call one helper, store to `0x8(r31)`
and return `this`. A 4-argument free function cannot compile to that.

The "byte-identical 20-instruction replace table across two units" that looked
like overwhelming shared-cause evidence is just two *near-identical mispair
targets* (the two bodies differ only in one `bl` callee — they are the same
method on two different classes). **A perfectly shared mismatch profile is
necessary but not sufficient for ONE CAUSE**; it is equally the signature of two
map entries pointing at two instances of the same wrong function.

> Landing the proposed fix would have defined `_STLP_DONT_USE_EXCEPTIONS`
> fleet-wide across ~281 STLport-consuming TUs on the strength of a mispair.

### 2.6 The `sizeof` SHARED MECHANISM clusters do NOT fail the mispair test

Worth recording as a **negative** result for the argument-register screen, so it
is not over-applied. Lane Y flagged `?resize@` (S=340) and `?_M_fill_insert@`
(S=302) as "map-suspect". Testing them the same way:

- `?resize@?$vector<IKTarget@CharIKHand>@` -> `0x82373ce8`. Signature
  `resize(unsigned, const IKTarget&)` = r3/r4/r5; the target reads exactly
  r3, r4, r5. It loads `0x0(r3)` and `0x4(r3)` (`_M_start`, `_M_finish`) and
  divides by `li r8, 0x1c` — a correct `resize` shape.
- `?_M_fill_insert@?$vector<String>@` -> `0x822c83d0`. Signature
  `(String*, unsigned, const String&)` = r3/r4/r5/r6; the target reads r3, r5,
  r6, loads `0x4(r3)` and `0x8(r3)` (`_M_finish`, `_M_end_of_storage`) and does
  `srawi r10, r10, 3` — i.e. retail `sizeof(String) == 8`. Also a correct shape.

The two functions read *different* vector member offsets (0x0/0x4 vs 0x4/0x8),
which is exactly right for `size()` vs `capacity()` — mutually consistent, so
these are genuinely paired. **These are real element-`sizeof` divergences, not
mispairs.** They belong to the already-recorded struct-stride vein
(`project_struct_stride_vein_2026-07-20`), where `IKTarget` padding is a logged
**negative control** (regressed -7). Not re-attempted.

### 2.5 Triage results across the whole live pool

| band | clusters | fns | ONE CAUSE | SHARED MECH | MAP MISPAIR | COINCIDENCE / deep-divergent |
|---|--:|--:|--:|--:|--:|--:|
| A (>=88%) | 14 | 50 | **1** (`SavePlayerStats`, landed) | 3 | 1 (`SetObj`, §2.2) | rest |
| B (40-85%) | 7 | 37 | 0 (the 1 claimed was §2.4) | 0 | 3 clusters (~16 fns) | 4 clusters |
| C (<6%) | 11 | 41 | **0** | 0 | ~9 confirmed + 3 suspected | ~28 |

Band C's own summary: *"No ONE CAUSE or SHARED MECHANISM cluster was found.
Every shared-pct grouping traced back to either confirmed mixed mispairs or
coincidental score overlap between structurally-unrelated functions."*

---

## 3. ★★ THE HEADLINE: the structural pool is mostly NOT one-edit work

Stated plainly, because the handoff framed 248 functions as a shared-cause tier:

**Of 179 STRUCTURAL functions at 29,653, 3 flipped from a shared cause inside
the pool — and the *causes* those 3 exposed were worth +12 once generalised
outside it.**

| disposition | fns | share |
|---|--:|--:|
| retail coverage stubs / bare tail-branches (unfixable in source) | 55 | 31% |
| map mispairs (another lane's; confirmed or strongly evidenced) | ~30 | 17% |
| SHARED MECHANISM — element `sizeof` (see §2.6) | 11 | 6% |
| deep-divergent real targets (per-function body ports, not cluster work) | ~80 | 45% |
| **flipped by a shared-cause edit** | **3** | **2%** |

(The 3 in-pool flips are `FocusTracker`/`DeployCountTracker`/`AccuracyTracker`
`SavePlayerStats`. The other 10 of the lane's +13 came from generalising those
causes to code the clusters never pointed at.)

**Whole-lane measured result: 29,653 -> 29,666 (+13), 0 regressions.** Only 3 of
those 13 were cluster members; the other 10 came from generalising the *causes*
outside the pool (9 by grepping, 1 by a scanner). That is the real lesson of the
lane — see §2.1b and §2.1d.

The "identical percentage => one shared cause" signature is real and did produce
a clean win, but at this stage of the project **its dominant failure mode is that
both members are bound to the wrong target.** Lane Y had already shown this for
the ARG-ONLY clusters (its §7.2). This lane extends the same finding to the
STRUCTURAL ones, and it means the honest shared-cause fraction of the named
sub-100 pool is **far below** the 13.6% the STRUCTURAL label implied — the label
measures *penalty composition*, which a mispair passes trivially.

### 3.1 Measured flip rate by cluster size

| cluster size | clusters | fns | flipped | rate |
|---|--:|--:|--:|--:|
| >=20 | 2 | 45 | 0 | 0.0% (100% coverage stubs) |
| 10-19 | 1 | 13 | 0 | 0.0% |
| 5-9 | 7 | 44 | 0 | 0.0% |
| 3-4 | 24 | 77 | **3** | **3.9%** |
| **TOTAL** | **34** | **179** | **3** | **1.7%** |

(In-pool flips only. Counting the +9 harvested by generalising those causes
outside the pool, the lane's yield per triaged cluster is far better than 1.7%
suggests — but that credit belongs to the *grep*, not to the clustering.)

**Cluster size does not predict yield here — it anti-predicts it.** The large
clusters are large precisely because they are degenerate shapes (stubs, thunks)
that many functions share. Price future waves on the *screens* in §1.1/§3.2, not
on member count.

### 3.2 ★ The free screens, in the order they should run

Every one of these costs no build, and together they disposed of ~48% of the
pool before a single objdiff call:

1. **Coverage-stub screen** — 8-instruction breadcrumb body. Killed 51.
2. **★ Argument-register count** — does the target read an argument register the
   symbol's signature does not declare? Cracked §2.2 (5 fns, correcting a
   published `at_limit`) and §2.4 (2 fns, killing a fleet-wide STLport edit).
   Productionised as `scripts/harvest/argreg_mispair_scan.py`.
3. **Bare tail-branch screen** — target body is one `b fn_XXXX`. Killed 4.
4. **Return-slot check** — a function returning a class by value must write to a
   hidden return slot; a target that never writes memory is mispaired.
5. **Name-shape purity** — only then spend the two objdiff calls.

---

## 3.5 The cross-axis hunt: `pct` has one real, characterisable blind spot

A dedicated worker recomputed the STRUCTURAL pool on the `score_shape` and
`delta_shape` axes and diffed the member sets against `pct`:

| axis | STRUCTURAL clusters | STRUCTURAL fns |
|---|--:|--:|
| `pct` | 34 | 179 |
| `score_shape` | 9 | 35 |
| `delta_shape` | 23 | 103 |

13 `delta_shape` STRUCTURAL clusters are **100% absent** from every `pct`
cluster. The blind spot is exactly the predicted one: `pct` encodes `(S, N)`
*jointly*, so **one defect hitting instantiations of different length shatters
into unrelated-looking percentages.** Two concrete payoffs:

- **`AccuracyTracker::SavePlayerStats`** (§2.1) — `delta_shape` 0 B, invisible to
  `pct`. **Flipped, +1.** This alone justifies running the extra axes.
- **A 12-member `??0` constructor cluster at `delta_shape` = −16 B, 0/12 visible
  to `pct`** (`Hmx::Object`, `SharedGroup`, `RndMorph`, `RndMultiMesh`,
  `NgSpotlightDrawer`, `TrackPanel`, …). Two members from unrelated units have a
  byte-identical mismatch shape whose punchline is that **retail has no call at
  all** where we `bl ??0?$ObjPtr@V<T>@@`. Root cause is an **inlining-heuristic
  divergence, not a source defect**: `ObjPtr<T>::ObjPtr(owner, ptr=nullptr)` is
  out-of-line at `src/system/obj/ObjPtr_p.h:249`, and for single-call-site
  instantiations with a statically-null `ptr`, retail's `/O1 /Ob2` inlines it
  away entirely. The rb3-Wii oracle confirms our source is already equivalent
  (`../rb3/src/system/rndobj/Morph.cpp:15`, `mTarget(this, 0)`). **Do not body-port
  these**; any fix is structural and risks the high-fanout shared instantiations
  the header's `ObjPtr_p.h:243-247` comment deliberately protects.

**But `delta_shape` is noisier than `pct`.** Several high-rank net-new clusters
(`?Poll@` −4 B, `??0` +4 B) were coincidences at the instruction level despite
matching byte deltas. **Run the extra axes to find candidates; still spend the
two objdiff calls before funding.**

## 3.7 ★★ The durable asset: `scripts/harvest/argreg_mispair_scan.py`

The argument-register test (§3.2 step 2) was productionised and **validated to a
0% false-positive rate**, which is what makes it fundable:

| control: every strict-100.0 named+paired function (13,453; 12,183 judged) | FPs | rate |
|---|--:|--:|
| **forward signal** (reads an undeclared arg register) | **0** | **0.0000%** |
| inverse, `high` (declared params never read) | 2 | 0.016% |
| inverse, `medium` | 7 | 0.058% |
| inverse, `low` | 49 | 0.402% — **do not use** |

A 100%-matching function cannot be mispaired, so this control is the right one.
Runs in ~3 s, needs **no build**, and never writes `target_symbol_map.json`.

Over the 1,556 non-zero named+paired sub-100 pool: **36 MISPAIR_FORWARD**
(9 high / 27 medium), **21 INVERSE_WEAK**, and **23 FRAGMENT** — a category the
tool had to invent, because dtk **over-carve** produces bodies that read `r0`/
`r11`/`r12`/`f9-f13` undefined and masquerade as confident mispairs. Those 23 are
evidence of a *carving* defect for the jeff/dtk lane, not a map defect. (The FP
control cannot exercise this at all — a matching function is never mis-carved —
so it is the tool's one unmeasurable caveat.)

**It concentrates in families, which is where the value is:**

| family | flagged | sub-100 / paired |
|---|--:|---|
| `ObjRefConcrete<T>::SetObj` | **8** | 10 / 10 — whole family, generalises §2.2 |
| `ObjRefConcrete<T>::~ObjRefConcrete` | 2 | 3 / 25 — same `mr r3, r5` shape |
| `__uninitialized_copy` (inverse) | 6 | 43 / 89 — generalises §2.4 |
| `__uninitialized_fill_n` (inverse) | 5 | 48 / 96 |
| `DateTime::ToString` + `GetTimeZoneBias` | 2 | both off by exactly one register in one unit — likely a single **shifted map region**, not two errors |

> **This is the lane's most reusable output.** It converts "is this pair even
> right?" — previously a judgement call that cost a build and was got wrong at
> least three times in published lane notes — into a 3-second, zero-FP check.
> Run it **before** any near-miss campaign, not after.

Limits the author measured honestly: 9.4% of functions are punted (by-value
aggregate params make the register count unbounded); `this`-ness is read from
`undname`'s access specifier because the mangled letter is genuinely ambiguous
inside template parameter lists; the inverse `low` tier is noise (its FPs are
retail-stripped `MILO_DEBUG` file/line args and pass-through base ctors).

### 3.6 Two handoffs

- **To the map lane** — the `ObjRefConcrete` family (§2.2, 10 entries, all
  sub-100, real `Replace` bodies unclaimed); the `__uninitialized_copy` pair
  (§2.4); plus ~20 further mispairs itemised by the band-B/C workers, whose
  reports are in the task transcripts and whose worktrees
  (`~/tmp/wt-laneAA-triB`, `~/tmp/wt-laneAA-triC`) are left in place.
- **To whoever owns landing** — `?_M_insert_overflow_aux@` (10 fns, `delta_shape`
  4/12/24 B) is already fixed on the **unlanded** branch `laneY-stlv`
  (`94f306cb`, `0902dc1e`, `src/system/utl/StlAlloc.h`, "+28"). `git merge-base
  --is-ancestor` confirms neither commit is in main. Landing that branch closes
  these without new work.

---

## 4. Method notes worth keeping

- **The two-call rule works and is cheap.** One `run_objdiff` pair settled the
  `SavePlayerStats` cluster in two calls and produced a landed flip.
- **But run the free screens first.** Dumping the target body from
  `build/45410914/asm/` costs no build and killed 31% of the pool plus one
  5-member cluster (§2.2) that two objdiff calls had previously *mis*-diagnosed.
  Order: coverage-stub screen -> argument-register count -> tail-branch screen ->
  name-shape purity -> only then spend objdiff calls.
- **`report.json` is the only truth.** `run_objdiff` printed "95.2% normalized"
  for a function `report.json` records at 95.625.
- Helper used throughout: `/home/free/tmp/tgtasm.py` — `dump(unit, mangled_name)`
  prints any symbol's retail target assembly via `scripts/target_symbol_map.json`
  plus the pre-split listings.
