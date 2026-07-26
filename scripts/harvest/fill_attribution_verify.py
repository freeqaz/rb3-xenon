#!/usr/bin/env python3
"""fill_attribution_verify.py — adversarial cross-check of ``pdata_parent_owner``
verdicts, and a static repair-survival model for mis-attributed splits fills.

WHY
---
``scripts/harvest/pdata_parent_owner.py`` derives a *hard* owner for an EH funclet
by walking ``.pdata`` -> ``_s_FuncInfo`` -> unwind/try maps to its parent function,
then joining the parent's VA against ``config/45410914/splits.txt`` ``.text`` spans.
That last join is the weak link: **a pinned span can OVER-COVER**.  Retail
interleaves TUs, so unit A's pinned range can physically contain code owned by
unit B, and then a "CONTRADICTED" verdict is a bad *pin*, not a bad *fill*.

This module re-adjudicates every verdict with four channels, none of which is the
pin:

  A  obj-definition   which COMPILED source obj(s) DEFINE the parent's mangled
                      name (COFF symbol tables of build/45410914/src/**/*.obj)
  A2 claim-exclusion  does the CLAIMED unit's obj define it at all?  (only counted
                      when that obj is substantive, >= 50 defined symbols)
  B  byte confirmation the parent's own objdiff match% under the pinned unit
  C  span health      do the OTHER named functions in the parent's pinned span
                      resolve to the same source obj?  (direct over-cover probe)
  D  name semantics   does the mangled name name the PINNED unit's class or the
                      CLAIMED unit's class?

★ JOIN DISCIPLINE.  Never join splits headers to units by basename: 45 of the 912
headers share a basename, and 27 *pairs of headers pin the same compiled source
obj* (e.g. ``SongDB.cpp`` and ``band3/game/SongDB.cpp`` both -> 
``build/45410914/src/band3/game/SongDB.obj``).  Join header -> objdiff.json unit by
``target_path == build/45410914/obj/<header with .cpp->.obj>`` (exact, 912/912) and
compare ownership at the level of ``base_path`` (the compiled source obj).  Doing
this by header produced 14 spurious refutations in the first pass of this audit.

MEASURED (2026-07-26, laneAM ``docs/plans/laneAM/fills-T1.json``, 1,627 fills)
-----------------------------------------------------------------------------
Of the 685 fills the parent map decides against PRE-laneAM pins::

    pin verdict CONTRADICTED  88 ->  47 CONFIRMED / 40 UNDECIDABLE /  1 REFUTED
    pin verdict PROVEN       597 -> 487 CONFIRMED / 110 UNDECIDABLE /  0 REFUTED

The single refutation is the predicted failure mode: ``OSCMessenger.cpp`` carries a
one-function ``.text`` micro-pin [0x825C5F00,0x825C5FB8) whose only function is
``?SetBestBattleScore@AppLabel@@QAAXPAVHamProfile@@H@Z`` and whose right neighbour
is AppLabel's own span at 0x825C5FE0 — the pin over-covers, laneAM was right.
So **over-cover explains 1 of 88 contradictions (1.1%), not the bulk of them.**

REPAIR SURVIVAL
---------------
A mis-attributed fill is byte-true but credited to the wrong unit.  Moving it to
its true owner keeps the match only if the owner's COMPILED obj holds a free
funclet-shaped symbol with the same reloc-masked signature (objdiff
``funclet_signature``: zero the 4-byte instruction word at every relocation inside
the symbol; ``objdiff-core/src/diff/mod.rs``).  ``survival`` implements that
statically and was validated against a real whole-binary A/B: **47/47 per-fill
predictions correct**, 11 survive, 36 lose, whole-binary delta **-34** strict
matches (2 collateral gains elsewhere).  Repairing is therefore net-negative;
the honest action is to annotate, not to move.

USAGE
    python3 scripts/harvest/fill_attribution_verify.py index --out objsyms.json
    python3 scripts/harvest/fill_attribution_verify.py adjudicate \
        --audit <pdata_parent_owner audit --json output> --objsyms objsyms.json \
        --splits /path/to/pre-edit/splits.bak --out verdicts.json
    python3 scripts/harvest/fill_attribution_verify.py survival \
        --verdicts verdicts.json --out repair.json
"""
from __future__ import annotations
import argparse, bisect, collections, json, os, re, struct, sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
FUNCLET = re.compile(
    r"^(__unwind\$\d+|__catch\$\d+|__unwind__merged_[0-9A-Fa-f]+|fn_[0-9A-Fa-f]{8}|\?\?__E.*|\?\?__F.*)$")


