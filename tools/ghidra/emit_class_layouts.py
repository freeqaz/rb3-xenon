#!/usr/bin/env python3
"""emit_class_layouts.py -- ask the COMPILER for every class layout in a set of TUs and merge them into one
JSON the Ghidra loader (apply_class_layouts.py) reads. One /d1reportAllClassLayout compile per TU, through
scripts/harvest/class_layout_report.py (the only authoritative layout source; header `// 0x..` comments and
struct_db.sqlite are derived and have been wrong).

  python3 tools/ghidra/emit_class_layouts.py --out ~/tmp/ghidra-sync/layouts.json [--dirs band3 system/meta ...]

A class seen in several TUs is kept only if every TU reports the same size and the same member offsets; a
class whose TUs disagree (an ODR split, a per-TU /D gate) is dropped and listed under "conflicts". TUs whose
compile prints a diagnostic contribute nothing ("failed_tus"). Needs a configured tree (build.ninja).
"""
import argparse
import concurrent.futures
import json
import os
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO, "scripts", "harvest"))
import class_layout_report as clr  # noqa: E402

# src/band3 plus the src/system dirs the native port compiles (native/CMakeLists.txt), 2026-11-22.
DEFAULT_DIRS = ["band3", "system/synth", "system/bandobj", "system/beatmatch", "system/meta",
                "system/flow", "system/rndobj", "system/midi", "system/dsp", "system/utl", "system/ui",
                "system/track", "system/obj", "system/world", "system/os", "system/char", "system/movie",
                "system/math"]


def tus_for(project_dir, dirs):
    r = subprocess.run(["ninja", "-t", "targets", "all"], cwd=project_dir, capture_output=True, text=True)
    objs = set()
    for line in r.stdout.splitlines():
        t = line.split(":")[0]
        if t.startswith("build/45410914/src/") and t.endswith(".obj"):
            objs.add(t)
    out = []
    for o in sorted(objs):
        rel = o[len("build/45410914/src/"):]
        if any(rel.startswith(d + "/") for d in dirs):
            src = "src/" + rel[:-4] + ".cpp"
            if os.path.exists(os.path.join(project_dir, src)):
                out.append(src)
    return out


def one(project_dir, src):
    try:
        _, text = clr.run_report(project_dir, None, tu=src, all_classes=True)
    except SystemExit as e:
        return src, None, str(e)[:300]
    diags = clr.diagnostics(text)
    if diags:
        return src, None, "; ".join(diags[:3])
    return src, clr.parse(text), None


def signature(c):
    return (c["size"], tuple((m["offset"], m["name"]) for m in c["members"]))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--dirs", nargs="*", default=DEFAULT_DIRS, help="dirs under src/")
    ap.add_argument("--project-dir", default=REPO)
    ap.add_argument("-j", type=int, default=max(1, (os.cpu_count() or 4) // 2))
    ap.add_argument("--limit", type=int, default=0)
    args = ap.parse_args()

    tus = tus_for(args.project_dir, args.dirs)
    if args.limit:
        tus = tus[:args.limit]
    print("[emit_layouts] %d TUs under %s" % (len(tus), args.dirs), flush=True)
    classes, seen_in, sigs, conflicts, failed, vtables = {}, {}, {}, set(), {}, {}
    done = 0
    with concurrent.futures.ThreadPoolExecutor(args.j) as ex:
        for src, parsed, err in ex.map(lambda s: one(args.project_dir, s), tus):
            done += 1
            if done % 50 == 0:
                print("[emit_layouts] %d/%d TUs, %d classes" % (done, len(tus), len(classes)), flush=True)
            if parsed is None:
                failed[src] = err
                continue
            for name, c in parsed["classes"].items():
                sig = signature(c)
                if name in sigs and sigs[name] != sig:
                    conflicts.add(name)
                    continue
                sigs[name] = sig
                classes.setdefault(name, c)
                seen_in.setdefault(name, []).append(src)
            for k, v in parsed["vtables"].items():
                vtables.setdefault(k, v)
    for n in conflicts:
        classes.pop(n, None)
    for n, c in classes.items():
        c["tus"] = seen_in[n][:3]
        c["n_tus"] = len(seen_in[n])
    json.dump({"classes": classes, "vtables": vtables, "conflicts": sorted(conflicts),
               "failed_tus": failed, "dirs": args.dirs, "n_tus": len(tus)},
              open(args.out, "w"), indent=0)
    print("[emit_layouts] DONE %d TUs (%d failed): %d classes kept, %d dropped as conflicting -> %s"
          % (len(tus), len(failed), len(classes), len(conflicts), args.out), flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
