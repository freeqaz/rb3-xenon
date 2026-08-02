#!/usr/bin/env python3
"""Guard: `config/<title>/symbols.txt` must be at jeff's SPLIT FIXPOINT.

WHY THIS EXISTS
---------------
`config/<title>/symbols.txt` is BOTH an input to dtk's SPLIT step and an output
of it.  `build/<title>/dep` declares it as a discovered dependency of
`build/<title>/config.json`, and dtk REWRITES the same file as a side effect of
splitting.  So it is a FEEDBACK file: this run's output is next run's input, and
"correct" means FIXED POINT -- splitting a tree whose symbols.txt is already the
fixpoint must reproduce it byte-for-byte.

When the committed file is NOT the fixpoint, dtk is carving function boundaries
that it would itself immediately revise.  That costs real, free match%: an
over-fragmented function is split into a head plus an ~8-byte tail row, and
neither piece can ever equal the retail function, so the row scores 0 and its
bytes never enter `matched_code`.

MEASURED PRECEDENT (this is not hypothetical -- it has paid twice):
  * 954c01f8 (2026-07-30) re-carved to the fixpoint.
  * a08f0fd2 (2026-08-02) had to do it AGAIN: the tree had drifted back off the
    fixpoint in three days WITH THE SAME jeff BINARY.  Re-carving 24 rows into
    11 was worth **+2 matched / +292 matched_code bytes / +0.002792pp**, and
    +0 masked_equal (i.e. real matching, not disclosure credit).

WHY IT RECURS
-------------
Two inputs feed jeff's merge derivation, and ordinary landings edit both:
  * `config/<title>/splits.txt` -- when a span is EXTENDED, a fragmented tail
    row that used to sit outside the pin comes inside and becomes mergeable.
  * `scripts/target_symbol_map.json` -- via JEFF_MERGE_PROTECT.  A row that
    LOSES its map entry is unprotected and becomes mergeable.  One unit
    (ViewSetting) drifted from this input ALONE, with byte-identical spans.
(The jeff binary itself is a third, standing hazard: rebuilding ../jeff silently
changes split output, which is why CLAUDE.md forbids casual rebuilds.)

⚠ MEASURED REFINEMENT (lane DB-4, 2026-08-02) -- the drift is NOT per-landing.
DB-4 predicted that 20ed64f7, which edited BOTH splits.txt and the map, would
have knocked the tree off the fixpoint again.  IT DID NOT: a forced re-split at
9c8e4f2c reproduced symbols.txt byte-identically.  The trigger is specific
(a span extension that SWALLOWS a fragmented tail, or a map row being REMOVED),
not "any splits/map edit".  So run this guard to find out; do not assume.

WHAT THIS GUARD DOES
--------------------
Restores the committed symbols.txt, FORCES a re-split, and asserts the file
comes back byte-identical.  Any difference means the tree is off jeff's fixpoint
with free match% on the table, and the fix is a deliberate re-carve landed as
its own reviewable patch (see --emit-patch).

*** THE POINT OF THE FORCED SPLIT IS ANTI-VACUITY. ***
The naive check -- build, then `git diff --quiet -- config/<title>/symbols.txt`
-- CANNOT FAIL when the build didn't actually split.  Lane DA-1 hit exactly this:
`scripts/setup_worktree.sh`'s prime runs `ninja config.json`, dtk's output was
already current in the reflinked warm build dir, SPLIT NEVER RAN, and the clean
worktree read as a PASS.  DB-4 reproduced it: the first settle build in a fresh
warm worktree does 395 work edges with **SPLIT=0**.  So a clean diff is only
evidence if SPLIT demonstrably ran, and this guard FAILS LOUDLY when it did not
rather than reporting the reassuring answer.

The other vacuity this guards against is DA-1's own near-miss: a stray `cd` left
both `sha256sum` calls failing, and `empty == empty` printed "FIXPOINT
CONFIRMED".  Hence the explicit non-empty / plausible-shape assertions on BOTH
reads, which is why this script compares CONTENT it has proven it actually read.

PROVE IT CAN FAIL
-----------------
    python3 tools/symbols_fixpoint_guard.py --self-break

re-fragments N function rows (the exact inverse of the merge jeff performs: a
byte-conserving head + ~8-byte tail) and requires the guard to REPORT DRIFT.
If the guard still says "on fixpoint", the guard is vacuous and --self-break
exits 1.

USAGE
-----
    python3 tools/symbols_fixpoint_guard.py -v          # is the tree on fixpoint?
    python3 tools/symbols_fixpoint_guard.py --self-break
    python3 tools/symbols_fixpoint_guard.py --emit-patch ~/tmp/recarve.patch

⚠ SHARED-TREE SAFETY: this forces a split, which rewrites build state other
agents depend on, so it REFUSES to run in the shared main repo.  Use a worktree
(scripts/setup_worktree.sh), or pass --ci in a disposable CI container.
The working file is always restored to the committed bytes on exit, and is
written back ONLY if it actually differs (an unconditional write would bump the
mtime and re-dirty the SPLIT edge for the next build).
"""