# ------------------------------------------------------------------ COFF
class Coff:
    """Minimal MS-COFF reader: defined symbols, section bytes, relocation offsets."""

    def __init__(self, path):
        d = self.d = open(path, "rb").read()
        nsec = struct.unpack_from("<H", d, 2)[0]
        symptr, nsym = struct.unpack_from("<II", d, 8)
        self.strtab = symptr + nsym * 18
        self.secs = []
        for i in range(nsec):
            o = 20 + i * 40
            vs, va, sz, ptr, rptr, lptr, nr, nl, fl = struct.unpack_from("<IIIIIIHHI", d, o + 8)
            self.secs.append(dict(size=sz, ptr=ptr, rptr=rptr, nreloc=nr, flags=fl))
        self.syms = []
        i = 0
        while i < nsym:
            o = symptr + i * 18
            raw = d[o:o + 8]
            if raw[:4] == b"\0\0\0\0":
                off = struct.unpack_from("<I", raw, 4)[0]
                e = d.index(b"\0", self.strtab + off)
                name = d[self.strtab + off:e].decode("latin1")
            else:
                name = raw.rstrip(b"\0").decode("latin1")
            val, sec, typ, cls, naux = struct.unpack_from("<IhHBB", d, o + 8)
            self.syms.append((name, val, sec, cls))
            i += 1 + naux
        self.bysec = collections.defaultdict(list)
        for name, val, sec, cls in self.syms:
            if sec > 0 and cls in (2, 3) and not name.startswith("."):
                self.bysec[sec].append((val, name))
        for v in self.bysec.values():
            v.sort()
        self.relocs = {}
        for si, s in enumerate(self.secs, 1):
            self.relocs[si] = [struct.unpack_from("<I", d, s["rptr"] + i * 10)[0]
                               for i in range(s["nreloc"])]

    def defined(self):
        return {n for n, v, s, c in self.syms if s > 0 and c in (2, 3) and not n.startswith(".")}

    def is_code(self, si):
        return bool(self.secs[si - 1]["flags"] & 0x20)

    def extent(self, si, val):
        s = self.secs[si - 1]
        nxt = s["size"]
        for v, n in self.bysec[si]:
            if v > val:
                nxt = v
                break
        return val, nxt

    def signature(self, si, start, end):
        """objdiff funclet_signature: bytes with every reloc-covered word zeroed."""
        s = self.secs[si - 1]
        if s["ptr"] == 0 or end <= start or end > s["size"]:
            return None
        b = bytearray(self.d[s["ptr"] + start:s["ptr"] + end])
        for a in self.relocs[si]:
            if start <= a < end:
                o = a - start
                for k in range(o, min(o + 4, len(b))):
                    b[k] = 0
        return bytes(b)

    def sig_of(self, name, size=None):
        for si, lst in self.bysec.items():
            for val, n in lst:
                if n == name:
                    st, en = self.extent(si, val)
                    return self.signature(si, st, st + size if size else en)
        return None

    def funclets(self):
        out = []
        for si, lst in self.bysec.items():
            if not self.is_code(si):
                continue
            for val, name in lst:
                if not FUNCLET.match(name):
                    continue
                st, en = self.extent(si, val)
                sg = self.signature(si, st, en)
                if sg:
                    out.append((name, sg))
        return out


PAD_OK = lambda t: (not t) or all(b == 0 for b in t) or (
    len(t) % 4 == 0 and all(t[i:i + 4] == b"\x60\x00\x00\x00" for i in range(0, len(t), 4)))


def prefix_ok(base_sig, tgt_sig):
    n = len(tgt_sig)
    return len(base_sig) >= n and base_sig[:n] == tgt_sig and PAD_OK(base_sig[n:])


# --------------------------------------------------------------- joins
def unit_tables(repo: Path):
    """header('Foo.cpp') -> dict(unit=objdiff name, base=compiled obj, target=target obj)."""
    oj = json.load(open(repo / "objdiff.json"))
    t = {}
    for u in oj["units"]:
        tp = u["target_path"]
        if not tp.startswith("build/45410914/obj/"):
            continue
        t[tp[len("build/45410914/obj/"):]] = dict(unit=u["name"], base=u.get("base_path"), target=tp)
    return t


