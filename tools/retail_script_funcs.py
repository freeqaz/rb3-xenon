#!/usr/bin/env python3
"""Every C++ script function retail RB3 registers, read off band.exe (W16-UG).

A registration is DataRegisterFunc(Symbol(name), func). Retail compiles it two
ways, and this decodes both from the image with no symbols beyond the map:

  call    bl Symbol::Symbol(const char *) with r4 = the name string, then
          bl DataRegisterFunc (0x827639C0) with r4 = the function;
  inline  the same Symbol ctor, then bl map<Symbol,DataFunc*>::operator[]
          (0x82359F28) in a function that addresses gDataFuncs (0x82E05D30),
          and the function's address built into a register before the store.
          (DataInitFuncs inlines all of its sites; gDataFuncs stays in a
          non-volatile register, so it is addressed once per function.)

Calibration, checked on every run (rc 2 if any fails, so a decoder that stops
seeing registrations cannot report "nothing missing"):
  * DataInitFuncs yields exactly the 147 string-literal names of our
    src/system/obj/DataFunc.cpp, in source order;
  * the stage-kit init (0x82522608) yields the 13 stagekit_* names;
  * every bl DataRegisterFunc site, and every operator[] call in a gDataFuncs
    function, is accounted for except the 17 whose name is built on the stack
    (DataInitFuncs' 7 `magic` keys, Synth::InitSecurity 2, ByteGrinder::Init 8;
    all three are in our source, shared with native).

Usage:
  retail_script_funcs.py list [--json OUT]
  retail_script_funcs.py census DUMP...   (RB3_DATAFUNCS_DUMP files, one per
                                          target; prints retail names each lacks,
                                          grouped by retail registrar)
"""
import argparse
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import retail_boot_config as rbc  # noqa: E402

GDATAFUNCS = 0x82E05D30
DATA_REGISTER_FUNC = 0x827639C0
MAP_INDEX = 0x82359F28  # map<Symbol, DataFunc *>::operator[]
SYMBOL_CTOR = 0x827C0728
DATA_INIT_FUNCS = 0x82763A00
STAGEKIT_INIT = 0x82522608
COMPUTED_NAME_SITES = 17

LOADS = (32, 34, 40, 42)  # GPR-writing D-form loads (lfs/lfd write FPRs)
DFORM = (32, 34, 36, 38, 40, 44, 48, 50, 52, 54)


