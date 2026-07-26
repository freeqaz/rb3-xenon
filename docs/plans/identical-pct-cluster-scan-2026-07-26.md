# The identical-percentage cluster scan — how much of the sub-100 pool is shared-cause?

**Lane Y, 2026-07-26.** Worktree `~/tmp/wt-laneY-pctcluster`, branch
`laneY-pctcluster`, base `6c32b938` (**29,474** strict).

Tool shipped: **`scripts/harvest/identical_pct_cluster_scan.py`**
Prior art: `docs/plans/funclet-cascade-lever-2026-07-25.md` §31.1, §32.4, §32.6.

---

## 1. The signature, and why the percentage is so sharp

Lane X closed with a recommendation rather than a lever:

> *"Generalise the **signature**, not the family: an identical raw percentage
> across unrelated units is the single highest-precision lead shape this lane
> has found (13/13 on `SetType`, 18→1 for `SyncProperty`). Worth a standing scan
> that buckets all sub-100 functions by exact `match_percent_normalized`."*
> — §32.6

This lane built that scan. The first thing it establishes is **why** the
signature works, which lane X did not have:

`objdiff-core/src/diff/code.rs:276-291`

```rust
let max_score = left_ops.len() as u64 * PENALTY_INSERT_DELETE;   // = N * 100
let match_percent_normalized = ((1.0 - (normalized_diff_score as f64
                                        / max_score as f64)) * 100.0) as f32;
```

with `PENALTY_INSERT_DELETE=100`, `PENALTY_REPLACE=60`, `PENALTY_REG_DIFF=5`,
`PENALTY_IMM_DIFF=1`.

**The percentage is not a fuzzy score. It is a lossless encoding of the rational
`S / (100·N)`** — penalty score over instruction count. Two functions sharing a
percentage share `(S, N)` *exactly*. And because `N` is recoverable from the
function's byte size / 4, the percentage can be **inverted** to the exact integer
penalty:

```
S = round((100 - pct) * N)      # confirmed by an f32 round-trip
```

`invert_score()` recovers **3,755 of 3,755** paired sub-100 functions with zero
failures. So `98.78049` is not "about 98.8% matched" — it is
**"82 instructions, penalty 100, i.e. exactly ONE inserted-or-deleted
instruction"**. That is directly actionable in a way a percentage is not.

> **Gotcha worth keeping.** `report.json` stores the *shortest decimal repr of
> objdiff's f32*. Python parses it to the nearest **double**, which is not
> bit-equal to the f32. A round-trip check that compares the reconstruction
> against the parsed double fails on ~90% of inputs. Force both sides through
> `struct.pack("<f", …)`. The first cut of this tool had exactly that bug and
> reported 5 clusters where there are 37.

## 2. Five axes, of which two are the load-bearing ones

| axis | key | use |
|---|---|---|
| `pct` | exact `repr()` of the f32 | the signature as lane X stated it |
| `score` | integer `S` alone | **diagnostic only** — scale-free, merges a 4-instruction stub with a 128-instruction method |
| **`score_shape`** | (name-shape, `S`) | ★ the generalisation of `pct` |
| `delta` | base-minus-target byte delta | **diagnostic only** — the `+0`/`+4` bins are enormous |
| **`delta_shape`** | (name-shape, byte delta) | ★ independent confirmation channel |

**`score_shape` is strictly better than `pct`**, and this is the tool's main
conceptual contribution. `pct` encodes `(S, N)` *jointly*, so **one cause hitting
instantiations of different length splits into several near-but-unequal
percentages**. The largest cluster in the project demonstrates it:

| pct | N | S | members |
|---|--:|--:|--:|
| 98.765434 | 81 | 100 | 6 |
| 98.78049 | 82 | 100 | 17 |
| 98.809525 | 84 | 100 | 4 |

Three "different" percentages, one defect. The `pct` axis reports three clusters
of 6/17/4; `score_shape` reports **one cluster of 28**. This is exactly the
prompt's ★"also bucket by exact instruction-count / byte-size delta — a shared
cause often shows there when the percentage differs slightly because the bodies
differ in length". It does, and `score_shape` is the clean way to see it.

`delta_shape` is the independent check: the same family shows up as
**`?_M_insert_overflow_aux@` + exactly +4 bytes, 45 members / 38 units**. One
surplus instruction = 4 bytes. A cluster that appears on **both** `score_shape`
and `delta_shape` is as close to certain as this project gets.

## 3. ★ The strategic number: how much of the pool is shared-cause?

Measured on `build/45410914/report.json` at 29,474 strict.

### 3.1 Named functions (the actionable pool)

**Pool: 1,830 named, paired, sub-100 functions.**

| axis | clusters (≥3 members, ≥2 units) | functions covered | share of pool |
|---|--:|--:|--:|
| `pct` | **68** | 517 | **28.3%** |
| `score_shape` | **37** | 293 | 16.0% |
| `delta_shape` | **68** | 535 | 29.2% |

Cluster size distribution on the `pct` axis: **6 clusters ≥20 members, 6 of
10–19, 26 of 5–9, 30 of 3–4.**

Roughly 28% of the named sub-100 pool sits in an identical-percentage cluster,
heavily front-loaded (the top 6 clusters alone hold ~140 functions).

**But 28% is the gross number, and it is not the number to fund.** §3.3 splits it.

### 3.2 Whole pool including anonymous symbols

**Pool: 3,755 paired sub-100 functions** (excluding the 36,186 at `0.0`, which
are unpaired and carry no diff signal at all).

| axis | clusters | covered | share |
|---|--:|--:|--:|
| `pct` | 86 | 2,418 | **64.4%** |
| `score_shape` | 54 | 374 | 10.0% |

The 64.4% looks spectacular and is *mostly one already-known thing*. See §4.

### 3.3 ★★ STRUCTURAL vs ARG-ONLY — the number that actually matters

