#!/usr/bin/env python3
"""Name the retail addresses W16-UH's census proved our callee to be, where the
name lets a retail row pair with a body we compile.  (Lane W16-UM, 2026-10-07.)

INPUT
-----
W16-UH's census (`tools/placeholder_callee_census.py --out`), default
~/tmp/w16uh/census_after.json: 236 distinct (A, F) pairs whose verdict is
PROVEN -- `icf_pair_adjudicate.chase(fn_<A>, F)` holds, i.e. retail's body at
A equals our compiled F modulo relocated fields and every relocation target
agrees, recursively.  They cover 86 distinct addresses; folded templates give
one address up to 31 spellings.

WHAT PAIRS
----------
objdiff pairs a retail row with ours BY NAME, inside one unit.  Naming A as S
pays only when A's retail row sits in a unit whose compiled obj DEFINES S.  So
per address (`plan`):

  NAMEABLE          a census spelling of A is compiled in A's unit obj and is
                    not the applied map name of any other address.  Survivor S
                    = that spelling (most call sites first).
  TWIN              no census spelling is compiled there, but our unit obj
                    defines an unmapped, unpaired function T of the same size
                    that ALSO chases PROVEN at A (a fold twin: retail kept one
                    body for both).  Survivor S = T.
  NO-BASE-OBJ       A's unit has no compiled obj (an `auto_*` span, or a
                    vendor unit with no source): nothing to pair with.
  NOT-IN-UNIT-OBJ   A's unit obj compiles neither a census spelling nor a twin.
                    The spelling's body lives in another unit's obj, so the
                    address is in another unit's range for pairing purposes.
  MAPPED-ELSEWHERE  every candidate spelling is already the map's name at a
                    different address.

Every other PROVEN spelling of A is a call-site spelling of the same retail
body.  Once A is named S, a site spelled F != S is CHARGED under name_check
unless an alias group at A folds F.  `alias` admits each such F into the group
at A (opening one if needed) only when chase(S, F) is PROVEN with no cycle
assumed and no slot left undischarged, F is not the map name anywhere, and F is
not already folded at another placed address.  Anything refused is recorded;
its sites become charged, which the wave's A/B prices.

MODES
-----
    python3 tools/name_proven_placeholders.py plan  [--census P] [--out LEDGER]
    python3 tools/name_proven_placeholders.py map   --ledger L --addrs 0x..,0x.. [--dry-run]
    python3 tools/name_proven_placeholders.py alias --ledger L --addrs 0x..,0x.. [--write]

`map` inserts absent keys through tools/gated_map_write.py (all its gates) and
replaces a deliberate `null` row by a one-line splice, then audits the whole
map (P7 object-side injectivity) and name injectivity.  `alias` needs a tree
BUILT after the map edit (the target objs must carry the new names).

Read-only except for the two map/alias writes.
"""
import argparse
import collections
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "scripts"))
MAP = ROOT / "scripts/target_symbol_map.json"
LEDGER = ROOT / "scripts/symbol_aliases.json"
LANE = "W16-UM 2026-10-07"   # --lane overrides (W16-UN reused this tool)
WORK = Path.home() / "tmp/w16um"   # --workdir overrides
DEFAULT_CENSUS = Path.home() / "tmp/w16uh/census_after.json"


def applied_map():
    m = json.loads(MAP.read_text())
    by_addr, keys = {}, set()
    for k, v in m.items():
        if not k.lower().startswith("0x"):
            continue
        keys.add(int(k, 16))
        if isinstance(v, str) and v:
            by_addr[int(k, 16)] = v
    by_name = collections.defaultdict(list)
    for a, n in by_addr.items():
        by_name[n].append(a)
    return by_addr, by_name, keys


def obj_names(path, cache={}):
    from coff_bodies_ext import function_bodies_ext
    if path not in cache:
        cache[path] = {n: len(raw) for n, raw, _r, _e in function_bodies_ext(str(path))}
    return cache[path]


