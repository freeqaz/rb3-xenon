# Proposed CLAUDE.md amendments — 2026-07-29 (lane BM) — **FOR REVIEW, NOT APPLIED**

`CLAUDE.md` was **not modified**. This lane had no authorization to change load-bearing
project instructions, so every proposal below is written as an apply-ready diff for the
project owner to accept, reject, or edit.

Each item states **why**, with the evidence, so the owner can adjudicate without re-deriving.

**What was re-verified as CORRECT in CLAUDE.md and needs no change** (so the owner knows
the audit was two-sided): the obj-patcher wiring list (pre-compile `obj_target_symbol_renamer`
+ post-compile `anon_ns` → `dynamic_init` → `guard` → `bool_mangle` → `atexit_scope`, with
`regswap`/`transplant` unwired) matches `configure.py` l.502-586 **exactly**; the orchestrator
MCP exposes **exactly** the 11 named tools; the `.pdata`-is-derived-output rule and both ★ traps
in the splits.txt bullet are accurate; the `/O1 /Oi /GR /EHsc` + no-LTCG framing is unchanged;
the `~/tmp`-not-`/tmp` and worktree rules are accurate.

---

## P1 — Add `orig/45410914/band.exe` as a first-class oracle (HIGH VALUE)

**Why.** CLAUDE.md never mentions `band.exe`, yet **40 tools already consume it** and it is
the only way to read retail's *real* bytes. Agents currently infer retail behaviour from the
symbol map and from dtk's carve asm — both derived, both wrong in known ways. It also
dissolves the ICF confounder (one physical body, not N symbol aliases).

**Where.** New bullet at the end of the "Build wiring" section, or as its own short section
right after "Two build tracks".

```diff
+### The retail oracles (read these, don't infer)
+
+- **`orig/45410914/band.exe` is the DECOMPRESSED RETAIL PE** (imagebase `0x82000000`,
+  extracted from `default.xex`). It lets you read retail's **actual bytes** — instruction
+  stream, literal pool, `.pdata` — at any VA, instead of inferring from
+  `scripts/target_symbol_map.json`, and it **dissolves the ICF confounder** (you see the
+  one physical body, not N folded symbol aliases). 40 tools already consume it
+  (`scripts/harvest/funclet_cascade_rank.py`, `switch_frame_census.py`,
+  `localstatic_symbol_audit.py`, `scripts/map_verify.py`, `scripts/symbols_hygiene.py`,
+  `tools/va_disasm.py`, `tools/xex_string_at.py`, …). **Prefer it over any derived artifact.**
+  ★ To recover a dispatch-arm list *in retail's exact order*, don't `strings` it — pull the
+  target fn from `build/45410914/asm/<Unit>.s`, grep its `addi r4, r11, lbl_…` sequence, and
+  resolve each label in `auto_00_82000400_rdata.s`. Cheaper, ordered, and it tells you *where
+  to insert* (laneBK, `94244fbd`).
+- `orig/45410914/default.xex` is the **TU5** target (`default_tu5.xex` = same bytes).
+  `orig/45410914/tu0-archive/` is pre-2026-07-15 — **every TU0-era address is invalid.**
```

---

## P2 — Add the stale-build-artifact hazard to "Known issues / expected noise" (HIGH VALUE)

**Why.** This is currently only in `docs/INDEX.md`. It has silently corrupted at least three
analyses (a 36% inflation, a 1.91× over-count, and a lane that read pre-TU5 carve geometry).
Measured on main **2026-07-29**: `build/45410914/asm` = 12,994 `.s`, **3,932 live, 9,062
stale (69.7%)**; `obj` = 13,016, same 9,062 stale; `objdiff.json` lists 3,862 live units.

```diff
+- ★ **`build/45410914/{asm,obj}` is ~70% STALE.** ninja declares only `config.json` as the
+  split rule's output and jeff plain-creates each `.s`/`.obj`, so every dead `splits.txt`
+  generation leaves its carve behind **forever**, frozen at that era's binary geometry —
+  thousands of `auto_03_*`, many predating the 2026-07-15 TU0→TU5 flip with bytes that occur
+  **nowhere in the current target**. Measured 2026-07-29: **9,062 of 12,994 `.s` are stale
+  (69.7%)**. **Any tool that globs those directories without
+  `scripts/harvest/live_units.py` is suspect** — offender list in `docs/decomp/TOOLING.md` §3.
+  **mtime is NOT a usable freshness proxy** (72 of 90 named orphans carried the same day's
+  date; `asm/Faders.s` live and `asm/system/synth/Faders.s` orphan coexist ten minutes apart).
+  The only sound discriminator is `objdiff.json` membership. Remedy: `scripts/prune_orphan_asm.py`.
```

---

## P3 — Add the reloc-masked "100% but wrong" defect class + the map-mispair rule (HIGH VALUE)

**Why.** CLAUDE.md's whole framing is "matching machine code", which implies 100% == correct.
It is not: relocation-masked operands (string/constant addresses) are invisible to the
normalized diff, so a **wrong constant or wrong literal scores 100%**. 40 such fixes landed
2026-07-29 at exactly 0 metric movement. And the counter-rule matters even more — **43% of
that worklist were map mispairs, not source bugs.**

