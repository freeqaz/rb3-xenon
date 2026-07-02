# WS3 — Option-C oracle-cluster PORT-THEN-PIN harvest (execution plan, 2026-07-02)

**Cold-start doc.** Everything a fresh agent needs is embedded here: paths, hashes,
VAs, commands, expected numbers. Read `/home/free/code/milohax/rb3-xenon/CLAUDE.md`
first (worktree rules, never-stash rules, build tracks).

## Objective

Execute the two GO verdicts from the 2026-06-30 option-C investigation
(`docs/decomp/research/2026-06-30-option-C-scan-directions.md`):

1. **DC3-oracle engine BODY harvest** — for each remaining target TU: locate its
   real code via `dc3_oracle.json` anchors, wire + pin + port the body from the
   DC3/rb3-Wii source oracles, and land honest strict matches (+40–70 estimated
   at scan time, part already consumed).
2. **Stub-filtered contiguity scan tool** — a reusable, build-FIRST deliverable
   that makes target selection rigorous for this and future waves (rank unpinned
   TUs by HONEST matchable real-body bytes, not stub/funclet-inflated counts).

## Current state (verified 2026-07-02, this session)

- main @`f83045e` (44 commits past the master doc's `44f57c6`).
  `build/45410914/report.json` measures: **10,934 / 65,607 matched (16.67%)**,
  8.42% strict code bytes, 11.57% fuzzy. (Master doc
  `docs/plans/frontier-workstreams-2026-07-02.md` says 10,870/65,596 — it is a
  few landings stale; re-read report.json when you start.)
- `dc3_oracle.json` — EXISTS, tracked, repo root, 33,987 rows of
  `{dc3_va, rb3_va, dc3_name, dc3_tu, similarity, confidence}`. Generator:
  `tools/build_dc3_oracle.py` (BinDiff DC3↔RB3 sqlite × leaked `ham_xbox_r.map`).
- `tools/icf_alias_check.py` — EXISTS, tracked. CLI:
  `--tu Foo.cpp | --range 0xA-0xB | --worktree PATH [--base-ref REF]`;
  exit 0 = HONEST, 1 = ICF-ALIAS INFLATION.
- `fingerprints.json` (12 MB) + `autoid.json` — EXIST at repo root (gitignored,
  regenerable via `tools/fingerprint_match.py extract`).
- Supporting tools verified present: `tools/ab_measure.py`,
  `scripts/harvest/measure_delta.py`, `scripts/harvest/land.sh`,
  `tools/safe_name_merge.py`, `tools/reveal_sweep.py`,
  `tools/gen_game_target_map.py`, `tools/dc3_name_eligible.py`,
  `scripts/setup_worktree.sh`.

### Status of the 8 ranked targets from the 2026-06-30 doc (each VERIFIED)

| # | Target (optC rank) | Status 2026-07-02 | Evidence |
|---|---|---|---|
| 1 | synth/Sound.cpp | **KILL — TU does not exist in RB3** | see "Sound.cpp kill" below |
| 2 | CharClipGroup.cpp | **CONSUMED** (fuzzy-pinned; banked +2 strict flip READY) | `config/45410914/splits.txt` has 2 micro-pins (0x8237B698, 0x8237C598); handoff `docs/decomp/handoff/charclipgroup-objvector-flip-READY.md`; cold worktree `/home/free/tmp/wt-ov-CharClipGroup` (branch `ov-CharClipGroup`) |
| 3 | utl/SongInfoAudioType.cpp | **DEMOTED — optC span was a funclet island, not this TU** | see per-target section |
| 4 | NavListNode.cpp | **LIVE** (STL cluster free, names already in map via content-match) | gap [0x82643bd0,0x82649c38) unpinned |
| 5 | synth/MoggClip.cpp | **LIVE — best remaining target** | 11 sim-1.0 anchors, gap [0x826efb60,0x826f1520) free |
| 6 | AccomplishmentProgress.cpp | **CONSUMED** | pinned [0x82577680,0x8257a960); report.json: 69/110 fns matched |
| 7 | MotionBlur.cpp + SoftParticles.cpp | **LIVE — lowest friction** | both clusters FREE in gap [0x82480660,0x824822d8) |
| 8 | ProfileMgr.cpp | **KILL for ws3** | optC span [0x825FDA94,0x826006A0) sits inside the `band3/meta_band/EditSetlistPanel.cpp` pin [0x825fe180,0x82603030) → case-B territory (ws5). ProfileMgr.cpp itself already has 3 micro-pins @`b08dc4e` at 0x82532198+ |
| — | MetaMusic.cpp (not in the 8, same method) | **CONSUMED +37** | commit `7c7823d` |

### ⭐ Critical calibration discovered while verifying (bakes into everything below)

**The optC doc's per-TU spans/counts are FUNCLET-INFLATED.** Re-deriving the
"dense clusters" from `dc3_oracle.json` shows the optC spans for Sound
(~0x8243a10c, "30 methods purity 1.0"), MoggClip (~0x8244905c–0x8244922c), and
SongInfoAudioType (0x82530f50–0x82531198, "18 purity 1.0") are islands of
`__unwind$NNNNN` **EH funclets** (~0x20 bytes each), not method bodies. Always
filter `dc3_name.startswith("__unwind")` before clustering.

**Oracle recall on real bodies is near zero for spans — it gives ANCHORS.**
Ground truth: the landed MetaMusic pin [0x826F1BF4,0x826F40C0) contains exactly
**1 of the oracle's 52 MetaMusic.obj rows** (`?ChooseStartMs@MetaMusic@@ABAHXZ`
@0x826f2028, sim 1.0). The other 51 rows are ICF/funclet scatter across the
whole binary. This is the reframe from the optC doc, sharpened: *the oracle sim
is a locator/namer, NOT a feasibility scorer* — and even as a locator it yields
one-or-few high-sim anchor VAs, not a span. The span comes from the
**splits.txt gap neighborhood** around the anchor + `config/45410914/symbols.txt`
fn boundaries. The real port-success predictor = body composition (clean logic
vs STL-template soup) × locatability × stub ratio.

