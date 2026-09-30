# W16-GV: FCNP non-member tail — pairs #14, #16-44

**Lane:** W16-GV (first-pass implementer), worktree `/home/free/tmp/wt-w16-gv`, branch `w16-gv`
**Base:** main `460d70d85` · **HEAD:** `33b392cdb` (9 commits: 8 group changes + 1 predictions doc)
**Date:** 2026-09-30

## Scope

Prior lane W16-GQ adjudicated the top 15 of 44 "FCNP non-member" pairs from
`/home/free/tmp/w16gq_nonfcnp_full_table.json`, installing 10 for +17 fns /
+6,220 B. This lane covers the remainder: **pair #14** (deferred by W16-GQ
pending W16-GP's concurrent relabelling, now resolved) plus **pairs #16-44**
(the scout's price-ranked tail). Pairs #1, #3, #11, #12 stay declined from the
prior wave and are untouched here. Lane W16-GR's group 10
(`push_back<ChatReceiver*>` @ `0x82b5f808`) is untouched here.

## Per-pair table (all 30 rows)

Admission gate: verdict PROVEN, `slot_refuted==0`, `bytes_differ==0`,
`cycle_assumed==0`, named-relocation count > 0, no placeholder relocations
tolerated, "ours" not map-resident at its own body, no co-withdrawal
contamination.

### Admitted (18)

| row | ours (short) | retail (short) | bytes | named relocs | group address | group action |
|----:|---|---|---:|---:|---|---|
| 14 | `insert<list<BoneState,BandCharacter>>` | `insert<list<MeshInstance>>` | 364 | 10 | `0x827e16a0` | new group `insert_list_MeshInstance` |
| 17 | `~vector<ObjPtr<Sequence...>>` | `~vector<String,...>` | 272 | 2 | `0x823d6fa8` | extended (existing) |
| 18 | `~vector<ObjPtr<RndGroup...>>` | `~vector<String,...>` | 260 | 2 | `0x823d6fa8` | extended (existing) |
| 19 | `~vector<ObjOwnerPtr<Waypoint...>>` | `~vector<String,...>` | 248 | 2 | `0x823d6fa8` | extended (existing) |
| 20 | `insert<list<OrganizedFileMerger...>>` | `insert<list<WeightContext...>>` | 208 | 10 | `0x823d6fa8` | extended (existing) |
| 21 | `~vector<ObjPtr<RndTex...>>` | `~vector<String,...>` | 208 | 2 | `0x822d8cc0` | extended (existing) |
| 22 | `~vector<ObjOwnerPtr<RndDrawable...>>` | `~vector<String,...>` | 200 | 2 | `0x822d8cc0` | extended (existing) |
| 24 | `__destroy_range<pair<vector<H>,StlNodeAlloc<H>>...>` | `__destroy_range<DistEntry...>` | 176 | 6 | `0x82634690` | new group `destroy_range_DistEntry` |
| 26 | `insert<list<pair<RndMultiMesh...>>>` | `insert<list<WeightContext...>>` | 156 | 10 | `0x823d6fa8` | extended (existing) |
| 27 | `insert<list<pair<Symbol...>>>` | `insert<list<WeightContext...>>` | 156 | 10 | `0x823d6fa8` | extended (existing) |
| 28 | `~vector<ObjPtr<SeqInst...>>` | `~vector<String,...>` | 144 | 2 | `0x822d8cc0` | extended (existing) |
| 29 | `list<Job*>::ctor` | `list<Dep,CharPollableSorter>::ctor` | 140 | 12 | `0x82370c80` | new group `list_Dep_CharPollableSorter_ctor` |
| 30 | `insert<list<Collision,RndDrawable...>>` | `insert<list<DecompressTask...>>` | 136 | 10 | `0x824071e0` | new group `insert_list_DecompressTask` |
| 32 | `~vector<ObjPtr<Object...>>` | `~vector<String,...>` | 128 | 2 | `0x822d8cc0` | extended (existing) |
| 34 | `insert<list<CallbackFile,ContentMgr...>>` | `insert<list<DataNode...>>` | 120 | 10 | `0x8251f060` | new group `insert_list_DataNode` |
| 36 | `_Param_Construct<Gem...>` | `_Param_Construct<Char3D,Ch...>` | 96 | 18 | `0x82b9b590` | extended (existing) |
| 37 | `insert<list<ScreenParams,S...>>` | `insert<list<WeightContext...>>` | 96 | 10 | `0x823d6fa8` | extended (existing) |
| 38 | `~vector<ObjOwnerPtr<Ch...>>` | `~vector<String,...>` | 88 | 2 | `0x822d8cc0` | extended (existing) |

All 18: `slot_refuted=0`, `bytes_differ=0`, `cycle_assumed=0`, `placeholder=0`,
`ours_map_addrs=[]` (ours not map-resident at its own address), retail not
already co-withdrawn from the target group. Group changes: **3 extensions**
(`0x823d6fa8` +8 members across rows 17/18/19/20/26/27/28... — actually split:
`0x823d6fa8` took 20/26/27/37 this lane, `0x822d8cc0` took 17/18/19/21/22/28/32/38,
`0x82b9b590` took 36) + **5 new groups** (rows 14/24/29/30/34).

### Declined (12)

| row | ours (short) | retail (short) | verdict | disqualifying reason |
|----:|---|---|---|---|
| 16 | `_M_fill_insert<vector<RGTr...>>` | `_M_fill_insert<vector<Note...>>` | PROVEN | **CYCLE-ASSUMED=1** — chase only closes by assuming the very cycle it's meant to prove; same failure mode as declined #1/#11 |
| 23 | `_M_fill_insert<vector<Sect...>>` | `_M_fill_insert<vector<Note...>>` | PROVEN | **CYCLE-ASSUMED=1**, same pattern as #16 |
| 25 | `~ObjRefConcrete<RndText>` | `~ObjRef` | REFUTED | 0 named relocations — no positive evidence, flat refutation |
| 31 | `~vector<Node,ObjPtrVec>` | `~Object@Hmx` | REFUTED | `bytes_differ=1` — bodies are not byte-identical once relocations are normalized |
| 33 | `_M_fill_insert<vector<Phra...>>` | `_M_fill_insert<vector<Note...>>` | PROVEN | **CYCLE-ASSUMED=1**, same pattern as #16/#23 |
| 35 | `insert<list<Voice*,...>>` | `insert<list<Instance,RndMu...>>` | REFUTED | **double-disqualified**: `slot_refuted=1` AND `bytes_differ=1` |
| 39 | `~ObjRefConcrete<UILabel>` | `~ObjRef` | REFUTED | 0 named relocations |
| 40 | `~ObjRefConcrete<CharServo...>` | `~ObjRef` | REFUTED | 0 named relocations |
| 41 | `_Rb_tree<MoveParent*,...>::ctor` | `hashtable<pair<Sy...>>::ctor` | REFUTED | `bytes_differ=1` |
| 42 | `~ObjRefConcrete<SongSection>` | `~ObjRef` | REFUTED | 0 named relocations |
| 43 | `~ObjRefConcrete<RndDrawable>` | `~ObjRef` | REFUTED | 0 named relocations |
| 44 | `~ObjRefConcrete<RndLight>` | `~ObjRef` | REFUTED | **double-disqualified**: chase REFUTED **and** "ours" is map-resident at its own address `0x824710c0` (has its own body — not a true fold candidate) |

**Pattern note on 16/23/33**: all three are `_M_fill_insert<vector<T>>` STL
instantiations chasing the same retail `_M_fill_insert<vector<Note...>>`
survivor that declined pair #1 (from W16-GQ's wave) also chased. Each fails
independently on its own merits (own `cycle_assumed=1`), not merely by
association with #1 — but the repeated shape across four independent rows
(#1, #16, #23, #33) suggests this particular retail body's call graph has a
structural cycle that the chase tool cannot resolve, not that these four rows
are individually unlucky.

**Pattern note on the `~ObjRefConcrete<T>` family (25/39/40/42/43)**: five
rows all chase the same trivial `~ObjRef` retail destructor and all get
`named=0` — the body is apparently too small/generic to carry any relocation
evidence one way or the other. Flat REFUTED, no positive counter-evidence.

## Step 3: install

All 18 admitted pairs installed across exactly 8 group changes (3 extensions
+ 5 new groups), one commit each, using the exact required JSON-write
incantation (`json.dumps(data, indent=1, ensure_ascii=True)`, no trailing
newline). Commits (worktree `w16-gv` branch, base `460d70d85`):

```
9a20d62cd  new group 0x827e16a0 (row 14)
acbc00f65  new group 0x82634690 (row 24)
0f6396186  new group 0x82370c80 (row 29)
86c4c14c4  new group 0x824071e0 (row 30)
8ca8646e6  new group 0x8251f060 (row 34)
```
(3 extension commits for rows 17/18/19/20/21/22/26/27/28/32/36/37/38 preceded
these in the same session — all 8 total, each verified via isolated
`git diff --stat` before committing.)

Post-install sanity: `scripts/symbol_aliases.json` parses (1668 groups, up
from 1663 at base — exactly +5 for the 5 new groups). Cumulative diff from
base: `1 file changed, 64 insertions(+), 6 deletions(-)`.

**No pre-existing membership was lost** — verified by loading
`scripts/symbol_aliases.json` at both `460d70d85` and `HEAD` and diffing
every base group's `{survivor} ∪ {folded}` set against its HEAD counterpart
by address: 0 missing members, 0 changed survivors, 0 deleted groups, across
all 1663 base groups. 5 new groups added, matching the 5 new-group commits
exactly.

## Step 4: predictions (written before measuring)

`docs/decomp/W16GV_PREDICTIONS_2026-09-30.md` (commit `33b392cdb`), predicted:

- `Δmatched_functions = 0` — reasoned from the whole-binary
  ALIASAUDIT-1/ALIAS-2/PATCH-LIVE ablation studies (CLAUDE.md), which found
  the alias-forgiveness mechanism moves `matched_code` bytes but leaves
  `matched_functions` exactly flat.
- `Δmatched_code ≈ +3,088 to +3,196 B` (18-row scout-table ceiling, with a
  hedge for the 108 B `full_bytes_masked` flag on rows 24/32 possibly being
  double-counted via a separate masked-equal forgiveness path).
- `Δmasked_equal_functions = 0`.

## Step 5: measurement

**Method note**: the task's step-5 wording assumes `--from-dirty`, but by the
time Step 5 began all 18 pairs were already committed (per the "one commit
per group change" requirement), leaving no dirty state to measure. Adapted to
`--patch` with explicit forward/reverse diffs (`git diff 460d70d85..HEAD` and
its inverse) plus a detached-HEAD checkout to align the worktree to the
pre-patch state for the forward leg. This achieves the same forward+reverse
A/B intent; both diffs and full stdout logs are preserved at
`~/tmp/w16gv_fcnp_tail{,_reverse}.patch` and `~/tmp/w16gv_ab_{forward,reverse}.log`.

### Forward (base `460d70d85` → all 8 group changes applied)

```
Δmatched         = +20
Δmasked_equal    = +0
Δhonest          = +20
Δcode%           = +0.031189 pp
Δcode_bytes      = +3,196
Δfuzzy           = +0.000047 pp
```

17 units improved: BandCharacter +2, OutfitConfig +2, Sequence +2, and 14
more units +1 each (BandCrowdMeter, CameraShot, CharBonesMeshes, CharEyes,
ContentMgr, DirUnloader, Draw, FileMergerOrganizer, GemManager, JobMgr,
SetlistMergePanel, Splash, + 2 more). Units at 100% (mpn ruler): 201→202
(Δ+1 — JobMgr reached 100%, `MATCHED_ROSE`, matched 9→10 of 10 rows). Units
at 100% (all-rows-fuzzy ruler): 177→177 (Δ+0).

Control: `none` ruler flat (+0 B) while default (`name_check`) ruler moved up
(+3,196 B) — the tool's `control_none_shape()` correctly flags this as
`ALIAS_SUSPECT` shape, which is the expected/documented signature for **any**
map-only patch that adds real aliases. This is not a new red flag here: the
prior adjudication (chase + retail-byte evidence, Step 2) is exactly what
distinguishes a proven fold from a fabricated one, and that evidence was
gathered before installation, not inferred from this shape.

Split fixed point: **0 extra forced re-splits** — the first split was already
its own fixed point.

### Reverse (branch tip → base, undoing all 8 group changes)

Exact sign-mirror of forward:

```
Δmatched         = -20
Δmasked_equal    = +0
Δhonest          = -20
Δcode%           = -0.031189 pp
Δcode_bytes      = -3,196
Δfuzzy           = -0.000047 pp
```

JobMgr fell off 100% (10→9 matched rows). Control: `FLAT` (not
`ALIAS_SUSPECT` — the shape-detector only fires on an "up" move on the
default ruler, which is correct: removing real aliases should not trip a
fabrication warning).

Split fixed point: **0 extra forced re-splits**, both directions.

### Validator before/after

```
python3 tools/icf_alias_finder.py --validate
```

Both pre-measurement and post-measurement runs (worktree restored to branch
tip, HEAD=`33b392cdb`) returned **identical** results:

```
COVERAGE: 1668 groups classified (1668/1668 reached, 7204 member spellings looked up)
  OK (MAP-CONSISTENT)              1417
  TOLERATED PLACEHOLDER_SURVIVOR     34
  TOLERATED STALE_SPELLING           89
  TOLERATED SURVIVOR_MISLABELED      27
  TOLERATED UNWITNESSED             100
  CONTRADICTION_EXEMPT                1
  CONTRADICTED (FATAL)                0
VALIDATE: PASS -- 1417 map-consistent, 250 tolerated, 0 contradicted, 1668 total
```

Byte-for-byte identical group counts and tier breakdown before and after both
A/B round-trips (forward via detached-HEAD checkout + patch apply, reverse
via patch-on-branch-tip) — confirms the measurement process introduced no
corruption and lost no membership.

## Native build gate

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

Full pass, 18/18, rc=0, 0 skipped/partial/failed.

## Reviewer questions / open items

1. **⚠️ Prediction failure — `Δmatched=+20` measured vs. `Δ=0` predicted.**
   My Step-4 prediction reasoned (from the whole-binary ALIASAUDIT-1/ALIAS-2/
   PATCH-LIVE ablations cited in CLAUDE.md) that alias forgiveness moves
   `matched_code` bytes but leaves `matched_functions` exactly flat. That did
   **not** hold here: this 18-pair installation moved `matched_functions` by
   exactly +20 (one per admitted pair minus two — 18 pairs, 22 call sites per
   the scout table, +20 measured; the site/function-count reconciliation
   itself may be worth a second look). **Working hypothesis, not verified**:
   the `vetted_reloc_name_diff` mechanism (objdiff-core, `name_check`-only)
   only auto-excludes *specific, screened* relocation-name diffs from
   `arg_diff_score` while still registering something at `diff_score`/other
   levels, whereas an explicit `symbol_aliases.json`
   `SymbolEquivalences` entry may make objdiff treat the two names as fully
   equal with **no diff registered at all** — so a row whose only defect was
   the now-aliased name-mismatch could flip entirely to `mpn == 100`. The
   large whole-binary ablations (over ALL ~1,528 groups at once) may not have
   isolated this effect, or this specific population (previously-withdrawn
   FCNP spellings, likely already close to matching except for the callee
   name) behaves differently from the general aliased population. This is an
   open question for the reviewer, not a settled explanation — I have not
   independently traced objdiff-core's diff scoring to confirm it.
2. **Realized bytes matched the scout-table ceiling exactly** (+3,196 B, not
   the discounted ~3,088 B I had hedged toward over the 108 B
   `full_bytes_masked` flag on rows 24/32) — the hedge was unnecessary; full
   price was collected. Worth noting as a resolved uncertainty, not a defect.
3. **Rows 16/23/33 (shared `_M_fill_insert<vector<Note...>>` target,
   cycle-assumed)**: each fails independently on its own chase, but the
   repeated shape (now 4 rows total including declined #1 from the prior
   wave) suggests the underlying retail body's call graph has a structural
   cycle the chase tool cannot close. Possibly worth a dedicated tool
   improvement rather than re-litigating each row by hand in a future wave.
4. **Row 44 double-disqualification**: chase REFUTED **and** "ours" is
   already map-resident at its own address (`0x824710c0`) — i.e. it has its
   own real body and was never a true fold candidate in the first place. This
   suggests the FCNP-candidate generator (upstream of this lane) could filter
   out map-resident spellings before scoring them, saving a chase cycle on
   future waves.

## Summary

Adjudicated the remaining 30 FCNP non-member candidate pairs (#14, #16-44).
18 admitted (all PROVEN with zero slot-refuted/bytes-differ/cycle-assumed
counts, non-zero named relocations, no map residency); 12 declined (3 on
cycle-assumption, 5 flat-refuted `~ObjRefConcrete`/`ObjRef` rows with zero
positive evidence, 2 on bytes-differ alone, 1 double-disqualified on
slot-refuted+bytes-differ, 1 double-disqualified on refuted+map-residency).
Installed the 18 admitted pairs as 8 group changes (3 extensions, 5 new
groups), one commit each. Measured forward and reverse: **+20 matched
functions / +3,196 matched bytes / +0 masked_equal / +0.031189 pp code%**,
exact sign-mirror on reversal, 0 extra forced re-splits either direction.
Validator PASS/0-contradicted before and after, with an explicit membership
diff confirming zero pre-existing groups were altered or lost. Native build
gate: **PASS 18/18, rc=0**. One open surprise flagged for review: measured
`Δmatched=+20` contradicts my own Δ=0 prediction drawn from the whole-binary
ablation literature — documented as an unresolved hypothesis, not
reconciled away.
