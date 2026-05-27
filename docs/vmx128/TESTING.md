# VMX128 Ghidra Testing Guide

This document covers headless testing and validation of VMX128 support.

## Prerequisites

| Component | Path |
|-----------|------|
| Stock Ghidra | `/opt/ghidra/` (system install, no VMX128) |
| Modified Ghidra | `~/code/milohax/vmx128-research/ghidra-vmx128/build/dist/` |
| DC3 XEX | Your DC3 executable |
| Test scripts | `~/code/milohax/vmx128-research/ghidra-scripts/` |

## Setup

### 1. Extract modified Ghidra

```bash
cd ~/code/milohax/vmx128-research
mkdir -p ghidra-test
unzip ghidra-vmx128/build/dist/ghidra_12.0_DEV_*_linux_x86_64.zip -d ghidra-test/
```

### 2. Create script directory

```bash
mkdir -p ~/code/milohax/vmx128-research/ghidra-scripts
```

---

## Headless Analysis

### Basic Command Structure

```bash
# Stock Ghidra (no VMX128)
/opt/ghidra/support/analyzeHeadless \
    /tmp/ghidra_projects StockProject \
    -import /path/to/dc3.xex \
    -processor "PowerPC:BE:64:A2ALT" \
    -postScript "TestScript.py" \
    -scriptPath ~/code/milohax/vmx128-research/ghidra-scripts

# Modified Ghidra (with VMX128)
~/code/milohax/vmx128-research/ghidra-test/ghidra_12.0_DEV/support/analyzeHeadless \
    /tmp/ghidra_projects VMX128Project \
    -import /path/to/dc3.xex \
    -processor "PowerPC:BE:64:Xenon" \
    -postScript "TestScript.py" \
    -scriptPath ~/code/milohax/vmx128-research/ghidra-scripts
```

### Key Options

| Option | Description |
|--------|-------------|
| `-import <file>` | Import binary (creates new program) |
| `-process <name>` | Process existing program in project |
| `-processor <lang>` | Language ID (e.g., `PowerPC:BE:64:Xenon`) |
| `-postScript <script>` | Run script after analysis |
| `-scriptPath <dir>` | Additional script search path |
| `-noanalysis` | Skip auto-analysis (faster for disasm-only) |
| `-log <file>` | Write log to file |
| `-overwrite` | Overwrite existing program |

---

## Test Scripts

### VMX128 Validation Script

`vmx128_validate.py` - Scans for VMX128 instructions and reports statistics:

```python
# @category VMX128
# @description Validate VMX128 instruction recognition

from ghidra.program.model.listing import CodeUnit

VMX128_MNEMONICS = {
    # Load/Store
    "lvx128", "lvxl128", "stvx128", "stvxl128",
    "lvlx", "lvlxl", "lvrx", "lvrxl",
    "stvlx", "stvlxl", "stvrx", "stvrxl",
    # Arithmetic
    "vaddfp128", "vsubfp128", "vmulfp128", "vmaddfp128", "vmsubfp128",
    "vnmsubfp128", "vdot3fp128", "vdot4fp128",
    # Compare
    "vcmpgtfp128", "vcmpeqfp128", "vcmpgefp128", "vcmpbfp128",
    # Logical
    "vor128", "vand128", "vandc128", "vxor128", "vnor128",
    # Permute/Shift
    "vperm128", "vsldoi128", "vmrghw128", "vmrglw128",
    "vspltw128", "vspltisw128", "vsplth128", "vspltb128",
    # Pack/Unpack
    "vpkd3d128", "vupkd3d128",
    # Convert/Round
    "vrfim128", "vrfin128", "vrfip128", "vrfiz128",
    "vcsxwfp128", "vcfpsxws128", "vcuxwfp128", "vcfpuxws128",
    # Estimates
    "vrefp128", "vrsqrtefp128", "vexptefp128", "vlogefp128",
    # Min/Max
    "vmaxfp128", "vminfp128",
    # Select
    "vsel128",
    # Rotate/Insert
    "vrlimi128", "vpermwi128",
}

def run():
    listing = currentProgram.getListing()

    stats = {
        "vmx128_recognized": 0,
        "vmx128_by_mnemonic": {},
        "unknown": 0,
        "unknown_addresses": [],
        "total_instructions": 0,
    }

    for inst in listing.getInstructions(True):
        stats["total_instructions"] += 1
        mnemonic = inst.getMnemonicString().lower()

        # Check for recognized VMX128
        if mnemonic in VMX128_MNEMONICS or mnemonic.rstrip('.') in VMX128_MNEMONICS:
            stats["vmx128_recognized"] += 1
            base = mnemonic.rstrip('.')
            stats["vmx128_by_mnemonic"][base] = stats["vmx128_by_mnemonic"].get(base, 0) + 1

        # Check for unknown/bad instructions
        elif mnemonic in ("??", "bad", "unknown") or mnemonic.startswith("word"):
            stats["unknown"] += 1
            if len(stats["unknown_addresses"]) < 20:
                stats["unknown_addresses"].append(str(inst.getAddress()))

    # Report
    print("\n" + "="*60)
    print("VMX128 VALIDATION REPORT")
    print("="*60)
    print("Program: {}".format(currentProgram.getName()))
    print("Total instructions: {:,}".format(stats["total_instructions"]))
    print("VMX128 recognized: {:,}".format(stats["vmx128_recognized"]))
    print("Unknown/bad: {:,}".format(stats["unknown"]))
    print("")

    if stats["vmx128_by_mnemonic"]:
        print("VMX128 by mnemonic:")
        for mnem, count in sorted(stats["vmx128_by_mnemonic"].items(),
                                   key=lambda x: -x[1])[:20]:
            print("  {:20s} {:,}".format(mnem, count))

    if stats["unknown_addresses"]:
        print("\nSample unknown addresses:")
        for addr in stats["unknown_addresses"][:10]:
            print("  {}".format(addr))

    print("="*60)

run()
```

