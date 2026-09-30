#!/usr/bin/env python3
"""Adjudicate proposed names for anonymous fn_ rows on RETAIL BYTES.

Lane W16-HF (2026-09-30). Input: a proposals JSON from
tools/anon_candidate_scorer.py (`apply`), e.g.
docs/decomp/W16HD_ANON_PROPOSALS_T75_2026-09-30.json.

A fuzzy score says the SHAPE fits. It does not say the relocations fit the
name. Under name_check the scorer's diff FORGIVES every placeholder target
(`fn_`/`lbl_`), and almost every callee and datum of an anonymous row is a
placeholder. So a high score is compatible with the wrong callee, the wrong
string or the wrong vtable. This tool checks each relocation the name implies
against evidence that does not come from the scorer and not from the proposal:

  string    `??_C@` literal  -> the C string at the retail address
  float     `__real@<hex>`   -> the bytes at the retail address
  vtable    `??_7X@@6B@`     -> retail RTTI (COL -> TypeDescriptor) of that vtable
  binding   any other name   -> the BINDING TABLE: for every already-paired named
                                row that objdiff scores mpn == 100 with equal
                                size, the instructions align 1:1, so the retail
                                placeholder at offset k and our symbol at offset k
                                are the same entity. Majority address per name.
                                (Rows that are themselves proposals are excluded,
                                so the table never witnesses itself.)
  map       target_symbol_map.json placing our callee name at a DIFFERENT address

Plus two checks on the row as a whole:

  callers   the binding table run backwards: which of OUR names do matched
            retail callers call at this address? A matched caller calling the
            runner-up (or anything else) here is a contradiction.
  vslot     for a virtual name, retail RTTI vtables containing the address,
            and the slot our own vtable gives that name.

Every proposal is run alongside its runner-up, from the same objdiff listing
path (scratch rename of fn_<addr> in a copy of the target obj; the scorer's
exact ruler flags).

Usage:
    python3 tools/anon_proposal_adjudicate.py <proposals.json> --json-out <out>
Writes per-name evidence; the verdict column is a MECHANICAL pre-verdict
(CONTRADICTED / SUPPORTED / UNANCHORED). The final accept/refuse also reads
the runner-up's evidence and is recorded in the lane doc.
"""
import argparse
import collections
import json
import os
import re
import struct
import subprocess
import sys
import tempfile
from multiprocessing import Pool
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
os.chdir(ROOT)
sys.path.insert(0, str(ROOT / "scripts"))
sys.path.insert(0, str(ROOT / "tools"))
from obj_target_symbol_renamer import rename_symbols  # noqa: E402

RULER = ["-c", "functionRelocDiffs=name_check", "-c", "combineDataSections=true",
         "-c", "combineTextSections=true", "-c", "ppc.calculatePoolRelocations=false",
         "--map-file", "build/45410914/icf_aliases.map"]
SCRATCH = Path(os.path.expanduser("~/tmp/anon_proposal_adjudicate"))
PLACEHOLDER = re.compile(r"^(fn|lbl|data|rdata|bss|jumptable|vftable)_([0-9A-Fa-f]{8})$")


