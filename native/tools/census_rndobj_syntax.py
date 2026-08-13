#!/usr/bin/env python3
"""Measure how much of src/system/rndobj/ would compile into the native build,
by running -fsyntax-only over every rndobj TU and grouping the failures by root
cause.

Originally written by lane X2/CENSUS to independently re-measure the claim
"72 of 86 rndobj TUs pass -fsyntax-only". Harvested and hardened by lane
HARVEST-TOOLS (2026-08-13).

Flags are NOT invented: they are lifted verbatim from the compile_commands.json
entry for src/system/rndobj/Anim.cpp as built for the rb3-ark target -- i.e. the
ONE rndobj TU already in the native build, so a newly-globbed rndobj TU would get
exactly this command.  We swap -o <obj> -c for -fsyntax-only and substitute the
source path.

⛔ WHY THIS TOOL REFUSES, AND WHY IT CONTROLS ON ITS OWN TEMPLATE
-----------------------------------------------------------------
As originally written this script was VACUOUS in both directions.

*Too little.* A wrong repo root globbed nothing and still printed a clean
result (measured 2026-08-13, before the guards below existed)::

    $ census_rndobj_syntax.py build /nonexistent-root
    [scan] 0 rndobj/*.cpp
    PASS 0 / 0   FAIL 0                          # rc=0

``PASS 0 / 0   FAIL 0`` reads as "nothing failed". An empty population cannot
fail, so silence here means nothing at all -- exactly the class of clean,
decisive negative that closes veins (tools/screen_gate.py documents six caught
in one session).

*Too much.* The opposite failure is subtler and no count-based guard catches
it: if the harvested flag list is wrong -- a stale compile_commands.json, a
missing include dir, the wrong target -- then EVERY TU fails and the tool
reports a catastrophic, entirely fictitious ``PASS 0 / 86``. That number looks
like a finding rather than a broken harness.

The control for it is free and decisive: **the template TU is already in the
native build, so it MUST pass -fsyntax-only under its own flags.** If Anim.cpp
itself fails, the flags are wrong and every other verdict in the run is an
artifact, so the run refuses instead of reporting. This is the MUST-FIRE /
positive-control discipline from tools/screen_gate.py applied to a census: a
harness that cannot demonstrate it works on a known-good input tells you
nothing about the inputs you care about.

Usage: census_rndobj_syntax.py <build-dir> <repo-root> [--target rb3-ark]
Output JSON goes to $OUT (default ~/tmp), never /tmp (RAM-backed tmpfs).
"""
import json, os, re, subprocess, sys, glob, shlex, collections
from concurrent.futures import ThreadPoolExecutor


def die(msg):
    """Refuse loudly. Exit 2 == 'this run proved nothing', never 'clean'."""
    print(f"REFUSED: {msg}", file=sys.stderr)
    sys.exit(2)


