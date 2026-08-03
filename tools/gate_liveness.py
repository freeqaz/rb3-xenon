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

★★ COMPARATOR VERSIONS -- READ BEFORE QUOTING ANY `owned` NUMBER (lane DO-4)
----------------------------------------------------------------------------
The section comparator was replaced on 2026-08-03. `owned`/`template` totals
printed before that date are NOT comparable with totals printed after it.

  v2 "symbol"     -- CORRECT, default. Pairs .text COMDATs by symbol.
  v1 "positional" -- LEGACY, DEFECTIVE. Pairs by index (zip). Kept ONLY to
                     reproduce a pre-2026-08-03 figure: --comparator positional.

Every run prints a banner naming the comparator; every result dict carries a
`comparator` key. If a number has no comparator attached, it is v1.

Migration, measured over 250 gated/consumer units (both comparators scored from
the SAME compile pair, so the delta is attributable purely to pairing):

  LABEL FLIPS ................ 0 / 250   (LIVE 223, DEAD 21, INERT 5, unchanged)
  `owned` total changed ...... 202 / 250 ; median |v1-v2|/v2 = 32.7%, max 1062%
  v1 mis-paired >=1 section .. 200 / 240 probed pairs (max 3,781 mis-pairs)
  legs with != section counts  194 / 240

  => the LABELS were robust to the defect; the MAGNITUDES were not. That is why
     three lanes' controls passed over it, and why DL-4's standing advice ("do
     not use `owned` to prioritise anything but LIVE-vs-DEAD") was right.

Restated controls (v1 -> v2), all labels held:
  positive TourProgress/MAP_0x1C ....... owned 101 -> 108, tmpl 120 (per-symbol
        breakdown IDENTICAL: Handle 33w, ctor 21w, ResetTourData 6w). The +7 is
        one ADDED and one REMOVED vbase thunk (??_E...$4PPPPPPPM@BAA@ ->
        ...@PI@) -- the changed sizeof(map) changes the vbase displacement.
  null --flag RB3_NULL_CONTROL_XYZZY ... DEAD/owned=0 on 5 real target TUs
  4 MAP_0x1C INERT ..................... owned=0, template=119  (4/4, unchanged)
  DK-3's 6 DEAD SYNCPROP ............... byte-identical 6/6     (unchanged)
  DJ-3's 6 NEEDED ...................... range owned 17..207 -> 17..231
  TourPerformerLocal (DJ-3 "WRONG") .... owned 6 -> 6           (unchanged)

★ Structural corroboration that v2 is the correct one, independent of any
  control: DL-4 argued on MECHANISM that the HANDLE gate touches no STL
  instantiation, so the shared-template floor cannot exist for it. v1 reported
  template>0 on 132 of 153 HANDLE units, contradicting that; v2 reports
  template>0 on 0 of 153, while preserving the 119-word floor on 24/27
  MAP_0x1C units. v2 reproduces the mechanism's prediction in BOTH directions.

USAGE
-----
  python3 tools/gate_liveness.py band3/tour/TourProgress.cpp
  python3 tools/gate_liveness.py --flag RB3_HANDLE_LOCAL_STATIC system/meta/Foo.cpp
  python3 tools/gate_liveness.py --comparator positional <unit>   # reproduce a pre-DO-4 figure
  python3 tools/gate_liveness.py --selftest                       # no toolchain needed
Unit keys are objects.json-style paths relative to src/ (e.g. band3/tour/Tour.cpp).

⚠ ENVIRONMENT PRECONDITION, not a tool bug: in a fresh worktree
`scripts/setup_worktree.sh` truncates `build/45410914/pch/system.pch` to 0 bytes,
and an out-of-band compile never triggers ninja's rebuild edge, so any of the 380
PCH-eligible TUs dies with C1094. Run `ninja build/45410914/pch/system.pch` first.
(`system/meta/MoviePanel.cpp` carries /Y- and is the ONE immune TU -- therefore a
VACUOUS control for anything PCH-related.)
"""
import argparse, os, re, shutil, struct, subprocess, sys, tempfile

WIBO = "/home/free/code/milohax/wibo/build/release/wibo"
HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

STL_RE = re.compile(r"@stlpmtx_std@@|@std@@|^\?\?\$|\?\$vector@|\?\$map@|\?\$_Rb_tree|"
                    r"\?\$multimap@|\?\$set@|\?\$pair@|\?\$less@|StlNodeAlloc")


# ------------------------------------------------------------------ COFF parse
def parse_obj(path):
    """-> list of sections: dict(name, body, comdat, sym) in section order.
    Accepts a path or raw bytes (the selftest fixtures stay in memory: /tmp is a
    RAM-backed tmpfs here and the house rule is to never write to it)."""
    data = path if isinstance(path, (bytes, bytearray)) else open(path, "rb").read()
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


# -------------------------------------------------------------- the comparators
#
# ★ TWO COMPARATORS SHIP, AND THE NUMBERS THEY PRODUCE ARE NOT THE SAME NUMBER.
#
#   v2 "symbol"     -- CORRECT. Pairs .text sections by their COMDAT symbol.
#   v1 "positional" -- LEGACY, KNOWN DEFECTIVE. Pairs sections by INDEX via zip().
#
# v1 is retained ONLY so a figure published before 2026-08-03 can be reproduced on
# demand. It is defective in two independent ways, both of which fire in the wild
# exactly when the gate adds a function-local static (i.e. the case this tool is
# most often pointed at):
#
#   MISALIGNMENT -- a gate that adds a local static inserts .bss/.rdata/.pdata
#     sections BETWEEN .text sections. zip() then pairs a .text body against a
#     .bss body and attributes the resulting garbage word-count to the .text
#     symbol. Measured on system/synth/Faders.cpp + /DRB3_HANDLE_LOCAL_STATIC:
#     off=389 sections, on=396, and 188 of the 389 zip() pairs compare two
#     DIFFERENT symbols. The published "Faders owned=376 template=211" is that.
#   TRUNCATION -- zip() stops at the shorter list, so sections past the shorter
#     object's count are NEVER COMPARED (7 of them on that same pair). A .text
#     COMDAT added by the gate can therefore be silently invisible.
#
# Both comparators use the same word-differ and the same owned/template
# discriminator, so a v1-vs-v2 delta is attributable purely to pairing.

COMPARATOR_VERSIONS = {
    "symbol":     "v2 symbol-keyed (CORRECT, default since 2026-08-03, lane DO-4)",
    "positional": "v1 positional zip() (LEGACY, KNOWN DEFECTIVE -- reproduces pre-DO-4 figures)",
}
DEFAULT_COMPARATOR = "symbol"


def _changed_words(pa, pb):
    """Count differing 4-byte words, counting the tail of the longer body as changed.

    For equal-length bodies this is identical to v1's `min(len)-3` loop; it differs
    only when a COMDAT changes SIZE, which v1 silently dropped."""
    n = 0
    for k in range(0, max(len(pa), len(pb)), 4):
        if pa[k:k + 4] != pb[k:k + 4]:
            n += 1
    return n


def _classify(owned, tmpl, syms, added, removed, comparator, misaligned=None):
    syms.sort(reverse=True)
    r = dict(owned=owned, template=tmpl, live=(owned > 0), syms=syms,
             added=added, removed=removed, comparator=comparator)
    if misaligned is not None:
        r["misaligned_pairs"] = misaligned
    return r


def compare_symbol(A, B):
    """v2. Pair .text sections by COMDAT symbol; sections present in only one leg
    count ALL their words as changed. REFUSES (raises) if .text cannot be keyed."""
    def index(secs, leg):
        idx = {}
        for s in secs:
            if not s["name"].startswith(".text"):
                continue
            k = s["sym"]
            # Refuse rather than return a misleading number: an unnameable or
            # duplicated .text COMDAT cannot be paired by symbol, and silently
            # dropping/overwriting it is exactly the v1 failure in a new costume.
            if not k:
                raise ValueError(f"{leg} leg: a .text section has no COMDAT symbol "
                                 f"-- cannot pair by symbol (use --comparator positional "
                                 f"only if you accept v1's known defect)")
            if k in idx:
                raise ValueError(f"{leg} leg: duplicate .text symbol {k!r} -- ambiguous pairing")
            idx[k] = s
        return idx

    ia, ib = index(A, "off"), index(B, "on")
    owned = tmpl = added = removed = 0
    syms = []
    for k in sorted(set(ia) | set(ib)):
        a, b = ia.get(k), ib.get(k)
        if a is not None and b is not None:
            if a["body"] == b["body"] and a["rels"] == b["rels"]:
                continue
            n = _changed_words(a["body"], b["body"])
        elif b is None:
            removed += 1
            n = _changed_words(a["body"], b"")
        else:
            added += 1
            n = _changed_words(b"", b["body"])
        if STL_RE.search(k):
            tmpl += n
        else:
            owned += n
            syms.append((n, k))
    return _classify(owned, tmpl, syms, added, removed, "symbol")


def compare_positional(A, B):
    """v1, VERBATIM. Kept only to reproduce figures published before 2026-08-03.
    Also reports how many pairs it mis-paired, so the defect is visible in output."""
    owned = tmpl = 0
    syms = []
    misaligned = sum(1 for a, b in zip(A, B)
                     if (a["name"], a["sym"]) != (b["name"], b["sym"]))
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
    return _classify(owned, tmpl, syms, "n/a", "n/a", "positional", misaligned=misaligned)


COMPARATORS = dict(symbol=compare_symbol, positional=compare_positional)


# ------------------------------------------------------------------- the probe
def gate_liveness(repo, unit_key, flag="RB3_MAP_0x1C", all_syms=False, scratch=None,
                  comparator=DEFAULT_COMPARATOR, keep=None):
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
        if keep:
            os.makedirs(keep, exist_ok=True)
            for leg in ("on", "off"):
                shutil.copy(os.path.join(tmp, f"{leg}.obj"), os.path.join(keep, f"{leg}.obj"))
        A, B = parse_obj(os.path.join(tmp, "off.obj")), parse_obj(os.path.join(tmp, "on.obj"))
        try:
            r = COMPARATORS[comparator](A, B)
        except ValueError as e:
            return dict(unit=unit_key, error=f"REFUSED: {e}")
        r.update(unit=unit_key, gated=(dflag in edge["cflags"].split()),
                 sections=(len(A), len(B)))
        if not all_syms:
            r["syms"] = r["syms"][:6]
        return r
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


# ------------------------------------------------------- selftest / regression
# A synthetic COFF pair pinning the EXACT bug v1 has. No toolchain needed, so this
# can never skip (INSTRUMENT_DESIGN rule 1: assert a known-positive before trusting
# a zero; rule 2: the control must be demonstrated able to FAIL).

def _make_coff(sections):
    """sections: list of (name, symbol, body) -> bytes of a minimal COFF object."""
    nsec, nsym = len(sections), len(sections)
    hdrs_end = 20 + nsec * 40
    bodies, ptrs, off = b"", [], hdrs_end
    for _n, _s, body in sections:
        ptrs.append(off if body else 0)
        bodies += body
        off += len(body)
    symptr = off
    strtab, strdata = {}, b""
    for _n, sym, _b in sections:
        if len(sym) > 8:
            strtab[sym] = 4 + len(strdata)
            strdata += sym.encode() + b"\0"
    out = struct.pack("<HHIIIHH", 0x01F2, nsec, 0, symptr, nsym, 0, 0)
    for i, (name, _s, body) in enumerate(sections):
        out += name.encode().ljust(8, b"\0")
        out += struct.pack("<IIIIIIHHI", 0, 0, len(body), ptrs[i], 0, 0, 0, 0, 0x1000)
    out += bodies
    for i, (_n, sym, _b) in enumerate(sections):
        nm = (struct.pack("<II", 0, strtab[sym]) if sym in strtab
              else sym.encode().ljust(8, b"\0"))
        out += nm + struct.pack("<I", 0) + struct.pack("<hHBB", i + 1, 0x20, 2, 0)
    out += struct.pack("<I", len(strdata) + 4) + strdata
    return out


def _fixture(sections):
    """Build a synthetic COFF and parse it, entirely in memory -- no filesystem."""
    return parse_obj(_make_coff(sections))


def selftest():
    """Return (ok, lines). Three fixtures; both labels must be produced."""
    L, ok = [], True
    A_ = b"\x11" * 16          # a .text body, 4 words
    B_ = b"\x22" * 16
    C_ = b"\x33" * 16
    GUARD = b"\x00" * 8        # a .bss the gate inserts for a function-local static

    def chk(label, cond, detail=""):
        nonlocal ok
        L.append(f"  [{'PASS' if cond else 'FAIL'}] {label}{(' -- ' + detail) if detail else ''}")
        ok = ok and cond

    # ---- fixture 1: INSERTED SECTIONS ONLY. Ground truth: no .text body changed.
    off = _fixture([(".text", "?A@@QAAXXZ", A_), (".text", "?B@@QAAXXZ", B_),
                    (".text", "?C@@QAAXXZ", C_), (".rdata", "?r@@3HA", b"\x01" * 8)])
    on = _fixture([(".text", "?A@@QAAXXZ", A_), (".bss", "?g1@@3HA", GUARD),
                   (".text", "?B@@QAAXXZ", B_), (".text", "?C@@QAAXXZ", C_),
                   (".rdata", "?r@@3HA", b"\x01" * 8), (".bss", "?g2@@3HA", GUARD)])
    L.append("fixture 1 -- gate inserts .bss between .text COMDATs; NO .text body changes")
    L.append(f"           sections off={len(off)} on={len(on)}; ground truth: owned=0 (INERT)")
    v2 = compare_symbol(off, on)
    chk("v2 symbol-keyed reports INERT owned=0", v2["owned"] == 0 and not v2["live"],
        f"got owned={v2['owned']}")
    v1 = compare_positional(off, on)
    # This is the demonstration that the control CAN FAIL. If v1 ever passes here,
    # someone silently changed the legacy comparator and every reproduced pre-DO-4
    # figure became unreproducible -- so this asserts v1 STAYS WRONG.
    chk("v1 positional is DEMONSTRATED WRONG here (reports LIVE on an inert pair)",
        v1["live"] and v1["owned"] > 0, f"got owned={v1['owned']} live={v1['live']}")
    chk("v1 reports its own misalignment count > 0", v1["misaligned_pairs"] > 0,
        f"got {v1['misaligned_pairs']}")
    L.append(f"           v2 owned={v2['owned']}  v1 owned={v1['owned']} "
             f"(misaligned pairs={v1['misaligned_pairs']}, "
             f"{len(on) - len(off)} sections never compared)")

    # ---- fixture 2: a REAL body change. Proves v2 is not hardcoded to zero and
    #      attributes the change to the right symbol (rule 4: produce both labels).
    on2 = _fixture([(".text", "?A@@QAAXXZ", A_), (".bss", "?g1@@3HA", GUARD),
                    (".text", "?B@@QAAXXZ", B_[:8] + b"\x99" * 8),
                    (".text", "?C@@QAAXXZ", C_), (".rdata", "?r@@3HA", b"\x01" * 8)])
    L.append("fixture 2 -- same insertion PLUS 2 genuinely changed words in ?B@@")
    v2b = compare_symbol(off, on2)
    chk("v2 reports LIVE owned=2", v2b["owned"] == 2 and v2b["live"], f"got owned={v2b['owned']}")
    chk("v2 attributes the change to ?B@@QAAXXZ",
        v2b["syms"] and v2b["syms"][0][1] == "?B@@QAAXXZ", f"got {v2b['syms']}")

    # ---- fixture 3: the gate ADDS a .text COMDAT past the shorter obj's count.
    #      v1 truncates and cannot see it at all -> a silent false INERT.
    off3 = _fixture([(".text", "?A@@QAAXXZ", A_)])
    on3 = _fixture([(".text", "?A@@QAAXXZ", A_), (".text", "?NEW@@QAAXXZ", B_)])
    L.append("fixture 3 -- gate ADDS a .text COMDAT (4 words) past the shorter obj's end")
    v2c, v1c = compare_symbol(off3, on3), compare_positional(off3, on3)
    chk("v2 sees the added COMDAT (owned=4, added=1)",
        v2c["owned"] == 4 and v2c["added"] == 1, f"got owned={v2c['owned']} added={v2c['added']}")
    chk("v1 is DEMONSTRATED WRONG here (truncates -> false INERT owned=0)",
        v1c["owned"] == 0 and not v1c["live"], f"got owned={v1c['owned']}")

    # ---- refusal: unkeyable .text must REFUSE, not return a misleading number.
    L.append("refusal   -- an unnameable/duplicate .text COMDAT must REFUSE")
    dup = _fixture([(".text", "?A@@QAAXXZ", A_), (".text", "?A@@QAAXXZ", B_)])
    try:
        compare_symbol(dup, dup)
        chk("v2 refuses duplicate .text symbols", False, "returned a number instead")
    except ValueError as e:
        chk("v2 refuses duplicate .text symbols", True, str(e)[:60])
    return ok, L


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[2],
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("units", nargs="*", help="objects.json-style unit keys, e.g. band3/tour/Tour.cpp")
    ap.add_argument("--repo", default=HERE, help="repo/worktree root to compile in")
    ap.add_argument("--flag", default="RB3_MAP_0x1C")
    ap.add_argument("--all-syms", action="store_true")
    ap.add_argument("--scratch", default=os.path.expanduser("~/tmp"))
    ap.add_argument("--quiet-inert", action="store_true")
    ap.add_argument("--comparator", choices=sorted(COMPARATORS), default=DEFAULT_COMPARATOR,
                    help="symbol = v2 (correct, default); positional = v1 (legacy, defective)")
    ap.add_argument("--keep", help="also copy both legs' objs to this dir (debug)")
    ap.add_argument("--selftest", action="store_true",
                    help="run the synthetic-COFF regression fixtures; no toolchain needed")
    a = ap.parse_args()
    if a.selftest:
        ok, lines = selftest()
        print(f"# gate_liveness selftest -- comparator pairing regression")
        print("\n".join(lines))
        print("SELFTEST " + ("PASS" if ok else "FAIL"))
        return 0 if ok else 1
    if not a.units:
        ap.error("no units given (and --selftest not requested)")
    # ★ BANNER: a future reader must be able to tell WHICH comparator produced a
    #   number. owned/template totals are NOT comparable across comparators.
    print(f"# gate_liveness comparator={a.comparator} :: {COMPARATOR_VERSIONS[a.comparator]}")
    if a.comparator == "positional":
        print("# ⚠ LEGACY COMPARATOR: pairs sections by index; misaligns on any inserted "
              "section and never compares past the shorter obj. Figures are reproduced, not trusted.")
    os.makedirs(a.scratch, exist_ok=True)
    rc = 0
    for u in a.units:
        r = gate_liveness(a.repo, u, a.flag, a.all_syms, a.scratch, a.comparator, a.keep)
        if "error" in r:
            print(f"{u:52s} ERROR {r['error']} {r.get('detail','')}")
            rc = 1
            continue
        if a.quiet_inert and not r["live"]:
            continue
        extra = ""
        if r["comparator"] == "symbol" and (r["added"] or r["removed"]):
            extra = f" +{r['added']}/-{r['removed']} text-COMDATs"
        elif r["comparator"] == "positional" and r.get("misaligned_pairs"):
            extra = f" ⚠{r['misaligned_pairs']} misaligned pairs"
        print(f"{u:52s} {'LIVE' if r['live'] else 'INERT':5s} gated={r['gated']!s:5s} "
              f"owned={r['owned']:4d} template={r['template']:4d}{extra}")
        for n, s in r["syms"]:
            print(f"      {n:4d} words  {s}")
    return rc


if __name__ == "__main__":
    sys.exit(main())
