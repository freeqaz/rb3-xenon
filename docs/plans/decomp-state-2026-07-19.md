# rb3-xenon decomp — state & live veins (2026-07-19)

**Current: 18,621 strict-matched functions** (`build/45410914/report.json`,
`match_percent_normalized == 100.0` exactly). Denominator is the whole TU5 XEX
(~69k functions). HEAD `d0864f7c`.

## Recent arc

| date | strict | delta | driver |
|---|---|---|---|
| 2026-07-17/18 mega-run | 17,445 | +2,081 | identification stack (+1,871 names), lane-B near-pair, naming wave, BandSwatch, struct leads |
| 2026-07-18 review | 17,445 | — | 3 Opus scouts ranked pools; `docs/plans/review-2026-07-18-next-focus.md` |
| 2026-07-19 body-port/recarve/scatter/id-flywheel | **18,621** | **+1,176** | the "mapped-but-0%" pool cracked open (see below) |

The +952 came from **one discovery**: the "mapped-but-0%" pool (functions with
real mangled names stuck at 0%) is overwhelmingly **COMDAT-scatter / TU-composition
drift**, NOT missing source. Retail MSVC/X360 (`/O1`, no LTCG) emits each function
into its own COMDAT and the linker scatters them across `.text`; dtk carves the
retail binary into per-source-file target objs by address range, so a function
whose COMDAT landed in unit X's span is attributed to X even though its source
lives in unit Y — and *our* obj for Y is the one that emits the matching bytes,
under a name objdiff never pairs into X.

### The three fix shapes (all landed, all regression-clean)

1. **Owner-TU whole-file include** — append `#include "<owner>.cpp"` to the
   span-owning `.cpp` so its obj emits the scattered COMDATs. INIT_REVS `gRev`
   collisions on double-include → byte-neutral `#define gRev gRev_<Owner>`.
   Landed: bp3 (+26), bp2r (+84), scatter-sweep w1 (+174). Idiom at HEAD in
   `TDStretch.cpp`, `MeshAnim.cpp`, `Console.cpp`.
2. **Retail-arity body duplication under `#ifndef HX_NATIVE`** — when whole-file
   include collides (statics/anon-ns/PROPSYNC barewords), copy just the needed
   bodies into the span owner with extern decls; native keeps canonical defs.
   Landed: bp1 (+36). Idiom in `Debug.cpp`, `DirLoader.cpp`, `MemHeap.cpp`.
3. **Splits gap-fill recarve** — when the auto blob is the missing *middle* of an
   already-pinned TU, add one gap `.text` range + reloc-masked byte-identity map
   entries (`tu5_reloc_masked_correlate.py`); ICF-twin MULTI groups resolve by
   order-preserving assignment; funclets cascade free. Landed: rc1 (+130,
   AccomplishmentPanel), rc3 (+14, TrackWatcherImpl).

**Instrument:** `scripts/harvest/comdat_scatter_scan.py` (~0.9s, re-runnable)
scans the COFF symbol tables of all ~836 compiled objs and splits every named-0%
function into **SCATTER** (emitted by another wired obj → owner-include/dup
fixable) vs **UNWIRED** (no wired obj emits it → gameport pool).

**Kill test before recarving any auto blob:** if the span's pre-mapped names are
emitted by *no* wired obj, the blob is a COMDAT catch-all from unwired classes —
a gameport target, not an attribution gap. (rc2/SongSort `0x826DD570` was
correctly skipped this way: SkillsAwardList / CampaignEra* / a NavListSortMgr
SongSortMgr redesign that matches DC3, not our older port.)

## Live veins (ranked by EV)

### 1. COMDAT-scatter sweep — TOP LIVE VEIN
After 3 sweep waves (+661) the scanner reports **275 SCATTER candidates /
218 proposals** still open — now net-0 unpairable owners + genuinely divergent
bodies; the mechanical scatter vein is **nearly drained**. Cross-dialect walls
were unlocked by the wave-3 byte-neutral shim `obj/dialect_object_{push,pop}.h`.

The remaining 218 proposals are dominated by **net-0 unpairable owners** (the
scattered body diverges from ours, so the include exposes a fuzzy near-miss, not
a strict flip) and a handful of **body-dup** cases that whole-file include can't
take (CameraShot←Flow, PropAnim←PropKeys, CharBonesMeshes←GemManager — need the
`#ifndef HX_NATIVE` body-dup lever, not include). The **reverse-dialect vein**
(Object-dialect consumer ← ObjMacros-dialect owner, 21 candidates) is EMPTY
(all net-0/BUILDFAIL — do not re-hunt). Method is mechanical + gated (per-unit
whole-binary A/B, auto-revert on loss); **re-run the scanner between waves** —
fixing one owner unblocks chained proposals (w1's MidiSynth←PropSync only
appeared after PropSync←Dir landed). Verdict: **the high-yield mechanical
scatter vein is essentially worked out; what's left is body-port-grade.**