### Decompiler Test Script

`vmx128_decompile.py` - Decompile functions and check output:

```python
# @category VMX128
# @description Test decompilation of VMX-heavy functions

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

# Function name patterns likely to contain VMX128 code
VMX_PATTERNS = ["XMVector", "ST::", "Matrix", "Quat", "Vector"]

def run():
    monitor = ConsoleTaskMonitor()
    decomp = DecompInterface()
    decomp.openProgram(currentProgram)

    fm = currentProgram.getFunctionManager()
    tested = 0
    success = 0
    has_pcodeop = 0

    print("\n" + "="*60)
    print("VMX128 DECOMPILATION TEST")
    print("="*60)

    for func in fm.getFunctions(True):
        name = func.getName()

        # Check if function name matches VMX patterns
        if not any(p in name for p in VMX_PATTERNS):
            continue

        tested += 1
        result = decomp.decompileFunction(func, 30, monitor)

        if result.decompileCompleted():
            success += 1
            code = result.getDecompiledFunction().getC()

            # Check for VMX128 pcodeop calls
            if "vector" in code.lower() or "128" in code:
                has_pcodeop += 1
                if tested <= 5:  # Show first 5 examples
                    print("\nFunction: {} @ {}".format(name, func.getEntryPoint()))
                    print("-" * 40)
                    lines = code.split('\n')[:30]
                    for line in lines:
                        print("  " + line)
                    if len(code.split('\n')) > 30:
                        print("  ... ({} more lines)".format(len(code.split('\n')) - 30))
        else:
            if tested <= 10:
                print("FAILED: {} - {}".format(name, result.getErrorMessage()))

        if tested >= 50:
            break

    print("\n" + "="*60)
    print("SUMMARY")
    print("="*60)
    print("Functions tested: {}".format(tested))
    print("Decompilation succeeded: {}".format(success))
    print("Contains VMX pcodeops: {}".format(has_pcodeop))
    print("="*60)

    decomp.dispose()

run()
```

### Address-Specific Disassembly

`vmx128_disasm_range.py` - Disassemble a specific address range:

```python
# @category VMX128
# @description Disassemble address range and show instructions
# @runtime Jython

def run():
    # Configure these addresses
    START = 0x82000000  # Adjust to your binary
    END = 0x82001000

    af = currentProgram.getAddressFactory()
    start_addr = af.getAddress(hex(START))
    end_addr = af.getAddress(hex(END))

    listing = currentProgram.getListing()

    print("\nDisassembly from {} to {}:".format(start_addr, end_addr))
    print("-" * 60)

    addr = start_addr
    while addr and addr.compareTo(end_addr) < 0:
        inst = listing.getInstructionAt(addr)
        if inst:
            print("{}: {:20s} {}".format(
                addr,
                inst.getMnemonicString(),
                inst.toString()
            ))
            addr = inst.getNext().getAddress() if inst.getNext() else None
        else:
            # Try to get data or skip
            addr = addr.add(4)

run()
```

