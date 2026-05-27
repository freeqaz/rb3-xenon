# VMX128 Instruction Set Reference

Source: `~/code/milohax/vmx128-research/powerpc-rs/isa.yaml` (by encounter)

Additional references used for semantics and encoding details:
- `~/code/milohax/vmx128-research/xenia-reference/docs/ppc/vmx128.txt` (bit layout diagrams, VMX128-specific semantics)
- `~/code/milohax/vmx128-research/qemu-reference/target/ppc/int_helper.c` (AltiVec/VMX runtime semantics)
- `~/code/milohax/vmx128-research/PPC-Altivec-IDA/plugin.cpp` (operand extraction and immediate packing)

## Overview

VMX128 extends AltiVec with 128 vector registers and ~80 additional instructions optimized for Xbox 360 graphics and physics.

---

## Load Instructions

| Mnemonic | Description | Bitmask | Pattern |
|----------|-------------|---------|---------|
| lvewx128 | Load Vector128 Element Word Indexed | 0xfc0007f3 | 0x10000083 |
| lvlx128 | Load Vector128 Left Indexed | 0xfc0007f3 | 0x10000403 |
| lvlxl128 | Load Vector128 Left Indexed LRU | 0xfc0007f3 | 0x10000603 |
| lvrx128 | Load Vector128 Right Indexed | 0xfc0007f3 | 0x10000443 |
| lvrxl128 | Load Vector128 Right Indexed LRU | 0xfc0007f3 | 0x10000643 |
| lvsl128 | Load Vector128 for Shift Left | 0xfc0007f3 | 0x10000003 |
| lvsr128 | Load Vector128 for Shift Right | 0xfc0007f3 | 0x10000043 |
| lvx128 | Load Vector128 Indexed | 0xfc0007f3 | 0x100000C3 |
| lvxl128 | Load Vector128 Indexed LRU | 0xfc0007f3 | 0x100002C3 |

All load instructions use format VX128_1 with operands: `VDS128, rA, rB`

---

## Store Instructions

| Mnemonic | Description | Bitmask | Pattern |
|----------|-------------|---------|---------|
| stvewx128 | Store Vector128 Element Word Indexed | 0xfc0007f3 | 0x10000183 |
| stvlx128 | Store Vector128 Left Indexed | 0xfc0007f3 | 0x10000503 |
| stvlxl128 | Store Vector128 Left Indexed LRU | 0xfc0007f3 | 0x10000703 |
| stvrx128 | Store Vector128 Right Indexed | 0xfc0007f3 | 0x10000543 |
| stvrxl128 | Store Vector128 Right Indexed LRU | 0xfc0007f3 | 0x10000743 |
| stvx128 | Store Vector128 Indexed | 0xfc0007f3 | 0x100001C3 |
| stvxl128 | Store Vector128 Indexed LRU | 0xfc0007f3 | 0x100003C3 |

All store instructions use format VX128_1 with operands: `VDS128, rA, rB`

---

## Floating-Point Arithmetic

| Mnemonic | Description | Bitmask | Pattern |
|----------|-------------|---------|---------|
| vaddfp128 | Vector128 Add Floating Point | 0xfc0003d0 | 0x14000010 |
| vsubfp128 | Vector128 Subtract Floating Point | 0xfc0003d0 | 0x14000050 |
| vmulfp128 | Vector128 Multiply Floating-Point | 0xfc0003d0 | 0x14000090 |
| vmaddfp128 | Vector128 Multiply Add Floating Point | 0xfc0003d0 | 0x140000d0 |
| vmaddcfp128 | Vector128 Multiply Add Carryout FP | 0xfc0003d0 | 0x14000110 |
| vnmsubfp128 | Vector128 Negative Multiply-Subtract FP | 0xfc0003d0 | 0x14000150 |
| vmsum3fp128 | Vector128 Multiply Sum 3-way FP (dot3) | 0xfc0003d0 | 0x14000190 |
| vmsum4fp128 | Vector128 Multiply Sum 4-way FP (dot4) | 0xfc0003d0 | 0x140001d0 |
| vmaxfp128 | Vector128 Maximum Floating Point | 0xfc0003d0 | 0x18000280 |
| vminfp128 | Vector128 Minimum Floating Point | 0xfc0003d0 | 0x180002c0 |

Operands: `VDS128, VA128, VB128` (vmaddfp128/vnmsubfp128 use VD as implicit 4th operand)

**Dot product semantics** (`vmsum3fp128`, `vmsum4fp128`) come from `vmx128.txt` and are not covered in standard AltiVec docs.

---

## Floating-Point Estimates

| Mnemonic | Description | Bitmask | Pattern |
|----------|-------------|---------|---------|
| vrefp128 | Vector128 Reciprocal Estimate FP | 0xfc1f07f0 | 0x18000630 |
| vrsqrtefp128 | Vector128 Reciprocal Square Root Estimate FP | 0xfc1f07f0 | 0x18000670 |
| vexptefp128 | Vector128 2^x Estimate FP | 0xfc1f07f0 | 0x180006b0 |
| vlogefp128 | Vector128 Log2 Estimate FP | 0xfc1f07f0 | 0x180006f0 |

