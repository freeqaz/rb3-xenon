#!/usr/bin/env python3
"""stamp_if_changed.py — write a ninja stamp ONLY when its inputs moved.

The problem this exists for
---------------------------
Two gates in this build must run on EVERY invocation, because the things they
catch are mtime-invisible: a hand-edited `icf_aliases.map`, a
`symbol_aliases.json` restored by `cp -a` with an older mtime, a
`target_symbol_map.json` that gained a cross-unit duplicate name. Neither can be
expressed as "input newer than output", so both edges carry the `always` phony
and re-run unconditionally. That part is correct and stays.

What was wrong is how they ENDED: `<gate> && touch $out`. `touch` moves the
stamp's mtime on every build, and `map_name_injectivity_checked.stamp` is a
declared implicit input of the REPORT edge (deliberately — lane J3 wired it
there so `ninja build/45410914/report.json`, the target
`scripts/sync_match_percent.py --build` invokes, cannot skip the gate). So an
always-dirty stamp made REPORT always dirty, and rb3-xenon regenerated a 14 MB
`report.json` on every single ninja invocation, forever, with nothing changed.
dc3 never showed this only because its report edge consumes no gate stamp.

Measured cost of the churn (2026-08-13, this tree): `report generate` over 2,285
units is the single longest edge in a no-op build, and it is the ONLY reason a
converged rb3-xenon build did work at all.

The fix, and why it is not "drop the gate"
------------------------------------------
The gate still runs every build and still fails the build. Only the STAMP
becomes content-addressed: this script digests the gate's real inputs and
rewrites the stamp only when that digest changes. Paired with `restat = 1` on
the rule, ninja re-stats the unchanged stamp and CLEANS the downstream report
instead of rebuilding it. Same idiom the `icf_alias_map` and `split` rules in
tools/project.py already use, and for the same reason.

The failure direction matters and is the safe one. If the digest is wrong in the
"changed when it didn't" direction we regenerate a report we did not need — the
status quo. There is no "unchanged when it did" direction available: the digest
is over the file bytes, so any edit to a gate input or to the gate script itself
moves it. A gate that FAILS never reaches this script at all (`&&`), so a failed
build can never leave a stamp claiming the gate passed.

Deliberately NOT `touch`-on-success. A stamp that says "the gate passed at time
T" is the thing that caused this; a stamp that says "the gate passed over
content C" is what a downstream consumer actually needs to know.

Usage:
    stamp_if_changed.py --out STAMP INPUT [INPUT ...]

Paths are recorded as given (relative, from ninja's cwd) so the digest — and the
ninja command string that produces it — stay byte-identical in the main checkout
and in every worktree. That parity is what keeps warm-worktree command-hash
reuse working; see the JEFF_MERGE_PROTECT comment on the `split` rule.
"""

from __future__ import annotations

import argparse
import hashlib
import os
import sys

# Bumped when the digest's meaning changes, so a format change invalidates every
# existing stamp exactly once instead of silently comparing incomparable digests.
FORMAT = "stamp_if_changed/1"


def digest(paths: list[str]) -> str:
    h = hashlib.sha256()
    h.update(FORMAT.encode())
    for p in paths:
        h.update(b"\0")
        h.update(p.encode())
        h.update(b"\0")
        try:
            with open(p, "rb") as fh:
                for chunk in iter(lambda: fh.read(1 << 20), b""):
                    h.update(chunk)
        except FileNotFoundError:
            # A missing input is a real state, distinct from an empty one, and
            # it must move the digest — otherwise deleting a gate input would
            # leave the stamp (and the report) frozen at the last good content.
            h.update(b"<absent>")
    return f"{FORMAT} {h.hexdigest()}\n"


