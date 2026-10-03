#!/usr/bin/env python3
"""Give W16-OS's undecided and withdrawn alias memberships a RETAIL ADDRESS,
where retail bytes settle one, and move each membership there.  (Lane W16-OU,
2026-10-03; follows docs/decomp/W16OS_ALIAS_SURVIVOR_RELABEL_2026-10-03.md §7.)

Two instruments locate a function of ours in retail.  Neither SEARCHES for a
best match; each must come back with exactly ONE address or it says nothing.

BODY      `icf_pair_adjudicate.locate_retail(ours)`: the retail bodies our
          COMDAT chases PROVEN against with no cycle assumed and no slot left
          undischarged.  Settles only when exactly one retail address answers.
          It CANNOT discriminate a family whose retail copies are identical in
          every byte and every relocation target: the 24 hashtable bucket-count
          ctors W16-OS withdrew from 0x825a07e0 chase clean at FIVE retail
          addresses, all 120 B, all calling the one `_M_initialize_buckets` at
          0x8256ad18.  Retail kept five copies of one body.
CALL-SITE retail's OWN relocations.  Walk up our call graph from F to a caller
          C that IS paired (same name in the target objs, same relocation
          shape), then walk back DOWN retail: C's relocation at the slot where
          our C names the next function down, then that retail body's
          relocation at the slot where ours names the next, and so on to F.
          Every intermediate retail body must equal ours (masked bytes and
          relocation shape), so the slots correspond; the address is read off
          retail, never chosen.  Every path must agree on ONE address.
          A paired caller whose body differs from ours is admitted only when
          its relocation list has the same length and the slot has the same
          relocation type, and it is labelled OFFSET-ALIGNED.

DECISIONS
---------
A  (hashtable bucket-count ctors, withdrawn at 0x825a07e0 by W16-OS):
   CALL-SITE address Y, then `chase(retail name at Y, F)` must be PROVEN with
   no cycle assumed.  Admitted at Y with an `admitted` record.
D  (W16-OS CALLEE-UNANCHORED memberships, held or carried):
   our callee `on` vs retail callee `rn` at the group address.  If our callee
   is located (BODY, unique; or CALL-SITE, unique, AND retail's body there
   differs from retail's body at rn) at an address other than rn's, retail
   holds BOTH callee bodies and they differ, so the member's body is not the
   one at the group address: REFUTED (CALLEE-LOCATED-ELSEWHERE).  A refuted
   carried/folded member is WITHDRAWN with a record, and RE-HOMED where it is
   itself located (BODY unique, or CALL-SITE + chase PROVEN 0-cycle).  A
   refuted HELD label gets a withdrawal record and its held entry is annotated.
   Our callee, when BODY-located (a clean chase), is admitted at its address
   unless it is already a member somewhere or the map names it elsewhere.
   Rows neither instrument settles stay as they are, with the attempt recorded.

Nothing is pruned.  Every change leaves a record (`rechase_w16ou`, `admitted`,
`withdrawn`).  Invariants asserted before writing: one spelling per address,
no survivor drift, unique survivors and addresses, and every admitted
membership re-chased PROVEN with 0 cycle-assumed at the address it lands on.

    python3 tools/alias_locate_home.py                 # dry run
    python3 tools/alias_locate_home.py --write [--json OUT]

Requires a BUILT tree (the target objs must carry their mangled names).
"""
import argparse
import collections
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
LEDGER = ROOT / "scripts/symbol_aliases.json"
LANE = "W16-OU 2026-10-03"
RKEY = "rechase_w16ou"
HASHTABLE_ADDR = "0x825a07e0"
W16OS_WD = ("STALE_SURVIVOR_RECHASE_REFUTED", "FORMER_SURVIVOR_LABEL_REFUTED")


def shape(rec):
    return [(x[0], x[2]) for x in rec[1]]


