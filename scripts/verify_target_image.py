#!/usr/bin/env python3
"""Refuse to split a target image that is not the one this tree is written against.

Why this exists
---------------
Everything downstream of `dtk xex split` -- splits.txt, symbols.txt,
target_symbol_map.json, and the source itself -- is written against ONE
specific retail image, but `orig/` is gitignored and nothing in the build ever
checked which image it holds. Two images that matter here have identical
section tables, identical PE timestamps and identical sizes once extracted, so
a swapped image does not fail loudly: it splits cleanly and moves a handful of
rows a few points, which reads as a plausible regression rather than as the
wrong input.

    clean retail TU5      default.xex sha1 d56e7f31...   PE 5f3f667a...  <- target
    RB3 Deluxe (RB3DX)    default.xex sha1 c5a17091...   PE 2fbdbc6b...  <- previous target
    vanilla retail TU0    default.xex sha1 35adb6b4...   (tu0-archive/)

RB3DX is clean TU5 with 53 words patched in place (10 groups;
docs/decomp/W16PT_CLEAN_TU5_RETARGET_2026-10-06.md). Against the RB3DX image,
`DataSet`, `SetDiskError`, `IsDemo`, `AddSongData` and `main` can never reach
100 by any source change, and `ByteGrinder::HvDecrypt` reads 95.5 against the
clean-TU5 source in this tree. This check makes that state impossible to reach
by accident.

What it checks
--------------
Every `<sha1>  <path>` line of config/<v>/build.sha1 (paths repo-root
relative; blank lines and `#` comments ignored). The file has to list at least
one entry -- an empty manifest would pass vacuously.

Exit codes
----------
    0   every listed file exists and hashes to its recorded sha1
    1   a listed file is missing or hashes to something else (nothing written)
    2   the manifest is unreadable, malformed, or empty

`--stamp-out` writes the verified lines to a stamp file, only when its content
changes, so the ninja edge can use restat and stay quiet on an unchanged image.
`--selftest` builds a scratch manifest and requires both a pass and a fail.
"""
import argparse
import hashlib
import os
import sys
import tempfile
from pathlib import Path

DEFAULT_MANIFEST = Path("config") / "45410914" / "build.sha1"

# Identities worth naming in a failure message. Keyed by full sha1 of the XEX.
KNOWN = {
    "d56e7f31101f7851c96342349d6f9daff0223753": "clean retail TU5 v0.0.5.1 (the target)",
    "c5a17091cb44c0119424390a1738d161995e430e":
        "RB3 Deluxe release xex (clean TU5 + 53 patched words) -- the PREVIOUS target",
    "35adb6b4eadab3b0aae354e20ed45781ff0b8fc8": "vanilla retail TU0 (pre-2026-07-15 target)",
}
SWAP_DOC = "docs/decomp/W16PT_CLEAN_TU5_RETARGET_2026-10-06.md"


def sha1_file(path):
    h = hashlib.sha1()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def parse_manifest(path):
    entries = []
    for n, raw in enumerate(Path(path).read_text().splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split(None, 1)
        if len(parts) != 2 or len(parts[0]) != 40:
            raise ValueError(f"{path}:{n}: expected '<sha1>  <path>', got {raw!r}")
        try:
            int(parts[0], 16)
        except ValueError:
            raise ValueError(f"{path}:{n}: not a hex sha1: {parts[0]!r}")
        entries.append((parts[0].lower(), parts[1].strip()))
    if not entries:
        raise ValueError(f"{path}: no entries (an empty manifest would pass vacuously)")
    return entries


def check(manifest, root):
    """Return (problems, verified_lines)."""
    problems, verified = [], []
    for want, rel in parse_manifest(manifest):
        p = Path(root) / rel
        if not p.is_file():
            problems.append(f"{rel}: MISSING (expected sha1 {want})")
            continue
        got = sha1_file(p)
        if got != want:
            msg = (f"{rel}: sha1 {got}\n"
                   f"    expected {want}  [{KNOWN.get(want, 'unrecognised')}]\n"
                   f"    on disk  {got}  [{KNOWN.get(got, 'unrecognised')}]")
            problems.append(msg)
        else:
            verified.append(f"{want}  {rel}")
    return problems, verified


def write_if_changed(path, text):
    p = Path(path)
    if p.is_file() and p.read_text() == text:
        return
    p.parent.mkdir(parents=True, exist_ok=True)
    tmp = p.with_suffix(p.suffix + ".tmp")
    tmp.write_text(text)
    os.replace(tmp, p)


def selftest():
    with tempfile.TemporaryDirectory() as d:
        img = Path(d) / "img.bin"
        img.write_bytes(b"target image")
        good = hashlib.sha1(b"target image").hexdigest()
        man = Path(d) / "build.sha1"
        man.write_text(f"# comment\n{good}  img.bin\n")
        probs, ok = check(man, d)
        assert not probs and ok == [f"{good}  img.bin"], (probs, ok)
        img.write_bytes(b"patched image")  # same path, other bytes
        probs, ok = check(man, d)
        assert len(probs) == 1 and not ok, (probs, ok)
        img.unlink()
        probs, ok = check(man, d)
        assert len(probs) == 1 and "MISSING" in probs[0], probs
        man.write_text("# nothing but comments\n")
        try:
            check(man, d)
        except ValueError:
            pass
        else:
            raise AssertionError("empty manifest was accepted")
    print("verify_target_image selftest: PASS (match passes; changed bytes, "
          "missing file and empty manifest all fail)")
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--manifest", default=str(DEFAULT_MANIFEST))
    ap.add_argument("--root", default=".", help="repo/worktree root (paths resolve here)")
    ap.add_argument("--stamp-out", default=None)
    ap.add_argument("--quiet", action="store_true")
    ap.add_argument("--selftest", action="store_true")
    a = ap.parse_args(argv)
    if a.selftest:
        return selftest()
    try:
        problems, verified = check(Path(a.root) / a.manifest, a.root)
    except (OSError, ValueError) as e:
        print(f"verify_target_image: cannot read manifest: {e}", file=sys.stderr)
        return 2
    if problems:
        print("verify_target_image: FAIL -- the target image is not the one this "
              "tree is written against.", file=sys.stderr)
        for p in problems:
            print("  " + p, file=sys.stderr)
        print(f"  Put the right image in place (steps: {SWAP_DOC}), then re-run "
              "the build.", file=sys.stderr)
        return 1
    if a.stamp_out:
        write_if_changed(a.stamp_out, "\n".join(verified) + "\n")
    if not a.quiet:
        for v in verified:
            print(f"verify_target_image: OK {v}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
