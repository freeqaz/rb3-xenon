#!/usr/bin/env python3
"""Size the ICF-alias forgiveness mechanism and adjudicate it on RETAIL BYTES.

    python3 tools/alias_forgiveness_audit.py measure    --wt <worktree>   # FULL vs EMPTY legs
    python3 tools/alias_forgiveness_audit.py adjudicate --wt <worktree>   # layered verdicts
    python3 tools/alias_forgiveness_audit.py control    --wt <worktree>   # decoy discrimination

WHY IT EXISTS (lane GROUNDED-1, 2026-08-14)
-------------------------------------------
`tools/icf_alias_finder.py --validate` classifies a group as OK from MAP-RESIDENCY.
That was quoted upstream as if it were proof of folding, over ~7 pp of matched_code.
It is not, and it is provably insensitive to the claim: a group emptied of every
folded spelling still classifies OK.  This tool answers the question the label
cannot -- how many of the forgiven BYTES are actually proven, and how many are not.

MEASURE -- the row set is measured, never modelled
--------------------------------------------------
scripts/symbol_aliases.json feeds gen_symbol_alias_map.py -> icf_aliases.map, which
objdiff reads at REPORT time.  So a leg is a map + report regeneration with ZERO
compiles, and the settling hazard that makes source A/Bs expensive does not arise --
but the leg CHECKS that rather than assuming it, and refuses if ninja compiled.
Running FULL vs EMPTY yields the exact per-row fall set.

★ ITS CONTROL: this reproduced ALIASAUDIT-1's full `ab_measure` figure -- 720,992 B
/ 6.985907 pp, matched_functions +0 -- TO THE BYTE, from a report-only leg, and the
fallen rows' sizes sum to exactly that figure.  A cheap instrument that reproduces
an expensive one's known answer is licensed; one that merely looks plausible is not.

ADJUDICATE -- layered, because flat T1 is one of five channels this tree owns
-----------------------------------------------------------------------------
  L1 T1         flat retail-byte identity + relocation TARGET NAMES  (icf_alias_build)
  L2 RECURSIVE  the /OPT:ICF fixpoint, name equality closed under proven folds (chase)
  L3 EXACT      full-word compare with NO vacuity floor (comdat_fold_gate's comparator)
  L4 OURSIDE    our two COMDATs byte+reloc identical => the linker MUST fold them
  L5 INCONSIST. retail's own callees name >=2 instantiations of one family -- proof by
                internal inconsistency, which needs no fold MODEL at all

⚠ FLAT T1'S VACUITY GUARD IS RIGHT AS A GUARD AND WRONG AS A VERDICT.  It exists so a
masked `b X` cannot compare equal to `b Y`.  But when the relocation target NAMES
agree the destination is not masked at all, and for a thunk the destination is the
entire information content -- the strongest test available, not the weakest.  And a
body with NO relocation has nothing masked, so byte identity IS /OPT:ICF's complete
criterion.  Using flat T1 alone reports 55.5% proven; layered reports 82.51%.

⛔ WHAT NO LAYER CAN RECOVER.  For a relocation-free thunk (`lwz r3,0x14(r3); blr`)
byte identity proves the linker folded the bodies, but WHICH NAME THE CALL SITE MEANT
WAS DESTROYED BY ICF ITSELF.  The objdiff-level claim holds; the source-level one is
irrecoverable from the image by any tool.  Reported separately -- it is an
irreducible remainder, not an unfunded one.

CONTROL -- both new rules are fired at decoys whose answer is known
------------------------------------------------------------------
4,000 random same-size non-alias pairs: L3 says YES on 0.07%, L4 on 0.05% (an upper
bound on false positives -- some decoys genuinely fold).  A rule that also accepted
the decoys would be proving nothing, and ~18 instruments in this project's history
were caught unable to fail.

Full write-up: docs/decomp/ALIAS_FORGIVENESS_SIZED_2026-08-14.md
"""
import argparse, collections, glob, hashlib, json, random, re, struct, subprocess, sys
from pathlib import Path

OPMASK_B = 0xFC000003          # opcode + AA + LK, the bits a branch reloc cannot change


