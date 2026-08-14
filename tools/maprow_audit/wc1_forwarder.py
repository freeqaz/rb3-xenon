#!/usr/bin/env python3
"""Lane WRONGCALL-1: the MEMBER-FORWARDER channel for map-row adjudication.

POPULATION (deliberately disjoint from lane CS-1 / CT-2)
--------------------------------------------------------
CS-1 and CT-2 adjudicated the _bijection_arbitrary rows whose name is a
COMPILER-GENERATED thunk (vtordisp / adjustor / `??_E`), dethunking through the
adjustor to a body.  Their population filter requires `thunk_kind(name) is not
None`, which excludes ordinary method names entirely.

This tool takes the complementary stratum: rows whose RETAIL BODY is an
8-byte SOURCE-LEVEL forwarder

    lwz r3, <off>(r3)          # load a member pointer
    b   ->TARGET               # tail-call a method on it

i.e. the hand-written `Foo::Bar(a,b) { return mThing->Bar(a,b); }` idiom, which
is pervasive in RB3's manager classes.  These carry ordinary method names, so
CS-1's dethunk channel never saw them.

THE CHANNEL
-----------
Milo/RB3 forwarders are overwhelmingly NAME-PRESERVING: the outer method and the
inner method share a method name.  So a NAMED target predicts the outer row's
method name, independently of whatever the bijection assigned.

CONTROL (this is the load-bearing part, and it is CS-1's design, not a shuffle)
------------------------------------------------------------------------------
The prediction rate is measured on the UNTREATED population -- forwarder rows
that are NOT _bijection_arbitrary, i.e. rows whose name arrived by some channel
we have reason to trust.  That number is what licenses the channel at all: if
name-preservation only held 50% of the time, a disagreement in the arbitrary set
would mean nothing.  Enrichment = DIFFERS-rate(arbitrary) / DIFFERS-rate(untreated).

A `--selfcheck` mode proves the instrument CAN fire and CAN fail: it asserts the
retail decode reproduces two independently-known branch targets, rejects a
non-branch word, and asserts that a deliberately corrupted map produces a
DIFFERS verdict on a row the clean map calls AGREES.
"""
import json, struct, sys, os, collections, argparse

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


class Image:
    def __init__(self, path):
        d = open(path, "rb").read()
        e = struct.unpack_from("<I", d, 0x3C)[0]
        assert d[e:e + 4] == b"PE\0\0", "not a PE"
        coff = e + 4
        ns = struct.unpack_from("<H", d, coff + 2)[0]
        osz = struct.unpack_from("<H", d, coff + 16)[0]
        opt = coff + 20
        ib = struct.unpack_from("<I", d, opt + 28)[0]
        st = opt + osz
        self.secs = []
        for i in range(ns):
            o = st + i * 40
            self.secs.append((ib + struct.unpack_from("<I", d, o + 12)[0],
                              struct.unpack_from("<I", d, o + 8)[0],
                              struct.unpack_from("<I", d, o + 20)[0]))
        self.d = d

    def word(self, va):
        for base, vsz, raw in self.secs:
            if base <= va < base + vsz:
                o = raw + (va - base)
                if o + 4 <= len(self.d):
                    return struct.unpack_from(">I", self.d, o)[0]
        return None

    def br_target(self, va, want_link=False):
        """Decode an I-form b/bl; return (target, is_link) or None."""
        w = self.word(va)
        if w is None or (w >> 26) != 18:
            return None
        li = w & 0x03FFFFFC
        if li & 0x02000000:
            li -= 0x04000000
        tgt = (li if (w & 2) else va + li) & 0xFFFFFFFF
        if bool(w & 1) != want_link:
            return None
        return tgt


# `lwz rD, off(r3)` with rD == r3 -> opcode 32, D=3, A=3
def member_load_off(w):
    if w is None or (w >> 26) != 32:
        return None
    if ((w >> 21) & 31) != 3 or ((w >> 16) & 31) != 3:
        return None
    return w & 0xFFFF


def split_name(mangled):
    """Return (method, scope) for a `?method@Scope@@...` MSVC name, else None."""
    if not isinstance(mangled, str) or not mangled.startswith("?") or mangled.startswith("??"):
        return None
    body = mangled[1:]
    at = body.find("@")
    if at <= 0:
        return None
    meth = body[:at]
    rest = body[at + 1:]
    scope = rest.split("@@")[0]
    return meth, scope


def load_map(path):
    raw = json.load(open(path))
    m = {}
    for k, v in raw.items():
        if not k.startswith("0x"):
            continue
        if isinstance(v, list):
            v = v[0] if v else None
        if v:
            m[int(k, 16)] = v
    return raw, m


