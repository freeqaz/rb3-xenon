#!/usr/bin/env python3
"""W33-FOLDPROOF: adjudicate a relocation-NAME charge into FOLD / MAP / CALLEE.

THE QUESTION
============
objdiff charges a site where RETAIL's object relocates a call to symbol A and
OUR object relocates it to symbol B.  Exactly three things can be true:

  FOLD    retail's code at addr(A) IS our B's code -- /OPT:ICF merged two
          identical COMDATs and the surviving map name is arbitrary.  Our
          source is right; an alias is legitimate.
  MAP     addr(A) is misidentified in target_symbol_map.json.
  CALLEE  our source genuinely calls the wrong function -- a real bug.

THE INSTRUMENT (retail bytes, NOT names)
========================================
Compare RETAIL's body for A -- taken from the dtk-split TARGET obj, which is
retail machine code with map names applied -- against OUR COMPILED body for B,
relocation-normalized WITH RELOCATION TARGET NAMES COMPARED.  Identical =>
whatever retail placed at addr(A) is the same code as our B, so the call is
correct and the name is arbitrary (FOLD).  Different => retail's callee is
genuinely different code from our B (MAP or CALLEE).

WHY NOT THE OBVIOUS TESTS -- each is silently vacuous (CLAUDE.md):
  * raw memcmp: PC-relative `bl` displacements differ at different addresses,
    so identical functions are NOT identical bytes.  It "proves" ICF by finding
    nothing.
  * masking the `bl` entirely: says "identical" where the absolute call targets
    differ, which is precisely the wrong-callee case.  So relocation TARGET
    NAMES are compared, never masked.

THE SIZE TRAP (lane STLPORT-1, and it cost six wrongly-withdrawn folds)
=======================================================================
"Different-size COMDATs cannot fold" is true, but a size test CANNOT catch a
one-sided reader artifact because it cancels on both legs.  The two sides here
are NOT natively the same measurement:

  * OUR obj is MSVC /Gy: one COMDAT section per function, so section data is
    the body -- plus possible trailing alignment padding.
  * The TARGET obj is a dtk split: ONE .text per unit with many symbols, so an
    extent must be derived as [sym, next sym).  That extent can absorb the
    NEXT function's 8-byte EH prefix (a .text pointer + an .rdata pointer),
    which is exactly what billed a successor's funclet into a COMDAT span and
    manufactured a phantom "+8 B" bug.

So both sides are normalized explicitly and the normalization is REPORTED, and
a size verdict is never issued blind.  Every refusal is loud: a symbol that
cannot be found is REFUSE, never "not identical" -- a negative that agrees with
your prior is the hardest kind to catch.
"""
import argparse
import collections
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "scripts" / "analysis"))
from scripts.analysis.coffx import read_coff  # noqa: E402
from cascade_price import is_placeholder  # noqa: E402


_ALIAS_GROUPS = None


def alias_covers(a, b):
    """Are `a` and `b` already members of one group in symbol_aliases.json?"""
    global _ALIAS_GROUPS
    if _ALIAS_GROUPS is None:
        try:
            al = json.load(open(ROOT / "scripts" / "symbol_aliases.json"))
            _ALIAS_GROUPS = [set([g["survivor"]] + (g.get("folded") or []))
                             for g in al.get("groups", [])]
        except Exception:
            _ALIAS_GROUPS = []
    return any(a in g and b in g for g in _ALIAS_GROUPS)


def index_objs(dirs):
    """symbol name -> [obj paths] over every COFF in dirs (recursive)."""
    idx = collections.defaultdict(list)
    for d in dirs:
        for p in sorted(Path(d).rglob("*.obj")):
            try:
                data = p.read_bytes()
                secs, syms = read_coff(data)
            except Exception:
                continue
            if secs is None:
                continue
            for s in syms:
                if s.sec > 0 and s.name:
                    idx[s.name].append(p)
    return idx


def strip_tail_padding(body, relocs):
    """Drop trailing alignment padding (zero words / nops) carrying no reloc."""
    n = len(body)
    relmax = max([r[0] for r in relocs], default=-1)
    while n >= 4:
        w = body[n - 4:n]
        if (n - 4) <= relmax:
            break
        if w in (b"\x00\x00\x00\x00", b"\x60\x00\x00\x00"):
            n -= 4
        else:
            break
    return body[:n], (len(body) - n)


def strip_eh_prefix_tail(body, relocs):
    """Drop a trailing 8-byte EH prefix belonging to the NEXT function.

    CLAUDE.md: an 8-byte prefix (a .text pointer + an .rdata pointer) precedes
    a function; ~17,200 split-log lines were this one cause.  When an extent is
    derived as [sym, next sym) that prefix lands at the END of the previous
    symbol's span.  Signature: the last 8 bytes carry exactly 2 relocations at
    offsets len-8 and len-4.
    """
    n = len(body)
    if n < 8:
        return body, 0, None
    offs = {r[0] for r in relocs}
    if (n - 8) in offs and (n - 4) in offs:
        tail = [r for r in relocs if r[0] >= n - 8]
        return body[:n - 8], 8, [t[2] for t in tail]
    return body, 0, None


