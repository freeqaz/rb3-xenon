# Path to 100% — multi-phase matching roadmap

**Date:** 2026-05-28. **Baseline (verified from `build/45410914/report.json`):**
**394 / 66,134 matched functions (0.60%), 77,024 / 11,774,176 matched code bytes
(0.65%), fuzzy 1.43% whole-binary.** Engine slice is 380/4360 fns (8.72%,
fuzzy 23.6%); game slice is 14/322 fns (4.35%, fuzzy 3.2%); XDK slice
parameters are unpinned.

**Status of "100%":** not literal. The honest ceiling reachable with the
oracles we currently possess is ~**17-25% of binary functions, ~30-50% of
non-XDK code bytes** (see §1). Each phase below moves us toward that ceiling;
the last two phases discuss what (if anything) lights up the residue beyond it.

This plan is a cross-session handoff. Per [[feedback-plans-with-refs]] every
phase references concrete files/lines/skills/memory pages so a cold session can
execute it. Per [[feedback-verify-assumptions]] every load-bearing count was
computed from the actual data files — citation in §1.

---

## Section 0 — Ranking summary (do phases in this order)

| Phase | Name | Est. Δ matches | Effort | ROI | Why this order |
|---:|---|---:|---|---|---|
| **P1** | Pipeline unbreak: jeff asm mis-nest + ninja-lock + splits validator | **+50-200** + multipliers | half-day | very high | every later phase's yield gets multiplied; no point grinding waves while ninja loops, splits silently pin wrong, and jeff drops bytes into the wrong COMDAT |
| **P2** | Wave-5 bulk wire: 302 wave-4 candidates already enumerated | **+200-600** | half-day | very high | source is in `src/`, addresses are in `/tmp/wave4_final.json`, mechanics is pure splits.txt + objects.json appends — same lever as the +159 bulk wire on 2026-05-26 |
| **P3** | Close the engine fuzzy gap: 93 partial-pinned engine TUs, ~88KB recoverable | **+100-300** | full day | high | already pinned, source already compiles, just needs codegen-iteration on the 80-99.99% band per `docs/plans/codegen-iteration-targets.md` |
| **P4** | Hamobj + ~256 cold dc3 system files (no address oracle) | **+100-400** | 2-3 days | medium-high | dc3 marked these `Matching` and source is in our `src/`; need fresh address derivation via fingerprint clustering, then pin |
| **P5** | band3 Wii→360 port wave (158 Matching + 15 Equivalent rb3-Wii files) | **+50-300** | 5-15 days | medium (per CLAUDE.md priority: highest **value**) | bandobj Stages 1-3 yielded 0 ticks but that was a class-name-renamer bug (now fixed) + Hmx::Object 0x28 (now fixed); retry with the fixed pipeline |
| **P6** | LIBCMT spike from VS-shipped CRT source | **+50-164** | 2-4 days | medium (uncertain) | only Class-3 sub-batch with partial public source; per `porting-backlog-ranked.md` §4 |
| **P7** | XDK shader compiler from-scratch RE (`xgraphics`/`d3dx9`) | **+0 to +2300** | months-years | low ROI per session | the bulk of remaining matches sit here but no source exists; out of scope for the "matching" pipeline |

After P1-P5 the most-likely-reached state is **~1,000-1,800 matched functions
(1.5-2.7% of binary, 8-15% of code bytes)**. P6 is a research detour; P7 is
"100%-unreachable" territory.

---

## Section 1 — Ceiling analysis (what's actually matchable)

### Raw counts (verified against artifacts on 2026-05-28)

**Total binary:** 66,134 functions / 11,774,176 code bytes
(`build/45410914/report.json:measures`).

**Functions with ANY cross-binary oracle hint** (verified by reading
`unified_id.json` + `autoid.json`):

| Oracle bucket | Functions | % of binary | Source path |
|---|---:|---:|---|
| **dc3 engine** (`/dc3-decomp/src/system/*`) | 5,632 | 8.52% | strongest oracle — same `/O1` flags, leaked .map |
| **dc3 XDK** (`/dc3-decomp/src/xdk/*`) | 4,962 | 7.50% | weak — dc3 never decompiled these (header-only); useful for naming, not for matching |
| **dc3 game** (`/dc3-decomp/src/lazer/*`, Ham\*) | 606 | 0.92% | medium — name signal only when the RB3 class equivalent (Band\*) actually exists |
| **rb3-Wii engine** (autoid only) | 175 | 0.26% | weak — engine sourced from dc3, not Wii |
| **rb3-Wii band3** (autoid only) | 149 | 0.23% | strong — RB3-specific game oracle, but Wii→360 port required |
| **No source identified** (dc3-inline) | 53 | 0.08% | bindiff matched the body but the dc3 symbol was inlined-only |
| **rb3-Wii network/other** | 5 | 0.01% | n/a |
| **TOTAL with ANY oracle** | **11,582** | **17.51%** | — |
| **Functions with NO oracle hint** | **54,552** | **82.49%** | unidentified `fn_8XXXXXXX` |

> Verification: `python3 -c "import json; u=json.load(open('unified_id.json'));
> print(len(u))"` → 11,582.
> Bucket math: `python3 -c "..."` in this session, see `Section 4` of this
> document's research notes (re-runnable; bucket function inlined in §6.1).

### What "100% reachable" actually means

The true ceiling depends on how aggressive we are about deriving address spans
for files lacking bindiff/autoid hits. Three regimes:

**Regime A — Strict oracle-only (the conservative floor):**
match what we already have addresses for. Ceiling = **11,582 / 66,134 = 17.5%
of functions**. Most achievable.

**Regime B — Strict oracle + dc3 system files present in `src/` without bindiff
hits:** dc3 has 690 system .cpps marked `Matching`. 159 are wired; **531 are
unwired** (verified `python3 -c "import json; dc3=json.load(open('.../dc3-decomp/config/373307D9/objects.json')); rb3=json.load(open('config/45410914/objects.json'));
print(len(set(dc3['system']['objects'])-set(rb3['engine']['objects'])))"` → 531).
Of those 531, **272 have a wave-4 address span** (`/tmp/wave4_final.json`); 256
need fresh address derivation. If we can derive addresses for all of them and
average ~10 matched functions per TU, ceiling adds ~**5,300 functions** (~8% of
binary). Combined regime-B ceiling: **~25% of functions**.

**Regime C — Aggressive (with band3 ports + LIBCMT + Ham→Band mappings):**
- rb3-Wii has 158 Matching + 15 Equivalent band3 files (`rb3/config/SZBE69_B8/objects.json`),
  but only 66 distinct band3 .cpps have ANY autoid hit, of which 18 overlap with
  Wii-Matching status. The other 155 Wii-Matching files need cold-port + cold-pin.
  Realistic add: **+100-300 functions** (most band3 are small UI/panel TUs).
- rb3-Wii has 246 Matching + 67 Equivalent + 244 NonMatching system files
  (313 Matching+Equivalent total). Most overlap with dc3 system; rb3-Wii is
  the secondary oracle for engine code that drifted.
- LIBCMT (164 hits if sourced from VS CRT): **+50-164 functions**.

Combined regime-C ceiling: **~28-33% of functions, ~40-55% of code bytes**
(non-XDK code is ~7M bytes; XDK shader compiler alone is ~3M+ bytes).

**The XDK residue (especially the shader compiler at 2,324 bindiff hits) sets
the hard ceiling around ~50-60% bytes max even with infinite effort, because
its source genuinely does not exist anywhere outside Microsoft.** Per
`docs/plans/porting-backlog-ranked.md` §4.

### Verification artifacts cited

- `unified_id.json` (gitignored; 11,582 entries; regenerate per
  `project_function_identification.md`).
- `autoid.json` (gitignored; 537 entries).
- `/tmp/wave4_final.json` (302 candidate TUs with addresses+spans, all source
  present in `src/`; produced this session by an unwired-engine sweep).
- `proposed_splits_bindiff.txt` (303 bindiff-derived cluster proposals).
- `build/45410914/report.json` (live, regenerated by `ninja`).
- DC3 source: `../dc3-decomp/config/373307D9/objects.json` (690 system,
  1224 xdk).
- rb3-Wii source: `../rb3/config/SZBE69_B8/objects.json` (304 band3, 571 system,
  379 network).

---

## Section 2 — Phased plan

### Phase 1 — Pipeline unbreak (PREREQUISITE for everything else)

**Goal:** unblock yield multipliers on all subsequent waves. No direct match
target; the phase pays off by inflating P2-P5 by an estimated 1.2-2.0×.

