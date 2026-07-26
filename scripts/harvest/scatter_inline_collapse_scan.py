#!/usr/bin/env python3
"""Scanner for the **scatter-include inlining-policy collapse**.

Background (docs/plans/funclet-cascade-lever-2026-07-25.md §15)
--------------------------------------------------------------
When a host TU ``H.cpp`` scatter-includes a guest ``G.cpp`` (our technique for
pairing retail's scattered COMDATs), every non-inline function defined in
``H.cpp`` becomes *visible* to ``G.cpp``'s code.  Retail built ``G.cpp`` as its
own TU, where those functions are opaque.  Two things follow that retail cannot
do:

1. ``/Ob2`` **inlines** the host function into the guest's callers.
2. The now-local callee is provably nothrow, so MSVC **deletes the callers'
   EH cleanup funclets outright** together with the EH-state spills.

Retail therefore *has* funclets that we do not emit at all, and the guest
functions become permanently unmatchable.  ``__declspec(noinline)`` is not a
fix: it stops (1) but not (2).

What this scanner measures
--------------------------
For every scatter-include edge it compares, per guest function:

* **target funclet count** — exact, from the retail PE's ``_s_FuncInfo`` EH maps
  (``funclet_cascade_rank.parse_eh``), screened to r12-frame funclets.
* **base funclet count** — exact, from *our* compiled host ``.obj``:  MSVC emits
  one ``__unwindtable$<mangled>`` COMDAT per EH function whose relocations name
  that function's ``__unwind$N`` / ``__catch$N`` funclets.

``base < target`` on a function that came from a scatter-included guest is the
collapse fingerprint.  The scanner also reports how many of the missing funclets
are currently un-banked (``match_percent_normalized != 100``), which is the real
opportunity size, and a ``bl``-count delta as a corroborating inlining signal.

Usage
-----
    venv/bin/python scripts/harvest/scatter_inline_collapse_scan.py \
        --repo . --json ~/tmp/laneP/collapse.json --top 40

Requires a built tree (``build/45410914/report.json`` + ``build/45410914/src``)
and ``orig/45410914/band.exe``.
"""

from __future__ import annotations

import argparse
import json
import re
import struct
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from funclet_cascade_rank import (  # noqa: E402
    PE,
    Coff,
    load_report,
    load_symbol_map,
    parse_eh,
    parse_pdata,
    parse_splits,
    prologue_screen,
    report_name,
    span_unit,
    unit_function_vas,
    unit_report_key,
    unit_text_spans,
)

INCLUDE_RX = re.compile(r'^\s*#\s*include\s+"([^"]+\.cpp)"')

# Symbols in a COFF that are not user functions.
NOISE_PREFIX = ("__unwind$", "__catch$", "__unwindtable$", "__catchsym$", "$", ".")


# --------------------------------------------------------------------- edges
def resolve_guest(repo: Path, inc: str) -> str | None:
    """Resolve an #include "x/y.cpp" against the project include order."""
    for base in ("src", "src/system", "src/xdk/LIBCMT", "src/system/stlport"):
        p = repo / base / inc
        if p.is_file():
            return str(p.relative_to(repo))
    # last resort: unique basename match anywhere under src/
    hits = sorted((repo / "src").rglob(Path(inc).name))
    if len(hits) == 1:
        return str(hits[0].relative_to(repo))
    return None


def find_edges(repo: Path) -> list[dict]:
    out = []
    for p in sorted((repo / "src").rglob("*.cpp")):
        try:
            lines = p.read_text(errors="replace").splitlines()
        except OSError:
            continue
        for i, line in enumerate(lines, 1):
            m = INCLUDE_RX.match(line)
            if not m:
                continue
            host = str(p.relative_to(repo))
            out.append(
                {
                    "host": host,
                    "line": i,
                    "include": m.group(1),
                    "guest": resolve_guest(repo, m.group(1)),
                }
            )
    return out


