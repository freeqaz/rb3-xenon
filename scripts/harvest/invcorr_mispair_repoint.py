#!/usr/bin/env python3
"""INVERSE CORRELATOR: repair mispaired entries in scripts/target_symbol_map.json.

Background
----------
dtk carves the retail XEX into per-unit target objs (build/45410914/obj/*.obj).
A pre-compile renamer rewrites each target ``fn_<addr>`` symbol to an MSVC
mangled name using scripts/target_symbol_map.json (JSON dict
"0x<addr_hex>": "<mangled>", ONE ENTRY PER LINE, mixed leading whitespace).
When a map entry points a mangled name at the WRONG address, objdiff compares
our correct compiled bytes against the wrong target carve -> false 0%.

This tool takes the confirmed-mispair list (/home/free/tmp/mispair_true.json,
122 entries) and, for each, finds the *correct* target address by content:
extract the reloc-masked body of the entry's base (compiled) symbol, then search
the global index of all unmapped target ``fn_<addr>`` bodies for a byte-identical
carve. A unique byte-identical hit is a guaranteed strict-100 repoint.

Layouts (both handled by tu5_reloc_masked_correlate.parse):
  - dtk target: one COMDAT .text section per function, func sym val=0.
  - base compiled: functions share a .text section, val=offset.
Relocations in a function's range are masked (4 bytes zeroed) before compare.

Modes
-----
  propose (default): classify all 122, write proposals json. No edits.
  --apply [--class UNIQUE-IDENTICAL] [--only N1,N2] [--limit N]:
      TEXTUAL, in-place repoint of UNIQUE-IDENTICAL entries only.

Classes
-------
  UNIQUE-IDENTICAL   exactly one byte-identical fn_ candidate surviving
                     reloc-target filtering, ALL reloc positions confirmed
                     (named target symbol == base symbol, or ICF-alias-equal),
                     all safety checks pass -> appliable (unless excluded).
  UNIQUE-IDENTICAL-RELOC-UNCONFIRMED
                     exactly one surviving candidate but >=1 reloc position
                     could not be confirmed (anonymous target symbol) ->
                     proposal only, never applied.
  LOW-HAMMING-SINGLE no exact match; exactly one same-length candidate within a
                     tiny word-hamming budget -> proposal only, NEVER applied.
  FUZZY-BEST         no exact/hamming; a single clear best fuzzy match within the
                     entry's OWN unit target obj (length-tolerant, difflib) ->
                     proposal only, NEVER applied. (Surfaces real near-misses
                     whose source diverges from retail by regalloc/struct-offset.)
  MULTI              >1 byte-identical candidates -> skip.
  ALREADY-OK         the currently-mapped target body already equals the base
                     body -> map is fine, mispair row is stale.
  SKIP-RELOC-CONTRADICTED
                     byte-identical candidate(s) existed but every one had a
                     named reloc-target mismatch (STL-T clone for another T,
                     an unproven ICF fold, or a cascading-mispair neighborhood
                     where the map names backing verification are themselves
                     wrong). Never re-enters via the hamming fallback.
  SKIP-NOMAPENTRY / SKIP-DUPMAPPED / SKIP-THUNK / SKIP-TEMPLATE / SKIP-NOBASE /
  SKIP-CONFLICT / ICF-AMBIG / SKIP-NOMATCH  -> not appliable.
"""
import argparse
import hashlib
import json
import os
import pickle
import re
import sys
import time
from collections import defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from tu5_reloc_masked_correlate import parse, IMAGE_SCN_CNT_CODE  # noqa: E402

ROOT = HERE.parent.parent  # worktree root
MAP_PATH = ROOT / "scripts" / "target_symbol_map.json"
OBJDIFF = ROOT / "objdiff.json"
ALIASES = ROOT / "scripts" / "symbol_aliases.json"
TARGET_OBJ_DIR = ROOT / "build" / "45410914" / "obj"
CACHE_PATH = Path(os.path.expanduser("~/tmp/invcorr_target_index.pkl"))

MISPAIR_DEFAULT = "/home/free/tmp/mispair_true.json"
PROPOSALS_DEFAULT = os.path.expanduser("~/tmp/invcorr_proposals.json")

THUNK_PREFIXES = ("??_G", "??_E", "??_D")
TEMPLATE_MARK = "ObjRefConcrete"