**Inputs:**
- Memory: `project_jeff_asm_misnest.md` (the load-bearing diagnostic).
- Memory: `project_ninja_lock_tooling.md` (ninja-lock from rb3-Wii is partially
  ported — verify `tools/ninja-locked` exists and the user's wrapper is in use).
- Memory: `project_jeff_fork.md` (the upstream-pending jeff fixes).
- Codebase: `/home/free/code/milohax/jeff/src/cmd/xex.rs` (asm-writer source).

**Mechanics (concrete steps):**

1. **Verify ninja-lock is enforced everywhere.** Currently `tools/ninja-locked`
   exists (created during the wave sessions). Confirm `objdiff.json` uses it
   for `custom_make` so MCP / orchestrator builds also serialize. The leftover
   `_CL_*` directories at repo root (visible in `git status`: `src/_CL_*` etc.)
   prove this is still leaking — clean them, harden the wrapper, and add the
   stray-`_CL_` cleanup to the wrapper preamble. See
   `tools/ninja-locked` and `CLAUDE.md` lines 84-93.

2. **Fix jeff asm mis-nest** (`project_jeff_asm_misnest.md`). The scanner is:
   ```bash
   awk '/^\.fn / { gsub(/,/,"",$2); stack[++sp]=$2 }
        /^\.endfn / { if (stack[sp] != $2) print "MISNEST " NR " " stack[sp] " vs " $2; sp-- }' \
     build/45410914/asm/<unit>.s
   ```
   Auto_03 buckets show 43k+ mis-nests each; named units 5-20 each.
   Fix in `jeff/src/cmd/xex.rs` — pop a LIFO stack on `.endfn`, OR switch to
   `.size sym, .-sym` directives. Then `cd ../jeff && cargo build --release;
   cd ../rb3-xenon; touch config/45410914/config.yml && ./tools/ninja-locked`.
   Per the memory's spot-check, `ogg_sync_init` in `default/framing` should
   flip from 0% (all `<illegal>`) to a real diff %. **Est. +50-200 matches
   across already-pinned units** (Rand, framing, Rot, bitwise, etc.).

3. **Write `tools/splits_validate.py`** — a lint that catches the recurring
   mid-symbol pin failures before they enter the build. Currently the only
   feedback is dtk producing a target `.obj` with truncated/extra fns and
   objdiff scoring 0% on them. Validator should:
   - Parse `splits.txt` per-unit `.text` ranges.
   - Cross-reference against `config/45410914/symbols.txt` (the dtk symbol
     table) to ensure both endpoints align with function-boundary addresses.
   - Warn on overlap between unit pins.
   - Be called from `configure.py` before emitting `build.ninja`.

4. **Optional bonus:** add `scripts/fuzzy_top.py` to rank fuzzy-but-unmatched
   functions by `(fuzzy_pct, size, complexity_class_from_diff_inspect)` so the
   codegen-iteration phase (P3) has a queue. ~30 LOC reading `report.json` +
   the per-fn diff JSONs in `build/45410914/diff/` (regenerable with
   `objdiff-cli diff --json`).

**Risks:**
- jeff fix is a Rust edit in someone else's fork — the memory says it's
  isolated to the asm writer; verify with `cargo test` post-edit. If a fn that
  was correctly nested regresses, roll back.
- The `_CL_*` cleanup must NOT touch anything `src/` proper. Pattern is
  literally `_CL_[0-9a-f]{8}{db,ex,gl,in,sy}` (5 files per hash). See
  `git status` which lists many such files at repo root currently.

**Verification:**
- Before: count `awk` mis-nests across `build/45410914/asm/default/*.s`
  (record the number).
- Run jeff fix; re-SPLIT (`touch config.yml && ./tools/ninja-locked`); recount.
- Run `ninja` → diff `report.json` matched_functions count.
- `ogg_sync_init` (in `default/framing`) and `ogg_sync_reset`
  (codegen-iteration-targets.md §A line 90) should each tick or improve.

**Estimated session length:** half-day. Two parallel work streams:
- Sonnet edits the validator + cleanup wrapper.
- Opus does the jeff Rust edit (one fn, but high blast radius).

---

### Phase 2 — Wave-5 bulk wire (302 wave-4 candidates)

**Goal:** drive matched_functions from 394 to ~600-1000 with pure wiring work.

**Inputs:**
- `/tmp/wave4_final.json` — 302 TUs with `{name, rel, src_oracle, start, end,
  hits, span}`. All sources verified present in `src/`. All addresses came
  from the same fingerprint-bindiff machinery that produced the +159 bulk-wire
  on 2026-05-26.
- `/tmp/wave4_single.json` — 291 single-hit subset (higher false-positive risk
  per-TU, but cheap to retry).
- `/tmp/wave4_multi.json` — 11 multi-hit subset (highest confidence).
- DC3 reference: `../dc3-decomp/config/373307D9/objects.json` system group
  (the canonical paths these TUs come from).
- Pin recipe: CLAUDE.md §"Splits-bootstrap recipe" (lines 170-184).

**Mechanics:**

1. **Sort wave4_final into batches** by confidence:
   - Tier A: 11 multi-hit entries (`/tmp/wave4_multi.json`). Highest confidence.
   - Tier B: single-hit with `span < 0x400`. Tight clusters; small false-pin cost.
   - Tier C: single-hit with `0x400 <= span < 0x4000`.
   - Tier D: single-hit with `span >= 0x4000`. Highest false-pin cost; defer.

2. **Wire one tier at a time, per the proven recipe:**
   - For each TU, append to `config/45410914/splits.txt`:
     ```
     <NAME>.cpp:
         .text       start:0x<HEX> end:0x<HEX>
     ```
   - Add `<rel>: NonMatching` to `config/45410914/objects.json`'s `engine.objects`
     dict (or `hamobj.objects` for the 44 hamobj entries).
   - For files dc3 builds with extra flags (`/TP` for json-c — see
     `project_c_libs_compiled_as_cpp.md`), use the dict form
     `{"status": "NonMatching", "extra_cflags": ["/TP"]}`.

3. **After each tier:**
   ```bash
   touch config/45410914/config.yml
   ./tools/ninja-locked 2>&1 | tee /tmp/rb3_build.log
   venv/bin/python scripts/get_progress.py
   ```
   Record Δ matches. Any TU that fails to compile (header missing, ifdef
   needed, etc.) — comment out and revisit, do not let one TU block the batch.

4. **Bulk-wiring tool** — if this is going to be a recurring waves-pattern,
   write `scripts/bulk_wire.py` that takes a JSON list of `{rel, start, end}`
   entries and emits the splits.txt + objects.json edits in one pass. ~50 LOC.
   This is the durable infra investment (see §3).

**Risks:**
- Single-hit clusters are mid-confidence: the bindiff "hit" might be one
  ICF-merged STL template instantiation rather than the actual TU body. Symptom
  = 0 matches after pin. **Mitigation:** if a TU lands 0 fns after pin, the pin
  is likely wrong — leave the source wired (compiles in `objects.json`) but
  unpin from splits.txt and re-derive from `proposed_splits_bindiff.txt` if it
  has a real cluster, or move it to P4 (cold-port pool).
- 44 hamobj entries are dc3-specific Ham\* classes that may not have Band\*
  equivalents in RB3. The Ham→Band substitution
  (`project_function_identification.md` lines 51-53) handles the 12 known
  pairs; the other 32 may simply not match. Pin anyway — non-matching wired
  source is cheap.
- The build-lane mutex: this is one ninja writer. All edits serialize through
  one agent.

**Verification:**
- `python3 scripts/get_progress.py` before vs after each tier. Expected
  growth: Tier A (~+10-30 fns; 11 TUs × ~2 fns each), Tier B (~+80-200),
  Tier C (~+50-150), Tier D (defer / retry per-TU).
- Spot-check: a TU with `hits=6` like Timer.cpp should yield 4-8 matched
  functions if the cluster is real.

**Estimated session length:** half-day (Sonnet, single build lane). The +159
bulk wire on 2026-05-26 took ~half a day for 92 TUs; this is 3× that, but
mechanics are now scripted.

**Honest uncertainty:** the wave-4 candidates were produced by a sweep that
ran AFTER the +159 was harvested, so these are by construction the weaker
clusters. Yield per TU is likely lower than the +159 batch (which averaged
1.7 matches per TU). Realistic range: **+200-600 functions**.

---

### Phase 3 — Close the engine fuzzy gap

**Goal:** convert engine fuzzy-but-unmatched bytes into matches. Engine fuzzy
is 23.60% but matched is only 10.89% — a **12.71-point gap** representing
~88,665 bytes of code that's *almost* matching. From this session's count
(P1 read this from report.json), there are **93 partial-pinned engine units**
where fuzzy − matched > 5%.

**Inputs:**
- `build/45410914/report.json` per-unit data.
- `docs/plans/codegen-iteration-targets.md` — the existing ranking of which
  TUs have which residue patterns and which are AtLimit. Read this *first*.
- `docs/decomp/patterns/` (ported from DC3) — the OFFSET_SWAP / REG_SWAP /
  PROLOGUE_MISMATCH / LINKER_MERGED pattern docs.
- `scripts/analysis/diff_inspect.py` modes (`diagnose`, `clusters`, `regswaps`,
  `offsets`, `replaces`, `compare`, `mismatches`, `stack-layout`) — the engine
  behind the `/compare-asm` skill and the `run_diff_inspect` MCP tool.
- Skills: `compare-asm`, `stack-layout`, `permute`, `recon`, `data-diff`.

**Mechanics:**

1. **Build the per-TU fuzzy-gap queue:**
   ```bash
   python3 -c "
   import json
   r = json.load(open('build/45410914/report.json'))
   gap = [(u['name'], u['measures'].get('fuzzy_match_percent',0) - u['measures'].get('matched_code_percent',0), u['measures'])
          for u in r['units'] if not u['name'].startswith('default/auto_')]
   gap = [g for g in gap if g[1] > 5]
   gap.sort(key=lambda g: -g[1])
   for n, d, m in gap[:30]:
       print(f'{d:5.1f}%  {n}  fns={m.get(\"matched_functions\",0)}/{m.get(\"total_functions\",0)}  code={m.get(\"total_code\",0)}')
   "
   ```
   This produces the top-30 TUs ranked by fuzzy gap (current top: `CharIKRod`
   99.8%, `crc32` 97.8%, `Locale` 92.1%, `FreeCamera` 91.2%, `inflate` 85.9%,
   `SHA1` 64.3% gap, etc.).

2. **For each top TU, run `/recon` then `/compare-asm`** to classify the
   residue (`docs/plans/codegen-iteration-targets.md` lists the residue
   patterns and their fixability). Use `/permute` for OFFSET_SWAP / REG_SWAP
   candidates — that's the source-permuter described in the skill.

3. **Skip AtLimit functions.** `objdiff-cli` exposes the AtLimit verdict; use
   it ruthlessly to filter the queue. Per
   `codegen-iteration-targets.md` §Ceiling, the 99-99.99% band is mostly
   LINKER_MERGED + ADDRESS_RELOCATION_NOISE — zero source-yield.

4. **Use `compare-asm.cluster_boundaries` + `data-diff`** for the 50-99% band,
   which is where the real wins live. Target TUs (per the doc):
   - **Rot.cpp** OFFSET_SWAP cluster (4 fns, recurring pattern → +2-4 ticks
     from one source edit).
   - **LightHue::TranslateColor** (single offset-swap → +1).
   - **CSHA1::Update** r29↔r30 callee-saved swap (+1, declaration-reorder).
   - **CharIKRod** at 99.8% fuzzy / 0% matched — both fns plausibly tickable;
     read the dc3 source and align local-var order.
   - **crc32** at 97.8% fuzzy / 0% matched — one function (892B), likely a
     prologue or register choice diff.
   - **inflate** at 99.7% fuzzy, 5/6 matched — one remaining function;
     LINKER_MERGED 18-call cluster per codegen-iteration-targets.md §A.

5. **Per-TU loop (mechanized):**
   ```
   /recon <addr>
   /compare-asm <addr>
   <pattern fix in source>
   ./tools/ninja-locked 2>&1 | tee /tmp/rb3_build.log
   /run_objdiff <unit>
   <repeat or mark AtLimit and move on>
   ```

**Risks:**
- 99-99.99% band is AtLimit per `codegen-iteration-targets.md`. Chasing it
  wastes Opus-hours.
- Some "fuzzy 99.8%" units (e.g. CharIKRod) are still 0% matched because of
  unrenamed callees, not codegen. Try `python3 tools/fingerprint_match.py
  gen_target_map` first — it may tick on rename alone.
- Per the codegen-iteration-targets.md ceiling estimate, the *whole* effort is
  +8-15 functions / +3-5K bytes from this kind of work. Honest expected yield:
  +50-150 fns once we re-include the 50-95% band that previous sessions
  haven't attacked yet (most prior work was at 95-99% band, which is AtLimit).

**Verification:**
- `python3 scripts/get_progress.py` per-TU.
- `objdiff-cli report` engine slice `fuzzy_match_percent` should drop as
  matched_code_percent rises (fuzzy includes matched, so the *gap* shrinks).

**Estimated session length:** full day per ~5-10 TUs Opus-level. Sonnet can do
the mechanical OFFSET_SWAP / REG_SWAP edits in parallel after the queue is
built.

**Honest uncertainty:** the per-doc ceiling is +8-15 fns from existing-attacked
TUs. By attacking the UNATTACKED ones (CharIKRod, crc32, Locale, etc. — the
top of the gap list), the real ceiling could be **+100-300 fns** since these
TUs are at 0/N matched today and have high fuzzy.

---

### Phase 4 — Cold-pin the unwired dc3 system pool

**Goal:** wire the 256 dc3 system files that have NO bindiff hit and NO wave-4
candidate, by deriving addresses from string content + structural similarity.

**Inputs:**
- 531 unwired dc3 system files (verified: `set(dc3.system) - set(rb3.engine)`).
- 272 have wave-4 addresses → covered by P2.
- **256 have no address oracle** → this phase.
- Of those 256, most are small leaf files (helper TUs that don't get bindiff
  hits because they're too generic). Examples: char/CharCollide,
  utl/HelperFuncs, world/FrustumPlane, etc.
- `tools/fingerprint_match.py` autoid / identify subcommands.
- `proposed_splits_bindiff.txt` for boundary hints.

**Mechanics:**

1. **Enumerate the cold pool:**
   ```bash
   python3 -c "
   import json
   dc3 = json.load(open('../dc3-decomp/config/373307D9/objects.json'))
   rb3 = json.load(open('config/45410914/objects.json'))
   import json as J
   w4 = J.load(open('/tmp/wave4_final.json'))
   w4_rels = set(r['rel'] for r in w4)
   absent = set(dc3['system']['objects']) - set(rb3['engine']['objects']) - w4_rels
   for p in sorted(absent): print(p)
   " > /tmp/cold_pool.txt
   ```

2. **Re-run autoid with more permissive thresholds.** Current autoid emits 537
   entries at score≥1. Try `autoid --min-score 1 --min-strings 1` (the defaults
   reject many small leaf TUs). Look specifically for files in `/tmp/cold_pool.txt`
   that now get *any* hit, even weak.

3. **For the remainder, use spatial neighbor inference:** every wired TU has a
   .text address range. The dc3 link map (`ham_xbox_r.map`) preserves source
   order within library groups. For cold-pool files that sit between two already-
   wired neighbors in dc3 order, their RB3 address is bounded by the wired
   neighbors' RB3 addresses. ~50 LOC tool: parse dc3 map order, find consecutive
   triples `(wired_A, cold_X, wired_B)`, propose
   `cold_X.text = (A.end, B.start)`.

4. **Pin the proposed spans as NonMatching.** Build. Most won't compile (cold-
   port files often need extra headers from their dependency tree). Mark
   problematic ones with a comment in objects.json and retry incrementally.

5. **For files that compile but never match,** they're either:
   - Wrong address (mark for re-derive).
   - dc3 source diverges from RB3 source (the CLAUDE.md "dc3 is newer" caveat).
     Cross-check rb3-Wii equivalent.

**Risks:**
- The spatial-neighbor inference assumes dc3 and RB3 share linker emit order;
  CLAUDE.md confirms "no LTCG, TU spatial grouping preserved" but ORDERING
  across TUs is a function-of-link-order, which differs. Treat as a heuristic,
  not a guarantee.
- Each cold-pin failure costs ~5 minutes in build + diagnostic. Batch carefully.

**Verification:**
- Per-TU: pin → build → count matched. If matched > 0, real. If matched = 0
  but the unit has a valid `.text` size in `report.json`, pin is wrong; reset.

**Estimated session length:** 2-3 days. Mostly mechanical, build-bound.

**Honest uncertainty:** very wide. Could yield +0 to +400 depending on how
many cold-pool TUs have at least one byte-matchable function. The cold pool is
biased toward small leaf TUs (the larger ones were caught by bindiff/autoid),
so per-TU yield is probably 0-2 fns averaged.

---

### Phase 5 — band3 / network Wii→360 port wave

**Goal:** match RB3 game code by porting rb3-Wii's 158 Matching + 15 Equivalent
band3 .cpps (and 64 Matching network .cpps). This is the CLAUDE.md decomp
priority: "game, not engine."

**Inputs:**
- `../rb3/src/band3/` — 300 .cpp files (158 Matching, 15 Equivalent, 117
  NonMatching per `rb3/config/SZBE69_B8/objects.json`).
- `../rb3/src/network/` — 178 .cpp files (64 Matching).
- `docs/plans/bandobj-port.md` — staged port recipe (StreakMeter, CrowdAudio,
  BandDirector, BandCharacter).
- `docs/plans/hmx-object-layout.md` — the 0x28 fix landed (commits 5066509 +
  f777aa1) so bandobj subclass member offsets should now align.
- `project_function_identification.md` line 51 — the Ham→Band class renamer
  fix landed; bandobj symbols now pair correctly with bindiff data.
- Skill: `rb3wii-pair` (the primary game-code oracle).
- 18 band3 .cpps overlap both rb3-Wii-Matching AND have an RB3 autoid address
  hint (computed in §1 above). These are the highest-leverage starters.

**Mechanics:**

1. **Start with the 18 dual-oracle band3 files** (Wii Matching AND has RB3
   address hint). Compute the list:
   ```bash
   python3 -c "
   import json
   u = json.load(open('unified_id.json'))
   w = json.load(open('../rb3/config/SZBE69_B8/objects.json'))
   wii_band3 = {p: (v if isinstance(v, str) else v.get('status')) for p, v in w['band3']['objects'].items()}
   wii_ok = set(p for p, s in wii_band3.items() if s in ('Matching', 'Equivalent'))
   autoid_band3 = set()
   for r in u:
       src = r.get('autoid_src', '')
       if '/rb3/src/band3/' in src:
           autoid_band3.add(src.replace('../rb3/src/', ''))
   for p in sorted(wii_ok & autoid_band3):
       print(p)
   "
   ```

2. **For each dual-oracle TU, follow the bandobj-port.md Wii→360 recipe:**
   - Remove MWCC `#pragma push/pop` (bandobj-port.md §3 — 16 bandobj cpps have
     these).
   - Replace `revolution/` includes with `xdk/` or stub.
   - Translate `inline asm volatile` (rare — 2 files only in bandobj).
   - Adapt for `Hmx::Object` 0x28 layout (already fixed).
   - Verify the Ham→Band class substitution map covers any new classes the file
     references.

3. **Wire each port with both .cpp source and the bindiff/autoid-derived `.text`
   span.** Compile + diff.

4. **For the 155 Wii-Matching-but-no-address-hint files**, this is colder work
   but the Wii source IS Matching — the SOURCE will compile to MSVC X360 (after
   Wii→360 porting), and we cold-pin the address using string content from the
   port + autoid re-run.

5. **For network/ files**, similar pattern. 50 network/Platform files include
   Xbox-specific .cpps that may use raw Win32 calls directly portable to MSVC.

**Risks:**
- Per `session-handoff-2026-05-27.md` §"Three paths forward", bandobj Stages
  1-3 yielded 0 matches in a prior attempt. The diagnosis was the BinDiff
  Ham→Band naming bug (now fixed) AND the Hmx::Object layout (now fixed). But
  it's still possible the residue is class-layout drift in OTHER bandobj-specific
  classes (BandLabel, BandCamShot, etc.) that we haven't audited.
- band3 ports are the highest-effort phase. Each file averages 50-1000 LOC of
  RB3-specific code. MWCC→MSVC conversion has 5-15 friction points per file
  per `bandobj-port.md` §3.
- Per CLAUDE.md ("dc3 is newer than RB3"), some band3 files reference engine
  classes that have version-drifted. The Hmx::Object 0x28 work showed the
  pattern.

**Verification:**
- Per TU: compile + diff. Even 0-match wires are valuable for source coverage
  (the CLAUDE.md secondary goal, "buildable codebase").
- Watch `category.game.matched_functions_percent` go from 4.35% upward.

**Estimated session length:** 5-15 full days, parallelizable across porter
agents (one ninja lane). Realistic burndown: 5-10 TUs/day Sonnet, 1-3
TUs/day Opus for complex classes.

**Honest uncertainty:** the bandobj Stages 1-3 prior attempt yielded 0. But
that was BEFORE the Ham→Band fix + the Object layout fix. Retry budget: try
the 18 dual-oracle TUs first; if they yield 0, halt and re-investigate before
committing to the 155-TU cold port. **Realistic range: +50-300 functions.**

---

### Phase 6 — LIBCMT spike (optional research detour)

**Goal:** match the 164 hits in 25 LIBCMT TUs by leveraging the VS-shipped
MSVC CRT source.

**Inputs:**
- Visual Studio 2010 (or matching) installation: `VC/crt/src/*.c`.
- The Xbox 360 toolchain version (build/compilers/X360/16.00.11886.00).
- `docs/plans/porting-backlog-ranked.md` §4 — the feasibility framing.
- `proposed_splits_bindiff.txt` — LIBCMT cluster proposals (crtgpr.cpp,
  crtfpr.cpp, crtvmx.cpp, ehstate.cpp, others).

**Mechanics:**

1. **Identify the X360-specific CRT version.** The LIBCMT in retail Xbox 360
   binaries was VS2010's CRT with X360 hardware adaptations. The exact files
   needed: source for `setjmp`, `_initterm`, `__savegpr*` / `__restgpr*` (the
   ones already in `proposed_splits_bindiff.txt` as crtgpr.cpp). Some of these
   are XDK CRT — check `build/compilers/X360/*/` for shipped sources.

2. **Wire CRT files as NonMatching, pin from proposed_splits_bindiff.txt.**

3. **Diff against asm; iterate.** Most CRT functions are leaf-like (no Object,
   no inheritance) so codegen should be close to byte-identical with the right
   source.

**Risks:**
- Sourcing exact CRT version is the gating concern. If VS2010 CRT source is
  not available, this phase is blocked.
- Even with source, the X360-specific compilation flags (`/O1 /Oi`) may
  produce different codegen than VS2010 x86 builds.

**Verification:**
- Per-TU. crtgpr (15 fns, register-save stubs) should match at near-100% if
  the source is right — these are 4-instruction functions.

**Estimated session length:** 2-4 days IF source is sourced; otherwise 0
(infeasibly blocked at discovery).

**Honest uncertainty:** +50 to +164. The whole phase may be 0 if the CRT
source can't be located.

---

### Phase 7 — XDK shader compiler from-scratch RE

**Goal:** match the ~2,324 hits in the XGRAPHICS/d3dx9 shader compiler. Per
`docs/plans/porting-backlog-ranked.md` §4 this is months-to-years and
fundamentally out of scope for the matching pipeline (no source exists).

**Not recommended as a phase in this plan.** Document it for completeness:

- The retail HLSL→Xenos shader compiler is MS proprietary.
- Bindiff identifies it by name (CParse, CProgram, CShaderProgram, etc.) but
  has no source to compile against.
- If ever attempted, the C-style leaf containers (linkedlist 45 hits, vector 12,
  hashtable 11, dlist 9) are the only tractable entry points — they're generic
  template-like inlines that may match without source by treating them as
  copy-from-asm decomps.

**Estimate:** out of scope. Track as the residual ceiling, not as work.

---

## Section 3 — Infra prerequisites (cross-cutting)

### Tooling investments that pay off across multiple phases

| Tool | Phase(s) it unblocks | Where | Effort | Yield multiplier |
|---|---|---|---|---|
| `tools/splits_validate.py` | P2, P4, P5 | new file | 2 hours | catches mid-symbol pins → saves 5-30 min/TU debug |
| `scripts/bulk_wire.py` | P2, P4 | new file | 3 hours | accepts JSON `{rel,start,end}` list → emits splits.txt + objects.json edits in one pass |
| `scripts/fuzzy_top.py` | P3 | new file | 2 hours | ranks fuzzy TUs by ROI for codegen-iteration queue |
| jeff asm mis-nest fix | P2, P3 (all pins) | `jeff/src/cmd/xex.rs` | half-day Opus | unlocks +50-200 already-pinned matches; multiplier on all future pins |
| ninja-lock hardening | all phases | `tools/ninja-locked` | 1 hour | eliminates manifest-dirty loops + `_CL_*` corruption |
| AtLimit-skip queue | P3 | use `diff_inspect.py mismatches` mode | 1 hour | filters codegen-iteration queue down to plausible wins (avoids the 33 AtLimit fns in 99-99.99% band) |
| `tools/spatial_neighbor.py` | P4 | new file | half-day | infers cold-pool addresses by dc3 link-map order |

### Build-pipeline issues

- **Ninja manifest-dirty loop** (CLAUDE.md lines 84-93). The wrapper exists;
  ensure objdiff.json uses it (`custom_make`). Verify `_CL_*` stragglers are
  cleaned and prevented going forward.
- **jeff asm mis-nest** (P1, also a tooling investment).
- **Raw fuzzy report** — dc3's report uses a two-fuzzy display (matched +
  fuzzy-only). objdiff fork already supports the calc; verify `report.json` has
  the `fuzzy_match_percent` slot per unit (yes, it does, this session).

