# W16-NS — the 50 register/scheduling-only rows with no per-row record (2026-10-02)

**Branch** `w16-ns`, off main `a01ba367e`. Fork branches `w16-ns-A` and `w16-ns-B` are merged into it with `--no-ff`.
**Ruler** `name_check` (graded, from `report.json` `provenance.diff_config`). **The permuter was not run.**

`CAMPAIGN_STATE_2026-10-02.md` §4/§5.3 lists P1 + P2 rows with no per-row adjudication record: **50 rows / 16,040 B**.
The list is W16-NP's `~/tmp/w16np/final.json`, filtered to `k ∈ {P1, P2}` and no `adj`. On a fresh build of `a01ba367e`, all
50 rows were at the same fuzzy and `mpn` W16-NP recorded (`~/tmp/w16ns_rows.json`).

**12 of the 50 already had a per-row record that W16-NP's overlay missed**, because it joins non-W16-NA records on short
names. All 12 are in W16-ND §4's "rows left" table: `WorkVerts::SetSameVerts`, `StickerProvider::SetStickers`,
`BuildSphereStratified`, `PackVector`, `InterpTangent`, `ctr_encrypt_fast`, `DxShaderMgr::SetPConstant`, `~RndFont`,
`fn_822DB278`, `_S_sort<BSPFace>`, `__partial_sort<GameGem>` and `list<Instance>::operator=`. They were not reopened here.
So the truly unopened set was **38 rows**.


## 1. Whole-branch A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-ns-ab --patch <git diff main w16-ns -- . ':!docs'>`
- **Worktree:** fresh, made with `scripts/setup_worktree.sh` at main `a01ba367e`.
- **Patch:** 11 files, `+37/−40`: one `objects.json` flag (configgen) and ten source files. No map, splits or `symbols.txt` edits.
- **objdiff-cli:** sha `c1b7d952`, stable across legs.
- **Run dir:** `~/tmp/wt-w16-ns-ab/.ab_measure_runs/20261002-172102-ab_branch-1611921/`.

```
leg A: matched=51448 masked=24631 honest=26817 code%=54.725150  (recompiles: 0, settled)
leg B: matched=51450 masked=24631 honest=26819 code%=54.769770  (recompiles: 114, split=0, patch_steps=6, settle iterations: 2)
Δmatched=+2  Δmasked_equal=+0  Δhonest=+2  Δcode%=+0.044620pp  Δcode_bytes=+4572
Δfuzzy=+0.000177pp   (legA 60.393993 -> legB 60.394170)
unit net (ALL units) = +2   vs whole-binary Δmatched = +2
units at 100% [mpn ruler]: legA 482 -> legB 483  (RGUtl reached 100, 0 fell off)
units at 100% [all-rows-fuzzy ruler]: legA 429 -> legB 429
```

**Prediction, written before the run:** +2 functions / +4,572 B. That is the sum of the in-tree row diffs: keygen flag
+1,888 B / +0 fns (measured on the unit), my two rows +424 B / +1 fn, fork A +992 B / +1 fn, fork B +1,268 B / +0 fns.
**Measured: +2 / +4,572 B.**

**Row-level diff of the archived legs** (`~/tmp/w16ns/rowdiff.py`): **26 rows up, 0 rows down**, no rows appeared or
vanished. The 26 are the 23 listed rows that moved (§2, §3), plus `KeyChain::getKey`, `random` and `getMasher` in keygen.
The two functions are `AddChordLevel` and `RndText::GetStringWidthUTF8` reaching `mpn` 100.

## 2. keygen_xbox: thirteen "register-only" rows were a compile flag

Thirteen of the 38 rows sit in `keygen_xbox.cpp`, a TU compiled `/Od`: `swap`, `roll`, `mash`, `asciiDigitToHex`,
`parseHex16`, `shuffle1`–`shuffle6`, `memcpy_cs` and `revealKey`. Every charge had the same shape. Retail reuses r11 for each
temporary (`lwz r11,c1; lbz r11,0(r11); stb r11,tmp`), and ours hands out a fresh volatile register per temporary
(`lbz r10,0(r11)`, then r9, r8, ... down to r3). An earlier attempt (`get_attempts`, Sonnet) had filed all six shuffles AT_LIMIT
as a "systemic register-numbering wall".

