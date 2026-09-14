#!/usr/bin/env python3
"""W16-AG: adjudicate the refutation guard's INCONCLUSIVE_TWIN refusals.

W16-AD's `Witnesser.guard_pair` refuses a refutation when the two candidate
retail bodies compare identical under `masked_body`, which masks branch
displacements AND imm16.  That is WIDER than the hazard the guard exists for.

THE HAZARD (CD-7, `docs/decomp/` + CLAUDE.md): `/OPT:ICF` folds only COMDATs
identical *including relocations*.  CD-7 measured a residual population of 51
surplus copies that are byte-identical INCLUDING call targets and still were not
folded.  For such a pair, "two addresses" says nothing about whether retail kept
the parent callees apart -- so INCONCLUSIVE is the correct verdict.

=> TRUE REFUSAL CRITERION, stated so it can fail:
     the two retail bodies are identical INCLUDING CALL TARGETS
     (same length, and every word equal once PC-relative branch displacements
      are resolved to ABSOLUTE destinations).
   Met      -> genuine CD-7 twin; the guard is RIGHT; keep the membership.
   Not met  -> the bodies are genuinely different code, /OPT:ICF could not have
               folded them, so the pair DOES discriminate; the guard is a FALSE
               refusal and the refutation stands.

⛔ WHY NOT RAW `memcmp`, which `w16ad_strict_twin_audit.py` uses: CLAUDE.md
records raw duplicate-body comparison as SILENTLY VACUOUS -- two copies of an
identical function at different addresses have DIFFERENT `bl` displacement words
purely because the displacement is PC-relative.  A raw test therefore reports
"different" for a genuine twin, i.e. it flips every row to SEPARATE and would
manufacture the withdrawals it is supposed to gate.  Resolving the displacement
to its absolute destination is what makes the comparison mean "same call".

A difference in imm16 alone also counts as SEPARATE and that is correct: an
immediate is part of the COMDAT bytes, so differing immediates make the two
COMDATs non-identical and ICF cannot fold them.

⛔⛔ AND PLAIN ABSOLUTE RESOLUTION IS THE SAME TRAP IN MIRROR IMAGE -- caught
here by inspecting the first run's own evidence rather than by a control.  An
INTERNAL branch (a null check jumping forward 12 bytes) is the SAME code in both
bodies, but two copies at different addresses resolve it to two different
absolute addresses, so an absolute comparison reports "different call target"
for identical code.  On gi=942 the flagged word at +0x40 was pc+12 on BOTH
sides -- one instruction, reported as a divergence.  Had a row's ONLY difference
been such a branch, this tool would have manufactured a FALSE REFUTATION, which
is the costly error the guard exists to prevent.

=> the comparison token is chosen by where the branch LANDS:
     target inside the function's own .pdata extent -> compare OFFSET FROM BASE
                                                       (same internal control flow)
     target outside                                 -> compare ABSOLUTE ADDRESS
                                                       (same callee)
   Neither raw words nor absolute targets alone are a correct twin test.
"""
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "scripts"))
from wrong_callee_triage import Image                             # noqa: E402


