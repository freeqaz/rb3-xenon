# jeff leaf-split (Class-2 pdata over-split) fix — status

> **STATUS (2026-07-17): LANDED — `eb4863cc` (+67 vs current main 15,236 →
> 15,303, 0 regressions).** jeff `../jeff` fast-forwarded to `7f69b9e`
> (7e49a38 merge pass + 7f69b9e doc); verified clone binary atomically swapped
> in (old binary backed up at `../jeff/target/release/dtk.pre-class2-bak`);
> `tools/project.py` split rule wired with
> `JEFF_MERGE_PROTECT=scripts/target_symbol_map.json` (P6, load-bearing);
> `config/45410914/symbols.txt` regenerated (1,727 fragments absorbed,
> total_functions 71,123 → 70,273). Delta was +77 on the stream's old 15,207
> baseline; ~10 overlapped the intervening BinStreamRev/re-anchor lands, so
> +67 on the current baseline. Original GO verdict + landing recipe preserved
> below.

## Objective

Implement + A/B-verify a jeff (dtk fork, `/home/free/code/milohax/jeff`) merge
pass for **Class 2 — AddRoll-class `.pdata` over-splits**: retail `.pdata`
splits one contiguous compiled function into 2+ consecutive entries; jeff
(post-a670a12) treats each entry as an authoritative separate function, emits
both fragments as separate anon `fn_` symbols, and objdiff pairs our single
compiled body against the first fragment → permanent mismatch.

Authoritative spec: `docs/plans/jeff-pdata-boundary-round3.md` (Class 2
section, incl. detection predicate, invariant updates, validation plan).
Classes 1 (terminatorless fragments) and 3 (stray func_type==3 except_data)
are secondary — scoped only if Class 2 lands cleanly.

## Evidence base (why now)

- Confirmed fixture: `Stats::AddRoll` @ `fn_826791B0`(16B) + `fn_826791C0`
  (20B, zero xrefs); our compile = one 36B function. at_limit purely on the
  split (wave-38).
- TU5 wave-6 lane bresidual (`~/tmp/p5w6/bresidual-notes.md`): the
  SongSortBy{Rank,Song,Artist,Review,Recent} residual losses are all "anon
  fn_ leaf-split/EH-funclet triples at confirmed_twin fz=99.900" whose NAMED
  functions are already 100%; SongStatusMgr/Track/TrackWidget same shape.
  **Caveat under investigation:** fz=99.9 pairing is evidence these may be a
  different class (EH-funclet naming/pairing) than the low-% AddRoll shape —
  stage-1 agent B is classifying them before we assume the Class-2 pass
  reclaims them.

## Plan / stage gates

1. **Stage 1 (running):**
   - Agent A: isolated census — clone jeff → `~/tmp/jeff-leaf-clone`,
     instrument a `JEFF_CLASS2_CENSUS=1` dump using jeff's own reloc set,
     run in worktree `~/tmp/wt-jeffleaf-census` (dtk pointed at the clone via
     `configure.py --dtk`). Output `~/tmp/jeffleaf/census.md`.
   - Agent B: read-only classification of the SongSortBy*/SongStatusMgr/Track
     anon-triple residuals vs the 3 defect classes. Output
     `~/tmp/jeffleaf/songsort-classification.md`.
   - **Gate:** <10 fixable families binary-wide → downscope to one-time
     symbols.txt repair or NO-GO.
2. **Stage 2+3 (RUNNING, combined):** implement `merge_fallthrough_leaf_fragments`
   in the jeff clone (branch `class2-merge`) AND run the whole-binary A/B in the
   SAME agent as a tight implement→test→A/B→tighten loop — because the dominant
   risk (over-fire merging a currently-matching funclet) is only detectable via
   the matched-SET A/B and fixing it requires iterating the predicate. Pass
   operates on the FINAL function-symbol set (inverse of a670a12), NOT the pdata
   table. Predicate = census §h (P1 adjacency, P2 fall-through, P3 zero-xref+anon
   on true post-tracker.apply reloc set, P4 not-pdata-anchored), chain-extended.
   Gates: cargo test green (119+) + clippy clean; audit log per merge; 3× re-split
   byte-stable + matched-set stable; guard-5 doc-comment/pdata_anchored updated;
   matched-SET losses = 0 or individually explained (benign re-pairing only).
   Output `~/tmp/jeffleaf/verdict.md`.
4. **Landing:** by the parent coordinator only (this stream produces the
   patched clone/branch + verdict; nothing lands to jeff main or rb3-xenon
   main from here).

## Isolation discipline (enforced on all agents)

- jeff builds only in the clone `~/tmp/jeff-leaf-clone` — never in
  `../jeff` (shared live binary; wave-34 contamination precedent).
