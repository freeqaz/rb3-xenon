#!/usr/bin/env python3
"""nothrow_stub_screen -- find sub-100 CALLERS whose same-TU callee is a LEAF STUB
in our build but a NON-LEAF (can-throw) body in retail.  Read-only; not a build input.

Provenance: lane W16-GN (2026-09-30), generalising W16-GI's fix
(docs/decomp/W16GI_BANDSTOREPANEL_HANDLE_NOTHROW_STUB_2026-09-16.md).

THE MECHANISM
-------------
MSVC proves a same-TU function body NOTHROW when it can see that the body makes
no call that could throw.  A caller that invokes a nothrow callee while a stack
temporary with a non-trivial destructor is live needs NO EH region around that
call, and without the region the post-RA scheduler is free to reorder the
argument setup.  So a stub body copied from the rb3-Wii DEV oracle
(`return DataNode(1);`, `{}`) changes the CALLER's codegen, while the callee
itself may not even be a scored row.  In W16-GI the retail body of the stub
(0x826067C0, 200 B by .pdata) is pinned inside Mat.cpp's unit.

WHY THE EXISTING SCREENS COULD NOT SEE IT
-----------------------------------------
* tools/stub_sweep.py keys on the STUB'S OWN ROW (retail size from report
  rows).  GI's stub has no row in its own unit -> invisible by construction.
* tools/empty_extrn_sweep.py requires our callee to be exactly a 4-byte `blr`.
  GI's stub (`return DataNode(1)` through sret) is several instructions.
This screen is CALLER-keyed: it walks objdiff's aligned instruction pairs in
every sub-100 row and looks at the callee on BOTH sides of each paired `bl`.

THE PREDICATE (thresholds chosen from --survey data, see the lane doc)
---------------------------------------------------------------------------
At a paired `bl` (target and base both `bl`):
  base callee B   defined in the SAME compiled object, LEAF (no REL24 reloc to
                  anything but the save/restore helpers, no `bctrl`), and
                  len(B) <= --base-max bytes
  retail callee T resolved to an address (fn_<addr> or the map's inverse),
                  NON-LEAF (>=1 `bl`/tail-`b` to a non-helper or `bctrl` in its
                  retail extent) and >= --min-ratio x len(B)
The LEAF/NON-LEAF flip is the mechanism itself (nothrow vs may-throw); size is
a secondary guard.  Retail size is the .pdata extent, falling back to the
symbols.txt size (sub-.pdata leaf stubs have no unwind record).

Known false-positive shapes, and how each is handled
  (a) retail is genuinely empty too  -> retail is then a LEAF -> rejected by
      construction (no label needed).
  (b) ICF fold / alias                -> every hit is labelled `alias` if B or T
      appears in scripts/symbol_aliases.json (read-only).  A fold cannot by
      itself make a bigger function stand in for a smaller one (folding needs
      identical bytes), but an alias MAY mean the pairing is wrong.
  (c) MILO_ASSERT / HX_NATIVE-gated   -> retail would be a leaf too -> (a).
  (d) objdiff mis-alignment          -> `names_agree` / `t_placeholder` labels,
      and `adj` = distance (in instructions) to the nearest charged instruction.

VALIDATION (the part that makes the output worth anything)
----------------------------------------------------------
--expect-hit SYM / --expect-silent SYM exit 3 if the named caller is not /
is among the hits.  The lane validated `?Handle@BandStorePanel@@...` fires on
the pre-GI tree (85b84e32) and is silent on the post-GI tree.
--selftest runs predicate + retail-decoder known answers; --self-break
sabotages the leaf test and REQUIRES --selftest to fail (exit 0 only if it did).

Usage:
  python3 tools/nothrow_stub_screen.py [--project-dir DIR] [--json OUT]
        [--layers game|all] [--survey OUT] [--base-max N] [--min-ratio R]
        [--expect-hit SYM] [--expect-silent SYM]
  python3 tools/nothrow_stub_screen.py --selftest [--self-break]

Build the tree FIRST (a fresh worktree's target objs are pre-renamer).
"""
import argparse
import collections
import concurrent.futures
import json
import os
import re
import struct
import subprocess
import sys

VERSION = "45410914"
REL24 = 0x0006
BCTRL = 0x4E800421
HELPER_PREFIXES = ("__savegpr", "__restgpr", "__savefpr", "__restfpr",
                   "__savevmx", "__restvmx", "__savegprlr", "__restgprlr")