# ---------------------------------------------------------------- COFF
def coff(path):
    """symbols: list of (name, secidx, value, sclass, type); relocs: {sec: [(off, symname, type)]};
    secdata: {sec: bytes}"""
    d = Path(path).read_bytes()
    _m, nsec, _t, psym, nsym, _o, _c = struct.unpack_from("<HHIIIHH", d, 0)
    strtab = psym + nsym * 18
    names = {}
    syms = []
    i = 0
    while i < nsym:
        o = psym + i * 18
        raw = d[o:o + 8]
        val, sec, typ, scl, naux = struct.unpack_from("<IhHBB", d, o + 8)
        if raw[:4] == b"\0\0\0\0":
            so = struct.unpack_from("<I", raw, 4)[0]
            name = d[strtab + so:d.index(b"\0", strtab + so)].decode("latin1")
        else:
            name = raw.rstrip(b"\0").decode("latin1")
        names[i] = name
        syms.append((name, sec, val, scl, typ))
        i += 1 + naux
    relocs, secdata = {}, {}
    for s in range(nsec):
        o = 20 + s * 40
        _vs, _va, size, ptr, prel, _pl, nrel, _nl, ch = struct.unpack_from("<IIIIIIHHI", d, o + 8)
        secdata[s + 1] = d[ptr:ptr + size] if ptr else b""
        rl = []
        first = 0
        if (ch & 0x01000000) and nrel == 0xFFFF:   # IMAGE_SCN_LNK_NRELOC_OVFL
            nrel = struct.unpack_from("<I", d, prel)[0]
            first = 1
        for k in range(first, nrel):
            va_, si, ty = struct.unpack_from("<IIH", d, prel + k * 10)
            rl.append((va_, names.get(si, "?"), ty))
        relocs[s + 1] = rl
    return syms, relocs, secdata


def function_relocs(parsed, name, size=None):
    """[(rel_off, symname)] for function `name`, bounded by `size` or the next
    symbol at a higher value in the same section."""
    syms, relocs, secdata = parsed
    hit = [s for s in syms if s[0] == name and s[1] > 0]
    if not hit:
        return None
    _n, sec, val, _scl, _typ = hit[0]
    if size is None:
        later = sorted(s[2] for s in syms if s[1] == sec and s[2] > val and s[3] in (2, 3))
        end = later[0] if later else len(secdata[sec])
    else:
        end = val + size
    return sorted((o - val, n) for o, n, _t in relocs.get(sec, []) if val <= o < end)


def defined_cstr(parsed, name):
    syms, _r, secdata = parsed
    for n, sec, val, _s, _t in syms:
        if n == name and sec > 0:
            b = secdata[sec][val:]
            return b.split(b"\0")[0].decode("latin1", "replace")
    return None


# ---------------------------------------------------------------- binding table
def _unit_bindings(args):
    uname, tpath, bpath, rows, exclude = args
    out = []
    try:
        tp, bp = coff(tpath), coff(bpath)
    except Exception:
        return out
    for name, size in rows:
        if name in exclude:
            continue
        tr = function_relocs(tp, name, size)
        br = function_relocs(bp, name)
        if not tr or br is None:
            continue
        bmap = collections.defaultdict(set)
        for o, n in br:
            bmap[o].add(n)
        for o, tn in tr:
            m = PLACEHOLDER.match(tn)
            if not m:
                continue
            for bn in bmap.get(o, ()):
                if bn.startswith("$") or bn.startswith("."):
                    continue
                out.append((bn, int(m.group(2), 16), name))
    return out


def build_bindings(report, cfg, exclude_names, workers=12):
    units = {u["name"]: u for u in cfg["units"]}
    jobs = []
    for u in report["units"]:
        c = units.get(u["name"])
        if not c or not c.get("base_path") or not c.get("target_path"):
            continue
        if not (Path(c["base_path"]).exists() and Path(c["target_path"]).exists()):
            continue
        rows = []
        for f in u.get("functions", []):
            n = f["name"]
            if PLACEHOLDER.match(n) or float(f.get("match_percent_normalized", 0) or 0) != 100.0:
                continue
            rows.append((n, int(f["size"])))
        if rows:
            jobs.append((u["name"], c["target_path"], c["base_path"], rows, exclude_names))
    fwd = collections.defaultdict(collections.Counter)   # our name -> {addr: n}
    rev = collections.defaultdict(collections.Counter)   # addr -> {our name: n}
    with Pool(workers) as pool:
        for res in pool.imap_unordered(_unit_bindings, jobs, chunksize=4):
            for bn, addr, caller in res:
                fwd[bn][addr] += 1
                rev[addr][bn] += 1
    return fwd, rev


