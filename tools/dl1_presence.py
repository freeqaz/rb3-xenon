#!/usr/bin/env python3
"""Lane DL-1: do the 147 compiled-but-unpinned TUs EXIST in RB3 retail at all?

DK-4 concluded "0 of 152 can be bounded on Tier-A evidence" and read that as an
identification failure.  There is a rival explanation with completely different
consequences: some of these TUs are DC3 code we inherited via ../dc3-decomp that
RB3 retail never contained.  The directory names are suggestive -- 39 are
system/hamobj ("ham" is Dance Central's own codename, cf. ham_xbox_r.map), 20 are
system/gesture (Kinect), and CLAUDE.md/the brief state RB3 retail has ZERO Flow*
strings while 6 are system/flow.  If a class is absent from RB3 retail then NO
identification channel -- structural, string, or otherwise -- can ever bound it,
and the budget spent hunting it is wasted by construction.

INSTRUMENT (non-metric, ground truth): MSVC RTTI TypeDescriptor presence.
A class with a vtable emits a ??_R0 TypeDescriptor whose name string is
'.?AV<Class>@@' verbatim in the image.  Search the raw bytes of both retail PEs.
Pure Python -- the shell's `grep` is binary-blind (ugrep -I) and would return
only false negatives here, which is precisely the shape of a fake decisive
negative.

CONTROL (the untreated population -- rule 3): run the identical test over the
ALREADY-PINNED TUs.  Those are known to exist in RB3 retail, so their presence
rate is the base rate.  Without it, a raw "62% of unpinned TUs are present" is
uninterpretable: plenty of TUs (math helpers, enum tables, template-only units)
define no RTTI-bearing class at all, and would read absent in BOTH groups.
The finding is the DIFFERENCE between the groups, never the treatment rate.
"""
import json
import os
import re
import sys
from collections import Counter, defaultdict

REPO = "/home/free/code/milohax/rb3-xenon"
DC3EXE = "/home/free/code/milohax/dc3-decomp/orig/373307D9/ham_xbox_r.exe"
DC3MAP = "/home/free/code/milohax/dc3-decomp/orig/373307D9/ham_xbox_r.map"
sys.path.insert(0, os.path.join(REPO, "tools"))
import dc3_map  # noqa: E402


def pinned_units():
    """Unit stems that have a .text pin in splits.txt (READ ONLY -- DL-3 owns it)."""
    out = []
    for line in open(os.path.join(REPO, "config/45410914/splits.txt")):
        m = re.match(r"^(\S+\.cpp):\s*$", line)
        if m:
            out.append(m.group(1))
    return out


def main():
    rb3 = open(os.path.join(REPO, "orig/45410914/band.exe"), "rb").read()
    dc3 = open(DC3EXE, "rb").read()

    def present(blob, cls):
        return blob.find(b".?AV" + cls.encode() + b"@@") >= 0

    # --- C1 known-positive: a class that certainly IS in both images --------
    for cls in ("RndText", "Character", "Object@Hmx"):
        assert present(rb3, cls), f"C1 FAILED: {cls} absent from RB3 image"
    assert present(dc3, "Character"), "C1 FAILED: Character absent from DC3 image"
    # --- C1b known-NEGATIVE: the detector must also be able to say NO ------
    assert not present(rb3, "ZzzNotARealClassXyz"), "C1b FAILED: detector says yes to everything"
    print("[C1 PASS] RTTI presence detector fires on known positives "
          "and refuses a known negative")

    dmap = dc3_map.parse_map(DC3MAP)
    dc3_objs = {e["obj"].split(":")[-1] for e in dmap.values()}

    unpinned = json.load(open("/home/free/tmp/laneDF4/unpinned_tus.json"))
    pin = pinned_units()
    print(f"[in] unpinned TUs {len(unpinned)}   pinned (control) TUs {len(pin)}")

    def survey(stems, label):
        rows = []
        c = Counter()
        for stem in stems:
            base = os.path.basename(stem)[:-4]      # strip .cpp
            inr, ind = present(rb3, base), present(dc3, base)
            inobj = (base + ".obj") in dc3_objs
            key = ("RB3" if inr else "-") + "/" + ("DC3" if ind else "-")
            c[key] += 1
            rows.append({"src": stem, "cls": base, "in_rb3": inr,
                         "in_dc3": ind, "dc3_obj": inobj})
        n = len(stems)
        print(f"\n=== {label} (n={n}) ===")
        for k in ("RB3/DC3", "RB3/-", "-/DC3", "-/-"):
            print(f"   {k:9s} {c[k]:5d}  ({100*c[k]/n:5.1f}%)")
        print(f"   -> class present in RB3 retail: "
              f"{c['RB3/DC3']+c['RB3/-']}/{n} = "
              f"{100*(c['RB3/DC3']+c['RB3/-'])/n:.1f}%")
        print(f"   -> DC3-ONLY (in DC3, absent from RB3): {c['-/DC3']}/{n} = "
              f"{100*c['-/DC3']/n:.1f}%")
        return rows, c

    prow, pc = survey(pin, "CONTROL: already-pinned TUs (known to be in RB3)")
    urow, uc = survey([u["src"] for u in unpinned],
                      "TREATMENT: the compiled-but-unpinned TUs")

    pr = (pc['RB3/DC3'] + pc['RB3/-']) / max(1, len(pin))
    ur = (uc['RB3/DC3'] + uc['RB3/-']) / max(1, len(unpinned))
    print(f"\n=== SEPARATION ===")
    print(f"  RB3-presence rate  pinned(control) {100*pr:.1f}%  vs "
          f"unpinned(treatment) {100*ur:.1f}%")
    if ur:
        print(f"  depletion factor: {pr/ur:.2f}x")

    # per-directory breakdown of the treatment group
    bydir = defaultdict(lambda: [0, 0])
    for r in urow:
        d = "/".join(r["src"].split("/")[:2])
        bydir[d][1] += 1
        if r["in_rb3"]:
            bydir[d][0] += 1
    print("\n=== unpinned TUs: RB3 presence BY DIRECTORY ===")
    for d, (a, b) in sorted(bydir.items(), key=lambda x: -x[1][1]):
        print(f"   {d:28s} present {a:3d}/{b:3d}")

    absent = [r for r in urow if not r["in_rb3"]]
    print(f"\n=== {len(absent)} unpinned TUs whose class is ABSENT from RB3 retail ===")
    for r in absent:
        tag = "DC3-only" if r["in_dc3"] else "neither"
        print(f"   {r['src']:58s} {tag}")

    json.dump({"pinned": prow, "unpinned": urow},
              open("/home/free/tmp/laneDL1/presence.json", "w"), indent=1)


if __name__ == "__main__":
    main()
