#!/usr/bin/env python3
"""Find the container/element-TYPE divergence class straight out of the COFF
symbol tables -- no objdiff, no report.json.

WHY THIS IS THE SAME CLASS
--------------------------
A charged relocation-name site at the name_check ruler is, mechanically, "the
target obj's relocation points at symbol T, ours points at symbol B". Both objs
carry those names in their own COFF symbol tables. So the class

    retail  ?insert@?$list@PAVCharClip@@...  (list<CharClip*>::insert)
    ours    ?insert@?$list@PAVObject@@...    (list<Object*>::insert)

is visible as: for one unit, the TARGET references an instantiation of some
class template that OURS does not, while OURS references a sibling instantiation
of the same template with the same member that the TARGET does not.

*** THE TARGET OBJ MUST BE POST-RENAMER. *** A fresh worktree's reflinked target
objs still carry anonymous fn_<addr> symbols, so every mangled lookup answers
"absent" and this whole scan reads a confident, vacuous zero. Run a full build
first; tools/check_target_objs_renamed.py is the guard.

WHAT IT DOES NOT SETTLE
-----------------------
Which side is wrong. A divergence here is a locus to read retail bytes at.
"""
import argparse, collections, json, re, struct, sys
from pathlib import Path

IMAGE_SYM_CLASS_EXTERNAL = 2


def coff_symbols(path):
    """All symbol names in a COFF obj, split into (defined, referenced)."""
    try:
        d = Path(path).read_bytes()
    except OSError:
        return set(), set()
    if len(d) < 20:
        return set(), set()
    nsyms, symptr = struct.unpack_from("<I", d, 12)[0], None
    symptr = struct.unpack_from("<I", d, 8)[0]
    nsyms = struct.unpack_from("<I", d, 12)[0]
    if not symptr or not nsyms:
        return set(), set()
    strtab_off = symptr + nsyms * 18
    if strtab_off + 4 > len(d):
        return set(), set()
    strsz = struct.unpack_from("<I", d, strtab_off)[0]
    strtab = d[strtab_off: strtab_off + max(strsz, 4)]
    defined, referenced = set(), set()
    i = 0
    while i < nsyms:
        off = symptr + i * 18
        if off + 18 > len(d):
            break
        raw = d[off:off + 8]
        secnum = struct.unpack_from("<h", d, off + 12)[0]
        sclass = d[off + 16]
        naux = d[off + 17]
        if raw[:4] == b"\0\0\0\0":
            so = struct.unpack_from("<I", raw, 4)[0]
            end = strtab.find(b"\0", so)
            name = strtab[so:end].decode("ascii", "replace") if end > 0 else ""
        else:
            name = raw.rstrip(b"\0").decode("ascii", "replace")
        if name:
            if sclass == IMAGE_SYM_CLASS_EXTERNAL and secnum == 0:
                referenced.add(name)       # undefined external = a call target
            elif secnum > 0:
                defined.add(name)
        i += 1 + naux
    return defined, referenced


def tmpl_class(name):
    """-> (member, class-template-base, class-args) or None."""
    if not name.startswith("?"):
        return None
    m = re.match(r"^(\?\?[0-9A-Za-z_]|\?\?_[0-9A-Za-z])(.*)$", name)
    if m:
        member, rest = m.group(1), m.group(2)
    else:
        m = re.match(r"^\?([A-Za-z0-9_]+)@(.*)$", name)
        if not m:
            return None
        member, rest = m.group(1), m.group(2)
    if not rest.startswith("?$"):
        return None
    base = rest[2:].split("@", 1)[0]
    # The FULL mangled name must be reconstructed: callers look these up in
    # scripts/target_symbol_map.json, and a bare suffix matches NOTHING there --
    # a lookup that silently misses every row returns a clean, decisive zero
    # that is indistinguishable from "no candidates exist".
    full = (member + rest) if member.startswith("??") else ("?" + member + "@" + rest)
    return (member, base, full)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--project", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--min-rows", type=int, default=1)
    a = ap.parse_args()
    proj = Path(a.project)

    pairing = json.load(open(proj / "objdiff.json"))
    units = pairing.get("units", [])
    print(f"[coff-scan] {len(units)} units declared in objdiff.json")

    # anti-vacuity: the target objs must be post-renamer
    probe_named = 0
    rows = []
    for u in units:
        tp, bp = u.get("target_path"), u.get("base_path")
        if not tp or not bp:
            continue
        tdef, tref = coff_symbols(proj / tp)
        bdef, bref = coff_symbols(proj / bp)
        probe_named += sum(1 for n in tdef | tref if n.startswith("?"))
        # index each side's referenced template instantiations by (member, base)
        tix, bix = collections.defaultdict(set), collections.defaultdict(set)
        # A relocation target can be an UNDEFINED external (call into another TU)
        # or a DEFINED COMDAT (a template instantiation this TU emits itself).
        # Restricting to undefined externals misses every `list<T>::insert`-class
        # instantiation BY CONSTRUCTION and reads a confident, vacuous zero.
        for n in tref | tdef:
            p = tmpl_class(n)
            if p:
                tix[(p[0], p[1])].add(p[2])
        for n in bref | bdef:
            p = tmpl_class(n)
            if p:
                bix[(p[0], p[1])].add(p[2])
        for key in set(tix) & set(bix):
            only_t, only_b = tix[key] - bix[key], bix[key] - tix[key]
            if only_t and only_b:
                rows.append({
                    "unit": u["name"], "member": key[0], "family": key[1],
                    "target_only": sorted(only_t), "base_only": sorted(only_b),
                })
    if probe_named < 1000:
        sys.exit(f"REFUSING: only {probe_named} mangled names across all target "
                 f"objs -- the tree looks PRE-RENAMER. Run a full build first.")
    print(f"[coff-scan] {probe_named} mangled names seen (anti-vacuity OK)")
    json.dump(rows, open(a.out, "w"), indent=1)
    fam = collections.Counter(r["family"] for r in rows)
    unit = collections.Counter(r["unit"] for r in rows)
    print(f"[coff-scan] {len(rows)} divergent (member,family) cells "
          f"in {len(unit)} units")
    print("\ntop families:")
    for k, v in fam.most_common(20):
        print(f"  {v:5d}  {k}")
    print("\ntop units:")
    for k, v in unit.most_common(25):
        print(f"  {v:5d}  {k}")
    print(f"\nwrote {a.out}")


if __name__ == "__main__":
    main()
