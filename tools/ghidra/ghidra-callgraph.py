#!/usr/bin/env python3
"""
Generate call graph for a function in the Ghidra project.

Usage:
    ghidra-callgraph.py "Game::Poll" --direction calling
    ghidra-callgraph.py "Game::Poll" --direction called
    ghidra-callgraph.py "Game::Poll" --output graph.md
"""

import argparse
import json
import sys
from pathlib import Path

# Add parent directory to path for imports
sys.path.insert(0, str(Path(__file__).parent))

from mcp_client import MCPClient, MCPError, DEFAULT_BINARY


def format_callgraph(result: dict, function: str, direction: str) -> str:
    """Format call graph results for display."""
    lines = []

    lines.append(f"Call Graph for: {function}")
    lines.append(f"Direction: {direction}")
    lines.append("")

    # Get the nodes/edges from result
    nodes = result.get("nodes", result.get("functions", []))
    edges = result.get("edges", result.get("calls", []))

    if direction == "calling":
        lines.append(f"Functions that call {function}:")
        lines.append("")
        if nodes:
            for node in nodes:
                if isinstance(node, dict):
                    name = node.get("name", node.get("function", str(node)))
                    addr = node.get("address", "")
                    if addr:
                        lines.append(f"  - {name} @ {addr}")
                    else:
                        lines.append(f"  - {name}")
                else:
                    lines.append(f"  - {node}")
        else:
            lines.append("  (no callers found)")
    else:  # called
        lines.append(f"Functions called by {function}:")
        lines.append("")
        if nodes:
            for node in nodes:
                if isinstance(node, dict):
                    name = node.get("name", node.get("function", str(node)))
                    addr = node.get("address", "")
                    if addr:
                        lines.append(f"  - {name} @ {addr}")
                    else:
                        lines.append(f"  - {name}")
                else:
                    lines.append(f"  - {node}")
        else:
            lines.append("  (no callees found)")

    # Show edges if available
    if edges:
        lines.append("")
        lines.append("Call relationships:")
        for edge in edges:
            if isinstance(edge, dict):
                caller = edge.get("from", edge.get("caller", "?"))
                callee = edge.get("to", edge.get("callee", "?"))
                lines.append(f"  {caller} -> {callee}")
            else:
                lines.append(f"  {edge}")

    lines.append("")
    lines.append(f"Total nodes: {len(nodes)}, edges: {len(edges)}")

    return "\n".join(lines)


def format_callgraph_markdown(result: dict, function: str, direction: str) -> str:
    """Format call graph as Markdown for export."""
    lines = []

    lines.append(f"# Call Graph: {function}")
    lines.append("")
    lines.append(f"**Direction:** {direction}")
    lines.append("")

    nodes = result.get("nodes", result.get("functions", []))
    edges = result.get("edges", result.get("calls", []))

    if direction == "calling":
        lines.append(f"## Functions that call `{function}`")
    else:
        lines.append(f"## Functions called by `{function}`")
    lines.append("")

    if nodes:
        for node in nodes:
            if isinstance(node, dict):
                name = node.get("name", node.get("function", str(node)))
                addr = node.get("address", "")
                if addr:
                    lines.append(f"- `{name}` @ `{addr}`")
                else:
                    lines.append(f"- `{name}`")
            else:
                lines.append(f"- `{node}`")
    else:
        lines.append("*No functions found.*")

    if edges:
        lines.append("")
        lines.append("## Call Relationships")
        lines.append("")
        lines.append("```")
        for edge in edges:
            if isinstance(edge, dict):
                caller = edge.get("from", edge.get("caller", "?"))
                callee = edge.get("to", edge.get("callee", "?"))
                lines.append(f"{caller} -> {callee}")
            else:
                lines.append(str(edge))
        lines.append("```")

    lines.append("")
    lines.append(f"---")
    lines.append(f"*Total: {len(nodes)} nodes, {len(edges)} edges*")

    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(
        description="Generate call graph for a function",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s "Game::Poll" --direction calling   # Who calls Game::Poll?
  %(prog)s "Game::Poll" --direction called    # What does Game::Poll call?
  %(prog)s "Game::Poll" -o graph.md           # Save as Markdown
  %(prog)s "Game::Poll" --json                # Output raw JSON
        """
    )
    parser.add_argument(
        "function",
        help="Function name to analyze"
    )
    parser.add_argument(
        "--direction", "-d",
        choices=["calling", "called"],
        default="calling",
        help="'calling' = functions that call this one, 'called' = functions this one calls (default: calling)"
    )
    parser.add_argument(
        "--binary", "-b",
        default=DEFAULT_BINARY,
        help=f"Binary name (default: {DEFAULT_BINARY})"
    )
    parser.add_argument(
        "--output", "-o",
        help="Write output to file (uses Markdown format for .md files)"
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

        result = client.get_callgraph(args.function, direction=args.direction)

        # Determine output format
        if args.json:
            output = json.dumps(result, indent=2)
        elif args.output and args.output.endswith(".md"):
            output = format_callgraph_markdown(result, args.function, args.direction)
        else:
            output = format_callgraph(result, args.function, args.direction)

        # Write to file or stdout
        if args.output:
            Path(args.output).write_text(output)
            print(f"Call graph written to: {args.output}")
        else:
            print(output)

    except MCPError as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)
    except KeyboardInterrupt:
        sys.exit(130)


if __name__ == "__main__":
    main()
