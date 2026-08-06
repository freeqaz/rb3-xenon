#!/usr/bin/env python3
"""Gated writer for scripts/target_symbol_map.json — textual insertion + audit.

    python3 tools/gated_map_write.py --target <map.json> \
        --row 0x82637190=??_GSigninScreen@@UAAPAXI@Z [--row ...] [--dry-run]
    python3 tools/gated_map_write.py --target <map.json> --rows-json rows.json
    python3 tools/gated_map_write.py --selftest        # non-zero exit on failure

★ THERE IS DELIBERATELY NO DEFAULT --target.
You must name the file.  The live map is too easy to clobber by accident, and
several lanes have wanted a shared applier; making the path explicit is the
cheapest way to guarantee no accidental write.

★ NEVER json.dump
-----------------
The map's 1-space indent is load-bearing (every applier, differ and review
tool in the tree assumes one row per line, ` "0x...": "name",`).  A
`json.dump` round-trip reformats all ~28,000 lines, producing a diff nobody
can review and destroying key ORDER — which is the very thing the duplicate-key
trap turns on.  This tool only ever splices text and then proves, byte for
byte, that nothing outside the inserted lines moved.

★ THE DUPLICATE-KEY PHANTOM TRAP  (docs: project_map_applier_duplicate_key_trap)
-------------------------------------------------------------------------------
Appliers insert at the TOP of the object.  JSON permits duplicate keys and
`json.load` keeps the **LAST** one.  So inserting a key that already exists
further down gives you: a file that changed, a clean-looking one-line diff, and
**no semantic change at all** — a phantom edit that measures Δ0 and wastes a
whole A/B leg.  Two gates catch that shape:
  * PRE  — the key must be absent from *every* root pair (checked through
           object_pairs_hook, because the parsed dict cannot show you a
           duplicate: it has already discarded one).
  * POST — the EFFECTIVE parsed value for each inserted key must equal what we
           inserted.  This catches the phantom directly, wherever it came from.
The selftest proves this is not theatre: it performs the naive top-insert on a
copy and shows the phantom (bytes change, parsed value does not), then shows
this tool refusing the identical request.

GATES
-----
PRE (refuse, exit 4 — file untouched):
  P1 target exists, parses, root is an object
  P2 no PRE-EXISTING duplicate root keys (file already compromised)
  P2b no PRE-EXISTING root keys equal UP TO CASE (one address, two rows)
  P3 batch is internally consistent (no repeated key up to case, no repeated value)
  P4 every new key is absent from the root pair list, COMPARED CASE-FOLDED
  P5 every new value is absent from the root values (NAME INJECTIVITY)
  P6 key is lowercase `0x` + 8 hex digits; value is a plain one-line string

★ WHY P4 FOLDS CASE
-------------------
`obj_target_symbol_renamer.py` reads a key as `int(k.lower().removeprefix("0x"),
16)` — so `0x82357DD0` and `0x82357dd0` denote the SAME address, while JSON (and
an exact-string P4) treats them as different keys.  The live map shipped 7
uppercase keys; with P6 forcing every NEW key lowercase, an exact-string P4 was
structurally incapable of seeing them, and the same address could be inserted a
second time in the other case — two live rows for one address, no duplicate-key
gate firing, and the renamer honouring whichever `json.load` kept last.  Folding
case in P4 (plus P2b/P3/Q3b) closes that; the 7 keys were also normalized in the
file itself (`tools/normalize_map_key_case.py`), so the two fixes are
independent and either alone would have shut the hole.
POST (exit 5).  These run on the spliced text BEFORE anything is written, so a
post-gate refusal leaves the target byte-for-byte untouched — there is no
window in which a bad file exists on disk, and the final write is an atomic
tmp+replace:
  Q1 the result still parses
  Q2 root row count == before + N
  Q3 zero duplicate root keys
  Q4 duplicate-value count unchanged
  Q5 EFFECTIVE value matches for every inserted key   <- phantom gate
  Q6 total pair count (incl. nested objects) == before + N
  Q7 removing exactly the inserted lines reproduces the original bytes

SUBSUMES ~/tmp/laneDE3/apply_map.py and ~/tmp/laneDG1/apply_map.py (which were
byte-identical to each other apart from the hardcoded worktree path and the
hardcoded ROWS list).  Their gate set is kept in full and extended with: root
object isolation, the effective-value phantom gate (Q5), key/value format
validation, the byte-identity proof (Q7), rollback on post-failure, and rows on
the command line instead of edited into the source.
"""
from __future__ import annotations

