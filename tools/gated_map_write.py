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

★ INJECTIVITY IS TWO INVARIANTS, NOT ONE  (doc 66, 2026-08-06)
--------------------------------------------------------------
P5 asks "is this name already a value in the MAP".  That is map-side
injectivity and it is not the invariant that matters, because the renamer
writes names into an OBJECT that already has names of its own.  Doc 65 §4.2
found four split objects that already carry the name the map assigns to a
*second* address; this sweep found a fifth, of a second shape (two map rows
claiming one name, both landing in one object).  In every case the patched
object ends up defining one function name twice, and the reader every strict
verdict is read through — decomp-synth `tools/il_witness/coff_ppc.parse` —
keys its `funcs` dict by NAME, so one of the two functions becomes unreachable.

P7 closes that: the OBJECT's post-rename function-name set is in scope.
See `_object_collisions`.  `--audit-objects` runs the same check over the
WHOLE map with no write, which is the mode that finds a collision the file
already produces (P5 and P7 only ever guard the rows being ADDED).

GATES
-----
PRE (refuse, exit 4 — file untouched):
  P1 target exists, parses, root is an object
  P2 no PRE-EXISTING duplicate root keys (file already compromised)
  P2b no PRE-EXISTING root keys equal UP TO CASE (one address, two rows)
  P3 batch is internally consistent (no repeated key up to case, no repeated value)
  P4 every new key is absent from the root pair list, COMPARED CASE-FOLDED
  P5 every new value is absent from the root values (MAP-side NAME INJECTIVITY)
  P6 key is lowercase `0x` + 8 hex digits; value is a plain one-line string
  P7 every new value is absent from the post-rename function names of the split
     OBJECT its address lands in (OBJECT-side NAME INJECTIVITY)

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
import struct
import sys
import tempfile
from collections import Counter, defaultdict
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple

EXIT_OK = 0
EXIT_ERROR = 1
EXIT_REFUSED_PRE = 4
EXIT_REFUSED_POST = 5

KEY_RX = re.compile(r"^0x[0-9a-f]{8}$")
PLACEHOLDER_RX = re.compile(r"^(?:fn|lbl)_([0-9A-F]{8})$")
DEFAULT_OBJ_DIR = "build/45410914/obj"

#: IMAGE_SCN_CNT_CODE — the section characteristic that makes a section code.
SCN_CNT_CODE = 0x00000020

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


# ==========================================================================
# P7 — OBJECT-SIDE NAME INJECTIVITY
# ==========================================================================
def code_function_names(data: bytes) -> List[str]:
    """Defined function symbols in CODE sections, in symbol-table order.

    ★ This deliberately mirrors, predicate for predicate, the keying of
    decomp-synth `tools/il_witness/coff_ppc.parse` — storage class 2 (external)
    or 3 (static), section number > 0, COFF type high nibble 0x20 (function),
    and the section carries `IMAGE_SCN_CNT_CODE`.  That reader builds a dict
    keyed by NAME, so exactly this set is what a duplicate name truncates.  A
    looser scan (every symbol, the way `obj_target_symbol_renamer`'s own
    `parse_coff_symbols` reports them) would refuse on an UNDEF external that
    merely *references* the name, which is not a collision at all — the split
    objects reference their callees by exactly the names we are writing.
    """
    if len(data) < 20:
        return []
    _machine, nsec, _ts, symptr, nsym, optsz, _char = struct.unpack_from(
        "<HHIIIHH", data, 0)
    if not symptr or not nsym:
        return []
    strtab_off = symptr + nsym * 18

    code_sec = set()
    off = 20 + optsz
    for i in range(1, nsec + 1):
        h = data[off:off + 40]
        if len(h) < 40:
            break
        chc = struct.unpack_from("<I", h, 36)[0]
        if chc & SCN_CNT_CODE:
            code_sec.add(i)
        off += 40

    out: List[str] = []
    i = 0
    while i < nsym:
        e = data[symptr + i * 18: symptr + i * 18 + 18]
        if len(e) < 18:
            break
        if e[:4] == b"\0\0\0\0":
            o = strtab_off + struct.unpack_from("<I", e, 4)[0]
            end = data.find(b"\0", o)
            name = data[o:end if end >= 0 else len(data)].decode("latin1")
        else:
            z = e.find(b"\0", 0, 8)
            name = e[:z if 0 <= z < 8 else 8].decode("latin1")
        _val, secnum, typ, scl, naux = struct.unpack_from("<IhHBB", e, 8)
        if scl in (2, 3) and secnum in code_sec and (typ & 0xF0) == 0x20:
            out.append(name)
        i += 1 + naux
    return out


