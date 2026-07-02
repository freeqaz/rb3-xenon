#!/usr/bin/env python3
"""span_confirm.py — oracle-plurality span-identity confirmer (ws7 R3).

Cheap identity check for "this .text span is TU X" using the committed
cross-binary function-pairing oracle `dc3_oracle.json`. Because MSVC /O1
preserves TU spatial grouping in .text, the DC3-side TU (`dc3_tu`) of the
oracle rows whose `rb3_va` falls inside a candidate span form a plurality
vote for the span's identity. A margin gate (>=5 in-span rows, top vote >=3,
top >= 2x second) turns the raw plurality (71% correct on DC3-shared pinned
TUs) into an 84%-precision / 63%-coverage confirmer.

  >>> STANDING WARNING <<<
  NOT VALID for oracle-located candidates (ws3 dc3-cluster spans:
  pin_candidates.json / game_splits.json) — the oracle would vote for itself.
  Use ONLY on spans located by an INDEPENDENT signal
  (ghidriff / BSim / crossval / contiguity).

Stdlib-only, read-only. Never writes to tracked build inputs.

Modes:
  --span 0xLO:0xHI --claim <TU>   single-span CONFIRM/CONTRA/ABSTAIN
  --candidates FILE.json          batch verdicts over [{lo,hi,claim,source?}]
  --calibrate                     pinned-TU ground-truth evaluation (accept gate)
  --triage                        build unpinned-candidate set from independent
                                  sources and emit verdict tables (md or json)
"""
import argparse
import bisect
import json
import os
from collections import Counter, defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
DEFAULT_ORACLE = os.path.join(REPO, "dc3_oracle.json")
DEFAULT_SPLITS = os.path.join(REPO, "config", "45410914", "splits.txt")

GATE_MIN_ROWS = 5      # >=5 in-span oracle rows
GATE_MIN_TOP = 3       # top vote count >=3
GATE_MARGIN = 2        # top >= 2x second
CLUSTER_GAP = 0x10000  # 64 KiB gap splits a stem's fns into separate clusters
SPAN_PAD = 0x400       # candidate span = [min_va, max_va + 0x400)

WARNING = (
    "NOT VALID for oracle-located candidates (ws3 dc3-cluster spans) — the "
    "oracle would vote for itself. Use only on spans located by independent "
    "signal (ghidriff/BSim/crossval/contiguity)."
)


# ---------------------------------------------------------------------------
# name normalization
# ---------------------------------------------------------------------------
def norm(name):
    """Strip path/':' prefixes and extension, lowercase. TU basename -> stem."""
    if name is None:
        return ""
    b = str(name).replace("\\", "/").split("/")[-1]
    b = b.split(":")[-1]
    if "." in b:
        b = b.rsplit(".", 1)[0]
    return b.lower()


def _variants(stem):
    """ham<->band leading-substitution twin set (hamcamshot == bandcamshot)."""
    out = {stem}
    if stem.startswith("ham"):
        out.add("band" + stem[3:])
    if stem.startswith("band"):
        out.add("ham" + stem[4:])
    return out


def stems_equal(a, b):
    na, nb = norm(a), norm(b)
    if na == nb:
        return True
    return bool(_variants(na) & _variants(nb))


# ---------------------------------------------------------------------------
# input loading
# ---------------------------------------------------------------------------
def _to_int(v):
    if isinstance(v, int):
        return v
    return int(str(v), 16)


def load_oracle(path):
    """Return (sorted_vas, tus) parallel arrays + set of all TU stems."""
    with open(path) as f:
        rows = json.load(f)
    pairs = []
    for r in rows:
        va = r.get("rb3_va")
        tu = r.get("dc3_tu")
        if va is None or tu is None:
            continue
        try:
            pairs.append((_to_int(va), norm(tu)))
        except (ValueError, TypeError):
            continue
    pairs.sort(key=lambda p: p[0])
    vas = [p[0] for p in pairs]
    tus = [p[1] for p in pairs]
    return vas, tus, set(tus)


