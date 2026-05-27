# VMX128 Reference Sources

Authoritative sources for PowerPC AltiVec/VMX/VMX128 instruction semantics.

---

## Local Reference Repositories

Cloned into `~/code/milohax/vmx128-research/` for offline reference:

### GCC (rs6000 AltiVec)

**Path**: `~/code/milohax/vmx128-research/gcc-reference/`

Key files:

| File | Purpose |
|------|---------|
| `gcc/config/rs6000/altivec.h` | Intrinsic definitions, CR6 constants |
| `gcc/config/rs6000/altivec.md` | Machine description patterns (158KB) |

**CR6 constants** (from `altivec.h` lines 53-56):
```c
#define __CR6_EQ      0   // All elements true
#define __CR6_EQ_REV  1   // Not all elements true
#define __CR6_LT      2   // All elements false
#define __CR6_LT_REV  3   // Not all elements false
```

### Xenia (Xbox 360 Emulator)

**Path**: `~/code/milohax/vmx128-research/xenia-reference/`

Key files:

| File | Purpose |
|------|---------|
| `docs/ppc/vmx128.txt` | Complete VMX128 encoding reference (22KB) |
| `docs/ppc/altivec_instructions.pdf` | AltiVec reference (667KB) |
| `docs/ppc/core_instructions.pdf` | Core PPC reference (1.7MB) |

**vmx128.txt** is THE authoritative source for VMX128 instruction encodings. Key locations:
- vmsum3fp128: lines 315-320
- vmsum4fp128: lines 323-328
- vrefp128: lines 448-452
- vrsqrtefp128: lines 503-507
- vperm128: lines 362-365
- vpermwi128: lines 369-372

**Notes**:
- `vmx128.txt` is reverse-engineered and includes occasional typos (e.g., `vmsub3fp128` label in the encoding section). Prefer `powerpc-rs/isa.yaml` for canonical mnemonic names and `vmx128.txt` for bit layout diagrams.

### powerpc-rs (ISA Definitions)

**Path**: `~/code/milohax/vmx128-research/powerpc-rs/`

Key files:

| File | Purpose |
|------|---------|
| `isa.yaml` | Machine-readable instruction definitions (AltiVec + VMX128) |

Use `isa.yaml` for bitmask/pattern/operand lists and mnemonic names. This is the canonical local source for opcode listings and signatures.

### PPC-Altivec-IDA (Encoding Reference)

**Path**: `~/code/milohax/vmx128-research/PPC-Altivec-IDA/`

Key files:

| File | Purpose |
|------|---------|
| `plugin.cpp` | VMX128 register extraction logic and format masks |

Key locations:
- VMX128 format masks: `VX128*` macros (around lines 324-337)
- VMX128 register extraction: `VD128`, `VA128`, `VB128`, `VC128` cases (around lines 1280-1335)
- `VPERM128` immediate packing (around lines 1322-1328)

### QEMU (PowerPC Emulator)

**Path**: `~/code/milohax/vmx128-research/qemu-reference/`

Key files for AltiVec/VMX instruction implementations:

| File | Purpose |
|------|---------|
| `target/ppc/int_helper.c` | Integer and vector helper functions including `helper_vcmpbfp` |
| `target/ppc/fpu_helper.c` | Floating-point helper functions |
| `target/ppc/translate/vmx-impl.c.inc` | VMX instruction translation |
| `target/ppc/translate/vmx-ops.c.inc` | VMX opcode definitions |
| `target/ppc/helper.h` | Helper function declarations |

**vcmpbfp location**: `target/ppc/int_helper.c` lines 681-717:
- `vcmpbfp_internal()` (line 681) - core implementation
- `helper_vcmpbfp()` (line 709) - wrapper without CR update
- `helper_vcmpbfp_dot()` (line 714) - wrapper with CR update (vcmpbfp.)

### RPCS3 (PS3 Emulator)

