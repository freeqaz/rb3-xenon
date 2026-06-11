# bp4 deep-dive: WHY the banked ObjectDir-vbase patch is net-0 (2026-06-11)

Mission: root-cause the `TrackReset +1 / CharClipSet fn_823C2044 −1` coupling of
the banked patch (`docs/decomp/handoff/objectdir-vbase-banked.patch`), and
either compose it to net ≥ +1 or park it with evidence.

Worktree used: `/home/free/code/milohax/wt-bp4-vbase-recompose`, branch
`bp4-vbase-recompose` (branched from main @78a6ee6, baseline **7785**).
Branch state at close: `4026980` (the banked patch) + `961b2e1` (new: CharLipSync
mPropAnim prerequisite, 0-delta). All A/Bs below are against this worktree's own
fresh baselines (main moved to 7866 mid-investigation from concurrent landings —
main's report is NOT comparable).

## VERDICT (TL;DR)

**PARK the banked patch as a standalone land (net-0 confirmed, mechanism now
fully understood) — but RECLASSIFY the −1: it is not a regression, it is the
patch correctly deleting an artifact match that was funded by the very header
bug the patch fixes.** The pre-patch "100%" on fn_823C2044 was paired against a
COMDAT (`??0DataArrayPtr@@QAA@PAVDataArray@@@Z`) that exists in our
CharClipSet.obj **only because of the bogus DC3-only
`virtual GetExposedProperties()`** — retail's CharClipSet TU contains no such
function (byte-pattern absent from the entire pinned range). No cheap honest
composition exists. The honest +N composition is the **CharLipSync.cpp
re-pin + port campaign** (§6): the patch becomes a pure +1 inside it, with
~29 named-method upside on top. The campaign prerequisite (mPropAnim layout
fix) is implemented and committed on the branch (0-delta, zero risk).

---

## 1. fn_823C2044 identity

40-byte EH unwind funclet at 0x823C2044, inside the `CharClipSet.cpp` pin
(.text 0x823BEA70–0x823C3FF0), report key `default/CharClipSet fn_823C2044`.
Retail body (dtk asm, `build/45410914/asm/CharClipSet.s`):

```
subi r31, r12, 0x70     ; parent frame 0x70
mflr r12 / stw r12,-8(r1) / stwu r1,-0x60(r1)
lwz  r3, 0x50(r31)      ; load pointer from parent EH-state slot 0x50
bl   fn_82260288        ; trivial dtor: stores vptr 0x8200098C, blr (ICF-folded)
addi r1,r1,0x60 / lwz r12,-8(r1) / mtlr r12 / blr
```

**Its true parent is fn_823C1F80 = retail `??0CharLipSync@@IAA@XZ`** (NOT a
CharClipSet function, and NOT "SyncProperty's DataNode-dtor funclet" as the
banked-patch commit text guessed):

- fn_823C1F80 (0x9C) stores vtable **0x82051D3C** at `0(r30)`; RTTI COL chain
  decoded from `auto_00_82000400_rdata.obj` + `auto_06_82C34400_data.obj` gives
  TypeDescriptor name **`.?AVCharLipSync@@`**.
- It inline-constructs a member at this+0x28 with vptr **0x82013EA8** = RTTI
  **`.?AV?$ObjPtr@VRndPropAnim@@VObjectDir@@@@`** (owner=this stored at +0x2c),
  then two 3-word zeroed members (vectors) at +0x34 / +0x44, maintaining the
  EH "current member" marker in frame slot 0x50(r31).
- fn_823C2044 is the **member-unwind funclet** (destroy the partially
  constructed member via the 0x50 marker). Its sibling fn_823C201C
  (`lwz r3, 0x84(r31); bl ??1Object@Hmx`) is the **base-unwind funclet**.

Layout proven for retail RB3-360 CharLipSync: `Hmx::Object(0x28)` +
`ObjPtr<RndPropAnim> mPropAnim @0x28` + `vector<String> mVisemes @0x34` +
`int mFrames @0x40` + `vector<uchar> mData @0x44` (sizeof 0x50). This matches
**rb3-Wii** (`ObjPtr<RndPropAnim> mPropAnim; // 0x1c`, first member) and the
**DC3 binary** (ham_xbox_r.map has `??_G?$ObjPtr@VRndPropAnim@@@@` in
`char:CharLipSync.obj`) — only **dc3-decomp's header dropped the member**, and
rb3-xenon inherited that header.

### Why CharLipSync code is inside the CharClipSet pin

The wired `CharLipSync.cpp` pin (.text 0x822CADA8–0x822CB7C8) is a **displaced
sliver** (4/19 matched, all generic `_Bit_iter` STL, every real fn 0%). The real
CharLipSync.cpp TU sits INSIDE the CharClipSet pin: the full retail CharLipSync
vtable @0x82051D3C points its own-virtual slots at 0x823C25A8, 0x823C2070,
0x823C20A8, 0x823C2AB8, 0x823C11E8, 0x823C22C8, 0x823C2E28 — all in
[0x823C11E8, 0x823C2E28]. Meanwhile CharClipSet's own vtables (RTTI-chained
from `.?AVCharClipSet@@`, vtables @0x82051254/0x8205124c/etc.) point at
0x823BE0E8…0x823C0CC8 — several of which are **below the pin start, inside the
CharSleeve pin** (0x823BD6D8–0x823BEA70). So the true TU boundary
CharClipSet→CharLipSync lies in **(0x823C0CD8, 0x823C11E8]**, and the
CharSleeve→CharClipSet boundary is ≤0x823BE0E8. Classic wave-3 "displaced pin"
disease, one TU-slot shifted.

