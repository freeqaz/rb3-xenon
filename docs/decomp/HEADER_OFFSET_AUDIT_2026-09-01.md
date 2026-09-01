# Tree-wide header offset audit — 2026-09-01 (re-run + adjudication)

> **STATUS: CURRENT.** Supersedes
> [HEADER_OFFSET_AUDIT_2026-08-18.md](HEADER_OFFSET_AUDIT_2026-08-18.md), whose
> artifact was produced by a buggy `audit_header()`. Raw findings:
> `header_offset_audit_2026-09-01.json`; per-class triage:
> `header_offset_triage_2026-09-01.json`.
> Tools: `tools/header_offset_audit_par.py` (sweep),
> `tools/header_offset_adjudicate.py` (target-binary triage).

## 1. The fourth instrument defect, and why the old artifact was retired

`audit_header()` built its `{member: offset}` map with a plain `setdefault` over
the compiler's **offset-ordered** member list. That list contains every base
sub-object's members, and a base member always sorts first, so for any derived
class that redeclares a base member's *name* the base won — while the comment
being audited sits in the **derived** class's body.

`--fix-header` writes rows back. Running the fixer on this tree before the fix
would have rewritten correct comments into base-class offsets.

Found on dc3-decomp (`21db38da9`) while porting this repo's tooling *there*,
where 47 of 176 raw rows were this family alone. Ported back in `f68f5276`.

**Negative control, this tree, same class, same compile:**

```
old tool:  WRONG line 201: mTarget commented 0x2dc but is really 0xcc
new tool:  WRONG line 201: mTarget commented 0x2dc but is really 0x284
```

`src/system/world/Spotlight.h:201` is `ObjPtr<RndTransformable> mTarget; // 0x2dc`
and `RndTransformable` has its own `mTarget` at `0xcc`. `0x284` is the only value
consistent with the neighbours (`mLightCanOffset 0x280`, `mSpotTarget 0x294`).

**A/B over the identical cached layouts** (2,123 classes, one phase-1 sweep, two
phase-2 passes): OLD 3,268 rows / NEW 3,262 rows; **23 rows exist only under the
OLD rule** — every one of them a row `--fix-header` would have written to a base
offset — and **17 rows exist only under the NEW rule**, drift the old rule hid.
Six of the 23 are on comments that are **correct**, i.e. would have been
*destroyed*: `SyncGameStartPanel::mState 0x3c→0x20`,
`InterstitialPanel::mShowing 0x98→0x25`, `CreditsPanel::mLoader 0x40→0x10`,
`CreditsPanel::mPaused 0x58→0x28`, `StreamNull::mFaders 0x40→0x4`,
`Synth360::mMics 0xa8→0x44`.

Pinned by `class_layout_report.py --selftest` **leg F**, which is offline,
deterministic and *self-sabotaging*: it reconstructs the old `setdefault` rule
inline and requires it to produce the base offset, so it cannot pass on a tool
with an empty member list. 5/5 controls pass.

## 2. Denominator

| | |
|---|---:|
| TUs | **1,170** (0 compile failures) |
| classes examined | 2,123 |
| classes without a `src/` header | 6,407 (STL/CRT/XDK — no `// 0xHEX` contract) |
| headers with rows | 471 |
| **disagreeing comments** | **3,260** across **536** classes |

## 3. ⚠ What a row is, and is not

The sweep compares our comments against **our own compiler**. It has **no
target-binary input**. Every row means *"comment disagrees with our layout"*
(class A). Whether *our layout* is right (class B) needs a target witness.

`tools/header_offset_adjudicate.py` supplies that step:

| bucket | classes | rows | meaning |
|---|---:|---:|---|
| `WITNESSED_LAYOUT_OK` | 323 | 1,898 | a ≥0x40-byte function of the class is at exactly 100.0 ⇒ the binary agrees with our compiler ⇒ the comment is drift |
| `NEEDS_INSTRUCTION_WITNESS` | 20 | 93 | compiled code exists, none perfect ⇒ the comment may be the RE record and *our layout* the bug |
| `UNWITNESSED` | 193 | 1,269 | no compiled function at all (mostly `system/hamobj/**`, `system/gesture/**`) ⇒ the binary has nothing to say |

Two traps, both carried in the tool's docstring because both nearly produced
wrong fixes on dc3:

1. **`sizeof` agreement is not enough.** §5 below is exactly that case.
2. **"the target uses the commented offset" is NOT evidence on a 100%-matched
   function** — base and target are the same bytes there, so the instruction is
   addressing some *other* object.

## 4. The class-B hunt: two sweeps, one bug

**Sweep A — allocation size.** 85 flagged classes have a `?NewObject@<C>@@`
factory. **83 read exactly 100.0**; the two that do not
(`StarDisplay` 99.82, `RGTrainerPanel` 99.80) carry `li r3, 0x198` and
`li r3, 0x2a4` *identically on both sides* — their residual is relocation names.
**Zero `sizeof` errors.**