§5.2 establishes, on measured controls, that an identical percentage is only
strong evidence when the **penalty is structural** (an insert / delete / replace)
rather than pure operand noise. Splitting every cluster on that basis:

| pool | axis | STRUCTURAL | ARG-ONLY |
|---|---|--:|--:|
| named (1,830) | `pct` | **41 clusters / 248 fns (13.6%)** | 27 clusters / 269 fns (14.7%) |
| named (1,830) | `score_shape` | 13 clusters / 92 fns | 24 clusters / 201 fns |
| all paired (3,755) | `pct` | 54 clusters / 498 fns | 31 clusters / **1,917** fns |

> **THE HEADLINE. About 14% of the named sub-100 pool — 248 functions in 41
> clusters — is high-confidence shared-cause work. Another ~15% shares a
> percentage on ARG-ONLY evidence, which is measurably coincidence at least some
> of the time.** So the honest answer to "how much of the sub-100 pool is
> shared-cause rather than per-function?" is **~14%, not the ~28% the raw
> clustering suggests and definitely not the 64% the whole-pool figure
> suggests.**
>
> Note how cleanly the split falls out on the whole pool: **1,917 of the 2,418
> clustered functions are ARG-ONLY, and they are almost exactly the EH-funclet
> mass** (§4). That is independent corroboration of lane L's correction — the
> funclet tier is one-or-two-immediates-off and is *not* one-edit work.

## 4. ★ The honest caveat: the giant anonymous clusters are the funclet pool restated

The four largest `pct` clusters in the whole-pool census are anonymous:

| pct | n | N | S | shape |
|---|--:|--:|--:|---|
| 99.9 | **924** | 10 | 1 | 1 immediate diff |
| 99.8 | 407 | 10 | 2 | 2 immediate diffs |
| 92.5 | 76 | 8 | 60 | 1 replace |
| 93.9 | 72 | 10 | 61 | 1 replace + 1 imm |

Pulling a member's target asm settles what they are:

```
.fn fn_8277DC34
    subi  r31, r12, 0xc0        <-- parent frame size
    mflr  r12
    stw   r12, -0x8(r1)
    stwu  r1, -0x60(r1)
    addi  r3, r31, 0x58
    bl    fn_828043A8           <-- member dtor
    addi  r1, r1, 0x60
    lwz   r12, -0x8(r1)
    mtlr  r12
    blr
```

That is the **EH funclet** shape from `funclet-cascade-lever-2026-07-25.md` §1
verbatim. So the 924-member cluster is: *924 EH funclets that are exactly ONE
immediate away from strict 100%*, and that immediate is the parent's frame size.

This is a genuinely useful number — it is the honest size of the
"one-immediate-away" funclet tier — but it is **not a new lever**, and it is
**not a one-edit fix**. Lane L's correction at the head of the funclet doc is
decisive: a funclet flips when the *parent's compiled frame changes*, which is
per-parent body work. 924 funclets ≠ 924 cheap flips.

The 92.5 cluster (76 members, 8 instructions, one `replace`) is a **distinct
funclet sub-shape** worth naming — an *unwind action* funclet that clears a
static-init guard bit and therefore needs no `subi rX, r12`:

```
.fn fn_82305F0C                 (default/TrackPanelDir)
    stwu   r1, -0x60(r1)
    lis    r11, lbl_82CBD9E8@ha
    lwz    r11, lbl_82CBD9E8@l(r11)
    rlwinm r11, r11, 0, 30, 28     <-- clears guard bit
    lis    r10, lbl_82CBD9E8@ha
    stw    r11, lbl_82CBD9E8@l(r10)
    addi   r1, r1, 0x60
    blr
```

76 of these, each one instruction-replacement away. The `rlwinm` mask encodes
**which** guard bit, i.e. how many function-local statics precede it — the same
quantity the §23.2 local-static `Symbol` lever reads. These are downstream of
that lever, not independent work.

> **So: report the 64.4% whole-pool figure only with this attached.** The
> defensible number for "how much of the pool is shared-cause work you can
> actually route to a source edit" is the **28.3% named-pool figure**, and even
> that includes STL-template families whose shared cause may be a codegen
> artifact rather than a source surplus.

## 5. The top named clusters (the worklist this scan produces)

`score_shape` axis, ranked by (members × mean function size), with the two
columns that decide whether to fund: the **confidence** label (§5.2) and the
number of same-name-shape functions **already at strict 100.0** (§5.1).

| family | S | n | units | shape | confidence | sibs@100 |
|---|--:|--:|--:|---|---|--:|
| `?_M_insert_overflow_aux@?$vector<T>` | 100 | **28** | 26 | 1 ins/del | **STRUCTURAL** | 19 |
| `?_M_insert_overflow_aux@?$vector<T>` | 103 | 14 | 13 | 1 ins/del + 3 imm | **STRUCTURAL** | 19 |
| `?_M_fill_insert@?$vector<T>` | 1 | 20 | 18 | 1 imm | ARG-ONLY | 46 |
| `??_G<Class>` (scalar deleting dtor) | 2 | **32** | 31 | 2 imm | ARG-ONLY | 390 |
| `??_G<Class>` | 1 | 21 | 21 | 1 imm | ARG-ONLY | 390 |
| `?_M_fill_insert_aux@?$vector<T>` | 9 | 5 | 3 | 1 reg + 4 imm | ARG-ONLY | 53 |
| `??$__uninitialized_fill_n@<T>` | 1 | 16 | 12 | 1 imm | ARG-ONLY | 47 |
| `??0<Class>` ctors | 580 | 5 | 5 | 5 ins/del + 1 replace | **STRUCTURAL** | 885 |
| `?resize@?$vector<T>` | 2 | 10 | 10 | 2 imm | ARG-ONLY | 93 |
| `?_M_fill_insert@?$vector<T>` | 302 | 10 | 10 | 3 ins/del + 2 imm | **STRUCTURAL** | 46 |
| `?push_back@?$vector<T>` | 1 | 9 | 8 | 1 imm | ARG-ONLY | 61 |
| `?_M_insert_overflow_aux@?$vector<T>` | 783 | 3 | 3 | — | **STRUCTURAL** | 19 |
| `??$__uninitialized_copy@<T>` | 2 | 10 | 7 | 2 imm | ARG-ONLY | 46 |
| `??$__destroy_range_aux@<T>` | 2 | 9 | 7 | 2 imm | ARG-ONLY | 58 |
| `??$_M_allocate_and_copy@<T>` | 1 | 7 | 7 | 1 imm | ARG-ONLY | 31 |
| `?resize@?$vector<T>` | 340 | 5 | 4 | 1 ins/del + 4 replace | **STRUCTURAL** | 93 |
| `??$_Destroy_Range@<T>` | 1 | 7 | 7 | 1 imm | ARG-ONLY | 32 |
| `??1<Class>` dtors | 2 | 5 | 5 | 2 imm | ARG-ONLY | 711 |
| `?SetObj@?$ObjRefConcrete<T>` | 105 | 5 | 3 | 1 ins/del + 1 reg | **STRUCTURAL** | 0 |

