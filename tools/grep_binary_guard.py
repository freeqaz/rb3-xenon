#!/usr/bin/env python3
"""Guard: prove that our binary-scanning methods can still FIND things in binaries.

WHY THIS EXISTS
---------------
`grep` in an agent's Claude shell is NOT /usr/bin/grep.  The shell snapshot
(~/.claude/shell-snapshots/snapshot-zsh-*.sh) defines a shell FUNCTION that
routes grep through **ugrep with `-I` (ignore binary files)**.  Consequence:

    $ grep -c '.?AVUIComponent@@' orig/45410914/band.exe
    $                                   <- no output, no error, exit 1
    $ command grep -ac '.?AVUIComponent@@' orig/45410914/band.exe
    1

The failure is silent and produces ONLY FALSE NEGATIVES -- the shape of a
"decisive negative", the verdict class that closes veins and stops future work.
This has already cost real yield: a claim that "no `$4` form of
`?SetTypeDef@UIComponent@@` exists anywhere" blocked a repairable row; re-testing
in Python found 21 distinct `$4` forms in our own objs.

There are TWO independent false-negative sources, and `-a` defeats both:
  1. the shim's `-I`      -> total suppression (no output, exit 1), files AND stdin
  2. real grep on binary  -> "binary file matches" instead of the matching lines
                             (affects PRINTING modes; `-c`/`-q` are unaffected)

THE RULE: **always pass `-a` when grep may see binary bytes.**

WHAT THIS GUARD ASSERTS
-----------------------
It builds its OWN tiny binary fixture (NUL bytes around a known needle), so it
ALWAYS runs -- it does not depend on orig/45410914/band.exe, which is gitignored
and absent from CI and fresh worktrees.  A guard that skips when the binary is
missing is a guard that cannot fail, which is the very defect it is policing.

PASS  = every method this repo recommends for binary scanning finds the needle.
FAIL  = a recommended method MISSED a needle that is provably present
        (i.e. some scan you run could be silently lying to you).

The "bare grep is binary-blind" probe is reported as a live-hazard WARNING, not
a failure: the shim is the environment's default and an agent cannot remove it.
What must never regress is that the *remedy* works.

USAGE
-----
    python3 tools/grep_binary_guard.py           # normal
    python3 tools/grep_binary_guard.py -v        # show every probe

    # prove it can fail (falsification control, house style):
    python3 tools/grep_binary_guard.py --self-break

Exit 0 = pass, 1 = a recommended method returned a false negative.
"""
import argparse
import glob
import os
import shutil
import subprocess
import sys
import tempfile

NEEDLE = "RB3_GREP_GUARD_NEEDLE_a7f3"
# Bytes that make the file unambiguously "binary" to every heuristic: NULs and
# high bytes on BOTH sides of the needle.
FIXTURE = b"\x00\x01\x02\x03" * 64 + NEEDLE.encode() + b"\x00\xff\xfe" * 64

ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
ap.add_argument("-v", "--verbose", action="store_true")
ap.add_argument("--self-break", action="store_true",
                help="deliberately corrupt the guard's own expectations to prove "
                     "it reports FAIL (falsification control)")
args = ap.parse_args()

results = []   # (level, name, detail)   level in {"PASS","FAIL","WARN","INFO"}


def record(level, name, detail):
    results.append((level, name, detail))
    if args.verbose or level in ("FAIL", "WARN"):
        print(f"  [{level}] {name}: {detail}")


def latest_snapshot():
    snaps = sorted(glob.glob(os.path.expanduser(
        "~/.claude/shell-snapshots/snapshot-zsh-*.sh")), key=os.path.getmtime)
    return snaps[-1] if snaps else None


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)


def in_shim_shell(snapshot, shell_cmd):
    """Run shell_cmd in a zsh that has sourced the Claude snapshot.

    A plain `#!/bin/bash` script or a python subprocess does NOT inherit the
    shim (zsh does not export functions), so the ONLY way a checked-in guard can
    exercise the real hazard is to reconstruct it like this.  Without this, the
    guard would silently test /usr/bin/grep and could never fail on the actual
    bug.
    """
    return run(["zsh", "-c", f"source {snapshot!r} >/dev/null 2>&1; {shell_cmd}"])