def parse_splits(path):
    """Return dict {TU_stem: [(lo, hi), ...]} of pinned .text spans."""
    tus = defaultdict(list)
    cur = None
    with open(path) as f:
        for line in f:
            s = line.rstrip("\n")
            if not s:
                continue
            if not s[0].isspace() and s.rstrip().endswith(":"):
                cur = norm(s.rstrip()[:-1])
                continue
            t = s.strip()
            if cur and t.startswith(".text"):
                parts = t.split()
                lo = hi = None
                for p in parts:
                    if p.startswith("start:"):
                        lo = _to_int(p[6:])
                    elif p.startswith("end:"):
                        hi = _to_int(p[4:])
                if lo is not None and hi is not None:
                    tus[cur].append((lo, hi))
    return dict(tus)


def flat_pinned_spans(splits):
    spans = sorted((lo, hi) for lst in splits.values() for (lo, hi) in lst)
    return spans


def _is_pinned(va, sorted_spans):
    """True if va falls inside any pinned .text span. sorted by lo."""
    i = bisect.bisect_right(sorted_spans, (va, float("inf"))) - 1
    if i >= 0 and sorted_spans[i][0] <= va < sorted_spans[i][1]:
        return True
    return False


# ---------------------------------------------------------------------------
# core vote
# ---------------------------------------------------------------------------
def vote_span(lo, hi, vas, tus):
    """Return dict: n_rows, counter, top_tu, top, second."""
    i = bisect.bisect_left(vas, lo)
    j = bisect.bisect_left(vas, hi)
    ctr = Counter(tus[i:j])
    n = j - i
    ranked = ctr.most_common()
    top_tu, top = (ranked[0] if ranked else (None, 0))
    second = ranked[1][1] if len(ranked) > 1 else 0
    return {"n_rows": n, "counter": ctr, "ranked": ranked,
            "top_tu": top_tu, "top": top, "second": second}


def verdict(lo, hi, claim, vas, tus):
    v = vote_span(lo, hi, vas, tus)
    n, top, second = v["n_rows"], v["top"], v["second"]
    if n < GATE_MIN_ROWS:
        v["verdict"], v["reason"] = "ABSTAIN", "sparse (n<%d)" % GATE_MIN_ROWS
    elif top < GATE_MIN_TOP:
        v["verdict"], v["reason"] = "ABSTAIN", "sparse-top (top<%d)" % GATE_MIN_TOP
    elif top < GATE_MARGIN * second:
        v["verdict"], v["reason"] = "ABSTAIN", "no-margin (top<%dx second)" % GATE_MARGIN
    elif stems_equal(v["top_tu"], claim):
        v["verdict"], v["reason"] = "CONFIRM", ""
    else:
        v["verdict"], v["reason"] = "CONTRA", "top=%s != claim=%s" % (v["top_tu"], norm(claim))
    return v


def _vote_table(v, k=4):
    return ", ".join("%s:%d" % (t, c) for t, c in v["ranked"][:k]) or "(none)"


# ---------------------------------------------------------------------------
# mode: single span
# ---------------------------------------------------------------------------
def cmd_span(args, vas, tus):
    lo_s, hi_s = args.span.split(":")
    lo, hi = _to_int(lo_s), _to_int(hi_s)
    v = verdict(lo, hi, args.claim, vas, tus)
    print("# span_confirm — WARNING: %s" % WARNING)
    print("span   : 0x%08x-0x%08x  claim=%s" % (lo, hi, norm(args.claim)))
    print("gate   : n_rows=%d  top=%d  second=%d  (need n>=%d, top>=%d, top>=%dx second)"
          % (v["n_rows"], v["top"], v["second"], GATE_MIN_ROWS, GATE_MIN_TOP, GATE_MARGIN))
    print("votes  : %s" % _vote_table(v))
    tail = ("  (%s)" % v["reason"]) if v["reason"] else ""
    print("VERDICT: %s%s" % (v["verdict"], tail))


