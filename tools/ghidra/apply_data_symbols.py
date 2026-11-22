#!/usr/bin/env python3
"""apply_data_symbols.py -- phase 2 of docs/plans/ghidra-data-sync-2026-11-22.md: put the data names from
build_data_symbol_map.py into a Ghidra program as PRIMARY USER_DEFINED labels.

Headless, exclusive project access (run on a copy, or with the :8002 service stopped). Idempotent.
Reconciles its own previous run: a USER_DEFINED label whose name is a data-mangled spelling this script writes
(??_7 / ??_R / ??_C@ / __real@ / __imp_) on an address the map no longer lists is removed.

  run.sh tools/ghidra/apply_data_symbols.py --project-location <dir with RB3Xenon.gpr> \
      --program default_tu5.xex-c5a170 --map ~/tmp/ghidra-sync/data_map.json [--dry-run]
"""
import argparse
import json
import sys

OURS = ("??_7", "??_R", "??_C@", "??_8", "__real@", "__imp_")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--project-location", required=True)
    ap.add_argument("--project-name", default="RB3Xenon")
    ap.add_argument("--program", default="default_tu5.xex-c5a170")
    ap.add_argument("--map", required=True)
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()
    mapping = json.load(open(args.map))
    print("[apply_data] %d entries from %s" % (len(mapping), args.map))

    import pyghidra
    pyghidra.start(verbose=False)
    from ghidra.base.project import GhidraProject
    from ghidra.program.model.symbol import SourceType, SymbolType

    project = GhidraProject.openProject(args.project_location, args.project_name, True)
    program = project.openProgram("/", args.program, False)
    added = made_primary = already = no_mem = in_func = removed = errors = 0
    try:
        af = program.getAddressFactory()
        st = program.getSymbolTable()
        fm = program.getFunctionManager()
        mem = program.getMemory()
        tx = program.startTransaction("apply rb3 data symbols")
        ok = False
        try:
            for va, info in mapping.items():
                sym = info["symbol"]
                addr = af.getAddress(va)
                if addr is None or not mem.contains(addr):
                    no_mem += 1
                    continue
                if fm.getFunctionAt(addr) is not None:
                    in_func += 1          # never relabel a function entry with a data name
                    continue
                existing = [s for s in st.getSymbols(addr) if s.getName() == sym]
                if existing:
                    if existing[0].isPrimary():
                        already += 1
                        continue
                    if not args.dry_run:
                        existing[0].setPrimary()
                    made_primary += 1
                    continue
                if not args.dry_run:
                    try:
                        s = st.createLabel(addr, sym, SourceType.USER_DEFINED)
                        s.setPrimary()
                    except Exception as e:
                        errors += 1
                        print("[apply_data] ERROR %s %s: %s" % (va, sym, e))
                        continue
                added += 1
            claimed = set(mapping)
            for s in list(st.getAllSymbols(False)):
                if (s.getSymbolType() == SymbolType.LABEL and s.getSource() == SourceType.USER_DEFINED
                        and s.getName().startswith(OURS)
                        and ("0x%08x" % s.getAddress().getOffset()) not in claimed
                        and fm.getFunctionAt(s.getAddress()) is None):
                    if not args.dry_run:
                        s.delete()
                    removed += 1
            ok = True
        finally:
            program.endTransaction(tx, ok and not args.dry_run)
        if ok and not args.dry_run:
            project.save(program)
    finally:
        project.close()
    print("[apply_data] DONE added=%d made_primary=%d already=%d removed=%d in_function=%d not_in_memory=%d "
          "errors=%d%s" % (added, made_primary, already, removed, in_func, no_mem, errors,
                           " (DRY-RUN)" if args.dry_run else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
