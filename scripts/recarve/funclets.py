#!/usr/bin/env python3
"""Shared EH-funclet detector for the recarve pipeline.

MSVC X360 EH unwind funclets establish the PARENT frame via r12:
their first instruction is `subi/addi r3x, r12, imm` (e.g. 3B EC FF 90 =
subi r31, r12, 0x70). They are parent-dependent code with ZERO standalone
match yield (wave-17 ops lesson: "78 quick closes" that were all funclets),
at any size — the old 0x20-0x30 size band missed 48B ones (Waypoint E2E,
2026-07-12).

Builds a VA-set cache by scanning the dtk target asm (`build/.../asm/**/*.s`)
first-instruction lines once; cache invalidates on the asm dir's newest mtime.
"""
import json, re, subprocess, sys
from pathlib import Path

CACHE = Path.home() / "tmp/recarve/funclet_vas.json"
FN_RX = re.compile(r"^\.fn (fn_)?([0-9A-Fa-f]{8})?")
# first-instr tell: subi/addi rX, r12, imm  (parent-frame establisher)
PROLOGUE_RX = re.compile(r"\b(?:subi|addi)\s+r\d+,\s*r12,")


def build_funclet_set(asm_dir: Path) -> set[int]:
    """Scan all target .s files; return VAs whose FIRST instruction is the
    r12 parent-frame establisher. Uses rg for the initial narrowing pass."""
    vas = set()
    # rg: every .fn line + the line after it, across the whole asm tree
    r = subprocess.run(
        ["rg", "-A1", "--no-filename", "--no-heading", r"^\.fn ", str(asm_dir)],
        stdout=subprocess.PIPE, text=True)
    cur_va = None
    for line in r.stdout.splitlines():
        m = re.match(r"^\.fn \S*?([0-9A-Fa-f]{8}),", line)
        if m:
            cur_va = int(m.group(1), 16)
            continue
        if line.startswith("--"):
            cur_va = None
            continue
        if cur_va is not None:
            # this is the first line after .fn — the first instruction
            if "/* " in line and PROLOGUE_RX.search(line):
                vas.add(cur_va)
            cur_va = None
    return vas


def load_funclet_set(repo: Path, rebuild: bool = False) -> set[int]:
    asm_dir = repo / "build/45410914/asm"
    if not asm_dir.exists():
        return set()
    newest = max((p.stat().st_mtime for p in asm_dir.rglob("*.s")), default=0)
    if not rebuild and CACHE.exists():
        try:
            d = json.load(open(CACHE))
            if d.get("asm_mtime") == newest and d.get("repo") == str(repo):
                return set(d["vas"])
        except Exception:
            pass
    vas = build_funclet_set(asm_dir)
    CACHE.parent.mkdir(parents=True, exist_ok=True)
    json.dump({"repo": str(repo), "asm_mtime": newest, "vas": sorted(vas)},
              open(CACHE, "w"))
    return vas


if __name__ == "__main__":
    repo = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parent.parent.parent
    vas = load_funclet_set(repo, rebuild=True)
    print(f"{len(vas)} funclet VAs cached -> {CACHE}")
