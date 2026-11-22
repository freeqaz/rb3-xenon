#!/usr/bin/env python3
"""import_clean_tu5.py -- import CLEAN retail TU5 (orig/45410914/default.xex, sha1 d56e7f31...) as its own
Ghidra program, named the way pyghidra-mcp names imports (basename + sha1[:6] = default.xex-d56e7f), with the
SAME analyzer options as the existing Deluxe program (default_tu5.xex-c5a170), fully analysed, saved, and
exported as a .gzf so it can be added to the live project later without the live service ever being touched.

Runs in a SCRATCH project (never the live one). Long: full auto-analysis of a 15 MB XEX. Run in background:
  ~/tmp/ghidra-sync/run.sh tools/ghidra/import_clean_tu5.py --scratch ~/tmp/ghidra-sync/tu5scratch \
      --ref-project ~/tmp/ghidra-sync/proj0_pristine/RB3Xenon --gzf ~/tmp/ghidra-sync/default.xex-d56e7f.gzf
Done when the log prints "[import_clean_tu5] DONE" and the .gzf exists.
"""
import argparse
import hashlib
import os
import sys
import time

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--xex", default=os.path.join(REPO, "orig/45410914/default.xex"))
    ap.add_argument("--scratch", required=True, help="dir for a NEW scratch project")
    ap.add_argument("--ref-project", required=True, help="dir holding RB3Xenon.gpr (a copy, not live)")
    ap.add_argument("--ref-program", default="default_tu5.xex-c5a170")
    ap.add_argument("--gzf", required=True)
    args = ap.parse_args()

    sha = hashlib.sha1(open(args.xex, "rb").read()).hexdigest()
    if not sha.startswith("d56e7f31"):
        print("[import_clean_tu5] REFUSING: %s sha1 %s is not clean retail TU5" % (args.xex, sha))
        return 2
    prog_name = "%s-%s" % (os.path.basename(args.xex), sha[:6])
    print("[import_clean_tu5] program name %s" % prog_name, flush=True)

    import pyghidra
    from pyghidra_mcp.server import _install_xex_loader
    launcher = pyghidra.HeadlessPyGhidraLauncher()
    _install_xex_loader(launcher)
    launcher.start()

    from java.io import File
    from ghidra.base.project import GhidraProject
    from ghidra.program.model.lang import LanguageID
    from ghidra.program.util import DefaultLanguageService, GhidraProgramUtilities
    from ghidra.program.flatapi import FlatProgramAPI
    from ghidra.framework.options import OptionType

    # Reference analyzer options.
    ref = GhidraProject.openProject(args.ref_project, "RB3Xenon", True)
    refprog = ref.openProgram("/", args.ref_program, True)
    ropts = refprog.getOptions("Analyzers")
    ref_vals = {}
    for n in ropts.getOptionNames():
        try:
            if ropts.getType(n) in (OptionType.BOOLEAN_TYPE, OptionType.INT_TYPE,
                                    OptionType.STRING_TYPE, OptionType.LONG_TYPE,
                                    OptionType.DOUBLE_TYPE, OptionType.FLOAT_TYPE):
                ref_vals[n] = ropts.getObject(n, None)
        except Exception:
            pass
    ref_funcs = refprog.getFunctionManager().getFunctionCount()
    ref.close()
    print("[import_clean_tu5] %d analyzer options copied from %s" % (len(ref_vals), args.ref_program), flush=True)

    os.makedirs(args.scratch, exist_ok=True)
    proj = GhidraProject.createProject(args.scratch, "TU5Import", False)
    try:
        lang = DefaultLanguageService.getLanguageService().getLanguage(LanguageID("PowerPC:BE:64:Xenon"))
        program = proj.importProgram(File(args.xex), lang, lang.getDefaultCompilerSpec())
        program.setName(prog_name)
        proj.saveAs(program, "/", prog_name, True)
        print("[import_clean_tu5] imported; functions before analysis: %d"
              % program.getFunctionManager().getFunctionCount(), flush=True)
        opts = program.getOptions("Analyzers")
        tx = program.startTransaction("copy analyzer options")
        set_n = 0
        try:
            for n, v in ref_vals.items():
                try:
                    if opts.contains(n) and v is not None:
                        opts.putObject(n, v)
                        set_n += 1
                except Exception:
                    pass
        finally:
            program.endTransaction(tx, True)
        print("[import_clean_tu5] %d analyzer options set; analysing (long) ..." % set_n, flush=True)
        t0 = time.time()
        FlatProgramAPI(program).analyzeAll(program)
        GhidraProgramUtilities.markProgramAnalyzed(program)
        proj.save(program)
        print("[import_clean_tu5] analysed in %.0f s: functions %d (Deluxe reference had %d before phase 1)"
              % (time.time() - t0, program.getFunctionManager().getFunctionCount(), ref_funcs), flush=True)
        if os.path.exists(args.gzf):
            os.remove(args.gzf)
        program.getDomainFile().packFile(File(args.gzf), None)
        print("[import_clean_tu5] DONE gzf=%s" % args.gzf, flush=True)
    finally:
        proj.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
