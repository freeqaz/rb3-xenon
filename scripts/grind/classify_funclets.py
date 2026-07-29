#!/usr/bin/env python3
"""classify_funclets.py — EH-unwind-funclet detector + decomp.db tagger.

Wave-3 lesson: the grind worklist repeatedly picked up MSVC X360 exception
*unwind funclets* and mistook them for workable 99.x near-misses. A funclet is
NOT an independent decomp target — the compiler emits it as part of its parent
function's try/cleanup region (one `.pdata` entry per funclet), and it matches
byte-for-byte automatically the moment the parent matches. Feeding one to a
matcher agent is a guaranteed zero-yield group.

## The signature (measured, not guessed)

Scanning every dtk target `.s` under ``build/45410914/asm/`` for blocks whose
FIRST instruction is ``subi r31, r12, IMM`` finds ~41.7k blocks — **every single
one anonymous** (`fn_XXXXXXXX`), 0 named; 99.7% have ``mflr`` as the second
instruction; sizes cluster tight at 10-17 instructions (0x28-0x44). No named,
real function begins this way: a normal MSVC prologue does ``mflr r12`` FIRST
(r12 is not yet meaningful), whereas an unwind funclet is entered with r12 =
the parent code address and computes its establisher frame via
``subi r31, r12, IMM`` before touching lr. That makes the prologue a
funclet-exclusive marker with ~zero false-positive risk.

## What this does

1. Parse each ``.fn NAME, ... / .endfn`` block in every target `.s`.
2. Classify a block as a funclet when:
     * NAME is anonymous (`fn_*`), AND
     * insn[0] is ``subi r31, r12, <imm>``, AND
     * insn[1] mnemonic is ``mflr``, AND
     * byte size (== insn_count * 4) <= ``--max-size`` (default 0x50).
   The prologue is the definitive test; the size cap is a loose guard against
   the handful of >0x50 outliers.
3. Tag matching rows in ``decomp.db``:
     * ``primary_pattern = 'eh_funclet'``
     * ``excluded = 1``   (unless ``--no-exclude``)
   ``excluded = 1`` is the DB's established "not a real workable target" flag
   (obj_regswap_patcher, the priority-score queries, and get_progress all treat
   ``excluded = 0`` as the workable set), so funclets belong there.
4. Print a summary: files scanned, blocks parsed, funclets detected, rows tagged,
   and a per-unit histogram.

Read-only DB access with ``--dry-run``. Stdlib only (no decomp_synth import) so
it runs anywhere the repo + DB are present.

Usage:
    venv/bin/python scripts/grind/classify_funclets.py
    venv/bin/python scripts/grind/classify_funclets.py --dry-run
    venv/bin/python scripts/grind/classify_funclets.py --max-size 0x40 --no-exclude
"""

from __future__ import annotations

import argparse
import glob
import os
import re
import sqlite3
import sys
from collections import Counter

_HERE = os.path.dirname(os.path.abspath(__file__))
_REPO = os.path.dirname(os.path.dirname(_HERE))

sys.path.insert(0, os.path.join(_REPO, "scripts", "harvest"))
from live_units import filter_live, repo_root_from_build_subdir  # noqa: E402

# `.fn fn_82758A50, global`  /  `.endfn fn_82758A50`
_FN_RE = re.compile(r"^\.fn\s+(\S+?)\s*,")
_ENDFN_RE = re.compile(r"^\.endfn\b")
# `/* 8275764C 00752E4C  3B EC FF 20 */\tsubi r31, r12, 0xe0`
#   grp1 = virtual address (hex), grp2 = mnemonic, grp3 = operands
_INSN_RE = re.compile(r"^\s*/\*\s*([0-9A-Fa-f]+)\b[^*]*\*/\s*(\S+)(.*)$")


def iter_fn_blocks(path):
    """Yield (name, addr0, insns) for each ``.fn .. .endfn`` block in a `.s` file.

    ``insns`` is a list of (mnemonic, operands_stripped) in program order; only
    real instruction lines (the ``/* addr .. */ mnem ops`` form) are collected,
    so data directives (``.4byte``, ``.balign``, ...) inside a block are ignored.
    ``addr0`` is the virtual address of the first instruction (hex str) or None.
    """
    with open(path, "r", errors="replace") as fh:
        name = None
        addr0 = None
        insns = []
        for line in fh:
            m = _FN_RE.match(line)
            if m:
                name, addr0, insns = m.group(1), None, []
                continue
            if name is None:
                continue
            mi = _INSN_RE.match(line)
            if mi:
                if addr0 is None:
                    addr0 = mi.group(1).upper()
                insns.append((mi.group(2), mi.group(3).strip()))
                continue
            if _ENDFN_RE.match(line):
                yield name, addr0, insns
                name, addr0, insns = None, None, []


def is_funclet(name, insns, max_size_bytes):
    """True when the block matches the MSVC X360 EH-unwind-funclet signature."""
    if not name.startswith("fn_"):
        return False
    if len(insns) < 2:
        return False
    m0, ops0 = insns[0]
    if m0 != "subi" or not ops0.replace(" ", "").startswith("r31,r12,"):
        return False
    if insns[1][0] != "mflr":
        return False
    return len(insns) * 4 <= max_size_bytes


