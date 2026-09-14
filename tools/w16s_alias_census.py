#!/usr/bin/env python3
"""W16-S: classify every LIVE alias membership as a FOLD or a mere IDENTIFICATION.

THE QUESTION, AND WHY THE INSTALLED EVIDENCE CANNOT ANSWER IT
=============================================================
`scripts/symbol_aliases.json` installs memberships on **T1** evidence, whose own
text reads:

    T1 = RB3 retail bytes at the survivor address are byte-identical (modulo
         relocated fields) to our compiled body for the folded spelling

That is a claim about ONE side against retail.  It proves

    retail-at-address-X  IS  our-COMDAT-for-N          (an IDENTIFICATION)

and it does NOT prove

    our-COMDAT-for-N  folded with  our-COMDAT-for-S    (a FOLD)

A fold under `/OPT:ICF` is a statement about TWO COMDATs of OURS being identical
to each other *including relocation target names* -- the linker's own condition.
The T1 instrument never looks at our COMDAT for S at all, so it is structurally
incapable of separating the two claims (lane W16-Q, `list<EventCall>` under
survivor `list<HamCamShot::Target>` @0x824c93c0: the two differ in one `bl`,
72 B vs 316 B element serializer -- unfoldable by construction).

THE DISCRIMINATOR IS ENTIRELY OUR-SIDE
======================================
    our N == our S  (raw bytes AND relocation (offset, target NAME, type))

No map resolution, no retail address, no masking -- therefore NOT subject to the
masked-T1 thunk vacuity.  This is `tools/ourside_fold_sweep.py`'s argument,
applied to the alias file's memberships instead of to name_check charges.

THE SECONDARY SPLIT DOES TOUCH RETAIL, AND CARRIES BOTH KNOWN TRAPS
===================================================================
When our N != our S, the membership is either an identification (our N IS the
body at X, so the *name* at X is wrong) or a contradiction (our N is not that
body either).  Deciding that compares our N against retail@X, where:

  * ⛔ MASKED comparison is VACUOUS FOR THUNKS (w33_fold_adjudicate's lesson):
    if the body is mostly a branch, the DESTINATION is the entire information
    content, so masking the relocated word masks the only discriminator.  This
    tool therefore ALSO decodes retail's branch displacements, resolves them
    through target_symbol_map.json, and compares them to our relocation target
    NAMES.  A membership decided on masked bytes alone is labelled
    `masked_only=True` and is never treated as proven.
  * ⛔ THE SIZE TRAP (lane STLPORT-1): dtk-split extents can absorb a
    successor's 8-byte EH prefix.  Retail sizes here come from
    `wrong_callee_triage.load_sizes()` (.pdata-authoritative function extents),
    not from a split-obj [sym, next sym) derivation, so the artifact is out of
    reach by construction.  Size disagreements are REPORTED, never used as a
    silent verdict.

VERDICTS (one per live membership N under survivor S at address X)
==================================================================
  FOLD_CONFIRMED            our N == our S incl. relocs.  The linker's own
                            condition holds.  Leave alone.
  IDENTIFICATION_NOT_A_FOLD our N != our S, our N == retail@X, AND our S !=
                            retail@X.  X IS N; the map name at X (the
                            survivor's) is wrong for X.  The NON-match against S
                            is mandatory -- see UNDECIDED_MASKED.
  UNDECIDED_MASKED          retail@X equals our N and our S BOTH, because the
                            comparison masks relocated words whose destination
                            the map does not name.  The address cannot
                            discriminate; claim nothing.
  CONTRADICTED_ON_RETAIL    our N != our S and our N != retail@X.
  NEEDS_SOURCE              our S absent from our build -> undecidable our-side.
  REFERENCED_UNDEFINED      no COMDAT defines N, but our objs reference it.
                            LIVE -- reloc_eq compares target NAMES.
  ABSENT_FROM_BUILD         N appears nowhere in our objs.  ⛔ STILL DO NOT PRUNE
                            -- these become live as porting advances; a prior
                            prune cost +94,616 B to reverse.
  DATA_*                    N is a data COMDAT (vtable / RTTI).  Same fold
                            question our-side; no .pdata extent to compare.

A NEGATIVE THAT AGREES WITH YOUR PRIOR IS THE HARDEST KIND TO CATCH
===================================================================
Every "absent" is reported as its own class, never folded into "not identical".
`--assert-renamed` refuses to run unless the target objs carry mangled names
(a reflinked, unbuilt worktree has NONE, and every retail lookup would then
return a confident, wrong "absent").
"""
import argparse
import collections
import glob
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "scripts"))
from comdat_bytes import comdats                        # noqa: E402
from wrong_callee_triage import Image, load_sizes       # noqa: E402