def objkey(header):
    return re.sub(r"\.(cpp|c|cc)$", ".obj", header)


def load_text_spans(path):
    units = collections.defaultdict(list)
    cur = None
    for ln in open(path):
        if not ln.strip() or ln.strip().startswith("#"):
            continue
        if not ln[0].isspace():
            m = re.match(r"^(\S+):", ln.strip())
            cur = m.group(1) if m else cur
            continue
        m = re.match(r"\s*\.text\s+start:(0x[0-9A-Fa-f]+)\s+end:(0x[0-9A-Fa-f]+)", ln)
        if m and cur:
            units[cur].append((int(m.group(1), 16), int(m.group(2), 16)))
    return units


# ------------------------------------------------------------- commands
def cmd_index(args):
    root = Path(args.repo) / "build/45410914/src"
    idx = {}
    objs = sorted(root.rglob("*.obj"))
    for p in objs:
        rel = "build/45410914/src/" + str(p.relative_to(root))
        for s in Coff(p).defined():
            idx.setdefault(s, []).append(rel)
    json.dump(idx, open(args.out, "w"))
    print("objs=%d distinct-symbols=%d -> %s" % (len(objs), len(idx), args.out))


def cmd_adjudicate(args):
    repo = Path(args.repo)
    tbl = unit_tables(repo)
    objidx = json.load(open(args.objsyms))
    symmap = json.load(open(repo / "scripts/target_symbol_map.json"))
    rep = json.load(open(repo / "build/45410914/report.json"))
    pct_by = collections.defaultdict(dict)
    for u in rep["units"]:
        for f in u.get("functions", []):
            pct_by[u["name"]][f["name"]] = f.get("match_percent_normalized")
    nsyms = collections.Counter()
    for s, objs in objidx.items():
        for o in objs:
            nsyms[o] += 1
    US = load_text_spans(args.splits)
    spans = sorted((s, e, u) for u, v in US.items() for s, e in v)
    starts = [x[0] for x in spans]
    fvas = sorted(int(k, 16) for k in symmap if not k.startswith("_"))

    def stem(h):
        return h.split("/")[-1].rsplit(".", 1)[0]

    rows = []
    for e in json.load(open(args.audit)):
        p = int(e["parent"], 16)
        C, PU = e["claimed_by"], e["parent_unit"]
        cb = (tbl.get(objkey(C)) or {}).get("base")
        pb = (tbl.get(objkey(PU)) or {}).get("base")
        pu_unit = (tbl.get(objkey(PU)) or {}).get("unit")
        nm = symmap.get("0x%08x" % p)
        bs = set(objidx.get(nm) or []) if nm else set()
        pct = pct_by.get(pu_unit, {}).get(nm) if nm else None
        i = bisect.bisect_right(starts, p) - 1
        span = spans[i] if i >= 0 and spans[i][0] <= p < spans[i][1] else None
        # C: span health
        elsewhere = named = 0
        if span:
            j = bisect.bisect_left(fvas, span[0])
            while j < len(fvas) and fvas[j] < span[1]:
                n2 = symmap.get("0x%08x" % fvas[j]); j += 1
                if not n2:
                    continue
                named += 1
                b2 = set(objidx.get(n2) or [])
                if b2 and pb not in b2:
                    elsewhere += 1
        pts, why = 0, []
        if nm and bs:
            if len(bs) == 1:
                b = next(iter(bs))
                if b == pb: pts += 3; why.append("A: name defined by exactly one compiled obj = PINNED unit")
                elif b == cb: pts -= 5; why.append("A: name resolves to the CLAIMED unit -> pin OVER-COVERS")
                else: pts -= 2; why.append("A: name resolves to a third obj")
            elif pb in bs: pts += 1; why.append("A: COMDAT in %d objs incl. the PINNED unit" % len(bs))
            elif cb in bs: pts -= 5; why.append("A: COMDAT set includes the CLAIMED unit, not the pin")
            else: pts -= 2; why.append("A: COMDAT set includes neither")
            if cb and cb not in bs and nsyms[cb] >= 50:
                pts += 1; why.append("A2: CLAIMED unit's obj (%d syms) does NOT define it" % nsyms[cb])
        if pct == 100.0: pts += 2; why.append("B: parent is a 100% match under the pinned unit")
        elif pct: pts += 1; why.append("B: parent partially matches (%.2f%%) under the pinned unit" % pct)
        if elsewhere: pts -= 2; why.append("C: %d named fn(s) in the span resolve elsewhere" % elsewhere)
        elif named >= 3: pts += 1; why.append("C: all %d named fns in the span resolve to the pinned unit" % named)
        segs = set(re.findall(r"[A-Za-z_][A-Za-z0-9_]*", nm or ""))
        if stem(C) in segs and stem(PU) not in segs:
            pts -= 5; why.append("D: the name names the CLAIMED unit's class -> pin OVER-COVERS")
        elif stem(PU) in segs and stem(C) not in segs:
            pts += 2; why.append("D: the name names the PINNED unit's class/type")
        rows.append({**e, "parent_name": nm, "parent_def_objs": sorted(bs), "parent_pct": pct,
                     "claimed_base": cb, "owner_base": pb, "span_named": named,
                     "span_resolves_elsewhere": elsewhere, "indep_score": pts, "indep_why": why,
                     "indep_verdict": "CONFIRMED" if pts >= 3 else ("REFUTED" if pts <= -2 else "UNDECIDABLE")})
    json.dump(rows, open(args.out, "w"), indent=1)
    for v in ("CONTRADICTED", "PROVEN"):
        rs = [r for r in rows if r["verdict"] == v]
        print("pin=%-13s n=%4d -> %s" % (v, len(rs), dict(collections.Counter(r["indep_verdict"] for r in rs).most_common())))
    print("-> %s" % args.out)


