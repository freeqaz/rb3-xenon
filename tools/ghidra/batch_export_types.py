#!/usr/bin/env python3
"""
Batch export Ghidra-inferred structure types into struct_db.sqlite.

Note: rb3-xenon has no leaked .map file (unlike DC3). The seed pipeline here
skips the map-file-based steps (bulk_create_functions from map, apply_demangled_signatures).
Use tools/fingerprint_match.py for function identification instead.

Seed pipeline (creates structures from headers):
1. create_structures → seed struct_db types into Ghidra DTM

Extract pipeline:
2. Batch-decompile functions to trigger type inference, collect enriched structures
3. Merge Ghidra-inferred structures into struct_db (only new classes)

Requires the pyghidra-mcp service to be running:
    ./tools/ghidra/pyghidra-service.sh start

Usage:
    # Seed Ghidra DTM with struct_db types
    python3 tools/ghidra/batch_export_types.py --seed

    # Extract only (assumes already seeded)
    python3 tools/ghidra/batch_export_types.py --extract --max-functions 500

    # Dry run
    python3 tools/ghidra/batch_export_types.py --seed --extract --dry-run

    # Show stats
    python3 tools/ghidra/batch_export_types.py --stats
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

from tools.ghidra.mcp_client import MCPClient, MCPError
from tools.struct_db import ClassInfo, Member, StructDB

DEFAULT_DB = PROJECT_ROOT / "struct_db.sqlite"

logger = logging.getLogger(__name__)


def is_low_quality(structure: dict) -> bool:
    """Check if a structure has mostly auto-generated field names.

    Returns True if more than 80% of members have field_0x*, undefined*, or
    no names — indicating the decompiler couldn't infer meaningful names.
    """
    members = structure.get("members", [])
    if not members:
        return True

    auto_count = 0
    for m in members:
        name = m.get("name") or ""
        if (not name
                or name.startswith("field_0x")
                or name.startswith("field0x")
                or name.startswith("undefined")
                or name.startswith("padding")):
            auto_count += 1

    return auto_count / len(members) > 0.8


def structure_to_classinfo(structure: dict) -> ClassInfo:
    """Convert a Ghidra structure dict to a ClassInfo for struct_db insertion."""
    members = []
    for m in structure.get("members", []):
        name = m.get("name")
        if not name:
            name = f"unk_0x{m['offset']:x}"
        members.append(Member(
            name=name,
            type_str=m.get("type_name", "undefined"),
            offset=m["offset"],
            line_number=0,
        ))

    return ClassInfo(
        name=structure["name"],
        file_path="ghidra",
        members=members,
    )


def show_stats(db_path: Path) -> None:
    """Show struct_db statistics."""
    with StructDB(str(db_path)) as db:
        all_classes = db.list_classes()

    header_sourced = [c for c in all_classes if c["file_path"] != "ghidra"]
    ghidra_sourced = [c for c in all_classes if c["file_path"] == "ghidra"]

    print("struct_db Statistics")
    print("=" * 40)
    print(f"Total classes:     {len(all_classes)}")
    print(f"  Header-sourced:  {len(header_sourced)}")
    print(f"  Ghidra-sourced:  {len(ghidra_sourced)}")


def seed_ghidra_dtm(
    client: MCPClient,
    db_path: Path,
    dry_run: bool = False,
) -> None:
    """Seed Ghidra's DTM with rb3-xenon struct_db type info.

    Pipeline:
        Step 1: create_structures from struct_db headers
    """
    print("\n" + "=" * 70)
    print("Seed Ghidra DTM — struct_db Pipeline")
    print("=" * 70)

    # Load all classes from struct_db
    with StructDB(str(db_path)) as db:
        all_classes = db.list_classes()

    # Filter to header-sourced classes only (skip existing Ghidra-sourced)
    header_classes = [c for c in all_classes if c["file_path"] != "ghidra"]
    logger.info(f"Found {len(header_classes)} header-sourced classes in struct_db")

    # Build class_defs for Ghidra
    class_defs = []
    with StructDB(str(db_path)) as db:
        for cls in header_classes:
            info = db.get_class_info(cls["name"])
            if not info:
                continue

            # Build member list with sizes inferred from offset gaps
            raw_members = sorted(info["members"], key=lambda m: m["offset"])
            members = []
            for i, m in enumerate(raw_members):
                # Infer size from gap to next member, default 4
                if i + 1 < len(raw_members):
                    gap = raw_members[i + 1]["offset"] - m["offset"]
                    size = min(gap, 4) if gap > 0 else 4
                else:
                    size = 4
                members.append({
                    "name": m["name"],
                    "type_str": m["type_str"],
                    "offset": m["offset"],
                    "size": size,
                })

            # Calculate total_size from max member offset + size
            total_size = 0
            for m in members:
                end = m["offset"] + m["size"]
                if end > total_size:
                    total_size = end

            class_defs.append({
                "name": cls["name"],
                "members": members,
                "total_size": total_size,
            })

    print(f"\nCreating {len(class_defs)} structures in Ghidra DTM...")
    if not dry_run:
        result = client.create_structures(class_defs)
        print(f"    Created: {result['created']}, Errors: {result['errors']}")
    else:
        print("    [DRY RUN] Would create structures")

    print("\nSeeding complete!")


def extract_ghidra_structures(
    client: MCPClient,
    max_functions: int = 0,
    timeout_per_func: int = 30,
) -> tuple[list[dict], dict]:
    """Extract structures from Ghidra decompiler by batch-decompiling.

    Step 2: Batch-decompile functions, collect inferred structures
    """
    print("\n" + "=" * 70)
    print("Step 2: Extract Structures from Ghidra Decompiler")
    print("=" * 70)

    scope = f"first {max_functions}" if max_functions > 0 else "all"
    print(f"\nDecompiling {scope} functions to trigger type inference...")
    print("(This may take a while — check pyghidra-mcp logs for progress)")

    start_time = time.time()
    result = client.extract_structures(
        max_functions=max_functions,
        timeout_per_func=timeout_per_func,
    )
    elapsed = time.time() - start_time

    structures = result.get("structures", [])
    stats = result.get("stats", {})

    print(f"\nExtraction complete in {elapsed:.1f}s")
    print(f"  Functions decompiled: {stats.get('decompiled', '?')}/{stats.get('total_functions', '?')}")
    print(f"  Decompile errors:     {stats.get('errors', '?')}")
    print(f"  Structures found:     {len(structures)}")

    return structures, stats


def import_structures_to_db(
    db_path: Path,
    structures: list[dict],
    dry_run: bool = False,
) -> None:
    """Import Ghidra-inferred structures into struct_db.

    Step 3: Merge new structures into struct_db (skip existing)
    """
    print("\n" + "=" * 70)
    print("Step 3: Import Ghidra Structures into struct_db")
    print("=" * 70)

    if not structures:
        print("\nNo structures to import.")
        return

    with StructDB(str(db_path)) as db:
        db.create_schema()

        # Get existing classes to avoid overwriting header-sourced data
        existing_classes = {c["name"] for c in db.list_classes()}

        imported = 0
        skipped_exists = 0
        skipped_low_quality = 0

        for struct in structures:
            name = struct["name"]

            # Skip if class already exists in struct_db (preserve header data)
            if name in existing_classes:
                skipped_exists += 1
                continue

            # Skip low-quality structures
            if is_low_quality(struct):
                skipped_low_quality += 1
                continue

            if dry_run:
                members = struct.get("members", [])
                print(f"  Would import: {name} ({len(members)} members, "
                      f"size=0x{struct.get('size', 0):x})")
            else:
                info = structure_to_classinfo(struct)
                db.insert_class(info)

            imported += 1

        if not dry_run:
            db.conn.commit()

    # Report
    print()
    action = "Would import" if dry_run else "Imported"
    print(f"{action}: {imported} structures")
    print(f"Skipped (already in db): {skipped_exists}")
    print(f"Skipped (low quality):   {skipped_low_quality}")

    if not dry_run:
        print()
        show_stats(db_path)


def main():
    parser = argparse.ArgumentParser(
        description="Seed Ghidra with rb3-xenon types → extract enriched structures → import to struct_db"
    )
    parser.add_argument(
        "--db",
        type=Path,
        default=DEFAULT_DB,
        help=f"struct_db database path (default: {DEFAULT_DB})",
    )
    parser.add_argument(
        "--mcp-url",
        default="http://127.0.0.1:8002/mcp",
        help="pyghidra-mcp service URL",
    )

    # Pipeline flags
    parser.add_argument(
        "--seed",
        action="store_true",
        help="Step 1: Seed Ghidra DTM with structures from struct_db",
    )
    parser.add_argument(
        "--extract",
        action="store_true",
        help="Step 2-3: Extract structures by decompiling + import to struct_db",
    )

    # Extract options
    parser.add_argument(
        "--max-functions",
        type=int,
        default=0,
        help="Max functions to decompile (0 = all, use small number for testing)",
    )
    parser.add_argument(
        "--timeout-per-func",
        type=int,
        default=30,
        help="Decompiler timeout per function in seconds (default: 30)",
    )

    # Misc
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Preview without writing",
    )
    parser.add_argument(
        "--stats",
        action="store_true",
        help="Show struct_db statistics and exit",
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

    if not args.seed and not args.extract:
        print("Error: Must specify at least one of --seed or --extract")
        print("Use --help for usage")
        sys.exit(1)

    # Connect to pyghidra-mcp
    print(f"Connecting to pyghidra-mcp at {args.mcp_url}...")
    client = MCPClient(url=args.mcp_url)
    try:
        client.initialize()
        print(f"Connected (session: {client.session_id})")
        print(f"Binary: {client.binary}")
    except MCPError as e:
        print(f"ERROR: Could not connect to pyghidra-mcp: {e}")
        print("Start the service with: ./tools/ghidra/pyghidra-service.sh start")
        sys.exit(1)

    # Run pipeline steps
    if args.seed:
        seed_ghidra_dtm(
            client=client,
            db_path=args.db,
            dry_run=args.dry_run,
        )

    if args.extract:
        structures, stats = extract_ghidra_structures(
            client=client,
            max_functions=args.max_functions,
            timeout_per_func=args.timeout_per_func,
        )

        import_structures_to_db(
            db_path=args.db,
            structures=structures,
            dry_run=args.dry_run,
        )

    print("\nPipeline complete!")


if __name__ == "__main__":
    main()