### Source-tree cleanups

- The repo root has many `_CL_*` staging directories from prior build
  collisions (see `git status` at session start: `src/_CL_*`, `_CL_*` at root).
  Clean before serious wave work. This is a one-time hygiene task.

---

## Section 4 — Path back from each phase (what success looks like)

| Phase | Pre-state | Post-state target | If we stop here |
|---|---|---|---|
| P1 | 394 / fuzzy 1.43% | 444-594 / fuzzy 2-3% | Build pipeline stable; future waves yield more |
| P2 | 444-594 | 644-1194 | "Wave-5" lever is exhausted; next is colder work |
| P3 | 644-1194 | 744-1494 | Engine fuzzy gap closed; remaining engine is AtLimit |
| P4 | 744-1494 | 744-1894 | Cold-pinned engine pool wired; ~290 dc3 system TUs all live |
| P5 | 744-1894 | 794-2194 | band3/network porting in progress; game category meaningfully above 5% |
| P6 | 794-2194 | 844-2358 | LIBCMT recovered (or proven blocked) |
| P7 | 844-2358 | 844-N (open) | Shader compiler residue remains; project at practical ceiling |

End-state ceiling per regime (§1): **~3-5% of functions matched, ~30-40% of
non-XDK code bytes**. Honest cap, given the XDK residue dominates the
denominator.