def cmd_survival(args):
    repo = Path(args.repo)
    tbl = unit_tables(repo)
    cache = {}

    def coff(p):
        if p not in cache:
            cache[p] = Coff(repo / p) if (repo / p).exists() else None
        return cache[p]

    out = []
    for r in json.load(open(args.verdicts)):
        if r["verdict"] != "CONTRADICTED" or r["indep_verdict"] != "CONFIRMED":
            continue
        C, P, va, sz = r["claimed_by"], r["parent_unit"], int(r["va"], 16), r["size"]
        tC = coff("build/45410914/obj/" + objkey(C))
        sig = tC.sig_of("fn_%08X" % va, sz) if tC else None
        if sig is None:
            out.append(dict(va=r["va"], survives=None, reason="target signature not recoverable")); continue
        bP = coff((r["owner_base"] or "")) if r["owner_base"] else None
        tP = coff("build/45410914/obj/" + objkey(P))
        bC = coff(r["claimed_base"]) if r["claimed_base"] else None
        supply = sum(1 for n, s in bP.funclets() if prefix_ok(s, sig)) if bP else 0
        demand = sum(1 for n, s in tP.funclets() if prefix_ok(s, sig) or prefix_ok(sig, s)) if tP else 0
        calib = sum(1 for n, s in bC.funclets() if prefix_ok(s, sig)) if bC else 0
        out.append(dict(va=r["va"], size=sz, claimed_by=C, true_owner=P,
                        supply_in_owner_obj=supply, demand_in_owner_target=demand,
                        calibration_supply_in_claimed_obj=calib, survives=supply > demand))
    json.dump(out, open(args.out, "w"), indent=1)
    print("fills=%d survive=%d lose=%d  calibration(claimed obj has it)=%d/%d"
          % (len(out), sum(1 for r in out if r.get("survives")),
             sum(1 for r in out if r.get("survives") is False),
             sum(1 for r in out if r.get("calibration_supply_in_claimed_obj", 0) >= 1), len(out)))
    print("-> %s" % args.out)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--repo", default=str(REPO))
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("index"); p.add_argument("--out", required=True)
    p = sub.add_parser("adjudicate")
    p.add_argument("--audit", required=True); p.add_argument("--objsyms", required=True)
    p.add_argument("--splits", required=True); p.add_argument("--out", required=True)
    p = sub.add_parser("survival")
    p.add_argument("--verdicts", required=True); p.add_argument("--out", required=True)
    a = ap.parse_args()
    {"index": cmd_index, "adjudicate": cmd_adjudicate, "survival": cmd_survival}[a.cmd](a)


if __name__ == "__main__":
    main()
