# XEXLoaderWV - Ghidra Xbox 360 XEX Loader

> **Note for rb3-xenon:** There is no equivalent leaked .map file for RB3. `tools/fingerprint_match.py` fills this role for function identification (string/callee cross-referencing). The PDB loading and map-file workflow sections below are DC3-specific and not applicable.

A Ghidra extension for loading and analyzing Xbox 360 XEX executables. Essential for reverse engineering the original RB3 binary.

## Quick Start

```bash
# Build the extension (requires Java 17+ JDK)
cd ~/code/milohax/XEXLoaderWV/XEXLoaderWV
JAVA_HOME=/usr/lib/jvm/java-25-openjdk /opt/ghidra/support/gradle/gradlew -PGHIDRA_INSTALL_DIR=/opt/ghidra

# Output: dist/ghidra_12.0.1_DEV_*_XEXLoaderWV.zip
```

**Install:**
1. Open Ghidra
2. File → Install Extensions
3. Add the built `.zip` file
4. Restart Ghidra

## Loading the RB3 XEX

The original RB3 executable is at `orig/45410914/default.xex`.

1. Create/open a Ghidra project
2. File → Import File → select `default.xex`
3. XEXLoaderWV auto-detects by magic number (`XEX2`)
4. Configure options:
   - **Process .pdata**: Enable (adds function symbols from exception data)
   - **Load PDB File**: Enable if you have debug symbols (`.pdb` or `.xdb`)
5. Analyze with default analyzers

**Architecture:** PowerPC:BE:64:A2ALT-32addr (Xbox 360 Xenon CPU)

## Loader Options

| Option | Default | Description |
|--------|---------|-------------|
| Process .pdata | true | Extract function symbols from exception handling data |
| Load PDB File | false | Prompt for PDB/XDB debug symbol file |
| Use experimental PDB loader | false | Use Ghidra's universal parser vs MSDIA |
| Path to xexp | "" | Optional XEXP delta patch file |

## PDB Loading

If you have a PDB file (rare for retail games):

1. Check **Load PDB File** and **Use experimental PDB loader**
2. Uncheck **Process .pdata**
3. When prompted, select MSDIA parser
4. Browse to your `.pdb` or `.xdb` file

## Use Cases for rb3-xenon

### 1. Function Discovery

XEXLoaderWV extracts function boundaries from `.pdata` sections, giving you a head start on identifying functions the decomp doesn't cover yet.

```bash
# After loading in Ghidra, export function list:
# Window → Symbol Table → Export to CSV
```

### 2. Understanding Unknown Code

When objdiff shows a function at 0% or you're working on new code:

1. Find the function address in Ghidra
2. Study the decompiler output (Ctrl+E)
3. Compare with your source code

### 3. Verifying Struct Layouts

Ghidra's data type manager can help verify class/struct layouts:

1. Right-click address → Data → Create Structure
2. Compare member offsets with your headers

### 4. Import Analysis

XEXLoaderWV resolves Xbox 360 system imports (xboxkrnl, xam, etc.):

- Functions named with `__imp__` prefix
- Thunks generated for trampolines
- Ordinals mapped to function names via internal lookup table

### 5. Cross-Reference Analysis

Find all callers/callees of a function:

1. Navigate to function
2. Right-click → References → Show References To/From
3. Useful for understanding control flow

## Technical Details

**Supported Formats:**
- Retail XEX (AES-128-CBC encrypted)
- Dev kit XEX (unencrypted)
- Compressed (Basic, Normal, LZX)
- Delta patches (XEXP files)

**File Format:**
```
XEX2 Header
├── Optional Headers (metadata, import tables)
├── Loader Info (RSA signature, regions, media flags)
├── Security Info (encryption key)
└── PE Image (compressed/encrypted)
    ├── DOS Header
    ├── NT/PE Headers
    └── Sections (.text, .data, .rdata, .pdata, etc.)
```

## Building From Source

**Requirements:**
- JDK 17+ (not JRE)
- Ghidra 12.0+

**Build:**
```bash
cd ~/code/milohax/XEXLoaderWV/XEXLoaderWV

# Use Ghidra's bundled gradle wrapper
JAVA_HOME=/usr/lib/jvm/java-25-openjdk \
  /opt/ghidra/support/gradle/gradlew \
  -PGHIDRA_INSTALL_DIR=/opt/ghidra

# Or with environment variable
export GHIDRA_INSTALL_DIR=/opt/ghidra
export JAVA_HOME=/usr/lib/jvm/java-25-openjdk
/opt/ghidra/support/gradle/gradlew
```

**Output:** `dist/ghidra_*_XEXLoaderWV.zip`

## Workflow Integration

### With objdiff

1. Load XEX in Ghidra for reference
2. Use objdiff CLI to compare your decomp:
   ```bash
   objdiff-cli diff -p . "Game::Poll" -f json --include-instructions
   ```
3. Cross-reference Ghidra's decompilation when stuck

### Finding Near-Match Functions

```bash
# Find functions at 90-99% match
objdiff-cli report query build/45410914/report.json \
  --functions --min-percent 90 --max-percent 99 --limit 10

# Load XEX in Ghidra to analyze what's different
```

### Reverse Engineering New Functions

1. Identify target function address from objdiff/report
2. Navigate to address in Ghidra (G → enter address)
3. Study decompiled output
4. Write matching C++ code
5. Build and verify with objdiff

## Source Code Overview

Key classes in `~/code/milohax/XEXLoaderWV/XEXLoaderWV/src/`:

| File | Purpose |
|------|---------|
| `XEXLoaderWVLoader.java` | Main entry point, Ghidra loader interface |
| `XEXHeader.java` | XEX format parser, decryption coordinator |
| `LzxDecompression.java` | LZX decompression algorithm |
| `ImportRenamer.java` | Xbox 360 import ordinal → name mapping |
| `PDBFile.java` | PDB debug symbol parser |
| `TPIStream.java` | PDB type information processor |

## Troubleshooting

**"Toolchain installation does not provide JAVA_COMPILER"**
- You have a JRE, not JDK. Install JDK: `pacman -S jdk-openjdk`
- Or use Java 25: `JAVA_HOME=/usr/lib/jvm/java-25-openjdk`

**XEX not recognized**
- Ensure file has `XEX2` magic at offset 0
- Check file isn't truncated/corrupted

**Missing symbols after load**
- Enable "Process .pdata" option
- Run auto-analysis after loading

## References

- [XEXLoaderWV GitHub](https://github.com/zeroKilo/XEXLoaderWV)
- [Ghidra Documentation](https://ghidra.re/ghidra_docs/)
- [XEX File Format](http://free60.org/wiki/XEX)