# ---------------------------------------------------------------------------
# mode: batch candidates
# ---------------------------------------------------------------------------
def _run_candidates(cands, vas, tus):
    rows = []
    for c in cands:
        lo, hi = _to_int(c["lo"]), _to_int(c["hi"])
        claim = c.get("claim", "")
        v = verdict(lo, hi, claim, vas, tus)
        rows.append({
            "source": c.get("source", ""),
            "provenance": c.get("provenance", c.get("source", "")),
            "claim": norm(claim), "lo": lo, "hi": hi,
            "n_rows": v["n_rows"], "top_tu": v["top_tu"], "top": v["top"],
            "second": v["second"], "verdict": v["verdict"],
            "votes": _vote_table(v),
        })
    return rows


def _md_table(rows):
    out = ["| source | claim | span | n | top vote/count | 2nd | verdict |",
           "|---|---|---|---:|---|---:|---|"]
    for r in rows:
        out.append("| %s | %s | 0x%08x-0x%08x | %d | %s / %s | %d | %s |" % (
            r["source"], r["claim"], r["lo"], r["hi"], r["n_rows"],
            r["top_tu"] or "-", r["top"], r["second"], r["verdict"]))
    return "\n".join(out)


def cmd_candidates(args, vas, tus):
    with open(args.candidates) as f:
        cands = json.load(f)
    if args.provenance:
        for c in cands:
            c.setdefault("provenance", args.provenance)
    rows = _run_candidates(cands, vas, tus)
    print("# span_confirm batch — WARNING: %s" % WARNING)
    if args.format == "json":
        print(json.dumps(rows, indent=1))
    else:
        print(_md_table(rows))
    counts = Counter(r["verdict"] for r in rows)
    print("\nsummary: CONFIRM=%d CONTRA=%d ABSTAIN=%d" %
          (counts["CONFIRM"], counts["CONTRA"], counts["ABSTAIN"]))


# ---------------------------------------------------------------------------
# mode: calibrate (ACCEPTANCE GATE)
# ---------------------------------------------------------------------------
def calibrate(vas, tus, oracle_stems, splits):
    """Ground-truth eval on DC3-shared pinned TUs. Returns metrics dict."""
    records = []  # (truth, n_rows, top_tu, top, second, correct, passes_gate)
    for tu, spans in splits.items():
        if tu not in oracle_stems:  # DC3-shared only
            continue
        n = top = second = 0
        ctr = Counter()
        for (lo, hi) in spans:
            v = vote_span(lo, hi, vas, tus)
            n += v["n_rows"]
            ctr += v["counter"]
        ranked = ctr.most_common()
        top_tu, top = (ranked[0] if ranked else (None, 0))
        second = ranked[1][1] if len(ranked) > 1 else 0
        if n < 1:
            continue
        correct = stems_equal(top_tu, tu) if top_tu else False
        passes = (n >= GATE_MIN_ROWS and top >= GATE_MIN_TOP
                  and top >= GATE_MARGIN * second)
        records.append((tu, n, top_tu, top, second, correct, passes))

    def raw_at(thresh):
        sub = [r for r in records if r[1] >= thresh]
        corr = sum(1 for r in sub if r[5])
        return corr, len(sub)

    base = [r for r in records if r[1] >= GATE_MIN_ROWS]  # population for coverage
    passers = [r for r in base if r[6]]
    prec_corr = sum(1 for r in passers if r[5])
    return {
        "n_dc3_shared": len(records),
        "raw5": raw_at(5), "raw10": raw_at(10), "raw20": raw_at(20),
        "base": len(base), "passers": len(passers), "prec_corr": prec_corr,
    }


