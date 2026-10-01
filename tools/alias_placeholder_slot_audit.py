#!/usr/bin/env python3
"""Audit every landed ICF alias membership for the PLACEHOLDER-SLOT hole (W16-JG).

THE HOLE.  Both flat T1 (`icf_alias_build.relocs_agree`) and the general path of
`tools/icf_pair_adjudicate.py --chase` used to tolerate any relocation slot whose
retail target is a placeholder (`fn_X`, `lbl_X`, `vftable_X`).  For a ctor/dtor
that slot is the VTABLE; for a template wrapper it is the UNNAMED RETAIL CALLEE.
Lane W16-JE found 11 bad folds that way in ~300 of its own pairs
(docs/decomp/W16JE_NAME_ONLY_RELOC_PAIRS_2026-10-01.md §3).  This tool asks the
same question of EVERY membership in scripts/symbol_aliases.json.

PER MEMBERSHIP (survivor S at the group address, folded spelling F):

  1. Alignment.  retail(S) and ours(F) must have equal masked bytes and equal
     relocation shape for slots to be comparable at all.  Memberships that are
     not aligned rest on other evidence (T2/T3, thunk gates, ...); for those only
     the vtable-SET check below applies.
  2. Depth-0 sweep -- every placeholder slot, NO early break, each decided by
     `icf_pair_adjudicate.discharge_slot` (RTTI / literal / constant / callee
     chase), with the survivor's own COMDAT as the callee ANCHOR.
  3. Strict chase (SLOT_POLICY="discharge") vs lax chase ("lax", the old rule).
     lax PROVEN / strict not  ==  the membership rested on the hole.

VERDICTS
  CONTRADICTED   some slot shows POSITIVE evidence against the fold (RTTI names a
                 different class, a literal/constant differs, an anchored
                 different callee, a pigeonhole, CD-9 mapped-vs-placeholder).
                 These are withdrawn by --apply, with a record; never pruned.
  UNDISCHARGED   no contradiction, but some slot could not be proven -- the
                 membership's T1 support rested on the hole and is now UNPROVEN.
                 Reported, NOT withdrawn: "not proven" is not "refuted".
  CLEAN          every placeholder slot discharged (CLEAN-CYCLE: only through a
                 coinductive cycle assumption -- the pre-existing class).
  NOT-ALIGNED / NO-RETAIL / NO-OURS   outside this hole's reach (counted).

    python3 tools/alias_placeholder_slot_audit.py --out ~/tmp/w16jg/audit.json
    python3 tools/alias_placeholder_slot_audit.py --out A.json --apply   # withdraw
"""

import argparse
import collections
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import icf_pair_adjudicate as A  # noqa: E402
from icf_alias_build import placeholder  # noqa: E402

LANE = "W16-JG 2026-10-01"
CONTRA_KINDS = ("SLOT-CONTRADICTED", "MAPPED-VS-PLACEHOLDER")


def aligned(rt, ob):
    return (rt[0] == ob[0] and len(rt[1]) == len(ob[1]) and
            all(a[0] == b[0] and a[2] == b[2] for a, b in zip(rt[1], ob[1])))


def depth0_sweep(tgt, ours, S, F, mapped):
    """Every depth-0 placeholder slot, decided individually (no early break)."""
    rt, ob = tgt[S], ours[F]
    a = ours.get(S)
    anc = None
    if a is not None and len(a[1]) == len(rt[1]) and all(
            x[0] == y[0] and x[2] == y[2] for x, y in zip(a[1], rt[1])):
        anc = [n for (_o, n, _t) in a[1]]
    ctx, res = {}, []
    for i, ((ro, rn, _t), (_o, on, _u)) in enumerate(zip(rt[1], ob[1])):
        if rn == on:
            continue
        if rn.startswith(("fn_", "lbl_")) and on in mapped:
            res.append(("CONTRADICTED", "MAPPED-VS-PLACEHOLDER", ro, rn, on,
                        "our callee is map-resident elsewhere; retail's slot is not it"))
            continue
        if not (placeholder(rn) or placeholder(on)):
            continue
        out = []
        st, kind, det = A.discharge_slot(tgt, ours, rn, on, mapped, 0, [(S, F)], {},
                                         out, 12, ctx, anchor=anc[i] if anc else None)
        res.append((st, kind, ro, rn, on, det))
    return res