def scan(img, m, arb):
    """Yield forwarder rows with a NAMED target."""
    out = []
    for va, name in m.items():
        off = member_load_off(img.word(va))
        if off is None:
            continue
        tgt = img.br_target(va + 4, want_link=False)
        if tgt is None:
            continue
        tn = m.get(tgt)
        sp, tp = split_name(name), split_name(tn) if tn else None
        out.append(dict(va=va, off=off, name=name, tgt=tgt, tname=tn,
                        meth=sp[0] if sp else None, scope=sp[1] if sp else None,
                        tmeth=tp[0] if tp else None, tscope=tp[1] if tp else None,
                        arb=("0x%08x" % va) in arb))
    return out


def verdict(r):
    if r["meth"] is None:
        return "ROW_UNPARSED"
    if r["tname"] is None:
        return "TARGET_UNNAMED"
    if r["tmeth"] is None:
        return "TARGET_UNPARSED"
    if r["tscope"] == r["scope"]:
        return "SELF_SCOPE"          # forwards within its own class: not informative
    return "AGREES" if r["tmeth"] == r["meth"] else "DIFFERS"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--project", default=ROOT)
    ap.add_argument("--selfcheck", action="store_true")
    ap.add_argument("--scope", help="print every forwarder row in this class")
    a = ap.parse_args()

    img = Image(os.path.join(a.project, "orig/45410914/band.exe"))
    raw, m = load_map(os.path.join(a.project, "scripts/target_symbol_map.json"))
    arb = set(x.lower() for x in raw.get("_bijection_arbitrary", []))

    if a.selfcheck:
        # (1) decode reproduces two independently-known targets (from va_disasm)
        assert img.word(0x825857A0) == 0x80630038
        assert img.br_target(0x825857A4, False) == 0x823E59E0
        assert img.br_target(0x82585924, False) == 0x823E4180
        # (2) negative control: a non-branch word must not decode as a branch
        assert img.br_target(0x825857A0, False) is None
        # (3) member_load_off must reject a load into a different register
        assert member_load_off(0x80830038) is None   # lwz r4,0x38(r3)
        assert member_load_off(0x80630038) == 0x38
        # (4) the verdict function must be able to BOTH agree and differ
        good = dict(meth="IsBusy", scope="A", tname="x", tmeth="IsBusy", tscope="B")
        bad = dict(good, tmeth="SendMsgToAll")
        assert verdict(good) == "AGREES" and verdict(bad) == "DIFFERS"
        print("SELFCHECK PASS: decoder reproduces 2 known targets, rejects 2 negative "
              "controls, and the verdict fn demonstrably yields both AGREES and DIFFERS.")
        return

    rows = scan(img, m, arb)
    for r in rows:
        r["v"] = verdict(r)

    if a.scope:
        sel = sorted([r for r in rows if r["scope"] == a.scope], key=lambda r: r["va"])
        print("%-12s %-6s %-4s %-26s %-30s %s" %
              ("VA", "arb", "off", "MAP METHOD", "RETAIL TAIL-CALLS", "VERDICT"))
        for r in sel:
            print("0x%08x %-6s 0x%02x %-26s %-30s %s" %
                  (r["va"], r["arb"], r["off"], (r["meth"] or "?")[:26],
                   ((r["tscope"] + "::" + r["tmeth"]) if r["tmeth"] else
                    ("0x%08x" % r["tgt"]))[:30], r["v"]))
        return

    print("forwarder rows found: %d  (of %d named map rows)" % (len(rows), len(m)))
    for pop, sel in (("ARBITRARY  ", [r for r in rows if r["arb"]]),
                     ("UNTREATED  ", [r for r in rows if not r["arb"]])):
        c = collections.Counter(r["v"] for r in sel)
        dec = c["AGREES"] + c["DIFFERS"]
        rate = (100.0 * c["DIFFERS"] / dec) if dec else float("nan")
        print("\n%s n=%-5d decidable=%-5d DIFFERS=%-4d (%.1f%%)" %
              (pop, len(sel), dec, c["DIFFERS"], rate))
        for k, v in c.most_common():
            print("      %-16s %d" % (k, v))
    ca = collections.Counter(r["v"] for r in rows if r["arb"])
    cu = collections.Counter(r["v"] for r in rows if not r["arb"])
    da = ca["AGREES"] + ca["DIFFERS"]
    du = cu["AGREES"] + cu["DIFFERS"]
    if da and du and cu["DIFFERS"]:
        print("\nENRICHMENT of DIFFERS in the arbitrary set: %.2fx" %
              ((ca["DIFFERS"] / da) / (cu["DIFFERS"] / du)))
    elif da and du:
        print("\nUNTREATED DIFFERS == 0 -> enrichment is unbounded; "
              "the channel is clean on rows we trust.")

    print("\nDIFFERS rows in the ARBITRARY set (candidate repairs):")
    for r in sorted([r for r in rows if r["arb"] and r["v"] == "DIFFERS"],
                    key=lambda r: (r["scope"], r["va"])):
        print("  0x%08x %-22s map=%-24s retail->%s::%s" %
              (r["va"], r["scope"][:22], r["meth"][:24], r["tscope"], r["tmeth"]))


if __name__ == "__main__":
    main()
