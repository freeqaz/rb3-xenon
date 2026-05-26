# Plan — `FindXboxVtables` analysis pass for the local jeff fork

**Goal.** Add a `FindXboxVtables` analysis pass to `/home/free/code/milohax/jeff`
that detects C++ vtables in `.rdata` and emits two outputs:

1. Synthetic `vftable_<addr>` `ObjSymbol` records into `CfaConfig.known_symbols`
   (same channel `FindSaveRestSledsXbox` uses), so CFA treats each vtable's run
   of function pointers as authoritative function seeds.
2. A new `build/<title>/proposed_splits.txt` written from the `disasm` command —
   one block per detected vtable, with the implied `.text` and `.rdata` hulls
   formatted as `splits.txt` candidates a human can paste into
   `config/45410914/splits.txt`.

The goal is *proposing candidate TU boundaries*, not RTTI-aware naming.

## 1. Verify the signal (already done — but the impl agent must redo it)

Re-run the empirical probe against
`build/45410914/asm/auto_00_82000400_rdata.s` before touching jeff. Numbers
from the verification round on 2026-05-26:

| metric        | value |
|---            |---    |
| runs ≥ 4      | 2,741 |
| runs ≥ 8      | 884   |
| runs ≥ 16     | 176   |
| max run       | 136   |

Probe (Python — awk drops the alt-branch in negative-lookahead):

```python
import re
fn_re = re.compile(r'\.4byte fn_')
with open('build/45410914/asm/auto_00_82000400_rdata.s') as f:
    runs=[]; cur=0
    for line in f:
        if fn_re.search(line): cur += 1
        elif line.strip().startswith('.'):
            if cur >= 4: runs.append(cur)
            cur = 0
print(len(runs), sum(r>=8 for r in runs), sum(r>=16 for r in runs), max(runs))
```

EH-version-marker spot-check: `grep -n "0x19930521\|0x19930522"
build/45410914/asm/auto_00_82000400_rdata.s` returns hits starting at
`0x19930522` near rdata offset ~`0x14F30` — confirms MSVC EH metadata
co-resides in the same section, available as an optional strengthener.

Spot-check the first ≥8 run: `0x8200E9D4` len=11. Open
`config/45410914/symbols.txt` and confirm 11 consecutive `fn_…` targets it
references are real text-section functions. Reject the hypothesis if not.

## 2. Algorithm spec

Inputs: a fully-loaded `ObjInfo` (sections + `known_functions` populated by
pdata processing).

For each section with `kind == ObjSectionKind::ReadOnlyData`:

1. Walk u32 big-endian values at 4-byte aligned offsets.
2. A value `v` is a **fn hit** iff some text section `t` exists where
   `SectionAddress::new(t, v)` is in `obj.known_functions`.
   (`known_functions` is `BTreeMap<SectionAddress, Option<u32>>` —
   see `obj/mod.rs:82`.)
3. Track current run: `(run_start_rdata_addr, run_len, text_section_idx,
   collected_fn_addrs)`. A non-hit, or a hit pointing into a *different* text
   section than the run's first hit, terminates the run.
4. On run termination, emit a candidate iff **all** of:
   - `run_len >= 4`
   - `run_start_rdata_addr` is 8-byte aligned (vtable layouts are
     pointer-aligned; on 32-bit Xbox PowerPC that's 4-byte, but MSVC emits
     vftables 8-byte aligned in practice — keep this conservative filter and
     drop it to 4 if recall is too low after first run).
   - **No overlap** with `CfaConfig.skip_ranges` (the existing
     jump-table-in-rdata regions). `skip_ranges:
     BTreeMap<SectionAddress, SectionAddress>` per `cfa.rs:139`.
   - `.text` hull: `last_fn_addr + size_of_last_fn - first_fn_addr <= 0x10000`
     (64 KB). Skips cross-TU inherited-vtable cases where virtuals from
     multiple TUs land in one vtable.
5. **Optional strengthener (off by default, gated behind a config flag for the
   first run so we can measure FP rate):** look for `0x19930521` or
   `0x19930522` u32 within `±0x40` of `run_start_rdata_addr`. Record this as a
   confidence bit on the candidate, do not gate on it.

Emit, for each accepted candidate:

