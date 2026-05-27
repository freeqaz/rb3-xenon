#!/usr/bin/env python3
"""
List cross-references for a symbol or address in the Ghidra project.

Usage:
    ghidra-xrefs.py "Symbol::Symbol"
    ghidra-xrefs.py 0x82000000
"""

import argparse
import json
import sys
from pathlib import Path

# Add parent directory to path for imports
sys.path.insert(0, str(Path(__file__).parent))

from mcp_client import MCPClient, MCPError, DEFAULT_BINARY


def format_xrefs(result: dict, target: str) -> str:
    """Format cross-reference results for display."""
    lines = []

    # Get the references lists
    refs_to = result.get("references_to", result.get("xrefs_to", []))
    refs_from = result.get("references_from", result.get("xrefs_from", []))

    lines.append(f"Cross-references for: {target}")
    lines.append("")

    # References TO this location (callers/users)
    lines.append(f"=== References TO ({len(refs_to)}) ===")
    if refs_to:
        lines.append(f"{'From Address':<14} {'Type':<10} From Function")
        lines.append("-" * 60)
        for ref in refs_to:
            from_addr = ref.get("from_address", ref.get("address", "unknown"))
            ref_type = ref.get("type", ref.get("ref_type", "unknown"))
            from_func = ref.get("from_function", ref.get("function", ""))
            lines.append(f"{from_addr:<14} {ref_type:<10} {from_func}")
    else:
        lines.append("  (none)")

    lines.append("")

    # References FROM this location (callees/targets)
    lines.append(f"=== References FROM ({len(refs_from)}) ===")
    if refs_from:
        lines.append(f"{'To Address':<14} {'Type':<10} To Symbol")
        lines.append("-" * 60)
        for ref in refs_from:
            to_addr = ref.get("to_address", ref.get("address", "unknown"))
            ref_type = ref.get("type", ref.get("ref_type", "unknown"))
            to_sym = ref.get("to_symbol", ref.get("symbol", ""))
            lines.append(f"{to_addr:<14} {ref_type:<10} {to_sym}")
    else:
        lines.append("  (none)")

    # Summary
    lines.append("")
    lines.append(f"Total: {len(refs_to)} references to, {len(refs_from)} references from")

    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(
        description="List cross-references for a symbol or address",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s "Symbol::Symbol"    # Find all references to/from Symbol::Symbol
  %(prog)s 0x82000000          # Find references to/from address
  %(prog)s "Game::Poll" --json # Output raw JSON
        """
    )
    parser.add_argument(
        "target",
        help="Symbol name or address to find cross-references for"
    )
    parser.add_argument(
        "--binary", "-b",
        default=DEFAULT_BINARY,
        help=f"Binary name (default: {DEFAULT_BINARY})"
    )
    parser.add_argument(
        "--json", "-j",
        action="store_true",
        help="Output raw JSON instead of formatted results"
    )
    parser.add_argument(
        "--reinit",
        action="store_true",
        help="Force session re-initialization"
    )

    args = parser.parse_args()

    try:
        client = MCPClient(binary=args.binary)
        client.initialize(force=args.reinit)

        result = client.list_xrefs(args.target)

        if args.json:
            print(json.dumps(result, indent=2))
        else:
            print(format_xrefs(result, args.target))

    except MCPError as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)
    except KeyboardInterrupt:
        sys.exit(130)


if __name__ == "__main__":
    main()
