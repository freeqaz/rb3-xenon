# WAVE-5 lane handoff — EndingBonus (port + wire + pin)

Branch: `w5-endingbonus`  Worktree: `/home/free/tmp/wt-w5-endingbonus`
Base: main `5cb96d4`. Commits: `f3521b6` (port + 3 worklist targets), `3d8480a`
(cascade Reset -> true-100), plus a docs commit.

## What landed

Ported `src/system/bandobj/EndingBonus.cpp` (Wii MWCC -> MSVC X360), wired
`system/bandobj/EndingBonus.cpp: NonMatching` into `config/45410914/objects.json`
(engine module, mirrors BandCharDesc), added a bounded splits unit + 5 ADD-ONLY
`scripts/target_symbol_map.json` VA->mangled entries. Compile-gate clean.

### Header fix (REQUIRED — a lander must keep this)
`src/system/bandobj/EndingBonus.h` was pre-ported but did NOT compile standalone:
it uses `DECLARE_REVS` / `HANDLE_CHECK` / `REGISTER_OBJ_FACTORY_FUNC`, which live in
`obj/ObjMacros.h`, NOT `obj/Object.h`. Added `#include "obj/ObjMacros.h"` as the
first include (mirrors BandScoreboard.h). Because the include guard fires before
`bandobj/UnisonIcon.h` is pulled, this ALSO fixes UnisonIcon.h's identical latent
missing-include (UnisonIcon.h still lacks its own include — harmless while only
EndingBonus.cpp consumes it, but a future UnisonIcon.cpp port should add it there).
The C4005 macro-redefinition warnings (ObjMacros.h vs Object.h) are the SAME benign
pattern BandScoreboard.cpp already emits — not errors.

## Per-id outcomes

COMPOSED FULL REPORT (fresh `report.json`, all_source rebuilt, 779 objs) — the
authoritative gate. **EndingBonus unit = 5/5 matched, unit fuzzy = 100.0%.**
Whole-binary matched_functions 10897 -> **10902 (+5, zero regressions)** — neighbours
MoveMgr 24/123, StreakMeter 3/14, BandScoreboard 15/21 all unchanged; all 5 pins
report in their OWN unit (default/EndingBonus).

| symbol | VA | size | interactive cli norm% | report norm% | verdict |
|---|---|---|---|---|---|
| `?UnisonEnd@EndingBonus@@QAAXXZ` | 0x822C1F18 | 92 | 100.0000 | **100.0000** | strict (worklist) |
| `?Reset@EndingBonus@@QAAXXZ` | 0x822C2610 | 128 | 100.0000 | **100.0000** | strict (BONUS) |
| `?UnisonStart@EndingBonus@@QAAXH@Z` | 0x822C2FE8 | 124 | 99.8387 | **100.0000** | strict / report-norm-100 size-exact (worklist) |
| `?Failed@MiniIconData@EndingBonus@@QAAXXZ` | 0x822C1EA0 | 68 | 99.7059 | **100.0000** | strict / report-norm-100 size-exact (worklist) |
| `?Reset@MiniIconData@EndingBonus@@QAAXXZ` | 0x822C25C0 | 80 | 99.5000 | **100.0000** | strict / report-norm-100 size-exact (BONUS, load-bearing) |

All 5 are SIZE-EXACT (target_size==base_size) and identity-certain. The interactive
cli sub-100 for three of them is pure reloc/callee-name/callee-divergence residue on
byte-identical branches — the composed report normalizes it to 100 (exactly the
"cli 99.4-99.7 size-exact = report-normalized 100" recipe rule). ICF honesty: none
of the 5 addrs appear in icf_aliases.map; the two >44B true-cli-100 anchors
(UnisonEnd 92B, Reset 128B) are real bodies. Interactive-cli residual detail below
(useful for a future UnisonIcon lane):

- **UnisonStart** residual = 1 `bl` to `SetIconOrder` (0x822C2B58), which is REAL
  divergence (target 528B vs base 492B, 79.65% — permuter-class, NOT pinned). A
  bl to a size-MISMATCHED callee propagates a diff into the caller, so UnisonStart
  cannot reach 100 until SetIconOrder is matched (out of reach here).
- **Failed** residual = 1 `bl fn_822C1130` = `UnisonIcon::Fail` (cross-TU, unnamed).
  A future UnisonIcon.cpp lane naming 0x822C1130 flips Failed to 100.
- **MiniIconData::Reset** residual = `bl` to `MiniIconData::SetUsed` (same-TU,
  unnamed) + `UnisonIcon::Reset` (cross-TU, unnamed).
- **Reset reached true-100** precisely because its only same-TU callee
  (MiniIconData::Reset) is SIZE-EXACT: a bl to a size-exact callee matches even if
  that callee has its own internal name-residuals. That is the mechanism a lander
  should understand — Reset's map entry DEPENDS on MiniIconData::Reset's map entry
  existing (drop it and Reset falls back to 99.5%).