Operands: `VDS128, VB128`

**Estimate semantics** (`vrefp128`, `vrsqrtefp128`) follow QEMU's per-lane exact math implementation; real hardware is approximate.

---

## Floating-Point Rounding

| Mnemonic | Description | Bitmask | Pattern |
|----------|-------------|---------|---------|
| vrfim128 | Round to FP Integer toward -Infinity | 0xfc1f07f0 | 0x18000330 |
| vrfin128 | Round to FP Integer toward Nearest | 0xfc1f07f0 | 0x18000370 |
| vrfip128 | Round to FP Integer toward +Infinity | 0xfc1f07f0 | 0x180003b0 |
| vrfiz128 | Round to FP Integer toward Zero | 0xfc1f07f0 | 0x180003f0 |

Operands: `VDS128, VB128`

---

## Floating-Point Conversion

| Mnemonic | Aliases | Description | Bitmask | Pattern |
|----------|---------|-------------|---------|---------|
| vctsxs128 | vcfpsxws128 | Convert to Signed Fixed-Point Word Saturate | 0xfc0007f0 | 0x18000230 |
| vctuxs128 | vcfpuxws128 | Convert to Unsigned Fixed-Point Word Saturate | 0xfc0007f0 | 0x18000270 |
| vcfsx128 | vcsxwfp128 | Convert From Signed Fixed-Point Word | 0xfc0007f0 | 0x180002b0 |
| vcfux128 | vcuxwfp128 | Convert From Unsigned Fixed-Point Word | 0xfc0007f0 | 0x180002f0 |

Operands: `VDS128, VB128, simm/uimm`

---

## Comparison Instructions

All comparison instructions support the Rc128 modifier (`.` suffix) at bit 25.

| Mnemonic | Description | Bitmask | Pattern |
|----------|-------------|---------|---------|
| vcmpeqfp128[.] | Compare Equal-to FP | 0xfc000390 | 0x18000000 |
| vcmpgefp128[.] | Compare Greater-Than-or-Equal FP | 0xfc000390 | 0x18000080 |
| vcmpgtfp128[.] | Compare Greater-Than FP | 0xfc000390 | 0x18000100 |
| vcmpbfp128[.] | Compare Bounds FP (bounds flags, not a boolean mask) | 0xfc000390 | 0x18000180 |
| vcmpequw128[.] | Compare Equal-to Unsigned Word | 0xfc000390 | 0x18000200 |

Operands: `VDS128, VA128, VB128`

**Note on vcmpbfp128**: Unlike other comparisons which produce full boolean masks (`0xFFFFFFFF`/`0x00000000`), vcmpbfp128 produces a **2-bit code per lane**:
- Bit 31: Set if `a > b` (upper bound exceeded)
- Bit 30: Set if `a < -b` (lower bound exceeded)
- Result: `0x00000000` (in bounds), `0x80000000` (high), `0x40000000` (low), `0xC0000000` (NaN)

See [VCMPBFP128_SEMANTICS.md](VCMPBFP128_SEMANTICS.md) for full details.

Comparison lane mask behavior follows standard AltiVec semantics (see `altivec_instructions.pdf` in the Xenia docs bundle).

---

## Logical Operations

| Mnemonic | Description | Bitmask | Pattern |
|----------|-------------|---------|---------|
| vand128 | Logical AND | 0xfc0003d0 | 0x14000210 |
| vandc128 | Logical AND with Complement | 0xfc0003d0 | 0x14000250 |
| vnor128 | Logical NOR | 0xfc0003d0 | 0x14000290 |
| vor128 | Logical OR | 0xfc0003d0 | 0x140002d0 |
| vxor128 | Logical XOR | 0xfc0003d0 | 0x14000310 |
| vsel128 | Select (bitwise mux) | 0xfc0003d0 | 0x14000350 |

Operands: `VDS128, VA128, VB128`

---

## Shift and Rotate

| Mnemonic | Description | Bitmask | Pattern |
|----------|-------------|---------|---------|
| vrlw128 | Rotate Left Word | 0xfc0003d0 | 0x18000050 |
| vslw128 | Shift Left Word | 0xfc0003d0 | 0x180000d0 |
| vsraw128 | Shift Right Arithmetic Word | 0xfc0003d0 | 0x18000150 |
| vsrw128 | Shift Right Word | 0xfc0003d0 | 0x180001d0 |
| vslo128 | Shift Left Octet | 0xfc0003d0 | 0x14000390 |
| vsro128 | Shift Right Octet | 0xfc0003d0 | 0x140003d0 |
| vsldoi128 | Shift Left Double by Octet Immediate | 0xfc000010 | 0x10000010 |
| vrlimi128 | Rotate Left Immediate and Mask Insert | 0xfc000730 | 0x18000710 |

---

## Merge Operations

