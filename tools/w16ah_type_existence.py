#!/usr/bin/env python3
"""W16-AH: adjudicate `NO_WITNESS_FOLDED_SIDE` alias memberships by TYPE EXISTENCE.

The retail-side fold witness (`tools/w16ad_fold_witness.py`) declines these rows
for a stated structural reason: the FOLDED spelling's callee `c_N` has no
map-named caller that compares EQ to retail, so no branch can be decoded for it
and neither address can be compared.  W16-AG §9.1 left them open.

This tool adjudicates a SUBSET of them by a different question, which needs no
witness at all:

    If `c_N` is a template instantiated on a class that DOES NOT EXIST in RB3
    retail, then `c_N` was never in any retail COMDAT group, so the membership
    asserts a fold that cannot have happened.

THE CRITERION (all three required; stated so it can fail, and it DOES fail --
see `DECLINED` in the output):

  (1) RTTI:   the type descriptor is ABSENT from retail
              `orig/45410914/band.exe`.  TWO probe forms, and using the wrong
              one is a live false-refutation channel:
                - a NON-template class T is probed as `.?AV<T>@@` / `.?AU<T>@@`;
                - a TEMPLATE is probed by its INSTANTIATION PREFIX `?$<T>@`,
                  because a template's bare name NEVER appears in any binary --
                  retail spells `.?AV?$ObjPtrVec@VSpotlight@@VObjectDir@@@@`.
              ⛔ The first version of this tool probed templates by bare name,
              which is satisfied STRUCTURALLY for every template, and it
              selected 296 rows where ~210 were expected -- it was about to
              refute rows over `ObjPtrVec<Spotlight>` and `ObjPtrVec<RndTex>`,
              whose element types are retail-PRESENT.  The pre-registered row
              count is what caught it.  Template-ness is read off the DEMANGLED
              text (`Name<`), never guessed from the mangling.
  (2) VTABLE: T is POLYMORPHIC in our source -- it declares `virtual` members or
              derives from a class that does.  Required because (1) speaks ONLY
              for polymorphic types: under `/GR` (retail-verified, 2,220 COLs) an
              emitted vtable always carries a Complete Object Locator whose
              `??_R0` holds the `.?AV` string, so for a polymorphic class
              "descriptor absent" == "no vtable emitted" == "no instance ever
              constructed".  A NON-polymorphic class has no vtable and reads
              "absent" whether or not it exists -- `Symbol` is the standing
              example, and this tool declines on exactly that ground.
  (3) ORACLE: T is absent from the rb3-Wii RB3 oracle (`../rb3/src`) and present
              in DC3 (`../dc3-decomp/src`) -- i.e. independently DC3-only.
              rb3-Wii is RB3's OWN dev-build decomp and retains the names retail
              stripped, so it is an instrument on RB3's source, wholly
              independent of the retail bytes probed in (1).

CONTROLS, run before the selection and FATAL if they misbehave:

  * POSITIVE -- the RTTI probe must return PRESENT for polymorphic classes that
    really are in retail, spanning several families (Spotlight/world,
    RndTex/rndobj, ObjectDir/obj, EventTrigger, NoteVoiceInst/synth, Fader,
    Hmx::Object).  A probe that answered "absent" for everything would select
    every row and prove nothing.
  * TEMPLATE-CHANNEL -- the instantiation-prefix probe must return PRESENT for
    at least one polymorphic template in the same family, else that channel is
    vacuous.  `ObjPtrList` supplies it: `?$ObjPtrList@` occurs 45 times in
    retail while `?$ObjPtrVec@` occurs 0, and the two templates share the very
    same `ObjRefOwner` base and shape -- as close a sibling control as exists.
  * NEGATIVE-CAPABILITY -- the criterion must be able to DECLINE.  Two classes
    here are RTTI-absent yet present in the rb3-Wii oracle and absent from DC3
    (`BandPatchMesh`, `HighlightObject`): they are RB3 types.  Half (2) is what
    catches them -- both are plain classes with no base and no `virtual` -- so
    they are declined rather than withdrawn.  If the criterion ever stops
    declining them, it has lost its discriminating power.

⚠ Two probes that LOOK independent were tested and are NOT usable here; they are
  recorded so nobody re-derives them (see the lane doc):
    - a standalone-ASCII-class-name-token scan of retail reads ABSENT for
      `RndTex` and `Fader`, which RTTI proves PRESENT -- false negatives;
    - "no `hamobj/` source-path strings in retail" is VACUOUS: retail carries
      ZERO Harmonix source paths (`rndobj`, `bandobj`, `synth`, `src/system` all
      read 0); its 167 `.cpp` strings are all Quazal NetZ middleware.

Output: a selection JSON keyed on `(survivor, addr)` -- NEVER the census `gi`,
which W16-AD §4.2 measured wrong on 4,785 of 5,315 rows.
"""
import collections
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RETAIL = ROOT / "orig/45410914/band.exe"
WITNESS = ROOT / "docs/decomp/W16AG_fold_witness_2026-09-14.json"
RB3WII = Path("/home/free/code/milohax/rb3/src")
DC3 = Path("/home/free/code/milohax/dc3-decomp/src")