class Retail:
    def __init__(self):
        self.img = Image(ROOT / "orig" / "45410914" / "band.exe")
        self.begins = {}
        for name, _sva, raw, rawsz in self.img.secs:
            if name == ".pdata":                  # X360 .pdata is BIG-ENDIAN
                for i in range(0, rawsz - 7, 8):
                    b, f = struct.unpack_from(">II", self.img.data, raw + i)
                    if b:
                        self.begins[b] = ((f >> 8) & 0x3FFFFF) * 4

    def words(self, va):
        n = self.begins.get(va)
        o = self.img.off(va)
        if not n or o is None:
            return None
        return [struct.unpack_from(">I", self.img.data, o + 4 * i)[0]
                for i in range(n // 4)]

    def resolved(self, va):
        """Body as comparable tokens.

        A branch that lands INSIDE this function is internal control flow and is
        compared by its offset from the function base (identical code at two
        addresses must compare equal).  A branch that leaves the function is a
        call and is compared by ABSOLUTE destination (a different callee is
        different code).  Everything else is compared raw.
        """
        w = self.words(va)
        if w is None:
            return None
        end = va + 4 * len(w)
        out = []
        for i, x in enumerate(w):
            op = x >> 26
            pc = va + 4 * i
            tgt = None
            if op == 18:                                   # b / bl / ba / bla
                d = x & 0x03FFFFFC
                if d & 0x02000000:
                    d -= 0x04000000
                tgt = d if (x & 2) else pc + d             # AA bit
                kind, extra = "B", (x & 3,)                # keep AA/LK
            elif op == 16:                                 # bc
                d = x & 0xFFFC
                if d & 0x8000:
                    d -= 0x10000
                tgt = d if (x & 2) else pc + d
                kind, extra = "BC", (x & 3, (x >> 16) & 0x3FF)
            if tgt is None:
                out.append(("W", x))
            elif va <= tgt < end:                          # internal: relative
                out.append((kind + "-internal", tgt - va) + extra)
            else:                                          # external: absolute
                out.append((kind + "-call", tgt) + extra)
        return out


def main():
    rows_path = sys.argv[1] if len(sys.argv) > 1 else str(
        ROOT / "docs/decomp/W16AD_fold_witness_2026-09-14.json")
    R = Retail()
    rows = json.loads(Path(rows_path).read_text())
    out = []
    for r in rows:
        if r.get("witness_verdict") != "WITNESS_INCONCLUSIVE":
            continue
        for p in r.get("pairs", []):
            g = p.get("guards") or {}
            if g.get("verdict") != "INCONCLUSIVE_TWIN":
                continue
            a = (p.get("N_strong") or p.get("N_medium") or [None])[0]
            b = (p.get("S_strong") or p.get("S_medium") or [None])[0]
            rec = {"gi": r["gi"], "addr": r["addr"], "bytes": int(r["bytes"]),
                   "survivor": r["survivor"], "folded": r["folded"],
                   "c_N": p["c_N"], "c_S": p["c_S"],
                   "c_N_retail": a, "c_S_retail": b,
                   "tier_N": p.get("tier"), "guards": g}
            if not a or not b:
                rec["strict"] = "UNREADABLE_NO_WITNESS_ADDR"
                out.append(rec); continue
            av, bv = int(a, 16), int(b, 16)
            ra, rb = R.resolved(av), R.resolved(bv)
            if ra is None or rb is None:
                rec["strict"] = "UNREADABLE_NOT_PDATA_BEGIN"
                out.append(rec); continue
            rec["len_N"], rec["len_S"] = len(ra) * 4, len(rb) * 4
            if av == bv:
                rec["strict"] = "SAME_ADDRESS"
                out.append(rec); continue
            if len(ra) != len(rb):
                rec["strict"] = "SEPARATE"
                rec["why"] = "different extents (%d vs %d B)" % (len(ra) * 4,
                                                                len(rb) * 4)
                out.append(rec); continue
            diffs = [(i, x, y) for i, (x, y) in enumerate(zip(ra, rb))
                     if x != y]
            rec["n_diff_words"] = len(diffs)
            if not diffs:
                rec["strict"] = "TWIN"          # CD-7 51-surplus class
                rec["why"] = ("identical INCLUDING call targets over %d words "
                              "-> genuine unfolded duplicate; guard is RIGHT"
                              % len(ra))
            else:
                rec["strict"] = "SEPARATE"
                d0 = diffs[0]
                rec["diff_detail"] = [
                    {"word_index": i, "byte_off": i * 4, "N": str(x),
                     "S": str(y),
                     "kind": ("external call target"
                              if "call" in x[0] or "call" in y[0] else
                              ("internal branch"
                               if "internal" in x[0] or "internal" in y[0]
                               else "non-branch word"))}
                    for i, x, y in diffs[:8]]
                rec["why"] = ("%d of %d words differ once branch displacements "
                              "are resolved to absolute destinations (first at "
                              "+0x%x) -> the two COMDATs are NOT identical "
                              "including call targets, so /OPT:ICF could not "
                              "have folded them: NOT the CD-7 51-surplus class"
                              % (len(diffs), len(ra), d0[0] * 4))
            out.append(rec)

    Path(sys.argv[2] if len(sys.argv) > 2 else "/dev/null").write_text(
        json.dumps(out, indent=1) + "\n")
    n_sep = sum(1 for r in out if r["strict"] == "SEPARATE")
    n_twin = sum(1 for r in out if r["strict"] == "TWIN")
    b_sep = sum(r["bytes"] for r in out if r["strict"] == "SEPARATE")
    print("GUARD ADJUDICATION -- strict test = identical INCLUDING call targets")
    print("=" * 78)
    print("pair-instances blocked by INCONCLUSIVE_TWIN : %d" % len(out))
    print("  TRUE refusal  (genuine CD-7 twin, KEEP)   : %d" % n_twin)
    print("  FALSE refusal (separates, REFUTATION OK)  : %d  / %d B"
          % (n_sep, b_sep))
    for r in out:
        print("\n gi=%-5s %s  %d B   -> %s" % (r["gi"], r["addr"], r["bytes"],
                                               r["strict"]))
        print("   c_N %s @%s" % (r["c_N"][:64], r["c_N_retail"]))
        print("   c_S %s @%s" % (r["c_S"][:64], r["c_S_retail"]))
        print("   %s" % r.get("why", ""))
        for d in r.get("diff_detail", [])[:4]:
            print("     +0x%03x %-14s N=%s  S=%s"
                  % (d["byte_off"], d["kind"], d["N"], d["S"]))


if __name__ == "__main__":
    main()
