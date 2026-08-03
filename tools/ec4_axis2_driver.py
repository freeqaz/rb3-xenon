#!/usr/bin/env python3
"""EC-4: drive EC-2's incoming-arg-register witness over an EXPLICIT row list.

tools/ec2_misattribution_scan.py probes every sub-100 row of every unit in a
census BUCKET.  Binary-wide that is tens of thousands of objdiff-cli
invocations.  This driver imports its `probe`/`classify` verbatim -- same
witness, same vacuity guards -- and applies them to a shortlist produced by the
neighbourhood oracle x block-position join, which is the only population where
the second axis is worth paying for.

Reuse, not reimplementation: if the witness is wrong, it is wrong in both.
"""
import argparse
import importlib.util
import json
import pathlib
import sys


def load(root):
    p = pathlib.Path(root) / "tools/ec2_misattribution_scan.py"
    spec = importlib.util.spec_from_file_location("ec2scan", p)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=".")
    ap.add_argument("--rows", required=True, help="JSON list with unit+sym keys")
    ap.add_argument("--out", required=True)
    a = ap.parse_args()

    root = pathlib.Path(a.root).resolve()
    ec2 = load(root)
    rows = json.loads(pathlib.Path(a.rows).read_text())
    out = []
    for i, r in enumerate(rows):
        d = ec2.probe(root, r["unit"], r["sym"])
        if d is None:
            rec = dict(r, err="probe_failed")
        else:
            rec = dict(r)
            rec.update(ec2.classify(d))
        out.append(rec)
        print(f"[{i+1}/{len(rows)}] {r['unit'][:34]:34s} {r['sym'][:40]:40s} "
              f"{rec.get('flags', rec.get('err'))}", file=sys.stderr)
    pathlib.Path(a.out).write_text(json.dumps(out, indent=1))

    import collections
    fc = collections.Counter()
    for r in out:
        for f in r.get("flags", []):
            fc[f] += 1
        if "err" in r:
            fc["probe_failed"] += 1
    print("\n=== flag counts over the shortlist ===")
    for k, v in fc.most_common():
        print(f"  {k:28s} {v:4d} / {len(out)}")
    print(f"wrote {a.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
