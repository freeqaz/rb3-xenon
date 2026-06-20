# W9-L10 dossier — "reveal-sweep-generic-50-on-landed-prereq"

**Verdict: REAL_ACTIONABLE.** The frontier's stale `/tmp/reveal_postprereq.json`
(50 entries, generated on the a7175af "Handle macros" base) is now **49 valid
reveals on current main@8314** (812e1df). One entry (UIComponent::ResourceDir
@0x827D9428) went stale post-wave-8 and dropped. Of the 49, a **fresh, honest
`reveal_sweep.py` + `safe_name_merge.py` gate on main@8314** yields **20
gate-clean, byte-exact, independently landable reveals (+20)**, plus a recoverable
**+18 the gate over-rejects** (cross-unit ICF name collisions where the existing
dup lives in a *dead* `auto_03_*_text` blob @0% — different obj, no ambiguity).
The frontier's est +45 is **plausible** if the cross-unit recovery lands; the
**floor is +20** and is rock-solid.

## Method / ground truth (all read-only in main; reveal_sweep reads existing build objs, no ninja)

1. `git`: a7175af IS an ancestor of main; wave-8 (812e1df) added Character +45
   relocation (38a773a), Waypoint mConnections ObjVector (d3c6e4f), MakeString
   by-value (36b9817) — all AFTER the json was captured. So the stale json must
   be re-validated against current objs, not trusted.
2. All 50 stale addrs are present in current `report.json` as `fn_<addr>` @0.0%
   in their owning pinned unit — the reveal precondition holds.
3. **Fresh run on main**: `tools/reveal_sweep.py --out /tmp/reveal_now.json
   --emit-fragment /tmp/reveal_now_frag.json` → **49 candidates** (vs 50 stale).
   Diff: only `0x827D9428 UIComponent::ResourceDir` dropped (no longer byte-exact
   after a wave-8 UIComponent change). No name/addr mismatches on the other 49.
4. **Gate**: `tools/safe_name_merge.py --gate /tmp/reveal_now_frag.json
   --out /tmp/reveal_safe.json` → **20 safe, 29 rejected (all `name_collision_tsm`)**.
5. Independent byte-equality (`fuzzy_content_match.word_eq_frac` on the current
   target obj vs current compiled base obj): **all 20 safe = word_eq 1.0**, sizes
   match. Spot-checked cross-unit and same-unit rejects: also word_eq 1.0 (byte-
   equality is NOT the blocker; objdiff name-pairing ambiguity is).

## The 20 SAFE (+20 floor) — gate-clean, byte-exact, in current pins

| addr | unit | sz | symbol |
|---|---|---|---|
| 0x82282488 | BandDirector | 124 | `?SetCharSpot@BandDirector@@QAAXVSymbol@@0@Z` |
| 0x82453D98 | Console | 312 | `?List@RndConsole@@QAAXXZ` |
| 0x82322DA0 | BandCharDesc | 172 | `?MakeInstrumentPath@BandCharDesc@@QAAXVSymbol@@0AAVFilePath@@@Z` |
| 0x827A63B0 | HxGuid | 32 | `?ToString@HxGuid@@QBAPBDXZ` |
| 0x8235B350 | Character | 48 | `?RemovingObject@Character@@MAAXPAVObject@Hmx@@@Z` |
| 0x8235B4B8 | Character | 200 | `?UpdateSphere@Character@@UAAXXZ` |
| 0x8235B700 | Character | 52 | `?BoneServo@Character@@QAAPAVCharServoBone@@XZ` |
| 0x8235B840 | Character | 156 | `?ComputeScreenSize@Character@@UAAMPAVRndCam@@@Z` |
| 0x8235BE18 | Character | 88 | `??$New@VRndGroup@@@ObjectDir@@QAAPAVRndGroup@@...` |
| 0x8235C5D8 | Character | 88 | `??4Lod@Character@@QAAAAU01@ABU01@@Z` |
| 0x8235CF48 | Character | 160 | `?OnPlayClip@Character@@IAA?AVDataNode@@PAVDataArray@@@Z` |
| 0x82479EE8 | AmbientOcclusion | 120 | `?DumpObjList@RndAmbientOcclusion@@IBAXPBDABV...@Z` |
| 0x823C82F0 | Waypoint | 156 | `?OnWaypointFind@Waypoint@@CA?AVDataNode@@PAVDataArray@@@Z` |
| 0x823C8390 | Waypoint | 176 | `?OnWaypointNearest@Waypoint@@CA?AVDataNode@@PAVDataArray@@@Z` |
| 0x823C8790 | Waypoint | 100 | `??$?6V?$ObjOwnerPtr@VWaypoint@@@@...@@YAAAVBinStream@@...` |
| 0x823C8B58 | Waypoint | 116 | `?_M_erase@?$vector@V?$ObjOwnerPtr@VWaypoint@@@@...@Z` |
| 0x823C8F28 | Waypoint | 84 | `?Terminate@Waypoint@@SAXXZ` |
| 0x823C8F80 | Waypoint | 340 | `??4?$vector@V?$ObjOwnerPtr@VWaypoint@@@@...@Z` |
| 0x823C9870 | Waypoint | 108 | `??_DWaypoint@@QAAXXZ` |
| 0x82517900 | Memcard_Xbox | 76 | `?GenerateDriveName@MemcardXbox@@QAA?AVString@@...` |