import argparse
import json
import re
import shutil
import sys
import tempfile
from collections import Counter
from pathlib import Path
from typing import Dict, List, Optional, Tuple

EXIT_OK = 0
EXIT_ERROR = 1
EXIT_REFUSED_PRE = 4
EXIT_REFUSED_POST = 5

KEY_RX = re.compile(r"^0x[0-9a-f]{8}$")

#: Test-only hook.  If set, it rewrites the spliced text before the POST gates
#: run, so the selftest can prove those gates are capable of firing.  An
#: unexercised gate is indistinguishable from an absent one.
_SABOTAGE_SPLICE = None


class Refused(Exception):
    def __init__(self, code: int, msg: str):
        super().__init__(msg)
        self.code = code


def parse_root(text: str) -> Tuple[List[Tuple[str, object]], int]:
    """(root object's ordered pairs, total pairs incl. nested).

    A plain `json.load` CANNOT report duplicate keys — it silently keeps the
    last.  object_pairs_hook sees every pair.  Objects complete depth-first, so
    the LAST list handed to the hook is the root object's.
    """
    seen: List[List[Tuple[str, object]]] = []

    def hook(kv):
        seen.append(list(kv))
        return dict(kv)

    json.loads(text, object_pairs_hook=hook)
    if not seen:
        raise Refused(EXIT_REFUSED_PRE, "document root is not an object")
    return seen[-1], sum(len(x) for x in seen)


def audit(text: str) -> Dict[str, object]:
    root, total = parse_root(text)
    keys = [k for k, _ in root]
    vals = [v for k, v in root if k.startswith("0x") and isinstance(v, str)]
    # ★ CASE.  `0x82357DD0` and `0x82357dd0` are the SAME ADDRESS but different
    # JSON keys.  `obj_target_symbol_renamer.py` lowercases and `int(...,16)`s
    # the key, so the map's meaning is case-insensitive while the file's key
    # identity is not.  P6 forces every NEW key lowercase, so an exact-string
    # P4 was blind to the 7 uppercase keys the file already carried and would
    # have let the same address be inserted a second time in the other case —
    # two live rows for one address, with the renamer honouring whichever it
    # read last.  Every membership test below therefore folds case; the literal
    # key set is kept only for the byte-level duplicate-key gate.
    keys_ci = [k.lower() for k in keys]
    return dict(
        root=root,
        n_root=len(root),
        n_total=total,
        dup_keys=[k for k, c in Counter(keys).items() if c > 1],
        dup_keys_ci=[k for k, c in Counter(keys_ci).items() if c > 1],
        dup_vals=[v for v, c in Counter(vals).items() if c > 1],
        key_set=set(keys),
        key_set_ci=set(keys_ci),
        val_set=set(vals),
        effective={k: v for k, v in root},
    )


def render_row(k: str, v: str) -> str:
    """Exactly the file's house format: ONE leading space, trailing comma."""
    return f" {json.dumps(k)}: {json.dumps(v)},\n"


