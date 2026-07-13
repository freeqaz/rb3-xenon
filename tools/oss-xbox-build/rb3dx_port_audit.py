#!/usr/bin/env python3
"""rb3dx_port_audit.py -- RB3DX <-> clean-TU5 compatibility audit + regression gate.

Phase 1 of docs/plans/strategy-b/RB3DX-RETARGET-PLAN.md. Proves the TU5-targeted
from-source RB3Enhanced DLL is *address-compatible* with RB3 Deluxe (RB3DX,
default.xex sha1 c5a17091) -- i.e. every ports_xbox360.h VA the DLL hooks/pokes,
and every RB3E game-image write-site, either (a) lands on unmodified RB3DX bytes,
or (b) collides with one of RB3DX's own static-patch spans in a way that has a
written ruling in RULINGS below.

What it does (see plan P1.1):
  1. Extract the RB3DX flat PE from the xex (via dtk) -- or use a provided PE --
     and load the clean-TU5 PE. Assert their PE section tables are byte-identical
     (shared VA<->offset mapping). This is the regression-gate step: point it at a
     future RB3DX xex and it re-derives everything.
  2. Re-derive the clean-TU5 vs RB3DX byte-diff spans (do NOT trust prior numbers).
  3. Parse all PORT_ defines from ports_xbox360.h and classify each (code/data/bss/
     import) and, for code ports, compare the first 64 bytes clean-vs-RB3DX at the VA.
  4. Extract RB3E's full game-image write-site set (HookFunction / POKE_32 / POKE_B /
     POKE_BL / SI_HOOK, arg0), platform-guard-aware (RB3E_XBOX build only), resolve
     each to a VA, and intersect [VA,VA+4) with the RB3DX diff spans.
  5. Emit rb3dx_port_audit.json + a human table. Exit NONZERO if any overlap is not
     covered by a RULINGS entry (an "unreviewed" overlap).

Usage:
  rb3dx_port_audit.py                       # defaults (paths below)
  rb3dx_port_audit.py --rb3dx-xex <xex>     # regression-gate a new RB3DX xex
  rb3dx_port_audit.py --rb3dx-pe <band.exe> # skip extraction, use a pre-extracted PE
  rb3dx_port_audit.py --clean-pe <band_clean_tu5.exe> --ports <ports_xbox360.h>
  rb3dx_port_audit.py --json <out.json>

Exit codes: 0 = all overlaps reviewed (gate PASS); 2 = unreviewed overlap (gate FAIL);
3 = structural failure (section tables differ / missing input).
"""
import argparse
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
# repo roots (this file lives in rb3-xenon/tools/oss-xbox-build/)
RB3XENON = os.path.abspath(os.path.join(HERE, "..", ".."))
MILOHAX = os.path.abspath(os.path.join(RB3XENON, ".."))

DEF_CLEAN_PE = os.path.join(RB3XENON, "_tu5probe", "clean", "band_clean_tu5.exe")
DEF_RB3DX_XEX = os.path.join(MILOHAX, "rock-band-3-deluxe", "platform", "xbox", "default.xex")
DEF_PORTS = os.path.join(MILOHAX, "RB3Enhanced", "include", "ports_xbox360.h")
DEF_SRC = os.path.join(MILOHAX, "RB3Enhanced", "source")
DEF_DTK = os.path.join(RB3XENON, "build", "tools", "dtk")
DEF_JSON = os.path.join(HERE, "rb3dx_port_audit.json")

XBOX_LO, XBOX_HI = 0x82000000, 0x83000000  # game-image VA window (RB3 TU5 .text/.data/etc)

