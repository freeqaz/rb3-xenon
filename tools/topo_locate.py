#!/usr/bin/env python3
"""topo_locate.py — Callee-Set Topological Locator (hard-frontier identifier).

Locates scattered-TU game methods in the RB3-360 retail binary purely by
CONFIRMED-ANCHOR CALL-GRAPH TOPOLOGY: a Wii method M calls a set of named
callees; the subset of those callees that are *confirmed anchors* (matched@100
with a known retail VA) pins M into the retail call graph. The retail function
that calls that same set of anchored callees IS M. No similarity diffusion, no
Ghidra/BSim/objdiff in the loop — pure call-bag intersection over committed
on-disk assets.

Design + verdict + kill criteria:
  docs/decomp/research/2026-06-30-topo-locator-design.md

All inputs are COMMITTED, read-only. Runs in seconds.

Subcommands:
  locate   <TU.cpp...>   locate methods of one or more Wii TUs
  --validate             self-relocation held-out precision@1 + calibration
  --classb-floor         SongSortNode + BandProfile N>=2 floor test
  --harvest              Stage-3 scan over band3+network unconfirmed methods
  --emit-gate FILE       write {VA:{class,confidence}} sidecar (locator.py fmt)
  --pin-only  FILE       write [VA,...] of methods whose topo-VA == oracle VA
"""
import argparse
import json
import os
import re
import sys

# ---------------------------------------------------------------------------
# Paths (anchored to repo root, derived from this file's location).
# ---------------------------------------------------------------------------
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
# Some inputs (fingerprints.json) are gitignored/untracked, so a CoW worktree
# may lack them. Resolve each input against this tree first, falling back to
# the canonical main repo root (read-only) for any that are missing.
MAIN_ROOT = "/home/free/code/milohax/rb3-xenon"


def _resolve(rel):
    p = os.path.join(ROOT, rel)
    if os.path.exists(p):
        return p
    return os.path.join(MAIN_ROOT, rel)


REPORT = _resolve("build/45410914/report.json")
TARGET_MAP = _resolve("scripts/target_symbol_map.json")
FINGERPRINTS = _resolve("fingerprints.json")
UNIFIED = _resolve("unified_id_rb3wii.json")
CROSSVAL = _resolve("docs/decomp/gameid/crossval_agree.json")
WII_ASM_ROOT = "/home/free/code/milohax/rb3/build/SZBE69_B8/asm"

# Stub/compiler-helper callees that carry zero topological signal (they fold
# identically across every TU under ICF and never disambiguate).
STUB_PAT = re.compile(
    r"^(_savegpr|_restgpr|_savefpr|_restfpr|_save|_rest|__dt|__dl|__nw|__nwa"
    r"|__dla|_MemOrPool|stlpmtx|memcpy|memset|memmove|memcmp|__construct"
    r"|__destroy|__fill|__copy|__uninitialized|strlen|strcmp|strcpy"
    r"|__lower_bound|__upper_bound|__equal_range|__throw|__stl_throw"
    r"|__cnt|__shl|__shr|__div|__mod|__mul|__eti|__init_cpp|__sti)")

# A "stub" retail fn (too small to carry a real divergent body / pure thunk).
STUB_SIZE = 44

FN_VA_RE = re.compile(r"fn_(8[0-9A-Fa-f]{7})")


# ---------------------------------------------------------------------------
# 1. anchor VA set  (matched@100 retail functions with a recoverable VA)
# ---------------------------------------------------------------------------
def build_anchor_va_set(report, name2va):
    """Set of retail VAs that are matched@100 (the confirmed anchors)."""
    anchors = set()
    for un in report["units"]:
        for f in un.get("functions") or []:
            if float(f.get("match_percent_normalized") or 0.0) < 100.0:
                continue
            nm = f["name"]
            m = FN_VA_RE.search(nm)
            if m:
                anchors.add(int(m.group(1), 16))
            elif nm in name2va:
                anchors.add(name2va[nm])
    return anchors


def load_name2va(target_map):
    """Invert target_symbol_map: mangled-name -> VA (keys are 0X<HEX>)."""
    out = {}
    for k, v in target_map.items():
        if k.startswith("_"):
            continue
        out[v] = int(k[2:], 16)
    return out


