#!/usr/bin/env python3
"""sigscan.py -- tree-wide scan for the DC3-SIGNATURE map defect class.

WHAT THIS LOOKS FOR (lane GAMEROW-1's class, 3a1af7e3)
=====================================================
`scripts/target_symbol_map.json` assigns MSVC mangled names to RB3 retail
addresses. Retail symbols are STRIPPED, so every one of those names is OUR OWN
ASSIGNMENT -- usually transferred from DC3 (engine) or the rb3-Wii oracle
(game). DC3 is NEWER than RB3, so a function whose signature DC3 refactored
carries DC3's signature on an RB3 address where retail has the OLDER form.

The symptom under `functionRelocDiffs=name_check`: a call site is charged
because the TARGET reloc names `?Foo@@YAXHAAVLocale@@@Z` while OUR obj names
`?Foo@@YAXH@Z`. Both DEMANGLE TO THE SAME FUNCTION with a DIFFERENT PARAMETER
LIST. That is a fingerprint no fold-alias produces: ICF folds bodies, it does
not rename a function into a different overload of ITSELF.

GAMEROW-1 ran this over the GAME tier only (1,161 uncrossed rows / 7,494
sites) and closed three rows for +7,176 B. This runs it TREE-WIDE.

HOW IT WORKS
============
Charged sites are read STRUCTURALLY out of the COFF objs, not out of the diff:
for every symbol present in both the dtk target objs and our compiled objs with
the same relocation count, compare slot-wise, and take every slot where the
offset and type agree but the NAME does not. That is exactly the population
`name_check` charges, and it is the same definition tools/icf_relocname_census.py
uses (this reuses its imports rather than restating them).

Two subtractions, both because `name_check` does not charge them either:
  - pairs already in scripts/symbol_aliases.json (objdiff forgives them)
  - pairs where EITHER side is a placeholder (fn_/lbl_/data_/...) -- an
    unidentified callee is forgiven, so it is never a charge.

CLASSIFICATION (llvm-undname, not hand-rolled mangling arithmetic)
==================================================================
  SIG_SAME_QUALNAME  qualified name IDENTICAL incl. template args, parameter
                     list DIFFERENT. <-- the GAMEROW-1 defect class.
  TEMPLATE_ARGS      qualified name differs but the template-stripped name is
                     equal (`_Rb_tree<A,B>::f` vs `_Rb_tree<A,C>::f`). This is
                     the STL fold / wrong-container stratum. GAMEROW-1 declined
                     it deliberately; lane CONTAINER-1 owns the _Rb_tree case.
  UNRELATED          different functions entirely (wrong callee, or fold alias).

⚠ WHY MANGLED-STRING SURGERY IS NOT USED ANYWHERE HERE. MSVC back-references
(`V4@`) are valid only relative to PARAMETER POSITION, so a substring edit of a
signature inside a mangled name silently produces an impossible name. That cost
GAMEROW-1 a 1,744 B row exactly (-540 measured against +1,204 predicted). This
tool only ever READS names; any fix must RE-MANGLE.

PRICING
=======
Rows are joined back to build/45410914/report.json. `matched_code` keys on
`fuzzy_match_percent == 100`, ALL-OR-NOTHING PER ROW, so a row's prize is its
FULL size and only if EVERY charged site in it closes. We therefore report,
per candidate pair, the rows it appears in and how many of each row's charged
sites the pair accounts for -- a row where our pair is 11 of 13 sites is not a
prize until the other 2 are understood (GAMEROW-1's abandoned
CountOrCreateExpandedDetails, exactly).
"""
import collections
import glob
import json
import os
import re
import subprocess
import sys

sys.path.insert(0, "tools")
sys.path.insert(0, ".")
from icf_alias_build import collect, placeholder  # noqa: E402

ROOT = os.getcwd()


