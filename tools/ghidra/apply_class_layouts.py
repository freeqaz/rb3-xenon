#!/usr/bin/env python3
"""apply_class_layouts.py -- phase 4 of docs/plans/ghidra-data-sync-2026-11-22.md: turn compiler-reported class
layouts (tools/ghidra/emit_class_layouts.py output) into the Ghidra class structures that `this` already points
at, so `this + 0x34` prints as `this->mSinks`.

apply_prototypes.py put every method in its class namespace; Ghidra types a __thiscall `this` as a pointer to
the class structure of that namespace (VariableUtilities.findOrCreateClassStruct), which until now was an
empty placeholder. This script fills those structures, and only those: a layout is applied to a Ghidra class
namespace whose simple name equals the layout's class name, when exactly one such namespace exists and it is
not nested inside another class (the compiler's report prints simple names only, so a nested class cannot be
told apart). Members: every member the report prints, flattened through base classes, at its reported offset;
type = pointer (4 bytes) for `T *`, the builtin for a primitive, the class structure for a by-value member of
a seeded class whose size fits, else undefined bytes up to the next member. Re-running replaces what it wrote.

Headless, exclusive project access: run on a reflink COPY of the live project, then swap.
  ~/tmp/ghidra-sync/run.sh tools/ghidra/apply_class_layouts.py --project-location <copy>/RB3Xenon \
      --layouts ~/tmp/ghidra-sync/layouts.json [--program default.xex-d56e7f] [--dry-run]
"""
import argparse
import json
import re
import sys

IDENT = re.compile(r"^[A-Za-z_]\w*$")
PRIM = {
    "bool": ("bool", 1), "char": ("char", 1), "signed char": ("schar", 1), "unsigned char": ("uchar", 1),
    "short": ("short", 2), "unsigned short": ("ushort", 2), "wchar_t": ("wchar16", 2),
    "int": ("int", 4), "unsigned int": ("uint", 4), "long": ("int", 4), "unsigned long": ("uint", 4),
    "float": ("float", 4), "double": ("double", 8), "__int64": ("longlong", 8),
    "unsigned __int64": ("ulonglong", 8),
}