BUILD_ID = "45410914"


def our_comdat_index():
    """name -> set of (raw_bytes, reloc_tuple) over every compiled obj."""
    ours = collections.defaultdict(set)
    fns = collections.defaultdict(set)
    data = collections.defaultdict(set)
    where = collections.defaultdict(set)
    for p in glob.glob(str(ROOT / "build" / BUILD_ID / "src" / "**" / "*.obj"),
                       recursive=True):
        try:
            c = comdats(p)
        except Exception:
            continue
        for n, v in c.items():
            if not v.get("is_code"):
                # ⛔ DATA COMDATs are NOT out of scope -- 143 of the 289
                # spellings this census first called STALE_SPELLING are
                # `??_7X@@6B@` VTABLES, live data symbols our build defines.
                # /OPT:ICF folds identical data COMDATs too, and objdiff's
                # reloc_eq does not care whether a relocation target is code.
                # Labelling them "stale" would license exactly the prune the
                # house rule forbids (a prior one cost +94,616 B to reverse).
                drel = tuple(sorted((o, s, t) for o, s, t in (v["relocs"] or [])
                                    if s != "@comp.id"))
                data[n].add((v["raw"], drel))
                continue
            rel = tuple(sorted((o, s, t) for o, s, t in (v["relocs"] or [])
                               if s != "@comp.id"))
            ours[n].add((v["raw"], rel))
            # function-only extent -- the ONLY safe thing to compare to retail.
            # See comdat_bytes.py's W16-S note: `raw` runs to section end and so
            # bills the trailing __unwind$ funclet into the body, a ONE-SIDED
            # artifact worth a constant -44 B on 587 STLport memberships.
            frel = tuple(sorted((o, s, t) for o, s, t in (v["fn_relocs"] or [])
                                if s != "@comp.id"))
            fns[n].add((v["fn_raw"], frel))
            where[n].add(p)
    return ours, fns, data, where


