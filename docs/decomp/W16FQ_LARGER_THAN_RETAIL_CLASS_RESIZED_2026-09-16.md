# W16-FQ — the "our body is LARGER than retail's" class, re-derived and resized

**Date** 2026-09-16 · **Base** `main` `20e2614e` · **Branch** `w16-fq` ·
**Ruler** `name_check` (graded), read from `report.json`'s own `provenance`
block (objdiff 4.2.9, `tool_binary_hash 5a51cd51fe0a353f`).

Brief: W16-FN proved a reusable defect class — the rb3-Wii oracle hand-expands
helpers that retail emits once and CALLS, so our body is LARGER than retail's
while the row scores ~0 despite pairing fine. W16-FB's census named that class
**C5 — paired, divergent body** at **43 rows / 5,684 B**, explicitly flagged as
UNVERIFIED with no surviving row list. My job: re-derive the population and
report what I actually measure.

**Headline: the class is real but it is CONTAMINATED, and the contamination is
the finding.** The signature "our body is larger than retail's" does *not*
isolate the FN mechanism — it also catches rows where the **map name is simply
wrong**, i.e. where retail's body is a completely different function. Those are
map work, not source work, and no amount of decomp can close them.

---

## 1. Baseline, and an instrument that has gone vacuous

Worktree built full before any measurement (a reflinked worktree's target objs
are **pre-renamer**, so every retail mangled name reads "absent" until you
build). The build reproduces W16-FN's landing figures exactly, which is the
anti-vacuity check:

```
total_functions 69240   total_code 10247068
matched_functions 44034 matched_code 4148932  matched_code_percent 40.488968
fuzzy_match_percent 50.36186  masked_equal_functions 23245
```