class ObjectIndex:
    """Split-object symbol view: which object defines each `fn_`/`lbl_` address,
    and what function names each object defines.  Read once, cached."""

    def __init__(self, obj_dir: Path):
        self.obj_dir = obj_dir
        self.names: Dict[str, List[str]] = {}         # relpath -> code fn names
        self.objs_for_addr: Dict[str, Set[str]] = defaultdict(set)  # 0xaddr -> rel
        for p in sorted(obj_dir.glob("**/*.obj")):
            rel = str(p.relative_to(obj_dir))
            try:
                nm = code_function_names(p.read_bytes())
            except OSError:
                continue
            self.names[rel] = nm
            for n in nm:
                m = PLACEHOLDER_RX.match(n)
                if m:
                    self.objs_for_addr["0x" + m.group(1).lower()].add(rel)

    def __len__(self) -> int:
        return len(self.names)

    def post_rename(self, rel: str, amap: Dict[str, str]) -> List[str]:
        """This object's function names AFTER the renamer applies `amap`
        (address-keyed, lowercase `0x…`), exactly as the renamer would."""
        out = []
        for n in self.names[rel]:
            m = PLACEHOLDER_RX.match(n)
            out.append(amap.get("0x" + m.group(1).lower(), n) if m else n)
        return out


def address_map(effective: Dict[str, object]) -> Dict[str, str]:
    """The map as the renamer reads it: lowercase `0x…` -> name, nulls and
    non-strings dropped (`obj_target_symbol_renamer.load_address_map`)."""
    return {k.lower(): v for k, v in effective.items()
            if isinstance(k, str) and k.lower().startswith("0x")
            and isinstance(v, str)}


def _object_collisions(index: ObjectIndex, amap: Dict[str, str]) -> List[dict]:
    """Every object whose POST-RENAME function names contain a duplicate.

    This is the whole-map audit — the check that finds a collision the file
    ALREADY produces.  P7 is the same question asked about the rows being
    added.
    """
    out = []
    for rel in sorted(index.names):
        names = index.post_rename(rel, amap)
        for name, n in sorted(Counter(names).items()):
            if n > 1:
                claimants = sorted(
                    a for a, v in amap.items()
                    if v == name and rel in index.objs_for_addr.get(a, ()))
                # An obj tree that has ALREADY been through the renamer has no
                # `fn_`/`lbl_` placeholder left to attribute the collision to
                # (the live `build/45410914/obj` is exactly that — the split
                # step runs the renamer, `target_symbol_renames.stamp`). Fall
                # back to naming every map row that claims the name, so the
                # readout still points at the rows to look at.
                out.append({"obj": rel, "name": name, "count": n,
                            "map_addresses_in_this_obj": claimants,
                            "map_addresses_claiming_name":
                                sorted(a for a, v in amap.items() if v == name)})
    return out


def _p7_gate(index: Optional[ObjectIndex], a: Dict[str, object],
             rows: List[Tuple[str, str]]) -> List[str]:
    """OBJECT-side name injectivity.  Returns advisory lines; raises on refusal.

    For each new row, the object(s) that define its placeholder are re-rendered
    under the effective map WITHOUT this row; if the row's name is already in
    that set, the patched object would define the name twice and
    `coff_ppc.parse` would drop one of the two functions.
    """
    notes: List[str] = []
    if index is None:
        return notes
    base = address_map(a["effective"])          # the map as it stands today
    for k, v in rows:
        objs = sorted(index.objs_for_addr.get(k.lower(), ()))
        if not objs:
            notes.append(f"P7 note: no split object defines fn_/lbl_{k[2:].upper()} "
                         f"— the rename is a no-op, nothing to collide with")
            continue
        for rel in objs:
            existing = index.post_rename(rel, base)
            if v in existing:
                owners = sorted(ad for ad, nm in base.items()
                                if nm == v and rel in index.objs_for_addr.get(ad, ()))
                why = (f"another mapped address in the same object already claims "
                       f"it ({', '.join(owners)})" if owners else
                       "the split object already defines that name at another "
                       "address, with no map row involved")
                raise Refused(
                    EXIT_REFUSED_PRE,
                    f"P7 value {v!r} is ALREADY a function name in {rel} — "
                    f"{why}. Patching {k} would make that object define one name "
                    f"TWICE, and coff_ppc.parse keys `funcs` by name, so one of "
                    f"the two functions becomes unreachable to objdiff and to "
                    f"every strict verdict. This is OBJECT-side injectivity; P5 "
                    f"only sees the map.")
    return notes


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