# --------------------------------------------------------------------------
# body extraction (mirrors tu5_reloc_masked_correlate.func_bodies but returns
# ALL function symbols as a list so we can (a) read the fn_<addr> address off
# the symbol name and (b) count identical bodies for the uniqueness check).
# The _ex variant also returns each function's in-range relocations as
# (off_in_body, target_symbol_name, reloc_type) -- the discriminating info
# that reloc-masking erases (the STL-T trap: template clones for a different
# T with the same stride are byte-identical after masking; only the reloc
# TARGETS differ).
# --------------------------------------------------------------------------
IMAGE_REL_PPC_PAIR = 0x12  # its symidx field holds a displacement, NOT a symbol


def func_body_list_ex(path):
    data, secs, syms = parse(path)
    idx2name = {sy["idx"]: sy["name"] for sy in syms}
    by_sec = defaultdict(list)
    for sy in syms:
        if sy["secn"] > 0 and sy["secn"] in secs and sy["sc"] in (2, 3) and sy["typ"] == 0x20:
            by_sec[sy["secn"]].append(sy)
    out = []
    for secn, members in by_sec.items():
        s = secs[secn]
        if not (s["chars"] & IMAGE_SCN_CNT_CODE):
            continue
        members = sorted(members, key=lambda x: x["val"])
        seen_start = set()
        for k, sy in enumerate(members):
            start = sy["val"]
            end = members[k + 1]["val"] if k + 1 < len(members) else s["raw_size"]
            if s["praw"] == 0 or end <= start:
                continue
            # keep only the first symbol per (secn,start) -- matches func_bodies'
            # setdefault semantics; avoids double-counting label aliases.
            if (secn, start) in seen_start:
                continue
            seen_start.add((secn, start))
            body = bytearray(data[s["praw"] + start:s["praw"] + end])
            for rva in s["relocs"]:
                if start <= rva < end:
                    off = rva - start
                    for b in range(4):
                        if off + b < len(body):
                            body[off + b] = 0
            relocs = []
            for (rva, symi, rtyp) in s.get("relocs_full", []):
                if start <= rva < end and rtyp != IMAGE_REL_PPC_PAIR:
                    relocs.append((rva - start, idx2name.get(symi, f"??idx{symi}"), rtyp))
            out.append((sy["name"], bytes(body), tuple(relocs)))
    return out


def func_body_list(path):
    """Back-compat view: (name, masked_body) pairs."""
    return [(n, b) for n, b, _r in func_body_list_ex(path)]


def norm_addr(a):
    """Normalize any '0x..'/'0X..' or fn_<HEX> token to lowercase 0x%08x."""
    if a.startswith("fn_"):
        a = a[3:]
    a = a.lower()
    if a.startswith("0x"):
        a = a[2:]
    return "0x%08x" % int(a, 16)


def sha(b):
    return hashlib.sha1(b).digest()


def words(b):
    return [b[i:i + 4] for i in range(0, len(b) - 3, 4)]


# --------------------------------------------------------------------------
# global target fn_ index (cached)
# --------------------------------------------------------------------------
INDEX_VERSION = 3  # v2: +reloc targets; v3: recursive glob (nested unit objs)


def build_target_index(verbose=True):
    objs = sorted(TARGET_OBJ_DIR.rglob("*.obj"))
    # cache key: version + sorted (name,size,mtime_ns) manifest
    manifest = [INDEX_VERSION] + [(o.name, o.stat().st_size, o.stat().st_mtime_ns)
                                  for o in objs]
    mkey = sha(pickle.dumps(manifest))
    if CACHE_PATH.exists():
        try:
            blob = pickle.loads(CACHE_PATH.read_bytes())
            if blob.get("mkey") == mkey:
                if verbose:
                    print(f"[index] cache hit ({len(blob['records'])} fn_ records)")
                return blob["records"]
        except Exception as e:  # noqa: BLE001
            if verbose:
                print(f"[index] cache unreadable ({e}); rebuilding")
    records = []  # (norm_addr, obj_basename, masked_body, relocs)
    t0 = time.time()
    for i, o in enumerate(objs):
        if verbose and i % 500 == 0:
            print(f"[index] {i}/{len(objs)} objs, {len(records)} fn_ so far "
                  f"({time.time() - t0:.0f}s)")
        try:
            rel = str(o.relative_to(TARGET_OBJ_DIR))[:-4]  # unit-ish id
            for name, body, relocs in func_body_list_ex(str(o)):
                if name.startswith("fn_"):
                    records.append((norm_addr(name), rel, body, relocs))
        except Exception as e:  # noqa: BLE001
            if verbose:
                print(f"[index] WARN parse failed {o.name}: {e}")
    CACHE_PATH.parent.mkdir(parents=True, exist_ok=True)
    CACHE_PATH.write_bytes(pickle.dumps({"mkey": mkey, "records": records}))
    if verbose:
        print(f"[index] built {len(records)} fn_ records from {len(objs)} objs "
              f"in {time.time() - t0:.0f}s -> {CACHE_PATH}")
    return records


