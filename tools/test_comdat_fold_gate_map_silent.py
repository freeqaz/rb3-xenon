#!/usr/bin/env python3
"""Unit + integration test for comdat_fold_gate.py's MAP-SILENT tier CF5 (lane W16-FM).

WHAT IT GUARDS.  Before this lane the gate coerced `base_addr` unconditionally
(`fa = int(r["base_addr"], 16)`), so a folded spelling that target_symbol_map.json
names NOWHERE had no representable input: the caller had to fabricate an address,
and the natural choice (base_addr := survivor) then tripped the documented
`same_function(A, A)` vacuum and produced a REFUSE that was an artifact of the
fabricated input rather than evidence.  CF5 gives that population an honest
spelling -- `base_addr: null` -- and adjudicates it.

WHY IT NEEDS A TEST AT ALL.  CF5 admits pairs the gate previously could not even
accept as input, so it can only ever LOOSEN the gate.  A tier that loosens must
be shown to still refuse, or it is indistinguishable from deleting the gate.
That is the house rule, and it is why TIER CF4 was removed: it admitted on a
statement about OUR confidence dressed up as evidence about retail.

THE REFUSALS THAT MUST SURVIVE, each exercised below by mutation:

  map_present   a spelling the map DOES place, declared `base_addr: null`.
                This is the laundering attack: if CF5 adjudicated it, any pair
                the CF1/CF2/CF3 chain refuses could be re-submitted with the
                address dropped and admitted instead.  MUST REFUSE.
  not_unique    the survivor body has a relocation-normalised RIVAL somewhere in
                the image, so our COMDAT is equally identical to two bodies and
                which one a `bl` denotes is undetermined.  MUST REFUSE.
  unreadable    no symbols.txt extent for the survivor.  MUST REFUSE.
  stage1        our COMDAT is NOT the retail body at the survivor address.  CF5
                never runs -- stage 1 is upstream of it and stays fail-closed.

  admit         the real, adjudicated pair ADMITs at CF5.  Without this control
                the four refusals above would also be satisfied by a tier that
                refuses everything, which proves nothing.

Exit 0 on pass, 1 on any failed expectation.  Registered in scripts/test_tools.py.
"""
import json
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "scripts"))

import comdat_fold_gate as G  # noqa: E402

FAILS = []


def check(label, cond, detail=""):
    print("  [%s] %s%s" % ("PASS" if cond else "FAIL", label,
                           "" if not detail else " -- " + detail))
    if not cond:
        FAILS.append(label)


class FakeRetail:
    """Only the surface map_silent() touches, so each precondition is isolated."""

    def __init__(self, byname=None, rivals=None, size=64):
        self.byname = byname or {}
        self._rivals = rivals
        self.size = {0x82520150: size, 0x82000000: size}
        self.starts = [0x82520150, 0x82000000]

    def rivals(self, va):
        return self._rivals


def run_unit():
    print("unit -- map_silent() preconditions (synthetic Retail):")
    F = "?_M_create_node@?$list@PAVContent@@...@Z"

    # (1) ADMIT control: map silent, survivor unique.
    row, refused = {}, []
    tier, disc = G.map_silent(FakeRetail(rivals=[]), F, 0x82520150, row,
                              lambda w: refused.append(w))
    check("map-silent + UNIQUE survivor ADMITs at CF5",
          tier == "CF5" and not refused, "tier=%r refused=%d" % (tier, len(refused)))
    check("CF5 discredit states silence is NOT the warrant",
          tier == "CF5" and "silence is NOT the warrant" in (disc or ""))
    check("CF5 records survivor_unique on the row", row.get("survivor_unique") is True)

    # (2) LAUNDERING MUTATION: the map DOES place this spelling.
    row, refused = {}, []
    tier, _ = G.map_silent(FakeRetail(byname={F: {0x82345678}}, rivals=[]),
                           F, 0x82520150, row, lambda w: refused.append(w))
    check("map-PRESENT spelling declared map-silent is REFUSED",
          tier is None and len(refused) == 1, "tier=%r" % (tier,))
    check("  ... and the refusal names the address the map gives it",
          bool(refused) and "0x82345678" in refused[0])

    # (3) UNIQUENESS MUTATION: a relocation-normalised rival exists.
    row, refused = {}, []
    tier, _ = G.map_silent(FakeRetail(rivals=[0x82000000]), F, 0x82520150, row,
                           lambda w: refused.append(w))
    check("non-UNIQUE survivor body is REFUSED",
          tier is None and len(refused) == 1, "tier=%r" % (tier,))
    check("  ... and the refusal names the rival address",
          bool(refused) and "0x82000000" in refused[0])

    # (4) unreadable survivor.
    row, refused = {}, []
    tier, _ = G.map_silent(FakeRetail(rivals=None), F, 0x82520150, row,
                           lambda w: refused.append(w))
    check("unreadable survivor body is REFUSED", tier is None and len(refused) == 1)