⛔ **W16-FB's classification instrument no longer discriminates.** FB keyed on
the *absence* of the `match_percent_normalized` key ("objdiff never computed a
diff"), and reported 263 of 316 rows lacking it. On this tree **0 of 69,240 rows
lack that key** — objdiff now emits `match_percent_normalized` unconditionally.
A classifier whose discriminating signal is present on 0% of the population
cannot fail, and would have silently reported "everything is paired".

I therefore used the instrument FB actually *validated* and which is independent
of objdiff's serialization: **the COFF symbol table of our compiled object** —
does it DEFINE the symbol, or merely reference it?

## 2. The population, re-derived (NOT inherited)

Population = named rows (placeholder prefixes `fn_`/`lbl_`/`jumptable_`/`data_`/
`bss_`/`rdata_` excluded) at `fuzzy == 0` inside a unit with an existing base
object:

| measure | W16-FB (`1d028679`) | **W16-FQ (`20e2614e`, measured)** |
|---|---:|---:|
| population | 316 rows / 46,000 B | **304 rows / 36,996 B** |
| **C5 — our obj DEFINES the symbol** | 43 rows / 5,684 B | **50 rows / 4,944 B** |

So the briefed figure is **approximately right in row count and 13% high in
bytes**. C5 is 4,944 B = **0.048% of `total_code`** — a small vein whichever way
it is counted, and that ceiling matters more than the row count.

**The "their bodies are larger" claim is 82% true, not universally true:**

| ours vs retail | rows | retail bytes |
|---|---:|---:|
| LARGER | 41 | 4,304 |
| EQUAL | 8 | 544 |
| SMALLER | 1 | 96 |

⚠ **Anti-vacuity control on the size measurement.** STLPORT-1 (2026-08-16)
proved that a "retail is smaller than us" reading was once a pure artifact of
our own COMDAT reader billing the *successor* symbol's 8-byte EH prefix into the
span — producing a uniform **+8**. I used the fixed `tools/coff_bodies_ext.py`
and checked the delta histogram for that signature: `+8` occurs **3 times in
50**, against deltas of +936, +888, +864, +692, +404… The class is **not** the
EH-prefix artifact resurfacing.

## 3. ⛔ THE CLASS IS CONTAMINATED BY WRONG MAP NAMES — the main finding

FB wrote that class 2 (wrong map name) "is not separable en masse" because a
wrong name is indistinguishable from *missing source* by the COFF check. **What
that reasoning missed is that a wrong name is ALSO indistinguishable from a
divergent body when our object DOES define the symbol** — so wrong names land
inside C5, on the favourable side of the count.

The separating instrument is cheap and mechanical: extract the **relocation
target names** from retail's body and from ours, for the same symbol, and read
whether retail's callees are plausible for the function the name claims.

Adjudicated instances, from retail bytes:

| row | retail size | retail's body actually calls | verdict |
|---|---:|---|---|
| `??DHmx@@YA?AVMatrix4@0@ABVTransform@@ABV10@@Z` | 100 | `ObjectStage` ctor, `Keys<TexPtr<RndMatAnim>,RndTex*>::Add`, `~ObjRefConcrete<RndTex>` — takes a float in `f1` | **WRONG NAME.** Not a matrix multiply by any reading. |
| `?DoPost@NgPostProc@@UAAXXZ` | 124 | `GetAward@AccomplishmentManager`, `GetCurrentGigNum@TourProgress`, `GetNumSongsForGigNum@TourDesc`, `TheTour` | **WRONG NAME.** Tour/accomplishment code, not a post-processor. |
| `?Intensity@RndLight@@QBAMXZ` | 92 | `UniqueFilename`, `~String` | **WRONG NAME.** A float getter does not build filenames. |
| `?resize@vector<RndPointTest>` | 72 | `NewObject@Object`, `StaticClassName@DxTex`, `__RTDynamicCast` | **WRONG NAME.** |
| `?_Destroy_Range<reverse_iterator<SingerStats*>>` | 72 | `_Copy_Construct<KeyFrame<EventAnim>>`, `MemOrPoolAlloc` | **WRONG NAME.** A destroy-range does not allocate. |
| `??0DrawString3D@@QAA@PBDABVVector3@@ABVColor@Hmx@@@Z` | 72 | `~ObjRefConcrete<SeqInst>` | **WRONG NAME.** |

That is **6 rows / 532 B of the 50 rows / 4,944 B adjudicated as wrong map
names**, found by inspecting only the rows I opened — it is a floor, not a
census. ⇒ **C5 = 4,944 B is an UPPER BOUND on the source-work vein.**

⚠ Consequence for anyone briefing this class: **a C5 row is not automatically
decomp work.** Adjudicate retail's callee set before spending. Doing so costs
one script run and prevents grinding source against a body that was never the
function you think it is.

### 3.1 The class is heterogeneous — at least four mechanisms, only one is FN's

Beyond wrong names, the rows I opened split into genuinely different causes:

- **FN's outlined-helper mechanism** — `ComputeElbowPullAndQuat@BandIKEffector`
  (§4). Retail calls `MultiplyTranspose`; our source hand-expands it.
- **`MILO_DEBUG` dev-build code retail compiled out** —
  `?DrawShowing@BandCharacter@@` is **68 B in retail and 1,004 B in our build**,
  and our body draws `bandcharacter.show_slot` / `bandcharacter.show_spheres`
  debug visualisations via `MakeString`. Retail's body is `DrawShowing@Character`
  plus one call. Same shape at `?NewMs@DeJitter@@` (84 B vs 488 B, ours carrying a
  `dejitter_disable` `DataVariable` path). This is the force-defined-`MILO_DEBUG`
  class CLAUDE.md warns about, **not** an outlined helper.
- **Wrong class shape (engine-version divergence)** — the `PracticeSection`
  cluster is the single biggest concentration in C5 (**4 rows / 688 B**:
  `SyncProperty` 268, `Load` 160, `Copy` 136, `Save` 124). Retail's
  `PracticeSection` loads an `ObjRefConcrete<RndTransformable>` and copies an
  `ObjRefConcrete<BandCharacter>`; ours reads `PracticeStep` vectors and
  `DancerSequence` lists. Our class has the wrong members, so all four move
  together or not at all.

⇒ **"our body is larger than retail's" predicts *a structural source
divergence*, but it does NOT predict FN's outlined-helper mechanism
specifically.** That is the honest resizing of the brief's premise.

## 4. The one clean FN-mechanism instance, and a refuted cascade

`?ComputeElbowPullAndQuat@BandIKEffector@@IAAXAAVQuatXfm@@ABVTransform@@ABVVector3@@@Z`
— retail **184 B**, ours **288 B**, fuzzy **0.000000**.

Retail's body calls `?MultiplyTranspose@@YAXABVTransform@@ABVVector3@@AAV2@@Z`
once. Our source hand-expanded it verbatim: a `Subtract` into `dx/dy/dz`
followed by the three `Dot`s, which is exactly `MultiplyTranspose`'s body
(`Mtx.h:440`). Textbook FN.

⛔ **The obvious tree-wide fix was REFUTED by a control before I wrote it.**
`MultiplyTranspose` is marked `inline` in `Mtx.h`, so the natural hypothesis was
"retail has it out of line, we inline it — move it to `mtx.cpp`", a header change
cascading to 13 TUs / 22 call sites. I counted actual `bl` relocations to that
symbol in the built objects first:

| | refs | functions |
|---|---:|---:|
| retail (target objs) | 15 | 7 |
| **ours (base objs)** | **94** | **48** |

**We already call it out of line at 94 sites** — `/O1` does not inline it, and
our `Set@BSPFace` / `Intersect` / `SetSphereBase` / `OnPointCollide` all match at
**fuzzy 100** while doing so. The header is correct as it stands; the defect was
purely local to this one function, which failed to use the helper at all. **A
13-TU header cascade was avoided by one read-only measurement.**

### The fix

```cpp
Vector3 localElbow;
MultiplyTranspose(shoulderXfm, elbowTarget, localElbow);
const Vector3 &armVec = mEffector->TransParent()->mLocalXfm.v;
MakeRotQuat(armVec, localElbow, outQuat.q);
```

plus assigning **only** `outQuat.v.x` early (retail stores it and reloads it at
idx 32; `dy`/`dz` stay in registers until the final scale), and a named `armLen`
local — retail loads `armVec.x` early, which a named local reproduces.

**Result: fuzzy 0.000000 → 92.5, mpn 0 → 95.108696, and our COMDAT went 288 B →
184 B, exactly retail's size.** Instructions 0–17 — the entire
`MultiplyTranspose` + `MakeRotQuat` prologue — are byte-exact.

### Two measured negatives on the residual

- **FP sum term order is INERT.** Reversing `dy*dy + v.x*v.x + dz*dz` to
  `dz*dz + v.x*v.x + dy*dy` measured **91.521736 → 91.521736**, identical to the
  last digit. `/fp:fast` reassociates; do not spend on FP term order here.
- **Declaration order is INERT for the register residual.** Three source
  orderings (store-first, `armLen`-local, all-pure-locals) produced 91.52 / 92.5
  / 92.5. This is exactly `MSVC_X360_REGALLOC.md`'s corrected rule — declaration
  order controls **stack slots**, registers follow **liveness and scheduling**.

The remaining 18 mismatched instructions are one coupled root cause: retail
computes `dy` first and therefore *reloads* `outQuat.v.x` from memory, where we
keep it live. That is a register-allocation residual (`REGISTER_SWAP`,
f10↔f9 / f11↔f13), which the standing directive defers rather than grinds.

## 5. PRE-REGISTERED PREDICTIONS (written and committed BEFORE the A/B)

- **P1 (row).** `?ComputeElbowPullAndQuat@BandIKEffector@@…` reads
  `fuzzy = 92.5`, `mpn = 95.108696`, base size **184** (== target) in leg B, vs
  `fuzzy = 0.0`, base size 288 in leg A.
- **P2 (metric — the modal outcome).** **Δmatched_code = exactly 0** and
  **Δmatched_functions = exactly 0.** `matched_code` is all-or-nothing per row
  and 92.5 < 100; `matched_functions` counts `mpn == 100` and 95.1 < 100. This
  lands as a **correctness + legibility** fix, not a byte fix. A Δ0 here is the
  predicted success, not a refutation.
- **P3 (falsifier — denominator).** `total_code` stays **10,247,068** and
  `total_functions` stays **69,240**. A source-only edit cannot move the
  denominator; if it moves, my model of the edit is wrong.
- **P4 (falsifier — blast radius).** `?ComputeHandPullAndQuat@BandIKEffector@@…`
  (444 B) sits in the SAME TU and calls the SAME helper. I predict it stays at
  **fuzzy 89.1982**. If it moves, the edit's reach is not what I claim and the
  attribution is unclean.
- **P5 (the set must be able to express what I do not expect).** Δmatched_code ∈
  **{0 (predicted), +184 (the row unexpectedly crosses to 100), any negative
  value (a regression elsewhere in `default/BandIKEffector`, which would falsify
  P2 and require revert)}**. I am not enumerating only multiples of my own
  target — a negative outcome is explicitly in the set.

## 6. Deliberately NOT done

- **`?PollRefresh@XboxContentMgr@@` (824 B) — untouched by directive.** It is the
  largest C5 row and lane W16-FP owns it.
- **The `PracticeSection` cluster (4 rows / 688 B) is diagnosed but not
  attempted.** Retail's class holds `ObjRefConcrete<RndTransformable>` and
  `ObjRefConcrete<BandCharacter>` where ours holds `PracticeStep`/`DancerSequence`
  containers. That is a **class-layout rewrite**, not a body port; it would move
  all four rows together and needs its own lane with a layout report. Recorded
  here so the next lane does not re-derive it.
- **`?DrawShowing@BandCharacter@@` (68 B, ours 1,004 B) not attempted.** The
  diagnosis (force-defined `MILO_DEBUG` debug drawing) is recorded, but the fix is
  a per-site `#if defined(MILO_DEBUG) && defined(HX_NATIVE)` gate whose
  whole-binary control CLAUDE.md measures at **−21** when applied bluntly. 68 B
  does not justify that risk without a dedicated control.
- **No map edits.** Six wrong map names are *identified* above but none renamed.
  Naming is a bet with zero call-site upside under placeholder forgiveness, and a
  half-adjudicated map edit is worse than none. `fn_8238DA40`
  (`HasClip@CharClipGroup`'s retail callee) and `fn_822C12F8` (`Normalize`'s) are
  likewise left anonymous.
- **No permuter run**, per standing directive.
