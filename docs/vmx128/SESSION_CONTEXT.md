# VMX128 Ghidra Support - Session Context

Copy this into a new Claude session to continue work on VMX128 support.

---

## Project Summary

We're adding VMX128 (Xbox 360 SIMD) support to Ghidra to enable decompilation of vector-heavy code in the DC3 decomp project. Currently, large chunks of code are unintelligible because Ghidra doesn't understand VMX128 instructions.

**VMX128** = Xbox 360's extended AltiVec with 128 vector registers (vs standard 32) and ~80 additional instructions for 3D graphics/physics. No official docs exist (NDA only).

## Current State

- **Phase 1 COMPLETE**: Audited DC3's VMX128 usage
- **Phase 2 COMPLETE**: Fixed 0dinD's Ghidra fork (2025-01-25)
- **Phase 3 COMPLETE**: Added pcode semantics (lane-wise FP + compares)
- **Phase 4 COMPLETE**: Core VMX128 semantics + CR6 updates implemented; low-impact pack/unpack stubs remain
- **Phase 5 IN PROGRESS**: Applying VMX128 tooling to decomp targets

### Phase 1 Results

- **37,020 VMX128 instructions** found in DC3 binary
- **77 of ~80** VMX128 opcodes used
- **Top 8 instructions** account for 60% of usage:
  1. `vcmpgtfp128` (8,020) - Compare greater-than FP
  2. `lvx128` (3,719) - Load vector
  3. `vsldoi128` (2,758) - Shift left by octet immediate
  4. `stvx128` (2,709) - Store vector
  5. `vperm128` (1,961) - Permutation
  6. `vmulfp128` (1,701) - Multiply FP
  7. `vor128` (1,423) - Logical OR
  8. `vaddfp128` (1,039) - Add FP

### Research Repos

Located at `~/code/milohax/vmx128-research/`:
- `powerpc-rs/` - Complete VMX128 ISA in isa.yaml (authoritative reference)
- `PPC-Altivec-IDA/` - Working IDA plugin (encoding reference)
- `ghidra-vmx128/` - 0dinD's partial Ghidra implementation (disassembly works, no pcode)
- `xenia-reference/` - vmx128.txt and AltiVec PDF docs (encoding + semantics references)
- `qemu-reference/` - AltiVec/VMX helper implementations

## Issues Fixed in 0dinD's Fork (Phase 2)

1. ✓ Register size: `size=4` → `size=16` (128-bit vectors)
2. ✓ Missing immediate extractions:
   - `vpermwi128` - Added combined PERM field (8-bit)
   - `vrlimi128` - Added Zimm field (2-bit)
   - `vpkd3d128` - Added D3DType, VMASK, Zimm fields
3. ✓ Fixed pre-existing syntax error in `vperm128` (trailing comma)
4. ✓ **BUILD TESTED** - All 19 PowerPC SLEIGH variants compile successfully
5. ✓ Phase 3 semantics implemented (lane-wise FP + compares)

## Project Phases

1. **Audit DC3's VMX128 usage** ✓ COMPLETE
2. **Fix 0dinD's fork** ✓ COMPLETE (2025-01-25)
3. **Add pcode semantics** ✓ COMPLETE
4. **Test & iterate** ✓ COMPLETE
5. **Decomp targets** - VMX128-guided decomp in progress
6. **Upstream** - Contribute to Ghidra (future)

## Key Files

```
docs/vmx128/PLAN.md                    # Full project plan
docs/vmx128/DC3_VMX128_USAGE.md        # Phase 1 results - prioritized instruction list
docs/vmx128/ISA_REFERENCE.md           # All VMX128 instructions
docs/vmx128/REGISTER_ENCODING.md       # 7-bit register encoding scheme
docs/vmx128/GHIDRA_IMPLEMENTATION.md   # Sleigh implementation notes
tmp/vmx128/vmx128_scanner.py           # Binary scanner script
src/xdk/LIBCMT/vectorintrinsics.h      # Current XMVECTOR/intrinsics defs
```

## Next Step

Continue Phase 5 decomp work. See `docs/vmx128/SESSION_HANDOFF.md` for the latest targets and tooling gaps.

## Quick Commands

```bash
# Phase 1 results
cat docs/vmx128/DC3_VMX128_USAGE.md

# Re-run scanner
python3 tmp/vmx128/vmx128_scanner.py orig/45410914/ham_xbox_r.exe

# 0dinD's Ghidra implementation (needs fixing)
less ~/code/milohax/vmx128-research/ghidra-vmx128/Ghidra/Processors/PowerPC/data/languages/vmx128.sinc

# VMX128 ISA reference (line ~4580)
less ~/code/milohax/vmx128-research/powerpc-rs/isa.yaml
```

## Links

- [Ghidra Issue #2094](https://github.com/NationalSecurityAgency/ghidra/issues/2094) - Upstream VMX128 request
- [0dinD's Fork](https://github.com/0dinD/ghidra/tree/vmx128) - Existing partial implementation
- [powerpc-rs](https://github.com/encounter/powerpc-rs) - ISA definitions
- [PPC-Altivec-IDA](https://github.com/hayleyxyz/PPC-Altivec-IDA) - IDA plugin reference
