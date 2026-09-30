# W16-GZ — packed local-static guard sweep, 13 named rows

Lane W16-GZ (first-pass implementer). Worktree `/home/free/tmp/wt-w16-gz`,
branch `w16-gz`, based on `main` merge-base `9de0f3339`. Task: 13 named rows
flagged by prior lane W16-GA with a "packed local-static guard" defect —
retail packs up to 32 function-local statics into one guard integer, one bit
per static, claimed in declaration/construction order (not necessarily
textual/execution order); our source was comparing against centralized
`utl/Symbols*.h` globals instead of declaring the statics function-locally,
so the guard-check/construction prologue retail emits was entirely absent
from our object.

All 13 rows are committed individually on the branch (one commit per row,
per the task brief). Method per row: read the guard word, per-bit claim
order and each static's initializer off retail's full instruction listing
(`run_objdiff` full_listing / `objdiff-cli diff`), add the missing
function-local statics in that exact order, rebuild with `./tools/ninja-locked`,
score with `objdiff-cli diff -p . -u <unit> '<symbol>' -f json` (no `--build`,
since the six post-compile object patchers are part of the ruler). Stop rule:
stop once the guard-bit layout matches retail (size gap closed) and
`run_diff_inspect mode=diagnose` shows every remaining `diff_arg` instruction
"Explained by root causes" with "Unexplained: 0" and `diff_op: none` — i.e.
pure register-allocation/scheduling noise, not a named source construct.

## Per-row results

### 1. AccomplishmentCategory::Configure — commit `bcc8c2ba9`
File: `src/band3/meta_band/AccomplishmentCategory.cpp`.
Retail declares `group` and `award` as function-local static Symbols sharing
one guard word, `group` claiming bit 0x1 and `award` bit 0x2, in call order —
not the `utl/Symbols.h` globals. Converting the two lookups to inline
`static Symbol` locals in that order reaches byte-exact.
**fuzzy 38.57 -> 100.0** (+188 B, +1 fn). No residual.

### 2. BandSongMgr::RankTierToken — commit `052c68349`
File: `src/band3/meta_band/BandSongMgr.cpp`.
Retail declares `song_groupings`, `rank` and `band` as function-local static
Symbols sharing one guard word, claimed in that order (bits 0x1, 0x2, 0x4 —
matches left-to-right expression evaluation) instead of the centralized
globals. **fuzzy 24.96 -> 100.0** (+224 B, target==base==224 B). No residual.

### 3. BandSongMetadata::Rank — commit `e9b0376f4`
File: `src/band3/meta_band/BandSongMetadata.cpp`.
Retail declares `real_guitar` and `real_bass` as function-local static
Symbols sharing one guard word (`real_guitar` 0x1, `real_bass` 0x2, in
source order). **fuzzy 48.15 -> 100.0** (+236 B). One row-alignment artifact
resolved automatically: an apparent wrong-callee diff at the `ID()`/
`GetUpgradeData` call site (target showed `NetSavedSetlist::GetType`, base
showed `SongMetadata::ID`) was a position-drift artifact from the missing
guard prologue, not a real defect — it disappeared once the statics were
added.

### 4. BandDirector::GetModeInst — commit `ffb92c924`
File: `src/system/bandobj/BandDirector.cpp`.
Retail packs `coop_bk` (bit 0x1), `coop_gk` (bit 0x2) and `keyboard`
(bit 0x4) as function-local statics sharing guard word `lbl_82CBC9E4`,
declared together at the top of the "guitar"/"bass" block. Bit identity
traced by register provenance: bit 0x1's storage feeds the `playmode`
comparison inside the `s=="guitar"` branch, bit 0x2's feeds `s=="bass"`,
bit 0x4's storage is what's stored into the return slot on both paths.
**fuzzy 53.30 -> 100.0** (target_size 332 == base_size 332, diff_score
0/8300). A pre-fix `WRONG_CALLEE`/`PROLOGUE_MISMATCH` pattern-detector flag
was the same instruction-misalignment artifact seen on other rows — resolved
automatically once the guard prologue was added.

