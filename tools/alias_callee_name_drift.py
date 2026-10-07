#!/usr/bin/env python3
"""Re-prove an alias group when the map renames a callee its proof reads.

WHY THIS EXISTS (lane W16-TV, 2026-10-07)
------------------------------------------
A fold proof compares RELOCATION TARGET NAMES, and the retail side of every
name comes from scripts/target_symbol_map.json.  So a proof is a function of
the map, not only of the bytes -- and the map is edited every day.  Two
installed memberships went wrong exactly that way (lane W16-TT,
docs/decomp/W16TT_FOLD_LEADS_ON_RETAIL_2026-10-07.md §2-§3):

  * group 771 was recorded PROVEN on 2026-09-15 while the map put OUR spelling
    on the depth-1 callee 0x82773E70 (a twin body).  W16-SG renamed that callee
    on 2026-10-07; the group's own survivor did not change, so nothing re-chased
    it, and it kept a record that its own tool refuted.
  * groups 723/875/1201/1223 were proven while their depth-1 callees were
    UNNAMED (flat T1 tolerated the fn_ slot).  The map named those callees on
    2026-09-30, and nothing looked again.

`tools/alias_survivor_drift.py` (W16-OS) pins the SURVIVOR's name.  This pins
the names of the survivor's CALLEES, to depth DEPTH through retail's own
relocations, and re-proves the group when one of them changes.

THE SNAPSHOT  (scripts/alias_callee_names.json)
-----------------------------------------------
Per placed group with >=1 folded member: the survivor, the retail callee
addresses reachable from the survivor's body in <= DEPTH relocation steps, and
the verdict tools/icf_pair_adjudicate.membership_verdict gave each folded
member when the snapshot was written.  Callee names live in one shared table
{va: applied map name, or null for unnamed}.

MODES
-----
  --check          (no build needed; WIRED into the build next to the survivor
                   drift edge) -- a group is STALE when it is not recorded, its
                   survivor changed, a folded member has no recorded verdict, or
                   the applied map name at any recorded callee address differs
                   from the recorded one.  rc 1 on any stale group.
  --reprove        (needs a BUILT tree: renamed target objs + our objs) --
                   re-prove every membership of every stale group with today's
                   comparators.  A REGRESSION is a member that was PROVEN and is
                   not now, or any member that now reads REFUTED.  rc 1 on any.
  --reprove --write  also refresh the snapshot for every stale group WITHOUT a
                   regression.  A regressed group keeps its old record, so the
                   build stays red until the bad membership is withdrawn (with a
                   record) -- the check never launders a refutation.
  --record-all     re-prove EVERY group and rewrite the whole snapshot (same
                   regression rule; regressed groups are not written).
  --selftest       frozen fixtures for --check, plus the planted group-771
                   timeline (needs a built tree): the check must flag the group
                   the survivor-drift check passes, and the re-proof must refute.

    python3 tools/alias_callee_name_drift.py --check
    python3 tools/alias_callee_name_drift.py --reprove --write
"""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))
sys.path.insert(0, str(ROOT / "tools"))
from alias_survivor_drift import applied_names  # noqa: E402
import stamp_if_changed  # noqa: E402

ALIASES = ROOT / "scripts" / "symbol_aliases.json"
MAP = ROOT / "scripts" / "target_symbol_map.json"
SNAPSHOT = ROOT / "scripts" / "alias_callee_names.json"
DEPTH = 3
ADDR_PH = ("fn_", "lbl_", "vftable_")


def _hx(va):
    return "0x%08x" % va


def placed(groups):
    """(key, group) for every group that forgives something.  The key is the
    group's address; two groups at one address are disambiguated by index."""
    seen = {}
    for i, g in enumerate(groups):
        if not g.get("address") or not g.get("folded"):
            continue
        k = g["address"].lower()
        if k in seen:
            k = "%s#%d" % (k, i)
        seen[k] = i
        yield k, g


# --------------------------------------------------------------- the check ---
def find_stale(groups, applied, snap):
    """-> {key: [reason, ...]} for every stale group.  Empty == green."""
    recs, names = snap.get("groups", {}), snap.get("callee_names", {})
    out = {}
    for k, g in placed(groups):
        r = recs.get(k)
        why = []
        if r is None:
            why.append("not recorded")
        else:
            if r.get("survivor") != g["survivor"]:
                why.append("survivor changed")
            missing = [f for f in g["folded"] if f not in r.get("verdicts", {})]
            if missing:
                why.append("%d folded member(s) without a recorded proof, e.g. %s"
                           % (len(missing), missing[0][:70]))
            for a in r.get("callees", []):
                was, now = names.get(a), applied.get(int(a, 16))
                if was != now:
                    why.append("callee %s renamed: %s -> %s"
                               % (a, (was or "<unnamed>")[:60], (now or "<unnamed>")[:60]))
                    break
        if why:
            out[k] = why
    return out


