# Upstream-Port Workflow

When a related decomp tree (RB3, upstream rjkiv/dc3-decomp, etc.) has a
function at 100% match and we don't, the highest-yield approach is often
to **port their literal source verbatim** rather than fine-tuning ours.

This is not "copy-paste their bugs": the goal is to eliminate accidental
divergence in expression-shape, control-flow ordering, and helper-vs-
inline boundaries. Their version compiles to the target binary. Ours
compiles to something else despite expressing similar logic.

This doc captures the workflow proven during the 2026-05-12 upstream
merge gap-recovery pass (35+ functions ported from -0.73% → -0.15% in
meta_ham, 27 commits across 5 parallel waves).

---

## When To Use This Workflow

Use it when **all** of the following hold:

1. A related repo has the function at 95-100% match.
2. Our function is at 80-99% — a real source-level gap, not AT_LIMIT.
3. The two sources express the same logic but differ in expression
   shape, declaration order, or helper inlining.

**Don't** use it when:

- Our impl is 100% and the related repo's is lower (we win — they lose).
- The related repo's impl is also <90% (no proven model to port).
- Logic differs (e.g., HX_NATIVE conditional code paths). Resolve the
  semantic difference first, then port the matching variant.

---

## Step-by-Step

### 1. Identify candidates

Compare per-function match% between our `report.json` and the related
repo's report.json. For each function where the related repo is at
100% and we're below, calculate gap = `(their_match - our_match) * size`.
Rank by gap size and tackle largest-first.

A scratch script that does the comparison lives in
`scripts/analysis/compare_progress.py`. To find function-level gaps:

```python
import json
with open('build/45410914/report.json') as f: r = json.load(f)
with open('/home/free/code/milohax/dc3-decomp/build/373307D9/report.json') as f: o = json.load(f)

def collect(report, subsys):
    funcs = {}
    for unit in report['units']:
        if subsys not in unit['name']: continue
        for f in unit['functions']:
            sz = int(f.get('size', '0') or 0)
            if sz == 0: continue
            funcs[f['name']] = (f.get('fuzzy_match_percent', 100.0), sz, unit['name'])
    return funcs

ours = collect(r, 'lazer/meta_ham'); up = collect(o, 'lazer/meta_ham')
diffs = []
for name in set(ours) | set(up):
    op, sz, _ = ours.get(name, (100.0, up.get(name, (0,0,''))[1], ''))
    upp, _, _ = up.get(name, (100.0, 0, ''))
    sz = max(sz, up.get(name, (0,0,''))[1])
    if op + 0.5 < upp:
        diffs.append((name, op, upp, sz))
diffs.sort(key=lambda d: -(d[2] - d[1])/100 * d[3])
for n, op, upp, sz in diffs[:25]:
    print(f'{(upp-op)/100*sz:5.0f}b ours={op:5.1f} up={upp:5.1f} sz={sz:5d} {n[:80]}')
```

### 2. Port verbatim, then remap

For each target function:

1. **Read** their impl + ours side-by-side.
2. **Replace** our function body with theirs verbatim.
3. **Remap unk* → named members.** Their tree may have `unk38`, `unk39[i]`
   where ours has `mFirstTimePlayed`, `mOneTimeTaskFlags[i]`. Same offsets,
   different names. Verify offsets match by comparing the headers.
4. **Translate accessors.** They may call `playerData->Unk40()` where we
   have `playerData->PlayerIndex()`. Use ours; same vtable slot.
5. **Preserve our HX_NATIVE guards** — only relevant where we have
   conditional native-port code that the related repo lacks.
6. **Build + verify** with `mcp__orchestrator__run_objdiff`.

### 3. Iterate when stuck

If verbatim port doesn't reach 100%:

- Run `mcp__orchestrator__run_diff_inspect` mode=`diagnose` for a verdict.
- Common residuals after a verbatim port:
  - **Register swap** (rXX↔rYY) → sometimes fixable by extracting an
    intermediate local; usually AT_LIMIT.
  - **Static guard mismatch** → see [Function Definition Order](patterns/fixable-declarations.md#function-definition-order-tu-wide-static-guard-counters).
  - **`bl` to wrong target** → check [Inline Boundary Cascade](patterns/fixable-inline-boundary.md#inline-boundary-cascade-icf-merge-of-out-of-line-accessor).
  - **`MakeString` template arg differs** → see [MakeString Template Type Mismatch](patterns/fixable-casting.md#makestring-template-type-mismatch-milo-macro-arguments).

### 4. Trust but verify "AT_LIMIT" claims

Some functions look AT_LIMIT but yield to the right tweak:

- The orchestrator's heuristic that classifies register-swaps as
  "not source-controllable" is wrong sometimes — verbatim porting from
  upstream did shift register allocation in our favor in many of the
  2026-05-12 wave fixes.
- Conversely, upstream's `report.json` percentages can be **stale**.
  An upstream "95.3%" may not reproduce when you actually rebuild
  upstream's source — verify by running their .obj through objdiff
  yourself (`./bin/objdiff-cli diff -1 their.obj -2 our.obj <symbol> --map-file ...`).
  CursorPanel::Poll, claimed by upstream's report at 95.3%, actually
  matches at 78% when you rebuild — our 79% was at the source ceiling.

---

## Parallel Execution

Each function lives in a different (or grouped) source file, so the
work parallelizes well. The proven recipe:

- Group functions by **file** (one agent per file, or per 2-3 related
  files when they're small).
- Cap concurrency at **6 agents** per memory note.
- Each agent gets:
  - Mangled symbol name(s) and current/target match%.
  - Path to upstream source.
  - Brief context from prior merge logs (if any).
  - Instructions to commit when done with the canonical message format.

Validate after each wave with `ninja` + `compare_progress.py`. Run
the next wave on whatever's still gappy.

---

## Anti-Patterns

- **Don't port speculative impls.** Some upstream functions have no
  body (declaration only). If their match% is 100% but their source
  is empty, the function is being inlined or ICF-merged elsewhere; a
  blind port will regress us. Verify their source is real.
- **Don't port across diverged headers.** When SaveLoadManager.cpp is
  byte-identical to upstream but our match is lower because *header*
  files (HamProfile.h, ProfileMgr.h) differ — that's a cross-cutting
  AT_LIMIT, not a port candidate. Reverting headers to upstream would
  break unrelated working code.
- **Don't strip HX_NATIVE.** Some HEAD ports of upstream forgot to
  re-add HX_NATIVE conditional code (poll guards in ShellInput,
  TheNetCacheMgr null-checks in MainMenuPanel). Always preserve
  ours where they exist.

---

## Discoveries Captured Elsewhere

The mechanical patterns that emerged from the 2026-05-12 wave have
been added to the pattern docs:

- [Inline Boundary patterns](patterns/fixable-inline-boundary.md) —
  Sort comparator inline location, inline ctor in header vs cpp,
  ICF cascade.
- [Manual Helper Inlining](patterns/fixable-control-flow.md#manual-helper-inlining-reverse-inline-a-trivial-helper) —
  Reverse-inline a trivial inline helper for the last 2-12%.
- [Static Variable Type in MakeString Args](patterns/fixable-casting.md#sub-pattern-static-variable-type-in-makestring-args) —
  When a `static const T sVar` mangles into a MakeString template
  instantiation, T's type matters.
- [Iterator Address-Of (`&*iter`)](patterns/harmful-avoid.md#iterator-address-of-iter) —
  Don't pass `&*it` to range functions; pass `it` directly.

---

## See Also

- [docs/decomp/RB3_REFERENCE.md](RB3_REFERENCE.md) — How to use the
  Rock Band 3 decomp as a reference (shared Milo engine).
- [docs/decomp/patterns/INDEX.md](patterns/INDEX.md) — Full pattern catalog.
- [docs/decomp/SUBAGENT_STRATEGY.md](SUBAGENT_STRATEGY.md) — How to brief
  parallel subagents for decomp work.