On the `pct` axis only (not merged by `score_shape` because their percentages
are exact and their shapes mixed): the `Accomplishment` family accessors at
**17.5** (24 members / 6 units) and **25.0** (21 / 6) — see §7.3.

Full listing: `python3 scripts/harvest/identical_pct_cluster_scan.py --axis all --with-sizes --top 40`.

### 5.1 The `--siblings` negative control is not optional

Lane X §32.2 nearly lost `ColorPalette`/`SpotlightDrawer` to a mechanically
ported recipe: they carry the *exact* shape a previous sweep removed elsewhere,
**and match retail at 100% because it is there**. The scanner therefore prints,
for every cluster, the count of same-name-shape functions that **already read
100.0** — and this is on by default.

It matters immediately here. **19 `_M_insert_overflow_aux` instantiations are
already at strict 100.0**, alongside the 42 that are not. Whatever the shared
cause is, the fix must be conditioned on something that distinguishes the two
groups — it cannot be an unconditional edit to the template body. Same for
`?resize@` (93 siblings at 100.0) and `??$__uninitialized_fill_n@` (47).

> **Rule:** a non-empty sibling list means *change the call sites / the
> instantiating types, not the shared definition.*

### 5.2 ★★ The discriminator: STRUCTURAL beats ARG-ONLY (two measured controls)

The single most important calibration this lane produced. Both controls were run
on the same build, at 29,474.

**Control A — a TRUE cluster.** `?_M_insert_overflow_aux@` at S=100, one
insert/delete. Two members from entirely unrelated units:

| | `vector<LevelData>` (default/system/synth_xbox/Synth) | `vector<Keyframe@LightPreset>` (default/LightPreset) |
|---|---|---|
| instructions | 82 | 82 |
| **delete @ idx 26** | `stw r24, 0x58, r31` | `stw r24, 0x58, r31` |
| arg-diffs | idx 10, 24, 27, 37, 67, 76 (r23↔r24) | idx 10, 24, 27, 37, 67, 76 (r23↔r24) |

A **byte-identical mismatch profile** across unrelated instantiations. One shared
cause, and the whole-function r23↔r24 swap is a §24 *symptom* of the surplus
store, not an independent regalloc wall.

**Control B — a FALSE cluster.** `??$_Destroy_Range@` at S=1, one immediate.
7 members, 7 units at exactly 99.95 (the 99.95 `pct` bucket holds **29**
functions, but only 7 are `_Destroy_Range` — the other 22 are `??_G` thunks, a
neat demonstration in its own right that the raw `pct` axis mixes families and
`score_shape` is the right key):

| member | the single differing immediate |
|---|---|
| `Character::Lod` | `addi [off:-4]` |
| `FileMerger::Merger` | `addi [off:+84]` |

**Same score, unrelated causes** — per-instantiation element `sizeof`. The
identical percentage here is an artifact of the divergence being *minimal* (one
immediate in a 20-instruction loop), not of the cause being shared.

> **THE RULE, now encoded in the tool as a `confidence:` line.**
> The precision of the identical-percentage signature comes from the penalty
> being **structural** — a whole instruction inserted/deleted/replaced at the
> *same index* implies the same source-level defect. A penalty made only of
> **immediate/register operand diffs implies nothing**, because templates and
> per-class compiler thunks parameterise exactly those immediates. An identical
> tiny score across instantiations is the *expected* coincidence.
>
> This retro-explains lane X's own hit rate: `SetType` was S≈296 over N=8 and
> `SyncProperty` carried +32…+52-byte insert clusters — both **STRUCTURAL**.
> Both flipped ~100%. Nobody had yet tried the heuristic on an ARG-ONLY cluster.

