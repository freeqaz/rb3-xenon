# UI base-class layout reconstruction (UIComponent / UIPanel) — design + plan

**Status:** DESIGN DOC for a dedicated session. The retail UIComponent layout is
**already reverse-engineered and verified non-regressing** (below). But landing it
is a **3-step effort**, not a one-shot cascade — a UIComponent agent (2026-05-29)
proved the layout fix alone registers **+0** and explained exactly why. This doc
captures the verified layout + the real sequence so the next session doesn't
re-derive it.

## TL;DR — the premise was wrong, here's the truth
"Fix the UIComponent base layout → the 79 UI near-misses close for free" is
**FALSE**. The layout is **necessary but not sufficient**, and currently
**unmeasurable** on its own:
1. **Most UI units have no pinned `.text` splits** (UIButton, UILabel, UIComponent,
   UIEvent, UIList show `0/1`, `0/12`, …). With no pinned range, no layout change
   can ever register a match there. They must be split-pinned first
   (CLAUDE.md "Splits-bootstrap recipe").
2. **The units that DO have functions need their bodies ported too.** UISlider
   `OnMsg` stays at exactly 75.0% with the *correct* layout — its residual is
   body-logic divergence (insert/delete around `DataArray::Node`/`DataNode`,
   r26↔r27 regswaps, different helper calls), not offsets. The `0x154` member
   access already matches the target, proving offsets are right.

So the layout change is a **foundation with zero immediate EV**; the
net-positive-or-revert gate correctly rejected it as a standalone edit (it built
clean, 274 compiles, but moved 0 units).

## The verified retail UIComponent layout (the durable artifact)
Reconstructed from the retail XEX via Ghidra: UISlider ctor `fn_827E47F0`, UISlider
dtor `fn_827DABC0` (destroys every member at known offsets — the gold mine), and
member-dtor identities. **Retail uses plain (non-virtual) multiple inheritance**,
NOT DC3's virtual inheritance — the root structural divergence.

Base spans (from dtor vptr stores): `RndDrawable [0x0,0x24)`,
`RndTransformable [0x24,0xd8)`, `RndPollable [0xd8,0xe4)`. **UIComponent total = 0x140.**

| offset | member | evidence |
|---|---|---|
| 0x0 / 0x24 / 0xd8 | RndDrawable / RndTransformable / RndPollable vptrs | dtor `-0x144/-0x120/-0x6c` |
| 0xe4 | `ObjPtr<UIComponent> mNavRight` (0xc) | dtor `-0x60` → ObjPtr dtor `fn_827D94D8` |
| 0xf0 | `ObjPtr<UIComponent> mNavDown` (0xc) | dtor `-0x54` → ObjPtr dtor |
| 0xfc | `LocalUser* mSelectingUser` | — |
| 0x100 | `UIScreen* mSelectScreen` | — |
| 0x104 | `UIResource* mResource` | — |
| **0x108** | **unidentified 4-byte slot** (gap before mMeshes) | inferred from mMeshes anchor — OPEN ITEM |
| 0x10c | `std::vector<UIMesh> mMeshes` (stride 0x18) | dtor `-0x38/-0x30` |
| 0x118 | `String mResourceName` (0xc) | dtor `-0x2c` → String dtor `fn_82798F98` |
| 0x124 | `ObjDirPtr<ObjectDir> mResourceDir` (0x14) | dtor `-0x20` → `fn_82260A38` |
| 0x130 | `String mResourcePath` (0xc) | dtor `-0x14` → String dtor |
| 0x13c–0x13f | `mSelected / mState / mLoading / mMockSelect` (4 bytes) | fills to 0x140 |

Then `UISlider`: `ScrollSelect base @0x140`, `mCurrent @0x14c`, `mNumSteps @0x150`,
`mVertical @0x154` (confirmed directly: OnMsg target `lbz r5, 0x154, r30`).