# ----------------------------------------------------------------- coff view
def obj_path(objdir: Path, source: str) -> Path:
    rel = source[4:] if source.startswith("src/") else source
    return (objdir / rel).with_suffix(".obj")


class ObjView:
    """Function symbols + per-function funclet counts for one compiled obj."""

    def __init__(self, path: Path):
        self.path = path
        self.coff = Coff(path)
        self.funcs: set[str] = set()
        for name, (_val, sec) in self.coff.syms.items():
            if sec > 0 and not name.startswith(NOISE_PREFIX):
                self.funcs.add(name)

    def funclets(self, sym: str) -> int | None:
        """Funclets our compiler emitted for ``sym``; None if it has no EH data."""
        ent = self.coff.syms.get("__unwindtable$" + sym)
        if not ent:
            return None
        _val, sec = ent
        if sec <= 0 or sec > len(self.coff.secs):
            return None
        size = self.coff.secs[sec - 1]["size"]
        names = set(self.coff.reloc_syms(sec, 0, size).values())
        return sum(
            1 for n in names if n.startswith("__unwind$") or n.startswith("__catch$")
        )

    def bl_count(self, sym: str) -> int | None:
        words, _sec, _val = self.coff.func_words(sym, n=4096)
        if not words:
            return None
        return sum(1 for w in words if (w >> 26) == 18 and (w & 1))


# --------------------------------------------------------------- name scopes
MANGLED_SCOPE_RX = re.compile(r"^\?\??\$?[^@]*@([^@]+)@")


def scope_of(mangled: str) -> str | None:
    m = MANGLED_SCOPE_RX.match(mangled)
    return m.group(1) if m else None


def source_scopes(path: Path) -> set[str]:
    """Fallback attribution: class/struct scopes textually defined in a .cpp."""
    out: set[str] = set()
    try:
        text = path.read_text(errors="replace")
    except OSError:
        return out
    for m in re.finditer(r"^[^\s#/].*?\b(\w+)\s*::\s*~?\w+\s*\(", text, re.M):
        out.add(m.group(1))
    return out


