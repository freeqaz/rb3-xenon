# Diff: bool __cdecl ParseNode(void)

- **Symbol**: `?ParseNode@@YA_NXZ`
- **Demangled**: `bool __cdecl ParseNode(void)`
- **Match**: 98.5% normalized (97.7% raw)
  - High-match band. Run the source permuter as the first action (regswaps, FPR scheduling, and bool materialization cascade here). Hand-edit fallbacks: variable reorder, inline assignment, member-cache hoists.
- **Target Size**: 2432 bytes
- **Base Size**: 2444 bytes
- **Diff Score**: 921 / 60800

## Instruction Summary

| Type | Count | Percent |
|------|------:|--------:|
| equal | 553 | 90.2% |
| diff_arg | 51 | 8.3% |
| diff_op | 1 | 0.2% |
| replace | 1 | 0.2% |
| delete | 2 | 0.3% |
| insert | 5 | 0.8% |
| **Total** | 613 | 100.0% |

## Region Summary

| Region | Instructions | Match % | Notes |
|--------|------------:|--------:|-------|
| 0-31 | 32 | 100% |  |
| 32-32 | 1 | 0% |  |
| 33-61 | 29 | 100% |  |
| 62-62 | 1 | 0% |  |
| 63-88 | 26 | 100% |  |
| 89-98 | 10 | 50% |  |
| 99-111 | 13 | 100% |  |
| 112-112 | 1 | 0% |  |
| 113-132 | 20 | 100% |  |
| 133-133 | 1 | 0% |  |
| 134-155 | 22 | 100% |  |
| 156-165 | 10 | 60% |  |
| 166-205 | 40 | 100% |  |
| 206-211 | 6 | 33% |  |
| 212-226 | 15 | 100% |  |
| 227-234 | 8 | 50% | 3 offset swaps, 1 addr relocation, 1 deletes |
| 235-246 | 12 | 100% |  |
| 247-247 | 1 | 0% |  |
| 248-386 | 139 | 100% |  |
| 387-413 | 27 | 19% | 17 register swaps, 1 addr relocation, 4 inserts |
| 414-427 | 14 | 100% |  |
| 428-435 | 8 | 50% | 3 offset swaps, 2 addr relocation |
| 436-448 | 13 | 100% |  |
| 449-455 | 7 | 14% | 1 control flow, 1 inserts, 1 deletes |
| 456-463 | 8 | 100% |  |
| 464-464 | 1 | 0% | 1 commutative ops |
| 465-482 | 18 | 100% |  |
| 483-487 | 5 | 40% |  |
| 488-523 | 36 | 100% |  |
| 524-524 | 1 | 0% |  |
| 525-572 | 48 | 100% |  |
| 573-573 | 1 | 0% |  |
| 574-612 | 39 | 100% |  |

## Patterns Detected