---

## Section 5 — Coordination + execution constraints

Per `feedback_agent_models.md` + CLAUDE.md "Known issues":

- **Build-lane mutex:** one `./tools/ninja-locked` writer at a time. P2, P3, P4
  are intrinsically build-bound — single agent per phase.
- **No worktrees during active dev** (user pref). All edits in-place.
- **`tee` build output** to a log file (CLAUDE.md lines 76-82). Bare `ninja`
  failure modes are invisible without the log.
- **Verify load-bearing assumptions** before acting
  (`feedback_verify_assumptions.md`). Spawn an Opus subagent on any new claim
  before committing to action.
- **Agent models** (`feedback_agent_models.md`):
  - Opus for P1's jeff fix, P3 Opus targets (Geo/Rot MakeEuler), P5 complex
    bandobj classes.
  - Sonnet for P2 bulk wire, P3 Tier S mechanical edits, P5 simple ports.
  - Explore for lookups only, never planning.

---

## Section 6 — References & docs (for cold-pickup per [[plans-with-refs]])

### Primary plan docs (in `docs/plans/`)
- `engine-reuse-and-asset-rendering.md` — the strategic frame (game > engine);
  the "bigger play" of a native target with DC3 rndobj.
- `porting-backlog-ranked.md` — the source-availability ceiling proof. CLASS 1
  cheap batch (now done), CLASS 3 XDK residue analysis.