import argparse
import hashlib
import os
import re
import subprocess
import sys
import time
from pathlib import Path

DEFAULT_TITLE = "45410914"
MIN_PLAUSIBLE_LINES = 1000   # real file is ~226,000 lines; anything tiny is a read failure
SELF_BREAK_ROWS = 12         # how many rows --self-break re-fragments
TAIL_BYTES = 8               # size of the synthetic tail (matches the real defect: ~8-byte tails)

FN_ROW = re.compile(
    r"^(?P<name>fn_(?P<addr>[0-9A-Fa-f]{8}))\s*=\s*\.text:0x(?P<addr2>[0-9A-Fa-f]{8});"
    r"\s*//\s*type:function\s+size:0x(?P<size>[0-9A-Fa-f]+)\s*$"
)
SPLIT_EDGE = re.compile(r"\]\s+SPLIT\b")


class Fail(Exception):
    """A guard failure or a refusal. Always loud, never a silent pass."""


def run(cmd, cwd, capture=True, timeout=1800):
    p = subprocess.run(cmd, cwd=str(cwd), text=True, timeout=timeout,
                       stdout=subprocess.PIPE if capture else None,
                       stderr=subprocess.STDOUT if capture else None)
    return p.returncode, (p.stdout or "")


def git(root, *args, check=True):
    rc, out = run(["git", "-C", str(root)] + list(args), root)
    if check and rc != 0:
        raise Fail(f"git {' '.join(args)} failed rc={rc}: {out.strip()[:400]}")
    return rc, out


def sha256(b):
    return hashlib.sha256(b).hexdigest()


def assert_plausible(data, where):
    """DA-1's vacuity trap: empty == empty reads as CONFIRMED. Refuse to compare
    anything we have not proven we actually read."""
    if not data:
        raise Fail(f"{where}: symbols.txt is EMPTY or unreadable. REFUSING to "
                   "compare -- an empty-vs-empty comparison is the exact "
                   "false-confirmation DA-1 nearly shipped.")
    text = data.decode("utf-8", "replace")
    n = text.count("\n")
    if n < MIN_PLAUSIBLE_LINES:
        raise Fail(f"{where}: symbols.txt has only {n} lines (< {MIN_PLAUSIBLE_LINES}). "
                   "That is not the real file; REFUSING to compare.")
    if "type:function" not in text:
        raise Fail(f"{where}: symbols.txt contains no 'type:function' row. "
                   "Wrong file or truncated; REFUSING to compare.")
    return text


def parse_text_spans(splits_path):
    """Pinned .text ranges from splits.txt (half-open [start,end))."""
    spans = []
    for line in splits_path.read_text(errors="replace").splitlines():
        s = line.strip()
        if not s.startswith(".text"):
            continue
        m = re.search(r"start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)", s)
        if m:
            spans.append((int(m.group(1), 16), int(m.group(2), 16)))
    spans.sort()
    return spans