def retail_compare(img, size, byva, va, raw, rel, closure=None):
    """our body (raw, rel) vs retail@va.  Returns (verdict, detail, masked_only).

    verdict in {'EQ','NE','NOSIZE','NOOFF','SIZE'}.
    masked_only is True when NO relocated word could be adjudicated by NAME,
    i.e. the EQ rests on masked bytes and is vacuous for a thunk.
    """
    n = size.get(va)
    if not n:
        return "NOSIZE", "no .pdata extent for %08x" % va, True
    o = img.off(va)
    if o is None:
        return "NOOFF", "address %08x not in image" % va, True
    if n != len(raw):
        return "SIZE", "retail %d B vs our %d B" % (n, len(raw)), True
    rw = list(struct.unpack_from(">%dI" % (n // 4), img.data, o))
    ow = list(struct.unpack(">%dI" % (n // 4), raw))
    relo = {off: (s, t) for off, s, t in rel}
    named_checked = 0
    for i, (x, y) in enumerate(zip(rw, ow)):
        off = i * 4
        if off in relo:
            if (x >> 26) != (y >> 26):
                return "NE", "opcode differs at +%x" % off, False
            if (x >> 26) in (16, 18) and (x & 3) != (y & 3):
                return "NE", "AA/LK differs at +%x" % off, False
            # ---- the anti-vacuity clause: adjudicate the DESTINATION by NAME.
            if (x >> 26) == 18 and not (x & 2):          # b/bl, AA=0
                d = x & 0x03FFFFFC
                if d & 0x02000000:
                    d -= 0x04000000
                dest = (va + off + d) & 0xFFFFFFFF
                nm = byva.get(dest)
                if nm:
                    named_checked += 1
                    # ⛔ compare the DESTINATIONS under the same /OPT:ICF fixed
                    # point used for the bodies.  Retail's callee and ours being
                    # spelled differently is not a divergence if the two callees
                    # are themselves co-folded -- applying the closure to bodies
                    # but not to their call targets would charge, at one remove,
                    # exactly the thing the closure exists to forgive.
                    ours_nm = relo[off][0]
                    same = (nm == ours_nm)
                    if not same and closure is not None:
                        a, b = closure.get(nm), closure.get(ours_nm)
                        same = (a is not None and a == b)
                    if not same:
                        return ("NE", "branch at +%x: retail -> %s, ours -> %s"
                                % (off, nm, relo[off][0]), False)
        elif x != y:
            return "NE", "non-relocated word differs at +%x" % off, False
    nrelbr = sum(1 for off, _s, _t in rel
                 if off // 4 < len(ow) and (ow[off // 4] >> 26) == 18)
    return "EQ", "identical (%d/%d branch relocs adjudicated by name)" % (
        named_checked, nrelbr), (nrelbr > 0 and named_checked == 0)


def icf_closure(fns):
    """The LINKER's fold condition, as a FIXED POINT -- not name identity.

    ⛔ "our N == our S including relocation target NAMES" is STRICTER THAN
    /OPT:ICF.  MSVC folds to a fixed point: two bodies that differ only in which
    symbols their relocations name still fold IF those targets are themselves in
    one fold class.  Measured on this binary: 115 of 118 memberships the
    name-identity test called CONTRADICTED, and 732 of 732 it called UNDECIDED,
    have IDENTICAL masked bytes and IDENTICAL relocation shape and differ ONLY in
    target names -- i.e. the strict test's non-folds are overwhelmingly the
    closure's candidates.  Shipping those as "contradictions" would close veins
    on an artifact of the instrument.

    Partition refinement, exactly as ICF computes it:
      seed  : (masked body bytes, relocation offsets+types)
      refine: append the CURRENT class id of every relocation target, in order
      stop  : when the class count stops changing.
    A target no COMDAT of ours defines is its own singleton (we compile, we never
    link), which is fail-closed: it can only keep classes apart, never merge them.
    """
    sig, cls = {}, {}
    for n, vs in fns.items():
        # ⛔ DETERMINISTIC tie-break.  `vs` is a SET of tuples containing bytes,
        # so a bare max() on length alone breaks ties by set iteration order,
        # which PYTHONHASHSEED randomises: two runs on an UNCHANGED tree gave
        # 43,053 and 43,051 fold classes.  A census that does not reproduce is
        # not evidence.  Include the payload in the key.
        raw, rel = max(vs, key=lambda kv: (len(kv[0]), kv[0], kv[1]))
        m = bytearray(raw)
        for o, _s, _t in rel:
            m[o:o + 4] = b"\0\0\0\0"
        sig[n] = (bytes(m), tuple((o, t) for o, _s, t in rel),
                  tuple(nm for _o, nm, _t in rel))
        cls[n] = (bytes(m), tuple((o, t) for o, _s, t in rel))
    ids = {k: i for i, k in enumerate(sorted(set(cls.values()), key=repr))}
    cur = {n: ids[cls[n]] for n in cls}
    prev = -1
    for _ in range(24):
        nclasses = len(set(cur.values()))
        if nclasses == prev:
            break
        prev = nclasses
        key = {}
        for n in cur:
            key[n] = (cur[n], tuple(cur.get(t, ("EXT", t))
                                    for t in sig[n][2]))
        ids = {k: i for i, k in enumerate(sorted(set(key.values()), key=repr))}
        cur = {n: ids[key[n]] for n in key}
    print("[icf closure] %d COMDATs -> %d fold classes after refinement"
          % (len(cur), len(set(cur.values()))))
    return cur


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--json", default=None)
    ap.add_argument("--only-evidence", default=None,
                    help="restrict to groups whose evidence contains this")
    ap.add_argument("--assert-renamed", type=int, default=50000,
                    help="refuse unless target objs carry >= N mangled names")
    args = ap.parse_args()

    # ---- anti-vacuity: a pre-renamer (reflinked, unbuilt) worktree carries the
    # target objs but NOT the effect of the renamer step, so every retail name
    # reads absent and every verdict agrees with whatever you expected.
    sys.path.insert(0, str(ROOT))
    from scripts.analysis.coffx import read_coff
    mang = 0
    for p in glob.glob(str(ROOT / "build" / BUILD_ID / "obj" / "**" / "*.obj"),
                       recursive=True):
        try:
            _secs, syms = read_coff(Path(p).read_bytes())
        except Exception:
            continue
        if syms:
            mang += sum(1 for s in syms if s.name.startswith("?"))
    if mang < args.assert_renamed:
        sys.exit("REFUSING: only %d mangled names in target objs (< %d). "
                 "Build the worktree first -- reflinked objs are PRE-RENAMER."
                 % (mang, args.assert_renamed))
    print("[renamer assertion] %d mangled names in target objs -- OK" % mang)

    img = Image(ROOT / "orig" / BUILD_ID / "band.exe")
    size = load_sizes()
    smap = json.loads((ROOT / "scripts" / "target_symbol_map.json").read_text())
    byva = {int(a, 16): n for a, n in smap.items()
            if a.startswith("0x") and isinstance(n, str)}
    byname = collections.defaultdict(set)
    for va, n in byva.items():
        byname[n].add(va)

    ours, fns, data, where = our_comdat_index()
    # ---- REFERENCE index.  objdiff's reloc_eq compares relocation TARGET
    # NAMES, and a relocation may name a symbol NO obj of ours defines (we
    # compile, we never link).  So "our build defines no COMDAT for N" is NOT
    # "N is inert": 143 of the 289 spellings this census first called
    # STALE_SPELLING are `??_7X@@6B@` vtables referenced but not defined, and
    # ABLATING that class cost -8 fns / -8,936 B -- a null that FAILED to be
    # null.  Liveness is REFERENCE, not definition.
    from scripts.analysis.coffx import read_coff as _rc
    refs = set()
    for _p in glob.glob(str(ROOT / "build" / BUILD_ID / "src" / "**" / "*.obj"),
                        recursive=True):
        try:
            _s, _sy = _rc(Path(_p).read_bytes())
        except Exception:
            continue
        if _sy:
            refs.update(x.name for x in _sy)
    print("[our build] %d names in obj symbol tables (defs + undefined externs)"
          % len(refs))
    print("[our build] %d distinct code COMDAT names" % len(ours))

    closure = icf_closure(fns)

    ali = json.loads((ROOT / "scripts" / "symbol_aliases.json").read_text())
    out = []
    for gi, g in enumerate(ali["groups"]):
        folded = g.get("folded") or []
        if not folded:
            continue
        ev = g.get("evidence") or ""
        if args.only_evidence and args.only_evidence not in ev:
            continue
        S = g["survivor"]
        addr = g.get("address")
        X = int(addr, 16) if addr else None
        sv = ours.get(S)
        svf = fns.get(S)
        for N in folded:
            fv = ours.get(N)
            fvf = fns.get(N)
            rec = {"gi": gi, "name": g.get("name"), "addr": addr,
                   "survivor": S, "folded": N,
                   "evidence_head": ev[:46]}
            if not fv:
                dv, dsv = data.get(N), data.get(S)
                if dv:
                    # data symbol (vtable / RTTI / jump table).  Same fold
                    # question, our-side, no retail .pdata extent to compare to.
                    rec["bytes"] = max(len(r) for r, _ in dv)
                    if dsv and (dv & dsv):
                        rec.update(verdict="DATA_FOLD_CONFIRMED",
                                   detail="our DATA COMDAT for N == our DATA "
                                          "COMDAT for S incl. reloc target names")
                    elif dsv:
                        rec.update(verdict="DATA_DIFFERS",
                                   detail="both are data COMDATs in our build "
                                          "and they are NOT identical")
                    else:
                        rec.update(verdict="DATA_NEEDS_SOURCE",
                                   detail="N is a data COMDAT; our build has no "
                                          "COMDAT for the survivor")
                    out.append(rec)
                    continue
                if N in refs:
                    rec.update(verdict="REFERENCED_UNDEFINED",
                               detail="no COMDAT defines N, but our objs REFERENCE "
                                      "it -- reloc_eq compares target NAMES, so "
                                      "this membership is LIVE", bytes=0)
                else:
                    rec.update(verdict="ABSENT_FROM_BUILD",
                               detail="N appears nowhere in our objs, defined or "
                                      "referenced. ⛔ STILL DO NOT PRUNE -- these "
                                      "become live as porting advances", bytes=0)
                out.append(rec)
                continue
            rec["bytes"] = max(len(r) for r, _ in (fvf or fv))
            if not sv:
                rec.update(verdict="NEEDS_SOURCE",
                           detail="our build defines no COMDAT for the survivor")
                out.append(rec)
                continue
            if fv & sv:
                rec.update(verdict="FOLD_CONFIRMED",
                           detail="our COMDAT for N == our COMDAT for S "
                                  "including relocation target names")
                out.append(rec)
                continue
            if (N in closure and S in closure
                    and closure[N] == closure[S]):
                rec.update(verdict="FOLD_CONFIRMED_CLOSURE",
                           detail="our N and our S land in ONE /OPT:ICF fold "
                                  "class under the linker's own fixed point "
                                  "(masked bytes + reloc shape equal; every "
                                  "differing target name is itself co-folded)")
                out.append(rec)
                continue
            if fvf and svf and (fvf & svf):
                # bodies identical but the whole COMDAT is not -- the trailing
                # EH funclet differs, which /OPT:ICF also compares.  Reported as
                # its own class rather than silently admitted or refused.
                rec.update(verdict="FOLD_BODY_ONLY",
                           detail="function bodies identical incl. reloc names; "
                                  "full COMDATs (with trailing EH funclet) differ")
                out.append(rec)
                continue
            # our N != our S.  Identification, or contradiction?
            if X is None:
                rec.update(verdict="NO_ADDRESS",
                           detail="group makes no address claim; our N != our S")
                out.append(rec)
                continue
            best = None
            for raw, rel in sorted(fvf or fv, key=lambda kv: (-len(kv[0]), kv[0], kv[1])):
                v, d, mo = retail_compare(img, size, byva, X, raw, rel, closure)
                if best is None or v == "EQ":
                    best = (v, d, mo)
                if v == "EQ":
                    break
            v, d, mo = best
            # how our S compares to retail@X -- REQUIRED, not decoration.
            sbest = None
            for raw, rel in sorted(svf or sv, key=lambda kv: (-len(kv[0]), kv[0], kv[1])):
                sv_v, sv_d, _ = retail_compare(img, size, byva, X, raw, rel, closure)
                if sbest is None or sv_v == "EQ":
                    sbest = (sv_v, sv_d)
                if sv_v == "EQ":
                    break
            rec["survivor_vs_retail"] = sbest[0]
            rec["survivor_vs_retail_detail"] = sbest[1]
            rec["masked_only"] = mo
            # ⛔ AN IDENTIFICATION REQUIRES THE ADDRESS TO DISCRIMINATE.
            # "retail@X == our N" alone is NOT an identification if retail@X
            # ALSO == our S: the comparison masks every relocated word whose
            # destination is unnamed in the map, so two bodies differing only in
            # relocation TARGET NAMES both read EQ.  That is the masked-T1
            # thunk vacuity (w33_fold_adjudicate) operating one level up, and it
            # inflated this class 208 -> 940 before the clause was added.
            # Requiring NON-match against S makes the address the discriminator
            # it has to be for the claim "X is N, not S" to mean anything.
            if v == "EQ" and sbest[0] == "EQ":
                rec.update(verdict="UNDECIDED_MASKED",
                           detail="retail@X equals our N AND our S under a "
                                  "comparison that masks unnamed relocation "
                                  "destinations -- the address does not "
                                  "discriminate (N: %s)" % d)
            elif v == "EQ":
                rec.update(verdict="IDENTIFICATION_NOT_A_FOLD", detail=d)
            elif v in ("NOSIZE", "NOOFF"):
                rec.update(verdict="RETAIL_UNREADABLE", detail=d)
            else:
                rec.update(verdict="CONTRADICTED_ON_RETAIL", detail=d)
            out.append(rec)

    c = collections.Counter(r["verdict"] for r in out)
    b = collections.Counter()
    for r in out:
        b[r["verdict"]] += r.get("bytes", 0)
    print("\n%-28s %7s %12s" % ("verdict", "memb", "our-body B"))
    for k, n in c.most_common():
        print("%-28s %7d %12d" % (k, n, b[k]))
    print("%-28s %7d %12d" % ("TOTAL", sum(c.values()), sum(b.values())))
    if args.json:
        Path(args.json).write_text(json.dumps(out, indent=1))
        print("\nwrote %s" % args.json)


if __name__ == "__main__":
    main()
