---
name: stack-layout
description: Diff stack-frame layouts between target and base for a function. Labels base-side slots with source variable names from a MSVC /Z7 CodeView recompile. Identifies SWAPPED pairs (decl-reorder candidates), SHIFTED slots, DIFFER (different variables in same slot), and TGT_ONLY / BASE_ONLY (extra/missing locals). Filters out callee-save slots.
argument-hint: "<symbol_name> [-u <unit>]"
allowed-tools: Bash(python3 scripts/analysis/stack_layout.py *), Bash(python3 scripts/analysis/codeview_locals.py *), Bash(python3 scripts/analysis/diff_inspect.py *), Bash(python3 scripts/harvest/class_layout_report.py *), Read, Grep, Glob
---

# Stack Layout Diff Skill (MSVC X360)

Compare stack-frame layouts between target and our build for a single function.
Returns frame size and callee-save counts at the top, then a per-slot diff table
with verdicts that point to specific source fixes.

## Arguments

`$ARGUMENTS` — the symbol (MSVC mangled `?Name@Class@@QAEHXZ` or demangled
`Class::Method(args)`). Append `-u <unit>` if ambiguous (e.g.
`-u default/system/hamobj/HamCharacter`).

## Steps

1. **Run the diff**:
   ```bash
   python3 scripts/analysis/stack_layout.py --symbol "$ARGUMENTS" --project-dir .
   ```

2. **Read the prologue summary** at the top:
   - `Frame Δ` and `Callee-saved GPR/FPR Δ`
   - On MSVC X360, GPRs are 64-bit (`std`) — 8 bytes per saved GPR.
   - If frame Δ is **fully explained by callee-save counts** → AT_LIMIT
     (not source-fixable).
   - Otherwise the structural Δ remainder is the real lever.

3. **Read the verdict table**. Rows are sorted most-actionable first:

   | Verdict | Meaning | Action |
   |---|---|---|
   | **SWAPPED** | Two slots' fingerprints exchanged | Reorder the two declarations |
   | **DIFFER** | Same offset, different fingerprint | Different variable lives there — decl-reorder |
   | **SHIFTED** | Same fingerprint, offset differs by the dominant Δ | One side has an extra local pushing the rest |
   | **TGT_ONLY** | Slot exists only on target | Target spills a temp we keep in a register (or vice versa) |
   | **BASE_ONLY** | Slot exists only on our build | Extra spill; usually a register-pressure symptom |
   | **MATCH** | Hidden by default; pass `--show-equal` to see |

4. **Fingerprint columns** (`kind sz=N L=loads S=stores A=accesses [first..last]`):
   - `float sz=4` → `float`
   - `float sz=8` → `double` / paired-single store
   - `int sz=4` → `int`, pointer, `bool`, or 32-bit member
   - `int sz=8` → 64-bit `std`/`ld` (frequent on Xenon)
   - `addr sz=0` → an `addi rN, r1, off` taking address-of

5. **"base var" column** — the source variable name our build allocates at that
   offset, extracted from a MSVC `/Z7` CodeView recompile. Use it to identify
   exactly which declaration to reorder.

## When to Use

- `run_objdiff` output flagged `**Stack:** frame Δ ... | N SWAPPED ...`
- Function diff shows many `[off:+N]` annotations
- Frame sizes don't match between target and our build
- You suspect a declaration reorder is the fix
- ★ The whole diff is **one differing immediate** (`S=1`, so the function scores
  99.9x%) and that immediate is `r1`-relative. That tier is a stack/layout tier
  wearing an immediate mask — see the sizing rule below.

## ★ First: is it a STACK slot or a CLASS offset?

An `S=1` / single-immediate diff splits cleanly by base register, and the two
halves have completely different fixes:

| The differing immediate is… | It is a… | Fix with |
|---|---|---|
| `r1`- or (frame-aliased) `r31`-relative | **stack slot** | this skill — decl reorder / extra-or-missing local |
| relative to an object pointer (`r3`, `r30`, …), or a `sizeof` in a `new`/`memset`, or a `subi rX, rY, N` base-sub-object adjust | **class layout** | `/struct-info` + `scripts/harvest/class_layout_report.py` |