### 2. UNWIRED gameport pool — 327 fns / 138 units
Functions no wired obj emits. Two sub-classes:
- **Engine, oracle-backed (portable):** rnddx9/CubeTex (8 Dx* ctors, DC3 oracle),
  Anim (7), Lit_NG, rnddx9/Rnd, Sequence — DC3 near-verbatim. These are true
  body-ports / TU wirings, ~medium cost.
- **Oracle-poor (defer, hard):** FFT (10 fns, VMX128 hand-asm — DC3's FFT unit is
  only 23%), System/LEAPCORE (32, no oracle), Mic + ExternalMic (25, Xbox voice),
  Compress/XGRAPHICS (10, shader-microcode), GranularSynth/SpectralAnalysis/
  PeakDetector (DSP hand code), rtti/osfinfo (CRT). Lowest ROI — leave for last.
- **Game (band3):** 16 units incl. TrainerPanel (5), DataArraySongInfo (11) —
  rb3-Wii oracle, gameport cost.

### 3. Exposed near-misses (fuzzy → strict fodder)
Pairing the scattered bodies revealed genuine near-misses previously hidden as
0% stubs: **NgRnd::UpdateOverlay 94.2** (stripped `spotlights %d` debug print +
`gNgStats` +4 offset drift — clean `#ifdef` gate candidate), RndShaderMgr::
UpdateCache 99.8 / Terminate 99.9, MakeWorldSphere 97.2, enableAAFilter 99.5
(RateTransposer +16B member), RingBuffer::Write 91.4, MemTracker::StopLog 77
(mLog −8). Small, per-fn, cheap.

### 4. Remaining recarve gap-fills
Mid-address auto blobs from the 2026-07-18 review not yet carved:
**0x8234FCEC** DataArray/ObjectDir (94 real, cold map — Stage-C identity work),
**0x82560660** UI-message run (64 real). Apply the kill test first. The
Accomplishment/TrackWatcher blobs are done; SongSort is UNWIRED (vein #2).

### 5. Deep grinds (banked, lower EV)
- **TrackWatcherImpl 121 flat-0% bodies** — beatmatch gameport (oracle
  `../rb3/src/system/beatmatch/TrackWatcherImpl.cpp`, largest 4488B).
- **Grouped-globals wall** — retail addresses `gNumHeaps`/`gHeaps`,
  `gSystemMs`/`gSystemFrac` via a shared base+offset (`lbl_82E06BA8`/`82CC999C`);
  ours are independent globals → walls MemFindAddrHeap 85, SystemMs 90. Needs a
  data-layout mechanism, not source edits.
- **DxRnd::UpdateScalerParams** (0x82739948) — paired at 0% since the vtable fix,
  genuine body-port lead.
- **BandCharacter −4 container compaction** (cr6), **BandCamShot vbase-MI
  reconstruction** (documented wall, pad-probe-killed the tempting +0x80 tail).

### 6. Identification round-4 — DONE +170 (`39038c09`), FLYWHEEL CONFIRMED
The scatter/recarve campaign's +250 names & 3 new pinned clusters cleared the
round-3 fixed-point gate. Re-running `scripts/harvest/TU5_SCANNER_STACK.md`
yielded **+170 strict** at ~0.157 flips/name (5x the collapsed 0.031 rate),
fixed point in 3 rounds, 6/6 Ghidra spot-checks. **Key insight: the scatter
vein FEEDS the identifier** — every owner-TU-include body flip creates a fresh
clean byte-identity pair the scanner then cracks. Round-5 not warranted until
+~1,000 more names. This coupling means future body-port waves should be
followed by an identification re-run.

## Dead / banned (do NOT re-hunt)
Permuter (user directive — low yield, grinds the box); ≥99 fixwave round-2
(rejected — 80% funclet mirage, ~20-30 fixable, no cascade); lane-B near-pair
residue (drained); A_TOOLING ICF fold mirage; pad-probe deferred struct walls
(drained); local-static mechanical wave; the 3 scatter-sweep w1 lossy candidates
(CameraShot←Flow, PropAnim←PropKeys/AmbientOcclusion, CharBonesMeshes←GemManager
— need body-dup, not whole-file include).

## Method (stable)
Fable coordinator delegates to Opus agents in `scripts/setup_worktree.sh`
worktrees under `~/tmp`; coordinator independently re-verifies every diff with a
fresh clean-worktree whole-binary A/B (strict set keyed `(unit, name)`, LOST must
be empty) before a path-limited commit on main; `touch config/45410914/config.yml`
before any A/B leg that changed splits/map (renamer re-split trap). Scoreboard:
`docs/plans/tu5-p5-progress.md`. Memory: `project_comdat_scatter_lever_2026-07-19`.