- rb3-xenon builds only in worktrees under `~/tmp` created by
  `scripts/setup_worktree.sh`, re-pointed via `configure.py --dtk` at the
  clone's binary (preserving setup_worktree's other explicit flags —
  "worktree dtk trap").

## Results

- **SongSortBy* classification (Agent B, DONE): NOT Class 2 — motivation
  retracted.** The SongSortBy{Rank,Song,Artist,Review,Recent} anon `fn_`
  residuals are anonymous, correctly-bounded, complete `func_type==3` EH
  functions (ctors/dtors/factories) missing a `target_symbol_map.json` entry →
  renamer leaves them `fn_XXXXXXXX` → objdiff can't name-pair → report scores
  them **0.0% unpaired** (NOT 99.9%; the "confirmed_twin fz=99.900" was a
  twin-finder heuristic artifact/transcription slip — decisive counter:
  `fn_8265DFF8` retail 152B = `NewSongNode` vs our compiled 272B body, can't be
  99.9%). Class-2 census over all 8 SongSort* units = **0** fall-through
  pdata pairs. **A Class-2 merge pass reclaims ZERO in these units.** Correct
  path for them is a `gen_game_target_map` identification wave + EH body-ports
  from the rb3-Wii oracle — a *different* workstream. SongStatusMgr/Track/
  TrackWidget are broadly under-identified (unmapped anon game fns + real named
  near-misses like TrackWidget::Poll 65%), also not boundary defects.
  Full detail: `~/tmp/jeffleaf/songsort-classification.md`.
- **Census (Agent A, DONE) — GATE CLEARED, but mechanism reframed.**
  Full detail: `~/tmp/jeffleaf/census.md` + `census-full.txt` (1,789 lines).
  - **The design doc's "raw `.pdata` over-split" characterization is WRONG.**
    The literal design-doc predicate (predicates 1-4 over consecutive `.pdata`
    entries) finds **0** candidates — the phenomenon is not in the pdata table.
    The AddRoll fragments sit in a `.pdata` **gap** as PDATA-LESS leaf
    functions carved by jeff's own CFA / `synthesize_reloc_targeted_leaf_functions`
    (a670a12) and persisted in symbols.txt. **The fix is the exact INVERSE of
    a670a12, in the same symbol layer** — a merge over the FINAL
    function-symbol set, NOT the pdata table.
  - **Refined symbol-level census: 1,789 candidate fragment pairs → 1,414
    merge-groups** (272 multi-pair chains, longest = 9 fragments). Filter
    cascade: 45,122 adjacent symbol pairs → 2,334 fall-through → 1,794
    zero-xref+anon → 1,789 not-pdata-anchored. 621 in pinned units, 1,168 in
    unpinned gaps. Used jeff's TRUE post-`tracker.apply` reloc set. 10/10
    hand-checks are unambiguous mid-function truncations (`lis` hi-half of an
    address load with the lo-half in S2; cond-branch INTO S2; store mid-body).
  - AddRoll fixture confirmed: TU5 `fn_826976E0`(16B)+`fn_826976F0`(20B),
    `lastS1=addi r11,r11,1` fall-through, `xrefS2=0`, merges to 36B in
    `band3/game/Stats.cpp`.
  - **Refined predicate (as-implemented in census, to reuse in the pass):**
    P1 `S1.addr+S1.size==S2.addr`; P2 last insn of S1 not
    `is_hard_flow_terminator`; P3 `S2.addr` zero incoming relocs (true
    post-`tracker.apply` set) AND `S2.name` anon (`fn_`/`lbl_`); P4 neither S1
    nor S2 in `obj.pdata_funcs` (a genuine pdata boundary is authoritative,
    never merge across it; subsumes `func_type==3`).
  - **Gate decision: PASS** (1,789 >> 10). Proceed to impl. Honest prize is
    still A/B-bounded: 1,789 is an upper bound on target-boundary fixes;
    strict-match gain depends on how many pinned units already have correct
    source (AddRoll = +1 confirmed). Residual risk: over-fire that MERGES a
    currently-matching standalone funclet → the whole-binary matched-SET A/B
    (losses=0-or-explained) is the acceptance gate.
- Isolation recipe VERIFIED (in census.md §"Verified isolation recipe"):
  clone `~/tmp/jeff-leaf-clone` @ c8b21dd, worktree
  `~/tmp/wt-jeffleaf-census`, `configure.py --dtk <clone> --objdiff <shared>
  --wrapper <shared>`, build.ninja has 0 refs to shared jeff. Both left in
  place for later stages.