GAME_SRC_PREFIXES = ("src/band3/", "src/network/")
# ⛔ Layer is decided by objdiff.json metadata.source_path, NEVER by the unit
# name: bare-heading units (default/BandCharacter, default/RockCentral, ...)
# are game code too.  The first version of this tool keyed on the unit-name
# prefix and silently dropped them (lane W16-GN).
DEFAULT_BASE_MAX = 64      # bytes (16 insns); from --survey: every leaf->non-leaf
                           # flip on the pre-GI tree had a base of 4..20 B
DEFAULT_MIN_RATIO = 2.0

_SELF_BREAK = False


# ---------------------------------------------------------------- retail ----
def _pe_sections(buf):
    po = struct.unpack_from("<I", buf, 0x3C)[0]
    coff = po + 4
    nsec = struct.unpack_from("<H", buf, coff + 2)[0]
    optsz = struct.unpack_from("<H", buf, coff + 16)[0]
    opt = coff + 20
    imgbase = struct.unpack_from("<I", buf, opt + 28)[0]
    out, off = [], opt + optsz
    for _ in range(nsec):
        vsize, vaddr, _rawsz, rawptr = struct.unpack_from("<IIII", buf, off + 8)
        out.append((imgbase + vaddr, vsize, rawptr))
        off += 40
    return out


class Retail:
    def __init__(self, repo):
        sys.path.insert(0, os.path.join(repo, "tools"))
        from pdata_map_audit import load_extents
        exe = os.path.join(repo, "orig", VERSION, "band.exe")
        self.buf = open(exe, "rb").read()
        self.secs = _pe_sections(self.buf)
        self.ext = load_extents(exe)
        self.sym_size, self.helpers = {}, set()
        pat = re.compile(r"^(\S+) = \.text:0x([0-9A-Fa-f]+);.*?size:0x([0-9A-Fa-f]+)")
        for line in open(os.path.join(repo, "config", VERSION, "symbols.txt")):
            m = pat.match(line)
            if not m:
                continue
            a = int(m.group(2), 16)
            self.sym_size.setdefault(a, int(m.group(3), 16))
            if m.group(1).startswith(HELPER_PREFIXES):
                self.helpers.add(a)
        # the save/restore helpers are one contiguous block of entry points
        self.hlo = min(self.helpers) if self.helpers else 0
        self.hhi = (max(self.helpers) + 0x200) if self.helpers else 0

    def read(self, va, n):
        for va0, vsize, rawptr in self.secs:
            if va0 <= va < va0 + vsize:
                o = rawptr + (va - va0)
                return self.buf[o:o + n]
        return b""

    def size(self, va):
        s = self.ext.get(va)
        return (s, "pdata") if s else (self.sym_size.get(va), "symbols")

    def is_helper(self, va):
        return self.hlo <= va < self.hhi

    def calls(self, va, size):
        """Count potentially-throwing transfers in [va, va+size): bl / external
        tail-b to a non-helper, and bctrl.  Decoded word-by-word (no linear
        disassembler that can halt early)."""
        n = 0
        body = self.read(va, size)
        for i in range(0, len(body) - 3, 4):
            w = struct.unpack_from(">I", body, i)[0]
            if w == BCTRL:
                n += 1
                continue
            if (w >> 26) != 18 or (w & 2):
                continue
            li = w & 0x03FFFFFC
            if li & 0x02000000:
                li -= 0x04000000
            dst = (va + i + li) & 0xFFFFFFFF
            if self.is_helper(dst):
                continue
            if w & 1:
                n += 1
            elif not (va <= dst < va + size):
                n += 1
        return n


# ----------------------------------------------------------------- ours ----
def base_bodies(repo, rel):
    sys.path.insert(0, os.path.join(repo, "tools"))
    from coff_bodies_ext import function_bodies_ext
    out = {}
    for nm, body, rl, _e in function_bodies_ext(os.path.join(repo, rel)):
        calls = 0
        for (_o, tgt, ty) in rl:
            if ty == REL24 and not str(tgt).startswith(HELPER_PREFIXES):
                calls += 1
        for i in range(0, len(body) - 3, 4):
            if struct.unpack_from(">I", body, i)[0] == BCTRL:
                calls += 1
        if _SELF_BREAK:
            calls = 0 if calls else 1          # sabotage: invert leafness
        out.setdefault(nm, (len(body), calls))
    return out


