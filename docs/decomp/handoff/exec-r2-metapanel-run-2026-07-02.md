# exec/r2-metapanel — ws7 R2 MetaPanel/AppLabel axis-A lever + reharvest (2026-07-02)

Planning agent record. Stream doc: `docs/plans/workstreams-2026-07-02/ws7-dead-lever-reaudit.md`
(Part 1, R2). Kill-doc parent: `docs/decomp/identity-transfer/B2-FINDINGS-oracle-wall.md`.

## Phase-0 verification (planner-run, all LIVE)

1. **State re-verified on main:** `?Unload@MetaPanel@@UAAXXZ` = 99.9% normalized,
   exactly 3 diff_arg (`lwz`/`addi`/`stw`, all `off:+4`). `mMusic // 0x5c` in
   `src/band3/meta_band/MetaPanel.h`, untouched since scaffold. MetaPanel pinned
   span on main covers ONLY Unload (0x8255A890–0x8255A8F0, 0x60 B, 1 fn).
2. **Decisive probe RAN — fires, with the doc's direction INVERTED.**
   - Doc said "insert a 4-byte member before mMusic". Inserting moved the deltas
     +4 → +8 (worse). Objdiff `off:+N` here = base − target: our layout is 4 bytes
     too FAT before mMusic, not too thin.
   - Mirrored probe (shrink by 4: move `int unk58;` to after `mSongPreview`):
     **Unload flipped 99.9 → 100.0% normalized, 24/24 instructions equal**
     (99.6% raw = reloc naming, expected TRUE after composed renamer — per
     stream rules do NOT claim strict until the composed A/B).
   - Ruling: probe FIRES (the lever is a single 4-byte axis-A shift, direction
     shrink). Stream lives.
3. **Second-order evidence (planner ran, extended pin in this worktree):**
   pin extended to 0x8255A890–0x8255B42C (through PickLoopIndex); PickLoopIndex
   access flipped to protected (target map name is `?PickLoopIndex@MetaPanel@@IAAHH@Z`
   = protected; our header had it public = QAA → symbol-pair failure, false 0%).
   PickLoopIndex now pairs at **94.2%** and pins the retail layout further:
   - `unk58` (loop counter) accesses show `off:+140` → base 0xd0 (probe position)
     − 0x8C = **retail counter at 0x44**.
   - `mRecentIndices` begin/end loads show `off:+4` → **retail vector at 0x4c**.
   - Unload's flip fixes mMusic/mSongPreview → **retail mMusic 0x58, mSongPreview 0x5c**
     (0x5c + 0x74 = 0xd0 for whatever follows).
   - Residual PickLoopIndex diffs beyond offsets: loop-shape diffs
     ([13] blt, [14] li reg, [15] insert li, [24] addi off:-3 / [25] insert addi)
     — body-source tweak territory, may or may not clear once offsets are right.
4. **Retail layout hypothesis** (worker must finalize — 5 slots 0x38..0x4c hold
   mTour, mCampaign, mNameGenerator + counter + ONE of {mMetaMusicMgr, mHAQMgr};
   the other pointer must live after mSongPreview, likely 0xd0 region):
   ```
   0x38 mTour, 0x3c mCampaign, 0x40 mNameGenerator,
   0x44 unk58 (counter, retail-true), 0x48 <one of mMetaMusicMgr/mHAQMgr>,
   0x4c mRecentIndices (0xC), 0x58 mMusic, 0x5c mSongPreview (0x74),
   0xd0 <the other pointer / unkd4 cluster>, ...
   ```
   `unk44` is referenced NOWHERE in the cpp — it does not exist in retail; delete it.
   `unkd4` is real (MetaPanel.cpp:416,429). Discriminate mMetaMusicMgr vs mHAQMgr
   position via UpdateMetaMusic/HAQ-touching fns in the extended span, or ctor asm.
5. **Oracle cross-check:** rb3-Wii header (`../rb3/src/band3/meta_band/MetaPanel.h`)
   has the SAME (wrong-for-retail) order we scaffolded — Wii dev layout drifts from
   retail 360; per B2 doc DC3 drifts the other way (mMusic 0x60/0x64). Neither
   oracle is authoritative here; the target asm is.
6. **oracle_quality re-run (matches doc):** `MetaPanel.cpp: real=35 GOOD=11
   mis-size=12 foreign=12 stub=3`. 8/11 GOOD are foreign `NewObject()` boilerplate;
   genuine = Exit (wii32/retail64), Enter (wii32/retail72), Exiting (0x8255a980,
   retail 120 = fn_8255A980 0x78, IN the extended span). Oracle VAs for Enter/Exit
   (0x822d1738 / 0x8232c410) are far outside the MetaPanel cluster = the known VA
   misattribution class; in-cluster size candidates: Exit=fn_8255A940 (0x40),
   Enter=fn_8255AA48 (0x48).
