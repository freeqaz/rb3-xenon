#!/usr/bin/env python3
"""
Convert objdiff JSON output to m2c-compatible assembly format.

This script parses the JSON output from objdiff-cli (with --include-instructions)
and converts it to GNU-as style assembly that m2c can process.

Canonical home: this file is the single source of truth for the converter.
Each consuming target repo (dc3-decomp, rb3-xenon, and the mwcc/ELF GC/Wii
clone repos under ~/code/milohax/decomp-clones/<key>) vendors its own copy at
`tools/objdiff_to_m2c.py` (or points `$OBJDIFF_TO_M2C_PATH` /
`decomp-synth.json`'s `objdiff_to_m2c_path` at this file directly) --
`tools/il_witness/build_decomp_target_corpus.py:_resolve_m2c()` documents the
resolution order. Fix bugs HERE first, then re-vendor.

Two object formats feed this script:
  - MSVC/COFF PPC (dc3, rb3-xenon -- Xbox 360 targets)
  - mwcc/ELF32-BE PPC (the GC/Wii clone repos -- melee, tww, tp, ss, pikmin2,
    pikmin, pik2wii, bfbb)
objdiff-cli's disassembly hides that difference for ordinary instructions, but
one mwcc/ELF-specific quirk leaks through: constant-pool / small-data-area
(SDA) relocations (`R_PPC_EMB_SDA21`/`R_PPC_EMB_SDA2REL`). In the *unlinked*
object, the raw instruction bytes encode a placeholder base register (rA=0)
that only gets patched to the real r2/r13 SDA base at link time, so objdiff's
disassembly of these ops omits the base register entirely and just shows the
bare pool-ref symbol -- spelled either `@NNN` (a per-TU constant-pool counter,
e.g. `lfd f0, @158`) or a plain SDA global name (e.g. `stw r3, sSkidMarkRaster`).
See `format_instruction()`'s memory-op handling below for the fix.

Usage:
    # Pipe from objdiff-cli
    ./bin/objdiff-cli diff -p . "CharClip::SetFlags" -f json --include-instructions | \
        python3 tools/objdiff_to_m2c.py

    # From file
    python3 tools/objdiff_to_m2c.py -i function.json -o function.s

    # With custom symbol name
    ./bin/objdiff-cli diff -p . "CharClip::SetFlags" -f json --include-instructions | \
        python3 tools/objdiff_to_m2c.py --symbol CharClip_SetFlags

    # Full pipeline to m2c
    ./bin/objdiff-cli diff -p . "CharMirror::Load" -f json --include-instructions | \
        python3 tools/objdiff_to_m2c.py | \
        python3 ~/code/milohax/m2c/m2c.py -t ppc -

    # With jump table resolution (for functions with switch statements)
    ./bin/objdiff-cli diff -p . "BustAMovePanel::OnBeat" -f json --include-instructions | \
        python3 tools/objdiff_to_m2c.py --obj build/45410914/obj/lazer/game/BustAMovePanel.obj | \
        python3 ~/code/milohax/m2c/m2c.py -t ppc -

Examples:
    # Extract target binary disassembly and decompile
    ./bin/objdiff-cli diff -p . "Game::Poll" -f json --include-instructions 2>/dev/null | \
        python3 tools/objdiff_to_m2c.py | \
        python3 ~/code/milohax/m2c/m2c.py -t ppc -
"""

import argparse
import json
import os
import re
import struct
import sys
from typing import Dict, List, Optional, Tuple


# PPC branch mnemonics as objdiff spells them. Shared by the two sites that
# have to agree about what a branch is: `parse_branch_targets`, which MINTS a
# `.L_` label from a branch's destination, and `convert_objdiff_json`'s emit
# loop, which CONSUMES one by respelling that destination as the label. The
# two used to disagree -- the consumer had no opcode test at all -- and that
# asymmetry is the bug described in `_is_branch_opcode` below.
BRANCH_OPCODES = frozenset({
    'b', 'bl', 'ba', 'bla',
    'bc', 'bcl', 'bca', 'bcla',
    'bclr', 'bclrl', 'bcctr', 'bcctrl',
    'beq', 'bne', 'blt', 'bgt', 'ble', 'bge',
    'beqlr', 'bnelr', 'bltlr', 'bgtlr', 'blelr', 'bgelr',
    'bdnz', 'bdz', 'bdnzl', 'bdzl',
    'bdzf', 'bdzt', 'bdnzf', 'bdnzt',
})

# A trailing hex operand, refused when it is glued to the token on its left.
# The `(?<![-\w.])` guard makes the match a whole operand rather than the tail
# of one: it refuses `-0x10` (a negative displacement, whose magnitude is not
# an address), `foo0x10` / `lbl_0x10` (a symbol that merely ends in something
# hex-shaped) and `.0x10`. On the measured rb3-xenon corpus the guard changes
# nothing on its own -- it is a second line of defence behind the opcode gate,
# not the load-bearing half of the repair. See `convert_objdiff_json`.
_TRAILING_HEX_RE = re.compile(r'(?<![-\w.])(0x[0-9a-fA-F]+)$')


