#!/usr/bin/env python3
"""incomplete_group_merge_adjudicate.py -- adjudicate GROUP-MERGE candidates, strictly.

A MEMBERSHIP says "this extra spelling landed on an address the group ALREADY
claims". A MERGE says "these TWO DISTINCT RETAIL ADDRESSES are one body". That is
a strictly stronger claim and the membership comparator CANNOT test it: flat T1
compares our body against retail at addr(S1) and never looks at addr(S2) at all
(lane T1-AUDIT: "the T1 warrant NEVER adjudicates addr(F)").

THE DECISIVE TEST is retail-vs-retail at the two addresses, and its logic is a
fork with no favourable branch for a merge:

  * bodies DIFFER  -> the two addresses hold different code. No fold. The charge
                      is a real wrong-callee or a map defect; an alias would
                      forgive a genuine defect. REFUSE.
  * bodies SAME    -> two LIVE addresses hold the same body, i.e. /OPT:ICF did
                      NOT fold them. That refutes the fold too. REFUSE --
                      unless addr(S2) is not a real function start at all, which
                      is the FOLD-THUNK tier's scenario (the map is a VA->name
                      FUNCTION over a folded link and parks a fold's loser on
                      whatever address is left). Only then is a merge arguable,
                      and it is then really a MAP-ROW REPAIR.

So this tool also asks whether each address is a genuine `.pdata` BeginAddress,
because that is what separates "two real functions" from "the map parked a name
on debris".
"""

import collections
import glob
import json
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from icf_alias_build import collect, relocs_agree, placeholder  # noqa: E402
from incomplete_group_census import charged_pairs, load_groups  # noqa: E402


def retail_relocs_identical(t1, t2):
    """Relocation-target comparison for RETAIL-vs-RETAIL. Deliberately NOT relocs_agree.

    ⚠ `icf_alias_build.relocs_agree` TOLERATES a retail-side `fn_`/`lbl_`
    placeholder, and that tolerance is correct for its own question (retail vs
    OURS: dtk spells a callee `fn_<B>` only when B is absent from the map, so the
    name carries no information our side could contradict). It is WRONG here.
    When BOTH sides are retail, two different placeholder names in one slot mean
    dtk resolved that slot to two DIFFERENT symbols -- which is precisely the
    thing /OPT:ICF refuses to fold.

    Measured on this lane's top merge candidate: the `ObjRefConcrete<FlowLabel>`
    and `ObjRefConcrete<EventTrigger>` destructors are masked-byte-identical
    (116 B) and even share their callee `fn_8275B378`, but differ at slots 24/36
    (`lbl_8201BA34` vs `lbl_8202158C` -- each type's own .rdata). Tolerating that
    reports them as an unfolded duplicate pair; comparing literally shows they are
    simply DIFFERENT FUNCTIONS, which is a stronger and more accurate refusal.

    (Name INEQUALITY is safe evidence even though CLAUDE.md warns `lbl_` names
    lie about their address: we use only "dtk resolved these to distinct
    symbols", never the address the name encodes.)
    """
    r1, r2 = t1[1], t2[1]
    if len(r1) != len(r2):
        return False
    return all(o1 == o2 and ty1 == ty2 and n1 == n2
               for (o1, n1, ty1), (o2, n2, ty2) in zip(r1, r2))


def pdata_starts():
    """Set of .pdata BeginAddresses. ⚠ .pdata is BIG-ENDIAN on this target."""
    pe = os.path.join(ROOT, "orig", "45410914", "band.exe")
    data = open(pe, "rb").read()
    off = struct.unpack_from("<I", data, 0x3C)[0]
    nsec = struct.unpack_from("<H", data, off + 6)[0]
    opt = struct.unpack_from("<H", data, off + 20)[0]
    base = struct.unpack_from("<I", data, off + 24 + 28)[0]
    so = off + 24 + opt
    starts = set()
    for i in range(nsec):
        b = data[so + i * 40: so + i * 40 + 40]
        name = b[0:8].rstrip(b"\0").decode(errors="replace")
        vsz, va, rsz, ptr = struct.unpack_from("<IIII", b, 8)
        if name == ".pdata":
            blob = data[ptr: ptr + min(vsz, rsz)]
            for j in range(0, len(blob) - 7, 8):
                starts.add(struct.unpack_from(">I", blob, j)[0])
    return starts