def write_if_changed(out: str, inputs: list[str]) -> bool:
    """Maintain `out` as a digest of `inputs`. True iff the file was written.

    Imported by the gate scripts themselves (`--stamp`) rather than run as a
    second process: two extra interpreter starts cost ~0.06 s on every ninja
    invocation, which is more than the report write this whole change saves.
    The CLI below stays for hand use and for testing.
    """
    want = digest(inputs)
    try:
        with open(out, "r", encoding="utf-8") as fh:
            if fh.read() == want:
                return False  # unchanged: leave the mtime alone, that is the point
    except (FileNotFoundError, UnicodeDecodeError, IsADirectoryError):
        pass

    parent = os.path.dirname(os.path.abspath(out))
    if parent:
        os.makedirs(parent, exist_ok=True)
    with open(out, "w", encoding="utf-8") as fh:
        fh.write(want)
    return True


def add_arguments(ap: argparse.ArgumentParser) -> None:
    """Add the `--stamp` / `--stamp-input` pair to a gate script's parser.

    The input list is passed on the COMMAND LINE rather than inferred inside the
    gate, so `build.ninja` states what the stamp attests to and the digest cannot
    drift out of step with the edge's declared implicit inputs.
    """
    ap.add_argument("--stamp", default=None,
                    help="on success, maintain this stamp as a digest of "
                         "--stamp-input (see tools/stamp_if_changed.py)")
    ap.add_argument("--stamp-input", action="append", default=[],
                    dest="stamp_inputs", metavar="PATH",
                    help="file the --stamp attests to; repeatable")


def apply(args) -> None:
    """Honour `--stamp` from a parser built with `add_arguments`."""
    if getattr(args, "stamp", None):
        write_if_changed(args.stamp, list(getattr(args, "stamp_inputs", [])))


def selftest() -> int:
    """Exercise the write/no-write decision, including the cases that would make
    this silently useless. A stamp helper that always writes is invisible: the
    build just keeps churning and nothing fails."""
    import tempfile

    fails = []

    def check(cond, what):
        if not cond:
            fails.append(what)
        print(f"  {'ok  ' if cond else 'FAIL'}  {what}")

    with tempfile.TemporaryDirectory() as d:
        a = os.path.join(d, "a"); b = os.path.join(d, "b")
        out = os.path.join(d, "sub", "stamp")
        open(a, "w").write("one")
        open(b, "w").write("two")

        check(write_if_changed(out, [a, b]) is True, "first call creates the stamp")
        check(write_if_changed(out, [a, b]) is False,
              "unchanged inputs do NOT rewrite (the whole point)")

        mt = os.stat(out).st_mtime_ns
        write_if_changed(out, [a, b])
        check(os.stat(out).st_mtime_ns == mt, "mtime is untouched on a no-write")

        open(a, "w").write("ONE")
        check(write_if_changed(out, [a, b]) is True, "content change rewrites")
        check(write_if_changed(out, [b, a]) is True,
              "input ORDER is part of the digest")
        check(write_if_changed(out, [b]) is True,
              "dropping an input rewrites")

        write_if_changed(out, [a, b])
        os.remove(a)
        check(write_if_changed(out, [a, b]) is True,
              "a DELETED input rewrites (absent != empty)")

        # The concatenation trap: without a separator, ("ab","c") and ("a","bc")
        # would digest identically and a real edit could read as no-change.
        x = os.path.join(d, "x"); y = os.path.join(d, "y")
        open(x, "w").write("ab"); open(y, "w").write("c")
        s1 = digest([x, y])
        open(x, "w").write("a"); open(y, "w").write("bc")
        check(digest([x, y]) != s1, "content boundaries are separated in the digest")

    print(f"selftest: {'PASS' if not fails else f'FAIL ({len(fails)})'}")
    return 1 if fails else 0


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--out", help="stamp file to maintain")
    ap.add_argument("--selftest", action="store_true",
                    help="exercise the write/no-write decision and exit")
    ap.add_argument("inputs", nargs="*", help="files the stamp attests to")
    args = ap.parse_args(argv)
    if args.selftest:
        return selftest()
    if not args.out or not args.inputs:
        ap.error("--out and at least one input are required")
    write_if_changed(args.out, args.inputs)
    return 0


if __name__ == "__main__":
    sys.exit(main())