def rtti_set_check(tgt, ours, S, F):
    """For NOT-ALIGNED memberships: the classes whose vtables each body stores."""
    rt, ob = tgt.get(S), ours.get(F)
    if rt is None or ob is None:
        return None
    ours_cls = {A._vtable_class(n) for (_o, n, _t) in ob[1] if n.startswith("??_7")}
    ours_cls.discard(None)
    ret_cls = set()
    for (_o, n, _t) in rt[1]:
        x = A._ph_addr(n)
        if x is not None and n.startswith(("lbl_", "vftable_")):
            t = A.retail_rtti_name(x)
            if t:
                ret_cls.add(t[4:])
    if ours_cls and ret_cls and not (ours_cls & ret_cls):
        return ("CONTRADICTED", "VTABLE-SET-DISJOINT",
                "retail stores %s, ours %s" % (sorted(ret_cls)[:3], sorted(ours_cls)[:3]))
    return None


def audit(tgt, ours, mapped, groups):
    rows = []
    for gi, g in enumerate(groups):
        S = g["survivor"]
        for F in g.get("folded", []):
            r = dict(gi=gi, address=g["address"], survivor=S, folded=F)
            rt, ob = tgt.get(S), ours.get(F)
            if rt is None:
                r["verdict"] = "NO-RETAIL"
            elif ob is None:
                r["verdict"] = "NO-OURS"
            elif not aligned(rt, ob):
                c = rtti_set_check(tgt, ours, S, F)
                r["verdict"] = "CONTRADICTED" if c else "NOT-ALIGNED"
                if c:
                    r["slots"] = [(c[0], c[1], None, None, None, c[2])]
            else:
                sw = depth0_sweep(tgt, ours, S, F, mapped)
                A.SLOT_POLICY = "discharge"
                st_out = []
                st_ok = A.chase(tgt, ours, S, F, mapped, out=st_out)
                A.SLOT_POLICY = "lax"
                lx_ok = A.chase(tgt, ours, S, F, mapped, out=[])
                A.SLOT_POLICY = "discharge"
                kinds = collections.Counter(k for _d, k, _x, _y in st_out)
                contra = [s for s in sw if s[0] == "CONTRADICTED"] + [
                    ("CONTRADICTED", k, None, x, y, "in strict chase at depth %d" % d)
                    for d, k, x, y in st_out if k.startswith(CONTRA_KINDS)]
                r.update(n_ph=len(sw), strict=st_ok, lax=lx_ok,
                         cycle=kinds.get("CYCLE-ASSUMED", 0),
                         slot_kinds=dict(collections.Counter(s[1] for s in sw)),
                         data=[(s[4], s[3]) for s in sw if s[1] == "DATA-ACCEPTED"])
                if contra:
                    r["verdict"], r["slots"] = "CONTRADICTED", contra
                elif st_ok:
                    r["verdict"] = "CLEAN-CYCLE" if r["cycle"] else "CLEAN"
                elif lx_ok:
                    r["verdict"] = "UNDISCHARGED"
                    r["slots"] = [s for s in sw if s[0] != "OK"] or [
                        ("UNDISCHARGED", k, None, x, y, "strict chase, depth %d" % d)
                        for d, k, x, y in st_out if k.startswith("SLOT-UNDISCHARGED")]
                else:
                    r["verdict"] = "LAX-ALSO-FAILS"
            rows.append(r)
    return rows


def data_consistency(rows):
    """One of our data names must sit at ONE retail address across the file."""
    m = collections.defaultdict(lambda: collections.defaultdict(list))
    for r in rows:
        for on, rn in r.get("data", []):
            m[on][rn].append((r["address"], r["folded"]))
    return {on: {rn: len(v) for rn, v in d.items()} for on, d in m.items() if len(d) > 1}