**Path**: `~/code/milohax/vmx128-research/rpcs3-reference/`

Key files for PPU/AltiVec instruction implementations:

| File | Purpose |
|------|---------|
| `rpcs3/Emu/Cell/PPUInterpreter.cpp` | PPU instruction interpreter (197KB) |
| `rpcs3/Emu/Cell/PPUTranslator.cpp` | PPU-to-LLVM translator |
| `rpcs3/Emu/Cell/PPUOpcodes.h` | Opcode definitions |
| `rpcs3/Emu/Cell/PPUDisAsm.cpp` | Disassembler |
| `rpcs3/Emu/Cell/PPUAnalyser.cpp` | Static analysis |

**vcmpbfp location**: `rpcs3/Emu/Cell/PPUInterpreter.cpp`:
- Line 1045: `auto VCMPBFP()` function template
- Lines 6848-6849: Instruction table entries
- Line 7464: INIT_VCMP macro instantiation

### DC3 XDK Vector Intrinsics (Local)

**Path**: `src/xdk/LIBCMT/vectorintrinsics.h`

Key details:
- `XMVECTOR` is a C-facing struct that maps 1:1 to a VMX register.
- The file documents the local intrinsic surface used in this repo; it is likely a subset of Microsoft's full intrinsics set.
- Intrinsic declarations reference `isa.yaml` for opcode signatures (see inline comments).

### Ghidra VMX128 Fork (SLEIGH Implementation)

**Path**: `~/code/milohax/vmx128-research/ghidra-vmx128/`

Key files:

| File | Purpose |
|------|---------|
| `Ghidra/Processors/PowerPC/data/languages/vmx128.sinc` | VMX128 instruction decode and semantics |
| `Ghidra/Processors/PowerPC/data/languages/ppc.ldefs` | Xenon language entry |

---

## Official Documentation

### AltiVec/VMX