# ---------------------------------------------------------------- measure ----
def measures_and_rows(wt):
    d = json.loads((wt / "build/45410914/report.json").read_text())
    m = d["measures"]
    out = {k: int(m[k]) if isinstance(m.get(k), str) else m.get(k)
           for k in ("matched_functions", "matched_code", "total_code",
                     "masked_equal_functions")}
    out["matched_code_percent"] = float(m["matched_code_percent"])
    rows = {}
    for u in d["units"]:
        for f in u.get("functions", []):
            if f.get("name"):
                rows[(u["name"], f["name"])] = (float(f.get("fuzzy_match_percent", 0) or 0),
                                                int(f.get("size", 0) or 0))
    return out, rows


def leg(wt, keep_all, label):
    ali = wt / "scripts/symbol_aliases.json"
    backup = ali.read_bytes()
    sha0 = hashlib.sha256(backup).hexdigest()
    doc = json.loads(backup)
    try:
        if not keep_all:
            doc["groups"] = []
            ali.write_text(json.dumps(doc, indent=1) + "\n")
        for p in ("build/45410914/report.json", "build/45410914/report.cache"):
            (wt / p).unlink(missing_ok=True)
        r = subprocess.run("./tools/ninja-locked build/45410914/report.json", cwd=wt,
                           shell=True, capture_output=True, text=True)
        if r.returncode != 0:
            sys.exit("BUILD FAILED (%s):\n%s" % (label, (r.stdout + r.stderr)[-3000:]))
        log = r.stdout + r.stderr
        n = len(re.findall(r"^\[\d+/\d+\] .*(cl\.exe|objcache)", log, re.M))
        if n:
            sys.exit("REFUSING: leg %s recompiled %d TUs -- this probe is licensed only "
                     "for report-only legs; a compile means the two legs differ in more "
                     "than the alias map." % (label, n))
        return measures_and_rows(wt)
    finally:                                     # restore on EVERY exit path
        ali.write_bytes(backup)
        if hashlib.sha256(ali.read_bytes()).hexdigest() != sha0:
            sys.exit("FATAL: failed to restore scripts/symbol_aliases.json")


def cmd_measure(wt, out):
    fm, fr = leg(wt, True, "FULL")
    em, er = leg(wt, False, "EMPTY")
    fell = [(k, sz) for k, (fz, sz) in fr.items()
            if fz == 100.0 and (k not in er or er[k][0] < 100.0)]
    rose = [k for k, (fz, sz) in fr.items() if fz < 100.0 and er.get(k, (0,))[0] == 100.0]
    tot = sum(sz for _, sz in fell)
    d = fm["matched_code"] - em["matched_code"]
    print("FULL  matched_code %d (%.6f%%)" % (fm["matched_code"], fm["matched_code_percent"]))
    print("EMPTY matched_code %d (%.6f%%)" % (em["matched_code"], em["matched_code_percent"]))
    print("DELTA %d B / %.6f pp   matched_functions %+d   masked_equal %+d"
          % (d, fm["matched_code_percent"] - em["matched_code_percent"],
             fm["matched_functions"] - em["matched_functions"],
             fm["masked_equal_functions"] - em["masked_equal_functions"]))
    print("rows that fell: %d, totalling %d B -- %s" %
          (len(fell), tot, "RECONCILES" if tot == d else "MISMATCH (%d)" % (tot - d)))
    print("rows that rose: %d" % len(rose))
    if tot != d:
        sys.exit("REFUSING: the fallen rows do not sum to the measured delta; the row "
                 "set cannot be used for attribution.")
    json.dump([["\t".join(k), sz] for k, sz in fell], open(out, "w"))
    print("wrote %s" % out)


