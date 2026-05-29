# Game-code pairing pipeline + full matched-function delta

**Date:** 2026-05-29. **Status:** tooling built, measured end-to-end.
**Worktree:** `.claude/worktrees/pair-pipeline` (isolated; not committed).
**Build:** clean re-SPLIT + compile via `./tools/ninja-locked` (log `/tmp/pair_pipe.log`).

## TL;DR (the honest numbers)

- Built **`tools/gen_game_target_map.py`** — generates `scripts/target_symbol_map.json`
  entries that pair each located game-TU target `fn_<addr>` to the MSVC-mangled
  name of the matching function in our compiled obj, by **demangle-and-match**
  on `class::method` (+ arg-count tiebreak).
- **Paired 58 game functions across 8 high-purity meta_band TUs** (7 already
  pinned at candidate spans + 1 newly pinned, AccomplishmentProgress narrowed).
- **Matched-function delta: +0** (worktree baseline 1593 → 1593). **Zero game
  functions byte-match retail.** The Wii-ported source is *not* byte-faithful.
- **But pairing succeeded** — the 8 game units now report real fuzzy %
  (11.8%–42.5% per unit) where before pinning gave 0.0% fuzzy (no pairing, no
  comparison). The diagnostic value is unlocked.
- **Verdict: game matching here is MOSTLY-FIDELITY, not mostly-pairing.** Of the
  58 functions *I* paired, **0 are ≥90% near-misses** — they cluster at
  **1–49% fuzzy** because retail's accessors are structurally different from the
  Wii dev source (a systematic instrumentation-counter inline; see §Sample diffs).
  This is per-function source work, *not* permuter fodder.

## Pipeline design

### The problem (restated, confirmed)

objdiff pairs the dtk-SPLIT **target** obj (`build/45410914/obj/<TU>.obj`,
anonymous `fn_<addr>` symbols) to our compiled **base** obj
(`build/45410914/src/band3/meta_band/<TU>.obj`, MSVC-mangled symbols) by
*symbol-name equality*. For game TUs the names never coincide, so no pair forms
and no match (or fuzzy %) can register even when bytes agree. Pinning a game
TU's `.text` therefore yields +0 and **0.0% fuzzy** — invisible.

### How engine TUs already pair (verified)

`configure.py` `custom_build_steps["pre-compile"]` runs
`scripts/obj_target_symbol_renamer.py --batch --apply` after SPLIT and before
the report step. The renamer rewrites **target-side** `fn_<addr>`/`lbl_<addr>`
symbols to MSVC-mangled names from `scripts/target_symbol_map.json`
(`{ "0xADDR": "<mangled>" }`). Once renamed, objdiff auto-pairs the top-level
symbol *and* the relocations that reference renamed callees. The engine side
relies on this renamer (the map already held 9,438 engine entries); it does
**not** rely on objdiff byte-pairing (objdiff finds 0 identical pairs across
the anonymized boundary). So the renamer is the universal pairing mechanism;
our job for game code is to **populate map entries for game addresses**.

### The generator: `tools/gen_game_target_map.py`

Inputs: the RB3-Wii BinDiff oracle `unified_id_rb3wii.json` (9,301
`rb3_addr → wii_name` game-code identifications), `/tmp/candidate_spans.json`
(per-TU dominant `.text` span + purity), and the compiled objs.

**Demangle-and-match (avoids the fragile inverse re-mangling):**

1. For each high-purity (≥0.70) meta_band TU, collect the oracle entries whose
   `bindiff_src` names that TU.
2. Parse each `wii_name` (Ghidra-demangled, e.g. `Accomplishment::GetType()_const`)
   into `(class, method, argcount)`. Ghidra renders spaces as `_` and appends
   `_const`; both are stripped. No `::` ⇒ free/file-static function (`class=None`).
3. Parse the *defined* (section-number > 0) MSVC-mangled function symbols in the
   compiled obj into `(class, method)` by decoding the mangling **head**:
   `?Method@Class@@…` → `(Class, Method)`; `??0Class@@…` → ctor; `??1Class@@…`
   → dtor; `?Name@@Y…` → free function `(None, Name)`. (No full demangler
   needed — the head is trivially parseable; arg-types are irrelevant for the
   key.) External references and engine inlines pulled in as *undefined*
   symbols are excluded.
4. Match by `class::method`. Same-name overloads disambiguate by arg count
   (best-effort: `…XZ` ⇒ 0 args; otherwise left to class::method only). Wii-vs-360
   type-width differences (`int`/`long`, ref/ptr) don't matter — we key on
   class::method + arity, not exact types.
5. Class-method entries are scoped to the TU's **dominant class set**, so a
   neighbor-TU function interleaved in a low-purity span is rejected (it has a
   foreign class). Free functions are TU-local and exempt.
6. Emit `{ "0xADDR": "<mangled>" }`; `--apply` merges into the map (idempotent;
   re-run = 0 new).