ACCOMPLISHMENT_BAND = "0x825f3"        # W16-AF owns these

POSITIVE_CONTROLS = ["Spotlight", "RndTex", "ObjectDir", "EventTrigger",
                     "NoteVoiceInst", "Fader", "Hmx::Object"]
DECLINE_CONTROLS = ["BandPatchMesh", "HighlightObject"]
# the template channel's own positive control: same family, same base as the
# template it is used to judge.  ("?$ObjPtrList@" = 45 hits, "?$ObjPtrVec@" = 0)
TEMPLATE_CONTROL = "ObjPtrList"

CLASS_RE = re.compile(r'\b(?:class|struct)\s+'
                      r'((?:[A-Za-z_][A-Za-z0-9_]*::)*[A-Za-z_][A-Za-z0-9_]*)')


def demangle(names):
    """One subprocess per symbol: llvm-undname echoes its input before the
    demangling, and a batch call silently MIS-ALIGNS when any entry emits a
    different number of lines."""
    out = {}
    for n in names:
        r = subprocess.run(["llvm-undname", n], capture_output=True, text=True)
        cand = [ln.strip() for ln in r.stdout.splitlines()
                if ln.strip() and ln.strip() != n]
        if not cand:
            sys.exit("REFUSING: llvm-undname could not demangle %s" % n)
        out[n] = cand[-1]
    return out


def mangled_rtti(cls):
    """Hmx::Object -> Object@Hmx@@ (RTTI writes scopes inner-to-outer)."""
    return "@".join(reversed(cls.split("::"))) + "@@"


def probe(img, cls, is_template):
    """Return (hits, probe_string).  A template has no bare-name descriptor in
    ANY binary, so it is probed by its instantiation prefix instead."""
    if is_template:
        p = "?$" + cls.split("::")[-1] + "@"
        return img.count(p.encode()), p
    m = mangled_rtti(cls)
    return (img.count((".?AV" + m).encode()) +
            img.count((".?AU" + m).encode())), ".?AV" + m


class Source:
    """Class declarations from our own headers, for the polymorphism half."""

    def __init__(self, root):
        self.text = {}
        for p in sorted(root.rglob("*.h")):
            try:
                self.text[p] = p.read_text(errors="replace")
            except OSError:
                pass
        self._cache = {}

    def decl(self, cls):
        """Return (base_clause, body) for the real definition (not a forward
        declaration) of `cls`, else None."""
        leaf = cls.split("::")[-1]
        pat = re.compile(r'^(?:class|struct)\s+' + re.escape(leaf) +
                         r'\s*(:[^{;]*)?\{', re.M)
        for _, src in self.text.items():
            m = pat.search(src)
            if not m:
                continue
            i, depth = m.end(), 1
            while i < len(src) and depth:
                if src[i] == '{':
                    depth += 1
                elif src[i] == '}':
                    depth -= 1
                i += 1
            return (m.group(1) or "").strip(), src[m.end():i]
        return None

    def polymorphic(self, cls, depth=0):
        """True if `cls` declares virtual members, or derives from a class that
        does.  The `virtual` of VIRTUAL INHERITANCE lives in the base clause and
        is deliberately not counted -- only the body is scanned."""
        if cls in self._cache:
            return self._cache[cls]
        if depth > 6:
            return False
        self._cache[cls] = False            # cycle guard
        d = self.decl(cls)
        if d is None:
            return False
        base_clause, body = d
        # strip nested class bodies?  not needed: a virtual member of a nested
        # class still makes the enclosing TU emit a vtable only for the nested
        # type, so this is deliberately GENEROUS -- it can only make us DECLINE
        # (a generous polymorphism answer never licenses a withdrawal on its
        # own; half (1) and half (3) still have to hold).
        r = False
        if re.search(r'\bvirtual\b', body):
            r = True
        else:
            for b in re.findall(r'(?:public|protected|private|virtual)\s+'
                                r'((?:[A-Za-z_][A-Za-z0-9_]*::)*'
                                r'[A-Za-z_][A-Za-z0-9_]*)',
                                base_clause):
                if b in ("public", "protected", "private", "virtual"):
                    continue
                if self.polymorphic(b, depth + 1):
                    r = True
                    break
        self._cache[cls] = r
        return r