# ------------------------------------------------------------- adjudicate ----
class Sides:
    def __init__(self, wt):
        sys.path.insert(0, str(wt / "tools")); sys.path.insert(0, str(wt))
        from icf_alias_build import collect, placeholder                  # noqa
        from coff_bodies_ext import function_bodies_ext                   # noqa
        self.placeholder = placeholder
        tobj = sorted(glob.glob(str(wt / "build/45410914/obj/**/*.obj"), recursive=True))
        oobj = sorted(glob.glob(str(wt / "build/45410914/src/**/*.obj"), recursive=True))
        self.tgt, self.ours = collect(tobj, "retail"), collect(oobj, "ours")

        def raws(paths):
            o = {}
            for p in paths:
                for n, raw, rel, _e in function_bodies_ext(Path(p)):
                    o.setdefault(n, (raw, rel))
            return o
        self.traw, self.oraw = raws(tobj), raws(oobj)
        self.mapped = set()
        for a, n in json.loads((wt / "scripts/target_symbol_map.json").read_text()).items():
            for x in (n if isinstance(n, list) else [n]):
                if x:
                    self.mapped.add(x)
        self.eq = collections.defaultdict(set)
        for g in json.loads((wt / "scripts/symbol_aliases.json").read_text())["groups"]:
            mem = set([g["survivor"]] + list(g["folded"]))
            for x in mem:
                self.eq[x] |= mem

    def equiv(self, a, b):
        return a == b or b in self.eq.get(a, ()) or a in self.eq.get(b, ())

    def l3_exact(self, tn, bn):
        rt, ob = self.traw.get(tn), self.oraw.get(bn)
        if rt is None or ob is None:
            return None, "absent"
        rraw, rrel = rt; oraw_, orel = ob
        if len(rraw) != len(oraw_):
            return "NO", "size %d vs %d" % (len(rraw), len(oraw_))
        rm = {o: (n, t) for o, n, t in rrel}; om = {o: (n, t) for o, n, t in orel}
        if set(rm) != set(om):
            return "NO", "relocated offsets differ"
        for i in range(0, len(rraw), 4):
            rw = struct.unpack_from(">I", rraw, i)[0]
            ow = struct.unpack_from(">I", oraw_, i)[0]
            if i not in rm:
                if rw != ow:
                    return "NO", "word differs @0x%x" % i
                continue
            (rn, rty), (on, oty) = rm[i], om[i]
            if rty != oty or (rw & OPMASK_B) != (ow & OPMASK_B):
                return "NO", "reloc type/opcode differs @0x%x" % i
            if self.equiv(rn, on):
                continue
            if self.placeholder(rn) or self.placeholder(on):
                return "UNRESOLVED", "unidentified target %s @0x%x" % (rn, i)
            return "NO", "target names differ @0x%x: %s vs %s" % (i, rn[:50], on[:50])
        return "YES", "every word compared in full; every relocated target name agrees"

    def l4_ourside(self, tn, bn):
        a, b = self.oraw.get(tn), self.oraw.get(bn)
        if a is None or b is None:
            return None, "we do not compile both spellings"
        if a[0] != b[0]:
            return "NO", "our two COMDATs differ in bytes"
        if len(a[1]) != len(b[1]) or any(
                o1 != o2 or t1 != t2 or not self.equiv(n1, n2)
                for (o1, n1, t1), (o2, n2, t2) in zip(a[1], b[1])):
            return "NO", "our two COMDATs differ in relocations"
        rt, ob = self.tgt.get(tn), self.ours.get(bn)
        if rt is None:
            return "UNRESOLVED", "our COMDATs identical but retail's body is unpinned"
        if rt[2] != ob[2] or rt[0] != ob[0]:
            return "NO", "retail's body at the survivor does not corroborate"
        return "YES", "our COMDATs are byte+reloc identical => /OPT:ICF must fold; retail corroborates"

    # -- L5: proof by internal inconsistency -------------------------------
    @staticmethod
    def _base(n):
        m = re.match(r"\?\?[\$]?([A-Za-z_0-9]+)@", n) or re.match(r"\?([A-Za-z_0-9]+)@", n)
        if m:
            return m.group(1)
        m = re.match(r"(\?\?[A-Z_0-9]+)", n)
        return m.group(1) if m else n[:6]

    @staticmethod
    def _family(n):
        # ⚠ a leading `??$` is the FUNCTION-template prefix (??$_M_find@H@?$hashtable@..).
        # Searching from 0 returns the METHOD as the "family", which made two callees of
        # one hashtable look unrelated and SILENTLY refused a provable fold. Skip it.
        s = 3 if n.startswith("??$") else 0
        m = re.search(r"\?\$([A-Za-z_0-9]+)@", n[s:])
        return m.group(1) if m else None

    @staticmethod
    def _targ(n):
        s = 3 if n.startswith("??$") else 0
        i = n.find("?$", s)
        if i < 0:
            return None
        j = n.find("@", i + 2)
        return n[j:] if j > 0 else None

    def l5_inconsistent(self, tn, bn):
        rt, ob = self.traw.get(tn), self.oraw.get(bn)
        if rt is None or ob is None or len(rt[0]) != len(ob[0]):
            return "NO", "different body SIZE -- cannot be one COMDAT"
        for (ro, rn, _), (oo, on, _) in zip(rt[1], ob[1]):
            if rn == on or self.placeholder(rn) or self.placeholder(on):
                continue
            if self._base(rn) == self._base(on) and self._family(rn) == self._family(on):
                continue                      # same method of same family, different T
            return "NO", "DIFFERENT METHOD called: %s vs %s" % (rn[:60], on[:60])
        fams = collections.defaultdict(set)
        for _o, n, _t in rt[1]:
            f = self._family(n)
            if f:
                fams[f].add(self._targ(n))
        ev = [(f, len(v)) for f, v in fams.items() if len(v) > 1]
        if ev:
            return "YES", "retail's own callees name >=2 instantiations of one family %s" % ev[:2]
        return "UNPROVEN", "template-twin slots only; retail shows no internal inconsistency"

    def verdict(self, tn, bn, memo):
        from tools.icf_pair_adjudicate import adjudicate, chase           # noqa
        v, d = adjudicate(self.tgt, self.ours, tn, bn, self.mapped, verbose=False)
        if v == "PROVEN":
            return "L1_T1", "flat T1"
        if v == "REFUTED" and "relocation TARGETS" in d.get("why", ""):
            if chase(self.tgt, self.ours, tn, bn, self.mapped, out=[], memo=memo):
                return "L2_RECURSIVE", "ICF fixpoint via chase"
        e, er = self.l3_exact(tn, bn)
        if e == "YES":
            return "L3_EXACT", er
        o, orr = self.l4_ourside(tn, bn)
        if o == "YES":
            return "L4_OURSIDE", orr
        i, ir = self.l5_inconsistent(tn, bn)
        if i == "YES":
            return "L5_INCONSISTENCY", ir
        if e == "UNRESOLVED" or o == "UNRESOLVED":
            return "NEEDS_MAP_ID", er if e == "UNRESOLVED" else orr
        if e is None:
            return "NEEDS_SOURCE", er
        return "CONTRADICTED", "%s | %s" % (er, ir)