### Sound.cpp kill (verified, do not re-attempt)

- rb3-Wii has **no** `src/system/synth/Sound.cpp` (checked `../rb3/src/system/synth/`
  — Sfx.cpp/Sequence.cpp/Stream.cpp etc., no Sound). RB3-360 compiles the same
  game tree → almost certainly no Sound TU.
- DC3 `../dc3-decomp/src/system/synth/Sound.cpp` handler strings
  (`camp_gameplay_failure`, `on_marker_event`, `interrupted`) → **0 hits** in
  `strings -a orig/45410914/band.exe`.
- Its only non-funclet high-sim oracle anchors are generic templates
  (`vector<SampleMarker>` dtor 0.938, `ObjRefConcrete<MoggClip>::GetObj` 1.0) = ICF decoys.
- The 30-funclet island 0x8243a10c–0x8243a4fc sits in the carved gap between two
  `Part.cpp` spans ([...,0x82439d8c) and [0x8243b7d8,...)). Whatever TU owns that
  gap, it is an RB3 synth TU (candidate: Sfx.cpp / SampleData.cpp — check the
  rb3-Wii synth list), **not** DC3's Sound. Re-locating that gap is a fine side
  quest for the scan tool, not a ws3 target.

## Remaining ranked targets (this wave)

Rank = real-bodies × cleanliness × low-risk, per the optC gating rule.

### Target 0 (quick win, banked): CharClipGroup ObjVector flip — +2 strict

Fully scoped and asm-validated; blocked only on box load at the time. Follow
`docs/decomp/handoff/charclipgroup-objvector-flip-READY.md` **verbatim** (exact
header/cpp edits listed there). Worktree `/home/free/tmp/wt-ov-CharClipGroup`
(branch `ov-CharClipGroup`, 0 commits) is already set up. Expected: FindClip
0x8237B698 91.9%→100, Save 0x8237C598 99.9%→100. Gates: objdiff both fns 100,
whole-binary A/B net ≥ +2 with 0 regressions, `icf_alias_check`. Do NOT retry
Flow/FlowNode (refuted in the same handoff).

### Target 1: rndobj/MotionBlur.cpp + rndobj/SoftParticles.cpp (engine pair)