---

## Side-by-Side Comparison

### Comparison Script

`vmx128_compare.sh` - Run both Ghidra versions and compare:

```bash
#!/bin/bash
set -e

DC3_XEX="${1:?Usage: $0 <path-to-dc3.xex>}"
OUTPUT_DIR="/tmp/vmx128_comparison"
SCRIPT_DIR="$HOME/code/milohax/vmx128-research/ghidra-scripts"

STOCK_GHIDRA="/opt/ghidra"
MOD_GHIDRA="$HOME/code/milohax/vmx128-research/ghidra-test/ghidra_12.0_DEV"

mkdir -p "$OUTPUT_DIR"

echo "=== VMX128 Comparison Test ==="
echo "DC3 XEX: $DC3_XEX"
echo "Output: $OUTPUT_DIR"
echo ""

# Clean up old projects
rm -rf /tmp/ghidra_projects/StockTest* /tmp/ghidra_projects/VMX128Test*

echo "--- Running Stock Ghidra (no VMX128) ---"
"$STOCK_GHIDRA/support/analyzeHeadless" \
    /tmp/ghidra_projects StockTest \
    -import "$DC3_XEX" \
    -processor "PowerPC:BE:64:A2ALT" \
    -postScript "vmx128_validate.py" \
    -scriptPath "$SCRIPT_DIR" \
    -log "$OUTPUT_DIR/stock.log" \
    2>&1 | tee "$OUTPUT_DIR/stock_output.txt"

echo ""
echo "--- Running Modified Ghidra (with VMX128) ---"
"$MOD_GHIDRA/support/analyzeHeadless" \
    /tmp/ghidra_projects VMX128Test \
    -import "$DC3_XEX" \
    -processor "PowerPC:BE:64:Xenon" \
    -postScript "vmx128_validate.py" \
    -scriptPath "$SCRIPT_DIR" \
    -log "$OUTPUT_DIR/vmx128.log" \
    2>&1 | tee "$OUTPUT_DIR/vmx128_output.txt"

echo ""
echo "=== Results ==="
echo "Stock output: $OUTPUT_DIR/stock_output.txt"
echo "VMX128 output: $OUTPUT_DIR/vmx128_output.txt"
echo ""
echo "Key metrics to compare:"
grep -E "(VMX128 recognized|Unknown/bad)" "$OUTPUT_DIR/stock_output.txt" || true
echo "---"
grep -E "(VMX128 recognized|Unknown/bad)" "$OUTPUT_DIR/vmx128_output.txt" || true
```

### Expected Results

| Metric | Stock Ghidra | Modified Ghidra |
|--------|--------------|-----------------|
| VMX128 recognized | 0 | ~37,000 |
| Unknown instructions | ~37,000+ | ~0 |
| Decompiler pcodeops | None | `vectorMultiplyFloatingPoint128()` etc. |

---

## Quick Validation Checklist

### Disassembly
- [ ] VMX128 mnemonics appear (e.g., `vcmpgtfp128`, `lvx128`)
- [ ] No "??" or "bad instruction" for VMX128 opcodes
- [ ] Extended registers shown (vr64-vr127, not just vr0-vr31)
- [ ] Immediate fields display correctly (PERM, D3DType, etc.)

### Decompiler
- [ ] Functions with VMX128 code decompile without errors
- [ ] Output shows pcodeop calls (e.g., `vectorAddFloatingPoint128(...)`)
- [ ] Variable types show as 16-byte vectors where appropriate

### Regression
- [ ] Standard AltiVec instructions still work
- [ ] Non-VMX code unaffected
- [ ] No crashes during analysis

---

## Troubleshooting

### "Unknown language" error
The Xenon processor isn't available. Ensure you're using the modified Ghidra build.

### Script not found
Check `-scriptPath` points to the directory containing the `.py` files.

### Analysis hangs
Try `-noanalysis` to skip auto-analysis, or increase timeout.

### Memory issues
Add to launch.properties: `MAXMEM=8G`

---

## Next Steps After Validation

1. **Document findings** - Update PHASE4_TODO.md with any issues
2. **Generate report** - Run comparison, save output for documentation
3. **Iterate** - Fix any problems found, rebuild, retest
4. **Phase 5** - Prepare for upstream contribution