- **Flags tried on the real unit** (`~/tmp/w16ns/kgflags.py`: edit `extra_cflags`, `configure.py`, full build, read
  `report.json`): `/Od`, `/Od /Oi-`, `/Od /Ob1`, `/Od /Oi- /Ob1`, `/Od /Oi- /EHs-c- /Ob1` (the Quazal sets). **All five were
  identical**: 156 of 2,452 B at fuzzy 100. The log shows keygen recompiled, so this was not a stale object.
- **Direct `/FAs` compile of `swap()` under ten flag sets** (cl 10224): `/O1 /Oi /Od`, `/Od`, `/O1 /Od /Ot`,
  `/O1 /Od /Ob0`, `/O1 /Od /Oy-`, `/O2 /Od`, `/Od /Ot`, `/Od /Gy`, `/Od /GF` all gave the descending chain. **Only `/Od /Os`**
  (with `/Os` after `/Od`) gave retail's `lbz r11,0(r11); stb r11,tmp`.
- **Mechanism.** The base cflags put `/O1` (which implies `/Os`) before the per-TU `/Od`, and `/Od` cancels `/Os`. Retail's keygen
  object was evidently built with `/Os` still in force. Under `/Od`, `/Os` decides whether the allocator reuses a freed
  temporary register.
- **Result** (`objects.json` `extra_cflags: ["/Od", "/Os"]`, commit `3e1f0bdb3`): unit 156 → **2,044 / 2,452 B**. All 13
  listed rows plus `KeyChain::getKey` (98.8, not in the list) reach 100. `random` 78.3 → 83.3 and `getMasher` 81.5 → 84.8 improve
  without crossing. Every row was already at `mpn` 100, so this is +1,888 B and +0 functions.
- **Negative control: the lever is not a blanket rule.** Appending `/Os` to the six Quazal `/Od` TUs (`MD5`,
  `KeyedChecksumAlgorithm`, `DuplicatedObject`, `MemoryManager`, `BandwidthCounter`, `ChecksumAlgorithm`) cost
  **−7,680 B / −3 functions** (MD5 6,792 → 0 B). Those TUs really are bare `/Od`, and that was reverted. ⇒ **The `/Od` island is
  not one flag set: Quazal is `/Od [/Oi- /Ob1]`, keygen is `/Od /Os`.** A per-TU `/Od` population with an r11-reuse
  versus descending-chain charge should be tested for `/Os` before anyone spells source.
- `random`'s residual is insert/delete (retail materialises `s_seed`'s full address with `addi` and then accesses `0(r)`; ours
  folds `@l` into the access). That suggests a different linkage for the seed. It is a named insert/delete row, so it belongs
  to W16-NQ and was not edited.

## 3. Levers that closed or moved the other rows

