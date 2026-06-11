# AccomplishmentProgress compound fix — research dossier (2026-06-11)

**Status: SOLVED at the evidence level. The roadmap's proposed fix is WRONG in one
important way — do NOT apply RB3_RBTREE_0x1C to this TU at all.**

- Unit: `default/band3/meta_band/AccomplishmentProgress` (pinned `.text
  0x82577680–0x8257A960`, splits.txt:2466)
- Source: `src/band3/meta_band/AccomplishmentProgress.{h,cpp}` (fully ported,
  1325+ lines)
- Baseline (main @154a11a, report.json): unit **35/109 matched**, fuzzy 22.84%,
  binary total **6932** `measures.matched_functions`.
- Prior art: commit `00e2355` added `int unk50;` @0x50 (+8). Wave-2 batch-1
  applied `/DRB3_RBTREE_0x1C` alone = **−14, reverted**
  (docs/plans/decomp-state-and-roadmap-2026-06-09.md:496).

## TL;DR — the finding

In the retail AccomplishmentProgress TU, **`std::map` is 0x1c bytes while
`std::set` is 0x18 bytes — in the SAME TU.** This is proven byte-for-byte by
the retail dtor + its EH funclets (below). The "+4 deficit before the first
rbtree" framing in the roadmap was a misread: there is **no missing game
member**. The gap words at 0x610/0x62c/0x648 are **dead padding inside
`std::map`** — no instruction in the entire pinned unit ever reads or writes
them (verified by exhaustive scan of `build/45410914/asm/band3/meta_band/AccomplishmentProgress.s`).

The compound fix is therefore:

1. Add a gated 4-byte pad to **`std::map`/`std::multimap` only** (NOT
   `_Rb_tree`, NOT `set`) — new macro `RB3_MAP_0x1C` in
   `src/system/stlport/stl/_map.h`.
2. **Remove (gate off) the `unk50` compensation hack** in
   `AccomplishmentProgress.h` for this TU — it was compensating for
   `mStepTrackingMap` being 0x18 instead of 0x1c.
