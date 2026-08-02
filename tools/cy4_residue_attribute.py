#!/usr/bin/env python3
"""Lane CY-4 part 4: attribute the STILL-UNKNOWN at-100 residue to a CAUSE.

The brief's central question is not "how many can I flip" but "how much of this
is an instrument gap and how much is ordinary porting backlog". CW-2's UNKNOWN
reason strings already carry the answer, because every one of them is a
statement about which SIDE could not be read:

    B:<status>  T:<status>  loc:<status>

`no_our_body` and `length_differs` are statements about OUR OWN build. Only
`no_pdata_extent`, `T_multiple_addrs` and the deliberately-refused
`unknown_reloc_type_0x02` are statements about retail or the tool.

ATTRIBUTION LATTICE -- ordered, and the residual bucket is EXPLICIT
------------------------------------------------------------------
  MAP_noninjective     T resolves to several addresses, or none
  RETAIL_no_extent     addr(T) has no .pdata extent to read
  TOOL_refused_reloc   an absolute (0x02) relocation the masker REFUSES rather
                       than silently permitting -- a deliberate tool limit
  TOOL_body_too_small  <4 words; below the anti-vacuity floor
  SRC_absent_B         we compile no body for the callee our own code names
  SRC_absent_T         we compile no body for retail's callee
  SRC_imperfect_T      we have T but it does not reproduce retail@addr(T)
                       => our decomp of T is wrong (or the map row is)
  SRC_unverifiable_B   T verified, B compiled, but B matches nothing in retail
  UNATTRIBUTED         explicit. never folded into any of the above.

Then SRC_absent_B is sub-classified against our compiled objs: is the callee's
enclosing scope compiled at all (unported TU) or compiled but missing this one
function (unported FN)? Names the parser cannot scope are reported as
SCOPE_UNPARSED, never guessed.

CONDITIONED NULL for the sub-classifier
---------------------------------------
"Callee absent from our build" is meaningless without knowing the background
rate, and the population must be conditioned or the null flatters the tool (the
lesson from CW-2's unconditioned length null, which decided only 108 rows). The
null here is the set of callees at UNCHARGED (name-AGREEING) call sites in the
SAME at-100 functions -- same callers, same units, same optimisation regime, the
only difference being whether the census charged the slot.

Read-only.
"""

import argparse
import collections
import json
import os
import re
import sys
from pathlib import Path

ROOT = os.environ.get("CY4_ROOT", str(Path(__file__).resolve().parent.parent))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from cy4_weakext_adjudicate import build_index                 # noqa: E402
from icf_site_census import load_unit_objs, index_fns          # noqa: E402

PLACEHOLDER = re.compile(r'^_?(fn|lbl|jumptable|code|data|bss|rdata)_[0-9a-fA-F_]+$')


def forgiven(n):
    return bool(PLACEHOLDER.match(n)) or n.startswith('$')


def scope_of(mangled):
    """Enclosing class/namespace of an MSVC mangled name, or None.

    ?Name@Class@@...            -> Class
    ?Name@Class@NS@@...         -> Class@NS
    ??_GClass@@...              -> Class
    ??Name@Class@@ (ctor/dtor)  -> Class
    Templates keep their argument list; that is fine, we only need a key that is
    STABLE between a definition and a reference, and the full scope string is.
    """
    if not mangled.startswith('?'):
        return None
    body = mangled[1:]
    if body.startswith('?'):
        body = body[1:]
        if body[:2] in ('_G', '_E', '_7', '_R'):
            body = body[2:]
        else:
            i = 0
            while i < len(body) and body[i] not in '@':
                i += 1
            body = body[i:].lstrip('@')
            if not body:
                return None
            return body.split('@@')[0] or None
    parts = body.split('@@')[0]
    seg = parts.split('@')
    if len(seg) < 2:
        return None
    return '@'.join(seg[1:]) or None


