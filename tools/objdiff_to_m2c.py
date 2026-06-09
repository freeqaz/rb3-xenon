#!/usr/bin/env python3
"""
Convert objdiff JSON output to m2c-compatible assembly format.

This script parses the JSON output from objdiff-cli (with --include-instructions)
and converts it to GNU-as style assembly that m2c can process.

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


def parse_branch_targets(instructions: list) -> set:
    """
    Identify branch target addresses from branch instructions.
    Returns a set of addresses that are branch targets.
    """
    targets = set()
    branch_opcodes = {
        'b', 'bl', 'ba', 'bla',
        'bc', 'bcl', 'bca', 'bcla',
        'bclr', 'bclrl', 'bcctr', 'bcctrl',
        'beq', 'bne', 'blt', 'bgt', 'ble', 'bge',
        'beqlr', 'bnelr', 'bltlr', 'bgtlr', 'blelr', 'bgelr',
        'bdnz', 'bdz', 'bdnzl', 'bdzl',
        'bdzf', 'bdzt', 'bdnzf', 'bdnzt',
    }

    for instr in instructions:
        target = instr.get('target', {})
        opcode = target.get('opcode', '')

        # Check if this is a branch instruction
        if opcode in branch_opcodes or opcode.startswith('b'):
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
    MSVC mangled names contain ? and @ which need quoting.
    """
    # Check if quoting is needed
    if '?' in sym or '@' in sym or '$' in sym or '<' in sym or '>' in sym:
        # Don't double-quote
        if sym.startswith('"') and sym.endswith('"'):
            return sym
        return f'"{sym}"'
    return sym


def _is_reloc_symbol(s: str) -> bool:
    """Check if a string looks like a relocation symbol appended by objdiff."""
    # MSVC mangled names, labels, merged symbols
    return (s.startswith('?') or s.startswith('merged_') or
            s.startswith('lbl_') or s.startswith('jumptable_') or
            s.startswith('switch_') or s.startswith('__jtbl') or
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
            if operand and not operand.startswith('0x') and not operand.lstrip('-').isdigit():
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
            if last and not last.startswith('0x') and not last.lstrip('-').isdigit():
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
            if offset and not offset.startswith('0x') and not offset.lstrip('-').isdigit():
                return f"{opcode} {reg_dest}, {quote_symbol(offset)}@l({reg_base})"
            # Format: "lwz r11, 0x4c, r3" -> "lwz r11, 0x4c(r3)"
            return f"{opcode} {reg_dest}, {offset}({reg_base})"
        elif len(parts) == 4:
            # Format with relocation: "lwz r11, 0x3c, r10, ?TheTaskMgr..." -> "lwz r11, 0x3c(r10)"
            reg_dest, offset, reg_base, _reloc = parts
            return f"{opcode} {reg_dest}, {offset}({reg_base})"

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
            # Convert branch target addresses to labels
            # Look for branch to hex address pattern
            match = re.search(r'(0x[0-9a-fA-F]+)$', asm)
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
