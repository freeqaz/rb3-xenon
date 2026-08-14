#!/usr/bin/env python3
"""SRCCAND-1: characterise the WC2 SOURCE_CAND queue by DEFECT CLASS.

Two lanes (WRONGCALL-2, REGORDER-1) isolated this queue without ever saying what
kind of wrong callee it holds.  This tool answers that, on two orthogonal axes.

AXIS 1 -- NAME SHAPE.  How do the two spellings of a charged pair differ?

  TEMPLATE_TWIN        same template, different type argument(s).  The BandInit
                       archetype: ``list<CharClip*>`` vs ``list<void(*)()>``.
                       Machine-code-identical families whose only discriminator
                       is a relocation -- the shape /OPT:ICF folds.
  SAME_CLASS_METHOD    same enclosing class, DIFFERENT method.  The only shape
                       that cannot be explained by a fold or an element type:
                       our source calls the wrong member of the right object.
  SAME_METHOD_CLASS    same method name, different class (``NewObject@A`` vs
                       ``NewObject@B``).  The factory-registration shape
                       REGORDER-1 worked; ordering, not naming.
  CTOR_FAMILY/DTOR_FAMILY   ctors/dtors of different classes.
  UNRELATED            no structural relationship at all.

AXIS 2 -- PERMUTATION, and this is the axis that decides ACTIONABILITY.
  Group the charged slots by the VICTIM function that carries them.  If, within
  one victim, the multiset of retail-side names equals the multiset of our-side
  names, then our source calls exactly the right set of functions IN THE WRONG
  ORDER.  That is a real source defect, it is read straight off the victim's own
  retail body, and -- crucially -- adjudicating it needs no third channel: the
  two names are already body-attested by screens A(rn) and A(on), and the ORDER
  is a property of the victim, not of the map.

  A victim whose charges are NOT a permutation is a genuine substitution: we
  call something retail never calls.  Those need the retail-byte or .rdata
  channel and are priced separately.

>> --selfcheck proves each classifier CAN fire and CAN fail before any count is
   believed.  Sixteen instruments in three days were caught unable to fail.
"""

import argparse
import collections
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent


def undname(names):
    """Batch-demangle MSVC symbols through llvm-undname."""
    # llvm-undname ECHOES the input symbol, then prints the demangling, then a
    # blank line.  Parse pairs; a bare echo with no demangling (unparseable name)
    # maps to itself rather than silently shifting every later row by one.
    p = subprocess.run(["llvm-undname"], input="\n".join(names) + "\n",
                       capture_output=True, text=True)
    lines = p.stdout.splitlines()
    out, i = {}, 0
    for n in names:
        while i < len(lines) and not lines[i].strip():
            i += 1
        if i >= len(lines) or lines[i].strip() != n:
            sys.exit("REFUSING: llvm-undname output desynchronised at %r -- the "
                     "join is broken, do not read the classification below." % n)
        i += 1
        val = lines[i].strip() if i < len(lines) and lines[i].strip() else n
        if val != n:
            i += 1
        out[n] = val
    if len(out) != len(set(names)):
        sys.exit("REFUSING: demangled %d of %d distinct names." % (len(out), len(set(names))))
    return out


# ---------------------------------------------------------------- name shape

_OPCODE = {"0": "ctor", "1": "dtor", "4": "operator=", "R": "operator()",
           "5": "operator>>", "6": "operator<<", "8": "operator==",
           "9": "operator!=", "M": "operator<", "2": "operator[]",
           "A": "operator[]", "B": "operator-cast", "C": "operator*",
           "D": "operator*", "E": "operator+", "G": "operator-"}


