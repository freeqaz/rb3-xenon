# Spill-store leverage campaign (2026-07-10)

Turning the cracked spill-store-count mechanism into fleet-wide matches: scan
tooling, per-class harvest levers, and a header-trial experiment track.

**Prerequisite reading** (this doc assumes them; do not re-derive):

- `docs/decomp/research/2026-07-10-spill-store-homing-mechanism.md` — the
  canonical three-way mechanism split (address-taken writes / EH-conservative
  homing / true register-pressure spill), proven minimal pairs, the FaderGroup
  specimen, and the direction-inversion tooling trap.
- `docs/decomp/MSVC_X360_REGALLOC.md` — c2.dll coloring chain (declaration
  order → symbol IDs → coloring order → register assignment); scopes what is
  deterministic vs intractable.
- `/home/free/tmp/fg_dtor2_findings.md` + `/home/free/tmp/spillrepro/` —
  scratch specimens (10-shape S-table, `cc.sh` one-off production-flag
  compiler, minimal pairs `at1..at4`, `eh0..eh2`).
- `docs/decomp/patterns/unfixable-compiler.md` "Stack Spill Scheduling" — the
  legacy monolithic bucket this campaign is splitting.

## 1. The mechanisms (operational summary)

For any near-miss whose diff involves an extra/missing `stw rN, slot(r31)`
(or r30/r1 frame base), classify by how the **retail** side uses the slot:

| Class | Retail slot usage | Mechanism | Lever |
|---|---|---|---|
| **A — address-taken** | `addi rX, r31, slot` exists, or slot is reloaded (`lwz`) | One `stw` per source write surviving DSE; writes separated by an opaque call cannot fold (escaped address) | **Source-fixable now.** Match retail's write count/placement: add/remove/move a write across the call boundary. Proven S=1→S=2 minimal pair. |
| **B — EH homing** | Write-only, never reloaded, funclet doesn't read it, function has EH (`__ehfuncinfo$`), value is an inlined container node/iterator | EH-conservative homing of an EH-tracked temp across a throwing call; S = number of independent node-materializing expressions | **Quantized.** Body shape moves S in steps (0, 2, ...); some retail values (e.g. S=1 via value-CSE) are only reachable through shared-header inlining changes → the header-trial track. |
| **C — pressure spill** | Write-only, no EH temp involved, high register pressure | Defensive spill by the allocator | **Unfixable.** Don't chase (per REGALLOC doc). Value of the scan: *positively identifying* these stops agents from burning cycles. |

Diagnostic order matters: check for `addi`/reload first (A), then EH context
(B), else C. The classification is mechanical — that's why it should be a tool,
not an agent judgment call.

**Tooling trap to bake in everywhere:** verify objdiff target/base orientation
against the dtk asm (`build/45410914/asm/<unit>.s`, anonymous `fn_` callees =
target/retail) before reasoning about which side has the "extra" store. The
FaderGroup first pass had it backwards.

## 2. Threads

### T1 — Signature scanner (`scripts/harvest/spill_signature_scan.py`)

Batch classifier over the near-miss pool. Template:
`scripts/harvest/offset_drift_sweep.py` (same skeleton: pool JSON in →
`diff_inspect.run_objdiff_for_symbol` per function → per-function record out,
`--resume` support).

Per function:

1. Run objdiff, collect insert/delete instructions.
2. **Signature gate:** the diff's insert/delete set is dominated by
   store instructions to a frame slot (`stw/stfs/stfd/stb/sth rN, imm(r31|r30|r1)`).
   Record purity: `sole_diff` (nothing else differs), `dominant`
   (≥80% of indels), `present` (any).
3. For each implicated slot, classify A/B/C by scanning the **retail** asm
   side for `addi rX, rBase, slot` / reload `lwz` on the same slot, and for
   EH markers (funclet symbols, `__ehfuncinfo$`, throwing calls adjacent to
   the store).
4. Emit JSON: `{sym, unit, pct, size, purity, direction (ours-extra |
   ours-missing), slots: [{off, class, evidence}], verdict}`.

Output feeds both harvest fan-out (class A) and the sizing decision (class B).

### T2 — Class-A harvest (proven lever, immediate)

For every class-A hit: an agent matches retail's write count/placement in the
source (writes on the correct side of the opaque call, hoist single-assignment
loop-invariant words). Standard worktree flow (`scripts/setup_worktree.sh`
under `~/tmp`), objdiff verify, patch back to main, path-limited commit.
Expected to be a modest but clean vein — every hit is a real strict candidate.