| Document | URL | Content |
|----------|-----|---------|
| AltiVec PEM (ALTIVECPEM) | [nxp.com](https://www.nxp.com/docs/en/reference-manual/ALTIVECPEM.pdf) | Full AltiVec instruction set reference |
| AltiVec PIM (ALTIVECPIM) | [nxp.com](https://www.nxp.com/docs/en/reference-manual/ALTIVECPIM.pdf) | Programming interface manual |
| Power ISA v3.1 | [openpowerfoundation.org](https://openpowerfoundation.org/specifications/isa/) | Official Power ISA specification |

### VMX128 (Xbox 360 Extensions)

| Document | URL | Content |
|----------|-----|---------|
| Xenia VMX128 docs | [github.com](https://github.com/xenia-project/xenia/blob/master/docs/ppc/vmx128.txt) | VMX128 opcode encoding |
| biallas VMX128 | [biallas.net](http://biallas.net/doc/vmx128/vmx128.txt) | Original VMX128 reverse-engineering |
| powerpc-rs isa.yaml | [github.com](https://github.com/encounter/powerpc-rs/blob/main/isa.yaml) / Local: `vmx128-research/powerpc-rs/` | Machine-readable instruction definitions (AltiVec + VMX128), by encounter |
| binutils VMX128 patch | [sourceware.org](https://sourceware.org/legacy-ml/binutils/2007-03/msg00366.html) | Original binutils patch adding VMX128 support; includes D3D operand field definitions |

#### binutils VMX128 Patch Details

The March 2007 binutils patch provides authoritative operand encoding details:

**Split-Field Register Operands** (7-bit, supports 128 registers):
- **VDS128/VS128**: bits [1:0] combined with [21:26]
- **VA128**: bits [6], [11], and [21:26]
- **VB128**: bits [1:0] and [16:26]
- **VPERM**: 8-bit, bits [10:8] and [21:26]

**D3D-Specific Operands** (VX128_4 form):
- **VD3D0**: 3 bits (18-20) - D3D type selector
- **VD3D1**: 2 bits (16-17) - Vector mask
- **VD3D2**: 2 bits (6-7) - Zero immediate

**Instruction Forms Defined**:
- VX128: Basic 3-operand format
- VX128_1: Load/store format
- VX128_4: D3D pack/unpack format (vpkd3d128, vrlimi128)

---

## GCC Compiler References

| File | Purpose |
|------|---------|
| `gcc/config/rs6000/altivec.h` | Intrinsic definitions, CR6 codes |
| `gcc/config/rs6000/altivec.md` | Machine description patterns |
| `gcc/config/rs6000/rs6000-builtins.def` | Built-in function definitions |

**Local copy**: `~/code/milohax/vmx128-research/gcc-reference/gcc/config/rs6000/`

---

## Key Instruction Semantics

### vcmpbfp / vcmpbfp128 (Compare Bounds FP)

**Authoritative source**: QEMU `target/ppc/int_helper.c`

```c
// vcmpbfp_internal
int le = le_rel != float_relation_greater;
int ge = ge_rel != float_relation_less;
r->u32[i] = ((!le) << 31) | ((!ge) << 30);
```

Result per lane:
- `0x00000000` = in bounds (-b <= a <= b)
- `0x80000000` = a > b (upper bound exceeded)
- `0x40000000` = a < -b (lower bound exceeded)
- `0xC0000000` = NaN (unordered)

See [VCMPBFP128_SEMANTICS.md](VCMPBFP128_SEMANTICS.md) for complete documentation.

### vmsum3fp128 / vmsum4fp128 (Dot Products)

**VMX128-specific** - not in standard AltiVec, QEMU, or RPCS3.

From Xenia vmx128.txt (lines 315-328):
- `vmsum3fp128`: 3D dot product with result splatted to all lanes
- `vmsum4fp128`: 4D dot product with result splatted to all lanes

```
vmsum3fp128 vD, vA, vB:  vD = splat(vA.x*vB.x + vA.y*vB.y + vA.z*vB.z)
vmsum4fp128 vD, vA, vB:  vD = splat(vA.x*vB.x + vA.y*vB.y + vA.z*vB.z + vA.w*vB.w)
```

### vrefp128 / vrsqrtefp128 (Reciprocal Estimates)

**Authoritative source**: QEMU `target/ppc/int_helper.c` lines 1526-1562

```c
// vrefp: per-lane reciprocal
r->f32[i] = 1.0f / b->f32[i];

// vrsqrtefp: per-lane reciprocal sqrt
r->f32[i] = 1.0f / sqrt(b->f32[i]);
```

Note: Hardware produces estimates (~12-bit precision); emulators use exact math.

### vperm128 / vpermwi128 (Permutation)

**vperm128** - see QEMU `target/ppc/int_helper.c` lines 1237-1253:
- Each byte of vC selects a byte from the 32-byte concatenation vA||vB
- Selector bit 4: 0=select from vA, 1=select from vB
- Selector bits 0-3: byte index within selected vector

**vpermwi128** - Xbox 360 specific:
- 8-bit immediate encodes 4 x 2-bit word selectors
- Each 2-bit value selects which word from vB goes to that output position

---

## Usage Notes

When verifying instruction semantics:

1. **Check QEMU first** - Most authoritative open-source implementation
2. **Cross-reference RPCS3** - Good for NaN handling edge cases
3. **Verify with official docs** - AltiVec PEM for standard instructions
4. **Test empirically** - Run on real hardware or Xenia for VMX128-specific behavior

---

## Related Documents

- [ISA_REFERENCE.md](ISA_REFERENCE.md) - Full VMX128 instruction set
- [VCMPBFP128_SEMANTICS.md](VCMPBFP128_SEMANTICS.md) - vcmpbfp128 detailed semantics
- [PHASE4_TODO.md](PHASE4_TODO.md) - Implementation priorities
