#!/usr/bin/env python3
"""Capture a self-contained reproducer when cl.exe dies from a signal under wibo.

WHY THIS EXISTS
---------------
Lane BR-1 (2026-07-30) root-caused the recurring `wibo`/cl.exe SIGSEGV build
failures.  Every genuine crash record with a recoverable guest instruction
pointer faults inside **c1xx.dll** (the MSVC C++ front end, ImageBase
0x10500000) on a *pointer dereference* -- `mov r32,[reg+disp]` / `cmp [reg],r16`
with small displacements (0x0..0x1c) and si_code SEGV_MAPERR.  That is the
signature of a NULL base pointer: an allocation or lookup returned NULL and the
front end did not check it.

Critically, the crashes are **deterministic for a given input state** (22/22
Geo.obj records shared the exact guest RIP 0x1055b3f5) but the input state is
**ephemeral** -- it lives in an agent's worktree as uncommitted edits and is
gone hours later.  A 5000-run soak of the current tree and a sweep of all 439
worktrees reproduced nothing.  So the population is unfixable not because it is
mysterious but because no reproducer survives.

This wrapper fixes that.  On a signal death it snapshots the *preprocessed*
translation unit (`/E`), which is a single self-contained file that reproduces
the crash forever, independent of worktree state.

SAFETY
------
Pure pass-through.  On success (and on ordinary non-zero compiler exits) it
execs nothing extra, touches no output, and returns the child's exit code
unchanged -- so it cannot perturb object bytes.  Capture work happens only
after a signal death, when the edge has already failed.

USAGE
-----
Wrap an existing compile command (it becomes a no-op unless the child dies from
a signal):

    tools/clcrash_capture.py -- <wibo> <cl.exe> <args...>

Enable fleet-wide by prefixing the msvc rule in configure.py, the same way
`objcache exec` is prefixed.  Disable with WIBO_CLCRASH_CAPTURE=0.

Captures land in $WIBO_CLCRASH_DIR (default ~/tmp/clcrash/<timestamp>-<obj>/).
"""

import os
import shlex
import shutil
import subprocess
import sys
import time

DEFAULT_DIR = os.path.expanduser("~/tmp/clcrash")


def _flag_value(argv, prefix):
    """Return the value of an MSVC-style /Fooutput.obj joined flag."""
    for a in argv:
        if a.startswith(prefix):
            return a[len(prefix):]
    return None


def _strip_output_flags(argv):
    """Drop flags that conflict with /E (compile-to-object, PCH, dep listing)."""
    out = []
    for a in argv:
        low = a.lower()
        if low.startswith("/fo") or low.startswith("-fo"):
            continue
        if low in ("/c", "-c", "/showincludes", "-showincludes"):
            continue
        # PCH use/create would make the .i non-self-contained.
        if low.startswith("/yu") or low.startswith("/yc") or low.startswith("/fp"):
            continue
        # Undocumented /d1 and /d2 back-end/diagnostic switches can themselves be the
        # thing that crashes the front end -- a bare /d1reportSingleClassLayout is a
        # confirmed NULL-string deref in c1xx.dll.  Keeping them here would just crash
        # the preprocess pass too and leave us with an empty .i.
        if low.startswith("/d1") or low.startswith("/d2"):
            continue
        out.append(a)
    return out


