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
import re
import sys

DEFAULT_NAME = re.compile(r"^(thunk_)?(FUN|Function)_[0-9A-Fa-f]{8}$")

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--project-location", required=True,
                    help="Directory containing <name>.gpr")
    ap.add_argument("--project-name", default="RB3Xenon")
    ap.add_argument("--program", default="default.xex-d56e7f",
                    help="Program name in project (default: default.xex-d56e7f, clean retail TU5)")
    ap.add_argument("--map", default=os.path.join(REPO, "tools/ghidra/rb3_symbol_map.json"))
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--verbose", action="store_true", help="print every CHANGE/REMOVE")
    ap.add_argument("--prev-map", default=None,
                    help="previously applied bank; a name equal to it on an unclaimed VA is removed")
    ap.add_argument("--target-map", default=os.path.join(REPO, "scripts/target_symbol_map.json"),
                    help="for the _bijection_arbitrary/_icf_arbitrary address lists")
    ap.add_argument("--aliases", default=os.path.join(REPO, "scripts/symbol_aliases.json"),
                    help="PROVEN ICF fold groups; folded spellings become secondary labels")
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
            cands = [n for n in names if n == "default.xex-d56e7f"] or names
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

    # ---- reconciliation inputs -------------------------------------------------
    # Today's map decides. A USER_DEFINED name on a body the map does not claim is
    # REMOVED when we put it there: it equals the previous bank's name at that VA
    # (--prev-map), or it is a mangled MSVC spelling ("?...") -- this script is the
    # only writer of mangled names into the program. Any other user name (hand RE
    # names such as "MyHelper") is kept and counted. Map rows that are null
    # ("deliberately unclaimed") count as unclaimed.
    prev = {}
    if args.prev_map and os.path.exists(args.prev_map):
        for k, v in json.load(open(args.prev_map)).items():
            sym = v.get("symbol") if isinstance(v, dict) else v
            if sym:
                prev[k.lower()] = sym
    arbitrary = set()
    if args.target_map and os.path.exists(args.target_map):
        tm = json.load(open(args.target_map))
        for key in ("_bijection_arbitrary", "_icf_arbitrary"):
            arbitrary |= {a.lower() for a in tm.get(key, []) if a}
    aliases = []
    if args.aliases and os.path.exists(args.aliases):
        for g in json.load(open(args.aliases)).get("groups", []):
            a = (g.get("address") or "").lower()
            for alt in g.get("folded", []) or []:
                if a and alt:
                    aliases.append((a, alt))
    claimed = {k.lower() for k, v in mapping.items() if v.get("symbol")}

    renamed = changed = already = created = no_func = errors = 0
    removed = kept_foreign = labels_added = labels_present = arb_tagged = 0
    ARB_TAG = ("[sync] name is an arbitrary pick among byte-identical twins "
               "(target_symbol_map _bijection_arbitrary/_icf_arbitrary)")
    try:
        af = program.getAddressFactory()
        fm = program.getFunctionManager()
        st = program.getSymbolTable()
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
                old = func.getName()
                if old == symbol:
                    already += 1
                else:
                    is_change = (func.getSymbol().getSource() == SourceType.USER_DEFINED
                                 and not DEFAULT_NAME.match(old))
                    if args.dry_run:
                        changed += is_change
                        renamed += not is_change
                        if is_change and args.verbose:
                            print("[apply_symbols] CHANGE %s %s -> %s" % (addr_str, old, symbol))
                        continue
                    try:
                        func.setName(symbol, SourceType.USER_DEFINED)
                        # apply_prototypes.py moves a function into its class namespace; a new name
                        # may belong to another class, so go back to global and let it re-place it.
                        if not func.getParentNamespace().isGlobal():
                            func.setParentNamespace(program.getGlobalNamespace())
                        if is_change:
                            changed += 1
                            if args.verbose:
                                print("[apply_symbols] CHANGE %s %s -> %s" % (addr_str, old, symbol))
                        else:
                            renamed += 1
                    except Exception as e:
                        errors += 1
                        print("[apply_symbols] rename ERROR %s -> %s: %s" % (addr_str, symbol, e))
                        continue
                if args.dry_run:
                    continue
                # Comments: drop stale [decomp] lines, add the current one.
                dem = info.get("demangled")
                tag = ("[decomp] %s" % dem) if dem else None
                existing = func.getComment() or ""
                lines = [l for l in existing.split("\n")
                         if l and not (l.startswith("[decomp] ") and l != tag)]
                if tag and tag not in lines:
                    lines.append(tag)
                if addr_str.lower() in arbitrary and ARB_TAG not in lines:
                    lines.append(ARB_TAG)
                    arb_tagged += 1
                new_comment = "\n".join(lines)
                if new_comment != existing:
                    func.setComment(new_comment or None)

            # Reconcile: names on bodies the map no longer claims.
            for func in list(fm.getFunctions(True)):
                va = "0x%08x" % func.getEntryPoint().getOffset()
                if va in claimed:
                    continue
                sym = func.getSymbol()
                if sym.getSource() != SourceType.USER_DEFINED:
                    continue
                name = func.getName()
                if prev.get(va) == name or name.startswith("?"):
                    if args.verbose:
                        print("[apply_symbols] REMOVE %s %s" % (va, name))
                    if not args.dry_run:
                        if not func.getParentNamespace().isGlobal():
                            func.setParentNamespace(program.getGlobalNamespace())
                        func.setName(None, SourceType.DEFAULT)
                        c = func.getComment() or ""
                        keep = [l for l in c.split("\n") if l and not l.startswith("[decomp] ")
                                and l != ARB_TAG]
                        func.setComment("\n".join(keep) or None)
                    removed += 1
                else:
                    kept_foreign += 1

            # Fold spellings (scripts/symbol_aliases.json) as secondary labels.
            from ghidra.program.model.symbol import SymbolType
            for a, alt in aliases:
                addr = af.getAddress(a)
                if addr is None:
                    continue
                present = any(s.getName() == alt for s in st.getSymbols(addr))
                if present:
                    labels_present += 1
                    continue
                if not args.dry_run:
                    st.createLabel(addr, alt, SourceType.USER_DEFINED)
                labels_added += 1
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

    print("[apply_symbols] DONE renamed=%d changed=%d removed=%d kept_foreign=%d already=%d "
          "created_fn=%d no_func=%d errors=%d alias_labels_added=%d alias_labels_present=%d "
          "arbitrary_tagged=%d%s"
          % (renamed, changed, removed, kept_foreign, already, created, no_func, errors,
             labels_added, labels_present, arb_tagged,
             " (DRY-RUN, no save)" if args.dry_run else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
