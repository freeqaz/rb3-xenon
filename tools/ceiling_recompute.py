#!/usr/bin/env python3
"""Recompute the reachable ceiling (CAMPAIGN_STATE_2026-08-17 S3 method).

  ceiling = PAIRABLE bytes (unit has a base obj) - map-scaffold shells

SCAFFOLD THRESHOLD.  The published rule is "base obj defines <= 2 symbols".
That is a count of **named*/*function* symbols; this script counts every
DEFINED symbol (sclass 2/3 with a real section), which additionally sees the
per-COMDAT SECTION symbols MSVC emits under /Gy.  The same 105 units therefore
land at <= 6 here.  Do not "fix" this to <= 2 -- that selects ZERO units and
silently INFLATES the ceiling by 180,196 B (61.121%% -> 62.867%%), which is the
vacuous-0 failure lane W3-IDENT hit and correctly declined to publish.

The cliff is printed at thr 4/5/6/7 on every run so it stays visibly a cliff
(0 / 0 / 105 / 105) and never becomes a fitted cut.

Self-validates the scaffold rule against the published 105 units / 914 rows /
180,196 B before the number is trusted -- an unvalidated scaffold rule silently
INFLATES the ceiling (lane W3-IDENT hit exactly that, incl. a vacuous 0 B).
"""
import gzip, json, struct, sys, os

def coff_symcount(path):
    """(n_defined_symbols, n_text_functions) for an MSVC PE/COFF object."""
    try:
        d = open(path, "rb").read()
    except OSError:
        return None
    if len(d) < 20:
        return (0, 0)
    mach, nsec, ts, psym, nsym, opt, chars = struct.unpack_from("<HHIIIHH", d, 0)
    if psym == 0 or nsym == 0:
        return (0, 0)
    secs = []
    off = 20 + opt
    for i in range(nsec):
        try:
            name, vsize, vaddr, size, praw, prel, pln, nrel, nln, sc = struct.unpack_from(
                "<8sIIIIIIHHI", d, off + 40 * i)
        except struct.error:
            return (0, 0)
        secs.append(name.rstrip(b"\0").decode("latin1"))
    ndef = ntext = 0
    i = 0
    while i < nsym:
        base = psym + 18 * i
        if base + 18 > len(d):
            break
        val, secnum, typ, sclass, naux = struct.unpack_from("<IhHBB", d, base + 8)
        if secnum > 0 and sclass in (2, 3):
            ndef += 1
            if secnum <= len(secs) and secs[secnum - 1].startswith(".text") and typ == 0x20:
                ntext += 1
        i += 1 + naux
    return (ndef, ntext)

def load(p):
    op = gzip.open if p.endswith(".gz") else open
    with op(p, "rt") as fh:
        return json.load(fh)

def main(report_path, objdiff_path, root, label):
    r = load(report_path)
    units = {u["name"]: u for u in json.load(open(objdiff_path))["units"]}
    tc = int(r["measures"]["total_code"])
    tf = int(r["measures"]["total_functions"])
    mc = int(r["measures"]["matched_code"])

    rows_seen = bytes_seen = 0
    pair_b = pair_r = pair_u = 0
    per_unit = []
    for u in r["units"]:
        fns = u.get("functions") or []
        nb = sum(int(f.get("size", 0)) for f in fns)
        rows_seen += len(fns); bytes_seen += nb
        od = units.get(u["name"], {})
        bp = od.get("base_path")
        if bp:
            pair_u += 1; pair_r += len(fns); pair_b += nb
            per_unit.append((u["name"], bp, len(fns), nb))
    assert rows_seen == tf and bytes_seen == tc, \
        "JOIN DROPPED ROWS: %d/%d rows %d/%d bytes" % (rows_seen, tf, bytes_seen, tc)

    # scaffold rule, with the threshold cliff printed so it is not a fitted cut
    counts = {}
    for name, bp, nr, nb in per_unit:
        p = bp if os.path.isabs(bp) else os.path.join(root, bp)
        counts[name] = coff_symcount(p)
    for thr in (4, 5, 6, 7):
        su = [(n, nr, nb) for (n, bp, nr, nb) in per_unit
              if counts[n] and counts[n][0] <= thr]
        print("  scaffold thr<=%d : %4d units, %5d rows, %8d B"
              % (thr, len(su), sum(x[1] for x in su), sum(x[2] for x in su)))
    scaff = [(n, nr, nb) for (n, bp, nr, nb) in per_unit if counts[n] and counts[n][0] <= 6]
    sb = sum(x[2] for x in scaff); sr = sum(x[1] for x in scaff)
    ceil = pair_b - sb
    print("\n[%s]" % label)
    print("  total_code            %10d" % tc)
    print("  PAIRABLE              %10d = %.3f%%  (%d units, %d rows)" % (pair_b, 100.0*pair_b/tc, pair_u, pair_r))
    print("  - scaffold shells     %10d   (%d units, %d rows)" % (sb, len(scaff), sr))
    print("  = reachable ceiling   %10d = %.3f%%" % (ceil, 100.0*ceil/tc))
    print("  matched_code          %10d = %.3f%% of total_code = %.2f%% of ceiling"
          % (mc, 100.0*mc/tc, 100.0*mc/ceil))
    print("  gap to ceiling        %10d" % (ceil - mc))
    return dict(total_code=tc, pairable=pair_b, scaffold=sb, ceiling=ceil, matched=mc)

if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4])