7. **AppLabel live state:** span already pinned (0x825ACF88–0x825AF938),
   100 target fns, **52 already perfect**, 48 at 0% — all unpaired `fn_` (no map
   entries), incl. 2 foreign-named fns (AccomplishmentManager, SpeechRecoMessage)
   → partial misattribution in span; screen with oracle_quality before pairing.

## Worktree state handed to the worker (probe edits LEFT IN PLACE)

- Worktree: `/home/free/tmp/wt-exec-r2-metapanel`, branch `exec/r2-metapanel-0702`.
- Baseline report snapshot (pre-any-edit): `/home/free/tmp/exec-r2-metapanel-baseline-report.json`.
- Dirty files (probe state, worker finalizes or replaces):
  - `src/band3/meta_band/MetaPanel.h` — variant-A probe: unk58 moved after
    mSongPreview (KNOWN-WRONG position; retail counter is 0x44 — see #3),
    PickLoopIndex moved to a `protected:` block.
  - `config/45410914/splits.txt` — MetaPanel .text extended to
    `start:0x8255A890 end:0x8255B42C`, .pdata line dropped (dtk re-derives).
- `configure.py` + `touch config.yml` already run; MetaPanel.obj builds clean.

## Success / kill bars (from stream doc, unchanged)

- Success: ≥ +3 net strict across MetaPanel+AppLabel, 0 regressions in the
  composed whole-binary A/B vs the baseline snapshot.
- Kill (Lane 2): skip AppLabel iff MetaPanel nets < 2 strict.
- Kill (record wall): post-fix reharvest nets < 2 strict → record panel axis-A
  as body-divergence-in-disguise, close permanently.

## Worker results (Opus, 2026-07-02) — packet metapanel-axisa

### Phase A — retail MetaPanel layout FINALIZED (target asm is authority)

The planner's variant-A probe (unk58 moved after mSongPreview) was a red herring: it
fixed Unload superficially while leaving a uniform `off:+4` on the whole tail. Ground
truth from the pinned target asm:

- `fn_8255A890` (Unload): `lwz r3,0x60(r31)` = mMusic@0x60; `addi r3,r31,0x64` = mSongPreview@0x64.
- `fn_8255B378` (PickLoopIndex): `lwz 0x50/0x54` = mRecentIndices@0x50 (vector 0xC);
  `lwz/stw 0x5c` = unk58 counter@0x5c.

Root cause of the persistent `off:+4`: **`int unk44` does not exist in retail** (it is
referenced nowhere in the cpp). Deleting it shifts the whole tail down 4 and every
offset diff vanishes. UIPanel's virtual base makes mTour land at 0x3c (not the scaffold's
guessed 0x38). FINAL retail layout (all offsets verified by objdiff going to 0):

```
0x3c mTour, 0x40 mCampaign, 0x44 mNameGenerator, 0x48 mMetaMusicMgr, 0x4c mHAQMgr,
0x50 mRecentIndices (std::vector<int>, 0xC), 0x5c unk58 (loop counter),
0x60 mMusic, 0x64 mSongPreview (0x74), 0xd8 unkd4.
```

PickLoopIndex body: after offsets, 2 source rewrites closed it to 100.0:
1. inner dup-check loop uses a single counter (`for(;count<prevSize;count++)` with
   `idx == mRecentIndices[count]`) instead of separate `i`+`count` — collapses 3 GPRs to 2.
2. first break (`numLoops < prevSize+2`) is an **early `return idx;`** (target jumps to the
   epilogue, skipping the `mRecentIndices[unk58++]` store) — not a `break`.

### Phase B — in-span reharvest (target_symbol_map.json entries added)

| symbol | target fn | result |
|---|---|---|
| `?Unload@MetaPanel@@UAAXXZ` | fn_8255A890 (pre-existing) | **100.0** normalized (24/24) |
| `?PickLoopIndex@MetaPanel@@IAAHH@Z` | fn_8255B378 (pre-existing) | **100.0** (45/45) |
| `?Exiting@MetaPanel@@UBA_NXZ` | fn_8255A980 | **100.0** (30/30) |
| `?Enter@MetaPanel@@UAAXXZ` | fn_8255A8F0 | **100.0** (19/19) |
| `?SyncGameTimer@MetaPanel@@QAAXXZ` | fn_8255A9F8 | **100.0** (17/17) |
| `?Exit@MetaPanel@@UAAXXZ` | fn_8255A940 | 81.2% (correct ID; retail has an extra Xbox-system call — `bl fn_82A3E9C0` via thunk fn_8250916C+4, XamGetSystemVersion path — that the rb3-Wii DEV source lacks; left mapped for documentation, not counted) |

