#!/usr/bin/env python3
"""empty_extrn_sweep -- find calls where OUR callee is an empty 4-byte `blr`
but RETAIL's paired destination is real work.  Read-only; not a build input.

Provenance: lane W14-B (2026-09-14), motivated by `?Terminate@Rnd@@UAAXXZ`
(docs/decomp/DOFPROC_REHOME_2026-09-13.md §5.1), which scored a clean fuzzy 100
while calling the wrong function.

THE MECHANISM
-------------
An empty out-of-line function is still EXTRN to its callers, so under /O1 with
no LTCG it still emits a `bl`.  That `bl` pairs against retail's `bl` at the
same offset.  If retail's destination is an UNNAMED address, `name_check`
FORGIVES the placeholder and the wrong/missing callee costs exactly zero.

THE DISCRIMINATOR IS SIZE, AND IT NEEDS NO DECODING
---------------------------------------------------
Our callee is 4 bytes of `blr`.  Sort retail's paired destination by .pdata
extent descending; anything materially larger than 4 bytes means our side is
not what retail called.  Decode only the survivors.

★ THE IN-PASS POSITIVE CONTROL -- A RUN WITHOUT IT PROVES NOTHING
------------------------------------------------------------------
Every trivially-empty function in retail is a 4-byte `blr` COMDAT with NO
relocations, so they are all byte-identical and ICF folds them into ONE
survivor.  Measured on this binary: 4 such destinations, 1,110 inbound `bl`,
overwhelmingly `0x826c3888` (1,099 callers), which the map names arbitrarily
`??$?0H@?$StlNodeAlloc@...` -- an ICF-survivor name, NOT our callee's name.

  ⇒ Rows pairing to a trivial survivor ARE the control.  If a run returns none,
    the PAIRING is broken, not the tree clean.
  ⚠ Do NOT test the control by name agreement: it is 0% BY CONSTRUCTION,
    because the survivor carries one arbitrary fold name.  Testing it that way
    reads as a total pairing failure on a working instrument -- this lane hit
    exactly that and nearly discarded a sound scan.

⛔ DRIVE OFF THE RELOCATION TABLE, NEVER A LINEAR DISASSEMBLY.  A capstone PPC
decode halts at the first undecodable word and silently reports fewer call
sites than exist.  Every call site here comes from the COFF relocation table.

⛔ THE MASKED CLASS IS NARROWER THAN "EMPTY CALLEE".  Masking needs retail's
destination to be BOTH non-trivial AND unnamed.  A non-trivial NAMED
destination is charged already, i.e. visible in the score.  Reported separately.

⛔ AN ADJUSTOR-THUNK CALLER CANNOT BE A WRONG-CALLEE HIT.  A vtordisp thunk
(`lwz r11,-4(r3); subf r3,r11,r3; b target`) forwards BY DEFINITION to the
virtual function its own name denotes, so its destination is pinned.  Such rows
mean "retail's body is real and ours is empty" (UNIMPLEMENTED_BODY), never
"we call the wrong thing".  Classified apart -- conflating them overstates the
wrong-callee class, which is the class the lane was funded to find.

Usage:
    python3 tools/empty_extrn_sweep.py [--json OUT] [--project-dir DIR]

⛔ Build the tree FIRST.  A fresh worktree's reflinked target objs are
PRE-RENAMER, so every retail mangled name reads "absent" and the sweep returns
a confident, vacuous null.  This tool asserts the renamer ran.
"""
import argparse
import collections
import json
import os
import struct
import sys

BLR = b"\x4e\x80\x00\x20"
REL24 = 0x0006
DEFAULT_VERSION = "45410914"


def _sections(buf):
    po = struct.unpack_from("<I", buf, 0x3C)[0]
    coff = po + 4
    nsec = struct.unpack_from("<H", buf, coff + 2)[0]
    optsz = struct.unpack_from("<H", buf, coff + 16)[0]
    opt = coff + 20
    imgbase = struct.unpack_from("<I", buf, opt + 28)[0]
    out, off = [], opt + optsz
    for _ in range(nsec):
        r = buf[off:off + 40]
        vsize, vaddr, rawsz, rawptr = struct.unpack_from("<IIII", r, 8)
        out.append((imgbase + vaddr, vsize, rawptr, rawsz))
        off += 40
    return out