def main():
    tgt = collect(sorted(glob.glob(os.path.join(ROOT, "build/45410914/obj/**/*.obj"), recursive=True)), "t")
    ours = collect(sorted(glob.glob(os.path.join(ROOT, "build/45410914/src/**/*.obj"), recursive=True)), "o")
    owner, groups = load_groups(os.path.join(ROOT, "scripts/symbol_aliases.json"))
    surv = {g["survivor"]: i for i, g in enumerate(groups)}
    m = json.load(open(os.path.join(ROOT, "scripts/target_symbol_map.json")))
    name2addr = collections.defaultdict(set)
    for a, n in m.items():
        for x in (n if isinstance(n, list) else [n]):
            if x:
                name2addr[x].add(a)
    mapped = set(name2addr)
    starts = pdata_starts()
    print("  .pdata BeginAddresses: %d" % len(starts))

    sites, victims, _ = charged_pairs(tgt, ours)
    for p in [p for p in sites if placeholder(p[0]) or placeholder(p[1])]:
        del sites[p]
    for p in list(sites):
        gi, gj = owner.get(p[0]), owner.get(p[1])
        if gi is not None and gi == gj:
            del sites[p]
    cand = [p for p in sites if owner.get(p[0]) is not None and owner.get(p[1]) is not None]

    tally = collections.Counter()
    recs = []
    for rn, on in sorted(cand, key=lambda p: -sites[p]):
        g1, g2 = owner[rn], owner[on]
        a1 = sorted(name2addr.get(rn, ()))
        a2 = sorted(name2addr.get(on, ()))
        r = {"retail": rn, "ours": on, "sites": sites[(rn, on)],
             "g_retail": g1, "g_ours": g2,
             "surv_retail": rn in surv, "surv_ours": on in surv,
             "addr_retail": a1, "addr_ours": a2}

        if not a2:
            # our spelling is map-ABSENT: this is not a two-address merge at all,
            # it is a membership question against a DIFFERENT group than the one
            # that currently owns the spelling.
            r["verdict"] = "NOT_A_MERGE_spelling_map_absent"
            r["why"] = ("our spelling has no retail address, so no second address is being "
                        "asserted; it is owned by group %d but charged against group %d's "
                        "survivor. A cross-group membership, decided by whether the two "
                        "groups' survivors are one address." % (g2, g1))
            tally[r["verdict"]] += 1
            recs.append(r)
            continue

        same_addr = bool(set(a1) & set(a2))
        r["same_address"] = same_addr
        if same_addr:
            r["verdict"] = "ALREADY_ONE_ADDRESS"
            r["why"] = "both spellings already resolve to a shared address; not a merge"
            tally[r["verdict"]] += 1
            recs.append(r)
            continue

        r["pdata_retail"] = [a for a in a1 if int(a, 16) in starts]
        r["pdata_ours"] = [a for a in a2 if int(a, 16) in starts]

        t1, t2 = tgt.get(rn), tgt.get(on)
        if t1 is None or t2 is None:
            r["verdict"] = "UNDECIDABLE_second_body_unpinned"
            r["why"] = ("retail body for %s is not pinned, so the two addresses cannot be "
                        "compared retail-vs-retail; the merge is untested, not proven."
                        % (rn if t1 is None else on))
            tally[r["verdict"]] += 1
            recs.append(r)
            continue

        r["size_retail"], r["size_ours_addr"] = t1[2], t2[2]
        if t1[2] != t2[2] or t1[0] != t2[0]:
            r["verdict"] = "REFUTED_two_addresses_differ"
            r["why"] = ("retail's own bodies at %s and %s differ (%d vs %d B) -- the two "
                        "addresses are different code, so they did not fold. The charge is a "
                        "real defect or a map error; an alias would forgive it."
                        % (",".join(a1), ",".join(a2), t1[2], t2[2]))
        elif retail_relocs_identical(t1, t2):
            live2 = bool(r["pdata_ours"])
            r["verdict"] = ("REFUTED_both_live_unfolded" if live2
                            else "ARGUABLE_map_parked_loser")
            r["why"] = ("retail's bodies at the two addresses are identical modulo relocated "
                        "fields with targets agreeing. %s"
                        % ("Both addresses are genuine .pdata function starts, so /OPT:ICF did "
                           "NOT fold them -- two live copies refute the fold."
                           if live2 else
                           "addr(ours)=%s is NOT a .pdata BeginAddress, consistent with the map "
                           "parking a fold's loser on debris (FOLD-THUNK tier). This is a "
                           "MAP-ROW question, not a unilateral alias merge."
                           % ",".join(a2)))
        else:
            r["verdict"] = "REFUTED_two_addresses_differ"
            r["why"] = ("bodies masked-equal at the two addresses but relocation targets "
                        "disagree, so they are not one COMDAT.")
        tally[r["verdict"]] += 1
        recs.append(r)

    print("=" * 78)
    print("MERGE ADJUDICATION -- %d candidate pairs" % len(cand))
    print("=" * 78)
    for k, v in tally.most_common():
        print("  %-36s %4d pairs  %5d sites" % (k, v, sum(x["sites"] for x in recs if x["verdict"] == k)))
    json.dump(recs, open("/home/free/tmp/merge_verdicts.json", "w"), indent=1)

    for k in ("ARGUABLE_map_parked_loser", "REFUTED_both_live_unfolded"):
        ex = [r for r in recs if r["verdict"] == k]
        if not ex:
            continue
        print("\n  %s -- examples:" % k)
        for r in sorted(ex, key=lambda x: -x["sites"])[:6]:
            print("    %3d sites  %s @%s\n           <- %s @%s"
                  % (r["sites"], r["retail"][:56], ",".join(r["addr_retail"]),
                     r["ours"][:56], ",".join(r["addr_ours"])))
    return 0


if __name__ == "__main__":
    sys.exit(main())