- A `vftable_<rdata_addr>` `ObjSymbol`:
  ```rust
  ObjSymbol {
      name: format!("vftable_{:08X}", rdata_addr),
      address: rdata_addr as u64,
      section: Some(rdata_section_idx),
      size: (run_len * 4) as u64,
      size_known: true,
      flags: ObjSymbolFlagSet(ObjSymbolFlags::Global.into()),
      kind: ObjSymbolKind::Object,
      ..Default::default()
  }
  ```
  inserted via `config.known_symbols.entry(rdata_section_addr).or_default()
  .push(...)`.
- A `VtableCandidate` record buffered for the `proposed_splits.txt` writer
  (text hull, rdata hull, EH-magic bit, fn count).

## 3. Exact file:line edits in jeff

### 3a. `jeff/src/analysis/pass.rs` — new pass after line 180

After the closing `}` of `impl AnalysisPass for FindSaveRestSledsXbox` (line
180), insert:

```rust
/// Vtable record for downstream consumers (e.g. proposed_splits.txt emitter).
#[derive(Debug, Clone)]
pub struct VtableCandidate {
    pub rdata_addr: SectionAddress,
    pub fn_count: u32,
    pub fn_addrs: Vec<SectionAddress>, // in vtable order
    pub text_hull: (SectionAddress, SectionAddress),
    pub eh_magic_nearby: Option<u32>, // 0x19930521 / 0x19930522 if within +/- 0x40
}

pub struct FindXboxVtables {}

impl FindXboxVtables {
    pub const MIN_RUN: u32 = 4;
    pub const MAX_TEXT_HULL: u32 = 0x10000;
    pub const EH_PROBE_RANGE: i32 = 0x40;

    /// Returns the collected candidates so the caller (xex.rs disasm path) can
    /// also write them to proposed_splits.txt. The known_symbols side-effect
    /// happens regardless.
    pub fn execute_collect(
        config: &mut CfaConfig,
        obj: &ObjInfo,
    ) -> Result<Vec<VtableCandidate>> {
        // … scan loop here; pushes ObjSymbol into config.known_symbols and
        // returns the Vec<VtableCandidate>.
        todo!()
    }
}

impl AnalysisPass for FindXboxVtables {
    fn execute(config: &mut CfaConfig, obj: &ObjInfo) -> Result<()> {
        let _ = Self::execute_collect(config, obj)?;
        Ok(())
    }
}
```

Implementation notes:

- Iterate sections with `obj.sections.iter().filter(|(_, s)| s.kind ==
  ObjSectionKind::ReadOnlyData)` (mirrors `pass.rs:200`).
- Read u32s with `u32::from_be_bytes(...)`. Use `section.data` directly the
  way `FindSaveRestSledsXbox` does (`pass.rs:140`).
- For each `v`, do **one** `BTreeMap::range` lookup: text sections are few
  (usually 1–2), so a precomputed `Vec<(SectionIndex, range)>` of text section
  bounds + a check against `obj.known_functions.contains_key(...)` is fine.
  Don't iterate all known_functions per word — that's O(N·M).
- `size_of_last_fn`: `obj.known_functions.get(&last_fn_section_addr).unwrap()`
  is `Option<u32>`. If `None`, fall back to 4 (we won't be exact, that's fine
  for the hull estimate — note it in the proposed_splits comment).
- Skip-range check: a candidate `[start, start + run_len*4)` overlaps an entry
  in `config.skip_ranges` iff its half-open interval intersects any
  `[k, v)` in the map. With a `BTreeMap<SectionAddress, SectionAddress>`
  keyed by start, a single `range(..end).next_back()` lookup suffices.
- EH magic probe: scan the same `section.data` for `0x19930521` /
  `0x19930522` at any 4-byte aligned offset in
  `[rdata_addr - 0x40, rdata_addr + 0x40)`.

### 3b. `jeff/src/cmd/xex.rs:583` and `:625` — register the pass

At `xex.rs:583` (in `analyze`, after `FindSaveRestSledsXbox::execute(...)?;`):

```rust
FindSaveRestSledsXbox::execute(&mut config, &obj)?;
FindXboxVtables::execute(&mut config, &obj)?;       // NEW
let result = run_cfa(&obj, &config)?;
```

At `xex.rs:625` (in `disasm`, after `FindSaveRestSledsXbox::execute(...)?;`):

```rust
FindSaveRestSledsXbox::execute(&mut config, &obj)?;
let vtables = FindXboxVtables::execute_collect(&mut config, &obj)?;  // NEW
let result = run_cfa(&obj, &config)?;
```

Use `execute_collect` on the disasm path so we can also pipe `vtables` into
the new writer.

