#!/usr/bin/env python3
"""Unified function reconnaissance — single command for full function intel.

Combines:
  - Orchestrator DB: match%, verdict, attempt count, unicorn data
  - Unicorn runner: behavioral comparison + divergence classification
  - Field access probing: struct offset read/write map
  - Struct DB: field name resolution

Usage:
  python3 scripts/recon.py --symbol '?Method@Class@@...'
  python3 scripts/recon.py --symbol '?Method@Class@@...' --unit system/char/Char
  python3 scripts/recon.py --symbol '?Method@Class@@...' --json
"""

import argparse
import json
import os
import sys

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, PROJECT_ROOT)


def _load_db_info(symbol, db_path):
    """Load function info from orchestrator DB."""
    try:
        from scripts.orchestrator.database import get_connection
        conn = get_connection(db_path)
        row = conn.execute(
            """SELECT id, symbol, demangled, unit, size, current_percent, best_percent,
                      verdict, verdict_reason, attempt_count, last_model,
                      unicorn_verdict, unicorn_class, unicorn_confidence,
                      reachable_100, primary_pattern, has_linker_merged,
                      has_bool_mask, has_addtostrings, has_makestring,
                      priority_score, ease_score, impact_score, confidence_score,
                      excluded, exclusion_reason
               FROM functions WHERE symbol = ?""",
            (symbol,),
        ).fetchone()
        return dict(row) if row else None
    except Exception:
        return None


def _load_struct_db():
    """Load StructDB if available."""
    try:
        from tools.struct_db import StructDB
        db_path = os.path.join(PROJECT_ROOT, "struct_db.sqlite")
        if os.path.exists(db_path):
            db = StructDB(db_path)
            db.connect()
            return db
    except ImportError:
        pass
    return None


def _run_field_access(symbol, unit_name, timeout, coload):
    """Run field access probing. Returns list of FieldAccess or None."""
    try:
        from scripts.unicorn_runner.run import resolve_unit
        from scripts.unicorn_runner.coff import COFFParser
        from scripts.unicorn_runner.prober import probe_field_access
        from scripts.unicorn_runner.typed_fixture import extract_class_from_symbol

        if unit_name:
            decomp_path, orig_path = resolve_unit(unit_name)
        else:
            # Try to resolve from DB unit
            return None

        if not os.path.exists(decomp_path) or not os.path.exists(orig_path):
            return None

        decomp_coff = COFFParser(decomp_path)
        orig_coff = COFFParser(orig_path)

        cls_name = extract_class_from_symbol(symbol)
        sdb = _load_struct_db()

        accesses = probe_field_access(
            symbol, decomp_coff, orig_coff,
            timeout=timeout, coload=coload,
            struct_db=sdb, class_name=cls_name)

        if sdb is not None:
            sdb.close()

        return accesses
    except Exception:
        return None


def _run_unicorn_comparison(symbol, unit_name, timeout, coload):
    """Run unicorn behavioral comparison. Returns (verdict, class, confidence) or None."""
    try:
        from scripts.unicorn_runner.run import resolve_unit, _run_comparison_core
        from scripts.unicorn_runner.run import EXIT_EQUIVALENT, EXIT_DIVERGENT, EXIT_SKIPPED
        from scripts.unicorn_runner.coff import COFFParser
        from scripts.unicorn_runner.comparator import classify_divergence

        if not unit_name:
            return None

        decomp_path, orig_path = resolve_unit(unit_name)
        if not os.path.exists(decomp_path) or not os.path.exists(orig_path):
            return None

        decomp_coff = COFFParser(decomp_path)
        orig_coff = COFFParser(orig_path)

        exit_code, bundle, _, _ = _run_comparison_core(
            symbol, decomp_coff, orig_coff, timeout=timeout, coload=coload)

        verdict_map = {
            EXIT_EQUIVALENT: "EQUIVALENT",
            EXIT_DIVERGENT: "DIVERGENT",
            EXIT_SKIPPED: "SKIPPED",
        }
        verdict = verdict_map.get(exit_code, "ERROR")

        div_class = None
        if exit_code == EXIT_DIVERGENT and bundle is not None:
            div_class = classify_divergence(
                bundle.result, bundle.decomp_result, bundle.orig_result,
                bundle.decomp_relocs, bundle.orig_relocs)

        return {"verdict": verdict, "class": div_class, "confidence": "high"}
    except Exception as e:
        return {"verdict": "ERROR", "class": None, "confidence": None, "error": str(e)}


def _resolve_unit_from_db(db_info):
    """Extract unit name from DB info, converting default/ prefix to bare path."""
    if not db_info or not db_info.get("unit"):
        return None
    unit = db_info["unit"]
    # DB stores "default/system/char/Char" but resolve_unit wants "system/char/Char"
    if unit.startswith("default/"):
        return unit[len("default/"):]
    return unit