def _pre_gates(a: Dict[str, object], rows: List[Tuple[str, str]]) -> None:
    if a["dup_keys"]:
        raise Refused(EXIT_REFUSED_PRE,
                      f"P2 target ALREADY has {len(a['dup_keys'])} duplicate root "
                      f"key(s), e.g. {a['dup_keys'][:3]} — refusing to build on a "
                      f"compromised file")
    if a["dup_keys_ci"]:
        raise Refused(EXIT_REFUSED_PRE,
                      f"P2b target ALREADY has {len(a['dup_keys_ci'])} root key(s) "
                      f"repeated UP TO CASE, e.g. {a['dup_keys_ci'][:3]} — two rows "
                      f"for one address; normalize the file before adding to it")
    bk = [k for k, c in Counter(k.lower() for k, _ in rows).items() if c > 1]
    bv = [v for v, c in Counter(v for _, v in rows).items() if c > 1]
    if bk:
        raise Refused(EXIT_REFUSED_PRE, f"P3 batch repeats key(s) {bk} (case-folded)")
    if bv:
        raise Refused(EXIT_REFUSED_PRE, f"P3 batch repeats value(s) {bv}")
    for k, v in rows:
        if not KEY_RX.match(k):
            raise Refused(EXIT_REFUSED_PRE,
                          f"P6 key {k!r} is not lowercase 0x + 8 hex digits")
        if not isinstance(v, str) or not v or any(c in v for c in '"\\\n\r\t'):
            raise Refused(EXIT_REFUSED_PRE,
                          f"P6 value for {k} must be a plain one-line string: {v!r}")
        if k.lower() in a["key_set_ci"]:
            existing = next((kk for kk in a["key_set"] if kk.lower() == k.lower()), k)
            raise Refused(EXIT_REFUSED_PRE,
                          f"P4 key {k} is ALREADY present as {existing!r} (current value "
                          f"{a['effective'].get(existing)!r}) — inserting it would be a "
                          f"PHANTOM EDIT: json.load keeps the LAST duplicate, so the "
                          f"file would change and the meaning would not")
        if v in a["val_set"]:
            raise Refused(EXIT_REFUSED_PRE,
                          f"P5 value {v!r} is already used — breaks NAME INJECTIVITY")


def _post_gates(before_text: str, after_text: str, a0: Dict[str, object],
                rows: List[Tuple[str, str]], lines: List[str]) -> Dict[str, object]:
    try:
        a1 = audit(after_text)
    except Exception as e:
        raise Refused(EXIT_REFUSED_POST, f"Q1 result does not parse: {e!r}")
    n = len(rows)
    if a1["n_root"] != a0["n_root"] + n:
        raise Refused(EXIT_REFUSED_POST,
                      f"Q2 root rows {a0['n_root']} -> {a1['n_root']}, expected +{n}")
    if a1["dup_keys"]:
        raise Refused(EXIT_REFUSED_POST, f"Q3 duplicate keys introduced: {a1['dup_keys'][:3]}")
    if len(a1["dup_keys_ci"]) != len(a0["dup_keys_ci"]):
        raise Refused(EXIT_REFUSED_POST,
                      f"Q3b case-folded duplicate keys {len(a0['dup_keys_ci'])} -> "
                      f"{len(a1['dup_keys_ci'])}: {a1['dup_keys_ci'][:3]}")
    if len(a1["dup_vals"]) != len(a0["dup_vals"]):
        raise Refused(EXIT_REFUSED_POST,
                      f"Q4 duplicate-value count {len(a0['dup_vals'])} -> {len(a1['dup_vals'])}")
    for k, v in rows:
        got = a1["effective"].get(k)
        if got != v:
            raise Refused(EXIT_REFUSED_POST,
                          f"Q5 PHANTOM: effective value for {k} is {got!r}, not {v!r}")
    if a1["n_total"] != a0["n_total"] + n:
        raise Refused(EXIT_REFUSED_POST,
                      f"Q6 total pairs {a0['n_total']} -> {a1['n_total']}, expected +{n}")
    # Q7: strip exactly the inserted lines; the rest must be byte-identical.
    rest = after_text
    for ln in lines:
        i = rest.find(ln)
        if i < 0:
            raise Refused(EXIT_REFUSED_POST, f"Q7 inserted line not found verbatim: {ln!r}")
        rest = rest[:i] + rest[i + len(ln):]
    if rest != before_text:
        raise Refused(EXIT_REFUSED_POST,
                      "Q7 bytes OUTSIDE the inserted lines changed — this is the "
                      "json.dump-reformat signature")
    return a1


