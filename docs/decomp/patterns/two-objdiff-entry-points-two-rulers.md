# `objdiff-cli diff` and `objdiff-cli report generate` were two different rulers

**Repo: rb3-xenon** (Rock Band 3, Xbox 360 retail, MSVC/PowerPC, title `45410914`).
Numbers below are this binary's. The same defect exists in `../dc3-decomp` and
`../rb3` with different populations — do not quote one repo's figure against
another's binary.

**Status:** cause found upstream, fixed in project config 2026-08-31, guarded by
`scripts/verify_ruler_agreement.py` and by a ninja edge gating `REPORT`.
Read this before comparing any per-function number against `report.json`.

## The mechanism

The two CLI entry points carry **different hardcoded base configs**, and neither
is the schema default:

| | `report generate`<br>`objdiff-cli/src/cmd/report.rs:581` | `diff`<br>`objdiff-cli/src/cmd/diff.rs:1070`<br>(and `--batch` at `diff.rs:1807`) |
|---|---|---|
| `functionRelocDiffs` | `none` | `data_value` |
| `combineDataSections` | **true** | false (schema default) |
| `combineTextSections` | **true** | false (schema default) |
| `ppc.calculatePoolRelocations` | **false** | **true** (schema default) |

Both then layer `objdiff.json`'s `options` block on top. Since `d04c83df`
(2026-08-12) that block set only `functionRelocDiffs`, so it fixed the ruler the
two paths argue about most visibly and left them disagreeing on the other three.

`ppc.calculatePoolRelocations` is the one that bites. It **synthesizes**
`R_PPC_NONE` relocations for pooled data loads —
`objdiff-core/src/arch/ppc/mod.rs:819 make_fake_pool_reloc`, reached from
`objdiff-core/src/obj/read.rs:708` — and the config schema calls them *"fake
relocations"* in as many words. They are reconstructed per object by walking that
object's control flow and looking the computed address up **in that object's own
symbol table**. A dtk-carved *target* obj (a whole linked data section, anonymous
`lbl_*` labels) and our MSVC per-TU COMDAT *base* obj do not reconstruct the same
set.

`reloc_eq` then charges the asymmetry:

```rust
// objdiff-core/src/diff/code.rs:1330-1338
(None, Some(_)) => return relax_reloc_diffs || name_check,   // base-only: forgiven
(None, None)    => return true,
_               => return false,                              // TARGET-only: CHARGED
```

A relocation present on one side and absent on the other is charged under
**every** `functionRelocDiffs` mode except `none` — `name_check` included. So a
synthesized *display annotation* that only one side reconstructs costs a real
point, and the charged row can be two **textually identical** instructions.

This is **upstream objdiff behaviour, not a fork bug**: the three extra
report-side values arrive in `0c9e552 "Combine sections when generating report"`
(Luke Street, 2025-05-07), which touched `report.rs` only. `bin/objdiff-cli` is
a symlink shared with `../rb3` and `../dc3-decomp`, so all three repos were
exposed, and the fix is config-only in each — **no tool rebuild**.

## Scope on this binary: 102 functions, 55,604 bytes

Whole-binary sweep, rb3-xenon worktree at `26576070`, full `./tools/ninja-locked`
completed before reading `report.json`, one objdiff-cli **4.2.8**
(`358c715835cc`, xxh3 `9b2bb6f1f3a21062`), `diff --batch` over every
uniquely-named function in the report:

* **comparable rows** (a real percent on both sides): **47,208**
* **disagreements attributable to the config split: 102 (55,604 bytes)**
* direction: `report` higher on **100**, `diff` higher on **2**
* magnitude: up to **65.20 pp**
* **1** of them (308 B, `?GetWearing@CustomizePanel@@QAA?AVSymbol@@XZ`) reads
  exactly 100.0 in `report.json` and <100 through `diff` — the class where a lane
  refuses a promotion for a reason that does not exist

Not disagreement, and not counted as either:

* **22,009 unpaired rows** — `diff --batch` returns `null`, the report returns
  `0.0`. Both say "no base symbol"; that is agreement.
* **123 rows carrying `base_unit`** — the batch path's *disclosed* cross-unit
  COMDAT fallback, which finds the body in another unit's base obj. The report
  scores per-unit only. Those two numbers answer different questions.

### Attribution: all four keys, not just the pool one

| config applied to `diff` | disagreements |
|---|---|
| as configured (only `functionRelocDiffs` pinned) | **102** |
| `+ ppc.calculatePoolRelocations=false` alone | **0** |
| `+ combineDataSections/combineTextSections=true` alone | **104** |
| all four pinned | **0** |

