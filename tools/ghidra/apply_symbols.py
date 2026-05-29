#!/usr/bin/env python3
"""
apply_symbols.py — import our known mangled symbols into the RB3 Ghidra project
by renaming the anonymous fn_<addr>/FUN_<addr> functions, so the decomp_synth
permuter's Ghidra-guided fetch (which queries by MANGLED symbol via pyghidra-mcp's
exact func.name match — Strategy 2 in pyghidra_mcp/tools.py) resolves.

This is a STANDALONE PyGhidra (Python 3 + JPype) script. It opens the existing
Ghidra project headlessly with exclusive access, renames functions by address,
saves, and closes. Run via tools/ghidra/run_apply_symbols.sh, which stops the
:8002 pyghidra-mcp service first (the service holds the project lock) and
restarts it afterward.

Reads the map produced by build_symbol_map.py:
  tools/ghidra/rb3_symbol_map.json  ==  {"0x82463e08": {"symbol": "?CalcScale@...",
                                          "demangled": "...", ...}, ...}

IDEMPOTENT: setting the same primary name is a no-op; re-runnable after a Ghidra
re-import (tools/ghidra/import-xex.sh). We set the mangled symbol as the function's
PRIMARY name with SourceType.USER_DEFINED so pyghidra-mcp's exact func.name match
finds it.

Env required (set by run_apply_symbols.sh): GHIDRA_INSTALL_DIR, JAVA_HOME.

Usage:
  python3 tools/ghidra/apply_symbols.py \
      --project-location <dir containing .gpr> --project-name RB3Xenon \
      [--program default.xex-35adb6] [--map tools/ghidra/rb3_symbol_map.json] \
      [--dry-run]
"""
import argparse
import json
import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--project-location", required=True,
                    help="Directory containing <name>.gpr")
    ap.add_argument("--project-name", default="RB3Xenon")
    ap.add_argument("--program", default=None,
                    help="Program name in project (default: first program found)")
    ap.add_argument("--map", default=os.path.join(REPO, "tools/ghidra/rb3_symbol_map.json"))
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    with open(args.map) as f:
        mapping = json.load(f)
    print("[apply_symbols] %d entries from %s" % (len(mapping), args.map))

    import pyghidra
    pyghidra.start(verbose=False)

    from ghidra.base.project import GhidraProject
    from ghidra.program.model.symbol import SourceType
    from ghidra.app.cmd.function import CreateFunctionCmd

    project = GhidraProject.openProject(args.project_location, args.project_name, True)
    try:
        root = project.getProject().getProjectData().getRootFolder()
        files = list(root.getFiles())
        names = [df.getName() for df in files]
        print("[apply_symbols] programs in project: %s" % names)
        target_name = args.program
        if target_name is None:
            cands = [n for n in names if n.startswith("default.xex")] or names
            if not cands:
                print("[apply_symbols] ERROR: no programs in project")
                return 2
            target_name = cands[0]
        print("[apply_symbols] opening program: %s" % target_name)
        program = project.openProgram("/", target_name, False)  # not read-only
    except Exception as e:
        print("[apply_symbols] ERROR opening program: %s" % e)
        project.close()
        return 2

    renamed = already = created = no_func = errors = 0
    try:
        af = program.getAddressFactory()
        fm = program.getFunctionManager()
        tx = program.startTransaction("apply rb3 symbol map")
        committed = False
        try:
            for addr_str, info in mapping.items():
                symbol = info.get("symbol")
                if not symbol:
                    continue
                try:
                    addr = af.getAddress(addr_str)
                except Exception:
                    addr = None
                if addr is None:
                    errors += 1
                    continue
                func = fm.getFunctionAt(addr)
                if func is None:
                    cmd = CreateFunctionCmd(addr)
                    if not args.dry_run and cmd.applyTo(program):
                        func = fm.getFunctionAt(addr)
                        if func is not None:
                            created += 1
                    if func is None:
                        cont = fm.getFunctionContaining(addr)
                        if cont is None or cont.getEntryPoint() != addr:
                            no_func += 1
                            continue
                        func = cont
                if func.getName() == symbol:
                    already += 1
                    continue
                if args.dry_run:
                    renamed += 1
                    continue
                try:
                    func.setName(symbol, SourceType.USER_DEFINED)
                    renamed += 1
                    dem = info.get("demangled")
                    if dem:
                        existing = func.getComment()
                        tag = "[decomp] %s" % dem
                        if not existing:
                            func.setComment(tag)
                        elif tag not in existing:
                            func.setComment(existing + "\n" + tag)
                except Exception as e:
                    errors += 1
                    print("[apply_symbols] rename ERROR %s -> %s: %s" % (addr_str, symbol, e))
            committed = True
        finally:
            program.endTransaction(tx, committed and not args.dry_run)

        if not args.dry_run and committed:
            print("[apply_symbols] saving program ...")
            project.save(program)
    finally:
        # GhidraProject.openProgram registers the project as the consumer; closing
        # the project releases it. Calling program.release(project) here can throw
        # an "unknown consumer" IllegalArgumentException on this Ghidra build, so
        # just close the project (which releases its programs).
        try:
            project.close()
        except Exception as e:
            print("[apply_symbols] (note) project.close: %s" % e)

    print("[apply_symbols] DONE renamed=%d already=%d created_fn=%d no_func=%d errors=%d%s"
          % (renamed, already, created, no_func, errors,
             " (DRY-RUN, no save)" if args.dry_run else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