# ---------------------------------------------------------------------------
# 2. game anchor map  (class::method -> {VA, ...}, KEEPING ICF aliases)
# ---------------------------------------------------------------------------
def build_game_anchor_map(unified, anchor_vas):
    """class::method -> set(VA) for confirmed game anchors.

    Keeps the ICF-alias class: one class::method may fold to several retail VAs.
    """
    amap = {}
    src = {}  # class::method -> wii TU stem (for src filtering in harvest)
    for r in unified:
        addr = r.get("rb3_addr")
        if not addr:
            continue
        va = int(addr, 16)
        if va not in anchor_vas:
            continue
        wn = r.get("wii_name", "")
        if "::" not in wn:
            continue
        cm = wn.split("(")[0]
        amap.setdefault(cm, set()).add(va)
        bs = r.get("bindiff_src") or ""
        if bs:
            src.setdefault(cm, os.path.basename(bs))
    return amap, src


# ---------------------------------------------------------------------------
# 3. retail call graph  (reverse edges + VA->class::method)
# ---------------------------------------------------------------------------
def build_retail_graph(fingerprints, game_anchor_map):
    """callers_of[calleeVA] = set(callerVA); va2cm[VA] = class::method."""
    callers_of = {}
    for hexva, fp in fingerprints.items():
        try:
            caller = int(hexva, 16)
        except ValueError:
            continue
        for cal in fp.get("callees") or []:
            try:
                cva = int(cal, 16)
            except ValueError:
                continue
            callers_of.setdefault(cva, set()).add(caller)
    va2cm = {}
    for cm, vas in game_anchor_map.items():
        for va in vas:
            va2cm[va] = cm
    return callers_of, va2cm


# ---------------------------------------------------------------------------
# 4. Wii method callee parse  (reuse the fingerprint_match .fn/.bl machinery)
# ---------------------------------------------------------------------------
CG_FN_RE = re.compile(r"^\.fn\s+(.+?),\s+(?:global|local)\s*$")
CG_ENDFN_RE = re.compile(r"^\.endfn\b")
CG_BL_RE = re.compile(r"bl\s+(\"?[A-Za-z_][\w$<>,:.*&\"() -]*\"?)\s*$")
CG_STR_RE = re.compile(r'@stringbase|\.string\s+"([^"]+)"|"([^"]{4,})"')


def _tu_to_asm_path(tu_basename):
    """Find the Wii .s for a TU basename (e.g. SongSortNode.cpp)."""
    stem = tu_basename
    if stem.endswith(".cpp"):
        stem = stem[:-4]
    target = stem + ".s"
    for root, _dirs, files in os.walk(WII_ASM_ROOT):
        if target in files:
            return os.path.join(root, target)
    return None


def parse_wii_method_callees(tu_basename):
    """{wii_fn_mangled: {'callees':[names], 'insns':int, 'strings':[str]}}.

    Drops STL/compiler stub callees (zero topological signal).
    """
    path = _tu_to_asm_path(tu_basename)
    if not path:
        return {}, None
    out = {}
    cur = None
    callees = []
    insns = 0
    strings = []
    with open(path, errors="replace") as f:
        for line in f:
            fm = CG_FN_RE.match(line)
            if fm:
                if cur is not None:
                    out[cur] = {"callees": callees, "insns": insns,
                                "strings": strings}
                cur = fm.group(1).strip().strip('"')
                callees, insns, strings = [], 0, []
                continue
            if CG_ENDFN_RE.match(line):
                if cur is not None:
                    out[cur] = {"callees": callees, "insns": insns,
                                "strings": strings}
                cur = None
                continue
            if cur is None:
                continue
            if "/*" in line and "*/" in line:
                insns += 1
            bm = CG_BL_RE.search(line.rstrip())
            if bm:
                tgt = bm.group(1).strip().strip('"')
                if not STUB_PAT.match(tgt):
                    callees.append(tgt)
            sm = CG_STR_RE.search(line)
            if sm:
                s = sm.group(1) or sm.group(2)
                if s:
                    strings.append(s)
        if cur is not None:
            out[cur] = {"callees": callees, "insns": insns, "strings": strings}
    return out, path


# ---------------------------------------------------------------------------
# 5. demangle gcc2 -> class::method
# ---------------------------------------------------------------------------
_DEM_RE = re.compile(r"^(.+?)__(Q\d+_?\d+|\d+)(.+)$")