```diff
+## A 100% match is not proof of correctness
+
+objdiff's normalized diff **masks relocation operands**, so a function with the wrong
+string, the wrong float constant, or the wrong save-revision can score **100%**. No scanner
+finds these, because every scanner looks *below* 100%. 40 such correctness fixes landed
+2026-07-29 at exactly **0 metric movement** (`docs/plans/realbug-fixes-2026-07-29.md`;
+instrument: `scripts/harvest/reloc_correspondence.py --census`).
+
+★★ **But 43% of that worklist were MAP MISPAIRS, not source bugs.** Rule: *if retail's
+diverging operands coherently describe a **different** function — a sibling, a template
+twin, another class — the defect is in `scripts/target_symbol_map.json`, not in the source.*
+Fix the map. Do **not** "fix" the source to match a function you were never paired against.
```

---

## P4 — Point the "Skills / analysis engine" block at the audited tooling inventory (MEDIUM)

**Why.** CLAUDE.md enumerates ~10 tools out of ~350. Agents re-derive tool choice every
session, and several tools they reach for are broken or superseded
(`handler_list_diff.py` emits **false surplus**; `autocarve_funnel.py` is superseded by
`diffunit_gap_funnel.py`; `localstatic_census_wide.py` is 71% stale-contaminated;
`reloc_correspondence.py --symbol` times out at 10 min).

```diff
 **Analysis engine** (`scripts/analysis/diff_inspect.py`, 1969 LOC): modes
 `diagnose`, `clusters`, `regswaps`, `offsets`, `replaces`, `compare`,
 `save_baseline`, `mismatches`, `stack-layout`, `asm_listing`.
+
+★ **Tooling inventory: `docs/decomp/TOOLING.md`** — the audited census of every tool in
+`tools/`, `scripts/`, and `scripts/harvest/` (each one actually invoked, 2026-07-29):
+status (WORKING / BROKEN / SUPERSEDED / ONE-SHOT), one-line purpose, invocation, and a
+**known-defects table**. Read it before running an unfamiliar scanner. It also carries the
+"start here for task X" routing table and the ground-truth artifact table.
```

---

## P5 — Complete the skills enumeration (LOW, factual)

**Why (and a correction to my own first draft).** I initially proposed that the "24 total"
count was stale; I then counted `.claude/skills/` and it is **exactly 24 — the count is
CORRECT**. What is incomplete is the *enumeration*: CLAUDE.md names 16 and omits 8, all of
them the native-port / asset / emulation side, which is precisely the area an agent would
otherwise assume has no tooling. (The `bodyport-batch*` / `gameport*` / `permuter-sweep*`
campaign skills are **not** in `.claude/skills/` — they are supplied at the harness level,
so CLAUDE.md is right not to list them.)

```diff
 resolve-vcall, stack-layout, struct-info, vtable, dc3-pair (primary engine
 oracle — DC3 is the closest twin), rb3wii-pair (game-code oracle — richer
-named-function source). All ported with port 8002 + title-ID 45410914
-substitutions applied.
+named-function source), unicorn-query — plus the native/asset/emulation set:
+asset-extract, native-build, screenshot, xenia-gameplay, gpu-capture, gpu-debug,
+gpu-inspect. All ported with port 8002 + title-ID 45410914 substitutions applied.
```

---

## P6 — Add the two levers/anti-levers proven 2026-07-29 (MEDIUM)

**Why.** Both are cheap to state and expensive to re-derive; one is a *negative* result that
an agent would otherwise waste a lane discovering.

```diff
+- **The local-static frame cascade (LEVER).** Retail builds each dispatch `Symbol` as a
+  **function-local static**; ours used globals ⇒ 2 extra callee-saves ⇒ the parent frame
+  shifts ⇒ and because **every EH funclet encodes the parent frame in its first
+  instruction**, one per-TU macro gate flips a whole cascade. **76% of the 96–100% band is
+  funclets, not near-miss functions** — price funclet cascades, not function counts.
+- **Early-return restructuring (ANTI-LEVER, do not use).** Measured **NEGATIVE**: 98.3% → 34.1%.
+- **UNMERGED ≠ UNLANDED.** Lanes land by patch, not by merge, so a stale branch is not
+  pending work — and landing one can be net-HARMFUL (`docs/plans/branch-audit-2026-07-29.md`).
```

---

## P7 — Flag the `land.sh` deletion hole in the git/worktree section (MEDIUM)

**Why.** CLAUDE.md's git section is where an agent decides how to land. The landing path can
silently discard a valid patch.

```diff
+- **`scripts/harvest/land.sh` cannot land a `splits.txt` DELETION.** It resolves conflicts
+  with `resolve_splits_union.py`, a **line-union** seeded from *ours* (= main under rebase);
+  its own docstring says "No removals are propagated". So every pin the lane **removed**
+  survives, and `land.sh` still prints `READY:`. Unpinning is a real landed fix shape
+  (phantom-shell TUs, XDK-territory spans). ⇒ if your patch's value is a deletion, resolve
+  by hand and re-verify with `scripts/harvest/overlap_check.py`. (`resolve_json_union.py`
+  *is* 3-way and does respect deletions.)
```

---

## Not proposed (considered and rejected)

- **Adding the 39,743 match count to CLAUDE.md.** It would be stale within a day and CLAUDE.md
  already correctly points at `report.json`/`decomp.db` for live numbers.
- **Rewriting the "Progress: 0.00% matched" baseline line.** It is a *format* explanation, not
  a current-state claim, and remains accurate.
- **Touching `decomp_pch.h` guidance.** Explicitly marked sacred; nothing in this audit
  contradicts it.