## What a lander must know
- Map is ADD-ONLY; all 5 entries are correct identities (verified vs the compiled
  EndingBonus.obj COFF symtab + confirmed by direct `bl` targets from UnisonStart/
  Reset). Two are true-100 (UnisonEnd, Reset). The other three are size-exact fuzzy
  (report-normalized-100 candidates); keep them (worklist targets + Reset's
  dependency).
- Splits: single EndingBonus.cpp unit, 5 disjoint `.text` ranges + 5 `.pdata`.
  Overlap self-check = 0/0. dtk re-serializes `.pdata` on split; ranges are already
  tight to real function extents (Reset 0x822C2610..0x822C2690 = dtk's 128B).
- NOT pinned: SetIconOrder (0x822C2B58) — real 528/492 divergence, left unclaimed.
- No MILO_DEBUG landmine in this TU (asserts gate on HX_NATIVE = off in the match
  build; MILO_ASSERT -> (void)(cond), MILO_WARN -> (void)sizeof()).
- The other ~25 EndingBonus methods + the stlport vector<MiniIconData> template
  instantiations live in the same gap (0x822C1EA0..~0x822C3068 for the real methods)
  but are unclaimed — a follow-up could pin the leaf MiniIconData setters (each calls
  exactly one UnisonIcon:: fn) once a UnisonIcon lane names those cross-TU callees.

## Placement
Clean gap between MoveMgr.cpp `.text` end 0x822C1E58 and StreakMeter.cpp start
0x822C4930. No carving of a neighbour unit. All claimed ranges < 0x822C4930.

---

## WAVE-5 AUDIT (independent re-verification) — VERDICT: CLEAR

Audited the lane in-worktree against the merge-base (`5cb96d4`). Every claim reproduced.

**1. Strict-100 re-verify (objdiff-cli-direct, JSON→file).** Ran per-symbol `diff`
with both the report's binary (v4.2.3) and the repo's (v4.2.1); results identical:
- `?UnisonEnd@EndingBonus@@QAAXXZ` — **raw/norm/fuzzy 100.0** (true anchor, 92B)
- `?Reset@EndingBonus@@QAAXXZ` — **raw/norm/fuzzy 100.0** (true anchor, 128B)
- `?UnisonStart@EndingBonus@@QAAXH@Z` — interactive 99.83871, size-exact 124/124
- `?Failed@MiniIconData@EndingBonus@@QAAXXZ` — interactive 99.70588, size-exact 68/68
- `?Reset@MiniIconData@EndingBonus@@QAAXXZ` — interactive 99.5, size-exact 80/80

The three sub-100 interactive scores are each a SINGLE `diff_arg` on a `bl` whose
target callee is unnamed in the split (cross-TU `UnisonIcon::Fail/UnisonEnd/Reset`,
and same-TU unpinned `SetIconOrder`). Every non-`bl` byte (opcodes, operands,
registers, field load/store offsets) is identical. This is the documented
"cli 99.4-99.7 + size-exact = report-normalized 100" reloc-naming residue.

**2. Composed report is the truth and it reproduces.** Regenerated a FRESH
`report generate` in the isolated worktree (2243 units, COLD cache 0 hits/2243
misses, 24.9s): `default/EndingBonus` = **5/5 matched, unit fuzzy 100.0**, all five
functions fuzzy 100.0. Whole-binary **matched_functions = 10902** (matches the
claimed 10897→10902 delta). The five symbols appear in NO other unit (no
double-count). The on-disk report.json (04:51) is consistent with this regen.

**3. ICF honesty gate — HONEST.** `icf_alias_check.py --tu EndingBonus.cpp`:
5 REAL-BODIED / 0 STUB-FOLD, longest contiguous stub/foreign run = 0, verdict
HONEST (real-bodied-dominated, 5 anchors, all >44B). None of the five VAs appear
in `icf_aliases.map` (which holds only PoolAlloc/MemOrPoolAlloc). Sibling-aliasing
byte-check: `MiniIconData::Failed` (68B) has a same-size near-twin `Succeeded`
(68B) — DISAMBIGUATED, because the Failed diff has ONLY the single `bl` mismatch;
its `mFailed` load/store offsets match base, so 0x822C1EA0 is honestly Failed
(a Succeeded body would mismatch the field offset too).

**4. Splits/map clean.** Map diff = 5 ADD-ONLY entries (no deletions/edits);
identities confirmed present in the compiled COFF symtab. Splits: EndingBonus
span 0x822C1EA0..0x822C3064 sits strictly between MoveMgr.cpp `.text` end
0x822C1E58 and StreakMeter.cpp `.text` start 0x822C4930; global overlap self-check
= 0 pdata / 0 text; no foreign range intersects the span.

**5. Compile-gate re-run (direct cl.exe).** Clean object produced; only the benign
C4005 ObjMacros-vs-Object macro-redef warnings (same pattern BandScoreboard emits)
+ a C4003 in rndobj/Part.h (not this TU). `EndingBonus.h`'s `#include
"obj/ObjMacros.h"` fix is required and correct.

**6. MILO_DEBUG landmine.** None — size-exact matches (68/68, 80/80, 92/92, 128/128,
124/124) prove `sizeof(MiniIconData)`/layout are retail-correct; MILO_ASSERT/WARN
lower to no-ops. No dev-only members removed.

**Minor doc nit (non-blocking):** the bullet describing MiniIconData::Reset's
residual says its callees are `MiniIconData::SetUsed (same-TU) + UnisonIcon::Reset`;
the actual diff shows `UnisonIcon::UnisonEnd + UnisonIcon::Reset` (SetUsed(false)
was inlined). Cosmetic — the pin identity and match are correct.

**Lander note:** branch is based on `5cb96d4`; main has since advanced (`aa58cee`).
Rebase is ADD-ONLY on objects.json / splits.txt / target_symbol_map.json against a
clean address gap (new file, no owner collision expected) — standard union land.

**AUDIT VERDICT: CLEAR — landable as-is (+5 strict, honesty-clean).**

---

## WAVE-5 AUDIT — 2nd independent pass (2026-07-02) — VERDICT: CLEAR

Fully re-derived from the artifacts (not from the section above). All claims hold.

- **objdiff-cli-direct, per-symbol JSON→file** (same objects the report used;
  base obj 04:34 is newer than source 04:27 = reflects committed source):
  | symbol | tgt/base | fuzzy | norm | raw | diff_score |
  |---|---|---|---|---|---|
  | UnisonEnd | 92/92 | 100.0 | 100.0 | 100.0 | **0/2300** (true anchor) |
  | Reset (EndingBonus) | 128/128 | 100.0 | 100.0 | 100.0 | **0/3200** (true anchor) |
  | UnisonStart | 124/124 | 99.8387 | 99.8387 | 99.6774 | **5/3100** (1 bl→unpinned SetIconOrder) |
  | Failed (MiniIconData) | 68/68 | 99.7059 | 99.7059 | 99.7059 | **5/1700** (1 bl→unnamed UnisonIcon::Fail) |
  | Reset (MiniIconData) | 80/80 | 99.5 | 99.5 | 99.5 | **10/2000** (2 bl→unnamed callees) |
- **Composed report.json (04:51, 2243 units, whole-binary) is the gate and it reproduces:**
  `matched_functions = 10902/65596`; `default/EndingBonus = 5/5 @ fuzzy 100.0`; every one
  of the 5 reports at 100.0 in its OWN unit, appears in NO other unit (no double-count).
  The 3 sub-100 interactive scores normalize to 100 in the composed report (documented
  size-exact reloc-naming residue). All 5 pins satisfy the OWNER bar
  (true-100 ×2, report-normalized-100 + size-exact ×3).
- **ICF honesty — HONEST (exit 0):** `icf_alias_check.py --tu EndingBonus.cpp` = 5 REAL-BODIED,
  0 STUB-FOLD, longest contiguous stub/foreign run 0, all anchors >44B. None of the 5 VAs
  appear in `icf_aliases.map` (15-line PoolAlloc-only fold list). (`--worktree` newly-matched
  mode needs a 5cb96d4 baseline report = whole-binary build, skipped for owner contention;
  `--tu` is conclusive here — the stub-fold inflation shape is absent, all 5 bodies 68-128B.)
- **Sibling-aliasing ruled out:** MiniIconData::Failed(68B)/Succeeded(68B) differ only in
  field offset (mFailed vs mSucceeded) + callee. The Failed diff_score is 5 (single bl only) —
  a wrong-sibling identity would also mismatch the field load/store offset and score far
  higher. Field offsets match base ⇒ 0x822C1EA0 is honestly Failed. Both true anchors score 0.
- **Map ADD-ONLY** (5 entries, all 0x822C1EA0-0x822C2FE8, no deletions/edits). **Splits clean:**
  global overlap self-check 0 pdata / 0 text; EndingBonus text span 0x822C1EA0..0x822C3064 sits
  strictly in the MoveMgr .text-end 0x822C1E58 → StreakMeter .text-start 0x822C4930 gap; pdata in
  the matching gap; no foreign range intersects. objects.json = 1 add-only NonMatching line.
- **Compile-gate re-run (direct cl.exe, RC=0):** only benign C4005 ObjMacros-vs-Object redefs +
  one C4003 in rndobj/Part.h (not this TU). `EndingBonus.h`'s `#include "obj/ObjMacros.h"` fix
  is present and required.
- **No MILO_DEBUG landmine:** all 5 size-exact ⇒ sizeof/layout retail-correct; no dev members removed.

**2ND-PASS VERDICT: CLEAR — landable as-is. +5 whole-binary (10897→10902), 2 true byte-100
anchors + 3 size-exact report-normalized-100, ICF-honest, splits/map ADD-ONLY & non-overlapping.**
