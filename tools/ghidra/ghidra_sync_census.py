#!/usr/bin/env python3
"""ghidra_sync_census.py -- the ruler for the Ghidra data sync (docs/plans/ghidra-data-sync-2026-11-22.md).

Opens a Ghidra project HEADLESSLY (exclusive access: never point it at the live :8002 project while the
service runs; use a reflink copy) and reports, for one program:

  * the function-name census: total functions, default-named (FUN_/Function_/thunk_FUN_), named, and how
    the program's names compare with scripts/target_symbol_map.json (agree / differ / map-only / program-only)
  * the 20-body decompile ruler (--bodies FILE: one "0xVA ..." per line): per body, unnamed call targets
    (FUN_/Function_ call sites), raw `param_N + 0x..`/`this + 0x..` member accesses, DAT_/PTR_DAT_ refs.
    Decompiles are saved under --out-dir so phases can be diffed.

Run (env as run_apply_symbols.sh: GHIDRA_INSTALL_DIR, JAVA_HOME):
  uv run --python 3.10 --project ../pyghidra-mcp python tools/ghidra/ghidra_sync_census.py \
      --project-location ~/tmp/ghidra-sync/proj/RB3Xenon --bodies ~/tmp/ghidra-sync/bodies.txt \
      --out-dir ~/tmp/ghidra-sync/census/phase0
"""
import argparse
import json
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DEFAULT_RE = re.compile(r"^(thunk_)?(FUN|Function)_[0-9A-Fa-f]{8}$")
CALL_UNNAMED_RE = re.compile(r"\b(?:thunk_)?(?:FUN|Function)_[0-9A-Fa-f]{8}\s*\(")
RAW_MEMBER_RE = re.compile(r"\b(?:param_\d+|this|in_r\d+|unaff_r\d+)\s*\+\s*0x[0-9a-fA-F]+")
DAT_RE = re.compile(r"\b(?:PTR_)?DAT_[0-9a-fA-F]{8}\b")
# every Ghidra auto-named data reference: DAT_, PTR_<target>_<va> (e.g. a vftable shown by its first slot),
# s_<text>_<va> strings. Phase 2 (data names) is measured on this one.
AUTO_DATA_RE = re.compile(r"\b(?:PTR_\w*?_[0-9a-fA-F]{8}|(?:PTR_)?DAT_[0-9a-fA-F]{8}|s_\w*?_[0-9a-fA-F]{8})\b")


def load_map(path):
    m = json.load(open(path))
    return {k.lower(): v for k, v in m.items()
            if k.lower().startswith("0x") and isinstance(v, str) and v}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--project-location", required=True)
    ap.add_argument("--project-name", default="RB3Xenon")
    ap.add_argument("--program", default="default.xex-d56e7f")
    ap.add_argument("--map", default=os.path.join(REPO, "scripts/target_symbol_map.json"))
    ap.add_argument("--bodies", default=None)
    ap.add_argument("--out-dir", required=True)
    args = ap.parse_args()
    os.makedirs(args.out_dir, exist_ok=True)
    tmap = load_map(args.map)

    import pyghidra
    pyghidra.start(verbose=False)
    from ghidra.base.project import GhidraProject
    from ghidra.app.decompiler import DecompInterface, DecompileOptions
    from ghidra.util.task import ConsoleTaskMonitor

    project = GhidraProject.openProject(args.project_location, args.project_name, True)
    program = project.openProgram("/", args.program, True)  # read-only
    try:
        fm = program.getFunctionManager()
        total = default = 0
        prog_names = {}
        for f in fm.getFunctions(True):
            total += 1
            n = f.getName()
            va = "0x%08x" % f.getEntryPoint().getOffset()
            if DEFAULT_RE.match(n):
                default += 1
            else:
                prog_names[va] = (n, str(f.getSymbol().getSource()))
        agree = sum(1 for va, s in tmap.items() if va in prog_names and prog_names[va][0] == s)
        differ = sum(1 for va, s in tmap.items() if va in prog_names and prog_names[va][0] != s)
        map_only = sum(1 for va in tmap if va not in prog_names)
        prog_only = {va: v for va, v in prog_names.items() if va not in tmap}
        prog_only_mangled = sum(1 for v in prog_only.values() if v[0].startswith("?"))
        census = {
            "program": args.program, "functions": total, "default_named": default,
            "named": len(prog_names), "map_rows": len(tmap),
            "map_name_agrees": agree, "map_name_differs": differ, "map_only": map_only,
            "program_only_named": len(prog_only), "program_only_mangled": prog_only_mangled,
            "labels_total": program.getSymbolTable().getNumSymbols(),
            "datatypes": program.getDataTypeManager().getDataTypeCount(True),
        }
        json.dump(prog_names, open(os.path.join(args.out_dir, "program_names.json"), "w"), indent=0)
        print("[census] " + json.dumps(census))

        if args.bodies:
            di = DecompInterface()
            di.setOptions(DecompileOptions())
            di.openProgram(program)
            af = program.getAddressFactory()
            rows = []
            for line in open(args.bodies):
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                va = line.split()[0].lower()
                f = fm.getFunctionAt(af.getAddress(va))
                if f is None:
                    rows.append({"va": va, "error": "no function"})
                    continue
                res = di.decompileFunction(f, 120, ConsoleTaskMonitor())
                c = res.getDecompiledFunction().getC() if res.decompileCompleted() else ""
                open(os.path.join(args.out_dir, "%s.c" % va), "w").write(c)
                body = c.split("{", 1)[1] if "{" in c else c
                sig = c.split("{", 1)[0]
                rows.append({"va": va, "name": f.getName(),
                             "unnamed_calls": len(CALL_UNNAMED_RE.findall(body)),
                             "raw_member": len(RAW_MEMBER_RE.findall(body)),
                             "dat_refs": len(DAT_RE.findall(body)),
                             "auto_data": len(AUTO_DATA_RE.findall(body)),
                             "undefined_in_sig": sig.count("undefined"),
                             "ok": bool(c)})
            di.dispose()
            tot = {k: sum(r.get(k, 0) for r in rows) for k in
                   ("unnamed_calls", "raw_member", "dat_refs", "auto_data", "undefined_in_sig")}
            json.dump({"rows": rows, "totals": tot}, open(os.path.join(args.out_dir, "bodies.json"), "w"), indent=1)
            for r in rows:
                print("[body] %s %-5s calls=%-3s raw=%-3s dat=%-3s undef_sig=%-2s %s" % (
                    r["va"], r.get("ok"), r.get("unnamed_calls"), r.get("raw_member"),
                    r.get("dat_refs"), r.get("undefined_in_sig"), r.get("name", r.get("error"))[:70]))
            print("[bodies] totals " + json.dumps(tot))
        json.dump(census, open(os.path.join(args.out_dir, "census.json"), "w"), indent=1)
    finally:
        project.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
