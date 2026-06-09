"""diff-asm: compile two variants and diff assembly listings.

Compiles both source files with assembly listings, normalizes them
to remove non-semantic differences, then diffs. Detects consistent
register renaming patterns (e.g. r11<->r10 swaps) separately from
semantic changes.
"""

import difflib
import re
import sys
import tempfile
from collections import Counter
from pathlib import Path
from typing import Dict, List, Optional, Tuple

from .invoker import CompilerInvoker


def extract_function(lines: List[str], func_name: str) -> List[str]:
    """Extract a single function from an MSVC assembly listing.

    Matches function boundaries using PROC NEAR / ENDP markers.
    func_name can be a mangled name (e.g. ?ResetSurfaces@DxTex@@AAAXXZ)
    or a substring to match (e.g. ResetSurfaces).
    """
    result = []
    in_function = False

    for line in lines:
        stripped = line.rstrip()

        # Match PROC NEAR line: "?Name@Class@@... PROC NEAR"
        if not in_function and "PROC NEAR" in stripped:
            # Extract the symbol name (everything before " PROC NEAR")
            sym = stripped.split(" PROC NEAR")[0].strip()
            if func_name in sym or sym == func_name:
                in_function = True
                result.append(line)
                continue

        if in_function:
            result.append(line)
            # Match ENDP line: "?Name@Class@@... ENDP"
            if " ENDP" in stripped and "EXTRN" not in stripped:
                break

    if not result:
        # Try case-insensitive search as fallback
        for line in lines:
            stripped = line.rstrip()
            if "PROC NEAR" in stripped:
                sym = stripped.split(" PROC NEAR")[0].strip()
                if func_name.lower() in sym.lower():
                    in_function = True
                    result.append(line)
                    continue
            if in_function:
                result.append(line)
                if " ENDP" in stripped and "EXTRN" not in stripped:
                    break

    return result


def normalize_listing(lines: List[str], source_name: str = "") -> List[str]:
    """Normalize an assembly listing for diffing.

    Strips:
    - Header lines (TITLE, .PPC, .MODEL, PUBLIC, COMDAT metadata)
    - Address columns (hex prefixes on instruction lines)
    - File paths (Z:\\path\\to\\file)
    - Trailing whitespace
    """
    result = []
    skip_prefixes = (
        "; Listing generated",
        "\tTITLE",
        "\t.PPC",
        "\t.MODEL",
        "PUBLIC",
        ";	COMDAT",
        ".XBLD",
        "__C1_",
        "__C2_",
        "\tDW\t",
        "\tDB\t",
    )

    for line in lines:
        stripped = line.rstrip()

        # Skip header/metadata lines
        if any(stripped.startswith(p) for p in skip_prefixes):
            continue
        if stripped == "END":
            continue
        if not stripped:
            continue

        # Normalize file paths in source line comments
        if source_name:
            stripped = stripped.replace(source_name, "<source>")
        # Generic path normalization
        stripped = re.sub(r"[A-Z]:\\[^\s]+", "<path>", stripped)

        # Strip address + hex code prefix from instruction lines
        # Format: "  00008	81230000	 lwz ..." -> "lwz ..."
        stripped = re.sub(r"^\s+[0-9a-f]+\t[0-9a-f]+\t ", "\t", stripped)

        result.append(stripped)
    return result


def detect_register_swaps(
    lines_a: List[str], lines_b: List[str], strict: bool = True
) -> Dict[str, str]:
    """Detect consistent register swaps between two listings.

    Returns a mapping of register renames (e.g. {"r11": "r10", "r10": "r11"}).

    Args:
        lines_a: Assembly lines from variant A.
        lines_b: Assembly lines from variant B.
        strict: If True (default), only consider lines that differ ONLY in
            register names (original behavior). If False, also extract register
            pairs from lines with mixed register+non-register differences,
            requiring 2+ consistent occurrences for each swap pair.
    """
    # PPC register pattern
    reg_pattern = re.compile(r"\br(\d+)\b")

    # Find lines that differ only in register names
    swap_candidates: Counter = Counter()
    total_diffs = 0

    for a, b in zip(lines_a, lines_b):
        if a == b:
            continue

        # Extract registers from both lines
        regs_a = reg_pattern.findall(a)
        regs_b = reg_pattern.findall(b)

        if len(regs_a) != len(regs_b):
            if not strict:
                # In relaxed mode, try to extract positional register pairs
                # from matching register positions
                _extract_positional_pairs(a, b, reg_pattern, swap_candidates)
            continue

        # Check if lines differ only in register names
        a_no_regs = reg_pattern.sub("rX", a)
        b_no_regs = reg_pattern.sub("rX", b)
        if a_no_regs != b_no_regs:
            if not strict:
                # In relaxed mode, still extract register pairs from matching positions
                for ra, rb in zip(regs_a, regs_b):
                    if ra != rb:
                        pair = tuple(sorted([f"r{ra}", f"r{rb}"]))
                        swap_candidates[pair] += 1
                total_diffs += 1
            continue

        # Record register pairings
        total_diffs += 1
        for ra, rb in zip(regs_a, regs_b):
            if ra != rb:
                pair = tuple(sorted([f"r{ra}", f"r{rb}"]))
                swap_candidates[pair] += 1

    if total_diffs == 0 and not swap_candidates:
        return {}

    # A swap is "consistent" if it accounts for register differences
    min_count = 2 if not strict else 1
    swaps = {}
    for (ra, rb), count in swap_candidates.most_common():
        if count < min_count:
            continue
        if ra not in swaps and rb not in swaps:
            swaps[ra] = rb
            swaps[rb] = ra

    return swaps