| Mnemonic | Description | Bitmask | Pattern |
|----------|-------------|---------|---------|
| vmrghw128 | Merge High Word | 0xfc0003d0 | 0x18000300 |
| vmrglw128 | Merge Low Word | 0xfc0003d0 | 0x18000340 |

Operands: `VDS128, VA128, VB128`

---

## Permutation

| Mnemonic | Description | Bitmask | Pattern | Operands |
|----------|-------------|---------|---------|----------|
| vperm128 | Permutation | 0xfc000210 | 0x14000000 | VDS128, VA128, VB128, VC128 |
| vpermwi128 | Permute Word Immediate | 0xfc000630 | 0x18000210 | VDS128, VB128, PERM |

**Immediate packing** for `vpermwi128` matches `vmx128.txt` (PERMh/PERMl) and the IDA plugin's `VPERM128` extraction.

---

## Splat Operations

| Mnemonic | Description | Bitmask | Pattern |
|----------|-------------|---------|---------|
| vspltw128 | Splat Word | 0xfc0007f0 | 0x18000730 |
| vspltisw128 | Splat Immediate Signed Word | 0xfc0007f0 | 0x18000770 |

Operands: `VDS128, VB128, uimm/simm`

---

## Pack Operations

| Mnemonic | Description | Bitmask | Pattern |
|----------|-------------|---------|---------|
| vpkshss128 | Pack Signed Half Word Signed Saturate | 0xfc0003d0 | 0x14000200 |
| vpkshus128 | Pack Signed Half Word Unsigned Saturate | 0xfc0003d0 | 0x14000240 |
| vpkswss128 | Pack Signed Word Signed Saturate | 0xfc0003d0 | 0x14000280 |
| vpkswus128 | Pack Signed Word Unsigned Saturate | 0xfc0003d0 | 0x140002c0 |
| vpkuhum128 | Pack Unsigned Half Word Unsigned Modulo | 0xfc0003d0 | 0x14000300 |
| vpkuhus128 | Pack Unsigned Half Word Unsigned Saturate | 0xfc0003d0 | 0x14000340 |
| vpkuwum128 | Pack Unsigned Word Unsigned Modulo | 0xfc0003d0 | 0x14000380 |
| vpkuwus128 | Pack Unsigned Word Unsigned Saturate | 0xfc0003d0 | 0x140003c0 |
| vpkd3d128 | Pack D3Dtype (Xbox 360 graphics) | 0xfc000730 | 0x18000610 |

### D3D Pack/Unpack Instructions (VX128_4 Form)

The `vpkd3d128` and `vupkd3d128` instructions use specialized D3D operand fields for Xbox 360 graphics format conversion.

**Source**: [binutils VMX128 patch](https://sourceware.org/legacy-ml/binutils/2007-03/msg00366.html)

**D3D Operand Fields**:

| Operand | Bits | Width | Description |
|---------|------|-------|-------------|
| VD3D0 | 18-20 | 3 bits | D3D type selector |
| VD3D1 | 16-17 | 2 bits | Vector mask |
| VD3D2 | 6-7 | 2 bits | Zero immediate |

**vpkd3d128 Syntax**: `vpkd3d128 VD, VB, VD3D0, VD3D1, VD3D2`

**vupkd3d128 Syntax**: `vupkd3d128 VD, VB, VD3D0`

These instructions pack and unpack vector data to/from Direct3D vertex and pixel formats used by the Xbox 360 GPU.

---

## Unpack Operations

| Mnemonic | Description | Bitmask | Pattern |
|----------|-------------|---------|---------|
| vupkhsb128 | Unpack High Signed Byte | 0xfc1f07f0 | 0x18000380 |
| vupkhsh128 | Unpack High Signed Half Word | 0xfc1f07f0 | 0x180007a0 |
| vupklsb128 | Unpack Low Signed Byte | 0xfc1f07f0 | 0x180003c0 |
| vupklsh128 | Unpack Low Signed Half Word | 0xfc1f07f0 | 0x180007e0 |
| vupkd3d128 | Unpack D3Dtype | 0xfc0007f0 | 0x180007f0 |

---

## Primary Opcode Summary

| Opcode | Hex | Instruction Types |
|--------|-----|-------------------|
| 4 | 0x10 | Load/Store (VX128_1), vsldoi128 (VX128_5) |
| 5 | 0x14 | Arithmetic, Logical, Pack (VX128, VX128_2) |
| 6 | 0x18 | Compare, Convert, Round, Shift, Unpack (VX128, VX128_3, VX128_4, VX128_P) |

---

## Notes

1. **LRU Variants**: Instructions ending in `l` provide cache hints (Least Recently Used)
2. **D3D Instructions**: `vpkd3d128` and `vupkd3d128` are Xbox 360 graphics-specific
3. **Dot Products**: `vmsum3fp128` and `vmsum4fp128` compute 3D and 4D dot products
4. **Implicit Operands**: `vmaddfp128` uses VD as both destination and addend