def recon(symbol, unit_name=None, db_path="decomp.db", timeout=5_000_000,
          coload=True, run_unicorn=True, run_field_access=True):
    """Run full reconnaissance on a function.

    Returns a dict with all collected intel.
    """
    result = {"symbol": symbol}

    # 1. DB info
    db_info = _load_db_info(symbol, db_path)
    if db_info:
        result["db"] = {
            "demangled": db_info.get("demangled"),
            "unit": db_info.get("unit"),
            "size": db_info.get("size"),
            "current_percent": db_info.get("current_percent"),
            "best_percent": db_info.get("best_percent"),
            "verdict": db_info.get("verdict"),
            "verdict_reason": db_info.get("verdict_reason"),
            "attempt_count": db_info.get("attempt_count"),
            "last_model": db_info.get("last_model"),
            "priority_score": db_info.get("priority_score"),
            "reachable_100": bool(db_info.get("reachable_100")),
            "primary_pattern": db_info.get("primary_pattern"),
        }
        result["unicorn_cached"] = {
            "verdict": db_info.get("unicorn_verdict"),
            "class": db_info.get("unicorn_class"),
            "confidence": db_info.get("unicorn_confidence"),
        }
        result["flags"] = {
            "has_linker_merged": bool(db_info.get("has_linker_merged")),
            "has_bool_mask": bool(db_info.get("has_bool_mask")),
            "has_addtostrings": bool(db_info.get("has_addtostrings")),
            "has_makestring": bool(db_info.get("has_makestring")),
            "excluded": bool(db_info.get("excluded")),
            "exclusion_reason": db_info.get("exclusion_reason"),
        }

    # Resolve unit for unicorn/field-access
    effective_unit = unit_name or _resolve_unit_from_db(db_info)

    # 2. Live unicorn comparison
    if run_unicorn and effective_unit:
        unicorn_live = _run_unicorn_comparison(symbol, effective_unit, timeout, coload)
        if unicorn_live:
            result["unicorn_live"] = unicorn_live

    # 3. Field access probing
    if run_field_access and effective_unit:
        accesses = _run_field_access(symbol, effective_unit, timeout, coload)
        if accesses is not None:
            result["field_access"] = [
                {
                    "offset": a.offset,
                    "hex_offset": f"0x{a.offset:03X}",
                    "access_type": a.access_type,
                    "source": a.source,
                    "field_name": a.field_name,
                }
                for a in accesses
            ]

    return result


def format_recon(data):
    """Format recon result as human-readable text."""
    lines = []
    symbol = data["symbol"]

    # Header
    db = data.get("db", {})
    demangled = db.get("demangled") or symbol
    lines.append(f"=== RECON: {demangled} ===")
    lines.append(f"Symbol: {symbol}")
    if db.get("unit"):
        lines.append(f"Unit: {db['unit']}")
    if db.get("size"):
        lines.append(f"Size: {db['size']} bytes")

    # Match status
    pct = db.get("current_percent")
    verdict = db.get("verdict")
    if pct is not None:
        status_line = f"Match: {pct:.2f}%"
        if verdict:
            status_line += f"  [{verdict}]"
        reason = db.get("verdict_reason")
        if reason:
            status_line += f"  ({reason})"
        lines.append(status_line)

    # Priority
    priority = db.get("priority_score")
    if priority:
        lines.append(f"Priority: {priority:.1f}  (ease={db.get('ease_score', '?')}, "
                     f"impact={db.get('impact_score', '?')}, "
                     f"confidence={db.get('confidence_score', '?')})")

    # Pattern flags
    pattern = db.get("primary_pattern")
    reachable = db.get("reachable_100")
    if pattern:
        reach_str = "reachable" if reachable else "BLOCKED"
        lines.append(f"Pattern: {pattern}  [{reach_str}]")

    flags = data.get("flags", {})
    active_flags = [k for k, v in flags.items() if v and k not in ("excluded",)]
    if active_flags:
        lines.append(f"Flags: {', '.join(active_flags)}")

    # Attempts
    attempts = db.get("attempt_count")
    if attempts:
        lines.append(f"Attempts: {attempts}  (last: {db.get('last_model', '?')})")

    # Unicorn verdicts
    lines.append("")
    cached = data.get("unicorn_cached", {})
    live = data.get("unicorn_live", {})

    if cached.get("verdict") or live.get("verdict"):
        lines.append("--- Unicorn Behavioral Analysis ---")

    if cached.get("verdict"):
        cls = cached.get("class")
        cls_str = f"  class={cls}" if cls else ""
        lines.append(f"  Cached: {cached['verdict']}{cls_str}")

    if live.get("verdict"):
        cls = live.get("class")
        cls_str = f"  class={cls}" if cls else ""
        err = live.get("error")
        err_str = f"  ({err})" if err else ""
        lines.append(f"  Live:   {live['verdict']}{cls_str}{err_str}")

    # Field access
    fa = data.get("field_access")
    if fa:
        lines.append("")
        lines.append("--- Field Access Map ---")
        for a in fa:
            field = f"({a['field_name']})" if a.get("field_name") else ""
            lines.append(f"  {a['access_type']:10s} {a['hex_offset']} {field:40s} via {a['source']}")
        reads = sum(1 for a in fa if "READ" in a["access_type"])
        writes = sum(1 for a in fa if "WRITE" in a["access_type"])
        lines.append(f"  Total: {len(fa)} fields ({reads} reads, {writes} writes)")

    # Assessment
    lines.append("")
    lines.append("--- Assessment ---")
    assessment = _assess(data)
    for line in assessment:
        lines.append(f"  {line}")

    return "\n".join(lines)