def main():
    if len(sys.argv) < 3:
        die("usage: census_rndobj_syntax.py <build-dir> <repo-root> [--target NAME]")
    bd, root = sys.argv[1], sys.argv[2]
    target = sys.argv[sys.argv.index("--target") + 1] if "--target" in sys.argv else "rb3-ark"

    ccpath = os.path.join(bd, "compile_commands.json")
    if not os.path.isfile(ccpath):
        die(f"no compile_commands.json at {ccpath} -- configure the native build first")
    cc = json.load(open(ccpath))

    tmpl = None
    for e in cc:
        if e["file"].endswith("/src/system/rndobj/Anim.cpp") and target in e.get("output", ""):
            tmpl = e
            break
    if tmpl is None:
        die(f"no rndobj/Anim.cpp entry for target {target} in {ccpath}")
    print(f"[flags] copied from: {tmpl['file']}")
    print(f"[flags] output was : {tmpl['output']}")

    args = shlex.split(tmpl["command"])
    # strip -o <x>, -c, -MD/-MT/-MF <x>, and the source path
    out, i = [], 0
    while i < len(args):
        a = args[i]
        if a in ("-o", "-MT", "-MF", "-MQ"):
            i += 2; continue
        if a in ("-c", "-MD", "-MMD"):
            i += 1; continue
        if a.endswith(tmpl["file"]) or a == tmpl["file"]:
            i += 1; continue
        out.append(a); i += 1
    base = out + ["-fsyntax-only", "-ferror-limit=0"]
    print(f"[flags] {len(base)} args; cwd={tmpl['directory']}")

    def run(s):
        p = subprocess.run(base + [s], cwd=tmpl["directory"],
                           capture_output=True, text=True)
        return s, p.returncode, p.stderr

    # ---- CONTROL: the template TU must pass under its own flags -----------
    # It is already compiled by this very build, so a failure here means the
    # harness is broken, NOT that the TU is broken. Without this, a bad flag
    # list reports a confident "PASS 0 / 86".
    _, ctl_rc, ctl_err = run(tmpl["file"])
    if ctl_rc != 0:
        # Match ": fatal error: " too. Matching only ": error: " left this
        # message citing an EMPTY first error when the control was self-broken
        # by stripping -I (clang reports a *fatal* error for a missing header),
        # i.e. a refusal with no evidence for WHY -- indistinguishable from the
        # compiler failing to launch at all.
        first = next((l for l in ctl_err.splitlines()
                      if re.search(r": (fatal )?error: ", l)), "")
        if not first:
            first = (ctl_err.strip().splitlines() or ["<no stderr at all -- "
                     "compiler may not have run>"])[0]
        die("positive control FAILED: the template TU "
            f"{os.path.basename(tmpl['file'])} does not pass -fsyntax-only under "
            f"its own harvested flags, so every verdict in this run would be a "
            f"harness artifact. First error: {first.strip()[:200]}")
    print(f"[control] {os.path.basename(tmpl['file'])} passes under its own flags -- "
          f"flag harvest is sound")

    srcs = sorted(glob.glob(os.path.join(root, "src/system/rndobj/*.cpp")))
    print(f"[scan] {len(srcs)} rndobj/*.cpp\n")

    # ---- GUARD: an empty population cannot fail ---------------------------
    if not srcs:
        die(f"globbed 0 files from {root}/src/system/rndobj/*.cpp -- "
            f"'PASS 0 / 0  FAIL 0' would read as 'nothing failed'. Check the repo root.")

    res = list(ThreadPoolExecutor(16).map(run, srcs))
    ok = [r for r in res if r[1] == 0]
    bad = [r for r in res if r[1] != 0]
    print(f"PASS {len(ok)} / {len(srcs)}   FAIL {len(bad)}\n")

    # root-cause grouping: first error line of each failure
    groups = collections.defaultdict(list)
    detail = {}
    for s, rc, err in bad:
        first = ""
        for l in err.splitlines():
            if re.search(r": (fatal )?error: ", l):
                first = l.split("error:")[-1].strip()
                break
        key = re.sub(r"'[^']*'", "'X'", first)[:90]
        groups[key].append(os.path.basename(s))
        detail[os.path.basename(s)] = first
    for k, v in sorted(groups.items(), key=lambda kv: -len(kv[1])):
        print(f"--- [{len(v)}] {k}")
        for f in sorted(v):
            print(f"      {f}   :: {detail[f][:110]}")

    outdir = os.environ.get("OUT", os.path.expanduser("~/tmp"))
    os.makedirs(outdir, exist_ok=True)
    dest = os.path.join(outdir, "rndobj_syntax.json")
    json.dump({"pass": [os.path.basename(o[0]) for o in ok],
               "fail": {os.path.basename(b[0]): detail[os.path.basename(b[0])] for b in bad},
               "total": len(srcs)},
              open(dest, "w"), indent=1)
    print(f"\n[out] {dest}")


main()