```
# generate + merge meta_band game entries (purity bar 0.70)
python3 tools/gen_game_target_map.py --purity 0.70 --area meta_band --apply
# single TU / dry-run / custom span file
python3 tools/gen_game_target_map.py --tu UIEvent.cpp
python3 tools/gen_game_target_map.py --tu AccomplishmentProgress.cpp --spans /tmp/spans_narrowed.json --apply
```

The tool is the **production, reusable** piece: point it at any game area
(`--area band3`, `--area network/net`, `--area ""` for all) once those TUs are
ported + pinned, and it back-fills pairing entries.

## What was paired (58 functions, 8 TUs)

| TU | oracle-in-TU | **paired** | noObj | pinned span |
|---|---|---|---|---|
| Accomplishment.cpp | 39 | **20** | 2 | 0x8243A18C–0x8243A614 (pre-existing) |
| AccomplishmentProgress.cpp | 118 | **14** | 4 | 0x82439DF4–0x8243A12C (**newly pinned, narrowed**) |
| UIEvent.cpp | 17 | **8** | 0 | 0x825519DC–0x82551B5C (pre-existing) |
| AccomplishmentSetlist.cpp | 5 | **4** | 1 | 0x8243F220–0x8243F330 (pre-existing) |
| AccomplishmentSongFilterConditional.cpp | 17 | **4** | 0 | 0x8243F378–0x8243F418 (pre-existing) |
| AccomplishmentCategory.cpp | 5 | **3** | 0 | 0x8243EF98–0x8243EFF8 (pre-existing) |
| ContextChecker.cpp | 10 | **3** | 0 | 0x826D1558–0x826D1E68 (pre-existing); 3 **free fns** |
| AccomplishmentPlayerConditional.cpp | 12 | **2** | 2 | 0x8243F178–0x8243F220 (pre-existing) |
| **TOTAL** | | **58** | | |

`noObj` = oracle named the function but it is absent from our compiled obj under
that name (inlined by `/O1 /Ob2`, or compiled as a `?A0x…`-decorated static, or
renamed in the Wii→360 port). That is a fidelity signal, not a tool bug.

### TUs intentionally NOT pinned (span conflict with engine pins)

- **AccomplishmentManager.cpp** — its entire 13-fn dominant cluster
  (0x8248BBA4–0x8248CA38) sits **inside** the already-pinned engine TU
  `EventTrigger.cpp` (0x82489D38–0x8248EB80). Can't pin without conflicting.
- **AccomplishmentProgress.cpp** — the raw candidate span (0x8243854C–0x8243A12C)
  overlaps the pinned engine TU `Part.cpp` (0x82433AB0–0x82439D8C). But 18 of 19
  of its functions form a tight run **after** Part.cpp's end; I pinned that
  narrowed span (0x82439DF4–0x8243A12C, 85.7% purity, snaps to symbols.txt) —
  a clean win with no engine conflict. (The one 0x8243854C outlier deep inside
  Part.cpp was a noise match.)

**Finding for the orchestrator:** `Part.cpp` and `EventTrigger.cpp` are pinned
across address ranges that the RB3-Wii oracle attributes to *game* TUs
(AccomplishmentProgress, AccomplishmentManager). Either those engine pins are
too greedy (claiming game-code bytes) or the oracle aliased a few functions.
Worth a cross-check — if the engine pins are wrong, AccomplishmentManager
(+13 fns) and the front of AccomplishmentProgress become pinnable.

