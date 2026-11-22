#!/usr/bin/env python3
"""add_program_to_project.py -- copy one program from a SOURCE Ghidra project into a DESTINATION project,
headlessly, by packing it to a .gzf and unpacking it at the destination root under the same name.

Never point --dst at the live project while the :8002 service runs: work on a reflink copy and swap it in
(see docs/plans/ghidra-data-sync-2026-11-22.md, "swap").

  ~/tmp/ghidra-sync/run.sh tools/ghidra/add_program_to_project.py \
      --src ~/tmp/ghidra-sync/tu5scratch/TU5Import --program default.xex-d56e7f \
      --dst ~/tmp/ghidra-sync/proj3/RB3Xenon/RB3Xenon --gzf ~/tmp/ghidra-sync/default.xex-d56e7f.named.gzf

--src / --dst are <dir>/<name> of a project (the .gpr is <dir>/<name>.gpr). Refuses if --dst already holds
the program (pass --replace to delete it first). Prints the function count on both sides as its control.
"""
import argparse
import os
import sys


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", required=True, help="source project <dir>/<name>")
    ap.add_argument("--program", required=True)
    ap.add_argument("--dst", required=True, help="destination project <dir>/<name>")
    ap.add_argument("--gzf", required=True, help="where to write the packed program")
    ap.add_argument("--replace", action="store_true")
    args = ap.parse_args()

    import pyghidra
    pyghidra.start()
    from java.io import File
    from ghidra.base.project import GhidraProject
    from ghidra.util.task import TaskMonitor

    sdir, sname = os.path.split(os.path.abspath(args.src))
    src = GhidraProject.openProject(sdir, sname, False)
    try:
        prog = src.openProgram("/", args.program, True)
        n_src = prog.getFunctionManager().getFunctionCount()
        src.close(prog)
        # the project's own DomainFile (a read-only open hands back a non-database proxy)
        df = src.getProjectData().getRootFolder().getFile(args.program)
        if os.path.exists(args.gzf):
            os.remove(args.gzf)
        df.packFile(File(args.gzf), TaskMonitor.DUMMY)
        print("[add_program] packed %s (%d fns) -> %s (%d bytes)"
              % (args.program, n_src, args.gzf, os.path.getsize(args.gzf)), flush=True)
    finally:
        src.close()

    ddir, dname = os.path.split(os.path.abspath(args.dst))
    dst = GhidraProject.openProject(ddir, dname, False)
    try:
        root = dst.getProjectData().getRootFolder()
        old = root.getFile(args.program)
        if old is not None:
            if not args.replace:
                print("[add_program] REFUSING: %s already in %s (pass --replace)" % (args.program, args.dst))
                return 2
            old.delete()
        nf = root.createFile(args.program, File(args.gzf), TaskMonitor.DUMMY)
        p2 = dst.openProgram("/", args.program, True)
        n_dst = p2.getFunctionManager().getFunctionCount()
        dst.close(p2)
        names = sorted(f.getPathname() for f in root.getFiles())
        print("[add_program] unpacked as %s: %d fns (source %d); project now holds %s"
              % (nf.getPathname(), n_dst, n_src, names), flush=True)
        if n_dst != n_src:
            print("[add_program] CONTROL FAILED: function counts differ")
            return 1
    finally:
        dst.close()
    print("[add_program] DONE")
    return 0


if __name__ == "__main__":
    sys.exit(main())
