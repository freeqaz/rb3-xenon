# W16-ON — leftovers from W16-OK and W16-OL (2026-10-03)

**Branch** `w16-on`, rebased onto main `e5586e330` (W16-OM). **Ruler** `name_check` (graded, from `report.json`
`provenance.diff_config`). The permuter was not run. `src/network`, the Quazal block and
`tools/icf_pair_adjudicate.py` were not touched.

The five leads came from `W16OK_BAND3_SUB100_FINISHER_2026-10-03.md` §6 and `W16OL_SYSTEM_DIRS_SUB100_2026-10-03.md`
§4/§6. All five are done. Each is one commit:

| commit | lead | in-tree effect |
|---|---|---|
| `731b6885a` | `~BandHeadShaper` carve (`symbols.txt`) + drop the `??0Vector3` label | +0 fns / +32 B, total_functions −1 |
| `3fbfc9ee7` | `GemRepTemplate::CreateTail` calls `reserve` | +1 / +104 B |
| `2a14efc85` | `MemTruncate` / `MemRealloc` release ABI | +0 / +0 (Resize 79.78 → 98.00) |
| `b0e72ad44` | `InitialCrowdRating` re-home | +1 / +124 B |
| `3c8a8c18f` | Campaign's `map` islands: re-home, survivor swaps, two new folds | +2 / +564 B |

The carve commit sits first on the branch so that it can be the A/B's leg-A base (§1).

## 1. Whole-binary A/B

`ab_measure` refuses a patch that touches `symbols.txt`, so the measurement is split the way W16-NV did it.

### 1.1 main → carve alone

Measured in the A/B worktree (`scripts/setup_worktree.sh ~/tmp/wt-w16-on-ab`, branch `w16-on-ab-base` = `731b6885a`)
before `ab_measure` ran. Each state was built with a forced re-split (renamer stamp removed, `config.yml` touched),
built again to zero work, then read once more with `report.json` + `report.cache` wiped:

| | matched_functions | matched_code | total_functions | total_code | code% | fuzzy |
|---|---:|---:|---:|---:|---:|---:|
| main `e5586e330` | 53,216 | 5,873,272 | 68,914 | 10,247,792 | 57.31256 | 63.534855 |
| + carve `731b6885a` | 53,216 | 5,873,304 | 68,913 | 10,247,792 | 57.31287 | 63.534904 |

**Predicted before the build: +0 fns / +32 B, total_functions −1, total_code unchanged. Measured: identical.**
Row level: `??1BandHeadShaper@@QAA@XZ` 87.5 → 100 (now 36 B), and the 4-byte `??0Vector3@@QAA@XZ` row is gone. That
row read 100 before, but it was a phantom: one dead `blr` paired against an empty inline ctor. 0 rows down. The
re-split left `symbols.txt` unchanged in both states (a fixed point).

