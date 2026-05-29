# Execution Schedule — dependency-aware matching roadmap

**Date:** 2026-05-28. **Supersedes the *ordering* of:** `docs/plans/path-to-100.md`
(P1-P7 phases) and `docs/plans/exploratory-techniques.md` (§3 next-steps).
**Does NOT supersede** their *content* — this doc re-orders and re-scopes those
phases around three blockers/levers that landed THIS session, and designs the one
piece neither doc specified: a unified oracle→pin ranker.

Per `feedback_plans_with_refs.md` every item carries file/line/doc refs for cold
pickup. Per `feedback_verify_assumptions.md` every load-bearing count below was
recomputed from the live artifacts this session (citations inline).

## Verified baseline (this session, from `build/45410914/report.json:measures`)

- **394 matched / 66,165 functions** (0.595%); 77,024 / 11,774,312 code bytes (0.654%);
  whole-binary fuzzy 1.43%.
- **Milo Engine Code:** 380/4,323 fns; **10.96% matched / 23.74% fuzzy** (the gap P3
  targets). **Game Code:** 14/322 fns; 49.34% matched-code / 3.20% fuzzy.
- Build green at HEAD `1f1c7d3`.
- `config/45410914/objects.json`: engine 186, band3 48 (43 meta_band), hamobj 1,
  main 3, xdk 1. `splits.txt`: **402 `.cpp` blocks pinned**.

## What changed since path-to-100 was written (the re-planning inputs)