def load_map_inverse(repo):
    raw = json.load(open(os.path.join(repo, "scripts", "target_symbol_map.json")))
    n2a = {}
    for k, v in raw.items():
        if k.lower().startswith("0x") and isinstance(v, str) and v:
            n2a.setdefault(v, int(k, 16))
    return n2a


def load_split_owner(repo):
    """[(start, end, heading)] for .text ranges, keyed on the FULL heading."""
    spans, head = [], None
    for line in open(os.path.join(repo, "config", VERSION, "splits.txt")):
        if line and not line[0].isspace() and line.rstrip().endswith(":"):
            head = line.rstrip()[:-1]
            continue
        m = re.match(r"\s+\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)", line)
        if m and head:
            spans.append((int(m.group(1), 16), int(m.group(2), 16), head))
    spans.sort()
    return spans


def owner_of(spans, va):
    for s, e, h in spans:
        if s <= va < e:
            return h
    return None


# ----------------------------------------------------------------- scan ----
def batch_diff(repo, unit, syms):
    p = subprocess.run(
        [os.path.join(repo, "bin", "objdiff-cli"), "diff", "-p", repo, "-u", unit,
         "--batch", "--include-instructions", "-f", "json", "-o", "-"],
        input="\n".join(syms) + "\n", capture_output=True, text=True, cwd=repo)
    out = []
    for line in p.stdout.splitlines():
        line = line.strip()
        if line.startswith("{"):
            try:
                out.append(json.loads(line))
            except ValueError:
                pass
    return out


def is_game(unit):
    if not unit:
        return False
    sp = (unit.get("metadata") or {}).get("source_path") or ""
    return sp.startswith(GAME_SRC_PREFIXES)


def eh_deficits(repo):
    """name -> (retail_maxState, our_min_maxState) where ours < retail.

    The caller-side signature of the whole nothrow class, independent of HOW
    the callee became nothrow (out-of-line stub, INLINED stub, transitive).
    Uses tools/eh_state_screen.py from THIS tool's tree (it carries the
    recursive-glob fix) against `repo`'s artifacts."""
    import importlib.util
    here = os.path.dirname(os.path.abspath(__file__))
    spec = importlib.util.spec_from_file_location("ehs", os.path.join(here, "eh_state_screen.py"))
    e = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(e)
    import glob as _g
    img = e.Image(os.path.join(repo, "orig", VERSION, "band.exe"))
    r = e.retail_maxstates(img, os.path.join(repo, "build", VERSION, "asm"))
    sm = json.load(open(os.path.join(repo, "scripts", "target_symbol_map.json")))
    a2n = {k.lower(): v for k, v in sm.items() if isinstance(v, str) and k.lower().startswith("0x")}
    ours = {}
    for p in _g.glob(os.path.join(repo, "build", VERSION, "src", "**", "*.obj"), recursive=True):
        for k, v in e.coff_ehfuncinfo(p).items():
            ours.setdefault(k, set()).add(v)
    out, joined = {}, 0
    for fn, rms in r.items():
        n = a2n.get("0x" + fn[3:].lower())
        if n and n in ours:
            joined += 1
            if min(ours[n]) < rms:
                out[n] = (rms, min(ours[n]))
    return out, {"retail_eh": len(r), "joined": joined}


def sym_arg(side):
    for a in side.get("typed_args") or []:
        if a.get("type") == "Symbol":
            return a.get("value")
    return None