def plan(census, out):
    import placeholder_callee_census as C
    J = C.Judge()
    cen = json.loads(Path(census).read_text())["pairs"]
    proven = [v for v in cen.values() if v["verdict"] == "PROVEN"]
    od = json.loads((ROOT / "objdiff.json").read_text())["units"]
    _by_addr, by_name, keys = applied_map()
    tgt_unit = collections.defaultdict(list)
    for u in od:
        tp = u.get("target_path")
        if tp and (ROOT / tp).exists():
            for n in obj_names(ROOT / tp):
                tgt_unit[n].append(u)
    byA = collections.defaultdict(list)
    for v in proven:
        byA[int(v["A"], 16)].append(v)
    rows = []
    for A, vs in sorted(byA.items()):
        s = "fn_%08X" % A
        sites = {v["F"]: len(v["sites"]) for v in vs}
        spell = sorted(sites, key=lambda f: (-sites[f], f))
        r = dict(A="0x%08x" % A, retail_size=vs[0]["retail_size"], spellings=spell,
                 sites=sites, key_is_null_row=A in keys)
        units = tgt_unit.get(s, [])
        if len(units) != 1:
            r.update(cls="NO-RETAIL-ROW", reason="fn_%08X is in %d target objs" % (A, len(units)))
            rows.append(r)
            continue
        u = units[0]
        r["unit"] = u["name"]
        bp = u.get("base_path")
        if not bp or not (ROOT / bp).exists():
            r.update(cls="NO-BASE-OBJ",
                     reason="unit %s has no compiled obj (no source is built for this span), so "
                            "no name at A can pair its row" % u["name"])
            rows.append(r)
            continue
        base = obj_names(ROOT / bp)
        tnames = set(obj_names(ROOT / u["target_path"]))
        here = [f for f in spell if f in base]
        free = [f for f in here if not by_name.get(f)]
        if free:
            r.update(cls="NAMEABLE", survivor=free[0],
                     reason="%s is compiled in %s and named nowhere else" % (free[0][:80], bp))
        elif here:
            r.update(cls="MAPPED-ELSEWHERE",
                     reason="every spelling compiled in %s is already the map name at %s" % (
                         bp, {f[:60]: ["0x%08x" % x for x in by_name[f]] for f in here}))
        else:
            twins = []
            rt = J.tgt.get(s)
            for n, sz in base.items():
                if n in tnames or by_name.get(n) or n in sites or not rt:
                    continue
                ob = J.ours.get(n)
                if ob and ob[2] == rt[2] and J.chase(s, n)[0]:
                    twins.append(n)
            if twins:
                twins.sort()
                r.update(cls="TWIN", survivor=twins[0], twins=twins,
                         reason="no census spelling is compiled in %s; %s is, and chases PROVEN "
                                "at A (retail folded them)" % (bp, twins[0][:80]))
            else:
                elsewhere = sorted({x["name"] for f in spell for x in od
                                    if x.get("base_path") and (ROOT / x["base_path"]).exists()
                                    and f in obj_names(ROOT / x["base_path"])})
                r.update(cls="NOT-IN-UNIT-OBJ",
                         reason="A's row is in %s, whose obj compiles no spelling of it and no "
                                "PROVEN twin; the spellings are compiled in %s" % (
                                    u["name"], elsewhere[:6]))
        rows.append(r)
    Path(out).write_text(json.dumps(rows, indent=1) + "\n")
    c = collections.Counter(r["cls"] for r in rows)
    print("addresses %d, pairs %d: %s" % (len(rows), len(proven), dict(c)))
    print("pairs by class:", dict(collections.Counter(
        r["cls"] for r in rows for _ in r["spellings"])))
    print("wrote", out)


def pick(ledger, addrs):
    rows = {r["A"]: r for r in json.loads(Path(ledger).read_text())}
    out = []
    for a in addrs:
        r = rows["0x%08x" % int(a, 16)]
        assert r["cls"] in ("NAMEABLE", "TWIN"), (a, r["cls"])
        out.append(r)
    return out