class Scanner:
    def __init__(self, exe=rbc.DEFAULT_EXE):
        self.img = rbc.Image(exe)
        by_name, self.sizes = rbc.load_names()
        self.names = {v: k for k, v in by_name.items()}
        self.starts = set(self.sizes)

    def string(self, addr):
        try:
            s = self.img.cstr(addr)
        except (ValueError, IndexError):
            return None
        if 0 < len(s) < 80 and all(32 <= ord(c) < 127 for c in s):
            return s
        return None

    def events(self, start):
        """Linear decode of one function: SYM(name) at each Symbol ctor whose r4
        is a string, GDF where &gDataFuncs is formed or addressed, DRF at each
        bl DataRegisterFunc (with r4), BL for other calls, FN where a register
        is built to another function's start."""
        img, regs, ev = self.img, {}, []
        for a in range(start, start + self.sizes[start], 4):
            w = img.word(a)
            op, rd, ra = w >> 26, (w >> 21) & 31, (w >> 16) & 31
            imm = w & 0xFFFF
            simm = imm - 0x10000 if imm & 0x8000 else imm

            def setv(reg, v, full):
                # Only an addi completes an address; a bare lis holds the high
                # half, which can equal a 64 KB-aligned function start by
                # coincidence (0x82760000 is both lis 0x8276 and DataObject).
                regs[reg] = v
                if v == GDATAFUNCS:
                    ev.append((a, "GDF", v))
                elif full and v is not None and v in self.starts and v != start:
                    ev.append((a, "FN", v))

            if op == 15:  # lis / addis
                base = 0 if ra == 0 else regs.get(ra)
                setv(rd, None if base is None else (base + (simm << 16)) & 0xFFFFFFFF, False)
            elif op == 14:  # li / addi
                base = 0 if ra == 0 else regs.get(ra)
                setv(rd, None if base is None else (base + simm) & 0xFFFFFFFF, True)
            elif op == 24:  # ori writes rA
                regs[ra] = (regs[rd] | imm) if regs.get(rd) is not None else None
            elif op == 18 and (w & 3) == 1:  # bl
                li = w & 0x03FFFFFC
                if li & 0x02000000:
                    li -= 0x04000000
                t = (a + li) & 0xFFFFFFFF
                if t == SYMBOL_CTOR:
                    r4 = regs.get(4)
                    ev.append((a, "SYM", self.string(r4) if r4 is not None else None))
                elif t == DATA_REGISTER_FUNC:
                    ev.append((a, "DRF", regs.get(4)))
                else:
                    ev.append((a, "BL", t))
                for r in range(3, 13):
                    regs.pop(r, None)
            elif op in DFORM:
                if ra and regs.get(ra) is not None and (regs[ra] + simm) & 0xFFFFFFFF == GDATAFUNCS:
                    ev.append((a, "GDF", GDATAFUNCS))
                if op in LOADS:
                    regs[rd] = None
            elif op == 31:
                xo, rb = (w >> 1) & 0x3FF, (w >> 11) & 31
                if xo == 444 and rd == rb:
                    regs[ra] = regs.get(rd)
                elif xo in rbc.LOGICAL_XO:
                    regs[ra] = None
                else:
                    regs[rd] = None
            elif op in (21, 23, 25, 26, 27, 28, 29, 30):
                regs[ra] = None
            elif op in (7, 8, 12, 13, 33, 35, 41, 43, 46, 58):
                regs[rd] = None
        return ev

    def registrations(self, start):
        ev = self.events(start)
        if not any(e[1] in ("GDF", "DRF") for e in ev):
            return [], ev
        out = []
        for i, e in enumerate(ev):
            if e[1] != "SYM" or e[2] is None:
                continue
            win = []
            for x in ev[i + 1:]:
                if x[1] == "SYM":
                    break
                win.append(x)
            drf = [x for x in win if x[1] == "DRF"]
            if drf:
                out.append((e[0], e[2], drf[0][2], "call"))
            elif any(x[1] == "BL" and x[2] == MAP_INDEX for x in win):
                fns = [x[2] for x in win if x[1] == "FN"]
                out.append((e[0], e[2], fns[-1] if fns else None, "inline"))
        return out, ev

    def scan(self):
        regs, unaccounted = [], []
        for a in sorted(self.sizes):
            found, ev = self.registrations(a)
            sites = {r[0] for r in found}
            for r in found:
                regs.append(dict(site=r[0], registrar=a, name=r[1], func=r[2], kind=r[3]))
            has_gdf = any(e[1] in ("GDF", "DRF") for e in ev)
            if a == DATA_REGISTER_FUNC:
                continue  # its own operator[] call is the body, not a site
            for i, e in enumerate(ev):
                if e[1] == "DRF" or (has_gdf and e[1] == "BL" and e[2] == MAP_INDEX):
                    j = i - 1
                    while j >= 0 and ev[j][1] != "SYM":
                        j -= 1
                    if j < 0 or ev[j][0] not in sites:
                        unaccounted.append((a, e[0]))
        return regs, unaccounted

    def label(self, addr):
        if addr is None:
            return "?"
        return self.names.get(addr) or "fn_%08X" % addr


def source_datainitfuncs():
    src = open(os.path.join(rbc.ROOT, "src", "system", "obj", "DataFunc.cpp")).read()
    body = src[src.index("void DataInitFuncs() {"):]
    body = body[:body.index("\n}\n")]
    return re.findall(r'DataRegisterFunc\("([^"]+)"', body)