Add the corresponding `use` import for `FindXboxVtables` at the top of
`xex.rs` (where `FindSaveRestSledsXbox` is already imported).

### 3c. New file `jeff/src/util/proposed_splits.rs`

Public API:

```rust
use std::path::Path;
use anyhow::Result;
use crate::analysis::pass::VtableCandidate;
use crate::obj::ObjInfo;

pub fn write_proposed_splits(
    out_path: &Path,
    obj: &ObjInfo,
    vtables: &[VtableCandidate],
) -> Result<()>;
```

Add `pub mod proposed_splits;` to `jeff/src/util/mod.rs`.

Call site: in `xex.rs` `disasm`, after the existing `update_splits` /
`split_obj` work and before / alongside the `config.json` write, locate the
output dir of `args.out` and write `<out_dir>/proposed_splits.txt`.

### 3d. Sample `proposed_splits.txt` block format

```
# vtable @ 0x82015820  (24 virtuals)  EH magic: 0x19930521 within 0x40: yes
# .text hull: 0x82261A40..0x82262E80  (0x1440 bytes)
# .rdata hull: 0x82015820..0x82015880 (0x60 bytes)
class_82015820.cpp:
    .text  start:0x82261A40 end:0x82262E80
    .rdata start:0x82015820 end:0x82015880
```

If `EH magic ... within 0x40: no`, still emit — the operator decides. Sort
candidates ascending by `rdata_addr`. Emit a one-line header
`# Generated by jeff FindXboxVtables — DO NOT EDIT — paste-bait for splits.txt`
at the top.

## 4. Verification protocol

1. Rebuild jeff:
   `cd /home/free/code/milohax/jeff && cargo build --release` — must succeed
   clean (no new warnings on the new files).
2. Re-run rb3-xenon ninja: `cd /home/free/code/milohax/rb3-xenon && touch
   config/45410914/config.yml && ninja`. SPLIT must still complete (jeff's
   `Make dtk xex split run end-to-end on RB3 retail XEX` invariants).
3. Inspect `build/45410914/proposed_splits.txt` — expect ~hundreds to ~thousand
   blocks (≤2,741 ceiling minus overlaps and rejects).
4. Spot-checks against already-pinned TUs in `config/45410914/splits.txt`:
   `MasterAudio.cpp` (`.text 0x82758380..0x8275A534`), `BandDirector.cpp`
   (`0x8227E728..0x8227EF2C`), `BandWardrobe.cpp`
   (`0x8231CCB0..0x8231E258`). For each, grep `proposed_splits.txt` for any
   candidate whose `.text` hull is a *subset* of the pinned range — that's a
   true positive. Count them. Below 50% recall on these three is a red flag;
   tighten the alignment / hull filter.
5. Cross-check against `tools/autoid.json` and `tools/bindiff_match.json` (if
   present) — count how many vtable rdata addrs match existing class IDs.

## 5. Risks + mitigations

- **False positives — function pointer tables, dispatch tables, hand-written
  jump tables not yet caught by the existing pass.** Mitigations:
  - 8-byte alignment requirement for run start (most non-vtable tables are
    4-byte aligned only).
  - Optional EH-magic strengthener (kept off for first run; turn on if FP rate
    visibly high).
  - 64 KB text-hull cap.
  - Exclusion against `config.skip_ranges` (existing jump-table-in-rdata
    detections — `analysis/mod.rs:770+`).
- **Cross-TU vtables (multiple inheritance / vbase ptrs leaking across
  modules):** the 64 KB hull cap drops these. Cost: we lose some legitimate
  large vtables; we surface them only as a TODO log line at `debug!`.
- **Performance:** ~2 MB of rdata, linear u32 scan with a hashed
  `known_functions` lookup → < 1 s. Negligible vs the CFA itself.
- **Channel conflict with existing CFA seeds:** none — we write to the same
  `config.known_symbols` map `FindSaveRestSledsXbox` uses (`pass.rs:154`);
  CFA treats those as authoritative.
- **Out-of-tree feature in jeff:** this is an rb3-xenon-specific enhancement.
  Mention in commit message that it should *not* be upstreamed yet (let the
  fork land it first, prove value, upstream later — same as the asm-write
  downgrade and CFA panic→warn fixes already on the fork).

## 6. Out of scope (do NOT implement in this pass)

- **RTTI-aware naming.** Parsing MSVC's `_TypeDescriptor` /
  `_RTTICompleteObjectLocator` / `_RTTIBaseClassDescriptor` graph to recover
  class names. Worthwhile but separate work.