All Character reveals ∈ current pin [0x8235B1D0,0x8235F180); all Waypoint reveals
∈ current pin [0x823C7CC8,0x823CA668) — i.e. brought in by the wave-8 relocations,
now byte-exact, just unnamed. No intra-batch (unit,name) dup; none already matched
in their unit.

## The 29 rejects — two distinct classes

**Class A — 20 CROSS-unit collisions (RECOVERABLE; gate over-rejects).** The
colliding existing name lives in a DIFFERENT unit obj. `obj_target_symbol_renamer`
keys by `fn_<addr>` symbol and writes into the one obj that holds that VA, so two
addrs→one name across two objs each get exactly one same-named symbol → no per-obj
ambiguity. 18 of these have the existing dup in a **dead `auto_03_*_text` blob @0%**
(unpinned, never matched) — recovering the reveal is pure upside, the blob entry is
inert. The other 2: EventTrigger `??_GObjRefOwner` dup is in **DepthBuffer3D @100%**
(different obj — should be undisturbed but A/B-verify); Voice `push_back<deque<PoolVoice>>`
dup is in **VocalTrack @0%** (inert). To recover: the reveal frag must be merged
*bypassing the global rule-2 value-collision check* OR add the reveal addr while
the conflicting auto_03 entry can stay (it's harmless). Safest mechanical path: add
these per-addr to `target_symbol_map.json` in a worktree, whole-binary A/B, keep
only report-normalized ≥100 with zero per-unit regressions (esp. watch
DepthBuffer3D for the one 100% dup).

**Class B — 9 SAME-unit collisions (NOT recoverable via reveal).** The name already
exists at another addr in the SAME unit obj → naming the reveal makes objdiff
pairing genuinely ambiguous (the -138 footgun). KEEP REJECTED. Members: Console
`DataContinue`/`Step`, LightPreset 2×, HamCamTransform `ObjVector=`, SpotlightDrawer
`SpotDrawParams=`, Memcard `NoDeviceChosenMsg` ctor, PropKeys `AtFrame`, **Player
`SetQuarantined`**.

### Secondary discovery (separate lever, NOT this item)
Console already has THREE addrs mapped to `?DataContinue@@...` reading 17.9% / 0% /
0% — an existing 3-way same-unit ambiguity that is itself suppressing matches
(only one can pair). This pre-existing mis-map is a candidate for the
sliver-relocation / ICF-dedup technique. Flagged as discovered_frontier.

## Self-contained landing recipe (one worktree, independent vs main@8314)
```
scripts/setup_worktree.sh /tmp/wt-reveal50 reveal-generic-50
# in worktree:
python3 tools/reveal_sweep.py --emit-fragment /tmp/frag.json     # regenerate on worktree base
python3 tools/safe_name_merge.py --gate /tmp/frag.json --out /tmp/safe.json
#   merge /tmp/safe.json (the 20) into scripts/target_symbol_map.json
#   THEN attempt the 18 dead-blob cross-unit adds (manual, bypass rule-2)
rm -f build/45410914/target_symbol_renames.stamp && touch config/45410914/config.yml && NINJA_JOBS=8 tools/fresh_report.sh
#   re-run once for splits-only FP; keep ONLY addrs landing ≥100, zero per-unit regressions
```
No source edits needed — reveals are already byte-exact, just unpaired.

**attribution_risk = true** (pin/naming relocation: these add names to pinned-unit
slivers and the cross-unit set touches names shared with auto_03 blobs).