def apply_rows(target: Path, rows: List[Tuple[str, str]], *,
               dry_run: bool = False, verbose: bool = True) -> Dict[str, object]:
    """Insert rows at the top of the root object.  Raises Refused on any gate."""
    if not target.exists():
        raise Refused(EXIT_REFUSED_PRE, f"P1 no such target: {target}")
    before = target.read_text()
    a0 = audit(before)
    if verbose:
        print(f"BEFORE: root_rows={a0['n_root']} total_pairs={a0['n_total']} "
              f"dup_keys={len(a0['dup_keys'])} dup_names={len(a0['dup_vals'])}")
    _pre_gates(a0, rows)

    i = before.index("{\n") + 2          # top-of-object insertion point
    lines = [render_row(k, v) for k, v in rows]
    after = before[:i] + "".join(lines) + before[i:]
    if _SABOTAGE_SPLICE is not None:
        after = _SABOTAGE_SPLICE(after)

    a1 = _post_gates(before, after, a0, rows, lines)   # validated BEFORE writing
    if dry_run:
        if verbose:
            print(f"DRY-RUN: all gates pass; would add {len(rows)} row(s); not written")
        return dict(written=False, n_added=len(rows), after=a1)
    tmp = target.with_suffix(target.suffix + ".gmw.tmp")
    tmp.write_text(after)
    tmp.replace(target)                  # atomic within the same filesystem
    if verbose:
        print(f"AFTER : root_rows={a1['n_root']} total_pairs={a1['n_total']} "
              f"dup_keys={len(a1['dup_keys'])} dup_names={len(a1['dup_vals'])}")
        print(f"OK: +{len(rows)} row(s); Q1-Q7 pass; bytes outside the inserted "
              f"line(s) are IDENTICAL")
    return dict(written=True, n_added=len(rows), after=a1)


# ==========================================================================
# CONTROLS — all on a TEMP COPY; the live map is never touched.
# ==========================================================================
def _find_live_map() -> Path:
    for root in (Path(__file__).resolve().parent.parent, Path.cwd()):
        p = root / "scripts/target_symbol_map.json"
        if p.exists():
            return p
    raise SystemExit("cannot locate scripts/target_symbol_map.json to copy")


