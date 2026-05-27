# Ghidra VMX128 Implementation Notes

Technical notes for implementing VMX128 support in Ghidra's Sleigh language.

## Current State (Extended 0dinD's Fork)

Location: `~/code/milohax/vmx128-research/ghidra-vmx128/`

**Status**: All 19 PowerPC SLEIGH variants compile successfully. VMX128 decompilation tested and working on DC3 (as of 2026-01-25).

**Build**: `JAVA_HOME=/usr/lib/jvm/java-25-openjdk gradle --project-dir=/home/free/code/milohax/vmx128-research/ghidra-vmx128 buildGhidra -x test`

### Files Added

```
Ghidra/Processors/PowerPC/data/languages/
├── vmx128.sinc           # VMX128 instruction definitions (heavily modified)
└── ppc_64_xenon.slaspec  # Xenon processor variant
```

### Files Modified

```
Ghidra/Processors/PowerPC/data/languages/
└── ppc.ldefs             # Added Xenon language entry
```

### vmx128.sinc Changes Summary

| Change | Description |
|--------|-------------|
| Register size | `size=4` → `size=16` (128-bit vectors) |
| Immediate fields | Added `d3dtype_18_20`, `vmask_16_17`, `zimm_06_07`, `perm128` |
| Pcodeop definitions | Added 86 pcodeop definitions for all operations |
| Instruction semantics | All 77 instructions have pcode (full or stubs) |

Sources used for the SLEIGH definitions:
- `~/code/milohax/vmx128-research/powerpc-rs/isa.yaml` (bitmask/pattern/operand lists)
- `~/code/milohax/vmx128-research/xenia-reference/docs/ppc/vmx128.txt` (bit layout diagrams)
- `~/code/milohax/vmx128-research/PPC-Altivec-IDA/plugin.cpp` (operand extraction and immediate packing)
- `~/code/milohax/vmx128-research/qemu-reference/target/ppc/int_helper.c` (VMX/AltiVec semantics)

### Language Definition Entry

In `ppc.ldefs`:
```xml
<language processor="PowerPC"
          endian="big"
          size="32"
          variant="Xenon"
          version="1.0"
          slafile="ppc_64_xenon.sla"
          processorspec="ppc_64.pspec"
          id="PowerPC:BE:64:Xenon">
  <description>PowerPC 64-bit big endian for Xbox 360 (Xenon) with VMX128</description>
  <truncate_space space="ram" size="4"/>
  <compiler name="default" spec="ppc_64_32.cspec" id="default"/>
</language>
```

## Issues Fixed (Phase 2 & 3)

### Issue 1: Register Size ✓ FIXED

```sleigh
# Fixed: size=4 → size=16 (128-bit vectors)
define register offset=0x10000 size=16 [
    vr0 vr1 vr2 ... vr127
];
```

### Issue 2: Missing Immediate Extractions ✓ FIXED

All immediate fields now properly extracted:

**vpermwi128** - Added `perm128` combined field:
```sleigh
perm128: val is permH_06_08 & permL_11_15 [ val = (permH_06_08 << 5) | permL_11_15; ]
:vpermwi128 vregD_21_25, vregB_11_15, perm128 is ...
```

**vpkd3d128** - Added D3DType, VMASK, Zimm:
```sleigh
:vpkd3d128 vregD_21_25, vregB_11_15, d3dtype_18_20, vmask_16_17, zimm_06_07 is ...
```

**vrlimi128** - Added Zimm:
```sleigh
:vrlimi128 vregD_21_25, vregB_11_15, uimm_16_20, zimm_06_07 is ...
```

### Issue 3: Empty Pcode Semantics ✓ FIXED

All 77 instructions now have pcode semantics:
- **Full semantics**: Load/store (`lvx128`, `stvx128`), logical ops (`vor128`, `vand128`, `vxor128`, `vnor128`, `vandc128`), `vsldoi128`
- **Pcodeop stubs**: All remaining instructions