# -------------------------------------------------------------------- driver
def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--repo", default=str(Path(__file__).resolve().parents[2]))
    ap.add_argument("--exe", default=None)
    ap.add_argument("--json", help="write full rows here")
    ap.add_argument("--top", type=int, default=40)
    ap.add_argument(
        "--all-owners",
        action="store_true",
        help="also report host-owned parents (default: guest-owned only)",
    )
    args = ap.parse_args()

    repo = Path(args.repo).resolve()
    objdir = repo / "build/45410914/src"
    exe = Path(args.exe) if args.exe else repo / "orig/45410914/band.exe"

    edges = find_edges(repo)
    hosts: dict[str, list[str]] = defaultdict(list)
    for e in edges:
        if e["guest"]:
            hosts[e["host"]].append(e["guest"])
    # splits.txt keys units by BASENAME; several source paths can share one.
    by_base: dict[str, list[str]] = defaultdict(list)
    for h in hosts:
        by_base[Path(h).name].append(h)

    # ---- guest symbol sets (exact when the guest is also its own wired TU)
    guest_syms: dict[str, set[str]] = {}
    guest_scopes: dict[str, set[str]] = {}
    guest_exact: dict[str, bool] = {}
    for g in {g for gs in hosts.values() for g in gs}:
        gp = obj_path(objdir, g)
        if gp.exists():
            try:
                guest_syms[g] = ObjView(gp).funcs
                guest_exact[g] = True
            except Exception:
                guest_syms[g] = set()
                guest_exact[g] = False
        else:
            guest_syms[g] = set()
            guest_exact[g] = False
        guest_scopes[g] = source_scopes(repo / g)

    # ---- retail ground truth
    pe = PE(exe)
    funcs = parse_pdata(pe)
    funclets_of, _parent_of, _ehstats = parse_eh(pe, funcs)
    screened = prologue_screen(pe, funcs)

    units = parse_splits(repo / "config/45410914/splits.txt")
    va2unit, _ = unit_function_vas(pe, units)
    spans = unit_text_spans(units)
    match, _srcmap = load_report(repo / "build/45410914/report.json")
    report_units = {u for (u, _n) in match}
    symmap = load_symbol_map(repo / "scripts/target_symbol_map.json")

    def unit_cpp(va: int) -> str | None:
        return va2unit.get(va) or span_unit(spans, va)

    objcache: dict[str, ObjView | None] = {}

    def host_view(host: str) -> ObjView | None:
        if host not in objcache:
            p = obj_path(objdir, host)
            try:
                objcache[host] = ObjView(p) if p.exists() else None
            except Exception:
                objcache[host] = None
        return objcache[host]

    cov = defaultdict(int)
    rows = []
    for parent, kids in funclets_of.items():
        kid_vas = [k for _kind, k in kids if k in screened]
        if not kid_vas:
            continue
        cov["eh_parents_total"] += 1
        unit = unit_cpp(parent)
        cands = by_base.get(Path(unit).name, []) if unit else []
        if not cands:
            continue
        cov["in_scatter_host_span"] += 1
        cov["in_scatter_host_span_funclets"] += len(kid_vas)
        name = symmap.get("0x%08x" % parent)
        if not name:
            cov["unnamed"] += 1
            cov["unnamed_funclets"] += len(kid_vas)
            continue
        cov["named"] += 1

        # disambiguate a shared basename by which host obj defines the symbol
        host, hv = None, None
        for h in cands:
            v = host_view(h)
            if v is not None and name in v.funcs:
                host, hv = h, v
                break
        if host is None:
            host = cands[0]
            hv = host_view(host)
        if hv is None:
            continue

        # which guest (if any) defines this symbol?
        owner, how = None, None
        for g in hosts[host]:
            if name in guest_syms[g]:
                owner, how = g, "obj"
                break
        if owner is None:
            sc = scope_of(name)
            if sc:
                for g in hosts[host]:
                    if sc in guest_scopes[g]:
                        owner, how = g, "scope"
                        break
        if owner is not None:
            cov["guest_attributed"] += 1
        else:
            cov["host_owned"] += 1
        if owner is None and not args.all_owners:
            continue

        have_body = name in hv.funcs
        base_fl = hv.funclets(name)
        base_fl = 0 if base_fl is None else base_fl
        tgt_fl = len(kid_vas)

        ukey = unit_report_key(unit, report_units)
        pmatch = match.get((ukey, report_name(symmap, parent))) if ukey else None
        unbanked = 0
        for k in kid_vas:
            u = unit_cpp(k)
            uk = unit_report_key(u, report_units) if u else None
            mv = match.get((uk, report_name(symmap, k))) if uk else None
            if mv != 100.0:
                unbanked += 1

        tgt_bl = sum(
            1
            for w in (pe.words(parent, funcs[parent]["size"] // 4) or [])
            if (w >> 26) == 18 and (w & 1)
        )
        base_bl = hv.bl_count(name) if have_body else None

        rows.append(
            {
                "parent_va": "0x%08x" % parent,
                "name": name,
                "host": host,
                "guest": owner,
                "attribution": how,
                "have_body": have_body,
                "target_funclets": tgt_fl,
                "base_funclets": base_fl,
                "missing_funclets": tgt_fl - base_fl,
                "unbanked_funclets": unbanked,
                "parent_match": pmatch,
                "target_bl": tgt_bl,
                "base_bl": base_bl,
                "bl_delta": (tgt_bl - base_bl) if base_bl is not None else None,
            }
        )

    rows.sort(key=lambda r: (-r["missing_funclets"], -r["unbanked_funclets"]))
    collapse = [r for r in rows if r["have_body"] and r["missing_funclets"] > 0]
    # SHARP fingerprint: the §15 mechanism predicts BOTH halves at once —
    # every funclet deleted (base==0) AND a call inlined away (target has more
    # `bl` than we emit).  A parent that merely has fewer funclets while WE
    # emit more calls is ordinary body divergence, not this lever.
    for r in rows:
        r["sharp"] = bool(
            r["have_body"]
            and r["base_funclets"] == 0
            and r["target_funclets"] > 0
            and (r["bl_delta"] or 0) > 0
        )
    sharp = [r for r in rows if r["sharp"]]

    # ------------------------------------------------------------- reporting
    print("scatter-include edges: %d (%d resolved, %d hosts)"
          % (len(edges), sum(1 for e in edges if e["guest"]), len(hosts)))
    print("guest symbol sets: %d exact (own obj) / %d scope-fallback"
          % (sum(guest_exact.values()), len(guest_exact) - sum(guest_exact.values())))
    print()
    print("EH parents inside a scatter-include host span, guest-attributed: %d"
          % len(rows))
    print("  ... with a compiled body:            %d"
          % sum(1 for r in rows if r["have_body"]))
    print("  ... COLLAPSE (base funclets < target): %d parents, %d missing funclets"
          % (len(collapse), sum(r["missing_funclets"] for r in collapse)))
    print("  ... of those, un-banked funclets:    %d"
          % sum(r["unbanked_funclets"] for r in collapse))
    print("  ... parents themselves not strict:   %d"
          % sum(1 for r in collapse if r["parent_match"] != 100.0))
    print("  ... TOTAL blocked functions (parent+funclet): %d"
          % (sum(1 for r in collapse if r["parent_match"] != 100.0)
             + sum(r["unbanked_funclets"] for r in collapse)))

    per_edge: dict[tuple[str, str], dict] = defaultdict(
        lambda: {"parents": 0, "missing": 0, "unbanked": 0}
    )
    for r in collapse:
        d = per_edge[(r["host"], r["guest"])]
        d["parents"] += 1
        d["missing"] += r["missing_funclets"]
        d["unbanked"] += r["unbanked_funclets"]
    print("\ncoverage census (why a parent is or is not visible to this scan):")
    for k in ("eh_parents_total", "in_scatter_host_span",
              "in_scatter_host_span_funclets", "named", "unnamed",
              "unnamed_funclets", "guest_attributed", "host_owned"):
        print("  %-32s %6d" % (k, cov[k]))

    print("\naffected edges: %d of %d resolved\n" % (len(per_edge), len(edges)))

    print("%-46s %-40s %5s %5s %5s" % ("HOST", "GUEST", "PAR", "MISS", "UNBK"))
    for (h, g), d in sorted(
        per_edge.items(), key=lambda kv: -kv[1]["unbanked"]
    )[: args.top]:
        print("%-46s %-40s %5d %5d %5d"
              % (h[-46:], (g or "(host-owned)")[-40:],
                 d["parents"], d["missing"], d["unbanked"]))

    # ---- tier 3: naming-free per-unit funclet deficit, with a CONTROL GROUP.
    # For every pinned unit: how many r12-frame funclets does retail have inside
    # its .text span, vs how many our compiled obj emits?  This needs no symbol
    # map at all, so it also covers the ~873 unnamed parents.  Comparing
    # scatter-include hosts against non-hosts prices the lever honestly.
    # NOTE: an obj-wide `__unwind$` count is NOT usable here — our objs emit far
    # more COMDATs than the linker keeps.  The only fair comparison is per
    # NAMED parent that our obj actually defines, done identically for both
    # groups.
    host_bases = set(by_base)
    src_by_unitkey = {k: v for k, v in _srcmap.items()}
    grp_stats = {True: defaultdict(int), False: defaultdict(int)}
    control_rows = []
    for parent, kids in funclets_of.items():
        kid_vas = [k for _kind, k in kids if k in screened]
        if not kid_vas:
            continue
        unit = unit_cpp(parent)
        if not unit:
            continue
        ukey = unit_report_key(unit, report_units)
        src = src_by_unitkey.get(ukey) if ukey else None
        if not src:
            continue
        v = host_view(src)
        if v is None:
            continue
        name = symmap.get("0x%08x" % parent)
        if not name or name not in v.funcs:
            continue
        bf = v.funclets(name)
        bf = 0 if bf is None else bf
        flag = Path(src).name in host_bases
        g = grp_stats[flag]
        g["parents"] += 1
        g["target"] += len(kid_vas)
        g["base"] += bf
        if bf == 0:
            tb = sum(
                1
                for w in (pe.words(parent, funcs[parent]["size"] // 4) or [])
                if (w >> 26) == 18 and (w & 1)
            )
            bb = v.bl_count(name)
            if bb is not None and tb > bb:
                g["sharp"] += 1
        if bf < len(kid_vas):
            g["short_parents"] += 1
            g["short_funclets"] += len(kid_vas) - bf
            if not flag:
                control_rows.append(
                    {
                        "parent_va": "0x%08x" % parent,
                        "name": name,
                        "source": src,
                        "target_funclets": len(kid_vas),
                        "base_funclets": bf,
                    }
                )

    print("\ntier-3 funclet deficit over NAMED parents our obj defines "
          "(identical method both groups)")
    print("%-16s %8s %8s %8s %8s %10s %9s %7s %8s"
          % ("GROUP", "PARENTS", "TGT_FL", "BASE_FL", "SHORT_P", "SHORT_FL",
             "RATE", "SHARP", "S_RATE"))
    for flag, lbl in ((True, "scatter-host"), (False, "control")):
        g = grp_stats[flag]
        rate = 100.0 * g["short_parents"] / g["parents"] if g["parents"] else 0.0
        srate = 100.0 * g["sharp"] / g["parents"] if g["parents"] else 0.0
        print("%-16s %8d %8d %8d %8d %10d %8.2f%% %7d %7.2f%%"
              % (lbl, g["parents"], g["target"], g["base"],
                 g["short_parents"], g["short_funclets"], rate, g["sharp"],
                 srate))

    # ---- tier 2: inlined-away call signal (no EH involvement required)
    inl = [r for r in rows
           if r["have_body"] and r["bl_delta"] is not None and r["bl_delta"] > 0
           and r["parent_match"] != 100.0]
    print("\ntier-2 (inlined-away call: target has more `bl` than base, parent "
          "not strict): %d parents" % len(inl))

    print("\nSHARP collapse (base funclets==0 AND target has more `bl`) — the "
          "genuine \u00a715 fingerprint: %d parents" % len(sharp))
    print("%-54s %6s %4s %5s %5s" % ("SYMBOL", "MATCH", "TGT", "BL\u0394", "OWNER"))
    for r in sharp:
        print("%-54s %6s %4d %5s  %s"
              % (r["name"][:54],
                 ("%.1f" % r["parent_match"]) if r["parent_match"] is not None else "-",
                 r["target_funclets"], r["bl_delta"],
                 Path(r["guest"]).name if r["guest"] else "(host)" ))

    print("\ntop collapse parents:")
    print("%-52s %6s %4s %4s %4s %7s %5s"
          % ("SYMBOL", "MATCH", "TGT", "BAS", "UNBK", "BLΔ", "GUEST"))
    for r in collapse[: args.top]:
        print("%-52s %6s %4d %4d %4d %7s  %s"
              % (r["name"][:52],
                 ("%.1f" % r["parent_match"]) if r["parent_match"] is not None else "-",
                 r["target_funclets"], r["base_funclets"], r["unbanked_funclets"],
                 r["bl_delta"] if r["bl_delta"] is not None else "-",
                 Path(r["guest"]).name if r["guest"] else "(host)"))

    # ---------------------------------------------------------------- §4b
    # PRECISION FILTER.  The SHARP fingerprint (base funclets==0 + target has
    # more `bl`) is NECESSARY but NOT SUFFICIENT for the scatter-include lever:
    # it fires identically on ordinary header-inline-policy divergence, where a
    # class-template member defined in-class in a header gets inlined in EVERY
    # TU regardless of any scatter-include.  Discriminate by asking WHERE the
    # inlined-away callee is defined:
    #   defined in exactly ONE of our objs  -> a .cpp-level definition; if that
    #       obj is the host, the scatter-include really is what exposed it.
    #   defined in MANY objs                -> a header inline/template COMDAT,
    #       emitted everywhere it is used -> NOT scatter-attributable.
    if sharp:
        print("\n\u00a74b precision filter: where is the inlined-away callee defined?")
        defobjs: dict[str, set] = defaultdict(set)
        for op in objdir.rglob("*.obj"):
            try:
                c = Coff(op)
            except Exception:
                continue
            rel = str(op.relative_to(objdir))
            for nm, (_v, sc) in c.syms.items():
                if sc > 0 and not nm.startswith(NOISE_PREFIX):
                    defobjs[nm].add(rel)
        tgtdir = repo / "build/45410914/obj"
        verdicts = defaultdict(int)
        for r in sharp:
            unit_stem = Path(r["host"]).stem
            tp = tgtdir / (unit_stem + ".obj")
            hv2 = host_view(r["host"])
            host_rel = str(obj_path(objdir, r["host"]).relative_to(objdir))
            guest_rels = {
                str(obj_path(objdir, g).relative_to(objdir)) for g in hosts[r["host"]]
            }
            tonly = []
            try:
                tc = Coff(tp)
                tw, tsec, tval = tc.func_words(r["name"], n=4096)
                if tw and hv2:
                    tcall = set(tc.reloc_syms(tsec, tval, tval + 4 * len(tw)).values())
                    bw, bsec, bval = hv2.coff.func_words(r["name"], n=4096)
                    bcall = set(
                        hv2.coff.reloc_syms(bsec, bval, bval + 4 * len(bw)).values()
                    ) if bw else set()
                    tonly = sorted(
                        n for n in tcall - bcall
                        if not n.startswith(NOISE_PREFIX) and not n.startswith("fn_")
                    )
            except Exception:
                pass
            # A callee can only have been INLINED by us if its body is in our TU.
            cls, detail = "UNKNOWN", ""
            if tonly and hv2:
                in_tu = [n for n in tonly if n in hv2.funcs]
                if not in_tu:
                    cls = "NOT-IN-TU (body divergence)"
                    detail = tonly[0][:54]
                else:
                    # Available outside the host TU and its guests => header
                    # inline/template COMDAT, emitted in every user regardless
                    # of any scatter-include.
                    scatter = []
                    for n in in_tu:
                        others = defobjs[n] - {host_rel} - guest_rels
                        if not others:
                            scatter.append(n)
                    if scatter:
                        cls = "SCATTER-ATTRIBUTABLE"
                        detail = scatter[0][:54]
                    else:
                        n0 = in_tu[0]
                        cls = "HEADER-INLINE (not scatter)"
                        detail = "%s in %d objs" % (n0[:40], len(defobjs[n0]))
            verdicts[cls] += 1
            r["callee_class"] = cls
            r["target_only_callees"] = tonly
            print("  %-46s %-30s %s" % (r["name"][:46], cls, detail))
        print("\n  verdict tally: " + ", ".join(
            "%s=%d" % (k, v) for k, v in sorted(verdicts.items())))

    if args.json:
        out = Path(args.json)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(
            json.dumps(
                {
                    "edges": edges,
                    "rows": rows,
                    "collapse": collapse,
                    "sharp": sharp,
                    "per_edge": [
                        {"host": h, "guest": g, **d} for (h, g), d in per_edge.items()
                    ],
                },
                indent=1,
            )
        )
        print("\nwrote %s" % out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