**Practical consequence:** `??_G` S=2 (32 members), `?_M_fill_insert@` S=1 (20),
`??$__uninitialized_fill_n@` S=1 (27), `?resize@`/`?push_back@` S=2 — all the
biggest-looking *named* clusters — are ARG-ONLY and should be cost two objdiff
calls (compare the immediate's value across two members), not a fix wave. This
is also consistent with project memory's already-measured verdicts
(`_M_fill_insert 0/9`, `STL 0/6`, "root cause = element sizeof").

### 5.3 Truth-discipline note: `run_objdiff` normalized ≠ `report.json` normalized

`??$_Destroy_Range@PAULod@Character@@…` reads **99.95** in `report.json` but
`run_objdiff` prints **"Match: 100.0% normalized"** for the same function in the
same worktree. This is stronger than the known "99.976 rounds to 100.0" trap:
the two paths disagree on whether an immediate-operand diff counts toward the
normalized score at all. **`report.json` is the only truth.**

---

## 6. ★★ CLOSED: `RndLine::Save` is a MAP MISPAIR, not a missing `SAVE_SUPERCLASS`

`funclet-cascade-lever-2026-07-25.md` §32.3 left one open one-off:

> *"`RndLine::Save`'s `SAVE_SUPERCLASS(RndTransformable)` appears absent from the
> target stream … Unverified — worth one experiment, not a sweep."*

**Experiment run. The chain is fine; the map entry is wrong.** Reading the full
target listing for `?Save@RndLine@@UAAXAAVBinStream@@@Z`
(`scripts/target_symbol_map.json` → `0x82483858`), the target function calls:

| target | ours |
|---|---|
| `?Save@Object@Hmx@@` | `?Save@Object@Hmx@@` |
| **`?Save@RndAnimatable@@`** | `?Save@RndDrawable@@` |
| — | `?Save@RndTransformable@@` |
| **`op<<(vector<Pose@RndMorph>)`** | `op<<(vector<Point@RndLine>)` |
| **`ObjRefConcrete<RndTex>`** | `op<<(ObjRefConcrete<RndMat>)` |
| `lbz -0xc(r31)` → `Write(…,1)` | |
| `lbz -0xb(r31)` → `Write(…,1)` | |
| `lfs -0x8(r31)` → `WriteEndian(…,4)` | |

Two bools then a float, after a `vector<Pose>` and an `ObjPtr<RndTex>`, chained
off `Hmx::Object` + `RndAnimatable`. That is `RndMorph::Save` exactly
(`src/system/rndobj/Morph.cpp`):

```cpp
BEGIN_SAVES(RndMorph)
    SAVE_REVS(4, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(RndAnimatable)
    bs << mPoses << mTarget << mNormals << mSpline << mIntensity;
END_SAVES
```

So `0x82483858` is `?Save@RndMorph@@UAAXAAVBinStream@@@Z`, and the map currently
also points `?Save@RndMorph@@` at `0x826a8eb8` — one of the two is wrong.

**Not repaired here — map repair is single-owner and belongs to another lane.
Reported, not applied.** The §32.3 lead is closed either way: there is no
`SAVE_SUPERCLASS` defect on `RndLine`, and the SAVE family stays at 0/340.

> **Transferable lesson.** Before concluding "retail is missing a call we emit",
> check the calls retail *does* make against the class's own members. A mispair
> presents as a plausible-looking chain difference, and §32.3's own warning
> ("reading `bl`-vs-`bl` as 'call present' is not enough") generalises: read the
> *callee identities and the member offsets they touch*, not just the call
> count.

## 6.5 ★★ A THIRD category the evidence forced: SHARED MECHANISM, PER-INSTANCE CAUSE

`STRUCTURAL` turned out to be **necessary but not sufficient**. The counter-case
is `?_M_fill_insert@?$vector<T>` at S=302 (10 members / 10 units, 89.21429 and
88.81481) — structural by the label, and the mismatch really is at the *same
indices* in every member. But the **direction flips**:

| member | idx 7 | idx 10 | idx 12 |
|---|---|---|---|
| `vector<TransformCrowd>` (HamCamTransform) | target-only `li r9, 0x1c` | target-only `divw r10, r10, r9` | **base**-only `srawi r10, r10, 4` |
| `vector<PracticeStep>` (PracticeSection) | **base**-only `li r9, 0x1c` | **base**-only `divw r10, r10, r9` | target-only `srawi r10, r10, 4` |

Same instructions, same indices, **opposite sides**. `divw` vs `srawi` is the
compiler dividing by the element `sizeof`: a non-power-of-2 size forces `divw`,
a power-of-2 becomes `srawi`. So `TransformCrowd` is 0x1c in *our* header and 16
in retail, while `PracticeStep` is 16 in ours and 0x1c in retail. This is the
known **struct-stride vein** (`project_struct_stride_vein_2026-07-20`).

> **Refined taxonomy — use this to route, not the raw cluster size:**
>
> | category | test | action |
> |---|---|---|
> | **ONE CAUSE** | same instruction, same index, **same direction** across ≥3 members | fund a fix wave |
> | **SHARED MECHANISM** | same instruction & index, **direction varies** | N per-instance jobs sharing a diagnosis; fund only if the per-instance fix is cheap |
> | **COINCIDENCE** | ARG-ONLY, immediate values differ across members | 2 objdiff calls, verdict, stop |
>
> **The decisive test is always the same and always cheap: run objdiff on two
> members from unrelated units and compare the mismatch text, index AND
> direction.** Two tool calls settle a 30-member cluster. Do this before funding
> anything.

### 6.6 The `sizeof` direction-flip is the single most common cluster shape

Four independent STRUCTURAL clusters were tested this session and **all four**
turned out to be the same `divw`-vs-`srawi` element-`sizeof` mechanism with the
direction varying between members:

| cluster | S | n / units | witness A | witness B |
|---|--:|--:|---|---|
| `?_M_fill_insert@?$vector<T>` | 302 | 10 / 10 | `TransformCrowd`: target `divw`, ours `srawi 4` | `PracticeStep`: target `srawi 4`, ours `divw` |
| `?resize@?$vector<T>` | 340 | 5 / 4 | `IKTarget` 0x1c | `CamShotCrowd` 0x108 |
| `??4?$ObjVector<T>::operator=` | 160 | 5 / 5 | `IKTarget`: target `li r10,0x4c` + `divw`, ours `srawi 4` | `Style@RndText`: target `srawi 5`, ours `li r10,0x24` + `divw` |
| `??$__uninitialized_fill_n@` | 3 | 5 / 5 | `FilePath` −4 | `Grammar` +16 |

`divw` vs `srawi` is just the compiler dividing by the element size: a
non-power-of-2 `sizeof` forces `divw`, a power-of-2 collapses to a shift. So the
*mechanism* is shared and the *cause* is per-element-type.

> **★ And §7.2 casts doubt on even that.** If `vector<A>::_M_fill_insert` in our
> build is paired against retail's `vector<B>::_M_fill_insert`, the diff shows
> exactly this shape with no layout bug present at all. The `EyeDesc`
> contradiction proves at least some of these pairings are wrong. Note the
> corroboration from project memory: **`IKTarget` padding was already tried and
> regressed (−7)** — which is what you would expect if the "wrong size" were a
> mispair rather than a real layout error.
>
> **Recommendation: treat the struct-stride vein as map-suspect until each
> candidate's delta is shown consistent across every other function of the same
> class.** That check is free (§7.2 handoff 1) and should gate any padding edit.

## 7. Results

### 7.1 Verified findings from the fix wave

**`?SetObj@?$ObjRefConcrete<T>@` S=105 (5 members / 3 units) — ONE CAUSE, but
`at_limit`.** Three members from three units (`CharClip`/CharDriver,
`UIColor`/InlineHelp, `RndTex`/OutfitConfig) have a byte-identical mismatch
shape: `mr r3, r5` vs `mr r3, r4` at idx 5, `bne` vs `beq cr6` at idx 8, and a
base-only `lwz r3, 0x8(r31)` at idx 19. Source is
`src/system/obj/ObjPtr_p.h:120-130`. The residual is a return-value
register-retention artifact that depends on `SetObjConcrete`'s own codegen
(its `AddRef` virtual-dispatch tail happening to leave `r3` holding the right
pointer across the `bl`) — not reproducible from a confined template edit. DC3's
newer `ObjPtr_p.h:81` drops the `mObject != root_obj` guard entirely and is
**not** the fix; our guard order is the one matching retail. Declined, 0 edits.

**`?resize@?$vector<T>` S=340 (5 / 4) — SHARED MECHANISM.** Target divides by a
non-power-of-2 `sizeof` (0x1c / 0x48 / 0x108) where we emit a power-of-2 shift.
Per-class struct sizes, and `IKTarget` is already a recorded **negative control**
(padding it regressed −7). Not re-attempted.

**`??$__uninitialized_fill_n@` S=3 (5 / 5) — COINCIDENCE, confirmed.** Same
indices (9/17/19) in every member, but the immediate deltas are unrelated:
`pair<DataArray*,DataNode>` −20, `FilePath` −4, `Grammar` +16, and PanelDir's
stack-slot pair 0x50/0x54 is *reversed* relative to the other two. Textbook
ARG-ONLY.

**`??$_Destroy_Range@` S=1 (7 / 7) — COINCIDENCE** (§5.2 control B).

**`?_M_fill_insert@` S=302 (10 / 10) — SHARED MECHANISM** (§6.5).

### 7.2 ★★★ The biggest result: the compiler-generated ARG-ONLY clusters are MAP MISPAIRS

This supersedes the "coincidence" reading for a whole class of clusters, and it
corrects a standing project-memory claim.

**The `??_G` cluster (32 members / 31 units, all at 99.882355) is 31 target
mispairs.** Every Milo class inherits `Hmx::Object` **virtually**, so its scalar
deleting destructor compiles to a 17-instruction body whose only class-specific
content is *one immediate* — the vbase offset, emitted twice — plus three
relocations:

```
 3  subi r30, r3, 0x30    |  subi r30, r3, 0x90     <- vbase offset
 5  addi r31, r30, 0x30   |  addi r31, r30, 0x90    <- the same immediate
 7  bl ??1<Class>         |  bl ??1<Class>          (reloc -- MASKED)
 9  bl ??1Object@Hmx      |  bl ??1Object@Hmx
13  bl ?MemFree           |  bl ??3<Class>@@SAXPAX@Z (reloc -- MASKED)
```

Under normalized diff (`functionRelocDiffs=none`) the relocations are masked, so
**any two such thunks whose vbase offsets differ score exactly 15/17 =
99.882355.** The percentage is the fingerprint of the *shape*, not of a cause.
A full 31-member scan found vbase deltas scattered from **−292 to +336**, and in
**30 of 31** the target thunk destroys a completely unrelated class
(`??_GCharDriver` ↔ target destroying `CharNeckTwist`; `??_GFlow` ↔
`RndCamAnim`; `??_GRndMeshAnim` ↔ `StorePreviewMgr`; `??_GSharedGroup` ↔
`BandRetargetVignette`).

**The smoking gun, reproduced independently by the lane lead.** Two functions of
the *same* template instantiation, in the *same* unit `default/HamCamTransform`:

| function | target-vs-base immediate |
|---|---|
| `?_M_fill_insert@?$vector<EyeDesc@CharEyes>` | `li` **off:−36** |
| `?resize@?$vector<EyeDesc@CharEyes>` | `li`/`mulli` **off:+48** |

Both immediates are `sizeof(CharEyes::EyeDesc)`. **One retail binary cannot hold
two different values for it.** At least one of the two is mispaired — the
contradiction is logically airtight, independent of any theory about layout. The
same contradiction holds across all 40 members of the 99.96429 / 99.9375 bands
(`Spotlight::BeamDef` 12 vs 108; `TrackDir` `Transform` 120 vs 64;
`MoveAsyncDetector::DetectFrame` 76 vs 1072).

> **★ Correction to project memory.** `project_struct_stride_vein_2026-07-20`
> records "STL `fill_insert` family root cause = element `sizeof` (`divw` vs
> `srawi` cluster)". That is **half right and misleading**: the differing
> immediate *is* a `sizeof`, but it is frequently **the wrong instantiation's**
> `sizeof`. Padding a struct to chase it is chasing a mispair. This also
> explains §6.5's direction flip (`TransformCrowd` vs `PracticeStep` showing
> `divw`/`srawi` on opposite sides) without needing two opposite layout bugs.