def _is_branch_opcode(opcode: str) -> bool:
    """
    True when `opcode` is a PPC branch, i.e. when its trailing hex operand is
    a code ADDRESS and may legitimately be respelled as a `.L_xxxxxxxx` label.

    Every other opcode's trailing hex operand is an IMMEDIATE, and respelling
    it produces assembly m2c cannot parse -- `addi r3, r3, .L_00000120` for
    `addi r3, r3, 0x120`, whenever 0x120 also happens to be some branch's
    destination in the same function. Measured on rb3-xenon build/45410914,
    all 1948 functions scoring in [90,100): 453 such lines across 147
    functions (addi 293, li 43, cmplwi 42, subi 37, ori 18, cmpwi 8, oris 6,
    mulli 4, lis 1, subfic 1), against 6931 legitimate branch respellings
    across 475 functions. Gating on this predicate drops all 453 and loses 0
    of the 6931. The failure surfaced as an *m2c* parse failure rather than as
    a converter error, which is why it went unnoticed.

    Deliberately the same predicate `parse_branch_targets` uses to mint the
    labels in the first place, so producer and consumer cannot drift apart
    again. The bare `startswith('b')` arm is intentional (it catches `blr`,
    `bctr`, `bctrl` and any `bcond` spelling not enumerated above); on the
    measured corpus every b-prefixed opcode objdiff emits is in fact a branch.
    """
    return opcode in BRANCH_OPCODES or opcode.startswith('b')


def symbol_to_label(name: str) -> str:
    """
    Convert a symbol name to a valid assembly label.

    Handles:
    - C++ demangled names like "CharClip::SetFlags"
    - MSVC mangled names like "?SetFlags@CharClip@@QAAXH@Z"
    - Special names like "__savegprlr_29"

    Returns a valid C identifier usable as an assembly label.
    """
    if not name:
        return 'unknown'

    # Handle special names (compiler intrinsics)
    if name.startswith('__'):
        return name

    # Handle C++ demangled names with full signatures
    # e.g. "private: class MoveFrame * __cdecl MoveDir::ClosestMoveFrame(void)"
    # Extract the qualified name (Class::Method) before the parameter list
    if '::' in name:
        # Strip parameter list
        paren_idx = name.find('(')
        if paren_idx != -1:
            name = name[:paren_idx]
        # Take the last space-separated token(s) containing ::
        # This strips return type, access specifier, calling convention
        tokens = name.split()
        qualified = [t for t in tokens if '::' in t]
        if qualified:
            label = qualified[-1].replace('::', '_')
        else:
            label = tokens[-1].replace('::', '_')
        # Sanitize any remaining invalid chars
        label = re.sub(r'[^a-zA-Z0-9_]', '_', label)
        label = re.sub(r'_+', '_', label).strip('_')
        return label or 'unknown'

    # Handle MSVC mangled names like "?SetFlags@CharClip@@QAAXH@Z"
    if name.startswith('?'):
        # Remove leading ? or ??
        clean = re.sub(r'^\?\??', '', name)

        # Extract function and class names before @@
        parts = clean.split('@')
        if len(parts) >= 2:
            func_name = parts[0]
            class_name = parts[1] if parts[1] and not parts[1].startswith('@') else None
            if class_name:
                return f"{class_name}_{func_name}"
            return func_name

        # Fallback: replace @ with _ and clean up
        clean = clean.replace('@', '_')
        clean = re.sub(r'_+$', '', clean)
        return clean or 'unknown'

    # Handle template names with < >
    name = name.replace('<', '_').replace('>', '_').replace(',', '_')
    name = re.sub(r'_+', '_', name)  # Collapse multiple underscores
    name = name.strip('_')

    # Replace any remaining invalid chars
    name = re.sub(r'[^a-zA-Z0-9_]', '_', name)

    return name or 'unknown'


#: objdiff `typed_args` types that render as hex in the flat `args` spelling.
_HEX_ARG_TYPES = ('Signed', 'Unsigned', 'BranchDest')