# --------------------------------------------------------------------------
# map loading
# --------------------------------------------------------------------------
def load_map_raw():
    return json.loads(MAP_PATH.read_text())


def build_val2keys(m):
    v2k = defaultdict(list)
    mapped_addrs = set()
    for k, v in m.items():
        if k[:2].lower() != "0x" or not isinstance(v, str):
            continue
        v2k[v].append(k)
        mapped_addrs.add(norm_addr(k))
    return v2k, mapped_addrs


def load_denylist(m):
    dl = m.get("_denylist", [])
    return {norm_addr(a) for a in dl if isinstance(a, str)}


def load_icf_addrs():
    if not ALIASES.exists():
        return set()
    g = json.loads(ALIASES.read_text()).get("groups", [])
    return {norm_addr(grp["address"]) for grp in g if "address" in grp}


def load_alias_equiv():
    """name -> fold-group id; survivor + folded spellings share one id."""
    out = {}
    if not ALIASES.exists():
        return out
    for gi, grp in enumerate(json.loads(ALIASES.read_text()).get("groups", [])):
        for nm in [grp.get("survivor")] + list(grp.get("folded", [])):
            if nm:
                out[nm] = gi
    return out


# --------------------------------------------------------------------------
# reloc-target verification (the STL-T trap)
# --------------------------------------------------------------------------
_GENERATED_ADDR_RE = re.compile(
    r"^(fn|lbl|vftable|jumptable|except_data|except_record)_[0-9A-Fa-f]{8}$")


def _anon_target_sym(name):
    """Target-side symbols dtk/the renamer left anonymous (address-derived
    generated names, section/label symbols): cannot confirm, but no
    contradiction either."""
    if name.startswith((".", "@", "$", "??idx")):
        return True
    return bool(_GENERATED_ADDR_RE.match(name))


def compare_relocs(base_relocs, tgt_relocs, alias_of):
    """Position-wise reloc-target comparison between a base function body and a
    candidate target carve (bodies already byte-identical after masking).

    Returns (contradictions, n_unconfirmed, n_confirmed, n_positions).
    Positions are keyed (offset_in_body, reloc_type) because REFHI/REFLO pairs
    share an offset. PAIR relocs were already excluded at extraction.
    """
    bd = {}
    for off, nm, typ in base_relocs:
        bd.setdefault((off, typ), nm)
    td = {}
    for off, nm, typ in tgt_relocs:
        td.setdefault((off, typ), nm)
    contradictions = []
    unconfirmed = 0
    confirmed = 0
    keys = set(bd) | set(td)
    for k in sorted(keys):
        if k not in bd or k not in td:
            unconfirmed += 1  # position/type present on one side only
            continue
        nb, nt = bd[k], td[k]
        if _anon_target_sym(nt):
            unconfirmed += 1
            continue
        if "?A0x" in nb or "?A0x" in nt:
            # anonymous-namespace hash is machine/path-specific; never treat a
            # mismatch here as proof either way
            unconfirmed += 1
            continue
        if nb == nt or (nb in alias_of and nt in alias_of
                        and alias_of[nb] == alias_of[nt]):
            confirmed += 1
        else:
            contradictions.append((k[0], nb, nt))
    return contradictions, unconfirmed, confirmed, len(keys)


# --------------------------------------------------------------------------
# propose
# --------------------------------------------------------------------------
HAMMING_MAX_ABS = 2
HAMMING_MAX_FRAC = 0.02
FUZZY_MIN_RATIO = 0.80
FUZZY_MARGIN = 0.05
FUZZY_LEN_TOL = 0.15  # +/-15% length window for fuzzy candidates