# ------------------------------------------------------------- re-proving ---
def neighbourhood(tgt, survivor, va_of, depth=DEPTH):
    """Retail callee VAs reachable from `survivor`'s body in <= depth steps."""
    if survivor not in tgt:
        return []
    seen, done, front = set(), {survivor}, [survivor]
    for _ in range(depth):
        nxt = []
        for n in front:
            for (_o, c, _t) in tgt[n][1]:
                va = va_of(c)
                if va is not None:
                    seen.add(va)
                if c in tgt and c not in done:
                    done.add(c)
                    nxt.append(c)
        front = nxt
    return sorted(seen)


def make_va_of(applied):
    inv = {}
    for va, n in applied.items():
        inv.setdefault(n, va)

    def va_of(n):
        if n in inv:
            return inv[n]
        if n.startswith(ADDR_PH):
            try:
                return int(n.split("_", 1)[1], 16)
            except ValueError:
                return None
        return None
    return va_of


def reprove(groups, keys, tgt, ours, mapped, applied, snap, depth=DEPTH):
    """Re-prove the memberships of `keys`.  -> (new records, regressions)."""
    import icf_pair_adjudicate as m
    va_of = make_va_of(applied)
    recs = snap.get("groups", {})
    new, regress = {}, {}
    byk = dict(placed(groups))
    for k in keys:
        g = byk.get(k)
        if g is None:
            continue
        old = recs.get(k, {}).get("verdicts", {})
        ver, bad = {}, []
        for f in g["folded"]:
            v, why, _tr = m.membership_verdict(tgt, ours, g["survivor"], f, mapped)
            ver[f] = v
            was = old.get(f)
            if v == "REFUTED" or (was == "PROVEN" and v != "PROVEN"):
                bad.append((f, was, v, why))
        callees = neighbourhood(tgt, g["survivor"], va_of, depth)
        new[k] = {"survivor": g["survivor"], "callees": [_hx(a) for a in callees],
                  "verdicts": ver}
        if bad:
            regress[k] = bad
    return new, regress


def write_snapshot(snap, new, applied, depth, path=SNAPSHOT):
    recs = dict(snap.get("groups", {}))
    recs.update(new)
    names = {}
    for r in recs.values():
        for a in r["callees"]:
            names[a] = applied.get(int(a, 16))
    try:
        head = subprocess.run(["git", "-C", str(ROOT), "rev-parse", "--short=9", "HEAD"],
                              capture_output=True, text=True).stdout.strip()
    except OSError:
        head = "?"
    doc = {
        "_comment": ("Written by tools/alias_callee_name_drift.py (lane W16-TV). Per placed "
                     "alias group: the retail callee addresses its survivor reaches in <= "
                     "`depth` relocation steps, and the membership_verdict of each folded "
                     "member when recorded. `callee_names` holds the applied map name at "
                     "each address (null = unnamed). The build's --check re-proves a group "
                     "when any of those names changes. Do not edit by hand: run --reprove "
                     "--write on a built tree."),
        "depth": depth,
        "recorded_at": head,
        "callee_names": dict(sorted(names.items())),
        "groups": dict(sorted(recs.items())),
    }
    path.write_text(json.dumps(doc, indent=1, sort_keys=False) + "\n")


def load_snapshot(path=SNAPSHOT):
    if not Path(path).exists():
        return {"groups": {}, "callee_names": {}, "depth": DEPTH}
    return json.loads(Path(path).read_text())


def print_regressions(regress, out=sys.stderr):
    for k, bad in sorted(regress.items()):
        for f, was, v, why in bad:
            print("  %s  %s  %s -> %s  %s" % (k, f[:80], was or "(new)", v, why[:200]),
                  file=out)


