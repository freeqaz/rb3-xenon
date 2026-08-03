#!/usr/bin/env python3
"""Partition the NO_CLASS_ANCHOR frontier -- the rows no class-anchored
instrument this project owns can judge.

WHY THIS EXISTS
---------------
Every adjudication instrument rb3-xenon has built is CLASS-ANCHORED:

    tools/retail_rtti.py            needs a vtable / ??_R4 COL  -> needs a class
    tools/map_class_neighbour_audit needs class-named neighbours -> needs a class
    tools/vbase_dtor_class_audit    needs a base relation        -> needs a class
    reachable_ceiling attribution   needs `cls_of(mangled)`      -> needs a class

`tools/reachable_ceiling.py:cls_of()` is two anchored regexes:

    ??[0-9_][A-Z]?(Ident)@@      ctor/dtor/vtable form
    ?Ident@(Ident)@@             ordinary method form

Anything they cannot parse is labelled `NO_CLASS_ANCHOR` and NOT JUDGED.  Lane
DL-2 measured that as **over half the named blocker backlog**.  This tool asks
the question DL-2 could not: *what are those rows actually made of?*

★ THE HEADLINE CORRECTION THIS TOOL SHIPS
-----------------------------------------
`NO_CLASS_ANCHOR` is NOT "this row has no class".  It is "**the regex could not
parse one**".  A large share of the population are members of TEMPLATE classes
whose template ARGUMENTS are ordinary, RTTI-carrying Milo classes:

    ?_M_erase@?$vector@V?$Key@VTransform@@@@...   -> vector<Key<Transform>>

`Transform` is a perfectly good anchor; `cls_of` simply stops at the `?$`.  So
part of the frontier is not un-anchorable, only un-PARSED -- and the split
between those two is exactly what this tool measures.

INSTRUMENTS AND CONTROLS
------------------------
Two INDEPENDENT parsers classify every name, and they must AGREE:

  A. a hand-written MSVC basic-name/scope splitter (mangled-string only), and
  B. `llvm-undname` (an outside implementation this repo did not write).

Disagreement is REPORTED, never silently resolved -- shape 2 (a silently
vacuous scanner) is the failure this guards against.  Positive control: run
both over the rows that DID anchor; parser A must call essentially all of them
NAMED_SCOPE.  That control CAN fail (sabotage it with --sabotage anchor-blind).

The retail-occurrence sub-instrument ("template-argument anchoring") ships with
its own NULL, recomputed every run on the SAME KIND of population (template
rows already at mpn==100) -- shape 4, the base-rate error, is what killed the
unit-stem test at 0.84x and the fold-alias model at 1.95x.

RULER: `match_percent_normalized` (mpn) throughout -- the ruler
`matched_functions` is computed on.  mpn masks relocation args, so "at 100" is
NOT "correct"; it is only "the rows nobody is proposing to reclassify", which
is all a null needs it to be.

EXIT CODES
    0  partition produced
    2  a CONTROL FAILED (parser disagreement over threshold, or the
       known-positive anchor control did not fire)
    3  input missing / collapsed join
"""
from __future__ import annotations

import argparse
import collections
import hashlib
import json
import re
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

# --------------------------------------------------------------------------
# EXACT copy of tools/reachable_ceiling.py's anchor test.  Copied, not
# imported, ON PURPOSE: if that tool's regex changes, the drift control below
# fires instead of this partition silently re-basing onto a new definition.
# --------------------------------------------------------------------------
_CLS_CTOR = re.compile(r"\?\?[0-9_][A-Z]?([A-Za-z_]\w*)@@")
_CLS_METH = re.compile(r"\?[A-Za-z_]\w*@([A-Za-z_]\w*)@@")


def cls_of(name):
    m = _CLS_CTOR.match(name) or _CLS_METH.match(name)
    return m.group(1) if m else None


ANON_RX = re.compile(r"^fn_[0-9a-fA-F]{8}$")

# --------------------------------------------------------------------------
# PARSER A -- hand-written MSVC basic-name + immediate-scope splitter.
#
# An MSVC decorated name is  '?' <basic-name> <scope...> '@@' <encoding>.
# We only need the basic name's extent and the FIRST CHARACTERS of whatever
# follows it; that is decidable without parsing the type grammar.
# --------------------------------------------------------------------------
_SPECIAL = re.compile(r"^\?\?(__[0-9A-Za-z]|_[0-9A-Za-z]|[0-9A-Za-z])")
_PLAIN = re.compile(r"^\?([?$@]?[A-Za-z_0-9]*)@")

SCOPE_GLOBAL = "GLOBAL"
SCOPE_TEMPLATE = "TEMPLATE_CLASS"
SCOPE_ANON_NS = "ANON_NAMESPACE"
SCOPE_NAMED = "NAMED"
SCOPE_BACKREF = "BACKREF"
SCOPE_UNPARSED = "UNPARSED"


