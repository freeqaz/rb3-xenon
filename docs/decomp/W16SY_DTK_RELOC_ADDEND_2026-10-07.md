# W16-SY — dtk drops relocation addends on split target objs (2026-10-07)

**Lane:** W16-SY · **lever:** `CAMPAIGN_STATE_2026-10-07.md` §6 lever 8 (first finding in
`W16PW_CONSTANT_VALUE_AUDIT_2026-10-06.md` §"two instrument findings") ·
**jeff branch:** `w16-sy-reloc-addend` @ `cd2495c` (dtk **1.15.0**) ·
**rb3-xenon branch:** `w16-sy` (the converged `symbols.txt` + this record).

## The defect

`write_coff` cannot encode an addend on `IMAGE_REL_PPC_REFHI`/`REFLO`. It zeroes the 16-bit
immediate and writes the PAIR displacement as 0. When the tracker resolved a load to
`lbl_X + N` (because `lbl_X`'s inferred size spanned the target), the split object relocated it
to `lbl_X`. Example: `ProfileMgr::GetJoypadExtraLagInits` loads 74.0 from `0x82091FC0`, but
`lbl_82091FBC` (14.0, renamed `__real@41600000`) was `size:0x8`. objdiff therefore read the
target as loading 14.0 and charged our correct `__real@42940000`.

## Why the fix is a new label, not an encoded addend

MSVC never emits such an addend. I scanned all **1,268** compiled objs in this tree:
**678,382** REFHI/REFLO records. Every PAIR displacement is 0, and every immediate is 0 except
2, which are DS-form `lwa` opcode bits (`imm & 3 == 2`), not addends. So an `@l` that lands
inside a dtk label means the original object had a symbol starting there, and the label's size
is what is wrong.

## The pass (`anchor_interior_data_references`, jeff `src/cmd/xex.rs`)

The pass runs after the post-repair retrack and before `symbols.txt` is written. It applies to
every code `@ha`/`@l` relocation whose target is a non-function symbol in a non-code section,
with a positive addend that falls inside that symbol. For each one it:

- carves a label at the exact address;
- sizes the pieces to the next split point;
- takes the data kind and alignment the tracker inferred for that address (new public
  `Tracker::inferred_data_kind`, which `apply` now also uses);
- re-points every relocation that lands in a piece. ADDR32 relocations keep their effective
  target.

Scope:

- Only generated labels (`lbl_`, `jumptable_`, `stringBase`) and `vftable_` are carved. Named
  objects are left alone.
- `vftable_` is included because FindXboxVtables merges adjacent function-pointer tables that
  have no COL between them, in code built without RTTI (Quazal, XDK). Example:
  `lis r4,0x8218; addi r3,r4,0x4894; stw r3,0x50(r1)` stores the address of the second table
  inside `vftable_82184888` (`size:0x20`).

## Census (RB3 clean TU5)

| | sites | containers | labels created |
|---|---:|---:|---:|
| into `lbl_` | 169 | 16 | 79 |
| into `vftable_` | 308 | 8 | 72 |
| **total** | **477** | **24** | **151** |

All 477 relocations were re-pointed. `symbols.txt`: +151 labels, 24 sizes shrunk, 0 symbols
removed or renamed.

**Not done:** 294 `@ha/@l` sites with an addend into **function** symbols (135 functions,
r12-based code addresses in XDK, e.g. `xdk/xgraphics/import.s`). Carving code symbols has its
own `.pdata`/boundary hazards, and it is out of scope.

## Fixed point

One split starting from main's `symbols.txt` produces the converged file. The next split leaves
it byte-identical. (Before the pass used the tracker's inferred kinds it took two splits: the
second added only `data:`/`align:` attributes on 4 labels.) ⚠ That first split **trips the split
guard** ("THE SPLIT REWROTE ITS OWN INPUT"), as any `symbols.txt`-changing dtk must. The
converged `symbols.txt` therefore has to land **together with** the binary swap.

## Measurement

Setup: worktree `~/tmp/wt-w16sy` off main `379611b73`, ruler `name_check`. Both legs were
settled (the settle build ran only the always-run CHECK/PROGRESS edges). `report.json` and
`report.cache` were wiped before each read, and each read did 0 compiles and 0 SPLITs.
I measured by hand rather than with `ab_measure`, because the change is a tool binary plus a
`symbols.txt` rewrite, and `ab_measure` refuses patches that touch `symbols.txt`.

| leg | dtk | matched fns | matched_code | code% |
|---|---|---:|---:|---:|
| A | live `67e4a0f` (1.14.0) | 54,865 | 6,064,756 | 59.180800 |
| B | patched (pre-commit build) | 54,866 | 6,065,204 | 59.185173 |
| F | committed `cd2495c` (1.15.0) | 54,866 | 6,065,204 | 59.185173 |

**Δ = +1 fn / +448 B / +0.004373 pp.** `total_code` (10,247,844) and `total_functions` (68,884)
are unchanged, and F equals B on every measure. A row-level diff of A vs B finds **exactly one
changed row**: `?GetJoypadExtraLagInits@ProfileMgr@@…` 99.91071 → 100.0 (fuzzy and mpn). This
was predicted before measuring: `0x82091FC0` has no map entry, so the target becomes a
placeholder `lbl_82091FC0`, which `name_check` forgives against our `__real@42940000`.

The other 476 re-anchored sites move no score. They sit in out-of-scope or unpaired rows, where
the placeholder target was already forgiven. Their value is fidelity: a reader through
relocations (e.g. `tools/const_value_audit.py`) now sees retail's address. After the fix, the
dtk asm has **0** `@ha/@l` with an addend into a data symbol, and `std::exception::what` loads
`lbl_8200EB00`.

## Native gate

Run in the worktree after the converged split:
`NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`.

## Tests (jeff)

- Two end-to-end tracker fixtures. The first reproduces the defect (addend 4), shows the pass
  anchors it, and shows that a re-track is a fixed point. The second shows a named object is
  left untouched.
- The first test **goes red with the pass disabled**.
- Full suite **178/178**.

## For the coordinator

- Swap `~/tmp/jeff-build-sy/release/dtk` (sha256 `794e3495…dd873`, `dtk 1.15.0 (cd2495c…)`)
  into `jeff/target/release/dtk`, backing up the live binary first.
- Land rb3-xenon `w16-sy` (the `symbols.txt` + this doc) at the same time.
- `configure.py`'s `config.dtk_tag` still says `v1.13.0`. It was already stale before this lane
  and is not a selector.
- The swap also affects `../dc3-decomp`, which resolves the same binary. Its split was **not**
  measured here.
