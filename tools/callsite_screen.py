#!/usr/bin/env python3
"""Screen the named population for rows whose retail extent is UNREACHABLE by any
direct call -- a second false-pairing detector, orthogonal to the EH-funclet one.

WHY THIS EXISTS (lane BODIES-4, successor to BODIES-3)
  BODIES-3 automated the FUNCLET half of false-pairing triage and said plainly
  that an empty result there does NOT mean a candidate list is clean: its other
  two refutations were SEMANTIC and needed a human to read the retail body.
  This encodes a second cheap, mechanical half of that triage.

THE IDEA
  Harmonix code is compiled /Gy (one COMDAT per function -- verified in
  CLAUDE.md's flag work), so a function COMDAT that nothing references is
  discarded by the linker and never reaches .text at all. Therefore every
  function that IS present must be referenced somehow. For a VIRTUAL the
  reference is a vtable slot, and there may be no direct call anywhere. But for
  a NON-VIRTUAL member, a static member, or a free function, the ordinary
  reference is a `bl`.

  So: a row whose name says "non-virtual" but whose mapped address is the target
  of ZERO `bl` edges in the whole 197k-edge call graph is a row where the name
  and the address disagree about what the function is.

  Virtual-ness is read out of the MSVC mangling: the character right after the
  class-qualifier `@@` is the access/storage code, and E F M N U V are the
  virtual ones. (?Save@OutfitConfig@@UAAX... -> U -> virtual.)

THE CONTROL, AND WHY IT IS NOT OPTIONAL
  This detector CAN legitimately fire on real functions: anything reached only
  through a function pointer (`bctrl` off a table) has no `bl` edge either. So
  the rate on the untreated population -- named rows objdiff scores at
  mpn == 100, which are correctly paired by construction -- is the noise floor,
  and the treated rate is only interesting if it is far above it.

  Unlike the funclet detector, a NONZERO control here is EXPECTED and does not
  invalidate the screen; what would invalidate it is the two rates being equal.
  The tool prints the enrichment and refuses to call anything a candidate if the
  enrichment is below --min-enrichment.

  A separate self-check runs first: the bl decoder must find a plausible number
  of edges and must score known-called functions above zero. A decoder that
  silently finds nothing would make every row look like a false pairing -- the
  exact "screen that fires nowhere reads as a decisive negative" failure that
  cost BODIES-3 a cycle.

*** MEASURED RESULT ON THE BODIES-4 BASELINE: THIS SCREEN DOES NOT WORK. ***
      CONTROL  named non-virtual mpn==100 :  959/10961 unreachable = 8.7492%
      TREATED  named non-virtual sub-100  :  315/ 4701 unreachable = 6.7007%
      ENRICHMENT: 0.77x
  The sub-100 rows are LESS unreachable than the correctly-paired ones, so
  "zero direct callers" carries NO information about false pairing in this
  population, and the tool refuses to print candidates. Nearly 9% of rows
  objdiff scores at mpn == 100 -- which are correct BY CONSTRUCTION -- also have
  no direct caller, because /Gy COMDATs survive on pointer-table references,
  vtables of classes we do not model, and .data references the decoder cannot
  see.

  IT IS COMMITTED AS A DRAINED VEIN, NOT AS A TOOL TO RUN. The reason to keep it
  is that "0 call sites" is an attractive-looking argument that a lane will
  re-invent: BODIES-4 used it as one of three legs to refute
  RockCentral::CancelOutstandingCalls, and only building the control revealed
  that leg was worthless. The refutation survived on its OTHER two legs. Do not
  re-fund this; if you want the enrichment re-measured, run it -- it takes
  seconds and it will refuse.

WHAT IT DOES *NOT* PROVE
  A hit is a REASON TO READ THE RETAIL BODY, not a verdict. The verdict still
  comes from the body: for RockCentral::CancelOutstandingCalls (the row this
  tool was built from) the corroborating facts were that the body never touches
  its own Hmx::Object* parameter and that its callee is mapped to an unrelated
  class. Zero call sites was one leg of three, not the finding.
"""
import argparse
import collections
import json
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

VIRTUAL_CODES = set("EFMNUV")
# thunks / compiler-generated forms where the mangling games above do not apply
THUNK_PREFIXES = ("??_9", "??_E", "??_G", "??_7", "??_8", "??_R", "??__")


def sections(data):
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    nsec = struct.unpack_from("<H", data, pe + 6)[0]
    oh = struct.unpack_from("<H", data, pe + 20)[0]
    base = struct.unpack_from("<I", data, pe + 24 + 28)[0]
    out = []
    off = pe + 24 + oh
    for _ in range(nsec):
        r = data[off : off + 40]
        vs, va, srd, praw = struct.unpack_from("<IIII", r, 8)
        out.append((base + va, max(vs, srd), praw, r[:8].rstrip(b"\0").decode("latin1")))
        off += 40
    return out