def demangle_gcc2_key(mangled):
    """gcc2 mangled fn name -> 'Class::Method' (or None if not a member).

    Handles flat (Method__<len><Class>...) and nested Q<n>... names.
    """
    mangled = mangled.strip().strip('"')
    m = _DEM_RE.match(mangled)
    if not m:
        return None
    meth, lentok, rest = m.group(1), m.group(2), m.group(3)
    if meth.startswith("__"):
        # ctor/dtor/operator helpers: gcc2 uses __ct/__dt/__as etc. Keep as-is
        # so 'Class::__ct' still keys (rarely an anchor, but harmless).
        pass
    if lentok.startswith("Q"):
        # nested: QN_L1<name1>L2<name2>...  parse the L<name> segments.
        body = lentok[1:] + rest  # e.g. '2' + '3Hmx6Object...'
        mnest = re.match(r"(\d+)_?(.*)$", body)
        if not mnest:
            return None
        count = int(mnest.group(1))
        s = mnest.group(2)
        parts = []
        for _ in range(count):
            lm = re.match(r"(\d+)(.*)$", s)
            if not lm:
                break
            n = int(lm.group(1))
            parts.append(lm.group(2)[:n])
            s = lm.group(2)[n:]
        if not parts:
            return None
        cls = "::".join(parts)
        return cls + "::" + meth
    n = int(lentok)
    cls = rest[:n]
    if not cls:
        return None
    return cls + "::" + meth


# ---------------------------------------------------------------------------
# 6. locate
# ---------------------------------------------------------------------------
def _string_overlap(a, b):
    sa, sb = set(a), set(b)
    if not sa or not sb:
        return 0
    return len(sa & sb)


def _size_band(insns):
    # coarse band for the tiebreak; n_insns and size both available.
    return insns


class Locator:
    def __init__(self, anchor_vas, game_anchor_map, callers_of, va2cm,
                 fingerprints, src_map):
        self.anchor_vas = anchor_vas
        self.gam = game_anchor_map
        self.callers_of = callers_of
        self.va2cm = va2cm
        self.fp = fingerprints
        self.src_map = src_map

    def locate_method(self, wii_fn, info, hide_va=None,
                      pinned_other=None):
        """Locate ONE wii method. Returns a result dict or None.

        hide_va: for self-relocation HELD-OUT measurement. The method's own
                 true VA is removed from the CONFIRMED-ANCHOR knowledge (so the
                 self-consistency guard won't reject the tool's own answer as a
                 "collision" with the very method we hid), but it REMAINS a
                 legitimate candidate in the retail call graph — the whole point
                 is to test whether pure topology re-ranks it #1 blind.
        pinned_other: VA -> class::method already pinned; reject collisions.
        """
        # Resolve callees to anchored class::method groups.
        groups = {}  # class::method -> set(anchor VAs for that key)
        for cal in info["callees"]:
            key = demangle_gcc2_key(cal)
            if key is None:
                continue
            if key in self.gam:
                groups[key] = set(self.gam[key])
        n = len(groups)
        if n < 2:
            return None

        # Vote: each anchored callee group contributes its union-of-ICF callers.
        votes = {}  # candidate retail VA -> vote count (distinct groups)
        for key, vas in groups.items():
            callerset = set()
            for cva in vas:
                callerset |= self.callers_of.get(cva, set())
            for caller in callerset:
                votes[caller] = votes.get(caller, 0) + 1

        if not votes:
            return None
        thresh = max(2, n - 1)
        cands = {va: v for va, v in votes.items() if v >= thresh}
        if not cands:
            return None

        # Rank: (votes desc, string_overlap desc, |insn delta| asc, VA asc).
        m_strings = info["strings"]
        m_insns = info["insns"]

        def keyf(va):
            fp = self.fp.get("%08X" % va) or {}
            so = _string_overlap(m_strings, fp.get("strings") or [])
            di = abs((fp.get("n_insns") or 0) - m_insns)
            return (-cands[va], -so, di, va)

        ranked = sorted(cands, key=keyf)
        top = ranked[0]
        topfp = self.fp.get("%08X" % top) or {}

        # vote margin: top votes minus runner-up votes.
        sorted_votes = sorted((cands[v] for v in cands), reverse=True)
        margin = sorted_votes[0] - (sorted_votes[1] if len(sorted_votes) > 1
                                    else 0)
        has_string = _string_overlap(m_strings, topfp.get("strings") or []) > 0

        # SELF-CONSISTENCY GUARD.
        reject = None
        sz = int(topfp.get("size") or 0)
        if sz and sz <= STUB_SIZE:
            reject = "stub"
        elif top in self.va2cm and top != hide_va:
            # top is a confirmed anchor for some class::method already. Reject
            # only if it's a DIFFERENT confirmed method (a collision). The
            # held-out self (top == hide_va) is exempt — that's the answer.
            cm_at = self.va2cm[top]
            if cm_at != wii_fn and cm_at not in groups:
                reject = "collision:%s" % cm_at
        if pinned_other and top in pinned_other and \
                pinned_other[top] != wii_fn:
            reject = "already-pinned:%s" % pinned_other[top]

        return {
            "wii_fn": wii_fn,
            "n_groups": n,
            "located_va": "%08X" % top,
            "votes": cands[top],
            "vote_margin": margin,
            "has_string": has_string,
            "n_cands": len(cands),
            "size_band": _size_band(topfp.get("n_insns") or 0),
            "reject": reject,
            "groups": sorted(groups.keys()),
            "candset": ["%08X" % v for v in ranked],
        }