3. Do **NOT** define `RB3_RBTREE_0x1C` for this TU (it grows sets too → breaks
   the 0x5c–0xc4 anchors → that's the −14).
4. Add `target_symbol_map.json` entries for the ~12 functions identified below —
   their report 0% is a **pairing gap**, not divergence (verified:
   `?SetToursPlayed@...` → "Symbol not found in target").

This also **refines the R-B-tree ODR story** (`project_rbtree_4byte_deficit.md`):
at least for this TU the split is **map-vs-set**, not per-TU-uniform. The
AccomplishmentManager +28 precedent is fully consistent: AM's value members
0x20–0x170 are ALL maps (sets appear only as pointers,
`src/band3/meta_band/AccomplishmentManager.h:174-192`), so all-trees-0x1c ≡
maps-0x1c there. The TUs that "regressed" in the old rbtree sweep are predicted
to be the set-bearing ones — see Follow-ups.

## Evidence

### A. Retail dtor (`??1AccomplishmentProgress@@UAA@XZ` @ 0x82579CC0, 0x11c, currently 99.958 fuzzy / 69.0% strict)

`python3 scripts/analysis/diff_inspect.py --symbol '??1AccomplishmentProgress@@UAA@XZ' --unit 'default/band3/meta_band/AccomplishmentProgress' --compare-asm`

Members destroyed in reverse declaration order. ALL offsets equal except the
last three maps (stair-step +4/+8/+12):

| member (decl order)            | retail r3 | ours r3 | delta |
|--------------------------------|-----------|---------|-------|
| mStepTrackingMap (map)         | 0x30      | 0x30    | 0     |
| mGamerAwardStatusList (list)   | 0x54      | 0x54    | 0     |
| mAccomplishments (set)         | 0x5c      | 0x5c    | 0     |
| mNewlyAcquiredAccomps (set)    | 0x74      | 0x74    | 0     |
| unk7c (vector, buf ptr lwz)    | 0x8c      | 0x8c    | 0     |
| mAwards (set)                  | 0x9c      | 0x9c    | 0     |
| mNewAwards (list)              | 0xb4      | 0xb4    | 0     |
| mNewRewardVignettes (list)     | 0xbc      | 0xbc    | 0     |
| unkb0 (set)                    | 0xc4      | 0xc4    | 0     |
| mToursPlayedMap (map)          | 0x5f8     | 0x5f8   | 0     |
| mTourMostStarsMap (map)        | **0x614** | 0x610   | **+4** |
| mToursGotAllStarsMap (map)     | **0x630** | 0x628   | **+8** |
| mGigTypeCompletedMap (map)     | **0x64c** | 0x640   | **+12** |

Key deductions:
- Retail set spacing 0x5c→0x74→(…)→0x9c = **0x18** → retail `set<Symbol>` is 0x18.
- Retail trailing map spacing 0x5f8→0x614→0x630→0x64c = **0x1c** → retail
  `map<Symbol,int>` / `map<int,int>` is 0x1c.
- `mStepTrackingMap` (also `map<Symbol,int>`, so also 0x1c retail): 0x30 + 0x1c
  = 0x4c → mParentProfile @0x4c, mHardCoreStatusUpdatePending @0x50,
  mGamerAwardStatusList @0x54 — **without any unk50**. Our current build gets
  list@0x54 only because unk50 fills the hole left by our 0x18 map. Same
  anchor, two different explanations; retail's is the 0x1c map.

Ghidra cross-check (service @ http://127.0.0.1:8002/mcp was up;
`venv/bin/python tools/ghidra/ghidra-decompile.py 0x82579CC0`): tree dtors at
`puVar3 + 0x17e/0x185/0x18c/0x193` words = 0x5f8/0x614/0x630/0x64c, spacing 7
words = 0x1c; FixedSizeSaveable vtable stored at `puVar3[10]` = **FSS base
@0x28**; sets at +0x17/+0x1d/+0x27/+0x31 words = 0x5c/0x74/0x9c/0xc4. Exact
agreement.

### B. Dtor EH funclets (0x82579DDC–0x8257A06C, follow the dtor; frame slot 0xa4(r31) = saved `this`)

One funclet per destructible member, giving **member base addresses** (the
dtor proper shows tree-interior offsets for two of them; funclets are cleaner):
this+0 (~Hmx::Object), **0x28 (~FixedSizeSaveable base)**, 0x30, 0x54, 0x5c,
0x74, 0x8c, 0x9c, 0xb4, 0xbc, 0xc4, 0x5f8 — all **100% matched today** — then
`fn_82579FE8` (0x614), `fn_8257A014` (0x630), `fn_8257A040` (0x64c) at
**99.909** each (single addi-immediate diff: ours 0x610/0x628/0x640).

### C. The gap words are dead → STLport pad, not a game member

Exhaustive scan of the unit's target asm for any `(rN)`-relative access at
0x610/0x62c/0x648: **zero hits**. By contrast the tail members ARE accessed at
shifted addresses, fixing the tail layout:

- `mUploadDirty` retail **0x668** (ours 0x658): `stb @0x82577C00` (inside
  fn_82577978), `lbz @0x825784E0`, `stb @0x8257A094` (inside fn_8257A078)
- `unk645` retail **0x669**: fn_825776D8 (0x10), fn_82578F18 (0x64)
- `unk648` retail **0x66c**: fn_82577700 (0x30), fn_825785C8 (0xc0)
- predicted retail `sizeof(AccomplishmentProgress)` = **0x670** (ours today 0x660)

### D. Why `/DRB3_RBTREE_0x1C` alone was −14

The existing gate (`src/system/stlport/stl/_tree.h:318-329`, `_M_unused` after
`_M_key_compare`) grows **every** `_Rb_tree` — sets included. That shifts the
0x5c/0x74/0x8c/0x9c/0xb4/0xbc/0xc4 anchors (all currently matched, incl. 9
funclets) and double-shifts the middle region on top of unk50. The roadmap's
"find the member, THEN the flag" plan is refuted: **there is no member, and the
flag must never be applied to this TU.**

## The fix (exact edits)

### 1. `src/system/stlport/stl/_map.h` — gated pad in map + multimap

After `class map`'s `_Rep_type _M_t;  // red-black tree representing map`
(line 84):

```cpp
#if defined(RB3_MAP_0x1C)
  // Retail X360 RB3 (this TU): sizeof(std::map) == 0x1c while sizeof(std::set)
  // == 0x18 in the SAME TU (AccomplishmentProgress dtor funclets: trailing
  // maps spaced 0x1c at 0x5f8/0x614/0x630/0x64c, sets spaced 0x18 at
  // 0x5c/0x74/0x9c/0xc4). The extra word is dead — never read or written
  // anywhere in the unit. Pad the map class, NOT _Rb_tree (sets must stay 0x18).
  size_t _M_retail_pad;
#endif
```

Same block after `class multimap`'s `_Rep_type _M_t;` (line 271) for
consistency (no multimap in this TU; harmless).

Note: pad goes **after** `_M_t` so tree internals (`_M_node_count` @+0x10,
`_M_key_compare` @+0x14) keep their offsets — this is what preserves the
currently-100% `SetCurrentValue`/`UpdateTourPlayed` (operator[] on
mStepTrackingMap@0x30 / mToursPlayedMap@0x5f8, both bases unchanged).
`HX_NATIVE` never defines the macro → native build unaffected.

### 2. `src/band3/meta_band/AccomplishmentProgress.h:190` — gate the unk50 hack

```cpp
#ifndef RB3_MAP_0x1C
    // Compensation pad for TUs built with 0x18 maps. Retail truth: there is
    // NO member here — retail std::map is 0x1c in this TU (RB3_MAP_0x1C).
    int unk50;
#endif
```

This keeps every other includer (AccomplishmentManager.cpp [pinned, has
RB3_RBTREE_0x1C], AccomplishmentPanel.cpp, AccomplishmentPlayerConditional.cpp,
AccomplishmentTourConditional.cpp, TourDescPanel.cpp, BandProfile.h →
CampaignGoalsLeaderboardChoicePanel.h) **byte-identical** — they don't define
the macro, so they see today's exact layout. Blast radius outside this TU:
**zero by construction.** (`unk50` is never referenced in any .cpp — verified.)
BandProfile embeds AccomplishmentProgress by value @0xf4
(`src/band3/meta_band/BandProfile.h:158`) but BandProfile.cpp is neither
compiled nor pinned, and no flagged TU view changes.

### 3. `config/45410914/objects.json:681`

```json
"band3/meta_band/AccomplishmentProgress.cpp": { "status": "NonMatching", "extra_cflags": ["/DRB3_MAP_0x1C"] },
```

(Model: line 678's AccomplishmentManager entry. Do NOT also add
RB3_RBTREE_0x1C.) Then `python3 configure.py` to regenerate build.ninja.

### 4. `scripts/target_symbol_map.json` — pairing entries (the second half of the +N)

Verified: 0% here usually means **unpaired** (named symbols pair by name only;
`?SetToursPlayed@...` is absent from the target obj). Funclets pair via the
objdiff-fork's funclet machinery (that's why 9 of them already read 100%).
Identifications below are from member-offset fingerprints in the target asm;
**verify each with objdiff before/after adding** (add → `rm
build/45410914/target_symbol_renames.stamp && touch
config/45410914/config.yml && ninja` to re-run the renamer):

