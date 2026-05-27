# VMX128 Register Encoding

## The Problem

Standard AltiVec has 32 vector registers (vr0-vr31), which fit in a 5-bit field.
VMX128 has 128 vector registers (vr0-vr127), requiring 7 bits.

Primary references:
- `~/code/milohax/vmx128-research/xenia-reference/docs/ppc/vmx128.txt` (encoding conventions)
- `~/code/milohax/vmx128-research/PPC-Altivec-IDA/plugin.cpp` (operand extraction logic)

PowerPC instructions have fixed 32-bit encoding with no room for larger register fields.
VMX128 solves this by splitting the 7-bit register number across non-contiguous bit positions.

## Instruction Bit Layout

```
 31  30  29  28  27  26  25  24  23  22  21  20  19  18  17  16  15  14  13  12  11  10   9   8   7   6   5   4   3   2   1   0
+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
|      Primary Op      |      VD (5 bits)     |     VA (5 bits)      |     VB (5 bits)      |     XOP / Other      | VD/VB ext |
+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
```

The "VD/VB ext" and other bits provide the extra 2 bits needed for 7-bit addressing.

## Register Field Encoding

### VDS128 (Source/Destination)

**Bit positions**: 2..3 (high bits), 21..25 (low bits)

```
Register[6:0] = { bits[2:3], bits[21:25] }
              = { bits[2:3] << 5 } | bits[21:25]
```

In IDA plugin terms:
```c
reg = ((codeBytes >> 21) & 0x1F) | ((codeBytes & 0x0C) << 3);
```

### VA128 (Register A)

**Bit positions**: bit 10, bit 5, bits 16..20

```
Register[6:0] = { bit[10] << 6 } | { bit[5] << 5 } | bits[16:20]
```

In IDA plugin terms:
```c
reg = ((codeBytes >> 16) & 0x1F) | (codeBytes & 0x20) | ((codeBytes >> 4) & 0x40);
```

### VB128 (Register B)

**Bit positions**: bits 0..1 (high bits), bits 11..15 (low bits)

```
Register[6:0] = { bits[0:1] << 5 } | bits[11:15]
```

In IDA plugin terms:
```c
reg = ((codeBytes << 5) & 0x60) | ((codeBytes >> 11) & 0x1F);
```

### VC128 (Register C)

**Bit positions**: bits 6..8 (3 bits)

```
Register[2:0] = bits[6:8]
```

**Note**: VC128 can only address registers vr0-vr7 (3 bits = 8 values). This matches the `vregC_06_08` field in `~/code/milohax/vmx128-research/ghidra-vmx128/Ghidra/Processors/PowerPC/data/languages/vmx128.sinc`.

## Ghidra Sleigh Implementation

### Token Definition

```sleigh
define token vmx128_instr(32)
    # Standard register fields (low 5 bits)
    vrD_lo = (21, 25)
    vrA_lo = (16, 20)
    vrB_lo = (11, 15)
    vrC    = (6, 8)

    # Extended register bits
    vrD_hi = (2, 3)    # 2 bits
    vrA_b5 = (5, 5)    # 1 bit
    vrA_b6 = (10, 10)  # 1 bit
    vrB_hi = (0, 1)    # 2 bits
;
```

### Register Attachment

```sleigh
# Compute full 7-bit register number
VD128: vrD is vrD_lo & vrD_hi [ vrD = (vrD_hi << 5) | vrD_lo; ]
{
    local reg:16 = vr[vrD * 16, 16];
    export reg;
}

VA128: vrA is vrA_lo & vrA_b5 & vrA_b6
    [ vrA = (vrA_b6 << 6) | (vrA_b5 << 5) | vrA_lo; ]
{
    local reg:16 = vr[vrA * 16, 16];
    export reg;
}

VB128: vrB is vrB_lo & vrB_hi [ vrB = (vrB_hi << 5) | vrB_lo; ]
{
    local reg:16 = vr[vrB * 16, 16];
    export reg;
}

VC128: vrC is vrC
{
    local reg:16 = vr[vrC * 16, 16];
    export reg;
}
```

## Encoding Format Masks

Different instruction formats use different bits for the extended opcode vs register extension:

| Format | Mask | Free Bits for Register Extension |
|--------|------|----------------------------------|
| VX128 | 0x3d0 | bits 0-3, 5 |
| VX128_1 | 0x7f3 | bits 2-3 only |
| VX128_2 | 0x210 | bits 0-4, 6-8 |
| VX128_3 | 0x7f0 | bits 0-3 |
| VX128_4 | 0x730 | bits 0-3, 6-7 |
| VX128_5 | 0x10 | bits 0-3, 5-9 |
| VX128_P | 0x630 | bits 0-3, 7-8 |

## Example: Decoding `vaddfp128`

Instruction encoding: `000101 DDDDD AAAAA BBBBB 0 0000 0 1 aa cc`

Where:
- Primary opcode: 000101 (5)
- DDDDD: VD low 5 bits (bits 21-25)
- AAAAA: VA low 5 bits (bits 16-20)
- BBBBB: VB low 5 bits (bits 11-15)
- aa: VD high 2 bits (bits 2-3)
- cc: VB high 2 bits (bits 0-1)

Bitmask: 0xfc0003d0
Pattern: 0x14000010

```
Pattern:  0001 0100 0000 0000 0000 0000 0001 0000
          ^^^^ ^^                        ^    ^
          opcode=5                       |    |
                                    XOP bit   XOP bit
```

## Practical Notes

1. **Not all registers accessible everywhere**: VC128 only has 3 bits, limiting it to vr0-vr7

2. **Standard AltiVec compatibility**: vr0-vr31 use the same encoding as standard AltiVec when the extension bits are 0

3. **Register file**: The Xenon CPU has dedicated 128x128-bit register files per hardware thread

4. **Sleigh complexity**: The bit-splitting makes Sleigh patterns verbose but manageable
