#!/usr/bin/env python3
"""
Check Ghidra MCP server status and list available binaries.

Usage:
    ghidra-status.py
    ghidra-status.py --reinit
"""

import argparse
import json
import sys
from pathlib import Path

# Add parent directory to path for imports
sys.path.insert(0, str(Path(__file__).parent))

from mcp_client import MCPClient, MCPError, SESSION_CACHE_FILE


def main():
    parser = argparse.ArgumentParser(
        description="Check Ghidra MCP server status"
    )
    parser.add_argument(
        "--reinit",
        action="store_true",
        help="Force session re-initialization"
    )
    parser.add_argument(
        "--json", "-j",
        action="store_true",
        help="Output raw JSON"
    )

    args = parser.parse_args()

    try:
        client = MCPClient()

        # Check for cached session
        cached_session = None
        if SESSION_CACHE_FILE.exists() and not args.reinit:
            cached_session = SESSION_CACHE_FILE.read_text().strip()

        client.initialize(force=args.reinit)

        if args.json:
            result = {
                "status": "connected",
                "session_id": client.session_id,
                "was_cached": cached_session == client.session_id,
                "binaries": client.list_binaries()
            }
            print(json.dumps(result, indent=2))
        else:
            print("Ghidra MCP Status")
            print("=" * 40)
            print(f"Status: Connected")
            print(f"Session ID: {client.session_id}")
            if cached_session == client.session_id:
                print(f"Session: Using cached session")
            else:
                print(f"Session: New session created")

            print("")
            print("Available Binaries:")

            binaries = client.list_binaries()
            if isinstance(binaries, dict):
                bin_list = binaries.get("binaries", binaries.get("programs", []))
            elif isinstance(binaries, list):
                bin_list = binaries
            else:
                bin_list = [binaries]

            if bin_list:
                for b in bin_list:
                    if isinstance(b, dict):
                        name = b.get("name", b.get("path", str(b)))
                        print(f"  - {name}")
                    else:
                        print(f"  - {b}")
            else:
                print("  (none found)")

    except MCPError as e:
        if args.json:
            print(json.dumps({"status": "error", "message": str(e)}, indent=2))
        else:
            print(f"Status: Not connected")
            print(f"Error: {e}")
        sys.exit(1)
    except KeyboardInterrupt:
        sys.exit(130)


if __name__ == "__main__":
    main()