def flat_args_from_typed(typed_args: list) -> str:
    """
    Rebuild objdiff's flat `args` spelling from its `typed_args`.

    WHY THIS EXISTS. objdiff-cli fdc5113 ("ruler I", 2026-08-16) changed the
    JSON `args` string from the COMPARISON arg list to the DISPLAY parts:

        lwz r0, 0x0, r5                 ->  lwz r0, 0x0(r5)
        lis r11, ?TheDebug@@3VDebug@@A  ->  lis r11, ?TheDebug@@3VDebug@@A@h
        mr r5, r6, sDevices__6UsbWii    ->  mr r5, r6

    That is a strictly better rendering, and everything below was written
    against the old one. Parsing the new spelling with the old rules produced
    silent corruption rather than an error -- `lis r11, "sym@h"@ha` (the
    relocation suffix swallowed into the quoted symbol name, plus a second
    macro), and `lwz r4, "sym@l(r11)"@sda21(r13)` for a plain symbolic load.
    Measured over 7 real dc3/rb3-xenon functions: every one changed, and every
    change was a corruption.

    THE FIX IS NOT TO PARSE THE NEW SPELLING -- it is to stop depending on the
    spelling at all. `typed_args` is structured, carries the clean symbol name
    with no relocation suffix, and did NOT move across the change. Rebuilding
    the flat string from it makes this converter's output invariant to which
    objdiff built the JSON, which is the property we actually want: an m2c seed
    that changes because the ruler was rebuilt is a change to the proposer's
    input distribution that nothing downstream would flag.

    Verified exact, not assumed: over 4,466 instruction sides drawn from real
    dc3-decomp and rb3-xenon objects dumped under BOTH rulers, this function
    reproduces ruler H's `args` byte-for-byte, 4,466/4,466, 0 mismatches. The
    rendering rules mirror objdiff's own `InstructionArgValue` Display impl
    (`objdiff-core/src/obj/mod.rs:188`) and the pre-fdc5113 `build_instruction_info`
    join: Signed/Unsigned/BranchDest as `{:#x}` (negatives as `-0x…`), Register
    and Symbol and Other verbatim, joined with ", ".

    Rows with no `typed_args` at all keep whatever `args` they had; in the same
    sample those were 104 rows, all `bctrl`, all with `args: null`.
    """
    parts = []
    for arg in typed_args:
        value = arg.get('value')
        if arg.get('type') in _HEX_ARG_TYPES and isinstance(value, int):
            parts.append('-0x%x' % -value if value < 0 else '0x%x' % value)
        else:
            parts.append(str(value))
    return ', '.join(parts)


def normalize_instruction_args(instructions: list) -> int:
    """
    Rewrite every instruction side's `args` into the flat spelling, in place.

    Applied once at the parse boundary so all three `args` readers below --
    branch-target scanning, jump-table detection and format_instruction -- see
    one spelling and none of them has to know a ruler change happened.

    Returns the number of sides rewritten (0 when the JSON came from a
    pre-fdc5113 objdiff, since the rebuild is then a no-op).
    """
    rewritten = 0
    for instr in instructions:
        for side in ('target', 'base'):
            row = instr.get(side)
            if not isinstance(row, dict):
                continue
            typed_args = row.get('typed_args')
            if not typed_args:
                continue
            flat = flat_args_from_typed(typed_args)
            if flat != row.get('args'):
                row['args'] = flat
                rewritten += 1
    return rewritten


def parse_branch_targets(instructions: list) -> set:
    """
    Identify branch target addresses from branch instructions.
    Returns a set of addresses that are branch targets.
    """
    targets = set()

    for instr in instructions:
        target = instr.get('target', {})
        opcode = target.get('opcode', '')

        # Check if this is a branch instruction
        if _is_branch_opcode(opcode):
            args = target.get('args', '')
            # Look for hex addresses like 0x17dc or cr6, 0x17dc
            # Branch targets in objdiff are shown as absolute addresses
            match = re.search(r'0x([0-9a-fA-F]+)$', args)
            if match:
                addr = int(match.group(1), 16)
                targets.add(addr)

    return targets


def quote_symbol(sym: str) -> str:
    """
    Quote a symbol name if it contains characters that need escaping.
    MSVC mangled names contain ? and @ which need quoting. mwcc bare
    constant-pool refs (`@158`) and string-pool-base symbols (`@stringBase0`)
    also start with `@` and need the same quoting -- m2c's assembly grammar
    treats a leading `@` as the start of a `@ha`/`@l`/`@sda21`/... relocation
    macro, so an unquoted symbol whose NAME itself begins with `@` can't be
    told apart from that macro syntax without quotes.
    """
    # Check if quoting is needed
    if '?' in sym or '@' in sym or '$' in sym or '<' in sym or '>' in sym:
        # Don't double-quote
        if sym.startswith('"') and sym.endswith('"'):
            return sym
        return f'"{sym}"'
    return sym


#: A literal number in the normalized flat `args` spelling: hex (the form
#: `flat_args_from_typed` emits for Signed/Unsigned/BranchDest, `{:#x}`, with
#: negatives as `-0x…`) or bare decimal, either sign.
_NUMERIC_OPERAND_RE = re.compile(r'-?(?:0[xX][0-9a-fA-F]+|[0-9]+)')


def _is_numeric_operand(s: str) -> bool:
    """
    True when an operand is a literal number rather than a symbol reference.

    The four callers below all ask the same question -- "is this operand a
    displacement/immediate, or a relocated symbol that needs a `@ha`/`@l`/
    `@sda21` macro?" -- and each used to ask it as
    `not s.startswith('0x') and not s.lstrip('-').isdigit()`. That guard is
    blind to NEGATIVE hex: `'-0x120'.lstrip('-')` is `'0x120'`, which is not
    `isdigit()`, and the `0x` prefix test fails on the leading `-`. So every
    negative hex operand took the symbol branch and came out with a bogus
    relocation macro glued to a number -- `stwu r1, -0x120@l(r1)` for a plain
    stack-frame store, `addi r1, r1, -0x120@l`, `lis r11, -0x1@ha`.

    Negative hex is the whole of the miss: POSITIVE hex was already caught by
    the `startswith('0x')` test and both signs of decimal by `isdigit()`. The
    bug predates fdc5113 ("ruler I") -- the old comparison spelling emitted
    `-0x…` too -- and was left in place by the ruler-I repair only so that
    repair's differential stayed a clean no-op; this is that follow-on.
    """
    return bool(_NUMERIC_OPERAND_RE.fullmatch(s))