def split_name(m):
    """Structural split of an MSVC mangled name -> (kind, base, scope, targs).

    Deliberately shallow: we need the FUNCTION identity and its ENCLOSING scope,
    not a full type parse.  ``scope`` is the qualification text between the base
    name and the signature; ``targs`` is the template-argument text when the base
    is itself a template (``??$Name@ARGS@scope@@``).
    """
    if not m.startswith("?"):
        return ("plain", m, "", "")
    if m.startswith("??$"):                      # template FUNCTION
        rest = m[3:]
        i = rest.find("@")
        base, tail = rest[:i], rest[i + 1:]
        j = tail.find("@@")
        return ("tmplfn", base, tail[j + 2:] if j >= 0 else "", tail[:j] if j >= 0 else tail)
    if m.startswith("??"):                       # operator / ctor / dtor
        c = m[2]
        rest = m[3:]
        if c == "$":
            return ("tmplop", rest.split("@")[0], rest, "")
        if c == "?":                             # ??_7 vftable etc.
            return ("special", m[2:6], m, "")
        return (_OPCODE.get(c, "op" + c), _OPCODE.get(c, "op" + c), rest, "")
    rest = m[1:]                                 # ?Name@Scope@@sig
    i = rest.find("@")
    base, tail = rest[:i], rest[i + 1:]
    return ("method", base, tail, "")


def _scope_key(scope):
    """Enclosing scope up to the signature, normalised."""
    j = scope.find("@@")
    return scope[:j] if j >= 0 else scope


def _strip_targs(s):
    """Erase template argument blobs so two instantiations compare equal."""
    return re.sub(r"\?\$([A-Za-z_0-9]+)@[^@]*(?:@@[^@]*)*", r"?$\1", s)


def shape(rn, on):
    kr, br, sr, tr = split_name(rn)
    ko, bo, so, to = split_name(on)
    skr, sko = _scope_key(sr), _scope_key(so)

    # Template twins: same template base name, same enclosing scope, args differ.
    if kr == ko == "tmplfn" and br == bo and tr != to:
        return "TEMPLATE_TWIN"
    # Class-template twins: identical once template arguments are erased.
    if _strip_targs(rn) == _strip_targs(on) and rn != on:
        return "TEMPLATE_TWIN"
    if kr == "dtor" and ko == "dtor":
        return "TEMPLATE_TWIN" if _strip_targs(sr) == _strip_targs(so) else "DTOR_FAMILY"
    if kr == "ctor" and ko == "ctor":
        return "TEMPLATE_TWIN" if _strip_targs(sr) == _strip_targs(so) else "CTOR_FAMILY"
    if skr and skr == sko and br != bo:
        return "SAME_CLASS_METHOD"
    if br == bo and skr != sko:
        return "SAME_METHOD_CLASS"
    if br == bo and skr == sko:
        return "SAME_NAME_DIFF_SIG"
    return "UNRELATED"


# ------------------------------------------------------------- permutation