def resolve_obj_index(obj_dir: Optional[Path], *, required: bool,
                      verbose: bool = True) -> Optional[ObjectIndex]:
    """Build the `ObjectIndex` P7 needs, or refuse.

    ★ FAIL-CLOSED.  A gate that cannot run is not a gate that passes.  If the
    split objects are absent (an unbuilt worktree) P7 REFUSES rather than
    skipping, and the caller must say `--no-object-gate` to proceed without it —
    which prints, loudly, that the write went in unchecked.
    """
    if obj_dir is None:
        return None
    if not obj_dir.is_dir():
        if required:
            raise Refused(
                EXIT_REFUSED_PRE,
                f"P7 cannot run: no split objects at {obj_dir}. Build the tree, "
                f"point --obj-dir at one, or pass --no-object-gate to write "
                f"WITHOUT object-side injectivity checking (and say so).")
        return None
    idx = ObjectIndex(obj_dir)
    if not len(idx):
        if required:
            raise Refused(EXIT_REFUSED_PRE,
                          f"P7 cannot run: {obj_dir} contains no .obj files")
        return None
    if verbose:
        print(f"P7: indexed {len(idx)} split object(s) from {obj_dir}")
    return idx


def apply_rows(target: Path, rows: List[Tuple[str, str]], *,
               dry_run: bool = False, verbose: bool = True,
               obj_index: Optional[ObjectIndex] = None) -> Dict[str, object]:
    """Insert rows at the top of the root object.  Raises Refused on any gate."""
    if not target.exists():
        raise Refused(EXIT_REFUSED_PRE, f"P1 no such target: {target}")
    before = target.read_text()
    a0 = audit(before)
    if verbose:
        print(f"BEFORE: root_rows={a0['n_root']} total_pairs={a0['n_total']} "
              f"dup_keys={len(a0['dup_keys'])} dup_names={len(a0['dup_vals'])}")
    _pre_gates(a0, rows)
    for note in _p7_gate(obj_index, a0, rows):
        if verbose:
            print(note)
    if obj_index is None and verbose:
        print("⚠ P7 SKIPPED — object-side name injectivity was NOT checked")

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
def _synth_obj(func_names: List[str]) -> bytes:
    """A minimal PPC-BE COFF object defining one code COMDAT per name.

    Selftest fixture only.  Hermetic on purpose: the P7 controls must fire on a
    checkout with no `build/` at all, so they must not depend on the split
    objects being present.  `_audit_objects` against the real tree is the
    real-data check, and it is a command, not a control.
    """
    body = b"\x4e\x80\x00\x20"                     # blr
    n = len(func_names)
    hdr = 20 + n * 40
    out = bytearray()
    out += struct.pack("<HHIIIHH", 0x01F2, n, 0, hdr + n * len(body), n, 0, 0)
    for i in range(n):
        out += b".text\0\0\0" + struct.pack(
            "<IIIIII", 0, 0, len(body), hdr + i * len(body), 0, 0)
        out += struct.pack("<HHI", 0, 0, SCN_CNT_CODE | 0x00001000)
    out += body * n
    strtab = bytearray(b"\0\0\0\0")
    for i, nm in enumerate(func_names, start=1):
        raw = nm.encode("ascii")
        if len(raw) <= 8:
            fld = raw + b"\0" * (8 - len(raw))
        else:
            fld = struct.pack("<II", 0, len(strtab))
            strtab += raw + b"\0"
        out += fld + struct.pack("<IhHBB", 0, i, 0x20, 2, 0)
    struct.pack_into("<I", strtab, 0, len(strtab))
    return bytes(out) + bytes(strtab)


def _project_root() -> Path:
    return Path(__file__).resolve().parent.parent


def _default_obj_dir() -> Path:
    for root in (_project_root(), Path.cwd()):
        p = root / DEFAULT_OBJ_DIR
        if p.is_dir():
            return p
    return _project_root() / DEFAULT_OBJ_DIR