Region census (worktree report, patched state): below-boundary (CharClipSet
proper) 63 fns / 28 matched; ambiguous zone 9 fns / 1 matched; CharLipSync side
(≥0x823C11E8) **102 fns / 19 matched / 14 at 99.x** — the 19 are all anonymous
byte-paired funclets/thunks (parent-blind pairing, see §2).

## 2. Root cause of the −1 (mechanism, with evidence)

objdiff (fork) pairs anonymous funclet-likes (`fn_<8hex>`, `__unwind$N`,
`??__E/F`) by **masked byte signature** — every reloc-covered word zeroed —
in `objdiff-core/src/diff/mod.rs::pair_funclets_by_bytes` (pass 1 unique-exact,
pass 2/2b ambiguous/overflow exact, pass 3 same-size fuzzy ≥50% Hamming). The
`bl` callee is a reloc → **masked**; a funclet's identity reduces to
(frame-size word, r3-setup word). Pairing is deliberately parent-blind.

COFF symbol-table comparison of our compiled CharClipSet.obj pre/post patch
(HEAD~1 vs HEAD builds; objs saved at /tmp/bp4_ccs_{pre,post}.obj during the
session, log /tmp/bp4_vbasedeep_funclets.txt):

- PRE has `?GetExposedProperties@ObjectDir@@UAA?AVDataArrayPtr@@XZ` (sec 532)
  **and `??0DataArrayPtr@@QAA@PAVDataArray@@@Z` (sec 493)**. The DataArrayPtr
  ctor is pulled in solely by GetExposedProperties' inline body
  (`return nullptr` → DataArrayPtr temp construction; the ctor news a
  DataArray when passed null, so it carries an EH funclet).
- Sec 493 contains funclet **`__unwind$72600`**: masked bytes
  `3becff90 7d8802a6 9181fff8 9421ffa0 807f0050 <bl masked> 38210060 8181fff8
  7d8803a6 4e800020` — **byte-identical to target fn_823C2044** (frame 0x70 +
  `lwz r3,0x50(r31)`). Unique on both sides → pass-1 exact pair → 100%.
- POST: GetExposedProperties COMDAT **gone**, DataArrayPtr ctor COMDAT **gone**
  (frame-0x70 funclet family drops 14→13; only delta in the whole obj),
  `?AllowsInlineProxy@ObjectDir@@UAA_NXZ` added (trivial, no funclet).
- fn_823C2044 then falls to pass-3 fuzzy and pairs with PropSync's DataNode
  funclet (`addi r3, r31, 0x50; bl ??1DataNode@@QAA@XZ`) at 9/10 instructions =
  **94%**. Verified live: run_objdiff shows exactly `[4] lwz r3,0x50,r31` vs
  `addi r3,r31,0x50`, callee diff fn_82260288 vs ??1DataNode.

So the coupling is **(b) pairing perturbation** — but of a specific, damning
kind: the 100% partner was a **phantom COMDAT that only the DC3-only virtual
generated**. Retail's CharClipSet TU has no DataArrayPtr ctor anywhere in range
(no `stw r4,0(r3)` byte pattern in CharClipSet.s at all). The baseline 7785
"owed" this match to the header bug.