def victim_permutation(rows):
    """Group charged pairs by victim function; is each victim a PERMUTATION?

    ``rows`` carry only the first 6 victims (wc2_classify truncates), so a victim
    is analysed only when we hold all of its charges.  Truncated victims are
    reported separately rather than silently classified -- an under-populated
    multiset compares unequal and would read as SUBSTITUTION, which is the
    work-manufacturing direction.
    """
    byv = collections.defaultdict(list)
    truncated = set()
    for r in rows:
        if r["n_victims"] > len(r["victims"]):
            truncated.add((r["retail_name"], r["our_name"]))
        for v in r["victims"]:
            byv[v].append((r["retail_name"], r["our_name"]))
    out = {}
    for v, prs in byv.items():
        rset = collections.Counter(p[0] for p in prs)
        oset = collections.Counter(p[1] for p in prs)
        if any(p in truncated for p in prs):
            out[v] = "UNKNOWN_truncated"
        elif rset == oset:
            out[v] = "PERMUTATION"
        elif rset & oset:
            out[v] = "PARTIAL_overlap"
        else:
            out[v] = "SUBSTITUTION"
    return out, byv


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--classified", default="/home/free/tmp/srccand1_classified.json")
    ap.add_argument("--cls", default="SOURCE_CAND")
    ap.add_argument("--both-verified", action="store_true",
                    help="restrict to screenA_on == EQUAL (the 'can only be a "
                         "wrong callee' combination)")
    ap.add_argument("--selfcheck", action="store_true")
    ap.add_argument("--out", default="/home/free/tmp/srccand1_characterized.json")
    args = ap.parse_args()

    rows = json.load(open(args.classified))
    sel = [r for r in rows if r["cls"] == args.cls]
    if args.both_verified:
        sel = [r for r in sel if r["screenA_on"] == "EQUAL"]

    if args.selfcheck:
        # Each classifier must be shown able to return >1 verdict over the real
        # population, and the permutation axis must produce BOTH outcomes.
        sh = collections.Counter(shape(r["retail_name"], r["our_name"]) for r in sel)
        perm, _ = victim_permutation(sel)
        pv = collections.Counter(perm.values())
        print("SELFCHECK over %d rows" % len(sel))
        print("  shape verdicts:      %s" % dict(sh))
        print("  permutation verdicts:%s" % dict(pv))
        ok = len(sh) > 1 and len(pv) > 1
        print("  CAN DISCRIMINATE (both axes return >1 verdict): %s" % ok)
        # negative control: a shuffled pairing must NOT read as mostly TEMPLATE_TWIN
        import random
        random.seed(7)
        ons = [r["our_name"] for r in sel]
        random.shuffle(ons)
        null = collections.Counter(shape(r["retail_name"], o) for r, o in zip(sel, ons))
        tw = 100.0 * null["TEMPLATE_TWIN"] / max(1, len(sel))
        print("  NULL (shuffled our_name): TEMPLATE_TWIN %.1f%% vs real %.1f%%"
              % (tw, 100.0 * sh["TEMPLATE_TWIN"] / max(1, len(sel))))
        if not ok:
            print("  >> VACUOUS -- do not believe the classification.")
            return 1
        return 0

    perm, byv = victim_permutation(sel)
    sh_of = {}
    out = []
    for r in sel:
        s = shape(r["retail_name"], r["our_name"])
        sh_of[(r["retail_name"], r["our_name"])] = s
        pv = sorted({perm[v] for v in r["victims"]}) or ["NO_VICTIM"]
        out.append(dict(r, shape=s, victim_verdicts=pv))

    print("\n=== AXIS 1: NAME SHAPE (%d pairs, %d sites) ==="
          % (len(sel), sum(r["sites"] for r in sel)))
    c = collections.Counter(r["shape"] for r in out)
    for k, v in c.most_common():
        print("   %-20s %4d pairs  %4d sites" % (k, v, sum(r["sites"] for r in out if r["shape"] == k)))

    print("\n=== AXIS 2: VICTIM PERMUTATION (%d distinct victim functions) ===" % len(perm))
    pc = collections.Counter(perm.values())
    for k, v in pc.most_common():
        print("   %-20s %4d victims" % (k, v))

    print("\n=== CROSS (shape x victim verdict) ===")
    x = collections.Counter()
    for r in out:
        for pv in r["victim_verdicts"]:
            x[(r["shape"], pv)] += 1
    for (s, p), n in sorted(x.items(), key=lambda kv: -kv[1]):
        print("   %-20s %-20s %4d" % (s, p, n))

    print("\n=== RECIPROCAL pairs (both (a,b) and (b,a) charged) ===")
    ps = {(r["retail_name"], r["our_name"]) for r in sel}
    recip = {p for p in ps if (p[1], p[0]) in ps}
    print("   %d of %d pairs are reciprocal (%.1f%%) -- the permutation-family "
          "signature" % (len(recip), len(ps), 100.0 * len(recip) / max(1, len(ps))))

    json.dump({"rows": out,
               "victims": {v: perm[v] for v in perm},
               "victim_pairs": {v: p for v, p in byv.items()}},
              open(args.out, "w"), indent=1)
    print("\nwrote", args.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