`ppc.calculatePoolRelocations` alone explains 102/102. **The other two are not
inert**: applied *without* the pool key they add two fresh disagreements
(`?Intersect@?$kdTree@VTriangle@@@@…` 90.03 → 89.55,
`?DataVarName@@YAPBDPBVDataNode@@@Z` 100.00 → 99.81). Pin all four together.

## The fix

One project-config change, in `tools/project.py`'s `options` block — **both** CLI
entry points layer it:

```python
"options": {
    "functionRelocDiffs": "name_check",
    "combineDataSections": True,
    "combineTextSections": True,
    "ppc.calculatePoolRelocations": False,
},
```

**It changes no recorded number.** Same worktree, full build before and after:

| | matched_functions | matched_code | matched_code_percent | fuzzy_match_percent |
|---|---|---|---|---|
| before | 42,274 | 3,772,560 | 36.819992 | 48.921097 |
| after  | 42,274 | 3,772,560 | 36.819992 | 48.921097 |

objdiff certifies this independently: the post-change `REPORT` run logged
**`Report cache: 3091 hits, 0 misses`**, and that cache key covers *the resolved
config*. Zero misses means the report path's effective config is byte-identical —
which it must be, since the three values we added are the ones `report generate`
already hardcoded. The whole-binary re-sweep after the change: **0** disagreements
(47,207 examined, 123 `base_unit`, 22,010 unpaired).

## Consequences

* **The headline is not overstated by this.** The report path was never the lower
  of the two on any of the 100 rows where they differed by more than rounding.
  Nothing `report.json` counts as matched was being forgiven here.
* **Per-function readings below the headline were LOW**, by up to 65 pp, on 100
  functions. Any AT_LIMIT reasoning taken over those row sets was taken over
  phantom rows.

## Prior art, and why it did not close this

`scripts/analysis/ruler.py` already documented the base-config split, already
named `ppc.calculatePoolRelocations`, and already carried lane EB-4's measurement
("up to 14.75 pp on 118 of 1,639 named sub-100 rows"). It resolves the ruler at
runtime from `report.json`'s `provenance.diff_config`, which is the right design —
and it fixed `scripts/orchestrator/mcp_server.py`, so the MCP tools have been
correct since lane MCPRULER-1 (2026-08-14).

What it could not fix is everything that does **not** import it: a bare
`bin/objdiff-cli diff` on the command line, `--batch` sweeps, permuter scoring,
and any new script. `objdiff.json`'s `options` block is the one place *both* CLI
entry points read unconditionally, so pinning there covers the callers a Python
helper cannot reach.

## The guard

```
python3 scripts/verify_ruler_agreement.py --check      # ~0.2 s config-pin assertion
python3 scripts/verify_ruler_agreement.py --selftest   # ~40 s, with negative control
```

`--check` reads the effective config out of `report.json`'s own
`provenance.diff_config` (authoritative by construction: it is not a description
of the config, it *is* the config the score was taken under) and asserts each
divergent key is pinned in `objdiff.json`.

`--selftest` re-runs the end-to-end comparison with
`-c ppc.calculatePoolRelocations=true`, restoring `diff`'s own default, and
**requires** that to produce disagreements. If it does not, it exits **5**
("vacuous"), names the rotted witness set, and tells you to re-derive with
`--all` — it does not report success from a probe that examined nothing.
Measured on the fixed tree: **3,320 witness functions, 3,320 agree as configured,
31 disagree under the control flip**. The exit-5 path was itself verified by
pointing `WITNESS_UNITS` at a witness-free unit (`default/MasterAudio`): 126
functions examined, 0 disagreements either way, **exit 5**, not 0.

**Wired into the build.** `tools/project.py` emits a `CHECK RULER AGREEMENT` edge
whose stamp is an implicit input of `REPORT`, so a regenerated `objdiff.json` that
lost the pins stops the build instead of silently drifting. Verified by deleting
one pin from `objdiff.json` and building: `ninja: build stopped: subcommand
failed`, exit 1, and `REPORT` never ran.

The edge uses `--check --pins-only`, which deliberately skips the `report.json`
cross-check. That cross-check is the stronger assertion, but it is legitimately
false for exactly one build — the first one after a deliberate ruler change,
whose job is to replace the report it would be checked against. Gating `REPORT`
on it would deadlock. The pins have no legitimate transient, so that is what the
build asserts; the cross-check stays a manual step.