**Sweep B — comment/compiler offset pairs.** All **1,079** non-100% functions of
flagged classes instruction-diffed for a `target uses the commented offset /
base uses the compiler's` pair. **34 hits, 0 real.** Adjudicated individually:
stack-frame slots off an `r1`-derived frame pointer (NetSession::Poll,
BandDirector::Enter, VocalTrack, Spotlight, CharMeshHide, AmbientOcclusion
SmoothResults), a **vtable slot** (`RockCentral::OnMsg` — `lwz r11,0x0(r3)` then
`lwz r11,0x38(r11)` vs `0x34`: a missing virtual in a `Net` member's class, a
real bug of a different family), `RndMesh::Vert` fields inside a structurally
different `RndAmbientOcclusion::BlendVert`, and one commuted instruction pair in
`OutfitConfig::MatSwap::Compose`.

**Sweep C — comment-independent.** Every non-100% function of a flagged class
scanned for same-opcode/same-register rows differing only in a `this`-relative
displacement. 1,056 non-static functions, **102 rows**, of which one was real:

## 5. THE REAL BUG — `CamShot`'s four shake vectors (`95a62904`)

```
retail   0x128 mLastShakeOffset        0x138 mLastShakeAngOffset
         0x148 mLastDesiredShakeOffset 0x158 mLastDesiredShakeAngOffset
ours     the mirror image
```

Invisible to `sizeof` (four `0x10` vectors in either order):
`?NewObject@CamShot@@SAPAVObject@Hmx@@XZ` is **100.0%, diff score 0/2800**, with
`li r3, 0x1c8` identical on both sides. Invisible to the comments, which claim
`0x210/0x220/0x230/0x240` — a class running past `0x282` when retail's `sizeof`
is `0x1c8`.

Witness — `?Shake@CamShot@@IAAXMMABVVector2@@AAVVector3@@1@Z`, `r31` = `this`
(`mr r31, r3`, index 7):

```
 46 | lfs f12, 0x14c(r31)  | lfs f12, 0x12c(r31)
 48 | lfs f11, 0x148(r31)  | lfs f11, 0x128(r31)
120 | addi r30, r31, 0x128 | addi r30, r31, 0x148    <- the mirror
148 | lwz r11, 0x168(r31)  | lwz r11, 0x168(r31)     <- mShakeVelocity EQUAL
```

`mShakeVelocity` (`0x168`) and `mShakeAngVelocity` (`0x178`) being byte-equal on
both sides is what rules out a whole-class shift and pins the fault to those four
declarations.

`??0AutoPrepTarget@@QAA@AAVCamShotFrame@@@Z` was 100.0% *before* the fix because
its statement order had been written to compensate — it zeroed Last-first, which
under the transposed declaration produced retail's Desired-first store order.
Two errors cancelling in one function while diverging in another. Its statement
order is corrected alongside.

After: `?Shake` has **zero** `this`-relative offset mismatches (residual is
regalloc, fp scheduling, one `fsubs`/`fsub`+`frsp` pair), 95.912895 → 96.083626.
Full `ninja`: `matched_code` 3772844 and `matched_functions` 42276 unchanged,
**one** function moved, **zero** regressions.

## 6. Open lead — the `BandUser` virtual-base displacement

`?SyncLoad@RemoteBandUser@@UAAXAAVBinStream@@I@Z` adjusts `this` by `0xf4` in
retail and `0x104` in our build (`subic. r11, r3, 0xf4` vs `0x104`, then 17
further `-0xf4(r31)` vs `-0x104(r31)` pairs). `RemoteBandUser` is
`public virtual BandUser, public virtual RemoteUser`, and `BandUser`'s own audit
rows are a uniform `+12`. That is a class-B signal on a class whose vtable
history is already delicate — it wants a dedicated lane, not an opportunistic
fix. **The BandUser family's comments were deliberately NOT corrected**, so the
RE record survives.

## 7. What was written

3,155 of the 3,260 rows corrected to compiler truth (`ea6ca1c7`). Withheld: the
20 `NEEDS_INSTRUCTION_WITNESS` classes and the `BandUser` family — 105 rows.

Comment-only, proven textually rather than by a build hash (this repo has no
`obj_build_metadata_patcher`, so `tree_sha256` moves on every rebuild from COFF
`TimeDateStamp`/`S_OBJNAME` alone): masking the `// 0x<hex>` token leaves all
**3,155 of 3,155** changed line pairs identical. Full `ninja` after: every
headline measure unchanged.

`struct_db.sqlite` — what the orchestrator's `lookup_struct_offset` answers from,
and the reason these comments are not cosmetic — rebuilt: 3,035 classes / 10,821
members, **4,758 member offsets corrected**, classes with two members claiming a
single offset **56 → 34**. The file is gitignored, so rebuild it in the main
checkout after landing:

```sh
python3 tools/struct_db.py build
```

Pre-correction backup: `/home/free/tmp/struct_db.sqlite.bak-2026-09-01`.

## 8. Running it

```sh
# phase 1 (parallel compiles, ~50 min at 14 jobs) + phase 2
python3 tools/header_offset_audit_par.py --project-dir <wt> --json out.json
# phase 2 only — seconds; use after changing an audit RULE
python3 tools/header_offset_audit_par.py --project-dir <wt> --json out.json --phase2-only
# triage against the binary
python3 tools/header_offset_adjudicate.py --audit out.json \
        --report build/45410914/report.json --json triage.json
```
