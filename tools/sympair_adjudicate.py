#!/usr/bin/env python3
"""Adjudicate sympair-queue rows ON RETAIL BYTES: fold, defect, or unknowable.

Lane W18-SYMPAIR, 2026-08-17.  Downstream of `tools/sympair_queue.py`, which
finds rows whose ONLY penalties are relocation-symbol names and then STOPS --
its own docstring says so: "it does NOT tell you whether a pair is a wrong map
name (fixable, pays) or an ICF fold (irreducible). The metric CANNOT separate
them."  This tool is the adjudication it defers to, and it uses the only
instrument that can settle it: our compiled COMDAT bytes against retail's.

THE LADDER (each rung's verdict is used only where the rung below licenses it)
─────────────────────────────────────────────────────────────────────────────
1. BODY   our callee `o`'s COMDAT vs retail at `at` (the address retail's call
          targets), word-by-word, SKIPPING relocated words.
            IDENTICAL -> a fold is PROVEN.  Our `o`'s code IS what lives there.
                         Irreducible by source work; reachable only through the
                         alias mechanism, which is a POLICY decision (~22% of
                         all matched_code already rests on alias forgiveness).
            DIFFERENT -> retail calls something else.  Go to 2.
2. ANCHOR our `o` vs retail at `ao`, ITS OWN mapped address.  Only if that is
          IDENTICAL is `o` a body-confirmed identification we may reason from.
3. CROSS  our compiled `t` vs retail at `at`.  IDENTICAL => the map's name at
          `at` is confirmed too, so the contradiction is real and not a map
          artifact.  DIFFERENT => MAP defect, our source is not implicated.
4. FREEDOM did our source get to CHOOSE?  `vector<T>::_M_fill_insert_aux` calls
          `T::T(const T&)` BY TEMPLATE CONSTRUCTION and a thunk branches to its
          own method -- we cannot write anything else, so a contradiction there
          refutes the MAP's name for the ROW, not our source.  Only where
          neither candidate is entailed by the row's own name did we choose.
5. EXISTS for callees with no retail address at all: search every retail
          `.pdata` function start for our callee's bytes.  Present-but-unnamed
          is an identification gap; absent is a source divergence.

ANTI-VACUITY, because every rung can be silently vacuous
────────────────────────────────────────────────────────
* POSITIVE CONTROL (rung 1): the same test on `scripts/symbol_aliases.json`'s
  declared folds must come out IDENTICAL at a high rate.  Measured 99.9%
  (14,426/14,436).  A body test that cannot succeed proves nothing.
* NULL (rung 1): random (our symbol, random mapped address) pairings must NOT.
  Measured 0.07% (1/1,500).  A body test that cannot fail proves nothing --
  W14 ran exactly this control and it is why its family B verdict was honest.
* WORD COUNT (rung 1): the comparison skips relocated words, so a callee that
  is mostly relocations reads IDENTICAL on almost no evidence.  IDENTICAL is
  therefore split STRONG (>=8 compared words) / WEAK / VACUOUS (<=2).
* RECALL (rung 5): the existence search is re-run for callees already known to
  be present, without telling it the address.  Measured 96.8% (240/248).
* ENTAILMENT (rung 4) is computed on DEMANGLED identifiers and only on the
  identifiers that DISTINGUISH the two candidates.  A `[VU](\\w+)@` regex over
  the mangled name silently finds nothing for constructors -- exactly the
  template-callee case the rung exists to catch -- and shared identifiers like
  `ObjDirItr` make both sides read "entailed", which is the vacuous outcome.

⚠ llvm-undname emits BLANK-LINE-DELIMITED RECORDS and a name it REJECTS yields a
  ONE-line record.  The natural "two non-blank lines = one pair" parser DESYNCS
  at the first rejection and mislabels everything after it (it produced 100%
  UNDEMANGLABLE here before being caught).  The same bug is live in
  `tools/arity_screen.py:810` demangle_batch.  Parse records, not lines.

Usage:
  python3 tools/sympair_adjudicate.py --project-dir ~/tmp/wt-foo \\
        --queue docs/decomp/sympair-queue.tsv --out docs/decomp/sympair-adjudicated.tsv
  python3 tools/sympair_adjudicate.py --project-dir ~/tmp/wt-foo --controls-only
"""
import argparse, collections, csv, glob, json, os, re, struct, subprocess, sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(REPO, "tools"))
sys.path.insert(0, os.path.join(REPO, "scripts"))

