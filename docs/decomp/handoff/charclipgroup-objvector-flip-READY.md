# CharClipGroup ObjVector flip — READY TO APPLY (+2 strict), deferred on box saturation (2026-07-01)

The ObjVector-flip harvest (wf_00550624-264) fully scoped + asm-validated a **+2 STRICT** flip for
CharClipGroup but could NOT build (32-core box was 3x-oversubscribed, load 91-99, cold build crawled to
6/729). Flow/FlowNode were honestly REFUTED (their 99.9% near-misses are dtk-reloc name-noise, not
stride mismatches — the flip premise fails there; do NOT retry Flow/FlowNode). CharClipGroup is the real
one: the flip takes **FindClip 91.9%→100 and Save 99.9%→100** (upgrades the fuzzy pins @d696b52 to STRICT).
Worktree preserved: /home/free/tmp/wt-ov-CharClipGroup (branch ov-CharClipGroup, 0 commits).

## The edit (asm-validated vs build/45410914/asm/CharClipGroup.s: FindClip fn_8237B698 iterates
## begin=*(this+0x4)/end=*(this+0x8) STRIDE 0xc, obj@elem+0x8; Save fn_8237C598 has mWhich@0x14/mFlags@0x18)

### src/system/char/CharClipGroup.h (currently: ObjPtrVec<CharClip> mClips@0x4; mWhich@0x20; unk24; mFlags@0x28)
Flip + REORDER (the polymorphic 0x10 node -> thin, members shift down 0xc):
- `ObjPtrVec<CharClip> mClips; // 0x4`  ->  `ObjVector<ObjOwnerPtr<CharClip> > mClips; // 0x4`
- reorder to: `int mWhich; // 0x14`  then  `int mFlags; // 0x18`  then  `int unk24; // 0x1c`
  (retail Save proves mFlags@0x18 immediately after mWhich@0x14; unk24 is our-only field, offset free.)
  Size stays 0x20; regression-safe — nothing derives from CharClipGroup, every ref is ObjPtr<CharClipGroup>.

### src/system/char/CharClipGroup.cpp
- ctor: `mClips(this,(EraseMode)1),mWhich(0),unk24(0),mFlags(0)` -> `mClips(this),mWhich(0),mFlags(0),unk24(0)`
- DELETE the `#ifndef HX_NATIVE ... template class vector<ObjPtrVec<CharClip,ObjectDir>::Node,...>` block (Waypoint has none)
- ADD `#include <algorithm>`
- Load: `mClips.Load(d.stream,true,nullptr);` -> `d.stream >> mClips;`
- HasClip: `return mClips.end()!=mClips.find(clip);` -> index loop `for(int i=0;i<mClips.size();i++){if((CharClip*)mClips[i]==clip)return true;} return false;`
- GetClip: all 6 `mClips.swap(A,B)` -> `std::swap(mClips[A],mClips[B])`
- Sort: `mClips.sort(Alphabetically())` -> `std::sort(mClips.begin(),mClips.end(),Alphabetically())`
- FindClip: `return (CharClip*)mClips[i];` -> `return mClips[i];`
- LEAVE the RndMat operator<< specialization + `#include obj/ObjPtrVec_impl.h` untouched.

## To finish (at LOW box load)
1. Apply the 2 files in /home/free/tmp/wt-ov-CharClipGroup (COLD worktree already set up).
2. Rebuild; verify FindClip(0x8237B698)+Save(0x8237C598) reach 100 via objdiff; whole-binary COLD A/B
   (tools/measure_delta.py / ab_measure.py) must be strict net >= +2 with 0 regressions; icf_alias_check.
3. Commit to ov-CharClipGroup; land via scripts/harvest/land.sh. This SUPERSEDES the d696b52 fuzzy pins
   (they become strict-100). Precedent: Waypoint d3c6e4f (+7). Confidence HIGH (asm decoded).