### 5. OutfitConfig::MeshAO PropSync — commit `9247e951e`
File: `src/system/bandobj/OutfitConfig.cpp`.
Symbol: `?PropSync@@YA_NAAVMeshAO@OutfitConfig@@AAVDataNode@@PAVDataArray@@HW4PropOp@@@Z`.
Retail declares `meshname`/`coeffs`/`seams` as function-local statics sharing
guard word `lbl_82CBCCE0`, claimed in source order (meshname 0x1, coeffs
0x2, seams 0x4) — confirmed by cross-referencing each guard block's later
reference against base's `?meshname@@3VSymbol@@A`/`?coeffs@@3VSymbol@@A`/
`?seams@@3VSymbol@@A` positional hits, in order, matching the existing
`SYNC_PROP` call order exactly. Unlike every other row in this lane, the
PRE-FIX object was LARGER than retail (360 B target vs 224 B base): this TU
has no `RB3_SYNCPROP_LOCAL_STATIC` flag set in `objects.json` (only
`RB3_HANDLE_LOCAL_STATIC`/`RB3_NOTIFY_ONCE_EVAL`), so `SYNC_PROP` expands to
the plain global-compare form here. Fixed with explicit interleaved
`static Symbol x("x")` declarations before each `SYNC_PROP` call, matching
the spelling the adjacent `OutfitConfig::Overlay` block already uses — this
works under both macro forms without touching any per-TU `/D` flag.
**fuzzy 59.92 -> 100.0** (target_size 360 == base_size 360, diff_score
0/9000).

### 6. GameConfig::GetController — commit `790b5cebe`
File: `src/band3/game/GameConfig.cpp`.
Retail declares `joypad`/`controller_mapping` as function-local statics
sharing guard word `lbl_82E02424`, claimed in that order (joypad 0x1,
controller_mapping 0x2) — confirmed by tracing each storage pointer
(`lbl_82E02420`/`lbl_82E0241C`) forward to the `SystemConfig(joypad,
controller_mapping)` call site.
**★ Generalizable finding**: the first attempt placed the two
`static Symbol` declarations immediately before the `SystemConfig()` call
(their only use site) and matched byte size exactly (404==404) but fuzzy
REGRESSED (70.62% -> 53.68%), with a persisting prologue mismatch (target
saves r25-r31, base only r26-r31). Root cause: **MSVC emits a local
static's guard-check/construction code at its textual DECLARATION point,
not at first use.** Retail's guard machinery sits at the very top of the
function, before `GetGameplayOptions()`, so the declarations had to move
there too. Moving both statics to the top of the function (before
`bool lefty`) fixed it.
**fuzzy 70.62 -> [regressed 53.68] -> 100.0** (target_size == base_size ==
404, diff_score 0/10100).

### 7. MetaPerformer::GetSetlistMaxVocalParts — commit `d6682f9d5` (stopped)
File: `src/band3/meta_band/MetaPerformer.cpp`.
Retail declares `any` and `random` as function-local static Symbols sharing
one guard word (any 0x1, random 0x2, in source/condition order), not the
`utl/Symbols.h` globals. `gNullStr` is a plain global in retail too and left
untouched (its retail address is the unnamed `lbl_82C71838` — a map-naming
gap, out of scope for this lane).
**fuzzy 53.26 -> 90.20548** (size gap closed exactly: 204 -> 292 B, matching
retail). **Stopped per the stop rule**: `run_diff_inspect diagnose` shows
all 22 remaining `diff_arg` instructions fully explained by three root
causes (offset shifts, GPR register swaps, symbol relocations), Unexplained
0, `diff_op: none`.

### 8. MetaPerformer::PartPlaysInSet — commit `99c52f0f9` (stopped)
File: `src/band3/meta_band/MetaPerformer.cpp`.
Same pattern as row 7 in the same file: `any`/`random` function-local
static Symbols, same guard-word/bit convention, `gNullStr` untouched (same
map-naming gap).
**fuzzy 57.69620 -> 90.82278** (size gap closed exactly: 228 -> 316 B).
**Stopped per the stop rule**: 25 remaining `diff_arg` instructions, all
explained by the same three root causes, Unexplained 0, `diff_op: none`.

### 9. MetaPerformer::GetHighestDifficultyForPart — commit `2df2e01f5` (stopped)
File: `src/band3/meta_band/MetaPerformer.cpp`.
Same guard word layout as the two sibling functions above (any 0x1, random
0x2, guard word `lbl_82DFE9AC`), confirmed independently via `run_objdiff`
full listing before editing.
**fuzzy 62.67033 -> 91.95605** (target_size 364 == base_size 364, was 364
vs 276). **Stopped per the stop rule**: 23 remaining `diff_arg` instructions
(24 register-swap, 2 offset-shift, 6 symbol-reloc — arithmetic as reported
in the commit), Unexplained 0, `diff_op: none`.

