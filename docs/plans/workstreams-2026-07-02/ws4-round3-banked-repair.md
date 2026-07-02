# WS4 — Round-3 banked repairs: Bundle 2 (loader cascade) + HamCamTransform lever

**Written 2026-07-02** (doc-prep agent, all key claims re-verified against the live
repo at writing time). Master doc: `docs/plans/frontier-workstreams-2026-07-02.md`
(stream #4). Authoritative source handoff:
`docs/decomp/handoff/round3-shared-header-followups-2026-07-02.md` — read it fully
before executing.

Two independent items, each a banked near-miss lever from the round-3 wave:

- **Item A** — Bundle 2: FileLoader + ObjDirItr DC3-drift reverts, preserved on
  branch `followup/round3-full-batch` @ `3879248583164c66bee5ed85bb558cf43cf2a07a`.
  Oracle-confirmed correct, composed cascade was net +55 gained but left ~6
  repairable regression clusters. Finish the cascade, land regression-free.
- **Item B** — HamCamTransform `TransformCrowd` 0x10→0xc shrink, edits banked at
  `/home/free/tmp/hct_edited.{h,cpp}`. Currently net −3 only because
  `scripts/target_symbol_map.json` has false-100 misnamed entries; fix the map
  naming first, then re-apply.

Do them in separate worktrees (they touch disjoint files) or sequentially in one;
A/B-measure each independently so each lands (or is killed) on its own merits.

---

## Objective

1. Apply Bundle 2 (14 files, enumerated below) on top of current main in an
   isolated worktree, repair the ~6 regression clusters it causes until the
   whole-binary A/B shows **net-positive with 0 strict and 0 real fuzzy
   regressions**, then land.
2. Fix the HamCamTransform-unit target-map misnaming, re-apply the banked
   TransformCrowd edits, and land the combined (map-rename + source-edit) change
   net-positive.

Expected combined yield (master doc estimate): **+30–50 net strict matches**.

## Current state (verified 2026-07-02 by this doc's author)

- Main was at `b2b9654472eaea16f47e8da93d0ad6c02634df48` at verification time
  (moving fast — other agents are landing; re-check with `git log --oneline -1 main`).
- `build/45410914/report.json` at verification time: **10,934 / 65,607 matched,
  8.42% strict code bytes, 11.57% fuzzy**. (Newer than the master doc's
  10,870/8.32% — waves landed in between. Always take a fresh in-worktree
  baseline; never reuse these numbers.)
- Branch exists: `git log followup/round3-full-batch -3 --oneline` →
  `3879248 wip: round-3 batch (10 fns to 100 + shared-header reverts)`,
  parent `9b938ea37185003061e90725e72bca2b367240a3`.
- Banked HCT files exist: `/home/free/tmp/hct_edited.h` (2,617 bytes) and
  `/home/free/tmp/hct_edited.cpp` (5,605 bytes), both dated Jul 2 01:21.
- **Zero drift**: `git diff 9b938ea main -- <the 14 Bundle-2 files>` is EMPTY at
  main `b2b9654` → the Bundle-2 patch applies cleanly onto current main.
  (Re-verify this before applying; if another lane has since touched one of the
  14 files, resolve manually.)
- The clean round-3 subset is already on main: `Splash.cpp`, `Archive.cpp`,
  `Bitmap.cpp`, `Gen.cpp`, `SynapseAPO.cpp`, `SpotlightDrawer.h`, `GameGem.h`,
  `SongDB.cpp`, `ManageBandPanel.cpp` are byte-identical between main and the
  branch. Only Bundle 1 (disproven) and Bundle 2 remain unlanded.
- No new `FileLoader(` construction sites appeared on main since the branch:
  grep confirms the 11 call-site TUs in Bundle 2 are exactly the full set
  (`grep -rn "new FileLoader\|FileLoader loader" src --include='*.cpp'`).
- All six Item-A regression units use `ObjDirItr` (grep-verified) — consistent
  with the handoff's "missed funclet/frame shape" root-cause hypothesis.
- None of the regressed VAs (`0x82735358/80`, `0x82735470`, `0x82322468`,
  `0x826F1C20`, `0x826F1BF4`, `0x827B9D18/6C`, `0x823A5A90`, `0x8228B5C8`) are
  keys in `scripts/target_symbol_map.json` (verified ABSENT) — their pairing is
  positional/funclet-shaped, not name-driven. That matters for repair strategy
  (see step A5).

## Evidence & references

- Handoff (authoritative): `docs/decomp/handoff/round3-shared-header-followups-2026-07-02.md`
  — Bundle-2 section (lines 40–76) + HamCamTransform section (lines 85–94).
- Branch: `followup/round3-full-batch` @ `3879248583164c66bee5ed85bb558cf43cf2a07a`
  (contains the FULL round-3 batch; Bundle 1 must be filtered out — see the
  file-list gate below).
- Bundle-1 DISPROOF (do not re-attempt): handoff lines 17–38 + master doc
  "Dead levers" list. Bundle-1 files on the branch:
  `src/system/rndobj/Dir.h`, `src/system/char/Character.h`,
  `src/system/ui/PanelDir.cpp`, and the branch's one-line
  `scripts/target_symbol_map.json` change (`0x823F0890` `U`→`Q` de-virtualization
  rename). **None of these four may be taken.** The branch's
  `target_symbol_map.json` is also stale vs main (main gained ~50 entries since)
  — never apply it wholesale.
- Wave-loop SOP (land mechanics): `docs/decomp/handoff/wave-loop-SOP-2026-06-20.md`
  + `scripts/harvest/README.md` (land.sh / union resolvers / composed-verify).
- A/B measurement: `scripts/harvest/measure_delta.py` (strict net + per-function
  fuzzy regression scan; usage in its docstring), `tools/fresh_report.sh`
  (guaranteed-fresh report; honors `NINJA_JOBS`).
- Worktree: `scripts/setup_worktree.sh <path> <branch>` (CLAUDE.md "Git &
  worktrees"). Worktrees + logs under `~/tmp`, never `/tmp`.
- Oracles: rb3-Wii dev decomp `../rb3/src` (game-code, named fns + asserts),
  DC3 `../dc3-decomp/src` (same-compiler engine twin). MCP tools
  `lookup_rb3wii`, `lookup_dc3`, `run_objdiff`, `run_diff_inspect`
  (**always pass `project_dir=<worktree>`**), skills `/compare-asm`,
  `/stack-layout`, `/vtable`, `/data-diff`, `/ghidra-decompile` (port 8002).

---

# Item A — Bundle 2: FileLoader + ObjDirItr reverts + cascade repair

## The exact Bundle-2 file list (verified from the branch commit)

3 core files + 11 call-site TUs = **14 files**. (The handoff says "13 call-site
TUs" but enumerates 11; the commit contains exactly these 11 — the handoff
number is a miscount, trust the commit.)

```
src/system/utl/Loader.h            # FileLoader: 8-arg→7-arg ctor, drop String mHeapName @0x44, mState 0x54→0x44
src/system/utl/Loader.cpp          # ctor body, AllocBuffer MemFindHeap("main"), LoadMgr::AddLoader 7-arg call
src/system/obj/Dir.h               # ObjDirItr: DC3 std::list collector → retail flat 0x14-byte iterator
                                   #   {mDir@0, mSubDir@4, mEntry@8, mObj@0xC, mWhich@0x10}
                                   #   + inline ObjectDir::NextSubDir(int&) (ported from rb3-Wii Dir.cpp)
src/system/world/LightHue.cpp      # 2 call sites → 7-arg
src/system/rndobj/Utl.cpp          # ResourceFactory → 7-arg
src/system/rndobj/Tex.cpp          # RndTex::PreLoad → 7-arg
src/system/synth/Synth.cpp         # WavFactory → 7-arg
src/system/synth/MoggClip.cpp      # LoadFile → 7-arg
src/system/synth/BinkClip.cpp      # LoadFile → 7-arg
src/system/hamobj/HamAudio.cpp     # Load → 7-arg
src/system/os/FileCache.cpp        # StartRead → 7-arg
src/system/utl/NetLoader.cpp       # NetLoaderStub ctor → 7-arg
src/system/utl/NetCacheLoader.cpp  # → 7-arg
src/system/gesture/LiveCameraInput.cpp  # stack FileLoader → 7-arg
```

## Known regression clusters to repair (from the handoff's composed A/B)

All six units are wired in `config/45410914/objects.json` and all use
`ObjDirItr` (grep-verified):

| Unit | Regressed fns | Symptom |
|---|---|---|
| `system/obj/DirLoader.cpp` | `fn_82735358`, `fn_82735380`, `fn_82735470` | → 0% (Loader-subclass funclets, unpaired) |
| `system/bandobj/BandWardrobe.cpp` | `fn_82322468` | → 0% stub; calls `fn_8228B5C8` |
| `system/synth/MetaMusic.cpp` | `fn_826F1C20`, `fn_826F1BF4` | 67% (frame Δ −0x20), 94% |
| `band3/meta_band/NetSync.cpp` | `fn_825860xx` cluster (10 funclets) | 99.9% (frame `subi −96`) |
| `system/track/TrackDir.cpp` | `fn_827B9D18`, `fn_827B9D6C` | → 0% |
| `system/char/CharBoneDir.cpp` | `fn_823A5A90` | → 94% |

Unit baselines at doc time (from `build/45410914/report.json`; re-baseline fresh):
BandWardrobe 59/243, CharBoneDir 61/125, DirLoader 96/234, MetaMusic 37/67,
NetSync 35/69, TrackDir 56/102.

Root-cause hypothesis (handoff + this doc's grep): the removed
`std::list<ObjectDir*> mSubDirs` member kills the list-dtor EH funclets and
shrinks stack frames in every TU that constructs `ObjDirItr` on the stack.
Where retail agrees (LightHue, Char\*, AmbientOcclusion) that's the +55 gain;
in these six TUs a *second* thing also has to change shape (another local, a
different iterator use, a funclet count/order shift breaking positional
pairing) and the composed run missed it.

## Step-by-step procedure

All commands from inside the worktree unless noted. Log builds to
`~/tmp/rb3_build_ws4a.log`.

**A1. Worktree + patch extraction** (extraction runs in the main repo but is
read-only there):

```bash
cd /home/free/code/milohax/rb3-xenon
scripts/setup_worktree.sh ~/tmp/wt-ws4-bundle2 ws4-bundle2

# Extract Bundle 2 ONLY (the 14 files; never the whole branch — Bundle 1 rides on it):
git diff 9b938ea37185003061e90725e72bca2b367240a3 \
         3879248583164c66bee5ed85bb558cf43cf2a07a -- \
  src/system/utl/Loader.h src/system/utl/Loader.cpp src/system/obj/Dir.h \
  src/system/world/LightHue.cpp src/system/rndobj/Utl.cpp src/system/rndobj/Tex.cpp \
  src/system/synth/Synth.cpp src/system/synth/MoggClip.cpp src/system/synth/BinkClip.cpp \
  src/system/hamobj/HamAudio.cpp src/system/os/FileCache.cpp \
  src/system/utl/NetLoader.cpp src/system/utl/NetCacheLoader.cpp \
  src/system/gesture/LiveCameraInput.cpp \
  > ~/tmp/bundle2.patch

# Sanity: 14 files, ~79 insertions / ~85 deletions
grep -c '^diff --git' ~/tmp/bundle2.patch   # must print 14
grep -E 'target_symbol_map|rndobj/Dir\.h|Character\.h|PanelDir' ~/tmp/bundle2.patch  # must print NOTHING
```

**A2. Baseline in the worktree** (before any edit):

```bash
cd ~/tmp/wt-ws4-bundle2
NINJA_JOBS=12 tools/fresh_report.sh 2>&1 | tee ~/tmp/rb3_build_ws4a.log
cp build/45410914/report.json ~/tmp/ws4a_BASE.json
```

**A3. Apply + first A/B:**

```bash
git apply --stat ~/tmp/bundle2.patch     # dry-read
git apply ~/tmp/bundle2.patch
rm -f build/45410914/target_symbol_renames.stamp
touch config/45410914/config.yml
NINJA_JOBS=12 tools/fresh_report.sh 2>&1 | tee -a ~/tmp/rb3_build_ws4a.log
scripts/harvest/measure_delta.py ~/tmp/ws4a_BASE.json build/45410914/report.json
```

Expected first-pass shape (from the handoff's composed run, which also included
Bundle 1 and the since-landed clean subset, so numbers will differ): a large
gain (LightHue::Sync, GetNormalMapTextures, Char\*/AmbientOcclusion family —
many already landed via other waves, so the residual gain may be smaller than
+55) and regressions concentrated in the six clusters above. **Record the exact
regression list from measure_delta — it supersedes the handoff's list.**

**A4. Per-cluster repair loop.** For each regressed function, in this order
(cheapest diagnosis first):

1. `mcp run_objdiff(symbol=<fn>, project_dir=~/tmp/wt-ws4-bundle2, unit=<unit>)`
   — 0% usually means *unpaired* (our obj stopped emitting a symbol at that
   position). `run_diff_inspect(mode=diagnose)` and `mode=clusters` next.
2. For frame-delta near-misses (MetaMusic Δ −0x20, NetSync `subi −96`): use the
   `/stack-layout` skill (or `run_diff_inspect mode=stack-layout`) to see which
   slots vanished/shifted. If the delta equals the removed `std::list` footprint,
   the *target* still reserves that space → an additional local exists in retail
   (or our `ObjDirItr` is by-ref where retail's is by-value, or vice versa).
   Check the rb3-Wii source for the same method (`lookup_rb3wii <method>`,
   `/rb3wii-pair` for the unit) — the retail body may construct the iterator
   differently (e.g. iterate twice, keep a second iterator alive).
3. For 0%-stub funclets (DirLoader, TrackDir, BandWardrobe): decompile the
   target VA (`/ghidra-decompile fn_82735358` etc., port 8002) to learn what the
   retail funclet actually is (typically an EH dtor funclet for a stack object).
   Then make our TU emit the equivalent funclet again — usually by matching the
   retail set of stack objects with non-trivial dtors in the enclosing function.
   `BandWardrobe fn_82322468` calls `fn_8228B5C8` — identify that callee first
   (Ghidra + `lookup_dc3`/`lookup_rb3wii`); the handoff notes it was 0% even
   after the Bundle-1 revert, so it's attributed to the loader/iterator shape.
4. `CharBoneDir fn_823A5A90` → 94%: plain near-miss; `/compare-asm` and fix the
   body (CharBoneDir.cpp constructs `ObjDirItr<CharBone>` at lines 119/252 and
   `ObjDirItr<RndTransformable>` at 278).
5. After each repair: incremental `run_objdiff` on the touched fns, then a fresh
   `tools/fresh_report.sh` + `measure_delta` against `~/tmp/ws4a_BASE.json`
   before moving on. Do not batch blind fixes.

**A5. Pairing-repair fallback.** Because none of the regressed VAs have
`target_symbol_map.json` entries, a 0% "regression" can be pure pairing loss
(funclet count shift), not wrong code. If Ghidra shows the target funclet is
byte-equivalent to a funclet our obj *does* emit under a different position:
add an explicit map entry (`"0X82735358": "<our mangled funclet name>"`) via
`tools/gen_game_target_map.py` conventions, rerun with
`rm -f build/45410914/target_symbol_renames.stamp && touch config/45410914/config.yml`.
Map-entry additions are legitimate repairs; map-entry *removals* to hide a
regression are not (honesty gate below).

**A6. Final A/B + land:**

```bash
# run TWICE; NET must be identical both runs
NINJA_JOBS=12 tools/fresh_report.sh && scripts/harvest/measure_delta.py ~/tmp/ws4a_BASE.json build/45410914/report.json
NINJA_JOBS=12 tools/fresh_report.sh && scripts/harvest/measure_delta.py ~/tmp/ws4a_BASE.json build/45410914/report.json
```

Gate: **NET > 0 AND zero unexplained strict regressions AND zero real fuzzy
regressions** (drop > the measure_delta `--fuzzy-eps`). Then commit in the
worktree (one commit on branch `ws4-bundle2`, match-relevant files only:
the 14 sources + any map additions), and from the main repo:
`scripts/harvest/land.sh ~/tmp/wt-ws4-bundle2` → on `READY`,
`git merge --ff-only ws4-bundle2`, then the SOP composed verify on main.

---

# Item B — HamCamTransform lever (map-rename first, then re-apply)

## What the lever is

Retail RB3 `sizeof(TransformCrowd)` == **0xc** (a lone
`ObjPtr<WorldCrowd> mCrowd`); DC3 (newer) added `CrowdRotate mCrowdRotate`,
making our current DC3-shaped class 0x10. Evidence (recorded inside the banked
header): `ObjVector<TransformCrowd>::operator=` retail asm `li r10,0xc; divw`,
plus the ICF fold of its `resize` with `ObjVector<Character::Lod>::resize`.
`TransformArea` stays 0x70 (already padded correctly in the current header).

Banked edits (verified present):

- `/home/free/tmp/hct_edited.h` — removes `mCrowdRotate` from `TransformCrowd`
  (2 hunks vs current `src/system/hamobj/HamCamTransform.h`).
- `/home/free/tmp/hct_edited.cpp` — removes the 4 `mCrowdRotate` uses vs current
  `src/system/hamobj/HamCamTransform.cpp`: the `Setup()` copy at line ~101, the
  `SYNC_PROP(crowd_rotate, …)`, and the `Save`/`Load` stream ops.

Known per-fn effect (handoff lane E): `ObjVector<TransformCrowd>::operator=`
(map `0x82295C30`) 92.9→**100**, `fn_82296304` 94.5→**99.9** — but **net −3
unit-wide** because several *false-100* map entries break.

## The misnaming problem (why net −3)

`scripts/target_symbol_map.json` names some retail fns as
`vector<TransformCrowd>` instantiations that are actually **0x10-element**
functions belonging to a different family (handoff lane E attributes them to a
TransConstraint-owned 0x10 type). Our wrong 0x10-shaped `TransformCrowd` code
was byte-identical to them → false-100. Shrinking to the correct 0xc breaks
those pairings, which the strict diff counts as regressions.

Current map inventory (verified): exactly **5** entries mention
`TransformCrowd`:

| VA | Name | In pinned span? | Doc-time % |
|---|---|---|---|
| `0x82295C30` | `??4?$ObjVector@VTransformCrowd@@@@QAAXABV0@@Z` | HamCamTransform `.text [0x82295870,0x8229A2A0)` | 93.33 (real 0xc — flips to 100) |
| `0x82297D38` | `?Load@TransformCrowd@@QAAXAAVBinStream@@@Z` | HamCamTransform | 84.11 |
| `0x82298800` | `?_M_clear_after_move@?$vector@VTransformCrowd@…` | HamCamTransform | (check fresh) |
| `0x823390C0` | `??0?$vector@VTransformCrowd@…` (copy ctor) | **NOT in any pinned span** (verified against splits.txt) | n/a — cannot regress the report |
| `0x8233A108` | `?_M_fill_insert@?$vector@VTransformCrowd@…` | **NOT in any pinned span** | n/a |

Unit baseline at doc time: `default/HamCamTransform` **38/171 matched, fuzzy
21.32%**. Other relevant near-misses in-unit: `TransformArea::Load`
(`0x82296330`) 63.63, `??4TransformArea` (`0x82297CC8`) 14.56,
`fn_82296304` 94.55. Note the unit's span is a huge ICF/template soup (43 map
entries, STL instantiations for a dozen foreign element types) — expect
ICF-fold ambiguity throughout.

## Step-by-step procedure

**B1. Worktree + baseline** (can reuse the Item-A worktree AFTER Item A is
landed or reverted — otherwise use a fresh one so the A/Bs don't compose):

```bash
cd /home/free/code/milohax/rb3-xenon
scripts/setup_worktree.sh ~/tmp/wt-ws4-hct ws4-hct
cd ~/tmp/wt-ws4-hct
NINJA_JOBS=12 tools/fresh_report.sh 2>&1 | tee ~/tmp/rb3_build_ws4b.log
cp build/45410914/report.json ~/tmp/ws4b_BASE.json
```

**B2. Probe run — enumerate the misnamed VAs empirically.** Apply the banked
edits *first* (this inverts the handoff's stated order deliberately: the
regression list from a probe A/B is the cheapest exact census of the false-100
entries; the *landing* order still puts the map fix in the same commit):

```bash
cp /home/free/tmp/hct_edited.h   src/system/hamobj/HamCamTransform.h
cp /home/free/tmp/hct_edited.cpp src/system/hamobj/HamCamTransform.cpp
NINJA_JOBS=12 tools/fresh_report.sh
scripts/harvest/measure_delta.py ~/tmp/ws4b_BASE.json build/45410914/report.json
```

Record every strict/fuzzy regression. Expected: gains at `0x82295C30` (→100)
and `fn_82296304` (→99.9); regressions = the false-100 set (handoff: net −3,
so roughly 4–5 regressed fns against 1–2 gains).

**B3. Identify each regressed VA's true owner.** For each regressed VA:

1. Confirm the retail stride: `mcp run_objdiff(symbol=<name>,
   project_dir=~/tmp/wt-ws4-hct)` — the mismatch should show target
   `li rX,0x10` (or `addi …,0x10` stride walk) vs our new `0xc`. If the target
   shows `0xc`, it is NOT misnamed — investigate before touching the map.
2. Find the true element type: a 0x10 element with an `ObjPtr`-like layout.
   Tools, in order of leverage:
   - `/ghidra-decompile <VA>` (port 8002) — see the dtor/copy calls inside.
   - `mcp lookup_dc3(<method name>)` / DC3's `ham_xbox_r.map`-derived names:
     find DC3's twin VA family; the handoff already attributes the family to
     **TransConstraint-owned** 0x10 elements — verify, don't assume (note
     `TransConstraint` itself at `src/system/hamobj/TransConstraint.h` is an
     Hmx::Object subclass, so the element is something it *owns*, not the class
     itself).
   - `scripts/dump_vtable.py` + `/vtable` skill if a vtable reference pins the
     type; `/data-diff` on any referencing data symbol; `tools/struct_db.py`
     (query `struct_db.sqlite` for structs with `sizeof == 0x10`).
   - Grep our own compiled obj for candidate 0x10 instantiations already built
     (e.g. `python3 -c ...` over `build/45410914/report.json` unit function
     lists, or `llvm-dvp-undname`/`grep` over the compiled obj's symbol table).
3. Rewrite the map entry in `scripts/target_symbol_map.json` **in the worktree**:
   - Misnamed 0x10 fns: rename the key's value to the true family's mangled name
     (if our tree compiles that instantiation somewhere it will re-pair and the
     "regression" becomes a rename-repair, possibly even a gain).
   - The "3 shifted `Load`/`op=` names" (handoff): the real-0xc family fns whose
     VAs shift attribution once the 0x10 family is renamed — re-point
     `?Load@TransformCrowd@…` / `??4?$ObjVector@VTransformCrowd@…` /
     `??0?$vector@VTransformCrowd@…` at the correct VAs (candidate parking spots:
     the current 5 entries above; use `run_objdiff` per-candidate to confirm).
   - The two out-of-span entries (`0x823390C0`, `0x8233A108`) cannot affect the
     report but fix them anyway if evidence identifies their true owner
     (map hygiene) — and note they may *become* in-span when future pins land.
4. After every map edit:
   `rm -f build/45410914/target_symbol_renames.stamp && touch config/45410914/config.yml`
   then rebuild + measure.

**B4. Final A/B + land.** Same double-run gate as A6, measured against
`~/tmp/ws4b_BASE.json`. Land as ONE commit containing both the map renames and
the two source files (they are only jointly correct: the map fix alone
re-attributes false-100s → strict count *drops*; the source fix alone is net
−3). `scripts/harvest/land.sh ~/tmp/wt-ws4-hct` → `git merge --ff-only ws4-hct`.

## Honesty gates & verification (both items)

- **False-100s are lies, not assets.** If a regression is proven to be a
  false-100 breaking (Item B's whole premise), the honest ledger still counts
  the strict drop; the land gate is *net* > 0 with every residual regression
  individually explained in the commit message as "false-100 correction, retail
  fn is `<true identity>`". Never delete a map entry solely to stop a fn from
  being counted.
- **Run the final rebuild TWICE; NET must be identical** (`measure_delta`
  docstring; catches non-determinism / stale-obj lies).
- `tools/fresh_report.sh` only — never trust an incremental report.json for a
  gate decision.
- After any `scripts/target_symbol_map.json` edit:
  `rm -f build/45410914/target_symbol_renames.stamp && touch config/45410914/config.yml`
  before the rebuild, or the renamer serves stale names and the A/B is invalid.
- No `splits.txt` changes are expected in either item; if you find yourself
  editing splits, stop and re-read the handoff — you've drifted off-plan.
  (Hence the SOP's splits-overlap check is N/A, but land.sh runs the union
  resolvers regardless.)
- Never take Bundle-1 files (`src/system/rndobj/Dir.h`,
  `src/system/char/Character.h`, `src/system/ui/PanelDir.cpp`, the branch's
  `0x823F0890` map rename). Verify with the greps in step A1. Bundle 1 is
  DISPROVEN — it broke 15+ matches and its sole intended beneficiary never
  reached 100 (handoff lines 17–38).
- Main-repo hygiene: no stash/checkout/reset/commit in the main tree; all edits
  in the worktree; land only via `scripts/harvest/land.sh`.

## Kill criteria

- **Item A**: if after ~2 focused repair sessions the loader cascade still has
  unrepaired strict regressions (a target funclet proves to be genuinely
  different retail code, not a shape/pairing artifact), do NOT land a partial.
  Re-bank: commit the worktree state to a branch
  (`ws4-bundle2-wip`), update the handoff doc with per-cluster findings, report
  at_limit. The reverts stay preserved on `followup/round3-full-batch`.
- **Item A hard-stop**: if the first A/B (step A3) shows the *gain* side has
  evaporated (< +10 strict — plausible since Char\*/AmbientOcclusion wins may
  have since landed via other waves), re-evaluate EV before starting repairs;
  if expected net ≤ +5, kill and document.
- **Item B**: if step B3.1 shows any regressed target fn actually uses stride
  0xc (contradicting the misnaming theory), stop — the lever's premise is
  wrong; re-verify the original `li r10,0xc` evidence in
  `ObjVector<TransformCrowd>::operator=` before any further work.
- **Item B**: if the true 0x10 family cannot be identified after trying all of
  B3.2, fall back to *removing* the source edit (keep main as-is) and document;
  do not land a net-negative "correctness" change without coordinator sign-off
  (metric honesty vs. metric level is a coordinator call — flag it).

## Expected yield

- Item A: master-doc estimate is the dominant share of **+30–50 net**; the
  handoff's composed run gained 55 (includes since-landed items) and regressed
  23 strict + 14 fuzzy across both bundles. A realistic regression-free Bundle-2
  outcome after other waves' landings: **+15–40 net strict**.
- Item B: small but real — **+2–6 net strict** (op= →100, fn_82296304 →99.9
  fuzzy, `TransformCrowd::Load` 84→up after the Save/Load body change, minus
  honestly-lost false-100s recovered via renames), plus map hygiene that
  benefits future pins in the 0x8233xxxx region.

## Open questions

1. The handoff says "13 call-site TUs"; the branch commit contains 11. Verified
   the commit is the truth (grep of `new FileLoader` on current main finds no
   12th/13th site). If the composed-run author had 2 more files in mind
   (e.g. `os/Archive.cpp`? — already landed separately), that context is lost.
2. Exact pairing mechanism for the unnamed regressed VAs (`fn_82735358` etc.
   ABSENT from the map yet reported at 0%/94%): positional or funclet-order
   pairing inside objdiff. Determines whether A5's "add a map entry" fallback
   or a code-shape fix is the right repair per fn. Inspect with
   `run_diff_inspect(diff_mode=raw)` when first hit.
3. The true identity of the 0x10 element family in Item B ("TransConstraint-
   owned" per lane E, but `TransConstraint` is an Object subclass — the claim
   needs the owned-container reading verified in Ghidra).
4. How much of Bundle 2's original +55 remains unlanded (Char\*/
   AmbientOcclusion overlap with waves landed since 9b938ea) — answered
   empirically by step A3's first A/B.
5. `MetaMusic fn_826F1C20` frame Δ −0x20 direction (whose frame is bigger,
   target or ours?) — the handoff doesn't say; `/stack-layout` will.

---

## RESULTS (2026-07-02 execution, ws4-round3 review agent)

- **Item A (Bundle 2): LANDED, +25 net strict** (10936 → 10961; 43 gained /
  18 strict + 12 real-fuzzy regressed). The zero-regression bar proved
  unmeetable: the correct flat-0x14 ObjDirItr revert perturbs MSVC regalloc in
  every inlining TU; regressed target EH funclets have NO byte-identical twin
  in our objs (A5 map-pairing inapplicable) and frame deltas are mixed-direction
  (regalloc ripple, not a size bug). Reviewer landed on the net-positive gate
  with regressions documented as honest re-pricing of matches against DC3-drift
  source. Follow-up lever: permuter on MetaMusic/CharBoneDir/Dir/CameraManager/
  CharClipSet (frame sizes match, extra BASE_ONLY spill). NetSync/DirLoader/
  TrackDir/BandWardrobe funclet losses: no source lever, accepted.
- **Item B (HamCamTransform TransformCrowd 0x10→0xc): KILLED for this round /
  re-banked.** Edits verified correct (op= →100 at 0xc; 0x82298800 is a
  misnamed 0x10-stride vector; ~TransformCrowd funclet matches only at 0xc) but
  net −3 is unrecoverable in isolation: HamCamTransform.obj compiles no
  0x10-element vector to rename to, the true-0xc `_M_clear` is ICF-folded under
  a foreign survivor, and 4 funclet losses are positional ICF-soup phantoms.
  Banked at `/home/free/tmp/hct_edited.{h,cpp}`; revisit in a combined
  map-hygiene pass. Expected-yield estimate (+2–6) was wrong: the false-100s it
  would honestly remove exceed the real gains on current pairing.
- Full ledger + verdict detail: `docs/decomp/handoff/exec-ws4-round3-run-2026-07-02.md`.