def _is_reloc_symbol(s: str) -> bool:
    """Check if a string looks like a relocation symbol appended by objdiff."""
    # MSVC mangled names, labels, merged symbols, and mwcc bare pool-ref /
    # string-pool-base symbols (`@158`, `@stringBase0` -- these can only ever
    # be relocation-symbol annotations here since a real register/immediate
    # argument never starts with a literal `@`).
    return (s.startswith('?') or s.startswith('merged_') or
            s.startswith('lbl_') or s.startswith('jumptable_') or
            s.startswith('switch_') or s.startswith('__jtbl') or
            s.startswith('@') or
            (s.startswith('"') and '@' in s))


def find_obj_for_symbol(project_dir: str, symbol_name: str) -> Optional[str]:
    """
    Find the target OBJ file for a symbol by searching objdiff.json units.

    Extracts the class name from the demangled symbol and matches it against
    unit names in objdiff.json. Returns the absolute path to the target OBJ.
    """
    objdiff_json_path = os.path.join(project_dir, 'objdiff.json')
    try:
        with open(objdiff_json_path) as f:
            config = json.load(f)
    except (OSError, json.JSONDecodeError):
        return None

    # Extract class/file name from demangled symbol
    # e.g. "public: void __cdecl BustAMovePanel::OnBeat(void)" -> "BustAMovePanel"
    candidates = []
    match = re.search(r'(\w+)::\w+', symbol_name)
    if match:
        class_name = match.group(1)
        for unit in config.get('units', []):
            unit_name = unit.get('name', '')
            target_path = unit.get('target_path', '')
            if not target_path:
                continue
            # Match class name against the last component of the unit name
            # e.g. "default/lazer/game/BustAMovePanel" -> "BustAMovePanel"
            unit_basename = unit_name.rsplit('/', 1)[-1]
            if unit_basename == class_name:
                candidates.append(os.path.join(project_dir, target_path))

    # If we got exactly one match, use it
    if len(candidates) == 1:
        obj_path = candidates[0]
        if os.path.exists(obj_path):
            return obj_path

    # Multiple matches or no match - try matching against OBJ filename
    if not candidates:
        for unit in config.get('units', []):
            target_path = unit.get('target_path', '')
            if not target_path:
                continue
            obj_basename = os.path.basename(target_path).replace('.obj', '')
            if match and obj_basename == match.group(1):
                candidates.append(os.path.join(project_dir, target_path))

    # Return first existing match
    for path in candidates:
        if os.path.exists(path):
            return path

    return None


def detect_jump_tables(instructions: list) -> List[Dict]:
    """
    Detect MSVC PPC jump table patterns in instructions.

    Pattern:
        cmplwi crN, rX, max_case
        bgt crN, default_label
        lis rY, jumptable_*
        slwi rZ, rX, shift
        addi rY, rY, jumptable_*
        lhzx/lwzx/lbzx rZ, rY, rZ
        lis rW, base@ha
        addi rW, rW, base@l
        [nop]
        add rW, rW, rZ
        mtctr rW
        bctr

    Returns list of jump table info dicts with:
        - symbol: jump table symbol name (e.g. "jumptable_820FA100")
        - num_cases: number of cases from cmplwi + 1
        - entry_size: 1 (lbzx), 2 (lhzx), or 4 (lwzx)
        - bctr_addr: address of the bctr instruction
    """
    tables = []
    for i, instr in enumerate(instructions):
        t = instr.get('target', {})
        if t.get('opcode') != 'bctr':
            continue

        bctr_addr_str = t.get('address', '')
        if not bctr_addr_str:
            continue
        bctr_addr = int(bctr_addr_str, 16)

        # Scan backwards from bctr to find the pattern
        jtbl_symbol = None
        num_cases = None
        entry_size = None

        # Look back up to 15 instructions for the pattern
        start = max(0, i - 15)
        for j in range(start, i):
            tj = instructions[j].get('target', {})
            op = tj.get('opcode', '')
            args = tj.get('args', '')

            # Find lis with jumptable symbol
            if op == 'lis' and 'jumptable_' in args:
                parts = args.split(', ', 1)
                if len(parts) == 2:
                    jtbl_symbol = parts[1]

            # Find cmplwi to get case count
            if op == 'cmplwi':
                typed = tj.get('typed_args', [])
                for ta in typed:
                    if ta.get('type') == 'Unsigned':
                        num_cases = ta['value'] + 1  # cmplwi compares against max index

            # Find load instruction to determine entry size
            if op == 'lbzx':
                entry_size = 1
            elif op == 'lhzx':
                entry_size = 2
            elif op == 'lwzx':
                entry_size = 4

        if jtbl_symbol and num_cases and entry_size:
            tables.append({
                'symbol': jtbl_symbol,
                'num_cases': num_cases,
                'entry_size': entry_size,
                'bctr_addr': bctr_addr,
            })

    return tables