def main():
    tmp = tempfile.mkdtemp(prefix="grepguard.")
    try:
        fx = os.path.join(tmp, "fixture.bin")
        needle = NEEDLE
        with open(fx, "wb") as f:
            f.write(FIXTURE)

        if args.self_break:
            # Falsification control: ask for a needle that is genuinely ABSENT.
            # Every method must then "miss" it -- and the guard must say FAIL,
            # proving the PASS path is not vacuous.
            needle = "RB3_GREP_GUARD_NEEDLE_THAT_IS_NOT_PRESENT"
            print("!! --self-break: searching for a needle that is NOT in the "
                  "fixture; the guard MUST report FAIL below.\n")

        # ---- ground truth, computed in pure Python (no grep involved) --------
        raw = open(fx, "rb").read()
        truth = raw.count(needle.encode())
        record("INFO", "fixture", f"{len(raw)} bytes, python says needle x{truth}")
        if truth == 0 and not args.self_break:
            record("FAIL", "fixture", "needle absent from our own fixture (bug in guard)")
            return

        snapshot = latest_snapshot()
        if snapshot and not shutil.which("zsh"):
            record("INFO", "shim probe",
                   "snapshot found but no zsh available; shim path not exercised")
            snapshot = None

        # ---- CHECK 1: the documented remedy `grep -a`, THROUGH the shim ------
        # This is the assertion that must never regress.
        if snapshot:
            r = in_shim_shell(snapshot, f"grep -ac {needle} {fx!r}")
            got = (r.stdout or "").strip()
            ok = got == str(truth)
            record("PASS" if ok else "FAIL", "grep -a through shim",
                   f"expected {truth}, got {got!r} (exit {r.returncode})")

            # ---- CHECK 2: bare grep through the shim -> live-hazard probe ----
            r = in_shim_shell(snapshot, f"grep -c {needle} {fx!r}")
            got = (r.stdout or "").strip()
            if got != str(truth):
                record("WARN", "bare grep through shim",
                       f"BINARY-BLIND (expected {truth}, got {got!r}). The shim is "
                       f"LIVE: bare `grep` on a binary yields FALSE NEGATIVES. "
                       f"Use `grep -a` / `command grep -a` / Python.")
            else:
                record("INFO", "bare grep through shim",
                       "found it -- shim appears inactive in this environment")
        else:
            record("WARN", "shim probe",
                   "no Claude shell snapshot found; could not exercise the shim "
                   "path (checks below still ran against real grep)")

        # ---- CHECK 3: real grep, COUNT mode, no -a (should be fine) ----------
        gp = shutil.which("grep") or "/usr/bin/grep"
        r = run([gp, "-c", needle, fx])
        got = (r.stdout or "").strip()
        ok = got == str(truth)
        record("PASS" if ok else "FAIL", "real grep -c (no -a)",
               f"expected {truth}, got {got!r}")

        # ---- CHECK 4: real grep, PRINTING mode -- the second FN source -------
        # Without -a, real grep prints "binary file matches" instead of lines.
        r = run([gp, "-o", needle, fx])
        printed = (r.stdout or "").strip()
        r2 = run([gp, "-ao", needle, fx])
        printed_a = (r2.stdout or "").strip()
        if printed_a != needle and not args.self_break:
            record("FAIL", "real grep -ao", f"remedy failed: got {printed_a!r}")
        elif args.self_break and printed_a == "":
            record("FAIL", "real grep -ao",
                   "needle not found (expected, --self-break)")
        else:
            record("PASS", "real grep -ao", f"printed {printed_a!r}")
        if printed != needle:
            record("INFO", "real grep -o (no -a)",
                   f"suppressed to {printed!r} -- second false-negative source, "
                   f"independent of the shim")

        # ---- CHECK 5: opportunistic cross-check on the real retail binary ----
        band = os.path.join(os.path.dirname(os.path.dirname(
            os.path.abspath(__file__))), "orig/45410914/band.exe")
        if os.path.exists(band):
            probe = b".?AVUIComponent@@"
            with open(band, "rb") as f:
                n = f.read().count(probe)
            r = run([gp, "-ac", probe.decode().replace(".", r"\."), band])
            got = (r.stdout or "").strip()
            ok = got == str(n)
            record("PASS" if ok else "FAIL", "band.exe cross-check",
                   f"python says {n}, grep -a says {got!r}")
        else:
            record("INFO", "band.exe cross-check",
                   "skipped (gitignored / absent) -- checks 1-4 above still ran")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


main()

fails = [r for r in results if r[0] == "FAIL"]
warns = [r for r in results if r[0] == "WARN"]
print()
if fails:
    print(f"GREP BINARY GUARD: FAIL ({len(fails)} check(s) returned a FALSE NEGATIVE)")
    for _, n, d in fails:
        print(f"  FAIL {n}: {d}")
    print("\nA scan run this way could be silently lying. Do NOT trust any "
          "'decisive negative' produced with the affected method.")
    sys.exit(1)
print(f"GREP BINARY GUARD: PASS ({len(results)} probes, {len(warns)} hazard warning(s))")
for _, n, d in warns:
    print(f"  WARN {n}: {d}")
sys.exit(0)