# ---------------------------------------------------------------- listings
def _listing(job):
    addr, target, unit_t, unit_b, name = job
    SCRATCH.mkdir(parents=True, exist_ok=True)
    t = bytearray(Path(unit_t).read_bytes())
    n, _ = rename_symbols(t, {target: name})
    fd, tp = tempfile.mkstemp(suffix=".obj", dir=SCRATCH)
    os.write(fd, t)
    os.close(fd)
    try:
        r = subprocess.run(["bin/objdiff-cli", "diff", "-1", tp, "-2", unit_b, name, "-f", "json",
                            "--full-listing", *RULER], capture_output=True, timeout=180)
        o = json.loads(r.stdout) if r.returncode == 0 else {"error": r.stderr.decode()[-300:]}
    finally:
        os.unlink(tp)
    o["_renamed"] = n
    return addr, name, o


def sym_pairs(listing):
    """[(match_type, target_sym|None, base_sym|None, t_opcode, b_opcode)]"""
    out = []
    for r in listing.get("instructions", []):
        t = [a["value"] for a in r.get("target", {}).get("typed_args", []) if a["type"] == "Symbol"]
        b = [a["value"] for a in r.get("base", {}).get("typed_args", []) if a["type"] == "Symbol"]
        to, bo = r.get("target", {}).get("opcode"), r.get("base", {}).get("opcode")
        for i in range(max(len(t), len(b))):
            out.append((r["match_type"], t[i] if i < len(t) else None, b[i] if i < len(b) else None, to, bo))
    return out


# ---------------------------------------------------------------- retail
class Retail:
    def __init__(self):
        from retail_rtti import RetailRtti
        self.R = RetailRtti()

    def cstr(self, va):
        return self.R.cstr(va)

    def raw(self, va, n):
        out = bytearray()
        for k in range(0, n, 4):
            w = self.R.u32(va + k)
            if w is None:
                return None
            out += struct.pack(">I", w)
        return bytes(out[:n])

    def vt_class(self, va):
        try:
            return self.R.class_of_vtable(va)
        except Exception:
            return None

    def owners(self, va):
        try:
            return self.R.owning_vtables(va)
        except Exception:
            return []


SAVE_HELPER = re.compile(r"^__(save|rest)(gpr|fpr|vmx)")
JUNK = re.compile(r"^(@comp\.id|@feat|\$|\.)")


def vt_name_class(n):
    m = re.match(r"^\?\?_7(.+?)@@6B", n)
    return m.group(1) if m else None


def rtti_short(c):
    if not c:
        return None
    m = re.match(r"^\.\?A[VU](.+?)@@$", c)
    return m.group(1) if m else c


def side_syms(listing, side):
    """set of (is_branch, symbol) referenced by one side, save/restore helpers dropped."""
    out = set()
    for r in listing.get("instructions", []):
        s = r.get(side)
        if not s:
            continue
        for a in s.get("typed_args", []):
            if a["type"] == "Symbol" and not SAVE_HELPER.match(a["value"]) and not JUNK.match(a["value"]):
                out.add((s.get("opcode") in ("bl", "b"), a["value"]))
    return out


# -- fold-twin test: our symbol b vs retail's named survivor at its map address --
_BASE_INDEX = {}


def base_index(cfg):
    if _BASE_INDEX:
        return _BASE_INDEX
    paths = sorted({u["base_path"] for u in cfg["units"] if u.get("base_path") and Path(u["base_path"]).exists()})
    with Pool(12) as pool:
        for path, names in pool.imap_unordered(_obj_fn_names, paths, chunksize=8):
            for n in names:
                _BASE_INDEX.setdefault(n, path)
    return _BASE_INDEX


def _obj_fn_names(path):
    try:
        syms, _r, _d = coff(path)
    except Exception:
        return path, []
    return path, [s[0] for s in syms if s[1] > 0 and s[4] == 0x20 and s[3] in (2, 3)]