def scan_asm(asm_dir, max_size_bytes):
    """Return (funclets, stats) where funclets maps name -> addr0 for every
    detected funclet, and stats carries scan counters + a strict-0x40 subcount.

    A single funclet SYMBOL can render in several target `.s` files, because the
    ``auto_03_*`` catch-all splits cover address ranges that also belong to named
    units. We dedup by symbol name (the DB has one row per symbol), so every
    count here is over unique symbols, not block occurrences."""
    funclets = {}          # name -> addr0 (unique symbols)
    sizes = {}             # name -> byte size (same block => same size)
    files = sorted(glob.glob(os.path.join(asm_dir, "**", "*.s"), recursive=True))
    n_globbed = len(files)
    repo_root = repo_root_from_build_subdir(asm_dir)
    try:
        files = sorted(filter_live(files, repo_root))
    except (OSError, ValueError) as e:
        print(f"[scan] WARNING: live-unit filter unavailable ({e}); "
              f"scanning ALL {n_globbed} globbed .s files, including any stale "
              f"orphans not in objdiff.json", file=sys.stderr)
    else:
        n_dropped = n_globbed - len(files)
        if n_dropped:
            print(f"[scan] WARNING: dropped {n_dropped} STALE .s files not in "
                  f"objdiff.json's live units (globbed {n_globbed}, kept "
                  f"{len(files)}) -- see scripts/harvest/live_units.py",
                  file=sys.stderr)
    n_blocks = 0
    for path in files:
        for name, addr0, insns in iter_fn_blocks(path):
            n_blocks += 1
            if is_funclet(name, insns, max_size_bytes) and name not in funclets:
                funclets[name] = addr0
                sizes[name] = len(insns) * 4
    size_hist = Counter(sizes.values())
    strict_40 = sum(1 for s in sizes.values() if s <= 0x40)
    return funclets, {
        "files": len(files),
        "blocks": n_blocks,
        "detected": len(funclets),
        "strict_0x40": strict_40,
        "size_hist": size_hist,
    }


def tag_db(db_path, funclet_names, *, set_excluded, dry_run):
    """Tag ``primary_pattern='eh_funclet'`` (+ ``excluded=1``) for every funclet
    name present in the functions table. Returns (present, tagged, per_unit)."""
    conn = sqlite3.connect(db_path)
    conn.row_factory = sqlite3.Row
    names = list(funclet_names)
    present = []
    per_unit = Counter()
    # Chunk the IN() lookup to stay under SQLite's variable limit.
    for i in range(0, len(names), 500):
        chunk = names[i:i + 500]
        qs = ",".join("?" * len(chunk))
        for row in conn.execute(
            f"SELECT symbol, unit FROM functions WHERE symbol IN ({qs})", chunk
        ):
            present.append(row["symbol"])
            per_unit[row["unit"] or "(no unit)"] += 1
    tagged = 0
    if not dry_run and present:
        set_clause = "primary_pattern = 'eh_funclet'"
        if set_excluded:
            set_clause += ", excluded = 1"
        for i in range(0, len(present), 500):
            chunk = present[i:i + 500]
            qs = ",".join("?" * len(chunk))
            cur = conn.execute(
                f"UPDATE functions SET {set_clause} WHERE symbol IN ({qs})", chunk
            )
            tagged += cur.rowcount
        conn.commit()
    conn.close()
    return present, tagged, per_unit


def main(argv=None):
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--asm-dir", default=os.path.join(_REPO, "build/45410914/asm"),
                    help="root of dtk target .s files (default: build/45410914/asm)")
    ap.add_argument("--db", default=os.path.join(_REPO, "decomp.db"),
                    help="orchestrator SQLite DB (default: decomp.db)")
    ap.add_argument("--max-size", type=lambda s: int(s, 0), default=0x50,
                    help="max funclet byte size (default 0x50; the subi/mflr "
                         "prologue is the real discriminator, this is a guard)")
    ap.add_argument("--no-exclude", action="store_true",
                    help="only set primary_pattern='eh_funclet'; leave excluded untouched")
    ap.add_argument("--dry-run", action="store_true",
                    help="detect + report only; do not write the DB")
    ap.add_argument("--top", type=int, default=25, help="per-unit histogram rows to print")
    args = ap.parse_args(argv)

    if not os.path.isdir(args.asm_dir):
        print(f"error: asm dir not found: {args.asm_dir}", file=sys.stderr)
        return 1

    funclets, stats = scan_asm(args.asm_dir, args.max_size)
    print(f"[scan] {stats['files']} .s files, {stats['blocks']} .fn blocks, "
          f"{stats['detected']} funclets (max-size 0x{args.max_size:x}; "
          f"{stats['strict_0x40']} also <= 0x40)")
    hist = stats["size_hist"]
    print("[scan] funclet byte-size histogram: " +
          ", ".join(f"0x{sz:x}:{n}" for sz, n in sorted(hist.items())))

    if not os.path.exists(args.db):
        print(f"warning: DB not found ({args.db}); detection only, no tagging")
        return 0

    present, tagged, per_unit = tag_db(
        args.db, funclets, set_excluded=not args.no_exclude, dry_run=args.dry_run)
    verb = "would tag" if args.dry_run else "tagged"
    print(f"[db] {len(present)}/{len(funclets)} detected funclets present in DB; "
          f"{verb} {len(present) if args.dry_run else tagged} rows "
          f"(primary_pattern='eh_funclet'"
          f"{'' if args.no_exclude else ', excluded=1'})")
    print(f"[db] per-unit funclet histogram (top {args.top}):")
    for unit, n in per_unit.most_common(args.top):
        print(f"    {n:5d}  {unit}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
