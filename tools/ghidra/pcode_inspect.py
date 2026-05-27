#!/usr/bin/env python3
"""
Inspect Ghidra decompilation for switch statements and cast operations.

Since pyghidra-mcp does not expose raw pcode, this script analyzes:
- The annotated decompiled C output (which includes switch detection comments)
- Raw bytes at the function address for PowerPC instruction analysis
- Cross-references for call target understanding

Usage:
    pcode_inspect.py "CharBones::PoseMeshes"
    pcode_inspect.py "0x82878b58"
    pcode_inspect.py "CharBones::PoseMeshes" --switches
    pcode_inspect.py "CharBones::PoseMeshes" --casts

The tool first searches for matching symbols to ensure the correct function
is decompiled. If multiple matches are found, it shows them and uses the best match.
"""

import argparse
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

from mcp_client import MCPClient, MCPError


def resolve_function(client: MCPClient, name_or_addr: str, verbose: bool = True) -> tuple[str, str]:
    """Resolve a function name or address to the exact symbol to decompile.

    Returns:
        Tuple of (resolved_name, address_hex) for decompilation

    Raises:
        MCPError if function cannot be found
    """
    # Check if input is already an address
    stripped = name_or_addr.strip().lower().replace("0x", "")
    if all(c in "0123456789abcdef" for c in stripped) and len(stripped) >= 6:
        # It's an address, use it directly
        addr = int(stripped, 16)
        return name_or_addr, f"0x{addr:08x}"

    # Search for the symbol
    if verbose:
        print(f"Searching for symbol: {name_or_addr}...", file=sys.stderr)

    search_result = client.search_symbols(name_or_addr, limit=20)

    if isinstance(search_result, dict):
        symbols = search_result.get("symbols", search_result.get("results", []))
    elif isinstance(search_result, list):
        symbols = search_result
    else:
        symbols = []

    if not symbols:
        raise MCPError(f"No symbols found matching '{name_or_addr}'")

    # Filter and score matches
    candidates = []
    for sym in symbols:
        if isinstance(sym, dict):
            sym_name = sym.get("name", "")
            sym_addr = sym.get("address", "")
        else:
            continue

        # Detect thunk/vtable/RTTI entries
        is_thunk = (
            "thunk" in sym_name.lower() or
            "$4PPPPPPPM" in sym_name or
            "vtordisp" in sym_name.lower() or
            "`vftable'" in sym_name
        )

        # Score based on how well it matches
        score = 0
        name_lower = sym_name.lower()
        query_lower = name_or_addr.lower()

        # Exact match on demangled name portion
        if query_lower in sym_name.lower():
            score += 10

        # STRONGLY prefer non-thunk functions (negative score for thunks)
        if is_thunk:
            score -= 50

        # Prefer longer symbol names (more specific)
        score += min(len(sym_name) / 100, 3)

        # Prefer symbols with actual function signatures
        if "@@" in sym_name and ("QAAX" in sym_name or "UAAX" in sym_name or "IAAX" in sym_name):
            score += 3

        candidates.append((score, sym_name, sym_addr, is_thunk))

    # Sort by score descending
    candidates.sort(key=lambda x: x[0], reverse=True)

    if not candidates:
        raise MCPError(f"No valid function symbols found for '{name_or_addr}'")

    # If multiple good candidates and verbose, show them
    if verbose and len(candidates) > 1:
        top_score = candidates[0][0]
        good_candidates = [c for c in candidates if c[0] >= top_score - 3]
        if len(good_candidates) > 1:
            print(f"Found {len(good_candidates)} candidate functions:", file=sys.stderr)
            for i, (score, name, addr, is_thunk) in enumerate(good_candidates[:5]):
                thunk_marker = " [thunk]" if is_thunk else ""
                print(f"  {i+1}. {name}{thunk_marker} @ 0x{addr}", file=sys.stderr)
            print(f"Using best match: {candidates[0][1]}", file=sys.stderr)

    best = candidates[0]
    return best[1], f"0x{best[2]}".lower()


# PowerPC instruction decoding helpers