### 1.2 The rest of the branch

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-on-ab --patch ~/tmp/w16on/ab.patch --label w16on-branch`, where
the patch is `git diff 731b6885a w16-on` (map, splits, aliases and source, 6 files).

**Predicted before the run, from the in-tree row diffs:** +4 fns / +792 B, 0 rows down.

Run dir `~/tmp/wt-w16-on-ab/.ab_measure_runs/20261003-030912-w16on-branch-1968888/`, objdiff-cli `sha256:c1b7d952`
(stable across legs). Both legs were forced to re-split and read at a `symbols.txt` fixed point (0 extra re-splits).

```
leg A: matched=53216 masked=25132 honest=28084 code%=57.312870  (recompiles: 0, settled)
leg B: matched=53220 masked=25132 honest=28088 code%=57.320602  (recompiles: 1124, split=1, patch_steps=7, settle iterations: 2)
Δmatched=+4  Δmasked_equal=+0  Δhonest=+4  Δcode%=+0.007732pp  Δcode_bytes=+792
Δfuzzy=+0.007069pp   (legA 63.534904 -> legB 63.541973)
unit improvements: BandCrowdMeter +2, FileMergerOrganizer +1, GemTrackDir +1, GemRepTemplate +1
unit REGRESSIONS: CrowdMeterIcon (112->111)   [the funclet fn_822BB37C moved out with InitialCrowdRating, §2.3]
units at 100% [mpn]: 522 -> 523 (GemRepTemplate, MATCHED_ROSE); [all-rows-fuzzy]: 461 -> 462
[control none] Δmatched_code=+688 B -- NOT_APPLICABLE (kinds=map,source,splits)
```

**Measured: +4 fns / +4 honest / +792 B, identical to the prediction.** Leg A reproduces §1.1's carve state exactly
(53,216 / 57.312870 / fuzzy 63.534904).

**Whole branch against main** (§1.1 + §1.2): `matched_functions` 53,216 → 53,220 (+4), `matched_code` 5,873,272 →
5,874,096 (**+824 B**), `total_functions` 68,914 → 68,913 (−1 phantom row), `total_code` unchanged. The in-tree row
diff of the whole branch against main's build read the same +4 / +824 B.

**Row diff of the archived legs** (`~/tmp/w16na/rowdiff.py`; output `~/tmp/w16on/ab_rowdiff.txt`): 2 rows up, **0 rows
down, 0 off 100**. Six keys were renamed or re-homed, and every new key scores at least what its old key did:

| old key | new key | fuzzy |
|---|---|---|
| CrowdMeterIcon `fn_822BB300` | BandCrowdMeter `InitialCrowdRating` | 0 → 100 |
| CrowdMeterIcon `fn_822BB37C` | BandCrowdMeter `fn_822BB37C` | 100 → 100 |
| MemMgr `MemTruncate` (5-arg) | MemMgr `MemTruncate` (2-arg) | 87.73 → 87.73 |
| MemMgr `MemRealloc` (6-arg) | MemMgr `MemRealloc` (3-arg) | 100 → 100 |
| Campaign `insert_unique<Symbol,Symbol>` | GemTrackDir `insert_unique<unsigned,pair<int,RndMesh*>>` | 0 → 100 |
| Campaign `_M_create_node<Symbol,Symbol>` | FileMergerOrganizer `_M_create_node<Symbol,CatData>` | 0 → 100 |

## 2. What changed, and why

### 2.1 `GemRepTemplate::CreateTail` (band3, 104 B): 99.81 → 100

Retail calls `VertVector::reserve` (0x82418388, named by W16-OL) and ours called `resize`. W16-OL's naming of that
address exposed this as a wrong-callee charge. **This is a behaviour fix:** a new tail mesh now starts with
`GetRequiredVertCount(count)` vertices reserved, where ours started with that many live, uninitialised vertices.

### 2.2 `MemTruncate(mem, size)` and `MemRealloc(mem, size, align)` release ABI

Retail strips the debug strings from both calls. There are three retail witnesses:

- `CharClip::Transitions::Resize` loads only r3/r4 before `bl 0x827bc520` (MemTruncate) and r3/r4 with r5 = 0
  before `bl 0x827bd080` (MemRealloc).
- The 8-byte thunk at `0x82C30B58` is `li r5,0x0; b 0x827bd080`. That is the realloc entry the Ogg and JSON
  allocator tables share.
- Retail's MemRealloc body hands its third argument to `MemAlloc` as the alignment. Our 100% body had spelled that
  as `(MemAlloc)(size, (int)file)`; it is now `(MemAlloc)(size, align)`.

Implementation, the same pattern as the existing `MemAlloc` / `MemFree` release forms:

- `utl/MemMgr.h` keeps the debug signatures under `HX_NATIVE`.
- The match build gets `MemTruncate(void*, int)` and `MemRealloc(void*, int, int)`, plus macros that rewrite the
  inherited debug spellings. A MemRealloc call with any arity other than the 6-arg debug form fails to compile.
- The definitions parenthesise the name.
- Map: `0x827bc520` → `?MemTruncate@@YAPAXPAXH@Z`, `0x827bd080` → `?MemRealloc@@YAPAXPAXHH@Z`. The addresses are
  unchanged; the mangled names follow the new signatures.

Result (forced re-split): `Resize` 79.78 → **98.00**. `MemRealloc` stays 100 under its new key and `MemTruncate`
stays 87.73. Nothing else moved.

`Resize`'s last charge is one retail-only instruction, `stw r3,0x50(r31)` immediately before `bl MemRealloc`. It
stores `mNodeStart` to the first local slot and nothing reads it. A dead store survives `/O1` only through an
address-taken or EH-tracked object, and none of the source spellings examined has one, so none is invented. `MemTruncate`'s own 87.73 is a liveness difference (retail holds `gNumHeaps` in r29 across the
heap loop and reads it off a different base); the ABI does not touch it.

### 2.3 `InitialCrowdRating` re-home (124 B): 0 → 100

`0x822BB2F8..0x822BB39C` sat at the tail of CrowdMeterIcon's `.text` pin. The range holds the EH prefix,
`BandCrowdMeter::InitialCrowdRating` and its 0x20 unwind funclet at `0x822BB37C`, which clears the `static Symbol`
guard bit. CrowdMeterIcon's obj cannot define the function. The boundary now sits at `0x822BB2F8`, BandCrowdMeter's
block starts there, and `0x822bb300` is named `?InitialCrowdRating@BandCrowdMeter@@QBAMXZ`. dtk re-derived the two
`.pdata` lines on the first build (the split guard stopped once, as designed, and the retry was a fixed point). The
funclet stays 100 in its new unit.

### 2.4 Campaign's `map<Symbol,Symbol>` islands: 0 → 100 (472 B + 92 B)

**Pins and map names.** Both islands are retail's single surviving copy of a folded template body. Neither has a
`map<Symbol,Symbol>` owner in our objs. Each moved to the TU whose obj defines the spelling, with the map renamed
to that spelling:

| address | moved to | map name now |
|---|---|---|
| `0x822EA814..0x822EA9F0` (`insert_unique`, hint form) | GemTrackDir.cpp (the block sits between two GemTrackDir blocks) | `insert_unique<unsigned, pair<int,RndMesh*>>` |
| `0x823D9628..0x823D9684` (`_M_create_node`) | FileMergerOrganizer.cpp (inside its range) | `_M_create_node<Symbol, CatData>` |

**Survivor swaps.**
- **Group 337** (`0x822ea818`): the survivor is now the RndMesh spelling, and the CatData spelling is folded.
  `--chase` (after `--chasetest` passed): **FLAT T1 PROVEN, CHASED T1 PROVEN**, 0 cycle-assumed, 472 B / 472 B. The
  retail witness is FileMergerOrganizer's `map<Symbol,CatData>::operator[]`.
- **Group 1399** (`0x823d9628`): the survivor is now the CatData spelling, which was the group's only folded member,
  so `folded` is empty.

**The old `<Symbol,Symbol>` survivors were not kept as folded members.** That departs from W16-OL's recipe, on
purpose. No compiled obj defines or references either spelling, so `--chase` returns UNDECIDABLE ("our spelling is
in no compiled obj"). A membership that cannot be chased would break this lane's rule that every alias is PROVEN.
They forgave 0 B. Each group records them under `survivor_renamed` with the reason.

**Two new groups.** Both were needed before the GemTrackDir row could cross. Both chase **PROVEN with 0 cycle-assumed
leaves**, and retail 0x822EA818's two call sites witness them:

| ours | retail survivor | B | flat T1 | chased |
|---|---|---:|---|---|
| `_M_insert<unsigned,pair<int,RndMesh*>>` | `0x823d9970` `_M_insert<Symbol,CatData>` | 204 | REFUTED (the `_M_create_node` slot) | PROVEN, the slot discharged by the fold into `0x823D9628` |
| `insert_unique(const value&)<unsigned,pair<int,RndMesh*>>` | `0x823d9a40` `insert_unique(const value&)<Symbol,CatData>` | 224 | REFUTED (the `_M_insert`/`_M_create_node` slots) | PROVEN |

Measured in-tree: with only the swaps, the row read 99.92; with the `_M_insert` group, 99.96; with both groups, 100.
The report cache was wiped for each read, because the alias map is not in its key.

`tools/test_alias_group_key.py`, `test_icf_alias_join_guard.py`, `test_icf_alias_no_ourbuild_gate.py`,
`test_icf_alias_survivor_gate.py` and `test_icf_alias_withdrawal_guard.py` all pass, and the build's
CHECK ICF-ALIAS MAP step is OK.

### 2.5 `~BandHeadShaper` carve

See §1.1. With the carve at 0x24, the existing `~TrackerDesc` → `~BandHeadShaper` fold (W16-OK §3.2) now chases
**flat T1 PROVEN, 36 B / 36 B**. Before, it needed W16-OK's dead-`blr` argument. The re-check is recorded on its group.

## 3. Rows that went down

**None.** 0 rows down and 0 rows off 100 in the A/B on both rulers, and none in the carve measurement (§1.1). The
one `unit REGRESSIONS` line (CrowdMeterIcon 112 → 111) is a re-homed row leaving, not a score falling.

Two rows did not reach 100: `CharClip::Transitions::Resize` (79.78 → 98.00, one dead store, §2.2) and `MemTruncate`
(unchanged at 87.73).

## 4. Leads left

- **The realloc thunk `0x82C30B58`** (8 B, anonymous, pinned under `sharedbook`). After §2.2 our `OggRealloc` and
  `JsonRealloc` both compile to its exact bytes, so retail kept one of two ICF twins. Naming it needs a pin move
  (`JsonMemory.cpp` ends at `0x82C30B58`), a name, and a chased alias for the other spelling. Naming it also
  converts every forgiven vorbis call site into a checked one, so it is a bet. Not done.
- **`Resize`'s dead store** (§2.2) and **`MemTruncate`'s liveness residue**: left as recorded.

## 5. Native gate

Run last, on `22535358d` (the rebased branch tip; only this doc changed afterwards):

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```
Log: `~/tmp/w16on/native_gate.log`. The `MemMgr.h` change keeps the debug signatures under `HX_NATIVE`.