# ---------------------------------------------------------------------------
# data loading
# ---------------------------------------------------------------------------
def load_all():
    report = json.load(open(REPORT))
    target_map = json.load(open(TARGET_MAP))
    fingerprints = json.load(open(FINGERPRINTS))
    unified = json.load(open(UNIFIED))
    name2va = load_name2va(target_map)
    anchors = build_anchor_va_set(report, name2va)
    gam, src_map = build_game_anchor_map(unified, anchors)
    callers_of, va2cm = build_retail_graph(fingerprints, gam)
    return {
        "report": report, "fingerprints": fingerprints, "unified": unified,
        "name2va": name2va, "anchors": anchors, "gam": gam,
        "src_map": src_map, "callers_of": callers_of, "va2cm": va2cm,
    }


def oracle_va_for(unified, cm_key):
    """Return the set of oracle rb3 VAs for a class::method key."""
    out = set()
    for r in unified:
        wn = r.get("wii_name", "")
        if "::" in wn and wn.split("(")[0] == cm_key and r.get("rb3_addr"):
            out.add(int(r["rb3_addr"], 16))
    return out


# ---------------------------------------------------------------------------
# --validate : self-relocation held-out precision@1
# ---------------------------------------------------------------------------
def run_validate(D):
    """For each game anchor with N>=2 anchored callees: hide its VA, locate,
    measure top1==trueVA. Bucket by (N, has_string, vote_margin)."""
    loc = Locator(D["anchors"], D["gam"], D["callers_of"], D["va2cm"],
                  D["fingerprints"], D["src_map"])
    # Enumerate game anchor methods that have a Wii body we can parse: walk
    # the band3+network TUs, for each parsed wii fn whose demangled key is a
    # confirmed game anchor, run held-out locate.
    tus = collect_game_tus(D)
    n_total = 0          # pool size = game-anchor methods with N>=2 groups
    n_top1 = 0           # held-out top1 == a true VA of this method
    n_incand = 0         # a true VA appears anywhere in the candidate set
    n_nocand = 0         # method had N>=2 but no candidate cleared the vote floor
    buckets = {}
    examples = []
    seen = set()
    for tu in tus:
        methods, _ = parse_wii_method_callees(tu)
        for wfn, info in methods.items():
            key = demangle_gcc2_key(wfn)
            if key is None or key not in D["gam"]:
                continue
            if (tu, wfn) in seen:
                continue
            seen.add((tu, wfn))
            # Pool gate = N>=2 distinct anchored callee GROUPS (independent of
            # whether any retail caller votes clear the floor). This is the doc's
            # pool definition; un-locatable (NOVOTE) methods score as misses so
            # precision@1 is NOT inflated by silently dropping hard cases.
            groups = set()
            for cal in info["callees"]:
                k = demangle_gcc2_key(cal)
                if k and k in D["gam"]:
                    groups.add(k)
            n_g = len(groups)
            if n_g < 2:
                continue
            true_vas = D["gam"][key]
            n_total += 1
            # Held-out: hide the (first) true VA so the guard won't reject our
            # own answer as a self-collision; the VA stays a valid candidate.
            tva = sorted(true_vas)[0]
            res = loc.locate_method(wfn, info, hide_va=tva)
            if res is None:
                n_nocand += 1
                buckets.setdefault((n_g, False, 0), [0, 0])[1] += 1
                if len(examples) < 60:
                    examples.append({"method": key, "true": "%08X" % tva,
                                     "located": None, "correct": False,
                                     "n": n_g, "reason": "no-candidate"})
                continue
            top = int(res["located_va"], 16)
            correct = (top in true_vas)
            incand = any(int(c, 16) in true_vas for c in res["candset"])
            n_top1 += int(correct)
            n_incand += int(incand)
            bk = (res["n_groups"], res["has_string"],
                  min(res["vote_margin"], 3))
            b = buckets.setdefault(bk, [0, 0])
            b[0] += int(correct)
            b[1] += 1
            if len(examples) < 60:
                examples.append({
                    "method": key, "true": "%08X" % tva,
                    "located": res["located_va"], "correct": correct,
                    "n": res["n_groups"], "margin": res["vote_margin"],
                    "has_string": res["has_string"]})
    prec1 = (n_top1 / n_total) if n_total else 0.0
    incand_rate = (n_incand / n_total) if n_total else 0.0
    table = []
    for (n, hs, mg), (c, t) in sorted(buckets.items()):
        table.append({"n_groups": n, "has_string": hs, "vote_margin": mg,
                      "correct": c, "total": t,
                      "precision": round((c / t) if t else 0.0, 3)})
    return {
        "held_out_n": n_total,
        "precision_at_1": round(prec1, 4),
        "trueVA_in_candset_rate": round(incand_rate, 4),
        "n_no_candidate": n_nocand,
        "calibration_table": table,
        "examples": examples,
    }