**Why the correlator cannot see this.** Compiler-generated thunks and
`vector<T>` members have **zero source freedom** given the layout — they differ
only in immediates, which is exactly what a **reloc-masked byte-identity
correlator masks**. The homing scan will happily bind any same-shape address.
This is a *tooling degeneracy*, not a codegen wall.

**Not repaired here — map repair is single-owner. Reported, not applied.** Two
concrete handoffs for the map lane, both build-free:

1. **A free filter — shipped as `--mispair-check`.** Any `??_G` /
   `_M_fill_insert` / `resize` / `push_back` not at 100% is provably *either* a
   mispair *or* a layout bug — and a real layout bug must move **every** function
   of the instantiation, because they all step by the same `sizeof`. The scanner
   now groups functions by *instantiation* (method name and signature stripped)
   and lists the **583** instantiations where only a minority disagree:

   ```
   $ python3 scripts/harvest/identical_pct_cluster_scan.py --mispair-check
   instantiation ?$vector@UEyeDesc@CharEyes@@…   4 sub-100 vs 5 at 100.0
      SUB   98.744  default/HamCamTransform   ?_M_insert_overflow_aux
      SUB   99.908  default/HamCamTransform   ?_M_fill_insert_aux
      SUB   99.938  default/HamCamTransform   ?resize
      SUB   99.964  default/HamCamTransform   ?_M_fill_insert
      OK   100.000  default/CharEyes          ?_M_erase
      OK   100.000  default/CharEyes          ?_M_erase
   ```

   **`_M_erase` steps by `sizeof(EyeDesc)` too and is at strict 100.0.** A wrong
   `sizeof` in our header cannot produce that. Third independent confirmation
   that the EyeDesc divergence is a pairing defect, not a layout defect.
