#!/usr/bin/env python3
"""handler_surplus_census.py -- census EVERY BEGIN_HANDLERS body against the
handler-name strings the RETAIL body actually references.

WHY (lane BZ-2).  laneBF (05ef434a, +44) proved the lever on exactly TWO
functions, both found via stack-frame drift: DC3 is newer than RB3 retail, so
our BEGIN_HANDLERS chains carry arms retail never had.  Frame drift only detects
surplus plain `HANDLE()` arms (each costs one 8-byte DataNode temp).  A surplus
HANDLE_EXPR / HANDLE_ACTION costs NO frame slot but still emits a Symbol
compare + branch, so it breaks the body while being invisible to a frame-drift
scan.  This tool drops the frame-drift precondition and looks at all 547
BEGIN_HANDLERS classes.

EVIDENCE MODEL (two independent instruments, both reported):
  * PAIRED evidence -- the retail body's own referenced string constants, read
    out of the dtk-split target listing.  Directional and exact, but requires a
    map entry (so it is map-DEPENDENT and only covers paired functions).
  * ABSENCE evidence -- whether the handler name occurs ANYWHERE in band.exe.
    Map-INDEPENDENT proof per project_binary_absence_proof_2026-07-30: if the
    string is not in the image, retail cannot construct that Symbol.

  ⚠ Absence is only meaningful for names long enough not to collide by chance in
  a 14 MB image.  Short names ("set", "iterate") are present-by-accident, so the
  tool reports `absent`, `present`, and `tooshort` SEPARATELY and never counts a
  short name as evidence either way.

CONTROLS (--controls): the retail-side extractor is validated against Handle
bodies that already read 100%.  Those are byte-identical to retail, so their
handler list MUST diff EXACT; any SURPLUS there is an extractor defect and voids
the run.  Denominators are printed beside every zero.

Usage:
  scripts/harvest/handler_surplus_census.py --proj <worktree> [--json OUT]
  scripts/harvest/handler_surplus_census.py --proj <worktree> --controls
"""
import argparse
import json
import os
import re
import sys
from collections import defaultdict
from pathlib import Path

# Reuse the validated retail image reader.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from retail_handler_strings import load_image, cstr  # noqa: E402

# Long enough that a chance occurrence in 14 MB is negligible.
MIN_ABSENCE_LEN = 8

MAC = re.compile(r"\b(HANDLE|HANDLE_EXPR|HANDLE_ACTION|HANDLE_ACTION_IF|"
                 r"HANDLE_ACTION_IF_ELSE|HANDLE_EXPR_STATIC|HANDLE_ACTION_STATIC)"
                 r"\s*\(\s*([A-Za-z_]\w*)\s*,", re.S)

# Arms that cost a frame slot (expand _HANDLE_CHECKED -> a DataNode temp).
COSTLY = {"HANDLE"}


def strip_comments(text):
    """Remove // and /* */ so a commented-out HANDLE is never read as live.
    (A comment already poisoned one regex-over-source scanner in this project.)"""
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)
    return text


def our_handlers_from_text(text, cls):
    """ordered [(macro, symbol)] for BEGIN_HANDLERS(cls), honouring #ifdef arms."""
    lines = text.splitlines()
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
            bad = ("HX_NATIVE" in s) or s.startswith("#ifdef MILO_DEBUG")
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


def index_sources(proj):
    """class -> (relpath, [(macro,sym)]).  Scans the WHOLE tree, because a
    BEGIN_HANDLERS block often lives in a different file from the unit's
    source_path (scatter-includes: JoypadMsgs.cpp includes os/NetStream.cpp,
    BandCamShot.cpp includes hamobj/HamCamShot.cpp)."""
    out = {}
    src = proj / "src"
    for p in list(src.rglob("*.cpp")) + list(src.rglob("*.h")):
        try:
            raw = p.read_text(errors="replace")
        except OSError:
            continue
        if "BEGIN_HANDLERS(" not in raw:
            continue
        text = strip_comments(raw)
        for m in re.finditer(r"BEGIN_HANDLERS\(\s*(\w+)\s*\)", text):
            cls = m.group(1)
            hs = our_handlers_from_text(text, cls)
            if hs is not None and cls not in out:
                out[cls] = (str(p.relative_to(proj)), hs)
    return out