# ---------------------------------------------------------------------------
# TU collection
# ---------------------------------------------------------------------------
def collect_game_tus(D):
    """All band3+network Wii TU basenames referenced by unified rows."""
    tus = set()
    for r in D["unified"]:
        bs = r.get("bindiff_src") or ""
        if bs.startswith("band3/") or bs.startswith("network/"):
            tus.add(os.path.basename(bs))
    return sorted(tus)


# ---------------------------------------------------------------------------
# --classb-floor
# ---------------------------------------------------------------------------
def run_classb_floor(D):
    loc = Locator(D["anchors"], D["gam"], D["callers_of"], D["va2cm"],
                  D["fingerprints"], D["src_map"])
    out = {}
    for tu in ("SongSortNode.cpp", "BandProfile.cpp"):
        methods, path = parse_wii_method_callees(tu)
        n_fns = len(methods)
        n_ge2 = 0
        n_emitted = 0
        for wfn, info in methods.items():
            res = loc.locate_method(wfn, info)
            if res is not None and res["n_groups"] >= 2:
                n_ge2 += 1
                if res["reject"] is None:
                    n_emitted += 1
        out[tu] = {"path": path, "n_fns": n_fns, "n_ge2_anchored": n_ge2,
                   "n_emitted_candidates": n_emitted}
    floor_ok = all(v["n_ge2_anchored"] < 3 for v in out.values())
    return {"floor_ok": floor_ok, "per_tu": out}


# ---------------------------------------------------------------------------
# --harvest  (Stage 3)
# ---------------------------------------------------------------------------
def run_harvest(D):
    loc = Locator(D["anchors"], D["gam"], D["callers_of"], D["va2cm"],
                  D["fingerprints"], D["src_map"])
    # crossval oracle for the agree pass.
    crossval_vas = set()
    try:
        cv = json.load(open(CROSSVAL))
        for e in cv.get("agree_fns", []):
            crossval_vas.add(int(e["addr"], 16))
    except Exception:
        pass

    tus = collect_game_tus(D)
    candidates = []
    pinned = {}  # VA -> class::method (running self-consistency)
    for tu in tus:
        methods, _ = parse_wii_method_callees(tu)
        for wfn, info in methods.items():
            key = demangle_gcc2_key(wfn)
            if key is None:
                continue
            # UNCONFIRMED: skip methods already matched@100 (their VA is an
            # anchor for this exact key) — those are the trivial self.
            already = key in D["gam"]
            res = loc.locate_method(wfn, info, pinned_other=pinned)
            if res is None or res["reject"] is not None:
                continue
            top = int(res["located_va"], 16)
            if already and top in D["gam"][key]:
                continue  # trivially-confirmed self
            # oracle VA from unified for agree check
            oracle_vas = oracle_va_for(D["unified"], key)
            agrees = top in oracle_vas
            pinned[top] = key
            candidates.append({
                "wii_tu": tu,
                "method": key,
                "located_va": res["located_va"],
                "agrees_with_oracle": agrees,
                "vote_margin": res["vote_margin"],
                "has_string": res["has_string"],
                "n_groups": res["n_groups"],
                "votes": res["votes"],
                "crossval_agree": top in crossval_vas,
                "oracle_vas": ["%08X" % v for v in sorted(oracle_vas)],
            })
    return {"n_candidates": len(candidates), "candidates": candidates}