# ---------------------------------------------------------------------------
# demangling
# ---------------------------------------------------------------------------
def demangle_all(names):
    """{mangled: demangled} via llvm-undname, one batch process."""
    names = list(names)
    if not names:
        return {}
    p = subprocess.run(["llvm-undname"], input="\n".join(names) + "\n",
                       capture_output=True, text=True)
    out = {}
    lines = [ln for ln in p.stdout.split("\n")]
    # llvm-undname echoes the input line then prints the demangling, then a
    # blank line. Parse in triples but tolerate drift by re-syncing on the echo.
    i = 0
    idx = 0
    while i < len(lines) and idx < len(names):
        if lines[i].strip() == names[idx]:
            dem = lines[i + 1] if i + 1 < len(lines) else ""
            out[names[idx]] = dem.strip()
            idx += 1
            i += 2
        else:
            i += 1
    if len(out) != len(names):
        print("  WARN: demangled %d of %d names" % (len(out), len(names)),
              file=sys.stderr)
    return out


CALLCONV = (" __cdecl ", " __stdcall ", " __thiscall ", " __fastcall ",
            " __vectorcall ")


def split_name(dem):
    """demangled string -> (qualified_name, param_list) or None if not a function.

    ⛔ THE OBVIOUS IMPLEMENTATION IS WRONG AND ITS FAILURE IS SILENT. Finding the
    parameter `(` and walking BACKWARD to the previous space returns `delete` for
    `BinStream::operator delete(void *)` and `dtor'` for
    `SpotlightEnder::`vbase dtor'(void)` -- so EVERY `??3` / `??_D` / `??_G` pair
    in the binary compares equal and floods the SIG_SAME_QUALNAME class with
    pairs that are plainly different functions. That is exactly the
    "instrument confirms whatever you point it at" shape; it was caught only
    because the candidate dump prints the DEMANGLED names next to the verdict.

    Instead: MSVC's demangled form is
        [access: ][virtual|static ][return type ]__cdecl Qualified::Name(params)...
    so the qualified name is bounded on the left by the CALLING CONVENTION --
    which is present for every function symbol and absent for every data symbol
    (vftable / RTTI / string literals), giving a free non-function reject too.
    """
    if not dem:
        return None
    # ⛔ NOT rfind(). A template argument can itself be a FUNCTION, and then the
    # LAST calling convention belongs to the argument, not the symbol:
    #   void __cdecl _outline_SetClipType<{protected: void __cdecl
    #        CharDriver::SetClipType(class Symbol), 0, 0}>(CharDriver *, Symbol)
    # rfind() reads that as `CharDriver::SetClipType` and then MATCHES a target
    # genuinely named CharDriver::SetClipType -- a false SIG_SAME_QUALNAME hit
    # manufactured entirely by the parser. Take the FIRST callconv at nesting
    # depth 0 (rfind was chosen to survive function-POINTER return types; those
    # nest inside parens, so depth-0-first handles them too).
    depth = 0
    cc = -1
    for i, c in enumerate(dem):
        if c in "<({":
            depth += 1
        elif c in ">)}":
            depth -= 1
        elif depth == 0:
            for tok in CALLCONV:
                if dem.startswith(tok, i):
                    cc, cclen = i, len(tok)
                    break
            if cc >= 0:
                break
    if cc < 0:
        return None                      # data symbol: vftable, RTTI, ...
    start = cc + cclen
    # forward to the parameter list: first `(` at template depth 0 that is not
    # inside a `back-quoted special name` and not `operator()`'s own parens.
    depth = 0
    i = start
    intick = False
    open_paren = -1
    while i < len(dem):
        c = dem[i]
        if intick:
            if c == "'":
                intick = False
        elif c == "`":
            intick = True
        elif c == "<":
            depth += 1
        elif c == ">":
            depth -= 1
        elif c == "(" and depth == 0:
            if dem[start:i].rstrip().endswith("operator"):
                j = dem.find(")", i)
                if j < 0:
                    return None
                i = j + 1
                continue
            open_paren = i
            break
        i += 1
    if open_paren < 0:
        return None
    return dem[start:open_paren].strip(), dem[open_paren:].strip()


def strip_templates(q):
    out = []
    depth = 0
    for c in q:
        if c == "<":
            depth += 1
        elif c == ">":
            depth -= 1
        elif depth == 0:
            out.append(c)
    return "".join(out)