- **Location:** free gap between `AmbientOcclusion.cpp` pin end 0x82480660 and
  `system/rndobj/CubeTex.cpp` pin start 0x824822d8 (0x1c78 bytes, verified
  unpinned). Oracle anchors (sim 1.0, non-funclet):
  - MotionBlur: `?CanMotionBlur@RndMotionBlur@@IAA_NPAVRndDrawable@@@Z` @0x82480a90,
    `?Save@RndMotionBlur@@UAAXAAVBinStream@@@Z` @0x82480b40,
    `?Copy@RndMotionBlur@...` @0x824811e8.
  - SoftParticles: `?Save@RndSoftParticles@...` @0x824818e0,
    `?Copy@RndSoftParticles@...` @0x82481f50.
  - 0x82480A90 and 0x824818E0 are **already named** in
    `scripts/target_symbol_map.json` (content-match sweeps) — corroboration.
  - Likely layout: MotionBlur ≈ [0x82480660..0x82481~8xx), SoftParticles ≈
    [0x82481~8xx..0x824822d8). Refine with symbols.txt boundaries.
- **Sources:** `../dc3-decomp/src/system/rndobj/MotionBlur.{cpp,h}` (82 lines),
  `SoftParticles.{cpp,h}` (65 lines); cross-check
  `../rb3/src/system/rndobj/MotionBlur.cpp` + `SoftParticles.cpp` (both exist).
  `motion_blur` string: 4 hits in retail. AmbientOcclusion sibling already solved
  — reuse its patterns (`git log --oneline -S AmbientOcclusion -- config/45410914/splits.txt`).
- **Yield estimate:** +8–20 (11 + 5 non-funclet oracle rows; small clean TUs).

### Target 2: synth/MoggClip.cpp (engine, biggest remaining)

- **Location:** synth cluster chain proven by the MetaMusic land (`7c7823d`):
  … → MoggClip → MicClientMapper → MetaMusic → Stream. Free gap
  [0x826efb60, 0x826f1520) (0x19c0 bytes) + the contested head
  [0x826ef868, 0x826efb60) which interleaves with **three tiny `BinkClip.cpp`
  micro-pins** ([0x826EF948,0x826EF9BC), [0x826EFA28,0x826EFA84),
  [0x826EFB20,0x826EFB60)). Verified sim-1.0 anchors + existing map names:
  `?Pause@MoggClip@@UAAX_N@Z` @0x826ef868, `?IsReadyToPlay@` @0x826ef9e0,
  `?UnloadWhenFinishedPlaying@` @0x826EF9D8 (named in map),
  `?UpdateFaders@` @0x826EFD30, `?UpdatePanInfo@` @0x826efd98,
  `?AddFader@` @0x826F06E8, `??0MoggClip@@IAA@XZ` @0x826f0790,
  `??_EMoggClip@` @0x826f0c98, `?SetPan@` @0x826f0ce8,
  `?SetupPanInfo@` @0x826f0da8.
- **⚠ Conflict to resolve first:** the oracle attributes
  `?KillStream@MoggClip@@AAAXXZ` to 0x826efa28, which the `BinkClip.cpp` pin
  claims. BinkClip and MoggClip are sibling stream clips with ICF-identical tiny
  methods. Do NOT break the landed BinkClip matches: either carve the MoggClip
  pin ranges AROUND the three BinkClip micro-pins (multi-range pin, established
  practice — see the TambourineManager/DepthBuffer3D carve `f9d212c` for how
  carve-outs go wrong and get fixed), or prove via
  `python3 tools/ghidra/mcp_client.py` decompile + strings which TU really owns
  each and re-attribute in one commit.
- **Sources:** `../dc3-decomp/src/system/synth/MoggClip.cpp` (435 lines) and
  `../rb3/src/system/synth/MoggClip.cpp` (both exist — cross-check; per
  CLAUDE.md the DC3 flavour may be version-drifted; MetaMusic needed the rb3-Wii
  flavour). `MoggClip` string: 6 hits in retail.
- **Yield estimate:** +15–30 (33 non-funclet oracle rows, 9-row dense real
  cluster verified; MoggClipMap.cpp is ALREADY wired+pinned separately — do not
  collide with it).

### Target 3: NavListNode.cpp (game/meta, DC3-source oddity)