def decode_ppc_opcode(word):
    """Return (primary_opcode, extended_opcode) for a 32-bit PPC instruction."""
    primary = (word >> 26) & 0x3F
    extended = (word >> 1) & 0x3FF
    return primary, extended


def decode_ppc_instructions(hex_data, base_addr):
    """Decode raw hex bytes into a list of (address, mnemonic, detail) tuples.

    Only decodes instructions relevant to switch/cast analysis.
    """
    raw = bytes.fromhex(hex_data)
    instructions = []
    for i in range(0, len(raw) - 3, 4):
        word = struct.unpack(">I", raw[i:i+4])[0]
        addr = base_addr + i
        primary, ext = decode_ppc_opcode(word)

        mnemonic = None
        detail = ""

        # bctr: opcode 19, ext 528, BO=20 (always), BI=0, LK=0
        if primary == 19 and ext == 528 and (word & 1) == 0:
            bo = (word >> 21) & 0x1F
            if bo == 20:
                mnemonic = "bctr"

        # bctrl: same but LK=1
        if primary == 19 and ext == 528 and (word & 1) == 1:
            bo = (word >> 21) & 0x1F
            if bo == 20:
                mnemonic = "bctrl"

        # mtctr: mtspr 9, rS -> opcode 31, ext 467, SPR=9
        if primary == 31 and ext == 467:
            spr_lo = (word >> 16) & 0x1F
            spr_hi = (word >> 11) & 0x1F  # wrong order intentional for PPC encoding
            # SPR encoding: spr_lo is bits 16-20, spr_hi is bits 11-15
            # CTR = SPR 9 = spr_lo=9, spr_hi=0
            if spr_lo == 9 and spr_hi == 0:
                rs = (word >> 21) & 0x1F
                mnemonic = "mtctr"
                detail = f"r{rs}"

        # cmplwi: opcode 10
        if primary == 10:
            bf = (word >> 23) & 0x7
            ra = (word >> 16) & 0x1F
            imm = word & 0xFFFF
            mnemonic = "cmplwi"
            detail = f"cr{bf}, r{ra}, {imm}"

        # cmpwi: opcode 11
        if primary == 11:
            bf = (word >> 23) & 0x7
            ra = (word >> 16) & 0x1F
            imm = word & 0xFFFF
            if imm & 0x8000:
                imm -= 0x10000
            mnemonic = "cmpwi"
            detail = f"cr{bf}, r{ra}, {imm}"

        # lwzx: opcode 31, ext 23
        if primary == 31 and ext == 23:
            rd = (word >> 21) & 0x1F
            ra = (word >> 16) & 0x1F
            rb = (word >> 11) & 0x1F
            mnemonic = "lwzx"
            detail = f"r{rd}, r{ra}, r{rb}"

        # rlwinm (used for zero-extension / masking): opcode 21
        if primary == 21:
            rs = (word >> 21) & 0x1F
            ra = (word >> 16) & 0x1F
            sh = (word >> 11) & 0x1F
            mb = (word >> 6) & 0x1F
            me = (word >> 1) & 0x1F
            mnemonic = "rlwinm"
            detail = f"r{ra}, r{rs}, {sh}, {mb}, {me}"

        # extsb: opcode 31, ext 954
        if primary == 31 and ext == 954:
            rs = (word >> 21) & 0x1F
            ra = (word >> 16) & 0x1F
            mnemonic = "extsb"
            detail = f"r{ra}, r{rs}"

        # extsh: opcode 31, ext 922
        if primary == 31 and ext == 922:
            rs = (word >> 21) & 0x1F
            ra = (word >> 16) & 0x1F
            mnemonic = "extsh"
            detail = f"r{ra}, r{rs}"

        # extsw: opcode 31, ext 986
        if primary == 31 and ext == 986:
            rs = (word >> 21) & 0x1F
            ra = (word >> 16) & 0x1F
            mnemonic = "extsw"
            detail = f"r{ra}, r{rs}"

        # clrldi / rldicl (used for unsigned extension): opcode 30, ext varies
        if primary == 30:
            rs = (word >> 21) & 0x1F
            ra = (word >> 16) & 0x1F
            mnemonic = "rldicl"
            detail = f"r{ra}, r{rs}, ..."

        if mnemonic:
            instructions.append((addr, mnemonic, detail))

    return instructions


