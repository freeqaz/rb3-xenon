#!/usr/bin/env python3
"""Whole-image TRUE-COPY census over retail bytes -- the control that tells a
genuine unfolded copy apart from a mere SHAPE TWIN.

WHY THIS EXISTS (lane W16-EP, 2026-09-16).  W16-EP ran a whole-image census
that masked every word by INSTRUCTION FORM -- it called
`fold_thunk_gate.mask_word(w)` with one argument, taking the `relocated=True`
default -- and therefore masked the low 16 bits of every opcode in `IMM16_OPS`.
`li r3,N` is `addi` (opcode 14), so the **per-`T` node size** was masked away and
eleven *different* `list<T>::erase` bodies collapsed into one "class".  The lane
reported "eleven byte-identical 84-byte bodies ... retail did NOT fold them" and
used it to REFUSE two alias pairs.  Raw retail bytes refute it:

    822b1e60 vs 82447458, 84 B:  [7] 38600054 != 38600018   (li r3,84 vs li r3,24)
    mask_word() maps BOTH to 38600000.

⇒ A non-branch immediate differs, so `/OPT:ICF` can never fold those two
regardless of `bl` targets.  They are eleven different functions sharing a
shape.  W16-EN reached the same conclusion independently (11 extents, 11
distinct masked forms).

THE RULE THIS TOOL IMPLEMENTS.  In a LINKED image, a non-relocated literal is
real content, not a link-patched field.  Only the branch DISPLACEMENT is
position-dependent.  So:

  * mask ONLY the displacement of op 18 (`b`/`bl`) and op 16 (`bc`);
  * compare every other word as a FULL 32-bit value -- no imm16 masking;
  * additionally require the RESOLVED branch destinations to be equal.

⚠ ANTI-VACUITY.  A comparator that can only ever answer "1" would have
"confirmed" W16-EP's correction just as readily as it refutes its claim, so the
answer is worthless until the instrument is shown to report a LARGE true count
as well.  `--selftest` requires BOTH directions and exits non-zero otherwise.
"""
import argparse
import collections
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "scripts"))

from wrong_callee_triage import Image, load_sizes  # noqa: E402
from fold_thunk_gate import branch_target  # noqa: E402


def strict_key(img, va, n):
    """(form, destinations) -- the TRUE-COPY identity of the body at `va`.

    Two bodies share a key iff a linker could legally fold them: identical in
    every non-relocated word, and branching to the same places.
    """
    off = img.off(va)
    if off is None or n < 4 or n % 4 or off + n > len(img.data):
        return None
    ws = struct.unpack_from(">%dI" % (n // 4), img.data, off)
    form, dests = [], []
    for i, w in enumerate(ws):
        op = w >> 26
        if op == 18:
            form.append(w & 0xFC000003)
            dests.append(branch_target(w, va + 4 * i))
        elif op == 16:
            form.append(w & 0xFFFF0003)
            dests.append(branch_target(w, va + 4 * i))
        else:
            form.append(w)          # FULL word -- the node-size immediate lives here
    return (tuple(form), tuple(dests))


def shape_key(img, va, n):
    """The DEFECTIVE comparator, kept as the negative control: additionally
    masks every imm16 field, which is what collapsed the eleven."""
    IMM16 = {14, 15, 24, 25, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54}
    off = img.off(va)
    if off is None or n < 4 or n % 4 or off + n > len(img.data):
        return None
    ws = struct.unpack_from(">%dI" % (n // 4), img.data, off)
    out = []
    for w in ws:
        op = w >> 26
        if op == 18:
            out.append(w & 0xFC000003)
        elif op == 16:
            out.append(w & 0xFFFF0003)
        elif op in IMM16:
            out.append(w & 0xFFFF0000)
        else:
            out.append(w)
    return tuple(out)


def census(img, sizes, keyfn, lo=4, hi=4096):
    classes = collections.defaultdict(list)
    for va, n in sizes.items():
        if not n or n % 4 or not (lo <= n <= hi):
            continue
        k = keyfn(img, va, n)
        if k is not None:
            classes[(n, k)].append(va)
    return classes


def load(project_dir):
    root = Path(project_dir)
    img = Image(root / "orig" / "45410914" / "band.exe")
    return img, load_sizes()


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--project-dir", default=str(ROOT))
    ap.add_argument("--addr", action="append", default=[],
                    help="report the true-copy count for this VA (hex), repeatable")
    ap.add_argument("--top", type=int, default=8, help="show N largest classes")
    ap.add_argument("--selftest", action="store_true",
                    help="require the comparator to discriminate in BOTH directions")
    args = ap.parse_args()

    img, sizes = load(args.project_dir)
    true_cls = census(img, sizes, strict_key)
    biggest = max((len(v) for v in true_cls.values()), default=0)
    multi = sum(1 for v in true_cls.values() if len(v) >= 2)

    print("TRUE-COPY census: %d classes, %d with >=2 members, largest=%d"
          % (len(true_cls), multi, biggest))
    for (n, _k), vas in sorted(true_cls.items(), key=lambda kv: -len(kv[1]))[:args.top]:
        print("   size=%-5d members=%-4d  first=%08x" % (n, len(vas), min(vas)))

    index = {}
    for (n, k), vas in true_cls.items():
        for va in vas:
            index[va] = len(vas)
    for a in args.addr:
        va = int(a, 16)
        print("   %08x  size=%s  true copies=%s"
              % (va, sizes.get(va), index.get(va, "n/a")))

    if args.selftest:
        ok = True
        # (a) it must be able to report MANY -- else "1" is vacuous.
        if biggest < 50:
            print("SELFTEST FAIL: largest true-copy class is %d; a comparator that "
                  "cannot report a large count cannot certify a small one" % biggest)
            ok = False
        # (b) it must report 1 for a body the defective comparator pools.
        shape_cls = census(img, sizes, shape_key, lo=84, hi=84)
        pooled = max((len(v) for v in shape_cls.values()), default=0)
        t84 = {k: v for k, v in true_cls.items() if k[0] == 84}
        t84max = max((len(v) for v in t84.values()), default=0)
        print("SELFTEST: 84-byte stratum -- shape-twin largest class=%d, "
              "true-copy largest class=%d" % (pooled, t84max))
        if not (pooled > t84max):
            print("SELFTEST FAIL: the strict comparator did not split any shape "
                  "class; it is not discriminating on this stratum")
            ok = False
        print("SELFTEST %s" % ("PASS" if ok else "FAIL"))
        return 0 if ok else 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
