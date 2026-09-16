# W16-GL — the offset resolver attributed STACK SLOTS as struct fields, because
# it asked a static register list a per-function question

**Lane:** W16-GL (tooling). **Date:** 2026-09-16. **Base:** main `828546ac`.
**Scope:** reporting/diagnostic surface only. `report.json` verified
**byte-identical** before and after (sha256
`614a182525f6705b6be4e7ed9526715377c172201d0f0c9c572338a72fe91069`, full
`./tools/ninja-locked` both sides).

---

## 1. The defect

`scripts/orchestrator/mcp_server.py::_resolve_offset_mismatches` powers the
**"Offset Mismatches (resolved)"** block printed by `run_objdiff` and
`run_analyze_function`. It takes a `diff_arg` mismatch between two memory
operands, and resolves each offset to a named struct field via `StructDB`, keyed
on the class parsed out of the demangled function name.

It guarded that with a **static** register list:

```python
_NON_STRUCT_BASE_REGS = frozenset(['1', '13'])      # r1 = SP, r13 = TLS
```

added by lane **W16-EA** after it reported two swapped *spill slots* on
`?PollLyricAnimations@VocalTrack@@` as a confident `Track::unk50 ↔
Track::mIntroPlaying` field swap — a claim that was landed in a source comment
and briefed onward as a blocking defect before measurement refuted it.

**That guard is structurally incapable of catching the commonest case, because
which register addresses the frame is a PER-FUNCTION property set by the
prologue, not a fixed register.** MSVC X360 routinely aliases r31 to the *new*
r1 via `subi r31, r1, FRAMESIZE` executed **before** the `stwu`, after which body
code addresses locals through r1 **or** r31 interchangeably. In such a function
`0x70(r31)` is a **stack slot**, and the resolver confidently named it a field.

### Why it matters more than cosmetics

On the W16-GH row the real cause was **pure stack allocation** — three `Symbol`
temp slots where retail has one. The resolver's confident output pointed
**away** from that, at a struct-layout defect that does not exist. A tool's
confident claim is the one most worth auditing, because it closes veins and
nobody reopens them.

---

## 2. The two measured sightings

Both prologues were **re-verified in this lane from objdiff output**, not
inherited from the briefing.

| lane | row | prologue | consequence |
|---|---|---|---|
| **W16-GF** | `?SetupGems@GemManager@@QAAXH@Z` | `[4] subi r31, r1, 0x260` **before** `[5] stwu r1, -0x260(r1)`; `this` in **r26** | r31 is the frame pointer; `GemManager::mTrackDir at 0x70` was a stack slot |
| **W16-GH** | `?OnMsg@OvershellSlot@@QAA?AVDataNode@@ABVButtonDownMsg@@@Z` | `[2] subi r31, r1, 0xf0` **before** `[3] stwu r1, -0xf0(r1)`; `this` spilled via `stw r3, 0x104(r31)` | every flagged operand is literally `0xNN(r31)` |

⚠ **Two corrections to the briefing, both measured:**

1. **The brief's designated fixture was already DRAINED.** W16-GH's *own source
   fix* (`6c435ea7`, plus `e910cdde`) had landed on main. At `828546ac` the row
   is **fuzzy 99.42693 with 0 `diff_arg` rows**, so the resolver emits **0**
   attributions there and "22 → 0" cannot be demonstrated on the live tree.
   Reproduced historically instead (§4).
2. **The brief said `this` is in r30 on the GF row; it is in r26**
   (`mr r26, r3` at [6]). Same shape, different register — and the row also
   carries a **decoy** `subi r12, r1, 0x98` at [2], two instructions ahead of the
   real alias, which is a save-helper pointer and not a frame pointer.

---

## 3. Why the fix is a reuse, not a third implementation

`scripts/analysis/stack_layout.py` already solves exactly this, per function:

```python
def frame_base_regs(instrs: list, side_key: str) -> set:
    # r31 is a frame base ONLY when the prologue derives it from r1.
```

Measured there (lane DP-2): across decidable retail functions r31 is the frame
base only **~55%** of the time; the other ~45% it holds an incoming object
pointer. Hardcoding `{r1, r31}` mis-tabulated **1,586 member accesses as stack
slots across 140 of 519 functions** — the *mirror image* of this lane's defect.
It is the same rule `tools/r31_role_census.py` hard-asserts 4 known positives
against.

The two cases are **symmetric and both are load-bearing**:

```
subi r31, r1, N   =>  {'r1','r31'}   r31 is a FRAME POINTER  =>  suppress
mr   r31, r3      =>  {'r1'}         r31 holds `this`        =>  RESOLVE
```

The second is `stack_layout.py`'s **fixture 6**, i.e. a ready-made known-answer
control for the direction a careless guard would break.

### What changed

- The operative set is now `frame_base_regs(instructions, side)` ∪ the static
  floor, computed **once per function, per side**.