def attribute(reason):
    if reason in ('T_multiple_addrs', 'no_addr_for_T'):
        return 'MAP_noninjective'
    m = re.match(r'B:(\S+(?::\S+)?) T:(\S+(?::\S+)?) loc:(\S+)', reason)
    if not m:
        return 'UNATTRIBUTED'
    B, T, L = m.groups()
    if 'no_pdata_extent' in (B, T) or B.endswith('no_pdata_extent'):
        return 'RETAIL_no_extent'
    if 'unknown_reloc_type_0x02' in reason:
        return 'TOOL_refused_reloc'
    if B.endswith('no_our_body'):
        return 'SRC_absent_B'
    if T.endswith('no_our_body'):
        return 'SRC_absent_T'
    if 'mask_ge_half' in reason:
        return 'TOOL_mask_ge_half'
    if B.endswith('length_differs') and T == 'MATCH':
        return 'SRC_unverifiable_B'
    if T.endswith('length_differs') or T == 'DIFF':
        return 'SRC_imperfect_T'
    if L.endswith('body_too_small'):
        return 'TOOL_body_too_small'
    return 'UNATTRIBUTED'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=ROOT)
    ap.add_argument("--adj", required=True)
    ap.add_argument("--sites", required=True)
    ap.add_argument("--report", required=True)
    ap.add_argument("--resolved", required=True)
    a = ap.parse_args()
    root = Path(a.root)

    weak, defined, _p, nobj, _c = build_index(a.root)
    decided_by_cy4 = set()
    for r in json.load(open(a.resolved))["rows"]:
        if r[5] != "UNKNOWN":
            decided_by_cy4.add((r[0], r[1]))

    pairs = json.load(open(a.adj))["pairs"]
    still = []
    for T, B, cls, v, reason, _h, ns in pairs:
        if v != "UNKNOWN":
            continue
        if weak.get(B) == T and B not in defined:
            continue                       # decided BENIGN by the alias channel
        if (T, B) in decided_by_cy4:
            continue                       # decided by the resolved byte channel
        still.append((T, B, cls, reason, ns))

    print("=== STILL-UNKNOWN after CY-4: %d pairs / %d sites ==="
          % (len(still), sum(s[4] for s in still)))
    att = collections.Counter()
    attsites = collections.Counter()
    for T, B, cls, reason, ns in still:
        k = attribute(reason)
        att[k] += 1
        attsites[k] += ns
    tot = sum(att.values())
    print("\n%-22s %6s %8s %8s" % ("attribution", "pairs", "share", "sites"))
    for k, n in att.most_common():
        print("   %-22s %5d  %6.1f%%  %6d" % (k, n, 100.0 * n / tot, attsites[k]))

    OUR = sum(n for k, n in att.items() if k.startswith("SRC_"))
    TOOLM = sum(n for k, n in att.items() if k.startswith(("TOOL_", "RETAIL_", "MAP_")))
    print("\n   OUR-SIDE (source) causes      %5d  (%5.1f%%)"
          % (OUR, 100.0 * OUR / tot))
    print("   TOOL / RETAIL / MAP causes    %5d  (%5.1f%%)"
          % (TOOLM, 100.0 * TOOLM / tot))
    print("   UNATTRIBUTED (explicit)       %5d  (%5.1f%%)"
          % (att['UNATTRIBUTED'], 100.0 * att['UNATTRIBUTED'] / tot))

    # ---- sub-classify SRC_absent_B ---------------------------------------
    scopes_defined = collections.Counter()
    for n in defined:
        s = scope_of(n)
        if s:
            scopes_defined[s] += 1
    sub = collections.Counter()
    examples = collections.defaultdict(list)
    for T, B, cls, reason, ns in still:
        if attribute(reason) != 'SRC_absent_B':
            continue
        if B in weak:
            sub['weak_external_other_default'] += 1
            examples['weak_external_other_default'].append((B, weak[B]))
            continue
        s = scope_of(B)
        if s is None:
            sub['SCOPE_UNPARSED'] += 1
            examples['SCOPE_UNPARSED'].append((B, ''))
        elif scopes_defined.get(s):
            sub['UNPORTED_FN (scope IS compiled)'] += 1
            examples['UNPORTED_FN (scope IS compiled)'].append((B, s))
        else:
            sub['UNPORTED_TU (scope NOT compiled)'] += 1
            examples['UNPORTED_TU (scope NOT compiled)'].append((B, s))
    st = sum(sub.values())
    print("\n=== SRC_absent_B sub-classification (denominator %d pairs) ===" % st)
    for k, n in sub.most_common():
        print("   %-36s %5d  (%5.1f%%)" % (k, n, 100.0 * n / max(1, st)))
    for k in sub:
        print("     e.g. %s: %s" % (k, [e[0][:52] for e in examples[k][:3]]))

    # ---- CONDITIONED NULL -------------------------------------------------
    # ★ CZ-3: key on the FULL unit name -- see the note in
    # at100_adjudicate.load_rep. After f592571a the census emits full unit paths,
    # so the old stem key joined 0/895 and this conditioned null silently drew
    # from an EMPTY population, printing 0/0 rather than refusing.
    rep = json.load(open(a.report))
    pct = {}
    for u in rep["units"]:
        for f in (u.get("functions") or []):
            pct[(u["name"], f["name"])] = f["match_percent_normalized"]
    sites = json.load(open(a.sites))["records"]
    _su = {u for u, _f, _r in sites}
    _ru = {u for u, _f in pct}
    if not (_su & _ru):
        sys.exit("REFUSING: sites.json units do not join report.json units "
                 "(0/%d) -- the f592571a stem-vs-full-name breakage." % len(_su))
    charged_fns = set()
    for unit, fn, rows in sites:
        rs = [r for r in rows if not forgiven(r[1])]
        if rs and pct.get((unit, fn)) == 100.0:
            charged_fns.add((unit, fn))

    objp = load_unit_objs(root)
    cache = {}
    null_tot = null_absent = 0
    for unit, fn in charged_fns:
        if unit not in objp:
            continue
        if unit not in cache:
            try:
                cache[unit] = index_fns(objp[unit][1])
            except Exception:
                cache[unit] = None
        of = cache[unit]
        if not of or fn not in of:
            continue
        _sz, orl = of[fn]
        for _o, nm, ty in orl:
            if ty != 0x06 or forgiven(nm):
                continue
            null_tot += 1
            if nm not in defined and nm not in weak:
                null_absent += 1
    treat_absent = att['SRC_absent_B']
    treat_tot = tot
    print("\n=== CONDITIONED NULL for 'callee absent from our build' ===")
    print("   population: ALL bl callees of the SAME at-100 charged functions,")
    print("               charged and uncharged slots alike (same callers, same")
    print("               units, same regime) -- only the charge differs.")
    print("   null    : callee absent from our build  %5d / %5d  (%5.2f%%)"
          % (null_absent, null_tot, 100.0 * null_absent / max(1, null_tot)))
    print("   treated : SRC_absent_B in still-UNKNOWN %5d / %5d  (%5.2f%%)"
          % (treat_absent, treat_tot, 100.0 * treat_absent / max(1, treat_tot)))
    if null_absent:
        print("   enrichment %.1fx"
              % ((treat_absent / treat_tot) / (null_absent / null_tot)))
    else:
        print("   null fired 0 -- report as a BOUND, not an enrichment")


if __name__ == "__main__":
    main()
