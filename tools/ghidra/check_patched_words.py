#!/usr/bin/env python3
"""check_patched_words.py -- read the words tools/pe_word_diff.py reports between the Deluxe PE and clean TU5
(orig/45410914/rb3dx-archive/band.exe vs orig/45410914/band.exe; 53 words, docs/decomp/W16PT_CLEAN_TU5_RETARGET_
2026-10-06.md section 2) out of a Ghidra program and say whether each reads as retail (clean) or Deluxe.

  run.sh tools/ghidra/check_patched_words.py --project-location DIR --project-name NAME --program PROG \
      --diff ~/tmp/ghidra-sync/pe_word_diff.txt
"""
import argparse
import re
import sys


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--project-location", required=True)
    ap.add_argument("--project-name", required=True)
    ap.add_argument("--program", required=True)
    ap.add_argument("--diff", required=True, help="pe_word_diff.py output (old=Deluxe -> new=clean)")
    args = ap.parse_args()
    words = []
    for line in open(args.diff):
        m = re.match(r"(0x[0-9a-f]{8})\s+\S+\s+([0-9a-f]{8}) -> ([0-9a-f]{8})", line)
        if m:
            words.append((m.group(1), int(m.group(2), 16), int(m.group(3), 16)))
    import pyghidra
    pyghidra.start(verbose=False)
    from ghidra.base.project import GhidraProject
    proj = GhidraProject.openProject(args.project_location, args.project_name, True)
    prog = proj.openProgram("/", args.program, True)
    clean = deluxe = other = 0
    try:
        mem, af = prog.getMemory(), prog.getAddressFactory()
        for va, old, new in words:
            try:
                v = mem.getInt(af.getAddress(va)) & 0xFFFFFFFF
            except Exception:
                v = None
            if v == new:
                clean += 1
            elif v == old:
                deluxe += 1
            else:
                other += 1
                print("  OTHER %s ghidra=%s deluxe=%08x clean=%08x" % (va, v if v is None else "%08x" % v, old, new))
    finally:
        proj.close()
    print("[check_patched_words] %s: %d words: clean=%d deluxe=%d other=%d"
          % (args.program, len(words), clean, deluxe, other))
    return 0


if __name__ == "__main__":
    sys.exit(main())