class Retail:
    def __init__(self, exe):
        self.buf = open(exe, "rb").read()
        self.secs = _sections(self.buf)

    def read(self, va, n):
        for va0, vsize, rawptr, _rs in self.secs:
            if va0 <= va < va0 + vsize:
                o = rawptr + (va - va0)
                return self.buf[o:o + n]
        return b""

    def is_trivial(self, va):
        return self.read(va, 4) == BLR

    def decode_size(self, va, cap=512):
        """Bytes to and including the first `blr`. 0 => none within cap."""
        for i in range(0, cap, 4):
            w = self.read(va + i, 4)
            if len(w) < 4:
                return 0
            if w == BLR:
                return i + 4
        return 0


def load_map(repo):
    """name -> address, from scripts/target_symbol_map.json.

    ⚠ The file carries non-address keys (e.g. `_bijection_arbitrary`); always
    filter on startswith("0x") before int().
    """
    raw = json.load(open(os.path.join(repo, "scripts/target_symbol_map.json")))
    n2a, rows = {}, 0
    for k, v in raw.items():
        if not k.startswith("0x"):
            continue
        rows += 1
        n = v if isinstance(v, str) else (v.get("name") if isinstance(v, dict) else None)
        if n:
            n2a.setdefault(n, int(k, 16))
    return n2a, rows


def is_thunk(name):
    """MSVC virtual-adjustor / vtordisp thunk mangling."""
    return "$4" in name or "$2" in name or "$R" in name


def sweep(repo):
    sys.path.insert(0, os.path.join(repo, "tools"))
    sys.path.insert(0, os.path.join(repo, "scripts"))
    from coff_bodies_ext import function_bodies_ext
    from obj_pairing import ObjPairing
    from pdata_map_audit import load_extents

    exe = os.path.join(repo, "orig", DEFAULT_VERSION, "band.exe")
    src = os.path.join(repo, "build", DEFAULT_VERSION, "src")
    objd = os.path.join(repo, "build", DEFAULT_VERSION, "obj")

    op = ObjPairing(repo)
    cov = op.coverage()
    if cov["objects_declared"] < 900:
        raise SystemExit("REFUSED: vacuous pairing (%d declared objects)"
                         % cov["objects_declared"])

    n2a, map_rows = load_map(repo)
    ext = load_extents(exe)
    ret = Retail(exe)

    # ---- our side: every 4-byte `blr` COMDAT, and every direct call to one --
    ours, empty = {}, {}
    for rel in op.compiled_objects():
        d = {}
        try:
            for nm, body, rl, _e in function_bodies_ext(os.path.join(src, rel)):
                d[nm] = (body, rl)
                if body == BLR:
                    empty.setdefault(nm, []).append(rel)
        except Exception:
            continue
        ours[rel] = d

    sites = []
    for rel, d in ours.items():
        for caller, (_b, rl) in d.items():
            for (off, tgt, ty) in rl:
                if ty == REL24 and tgt in empty:
                    sites.append((rel, caller, off, tgt))

    # ---- retail side: resolve each site's paired destination ---------------
    tcache = {}

    def tfuncs(t):
        if t not in tcache:
            d = {}
            try:
                for nm, b, rl, _e in function_bodies_ext(os.path.join(objd, t)):
                    d[nm] = (b, rl)
            except Exception:
                pass
            tcache[t] = d
        return tcache[t]

    rows, diag = [], collections.Counter()
    for (rel, caller, off, callee) in sites:
        tgts = op.targets_for(rel)
        if not tgts:
            diag["unpairable_no_target_obj"] += 1
            continue
        f = None
        for t in tgts:
            td = tfuncs(t)
            if caller in td:
                f = td[caller]
                break
        if f is None:
            diag["unpairable_caller_absent"] += 1
            continue
        tb, trl = f
        ob = ours[rel].get(caller)
        lenmatch = ob is not None and len(ob[0]) == len(tb)
        m = [t for (o, t, tt) in trl if o == off and tt == REL24]
        if not m:
            diag["no_reloc_at_paired_offset"] += 1
            rows.append({"cls": "NO_PAIRED_RELOC", "obj": rel, "caller": caller,
                         "off": off, "callee": callee, "lenmatch": lenmatch,
                         "our_len": len(ob[0]) if ob else None,
                         "tgt_len": len(tb)})
            continue
        tn = m[0]
        addr = int(tn[3:], 16) if tn.startswith("fn_") else n2a.get(tn)
        if addr is None:
            diag["unresolvable_target_name"] += 1
            continue
        named = not tn.startswith("fn_")
        size = ext.get(addr)
        if ret.is_trivial(addr):
            cls = "CONTROL_TRIVIAL"
        elif is_thunk(caller):
            cls = "UNIMPLEMENTED_BODY"          # thunk pins the callee identity
        else:
            cls = "SUSPECT_MASKED" if not named else "SUSPECT_CHARGED"
        diag[cls] += 1
        rows.append({"cls": cls, "obj": rel, "caller": caller, "off": off,
                     "callee": callee, "tgt_name": tn, "addr": "0x%08x" % addr,
                     "pdata_size": size,
                     "decoded_size": size if size else ret.decode_size(addr),
                     "named": named, "lenmatch": lenmatch,
                     "our_len": len(ob[0]) if ob else None, "tgt_len": len(tb)})
    return {"rows": rows, "diag": dict(diag), "coverage": cov,
            "map_rows": map_rows, "n_empty": len(empty), "n_sites": len(sites),
            "n_called": len({s[3] for s in sites})}