def classify(entries, records, m, units):
    import difflib

    v2k, mapped_addrs = build_val2keys(m)
    denylist = load_denylist(m)
    icf_addrs = load_icf_addrs()
    alias_of = load_alias_equiv()

    # global content index over fn_ candidates
    content2fn = defaultdict(list)   # sha(body) -> [(addr, obj, size, relocs)]
    len2fn = defaultdict(list)       # len -> [(addr, obj, body)]
    for addr, obj, body, relocs in records:
        if addr in denylist:
            continue
        content2fn[sha(body)].append((addr, obj, len(body), relocs))
        len2fn[len(body)].append((addr, obj, body))

    # first pass: extract every entry's base body + relocs + hash, for the
    # "unique among all entries' base bodies" safety check.
    base_bodies = {}   # id(entry) -> (body, relocs)
    base_hash_count = defaultdict(int)
    for e in entries:
        uu = units.get(e["unit"])
        if not uu:
            continue
        bp = ROOT / uu["base_path"]
        if not bp.exists():
            continue
        try:
            bl = func_body_list_ex(str(bp))
        except Exception:  # noqa: BLE001
            continue
        # exact symbol, must be a real code definition
        for name, b, r in bl:
            if name == e["name"]:
                base_bodies[id(e)] = (b, r)
                base_hash_count[sha(b)] += 1
                break

    proposals = []
    for e in entries:
        name = e["unit_name"] = e["name"]
        unit = e["unit"]
        keys = v2k.get(name, [])
        excluded = name.startswith(THUNK_PREFIXES) or (TEMPLATE_MARK in name)
        excl_reason = ("SKIP-THUNK" if name.startswith(THUNK_PREFIXES)
                       else "SKIP-TEMPLATE" if TEMPLATE_MARK in name else None)

        p = {
            "unit": unit, "name": name, "addr_old": None, "addr_new": None,
            "class": None, "appliable": False,
            "base_len": None, "tgt_len": None, "n_candidates": 0,
            "hamming_words": None, "fuzzy_ratio": None,
            "reloc_confirmed": None, "reloc_unconfirmed": None,
            "reloc_contradicted_candidates": 0, "notes": "",
        }

        # (1) map-entry presence
        if len(keys) == 0:
            p["class"] = "SKIP-NOMAPENTRY"
            proposals.append(p)
            continue
        if len(keys) > 1:
            p["class"] = "SKIP-DUPMAPPED"
            p["notes"] = f"name mapped at {len(keys)} addrs: {keys}"
            proposals.append(p)
            continue
        addr_old = norm_addr(keys[0])
        p["addr_old"] = addr_old

        # (3) base body
        bb = base_bodies.get(id(e))
        if bb is None:
            p["class"] = "SKIP-NOBASE"
            p["notes"] = "base symbol not found as a real code definition"
            proposals.append(p)
            continue
        body, base_relocs = bb
        bh = sha(body)
        p["base_len"] = len(body)

        # (4) ALREADY-OK: currently-mapped target body already == base body.
        uu = units.get(unit)
        tgt_obj = ROOT / uu["target_path"] if uu else None
        cur_tgt_body = None
        if tgt_obj and tgt_obj.exists():
            try:
                for nm, b in func_body_list(str(tgt_obj)):
                    if nm == name:
                        cur_tgt_body = b
                        break
            except Exception:  # noqa: BLE001
                pass
        if cur_tgt_body is not None:
            p["tgt_len"] = len(cur_tgt_body)
            if cur_tgt_body == body:
                p["class"] = "ALREADY-OK"
                p["notes"] = "current mapped target already byte-identical to base"
                proposals.append(p)
                continue

        # (5) global strict match, with RELOC-TARGET verification (STL-T trap):
        # byte-identity after masking cannot distinguish template clones for a
        # different T with the same stride -- the discriminating info is the
        # reloc TARGET symbols. Drop candidates with any named-symbol
        # CONTRADICTION before uniqueness counting.
        raw_cands = [c for c in content2fn.get(bh, []) if c[0] != addr_old]

        def finish(cls, addr_new=None, appliable=False, note=""):
            p["class"] = cls
            if addr_new:
                p["addr_new"] = addr_new
            p["appliable"] = appliable and not excluded
            if excluded and appliable:
                note = (note + " " if note else "") + f"[non-appliable: {excl_reason}]"
            p["notes"] = note
            proposals.append(p)

        cands = []  # survivors: (addr, obj, size, unconf, conf)
        contradicted = []  # (addr, obj, [(off, base_sym, tgt_sym), ...])
        for addr_new, cobj, csize, trelocs in raw_cands:
            contra, unconf, conf, _npos = compare_relocs(base_relocs, trelocs,
                                                         alias_of)
            if contra:
                contradicted.append((addr_new, cobj, contra))
                continue
            cands.append((addr_new, cobj, csize, unconf, conf))
        n_contradicted = len(contradicted)
        contradicted_addrs = {c[0] for c in contradicted}
        p["n_candidates"] = len(cands)
        p["reloc_contradicted_candidates"] = n_contradicted
        if contradicted:
            p["contradictions"] = [
                {"addr": a, "obj": o,
                 "details": [f"off=0x{off:x} base={nb} tgt={nt}"
                             for off, nb, nt in det[:4]]}
                for a, o, det in contradicted[:3]]

        if len(cands) == 1:
            addr_new, cobj, csize, unconf, conf = cands[0]
            p["reloc_confirmed"] = conf
            p["reloc_unconfirmed"] = unconf
            # safety (a): candidate already a map key
            if addr_new in mapped_addrs:
                finish("SKIP-CONFLICT", addr_new,
                       note=f"candidate {addr_new} already a map key")
                continue
            # safety (b): candidate in an ICF fold group
            if addr_new in icf_addrs:
                finish("ICF-AMBIG", addr_new,
                       note=f"candidate {addr_new} is an ICF fold-group address")
                continue
            # safety (c): base body must be unique in own base obj AND across
            # all entries' base bodies
            own_dupe = _base_obj_dupe(e, name, body, units)
            if own_dupe > 1 or base_hash_count.get(bh, 0) > 1:
                finish("ICF-AMBIG", addr_new,
                       note=f"base body not unique (own-obj x{own_dupe}, "
                            f"across-entries x{base_hash_count.get(bh,0)})")
                continue
            reloc_note = (f"relocs {conf} confirmed / {unconf} unconfirmed; "
                          f"{n_contradicted} candidate(s) reloc-contradicted")
            if unconf == 0:
                # all positions confirmed (or zero relocs at all)
                finish("UNIQUE-IDENTICAL", addr_new, appliable=True,
                       note=f"byte-identical carve in {cobj} ({csize}B); "
                            + reloc_note)
            else:
                finish("UNIQUE-IDENTICAL-RELOC-UNCONFIRMED", addr_new,
                       appliable=False,
                       note=f"byte-identical carve in {cobj} ({csize}B); "
                            + reloc_note + "; proposal only")
            p["tgt_len"] = csize
            continue

        if len(cands) > 1:
            finish("MULTI", note=f"{len(cands)} byte-identical carves survive "
                   f"reloc filtering ({n_contradicted} contradicted): "
                   f"{[c[0] for c in cands[:6]]}")
            continue

        # (5b) hamming fallback: same-length fn_ candidates, tiny word budget.
        # Reloc-CONTRADICTED exact candidates are excluded -- they would
        # trivially re-enter with 0 differing words, laundering the
        # contradiction verdict.
        same_len = [c for c in len2fn.get(len(body), [])
                    if c[0] != addr_old and c[0] not in contradicted_addrs]
        bw = words(body)
        budget = max(HAMMING_MAX_ABS, int(HAMMING_MAX_FRAC * len(bw)))
        ham_hits = []
        for addr_new, cobj, cbody in same_len:
            cw = words(cbody)
            diff = sum(1 for x, y in zip(bw, cw) if x != y)
            if diff <= budget:
                ham_hits.append((diff, addr_new, cobj))
        if len(ham_hits) == 1:
            diff, addr_new, cobj = ham_hits[0]
            if addr_new in mapped_addrs:
                finish("SKIP-CONFLICT", addr_new,
                       note=f"hamming candidate {addr_new} already a map key")
                continue
            p["hamming_words"] = diff
            p["tgt_len"] = len(body)
            finish("LOW-HAMMING-SINGLE", addr_new, appliable=False,
                   note=f"{diff}/{len(bw)} words differ vs {cobj} (budget {budget}); "
                        f"proposal only, never applied")
            continue
        if len(ham_hits) > 1:
            finish("MULTI", note=f"{len(ham_hits)} same-length low-hamming candidates")
            continue

        # (5c) fuzzy fallback within the entry's OWN unit target obj
        # (contradicted exact candidates excluded -- byte-identical would score
        # ratio 1.0 and launder the contradiction verdict)
        best = _fuzzy_own_unit(tgt_obj, body, addr_old, contradicted_addrs)
        if best:
            ratio, addr_new, second, tlen = best
            if ratio >= FUZZY_MIN_RATIO and (ratio - second) >= FUZZY_MARGIN:
                if addr_new in mapped_addrs:
                    finish("SKIP-CONFLICT", addr_new,
                           note=f"fuzzy candidate {addr_new} already a map key")
                    continue
                p["fuzzy_ratio"] = round(ratio, 4)
                p["tgt_len"] = tlen
                finish("FUZZY-BEST", addr_new, appliable=False,
                       note=f"own-unit best fuzzy match ratio={ratio:.3f} "
                            f"(2nd={second:.3f}); NOT byte-identical -> proposal only, "
                            f"never applied (source near-miss vs retail carve)")
                continue

        if contradicted:
            # sole/only exact candidates were reloc-CONTRADICTED and nothing
            # else matches: surface the contradiction verdict itself (this is
            # either the STL-T trap doing its job, or a cascading-mispair
            # neighborhood where the map names backing the verification are
            # themselves wrong -- see notes/details).
            a0, o0, det0 = contradicted[0]
            finish("SKIP-RELOC-CONTRADICTED", a0,
                   note=f"{n_contradicted} byte-identical candidate(s) "
                        f"reloc-CONTRADICTED (first: {a0} in {o0}: "
                        f"{'; '.join(f'off=0x{off:x} base={nb} tgt={nt}' for off, nb, nt in det0[:3])}); "
                        f"addr_new records the contradicted candidate, NOT a proposal")
            continue
        finish("SKIP-NOMATCH", note="no exact / same-length-hamming / fuzzy match")

    return proposals


