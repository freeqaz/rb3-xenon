# The real-named-at-0 stratum: 351 rows classified, two repairs landed, and the thunk-name vein re-closed

**Lane W15-B · 2026-09-14 · branch `w15-b` off `main` `8995d566`**
**Worktree `~/tmp/wt-w15-b` · ruler `functionRelocDiffs=name_check` (graded, from `objdiff.json` options)**

## 0. Result in one line

**+8 matched functions / +1,692 B / +0.016515 pp**, from two coupled repairs, both
certified by `ab_measure`. The larger one paid **entirely through its caller
cascade** and not at all through the row it renamed — and my pre-registration
missed that, in the favourable direction.

| # | thing | verdict |
|---|---|---|
| 1 | `0x8245ee48` = `RndTransAnim::MakeTransform`, not `_M_allocate_and_copy<Key<Color>*>` | **LANDED, +7 fns / +1,616 B** |
| 2 | `DataArray::Release` was `#ifdef HX_NATIVE` — no definition in the match build | **LANDED, +1 fn / +76 B** |
| 3 | W14-B's six `Save`-vs-`??_E` thunk map defects | **NOT repaired — the naive rename creates map duplicates (§5)** |
| 4 | W14-B's six `UNIMPLEMENTED_BODY` rows | **not attempted; their thunk rows are already at fuzzy 100 (§6)** |
| 5 | classification of all 351 rows | **§2; phantom carves = 0** |

---

## 1. Reproducing the stratum

351 rows / 52,268 B — 274 rows / 43,836 B `system`, 77 rows / 8,432 B `band3` —
reproduced **exactly** at the digit before any work began. Rows were selected by
`metadata.source_path` on each report unit (which `report.json` carries directly,
so no reconstruction from `objects.json` and no `default/` prefix scan was
needed), a non-placeholder name, and `fuzzy_match_percent` absent-or-0, read with
`float()` coercion because protobuf-JSON omits defaults.

⚠ **Every name-keyed step in this lane ran AFTER a full build.** A fresh
worktree's reflinked target objs are pre-renamer and every mangled name reads
"absent", which would have made the whole classification a confident vacuous
null. Asserted first: `[renamed-check] 25536/29050 map names present in 3083
target objs = 87.9%`, and `verify_objs_patched.py --verify-manifest` OK.

---

## 2. The classification

Primary instrument: read our **compiled base object's COFF symbol table** and ask
whether it *defines* the row's name (section > 0), merely *references* it
(section 0 = UNDEF/weak-external), or does not carry it at all. Then a second
index over **all 1,205 compiled objects on disk** answers whether any object of
ours defines it.

| class | rows | bytes | what it is |
|---|---:|---:|---|
| **C — NOT BUILT ANYWHERE** | 199 | 38,100 | no object in our build defines the name |
| **B — BODY IN ANOTHER TU** | 87 | 6,340 | we do build it, in a different object than the one objdiff consults here |
| **D — PAIRED AND DIVERGENT** | 65 | 7,828 | **both** sides define the name; the row is paired and genuinely matches ~0% |
| **PHANTOM CARVE** | **0** | **0** | see §3 |
| total | **351** | **52,268** | |

Secondary axis, the base-object symbol state that produced it:

| in our base obj | rows | bytes | mpn |
|---|---:|---:|---|
| ABSENT | 266 | 33,028 | all exactly 0 |
| UNDEF / weak-external | 20 | 11,412 | all exactly 0 |
| DEFINED | 65 | 7,828 | **mostly > 0** |

### ⛔ A correction to the brief's framing

The dispatch calls this stratum **"real-named at 0 (unpaired)"**. **65 of the 351
rows (7,828 B, 15.0%) are NOT unpaired.** Our object defines the name, objdiff
pairs the row, and `mpn` is non-zero for 55 of them (2.5, 1.67, 0.83 …) — they
are simply *terrible matches* whose `fuzzy` rounds to absent. They belong to the
"divergence in code we already hold" vein, not to the naming vein, and a naming
lane cannot move them. The genuinely unpaired population is **286 rows /
44,440 B**, and those are all exactly `mpn == 0`, which is the clean signature.