def gate(pairs):
    """Run the real gate over `pairs`; return {folded: row}."""
    wl = {"generated_by": "test", "build": "45410914",
          "totals": {"pairs": len(pairs)}, "pairs": pairs}
    with tempfile.TemporaryDirectory() as d:
        wlp, outp = os.path.join(d, "wl.json"), os.path.join(d, "out.json")
        with open(wlp, "w") as f:
            json.dump(wl, f)
        r = subprocess.run([sys.executable, os.path.join(HERE, "comdat_fold_gate.py"),
                            "--worklist", wlp, "--subclass", "residual", "-o", outp],
                           cwd=ROOT, capture_output=True, text=True)
        if r.returncode != 0:
            print(r.stdout[-2000:], r.stderr[-2000:])
            return None
        with open(outp) as f:
            return {x["folded"]: x for x in json.load(f)["pairs"]}


def pair(S, sa, F, base_addr=None):
    return dict(subclass="residual", target=S, base=F, sites=1, units=1,
                calling_functions=1, target_addr=sa, base_addr=base_addr,
                target_size=None, base_size=None, target_fanin=1, base_fanin=0,
                target_arbitrary=None, base_arbitrary=None, target_none_pct=0.0,
                base_none_pct=0.0, two_cycle=False, reloc_masked_identical=False)


def run_integration():
    print("integration -- the real gate on the real image + our real objs:")
    smap = json.load(open(os.path.join(ROOT, "scripts", "target_symbol_map.json")))
    S_CN, S_INS = smap["0x82520150"], smap["0x823d14c0"]
    F_CN = ("?_M_create_node@?$list@PAVContent@@V?$StlNodeAlloc@PAVContent@@"
            "@stlpmtx_std@@@stlpmtx_std@@IAAPAU_List_node_base@2@ABQAVContent@@@Z")
    F_INS = ("?insert@?$list@PAVContent@@V?$StlNodeAlloc@PAVContent@@@stlpmtx_std@@"
             "@stlpmtx_std@@QAA?AU?$_List_iterator@PAVContent@@U?$_Nonconst_traits@"
             "PAVContent@@@stlpmtx_std@@@2@U32@ABQAVContent@@@Z")

    # The laundering fixture is a REAL pair the CF1/CF2/CF3 chain refuses because
    # the map places the folded spelling on a different LIVE body (one of 60 such
    # in the shipped worklist), resubmitted with the address dropped.  Its stage 1
    # PASSES, so the pair really does reach stage 2 -- a fixture whose stage 1
    # fails would test nothing here, which is how the first draft of this test
    # passed for the wrong reason.
    S_LDR = "??3BinStream@@SAXPAX@Z"           # survivor, 0x8240ddb0
    F_LDR = "??3Loader@@SAXPAX@Z"              # map places it at 0x823f4698

    got = gate([pair(S_CN, "0x82520150", F_CN),
                pair(S_INS, "0x823d14c0", F_INS),
                pair(S_LDR, "0x8240ddb0", F_LDR)])
    if got is None:
        check("gate ran", False, "non-zero exit")
        return

    r = got.get(F_CN, {})
    check("REAL map-silent pair ADMITs at CF5 (not a refuse-all gate)",
          r.get("verdict") == "ADMIT" and r.get("tier") == "CF5",
          "verdict=%s tier=%s" % (r.get("verdict"), r.get("tier")))
    check("  ... on STAGE-1 body evidence, full 32-bit words",
          "compared as FULL 32-bit values" in (r.get("body_evidence") or ""))

    r = got.get(F_INS, {})
    check("stage-1 mismatch still REFUSES before CF5 is reached",
          r.get("verdict") == "REFUSE" and r.get("tier") is None
          and "not the retail body" in (r.get("reason") or ""),
          "verdict=%s" % (r.get("verdict"),))

    # a MAP-RESIDENT spelling submitted with the address dropped
    r = got.get(F_LDR, {})
    check("map-RESIDENT spelling submitted as map-silent is REFUSED (laundering)",
          r.get("verdict") == "REFUSE" and "DOES name" in (r.get("reason") or ""),
          "verdict=%s reason=%s" % (r.get("verdict"), (r.get("reason") or "")[:70]))
    check("  ... and the refusal names the live address the map gives it",
          "0x823f4698" in (r.get("reason") or ""))


def main():
    print(__doc__.splitlines()[0])
    run_unit()
    run_integration()
    print("RESULT: %s" % ("PASS" if not FAILS else "FAIL -- " + ", ".join(FAILS)))
    return 1 if FAILS else 0


if __name__ == "__main__":
    sys.exit(main())