def _base_obj_dupe(e, name, body, units):
    """Count functions in the entry's own base obj whose masked body == body."""
    uu = units.get(e["unit"])
    if not uu:
        return 1
    bp = ROOT / uu["base_path"]
    try:
        cnt = 0
        for _nm, b in func_body_list(str(bp)):
            if b == body:
                cnt += 1
        return cnt
    except Exception:  # noqa: BLE001
        return 1


def _fuzzy_own_unit(tgt_obj, body, addr_old, exclude_addrs=frozenset()):
    """Return (best_ratio, best_addr, second_ratio, best_len) over fn_ syms in the
    entry's own target obj, length within +/-FUZZY_LEN_TOL of the base body."""
    import difflib
    if not tgt_obj or not tgt_obj.exists():
        return None
    lo = len(body) * (1 - FUZZY_LEN_TOL)
    hi = len(body) * (1 + FUZZY_LEN_TOL)
    bw = words(body)
    scored = []
    try:
        for nm, b in func_body_list(str(tgt_obj)):
            if not nm.startswith("fn_"):
                continue
            a = norm_addr(nm)
            if a == addr_old or a in exclude_addrs or not (lo <= len(b) <= hi):
                continue
            r = difflib.SequenceMatcher(a=bw, b=words(b), autojunk=False).ratio()
            scored.append((r, a, len(b)))
    except Exception:  # noqa: BLE001
        return None
    if not scored:
        return None
    scored.sort(reverse=True)
    best_r, best_a, best_len = scored[0]
    second = scored[1][0] if len(scored) > 1 else 0.0
    return best_r, best_a, second, best_len