`PatchPanel.cpp` (a candidate per `bindiff-vs-rb3wii.md`) is currently pinned at
0x8260DDD0–0x8260E290 — a span with **0** PatchPanel oracle entries (the game
cluster is at 0x825FA3E8). Its compiled obj is also absent from the build cache.
Left untouched (don't disturb the existing pin); needs separate investigation.

## Measured result: matched-function delta and fuzzy distribution

**Worktree baseline (pre-pairing): `matched_functions = 1593`.**
(NB: the task brief cited 572; the reflinked build cache in this worktree is
warmer — 1593 — so the honest delta is measured against 1593.)

**After pairing + AccomplishmentProgress pin: `matched_functions = 1593`. Delta = +0.**

The 16 functions in the report's "Game Code" category (0.46% matched) are all
pre-existing crypto/util matches (`keygen_xbox` ×12, `Memory_Xbox` ×4) — **none
of the 8 meta_band TUs contribute a byte-match.**

### Per-unit fuzzy (the payoff: pairing now lets these be measured)

| unit | funcs | matched | unit fuzzy% |
|---|---|---|---|
| UIEvent | 12 | 0 | **42.5** |
| AccomplishmentProgress | 25 | 0 | 38.6 |
| AccomplishmentCategory | 3 | 0 | 33.3 |
| AccomplishmentPlayerConditional | 5 | 0 | 30.4 |
| Accomplishment | 35 | 0 | 26.5 |
| AccomplishmentSetlist | 8 | 0 | 22.9 |
| AccomplishmentSongFilterConditional | 5 | 0 | 21.0 |
| ContextChecker | 3 | 0 | 11.8 |

### Per-function fuzzy distribution

**The 58 functions I paired (mangled names, the real signal):**

| bucket | count |
|---|---|
| 100% (match) | 0 |
| 90–99% | 0 |
| 70–89% | 0 |
| 50–69% | 0 |
| 30–49% | 7 |
| 1–29% | 50 |
| 0% | 1 |

There is a *separate* set of **16 functions objdiff auto-fuzzy-paired at 90–99%**
inside these objs — but those have **no oracle entry** and are tiny (40-byte)
boilerplate/compiler-generated stubs that structurally collide with unnamed
target functions; they are not game logic and are not part of the pairing work.

## Sample diff characterizations

All sampled paired functions show the **same structural divergence**, not
regswap/layout noise:

- **`Accomplishment::GetName` (35% fuzzy, highest paired):** target 32 bytes does
  `stwu; lis r11,lbl_82C90838; lwz r11,lbl_82C90838; rlwinm; lis r10,lbl_82C90838;
  stw r11,lbl_82C90838` — a **read-modify-write of a global at `lbl_82C90838`**
  inlined into the accessor. Our base is 12 bytes: a plain `lwz/stw` field copy
  (`lwz r11,0x4,r4; stw r11,0x0,r3`). 5 deletes + 2 diff_arg.
- **`AccomplishmentProgress::GetNumCompleted` (23% fuzzy):** identical pattern —
  target 32 bytes touches the same `lbl_82C90838` global; base is an 8-byte
  `lwz r3,0x68,r3; blr` trivial getter. 6 deletes.
- The recurring `lbl_82C90838` global write across many retail `Get*` accessors
  is a **systematic instrumentation/accessor-tracking counter that retail
  inlined into every getter** — the Wii *dev* decomp source has no such code.

This is the root cause of the low fuzzy: the Wii source skeleton for the simple
accessors (the functions the oracle most confidently identifies) is genuinely a
different shape than retail. It is a **source-fidelity gap**, addressable only by
editing the source to reproduce retail's inlined-global behavior — *not* by the
permuter (the diff is missing-implementation, not register/stack permutation;
objdiff's own verdict on each was "Likely missing implementation or wrong
skeleton … do not run the permuter yet").

## Assessment: how close is the ported game source to retail?

**Not close, for the accessor surface the oracle identifies best.** Concretely:

- **Pairing is solved** (this pipeline). Any located+pinned game TU can now be
  paired automatically; the renamer + map make matches *able* to register.
- **Fidelity is the wall.** The ported `meta_band` source compiles and is
  structurally recognizable (11–42% per-unit fuzzy), but **0/58 functions
  byte-match** and **0 are ≥90% near-misses**. The dominant divergence is a
  retail-only inlined global (`lbl_82C90838`) in the accessor family — a real
  behavioral/source difference between the Wii dev build and Xbox retail, not
  codegen noise.
- **Implication for the band3 pin wave:** pinning more game TUs will keep adding
  +0 matched until the *source* is brought to retail fidelity. The win this
  unlocks is **measurement** — fuzzy % per function now tells you exactly which
  game functions are close (none, here) vs far (most), turning the meta_band
  decomp into a tractable per-function source-fidelity backlog instead of a
  black box. The 7 `noObj` functions (named by oracle, absent from our obj) and
  the `lbl_82C90838` pattern are the first concrete leads.

## Files / artifacts (in this worktree)

- **`tools/gen_game_target_map.py`** — the generator (new).
- **`scripts/target_symbol_map.json`** — +58 game entries merged (9438 → 9496).
- **`config/45410914/splits.txt`** — +1 pin (`AccomplishmentProgress.cpp`
  `.text 0x82439DF4–0x8243A12C`).
- `build/45410914/report.json` — post-build report (matched 1593, Game Code
  16/422).
- `/tmp/pair_pipe.log` — build log. `/tmp/pre_apply_map.json` — pristine map
  backup. `/tmp/spans_narrowed.json` — narrowed-span input for
  AccomplishmentProgress.

## Reproduce

```bash
cd .claude/worktrees/pair-pipeline
python3 tools/gen_game_target_map.py --purity 0.70 --area meta_band --apply   # 7 pre-pinned TUs
python3 tools/gen_game_target_map.py --tu AccomplishmentProgress.cpp \
    --spans /tmp/spans_narrowed.json --purity 0.70 --area meta_band --apply    # narrowed pin
touch config/45410914/config.yml && ./tools/ninja-locked 2>&1 | tee /tmp/pair_pipe.log
# read build/45410914/report.json measures.matched_functions + per-unit fuzzy
```
