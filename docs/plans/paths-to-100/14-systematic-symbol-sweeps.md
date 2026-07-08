# Systematic sweeps — local-static-Symbol, guard thunks, and other one-pattern-many-functions fixes

Status: DRAFT-RFC | Date: 2026-07-08 | Author: Claude Opus (paths-to-100 wave) | Theme: body-divergence

## Summary

Two of the last two grind close-outs were the *same* fix: a function-local
`static Symbol sym("lit")` where our source used a global `Symbols.h` reference.
That single edit closed 2 functions AND paired 20 static-init guard/atexit thunks
(+22 matched, 0 regressions, commit `3342b30`). This RFC proposes a **mechanical
scanner** that enumerates every sub-100% function whose *only* remaining
divergence matches a known one-pattern-many-functions signature — starting with
local-static-Symbol, extending to the guard/atexit-thunk and `Symbols.h`-global
families — and emits a per-function worklist so the fix is applied by inspection
rather than rediscovered per function.

## Motivation

The project's own record is explicit that this pattern repeats. From the grind
close-out (`docs/plans/grind-loop-calibration-2026-07-07.md`, "Close-out proof"):

> Both closes were the same fix: retail band3 emits function-local
> `static Symbol sym("...")` (target relocs `?sym@?N??Fn@Class@@...@4V3@A` +
> `$S` guards), where our tree/oracle uses global `Symbols.h` refs. This directly
> attacks the known "guard-thunk wall" … **Likely systematic across band3 — a
> sweep candidate.**

The economics of *finding* the afflicted functions dominate. The grind loop found
these two by expensive best-of-N LLM sampling (`docs/plans/grind-loop-calibration-2026-07-07.md`
§"The real lever is VARIANCE"); the actual *fix*, once seen, is a 2-line source
edit that a cold agent applies in a minute. If N functions share the signature,
a scanner converts an O(N) discovery cost into O(1) discovery + O(N) trivial
edits. This is the highest-leverage shape of work in a decomp: **one insight,
many mechanically-applied instances.**

The obj patchers in `scripts/` (`obj_dynamic_init_patcher.py`, `obj_guard_patcher.py`,
`obj_bool_mangle_patcher.py`, `obj_atexit_scope_patcher.py`, `obj_anon_ns_patcher.py`)
already exist for this family — **but they patch NAMING** (COFF symbol storage
class / mangling) so objdiff can *pair* symbols. They do not change the emitted
**code**. When retail's function-local static changes the actual instruction
stream (the guard load, the `$S` init-once branch, the extra `??__E` initializer
thunk in `.text`), no symbol-table patch recovers the bytes — the *source* has to
emit the same construct. That's the gap this RFC fills: a **code-level** sweep,
downstream of the naming patchers.

## Current state (verified)

All figures verified 2026-07-08 against `build/45410914/report.json` and
`tools/fuzzy_progress.py` on main.

- **STRICT: 11,240 / 65,619 functions (17.13%); 962,656 / 11,074,108 code bytes
  (8.69%).** (`report.json measures`.)
- **Near-miss band [90,100): 1,821 functions** (computed from report.json
  fuzzy_match_percent). Of those: **280 band3, 2 network, 1,539 engine/other.**
- **Staircase (fuzzy_progress.py):** `>=95: 12,743 | >=90: 13,021 | >=80: 13,175`.
  Histogram: `[95,100): 1,503 | [90,95): 278 | [80,90): 154 | [50,80): 232`. The
  1,503 in `[95,100)` is the primary hunting ground — closest to closing.
- **The proven fix (commit `3342b30`):** `git show 3342b30` — `SavedSetlist.cpp`
  and `CampaignLevel.cpp`, each gaining `static Symbol name("literal");` at
  function scope. `SetlistTypeToSym` 16.4→100, `CampaignLevel::Configure`
  25.1→100; whole-binary A/B **+22 / 0 regressions** (2 fns + 20 guard/atexit
  thunks). Fuzzy-salvage of `CharClipGroup` (`d696b52`) is a *different* lever
  (micro-pinning), not this one.

### What the signature actually looks like (verified by obj inspection)

I scanned the COFF symbol tables of both sides:

