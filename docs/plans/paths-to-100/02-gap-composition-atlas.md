# Gap composition atlas — what exactly is the unmatched 91%

> Status: DRAFT-RFC | Date: 2026-07-08 | Author: Claude Opus (paths-to-100 wave) | Theme: strategy

## Summary

The unmatched 91.31% of code (10,111,452 of 11,074,108 bytes) is not one wall
but five distinct buckets with wildly different matchability. **72% of all code
is unpinned `auto_` .text** the tools have never even attempted; the *pinned*
frontier (3.13 MB, 773 units, already 30.7% matched) is where every past win
landed. This doc is the denominator-of-record: it quantifies each bucket, tags
its matchability class, and routes it to the sibling RFC that attacks it.

## Motivation

Every RFC in this set makes a claim about "the remainder." Without a single
audited breakdown, those claims drift — one plan assumes the gap is mostly
divergent bodies, another assumes it's mostly unidentified spans, and they
double-count or contradict. This atlas fixes the numbers once, from
`build/45410914/report.json` @ `a1312de`, so `03-master-sequencing-roadmap.md`
can sequence against a real denominator and `18-metrics-and-dashboard.md` can
account vein ROI against a fixed baseline.

## Current state (verified)

All figures below are from `build/45410914/report.json` (objdiff's canonical
output) at main `a1312de`, cross-checked with `tools/fuzzy_progress.py`. The
python aggregation queries are embedded so a cold agent can reproduce every
number.

### Whole-binary top line

```
STRICT   functions   11,240 / 65,619   (17.13%)
STRICT   code       962,656 / 11,074,108 B   (8.69%)
         => UNMATCHED code = 10,111,452 B  (91.31%)
total_data 4,118,360 B  (matched 16 B — effectively 0%)
total_units 2,456
```

Query:
```python
import json
r = json.load(open('build/45410914/report.json'))
print(r['measures'])   # total_code, matched_code, matched_functions, ...
```

### The pinned/unpinned split — the single most important cut

A "unit" in report.json is either a **pinned TU** (has a `source_path` and a
`splits.txt` `.text` range → objdiff diffs it byte-for-byte) or an **`auto_`
span** (`metadata.auto_generated == true` or `name` starts with `auto_` — a raw
address range dtk carved out that no source is wired to). This is the load-bearing
distinction.

```python
auto   = [u for u in r['units'] if u.get('metadata',{}).get('auto_generated') or u['name'].startswith('auto_')]
pinned = [u for u in r['units'] if u not in auto]
def tot(us,k): return sum(int(u['measures'].get(k,'0')) for u in us)
# PINNED: 773 units, total_code 3,132,020, matched 962,656 (30.74%), 23,494 fns, 11,240 matched
# AUTO:  1,683 units, total_code 7,942,088, matched 0 (0.00%),        42,125 fns,      0 matched
```

Result:

| Class  | units | total_code (B) | matched (B) | matched% | total fns | matched fns |
|--------|------:|---------------:|------------:|---------:|----------:|------------:|
| PINNED |   773 |      3,132,020 |     962,656 |  **30.74%** |   23,494 |     11,240 |
| AUTO   | 1,683 |      7,942,088 |           0 |    0.00%  |   42,125 |          0 |

**Scout-claim adjudication:**
- ✅ "pinned units 773 at 31% matched" — **correct** (773 units, 30.74%).
- ⚠️ "7.88MB / 71% of .text is unpinned auto_ spans" — **close but slightly
  low.** Verified: auto code = **7,942,088 B = 7.94 MB = 71.72%** of the 11.07 MB
  total code. Use 7.94 MB / 71.7%.
- ✅ "Bink = 65KB" — **correct** (Bink `.text` = 65,292 B; see middleware below).

### The five buckets (definitive)

Every unmatched byte falls into exactly one bucket. Buckets 1–2 are inside
pinned units; buckets 3–5 are the auto/data remainder.

