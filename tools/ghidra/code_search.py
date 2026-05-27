#!/usr/bin/env python3
"""
Semantic search over Ghidra decompiled code via pyghidra-mcp's ChromaDB vector index.

Usage:
    python3 tools/ghidra/code_search.py "iterates vector and deletes each element"
    python3 tools/ghidra/code_search.py --code "for (i = 0; i < count; i++) { arr[i]->Save(bs); }"
    python3 tools/ghidra/code_search.py --strings "CharBones"
    python3 tools/ghidra/code_search.py "switch on message type" --limit 5
    python3 tools/ghidra/code_search.py "wind random" --exclude "__unwind" --exclude "thunk"
"""

import argparse
import re
import sys
import textwrap

from mcp_client import MCPClient, MCPError, create_client

# Default patterns to exclude from search results (noise)
DEFAULT_EXCLUDE_PATTERNS = [
    r"^__unwind\$",           # Exception handling unwind stubs
    r"^__ehhandler\$",        # Exception handler stubs
    r"_GLOBAL_",              # Global constructor stubs
    r"\$vectored",            # Vectored exception handlers
]


def format_code_results(query: str, results, exclude_patterns: list = None) -> str:
    """Format search_code results for display.

    Args:
        query: The search query
        results: Raw results from MCP server
        exclude_patterns: List of regex patterns to filter out from results
    """
    exclude_patterns = exclude_patterns or []
    lines = [f'Results for: "{query}"', ""]

    if isinstance(results, str):
        return f"{lines[0]}\n\n{results}"

    if isinstance(results, dict):
        items = results.get("results", results.get("matches", []))
        if not items and "error" in results:
            return f"{lines[0]}\n\nError: {results['error']}"
        if not items:
            # Maybe the whole dict is a single result or raw text
            return f"{lines[0]}\n\n{results}"
    elif isinstance(results, list):
        items = results
    else:
        return f"{lines[0]}\n\n{results}"

    if not items:
        lines.append("No results found.")
        return "\n".join(lines)

    # Filter out noise patterns
    filtered_count = 0
    filtered_items = []
    for item in items:
        if isinstance(item, str):
            name = item
        else:
            name = item.get("function_name", item.get("name", item.get("symbol", "unknown")))

        # Check if name matches any exclude pattern
        excluded = False
        for pattern in exclude_patterns:
            if re.search(pattern, name):
                excluded = True
                filtered_count += 1
                break

        if not excluded:
            filtered_items.append(item)

    if filtered_count > 0:
        lines.append(f"(Filtered out {filtered_count} noise results)")
        lines.append("")

    if not filtered_items:
        lines.append("No relevant results found after filtering.")
        return "\n".join(lines)

    for i, item in enumerate(filtered_items, 1):
        if isinstance(item, str):
            lines.append(f"{i}. {item}")
            lines.append("")
            continue

        name = item.get("function_name", item.get("name", item.get("symbol", "unknown")))
        address = item.get("address", item.get("entry_point", ""))
        score = item.get("score", item.get("distance", item.get("similarity", "")))
        code = item.get("code", item.get("decompiled", item.get("snippet", item.get("text", ""))))

        header = f"{i}. {name}"
        if score != "":
            header += f" (score: {score})"
        lines.append(header)

        if address:
            addr_str = address if isinstance(address, str) else f"0x{address:08x}"
            lines.append(f"   Address: {addr_str}")

        lines.append("   " + "\u2500" * 40)

        if code:
            for code_line in str(code).split("\n"):
                lines.append(f"   {code_line}")

        lines.append("")

    return "\n".join(lines)


def format_string_results(query: str, results) -> str:
    """Format search_strings results for display."""
    lines = [f'String search for: "{query}"', ""]

    if isinstance(results, str):
        return f"{lines[0]}\n\n{results}"

    if isinstance(results, dict):
        items = results.get("results", results.get("strings", results.get("matches", [])))
        if not items and "error" in results:
            return f"{lines[0]}\n\nError: {results['error']}"
        if not items:
            return f"{lines[0]}\n\n{results}"
    elif isinstance(results, list):
        items = results
    else:
        return f"{lines[0]}\n\n{results}"

    if not items:
        lines.append("No results found.")
        return "\n".join(lines)

    for i, item in enumerate(items, 1):
        if isinstance(item, str):
            lines.append(f"{i}. {item}")
        elif isinstance(item, dict):
            value = item.get("value", item.get("string", item.get("text", str(item))))
            address = item.get("address", "")
            xrefs = item.get("xrefs", item.get("references", []))
            line = f"{i}. \"{value}\""
            if address:
                line += f"  @ {address}"
            lines.append(line)
            if xrefs:
                for xref in xrefs[:5]:
                    if isinstance(xref, dict):
                        lines.append(f"      ref: {xref.get('function', xref.get('address', xref))}")
                    else:
                        lines.append(f"      ref: {xref}")
        lines.append("")

    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(
        description="Semantic search over Ghidra decompiled code via ChromaDB"
    )
    parser.add_argument("query", help="Search query (natural language or code)")
    parser.add_argument("--code", action="store_true",
                        help="Treat query as a code snippet (uses search_code)")
    parser.add_argument("--strings", action="store_true",
                        help="Search string literals instead of code")
    parser.add_argument("--limit", type=int, default=10,
                        help="Max results to return (default: 10)")
    parser.add_argument("--exclude", action="append", default=[],
                        help="Regex pattern to exclude from results (can use multiple times). "
                             "Default excludes: __unwind$, __ehhandler$, _GLOBAL_, $vectored")
    parser.add_argument("--no-default-exclude", action="store_true",
                        help="Disable default noise filtering")
    args = parser.parse_args()

    # Combine default and user-specified exclude patterns
    exclude_patterns = [] if args.no_default_exclude else DEFAULT_EXCLUDE_PATTERNS.copy()
    exclude_patterns.extend(args.exclude)

    try:
        client = create_client()
    except MCPError as e:
        print(f"Error connecting to pyghidra-mcp: {e}", file=sys.stderr)
        print("\nIs the service running? Check with: ./tools/ghidra/pyghidra-service.sh status",
              file=sys.stderr)
        sys.exit(1)

    try:
        if args.strings:
            results = client.search_strings(args.query, limit=args.limit)
            print(format_string_results(args.query, results))
        else:
            results = client.search_code(args.query, limit=args.limit)
            print(format_code_results(args.query, results, exclude_patterns))
    except MCPError as e:
        msg = str(e)
        if "index" in msg.lower() or "chroma" in msg.lower() or "try again" in msg.lower():
            print(f"ChromaDB index not ready yet. The server may still be indexing.", file=sys.stderr)
            print(f"Detail: {msg}", file=sys.stderr)
            sys.exit(2)
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