def read_jump_table_from_obj(obj_path: str, symbol_name: str,
                             num_cases: int, entry_size: int) -> Optional[List[int]]:
    """
    Read jump table entries from a COFF object file.

    Parses the COFF symbol table to find the jump table symbol,
    then reads the entries from the appropriate section.

    Returns list of integer offsets, or None on failure.

    KNOWN GAP (documented, not fixed by the mwcc `@NNN` pool-ref patch):
    this reader is COFF-only (MSVC/X360 objects: dc3, rb3-xenon). It does
    NOT understand ELF32-BE objects (the mwcc/GC/Wii clone repos), because
    `detect_jump_tables()`'s pattern match is itself MSVC-specific (looks for
    a `jumptable_*`-named symbol fed through `lis`/`addi` HIGH/LOW halves --
    mwcc's switch-table codegen and symbol naming differ enough that the
    detector and this reader would both need separate ELF-aware
    implementations, not a one-line fix).

    Verified empirically (2026-08-05) that this degrades SAFELY rather than
    corrupting output or hanging: fed a real mwcc ELF `.o`
    (doldecomp_melee's `ansi_fp.o`), the COFF field layout misreads section
    headers, hits a `struct.error` on the very first `f.read(4)` short-read,
    and the `except (OSError, struct.error)` below catches it -- this
    function returns `None`, `convert_objdiff_json()`'s caller treats that
    exactly like "file unreadable" and skips resolving that jump table (the
    switch's case labels are left unresolved; m2c gets an assembly listing
    that still assembles, it just won't decompile the switch statement
    correctly). No ELF object crashed or hung during that check. A real ELF
    jump-table reader would need `elf_ppc.py`'s symbol/section access
    extended to non-`.text` data symbols -- out of scope for this converter
    file alone.
    """
    try:
        with open(obj_path, 'rb') as f:
            # COFF header (always little-endian, even for big-endian targets)
            f.seek(0)
            _machine = struct.unpack('<H', f.read(2))[0]
            num_sections = struct.unpack('<H', f.read(2))[0]
            f.read(4)  # timestamp
            symtab_offset = struct.unpack('<I', f.read(4))[0]
            num_symbols = struct.unpack('<I', f.read(4))[0]
            opt_size = struct.unpack('<H', f.read(2))[0]
            f.read(2)  # characteristics

            # Read section headers
            f.seek(20 + opt_size)

            sections = []
            for _ in range(num_sections):
                sec_name = f.read(8).rstrip(b'\x00').decode('ascii', errors='replace')
                f.read(8)  # virtual size + addr
                raw_size = struct.unpack('<I', f.read(4))[0]
                raw_offset = struct.unpack('<I', f.read(4))[0]
                f.read(16)  # relocs, linenums, etc
                sections.append({
                    'name': sec_name,
                    'raw_size': raw_size,
                    'raw_offset': raw_offset,
                })

            # Read string table
            strtab_offset = symtab_offset + num_symbols * 18
            f.seek(strtab_offset)
            strtab_size_bytes = f.read(4)
            if len(strtab_size_bytes) < 4:
                return None
            strtab_size = struct.unpack('<I', strtab_size_bytes)[0]
            f.seek(strtab_offset)
            strtab = f.read(strtab_size)

            def get_string(offset):
                end = strtab.find(b'\x00', offset)
                if end < 0:
                    end = len(strtab)
                return strtab[offset:end].decode('ascii', errors='replace')

            # Search symbol table for the jump table symbol
            f.seek(symtab_offset)
            sym_value = None
            sym_section = None
            i = 0
            while i < num_symbols:
                entry = f.read(18)
                if len(entry) < 18:
                    break
                name_field = entry[:8]
                value = struct.unpack('<I', entry[8:12])[0]
                section_num = struct.unpack('<h', entry[12:14])[0]
                num_aux = entry[17]

                if name_field[:4] == b'\x00\x00\x00\x00':
                    str_offset = struct.unpack('<I', name_field[4:8])[0]
                    name = get_string(str_offset)
                else:
                    name = name_field.rstrip(b'\x00').decode('ascii', errors='replace')

                if name == symbol_name:
                    sym_value = value
                    sym_section = section_num  # 1-based
                    break

                # Skip aux entries
                for _ in range(num_aux):
                    f.read(18)
                i += 1 + num_aux

            if sym_value is None or sym_section is None or sym_section < 1:
                return None

            # Get the section data
            sec = sections[sym_section - 1]  # convert to 0-based
            file_offset = sec['raw_offset'] + sym_value

            # Read entries (big-endian data on PPC)
            f.seek(file_offset)
            entries = []
            for _ in range(num_cases):
                if entry_size == 1:
                    data = f.read(1)
                    if len(data) < 1:
                        break
                    entries.append(data[0])
                elif entry_size == 2:
                    data = f.read(2)
                    if len(data) < 2:
                        break
                    entries.append(struct.unpack('>H', data)[0])
                elif entry_size == 4:
                    data = f.read(4)
                    if len(data) < 4:
                        break
                    entries.append(struct.unpack('>I', data)[0])

            return entries

    except (OSError, struct.error) as e:
        print(f"Warning: Failed to read jump table from {obj_path}: {e}",
              file=sys.stderr)
        return None