PROVEN = {"L1_T1", "L2_RECURSIVE", "L3_EXACT", "L4_OURSIDE", "L5_INCONSISTENCY"}


def cmd_adjudicate(wt, fellp, sitesp):
    S = Sides(wt)
    fell = [(tuple(k.split("\t")), sz) for k, sz in json.load(open(fellp))]
    cen = json.load(open(sitesp))
    groups = json.loads((wt / "scripts/symbol_aliases.json").read_text())["groups"]
    m2g = collections.defaultdict(set)
    for i, g in enumerate(groups):
        for n in [g["survivor"]] + list(g["folded"]):
            m2g[n].add(i)
    recs = {(u, f): s for u, f, s in cen["records"]}

    memo = {}
    vcache, rcache = {}, {}
    cls_rows, cls_bytes = collections.Counter(), collections.Counter()
    pair_bytes = collections.Counter()
    for key, sz in fell:
        pairs = {(tn, bn) for _k, tn, bn in (recs.get(key) or [])
                 if tn != bn and (m2g.get(tn, set()) & m2g.get(bn, set()))}
        if not pairs:
            cls_rows["UNATTRIBUTED"] += 1; cls_bytes["UNATTRIBUTED"] += sz; continue
        vs = set()
        for p in pairs:
            if p not in vcache:
                vcache[p], rcache[p] = S.verdict(p[0], p[1], memo)
            vs.add(vcache[p]); pair_bytes[p] += sz
        c = ("CONTRADICTED" if "CONTRADICTED" in vs else
             "PROVEN" if vs <= PROVEN else
             "NEEDS_MAP_ID" if "NEEDS_MAP_ID" in vs else "NEEDS_SOURCE")
        cls_rows[c] += 1; cls_bytes[c] += sz
    tot = sum(cls_bytes.values())
    print("\nROW-LEVEL SPLIT OF THE %d ALIAS-FORGIVEN BYTES" % tot)
    for c in ("PROVEN", "NEEDS_SOURCE", "NEEDS_MAP_ID", "CONTRADICTED", "UNATTRIBUTED"):
        print("  %-13s %5d rows %9d B  %5.2f%%" % (c, cls_rows[c], cls_bytes[c],
                                                   100.0 * cls_bytes[c] / tot))
    ch = collections.Counter()
    for p, b in pair_bytes.items():
        ch[vcache[p]] += b
    print("\nPROOF CHANNEL (pair-bytes; double-counts rows with >1 charged pair):")
    for c, b in ch.most_common():
        print("   %-18s %9d B" % (c, b))
    print("\nCONTRADICTED pairs:")
    for p, b in sorted(pair_bytes.items(), key=lambda x: -x[1]):
        if vcache[p] == "CONTRADICTED":
            print("  %6d B  %s\n            <- %s\n            %s"
                  % (b, p[0][:92], p[1][:92], rcache[p][:120]))