Re-creating the partner honestly inside CharClipSet.obj is impossible:
- no source construct retail had emits that COMDAT there (checked);
- injecting an artificial instantiation = attribution gaming (rejected).

## 3. Full binary-wide per-fn delta of the banked patch

Two-report compare (worktree patched vs main@7785 pre-move; later re-verified
against the worktree's own rebuilt baseline): **exactly 4 functions change**:

| unit/fn | pre → post | class |
|---|---|---|
| VocalTrackDir `?TrackReset@…` | 99.989 → **100.0 (+1)** | real fix (SyncObjects vcall slot 0x10→0xc) |
| CharClipSet `fn_823C2044` | **100.0 → 94.0 (−1)** | artifact-partner removal (§2) |
| CharClipSet `fn_823C2E00` | 99.8 → 93.9 | pass-3 greedy rotation; metric-neutral |
| CharClipSet `fn_823C3138` | 93.9 → 99.8 | swapped partner with fn_823C2E00; metric-neutral |

The 2E00/3138 pair literally exchange percentages (two same-size funclets,
String-dtor vs frame-0xf0; removal of __unwind$72600 reshuffles the pass-3
greedy order). Both sub-100 on both sides → no metric impact.

Notes vs the original research doc (2026-06-11-vtable-walls.md):
- `PanelDir::RemovingObject` did **NOT** gain: 99.978 pre AND post. Its residual
  `[37] lwz [off:+4]` is an address-reloc/static-placement artifact (live
  normalized objdiff reads 100.0, the report metric reads 99.978 — the
  known live-vs-report divergence class). The doc's "+1 gainer" claim is stale;
  do not count it in this patch's EV.
- No other ObjectDir-descendant fn moved: the GEP(+1-slot)/AllowsInlineProxy
  (−1-slot) errors cancel for all vcalls past InlineSubDirType, and
  TrackReset was the only pinned near-100 fn with a vcall in the broken window.

## 4. Refuted cheap fixes

1. **Map/renamer fix** — N/A: fn_823C2044 has no map entry; its pairing is the
   byte-fallback by design. Forcing a name would *break* the (correct) base
   pairing semantics, not fix it.
2. **Re-emit DataArrayPtr ctor in CharClipSet.cpp** — dishonest (retail TU
   lacks it; the only honest emitter was the bug being fixed). REJECTED.
3. **Quick re-pin of just the funclet region** — pins are contiguous and the
   CharLipSync zone is mid/upper pin with 19 currently-matched byte-pairs +
   the old sliver's 4 matches at risk; our CharLipSync.obj cannot yet re-form
   them (see §5). A re-pin before the port matures is net-negative-risk.
   Port-THEN-extend applies.

## 5. Composition feasibility — executed prerequisite + measured A/B

Executed in the worktree (committed `961b2e1`):

**CharLipSync mPropAnim layout fix** — add `ObjPtr<RndPropAnim> mPropAnim;`
@0x28 (member offsets shift 0x2c/0x38/0x3c → 0x34/0x40/0x44) + ctor init
`mPropAnim(this)` + Load(rev==1)-into-member + COPY_MEMBER + SYNC_PROP, all
rb3-Wii-faithful.

A/B (full `rm stamp + touch config.yml + NINJA_JOBS=8 tools/fresh_report.sh`,
logs /tmp/bp4_vbasedeep_ab{B,1,C}.log): **7785 → 7785, ZERO per-fn deltas**
(B vs C compare over all 65,545 fns). No pinned fn reads these offsets today —
pure prerequisite, zero risk, zero reward standalone.

Post-fix compiled `??0CharLipSync@@IAA@XZ` (0x94 vs retail 0x9C):
- now carries the EH machinery: stores `this` at 0x84(r31), maintains the
  member marker at 0x50(r31), and emits the **base-unwind funclet byte-identical
  to target fn_823C201C** (`lwz r3,0x84(r31)` + Object dtor) — first hard proof
  the campaign can pair real CharLipSync funclets;
- but the **member-unwind funclet (fn_823C2044's shape) is still missing**:
  retail INLINES the ObjPtr ctor to 3 stores (vptr/owner/null — old thin
  ObjPtr), while our post-migration poly ObjPtr ctor compiles **out-of-line**
  (`bl ??0ObjPtr`), so MSVC never materializes that EH state's funclet. This is
  the ObjPtr inline-policy wall (see project_objptr_relayout_migration /
  DataArray inline-Node keystone pattern) — the remaining technical gate to
  the +1.

## 6. The composition plan (numbered) — CharLipSync re-pin + port campaign

The banked patch should be landed **inside this campaign**, where its −1
structurally cannot occur (fn_823C2044 leaves the CharClipSet unit) and it
contributes a clean +1 (TrackReset). EV beyond that: DC3's map shows **~29 real
named methods** in CharLipSync.obj (ctor/dtor, Save/Load/Copy/Handle/
SyncProperty, Print/Parse/OnParse(Array), Generator::{Init,AddWeight,NextFrame,
Finish,RemoveViseme}, PlayBack::{ctor,Set,SetClips,Reset,Poll}, FindLipSync…),
102 target fns / 14 current 99.x near-misses in the region.

1. **Land `961b2e1` (mPropAnim) any time** — verified 0-delta, oracle-true.
2. **Determine the exact TU boundary** B ∈ (0x823C0CD8, 0x823C11E8]: port
   CharLipSync.cpp bodies first (step 3), then identify the first
   CharLipSync-owned COMDAT above CharClipSet's last fn by content-matching the
   compiled CharLipSync.obj against the target asm (the standard
   relocate/content-match kit; the zone fns are template COMDATs:
   `??$?6E…vector<uchar>`, `_M_insert_overflow…` — CharLipSync.cpp instantiates
   vector<uchar>/vector<String> streams).
3. **Port CharLipSync.cpp to RB3 shape** (rb3-Wii oracle, NOT DC3 where they
   diverge): REVS(1,0) not (2,0); check RegisterLipSync/sLipSyncMap existence in
   retail via Handle/dtor bodies; PlayBack uses GetPropAnim. Iterate per-fn with
   objdiff once pinned (step 5).
4. **Crack the ObjPtr-ctor inlining for the null case** so ??0CharLipSync
   matches and the member-unwind funclet (fn_823C2044's exact partner)
   materializes. Options: __forceinline on the (owner,T*) ctor path, or the
   header-inline-body keystone pattern (DataArray::Node precedent). This is the
   hardest step; it also unblocks every other ObjPtr-member ctor in the binary.
5. **Shared-boundary re-pin in ONE edit** (pdata-clean per the wave-3 rule):
   `CharClipSet.cpp .text → [0x823BEA70, B)`,
   `CharLipSync.cpp .text → [B, 0x823C3FF0)` (vacating the dead sliver
   0x822CADA8–0x822CB7C8 back to auto — its 4 generic-STL matches are at risk;
   net them in the A/B). Also queue the **CharSleeve→CharClipSet under-pin**
   (CharClipSet virtuals at 0x823BE0E8+ live in CharSleeve's pin) as a sibling
   boundary fix in the same wave.
6. **Land the banked vbase patch in the same wave.** Order within the wave is
   free; the composed A/B gate is the worktree's own fresh baseline; expected
   composed result ≥ +1 (TrackReset) + ctor/funclets + named ports, minus
   whatever subset of the 19 region byte-pairs + 4 sliver matches fail to
   re-form — measure, don't assume.
7. Re-run `tools/pin_audit.py` after landing (the CharSleeve/CharClipSet/
   CharLipSync triple suggests the one-slot-shift disease continues through
   this neighborhood).

## 7. Falsifiable claims ledger

- Banked patch alone: net 0, exactly the 4-fn delta of §3 — REPRODUCED twice.
- mPropAnim fix alone (on top of patch): net 0, ZERO deltas — MEASURED.
- `__unwind$72600` ∈ `??0DataArrayPtr` COMDAT, present pre / absent post —
  COFF-verified.
- Retail CharClipSet pin contains no DataArrayPtr ctor — byte-pattern search,
  no hits.
- fn_823C1F80 = CharLipSync ctor — RTTI chain from raw rdata/data objs.
- CharLipSync.cpp pin = displaced sliver — report census (4/19, all `_Bit_iter`).
- Our ctor's base-unwind funclet == target fn_823C201C bytes after masking —
  COFF-verified post-mPropAnim.
