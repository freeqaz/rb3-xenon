#!/usr/bin/env python3
"""Audit every OBJ_CLASSNAME literal in our tree against retail band.exe's
STRING POOL.  Lane BS-3, extending BP-7/BQ-1's StaticClassName-literal channel.

WHY THIS EXISTS -- IT IS MAP-INDEPENDENT
    staticclassname_literal_scan.py compares the literal a MAPPED VA builds
    against the literal the MAPPED CLASS declares.  A disagreement there is
    genuinely ambiguous: it can mean the map row is mispointed, OR that our
    header's OBJ_CLASSNAME argument is wrong (lane BQ-1 job C found both, and
    warned that repointing the map when the defect is in the source "fixes
    nothing and corrupts the map").

    This scan removes the ambiguity by never consulting the map.  OBJ_CLASSNAME(X)
    emits the C string "X" into .rdata and builds a Symbol from it.  If the exact
    NUL-terminated string "X" appears NOWHERE in the linked retail image, then no
    retail code can be building that Symbol -- our literal is provably impossible
    for this target.  One-sided but airtight: absence is proof, presence is not.

INTERPRETING A HIT -- two populations, and you must separate them
    (a) SOURCE DEFECT  the class DOES exist in retail under a different literal.
    (b) PHANTOM CLASS  the class does not exist in RB3 at all -- overwhelmingly
                       DC3-only code we inherited (src/system/{hamobj,gesture,
                       flow}, most of synth), or Wii-only game classes.  These
                       are NOT source defects; leave them alone.
    The script cannot decide this for you.  Confirm (a) by finding the retail
    body that builds the proposed literal and checking that ITS callers are the
    class's own ClassName()/SetType()/Init().

THE REPLACEMENT RULE -- "repeat the BASE's literal", NOT "strip the prefix"
    BQ-1 phrased this channel's rule as "retail's DTA class name drops the
    Ng/App prefix".  That is a coincidence of the examples it saw.  The real
    rule is that a platform/app subclass re-registers under its BASE class's
    DTA name, so milo data authored as the base type instantiates the subclass.
    AppLabel is the counterexample that separates the two: the prefix-stripped
    "Label" is built by ZERO retail bodies, while its base BandLabel is what
    the retail body actually builds.  So resolve a hit through the INHERITANCE
    CHAIN, never by string surgery on the prefix.

    Corollary: this scan deliberately does NOT propose a replacement.  An
    earlier prefix-stripping heuristic proposed "Label" for AppLabel and was
    wrong; emitting a confident wrong suggestion is worse than emitting none.

DO NOT BLINDLY FOLLOW THE ORACLES
    Both diverge from RB3-360 retail here.  Measured 2026-07-30:
      * DC3 renamed FxSendWah -> FxSendWah360; "FxSendWah360" is absent from
        retail and "FxSendWah" is present, so OUR literal is the correct one.
      * rb3-Wii's DEV decomp writes OBJ_CLASSNAME(AppInlineHelp) /
        (AppMiniLeaderboardDisplay) / (AppLabel) -- all three strings are absent
        from 360 retail.  Our headers were faithful copies of a source that does
        not describe this target.
    band.exe is the authority; the oracles are hints.

USAGE
    python3 scripts/harvest/objclassname_absence_scan.py [--all]
        default: hide the src/system/{hamobj,gesture,flow} DC3-only bulk
        --all:   show every absent literal
"""
import argparse, re, sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(HERE))
from icf_contradiction_adjudicate import PE, BANDEXE  # noqa: E402

# Directories that are DC3-only engine code we inherited; their classes are
# legitimately absent from RB3 and are not source defects.
DC3_ONLY = ("src/system/hamobj/", "src/system/gesture/", "src/system/flow/")


def strip_comments(t):
    """Blank comments -- a header documenting its own literal choice would
    otherwise be parsed as declaring the commented token (see the same guard in
    staticclassname_literal_scan.py; this bit us for real)."""
    t = re.sub(r'/\*.*?\*/', lambda m: re.sub(r'[^\n]', ' ', m.group(0)), t, flags=re.S)
    return re.sub(r'//[^\n]*', '', t)


def harvest(root):
    """(literal, declaring class, file, line) for every OBJ_CLASSNAME site."""
    out = []
    for p in sorted(list((root / "src").rglob("*.h")) + list((root / "src").rglob("*.cpp"))):
        try:
            t = strip_comments(p.read_text(errors="replace"))
        except Exception:
            continue
        if "OBJ_CLASSNAME" not in t:
            continue
        lines = t.split("\n")
        for i, ln in enumerate(lines):
            m = re.search(r'OBJ_CLASSNAME\(\s*(\w+)\s*\)', ln)
            if not m:
                continue
            if re.match(r'\s*#\s*define', ln):
                continue        # the macro's own definition, not a use site
            cls = "?"
            for j in range(i, -1, -1):
                c = re.match(r'\s*(?:class|struct)\s+(\w+)\b', lines[j])
                if c:
                    cls = c.group(1)
                    break
            out.append((m.group(1), cls, str(p.relative_to(root)), i + 1))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--all", action="store_true",
                    help="include the DC3-only hamobj/gesture/flow bulk")
    a = ap.parse_args()

    pe = PE(BANDEXE)

    def present(lit):
        return pe.data.find(lit.encode() + b"\0") >= 0

    sites = harvest(ROOT)
    absent = [s for s in sites if not present(s[0])]
    shown = absent if a.all else [s for s in absent
                                  if not any(s[2].startswith(d) for d in DC3_ONLY)]

    print("OBJ_CLASSNAME sites: %d   distinct literals: %d"
          % (len(sites), len(set(s[0] for s in sites))))
    print("absent from retail band.exe: %d  (%d after hiding DC3-only dirs)"
          % (len(absent), len(shown)))
    print("\nliteral                        declared by                    site")
    for lit, cls, f, ln in sorted(shown):
        print("  %-28s %-28s %s:%d" % (lit, cls, f, ln))
    print("\nEach line is a literal retail CANNOT be building.  Triage every one as")
    print("SOURCE DEFECT vs PHANTOM CLASS before touching it, and resolve a source")
    print("defect through the inheritance chain -- not by stripping the prefix.")


if __name__ == "__main__":
    main()