def format_instruction(instr: dict) -> str:
    """
    Format a single instruction from objdiff JSON to assembly.

    Handles:
    - Standard instructions
    - Relocations (lis/addi with @ha/@l suffixes)
    - Branch targets
    - Extra relocation info appended by objdiff (strip it for mr, etc.)
    - Quoting MSVC mangled symbols
    - mwcc constant-pool / SDA-relative memory ops with the base register
      omitted (bare `@NNN` or bare named SDA global -- see memory_ops below)
    """
    target = instr.get('target', {})
    opcode = target.get('opcode', '')
    args = target.get('args', '')

    if not opcode:
        return ''

    # Handle lis with relocation - needs @ha suffix
    # objdiff shows: "lis r11, ?TheDebug@@3VDebug@@A"
    # m2c needs: lis r11, "?TheDebug@@3VDebug@@A"@ha
    if opcode == 'lis' and args:
        # Check if this looks like a symbol reference (not just a number)
        parts = args.split(', ', 1)
        if len(parts) == 2:
            reg, operand = parts
            # If operand is a symbol (starts with ? or letter, not 0x number)
            if operand and not _is_numeric_operand(operand):
                # Quote and add @ha suffix for high-adjusted address
                return f"{opcode} {reg}, {quote_symbol(operand)}@ha"

    # Handle addi with relocation - needs @l suffix
    # objdiff shows: "addi r29, r11, ?TheDebug@@3VDebug@@A"
    # m2c needs: addi r29, r11, "?TheDebug@@3VDebug@@A"@l
    # Also handles: "addi r7, r28, 0x4, lbl_82017228" -> "addi r7, r28, 0x4"
    if opcode in ('addi', 'subi') and args:
        parts = args.split(', ')
        if len(parts) >= 3:
            # Check if we have 4 parts with relocation info appended
            if len(parts) == 4:
                # Format: "addi r7, r28, 0x4, lbl_82017228"
                # Strip the relocation info, keep: "addi r7, r28, 0x4"
                return f"{opcode} {parts[0]}, {parts[1]}, {parts[2]}"
            # Check if third part is a symbol
            last = parts[-1]
            if last and not _is_numeric_operand(last):
                # Reconstruct with @l suffix and quote
                prefix = ', '.join(parts[:-1])
                return f"{opcode} {prefix}, {quote_symbol(last)}@l"

    # Handle mr with extra relocation info
    # objdiff shows: "mr r7, r28, lbl_82017228" or "mr r3, r29, ?TheDebug@@3VDebug@@A"
    # mr only takes 2 register operands
    if opcode == 'mr' and args:
        parts = args.split(', ')
        if len(parts) >= 3:
            # Strip extra relocation info
            return f"{opcode} {parts[0]}, {parts[1]}"

    # Handle bl/b with symbol targets - quote if needed
    if opcode in ('bl', 'b') and args:
        # Check if target is a symbol (not an address or label)
        if not args.startswith('0x') and not args.startswith('.L_') and not args.startswith('__'):
            return f"{opcode} {quote_symbol(args)}"

    # Convert memory operands from objdiff format to GNU-as format
    # objdiff: "lwz r11, 0x4c, r3" -> GNU-as: "lwz r11, 0x4c(r3)"
    # This applies to load/store instructions with offset(base) format.
    # Indexed ops (ending in 'x') use 3 registers: "lbzx rD, rA, rB" - no conversion needed.
    memory_ops = {
        'lwz', 'lbz', 'lhz', 'lha', 'lfs', 'lfd', 'lmw',
        'stw', 'stb', 'sth', 'stfs', 'stfd', 'stmw',
        'lwzu', 'lbzu', 'lhzu', 'lfsu', 'lfdu',
        'stwu', 'stbu', 'sthu', 'stfsu', 'stfdu',
        # PPC64 load/store doubleword
        'ld', 'std', 'ldu', 'stdu',
    }

    if opcode in memory_ops and args:
        parts = args.split(', ')
        if len(parts) == 3:
            reg_dest, offset, reg_base = parts
            # Check if offset is a symbol reference (not numeric)
            # e.g. "lwz r4, ?gNullStr@@3PBDB, r11" -> "lwz r4, "?gNullStr..."@l(r11)"
            if offset and not _is_numeric_operand(offset):
                return f"{opcode} {reg_dest}, {quote_symbol(offset)}@l({reg_base})"
            # Format: "lwz r11, 0x4c, r3" -> "lwz r11, 0x4c(r3)"
            return f"{opcode} {reg_dest}, {offset}({reg_base})"
        elif len(parts) == 4:
            # Format with relocation: "lwz r11, 0x3c, r10, ?TheTaskMgr..." -> "lwz r11, 0x3c(r10)"
            reg_dest, offset, reg_base, _reloc = parts
            return f"{opcode} {reg_dest}, {offset}({reg_base})"
        elif len(parts) == 2:
            # mwcc/ELF SDA-relative memory op with the base register omitted.
            # In the unlinked object, R_PPC_EMB_SDA21/SDA2REL relocs leave the
            # instruction's rA field as a 0-placeholder (patched to r2/r13 at
            # link time), so objdiff's disassembly shows only the bare
            # symbol: "lfd f0, @158" (mwcc constant-pool id) or
            # "stw r3, sSkidMarkRaster" (a plain SDA global), never a
            # "reg, offset, base" triple like the len==3 case above.
            #
            # m2c's PPC parser requires an explicit base register in parens
            # (`sym@sda21(reg)` / `sym@sda2(reg)`, see m2c's own
            # end-to-end fixtures under tests/end_to_end/*/mwcc-o4p.s), but
            # m2c discards whichever register we name here once it recognizes
            # the sda2/sda21 macro (`translate.py:strip_macros` replaces the
            # whole `AsmAddressMode` with just the symbol, unconditionally --
            # confirmed by reading that code directly). So the register
            # choice below is cosmetic, not semantic; we still follow the
            # real embedded-PPC EABI convention for readability: r2 backs the
            # read-only `.sdata2`/`.sbss2` area (float/double constant pools,
            # conventionally reached via lfd/lfs/stfd/stfs), r13 backs the
            # read-write `.sdata`/`.sbss` area (everything else).
            reg_dest, sym = parts
            if sym and not _is_numeric_operand(sym):
                base_reg = 'r2' if opcode in ('lfd', 'lfs', 'stfd', 'stfs') else 'r13'
                return f"{opcode} {reg_dest}, {quote_symbol(sym)}@sda21({base_reg})"
            # Not a symbol-shaped operand (e.g. a bare numeric literal with no
            # base register) -- an addressing-mode shape we don't recognize.
            # Fall through to the general handling below, which will emit it
            # unchanged (fail-closed: m2c will reject it rather than us
            # guessing at semantics).

    # Indexed memory ops (ending in 'x') use 3 registers: "lbzx rD, rA, rB"
    # objdiff may append relocation info as a 4th part - strip it
    indexed_memory_ops = {
        'lwzx', 'lbzx', 'lhzx', 'lhax', 'lfsx', 'lfdx',
        'stwx', 'stbx', 'sthx', 'stfsx', 'stfdx',
        'lwbrx', 'lhbrx', 'stwbrx', 'sthbrx',
        'lwarx', 'stwcx.',
    }

    if opcode in indexed_memory_ops and args:
        parts = args.split(', ')
        if len(parts) == 4:
            # Strip relocation info: "lbzx r0, r12, r4, ??_C@..." -> "lbzx r0, r12, r4"
            return f"{opcode} {parts[0]}, {parts[1]}, {parts[2]}"

    # General relocation stripping: objdiff appends symbol info to many instructions
    # e.g. "add r12, r12, r0, ?SongInfoAudioTypeToSym..." -> "add r12, r12, r0"
    if args:
        parts = args.split(', ')
        if len(parts) >= 2 and _is_reloc_symbol(parts[-1]):
            cleaned = ', '.join(parts[:-1])
            return f"{opcode} {cleaned}"

    # Standard instruction formatting
    if args:
        return f"{opcode} {args}"
    else:
        return opcode


