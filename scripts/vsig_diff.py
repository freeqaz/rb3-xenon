#!/usr/bin/env python3
"""Three-way virtual-signature differ (ours / dc3-decomp / rb3-Wii).

Scans headers shared (same relative path) between this tree and the two
oracle trees, extracts every `virtual` method declaration, normalizes the
signature (param-name-insensitive, class/struct/enum + Hmx:: qualification
stripped), and prints every method whose signature differs from rb3-Wii's.

Tags:
  [OURS==DC3,WII-DIFF]  ours matches dc3, Wii differs (dc3-drift candidates)
  [3WAY]                all three differ
  [no-dc3]              dc3 has no such header/class/method

This is the "dc3-drift signature audit" tool (closeout25 a1 / closeout26 a2).
IMPORTANT CALIBRATION (verified over two rounds, 2026-07-10/11):
  - "retail follows rb3-Wii" is NOT a rule. RB3-360 retail is TU5-era and
    follows dc3's newer shape at least as often (MCResult family,
    PropKeys::SetFrame/Load, SetADSR/ADSRImpl, GetFileHandle, hash_map
    AddSongData, TextForNode...). EVERY flip must be verified against retail
    codegen (Ghidra call-site/body ABI or objdiff pairing) BEFORE testing.
  - A flag is only *testable* if an affected override/caller has a MAPPED
    retail pairing (near-miss in report.json). Rank by that first.
See docs/decomp/research/vsig-flags-2026-07-11.md for the audited flag list.

Usage:
  python3 scripts/vsig_diff.py [--root DIR] [--wii DIR] [--dc3 DIR] [--subdirs system,band3]
"""
import argparse
import glob
import os
import re
from collections import defaultdict


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)
    return text


def norm_type(t):
    t = t.strip()
    t = re.sub(r"\b(class|struct|enum)\b", "", t)
    t = re.sub(r"\bHmx::", "", t)  # Hmx:: qualification noise
    t = re.sub(r"\s+", " ", t).strip()
    t = re.sub(r"\s*([*&<>,])\s*", r"\1", t)
    return t


def split_params(params):
    # split on commas at depth 0 (angle brackets / parens)
    out, depth, cur = [], 0, ""
    for ch in params:
        if ch in "<(":
            depth += 1
        elif ch in ">)":
            depth -= 1
        if ch == "," and depth == 0:
            out.append(cur)
            cur = ""
        else:
            cur += ch
    if cur.strip():
        out.append(cur)
    return out


def norm_param(p):
    p = p.strip()
    if not p or p == "void":
        return None
    p = p.split("=")[0].strip()  # drop default value
    # strip trailing identifier (param name) heuristically
    m = re.match(r"^(.*?[\s*&>])([A-Za-z_]\w*)$", p)
    if m:
        base = m.group(1).strip()
        if base and base not in (
            "const", "unsigned", "signed", "long", "short",
            "struct", "class", "enum",
        ):
            p = base
    return norm_type(p)


def parse_sig(decl):
    d = " ".join(decl.split())
    d = re.sub(r"^virtual\s+", "", d)
    d = re.sub(r"\s*=\s*0\s*$", "", d)
    m = re.search(r"(operator\s*[^\s(]+|~?[A-Za-z_]\w*)\s*\(", d)
    if not m:
        return None
    name = m.group(1).replace(" ", "")
    if name in ("if", "while", "for", "switch", "MILO_ASSERT"):
        return None
    ret = d[: m.start()].strip()
    op = d.index("(", m.start())
    depth = 0
    cl = None
    for i in range(op, len(d)):
        if d[i] == "(":
            depth += 1
        elif d[i] == ")":
            depth -= 1
            if depth == 0:
                cl = i
                break
    if cl is None:
        return None
    params = d[op + 1 : cl]
    tail = d[cl + 1 :].strip()
    const = "const" in tail.split()
    ps = [norm_param(p) for p in split_params(params)]
    ps = [p for p in ps if p]
    return name, (norm_type(ret), tuple(ps), const)


def extract(path):
    """Parse a header: {class: {method: normalized_sig}} for virtual decls."""
    if not os.path.exists(path):
        return None
    text = strip_comments(open(path, errors="replace").read())
    n = len(text)
    heads = {}
    for m in re.finditer(r"\b(class|struct)\s+([A-Za-z_]\w*)(?:\s*:\s*[^{;]+)?\s*\{", text):
        b = text.index("{", m.start())
        heads[b] = m.group(2)
    out = defaultdict(dict)
    stack = []
    decl = None
    i = 0

    def curclass():
        for c in reversed(stack):
            if c:
                return c
        return None

    while i < n:
        ch = text[i]
        if ch == "{":
            if decl is not None:
                d2 = 1
                j = i + 1
                while j < n and d2:
                    if text[j] == "{":
                        d2 += 1
                    elif text[j] == "}":
                        d2 -= 1
                    j += 1
                r = parse_sig(decl)
                if r and curclass():
                    out[curclass()][r[0]] = r[1]
                decl = None
                i = j
                continue
            stack.append(heads.get(i))
        elif ch == "}":
            if stack:
                stack.pop()
        elif decl is None:
            if (
                text.startswith("virtual", i)
                and (i == 0 or not (text[i - 1].isalnum() or text[i - 1] == "_"))
                and i + 7 < n
                and not (text[i + 7].isalnum() or text[i + 7] == "_")
            ):
                decl = ""
        if decl is not None:
            if ch == ";":
                r = parse_sig(decl)
                if r and curclass():
                    out[curclass()][r[0]] = r[1]
                decl = None
            else:
                decl += ch
        i += 1
    return out


def fmt(sig):
    ret, ps, const = sig
    return f"{ret} ({','.join(ps)}){' const' if const else ''}"


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    ap.add_argument("--root", default=os.path.join(repo, "src"))
    ap.add_argument("--wii", default=os.path.expanduser("~/code/milohax/rb3/src"))
    ap.add_argument("--dc3", default=os.path.expanduser("~/code/milohax/dc3-decomp/src"))
    ap.add_argument("--subdirs", default="system,band3",
                    help="comma-separated subtrees to scan (shared relative paths)")
    args = ap.parse_args()

    rows = []
    for sub in args.subdirs.split(","):
        wt = os.path.join(args.root, sub)
        for p in sorted(glob.glob(wt + "/**/*.h", recursive=True)):
            rel = os.path.relpath(p, wt)
            wii_p = os.path.join(args.wii, sub, rel)
            if not os.path.exists(wii_p):
                continue
            ours = extract(p)
            wii = extract(wii_p)
            dc3 = extract(os.path.join(args.dc3, sub, rel))
            for cls, methods in ours.items():
                wcls = wii.get(cls, {})
                for name, sig in methods.items():
                    wsig = wcls.get(name)
                    if wsig is None or wsig == sig:
                        continue
                    dsig = dc3.get(cls, {}).get(name) if dc3 else None
                    tag = (
                        "OURS==DC3,WII-DIFF" if dsig == sig
                        else ("3WAY" if dsig else "no-dc3")
                    )
                    rows.append((sub + "/" + rel, cls, name, tag, sig, wsig, dsig))

    for h, cls, name, tag, sig, wsig, dsig in rows:
        print(f"=== {h} :: {cls}::{name}  [{tag}]")
        print(f"  ours: {fmt(sig)}")
        print(f"  wii : {fmt(wsig)}")
        if dsig and dsig != sig:
            print(f"  dc3 : {fmt(dsig)}")
    print(f"\n{len(rows)} semantic virtual-signature diffs")


if __name__ == "__main__":
    main()