def _extract_positional_pairs(
    line_a: str,
    line_b: str,
    reg_pattern: re.Pattern,
    candidates: Counter,
) -> None:
    """Extract register swap pairs from lines with different register counts.

    Uses token splitting to align register mentions by position in the
    non-register text structure.
    """
    # Split both lines by non-register content and try to align
    parts_a = reg_pattern.split(line_a)
    parts_b = reg_pattern.split(line_b)
    regs_a = reg_pattern.findall(line_a)
    regs_b = reg_pattern.findall(line_b)

    # Only try if the non-register structure partially matches
    if not parts_a or not parts_b:
        return

    # Simple alignment: match registers at same text positions
    min_regs = min(len(regs_a), len(regs_b))
    for i in range(min_regs):
        if regs_a[i] != regs_b[i]:
            pair = tuple(sorted([f"r{regs_a[i]}", f"r{regs_b[i]}"]))
            candidates[pair] += 1


def apply_register_map(
    lines: List[str], reg_map: Dict[str, str]
) -> List[str]:
    """Apply a register renaming map to assembly lines."""
    if not reg_map:
        return lines

    def replace_regs(match):
        reg = match.group(0)
        return reg_map.get(reg, reg)

    pattern = re.compile(r"\br\d+\b")
    return [pattern.sub(replace_regs, line) for line in lines]


def format_diff(
    lines_a: List[str],
    lines_b: List[str],
    name_a: str,
    name_b: str,
    reg_swaps: Dict[str, str],
) -> str:
    """Format the diff output with register swap annotations."""
    output = []

    # Register swap summary
    if reg_swaps:
        # Deduplicate bidirectional swaps
        seen = set()
        pairs = []
        for ra, rb in reg_swaps.items():
            pair = tuple(sorted([ra, rb]))
            if pair not in seen:
                seen.add(pair)
                pairs.append(pair)
        output.append("Register swaps detected:")
        for ra, rb in pairs:
            output.append(f"  {ra} <-> {rb}")
        output.append("")

    # Show diff after applying register normalization
    normalized_b = apply_register_map(lines_b, reg_swaps)
    remaining_diffs = []
    for i, (a, b) in enumerate(zip(lines_a, normalized_b)):
        if a != b:
            remaining_diffs.append((i, a, b))

    if remaining_diffs:
        output.append(f"Semantic differences (after register normalization): {len(remaining_diffs)}")
        output.append("")

    # Full unified diff (original, not normalized)
    diff = difflib.unified_diff(
        lines_a, lines_b, fromfile=name_a, tofile=name_b, lineterm=""
    )
    diff_lines = list(diff)
    if diff_lines:
        output.append("--- Assembly diff ---")
        output.extend(diff_lines)
    else:
        output.append("No differences found.")

    return "\n".join(output)


def cmd_diff_asm(args) -> None:
    """Entry point for diff-asm subcommand."""
    source_a = Path(args.source_a).resolve()
    source_b = Path(args.source_b).resolve()

    if not source_a.exists():
        print(f"Error: {source_a} not found", file=sys.stderr)
        sys.exit(1)
    if not source_b.exists():
        print(f"Error: {source_b} not found", file=sys.stderr)
        sys.exit(1)

    invoker = CompilerInvoker()

    if args.output_dir:
        output_dir = Path(args.output_dir).resolve()
    else:
        output_dir = Path(tempfile.mkdtemp(prefix="asm_diff_"))

    dir_a = output_dir / "a"
    dir_b = output_dir / "b"

    listing_type = args.listing_type

    print(f"Compiling {source_a.name}...")
    result_a = invoker.compile_with_asm(source_a, dir_a, listing_type=listing_type)
    if result_a.returncode != 0:
        print(f"Compilation failed for {source_a.name}:", file=sys.stderr)
        print(result_a.stderr, file=sys.stderr)
        sys.exit(1)

    print(f"Compiling {source_b.name}...")
    result_b = invoker.compile_with_asm(source_b, dir_b, listing_type=listing_type)
    if result_b.returncode != 0:
        print(f"Compilation failed for {source_b.name}:", file=sys.stderr)
        print(result_b.stderr, file=sys.stderr)
        sys.exit(1)

    # Find the .asm/.cod files
    asm_a = _find_listing(dir_a)
    asm_b = _find_listing(dir_b)
    if not asm_a or not asm_b:
        print("Error: assembly listing not generated", file=sys.stderr)
        sys.exit(1)

    print(f"Listings: {asm_a}, {asm_b}")
    print()

    raw_a = asm_a.read_text().splitlines()
    raw_b = asm_b.read_text().splitlines()

    func_filter = getattr(args, "function", None)
    if func_filter:
        func_a = extract_function(raw_a, func_filter)
        func_b = extract_function(raw_b, func_filter)
        if not func_a:
            print(f"Error: function '{func_filter}' not found in {asm_a}", file=sys.stderr)
            sys.exit(1)
        if not func_b:
            print(f"Error: function '{func_filter}' not found in {asm_b}", file=sys.stderr)
            sys.exit(1)
        print(f"Filtered to function: {func_filter} ({len(func_a)} / {len(func_b)} lines)")
        print()
        raw_a = func_a
        raw_b = func_b

    norm_a = normalize_listing(raw_a, source_a.name)
    norm_b = normalize_listing(raw_b, source_b.name)

    swaps = detect_register_swaps(norm_a, norm_b)
    output = format_diff(norm_a, norm_b, source_a.name, source_b.name, swaps)
    print(output)


def _find_listing(directory: Path) -> Optional[Path]:
    """Find the assembly listing file in a directory."""
    for ext in (".cod", ".asm"):
        files = list(directory.glob(f"*{ext}"))
        if files:
            return files[0]
    return None
