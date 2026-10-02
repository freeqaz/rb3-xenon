#!/usr/bin/env python3
"""W16-OA: make `survivor` and placed `address` unique per group in
scripts/symbol_aliases.json again -- by repairing the 12 duplications, not by
relaxing tools/test_alias_group_key.py / test_icf_alias_withdrawal_guard.py.

WHY THE TESTS ARE RIGHT.  objdiff buckets the rendered alias map BY ADDRESS, so
two placed groups at one address silently merge into one equivalence class; and
the withdrawal denylist (tools/alias_withdrawals.py) keys on (survivor|address,
spelling), so a duplicated key lets one group's withdrawal deny another group's
live, proven member.  Both were happening (see the doc for the cases).

WHY THEY DRIFTED UNSEEN.  Four of the five alias test files are pytest-only and
had no __main__, so `python3 tools/test_*.py` -- the gate lanes ran -- executed
zero tests and exited 0.

HOW EACH CASE IS DECIDED.  Every membership this tool keeps, adds, moves or
withdraws is RE-ADJUDICATED here, on retail bytes, with
tools/icf_pair_adjudicate.chase, and the tool REFUSES if any verdict differs
from the one the decision rests on.  The rule:
  * a member PROVEN at an address stays/goes live there;
  * a member CONTRADICTED at an address (positive evidence, e.g. retail stores
    another class's vtable) is withdrawn from it;
  * a member merely UNPROVEN (our build compiles no body for it -- so it can
    forgive no call site of ours) is carried unchanged: render-neutral, the same
    choice ALIAS-CONSOLIDATION made for its closure residue.
  * no withdrawal-denylist key is lost except (a) keys that deny a membership
    PROVEN live at that same address (moved to `restored`, W9-D's pattern), and
    (b) keys whose survivor is a stale label no generator can propose; the
    accounting is printed and asserted.

    python3 tools/w16oa_dedup_alias_groups.py            # dry run: verdicts + key diff
    python3 tools/w16oa_dedup_alias_groups.py --write
"""
import argparse
import copy
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
LEDGER = ROOT / "scripts/symbol_aliases.json"
MAP = ROOT / "scripts/target_symbol_map.json"
LANE = "W16-OA 2026-10-02"

# address merges: (address, index-free key of the OLD group = its survivor)
MERGES = ["0x82706208", "0x822cb4d8", "0x822d70e0", "0x82810ce0"]


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