def convert_objdiff_json(data: dict, symbol_override: Optional[str] = None,
                         obj_path: Optional[str] = None) -> str:
    """
    Convert objdiff JSON to m2c assembly format.

    Args:
        data: Parsed JSON from objdiff-cli
        symbol_override: Optional symbol name to use instead of extracted one
        obj_path: Optional path to target OBJ file for jump table resolution

    Returns:
        m2c-compatible assembly string
    """
    output = []

    # Get symbol name
    symbol = symbol_override or data.get('symbol', 'unknown')
    label = symbol_to_label(symbol)

    # Get instructions
    instructions = data.get('instructions', [])
    if not instructions:
        print(f"Warning: No instructions found for {symbol}", file=sys.stderr)
        return ""

    # Canonicalize the arg spelling BEFORE anything reads it, so the rest of
    # this file is independent of which objdiff wrote the JSON. See
    # flat_args_from_typed for what changed and why parsing the new spelling
    # would have been the wrong repair.
    normalize_instruction_args(instructions)

    # Find branch targets to create labels
    branch_targets = parse_branch_targets(instructions)

    # Detect and resolve jump tables
    jump_tables = detect_jump_tables(instructions)
    rdata_sections = []  # list of (symbol_name, target_addrs) for .rdata emission

    if jump_tables and obj_path:
        for jtbl in jump_tables:
            entries = read_jump_table_from_obj(
                obj_path, jtbl['symbol'],
                jtbl['num_cases'], jtbl['entry_size']
            )
            if entries is None:
                continue

            # Compute target addresses in objdiff address space
            # base = bctr_addr + 4 (the instruction right after bctr)
            base_addr = jtbl['bctr_addr'] + 4
            target_addrs = [base_addr + e for e in entries]

            # Add targets to branch_targets so labels are emitted
            for addr in target_addrs:
                branch_targets.add(addr)

            rdata_sections.append((jtbl['symbol'], target_addrs))

            print(f"Resolved jump table {jtbl['symbol']}: "
                  f"{jtbl['num_cases']} cases, "
                  f"entry_size={jtbl['entry_size']}, "
                  f"base=0x{base_addr:X}",
                  file=sys.stderr)
    elif jump_tables and not obj_path:
        for jtbl in jump_tables:
            print(f"Warning: Jump table {jtbl['symbol']} detected but no --obj "
                  f"provided; switch will not be decompiled correctly",
                  file=sys.stderr)

    # Build address-to-index map for the target side
    # Use the target addresses since we're extracting target binary
    addr_map = {}
    for idx, instr in enumerate(instructions):
        target = instr.get('target', {})
        addr_str = target.get('address', '')
        if addr_str:
            try:
                addr = int(addr_str, 16)
                addr_map[addr] = idx
            except ValueError:
                pass

    # Emit .text section header
    output.append(".text")

    # Emit function header
    output.append(f".global {label}")
    output.append(f"{label}:")

    # Emit instructions
    for instr in instructions:
        target = instr.get('target', {})
        addr_str = target.get('address', '')

        # Check if this address is a branch target - emit label
        if addr_str:
            try:
                addr = int(addr_str, 16)
                if addr in branch_targets:
                    output.append(f".L_{addr:08X}:")
            except ValueError:
                pass

        # Format the instruction
        asm = format_instruction(instr)
        if asm:
            # Convert branch target addresses to labels.
            #
            # ONLY on a branch. The trailing hex operand of a non-branch is an
            # immediate, not an address, and respelling it as a label emits
            # assembly m2c cannot parse -- `addi r3, r3, .L_00000120` for
            # `addi r3, r3, 0x120` -- whenever the immediate happens to equal
            # some branch's destination in the same function. See
            # `_is_branch_opcode` for the measured scale of that collision.
            if _is_branch_opcode(target.get('opcode', '')):
                match = _TRAILING_HEX_RE.search(asm)
                if match:
                    target_addr_str = match.group(1)
                    try:
                        target_addr = int(target_addr_str, 16)
                        if target_addr in branch_targets:
                            # Replace address with label reference
                            asm = asm[:match.start()] + f".L_{target_addr:08X}"
                    except ValueError:
                        pass

            output.append(f"\t{asm}")

    # Emit .rdata sections for jump tables
    if rdata_sections:
        output.append("")
        output.append(".rdata")
        for jtbl_symbol, target_addrs in rdata_sections:
            output.append(f".globl {jtbl_symbol}")
            output.append(f"{jtbl_symbol}:")
            for addr in target_addrs:
                output.append(f".word .L_{addr:08X}")

    return '\n'.join(output)