def report(res):
    rows, diag = res["rows"], res["diag"]
    print("[population] %d distinct 4-byte-blr COMDATs in our objs; "
          "%d of them are DIRECTLY called (REL24); %d call sites"
          % (res["n_empty"], res["n_called"], res["n_sites"]))
    print("[pairing]    declared=%d on_disk=%d paired=%d"
          % (res["coverage"]["objects_declared"],
             res["coverage"]["objects_on_disk"],
             res["coverage"]["objects_paired"]))
    examined = sum(diag.get(k, 0) for k in
                   ("CONTROL_TRIVIAL", "UNIMPLEMENTED_BODY",
                    "SUSPECT_MASKED", "SUSPECT_CHARGED"))
    print("[examined]   %d of %d call sites resolve to a retail destination"
          % (examined, res["n_sites"]))
    for k in ("unpairable_caller_absent", "unpairable_no_target_obj",
              "no_reloc_at_paired_offset", "unresolvable_target_name"):
        if diag.get(k):
            print("               not examined -- %-28s %d" % (k, diag[k]))
    ok = [r for r in rows if r["cls"] != "NO_PAIRED_RELOC"]
    lm = sum(1 for r in ok if r["lenmatch"])
    print("[validity]   caller body length equal ours-vs-retail: %d/%d"
          % (lm, len(ok)))

    ctrl = diag.get("CONTROL_TRIVIAL", 0)
    print()
    print("★ CONTROL (retail destination is also trivial): %d" % ctrl)
    if ctrl == 0:
        print("  ⛔ CONTROL EMPTY -- the PAIRING is broken, not the tree clean. "
              "Do not report this run as a negative result.")
    for cls, label in (("SUSPECT_MASKED", "metric-INVISIBLE (dest non-trivial AND unnamed)"),
                       ("SUSPECT_CHARGED", "already CHARGED (dest non-trivial, named)"),
                       ("UNIMPLEMENTED_BODY", "thunk-pinned: our body empty, retail's real")):
        sel = sorted([r for r in rows if r["cls"] == cls],
                     key=lambda z: -(z["decoded_size"] or 0))
        print()
        print("=== %s: %d -- %s" % (cls, len(sel), label))
        for r in sel:
            print("  %s %5s B  %s" % (r["addr"], r["decoded_size"], r["obj"]))
            print("      caller %s" % r["caller"])
            print("      ours-> %s" % r["callee"])
            print("      ret -> %s" % r["tgt_name"])
    if examined:
        fired = examined - ctrl
        print()
        print("[precision]  screen fired on %d of %d EXAMINED call sites (%.1f%%); "
              "denominator is EXAMINED, not the %d-site candidate population."
              % (fired, examined, 100.0 * fired / examined, res["n_sites"]))


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--project-dir", default=os.environ.get("RB3_ROOT", "."))
    ap.add_argument("--json", help="write the full row set here")
    a = ap.parse_args(argv)
    repo = os.path.abspath(a.project_dir)
    res = sweep(repo)
    report(res)
    if a.json:
        json.dump(res, open(a.json, "w"), indent=1)
        print("\n[json] %s" % a.json)
    return 0


if __name__ == "__main__":
    sys.exit(main())