def scan(repo, layers, base_max, min_ratio, survey_path=None, workers=8):
    report = json.load(open(os.path.join(repo, "build", VERSION, "report.json")))
    rp = report.get("provenance", {}).get("diff_config", [])
    ruler = next((x.split("=", 1)[1] for x in rp if x.startswith("functionRelocDiffs=")), "?")
    units = {u["name"]: u for u in json.load(open(os.path.join(repo, "objdiff.json")))["units"]}
    ret = Retail(repo)
    n2a = load_map_inverse(repo)
    spans = load_split_owner(repo)
    aliases_txt = open(os.path.join(repo, "scripts", "symbol_aliases.json")).read()

    todo, pop = [], collections.Counter()
    for u in report["units"]:
        if layers == "game" and not is_game(units.get(u["name"])):
            continue
        rows = {f["name"]: f for f in u.get("functions", [])
                if float(f.get("fuzzy_match_percent", 0)) < 100 and int(f.get("size", 0)) > 0}
        if rows and u["name"] in units:
            todo.append((u["name"], rows))
            pop["rows_sub100"] += len(rows)
            pop["bytes_sub100"] += sum(int(f["size"]) for f in rows.values())

    def work(item):
        uname, rows = item
        diffs = batch_diff(repo, uname, list(rows))
        try:
            bb = base_bodies(repo, units[uname]["base_path"])
        except Exception:
            bb = {}
        return uname, rows, diffs, bb

    sites, hits = [], []
    with concurrent.futures.ThreadPoolExecutor(workers) as ex:
        for uname, rows, diffs, bb in ex.map(work, todo):
            if not bb:
                pop["units_no_base_obj"] += 1
            for d in diffs:
                caller = d["symbol"]
                if caller not in rows:
                    continue
                pop["rows_diffed"] += 1
                ins = d.get("instructions") or []
                charged = [i for i, x in enumerate(ins) if x.get("match_type") != "equal"]
                if not ins:
                    continue
                pop["rows_with_instructions"] += 1
                if not any(x.get("base") for x in ins):
                    pop["rows_unpaired_no_base"] += 1
                    continue
                pop["rows_paired"] += 1
                for i, x in enumerate(ins):
                    t, b = x.get("target") or {}, x.get("base") or {}
                    if t.get("opcode") != "bl" or b.get("opcode") != "bl":
                        continue
                    tn, bn = sym_arg(t), sym_arg(b)
                    if not tn or not bn or bn not in bb or bn.startswith(HELPER_PREFIXES):
                        continue
                    bsize, bcalls = bb[bn]
                    addr = int(tn[3:], 16) if re.fullmatch(r"fn_[0-9A-Fa-f]{8}", tn) else n2a.get(tn)
                    tsize, tsrc, tcalls = None, None, None
                    if addr is not None:
                        tsize, tsrc = ret.size(addr)
                        if tsize:
                            tcalls = ret.calls(addr, tsize)
                    adj = min((abs(c - i) for c in charged), default=None)
                    rec = {
                        "unit": uname, "caller": caller,
                        "caller_size": int(rows[caller]["size"]),
                        "caller_fuzzy": float(rows[caller].get("fuzzy_match_percent", 0)),
                        "idx": i, "match_type": x.get("match_type"), "adj": adj,
                        "base_callee": bn, "base_size": bsize, "base_calls": bcalls,
                        "target_callee": tn,
                        "target_addr": ("0x%08X" % addr) if addr is not None else None,
                        "target_size": tsize, "target_size_src": tsrc,
                        "target_calls": tcalls,
                        "names_agree": tn == bn, "t_placeholder": tn.startswith("fn_"),
                    }
                    sites.append(rec)
                    if is_hit(rec, base_max, min_ratio):
                        rec["alias"] = (bn in aliases_txt) or (tn in aliases_txt)
                        rec["target_owner"] = owner_of(spans, addr)
                        rec["anon_caller"] = caller.startswith("fn_")
                        hits.append(rec)
    if survey_path:
        with open(survey_path, "w") as f:
            json.dump(sites, f)
    return {"ruler": ruler, "population": dict(pop), "n_sites": len(sites),
            "hits": hits, "base_max": base_max, "min_ratio": min_ratio}


def is_hit(rec, base_max, min_ratio):
    return (rec["base_calls"] == 0
            and rec["base_size"] <= base_max
            and rec["target_size"] is not None
            and (rec["target_calls"] or 0) >= 1
            and rec["target_size"] >= min_ratio * rec["base_size"])


