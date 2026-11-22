#!/usr/bin/env python3
"""apply_prototypes.py -- phase 3 of docs/plans/ghidra-data-sync-2026-11-22.md: give every function named with
an MSVC mangled spelling (by apply_symbols.py) the prototype its mangled name encodes.

Ghidra's DemangledFunction.applyTo is a silent no-op on a function whose symbol is USER_DEFINED
(isAlreadyDemangled), which is why pyghidra-mcp's apply_demangled_signatures never did anything here. So per
function: switch the symbol source to IMPORTED, applyTo (moves the function into its class namespace, sets
__thiscall + `Class *this`, params and return), then restore the MANGLED name as the USER_DEFINED primary
name -- tools look functions up by exact mangled func.getName(), so that convention is kept; only the parent
namespace changes (getName(True) = "MsgSource::?AddSink@MsgSource@@...").

Headless, exclusive project access (copy, or service stopped). Re-runnable.
"""
import argparse
import sys


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--project-location", required=True)
    ap.add_argument("--project-name", default="RB3Xenon")
    ap.add_argument("--program", default="default.xex-d56e7f")
    ap.add_argument("--limit", type=int, default=0)
    args = ap.parse_args()

    import pyghidra
    pyghidra.start(verbose=False)
    from ghidra.base.project import GhidraProject
    from ghidra.app.util.demangler import DemanglerOptions
    from ghidra.app.util.demangler.microsoft import MicrosoftDemangler
    from ghidra.program.model.symbol import SourceType
    from ghidra.util.task import TaskMonitor

    project = GhidraProject.openProject(args.project_location, args.project_name, True)
    program = project.openProgram("/", args.program, False)
    dem = MicrosoftDemangler()
    opts = DemanglerOptions()
    opts.setApplySignature(True)
    opts.setApplyCallingConvention(True)
    st = program.getSymbolTable()
    applied = not_function_name = demangle_failed = apply_failed = mismatch_before = 0
    bm = program.getBookmarkManager()
    try:
        mismatch_before = bm.getBookmarkCount("Error") if hasattr(bm, "getBookmarkCount") else 0
        tx = program.startTransaction("apply rb3 prototypes from mangled names")
        ok = False
        try:
            funcs = [f for f in program.getFunctionManager().getFunctions(True)
                     if f.getName().startswith("?") and f.getSymbol().getSource() == SourceType.USER_DEFINED]
            if args.limit:
                funcs = funcs[:args.limit]
            print("[apply_prototypes] %d mangled-named functions" % len(funcs), flush=True)
            for f in funcs:
                m = f.getName()
                try:
                    d = dem.demangle(m)
                except Exception:
                    d = None
                if d is None:
                    demangle_failed += 1
                    continue
                if d.getClass().getSimpleName() != "DemangledFunction":
                    not_function_name += 1
                    continue
                sym = f.getSymbol()
                good = False
                try:
                    sym.setSource(SourceType.IMPORTED)
                    good = d.applyTo(program, f.getEntryPoint(), opts, TaskMonitor.DUMMY)
                except Exception as e:
                    if apply_failed < 10:
                        print("[apply_prototypes] apply error %s %s: %s" % (f.getEntryPoint(), m[:60], e))
                finally:
                    # applyTo makes the demangled name primary and leaves the mangled one as a label:
                    # drop that label, then put the mangled spelling back on the function symbol.
                    for s in list(st.getSymbols(f.getEntryPoint())):
                        if s.getName() == m and s != f.getSymbol():
                            s.delete()
                    f.setName(m, SourceType.USER_DEFINED)
                    f.getSymbol().setSource(SourceType.USER_DEFINED)
                if good:
                    applied += 1
                else:
                    apply_failed += 1
            ok = True
        finally:
            program.endTransaction(tx, ok)
        if ok:
            project.save(program)
    finally:
        project.close()
    print("[apply_prototypes] DONE applied=%d not_a_function_name=%d demangle_failed=%d apply_failed=%d"
          % (applied, not_function_name, demangle_failed, apply_failed))
    return 0


if __name__ == "__main__":
    sys.exit(main())
