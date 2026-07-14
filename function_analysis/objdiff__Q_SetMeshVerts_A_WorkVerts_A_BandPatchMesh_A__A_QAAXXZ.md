# Diff: public: void __cdecl BandPatchMesh::WorkVerts::SetMeshVerts(void)

- **Symbol**: `?SetMeshVerts@WorkVerts@BandPatchMesh@@QAAXXZ`
- **Demangled**: `public: void __cdecl BandPatchMesh::WorkVerts::SetMeshVerts(void)`
- **Match**: 92.6% normalized (92.5% raw)
  - Fine-tuning band. Check comparison patterns (>= vs >, signed vs unsigned), casting, commutative-operand ordering, then run the permuter on any residual cascade.
- **Target Size**: 1280 bytes
- **Base Size**: 1264 bytes
- **Diff Score**: 2367 / 32000

## Instruction Summary

| Type | Count | Percent |
|------|------:|--------:|
| equal | 257 | 78.4% |
| diff_arg | 50 | 15.2% |
| diff_op | 1 | 0.3% |
| delete | 12 | 3.7% |
| insert | 8 | 2.4% |
| **Total** | 328 | 100.0% |

## Region Summary

| Region | Instructions | Match % | Notes |
|--------|------------:|--------:|-------|
| 0-145 | 146 | 100% |  |
| 146-146 | 1 | 0% |  |
| 147-164 | 18 | 100% |  |
| 165-174 | 10 | 50% | 2 register swaps, 2 inserts |
| 175-194 | 20 | 100% |  |
| 195-215 | 21 | 24% | 11 register swaps, 2 inserts, 2 deletes |
| 216-233 | 18 | 100% |  |
| 234-319 | 86 | 43% | 28 register swaps, 4 inserts, 10 deletes |
| 320-327 | 8 | 100% |  |

## Patterns Detected