def in_span(addr, spans):
    lo, hi = 0, len(spans) - 1
    while lo <= hi:
        mid = (lo + hi) // 2
        s, e = spans[mid]
        if addr < s:
            hi = mid - 1
        elif addr >= e:
            lo = mid + 1
        else:
            return True
    return False


def perturb_refragment(text, spans, want):
    """Inverse of jeff's merge: split `want` function rows into a byte-conserving
    head + TAIL_BYTES tail.  This reproduces the REAL defect class (an ~8-byte
    tail carved off its predecessor), so jeff should merge them back.

    Deterministic: walks rows in file order and takes the first eligible ones.
    Only touches rows inside a pinned .text span, since unpinned rows are not
    emitted and a merge there would not show up.
    """
    out, made = [], []
    for line in text.splitlines(keepends=True):
        if len(made) < want:
            m = FN_ROW.match(line.rstrip("\n"))
            if m:
                addr = int(m.group("addr"), 16)
                size = int(m.group("size"), 16)
                # need room for a real head plus the tail, and 4-byte alignment
                if size >= TAIL_BYTES + 16 and size % 4 == 0 and in_span(addr, spans):
                    head = size - TAIL_BYTES
                    tail_addr = addr + head
                    nl = "\n" if line.endswith("\n") else ""
                    out.append(f"fn_{addr:08X} = .text:0x{addr:08X}; "
                               f"// type:function size:0x{head:X}{nl}")
                    out.append(f"fn_{tail_addr:08X} = .text:0x{tail_addr:08X}; "
                               f"// type:function size:0x{TAIL_BYTES:X}{nl}")
                    made.append((addr, size, head, tail_addr))
                    continue
        out.append(line)
    return "".join(out), made


