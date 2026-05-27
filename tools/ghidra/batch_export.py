#!/usr/bin/env python3
"""
Batch export Ghidra decompilations and cross-references to SQLite cache.

Pre-caches all function decompilations so an orchestrator can serve them
from SQLite instead of holding a Ghidra project lock. The binary never
changes (fixed Xbox 360 retail build), so decompiling once is sufficient.

Note: rb3-xenon has no leaked .map file. Functions are identified as anonymous
fn_8XXXXXXX addresses. Use tools/fingerprint_match.py to map addresses to source.

Requires the pyghidra-mcp service to be running:
    ./tools/ghidra/pyghidra-service.sh start

Usage:
    # Export first 10 functions (test)
    python3 tools/ghidra/batch_export.py --limit 10

    # Full export with resume support (default)
    python3 tools/ghidra/batch_export.py --resume

    # Fresh export (wipe cache first)
    python3 tools/ghidra/batch_export.py --fresh

    # Only decompilations or xrefs
    python3 tools/ghidra/batch_export.py --decomp-only
    python3 tools/ghidra/batch_export.py --xrefs-only

    # Show cache statistics
    python3 tools/ghidra/batch_export.py --stats
"""

import argparse
import logging
import re
import sys
import time
from collections import defaultdict
from pathlib import Path

# Add project root to path
PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(PROJECT_ROOT))

from scripts.orchestrator.database import (
    get_cache_stats,
    get_cached_symbols,
    get_connection,
    init_database,
    put_decompilation,
    put_xrefs,
)
from tools.ghidra.mcp_client import MCPClient, MCPError

# Default paths — rb3-xenon has no .map file; address list comes from Ghidra directly
DEFAULT_DB = PROJECT_ROOT / "decomp.db"

logger = logging.getLogger(__name__)


def show_stats(db_path: Path) -> None:
    """Display cache statistics."""
    conn = get_connection(str(db_path))
    stats = get_cache_stats(conn)

    print("Ghidra Cache Statistics")
    print("=" * 40)
    print(f"Decompilations: {stats['decompilations_total']} total")
    print(f"  OK:     {stats['decompilations_ok']}")
    print(f"  Errors: {stats['decompilations_errors']}")
    print(f"Xrefs:          {stats['xrefs_total']} total")
    print(f"  OK:     {stats['xrefs_ok']}")
    print(f"  Errors: {stats['xrefs_errors']}")


def batch_export(
    db_path: Path,
    mcp_url: str,
    address_list: list[str] = None,
    resume: bool = True,
    decomp_only: bool = False,
    xrefs_only: bool = False,
    limit: int = 0,
    batch_size: int = 100,
) -> None:
    """Run batch export of Ghidra decompilations and xrefs to SQLite.

    Unlike DC3, rb3-xenon has no .map file. Addresses must be supplied via
    address_list (hex strings without 0x prefix) or derived from Ghidra's
    function list.

    Uses WAL mode for non-blocking concurrent reads during export.
    """
    do_decomp = not xrefs_only
    do_xrefs = not decomp_only

    if address_list is None:
        address_list = []

    if not address_list:
        print("No addresses provided. Pass --address-file or connect to Ghidra to enumerate functions.")
        print("Tip: Use tools/fingerprint_match.py to get candidate function addresses.")
        return

    unique_addresses = list(dict.fromkeys(address_list))  # deduplicate preserving order
    if limit > 0:
        unique_addresses = unique_addresses[:limit]

    print(f"Addresses to process: {len(unique_addresses)}")

    # Initialize database (runs migrations if needed)
    conn = init_database(str(db_path))
    conn.execute("PRAGMA busy_timeout = 5000")

    # Get already-cached symbols for resume
    cached = set()
    if resume:
        cached = get_cached_symbols(conn)
        print(f"Already cached: {len(cached)} addresses")
        unique_addresses = [a for a in unique_addresses if a not in cached]

    total = len(unique_addresses)
    if total == 0:
        print("Nothing to do — all addresses already cached.")
        show_stats(db_path)
        return

    print(f"Will process {total} addresses")
    print()

    # Connect to pyghidra-mcp service
    print(f"Connecting to pyghidra-mcp at {mcp_url}...")
    client = MCPClient(url=mcp_url)
    try:
        client.initialize()
        print(f"Connected (session: {client.session_id})")
        print(f"Binary: {client.binary}")
    except MCPError as e:
        print(f"ERROR: Could not connect to pyghidra-mcp: {e}")
        print("Start the service with: ./tools/ghidra/pyghidra-service.sh start")
        sys.exit(1)
    print()

    # Process addresses
    start_time = time.time()
    success = 0
    errors = 0
    consecutive_errors = 0

    for i, address in enumerate(unique_addresses):
        lookup_key = f"0x{address}"

        # --- Decompilation ---
        decomp_code = None
        decomp_error = None

        if do_decomp:
            try:
                result = client.call_tool("decompile_function", {
                    "binary_name": client.binary,
                    "name": lookup_key,
                })
                decomp_code = result.get("code", "") if isinstance(result, dict) else str(result)
                consecutive_errors = 0
            except MCPError as e:
                decomp_error = str(e)
                consecutive_errors += 1
                if i < 5 or consecutive_errors == 1:
                    logger.warning(f"Decomp error @ {lookup_key}: {e}")

        # --- Xrefs ---
        xref_callers = None
        xref_error = None

        if do_xrefs:
            try:
                result = client.list_xrefs(lookup_key)
                xref_list = []
                if isinstance(result, dict):
                    xref_list = result.get("cross_references", [])
                elif isinstance(result, list):
                    xref_list = result

                xref_callers = []
                for xref in xref_list:
                    if isinstance(xref, dict):
                        fn = xref.get("function_name")
                        if fn:
                            xref_callers.append(fn)
                consecutive_errors = 0
            except MCPError as e:
                xref_error = str(e)
                if i < 5 or consecutive_errors == 1:
                    logger.warning(f"Xrefs error @ {lookup_key}: {e}")

        # Store result
        if do_decomp:
            if decomp_error:
                put_decompilation(conn, address, address, code="", error=decomp_error)
            else:
                put_decompilation(conn, address, address, decomp_code, None)

        if do_xrefs:
            if xref_error:
                put_xrefs(conn, address, address, callers=[], callees=[], error=xref_error)
            else:
                put_xrefs(conn, address, address, xref_callers, callees=[])

        if decomp_error and xref_error:
            errors += 1
        else:
            success += 1

        # Bail if service seems down
        if consecutive_errors >= 20:
            print(f"\nERROR: {consecutive_errors} consecutive errors — service may be down")
            print("Check: ./tools/ghidra/pyghidra-service.sh status")
            conn.commit()
            break

        # Batch commit + progress
        if (i + 1) % batch_size == 0 or i == total - 1:
            conn.commit()

            elapsed = time.time() - start_time
            rate = (i + 1) / elapsed if elapsed > 0 else 0
            remaining = (total - i - 1) / rate if rate > 0 else 0

            pct = 100.0 * (i + 1) / total
            print(
                f"[{pct:5.1f}%] {i+1}/{total} addrs — "
                f"{success} ok, {errors} errors — "
                f"{rate:.1f} addr/s — "
                f"ETA: {remaining/60:.1f} min"
            )

    elapsed = time.time() - start_time
    print()
    print(f"Batch export complete in {elapsed/60:.1f} minutes")
    print(f"  Success: {success}, Errors: {errors}")
    print()
    show_stats(db_path)


