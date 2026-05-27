# Session handoff — 2026-05-26/27 → next session resumption

Comprehensive checkpoint of the matching-bootstrap session that took
rb3-xenon from **0 → 290 matched functions / 0.50% of the whole binary**.
This doc is the resumption oracle for the next session.

## Final state (verified)

- **HEAD**: `744f9e9` on `main`. Tree clean.
- **matched_functions: 290 / 66,102 (0.50%)** — `58,584 / 11,784,560` code bytes matched.
- 338 report units, ~20 cluster pins + 92 bulk-wired engine TUs.
- All 11 plan docs at `docs/plans/`.
- Build: `ninja` exits 0; `python3 configure.py progress` shows the above.
- Native build is pre-broken (separate from matching — `SongInfoCopy` virtual stubs missing from before 8b28623; `Fader::SetTranspose` was fixed this session).

## What we achieved this session

| Phase | Lever | Δ matches | Cumulative |
|---|---|---|---|
| 1 | Math leaves + MasterAudio thunks (after target-renamer + Object pin) | +24 | 24 |
| 2 | Bucket B: wire oggvorbis + leaf utl/synth (already in src/) | +86 | 110 |
| 3 | gen_target_map cross-TU renamer-gap fix | +1 | 111 |
| 4 | **Bulk-wire 92 dc3 engine TUs** (the big jump) | **+159** | **270** |
| 5 | Class-1 cheap batch (keygen_xbox, Memory_Xbox, StreamNull, ...) | +20 | 290 |
| 6 | bandobj Stages 1+2+3 (StreakMeter, CrowdAudio, BandDirector) | 0 | 290 |
| 7 | Codegen-iteration squeeze (LINKER_MERGED, Rot OFFSET_SWAP, ...) | 0 | 290 |

**Lesson:** the dominant lever was **wiring already-present dc3 engine source** (Phase 4 rsync'd dc3's full `src/system/` in a prior session). Heavy Wii→360 ports (bandobj) yielded 0 in our test slice because BinDiff names the *dc3 equivalent* class (HamDirector ≠ BandDirector), and codegen iteration is at AtLimit ceiling for most current near-misses.

## Durable infrastructure built