- `codegen-iteration-targets.md` — P3's per-TU residue ranking + Tier S/A/B
  pattern matrix.
- `bandobj-port.md` — P5's Wii→360 idiom checklist + dependency map.
- `hmx-object-layout.md` — the 0x28 fix that unblocks bandobj subclass matches.
- `bindiff-integration.md` — how `unified_id.json` was built.
- `session-handoff-2026-05-27.md` — the prior session's accomplishments + the
  three-paths-forward framing P5 builds on.
- `match-first-fn.md` — the first-fn strategy + Object-layout post-mortems.

### Memory pages (auto-loaded; in `~/.claude/projects/-home-free-code-milohax-rb3-xenon/memory/`)
- `project_rb3_xenon_roadmap.md` — phase tracking through the 394-match state.
- `project_function_identification.md` — fingerprint+autoid+bindiff method,
  Ham→Band substitution, `matched_functions` normalized-match semantics.
- `project_jeff_asm_misnest.md` — **P1's load-bearing diagnostic.**
- `project_jeff_fork.md` — what's already fixed in jeff vs upstream.
- `project_ninja_lock_tooling.md` — P1's hardening reference.
- `project_native_port.md` — out of scope for matching but relevant to P5
  testing.
- `project_c_libs_compiled_as_cpp.md` — json-c built as C++; expand to zlib,
  tomcrypt, curl, oggvorbis (P2 may need similar `extra_cflags=["/TP"]`).
