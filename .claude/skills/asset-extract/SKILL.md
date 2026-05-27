---
name: asset-extract
description: Extract assets/materials from RB3 .ark archives and .milo/.dta files. Use when you need game assets for the native port, debugging rendering, or inspecting data.
argument-hint: "[command] [options]"
allowed-tools: Bash, Read, Grep, Glob
---

# Asset Extract Skill

Extract and inspect assets from the RB3 game archives (.ark + .milo_xbox + .dta/.dtb).

## Arguments

`$ARGUMENTS`

## Quick Reference

The argument `$0` determines what to do:

| Command | Description |
|---|---|
| `extract` | Extract the full ark to `orig-assets/extracted/` |
| `extract --inflate` | Extract + decompress all .milo_xbox files |
| `extract --dta` | Extract + convert .dtb → .dta text |
| `extract --all` | Extract everything (inflate + dta convert) |
| `inflate <file>` | Decompress a single .milo_xbox file |
| `inflate --dir <dir>` | Decompress all milos in a directory |
| `find` | List all extracted assets with type summary |
| `find --type milo_xbox` | Filter by file extension |
| `find --grep pattern` | Search by name pattern |
| `find --tree` | Show directory tree |
| `info <file>` | Show milo compression info |
| `inspect <file>` | List object types/names inside a milo scene |
| `inspect <file> --search Foo` | Search raw milo bytes for a string |

## Workflow

### 1. Full Ark Extraction

Extract the RB3 ark archive. This is the starting point — run it once.

```bash
# Basic extraction (files only, no decompression)
scripts/milo/extract_ark.sh

# Full extraction with milo decompression + dta conversion
scripts/milo/extract_ark.sh --all

# Custom output directory
scripts/milo/extract_ark.sh /tmp/rb3-assets --all
```

The ark header is at `orig-assets/gen/main_xbox.hdr`.
Uses `arkhelper ark2dir` from `../milo-executable-library/`.

### 2. Find Assets

After extraction, search and list assets:

```bash
# Summary of all asset types
python3 scripts/milo/find_assets.py --summary

# Find all .milo_xbox files
python3 scripts/milo/find_assets.py --type milo_xbox

# Search by name
python3 scripts/milo/find_assets.py --grep "venue"

# Browse a subdirectory
python3 scripts/milo/find_assets.py --path world --tree
```

If no extracted dir exists, it falls back to the pre-extracted library at:
`~/code/milohax/milo-engine-libs/harmonix-repos/milo-rnd-library/rb3/`

### 3. Decompress Individual Milos

For working with specific .milo_xbox files (e.g. for the native port viewer):

```bash
# Decompress a single file
python3 scripts/milo/inflate_milo.py path/to/file.milo_xbox

# Decompress with custom output
python3 scripts/milo/inflate_milo.py path/to/file.milo_xbox output.milo_xbox

# Show compression info without extracting
python3 scripts/milo/inflate_milo.py --info path/to/file.milo_xbox

# Batch decompress a directory
python3 scripts/milo/inflate_milo.py --dir orig-assets/extracted/world/
```

### 4. Milo Compression Formats

| Version | Magic | Description |
|---|---|---|
| A | `0xCABEDEAF` | Uncompressed |
| B | `0xCBBEDEAF` | zlib compressed |
| C | `0xCCBEDEAF` | gzip compressed |
| D | `0xCDBEDEAF` | Hybrid (mixed compressed + uncompressed blocks) |

RB3 Xbox 360 typically uses version D (hybrid).

### 5. Inspect Milo Contents

Inspect objects inside a milo scene file (searches the raw binary data):

```bash
# List object types in a milo scene (parses entry table)
python3 scripts/milo/inspect_milo.py path/to/file.milo_xbox --summary

# Search for a specific type string in raw bytes
python3 scripts/milo/inspect_milo.py path/to/file.milo_xbox --search "EventTrigger"

# Filter entries by type
python3 scripts/milo/inspect_milo.py path/to/file.milo_xbox --type PropAnim
```

Use `--search` for definitive presence/absence checks (raw byte scan, no format assumptions).

## Tips

- `--inflate` decompresses milos in-place during ark extraction (saves a second pass)
- `--dta` is useful for reading game scripts (menus, flow logic, song data)
- Use `find_assets.py --grep` to locate specific materials, textures, or meshes by name
- Decompressed milos can be loaded directly by the native port's milo-viewer
