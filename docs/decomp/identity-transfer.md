# Per-function identity transfer for ICF-scattered TUs

**Status: PROVEN + TOOLED (2026-06-21).** `tools/identity_transfer.py` lands the
first real win: **RockCentral.cpp +17** (mechanism falsifier, measured whole-binary,
0 regressions). This doc is the capability spec; the tool is the implementation.

## The problem it solves

Our normal pinning model is **span-based**: one contiguous `.text [start,end)` per TU
in `config/45410914/splits.txt` → dtk carves it into one target `.obj` → objdiff pairs
target↔base by symbol name. This works for TUs the retail linker kept spatially grouped.

It **fails for ICF-scattered TUs** — game TUs whose methods the linker strewed binary-wide
with no contiguous cluster. Confirmed this class (all empirically refuted as span-pins this
session): **SongSortNode** (53 methods 0x8226f…–0x82b4af), **LockStepMgr** (Init@0x82598d80,
StartLock@0x82b7f3b0…), **BandProfile** (104 methods 0x822639F0..0x82BD66B0, largest run 3),
**SongSort**, **MainHubPanel** (44/45 ICF aliases). Span-pinning them mints **fake matches**
(tiny ≤44B stubs ICF-fold byte-identically across TUs — see the wave-14 +57 refutation and
`docs/plans/decomp-state-and-roadmap-2026-06-09.md` WAVE-14 CLOSE).

But the **rb3-Wii BinDiff oracle** (`unified_id_rb3wii.json`) knows each method's VA + name.
Identity transfer carves each scattered method into its TU obj by its individual VA.

## The mechanism (verified against jeff/dtk source)

Multi-range per-TU pinning works for **arbitrary N** — there is no per-range or per-TU limit
in jeff. The carve path (jeff `src/cmd/xex.rs`, `config.rs`, `split.rs`):

1. **Scaffold+compile** — the TU must be wired (`objects.json` NonMatching) so the build emits
   a compiled BASE obj that *defines* each method's MSVC-mangled symbol.
2. **Micro-pin** — append one `.text start:0xVA end:0xVA+pdata_len` line per scattered method
   under one `Foo.cpp:` header. `apply_splits` does a RAW `ObjSplits::push` (not `add_split`),
   so N same-unit ranges **accumulate as independent splits, never auto-merged** — the literal
   N-range generalization of the working 2-range `Part.cpp`/`PropKeys.cpp` pins.
3. **Carve** — `split_obj` groups by unit *name*; all same-named fragments **concatenate into
   one target `.text`**. `create_gap_splits` auto-owns the bytes between micro-ranges (`auto_*`
   units), so the 100%-section-coverage invariant holds without pinning the gaps.
4. **Name** — `gen_game_target_map.py` maps oracle `VA → mangled`, matched against the TU's
   compiled defined symbols; the renamer renames target `fn_<VA> → mangled`; objdiff pairs.

**Governing constraint:** each micro-range start AND end must land exactly on a function-symbol
boundary or `validate_splits` bails. Use `[VA, VA + pdata_len)` — `.pdata` is a clean `.text`
partition, so this never clips. dtk auto-derives the matching `.pdata` micro-entries.

## Case-A vs Case-B (the ATTRIBUTION_ORPHAN resolution)

- **Case-A — method sits in an UNOWNED `auto_*` blob.** Micro-pinning carves it cleanly into
  the TU obj. **Works with current tools, no fork.** This is the bulk (RockCentral: 104/129).
- **Case-B — method physically sits INSIDE another TU's pinned span.** objdiff pairs *within a
  unit*, so it can't pair against our compiled obj. Micro-pinning would either overlap (build
  break) or require **evicting/shrinking the foreign span** (regresses that TU). The tool
  **SKIPS case-B to a deferred worklist** (`/tmp/idtransfer_caseb_<tu>.json`). A future
  **optional objdiff fork** (a global byte-equality second pass — `objdiff-core diff/mod.rs`
  `pair_funclets_by_bytes` extended to match an unmatched named target fn against a byte-identical
  base symbol in *any* unit) would let case-B count by map-name only. Invasive; not built.

## The tool — `tools/identity_transfer.py`

410 LOC, ~80% reuse of `pin_identified` primitives (covering_pin case-A/B discriminator,
size loading). `--tu Foo.cpp --oracle unified_id_rb3wii.json [--apply]` (dry-run default).
- Reads the TU's scattered methods from the oracle; classifies case-A vs case-B.
- Coalesces contiguous case-A runs, emits the `.text` micro-range block + the `target_symbol_map`
  entries. **STRICT ADD-ONLY** on the map — never overwrites an existing key (protects prior-wave
  names; ICF collisions keep-existing). Never wholesale-regen (that is POISON).
- Fail-closed: a method whose `symbols.txt` size bisects another symbol is rejected (sizing-data
  limitation, not a mechanism failure).

## Honesty (carries over)

A scattered method counts **only if it byte-matches** (own real body, not an ICF-stub
coincidence). The tool's add-only collision-skip + per-method oracle attribution handle much of
this, but STL-heavy clusters (BandProfile's `vector<PatchDir*>`/`hash_map<Symbol,Lesson*>`) are
≤44B-stub-dominated — the **byte-match ≠ ownership** rule still applies. Verify a generalized run
the same way: per-unit A/B + confirm the newly-100 are real-bodied named methods, not stub folds.

## Generalization queue (gated on source-port cost, not the mechanism)

The transport is TU-agnostic; the cost is porting the MWCC source so the obj *defines* the methods.
- **BandProfile.cpp** — 70 case-A methods (the documented next target; 1013-line MWCC source, heavy
  meta_band includes = multi-hour port).
- **SongSortNode.cpp / SongSort.cpp**, **LockStepMgr.cpp**, **MainHubPanel.cpp** — same pattern.

Run order per TU: port-then-wire (objects.json NonMatching) → `identity_transfer.py --tu X --apply`
→ overlap self-check → composed verify → honesty per-unit A/B.
