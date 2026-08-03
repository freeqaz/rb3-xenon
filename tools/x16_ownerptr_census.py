#!/usr/bin/env python3
"""
x16_ownerptr_census.py — enumerate the "null means me" ObjOwnerPtr class.

THE DEFECT CLASS (X15 §3.4)
---------------------------
A member declared `ObjOwnerPtr<T> mX;`, seeded in the ctor init-list as
`mX(this, this)`, carries the invariant "mX is NEVER null; null means me".
The ONLY place that invariant is re-established after the referent dies is the
owning class's `Replace()` override, which does `if (!mX.SetObj(obj)) mX = this;`.

Native `~ObjectDir` Phase 0 (obj/Dir.cpp:119-135, HX_NATIVE-only) tears the ring
down via `NullifyAllRefs()` -> `ObjRef::NullifyObj()`, which by documented design
stores `mObject = nullptr` and does NOT fire `Replace()`. So every such member is
left NULL, violating an invariant the rest of the class dereferences blindly.
Retail X360's ring teardown DOES fire Replace, so this is native-only.

WHAT THIS INSTRUMENT REPORTS
----------------------------
For every `(this, this)`-seeded owning pointer in src/:
  EXPOSED         seeded (this,this) AND restored in Replace()  -> in the class
  SEEDED_NO_REPL  seeded (this,this) but NO Replace() restore    -> not in the class
  NOT_SELF_SEEDED an ObjOwnerPtr that is not seeded (this,this)  -> not in the class

CONTROLS (the instrument REFUSES to report if these fail)
---------------------------------------------------------
POSITIVE: CharWeightable::mWeightOwner and Character::mSphereBase are the two
sites X15 found by walking into them (gdb-confirmed NULL, both fixed). An
enumeration that does not rediscover BOTH cannot be trusted to find a third.

NEGATIVE: DirLoader::mProxyDir is an ObjOwnerPtr that is NOT seeded (this,this)
-- it holds a foreign dir and has no "null means me" invariant. An instrument
that reports it is over-matching on the type alone.

Both controls are checked in --self-test and the script exits non-zero on failure,
so an empty or malformed run cannot be read as "no further sites" (X15 §6.2:
silence is not a result).
"""

import os
import re
import sys
import json

SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src")
SRC = os.path.normpath(SRC)

# Members seeded as `mX(this, this)` in a ctor init-list. The owning-pointer
# ctor is ObjOwnerPtr(ObjRefOwner *owner, T *ptr) -- so (this, this) means
# "owned by me, and pointing at me".
SELF_SEED = re.compile(r"\b(m[A-Za-z0-9_]+)\s*\(\s*this\s*,\s*this\s*\)")
# Any owning-pointer member declaration.
OWNER_DECL = re.compile(r"\bObjOwnerPtr\s*<\s*([^;>]+?)\s*>\s*(m[A-Za-z0-9_]+)\s*;")


def walk(exts):
    for root, _dirs, files in os.walk(SRC):
        for f in files:
            if f.endswith(exts):
                yield os.path.join(root, f)


def _blank(m):
    """Replace a comment/string with same-length whitespace, preserving newlines
    so every byte offset and line number in the original text still holds."""
    return "".join(c if c == "\n" else " " for c in m.group(0))


COMMENTS = re.compile(
    r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'', re.S
)


def read(p):
    """Read a TU with comments and string literals blanked out.

    Comment pollution broke the first version of this instrument: the doc
    comment in CharWeightable.h that *quotes* `mWeightOwner(this, this)` was
    matched as a real ctor seed, and stray words after `class` inside prose
    ("...class already...") were picked up as class names. The controls caught
    it. Blanking preserves offsets so line numbers stay exact.
    """
    with open(p, "r", encoding="utf-8", errors="replace") as fh:
        return COMMENTS.sub(_blank, fh.read())


CLASS_HEAD = re.compile(
    r"\b(?:class|struct)\s+(?:[A-Z_][A-Z0-9_]*\s+)?([A-Za-z_][A-Za-z0-9_]*)"
    r"\s*(?::[^{;]*)?\{"
)


def class_scopes(text):
    """[(class_name, body_start, body_end)] via real brace matching.

    `class Foo;` forward declarations are skipped (no `{`), and nesting is
    handled by taking the INNERMOST enclosing scope at a given offset.
    """
    scopes = []
    for m in CLASS_HEAD.finditer(text):
        b = m.end() - 1  # the '{'
        depth, i = 0, b
        while i < len(text):
            if text[i] == "{":
                depth += 1
            elif text[i] == "}":
                depth -= 1
                if depth == 0:
                    break
            i += 1
        scopes.append((m.group(1), b, i))
    return scopes