| target addr | size | accesses | proposed symbol (exact, from our obj) |
|-------------|------|----------|----------------------------------------|
| 0x82579560 | 0x50 | map@0x5f8 find | `?GetToursPlayed@AccomplishmentProgress@@QBAHVSymbol@@@Z` |
| 0x825795B0 | 0x50 | map@0x614 find | `?GetTourMostStars@AccomplishmentProgress@@QBAHVSymbol@@@Z` |
| 0x82579600 | 0x50 | map@0x630 find | `?GetToursGotAllStars@AccomplishmentProgress@@QBAHVSymbol@@@Z` |
| 0x82579650 | 0x50 | map@0x64c find | `?GetQuestCompletedCount@AccomplishmentProgress@@QBAHW4TourGameType@@@Z` |
| 0x8257A620 | 0x50 | map@0x5f8 op[] | `?SetToursPlayed@AccomplishmentProgress@@QAAXVSymbol@@H@Z` |
| 0x8257A6B8 | 0x50 | map@0x614 op[] | `?SetMostStars@AccomplishmentProgress@@QAAXVSymbol@@H@Z` |
| 0x8257A708 | 0x50 | map@0x630 op[] | `?SetToursGotAllStars@AccomplishmentProgress@@QAAXVSymbol@@H@Z` |
| 0x8257A758 | 0x50 | map@0x64c op[] | `?SetQuestCompletedCount@AccomplishmentProgress@@QAAXW4TourGameType@@H@Z` |
| 0x825784E0 | 0x38 | lbz 0x668 | `?IsUploadDirty@AccomplishmentProgress@@QBA_NXZ` (0x38 is large for it — verify; could be a Get*+flag combo) |
| 0x82577978 | 0x298 | stb 0x668 + map clears | `?Clear@AccomplishmentProgress@@QAAXXZ` (probable) |
| 0x8257A078 | 0x1b8 | ALL 4 maps + 0x668/0x669/0x66c | `?FakeFill@...` or `?SaveFixed@AccomplishmentProgress@@UBAXAAVFixedSizeSaveableStream@@@Z` — verify by bl pattern |
| 0x82577700 | 0x30 | 0x66c | unk648 accessor — identify vs oracle |
| 0x82578F18 | 0x64 | 0x669 | unk645-related — identify vs oracle |

