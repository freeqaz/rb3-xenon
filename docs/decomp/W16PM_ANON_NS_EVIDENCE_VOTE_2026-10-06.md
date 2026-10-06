# W16-PM: the anon_ns patcher votes on evidence only, and the `0x82449930` pin is Part's (2026-10-06)

Lane W16-PM, branch `w16-pm`, based on main `7e5a99800`.
Brief: two items W16-PI left (`W16PI_LAYOUT_AND_MAP_DEFECTS_FROM_W16PC_2026-10-06.md` §6 and §9).
1. `obj_anon_ns_patcher.plan_object` picks each object's fallback hash by a majority vote that counts token-rule
   guesses. The module docstring calls those guesses non-evidence. Make the vote use evidence only.
2. Settle the Accomplishment.cpp pin at `0x82449930–0x82449998`, which sits inside Part.cpp's span, on retail
   evidence.

Working files (not committed): `~/tmp/w16pm/`.
- `vote_census.py`: per object, the old vote against the evidence-only vote.
- `rowcmp.py`: compares two `report.json` files row by row.
- `ab_*.log`: the three A/B runs.

## 1. Result

| item | change | Δfns | ΔB | rows up / down |
|---|---|---:|---:|---|
| 1 | rule 7 votes on rules 1–4 only | 0 | 0 | 0 / 0 |
| 1, control | same change, on W16-PI's "rename alone" map | +3 | +1,192 | 6 / 0 |
| 2 | pin, map name and alias survivor move to Part's `vector<Burst>` | 0 | 0 | 0 / 0 (one row re-homed at 100) |
| | **branch vs main, whole binary** | **0** | **0** | **0 / 0** |

The patcher change is worth nothing on today's tree. That is expected: W16-PI already fixed the map that triggered
the flip. Its value is that the next map edit cannot trigger the flip again, and the control in §2.3 shows the size
of that risk: the old vote cost 1,192 B in one realistic state.

## 2. The anon_ns patcher

### 2.1 The defect

`plan_object` resolves each anonymous-namespace hash occurrence by seven rules:
- Rules 1–4 match the name's template against retail. These are evidence.
- Rules 5–6 (`token`, `token_global`) match only the identifier before `?A0x`. These are fallbacks.
- Rule 7 (`majority`) gives every occurrence nothing else resolved the object's majority hash.

The vote was `Counter(edits.values())`, so rules 5–6 voted. One map edit can therefore move a token's resolution,
every occurrence of that token follows it, and the votes it adds can flip the fallback for every remaining
occurrence in the object. W16-PI §6 saw this on WaveFile.obj.

### 2.2 The change (commit `018a8d47f`)

- The vote counts only edits made by rules 1–4 (`evidence_votes`).
- If rules 1–4 resolved nothing in the object, the fallback is retail's dominant hash in the paired object. The old
  code used that only when *no* rule resolved anything. This case gets its own stat key, `majority_retail_weight`.
- A token edit still respells its own symbol. It just no longer votes.
- The docstring now states the rule and says why.

`scripts/test_obj_anon_ns_patcher.py` has 4 synthetic cases. With the pre-change patcher swapped in, the 2 vote
cases fail and the 2 invariant cases (a token edit still applies; the result is independent of our current
spelling) pass. With the new patcher all 4 pass.

### 2.3 Measurements

**Census first, before any build.** On the current tree, 36 objects use the fallback. For every one of them the
evidence-only vote picks the same hash as the old vote. ChunkStream is a 1–1 tie with no token votes, so the tie
breaks the same way under both. The prediction was therefore Δ0, with 0 rows moving.

**A/B 1: patcher change on main** (`ab_measure --pick 018a8d47f --allow-inert`). The patch has no build kind, so
`--allow-inert` is needed and the tool makes no application assertion. Application was checked by hand: leg B's log
shows the anon_ns edge re-ran and printed `majority_retail_weight=1`, a key only the new code emits. It rewrote 0
files.