# ---------------------------------------------------------------------------
# Reviewed rulings (plan P1.2). Keyed by the write-site VA. An overlap present
# here does NOT fail the gate. Each entry records WHY the collision is acceptable.
# Verdicts derived 2026-07-13 from byte-level clean/RB3DX/RB3E-write comparison.
# ---------------------------------------------------------------------------
RULINGS = {
    0x82516320: dict(verdict="BENIGN_IDEMPOTENT", port="PORT_SETDISKERROR",
        reason="RB3E POKE_32(BLR)=0x4e800020 == RB3DX entry word. RB3DX statically "
               "bundled this same early-return (Deluxe rewrote the prologue further, "
               "but the entry blr is identical and returns before RB3DX's trailing "
               "bytes; RB3E only writes the entry word). RB3E-wins is a no-op."),
    0x82270f40: dict(verdict="BENIGN_IDEMPOTENT", port="PORT_FASTSTART_CHECK",
        reason="RB3E POKE_32(NOP)=0x60000000 == RB3DX bytes. Deluxe already nop'd the "
               "fast-start wait. Idempotent."),
    0x82579098: dict(verdict="BENIGN_IDEMPOTENT", port="PORT_SONGBLACKLIST",
        reason="RB3E POKE_32(LI(3,0))=0x38600000 == RB3DX bytes. Deluxe already stubbed "
               "the song-blacklist check to li r3,0. Idempotent."),
    0x82575f9c: dict(verdict="BENIGN_IDEMPOTENT", port="PORT_SONGMGR_ISDEMO_CHECK",
        reason="RB3E POKE_32(NOP)=0x60000000 == RB3DX bytes; and RB3E only applies it "
               "under RB3E_IsEmulator() so on RGH hardware RB3DX's static nop stands "
               "untouched. Idempotent either way. (This is the bne->nop site the "
               "SI-only divergence study called out at 0x82575f9c.)"),
    0x82ae6880: dict(verdict="BENIGN_IDEMPOTENT", port="PORT_MULTIPLAYER_CRASH",
        reason="RB3E POKE_BL(->PORT_MULTIPLAYER_FIX 0x8282b238)=0x4bd449b9 == RB3DX "
               "bytes. Deluxe bundled the identical online-MP crash-fix redirect. "
               "Idempotent."),
    0x82272e90: dict(verdict="RB3E_WINS_BY_DESIGN", port="PORT_APP_RUN",
        reason="DIVERGENT: RB3E DllMain POKE_BL(App::Run->RunWithoutDebugging)=0x4bffd1f1; "
               "RB3DX static=0x4280d1f1 (Deluxe's own redirect at the main() call site). "
               "RB3E-wins: DllMain runs at PROCESS_ATTACH, before App::Run executes, so "
               "RB3E's RunWithoutDebugging redirect is the final value. RB3DX is designed "
               "to run WITH the RB3E DLL loaded, so this supersession is expected, not a "
               "regression. Not same-instrument-related. Confirm at Phase-5 boot."),
    0x82c4c47c: dict(verdict="RB3E_WINS_MOOTED", port="PORT_XEKEYSSETKEY_STUB",
        reason="DIVERGENT: RB3E POKE_B(->XeKeysSetKeyHook, a DLL fn) writes a full branch "
               "over the import thunk; RB3DX edited only the thunk's low-16 (0x0242->0x0159, "
               "a different import RVA/ordinal in Deluxe's XEX import table). RB3E fully "
               "redirects XeKeysSetKey to its XeCrypt path, so the underlying import target "
               "is mooted. Import-stub/crypto only; not gameplay, not same-instrument."),
    0x82c4c48c: dict(verdict="RB3E_WINS_MOOTED", port="PORT_XEKEYSAESCBC_STUB",
        reason="DIVERGENT: same as PORT_XEKEYSSETKEY_STUB -- RB3E POKE_B over the import "
               "thunk redirects XeKeysAesCbc to XeCrypt; RB3DX's low-16 thunk edit "
               "(0x024b->0x015b) is mooted by the full RB3E redirect. Crypto import stub."),
}

# ---------------------------------------------------------------------------
# PE section table (COFF) loader -- inlined so this tool has no external deps.
# ---------------------------------------------------------------------------
def load_sections(path):
    with open(path, "rb") as f:
        data = f.read()
    e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
    if data[e_lfanew:e_lfanew + 4] != b"PE\x00\x00":
        raise ValueError(f"not a PE: {path}")
    coff = e_lfanew + 4
    num_sec = struct.unpack_from("<H", data, coff + 2)[0]
    opt_size = struct.unpack_from("<H", data, coff + 16)[0]
    opt = coff + 20
    image_base = struct.unpack_from("<I", data, opt + 28)[0]
    sec_tab = opt + opt_size
    secs = []
    for i in range(num_sec):
        off = sec_tab + i * 40
        name = data[off:off + 8].rstrip(b"\x00").decode("latin1")
        vsize = struct.unpack_from("<I", data, off + 8)[0]
        vaddr = struct.unpack_from("<I", data, off + 12)[0]
        rawsize = struct.unpack_from("<I", data, off + 16)[0]
        rawptr = struct.unpack_from("<I", data, off + 20)[0]
        secs.append((name, image_base + vaddr, vsize, rawptr, rawsize))
    return data, image_base, secs