2. **A better anchor.** Retail's vtable data symbol `??_7C@@6BObject@Hmx@@@`
   slot 0 *is* the correct `??_GC`. Vtables are long relocation arrays and are
   **not shape-degenerate**, so they disambiguate where byte-identity cannot.

### 7.3 The `Accomplishment` 17.5 / 25.0 cluster — ONE CAUSE, and it is unfixable in source

44 of the 45 members have a **byte-for-byte identical 8-instruction target
body** (`build/45410914/asm/Accomplishment.s:700`, same shape in
`AccomplishmentProgress.s:139` and `UIEvent.s`):

```
stwu   r1, -0x60(r1)
lis    r11, lbl_82CC2E58@ha
lwz    r11, lbl_82CC2E58@l(r11)
rlwinm r11, r11, 0, 5, 3        # clears ONE unique bit
lis    r10, lbl_82CC2E58@ha
stw    r11, lbl_82CC2E58@l(r10)
addi   r1, r1, 0x60
blr                             # r3 NEVER touched
```

Only the bitmap word and the bit index vary, and consecutive functions march the
bit down by one. This is the **retail coverage-breadcrumb stub** already recorded
in `project_game_code_instrumentation` — *not* a member-offset, base-class,
`this`-adjustor or virtualness defect. All three structural hypotheses were
tested and are negative; `Accomplishment.cpp` really does say
`return kAccomplishmentTypeUnique;` and our codegen is correct.

**Why exactly two percentages** — this is the cluster's whole mechanism, and it
is a nice illustration of §1. Our side emits 2 instructions and the `blr` always
matches; the *second* base instruction decides the bucket:

| our body | objdiff verdict | pct |
|---|---|--:|
| `li r3, N` (returns a constant) | **replace** — no opcode credit | **17.5** |
| `lwz r3, off(r3)` (returns a member) | **diff_arg** vs the stub's `lwz r11, blob@l(r11)` — same opcode, partial credit | **25.0** |

Confirmed on `GetType@Accomplishment` (`li r3,0x0`), `IsDialogEvent@DialogEvent`
(`li r3,0x1`), `GetIndex@AccomplishmentCategory` (`lwz r3,0x4,r3`).

**Blast radius: 17,771 distinct coverage-stub functions** binary-wide, 5,520
distinct bitmap words, VA span `0x82260DE0..0x82BB0128` — **13.7% of the 130,033
carved `.fn` symbols.** Only 147 are currently paired to tracked symbols, so most
sit outside the measured denominator. VA list at
`/home/free/tmp/coverage_stub_syms.json`. Note some stubs are genuinely *live*
(`fn_82347CF8` is `bl`-called twice from `MoveMgr.s:5805,5831`), so the construct
is real retail code, not a carving artifact.

**Nothing is fixable here in source** — forging the stub body is the decisive
negative already recorded in `docs/plans/instrumentation-patcher-experiment.md`.

#### 7.3.1 ★ Lane-lead CORRECTION: the "+11 guaranteed flip" does not hold

The worker found 11 map entries with a duplicate shape — one stub VA and one
*real* VA whose body already matches byte-exactly — e.g.
`?GetTotalGemsSmashed@AccomplishmentProgress@@QBAHXZ` at stub `0x8244c89c` and
real `0x8258f518`. It proposed deleting the 11 stub-VA entries as "**+11 strict,
0 risk**". **Re-verified against the lane baseline: that is not correct.**

```
100.0  size= 8   default/band3/meta_band/AccomplishmentProgress   ?GetTotalGemsSmashed@…
 25.0  size=32   default/AccomplishmentProgress                   ?GetTotalGemsSmashed@…
```

The symbol is **already at strict 100.0** in the real unit and therefore already
counted. `default/AccomplishmentProgress` is a **duplicate split**
(`config/45410914/splits.txt:11553` duplicates `:8453`; both drive the same base
obj) holding 18 functions of which only 3 are at 100. Deleting a stub-VA map
entry leaves that target function as an unpaired `fn_`, so the duplicate unit's
row drops from 25.0% to unpaired — **it does not become 100.** Expected delta is
**0**, not +11. Removing the duplicate split instead is denominator hygiene and
costs **−3** by (unit,name) keying, **0** by name-only.