def oracle_files(root, cls):
    leaf = cls.split("::")[-1]
    if not root.exists():
        return -1
    r = subprocess.run(
        ["grep", "-rlE", r"(class|struct) %s\b" % re.escape(leaf),
         "--include=*.h", "--include=*.cpp", str(root)],
        capture_output=True, text=True)
    return len([x for x in r.stdout.splitlines() if x.strip()])


def main():
    img = RETAIL.read_bytes()
    rows = json.loads(WITNESS.read_text())
    rem = [r for r in rows
           if r["witness_verdict"] == "NO_WITNESS_FOLDED_SIDE"
           and not r["addr"].startswith(ACCOMPLISHMENT_BAND)]
    print("witness rows: %d   NO_WITNESS_FOLDED_SIDE: %d   in scope: %d (%d B)"
          % (len(rows),
             sum(1 for r in rows
                 if r["witness_verdict"] == "NO_WITNESS_FOLDED_SIDE"),
             len(rem), sum(int(r["bytes"]) for r in rem)))

    cn_rows = collections.Counter()
    for r in rem:
        for p in r["pairs"]:
            cn_rows[p["c_N"]] += 1
    dm = demangle(list(cn_rows))
    per_cn = {n: sorted(set(CLASS_RE.findall(d))) for n, d in dm.items()}
    classes = sorted({c for v in per_cn.values() for c in v})
    # template-ness is read off the DEMANGLED text: "Name<" means a template.
    templ = set()
    for d in dm.values():
        for c in CLASS_RE.findall(d):
            leaf = c.split("::")[-1]
            if re.search(re.escape(leaf) + r'\s*<', d):
                templ.add(c)

    src = Source(ROOT / "src")
    ev = {}
    for c in classes:
        present, pstr = probe(img, c, c in templ)
        ev[c] = {
            "is_template": c in templ,
            "rtti_mangled": pstr,
            "retail_rtti_hits": present,
            "polymorphic_in_our_source": src.polymorphic(c),
            "rb3wii_files": oracle_files(RB3WII, c),
            "dc3_files": oracle_files(DC3, c),
        }
        e = ev[c]
        e["dc3_only"] = (e["rb3wii_files"] == 0 and e["dc3_files"] > 0)
        e["qualifies"] = (e["retail_rtti_hits"] == 0 and
                          e["polymorphic_in_our_source"] and e["dc3_only"])

    # ---- controls -------------------------------------------------------
    print("\nCONTROLS")
    bad = []
    for c in POSITIVE_CONTROLS:
        if c not in ev:
            continue
        ok = ev[c]["retail_rtti_hits"] > 0
        print("  POSITIVE  %-16s rtti=%d poly=%s  %s"
              % (c, ev[c]["retail_rtti_hits"],
                 ev[c]["polymorphic_in_our_source"],
                 "OK" if ok else "FAILED"))
        if not ok:
            bad.append(c)
    for c in DECLINE_CONTROLS:
        if c not in ev:
            continue
        ok = not ev[c]["qualifies"]
        print("  DECLINE   %-16s rtti=%d poly=%s rb3wii=%d dc3=%d  %s"
              % (c, ev[c]["retail_rtti_hits"],
                 ev[c]["polymorphic_in_our_source"],
                 ev[c]["rb3wii_files"], ev[c]["dc3_files"],
                 "OK (declined)" if ok else "FAILED (would withdraw)"))
        if not ok:
            bad.append(c)
    tc = ev.get(TEMPLATE_CONTROL)
    if any(ev[c]["is_template"] and ev[c]["qualifies"] for c in ev):
        if tc is None:
            sys.exit("REFUSING: a TEMPLATE qualifies but the template-channel "
                     "control %s is absent from this population" % TEMPLATE_CONTROL)
        ok = tc["is_template"] and tc["retail_rtti_hits"] > 0
        print("  TEMPLATE  %-16s probe=%s hits=%d  %s"
              % (TEMPLATE_CONTROL, tc["rtti_mangled"], tc["retail_rtti_hits"],
                 "OK (channel can return PRESENT)" if ok else "FAILED"))
        if not ok:
            bad.append(TEMPLATE_CONTROL + "(template channel vacuous)")
    if bad:
        sys.exit("REFUSING: control(s) failed: %s" % bad)
    nq = sum(1 for c in ev if ev[c]["qualifies"])
    if nq == 0 or nq == len(ev):
        sys.exit("REFUSING: criterion is vacuous -- %d of %d classes qualify"
                 % (nq, len(ev)))

    print("\nPER-CLASS EVIDENCE (%d classes across %d c_N)"
          % (len(classes), len(cn_rows)))
    w = max(len(c) for c in classes)
    print("  %-*s  tmpl rtti poly rb3wii dc3  verdict" % (w, "class"))
    for c in classes:
        e = ev[c]
        print("  %-*s  %4s %4d %4s %6s %3s  %s"
              % (w, c, "T" if e["is_template"] else "-", e["retail_rtti_hits"],
                 "Y" if e["polymorphic_in_our_source"] else "n",
                 e["rb3wii_files"], e["dc3_files"],
                 "QUALIFIES (DC3-only polymorphic)" if e["qualifies"]
                 else "declined"))

    # ---- per-row selection ---------------------------------------------
    sel, stay = [], []
    for r in rem:
        hit = sorted({c for p in r["pairs"] for c in per_cn[p["c_N"]]
                      if ev[c]["qualifies"]})
        if hit:
            e = dict(r)
            e["qualifying_types"] = hit
            e["type_evidence"] = {c: ev[c] for c in hit}
            sel.append(e)
        else:
            why = collections.Counter()
            for p in r["pairs"]:
                for c in per_cn[p["c_N"]]:
                    e = ev[c]
                    if e["retail_rtti_hits"] > 0:
                        why["type_present_in_retail"] += 1
                    elif not e["polymorphic_in_our_source"]:
                        why["absent_but_NON_POLYMORPHIC(uninformative)"] += 1
                    elif not e["dc3_only"]:
                        why["absent+polymorphic_but_not_dc3_only"] += 1
            x = dict(r)
            x["stay_reasons"] = dict(why)
            stay.append(x)

    print("\nSELECTION")
    print("  WITHDRAW : %4d rows  %6d B" % (len(sel),
                                            sum(int(r["bytes"]) for r in sel)))
    print("  STAY     : %4d rows  %6d B" % (len(stay),
                                            sum(int(r["bytes"]) for r in stay)))
    agg = collections.Counter()
    for r in stay:
        agg.update(r["stay_reasons"])
    print("  stay reasons: %s" % dict(agg))
    byt = collections.Counter()
    for r in sel:
        for c in r["qualifying_types"]:
            byt[c] += 1
    print("  withdraw rows by qualifying type: %s" % dict(byt.most_common()))

    if len(sys.argv) > 1:
        Path(sys.argv[1]).write_text(json.dumps(sel, indent=1) + "\n")
        print("  wrote %s" % sys.argv[1])
    if len(sys.argv) > 2:
        Path(sys.argv[2]).write_text(
            json.dumps({"class_evidence": ev, "per_cn_classes": per_cn,
                        "cn_rowcount": dict(cn_rows), "demangled": dm,
                        "stay": stay}, indent=1) + "\n")
        print("  wrote %s" % sys.argv[2])


if __name__ == "__main__":
    main()