def clean_members(c):
    """Report rows -> [(offset, name, type, in_base)], monotone offsets, first row per offset, in range."""
    out, seen_off, last = [], set(), -1
    for m in c["members"]:
        off, name = m["offset"], m["name"]
        if off < last:          # the vbtable/vftable rows that follow a class are not members
            break
        last = off
        if off >= c["size"] or off in seen_off:
            continue
        if name in ("{vfptr}", "{vbptr}"):
            name = name.strip("{}")
        elif not IDENT.match(name):
            continue
        seen_off.add(off)
        out.append((off, name, m.get("type", ""), m.get("in_base", [])))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--project-location", required=True)
    ap.add_argument("--project-name", default="RB3Xenon")
    ap.add_argument("--program", default="default.xex-d56e7f")
    ap.add_argument("--layouts", required=True)
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()
    layouts = json.load(open(args.layouts))["classes"]

    import pyghidra
    pyghidra.start(verbose=False)
    from ghidra.base.project import GhidraProject
    from ghidra.program.model.data import (StructureDataType, PointerDataType, Undefined,
                                           ArrayDataType, BuiltInDataTypeManager)
    from ghidra.program.model.listing import VariableUtilities, GhidraClass

    project = GhidraProject.openProject(args.project_location, args.project_name, True)
    program = project.openProgram("/", args.program, False)
    dtm = program.getDataTypeManager()
    builtin = BuiltInDataTypeManager.getDataTypeManager()

    prim_cache = {}

    def builtin_dt(gname):
        if gname not in prim_cache:
            from java.util import ArrayList
            lst = ArrayList()
            builtin.findDataTypes(gname, lst)
            prim_cache[gname] = lst[0] if lst.size() else None
        return prim_cache[gname]

    # simple name -> class namespaces
    by_name = {}
    st = program.getSymbolTable()
    for ns in st.getClassNamespaces():
        by_name.setdefault(ns.getName(), []).append(ns)
    stats = {"layouts": len(layouts), "no_namespace": 0, "ambiguous": 0, "nested": 0, "template": 0,
             "applied": 0, "members": 0, "typed_ptr": 0, "typed_prim": 0, "typed_struct": 0}
    targets = {}
    for name, c in layouts.items():
        if name.startswith("?$") or "<" in name:
            stats["template"] += 1
            continue
        nss = by_name.get(name, [])
        if not nss:
            stats["no_namespace"] += 1
            continue
        if len(nss) > 1:
            stats["ambiguous"] += 1
            continue
        ns = nss[0]
        if isinstance(ns.getParentNamespace(), GhidraClass):
            stats["nested"] += 1
            continue
        targets[name] = (ns, c)
    print("[apply_layouts] candidates %d of %d layouts" % (len(targets), len(layouts)), flush=True)

    tx = program.startTransaction("apply compiler class layouts")
    ok = False
    try:
        structs = {}
        for name, (ns, c) in targets.items():
            structs[name] = VariableUtilities.findOrCreateClassStruct(ns, dtm)

        def build(name, c, with_structs):
            s0 = structs[name]
            sd = StructureDataType(s0.getCategoryPath(), s0.getName(), c["size"], dtm)
            mem = clean_members(c)
            used = set()
            for i, (off, mname, typ, in_base) in enumerate(mem):
                gap = (mem[i + 1][0] if i + 1 < len(mem) else c["size"]) - off
                if gap <= 0:
                    continue
                fname = mname
                if fname in used:
                    fname = "%s_%s" % (mname, in_base[-1] if in_base else off)
                if fname in used:
                    fname = "%s_%x" % (mname, off)
                used.add(fname)
                t = typ.strip()
                dt, kind = None, None
                if t.endswith("*") and gap >= 4:
                    base = t.rstrip("* ").replace("const ", "").strip()
                    pointee = structs.get(base) if with_structs else None
                    dt, kind = PointerDataType(pointee, 4, dtm), "typed_ptr"
                elif t in PRIM and PRIM[t][1] <= gap:
                    dt, kind = builtin_dt(PRIM[t][0]), "typed_prim"
                elif with_structs and t in structs and t != name and layouts[t]["size"] <= gap \
                        and layouts[t]["size"] > 0:
                    dt, kind = structs[t], "typed_struct"
                if dt is None:
                    kind = None
                    dt = Undefined.getUndefinedDataType(gap) if gap <= 8 else \
                        ArrayDataType(Undefined.getUndefinedDataType(1), gap, 1, dtm)
                try:
                    sd.replaceAtOffset(off, dt, dt.getLength() if dt.getLength() > 0 else gap, fname,
                                       ("in base %s" % "::".join(in_base)) if in_base else None)
                    stats["members"] += 1
                    if kind:
                        stats[kind] += 1
                except Exception as e:
                    print("[apply_layouts] %s +0x%x %s: %s" % (name, off, fname, e))
            return sd

        # pass 1: sizes and scalar/pointer members; pass 2: by-value class members (sizes now final)
        for p, with_structs in ((1, False), (2, True)):
            if p == 2:
                for k in ("members", "typed_ptr", "typed_prim", "typed_struct"):
                    stats[k] = 0
            for name, (ns, c) in targets.items():
                sd = build(name, c, with_structs)
                structs[name].replaceWith(sd)
        stats["applied"] = len(targets)
        # control: read every seeded structure back and compare it with the compiler's layout
        bad = []
        for name, (ns, c) in targets.items():
            sdt = structs[name]
            mem = clean_members(c)
            if sdt.getLength() != c["size"]:
                bad.append("%s size %d != %d" % (name, sdt.getLength(), c["size"]))
                continue
            for i, (off, mname, _t, _b) in enumerate(mem):
                gap = (mem[i + 1][0] if i + 1 < len(mem) else c["size"]) - off
                comp = sdt.getComponentAt(off) if gap > 0 else None
                if gap > 0 and (comp is None or comp.getOffset() != off
                                or not (comp.getFieldName() or "").startswith(mname)):
                    bad.append("%s +0x%x %s -> %s" % (name, off, mname,
                                                      comp.getFieldName() if comp else None))
                    break
        stats["verify_ok"] = len(targets) - len(bad)
        stats["verify_bad"] = len(bad)
        for b in bad[:20]:
            print("[apply_layouts] VERIFY " + b)
        ok = True
    finally:
        program.endTransaction(tx, ok and not args.dry_run)
    if ok and not args.dry_run:
        project.save(program)
    project.close()
    print("[apply_layouts] DONE " + json.dumps(stats))
    return 0


if __name__ == "__main__":
    sys.exit(main())