def _selftest() -> int:
    src = _find_live_map()
    rows: List[Tuple[str, bool, str]] = []
    tmpd = Path(tempfile.mkdtemp(prefix="gated_map_write_selftest_"))
    print(f"gated_map_write selftest  source={src}\n  scratch={tmpd}  "
          f"(the live map is READ ONLY here)")
    orig = src.read_text()

    def fresh(name: str) -> Path:
        p = tmpd / name
        shutil.copyfile(src, p)
        return p

    # ---- (1) clean insert succeeds and the diff is EXACTLY ONE LINE --------
    try:
        t = fresh("clean.json")
        before = t.read_text()
        k, v = "0xdeadbe00", "?SelftestOnly@GatedMapWrite@@QAAXXZ"
        apply_rows(t, [(k, v)], verbose=False)
        after = t.read_text()
        bl, al = before.splitlines(True), after.splitlines(True)
        added = [x for x in al if x not in bl]
        ok = (len(al) == len(bl) + 1 and len(added) == 1
              and added[0] == f' "{k}": "{v}",\n'
              and json.loads(after)[k] == v)
        rows.append(("clean insert: +1 line, exact house format, value effective",
                     ok, f"lines {len(bl)}->{len(al)}, added={added[0]!r}"))
    except Exception as e:
        rows.append(("clean insert: +1 line, exact house format, value effective",
                     False, f"EXC {e!r}"))

    # ---- (2) json.dump-style rewrite impossible: bytes outside are identical
    try:
        t = fresh("bytes.json")
        k, v = "0xdeadbe01", "?SelftestBytes@GatedMapWrite@@QAAXXZ"
        apply_rows(t, [(k, v)], verbose=False)
        after = t.read_text()
        line = f' "{k}": "{v}",\n'
        stripped = after.replace(line, "", 1)
        ok = (stripped == orig and len(after) == len(orig) + len(line))
        rows.append(("no reformat: removing the inserted line reproduces the "
                     "source BYTE-FOR-BYTE", ok,
                     f"len {len(orig)} -> {len(after)} (+{len(after)-len(orig)} "
                     f"= len(line) {len(line)}); stripped==orig -> {stripped == orig}"))
    except Exception as e:
        rows.append(("no reformat: removing the inserted line reproduces the "
                     "source BYTE-FOR-BYTE", False, f"EXC {e!r}"))

    # ---- (3) duplicate-KEY insert REFUSES ---------------------------------
    #      ...and first, the VACUITY CONTROL: show that the naive applier this
    #      tool replaces really does produce a silent phantom on this input.
    try:
        t = fresh("dup.json")
        base = json.loads(orig)
        dup_key = next(k for k in base if k.startswith("0x"))
        old_val = base[dup_key]
        new_val = "?PhantomWouldBeThis@Selftest@@QAAXXZ"

        naive = tmpd / "naive_phantom.json"
        txt = orig
        i = txt.index("{\n") + 2
        naive.write_text(txt[:i] + f' "{dup_key}": "{new_val}",\n' + txt[i:])
        phantom_bytes_changed = naive.read_text() != orig
        phantom_value_unchanged = json.loads(naive.read_text())[dup_key] == old_val
        rows.append(("VACUITY CONTROL: the naive top-insert really is a silent "
                     "phantom (bytes change, parsed value does NOT)",
                     phantom_bytes_changed and phantom_value_unchanged,
                     f"key={dup_key} bytes_changed={phantom_bytes_changed} "
                     f"parsed still {old_val[:44]!r} -> "
                     f"unchanged={phantom_value_unchanged}"))

        snapshot = t.read_text()
        try:
            apply_rows(t, [(dup_key, new_val)], verbose=False)
            rows.append(("duplicate-KEY insert REFUSES", False,
                         "tool ACCEPTED a duplicate key"))
        except Refused as r:
            ok = (r.code == EXIT_REFUSED_PRE and t.read_text() == snapshot)
            rows.append(("duplicate-KEY insert REFUSES (exit 4, file untouched)",
                         ok, f"code={r.code} file_untouched={t.read_text() == snapshot} "
                             f"msg={str(r)[:120]}"))
    except Exception as e:
        rows.append(("duplicate-KEY insert REFUSES", False, f"EXC {e!r}"))

    # ---- (4) duplicate-NAME insert REFUSES (injectivity) ------------------
    try:
        t = fresh("name.json")
        base = json.loads(orig)
        existing_name = next(v for k, v in base.items()
                             if k.startswith("0x") and isinstance(v, str))
        snapshot = t.read_text()
        try:
            apply_rows(t, [("0xdeadbe02", existing_name)], verbose=False)
            rows.append(("duplicate-NAME insert REFUSES (injectivity)", False,
                         "tool ACCEPTED a duplicate name"))
        except Refused as r:
            ok = (r.code == EXIT_REFUSED_PRE and t.read_text() == snapshot)
            rows.append(("duplicate-NAME insert REFUSES (injectivity)", ok,
                         f"code={r.code} untouched={t.read_text() == snapshot} "
                         f"name={existing_name[:50]!r}"))
    except Exception as e:
        rows.append(("duplicate-NAME insert REFUSES (injectivity)", False, f"EXC {e!r}"))

    # ---- (5) malformed key REFUSES ---------------------------------------
    try:
        t = fresh("badkey.json")
        snapshot = t.read_text()
        try:
            apply_rows(t, [("0xDEADBE03", "?Whatever@@QAAXXZ")], verbose=False)
            rows.append(("malformed key (uppercase hex) REFUSES", False, "accepted"))
        except Refused as r:
            ok = (r.code == EXIT_REFUSED_PRE and t.read_text() == snapshot)
            rows.append(("malformed key (uppercase hex) REFUSES", ok,
                         f"code={r.code} msg={str(r)[:90]}"))
    except Exception as e:
        rows.append(("malformed key (uppercase hex) REFUSES", False, f"EXC {e!r}"))

    # ---- (6) POST-GATE VACUITY CONTROL -----------------------------------
    #      Q1-Q7 never fire in the happy path, and an unexercised gate is
    #      indistinguishable from an absent one.  Sabotage the splice with the
    #      exact defect Q7 exists to catch -- a json.dump reformat -- and prove
    #      it refuses (exit 5) and writes nothing.
    global _SABOTAGE_SPLICE
    try:
        t = fresh("post.json")
        snapshot = t.read_text()

        def _reformat(after_text: str) -> str:
            # ⚠ indent=2, NOT indent=1.  The live map's house format IS
            # `json.dumps(..., indent=1)` byte-for-byte, so sabotaging with
            # indent=1 is a NO-OP: the "reformatted" text equals the spliced
            # text, Q7 legitimately passes, and the control reports FAIL while
            # the gate it is testing is perfectly healthy (the 6/7 recorded in
            # doc 55 §6).  Any indent != 1 reproduces the real defect Q7 exists
            # to catch — a whole-file reformat that no reviewer can diff.
            return json.dumps(json.loads(after_text), indent=2)   # the defect

        _SABOTAGE_SPLICE = _reformat
        try:
            apply_rows(t, [("0xdeadbe04", "?PostGate@Selftest@@QAAXXZ")], verbose=False)
            rows.append(("POST-GATE sabotage (json.dump reformat) REFUSES", False,
                         "tool ACCEPTED a reformatted file -- Q7 is VACUOUS"))
        except Refused as r:
            ok = (r.code == EXIT_REFUSED_POST and t.read_text() == snapshot)
            rows.append(("POST-GATE sabotage (json.dump reformat) REFUSES "
                         "(exit 5, nothing written)", ok,
                         f"code={r.code} untouched={t.read_text() == snapshot} "
                         f"msg={str(r)[:100]}"))
        finally:
            _SABOTAGE_SPLICE = None
    except Exception as e:
        _SABOTAGE_SPLICE = None
        rows.append(("POST-GATE sabotage (json.dump reformat) REFUSES", False, f"EXC {e!r}"))

    # ---- (7) UPPERCASE-KEY COLLISION HOLE --------------------------------
    #      P6 forces new keys lowercase, so an exact-string P4 CANNOT see an
    #      address the file already holds in uppercase (7 such keys shipped in
    #      the live map).  Rebuild that exact shape on a copy and prove P4 now
    #      refuses -- and, separately, that P2b refuses to build on a file that
    #      already holds one address twice in two cases.
    try:
        base = json.loads(orig)
        victim = next(k for k in base if KEY_RX.match(k))
        val = base[victim]

        t = fresh("upper.json")
        txt = t.read_text()
        old_line = f' "{victim}": {json.dumps(val)},\n'
        assert old_line in txt, "house-format line not found"
        t.write_text(txt.replace(old_line, f' "{victim.upper()}": {json.dumps(val)},\n', 1))
        snapshot = t.read_text()
        assert victim not in json.loads(snapshot), "fixture did not uppercase the key"
        try:
            apply_rows(t, [(victim, "?CaseHoleProbe@Selftest@@QAAXXZ")], verbose=False)
            rows.append(("uppercase-key COLLISION refuses (P4 folds case)", False,
                         f"tool ACCEPTED {victim} while {victim.upper()} is present "
                         f"-- one address would have TWO live rows"))
        except Refused as r:
            ok = (r.code == EXIT_REFUSED_PRE and t.read_text() == snapshot
                  and "P4" in str(r))
            rows.append(("uppercase-key COLLISION refuses (P4 folds case)", ok,
                         f"code={r.code} untouched={t.read_text() == snapshot} "
                         f"msg={str(r)[:110]}"))

        # and a file that ALREADY holds both cases is refused up front (P2b)
        t2 = fresh("bothcases.json")
        txt2 = t2.read_text()
        i = txt2.index("{\n") + 2
        t2.write_text(txt2[:i] + f' "{victim.upper()}": "?TwoRowsOneAddr@@QAAXXZ",\n'
                      + txt2[i:])
        snap2 = t2.read_text()
        try:
            apply_rows(t2, [("0xdeadbe05", "?AfterCaseDup@Selftest@@QAAXXZ")],
                       verbose=False)
            rows.append(("case-duplicated file refuses (P2b)", False,
                         "tool built on a file holding one address in two cases"))
        except Refused as r:
            ok = (r.code == EXIT_REFUSED_PRE and t2.read_text() == snap2
                  and "P2b" in str(r))
            rows.append(("case-duplicated file refuses (P2b)", ok,
                         f"code={r.code} untouched={t2.read_text() == snap2} "
                         f"msg={str(r)[:110]}"))
    except Exception as e:
        rows.append(("uppercase-key COLLISION refuses (P4 folds case)", False,
                     f"EXC {e!r}"))

    nfail = 0
    for name, ok, detail in rows:
        print(f"  [{'PASS' if ok else 'FAIL'}] {name}\n         {detail}")
        nfail += (not ok)
    print(f"  {len(rows) - nfail}/{len(rows)} controls passed")
    print(f"  live map untouched: {src.read_text() == orig}")
    shutil.rmtree(tmpd, ignore_errors=True)
    return EXIT_ERROR if nfail else EXIT_OK


