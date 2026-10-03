#!/usr/bin/env python3
"""Relabel drifted alias-group survivors to the map's name, and RE-CHASE every
membership against the corrected survivor.  (Lane W16-OS, 2026-10-03.)

`tools/alias_survivor_drift.py` defines drift and says why it matters.  This is
its fix, and it is not a rename: relabelling a group's survivor to the name
retail uses at its address SWITCHES ON forgiveness of every folded member
against that name (the rendered bucket is `[survivor, *folded]`).  A member
that was proven -- if at all -- against a stale label was proven against the
wrong body.  So every membership is re-adjudicated here, on retail bytes, with
`tools/icf_pair_adjudicate.chase`, against the body AT THE GROUP'S ADDRESS.

VERDICTS (each recorded per membership in the group's `rechase_w16os` list)
---------------------------------------------------------------------------
PROVEN       chase() returns True against the retail body at the address.
REFUTED      positive evidence on retail bytes that the member is a different
             function from the one at the address:
               * a SLOT-CONTRADICTED discharge (retail RTTI / type descriptor /
                 literal / anchored callee names something else) -- the
                 adjudicator's own positive class; or
               * a byte or relocation-shape difference (BYTES-DIFFER,
                 RELOC-COUNT, RELOC-SHAPE, MAPPED-VS-PLACEHOLDER) AND an ANCHOR:
                 some other spelling of OURS is PROVEN on the same retail body.
                 The anchor is what separates "this is a different function"
                 from "this is a defective port of the right one" -- without it
                 a failed chase is not evidence (icf_pair_adjudicate's own
                 rule: "not proven" is not "refuted").  When the difference is
                 at depth >= 1 (a callee), it also needs one of:
                   - the two callee names are different OPERATIONS
                     (`_M_copy_from` vs `_M_initialize_buckets`), or
                   - our callee is map-resident and PROVEN at its own address,
                     so retail holds BOTH callee bodies and they differ.
UNDECIDABLE  anything else: our build compiles no body (MISSING(ours)), the
             body is vacuous, a slot is undischarged, or a byte difference has
             no anchor.  The kind is recorded.

DISPOSITION
-----------
* survivor := the applied map name at the address (or its `fn_` placeholder
  when the map names nothing there).
* folded members: PROVEN and UNDECIDABLE stay; REFUTED are WITHDRAWN with a
  record -- unless the member is PROVEN at the stale label's own map address,
  in which case it is RE-HOMED to that address's group (created if absent).
  That is the W16-NK case at 0x827d5bb0: its proof was made against the stale
  label's body at 0x824f18c8, and that is where the member lives.
* the old label is kept as a recorded respelling (`relabelled`) and, when it
  is not the map's name somewhere else, as a membership by the same rule --
  PROVEN -> folded; REFUTED -> withdrawn; UNDECIDABLE -> folded only if the map
  name was already in the bucket (render-neutral), otherwise HELD as a record
  (folding it would activate forgiveness nobody proved).  An old label that
  the map places at another address is never folded here (it would put one
  name at two addresses) -- it is that address's survivor.
* an address-less partition class whose survivor IS the map name at the
  address is the same fold class: its members are chased and absorbed (W16-OA's
  pattern).
* nothing is pruned: every change leaves a record in the group.

    python3 tools/alias_survivor_relabel.py              # dry run: verdicts + accounting
    python3 tools/alias_survivor_relabel.py --write [--json OUT]

Requires a BUILT tree (the target objs must carry their mangled names).
"""
import argparse
import collections
import copy
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
LEDGER = ROOT / "scripts/symbol_aliases.json"

POS = ("BYTES-DIFFER", "RELOC-COUNT", "RELOC-SHAPE", "MAPPED-VS-PLACEHOLDER")
NONLEAF = ("SLOT-OK", "SLOT-FOLD-OK", "CYCLE-ASSUMED", "VACUOUS-BUT-IDENTICAL",
           "VACUOUS-DESTINATION-FOLD-PROVEN", "RETAIL-", "VACUOUS-SLOT-OK",
           "SLOT-REFUTED")


def spelling(w):
    return w if isinstance(w, str) else w.get("spelling")