```
leg A: matched=53484 masked=25192 honest=28292 code%=57.825882
leg B: matched=53484 masked=25192 honest=28292 code%=57.825882
Δ0 on every key; rowcmp: 68,909 rows each side, 0 changed
```

Run dir: `~/tmp/wt-w16-pm-ab/.ab_measure_runs/20261006-085748-w16-pm-vote-3884376/`.

**A/B 2: the control.** A Δ0 on today's tree cannot tell a fix from a no-op, so the change was also measured where
the old vote is known to misfire. W16-PI's "rename alone" state was rebuilt as a scratch commit (commit `8ddd361f5` on
branch `w16-pm-control`, not for landing):
- `target_symbol_map.json` and `symbol_aliases.json` as they were at `336548dcb^`;
- only `0x827d33f8` renamed to `__uninitialized_copy<WaveFileMarker>`;
- the one drifted survivor relabelled by `alias_survivor_relabel.py --write`, because the drift gate otherwise stops
  the build.

The alias file is identical in both legs, so the patcher is the only difference between them.

Census on that tree: WaveFile.obj's vote is
- old: `9335ac2a`, driven by 35 `Label` token votes;
- new: `81ddebd1`, from 7 against 4 and 2 evidence votes.

The fallback covers 270 occurrences. **Prediction:** a partial recovery of W16-PI's loss, because the 35 `Label`
token edits still go to `9335ac2a` in both legs.

```
leg A: matched=53480 code%=57.813310
leg B: matched=53483 code%=57.824944
Δmatched=+3  Δcode_bytes=+1192  none-ruler control +0 B
leg B patcher: 1 file, 270 replacements
```

**The prediction was wrong. The recovery is complete, not partial.** The six rows that came back are exactly the six
W16-PI listed as dropped by the rename. Their sizes sum to exactly 1,192 B:

| row | size | before → after |
|---|---:|---|
| `ReadMarkers` | 760 | 99.84 → 100 |
| `__adjust_heap<CuePoint>` | 204 | 99.90 → 100 |
| `__final_insertion_sort<CuePoint>` | 108 | 99.63 → 100 |
| `fn_827D3D68`, `fn_827D3D90`, `fn_827D3E58` | 40 each | 99.5 → 100 |

0 rows went down. The `Label` token edits that remain in both legs cost nothing. W16-PI's −1,096 B was a net figure
for its step and is not directly comparable.

`none` reading +0 B is the expected shape: the patcher changes only names, and names are free under `none`.

Run dir: `~/tmp/wt-w16-pm-ab/.ab_measure_runs/20261006-090318-w16-pm-control-3922515/`.

## 3. The `0x82449930` pin

### 3.1 Retail evidence

**The body.** It is 0x68 B and frameless: no `mflr` and no stack. It shifts 16-byte elements down by one with four
`lwz`/`stw` pairs, then decrements `_M_finish` by 0x10 and returns `pos`. That is
`vector<T>::_M_erase(T* pos, __false_type)` for a 16-byte T.

**No `.pdata` entry.** Retail `.pdata`, read big-endian from `band.exe`, goes `0x82449904 → 0x82449998`. That is
what a leaf function looks like, and it is why the pin is `.text`-only.

**Placement.** The body sits inside Part's contiguous `.text` run. The pins on both sides of it are Part's:
`0x82446148–0x82449930` and `0x82449998–…`. Accomplishment's code is at `0x822743F0–0x82277014` and from
`0x82593F8C`, far from here.

**Retail callers.** These were read from the `.fn` symbols, not from the address column, which is synthetic for
multi-block units. There are five call sites:

| unit | retail caller | element type |
|---|---|---|
| Part | `fn_8244E998` = `RndParticleSys::CheckBursts` | `Burst` |
| BandCrowdMeter | `fn_822BF980` = `PropSync<vector<Hmx::Color>>` | `Hmx::Color` |
| LightPreset | `fn_824B0430` = `RemoveSpotlightDrawer` (×2) | `SpotlightDrawerEntry` |
| LightPreset | `fn_824B2DB0` = `PropSync<vector<SpotlightDrawerEntry>>` | `SpotlightDrawerEntry` |