class Locator:
    def __init__(self, J):
        self.J, self.A = J, J.A
        self.tgt, self.ours = J.tgt, J.ours
        self.callers = collections.defaultdict(list)
        for n, (_mb, rl, _sz) in self.ours.items():
            for (o, t, ty) in rl:
                self.callers[t].append((n, o, ty))

    def addr(self, n):
        a = self.A._name_addr(n)
        if a is None and n in self.J.addr_of and len(self.J.addr_of[n]) == 1:
            a = self.J.addr_of[n][0]
        return a

    def clean(self, s, f):
        ok, tr = self.J.raw(s, f)
        ncyc = sum(1 for t in tr if t[1] == "CYCLE-ASSUMED")
        und = any(t[1].startswith("SLOT-UNDISCHARGED") for t in tr)
        return bool(ok) and ncyc == 0 and not und, ok, tr

    # ---- BODY ----
    def body(self, on, exclude=None):
        if on not in self.ours:
            return []
        out = []
        for y in self.A.locate_retail(self.tgt, self.ours, on, self.J.mapped,
                                      exclude=exclude, cap=256):
            a = self.addr(y)
            out.append((a, y))
        return out

    # ---- CALL-SITE ----
    def _slot(self, rec, o, ty):
        s = [t for (oo, t, tt) in rec[1] if oo == o and tt == ty]
        return s[0] if s else None

    def _up(self, F, depth, maxd, seen):
        for (C, o, ty) in self.callers.get(F, []):
            if C in seen:
                continue
            if C in self.tgt:
                rc, oc = self.tgt[C], self.ours[C]
                if shape(rc) == shape(oc):
                    yield [(C, "PAIRED")], self._slot(rc, o, ty)
                elif len(rc[1]) == len(oc[1]) and self._slot(rc, o, ty) is not None:
                    yield [(C, "OFFSET-ALIGNED")], self._slot(rc, o, ty)
                continue
            if depth >= maxd:
                continue
            for chain, rC in self._up(C, depth + 1, maxd, seen | {C}):
                if rC is None or rC not in self.tgt:
                    continue
                if self.tgt[rC][0] != self.ours[C][0] or shape(self.tgt[rC]) != shape(self.ours[C]):
                    continue
                yield chain + [(C, "BODY-EQUAL@%s" % rC[:40])], self._slot(self.tgt[rC], o, ty)

    def callsite(self, F, maxd=4):
        """-> (addresses Counter, paths list[(addr, name, chain)])"""
        paths = []
        for chain, rF in self._up(F, 0, maxd, {F}):
            if rF is None:
                continue
            paths.append((self.addr(rF), rF, chain))
        return collections.Counter(a for a, _n, _c in paths), paths

    def home(self, F, exclude=None, need_chase=True):
        """Where F lives in retail, or None.  -> (va, name, how, evidence)"""
        b = [(a, y) for a, y in self.body(F, exclude) if a is not None]
        if len({a for a, _y in b}) == 1:
            a, y = b[0]
            return a, y, "BODY", "locate_retail: exactly one retail body (%s) chases PROVEN clean" % hex(a)
        if len(b) > 1:
            return None, None, "BODY-AMBIGUOUS", "%d retail bodies chase clean: %s" % (
                len(b), sorted(hex(a) for a, _ in b))
        cnt, paths = self.callsite(F)
        if len(cnt) != 1:
            return None, None, "CALLSITE-NONE" if not cnt else "CALLSITE-DISAGREE", str(
                {hex(k) if k else k: v for k, v in cnt.items()})
        a = next(iter(cnt))
        if a is None:
            return None, None, "CALLSITE-NOADDR", ""
        y = paths[0][1]
        ev = "retail call sites: %d path(s), all to %s; e.g. %s" % (
            len(paths), hex(a), " <- ".join("%s[%s]" % (c[:60], k) for c, k in paths[0][2]))
        if need_chase:
            ok, _raw, _tr = self.clean(y, F) if F in self.ours else (False, None, None)
            if not ok:
                return None, None, "CALLSITE-UNCHASED", ev
            ev += "; chase(%s, ours) PROVEN, 0 cycle-assumed" % y[:60]
        return a, y, "CALLSITE", ev