def deny_keys(groups):
    ks = set()
    for g in groups:
        for w in g.get("withdrawn", []) or []:
            sp = spelling(w)
            if sp:
                ks.add(("S", g["survivor"], sp))
                if g.get("address"):
                    ks.add(("A", g["address"], sp))
    return ks


def operation(name):
    """The unqualified operation a mangled name denotes, or None if unknown.
    `?_M_copy_from@?$hashtable@...` -> `_M_copy_from`; `??0?$ObjPtr@...` -> `??0`;
    `??$_Copy_Construct@...` -> `_Copy_Construct`."""
    if not name or not name.startswith("?"):
        return None
    if name.startswith("??$"):
        return name[3:].split("@", 1)[0]
    if name.startswith("??_"):
        return name[:4]
    if name.startswith("??"):
        return name[:3]
    return name[1:].split("@", 1)[0]


class Judge:
    def __init__(self):
        import icf_pair_adjudicate as A
        import alias_survivor_drift as D
        self.A, self.D = A, D
        self.mapped = A.load_mapped()
        self.tgt, self.ours = A.load_sides()
        self.applied = D.applied_names()
        self.addr_of = collections.defaultdict(list)
        for va, n in self.applied.items():
            self.addr_of[n].append(va)
        self._self = {}

    def retail_name(self, va):
        n = self.applied.get(va)
        if n:
            return n
        for p in ("fn_%08X", "lbl_%08X"):
            if p % va in self.tgt:
                return p % va
        return "fn_%08X" % va

    def raw(self, s, f):
        """-> (ok, trace).  ok None == a side is absent."""
        if s not in self.tgt:
            return None, [(0, "MISSING(retail)", s, f)]
        if f not in self.ours:
            return None, [(0, "MISSING(ours)", s, f)]
        tr = []
        return bool(self.A.chase(self.tgt, self.ours, s, f, self.mapped, out=tr)), tr

    def self_proven(self, n):
        """our COMDAT for map-resident `n` is PROVEN on retail's body named `n`."""
        if n not in self._self:
            ok, _ = self.raw(n, n)
            self._self[n] = bool(ok)
        return self._self[n]

    @staticmethod
    def leaf(tr):
        for t in tr:
            if not t[1].startswith(NONLEAF):
                return t
        return tr[-1] if tr else (0, "?", "", "")

    def classify(self, s, f, ok, tr, anchors):
        """-> (verdict, kind, detail).  `anchors`: our spellings PROVEN on `s`."""
        ncyc = sum(1 for t in tr if t[1] == "CYCLE-ASSUMED")
        if ok:
            sz = "retail %d B / ours %d B" % (self.tgt[s][2], self.ours[f][2])
            return "PROVEN", "CHASED", "%s, %d cycle-assumed" % (sz, ncyc)
        kinds = [t[1] for t in tr]
        con = [t for t in tr if "SLOT-CONTRADICTED" in t[1]]
        if con:
            t = con[0]
            return "REFUTED", t[1], "%s | %s" % (str(t[2])[:80], str(t[3])[:140])
        d, k, rn, on = self.leaf(tr)
        det = "depth %d: retail %s | ours %s" % (d, str(rn)[:90], str(on)[:90])
        if k not in POS:
            return "UNDECIDABLE", k, det
        anc = [a for a in anchors if a != f]
        if not anc:
            return "UNDECIDABLE", "UNANCHORED:" + k, det + " (no spelling of ours is PROVEN on this body)"
        adet = det + "; anchor %s PROVEN here" % anc[0][:90]
        if d == 0:
            return "REFUTED", "BODY:" + k, adet
        if k != "BYTES-DIFFER":
            return "REFUTED", "CALLEE:" + k, adet
        opr, opo = operation(rn), operation(on)
        if opr and opo and opr != opo:
            return "REFUTED", "CALLEE-OPERATION-DIFFERS", adet + "; %s vs %s" % (opr, opo)
        if on in self.mapped and on in self.tgt and self.self_proven(on):
            return "REFUTED", "CALLEE-TWIN-DISTINCT", adet + (
                "; our callee is PROVEN at its own map address, so retail holds "
                "both callee bodies and they differ")
        return "UNDECIDABLE", "CALLEE-UNANCHORED:" + k, det + (
            " (same operation, our callee not proven at a retail address of its own)")

    def evidence(self, X, s, f, v, k, det):
        return ("tools/icf_pair_adjudicate.chase(%s @ %s, %s): %s [%s] %s"
                % (s[:70], X, f[:70], v, k, det))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--write", action="store_true")
    ap.add_argument("--json", help="write every verdict here")
    ap.add_argument("--lane", default="W16-OS 2026-10-03")
    a = ap.parse_args()
    LANE = a.lane
    RKEY = "rechase_w16os"
    doc = json.load(open(LEDGER))
    G = doc["groups"]
    J = Judge()
    D = J.D
    drift = D.find_drift(G, J.applied)
    print("drifted placed groups: %d of %d" % (len(drift), sum(1 for g in G if g.get("address"))))
    if not drift:
        return 0
    before = deny_keys(G)
    n0 = len(G)
    by_addr = {g["address"]: g for g in G if g.get("address")}
    unplaced_by_surv = {g["survivor"]: g for g in G if not g.get("address")}
    all_folded = collections.defaultdict(set)   # spelling -> addresses folded at
    for g in G:
        if g.get("address"):
            for f in g.get("folded", []):
                all_folded[f].add(g["address"])
    drifted_addrs = {g["address"] for g, _w, _r in drift}
    drop = set()
    rows = []             # every verdict, for --json and the doc
    rehome = collections.defaultdict(list)   # addr Y -> [(f, evidence, from X)]
    stats = collections.Counter()

    # ---- pass 1: judge every membership against the body at the address ----
    plans = []
    for g, want, _why in drift:
        X = g["address"]
        va = int(X, 16)
        M = want if want is not None else J.retail_name(va)
        old = g["survivor"]
        folded = list(g.get("folded", []))
        M_in_bucket = M in folded
        absorbed = unplaced_by_surv.get(M)
        cands = [("folded", f) for f in folded if f != M]
        if absorbed is not None:
            cands += [("absorbed", f) for f in absorbed.get("folded", [])
                      if f != M and f not in folded]
        if old != M:
            cands.append(("old-label", old))
        res = {}
        for role, f in cands:
            res[(role, f)] = J.raw(M, f)
        anchors = [f for (role, f), (ok, _t) in res.items() if ok]
        if M in J.ours and J.self_proven(M):
            anchors.insert(0, M)
        verdicts = {}
        for (role, f), (ok, tr) in res.items():
            v, k, det = J.classify(M, f, ok, tr, anchors)
            verdicts[(role, f)] = (v, k, det)
        plans.append((g, X, M, old, folded, M_in_bucket, absorbed, verdicts))

    # ---- pass 2: dispositions ----
    for g, X, M, old, folded, M_in_bucket, absorbed, verdicts in plans:
        old_elsewhere = [hex(va) for va in J.addr_of.get(old, []) if va != int(X, 16)]
        new_folded, wd_new, held, log = [], [], [], []
        for f in folded:
            if f == M:
                continue
            v, k, det = verdicts[("folded", f)]
            ev = J.evidence(X, M, f, v, k, det)
            disp = "kept"
            if v == "REFUTED":
                moved = None
                for Y in old_elsewhere:
                    ok, tr = J.raw(old, f)
                    ncyc = sum(1 for t in tr if t[1] == "CYCLE-ASSUMED")
                    if ok and ncyc == 0:
                        moved = Y
                        rehome[Y].append((f, J.evidence(Y, old, f, "PROVEN", "CHASED",
                                          "retail %d B / ours %d B, 0 cycle-assumed"
                                          % (J.tgt[old][2], J.ours[f][2])), X, ev))
                        break
                disp = "rehomed to %s" % moved if moved else "withdrawn"
                wd_new.append({"spelling": f, "lane": LANE,
                               "class": "REHOMED_FROM_STALE_SURVIVOR_ADDRESS" if moved
                               else "STALE_SURVIVOR_RECHASE_REFUTED",
                               "evidence": ev + (". PROVEN instead at %s (the stale label's map "
                                                 "address), where it now lives." % moved if moved else "")})
            else:
                new_folded.append(f)
            stats[("folded", v, disp)] += 1
            log.append({"spelling": f, "role": "folded", "verdict": v, "kind": k,
                        "disposition": disp, "evidence": ev})
        if absorbed is not None:
            for f in absorbed.get("folded", []):
                if f == M or ("absorbed", f) not in verdicts:
                    continue
                v, k, det = verdicts[("absorbed", f)]
                ev = J.evidence(X, M, f, v, k, det)
                if v == "PROVEN":
                    new_folded.append(f)
                    disp = "admitted (absorbed from the address-less class)"
                else:
                    held.append({"spelling": f, "lane": LANE, "verdict": v, "evidence": ev})
                    disp = "held (not activated)"
                stats[("absorbed", v, disp.split(" ")[0])] += 1
                log.append({"spelling": f, "role": "absorbed", "verdict": v, "kind": k,
                            "disposition": disp, "evidence": ev})
        if old != M:
            v, k, det = verdicts[("old-label", old)]
            ev = J.evidence(X, M, old, v, k, det)
            if old_elsewhere:
                disp = "map name at %s (not a member here)" % ",".join(old_elsewhere)
                if v == "REFUTED":
                    wd_new.append({"spelling": old, "lane": LANE,
                                   "class": "FORMER_SURVIVOR_LABEL_NOT_A_FOLD", "evidence": ev})
            elif v == "PROVEN":
                new_folded.append(old)
                disp = "folded (PROVEN)"
            elif v == "REFUTED":
                wd_new.append({"spelling": old, "lane": LANE,
                               "class": "FORMER_SURVIVOR_LABEL_REFUTED", "evidence": ev})
                disp = "withdrawn"
            elif M_in_bucket:
                new_folded.append(old)
                disp = "folded (carried: UNDECIDABLE, the map name was already in this bucket)"
            else:
                held.append({"spelling": old, "lane": LANE, "verdict": v, "evidence": ev})
                disp = "held as a record (UNDECIDABLE; folding it would activate forgiveness)"
            stats[("old-label", v, disp.split(" ")[0] + (" elsewhere" if old_elsewhere else ""))] += 1
            log.append({"spelling": old, "role": "old-label", "verdict": v, "kind": k,
                        "disposition": disp, "evidence": ev})
        # dedupe while keeping order
        seen = set()
        new_folded = [f for f in new_folded if not (f in seen or seen.add(f))]
        g["folded"] = new_folded
        if wd_new:
            g.setdefault("withdrawn", []).extend(wd_new)
        rec = {"from": old, "to": M, "lane": LANE,
               "why": "survivor was not the map's name at this address "
                      "(tools/alias_survivor_drift.py); relabelled and every "
                      "membership re-chased against the body here"}
        if held:
            rec["held"] = held
        if "relabelled" in g:
            g.setdefault("relabelled_history", []).append(g.pop("relabelled"))
        g["relabelled"] = rec
        g["survivor"] = M
        g[RKEY] = log
        if absorbed is not None:
            ac = copy.deepcopy(absorbed)
            g.setdefault("absorbed", []).append({
                "lane": LANE, "group": ac,
                "why": "address-less partition class whose survivor is the map name "
                       "at this address: the same fold class. Members chased here "
                       "(see %s); the class's survivor is now this group's." % RKEY})
            if absorbed.get("withdrawn"):
                g.setdefault("withdrawn", []).extend(
                    dict(w, carried_from={"address": None, "survivor": absorbed["survivor"],
                                          "lane": LANE}) if isinstance(w, dict) else w
                    for w in absorbed["withdrawn"])
            drop.add(id(absorbed))
        for r in log:
            rows.append(dict(r, address=X, survivor=M))

    # ---- re-home targets ----
    for Y, items in rehome.items():
        name = J.applied[int(Y, 16)]
        tgtg = by_addr.get(Y)
        if tgtg is not None:
            assert tgtg["survivor"] == name, (Y, tgtg["survivor"])
        else:
            assert name not in unplaced_by_surv, name
            tgtg = {"name": operation(name) or name[:20], "address": Y, "survivor": name,
                    "folded": [], "withdrawn": [],
                    "evidence": "Opened by %s: membership(s) re-homed here from a group "
                                "whose stale survivor label was this address's map name; "
                                "admitted on retail bytes (see 'admitted')." % LANE}
            G.append(tgtg)
            by_addr[Y] = tgtg
        for f, ev, X, evx in items:
            others = all_folded.get(f, set()) - {X}
            assert not others, (f, others)
            tgtg["folded"].append(f)
            tgtg.setdefault("admitted", []).append(
                {"spelling": f, "lane": LANE, "evidence": ev,
                 "moved_from": {"address": X, "evidence_there": evx}})
            rows.append({"address": Y, "survivor": name, "spelling": f, "role": "rehomed",
                         "verdict": "PROVEN", "kind": "CHASED", "disposition": "admitted",
                         "evidence": ev})

    G[:] = [g for g in G if id(g) not in drop]

    # ---- invariants ----
    survs = [g["survivor"] for g in G]
    addrs = [g["address"] for g in G if g.get("address")]
    dup_s = [s for s, c in collections.Counter(survs).items() if c > 1]
    dup_a = [s for s, c in collections.Counter(addrs).items() if c > 1]
    assert not dup_s, ("survivor not unique", dup_s[:5])
    assert not dup_a, ("address not unique", dup_a[:5])
    left = D.find_drift(G, J.applied)
    assert not left, ("drift remains", [(g["address"], r) for g, _w, r in left[:5]])
    fold_addrs = collections.defaultdict(set)
    for g in G:
        if g.get("address"):
            for f in g.get("folded", []):
                fold_addrs[f].add(g["address"])
    multi = {f: s for f, s in fold_addrs.items() if len(s) > 1}
    multi_before = {f: s for f, s in all_folded.items() if len(s) > 1}
    assert set(multi) <= set(multi_before), ("new one-spelling-two-addresses",
                                            sorted(set(multi) - set(multi_before))[:5])
    survset = set(survs)
    named_elsewhere = [(g["address"], f) for g in G if g.get("address") for f in g["folded"]
                       if J.addr_of.get(f) and hex(J.addr_of[f][0]) != g["address"]
                       and int(g["address"], 16) not in J.addr_of[f]]
    after = deny_keys(G)
    lost = before - after
    # a lost key is the OLD survivor-keyed copy of a record that still sits in
    # the same group (keyed now on the new survivor and, always, the address)
    explained, bad = 0, []
    relabel_from = {g["relabelled"]["from"]: g for g in G
                    if isinstance(g.get("relabelled"), dict) and g["relabelled"].get("lane") == LANE}
    for k in lost:
        kind, who, sp = k
        if kind == "S" and who in relabel_from and ("A", relabel_from[who]["address"], sp) in after:
            explained += 1
            continue
        if kind == "S" and who in unplaced_by_surv and id(unplaced_by_surv[who]) in drop:
            if ("S", who, sp) not in after and any(
                    ("A", g["address"], sp) in after for g in G
                    if any(x.get("group", {}).get("survivor") == who for x in g.get("absorbed", []))):
                explained += 1
                continue
        bad.append(k)

    print("groups %d -> %d; denylist keys %d -> %d (lost %d: %d old-survivor keys whose "
          "record is still address-keyed in the same group, %d UNEXPLAINED; gained %d)"
          % (n0, len(G), len(before), len(after), len(lost), explained, len(bad),
             len(after - before)))
    print("folded spellings named by the map at another address (validator would read "
          "CONTRADICTED): %d %s" % (len(named_elsewhere), named_elsewhere[:3]))
    for k, v in sorted(stats.items()):
        print("  %5d  %-10s %-12s %s" % (v, k[0], k[1], k[2]))
    print("re-homed: %s" % {Y: [f[:60] for f, *_ in it] for Y, it in rehome.items()})
    if bad:
        for k in bad[:20]:
            print("  UNEXPLAINED", k)
        raise SystemExit("REFUSING: a denylist key would be lost without a reason")
    if a.json:
        Path(a.json).write_text(json.dumps(rows, indent=1) + "\n")
        print("wrote", a.json)
    if a.write:
        LEDGER.write_text(json.dumps(doc, indent=1, ensure_ascii=False) + "\n")
        print("wrote", LEDGER)
    return 0


if __name__ == "__main__":
    sys.exit(main())
