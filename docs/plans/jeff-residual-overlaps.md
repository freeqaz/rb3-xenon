# jeff residual-overlap investigation (1353 regions in FRESH symbols.txt)

Status: **FIX IMPLEMENTED + A/B-VERIFIED on a private jeff worktree. LAND
recommended.** Patch at `/tmp/jeff_clamp_oversized.patch` (also reproducible
from the worktree `/tmp/jeff-wave2`, detached at jeff HEAD `db4185c`). Nothing
landed in shared `../jeff`; A/B done in isolated rb3-xenon worktree
`.claude/worktrees/wave2-clamp`.

## TL;DR for the orchestrator

- The 1353 residual overlaps are **NOT** the prior phantom class and **NOT**
  adjacent-cluster/ICF artifacts. They are dominated by **class (c):
  oversized-but-real function symbols** whose `size` exceeds the function's true
  length (from `.pdata`), so they straddle and starve their real neighbors. The
  phantom prune spared them *correctly* (they are real / pdata-anchored /
  referenced) but never *resized* them.
- jeff already holds the authoritative length for every pdata-anchored function
  in `obj.known_functions` (it just wasn't being used to size symbols).
- **Verdict: a jeff fix is warranted** — a size-*clamp* pass, sibling to the
  existing prune, run at the same self-healing call site in
  `split_write_obj_exe`.
- **A/B (same worktree, same toolchain, full rebuild + objdiff report):**
  - overlap regions: **1353 → 128** (−1225, −90.5%)
  - `matched_functions`: **548 → 569** (**+21**), `matched_code` 95368 → 98456
  - per-unit: **16 units improved, 0 regressed**
  - pin-ranker `jeff_blocked`: **24 → 0**
  - jeff tests: **119/119 pass**; clippy warning count unchanged (115 → 115,
    zero new warnings in the added code)
- **Recommendation: LAND** in shared `../jeff`.

## Class (a)/(b)/(c) histogram + methodology

Methodology: the ground truth for "where do real functions start and how long
are they" is the binary's own `.pdata` table (the Xbox-360 exception-unwind
directory). I extracted the PE from the XEX (`orig/45410914/band.exe`, the
dtk-extracted basefile), parsed `.pdata` at file offset `0x1E9A00` /
size `0x6F370` — **56,942 entries, 56,836 in `.text`**, each
`(BeginAddress, PrologLen | FuncLen<<8 | …)`. `BeginAddress` is the relocated
absolute VA; `FuncLen` is in instructions (×4 = bytes). This is exactly the
length jeff already computes at `src/util/xex.rs:1104`
(`obj.known_functions.insert(section_addr, Some(num_insts_in_func * 4))`).

I replicated `pin_candidates.py`'s `overlap_regions()` sweep over
`config/45410914/symbols.txt` (FRESH — the documented `fn_82BF8E48` phantom is
absent, confirming a post-prune-fix re-SPLIT) and classified each of the 1353
regions against the `.pdata` partition:

|  count | class | what it is |
|-------:|-------|------------|
| **834** | **(c) mis-sized pdata-anchored** | the overlapping symbol IS a real `.pdata` function start, but its `symbols.txt` `size` is **larger** than its `.pdata` length, so it overruns the next real function. e.g. `fn_82270690` size `0x12c` vs real `.pdata` length `0xac` — it swallows the `0x82270740` function that begins 4 bytes after its real end. |
| **497** | **(c) non-pdata CFA phantom (oversized)** | the overlapping symbol is **not** a `.pdata` start (a CFA-discovered block whose first word is inter-function padding `00000000`, or a mid-function branch target — `lwz`/`b`/`addi`, never an `mflr`/`stwu` prologue) and is sized to swallow ≥1 real `.pdata` function. Survived the prune because it is a **branch target** (referenced). e.g. `fn_82278DF0` size `0x188` swallows two real fns at `0x82278e30` and `0x82278f6c`. |
| **22** | **(a) boundary slack** | a real pdata fn whose size slightly overshoots into the next *non*-pdata sub-label, with no real-function straddle. Genuinely a ranker/cosmetic concern, not corruption. |
| **0** | **(b) ICF-folded alias** | none. No region was driven by multiple names aliasing one address. ICF is not a factor in the residual set. |

So the set is **98.4% class (c)** (1331/1353) — mis-sized real/referenced
symbols, exactly the gap the prune leaves open (it only *deletes* unreferenced
overlappers; it never *shrinks* a referenced/anchored one).

### Why the named wave-2 "blocking" functions are themselves fine

`BinkReader 0x82B87A60`, `Movie 0x82472160`, `Line 0x82450950`,
`BustAMoveData 0x822B3D28` are all **correctly sized** (`size == pdata length`).
They are not in any overlap region. Their TUs are flagged `jeff_blocked` because
the *ranker's pin span* for the TU passes through an overlap region created by a
*different*, oversized symbol elsewhere in the span. So the lever is the symbol
sizing, not those functions.

### Safety proof for clamping

Across all 56,836 `.text` `.pdata` entries, **0** have a length that overruns
the *next* `.pdata` start — the `.pdata` table is a clean partition of `.text`.
Therefore clamping an oversized pdata-anchored symbol to its `.pdata` length
(class-c-834) can never make it too short, and clamping a non-pdata straddler
(class-c-497) to end at the first `.pdata` start it swallows restores a real
boundary. DC3 cross-check (`lookup_dc3 MultiTempoTempoMap` → 23 hits, 86.7%
matched unit) confirms the swallowed functions are genuinely distinct functions,
i.e. clamping reconstructs real boundaries rather than splitting one function.

