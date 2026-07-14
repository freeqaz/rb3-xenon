# Diff: inflate

- **Symbol**: `inflate`
- **Match**: 99.5% normalized (99.2% raw)
  - High-match band. Run the source permuter as the first action (regswaps, FPR scheduling, and bool materialization cascade here). Hand-edit fallbacks: variable reorder, inline assignment, member-cache hoists.
- **Target Size**: 5296 bytes
- **Base Size**: 5300 bytes
- **Diff Score**: 700 / 132400

## Instruction Summary

| Type | Count | Percent |
|------|------:|--------:|
| equal | 1286 | 96.9% |
| diff_arg | 36 | 2.7% |
| delete | 2 | 0.2% |
| insert | 3 | 0.2% |
| **Total** | 1327 | 100.0% |

## Region Summary

| Region | Instructions | Match % | Notes |
|--------|------------:|--------:|-------|
| 0-112 | 113 | 100% |  |
| 113-113 | 1 | 0% |  |
| 114-184 | 71 | 100% |  |
| 185-185 | 1 | 0% |  |
| 186-217 | 32 | 100% |  |
| 218-218 | 1 | 0% |  |
| 219-247 | 29 | 100% |  |
| 248-248 | 1 | 0% |  |
| 249-276 | 28 | 100% |  |
| 277-277 | 1 | 0% |  |
| 278-324 | 47 | 100% |  |
| 325-332 | 8 | 75% |  |
| 333-349 | 17 | 100% |  |
| 350-357 | 8 | 75% |  |
| 358-374 | 17 | 100% |  |
| 375-383 | 9 | 78% |  |
| 384-407 | 24 | 100% |  |
| 408-408 | 1 | 0% |  |
| 409-443 | 35 | 100% |  |
| 444-444 | 1 | 0% |  |
| 445-453 | 9 | 100% |  |
| 454-454 | 1 | 0% |  |
| 455-500 | 46 | 100% |  |
| 501-501 | 1 | 0% |  |
| 502-531 | 30 | 100% |  |
| 532-532 | 1 | 0% |  |
| 533-547 | 15 | 100% |  |
| 548-548 | 1 | 0% |  |
| 549-582 | 34 | 100% |  |
| 583-583 | 1 | 0% |  |
| 584-654 | 71 | 100% |  |
| 655-655 | 1 | 0% |  |
| 656-680 | 25 | 100% |  |
| 681-681 | 1 | 0% |  |
| 682-700 | 19 | 100% |  |
| 701-701 | 1 | 0% |  |
| 702-727 | 26 | 100% |  |
| 728-728 | 1 | 0% |  |
| 729-746 | 18 | 100% |  |
| 747-747 | 1 | 0% |  |
| 748-860 | 113 | 100% |  |
| 861-861 | 1 | 0% |  |
| 862-898 | 37 | 100% |  |
| 899-899 | 1 | 0% |  |
| 900-948 | 49 | 100% |  |
| 949-949 | 1 | 0% |  |
| 950-977 | 28 | 100% |  |
| 978-978 | 1 | 0% |  |
| 979-1014 | 36 | 100% |  |
| 1015-1015 | 1 | 0% |  |
| 1016-1059 | 44 | 100% |  |
| 1060-1060 | 1 | 0% |  |
| 1061-1088 | 28 | 100% |  |
| 1089-1089 | 1 | 0% |  |
| 1090-1130 | 41 | 100% |  |
| 1131-1135 | 5 | 60% | 1 bool masks, 1 inserts |
| 1136-1237 | 102 | 100% |  |
| 1238-1248 | 11 | 18% | 2 inserts, 2 deletes |
| 1249-1326 | 78 | 100% |  |

## Patterns Detected

- **BOOL_MASK** (PermuterClass): 1 instruction(s), bit positions: [24] [docs](docs/decomp/patterns/fixable-bool-mask.md)

**Unattributed mismatches**: 40 | **Patterns checked**: 21

## Function Call Diff

**Target only:** `?adler32@D3DX@@YAKKPBEI@Z` (4), `__savegprlr` (1), `crc32_big` (12)
**Base only:** `__savegprlr_14` (1), `adler32` (4), `crc32` (12)

## Verdict: MaybeFixable (Medium confidence)

1 bool mask instruction(s) detected. Bool-return masking differences are typically permuter-class — the source permuter can usually flip the emission via small body restructurings.

### Verdict Factors

| Factor | Value | Threshold | Result |
|--------|-------|-----------|--------|
| bool_mask_detected | true | - | detected |

**Recommendation**: Try a source-permuter sweep on this function. If the gap is small (1-3%) and a full sweep yields nothing, then accept (99.5%) and mark at_limit. See fixable-bool-mask.md for the bool↔byte mask shapes that can be hand-edited.

### Suggestions

1. Run the source permuter on this function before accepting. ([docs](docs/decomp/patterns/fixable-bool-mask.md))

### Related Documentation