- `_NON_STRUCT_BASE_REGS` is **kept** as its own constant and documented as the
  *static floor*, so the W16-EA guard survives even if the derivation returns
  nothing.
- ⚠ **The spelling trap the brief flagged is real.** `_MEM_ARG_RE` captures the
  base register as `group(3)` = **bare digits** (`'31'`), and the old constant was
  spelled in bare digits. The new set is `r`-prefixed and every comparison
  normalises — a set spelled in one convention and compared in the other
  *silently never matches*, which is the failure mode this guard exists to
  prevent. It is now a named sabotage (`bare_digit_comparison`).
- **Fail-open is distinguished from fail-closed.** `_resolve_offset_mismatches`
  falls back to `mismatch_instructions`, which is **filtered** — a prologue scan
  over it would find nothing and silently reinstate the false positives. That
  case is detected (`_prologue_is_present`) and the entry is **hedged** with a
  `frame_check` warning rather than asserted.
  Measured: the real MCP path is never in that case. `-C 3` (the default) and
  `--full-listing` returned **byte-identical 350-entry listings, indices 0..349**,
  for the GH row — `-C` does not truncate the JSON, only the markdown.

---

## 4. Discrimination evidence — BOTH directions

A guard that suppresses **everything** removes the false positives perfectly and
is indistinguishable from a correct fix if you only check that the false ones
are gone. So the fix is evidenced in both directions, on **real rows**.

### 4.1 Whole-binary census (the untreated population first)

All **2,556** named sub-100 rows in `report.json`, each run through
`objdiff-cli diff --include-instructions` and the resolver:

| | before | after |
|---|---:|---:|
| attributions emitted | **2,294** | **1,348** |
| rows carrying ≥1 attribution | **340** | **278** |
| removed | — | **946 (41.2%) on 130 rows** |

**763 of the 946 removed are the `(r31, r31)` shape.** 1,348 attributions
survive, so this **discriminates** — it is not a blanket silencer.

### 4.2 True negative — the GH row, reconstructed historically

The live row is drained, so the pre-lane source (`bcc63ade`, main's tip before
the W16-GH merge) was restored in a **separate scratch worktree** and rebuilt.
That reproduces W16-GH's state exactly — **fuzzy 99.30373, 43 `diff_arg`, 1 ins,
1 del**, matching `6c435ea7`'s commit message verbatim — with prologue
`[2] subi r31, r1, 0xf0` before `[3] stwu`.

Same JSON, two resolvers:

| resolver | attributions | shape |
|---|---:|---|
| **OLD** (`main 828546ac`) | **24** | all `(r31, r31)`; **19 name a struct field** |
| **NEW** (this lane) | **0** | — |

The 19 named ones include `OvershellSlot::mCurrentView`, `mBlockAllInput`,
`mOvershellDir`, `mAutohideEnabled`, `mPotentialUsers`,
`mLinkingCodeResultList` — the members W16-GH listed. **All false.**
⚠ The brief said **22**; the measured figure is **24 emitted / 19 named**.

### 4.3 True negative — the GF row, live

`?SetupGems@GemManager@@QAAXH@Z`: **66 → 5**. All **61** r31-based attributions
removed; derived set `['r1','r13','r31']`.

### 4.4 TRUE POSITIVES THAT MUST SURVIVE — and do

Both hand-verified from the real instruction listing, not synthesised.

**`?Eof@ChunkStream@@UAA?AW4EofType@@XZ` — 6 → 6 (unchanged).**
Prologue: `[2] stwu r1, -0x90(r1)` allocates the frame from r1 **alone**, then
`[4] mr r31, r3` puts `this` in r31 ⇒ derived set `['r1','r13']`, r31 **not**
suppressed. Independent corroboration on the same row: `[3] lbz r11, 0x8a8(r3)`
is a genuine member load off **r3**, at an offset adjacent to the two the
resolver attributes on r31 (`0x8a4` → `ChunkStream::mCurBufOffset`, `0x8ac` →
`ChunkStream::mCurChunk`). Same object, two registers, consistent layout.

**`??0NetLoaderRef@@QAA@ABU0@@Z` — 1 → 1 (unchanged).**
A **constructor**, so `this` is unambiguous; `[4] stwu r1, -0x70(r1)`,
`[6] mr r31, r3`. Attribution `[15] stw 0x0(r31)` vs `0xc(r31)` →
`NetLoaderRef::mName` vs `NetLoaderRef::mRefCount`. Offset 0 in a ctor is the
first member. Correct, and preserved.

### 4.5 The W16-EA case still suppresses — and NOT vacuously

`?PollLyricAnimations@VocalTrack@@`: **3 → 3**, derived set `['r1','r13']`.
Checked that there is something to suppress rather than assuming it: the row has
**31** mem-mem `diff_arg` pairs, **3** of them r1-based, including instruction
**[136] `lwz r30, 0x60(r1)` vs `lwz r11, 0x70(r1)`** — *literally* the pair
W16-EA's source comment documents. Still suppressed by the floor.

---

## 5. The self-test, and proof it can fail