def main():
    parser = argparse.ArgumentParser(
        description="Convert objdiff JSON output to m2c-compatible assembly",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    parser.add_argument(
        "-i", "--input",
        help="Input JSON file (default: stdin)",
    )
    parser.add_argument(
        "-o", "--output",
        help="Output assembly file (default: stdout)",
    )
    parser.add_argument(
        "--symbol",
        help="Override the symbol name for the function label",
    )
    parser.add_argument(
        "--obj",
        help="Path to target OBJ file for jump table resolution",
    )
    parser.add_argument(
        "--project-dir",
        help="Project directory (auto-detects OBJ file from objdiff.json)",
    )
    parser.add_argument(
        "--use-base",
        action="store_true",
        help="Use base (compiled) instructions instead of target (original binary)",
    )
    args = parser.parse_args()

    # Read input
    try:
        if args.input:
            with open(args.input) as f:
                content = f.read()
        else:
            content = sys.stdin.read()
    except Exception as e:
        print(f"Error reading input: {e}", file=sys.stderr)
        sys.exit(1)

    # Parse JSON
    try:
        data = json.loads(content)
    except json.JSONDecodeError as e:
        print(f"Error parsing JSON: {e}", file=sys.stderr)
        sys.exit(1)

    # If --use-base is specified, swap target/base in instructions
    if args.use_base:
        for instr in data.get('instructions', []):
            instr['target'], instr['base'] = instr.get('base', {}), instr.get('target', {})

    # Resolve OBJ path for jump tables
    obj_path = args.obj
    if not obj_path and args.project_dir:
        symbol_name = data.get('symbol', data.get('demangled', ''))
        obj_path = find_obj_for_symbol(args.project_dir, symbol_name)
        if obj_path:
            print(f"Auto-detected OBJ: {obj_path}", file=sys.stderr)

    # Convert
    output = convert_objdiff_json(data, args.symbol, obj_path)

    if not output:
        sys.exit(1)

    # Write output
    if args.output:
        with open(args.output, 'w') as f:
            f.write(output)
            f.write('\n')
        print(f"Wrote {args.output}", file=sys.stderr)
    else:
        print(output)


if __name__ == '__main__':
    main()