# Analysis functions

def analyze_switches_from_bytes(instructions, base_addr):
    """Find switch patterns (bctr preceded by mtctr, lwzx, cmplwi) in decoded instructions."""
    switches = []
    for idx, (addr, mnem, detail) in enumerate(instructions):
        if mnem != "bctr":
            continue

        switch = {
            "address": f"0x{addr:08X}",
            "type": "unknown",
            "cases": None,
            "default": None,
            "comparison_register": None,
        }

        # Look backwards for the pattern
        lookback = min(idx, 30)
        for j in range(idx - 1, idx - lookback - 1, -1):
            if j < 0:
                break
            prev_addr, prev_mnem, prev_detail = instructions[j]

            if prev_mnem == "lwzx":
                switch["type"] = "jump table"

            if prev_mnem in ("cmplwi", "cmpwi"):
                parts = prev_detail.split(", ")
                if len(parts) >= 3:
                    try:
                        switch["cases"] = int(parts[2])
                        switch["comparison_register"] = parts[1]
                    except ValueError:
                        pass

        switches.append(switch)
    return switches


def analyze_switches_from_decompiled(code):
    """Extract switch info from the SWITCH STATEMENTS DETECTED annotation."""
    switches = []
    # Parse the annotation header that pyghidra-mcp adds
    pattern = re.compile(
        r"(\d+)\.\s+Address\s+((?:ram:)?0x[0-9a-fA-F]+)(?:\s*-\s*~(\d+)\s+cases)?",
    )
    for match in pattern.finditer(code):
        switch = {
            "address": match.group(2),
            "type": "jump table (from annotation)",
            "cases": int(match.group(3)) if match.group(3) else None,
            "default": None,
            "comparison_register": None,
        }
        switches.append(switch)
    return switches


def analyze_casts_from_bytes(instructions, base_addr):
    """Find sign/zero extension instructions in decoded PPC instructions."""
    casts = []
    for addr, mnem, detail in instructions:
        if mnem == "extsb":
            casts.append({
                "address": f"0x{addr:08X}",
                "operation": "INT_SEXT (signed byte extend)",
                "detail": detail,
            })
        elif mnem == "extsh":
            casts.append({
                "address": f"0x{addr:08X}",
                "operation": "INT_SEXT (signed halfword extend)",
                "detail": detail,
            })
        elif mnem == "extsw":
            casts.append({
                "address": f"0x{addr:08X}",
                "operation": "INT_SEXT (signed word extend)",
                "detail": detail,
            })
        elif mnem == "rlwinm":
            # rlwinm with specific masks is zero-extension
            parts = detail.split(", ")
            if len(parts) == 5:
                sh, mb, me = int(parts[2]), int(parts[3]), int(parts[4])
                if sh == 0:
                    if mb == 24 and me == 31:
                        casts.append({
                            "address": f"0x{addr:08X}",
                            "operation": "INT_ZEXT (unsigned byte mask, AND 0xFF)",
                            "detail": detail,
                        })
                    elif mb == 16 and me == 31:
                        casts.append({
                            "address": f"0x{addr:08X}",
                            "operation": "INT_ZEXT (unsigned halfword mask, AND 0xFFFF)",
                            "detail": detail,
                        })
    return casts