# --------------------------------------------------------------------------
# apply (textual, in-place)
# --------------------------------------------------------------------------
def do_apply(proposals, args):
    appliable = [p for p in proposals
                 if p["class"] == args.apply_class and p["appliable"] and p["addr_new"]]
    if args.only:
        want = set(args.only.split(","))
        appliable = [p for p in appliable if p["name"] in want]
    if args.limit:
        appliable = appliable[:args.limit]
    if not appliable:
        print("no appliable proposals for the given filters")
        return

    text = MAP_PATH.read_text()
    lines = text.split("\n")
    before = json.loads(text)
    n_before = len(before)

    changed = 0
    for p in appliable:
        name = p["name"]
        old = p["addr_old"]
        new = p["addr_new"]
        # find the exact line "0x<old>": "<name>" -- match by name on the value
        # side + normalized addr on the key side; preserve leading whitespace
        # and trailing comma exactly.
        hit_idx = None
        esc = re.escape(name)
        keyre = re.compile(r'^(\s*")(0[xX][0-9a-fA-F]+)("\s*:\s*")' + esc + r'(".*)$')
        for i, ln in enumerate(lines):
            mo = keyre.match(ln)
            if mo and norm_addr(mo.group(2)) == old:
                hit_idx = i
                break
        if hit_idx is None:
            print(f"  SKIP {name}: line not found for {old}")
            continue
        mo = keyre.match(lines[hit_idx])
        newkey = new  # lowercase 0x%08x
        lines[hit_idx] = f"{mo.group(1)}{newkey}{mo.group(3)}{name}{mo.group(4)}"
        changed += 1
        print(f"  REPOINT {name}: {old} -> {new}")

    new_text = "\n".join(lines)
    # validate by json.load
    after = json.loads(new_text)
    assert len(after) == n_before, f"entry count changed {n_before}->{len(after)}"
    for p in appliable:
        assert after.get(p["addr_new"]) == p["name"], \
            f"map[{p['addr_new']}] != {p['name']}"
    # verify no unrelated entries changed
    moved_names = {p["name"] for p in appliable}
    for k, v in before.items():
        if isinstance(v, str) and v not in moved_names:
            assert after.get(k) == v, f"unrelated entry {k} changed"
    MAP_PATH.write_text(new_text)
    print(f"\napplied {changed} repoints; validated json ({len(after)} entries).")
    print("REMINDER: `touch config/45410914/config.yml` before rebuilding so the "
          "renamer re-SPLITs (it never un-names a stale symbol).")


