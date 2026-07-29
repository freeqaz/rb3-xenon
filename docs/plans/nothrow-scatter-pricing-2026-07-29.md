# Pricing the scatter-include nothrow mechanism (laneBG, 2026-07-29)

**VERDICT: CLOSE.** The mechanism is real, but its unfixed-site population is
measured at **zero confirmed, with a 95% upper bound below one function
tree-wide**. The one known instance is already fixed in main by an equivalent
technique that has been deployed for months. Do not fund a scanner.

Baseline for everything below: worktree `~/tmp/wt-laneBG-1` off main `d112d2ef`,
full `./tools/ninja-locked`, `report.cache` removed → **39,520 matched / 69,378**.

---

## 1. Is the pragma byte-neutral? NO — hypothesis (d) is refuted

Cheapest discriminator first, per the brief. Reproduced laneP's experiment on the
**current** tree (main still hosts `DataNode::DataNode(const DataNode&)` in
`DataUtl.cpp`, i.e. the relocation workaround is live):

| leg | `DataArray::Insert` (`?Insert@DataArray@@QAAXHABVDataNode@@@Z`) |
| --- | --- |
| A — main: ctor hosted in `DataUtl.cpp` | **100.0 %** |
| C — ctor moved back to `DataNode.cpp`, no pragma | **28.0 %** |
| B — ctor moved back + `#pragma optimize("g", off)` | **100.0 %** |

The pragma moves bytes decisively (28.0 → 100.0, matching laneP's 27.99 → 100.00
from 2026-07-26). **(d) is refuted; the mechanism is real.**

## 2. Why was laneP's own delta exactly 0? Cause (a) — and (a) generalises *negatively*

Its A/B (28,238 → 28,238) compared the pragma against the **already-deployed
third-TU relocation**. Both legs fix the same defect, so the delta is 0 by
construction. Its site was already matched. That is possibility (a).

(a) does not generalise into yield — it generalises into a *ceiling*, because of
a hazard laneP's own commit gets backwards.

### ★ The pragma DOES perturb the guarded callee — laneP's central claim is false

laneP's commit says the pragma restores the funclets *"without touching the
callee's own emitted code"*, and that is the whole basis for preferring it to
relocation. Measured by hashing the callee's COMDAT out of the compiled obj:

| leg | `??0DataNode@@QAA@ABV0@@Z` COMDAT |
| --- | --- |
| no pragma | **44 bytes**, sha1 `fd809efd…` |
| `#pragma optimize("g", off)` | **100 bytes**, sha1 `d4acd563…` |

A 2.3× bloat of the guarded function. The pragma is harmless *at this site only*
because the ctor is **not a tracked target symbol** (absent from
`scripts/target_symbol_map.json` and from both `obj/DataNode.obj` and
`obj/DataUtl.obj`).

This collapses the claimed niche:

- callee **untracked** → pragma is safe, **but third-TU relocation is equally
  free** and is what main already does. Marginal yield over status quo = 0.
- callee **tracked** → relocation may be impossible, but the pragma would wreck
  the callee's own match. Marginal yield = **negative**.

The set "relocation impossible **and** pragma safe" is, by this measurement,
essentially empty. The pragma is at best an ergonomic variant of an existing fix.

## 3. Independent site count: 251 live edges, not 233

`grep -rn '#include "*.cpp"' src/` on the current tree:

- **252** scatter-include lines in **160** files.
- **159** of those owner files are declared in `config/45410914/objects.json`
  (**251** live edges). The one dead owner is
  `src/system/synth_xbox/StreamReceiver.cpp`.
- Those owners map to **166** objdiff units.

laneBC's "~233" is in the right family but low; the number is **251**. It is also
a count of *include edges*, not of defect sites — which is the pricing error the
brief warns about.

Gross pool inside those 166 units: **18,888** functions, **4,233** sub-100. Of
the sub-100, only **810** are named/pairable; **3,378** are anonymous
`fn_<hex>` and can never score.

## 4. The qualified intersection — measured against a control group

The mechanism's fingerprint is exact: retail emitted an exception handler for a
function and **our build did not**. Both sides are directly observable.

- **Target side**: dtk's `build/45410914/asm/<Unit>.s` emits raw `.pdata`
  entries as `.4byte fn_XXXXXXXX / .4byte 0xPPPPPPPP`. In the X360 PPC
  `RUNTIME_FUNCTION` packed word, **bit 31 is `ExceptionFlag`** (observed
  `0xC0003104` = EH, `0x40000A04` = no EH). Joined to mangled names through
  `scripts/target_symbol_map.json`.
- **Base side**: presence of `__unwindtable$<mangled>` in our compiled obj.

"EH-deleted" = target `ExceptionFlag == 1` **and** base has no
`__unwindtable$`. This is exactly laneP's own "unwindtables 0/4" metric,
computed tree-wide. **Crucially, it is also computed on the non-scatter units as
a control** — if scatter-inclusion caused a systematic nothrow defect, scatter
units must show an *elevated* rate.

| group | units | target `.pdata` entries | EH-bearing | name-mapped | **EH-deleted** | rate | sub-100 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| **scatter-composed** | 146 | 14,778 | 2,740 | 1,655 | **44** | **2.66 %** | 37 |
| **plain (control)** | 548 | 21,302 | 3,703 | 2,478 | **88** | **3.55 %** | 73 |

Tightening to the true defect *shape* (target larger than base **and** the diff
contains target-only `delete` instructions), and dropping the target-size-316
blob that is a single ICF-folded target mapped onto 12 different base symbols
(a map mispair, not an EH defect):