### The sub-shape of class B (87 rows), which is where the levers are

Mostly **COMDAT placement divergence**: retail's TU instantiated a template or
emitted an inline that our TU does not, because our source in that TU lacks the
code that references it — `??1?$ObjRefConcrete@…` dtors, `ObjectDir::Find<T>`,
`StaticClassName@Dx*`, `_M_allocate_and_copy<…>`, `ObjDirItr` increments. Each is
small (mean 73 B). Two members of this class were **genuine mis-pins** and are
what this lane repaired.

★ **Sibling evidence makes this class unusually cheap to price**: ~30 sibling
`_M_allocate_and_copy` instantiations elsewhere in the binary score **exactly
100.0**, so a row of that family that merely starts being instantiated in the
right TU tends to land at 100 on first contact — which is precisely what
`DataArray::Release` did.

---

## 3. Phantom carves: measured 0, with the direction that matters

For every row with a unique map address, checked against `.pdata` (big-endian,
per CLAUDE.md):

| geometry | rows | bytes |
|---|---:|---:|
| is a `.pdata` BeginAddress, extent EXACT | 97 | 19,216 |
| is a BeginAddress, extent +≤8 (padding) | 91 | 19,532 |
| is a BeginAddress, extent +>8 | 68 | 10,496 |
| **not** a `.pdata` BeginAddress | 95 | 3,024 |

⚠ The "extent" here is *distance to the next `.pdata` entry*, which **over-states
the function** by the inter-function padding and the next function's 8-byte EH
prefix. So the third row is an artifact of my own reader, not a defect — the same
one-sided-instrument shape STLPORT-1 recorded. **The test that actually settles
it is the SIGN**: a phantom over-carve is a row claiming *more* bytes than
`.pdata` allows. Over 256 comparable rows there are **0 negative deltas**; min 0,
max 192, and the mode is 0/4/8/12.

The 95 non-BeginAddress rows (mean 32 B) are the documented **sub-`.pdata` stub
stratum** — a 12–16 B leaf thunk touches neither the stack nor LR, so it gets no
unwind record. Not phantoms either.

⇒ **class (d) is empty in this stratum. Nobody needs to re-run it.**

---

## 4. Repair 1 — `0x8245ee48` is `RndTransAnim::MakeTransform` (+7 fns / +1,616 B)

W14-A recorded this row as a suspected wrong map name and left it. Adjudicated
on retail bytes:

- **Size**: 1,160 B, where every other `_M_allocate_and_copy` instantiation in
  the binary is 100 B — an **11.6× outlier**.
- **`.pdata`**: a real function begins at `0x8245ee48`, next entry `0x8245f2d0`,
  delta exactly `0x488` = 1,160. Not a phantom.
- **Callees**: `MakeRotMatrix(Quat)`, `MakeRotMatrix(Vector3)`, `MakeScale`,
  `InterpVector<Keys<Vector3>>` ×3, `Interp(Vector3)`, `Interp(Quat)`,
  `FastInterp(Quat)`, `QuatSpline`, `Keys<Color>::AtFrame`, `Limit`.
  *A vector allocate-and-copy does not build rotation matrices.*
- **Caller census** (whole-binary `bl` scan, from the relocation/opcode decode —
  never a linear disassembly): **11 sites, every one taking a `Transform&`** —
  `BandList::{ForceRevealed,RevealAnimPoll,ForceConcealed,ConcealAnimPoll}`,
  `RndTransAnim::SetFrame`, `RndGenerator::DrawShowing`,
  `CamShotFrame::BuildTransform`, `CamShot::SetPos`, plus a **self-call**.
- **Position**: between `StartFrame@RndTransAnim` and `SetFrame@RndTransAnim`.