def analyze_casts_from_decompiled(code):
    """Find cast patterns in decompiled C output."""
    casts = []

    # Common Ghidra cast patterns
    patterns = [
        (r"\(int\)\(?(\w+)\)?", "signed cast (int)"),
        (r"\(uint\)\(?(\w+)\)?", "unsigned cast (uint)"),
        (r"\(short\)\(?(\w+)\)?", "signed cast (short)"),
        (r"\(ushort\)\(?(\w+)\)?", "unsigned cast (ushort)"),
        (r"\(char\)\(?(\w+)\)?", "signed cast (char)"),
        (r"\(uchar\)\(?(\w+)\)?", "unsigned cast (uchar)"),
        (r"\(byte\)\(?(\w+)\)?", "unsigned cast (byte)"),
        (r"\(longlong\)\(?(\w+)\)?", "signed cast (longlong)"),
        (r"\(ulonglong\)\(?(\w+)\)?", "unsigned cast (ulonglong)"),
        (r"SUB\d+\(\w+,\s*\d+\)", "sub-register extract (SUBn)"),
        (r"CONCAT\d+\([^)]+\)", "register concatenation (CONCATn)"),
        (r"SEXT\d+\([^)]+\)", "sign extension (SEXTn)"),
        (r"ZEXT\d+\([^)]+\)", "zero extension (ZEXTn)"),
    ]

    for line_num, line in enumerate(code.split("\n"), 1):
        for pattern, desc in patterns:
            for match in re.finditer(pattern, line):
                casts.append({
                    "line": line_num,
                    "operation": desc,
                    "text": match.group(0),
                    "context": line.strip(),
                })

    return casts


# Output formatting

def print_switches(switches, source_label=""):
    """Print switch analysis results."""
    if not switches:
        print(f"No switch statements detected{source_label}.")
        return

    print(f"Switch statements{source_label}:")
    print()
    for sw in switches:
        print(f"  Switch at {sw['address']}:")
        print(f"    Type: {sw['type']}")
        if sw.get("cases") is not None:
            print(f"    Cases: {sw['cases']}")
        if sw.get("default"):
            print(f"    Default: {sw['default']}")
        if sw.get("comparison_register"):
            print(f"    Comparison register: {sw['comparison_register']}")
        print()


def print_casts_bytes(casts):
    """Print cast analysis from raw bytes."""
    if not casts:
        print("No cast/extension instructions found in raw bytes.")
        return

    print("Cast operations (from raw instructions):")
    print()
    for c in casts:
        print(f"  {c['address']}: {c['operation']} on {c['detail']}")
    print()


def print_casts_decompiled(casts):
    """Print cast analysis from decompiled output."""
    if not casts:
        print("No cast patterns found in decompiled output.")
        return

    print("Cast operations (from decompiled C):")
    print()
    # Deduplicate by (operation, text)
    seen = set()
    for c in casts:
        key = (c["operation"], c["text"])
        if key in seen:
            continue
        seen.add(key)
        print(f"  Line {c['line']}: {c['operation']}")
        print(f"    {c['text']}")
        print(f"    Context: {c['context']}")
        print()