| group | tight EH-deletion candidates | rate over mapped-EH |
| --- | --- | --- |
| **scatter-composed** | **11** | **0.66 %** |
| **plain (control)** | **30** | **1.21 %** |

★ **In both cuts the scatter-composed rate is LOWER than the control.**
Two-proportion 95 % CI on the excess attributable to scatter-inclusion (tight
cut): **[−1.13, +0.04] percentage points** → an upper bound of **0.04 % × 1,655
≈ 0.6 functions** tree-wide. The point estimate is negative.

There is no measurable excess EH deletion in scatter-composed units. The 251
edges are blast radius; the defect population is ~0.

## 5. Sample: 0 applicable sites out of the 3 best-qualified

The 11 tight candidates are dominated by mispairs and unported bodies
(`?ClassName@RndDir@@UBA?AVSymbol@@XZ` t=504/b=48; `?SecondsToBeat@@YAMM@Z`
t=648/b=16; `?ContentMounted@MusicLibrary@@` t=512/b=4 — a stub; four more with
100–630 target-only instructions, i.e. whole missing bodies). Only three are
plausible EH-only defects. Each was audited for whether the mechanism is even
*applicable* — i.e. whether the callee whose EH vanished is cross-origin inside
the combined TU:

| candidate | unit / owner | applicable? |
| --- | --- | --- |
| `??_GSpotlightDrawer@@UAAPAXI@Z` (42.6 %, t=112/b=76) | `default/MoveMgr` ← `src/system/hamobj/MoveMgr.cpp` | **No.** MoveMgr.cpp scatter-includes only `world/CameraManager.cpp`; `SpotlightDrawer::~SpotlightDrawer` lives in `src/system/world/SpotlightDrawer.cpp`, not in this TU. Nothing to guard. |
| `??$_Destroy_Range@PAULabel@?A0x81ddebd1@@…` (37.3 %, t=96/b=80) | `default/WaveFile` ← `src/system/utl/WaveFile.cpp` | **No.** `struct Label` is in WaveFile.cpp's *own* anonymous namespace (line 15). Same origin file; scatter-inclusion of `rndobj/Group.cpp` is irrelevant. |
| `?Copy@RndSpline@@UAAXPBVObject@Hmx@@W4CopyType@23@@Z` (1.8 %, t=248/b=232) | `default/Line` ← `src/system/rndobj/Line.cpp` | **No.** At 1.8 % with 34 target-only instructions this is a wholesale body divergence, not an EH-only defect. |

Also checked: `?clear@?$ObjPtrList@VCamShot@@…` (PropSync, 65 %) and
`??_GSetUserDifficultyMsg@@…` (MusicLibrary, 74 %) — both have
`target_size == base_size` and **zero** insert/delete instructions, so no EH
frame is missing at all; the `__unwindtable$` detector false-positives on them.

**Measured per-site flip rate: 0 / 3 applicable (denominator 3 audited of 11
tight candidates of 810 named sub-100 in 166 scatter units).** Confirmed
instances of the mechanism anywhere in the tree: **1 / 1 — laneP's own, already
fixed in main.**

## 6. Recommendation

**CLOSE the channel.** Concretely:

1. Do **not** land `laneP-nothrow`. It is a lateral swap of an already-working
   fix, and it inflates the guarded callee 44 → 100 bytes.
2. Do **not** fund a "scanner over ~233 sites". The scanner already exists — it
   is §4 of this document, it ran, and it found no excess. Total cost of the
   census: one full build plus ~15 minutes of `objdiff-cli --batch`. Anyone
   re-opening this should re-run the census, not re-derive the technique.
3. **Keep the knowledge, not the patch.** The reusable assets are (a) the
   `.pdata` bit-31 `ExceptionFlag` reader against dtk's `<Unit>.s`, which gives
   retail's per-function EH ground truth cheaply and is useful well beyond this
   channel, and (b) the finding that inlining suppression alone
   (`__declspec(noinline)`, `auto_inline(off)`, `inline_depth(0)`) never defeats
   MSVC's nothrow analysis — it plateaus at ~61.7 %, so the two effects must be
   attacked separately.
4. If a genuine instance is ever found, prefer **third-TU relocation** (main's
   `DataUtl.cpp` pattern). Reach for `#pragma optimize("g", off)` only after
   confirming the callee is absent from `target_symbol_map.json`, and gate it
   `#ifndef HX_NATIVE` — `#pragma optimize` is MSVC-only and warns under clang.

## 7. Reproduction

```
scripts/setup_worktree.sh ~/tmp/wt-laneBG-1 laneBG-1
cd ~/tmp/wt-laneBG-1 && rm -f build/45410914/report.cache && ./tools/ninja-locked
# (d) test: move DataNode copy ctor DataUtl.cpp -> DataNode.cpp, with/without the pragma,
#     then objdiff ?Insert@DataArray@@QAAXHABVDataNode@@@Z
# site count:  grep -rn '#include "[^"]*\.cpp"' src/ --include='*.cpp' --include='*.h'
# EH census:   parse '.4byte fn_XXXXXXXX / .4byte 0xPPPPPPPP' pairs from
#              build/45410914/asm/<Unit>.s, bit31 = ExceptionFlag; join via
#              scripts/target_symbol_map.json; compare against __unwindtable$<sym>
#              in build/45410914/<source_path>.obj.  Run it on scatter AND plain units.
```

Source tree was restored to baseline after every experiment; no source change is
proposed by this lane.