def enclosing_class(scopes, pos):
    """Innermost class scope containing pos, or None (file scope)."""
    best = None
    for name, b, e in scopes:
        if b < pos < e:
            if best is None or b > best[1]:
                best = (name, b)
    return best[0] if best else None


def scan():
    # 1. every ObjOwnerPtr member declaration, by (class, member)
    decls = {}  # (cls, member) -> (file, line, T)
    for p in walk((".h", ".hpp", ".cpp")):
        text = read(p)
        scopes = class_scopes(text)
        for m in OWNER_DECL.finditer(text):
            T, member = m.group(1).strip(), m.group(2)
            cls = enclosing_class(scopes, m.start())
            line = text[: m.start()].count("\n") + 1
            decls[(cls, member)] = (os.path.relpath(p, SRC), line, T)

    # 2. every `mX(this, this)` ctor seed, attributed to the ctor's class
    seeds = {}  # (cls, member) -> (file, line)
    for p in walk((".cpp", ".h")):
        text = read(p)
        scopes = class_scopes(text)
        for m in SELF_SEED.finditer(text):
            member = m.group(1)
            line = text[: m.start()].count("\n") + 1
            # Out-of-line ctor `Foo::Foo(...)` -> Foo; in-class ctor -> the
            # enclosing class scope. Prefer whichever actually DECLARES the
            # member, so a member seeded in a base's ctor still attributes right.
            head = text[: m.start()]
            ctor = None
            for c in re.finditer(r"\b([A-Za-z_][A-Za-z0-9_]*)\s*::\s*\1\s*\(", head):
                ctor = c.group(1)
            inline_cls = enclosing_class(scopes, m.start())
            cands = [c for c in (ctor, inline_cls) if c]
            cls = next((c for c in cands if (c, member) in decls), ctor or inline_cls)
            seeds[(cls, member)] = (os.path.relpath(p, SRC), line)

    # 3. does the class's Replace() restore `mX = this`?
    #    (search the whole TU that defines Replace for this class)
    restores = {}  # (cls, member) -> (file, line)
    for p in walk((".cpp", ".h")):
        text = read(p)
        for rm in re.finditer(
            r"\bvoid\s+([A-Za-z_][A-Za-z0-9_]*)\s*::\s*Replace\s*\(", text
        ):
            cls = rm.group(1)
            # crude brace-match for the Replace body
            b = text.find("{", rm.end())
            if b < 0:
                continue
            depth, i = 0, b
            while i < len(text):
                if text[i] == "{":
                    depth += 1
                elif text[i] == "}":
                    depth -= 1
                    if depth == 0:
                        break
                i += 1
            body = text[b : i + 1]
            # The restore is spelled at least four different ways in this tree,
            # and a narrower regex produced SEVEN false negatives that the
            # positive controls did NOT catch (both controls happen to use the
            # `= this` spelling). Verified spellings:
            #   mX = this;                          CharWeightable.cpp:12
            #   mX.SetObjConcrete(this)             EnvAnim.cpp:29
            #   mX.SetObj(this)
            #   RndFont *r = this; ... mX = r;      Font.cpp:167-176 (via local)
            # So: find the member's RefIs() guard arm and ask whether that arm
            # can write `this` into the member. Every hit is listed with its
            # evidence line for manual audit -- this is a screen, not an oracle.
            for gm in re.finditer(r"RefIs\s*\(\s*\w+\s*,\s*(m[A-Za-z0-9_]+)\s*\)", body):
                member = gm.group(1)
                arm = body[gm.end():]
                # the guard arm ends at its `return;`
                stop = arm.find("return")
                arm = arm[: stop if stop >= 0 else len(arm)]
                pats = [
                    (rf"\b{member}\s*=\s*this\b", "= this"),
                    (rf"\b{member}\s*\.\s*SetObjConcrete\s*\(\s*this\b", ".SetObjConcrete(this)"),
                    (rf"\b{member}\s*\.\s*SetObj\s*\(\s*this\b", ".SetObj(this)"),
                    (rf"=\s*this\s*;", "local = this (indirect)"),
                ]
                for pat, how in pats:
                    am = re.search(pat, arm)
                    if am:
                        line = text[: b + gm.end() + am.start()].count("\n") + 1
                        restores[(cls, member)] = (os.path.relpath(p, SRC), line, how)
                        break

    rows = []
    keys = set(decls) | set(seeds)
    for k in sorted(keys, key=lambda x: (str(x[0]), x[1])):
        cls, member = k
        d = decls.get(k)
        s = seeds.get(k)
        r = restores.get(k)
        if d is None:
            continue  # a (this,this) seed on a non-ObjOwnerPtr member
        if s is None:
            verdict = "NOT_SELF_SEEDED"
        elif r is None:
            verdict = "SEEDED_NO_REPL"
        else:
            verdict = "EXPOSED"
        rows.append(
            {
                "class": cls,
                "member": member,
                "T": d[2],
                "decl": f"{d[0]}:{d[1]}",
                "seed": f"{s[0]}:{s[1]}" if s else None,
                "replace_restore": f"{r[0]}:{r[1]} ({r[2]})" if r else None,
                "verdict": verdict,
            }
        )
    return rows