- **Location:** free gap between `SongSortNode.cpp` pin end 0x82643bd0 and
  `PropKeys.cpp` pin start 0x82649c38 (0x6068 bytes, verified unpinned). The
  dense STL cluster 0x826454e8–0x82646220 (7 oracle rows, sim 1.0:
  `__lower_bound/__upper_bound/__equal_range<_List_iterator<NavListSortNode*>...>`,
  `??0NavListShortcutNode`, `??1NavListShortcutNode`, `??_GNavListShortcutNode`,
  `?Insert@NavListShortcutNode` @0x82646220 sim 0.985) is **already fully named
  in `scripts/target_symbol_map.json`** — added by the content-match sweeps
  (`ea5b744` lineage), i.e. byte-equality against the DC3 oracle binary already
  confirmed RB3 retail contains this code. Second cluster
  0x825a6640–0x825a7090 is partially consumed by the
  `band3/meta_band/StoreSongSortNode.cpp` pin ([0x825a6640,0x825a7038)) —
  `??_GNavListSortNode` @0x825a7038 and `??0NavListHeaderNode` @0x825a7090 sit
  just past its end.
- **⚠ Source risk (the reason this is #3 not #1):** the ONLY source is
  `../dc3-decomp/src/lazer/meta_ham/NavListNode.cpp` (316 lines) — DC3 *game*
  code. rb3-Wii has NO NavList files (verified `grep -rl NavList ../rb3/src` →
  empty) and retail has no "NavList" strings (fine — these classes have no
  string anchors). Content-match already proved byte-identity for the named
  fns, so the port should reproduce them; the un-named remainder of the gap may
  be a DIFFERENT RB3 TU (SongSort-family). Pin conservatively: only the
  0x826454e8–0x82646220+ cluster you can boundary-verify, not the whole gap.
  Note the neighborhood was already partition-resolved once — read
  `git show b138024` (SongUpgradeMgr, "partition-resolve the 0x8263x multi-class
  gap") before touching it.
- **Yield estimate:** +7–15.

### Target 4 (thin tail): utl/SongInfoAudioType.cpp

- The optC span [0x82530f50,0x82531198) is a **funclet island** (18 `__unwind$`
  rows), and RB3's TU is the rb3-Wii flavour (pre-interned `utl/Symbols.h`
  symbols, 6 audio types — see `../rb3/src/system/utl/SongInfoAudioType.cpp`),
  NOT DC3's 18-static-Symbol version — so 18 funclets cannot be this TU. The
  single real oracle anchor is `?SymbolToAudioType@@YA?AW4SongInfoAudioType@@VSymbol@@@Z`
  @0x825d3958 (sim 0.597, unverified). Related named fn already in map:
  `?NumChannelsOfTrack@SongInfoCopy@...` @0x827ABEA0 (different TU, SongInfo.cpp).
- Only attempt if the scan tool (below) corroborates 0x825d3958's neighborhood;
  expected yield +2–4 (TU is ~3 functions). Otherwise skip.
- Side note for the scan: the free gap [0x8252e608, 0x82532198) (after
  `MusicLibrary.cpp`, before the ProfileMgr micro-pins) contains the 18-funclet
  island and is a real unpinned neighborhood worth re-attributing.

### Near-miss batch (optC 4c) — hand to the right owner, not ws3

`CharEyes::EyesOnTarget` @0x82371090 (named in map) and the "small-insert /
no-reg-swap" cohort are per-function near-miss work (permuter/body tweak), not
port-then-pin. Do it only if trivially adjacent to work you're already doing.
CharEyes history: `24f2c42` (struct-drift +16) already landed; check current %
with `run_objdiff` before touching.

## Build-first deliverable: STUB-FILTERED CONTIGUITY SCAN tool

Build this BEFORE (or in parallel with) target 1 — it de-risks every later pin
and is the reusable artifact the optC doc calls for.

**Path:** `tools/oracle_contiguity_scan.py` (new).
**Inputs (all exist):**
- `dc3_oracle.json` (repo root) — anchors + names.
- `fingerprints.json` (repo root, regen: `python3 tools/fingerprint_match.py extract`)
  and/or `config/45410914/symbols.txt` — real fn sizes
  (`fn_<VA> = .text:0x<VA>; // type:function size:0xHEX`).
- `config/45410914/splits.txt` — already-pinned `.text` ranges (parse: unit
  header line `^(\S.*?):$`, range line `^\s+\.text\s+start:(0x\w+) end:(0x\w+)$`).
- `scripts/target_symbol_map.json` — existing names (corroboration signal).

**Algorithm (per the optC spec, hardened by this session's findings):**
1. Drop oracle rows where `dc3_name.startswith("__unwind")` (funclet decoys) —
   but KEEP funclet islands as a separate secondary signal (≥8 contiguous
   funclets attributed to one dc3_tu = "a TU with EH lives adjacent"; bodies
   usually border the island).
2. Drop rows whose RB3 fn size (symbols.txt) ≤ 64 bytes AND sim < 0.9
   (stub-fold decoys; the icf_alias_check 44-byte lesson, padded).
3. Cluster surviving rows per dc3_tu by VA gap ≤ 0x1000.
4. Intersect clusters with the UNPINNED gaps of splits.txt; compute per-TU
   "honest matchable body bytes" = Σ symbols.txt sizes of real-bodied cluster
   members in unpinned space.
5. Rank TUs by honest bytes × cluster density; annotate with: source-file
   existence in `../dc3-decomp/src` and `../rb3/src`, count of members already
   named in target_symbol_map.json, and the neighboring pinned TUs of the gap.

**Validation gates (run before trusting output):**
- MoggClip, MotionBlur+SoftParticles, NavListNode must rank near the top.
- Sound.obj and SongInfoAudioType.obj must NOT rank (their signal is funclets).
- AccomplishmentProgress/MetaMusic must show as consumed (pinned).
Print these as a self-check mode (`--validate`).

## Per-TU procedure (the option-C plan, verbatim-in-spirit)

For each target, in an isolated worktree:

```bash
# 0. worktree (btrfs CoW, buildable; ~/tmp NEVER /tmp)
scripts/setup_worktree.sh ~/tmp/wt-ws3-<tu> ws3-<tu>
cd ~/tmp/wt-ws3-<tu>
# freeze baseline for A/B
cp build/45410914/report.json ~/tmp/ws3-<tu>.baseline.report.json
```

1. **Locate** the dense VA core-cluster from `dc3_oracle.json` — high-sim
   (≥0.9) NON-funclet anchors only (snippet):
   ```bash
   python3 - <<'EOF'
   import json
   d=json.load(open('dc3_oracle.json'))
   tu='MoggClip.obj'   # <-- target
   for e in sorted((e for e in d if (e.get('dc3_tu') or '').split(':')[-1]==tu
                    and not e['dc3_name'].startswith('__unwind')
                    and e['similarity']>=0.9), key=lambda e:int(e['rb3_va'],16)):
       print(e['rb3_va'], round(e['similarity'],3), e['dc3_name'][:80])
   EOF
   ```
2. **Refine to real fn boundaries**: walk `config/45410914/symbols.txt` from the
   anchors outward; the span must start/end exactly on fn boundaries and stay
   inside the free splits gap (verify with the overlap snippet in this doc's
   history, or `scripts/harvest/overlap_check.py`). Sanity-check ownership with
   Ghidra (`tools/ghidra/mcp_client.py`, port 8002) or
   `build/45410914/asm/` listings.
3. **Wire `config/45410914/objects.json`**: add the `.cpp` as `"NonMatching"`.
4. **Pin `config/45410914/splits.txt`** (`.text` only; dtk back-fills `.pdata`);
   carve multi-range around foreign micro-pins where needed (MoggClip/BinkClip):
   ```
   system/synth/MoggClip.cpp:
       .text       start:0xAAAAAAAA end:0xBBBBBBBB
   ```
   Then: `touch config/45410914/config.yml && ./tools/ninja-locked 2>&1 | tee ~/tmp/rb3_build_ws3-<tu>.log`
5. **Generate target-map names from the oracle mangled names** — ADD-ONLY into
   `scripts/target_symbol_map.json`, gated by `tools/safe_name_merge.py`
   (ICF/collision gate, as the MetaMusic land did). Engine names come straight
   from `dc3_oracle.json` `dc3_name`; game TUs use `tools/gen_game_target_map.py`
   (rb3-Wii oracle). After the port compiles, run `tools/reveal_sweep.py` to
   catch byte-exact fns the map missed (self-validating).
6. **Port the body**: engine from `../dc3-decomp/src/system/...`, game from
   `../rb3/src/...` — and ALWAYS cross-check the other oracle (CLAUDE.md caveat:
   DC3 is newer; MetaMusic needed the rb3-Wii flavour). MWCC→MSVC port notes in
   `git log -1 --format=%B 7c7823d` (pointer-to-member syntax, static-local
   Symbols, platform-call swaps) and `docs/decomp/patterns/`.
7. **Build** (step-4 command) and iterate with the `decomp` MCP tools
   (`run_objdiff` / `run_analyze_function` — ALWAYS pass
   `project_dir=~/tmp/wt-ws3-<tu>`).
8. **Stub filter / honesty audit**:
   `python3 tools/icf_alias_check.py --worktree ~/tmp/wt-ws3-<tu> --baseline-report ~/tmp/ws3-<tu>.baseline.report.json`
   — must exit 0. Reject ≤44–64B stub-folds; count ONLY real bodies in the
   claimed yield.
9. **Composed A/B, run1 == run2**: build twice;
   `python3 tools/ab_measure.py --worktree ~/tmp/wt-ws3-<tu> --baseline ~/tmp/ws3-<tu>.baseline.report.json`
   must report identical net on both runs, net > 0, **0 regressions** (also see
   `scripts/harvest/measure_delta.py`). Splits edits need `--resplit`.
10. **Land**: commit in the worktree branch (never on main; never stash/checkout
    in the main repo), then `scripts/harvest/land.sh` per its header /
    `scripts/harvest/README.md`, or hand to the lander agent with a handoff doc
    under `docs/decomp/handoff/`.

## Honesty gates (every target, no exceptions)

- `tools/icf_alias_check.py` exit 0 on the new/changed span.
- Whole-binary A/B net-positive with 0 regressions, reproduced twice
  (run1 == run2) from the frozen baseline.
- Yield counted in REAL bodies only (>44B or oracle-attributed to the claimed
  TU); funclets/`??__E`/`??__F`/guard thunks are not wins.
- No pin may overlap an existing splits.txt range
  (`scripts/harvest/overlap_check.py`; the composed-splits corruption fixed in
  `f9d212c` is the cautionary tale).
- Names merged ADD-ONLY via `tools/safe_name_merge.py`; a name that doesn't
  produce a byte-exact match is removed, not kept as decoration.

## Kill criteria

- **Per TU:** kill if (a) no ≥0.9-sim non-funclet anchor survives boundary
  refinement; (b) the ported body diverges structurally (BandProfile-style
  MWCC≠MSVC wall) after both oracle flavours are tried; (c) icf_alias_check
  flags inflation that carving can't fix; (d) A/B shows any sibling regression
  the fix for which exceeds the TU's own yield. Bank a handoff doc and move on.
- **Whole stream:** kill when the scan tool's ranked list has no TU with ≥6
  honest matchable real bodies in free space — that's the signal option-C is
  drained and effort moves to ws5 (case-B) / ws6 (reconstruction).
- Do NOT re-attempt the verified kills: synth/Sound.cpp (TU absent),
  ProfileMgr optC cluster (foreign-pinned = ws5), Flow/FlowNode flips,
  the dead levers listed in the master doc.

## Expected yield

| Item | Estimate |
|---|---|
| Target 0 CharClipGroup flip (banked, validated) | +2 strict |
| Target 1 MotionBlur + SoftParticles | +8–20 |
| Target 2 MoggClip | +15–30 |
| Target 3 NavListNode | +7–15 |
| Target 4 SongInfoAudioType (conditional) | +0–4 |
| Scan-tool discoveries (fresh TUs, e.g. the Part.cpp-gap synth TU) | +0–30 |
| **Stream total** | **~+35–100** |

Consistent with the master doc's +60–100 minus the already-consumed
MetaMusic (+37) and AccomplishmentProgress portions.

## Open questions (resolve during execution)

1. MoggClip vs BinkClip ownership of the three micro-pins at
   0x826EF948/0x826EFA28/0x826EFB20 — carve around, or re-attribute? (Needs a
   Ghidra/asm look; do not regress the landed BinkClip matches.)
2. Which RB3 synth TU owns the Part.cpp split gap [0x82439d8c,0x8243b7d8)
   (the 30-funclet island)? Candidates from `../rb3/src/system/synth/`:
   Sfx.cpp, SampleData.cpp, Sequence.cpp. Scan-tool + strings job.
3. NavListNode remainder: after pinning the verified STL cluster, does the rest
   of the [0x82643bd0,0x82649c38) gap belong to NavListNode bodies (DC3 lazer
   source ports cleanly?) or to an RB3 SongSort-family TU (rb3-Wii oracle)?
4. Does the funclet-island secondary signal (scan step 1) reliably border its
   TU's bodies on this binary? Calibrate on landed TUs (MetaMusic, MasterAudio,
   CharClipSet) before using it to propose spans.

## RESULTS (executed 2026-07-02, branch exec/ws3-optionc-0702)

**Composed A/B (reviewer-run, authoritative): baseline 10936 -> 11021, NET +85
strict matched functions, 5 units up / 0 units down, deterministic (run1==run2,
full resplit rebuilds).** Gates: `icf_alias_check --worktree` VERDICT HONEST
exit 0 (36 real-bodied / 32 own-unit stub-folds in the newly-matched set);
`overlap_check` clean (.text 0/1085, .pdata 0/994).

Per-packet verdicts (every key number reproduced by the reviewer via MCP
run_objdiff against the packet worktree):

| Packet | Verdict | Composed yield | Notes |
|---|---|---|---|
| p1 charclipgroup-flip | ACCEPT | +2 (CharClipGroup 0->2) | FindClip 100/100, Save 100 norm (99.9 raw, reloc-name residual). Handoff's "keep unk24" was WRONG (DC3 drift): retail has no unk24; vtordisp@0x1c + vbase@0x20. GetClip/MakeMRU ported from rb3-Wii. |
| p2 motionblur-softparticles | ACCEPT | +25 (MotionBlur 0->11, SoftParticles 0->14) | Copy x2 + ctor + Save spot-checked 100 norm. Fixed 4 pre-existing map mislabels (Copy/ClassName ICF twins). Conservative pin start 0x82480A90 (gap head is a foreign AO-sibling TU). |
| p3 moggclip | ACCEPT | +38 (MoggClip 0->38) | ctor/Save/PreLoad spot-checked 100 norm. BinkClip carve verified no-regress (SetLoop 88.3 == frozen baseline). 36 real-bodied anchors in composed ICF audit. Deferred: SynthPoll 97.6, LoadFile 95.8, SetupPanInfo 92.9, Play/Handle/SyncProperty/LoadNumChannels walls. |
| p4 navlist-scantool | ACCEPT | +20 (SongSortNode 1->21) | 17 real-bodied named (Insert@ShortcutNode/Renumber/ctor spot-checked 100 norm) + 3 small own stub-folds. KEY: 0x826454E8 cluster is SongSortNode.cpp's own unpinned code (DC3 renamed the classes NavList*), not a fresh NavListNode TU — 11 dormant DC3 NavList map names removed. Scan tool `tools/oracle_contiguity_scan.py --validate` = ALL GATES PASS **against the pre-p4 baseline config** (post-consumption the NavListNode/MoggClip gates report rank=None — expected, the self-test's expectations are baseline-state-dependent; run with `--splits/--map` pointing at a pre-consumption config to re-validate). |

Open question 1 (MoggClip vs BinkClip micro-pins) resolved: CARVE (multi-range
.text pin around the 3 landed BinkClip ICF micro-pins), zero BinkClip movement.
Open question 3 partially resolved: the STL/node cluster belongs to
SongSortNode.cpp itself.

Next-wave feed (scan-tool top ranks, pre-consumption config): Cam(13 bodies),
PlatformMgr_Xbox(10), StandardStream(9), Lit_NG(5), Faders(9), HamStorePanel(7),
HamSongMgr(5), PartyModeMgr(5), App(6), TexLoadPanel(4).