def leaf_pair(J, s, f):
    ok, tr = J.raw(s, f)
    for t in tr:
        if t[1] == "BYTES-DIFFER" and t[0] >= 1:
            return t[2], t[3]
    return None, None


operation = None


def main():
    global operation
    ap = argparse.ArgumentParser()
    ap.add_argument("--write", action="store_true")
    ap.add_argument("--json")
    ap.add_argument("--null-interior-rows", action="store_true",
                    help="C: null map rows that name a folded spelling at an interior address")
    a = ap.parse_args()
    if a.null_interior_rows:
        return null_interior_rows(a.write)
    from alias_survivor_relabel import Judge, operation
    J = Judge()
    L = Locator(J)
    doc = json.load(open(LEDGER))
    G = doc["groups"]
    by_addr = {g["address"]: g for g in G if g.get("address")}
    folded_at = collections.defaultdict(set)
    for g in G:
        if g.get("address"):
            for f in g.get("folded", []):
                folded_at[f].add(g["address"])
    rows, admit, stats = [], collections.defaultdict(list), collections.Counter()

    def plan_admit(va, name, f, how, ev, moved_from=None):
        X = hex(va)
        admit[X].append((name, f, how, ev, moved_from))

    # ---------------- A: hashtable bucket-count ctors ----------------
    hg = by_addr[HASHTABLE_ADDR]
    for w in hg.get("withdrawn", []):
        if not isinstance(w, dict) or w.get("lane", "").split(" ")[0] != "W16-OS" \
                or w.get("class") not in W16OS_WD:
            continue
        f = w["spelling"]
        va, y, how, ev = L.home(f, exclude=hg["survivor"])
        if how == "BODY-AMBIGUOUS":
            # the expected case for this family -- fall through to call sites
            cnt, paths = L.callsite(f)
            if len(cnt) == 1 and next(iter(cnt)) is not None:
                va = next(iter(cnt))
                y = paths[0][1]
                okc, _r, _t = L.clean(y, f)
                kinds = sorted({k.split("@")[0] for _a, _n, c in paths for _c, k in c})
                ev2 = ("BODY is ambiguous (%s); retail call sites: %d path(s), all to %s "
                       "[%s]; e.g. %s" % (ev, len(paths), hex(va), ",".join(kinds),
                                          " <- ".join(c[:60] for c, _k in paths[0][2])))
                if okc:
                    how, ev = "CALLSITE", ev2 + "; chase(%s, ours) PROVEN, 0 cycle-assumed" % y[:60]
                else:
                    how, ev, va = "CALLSITE-UNCHASED", ev2, None
            else:
                how, ev, va = "CALLSITE-NONE", ev + "; call sites %s" % dict(cnt), None
        r = {"part": "A", "address": HASHTABLE_ADDR, "spelling": f, "how": how,
             "home": hex(va) if va else None, "evidence": ev}
        rows.append(r)
        stats[("A", how)] += 1
        if va is not None and how in ("BODY", "CALLSITE"):
            assert not folded_at.get(f), (f, folded_at.get(f))
            plan_admit(va, J.retail_name(va), f, how, ev,
                       {"address": HASHTABLE_ADDR, "withdrawn_by": w.get("lane"),
                        "class": w.get("class")})

    # ---------------- D: callee-unanchored ----------------
    for g in G:
        rel = g.get("relabelled")
        held = {h["spelling"]: h for h in (rel.get("held") or [])} if isinstance(rel, dict) else {}
        log = []
        for rr in g.get("rechase_w16os", []) or []:
            if not rr["kind"].startswith("CALLEE-UNANCHORED"):
                continue
            X, S, f = g["address"], g["survivor"], rr["spelling"]
            rn, on = leaf_pair(J, S, f)
            rec = {"spelling": f, "role": rr["role"], "w16os": rr["disposition"],
                   "lane": LANE}
            if rn is None:
                rec.update(verdict="UNDECIDABLE", kind="NO-CALLEE-LEAF-NOW", disposition="unchanged")
                log.append(rec)
                stats[("D", "no-leaf")] += 1
                continue
            ra = L.addr(rn)
            cb = [(va, y) for va, y in L.body(on, exclude=rn) if va is not None]
            cva = cy = None
            if len({va for va, _ in cb}) == 1:
                cva, cy = cb[0]
                chow = "BODY"
                cev = "our callee %s PROVEN (clean chase) at %s = %s only" % (on[:70], hex(cva), cy[:70])
            else:
                cnt, paths = L.callsite(on)
                chow, cev = "NONE", "our callee located by neither instrument (body: %d; call sites: %s)" % (
                    len(cb), {hex(k) if k else k: v for k, v in cnt.items()})
                if len(cnt) == 1 and next(iter(cnt)) is not None:
                    va = next(iter(cnt))
                    yn = paths[0][1]
                    if yn in L.tgt and rn in L.tgt and (
                            L.tgt[yn][0] != L.tgt[rn][0] or shape(L.tgt[yn]) != shape(L.tgt[rn])):
                        cva, cy, chow = va, yn, "CALLSITE"
                        cev = ("retail calls our callee %s at %s (%d path(s), e.g. %s); retail's "
                               "body there (%d B) differs from retail's %s at %s (%d B)" % (
                                   on[:60], hex(va), len(paths),
                                   " <- ".join(c[:50] for c, _k in paths[0][2]),
                                   L.tgt[yn][2], rn[:50], hex(ra) if ra else "?", L.tgt[rn][2]))
            if cva is None or cva == ra:
                rec.update(verdict="UNDECIDABLE", kind="CALLEE-UNLOCATED", disposition="unchanged",
                           evidence=cev)
                log.append(rec)
                stats[("D", "unlocated")] += 1
                continue
            ev = ("chase(%s @ %s, %s) fails at callee: retail %s @ %s vs ours %s. %s. Retail holds "
                  "both callee bodies and they differ, so the body at %s is not this member's."
                  % (S[:60], X, f[:60], rn[:60], hex(ra) if ra else "?", on[:60], cev, X))
            rec.update(verdict="REFUTED", kind="CALLEE-LOCATED-ELSEWHERE:" + chow,
                       callee_home=hex(cva), evidence=ev)
            # member's own home
            mva, my, mhow, mev = L.home(f, exclude=S)
            rec["member_home"] = hex(mva) if mva else None
            rec["member_how"], rec["member_evidence"] = mhow, mev
            role = rr["role"]
            in_folded = f in g.get("folded", [])
            elsewhere = [hex(v) for v in J.addr_of.get(f, []) if hex(v) != X]
            if elsewhere:
                rec["disposition"] = "record only (map name at %s)" % ",".join(elsewhere)
                stats[("D", "record-mapname-elsewhere")] += 1
            elif in_folded:
                g["folded"] = [x for x in g["folded"] if x != f]
                g.setdefault("withdrawn", []).append({
                    "spelling": f, "lane": LANE, "class": "CALLEE_LOCATED_ELSEWHERE_W16OU",
                    "evidence": ev + (" Re-homed to %s (%s)." % (hex(mva), mhow) if mva else "")})
                rec["disposition"] = "withdrawn" + (", re-homed to %s" % hex(mva) if mva else "")
                stats[("D", "withdrawn" + ("+rehomed" if mva else ""))] += 1
                if mva is not None:
                    plan_admit(mva, J.retail_name(mva), f, mhow, mev,
                               {"address": X, "evidence_there": ev})
            elif f in held:
                held[f]["resolved_w16ou"] = {"verdict": "REFUTED", "kind": rec["kind"],
                                             "member_home": rec["member_home"], "evidence": ev}
                g.setdefault("withdrawn", []).append({
                    "spelling": f, "lane": LANE, "class": "FORMER_SURVIVOR_LABEL_REFUTED",
                    "evidence": ev})
                rec["disposition"] = "held label refuted (withdrawal record)" + (
                    ", re-homed to %s" % hex(mva) if mva else "")
                stats[("D", "held-refuted" + ("+rehomed" if mva else ""))] += 1
                if mva is not None:
                    plan_admit(mva, J.retail_name(mva), f, mhow, mev,
                               {"address": X, "evidence_there": ev})
            else:
                rec["disposition"] = "record only"
                stats[("D", "record")] += 1
            # our callee, when BODY-located, gets that address too
            if chow == "BODY" and not folded_at.get(on) and not J.addr_of.get(on) \
                    and on != J.retail_name(cva):
                plan_admit(cva, J.retail_name(cva), on, "BODY", cev,
                           {"address": X, "as": "callee of " + f[:80]})
            log.append(rec)
        if log:
            g.setdefault(RKEY, []).extend(log)
            for r in log:
                rows.append(dict(r, part="D", address=g["address"]))

    # ---------------- admissions ----------------
    planned = collections.defaultdict(set)
    for X, items in admit.items():
        for name, f, *_ in items:
            planned[f].add(X)
    dup = {f: s for f, s in planned.items() if len(s) > 1}
    assert not dup, ("one spelling planned at two addresses", list(dup.items())[:3])
    for X, items in sorted(admit.items()):
        va = int(X, 16)
        name = J.retail_name(va)
        g = by_addr.get(X)
        if g is None:
            g = {"name": operation(name) or name[:20],
                 "address": X, "survivor": name, "folded": [], "withdrawn": [],
                 "evidence": "Opened by %s: membership(s) located here on retail bytes "
                             "(see 'admitted')." % LANE}
            G.append(g)
            by_addr[X] = g
        assert g["survivor"] == name, (X, g["survivor"], name)
        for _n, f, how, ev, moved in items:
            if f in g["folded"] or f == g["survivor"]:
                continue
            others = folded_at.get(f, set()) - {X}
            others = {o for o in others if f in by_addr[o].get("folded", [])}
            assert not others, (f, others)
            okc, _ok, tr = L.clean(name, f)
            assert okc, ("admission does not chase PROVEN clean", X, f[:80], [t[1] for t in tr][:6])
            g["folded"].append(f)
            g.setdefault("admitted", []).append({
                "spelling": f, "lane": LANE, "how": how, "evidence": ev,
                "chase": "tools/icf_pair_adjudicate.chase(%s @ %s, ours): PROVEN, 0 cycle-assumed"
                         % (name[:70], X),
                "moved_from": moved})
            rows.append({"part": "admit", "address": X, "survivor": name, "spelling": f,
                         "how": how})
            stats[("admit", how)] += 1

    # ---------------- invariants ----------------
    D = J.D
    left = D.find_drift(G, J.applied)
    assert not left, ("drift", [(g["address"], r) for g, _w, r in left[:5]])
    survs = [g["survivor"] for g in G]
    addrs = [g["address"] for g in G if g.get("address")]
    assert len(set(survs)) == len(survs), "survivor not unique"
    assert len(set(addrs)) == len(addrs), "address not unique"
    fa = collections.defaultdict(set)
    for g in G:
        if g.get("address"):
            for f in g.get("folded", []):
                fa[f].add(g["address"])
    before_multi = {f for f, s in folded_at.items() if len(s) > 1}
    new_multi = {f for f, s in fa.items() if len(s) > 1} - before_multi
    assert not new_multi, ("new one-spelling-two-addresses", sorted(new_multi)[:5])

    for k, v in sorted(stats.items()):
        print("  %4d  %s" % (v, " / ".join(k)))
    print("groups now %d; admissions at %d address(es)" % (len(G), len(admit)))
    for X, items in sorted(admit.items()):
        print("   %s %s <- %d: %s" % (X, J.retail_name(int(X, 16))[:50], len(items),
                                   [f[:50] for _n, f, *_ in items][:4]))
    if a.json:
        Path(a.json).write_text(json.dumps(rows, indent=1) + "\n")
    if a.write:
        LEDGER.write_text(json.dumps(doc, indent=1, ensure_ascii=False) + "\n")
        print("wrote", LEDGER)
    return 0