### T3 — Class-B sizing → header-trial campaign

The FaderGroup analysis showed the S-count for thin-node `ObjPtrList`
temps is set by shared-header phrasing (`obj/Object.h` / `obj/ObjPtr_p.h`
thin-node `front()`/`Obj()`/`iterator`/`pop_front()`), not the function body.
DC3 matches the identical body with a different node layout — the header is
the variable.

- **Sizing gate:** count class-B hits where the implicated temp is an
  ObjPtrList/thin-node value (callee `Unlink`, `ObjPtrList` types in the
  demangled name or adjacent calls). If ≥ ~10 functions share the signature,
  a header trial amortizes; if it's 1–2, stay parked (blast radius isn't paid
  for by one dead store).
- **Trial protocol (worktree-isolated, A/B-gated):** each trial = one candidate
  rephrasing of the thin-node accessors in a **fresh buildable worktree**
  (`setup_worktree.sh ~/tmp/wt-htrial-N`), full build, `tools/fresh_report.sh`,
  compare whole-binary strict + fuzzy vs baseline
  (`scripts/harvest/measure_delta.py` + manual fuzzy check — measure_delta
  gates strict only). **Hard gate: zero strict regressions** across the fleet
  (every ObjPtrList user, incl. 100% TUs like MasterAudio). Land only net-up.
- Candidate rephrasings to enumerate (each its own trial): single-read
  `mNodes` caching in `pop_front`, `front()` returning through a named local,
  iterator-mediated `front`, merged `front+pop` helper used by callers, and
  `Unlink` signature variations (value vs ref). The gated
  `RB3_FADERGROUP_DTOR_COALESCE` prototype (byte-identical result) already
  eliminated the erase-iterator-wrapper hypothesis — don't repeat it.

### T4 — Generalize the scan beyond `stw`-to-frame

Second-wave signatures once T1 works: extra/missing **reload** (`lwz` from a
frame slot) as the sole diff (the read side of the same homing decision);
extra/missing `mr` register copies adjacent to calls (coloring artifacts,
cross-ref REGALLOC doc); store *ordering* swaps within a block (scheduling,
likely class C). Each gets a purity/direction record in the same JSON shape so
downstream triage is uniform.

### T5 — Fold into the standing tooling & docs

- Split `docs/decomp/patterns/unfixable-compiler.md` "Stack Spill Scheduling"
  into the three-way table, referencing the research doc. (Was blocked on
  another agent's in-flight edits to that file — re-check `git status` first.)
- Add a `stw-classify` mode to `scripts/analysis/diff_inspect.py` (and thus
  the MCP `run_diff_inspect`) so any agent hitting a store-count mismatch gets
  the A/B/C verdict + lever mechanically, without re-reading the research doc.
- Record class-C hits into `scripts/harvest/nearmiss_verdicts.json` walls so
  the pool generator stops re-serving them.

## 3. Coarse roadmap

| Phase | What | Gate to next |
|---|---|---|
| 0 | This doc + research doc (done) | — |
| 1 | Build T1 scanner; smoke on `??1FaderGroup@@QAA@XZ` (must classify: sole-diff, ours-extra, slot 0x54 → class B; slot 0x50 → class A/agree) | Smoke passes |
| 2 | Full scan over the 85–99.99 pool (~1,700 fns; `gen_nearmiss_pool.py --min 85`); triage counts per class × purity | Scan JSON + triage table |
| 3a | Class-A harvest fan-out (worktree agents, patch-queue land) | Each fix objdiff-verified |
| 3b | Class-B sizing decision → header trials if ≥ ~10 shared-signature fns | Zero-strict-regression A/B per trial |
| 4 | T5 fold-in: pattern-doc split, `stw-classify` mode, verdict walls for class C | Committed, path-limited |
| 5 | T4 second-wave signatures if phase 2 shows adjacent populations | — |

Phases 1–2 run as one workflow (build → scan → triage); phase 3 fans out from
the triage JSON as a second workflow; phase 4 is small enough for a single
agent pass.

## 4. Artifacts / state

- Scan pool: `~/tmp/spill_pool.json`; scan output: `~/tmp/spill_scan.json`;
  triage summary: `~/tmp/spill_scan_triage.md` (scratch — durable results get
  folded back into this doc's §5 as they land).
- Memory: `project_spill_store_mechanism_2026-07-10.md` (mechanism),
  this campaign will get its own topic file at close.

## 5. Results log

*(append as phases complete)*

- 2026-07-10: campaign opened; phases 0–2 launched.