def do_map(ledger, addrs, dry):
    rows = pick(ledger, addrs)
    insert = {r["A"]: r["survivor"] for r in rows if not r["key_is_null_row"]}
    nulls = {r["A"]: r["survivor"] for r in rows if r["key_is_null_row"]}
    if insert:
        tmp = WORK / "rows.json"
        tmp.write_text(json.dumps(insert, indent=1))
        cmd = [sys.executable, str(ROOT / "tools/gated_map_write.py"), "--target", str(MAP),
               "--rows-json", str(tmp)] + (["--dry-run"] if dry else [])
        rc = subprocess.call(cmd)
        if rc:
            sys.exit("gated_map_write refused (rc %d)" % rc)
    if nulls:
        _by_addr, by_name, _k = applied_map()
        text = MAP.read_text()
        for a, n in nulls.items():
            assert not by_name.get(n), (a, n, by_name.get(n))
            pat = '\n "%s": null,' % a
            assert text.count(pat) == 1, (a, text.count(pat))
            text = text.replace(pat, '\n "%s": %s,' % (a, json.dumps(n)))
        if not dry:
            MAP.write_text(text)
        print("%s %d null row(s): %s" % ("would replace" if dry else "replaced", len(nulls),
                                         sorted(nulls)))
    if not dry:
        _by_addr, _bn, _k = applied_map()
        for a, n in {**insert, **nulls}.items():
            assert _by_addr.get(int(a, 16)) == n, (a, n)
        for cmd in ([sys.executable, str(ROOT / "tools/gated_map_write.py"), "--target",
                     str(MAP), "--audit-objects"],
                    [sys.executable, str(ROOT / "tools/map_name_injectivity.py")]):
            rc = subprocess.call(cmd, stdout=subprocess.DEVNULL)
            if rc:
                sys.exit("post-write audit failed: %s (rc %d)" % (cmd[1], rc))
        print("map audits PASS (object-side and name injectivity)")


