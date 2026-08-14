#!/usr/bin/env python3
"""fold_literal_probe.py — RESOLVE the tolerated relocation slots instead of tolerating them.

Lane FOLDPROVE-1. NOGROUP-1 left 60 "fold-shaped but unproven" pairs, of which the
largest sub-class rests on a slot `relocs_agree` TOLERATED because BOTH spellings are
placeholders: retail spells an interned string literal `lbl_82XXXXXX` and we spell it
`??_C@...`, and `??_C@` is in icf_alias_build._PLACEHOLDER. Two `Type()` statics
interning DIFFERENT literals therefore compare EQUAL (NOGROUP-1's own refutation
example, doc `nogroup-pairs-censused-2026-08-14.md`).

Both sides are in fact RESOLVABLE, which is what makes this a STRONGER CHANNEL rather
than a relaxed gate -- it adds evidence, it does not lower a threshold:

  * ours   -- MSVC encodes the literal TEXT directly in the mangled name:
              ??_C@_0BA@KMFCJMLJ@AddUserResultMsg?$AA@  ->  "AddUserResultMsg"
  * retail -- `lbl_<VA>` carries a virtual address; tools/xex_string_at.py maps
              VA -> file offset in the extracted PE and reads the bytes.

Comparing the RESOLVED literals decides the slot. Equal literals leave the fold shape
intact; DIFFERENT literals REFUTE it outright -- each body interns its own string, so
they cannot be one COMDAT and an alias would forgive a genuinely wrong callee.

Usage:  python3 tools/fold_literal_probe.py --verdicts <nogroup_verdicts.json>
"""
import argparse
import collections
import glob
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))

import xex_string_at                                    # noqa: E402
from icf_alias_build import collect                     # noqa: E402

EXE = os.path.join(ROOT, "orig", "45410914", "band.exe")


def decode_msvc_literal(name):
    """??_C@_0BA@KMFCJMLJ@AddUserResultMsg?$AA@ -> 'AddUserResultMsg' (None if not one).

    ⚠ The length field is NOT uniformly @-terminated: MSVC spells lengths 0-9 as a bare
    decimal digit glued to the hash (`_04LMGLIOJM@Flow?$AA@` = len 4) and lengths >= 10
    as @-terminated letters (`_0BA@KMFCJMLJ@` = len 16). An index-based parse (parts[3])
    therefore silently returns '' for every SHORT literal -- which reads as "cannot
    resolve" and UNDER-REFUTES, the defect-manufacturing direction. The text is always
    the last @-delimited segment before the trailing '@' (a raw '@' cannot occur inside
    the text; MSVC escapes it), so index from the END.
    """
    if not name.startswith("??_C@"):
        return None
    parts = name.split("@")
    if len(parts) < 4 or parts[-1] != "":
        return None
    txt = parts[-2]
    txt = txt.replace("?$AA", "")          # NUL terminator
    return txt or None


class Retail:
    def __init__(self, path):
        self.data, self.base, self.sections = xex_string_at.load_sections(path)

    def string_at(self, va):
        off, sec = xex_string_at.va_to_offset(va, self.base, self.sections)
        if off is None:
            return None, None
        raw = xex_string_at.read_cstring(self.data, off, 128)
        try:
            return raw.decode("ascii"), sec
        except UnicodeDecodeError:
            return repr(raw), sec


def lbl_va(name):
    m = re.match(r"^(?:lbl|fn|jumptable|data|rdata|bss)_([0-9A-Fa-f]{8})$", name)
    return int(m.group(1), 16) if m else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--verdicts", default="/home/free/tmp/foldprove_verdicts.json")
    ap.add_argument("--out", default="/home/free/tmp/foldprove_literals.json")
    args = ap.parse_args()

    V = json.load(open(args.verdicts))
    T = [r for r in V
         if r["verdict"].startswith("FOLD_unproven") or r["verdict"] == "FOLD_blocked_residency"]
    # the honest target set: not also blocked on residency / uniqueness
    C = [r for r in T if not r["addr_ours"] and r.get("retail_addrs_for_body") == 1]
    print("target pairs (all)            : %d" % len(T))
    print("not residency/uniqueness-blocked: %d" % len(C))

    tgt = collect(sorted(glob.glob(os.path.join(ROOT, "build/45410914/obj/**/*.obj"),
                                   recursive=True)), "t")
    ours = collect(sorted(glob.glob(os.path.join(ROOT, "build/45410914/src/**/*.obj"),
                                    recursive=True)), "o")
    rt_img = Retail(EXE)

    out = []
    for r in C:
        S, F = r["retail"], r["ours"]
        a, b = tgt.get(S), ours.get(F)
        rec = {"retail": S, "ours": F, "size": r["size_retail"], "nrel": r["nrel"],
               "verdict_in": r["verdict"], "slots": [], "decision": None}
        if a is None or b is None:
            rec["decision"] = "NO_BODY"
            out.append(rec)
            continue

        differing = 0
        undecided = 0
        for (ro, rn, rty), (oo, on, oty) in zip(a[1], b[1]):
            if rn == on:
                continue
            slot = {"off": ro, "retail": rn, "ours": on}
            va = lbl_va(rn)
            our_lit = decode_msvc_literal(on)
            if va is not None:
                s, sec = rt_img.string_at(va)
                slot["retail_resolved"] = s
                slot["retail_section"] = sec
            if our_lit is not None:
                slot["ours_resolved"] = our_lit
            rs, os_ = slot.get("retail_resolved"), slot.get("ours_resolved")
            if rs is not None and os_ is not None:
                slot["agree"] = (rs == os_)
                if rs != os_:
                    differing += 1
            else:
                slot["agree"] = None
                undecided += 1
            rec["slots"].append(slot)

        if differing:
            rec["decision"] = "REFUTED_literal"
        elif undecided:
            rec["decision"] = "UNDECIDED_unresolvable"
        else:
            rec["decision"] = "LITERALS_AGREE"
        rec["n_differing"] = differing
        rec["n_undecided"] = undecided
        out.append(rec)

    json.dump(out, open(args.out, "w"), indent=1)

    print()
    print("%-24s %s" % ("decision", "pairs"))
    c = collections.Counter(x["decision"] for x in out)
    for k, v in c.most_common():
        print("%-24s %d" % (k, v))

    print()
    print("=" * 96)
    print("PER-PAIR")
    print("=" * 96)
    for x in out:
        print("[%s] %s B nrel=%s" % (x["decision"], x["size"], x["nrel"]))
        print("   S=%s" % x["retail"])
        print("   F=%s" % x["ours"])
        for s in x["slots"]:
            if s.get("agree") is False:
                print("   ⛔ off=0x%x retail %r != ours %r" %
                      (s["off"], s.get("retail_resolved"), s.get("ours_resolved")))
            elif s.get("agree") is None:
                print("   ?  off=0x%x retail=%s ours=%s (unresolvable)" %
                      (s["off"], s["retail"], s["ours"]))
        print()
    print("wrote %s" % args.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