Our `TransAnim.cpp:229` is `RndTransAnim::MakeTransform(float, Transform&, bool,
float)` containing exactly those calls; it recurses via
`mKeysOwner->MakeTransform` (matching retail's self-call); and `Gen.cpp:413`
calls it from the generator path (matching retail caller
`RndGenerator::DrawShowing`). Three independent confirmations.

**Preconditions checked before the edit:** our `TransAnim.obj` **defines**
`?MakeTransform@RndTransAnim@@QAAXMAAVTransform@@_NM@Z` (storage class 2,
EXTERNAL); the map contains **no** `MakeTransform` entry anywhere, so there is no
injectivity break.

### ★ This is the CIRCULAR PIN HAZARD, caught in the act

`MatAnim.cpp` pinned `0x8245EE48-0x8245F2D0` as a 1,160 B **hole punched into
TransAnim.cpp's otherwise contiguous run** (TransAnim holds `…-0x8245EE48` and
`0x8245F2D0-…` on either side). The plausible name — "Color keys", and MatAnim
animates colors — justified the pin, and the pin then corroborated the name.
**The repair is therefore COUPLED**: map rename *and* splits re-home. A
names-only edit would have left the row in a unit whose object cannot define it,
reading 0% forever.

### ⛔ My pre-registration was WRONG, in the favourable direction

Pre-registered Δmatched **0**, Δcode **0 B**, Δfuzzy +0.003…+0.010 pp — by
pricing **only the renamed row**. Certified measurement (`ab_measure --revert`,
both legs at a `symbols.txt` split fixed point):

```
Δmatched=+7  Δcode_bytes=+1616  Δcode%=+0.015775pp  Δfuzzy=+0.010075pp
units at 100%: 165 -> 165 (0 reached, 0 fell off)   total_code unchanged
per-unit: BandList +4, CameraShot +1, Gen +1, TransAnim +1
```

**The entire payout is the CALLER CASCADE; the renamed row itself contributes
0 bytes** (it lands at fuzzy 94.06 / mpn 94.80, up from 5.59, and 94 < 100 means
`matched_code` pays nothing). All 11 call sites had been charged a
relocation-name penalty under `name_check`; one rename discharged them and seven
caller rows crossed:

```
SetFrame@RndTransAnim    99.875 -> 100.000  +160 B
ForceRevealed@BandList   99.884 -> 100.000  +172 B
ForceConcealed@BandList  99.891 -> 100.000  +184 B
RevealAnimPoll@BandList  99.954 -> 100.000  +432 B
DrawShowing@RndGenerator 99.970 -> 100.000  +668 B
ConcealAnimPoll@BandList  mpn -> 100.000      +0 B
SetPos@CamShot            mpn -> 100.000      +0 B
                                     160+172+184+432+668 = 1,616 exactly
```

★★ CLAUDE.md's map/name economics says caller-spelling **DISPERSION** usually
makes the cascade structurally 0. Here every caller spelled it `MakeTransform`,
so the cascade was **maximal**. ⇒ **Price a rename from its caller census, not
from the row.** The census is also what bounds the downside, and it is the step
that turned a "recorded, not acted on" handoff into the lane's largest win.

★ Control: a per-row sweep of all **69,219** rows shows exactly one row out and
one row in, and **8 common rows changed — all of them the callers above**.
`total_code` held at 10,245,956, confirming pure reattribution.

---

## 5. ⛔ W14-B's six thunk map defects: NOT repaired, and the reason is the vein

W14-B proved the *destinations* of six vtordisp thunk rows are correct and the
*thunk* names wrong. I confirm its 6/6 split **from an independent direction it
never used**: in `report.json` the six map-defect rows all sit at **fuzzy
98.333** (12 B thunks, one charged relocation-name site), while its six
genuine-missing-body rows all sit at **100.000**. Two methods, same partition.

**But the naive repair is unsafe.** For **5 of the 6**, the name the destination
implies **already exists in the map at a different address**:

| thunk row | implied correct name | already in map at |
|---|---|---|
| `?Copy@BandTrack@@$4…` | `?SyncProperty@BandTrack@@$4…` | `0x8234ebc8` |
| `?Load@BandTrack@@$4…` | `?PostLoad@GemTrackDir@@$4…` | `0x822ee710` |
| `?Highlight@Waypoint@@$4…` | `?Load@Waypoint@@$4…` | `0x822cb008` |
| `?Save@RndMultiMeshProxy@@$4…` | `?SetType@RndMultiMeshProxy@@$4…` | `0x82481390` |
| `?Save@CrowdMeterIcon@@$4…` | `?PostLoad@CrowdMeterIcon@@$4…` | `0x822bae88` |

A names-only edit therefore creates **duplicate map names** — an injectivity
break. These are a **coupled permutation**, exactly W14-C's 6-cycle shape, and
W14-C's verdict applies unchanged: *half is worse than none*. This is also
CLAUDE.md's standing rule demonstrating itself — **proving a name wrong does not
make renaming safe.**

### The tree-wide census I built instead

A vtordisp thunk is **self-witnessing**: it forwards *by definition* to the
method it names. Scanning `.text` for the exact 3-word shape
(`lwz r11,-4(r3)` / `subf r3,r11,r3` / `b dest`) finds **1,302 thunks**, 1,127
with a map name, 1,024 with both ends named:

| relation of thunk name to destination name | n |
|---|---:|
| **AGREE** (same identity core) | **788** |
| same class, `??_E`/`??_G` destructor family | 155 |
| same class, different method | 47 |
| different class entirely | 34 |

★ **788 agreeing is the control that the instrument can pass** — and it was
needed. My first version reconstructed the expected mangling by string
substitution and scored **0 / 1021 agreement**, which reads like a devastating
finding and is simply a broken reconstruction (it dropped the calling
convention). *A check that cannot distinguish broken from correct is not a
control* — W14-B's lesson, hit again one lane later.

⛔ **And the 236 disagreements CANNOT be convicted by destination name alone.**
ICF folds identical bodies and the survivor's name is arbitrary, so a *correct*
thunk whose destination was folded reads as a disagreement. The 155 `??_E`→`??_G`
rows are almost certainly exactly that and are **not** defects. Convicting any of
the remaining 81 requires per-row body adjudication of the kind W14-B did for two
of them. **I did not do it, and I am not reporting the 81 as defects.**

---

## 6. W14-B's six `UNIMPLEMENTED_BODY` rows — confirmed, not attempted

`BandList::Save` (388 B), `WorldInstance::PreSave` (136 B),
`BandScoreboard::Save` (128 B), `OverdriveMeter::Save` (96 B),
`BandSwatch::Save` (80 B), `EventAnim::Save` (68 B). Their **thunk rows are
already at fuzzy 100** — the thunk is fine; what is empty is the body at the
destination. Each is a real per-function decomp job against retail bytes, the
destinations are unnamed (so the call sites are placeholder-**forgiven** and
writing the bodies is expected to be **Δ0 on the metric**), and W14-B's ordering
rule stands: **fix the body first, then name** — naming first converts a forgiven
site into a charged one and costs bytes.

---

## 7. Repair 2 — `DataArray::Release` was `#ifdef HX_NATIVE` (+1 fn / +76 B)

The row `?Release@DataArray@@QAAXXZ` (76 B, pinned to `PropKeys`) read 0 because
**no object in our entire build defines the name.** `DataArray.cpp:23` does
define it — inside the `#ifdef HX_NATIVE` block opened at line 16 for native-only
support (`<cstdlib>`, `<unordered_set>`, the `DTA_TRACE`/`DTA_VALIDATE` helpers).
The match build never defines `HX_NATIVE`, so it emitted **no body**, and every
call site — including `~DataArrayPtr()` at `Data.h:771` — compiled to a dangling
UNDEF external.

**The matching build is structurally incapable of catching this**: it compiles
and never links. That is the same class CLAUDE.md cites as the reason the native
gate exists.