# --------------------------------------------------------------- selftest ---
def selftest(with_tree=True) -> int:
    ok = True

    def check(label, cond):
        nonlocal ok
        print("  [%s] %s" % ("PASS" if cond else "FAIL", label))
        ok &= bool(cond)

    A, B = "0x82000010", "0x82000100"
    applied = {0x82000010: "?s@@YAXXZ", 0x82000100: "?callee@@YAXXZ"}
    groups = [{"address": A, "survivor": "?s@@YAXXZ", "folded": ["?f@@YAXXZ"]},
              {"address": "0x82000020", "survivor": "?t@@YAXXZ", "folded": []}]
    snap = {"groups": {A: {"survivor": "?s@@YAXXZ", "callees": [B],
                           "verdicts": {"?f@@YAXXZ": "PROVEN"}}},
            "callee_names": {B: "?callee@@YAXXZ"}}
    check("healthy snapshot: nothing stale", find_stale(groups, applied, snap) == {})
    check("a group with no folded member is exempt",
          "0x82000020" not in find_stale(groups, applied, {"groups": {}}))
    for label, ap2, g2, s2 in (
        ("callee renamed", {**applied, 0x82000100: "?other@@YAXXZ"}, groups, snap),
        ("callee named (was unnamed)", applied, groups,
         {**snap, "callee_names": {B: None}}),
        ("callee unnamed (was named)", {0x82000010: "?s@@YAXXZ"}, groups, snap),
        ("group not recorded", applied, groups, {"groups": {}, "callee_names": {}}),
        ("new folded member", applied,
         [{**groups[0], "folded": ["?f@@YAXXZ", "?g@@YAXXZ"]}], snap),
        ("survivor changed", applied, [{**groups[0], "survivor": "?s2@@YAXXZ"}], snap),
    ):
        check("stale fires: " + label, A in find_stale(g2, ap2, s2))
    check("a WITHDRAWN member is not stale", find_stale(
        [{**groups[0], "folded": ["?f@@YAXXZ"]}], applied,
        {**snap, "groups": {A: {**snap["groups"][A],
                                "verdicts": {"?f@@YAXXZ": "PROVEN", "?gone@@YAXXZ": "PROVEN"}}}}) == {})
    if with_tree:
        ok &= planted_771()
    print("SELFTEST:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


def planted_771() -> bool:
    """The 2026-09-15 -> 2026-10-07 timeline of group 771, replayed in memory.

    09-15: the map names callee 0x82773E70 with OUR spelling, and the membership
    is recorded PROVEN.  10-07: W16-SG renames 0x82773E70.  The survivor-drift
    check must PASS (the survivor never changed -- that is the gap), this check
    must flag the group, and the re-proof must REFUTE the membership."""
    import icf_pair_adjudicate as m
    from alias_survivor_drift import find_drift
    sys.path.insert(0, str(ROOT / "tools"))
    from test_alias_proof_gaps import (G771_ADDR, G771_FOLDED, OURS_DRA, OLD,
                                       plant_771, under)
    tgt, ours = m.load_sides()
    mapped = m.load_mapped()
    applied = applied_names()
    groups = json.loads(ALIASES.read_text())["groups"]
    g = next(x for x in groups if (x.get("address") or "").lower() == G771_ADDR)
    g = dict(g, folded=[G771_FOLDED])                     # the 09-15 membership
    dra_va = 0x82773E70
    # 09-15 world: map names 0x82773E70 with our spelling
    applied_0915 = dict(applied)
    applied_0915[dra_va] = OURS_DRA
    tgt_0915, mapped_0915 = plant_771(tgt, mapped)
    snap = {"groups": {}, "callee_names": {}}
    # recorded with the comparators of the day (pre-W16-TV), so this leg tests
    # the DRIFT CHECK alone -- not the same-name read, which would refuse it
    new, regress = under(OLD, lambda: reprove([g], [G771_ADDR], tgt_0915, ours,
                                              mapped_0915, applied_0915, snap))
    rec = new.get(G771_ADDR, {})
    print("    09-15 record: verdict %s, %d callee(s) recorded, 0x82773e70 in them: %s"
          % (rec.get("verdicts", {}).get(G771_FOLDED), len(rec.get("callees", [])),
             "0x82773e70" in rec.get("callees", [])))
    ok = rec.get("verdicts", {}).get(G771_FOLDED) == "PROVEN" and not regress
    names = {a: applied_0915.get(int(a, 16)) for a in rec.get("callees", [])}
    snap = {"groups": {G771_ADDR: rec}, "callee_names": names}
    # 10-07 world: today's map (0x82773E70 renamed to RangedData<uint>)
    sd = find_drift([g], applied)
    st = find_stale([g], applied, snap)
    print("    10-07 survivor-drift check: %s ; callee-name check: %s"
          % ("drift" if sd else "PASS (blind to it)", st.get(G771_ADDR, "not stale")))
    # re-proved with the 09-15 comparators too: a renamed callee alone must be
    # enough to turn the recorded proof into a refutation
    new2, regress2 = under(OLD, lambda: reprove([g], [G771_ADDR], tgt, ours, mapped,
                                                applied, snap))
    v2 = new2.get(G771_ADDR, {}).get("verdicts", {}).get(G771_FOLDED)
    print("    10-07 re-proof: %s ; regression reported: %s" % (v2, G771_ADDR in regress2))
    good = ok and not sd and G771_ADDR in st and v2 != "PROVEN" and G771_ADDR in regress2
    print("  [%s] planted group-771 timeline: recorded PROVEN on 09-15, flagged and "
          "refuted after the 10-07 callee rename" % ("PASS" if good else "FAIL"))
    return good


# ------------------------------------------------------------------- main ---
def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--aliases", default=str(ALIASES))
    ap.add_argument("--map", default=str(MAP))
    ap.add_argument("--snapshot", default=str(SNAPSHOT))
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--reprove", action="store_true")
    ap.add_argument("--record-all", action="store_true")
    ap.add_argument("--write", action="store_true")
    ap.add_argument("--depth", type=int, default=None)
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--no-tree", action="store_true",
                    help="--selftest: frozen fixtures only (no built tree needed)")
    ap.add_argument("--quiet", action="store_true", help="print nothing when green")
    stamp_if_changed.add_arguments(ap)
    a = ap.parse_args(argv)
    if a.selftest:
        return selftest(with_tree=not a.no_tree)
    groups = json.loads(Path(a.aliases).read_text()).get("groups", [])
    applied = applied_names(a.map)
    snap = load_snapshot(a.snapshot)
    depth = a.depth or snap.get("depth", DEPTH)
    n_placed = sum(1 for _ in placed(groups))
    if not n_placed:
        print("REFUSING: %s declares no placed group with a folded member" % a.aliases,
              file=sys.stderr)
        return 2
    stale = find_stale(groups, applied, snap)

    if a.reprove or a.record_all:
        import icf_pair_adjudicate as m
        tgt, ours = m.load_sides()
        mangled = sum(1 for n in tgt if n.startswith("?"))
        if mangled < 1000:
            print("REFUSING: target objs look PRE-RENAMER (%d mangled names) -- build "
                  "the tree first" % mangled, file=sys.stderr)
            return 2
        keys = [k for k, _g in placed(groups)] if a.record_all else sorted(stale)
        new, regress = reprove(groups, keys, tgt, ours, m.load_mapped(), applied, snap,
                               depth)
        import collections
        c = collections.Counter(v for r in new.values() for v in r["verdicts"].values())
        print("[alias-callee] re-proved %d group(s), %d membership(s): %s"
              % (len(new), sum(c.values()), dict(c)))
        print("[alias-callee] same-name slots read: %s" % dict(m.SAMENAME_TALLY))
        if regress:
            print("[alias-callee] REGRESSED: %d group(s) -- not written:" % len(regress),
                  file=sys.stderr)
            print_regressions(regress)
        if a.write:
            ok_new = {k: r for k, r in new.items() if k not in regress}
            # drop records of groups that no longer forgive anything
            live = {k for k, _g in placed(groups)}
            snap["groups"] = {k: r for k, r in snap.get("groups", {}).items() if k in live}
            write_snapshot(snap, ok_new, applied, depth, Path(a.snapshot))
            print("[alias-callee] wrote %d group record(s) to %s" % (len(ok_new), a.snapshot))
        return 1 if regress else 0

    if not stale:
        if not a.quiet:
            print("[alias-callee] OK: %d placed groups, every recorded callee name is "
                  "unchanged and every folded member has a recorded proof" % n_placed)
        stamp_if_changed.apply(a)
        return 0
    print("", file=sys.stderr)
    print("ALIAS CALLEE-NAME DRIFT: %d of %d placed groups in %s must be re-proved:"
          % (len(stale), n_placed, a.aliases), file=sys.stderr)
    for k, why in sorted(stale.items())[:40]:
        print("  %s  %s" % (k, "; ".join(why)[:200]), file=sys.stderr)
    if len(stale) > 40:
        print("  ... and %d more" % (len(stale) - 40), file=sys.stderr)
    print("A fold proof compares relocation target NAMES, and the retail names come from "
          "the map,\nso a renamed callee can turn a recorded proof into a refutation "
          "(group 771, W16-TT).\nFix: python3 tools/alias_callee_name_drift.py --reprove "
          "--write  (on a built tree;\nit re-proves and refuses to record any group whose "
          "membership no longer proves).", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