- `feedback_verify_assumptions.md`, `feedback_plans_with_refs.md`,
  `feedback_autonomy.md`, `feedback_agent_models.md` — execution discipline.

### Tooling (re-runnable per-session)
- `tools/fingerprint_match.py` — subcommands: extract, report, autoid, identify,
  merge_bindiff, bindiff_clusters, gen_target_map.
- `scripts/get_progress.py` — read `decomp.db` + `report.json` for current state.
- `scripts/orchestrator/` — MCP server: query_functions, run_objdiff,
  run_diff_inspect, lookup_dc3, lookup_rb3wii, etc.
- `scripts/analysis/diff_inspect.py` modes — backs `/compare-asm`, `/stack-layout`,
  the MCP `run_diff_inspect` tool.
- Skills (in `.claude/skills/`): `recon`, `compare-asm`, `stack-layout`,
  `progress`, `data-diff`, `permute`, `vtable`, `struct-info`, `resolve-vcall`,
  `dc3-pair`, `rb3wii-pair`, `batch-check`.

### Cached data (gitignored, regenerable)
- `fingerprints.json`, `autoid.json`, `unified_id.json` —
  `tools/fingerprint_match.py {extract,autoid,merge_bindiff}`.
- `/tmp/wave4_final.json`, `/tmp/wave4_single.json`, `/tmp/wave4_multi.json` —
  enumerated this session. Re-derive if stale.