- **REGISTER_SWAP** (MaybeFixable): 55 instructions across 8 pairs, dominated by r7↔r8 (14 of 55) [mixed volatile+callee-saved] [docs](docs/decomp/patterns/permuter-roi.md#register-allocation-cascades)
  - r7↔r8: 14
  - r26↔r27: 10
  - r8↔r9: 10
  - ...and 5 more

**Unattributed mismatches**: 16 | **Patterns checked**: 21

## Function Call Diff

**Target only:** `?_M_fill_insert@?$vector@EV?$StlNodeAlloc@E@stlpmtx_std@@@stlpmtx_std@@AAAXPAEIABE@Z` (1), `fn_823360A0` (1), `fn_82709EE0` (1), `fn_82797500` (1), `fn_827975C8` (1), `fn_82B38C08` (1), `lbl_82337870` (1)
**Base only:** `??0MemTemp@@QAA@XZ` (1), `??1MemTemp@@QAA@XZ` (1), `??_U@YAPAXI@Z` (1), `?_M_fill_insert@?$vector@IV?$StlNodeAlloc@I@stlpmtx_std@@@stlpmtx_std@@AAAXPAIIABI@Z` (1), `?reserve@?$vector@GV?$StlNodeAlloc@G@stlpmtx_std@@@stlpmtx_std@@QAAXI@Z` (1), `?reserve@?$vector@PAVMeshVert@BandPatchMesh@@V?$StlNodeAlloc@PAVMeshVert@BandPatchMesh@@@stlpmtx_std@@@stlpmtx_std@@QAAXI@Z` (1), `?resize@?$vector@VMeshFace@BandPatchMesh@@V?$StlNodeAlloc@VMeshFace@BandPatchMesh@@@stlpmtx_std@@@stlpmtx_std@@QAAXIABVMeshFace@BandPatchMesh@@@Z` (1)

## Insert/Delete Clusters

| Range | Inserts | Deletes | Dominant Opcodes |
|-------|--------:|--------:|------------------|
| 209-214 | 2 | 2 | addi, slwi, lhz |
| 251-257 | 1 | 3 | mr, lwzx, stw |
| 283-295 | 3 | 5 | divw, slwi, addi |

## Verdict: LikelyFixable (Medium confidence)

1 control flow difference(s) detected with low merged ratio (0.0%).

### Verdict Factors

| Factor | Value | Threshold | Result |
|--------|-------|-----------|--------|
| bool_mask_detected | false | - | not_detected |
| merged_call_ratio | 0.00 | 0.8 | below_threshold |
| control_flow_diffs | 1.00 | 1.0 | detected |

**Recommendation**: Investigate control flow structure.

### Suggestions

1. Try `> 0` vs `!= 0`, `>=` vs `>`, if/else inversion ([docs](docs/decomp/patterns/fixable-comparison.md#unsigned-zero-comparison))

### Related Documentation

- [docs/decomp/patterns/fixable-declarations.md#variable-declaration-order](docs/decomp/patterns/fixable-declarations.md#variable-declaration-order)
- [docs/decomp/patterns/permuter-roi.md#register-allocation-cascades](docs/decomp/patterns/permuter-roi.md#register-allocation-cascades)

## Full Instruction Listing

| Index | Target | Base | Match |
|------:|--------|------|-------|
| 0 | `mflr r12` | `mflr r12` |  |
| 1 | `bl __savegprlr_22` | `bl __savegprlr_22` |  |
| 2 | `subi r31, r1, 0xc0` | `subi r31, r1, 0xc0` |  |
| 3 | `stwu r1, -0xc0, r1` | `stwu r1, -0xc0, r1` |  |
| 4 | `mr r29, r3` | `mr r29, r3` |  |
| 5 | `addi r3, r31, 0x58` | `addi r3, r31, 0x58` |  |
| 6 | `bl fn_82797500` | `bl ??0MemTemp@@QAA@XZ` |  |
| 7 | `lwz r11, 0x44, r29` | `lwz r11, 0x44, r29` |  |
| 8 | `addi r3, r29, 0x14` | `addi r3, r29, 0x14` |  |
| 9 | `lwz r11, 0x110, r11` | `lwz r11, 0x110, r11` |  |
| 10 | `lwz r4, 0xdc, r11` | `lwz r4, 0xdc, r11` |  |
| 11 | `bl fn_82B38C08` | `bl ?reserve@?$vector@PAVMeshVert@BandPatchMesh@@V?$StlNodeAlloc@PAVMeshVert@BandPatchMesh@@@stlpmtx_std@@@stlpmtx_std@@QAAXI@Z` |  |
| 12 | `lwz r11, 0x44, r29` | `lwz r11, 0x44, r29` |  |
| 13 | `li r28, 0x6` | `li r28, 0x6` |  |
| 14 | `addi r3, r29, 0x2c` | `addi r3, r29, 0x2c` |  |
| 15 | `lwz r11, 0x110, r11` | `lwz r11, 0x110, r11` |  |
| 16 | `addi r10, r11, 0xe4` | `addi r10, r11, 0xe4` |  |
| 17 | `stw r10, 0x54, r31` | `stw r10, 0x54, r31` |  |
| 18 | `lwz r10, 0xe8, r11` | `lwz r10, 0xe8, r11` |  |
| 19 | `lwz r11, 0xe4, r11` | `lwz r11, 0xe4, r11` |  |
| 20 | `subf r11, r11, r10` | `subf r11, r11, r10` |  |
| 21 | `divw r4, r11, r28` | `divw r4, r11, r28` |  |
| 22 | `bl fn_823360A0` | `bl ?reserve@?$vector@GV?$StlNodeAlloc@G@stlpmtx_std@@@stlpmtx_std@@QAAXI@Z` |  |
| 23 | `addi r11, r31, 0x50` | `addi r11, r31, 0x50` |  |
| 24 | `li r24, 0x0` | `li r24, 0x0` |  |
| 25 | `addi r30, r29, 0x38` | `addi r30, r29, 0x38` |  |
| 26 | `addi r5, r31, 0x50` | `addi r5, r31, 0x50` |  |
| 27 | `mr r3, r30` | `mr r3, r30` |  |
| 28 | `stb r24, 0x0, r11` | `stb r24, 0x0, r11` |  |
| 29 | `lwz r11, 0x44, r29` | `lwz r11, 0x44, r29` |  |
| 30 | `lwz r11, 0x110, r11` | `lwz r11, 0x110, r11` |  |
| 31 | `addi r10, r11, 0xe4` | `addi r10, r11, 0xe4` |  |
| 32 | `stw r10, 0x54, r31` | `stw r10, 0x54, r31` |  |
| 33 | `lwz r10, 0xe8, r11` | `lwz r10, 0xe8, r11` |  |
| 34 | `lwz r11, 0xe4, r11` | `lwz r11, 0xe4, r11` |  |
| 35 | `subf r11, r11, r10` | `subf r11, r11, r10` |  |
| 36 | `divw r4, r11, r28` | `divw r4, r11, r28` |  |
| 37 | `bl lbl_82337870` | `bl ?resize@?$vector@VMeshFace@BandPatchMesh@@V?$StlNodeAlloc@VMeshFace@BandPatchMesh@@@stlpmtx_std@@@stlpmtx_std@@QAAXIABVMeshFace@BandPatchMesh@@@Z` |  |
| 38 | `lwz r10, 0x3c, r29` | `lwz r10, 0x3c, r29` |  |
| 39 | `lwz r9, 0x38, r29` | `lwz r9, 0x38, r29` |  |
| 40 | `li r27, -0x1` | `li r27, -0x1` |  |
| 41 | `mr r11, r24` | `mr r11, r24` |  |
| 42 | `subf. r10, r9, r10` | `subf. r10, r9, r10` |  |
| 43 | `beq 0xd0` | `beq 0xd8` |  |
| 44 | `lwz r10, 0x0, r30` | `lwz r10, 0x0, r30` |  |
| 45 | `stbx r27, r10, r11` | `stbx r27, r10, r11` |  |
| 46 | `addi r11, r11, 0x1` | `addi r11, r11, 0x1` |  |
| 47 | `lwz r10, 0x4, r30` | `lwz r10, 0x4, r30` |  |
| 48 | `lwz r9, 0x0, r30` | `lwz r9, 0x0, r30` |  |
| 49 | `subf r10, r9, r10` | `subf r10, r9, r10` |  |
| 50 | `cmplw cr6, r11, r10` | `cmplw cr6, r11, r10` |  |
| 51 | `blt cr6, 0xb0` | `blt cr6, 0xb8` |  |
| 52 | `lwz r10, 0x44, r29` | `lwz r10, 0x44, r29` |  |
| 53 | `addi r30, r29, 0x4` | `addi r30, r29, 0x4` |  |
| 54 | `lwz r11, 0x4, r29` | `lwz r11, 0x4, r29` |  |
| 55 | `lwz r5, 0x8, r29` | `lwz r5, 0x8, r29` |  |
| 56 | `stw r24, 0x54, r31` | `stw r24, 0x54, r31` |  |
| 57 | `subf r9, r11, r5` | `subf r9, r11, r5` |  |
| 58 | `lwz r10, 0x110, r10` | `lwz r10, 0x110, r10` |  |
| 59 | `srawi r9, r9, 2` | `srawi r9, r9, 2` |  |
| 60 | `lwz r10, 0xdc, r10` | `lwz r10, 0xdc, r10` |  |
| 61 | `cmplw cr6, r10, r9` | `cmplw cr6, r10, r9` |  |
| 62 | `bge cr6, 0x118` | `bge cr6, 0x120` |  |
| 63 | `slwi r10, r10, 2` | `slwi r10, r10, 2` |  |
| 64 | `stw r5, 0x54, r31` | `stw r5, 0x54, r31` |  |
| 65 | `mr r3, r30` | `mr r3, r30` |  |
| 66 | `stw r11, 0x54, r31` | `stw r11, 0x54, r31` |  |
| 67 | `add r4, r10, r11` | `add r4, r10, r11` |  |
| 68 | `bl ?erase@?$vector@IV?$StlNodeAlloc@I@stlpmtx_std@@@stlpmtx_std@@QAAPAIPAI0@Z` | `bl ?erase@?$vector@IV?$StlNodeAlloc@I@stlpmtx_std@@@stlpmtx_std@@QAAPAIPAI0@Z` |  |
| 69 | `b 0x138` | `b 0x140` |  |
| 70 | `lwz r4, 0x4, r30` | `lwz r4, 0x4, r30` |  |
| 71 | `addi r6, r31, 0x54` | `addi r6, r31, 0x54` |  |
| 72 | `mr r3, r30` | `mr r3, r30` |  |
| 73 | `subf r11, r11, r4` | `subf r11, r11, r4` |  |
| 74 | `srawi r11, r11, 2` | `srawi r11, r11, 2` |  |
| 75 | `stw r4, 0x5c, r31` | `stw r4, 0x5c, r31` |  |
| 76 | `subf r5, r11, r10` | `subf r5, r11, r10` |  |
| 77 | `bl ?_M_fill_insert@?$vector@EV?$StlNodeAlloc@E@stlpmtx_std@@@stlpmtx_std@@AAAXPAEIABE@Z` | `bl ?_M_fill_insert@?$vector@IV?$StlNodeAlloc@I@stlpmtx_std@@@stlpmtx_std@@AAAXPAIIABI@Z` |  |
| 78 | `lwz r11, 0x4, r30` | `lwz r11, 0x4, r30` |  |
| 79 | `mr r10, r24` | `mr r10, r24` |  |
| 80 | `lwz r9, 0x0, r30` | `lwz r9, 0x0, r30` |  |
| 81 | `subf r11, r9, r11` | `subf r11, r9, r11` |  |
| 82 | `srawi. r11, r11, 2` | `srawi. r11, r11, 2` |  |
| 83 | `beq 0x17c` | `beq 0x184` |  |
| 84 | `mr r11, r24` | `mr r11, r24` |  |
| 85 | `lwz r9, 0x0, r30` | `lwz r9, 0x0, r30` |  |
| 86 | `addi r10, r10, 0x1` | `addi r10, r10, 0x1` |  |
| 87 | `stwx r24, r11, r9` | `stwx r24, r11, r9` |  |
| 88 | `addi r11, r11, 0x4` | `addi r11, r11, 0x4` |  |
| 89 | `lwz r9, 0x4, r30` | `lwz r9, 0x4, r30` |  |
| 90 | `lwz r8, 0x0, r30` | `lwz r8, 0x0, r30` |  |
| 91 | `subf r9, r8, r9` | `subf r9, r8, r9` |  |
| 92 | `srawi r9, r9, 2` | `srawi r9, r9, 2` |  |
| 93 | `cmplw cr6, r10, r9` | `cmplw cr6, r10, r9` |  |
| 94 | `blt cr6, 0x154` | `blt cr6, 0x15c` |  |
| 95 | `lwz r11, 0x44, r29` | `lwz r11, 0x44, r29` |  |
| 96 | `mr r6, r24` | `mr r6, r24` |  |
| 97 | `lwz r11, 0x110, r11` | `lwz r11, 0x110, r11` |  |
| 98 | `lwz r10, 0xe8, r11` | `lwz r10, 0xe8, r11` |  |
| 99 | `lwz r11, 0xe4, r11` | `lwz r11, 0xe4, r11` |  |
| 100 | `subf r11, r11, r10` | `subf r11, r11, r10` |  |
| 101 | `divw. r11, r11, r28` | `divw. r11, r11, r28` |  |
| 102 | `beq 0x200` | `beq 0x208` |  |
| 103 | `mr r8, r24` | `mr r8, r24` |  |
| 104 | `lwz r10, 0x44, r29` | `lwz r10, 0x44, r29` |  |
| 105 | `li r11, 0x3` | `li r11, 0x3` |  |
| 106 | `lwz r10, 0x110, r10` | `lwz r10, 0x110, r10` |  |
| 107 | `mtctr r11` | `mtctr r11` |  |
| 108 | `lwz r11, 0xe4, r10` | `lwz r11, 0xe4, r10` |  |
| 109 | `add r11, r8, r11` | `add r11, r8, r11` |  |
| 110 | `subi r11, r11, 0x2` | `subi r11, r11, 0x2` |  |
| 111 | `lhzu r9, 0x2, r11` | `lhzu r9, 0x2, r11` |  |
| 112 | `lwz r10, 0x0, r30` | `lwz r10, 0x0, r30` |  |
| 113 | `rotlwi r9, r9, 2` | `rotlwi r9, r9, 2` |  |
| 114 | `lwzx r7, r9, r10` | `lwzx r7, r9, r10` |  |
| 115 | `addi r7, r7, 0x1` | `addi r7, r7, 0x1` |  |
| 116 | `stwx r7, r9, r10` | `stwx r7, r9, r10` |  |
| 117 | `bdnz 0x1bc` | `bdnz 0x1c4` |  |
| 118 | `lwz r11, 0x44, r29` | `lwz r11, 0x44, r29` |  |
| 119 | `addi r6, r6, 0x1` | `addi r6, r6, 0x1` |  |
| 120 | `addi r8, r8, 0x6` | `addi r8, r8, 0x6` |  |
| 121 | `lwz r11, 0x110, r11` | `lwz r11, 0x110, r11` |  |
| 122 | `lwz r10, 0xe8, r11` | `lwz r10, 0xe8, r11` |  |
| 123 | `lwz r11, 0xe4, r11` | `lwz r11, 0xe4, r11` |  |
| 124 | `subf r11, r11, r10` | `subf r11, r11, r10` |  |
| 125 | `divw r11, r11, r28` | `divw r11, r11, r28` |  |
| 126 | `cmplw cr6, r6, r11` | `cmplw cr6, r6, r11` |  |
| 127 | `blt cr6, 0x1a0` | `blt cr6, 0x1a8` |  |
| 128 | `lwz r11, 0x4, r30` | `lwz r11, 0x4, r30` |  |
| 129 | `mr r3, r24` | `mr r3, r24` |  |
| 130 | `lwz r9, 0x0, r30` | `lwz r9, 0x0, r30` |  |
| 131 | `mr r10, r24` | `mr r10, r24` |  |
| 132 | `subf r11, r9, r11` | `subf r11, r9, r11` |  |
| 133 | `srawi. r11, r11, 2` | `srawi. r11, r11, 2` |  |
| 134 | `beq 0x268` | `beq 0x270` |  |
| 135 | `mr r11, r24` | `mr r11, r24` |  |
| 136 | `lwz r8, 0x0, r30` | `lwz r8, 0x0, r30` |  |
| 137 | `addi r10, r10, 0x1` | `addi r10, r10, 0x1` |  |
| 138 | `lwzx r9, r11, r8` | `lwzx r9, r11, r8` |  |
| 139 | `stwx r3, r11, r8` | `stwx r3, r11, r8` |  |
| 140 | `addi r11, r11, 0x4` | `addi r11, r11, 0x4` |  |
| 141 | `addi r9, r9, 0x1` | `addi r9, r9, 0x1` |  |
| 142 | `lwz r7, 0x0, r30` | `lwz r7, 0x0, r30` |  |
| 143 | `stw r8, 0x5c, r31` | `stw r8, 0x5c, r31` |  |
| 144 | `clrrwi r9, r9, 1` | `clrrwi r9, r9, 1` |  |
| 145 | `stw r8, 0x5c, r31` | `stw r8, 0x5c, r31` |  |
| 146 | `addi r9, r9, 0x1e` | `addi r9, r9, 0x1a` | diff_arg |
| 147 | `slwi r9, r9, 1` | `slwi r9, r9, 1` |  |
| 148 | `add r3, r9, r3` | `add r3, r9, r3` |  |
| 149 | `lwz r9, 0x4, r30` | `lwz r9, 0x4, r30` |  |
| 150 | `subf r9, r7, r9` | `subf r9, r7, r9` |  |
| 151 | `srawi r9, r9, 2` | `srawi r9, r9, 2` |  |
| 152 | `cmplw cr6, r10, r9` | `cmplw cr6, r10, r9` |  |
| 153 | `blt cr6, 0x220` | `blt cr6, 0x228` |  |
| 154 | `bl fn_82709EE0` | `bl ??_U@YAPAXI@Z` |  |
| 155 | `stw r3, 0x10, r29` | `stw r3, 0x10, r29` |  |
| 156 | `lwz r11, 0x4, r30` | `lwz r11, 0x4, r30` |  |
| 157 | `mr r10, r24` | `mr r10, r24` |  |
| 158 | `lwz r9, 0x0, r30` | `lwz r9, 0x0, r30` |  |
| 159 | `subf r11, r9, r11` | `subf r11, r9, r11` |  |
| 160 | `srawi. r11, r11, 2` | `srawi. r11, r11, 2` |  |
| 161 | `beq 0x2e0` | `beq 0x2f0` |  |
| 162 | `mr r11, r24` | `mr r11, r24` |  |
| 163 | `lwz r7, 0x0, r30` | `lwz r7, 0x0, r30` |  |
| 164 | `addi r10, r10, 0x1` | `addi r10, r10, 0x1` |  |
| 165 | `lwz r8, 0x10, r29` | `lwz r9, 0x10, r29` | diff_arg |
| 166 | `lwzx r9, r11, r7` | `lwzx r8, r11, r7` | diff_arg |
| 167 | - | `stw r7, 0x5c, r31` | insert |
| 168 | `add r9, r8, r9` | `add r9, r8, r9` |  |
| 169 | - | `stw r7, 0x5c, r31` | insert |
| 170 | `stwx r9, r11, r7` | `stwx r9, r11, r7` |  |
| 171 | `lwz r9, 0x0, r30` | `lwz r9, 0x0, r30` |  |
| 172 | `lwzx r9, r11, r9` | `lwzx r9, r11, r9` |  |
| 173 | `addi r11, r11, 0x4` | `addi r11, r11, 0x4` |  |
| 174 | `stb r24, 0x2f, r9` | `stb r24, 0x27, r9` | diff_arg |
| 175 | `stw r27, 0x30, r9` | `stw r27, 0x30, r9` |  |
| 176 | `stw r27, 0x34, r9` | `stw r27, 0x34, r9` |  |
| 177 | `sth r24, 0x38, r9` | `sth r24, 0x38, r9` |  |
| 178 | `stw r24, 0x0, r9` | `stw r24, 0x0, r9` |  |
| 179 | `sth r24, 0x2c, r9` | `sth r24, 0x2c, r9` |  |
| 180 | `lwz r9, 0x4, r30` | `lwz r9, 0x4, r30` |  |
| 181 | `lwz r8, 0x0, r30` | `lwz r8, 0x0, r30` |  |
| 182 | `subf r9, r8, r9` | `subf r9, r8, r9` |  |
| 183 | `srawi r9, r9, 2` | `srawi r9, r9, 2` |  |
| 184 | `cmplw cr6, r10, r9` | `cmplw cr6, r10, r9` |  |
| 185 | `blt cr6, 0x28c` | `blt cr6, 0x294` |  |
| 186 | `lwz r10, 0x44, r29` | `lwz r10, 0x44, r29` |  |
| 187 | `mr r11, r24` | `mr r11, r24` |  |
| 188 | `lwz r10, 0x110, r10` | `lwz r10, 0x110, r10` |  |
| 189 | `lwz r9, 0xe8, r10` | `lwz r9, 0xe8, r10` |  |
| 190 | `lwz r10, 0xe4, r10` | `lwz r10, 0xe4, r10` |  |
| 191 | `subf r10, r10, r9` | `subf r10, r10, r9` |  |
| 192 | `divw. r10, r10, r28` | `divw. r10, r10, r28` |  |
| 193 | `beq 0x37c` | `beq 0x38c` |  |
| 194 | `mr r9, r24` | `mr r9, r24` |  |
| 195 | `lwz r7, 0x44, r29` | `lwz r8, 0x44, r29` | diff_arg |
| 196 | `li r10, 0x3` | `li r10, 0x3` |  |
| 197 | `clrlwi r8, r11, 16` | `clrlwi r7, r11, 16` | diff_arg |
| 198 | `lwz r7, 0x110, r7` | `lwz r8, 0x110, r8` | diff_arg |
| 199 | `mtctr r10` | `mtctr r10` |  |
| 200 | `lwz r10, 0xe4, r7` | `lwz r10, 0xe4, r8` | diff_arg |
| 201 | `add r10, r9, r10` | `add r10, r9, r10` |  |
| 202 | `subi r10, r10, 0x2` | `subi r10, r10, 0x2` |  |
| 203 | `lhzu r7, 0x2, r10` | `lhzu r8, 0x2, r10` | diff_arg |
| 204 | `lwz r6, 0x0, r30` | `lwz r6, 0x0, r30` |  |
| 205 | `rotlwi r7, r7, 2` | `rotlwi r8, r8, 2` | diff_arg |
| 206 | `lwzx r6, r7, r6` | `lwzx r6, r8, r6` | diff_arg |
| 207 | `lhz r7, 0x38, r6` | `lhz r8, 0x38, r6` | diff_arg |
| 208 | `addi r7, r7, 0x1d` | `addi r5, r8, 0x19` | diff_arg |
| 209 | - | `addi r8, r8, 0x1` | insert |
| 210 | - | `slwi r5, r5, 1` | insert |
| 211 | `slwi r7, r7, 1` | `clrlwi r8, r8, 16` | diff_op |
| 212 | `sthx r8, r7, r6` | `sthx r7, r5, r6` | diff_arg |
| 213 | `lhz r7, 0x38, r6` | - | delete |
| 214 | `addi r7, r7, 0x1` | - | delete |
| 215 | `sth r7, 0x38, r6` | `sth r8, 0x38, r6` | diff_arg |
| 216 | `bdnz 0x324` | `bdnz 0x334` |  |
| 217 | `lwz r10, 0x44, r29` | `lwz r10, 0x44, r29` |  |
| 218 | `addi r11, r11, 0x1` | `addi r11, r11, 0x1` |  |
| 219 | `addi r9, r9, 0x6` | `addi r9, r9, 0x6` |  |
| 220 | `lwz r10, 0x110, r10` | `lwz r10, 0x110, r10` |  |
| 221 | `lwz r8, 0xe8, r10` | `lwz r8, 0xe8, r10` |  |
| 222 | `lwz r10, 0xe4, r10` | `lwz r10, 0xe4, r10` |  |
| 223 | `subf r10, r10, r8` | `subf r10, r10, r8` |  |
| 224 | `divw r10, r10, r28` | `divw r10, r10, r28` |  |
| 225 | `cmplw cr6, r11, r10` | `cmplw cr6, r11, r10` |  |
| 226 | `blt cr6, 0x304` | `blt cr6, 0x314` |  |
| 227 | `lwz r11, 0x44, r29` | `lwz r11, 0x44, r29` |  |
| 228 | `addi r10, r29, 0x20` | `addi r10, r29, 0x20` |  |
| 229 | `lwz r9, 0x20, r29` | `lwz r9, 0x20, r29` |  |
| 230 | `lwz r8, 0x24, r29` | `lwz r8, 0x24, r29` |  |
| 231 | `subf r9, r9, r8` | `subf r9, r9, r8` |  |
| 232 | `lwz r11, 0x110, r11` | `lwz r11, 0x110, r11` |  |
| 233 | `srawi. r9, r9, 2` | `srawi. r9, r9, 2` |  |
| 234 | `lwz r28, 0xd8, r11` | `lwz r29, 0xd8, r11` | diff_arg |
| 235 | `beq 0x4f0` | `beq 0x4e8` |  |
| 236 | `li r25, 0x1` | `li r25, 0x1` |  |
| 237 | `mr r26, r24` | `mr r27, r24` | diff_arg |
| 238 | `mr r27, r25` | `mr r26, r25` | diff_arg |
| 239 | `li r29, 0x60` | `li r3, 0x60` | diff_arg |
| 240 | `lwz r9, 0x0, r10` | `lwz r11, 0x0, r10` | diff_arg |
| 241 | `lwz r11, 0x0, r30` | `lwz r8, 0x0, r30` | diff_arg |
| 242 | `lwzx r4, r26, r9` | `lwzx r4, r27, r11` | diff_arg |
| 243 | `stw r11, 0x5c, r31` | - | delete |
| 244 | `subf r9, r28, r4` | `subf r11, r29, r4` | diff_arg |
| 245 | `divw r8, r9, r29` | `divw r9, r11, r3` | diff_arg |
| 246 | `slwi r3, r8, 2` | `slwi r28, r9, 2` | diff_arg |
| 247 | `lwzx r9, r11, r3` | `lwzx r11, r8, r28` | diff_arg |
| 248 | `lwz r9, 0x30, r9` | `lwz r8, 0x30, r11` | diff_arg |
| 249 | `cmpwi cr6, r9, -0x1` | `cmpwi cr6, r8, -0x1` | diff_arg |
| 250 | `bne cr6, 0x4cc` | `bne cr6, 0x4c4` |  |
| 251 | `lwzx r9, r11, r3` | - | delete |
| 252 | `mr r6, r8` | - | delete |
| 253 | `stw r11, 0x5c, r31` | `stw r9, 0x30, r11` | diff_arg |
| 254 | `mr r7, r27` | `mr r6, r9` | diff_arg |
| 255 | `stw r8, 0x30, r9` | - | delete |
| 256 | `lwz r11, 0x4, r10` | `lwz r11, 0x4, r10` |  |
| 257 | - | `mr r8, r26` | insert |
| 258 | `lwz r9, 0x0, r10` | `lwz r7, 0x0, r10` | diff_arg |
| 259 | `subf r11, r9, r11` | `subf r11, r7, r11` | diff_arg |
| 260 | `srawi r11, r11, 2` | `srawi r11, r11, 2` |  |
| 261 | `cmplw cr6, r27, r11` | `cmplw cr6, r26, r11` | diff_arg |
| 262 | `bge cr6, 0x4cc` | `bge cr6, 0x4c4` |  |
| 263 | `addi r9, r26, 0x4` | `addi r7, r27, 0x4` | diff_arg |
| 264 | `lwz r11, 0x0, r10` | `lwz r11, 0x0, r10` |  |
| 265 | `lfs f0, 0x0, r4` | `lfs f0, 0x0, r4` |  |
| 266 | `lwzx r5, r9, r11` | `lwzx r11, r7, r11` | diff_arg |
| 267 | `stw r11, 0x5c, r31` | - | delete |
| 268 | `lfs f13, 0x0, r5` | `lfs f13, 0x0, r11` | diff_arg |
| 269 | `fcmpu cr6, f0, f13` | `fcmpu cr6, f0, f13` |  |
| 270 | `bne cr6, 0x44c` | `bne cr6, 0x44c` |  |
| 271 | `lfs f0, 0x4, r4` | `lfs f0, 0x4, r4` |  |
| 272 | `lfs f13, 0x4, r5` | `lfs f13, 0x4, r11` | diff_arg |
| 273 | `fcmpu cr6, f0, f13` | `fcmpu cr6, f0, f13` |  |
| 274 | `bne cr6, 0x44c` | `bne cr6, 0x44c` |  |
| 275 | `lfs f13, 0x8, r5` | `lfs f0, 0x8, r4` | diff_arg |
| 276 | `mr r5, r24` | `mr r5, r24` |  |
| 277 | `lfs f0, 0x8, r4` | `lfs f13, 0x8, r11` | diff_arg |
| 278 | `fcmpu cr6, f0, f13` | `fcmpu cr6, f0, f13` |  |
| 279 | `beq cr6, 0x450` | `beq cr6, 0x450` |  |
| 280 | `mr r5, r25` | `mr r5, r25` |  |
| 281 | `clrlwi. r5, r5, 24` | `clrlwi. r5, r5, 24` |  |
| 282 | `bne 0x4b8` | `bne 0x4b0` |  |
| 283 | `lwzx r5, r9, r11` | - | delete |
| 284 | `slwi r23, r6, 2` | - | delete |
| 285 | `stw r11, 0x5c, r31` | - | delete |
| 286 | `addi r7, r7, 0x1` | - | delete |
| 287 | `subf r11, r28, r5` | `subf r11, r29, r11` | diff_arg |
| 288 | `lwz r5, 0x0, r30` | `lwz r5, 0x0, r30` |  |
| 289 | - | `slwi r23, r6, 2` | insert |
| 290 | - | `divw r11, r11, r3` | insert |
| 291 | `addi r9, r9, 0x4` | `addi r8, r8, 0x1` | diff_arg |
| 292 | `divw r11, r11, r29` | - | delete |
| 293 | `slwi r22, r11, 2` | `slwi r22, r11, 2` |  |
| 294 | `mr r6, r11` | `mr r6, r11` |  |
| 295 | - | `addi r7, r7, 0x4` | insert |
| 296 | `lwzx r5, r5, r22` | `lwzx r5, r5, r22` |  |
| 297 | `stw r8, 0x30, r5` | `stw r9, 0x30, r5` | diff_arg |
| 298 | `lwz r5, 0x0, r30` | `lwz r5, 0x0, r30` |  |
| 299 | `lwzx r5, r5, r22` | `lwzx r5, r5, r22` |  |
| 300 | `stb r25, 0x2f, r5` | `stb r25, 0x27, r5` | diff_arg |
| 301 | `lwz r5, 0x0, r30` | `lwz r5, 0x0, r30` |  |
| 302 | `lwzx r5, r23, r5` | `lwzx r5, r23, r5` |  |
| 303 | `stw r11, 0x34, r5` | `stw r11, 0x34, r5` |  |
| 304 | `lwz r11, 0x4, r10` | `lwz r11, 0x4, r10` |  |
| 305 | `lwz r5, 0x0, r10` | `lwz r5, 0x0, r10` |  |
| 306 | `subf r11, r5, r11` | `subf r11, r5, r11` |  |
| 307 | `srawi r11, r11, 2` | `srawi r11, r11, 2` |  |
| 308 | `cmplw cr6, r7, r11` | `cmplw cr6, r8, r11` | diff_arg |
| 309 | `blt cr6, 0x40c` | `blt cr6, 0x410` |  |
| 310 | `cmpw cr6, r6, r8` | `cmpw cr6, r6, r9` | diff_arg |
| 311 | `beq cr6, 0x4cc` | `beq cr6, 0x4c4` |  |
| 312 | `lwz r11, 0x0, r30` | `lwz r11, 0x0, r30` |  |
| 313 | `lwzx r11, r11, r3` | `lwzx r11, r11, r28` | diff_arg |
| 314 | `stb r25, 0x2f, r11` | `stb r25, 0x27, r11` | diff_arg |
| 315 | `lwz r11, 0x4, r10` | `lwz r11, 0x4, r10` |  |
| 316 | `addi r27, r27, 0x1` | `addi r26, r26, 0x1` | diff_arg |
| 317 | `lwz r9, 0x0, r10` | `lwz r9, 0x0, r10` |  |
| 318 | `addi r26, r26, 0x4` | `addi r27, r27, 0x4` | diff_arg |
| 319 | `subi r8, r27, 0x1` | `subi r8, r26, 0x1` | diff_arg |
| 320 | `subf r11, r9, r11` | `subf r11, r9, r11` |  |
| 321 | `srawi r11, r11, 2` | `srawi r11, r11, 2` |  |
| 322 | `cmplw cr6, r8, r11` | `cmplw cr6, r8, r11` |  |
| 323 | `blt cr6, 0x3b0` | `blt cr6, 0x3c0` |  |
| 324 | `addi r3, r31, 0x58` | `addi r3, r31, 0x58` |  |
| 325 | `bl fn_827975C8` | `bl ??1MemTemp@@QAA@XZ` |  |
| 326 | `addi r1, r31, 0xc0` | `addi r1, r31, 0xc0` |  |
| 327 | `b __restgprlr_22` | `b __restgprlr_22` |  |



## Shift Semantics

- [246] ?

## Detected Patterns

- **unknown**

## Key Mismatches (15 of 71 shown)

- [146] diff_arg: `addi` [off:-4]
- [165] diff_arg: `lwz` [reg:r8->r9]
- [166] diff_arg: `lwzx` [reg:r9->r8]
- [167] insert: `stw      r7, 0x5c, r31`
- [169] insert: `stw      r7, 0x5c, r31`
- [174] diff_arg: `stb` [off:-8]
- [195] diff_arg: `lwz` [reg:r7->r8]
- [197] diff_arg: `clrlwi` [reg:r8->r7]
- [198] diff_arg: `lwz` [reg:r7->r8, reg:r7->r8]
- [200] diff_arg: `lwz` [reg:r7->r8]
- [203] diff_arg: `lhzu` [reg:r7->r8]
- [205] diff_arg: `rotlwi` [reg:r7->r8, reg:r7->r8]
- [206] diff_arg: `lwzx` [reg:r7->r8]
- [207] diff_arg: `lhz` [reg:r7->r8]
- [208] diff_arg: `addi` [reg:r7->r5, reg:r7->r8, off:-4]

*(Use `run_diff_inspect mode: "mismatches"` for full list)*

**Stack:** 1 DIFFER — different variables in same slots. Run `run_diff_inspect mode=stack-layout` for the full table.

## Auto-Diagnosis (diff_inspect)

======================================================================
DIAGNOSIS REPORT
======================================================================

Total instructions: 328
Match estimate:     ~78.4% (257/328 equal)

Instruction breakdown:
  equal       :   257 ( 78.4%)
  diff_arg    :    50 ( 15.2%)
  delete      :    12 (  3.7%)
  insert      :     8 (  2.4%)
  diff_op     :     1 (  0.3%)

----------------------------------------------------------------------
ROOT CAUSES
----------------------------------------------------------------------

  Stack/offset shift: dominant delta = -8 (3 instructions)
  Top offset deltas:
        -8:    3 instructions
        -4:    2 instructions
       -44:    1 instructions
        -3:    1 instructions

  Register swaps: 68 instructions across 17 pairs
  Top swap pairs:
    r7   <-> r8  :   14 (idx 195-308) [GPR]
    r8   <-> r9  :   10 (idx 165-310) [GPR]
    r26  <-> r27 :   10 (idx 237-319) [GPR]
    r11  <-> r9  :    7 (idx 240-253) [GPR]
    r7   <-> r9  :    4 (idx 258-266) [GPR]
    r11  <-> r5  :    4 (idx 266-287) [GPR]
    r28  <-> r29 :    3 (idx 234-287) [GPR]
    r28  <-> r3  :    3 (idx 246-313) [GPR]
    r5   <-> r7  :    2 (idx 208-212) [GPR]
    r29  <-> r3  :    2 (idx 239-245) [GPR]
    r11  <-> r8  :    2 (idx 241-247) [GPR]
    f0   <-> f13 :    2 (idx 275-277) [FPR]

----------------------------------------------------------------------
ACTIONABLE MISMATCHES
----------------------------------------------------------------------

  diff_op (opcode mismatches): 1
    idx  211: TGT slwi       r7, r7, 1
             SRC clrlwi     r8, r8, 16

  insert/delete: 20 instructions in 6 clusters
    cluster 1: idx 167-169 (2 instrs: 2I/0D)
    cluster 2: idx 209-214 (4 instrs: 2I/2D)
    cluster 3: idx 243-243 (1 instrs: 0I/1D)
    cluster 4: idx 251-257 (4 instrs: 1I/3D)
    cluster 5: idx 267-267 (1 instrs: 0I/1D)
    cluster 6: idx 283-295 (8 instrs: 3I/5D)

----------------------------------------------------------------------
NOISE BUDGET
----------------------------------------------------------------------

  diff_arg instructions: 50
    Explained by root causes: 50
      Offset shifts:     7 arg diffs
      Register swaps:    68 arg diffs
      Symbol relocs:     0 arg diffs
      Branch dests:      0 arg diffs
    Unexplained:         0

  Other non-equal: 21
    diff_op:   1
    replace:   0
    insert:    8
    delete:    12

[stderr]
Building incremental: build/45410914/src/system/bandobj/BandPatchMesh.obj
Loaded 5 ICF equivalence entries from /home/free/code/milohax/rb3-xenon/build/45410914/icf_aliases.map