1. **Permuter is wired.** The permuter is the installed **`decomp_synth`** package
   (consumed via rb3-xenon's `venv`; `ast_queries`, `beam_search`, … importable).
   This *removes* the hard blocker in `feedback_fuzzy_gap_needs_permuter.md` that
   made path-to-100 P3 "over-optimistic." The `permute` skill should now function.
2. **jeff asm mis-nest fix in flight** (isolated worktree — 3 locked agent worktrees
   exist: `git worktree list`). Quantified blast radius **this session**: **438
   mis-nests across 100 named (non-auto) units** (scan below). Top: BandDirector 25,
   TexBlender 21, DepthBuffer3D 16, CharClip 14, Shader/MidiParser/LightPreset 13.
3. **Three new oracles shipped as `fingerprint_match.py` subcommands** and their
   output files exist (verified): `unified_id_callgraph.json` (1,555),
   `unified_id_rtti.json` (384), `unified_id_vtable.json` (37). **All three are
   pre-deduplicated against `unified_id.json` — 0 address overlap with bindiff's
   11,539.** Union = **13,491** identified fns (**+1,952** beyond bindiff). They are
   NOT yet consumed into pins. **This is the biggest unrealized lever (§2).**
4. **band3 meta_band porting productive:** 43 meta_band TUs wired
   (`objects.json:band3`), compile-clean but +0 matches (unpinned). 172 meta_band
   `.cpp` exist in rb3-Wii (`../rb3/src/band3/meta_band/`), 41 present locally — a
   ~130-TU port backlog. Playbook grows in `meta_band-port-breaking-changes.md`.

---

## Section 1 — Dependency graph (the DAG)

```
                          ┌─────────────────────────────────────────────┐
                          │  INDEPENDENT ROOTS (can all start now)        │
                          └─────────────────────────────────────────────┘

  [A] jeff mis-nest fix          [B] unified oracle ranker      [C] permuter smoke-test
  (isolated jeff worktree,        (tools/pin_candidates.py;       (verify decomp_synth
   Rust edit, no main-tree         pure read-only data;            runs end-to-end on ONE
   write)                          no build)                       known near-miss fn)
        │                               │                               │
        │ unblocks clean pins           │ emits ranked pin wave         │ gates all P3 work
        │ for overlapping-symbol        │ (objects.json+splits.txt)     │
        │ regions (438 mis-nests)       │                               │
        ▼                               ▼                               ▼
  ┌──────────────────────────────────────────────┐        ┌────────────────────────────┐
  │ [D] PIN WAVES (serialize on main-tree SPLIT)  │        │ [E] FUZZY-GAP PERMUTER WAVES │
  │  D1: ranker wave  (194 TUs / 981 oracle fns)  │        │  (91 named units, ~98KB;     │
  │  D2: wave-5 re-pin (155 stragglers, needs A)  │        │   permuter-class reg-swaps)  │
  │  D3: band3 pin    (43 wired TUs, needs A)     │        │  E runs in WORKTREES in      │
  │  ── ALL share ONE ninja SPLIT lane ──         │        │   parallel w/ D (build-iso)  │
  └──────────────────────────────────────────────┘        └────────────────────────────┘
        │                                                            │
        │ (D1 needs B; D2/D3 need A; D1 weakly benefits from A)      │ (E needs C)
        ▼                                                            ▼
  ┌──────────────────────────────────────────────────────────────────────────────┐
  │ [F] band3 PORT+PIN waves (port from rb3-Wii → compile → ranker-pin)            │
  │     needs: A (clean pins) + B (ranker for address) + meta_band playbook        │
  └──────────────────────────────────────────────────────────────────────────────┘

  PARALLEL, NON-BLOCKING (own lane, never touches matching SPLIT):
  [G] native-port track (native/ cmake)   [H] String 8→12 layout investigation
                                            (RISKY; gated, single-fn-measured)
```

**Blocking edges (what must land first):**

| Edge | Meaning | Evidence |
|---|---|---|
| A → D2 | jeff fix unblocks the ~30 wave-5 stragglers dropped for "start/end lands in overlapping symbol" | `wave5-session-2026-05-28.md:38-58` |
| A → D3 | band3 TUs can't pin cleanly until mis-nest gone (BandDirector alone has 25 mis-nests) | mis-nest scan §1.1; `meta_band-port-breaking-changes.md:245-268` |
| B → D1 | the ranker IS the producer of the next pin wave | §2 |
| C → E | permuter must run or fuzzy-gap work flips 0 fns (proven) | `feedback_fuzzy_gap_needs_permuter.md` |
| A → (all future pins, weakly) | mis-nest corrupts COMDAT section assignment → `<illegal>` targets | `project_jeff_asm_misnest.md` |

**Independent / parallelizable:** A, B, C have no inter-dependencies — start all
three immediately. E (worktree-isolated builds) runs concurrently with the
D-series (main-tree SPLIT). G and H never touch the matching SPLIT lane.

**Serialization chokepoint:** D1, D2, D3 all mutate `splits.txt` + `objects.json`
+ trigger a main-tree re-SPLIT. **Only ONE agent may hold the SPLIT lane.** They
must run sequentially (or be merged into one batched wave). See §4.

### 1.1 jeff mis-nest scan (reproduce)
```bash
find build/45410914/asm -name '*.s' ! -name 'auto_*' | while read f; do
  awk '/^\.fn / { gsub(/,/,"",$2); stack[++sp]=$2 }
       /^\.endfn / { if (stack[sp] != $2) c++; sp-- }
       END { if (c>0) print c, FILENAME }' "$f"
done | sort -rn
# This session: 438 mis-nests / 100 named units. framing.s = 5 (the canonical case).
```
Note: named-unit `.s` files currently show **0** literal `<illegal>` tokens — the
mis-nest corrupts the *target* `.obj` COMDAT sections (objdiff-side), not the
source `.s`. So the fix's payoff registers as pinnable-clean regions + ticked
matches, not as fewer `<illegal>` in `asm/`.

---

## Section 2 — The unified oracle ranker (DESIGN)

**The gap:** we have 5 oracle sources but only bindiff (11,539) ever fed a pin
wave. The other 1,952 identifications (callgraph/rtti/vtable) are inert leads
lists. No tool merges them, filters to pinnable source, snaps to symbol
boundaries, and emits the next wave. **Build `tools/pin_candidates.py`.**

### 2.1 Inputs (all read-only, all verified present this session)

| File | Records | Role |
|---|---:|---|
| `unified_id.json` | 11,539 addrs | bindiff baseline; conf 0.5-1.0, `similarity`, `bindiff_src` |
| `unified_id_callgraph.json` | 1,555 | callgraph; `cg_tier` (single/multi), `cg_distinct_callers` |
| `unified_id_rtti.json` | 384 | rtti+vtable transitivity; `rtti_tier` HIGH/LOW, `rtti_class` |
| `unified_id_vtable.json` | 37 | vtable-only; subsumed by rtti, low weight |
| `autoid.json` | 537 | string-content; merged into unified_id as `autoid_src` already |
| `config/45410914/symbols.txt` | 252,677 lines | **XCOFF symbol table** — the snap oracle (NOT report.json; per `wave5-session-2026-05-28.md:90-106`) |
| `config/45410914/splits.txt` | 402 blocks | already-pinned set (skip these) |
| `src/**/*.cpp` | — | source-presence filter |

**Shared schema** (verified): every oracle record has `rb3_fn`, `rb3_addr`,
`size`, `source`, `dc3_name`, `dc3_name_demangled`, `bindiff_src`, `confidence`.
This uniformity is why a single merge pass works.

### 2.2 Algorithm

```
1. LOAD all 4 oracle JSONs → list of records. (autoid already folded into unified_id.)
2. GROUP by rb3_addr (lowercased). For each address build {source: record}.
3. CONSENSUS CONFIDENCE per address:
      base = max(record.confidence over sources)        # bindiff conf already 0.5-1.0
      multi_bonus:
        +0.15 if >=2 DISTINCT oracle sources name it AND agree on dc3_name
        -0.30 if >=2 sources DISAGREE on dc3_name        # ICF-alias review flag
      tier multipliers (from exploratory-techniques.md §1-2 measured precision):
        bindiff sim==1.0            -> S  (treat as 0.98)
        callgraph cg_tier==multi    -> A  (0.94)          [437 fns, exploratory §2]
        rtti_tier==HIGH             -> B  (0.80, real ~0.85 after ICF)  [361 fns]
        callgraph cg_tier==single   -> C  (0.94 measured but single-evidence -> 0.75)
        rtti_tier==LOW / vtable     -> C- (0.41 -> EXCLUDE from auto-pin; review file)
4. SOURCE-PRESENCE FILTER: keep only addrs whose basename(bindiff_src) exists in
   src/ (case-insensitive .cpp index). THIS IS THE GATE — a fn can only be pinned
   if its TU compiles. (6,972 of 13,491 pass; see §2.4.)
5. GROUP surviving addrs by target TU (the src/ .cpp path).
6. DROP TUs already in splits.txt (402 pinned). Leaves the NEW pin surface.
7. SNAP per TU: for the set of oracle addrs in a TU, compute the .text span
   [min(addr), max(addr)+size). Then SNAP both endpoints to XCOFF symbol
   boundaries from symbols.txt:
        - start := largest symbol-start <= min(addr)
        - end   := smallest symbol-end  >= max(addr)+size
        - WALK-BACK up to 50 symbols on overlap (jeff mis-nest creates overlapping
          fn_ at same range — wave5-session-2026-05-28.md:114). If overlapping
          symbols still straddle the endpoint, FLAG "jeff-blocked" and defer to
          post-[A].
8. EMIT per TU, ranked by (consensus-weighted oracle-fn count) desc:
        splits.txt block:   <Name>.cpp:\n    .text  start:0x.. end:0x..
        objects.json entry: "<rel>": "NonMatching"   (group: engine | band3 | hamobj)
   plus a sidecar pin_candidates.json: {rel, span, n_oracle_fns, tiers, jeff_blocked}
9. OUTPUT a review bucket (pin_candidates_review.json) for: disagreement-flagged
   addrs, rtti-LOW, vtable-only — never auto-pinned.
```

### 2.3 Output format
- `pin_candidates.json` — ranked machine-readable wave (consumed by a bulk-wire pass).
- Two appendable text blobs (`*.splits.append`, `*.objects.append`) for the
  human/agent holding the SPLIT lane to paste in tiers.
- `pin_candidates_review.json` — low-confidence + disagreement queue (manual).

### 2.4 Expected yield (verified counts this session)

- Union of 4 oracles: **13,491 unique addrs** (`+1,952` beyond bindiff 11,539).
- Multi-oracle agreement: only **24 addrs** carry ≥2 sources (22 rtti+vtable, all
  agree) — consensus bonus is rare; most lift comes from the orthogonal NEW set.
- **Source-present (pinnable) addrs: 6,972 / 13,491** (basename-in-`src/`).
- After dropping already-pinned TUs: **194 NEW-pinnable TUs covering 981
  oracle-identified functions.** (Top: `Rnd_Xbox.cpp` 42, `FlowNode.cpp` 39,
  `Main.cpp` 34, `Archive.cpp` 32, `PlatformMgr_Xbox.cpp` 31, `MidiInstrument.cpp`
  28, `DataFile.cpp` 26, `ShaderMgr.cpp`/`BinStream.cpp` 25, `MidiSynth.cpp`/
  `AccomplishmentManager.cpp` 24…)
- **Realistic Δmatches from D1:** not all 981 tick (normalized-match rejects
  reg-swaps/ICF/data-reloc per `project_function_identification.md`). Using the
  wave-1 observed ~1.7 byte-match fns/TU and discounting for the weaker
  callgraph-single tier: **+150-450 matched fns** across the 194 TUs, plus the
  rest become permuter-feed (E) and source-coverage. This roughly *doubles to
  triples* the 394 baseline and is the single highest-leverage next item.

Reproduce the yield counts:
```bash
venv/bin/python - <<'PY'
import json, os, re
recs=[]
for f in ['unified_id.json','unified_id_callgraph.json','unified_id_rtti.json','unified_id_vtable.json']:
    recs+=json.load(open(f))
src={}
for r,_,fs in os.walk('src'):
    for fn in fs:
        if fn.endswith('.cpp'): src.setdefault(fn.lower(),[]).append(os.path.join(r,fn))
pinned={os.path.basename(m.group(1)).lower() for line in open('config/45410914/splits.txt')
        if (m:=re.match(r'\s*(\S+\.cpp):',line))}
from collections import defaultdict
hits=defaultdict(set)
for rc in recs:
    b=os.path.basename(rc.get('bindiff_src') or '').lower()
    if b in src: hits[b].add(rc['rb3_addr'].lower())
unp={b for b in hits if b not in pinned}
print('present TUs',len(hits),'unpinned-new',len(unp),'oracle fns in new',sum(len(hits[b]) for b in unp))
PY
```

### 2.5 Orthogonality basis (from `exploratory-techniques.md`)
- Callgraph: **94.1% precision** (5,310/5,642 overlap), orthogonal to bindiff
  (overlap measured 9/2,735 union; this session: 0/1,555 by construction —
  pre-deduped). exploratory §1.1, §Exec-summary table.
- RTTI HIGH: **71.1% raw / ~80-85% after ICF-alias discount.** exploratory §1.3.
- Vtable-only: subsumed by rtti; 37 fns, low weight. exploratory §1.4.
- ICF-alias is the dominant disagreement source → the ranker keeps BOTH names as
  candidates, never arbitrates (exploratory §5).

---

## Section 3 — Ordered execution schedule (next ~8-12 sessions)

ROI-ordered subject to dependencies. "Lane" = main-tree SPLIT (serial) vs
worktree (parallel) vs no-build (free).

### S0 — Land the three roots (parallel, day 1)

| Item | Goal | Pre | Lane | Δ | Verify |
|---|---|---|---|---|---|
| **[A]** jeff mis-nest fix | LIFO-stack the `.fn`/`.endfn` close (or `.size sym,.-sym`) in `../jeff/src/cmd/xex.rs`; `cargo build --release` | none | **isolated jeff worktree** (private jeff copy; never the main-tree jeff) | unblocks D2/D3 + +50-200 on already-pinned units | `cargo test`; re-SPLIT in a *throwaway* rb3 worktree; `framing.s` mis-nests 5→0; `ogg_sync_init` 0%→>0% (`project_jeff_asm_misnest.md`) |
| **[B]** unified ranker | build `tools/pin_candidates.py` per §2 | none | **no build** (read-only data) | produces D1 | re-run §2.4 snippet → 194 TUs/981 fns; spot-check 5 snapped spans vs symbols.txt |
| **[C]** permuter smoke-test | run `permute` skill on ONE known near-miss (e.g. `CSHA1::Update` r29↔r30, `codegen-iteration-targets.md:130`) | none | worktree | gates E | permuter emits candidate variants & at least re-derives current % (no crash) |

A is Opus (high blast radius, one fn). B is Sonnet (~250 LOC, mechanical). C is
Explore/Sonnet (smoke-test only).

### S1 — Ranker pin wave D1 (highest leverage; needs B; benefits from A)

- **Goal:** pin the 194 NEW-pinnable TUs in confidence tiers; harvest +150-450.
- **Pre:** B done. Run AFTER A if available (clean spans); if A not yet landed,
  pin only the non-`jeff_blocked` TUs (the ranker flags them).
- **Inputs:** `pin_candidates.json`, `*.splits.append`, `*.objects.append`.
- **Mechanics:** apply Tier S/A (bindiff-sim-1.0 + callgraph-multi) first → `touch
  config/45410914/config.yml && ./tools/ninja-locked 2>&1 | tee /tmp/rb3_build.log`
  → `venv/bin/python scripts/get_progress.py`. Then Tier B (rtti-HIGH), then
  Tier C. Any TU that fails compile: comment in objects.json, keep span, move on
  (don't block the batch — `path-to-100.md:246-256`).
- **Lane:** main-tree SPLIT — **one agent only.**
- **Δ:** +150-450 (§2.4 basis). **Verify:** per-tier progress delta; a TU with
  N oracle hits yielding 0 matched ⇒ likely false/ICF cluster — unpin span, keep
  source wired.

### S2 — Wave-5 re-pin D2 (needs A)

- **Goal:** re-pin the ~155 wave-5 stragglers, esp. the ~30 dropped for overlapping
  symbols now resolved by A. Plus the Fader-API synth files once their API is
  reconciled (`wave5-session-2026-05-28.md:60-70`; RB3 `Faders.h` ≠ DC3).
- **Pre:** A landed + re-SPLIT done.
- **Inputs:** `/tmp/wave5_remaining6_batches.json` (regenerate via symbols.txt snap
  if `/tmp` cleared — `wave5-session-2026-05-28.md:135`).
- **Lane:** main-tree SPLIT (serial; conflicts with D1/D3 — schedule after D1).
- **Δ:** +30-120 (155 stragglers, weaker single-hit clusters; `path-to-100.md:270`).
- **Verify:** the 6 named dropped TUs (ConnectionStatusPanel, FlowPickOne,
  FlowSwitchCase, FlowCommand, TransConstraint, StoreOffer) now pin without dtk
  "ends within symbol" rejection.

### S3 — band3 pin wave D3 (needs A)

- **Goal:** pin the 43 already-wired+compiling meta_band TUs to convert +0→matches.
- **Pre:** A landed (BandDirector has 25 mis-nests; band3 spans straddle them).
- **Inputs:** ranker output for band3 group (24 `AccomplishmentManager`, 15
  `AccomplishmentProgress` oracle hits seen in §2.4); rb3-Wii addresses via autoid.
- **Lane:** main-tree SPLIT (serial). **Δ:** +20-80 (game-code, mostly small UI
  TUs; `path-to-100.md:534`). **Verify:** `category.game.matched_functions` rises
  from 14.

### S4 — Fuzzy-gap permuter waves E (needs C; parallel to D via worktrees)

- **Goal:** crack the engine fuzzy gap — **91 named units / ~98,443 recoverable
  bytes** (this session, `gap>5%`). Top 0%-matched-but-high-fuzzy targets are
  exactly permuter-class: **CharIKRod 99.8%, crc32 97.8%, Locale 92.1%, FreeCamera
  91.2%, UIListArrow 87.1%**, plus inflate (5/6, 1 left), keygen_xbox (10/21).
- **Pre:** C passed. Use `codegen-iteration-targets.md` Tier S/A queue
  (Rot OFFSET_SWAP cluster, LightHue::TranslateColor, CSHA1::Update).
- **Mechanics:** per fn: `/recon` → `/permute` → check; `permute` mechanizes the
  FP reg-swap cascades hand-edits can't fix (`feedback_fuzzy_gap_needs_permuter.md`).
  Skip AtLimit verdicts (the 99-99.99% band is LINKER_MERGED+reloc-noise,
  `codegen-iteration-targets.md:146`).
- **Lane:** **worktree-isolated builds (parallel with D-series).** Each permuter
  agent runs in its own `scripts/setup_worktree.sh` tree so its builds don't grab
  the main `.ninja-build.lock`.
- **Δ:** +50-200. path-to-100 P3 estimated +100-300 for unattacked TUs;
  `codegen-iteration-targets.md` caps already-attacked TUs at +8-15. The permuter
  (now available) is what moves this from "0 flipped" to the real range.
- **Verify:** per-TU `objdiff-cli` %; fuzzy-gap recompute should shrink.

### S5 — band3 port+pin waves F (needs A+B + playbook)

- **Goal:** port the ~130-TU meta_band backlog (172 in rb3-Wii vs 41 local) +
  other band3/network, then ranker-pin. This is the CLAUDE.md priority (game>engine).
- **Pre:** A (clean pins), B (addresses), `meta_band-port-breaking-changes.md`
  playbook (Symbol::Str(), strip `#pragma force_active`, TheUI pointer-vs-ref,
  single-arg `ObjPtr<T>`, diamond-inheritance `using Hmx::Object::Handle`, etc.).
- **Mechanics:** port a batch (rb3-Wii source via `rb3wii-pair` skill) → compile →
  ranker emits span → pin → diff. Excluded files (BandStoreUIPanel, AccomplishmentPanel)
  need deep beatmatch headers — defer (`meta_band-port-breaking-changes.md:86,377`).
- **Lane:** porting (source edits) parallel across disjoint file trees; compile +
  pin serial on SPLIT. **Δ:** +50-300 over many sessions (`path-to-100.md:534`).
- **Verify:** compile-clean per batch (source-coverage value even at +0 matches);
  game category climbs.

### S6 — Cold-pin dc3 system pool + LIBCMT spike (path-to-100 P4/P6, lower ROI)

- Defer until S1-S5 exhausted. P4 = 256 dc3 system files with no address oracle
  (`path-to-100.md:376-444`, spatial-neighbor inference, +0-400 wide). P6 = LIBCMT
  from VS CRT source (`path-to-100.md:538-578`, +50-164 if source obtainable).

### Parallel non-blocking tracks

- **[G] Native port** (`docs/plans/engine-reuse-and-asset-rendering.md`): own
  cmake build (`native/`), never touches matching SPLIT. Strategic (game-code
  behavioral oracle + the "bigger play" native target). Doesn't tick
  matched_functions; run on its own cadence. Build:
  `cd native && cmake -S . -B build -G Ninja … && cmake --build build`.
- **[H] String 8→12 layout** (`feedback_fuzzy_gap_needs_permuter.md` secondary
  blocker): RB3 `String`/`FilePath` wants 12 bytes (2nd vtable ptr) vs our 8 →
  shifts member offsets 0x34→0x38, blocks LightHue + FilePath-member fns. **RISKY**
  — `src/system/utl/Str.h:59-61` documents a prior reorder that dropped String
  copy-ctor 96.7%→39.4%. **Gate:** single dedicated session, per-function-measured,
  in a worktree; do NOT fold into a bulk wave. Δ uncertain (+5-30 if it lands).

---

## Section 4 — Parallelization & coordination rules

Codified so multi-agent rounds are safe by construction.

1. **One SPLIT writer.** Only ONE agent may edit `splits.txt`/`objects.json` +
   run a main-tree `./tools/ninja-locked` re-SPLIT at a time. D1, D2, D3 serialize.
   The `meta_band-port-breaking-changes.md:274-284` incident (concurrent bulk-wire
   corrupting band3 objects.json) is the cautionary tale — **do not run the engine
   pin-wave agent concurrently with a band3 pin agent on the main tree.**
2. **Build isolation for parallel work.** Any build-touching agent that is NOT the
   SPLIT writer MUST isolate via `scripts/setup_worktree.sh [path] [branch]`
   (btrfs CoW reflinks orig/ + build/, private build dir — CLAUDE.md "Git &
   worktrees"). Permuter waves (E) and the String investigation (H) run this way.
3. **jeff fix is isolated by construction.** Edit a **private jeff copy** in an
   isolated worktree; never mutate the main-tree `../jeff` while agents build
   against it. Rebuild + validate there; only after `cargo test` + a throwaway-tree
   re-SPLIT confirms `framing.s` 5→0 does the main thread adopt it.
4. **Agents never commit.** Leave all changes in the working tree; the main thread
   reviews + commits sequentially (keep the `Co-Authored-By` trailer — CLAUDE.md).
5. **No `git stash` / `checkout`/`restore`/`reset --hard` of files in the main
   tree** (CLAUDE.md hard rules — destroys concurrent agents' in-flight edits).
6. **Lane partitioning by file tree** for parallel porters: assign disjoint
   `src/band3/<subdir>/` slices so source edits never collide; compile/pin still
   funnels through the single SPLIT writer.
7. **Always `tee` builds** to `/tmp/rb3_build.log` (CLAUDE.md;
   `feedback_pipe_build_output.md`). **Always `./tools/ninja-locked`, never bare
   ninja** (flock on `.ninja-build.lock`).
8. **`touch config/45410914/config.yml`** before every re-SPLIT (ninja doesn't
   track splits.txt as a dep — CLAUDE.md). The "config.json newer than config.yml"
   trick (`meta_band-port-breaking-changes.md:254`) skips SPLIT for objects-only
   changes.

---

## Section 5 — Refreshed ceiling

path-to-100 §1 set three regimes off the **11,582-fn bindiff oracle (17.5% of
binary)**. The new oracles + unblocked levers shift this:

| Quantity | path-to-100 | Now (verified this session) |
|---|---|---|
| Oracle-identified fns | 11,582 (17.5%) | **13,491 (20.4%)** union of 4 oracles |
| Pinnable (source-present) fns | (implicit) | **6,972** of those 13,491 |
| NEW-pinnable TUs (source present, unpinned) | ~272 wave-4 | **194** (oracle-driven) / 981 fns |
| Engine fuzzy gap | "~88KB" | **~98,443 bytes / 91 named units** (now permuter-attackable) |
| P3 realistic yield | "over-optimistic w/o permuter" | **unblocked** — permuter wired |

**Regime restatement:**
- **Regime A (strict oracle-only):** ceiling rises 17.5% → **~20.4% of fns**
  (the +1,952 orthogonal identifications), of which ~6,972 are *actually pinnable*
  today (rest need source ported or are XDK).
- **Regime B (+ dc3 system files present):** path-to-100's ~25% holds; the ranker
  makes the *reachable-now* fraction larger because it mechanizes address
  derivation that P4 left manual.
- **Regime C (+ band3 ports + LIBCMT):** path-to-100's ~28-33% fns / 40-55% non-XDK
  bytes is unchanged in ceiling, but the **time-to-ceiling shortens**: A unblocks
  ~185 already-pinned/wired units, the permuter unblocks the ~98KB fuzzy gap, and
  the ranker turns the oracle expansion into pins without per-TU hand-derivation.

**The hard ceiling is unmoved:** the XDK shader compiler (2,324 bindiff hits, no
source anywhere — `porting-backlog-ranked.md` §4) caps whole-binary bytes around
50-60% with infinite effort. These levers accelerate progress toward the same
practical end-state path-to-100 named: **~1,000-1,800 matched fns after the
unblocked waves (A+B+D+E+F)**, vs the 394 baseline — i.e. a 2.5-4.5× near-term
multiplier, dominated by D1 (ranker wave) and E (permuter fuzzy-gap).

---

## References & docs (cold-pickup)

**Plans:** `path-to-100.md` (phases, ceiling §1), `exploratory-techniques.md`
(oracle precision §1, consensus tiers §2, next-steps §3),
`wave5-session-2026-05-28.md` (snap-via-symbols.txt §90-106, dropped stragglers
§38-58, Fader API §60-70), `codegen-iteration-targets.md` (fuzzy-gap Tier S/A
queue, AtLimit ceiling), `meta_band-port-breaking-changes.md` (MWCC→MSVC playbook,
SPLIT issue §245-268, concurrency incident §274-284),
`porting-backlog-ranked.md` (XDK source-absence ceiling),
`engine-reuse-and-asset-rendering.md` (native track / bigger play).

**Memory:** `project_jeff_asm_misnest.md` (the fix + scanner + spot-check),
`feedback_fuzzy_gap_needs_permuter.md` (permuter prereq + String 8→12 blocker),
`project_function_identification.md` (oracle method, normalized-match semantics,
Ham→Band substitution), `project_jeff_fork.md`, `project_ninja_lock_tooling.md`,
`feedback_verify_assumptions.md`, `feedback_plans_with_refs.md`,
`feedback_agent_models.md` (Opus=A/E-Geo, Sonnet=B/D mechanical, Explore=lookups).

**Tools/data (gitignored, regenerable):** `tools/fingerprint_match.py`
(triangulate/rtti/vtable/merge_bindiff/bindiff_clusters subcommands — all verified
present), `unified_id{,_callgraph,_rtti,_vtable}.json`, `autoid.json`,
`fingerprints.json`, `proposed_splits_bindiff.txt`, `config/45410914/symbols.txt`
(snap oracle), `scripts/get_progress.py`, the `decomp_synth` package (installed),
`scripts/setup_worktree.sh`, `tools/ninja-locked`.

**External:** `../dc3-decomp/src` (engine oracle), `../rb3/src` (band3/network
oracle), `../jeff/src/cmd/xex.rs` (mis-nest fix site), `../objdiff`.

**New tool this plan specifies:** `tools/pin_candidates.py` (§2) — the unified
oracle ranker. Not yet written.