## Remaining Issues

See [PHASE4_TODO.md](PHASE4_TODO.md) for details:

1. **D3D pack/unpack** - `vpkd3d128`, `vupkd3d128` still use pcodeop stubs
2. **Pack/unpack saturation** - `vpk*128`/`vupk*128` saturation variants still use pcodeop stubs

### D3D Operand Field Reference

From [binutils VMX128 patch](https://sourceware.org/legacy-ml/binutils/2007-03/msg00366.html):

| Operand | Bits | Width | Description |
|---------|------|-------|-------------|
| VD3D0 | 18-20 | 3 bits | D3D type selector |
| VD3D1 | 16-17 | 2 bits | Vector mask |
| VD3D2 | 6-7 | 2 bits | Zero immediate |

These correspond to `d3dtype_18_20`, `vmask_16_17`, `zimm_06_07` in `vmx128.sinc`.

## Pcode Implementation Strategy

### Tier 1: Full Semantics

For simple operations, implement real pcode:

**Load/Store**:
```sleigh
:lvx128 VD,RA,RB is OP=4 & VD & RA & RB & XOP=0xC3 {
    local ea:4 = RA + RB;
    ea = ea & ~0xF;  # Align to 16 bytes
    VD = *:16 ea;
}

:stvx128 VS,RA,RB is OP=4 & VS & RA & RB & XOP=0x1C3 {
    local ea:4 = RA + RB;
    ea = ea & ~0xF;  # Align to 16 bytes
    *:16 ea = VS;
}
```

**Arithmetic** (using sub-register access):
```sleigh
:vaddfp128 VD,VA,VB is OP=5 & VD & VA & VB & XOP=0x10 {
    VD[0,32] = VA[0,32] f+ VB[0,32];
    VD[32,32] = VA[32,32] f+ VB[32,32];
    VD[64,32] = VA[64,32] f+ VB[64,32];
    VD[96,32] = VA[96,32] f+ VB[96,32];
}

:vmulfp128 VD,VA,VB is OP=5 & VD & VA & VB & XOP=0x90 {
    VD[0,32] = VA[0,32] f* VB[0,32];
    VD[32,32] = VA[32,32] f* VB[32,32];
    VD[64,32] = VA[64,32] f* VB[64,32];
    VD[96,32] = VA[96,32] f* VB[96,32];
}
```

**Logical**:
```sleigh
:vand128 VD,VA,VB is OP=5 & VD & VA & VB & XOP=0x210 {
    VD = VA & VB;
}

:vor128 VD,VA,VB is OP=5 & VD & VA & VB & XOP=0x2D0 {
    VD = VA | VB;
}

:vxor128 VD,VA,VB is OP=5 & VD & VA & VB & XOP=0x310 {
    VD = VA ^ VB;
}
```

### Tier 2: Pcodeop Stubs

For complex operations, use stubs that show in decompiler:

```sleigh
define pcodeop vmaddfp128_impl;
define pcodeop vmsum3fp128_impl;
define pcodeop vperm128_impl;
define pcodeop vpkd3d128_impl;

:vmaddfp128 VD,VA,VB is OP=5 & VD & VA & VB & XOP=0xD0 {
    VD = vmaddfp128_impl(VA, VB, VD);  # VD is also addend
}

:vmsum3fp128 VD,VA,VB is OP=5 & VD & VA & VB & XOP=0x190 {
    VD = vmsum3fp128_impl(VA, VB);  # 3D dot product
}

:vperm128 VD,VA,VB,VC is OP=5 & VD & VA & VB & VC & XOP=0x0 {
    VD = vperm128_impl(VA, VB, VC);
}
```

Decompiler output will look like:
```c
vr47 = vmaddfp128_impl(vr12, vr33, vr47);
vr10 = vmsum3fp128_impl(vr5, vr6);
```

## Register Sub-Access

For operations that work on individual lanes, use Sleigh's sub-register access:

```sleigh
# Access individual 32-bit floats in a 128-bit register
# VD[offset, size] where offset is in bits

VD[0,32]   # First float (bits 0-31)
VD[32,32]  # Second float (bits 32-63)
VD[64,32]  # Third float (bits 64-95)
VD[96,32]  # Fourth float (bits 96-127)
```

Lane order follows PowerPC big-endian element ordering (see `altivec_instructions.pdf` in the Xenia docs bundle).

## Comparison Instructions with Rc

VMX128 uses bit 25 for the record bit (different from standard AltiVec bit 21):

```sleigh
define token vmx128_instr(32)
    Rc128 = (25, 25)
;

:vcmpeqfp128 VD,VA,VB is OP=6 & VD & VA & VB & Rc128=0 & XOP=0x0 {
    # Set each element to all 1s if equal, all 0s if not
    # ... implementation ...
}

:vcmpeqfp128. VD,VA,VB is OP=6 & VD & VA & VB & Rc128=1 & XOP=0x0 {
    # Same as above, but also update CR6
    # ... implementation ...
}
```

## Building and Testing

### Build Ghidra with Modifications

```bash
cd ~/code/milohax/vmx128-research/ghidra-vmx128
gradle buildGhidra
```

### Install Modified Processor

The built processor module will be in the build output. Copy to your Ghidra installation or run from the build directory.

### Test with DC3

1. Launch modified Ghidra
2. Import DC3 XEX (use XEXLoaderWV)
3. Select "PowerPC:BE:64:Xenon" as the language
4. Navigate to a VMX-heavy function
5. Verify disassembly and decompiler output

## Reference Files

### powerpc-rs isa.yaml

The authoritative VMX128 reference:
`~/code/milohax/vmx128-research/powerpc-rs/isa.yaml` (line ~4580)

### PPC-Altivec-IDA plugin.cpp

Good reference for encoding macros and operand extraction:
`~/code/milohax/vmx128-research/PPC-Altivec-IDA/plugin.cpp`

### Standard AltiVec

Reference for how Ghidra handles standard vector ops:
`/opt/ghidra/Ghidra/Processors/PowerPC/data/languages/altivec.sinc`

## Gotchas

1. **Endianness**: Xbox 360 is big-endian; bit numbering in docs may vary

2. **Register overlap**: VMX128 vr0-vr31 may or may not overlap with standard AltiVec vr0-vr31 depending on how we define them

3. **Sleigh limitations**: Some complex operations (like permute) are hard to express in pcode; stubs are acceptable

4. **XEXLoaderWV**: Currently specifies `PowerPC:BE:64:A2ALT-32addr`; may need updating to use new Xenon variant

## Backups

Pre-D3D-stub backup (2026-01-25): `/tmp/claude/vmx128-backups/vmx128.sinc.backup_20260125_105836`

This backup contains the full D3D pack/unpack implementations (~650 lines) before they were replaced with pcodeop stubs due to SLEIGH syntax complexity.

## Testing Results (2026-01-25)

VMX128 decompilation tested successfully on DC3 `ham_xbox_r.exe`:

**Verified Working**:
- `lvx128`/`stvx128` - Load/store vector indexed
- `vspltw128` - Splat word
- `vmaddcfp128`/`vmaddfp128` - FP multiply-add (decompiles to proper `a*b + c*d + ...` expressions)
- `vmulfp128` - FP multiply

**Example Decompiled Output** (matrix-vector multiply):
```c
*(float *)(&stack0x000000b0 + iVar8) =
     in_register_000103e0 * in_register_000103b0 +
     in_register_000103e4 * in_register_000103c0 +
     in_register_000103d0 * in_register_000103e8 + in_register_000103f0;
```

This shows the lane-wise FP semantics are properly enabling data flow analysis through VMX128 operations.

**D3D Instructions**: Use `vectorPackD3D128`/`vectorUnpackD3D128` pcodeop stubs (complex bit manipulation not worth implementing in SLEIGH)
