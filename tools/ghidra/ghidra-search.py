#!/usr/bin/env python3
"""
Search for symbols, strings, or code in the Ghidra project.

Usage:
    ghidra-search.py symbols GameMode
    ghidra-search.py strings "error"
    ghidra-search.py code "switch case"
"""

import argparse
import json
import sys
from pathlib import Path

# Add parent directory to path for imports
sys.path.insert(0, str(Path(__file__).parent))

from mcp_client import MCPClient, MCPError, DEFAULT_BINARY


def format_symbols(result: dict) -> str:
    """Format symbol search results for display."""
    lines = []
    symbols = result.get("symbols", result.get("results", []))

    if not symbols:
        return "No symbols found."

    # Header
    lines.append(f"Found {len(symbols)} symbol(s):")
    lines.append("")
    lines.append(f"{'Address':<14} {'Type':<12} Name")
    lines.append("-" * 60)

    for sym in symbols:
        addr = sym.get("address", "unknown")
        sym_type = sym.get("type", sym.get("symbol_type", "unknown"))
        name = sym.get("name", "unknown")
        lines.append(f"{addr:<14} {sym_type:<12} {name}")

    total = result.get("total_count", len(symbols))
    if total > len(symbols):
        lines.append("")
        lines.append(f"(Showing {len(symbols)} of {total} results)")

    return "\n".join(lines)


def format_strings(result: dict) -> str:
    """Format string search results for display."""
    lines = []
    strings = result.get("strings", result.get("results", []))

    if not strings:
        return "No strings found."

    lines.append(f"Found {len(strings)} string(s):")
    lines.append("")
    lines.append(f"{'Address':<14} String")
    lines.append("-" * 60)

    for s in strings:
        addr = s.get("address", "unknown")
        value = s.get("value", s.get("string", "unknown"))
        # Truncate long strings
        if len(value) > 60:
            value = value[:57] + "..."
        # Escape newlines for display
        value = value.replace("\n", "\\n").replace("\r", "\\r")
        lines.append(f"{addr:<14} {value}")

    return "\n".join(lines)


def format_code(result: dict) -> str:
    """Format code search results for display."""
    lines = []
    matches = result.get("matches", result.get("results", []))

    if not matches:
        return "No code matches found."

    lines.append(f"Found {len(matches)} match(es):")

    for i, match in enumerate(matches):
        lines.append("")
        lines.append(f"--- Match {i+1} ---")
        func = match.get("function", match.get("function_name", "unknown"))
        addr = match.get("address", "unknown")
        lines.append(f"Function: {func} @ {addr}")

        # Show matched code if available
        code = match.get("code", match.get("matched_code", match.get("context", "")))
        if code:
            lines.append("")
            for line in code.split("\n"):
                lines.append(f"  {line}")

    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(
        description="Search Ghidra project for symbols, strings, or code",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s symbols GameMode        # Search for symbols containing "GameMode"
  %(prog)s strings "error"         # Search for strings containing "error"
  %(prog)s code "blr"              # Search for code patterns
  %(prog)s symbols Poll --limit 50 # Get more results
        """
    )
    parser.add_argument(
        "type",
        choices=["symbols", "strings", "code"],
        help="Type of search to perform"
    )
    parser.add_argument(
        "query",
        help="Search query"
    )
    parser.add_argument(
        "--binary", "-b",
        default=DEFAULT_BINARY,
        help=f"Binary name (default: {DEFAULT_BINARY})"
    )
    parser.add_argument(
        "--limit", "-l",
        type=int,
        default=25,
        help="Maximum number of results (default: 25)"
    )
    parser.add_argument(
        "--offset", "-o",
        type=int,
        default=0,
        help="Skip first N results (for pagination)"
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

        # Perform the appropriate search
        if args.type == "symbols":
            result = client.search_symbols(args.query, offset=args.offset, limit=args.limit)
            formatter = format_symbols
        elif args.type == "strings":
            result = client.search_strings(args.query, limit=args.limit)
            formatter = format_strings
        elif args.type == "code":
            result = client.search_code(args.query, limit=args.limit)
            formatter = format_code
        else:
            print(f"Unknown search type: {args.type}", file=sys.stderr)
            sys.exit(1)

        if args.json:
            print(json.dumps(result, indent=2))
        else:
            print(formatter(result))

    except MCPError as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)
    except KeyboardInterrupt:
        sys.exit(130)


if __name__ == "__main__":
    main()
