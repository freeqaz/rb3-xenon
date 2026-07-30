#!/usr/bin/env python3
"""Audit every `?StaticClassName@<Class>@@` map entry against the CLASS-NAME
STRING the retail body actually builds.  Lane BP-7.

THE PROOF THIS AUTOMATES
    OBJ_CLASSNAME(literal) expands to a StaticClassName() that constructs a
    Symbol from the string literal `"literal"`.  Every such body is therefore
    IDENTICAL machine code across all classes except for ONE field: the
    relocation that supplies the string pointer.  objdiff runs
    functionRelocDiffs=None, so that single distinguishing field is invisible to
    the score and EVERY StaticClassName body matches EVERY other one at 100.0%.
    A whole family of interchangeable 0x58 bodies is thus free to be scrambled
    across the map while reading a clean 100% -- the at-100% defect class
    (memory: project_correctness_vs_metric) in its purest form.

    But the string is right there in the body.  Disassemble the mapped VA,
    recover the lis/addi (or lis/ori) pair that builds the .rdata pointer, read
    the C string, and compare it to the OBJ_CLASSNAME literal that the mapped
    class declares in our own source.  Disagreement is a proven mispair, with no
    oracle and no build required.

    Note the literal is NOT always the C++ class name -- `class RndSpline` uses
    OBJ_CLASSNAME(Spline), `class RndCam` uses OBJ_CLASSNAME(Cam).  This script
    therefore parses the real OBJ_CLASSNAME argument out of the class body
    rather than assuming the class name, which is exactly the trap that made an
    earlier phantom-class sweep misclassify RndSpline as absent from retail.

USAGE
    python3 scripts/harvest/staticclassname_literal_scan.py [--out out.json]
"""
import argparse, json, re, struct, sys
from pathlib import Path
from collections import defaultdict, Counter

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(HERE))
from icf_contradiction_adjudicate import PE, load_symbols, BANDEXE  # noqa: E402


def strip_comments(t):
    """Blank out // and /* */ comments, preserving line structure.

    LOAD-BEARING (lane BS-3): the OBJ_CLASSNAME search below is a plain regex
    over the class body and will happily match a macro call written inside a
    COMMENT.  A header that documents its own literal choice -- e.g. "DC3's
    AppLabel repeats its base (OBJ_CLASSNAME(HamLabel))" -- then makes this
    index report the commented token instead of the declared one, silently
    inverting the row's verdict.  Observed for real; do not remove.
    """
    t = re.sub(r'/\*.*?\*/', lambda m: re.sub(r'[^\n]', ' ', m.group(0)), t, flags=re.S)
    return re.sub(r'//[^\n]*', '', t)


def build_lit_index(root):
    """C++ class name -> OBJ_CLASSNAME literal, parsed from our own source."""
    cls2lit = {}
    for p in list((root / "src").rglob("*.h")) + list((root / "src").rglob("*.cpp")):
        try:
            t = strip_comments(p.read_text(errors="replace"))
        except Exception:
            continue
        if "OBJ_CLASSNAME" not in t:
            continue
        for m in re.finditer(r'class\s+(\w+)\s*(?::[^{;]*)?\{(.*?)\n\};', t, re.S):
            k = re.search(r'OBJ_CLASSNAME\(\s*(\w+)\s*\)', m.group(2))
            if k:
                cls2lit.setdefault(m.group(1), k.group(1))
    return cls2lit


def literals_in(pe, body, va):
    """Absolute addresses built by lis+addi / lis+ori, resolved to C strings."""
    his, out = {}, []
    for i in range(0, len(body) - 3, 4):
        w = struct.unpack_from(">I", body, i)[0]
        op, rt, ra, imm = (w >> 26) & 0x3F, (w >> 21) & 0x1F, (w >> 16) & 0x1F, w & 0xFFFF
        if op == 15 and ra == 0:
            his[rt] = imm << 16
        elif op == 14 and ra in his:
            si = imm - 0x10000 if imm & 0x8000 else imm
            out.append((his[ra] + si) & 0xFFFFFFFF)
        elif op == 24 and ra in his:
            out.append((his[ra] | imm) & 0xFFFFFFFF)
    res = []
    for a in out:
        o, s = pe.va2off(a)
        if o is None or s not in (".rdata", ".data"):
            continue
        try:
            e = pe.data.index(b"\0", o)
        except ValueError:
            continue
        v = pe.data[o:e]
        if 0 < len(v) < 64 and all(32 <= c < 127 for c in v):
            res.append(v.decode("latin1"))
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out")
    a = ap.parse_args()
    pe, syms = PE(BANDEXE), load_symbols()
    m = json.loads((ROOT / "scripts" / "target_symbol_map.json").read_text())
    cls2lit = build_lit_index(ROOT)
    rep = json.loads((ROOT / "build" / "45410914" / "report.json").read_text())
    pct = {}
    for u in rep["units"]:
        for f in (u.get("functions") or []):
            pct.setdefault(f["name"], f.get("match_percent_normalized"))

    rows, tally = [], Counter()
    for k, v in m.items():
        if not k.startswith("0x"):
            continue
        for n in (v if isinstance(v, list) else [v]):
            mm = re.match(r'^\?StaticClassName@(\w+)@@', n)
            if not mm:
                continue
            cls = mm.group(1)
            want = cls2lit.get(cls)
            va = int(k, 16)
            _, size, _ = syms.get(va, (None, 0, None))
            got = literals_in(pe, pe.read(va, size)[0], va) if size else []
            if want is None:
                verdict = "NO_SOURCE_LITERAL"
            elif not got:
                verdict = "NO_STRING_FOUND"
            elif want in got:
                verdict = "AGREE"
            else:
                verdict = "CONTRADICT"
            tally[verdict] += 1
            rows.append(dict(va=k, name=n, cls=cls, want=want, got=got,
                             verdict=verdict, pct=pct.get(n), size=size))
    for k2, c in tally.most_common():
        print("%-18s %d" % (k2, c))
    bad = [r for r in rows if r["verdict"] == "CONTRADICT"]
    print("\nCONTRADICT (%d, of which at 100%%: %d):"
          % (len(bad), sum(1 for r in bad if (r["pct"] or 0) >= 99.99)))
    for r in sorted(bad, key=lambda r: r["va"]):
        print("  %s %-46s declares %-22s body builds %s"
              % (r["va"], r["name"][:46], r["want"], r["got"]))
    if a.out:
        Path(a.out).write_text(json.dumps(rows, indent=1))
        print("\nwrote %s" % a.out)


if __name__ == "__main__":
    main()