def main():
    parser = argparse.ArgumentParser(
        description="Batch export Ghidra decompilations to SQLite cache"
    )
    parser.add_argument(
        "--db",
        type=Path,
        default=DEFAULT_DB,
        help="Database path (decomp.db)",
    )
    parser.add_argument(
        "--mcp-url",
        default="http://127.0.0.1:8002/mcp",
        help="pyghidra-mcp service URL (default: http://127.0.0.1:8002/mcp)",
    )
    parser.add_argument(
        "--address-file",
        type=Path,
        help="File with one hex address per line (without 0x prefix)",
    )
    parser.add_argument(
        "--resume",
        action="store_true",
        default=True,
        help="Skip already-cached addresses (default)",
    )
    parser.add_argument(
        "--fresh",
        action="store_true",
        help="Wipe cache tables and re-export everything",
    )
    parser.add_argument(
        "--decomp-only",
        action="store_true",
        help="Only export decompilations (skip xrefs)",
    )
    parser.add_argument(
        "--xrefs-only",
        action="store_true",
        help="Only export cross-references (skip decompilations)",
    )
    parser.add_argument(
        "--limit",
        type=int,
        default=0,
        help="Max addresses to process (0 = all)",
    )
    parser.add_argument(
        "--batch-size",
        type=int,
        default=100,
        help="Commit every N addresses (default: 100)",
    )
    parser.add_argument(
        "--stats",
        action="store_true",
        help="Show cache statistics and exit",
    )
    parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="Enable verbose logging",
    )

    args = parser.parse_args()

    level = logging.DEBUG if args.verbose else logging.INFO
    logging.basicConfig(
        level=level,
        format="%(asctime)s %(levelname)s %(message)s",
        datefmt="%H:%M:%S",
    )

    if args.stats:
        show_stats(args.db)
        return

    if args.fresh:
        print("Wiping cache tables...")
        conn = get_connection(str(args.db))
        conn.execute("DELETE FROM decompilations")
        conn.execute("DELETE FROM xrefs")
        conn.commit()
        print("Cache cleared.")

    # Load addresses from file if provided
    address_list = []
    if args.address_file:
        if not args.address_file.exists():
            print(f"ERROR: Address file not found: {args.address_file}", file=sys.stderr)
            sys.exit(1)
        with open(args.address_file) as f:
            for line in f:
                line = line.strip().lower().replace("0x", "")
                if line:
                    address_list.append(line)
        print(f"Loaded {len(address_list)} addresses from {args.address_file}")

    batch_export(
        db_path=args.db,
        mcp_url=args.mcp_url,
        address_list=address_list,
        resume=not args.fresh,
        decomp_only=args.decomp_only,
        xrefs_only=args.xrefs_only,
        limit=args.limit,
        batch_size=args.batch_size,
    )


if __name__ == "__main__":
    main()