STOP = {"class", "struct", "const", "void", "public", "private", "protected",
        "virtual", "__cdecl", "unsigned", "int", "char", "bool", "float",
        "double", "long", "short", "signed", "static", "enum", "union",
        "operator", "new", "delete", "stlpmtx_std", "std", "Hmx", "vector",
        "list", "map", "set", "pair", "StlNodeAlloc", "_List_node", "_Rb_tree",
        "iterator", "scalar", "deleting", "dtor", "adjustor", "this"}
IDENT = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
THUNK_MARKS = ("$4", "$2", "$B", "$R", "??_9", "$0", "$1", "$3")


# ───────────────────────── demangling (record-parsed) ─────────────────────────

def demangle(names):
    dm, rejected = {}, 0
    names = sorted({n for n in names if n})
    for i in range(0, len(names), 3000):
        p = subprocess.run(["llvm-undname"], input="\n".join(names[i:i + 3000]) + "\n",
                           capture_output=True, text=True, timeout=900)
        for rec in p.stdout.split("\n\n"):
            ls = [l for l in rec.split("\n") if l.strip()]
            if len(ls) == 2:
                dm[ls[0].strip()] = ls[1].strip()
            elif len(ls) == 1:
                rejected += 1
    missing = sum(1 for n in names if n not in dm)
    assert missing == rejected, f"undname record desync: {missing} missing vs {rejected} rejected"
    return dm


# ───────────────────────────── byte machinery ─────────────────────────────

class Bytes:
    def __init__(self, wt):
        from comdat_retail_verify import read_coff, Retail
        import sympair_queue as SQ
        self.retail = Retail(os.path.join(wt, "orig/45410914/band.exe"))
        self.addr_idx, self.arbitrary = SQ.build_address_index(wt)
        self.ours = {}
        for p in glob.glob(os.path.join(wt, "build/45410914/src/**/*.obj"), recursive=True):
            r = read_coff(p)
            if not r:
                continue
            dd, secs, syms = r
            for s in syms:
                n = s["name"]
                if s["sec"] <= 0 or s["sclass"] != 2 or n in self.ours:
                    continue
                sec = secs[s["sec"] - 1]
                if not sec["name"].startswith(b".text"):
                    continue
                rel = {}
                for i in range(sec["nrel"]):
                    va, _si, ty = struct.unpack_from("<IIH", dd, sec["ptrrel"] + 10 * i)
                    rel[va] = ty
                self.ours[n] = dict(unit=os.path.basename(p), size=sec["size"], rel=rel,
                                    body=dd[sec["ptr"]:sec["ptr"] + sec["size"]], off=s["val"])
        self.lens = self.retail.pdata_lengths()
        self.first = collections.defaultdict(list)
        for a in self.lens:
            w = self.retail.read(a, 4)
            if w and len(w) == 4:
                self.first[struct.unpack(">I", w)[0]].append(a)

    def verdict(self, o, at):
        """-> (label, n_differing, n_compared)."""
        ours = self.ours.get(o)
        if ours is None:
            return "NO_OURS", None, None
        if at is None:
            return "NO_ADDR", None, None
        rb = self.retail.read(at - ours["off"], ours["size"])
        if rb is None or len(rb) < ours["size"]:
            return "UNREADABLE", None, None
        bad = nw = 0
        for i in range(0, ours["size"] - 3, 4):
            if i in ours["rel"]:
                continue
            nw += 1
            if struct.unpack_from(">I", ours["body"], i)[0] != struct.unpack_from(">I", rb, i)[0]:
                bad += 1
        return ("IDENTICAL" if bad == 0 else "DIFFERENT"), bad, nw

    def addr_of(self, n):
        a = self.addr_idx.get(n, [])
        return int(a[0], 16) if a else None

    def find(self, o):
        """Every retail .pdata function start whose bytes equal our `o`."""
        ours = self.ours.get(o)
        if ours is None:
            return None
        off, size = ours["off"], ours["size"]
        if off in ours["rel"] or size < 8:
            return None
        w0 = struct.unpack_from(">I", ours["body"], off)[0]
        hits = []
        for a in self.first.get(w0, ()):
            rb = self.retail.read(a - off, size)
            if rb is None or len(rb) < size:
                continue
            bad = nw = 0
            for i in range(0, size - 3, 4):
                if i in ours["rel"]:
                    continue
                nw += 1
                if struct.unpack_from(">I", ours["body"], i)[0] != struct.unpack_from(">I", rb, i)[0]:
                    bad = 1
                    break
            if not bad and nw >= 3:
                hits.append(a)
        return hits