Planner's candidate `Enter=fn_8255AA48` was WRONG (that fn builds a stack object and calls
fn_823D2F18 — not Enter); removed. Real Enter is fn_8255A8F0. `Exit=fn_8255A940` and
`Exiting=fn_8255A980` confirmed correct. Remaining in-span fns are inline-helper-class
factory/ctor/dtor + message boilerplate (ICF-fold, not clean matches); the other MetaPanel
methods (Poll/Load/UpdateMusicMuteState/OnMsg/…) are NOT in this span (different clusters).

**MetaPanel net: +5 strict** (Unload 99.9→100, PickLoopIndex new→100, Exiting/Enter/SyncGameTimer new→100).

### Phase C — AppLabel: NO LEVER, oracle-walled

`oracle_quality --tu AppLabel.cpp` GOOD rows (11 Set* methods) all sit at
**VA-misattributed** addresses (0x822a..0x82b0) OUTSIDE the pinned span (0x825ACF88–0x825AF938);
`gen_game_target_map --tu AppLabel.cpp` yields 0. AppLabel already has **12 correct in-span
mappings** (incl. SetSectionName at the real 0x825ADE28) and 52/100 perfect — so the
AppLabel.h layout is **already correct; there is no off:+N axis-A lever to apply**. The
remaining 48@0% are pure manual per-function identification (no force-multiplier, oracle
misattributed). Recorded as ID-grind, not a layout lever — deferred. No AppLabel edits made.

### Phase D notes
Per stream WORKER RULES (no whole-binary build / no report.json regen / reviewer commits),
the composed whole-binary A/B is left to the reviewer. Regression reasoning: the only
header change is deleting the phantom `unk44`, which makes the layout MATCH retail — it
cannot regress a previously-100 cross-TU function (those never matched with the wrong
layout). MetaPanel is heap-only (factory NEW_OBJ), not embedded elsewhere, so the 4-byte
size change is contained to MetaPanel.cpp's own (unpinned) ctor/NewObject.

Files touched: `src/band3/meta_band/MetaPanel.h`, `src/band3/meta_band/MetaPanel.cpp`,
`scripts/target_symbol_map.json` (+Exiting/Enter/SyncGameTimer/Exit), `config/45410914/splits.txt`
(pre-existing planner extension, unchanged by worker).

## Reviewer verdicts + composed A/B (Fable, 2026-07-02)

### Packet metapanel-axisa — VERIFIED, LAND

All six objdiff readings reproduced independently (MCP run_objdiff,
project_dir=worktree):

| symbol | reviewer reading | stub-fold guard |
|---|---|---|
| `?Unload@MetaPanel@@UAAXXZ` | 100.0 norm, 24/24 eq | 96 B, named — clean |
| `?PickLoopIndex@MetaPanel@@IAAHH@Z` | 100.0 norm+raw, 45/45 eq | 180 B, named — clean |
| `?Exiting@MetaPanel@@UBA_NXZ` | 100.0 norm, 30/30 eq | 120 B, named — clean |
| `?Enter@MetaPanel@@UAAXXZ` | 100.0 norm, 19/19 eq | 76 B, named — clean |
| `?SyncGameTimer@MetaPanel@@QAAXXZ` | 100.0 norm, 17/17 eq | 68 B, named — clean |
| `?Exit@MetaPanel@@UAAXXZ` | 81.2 norm (3 delete: lis/addi/bl fn_8250916C) | correctly NOT counted |

Worker claims accurate; no repair needed. Exit's residual is the retail-only
extra call (thunk fn_8250916C, lbl_82C926B8 string arg) absent from the rb3-Wii
DEV oracle — documented, left mapped at 81.2%.

### Composed whole-binary A/B

Full build clean (`~/tmp/rb3_build_exec-r2-metapanel-ab.log`, known warnings only).

- Baseline (pre-edit snapshot): **10,936** matched / 65,607 (8.4213% code)
- Composed: **10,944** matched / 65,607 (8.4271% code)
- **Delta: +8 matched, 0 regressions** (only `default/MetaPanel` moved, 0→8)

The +8 = the 5 named strict wins above + 3 anonymous span-extension folds
(fn_8255AA90 @40 B, fn_8255AECC @32 B, fn_8255B24C @32 B — all ≤44 B, no
symbol-name pairing → per stub-fold guard NOT claimed as strict).
**Conservative net strict: +5** (bar was ≥ +3). Success bar met; kill bars not
triggered (MetaPanel ≥ 2, AppLabel correctly skipped as oracle-walled ID-grind,
no layout lever).

Hygiene: removed scratch `global_fuzzy_pairs.json`; restored trailing newline
in `scripts/target_symbol_map.json`; committed files limited to the 4
match-relevant edits + docs.