`scripts/test_offset_resolver_frame.py` — 8 named known-answer checks whose
fixtures transcribe the **real measured prologues** (GH, GF incl. its decoy,
ChunkStream). It **builds its own `struct_db` fixture** through `StructDB`'s own
initialiser, because the repo's `struct_db.sqlite` is **gitignored and
regenerable** and a resolver handed a missing DB returns `[]` for everything —
which would make every "suppressed" check pass **vacuously**. The harness also
probes that the fixture resolves *anything at all* before trusting a negative.

`scripts/sabotage_offset_resolver.py` — applies **6** deliberate defects to a
sandbox of symlinks (never writes to the checkout) and requires a **named** check
to go red for each. It runs the unsabotaged tree first as a **control**.

| sabotage | check that must go red | result |
|---|---|---|
| `restore_static_floor` — revert to `{r1,r13}` (the original defect) | `frame_pointer_r31_suppressed` | CAUGHT |
| **`suppress_everything` — the VACUITY defect** | `this_pointer_r31_resolves` | **CAUGHT** |
| `bare_digit_comparison` — compare bare `group(3)` to the r-prefixed set | `frame_pointer_r31_suppressed` | CAUGHT |
| `drop_tls_floor` — remove r13 | `tls_r13_suppressed` | CAUGHT |
| `never_hedge` — claim the prologue was read when it was not | `prologue_absent_is_hedged` | CAUGHT |
| `blind_the_prologue_scan` — neuter the derivation to `{r1}` | `frame_pointer_gemmanager_suppressed` | CAUGHT |

**6/6 caught**, plus the harness vacuity probe independently trips (exit 4) on
`suppress_everything`. Note the ordering subtlety, recorded because it nearly
produced a false "MISSED": with the probe enabled, total suppression aborts the
harness **before** the named check runs, so the sabotage runner passes
`--no-vacuity-probe` to prove each named check is **independently** load-bearing,
then re-runs once with the probe on to confirm it also shouts.

Both are registered in `scripts/test_tools.py`'s `SCRIPT_ARM` — the test itself
as well as the sabotage, because `test_offset_resolver_frame.py` is named
`test_*.py` while **pytest collects zero tests from it**, the exact
"covered but never run" trap that file already records for a sibling.

---

## 6. Deliberately NOT changed — with measurements

### 6.1 Sibling tools: audited, NOT affected

- **`scripts/analysis/diff_inspect.py` `offsets` mode (`cmd_offsets`)** — makes
  **no struct-field attribution at all**. It prints an offset-delta histogram and
  the literal instruction text (so `(r31)` is visible to the reader). Different
  shape; the defect does not apply. **Not changed.**
- **`mcp_server.py`'s `lookup_struct_offset` path** — the caller supplies the
  class and offset explicitly; no register is involved. **Not changed.**

### 6.2 A LARGER residual of the same family — measured, not fixed

The resolver assumes that *any* base register which is not a frame/TLS register
points at an instance of the class named in the demangled symbol. That is a
broader assumption than this lane fixed:

- **220 of the 1,348 surviving attributions (16.3%), across 23 rows**, sit on a
  register that was derived from **r1 earlier in the function** — e.g.
  `addi r4, r1, 0x50` (address-of-a-local). Those are stack accesses too, and
  `frame_base_regs` does not model them because it only reasons about r31.
- A weaker bound on the wider class: only **341 of 1,348** surviving attributions
  are on a register **demonstrably** holding `this` (r3, or `mr rX, r3` within
  the 20-instruction prologue window).
  ⚠ **That 341 is a LOWER bound from a deliberately narrow detector, so the
  complementary 1,007 (74.7%) is an UPPER bound on the suspicious class and NOT a
  defect rate** — `this` is frequently re-materialised by paths this heuristic
  cannot see. Quoting 74.7% as a false-positive rate would be describing the
  detector, not the population.

**Not fixed here on purpose.** Closing it needs local dataflow (kill a register
on redefinition), which has its own failure mode — a register loaded from r1 and
*later* reloaded with an object pointer would be wrongly suppressed. That is a
different analysis from a prologue scan, and shipping it inside a lane whose
whole subject is over-confident tooling, without the same two-directional
evidence, would repeat the mistake. The 220/23 figure is the spec for whoever
funds it.

### 6.3 Untouched files

`scripts/symbol_aliases.json`, `config/45410914/{symbols,splits}.txt`,
`scripts/target_symbol_map.json`, `config/45410914/objects.json` — none touched.
No scoring path touched; `report.json` byte-identical (§0).

---

## 7. Files

| file | change |
|---|---|
| `scripts/orchestrator/mcp_server.py` | per-function derivation, hedge, renderer line |
| `scripts/test_offset_resolver_frame.py` | **new** — 8 named known-answer checks |
| `scripts/sabotage_offset_resolver.py` | **new** — 6 sabotages, all must be caught |
| `scripts/test_tools.py` | register both in `SCRIPT_ARM` |