> **Process note.** This is exactly why every worker claim is re-verified against
> the lane's own baseline pickle rather than taken at face value. The worker's
> *diagnosis* (11 genuine duplicate map entries, a real defect) was correct and
> valuable; its *arithmetic* was not. Diagnosis and delta must be checked
> separately.

Handed to the map lane as a **hygiene** item, not a flip: 11 duplicate stub-VA
entries plus the duplicate splits for `AccomplishmentProgress`,
`AccomplishmentSongConditional`, `AccomplishmentDiscSongConditional`,
`AccomplishmentConditional`, `AccomplishmentPanel`. Also unclaimed and adjacent:
`Accomplishment`'s own real accessors sit in a **splits gap** —
`fn_82594580`/`fn_82594590`/`fn_825945A0` (`lbz r3,0x4c(r3); blr`) in
`build/45410914/asm/auto_03_82594580_text.s`, between pinned ranges
`0x82594548..0x82594580` and `0x825945A8..0x825945B0`. Gap-filling only pays
combined with map repointing, so it belongs to the same hand-off.

Two riders worth separating out:
* `?PostSave@Object@Hmx@@UAAXAAVBinStream@@@Z` in `default/TexLoadPanel` is the
  one non-stub member of the 25.0 bucket — a genuine coincidence rider.
* `?PreInit@NgRnd@@UAAXXZ` was listed in the brief's 25.0 bucket from a stale
  snapshot; it now reads **88.0** (6 deleted instructions — a missing
  `RegisterFactory` call for a fourth Ng class, `fn_82B86F78`/`fn_82B86FF8`, in
  `src/system/rndobj/Env_NG.cpp`). Unrelated to the cluster, but a cheap
  independent lead.

**And a risk to the headline number, flagged not confirmed.** Auditing the 390
`??_G` thunks currently reading **100.0**: 280 self-consistent, 71 with an
unnamed `fn_` callee (indeterminate), and **~39 named-vs-named cross-class
callee mismatches** (`AccomplishmentConditional`↔`AccomplishmentTrainerConditional`,
`MsgSource`↔`HamCamTransform`, `BeatMaster`↔`MetaMusic`, `NgRnd`↔`RndEnviron`).
Some are certainly benign ICF folding — `ObjRefConcrete<T>` and
`_List_base<T>::clear` genuinely fold — but the cross-class ones cannot all be.
**Shape degeneracy pays out in both directions: it can manufacture false 100s as
well as false near-misses.** Worth a dedicated audit; not quantified here.

## 7.4 ★★ THE FLIP: `_M_insert_overflow_aux` — one line, 28/28 of the cluster

**29,474 → 29,502. +28 gained, 0 lost, identical by (unit,name) and by name
alone.** Independently re-verified by the lane lead against its own baseline
pickle. **All 28 gains are `vector<T>::_M_insert_overflow_aux` instantiations
across 26 units** — i.e. the entire S=100 cluster flipped, and nothing else
moved.

Branch `laneY-stlv`, commit `94f306cb`, one file, net +9 lines.

**The cause.** Retail's `StlNodeAlloc<T>::allocate` computes the byte count into
a **named local** before calling the pool dispatcher — exactly as `deallocate()`
directly below it still does, and as DC3's `StlAlloc.h` does for both. Our
`!HX_NATIVE` branch had collapsed it to a single expression:

```cpp
-  return reinterpret_cast<pointer>(MemOrPoolAllocSTL(count * sizeof(T)));
+  int size = count * sizeof(T);
+  return reinterpret_cast<pointer>(MemOrPoolAllocSTL(size));
```

MSVC `/O1` homes that local into the **caller's** frame when `allocate` inlines,
so retail emits one extra `stw <bytes>, __new_start$(r31)` immediately before the
allocator call — a dead-on-arrival home store sharing `__new_start`'s slot.
Confirmed at the `/FAsc` listing level on `vector<LevelData>` (Synth.cpp): the
frame map names slot `0x58` `__new_start$`, and retail's unwind funclet
`fn_82B5CD70` reads `0x54`/`0x58`/`0x5c` as `__new_finish`/`__new_start`/`__len`.

**This is the §5.2 discriminator paying off exactly as specified.** The cluster
was flagged `STRUCTURAL`; the two-member witness test showed a byte-identical
mismatch profile (same `delete: stw r24, 0x58, r31` at index 26, same six
arg-diffs) across `Synth` and `LightPreset`; one edit flipped **28 of 28**.

Two corollaries worth keeping:

* **§24 of the funclet doc is vindicated again.** The whole-function r23↔r24 swap
  was a *downstream symptom* of the missing store consuming a callee-saved
  register — which is why the diff read as "1 delete + 6 register swaps" rather
  than a plain size delta. Triaging it as "regswap ⇒ at_limit" while the size
  delta was non-zero would have killed a +28.
* **The negative control held.** All 19 `_M_insert_overflow_aux` instantiations
  that already read strict 100.0 are still at 100.0. The edit was safe precisely
  because it *restored* the form the sibling `deallocate()` already had, rather
  than inventing one.

### 7.4.1 Mispairing was explicitly ruled out for this cluster

Because §7.2 had just shown that look-alike STL clusters can be pairing defects,
the worker was asked to disprove it here before touching a shared header, and
did:

* Retail's unwind funclet `fn_82B5CD70` reads exactly the three slots MSVC's own
  `/FAsc` listing names `__new_finish$` / `__new_start$` / `__len$` for the same
  template, and recomputes `mulli r3, r11, 0x1c` = `sizeof(LevelData)` = 0x1c,
  agreeing with the parent's `li r10, 0x1c` / `divw`.
* Negative-control member `vector<RndMesh::Face>` is **102/102 instructions equal
  end-to-end**.

