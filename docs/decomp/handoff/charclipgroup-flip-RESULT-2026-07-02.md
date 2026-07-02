# CharClipGroup ObjVector flip — APPLIED, +2 strict (result, 2026-07-02)

Worker ws3-optionc p1. Worktree /home/free/tmp/wt-exec-ws3-optionc, branch exec/ws3-optionc-0702.

## Result (per-symbol objdiff, project_dir=worktree)
- `?FindClip@CharClipGroup@@QBAPAVCharClip@@PBD@Z` (fn_8237B698): 91.53% -> **100.0% normalized (100.0% raw)**
- `?Save@CharClipGroup@@UAAXAAVBinStream@@@Z` (fn_8237C598): 99.9% -> **100.0% normalized (99.9% raw)**
  - The residual 0.1% raw is the ObjOwnerPtr-vector `operator<<` relocation name only (normalized=100). Strict metric = normalized.
- icf_alias_check: EXIT 0, VERDICT HONEST (2 real-bodied, 0 stub-fold, 0 foreign).

## DIVERGENCE FROM THE READY handoff (important — reviewer please note)
The READY doc said "keep unk24, it is offset-free (mWhich@0x14, mFlags@0x18, unk24@0x1c), size stays 0x20."
That is WRONG per the built asm. With unk24 kept + members reordered exactly as the doc said, Save
stayed at **99.9%**: the retail Save asm places the virtual base at this+0x20 (there is a 4-byte MSVC
**vtordisp** at 0x1c, before the Hmx::Object vbase — CharClipGroup overrides vbase virtuals). unk24 as a
real int consumes 0x1c..0x20 and pushes vtordisp+vbase to 0x24, shifting every member-relative load by +4
(5 diff_arg: vbptr this-0x24 vs this-0x20, &mClips this-0x20 vs this-0x1c, mWhich this-0x10 vs this-0xc,
mFlags this-0xc vs this-0x8).

Root cause = the DC3-newer-drift caveat in CLAUDE.md. The rb3-Wii GAME oracle
(../rb3/src/system/char/CharClipGroup.h + .cpp) shows retail RB3 CharClipGroup has **only mClips, mWhich,
mFlags — NO unk24**, and GetClip is a MakeMRU rotation, not DC3's persistent-shuffle-index (unk24) logic.

## What I changed beyond the doc
- Removed `int unk24` entirely (header + ctor). Layout now vbptr@0, mClips@0x4 (0x10), mWhich@0x14,
  mFlags@0x18, vtordisp@0x1c, Hmx::Object vbase@0x20 = matches retail Save asm exactly.
- Ported GetClip(int), GetClip() no-arg, and MakeMRU(int) verbatim from the rb3-Wii oracle (they need no
  unk24). Removed QueueRandom (DC3-only, unused after the port). Header: dropped QueueRandom decl, added
  `void MakeMRU(int);`.
- All the doc's other edits applied verbatim (ObjPtrVec->ObjVector<ObjOwnerPtr>, delete the STLport node
  template-instantiation block, +#include <algorithm>, `d.stream >> mClips`, index-loop HasClip,
  std::swap in remaining swaps, std::sort with Alphabetically, FindClip returns mClips[i]).
- One extra compile-forced edit: `HANDLE_EXPR(get_size, (int)mClips.size())` — std::vector::size() is now
  unsigned size_type (was ObjPtrVec::size()->int); the (int) cast matches the rb3-Wii oracle and does not
  change the DataNode(int) codegen. Handle is untracked anyway.

## Scope / regression safety
- Only FindClip + Save are in scripts/target_symbol_map.json for CharClipGroup; GetClip/MakeMRU/HasClip/
  Sort are UNTRACKED, so the GetClip rewrite cannot create/destroy a scored match. FindClip does not touch
  unk24/GetClip and stays 100.
- CharClipGroup.h is included by ~11 sibling char/bandobj TUs, but every reference is a pointer
  (ObjPtr<CharClipGroup> / CharClipGroup*, 17 sites) — **zero by-value embeds**. Member offsets mClips@0x4,
  mWhich@0x14, mFlags@0x18 are unchanged; only total size / vbase offset moved, and that is resolved inside
  CharClipGroup.obj (heap alloc via OBJ_MEM_OVERLOAD) not in siblings. Sibling-regression risk ~nil.

## NOT DONE (per worker rules, left for reviewer)
- Whole-binary A/B (ab_measure.py) — worker rules forbid whole-binary builds; the reviewer runs the one
  composed A/B. Expect strict net +2 (FindClip, Save), watch for sibling char-TU regressions (analysis
  above predicts none).
- Commit — worker rules forbid git writes. Files left dirty:
  src/system/char/CharClipGroup.h, src/system/char/CharClipGroup.cpp.
  Intended commit msg: "flip(CharClipGroup): ObjPtrVec->ObjVector<ObjOwnerPtr>, member reorder, drop
  DC3 unk24 — FindClip+Save strict (+2)".