# ------------------------------------------------------------- selftest ----
def selftest(repo):
    fails = []
    gi = {"base_calls": 0, "base_size": 20, "target_size": 200, "target_calls": 2}
    if not is_hit(gi, DEFAULT_BASE_MAX, DEFAULT_MIN_RATIO):
        fails.append("GI-shape record did not fire")
    for why, r in [("retail empty too", dict(gi, target_size=20, target_calls=0)),
                   ("retail leaf", dict(gi, target_calls=0)),
                   ("base non-leaf", dict(gi, base_calls=1)),
                   ("base too big", dict(gi, base_size=DEFAULT_BASE_MAX + 4)),
                   ("unresolved retail", dict(gi, target_size=None))]:
        if is_hit(r, DEFAULT_BASE_MAX, DEFAULT_MIN_RATIO):
            fails.append("fired on negative: " + why)
    ret = Retail(repo)
    s, src = ret.size(0x826067C0)
    if s != 0xC8 or src != "pdata":
        fails.append("retail size of 0x826067C0 = %r/%s, want 0xC8/pdata" % (s, src))
    elif ret.calls(0x826067C0, s) < 1:
        fails.append("retail 0x826067C0 decoded as a leaf; it calls GetObj/TriggerEvent")
    s2, _ = ret.size(0x826C3888)
    if not s2 or ret.read(0x826C3888, 4) != b"\x4e\x80\x00\x20" or ret.calls(0x826C3888, s2) != 0:
        fails.append("0x826C3888 (4-byte blr survivor) not decoded as a leaf")
    # our side: the known-leaf and known-non-leaf in the CURRENT BandStorePanel obj
    units = {u["name"]: u for u in json.load(open(os.path.join(repo, "objdiff.json")))["units"]}
    bb = base_bodies(repo, units["default/band3/meta_band/BandStorePanel"]["base_path"])
    h = bb.get("?Handle@BandStorePanel@@UAA?AVDataNode@@PAVDataArray@@_N@Z")
    if not h or h[1] == 0:
        fails.append("our BandStorePanel::Handle read as a leaf: %r" % (h,))
    leafs = [n for n, (sz, c) in bb.items() if c == 0 and sz <= 8]
    if not leafs:
        fails.append("no tiny leaf found in BandStorePanel obj (leaf parse broken?)")
    import importlib.util
    here = os.path.dirname(os.path.abspath(__file__))
    spec = importlib.util.spec_from_file_location("ehs", os.path.join(here, "eh_state_screen.py"))
    e = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(e)
    ms = e.retail_maxstates(e.Image(os.path.join(repo, "orig", VERSION, "band.exe")),
                            os.path.join(repo, "build", VERSION, "asm"))
    if ms.get("fn_82607ED8") != 25:
        fails.append("retail maxState of NESTED-unit fn_82607ED8 = %r, want 25 "
                     "(eh_state_screen glob not recursive?)" % ms.get("fn_82607ED8"))
    for f in fails:
        print("SELFTEST FAIL:", f)
    print("SELFTEST", "FAIL" if fails else "PASS", "(%d checks failed)" % len(fails))
    return 1 if fails else 0