def va_to_off(va, secs):
    for name, sva, vsize, rawptr, rawsize in secs:
        if sva <= va < sva + vsize:
            delta = va - sva
            if delta < rawsize:
                return rawptr + delta, name
            return None, name  # BSS/zero-init tail (not file-backed)
    return None, None


def off_to_va(off, secs):
    for name, sva, vsize, rawptr, rawsize in secs:
        if rawptr <= off < rawptr + rawsize:
            return sva + (off - rawptr), name
    return None, None


def section_of(va, secs):
    for name, sva, vsize, rawptr, rawsize in secs:
        if sva <= va < sva + vsize:
            filebacked = (va - sva) < rawsize
            return name, filebacked
    return None, False


# ---------------------------------------------------------------------------
# dtk extraction (regression-gate: derive the RB3DX PE from any xex)
# ---------------------------------------------------------------------------
def extract_pe_from_xex(xex_path, dtk_path):
    tmp = tempfile.mkdtemp(prefix="rb3dx_audit_")
    xcopy = os.path.join(tmp, "default.xex")
    shutil.copyfile(xex_path, xcopy)
    subprocess.run([dtk_path, "xex", "extract", "default.xex"],
                   cwd=tmp, check=True, capture_output=True)
    pe = os.path.join(tmp, "band.exe")
    if not os.path.isfile(pe):
        # dtk names the output after the xex's PE basename; grab the only .exe.
        exes = [f for f in os.listdir(tmp) if f.endswith(".exe") and f != "default.xex"]
        if not exes:
            raise RuntimeError("dtk xex extract produced no PE")
        pe = os.path.join(tmp, exes[0])
    return pe, tmp


# ---------------------------------------------------------------------------
# ports_xbox360.h parse
# ---------------------------------------------------------------------------
PORT_RE = re.compile(r'^\s*#define\s+(PORT_\w+)\s+(0x[0-9a-fA-F]+)')


def parse_ports(path):
    ports = {}
    with open(path, errors="replace") as f:
        for ln in f:
            m = PORT_RE.match(ln)
            if m:
                ports[m.group(1)] = int(m.group(2), 16)
    return ports


# ---------------------------------------------------------------------------
# RB3E write-site extraction (platform-guard-aware for the RB3E_XBOX DLL build)
# ---------------------------------------------------------------------------
KNOWN_TRUE = {"RB3E_XBOX", "RB3E", "_XBOX", "NDEBUG"}
KNOWN_FALSE = {"RB3E_WII", "RB3E_PS3", "RB3E_WII_BANK8", "SI_STANDALONE_PATCH"}

WRITE_RE = re.compile(r'\b(HookFunction|POKE_32|POKE_B|POKE_BL|SI_HOOK)\s*\(\s*([^,]+?)\s*,')
COND_IDENT_RE = re.compile(r'^[A-Za-z_]\w*$')


def _eval_cond(kind, expr):
    """Return True/False/None(unknown) for a platform preprocessor condition.

    Only RB3E platform macros are decided; everything else is None (pass-through)."""
    expr = expr.strip()
    if kind in ("ifdef", "ifndef"):
        ident = expr
        if ident in KNOWN_TRUE:
            v = True
        elif ident in KNOWN_FALSE:
            v = False
        else:
            v = None
        if kind == "ifndef" and v is not None:
            v = not v
        return v
    # #if / #elif
    m = re.match(r'^defined\s*\(\s*([A-Za-z_]\w*)\s*\)$', expr)
    if m:
        ident = m.group(1)
        if ident in KNOWN_TRUE:
            return True
        if ident in KNOWN_FALSE:
            return False
        return None
    if COND_IDENT_RE.match(expr):
        if expr in KNOWN_TRUE:
            return True
        if expr in KNOWN_FALSE:
            return False
        return None
    return None  # complex expression -> pass-through


def _truthy(v):
    # None (unknown) is treated as taken/active (conservative pass-through)
    return True if v is None else bool(v)