class Judge:
    """chase() verdicts on the live tree, with the trace kinds kept."""

    def __init__(self):
        import icf_pair_adjudicate as A
        self.A = A
        self.mapped = A.load_mapped()
        self.tgt, self.ours = A.load_sides()
        self.log = []

    def __call__(self, addr, ours, want):
        A = self.A
        s = A._retail_name_at(int(addr, 16))
        tr = []
        if s not in self.tgt or ours not in self.ours:
            got, kinds = "UNPROVEN", ["MISSING(%s)" % ("retail" if s not in self.tgt else "ours")]
        else:
            ok = A.chase(self.tgt, self.ours, s, ours, self.mapped, out=tr)
            kinds = [k for _d, k, _x, _y in tr]
            if ok:
                got = "PROVEN"
            elif any(k.startswith("SLOT-CONTRADICTED") or k in ("BYTES-DIFFER",)
                     for k in kinds):
                got = "CONTRADICTED"
            else:
                got = "UNPROVEN"
        sz = ""
        if s in self.tgt and ours in self.ours:
            sz = " retail %d B / ours %d B;" % (self.tgt[s][2], self.ours[ours][2])
        ev = ("tools/icf_pair_adjudicate.py chase(%s @ %s, ours): %s;%s trace %s"
              % (s[:60], addr, got, sz, ", ".join(k for k in kinds if not k.startswith("SLOT-OK"))[:300] or "clean"))
        self.log.append((addr, ours, want, got))
        if got != want:
            raise SystemExit("REFUSING: %s @ %s is %s, the decision rests on %s.\n  %s"
                             % (ours[:80], addr, got, want, ev))
        return ev


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--write", action="store_true")
    a = ap.parse_args()
    doc = json.load(open(LEDGER))
    G = doc["groups"]
    m = json.load(open(MAP))
    before = deny_keys(G)
    n0 = len(G)
    judge = Judge()
    drop = set()          # ids of groups removed
    restored_keys = set() # denylist keys removed on purpose (PROVEN live)
    accepted_keys = set() # removed with a stated reason (see each site)
    stale_wd = {}         # stale survivor -> its withdrawals not restored

    def by_addr(a):
        return [g for g in G if g.get("address") == a]

    def by_surv(s):
        return [g for g in G if g["survivor"] == s]

    def absorbed(into, g, why):
        # every withdrawal record of the removed group is carried verbatim into a
        # live `withdrawn` (or into `restored`'s superseded_records), so the
        # history copy keeps only their count -- not a second copy of each
        gc = copy.deepcopy(g)
        if gc.get("withdrawn"):
            gc["withdrawn"] = {"n_records": len(gc["withdrawn"]),
                               "disposition": "carried into a live `withdrawn` list, or into "
                                              "`restored` where re-adjudicated PROVEN (lane %s)" % LANE}
        into.setdefault("absorbed", []).append({"lane": LANE, "why": why, "group": gc})

    # ---- 1. address-less partition twins identical to their placed group ----
    for addr in ("0x82557060", "0x822a6ce8", "0x82483d90", "0x82483d40", "0x82714c98"):
        (p,) = by_addr(addr)
        twins = [g for g in by_surv(p["survivor"]) if g is not p]
        assert len(twins) == 1 and twins[0].get("address") is None, addr
        t = twins[0]
        assert t["repair"]["origin_address"] == addr
        if t.get("folded"):
            assert t["folded"] == p["folded"] and not t.get("withdrawn"), addr
            for f in p["folded"]:
                judge(addr, f, "PROVEN")
            why = ("Address-less partition twin of this group with the IDENTICAL survivor and "
                   "folded list: its class was restored into this placed group by a later lane "
                   "(see `restored`), leaving two groups keyed on one survivor. Removed; each "
                   "membership re-adjudicated PROVEN here.")
        else:
            # the empty shell W16-NR left: its one record denies THIS group's live member
            (w,) = t["withdrawn"]
            assert w["class"] == "MOVED_FROM_ADDRESSLESS_PARTITION" and w["spelling"] in p["folded"]
            judge(addr, w["spelling"], "PROVEN")
            restored_keys.add(("S", t["survivor"], w["spelling"]))
            why = ("Empty address-less shell left by W16-NR after it moved this membership here. "
                   "Its only withdrawal record (disposition 'membership moved, group kept') was "
                   "keyed on the SAME survivor, so the denylist denied this group's own live "
                   "member. Removed; the member re-adjudicated PROVEN here.")
        absorbed(p, t, why)
        drop.add(id(t))

    # ---- 2. ~ObjPtr<RndTex> held as survivor at 0x8229d930 and 0x8243dd20 ----
    (real,) = by_addr("0x8229d930")
    (wrap,) = by_addr("0x8243dd20")
    assert real["survivor"] == wrap["survivor"] == m["0x8229d930"] and m.get("0x8243dd20") is None
    assert not wrap.get("folded")
    for w in wrap["withdrawn"]:
        w2 = dict(w, carried_from={"address": "0x8243dd20", "lane": LANE})
        real.setdefault("withdrawn", []).append(w2)
        # 0x8243dd20 is an unnamed wrapper (map row null): no map-resident
        # survivor exists there for a generator to propose, so its address key
        # guards nothing; the survivor key is carried above.
        accepted_keys.add(("A", "0x8243dd20", spelling(w)))
    absorbed(real, wrap, "Group at 0x8243dd20 whose survivor is this group's survivor: W17-OPTR "
             "moved the name to 0x8229d930 (the real 100 B body; 0x8243dd20 is an unnamed 4-byte "
             "`b 0x8229d930` wrapper) and left the old group carrying it. Its withdrawal record "
             "is carried here (survivor key preserved; its address key moves with it).")
    drop.add(id(wrap))

    # ---- 3. BandCamShot list::insert at 0x824ce130 and 0x822b55e0 -- relabel chain ----
    (g621,) = by_addr("0x824ce130")
    (g2046,) = by_addr("0x822b55e0")
    (gpsm,) = by_addr("0x8230ed80")
    assert g621["survivor"] == g2046["survivor"] == m["0x822b55e0"]
    assert gpsm["survivor"] == m["0x824ce130"]
    assert {spelling(w) for w in g621["withdrawn"]} <= {spelling(w) for w in g2046["withdrawn"]}
    ev = judge("0x8230ed80", m["0x8230ed80"], "PROVEN")
    old = gpsm["survivor"]
    keep, sup = [], []
    for w in gpsm["withdrawn"]:
        (sup if spelling(w) == m["0x8230ed80"] else keep).append(w)
    assert len(sup) == 1
    gpsm["withdrawn"] = keep
    gpsm["survivor"] = m["0x8230ed80"]
    gpsm["relabelled"] = {"from": old, "lane": LANE, "evidence": ev,
                          "why": "Survivor label matched the map row at 0x824ce130, not this "
                                 "address; relabelled to the map name here so 0x824ce130's group "
                                 "can carry its own map name."}
    gpsm.setdefault("restored", []).append({
        "spelling": m["0x8230ed80"], "lane": LANE,
        "verdict": "SURVIVOR -- PROVEN on retail bytes: " + ev,
        "why_the_original_premise_failed": "ALIAS-CONSOLIDATION withdrew it as one member of a "
            "closure shared by 10 groups ('nothing says which'); the map now names this address "
            "with it and the chase proves it, so this is the one address it belongs to.",
        "superseded_records": sup})
    restored_keys |= {("S", old, m["0x8230ed80"]), ("A", "0x8230ed80", m["0x8230ed80"])}
    ev = judge("0x824ce130", m["0x824ce130"], "PROVEN")
    old = g621["survivor"]
    g621["survivor"] = m["0x824ce130"]
    g621["relabelled"] = {"from": old, "lane": LANE, "evidence": ev,
                          "why": "W16-NH re-homed this label to 0x822b55e0 (group there) and left "
                                 "it here too, so one survivor named two groups. Relabelled to the "
                                 "map name at this address; the old label's withdrawal keys are "
                                 "identical in the 0x822b55e0 group."}

    # ---- 4. CVEIN-1 group at 0x82594990: survivor's true address is 0x82271e70 ----
    (cv,) = [g for g in by_addr("0x82594990") if g["survivor"] == m["0x82271e70"]]
    (ha,) = [g for g in by_addr("0x82594990") if g["survivor"] == m["0x82594990"]]
    assert not cv.get("folded") and not by_addr("0x82271e70")
    for f in [ha["survivor"]] + ha["folded"]:
        if f == ha["survivor"]:
            ev = judge("0x82594990", f, "PROVEN")
        else:
            judge("0x82594990", f, "PROVEN")
    sup = [w for w in cv["withdrawn"] if spelling(w) in [ha["survivor"]] + ha["folded"]]
    cv["address"] = "0x82271e70"
    cv["readdressed"] = {"from": "0x82594990", "to": "0x82271e70", "lane": LANE,
                         "why": "The map moved this survivor to 0x82271e70 and named 0x82594990 "
                                "for set<ScoreType>::insert_unique (W16-HA's group there). This "
                                "group kept the old address, so two placed groups shared it and "
                                "its records denied, at 0x82594990, the survivor and a PROVEN "
                                "member of the group that now owns it. Re-addressed; its records "
                                "stay keyed on its own survivor, which is what they are about."}
    for w in sup:
        restored_keys.add(("A", "0x82594990", spelling(w)))
    ha.setdefault("restored", []).append({
        "lane": LANE, "spellings": [spelling(w) for w in sup],
        "verdict": "PROVEN at 0x82594990 (re-adjudicated: " + ev + ")",
        "why_the_original_premise_failed": "The W4b-DUALWIT records compare these spellings with "
            "map<int,float>::insert_unique, whose body is at 0x82271e70; they remain true there "
            "and stay in that group. They only denied these spellings at 0x82594990 because that "
            "group sat at the wrong address.",
        "superseded_records": copy.deepcopy(sup)})

    # ---- 5. address merges: stale-survivor group beside the map-name group ----
    new_groups = []
    for X in MERGES:
        gs = by_addr(X)
        (B,) = [g for g in gs if g["survivor"] == m[X]]
        (A,) = [g for g in gs if g is not B]
        S = B["survivor"]
        for f in B["folded"]:
            judge(X, f, "PROVEN")
        live = {S} | set(B["folded"])
        merged_folded = list(B["folded"])
        carried, wd_new = [], []
        # the stale survivor
        aS = A["survivor"]
        if aS in m.values():
            ev = judge(X, aS, "CONTRADICTED")
            true = [k for k, v in m.items() if v == aS]
            wd_new.append({"spelling": aS, "lane": LANE, "class": "FORMER_SURVIVOR_LABEL_NOT_A_FOLD",
                           "evidence": ev + ". The map places this spelling at %s; it was this "
                           "address's survivor label in a group that a later lane duplicated." % true})
        else:
            ev = judge(X, aS, "UNPROVEN")
            if aS not in live:
                merged_folded.append(aS)
                carried.append({"spelling": aS, "lane": LANE, "from": "former survivor label",
                                "evidence": ev + ". Carried unchanged (render-neutral): our build "
                                "compiles no body for it, so it can forgive no call site of ours."})
        for f in A.get("folded", []):
            if f in live or f in merged_folded:
                continue
            want = "CONTRADICTED" if "RndPartLauncher" in f else "UNPROVEN"
            ev = judge(X, f, want)
            if want == "CONTRADICTED":
                wd_new.append({"spelling": f, "lane": LANE, "class": "SLOT_CONTRADICTED_AT_MERGE",
                               "evidence": ev})
                # its _Copy_Construct sibling is the map name at 0x822d7150
                assert "RndPartLauncher" in m["0x822d7150"] and "_Copy_Construct" in m["0x822d7150"]
                new_groups.append(("0x822d7150", f))
            else:
                merged_folded.append(f)
                carried.append({"spelling": f, "lane": LANE, "from": "folded in the merged group",
                                "evidence": ev + ". Carried unchanged (render-neutral)."})
        restored = []
        for w in A.get("withdrawn", []) or []:
            sp = spelling(w)
            if sp in live:
                ev = judge(X, sp, "PROVEN") if sp != S else "survivor = map name at %s" % X
                restored.append((sp, w))
                restored_keys |= {("S", aS, sp), ("A", X, sp)}
            else:
                wd_new.append(w)
                stale_wd.setdefault(aS, []).append(w)
        B["folded"] = merged_folded
        B.setdefault("withdrawn", []).extend(wd_new)
        if carried:
            B.setdefault("carried", []).extend(carried)
        bys = {}
        for sp, w in restored:
            bys.setdefault(sp, []).append(w)
        for sp, ws in bys.items():
            B.setdefault("restored", []).append({
                "spelling": sp, "lane": LANE,
                "verdict": "PROVEN on retail bytes at %s, re-adjudicated by this lane%s" % (
                    X, "" if sp == S else ": " + judge(X, sp, "PROVEN")),
                "why_the_original_premise_failed": "The record was written in the stale-survivor "
                    "group that shared this address; the admitting lane overrode it in prose by "
                    "opening a second group here instead of naming it. Merged, the record would "
                    "deny this group's own proven member, so it is superseded.",
                "superseded_records": [dict(w, group_survivor=aS) if isinstance(w, dict)
                                       else {"spelling": w, "group_survivor": aS} for w in ws]})
        absorbed(B, A, "Second placed group at %s (survivor %s, a stale label). Merged under the "
                 "map-name survivor; see `restored`, `carried` and this lane's `withdrawn` records."
                 % (X, aS[:80]))
        drop.add(id(A))

    # the contradicted RndPartLauncher member's real home
    for addr, f in new_groups:
        assert not by_addr(addr)
        ev = judge(addr, f, "PROVEN")
        # its survivor was the stale label at 0x822cb4d8; carry that group's
        # withdrawals so the denials keyed on this survivor survive the merge
        carried_wd = [dict(w, carried_from={"address": "0x822cb4d8", "lane": LANE})
                      if isinstance(w, dict) else w for w in stale_wd.get(m[addr], [])]
        G.append({"name": "_Copy_Construct", "address": addr, "survivor": m[addr], "folded": [f],
                  "withdrawn": carried_wd,
                  "evidence": "Opened by W16-OA 2026-10-02: membership admitted on retail bytes "
                              "(see 'admitted').",
                  "admitted": [{"spelling": f, "lane": LANE,
                                "evidence": ev + ". Moved here from 0x822cb4d8, where retail "
                                "stores another class's vtable (CONTRADICTED)."}]})

    G[:] = [g for g in G if id(g) not in drop]
    after = deny_keys(G)

    # ---- invariants the two tests pin ----
    survs = [g["survivor"] for g in G]
    addrs = [g["address"] for g in G if g.get("address")]
    assert len(survs) == len(set(survs)), "survivor not unique"
    assert len(addrs) == len(set(addrs)), "address not unique"
    unplaced = [g for g in G if not g.get("address")]
    assert unplaced and len(unplaced) < 0.10 * len(G)

    lost = before - after
    unexplained = lost - restored_keys - accepted_keys
    # remaining losses must be survivor keys of a stale label that is not the
    # map name anywhere (no map-resident generator can propose it), or of a
    # label the merged/relabelled group still denies by address
    mapnames = set(v for v in m.values() if isinstance(v, str))
    bad, live_at_true = [], set()
    for k in sorted(unexplained):
        kind, who, sp = k
        if kind == "S" and who not in mapnames:
            continue
        if kind == "S" and ("A", [kk for kk, v in m.items() if v == who][0], sp) in after:
            continue
        if kind == "S":
            # the stale label's TRUE map address holds `sp` LIVE: a denial keyed
            # on the label that names that address would deny that group's own
            # live member -- the contradiction the uniqueness tests guard against
            t = [kk for kk, v in m.items() if v == who][0]
            if any(sp in g.get("folded", []) for g in G if g.get("address") == t):
                live_at_true.add(k)
                continue
        if kind == "A" and ("S", m.get(who), sp) in after:
            continue
        bad.append(k)
    print("groups %d -> %d; denylist keys %d -> %d (lost %d: %d restored-on-PROVEN, "
          "%d accepted, %d stale-label/covered, %d UNEXPLAINED; gained %d)"
          % (n0, len(G), len(before), len(after),
             len(lost), len(lost & restored_keys), len(lost & accepted_keys),
             len(unexplained) - len(bad), len(bad), len(after - before)))
    import collections
    print("  lost-by-kind:", dict(collections.Counter(
        ("restored" if k in restored_keys else "accepted" if k in accepted_keys
         else "bad" if k in bad else "live-at-true-address" if k in live_at_true
         else "stale/covered", k[0], k[1][:40]) for k in lost)))
    for k in bad[:20]:
        print("  UNEXPLAINED", k[0], k[1][:60], k[2][:60])
    if bad:
        raise SystemExit("REFUSING: a denylist key would be lost without a reason")
    print("verdicts: %d adjudicated, all as the decisions assume" % len(judge.log))
    if a.write:
        LEDGER.write_text(json.dumps(doc, indent=1, ensure_ascii=False) + "\n")
        print("wrote", LEDGER)
    return 0


if __name__ == "__main__":
    sys.exit(main())
