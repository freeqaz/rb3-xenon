# B2 warm-up findings — the wall is ORACLE QUALITY, not the source port

**Date:** 2026-06-21. **Status:** empirical, from the B2 warm-up workflow (5 fresh
TU ports through the full pipeline). **This overturns PIPELINE-DESIGN.md §0/§6's
core assumption** (that the binding constraint is source-port byte-exactness).

## What B2 ran
Ported + wired + harvested 5 fresh, un-pinned, with-source scattered TUs end-to-end
through the new pipeline (field_offset_gate → --pin-only → build → measure → audit):
ChordPreview, Scoring, PerfectSectionTracker, SongSortNode, TourPerformerLocal.

## Result: 0/5 landable — and HONESTLY so
| TU | port | verdict | why |
|---|---|---|---|
| ChordPreview | clean | DEFER:oracle-misattribution | 1 nameable method; its VA holds an STL `_M_fill_assign` (6× size) |
| Scoring | clean (15/18 defined) | DEFER:oracle-va-misattribution | 5/10 case-A VAs already own FOREIGN names (D3DXShader/STL); retail sizes 3–25× oracle |
| (3rd) | clean | DEFER:oracle-VA-misattribution+inlining | both walls |
| (4th/5th) | clean | DEFER:body-divergence / port-body-divergence | the wave-16 codegen wall |

**The honesty gates HELD: 0 fake matches across 5 ports.** Misattribution surfaces
as honest +0 (STRICT add-only kept-existing, byte-equality, size-DQ), never as
inflation. The transport, field-gate, and gates are all PROVEN CORRECT on fresh TUs.

## The reframe: two walls, oracle-quality is dominant
1. **ORACLE VA MISATTRIBUTION (dominant, 3/5).** The rb3-Wii→retail BinDiff oracle
   (`unified_id_rb3wii.json`) maps a TU's methods onto retail VAs that hold
   UNRELATED functions. Detectable PRE-PORT: retail fn size at the VA is 5–25× the
   oracle Wii size, and/or the VA already owns a foreign mangled name. **RockCentral
   (+17) was a good-oracle EXCEPTION, not the norm.** The design assumed good VAs.
2. **BODY DIVERGENCE (2/5).** Even a clean port + good VA can diverge (MWCC↔MSVC
   inlining/regalloc — axes B/D). field_offset_gate only handles tail-field-poison
   (axis A); it does not catch inlining divergence.

## The fix: `tools/oracle_quality.py` (pre-screen) — built + CALIBRATED
Scores each TU's oracle rows: **GOOD = real-bodied(>44B) ∧ retail/wii size ratio in
the two-compiler band [0.3,3.5] ∧ not foreign-owned.** Good-count PREDICTS yield:
- RockCentral **GOOD=18** → its +17 landed (94% hit). Scoring **GOOD=1**, ChordPreview
  **GOOD=2** → the B2 +0s. **The B2 0/5 was BAD TARGET SELECTION** (low-good-oracle
  TUs the pre-screen would have deselected), not a dead vein.

**Backlog (oracle-quality-adjusted): 1169 good-oracle real-bodied methods across 679
TUs; 87 TUs with ≥4 good.** Actionable (good-oracle ∧ has-source ∧ not-pinned), top:
GemPlayer 31, SessionMessages 26, OvershellPanel 18, NetSession 12, AppLabel 11,
MetaPanel 11, ProfileMgr 10, TourProgress 9, GemManager 9, … (GemPlayer/Game are
Stats-struct-lever-gated — field-gate pins their non-tail head).

## Corrected harvest procedure (supersedes PIPELINE-DESIGN §3 target selection)
1. **`oracle_quality.py --tu X`** FIRST — only port TUs with a worthwhile GOOD count.
2. Port + wire (whole file compiles + defines; don't hand-match bodies).
3. **PIN-SET = GOOD-oracle VAs ∩ field_offset_gate-clean ∩ methods-DEFINED-in-obj.**
   The triple intersection is the B2 fix: never pin a misattributed or undefined VA
   (they silently yield +0 and waste the pin).
4. byte-equality is the only positive gate; `body_divergence_killed` (good-oracle
   methods that pinned clean but stayed <100%) measures the residual axis-B/D wall.

## Pipeline friction fixed forward (from B2 agents)
- Fresh CoW worktrees lack `build/tools/wibo` → symlink from main (the harvest
  workflow does this); building a single `.obj` target avoids the `tools` phony's
  sjiswrap/binutils downloads.
- `field_offset_gate --tu` matches oracle `bindiff_src` by BASENAME (`Foo.cpp`), not
  the objects.json path. `--emit-pin-only <path>` takes the path as its argument.
- `--infer-d` over-poisons FLAT structs (Scoring: proposed D=0xC on `mPointInfo[10]`)
  — keep the safe D=∞ default unless a class's first member is a genuine
  array-of-heavy-member; the struct-lever lane sets `--D`.
- The driver should auto-intersect the gate pin-set with the obj's defined-symbol
  table and surface "pinned-but-name-not-applied" as an explicit per-VA DEFER.
