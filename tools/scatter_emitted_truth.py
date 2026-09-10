#!/usr/bin/env python3
"""scatter_emitted_truth.py -- the COMPILER's answer to "which .cpp files does
each native target actually emit", used as an independent cross-check on
tools/scatter_audit.py.

WHY THIS EXISTS
---------------
scatter_audit.py computes each target's scatter closure with a hand-written
textual state machine that traverses UNCONDITIONAL `#include "x.cpp"` edges
only. That is deliberate -- it mirrors native/cmake/ScatterIncludes.cmake, so
the audit and the build's dedupe agree BY CONSTRUCTION.

But agreeing with the dedupe module is not the same as agreeing with the
COMPILER, and docs/decomp/NATIVE_HEALTH.md flagged exactly this as unverified:
"scatter_audit.py's own correctness ... its agreement with ScatterIncludes.cmake
-- which it claims is true by construction -- was not independently tested."

It is not correct, and the failure is in the direction that INFLATES the
headline. A scatter edge can sit behind `#ifdef SENTINEL` where SENTINEL is
`#define`d by the host file itself, a few lines above, precisely so the include
fires when the host is the PRIMARY TU and stays inert when the host is itself
scatter-included. src/system/ui/UIList.cpp:7 does this for
band3/bandtrack/GemTrack.cpp. The state machine sees `#ifdef` and files the
edge as conditional, so it never traverses it -- and reports GemTrack.cpp as
"reaching no native target" while the linker is, in fact, already defining
GemTrack's every symbol out of UIList.cpp.o.

⇒ Some of the "unlinked scatter guests" are FALSE POSITIVES: already emitted.
A count that drifts is bad; a count that is wrong in the flattering direction
and drives wiring work is worse, because the work is absent-vs-absent.

METHOD
------
Ask the compiler. `clang++ -M` on a TU emits its full dependency list with every
conditional resolved exactly as the real compile resolves it, and it is far
cheaper than `-E` (a dep list, not preprocessed text). Any `.cpp` in that list
other than the TU itself IS emitted by it -- that is what "emitted" means.

Commands come from `ninja -t compdb`, so each TU is preprocessed with ITS OWN
target's real defines. Using one target's flags for another would reintroduce
the guess this tool exists to remove.

USAGE
    tools/scatter_emitted_truth.py [project_dir] [--json OUT] [--jobs N]

EXIT CODES
    0  ran; the two instruments AGREE
    1  ran; they DISAGREE (the disagreement list is the finding, not an error)
    2  could not run (no build dir, no compdb, no targets)
"""

import argparse
import collections
import json
import os
import re
import shlex
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

CPP_INC = re.compile(r'#[ \t]*include[ \t]*"([^"]+\.cpp)"')


def compdb(build):
    """object -> full compile command, from ninja's own compdb."""
    try:
        out = subprocess.run(["ninja", "-C", build, "-t", "compdb"],
                             capture_output=True, text=True, check=True).stdout
    except (OSError, subprocess.CalledProcessError) as e:
        sys.exit("scatter_emitted_truth: cannot run ninja -t compdb in %s (%s)"
                 % (build, e))
    try:
        entries = json.loads(out)
    except ValueError:
        sys.exit("scatter_emitted_truth: compdb was not JSON")
    by_out = {}
    for e in entries:
        if "output" in e and "command" in e:
            by_out[e["output"]] = e
    return by_out