Also **fix a wrong existing entry**: `0x825776D8 →
?IsRest@HamMove@@QBA_NXZ` is a stale/ICF-junk mapping — that fn accesses
`unk645@0x669`; correct identity is likely
`?HasNewRewardVignetteFestival@AccomplishmentProgress@@QBA_NXZ` or
`?ClearNewRewardVignetteFestival@AccomplishmentProgress@@QAAXXZ` (map-lint
obj_orphan class; `tools/map_lint.py --check obj_orphan`).

Alternative to hand entries: `tools/gen_game_target_map.py --tu
band3/meta_band/AccomplishmentProgress.cpp` (uses gitignored
`unified_id_rb3wii.json`; setup_worktree copies it) — but hand-curated entries
from the table above are higher precision.

## Predicted outcome

- **Near-certain (+4):** dtor 0x82579CC0 (99.958→100) + funclets
  0x82579FE8/0x8257A014/0x8257A040 (99.909→100). These pair already; the only
  diffs are the three map offsets.
- **High (+6–8):** the eight 0x50 Get*/Set* fns above — bodies are one-liner
  map find/op[]; the unshifted sibling pattern (SetToursPlayed only touches
  0x5f8) plus a map entry should land them; the shifted six additionally need
  the layout fix.
- **Medium (+2–6):** tail accessors (0x825784E0, 0x82577700, 0x82578F18,
  0x825776D8 remap), Clear (0x82577978), fn_8257A078, fn_825785C8.
- **Possible (+0–5):** fn_825796A8's funclet family (fn_825798AC 0%,
  fn_825798CC 99.9, fn_825798F4 99.9, fn_8257991C 93.9) — frame-local
  destructors whose slots (r31+0x60/0x68/0x70) may realign once a **local**
  std::map in the parent grows to 0x1c. fn_82578A58/fn_82578B44 (99.9/99.8) and
  `LoadStdPtr` (99.983) look like non-layout residuals — don't count them.

**Predicted net: +10 to +18 on this unit (floor +4).** `Poll` (1.143%) and
`??0GamerAwardStatus@@QAA@XZ` (70.1%) are body/ordering issues unrelated to
this lever — out of scope.

## Regression risks