def is_boundary_symbol(name):
    """Does `name` mark the start of a NEW body, bounding the previous one?

    ⛔ THE CONTROL CAUGHT BOTH HALVES OF THIS.  Getting it wrong in either
    direction silently makes the two legs different measurements:

      * TOO PERMISSIVE -- MSVC emits `$M<n>` line/scope LABELS inside a
        function.  Treating one as a boundary truncates the body mid-function.
      * TOO STRICT -- our MSVC COMDAT section holds the parent body AND its EH
        funclets (`__unwind$...`), while the dtk target extent stops at the next
        symbol.  Not bounding our side left the funclets in, and the size
        difference came out as a multiple of 40 -- the exact funclet size.  That
        is lane STLPORT-1's "+8 B" artifact in mirror image: a size test cannot
        catch a one-sided reader error, because it cancels on both legs.
    """
    if not name or name.startswith("$M"):
        return False
    if name.startswith(".") or name.startswith("@"):
        return False
    return True


def extract(objpath, name, derive_extent):
    """(body, relocs, note) for `name`.  relocs = [(off, type, target_name)].

    Both legs derive the extent the SAME way -- [sym, next boundary symbol) --
    so the comparison is one measurement, not two.  `derive_extent` only selects
    the extra normalization the dtk split side needs (EH prefix of the next fn).
    """
    data = Path(objpath).read_bytes()
    secs, syms = read_coff(data)
    if secs is None:
        return None, None, "not a COFF object"
    byidx = {s.index: s.name for s in syms}
    target = None
    for s in syms:
        if s.name == name and s.sec > 0:
            sec = secs[s.sec - 1]
            if sec.is_code:
                target = s
                break
    if target is None:
        return None, None, "symbol absent or not in a code section"
    sec = secs[target.sec - 1]
    start = target.value
    nxt = [s.value for s in syms
           if s.sec == target.sec and s.value > start
           and is_boundary_symbol(s.name)]
    end = min(nxt) if nxt else len(sec.data)
    body = sec.data[start:end]
    rel = sorted((va - start, typ, byidx.get(si, "?%d" % si))
                 for va, si, typ in sec.relocs if start <= va < end)
    notes = []
    if derive_extent:
        body2, cut, tailnames = strip_eh_prefix_tail(body, rel)
        if cut:
            body = body2
            rel = [r for r in rel if r[0] < len(body)]
            notes.append("stripped 8B EH prefix of next fn (-> %s)" % tailnames)
    body, pad = strip_tail_padding(body, rel)
    if pad:
        notes.append("stripped %dB trailing padding" % pad)
    return body, rel, "; ".join(notes)


def normalize(body, relocs):
    """Zero the relocated word at each relocation site.

    The relocated words are the ONLY bytes that legitimately differ between two
    copies of the same function at different addresses.  Their target NAMES are
    compared separately and are never masked -- masking them is what makes a
    wrong callee look identical.
    """
    b = bytearray(body)
    for off, _typ, _name in relocs:
        if 0 <= off <= len(b) - 4:
            b[off:off + 4] = b"\x00\x00\x00\x00"
    return bytes(b)