None of them is in Accomplishment.

**The type.** `RndParticleSys::Burst` is four floats, 16 B. Our Part.obj defines
`?_M_erase@?$vector@VBurst@RndParticleSys@@…@ABU__false_type@2@@Z`. Part.obj does not define the `Hmx::Color`
spelling, which six other objects do.

**The fold.** `icf_pair_adjudicate --survivor 0x82449930 --chase` proves both spellings, Burst and Color, CHASED T1.
The three-way fold was already in `symbol_aliases.json` (group 180). The survivor was the Color spelling, with Burst
and SpotlightDrawerEntry folded.

**Verdict.** This is an ICF survivor whose COMDAT Part.obj contributed. Under ICF the name shown at the address is
any one of the folded spellings, and the placement says which object's copy was kept. The pin was put in
Accomplishment because the map name it followed (Color) is defined by Accomplishment.obj. That pin came from lane U's
wrong-unit sweep, `36f6b4df6`. The address does not support it.

### 3.2 Change (commit `e7130cee3`)

- The `.text` block `0x82449930–0x82449998` moves from Accomplishment.cpp to Part.cpp. Accomplishment keeps its other
  blocks, so it does not become an empty unit.
- The map name at `0x82449930` becomes the Burst spelling. Nothing else in the map uses that spelling.
- `tools/alias_survivor_relabel.py --write` relabelled group 180 against fresh objects:
  - the survivor is now Burst;
  - Color was re-chased PROVEN and is now a folded member;
  - SpotlightDrawerEntry was re-chased PROVEN and kept.

**Prediction:** Δ0 fns and Δ0 B. The row goes from 100 in Accomplishment to 100 in Part, and every call site is
forgiven before and after. **Measured** on a full re-split build against the base report: exactly that. One row is
"ONLY A" in Accomplishment and one is "ONLY B" in Part, both at 100, 104 B. 0 rows went up or down.

Validators on the lane tree:
- `alias_survivor_drift`: rc 0.
- `map_name_injectivity`: OK. There are 35,204 applied rows and 35,203 distinct names, plus 1 enumerated exception.
- `icf_alias_finder --validate`: PASS, with 0 contradicted.
- `test_alias_survivor_drift` and the new test: 18 passed.

## 4. Whole-binary A/B of the branch

`ab_measure --patch <git diff 7e5a99800 w16-pm>`, patch kinds `map` and `splits`. Both legs were force re-split and
both were at the `symbols.txt` fixed point after 0 extra re-splits. Leg B's renamer patched 1,845 files.

```
leg A: matched=53484 masked=25192 honest=28292 code%=57.825882
leg B: matched=53484 masked=25192 honest=28292 code%=57.825882
Δ0 on every key; units at 100%: 548 -> 548 (mpn), 488 -> 488 (all-rows-fuzzy)
rowcmp: 0 up, 0 down; ONLY A Accomplishment _M_erase<Color> (100, 104 B), ONLY B Part _M_erase<Burst> (100, 104 B)
```

Run dir: `~/tmp/wt-w16-pm-ab/.ab_measure_runs/20261006-090844-w16-pm-branch-3962184/`.

## 5. Not done, and one thing flagged

- **The token rule itself was not changed.** It is a per-symbol fallback for names retail never had, and the brief's
  defect was the vote. Demoting it would change 275 assignments on today's tree and needs its own A/B.
- **The ChunkStream tie is left to `Counter` insertion order,** as before. Its evidence is 1–1 and its retail weight
  is also 1–1, so no retail fact breaks the tie.
- **Flagged, not investigated: Accomplishment.cpp has 4-byte `.text` blocks.** They are `0x8227459C–0x822745A0`,
  `0x8227500C–0x82275010` and `0x822750BC–0x822750C0`. A 4-byte function is unusual and these may be
  mis-carves. They are outside this lane's brief.
- Nothing in `src/network` or `src/xdk` was touched. No source changed.

## 6. Native checks

See the end of this section. Both checks run last, on the final commit.