def calibrate(sc, regs, unaccounted):
    problems = []
    got = [r["name"] for r in regs if r["registrar"] == DATA_INIT_FUNCS]
    want = source_datainitfuncs()
    if got != want or len(want) != 147:
        problems.append("DataInitFuncs: decoded %d names, source has %d (%s)"
                        % (len(got), len(want), "order differs" if sorted(got) == sorted(want) else "sets differ"))
    sk = sorted(r["name"] for r in regs if r["registrar"] == STAGEKIT_INIT)
    if len(sk) != 13 or not all(n.startswith("stagekit_") or n == "set_stagekit_strobe"
                                 or n == "set_stagekit_leds" for n in sk):
        problems.append("stage-kit init: decoded %r" % sk)
    if len(unaccounted) != COMPUTED_NAME_SITES:
        problems.append("%d registration sites unaccounted, want %d (the computed-name sites): %s"
                        % (len(unaccounted), COMPUTED_NAME_SITES,
                           ", ".join("%s@%08X" % (sc.label(f), s) for f, s in unaccounted[:24])))
    if any(r["func"] is None for r in regs):
        problems.append("%d registration(s) with no function address"
                        % sum(r["func"] is None for r in regs))
    return problems


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("list")
    p.add_argument("--json")
    p.add_argument("--exe", default=rbc.DEFAULT_EXE)
    p = sub.add_parser("census")
    p.add_argument("dumps", nargs="+")
    p.add_argument("--exe", default=rbc.DEFAULT_EXE)
    a = ap.parse_args()

    sc = Scanner(a.exe)
    regs, unaccounted = sc.scan()
    problems = calibrate(sc, regs, unaccounted)
    by_registrar = {}
    for r in regs:
        by_registrar.setdefault(r["registrar"], []).append(r)
    print("retail registers %d named script function(s) in %d registrar(s); %d distinct names; "
          "%d more sites build their name on the stack"
          % (len(regs), len(by_registrar), len({r["name"] for r in regs}), len(unaccounted)))
    for p_ in problems:
        print("CALIBRATION FAILED: " + p_)
    if problems:
        return 2
    print("calibration: DataInitFuncs 147/147 in source order, stage-kit 13, "
          "computed-name sites %d" % COMPUTED_NAME_SITES)

    if a.cmd == "list":
        for reg in sorted(by_registrar, key=lambda x: -len(by_registrar[x])):
            rs = by_registrar[reg]
            print("  %-60s %3d  %s" % (sc.label(reg)[:60], len(rs), " ".join(r["name"] for r in rs)[:200]))
        if a.json:
            with open(a.json, "w") as f:
                json.dump([dict(r, site="0x%08X" % r["site"], registrar="0x%08X" % r["registrar"],
                                registrar_name=sc.label(r["registrar"]),
                                func="0x%08X" % r["func"] if r["func"] else None,
                                func_name=sc.label(r["func"])) for r in regs], f, indent=1)
        return 0

    # census
    rc = 0
    for d in a.dumps:
        have = set(l.strip() for l in open(d) if l.strip())
        if not have:
            print("%s: EMPTY dump" % d)
            rc = 2
            continue
        missing = {}
        for r in regs:
            if r["name"] not in have:
                missing.setdefault(r["registrar"], []).append(r["name"])
        n = sum(len(v) for v in missing.values())
        print("%s: %d registered natively; %d of %d retail names absent, from %d registrar(s)"
              % (os.path.basename(d), len(have), n, len({r["name"] for r in regs}), len(missing)))
        for reg in sorted(missing, key=lambda x: -len(missing[x])):
            print("    %-58s %3d  %s" % (sc.label(reg)[:58], len(missing[reg]),
                                         " ".join(missing[reg])[:150]))
    return rc


if __name__ == "__main__":
    sys.exit(main())