def main():
    global _SELF_BREAK
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--project-dir", default=os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    ap.add_argument("--json")
    ap.add_argument("--survey")
    ap.add_argument("--layers", choices=["game", "all"], default="game")
    ap.add_argument("--base-max", type=int, default=DEFAULT_BASE_MAX)
    ap.add_argument("--min-ratio", type=float, default=DEFAULT_MIN_RATIO)
    ap.add_argument("--expect-hit", action="append", default=[])
    ap.add_argument("--expect-silent", action="append", default=[])
    ap.add_argument("--eh", action="store_true",
                    help="also run the EH-state-deficit pass (inlining-proof complement)")
    ap.add_argument("--expect-eh-hit", action="append", default=[])
    ap.add_argument("--expect-eh-silent", action="append", default=[])
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--self-break", action="store_true")
    a = ap.parse_args()
    repo = os.path.abspath(a.project_dir)
    if a.selftest:
        if a.self_break:
            _SELF_BREAK = True
            rc = selftest(repo)
            print("SELF-BREAK:", "OK (selftest failed as required)" if rc else
                  "VACUOUS (selftest passed under sabotage)")
            return 0 if rc else 4
        return selftest(repo)
    res = scan(repo, a.layers, a.base_max, a.min_ratio, a.survey)
    if len(res["hits"]) == 0 and res["population"].get("rows_paired", 0) < 50:
        print("REFUSED: vacuous scan (%r)" % res["population"])
        return 2
    by_callee = collections.defaultdict(list)
    for h in res["hits"]:
        by_callee[(h["unit"], h["base_callee"])].append(h)
    groups = []
    for (unit, bn), hs in by_callee.items():
        callers = {h["caller"]: h for h in hs}
        groups.append({"unit": unit, "base_callee": bn, "base_size": hs[0]["base_size"],
                       "target_callees": sorted({h["target_callee"] for h in hs}),
                       "target_sizes": sorted({h["target_size"] for h in hs}),
                       "target_owner": sorted({str(h.get("target_owner")) for h in hs}),
                       "alias": any(h["alias"] for h in hs),
                       "anon_callers_only": all(h["anon_caller"] for h in hs),
                       "callers": [{"caller": c, "size": h["caller_size"],
                                    "fuzzy": h["caller_fuzzy"], "adj": h["adj"],
                                    "names_agree": h["names_agree"]}
                                   for c, h in callers.items()],
                       "size_if_crosses": sum(h["caller_size"] for h in callers.values())})
    groups.sort(key=lambda g: -g["size_if_crosses"])
    res["groups"] = groups
    hit_callers = {h["caller"] for h in res["hits"]}
    eh_hits = set()
    if a.eh:
        defs, ehpop = eh_deficits(repo)
        report = json.load(open(os.path.join(repo, "build", VERSION, "report.json")))
        units = {u["name"]: u for u in json.load(open(os.path.join(repo, "objdiff.json")))["units"]}
        eh_rows, at100 = [], 0
        for u in report["units"]:
            if a.layers == "game" and not is_game(units.get(u["name"])):
                continue
            for f in u.get("functions", []):
                if f["name"] in defs:
                    if float(f.get("fuzzy_match_percent", 0)) >= 100:
                        at100 += 1
                        continue
                    rms, oms = defs[f["name"]]
                    eh_rows.append({"unit": u["name"], "caller": f["name"],
                                    "size": int(f.get("size", 0)),
                                    "fuzzy": float(f.get("fuzzy_match_percent", 0)),
                                    "retail_maxstate": rms, "our_maxstate": oms})
        eh_rows.sort(key=lambda r: -r["size"])
        eh_hits = {r["caller"] for r in eh_rows}
        res["eh_deficit_rows"] = eh_rows
        print("EH-DEFICIT pass (ours maxState < retail): %s; in-layer sub-100 rows %d, "
              "in-layer rows at fuzzy 100 carrying a deficit %d" % (json.dumps(ehpop), len(eh_rows), at100))
        for r in eh_rows:
            print("  %6d B fuzzy %7.3f  retail=%d ours=%d  %s" % (
                r["size"], r["fuzzy"], r["retail_maxstate"], r["our_maxstate"], r["caller"][:100]))
    pop = res["population"]
    print("ruler=%s  layers=%s  base_max=%d  min_ratio=%.1f" % (res["ruler"], a.layers, a.base_max, a.min_ratio))
    print("population:", json.dumps(pop))
    print("paired same-obj bl sites examined: %d" % res["n_sites"])
    print("hit sites: %d  hit callers: %d (%.2f%% of %d PAIRED sub-100 rows)  stub groups: %d" % (
        len(res["hits"]), len(hit_callers),
        100.0 * len(hit_callers) / max(1, pop.get("rows_paired", 0)),
        pop.get("rows_paired", 0), len(groups)))
    for g in groups[:40]:
        print("%7d B  %-44s base %3dB -> retail %s B  owner=%s alias=%s anon_only=%s" % (
            g["size_if_crosses"], g["base_callee"][:44], g["base_size"],
            g["target_sizes"], ",".join(g["target_owner"])[:40], g["alias"],
            g["anon_callers_only"]))
        for c in g["callers"]:
            print("           %6d B fuzzy %.3f adj=%s agree=%s  %s" % (
                c["size"], c["fuzzy"], c["adj"], c["names_agree"], c["caller"][:90]))
    if a.json:
        with open(a.json, "w") as f:
            json.dump(res, f, indent=1)
    rc = 0
    for s in a.expect_hit:
        ok = s in hit_callers
        print("EXPECT-HIT %s: %s" % ("PASS" if ok else "FAIL", s))
        rc = rc or (0 if ok else 3)
    for s in a.expect_eh_hit:
        ok = s in eh_hits
        print("EXPECT-EH-HIT %s: %s" % ("PASS" if ok else "FAIL", s))
        rc = rc or (0 if ok else 3)
    for s in a.expect_eh_silent:
        ok = s not in eh_hits
        print("EXPECT-EH-SILENT %s: %s" % ("PASS" if ok else "FAIL", s))
        rc = rc or (0 if ok else 3)
    for s in a.expect_silent:
        ok = s not in hit_callers
        print("EXPECT-SILENT %s: %s" % ("PASS" if ok else "FAIL", s))
        rc = rc or (0 if ok else 3)
    return rc


if __name__ == "__main__":
    sys.exit(main())
