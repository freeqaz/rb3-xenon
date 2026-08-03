#!/usr/bin/env python3
"""
gate_liveness.py -- a NON-METRIC liveness probe for a per-TU /D compile gate.

WHY THIS EXISTS
---------------
Lane DJ-3 established that retail's per-TU map-layout ODR split is real: of 11
TUs carrying /DRB3_MAP_0x1C, removing the gate cost -72 matched across six of
them, while four moved the metric by EXACTLY ZERO. Those four looked like dead
flags -- but "the metric did not move" is not "the flag does nothing". The
metric is a lossy function of codegen: a function that is 0% with and without
the flag contributes 0 either way, and a shared template COMDAT may not be
paired at all. DJ-3 therefore declined to remove them, correctly, because it
had no instrument that could tell dead from merely-unmeasured.

This is that instrument. It never consults the match metric.

METHOD
------
Compile the TU twice from the repo root -- once with the flag, once without --
and compare the .text section bodies plus the .text relocation table.

Every known obj-byte-comparison hazard in this repo is neutralised explicitly:
  * cache-served objs embed the POPULATING worktree's /Fo path (4 chars of path
    difference => ~96k differing bytes) -> OBJCACHE=off on BOTH legs.
  * the /Fo path string differs between roots -> both legs write the SAME /Fo
    path, so that string is byte-identical and cannot fabricate a difference.
  * the 4-byte COFF timestamp always differs -> we never read the COFF header.
  * the PCH consistency signature (~0x010980) differs on PCH rebuilds -> it is
    outside .text and the PCH is not rebuilt between legs.
The "obvious determinism control" that is VACUOUS elsewhere (rebuild with the
cache on -- it re-serves identical bytes) is replaced here by a real null: run
with --flag set to a name nothing tests. That must report INERT. It does.

THE DISCRIMINATOR
-----------------
Raw .text identity is too blunt. Toggling RB3_MAP_0x1C in any TU instantiating
stlpmtx_std::vector<stlpmtx_std::map<...>> perturbs that shared template's
COMDATs -- a fixed 119-word "template floor" for the <int,float> instantiation,
byte-identical across unrelated TUs. That floor is emitted by many TUs and the
linker picks one winner arbitrarily, so it cannot move THIS unit's pairing.

The signal is changed words in TU-OWNED symbols: changed .text COMDATs whose
symbol is not an STL template instantiation.

    owned == 0  ->  INERT. The metric CANNOT move. Sound, decisive, non-metric.
    owned  > 0  ->  LIVE.  The metric CAN move; DIRECTION IS NOT PREDICTED and
                    still requires an A/B. (TourPerformerLocal is LIVE and the
                    gate was HARMFUL there, +1 when removed.)

VALIDATION (11/11, against a fully independent metric census)
-------------------------------------------------------------
  positive control  TourProgress            LIVE  owned=101   (metric -20)
  null control      --flag RB3_NULL_...     INERT owned=0     (comparator can fire negative)
  6 DJ-3 "NEEDED"   all LIVE   owned 17..207
  4 DJ-3 "INERT"    all INERT  owned=0, tmpl=119 (template floor only)
  1 DJ-3 "WRONG"    TourPerformerLocal      LIVE  owned=6

USAGE
-----
  python3 tools/gate_liveness.py band3/tour/TourProgress.cpp
  python3 tools/gate_liveness.py --flag RB3_HANDLE_LOCAL_STATIC system/meta/Foo.cpp
  python3 tools/gate_liveness.py --repo ~/tmp/wt-x --all-syms <unit>
Unit keys are objects.json-style paths relative to src/ (e.g. band3/tour/Tour.cpp).
"""
import argparse, os, re, shutil, struct, subprocess, sys, tempfile

WIBO = "/home/free/code/milohax/wibo/build/release/wibo"
HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

STL_RE = re.compile(r"@stlpmtx_std@@|@std@@|^\?\?\$|\?\$vector@|\?\$map@|\?\$_Rb_tree|"
                    r"\?\$multimap@|\?\$set@|\?\$pair@|\?\$less@|StlNodeAlloc")


# ------------------------------------------------------------------ COFF parse
def parse_obj(path):
    """-> list of sections: dict(name, body, comdat, sym) in section order."""
    data = open(path, "rb").read()
    _mach, nsec, _ts, symptr, nsym, optsz, _ch = struct.unpack_from("<HHIIIHH", data, 0)
    off = 20 + optsz
    strtab = symptr + nsym * 18

    def sname(raw):
        if raw[:1] == b"/":
            s = int(raw[1:].rstrip(b"\0").decode())
            return data[strtab + s:data.index(b"\0", strtab + s)].decode()
        return raw.rstrip(b"\0").decode()

    secs = []
    for i in range(nsec):
        h = data[off + i * 40: off + (i + 1) * 40]
        _vs, _va, size, ptr, relptr, _lp, nrel, _nl, flags = struct.unpack_from("<IIIIIIHHI", h, 8)
        rels = []
        for r in range(nrel):
            ro = relptr + r * 10
            if ro + 10 <= len(data):
                rels.append(struct.unpack_from("<IIH", data, ro))
        secs.append(dict(name=sname(h[:8]), body=data[ptr:ptr + size] if ptr and size else b"",
                         comdat=bool(flags & 0x1000), rels=rels, sym=None))
    i = 0
    while i < nsym:
        e = data[symptr + i * 18: symptr + (i + 1) * 18]
        if e[:4] == b"\0\0\0\0":
            so = struct.unpack_from("<I", e, 4)[0]
            nm = data[strtab + so:data.index(b"\0", strtab + so)].decode(errors="replace")
        else:
            nm = e[:8].rstrip(b"\0").decode(errors="replace")
        secnum, typ, sclass, naux = struct.unpack_from("<hHBB", e, 12)
        if 0 < secnum <= len(secs) and typ == 0x20 and sclass in (2, 3, 6):
            if secs[secnum - 1]["sym"] is None:
                secs[secnum - 1]["sym"] = nm
        i += 1 + naux
    return secs