- **Our compiled objs** (`build/45410914/src/**/*.obj`, n=801): **547 already
  emit local-scope mangled symbols** matching `@\?[12N]\?\?` (e.g.
  `?name@?1??StaticClassName@Object@Hmx@@SA?AVSymbol@@XZ@4V4@A` in `Object.obj`,
  `MasterAudio.obj`, etc.). Most are the `StaticClassName` macro, i.e. the pattern
  is *already pervasive* where our source follows retail. This is the "good" side.
- **The dtk SPLIT target objs** (`build/45410914/obj/*.obj`, n=3,091): **ZERO
  match** the mangled-name signature. **This refutes a naive scanner design.** dtk
  leaves local-static data slots as anonymous `lbl_`/`fn_` placeholders (confirmed
  by the grind doc's "Gotcha" note and by my scan returning 0/3091). **You cannot
  read the retail mangled name off the target obj.** The signal must be
  *inferred*: our side has a data relocation to a **global** `Symbols.h` symbol
  where the target has a relocation to an **anonymous local slot inside the same
  function's data**, plus a paired `$S`/`??_B` guard slot the target has and we
  don't.

This is the load-bearing correction to the brief. The brief says "scan ALL target
relocs/symbols for … local-static `@?1??` mangling in the retail obj" — **that
data is not present in the dtk target obj.** The scanner keys on the *diff*
(objdiff relocation-target mismatch), not on a target-side name grep.

### Source-side prevalence (verified, cheap sample)

Of the **48 band3 units** that contain a near-miss `[90,100)` function, **11
already use** the `Symbol foo("literal")` function-local form; the other 37 either
use globals, don't have their source resolved, or use Symbols a different way.
That 37 is a loose upper bound on band3 units *possibly* afflicted — not a
per-function count (a unit can have the pattern in one fn and be near-miss for an
unrelated reason). The realistic afflicted-function count is well below the 280
band3 near-miss total; see "Effort & expected value".

## Proposal

A read-only scanner, **`tools/symbol_sweep_scan.py`**, that enumerates sub-100%
functions matching each of a small set of one-pattern-many-functions signatures
and emits a ranked, per-function worklist keyed by pattern. Design in three parts.

### Part A — data source: objdiff relocation-mismatch JSON

The scanner does **not** grep target objs for names (proven empty). It builds on
the objdiff JSON the grind scorer already produces. From
`docs/plans/grind-loop-calibration-2026-07-07.md` §"What's in scripts/grind/",
the proven invocation is:

```
../objdiff/target/release/objdiff-cli diff -p . '<mangled_sym>' \
    --build -f json --verdict --include-instructions
```

(last stdout line = JSON). `--include-instructions` carries, per instruction, the
relocation target on each side (base = our obj, target = retail). The scanner
walks these and classifies each **mismatched instruction pair** by its relocation
delta. Reuse `scripts/analysis/diff_inspect.py::run_objdiff_for_symbol`
(line 1693) and its normalization helpers rather than re-parsing raw objdiff.

### Part B — signature classifiers (the "many patterns" set)

For each sub-100% function, classify its residual mismatches. A function is a
**candidate** only if its mismatches are *dominated* by one signature (so the fix
is likely to close it, not merely improve it):

1. **`local-static-Symbol`** (primary). Base side has a data-reloc to a symbol
   that (a) is EXTERNAL/global and (b) resolves to a `Symbols.h`-style global
   `Symbol` (grep the resolved source symbol name against `src/**/Symbols*.h` and
   for global `Symbol gFoo;`), while the target side has a reloc into an
   **anonymous local slot** (`lbl_`/`fn_` at an address inside the same fn's
   `.data`/`.rdata` range) AND the target `.text` contains a guard-load +
   init-once branch our side lacks. Emit: `{fn, unit, src_cpp, kind:
   local_static_symbol, literals: [...], guard_slots: N}`.
2. **`missing-guard-thunk`** (companion, often subsumed by #1). Target emits a
   `??__E<obj>` dynamic-init thunk and/or `$S` guard our TU doesn't. If the naming
   patchers (`obj_dynamic_init_patcher.py`, `obj_guard_patcher.py`) already
   promote/rename but the fn still diverges, it's a *code* miss → same fix family
   as #1. Distinguish "naming-only, patcher fixes it" from "code, needs source."
3. **`global-Symbol-out-of-line`** (inverse). Our source uses a function-local
   static but retail used a *global* (rare; the `StaticClassName` macro is the
   common correct-global case). Detect when target has the EXTERNAL global reloc
   and we have the local slot. Emit as `kind: prefer_global`.
4. **`bool-backref` / `anon-ns`** residuals that survive the naming patchers —
   flag for manual review, do not auto-worklist (lower confidence).

Signatures 1–3 are the "one-pattern-many-functions" core. The classifier must be
**conservative**: if a function has *any* substantial non-signature mismatch
(register alloc, instruction selection, struct-offset), it is **not** a candidate
— the Symbol fix alone won't close it, and applying it burns an A/B slot for a
partial. This mirrors the `nearmiss-classify` skill's "separate systematic
repeat-pattern levers from one-off permuter-class noise" doctrine.

### Part C — worklist output + apply loop

Output `~/tmp/symbol_sweep/worklist.json`:

```json
{"fn": "?SetMode@GameMode@@QAAXVSymbol@@@Z",
 "unit": "default/GameMode", "src": "src/system/game/GameMode.cpp",
 "kind": "local_static_symbol", "baseline_pct": 90.57,
 "literals": ["mode_a","mode_b"], "guard_slots": 2, "confidence": "high"}
```

Ranked by `(kind priority, baseline_pct desc)` so the closest-to-100 land first.
The apply loop is deliberately **not** automated end-to-end — it's a mechanical
human/agent loop, one fn per isolated worktree:

1. `scripts/setup_worktree.sh ~/tmp/wt-sweep-<fn> sweep-<fn>` (CoW, `~/tmp`).
2. Convert the flagged global `Symbol` refs in `src` to function-local
   `static Symbol name("literal");` (literals supplied by the scanner from the
   resolved global's initializer or the target's anonymous-slot string, cross-read
   from the rb3-Wii / dc3 oracle for the exact spelling).
3. Build (`objcache` all-hit ≈ 3.5 s) → objdiff the fn. Expect RAW verdict to sit
   ~97.5 even when byte-exact (dtk anonymous-slot gotcha) — **trust `normalized` /
   report.json**, per the grind doc.
4. Whole-binary A/B (cold-cache per HONESTY GATES — warm CoW can serve stale
   objs → false net-zero). Land only if **net-WIRED-positive with 0 regressions**.
5. `mark_patch_result` / coordinator lands one patch at a time onto main.

The literals and exact source spelling come from the oracles: `dc3-pair` skill for
engine Symbols, `rb3wii-pair` for band3. The scanner *locates*; the oracle
*specifies*; the A/B *gates*. No step guesses bytes.

### Extending the sweep to other one-pattern families

The same scanner scaffold generalizes to any signature expressible as "residual
mismatches dominated by one reloc/idiom class." Natural next entries, each its own
classifier + worklist, cross-referenced to sibling RFCs:

- **`SAVE_REVS` / load-revision constants** — the `saverev-sweep` skill's
  PostProcer `SAVE_REVS(1,0)->(0,0)` family (immediate-operand mismatch on a
  version constant). A pure data-constant sweep.
- **Codegen idioms** (`strcpy` `extsb`/`cmplwi`, etc.) → hand off to
  **`13-codegen-idiom-library.md`**; those are instruction-selection, not reloc,
  and want the idiom library not this scanner.
- **Struct-offset uniform deltas** → **`grind-execute` skill** / member-delta
  finder; out of scope here (already tool-supported).

This RFC owns the **reloc/symbol/guard-thunk** family specifically.

## Alternatives considered

- **Target-obj name grep (the brief's literal design).** Refuted above: dtk
  target objs contain zero local-static mangled names (0/3091 scanned). Cannot be
  the primary signal.
- **Keep finding these via the grind loop.** The grind loop's own verdict
  (`project_grind_loop_2026-07-07.md`): it's a *draft generator*, ~$0.005–0.009/fn,
  closes nothing to 100% on its own, and variance-dominated. It stumbled onto this
  pattern twice by luck. A deterministic scanner is strictly cheaper and complete
  over the near-miss set. The two are complementary: grind for genuine logic
  drafts, sweep for known-shape reloc fixes.
- **Fold the fix into the obj patchers** (emit the guard/thunk at patch time).
  Rejected: the patchers rewrite symbol *names/classes*, not `.text`. Synthesizing
  the guard-load + init-once branch + `??__E` thunk into a compiled obj is a code
  transplant (`obj_transplant_patcher.py` territory) — brittle, and it would make
  the source *not* match its own obj, defeating the point of a source decomp.
- **Just port bodies wave-by-wave** (bodyport skills). Those waves already exist
  and are broad; this RFC is the *cheap, mechanical subset* that doesn't need a
  per-fn body-port judgment call — it's worth carving out precisely because it's
  the low-effort tail of that work.

## Effort & expected value

**Effort:** scanner ~1–1.5 days (Part A/B reuse `diff_inspect.py`; the reloc
classifier is the new work). Per-fn apply ≈ 10–20 min (worktree + 2-line edit +
oracle literal + A/B), so a 30-fn worklist ≈ 1–1.5 agent-days of apply.

**Expected value — honest, anchored:**

- **Direct anchor:** the one applied instance closed **2 functions + 20 thunks =
  +22 matched** for a 2-file edit (`3342b30`). Thunk-pairing multiplies the
  per-fn yield well above 1.
- **Yield ceiling is modest.** The afflicted set is *not* the full 1,821
  near-miss band. Only **280** are band3 (where this pattern lives — it's a
  band3 game-code idiom, not an engine one), and of the 48 band3 near-miss units,
  ~11 already use the pattern correctly. Realistically the `local-static-Symbol`
  worklist is **~15–40 band3 functions**, most in `[95,100)`. Engine near-misses
  (1,539) are dominated by other causes (the engine is DC3-solved; its residuals
  are ICF/regalloc/struct, not this idiom).
- **Point estimate: +30 to +90 matched** across the whole reloc/symbol/guard
  family (local-static-Symbol + companion thunk pairing + the SAVE_REVS
  sub-sweep), if the thunk-pairing multiplier (~10× on the one datapoint) holds
  even weakly. **Low case +15** (pattern rarer than the two lucky finds suggest).
  **High case +120** if guard-thunk pairing is as productive across band3 as it
  was on the two fns.
- Comparable past veins for calibration: class-A span harvest +403 (one session,
  now exhausted); grind close +22; Waypoint ObjPtrVec flip +7. This vein is in the
  "+tens" tier — real, bounded, cheap. Not a +hundreds lever.

**Why it's still worth doing:** cost/benefit. It's the cheapest matched-fn-per-
agent-minute work available once class-A is exhausted, and the scanner is reusable
infrastructure for every future reloc-family idiom (it becomes the front-end for
`13-codegen-idiom-library.md` and the `saverev-sweep` skill).

## Risks & failure modes

- **Thunk-pairing multiplier may not generalize.** The +20 thunks on `3342b30`
  could be specific to those two units' static-init layout. If most fns yield only
  the 1 fn + 0–2 thunks, the vein is +15–30, not +90. **Mitigation:** the scanner
  reports predicted `guard_slots` per fn; the first 5 applies validate the
  multiplier before committing the full worklist.
- **False candidates that don't close.** A fn flagged local-static-Symbol may have
  a co-resident regalloc/struct mismatch the classifier under-weighted → applies
  to 98%, not 100. **Mitigation:** conservative "dominated by one signature" gate;
  keep improved-but-unclosed edits only if net-WIRED-positive (pinning grows the
  denominator — see `d696b52` gate note).
- **Stale-obj false net-zero.** Warm CoW worktrees serve stale objs → an A/B reads
  +0 for a real win. **Mitigation:** cold-cache A/B for every landing (HONESTY
  GATES).
- **Oracle literal wrong.** Guessing the Symbol string wrong yields a byte-exact-
  *looking* match with wrong data. **Mitigation:** literals come from the resolved
  global's initializer or the oracle, never invented; verify against dc3/rb3-Wii.
- **RAW-verdict confusion wastes cycles.** Agents may chase the ~97.5 RAW ceiling
  on already-byte-exact fns. **Mitigation:** scanner output stamps every row
  "trust normalized/report.json, not RAW" (the documented gotcha).

## Kill criteria

- The first **5** applied worklist entries yield **< +8 matched total** (i.e. the
  thunk-pairing multiplier collapsed to ~1× and the fn count is small) → the vein
  is a handful of manual fixes, not worth the scanner; stop, apply the ~5 known
  ones by hand, close the RFC.
- Scanner's `local-static-Symbol high-confidence` worklist has **< 10 functions**
  after classification → not "systematic," just the two we found plus a few;
  downgrade to a `nearmiss-classify` footnote, don't build dedicated tooling.
- The classifier can't separate "close-able by this fix" from "improves only" with
  useful precision (mirrors the topo-locator precision-0.13 death,
  `docs/decomp/research/2026-06-30-topo-locator-design.md`) → the reloc signal is
  too noisy; fall back to grind + manual.

## Open questions

- Does objdiff JSON expose the *resolved* target of an anonymous local-slot reloc,
  or only its address? If only the address, the classifier needs a target-obj
  address→section map to confirm "local slot inside this fn's data" (buildable
  from the same COFF parse used above, but extra work).
- How many of the 1,503 `[95,100)` fns are band3 vs engine? (I have the band3
  near-miss total = 280 but not the `[95,100)`-only split by layer.) A 20-fn
  `diff_inspect` sample across band3 `[95,100)` would pin the true afflicted rate
  before building — recommended as the RFC's first concrete step.
- Can the SAVE_REVS constant sub-sweep share the scanner scaffold, or is its
  immediate-operand signal different enough to warrant a separate tool? (Likely
  shareable: both are "residuals dominated by one reloc/immediate class.")
- Does the local-static conversion ever *regress* a sibling fn in the same TU
  (extra `??__E` thunk changing another fn's `.text` layout)? The +0 regressions
  on `3342b30` says no for those two, but the sweep must A/B whole-binary, not
  per-fn.

## References

- `docs/plans/grind-loop-calibration-2026-07-07.md` — §"Close-out proof" +
  "Discovered lever — the local-`static Symbol` pattern" (the origin datapoint).
- Commit `3342b30` (`git show`) — the one applied instance: `SavedSetlist.cpp`,
  `CampaignLevel.cpp`; +22/0. Commit `a1312de` — docs of same.
- `scripts/obj_dynamic_init_patcher.py`, `scripts/obj_guard_patcher.py`,
  `scripts/obj_atexit_scope_patcher.py`, `scripts/obj_bool_mangle_patcher.py`,
  `scripts/obj_anon_ns_patcher.py`, `scripts/obj_target_symbol_renamer.py` — the
  NAMING patchers this sweep sits downstream of (they pair symbols; the sweep
  fixes code). `scripts/obj_transplant_patcher.py` — code-transplant, the
  rejected alternative.
- `tools/gen_game_target_map.py` — how game-TU target symbols get named
  (`unified_id_rb3wii.json` oracle → mangled map); context for why target objs
  carry `fn_<addr>` not local-static names.
- `scripts/analysis/diff_inspect.py` (`run_objdiff_for_symbol` L1693, compare/
  mismatches modes) — reuse for Part A JSON parsing.
- `scripts/grind/splice_scorer.py` — the proven `objdiff-cli diff … -f json
  --include-instructions` invocation the scanner reads.
- `tools/fuzzy_progress.py` — staircase/histogram; the `[95,100)=1,503` primary
  band and the WIRED-denominator landing gate.
- Skills: `nearmiss-classify` (systematic-vs-noise doctrine), `saverev-sweep`
  (sibling constant sweep), `dc3-pair` / `rb3wii-pair` (Symbol literal oracle).
- `docs/decomp/research/2026-06-30-topo-locator-design.md` — precision-death
  precedent informing the kill criteria.
- Sibling RFCs: **13-codegen-idiom-library.md** (non-reloc idiom family — hand
  off codegen mismatches there), **11-permuter-farm.md** (finishes fns this
  sweep lifts but doesn't close), **12-grind-fleet-v2.md** (the expensive
  discovery path this scanner replaces for reloc-shaped fixes),
  **16-auto-landing-pipeline.md** (the net-positive A/B land gate),
  **02-gap-composition-atlas.md** (where this vein sits in the total gap).