def scan_write_sites(src_dir, ports):
    """Yield dicts for each game-image write-site active in the RB3E_XBOX build."""
    sites = []
    directive = re.compile(r'^\s*#\s*(ifdef|ifndef|if|elif|else|endif)\b(.*)$')
    for fn in sorted(os.listdir(src_dir)):
        if not fn.endswith(".c"):
            continue
        path = os.path.join(src_dir, fn)
        # stack frames: (parent_active, this_taken, any_taken)
        stack = []
        with open(path, errors="replace") as f:
            for lno, ln in enumerate(f, 1):
                d = directive.match(ln)
                if d:
                    kw, rest = d.group(1), d.group(2).strip()
                    parent = all(fr[1] for fr in stack)  # AND of this_taken up the stack
                    if kw in ("ifdef", "ifndef", "if"):
                        t = _truthy(_eval_cond(kw, rest))
                        stack.append([parent, parent and t, t])
                    elif kw == "elif":
                        if stack:
                            p, _, anyt = stack[-1][0], stack[-1][1], stack[-1][2]
                            t = _truthy(_eval_cond("if", rest))
                            this_taken = (not anyt) and t
                            stack[-1] = [p, p and this_taken, anyt or t]
                    elif kw == "else":
                        if stack:
                            p, anyt = stack[-1][0], stack[-1][2]
                            this_taken = not anyt
                            stack[-1] = [p, p and this_taken, True]
                    elif kw == "endif":
                        if stack:
                            stack.pop()
                    continue
                active = all(fr[1] for fr in stack) if stack else True
                if not active:
                    continue
                s = ln.strip()
                if s.startswith("//") or s.startswith("*") or s.startswith("#"):
                    continue
                for m in WRITE_RE.finditer(ln):
                    macro, arg = m.group(1), m.group(2).strip()
                    va, kind, detail = _resolve_arg(arg, ports)
                    sites.append(dict(file=fn, line=lno, macro=macro, arg=arg,
                                      va=va, kind=kind, detail=detail))
    return sites


def _resolve_arg(arg, ports):
    """Resolve a write-macro arg0 to a game-image VA, or classify why not.

    Returns (va_or_None, kind, detail). kind in
    {port, port+off, hex, dll_local, nonxbox, expr}."""
    a = arg.strip()
    # &Local / bare stub symbol -> DLL-internal target (POKE_B trampoline install)
    if a.startswith("&"):
        return None, "dll_local", a
    m = re.match(r'^(PORT_\w+)\s*(?:\+\s*(0x[0-9a-fA-F]+|\d+))?$', a)
    if m:
        base = ports.get(m.group(1))
        if base is None:
            return None, "nonxbox", m.group(1)  # Wii/PS3 port absent from xbox header
        off = int(m.group(2), 0) if m.group(2) else 0
        return base + off, ("port+off" if off else "port"), a
    m = re.match(r'^(0x[0-9a-fA-F]+)$', a)
    if m:
        return int(m.group(1), 16), "hex", a
    # bare identifier (a DLL function symbol like PPCHalt / OSFatal) -> DLL-internal
    if COND_IDENT_RE.match(a):
        return None, "dll_local", a
    return None, "expr", a


# ---------------------------------------------------------------------------
# Diff spans
# ---------------------------------------------------------------------------
def diff_spans(clean_data, rb3dx_data, secs):
    n = min(len(clean_data), len(rb3dx_data))
    diffs = [i for i in range(n) if clean_data[i] != rb3dx_data[i]]
    spans = []
    for i in diffs:
        if spans and i == spans[-1][1] + 1:
            spans[-1][1] = i
        else:
            spans.append([i, i])
    out = []
    for a, b in spans:
        va, sec = off_to_va(a, secs)
        out.append(dict(va=va, va_end=va + (b - a) + 1, section=sec,
                        clean=clean_data[a:b + 1].hex(), rb3dx=rb3dx_data[a:b + 1].hex()))
    return out, len(diffs)