def deps_of(cmd, src, cwd):
    """The .cpp files this TU textually includes, per the COMPILER.

    -M replaces compilation with dependency generation, so every conditional is
    resolved the way the real build resolves it. Failures return None and are
    reported rather than silently counted as "emits nothing" -- a preprocess
    error that read as an empty set would be a vacuous pass.
    """
    toks = shlex.split(cmd)
    out = []
    i = 0
    while i < len(toks):
        t = toks[i]
        if t in ("-o", "-MF", "-MT", "-MQ"):
            i += 2
            continue
        if t in ("-c", "-MD", "-MMD", "-M", "-MM"):
            i += 1
            continue
        out.append(t)
        i += 1
    out += ["-M", "-MG", src]
    p = subprocess.run(out, capture_output=True, text=True, cwd=cwd)
    if p.returncode != 0:
        return None
    found = set()
    # A make rule: line continuations and space-separated paths.
    text = p.stdout.replace("\\\n", " ")
    if ":" in text:
        text = text.split(":", 1)[1]
    for tok in text.split():
        if tok.endswith(".cpp"):
            ap = os.path.abspath(os.path.join(cwd, tok))
            if ap != os.path.abspath(src):
                found.add(ap)
    return found


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("project_dir", nargs="?", default=None)
    ap.add_argument("--json", default=None)
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 8)
    a = ap.parse_args()

    here = os.path.dirname(os.path.abspath(__file__))
    repo = a.project_dir or os.path.dirname(here)
    repo = os.path.abspath(repo)
    sys.path.insert(0, os.path.join(repo, "tools"))
    import scatter_audit as SA  # noqa: E402  (needs repo on the path first)

    build = os.path.join(repo, "native", "build")
    if not os.path.isdir(build):
        sys.exit("scatter_emitted_truth: no native/build -- configure first")

    targets = SA.target_sources()
    if not targets:
        sys.exit("scatter_emitted_truth: no targets parsed from build.ninja")
    db = compdb(build)

    # Map each target's sources to their compile commands. The object path CMake
    # uses embeds the target name, which is what lets one compdb serve all 18.
    jobs = []
    for t, S in sorted(targets.items()):
        for src in sorted(S):
            try:
                if not CPP_INC.search(open(src, errors="replace").read()):
                    continue
            except OSError:
                continue
            key = None
            for outp, e in db.items():
                if ("%s.dir" % t) in outp and os.path.abspath(
                        os.path.join(build, e["file"])) == os.path.abspath(src):
                    key = e
                    break
            if key:
                jobs.append((t, src, key["command"], key.get("directory", build)))

    emitted = collections.defaultdict(set)
    failures = []

    def work(j):
        t, src, cmd, d = j
        r = deps_of(cmd, src, d)
        return (t, src, r)

    with ThreadPoolExecutor(max_workers=a.jobs) as ex:
        for t, src, r in ex.map(work, jobs):
            if r is None:
                failures.append(SA.rel(src))
            else:
                emitted[t] |= r

    # Compare against scatter_audit's unconditional-only closure.
    report = {"targets": {}, "disagreements": [], "preprocess_failures": failures}
    dis = []
    for t, S in sorted(targets.items()):
        E_audit, _ = SA.closure(S)
        E_true = emitted.get(t, set())
        only_true = sorted(E_true - E_audit - S)
        only_audit = sorted(E_audit - E_true - S)
        report["targets"][t] = {
            "compiled": len(S),
            "emitted_audit": len(E_audit),
            "emitted_truth": len(E_true),
            "truth_only": [SA.rel(x) for x in only_true],
            "audit_only": [SA.rel(x) for x in only_audit],
        }
        for x in only_true:
            dis.append({"target": t, "file": SA.rel(x), "kind": "emitted-but-audit-missed"})
        for x in only_audit:
            dis.append({"target": t, "file": SA.rel(x), "kind": "audit-claims-emitted-compiler-does-not"})
    report["disagreements"] = dis

    # The corrected headline: guests reaching NO target, conditionals resolved.
    T = set(targets)
    hosts = SA.hosts_map()
    unlinked_audit, unlinked_true = [], []
    for g, hs in hosts.items():
        uh = [x for x, k in hs if k == "uncond"]
        if not uh:
            continue
        na, nt = 0, 0
        for t, S in targets.items():
            E_audit, _ = SA.closure(S)
            E_true = emitted.get(t, set())
            if not (g in S or g in E_audit) and not any(x in S or x in E_audit for x in uh):
                na += 1
            if not (g in S or g in E_true):
                nt += 1
        if na == len(T):
            unlinked_audit.append(SA.rel(g))
        if nt == len(T):
            unlinked_true.append(SA.rel(g))
    report["unlinked_audit"] = sorted(unlinked_audit)
    report["unlinked_truth"] = sorted(unlinked_true)
    report["false_positives"] = sorted(set(unlinked_audit) - set(unlinked_true))

    print("preprocess jobs: %d, failures: %d" % (len(jobs), len(failures)))
    print("unlinked (scatter_audit, uncond-only): %d" % len(unlinked_audit))
    print("unlinked (compiler truth):             %d" % len(unlinked_true))
    print("FALSE POSITIVES (reported unlinked, actually emitted): %d" %
          len(report["false_positives"]))
    for f in report["false_positives"]:
        print("   %s" % f)
    print("instrument disagreements: %d" % len(dis))
    if a.json:
        with open(a.json, "w") as fh:
            json.dump(report, fh, indent=1)
        print("json: %s" % a.json)
    return 1 if dis else 0


if __name__ == "__main__":
    sys.exit(main())