**Oracle:** rb3-Wii's `ui/UIComponent.h` is the faithful structural oracle (same
member set + non-virtual MI; only absolute offsets differ because Wii MWCC bases
are smaller). DC3's `UIComponent.h` is the *divergence source* (virtual
inheritance, extra members) — do not copy it.

## UIPanel — the sibling base (from the MoviePanel agent)
`UIPanel : public virtual Hmx::Object` virtual-base layout is similarly wrong:
MoviePanel members are uniformly **+4** vs target (target mCurrentMovie@0x44,
mMovies@0x48, mRecent@0x54, mMovie@0x5c). Evidence: ctor-unwind funclets
`fn_8278BA34/BA64/BA94` + `ChooseMovie fn_8278B4D0`. MoviePanel's own members match
the oracles — the skew is in the shared `src/system/ui/UIPanel.h` virtual-base
layout. ~14 MoviePanel near-misses (99.8–99.94%) flip once UIPanel is correct.
Note rb3-Wii's `ui/UIPanel.h:60-73` is self-consistent with String=0xc; DC3's is
internally inconsistent (mFocusName compressed to 8 bytes but mState still at 0x1c).

## The plan (3 phases, in order)
**Phase A — land the verified UIComponent + UIPanel layout as a foundation.**
Apply the table above to `src/system/ui/UIComponent.h` (and `UIPanel.h`). It builds
clean and is non-regressing (verified). Expect **+0** measured — that's fine, it's
infrastructure. Commit it as foundation so B/C can build on it. (Open item: nail
the 4-byte slot at 0x108 — possibly an X360-only member rb3-Wii lacks; and port
`ui/UIResource.h`, currently missing from our tree — the agent forward-declared it.)

**Phase B — pin `.text` splits for the UI units at `0/N`.** Use the
CLAUDE.md splits-bootstrap recipe for UIButton, UILabel, UIComponent, UIEvent,
UIList, etc.: run `tools/fingerprint_match.py autoid`, compute each cluster's
`[min(fn), max(fn)+size)` `.text` span, add to `splits.txt`, `touch config.yml`,
rebuild. Now layout changes can register matches in those units.

**Phase C — port the function bodies** from the rb3-Wii oracle against the now-correct
layout (UISlider `OnMsg` first — its 75% residual is `DataArray::Node`/`DataNode`
inlining + regswaps; the body must be ported, not the offsets fixed).

**Why this order:** A is correct-but-invisible until B exists; C is what actually
moves the metric. Doing A alone (as the agent did) is a verified-correct no-op.

## Risks
- Wide blast radius: UIComponent/UIPanel are bases for ~15 UI units. Validate per-unit
  (the `report.json units[]`) so a net-zero total doesn't hide a regression.
- The `DataArray::Node` out-of-line-vs-inline issue (UISlider OnMsg, and LightHue
  OnSaveDefault) is a separate DataArray-header-wide lever that also gates some UI
  bodies — see if it's worth doing alongside C.
- Interacts with the ObjPtr family migration (mNavRight/mNavDown are `ObjPtr`,
  mResourceDir is `ObjDirPtr`) — do the ObjPtr re-layout (bug #1) FIRST or the UI
  ObjPtr-member ctors won't fully match.

## Refs
- `docs/plans/next-wave-onediff-clusters.md` (UIComponent lever entry — points here).
- `docs/plans/objptr-family-relayout-migration.md` (do first — UI uses ObjPtr/ObjDirPtr).
- Ghidra: UISlider ctor `fn_827E47F0`, dtor `fn_827DABC0` (member-offset gold mine),
  ObjPtr dtor `fn_827D94D8`, String dtor `fn_82798F98`, ObjDirPtr dtor `fn_82260A38`;
  MoviePanel ctor-unwind `fn_8278BA34/BA64/BA94`, `ChooseMovie fn_8278B4D0`.
- Oracles: rb3-Wii `~/code/milohax/rb3/src/system/ui/` (faithful), DC3
  `~/code/milohax/dc3-decomp/src/system/ui/` (divergent — virtual inheritance).
- Memory: `[[project-engine-baseclass-layout-wall]]`.
