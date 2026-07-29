#!/usr/bin/env python3
"""handler_list_diff.py -- for every frame-drift candidate that is an
`X::Handle` (or any BEGIN_HANDLERS body), diff OUR handler-symbol chain against
the handler-name strings the RETAIL body actually references.

Consumes handler_drift_scan.py's --json output.  For each candidate:
  * retail list  <- retail_handler_strings.py machinery (string constants in the
                    dtk-split target listing, resolved against orig/.../band.exe)
  * our list     <- the BEGIN_HANDLERS(<Class>) block in our source (preprocessor-
                    inactive arms such as `#ifdef HX_NATIVE` are dropped)
  * verdict      <- SURPLUS (we have handlers retail lacks) / MISSING / EXACT

Only SURPLUS with the right slot arithmetic is the proven lever; EXACT means the
frame drift has some other cause and should NOT be blamed on handlers.
"""
import json
import os
import re
import sys

PROJ = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from retail_handler_strings import load_image, cstr  # noqa: E402

# NOTE: the first argument is frequently on the NEXT line (clang-format wraps long
# HANDLE_ACTION/HANDLE_EXPR bodies), so this must be matched over joined text, not
# line-by-line -- a line-anchored version produced false MISSING reports.
MAC = re.compile(r"\b(HANDLE|HANDLE_EXPR|HANDLE_ACTION|HANDLE_ACTION_IF|"
                 r"HANDLE_ACTION_IF_ELSE|HANDLE_EXPR_STATIC|HANDLE_ACTION_STATIC)"
                 r"\s*\(\s*([A-Za-z_]\w*)\s*,", re.S)


def our_handlers(path, cls):
    """ordered (macro, symbol) handlers of BEGIN_HANDLERS(cls), honouring #ifdef"""
    try:
        lines = open(path, errors="replace").read().splitlines()
    except OSError:
        return None
    on = False
    dead = 0
    stack = []
    live = []
    for ln in lines:
        if re.match(r"\s*BEGIN_HANDLERS\(\s*%s\s*\)" % re.escape(cls), ln):
            on = True
            continue
        if not on:
            continue
        if re.match(r"\s*END_HANDLERS", ln):
            break
        s = ln.strip()
        if s.startswith("#if"):
            # HX_NATIVE / MILO_DEBUG arms are NOT in the match build
            bad = ("HX_NATIVE" in s) or (s.startswith("#ifdef MILO_DEBUG"))
            stack.append(bad)
            if bad:
                dead += 1
            continue
        if s.startswith("#else"):
            if stack:
                b = stack[-1]
                dead += 1 if not b else -1
                stack[-1] = not b
            continue
        if s.startswith("#endif"):
            if stack and stack.pop():
                dead -= 1
            continue
        if dead:
            continue
        live.append(ln)
    if not on:
        return None
    return [(m.group(1), m.group(2)) for m in MAC.finditer("\n".join(live))]


def retail_strings(unit, fnaddr, img):
    d, secs = img
    sp = os.path.join(PROJ, "build/45410914/asm/%s.s" % unit.split("/")[-1])
    if not os.path.exists(sp):
        return None
    fn = "fn_" + fnaddr[2:].upper()
    on = False
    seen, order = set(), []
    for line in open(sp, errors="replace"):
        if line.startswith(".fn %s," % fn):
            on = True
            continue
        if on and line.startswith(".endfn"):
            break
        if not on:
            continue
        for lbl in re.findall(r"lbl_([0-9A-F]{8})", line):
            va = int(lbl, 16)
            if va in seen:
                continue
            seen.add(va)
            s = cstr(d, secs, va)
            # handler names are lowercase identifiers; require >=3 chars so the
            # 2-char garbage that survives the printable test ("ak", "8!") is not
            # mistaken for a retail handler we are missing.
            if s and len(s) >= 3 and re.fullmatch(r"[a-z_][a-z0-9_]*", s):
                order.append(s)
    return order if on else None


def main():
    rows = json.load(open(sys.argv[1]))
    m = json.load(open(os.path.join(PROJ, "scripts/target_symbol_map.json")))
    rev = {}
    for k, v in m.items():
        for x in v if isinstance(v, list) else [v]:
            rev.setdefault(x, k)
    img = load_image(os.path.join(PROJ, "orig/45410914/band.exe"))

    for r in rows:
        mm = re.match(r"\?Handle@(\w+)@@", r["func"])
        if not mm:
            continue
        cls = mm.group(1)
        addr = rev.get(r["func"])
        if not addr:
            continue
        rl = retail_strings(r["unit"], addr, img)
        ours = our_handlers(os.path.join(PROJ, r["src"] or ""), cls) if r.get("src") else None
        if rl is None or ours is None:
            continue
        rs, os_ = set(rl), set(s for _, s in ours)
        surplus = [(mac, h) for mac, h in ours if h not in rs]
        missing = [h for h in rl if h not in os_]
        verdict = "EXACT" if not surplus and not missing else (
            "SURPLUS" if surplus and not missing else
            "MISSING" if missing and not surplus else "BOTH")
        # only plain HANDLE() expands _HANDLE_CHECKED -> a `DataNode result` temp,
        # i.e. one 8-byte frame slot.  HANDLE_EXPR/HANDLE_ACTION return directly.
        costly = sum(1 for mac, _ in surplus if mac == "HANDLE")
        print("%-9s casc=%-3d delta=%+5d (%+d slots)  costlySurplus=%d  "
              "ours=%s retail=%s  %s :: %s"
              % (verdict, r["cascade"], r["delta"], r["slots"], costly,
                 r["our_frame"], r["retail_frame"], r["unit"], cls))
        if surplus:
            print("      SURPLUS(%d): %s" % (
                len(surplus), ", ".join("%s[%s]" % (h, mac) for mac, h in surplus)))
        if missing:
            print("      MISSING(%d): %s" % (len(missing), ", ".join(missing)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