- **Stage 2+3 (impl + A/B, DONE) — VERDICT GO.** Full detail
  `~/tmp/jeffleaf/verdict.md`. Deliverable = jeff clone branch `class2-merge`
  (`7e49a38` merge pass + `7f69b9e` guard-5 cross-ref doc, base `c8b21dd`);
  **independently confirmed** the branch/commits exist, the pass
  `merge_fallthrough_leaf_fragments` is wired (xex.rs L1563), and main
  `/home/free/code/milohax/jeff` is untouched at `c8b21dd`.
  - **A/B (converged, whole-binary matched-SET): +77 strict, 0 losses**
    (baseline 15207 → 15284, report.json `match_percent_normalized==100`).
    **AddRoll closed to 100%** (`fn_826976F0` absorbed into 36B `fn_826976E0`).
    1366 merge runs, 1727 fragments absorbed. `.pdata`/`except_data` unchanged
    (9368→9368; symbol-layer only). Function-symbol count −1727 (exactly the
    absorbed fragments).
  - **cargo test 131 passed** (+12 new merge tests incl. chain, cross-unit,
    protected), **clippy clean** in new ranges.
  - **Idempotent:** 3 re-splits → identical symbols.txt/splits.txt/matched-set
    (byte-stable fixed point at 15284).
  - **Predicate = census §h P1-P4 PLUS two guards the A/B loop surfaced:**
    - **P5 same-split-unit** — S2 must be in the same pinned split unit as the
      chain head. Without it the pass fused a gap fn into the next pinned unit
      (before MoveMgr.cpp) → hard dtk `"split ends within symbol"` BUILD
      FAILURE. AddRoll unaffected (both frags in Stats.cpp).
    - **P6 external-protect** — optional `JEFF_MERGE_PROTECT` map
      (`scripts/target_symbol_map.json`): never ABSORB a map-identified real
      function (chain head may still grow). Without P6 the structural predicate
      over-fires by exactly 2 losses (`MidiReader::_M_erase`,
      `BandCharDesc operator delete(void*,void*)`) — anon fragments the renamer
      names into real functions that coincidentally matched our shorter/
      ICF-folded codegen. **P6 → +77/0; no P6 → +70/−2.** The 2 losses are NOT
      the sanctioned benign re-pairing class, so P6 is required to pass the gate.
  - Pass is **always-on** (A/B gate `JEFF_CLASS2_MERGE` removed; always-on ==
    gated-on verified). `JEFF_MERGE_PROTECT` is NOT a gate — an optional
    correctness input that MUST be wired at landing.

## FINAL VERDICT + LANDING (for the parent coordinator)

**GO.** +77 strict / 0 losses, tests green, idempotent, AddRoll closed. Nothing
was landed from this stream — the parent lands after review.

Landing steps (order matters; observe the wave-34 staging discipline — the live
`../jeff/target/release/dtk` is shared fleet-wide):
1. **jeff source:** cherry-pick `7e49a38` + `7f69b9e` from
   `/home/free/tmp/jeff-leaf-clone` (branch `class2-merge`) onto
   `/home/free/code/milohax/jeff` main; `cargo build --release`; **gated atomic
   swap** of `../jeff/target/release/dtk` during a quiescent window (backup the
   old binary first); commit the jeff source immediately after the swap.
2. **Wire `JEFF_MERGE_PROTECT`** into rb3-xenon's dtk split edge in
   `configure.py`/`build.ninja` so the split runs with
   `JEFF_MERGE_PROTECT=<abs>/scripts/target_symbol_map.json`. **Load-bearing:**
   without it the pass is correct but over-fires by the 2 explained losses
   (net +70 instead of +77).
3. **Commit `config/45410914/symbols.txt` + `splits.txt`** (the merge changes
   them) in the SAME rb3-xenon commit as the match promotions.
4. Re-run one full split after landing to confirm the committed symbols.txt is
   at the converged fixed point (one settle round may be needed; matched set
   should read 15284).

Deliverable branch: `/home/free/tmp/jeff-leaf-clone` @ `class2-merge`
(`7e49a38`, `7f69b9e`). Verdict detail: `~/tmp/jeffleaf/verdict.md`.

Secondary/deferred (NOT this stream):
- The SongSortBy*/SongStatusMgr/Track anon-fn residuals are a SEPARATE workstream
  (`gen_game_target_map` identification wave + EH body-ports), NOT boundary
  defects — see `~/tmp/jeffleaf/songsort-classification.md`.
- Design-doc Classes 1 (terminatorless fragments) and 3 (stray func_type==3
  except_data) remain unaddressed; scope separately if desired. The 1,168
  unpinned-gap merge candidates also improve identification surface for future
  fingerprint waves once the pass lands.