For the class-layout half, get ground truth from the compiler — **not** from the
`// 0xHEX` header comments and not from `lookup_struct_offset`, both of which are
measurably wrong in several headers:

```bash
python3 scripts/harvest/class_layout_report.py <Class> --exact --no-vtable --project-dir .
python3 scripts/harvest/class_layout_report.py <Class> --offset 0x118 --project-dir .
python3 scripts/harvest/class_layout_report.py <Class> --check-header --project-dir .
```

That report also prints `this` adjustors, which is what a
`subi r3, r29, 0x1c` vs `0x64` mismatch is actually measuring — a base
sub-object sitting at the wrong offset, not a stack slot.

Note also that a *class* size change moves stack slots too: a caller that holds a
by-value temp of a mis-sized class shows up here as a frame Δ. If frame Δ is a
multiple of a class's size delta, fix the class first and re-run.

## Output knobs

- `--no-names` — skip CodeView recompile + name extraction
- `--show-equal` — include MATCH rows
- `--show-callee-save` — include prologue/epilogue callee-save slots (hidden by default)
- `--json-file <path>` — skip objdiff invocation; load diff JSON from cached path

## How name extraction works

The tool recompiles the function's source file with `/Z7` to embed CodeView
records in the `.debug$S` COFF section, parses `S_REGREL32` records, and maps
each register-relative variable to a frame-offset → name pair. Cached at
`/tmp/claude/stack_codeview/<base>.<hash>.cv.obj` by source mtime + cflags
hash; second runs are ~0.5s.

Frame-register handling: CodeView records `reg=2` (= PPC r1) or `reg=32`
(= PPC r31). MSVC X360 commonly aliases r31 to `new r1` via
`subi r31, r1, FRAMESIZE` before `stwu`, so r31-relative offsets equal
r1-relative offsets after frame allocation. The tool accepts both.

Limits:
- **Base side only**: there's no debug build oracle for the target. TGT_ONLY
  rows show no name.
- **Compiler temps unnamed**: `/O1` strips many locals into registers — empty
  "base var" cell ≠ "unknown variable" — it's "no source declaration."
- **Same name in nested scopes**: deeper scope wins via min-depth merging.

## Detection limits

- Callee-save detection handles:
  - `bl __savegprlr_NN` / `bl __savefpr_NN` helpers
  - Manual pre-stwu saves: `stw r12, -8, r1`, `std rN, -off, r1`, `stfd fN, -off, r1`
  - Post-stwu `stmw rN, off, r1` (rare on X360)
- Unusual prologue shapes may under- or over-count; verify against the asm if
  the frame summary looks off.

## MSVC X360 prologue cheat-sheet

```
mflr r12                              ; LR -> r12 (not r0 like Wii MWCC!)
bl __savegprlr_NN                     ; r12->-8(r1), r31..rNN std at -16, -24, ...
stfd f31, -0x?, r1                    ; manual FPR save (pre-stwu, NEGATIVE offset)
subi r31, r1, FRAMESIZE               ; optional: r31 = new r1 (frame-ptr alias)
stwu r1, -FRAMESIZE, r1               ; allocate frame
... body code (uses r1 or r31 base) ...
addi r1, r1, FRAMESIZE                ; deallocate
b __restgprlr_NN                      ; restore + return
```

After `stwu`, the saved-register slots end up at `frame_size - 8` (LR) down to
`frame_size - 8 - 8*(saved_count-1)`.

## Tips

- After a declaration reorder, re-run to confirm SWAPPED rows resolve.
- `Dominant body-offset shift` is reported separately from frame Δ — the
  dominant shift is what SHIFTED rows are normalized against.
- If verdicts are all DIFFER with no clean SHIFT/SWAP, the function is
  mid-reflow; try `/compare-asm` or `run_diff_inspect mode=diagnose`.