# --------------------------------------------------------------------------
def summarize(proposals):
    from collections import Counter
    c = Counter(p["class"] for p in proposals)
    return dict(sorted(c.items(), key=lambda kv: (-kv[1], kv[0])))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mispair", default=MISPAIR_DEFAULT)
    ap.add_argument("--extra-entries", default=None,
                    help="extra entries json (same schema as mispair file; "
                         "minimal fields unit+name)")
    ap.add_argument("--out", default=PROPOSALS_DEFAULT)
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("--class", dest="apply_class", default="UNIQUE-IDENTICAL")
    ap.add_argument("--only", default=None)
    ap.add_argument("--limit", type=int, default=None)
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()

    entries = json.loads(Path(args.mispair).read_text())
    if args.extra_entries:
        extra = json.loads(Path(args.extra_entries).read_text())
        for x in extra:
            x.setdefault("extra", True)
        entries = entries + extra
    m = load_map_raw()
    units = {u["name"]: u for u in json.loads(OBJDIFF.read_text())["units"]}
    records = build_target_index(verbose=not args.quiet)

    proposals = classify(entries, records, m, units)

    summary = summarize(proposals)
    out = {"summary": summary, "n": len(proposals), "proposals": proposals}
    Path(args.out).write_text(json.dumps(out, indent=1))

    print("\n=== class summary (%d entries) ===" % len(proposals))
    for k, v in summary.items():
        print(f"  {k:18s} {v}")
    print(f"proposals -> {args.out}")

    for cls in ("UNIQUE-IDENTICAL", "UNIQUE-IDENTICAL-RELOC-UNCONFIRMED"):
        ui = [p for p in proposals if p["class"] == cls]
        print(f"\n=== {cls} ({len(ui)}) ===")
        for p in ui:
            print(f"  {p['name']}  {p['addr_old']}->{p['addr_new']}  "
                  f"base={p['base_len']}B tgt={p['tgt_len']}B "
                  f"relocs={p['reloc_confirmed']}c/{p['reloc_unconfirmed']}u "
                  f"appliable={p['appliable']}")

    if args.apply:
        print("\n=== APPLY ===")
        do_apply(proposals, args)


if __name__ == "__main__":
    main()