```python
# Bucket sizing script (run against report.json):
pinned_unm = sum(int(f.get('size',0) or 0)
                 for u in pinned for f in u['functions']
                 if f.get('fuzzy_match_percent') != 100.0)
# => 2,169,364 B across 12,307 pinned-but-unmatched functions
```

| # | Bucket | Bytes | % of unmatched 10.11 MB | Matchability class |
|---|--------|------:|---------:|--------------------|
| 1 | Pinned, real-body near/mid-miss (>44 B) | 1,925,408 | 19.0% | oracle-exists / portable-source-exists |
| 2 | Pinned boilerplate (funclets + ≤44 B stubs) | 243,956 | 2.4% | boilerplate |
| 3 | Unpinned `auto_` .text (game+engine+mw+CRT) | 7,942,088 | 78.5% | mixed — see 3a–3d |
| 4 | Bink / middleware sections | (65,292 .text; ~59 KB data) | ~0.6% (.text) | opaque (no source) |
| 5 | Data sections (.rdata/.data/.idata) | 4,118,360 (own denom) | n/a (not in code denom) | anchoring signal, not a match target |

Buckets 1+2 sum to 2,169,364 B = the pinned-unmatched total; bucket 3 is 7.94 MB;
1+2+3 = 10,111,452 = the whole unmatched code figure. Consistency confirmed.

#### Bucket 1 — Pinned real-body misses (1.93 MB, oracle-exists)

7,116 pinned functions >44 B are below 100%. This is the **proven grind
frontier** — every landed win (grind loop +22 @ `3342b30`/`a1312de`, bodyport
waves, CharClipGroup micro-pin @ `d696b52`, Waypoint ObjVector flip @ `d3c6e4f`)
came from here. Top-20 pinned units by unmatched bytes:

```
  57400 B  BandDirector          fn%=52.1  src/system/bandobj/BandDirector.cpp
  41260 B  VocalTrack            fn%=41.1  src/band3/bandtrack/VocalTrack.cpp
  34568 B  band3/game/VocalPlayer fn%=28.1 src/band3/game/VocalPlayer.cpp
  33952 B  LightPreset           fn%=63.8  src/system/world/LightPreset.cpp
  30456 B  BandCharacter         fn%=48.8  src/system/bandobj/BandCharacter.cpp
  28088 B  DirLoader             fn%=41.5  src/system/obj/DirLoader.cpp
  27572 B  system/rndobj/Utl     fn%=51.5  src/system/rndobj/Utl.cpp
  26664 B  BandCamShot           fn%=43.0  src/system/bandobj/BandCamShot.cpp
  26640 B  Dir                   fn%=23.6  src/system/world/Dir.cpp
  26456 B  BandWardrobe          fn%=23.0  src/system/bandobj/BandWardrobe.cpp
  26212 B  band3/game/Player     fn%=34.3  src/band3/game/Player.cpp
  24932 B  system/rndobj/Rnd     fn%=43.8  src/system/rndobj/Rnd.cpp
  24768 B  EventTrigger          fn%=59.2  src/system/rndobj/EventTrigger.cpp
  24596 B  PhysicsVolume         fn%=26.9  src/system/world/PhysicsVolume.cpp
  23732 B  Spotlight             fn%=66.7  src/system/world/Spotlight.cpp
  22920 B  band3/bandtrack/GemManager fn%=29.1 src/band3/bandtrack/GemManager.cpp
  22464 B  FileMerger            fn%=51.1  src/system/char/FileMerger.cpp
  21436 B  AsyncFileHolmes       fn%= 3.3  src/system/os/AsyncFileHolmes.cpp
  21408 B  CameraShot            fn%=63.3  src/system/world/CameraShot.cpp
  21264 B  MeshAnim              fn%=31.4  src/system/rndobj/MeshAnim.cpp
```

```python
rows=[]
for u in pinned:
    m=u['measures']; tc=int(m['total_code']); mc=int(m['matched_code'])
    rows.append((tc-mc, u['name'], m.get('matched_functions_percent',0),
                 u['metadata'].get('source_path','')))
rows.sort(reverse=True); rows[:20]
```