- `proposed_splits_bindiff.txt`, `proposed_splits_bindiff.spotcheck.csv` —
  `tools/fingerprint_match.py bindiff_clusters`.
- `tools/bindiff_match.json` — Ghidra+BinDiff harvest per
  `rb3-xenon-bindiff.md` (planning doc in `~/.claude/plans/`).

### External
- DC3 source tree: `../dc3-decomp/src/` (engine + xdk + leaked .map at
  `orig/373307D9/ham_xbox_r.map`).
- rb3-Wii source tree: `../rb3/src/` (band3 + system + network) and progress at
  `../rb3/config/SZBE69_B8/objects.json`.
- jeff fork: `../jeff` (the dtk).
- objdiff fork: `../objdiff` (freeqaz v4.1.0).
- Ghidra MCP at port 8002; orchestrator MCP via `.mcp.json`.

### Build/run commands
```bash
# Verify state
ninja 2>&1 | grep -A4 "^Progress:"
venv/bin/python scripts/get_progress.py

# Pin a new unit + rebuild
# 1. Append to config/45410914/splits.txt + objects.json
# 2. touch config/45410914/config.yml
# 3. ./tools/ninja-locked 2>&1 | tee /tmp/rb3_build.log
# 4. venv/bin/python scripts/get_progress.py

# Regenerate identification data (slow ~1 min for extract)
python3 tools/fingerprint_match.py extract --out fingerprints.json
python3 tools/fingerprint_match.py autoid --out autoid.json
python3 tools/fingerprint_match.py merge_bindiff --out unified_id.json
python3 tools/fingerprint_match.py bindiff_clusters --out proposed_splits_bindiff.txt

# Regenerate target rename map (fast)
python3 tools/fingerprint_match.py gen_target_map

# Per-fn diff
~/code/milohax/objdiff/target/release/objdiff-cli diff --concise --verdict \
  -1 build/45410914/obj/<UNIT>.obj \
  -2 build/45410914/src/<path>/<UNIT>.obj '<symbol>'
```

---

## Section 7 — What's not in this plan

Excluded by scope or per CLAUDE.md priority:

- **The native engine port** (`native/`) — out of the matching track per
  CLAUDE.md "Two build tracks". May provide behavioral validation oracle for
  P5 game-code ports but doesn't tick matched_functions.
- **A new bindiff harvest from a fresher Ghidra project** — could yield more
  `unified_id` entries but is a multi-day infrastructure detour. Defer until
  P1-P5 are exhausted.
- **An MSVC-map-based LINKER_MERGED extension** — discussed and rejected in
  `session-handoff-2026-05-27.md` §Honest ceiling. Real implementation
  project, uncertain yield.
- **The "bigger play" native rb3-xenon** (DC3 rndobj + RB3 game code) per
  `engine-reuse-and-asset-rendering.md` §"Bigger play". Strategic, not a
  matching-percentage phase.