def fold_twin(n, b, ctx):
    """True/False/None: is our body of b reloc-masked identical to retail's body at n's map address?"""
    addrs = ctx["smap_rev"].get(n, [])
    path = base_index(ctx["cfg"]).get(b)
    if not addrs or not path:
        return None
    parsed = ctx["pcache"].setdefault(path, coff(path))
    syms, relocs, secdata = parsed
    hit = [s for s in syms if s[0] == b and s[1] > 0]
    if not hit:
        return None
    _n, sec, val, _scl, _t = hit[0]
    later = sorted(s[2] for s in syms if s[1] == sec and s[2] > val and s[3] in (2, 3))
    end = later[0] if later else len(secdata[sec])
    body = secdata[sec][val:end]
    roffs = {o - val for o, _nm, _ty in relocs.get(sec, []) if val <= o < end}
    for a in addrs:
        ext = ctx["retail"].R.function_extent(a)
        if ext is None:
            ext = len(body)          # leaf stubs carry no .pdata; compare our extent
        if ext != len(body):
            continue
        rb = ctx["retail"].raw(a, ext)
        if rb is None:
            continue
        ok = True
        for k in range(0, len(body) // 4 * 4, 4):
            w1 = struct.unpack_from(">I", body, k)[0]
            w2 = struct.unpack_from(">I", rb, k)[0]
            if (k & ~3) in roffs or (k + 2) in roffs:
                m = 0xFC000000 if (w1 >> 26) == 18 else 0xFFFF0000
                if (w1 & m) != (w2 & m):
                    ok = False
                    break
            elif w1 != w2:
                ok = False
                break
        if ok:
            return True
    return False


def our_identity(b, ctx):
    if b.startswith("??_C@"):
        s = defined_cstr(ctx["pbase"], b)
        return ("str", s) if s is not None else ("name", b)
    if b.startswith("__real@"):
        return ("real", b[len("__real@"):].lower())
    vc = vt_name_class(b)
    if vc:
        return ("vt", vc)
    return ("name", b)


def retail_identities(t, ctx):
    """All identities a retail placeholder could carry, from retail bytes / independent witnesses."""
    m = PLACEHOLDER.match(t)
    if not m:
        ids = {("name", t)}
        if t.startswith("??_C@"):               # a map-named string: its content too
            for a_ in ctx["smap_rev"].get(t, []):
                s_ = ctx["retail"].cstr(a_)
                if s_ is not None:
                    ids.add(("str", s_))
        return ids, "named"
    addr = int(m.group(2), 16)
    R = ctx["retail"]
    ids = set()
    if t.startswith(("lbl_", "rdata_", "data_", "vftable_")):
        s = R.cstr(addr)
        if s is not None and len(s) >= 1 and all(32 <= ord(ch) < 127 for ch in s):
            ids.add(("str", s))
        vc = rtti_short(R.vt_class(addr))
        if vc:
            ids.add(("vt", vc))
        for n in (4, 8):
            raw = R.raw(addr, n)
            if raw:
                ids.add(("real", raw.hex()))
    if addr in ctx["smap"]:
        ids.add(("name", ctx["smap"][addr]))
    if addr in ctx["props"] and t.startswith("fn_"):
        ids.add(("name", ctx["props"][addr]))
    for n in ctx["rev"].get(addr, {}):
        if not JUNK.match(n):
            ids.add(("name", n))
    return ids, ("bound" if any(k == "name" for k, _ in ids) else "content")


def evaluate(addr, name, listing, ctx):
    ev = collections.Counter()
    items = []
    R_ = side_syms(listing, "target")
    B_ = side_syms(listing, "base")
    ours = {}
    for br, b in B_:
        ours.setdefault(our_identity(b, ctx), b)
        ours.setdefault(("name", b), b)          # a retail-NAMED string/float/vtable matches by name too
    # objdiff pairs whose names differ but whose row is `equal`: icf_aliases.map folds
    aliased = collections.defaultdict(set)
    for r in listing.get("instructions", []):
        if r["match_type"] != "equal":
            continue
        ts = [x["value"] for x in r.get("target", {}).get("typed_args", []) if x["type"] == "Symbol"]
        bs = [x["value"] for x in r.get("base", {}).get("typed_args", []) if x["type"] == "Symbol"]
        for t_, b_ in zip(ts, bs):
            # a placeholder target is FORGIVEN by name_check, so an `equal` row proves nothing
            # about it -- only a retail-NAMED symbol reaching `equal` is an icf_aliases.map fold
            if t_ != b_ and not PLACEHOLDER.match(t_):
                aliased[t_].add(b_)
    used = set()
    for br, t in sorted(R_):
        al = [b for b in aliased.get(t, ()) if ("name", b) in ours]
        if al:
            used.update(("name", b) for b in al)
            items.append(("FOLD", f"retail {t[:60]} ~ ours {al[0][:60]} (icf_aliases.map)"))
            ev["FOLD"] += 1
            continue
        ids, how = retail_identities(t, ctx)
        hit = [i for i in ids if i in ours]
        kindword = "callee" if br else "data"
        if hit:
            used.update(hit)
            items.append(("AGREE", f"{kindword} {hit[0][0]}: {str(hit[0][1])[:90]}"))
            ev["AGREE"] += 1
            continue
        if how == "named" and br:
            twins = [b for (k, v), b in ours.items() if k == "name" and (k, v) not in used and fold_twin(t, b, ctx)]
            if twins:
                used.add(("name", twins[0]))
                items.append(("FOLD", f"retail {t[:60]} ~ ours {twins[0][:60]} (reloc-masked identical)"))
                ev["FOLD"] += 1
                continue
        if any(k in ("str", "vt", "name") for k, _ in ids):
            desc = "; ".join(f"{k}:{str(v)[:70]}" for k, v in sorted(ids, key=str) if k != "real")
            items.append(("RETAIL_ONLY", f"{kindword} {t}: {desc[:200]}"))
            ev["RETAIL_ONLY"] += 1
        else:
            items.append(("UNVER", f"{kindword} {t} unidentified"))
            ev["UNVER"] += 1
    # every one of our symbols whose identity was consumed (two vtables of one class share a key)
    used_syms = {b for _br, b in B_ if our_identity(b, ctx) in used or ("name", b) in used}
    # Symbol-literal equivalence: retail builds Symbol("S") in place (string + ??0Symbol ctor),
    # our source references a global/static `?S@...VSymbol@@A`. Same identity, different spelling.
    sym_globals = {}
    for _br, b in B_:
        m = re.match(r"^\?([A-Za-z_0-9]+)@.*VSymbol@@A$", b)
        if m and b not in used_syms:
            sym_globals.setdefault(m.group(1), b)
    eq_hits = 0
    for j, (kind, det) in enumerate(items):
        if kind != "RETAIL_ONLY" or " str:" not in det:
            continue
        strs = re.findall(r"str:([^;]+)", det)
        for st in strs:
            st = st.strip()
            if st in sym_globals:
                items[j] = ("AGREE", f"symbol-literal {st!r} ~ ours {sym_globals[st][:60]}")
                ev["RETAIL_ONLY"] -= 1
                ev["AGREE"] += 1
                used_syms.add(sym_globals.pop(st))
                eq_hits += 1
                break
    if eq_hits:
        for j, (kind, det) in enumerate(items):
            if kind == "RETAIL_ONLY" and "??0Symbol@@QAA@PBD@Z" in det:
                items[j] = ("AGREE", "callee ??0Symbol@@QAA@PBD@Z explained by symbol-literal construction")
                ev["RETAIL_ONLY"] -= 1
                ev["AGREE"] += 1
    reported = set()
    for (k, v), b in ours.items():
        if (k, v) in used or b in used_syms or b in reported:
            continue
        reported.add(b)
        k, v = our_identity(b, ctx)
        if k in ("str", "vt"):
            items.append(("OURS_ONLY", f"{k}: {str(v)[:90]}"))
            ev["OURS_ONLY"] += 1
            continue
        if k == "real":
            items.append(("UNVER", f"our float {v} unmatched"))
            ev["UNVER"] += 1
            continue
        placed = set(ctx["smap_rev"].get(v, [])) | {a for a, c in ctx["fwd"].get(v, {}).items()}
        if placed:
            items.append(("OURS_ONLY", f"{v[:80]} identified at {','.join(hex(a) for a in sorted(placed)[:3])}, not referenced by retail here"))
            ev["OURS_ONLY"] += 1
        else:
            items.append(("UNVER", f"ours {v[:80]} (no retail address known)"))
            ev["UNVER"] += 1
    callers = {k: v for k, v in ctx["rev"].get(addr, {}).items() if not JUNK.match(k)}
    if name in callers:
        ev["CALLER_AGREE"] += callers[name]
    others = {k: v for k, v in callers.items() if k != name}
    if others:
        ev["CALLER_CONTRA"] += sum(others.values())
        items.append(("CALLER_CONTRA", f"matched callers call {others}"))
    elsewhere = {hex(a): n for a, n in (ctx["fwd"].get(name) or {}).items() if a != addr}
    if elsewhere:
        ev["NAME_BOUND_ELSEWHERE"] += sum(elsewhere.values())
        items.append(("NAME_BOUND_ELSEWHERE", str(elsewhere)))
    mapped_else = [hex(a) for a in ctx["smap_rev"].get(name, []) if a != addr]
    if mapped_else:
        ev["NAME_MAPPED_ELSEWHERE"] += 1
        items.append(("NAME_MAPPED_ELSEWHERE", str(mapped_else)))
    return {"name": name, "fuzzy": listing.get("fuzzy_match_percent"),
            "counts": dict(ev), "items": items, "callers": callers}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("proposals")
    ap.add_argument("--json-out", required=True)
    ap.add_argument("--workers", type=int, default=12)
    ap.add_argument("--independent", action="store_true",
                    help="do NOT resolve a retail fn_ placeholder through ANOTHER proposal's name; every verdict "
                         "must then stand on map/binding/content evidence alone")
    ap.add_argument("--control", type=int, default=0,
                    help="instead of adjudicating, run N already-paired named rows (fuzzy 75-99.99, same units) "
                         "through the same evaluation under their TRUE name and under a closest-size WRONG sibling")
    a = ap.parse_args()
    P = json.loads(Path(a.proposals).read_text())["proposals"]
    if a.control:
        return run_control(a, P)
    report = json.loads(Path("build/45410914/report.json").read_text())
    cfg = json.loads(Path("objdiff.json").read_text())
    units = {u["name"]: u for u in cfg["units"]}
    smap = {int(k, 16): v for k, v in json.loads(Path("scripts/target_symbol_map.json").read_text()).items()
            if k.startswith("0x") and v}
    smap_rev = collections.defaultdict(list)
    for k, v in smap.items():
        smap_rev[v].append(k)
    props = {int(p["addr"], 16): p["name"] for p in P}
    exclude = set(props.values()) | {p["runner_up"][0] for p in P if p["runner_up"]}
    print(f"building binding table (excluding {len(exclude)} proposal/runner-up names)...", file=sys.stderr)
    fwd, rev = build_bindings(report, cfg, exclude, a.workers)
    print(f"  {len(fwd)} bound names, {sum(sum(c.values()) for c in fwd.values())} witnesses", file=sys.stderr)
    jobs = []
    for p in P:
        u = units[p["unit"]]
        jobs.append((p["addr"], p["target"], u["target_path"], u["base_path"], p["name"]))
        if p["runner_up"]:
            jobs.append((p["addr"], p["target"], u["target_path"], u["base_path"], p["runner_up"][0]))
    with Pool(a.workers) as pool:
        L = {(ad, nm): o for ad, nm, o in pool.map(_listing, jobs)}
    retail = Retail()
    pcache = {}
    out = []
    for p in P:
        u = units[p["unit"]]
        pb = pcache.setdefault(u["base_path"], coff(u["base_path"]))
        ctx = {"fwd": fwd, "rev": rev, "smap": smap, "smap_rev": smap_rev, "retail": retail, "pbase": pb,
               "props": {} if a.independent else props, "cfg": cfg, "pcache": pcache}
        addr = int(p["addr"], 16)
        rec = {"addr": p["addr"], "unit": p["unit"], "size": p["size"],
               "prop": evaluate(addr, p["name"], L[(p["addr"], p["name"])], ctx)}
        if p["runner_up"]:
            rec["ru"] = evaluate(addr, p["runner_up"][0], L[(p["addr"], p["runner_up"][0])], ctx)
        rec["owners"] = [list(x) for x in retail.owners(addr)]
        c = rec["prop"]["counts"]
        contra = sum(c.get(k, 0) for k in ("RETAIL_ONLY", "OURS_ONLY", "CALLER_CONTRA",
                                             "NAME_BOUND_ELSEWHERE", "NAME_MAPPED_ELSEWHERE"))
        support = c.get("AGREE", 0) + c.get("CALLER_AGREE", 0) + c.get("FOLD", 0)
        rec["pre_verdict"] = "CONTRADICTED" if contra else ("SUPPORTED" if support else "UNANCHORED")
        out.append(rec)
    Path(a.json_out).write_text(json.dumps(out, indent=1))
    tally = collections.Counter(r["pre_verdict"] for r in out)
    print(dict(tally))


def _listing_named(job):
    """control: diff `name` against a target copy in which `true` is renamed to `name` (identity if equal)."""
    addr, true, unit_t, unit_b, name = job
    SCRATCH.mkdir(parents=True, exist_ok=True)
    t = bytearray(Path(unit_t).read_bytes())
    n = 1
    if name != true:
        # the wrong sibling is itself a paired row in this target obj: move it out of the
        # way FIRST, or objdiff pairs `name` with the sibling's own row (a vacuous leg)
        rename_symbols(t, {name: "zz_ctrl_displaced_" + str(abs(hash(name)))})
        n, _ = rename_symbols(t, {true: name})
    fd, tp = tempfile.mkstemp(suffix=".obj", dir=SCRATCH)
    os.write(fd, t)
    os.close(fd)
    try:
        r = subprocess.run(["bin/objdiff-cli", "diff", "-1", tp, "-2", unit_b, name, "-f", "json",
                            "--full-listing", *RULER], capture_output=True, timeout=180)
        o = json.loads(r.stdout) if r.returncode == 0 else {"error": r.stderr.decode()[-300:]}
    finally:
        os.unlink(tp)
    o["_renamed"] = n
    return addr, true, name, o


def run_control(a, P):
    import random
    report = json.loads(Path("build/45410914/report.json").read_text())
    cfg = json.loads(Path("objdiff.json").read_text())
    units = {u["name"]: u for u in cfg["units"]}
    smap = {int(k, 16): v for k, v in json.loads(Path("scripts/target_symbol_map.json").read_text()).items()
            if k.startswith("0x") and v}
    smap_rev = collections.defaultdict(list)
    for k, v in smap.items():
        smap_rev[v].append(k)
    punits = sorted({p["unit"] for p in P})
    pool_rows = []
    for u in report["units"]:
        if u["name"] not in punits:
            continue
        rows = [f for f in u.get("functions", []) if not PLACEHOLDER.match(f["name"])]
        base = units[u["name"]]["base_path"]
        ours = coff(base)
        defined = {s_[0] for s_ in ours[0] if s_[1] > 0 and s_[4] == 0x20}
        named = [(f["name"], int(f["size"])) for f in rows if f["name"] in defined]
        for f in rows:
            fz = float(f.get("fuzzy_match_percent", 0) or 0)
            if not (75.0 <= fz < 100.0) or f["name"] not in defined or f["name"] not in smap_rev:
                continue
            sz = int(f["size"])
            sib = sorted((abs(s2 - sz), n2) for n2, s2 in named if n2 != f["name"] and n2 not in smap_rev.get(f["name"], []))
            if sib:
                pool_rows.append((u["name"], f["name"], smap_rev[f["name"]][0], sz, sib[0][1]))
    random.Random(20260930).shuffle(pool_rows)
    rows = pool_rows[:a.control]
    exclude = {r[1] for r in rows} | {r[4] for r in rows}
    fwd, rev = build_bindings(report, cfg, exclude, a.workers)
    jobs = []
    for un, true, addr, sz, wrong in rows:
        u = units[un]
        jobs.append((addr, true, u["target_path"], u["base_path"], true))
        jobs.append((addr, true, u["target_path"], u["base_path"], wrong))
    with Pool(a.workers) as pool:
        L = {(ad, nm): o for ad, _t, nm, o in pool.map(_listing_named, jobs)}
    retail = Retail()
    pcache = {}
    res = {"TRUE": collections.Counter(), "WRONG": collections.Counter()}
    detail = []
    NEG = ("RETAIL_ONLY", "OURS_ONLY", "CALLER_CONTRA", "NAME_BOUND_ELSEWHERE", "NAME_MAPPED_ELSEWHERE")
    for un, true, addr, sz, wrong in rows:
        u = units[un]
        pb = pcache.setdefault(u["base_path"], coff(u["base_path"]))
        # A real proposal is an UNCLAIMED name, so NAME_MAPPED_ELSEWHERE can never fire on one. The
        # control's wrong sibling, though, is by construction mapped at its OWN address, so leaving
        # its row in smap_rev made that negative fire on 100% of WRONG rows -- a rejection the real
        # run can never produce (found 2026-09-30 by the W17 session; the "200/200 WRONG rejected"
        # it printed was vacuous). Hide both names' own rows so both legs look like real proposals.
        # NAME_BOUND_ELSEWHERE is already neutralised by `exclude` in build_bindings above.
        ctl_rev = collections.defaultdict(list, {k: v for k, v in smap_rev.items() if k not in (true, wrong)})
        ctx = {"fwd": fwd, "rev": rev, "smap": smap, "smap_rev": ctl_rev, "retail": retail, "pbase": pb,
               "props": {}, "cfg": cfg, "pcache": pcache}
        for leg, nm in (("TRUE", true), ("WRONG", wrong)):
            o = L[(addr, nm)]
            if "error" in o or o.get("_renamed") != 1:
                res[leg]["error"] += 1
                continue
            e = evaluate(addr, nm, o, ctx)
            c = e["counts"]
            neg = sum(c.get(k, 0) for k in NEG)
            pos = c.get("AGREE", 0) + c.get("FOLD", 0) + c.get("CALLER_AGREE", 0)
            res[leg]["n"] += 1
            res[leg]["neg>0"] += neg > 0
            # the real run's pre_verdict: SUPPORTED iff no negative and some support
            res[leg]["accepted(SUPPORTED)"] += (neg == 0 and pos > 0)
            res[leg]["pos>neg"] += pos > neg
            res[leg]["pos>=2*neg_and_pos>0"] += (pos > 0 and pos >= 2 * neg)
            res[leg]["strong_neg"] += any(k in ("OURS_ONLY",) or (k == "RETAIL_ONLY" and (d.split()[1] if len(d.split()) > 1 else "").startswith("lbl_") and ("str:" in d or "vt:" in d))
                                          for k, d in e["items"])
            detail.append({"leg": leg, "unit": un, "addr": hex(addr), "name": nm, "fuzzy": e["fuzzy"], "counts": c,
                           "neg_items": [x for x in e["items"] if x[0] in NEG]})
    Path(a.json_out).write_text(json.dumps({"summary": {k: dict(v) for k, v in res.items()}, "rows": detail}, indent=1))
    for leg in ("TRUE", "WRONG"):
        print(leg, dict(res[leg]))


if __name__ == "__main__":
    main()