Two sub-flavours within bucket 1, mapped to sibling RFCs:
- **body-divergent** (identity correct, MWCC-source→MSVC codegen stalls the last
  few %) → attacked by `13-codegen-idiom-library.md`, `11-permuter-farm.md`,
  `12-grind-fleet-v2.md`. This is the confirmed BODY-DIVERGENCE wall
  (`docs/decomp/research/2026-06-24-pivot-bodyport-classb-results.md`).
- **systematic-pattern** (one struct-offset / local-static-Symbol / save-rev fix
  clears many fns) → `14-systematic-symbol-sweeps.md`.

Note the WIRED-fuzzy view: `tools/fuzzy_progress.py` reports the WIRED set (the
13,584 attempted fns) at **94.602%** fuzzy — i.e. bucket-1 functions are on
average *close*. The completion staircase (`>=95: 12,743 | >=90: 13,021`) means
~1,503 fns sit in [95,100) — the ripest near-miss band for `12-grind-fleet-v2.md`.

#### Bucket 2 — Pinned boilerplate (244 KB, boilerplate)

```
funclet/unwind ($ in name):  729 fns   111,476 B
small ≤44 B (non-funclet):  4,462 fns   132,480 B
??_9 vcall thunks:              0 fns         0 B  (none in pinned units)
                            ----------  ---------
                            5,191 fns   243,956 B
```

These are EH funclets (`?...$...`), tiny getters/setters, and dtor/ctor
fragments. Most are gated not by codegen but by the **guard-thunk wall**: retail
emits `??__E`/`??__F`/`$S` guard thunks our objs don't pair (see CLAUDE.md "Obj
patchers"). The obj symbol patchers (`anon_ns`, `dynamic_init`, `guard`,
`bool_mangle`, `atexit_scope`) already neutralize the *naming* half; the residual
is real per-funclet layout. **Low ROI per byte** (avg 47 B/fn) but high *count*
— relevant to `14-systematic-symbol-sweeps.md` (one-pattern-many-functions) and
`17-unicorn-equivalence-lane.md` (many are behaviorally trivial → equivalence
credit even without byte-match).

#### Bucket 3 — Unpinned `auto_` .text (7.94 MB, MIXED — the real denominator)

923 auto units carry `.text`. Their 42,125 functions split:

```
named symbols (reachable, pulled in, uncredited):  7,086 fns  2,627,100 B
anonymous fn_<addr> (never identified):           35,039 fns  5,314,988 B
```

```python
autotext=[u for u in auto if any(s['name']=='.text' for s in u.get('sections',[]))]
named=sum(int(f['size'] or 0) for u in autotext for f in u.get('functions',[])
          if not f['name'].startswith('fn_'))
anon =sum(int(f['size'] or 0) for u in autotext for f in u.get('functions',[])
          if f['name'].startswith('fn_'))
# named 2,627,100 B ; anon 5,314,988 B
```

The 7,086 **named** functions in auto spans are a paradox worth flagging: they
carry MSVC-mangled names (so identity is *known*) but live outside any pinned
`splits.txt` range, so objdiff never scores them. Wiring their parent TU into
`splits.txt` is exactly `04-pinning-at-scale.md`'s job — a large chunk of the
2.63 MB is "already-identified, just-not-pinned" and should be near-free credit.

The 5.31 MB of **anonymous** `fn_<addr>` is the hard core: functions with no
name and (per the two CONFIRMED IDENTIFICATION walls) no reliable locator —
`topo_locate` died at precision 0.13
(`docs/decomp/research/2026-06-30-topo-locator-design.md`), Ghidra BSim
seed-propagation at 0.24. Composition estimate below.

**Composition estimate of bucket 3** (game vs engine vs middleware vs CRT). We do
NOT have a per-`fn_<addr>` category label (report.json only categorizes pinned
units). Two independent signals bound it:

1. **`autoid.json`** (`tools/fingerprint_match.py autoid`, regenerated
   2026-06-23): 511 large auto spans string-matched to a source file, covering
   551,260 B. Of those: **320 → `../rb3` (rb3-Wii game oracle)**, **191 →
   `../dc3-decomp` (engine oracle)**. This is a *biased* sample (only big,
   string-rich fns get matched) but says the identifiable auto remainder skews
   ~63% game / ~37% engine by count.
2. **Category totals** (pinned only, so a lower bound on each layer's true size):
   game 732,808 B tracked, engine 2,379,848 B, network 19,348 B. Retail RB3's
   full game layer is far larger than the 154 pinned game units expose — most of
   the RB3-specific code is *in* bucket 3, unpinned.

**[UNVERIFIED] composition split of the 7.94 MB** (no ground-truth labels exist
for `fn_<addr>`; treat as an informed estimate, not a measurement):
- Game (band3/network RB3-specific): ~2.5–3.5 MB — oracle = `../rb3` (rb3-Wii),
  but MWCC bodies diverge → `09-sibling-title-oracles.md`, `13-codegen-idiom-library.md`.
- Engine (Milo, `src/system`): ~2.5–3.5 MB — oracle = `../dc3-decomp` (same
  compiler, byte-exact plausible) → `06-oracle-refresh-loops.md`,
  `04-pinning-at-scale.md`.
- Middleware in `.text` (RAD Bink is bucket 4; Quazal is in the 19 KB network
  category; STLport/CRT templates scattered throughout) → `10-middleware-and-denominator.md`.
- The identification wall itself (which fns can even be located) →
  `07-icf-constraint-solver.md`, `08-ml-embedding-triage.md`, `05-data-xref-anchoring.md`.

This estimate's uncertainty is the single biggest open question in the whole RFC
set (see Open questions). `03-master-sequencing-roadmap.md` must not treat it as
firm.

#### Bucket 4 — Bink / middleware sections (~65 KB .text, opaque)

RAD Game Tools Bink video, statically linked, in its own PE sections:

```
auto_04 ... BINK      .text  65,292 B  (code)
auto_02 ... BINKCONS         10,528 B  (data)
auto_05 ... BINKBSS          17,312 B  (bss/data)
auto_07 ... BINKDATA         15,700 B  (data)
auto_08 ... .XBMOVIE             12 B
```

Confirms scout's "Bink = 65KB". **Opaque**: proprietary, no source oracle,
hand-written PPC asm in places. `10-middleware-and-denominator.md` argues for
*excluding* this from the honest denominator rather than matching it. It is
~0.65% of code — not worth pursuing byte-for-byte.

#### Bucket 5 — Data sections (4.12 MB, anchoring signal not a match target)

```
.rdata  2,028,144 B   (vtables, RTTI, string pools, jump tables)
.data   2,045,420 B   (static initializers, globals)
.idata      1,228 B   (import table)
+ Bink data sections (~59 KB, in bucket 4)
total_data 4,118,360 B ; matched_data 16 B (0.0004%)
```

Data is measured on its **own denominator** (`total_data`), NOT the 11.07 MB code
figure — matching it does not move the code %. Its value is as an
**identification signal**: `.rdata` vtables/RTTI anchor which class a `fn_<addr>`
belongs to (`05-data-xref-anchoring.md`), and `19-shiftable-relink-milestone.md`
needs data pinned for a bootable relink. Do not chase data % as a headline metric.

## Proposal

This document is **not** an action plan — it is the denominator-of-record. Its
deliverable is the atlas above plus three standing artifacts:

1. **A regeneration script** — add `tools/gap_atlas.py` [UNVERIFIED: does not yet
   exist] that emits the five-bucket table from `report.json` on demand, so the
   atlas can be refreshed after every landing wave and diffed against this
   baseline. Until it exists, the embedded python queries in "Current state" are
   the reproduction recipe.
2. **A routing table** (below) binding each bucket to the sibling RFC that owns
   it, so no bucket is orphaned and none is double-claimed.
3. **A baseline snapshot** — the numbers here are frozen at `a1312de` /
   2026-07-08. `18-metrics-and-dashboard.md` accounts all future ROI as deltas
   against this snapshot.

### Routing table (bucket → owning RFC)

| Bucket | Bytes | Class | Primary RFC | Supporting RFCs |
|--------|------:|-------|-------------|-----------------|
| 1 body-divergent (pinned >44B) | ~1.5 MB (subset of 1.93) | oracle-exists | `13-codegen-idiom-library` | `11-permuter-farm`, `12-grind-fleet-v2`, `15-ghidra-guided-synthesis` |
| 1 systematic-pattern | ~0.4 MB (subset) | portable-source | `14-systematic-symbol-sweeps` | `06-oracle-refresh-loops` |
| 2 boilerplate | 244 KB | boilerplate | `14-systematic-symbol-sweeps` | `17-unicorn-equivalence-lane` |
| 3 named-but-unpinned | 2.63 MB | oracle-exists | `04-pinning-at-scale` | `06-oracle-refresh-loops` |
| 3 anonymous fn_ | 5.31 MB | opaque→identifiable | `05-data-xref-anchoring`, `07-icf-constraint-solver` | `08-ml-embedding-triage`, `09-sibling-title-oracles`, `15-ghidra-guided-synthesis` |
| 4 Bink/middleware | 65 KB code | opaque | `10-middleware-and-denominator` | — |
| 5 data sections | 4.12 MB (own denom) | anchoring signal | `05-data-xref-anchoring` | `19-shiftable-relink-milestone` |

The endgame taxonomy in `01-endgame-definitions.md` decides which of these
buckets even *count* toward "100%"; `03-master-sequencing-roadmap.md` orders the
attack; `18-metrics-and-dashboard.md` scores it.

## Alternatives considered

- **Byte-histogram-only atlas (no matchability class).** Rejected: raw byte
  counts without a class tag are what caused the double-counting this doc exists
  to fix. The class column is the point.
- **Per-`fn_<addr>` ML classification to firm up bucket-3 composition.** Deferred
  to `08-ml-embedding-triage.md`; embedding-based labels are exactly the
  triage-amplifier that RFC proposes, and re-deriving them here would duplicate it.
- **Use whole-binary fuzzy% (11.9%) as the headline instead of STRICT 8.69%.**
  Rejected as the *denominator* — fuzzy is a progress signal, not the bar
  (`tools/fuzzy_progress.py` says so explicitly). The atlas is denominated in
  STRICT bytes; fuzzy is cited only to characterize how *close* bucket 1 is.

## Effort & expected value

This RFC is cheap (~1 day: aggregation + `tools/gap_atlas.py`) and its EV is
**indirect** — it does not itself match a single function. Its value is
preventing the other 19 RFCs from mis-estimating. Anchored to past results:

- The largest single proven vein was **class-A TU-pure span harvest: +403 fns in
  one session** — and it is now EXHAUSTED (wave-8 +0). That vein lived entirely
  in bucket 3's *named-but-unpinned* sub-bucket. Its exhaustion is precisely why
  bucket-3 anonymous (5.31 MB) is the new frontier and why `04-pinning-at-scale`
  must find fresh spans, not re-harvest.
- The grind loop lands **~+22 fns/session** from bucket 1 (`3342b30`/`a1312de`).
  Bucket 1 has 7,116 real-body targets; at that rate it is a long tail, which is
  why `11`/`12`/`13` aim to raise the per-session rate.
- Bucket-3 named-but-unpinned (2.63 MB, 7,086 fns already-identified) is the
  highest-EV *untapped* pool: if even 40% pin cleanly that is ~2,800 fns — a
  multiple of every prior session. But that is `04-pinning-at-scale`'s claim to
  prove, not this doc's.

## Risks & failure modes

- **The [UNVERIFIED] bucket-3 game/engine/middleware split is wrong.** It rests
  on a 511-entry biased `autoid.json` sample. If the true split is (say) 70%
  middleware/CRT rather than ~mostly game+engine, the reachable ceiling is far
  lower. Mitigation: `08-ml-embedding-triage` / `05-data-xref-anchoring` should
  produce a real labeled census and *replace* the estimate here.
- **Staleness.** These numbers are frozen at `a1312de`. Every landing wave shifts
  them. If agents cite this doc months later without re-running the queries, they
  will sequence against a dead baseline. Mitigation: `tools/gap_atlas.py` +
  re-snapshot on each wave.
- **Double-counting across the pinned/auto boundary.** The 7,086 named auto fns
  could be mistaken for bucket-1 near-misses. They are NOT — they score 0 (never
  attempted), not near-100. Keep the pinned/auto cut primary.

## Kill criteria

- If a labeled census (from `05`/`08`) shows bucket 3 is >60% opaque
  middleware/CRT with no oracle, then this atlas's "~2.5–3.5 MB game / ~2.5–3.5
  MB engine" estimate is refuted and the whole RFC set's reachable ceiling must
  be revised down in `01-endgame-definitions.md`. That is a *success* for the
  atlas (it exposed a false hope), even though it kills the estimate.
- If `tools/gap_atlas.py` cannot reproduce these five bucket totals to within
  rounding from a clean `report.json`, the aggregation logic here is wrong and
  must not be cited until fixed.

## Open questions

1. **What is the real category split of the 5.31 MB anonymous fn_ bucket?** The
   single highest-leverage unknown. Owned by `08-ml-embedding-triage` /
   `05-data-xref-anchoring`. Everything downstream (ceiling, sequencing) hinges
   on it.
2. **How much of the 2.63 MB named-but-unpinned pins cleanly?** Owned by
   `04-pinning-at-scale`. If most pins near-free, this reorders the roadmap.
3. **Does the endgame count bucket 4 (Bink) and bucket 5 (data)?** Owned by
   `01-endgame-definitions` / `10-middleware-and-denominator`. Determines whether
   the honest denominator is 11.07 MB or 11.07 − 0.065 (Bink) − ... MB.
4. **Are the 7,086 named auto fns unique, or ICF-folded duplicates of pinned
   ones?** ICF folding (CLAUDE.md, `icf_alias_check` honesty gate) could inflate
   this. Needs `mcp__orchestrator__lookup_merged_symbol` spot-checks before
   `04` treats them as free credit.

## References

- `build/45410914/report.json` — objdiff canonical output; source of every number here (@ `a1312de`).
- `tools/fuzzy_progress.py` — STRICT/FUZZY/staircase; WIRED 94.602%, sub-goal splits.
- `tools/fingerprint_match.py` — `autoid` subcommand → `autoid.json` (511 entries, 320 rb3-Wii / 191 dc3).
- `autoid.json`, `fingerprints.json` — string-based function→source proposals (regenerable, 2026-06-23).
- `config/45410914/splits.txt` — 3,870 lines; pins `.text` ranges that promote auto spans to pinned units.
- `config/45410914/objects.json` — declares which `.cpp` compile + match status.
- `docs/decomp/research/2026-06-30-topo-locator-design.md` — IDENTIFICATION wall (topo_locate precision 0.13).
- `docs/decomp/research/2026-06-24-pivot-bodyport-classb-results.md` — BODY-DIVERGENCE wall (BandProfile 0/64).
- `docs/INDEX.md` — audited master doc index (check for stale-banner docs before trusting current-state claims).
- Sibling RFCs: `01-endgame-definitions`, `03-master-sequencing-roadmap`, `04-pinning-at-scale`,
  `05-data-xref-anchoring`, `06-oracle-refresh-loops`, `07-icf-constraint-solver`, `08-ml-embedding-triage`,
  `09-sibling-title-oracles`, `10-middleware-and-denominator`, `11-permuter-farm`, `12-grind-fleet-v2`,
  `13-codegen-idiom-library`, `14-systematic-symbol-sweeps`, `15-ghidra-guided-synthesis`,
  `17-unicorn-equivalence-lane`, `18-metrics-and-dashboard`, `19-shiftable-relink-milestone`.
- Landmark commits: `3342b30`/`a1312de` (grind +22), `d696b52` (CharClipGroup micro-pin), `d3c6e4f` (Waypoint ObjVector flip).