def force_split(root, title, ninja, full_build, logdir, tag):
    """rm the renamer stamp + touch config.yml, then build.  Both are required:
    the stamp gates the renamer, config.yml is the SPLIT edge's declared input."""
    stamp = root / f"build/{title}/target_symbol_renames.stamp"
    stamp.unlink(missing_ok=True)
    (root / f"config/{title}/config.yml").touch()
    target = [] if full_build else [f"build/{title}/config.json"]
    t0 = time.time()
    rc, out = run([ninja] + target, root)
    dt = time.time() - t0
    if logdir:
        (Path(logdir) / f"symbols_fixpoint_{tag}.log").write_text(out)
    return rc, out, dt


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-v", "--verbose", action="store_true")
    ap.add_argument("--version", default=DEFAULT_TITLE, help="title id (default %(default)s)")
    ap.add_argument("--self-break", action="store_true",
                    help="re-fragment rows first; the guard MUST then report drift, "
                         "otherwise it is vacuous and this exits 1")
    ap.add_argument("--emit-patch", metavar="PATH",
                    help="on drift, write the re-carve as a git diff for review "
                         "(land it as its OWN patch -- see the module docstring)")
    ap.add_argument("--full-build", action="store_true",
                    help="build everything instead of just the split target "
                         "(slower; the targeted split is ~10s and is sufficient)")
    ap.add_argument("--ci", action="store_true",
                    help="allow running outside a linked worktree (disposable CI container); "
                         "also defaults --ninja to plain `ninja`")
    ap.add_argument("--ninja", metavar="BIN",
                    help="ninja binary to use. Default: tools/ninja-locked locally (it "
                         "serialises against concurrent agents), but plain `ninja` under "
                         "--ci -- ninja-locked shells out to `configure.py progress` on a "
                         "no-op build, and configure.py's correct arguments are "
                         "repo-state-dependent, so letting it run bare in CI risks "
                         "regenerating build.ninja without --binutils/--compilers")
    ap.add_argument("--allow-main", action="store_true",
                    help="override the shared-main refusal (you had better be sure)")
    ap.add_argument("--logdir", default=os.path.expanduser("~/tmp"),
                    help="where to tee build logs (default %(default)s)")
    args = ap.parse_args()

    def say(*a):
        if args.verbose:
            print(*a, flush=True)

    here = Path(__file__).resolve().parent
    rc, out = run(["git", "-C", str(here), "rev-parse", "--show-toplevel"], here)
    if rc != 0:
        raise Fail("not inside a git repository")
    root = Path(out.strip())
    title = args.version
    rel = f"config/{title}/symbols.txt"
    wfile = root / rel
    say(f"[guard] repo root {root}")
    say(f"[guard] target    {rel}")

    # ---- shared-tree safety -------------------------------------------------
    _, gd = git(root, "rev-parse", "--git-dir")
    _, gcd = git(root, "rev-parse", "--git-common-dir")
    is_main = (root / gd.strip()).resolve() == (root / gcd.strip()).resolve()
    if is_main and not (args.ci or args.allow_main):
        raise Fail(
            f"{root} is the MAIN repo, not a linked worktree. This guard FORCES A "
            "SPLIT, which rewrites build state concurrent agents depend on. Use "
            "scripts/setup_worktree.sh <path> <branch>, or pass --ci in a "
            "disposable container.")

    # ---- read the committed bytes; restore the working file -----------------
    if not wfile.is_file():
        raise Fail(f"{rel} does not exist")
    rc, _ = run(["git", "-C", str(root), "cat-file", "-e", f"HEAD:{rel}"], root)
    if rc != 0:
        raise Fail(f"{rel} is not tracked at HEAD; nothing to be a fixpoint OF")
    committed = subprocess.run(["git", "-C", str(root), "show", f"HEAD:{rel}"],
                               stdout=subprocess.PIPE, check=True).stdout
    assert_plausible(committed, "committed HEAD blob")
    on_disk = wfile.read_bytes()
    if on_disk != committed:
        say(f"[guard] working file differs from HEAD ({len(on_disk)} vs "
            f"{len(committed)} bytes) -- restoring committed bytes before the "
            "split (drift is expected state, but a split MUST start from the "
            "committed file or the comparison is against the wrong baseline)")
        wfile.write_bytes(committed)

    # ---- optional sabotage --------------------------------------------------
    perturbed = []
    if args.self_break:
        spans = parse_text_spans(root / f"config/{title}/splits.txt")
        text = committed.decode("utf-8", "replace")
        newtext, perturbed = perturb_refragment(text, spans, SELF_BREAK_ROWS)
        if len(perturbed) < SELF_BREAK_ROWS:
            raise Fail(f"--self-break could only re-fragment {len(perturbed)} of "
                       f"{SELF_BREAK_ROWS} rows; the perturbation itself is broken, "
                       "so the vacuity test would be meaningless")
        wfile.write_bytes(newtext.encode())
        say(f"[guard] --self-break: re-fragmented {len(perturbed)} rows into "
            f"head+{TAIL_BYTES}B tail pairs, e.g. "
            f"fn_{perturbed[0][0]:08X} 0x{perturbed[0][1]:X} -> "
            f"0x{perturbed[0][2]:X} + fn_{perturbed[0][3]:08X} 0x{TAIL_BYTES:X}")

    before = wfile.read_bytes()
    before_text = assert_plausible(before, "pre-split read")
    sha_b = sha256(before)
    say(f"[guard] pre-split  sha256 {sha_b[:16]}  lines {before_text.count(chr(10))}")

    # ---- force the split ----------------------------------------------------
    if args.ninja:
        ninja = args.ninja
    elif args.ci:
        ninja = "ninja"
    else:
        ninja = str(root / "tools/ninja-locked")
        if not os.access(ninja, os.X_OK):
            ninja = "ninja"
    say(f"[guard] forcing re-split via {ninja} "
        f"({'full build' if args.full_build else f'build/{title}/config.json'})")
    rc, log, dt = force_split(root, title, ninja, args.full_build, args.logdir,
                              "selfbreak" if args.self_break else "check")
    say(f"[guard] build finished rc={rc} in {dt:.1f}s")
    if rc != 0:
        raise Fail(f"the forced build FAILED rc={rc}. A guard cannot certify a "
                   f"fixpoint from a failed split.\n--- tail ---\n{log[-2000:]}")

    # ---- ANTI-VACUITY GATE: SPLIT must actually have run --------------------
    if not SPLIT_EDGE.search(log):
        raise Fail(
            "VACUOUS RUN: the build did NOT execute the SPLIT edge, so symbols.txt "
            "was never re-derived and a clean diff would prove NOTHING. This is "
            "lane DA-1's exact trap (setup_worktree.sh's prime found config.json "
            "current and never split). REFUSING to report a fixpoint.\n"
            f"--- build output ---\n{log[-2000:]}")
    say("[guard] anti-vacuity gate OK: the SPLIT edge ran")

    after = wfile.read_bytes()
    after_text = assert_plausible(after, "post-split read")
    sha_a = sha256(after)
    say(f"[guard] post-split sha256 {sha_a[:16]}  lines {after_text.count(chr(10))}")

    drift = sha_b != sha_a

    # ---- evidence + restore -------------------------------------------------
    numstat = ""
    if drift:
        _, numstat = git(root, "diff", "--numstat", "--", rel, check=False)
        if args.emit_patch and not args.self_break:
            _, patch = git(root, "diff", "--", rel, check=False)
            Path(args.emit_patch).write_text(patch)
            print(f"   re-carve patch written to {args.emit_patch}")
    if wfile.read_bytes() != committed:
        wfile.write_bytes(committed)
        say("[guard] restored committed symbols.txt")

    # ---- verdict ------------------------------------------------------------
    if args.self_break:
        if drift:
            print("PASS (--self-break): the guard DETECTED the injected drift.")
            print(f"   re-fragmented {len(perturbed)} rows; post-split sha "
                  f"{sha_a[:16]} != pre-split {sha_b[:16]}")
            print(f"   numstat: {numstat.strip() or '(rows re-merged)'}")
            return 0
        print("!! --self-break did NOT produce drift => THIS GUARD IS VACUOUS.")
        print("   jeff left the re-fragmented rows alone, so this guard cannot "
              "detect the defect class it exists for. DO NOT TRUST A PASS.")
        return 1

    if drift:
        ins = dele = "?"
        f = numstat.split()
        if len(f) >= 2:
            ins, dele = f[0], f[1]
        print("FAIL: config/%s/symbols.txt is OFF jeff's SPLIT FIXPOINT." % title)
        print(f"   pre-split  sha256 {sha_b}")
        print(f"   post-split sha256 {sha_a}")
        print(f"   drift: +{ins} / -{dele} lines")
        print("   => dtk is carving boundaries it would immediately revise. That is "
              "FREE match% left on the table (a08f0fd2 was worth +2 matched / "
              "+292 B for exactly this).")
        print("   FIX: re-carve to the fixpoint and land it as its OWN reviewable "
              "patch touching nothing but symbols.txt. Re-run with "
              "--emit-patch <path> to generate it. Iterate until this guard passes; "
              "a08f0fd2 converged in ONE iteration.")
        print("   NOTE: this is NOT the forbidden 'commit symbols.txt drift'. The "
              "prohibition is on committing an accidental split output; a measured "
              "re-carve to the fixpoint has precedent in 954c01f8 and a08f0fd2.")
        return 1

    print(f"PASS: config/{title}/symbols.txt is at jeff's split fixpoint "
          f"(sha256 {sha_a[:16]}, {after_text.count(chr(10))} lines; SPLIT ran).")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Fail as e:
        print(f"GUARD FAILURE: {e}", file=sys.stderr)
        sys.exit(2)