- [docs/decomp/patterns/fixable-bool-mask.md](docs/decomp/patterns/fixable-bool-mask.md)
- [docs/decomp/patterns/permuter-roi.md#bool-materialization](docs/decomp/patterns/permuter-roi.md#bool-materialization)

## Full Instruction Listing

| Index | Target | Base | Match |
|------:|--------|------|-------|
| 0 | `mflr r12` | `mflr r12` |  |
| 1 | `bl __savegprlr` | `bl __savegprlr_14` |  |
| 2 | `stwu r1, -0x140, r1` | `stwu r1, -0x140, r1` |  |
| 3 | `mr r16, r3` | `mr r16, r3` |  |
| 4 | `stw r4, 0x15c, r1` | `stw r4, 0x15c, r1` |  |
| 5 | `cmplwi cr6, r3, 0x0` | `cmplwi cr6, r3, 0x0` |  |
| 6 | `beq cr6, 0x130c` | `beq cr6, 0x1310` |  |
| 7 | `lwz r31, 0x1c, r3` | `lwz r31, 0x1c, r3` |  |
| 8 | `cmplwi cr6, r31, 0x0` | `cmplwi cr6, r31, 0x0` |  |
| 9 | `beq cr6, 0x130c` | `beq cr6, 0x1310` |  |
| 10 | `lwz r11, 0xc, r3` | `lwz r11, 0xc, r3` |  |
| 11 | `cmplwi cr6, r11, 0x0` | `cmplwi cr6, r11, 0x0` |  |
| 12 | `beq cr6, 0x130c` | `beq cr6, 0x1310` |  |
| 13 | `lwz r11, 0x0, r3` | `lwz r11, 0x0, r3` |  |
| 14 | `cmplwi cr6, r11, 0x0` | `cmplwi cr6, r11, 0x0` |  |
| 15 | `bne cr6, 0x4c` | `bne cr6, 0x4c` |  |
| 16 | `lwz r11, 0x4, r3` | `lwz r11, 0x4, r3` |  |
| 17 | `cmplwi cr6, r11, 0x0` | `cmplwi cr6, r11, 0x0` |  |
| 18 | `bne cr6, 0x130c` | `bne cr6, 0x1310` |  |
| 19 | `lwz r11, 0x0, r31` | `lwz r11, 0x0, r31` |  |
| 20 | `cmpwi cr6, r11, 0xb` | `cmpwi cr6, r11, 0xb` |  |
| 21 | `bne cr6, 0x60` | `bne cr6, 0x60` |  |
| 22 | `li r11, 0xc` | `li r11, 0xc` |  |
| 23 | `stw r11, 0x0, r31` | `stw r11, 0x0, r31` |  |
| 24 | `lwz r11, 0x10, r16` | `lwz r11, 0x10, r16` |  |
| 25 | `li r10, 0x0` | `li r10, 0x0` |  |
| 26 | `lwz r9, 0xc, r16` | `lwz r9, 0xc, r16` |  |
| 27 | `lwz r17, 0x4, r16` | `lwz r17, 0x4, r16` |  |
| 28 | `lwz r19, 0x0, r31` | `lwz r19, 0x0, r31` |  |
| 29 | `lwz r18, 0x0, r16` | `lwz r18, 0x0, r16` |  |
| 30 | `lwz r29, 0x30, r31` | `lwz r29, 0x30, r31` |  |
| 31 | `cmplwi cr6, r19, 0x1c` | `cmplwi cr6, r19, 0x1c` |  |
| 32 | `lwz r30, 0x34, r31` | `lwz r30, 0x34, r31` |  |
| 33 | `stw r9, 0x60, r1` | `stw r9, 0x60, r1` |  |
| 34 | `stw r10, 0x5c, r1` | `stw r10, 0x5c, r1` |  |
| 35 | `stw r11, 0x64, r1` | `stw r11, 0x64, r1` |  |
| 36 | `stw r11, 0x58, r1` | `stw r11, 0x58, r1` |  |
| 37 | `stw r17, 0x9c, r1` | `stw r17, 0x9c, r1` |  |
| 38 | `bgt cr6, 0x130c` | `bgt cr6, 0x1310` |  |
| 39 | `lis r25, lbl_8215F700` | `lis r25, ??_C@_0CE@GMIGFPBB@too?5many?5length?5or?5distance?5symb@` |  |
| 40 | `lis r9, lbl_8218AC10` | `lis r9, ?lenfix@?1??fixedtables@@9@9` |  |
| 41 | `lis r8, lbl_8218B5A0` | `lis r8, ??_C@_0BH@FGKKJGOC@incorrect?5length?5check?$AA@` |  |
| 42 | `lis r7, lbl_8215E814` | `lis r7, ??_C@_0BF@MEIGEHBE@incorrect?5data?5check?$AA@` |  |
| 43 | `lis r24, lbl_8215F890` | `lis r24, ??_C@_0BN@LGAADGOK@invalid?5stored?5block?5lengths?$AA@` |  |
| 44 | `addi r10, r25, lbl_8215F700` | `addi r10, r25, ??_C@_0CE@GMIGFPBB@too?5many?5length?5or?5distance?5symb@` |  |
| 45 | `lwz r25, 0x64, r1` | `lwz r25, 0x64, r1` |  |
| 46 | `addi r9, r9, lbl_8218AC10` | `addi r9, r9, ?lenfix@?1??fixedtables@@9@9` |  |
| 47 | `addi r8, r8, lbl_8218B5A0` | `addi r8, r8, ??_C@_0BH@FGKKJGOC@incorrect?5length?5check?$AA@` |  |
| 48 | `stw r10, 0x7c, r1` | `stw r10, 0x7c, r1` |  |
| 49 | `addi r7, r7, lbl_8215E814` | `addi r7, r7, ??_C@_0BF@MEIGEHBE@incorrect?5data?5check?$AA@` |  |
| 50 | `addi r26, r24, lbl_8215F890` | `addi r26, r24, ??_C@_0BN@LGAADGOK@invalid?5stored?5block?5lengths?$AA@` |  |
| 51 | `lwz r24, 0x60, r1` | `lwz r24, 0x60, r1` |  |
| 52 | `lis r22, lbl_8218B558` | `lis r22, ??_C@_0BE@GONKLEPM@header?5crc?5mismatch?$AA@` |  |
| 53 | `stw r9, 0x60, r1` | `stw r9, 0x60, r1` |  |
| 54 | `lis r6, lbl_8218B558` | `lis r6, ??_C@_0BO@ECPMAOGG@invalid?5distance?5too?5far?5back?$AA@` |  |
| 55 | `stw r8, 0x98, r1` | `stw r8, 0x98, r1` |  |
| 56 | `lis r5, lbl_8215F890` | `lis r5, ??_C@_0BG@LBKINIKP@invalid?5distance?5code?$AA@` |  |
| 57 | `stw r7, 0x94, r1` | `stw r7, 0x94, r1` |  |
| 58 | `lis r4, lbl_8215F890` | `lis r4, ??_C@_0BM@FFFLPBBC@invalid?5literal?1length?5code?$AA@` |  |
| 59 | `stw r26, 0x78, r1` | `stw r26, 0x78, r1` |  |
| 60 | `addi r10, r22, lbl_8218B558` | `addi r10, r22, ??_C@_0BE@GONKLEPM@header?5crc?5mismatch?$AA@` |  |
| 61 | `lwz r22, 0x58, r1` | `lwz r22, 0x58, r1` |  |
| 62 | `addi r9, r6, lbl_8218B558` | `addi r9, r6, ??_C@_0BO@ECPMAOGG@invalid?5distance?5too?5far?5back?$AA@` |  |
| 63 | `addi r8, r5, lbl_8215F890` | `addi r8, r5, ??_C@_0BG@LBKINIKP@invalid?5distance?5code?$AA@` |  |
| 64 | `stw r10, 0x70, r1` | `stw r10, 0x70, r1` |  |
| 65 | `addi r7, r4, lbl_8215F890` | `addi r7, r4, ??_C@_0BM@FFFLPBBC@invalid?5literal?1length?5code?$AA@` |  |
| 66 | `stw r9, 0x90, r1` | `stw r9, 0x90, r1` |  |
| 67 | `lis r23, lbl_8215F890` | `lis r23, ??_C@_0BD@PJCBIDD@invalid?5block?5type?$AA@` |  |
| 68 | `stw r8, 0x8c, r1` | `stw r8, 0x8c, r1` |  |
| 69 | `lis r21, lbl_8218B558` | `lis r21, ??_C@_0BJ@BLBBCOMO@unknown?5header?5flags?5set?$AA@` |  |
| 70 | `stw r7, 0x88, r1` | `stw r7, 0x88, r1` |  |
| 71 | `lis r3, lbl_8218B540` | `lis r3, ??_C@_0BG@GMDFCBGP@invalid?5distances?5set?$AA@` |  |
| 72 | `lis r28, lbl_8218B520` | `lis r28, ??_C@_0BM@IIMGAINC@invalid?5literal?1lengths?5set?$AA@` |  |
| 73 | `lis r27, lbl_8215F700` | `lis r27, ??_C@_0BK@BMMPFBBH@invalid?5bit?5length?5repeat?$AA@` |  |
| 74 | `lis r11, lbl_8215E7FC` | `lis r11, ??_C@_0BH@LIBMMIGA@incorrect?5header?5check?$AA@` |  |
| 75 | `addi r23, r23, lbl_8215F890` | `addi r23, r23, ??_C@_0BD@PJCBIDD@invalid?5block?5type?$AA@` |  |
| 76 | `addi r21, r21, lbl_8218B558` | `addi r21, r21, ??_C@_0BJ@BLBBCOMO@unknown?5header?5flags?5set?$AA@` |  |
| 77 | `lis r26, lbl_8218B4F0` | `lis r26, ??_C@_0BJ@HDEPPGOH@invalid?5code?5lengths?5set?$AA@` |  |
| 78 | `stw r23, 0x74, r1` | `stw r23, 0x74, r1` |  |
| 79 | `lis r20, lbl_8215E7E8` | `lis r20, ??_C@_0BE@EMOGCLGO@invalid?5window?5size?$AA@` |  |
| 80 | `stw r21, 0x6c, r1` | `stw r21, 0x6c, r1` |  |
| 81 | `lis r10, lbl_8215E7CC` | `lis r10, ??_C@_0BL@IHKGDAEE@unknown?5compression?5method?$AA@` |  |
| 82 | `addi r9, r3, lbl_8218B540` | `addi r9, r3, ??_C@_0BG@GMDFCBGP@invalid?5distances?5set?$AA@` |  |
| 83 | `addi r8, r28, lbl_8218B520` | `addi r8, r28, ??_C@_0BM@IIMGAINC@invalid?5literal?1lengths?5set?$AA@` |  |
| 84 | `addi r7, r27, lbl_8215F700` | `addi r7, r27, ??_C@_0BK@BMMPFBBH@invalid?5bit?5length?5repeat?$AA@` |  |
| 85 | `stw r9, 0x84, r1` | `stw r9, 0x84, r1` |  |
| 86 | `addi r11, r11, lbl_8215E7FC` | `addi r11, r11, ??_C@_0BH@LIBMMIGA@incorrect?5header?5check?$AA@` |  |
| 87 | `stw r8, 0x80, r1` | `stw r8, 0x80, r1` |  |
| 88 | `li r15, 0x1` | `li r15, 0x1` |  |
| 89 | `stw r7, 0x64, r1` | `stw r7, 0x64, r1` |  |
| 90 | `li r14, 0x1b` | `li r14, 0x1b` |  |
| 91 | `stw r11, 0x68, r1` | `stw r11, 0x68, r1` |  |
| 92 | `addi r26, r26, lbl_8218B4F0` | `addi r26, r26, ??_C@_0BJ@HDEPPGOH@invalid?5code?5lengths?5set?$AA@` |  |
| 93 | `addi r20, r20, lbl_8215E7E8` | `addi r20, r20, ??_C@_0BE@EMOGCLGO@invalid?5window?5size?$AA@` |  |
| 94 | `addi r21, r10, lbl_8215E7CC` | `addi r21, r10, ??_C@_0BL@IHKGDAEE@unknown?5compression?5method?$AA@` |  |
| 95 | `li r23, 0x0` | `li r23, 0x0` |  |
| 96 | `lis r12, jumptable_8218B4D0` | `lis r12, $T2812` |  |
| 97 | `slwi r0, r19, 1` | `slwi r0, r19, 1` |  |
| 98 | `addi r12, r12, jumptable_8218B4D0` | `addi r12, r12, $T2812` |  |
| 99 | `lhzx r0, r12, r0, jumptable_8218B4D0` | `lhzx r0, r12, r0, $T2812` |  |
| 100 | `lis r12, 0x0` | `lis r12, 0x1a8` |  |
| 101 | `addi r12, r12, 0x0` | `addi r12, r12, 0x1a8` |  |
| 102 | `nop` | `nop` |  |
| 103 | `add r12, r12, r0, inflate` | `add r12, r12, r0, inflate` |  |
| 104 | `mtctr r12` | `mtctr r12` |  |
| 105 | `bctr` | `bctr` |  |
| 106 | `lwz r10, 0x8, r31` | `lwz r10, 0x8, r31` |  |
| 107 | `cmpwi cr6, r10, 0x0` | `cmpwi cr6, r10, 0x0` |  |
| 108 | `bne cr6, 0x1e0` | `bne cr6, 0x1e0` |  |
| 109 | `li r11, 0xc` | `li r11, 0xc` |  |
| 110 | `stw r11, 0x0, r31` | `stw r11, 0x0, r31` |  |
| 111 | `b 0x1300` | `b 0x1304` |  |
| 112 | `cmplwi cr6, r17, 0x0` | `cmplwi cr6, r17, 0x0` |  |
| 113 | `beq cr6, 0x1358` | `beq cr6, 0x135c` | diff_arg |
| 114 | `lbz r11, 0x0, r18` | `lbz r11, 0x0, r18` |  |
| 115 | `subi r17, r17, 0x1` | `subi r17, r17, 0x1` |  |
| 116 | `addi r18, r18, 0x1` | `addi r18, r18, 0x1` |  |
| 117 | `slw r11, r11, r30` | `slw r11, r11, r30` |  |
| 118 | `addi r30, r30, 0x8` | `addi r30, r30, 0x8` |  |
| 119 | `add r29, r11, r29` | `add r29, r11, r29` |  |
| 120 | `cmplwi cr6, r30, 0x10` | `cmplwi cr6, r30, 0x10` |  |
| 121 | `blt cr6, 0x1c0` | `blt cr6, 0x1c0` |  |
| 122 | `rlwinm. r11, r10, 0, 30, 30` | `rlwinm. r11, r10, 0, 30, 30` |  |
| 123 | `beq 0x240` | `beq 0x240` |  |
| 124 | `cmplwi cr6, r29, 0x8b1f` | `cmplwi cr6, r29, 0x8b1f` |  |
| 125 | `bne cr6, 0x240` | `bne cr6, 0x240` |  |
| 126 | `li r5, 0x0` | `li r5, 0x0` |  |
| 127 | `li r4, 0x0` | `li r4, 0x0` |  |
| 128 | `li r3, 0x0` | `li r3, 0x0` |  |
| 129 | `bl crc32_big` | `bl crc32` |  |
| 130 | `li r11, 0x8b` | `li r11, 0x8b` |  |
| 131 | `li r10, 0x1f` | `li r10, 0x1f` |  |
| 132 | `stw r3, 0x14, r31` | `stw r3, 0x14, r31` |  |
| 133 | `stb r11, 0x55, r1` | `stb r11, 0x55, r1` |  |
| 134 | `li r5, 0x2` | `li r5, 0x2` |  |
| 135 | `stb r10, 0x54, r1` | `stb r10, 0x54, r1` |  |
| 136 | `addi r4, r1, 0x54` | `addi r4, r1, 0x54` |  |
| 137 | `lwz r3, 0x14, r31` | `lwz r3, 0x14, r31` |  |
| 138 | `bl crc32_big` | `bl crc32` |  |
| 139 | `stw r3, 0x14, r31` | `stw r3, 0x14, r31` |  |
| 140 | `mr r29, r23` | `mr r29, r23` |  |
| 141 | `stw r15, 0x0, r31` | `stw r15, 0x0, r31` |  |
| 142 | `mr r30, r23` | `mr r30, r23` |  |
| 143 | `b 0x1300` | `b 0x1304` |  |
| 144 | `clrlwi. r11, r10, 31` | `clrlwi. r11, r10, 31` |  |
| 145 | `stw r23, 0x10, r31` | `stw r23, 0x10, r31` |  |
| 146 | `beq 0x2d8` | `beq 0x2d8` |  |
| 147 | `srwi r10, r29, 8` | `srwi r10, r29, 8` |  |
| 148 | `clrlslwi r11, r29, 24, 8` | `clrlslwi r11, r29, 24, 8` |  |
| 149 | `li r9, 0x1f` | `li r9, 0x1f` |  |
| 150 | `add r11, r11, r10` | `add r11, r11, r10` |  |
| 151 | `divwu r10, r11, r9` | `divwu r10, r11, r9` |  |
| 152 | `mulli r10, r10, 0x1f` | `mulli r10, r10, 0x1f` |  |
| 153 | `subf. r11, r10, r11` | `subf. r11, r10, r11` |  |
| 154 | `bne 0x2d8` | `bne 0x2d8` |  |
| 155 | `clrlwi r11, r29, 28` | `clrlwi r11, r29, 28` |  |
| 156 | `cmplwi cr6, r11, 0x8` | `cmplwi cr6, r11, 0x8` |  |
| 157 | `beq cr6, 0x280` | `beq cr6, 0x280` |  |
| 158 | `stw r21, 0x18, r16` | `stw r21, 0x18, r16` |  |
| 159 | `b 0x12fc` | `b 0x1300` |  |
| 160 | `srwi r29, r29, 4` | `srwi r29, r29, 4` |  |
| 161 | `lwz r10, 0x1c, r31` | `lwz r10, 0x1c, r31` |  |
| 162 | `subi r30, r30, 0x4` | `subi r30, r30, 0x4` |  |
| 163 | `clrlwi r11, r29, 28` | `clrlwi r11, r29, 28` |  |
| 164 | `addi r11, r11, 0x8` | `addi r11, r11, 0x8` |  |
| 165 | `cmplw cr6, r11, r10` | `cmplw cr6, r11, r10` |  |
| 166 | `ble cr6, 0x2a4` | `ble cr6, 0x2a4` |  |
| 167 | `stw r20, 0x18, r16` | `stw r20, 0x18, r16` |  |
| 168 | `b 0x12fc` | `b 0x1300` |  |
| 169 | `li r5, 0x0` | `li r5, 0x0` |  |
| 170 | `li r4, 0x0` | `li r4, 0x0` |  |
| 171 | `li r3, 0x0` | `li r3, 0x0` |  |
| 172 | `bl ?adler32@D3DX@@YAKKPBEI@Z` | `bl adler32` |  |
| 173 | `nor r11, r29, r29` | `nor r11, r29, r29` |  |
| 174 | `li r10, 0x9` | `li r10, 0x9` |  |
| 175 | `stw r3, 0x14, r31` | `stw r3, 0x14, r31` |  |
| 176 | `mr r29, r23` | `mr r29, r23` |  |
| 177 | `stw r3, 0x30, r16` | `stw r3, 0x30, r16` |  |
| 178 | `rlwimi r10, r11, 24, 30, 30` | `rlwimi r10, r11, 24, 30, 30` |  |
| 179 | `mr r30, r23` | `mr r30, r23` |  |
| 180 | `stw r10, 0x0, r31` | `stw r10, 0x0, r31` |  |
| 181 | `b 0x1300` | `b 0x1304` |  |
| 182 | `lwz r11, 0x68, r1` | `lwz r11, 0x68, r1` |  |
| 183 | `b 0x12f8` | `b 0x12fc` |  |
| 184 | `cmplwi cr6, r17, 0x0` | `cmplwi cr6, r17, 0x0` |  |
| 185 | `beq cr6, 0x1358` | `beq cr6, 0x135c` | diff_arg |
| 186 | `lbz r11, 0x0, r18` | `lbz r11, 0x0, r18` |  |
| 187 | `subi r17, r17, 0x1` | `subi r17, r17, 0x1` |  |
| 188 | `addi r18, r18, 0x1` | `addi r18, r18, 0x1` |  |
| 189 | `slw r11, r11, r30` | `slw r11, r11, r30` |  |
| 190 | `addi r30, r30, 0x8` | `addi r30, r30, 0x8` |  |
| 191 | `add r29, r11, r29` | `add r29, r11, r29` |  |
| 192 | `cmplwi cr6, r30, 0x10` | `cmplwi cr6, r30, 0x10` |  |
| 193 | `blt cr6, 0x2e0` | `blt cr6, 0x2e0` |  |
| 194 | `clrlwi r11, r29, 24` | `clrlwi r11, r29, 24` |  |
| 195 | `stw r29, 0x10, r31` | `stw r29, 0x10, r31` |  |
| 196 | `cmpwi cr6, r11, 0x8` | `cmpwi cr6, r11, 0x8` |  |
| 197 | `bne cr6, 0x278` | `bne cr6, 0x278` |  |
| 198 | `rlwinm. r11, r29, 0, 16, 18` | `rlwinm. r11, r29, 0, 16, 18` |  |
| 199 | `beq 0x328` | `beq 0x328` |  |
| 200 | `lwz r11, 0x6c, r1` | `lwz r11, 0x6c, r1` |  |
| 201 | `b 0x12f8` | `b 0x12fc` |  |
| 202 | `rlwinm. r11, r29, 0, 22, 22` | `rlwinm. r11, r29, 0, 22, 22` |  |
| 203 | `beq 0x350` | `beq 0x350` |  |
| 204 | `extrwi r10, r29, 8, 16` | `extrwi r10, r29, 8, 16` |  |
| 205 | `stb r29, 0x54, r1` | `stb r29, 0x54, r1` |  |
| 206 | `li r5, 0x2` | `li r5, 0x2` |  |
| 207 | `stb r10, 0x55, r1` | `stb r10, 0x55, r1` |  |
| 208 | `addi r4, r1, 0x54` | `addi r4, r1, 0x54` |  |
| 209 | `lwz r3, 0x14, r31` | `lwz r3, 0x14, r31` |  |
| 210 | `bl crc32_big` | `bl crc32` |  |
| 211 | `stw r3, 0x14, r31` | `stw r3, 0x14, r31` |  |
| 212 | `li r11, 0x2` | `li r11, 0x2` |  |
| 213 | `mr r29, r23` | `mr r29, r23` |  |
| 214 | `mr r30, r23` | `mr r30, r23` |  |
| 215 | `stw r11, 0x0, r31` | `stw r11, 0x0, r31` |  |
| 216 | `b 0x384` | `b 0x384` |  |
| 217 | `cmplwi cr6, r17, 0x0` | `cmplwi cr6, r17, 0x0` |  |
| 218 | `beq cr6, 0x1358` | `beq cr6, 0x135c` | diff_arg |
| 219 | `lbz r11, 0x0, r18` | `lbz r11, 0x0, r18` |  |
| 220 | `subi r17, r17, 0x1` | `subi r17, r17, 0x1` |  |
| 221 | `addi r18, r18, 0x1` | `addi r18, r18, 0x1` |  |
| 222 | `slw r11, r11, r30` | `slw r11, r11, r30` |  |
| 223 | `addi r30, r30, 0x8` | `addi r30, r30, 0x8` |  |
| 224 | `add r29, r11, r29` | `add r29, r11, r29` |  |
| 225 | `cmplwi cr6, r30, 0x20` | `cmplwi cr6, r30, 0x20` |  |
| 226 | `blt cr6, 0x364` | `blt cr6, 0x364` |  |
| 227 | `lwz r11, 0x10, r31` | `lwz r11, 0x10, r31` |  |
| 228 | `rlwinm. r11, r11, 0, 22, 22` | `rlwinm. r11, r11, 0, 22, 22` |  |
| 229 | `beq 0x3c8` | `beq 0x3c8` |  |
| 230 | `srwi r11, r29, 24` | `srwi r11, r29, 24` |  |
| 231 | `stb r29, 0x54, r1` | `stb r29, 0x54, r1` |  |
| 232 | `extrwi r9, r29, 8, 16` | `extrwi r9, r29, 8, 16` |  |
| 233 | `stb r11, 0x57, r1` | `stb r11, 0x57, r1` |  |
| 234 | `extrwi r11, r29, 8, 8` | `extrwi r11, r29, 8, 8` |  |
| 235 | `stb r9, 0x55, r1` | `stb r9, 0x55, r1` |  |
| 236 | `li r5, 0x4` | `li r5, 0x4` |  |
| 237 | `stb r11, 0x56, r1` | `stb r11, 0x56, r1` |  |
| 238 | `addi r4, r1, 0x54` | `addi r4, r1, 0x54` |  |
| 239 | `lwz r3, 0x14, r31` | `lwz r3, 0x14, r31` |  |
| 240 | `bl crc32_big` | `bl crc32` |  |
| 241 | `stw r3, 0x14, r31` | `stw r3, 0x14, r31` |  |
| 242 | `li r11, 0x3` | `li r11, 0x3` |  |
| 243 | `mr r29, r23` | `mr r29, r23` |  |
| 244 | `mr r30, r23` | `mr r30, r23` |  |
| 245 | `stw r11, 0x0, r31` | `stw r11, 0x0, r31` |  |
| 246 | `b 0x3fc` | `b 0x3fc` |  |
| 247 | `cmplwi cr6, r17, 0x0` | `cmplwi cr6, r17, 0x0` |  |
| 248 | `beq cr6, 0x1358` | `beq cr6, 0x135c` | diff_arg |
| 249 | `lbz r11, 0x0, r18` | `lbz r11, 0x0, r18` |  |
| 250 | `subi r17, r17, 0x1` | `subi r17, r17, 0x1` |  |
| 251 | `addi r18, r18, 0x1` | `addi r18, r18, 0x1` |  |
| 252 | `slw r11, r11, r30` | `slw r11, r11, r30` |  |
| 253 | `addi r30, r30, 0x8` | `addi r30, r30, 0x8` |  |
| 254 | `add r29, r11, r29` | `add r29, r11, r29` |  |
| 255 | `cmplwi cr6, r30, 0x10` | `cmplwi cr6, r30, 0x10` |  |
| 256 | `blt cr6, 0x3dc` | `blt cr6, 0x3dc` |  |
| 257 | `lwz r11, 0x10, r31` | `lwz r11, 0x10, r31` |  |
| 258 | `rlwinm. r11, r11, 0, 22, 22` | `rlwinm. r11, r11, 0, 22, 22` |  |
| 259 | `beq 0x430` | `beq 0x430` |  |
| 260 | `extrwi r10, r29, 8, 16` | `extrwi r10, r29, 8, 16` |  |
| 261 | `stb r29, 0x54, r1` | `stb r29, 0x54, r1` |  |
| 262 | `li r5, 0x2` | `li r5, 0x2` |  |
| 263 | `stb r10, 0x55, r1` | `stb r10, 0x55, r1` |  |
| 264 | `addi r4, r1, 0x54` | `addi r4, r1, 0x54` |  |
| 265 | `lwz r3, 0x14, r31` | `lwz r3, 0x14, r31` |  |
| 266 | `bl crc32_big` | `bl crc32` |  |
| 267 | `stw r3, 0x14, r31` | `stw r3, 0x14, r31` |  |
| 268 | `li r11, 0x4` | `li r11, 0x4` |  |
| 269 | `mr r29, r23` | `mr r29, r23` |  |
| 270 | `mr r30, r23` | `mr r30, r23` |  |
| 271 | `stw r11, 0x0, r31` | `stw r11, 0x0, r31` |  |
| 272 | `lwz r11, 0x10, r31` | `lwz r11, 0x10, r31` |  |
| 273 | `rlwinm. r11, r11, 0, 21, 21` | `rlwinm. r11, r11, 0, 21, 21` |  |
| 274 | `beq 0x4b0` | `beq 0x4b0` |  |
| 275 | `b 0x470` | `b 0x470` |  |
| 276 | `cmplwi cr6, r17, 0x0` | `cmplwi cr6, r17, 0x0` |  |
| 277 | `beq cr6, 0x1358` | `beq cr6, 0x135c` | diff_arg |
| 278 | `lbz r11, 0x0, r18` | `lbz r11, 0x0, r18` |  |
| 279 | `subi r17, r17, 0x1` | `subi r17, r17, 0x1` |  |
| 280 | `addi r18, r18, 0x1` | `addi r18, r18, 0x1` |  |
| 281 | `slw r11, r11, r30` | `slw r11, r11, r30` |  |
| 282 | `addi r30, r30, 0x8` | `addi r30, r30, 0x8` |  |
| 283 | `add r29, r11, r29` | `add r29, r11, r29` |  |
| 284 | `cmplwi cr6, r30, 0x10` | `cmplwi cr6, r30, 0x10` |  |
| 285 | `blt cr6, 0x450` | `blt cr6, 0x450` |  |
| 286 | `lwz r11, 0x10, r31` | `lwz r11, 0x10, r31` |  |
| 287 | `stw r29, 0x38, r31` | `stw r29, 0x38, r31` |  |
| 288 | `rlwinm. r11, r11, 0, 22, 22` | `rlwinm. r11, r11, 0, 22, 22` |  |
| 289 | `beq 0x4a8` | `beq 0x4a8` |  |
| 290 | `extrwi r11, r29, 8, 16` | `extrwi r11, r29, 8, 16` |  |
| 291 | `stb r29, 0x54, r1` | `stb r29, 0x54, r1` |  |
| 292 | `li r5, 0x2` | `li r5, 0x2` |  |
| 293 | `stb r11, 0x55, r1` | `stb r11, 0x55, r1` |  |
| 294 | `addi r4, r1, 0x54` | `addi r4, r1, 0x54` |  |
| 295 | `lwz r3, 0x14, r31` | `lwz r3, 0x14, r31` |  |
| 296 | `bl crc32_big` | `bl crc32` |  |
| 297 | `stw r3, 0x14, r31` | `stw r3, 0x14, r31` |  |
| 298 | `mr r29, r23` | `mr r29, r23` |  |
| 299 | `mr r30, r23` | `mr r30, r23` |  |
| 300 | `li r11, 0x5` | `li r11, 0x5` |  |
| 301 | `stw r11, 0x0, r31` | `stw r11, 0x0, r31` |  |
| 302 | `lwz r11, 0x10, r31` | `lwz r11, 0x10, r31` |  |
| 303 | `rlwinm. r10, r11, 0, 21, 21` | `rlwinm. r10, r11, 0, 21, 21` |  |
| 304 | `beq 0x518` | `beq 0x518` |  |
| 305 | `lwz r28, 0x38, r31` | `lwz r28, 0x38, r31` |  |
| 306 | `cmplw cr6, r28, r17` | `cmplw cr6, r28, r17` |  |
| 307 | `ble cr6, 0x4d4` | `ble cr6, 0x4d4` |  |
| 308 | `mr r28, r17` | `mr r28, r17` |  |
| 309 | `cmplwi cr6, r28, 0x0` | `cmplwi cr6, r28, 0x0` |  |
| 310 | `beq cr6, 0x50c` | `beq cr6, 0x50c` |  |
| 311 | `rlwinm. r11, r11, 0, 22, 22` | `rlwinm. r11, r11, 0, 22, 22` |  |
| 312 | `beq 0x4f8` | `beq 0x4f8` |  |
| 313 | `mr r5, r28` | `mr r5, r28` |  |
| 314 | `lwz r3, 0x14, r31` | `lwz r3, 0x14, r31` |  |
| 315 | `mr r4, r18` | `mr r4, r18` |  |
| 316 | `bl crc32_big` | `bl crc32` |  |
| 317 | `stw r3, 0x14, r31` | `stw r3, 0x14, r31` |  |
| 318 | `lwz r11, 0x38, r31` | `lwz r11, 0x38, r31` |  |
| 319 | `subf r17, r28, r17` | `subf r17, r28, r17` |  |
| 320 | `add r18, r28, r18` | `add r18, r28, r18` |  |
| 321 | `subf r11, r28, r11` | `subf r11, r28, r11` |  |
| 322 | `stw r11, 0x38, r31` | `stw r11, 0x38, r31` |  |
| 323 | `lwz r11, 0x38, r31` | `lwz r11, 0x38, r31` |  |
| 324 | `cmplwi cr6, r11, 0x0` | `cmplwi cr6, r11, 0x0` |  |
| 325 | `bne cr6, 0x1358` | `bne cr6, 0x135c` | diff_arg |
| 326 | `li r11, 0x6` | `li r11, 0x6` |  |
| 327 | `stw r11, 0x0, r31` | `stw r11, 0x0, r31` |  |
| 328 | `lwz r11, 0x10, r31` | `lwz r11, 0x10, r31` |  |
| 329 | `rlwinm. r10, r11, 0, 20, 20` | `rlwinm. r10, r11, 0, 20, 20` |  |
| 330 | `beq 0x57c` | `beq 0x57c` |  |
| 331 | `cmplwi cr6, r17, 0x0` | `cmplwi cr6, r17, 0x0` |  |
| 332 | `beq cr6, 0x1358` | `beq cr6, 0x135c` | diff_arg |
| 333 | `mr r28, r23` | `mr r28, r23` |  |
| 334 | `lbzx r27, r28, r18` | `lbzx r27, r28, r18` |  |
| 335 | `addi r28, r28, 0x1` | `addi r28, r28, 0x1` |  |
| 336 | `cmplwi r27, 0x0` | `cmplwi r27, 0x0` |  |
| 337 | `beq 0x550` | `beq 0x550` |  |
| 338 | `cmplw cr6, r28, r17` | `cmplw cr6, r28, r17` |  |
| 339 | `blt cr6, 0x538` | `blt cr6, 0x538` |  |
| 340 | `rlwinm. r11, r11, 0, 18, 18` | `rlwinm. r11, r11, 0, 18, 18` |  |
| 341 | `beq 0x56c` | `beq 0x56c` |  |
| 342 | `mr r5, r28` | `mr r5, r28` |  |
| 343 | `lwz r3, 0x14, r31` | `lwz r3, 0x14, r31` |  |
| 344 | `mr r4, r18` | `mr r4, r18` |  |
| 345 | `bl crc32_big` | `bl crc32` |  |
| 346 | `stw r3, 0x14, r31` | `stw r3, 0x14, r31` |  |
| 347 | `subf r17, r28, r17` | `subf r17, r28, r17` |  |
| 348 | `add r18, r28, r18` | `add r18, r28, r18` |  |
| 349 | `cmplwi cr6, r27, 0x0` | `cmplwi cr6, r27, 0x0` |  |
| 350 | `bne cr6, 0x1358` | `bne cr6, 0x135c` | diff_arg |
| 351 | `li r11, 0x7` | `li r11, 0x7` |  |
| 352 | `stw r11, 0x0, r31` | `stw r11, 0x0, r31` |  |
| 353 | `lwz r11, 0x10, r31` | `lwz r11, 0x10, r31` |  |
| 354 | `rlwinm. r10, r11, 0, 19, 19` | `rlwinm. r10, r11, 0, 19, 19` |  |
| 355 | `beq 0x5e0` | `beq 0x5e0` |  |
| 356 | `cmplwi cr6, r17, 0x0` | `cmplwi cr6, r17, 0x0` |  |
| 357 | `beq cr6, 0x1358` | `beq cr6, 0x135c` | diff_arg |
| 358 | `mr r28, r23` | `mr r28, r23` |  |
| 359 | `lbzx r27, r28, r18` | `lbzx r27, r28, r18` |  |
| 360 | `addi r28, r28, 0x1` | `addi r28, r28, 0x1` |  |
| 361 | `cmplwi r27, 0x0` | `cmplwi r27, 0x0` |  |
| 362 | `beq 0x5b4` | `beq 0x5b4` |  |
| 363 | `cmplw cr6, r28, r17` | `cmplw cr6, r28, r17` |  |
| 364 | `blt cr6, 0x59c` | `blt cr6, 0x59c` |  |
| 365 | `rlwinm. r11, r11, 0, 18, 18` | `rlwinm. r11, r11, 0, 18, 18` |  |
| 366 | `beq 0x5d0` | `beq 0x5d0` |  |
| 367 | `mr r5, r28` | `mr r5, r28` |  |
| 368 | `lwz r3, 0x14, r31` | `lwz r3, 0x14, r31` |  |
| 369 | `mr r4, r18` | `mr r4, r18` |  |
| 370 | `bl crc32_big` | `bl crc32` |  |
| 371 | `stw r3, 0x14, r31` | `stw r3, 0x14, r31` |  |
| 372 | `subf r17, r28, r17` | `subf r17, r28, r17` |  |
| 373 | `add r18, r28, r18` | `add r18, r28, r18` |  |
| 374 | `cmplwi cr6, r27, 0x0` | `cmplwi cr6, r27, 0x0` |  |
| 375 | `bne cr6, 0x1358` | `bne cr6, 0x135c` | diff_arg |
| 376 | `li r11, 0x8` | `li r11, 0x8` |  |
| 377 | `stw r11, 0x0, r31` | `stw r11, 0x0, r31` |  |
| 378 | `lwz r11, 0x10, r31` | `lwz r11, 0x10, r31` |  |
| 379 | `rlwinm. r11, r11, 0, 22, 22` | `rlwinm. r11, r11, 0, 22, 22` |  |
| 380 | `beq 0x63c` | `beq 0x63c` |  |
| 381 | `b 0x618` | `b 0x618` |  |
| 382 | `cmplwi cr6, r17, 0x0` | `cmplwi cr6, r17, 0x0` |  |
| 383 | `beq cr6, 0x1358` | `beq cr6, 0x135c` | diff_arg |
| 384 | `lbz r11, 0x0, r18` | `lbz r11, 0x0, r18` |  |
| 385 | `subi r17, r17, 0x1` | `subi r17, r17, 0x1` |  |
| 386 | `addi r18, r18, 0x1` | `addi r18, r18, 0x1` |  |
| 387 | `slw r11, r11, r30` | `slw r11, r11, r30` |  |
| 388 | `addi r30, r30, 0x8` | `addi r30, r30, 0x8` |  |
| 389 | `add r29, r11, r29` | `add r29, r11, r29` |  |
| 390 | `cmplwi cr6, r30, 0x10` | `cmplwi cr6, r30, 0x10` |  |
| 391 | `blt cr6, 0x5f8` | `blt cr6, 0x5f8` |  |
| 392 | `lhz r11, 0x16, r31` | `lhz r11, 0x16, r31` |  |
| 393 | `cmplw cr6, r29, r11` | `cmplw cr6, r29, r11` |  |
| 394 | `beq cr6, 0x634` | `beq cr6, 0x634` |  |
| 395 | `lwz r11, 0x70, r1` | `lwz r11, 0x70, r1` |  |
| 396 | `b 0x12f8` | `b 0x12fc` |  |
| 397 | `mr r29, r23` | `mr r29, r23` |  |
| 398 | `mr r30, r23` | `mr r30, r23` |  |
| 399 | `li r5, 0x0` | `li r5, 0x0` |  |
| 400 | `li r4, 0x0` | `li r4, 0x0` |  |
| 401 | `li r3, 0x0` | `li r3, 0x0` |  |
| 402 | `bl crc32_big` | `bl crc32` |  |
| 403 | `stw r3, 0x14, r31` | `stw r3, 0x14, r31` |  |
| 404 | `li r11, 0xb` | `li r11, 0xb` |  |
| 405 | `stw r3, 0x30, r16` | `stw r3, 0x30, r16` |  |
| 406 | `b 0x1b8` | `b 0x1b8` |  |
| 407 | `cmplwi cr6, r17, 0x0` | `cmplwi cr6, r17, 0x0` |  |
| 408 | `beq cr6, 0x1358` | `beq cr6, 0x135c` | diff_arg |
| 409 | `lbz r11, 0x0, r18` | `lbz r11, 0x0, r18` |  |
| 410 | `subi r17, r17, 0x1` | `subi r17, r17, 0x1` |  |
| 411 | `addi r18, r18, 0x1` | `addi r18, r18, 0x1` |  |
| 412 | `slw r11, r11, r30` | `slw r11, r11, r30` |  |
| 413 | `addi r30, r30, 0x8` | `addi r30, r30, 0x8` |  |
| 414 | `add r29, r11, r29` | `add r29, r11, r29` |  |
| 415 | `cmplwi cr6, r30, 0x20` | `cmplwi cr6, r30, 0x20` |  |
| 416 | `blt cr6, 0x65c` | `blt cr6, 0x65c` |  |
| 417 | `slwi r10, r29, 16` | `slwi r10, r29, 16` |  |
| 418 | `rlwinm r11, r29, 0, 16, 23` | `rlwinm r11, r29, 0, 16, 23` |  |
| 419 | `rlwinm r9, r29, 24, 16, 23` | `rlwinm r9, r29, 24, 16, 23` |  |
| 420 | `add r11, r11, r10` | `add r11, r11, r10` |  |
| 421 | `srwi r10, r29, 24` | `srwi r10, r29, 24` |  |
| 422 | `slwi r11, r11, 8` | `slwi r11, r11, 8` |  |
| 423 | `li r8, 0xa` | `li r8, 0xa` |  |
| 424 | `add r11, r11, r9` | `add r11, r11, r9` |  |
| 425 | `mr r29, r23` | `mr r29, r23` |  |
| 426 | `add r11, r11, r10` | `add r11, r11, r10` |  |
| 427 | `mr r30, r23` | `mr r30, r23` |  |
| 428 | `stw r11, 0x14, r31` | `stw r11, 0x14, r31` |  |
| 429 | `stw r11, 0x30, r16` | `stw r11, 0x30, r16` |  |
| 430 | `stw r8, 0x0, r31` | `stw r8, 0x0, r31` |  |
| 431 | `lwz r11, 0xc, r31` | `lwz r11, 0xc, r31` |  |
| 432 | `cmpwi cr6, r11, 0x0` | `cmpwi cr6, r11, 0x0` |  |
| 433 | `beq cr6, 0x1318` | `beq cr6, 0x131c` |  |
| 434 | `li r5, 0x0` | `li r5, 0x0` |  |
| 435 | `li r4, 0x0` | `li r4, 0x0` |  |
| 436 | `li r3, 0x0` | `li r3, 0x0` |  |
| 437 | `bl ?adler32@D3DX@@YAKKPBEI@Z` | `bl adler32` |  |
| 438 | `li r11, 0xb` | `li r11, 0xb` |  |
| 439 | `stw r3, 0x14, r31` | `stw r3, 0x14, r31` |  |
| 440 | `stw r3, 0x30, r16` | `stw r3, 0x30, r16` |  |
| 441 | `stw r11, 0x0, r31` | `stw r11, 0x0, r31` |  |
| 442 | `lwz r11, 0x15c, r1` | `lwz r11, 0x15c, r1` |  |
| 443 | `cmpwi cr6, r11, 0x5` | `cmpwi cr6, r11, 0x5` |  |
| 444 | `beq cr6, 0x1358` | `beq cr6, 0x135c` | diff_arg |
| 445 | `lwz r11, 0x4, r31` | `lwz r11, 0x4, r31` |  |
| 446 | `cmpwi cr6, r11, 0x0` | `cmpwi cr6, r11, 0x0` |  |
| 447 | `beq cr6, 0x734` | `beq cr6, 0x734` |  |
| 448 | `clrlwi r11, r30, 29` | `clrlwi r11, r30, 29` |  |
| 449 | `li r10, 0x18` | `li r10, 0x18` |  |
| 450 | `srw r29, r29, r11` | `srw r29, r29, r11` |  |
| 451 | `subf r30, r11, r30` | `subf r30, r11, r30` |  |
| 452 | `b 0x2d0` | `b 0x2d0` |  |
| 453 | `cmplwi cr6, r17, 0x0` | `cmplwi cr6, r17, 0x0` |  |
| 454 | `beq cr6, 0x1358` | `beq cr6, 0x135c` | diff_arg |
| 455 | `lbz r11, 0x0, r18` | `lbz r11, 0x0, r18` |  |
| 456 | `subi r17, r17, 0x1` | `subi r17, r17, 0x1` |  |
| 457 | `addi r18, r18, 0x1` | `addi r18, r18, 0x1` |  |
| 458 | `slw r11, r11, r30` | `slw r11, r11, r30` |  |
| 459 | `addi r30, r30, 0x8` | `addi r30, r30, 0x8` |  |
| 460 | `add r29, r11, r29` | `add r29, r11, r29` |  |
| 461 | `cmplwi cr6, r30, 0x3` | `cmplwi cr6, r30, 0x3` |  |
| 462 | `blt cr6, 0x714` | `blt cr6, 0x714` |  |
| 463 | `srwi r10, r29, 1` | `srwi r10, r29, 1` |  |
| 464 | `clrlwi r9, r29, 31` | `clrlwi r9, r29, 31` |  |
| 465 | `clrlwi r11, r10, 30` | `clrlwi r11, r10, 30` |  |
| 466 | `stw r9, 0x4, r31` | `stw r9, 0x4, r31` |  |
| 467 | `subi r9, r30, 0x1` | `subi r9, r30, 0x1` |  |
| 468 | `cmplwi cr6, r11, 0x1` | `cmplwi cr6, r11, 0x1` |  |
| 469 | `blt cr6, 0x7ac` | `blt cr6, 0x7ac` |  |
| 470 | `beq cr6, 0x780` | `beq cr6, 0x780` |  |
| 471 | `cmplwi cr6, r11, 0x3` | `cmplwi cr6, r11, 0x3` |  |
| 472 | `blt cr6, 0x778` | `blt cr6, 0x778` |  |
| 473 | `bne cr6, 0x7b4` | `bne cr6, 0x7b4` |  |
| 474 | `lwz r11, 0x74, r1` | `lwz r11, 0x74, r1` |  |
| 475 | `stw r11, 0x18, r16` | `stw r11, 0x18, r16` |  |
| 476 | `stw r14, 0x0, r31` | `stw r14, 0x0, r31` |  |
| 477 | `b 0x7b4` | `b 0x7b4` |  |
| 478 | `li r11, 0xf` | `li r11, 0xf` |  |
| 479 | `b 0x7b0` | `b 0x7b0` |  |
| 480 | `li r8, 0x9` | `li r8, 0x9` |  |
| 481 | `lwz r11, 0x60, r1` | `lwz r11, 0x60, r1` |  |
| 482 | `li r7, 0x5` | `li r7, 0x5` |  |
| 483 | `stw r8, 0x4c, r31` | `stw r8, 0x4c, r31` |  |
| 484 | `addi r6, r11, 0x800` | `addi r6, r11, 0x800` |  |
| 485 | `li r8, 0x12` | `li r8, 0x12` |  |
| 486 | `stw r7, 0x50, r31` | `stw r7, 0x50, r31` |  |
| 487 | `stw r6, 0x48, r31` | `stw r6, 0x48, r31` |  |
| 488 | `stw r11, 0x44, r31` | `stw r11, 0x44, r31` |  |
| 489 | `stw r8, 0x0, r31` | `stw r8, 0x0, r31` |  |
| 490 | `b 0x7b4` | `b 0x7b4` |  |
| 491 | `li r11, 0xd` | `li r11, 0xd` |  |
| 492 | `stw r11, 0x0, r31` | `stw r11, 0x0, r31` |  |
| 493 | `srwi r29, r10, 2` | `srwi r29, r10, 2` |  |
| 494 | `subi r30, r9, 0x2` | `subi r30, r9, 0x2` |  |
| 495 | `b 0x1300` | `b 0x1304` |  |
| 496 | `clrlwi r11, r30, 29` | `clrlwi r11, r30, 29` |  |
| 497 | `subf r30, r11, r30` | `subf r30, r11, r30` |  |
| 498 | `srw r29, r29, r11` | `srw r29, r29, r11` |  |
| 499 | `b 0x7f0` | `b 0x7f0` |  |
| 500 | `cmplwi cr6, r17, 0x0` | `cmplwi cr6, r17, 0x0` |  |
| 501 | `beq cr6, 0x1358` | `beq cr6, 0x135c` | diff_arg |
| 502 | `lbz r11, 0x0, r18` | `lbz r11, 0x0, r18` |  |
| 503 | `subi r17, r17, 0x1` | `subi r17, r17, 0x1` |  |
| 504 | `addi r18, r18, 0x1` | `addi r18, r18, 0x1` |  |
| 505 | `slw r11, r11, r30` | `slw r11, r11, r30` |  |
| 506 | `addi r30, r30, 0x8` | `addi r30, r30, 0x8` |  |
| 507 | `add r29, r11, r29` | `add r29, r11, r29` |  |
| 508 | `cmplwi cr6, r30, 0x20` | `cmplwi cr6, r30, 0x20` |  |
| 509 | `blt cr6, 0x7d0` | `blt cr6, 0x7d0` |  |
| 510 | `nor r10, r29, r29` | `nor r10, r29, r29` |  |
| 511 | `clrlwi r11, r29, 16` | `clrlwi r11, r29, 16` |  |
| 512 | `srwi r10, r10, 16` | `srwi r10, r10, 16` |  |
| 513 | `cmplw cr6, r11, r10` | `cmplw cr6, r11, r10` |  |
| 514 | `beq cr6, 0x814` | `beq cr6, 0x814` |  |
| 515 | `lwz r11, 0x78, r1` | `lwz r11, 0x78, r1` |  |
| 516 | `b 0x12f8` | `b 0x12fc` |  |
| 517 | `li r10, 0xe` | `li r10, 0xe` |  |
| 518 | `stw r11, 0x38, r31` | `stw r11, 0x38, r31` |  |
| 519 | `mr r29, r23` | `mr r29, r23` |  |
| 520 | `mr r30, r23` | `mr r30, r23` |  |
| 521 | `stw r10, 0x0, r31` | `stw r10, 0x0, r31` |  |
| 522 | `lwz r28, 0x38, r31` | `lwz r28, 0x38, r31` |  |
| 523 | `cmplwi cr6, r28, 0x0` | `cmplwi cr6, r28, 0x0` |  |
| 524 | `beq cr6, 0x884` | `beq cr6, 0x884` |  |
| 525 | `cmplw cr6, r28, r17` | `cmplw cr6, r28, r17` |  |
| 526 | `ble cr6, 0x840` | `ble cr6, 0x840` |  |
| 527 | `mr r28, r17` | `mr r28, r17` |  |
| 528 | `cmplw cr6, r28, r25` | `cmplw cr6, r28, r25` |  |
| 529 | `ble cr6, 0x84c` | `ble cr6, 0x84c` |  |
| 530 | `mr r28, r25` | `mr r28, r25` |  |
| 531 | `cmplwi cr6, r28, 0x0` | `cmplwi cr6, r28, 0x0` |  |
| 532 | `beq cr6, 0x1358` | `beq cr6, 0x135c` | diff_arg |
| 533 | `mr r5, r28` | `mr r5, r28` |  |
| 534 | `mr r4, r18` | `mr r4, r18` |  |
| 535 | `mr r3, r24` | `mr r3, r24` |  |
| 536 | `bl memmove` | `bl memmove` |  |
| 537 | `lwz r11, 0x38, r31` | `lwz r11, 0x38, r31` |  |
| 538 | `subf r17, r28, r17` | `subf r17, r28, r17` |  |
| 539 | `subf r11, r28, r11` | `subf r11, r28, r11` |  |
| 540 | `add r18, r28, r18` | `add r18, r28, r18` |  |
| 541 | `subf r25, r28, r25` | `subf r25, r28, r25` |  |
| 542 | `stw r11, 0x38, r31` | `stw r11, 0x38, r31` |  |
| 543 | `add r24, r28, r24` | `add r24, r28, r24` |  |
| 544 | `b 0x1300` | `b 0x1304` |  |
| 545 | `li r11, 0xb` | `li r11, 0xb` |  |
| 546 | `b 0x1b8` | `b 0x1b8` |  |
| 547 | `cmplwi cr6, r17, 0x0` | `cmplwi cr6, r17, 0x0` |  |
| 548 | `beq cr6, 0x1358` | `beq cr6, 0x135c` | diff_arg |
| 549 | `lbz r11, 0x0, r18` | `lbz r11, 0x0, r18` |  |
| 550 | `subi r17, r17, 0x1` | `subi r17, r17, 0x1` |  |
| 551 | `addi r18, r18, 0x1` | `addi r18, r18, 0x1` |  |
| 552 | `slw r11, r11, r30` | `slw r11, r11, r30` |  |
| 553 | `addi r30, r30, 0x8` | `addi r30, r30, 0x8` |  |
| 554 | `add r29, r11, r29` | `add r29, r11, r29` |  |
| 555 | `cmplwi cr6, r30, 0xe` | `cmplwi cr6, r30, 0xe` |  |
| 556 | `blt cr6, 0x88c` | `blt cr6, 0x88c` |  |
| 557 | `clrlwi r11, r29, 27` | `clrlwi r11, r29, 27` |  |
| 558 | `srwi r10, r29, 5` | `srwi r10, r29, 5` |  |
| 559 | `addi r11, r11, 0x101` | `addi r11, r11, 0x101` |  |
| 560 | `srwi r9, r10, 5` | `srwi r9, r10, 5` |  |
| 561 | `stw r11, 0x58, r31` | `stw r11, 0x58, r31` |  |
| 562 | `clrlwi r10, r10, 27` | `clrlwi r10, r10, 27` |  |
| 563 | `clrlwi r11, r9, 28` | `clrlwi r11, r9, 28` |  |
| 564 | `addi r10, r10, 0x1` | `addi r10, r10, 0x1` |  |
| 565 | `addi r11, r11, 0x4` | `addi r11, r11, 0x4` |  |
| 566 | `srwi r29, r9, 4` | `srwi r29, r9, 4` |  |
| 567 | `stw r10, 0x5c, r31` | `stw r10, 0x5c, r31` |  |
| 568 | `stw r11, 0x54, r31` | `stw r11, 0x54, r31` |  |
| 569 | `subi r30, r30, 0xe` | `subi r30, r30, 0xe` |  |
| 570 | `lwz r11, 0x58, r31` | `lwz r11, 0x58, r31` |  |
| 571 | `cmplwi cr6, r11, 0x11e` | `cmplwi cr6, r11, 0x11e` |  |
| 572 | `bgt cr6, 0x910` | `bgt cr6, 0x910` |  |
| 573 | `clrrwi r11, r10, 0` | `clrrwi r11, r10, 0` |  |
| 574 | `cmplwi cr6, r11, 0x1e` | `cmplwi cr6, r11, 0x1e` |  |
| 575 | `bgt cr6, 0x910` | `bgt cr6, 0x910` |  |
| 576 | `li r11, 0x10` | `li r11, 0x10` |  |
| 577 | `stw r23, 0x60, r31` | `stw r23, 0x60, r31` |  |
| 578 | `stw r11, 0x0, r31` | `stw r11, 0x0, r31` |  |
| 579 | `b 0x978` | `b 0x978` |  |
| 580 | `lwz r11, 0x7c, r1` | `lwz r11, 0x7c, r1` |  |
| 581 | `b 0x12f8` | `b 0x12fc` |  |
| 582 | `cmplwi cr6, r17, 0x0` | `cmplwi cr6, r17, 0x0` |  |
| 583 | `beq cr6, 0x1358` | `beq cr6, 0x135c` | diff_arg |
| 584 | `lbz r11, 0x0, r18` | `lbz r11, 0x0, r18` |  |
| 585 | `subi r17, r17, 0x1` | `subi r17, r17, 0x1` |  |
| 586 | `addi r18, r18, 0x1` | `addi r18, r18, 0x1` |  |
| 587 | `slw r11, r11, r30` | `slw r11, r11, r30` |  |
| 588 | `addi r30, r30, 0x8` | `addi r30, r30, 0x8` |  |
| 589 | `add r29, r11, r29` | `add r29, r11, r29` |  |
| 590 | `cmplwi cr6, r30, 0x3` | `cmplwi cr6, r30, 0x3` |  |
| 591 | `blt cr6, 0x918` | `blt cr6, 0x918` |  |
| 592 | `lwz r11, 0x60, r31` | `lwz r11, 0x60, r31` |  |
| 593 | `clrlwi r10, r29, 29` | `clrlwi r10, r29, 29` |  |
| 594 | `lwz r9, 0x60, r1` | `lwz r9, 0x60, r1` |  |
| 595 | `srwi r29, r29, 3` | `srwi r29, r29, 3` |  |
| 596 | `slwi r11, r11, 1` | `slwi r11, r11, 1` |  |
| 597 | `addi r9, r9, 0x880` | `addi r9, r9, 0x880` |  |
| 598 | `subi r30, r30, 0x3` | `subi r30, r30, 0x3` |  |
| 599 | `lhzx r11, r11, r9` | `lhzx r11, r11, r9` |  |
| 600 | `addi r11, r11, 0x34` | `addi r11, r11, 0x34` |  |
| 601 | `slwi r11, r11, 1` | `slwi r11, r11, 1` |  |
| 602 | `sthx r10, r11, r31` | `sthx r10, r11, r31` |  |
| 603 | `lwz r11, 0x60, r31` | `lwz r11, 0x60, r31` |  |
| 604 | `addi r11, r11, 0x1` | `addi r11, r11, 0x1` |  |
| 605 | `stw r11, 0x60, r31` | `stw r11, 0x60, r31` |  |
| 606 | `lwz r10, 0x54, r31` | `lwz r10, 0x54, r31` |  |
| 607 | `lwz r11, 0x60, r31` | `lwz r11, 0x60, r31` |  |
| 608 | `cmplw cr6, r11, r10` | `cmplw cr6, r11, r10` |  |
| 609 | `blt cr6, 0x938` | `blt cr6, 0x938` |  |
| 610 | `b 0x9b8` | `b 0x9b8` |  |
| 611 | `lwz r11, 0x60, r31` | `lwz r11, 0x60, r31` |  |
| 612 | `lwz r10, 0x60, r1` | `lwz r10, 0x60, r1` |  |
| 613 | `slwi r11, r11, 1` | `slwi r11, r11, 1` |  |
| 614 | `addi r10, r10, 0x880` | `addi r10, r10, 0x880` |  |
| 615 | `lhzx r11, r11, r10` | `lhzx r11, r11, r10` |  |
| 616 | `addi r11, r11, 0x34` | `addi r11, r11, 0x34` |  |
| 617 | `slwi r11, r11, 1` | `slwi r11, r11, 1` |  |
| 618 | `sthx r23, r11, r31` | `sthx r23, r11, r31` |  |
| 619 | `lwz r11, 0x60, r31` | `lwz r11, 0x60, r31` |  |
| 620 | `addi r11, r11, 0x1` | `addi r11, r11, 0x1` |  |
| 621 | `stw r11, 0x60, r31` | `stw r11, 0x60, r31` |  |
| 622 | `lwz r11, 0x60, r31` | `lwz r11, 0x60, r31` |  |
| 623 | `cmplwi cr6, r11, 0x13` | `cmplwi cr6, r11, 0x13` |  |
| 624 | `blt cr6, 0x98c` | `blt cr6, 0x98c` |  |
| 625 | `addi r11, r31, 0x528` | `addi r11, r31, 0x528` |  |
| 626 | `li r10, 0x7` | `li r10, 0x7` |  |
| 627 | `stw r11, 0x44, r31` | `stw r11, 0x44, r31` |  |
| 628 | `addi r6, r31, 0x64` | `addi r6, r31, 0x64` |  |
| 629 | `stw r11, 0x64, r31` | `stw r11, 0x64, r31` |  |
| 630 | `addi r7, r31, 0x4c` | `addi r7, r31, 0x4c` |  |
| 631 | `stw r10, 0x4c, r31` | `stw r10, 0x4c, r31` |  |
| 632 | `addi r8, r31, 0x2e8` | `addi r8, r31, 0x2e8` |  |
| 633 | `li r5, 0x13` | `li r5, 0x13` |  |
| 634 | `addi r4, r31, 0x68` | `addi r4, r31, 0x68` |  |
| 635 | `li r3, 0x0` | `li r3, 0x0` |  |
| 636 | `bl inflate_table` | `bl inflate_table` |  |
| 637 | `stw r3, 0x5c, r1` | `stw r3, 0x5c, r1` |  |
| 638 | `cmpwi r3, 0x0` | `cmpwi r3, 0x0` |  |
| 639 | `beq 0xa08` | `beq 0xa08` |  |
| 640 | `stw r26, 0x18, r16` | `stw r26, 0x18, r16` |  |
| 641 | `b 0x12fc` | `b 0x1300` |  |
| 642 | `li r11, 0x11` | `li r11, 0x11` |  |
| 643 | `stw r23, 0x60, r31` | `stw r23, 0x60, r31` |  |
| 644 | `stw r11, 0x0, r31` | `stw r11, 0x0, r31` |  |
| 645 | `b 0xc38` | `b 0xc38` |  |
| 646 | `lwz r11, 0x4c, r31` | `lwz r11, 0x4c, r31` |  |
| 647 | `lwz r10, 0x44, r31` | `lwz r10, 0x44, r31` |  |
| 648 | `slw r11, r15, r11` | `slw r11, r15, r11` |  |
| 649 | `subi r11, r11, 0x1` | `subi r11, r11, 0x1` |  |
| 650 | `and r11, r11, r29` | `and r11, r11, r29` |  |
| 651 | `slwi r11, r11, 2` | `slwi r11, r11, 2` |  |
| 652 | `lwzx r11, r11, r10` | `lwzx r11, r11, r10` |  |
| 653 | `b 0xa74` | `b 0xa74` |  |
| 654 | `cmplwi cr6, r17, 0x0` | `cmplwi cr6, r17, 0x0` |  |
| 655 | `beq cr6, 0x1358` | `beq cr6, 0x135c` | diff_arg |
| 656 | `lbz r11, 0x0, r18` | `lbz r11, 0x0, r18` |  |
| 657 | `subi r17, r17, 0x1` | `subi r17, r17, 0x1` |  |
| 658 | `lwz r9, 0x4c, r31` | `lwz r9, 0x4c, r31` |  |
| 659 | `addi r18, r18, 0x1` | `addi r18, r18, 0x1` |  |
| 660 | `slw r10, r11, r30` | `slw r10, r11, r30` |  |
| 661 | `lwz r8, 0x44, r31` | `lwz r8, 0x44, r31` |  |
| 662 | `slw r11, r15, r9` | `slw r11, r15, r9` |  |
| 663 | `subi r11, r11, 0x1` | `subi r11, r11, 0x1` |  |
| 664 | `add r29, r10, r29` | `add r29, r10, r29` |  |
| 665 | `addi r30, r30, 0x8` | `addi r30, r30, 0x8` |  |
| 666 | `and r11, r11, r29` | `and r11, r11, r29` |  |
| 667 | `slwi r11, r11, 2` | `slwi r11, r11, 2` |  |
| 668 | `lwzx r11, r11, r8` | `lwzx r11, r11, r8` |  |
| 669 | `stw r11, 0x50, r1` | `stw r11, 0x50, r1` |  |
| 670 | `lbz r11, 0x51, r1` | `lbz r11, 0x51, r1` |  |
| 671 | `cmplw cr6, r11, r30` | `cmplw cr6, r11, r30` |  |
| 672 | `bgt cr6, 0xa38` | `bgt cr6, 0xa38` |  |
| 673 | `lhz r9, 0x52, r1` | `lhz r9, 0x52, r1` |  |
| 674 | `mr r10, r9` | `mr r10, r9` |  |
| 675 | `cmplwi cr6, r9, 0x10` | `cmplwi cr6, r9, 0x10` |  |
| 676 | `blt cr6, 0xac0` | `blt cr6, 0xac0` |  |
| 677 | `bne cr6, 0xb4c` | `bne cr6, 0xb4c` |  |
| 678 | `addi r9, r11, 0x2` | `addi r9, r11, 0x2` |  |
| 679 | `b 0xb10` | `b 0xb10` |  |
| 680 | `cmplwi cr6, r17, 0x0` | `cmplwi cr6, r17, 0x0` |  |
| 681 | `beq cr6, 0x1358` | `beq cr6, 0x135c` | diff_arg |
| 682 | `lbz r10, 0x0, r18` | `lbz r10, 0x0, r18` |  |
| 683 | `subi r17, r17, 0x1` | `subi r17, r17, 0x1` |  |
| 684 | `addi r18, r18, 0x1` | `addi r18, r18, 0x1` |  |
| 685 | `slw r10, r10, r30` | `slw r10, r10, r30` |  |
| 686 | `addi r30, r30, 0x8` | `addi r30, r30, 0x8` |  |
| 687 | `add r29, r10, r29` | `add r29, r10, r29` |  |
| 688 | `cmplw cr6, r30, r11` | `cmplw cr6, r30, r11` |  |
| 689 | `blt cr6, 0xaa0` | `blt cr6, 0xaa0` |  |
| 690 | `lwz r10, 0x60, r31` | `lwz r10, 0x60, r31` |  |
| 691 | `srw r29, r29, r11` | `srw r29, r29, r11` |  |
| 692 | `addi r10, r10, 0x34` | `addi r10, r10, 0x34` |  |
| 693 | `subf r30, r11, r30` | `subf r30, r11, r30` |  |
| 694 | `slwi r11, r10, 1` | `slwi r11, r10, 1` |  |
| 695 | `sthx r9, r11, r31` | `sthx r9, r11, r31` |  |
| 696 | `lwz r11, 0x60, r31` | `lwz r11, 0x60, r31` |  |
| 697 | `addi r11, r11, 0x1` | `addi r11, r11, 0x1` |  |
| 698 | `stw r11, 0x60, r31` | `stw r11, 0x60, r31` |  |
| 699 | `b 0xc38` | `b 0xc38` |  |
| 700 | `cmplwi cr6, r17, 0x0` | `cmplwi cr6, r17, 0x0` |  |
| 701 | `beq cr6, 0x1358` | `beq cr6, 0x135c` | diff_arg |
| 702 | `lbz r10, 0x0, r18` | `lbz r10, 0x0, r18` |  |
| 703 | `subi r17, r17, 0x1` | `subi r17, r17, 0x1` |  |
| 704 | `addi r18, r18, 0x1` | `addi r18, r18, 0x1` |  |
| 705 | `slw r10, r10, r30` | `slw r10, r10, r30` |  |
| 706 | `addi r30, r30, 0x8` | `addi r30, r30, 0x8` |  |
| 707 | `add r29, r10, r29` | `add r29, r10, r29` |  |
| 708 | `cmplw cr6, r30, r9` | `cmplw cr6, r30, r9` |  |
| 709 | `blt cr6, 0xaf0` | `blt cr6, 0xaf0` |  |
| 710 | `lwz r10, 0x60, r31` | `lwz r10, 0x60, r31` |  |
| 711 | `srw r29, r29, r11` | `srw r29, r29, r11` |  |
| 712 | `subf r30, r11, r30` | `subf r30, r11, r30` |  |
| 713 | `cmplwi cr6, r10, 0x0` | `cmplwi cr6, r10, 0x0` |  |
| 714 | `beq cr6, 0xc54` | `beq cr6, 0xc54` |  |
| 715 | `addi r10, r10, 0x33` | `addi r10, r10, 0x33` |  |
| 716 | `clrlwi r11, r29, 30` | `clrlwi r11, r29, 30` |  |
| 717 | `slwi r10, r10, 1` | `slwi r10, r10, 1` |  |
| 718 | `addi r11, r11, 0x3` | `addi r11, r11, 0x3` |  |
| 719 | `srwi r29, r29, 2` | `srwi r29, r29, 2` |  |
| 720 | `subi r30, r30, 0x2` | `subi r30, r30, 0x2` |  |
| 721 | `lhzx r10, r10, r31` | `lhzx r10, r10, r31` |  |
| 722 | `b 0xbec` | `b 0xbec` |  |
| 723 | `cmplwi cr6, r10, 0x11` | `cmplwi cr6, r10, 0x11` |  |
| 724 | `bne cr6, 0xba0` | `bne cr6, 0xba0` |  |
| 725 | `addi r9, r11, 0x3` | `addi r9, r11, 0x3` |  |
| 726 | `b 0xb7c` | `b 0xb7c` |  |
| 727 | `cmplwi cr6, r17, 0x0` | `cmplwi cr6, r17, 0x0` |  |
| 728 | `beq cr6, 0x1358` | `beq cr6, 0x135c` | diff_arg |
| 729 | `lbz r10, 0x0, r18` | `lbz r10, 0x0, r18` |  |
| 730 | `subi r17, r17, 0x1` | `subi r17, r17, 0x1` |  |
| 731 | `addi r18, r18, 0x1` | `addi r18, r18, 0x1` |  |
| 732 | `slw r10, r10, r30` | `slw r10, r10, r30` |  |
| 733 | `addi r30, r30, 0x8` | `addi r30, r30, 0x8` |  |
| 734 | `add r29, r10, r29` | `add r29, r10, r29` |  |
| 735 | `cmplw cr6, r30, r9` | `cmplw cr6, r30, r9` |  |
| 736 | `blt cr6, 0xb5c` | `blt cr6, 0xb5c` |  |
| 737 | `srw r7, r29, r11` | `srw r7, r29, r11` |  |
| 738 | `subf r9, r11, r30` | `subf r9, r11, r30` |  |
| 739 | `clrlwi r8, r7, 29` | `clrlwi r8, r7, 29` |  |
| 740 | `srwi r29, r7, 3` | `srwi r29, r7, 3` |  |
| 741 | `addi r11, r8, 0x3` | `addi r11, r8, 0x3` |  |
| 742 | `subi r30, r9, 0x3` | `subi r30, r9, 0x3` |  |
| 743 | `b 0xbe8` | `b 0xbe8` |  |
| 744 | `addi r9, r11, 0x7` | `addi r9, r11, 0x7` |  |
| 745 | `b 0xbc8` | `b 0xbc8` |  |
| 746 | `cmplwi cr6, r17, 0x0` | `cmplwi cr6, r17, 0x0` |  |
| 747 | `beq cr6, 0x1358` | `beq cr6, 0x135c` | diff_arg |
| 748 | `lbz r10, 0x0, r18` | `lbz r10, 0x0, r18` |  |
| 749 | `subi r17, r17, 0x1` | `subi r17, r17, 0x1` |  |
| 750 | `addi r18, r18, 0x1` | `addi r18, r18, 0x1` |  |
| 751 | `slw r10, r10, r30` | `slw r10, r10, r30` |  |
| 752 | `addi r30, r30, 0x8` | `addi r30, r30, 0x8` |  |
| 753 | `add r29, r10, r29` | `add r29, r10, r29` |  |
| 754 | `cmplw cr6, r30, r9` | `cmplw cr6, r30, r9` |  |
| 755 | `blt cr6, 0xba8` | `blt cr6, 0xba8` |  |
| 756 | `srw r7, r29, r11` | `srw r7, r29, r11` |  |
| 757 | `subf r9, r11, r30` | `subf r9, r11, r30` |  |
| 758 | `clrlwi r8, r7, 25` | `clrlwi r8, r7, 25` |  |
| 759 | `srwi r29, r7, 7` | `srwi r29, r7, 7` |  |
| 760 | `addi r11, r8, 0xb` | `addi r11, r8, 0xb` |  |
| 761 | `subi r30, r9, 0x7` | `subi r30, r9, 0x7` |  |
| 762 | `mr r10, r23` | `mr r10, r23` |  |
| 763 | `lwz r7, 0x60, r31` | `lwz r7, 0x60, r31` |  |
| 764 | `lwz r9, 0x5c, r31` | `lwz r9, 0x5c, r31` |  |
| 765 | `lwz r8, 0x58, r31` | `lwz r8, 0x58, r31` |  |
| 766 | `add r7, r7, r11` | `add r7, r7, r11` |  |
| 767 | `add r9, r9, r8` | `add r9, r9, r8` |  |
| 768 | `cmplw cr6, r7, r9` | `cmplw cr6, r7, r9` |  |
| 769 | `bgt cr6, 0xc54` | `bgt cr6, 0xc54` |  |
| 770 | `cmplwi cr6, r11, 0x0` | `cmplwi cr6, r11, 0x0` |  |
| 771 | `beq cr6, 0xc38` | `beq cr6, 0xc38` |  |
| 772 | `clrlwi r10, r10, 16` | `clrlwi r10, r10, 16` |  |
| 773 | `mtctr r11` | `mtctr r11` |  |
| 774 | `lwz r11, 0x60, r31` | `lwz r11, 0x60, r31` |  |
| 775 | `addi r11, r11, 0x34` | `addi r11, r11, 0x34` |  |
| 776 | `slwi r11, r11, 1` | `slwi r11, r11, 1` |  |
| 777 | `sthx r10, r11, r31` | `sthx r10, r11, r31` |  |
| 778 | `lwz r11, 0x60, r31` | `lwz r11, 0x60, r31` |  |
| 779 | `addi r11, r11, 0x1` | `addi r11, r11, 0x1` |  |
| 780 | `stw r11, 0x60, r31` | `stw r11, 0x60, r31` |  |
| 781 | `bdnz 0xc18` | `bdnz 0xc18` |  |
| 782 | `lwz r10, 0x58, r31` | `lwz r10, 0x58, r31` |  |
| 783 | `lwz r11, 0x5c, r31` | `lwz r11, 0x5c, r31` |  |
| 784 | `lwz r9, 0x60, r31` | `lwz r9, 0x60, r31` |  |
| 785 | `add r11, r11, r10` | `add r11, r11, r10` |  |
| 786 | `cmplw cr6, r9, r11` | `cmplw cr6, r9, r11` |  |
| 787 | `blt cr6, 0xa18` | `blt cr6, 0xa18` |  |
| 788 | `b 0xc60` | `b 0xc60` |  |
| 789 | `lwz r11, 0x64, r1` | `lwz r11, 0x64, r1` |  |
| 790 | `stw r11, 0x18, r16` | `stw r11, 0x18, r16` |  |
| 791 | `stw r14, 0x0, r31` | `stw r14, 0x0, r31` |  |
| 792 | `addi r11, r31, 0x528` | `addi r11, r31, 0x528` |  |
| 793 | `lwz r5, 0x58, r31` | `lwz r5, 0x58, r31` |  |
| 794 | `li r10, 0x9` | `li r10, 0x9` |  |
| 795 | `stw r11, 0x44, r31` | `stw r11, 0x44, r31` |  |
| 796 | `addi r28, r31, 0x64` | `addi r28, r31, 0x64` |  |
| 797 | `stw r11, 0x64, r31` | `stw r11, 0x64, r31` |  |
| 798 | `addi r27, r31, 0x2e8` | `addi r27, r31, 0x2e8` |  |
| 799 | `stw r10, 0x4c, r31` | `stw r10, 0x4c, r31` |  |
| 800 | `addi r7, r31, 0x4c` | `addi r7, r31, 0x4c` |  |
| 801 | `mr r6, r28` | `mr r6, r28` |  |
| 802 | `mr r8, r27` | `mr r8, r27` |  |
| 803 | `addi r4, r31, 0x68` | `addi r4, r31, 0x68` |  |
| 804 | `li r3, 0x1` | `li r3, 0x1` |  |
| 805 | `bl inflate_table` | `bl inflate_table` |  |
| 806 | `stw r3, 0x5c, r1` | `stw r3, 0x5c, r1` |  |
| 807 | `cmpwi r3, 0x0` | `cmpwi r3, 0x0` |  |
| 808 | `beq 0xcac` | `beq 0xcac` |  |
| 809 | `lwz r11, 0x80, r1` | `lwz r11, 0x80, r1` |  |
| 810 | `b 0x12f8` | `b 0x12fc` |  |
| 811 | `lwz r10, 0x0, r28` | `lwz r10, 0x0, r28` |  |
| 812 | `li r9, 0x6` | `li r9, 0x6` |  |
| 813 | `lwz r11, 0x58, r31` | `lwz r11, 0x58, r31` |  |
| 814 | `addi r7, r31, 0x50` | `addi r7, r31, 0x50` |  |
| 815 | `stw r9, 0x50, r31` | `stw r9, 0x50, r31` |  |
| 816 | `mr r8, r27` | `mr r8, r27` |  |
| 817 | `addi r11, r11, 0x34` | `addi r11, r11, 0x34` |  |
| 818 | `lwz r5, 0x5c, r31` | `lwz r5, 0x5c, r31` |  |
| 819 | `mr r6, r28` | `mr r6, r28` |  |
| 820 | `stw r10, 0x48, r31` | `stw r10, 0x48, r31` |  |
| 821 | `slwi r11, r11, 1` | `slwi r11, r11, 1` |  |
| 822 | `li r3, 0x2` | `li r3, 0x2` |  |
| 823 | `add r4, r11, r31` | `add r4, r11, r31` |  |
| 824 | `bl inflate_table` | `bl inflate_table` |  |
| 825 | `stw r3, 0x5c, r1` | `stw r3, 0x5c, r1` |  |
| 826 | `cmpwi r3, 0x0` | `cmpwi r3, 0x0` |  |
| 827 | `beq 0xcf8` | `beq 0xcf8` |  |
| 828 | `lwz r11, 0x84, r1` | `lwz r11, 0x84, r1` |  |
| 829 | `b 0x12f8` | `b 0x12fc` |  |
| 830 | `li r11, 0x12` | `li r11, 0x12` |  |
| 831 | `stw r11, 0x0, r31` | `stw r11, 0x0, r31` |  |
| 832 | `cmplwi cr6, r17, 0x6` | `cmplwi cr6, r17, 0x6` |  |
| 833 | `blt cr6, 0xd50` | `blt cr6, 0xd50` |  |
| 834 | `cmplwi cr6, r25, 0x102` | `cmplwi cr6, r25, 0x102` |  |
| 835 | `blt cr6, 0xd50` | `blt cr6, 0xd50` |  |
| 836 | `stw r24, 0xc, r16` | `stw r24, 0xc, r16` |  |
| 837 | `mr r4, r22` | `mr r4, r22` |  |
| 838 | `stw r25, 0x10, r16` | `stw r25, 0x10, r16` |  |
| 839 | `mr r3, r16` | `mr r3, r16` |  |
| 840 | `stw r18, 0x0, r16` | `stw r18, 0x0, r16` |  |
| 841 | `stw r17, 0x4, r16` | `stw r17, 0x4, r16` |  |
| 842 | `stw r29, 0x30, r31` | `stw r29, 0x30, r31` |  |
| 843 | `stw r30, 0x34, r31` | `stw r30, 0x34, r31` |  |
| 844 | `bl inflate_fast` | `bl inflate_fast` |  |
| 845 | `lwz r24, 0xc, r16` | `lwz r24, 0xc, r16` |  |
| 846 | `lwz r25, 0x10, r16` | `lwz r25, 0x10, r16` |  |
| 847 | `lwz r18, 0x0, r16` | `lwz r18, 0x0, r16` |  |
| 848 | `lwz r17, 0x4, r16` | `lwz r17, 0x4, r16` |  |
| 849 | `lwz r29, 0x30, r31` | `lwz r29, 0x30, r31` |  |
| 850 | `lwz r30, 0x34, r31` | `lwz r30, 0x34, r31` |  |
| 851 | `b 0x1300` | `b 0x1304` |  |
| 852 | `lwz r11, 0x4c, r31` | `lwz r11, 0x4c, r31` |  |
| 853 | `lwz r8, 0x44, r31` | `lwz r8, 0x44, r31` |  |
| 854 | `slw r11, r15, r11` | `slw r11, r15, r11` |  |
| 855 | `subi r11, r11, 0x1` | `subi r11, r11, 0x1` |  |
| 856 | `and r11, r11, r29` | `and r11, r11, r29` |  |
| 857 | `slwi r11, r11, 2` | `slwi r11, r11, 2` |  |
| 858 | `lwzx r10, r11, r8` | `lwzx r10, r11, r8` |  |
| 859 | `b 0xdac` | `b 0xdac` |  |
| 860 | `cmplwi cr6, r17, 0x0` | `cmplwi cr6, r17, 0x0` |  |
| 861 | `beq cr6, 0x1358` | `beq cr6, 0x135c` | diff_arg |
| 862 | `lbz r11, 0x0, r18` | `lbz r11, 0x0, r18` |  |
| 863 | `subi r17, r17, 0x1` | `subi r17, r17, 0x1` |  |
| 864 | `lwz r9, 0x4c, r31` | `lwz r9, 0x4c, r31` |  |
| 865 | `addi r18, r18, 0x1` | `addi r18, r18, 0x1` |  |
| 866 | `slw r10, r11, r30` | `slw r10, r11, r30` |  |
| 867 | `lwz r7, 0x44, r31` | `lwz r7, 0x44, r31` |  |
| 868 | `slw r11, r15, r9` | `slw r11, r15, r9` |  |
| 869 | `add r29, r10, r29` | `add r29, r10, r29` |  |
| 870 | `subi r11, r11, 0x1` | `subi r11, r11, 0x1` |  |
| 871 | `addi r30, r30, 0x8` | `addi r30, r30, 0x8` |  |
| 872 | `and r11, r11, r29` | `and r11, r11, r29` |  |
| 873 | `slwi r11, r11, 2` | `slwi r11, r11, 2` |  |
| 874 | `lwzx r10, r11, r7` | `lwzx r10, r11, r7` |  |
| 875 | `stw r10, 0x50, r1` | `stw r10, 0x50, r1` |  |
| 876 | `lbz r11, 0x51, r1` | `lbz r11, 0x51, r1` |  |
| 877 | `cmplw cr6, r11, r30` | `cmplw cr6, r11, r30` |  |
| 878 | `bgt cr6, 0xd70` | `bgt cr6, 0xd70` |  |
| 879 | `lbz r9, 0x50, r1` | `lbz r9, 0x50, r1` |  |
| 880 | `cmplwi r9, 0x0` | `cmplwi r9, 0x0` |  |
| 881 | `beq 0xe70` | `beq 0xe70` |  |
| 882 | `rlwinm. r7, r9, 0, 24, 27` | `rlwinm. r7, r9, 0, 24, 27` |  |
| 883 | `bne 0xe70` | `bne 0xe70` |  |
| 884 | `add r9, r11, r9` | `add r9, r11, r9` |  |
| 885 | `stw r10, 0x58, r1` | `stw r10, 0x58, r1` |  |
| 886 | `lhz r7, 0x52, r1` | `lhz r7, 0x52, r1` |  |
| 887 | `slw r9, r15, r9` | `slw r9, r15, r9` |  |
| 888 | `subi r9, r9, 0x1` | `subi r9, r9, 0x1` |  |
| 889 | `mr r10, r7` | `mr r10, r7` |  |
| 890 | `and r9, r9, r29` | `and r9, r9, r29` |  |
| 891 | `lbz r7, 0x59, r1` | `lbz r7, 0x59, r1` |  |
| 892 | `srw r11, r9, r11` | `srw r11, r9, r11` |  |
| 893 | `add r11, r11, r10` | `add r11, r11, r10` |  |
| 894 | `mr r10, r7` | `mr r10, r7` |  |
| 895 | `slwi r11, r11, 2` | `slwi r11, r11, 2` |  |
| 896 | `lwzx r11, r11, r8` | `lwzx r11, r11, r8` |  |
| 897 | `b 0xe54` | `b 0xe54` |  |
| 898 | `cmplwi cr6, r17, 0x0` | `cmplwi cr6, r17, 0x0` |  |
| 899 | `beq cr6, 0x1358` | `beq cr6, 0x135c` | diff_arg |
| 900 | `lbz r11, 0x58, r1` | `lbz r11, 0x58, r1` |  |
| 901 | `subi r17, r17, 0x1` | `subi r17, r17, 0x1` |  |
| 902 | `lbz r9, 0x0, r18` | `lbz r9, 0x0, r18` |  |
| 903 | `addi r18, r18, 0x1` | `addi r18, r18, 0x1` |  |
| 904 | `add r11, r11, r10` | `add r11, r11, r10` |  |
| 905 | `lhz r8, 0x5a, r1` | `lhz r8, 0x5a, r1` |  |
| 906 | `slw r9, r9, r30` | `slw r9, r9, r30` |  |
| 907 | `lwz r7, 0x44, r31` | `lwz r7, 0x44, r31` |  |
| 908 | `slw r11, r15, r11` | `slw r11, r15, r11` |  |
| 909 | `subi r11, r11, 0x1` | `subi r11, r11, 0x1` |  |
| 910 | `add r29, r9, r29` | `add r29, r9, r29` |  |
| 911 | `addi r30, r30, 0x8` | `addi r30, r30, 0x8` |  |
| 912 | `and r11, r11, r29` | `and r11, r11, r29` |  |
| 913 | `srw r11, r11, r10` | `srw r11, r11, r10` |  |
| 914 | `add r11, r11, r8` | `add r11, r11, r8` |  |
| 915 | `slwi r11, r11, 2` | `slwi r11, r11, 2` |  |
| 916 | `lwzx r11, r11, r7` | `lwzx r11, r11, r7` |  |
| 917 | `stw r11, 0x50, r1` | `stw r11, 0x50, r1` |  |
| 918 | `lbz r11, 0x51, r1` | `lbz r11, 0x51, r1` |  |
| 919 | `add r9, r11, r10` | `add r9, r11, r10` |  |
| 920 | `cmplw cr6, r9, r30` | `cmplw cr6, r9, r30` |  |
| 921 | `bgt cr6, 0xe08` | `bgt cr6, 0xe08` |  |
| 922 | `srw r29, r29, r10` | `srw r29, r29, r10` |  |
| 923 | `subf r30, r10, r30` | `subf r30, r10, r30` |  |
| 924 | `lhz r8, 0x52, r1` | `lhz r8, 0x52, r1` |  |
| 925 | `srw r29, r29, r11` | `srw r29, r29, r11` |  |
| 926 | `lbz r10, 0x50, r1` | `lbz r10, 0x50, r1` |  |
| 927 | `subf r30, r11, r30` | `subf r30, r11, r30` |  |
| 928 | `stw r8, 0x38, r31` | `stw r8, 0x38, r31` |  |
| 929 | `cmplwi r10, 0x0` | `cmplwi r10, 0x0` |  |
| 930 | `bne 0xe94` | `bne 0xe94` |  |
| 931 | `li r11, 0x17` | `li r11, 0x17` |  |
| 932 | `b 0x1b8` | `b 0x1b8` |  |
| 933 | `rlwinm. r11, r10, 0, 26, 26` | `rlwinm. r11, r10, 0, 26, 26` |  |
| 934 | `bne 0x884` | `bne 0x884` |  |
| 935 | `rlwinm. r11, r10, 0, 25, 25` | `rlwinm. r11, r10, 0, 25, 25` |  |
| 936 | `beq 0xeac` | `beq 0xeac` |  |
| 937 | `lwz r11, 0x88, r1` | `lwz r11, 0x88, r1` |  |
| 938 | `b 0x12f8` | `b 0x12fc` |  |
| 939 | `clrlwi r11, r10, 28` | `clrlwi r11, r10, 28` |  |
| 940 | `li r10, 0x13` | `li r10, 0x13` |  |
| 941 | `stw r11, 0x40, r31` | `stw r11, 0x40, r31` |  |
| 942 | `stw r10, 0x0, r31` | `stw r10, 0x0, r31` |  |
| 943 | `lwz r11, 0x40, r31` | `lwz r11, 0x40, r31` |  |
| 944 | `cmplwi cr6, r11, 0x0` | `cmplwi cr6, r11, 0x0` |  |
| 945 | `beq cr6, 0xf1c` | `beq cr6, 0xf1c` |  |
| 946 | `cmplw cr6, r30, r11` | `cmplw cr6, r30, r11` |  |
| 947 | `bge cr6, 0xefc` | `bge cr6, 0xefc` |  |
| 948 | `cmplwi cr6, r17, 0x0` | `cmplwi cr6, r17, 0x0` |  |
| 949 | `beq cr6, 0x1358` | `beq cr6, 0x135c` | diff_arg |
| 950 | `lbz r10, 0x0, r18` | `lbz r10, 0x0, r18` |  |
| 951 | `subi r17, r17, 0x1` | `subi r17, r17, 0x1` |  |
| 952 | `lwz r9, 0x40, r31` | `lwz r9, 0x40, r31` |  |
| 953 | `addi r18, r18, 0x1` | `addi r18, r18, 0x1` |  |
| 954 | `slw r10, r10, r30` | `slw r10, r10, r30` |  |
| 955 | `addi r30, r30, 0x8` | `addi r30, r30, 0x8` |  |
| 956 | `add r29, r10, r29` | `add r29, r10, r29` |  |
| 957 | `cmplw cr6, r30, r9` | `cmplw cr6, r30, r9` |  |
| 958 | `blt cr6, 0xed0` | `blt cr6, 0xed0` |  |
| 959 | `slw r10, r15, r11` | `slw r10, r15, r11` |  |
| 960 | `lwz r9, 0x38, r31` | `lwz r9, 0x38, r31` |  |
| 961 | `subi r10, r10, 0x1` | `subi r10, r10, 0x1` |  |
| 962 | `subf r30, r11, r30` | `subf r30, r11, r30` |  |
| 963 | `and r10, r10, r29` | `and r10, r10, r29` |  |
| 964 | `srw r29, r29, r11` | `srw r29, r29, r11` |  |
| 965 | `add r11, r10, r9` | `add r11, r10, r9` |  |
| 966 | `stw r11, 0x38, r31` | `stw r11, 0x38, r31` |  |
| 967 | `li r11, 0x14` | `li r11, 0x14` |  |
| 968 | `stw r11, 0x0, r31` | `stw r11, 0x0, r31` |  |
| 969 | `lwz r11, 0x50, r31` | `lwz r11, 0x50, r31` |  |
| 970 | `lwz r8, 0x48, r31` | `lwz r8, 0x48, r31` |  |
| 971 | `slw r11, r15, r11` | `slw r11, r15, r11` |  |
| 972 | `subi r11, r11, 0x1` | `subi r11, r11, 0x1` |  |
| 973 | `and r11, r11, r29` | `and r11, r11, r29` |  |
| 974 | `slwi r11, r11, 2` | `slwi r11, r11, 2` |  |
| 975 | `lwzx r10, r11, r8` | `lwzx r10, r11, r8` |  |
| 976 | `b 0xf80` | `b 0xf80` |  |
| 977 | `cmplwi cr6, r17, 0x0` | `cmplwi cr6, r17, 0x0` |  |
| 978 | `beq cr6, 0x1358` | `beq cr6, 0x135c` | diff_arg |
| 979 | `lbz r11, 0x0, r18` | `lbz r11, 0x0, r18` |  |
| 980 | `subi r17, r17, 0x1` | `subi r17, r17, 0x1` |  |
| 981 | `lwz r9, 0x50, r31` | `lwz r9, 0x50, r31` |  |
| 982 | `addi r18, r18, 0x1` | `addi r18, r18, 0x1` |  |
| 983 | `slw r10, r11, r30` | `slw r10, r11, r30` |  |
| 984 | `lwz r7, 0x48, r31` | `lwz r7, 0x48, r31` |  |
| 985 | `slw r11, r15, r9` | `slw r11, r15, r9` |  |
| 986 | `add r29, r10, r29` | `add r29, r10, r29` |  |
| 987 | `subi r11, r11, 0x1` | `subi r11, r11, 0x1` |  |
| 988 | `addi r30, r30, 0x8` | `addi r30, r30, 0x8` |  |
| 989 | `and r11, r11, r29` | `and r11, r11, r29` |  |
| 990 | `slwi r11, r11, 2` | `slwi r11, r11, 2` |  |
| 991 | `lwzx r10, r11, r7` | `lwzx r10, r11, r7` |  |
| 992 | `stw r10, 0x50, r1` | `stw r10, 0x50, r1` |  |
| 993 | `lbz r11, 0x51, r1` | `lbz r11, 0x51, r1` |  |
| 994 | `cmplw cr6, r11, r30` | `cmplw cr6, r11, r30` |  |
| 995 | `bgt cr6, 0xf44` | `bgt cr6, 0xf44` |  |
| 996 | `lbz r9, 0x50, r1` | `lbz r9, 0x50, r1` |  |
| 997 | `clrrwi. r6, r9, 4` | `clrrwi. r6, r9, 4` |  |
| 998 | `mr r7, r9` | `mr r7, r9` |  |
| 999 | `bne 0x1044` | `bne 0x1044` |  |
| 1000 | `add r9, r11, r9` | `add r9, r11, r9` |  |
| 1001 | `stw r10, 0x58, r1` | `stw r10, 0x58, r1` |  |
| 1002 | `lhz r7, 0x52, r1` | `lhz r7, 0x52, r1` |  |
| 1003 | `slw r9, r15, r9` | `slw r9, r15, r9` |  |
| 1004 | `subi r9, r9, 0x1` | `subi r9, r9, 0x1` |  |
| 1005 | `mr r10, r7` | `mr r10, r7` |  |
| 1006 | `and r9, r9, r29` | `and r9, r9, r29` |  |
| 1007 | `lbz r7, 0x59, r1` | `lbz r7, 0x59, r1` |  |
| 1008 | `srw r11, r9, r11` | `srw r11, r9, r11` |  |
| 1009 | `add r11, r11, r10` | `add r11, r11, r10` |  |
| 1010 | `mr r10, r7` | `mr r10, r7` |  |
| 1011 | `slwi r11, r11, 2` | `slwi r11, r11, 2` |  |
| 1012 | `lwzx r11, r11, r8` | `lwzx r11, r11, r8` |  |
| 1013 | `b 0x1024` | `b 0x1024` |  |
| 1014 | `cmplwi cr6, r17, 0x0` | `cmplwi cr6, r17, 0x0` |  |
| 1015 | `beq cr6, 0x1358` | `beq cr6, 0x135c` | diff_arg |
| 1016 | `lbz r11, 0x58, r1` | `lbz r11, 0x58, r1` |  |
| 1017 | `subi r17, r17, 0x1` | `subi r17, r17, 0x1` |  |
| 1018 | `lbz r9, 0x0, r18` | `lbz r9, 0x0, r18` |  |
| 1019 | `addi r18, r18, 0x1` | `addi r18, r18, 0x1` |  |
| 1020 | `add r11, r11, r10` | `add r11, r11, r10` |  |
| 1021 | `lhz r8, 0x5a, r1` | `lhz r8, 0x5a, r1` |  |
| 1022 | `slw r9, r9, r30` | `slw r9, r9, r30` |  |
| 1023 | `lwz r7, 0x48, r31` | `lwz r7, 0x48, r31` |  |
| 1024 | `slw r11, r15, r11` | `slw r11, r15, r11` |  |
| 1025 | `subi r11, r11, 0x1` | `subi r11, r11, 0x1` |  |
| 1026 | `add r29, r9, r29` | `add r29, r9, r29` |  |
| 1027 | `addi r30, r30, 0x8` | `addi r30, r30, 0x8` |  |
| 1028 | `and r11, r11, r29` | `and r11, r11, r29` |  |
| 1029 | `srw r11, r11, r10` | `srw r11, r11, r10` |  |
| 1030 | `add r11, r11, r8` | `add r11, r11, r8` |  |
| 1031 | `slwi r11, r11, 2` | `slwi r11, r11, 2` |  |
| 1032 | `lwzx r11, r11, r7` | `lwzx r11, r11, r7` |  |
| 1033 | `stw r11, 0x50, r1` | `stw r11, 0x50, r1` |  |
| 1034 | `lbz r11, 0x51, r1` | `lbz r11, 0x51, r1` |  |
| 1035 | `add r9, r11, r10` | `add r9, r11, r10` |  |
| 1036 | `cmplw cr6, r9, r30` | `cmplw cr6, r9, r30` |  |
| 1037 | `bgt cr6, 0xfd8` | `bgt cr6, 0xfd8` |  |
| 1038 | `lbz r9, 0x50, r1` | `lbz r9, 0x50, r1` |  |
| 1039 | `srw r29, r29, r10` | `srw r29, r29, r10` |  |
| 1040 | `subf r30, r10, r30` | `subf r30, r10, r30` |  |
| 1041 | `srw r29, r29, r11` | `srw r29, r29, r11` |  |
| 1042 | `subf r30, r11, r30` | `subf r30, r11, r30` |  |
| 1043 | `rlwinm. r10, r9, 0, 25, 25` | `rlwinm. r10, r9, 0, 25, 25` |  |
| 1044 | `clrlwi r11, r9, 24` | `clrlwi r11, r9, 24` |  |
| 1045 | `beq 0x1060` | `beq 0x1060` |  |
| 1046 | `lwz r11, 0x8c, r1` | `lwz r11, 0x8c, r1` |  |
| 1047 | `b 0x12f8` | `b 0x12fc` |  |
| 1048 | `lhz r10, 0x52, r1` | `lhz r10, 0x52, r1` |  |
| 1049 | `clrlwi r11, r11, 28` | `clrlwi r11, r11, 28` |  |
| 1050 | `li r9, 0x15` | `li r9, 0x15` |  |
| 1051 | `stw r11, 0x40, r31` | `stw r11, 0x40, r31` |  |
| 1052 | `stw r9, 0x0, r31` | `stw r9, 0x0, r31` |  |
| 1053 | `stw r10, 0x3c, r31` | `stw r10, 0x3c, r31` |  |
| 1054 | `lwz r11, 0x40, r31` | `lwz r11, 0x40, r31` |  |
| 1055 | `cmplwi cr6, r11, 0x0` | `cmplwi cr6, r11, 0x0` |  |
| 1056 | `beq cr6, 0x10d8` | `beq cr6, 0x10d8` |  |
| 1057 | `cmplw cr6, r30, r11` | `cmplw cr6, r30, r11` |  |
| 1058 | `bge cr6, 0x10b8` | `bge cr6, 0x10b8` |  |
| 1059 | `cmplwi cr6, r17, 0x0` | `cmplwi cr6, r17, 0x0` |  |
| 1060 | `beq cr6, 0x1358` | `beq cr6, 0x135c` | diff_arg |
| 1061 | `lbz r10, 0x0, r18` | `lbz r10, 0x0, r18` |  |
| 1062 | `subi r17, r17, 0x1` | `subi r17, r17, 0x1` |  |
| 1063 | `lwz r9, 0x40, r31` | `lwz r9, 0x40, r31` |  |
| 1064 | `addi r18, r18, 0x1` | `addi r18, r18, 0x1` |  |
| 1065 | `slw r10, r10, r30` | `slw r10, r10, r30` |  |
| 1066 | `addi r30, r30, 0x8` | `addi r30, r30, 0x8` |  |
| 1067 | `add r29, r10, r29` | `add r29, r10, r29` |  |
| 1068 | `cmplw cr6, r30, r9` | `cmplw cr6, r30, r9` |  |
| 1069 | `blt cr6, 0x108c` | `blt cr6, 0x108c` |  |
| 1070 | `slw r10, r15, r11` | `slw r10, r15, r11` |  |
| 1071 | `lwz r9, 0x3c, r31` | `lwz r9, 0x3c, r31` |  |
| 1072 | `subi r10, r10, 0x1` | `subi r10, r10, 0x1` |  |
| 1073 | `subf r30, r11, r30` | `subf r30, r11, r30` |  |
| 1074 | `and r10, r10, r29` | `and r10, r10, r29` |  |
| 1075 | `srw r29, r29, r11` | `srw r29, r29, r11` |  |
| 1076 | `add r11, r10, r9` | `add r11, r10, r9` |  |
| 1077 | `stw r11, 0x3c, r31` | `stw r11, 0x3c, r31` |  |
| 1078 | `lwz r11, 0x24, r31` | `lwz r11, 0x24, r31` |  |
| 1079 | `lwz r10, 0x3c, r31` | `lwz r10, 0x3c, r31` |  |
| 1080 | `subf r11, r25, r11` | `subf r11, r25, r11` |  |
| 1081 | `add r11, r11, r22` | `add r11, r11, r22` |  |
| 1082 | `cmplw cr6, r10, r11` | `cmplw cr6, r10, r11` |  |
| 1083 | `ble cr6, 0x10f8` | `ble cr6, 0x10f8` |  |
| 1084 | `lwz r11, 0x90, r1` | `lwz r11, 0x90, r1` |  |
| 1085 | `b 0x12f8` | `b 0x12fc` |  |
| 1086 | `li r11, 0x16` | `li r11, 0x16` |  |
| 1087 | `stw r11, 0x0, r31` | `stw r11, 0x0, r31` |  |
| 1088 | `cmplwi cr6, r25, 0x0` | `cmplwi cr6, r25, 0x0` |  |
| 1089 | `beq cr6, 0x1358` | `beq cr6, 0x135c` | diff_arg |
| 1090 | `lwz r11, 0x3c, r31` | `lwz r11, 0x3c, r31` |  |
| 1091 | `subf r9, r25, r22` | `subf r9, r25, r22` |  |
| 1092 | `cmplw cr6, r11, r9` | `cmplw cr6, r11, r9` |  |
| 1093 | `ble cr6, 0x1158` | `ble cr6, 0x1158` |  |
| 1094 | `lwz r10, 0x28, r31` | `lwz r10, 0x28, r31` |  |
| 1095 | `subf r11, r9, r11` | `subf r11, r9, r11` |  |
| 1096 | `lwz r9, 0x2c, r31` | `lwz r9, 0x2c, r31` |  |
| 1097 | `cmplw cr6, r11, r10` | `cmplw cr6, r11, r10` |  |
| 1098 | `ble cr6, 0x1140` | `ble cr6, 0x1140` |  |
| 1099 | `subf r11, r10, r11` | `subf r11, r10, r11` |  |
| 1100 | `lwz r10, 0x20, r31` | `lwz r10, 0x20, r31` |  |
| 1101 | `add r10, r9, r10` | `add r10, r9, r10` |  |
| 1102 | `subf r9, r11, r10` | `subf r9, r11, r10` |  |
| 1103 | `b 0x1148` | `b 0x1148` |  |
| 1104 | `subf r9, r11, r9` | `subf r9, r11, r9` |  |
| 1105 | `add r9, r9, r10` | `add r9, r9, r10` |  |
| 1106 | `lwz r10, 0x38, r31` | `lwz r10, 0x38, r31` |  |
| 1107 | `cmplw cr6, r11, r10` | `cmplw cr6, r11, r10` |  |
| 1108 | `ble cr6, 0x1164` | `ble cr6, 0x1164` |  |
| 1109 | `b 0x1160` | `b 0x1160` |  |
| 1110 | `lwz r10, 0x38, r31` | `lwz r10, 0x38, r31` |  |
| 1111 | `subf r9, r11, r24` | `subf r9, r11, r24` |  |
| 1112 | `mr r11, r10` | `mr r11, r10` |  |
| 1113 | `cmplw cr6, r11, r25` | `cmplw cr6, r11, r25` |  |
| 1114 | `ble cr6, 0x1170` | `ble cr6, 0x1170` |  |
| 1115 | `mr r11, r25` | `mr r11, r25` |  |
| 1116 | `subf r10, r11, r10` | `subf r10, r11, r10` |  |
| 1117 | `subf r25, r11, r25` | `subf r25, r11, r25` |  |
| 1118 | `stw r10, 0x38, r31` | `stw r10, 0x38, r31` |  |
| 1119 | `subf r10, r24, r9` | `subf r10, r24, r9` |  |
| 1120 | `lbzx r9, r10, r24` | `lbzx r9, r10, r24` |  |
| 1121 | `subic. r11, r11, 0x1` | `subic. r11, r11, 0x1` |  |
| 1122 | `stb r9, 0x0, r24` | `stb r9, 0x0, r24` |  |
| 1123 | `addi r24, r24, 0x1` | `addi r24, r24, 0x1` |  |
| 1124 | `bne 0x1180` | `bne 0x1180` |  |
| 1125 | `lwz r11, 0x38, r31` | `lwz r11, 0x38, r31` |  |
| 1126 | `cmplwi cr6, r11, 0x0` | `cmplwi cr6, r11, 0x0` |  |
| 1127 | `bne cr6, 0x1300` | `bne cr6, 0x1304` |  |
| 1128 | `li r11, 0x12` | `li r11, 0x12` |  |
| 1129 | `b 0x1b8` | `b 0x1b8` |  |
| 1130 | `cmplwi cr6, r25, 0x0` | `cmplwi cr6, r25, 0x0` |  |
| 1131 | `beq cr6, 0x1358` | `beq cr6, 0x135c` | diff_arg |
| 1132 | `lwz r11, 0x38, r31` | `lwz r11, 0x38, r31` |  |
| 1133 | `li r10, 0x12` | `li r10, 0x12` |  |
| 1134 | `subi r25, r25, 0x1` | `subi r25, r25, 0x1` |  |
| 1135 | - | `clrlwi r11, r11, 24` | insert |
| 1136 | `stb r11, 0x0, r24` | `stb r11, 0x0, r24` |  |
| 1137 | `addi r24, r24, 0x1` | `addi r24, r24, 0x1` |  |
| 1138 | `b 0x2d0` | `b 0x2d0` |  |
| 1139 | `lwz r11, 0x8, r31` | `lwz r11, 0x8, r31` |  |
| 1140 | `cmpwi cr6, r11, 0x0` | `cmpwi cr6, r11, 0x0` |  |
| 1141 | `beq cr6, 0x129c` | `beq cr6, 0x12a0` |  |
| 1142 | `b 0x11f8` | `b 0x11fc` |  |
| 1143 | `cmplwi cr6, r17, 0x0` | `cmplwi cr6, r17, 0x0` |  |
| 1144 | `beq cr6, 0x1358` | `beq cr6, 0x135c` |  |
| 1145 | `lbz r11, 0x0, r18` | `lbz r11, 0x0, r18` |  |
| 1146 | `subi r17, r17, 0x1` | `subi r17, r17, 0x1` |  |
| 1147 | `addi r18, r18, 0x1` | `addi r18, r18, 0x1` |  |
| 1148 | `slw r11, r11, r30` | `slw r11, r11, r30` |  |
| 1149 | `addi r30, r30, 0x8` | `addi r30, r30, 0x8` |  |
| 1150 | `add r29, r11, r29` | `add r29, r11, r29` |  |
| 1151 | `cmplwi cr6, r30, 0x20` | `cmplwi cr6, r30, 0x20` |  |
| 1152 | `blt cr6, 0x11d8` | `blt cr6, 0x11dc` |  |
| 1153 | `lwz r11, 0x14, r16` | `lwz r11, 0x14, r16` |  |
| 1154 | `subf. r5, r25, r22` | `subf. r5, r25, r22` |  |
| 1155 | `add r11, r11, r5` | `add r11, r11, r5` |  |
| 1156 | `stw r11, 0x14, r16` | `stw r11, 0x14, r16` |  |
| 1157 | `lwz r11, 0x18, r31` | `lwz r11, 0x18, r31` |  |
| 1158 | `add r11, r11, r5` | `add r11, r11, r5` |  |
| 1159 | `stw r11, 0x18, r31` | `stw r11, 0x18, r31` |  |
| 1160 | `beq 0x1248` | `beq 0x124c` |  |
| 1161 | `lwz r11, 0x10, r31` | `lwz r11, 0x10, r31` |  |
| 1162 | `subf r4, r5, r24` | `subf r4, r5, r24` |  |
| 1163 | `lwz r3, 0x14, r31` | `lwz r3, 0x14, r31` |  |
| 1164 | `cmpwi cr6, r11, 0x0` | `cmpwi cr6, r11, 0x0` |  |
| 1165 | `beq cr6, 0x123c` | `beq cr6, 0x1240` |  |
| 1166 | `bl crc32_big` | `bl crc32` |  |
| 1167 | `b 0x1240` | `b 0x1244` |  |
| 1168 | `bl ?adler32@D3DX@@YAKKPBEI@Z` | `bl adler32` |  |
| 1169 | `stw r3, 0x14, r31` | `stw r3, 0x14, r31` |  |
| 1170 | `stw r3, 0x30, r16` | `stw r3, 0x30, r16` |  |
| 1171 | `lwz r11, 0x10, r31` | `lwz r11, 0x10, r31` |  |
| 1172 | `mr r22, r25` | `mr r22, r25` |  |
| 1173 | `cmpwi cr6, r11, 0x0` | `cmpwi cr6, r11, 0x0` |  |
| 1174 | `beq cr6, 0x1260` | `beq cr6, 0x1264` |  |
| 1175 | `mr r11, r29` | `mr r11, r29` |  |
| 1176 | `b 0x1280` | `b 0x1284` |  |
| 1177 | `slwi r10, r29, 16` | `slwi r10, r29, 16` |  |
| 1178 | `rlwinm r11, r29, 0, 16, 23` | `rlwinm r11, r29, 0, 16, 23` |  |
| 1179 | `rlwinm r9, r29, 24, 16, 23` | `rlwinm r9, r29, 24, 16, 23` |  |
| 1180 | `add r11, r11, r10` | `add r11, r11, r10` |  |
| 1181 | `srwi r10, r29, 24` | `srwi r10, r29, 24` |  |
| 1182 | `slwi r11, r11, 8` | `slwi r11, r11, 8` |  |
| 1183 | `add r11, r11, r9` | `add r11, r11, r9` |  |
| 1184 | `add r11, r11, r10` | `add r11, r11, r10` |  |
| 1185 | `lwz r10, 0x14, r31` | `lwz r10, 0x14, r31` |  |
| 1186 | `cmplw cr6, r11, r10` | `cmplw cr6, r11, r10` |  |
| 1187 | `beq cr6, 0x1294` | `beq cr6, 0x1298` |  |
| 1188 | `lwz r11, 0x94, r1` | `lwz r11, 0x94, r1` |  |
| 1189 | `b 0x12f8` | `b 0x12fc` |  |
| 1190 | `mr r29, r23` | `mr r29, r23` |  |
| 1191 | `mr r30, r23` | `mr r30, r23` |  |
| 1192 | `li r11, 0x19` | `li r11, 0x19` |  |
| 1193 | `stw r11, 0x0, r31` | `stw r11, 0x0, r31` |  |
| 1194 | `lwz r11, 0x8, r31` | `lwz r11, 0x8, r31` |  |
| 1195 | `cmpwi cr6, r11, 0x0` | `cmpwi cr6, r11, 0x0` |  |
| 1196 | `beq cr6, 0x1340` | `beq cr6, 0x1344` |  |
| 1197 | `lwz r11, 0x10, r31` | `lwz r11, 0x10, r31` |  |
| 1198 | `cmpwi cr6, r11, 0x0` | `cmpwi cr6, r11, 0x0` |  |
| 1199 | `beq cr6, 0x1340` | `beq cr6, 0x1344` |  |
| 1200 | `b 0x12e0` | `b 0x12e4` |  |
| 1201 | `cmplwi cr6, r17, 0x0` | `cmplwi cr6, r17, 0x0` |  |
| 1202 | `beq cr6, 0x1358` | `beq cr6, 0x135c` |  |
| 1203 | `lbz r11, 0x0, r18` | `lbz r11, 0x0, r18` |  |
| 1204 | `subi r17, r17, 0x1` | `subi r17, r17, 0x1` |  |
| 1205 | `addi r18, r18, 0x1` | `addi r18, r18, 0x1` |  |
| 1206 | `slw r11, r11, r30` | `slw r11, r11, r30` |  |
| 1207 | `addi r30, r30, 0x8` | `addi r30, r30, 0x8` |  |
| 1208 | `add r29, r11, r29` | `add r29, r11, r29` |  |
| 1209 | `cmplwi cr6, r30, 0x20` | `cmplwi cr6, r30, 0x20` |  |
| 1210 | `blt cr6, 0x12c0` | `blt cr6, 0x12c4` |  |
| 1211 | `lwz r11, 0x18, r31` | `lwz r11, 0x18, r31` |  |
| 1212 | `cmplw cr6, r29, r11` | `cmplw cr6, r29, r11` |  |
| 1213 | `beq cr6, 0x1338` | `beq cr6, 0x133c` |  |
| 1214 | `lwz r11, 0x98, r1` | `lwz r11, 0x98, r1` |  |
| 1215 | `stw r11, 0x18, r16` | `stw r11, 0x18, r16` |  |
| 1216 | `stw r14, 0x0, r31` | `stw r14, 0x0, r31` |  |
| 1217 | `lwz r19, 0x0, r31` | `lwz r19, 0x0, r31` |  |
| 1218 | `cmplwi cr6, r19, 0x1c` | `cmplwi cr6, r19, 0x1c` |  |
| 1219 | `ble cr6, 0x180` | `ble cr6, 0x180` |  |
| 1220 | `li r3, -0x2` | `li r3, -0x2` |  |
| 1221 | `addi r1, r1, 0x140` | `addi r1, r1, 0x140` |  |
| 1222 | `b __restgprlr` | `b __restgprlr_14` |  |
| 1223 | `stw r24, 0xc, r16` | `stw r24, 0xc, r16` |  |
| 1224 | `li r3, 0x2` | `li r3, 0x2` |  |
| 1225 | `stw r25, 0x10, r16` | `stw r25, 0x10, r16` |  |
| 1226 | `stw r18, 0x0, r16` | `stw r18, 0x0, r16` |  |
| 1227 | `stw r17, 0x4, r16` | `stw r17, 0x4, r16` |  |
| 1228 | `stw r29, 0x30, r31` | `stw r29, 0x30, r31` |  |
| 1229 | `stw r30, 0x34, r31` | `stw r30, 0x34, r31` |  |
| 1230 | `b 0x1310` | `b 0x1314` |  |
| 1231 | `mr r29, r23` | `mr r29, r23` |  |
| 1232 | `mr r30, r23` | `mr r30, r23` |  |
| 1233 | `li r11, 0x1a` | `li r11, 0x1a` |  |
| 1234 | `stw r11, 0x0, r31` | `stw r11, 0x0, r31` |  |
| 1235 | `stw r15, 0x5c, r1` | `stw r15, 0x5c, r1` |  |
| 1236 | `b 0x1358` | `b 0x135c` |  |
| 1237 | `li r11, -0x3` | `li r11, -0x3` |  |
| 1238 | `stw r11, 0x5c, r1` | - | delete |
| 1239 | `stw r18, 0x0, r16` | - | delete |
| 1240 | `stw r25, 0x10, r16` | `stw r11, 0x5c, r1` | diff_arg |
| 1241 | `stw r24, 0xc, r16` | `stw r25, 0x10, r16` | diff_arg |
| 1242 | `stw r17, 0x4, r16` | `stw r24, 0xc, r16` | diff_arg |
| 1243 | `stw r29, 0x30, r31` | `stw r17, 0x4, r16` | diff_arg |
| 1244 | `stw r30, 0x34, r31` | `stw r18, 0x0, r16` | diff_arg |
| 1245 | `lwz r11, 0x20, r31` | `lwz r11, 0x20, r31` |  |
| 1246 | `cmplwi cr6, r11, 0x0` | `cmplwi cr6, r11, 0x0` |  |
| 1247 | - | `stw r29, 0x30, r31` | insert |
| 1248 | - | `stw r30, 0x34, r31` | insert |
| 1249 | `bne cr6, 0x1394` | `bne cr6, 0x1398` |  |
| 1250 | `lwz r11, 0x0, r31` | `lwz r11, 0x0, r31` |  |
| 1251 | `cmpwi cr6, r11, 0x18` | `cmpwi cr6, r11, 0x18` |  |
| 1252 | `bge cr6, 0x13b8` | `bge cr6, 0x13bc` |  |
| 1253 | `lwz r11, 0x10, r16` | `lwz r11, 0x10, r16` |  |
| 1254 | `cmplw cr6, r22, r11` | `cmplw cr6, r22, r11` |  |
| 1255 | `beq cr6, 0x13b8` | `beq cr6, 0x13bc` |  |
| 1256 | `mr r4, r22` | `mr r4, r22` |  |
| 1257 | `mr r3, r16` | `mr r3, r16` |  |
| 1258 | `bl updatewindow` | `bl updatewindow` |  |
| 1259 | `cmpwi r3, 0x0` | `cmpwi r3, 0x0` |  |
| 1260 | `beq 0x13b8` | `beq 0x13bc` |  |
| 1261 | `li r11, 0x1c` | `li r11, 0x1c` |  |
| 1262 | `li r3, -0x4` | `li r3, -0x4` |  |
| 1263 | `stw r11, 0x0, r31` | `stw r11, 0x0, r31` |  |
| 1264 | `b 0x1310` | `b 0x1314` |  |
| 1265 | `lwz r11, 0x10, r16` | `lwz r11, 0x10, r16` |  |
| 1266 | `lwz r9, 0x4, r16` | `lwz r9, 0x4, r16` |  |
| 1267 | `lwz r8, 0x9c, r1` | `lwz r8, 0x9c, r1` |  |
| 1268 | `subf r30, r11, r22` | `subf r30, r11, r22` |  |
| 1269 | `lwz r11, 0x14, r16` | `lwz r11, 0x14, r16` |  |
| 1270 | `lwz r10, 0x8, r16` | `lwz r10, 0x8, r16` |  |
| 1271 | `subf r29, r9, r8` | `subf r29, r9, r8` |  |
| 1272 | `add r11, r11, r30` | `add r11, r11, r30` |  |
| 1273 | `add r10, r10, r29` | `add r10, r10, r29` |  |
| 1274 | `stw r11, 0x14, r16` | `stw r11, 0x14, r16` |  |
| 1275 | `stw r10, 0x8, r16` | `stw r10, 0x8, r16` |  |
| 1276 | `lwz r11, 0x8, r31` | `lwz r11, 0x8, r31` |  |
| 1277 | `cmpwi cr6, r11, 0x0` | `cmpwi cr6, r11, 0x0` |  |
| 1278 | `lwz r11, 0x18, r31` | `lwz r11, 0x18, r31` |  |
| 1279 | `add r11, r11, r30` | `add r11, r11, r30` |  |
| 1280 | `stw r11, 0x18, r31` | `stw r11, 0x18, r31` |  |
| 1281 | `beq cr6, 0x1434` | `beq cr6, 0x1438` |  |
| 1282 | `cmplwi cr6, r30, 0x0` | `cmplwi cr6, r30, 0x0` |  |
| 1283 | `beq cr6, 0x1434` | `beq cr6, 0x1438` |  |
| 1284 | `lwz r11, 0x10, r31` | `lwz r11, 0x10, r31` |  |
| 1285 | `mr r5, r30` | `mr r5, r30` |  |
| 1286 | `lwz r3, 0x14, r31` | `lwz r3, 0x14, r31` |  |
| 1287 | `cmpwi cr6, r11, 0x0` | `cmpwi cr6, r11, 0x0` |  |
| 1288 | `lwz r11, 0xc, r16` | `lwz r11, 0xc, r16` |  |
| 1289 | `subf r4, r30, r11` | `subf r4, r30, r11` |  |
| 1290 | `beq cr6, 0x1428` | `beq cr6, 0x142c` |  |
| 1291 | `bl crc32_big` | `bl crc32` |  |
| 1292 | `b 0x142c` | `b 0x1430` |  |
| 1293 | `bl ?adler32@D3DX@@YAKKPBEI@Z` | `bl adler32` |  |
| 1294 | `stw r3, 0x14, r31` | `stw r3, 0x14, r31` |  |
| 1295 | `stw r3, 0x30, r16` | `stw r3, 0x30, r16` |  |
| 1296 | `lwz r10, 0x4, r31` | `lwz r10, 0x4, r31` |  |
| 1297 | `li r9, 0x40` | `li r9, 0x40` |  |
| 1298 | `lwz r11, 0x0, r31` | `lwz r11, 0x0, r31` |  |
| 1299 | `li r8, 0x80` | `li r8, 0x80` |  |
| 1300 | `subfic r10, r10, 0x0` | `subfic r10, r10, 0x0` |  |
| 1301 | `lwz r10, 0x34, r31` | `lwz r10, 0x34, r31` |  |
| 1302 | `subi r11, r11, 0xb` | `subi r11, r11, 0xb` |  |
| 1303 | `subfe r7, r7, r7` | `subfe r7, r7, r7` |  |
| 1304 | `subic r11, r11, 0x1` | `subic r11, r11, 0x1` |  |
| 1305 | `and r11, r7, r9` | `and r11, r7, r9` |  |
| 1306 | `subfe r9, r6, r6` | `subfe r9, r6, r6` |  |
| 1307 | `cmplwi cr6, r29, 0x0` | `cmplwi cr6, r29, 0x0` |  |
| 1308 | `and r9, r9, r8` | `and r9, r9, r8` |  |
| 1309 | `add r11, r11, r9` | `add r11, r11, r9` |  |
| 1310 | `add r11, r11, r10` | `add r11, r11, r10` |  |
| 1311 | `stw r11, 0x2c, r16` | `stw r11, 0x2c, r16` |  |
| 1312 | `bne cr6, 0x1480` | `bne cr6, 0x1484` |  |
| 1313 | `cmplwi cr6, r30, 0x0` | `cmplwi cr6, r30, 0x0` |  |
| 1314 | `beq cr6, 0x148c` | `beq cr6, 0x1490` |  |
| 1315 | `lwz r11, 0x15c, r1` | `lwz r11, 0x15c, r1` |  |
| 1316 | `cmpwi cr6, r11, 0x4` | `cmpwi cr6, r11, 0x4` |  |
| 1317 | `bne cr6, 0x14a0` | `bne cr6, 0x14a4` |  |
| 1318 | `lwz r3, 0x5c, r1` | `lwz r3, 0x5c, r1` |  |
| 1319 | `cmpwi cr6, r3, 0x0` | `cmpwi cr6, r3, 0x0` |  |
| 1320 | `bne cr6, 0x1310` | `bne cr6, 0x1314` |  |
| 1321 | `li r3, -0x5` | `li r3, -0x5` |  |
| 1322 | `b 0x1310` | `b 0x1314` |  |
| 1323 | `lwz r3, 0x5c, r1` | `lwz r3, 0x5c, r1` |  |
| 1324 | `b 0x1310` | `b 0x1314` |  |
| 1325 | `li r3, -0x4` | `li r3, -0x4` |  |
| 1326 | `b 0x1310` | `b 0x1314` |  |



## Detected Patterns

- **unknown**

## Mismatches (41 of 1327 instructions)

- [113] diff_arg: `beq` [br]
- [185] diff_arg: `beq` [br]
- [218] diff_arg: `beq` [br]
- [248] diff_arg: `beq` [br]
- [277] diff_arg: `beq` [br]
- [325] diff_arg: `bne` [br]
- [332] diff_arg: `beq` [br]
- [350] diff_arg: `bne` [br]
- [357] diff_arg: `beq` [br]
- [375] diff_arg: `bne` [br]
- [383] diff_arg: `beq` [br]
- [408] diff_arg: `beq` [br]
- [444] diff_arg: `beq` [br]
- [454] diff_arg: `beq` [br]
- [501] diff_arg: `beq` [br]
- [532] diff_arg: `beq` [br]
- [548] diff_arg: `beq` [br]
- [583] diff_arg: `beq` [br]
- [655] diff_arg: `beq` [br]
- [681] diff_arg: `beq` [br]
- [701] diff_arg: `beq` [br]
- [728] diff_arg: `beq` [br]
- [747] diff_arg: `beq` [br]
- [861] diff_arg: `beq` [br]
- [899] diff_arg: `beq` [br]
- [949] diff_arg: `beq` [br]
- [978] diff_arg: `beq` [br]
- [1015] diff_arg: `beq` [br]
- [1060] diff_arg: `beq` [br]
- [1089] diff_arg: `beq` [br]
- [1131] diff_arg: `beq` [br]
- [1135] insert: `clrlwi   r11, r11, 24`
- [1238] delete: `stw      r11, 0x5c, r1`
- [1239] delete: `stw      r18, 0x0, r16`
- [1240] diff_arg: `stw` [reg:r25->r11, off:+76, reg:r16->r1]
- [1241] diff_arg: `stw` [reg:r24->r25, off:+4]
- [1242] diff_arg: `stw` [reg:r17->r24, off:+8]
- [1243] diff_arg: `stw` [reg:r29->r17, off:-44, reg:r31->r16]
- [1244] diff_arg: `stw` [reg:r30->r18, off:-52, reg:r31->r16]
- [1247] insert: `stw      r29, 0x30, r31`
- [1248] insert: `stw      r30, 0x34, r31`

[stderr]
Building incremental: build/45410914/src/system/zlib/inflate.obj
Loaded 5 ICF equivalence entries from /home/free/code/milohax/rb3-xenon/build/45410914/icf_aliases.map