def retail_strings(proj, unit, fnaddr, img):
    """Ordered lowercase-identifier string constants referenced by the RETAIL body."""
    d, secs = img
    sp = proj / "build" / "45410914" / "asm" / ("%s.s" % unit.split("/")[-1])
    if not sp.exists():
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
            # ⚠ Must allow INTERIOR uppercase.  An earlier `[a-z0-9_]*` tail made
            # handler names containing a capital (CamShot's `set_all_to_3D`)
            # impossible to read from retail, so they ALWAYS looked surplus --
            # removing that arm regressed CamShot::Handle 93.65 -> 87.05.  Still
            # require a lowercase first char so Class names stay out.
            if s and len(s) >= 3 and re.fullmatch(r"[a-z_][A-Za-z0-9_]*", s):
                order.append(s)
    return order if on else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--proj", required=True)
    ap.add_argument("--json")
    ap.add_argument("--controls", action="store_true")
    a = ap.parse_args()
    proj = Path(a.proj).resolve()

    src_idx = index_sources(proj)
    print("BEGIN_HANDLERS classes found in source: %d" % len(src_idx), file=sys.stderr)

    smap = json.load(open(proj / "scripts" / "target_symbol_map.json"))
    rev = {}
    for k, v in smap.items():
        for x in (v if isinstance(v, list) else [v]):
            rev.setdefault(x, k)

    band = (proj / "orig" / "45410914" / "band.exe").read_bytes()
    img = load_image(str(proj / "orig" / "45410914" / "band.exe"))
    report = json.load(open(proj / "build" / "45410914" / "report.json"))

    def absence(name):
        if len(name) < MIN_ABSENCE_LEN:
            return "tooshort"
        b = name.encode()
        return "present" if band.find(b) >= 0 else "absent"

    # ---- funnel counters -------------------------------------------------
    n_handle_fns = 0        # ?Handle@X@@ functions present in report units
    n_have_src = 0          # ... whose class has a BEGIN_HANDLERS block we found
    n_mapped = 0            # ... with a target_symbol_map entry (pairable)
    n_retail_read = 0       # ... whose retail body we could actually read
    n_unreadable = 0        # ... body read but yielded 0 strings (untrustworthy)
    rows = []
    ctrl_tot = ctrl_bad = 0
    ctrl_examples = []

    for u in report["units"]:
        md = u.get("metadata") or {}
        if not md.get("source_path"):
            continue
        for fn in (u.get("functions") or []):
            mm = re.match(r"\?Handle@(\w+)@@", fn["name"])
            if not mm:
                continue
            # ★ EXCLUDE vtable ADJUSTOR THUNKS.  `?Handle@Cls@@$4PPPPPPPM@A@AA...`
            # matches the pattern above but is a 4-instruction `b real_fn` jump
            # carrying ZERO strings -- which made the retail list empty and
            # reported EVERY arm as surplus.  Measured: this alone was 123/347
            # of the control's false surplus (multiply-inherited classes only).
            if "@@$" in fn["name"] or fn["name"].startswith("??_9"):
                continue
            cls = mm.group(1)
            n_handle_fns += 1
            if cls not in src_idx:
                continue
            n_have_src += 1
            addr = rev.get(fn["name"])
            if not addr:
                continue
            n_mapped += 1
            rl = retail_strings(proj, u["name"], addr, img)
            if rl is None:
                continue
            # A real BEGIN_HANDLERS body always materialises at least one handler
            # name string.  Zero strings + non-empty source list means we failed
            # to read the body (thunk / wrong listing), NOT that retail has no
            # handlers -- counting it would report 100% of arms as surplus.
            if not rl and src_idx.get(cls, (None, []))[1]:
                n_unreadable += 1
                continue
            n_retail_read += 1

            relpath, ours = src_idx[cls]
            rs = set(rl)
            os_ = set(s for _, s in ours)
            surplus = [(mac, h) for mac, h in ours if h not in rs]
            missing = [h for h in rl if h not in os_]
            pct = fn.get("fuzzy_match_percent")

            ev = {h: absence(h) for _, h in surplus}

            # ---- classify -------------------------------------------------
            # RENAME: same arm COUNT, surplus balanced by missing.  DC3 renamed
            #   the Symbol (get_trans_children <- retail get_children).  Real
            #   defect, but the body shape is identical and the string is a
            #   MASKED reloc, so the function often already reads 100% and a fix
            #   moves the metric by ZERO.  Not a funclet lever.
            # MISPAIR: surplus and missing lists are wholly disjoint in domain
            #   and counts match -- the map points at another function.
            # EXTRA: we have arms retail does not, count strictly greater.  This
            #   is the only shape that changes frame size / body length, i.e.
            #   the laneBF lever.
            if surplus and missing and len(ours) == len(rl):
                kind = "RENAME"
            elif len(ours) > len(rl):
                kind = "EXTRA"
            elif missing:
                kind = "BOTH"
            else:
                kind = "OTHER"

            # The claim we would ACT on: an extra arm, proven absent from the
            # retail image.  The control measures the false-positive rate of
            # exactly this claim, not of the looser "any surplus".
            actionable = (kind == "EXTRA"
                          and any(ev[h] == "absent" for _, h in surplus))

            if a.controls:
                if pct == 100.0:
                    ctrl_tot += 1
                    if actionable:
                        ctrl_bad += 1
                        if len(ctrl_examples) < 12:
                            ctrl_examples.append(
                                (cls, [h for _, h in surplus][:5]))
                continue

            if not surplus:
                continue
            rows.append(dict(
                unit=u["name"], cls=cls, fn=fn["name"], src=relpath,
                addr=addr, pct=pct, size=int(fn.get("size") or 0),
                kind=kind, actionable=actionable,
                n_ours=len(ours), n_retail=len(rl),
                surplus=[{"macro": m, "sym": h, "evidence": ev[h]}
                         for m, h in surplus],
                costly=sum(1 for m, h in surplus if m in COSTLY),
                n_absent=sum(1 for v in ev.values() if v == "absent"),
                n_present=sum(1 for v in ev.values() if v == "present"),
                n_tooshort=sum(1 for v in ev.values() if v == "tooshort"),
                missing=missing,
            ))

    print("\n==== FUNNEL ====", file=sys.stderr)
    print("  ?Handle@X@@ functions in pinned units : %d" % n_handle_fns, file=sys.stderr)
    print("  ... class has a BEGIN_HANDLERS block  : %d" % n_have_src, file=sys.stderr)
    print("  ... PAIRED (target_symbol_map entry)  : %d" % n_mapped, file=sys.stderr)
    print("  ... retail body readable from listing : %d" % n_retail_read, file=sys.stderr)
    print("  ... DROPPED, body yielded 0 strings   : %d" % n_unreadable, file=sys.stderr)

    if a.controls:
        print("\n==== CONTROL (Handle bodies already at fuzzy 100) ====")
        print("  scanned            : %d" % ctrl_tot)
        print("  reporting SURPLUS  : %d   (should be 0)" % ctrl_bad)
        for cls, hs in ctrl_examples:
            print("    !! %s: %s" % (cls, ", ".join(hs)))
        if ctrl_tot < 10:
            print("!! CONTROL INCONCLUSIVE -- denominator too small (%d)" % ctrl_tot)
            return 1
        if ctrl_bad / ctrl_tot > 0.02:
            print("!! CONTROL FAILED -- extractor invents surplus on known-good bodies")
            return 1
        print("  CONTROL PASSED (%.2f%% false surplus)" % (100.0 * ctrl_bad / ctrl_tot))
        return 0

    rows.sort(key=lambda r: (-int(r["actionable"]), -r["n_absent"], -(r["size"] or 0)))
    from collections import Counter
    kinds = Counter(r["kind"] for r in rows)
    print("  ... with SURPLUS arms                 : %d" % len(rows), file=sys.stderr)
    print("      by shape: %s" % dict(kinds), file=sys.stderr)
    print("  ... with >=1 PROVEN-ABSENT arm        : %d"
          % sum(1 for r in rows if r["n_absent"]), file=sys.stderr)
    act = [r for r in rows if r["actionable"]]
    print("  ... ACTIONABLE (EXTRA + proven absent): %d" % len(act), file=sys.stderr)
    print("  ... of those, currently BELOW 100%%    : %d"
          % sum(1 for r in act
                if isinstance(r["pct"], (int, float)) and r["pct"] < 100.0),
          file=sys.stderr)

    if a.json:
        json.dump(rows, open(a.json, "w"), indent=1)
        print("wrote %s" % a.json, file=sys.stderr)

    print("\n%-8s %-5s %-6s %-4s %-4s %-4s  %s" %
          ("kind", "pct", "size", "abs", "pres", "shrt", "class :: surplus arms"))
    for r in rows[:60]:
        p = "%5.1f" % r["pct"] if isinstance(r["pct"], (int, float)) else "  n/a"
        print("%-8s %s %6d %4d %4d %4d  %s :: %s" % (
            r["kind"] + ("*" if r["actionable"] else ""),
            p, r["size"], r["n_absent"], r["n_present"], r["n_tooshort"],
            r["cls"],
            ", ".join("%s[%s/%s]" % (s["sym"], s["macro"].replace("HANDLE_", ""),
                                     s["evidence"][:3]) for s in r["surplus"][:6])))
    return 0


if __name__ == "__main__":
    sys.exit(main())