1. **AM/CharClip TUs** (the two `/DRB3_RBTREE_0x1C` users, objects.json:210,678):
   zero change by construction (they don't define RB3_MAP_0x1C; header gate
   `#ifndef RB3_MAP_0x1C` keeps unk50 for them). **A/B must still confirm**
   their unit counts are unchanged.
2. **Within this TU:** any currently-matched fn with a *local* `std::map`
   changes stack frame (0x18→0x1c, frame rounds by 8). Scan of the matched set
   (GetBest*, SaveStdPtr/LoadStdPtr, HandleSuccessfulUpload [touches only
   unk7c@0x8c + set@0x74 — verified safe], SetCurrentValue, UpdateTourPlayed,
   UpdateMostStars [calls out-of-line accessors only]) found no map locals —
   risk low, A/B catches it.
3. The pad is uninitialized (mirrors retail: word is dead). If some *other*
   future TU's retail code zeroes it, placement may need revisiting — not
   relevant here.
4. `_map.h` is shared with the native build — macro never defined there.

## A/B plan (implementation agent)

```bash
scripts/setup_worktree.sh /tmp/wt-ap-compound ap-compound   # buildable CoW worktree
cd /tmp/wt-ap-compound
# Baseline (warm cache is fine if you diff against the worktree's own fresh report):
NINJA_JOBS=12 tools/fresh_report.sh   # record measures.matched_functions
# Apply edits 1-3, then:
python3 configure.py
rm -f build/45410914/target_symbol_renames.stamp
touch config/45410914/config.yml
NINJA_JOBS=12 tools/fresh_report.sh 2>&1 | tee /tmp/rb3_build_ap_compound.log
# Judge ONLY by report.json measures.matched_functions (whole binary) and the
# unit's matched_functions. NEVER by diff_inspect --diagnose headline or bare
# objdiff-cli strict [sym] output.
python3 - <<'EOF'
import json; r=json.load(open('build/45410914/report.json'))
print('total', r['measures']['matched_functions'])
u=[u for u in r['units'] if 'AccomplishmentProgress' in u['name']][0]
print('unit', u['measures']['matched_functions'], '/', u['measures']['total_functions'])
EOF
# Spot-check: dtor should be 100
python3 scripts/analysis/diff_inspect.py --symbol '??1AccomplishmentProgress@@UAA@XZ' \
  --unit 'default/band3/meta_band/AccomplishmentProgress' --compare-asm | head -20
# Confirm AM unchanged:
#   unit default/band3/meta_band/AccomplishmentManager matched_functions == baseline
# Then apply step 4 (map entries) one batch at a time, re-running
#   rm build/45410914/target_symbol_renames.stamp && touch config/45410914/config.yml && ninja-locked
# and re-checking the unit count after each batch; drop any entry that doesn't
# verify ≥99.9 (mis-ID).
```

## Follow-ups (separate A/Bs, not this campaign)

1. **Re-run the rbtree sweep with RB3_MAP_0x1C instead of RB3_RBTREE_0x1C** on
   the TUs that previously *regressed* under the tree flag
   (`tools/rbtree_blast.py`, `project_rbtree_4byte_deficit.md`). The map-vs-set
   split predicts those are the set-bearing TUs — potentially a multi-unit vein.
2. Consider migrating AccomplishmentManager.cpp / CharClip.cpp from
   RB3_RBTREE_0x1C → RB3_MAP_0x1C (AM's layout is map-only so offsets are
   identical; only *local* sets / set-bearing header classes inside the TU
   could differ). If both migrate and no TU needs tree-level 0x1c, the
   `_tree.h` gate can be retired and the unified story documented.
3. Update `project_rbtree_4byte_deficit.md` + the roadmap addendum: per-TU ODR
   framing → map(0x1c)-vs-set(0x18) framing; AP "unk50 retail member" comment
   (header line 190 + the _tree.h:323 comment block referencing "AP mBestSolo
   needs 0x18 maps + real unk50") are now known stale — fix both comments in
   the implementation commit.
4. After the unit settles, `Poll`/`??0GamerAwardStatus`/`LoadStdPtr` are
   ordinary body-port near-misses for a later bodyport batch.