def cmd_calibrate(vas, tus, oracle_stems, splits):
    m = calibrate(vas, tus, oracle_stems, splits)
    r5c, r5n = m["raw5"]
    r10c, r10n = m["raw10"]
    r20c, r20n = m["raw20"]
    base, pas, pc = m["base"], m["passers"], m["prec_corr"]
    prec = (pc / pas) if pas else 0.0
    cov = (pas / base) if base else 0.0
    print("# span_confirm --calibrate  (DC3-shared pinned TUs, gate n>=%d/top>=%d/%dx)"
          % (GATE_MIN_ROWS, GATE_MIN_TOP, GATE_MARGIN))
    print("DC3-shared pinned TUs with >=1 in-span oracle row: %d" % m["n_dc3_shared"])
    print("raw plurality accuracy:")
    print("  n>=5 : %d/%d = %.1f%%" % (r5c, r5n, 100.0 * r5c / r5n if r5n else 0))
    print("  n>=10: %d/%d = %.1f%%" % (r10c, r10n, 100.0 * r10c / r10n if r10n else 0))
    print("  n>=20: %d/%d = %.1f%%" % (r20c, r20n, 100.0 * r20c / r20n if r20n else 0))
    print("margin-gated (population = n>=5 TUs = %d):" % base)
    print("  precision: %d/%d = %.1f%%" % (pc, pas, 100.0 * prec))
    print("  coverage : %d/%d = %.1f%%" % (pas, base, 100.0 * cov))
    ok = prec >= 0.80 and cov >= 0.50
    print("ACCEPTANCE (>=80%% precision, >=50%% coverage): %s" % ("PASS" if ok else "FAIL"))
    return m, prec, cov, ok


# ---------------------------------------------------------------------------
# mode: triage — build unpinned candidate set from INDEPENDENT sources
# ---------------------------------------------------------------------------
def _cluster(vas_list):
    """Split sorted VA list at gaps > CLUSTER_GAP; yield (lo, hi) spans."""
    vas_list = sorted(vas_list)
    if not vas_list:
        return
    start = prev = vas_list[0]
    for v in vas_list[1:]:
        if v - prev > CLUSTER_GAP:
            yield (start, prev + SPAN_PAD)
            start = v
        prev = v
    yield (start, prev + SPAN_PAD)


def _stem_of(tu):
    return norm(tu)


def build_triage_candidates(repo, splits):
    """Return list of candidate dicts from ghidriff / crossval / worklists.

    Every source is located by Wii<->Xenon signal (ghidriff/BinDiff/BSim),
    NEVER by dc3_oracle.json — so voting with the DC3 oracle is non-circular.
    Explicitly EXCLUDES pin_candidates.json / game_splits.json (oracle-located).
    """
    pinned = flat_pinned_spans(splits)
    cands = []

    def emit(source, prov, tag, stem_to_vas):
        for stem, vlist in sorted(stem_to_vas.items()):
            unp = [v for v in vlist if not _is_pinned(v, pinned)]
            if len(unp) < 2:
                continue
            for (lo, hi) in _cluster(unp):
                cands.append({"lo": lo, "hi": hi, "claim": stem,
                              "source": source, "provenance": prov, "tag": tag,
                              "n_fns": sum(1 for v in unp if lo <= v < hi)})

    # (1) ghidriff_identities.json
    p = os.path.join(repo, "ghidriff_identities.json")
    if os.path.exists(p):
        by = defaultdict(list)
        for r in json.load(open(p)):
            addr = r.get("rb3_addr")
            tu = r.get("tu")
            if addr and tu:
                by[_stem_of(tu)].append(_to_int(addr))
        emit("ghidriff", "ghidriff (Wii<->Xenon ExactInstr/BSim)",
             "ghidriff", by)

    # (2) crossval_agree.json — recheck pinned against CURRENT splits
    p = os.path.join(repo, "docs", "decomp", "gameid", "crossval_agree.json")
    if os.path.exists(p):
        d = json.load(open(p))
        af = d.get("agree_fns", d if isinstance(d, list) else [])
        by = defaultdict(list)
        for r in af:
            addr = r.get("addr")
            stem = r.get("stem")
            if addr and stem:
                by[_stem_of(stem)].append(_to_int(addr))
        emit("crossval", "crossval BinDiff&BSim agree (stem-only, ~80-86% "
             "empirical precision, network stems uncalibrated)", "crossval", by)

    # (3) sysnet + band3 worklists (same ghidriff/BSim provenance)
    for fn, src in (("sysnet_port_worklist.json", "sysnet-worklist"),
                    ("band3_port_worklist.json", "band3-worklist")):
        p = os.path.join(repo, fn)
        if not os.path.exists(p):
            continue
        d = json.load(open(p))
        wl = d.get("worklist", d if isinstance(d, list) else [])
        by = defaultdict(list)
        for r in wl:
            addr = r.get("rb3_addr")
            tu = r.get("tu")
            if addr and tu:
                by[_stem_of(tu)].append(_to_int(addr))
        emit(src, "%s (Wii<->Xenon BSim/ghidriff)" % src, src, by)

    return cands