def slot_evidence(tgt, ours, rn, on):
    """★ W16-JH.  Mechanical retail-byte evidence for ONE undischarged slot, so an
    UNPROVEN withdrawal records WHY the chase could not close it (and a later
    lane can see exactly what a port would have to change to restore it)."""
    on = (on or "").split(" | ")[0]
    X = A._ph_addr(rn or "")
    if on.startswith("??_7"):
        t = A.retail_rtti_name(X) if X else None
        cls = A._vtable_class(on)
        return "retail vtable %s RTTI %s; our class %s has %d retail vtable(s)" % (
            rn, t, cls, len(A.retail_vtables_of(cls)) if cls else -1)
    r, o = tgt.get(rn), ours.get(on)
    if r is None or o is None:
        return "retail %s %s; ours %s %s" % (rn, "present" if r else "ABSENT",
                                            on[:60], "compiled" if o else "NOT COMPILED")
    import capstone
    md = capstone.Cs(capstone.CS_ARCH_PPC, capstone.CS_MODE_32 | capstone.CS_MODE_BIG_ENDIAN)

    def ins(b, i):
        d = list(md.disasm(b[i:i + 4], i))
        return "%s %s" % (d[0].mnemonic, d[0].op_str) if d else b[i:i + 4].hex()
    first = next((i for i in range(0, min(len(r[0]), len(o[0])), 4)
                  if r[0][i:i + 4] != o[0][i:i + 4]), None)
    diff = ("first diff @+0x%x retail '%s' vs ours '%s'" % (first, ins(r[0], first), ins(o[0], first))
            if first is not None else "masked bytes equal over the common length")
    eq = [n for n, (mb, _rl, _s) in tgt.items() if mb == o[0]][:3]
    return "retail %s %d B vs our %s %d B; %s; retail bodies byte-equal to our callee: %s" % (
        rn, r[2], on[:60], o[2], diff, eq or "none")


def apply(path, rows, tgt=None, ours=None, undischarged=False, extra=None, lane=LANE):
    raw = Path(path).read_text()
    al = json.loads(raw)
    n = 0
    extra = extra or {}
    for r in rows:
        key = "%s|%s" % (r["address"], r["folded"])
        x = extra.get(key)
        if x is None and r["verdict"] != "CONTRADICTED" and not (
                undischarged and r["verdict"] == "UNDISCHARGED"):
            continue
        g = al["groups"][r["gi"]]
        assert g["address"] == r["address"] and g["survivor"] == r["survivor"]
        if r["folded"] not in g["folded"]:
            continue
        g["folded"].remove(r["folded"])
        if x is not None or r["verdict"] == "UNDISCHARGED":
            if x is not None:
                cls, ev, disp = x["class"], x["evidence"], x.get(
                    "disposition", "membership withdrawn, group kept")
            else:
                s = r["slots"][0]
                cls = "PLACEHOLDER_SLOT_UNPROVEN_" + s[1].split(":")[-1].replace("-", "_")
                disp = "membership withdrawn as UNPROVEN (NOT refuted), group kept"
                ev = ("tools/alias_placeholder_slot_audit.py (W16-JH): this membership's "
                      "T1 support rested on a relocation slot with an unnamed retail "
                      "target, which W16-JG's discharge could not close and W16-JH could "
                      "not prove on retail bytes. " + "; ".join(
                          "%s @+%s retail %s vs ours %s -- %s" % (
                              k, hex(o) if o is not None else "?", (xx or "")[:60],
                              (yy or "")[:70], slot_evidence(tgt, ours, xx, yy))
                          for _s, k, o, xx, yy, _d in r["slots"][:2]) +
                      ". RESTORE when the chase proves it (e.g. after the callee port).")
            g.setdefault("withdrawn", []).append({
                "spelling": r["folded"], "lane": lane, "class": cls,
                "disposition": disp, "evidence": ev})
            n += 1
            continue
        s = r["slots"][0]
        sup = [{"lane": x.get("lane"), "verdict": str(x.get("verdict", ""))[:160]}
               for x in g.get("restored", [])
               if isinstance(x, dict) and x.get("spelling") == r["folded"]]
        rec = {}
        if sup:
            rec["supersedes_restoration"] = sup
            rec["why_the_restoration_does_not_stand"] = (
                "the restoration rested on flat L1_T1 / chase, whose comparator "
                "tolerated a relocation slot with an unnamed retail target "
                "(icf_alias_build.relocs_agree / _slots_agree on the general "
                "path). That is exactly the slot decided here, on retail bytes.")
        g.setdefault("withdrawn", []).append({
            "spelling": r["folded"], "lane": lane,
            "class": "PLACEHOLDER_SLOT_" + s[1].split(":")[-1].replace("-", "_"),
            "disposition": "membership withdrawn, group kept",
            "evidence": ("tools/alias_placeholder_slot_audit.py: a relocation slot "
                         "whose retail target is unnamed was tolerated on trust when "
                         "this membership was admitted. Decided on retail bytes: "
                         + "; ".join("%s @+%s retail %s vs ours %s -- %s"
                                     % (k, hex(o) if o is not None else "?",
                                        (x or "")[:60], (y or "")[:70], d)
                                     for _s, k, o, x, y, d in r["slots"][:3])),
            **rec})
        n += 1
    Path(path).write_text(serialize_like(json.dumps(al, indent=1) + "\n", raw))
    return n