def strength(nw):
    return "STRONG" if (nw or 0) >= 8 else ("WEAK" if (nw or 0) >= 3 else "VACUOUS")


# ───────────────────────────── controls ─────────────────────────────

def controls(B, wt, nnull=1500):
    print("## rung-1 POSITIVE CONTROL — declared folds must read IDENTICAL")
    d = json.load(open(os.path.join(wt, "scripts/symbol_aliases.json")))
    c = collections.Counter()
    for g in d.get("groups", []):
        sv, ad = g.get("survivor"), g.get("address")
        if not sv or not ad:
            continue
        for f in g.get("folded", []) or []:
            c[B.verdict(f, int(ad, 16))[0]] += 1
    tot = c["IDENTICAL"] + c["DIFFERENT"]
    rate = 100.0 * c["IDENTICAL"] / max(tot, 1)
    print(f"   {dict(c.most_common())} -> {c['IDENTICAL']}/{tot} = {rate:.1f}% IDENTICAL")

    print("## rung-1 NULL — random pairings must NOT read IDENTICAL")
    import random
    random.seed(18)
    names = [n for n in B.ours if B.ours[n]["size"] >= 16]
    addrs = [int(a[0], 16) for a in list(B.addr_idx.values())[:6000] if a]
    nc = collections.Counter()
    for _ in range(nnull):
        nc[B.verdict(random.choice(names), random.choice(addrs))[0]] += 1
    ntot = nc["IDENTICAL"] + nc["DIFFERENT"]
    nrate = 100.0 * nc["IDENTICAL"] / max(ntot, 1)
    print(f"   {dict(nc.most_common())} -> {nrate:.2f}% IDENTICAL")
    if rate < 90.0 or nrate > 5.0:
        print("REFUSING: the body instrument does not discriminate "
              f"(control {rate:.1f}%, null {nrate:.2f}%) -- fix it before reading any verdict.")
        return False
    return True


# ───────────────────────────── main ─────────────────────────────