Retail emits it out-of-line at `0x82270510` with **1,882 `bl` call sites** — the
most-called function in the stratum. The name is independently corroborated:
W14-A's Tessellate callee table resolved `fn_82270510` to
`?Release@DataArray@@QAAXXZ` from a completely different direction.

**Coupled again**, because the address was also mis-pinned: `PropKeys.cpp` pinned
`0x82270438-0x82270580`, a cluster of three shared COMDATs of which **none is a
PropKeys function** (`ObjectDir::Find<Object>`, `DataArray::Release`,
`DataNode::~DataNode`). Un-gating without the re-home leaves the row unpairable;
re-homing without the body pairs against nothing.

Pre-registered Δmatched **0 or +1**, Δbytes **0 or +76**. Certified: the upper
branch — the row pairs at **fuzzy 100.0 / mpn 100.0 on first contact**.

```
Δmatched=+1  Δcode_bytes=+76  Δcode%=+0.000740pp  Δfuzzy=+0.000736pp
per-unit: DataArray +1     total_code unchanged
```

⚠ There is **no caller cascade here** and there was never going to be: our call
sites already emitted the correct *name* as an UNDEF reference, so `name_check`
was already satisfied at all 1,882 of them. Only the row itself moved. The two
repairs in this lane pay by opposite mechanisms, which is worth remembering when
pricing the next one.

---

## 8. What I did NOT do

- **Did not repair any of the 81 adjudicable thunk-name disagreements** (§5),
  including W14-B's six. They need per-row ICF adjudication and at least five are
  coupled permutations. **This is an escalation candidate, not a closed vein.**
- **Did not write any of the six missing `Save`/`PreSave` bodies** (§6) or
  W14-B's four Xbox-only bodies. Real decomp jobs, expected Δ0.
- **Did not touch the 65 PAIRED-AND-DIVERGENT rows** (7,828 B). They are body
  divergence, not naming, and belong to a grind lane.
- **Did not chase the remaining class-C rows** (199 rows / 38,100 B). A large
  part is genuinely absent vendor source (LEAPCORE `CBaseSkin`/`CCommandManager`
  inside `UILabel`'s span, XAudio2 `FxSendReverb360::SyncEffectParams` 3,280 B,
  `FFT`'s altivec kernels 6,432 B) which the standing XDK directive puts out of
  scope. **But `DataArray::Release` proves the class is not uniform** — at least
  one member was our own source gated out — so it should not be closed wholesale.
- **Did not fix `MakeTransform`'s remaining 6%.** At 94.06 it is
  regalloc/stack-shape residual on a 1,160 B function; the permuter is off by
  standing directive.
- **Did not audit the other functions inside PropKeys' mis-pinned COMDAT block**
  (`ObjectDir::Find<Object>`, `DataNode::~DataNode`) — same shape as the two I
  repaired, and a plausible next step.
- **Did not put any finding in a source comment.** A comment asserting what
  retail does is not evidence, and a comment-only commit has broken the native
  link here before via `ScatterIncludes`.

## 9. Handoffs

1. **The mis-pinned shared-COMDAT clusters are a repeatable vein.** Both repairs
   were the same shape: an address pinned to a unit whose object cannot define
   its name. The instrument is cheap — for each report row, ask whether *any* of
   our 1,205 objects defines the name, and if one does, whether it is the one
   objdiff consults. 87 rows / 6,340 B currently answer "another object".
2. **Rank rename candidates by CALLER CENSUS, not by row size.** Repair 1's row
   paid 0; its callers paid 1,616 B.
3. **81 thunk-name disagreements** (§5), with the ICF caveat, for a lane willing
   to adjudicate bodies.
4. **`DataArray::Release` was not unique in kind.** A sweep for other
   engine-critical definitions swallowed by `#ifdef HX_NATIVE` blocks is cheap
   and this one was worth a clean +76 B and a real correctness fix.

## 10. Gates, verbatim

```
$ ./tools/ninja-locked                       # full build, never a targeted .obj
EXIT=0
[renamed-check] 25537/29050 map names present in 3083 target objs = 87.9% (floor 40%)
```
