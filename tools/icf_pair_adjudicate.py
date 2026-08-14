#!/usr/bin/env python3
"""Adjudicate ONE (retail survivor, our spelling) alias candidate on retail bytes.

``tools/icf_alias_build.py`` is the batch generator; it only ever adjudicates
pairs some *enumerator* proposed, and its summary cannot distinguish "this pair
was REFUTED" from "this pair was never proposed". When a single charged site is
worth thousands of bytes, that distinction is the whole question, so this tool
takes an explicit pair and prints the T1 decision with every input shown.

It reuses ``icf_alias_build``'s primitives verbatim (``collect``, ``relocs_agree``,
``vacuous``) so a verdict here is the same verdict the generator would reach --
this is a magnifying glass on that adjudicator, NOT a second one.

T1 asks: are the RETAIL bytes at the survivor's address byte-identical, modulo
relocated fields, to what OUR compiler emits for the folded spelling -- AND do
the two agree on relocation TARGETS, not merely on shape? (Masked bytes alone
are vacuous for template twins: ``vector<Foo>::erase`` and ``vector<Bar>::erase``
have identical machine bytes and differ ONLY in the destructor they call.)

It additionally reports UNIQUENESS on both sides, because a body shared by many
functions proves nothing about which one a call site meant -- picking one of an
ICF-folded group is a coin flip.

    python3 tools/icf_pair_adjudicate.py --survivor '?Foo@@...' --ours '?Bar@@...'
    python3 tools/icf_pair_adjudicate.py --selftest      # controls; run this first
"""

import argparse
import collections
import glob
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
from icf_alias_build import collect, relocs_agree, vacuous, placeholder  # noqa: E402


def load_sides():
    tgt = collect(sorted(glob.glob(str(ROOT / "build/45410914/obj/**/*.obj"), recursive=True)),
                  "retail target objs")
    ours = collect(sorted(glob.glob(str(ROOT / "build/45410914/src/**/*.obj"), recursive=True)),
                   "our objs")
    return tgt, ours


def body_index(side):
    idx = collections.defaultdict(list)
    for name, (mb, _r, _s) in side.items():
        idx[mb].append(name)
    return idx


def adjudicate(tgt, ours, survivor, our_name, mapped, verbose=True):
    """Return (verdict, detail dict). Verdict in PROVEN / REFUTED / UNDECIDABLE."""
    d = {"survivor": survivor, "ours": our_name}
    rt, ob = tgt.get(survivor), ours.get(our_name)
    if rt is None:
        return "UNDECIDABLE", dict(d, why="survivor absent from the dtk target objs "
                                          "(address outside every pinned .text span)")
    if ob is None:
        return "UNDECIDABLE", dict(d, why="our spelling is in no compiled obj")
    d["retail_size"], d["our_size"] = rt[2], ob[2]
    if vacuous(rt) or vacuous(ob):
        return "UNDECIDABLE", dict(d, why="VACUOUS: body under 4 words or over half "
                                          "the words masked -- compares equal to too much")
    if rt[0] != ob[0]:
        return "REFUTED", dict(d, why="masked bodies DIFFER (retail did not keep the "
                                      "code our spelling compiles to)")
    tally = collections.Counter()
    if not relocs_agree(rt, ob, mapped, strict=True, tally=tally):
        return "REFUTED", dict(d, why="masked bodies match but relocation TARGETS "
                                      "disagree -- template-twin, not a fold",
                               reloc_tally=dict(tally))
    d["reloc_tally"] = dict(tally)
    d["n_relocs"] = len(rt[1])
    return "PROVEN", d


def uniqueness(tgt, ours, survivor, our_name):
    ti, oi = body_index(tgt), body_index(ours)
    out = {}
    if survivor in tgt:
        peers = ti[tgt[survivor][0]]
        out["retail_bodytwins"] = len(peers)
        out["retail_bodytwin_names"] = sorted(peers)[:8]
    if our_name in ours:
        peers = oi[ours[our_name][0]]
        out["our_bodytwins"] = len(peers)
        out["our_bodytwin_names"] = sorted(peers)[:8]
    return out


def load_mapped():
    m = json.load(open(ROOT / "scripts/target_symbol_map.json"))
    out = set()
    for _a, n in m.items():
        for x in (n if isinstance(n, list) else [n]):
            if x:
                out.add(x)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--survivor")
    ap.add_argument("--ours")
    ap.add_argument("--pairs", help="json list of [survivor, ours] pairs")
    ap.add_argument("--selftest", action="store_true")
    a = ap.parse_args()

    mapped = load_mapped()
    tgt, ours = load_sides()

    pairs = []
    if a.selftest:
        # A gate that cannot FAIL is worse than no gate, and a gate that cannot
        # PASS is equally useless. Both directions are exercised here against
        # ground truth taken from the already-landed scripts/symbol_aliases.json.
        al = json.load(open(ROOT / "scripts/symbol_aliases.json"))
        pos = None
        for g in al["groups"]:
            for f in g["folded"]:
                if g["survivor"] in tgt and f in ours:
                    pos = (g["survivor"], f)
                    break
            if pos:
                break
        neg_s = next(n for n in tgt if n.startswith("?") and not vacuous(tgt[n])
                     and tgt[n][2] > 400)
        neg_o = next(n for n in ours if n.startswith("?") and not vacuous(ours[n])
                     and ours[n][2] > 400 and ours[n][0] != tgt[neg_s][0])
        pairs = [("POSITIVE CONTROL (expect PROVEN)", pos[0], pos[1]),
                 ("NEGATIVE CONTROL (expect REFUTED)", neg_s, neg_o)]
    elif a.pairs:
        pairs = [("", s, o) for s, o in json.load(open(a.pairs))]
    else:
        pairs = [("", a.survivor, a.ours)]

    rc = 0
    for label, s, o in pairs:
        verdict, det = adjudicate(tgt, ours, s, o, mapped)
        det.update(uniqueness(tgt, ours, s, o))
        det["survivor_map_resident"] = s in mapped
        print("\n=== %s" % (label or "%s  <->  %s" % (s[:60], o[:60])))
        print("  survivor : %s" % s)
        print("  ours     : %s" % o)
        print("  VERDICT  : %s" % verdict)
        for k, v in det.items():
            if k in ("survivor", "ours"):
                continue
            print("      %-28s %s" % (k, v))
        if a.selftest:
            want = "PROVEN" if "POSITIVE" in label else "REFUTED"
            if verdict != want:
                print("  ** SELFTEST FAILED: wanted %s **" % want)
                rc = 1
    if a.selftest:
        print("\nselftest %s" % ("FAILED" if rc else "PASSED -- the instrument can "
                                                     "both pass and fail"))
    return rc


if __name__ == "__main__":
    sys.exit(main())
