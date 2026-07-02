# ws3-optionc p2 — MotionBlur + SoftParticles port-then-pin (2026-07-02)

Worker: Opus. Worktree `/home/free/tmp/wt-exec-ws3-optionc-p2`, branch
`exec/ws3-optionc-0702-p2` (main @00c5b19). Frozen baseline
`/home/free/tmp/exec-ws3-optionc-p2-baseline-report.json` (10936 matched).

## Result

**A/B (whole-binary, `tools/ab_measure.py --resplit --build`, reproduced twice, run1==run2):**
```
baseline 10936 -> candidate 10961   NET +25   (2 units up, 0 units down)
  +14  default/system/rndobj/SoftParticles  (0->14)
  +11  default/system/rndobj/MotionBlur     (0->11)
```
- Gate (a) `tools/icf_alias_check.py`: **HONEST**, exit 0 — 18 real-bodied / 25
  matched (72%); 7 interspersed stub-folds (own template getters + 1 foreign
  ??_E thunk left mislabeled), longest contiguous stub/foreign run = 2.
- Gate (b): NET **+25**, **0 regressions**, run1==run2 (both full rebuilds).
- Gate (c) real bodies (>44B, non-static-init): **18** (9 per unit).
- Gate (d): overlap_check clean (0 overlaps). NOT committed — left in worktree
  for reviewer (worker rule).

## Final spans (conservative, TU-boundary-attributed)

```
system/rndobj/MotionBlur.cpp:
    .text  start:0x82480A90 end:0x82481300    (.pdata auto-backfilled 0x82209370-0x822093E0)
system/rndobj/SoftParticles.cpp:
    .text  start:0x824818E0 end:0x824822D8    (.pdata auto-backfilled 0x82209448-0x822094E0)
```

**Boundary evidence (Ghidra port 8002):**
- MotionBlur start = **0x82480A90** (NOT 0x82480660). The planner's gap start
  0x82480660 belongs to a FOREIGN sibling TU: `fn_82480798` (0x21C) and
  `fn_82480718` (0x74) both `bl` into AmbientOcclusion's pinned range
  (`fn_82480548`, `fn_8247BA40`). Pinning from 0x82480660 swallowed them (read
  0% + false funclet folds); tightening start to CanMotionBlur removed that
  inflation with **zero** loss of MotionBlur bodies (Handle/SyncProperty/
  OnAllowedDrawable/ctor/Load all live ABOVE 0x82480A90).
- SoftParticles is a complete self-contained cluster [0x824818E0, 0x824822D8):
  Save/Handle/ctor/Copy/Load/DrawShowing/ListDrawChildren/SyncProperty all
  present; `??_E@0x82482288` (adjusts -0x40, calls `??_D@RndSoftParticles`
  @0x82481EF0) confirms it belongs to SoftParticles, ending exactly at CubeTex
  (0x824822D8). No foreign swallow at head or tail.
- TexBlender/TexBlendController/PollAnim occupy the UNPINNED gap
  [0x82481300, 0x824818E0) — deliberately left unpinned (not this packet;
  0 sim>=0.9 oracle rows).

## Sources

`src/system/rndobj/{MotionBlur,SoftParticles}.{cpp,h}` were already scaffolded
(DC3 flavor, commit 8b28623) and are **byte-correct as-is** — NOT modified.
Ghidra confirmed the DC3 flavor is the retail match, not the rb3-Wii flavor:
- MotionBlur::Save writes rev=1 (DC3 `SAVE_REVS(1,0)`); Copy does
  `COPY_MEMBER(mDrawList)` (dynamic_cast + ObjPtrList assign).
- SoftParticles has `mParticles`(0x40) + `mBlend`(0x54): Save writes both,
  Copy copies both, Load reads both, DrawShowing queues to
  `TheNgRnd.ParticleBuffer()`, ListDrawChildren exists — all DC3-only (the
  rb3-Wii flavor has neither member and a stub DrawShowing → would NOT match).

## target_symbol_map.json changes (all ADD-ONLY or verified corrections)

Corrections (Ghidra-proven mislabels the pre-existing map had inside the gap):
| addr | OLD (wrong) | NEW (verified) | match |
|---|---|---|---|
| 0x824811E8 | `??_G?$ObjRefConcrete@VRndTexBlendController...` | `?Copy@RndMotionBlur@@...` | 100% |
| 0x82481F50 | `?deallocate@?$StlNodeAlloc@...RndTexBlendController...` | `?Copy@RndSoftParticles@@...` | 100% |
| 0x82480DD8 | `?ClassName@RndTexBlender@@...` | `?ClassName@RndMotionBlur@@...` | 100% |
| 0x82481CF8 | `?ClassName@RndTexBlender@@...` | `?ClassName@RndSoftParticles@@...` | 100% |

Adds (via reveal_sweep + safe_name_merge, all confirmed 100%):
`0x82480B40 Save@RndMotionBlur`, `0x82480BC0 DrawShowing@RndMotionBlur`,
`0x82481188 ??_DRndMotionBlur`, `0x82480C30 ??0RndMotionBlur` (ctor),
`0x82481978 DrawShowing@RndSoftParticles`, `0x82481EF0 ??_DRndSoftParticles`,
`0x82482078 ListDrawChildren@RndSoftParticles`, `0x82481B48 ??0RndSoftParticles` (ctor).

`0x82482288 ??_ERndTexBlender` left AS-IS: it is really SoftParticles' -0x40
adjustor ??_E but neither variant byte-matched (8.45%/0%), so per gate 5 the
non-matching name was not introduced (original restored).

## Per-function 100% roster (25 fns; real bodies >44B in **bold**)

MotionBlur (11): **CanMotionBlur(172)**, **Save(124)**, **DrawShowing(104)**,
**??0RndMotionBlur/ctor(288)**, **??_D(96)**, **Copy(120)**, **ClassName(48)**,
**fn_82480D50(68)**, **fn_82480D94(68)** [ObjPtrList tmpl], fn_82480F6C(32),
fn_8248115C(40).

SoftParticles (14): **Save(148)**, **DrawShowing(92)**, **??0RndSoftParticles/ctor(296)**,
**??_D(96)**, **Copy(128)**, **ListDrawChildren(104)**, **ClassName(48)**,
**fn_82481C70(68)**, **fn_82481CB4(68)** [ObjPtrList tmpl], fn_82481AEC(40),
fn_82481B14(40), fn_82481E9C(32), fn_82482218(32), fn_82482238(32).

## Deferred (real divergences, NOT matched — not forced)

- `Load` (both): our compiled body is bloated by `ASSERT_REVS` (MB Load 276B vs
  retail fn_82481260 148B) — retail stripped the rev assert. A clean match needs
  the assert to compile out; deferred.
- `Handle` / `SyncProperty` (both): macro-generated, property-descriptor +
  Symbol-table reloc heavy; SyncProperty@SP probe read 10.9%. Deferred
  (permuter/macro-alignment class).

## Files touched (reviewer commits these on the packet branch)
- config/45410914/objects.json  (+2: MotionBlur.cpp, SoftParticles.cpp NonMatching)
- config/45410914/splits.txt     (+2 pins, .pdata auto-backfilled)
- scripts/target_symbol_map.json  (4 corrections + 8 adds)
- (sources unmodified; pre-scaffolded and correct)