def scope_kind_A(name: str, sabotage=None):
    """Return (scope_kind, basic_name_form) from the MANGLED string alone."""
    if sabotage == "anchor-blind":
        return SCOPE_UNPARSED, "sabotaged"
    if not name.startswith("?"):
        return "NOT_MANGLED", "c_symbol"
    if name.startswith("??$"):
        # template FUNCTION: ??$name@<args>@<scope>@@ -- the args carry nested
        # '@'s, so the scope is not decidable here.  Say so; parser B resolves.
        return "TEMPLATE_FN_UNDECIDED", "template_function"
    m = _SPECIAL.match(name)
    if m:
        basic = m.group(0)
        rest = name[m.end():]
    else:
        m = _PLAIN.match(name)
        if not m:
            return SCOPE_UNPARSED, "unparsed"
        basic = m.group(0)
        rest = name[m.end():]
    if rest.startswith("@"):
        return SCOPE_GLOBAL, basic
    if rest.startswith("?$"):
        return SCOPE_TEMPLATE, basic
    if rest.startswith("?A0x") or rest.startswith("?A@"):
        return SCOPE_ANON_NS, basic
    if rest[:1].isdigit():
        return SCOPE_BACKREF, basic
    if rest[:1].isalpha() or rest[:1] == "_":
        return SCOPE_NAMED, basic
    return SCOPE_UNPARSED, basic


def basic_name_extent(name: str):
    """Return the index just past the basic name, or None."""
    if not name.startswith("?") or name.startswith("??$"):
        return None
    m = _SPECIAL.match(name) or _PLAIN.match(name)
    return m.end() if m else None


_ENC = re.compile(r"@@([A-Z_$])")


def cls_of_v2(name: str):
    """`cls_of` with the two forms it cannot parse repaired.

    The shipped regexes demand `@@` IMMEDIATELY after the first scope
    identifier, so they resolve `?Meth@Class@@` but fail on:

        ?Meth@Inner@Outer@@      nested class
        ??0Inner@Outer@@         nested-class ctor/dtor/deleting-dtor
        ?fn@ns@@YAXXZ            namespace-scope free function

    The third is not a class at all, and the SHIPPED regex silently returns
    the NAMESPACE as if it were one.  v2 separates them using the encoding
    letter after `@@`: `Y` == free function (so the scope is a namespace),
    anything else == member.  Returns (class_or_None, reason).
    """
    e = basic_name_extent(name)
    if e is None:
        return None, "no_basic_name"
    rest = name[e:]
    m = re.match(r"([A-Za-z_]\w*)@", rest)
    if not m:
        if rest.startswith("?$"):
            return None, "template_scope"
        if rest.startswith("@"):
            return None, "global_scope"
        if rest.startswith("?A"):
            return None, "anonymous_namespace"
        return None, "unparsed_scope"
    enc = _ENC.search(name)
    if enc and enc.group(1) == "Y":
        return None, "namespace_scope_free_function"
    return m.group(1), "ok"


# basic-name -> human class of the SYMBOL ITSELF (not its scope)
def basic_kind(name: str) -> str:
    if not name.startswith("?"):
        return "c_symbol"
    if name.startswith("??$"):
        return "template_function"
    if name.startswith("??_7") or name.startswith("??_8"):
        return "vftable/vbtable"
    if name.startswith("??_R"):
        return "rtti"
    if name.startswith("??__E"):
        return "dynamic_init"
    if name.startswith("??__F"):
        return "atexit_dtor"
    if name.startswith("??_G") or name.startswith("??_E"):
        return "deleting_dtor"
    if name.startswith("??_9") or name.startswith("??_B"):
        return "vcall_thunk/guard"
    if name.startswith("??0"):
        return "ctor"
    if name.startswith("??1"):
        return "dtor"
    if re.match(r"^\?\?[2-9A-Z]", name):
        return "operator"
    return "method"


# --------------------------------------------------------------------------
# PARSER B -- llvm-undname, an implementation this repo did not write.
# --------------------------------------------------------------------------
def undname(names, exe="llvm-undname"):
    """Batch-demangle.  Returns {mangled: demangled-or-None}."""
    uniq = list(dict.fromkeys(names))
    out = {}
    CH = 4000
    for i in range(0, len(uniq), CH):
        chunk = uniq[i:i + CH]
        p = subprocess.run([exe], input="\n".join(chunk) + "\n",
                           capture_output=True, text=True)
        # llvm-undname echoes the input line, then the demangled line, then a
        # blank line -- and prints "error: Invalid mangled name" on stderr for
        # failures WITHOUT emitting a demangled line.  Parse defensively.
        lines = p.stdout.split("\n")
        j = 0
        for m in chunk:
            while j < len(lines) and lines[j].strip() == "":
                j += 1
            if j >= len(lines):
                out[m] = None
                continue
            if lines[j] != m:
                # desync -- refuse rather than mis-attribute
                raise SystemExit(
                    f"REFUSING: llvm-undname output desynchronised at {m!r} "
                    f"(saw {lines[j]!r}).  A mis-aligned demangler would "
                    f"silently attach the wrong scope to every row after it.")
            j += 1
            if j < len(lines) and lines[j].strip() != "" and lines[j] != m:
                out[m] = lines[j]
                j += 1
            else:
                out[m] = None
    return out