**Tooling (rb3-xenon):**
- `tools/fingerprint_match.py` — extract / autoid / identify / report / merge_bindiff / bindiff_clusters / gen_target_map (with `--include-unpinned`, `--min-similarity`, denylist).
- `tools/dc3_map.py` — MSVC demangler + dc3 `ham_xbox_r.map` parser (mtime-cached).
- `tools/bindiff_match.json` (gitignored, 11,057 cross-binary IDs from Ghidra+BinDiff; 3.6 MB; regenerate per docs/plans/rb3-xenon-bindiff.md in ~10 min).
- `unified_id.json` / `proposed_splits_bindiff.txt` / `proposed_splits_bindiff.spotcheck.csv` (all gitignored, regenerable).
- `scripts/obj_target_symbol_renamer.py` + `scripts/target_symbol_map.json` (~9,400 entries auto-gen'd) — **the unblock that ticks the counter**, renames dtk `fn_<addr>` targets to MSVC-mangled names.
- `configure.py` defaults `--objdiff` to `../objdiff` (freeqaz v4.1.0 fork).

**jeff fork (`../jeff`):**
- HEAD `b1bc97c`. Three new capabilities on top of `e86c858`:
  - `FindXboxVtables` analysis pass (`pass.rs`) — 2,741 vtable candidates emitted to `build/45410914/proposed_splits.txt` for human review.
  - **`genuine_except_data_set()` (BIG fix)** in `src/util/xex.rs`: stopped `write_coff` from zeroing live code in 18,098 spurious `except_data_*` symbols. Project-wide scoring unblocked.

**Source corrections (rb3-xenon):**
- `Hmx::Object` 0x28 layout via `OBJREF_VIRTUAL` (RB3 has non-polymorphic ObjRef). `#ifndef HX_NATIVE`-guarded so native build is preserved.
- `Fader::SetTranspose`/`GetTranspose` added (was missing → broke FreeCamera + ThreeDSound + native).
- `KeyChain` is `namespace`, not `class` (different mangling — unlocked 8 keygen matches).

## The honest ceiling — why we paused

Phases 6+7 = 0 match growth in ~6 agent-hours. Empirically verified:
- **99–99.99% band = AtLimit:** LINKER_MERGED + ADDRESS_RELOCATION_NOISE + volatile register-swap. Mathematically zero-yield to chase from source.
- **80–95% band = class-layout shifts + scheduler-driven offset swaps.** Source reorders regress, not improve. Needs real engine-API ports or a tactic change.
- **LINKER_MERGED renamer extension** (the "biggest untapped lever" hope): documented infeasible — objdiff needs an MSVC .map enumerating ICF-merged groups; the stripped retail XEX has none. Synthetic construction is a real implementation project with uncertain yield.

## Three paths forward (next session)

### Path A — Engine-API porting wave (heavy, uncertain)
Port LightPresetManager + LightPreset + CameraManager + CharBones + CharFaceServo from rb3-Wii (Wii→360 idiom checklist already captured in commits `f9d8306`, `19abea1`, `534d984`). Would unlock the bandobj subclass member matches that Stage 3 stubbed out. **Risk:** Stages 1+2+3 of bandobj yielded 0 each — the engine-API wave faces the same wrong-class-name BinDiff failure mode. Estimated 3-10 matches per ported TU, but Stage 1+2+3 over-estimated similarly. Honest expectation: maybe +20-50 over ~10 agent-hours.

### Path B — Tactic change: function-by-function hand decomp
Pick a TU with KNOWN ground-truth (e.g., we have source from rb3-Wii for game logic) and hand-decompile individual functions against asm. Slow (~1 fn/hour Opus) but high confidence per match. Recommended TUs: `MasterAudio::SetupTrackChannel` (98.8% — known close, real codegen diff), `BandDirector::*` (now compiled but unmatched), specific oggvorbis near-misses. **EV:** ~5-15 fns per agent-hour focused effort. Doesn't require new infrastructure.

### Path C — Decouple porting from matching
Accept matching plateau; focus on **source coverage** as the goal. Port more bandobj/band3 game TUs (BandWardrobe, BandCharacter, etc.) for compile-coverage even with 0 match-yield. Aligns with rb3-xenon's broader decomp-project value (a buildable codebase, even if not byte-matching). This is the user's stated underlying goal: "port from rb3 + dc3."

## Plan-doc index (under `docs/plans/`)

1. `match-first-fn.md` — fn_8275A2C0 strategy + post-mortems (Object layout discovery).
2. `hmx-object-layout.md` — the 0x28 reconstruction (executed).
3. `porting-wave-1.md` — Bucket B vs Bucket A framing.
4. `bandobj-port.md` — Stages 1-3 + Wii→360 idiom checklist.
5. `porting-backlog-ranked.md` — **the source-availability ceiling finding** (98.7% is MS-proprietary XDK).
6. `codegen-iteration-targets.md` — the squeeze-target ranking + the "real prize is 80-95%" framing.
7. `jeff-vtable-detector.md` — FindXboxVtables design (executed).
8. `bindiff-integration.md` — Ghidra+BinDiff flow (executed).
9. `rb3-xenon-bindiff.md` — operational setup.
10. `pin-tier2-clusters.md` — tier-2 pin batch (executed).
11. **This file** — handoff.

## Memory pointers (auto-loaded)

`~/.claude/projects/-home-free-code-milohax-rb3-xenon/memory/`:
- `project_rb3_xenon_roadmap.md` — phase tracking + the matching-progress live update.
- `project_function_identification.md` — fingerprinter approach + **the critical metric insight** (`matched_functions` is normalized, reloc-name-insensitive).
- `project_jeff_fork.md` — the 3 new capabilities + their commits.
- `feedback_agent_models.md` — Opus for planning/hard; Sonnet for mechanical; Explore only for simple lookups; **no worktrees during active dev**.
- `feedback_verify_assumptions.md` — Opus subagent verification before load-bearing.
- `feedback_plans_with_refs.md` — cross-session plans need doc/file/URL refs.

## Quick-start commands

```bash
cd /home/free/code/milohax/rb3-xenon

# Verify current state (expect 290 / 66102)
ninja 2>&1 | grep -A4 "^Progress:"

# Regenerate fingerprint indexes (slow: ~1 min) — only if dtk split changed
python3 tools/fingerprint_match.py extract --out fingerprints.json
python3 tools/fingerprint_match.py autoid   --out autoid.json
python3 tools/fingerprint_match.py merge_bindiff --out unified_id.json
python3 tools/fingerprint_match.py bindiff_clusters --out proposed_splits_bindiff.txt

# Regenerate target rename map (fast — after pinning new TUs)
python3 tools/fingerprint_match.py gen_target_map

# Per-function diff (the iteration loop)
~/.local/bin/objdiff-cli diff --concise --verdict \
  -1 build/45410914/obj/<UNIT>.obj \
  -2 build/45410914/src/<path>/<UNIT>.obj '<symbol>'
```

## Coordination constraints to honor

- **Build-lane mutex**: only ONE agent runs `ninja` / rebuilds `../jeff/target/release/dtk` at a time. Worktrees are disallowed for active dev. Plan for serial build-touching work.
- **Agent models**: Opus for planning + hard implementation (Rust, byte-matching, foundational); Sonnet for mechanical (mass-wiring, pattern edits, doc commits). Explore only for simple lookups, never for planning.
- **Verify-assumptions**: any load-bearing claim → Opus subagent to verify against ground truth before committing to action. This caught the false xidata lever, the dc3-debug-build myth, and the Class-1 backlog hypothesis this session.
- **No worktrees during active dev** — user pref; in-place agents only.
