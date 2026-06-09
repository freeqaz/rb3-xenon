"""annotate: disassemble c2.dll at funcmap-identified addresses.

Uses objdump to disassemble c2.dll regions around addresses flagged by
callgrind-diff experiments. Overlays callgrind execution counts when
available, and highlights funcmap addresses with observation counts.
"""

import subprocess
import sys
from pathlib import Path
from typing import Dict, List, Optional, Tuple

from .invoker import C2_DLL, C2_IMAGE_BASE, C2_TEXT_START, C2_TEXT_END
from .funcmap import C2FuncMap, DEFAULT_FUNCMAP_PATH
from .callgrind_diff import parse_callgrind_output


def _check_objdump() -> bool:
    """Check if objdump is available."""
    try:
        result = subprocess.run(
            ["objdump", "--version"], capture_output=True, text=True
        )
        return result.returncode == 0
    except FileNotFoundError:
        return False


def disassemble_region(
    dll_path: Path,
    start_va: int,
    end_va: int,
) -> List[Tuple[int, str, str]]:
    """Disassemble a region of c2.dll using objdump.

    c2.dll is a 32-bit x86 PE. objdump can disassemble it with -m i386.
    We use file offsets derived from VA - IMAGE_BASE (since .text RVA = 0x1000
    and file offset of .text = 0x1000 for most PE files).

    Returns [(address, hex_bytes, instruction_text), ...].
    """
    # Convert VA to file-relative addresses for objdump
    # objdump uses VMA (virtual memory address) for PE files
    result = subprocess.run(
        [
            "objdump",
            "-d",
            "-M", "intel",
            f"--start-address=0x{start_va:x}",
            f"--stop-address=0x{end_va:x}",
            str(dll_path),
        ],
        capture_output=True,
        text=True,
    )

    if result.returncode != 0:
        return []

    instructions = []
    for line in result.stdout.splitlines():
        line = line.strip()
        if not line or line.startswith("Disassembly") or line.endswith(":"):
            continue

        # objdump format: "  10b01234:	89 c0                	mov    eax,eax"
        if ":\t" not in line:
            continue

        parts = line.split(":\t", 1)
        try:
            addr = int(parts[0].strip(), 16)
        except ValueError:
            continue

        rest = parts[1]
        # Split hex bytes from instruction
        tab_parts = rest.split("\t", 1)
        hex_bytes = tab_parts[0].strip()
        insn_text = tab_parts[1].strip() if len(tab_parts) > 1 else ""

        instructions.append((addr, hex_bytes, insn_text))

    return instructions


def annotate_region(
    instructions: List[Tuple[int, str, str]],
    funcmap: C2FuncMap,
    callgrind_counts: Optional[Dict[int, int]] = None,
) -> List[str]:
    """Annotate disassembly with funcmap and callgrind data.

    Returns formatted lines ready for display.
    """
    # Build a quick lookup of funcmap addresses
    funcmap_addrs: Dict[int, int] = {}  # VA -> observation count
    for key, entry in funcmap.entries.items():
        va = entry["va"]
        funcmap_addrs[va] = len(entry["observations"])

    lines = []
    for addr, hex_bytes, insn_text in instructions:
        # Callgrind count column
        if callgrind_counts and addr in callgrind_counts:
            count_str = f"{callgrind_counts[addr]:>8}"
        else:
            count_str = "        "

        # Funcmap marker
        if addr in funcmap_addrs:
            obs = funcmap_addrs[addr]
            marker = f" <<< [{obs} obs]"
        else:
            marker = ""

        rva = addr - C2_IMAGE_BASE
        lines.append(
            f"{count_str}  0x{rva:06x}  {hex_bytes:<24} {insn_text}{marker}"
        )

    return lines


def cmd_annotate(args) -> None:
    """Entry point for annotate subcommand."""
    if not _check_objdump():
        print("Error: objdump not found", file=sys.stderr)
        sys.exit(1)

    if not C2_DLL.exists():
        print(f"Error: c2.dll not found at {C2_DLL}", file=sys.stderr)
        sys.exit(1)

    # Load funcmap
    funcmap_path = Path(args.funcmap) if args.funcmap else DEFAULT_FUNCMAP_PATH
    if not funcmap_path.exists():
        print(f"Error: funcmap not found at {funcmap_path}", file=sys.stderr)
        print("Run callgrind-diff first to build the funcmap.", file=sys.stderr)
        sys.exit(1)

    funcmap = C2FuncMap(funcmap_path)

    # Load callgrind data if provided
    callgrind_counts = None
    if args.callgrind:
        cg_path = Path(args.callgrind)
        if cg_path.exists():
            print(f"Loading callgrind data from {cg_path}...")
            callgrind_counts = parse_callgrind_output(cg_path)
            print(f"  {len(callgrind_counts)} c2.dll addresses with counts")
        else:
            print(f"Warning: callgrind file {cg_path} not found", file=sys.stderr)

    context_bytes = args.context

    if args.address:
        # Annotate a specific address
        addr_str = args.address
        if addr_str.startswith("0x") or addr_str.startswith("0X"):
            rva = int(addr_str, 16)
        else:
            rva = int(addr_str)

        va = rva + C2_IMAGE_BASE
        start = va - context_bytes
        end = va + context_bytes

        # Clamp to .text range
        start = max(start, C2_TEXT_START)
        end = min(end, C2_TEXT_END)

        print(f"\nc2.dll disassembly at RVA 0x{rva:06x} (VA 0x{va:08x}):")
        print(f"{'Count':>8}  {'RVA':<10} {'Bytes':<24} {'Instruction'}")
        print("-" * 80)

        insns = disassemble_region(C2_DLL, start, end)
        lines = annotate_region(insns, funcmap, callgrind_counts)
        for line in lines:
            print(line)

    else:
        # Annotate top N funcmap addresses by observation count
        top_n = args.top
        bps = funcmap.get_breakpoints(min_evidence=1)

        if not bps:
            print("No funcmap entries found. Run callgrind-diff to populate.")
            return

        # Take top N by observation count
        bps = bps[:top_n]
        print(f"Top {len(bps)} funcmap addresses ({funcmap.summary()}):")
        print()

        for va, entry in bps:
            rva = entry["rva"]
            obs_count = len(entry["observations"])
            label = entry.get("label", "")

            # Disassemble surrounding region
            start = va - context_bytes
            end = va + context_bytes
            start = max(start, C2_TEXT_START)
            end = min(end, C2_TEXT_END)

            header = f"RVA 0x{rva:06x} ({obs_count} observations)"
            if label:
                header += f" [{label}]"
            print(f"--- {header} ---")

            # Show observation tags
            tags = set(o["tag"] for o in entry["observations"])
            print(f"  Evidence: {', '.join(sorted(tags))}")

            print(f"  {'Count':>8}  {'RVA':<10} {'Bytes':<24} {'Instruction'}")
            print("  " + "-" * 76)

            insns = disassemble_region(C2_DLL, start, end)
            lines = annotate_region(insns, funcmap, callgrind_counts)
            for line in lines:
                print(f"  {line}")
            print()