def capture(argv, rc, signum, elapsed):
    """Snapshot a self-contained reproducer for a signal-killed compile."""
    obj = _flag_value(argv, "/Fo") or _flag_value(argv, "/fo") or "unknown.obj"
    tag = os.path.basename(obj).replace(".obj", "") or "unknown"
    stamp = time.strftime("%Y%m%d-%H%M%S")
    root = os.environ.get("WIBO_CLCRASH_DIR", DEFAULT_DIR)
    dest = os.path.join(root, f"{stamp}-{tag}-sig{signum}")
    try:
        os.makedirs(dest, exist_ok=True)
    except OSError as e:
        print(f"clcrash_capture: cannot create {dest}: {e}", file=sys.stderr)
        return

    # 1. The exact invocation + environment needed to replay it.
    with open(os.path.join(dest, "command.txt"), "w") as f:
        f.write("# cwd\n")
        f.write(os.getcwd() + "\n\n")
        f.write("# argv\n")
        f.write(" ".join(shlex.quote(a) for a in argv) + "\n\n")
        f.write(f"# exit={rc} signal={signum} elapsed={elapsed:.2f}s\n\n")
        f.write("# wibo-relevant environment\n")
        for k, v in sorted(os.environ.items()):
            if k.startswith(("WIBO_", "TMP", "TEMP", "INCLUDE", "LIB", "PATH")):
                f.write(f"{k}={v}\n")

    # 2. The preprocessed TU: a single file that reproduces the fault forever,
    #    with no dependency on the (ephemeral) worktree header state.
    pre_argv = _strip_output_flags(argv) + ["/E"]
    ipath = os.path.join(dest, f"{tag}.i")
    try:
        with open(ipath, "wb") as out, open(os.path.join(dest, "preprocess.log"), "wb") as errf:
            p = subprocess.run(pre_argv, stdout=out, stderr=errf, timeout=300)
        note = f"/E exit={p.returncode}"
    except Exception as e:  # noqa: BLE001 - best-effort forensics, never re-raise
        note = f"/E failed: {e}"

    size = os.path.getsize(ipath) if os.path.exists(ipath) else 0
    with open(os.path.join(dest, "README.txt"), "w") as f:
        f.write(
            "Self-contained reproducer for a cl.exe signal death under wibo.\n\n"
            f"{note}, {size} bytes of preprocessed source.\n\n"
            "Replay (the .i needs no include paths).  NOTE the `/Tp` -- cl.exe does\n"
            "not infer C++ from a .i extension and will report D8003 without it:\n"
            f"  <wibo> <cl.exe> /nologo /c /O1 /Fo/tmp/x.obj /Tp {tag}.i\n\n"
            "If it still faults, get the guest state with:\n"
            "  gdb -batch -ex 'handle SIGSEGV stop nopass' -ex run \\\n"
            "      -ex 'info registers rip rsp' \\\n"
            "      -ex 'p $_siginfo._sifields._sigfault.si_addr' --args <wibo> <cl.exe> ...\n\n"
            "Guest RIP in 0x10500000..0x10684000 is c1xx.dll (front end);\n"
            "RVA = rip - 0x10500000.  Disassemble c1xx.dll at that RVA to see\n"
            "whether the fault is a stack push or a pointer dereference.\n"
        )

    # Also snapshot the primary source, which is cheap and aids triage.
    src = next((a for a in argv if a.lower().endswith((".cpp", ".c", ".cc", ".cxx"))), None)
    if src and os.path.exists(src):
        try:
            shutil.copy2(src, os.path.join(dest, os.path.basename(src)))
        except OSError:
            pass

    print(f"clcrash_capture: signal {signum} in {tag}; reproducer saved to {dest}", file=sys.stderr)


def main(argv):
    if "--" in argv:
        child = argv[argv.index("--") + 1:]
    else:
        child = argv[1:]
    if not child:
        print(__doc__, file=sys.stderr)
        return 2

    start = time.time()
    p = subprocess.run(child)
    rc = p.returncode
    elapsed = time.time() - start

    # Signal death shows up as a negative returncode from subprocess, or as
    # 128+N if an intermediate shell already translated it.
    signum = 0
    if rc < 0:
        signum = -rc
    elif rc > 128 and rc < 160:
        signum = rc - 128

    enabled = os.environ.get("WIBO_CLCRASH_CAPTURE", "1") != "0"
    if signum and enabled:
        try:
            capture(child, rc, signum, elapsed)
        except Exception as e:  # noqa: BLE001 - forensics must never mask the real failure
            print(f"clcrash_capture: capture failed: {e}", file=sys.stderr)

    # Propagate faithfully so ninja/objcache still see the failure.
    return 128 + signum if signum else rc


if __name__ == "__main__":
    sys.exit(main(sys.argv))