def split_top_level(s: str, sep: str):
    """Split on `sep` at angle/paren/backtick depth 0.

    ★ The backtick clause is load-bearing, not cosmetic.  MSVC special names
    demangle to BACKTICK-QUOTED strings CONTAINING SPACES --
    ``Cls::`scalar deleting dtor'`` -- and without protecting them the
    return-type stripper below cuts at that space and leaves ``dtor'``, i.e.
    a single component, i.e. "global scope".  That bug produced 384 false
    parser disagreements on the first run of this tool and would have
    reclassified every ``??_G``/``??_E`` row in the tree as a free function.
    """
    parts, depth, buf, i, tick = [], 0, [], 0, False
    while i < len(s):
        c = s[i]
        if tick:
            if c == "'":
                tick = False
                depth -= 1
        elif c == "`":
            tick = True
            depth += 1
        elif c in "<(":
            depth += 1
        elif c in ">)":
            depth -= 1
        if depth == 0 and not tick and s.startswith(sep, i):
            parts.append("".join(buf))
            buf = []
            i += len(sep)
            continue
        buf.append(c)
        i += 1
    parts.append("".join(buf))
    return parts


_LEAD = re.compile(r"^(\[thunk\]:)?\s*(public:|private:|protected:)?\s*"
                   r"(static\s+|virtual\s+)*")


def qualified_from_demangled(dem: str):
    """Return the list of top-level scope components, innermost LAST.

    'public: void __cdecl A::B<int>::f(int)' -> ['A', 'B<int>', 'f']
    Returns None if the shape is not recognisable.
    """
    if not dem:
        return None
    s = _LEAD.sub("", dem).strip()

    def _walk(txt):
        """yield (index, char, depth, in_tick)"""
        depth, tick = 0, False
        for i, c in enumerate(txt):
            if tick:
                if c == "'":
                    tick = False
                    depth -= 1
                yield i, c, depth, True
                continue
            if c == "`":
                tick = True
                depth += 1
                yield i, c, depth, True
                continue
            if c == "<":
                depth += 1
            elif c == ">":
                depth -= 1
            yield i, c, depth, False

    # cut the parameter list: the first '(' at depth 0 outside a backtick
    cut = None
    for i, c, depth, tick in _walk(s):
        if c == "(" and depth == 0 and not tick:
            cut = i
            break
    head = s[:cut] if cut is not None else s
    # drop the return type: last depth-0, non-backtick whitespace
    last = 0
    for i, c, depth, tick in _walk(head):
        if c == " " and depth == 0 and not tick:
            last = i + 1
    head = head[last:]
    if not head:
        return None
    return split_top_level(head, "::")


# class/struct names named anywhere in a demangled string (template args
# included) -- these are the CANDIDATE anchors the regex threw away.
_TYPENAME = re.compile(r"\b(?:class|struct)\s+([A-Za-z_][\w:]*)")


def arg_class_names(dem: str):
    if not dem:
        return []
    outs = []
    for t in _TYPENAME.findall(dem):
        outs.append(t.split("::")[-1])
    return outs


# --------------------------------------------------------------------------
# retail occurrence scanner (pure Python bytes.count -- grep is binary-blind)
# --------------------------------------------------------------------------
class Retail:
    def __init__(self, path: Path, sabotage=None):
        self.path = Path(path)
        self.data = self.path.read_bytes() if self.path.exists() else None
        self.sabotage = sabotage
        self._memo = {}

    @property
    def available(self):
        return self.data is not None

    def count(self, s):
        if self.data is None:
            return None
        if self.sabotage == "retail-blind":
            return 0
        if s not in self._memo:
            self._memo[s] = self.data.count(s.encode())
        return self._memo[s]

    def controls(self):
        rows = []
        if not self.available:
            return [("retail binary present", False, f"missing {self.path}")]
        n = self.count("ObjectDir")
        rows.append(("known-positive 'ObjectDir'", n == 168, f"count={n} want 168"))
        n2 = self.count("Transform")
        rows.append(("known-positive 'Transform'", bool(n2), f"count={n2} want >0"))
        n3 = self.count("ZzQqNotAClassDM4")
        rows.append(("known-negative fabricated", n3 == 0, f"count={n3} want 0"))
        return rows


# --------------------------------------------------------------------------
STL_BASES = {
    "vector", "list", "_List_base", "_Rb_tree", "map", "multimap", "set",
    "multiset", "hash_map", "hash_set", "hashtable", "deque", "pair",
    "basic_string", "_STLP_alloc_proxy", "StlNodeAlloc", "allocator",
    "_Vector_base", "_Slist_base", "slist", "_Rb_tree_node", "priority_queue",
    "stack", "queue", "binder1st", "binder2nd", "less", "equal_to", "hash",
    "_Select1st", "_Identity", "reverse_iterator", "_Nonconst_traits",
    "_Const_traits", "auto_ptr", "iterator", "_Ht_iterator", "_List_iterator",
}


# --------------------------------------------------------------------------
# SCOPE annotation -- IS THIS ROW EVEN OURS TO WORK ON?
#
# A row in an `auto_NN_<addr>_text` unit has NO SOURCE FILE: those units are
# auto-derived address pins, not compiled translation units.  And a row whose
# outermost namespace is D3DXShader / XGRAPHICS / LEAPCORE / std / XAudio is
# XDK or vendor code, which the standing project scope directive HARD-SKIPS.
# Partitioning without this split reports the vendor tail as if it were
# backlog -- which is how "over half the named backlog is unjudgeable" reads
# as a wall when most of it is simply out of scope.
# --------------------------------------------------------------------------
AUTO_UNIT_RX = re.compile(r"(^|/)auto_\d+_[0-9A-Fa-f]+")
# ⚠ `std` / `stlpmtx_std` are DELIBERATELY NOT vendor.  An STL instantiation is
# owned by whichever Milo TU instantiates it (STLport is at src/system/stlport
# and is compiled by us), so excluding them would have deleted exactly the
# `vector<Key<Transform>>` class the brief pointed at.  A first pass of this
# tool did exclude them; the correction is recorded rather than silently made.
VENDOR_NS = {
    "D3DXShader", "XGRAPHICS", "LEAPCORE", "LEAPFX", "OAPIPELINE",
    "XAudio", "XAUDIO2", "XMA", "Xam", "Direct3D", "ATG", "D3D", "D3DXTex",
    "XG_D3DXTex", "XSIM", "XNet", "XHV", "XACT", "XGFX", "XPS", "DSP",
}
VENDOR_PREFIX = ("XG", "XInput", "XHVEngine", "XMA", "XAudio", "XeCrypt",
                 "XNet", "Xam", "XMemAlloc", "D3D", "XPhysical", "XeKeys")