def do_alias(ledger, addrs, write, extra=()):
    from alias_survivor_relabel import Judge, operation
    J = Judge()
    doc = json.loads(LEDGER.read_text())
    G = doc["groups"]
    by_addr = {g["address"].lower(): g for g in G if g.get("address")}
    folded_at = collections.defaultdict(set)
    for g in G:
        if g.get("address"):
            for f in g.get("folded", []):
                folded_at[f].add(g["address"].lower())
    log = []
    work = list(pick(ledger, addrs))
    for X, f in extra:
        # a fold spelling a paired row of this wave calls, at an address the map
        # already names: same admission rule, survivor = the map name there.
        X = "0x%08x" % int(X, 16)
        work.append(dict(A=X, survivor=J.retail_name(int(X, 16)), spellings=[f],
                         sites={f: 0}))
    for r in work:
        X, S = r["A"], r["survivor"]
        va = int(X, 16)
        assert J.retail_name(va) == S, ("map/target objs not at S yet -- build first", X,
                                        J.retail_name(va))
        ok, tr = J.raw(S, S)
        assert ok, ("survivor does not chase PROVEN on its own retail row", X, [t[1] for t in tr][:6])
        g = by_addr.get(X)
        if g is not None and g["survivor"] != S:
            sys.exit("group at %s has survivor %s: run tools/alias_survivor_relabel.py --write "
                     "first" % (X, g["survivor"][:60]))
        for f in r["spellings"]:
            if f == S or (g is not None and f in g.get("folded", [])):
                continue
            rec = {"A": X, "spelling": f, "sites": r["sites"][f]}
            others = folded_at.get(f, set()) - {X}
            if J.addr_of.get(f):
                rec.update(admitted=False, why="map name at %s" % [hex(x) for x in J.addr_of[f]])
            elif others:
                rec.update(admitted=False, why="already folded at %s" % sorted(others))
            else:
                okr, tr = J.raw(S, f)
                # A cycle is accepted only where it is SELF-RECURSION: the assumed
                # (retail, ours) callee pair is one the chase entered and checked
                # at a shallower depth (a recursive tree _M_erase).  Any other
                # assumption counts against admission, as in alias_locate_home.
                # (the trace truncates names on SLOT-OK rows, so match by prefix)
                entered = [(t[2], t[3]) for t in tr
                           if t[1] in ("SLOT-OK:CALLEE-CHASED", "SLOT-FOLD-OK")]
                ncyc = sum(1 for t in tr if t[1] == "CYCLE-ASSUMED"
                           and not any(str(t[2]).startswith(rn) and str(t[3]).startswith(on)
                                       for rn, on in entered))
                nrec = sum(1 for t in tr if t[1] == "CYCLE-ASSUMED") - ncyc
                und = any(t[1].startswith("SLOT-UNDISCHARGED") for t in tr)
                if not (okr and ncyc == 0 and not und):
                    rec.update(admitted=False, why="chase(%s, f) not clean: ok=%s cycles=%d "
                               "undischarged=%s" % (S[:50], okr, ncyc, und))
                else:
                    if g is None:
                        g = {"name": operation(S) or S[:20], "address": X, "survivor": S,
                             "folded": [], "withdrawn": [],
                             "evidence": "Opened by %s: W16-UH's census proved each folded "
                                         "spelling is the callee retail reaches at this address "
                                         "(see 'admitted')." % LANE}
                        G.append(g)
                        by_addr[X] = g
                    g["folded"].append(f)
                    g.setdefault("admitted", []).append({
                        "spelling": f, "lane": LANE, "how": "CENSUS-SITE",
                        "evidence": ("tools/placeholder_callee_census.py (W16-UH): %d aligned call "
                                     "site(s) where retail calls fn_%08X and ours calls this "
                                     "spelling; chase(fn_%08X, spelling) PROVEN" % (
                                         r["sites"][f], va, va)) if r["sites"][f] else (
                                     "a row %s paired calls this spelling where retail's "
                                     "relocation names the survivor (objdiff diff_arg)" % LANE.split()[0]),
                        "chase": "tools/icf_pair_adjudicate.chase(%s @ %s, ours): PROVEN, 0 "
                                 "undischarged, %d cycle(s) assumed%s" % (
                                     S[:70], X, nrec,
                                     " (each a self-recursive callee already entered and "
                                     "checked)" if nrec else "")})
                    folded_at[f].add(X)
                    rec.update(admitted=True)
            log.append(rec)
    # invariants (as tools/alias_locate_home.py asserts them)
    left = J.D.find_drift(G, J.applied)
    assert not left, ("drift", [(g["address"], why) for g, _w, why in left[:5]])
    survs = [g["survivor"] for g in G]
    assert len(set(survs)) == len(survs), "survivor not unique"
    addrs_ = [g["address"].lower() for g in G if g.get("address")]
    assert len(set(addrs_)) == len(addrs_), "address not unique"
    for rec in log:
        print("  %s %s %-70s %s" % (rec["A"], "ADMIT " if rec["admitted"] else "refuse",
                                    rec["spelling"][:70], rec.get("why", "")))
    print("admitted %d, refused %d" % (sum(r["admitted"] for r in log),
                                       sum(not r["admitted"] for r in log)))
    out = WORK / ("alias_log_%s.json" % "_".join(a[2:] for a in addrs[:1]))
    out.write_text(json.dumps(log, indent=1))
    if write:
        LEDGER.write_text(json.dumps(doc, indent=1, ensure_ascii=False) + "\n")
        print("wrote", LEDGER)


def main():
    global LANE, WORK
    ap = argparse.ArgumentParser()
    ap.add_argument("mode", choices=["plan", "map", "alias"])
    ap.add_argument("--census", default=str(DEFAULT_CENSUS))
    ap.add_argument("--out", default=str(Path.home() / "tmp/w16um/ledger.json"))
    ap.add_argument("--ledger", default=str(Path.home() / "tmp/w16um/ledger.json"))
    ap.add_argument("--addrs", default="")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--write", action="store_true")
    ap.add_argument("--extra", action="append", default=[], metavar="0xADDR=Spelling",
                    help="alias: also admit Spelling at the (already named) address")
    ap.add_argument("--lane", default=LANE, help="lane label written into alias records")
    ap.add_argument("--workdir", default=str(WORK), help="scratch dir for rows.json / alias logs")
    a = ap.parse_args()
    LANE, WORK = a.lane, Path(a.workdir)
    addrs = [x for x in re.split(r"[,\s]+", a.addrs) if x]
    if a.mode == "plan":
        return plan(a.census, a.out)
    if not addrs:
        ap.error("--addrs required")
    if a.mode == "map":
        return do_map(a.ledger, addrs, a.dry_run)
    return do_alias(a.ledger, addrs, a.write, [tuple(e.split("=", 1)) for e in a.extra])


if __name__ == "__main__":
    sys.exit(main())