- **Base-class chain / vbase ptr reconstruction.** Needed for proper class
  modeling but not for *split candidate proposal*.
- **Automated patching of `splits.txt`.** `proposed_splits.txt` is paste-bait;
  humans review and merge.
- **Cross-section vtables (vtables that mix text-section targets).** The
  algorithm terminates a run at section boundary; we don't try to recover
  these.

## 7. References

- `jeff/src/analysis/pass.rs:121-180` — `FindSaveRestSledsXbox` (pattern to
  mimic — section iter, data scan, `known_functions` validation,
  `known_symbols` emission).
- `jeff/src/analysis/cfa.rs:27-69` — `SectionAddress`.
- `jeff/src/analysis/cfa.rs:135-142` — `CfaConfig` (target output channel).
- `jeff/src/obj/mod.rs:57-130` — `ObjInfo` and `known_functions` field.
- `jeff/src/analysis/mod.rs:114-180`, `770+` — existing jump-table-in-rdata
  code (the FP-source we must NOT overlap with).
- `jeff/src/cmd/xex.rs:580-591` — `analyze` pass-registration site.
- `jeff/src/cmd/xex.rs:613-660` — `disasm` pass-registration + output-dir site.
- `jeff/src/util/split.rs:1104` — `update_splits`; we sit alongside it and
  don't modify it.
- `rb3-xenon/build/45410914/asm/auto_00_82000400_rdata.s` — empirical signal:
  2,741 / 884 / 176 / max 136. EH magic `0x19930522` first seen at file line
  1527 (~rdata offset `0x14F30`).
- `~/.claude/projects/-home-free-code-milohax-rb3-xenon/memory/project_jeff_fork.md`
  — fork state, what NOT to break.
- `~/.claude/projects/-home-free-code-milohax-rb3-xenon/memory/feedback_verify_assumptions.md`
  — why §1 (re-verify the signal) is mandatory, not optional.

## 8. Numbered impl-agent step list

1. Read this plan top-to-bottom; do not skip §1.
2. Re-run the §1 probe from inside `rb3-xenon/`; confirm 2,741 / 884 / 176 /
   max 136 on `auto_00_82000400_rdata.s`. If the numbers differ by >5%, stop
   and re-plan — the underlying asm has changed.
3. Spot-check one ≥8 run (e.g. `0x8200E9D4` len 11): open `symbols.txt`,
   confirm at least 4 of the 11 `fn_…` targets are real text-section
   functions. If <50%, stop — premise broken.
4. In `jeff/src/analysis/pass.rs`, add `VtableCandidate` struct and
   `FindXboxVtables` struct + `execute_collect` + `AnalysisPass` impl after
   line 180. Implement the §2 algorithm.
5. In `jeff/src/util/`, create `proposed_splits.rs` with the §3c API. Wire
   `pub mod proposed_splits;` into `util/mod.rs`.
6. In `jeff/src/cmd/xex.rs`: add `use` for `FindXboxVtables` (and
   `VtableCandidate` if needed) and `write_proposed_splits`. Insert the call
   at line 583 (analyze) using `::execute`. Insert at line 625 (disasm) using
   `::execute_collect` and capture `vtables`. After `update_splits` / before
   the final return, call `write_proposed_splits(out_dir.join(
   "proposed_splits.txt"), &obj, &vtables)?`.
7. `cd /home/free/code/milohax/jeff && cargo build --release`. Fix until
   green; do not introduce new clippy lints on the new files.
8. `cd /home/free/code/milohax/rb3-xenon && touch config/45410914/config.yml
   && ninja`. SPLIT must succeed (regression check against
   `project_jeff_fork.md` invariants).
9. Inspect `build/45410914/proposed_splits.txt`. Sanity-check candidate
   count is in `[200, 2_741]`.
10. Run the §4 spot-check against `MasterAudio.cpp`, `BandDirector.cpp`,
    `BandWardrobe.cpp` pinned `.text` ranges in `splits.txt`. Report the
    hit / miss counts.
11. Report:
    - candidate count (total / EH-magic-confirmed / hull≤4 KB / 4–16 KB /
      16–64 KB),
    - spot-check hit/miss for the three pinned TUs,
    - any new warnings emitted,
    - any SPLIT regressions (should be zero),
    - rough wall time for the new pass (instrument with `log::info!` at
      pass entry/exit).
12. Do **not** commit. Leave the working tree dirty for review.