def cmd_triage(args, vas, tus, splits):
    cands = build_triage_candidates(args.repo or REPO, splits)
    rows = _run_candidates(cands, vas, tus)
    for r, c in zip(rows, cands):
        r["tag"] = c.get("tag", "")
        r["n_fns"] = c.get("n_fns", 0)
    if args.format == "json":
        print(json.dumps(rows, indent=1))
        return
    # markdown grouped by source
    print("# span_confirm --triage")
    print("\n> WARNING: %s\n" % WARNING)
    bysrc = defaultdict(list)
    for r in rows:
        bysrc[r["source"]].append(r)
    for src in sorted(bysrc):
        srows = bysrc[src]
        print("\n## source: %s (%d candidate spans)\n" % (src, len(srows)))
        print(_md_table(srows))
        cnt = Counter(r["verdict"] for r in srows)
        print("\n**%s summary:** CONFIRM=%d CONTRA=%d ABSTAIN=%d\n" %
              (src, cnt["CONFIRM"], cnt["CONTRA"], cnt["ABSTAIN"]))
    tot = Counter(r["verdict"] for r in rows)
    print("\n## TOTAL: CONFIRM=%d CONTRA=%d ABSTAIN=%d (n=%d spans)" %
          (tot["CONFIRM"], tot["CONTRA"], tot["ABSTAIN"], len(rows)))


# ---------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--oracle", default=DEFAULT_ORACLE)
    ap.add_argument("--splits", default=DEFAULT_SPLITS)
    ap.add_argument("--repo", default=REPO, help="repo root for triage sources")
    ap.add_argument("--span", help="0xLO:0xHI single-span mode")
    ap.add_argument("--claim", help="TU basename claimed for --span")
    ap.add_argument("--candidates", help="batch: JSON list of {lo,hi,claim,source?}")
    ap.add_argument("--calibrate", action="store_true")
    ap.add_argument("--triage", action="store_true")
    ap.add_argument("--format", choices=["md", "json"], default="md")
    ap.add_argument("--provenance", help="record provenance string into batch output")
    args = ap.parse_args()

    vas, tus, oracle_stems = load_oracle(args.oracle)
    splits = parse_splits(args.splits)

    if args.calibrate:
        cmd_calibrate(vas, tus, oracle_stems, splits)
    elif args.triage:
        cmd_triage(args, vas, tus, splits)
    elif args.candidates:
        cmd_candidates(args, vas, tus)
    elif args.span:
        if not args.claim:
            ap.error("--span requires --claim")
        cmd_span(args, vas, tus)
    else:
        ap.error("pick a mode: --span/--candidates/--calibrate/--triage")


if __name__ == "__main__":
    main()