def main():
    parser = argparse.ArgumentParser(
        description="Inspect Ghidra decompilation for switch statements and cast operations."
    )
    parser.add_argument(
        "function",
        help='Function name (e.g. "CharBones::PoseMeshes") or address (e.g. "0x82878b58")',
    )
    parser.add_argument(
        "--switches", action="store_true",
        help="Show only switch statement analysis",
    )
    parser.add_argument(
        "--casts", action="store_true",
        help="Show only cast/extension analysis",
    )
    parser.add_argument(
        "--raw-bytes", type=int, default=0,
        help="Read N bytes from function address for instruction analysis (0 = auto from decompilation hints, max 8192)",
    )
    parser.add_argument(
        "--no-decompile", action="store_true",
        help="Skip decompilation, only analyze raw bytes",
    )
    parser.add_argument(
        "--address", action="store_true",
        help="Treat input as raw address (skip symbol search)",
    )
    args = parser.parse_args()

    show_all = not args.switches and not args.casts

    try:
        client = MCPClient()
        client.initialize()
    except MCPError as e:
        print(f"Error connecting to Ghidra MCP: {e}", file=sys.stderr)
        print("Is pyghidra-mcp running? Check: ./tools/ghidra/pyghidra-service.sh status", file=sys.stderr)
        sys.exit(1)

    # Resolve the function to decompile
    func_to_decompile = args.function
    func_address = None

    if not args.address:
        try:
            resolved_name, resolved_addr = resolve_function(client, args.function)
            func_to_decompile = resolved_addr  # Use address for more reliable lookup
            if resolved_addr.startswith("0x"):
                func_address = int(resolved_addr, 16)
        except MCPError as e:
            print(f"Warning: Symbol search failed: {e}", file=sys.stderr)
            print("Trying direct lookup...", file=sys.stderr)

    # Decompile the function
    decompiled_code = None

    if not args.no_decompile:
        try:
            print(f"Decompiling {func_to_decompile}...", file=sys.stderr)
            try:
                result = client.decompile_function(func_to_decompile)
            except MCPError as e:
                if "not found" in str(e).lower() and func_address is not None:
                    # Function object doesn't exist in Ghidra - create it first
                    addr_hex = f"{func_address:08x}"
                    print(f"Creating function at 0x{addr_hex}...", file=sys.stderr)
                    client.bulk_create_functions([addr_hex])
                    result = client.decompile_function(func_to_decompile)
                else:
                    raise

            if isinstance(result, dict):
                decompiled_code = result.get("code", result.get("decompiled_code", ""))
                func_name = result.get("name", result.get("function_name", args.function))
                # Try to extract address from the name field (format: "Name-ram:0xADDR")
                name_str = str(func_name)
                addr_match = re.search(r"(?:ram:)?0x([0-9a-fA-F]+)", name_str)
                if addr_match:
                    func_address = int(addr_match.group(1), 16)
            elif isinstance(result, str):
                decompiled_code = result
            else:
                decompiled_code = str(result)

        except MCPError as e:
            print(f"Decompilation failed: {e}", file=sys.stderr)
            if not args.raw_bytes:
                sys.exit(1)

    # Try to resolve address from input if we don't have it yet
    if func_address is None:
        stripped = args.function.strip().lower().replace("0x", "")
        if all(c in "0123456789abcdef" for c in stripped) and len(stripped) >= 6:
            try:
                func_address = int(stripped, 16)
            except ValueError:
                pass

    # Analyze raw bytes if we have an address
    byte_instructions = []
    read_size = args.raw_bytes if args.raw_bytes > 0 else 4096

    if func_address is not None:
        try:
            print(f"Reading {read_size} bytes at 0x{func_address:08X}...", file=sys.stderr)
            byte_result = client.read_bytes(f"0x{func_address:08x}", size=min(read_size, 8192))
            if isinstance(byte_result, dict):
                hex_data = byte_result.get("data", "")
            else:
                hex_data = str(byte_result)

            if hex_data:
                byte_instructions = decode_ppc_instructions(hex_data, func_address)
        except MCPError as e:
            print(f"Warning: Could not read bytes: {e}", file=sys.stderr)

    # Run analyses
    print()

    if show_all or args.switches:
        # Switch analysis from annotation
        if decompiled_code:
            annotation_switches = analyze_switches_from_decompiled(decompiled_code)
            if annotation_switches:
                print_switches(annotation_switches, " (from Ghidra annotation)")

        # Switch analysis from raw bytes
        if byte_instructions:
            byte_switches = analyze_switches_from_bytes(byte_instructions, func_address or 0)
            if byte_switches:
                print_switches(byte_switches, " (from raw instruction analysis)")

        if not decompiled_code and not byte_instructions:
            print("No switch analysis available (no decompilation or bytes).")
        print()

    if show_all or args.casts:
        # Cast analysis from raw bytes
        if byte_instructions:
            byte_casts = analyze_casts_from_bytes(byte_instructions, func_address or 0)
            print_casts_bytes(byte_casts)

        # Cast analysis from decompiled output
        if decompiled_code:
            decomp_casts = analyze_casts_from_decompiled(decompiled_code)
            print_casts_decompiled(decomp_casts)

        if not decompiled_code and not byte_instructions:
            print("No cast analysis available (no decompilation or bytes).")
        print()

    # Print full decompiled output in default mode
    if show_all and decompiled_code:
        print("=" * 70)
        print("Decompiled output:")
        print("=" * 70)
        print(decompiled_code)


if __name__ == "__main__":
    main()