def annotate_scope(r):
    r["auto_unit"] = bool(AUTO_UNIT_RX.search(r["unit"]))
    q = r.get("qual") or []
    root = q[0] if len(q) >= 2 else None
    if root:
        root = template_base(root)
    r["ns_root"] = root
    nm = r["name"]
    vendor = False
    if root in VENDOR_NS:
        vendor = True
    elif not nm.startswith("?"):
        vendor = (nm.startswith("_") or nm.startswith("__")
                  or any(nm.startswith(p) for p in VENDOR_PREFIX))
    elif any(x in nm for x in ("@D3DXShader@@", "@XGRAPHICS@@", "@LEAPCORE@@")):
        vendor = True
    r["vendor"] = vendor
    r["in_scope"] = (not r["auto_unit"]) and (not vendor)


def template_base(scope_component: str) -> str:
    """'vector<Key<Transform>,...>' -> 'vector'"""
    i = scope_component.find("<")
    return scope_component[:i] if i > 0 else scope_component


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--root", default=None)
    ap.add_argument("--report", default=None)
    ap.add_argument("--census", default=None,
                    help="reachable_ceiling --json output, for the top-60 "
                         "blocker view the DL-2 headline was computed on")
    ap.add_argument("--retail-exe", default=None)
    ap.add_argument("--json", dest="jsonout", default=None)
    ap.add_argument("--sabotage", choices=["anchor-blind", "retail-blind"])
    ap.add_argument("--max-disagree-pct", type=float, default=2.0)
    a = ap.parse_args()

    root = Path(a.root) if a.root else Path(__file__).resolve().parents[1]
    report = Path(a.report) if a.report else root / "build/45410914/report.json"
    retail = Path(a.retail_exe) if a.retail_exe else root / "orig/45410914/band.exe"
    if not report.exists():
        print(f"REFUSING(3): no report.json at {report}", file=sys.stderr)
        return 3
    doc = json.loads(report.read_text())

    # ---- population: EVERY named row below 100 on the mpn ruler ------------
    pop = []          # charged
    null_pop = []     # untreated: named rows at mpn == 100
    for u in doc.get("units", []):
        un = u.get("name") or u.get("metadata", {}).get("source_path") or "?"
        for f in u.get("functions") or []:
            nm = f["name"]
            if ANON_RX.match(nm):
                continue
            row = {"name": nm, "unit": un, "size": int(f.get("size", 0) or 0),
                   "mpn": f["match_percent_normalized"],
                   "fuzzy": f.get("fuzzy_match_percent", 0.0)}
            (null_pop if row["mpn"] == 100.0 else pop).append(row)
    if not pop:
        print("REFUSING(3): zero named sub-100 rows -- collapsed join",
              file=sys.stderr)
        return 3

    # ---- parser A over both populations -----------------------------------
    for r in pop + null_pop:
        r["anchor"] = cls_of(r["name"])
        r["scope_A"], r["basic"] = scope_kind_A(r["name"], a.sabotage)
        r["basic_kind"] = basic_kind(r["name"])

    # ---- CONTROL 1: parser A vs the anchor regex on rows that DID anchor ---
    anchored = [r for r in pop + null_pop if r["anchor"]]
    a_named = sum(1 for r in anchored if r["scope_A"] == SCOPE_NAMED)
    ctrl_anchor_ok = bool(anchored) and (a_named / len(anchored)) > 0.95
    ctrl1 = {
        "check": "parser A calls the ANCHORED population NAMED scope",
        "ok": ctrl_anchor_ok,
        "detail": f"{a_named}/{len(anchored)} = "
                  f"{100.0*a_named/max(1,len(anchored)):.2f}% (want >95%); "
                  f"--sabotage anchor-blind drives this to 0 and must REFUSE",
    }

    noanchor = [r for r in pop if not r["anchor"]]

    # ---- parser B (llvm-undname) on the NO-ANCHOR population + a sample of
    #      the anchored one (so the cross-check is not measured only where it
    #      is convenient)
    tocheck = [r["name"] for r in noanchor]
    sample_anchored = anchored[::max(1, len(anchored) // 2000)][:2000]
    tocheck += [r["name"] for r in sample_anchored]
    dem = undname(tocheck)

    disagree = []
    for r in noanchor + sample_anchored:
        d = dem.get(r["name"])
        r["demangled"] = d
        q = qualified_from_demangled(d)
        r["qual"] = q
        if q is None:
            r["scope_B"] = "UNDEMANGLED"
            continue
        if len(q) == 1:
            r["scope_B"] = SCOPE_GLOBAL
        else:
            encl = q[-2]
            if "<" in encl:
                r["scope_B"] = SCOPE_TEMPLATE
            elif encl.startswith("`anonymous namespace'"):
                r["scope_B"] = SCOPE_ANON_NS
            else:
                r["scope_B"] = SCOPE_NAMED
        # comparable verdicts only
        if r["scope_A"] in (SCOPE_GLOBAL, SCOPE_TEMPLATE, SCOPE_NAMED,
                            SCOPE_ANON_NS) and r["scope_B"] != r["scope_A"]:
            disagree.append((r["name"], r["scope_A"], r["scope_B"]))

    comparable = [r for r in noanchor + sample_anchored
                  if r.get("scope_B") not in (None, "UNDEMANGLED")
                  and r["scope_A"] in (SCOPE_GLOBAL, SCOPE_TEMPLATE,
                                       SCOPE_NAMED, SCOPE_ANON_NS)]
    dpct = 100.0 * len(disagree) / max(1, len(comparable))
    ctrl2 = {
        "check": "two INDEPENDENT parsers agree on scope kind",
        "ok": dpct <= a.max_disagree_pct,
        "detail": f"{len(disagree)}/{len(comparable)} disagree = {dpct:.2f}% "
                  f"(threshold {a.max_disagree_pct}%)",
        "samples": disagree[:15],
    }

    # ---- THE PARTITION ----------------------------------------------------
    def partition_label(r):
        sA, sB = r["scope_A"], r.get("scope_B")
        s = sB if sB not in (None, "UNDEMANGLED") else sA
        if sA == "NOT_MANGLED":
            return "C_SYMBOL_NO_MANGLING"
        if s == SCOPE_TEMPLATE:
            return "TEMPLATE_CLASS_MEMBER"
        if s == SCOPE_GLOBAL:
            return ("FREE_TEMPLATE_FUNCTION" if r["basic_kind"] == "template_function"
                    else "FREE_FUNCTION_OR_OPERATOR")
        if s == SCOPE_ANON_NS:
            return "ANON_NAMESPACE_SCOPE"
        if s == SCOPE_NAMED:
            return "NAMED_SCOPE_REGEX_MISSED"
        return "UNCLASSIFIED"

    for r in noanchor:
        r["partition"] = partition_label(r)
        if r["partition"] == "TEMPLATE_CLASS_MEMBER" and r.get("qual"):
            r["tmpl_base"] = template_base(r["qual"][-2])
        r["arg_classes"] = [c for c in arg_class_names(r.get("demangled") or "")
                            if c not in STL_BASES]
        annotate_scope(r)

    # same partition over the NULL population (untreated, at mpn==100), so
    # every rate below has its own base rate -- shape 4.
    null_noanchor = [r for r in null_pop if not r["anchor"]]
    dem_null = undname([r["name"] for r in null_noanchor])
    for r in null_noanchor:
        d = dem_null.get(r["name"])
        r["demangled"] = d
        q = qualified_from_demangled(d)
        r["qual"] = q
        if q is None:
            r["scope_B"] = "UNDEMANGLED"
        elif len(q) == 1:
            r["scope_B"] = SCOPE_GLOBAL
        else:
            encl = q[-2]
            r["scope_B"] = (SCOPE_TEMPLATE if "<" in encl
                            else SCOPE_ANON_NS if encl.startswith("`anonymous")
                            else SCOPE_NAMED)
        r["partition"] = partition_label(r)
        if r["partition"] == "TEMPLATE_CLASS_MEMBER" and r.get("qual"):
            r["tmpl_base"] = template_base(r["qual"][-2])
        r["arg_classes"] = [c for c in arg_class_names(r.get("demangled") or "")
                            if c not in STL_BASES]
        annotate_scope(r)

    def sized(rows):
        c = collections.Counter()
        b = collections.Counter()
        pen = collections.Counter()
        for r in rows:
            k = r["partition"]
            c[k] += 1
            b[k] += r["size"]
            pen[k] += r["size"] * (100.0 - r["mpn"]) / 100.0
        return {k: {"rows": c[k], "bytes": b[k], "penalty_bytes": round(pen[k], 1)}
                for k in sorted(c, key=lambda x: -c[x])}

    part_charged = sized(noanchor)
    part_null = sized(null_noanchor)

    # ---- THE SCOPE SPLIT: how much of this frontier is even ours? ---------
    # NULL (shape 4): the same split on the ANCHORED sub-100 rows -- the
    # backlog nobody calls unjudgeable.  Without it, "94% out of scope" could
    # simply be a property of the whole sub-100 backlog rather than of the
    # no-anchor subset, and the finding would be a base-rate error.
    for r in [x for x in pop if x["anchor"]]:
        r.setdefault("qual", None)
        annotate_scope(r)
    anchored_charged = [r for r in pop if r["anchor"]]
    null_scope = {
        "anchored_sub100_total": len(anchored_charged),
        "auto_pin_unit_no_source": sum(1 for r in anchored_charged if r["auto_unit"]),
        "in_scope": sum(1 for r in anchored_charged if r["in_scope"]),
    }
    null_scope["in_scope_pct"] = 100.0 * null_scope["in_scope"] / max(
        1, null_scope["anchored_sub100_total"])

    in_scope = [r for r in noanchor if r["in_scope"]]
    scope_break = {
        "total": len(noanchor),
        "auto_pin_unit_no_source": sum(1 for r in noanchor if r["auto_unit"]),
        "vendor_namespace_or_prefix": sum(1 for r in noanchor if r["vendor"]),
        "IN_SCOPE (source-backed unit, non-vendor)": len(in_scope),
        "in_scope_bytes": sum(r["size"] for r in in_scope),
        "in_scope_penalty_bytes": round(
            sum(r["size"] * (100.0 - r["mpn"]) / 100.0 for r in in_scope), 1),
    }
    scope_break["in_scope_pct"] = 100.0 * len(in_scope) / max(1, len(noanchor))
    scope_break["NULL_anchored_sub100"] = null_scope
    scope_break["enrichment_out_of_scope"] = (
        (100.0 - scope_break["in_scope_pct"]) / max(1e-9, 100.0 - null_scope["in_scope_pct"]))
    part_in_scope = sized(in_scope)
    ns_hist = collections.Counter(r["ns_root"] for r in noanchor if r["ns_root"])
    in_scope_units = collections.Counter(r["unit"] for r in in_scope)

    # ---- TEMPLATE-ARGUMENT ANCHORING, with its null ------------------------
    RN = Retail(retail, a.sabotage)
    retail_controls = RN.controls()
    if RN.available and not all(ok for _n, ok, _d in retail_controls):
        print("REFUSING(2): retail scanner control failed", file=sys.stderr)
        for n, ok, d in retail_controls:
            print(f"  {'ok ' if ok else 'FAIL'} {n}: {d}", file=sys.stderr)
        return 2

    def argfrac(rows):
        """Of template-class rows that EXPOSE a non-STL argument class, how
        many have at least one argument class ABSENT from band.exe?"""
        tot = withargs = absent = 0
        for r in rows:
            if r["partition"] != "TEMPLATE_CLASS_MEMBER":
                continue
            tot += 1
            ac = r.get("arg_classes") or []
            if not ac:
                continue
            withargs += 1
            if RN.available and any(RN.count(c) == 0 for c in ac):
                absent += 1
        return {"template_rows": tot, "with_nonstl_arg": withargs,
                "any_arg_absent": absent,
                "coverage_pct": 100.0 * withargs / max(1, tot),
                "absent_rate_pct": 100.0 * absent / max(1, withargs)}

    arg_charged = argfrac(noanchor)
    arg_null = argfrac(null_noanchor)
    enrich = (arg_charged["absent_rate_pct"] / arg_null["absent_rate_pct"]
              if arg_null["absent_rate_pct"] else None)

    # ---- ANCHOR v2: is NAMED_SCOPE_REGEX_MISSED a wall or a regex bug? -----
    # POSITIVE CONTROL that can fail: v2 must reproduce v1 EXACTLY on every
    # row v1 already anchored.  A v2 that quietly re-anchors those differently
    # would silently re-base the whole attribution instrument.
    v1v2 = collections.Counter()
    v1v2_ex = collections.defaultdict(list)
    churn = collections.Counter()
    for r in pop + null_pop:
        v1 = r["anchor"]
        v2, why = cls_of_v2(r["name"])
        if v1 == v2:
            continue
        if v1 and v2 and v2.endswith(v1) and len(v2) == len(v1) + 1:
            k = "CTOR_FIRST_LETTER_EATEN"
        elif v1 and v2 is None and why == "namespace_scope_free_function":
            k = "NAMESPACE_REPORTED_AS_CLASS"
        elif v1 is None and v2:
            k = "NEWLY_ANCHORED"
        elif v1 and v2 is None:
            k = "V2_DROPPED:" + why
        else:
            k = "UNEXPLAINED"
        v1v2[k] += 1
        if len(v1v2_ex[k]) < 5:
            v1v2_ex[k].append([r["name"], v1, v2])
        if v1 and v2 and RN.available:
            l1 = "ABS" if RN.count(v1) == 0 else "PRES"
            l2 = "ABS" if RN.count(v2) == 0 else "PRES"
            if l1 != l2:
                churn[f"{k}:{l1}->{l2}"] += 1
    unexplained = v1v2["UNEXPLAINED"]
    ctrl3 = {
        "check": "every v1-vs-v2 anchor difference is an EXPLAINED defect class",
        "ok": unexplained == 0,
        "detail": (f"{sum(v1v2.values())} differences, {unexplained} unexplained "
                   f"(want 0); classes={dict(v1v2)}"),
        "samples": {k: v1v2_ex[k] for k in v1v2},
        "retail_label_churn": dict(churn),
        "note": (
            "★ THE SHIPPED tools/reachable_ceiling.py cls_of() HAS A LIVE BUG: "
            "the optional [A-Z]? intended for the ??_G/??_E letter is GREEDY, so "
            "it eats the class's own initial -- ??0RndText@@ anchors on 'ndText' "
            "and ??1ObjectDir@@ on 'bjectDir'.  It is ALMOST ENTIRELY SELF-"
            "MASKING because RetailNames.count() is a SUBSTRING search and the "
            "truncated name is a substring of the full one; the measured label "
            "churn is reported above.  So this is a COVERAGE defect (rows never "
            "judged), NOT a correctness defect in DL-2's published enrichment."),
    }

    def v2_stats(rows):
        newly, reasons = [], collections.Counter()
        for r in rows:
            c, why = cls_of_v2(r["name"])
            r["anchor_v2"], r["anchor_v2_why"] = c, why
            if c:
                newly.append(r)
            else:
                reasons[why] += 1
        return newly, reasons

    new_charged, why_charged = v2_stats(noanchor)
    new_null, why_null = v2_stats(null_noanchor)

    # Re-calibrate the EXISTING class-absence instrument on the population v2
    # newly exposes, WITH ITS OWN NULL.  Inheriting DL-2's 3.32x would be a
    # base-rate error: that number was measured on a different population.
    def absrate(rows, only_in_scope):
        tot = ab = 0
        for r in rows:
            if only_in_scope and not r.get("in_scope"):
                continue
            tot += 1
            if RN.available and RN.count(r["anchor_v2"]) == 0:
                ab += 1
        return tot, ab, (100.0 * ab / max(1, tot))

    v2_cal = {}
    for tag, only in (("all_rows", False), ("in_scope_only", True)):
        ct, ca, cr = absrate(new_charged, only)
        nt, na, nr = absrate(new_null, only)
        v2_cal[tag] = {
            "charged": {"anchored": ct, "class_absent": ca, "rate_pct": cr},
            "null_at100": {"anchored": nt, "class_absent": na, "rate_pct": nr},
            "enrichment": (cr / nr) if nr else None,
        }

    anchor_v2 = {
        "newly_anchored_charged": len(new_charged),
        "newly_anchored_charged_in_scope": sum(1 for r in new_charged
                                               if r.get("in_scope")),
        "newly_anchored_null_at100": len(new_null),
        "still_unanchored_charged": dict(why_charged.most_common()),
        "calibration": v2_cal,
    }

    # ---- template base histogram + per-unit concentration ------------------
    tmpl_hist = collections.Counter(
        r.get("tmpl_base") for r in noanchor
        if r["partition"] == "TEMPLATE_CLASS_MEMBER")
    tmpl_hist_null = collections.Counter(
        r.get("tmpl_base") for r in null_noanchor
        if r["partition"] == "TEMPLATE_CLASS_MEMBER")
    unit_hist = collections.Counter(r["unit"] for r in noanchor)

    # ---- the DL-2 top-60 blocker view, for continuity ---------------------
    census_view = None
    if a.census and Path(a.census).exists():
        cj = json.loads(Path(a.census).read_text())
        byname = {}
        for r in noanchor:
            byname.setdefault(r["name"], r)
        cnt = collections.Counter()
        seen = 0
        for u in cj.get("units", []):
            for b in u.get("blocker_rows", []):
                if b.get("kind") != "named":
                    continue
                if (b.get("attribution") or {}).get("label") != "NO_CLASS_ANCHOR":
                    continue
                seen += 1
                m = byname.get(b["name"])
                cnt[m["partition"] if m else "NOT_IN_FULL_POP"] += 1
        census_view = {"census_no_anchor_blockers": seen,
                       "partition": dict(cnt.most_common()),
                       "note": "census blocker_rows are capped at 60/unit "
                               "(sorted by -mpn), so this is a SAMPLE of the "
                               "full population, not the population."}

    payload = {
        "tool": "no_anchor_partition.py",
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "provenance": {
            "report": str(report),
            "report_sha256": hashlib.sha256(report.read_bytes()).hexdigest(),
            "head": subprocess.run(["git", "-C", str(root), "rev-parse", "HEAD"],
                                   capture_output=True, text=True).stdout.strip(),
            "retail_exe": str(retail),
            "sabotage": a.sabotage,
        },
        "population": {
            "named_sub100_total": len(pop),
            "named_sub100_anchored": len(pop) - len(noanchor),
            "named_sub100_NO_CLASS_ANCHOR": len(noanchor),
            "named_at100_total(null)": len(null_pop),
            "named_at100_NO_CLASS_ANCHOR(null)": len(null_noanchor),
        },
        "controls": [ctrl1, ctrl2, ctrl3] + [{"check": n, "ok": ok, "detail": d}
                                             for n, ok, d in retail_controls],
        "anchor_v2": anchor_v2,
        "partition_charged": part_charged,
        "partition_null_at100": part_null,
        "scope_breakdown": scope_break,
        "partition_in_scope": part_in_scope,
        "namespace_root_hist": dict(ns_hist.most_common(25)),
        "in_scope_units": dict(in_scope_units.most_common(40)),
        "template_base_hist_charged": dict(tmpl_hist.most_common(30)),
        "template_base_hist_null": dict(tmpl_hist_null.most_common(30)),
        "top_units": dict(unit_hist.most_common(25)),
        "template_arg_anchor_instrument": {
            "charged": arg_charged,
            "null_at100_same_kind": arg_null,
            "enrichment": enrich,
            "verdict": None,   # filled below
        },
        "census_top60_view": census_view,
    }
    tai = payload["template_arg_anchor_instrument"]
    if enrich is None:
        tai["verdict"] = "NO NULL AVAILABLE -- not quotable"
    elif enrich <= 1.2:
        tai["verdict"] = ("NOT DISCRIMINATING (<=1.2x).  Do not fund.  This is "
                          "the same shape as the unit-stem test at 0.84x and "
                          "the fold-alias model at 1.95x.")
    elif enrich <= 2.0:
        tai["verdict"] = ("WEAK (<=2x).  Explicitly NOT a classifier; the "
                          "project has been burned three times at this "
                          "strength.  Report, do not act.")
    else:
        tai["verdict"] = (f"SUSPICION ONLY at {enrich:.2f}x -- adjudicate every "
                          f"flagged row on retail bytes, never on this label.")

    rows_out = [{k: v for k, v in r.items() if k != "demangled"} | {
        "demangled": r.get("demangled")} for r in noanchor]
    payload["rows"] = sorted(
        rows_out, key=lambda r: -(r["size"] * (100.0 - r["mpn"]) / 100.0))

    if a.jsonout:
        Path(a.jsonout).write_text(json.dumps(payload, indent=1))

    # ---------------- text report ----------------
    w = sys.stdout.write
    w("\n" + "=" * 74 + "\n")
    w("NO_CLASS_ANCHOR PARTITION  (regenerated, never cached)\n")
    w("=" * 74 + "\n")
    p = payload["provenance"]
    w(f"HEAD {p['head'][:12]}   report sha {p['report_sha256'][:12]}\n")
    if a.sabotage:
        w(f"** SABOTAGE ACTIVE: {a.sabotage} -- this run must REFUSE **\n")
    w("\nCONTROLS\n")
    for c in payload["controls"]:
        w(f"  [{'ok  ' if c['ok'] else 'FAIL'}] {c['check']}\n        {c['detail']}\n")
    w("\nPOPULATION (mpn ruler; named rows only)\n")
    for k, v in payload["population"].items():
        w(f"  {k:42s} {v}\n")
    w("\nPARTITION OF THE NO_CLASS_ANCHOR FRONTIER (charged = named sub-100)\n")
    w(f"  {'class':32s} {'rows':>6s} {'%':>6s} {'bytes':>9s} {'penalty_B':>10s}\n")
    tot = len(noanchor)
    for k, v in part_charged.items():
        w(f"  {k:32s} {v['rows']:6d} {100.0*v['rows']/tot:5.1f}% "
          f"{v['bytes']:9d} {v['penalty_bytes']:10.0f}\n")
    w("\nIS THIS FRONTIER EVEN OURS?  (scope split of the same population)\n")
    for k, v in scope_break.items():
        w(f"  {k:44s} {v}\n")
    w("\n  PARTITION RESTRICTED TO IN-SCOPE ROWS\n")
    ti = max(1, len(in_scope))
    for k, v in part_in_scope.items():
        w(f"  {k:32s} {v['rows']:6d} {100.0*v['rows']/ti:5.1f}% "
          f"{v['bytes']:9d} {v['penalty_bytes']:10.0f}\n")
    w("\n  OUTERMOST-NAMESPACE HISTOGRAM (whole no-anchor population)\n")
    for k, v in list(ns_hist.most_common(12)):
        w(f"  {str(k):34s} {v:5d}\n")
    w("\n  SAME PARTITION ON THE UNTREATED NULL (named rows at mpn==100)\n")
    tn = max(1, len(null_noanchor))
    for k, v in part_null.items():
        w(f"  {k:32s} {v['rows']:6d} {100.0*v['rows']/tn:5.1f}%\n")
    w("\nTEMPLATE BASE HISTOGRAM (charged)\n")
    for k, v in list(tmpl_hist.most_common(20)):
        w(f"  {str(k):34s} {v:5d}   (null at100: {tmpl_hist_null.get(k,0)})\n")
    w("\nANCHOR v2 -- REGEX BUG OR REAL WALL?\n")
    w(f"  newly anchored (charged sub-100)   {anchor_v2['newly_anchored_charged']}"
      f"   of which in-scope {anchor_v2['newly_anchored_charged_in_scope']}\n")
    w(f"  newly anchored (null, at 100)      {anchor_v2['newly_anchored_null_at100']}\n")
    w(f"  STILL unanchored, by reason        {anchor_v2['still_unanchored_charged']}\n")
    for tag, c in v2_cal.items():
        w(f"  class-absence recalibrated [{tag}]: charged "
          f"{c['charged']['rate_pct']:.2f}% vs null {c['null_at100']['rate_pct']:.2f}%"
          f"  => enrichment {c['enrichment']}\n")
    w("\nTEMPLATE-ARGUMENT ANCHORING INSTRUMENT\n")
    w(f"  charged : {arg_charged}\n")
    w(f"  null    : {arg_null}\n")
    w(f"  enrichment: {enrich}\n  VERDICT: {tai['verdict']}\n")
    if census_view:
        w("\nDL-2 TOP-60-PER-UNIT BLOCKER VIEW (for continuity)\n")
        w(f"  {census_view['census_no_anchor_blockers']} NO_CLASS_ANCHOR blockers: "
          f"{census_view['partition']}\n  {census_view['note']}\n")
    w("\nTOP UNITS BY NO_CLASS_ANCHOR ROW COUNT (all)\n")
    for k, v in list(unit_hist.most_common(15)):
        w(f"  {k:52s} {v}\n")
    w("\nTOP IN-SCOPE UNITS (source-backed, non-vendor)\n")
    for k, v in list(in_scope_units.most_common(25)):
        w(f"  {k:52s} {v}\n")

    bad = [c for c in payload["controls"] if not c["ok"]]
    if bad:
        print("\nREFUSING(2): control(s) failed -- the partition above is NOT "
              "trustworthy", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
