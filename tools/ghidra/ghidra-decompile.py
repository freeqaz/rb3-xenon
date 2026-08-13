#!/usr/bin/env python3
"""
Decompile a function from the Ghidra project.

Usage:
    ghidra-decompile.py "Game::Poll"
    ghidra-decompile.py 0x82000000
    ghidra-decompile.py "Symbol::Symbol" --json
"""

import argparse
import json
import sys
from pathlib import Path

# Add parent directory to path for imports
sys.path.insert(0, str(Path(__file__).parent))

from mcp_client import MCPClient, MCPError, DEFAULT_BINARY


class EmptyDecompilation(Exception):
    """The service answered, but there is no function body to show.

    Distinct from MCPError: the service is HEALTHY and simply has nothing at
    this target (an address that is not the start of a function, an undefined
    region). Callers must be able to tell "Ghidra is down" from "Ghidra says no",
    because only the first invalidates a run.
    """


def split_name(result: dict) -> tuple:
    """Split pyghidra-mcp's ``name`` ("mmioOpenW-82aa3928") into (func, addr).

    There are no ``function_name``/``address`` keys in the response — reading
    them printed "// Function: unknown" on every SUCCESSFUL decompile, which
    made a healthy result indistinguishable from a failed lookup by eye.
    """
    raw = result.get("name")
    if not isinstance(raw, str) or not raw:
        return "unknown", "unknown"
    head, sep, tail = raw.rpartition("-")
    if sep and head and all(c in "0123456789abcdefABCDEF" for c in tail):
        return head, "0x" + tail.lower()
    return raw, "unknown"


def format_decompiled(result: dict) -> str:
    """Format decompiled function output for display.

    Raises ``EmptyDecompilation`` when the service reports an error or returns
    no body. Printing a "// No decompiled code available" placeholder on stdout
    and exiting 0 (the old behaviour) is indistinguishable from success to every
    programmatic caller — the agent tool belt cached that placeholder and fed it
    to the model as a real decompilation.
    """
    err = result.get("error")
    if err:
        raise EmptyDecompilation(f"service reported an error: {err}")

    code = result.get("decompiled_code", result.get("code", ""))
    if not code or not code.strip():
        raise EmptyDecompilation(
            "no function body at this target — the service is up but has "
            "nothing here (not a function start, or an undefined region)")

    func_name, address = split_name(result)
    signature = result.get("signature", "")

    lines = [f"// Function: {func_name}", f"// Address: {address}"]
    if signature:
        lines.append(f"// Signature: {signature}")
    lines.append("")
    lines.append(code)

    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(
        description="Decompile a function from Ghidra",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s "Game::Poll"         # Decompile by name
  %(prog)s 0x82000000           # Decompile by address
  %(prog)s "Symbol::Symbol" --json  # Output raw JSON
        """
    )
    parser.add_argument(
        "function",
        help="Function name or address (e.g., 'Game::Poll' or '0x82000000')"
    )
    parser.add_argument(
        "--binary", "-b",
        default=DEFAULT_BINARY,
        help=f"Binary name (default: {DEFAULT_BINARY})"
    )
    parser.add_argument(
        "--json", "-j",
        action="store_true",
        help="Output raw JSON instead of formatted code"
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

        try:
            result = client.decompile_function(args.function)
        except MCPError as e:
            if "not found" in str(e).lower():
                # Function object doesn't exist in Ghidra - create it first
                addr_str = args.function.strip().lower().replace("0x", "")
                if all(c in "0123456789abcdef" for c in addr_str) and len(addr_str) >= 6:
                    print(f"Creating function at 0x{addr_str}...", file=sys.stderr)
                    client.bulk_create_functions([addr_str])
                    result = client.decompile_function(args.function)
                else:
                    raise
            else:
                raise

        if args.json:
            # --json is a programmatic surface too: apply the same emptiness
            # contract, so a caller cannot mistake {"error": ...} for a hit.
            if result.get("error"):
                raise EmptyDecompilation(
                    "service reported an error: %s" % result["error"])
            if not (result.get("decompiled_code") or result.get("code") or "").strip():
                raise EmptyDecompilation("no function body at this target")
            print(json.dumps(result, indent=2))
        else:
            print(format_decompiled(result))

    except MCPError as e:
        # EXIT 1 = INFRASTRUCTURE. The service is unreachable/broken; any run
        # depending on Ghidra must abort rather than continue without it.
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)
    except EmptyDecompilation as e:
        # EXIT 2 = ANSWERED, EMPTY. The service is healthy; this target has no
        # body. Recoverable — the caller should pick a different target.
        print(f"Empty: {e}", file=sys.stderr)
        sys.exit(2)
    except KeyboardInterrupt:
        sys.exit(130)


if __name__ == "__main__":
    main()