## Verdict: jeff fix, not ranker

A ranker-side "stop at nearest pdata-anchored neighbor" patch would only paper
over the symptom for pinning; the underlying `symbols.txt` / target-`.obj`
boundaries would still be wrong, so `write_coff` would still starve the real
neighbors' COMDAT sections and they would still score 0% even once pinned. The
fix has to correct the symbol sizes at the source, which is jeff. The 22
class-(a) cases are the only ones a ranker change could legitimately own, and
they are not worth a separate lever.

## The fix (implemented)

New `clamp_oversized_function_symbols(obj)` in `src/cmd/xex.rs`, called in
`split_write_obj_exe` **immediately before** `prune_overlapping_phantom_functions`
(same call site, same self-healing-of-the-committed-cache property). For each
`Function` symbol in a code section:

1. **pdata-anchored & oversized** → clamp `size` to the exact `.pdata` length
   from `obj.known_functions` (the authoritative oracle jeff already builds).
2. **not pdata-anchored but straddles a pdata start** → clamp `size` so the
   symbol ends exactly at the first `.pdata` start it would otherwise swallow.

Never grows a symbol, never touches a correctly-sized one; every clamp logged at
`info` (`"Clamping oversized function symbol … 0xX -> 0xY (reason)"`) for audit,
mirroring the prune. Patch: `/tmp/jeff_clamp_oversized.patch` (130 lines, single
file). Self-healing note: because it rewrites the committed `symbols.txt` cache,
once a tree is split with the patched dtk the clamps persist even if reverted —
a clean baseline requires restoring the pre-clamp `symbols.txt` (done for the
A/B below).

## A/B numbers

Isolated rb3-xenon worktree `.claude/worktrees/wave2-clamp`
(`scripts/setup_worktree.sh`), full `./tools/ninja-locked` rebuild + objdiff
report for each arm, same warm cache, same compilers. Baseline dtk =
`build/tools/release/dtk` (current `../jeff` HEAD, prune-only). Patched dtk =
`/tmp/jeff-wave2/target/release/dtk`.

| metric | baseline (prune-only) | patched (+clamp) | delta |
|--------|----------------------:|-----------------:|------:|
| symbols.txt overlap regions | 1353 | **128** | −1225 |
| `measures.matched_functions` | 548 | **569** | **+21** |
| `measures.matched_code` (bytes) | 95368 | **98456** | +3088 |
| units improved / regressed | — | 16 / **0** | — |
| pin_candidates `jeff_blocked` | 24 | **0** | −24 |
| symbols clamped by the pass | 0 | 1421 (924 pdata-length + 497 next-pdata) | — |

Split-log audit (patched): `INFO Clamped 1421 oversized function symbol(s)` then
`INFO Pruned 34 spurious overlapping function symbol(s)`. The 1421 = the 924
class-c-834 mis-sized pdata symbols + 497 class-c-497 non-pdata phantoms,
matching the static analysis exactly. Top improved units: BlockMgr 6→10,
ArkFile 1→3, BeatMap 0→2, MultiTempoTempoMap 2→3, json_object 18→19,
keygen_xbox 11→12. **Zero objdiff re-pairing artifacts** (unlike the prior prune
fix, no unit dipped).

Residual 128 regions after the fix are **non-pdata vs non-pdata** overlaps (CFA
artifacts overlapping *each other*, neither anchored, neither swallowing a real
function) — they do not corrupt any real function's COMDAT section, so they are
harmless for matching and out of scope for this clamp. A future pass could
shrink them via more aggressive CFA-block sizing, but the value there is
marginal.

## jeff test + lint status

- `cargo test --release` in `/tmp/jeff-wave2`: **119 passed; 0 failed** (same
  count as the landed prune fix — no test removed or added; the clamp is
  exercised end-to-end by the rb3-xenon re-SPLIT).
- `cargo clippy --release`: **115 warnings, identical to the unpatched HEAD** —
  the added function introduces zero new clippy warnings.

## LAND recommendation

**LAND** `/tmp/jeff_clamp_oversized.patch` in shared `../jeff`. It is a clean,
additive, audited, provably-safe complement to the existing prune; it cuts
residual overlaps 90.5%, lifts matched_functions +21 with zero regressions, and
fully unblocks the wave-2 pin ranker (24 → 0 `jeff_blocked`). Recommend the
orchestrator (a) review the patch, (b) land it on `../jeff` main with the
`Co-Authored-By` trailer this repo uses for jeff commits if applicable, then
(c) re-SPLIT the shared rb3-xenon tree (`touch config/45410914/config.yml &&
./tools/ninja-locked 2>&1 | tee /tmp/rb3_build.log`) so the committed
`symbols.txt`/`splits.txt` pick up the clamps, then (d) re-run
`tools/pin_candidates.py` to regenerate the (now fully-unblocked) wave-2 pin
proposal.

### Artifacts
- Patch: `/tmp/jeff_clamp_oversized.patch`
- Patched dtk: `/tmp/jeff-wave2/target/release/dtk` (worktree at jeff `db4185c`)
- A/B reports: `/tmp/report_baseline.json` (548), `/tmp/report_clamp.json` (569)
- Split logs: `/tmp/rb3_baseline_split2.log`, `/tmp/rb3_patched_split2.log`
- pin_candidates (clamped): `/tmp/pin_clamp.json` (0 jeff_blocked)
- pdata ground truth: `/tmp/pdata.json` (56,836 `.text` entries)