def call_graph(path):
    """-> Counter{target_va: n_direct_bl_edges} over .text."""
    data = open(path, "rb").read()
    sva = vs = praw = None
    for a, b, c, name in sections(data):
        if name == ".text":
            sva, vs, praw = a, b, c
    if sva is None:
        raise SystemExit("no .text section")
    body = data[praw : praw + vs]
    counts = collections.Counter()
    for i in range(0, (len(body) & ~3) - 3, 4):
        w = struct.unpack_from(">I", body, i)[0]
        # primary opcode 18 (b-form), LK=1 (bl), AA=0 (relative)
        if (w >> 26) == 18 and (w & 1) == 1 and (w & 2) == 0:
            li = w & 0x03FFFFFC
            if li & 0x02000000:
                li -= 0x04000000
            counts[(sva + i) + li] += 1
    return counts


def is_virtual(name):
    """Read MSVC's access/storage code -- the char after the class qualifier."""
    if not name.startswith("?"):
        return False
    i = name.find("@@")
    if i < 0 or i + 2 >= len(name):
        return False
    return name[i + 2] in VIRTUAL_CODES


def directly_callable(name):
    """True for shapes whose ordinary reference is a `bl` rather than a slot."""
    if name.startswith(THUNK_PREFIXES):
        return False
    if not name.startswith("?"):
        return False
    if "$4" in name or "$B" in name:  # this-adjustor / vcall thunks
        return False
    return not is_virtual(name)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--project-dir", default=ROOT)
    ap.add_argument("--report", default=None)
    ap.add_argument("--image", default=None)
    ap.add_argument("--min-enrichment", type=float, default=3.0)
    ap.add_argument("--limit", type=int, default=40)
    args = ap.parse_args()

    pd = os.path.abspath(args.project_dir)
    report = args.report or os.path.join(pd, "build/45410914/report.json")
    image = args.image or os.path.join(pd, "orig/45410914/band.exe")

    counts = call_graph(image)
    total_edges = sum(counts.values())
    print(f"call graph: {total_edges} direct bl edges, {len(counts)} distinct targets")

    # --- decoder self-check: must not be vacuous -------------------------------
    probes = {
        "?Save@Object@Hmx@@UAAXAAVBinStream@@@Z": 0x8275AB90,
        "?WriteEndian@BinStream@@QAAXPBXH@Z": 0x827C5098,
        "__RTDynamicCast": 0x8282A0C8,
    }
    bad = [n for n, a in probes.items() if counts.get(a, 0) == 0]
    print(f"decoder self-check: " + ", ".join(f"{n.split('@')[0]}={counts.get(a,0)}"
                                              for n, a in probes.items()))
    if total_edges < 10000 or bad:
        print("\nREFUSING: the bl decoder found nothing where it must find something. "
              "Every row would look unreachable.", file=sys.stderr)
        return 2

    n2a = collections.defaultdict(list)
    for a, n in json.load(open(os.path.join(pd, "scripts/target_symbol_map.json"))).items():
        if not a.startswith("0x"):
            continue
        for x in (n if isinstance(n, list) else [n]):
            n2a[x].append(int(a, 16))

    ctrl_hit = ctrl_tot = 0
    tr_hit = tr_tot = 0
    cands = []
    for u in json.load(open(report))["units"]:
        for f in u.get("functions", []):
            n = f.get("name", "")
            if not n or n.startswith(("fn_", "auto_")) or not directly_callable(n):
                continue
            addrs = n2a.get(n)
            if not addrs:
                continue
            edges = max(counts.get(a, 0) for a in addrs)
            # int()-coerce: report.json ships several numerics as JSON STRINGS
            mpn = float(f.get("match_percent_normalized", 0.0))
            if mpn >= 100.0:
                ctrl_tot += 1
                ctrl_hit += edges == 0
            else:
                tr_tot += 1
                if edges == 0:
                    tr_hit += 1
                    cands.append((int(f.get("size", 0)), mpn, n, u.get("name", "")))

    cr = ctrl_hit / max(ctrl_tot, 1)
    tr = tr_hit / max(tr_tot, 1)
    print(f"\nCONTROL  named non-virtual mpn==100 : {ctrl_hit}/{ctrl_tot} unreachable = {100*cr:.4f}%")
    print(f"TREATED  named non-virtual sub-100  : {tr_hit}/{tr_tot} unreachable = {100*tr:.4f}%")
    enr = (tr / cr) if cr else float("inf")
    print(f"ENRICHMENT: {enr:.2f}x  (threshold {args.min_enrichment}x)")
    if enr < args.min_enrichment:
        print("\nNOT PRINTING CANDIDATES: the sub-100 rows are no more unreachable "
              "than the matched ones, so this signal does not discriminate here.",
              file=sys.stderr)
        return 1

    cands.sort(reverse=True)
    print(f"\nCANDIDATES (sub-100, non-virtual, zero direct callers): "
          f"{len(cands)}, {sum(c[0] for c in cands)} B\n")
    for sz, mpn, n, unit in cands[: args.limit]:
        print(f"  {sz:6d} B  mpn={mpn:6.2f}  {unit}\n           {n}")
    print("\nA hit is a REASON TO READ THE RETAIL BODY, not a verdict: a function "
          "reached only\nthrough a pointer table has no bl edge either.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