def main(argv: Optional[List[str]] = None) -> int:
    ap = argparse.ArgumentParser(
        description="Gated textual writer for target_symbol_map.json. "
                    "Refuses on any anomaly; never json.dump.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="There is deliberately NO default --target. Exit codes: "
               "0 ok, 4 refused pre-write, 5 refused post-write "
               "(both leave the target byte-for-byte untouched), 1 error, "
               "2 argparse usage.  "
               "⚠ QUOTE YOUR --row: every MSVC mangled name starts with '?', "
               "which zsh treats as a glob -- an unquoted row dies with "
               "'no matches found' before python ever sees it.")
    ap.add_argument("--target", type=Path,
                    help="map file to modify (REQUIRED; no default, by design)")
    ap.add_argument("--row", action="append", default=[],
                    metavar="0xADDR=MangledName", help="row to insert (repeatable)")
    ap.add_argument("--rows-json", type=Path,
                    help='JSON file: {"0x...": "name", ...} or [["0x...","name"],...]')
    ap.add_argument("--dry-run", action="store_true",
                    help="run every gate, report, write nothing")
    ap.add_argument("--selftest", action="store_true",
                    help="run controls on a COPY of the live map")
    a = ap.parse_args(argv)

    if a.selftest:
        return _selftest()
    if not a.target:
        ap.error("--target is required (there is no default: the live map is "
                 "too easy to clobber)")

    rows: List[Tuple[str, str]] = []
    for r in a.row:
        if "=" not in r:
            ap.error(f"--row must be 0xADDR=MangledName, got {r!r}")
        k, v = r.split("=", 1)
        rows.append((k.strip(), v.strip()))
    if a.rows_json:
        blob = json.loads(a.rows_json.read_text())
        items = blob.items() if isinstance(blob, dict) else blob
        rows.extend((str(k).strip(), str(v).strip()) for k, v in items)
    if not rows:
        ap.error("no rows given (--row / --rows-json)")

    try:
        apply_rows(a.target, rows, dry_run=a.dry_run)
    except Refused as r:
        print(f"REFUSED: {r}", file=sys.stderr)
        return r.code
    return EXIT_OK


if __name__ == "__main__":
    sys.exit(main())
