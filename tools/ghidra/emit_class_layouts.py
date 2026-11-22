#!/usr/bin/env python3
"""emit_class_layouts.py -- ask the COMPILER for every class layout in a set of TUs and merge them into one
JSON the Ghidra loader (apply_class_layouts.py) reads. One /d1reportAllClassLayout compile per TU, through
scripts/harvest/class_layout_report.py (the only authoritative layout source; header `// 0x..` comments and
struct_db.sqlite are derived and have been wrong).

  python3 tools/ghidra/emit_class_layouts.py --out ~/tmp/ghidra-sync/layouts.json [--dirs band3 system/meta ...]

A class seen in several TUs is kept only if every TU reports the same size and the same member offsets; a
name whose TUs disagree is dropped and listed under "conflicts", with every layout and its TUs under
"conflict_detail". MSVC prints nested classes by their BARE name, so most conflicts are different classes
sharing a name (`Target` = BandCamShot::Target and HamCamShot::Target), not ODR splits: tools/layout_odr.py
is the instrument that ties a layout to its definition and decides. A name printed twice with different
blocks inside one TU is a conflict too (clr.parse() keys by name, so it would keep the last one). TUs whose
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


def intra_tu_multiple(text):
    """Names the report prints more than once in ONE TU with different blocks.

    MSVC prints a nested class by its bare name (`BandCamShot::Target` and
    `HamCamShot::Target` are both `Target`, every `ObjPtrList<T>::Node` is
    `Node`), and clr.parse() keys classes by that name, so the last block wins.
    Without this check a TU that printed two different `Node`s contributed
    whichever came last, and a name could be kept with the wrong class's layout
    whenever every TU happened to print the same one last."""
    heads = list(clr.RE_CLASS_M.finditer(text))
    blocks = {}
    for i, m in enumerate(heads):
        end = heads[i + 1].start() if i + 1 < len(heads) else len(text)
        blocks.setdefault(m.group(1), set()).add(text[m.start():end].strip())
    return {n for n, b in blocks.items() if len(b) > 1}


def skip_name(name, c):
    # `<unnamed-tag>` is every anonymous struct/union's placeholder, not a name;
    # a size(0) block is an uninstantiated template pattern, not a layout
    # (tools/layout_odr.py skips both for the same reasons).
    return name.startswith("<") or c["size_printed"] == 0


def one(project_dir, src):
    try:
        _, text = clr.run_report(project_dir, None, tu=src, all_classes=True)
    except SystemExit as e:
        return src, None, str(e)[:300]
    diags = clr.diagnostics(text)
    if diags:
        return src, None, "; ".join(diags[:3])
    parsed = clr.parse(text)
    parsed["multi"] = intra_tu_multiple(text)
    return src, parsed, None


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
    detail, multi_in = {}, {}
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
                if skip_name(name, c):
                    continue
                sig = signature(c)
                if name in parsed["multi"]:
                    conflicts.add(name)
                    multi_in.setdefault(name, []).append(src)
                if name in sigs and sigs[name] != sig:
                    conflicts.add(name)
                detail.setdefault(name, {}).setdefault(sig, []).append(src)
                if name in sigs:
                    if sigs[name] == sig:
                        seen_in.setdefault(name, []).append(src)
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
    # Why each name was dropped: every distinct (size, members) layout with the
    # TUs that printed it, and the TUs that printed the name twice themselves.
    conflict_detail = {
        n: {"layouts": [{"size": sg[0], "members": [list(m) for m in sg[1]][:12],
                         "n_members": len(sg[1]), "n_tus": len(t), "tus": t[:5]}
                        for sg, t in sorted(detail.get(n, {}).items(), key=lambda kv: -len(kv[1]))],
            "intra_tu_multiple": multi_in.get(n, [])[:5]}
        for n in sorted(conflicts)}
    json.dump({"classes": classes, "vtables": vtables, "conflicts": sorted(conflicts),
               "conflict_detail": conflict_detail,
               "failed_tus": failed, "dirs": args.dirs, "n_tus": len(tus)},
              open(args.out, "w"), indent=0)
    print("[emit_layouts] DONE %d TUs (%d failed): %d classes kept, %d dropped as conflicting -> %s"
          % (len(tus), len(failed), len(classes), len(conflicts), args.out), flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