A mispair does not produce byte-exact agreement in one member and a single
structurally identical surplus store in another. **Rule of thumb the lane is
leaving behind: a cluster is mispair-suspect when its evidence is immediates, and
mispair-clear when its evidence is a structural insert/delete that lands at the
same index with the same register in every member.**

### 7.4.2 Not fixed, and one deliberately unbundled lead

**The S=103 group (14 members) is only half-fixed.** They take the same shared
store, but each also carries three `off:-128` immediate diffs at indices
7/24/50 (`li sizeof`, `mulli sizeof`, `addi sizeof`) — a per-type `sizeof` drift
of exactly **128 bytes** in `DynamicPropertyEntry@Flow`, `EyeDesc@CharEyes`,
`TransformArea`, `NavItem@HamNavProvider`, `PracticeStep`, `CamShotFrame`, …
The **uniformity of −128 across unrelated types** is itself suspicious — a real
per-type layout drift would not be constant. Route to the map lane alongside
§7.2 rather than funding 14 blind struct fixes. (`EyeDesc@CharEyes` is in both
this list and the §7.2 contradiction, which is corroborating.)

**Secondary lead, deliberately not bundled.** Raw (un-normalized) match for the
flipped members is 99.7%, not 100%, because retail's `_vector.c:84` calls
`_Param_Construct<T,T>` where ours calls `_Copy_Construct<T>`. Both exist at
**distinct** retail addresses (`0x82b5bda0` vs `0x82b5ad48`), so they are *not*
ICF-folded — normalized scoring simply masks the relocation. Changing
`src/system/stlport/stl/_vector.c:84` to `_Param_Construct` would make these
byte-identical, but it does not move the strict metric and would perturb 40+
instantiations. Left as a separate, separately-measurable job.

## 7.5 ★ Measured flip rate of the wave

**9 clusters worked, 1 flipped, +28 strict, 0 regressions. Whole-binary
29,474 → 29,502.** Four subagents plus the lane lead.

| cluster | n | verdict | outcome |
|---|--:|---|---|
| `?_M_insert_overflow_aux@` **S=100** | 28 | **ONE CAUSE** | ★ **+28, 28/28 flipped** |
| `?_M_insert_overflow_aux@` S=103 | 14 | ONE CAUSE + per-type drift | not addressable from the header |
| `Accomplishment` accessors 17.5 / 25.0 | 45 | ONE CAUSE | retail coverage stubs — unfixable in source |
| `??_G` S=2 / S=1 | 53 | **MISPAIR** | handed to the map lane |
| `?SetObj@?$ObjRefConcrete<T>` S=105 | 5 | ONE CAUSE | callee-tail codegen artifact; `at_limit` |
| `?resize@?$vector<T>` S=340 | 5 | SHARED MECHANISM | per-element `sizeof`; map-suspect |
| `?_M_fill_insert@` S=302 | 10 | SHARED MECHANISM | ditto |
| `??4?$ObjVector<T>::operator=` S=160 | 5 | SHARED MECHANISM | direction flips |
| `??$__uninitialized_fill_n@` S=3 / `??$_Destroy_Range@` S=1 | 12 | COINCIDENCE | unrelated immediates |

**The distribution is the result.** One in nine clusters was a fundable shared
cause — but that one paid **+28 from a single line**, and the discriminator
identified it *before* any build. Lane X's 13/13 and 18→1 were not luck; they
were structural clusters, and so was this one. Everything that looked comparably
large but scored ARG-ONLY was a coincidence or a map mispair.

> **Routing rule for the next lane.** Never open a cluster without first running
> the two-member discriminator: objdiff two members from unrelated units and
> compare the mismatch **instruction, index and direction**. Fund only
> `confidence: STRUCTURAL` clusters whose witnesses agree on all three. Two tool
> calls settle a 30-member cluster; this wave spent four worker-sessions to learn
> what the discriminator now answers for free.

Other value banked at zero delta: the re-runnable scan itself; the honest
denominator (~14% of the named sub-100 pool is high-confidence shared-cause, not
the ~28% raw clustering or ~64% whole-pool figure); a new map-mispair detector
(`--mispair-check`, 583 candidates) with proof that a whole family of "layout
bugs" is a pairing defect; corrections to two standing project beliefs (§6.6/§7.2
struct-stride, §6 `RndLine::Save`); and the closure of lane X's last open one-off.

## 7.6 ★ The model predicted its own residual — re-run confirms

Re-running the scan on the post-flip build (**29,502**) closes the loop exactly
as the model says it should:

| | before (29,474) | after (29,502) |
|---|---|---|
| named sub-100 pool | 1,830 | **1,802** (−28, exactly the flips) |
| `score_shape` clusters | 37 / 293 fns (16.0%) | 36 / 266 fns (14.8%) |
| `?_M_insert_overflow_aux@` S=**100** | 28 members | **gone** |
| `?_M_insert_overflow_aux@` S=**103** | 14 members | — |
| `?_M_insert_overflow_aux@` S=**3** | — | **15 members**, `ARG-ONLY` |

The S=103 group became **S=103 − 100 = 3**: the shared *structural* penalty was
removed by the one-line fix, leaving precisely the three `sizeof` immediate
diffs. The tool re-labels the residual `ARG-ONLY / likely MAP MISPAIR` **without
being told** — which is the correct routing for it (§7.4.2).

This is the strongest single validation of the whole framework: the penalty
decomposition is not a heuristic dressing on a percentage, it is arithmetic, and
it predicts what a fix will and will not remove **before** the build.

## 8. Re-running

```bash
cd <worktree>
python3 scripts/harvest/identical_pct_cluster_scan.py --axis all --with-sizes --top 40
python3 scripts/harvest/identical_pct_cluster_scan.py --census --include-anon --min-pct 0
```

Re-run after **every** wave: as bodies flip, clusters shrink and new ones
surface, and the `--siblings` column moves. 0.2 s, or 2 s with `--with-sizes`.