### 10. PlayerLeaderboard::OnSelectRow — commit `9a2ef621d` (stopped)
File: `src/band3/meta_band/PlayerLeaderboards.cpp`.
Retail guard word packs three local-static Symbols (`pad_error`,
`privilege_error`, `gamertag_error`) at bits 0-2 in declaration order, all
constructed unconditionally right after the two early-return bounds checks
(`IsEnumComplete`/`NumData`) and before the sign-in gate.
**Scope note**: this row required substantially more than the guard fix —
the bulk of the function's logic (sign-in gate, `OnlineID` copy +
`ShowGamercard` call, and the `PrivilegeFailed`/`NotSignedIn`/`Success`/
fallback result chain) had no prior implementation at all and no oracle in
`../rb3` or `../dc3-decomp`; it is novel reconstruction from retail bytes,
outside this lane's nominal "add missing statics" scope, flagged here since
it carries a different risk profile than the other 12 rows.
Two structural findings about retail's tail-merging: (a) the two early
bounds checks share ONE physical destination in retail (a single far-away
`gNullStr` construction); writing them as a combined `&&` condition instead
of two sequential `if return` statements reproduced this (82.0 -> 87.0
fuzzy, fixing one of two `diff_op` branch-polarity mismatches) — a
nested-if variant that ALSO hoisted the statics above the bounds checks was
tried first and regressed badly (23.9% fuzzy): **the statics must stay in
their retail position, inside the guarded block, not hoisted to the top.**
(b) Same mechanism for `NotSignedIn` vs. the `ShowGamercard`-result
fallback — folding `NotSignedIn` into the "not Success" condition, funneling
both through one trailing `return gamertag_error;`, reproduced retail's
shared destination (87.0 -> 92.7 fuzzy, clearing the last `diff_op`
mismatch).
**fuzzy 82.0 -> 87.0 -> 92.7**. **Stopped per the stop rule** (`diff_op:
none`, all `diff_arg` explained).
**Residual for reviewer**: retail's `OnlineID oid = ...` compiles to a raw
5-instruction `ld`/`std` struct copy with no constructor call, implying a
compiler-IMPLICIT (trivial) `OnlineID` copy ctor. Our shared
`os/OnlineID.h` explicitly declares `OnlineID(const OnlineID &);`
out-of-line, forcing a real call at every `OnlineID` copy site in the whole
binary, not just this one — fixing it means changing shared-header behavior
with unknown blast radius elsewhere, out of scope for a single named row
(documented in a code comment in the row for whoever next touches
`OnlineID.h`). Also: `kShowGamercardResult_Offline` and
`kShowGamercardResult_Failed` are both `-1` in `PlatformMgr.h`; retail bytes
only prove the numeric value `-1` reaches this path, not which enumerator
name the original source spelled.

### 11. BandDirector::EnterVenue — commit `09f2bcb1d`
File: `src/system/bandobj/BandDirector.cpp`.
Retail packs three function-local statics into guard word `lbl_82CBCB34`,
claimed bit0..bit2 in program order (zero local statics existed here
before this fix — the whole layout was read fresh off the 151-instruction
target listing):
- bit0 `lbl_82CBCB2C`: `static Message("remove_midi_parsers")`, inside the
  `if (mCurWorld)` guard, built via temp-stack-Symbol then
  `Message::Message(Symbol)` then `atexit(fn_82C43FE0)`. Replaces the old
  shared-global `remove_midi_parsers_msg` reference (`Messages3.h`) — that
  global has no `.cpp` definition anywhere in the tree (`LITERAL_MSG` is
  declared but never invoked), so a local static is the only way retail
  could have constructed it here.
- bit1 `lbl_82CBCB28`: `static Symbol("venue")`, constructed directly (no
  `Message` wrapper, no `atexit`), unconditionally right after the
  `TheCrowdAudio->SetBank` call. Its value is never read again anywhere in
  this function's retail bytes; kept as declared-but-locally-unused for
  guard-bit parity only.
- bit2 `lbl_82CBCB20`: `static Message("setup_midi_parsers")`, same shape
  as bit0, replacing the old `setup_midi_parsers_msg` global.
Resolved the three string literals with `tools/xex_string_at.py`
(`0x82015DE8`/`0x82011FCC`/`0x82015DD4`) to confirm the exact text before
writing the fix.
**fuzzy 61.43 -> 100.0** (604/604 bytes, diff_score 0/15100).
**Residual for reviewer** (not chased, out of scope): idx148's
`bl fn_82291210` in retail vs. our
`bl ?ClearLighting@BandDirector@@QAAXXZ` is very likely the same function
under an unidentified target address (matching call shape, same trailing
position after the `Release`+`SetObjConcrete` block) — a naming gap, not a
guard-layout defect, flagged for a map lane.