def serialize_like(new, raw):
    """The ledger is `json.dumps(indent=1)` with ASCII escapes EXCEPT a few
    lines carrying literal non-ASCII (one `★`, at this writing).  Re-emit every
    line that is unchanged up to escaping with its ORIGINAL text, so a
    withdrawal diff contains only the withdrawal."""
    orig = {}
    for line in raw.splitlines():
        orig.setdefault(line.encode("ascii", "backslashreplace").decode("ascii"), line)
    return "\n".join(orig.get(l, l) for l in new.split("\n"))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--aliases", default=str(ROOT / "scripts/symbol_aliases.json"))
    ap.add_argument("--out", required=True)
    ap.add_argument("--apply", action="store_true",
                    help="withdraw CONTRADICTED memberships (record kept, group kept)")
    ap.add_argument("--withdraw-undischarged", action="store_true",
                    help="with --apply: ALSO withdraw UNDISCHARGED memberships as "
                         "UNPROVEN (class PLACEHOLDER_SLOT_UNPROVEN_*), with mechanical "
                         "retail-byte evidence per slot (W16-JH)")
    ap.add_argument("--extra", help="with --apply: JSON {\"<address>|<folded>\": "
                    "{class, evidence[, disposition]}} -- further decided withdrawals "
                    "(any verdict), applied with exactly that record")
    ap.add_argument("--lane", default=LANE, help="lane label for written records")
    a = ap.parse_args()
    mapped = A.load_mapped()
    tgt, ours = A.load_sides()
    groups = json.loads(Path(a.aliases).read_text())["groups"]
    rows = audit(tgt, ours, mapped, groups)
    c = collections.Counter(r["verdict"] for r in rows)
    print("memberships:", len(rows), dict(c))
    kinds = collections.Counter()
    for r in rows:
        for s in r.get("slot_kinds", {}).items():
            kinds[s[0]] += s[1]
    print("depth-0 slot kinds:", dict(kinds))
    dc = data_consistency(rows)
    print("data names with >1 retail address:", len(dc))
    for on, d in list(dc.items())[:20]:
        print("   ", on[:70], d)
    json.dump(dict(summary=dict(c), slot_kinds=dict(kinds), data_inconsistent=dc,
                   rows=rows), open(a.out, "w"), indent=0)
    if a.apply:
        extra = json.loads(Path(a.extra).read_text()) if a.extra else {}
        print("withdrawn:", apply(a.aliases, rows, tgt, ours, a.withdraw_undischarged,
                                  extra, a.lane))
    return 0


if __name__ == "__main__":
    sys.exit(main())
