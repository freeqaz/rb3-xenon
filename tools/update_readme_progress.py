#!/usr/bin/env python3
"""Regenerate the README progress table from build/45410914/report.json.

Rewrites the block between the `progress-table:begin` / `progress-table:end`
markers in README.md. Manually dispatched:

    python3 tools/update_readme_progress.py            # rewrite README.md
    python3 tools/update_readme_progress.py --check    # exit 1 if table is stale

The "as of" date is taken from the report file's mtime (i.e. the last build
that refreshed it), not from the wall clock.
"""

import argparse
import datetime
import json
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
README = REPO_ROOT / "README.md"
REPORT = REPO_ROOT / "build" / "45410914" / "report.json"

BEGIN = "<!-- progress-table:begin"
END = "<!-- progress-table:end -->"
BAR_WIDTH = 20

# Category id -> README row label. Order here is display order.
CATEGORY_ROWS = [
    ("game", "Game code (`src/band3/`)"),
    ("engine", "Milo engine (`src/system/`)"),
    ("network", "Quazal network (`src/network/`)"),
]


def bar(pct: float) -> str:
    filled = round(pct / 100 * BAR_WIDTH)
    filled = max(0, min(BAR_WIDTH, filled))
    return "`" + "█" * filled + "░" * (BAR_WIDTH - filled) + "`"


def pct(num: int, den: int) -> float:
    return 100.0 * num / den if den else 0.0


def fn_cell(matched: int, total: int) -> str:
    return f"{matched:,} / {total:,} ({pct(matched, total):.1f}%)"


def build_table(report: dict, as_of: datetime.date) -> str:
    m = report["measures"]
    total_fns = m["total_functions"]
    matched_fns = m["matched_functions"]
    total_code = int(m["total_code"])
    matched_code = int(m["matched_code"])

    lines = [
        f"As of **{as_of.isoformat()}**:",
        "",
        "| Area | Functions matched | Code bytes | Progress |",
        "|---|---:|---:|:---|",
        f"| **Whole binary** | **{fn_cell(matched_fns, total_fns)}** "
        f"| **{pct(matched_code, total_code):.1f}%** | {bar(pct(matched_fns, total_fns))} |",
    ]

    by_id = {c["id"]: c["measures"] for c in report.get("categories", [])}
    for cat_id, label in CATEGORY_ROWS:
        cm = by_id.get(cat_id)
        if not cm or not cm.get("total_functions"):
            continue
        cfns, cmfns = cm["total_functions"], cm.get("matched_functions", 0)
        ccode, cmcode = int(cm.get("total_code", 0)), int(cm.get("matched_code", 0))
        lines.append(
            f"| {label} | {fn_cell(cmfns, cfns)} "
            f"| {pct(cmcode, ccode):.1f}% | {bar(pct(cmfns, cfns))} |"
        )

    # Units carrying no progress_categories = the unattributed remainder
    # (XDK/CRT/middleware, EH funclets). Count each unit once.
    un_fns = un_matched = 0
    for u in report["units"]:
        if not (u.get("metadata") or {}).get("progress_categories"):
            um = u["measures"]
            un_fns += um.get("total_functions", 0)
            un_matched += um.get("matched_functions", 0)
    matched_str = f"{un_matched:,} / {un_fns:,}"
    if un_matched:
        matched_str += f" ({pct(un_matched, un_fns):.1f}%)"
    lines.append(
        f"| Not yet attributed¹ | {matched_str} | — | {bar(pct(un_matched, un_fns))} |"
    )
    return "\n".join(lines)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--check", action="store_true",
                    help="don't write; exit 1 if the README table is stale")
    args = ap.parse_args()

    if not REPORT.exists():
        sys.exit(f"error: {REPORT} not found — run the build first (./tools/ninja-locked)")

    report = json.loads(REPORT.read_text())
    as_of = datetime.date.fromtimestamp(REPORT.stat().st_mtime)
    table = build_table(report, as_of)

    text = README.read_text()
    pattern = re.compile(
        re.escape(BEGIN) + r".*?-->\n(.*?)\n" + re.escape(END), re.DOTALL
    )
    m = pattern.search(text)
    if not m:
        sys.exit(f"error: progress-table markers not found in {README}")

    if m.group(1) == table:
        print("README progress table already up to date")
        return 0
    if args.check:
        print("README progress table is STALE — run tools/update_readme_progress.py")
        return 1

    new_text = text[: m.start(1)] + table + text[m.end(1):]
    README.write_text(new_text)
    mm = report["measures"]
    print(
        f"README updated: {mm['matched_functions']:,}/{mm['total_functions']:,} "
        f"functions ({mm['matched_functions_percent']:.1f}%) as of {as_of.isoformat()}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