# ---------------------------------------------------------------------------
# gate / pin-only emission
# ---------------------------------------------------------------------------
def emit_gate(candidates, path):
    sidecar = {c["located_va"]: {"class": "TOPO_LOCATED",
                                 "confidence": 0.6}
               for c in candidates}
    json.dump(sidecar, open(path, "w"), indent=1)
    return len(sidecar)


def emit_pin_only(candidates, path):
    # only methods where topo-VA == oracle VA (safe to carve via existing path)
    vas = sorted({c["located_va"] for c in candidates
                  if c.get("agrees_with_oracle")})
    json.dump(vas, open(path, "w"), indent=1)
    return len(vas)


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("tus", nargs="*", help="TU basenames to locate")
    ap.add_argument("--validate", action="store_true",
                    help="self-relocation held-out precision@1 + calibration")
    ap.add_argument("--classb-floor", action="store_true",
                    help="SongSortNode + BandProfile floor test")
    ap.add_argument("--harvest", action="store_true",
                    help="Stage-3 unconfirmed harvest scan")
    ap.add_argument("--emit-gate", default=None,
                    help="write {VA:{class,confidence}} sidecar")
    ap.add_argument("--pin-only", default=None,
                    help="write [VA,...] of oracle-agreeing located methods")
    ap.add_argument("--json", default=None, help="write full result JSON")
    args = ap.parse_args()

    print("[topo] loading committed assets ...", file=sys.stderr)
    D = load_all()
    print(f"[topo] anchors={len(D['anchors'])} game_anchor_keys={len(D['gam'])} "
          f"game_anchor_vas={sum(len(v) for v in D['gam'].values())} "
          f"callee_nodes={len(D['callers_of'])}", file=sys.stderr)

    result = {"anchors": len(D["anchors"]),
              "game_anchor_keys": len(D["gam"]),
              "game_anchor_vas": sum(len(v) for v in D["gam"].values())}

    if args.validate:
        v = run_validate(D)
        result["validate"] = v
        print(f"[topo] VALIDATE precision@1={v['precision_at_1']} "
              f"held_out_n={v['held_out_n']} "
              f"incand_rate={v['trueVA_in_candset_rate']}", file=sys.stderr)

    if args.classb_floor:
        cf = run_classb_floor(D)
        result["classb_floor"] = cf
        for tu, s in cf["per_tu"].items():
            print(f"[topo] FLOOR {tu}: fns={s['n_fns']} "
                  f"N>=2={s['n_ge2_anchored']} emitted={s['n_emitted_candidates']}",
                  file=sys.stderr)
        print(f"[topo] classb_floor_ok={cf['floor_ok']}", file=sys.stderr)

    if args.harvest:
        h = run_harvest(D)
        result["harvest"] = h
        nagree = sum(1 for c in h["candidates"] if c["agrees_with_oracle"])
        ncv = sum(1 for c in h["candidates"] if c["crossval_agree"])
        print(f"[topo] HARVEST candidates={h['n_candidates']} "
              f"agree_oracle={nagree} crossval_agree={ncv}", file=sys.stderr)
        if args.emit_gate:
            n = emit_gate(h["candidates"], args.emit_gate)
            print(f"[topo] wrote gate {args.emit_gate} ({n})", file=sys.stderr)
        if args.pin_only:
            n = emit_pin_only(h["candidates"], args.pin_only)
            print(f"[topo] wrote pin-only {args.pin_only} ({n})",
                  file=sys.stderr)

    if args.tus:
        loc = Locator(D["anchors"], D["gam"], D["callers_of"], D["va2cm"],
                      D["fingerprints"], D["src_map"])
        per = []
        for tu in args.tus:
            methods, path = parse_wii_method_callees(tu)
            for wfn, info in methods.items():
                res = loc.locate_method(wfn, info)
                if res is not None and res["n_groups"] >= 2:
                    res["key"] = demangle_gcc2_key(wfn)
                    per.append(res)
        result["locate"] = per
        for r in per:
            print(f"  {r['key']:50s} -> {r['located_va']} "
                  f"N={r['n_groups']} votes={r['votes']} "
                  f"margin={r['vote_margin']} str={r['has_string']} "
                  f"rej={r['reject']}", file=sys.stderr)

    if args.json:
        json.dump(result, open(args.json, "w"), indent=1)
        print(f"[topo] wrote {args.json}", file=sys.stderr)


if __name__ == "__main__":
    main()