| row | B | before → after | lever |
|---|---:|---|---|
| `TrackData::FillChannelListWithInactiveSlots` | 424 | 99.906 → **100** | **fresh local instead of a reused one**: the push loop reused `i6` as the per-iteration bit. `int bit = 1 << i` gives retail's `and.` operand order. The bare `i5 & i6` swap was inert. `i6` is dead after the masking, so the meaning is unchanged. |
| `AddChordLevel` | 488 | 97.582 → 99.221, **mpn → 100 (+1 fn)** | **split the side effect out of the return expression**: `++i4; return i4 < i3 || i3 == -1;` stores before the `cmpw`, as retail does. `return i4++ + 1 < i3 ...` compared first. Residual: `bufIdx` is r31 in retail and r8 in ours (pure colouring; the body has no calls because `strcat`/`strlen` are inlined under `/Oi`). Declaring `bufIdx` first was inert; initialising it before the loop cost 1.9 pp. |
| `RndMat::Copy` (fork A) | 700 | 99.886 → **100** | `const Transform&` to `c->mTexXfm` before the assignment (W16-ND's BandConfiguration lever: source address first) |
| `DxRnd::SetDefaultRenderStates` (fork A) | 292 | 99.863 → **100** | **call the XDK inline instead of open-coding it**: the loop hand-wrote `D3DDevice_SetSamplerState_MipFilter3` (fetch word 3 at `0x48C`, bits 23–24, pending mask[3]). `stage_offset + device` was inert. |
| `RndText::GetStringWidthUTF8` (fork A) | 392 | 97.908 → 99.949, **mpn → 100 (+1 fn)** | define `us8` after `Style *style`. One compare is left (retail tests r30, ours r7). Defining `style` after the `cc2` block scored 95.867; testing `styleIn` directly was inert. |
| `NgEnviron::Select` (fork A) | 1756 | 99.269 → 99.369 | write the fade planes per member instead of through `Vector4::Set`, which also fixes the `fadeRefDot` `fmadds` order. Every remaining slot was traced and holds the same value. What is left is Vector4 store order at fog/ao/tone and an r27/r28 colouring in the projLights loop. `Set()` there was inert, per-member x/y/z/w scored 96.841, `projLightIdx` outside the for-init 98.863, and a named element pointer was inert. |
| **`ObjPtrList<CharCollide>::sort<ByRadius>` (fork B)** | 128 | 99.688 → **100** | **BEHAVIOUR BUG.** Retail's comparator is `c1->Radius() > c2->Radius()`, so the list sorts largest radius first. Ours had `c2 > c1`, so `BandCharacter`'s collide list sorted ascending. The `sort` instantiation in CharBoneDir matches at 100, which confirms the template calls `cmp(next, cur)`. Fix in `char/CharCollide.h`. |
| `Character::DrawShadow` (fork B) | 384 | 99.688 → **100** | **write the same expression at each use**: `-1.0f / plb0.b` at the three uses (the compiler CSEs them) gives retail's `fmuls` order. The named `scale` local with swapped operands was inert. |
| `BandPatchMesh::Render` (fork B) | 756 | 99.497 → **100** | an explicit `RndTransformable *trans = patch` for `SetLocalXfm` / `DirtyLocalXfm().Reset()`. A named `Transform&` was inert. |
| `TrackDir::SetSlotXfm` (fork B) | 336 | 99.167 → 99.405 | `const` reference to the source slot (99.286), then a `std::vector<Transform>&` for `vec2` (99.405). Only r29/r30 colouring is left; both sides copy `mSlots[i]` into `vec2[i]`. An explicit destination reference was inert, a reference to `mSlots` too gave 99.286, and inverting the outer branch gave 76.25. |

## 4. Rows left, with what was tried

| row | B | verdict | tried |
|---|---:|---|---|
| `MakeSessionJob::IsFinished` | 300 | permuter-only (block placement) | Retail places the cross-jumped `stb mSuccess; li r3,1; epilogue` tail after the error block, so the join-state path branches back into it. Ours puts it at the end. Values are identical. Explicit `return true` in the error block: inert. Same in the join-state path: inert. Success-first `if (res == 0) {...} else {error}`: 75.52. |
| `MakeTangentsLate` | 952 | permuter-only | `*(begin()+i)` inert; named reference for the copy 99.454 |
| `DxMesh::SetTransforms` | 448 | permuter-only | `hasFur` declared early (inert), structured `if (boneCount < 1)` instead of the goto (inert), `bool hasFur = fw > 0` 96.652 |
| `FixVertOrder` | 448 | permuter-only | `*(begin()+f)` inert; `dst->Faces()[f]` 96.714 |
| `RndXfmCache::GetXfms` | 96 | permuter-only (SCHED) | retail tests `start` first and loads both elements before the first compare; reordering the `||` 73.958, `&&` form 73.958, named locals after the guard 42.708 |
| `DxCam::ProjectZ` | 104 | permuter-only (SCHED) | product order swap inert, named subtraction locals inert. A regrouped spelling reached 82.885 but changes FP rounding, so it was not kept. |
| `IPP::Add_InPlace`, `IPP::Mul_InPlace` | 48 ×2 | permuter-only (SCHED) | retail loads `f2[i]` before `f1[i]`: `f1[i] + f2[i]` inert, named `f2[i]` local inert, `f2[i] = f2[i] + f1[i]` inert, pointer walk 66.25 |
| `SongSectionController::ResetAll` | 192 | permuter-only (SCHED) | retail stores the `-1` before loading the list head; named mapping reference and an early iterator declaration both inert |
| `CharClipSet::LoadCharacter` | 480 | permuter-only (SCHED) | argument setup order around the second dynamic cast; named result temporary inert, `ObjPtr&` 96.04 |
| `PatchDir::LoadStickerData` | 732 | permuter-only (SCHED) | one `mr r3`/`li r5` order at the `allow_color` lookup; dropping `!= 0` and a named `DataArray*` both inert |
| `GemTrackDir::SetPitch` | 380 | permuter-only | one `fmuls` operand order; swap, named reciprocal, y computed before copying `pos`: all inert |
| `CharWidgetImp::AddTextInstance` | 244 | permuter-only | `this`/`b2` r29/r30 swap, same values; `const bool` 97.21, if-set bool 93.92 |
| `UILabel::LabelUpdate` | 464 | permuter-only | mainfont / deferrer text pointer r27/r28 swap, same values; reversed predeclaration and named `RndText*` both inert |
| `NormalizeScale` (CharEyes) | 120 | blocker (header-bound) | the swapped `fmuls` is inside the `math/Vec.h` inline; the source records that an out-of-line copy breaks the caller (91.3 → 57.8). Not built. |
| the 12 W16-ND rows (intro) | 3,540 | as filed by W16-ND §4 | not reopened |

Every register-only charge on the rows left was traced to its defining instructions on both sides. Apart from ByRadius,
no value lands in the wrong place.

## 5. Tally

| verdict | rows | bytes |
|---|---:|---:|
| fixed to fuzzy 100 | **19** (13 keygen + 6) | **4,472** (+100 B from `getKey`, outside the list) |
| improved, not crossed | 4 (two reach `mpn` 100) | 2,972 |
| behaviour bug found | 1 (ByRadius, counted in the 19) | 128 |
| permuter-only after hand attempts | 14 | 4,936 |
| blocker (header-bound) | 1 | 120 |
| carried W16-ND record, not reopened | 12 | 3,540 |
| **total** | **50** | **16,040** |

Hand yield on the list: **4,472 B of 16,040 B (27.9%)**, or 4,472 of the 12,500 B truly unopened (35.8%). Without the
keygen flag it is 2,684 of 10,712 B (25.1%). That is in line with W16-NA/ND's 18.6% and W16-NB's 25.3%, and it confirms the
campaign doc's "hand-sweep first" call. The P1/P2 market for a permuter pilot gains these 14 rows / 4,936 B, all listed
in §4 with their spellings.

## 6. Notes for the next lane

- **Check the build mode before you hand-work a register-only row in an `/Od` TU.** A uniform descending-register chain
  against retail's single reused r11 is a flag signature (`/Os` under `/Od`), not a source one. One flag closed 13 rows
  that an earlier lane had filed AT_LIMIT.
- **Look for a reused local when an operand swap is inert.** A bare `a & b` → `b & a` swap was inert, as W16-NA measured; giving
  the reused variable a fresh name fixed the order.
- **A side effect inside a return expression schedules the compare first.** Splitting `i4++` into its own statement moved
  the store ahead of the `cmpw`.
- **Check comparators against retail's compare operands.** ByRadius's reversed comparator scored 99.688 with only register
  charges. A comparator bug reads exactly like an operand-order wobble.

## 7. Gates

GATES-PLACEHOLDER

## 8. Not done

- Not merged to main, per the brief.
- The permuter was not run.
- `random` / `getMasher` in keygen (insert/delete, W16-NQ's class) were not edited.
- The 12 rows with W16-ND records were not reopened.

Scratch: `~/tmp/w16ns/` (`kgflags.py`, `odflags.py`, `rowdiff.py`, `try.sh`, baselines `base_{A,B,C}.json`, the A/B patch and log).
