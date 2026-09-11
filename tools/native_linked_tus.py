#!/usr/bin/env python3
"""native_linked_tus.py -- the set of src/ TUs emitted into ANY native target.

WHY: lane W3-A (2026-09-11) audits wrong-callee divergences, which only become
runtime bugs in code the native executables actually LINK.  The set must be the
COMPILER's answer (scatter_emitted_truth.deps_of, `clang++ -M`), not the
textual state machine in scatter_audit.py, because conditional scatter edges
(UIList.cpp -> GemTrack.cpp behind a self-defined sentinel) are resolved only
by the preprocessor.  A TU counts if it is a compiled primary source of a target
OR is scatter-included (at any depth) by one.

USAGE
    tools/native_linked_tus.py [project_dir] [--out LIST] [--json OUT]
Requires native/build configured (ninja -t compdb).

EXIT: 0 ok; 2 could not run; 3 self-validation failed (a known-linked TU absent
-- the vacuous-empty-set trap).
"""
import argparse, collections, json, os, sys
from concurrent.futures import ThreadPoolExecutor

KNOWN = ["src/band3/game/GemPlayer.cpp", "src/system/utl/Symbol.cpp",
         "src/system/synth/Synth.cpp"]  # synth_xbox is NOT native-linked (brief said Voice.cpp; refuted 2026-09-11)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("project_dir", nargs="?", default=None)
    ap.add_argument("--out", default=None)
    ap.add_argument("--json", default=None)
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 8)
    a = ap.parse_args()
    here = os.path.dirname(os.path.abspath(__file__))
    repo = os.path.abspath(a.project_dir or os.path.dirname(here))
    sys.path.insert(0, os.path.join(repo, "tools"))
    import scatter_audit as SA
    import scatter_emitted_truth as SET
    build = os.path.join(repo, "native", "build")
    if not os.path.isdir(build):
        sys.exit(2)
    targets = SA.target_sources()
    if not targets:
        print("no targets", file=sys.stderr); sys.exit(2)
    db = SET.compdb(build)
    jobs = []
    for t, S in sorted(targets.items()):
        for src in sorted(S):
            key = None
            for outp, e in db.items():
                if ("%s.dir" % t) in outp and os.path.abspath(
                        os.path.join(build, e["file"])) == os.path.abspath(src):
                    key = e; break
            if key:
                jobs.append((t, src, key["command"], key.get("directory", build)))
    emitted = collections.defaultdict(set)
    failures = []
    def work(j):
        t, src, cmd, d = j
        return (t, src, SET.deps_of(cmd, src, d))
    with ThreadPoolExecutor(max_workers=a.jobs) as ex:
        for t, src, r in ex.map(work, jobs):
            if r is None: failures.append((t, SA.rel(src)))
            else: emitted[t] |= r
    per_target = {}
    union = set()
    for t, S in sorted(targets.items()):
        allt = set(S) | emitted.get(t, set())
        rels = sorted(SA.rel(x) for x in allt)
        rels = [r for r in rels if r.startswith("src/")]
        per_target[t] = rels
        union |= set(rels)
    union = sorted(union)
    print("targets: %d, preprocess jobs: %d, failures: %d" % (len(targets), len(jobs), len(failures)))
    for f in failures[:10]: print("  FAIL", f)
    for t in sorted(per_target): print("  %-16s %4d TUs" % (t, len(per_target[t])))
    print("UNION: %d src/ TUs" % len(union))
    missing = [k for k in KNOWN if k not in union]
    if missing:
        print("SELF-VALIDATION FAILED, absent:", missing); rc = 3
    else:
        print("self-validation: all %d known-linked TUs present" % len(KNOWN)); rc = 0
    if a.out:
        with open(a.out, "w") as fh: fh.write("\n".join(union) + "\n")
    if a.json:
        with open(a.json, "w") as fh:
            json.dump({"targets": per_target, "union": union, "failures": failures}, fh, indent=1)
    return rc

if __name__ == "__main__":
    sys.exit(main())