def null_interior_rows(write):
    """C: a folded spelling the MAP places at another address is either a second
    home for one name (impossible in a linked image) or a bad map row.  Null the
    row only when retail says the address is NOT a function entry: no dtk symbol
    starts there, it is no .pdata BeginAddress, and nothing in retail branches
    to it, stores it as a data word, or materialises it with lis/addi."""
    import icf_pair_adjudicate as A
    import alias_survivor_drift as D
    MAP = ROOT / "scripts/target_symbol_map.json"
    m = json.load(open(MAP))
    applied = D.applied_names()
    addr_of = collections.defaultdict(list)
    for va, n in applied.items():
        addr_of[n].append(va)
    tgt, _ours = A.load_sides()
    tgt_at = {A._name_addr(k) for k in tgt}
    doc = json.load(open(LEDGER))
    found = []
    for g in doc["groups"]:
        if not g.get("address"):
            continue
        for f in g.get("folded", []):
            for va in addr_of.get(f, []):
                if hex(va) == g["address"]:
                    continue
                r = A.retail_refs_to(va)
                img = A.retail_image()
                words = [img.word(va - 4), img.word(va)]
                interior = (not r["branch"] and not r["data"] and not r["halo"]
                            and not r["pdata"] and va not in tgt_at)
                found.append((g["address"], f, va, interior, r, words))
    rows = []
    for X, f, va, interior, r, words in found:
        nb = len(A.retail_refs_to(int(X, 16))["branch"])
        print("%s folded %s; map row %s: %s (refs branch=%d data=%d halo=%d pdata=%s; "
              "word before 0x%08x, at 0x%08x); group address has %d branch refs"
              % (X, f[:60], hex(va), "INTERIOR -> null" if interior else "KEPT (an entry)",
                 len(r["branch"]), len(r["data"]), len(r["halo"]), r["pdata"],
                 words[0] or 0, words[1] or 0, nb))
        if interior:
            rows.append((hex(va), f, X, words, nb))
    if write and rows:
        for k, f, X, words, nb in rows:
            assert m.get(k) == f, (k, m.get(k), f)
            m[k] = None
        m["_w16ou_interior_rows_comment"] = (
            "Rows NULLED by lane W16-OU (2026-10-03), tools/alias_locate_home.py "
            "--null-interior-rows. Each named a function that scripts/symbol_aliases.json "
            "already folds at its real address, and each address is INSIDE another "
            "function: no dtk symbol starts there, it is no .pdata BeginAddress, and "
            "retail holds no branch, data word or lis/addi pair naming it "
            "(icf_pair_adjudicate.retail_refs_to). The fold addresses are called: "
            + "; ".join("%s %s (word 0x%08x) -> folded at %s, %d branch refs"
                        % (k, f, words[1] or 0, X, nb) for k, f, X, words, nb in rows)
            + ". All four rows date to the TU0->TU5 address re-point (a320bc121) or the "
            "round-4 scanner stack (13fe51646).")
        MAP.write_text(json.dumps(m, indent=1, ensure_ascii=False) + "\n")
        print("nulled %d row(s) in %s" % (len(rows), MAP))
    return 0


if __name__ == "__main__":
    sys.exit(main())