# ---------------------------------------------------------------------------
# report join
# ---------------------------------------------------------------------------
def load_report(path="build/45410914/report.json"):
    r = json.load(open(path))
    rows = {}
    for u in r["units"]:
        meta = u.get("metadata", {}) or {}
        cats = meta.get("progress_categories") or []
        tier = cats[0] if cats else "unclassified"
        src = meta.get("source_path") or ""
        for f in u.get("functions", []):
            nm = f["name"]
            rows.setdefault(nm, []).append({
                "unit": u["name"],
                "tier": tier,
                "src": src,
                # ⚠ report.json numerics arrive as JSON STRINGS; coerce.
                "size": int(f.get("size", 0) or 0),
                "mpn": float(f.get("match_percent_normalized", 0) or 0),
                "fuzzy": float(f.get("fuzzy_match_percent", 0) or 0),
            })
    return r, rows


def main():
    limit_dump = int(sys.argv[1]) if len(sys.argv) > 1 else 60

    import pickle
    cache = os.path.expanduser("~/tmp/sigscan_sites.pkl")
    if os.environ.get("SIGSCAN_CACHE") == "1" and os.path.exists(cache):
        sites, victims, per_fn_charged, raw_pairs, raw_sites = pickle.load(open(cache, "rb"))
        print("  (reusing cached COFF sweep: %d raw pairs)" % raw_pairs, file=sys.stderr)
        tgt = ours = None
    else:
        tgt = collect(sorted(glob.glob("build/45410914/obj/**/*.obj", recursive=True)), "target")
        ours = collect(sorted(glob.glob("build/45410914/src/**/*.obj", recursive=True)), "ours")

        # ---- charged sites -------------------------------------------------
        sites = collections.Counter()      # (retail_name, our_name) -> sites
        victims = collections.defaultdict(collections.Counter)
        per_fn_charged = collections.Counter()
        for name, (mb, rel, sz) in ours.items():
            rt = tgt.get(name)
            if not rt or len(rt[1]) != len(rel):
                continue
            for (ro, rn, rty), (oo, on, oty) in zip(rt[1], rel):
                if ro != oo or rty != oty or rn == on:
                    continue
                sites[(rn, on)] += 1
                victims[(rn, on)][name] += 1
                per_fn_charged[name] += 1
        raw_pairs = len(sites)
        raw_sites = sum(sites.values())
        pickle.dump((sites, victims, per_fn_charged, raw_pairs, raw_sites),
                    open(cache, "wb"))

    # ---- subtract already-forgiven ----------------------------------------
    eq = {}
    try:
        al = json.load(open("scripts/symbol_aliases.json"))
        for g in al["groups"]:
            grp = set([g["survivor"]] + list(g["folded"]))
            for n in grp:
                eq.setdefault(n, set()).update(grp)
    except FileNotFoundError:
        pass
    n_alias = n_ph = 0
    for k in list(sites):
        if k[1] in eq.get(k[0], ()) or k[0] in eq.get(k[1], ()):
            del sites[k]
            n_alias += 1
    for k in list(sites):
        if placeholder(k[0]) or placeholder(k[1]):
            del sites[k]
            n_ph += 1

    # ⛔ RE-DERIVE the per-row site counts over the SURVIVING pairs only.
    # per_fn_charged is first built during the COFF sweep, i.e. BEFORE the alias
    # and placeholder subtractions -- so its denominator counted sites that
    # name_check never charges. That made the "pair_sites / row_sites" column
    # read 1/9 for a row whose only REAL charge was ours, and the tool then
    # filed the row as unrealisable.
    # MEASURED COST: UtilDrawSphere was pre-registered at +68 B and came in at
    # +272 B, because the 204 B ?Highlight@RndDrawable@@UAAXXZ row -- reported
    # as "1 of 9, cannot cross" and used as the CONTROL on that prediction --
    # was really 1 of 1 and crossed. Re-deriving moved the tool's
    # fully-realisable total from 200 B to 3,468 B.
    # The error direction matters: it UNDER-states realisable bytes, which is
    # the direction that silently closes candidates nobody reopens.
    per_fn_charged = collections.Counter()
    for pair, encs in victims.items():
        if pair not in sites:
            continue
        for enc, n in encs.items():
            per_fn_charged[enc] += n

    print("\n=== CHARGED-PAIR POPULATION (binary-wide, name_check definition) ===")
    print("  raw distinct (T,B) pairs            : %6d  (%d sites)" % (raw_pairs, raw_sites))
    print("  - already aliased (objdiff forgives): %6d" % n_alias)
    print("  - placeholder either side (forgiven): %6d" % n_ph)
    print("  = CHARGED pairs both real-named     : %6d  (%d sites)"
          % (len(sites), sum(sites.values())))

    # ---- classify ----------------------------------------------------------
    allnames = set()
    for a, b in sites:
        allnames.add(a)
        allnames.add(b)
    dem = demangle_all(sorted(allnames))

    cls = collections.defaultdict(list)
    for (rn, on), c in sites.items():
        sa, sb = split_name(dem.get(rn, "")), split_name(dem.get(on, ""))
        if not sa or not sb:
            cls["UNPARSED"].append((rn, on, c))
            continue
        qa, pa = sa
        qb, pb = sb
        if qa == qb:
            cls["SIG_SAME_QUALNAME"].append((rn, on, c))
        elif strip_templates(qa) == strip_templates(qb):
            cls["TEMPLATE_ARGS"].append((rn, on, c))
        else:
            cls["UNRELATED"].append((rn, on, c))

    print("\n=== CLASSIFICATION of the charged pairs ===")
    for k in ("SIG_SAME_QUALNAME", "TEMPLATE_ARGS", "UNRELATED", "UNPARSED"):
        v = cls.get(k, [])
        print("  %-18s %5d pairs %7d sites" % (k, len(v), sum(x[2] for x in v)))

    # ---- price against report.json ----------------------------------------
    rep, rows = load_report()

    def price(pairs):
        """-> (tier -> [n_pairs, n_sites, n_uncrossed_rows, bytes_at_stake])"""
        by_tier = collections.defaultdict(lambda: [0, 0, set(), 0])
        for rn, on, c in pairs:
            tiers_hit = set()
            for enc, n in victims[(rn, on)].items():
                for row in rows.get(enc, []):
                    if row["fuzzy"] >= 100.0:
                        continue  # already crossed: no bytes to win
                    t = row["tier"]
                    tiers_hit.add(t)
                    by_tier[t][1] += n
                    if enc not in by_tier[t][2]:
                        by_tier[t][2].add(enc)
                        by_tier[t][3] += row["size"]
            for t in tiers_hit:
                by_tier[t][0] += 1
        return by_tier

    for k in ("SIG_SAME_QUALNAME", "TEMPLATE_ARGS"):
        bt = price(cls.get(k, []))
        print("\n=== %s -- UNCROSSED rows only, by tier ===" % k)
        print("  %-14s %6s %7s %8s %12s" % ("tier", "pairs", "sites", "rows", "bytes"))
        tot = [0, 0, 0, 0]
        for t, v in sorted(bt.items(), key=lambda x: -x[1][3]):
            print("  %-14s %6d %7d %8d %12d" % (t, v[0], v[1], len(v[2]), v[3]))
            tot[1] += v[1]
            tot[2] += len(v[2])
            tot[3] += v[3]
        print("  %-14s %6s %7d %8d %12d" % ("TOTAL", "-", tot[1], tot[2], tot[3]))

    # ---- sub-stratify SIG_SAME_QUALNAME -----------------------------------
    # Not every same-qualname pair is the DC3-signature class. `OnMsg(AMsg&)`
    # vs `OnMsg(BMsg&)` is the SAME arity with a DIFFERENT message type: those
    # handler bodies are frequently identical, ICF folds them, and the map names
    # whichever survivor it saw. The DC3-REFACTOR shape is a changed PARAMETER
    # COUNT (a parameter added or removed) or a changed CV-QUALIFIER -- neither
    # of which an overload set produces by accident.
    def paramlist(p):
        """'(int, class X &) const' -> ('int, class X &', ' const')"""
        if not p.startswith("("):
            return p, ""
        depth = 0
        for i, c in enumerate(p):
            if c == "(":
                depth += 1
            elif c == ")":
                depth -= 1
                if depth == 0:
                    return p[1:i], p[i + 1:].strip()
        return p, ""

    def nparams(p):
        inner, _ = paramlist(p)
        if inner.strip() in ("", "void"):
            return 0
        n, depth = 1, 0
        for c in inner:
            if c in "<(":
                depth += 1
            elif c in ">)":
                depth -= 1
            elif c == "," and depth == 0:
                n += 1
        return n

    sub = collections.defaultdict(list)
    sublabel = {}
    for rn, on, c in cls.get("SIG_SAME_QUALNAME", []):
        pa, pb = split_name(dem[rn])[1], split_name(dem[on])[1]
        ia, sa_ = paramlist(pa)
        ib, sb_ = paramlist(pb)
        if nparams(pa) != nparams(pb):
            k = "ARITY"
        elif ia == ib:
            # identical parameter list => the divergence is the trailing
            # cv-qualifier or the access specifier / storage class.
            k = "CV_OR_ACCESS"
        else:
            k = "PARAM_TYPE"
        sub[k].append((rn, on, c))
        sublabel[(rn, on)] = k
    print("\n=== SIG_SAME_QUALNAME sub-strata ===")
    print("  (ARITY / CV_OR_ACCESS = the DC3-refactor shape;")
    print("   PARAM_TYPE at equal arity = overload-fold suspect)")
    for k in ("ARITY", "CV_OR_ACCESS", "PARAM_TYPE"):
        v = sub.get(k, [])
        print("  %-14s %4d pairs %5d sites" % (k, len(v), sum(x[2] for x in v)))

    # ---- FULLY realisable prize -------------------------------------------
    # matched_code is ALL-OR-NOTHING PER ROW, so a pair only banks a row's
    # bytes when it accounts for EVERY charged site in that row.
    print("\n=== FULLY-REALISABLE rows (pair accounts for ALL charged sites) ===")
    real_tot = collections.Counter()
    for k in ("ARITY", "CV_OR_ACCESS", "PARAM_TYPE"):
        for rn, on, c in sub.get(k, []):
            for enc, n in victims[(rn, on)].items():
                for row in rows.get(enc, []):
                    if row["fuzzy"] >= 100.0 or n != per_fn_charged[enc]:
                        continue
                    real_tot[k] += row["size"]
                    print("  %-13s %6d B %-8s %-30s %s"
                          % (k, row["size"], row["tier"], row["unit"], enc[:60]))
    print("  ---- realisable by sub-stratum: %s ; TOTAL %d B"
          % (dict(real_tot), sum(real_tot.values())))

    # ---- candidate dump: SIG_SAME_QUALNAME ranked by bytes-if-it-crosses ---
    print("\n=== SIG_SAME_QUALNAME candidates, ranked by uncrossed bytes ===")
    cand = []
    for rn, on, c in cls.get("SIG_SAME_QUALNAME", []):
        rowinfo = []
        byts = 0
        seen = set()
        for enc, n in victims[(rn, on)].items():
            for row in rows.get(enc, []):
                if row["fuzzy"] >= 100.0 or enc in seen:
                    continue
                seen.add(enc)
                byts += row["size"]
                rowinfo.append((row["size"], enc, row["tier"], row["unit"],
                                n, per_fn_charged[enc], row["fuzzy"]))
        if not rowinfo:
            continue
        rowinfo.sort(reverse=True)
        cand.append((byts, c, rn, on, rowinfo))
    cand.sort(reverse=True)
    print("  %d candidate pairs touch >=1 uncrossed row" % len(cand))
    out = []
    for byts, c, rn, on, rowinfo in cand[:limit_dump]:
        print("\n  [%d B over %d uncrossed rows] %d charged sites  <%s>"
              % (byts, len(rowinfo), c, sublabel.get((rn, on), "?")))
        print("    TARGET(map): %s" % dem.get(rn, rn))
        print("    OURS       : %s" % dem.get(on, on))
        print("    T mangled  : %s" % rn)
        print("    B mangled  : %s" % on)
        for sz, enc, tier, unit, n, tot_sites, fz in rowinfo[:6]:
            print("      %8d B  %-8s %-34s sites %d/%d  fuzzy %.2f  %s"
                  % (sz, tier, unit, n, tot_sites, fz, enc[:70]))
        out.append({"bytes": byts, "sites": c, "target": rn, "ours": on,
                    "target_dem": dem.get(rn), "ours_dem": dem.get(on),
                    "rows": [{"size": s, "sym": e, "tier": t, "unit": u,
                              "pair_sites": n, "row_sites": ts, "fuzzy": f}
                             for s, e, t, u, n, ts, f in rowinfo]})
    json.dump(out, open(os.path.expanduser("~/tmp/sigscan_candidates.json"), "w"), indent=1)
    print("\n  -> ~/tmp/sigscan_candidates.json")


if __name__ == "__main__":
    main()