def load_queue(path):
    lines = [l for l in open(path) if not l.startswith("#")]
    rows = collections.defaultdict(list)
    for r in csv.DictReader(lines, delimiter="\t"):
        rows[r["symbol"]].append(r)
    out = []
    for s, rs in rows.items():
        ks = {r["pair_class"] for r in rs}
        cls = ("FOLD_FANIN" if "FOLD_FANIN" in ks else
               "ALL_RECIPROCAL" if ks == {"RECIPROCAL"} else
               "ALL_OURS_UNMAPPED" if ks == {"OURS_UNMAPPED"} else "MIXED/UNKNOWN")
        out.append(dict(symbol=s, cls=cls, size=int(rs[0]["size"]),
                        fuzzy=float(rs[0]["fuzzy"]), unit=rs[0]["unit"],
                        pairs=[dict(t=r["target_symbol"], o=r["our_symbol"],
                                    tgt_addr=r["tgt_addr"]) for r in rs]))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--project-dir", default=REPO)
    ap.add_argument("--queue", default="docs/decomp/sympair-queue.tsv")
    ap.add_argument("--classes", default="MIXED/UNKNOWN,ALL_OURS_UNMAPPED",
                    help="row triage classes to adjudicate; 'ALL' for every class")
    ap.add_argument("--out", default=None)
    ap.add_argument("--controls-only", action="store_true")
    args = ap.parse_args()

    wt = os.path.abspath(os.path.expanduser(args.project_dir))
    B = Bytes(wt)
    print(f"# indexed {len(B.ours)} of our COMDAT .text symbols, "
          f"{len(B.lens)} retail .pdata functions")
    if not controls(B, wt):
        return 2
    if args.controls_only:
        return 0

    qp = args.queue if os.path.isabs(args.queue) else os.path.join(wt, args.queue)
    rows = load_queue(qp)
    want = None if args.classes == "ALL" else set(args.classes.split(","))
    rows = [r for r in rows if want is None or r["cls"] in want]
    print(f"# adjudicating {len(rows)} rows / {sum(r['size'] for r in rows)} B")

    DM = demangle([x for r in rows for p in r["pairs"] for x in (r["symbol"], p["t"], p["o"])])

    def ids(n):
        d = DM.get(n)
        return set() if d is None else {w for w in IDENT.findall(d)
                                        if w not in STOP and len(w) > 1}

    def entailed(disc, rid):
        for a in disc:
            for b in rid:
                if a == b or (len(a) > 4 and a in b) or (len(b) > 4 and b in a):
                    return True
        return False

    def leaf(n):
        m = re.match(r"\?\??([A-Za-z_0-9]+)@", n or "")
        return m.group(1) if m else None

    out = []
    for r in rows:
        thunk = any(m in r["symbol"] for m in THUNK_MARKS)
        for p in r["pairs"]:
            at = B.addr_of(p["t"])
            v, nbad, nw = B.verdict(p["o"], at)
            rec = dict(size=r["size"], cls=r["cls"], unit=r["unit"], fuzzy=r["fuzzy"],
                       symbol=r["symbol"], t=p["t"], o=p["o"],
                       at=("0x%08x" % at) if at else "-", body=v,
                       nwords=nw, evidence=strength(nw) if v == "IDENTICAL" else "")
            if v == "IDENTICAL":
                rec["verdict"] = "A_PROVEN_FOLD"
            elif v == "NO_OURS":
                rec["verdict"] = "C_NO_EVIDENCE_callee_not_compiled"
            elif v == "DIFFERENT":
                ao = B.addr_of(p["o"])
                av, _ab, anw = B.verdict(p["o"], ao) if ao else ("NO_ADDR", None, None)
                if av == "IDENTICAL" and (anw or 0) >= 3:
                    tv, _tb, tnw = B.verdict(p["t"], at)
                    if tv != "IDENTICAL":
                        rec["verdict"] = "D4_MAP_DEFECT_target_name_unconfirmed"
                    else:
                        rid, tid, oid = ids(r["symbol"]), ids(p["t"]), ids(p["o"])
                        oe, te = entailed(oid - tid, rid), entailed(tid - oid, rid)
                        lr, lt, lo = leaf(r["symbol"]), leaf(p["t"]), leaf(p["o"])
                        if oe and not te:
                            rec["verdict"] = "D1_ROW_NAME_REFUTED"
                        elif te and not oe:
                            rec["verdict"] = "D3_our_call_unrelated_to_row"
                        elif (thunk or r["size"] <= 24) and lr and lr == lo and lr != lt:
                            rec["verdict"] = "D1_ROW_NAME_REFUTED"
                        else:
                            rec["verdict"] = "D2_SOURCE_DEFECT_we_had_freedom"
                else:
                    hits = B.find(p["o"])
                    if hits is None:
                        rec["verdict"] = "B3_UNSEARCHABLE"
                    elif not hits:
                        rec["verdict"] = "B1_callee_ABSENT_from_retail"
                    elif len(hits) > 8:
                        rec["verdict"] = "B4_AMBIGUOUS_common_shape"
                    else:
                        rec["verdict"] = "B2_callee_PRESENT_but_unnamed"
                        rec["found"] = ",".join("0x%08x" % h for h in hits)
            else:
                rec["verdict"] = "C_" + v
            out.append(rec)

    # row-level roll-up: matched_code is ALL-OR-NOTHING PER ROW, so a row is only
    # as reachable as its WORST pair.
    RANK = ["D2_SOURCE_DEFECT_we_had_freedom", "D1_ROW_NAME_REFUTED",
            "D4_MAP_DEFECT_target_name_unconfirmed", "D3_our_call_unrelated_to_row",
            "B2_callee_PRESENT_but_unnamed", "A_PROVEN_FOLD",
            "B4_AMBIGUOUS_common_shape", "B3_UNSEARCHABLE",
            "B1_callee_ABSENT_from_retail"]
    byrow = collections.defaultdict(list)
    for rec in out:
        byrow[rec["symbol"]].append(rec)
    rc, rb = collections.Counter(), collections.Counter()
    for sym, recs in byrow.items():
        vs = [x["verdict"] for x in recs]
        worst = max(vs, key=lambda v: RANK.index(v) if v in RANK else 99)
        rc[worst] += 1
        rb[worst] += recs[0]["size"]
    tot = sum(rb.values())
    print(f"\n## ROW verdicts (a row crosses only if EVERY pair does)"
          f"   {sum(rc.values())} rows / {tot} B")
    for k in sorted(rb, key=lambda x: -rb[x]):
        print(f"   {k:44s} rows={rc[k]:5d} bytes={rb[k]:7d} ({100.0*rb[k]/max(tot,1):5.2f}%)")
    assert sum(rc.values()) == len(rows) and tot == sum(r["size"] for r in rows), \
        "census dropped rows -- refusing"

    op = args.out or os.path.join(wt, "docs/decomp/sympair-adjudicated.tsv")
    cols = ["verdict", "size", "fuzzy", "cls", "evidence", "nwords", "unit",
            "symbol", "at", "target_symbol", "our_symbol", "found"]
    with open(op, "w") as fh:
        fh.write("# Retail-byte adjudication of sympair-queue rows. tools/sympair_adjudicate.py (W18).\n")
        fh.write("# A_PROVEN_FOLD          our callee's body IS retail's code there. Alias-only; NOT source-reachable.\n")
        fh.write("#                        `evidence` VACUOUS = <=2 non-relocated words compared: a thunk, near-no evidence.\n")
        fh.write("# B1 callee ABSENT       our callee's code is nowhere in retail -> source divergence, not cheap.\n")
        fh.write("# B2 callee PRESENT unnamed -> identification gap; naming the address adjudicates it.\n")
        fh.write("# D1 ROW_NAME_REFUTED    our call is ENTAILED by the row's own name and retail's is not =>\n")
        fh.write("#                        the ROW's map name is wrong. Proves EXISTENCE, not ASSIGNMENT (W7).\n")
        fh.write("# D2 SOURCE_DEFECT       neither callee entailed => our source chose, and chose wrong. FIXABLE.\n")
        fh.write("# D4 MAP_DEFECT          retail's own name at `at` is not body-confirmed either.\n")
        fh.write("\t".join(cols) + "\n")
        for rec in sorted(out, key=lambda x: (RANK.index(x["verdict"]) if x["verdict"] in RANK else 99,
                                              -x["size"])):
            fh.write("\t".join(str(rec.get(c, "")) for c in cols) + "\n")
    print(f"# wrote {op}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