- **REGISTER_SWAP** (RarelyHandFixable): 20 instructions, 1 pair (r10↔r9) [volatile — try permuter sweep] [docs](docs/decomp/patterns/permuter-roi.md#register-allocation-cascades)
  - r10↔r9: 20
- **CONTROL_FLOW** (LikelyFixable): 1 condition inversion(s) (bne↔beq) [docs](docs/decomp/patterns/fixable-control-flow.md#branch-polarity-steering-beqbne-blebge)
  - idx 454: bne vs beq (diff_op)
- **COMMUTATIVE_OP_ORDER** (LikelyFixable): 1 commutative operand swap(s) [docs](docs/decomp/patterns/fixable-operators.md#commutative-operand-order)
  - idx 464: add (r30,r11 vs r11,r30)
- **OFFSET_SWAP** (LikelyFixable): 3 offset swaps, dominated by (0xe8,0xf0) x2 [docs](docs/decomp/patterns/fixable-declarations.md#offset-swap)
  - (0xe8,0xf0): 2 swap(s)
  - (0xec,0xf4): 1 swap(s)
- **ADDRESS_RELOCATION_NOISE** (RarelyHandFixable): 26 address relocation(s), 0 lis/addi pair(s) (linker artifact — different .text layout) [docs](docs/decomp/patterns/at-limit-mwcc.md#address-relocation-noise)

**Unattributed mismatches**: 6 | **Patterns checked**: 21

## Function Call Diff

**Target only:** `?erase@?$list@PBDV?$StlNodeAlloc@PBD@stlpmtx_std@@@stlpmtx_std@@QAA?AU?$_List_iterator@PBDU?$_Nonconst_traits@PBD@stlpmtx_std@@@2@U32@@Z` (1), `fn_82743E10` (1), `fn_827443D0` (2), `fn_82745BD0` (2)
**Base only:** `?DataMergeTags@@YAXPAVDataArray@@0@Z` (1), `?DataSetMacro@@YAXVSymbol@@PAVDataArray@@@Z` (2), `?ReadEmbeddedFile@@YAPAVDataArray@@PBD_N@Z` (2), `?erase@?$list@_NV?$StlNodeAlloc@_N@stlpmtx_std@@@stlpmtx_std@@QAA?AU?$_List_iterator@_NU?$_Nonconst_traits@_N@stlpmtx_std@@@2@U32@@Z` (1)

## Insert/Delete Clusters

| Range | Inserts | Deletes | Dominant Opcodes |
|-------|--------:|--------:|------------------|
| 401-404 | 4 | 0 | b, li, bne |

## Verdict: LikelyFixable (Medium confidence)

2 control flow difference(s) detected with low merged ratio (0.0%).

### Verdict Factors

| Factor | Value | Threshold | Result |
|--------|-------|-----------|--------|
| bool_mask_detected | false | - | not_detected |
| merged_call_ratio | 0.00 | 0.8 | below_threshold |
| control_flow_diffs | 2.00 | 1.0 | detected |

**Recommendation**: Investigate control flow structure.

### Suggestions

1. 1 condition inversion(s) (bne↔beq) ([docs](docs/decomp/patterns/fixable-control-flow.md#branch-polarity-steering-beqbne-blebge))
2. idx 454: bne vs beq (diff_op)
3. Try `> 0` vs `!= 0`, `>=` vs `>`, if/else inversion ([docs](docs/decomp/patterns/fixable-comparison.md#unsigned-zero-comparison))

### Related Documentation

- [docs/decomp/patterns/at-limit-mwcc.md#address-relocation-noise](docs/decomp/patterns/at-limit-mwcc.md#address-relocation-noise)
- [docs/decomp/patterns/fixable-comparison.md#unsigned-zero-comparison](docs/decomp/patterns/fixable-comparison.md#unsigned-zero-comparison)
- [docs/decomp/patterns/fixable-control-flow.md#branch-polarity-steering-beqbne-blebge](docs/decomp/patterns/fixable-control-flow.md#branch-polarity-steering-beqbne-blebge)
- [docs/decomp/patterns/fixable-declarations.md#offset-swap](docs/decomp/patterns/fixable-declarations.md#offset-swap)
- [docs/decomp/patterns/fixable-declarations.md#variable-declaration-order](docs/decomp/patterns/fixable-declarations.md#variable-declaration-order)
- [docs/decomp/patterns/fixable-operators.md#commutative-operand-order](docs/decomp/patterns/fixable-operators.md#commutative-operand-order)
- [docs/decomp/patterns/permuter-roi.md#register-allocation-cascades](docs/decomp/patterns/permuter-roi.md#register-allocation-cascades)
- [docs/decomp/patterns/permuter-roi.md#stack-slot-inversion](docs/decomp/patterns/permuter-roi.md#stack-slot-inversion)

## Full Instruction Listing

| Index | Target | Base | Match |
|------:|--------|------|-------|
| 0 | `mflr r12` | `mflr r12` |  |
| 1 | `bl __savegprlr_25` | `bl __savegprlr_25` |  |
| 2 | `subi r31, r1, 0x150` | `subi r31, r1, 0x150` |  |
| 3 | `stwu r1, -0x150, r1` | `stwu r1, -0x150, r1` |  |
| 4 | `bl yylex` | `bl yylex` |  |
| 5 | `lis r11, lbl_82DA0017` | `lis r11, ?gConditional@@3V?$list@_NV?$StlNodeAlloc@_N@stlpmtx_std@@@stlpmtx_std@@A` |  |
| 6 | `li r27, 0x0` | `li r27, 0x0` |  |
| 7 | `addi r25, r11, lbl_82DA0017` | `addi r25, r11, ?gConditional@@3V?$list@_NV?$StlNodeAlloc@_N@stlpmtx_std@@@stlpmtx_std@@A` |  |
| 8 | `mr r28, r3` | `mr r28, r3` |  |
| 9 | `lwz r11, lbl_82DA0017, r11` | `lwz r11, ?gConditional@@3V?$list@_NV?$StlNodeAlloc@_N@stlpmtx_std@@@stlpmtx_std@@A, r11` |  |
| 10 | `b 0x3c` | `b 0x44` |  |
| 11 | `lbz r10, 0x8, r11` | `lbz r10, 0x8, r11` |  |
| 12 | `cmplwi r10, 0x0` | `cmplwi r10, 0x0` |  |
| 13 | `beq 0xb8` | `beq 0xc0` |  |
| 14 | `lwz r11, 0x0, r11` | `lwz r11, 0x0, r11` |  |
| 15 | `cmplw cr6, r11, r25` | `cmplw cr6, r11, r25` |  |
| 16 | `bne cr6, 0x2c` | `bne cr6, 0x34` |  |
| 17 | `li r11, 0x1` | `li r11, 0x1` |  |
| 18 | `clrlwi. r11, r11, 24` | `clrlwi. r11, r11, 24` |  |
| 19 | `bne 0x70` | `bne 0x78` |  |
| 20 | `cmpwi cr6, r28, 0x14` | `cmpwi cr6, r28, 0x14` |  |
| 21 | `beq cr6, 0x70` | `beq cr6, 0x78` |  |
| 22 | `cmpwi cr6, r28, 0x16` | `cmpwi cr6, r28, 0x16` |  |
| 23 | `beq cr6, 0x70` | `beq cr6, 0x78` |  |
| 24 | `cmpwi cr6, r28, 0x17` | `cmpwi cr6, r28, 0x17` |  |
| 25 | `beq cr6, 0x70` | `beq cr6, 0x78` |  |
| 26 | `cmpwi cr6, r28, 0x18` | `cmpwi cr6, r28, 0x18` |  |
| 27 | `bne cr6, 0x974` | `bne cr6, 0x988` |  |
| 28 | `lis r11, lbl_82DA0017` | `lis r11, gCachingFile` |  |
| 29 | `lis r26, lbl_82E278A8` | `lis r26, yyleng` |  |
| 30 | `addi r30, r11, lbl_82DA0017` | `addi r30, r11, gCachingFile` |  |
| 31 | `lis r29, lbl_82E278A4` | `lis r29, yytext` |  |
| 32 | `lwz r11, 0x4, r30, lbl_82DA0017` | `lwz r11, 0xf, r30, gParse` | diff_arg |
| 33 | `cmpwi cr6, r11, 0x0` | `cmpwi cr6, r11, 0x0` |  |
| 34 | `bne cr6, 0xc0` | `bne cr6, 0xc8` |  |
| 35 | `lis r11, except_record_82749C40` | `lis r11, ?bom@?4??ParseNode@@YA_NXZ@4QBDB` |  |
| 36 | `lwz r3, lbl_82E278A4, r29` | `lwz r3, yytext, r29` |  |
| 37 | `li r5, 0x3` | `li r5, 0x3` |  |
| 38 | `addi r4, r11, except_record_82749C40` | `addi r4, r11, ?bom@?4??ParseNode@@YA_NXZ@4QBDB` |  |
| 39 | `bl strncmp` | `bl strncmp` |  |
| 40 | `cmpwi r3, 0x0` | `cmpwi r3, 0x0` |  |
| 41 | `bne 0xc0` | `bne 0xc8` |  |
| 42 | `lwz r10, lbl_82E278A8, r26` | `lwz r10, yyleng, r26` |  |
| 43 | `cmpwi cr6, r10, 0x3` | `cmpwi cr6, r10, 0x3` |  |
| 44 | `bgt cr6, 0xc4` | `bgt cr6, 0xcc` |  |
| 45 | `b 0x974` | `b 0x988` |  |
| 46 | `mr r11, r27` | `mr r11, r27` |  |
| 47 | `b 0x48` | `b 0x50` |  |
| 48 | `lwz r10, lbl_82E278A8, r26` | `lwz r10, yyleng, r26` |  |
| 49 | `cmpwi cr6, r28, 0x0` | `cmpwi cr6, r28, 0x0` |  |
| 50 | `bne cr6, 0xd4` | `bne cr6, 0xdc` |  |
| 51 | `li r3, 0x0` | `li r3, 0x0` |  |
| 52 | `b 0x978` | `b 0x98c` |  |
| 53 | `cmpwi cr6, r28, 0x8` | `cmpwi cr6, r28, 0x8` |  |
| 54 | `beq cr6, 0xcc` | `beq cr6, 0xd4` |  |
| 55 | `cmpwi cr6, r28, 0xa` | `cmpwi cr6, r28, 0xa` |  |
| 56 | `beq cr6, 0xcc` | `beq cr6, 0xd4` |  |
| 57 | `cmpwi cr6, r28, 0xc` | `cmpwi cr6, r28, 0xc` |  |
| 58 | `beq cr6, 0xcc` | `beq cr6, 0xd4` |  |
| 59 | `cmpwi cr6, r28, 0x11` | `cmpwi cr6, r28, 0x11` |  |
| 60 | `bne cr6, 0x198` | `bne cr6, 0x1a0` |  |
| 61 | `bl yylex` | `bl yylex` |  |
| 62 | `lbz r11, 0x10, r30, lbl_82DA0017` | `lbz r11, 0x0, r30, gCachingFile` | diff_arg |
| 63 | `lwz r4, lbl_82E278A4, r29` | `lwz r4, yytext, r29` |  |
| 64 | `addi r3, r31, 0x54` | `addi r3, r31, 0x54` |  |
| 65 | `cmplwi r11, 0x0` | `cmplwi r11, 0x0` |  |
| 66 | `beq 0x13c` | `beq 0x144` |  |
| 67 | `bl ??0Symbol@@QAA@PBD@Z` | `bl ??0Symbol@@QAA@PBD@Z` |  |
| 68 | `lwz r11, 0x0, r3` | `lwz r11, 0x0, r3` |  |
| 69 | `li r10, 0x22` | `li r10, 0x22` |  |
| 70 | `stw r10, 0xe4, r31` | `stw r10, 0xe4, r31` |  |
| 71 | `stw r11, 0xe0, r31` | `stw r11, 0xe0, r31` |  |
| 72 | `addi r3, r31, 0xe0` | `addi r3, r31, 0xe0` |  |
| 73 | `bl ?PushBack@@YAXABVDataNode@@@Z` | `bl ?PushBack@@YAXABVDataNode@@@Z` |  |
| 74 | `lwz r11, 0xe4, r31` | `lwz r11, 0xe4, r31` |  |
| 75 | `rlwinm. r11, r11, 0, 27, 27` | `rlwinm. r11, r11, 0, 27, 27` |  |
| 76 | `beq 0x974` | `beq 0x988` |  |
| 77 | `lwz r3, 0xe0, r31` | `lwz r3, 0xe0, r31` |  |
| 78 | `b 0x970` | `b 0x984` |  |
| 79 | `bl ??0Symbol@@QAA@PBD@Z` | `bl ??0Symbol@@QAA@PBD@Z` |  |
| 80 | `lwz r3, 0x0, r3` | `lwz r3, 0x0, r3` |  |
| 81 | `bl ?DataGetMacro@@YAPAVDataArray@@VSymbol@@@Z` | `bl ?DataGetMacro@@YAPAVDataArray@@VSymbol@@@Z` |  |
| 82 | `mr. r28, r3` | `mr. r28, r3` |  |
| 83 | `bne 0x164` | `bne 0x16c` |  |
| 84 | `li r4, 0x1` | `li r4, 0x1` |  |
| 85 | `lwz r3, lbl_82E278A4, r29` | `lwz r3, yytext, r29` |  |
| 86 | `bl fn_82745BD0` | `bl ?ReadEmbeddedFile@@YAPAVDataArray@@PBD_N@Z` |  |
| 87 | `mr r28, r3` | `mr r28, r3` |  |
| 88 | `li r27, 0x1` | `li r27, 0x1` |  |
| 89 | `lwz r4, 0x4, r30, lbl_82DA0017` | `lwz r4, 0xf, r30, gParse` | diff_arg |
| 90 | `lwz r3, 0x0, r30, lbl_82DA0017` | `lwz r3, 0xb, r30, gParse` | diff_arg |
| 91 | `bl ?Resize@DataArray@@QAAXH@Z` | `bl ?Resize@DataArray@@QAAXH@Z` |  |
| 92 | `mr r4, r28` | `mr r4, r28` |  |
| 93 | `lwz r3, 0x0, r30, lbl_82DA0017` | `lwz r3, 0xb, r30, gParse` | diff_arg |
| 94 | `bl fn_82743E10` | `bl ?DataMergeTags@@YAXPAVDataArray@@0@Z` |  |
| 95 | `lwz r11, 0x0, r30, lbl_82DA0017` | `lwz r11, 0xb, r30, gParse` | diff_arg |
| 96 | `clrlwi. r10, r27, 24` | `clrlwi. r10, r27, 24` |  |
| 97 | `lha r11, 0x8, r11` | `lha r11, 0x8, r11` |  |
| 98 | `stw r11, 0x4, r30, lbl_82DA0017` | `stw r11, 0xf, r30, gParse` | diff_arg |
| 99 | `beq 0x974` | `beq 0x988` |  |
| 100 | `mr r3, r28` | `mr r3, r28` |  |
| 101 | `b 0x970` | `b 0x984` |  |
| 102 | `cmpwi cr6, r28, 0xf` | `cmpwi cr6, r28, 0xf` |  |
| 103 | `beq cr6, 0x8d0` | `beq cr6, 0x8e4` |  |
| 104 | `cmpwi cr6, r28, 0x10` | `cmpwi cr6, r28, 0x10` |  |
| 105 | `beq cr6, 0x8d0` | `beq cr6, 0x8e4` |  |
| 106 | `cmpwi cr6, r28, 0x14` | `cmpwi cr6, r28, 0x14` |  |
| 107 | `beq cr6, 0x7d8` | `beq cr6, 0x7ec` |  |
| 108 | `cmpwi cr6, r28, 0x16` | `cmpwi cr6, r28, 0x16` |  |
| 109 | `beq cr6, 0x7d8` | `beq cr6, 0x7ec` |  |
| 110 | `cmpwi cr6, r28, 0x17` | `cmpwi cr6, r28, 0x17` |  |
| 111 | `bne cr6, 0x20c` | `bne cr6, 0x214` |  |
| 112 | `lbz r11, 0x10, r30, lbl_82DA0017` | `lbz r11, 0x0, r30, gCachingFile` | diff_arg |
| 113 | `cmplwi r11, 0x0` | `cmplwi r11, 0x0` |  |
| 114 | `beq 0x1f4` | `beq 0x1fc` |  |
| 115 | `li r11, 0x8` | `li r11, 0x8` |  |
| 116 | `stw r27, 0x78, r31` | `stw r27, 0x78, r31` |  |
| 117 | `stw r11, 0x7c, r31` | `stw r11, 0x7c, r31` |  |
| 118 | `addi r3, r31, 0x78` | `addi r3, r31, 0x78` |  |
| 119 | `bl ?PushBack@@YAXABVDataNode@@@Z` | `bl ?PushBack@@YAXABVDataNode@@@Z` |  |
| 120 | `lwz r11, 0x7c, r31` | `lwz r11, 0x7c, r31` |  |
| 121 | `rlwinm. r11, r11, 0, 27, 27` | `rlwinm. r11, r11, 0, 27, 27` |  |
| 122 | `beq 0x974` | `beq 0x988` |  |
| 123 | `lwz r3, 0x78, r31` | `lwz r3, 0x78, r31` |  |
| 124 | `b 0x970` | `b 0x984` |  |
| 125 | `lwz r11, 0x4, r25, lbl_82DA0017` | `lwz r11, 0x4, r25, ?gConditional@@3V?$list@_NV?$StlNodeAlloc@_N@stlpmtx_std@@@stlpmtx_std@@A` |  |
| 126 | `lbz r10, 0x8, r11` | `lbz r10, 0x8, r11` |  |
| 127 | `cntlzw r10, r10` | `cntlzw r10, r10` |  |
| 128 | `extrwi r10, r10, 1, 26` | `extrwi r10, r10, 1, 26` |  |
| 129 | `stb r10, 0x8, r11` | `stb r10, 0x8, r11` |  |
| 130 | `b 0x974` | `b 0x988` |  |
| 131 | `cmpwi cr6, r28, 0x18` | `cmpwi cr6, r28, 0x18` |  |
| 132 | `bne cr6, 0x264` | `bne cr6, 0x26c` |  |
| 133 | `lbz r11, 0x10, r30, lbl_82DA0017` | `lbz r11, 0x0, r30, gCachingFile` | diff_arg |
| 134 | `cmplwi r11, 0x0` | `cmplwi r11, 0x0` |  |
| 135 | `beq 0x248` | `beq 0x250` |  |
| 136 | `li r11, 0x9` | `li r11, 0x9` |  |
| 137 | `stw r27, 0xd0, r31` | `stw r27, 0xd0, r31` |  |
| 138 | `stw r11, 0xd4, r31` | `stw r11, 0xd4, r31` |  |
| 139 | `addi r3, r31, 0xd0` | `addi r3, r31, 0xd0` |  |
| 140 | `bl ?PushBack@@YAXABVDataNode@@@Z` | `bl ?PushBack@@YAXABVDataNode@@@Z` |  |
| 141 | `lwz r11, 0xd4, r31` | `lwz r11, 0xd4, r31` |  |
| 142 | `rlwinm. r11, r11, 0, 27, 27` | `rlwinm. r11, r11, 0, 27, 27` |  |
| 143 | `beq 0x974` | `beq 0x988` |  |
| 144 | `lwz r3, 0xd0, r31` | `lwz r3, 0xd0, r31` |  |
| 145 | `b 0x970` | `b 0x984` |  |
| 146 | `lwz r11, 0x4, r25, lbl_82DA0017` | `lwz r11, 0x4, r25, ?gConditional@@3V?$list@_NV?$StlNodeAlloc@_N@stlpmtx_std@@@stlpmtx_std@@A` |  |
| 147 | `mr r4, r25, lbl_82DA0017` | `mr r4, r25, ?gConditional@@3V?$list@_NV?$StlNodeAlloc@_N@stlpmtx_std@@@stlpmtx_std@@A` |  |
| 148 | `addi r5, r31, 0x54` | `addi r5, r31, 0x54` |  |
| 149 | `addi r3, r31, 0x5c` | `addi r3, r31, 0x5c` |  |
| 150 | `stw r11, 0x54, r31` | `stw r11, 0x54, r31` |  |
| 151 | `bl ?erase@?$list@PBDV?$StlNodeAlloc@PBD@stlpmtx_std@@@stlpmtx_std@@QAA?AU?$_List_iterator@PBDU?$_Nonconst_traits@PBD@stlpmtx_std@@@2@U32@@Z` | `bl ?erase@?$list@_NV?$StlNodeAlloc@_N@stlpmtx_std@@@stlpmtx_std@@QAA?AU?$_List_iterator@_NU?$_Nonconst_traits@_N@stlpmtx_std@@@2@U32@@Z` |  |
| 152 | `b 0x974` | `b 0x988` |  |
| 153 | `cmpwi cr6, r28, 0xe` | `cmpwi cr6, r28, 0xe` |  |
| 154 | `bne cr6, 0x31c` | `bne cr6, 0x324` |  |
| 155 | `bl yylex` | `bl yylex` |  |
| 156 | `lwz r28, 0xc, r30, lbl_82DA0017` | `lwz r28, 0x3, r30, gOpenArray` | diff_arg |
| 157 | `stw r3, 0xc, r30, lbl_82DA0017` | `stw r3, 0x3, r30, gOpenArray` | diff_arg |
| 158 | `bl ?ParseArray@@YAPAVDataArray@@XZ` | `bl ?ParseArray@@YAPAVDataArray@@XZ` |  |
| 159 | `stw r28, 0xc, r30, lbl_82DA0017` | `stw r28, 0x3, r30, gOpenArray` | diff_arg |
| 160 | `mr r29, r3` | `mr r29, r3` |  |
| 161 | `mr r4, r3` | `mr r4, r3` |  |
| 162 | `li r5, 0x11` | `li r5, 0x11` |  |
| 163 | `addi r3, r31, 0xc8` | `addi r3, r31, 0xc8` |  |
| 164 | `bl ??0DataNode@@QAA@PAVDataArray@@W4DataType@@@Z` | `bl ??0DataNode@@QAA@PAVDataArray@@W4DataType@@@Z` |  |
| 165 | `lbz r11, 0x10, r30, lbl_82DA0017` | `lbz r11, 0x0, r30, gCachingFile` | diff_arg |
| 166 | `cmplwi r11, 0x0` | `cmplwi r11, 0x0` |  |
| 167 | `beq 0x2d4` | `beq 0x2dc` |  |
| 168 | `li r11, 0x24` | `li r11, 0x24` |  |
| 169 | `stw r27, 0x88, r31` | `stw r27, 0x88, r31` |  |
| 170 | `stw r11, 0x8c, r31` | `stw r11, 0x8c, r31` |  |
| 171 | `addi r3, r31, 0x88` | `addi r3, r31, 0x88` |  |
| 172 | `bl ?PushBack@@YAXABVDataNode@@@Z` | `bl ?PushBack@@YAXABVDataNode@@@Z` |  |
| 173 | `lwz r11, 0x8c, r31` | `lwz r11, 0x8c, r31` |  |
| 174 | `rlwinm. r11, r11, 0, 27, 27` | `rlwinm. r11, r11, 0, 27, 27` |  |
| 175 | `beq 0x2c8` | `beq 0x2d0` |  |
| 176 | `lwz r3, 0x88, r31` | `lwz r3, 0x88, r31` |  |
| 177 | `bl ?Release@DataArray@@QAAXXZ` | `bl ?Release@DataArray@@QAAXXZ` |  |
| 178 | `addi r3, r31, 0xc8` | `addi r3, r31, 0xc8` |  |
| 179 | `bl ?PushBack@@YAXABVDataNode@@@Z` | `bl ?PushBack@@YAXABVDataNode@@@Z` |  |
| 180 | `b 0x300` | `b 0x308` |  |
| 181 | `mr r4, r29` | `mr r4, r29` |  |
| 182 | `addi r3, r31, 0xc8` | `addi r3, r31, 0xc8` |  |
| 183 | `bl ?Command@DataNode@@QBAPAVDataArray@@PBV2@@Z` | `bl ?Command@DataNode@@QBAPAVDataArray@@PBV2@@Z` |  |
| 184 | `mr r4, r3` | `mr r4, r3` |  |
| 185 | `addi r3, r31, 0xf8` | `addi r3, r31, 0xf8` |  |
| 186 | `bl ?Execute@DataArray@@QAA?AVDataNode@@XZ` | `bl ?Execute@DataArray@@QAA?AVDataNode@@XZ` |  |
| 187 | `lwz r11, 0xfc, r31` | `lwz r11, 0xfc, r31` |  |
| 188 | `rlwinm. r11, r11, 0, 27, 27` | `rlwinm. r11, r11, 0, 27, 27` |  |
| 189 | `beq 0x300` | `beq 0x308` |  |
| 190 | `lwz r3, 0xf8, r31` | `lwz r3, 0xf8, r31` |  |
| 191 | `bl ?Release@DataArray@@QAAXXZ` | `bl ?Release@DataArray@@QAAXXZ` |  |
| 192 | `mr r3, r29` | `mr r3, r29` |  |
| 193 | `bl ?Release@DataArray@@QAAXXZ` | `bl ?Release@DataArray@@QAAXXZ` |  |
| 194 | `lwz r11, 0xcc, r31` | `lwz r11, 0xcc, r31` |  |
| 195 | `rlwinm. r11, r11, 0, 27, 27` | `rlwinm. r11, r11, 0, 27, 27` |  |
| 196 | `beq 0x974` | `beq 0x988` |  |
| 197 | `lwz r3, 0xc8, r31` | `lwz r3, 0xc8, r31` |  |
| 198 | `b 0x970` | `b 0x984` |  |
| 199 | `cmpwi cr6, r28, 0xd` | `cmpwi cr6, r28, 0xd` |  |
| 200 | `bne cr6, 0x3c4` | `bne cr6, 0x3c8` |  |
| 201 | `bl yylex` | `bl yylex` |  |
| 202 | `addi r3, r31, 0x60` | `addi r3, r31, 0x60` |  |
| 203 | `lwz r4, lbl_82E278A4, r29` | `lwz r4, yytext, r29` |  |
| 204 | `bl ??0Symbol@@QAA@PBD@Z` | `bl ??0Symbol@@QAA@PBD@Z` |  |
| 205 | `bl yylex` | `bl yylex` |  |
| 206 | `lwz r29, 0xc, r30, lbl_82DA0017` | `lwz r29, 0x3, r30, gOpenArray` | diff_arg |
| 207 | `stw r3, 0xc, r30, lbl_82DA0017` | `stw r3, 0x3, r30, gOpenArray` | diff_arg |
| 208 | `bl ?ParseArray@@YAPAVDataArray@@XZ` | `bl ?ParseArray@@YAPAVDataArray@@XZ` |  |
| 209 | `lbz r11, 0x10, r30, lbl_82DA0017` | `lbz r11, 0x0, r30, gCachingFile` | diff_arg |
| 210 | `mr r28, r3` | `mr r28, r3` |  |
| 211 | `stw r29, 0xc, r30, lbl_82DA0017` | `stw r29, 0x3, r30, gOpenArray` | diff_arg |
| 212 | `cmplwi r11, 0x0` | `cmplwi r11, 0x0` |  |
| 213 | `beq 0x3b4` | `beq 0x3b8` |  |
| 214 | `lwz r11, 0x60, r31` | `lwz r11, 0x60, r31` |  |
| 215 | `li r10, 0x20` | `li r10, 0x20` |  |
| 216 | `stw r10, 0x9c, r31` | `stw r10, 0x9c, r31` |  |
| 217 | `stw r11, 0x98, r31` | `stw r11, 0x98, r31` |  |
| 218 | `addi r3, r31, 0x98` | `addi r3, r31, 0x98` |  |
| 219 | `bl ?PushBack@@YAXABVDataNode@@@Z` | `bl ?PushBack@@YAXABVDataNode@@@Z` |  |
| 220 | `lwz r11, 0x9c, r31` | `lwz r11, 0x9c, r31` |  |
| 221 | `rlwinm. r11, r11, 0, 27, 27` | `rlwinm. r11, r11, 0, 27, 27` |  |
| 222 | `beq 0x384` | `beq 0x38c` |  |
| 223 | `lwz r3, 0x98, r31` | `lwz r3, 0x98, r31` |  |
| 224 | `bl ?Release@DataArray@@QAAXXZ` | `bl ?Release@DataArray@@QAAXXZ` |  |
| 225 | `li r5, 0x10` | `li r5, 0x10` |  |
| 226 | `mr r4, r28` | `mr r4, r28` |  |
| 227 | `addi r3, r31, 0xe8` | `addi r3, r31, 0xf0` | diff_arg |
| 228 | `bl ??0DataNode@@QAA@PAVDataArray@@W4DataType@@@Z` | `bl ??0DataNode@@QAA@PAVDataArray@@W4DataType@@@Z` |  |
| 229 | `addi r3, r31, 0xe8` | - | delete |
| 230 | `bl ?PushBack@@YAXABVDataNode@@@Z` | `bl ?PushBack@@YAXABVDataNode@@@Z` |  |
| 231 | `lwz r11, 0xec, r31` | `lwz r11, 0xf4, r31` | diff_arg |
| 232 | `rlwinm. r11, r11, 0, 27, 27` | `rlwinm. r11, r11, 0, 27, 27` |  |
| 233 | `beq 0x190` | `beq 0x198` |  |
| 234 | `lwz r3, 0xe8, r31` | `lwz r3, 0xf0, r31` | diff_arg |
| 235 | `bl ?Release@DataArray@@QAAXXZ` | `bl ?Release@DataArray@@QAAXXZ` |  |
| 236 | `b 0x190` | `b 0x198` |  |
| 237 | `mr r4, r28` | `mr r4, r28` |  |
| 238 | `lwz r3, 0x60, r31` | `lwz r3, 0x60, r31` |  |
| 239 | `bl fn_827443D0` | `bl ?DataSetMacro@@YAXVSymbol@@PAVDataArray@@@Z` |  |
| 240 | `b 0x190` | `b 0x198` |  |
| 241 | `cmpwi cr6, r28, 0x15` | `cmpwi cr6, r28, 0x15` |  |
| 242 | `bne cr6, 0x424` | `bne cr6, 0x428` |  |
| 243 | `bl yylex` | `bl yylex` |  |
| 244 | `addi r3, r31, 0x64` | `addi r3, r31, 0x64` |  |
| 245 | `lwz r4, lbl_82E278A4, r29` | `lwz r4, yytext, r29` |  |
| 246 | `bl ??0Symbol@@QAA@PBD@Z` | `bl ??0Symbol@@QAA@PBD@Z` |  |
| 247 | `lbz r11, 0x10, r30, lbl_82DA0017` | `lbz r11, 0x0, r30, gCachingFile` | diff_arg |
| 248 | `cmplwi r11, 0x0` | `cmplwi r11, 0x0` |  |
| 249 | `beq 0x414` | `beq 0x418` |  |
| 250 | `lwz r11, 0x64, r31` | `lwz r11, 0x64, r31` |  |
| 251 | `li r10, 0x25` | `li r10, 0x25` |  |
| 252 | `stw r10, 0x6c, r31` | `stw r10, 0x6c, r31` |  |
| 253 | `stw r11, 0x68, r31` | `stw r11, 0x68, r31` |  |
| 254 | `addi r3, r31, 0x68` | `addi r3, r31, 0x68` |  |
| 255 | `bl ?PushBack@@YAXABVDataNode@@@Z` | `bl ?PushBack@@YAXABVDataNode@@@Z` |  |
| 256 | `lwz r11, 0x6c, r31` | `lwz r11, 0x6c, r31` |  |
| 257 | `rlwinm. r11, r11, 0, 27, 27` | `rlwinm. r11, r11, 0, 27, 27` |  |
| 258 | `beq 0x974` | `beq 0x988` |  |
| 259 | `lwz r3, 0x68, r31` | `lwz r3, 0x68, r31` |  |
| 260 | `b 0x970` | `b 0x984` |  |
| 261 | `li r4, 0x0` | `li r4, 0x0` |  |
| 262 | `lwz r3, 0x64, r31` | `lwz r3, 0x64, r31` |  |
| 263 | `bl fn_827443D0` | `bl ?DataSetMacro@@YAXVSymbol@@PAVDataArray@@@Z` |  |
| 264 | `b 0x974` | `b 0x988` |  |
| 265 | `cmpwi cr6, r28, 0x7` | `cmpwi cr6, r28, 0x7` |  |
| 266 | `beq cr6, 0x778` | `beq cr6, 0x78c` |  |
| 267 | `cmpwi cr6, r28, 0xb` | `cmpwi cr6, r28, 0xb` |  |
| 268 | `beq cr6, 0x778` | `beq cr6, 0x78c` |  |
| 269 | `cmpwi cr6, r28, 0x9` | `cmpwi cr6, r28, 0x9` |  |
| 270 | `beq cr6, 0x778` | `beq cr6, 0x78c` |  |
| 271 | `cmpwi cr6, r28, 0x12` | `cmpwi cr6, r28, 0x12` |  |
| 272 | `bne cr6, 0x484` | `bne cr6, 0x488` |  |
| 273 | `lwz r11, lbl_82E278A4, r29` | `lwz r11, yytext, r29` |  |
| 274 | `addi r3, r31, 0x5c` | `addi r3, r31, 0x5c` |  |
| 275 | `addi r4, r11, 0x1` | `addi r4, r11, 0x1` |  |
| 276 | `bl ??0Symbol@@QAA@PBD@Z` | `bl ??0Symbol@@QAA@PBD@Z` |  |
| 277 | `lwz r3, 0x0, r3` | `lwz r3, 0x0, r3` |  |
| 278 | `bl ?DataVariable@@YAAAVDataNode@@VSymbol@@@Z` | `bl ?DataVariable@@YAAAVDataNode@@VSymbol@@@Z` |  |
| 279 | `li r11, 0x2` | `li r11, 0x2` |  |
| 280 | `stw r3, 0xa8, r31` | `stw r3, 0xa8, r31` |  |
| 281 | `stw r11, 0xac, r31` | `stw r11, 0xac, r31` |  |
| 282 | `addi r3, r31, 0xa8` | `addi r3, r31, 0xa8` |  |
| 283 | `bl ?PushBack@@YAXABVDataNode@@@Z` | `bl ?PushBack@@YAXABVDataNode@@@Z` |  |
| 284 | `lwz r11, 0xac, r31` | `lwz r11, 0xac, r31` |  |
| 285 | `rlwinm. r11, r11, 0, 27, 27` | `rlwinm. r11, r11, 0, 27, 27` |  |
| 286 | `beq 0x974` | `beq 0x988` |  |
| 287 | `lwz r3, 0xa8, r31` | `lwz r3, 0xa8, r31` |  |
| 288 | `b 0x970` | `b 0x984` |  |
| 289 | `cmpwi cr6, r28, 0x13` | `cmpwi cr6, r28, 0x13` |  |
| 290 | `bne cr6, 0x4b4` | `bne cr6, 0x4b8` |  |
| 291 | `li r11, 0x6` | `li r11, 0x6` |  |
| 292 | `stw r27, 0xd8, r31` | `stw r27, 0xd8, r31` |  |
| 293 | `stw r11, 0xdc, r31` | `stw r11, 0xdc, r31` |  |
| 294 | `addi r3, r31, 0xd8` | `addi r3, r31, 0xd8` |  |
| 295 | `bl ?PushBack@@YAXABVDataNode@@@Z` | `bl ?PushBack@@YAXABVDataNode@@@Z` |  |
| 296 | `lwz r11, 0xdc, r31` | `lwz r11, 0xdc, r31` |  |
| 297 | `rlwinm. r11, r11, 0, 27, 27` | `rlwinm. r11, r11, 0, 27, 27` |  |
| 298 | `beq 0x974` | `beq 0x988` |  |
| 299 | `lwz r3, 0xd8, r31` | `lwz r3, 0xd8, r31` |  |
| 300 | `b 0x970` | `b 0x984` |  |
| 301 | `cmpwi cr6, r28, 0x3` | `cmpwi cr6, r28, 0x3` |  |
| 302 | `bne cr6, 0x4e8` | `bne cr6, 0x4ec` |  |
| 303 | `lwz r3, lbl_82E278A4, r29` | `lwz r3, yytext, r29` |  |
| 304 | `bl atoi` | `bl atoi` |  |
| 305 | `stw r3, 0xb8, r31` | `stw r3, 0xb8, r31` |  |
| 306 | `stw r27, 0xbc, r31` | `stw r27, 0xbc, r31` |  |
| 307 | `addi r3, r31, 0xb8` | `addi r3, r31, 0xb8` |  |
| 308 | `bl ?PushBack@@YAXABVDataNode@@@Z` | `bl ?PushBack@@YAXABVDataNode@@@Z` |  |
| 309 | `lwz r11, 0xbc, r31` | `lwz r11, 0xbc, r31` |  |
| 310 | `rlwinm. r11, r11, 0, 27, 27` | `rlwinm. r11, r11, 0, 27, 27` |  |
| 311 | `beq 0x974` | `beq 0x988` |  |
| 312 | `lwz r3, 0xb8, r31` | `lwz r3, 0xb8, r31` |  |
| 313 | `b 0x970` | `b 0x984` |  |
| 314 | `cmpwi cr6, r28, 0x1` | `cmpwi cr6, r28, 0x1` |  |
| 315 | `bne cr6, 0x590` | `bne cr6, 0x594` |  |
| 316 | `lwz r11, lbl_82E278A4, r29` | `lwz r11, yytext, r29` |  |
| 317 | `mr r10, r27` | `mr r10, r27` |  |
| 318 | `li r9, 0x1` | `li r9, 0x1` |  |
| 319 | `mr r8, r11` | `mr r8, r11` |  |
| 320 | `lbz r7, 0x0, r8` | `lbz r7, 0x0, r8` |  |
| 321 | `addi r8, r8, 0x1` | `addi r8, r8, 0x1` |  |
| 322 | `cmplwi cr6, r7, 0x0` | `cmplwi cr6, r7, 0x0` |  |
| 323 | `bne cr6, 0x500` | `bne cr6, 0x504` |  |
| 324 | `subf r8, r11, r8` | `subf r8, r11, r8` |  |
| 325 | `subi r8, r8, 0x1` | `subi r8, r8, 0x1` |  |
| 326 | `clrrwi r8, r8, 0` | `clrrwi r8, r8, 0` |  |
| 327 | `add r11, r8, r11` | `add r11, r8, r11` |  |
| 328 | `subi r8, r11, 0x1` | `subi r8, r11, 0x1` |  |
| 329 | `lbz r11, -0x1, r11` | `lbz r11, -0x1, r11` |  |
| 330 | `b 0x560` | `b 0x564` |  |
| 331 | `cmpwi cr6, r11, 0x61` | `cmpwi cr6, r11, 0x61` |  |
| 332 | `blt cr6, 0x53c` | `blt cr6, 0x540` |  |
| 333 | `subi r11, r11, 0x57` | `subi r11, r11, 0x57` |  |
| 334 | `b 0x550` | `b 0x554` |  |
| 335 | `cmpwi cr6, r11, 0x41` | `cmpwi cr6, r11, 0x41` |  |
| 336 | `ble cr6, 0x54c` | `ble cr6, 0x550` |  |
| 337 | `subi r11, r11, 0x37` | `subi r11, r11, 0x37` |  |
| 338 | `b 0x550` | `b 0x554` |  |
| 339 | `subi r11, r11, 0x30` | `subi r11, r11, 0x30` |  |
| 340 | `mullw r11, r11, r9` | `mullw r11, r11, r9` |  |
| 341 | `add r10, r11, r10` | `add r10, r11, r10` |  |
| 342 | `lbzu r11, -0x1, r8` | `lbzu r11, -0x1, r8` |  |
| 343 | `slwi r9, r9, 4` | `slwi r9, r9, 4` |  |
| 344 | `extsb r11, r11` | `extsb r11, r11` |  |
| 345 | `cmpwi cr6, r11, 0x78` | `cmpwi cr6, r11, 0x78` |  |
| 346 | `bne cr6, 0x52c` | `bne cr6, 0x530` |  |
| 347 | `stw r10, 0x70, r31` | `stw r10, 0x70, r31` |  |
| 348 | `stw r27, 0x74, r31` | `stw r27, 0x74, r31` |  |
| 349 | `addi r3, r31, 0x70` | `addi r3, r31, 0x70` |  |
| 350 | `bl ?PushBack@@YAXABVDataNode@@@Z` | `bl ?PushBack@@YAXABVDataNode@@@Z` |  |
| 351 | `lwz r11, 0x74, r31` | `lwz r11, 0x74, r31` |  |
| 352 | `rlwinm. r11, r11, 0, 27, 27` | `rlwinm. r11, r11, 0, 27, 27` |  |
| 353 | `beq 0x974` | `beq 0x988` |  |
| 354 | `lwz r3, 0x70, r31` | `lwz r3, 0x70, r31` |  |
| 355 | `b 0x970` | `b 0x984` |  |
| 356 | `cmpwi cr6, r28, 0x2` | `cmpwi cr6, r28, 0x2` |  |
| 357 | `bne cr6, 0x5cc` | `bne cr6, 0x5d0` |  |
| 358 | `lwz r3, lbl_82E278A4, r29` | `lwz r3, yytext, r29` |  |
| 359 | `bl atof` | `bl atof` |  |
| 360 | `li r11, 0x1` | `li r11, 0x1` |  |
| 361 | `frsp f0, f1` | `frsp f0, f1` |  |
| 362 | `stfs f0, 0x80, r31` | `stfs f0, 0x80, r31` |  |
| 363 | `stw r11, 0x84, r31` | `stw r11, 0x84, r31` |  |
| 364 | `addi r3, r31, 0x80` | `addi r3, r31, 0x80` |  |
| 365 | `bl ?PushBack@@YAXABVDataNode@@@Z` | `bl ?PushBack@@YAXABVDataNode@@@Z` |  |
| 366 | `lwz r11, 0x84, r31` | `lwz r11, 0x84, r31` |  |
| 367 | `rlwinm. r11, r11, 0, 27, 27` | `rlwinm. r11, r11, 0, 27, 27` |  |
| 368 | `beq 0x974` | `beq 0x988` |  |
| 369 | `lwz r3, 0x80, r31` | `lwz r3, 0x80, r31` |  |
| 370 | `b 0x970` | `b 0x984` |  |
| 371 | `cmpwi cr6, r28, 0x5` | `cmpwi cr6, r28, 0x5` |  |
| 372 | `beq cr6, 0x6dc` | `beq cr6, 0x6f0` |  |
| 373 | `cmpwi cr6, r28, 0x6` | `cmpwi cr6, r28, 0x6` |  |
| 374 | `beq cr6, 0x6c4` | `beq cr6, 0x6d8` |  |
| 375 | `cmpwi cr6, r28, 0x4` | `cmpwi cr6, r28, 0x4` |  |
| 376 | `bne cr6, 0xcc` | `bne cr6, 0xd4` |  |
| 377 | `lwz r11, lbl_82E278A4, r29` | `lwz r11, yytext, r29` |  |
| 378 | `add r11, r10, r11` | `add r11, r10, r11` |  |
| 379 | `stb r27, -0x1, r11` | `stb r27, -0x1, r11` |  |
| 380 | `lwz r11, lbl_82E278A4, r29` | `lwz r11, yytext, r29` |  |
| 381 | `addi r4, r11, 0x1` | `addi r4, r11, 0x1` |  |
| 382 | `lbz r10, 0x0, r4` | `lbz r10, 0x0, r4` |  |
| 383 | `mr r11, r4` | `mr r11, r4` |  |
| 384 | `cmplwi r10, 0x0` | `cmplwi r10, 0x0` |  |
| 385 | `beq 0x6a0` | `beq 0x6b4` |  |
| 386 | `lis r8, lbl_82DA0017` | `lis r8, gDataLine` |  |
| 387 | `lbz r10, 0x0, r11` | `lbz r9, 0x0, r11` | diff_arg |
| 388 | `mr r9, r27` | `mr r10, r27` | diff_arg |
| 389 | `extsb r10, r10` | `extsb r9, r9` | diff_arg |
| 390 | `cmpwi cr6, r10, 0x5c` | `cmpwi cr6, r9, 0x5c` | diff_arg |
| 391 | `bne cr6, 0x650` | `bne cr6, 0x664` |  |
| 392 | `lbz r10, 0x1, r11` | `lbz r9, 0x1, r11` | diff_arg |
| 393 | `extsb r10, r10` | `extsb r9, r9` | diff_arg |
| 394 | `cmpwi cr6, r10, 0x6e` | `cmpwi cr6, r9, 0x6e` | diff_arg |
| 395 | `bne cr6, 0x638` | `bne cr6, 0x63c` |  |
| 396 | `li r10, 0xa` | `li r9, 0xa` | diff_arg |
| 397 | `b 0x644` | `b 0x658` |  |
| 398 | `cmpwi cr6, r10, 0x71` | `cmpwi cr6, r9, 0x71` | diff_arg |
| 399 | `bne cr6, 0x664` | `bne cr6, 0x64c` | diff_arg |
| 400 | `li r10, 0x22` | `li r9, 0x22` | diff_arg |
| 401 | - | `b 0x658` | insert |
| 402 | - | `cmpwi cr6, r9, 0x74` | insert |
| 403 | - | `bne cr6, 0x678` | insert |
| 404 | - | `li r9, 0x9` | insert |
| 405 | `li r9, 0x1` | `li r10, 0x1` | diff_arg |
| 406 | `stb r10, 0x0, r11` | `stb r9, 0x0, r11` | diff_arg |
| 407 | `b 0x664` | `b 0x678` |  |
| 408 | `cmpwi cr6, r10, 0xa` | `cmpwi cr6, r9, 0xa` | diff_arg |
| 409 | `bne cr6, 0x664` | `bne cr6, 0x678` |  |
| 410 | `lwz r10, lbl_82DA0017, r8` | `lwz r9, gDataLine, r8` | diff_arg |
| 411 | `addi r10, r10, 0x1` | `addi r9, r9, 0x1` | diff_arg |
| 412 | `stw r10, lbl_82DA0017, r8` | `stw r9, gDataLine, r8` | diff_arg |
| 413 | `clrlwi. r10, r9, 24` | `clrlwi. r10, r10, 24` | diff_arg |
| 414 | `beq 0x694` | `beq 0x6a8` |  |
| 415 | `lbz r9, 0x1, r11` | `lbz r9, 0x1, r11` |  |
| 416 | `addi r10, r11, 0x1` | `addi r10, r11, 0x1` |  |
| 417 | `b 0x68c` | `b 0x6a0` |  |
| 418 | `lbz r7, 0x1, r10` | `lbz r7, 0x1, r10` |  |
| 419 | `addi r9, r10, 0x1` | `addi r9, r10, 0x1` |  |
| 420 | `stb r7, 0x0, r10` | `stb r7, 0x0, r10` |  |
| 421 | `mr r10, r9` | `mr r10, r9` |  |
| 422 | `lbz r9, 0x0, r9` | `lbz r9, 0x0, r9` |  |
| 423 | `cmplwi r9, 0x0` | `cmplwi r9, 0x0` |  |
| 424 | `bne 0x678` | `bne 0x68c` |  |
| 425 | `lbzu r10, 0x1, r11` | `lbzu r10, 0x1, r11` |  |
| 426 | `cmplwi r10, 0x0` | `cmplwi r10, 0x0` |  |
| 427 | `bne 0x60c` | `bne 0x610` |  |
| 428 | `addi r3, r31, 0xf0` | `addi r3, r31, 0xe8` | diff_arg |
| 429 | `bl ??0DataNode@@QAA@PBD@Z` | `bl ??0DataNode@@QAA@PBD@Z` |  |
| 430 | `addi r3, r31, 0xf0` | `addi r3, r31, 0xe8` | diff_arg |
| 431 | `bl ?PushBack@@YAXABVDataNode@@@Z` | `bl ?PushBack@@YAXABVDataNode@@@Z` |  |
| 432 | `lwz r11, 0xf4, r31` | `lwz r11, 0xec, r31` | diff_arg |
| 433 | `rlwinm. r11, r11, 0, 27, 27` | `rlwinm. r11, r11, 0, 27, 27` |  |
| 434 | `beq 0x974` | `beq 0x988` |  |
| 435 | `lwz r3, 0xf0, r31` | `lwz r3, 0xe8, r31` | diff_arg |
| 436 | `b 0x970` | `b 0x984` |  |
| 437 | `lwz r11, lbl_82E278A4, r29` | `lwz r11, yytext, r29` |  |
| 438 | `add r11, r10, r11` | `add r11, r10, r11` |  |
| 439 | `stb r27, -0x1, r11` | `stb r27, -0x1, r11` |  |
| 440 | `lwz r11, lbl_82E278A4, r29` | `lwz r11, yytext, r29` |  |
| 441 | `addi r4, r11, 0x1` | `addi r4, r11, 0x1` |  |
| 442 | `b 0x6e0` | `b 0x6f4` |  |
| 443 | `lwz r4, lbl_82E278A4, r29` | `lwz r4, yytext, r29` |  |
| 444 | `addi r3, r31, 0x54` | `addi r3, r31, 0x54` |  |
| 445 | `bl ??0Symbol@@QAA@PBD@Z` | `bl ??0Symbol@@QAA@PBD@Z` |  |
| 446 | `lwz r3, 0x54, r31` | `lwz r3, 0x54, r31` |  |
| 447 | `bl ?DataGetMacro@@YAPAVDataArray@@VSymbol@@@Z` | `bl ?DataGetMacro@@YAPAVDataArray@@VSymbol@@@Z` |  |
| 448 | `mr. r28, r3` | `mr. r28, r3` |  |
| 449 | `mr r11, r27` | - | delete |
| 450 | `beq 0x70c` | `beq 0x71c` |  |
| 451 | `lbz r10, 0x10, r30, lbl_82DA0017` | `lbz r11, 0x0, r30, gCachingFile` | diff_arg |
| 452 | `cmplwi r10, 0x0` | `cmplwi r11, 0x0` | diff_arg |
| 453 | - | `li r11, 0x1` | insert |
| 454 | `bne 0x70c` | `beq 0x720` | diff_op |
| 455 | `li r11, 0x1` | `mr r11, r27` | replace |
| 456 | `clrlwi. r11, r11, 24` | `clrlwi. r11, r11, 24` |  |
| 457 | `beq 0x74c` | `beq 0x760` |  |
| 458 | `lha r11, 0x8, r28` | `lha r11, 0x8, r28` |  |
| 459 | `mr r29, r27` | `mr r29, r27` |  |
| 460 | `cmpwi r11, 0x0` | `cmpwi r11, 0x0` |  |
| 461 | `ble 0x974` | `ble 0x988` |  |
| 462 | `mr r30, r27` | `mr r30, r27` |  |
| 463 | `lwz r11, 0x0, r28` | `lwz r11, 0x0, r28` |  |
| 464 | `add r3, r30, r11` | `add r3, r11, r30` | diff_arg |
| 465 | `bl ?PushBack@@YAXABVDataNode@@@Z` | `bl ?PushBack@@YAXABVDataNode@@@Z` |  |
| 466 | `lha r11, 0x8, r28` | `lha r11, 0x8, r28` |  |
| 467 | `addi r29, r29, 0x1` | `addi r29, r29, 0x1` |  |
| 468 | `addi r30, r30, 0x8` | `addi r30, r30, 0x8` |  |
| 469 | `cmpw cr6, r29, r11` | `cmpw cr6, r29, r11` |  |
| 470 | `blt cr6, 0x728` | `blt cr6, 0x73c` |  |
| 471 | `b 0x974` | `b 0x988` |  |
| 472 | `lwz r11, 0x54, r31` | `lwz r11, 0x54, r31` |  |
| 473 | `li r10, 0x5` | `li r10, 0x5` |  |
| 474 | `stw r10, 0x94, r31` | `stw r10, 0x94, r31` |  |
| 475 | `stw r11, 0x90, r31` | `stw r11, 0x90, r31` |  |
| 476 | `addi r3, r31, 0x90` | `addi r3, r31, 0x90` |  |
| 477 | `bl ?PushBack@@YAXABVDataNode@@@Z` | `bl ?PushBack@@YAXABVDataNode@@@Z` |  |
| 478 | `lwz r11, 0x94, r31` | `lwz r11, 0x94, r31` |  |
| 479 | `rlwinm. r11, r11, 0, 27, 27` | `rlwinm. r11, r11, 0, 27, 27` |  |
| 480 | `beq 0x974` | `beq 0x988` |  |
| 481 | `lwz r3, 0x90, r31` | `lwz r3, 0x90, r31` |  |
| 482 | `b 0x970` | `b 0x984` |  |
| 483 | `lwz r29, 0xc, r30, lbl_82DA0017` | `lwz r29, 0x3, r30, gOpenArray` | diff_arg |
| 484 | `stw r28, 0xc, r30, lbl_82DA0017` | `stw r28, 0x3, r30, gOpenArray` | diff_arg |
| 485 | `bl ?ParseArray@@YAPAVDataArray@@XZ` | `bl ?ParseArray@@YAPAVDataArray@@XZ` |  |
| 486 | `mr r27, r3` | `mr r27, r3` |  |
| 487 | `stw r29, 0xc, r30, lbl_82DA0017` | `stw r29, 0x3, r30, gOpenArray` | diff_arg |
| 488 | `cmpwi cr6, r28, 0x7` | `cmpwi cr6, r28, 0x7` |  |
| 489 | `bne cr6, 0x79c` | `bne cr6, 0x7b0` |  |
| 490 | `li r5, 0x10` | `li r5, 0x10` |  |
| 491 | `b 0x7ac` | `b 0x7c0` |  |
| 492 | `cmpwi cr6, r28, 0xb` | `cmpwi cr6, r28, 0xb` |  |
| 493 | `li r5, 0x11` | `li r5, 0x11` |  |
| 494 | `beq cr6, 0x7ac` | `beq cr6, 0x7c0` |  |
| 495 | `li r5, 0x13` | `li r5, 0x13` |  |
| 496 | `mr r4, r27` | `mr r4, r27` |  |
| 497 | `addi r3, r31, 0x100` | `addi r3, r31, 0x100` |  |
| 498 | `bl ??0DataNode@@QAA@PAVDataArray@@W4DataType@@@Z` | `bl ??0DataNode@@QAA@PAVDataArray@@W4DataType@@@Z` |  |
| 499 | `bl ?PushBack@@YAXABVDataNode@@@Z` | `bl ?PushBack@@YAXABVDataNode@@@Z` |  |
| 500 | `lwz r11, 0x104, r31` | `lwz r11, 0x104, r31` |  |
| 501 | `rlwinm. r11, r11, 0, 27, 27` | `rlwinm. r11, r11, 0, 27, 27` |  |
| 502 | `beq 0x7d0` | `beq 0x7e4` |  |
| 503 | `lwz r3, 0x100, r31` | `lwz r3, 0x100, r31` |  |
| 504 | `bl ?Release@DataArray@@QAAXXZ` | `bl ?Release@DataArray@@QAAXXZ` |  |
| 505 | `mr r3, r27` | `mr r3, r27` |  |
| 506 | `b 0x970` | `b 0x984` |  |
| 507 | `subi r11, r28, 0x14` | `subi r11, r28, 0x14` |  |
| 508 | `cntlzw r11, r11` | `cntlzw r11, r11` |  |
| 509 | `extrwi r28, r11, 1, 26` | `extrwi r28, r11, 1, 26` |  |
| 510 | `bl yylex` | `bl yylex` |  |
| 511 | `cmpwi cr6, r3, 0x6` | `cmpwi cr6, r3, 0x6` |  |
| 512 | `bne cr6, 0x80c` | `bne cr6, 0x820` |  |
| 513 | `lwz r11, lbl_82E278A8, r26` | `lwz r11, yyleng, r26` |  |
| 514 | `lwz r10, lbl_82E278A4, r29` | `lwz r10, yytext, r29` |  |
| 515 | `add r11, r11, r10` | `add r11, r11, r10` |  |
| 516 | `stb r27, -0x1, r11` | `stb r27, -0x1, r11` |  |
| 517 | `lwz r11, lbl_82E278A4, r29` | `lwz r11, yytext, r29` |  |
| 518 | `addi r4, r11, 0x1` | `addi r4, r11, 0x1` |  |
| 519 | `b 0x810` | `b 0x824` |  |
| 520 | `lwz r4, lbl_82E278A4, r29` | `lwz r4, yytext, r29` |  |
| 521 | `addi r3, r31, 0x58` | `addi r3, r31, 0x58` |  |
| 522 | `bl ??0Symbol@@QAA@PBD@Z` | `bl ??0Symbol@@QAA@PBD@Z` |  |
| 523 | `clrlwi. r11, r28, 24` | `clrlwi. r11, r28, 24` |  |
| 524 | `lbz r11, 0x10, r30, lbl_82DA0017` | `lbz r11, 0x0, r30, gCachingFile` | diff_arg |
| 525 | `beq 0x86c` | `beq 0x880` |  |
| 526 | `cmplwi r11, 0x0` | `cmplwi r11, 0x0` |  |
| 527 | `beq 0x858` | `beq 0x86c` |  |
| 528 | `lwz r11, 0x58, r31` | `lwz r11, 0x58, r31` |  |
| 529 | `li r10, 0x7` | `li r10, 0x7` |  |
| 530 | `stw r10, 0xa4, r31` | `stw r10, 0xa4, r31` |  |
| 531 | `stw r11, 0xa0, r31` | `stw r11, 0xa0, r31` |  |
| 532 | `addi r3, r31, 0xa0` | `addi r3, r31, 0xa0` |  |
| 533 | `bl ?PushBack@@YAXABVDataNode@@@Z` | `bl ?PushBack@@YAXABVDataNode@@@Z` |  |
| 534 | `lwz r11, 0xa4, r31` | `lwz r11, 0xa4, r31` |  |
| 535 | `rlwinm. r11, r11, 0, 27, 27` | `rlwinm. r11, r11, 0, 27, 27` |  |
| 536 | `beq 0x974` | `beq 0x988` |  |
| 537 | `lwz r3, 0xa0, r31` | `lwz r3, 0xa0, r31` |  |
| 538 | `b 0x970` | `b 0x984` |  |
| 539 | `lwz r3, 0x58, r31` | `lwz r3, 0x58, r31` |  |
| 540 | `bl ?DataGetMacro@@YAPAVDataArray@@VSymbol@@@Z` | `bl ?DataGetMacro@@YAPAVDataArray@@VSymbol@@@Z` |  |
| 541 | `subic r11, r3, 0x1` | `subic r11, r3, 0x1` |  |
| 542 | `subfe r11, r11, r3` | `subfe r11, r11, r3` |  |
| 543 | `b 0x8b0` | `b 0x8c4` |  |
| 544 | `cmplwi r11, 0x0` | `cmplwi r11, 0x0` |  |
| 545 | `beq 0x8a0` | `beq 0x8b4` |  |
| 546 | `lwz r11, 0x58, r31` | `lwz r11, 0x58, r31` |  |
| 547 | `li r10, 0x23` | `li r10, 0x23` |  |
| 548 | `stw r10, 0xb4, r31` | `stw r10, 0xb4, r31` |  |
| 549 | `stw r11, 0xb0, r31` | `stw r11, 0xb0, r31` |  |
| 550 | `addi r3, r31, 0xb0` | `addi r3, r31, 0xb0` |  |
| 551 | `bl ?PushBack@@YAXABVDataNode@@@Z` | `bl ?PushBack@@YAXABVDataNode@@@Z` |  |
| 552 | `lwz r11, 0xb4, r31` | `lwz r11, 0xb4, r31` |  |
| 553 | `rlwinm. r11, r11, 0, 27, 27` | `rlwinm. r11, r11, 0, 27, 27` |  |
| 554 | `beq 0x974` | `beq 0x988` |  |
| 555 | `lwz r3, 0xb0, r31` | `lwz r3, 0xb0, r31` |  |
| 556 | `b 0x970` | `b 0x984` |  |
| 557 | `lwz r3, 0x58, r31` | `lwz r3, 0x58, r31` |  |
| 558 | `bl ?DataGetMacro@@YAPAVDataArray@@VSymbol@@@Z` | `bl ?DataGetMacro@@YAPAVDataArray@@VSymbol@@@Z` |  |
| 559 | `cntlzw r11, r3` | `cntlzw r11, r3` |  |
| 560 | `extrwi r11, r11, 1, 26` | `extrwi r11, r11, 1, 26` |  |
| 561 | `stw r25, 0x54, r31` | `stw r25, 0x54, r31` |  |
| 562 | `addi r6, r31, 0x50` | `addi r6, r31, 0x50` |  |
| 563 | `stb r11, 0x50, r31` | `stb r11, 0x50, r31` |  |
| 564 | `addi r5, r31, 0x54` | `addi r5, r31, 0x54` |  |
| 565 | `mr r4, r25, lbl_82DA0017` | `mr r4, r25, ?gConditional@@3V?$list@_NV?$StlNodeAlloc@_N@stlpmtx_std@@@stlpmtx_std@@A` |  |
| 566 | `addi r3, r31, 0x5c` | `addi r3, r31, 0x5c` |  |
| 567 | `bl ?insert@?$list@_NV?$StlNodeAlloc@_N@stlpmtx_std@@@stlpmtx_std@@QAA?AU?$_List_iterator@_NU?$_Nonconst_traits@_N@stlpmtx_std@@@2@U32@AB_N@Z` | `bl ?insert@?$list@_NV?$StlNodeAlloc@_N@stlpmtx_std@@@stlpmtx_std@@QAA?AU?$_List_iterator@_NU?$_Nonconst_traits@_N@stlpmtx_std@@@2@U32@AB_N@Z` |  |
| 568 | `b 0x974` | `b 0x988` |  |
| 569 | `subi r11, r28, 0xf` | `subi r11, r28, 0xf` |  |
| 570 | `cntlzw r11, r11` | `cntlzw r11, r11` |  |
| 571 | `extrwi r28, r11, 1, 26` | `extrwi r28, r11, 1, 26` |  |
| 572 | `bl yylex` | `bl yylex` |  |
| 573 | `lbz r11, 0x10, r30, lbl_82DA0017` | `lbz r11, 0x0, r30, gCachingFile` | diff_arg |
| 574 | `cmplwi r11, 0x0` | `cmplwi r11, 0x0` |  |
| 575 | `beq 0x924` | `beq 0x938` |  |
| 576 | `addi r3, r31, 0x5c` | `addi r3, r31, 0x5c` |  |
| 577 | `lwz r4, lbl_82E278A4, r29` | `lwz r4, yytext, r29` |  |
| 578 | `bl ??0Symbol@@QAA@PBD@Z` | `bl ??0Symbol@@QAA@PBD@Z` |  |
| 579 | `lwz r11, 0x0, r3` | `lwz r11, 0x0, r3` |  |
| 580 | `li r10, 0x21` | `li r10, 0x21` |  |
| 581 | `stw r10, 0xc4, r31` | `stw r10, 0xc4, r31` |  |
| 582 | `stw r11, 0xc0, r31` | `stw r11, 0xc0, r31` |  |
| 583 | `addi r3, r31, 0xc0` | `addi r3, r31, 0xc0` |  |
| 584 | `bl ?PushBack@@YAXABVDataNode@@@Z` | `bl ?PushBack@@YAXABVDataNode@@@Z` |  |
| 585 | `lwz r11, 0xc4, r31` | `lwz r11, 0xc4, r31` |  |
| 586 | `rlwinm. r11, r11, 0, 27, 27` | `rlwinm. r11, r11, 0, 27, 27` |  |
| 587 | `beq 0x974` | `beq 0x988` |  |
| 588 | `lwz r3, 0xc0, r31` | `lwz r3, 0xc0, r31` |  |
| 589 | `b 0x970` | `b 0x984` |  |
| 590 | `mr r4, r28` | `mr r4, r28` |  |
| 591 | `lwz r3, lbl_82E278A4, r29` | `lwz r3, yytext, r29` |  |
| 592 | `bl fn_82745BD0` | `bl ?ReadEmbeddedFile@@YAPAVDataArray@@PBD_N@Z` |  |
| 593 | `mr. r30, r3` | `mr. r30, r3` |  |
| 594 | `beq 0x974` | `beq 0x988` |  |
| 595 | `lha r11, 0x8, r30` | `lha r11, 0x8, r30` |  |
| 596 | `mr r28, r27` | `mr r28, r27` |  |
| 597 | `cmpwi r11, 0x0` | `cmpwi r11, 0x0` |  |
| 598 | `ble 0x96c` | `ble 0x980` |  |
| 599 | `mr r29, r27` | `mr r29, r27` |  |
| 600 | `lwz r11, 0x0, r30` | `lwz r11, 0x0, r30` |  |
| 601 | `add r3, r29, r11` | `add r3, r29, r11` |  |
| 602 | `bl ?PushBack@@YAXABVDataNode@@@Z` | `bl ?PushBack@@YAXABVDataNode@@@Z` |  |
| 603 | `lha r11, 0x8, r30` | `lha r11, 0x8, r30` |  |
| 604 | `addi r28, r28, 0x1` | `addi r28, r28, 0x1` |  |
| 605 | `addi r29, r29, 0x8` | `addi r29, r29, 0x8` |  |
| 606 | `cmpw cr6, r28, r11` | `cmpw cr6, r28, r11` |  |
| 607 | `blt cr6, 0x94c` | `blt cr6, 0x960` |  |
| 608 | `mr r3, r30` | `mr r3, r30` |  |
| 609 | `bl ?Release@DataArray@@QAAXXZ` | `bl ?Release@DataArray@@QAAXXZ` |  |
| 610 | `li r3, 0x1` | `li r3, 0x1` |  |
| 611 | `addi r1, r31, 0x150` | `addi r1, r31, 0x150` |  |
| 612 | `b __restgprlr_25` | `b __restgprlr_25` |  |



## Detected Patterns

- **unknown**
- **unknown**
- **unknown**
- **unknown**
- **unknown**

## Mismatches (60 of 613 instructions)

- [32] diff_arg: `lwz` [off:+11, sym]
- [62] diff_arg: `lbz` [off:-16, sym]
- [89] diff_arg: `lwz` [off:+11, sym]
- [90] diff_arg: `lwz` [off:+11, sym]
- [93] diff_arg: `lwz` [off:+11, sym]
- [95] diff_arg: `lwz` [off:+11, sym]
- [98] diff_arg: `stw` [off:+11, sym]
- [112] diff_arg: `lbz` [off:-16, sym]
- [133] diff_arg: `lbz` [off:-16, sym]
- [156] diff_arg: `lwz` [off:-9, sym]
- [157] diff_arg: `stw` [off:-9, sym]
- [159] diff_arg: `stw` [off:-9, sym]
- [165] diff_arg: `lbz` [off:-16, sym]
- [206] diff_arg: `lwz` [off:-9, sym]
- [207] diff_arg: `stw` [off:-9, sym]
- [209] diff_arg: `lbz` [off:-16, sym]
- [211] diff_arg: `stw` [off:-9, sym]
- [227] diff_arg: `addi` [off:+8]
- [229] delete: `addi     r3, r31, 0xe8`
- [231] diff_arg: `lwz` [off:+8]
- [234] diff_arg: `lwz` [off:+8]
- [247] diff_arg: `lbz` [off:-16, sym]
- [387] diff_arg: `lbz` [reg:r10->r9]
- [388] diff_arg: `mr` [reg:r9->r10]
- [389] diff_arg: `extsb` [reg:r10->r9, reg:r10->r9]
- [390] diff_arg: `cmpwi` [reg:r10->r9]
- [392] diff_arg: `lbz` [reg:r10->r9]
- [393] diff_arg: `extsb` [reg:r10->r9, reg:r10->r9]
- [394] diff_arg: `cmpwi` [reg:r10->r9]
- [396] diff_arg: `li` [reg:r10->r9]
- [398] diff_arg: `cmpwi` [reg:r10->r9]
- [399] diff_arg: `bne` [br]
- [400] diff_arg: `li` [reg:r10->r9]
- [401] insert: `b        0x658`
- [402] insert: `cmpwi    cr6, r9, 0x74`
- [403] insert: `bne      cr6, 0x678`
- [404] insert: `li       r9, 0x9`
- [405] diff_arg: `li` [reg:r9->r10]
- [406] diff_arg: `stb` [reg:r10->r9]
- [408] diff_arg: `cmpwi` [reg:r10->r9]
- [410] diff_arg: `lwz` [reg:r10->r9, sym]
- [411] diff_arg: `addi` [reg:r10->r9, reg:r10->r9]
- [412] diff_arg: `stw` [reg:r10->r9, sym]
- [413] diff_arg: `clrlwi.` [reg:r9->r10]
- [428] diff_arg: `addi` [off:-8]
- [430] diff_arg: `addi` [off:-8]
- [432] diff_arg: `lwz` [off:-8]
- [435] diff_arg: `lwz` [off:-8]
- [449] delete: `mr       r11, r27`
- [451] diff_arg: `lbz` [reg:r10->r11, off:-16, sym]
- [452] diff_arg: `cmplwi` [reg:r10->r11]
- [453] insert: `li       r11, 0x1`
- [454] diff_op: `bne` vs `beq`
- [455] replace: `li       r11, 0x1` vs `mr       r11, r27`
- [464] diff_arg: `add` [reg:r30->r11, reg:r11->r30]
- [483] diff_arg: `lwz` [off:-9, sym]
- [484] diff_arg: `stw` [off:-9, sym]
- [487] diff_arg: `stw` [off:-9, sym]
- [524] diff_arg: `lbz` [off:-16, sym]
- [573] diff_arg: `lbz` [off:-16, sym]

[stderr]
Building incremental: build/45410914/src/system/obj/DataFile.obj
Loaded 5 ICF equivalence entries from /home/free/code/milohax/rb3-xenon/build/45410914/icf_aliases.map