# ------------------------------------------------------------- build.ninja read
def _edge(repo, unit_key):
    t = open(os.path.join(repo, "build.ninja")).read()
    stem = unit_key[:-4] if unit_key.endswith(".cpp") else unit_key
    m = re.search(r"^build (build/\d+/src/" + re.escape(stem) + r"\.obj):", t, re.M)
    if not m:
        return None
    blk = re.search(r"^build " + re.escape(m.group(1)) + r":.*?(?=\nbuild |\Z)", t, re.M | re.S).group(0)
    blk = re.sub(r"\$\s*\n\s+", " ", blk)

    def var(n):
        mm = re.search(r"^\s+" + n + r" = (.*)$", blk, re.M)
        return mm.group(1).strip() if mm else None
    # NB: ninja wraps long output paths right after the colon ("...obj: $\n    msvc"),
    # so after unwrapping there may be MORE THAN ONE space before the rule name.
    head = re.match(r"^build \S+:\s+(\w+)\s+(\S+)", blk)
    return dict(rule=head.group(1), src=head.group(2),
                cflags=" ".join(var("cflags").split()), mw=var("mw_version"), pch=var("pch_file"))


def _compile(repo, edge, cflags, fo):
    cmd = [WIBO, f"build/compilers/{edge['mw']}/cl.exe"]
    if edge["rule"] == "msvc_pch":
        cmd += ["/Yudecomp_pch.h", "/FIdecomp_pch.h", f"/Fp{edge['pch']}"]
    cmd += cflags.split() + [f"/Fo{fo}", edge["src"]]
    return subprocess.run(cmd, cwd=repo, env=dict(os.environ, OBJCACHE="off", WIBO_FS_CACHE="1"),
                          capture_output=True, text=True)


# ------------------------------------------------------------------- the probe
def gate_liveness(repo, unit_key, flag="RB3_MAP_0x1C", all_syms=False, scratch=None):
    edge = _edge(repo, unit_key)
    if not edge:
        return dict(unit=unit_key, error="no build edge in build.ninja")
    dflag = "/D" + flag
    base = [f for f in edge["cflags"].split() if f != dflag]
    tmp = tempfile.mkdtemp(prefix="gate_", dir=scratch or tempfile.gettempdir())
    fo = os.path.join(tmp, "probe.obj")   # SAME /Fo both legs -- see module docstring
    try:
        for leg, flags in (("on", base + [dflag]), ("off", base)):
            r = _compile(repo, edge, " ".join(flags), fo)
            if r.returncode != 0 or not os.path.exists(fo):
                return dict(unit=unit_key, error=f"compile {leg} rc={r.returncode}",
                            detail=(r.stdout or r.stderr)[-400:])
            shutil.move(fo, os.path.join(tmp, f"{leg}.obj"))
        A, B = parse_obj(os.path.join(tmp, "off.obj")), parse_obj(os.path.join(tmp, "on.obj"))
        owned = tmpl = 0
        syms = []
        for a, b in zip(A, B):
            if not a["name"].startswith(".text") or (a["body"] == b["body"] and a["rels"] == b["rels"]):
                continue
            n = sum(1 for k in range(0, min(len(a["body"]), len(b["body"])) - 3, 4)
                    if a["body"][k:k + 4] != b["body"][k:k + 4])
            if a["sym"] and STL_RE.search(a["sym"]):
                tmpl += n
            else:
                owned += n
                syms.append((n, a["sym"] or "<anon>"))
        syms.sort(reverse=True)
        return dict(unit=unit_key, gated=(dflag in edge["cflags"].split()), owned=owned,
                    template=tmpl, live=(owned > 0), syms=syms if all_syms else syms[:6])
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[2],
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("units", nargs="+", help="objects.json-style unit keys, e.g. band3/tour/Tour.cpp")
    ap.add_argument("--repo", default=HERE, help="repo/worktree root to compile in")
    ap.add_argument("--flag", default="RB3_MAP_0x1C")
    ap.add_argument("--all-syms", action="store_true")
    ap.add_argument("--scratch", default=os.path.expanduser("~/tmp"))
    ap.add_argument("--quiet-inert", action="store_true")
    a = ap.parse_args()
    os.makedirs(a.scratch, exist_ok=True)
    rc = 0
    for u in a.units:
        r = gate_liveness(a.repo, u, a.flag, a.all_syms, a.scratch)
        if "error" in r:
            print(f"{u:52s} ERROR {r['error']} {r.get('detail','')}")
            rc = 1
            continue
        if a.quiet_inert and not r["live"]:
            continue
        print(f"{u:52s} {'LIVE' if r['live'] else 'INERT':5s} gated={r['gated']!s:5s} "
              f"owned={r['owned']:4d} template={r['template']:4d}")
        for n, s in r["syms"]:
            print(f"      {n:4d} words  {s}")
    return rc


if __name__ == "__main__":
    sys.exit(main())