def _audit_objects(target: Path, obj_dir: Optional[Path]) -> int:
    """Whole-map object-side injectivity audit.  Writes nothing."""
    try:
        idx = resolve_obj_index(obj_dir or _default_obj_dir(), required=True)
    except Refused as r:
        print(f"REFUSED: {r}", file=sys.stderr)
        return r.code
    amap = address_map(audit(target.read_text())["effective"])
    coll = _object_collisions(idx, amap)
    print(f"audit-objects: map={target} rows={len(amap)} objs={len(idx)}")
    for c in coll:
        print(f"  COLLISION {c['obj']}: {c['name']!r} defined {c['count']}x "
              f"after the rename")
        here = c["map_addresses_in_this_obj"] or "(none — already renamed, or not a map row)"
        print(f"      map rows in this obj still carrying a placeholder: {here}")
        print(f"      every map row claiming this name: "
              f"{c['map_addresses_claiming_name'] or '(none)'}")
    print(f"  {len(coll)} object-side name collision(s)")
    return EXIT_OK if not coll else EXIT_REFUSED_PRE


def _find_live_map() -> Path:
    for root in (Path(__file__).resolve().parent.parent, Path.cwd()):
        p = root / "scripts/target_symbol_map.json"
        if p.exists():
            return p
    raise SystemExit("cannot locate scripts/target_symbol_map.json to copy")