### 12. MicInputArrow::Handle — commit `01cac5408`
File: `src/system/bandobj/MicInputArrow.cpp`.
Retail packs one guard word (`lbl_82CBDCD8`) with five function-local static
Symbols, claimed bit0..bit4 in top-to-bottom `HANDLE_ACTION` order (read off
the full 178-instruction target listing): `set_mic_mgr` 0x1, `set_mic_connected`
0x2, `set_mic_extended` 0x4, `set_mic_preview` 0x8, `set_mic_hidden` 0x10 —
each built via `??0Symbol@@QAA@PBD@Z` directly into guarded static storage
(no `Message` wrapper, no `atexit`; `Symbol`'s dtor is trivial). Our
`HANDLE_ACTION` macro expanded to a compare against the centralized
`utl/Symbols.h` globals — exactly the `RB3_HANDLE_LOCAL_STATIC` dialect
(`ObjMacros.h`), but that gate is a per-TU `/D` flag in
`config/45410914/objects.json`, out of this lane's scope to touch, so
`HANDLE_ACTION` was overridden TU-locally instead (same lever as the
`SYNC_PROP` override on the same file, precedent from lane CT-4). Also
applied the same TU-local override to `SYNC_PROP`/`SYNC_PROP*` for
`MicInputArrow::PropSync`, and converted `MicInputArrow::Update`'s six
`DataArray`/`float` lookups (`connected_triggers`, `disconnected_triggers`,
`hidden_triggers`, `preview_triggers`, `extended_triggers`, `level_anims`,
`mic_energy_normalizer`) to function-local static Symbols sharing one guard
word with bits 0x1..0x40, per the same defect pattern (not one of the 13
named rows itself, but the same file's other guarded functions — done as
part of reaching this row cleanly per the file's own retail layout).
**100.0% fuzzy, 712/712 bytes.** ⚠ Caveat: the pre-fix score for this row
was not freshly re-verified in this session (inherited from a prior working
session) — the whole-binary A/B below is the ground truth if it disagrees.

### 13. VocalTrackDir::ApplyFontStyle — commit `2a4d6abbb`
File: `src/system/bandobj/VocalTrackDir.cpp`.
Retail packs one guard word (`lbl_82CBD71C`) with five function-local static
Symbols, claimed bit0..bit4 in this order (read off the full
299-instruction target listing: `clrlwi.`/`rlwinm.` extract bits
31,30,29,28,27 respectively — mask 0x1,0x2,0x4,0x8,0x10 — one Symbol ctor
(`??0Symbol@@QAA@PBD@Z`) per bit, straight into guarded static storage):
`font_style` 0x1 (constructed unconditionally, before the `if (o)` null
check), `lead_text` 0x2, `harmony_text` 0x4, `lead_phoneme_text` 0x8,
`harmony_phoneme_text` 0x10 (these four constructed unconditionally at the
top of the `type_matched` block, all four BEFORE the
`FindObject("milo",...)` lookup even though each is first used later). Our
source compared against the centralized `utl/Symbols2.h`/`Symbols3.h`
globals of the same names; shadowed them with function-local statics of the
same identifiers (legal — the local hides the extern for the rest of the
function).
**fuzzy 74.32302 -> 100.0** (target_size == base_size == 1164, diff_score
0/29100, masked_equal_rows 52). Pre-fix score was measured, not estimated:
temporarily reverted to the pre-edit file, rebuilt, scored
(target_size 1164, base_size 956, diff_score 7472/29100).
Two residual findings: (a) a row-alignment artifact resolved automatically
once the guard prologue was added (same class as rows 3/4 above); (b) a
target-only vs. base-only call-target name difference
(`SetObjConcrete<BandCharacter>` vs. `SetObjConcrete<RndText>`) at 8 sites
in this function, which reads as benign linker-fold (ICF/COMDAT) naming
noise rather than a real type/logic bug — the accompanying RTTI descriptor
args (`__RTDynamicCast`) agree between target and base at every site.
Fixing it would require an entry in `scripts/symbol_aliases.json`, off-limits
for this lane; **accepted as a residual for a map lane.**

## Whole-binary A/B measurement

Measured with `tools/ab_measure.py --worktree /home/free/tmp/wt-w16-gz-ab
--patch ~/tmp/w16gz_ab.patch` (run label `w16gz-13rows`, pid 3459698). The
lane worktree `/home/free/tmp/wt-w16-gz` already has all 13 commits on HEAD,
so it cannot itself be leg A (leg A must be the pre-fix state); instead a
disposable scratch worktree was created with
`scripts/setup_worktree.sh ~/tmp/wt-w16-gz-ab w16-gz-ab-measure 9de0f3339`
(branch `w16-gz-ab-measure`, at the same merge-base `9de0f3339` the 13 commits
are stacked on), and the combined diff of all 13 row-fix commits
(`git diff 9de0f3339 2a4d6abbb`, 372 lines, exactly the 10 files listed in the
per-row sections above, no off-limits paths) was applied there as `--patch`.
The tool classified the patch as `kinds=['source']`, settled both legs to
zero build-work before reading (leg A: 0 recompiles; leg B: 28 msvc recompiles
across 2 settle iterations, matching the 10 touched files' fan-out), wiped
`report.json`/cache before each read, and verified the tree was restored to
its exact pre-run state afterward. Full log: `~/tmp/w16gz_ab_measure.log`;
archived artifacts (including both legs' `report.json.gz`) under
`/home/free/tmp/wt-w16-gz-ab/.ab_measure_runs/20260930-072809-w16gz-13rows-3459698/`.

Measured result, verbatim from the tool:

```
leg A: matched=44343 masked=23324 honest=21019 code%=41.415943  (recompiles: 0, settled)
leg B: matched=44357 masked=23329 honest=21028 code%=41.458805  (recompiles: 28, split=0, patch_steps=6, settle iterations: 2)
Δmatched=+14  Δmasked_equal=+5  Δhonest=+9  Δcode%=+0.042862pp  Δcode_bytes=+4392
Δfuzzy=+0.021800pp   (legA 50.514090 -> legB 50.535890)
unit improvements: 9 unit(s), sum +14
    +3  default/AccomplishmentCategory  (2->5)
    +2  default/BandDirector  (506->508)
    +2  default/MicInputArrow  (55->57)
    +2  default/PlayerLeaderboards  (19->21)
    +1  default/BandSongMetadata  (219->220)
    +1  default/BandSongMgr  (142->143)
    +1  default/GameConfig  (75->76)
    +1  default/OutfitConfig  (185->186)
    +1  default/VocalTrackDir  (487->488)
unit net (ALL units) = +14   vs whole-binary Δmatched = +14
units at 100% [mpn ruler]: legA 207 -> legB 208  (Δ+1; 1 reached 100, 0 fell off; pairable units 1742->1742)
  +100%  default/AccomplishmentCategory  matched 2->5 (Δ+3)  rows 5->5 (Δ+0)  MATCHED_ROSE
units at 100% [all-rows-fuzzy ruler]: legA 181 -> legB 182  (Δ+1; 1 reached 100, 0 fell off; pairable units 1742->1742)
  +100%  default/AccomplishmentCategory  matched 2->5 (Δ+3)  rows 5->5 (Δ+0)  MATCHED_ROSE
```

`[control none]` leg: `Δmatched_code=+4392 B` on the `functionRelocDiffs=none`
ruler too (same magnitude as the default ruler) — the tool labels this
`NOT_APPLICABLE` for a source-kind patch ("this patch can move real code, so
movement on `none` is EXPECTED and means nothing is wrong"), i.e. this is
real code-shape improvement, not alias-suspect forgiveness.

**No row fell.** All 9 per-unit deltas are positive and sum exactly to the
whole-binary Δmatched of +14 (the tool's own internal consistency check:
"unit net (ALL units) = +14 vs whole-binary Δmatched = +14").

**Reconciliation against `docs/decomp/W16GZ_PREDICTIONS_2026-09-30.md`**
(written and committed before this measurement):

| measure | predicted | measured | note |
|---|---:|---:|---|
| `matched_code` (Δbytes) | +4,224 | **+4,392** | +168 B above prediction |
| `matched_functions` (Δ) | +9 to +13 | **+14** | 1 above the predicted ceiling |
| `masked_equal_functions` (Δ) | +0 expected | **+5** | unpredicted — see below |
| rows fallen | 0 expected | **0** | confirmed |

Three discrepancies, stated honestly rather than smoothed over:

1. **+168 B above the predicted +4,224.** The prediction only summed the 9
   rows that reached `fuzzy == 100` on their own single-symbol scores
   (188+224+236+332+360+404+604+712+1164 = 4,224, verified by recomputing the
   sum, not just re-quoting it). The extra 168 B is not accounted for by any
   of the 13 named rows individually; the most likely source is
   `MicInputArrow::Update`'s six `DataArray`/`float` lookups, which were
   converted to function-local statics as part of reaching row 12 cleanly
   (see row 12's write-up) but are **not one of the 13 named rows** and were
   never scored standalone. This is consistent with the per-unit table
   showing `default/MicInputArrow +2` rather than +1 — i.e. a second function
   in that TU (not `Handle`) also crossed to fuzzy 100 as a side effect. This
   explanation is stated as the most likely cause, not verified via a
   per-function report.json diff — flagged as a question for the reviewer if
   exact attribution matters.
2. **+14 vs a predicted ceiling of +13.** The predictions doc explicitly
   flagged 0-4 additional matches as uncertain from the 4 "stopped" rows
   (7-10), since `mpn` excludes arg-only penalties and all four reach
   `diff_op: none`. The measured +14 is only 1 above the stated ceiling of
   9+4=13, i.e. within the acknowledged uncertainty band, not a surprise in
   kind — just slightly larger than the stated worst case. Also consistent
   with the per-unit table's `+2`s for `BandDirector` (2 named rows in that
   file: EnterVenue + GetModeInst, exactly matching +2) and `PlayerLeaderboards`
   (1 named row, OnSelectRow — the stopped row plausibly counted under `mpn`
   plus one more function in that TU, unverified).
3. **+5 masked_equal — genuinely unpredicted.** The predictions doc expected
   `+0` on the reasoning that no map/alias file was touched
   (`scripts/symbol_aliases.json` / `scripts/target_symbol_map.json` both
   untouched, confirmed by the patch's file list). That reasoning was
   incomplete: `masked_equal_functions` increments whenever a row's
   `match_percent_normalized` reads 100 via ICF/COMDAT byte-signature
   disclosure, which can happen as a side effect of ordinary source changes
   (e.g. a newly-added local-static's constructor call folding against an
   existing identical-body COMDAT elsewhere), not only from map/alias edits.
   `Δhonest` (matched − masked_equal) is **+9**, which lines up closely with
   the 9 rows that were predicted to cross outright — a reassuring
   cross-check that the "real" (non-folded) gain is close to the confident
   prediction, with the +5 masked_equal and the extra function/byte counted
   separately rather than asserted to be understood in full. Not chased
   further this session; flagged as a question for the reviewer below.

## Native build gate

Run in the lane worktree (`/home/free/tmp/wt-w16-gz`) since this lane edited
shared `src/` files (`src/system/bandobj/*.cpp`), per house rule. Full
18/18 pass, 0 SKIPs, rc=0 — no seeding was needed (native/CMakeLists.txt
resolves its siblings from the real repo automatically). Exact verdict line:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

## Questions for the reviewer

1. **PlayerLeaderboard::OnSelectRow** (`OnlineID.h`): retail's `OnlineID`
   copy is implicit/trivial (raw struct copy) but our shared
   `os/OnlineID.h` declares an explicit out-of-line copy ctor, forcing a
   real call at every `OnlineID` copy site binary-wide. Changing this is a
   shared-header change with unknown blast radius — flagging for a
   dedicated lane rather than doing it inline on one row.
2. **BandDirector::EnterVenue**: idx148's `bl fn_82291210` (retail) is very
   likely our `?ClearLighting@BandDirector@@QAAXXZ` under an unidentified
   map address — a naming-gap candidate for a map lane.
3. **MetaPerformer::GetSetlistMaxVocalParts** / **PartPlaysInSet**: retail's
   `lbl_82C71838` is almost certainly the unnamed `gNullStr` — another
   naming-gap candidate.
4. **VocalTrackDir::ApplyFontStyle**: `SetObjConcrete<BandCharacter>` vs.
   `SetObjConcrete<RndText>` at 8 call sites reads as ICF-fold naming noise
   (RTTI args agree); would need a `scripts/symbol_aliases.json` entry,
   which this lane's guardrails exclude — candidate for the alias-adjudication
   track.
5. The 4 "stopped" rows (7-10 above) all reach `diff_op: none` with every
   `diff_arg` explained by register/offset/symbol-reloc noise — whether they
   already counted toward `matched_functions` under `mpn` pre-fix, or only
   flip post-fix, is resolved by the whole-binary A/B above rather than
   guessed.