def cmd_control(wt, n=4000):
    """Fire L3/L4 at decoys whose answer is known. A rule that cannot fail proves nothing."""
    S = Sides(wt)
    members = set()
    for g in json.loads((wt / "scripts/symbol_aliases.json").read_text())["groups"]:
        members.add(g["survivor"]); members.update(g["folded"])
    random.seed(20260814)
    bt, bo = collections.defaultdict(list), collections.defaultdict(list)
    for nm, (raw, _r) in S.traw.items():
        if nm not in members and not S.placeholder(nm):
            bt[len(raw)].append(nm)
    for nm, (raw, _r) in S.oraw.items():
        if nm not in members and not S.placeholder(nm):
            bo[len(raw)].append(nm)
    sizes = [s for s in bt if s in bo]
    y3 = y4 = 0; tried = 0
    while tried < n and sizes:
        s = random.choice(sizes)
        a, b = random.choice(bt[s]), random.choice(bo[s])
        if a == b:
            continue
        tried += 1
        y3 += S.l3_exact(a, b)[0] == "YES"
        y4 += S.l4_ourside(a, b)[0] == "YES"
    print("DECOY CONTROL over %d random same-size non-alias pairs:" % tried)
    print("  L3_EXACT   YES on %d (%.2f%%)" % (y3, 100.0 * y3 / tried))
    print("  L4_OURSIDE YES on %d (%.2f%%)" % (y4, 100.0 * y4 / tried))
    print("  (upper bounds on false positives -- some decoys genuinely fold)")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("cmd", choices=("measure", "adjudicate", "control"))
    ap.add_argument("--wt", required=True, help="worktree to operate in")
    ap.add_argument("--fell", default="", help="fallen-row JSON from `measure`")
    ap.add_argument("--sites", default="", help="icf_site_census.py --out JSON")
    a = ap.parse_args()
    wt = Path(a.wt).resolve()
    if a.cmd == "measure":
        cmd_measure(wt, a.fell or str(Path.home() / "tmp" / "alias_fell.json"))
    elif a.cmd == "adjudicate":
        if not (a.fell and a.sites):
            sys.exit("adjudicate needs --fell (from `measure`) and --sites "
                     "(python3 tools/icf_site_census.py --out ...)")
        cmd_adjudicate(wt, a.fell, a.sites)
    else:
        cmd_control(wt)


if __name__ == "__main__":       # ALIAS-2: this call was UNGUARDED, so the module
    main()                       # could not be imported -- `import Sides` ran main()
                                 # and died in argparse on the importer's argv.