def _selftest(scratch: Optional[Path] = None) -> int:
    src = _find_live_map()
    rows: List[Tuple[str, bool, str]] = []
    # ★ --scratch exists because TMPDIR here is `~/tmp`, and this box's standing
    #   rule is that nothing lands there: a run that times out mid-selftest
    #   leaves its evidence wherever it was writing. Default behaviour is
    #   unchanged for every existing caller.
    if scratch is not None:
        scratch.mkdir(parents=True, exist_ok=True)
        tmpd = Path(tempfile.mkdtemp(prefix="gated_map_write_selftest_",
                                     dir=str(scratch)))
    else:
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

    # ---- (8) P7 OBJECT-SIDE INJECTIVITY ----------------------------------
    #      The gate doc 65 §4.2 found missing.  Controls are built on a
    #      SYNTHESIZED .obj so they fire on any checkout, built or not — and
    #      each one is paired with the state that must NOT refuse, because a
    #      gate that refuses everything is as useless as one that refuses
    #      nothing (doc 58 §7: a control whose sabotage is a no-op passes
    #      silently while proving nothing).
    try:
        objd = tmpd / "objs"
        objd.mkdir()
        taken = "?AlreadyHere@Selftest@@QAAXXZ"
        free = "?NotThereYet@Selftest@@QAAXXZ"
        # one object defining fn_82DEAD00 and, separately, `taken`
        (objd / "Probe.obj").write_bytes(
            _synth_obj(["fn_82DEAD00", taken, "?Other@Selftest@@QAAXXZ"]))
        idx = ObjectIndex(objd)
        ok_idx = (len(idx) == 1
                  and idx.objs_for_addr.get("0x82dead00") == {"Probe.obj"}
                  and taken in idx.names["Probe.obj"])
        rows.append(("P7 fixture: synthesized obj parses to the expected "
                     "function names", ok_idx,
                     f"objs={len(idx)} names={idx.names.get('Probe.obj')}"))

        # (8a) THE GATE FIRES: the name is already defined in that object.
        t = fresh("p7.json")
        snapshot = t.read_text()
        try:
            apply_rows(t, [("0x82dead00", taken)], verbose=False, obj_index=idx)
            rows.append(("P7 fires: name already defined in the target OBJECT "
                         "REFUSES", False,
                         "tool ACCEPTED a row that would define one name twice"))
        except Refused as r:
            ok = (r.code == EXIT_REFUSED_PRE and t.read_text() == snapshot
                  and "P7" in str(r))
            rows.append(("P7 fires: name already defined in the target OBJECT "
                         "REFUSES (exit 4, file untouched)", ok,
                         f"code={r.code} untouched={t.read_text() == snapshot} "
                         f"msg={str(r)[:110]}"))

        # (8b) VACUITY CONTROL: the SAME row, same gates, object gate off ->
        #      accepted.  This is what proves P7 (and not P1-P6) refused above.
        t2 = fresh("p7_vacuity.json")
        try:
            apply_rows(t2, [("0x82dead00", taken)], verbose=False, obj_index=None)
            eff = json.loads(t2.read_text())["0x82dead00"]
            rows.append(("P7 VACUITY CONTROL: the identical row passes P1-P6 "
                         "and is ACCEPTED with the object gate off", eff == taken,
                         f"effective value {eff[:44]!r} — so the refusal in the "
                         f"previous control came from P7 alone"))
        except Refused as r:
            rows.append(("P7 VACUITY CONTROL: the identical row passes P1-P6 "
                         "and is ACCEPTED with the object gate off", False,
                         f"another gate refused it: {str(r)[:110]}"))

        # (8c) NO FALSE REFUSAL: a name the object does not define is accepted.
        t3 = fresh("p7_clean.json")
        try:
            apply_rows(t3, [("0x82dead00", free)], verbose=False, obj_index=idx)
            eff = json.loads(t3.read_text())["0x82dead00"]
            rows.append(("P7 does not over-refuse: a name absent from the "
                         "object is ACCEPTED", eff == free, f"value {eff!r}"))
        except Refused as r:
            rows.append(("P7 does not over-refuse: a name absent from the "
                         "object is ACCEPTED", False, f"REFUSED {str(r)[:110]}"))

        # (8d) SECOND SHAPE: two map rows claiming one name, both landing in
        #      one object.  P5 stops the second INSERT; `--audit-objects` is
        #      what sees the pair once both are in the file (the live map's
        #      `?DataDir@UIPanel@@$4PPPPPPPM@EM@…`, doc 66 §4).
        (objd / "Twin.obj").write_bytes(
            _synth_obj(["fn_82BEEF00", "fn_82BEEF10"]))
        idx2 = ObjectIndex(objd)
        twin = "?TwinName@Selftest@@QAAXXZ"
        coll = _object_collisions(
            idx2, {"0x82beef00": twin, "0x82beef10": twin})
        clean = _object_collisions(
            idx2, {"0x82beef00": twin, "0x82beef10": twin + "2"})
        rows.append(("--audit-objects sees TWO MAP ROWS claiming one name in "
                     "one object (and nothing when they differ)",
                     len(coll) == 1 and coll[0]["obj"] == "Twin.obj"
                     and coll[0]["count"] == 2 and not clean,
                     f"same-name -> {len(coll)} collision(s) "
                     f"{[c['name'] for c in coll]}; distinct-name -> {len(clean)}"))

        # (8e) FAIL-CLOSED: no objects -> P7 REFUSES rather than skipping.
        try:
            resolve_obj_index(tmpd / "no_such_obj_dir", required=True,
                              verbose=False)
            rows.append(("P7 is FAIL-CLOSED: a missing obj dir REFUSES", False,
                         "resolve_obj_index returned instead of refusing"))
        except Refused as r:
            rows.append(("P7 is FAIL-CLOSED: a missing obj dir REFUSES",
                         r.code == EXIT_REFUSED_PRE and "P7" in str(r),
                         f"code={r.code} msg={str(r)[:100]}"))
    except Exception as e:
        rows.append(("P7 object-side injectivity controls", False, f"EXC {e!r}"))

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
    ap.add_argument("--obj-dir", type=Path, default=None,
                    help=f"split target .obj dir for P7 (default: "
                         f"<project>/{DEFAULT_OBJ_DIR})")
    ap.add_argument("--no-object-gate", action="store_true",
                    help="write WITHOUT P7 object-side injectivity checking "
                         "(loud; use only on a tree with no built objects)")
    ap.add_argument("--audit-objects", action="store_true",
                    help="run P7's check over the WHOLE map and exit; writes "
                         "nothing. Needs --target. Exit 4 if any object would "
                         "define a function name twice after the rename.")
    ap.add_argument("--selftest", action="store_true",
                    help="run controls on a COPY of the live map")
    ap.add_argument("--scratch", type=Path, default=None,
                    help="directory to create the selftest scratch dir in "
                         "(default: TMPDIR, which on some boxes is ~/tmp)")
    a = ap.parse_args(argv)

    if a.selftest:
        return _selftest(a.scratch)
    if a.audit_objects:
        if not a.target:
            ap.error("--audit-objects needs --target (the map to audit)")
        return _audit_objects(a.target, a.obj_dir)
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
        idx = resolve_obj_index(
            None if a.no_object_gate else (a.obj_dir or _default_obj_dir()),
            required=not a.no_object_gate)
        apply_rows(a.target, rows, dry_run=a.dry_run, obj_index=idx)
    except Refused as r:
        print(f"REFUSED: {r}", file=sys.stderr)
        return r.code
    return EXIT_OK


if __name__ == "__main__":
    sys.exit(main())