def adjudicate(tidx, oidx, retail, ours, min_bytes=16):
    r = dict(retail=retail, ours=ours)
    tobjs, oobjs = tidx.get(retail), oidx.get(ours)
    if not tobjs:
        r["verdict"] = "REFUSE"
        r["why"] = "retail symbol absent from every target obj"
        return r
    if not oobjs:
        r["verdict"] = "REFUSE"
        r["why"] = "our symbol absent from every compiled obj (we may not instantiate it)"
        return r
    tb, tr, tn = extract(tobjs[0], retail, True)
    ob, orl, on = extract(oobjs[0], ours, False)
    if tb is None or ob is None:
        r["verdict"] = "REFUSE"
        r["why"] = "extract failed: %s / %s" % (tn, on)
        return r
    r.update(retail_obj=str(tobjs[0].name), ours_obj=str(oobjs[0].name),
             retail_size=len(tb), ours_size=len(ob),
             retail_relocs=len(tr), ours_relocs=len(orl),
             retail_note=tn, ours_note=on)
    if len(tb) != len(ob):
        r["verdict"] = "DIFFERENT"
        r["why"] = "size differs (%d vs %d) after explicit normalization -- different-size COMDATs cannot fold" % (len(tb), len(ob))
        return r
    if normalize(tb, tr) != normalize(ob, orl):
        n = sum(1 for i in range(0, len(tb) - 3, 4)
                if normalize(tb, tr)[i:i + 4] != normalize(ob, orl)[i:i + 4])
        r["verdict"] = "DIFFERENT"
        r["why"] = "%d/%d non-relocated words differ" % (n, len(tb) // 4)
        return r
    # Relocation TARGET NAMES are compared, never masked -- masking them is what
    # makes a wrong callee look identical.  But a PLACEHOLDER retail name
    # (`lbl_`/`fn_`/`data_`/...) carries no identity: retail's anonymous data
    # label vs our `__real@3f8020c5` float COMDAT is the SAME datum.  objdiff
    # forgives placeholders (W19's rule); so does this, via objdiff's own
    # predicate rather than a reinvented one.  Skipping this read a function
    # objdiff scores at fuzzy==100 as SHAPE_ONLY -- caught by the CTRL+ leg.
    # ⛔⛔ PLACEHOLDER FORGIVENESS IS CORRECT FOR SCORING AND UNSOUND FOR FOLD
    # PROOF -- THIS TOOL RETURNED A FALSE `IDENTICAL` BEFORE THIS SPLIT.
    # objdiff forgives a placeholder target because it cannot be spelled wrong.
    # But a fold proof asks whether two BODIES are the same code, and the datum
    # that distinguishes two template instantiations is frequently ANONYMOUS:
    # `??1ObjRefConcrete<FlowLabel>` and `<EventTrigger>` differ ONLY in the
    # per-type VTABLE pointer, which retail spells `lbl_8201BA34`.  Forgiving it
    # made two provably different functions read IDENTICAL -- the masked-`bl`
    # trap, moved from a call target to a data pointer.
    # Caught by a retail-vs-retail control: retail's own two dtors differ at
    # exactly that relocation (`lbl_8201BA34` vs `lbl_8202158C`), so they did
    # NOT fold, and the linker was right not to fold them.
    # ⇒ a placeholder-vs-named relocation is UNDECIDED, never proof.
    tnames = [(o, n) for o, _t, n in tr]
    onames = [(o, n) for o, _t, n in orl]
    hard_diffs = [(a, b) for a, b in zip(tnames, onames)
                  if a != b and not (is_placeholder(a[1]) or is_placeholder(b[1]))]
    soft_diffs = [(a, b) for a, b in zip(tnames, onames)
                  if a != b and (is_placeholder(a[1]) or is_placeholder(b[1]))]
    diffs = hard_diffs
    if soft_diffs and not hard_diffs:
        r["verdict"] = "UNDECIDED"
        r["why"] = ("bodies equal, but %d relocation targets are PLACEHOLDER on "
                    "one side (identity unresolved -- NOT a fold proof)" % len(soft_diffs))
        r["name_diffs"] = [{"off": a[0], "retail": a[1], "ours": b[1]}
                           for a, b in soft_diffs[:12]]
        return r
    if tnames != onames and diffs:
        r["verdict"] = "SHAPE_ONLY"
        r["why"] = "bodies identical but %d relocation TARGET NAMES differ" % len(diffs)
        r["name_diffs"] = [{"off": a[0], "retail": a[1], "ours": b[1],
                            "already_aliased": alias_covers(a[1], b[1])}
                           for a, b in diffs[:12]]
        # ⚠ Existing alias coverage is REPORTED, never silently applied.  These
        # groups are the thing under audit; forgiving them here would let a
        # fabricated alias validate itself.
        return r
    if len(tb) < min_bytes and not tr:
        r["verdict"] = "TOO_WEAK"
        r["why"] = "body is %dB with zero relocations -- too little content to prove a fold" % len(tb)
        return r
    r["verdict"] = "IDENTICAL"
    r["why"] = "relocation-normalized bodies AND all %d relocation target names equal" % len(tr)
    return r


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--project", default=str(ROOT))
    ap.add_argument("--pairs", required=True, help="JSON list of {retail, ours}")
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--self-test", action="store_true",
                    help="prove the comparator can return DIFFERENT (anti-vacuity)")
    args = ap.parse_args()
    root = Path(args.project)
    tdir = root / "build/45410914/obj"
    odir = root / "build/45410914/src"
    sys.stderr.write("indexing target objs (%s) ...\n" % tdir)
    tidx = index_objs([tdir])
    sys.stderr.write("indexing our objs (%s) ...\n" % odir)
    oidx = index_objs([odir])
    sys.stderr.write("target symbols %d / our symbols %d\n" % (len(tidx), len(oidx)))
    if not tidx or not oidx:
        sys.stderr.write("REFUSE: an empty symbol index would make every lookup "
                         "read ABSENT -- build the worktree first.\n")
        return 2

    pairs = json.load(open(args.pairs))
    out = []
    agg = collections.Counter()
    for p in pairs:
        r = adjudicate(tidx, oidx, p["retail"], p["ours"])
        r["n"] = p.get("n")
        r["rows"] = p.get("rows")
        out.append(r)
        agg[r["verdict"]] += 1
        print("%-11s %s\n            R %s\n            O %s\n            %s"
              % (r["verdict"], "(%sx)" % r.get("n"), p["retail"][:88],
                 p["ours"][:88], r["why"]))
    print()
    for k, v in agg.most_common():
        print("  %-11s %d pairs" % (k, v))
    if args.json_out:
        json.dump(out, open(args.json_out, "w"), indent=1)
        print("wrote -> %s" % args.json_out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