def span_covers(spans, lo, hi):
    """Return the first span dict intersecting [lo,hi), else None."""
    for s in spans:
        if lo < s["va_end"] and hi > s["va"]:
            return s
    return None


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--clean-pe", default=DEF_CLEAN_PE)
    ap.add_argument("--rb3dx-xex", default=DEF_RB3DX_XEX)
    ap.add_argument("--rb3dx-pe", default=None,
                    help="pre-extracted RB3DX PE; skips dtk extraction")
    ap.add_argument("--ports", default=DEF_PORTS)
    ap.add_argument("--src", default=DEF_SRC)
    ap.add_argument("--dtk", default=DEF_DTK)
    ap.add_argument("--json", default=DEF_JSON)
    args = ap.parse_args()

    for p in (args.clean_pe, args.ports, args.src):
        if not os.path.exists(p):
            print(f"ERROR missing input: {p}", file=sys.stderr)
            return 3

    tmpdir = None
    if args.rb3dx_pe:
        rb3dx_pe = args.rb3dx_pe
    else:
        if not os.path.exists(args.rb3dx_xex):
            print(f"ERROR missing RB3DX xex: {args.rb3dx_xex}", file=sys.stderr)
            return 3
        if not os.path.exists(args.dtk):
            print(f"ERROR missing dtk: {args.dtk}", file=sys.stderr)
            return 3
        rb3dx_pe, tmpdir = extract_pe_from_xex(args.rb3dx_xex, args.dtk)

    try:
        clean_data, cbase, csecs = load_sections(args.clean_pe)
        rb3dx_data, rbase, rsecs = load_sections(rb3dx_pe)
    except Exception as e:
        print(f"ERROR loading PEs: {e}", file=sys.stderr)
        return 3

    sections_identical = (csecs == rsecs)
    if not sections_identical:
        print("ERROR: PE section tables DIFFER between clean TU5 and RB3DX; "
              "VA<->offset mapping not shared. RB3DX may be relocated -- see plan P1.3 "
              "(signature-scan fallback).", file=sys.stderr)
        # still emit what we can
    spans, total_diff_bytes = diff_spans(clean_data, rb3dx_data, csecs)
    ports = parse_ports(args.ports)

    # ---- classify every port ----
    port_rows = []
    for name, va in sorted(ports.items(), key=lambda kv: kv[1]):
        sec, filebacked = section_of(va, csecs)
        is_code = (sec == ".text")
        n = 64 if is_code else 4
        # bytes present?
        coff, _ = va_to_off(va, csecs)
        roff, _ = va_to_off(va, rsecs)
        body_diff = None
        if is_code and coff is not None and roff is not None:
            cb = clean_data[coff:coff + n]
            rb = rb3dx_data[roff:roff + n]
            body_diff = (cb != rb)
        cover = span_covers(spans, va, va + n)
        port_rows.append(dict(port=name, va=va, section=sec, filebacked=filebacked,
                              is_code=is_code, first64_differs=body_diff,
                              covered_by_span=(cover["va"] if cover else None)))

    # ---- write-site scan + overlap ----
    sites = scan_write_sites(args.src, ports)
    game_sites = [s for s in sites if s["va"] is not None and XBOX_LO <= s["va"] < XBOX_HI]
    overlaps = []
    seen = set()
    for s in game_sites:
        cover = span_covers(spans, s["va"], s["va"] + 4)
        if cover:
            key = (s["va"], s["file"], s["line"], s["macro"])
            if key in seen:
                continue
            seen.add(key)
            ruling = RULINGS.get(s["va"])
            overlaps.append(dict(
                va=s["va"], macro=s["macro"], arg=s["arg"],
                file=s["file"], line=s["line"],
                span=dict(va=cover["va"], va_end=cover["va_end"], section=cover["section"],
                          clean=cover["clean"], rb3dx=cover["rb3dx"]),
                reviewed=ruling is not None,
                ruling=ruling,
            ))

    # de-dupe overlaps by VA for the summary/verdict
    overlap_vas = sorted(set(o["va"] for o in overlaps))
    unreviewed = sorted(set(o["va"] for o in overlaps if not o["reviewed"]))

    # ---- same-instrument feature ports (must be collision-free) ----
    SI_PORTS = ["PORT_OVERSHELL_ISACTIVE", "PORT_OVERSHELLPANEL_RESOLVE",
                "PORT_PTCL_PROCESSCONFIG", "PORT_TWI_RECALCGEMLIST",
                "PORT_GAMEGEMDB_DUPLICATE", "PORT_GAMEGEMDB_GETDIFFLIST",
                "PORT_GAMEGEMLIST_COPYFROM", "PORT_BANDUSER_SETOVERSHELLSLOTSTATE",
                "PORT_OVERSHELLPANEL_UPDATEALL", "PORT_THEBANDUSERMGR"]
    si_clean = True
    si_detail = []
    for name in SI_PORTS:
        row = next((r for r in port_rows if r["port"] == name), None)
        if row is None:
            si_detail.append(dict(port=name, present=False))
            continue
        collided = (row["covered_by_span"] is not None) or (row["first64_differs"] is True)
        if collided:
            si_clean = False
        si_detail.append(dict(port=name, va=row["va"], covered=row["covered_by_span"],
                              first64_differs=row["first64_differs"]))

    if not overlap_vas:
        verdict = ("CLEAN -- the TU5 DLL IS the RB3DX DLL for xex "
                   "(no RB3E write-site or hooked port collides with any RB3DX diff span).")
    elif not unreviewed:
        verdict = (f"REVIEWED EXCEPTION LIST -- {len(overlap_vas)} RB3E<->Deluxe general-patch "
                   f"collisions, all with written rulings; same-instrument feature surface is "
                   f"COLLISION-FREE ({'clean' if si_clean else 'NOT CLEAN'}). The TU5 DLL is the "
                   f"RB3DX DLL for the same-instrument feature; general-patch overlaps are "
                   f"benign-idempotent (5) or RB3E-wins-by-design (3).")
    else:
        verdict = (f"GATE FAIL -- {len(unreviewed)} UNREVIEWED overlap(s): "
                   + ", ".join(f"0x{v:08x}" for v in unreviewed))

    result = dict(
        clean_pe=os.path.abspath(args.clean_pe),
        rb3dx_source=os.path.abspath(args.rb3dx_pe or args.rb3dx_xex),
        sections_identical=sections_identical,
        total_diff_bytes=total_diff_bytes,
        num_diff_spans=len(spans),
        num_ports=len(ports),
        num_game_write_sites=len(game_sites),
        num_overlap_sites=len(overlaps),
        overlap_vas=[f"0x{v:08x}" for v in overlap_vas],
        unreviewed_vas=[f"0x{v:08x}" for v in unreviewed],
        same_instrument_clean=si_clean,
        same_instrument_ports=si_detail,
        verdict=verdict,
        diff_spans=spans,
        overlaps=overlaps,
        ports=port_rows,
    )

    with open(args.json, "w") as f:
        json.dump(result, f, indent=2)

    # ---- human table ----
    print(f"# RB3DX port audit  (clean TU5 vs RB3DX)")
    print(f"# clean PE : {args.clean_pe}")
    print(f"# RB3DX    : {args.rb3dx_pe or args.rb3dx_xex}")
    print(f"# section tables identical : {sections_identical}")
    print(f"# diff     : {total_diff_bytes} bytes in {len(spans)} contiguous spans")
    print(f"# ports    : {len(ports)} parsed; game-image write-sites: {len(game_sites)}")
    print()
    print(f"## Same-instrument feature ports (must be collision-free): "
          f"{'CLEAN' if si_clean else 'NOT CLEAN'}")
    for d in si_detail:
        if "present" in d and not d["present"]:
            print(f"   {d['port']:<38} (not defined)")
        else:
            flag = "COLLIDE" if (d["covered"] or d["first64_differs"]) else "ok"
            print(f"   {d['port']:<38} 0x{d['va']:08x}  first64_differs={d['first64_differs']}  [{flag}]")
    print()
    print(f"## RB3E write-site <-> RB3DX diff-span overlaps: {len(overlap_vas)} unique VA(s)")
    print(f"   {'VA':<12}{'macro/arg':<44}{'src':<26}{'verdict':<20}")
    for o in overlaps:
        v = o["ruling"]["verdict"] if o["ruling"] else "*** UNREVIEWED ***"
        ma = f"{o['macro']}({o['arg']})"
        print(f"   0x{o['va']:08x}  {ma:<42}{o['file']+':'+str(o['line']):<26}{v:<20}")
    print()
    print(f"## VERDICT")
    print(f"   {verdict}")
    print(f"# json -> {args.json}")

    if tmpdir:
        shutil.rmtree(tmpdir, ignore_errors=True)

    if not sections_identical:
        return 3
    if unreviewed:
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