POSITIVE_CONTROLS = [
    # X15's two gdb-confirmed sites. Both spell the restore `mX = this;`.
    ("CharWeightable", "mWeightOwner"),
    ("Character", "mSphereBase"),
    # SPELLING control, added after the `= this`-only regex produced 7 false
    # negatives that the two controls above sailed past: this site restores via
    # `mKeysOwner.SetObjConcrete(this)` (EnvAnim.cpp:29). A detector that only
    # understands assignment misclassifies it SEEDED_NO_REPL.
    ("RndEnvAnim", "mKeysOwner"),
    # LOCAL-INDIRECTION control: RndFont restores through a local
    # (`replace = this; ... mTextureOwner = replace;`), Font.cpp:165-176.
    ("RndFont", "mTextureOwner"),
]
NEGATIVE_CONTROLS = [("DirLoader", "mProxyDir")]


def self_test(rows):
    ok = True
    idx = {(r["class"], r["member"]): r for r in rows}
    print("== CONTROLS ==")
    if not rows:
        print("  FAIL: instrument produced ZERO rows -- refusing to report.")
        return False
    for c in POSITIVE_CONTROLS:
        r = idx.get(c)
        if r is None:
            print(f"  FAIL (positive): {c[0]}::{c[1]} NOT FOUND by the instrument")
            ok = False
        elif r["verdict"] != "EXPOSED":
            print(f"  FAIL (positive): {c[0]}::{c[1]} classified {r['verdict']}, want EXPOSED")
            ok = False
        else:
            print(f"  pass (positive): {c[0]}::{c[1]} -> EXPOSED  seed={r['seed']} replace={r['replace_restore']}")
    for c in NEGATIVE_CONTROLS:
        r = idx.get(c)
        if r is None:
            print(f"  FAIL (negative): {c[0]}::{c[1]} not seen at all -- cannot prove it was REJECTED")
            ok = False
        elif r["verdict"] == "EXPOSED":
            print(f"  FAIL (negative): {c[0]}::{c[1]} wrongly classified EXPOSED")
            ok = False
        else:
            print(f"  pass (negative): {c[0]}::{c[1]} -> {r['verdict']} (correctly excluded)")
    return ok


def main():
    rows = scan()
    ok = self_test(rows)
    print()
    print("== CENSUS ==")
    for v in ("EXPOSED", "SEEDED_NO_REPL", "NOT_SELF_SEEDED"):
        sub = [r for r in rows if r["verdict"] == v]
        print(f"\n-- {v}: {len(sub)}")
        for r in sub:
            print(f"   {r['class']}::{r['member']}  <{r['T']}>")
            print(f"      decl={r['decl']}  seed={r['seed']}  replace={r['replace_restore']}")
    print(f"\nTOTAL ObjOwnerPtr members: {len(rows)}")
    if "--json" in sys.argv:
        out = os.environ.get("X16_JSON", "/home/free/tmp/laneX16/evidence/x16-census.json")
        with open(out, "w") as fh:
            json.dump(rows, fh, indent=2)
        print(f"json -> {out}")
    if not ok:
        print("\nINSTRUMENT CONTROL FAILURE -- results above are NOT trustworthy.")
        sys.exit(2)
    print("\nControls pass; census is trustworthy.")


if __name__ == "__main__":
    main()