def _assess(data):
    """Generate brief assessment lines from recon data."""
    lines = []
    db = data.get("db", {})
    flags = data.get("flags", {})
    cached = data.get("unicorn_cached", {})
    live = data.get("unicorn_live", {})
    unicorn = live if live.get("verdict") else cached

    pct = db.get("current_percent")
    verdict = db.get("verdict")

    if verdict == "COMPLETE":
        lines.append("DONE - function matches 100%")
        return lines

    if verdict == "AT_LIMIT":
        reason = db.get("verdict_reason", "unknown")
        lines.append(f"AT_LIMIT ({reason}) - not a viable work target")
        return lines

    if flags.get("excluded"):
        lines.append(f"EXCLUDED ({flags.get('exclusion_reason', '?')}) - skip")
        return lines

    # Workable function - assess viability
    if unicorn.get("verdict") == "EQUIVALENT":
        if pct and pct < 100:
            lines.append(f"Behavior matches but asm differs at {pct:.1f}% — likely register "
                         "allocation / scheduling. Consider AT_LIMIT.")
        else:
            lines.append("Unicorn says EQUIVALENT — behavior is correct")

    if unicorn.get("class") == "build_env":
        lines.append("Divergence is build environment (__FILE__/merged) — unfixable from source")

    if unicorn.get("class") == "regalloc":
        lines.append("Divergence is register allocation — may be fixable with variable reordering")

    if unicorn.get("class") == "logic":
        lines.append("Divergence is real logic difference — needs source fix")

    if flags.get("has_linker_merged") and not db.get("reachable_100"):
        lines.append("Has linker-merged symbols + unreachable — AT_LIMIT candidate")

    if flags.get("has_addtostrings"):
        lines.append("Uses AddToStrings merged symbol — may need template workaround")

    fa = data.get("field_access", [])
    if fa:
        reads = [a for a in fa if "READ" in a["access_type"]]
        writes = [a for a in fa if "WRITE" in a["access_type"]]
        named_reads = [a for a in reads if a.get("field_name")]
        lines.append(f"Touches {len(fa)} struct fields ({len(reads)} reads, {len(writes)} writes, "
                     f"{len(named_reads)} resolved)")

    if not lines:
        if pct and pct >= 90:
            lines.append(f"At {pct:.1f}% — close to matching, likely small diff remaining")
        elif pct and pct >= 50:
            lines.append(f"At {pct:.1f}% — moderate match, significant work needed")
        else:
            lines.append("Low match or no data — needs investigation")

    return lines


def main():
    parser = argparse.ArgumentParser(
        description="Unified function reconnaissance"
    )
    parser.add_argument("--symbol", required=True, help="Mangled function symbol")
    parser.add_argument("--unit", type=str, default=None,
                       help="Unit name (auto-detected from DB if omitted)")
    parser.add_argument("--db", type=str, default="decomp.db",
                       help="Path to orchestrator database")
    parser.add_argument("--json", action="store_true",
                       help="Output JSON instead of human-readable text")
    parser.add_argument("--no-unicorn", action="store_true",
                       help="Skip live unicorn comparison")
    parser.add_argument("--no-field-access", action="store_true",
                       help="Skip field access probing")
    parser.add_argument("--timeout", type=int, default=5_000_000,
                       help="Unicorn timeout in microseconds")

    args = parser.parse_args()

    data = recon(
        args.symbol,
        unit_name=args.unit,
        db_path=args.db,
        timeout=args.timeout,
        run_unicorn=not args.no_unicorn,
        run_field_access=not args.no_field_access,
    )

    if args.json:
        print(json.dumps(data, indent=2))
    else:
        print(format_recon(data))


if __name__ == "__main__":
    main()
