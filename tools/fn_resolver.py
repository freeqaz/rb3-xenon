#!/usr/bin/env python3
"""fn_resolver.py — resolve anonymous fn_8XXXXXXX addresses to identities.

RB3's retail XEX has 66k anonymous functions named fn_8XXXXXXX. This tool
aggregates all available identity evidence across 7 tiers and returns the
best identity (mangled name + demangled + evidence source + confidence) for
any fn_ address.

Evidence tiers (highest → lowest confidence):
  T1   decomp.db named — function is already a named symbol in our compiled
       unit (renamed via target_symbol_map; COMPLETE or named in report.json).
  T2   target_symbol_map.json — dtk split target obj renamed from leaked map
       or identification campaign (0xADDR → mangled name).
  T3   dc3_content_match / game_content_match — byte-identical SHA match to a
       named DC3/game function.
  T3b  gameid_crossval — BinDiff ∩ BSim dual-tool agreement on the same
       rb3-Wii source stem; measured 0.95 precision on 25 known pins. Gives a
       source-file stem only (no mangled name). From
       docs/decomp/gameid/crossval_agree.json (146 functions, 93 unpinned).
  T4   global_fuzzy_pairs.json — Jaccard-similar (fuzzy) DC3 match.
  T5   unified_id (bindiff / vtable / rtti) — structural similarity or vtable
       slot identification from the unified_id_*.json files.
  T6   unified_id_rb3wii.json — cross-arch BinDiff match to rb3-Wii named fns.
  T7   autoid.json — string-fingerprint source-file attribution (gives src file
       only, no mangled name; lower confidence).

Usage:
  tools/fn_resolver.py resolve <addr> [addr...]
      Print best identity per address. Addresses may be 0x82XXXXXX or
      fn_82XXXXXX or plain 82XXXXXX.

  tools/fn_resolver.py index [--out FILE]
      Build/refresh fn_resolver_index.json aggregating ALL tiers for every
      fn_ address that has any evidence. Print coverage stats.
      Default output: ~/tmp/fn_resolver_index.json

  tools/fn_resolver.py unresolved --unit <unit>
      List the fn_ callees referenced by a unit's near-miss functions that
      still lack identity. The bl-target worklist.
      <unit> may be a short name like 'VocalPlayer' or the full report.json
      unit name 'default/VocalPlayer'.

  tools/fn_resolver.py validate [--n 10]
      Pick N known-matched functions, run resolve() on their addresses, and
      confirm the landed names appear in the results.

Examples:
  tools/fn_resolver.py resolve 0x82760000 fn_82758380
  tools/fn_resolver.py index
  tools/fn_resolver.py unresolved --unit VocalPlayer
  tools/fn_resolver.py validate
"""

import argparse
import json
import os
import re
import sqlite3
import sys
from collections import defaultdict
from pathlib import Path

# ---------------------------------------------------------------------------
# Paths (relative to repo root)
# ---------------------------------------------------------------------------
ROOT = Path(__file__).resolve().parent.parent


def _find_main_repo() -> Path:
    """Find the main (primary) working tree of this git repo.

    In a git worktree the `.git` entry is a file containing the path to the
    actual git dir (e.g. `gitdir: /main/repo/.git/worktrees/foo`).  The main
    working tree is the one where `.git` is a *directory*.  Many data files
    (unified_id*.json, decomp.db, etc.) are gitignored and only exist in the
    main working tree, so we fall back there when a local path is missing.
    """
    import subprocess
    try:
        out = subprocess.check_output(
            ["git", "worktree", "list", "--porcelain"],
            cwd=ROOT, stderr=subprocess.DEVNULL, text=True
        )
        for block in out.strip().split("\n\n"):
            lines = {l.split()[0]: " ".join(l.split()[1:]) for l in block.strip().splitlines() if l}
            if "HEAD" in lines:
                worktree_path = Path(block.strip().splitlines()[0].split(None, 1)[1])
                # The main worktree has .git as a directory
                if (worktree_path / ".git").is_dir():
                    return worktree_path
    except Exception:
        pass
    return ROOT  # fallback: same tree


def _repo_path(rel: str) -> Path:
    """Return the best path for a repo-relative file.

    Checks the local ROOT first (so a worktree with its own copy wins),
    then falls back to the main working tree.
    """
    local = ROOT / rel
    if local.exists():
        return local
    main = _find_main_repo()
    return main / rel


DB_PATH         = _repo_path("decomp.db")
REPORT_PATH     = ROOT / "build" / "45410914" / "report.json"   # worktree has its own build
SPLITS_PATH     = ROOT / "config" / "45410914" / "splits.txt"   # worktree has its own splits
TARGET_MAP_PATH = ROOT / "scripts" / "target_symbol_map.json"   # tracked, in worktree
DC3_MATCH_PATH  = _repo_path("dc3_content_match.json")
GAME_MATCH_PATH = _repo_path("game_content_match.json")
FUZZY_PATH      = _repo_path("global_fuzzy_pairs.json")

_main = _find_main_repo()
UNIFIED_ID_FILES = [
    # (path, source_tag, confidence_field)
    (_repo_path("unified_id.json"),         "bindiff_dc3",  "confidence"),
    (_repo_path("unified_id_vtable.json"),  "vtable",       "confidence"),
    (_repo_path("unified_id_rtti.json"),    "rtti",         "confidence"),
    (_repo_path("unified_id_rtti_low.json"),"rtti_low",     "confidence"),
]
RB3WII_PATH      = _repo_path("unified_id_rb3wii.json")
AUTOID_PATH      = _repo_path("autoid.json")
GAMEID_XVAL_PATH = ROOT / "docs" / "decomp" / "gameid" / "crossval_agree.json"
GHIDRIFF_PATH    = _repo_path("ghidriff_identities.json")

DEFAULT_INDEX   = Path.home() / "tmp" / "fn_resolver_index.json"

# ---------------------------------------------------------------------------
# Address normalisation
# ---------------------------------------------------------------------------
ADDR_RE = re.compile(r'(?:fn_|0x)?([0-9A-Fa-f]{8})', re.IGNORECASE)


def norm_addr(s: str) -> int | None:
    """Return integer address from fn_ADDR / 0xADDR / ADDR strings, or None."""
    s = s.strip()
    m = ADDR_RE.fullmatch(s)
    if m:
        return int(m.group(1), 16)
    # Also accept full hex like 0x8275A000 with variable length
    m2 = re.fullmatch(r'(?:fn_|0x)?([0-9A-Fa-f]+)', s, re.IGNORECASE)
    if m2:
        v = int(m2.group(1), 16)
        if 0x82000000 <= v <= 0x83000000:  # plausible XEX .text range
            return v
    return None


def addr_key(addr: int) -> str:
    """Canonical hex string key used in data files: 0x8XXXXXXX lowercase."""
    return f"0x{addr:08x}"


def addr_key_upper(addr: int) -> str:
    return f"0x{addr:08X}"


def fn_name(addr: int) -> str:
    return f"fn_{addr:08X}"


# ---------------------------------------------------------------------------
# Identity record
# ---------------------------------------------------------------------------
class Identity:
    """One candidate identity for a function address."""
    __slots__ = ("mangled", "demangled", "source", "confidence", "extra")

    def __init__(self, mangled: str, demangled: str | None,
                 source: str, confidence: float, extra: dict | None = None):
        self.mangled    = mangled
        self.demangled  = demangled or mangled
        self.source     = source
        self.confidence = confidence
        self.extra      = extra or {}

    def to_dict(self) -> dict:
        d = {
            "mangled":    self.mangled,
            "demangled":  self.demangled,
            "source":     self.source,
            "confidence": self.confidence,
        }
        if self.extra:
            d["extra"] = self.extra
        return d

    def __repr__(self):
        return (f"Identity(src={self.source!r}, conf={self.confidence:.2f}, "
                f"name={self.demangled!r})")


# ---------------------------------------------------------------------------
# Data loaders (lazy, cached at module level)
# ---------------------------------------------------------------------------
_cache: dict = {}


def _load_json(path: Path) -> object:
    key = str(path)
    if key not in _cache:
        if not path.exists():
            _cache[key] = None
            return None
        with open(path) as f:
            _cache[key] = json.load(f)
    return _cache[key]


def _get_db() -> sqlite3.Connection:
    if "db" not in _cache:
        if not DB_PATH.exists():
            _cache["db"] = None
        else:
            conn = sqlite3.connect(f"file:{DB_PATH}?mode=ro", uri=True)
            conn.row_factory = sqlite3.Row
            _cache["db"] = conn
    return _cache["db"]


def _get_report_addr_map() -> dict[int, tuple[str, str, str, float | None]]:
    """Return {addr: (mangled, unit_name, size, match_pct)} for named fns in report.json.

    Named functions in report.json have their absolute address computed from
    the unit's splits.txt text start + in-unit byte offset. This is the
    authoritative source for functions that have already been identified via
    the MSVC-symbol-renamer pipeline (target_symbol_map → obj-renamer).
    """
    if "report_addr_map" in _cache:
        return _cache["report_addr_map"]
    if not REPORT_PATH.exists():
        _cache["report_addr_map"] = {}
        return {}
    with open(REPORT_PATH) as f:
        rpt = json.load(f)
    splits = _parse_splits()
    out: dict[int, tuple] = {}
    for unit in rpt.get("units", []):
        short = unit["name"].replace("default/", "")
        cpp = short + ".cpp"
        text_start = splits.get(cpp, 0)
        for fn in unit.get("functions", []):
            name = fn["name"]
            if name.startswith("fn_"):
                continue  # anonymous; skip (T1a handles via DB)
            offset = int(fn.get("address", 0))
            if text_start:
                addr = text_start + offset
            else:
                continue
            pct = fn.get("fuzzy_match_percent", fn.get("match_percent_normalized"))
            out[addr] = (name, unit["name"], int(fn.get("size", 0)), pct)
    _cache["report_addr_map"] = out
    return out


# ---------------------------------------------------------------------------
# Per-tier lookup helpers (all return list[Identity])
# ---------------------------------------------------------------------------

def _t1_decomp_db(addr: int) -> list[Identity]:
    """T1: named symbol via two sub-tiers.

    T1a: report.json + splits.txt computed address → mangled name. This is
         the most authoritative source — if a function has been renamed by the
         obj-renamer pipeline it shows up here at its true absolute address.
    T1b: decomp.db functions table — fallback for functions that are named in
         the DB (e.g. via older ingestion paths) but don't have a matched
         address in the report yet.
    """
    results = []

    # T1a: report.json address map (highest confidence — pipeline-confirmed identity)
    rmap = _get_report_addr_map()
    if addr in rmap:
        mangled, unit_name, size, pct = rmap[addr]
        demangled = _demangle_simple(mangled)
        results.append(Identity(
            mangled=mangled,
            demangled=demangled,
            source="decomp_db_named",
            confidence=0.995,
            extra={"unit": unit_name, "match_pct": pct, "size": size},
        ))

    # T1b: decomp.db lookup by fn_ symbol name
    db = _get_db()
    if db is not None:
        sym = fn_name(addr)
        rows = db.execute(
            "SELECT symbol, demangled, unit, current_percent, verdict "
            "FROM functions WHERE symbol = ?",
            (sym,)
        ).fetchall()
        for row in rows:
            symbol = row["symbol"]
            demangled = row["demangled"] or symbol
            if row["unit"] and symbol.startswith("fn_"):
                results.append(Identity(
                    mangled=symbol,
                    demangled=f"<anon in {row['unit']}>",
                    source="decomp_db_unit",
                    confidence=0.5,
                    extra={"unit": row["unit"], "match_pct": row["current_percent"]},
                ))
    return results


def _norm_rb3_addr(s: str) -> int | None:
    """Normalise an rb3_addr field (0x82...) to int. Handles mixed case."""
    try:
        return int(s, 16)
    except (ValueError, TypeError):
        return None


# Pre-built lookup tables — built once at first use, keyed by int address.

def _get_target_symbol_map_idx() -> dict[int, str]:
    """addr → mangled_name (from target_symbol_map.json)."""
    if "tsm_idx" not in _cache:
        tsm = _load_json(TARGET_MAP_PATH)
        idx: dict[int, str] = {}
        if tsm:
            for k, v in tsm.items():
                a = _norm_rb3_addr(k)
                if a and v and not v.startswith("fn_"):
                    idx[a] = v
        _cache["tsm_idx"] = idx
    return _cache["tsm_idx"]


def _get_dc3_content_idx() -> dict[int, dict]:
    """addr → entry (from dc3_content_match.json)."""
    if "dc3_idx" not in _cache:
        data = _load_json(DC3_MATCH_PATH)
        idx: dict[int, dict] = {}
        if data:
            for entry in data:
                a = _norm_rb3_addr(entry.get("rb3_addr", ""))
                if a:
                    idx.setdefault(a, entry)  # first entry wins
        _cache["dc3_idx"] = idx
    return _cache["dc3_idx"]


def _get_game_content_idx() -> dict[int, dict]:
    """addr → entry (from game_content_match.json)."""
    if "game_idx" not in _cache:
        data = _load_json(GAME_MATCH_PATH)
        idx: dict[int, dict] = {}
        if data:
            for entry in data:
                a = _norm_rb3_addr(entry.get("rb3_addr", ""))
                if a:
                    idx.setdefault(a, entry)
        _cache["game_idx"] = idx
    return _cache["game_idx"]


def _get_fuzzy_idx() -> dict[int, list[dict]]:
    """addr → list[unique entries] (from global_fuzzy_pairs.json).

    The file may have duplicate (addr, dc3_name) pairs — deduplicate by name
    so the output doesn't repeat identical candidates.
    """
    if "fuzzy_idx" not in _cache:
        data = _load_json(FUZZY_PATH)
        idx: dict[int, dict[str, dict]] = defaultdict(dict)  # addr -> {name: entry}
        if data:
            for entry in data:
                a = _norm_rb3_addr(entry.get("rb3_addr", ""))
                if a:
                    name = entry.get("dc3_name", "")
                    if name not in idx[a]:
                        idx[a][name] = entry
        _cache["fuzzy_idx"] = {a: list(entries.values()) for a, entries in idx.items()}
    return _cache["fuzzy_idx"]


def _get_unified_id_idx() -> dict[str, dict[int, list[dict]]]:
    """source_tag → addr → list[entry] for all unified_id files."""
    if "uid_idx" not in _cache:
        result: dict[str, dict[int, list[dict]]] = {}
        for path, src_tag, _conf_field in UNIFIED_ID_FILES:
            data = _load_json(path)
            idx: dict[int, list[dict]] = defaultdict(list)
            if data:
                for entry in data:
                    a = _norm_rb3_addr(entry.get("rb3_addr", ""))
                    if a:
                        idx[a].append(entry)
            result[src_tag] = dict(idx)
        _cache["uid_idx"] = result
    return _cache["uid_idx"]


def _get_rb3wii_idx() -> dict[int, list[dict]]:
    """addr → list[entry] from unified_id_rb3wii.json."""
    if "rb3wii_idx" not in _cache:
        data = _load_json(RB3WII_PATH)
        idx: dict[int, list[dict]] = defaultdict(list)
        if data:
            for entry in data:
                a = _norm_rb3_addr(entry.get("rb3_addr", ""))
                if a:
                    idx[a].append(entry)
        _cache["rb3wii_idx"] = dict(idx)
    return _cache["rb3wii_idx"]


def _get_autoid_idx() -> dict[int, dict]:
    """addr → entry from autoid.json."""
    if "autoid_idx" not in _cache:
        data = _load_json(AUTOID_PATH)
        idx: dict[int, dict] = {}
        if data:
            for entry in data:
                fn = entry.get("fn", "")
                if fn.startswith("fn_"):
                    try:
                        a = int(fn[3:], 16)
                        idx[a] = entry
                    except ValueError:
                        pass
        _cache["autoid_idx"] = idx
    return _cache["autoid_idx"]


def _get_gameid_xval_idx() -> dict[int, dict]:
    """addr → entry from docs/decomp/gameid/crossval_agree.json.

    Returns empty dict if the file is absent (graceful degradation in worktrees
    that pre-date the file, though in practice docs/ is tracked and travels with
    the repo).
    """
    if "gameid_xval_idx" not in _cache:
        data = _load_json(GAMEID_XVAL_PATH)
        idx: dict[int, dict] = {}
        if data and isinstance(data, dict):
            for entry in data.get("agree_fns", []):
                a = _norm_rb3_addr(entry.get("addr", ""))
                if a:
                    idx[a] = entry
        _cache["gameid_xval_idx"] = idx
    return _cache["gameid_xval_idx"]


def _get_ghidriff_idx() -> dict[int, list[dict]]:
    """addr → list[entry] from ghidriff_identities.json (Bank 8 Wii, ACCEPT tier).

    The file uses 0x-prefixed lowercase rb3_addr (Xenon address) and
    wii_addr_bank8 (Bank 8 Wii address — NOT Bank 5). Gracefully returns {}
    when the file is absent (it is gitignored and regenerated by
    tools/ghidra/ingest_ghidriff_accepts.py in the rb3 repo).
    """
    if "ghidriff_idx" not in _cache:
        data = _load_json(GHIDRIFF_PATH)
        idx: dict[int, list[dict]] = defaultdict(list)
        if data:
            for entry in data:
                a = _norm_rb3_addr(entry.get("rb3_addr", ""))
                if a:
                    idx[a].append(entry)
        _cache["ghidriff_idx"] = dict(idx)
    return _cache["ghidriff_idx"]


# ---------------------------------------------------------------------------
# Per-tier lookup helpers — now all O(1) via index tables
# ---------------------------------------------------------------------------


def _t2_target_symbol_map(addr: int) -> list[Identity]:
    """T2: scripts/target_symbol_map.json — address → mangled name."""
    mangled = _get_target_symbol_map_idx().get(addr)
    if mangled:
        return [Identity(
            mangled=mangled,
            demangled=_demangle_simple(mangled),
            source="target_symbol_map",
            confidence=0.97,
        )]
    return []


def _t3_content_match(addr: int) -> list[Identity]:
    """T3: byte-identical SHA match (dc3_content_match, game_content_match)."""
    results = []

    entry = _get_dc3_content_idx().get(addr)
    if entry:
        name = entry.get("dc3_name", "")
        if name and not name.startswith("fn_"):
            results.append(Identity(
                mangled=name,
                demangled=_demangle_simple(name),
                source="dc3_content_match",
                confidence=0.95,
                extra={"dc3_obj": entry.get("dc3_obj"), "sha": entry.get("masked_sha")},
            ))

    entry2 = _get_game_content_idx().get(addr)
    if entry2:
        name = entry2.get("mangled_name", "")
        if name and not name.startswith("fn_"):
            results.append(Identity(
                mangled=name,
                demangled=_demangle_simple(name),
                source="game_content_match",
                confidence=0.95,
                extra={"unit": entry2.get("unit"), "sha": entry2.get("masked_sha")},
            ))
    return results


def _t3b_gameid_crossval(addr: int) -> list[Identity]:
    """T3b: docs/decomp/gameid/crossval_agree.json — dual-tool BinDiff∩BSim agreement.

    Yields a file-stem identity (no mangled name) at confidence 0.95, which is
    the measured precision on 25 known pins.  Placed between the byte-identical
    content-match tier (also 0.95) and the fuzzy Jaccard tier (≤0.90).

    NOTE: This tier gives only a source-file stem (e.g. "BandTrack"), not a
    mangled symbol name.  The demangled field is rendered as "<in Stem.cpp>" so
    callers can distinguish stem-only evidence from full-name evidence.
    """
    entry = _get_gameid_xval_idx().get(addr)
    if entry is None:
        return []
    stem = entry.get("stem", "")
    bindiff_conf = float(entry.get("bindiff_conf", 0.0))
    bsim_sim = float(entry.get("bsim_sim", 0.0))
    already_pinned = bool(entry.get("already_pinned", False))
    # Confidence is the measured ensemble precision (0.95), not the per-tool scores.
    conf = 0.95
    mangled = fn_name(addr)  # no mangled name available; keep as fn_ placeholder
    return [Identity(
        mangled=mangled,
        demangled=f"<in {stem}.cpp>",
        source="gameid_crossval",
        confidence=conf,
        extra={
            "stem": stem,
            "bindiff_conf": bindiff_conf,
            "bsim_sim": bsim_sim,
            "already_pinned": already_pinned,
        },
    )]


def _t4_fuzzy_pairs(addr: int) -> list[Identity]:
    """T4: global_fuzzy_pairs.json — Jaccard-similar DC3 match."""
    entries = _get_fuzzy_idx().get(addr, [])
    results = []
    for entry in entries:
        name = entry.get("dc3_name", "")
        jaccard = float(entry.get("jaccard", 0.0))
        if name and not name.startswith("fn_"):
            results.append(Identity(
                mangled=name,
                demangled=_demangle_simple(name),
                source="fuzzy_pairs",
                confidence=0.7 + 0.2 * jaccard,  # 0.70–0.90
                extra={"jaccard": jaccard, "dc3_obj": entry.get("dc3_obj")},
            ))
    return results


def _t4b_ghidriff(addr: int) -> list[Identity]:
    """T4b: ghidriff_identities.json — Wii↔Xenon ACCEPT-tier ghidriff pairs.

    Calibrated confidences (cross-arch, capped below T3 byte-SHA tier):
      ExactInstructionsFunctionHasher / Implied Match  → 0.94
      SymbolsHash                                       → 0.95
      SwitchSigHasher                                   → 0.90
      BSIM simconf >= 15                                → 0.93
      BSIM below threshold (shouldn't occur; filtered)  → 0.88

    The wii_addr_bank8 field carries a Bank 8 Wii address (NOT Bank 5).
    Do NOT feed this into _get_rb3wii_idx() — it expects Bank 5.

    Source: tools/ghidra/ingest_ghidriff_accepts.py (in rb3 repo).
    Provenance tag: entry["source"] == "ghidriff-run3".
    """
    entries = _get_ghidriff_idx().get(addr, [])
    results = []
    for entry in entries:
        wii_sym = entry.get("wii_symbol", "")
        if not wii_sym or wii_sym.startswith("fn_"):
            continue
        match_types = entry.get("match_types", [])
        simconf = entry.get("bsim_simconf")

        # Calibrated confidence per match type
        if "ExactInstructionsFunctionHasher" in match_types or "Implied Match" in match_types:
            conf = 0.94
        elif "SymbolsHash" in match_types:
            conf = 0.95
        elif "SwitchSigHasher" in match_types:
            conf = 0.90
        elif "BSIM" in match_types and simconf is not None and simconf >= 15.0:
            conf = 0.93
        else:
            conf = 0.88  # BSIM below threshold — shouldn't appear if file was filtered

        # Use the demangled form if distinct from CW-mangled
        demangled = entry.get("wii_symbol_demangled") or wii_sym

        results.append(Identity(
            mangled=wii_sym,       # CW-mangled Wii symbol
            demangled=demangled,   # Ghidra demangled (often same as CW-mangled)
            source="ghidriff_wii_b8",
            confidence=conf,
            extra={
                "match_types": match_types,
                "tu": entry.get("tu"),
                "category": entry.get("category"),
                "bsim_simconf": simconf,
                "source_tag": entry.get("source", "ghidriff-run3"),
                "wii_addr_bank8": entry.get("wii_addr_bank8"),
            },
        ))
    return results


def _t5_unified_id(addr: int) -> list[Identity]:
    """T5: unified_id_*.json — bindiff / vtable / rtti identification."""
    results = []
    uid_idx = _get_unified_id_idx()
    for path, src_tag, conf_field in UNIFIED_ID_FILES:
        for entry in uid_idx.get(src_tag, {}).get(addr, []):
            name = entry.get("dc3_name", "")
            conf = float(entry.get(conf_field, 0.8))
            if name and not name.startswith("fn_"):
                results.append(Identity(
                    mangled=name,
                    demangled=entry.get("dc3_name_demangled") or _demangle_simple(name),
                    source=src_tag,
                    confidence=conf,
                    extra={
                        "algorithm": entry.get("algorithm"),
                        "bindiff_src": entry.get("bindiff_src"),
                        "dc3_obj": entry.get("dc3_obj"),
                        "dc3_inline_only": entry.get("dc3_inline_only"),
                    },
                ))
    return results


def _t6_rb3wii(addr: int) -> list[Identity]:
    """T6: unified_id_rb3wii.json — cross-arch BinDiff to rb3-Wii names."""
    entries = _get_rb3wii_idx().get(addr, [])
    results = []
    for entry in entries:
        wii_name = entry.get("wii_name", "")
        conf = float(entry.get("confidence", 0.7))
        if wii_name:
            results.append(Identity(
                mangled=wii_name,   # Wii names are demangled/human style
                demangled=wii_name,
                source="rb3wii_bindiff",
                confidence=min(conf, 0.85),  # cap: cross-arch = noisier
                extra={
                    "similarity": entry.get("similarity"),
                    "algorithm": entry.get("algorithm"),
                    "bindiff_src": entry.get("bindiff_src"),
                },
            ))
    return results


def _t7_autoid(addr: int) -> list[Identity]:
    """T7: autoid.json — string-fingerprint source-file attribution."""
    entry = _get_autoid_idx().get(addr)
    if entry is None:
        return []
    fn = fn_name(addr)
    src = entry.get("src", "")
    score = int(entry.get("score", 0))
    n_str = int(entry.get("n_strings", 0))
    conf = min(0.3 + 0.03 * score, 0.65)  # max ~0.65 for score≥11
    return [Identity(
        mangled=fn,
        demangled=f"<in {os.path.basename(src)}>",
        source="autoid_fingerprint",
        confidence=conf,
        extra={"src_file": src, "score": score, "n_strings": n_str,
               "matched_strings": entry.get("matched_strings", [])[:10]},
    )]


# ---------------------------------------------------------------------------
# Master resolver
# ---------------------------------------------------------------------------
TIER_ORDER = ["decomp_db_named", "target_symbol_map", "dc3_content_match",
              "game_content_match", "gameid_crossval", "fuzzy_pairs",
              "ghidriff_wii_b8",   # T4b: ghidriff ACCEPT pairs (Bank 8 Wii, ~0.93 precision)
              "bindiff_dc3", "vtable", "rtti", "rtti_low",
              "rb3wii_bindiff", "autoid_fingerprint", "decomp_db_unit"]
TIER_RANK = {t: i for i, t in enumerate(TIER_ORDER)}


def resolve_all(addr: int) -> list[Identity]:
    """Return all identities for addr across all tiers, best first."""
    candidates: list[Identity] = []
    candidates.extend(_t1_decomp_db(addr))
    candidates.extend(_t2_target_symbol_map(addr))
    candidates.extend(_t3_content_match(addr))
    candidates.extend(_t3b_gameid_crossval(addr))
    candidates.extend(_t4_fuzzy_pairs(addr))
    candidates.extend(_t4b_ghidriff(addr))   # T4b: ghidriff ACCEPT (Bank 8 Wii)
    candidates.extend(_t5_unified_id(addr))
    candidates.extend(_t6_rb3wii(addr))
    candidates.extend(_t7_autoid(addr))

    # Sort: tier rank first, then descending confidence
    candidates.sort(key=lambda x: (TIER_RANK.get(x.source, 99), -x.confidence))
    return candidates


def resolve_best(addr: int) -> Identity | None:
    """Return the single best identity for addr, or None."""
    all_ids = resolve_all(addr)
    return all_ids[0] if all_ids else None


# ---------------------------------------------------------------------------
# Simple demangler (heuristic — no undname binary needed)
# ---------------------------------------------------------------------------
_DEMANGLE_CACHE: dict = {}


def _demangle_simple(mangled: str) -> str:
    """Best-effort MSVC demangling using the decomp.db cache, then heuristics."""
    if mangled in _DEMANGLE_CACHE:
        return _DEMANGLE_CACHE[mangled]
    # Try DB first
    db = _get_db()
    if db:
        row = db.execute(
            "SELECT demangled FROM functions WHERE symbol=? AND demangled IS NOT NULL",
            (mangled,)
        ).fetchone()
        if row and row[0]:
            _DEMANGLE_CACHE[mangled] = row[0]
            return row[0]
    # Heuristic: if already human-readable (contains space / :: and no @@)
    if "::" in mangled and "@@" not in mangled:
        _DEMANGLE_CACHE[mangled] = mangled
        return mangled
    _DEMANGLE_CACHE[mangled] = mangled
    return mangled


# ---------------------------------------------------------------------------
# Report / index builders
# ---------------------------------------------------------------------------

def _parse_splits() -> dict[str, int]:
    """Return {cpp_name: text_start_addr} from splits.txt."""
    if not SPLITS_PATH.exists():
        return {}
    result: dict[str, int] = {}
    current = None
    for line in SPLITS_PATH.read_text().splitlines():
        m = re.match(r'^(\S.*\.(?:cpp|c|cxx)):$', line)
        if m:
            current = m.group(1)
        m2 = re.match(r'\s+\.text\s+start:(0x[0-9A-Fa-f]+)', line)
        if m2 and current:
            result[current] = int(m2.group(1), 16)
    return result


def _load_report_fn_map() -> dict[int, dict]:
    """Return {addr: {name, unit, match_pct, size}} for every function in report.json."""
    if not REPORT_PATH.exists():
        return {}
    with open(REPORT_PATH) as f:
        rpt = json.load(f)
    splits = _parse_splits()
    out: dict[int, dict] = {}
    for unit in rpt.get("units", []):
        unit_name = unit["name"]
        short = unit_name.replace("default/", "")
        cpp = short + ".cpp"
        text_start = splits.get(cpp, 0)
        for fn in unit.get("functions", []):
            fn_sym = fn["name"]
            offset = int(fn.get("address", 0))
            if fn_sym.startswith("fn_"):
                addr = int(fn_sym[3:], 16)
            elif text_start:
                addr = text_start + offset
            else:
                continue
            out[addr] = {
                "name": fn_sym,
                "unit": unit_name,
                "match_pct": fn.get("fuzzy_match_percent", fn.get("match_percent_normalized", 0.0)),
                "size": int(fn.get("size", 0)),
            }
    return out


# ---------------------------------------------------------------------------
# Subcommands
# ---------------------------------------------------------------------------

def cmd_resolve(args):
    addrs = []
    for s in args.addrs:
        a = norm_addr(s)
        if a is None:
            print(f"[warn] cannot parse address: {s!r}", file=sys.stderr)
        else:
            addrs.append(a)
    if not addrs:
        print("No valid addresses given.", file=sys.stderr)
        sys.exit(1)

    for addr in addrs:
        ids = resolve_all(addr)
        print(f"\n{fn_name(addr)} (0x{addr:08X})")
        if not ids:
            print("  <no identity found>")
        else:
            for i, ident in enumerate(ids[:5]):  # top-5
                marker = "★" if i == 0 else " "
                print(f"  {marker} [{ident.source:24s}] conf={ident.confidence:.2f}  {ident.demangled}")
                if ident.mangled != ident.demangled:
                    print(f"      mangled: {ident.mangled}")
                if ident.extra:
                    for k, v in ident.extra.items():
                        if v not in (None, False, [], {}):
                            print(f"      {k}: {v}")


def _get_db_fn_set() -> dict[int, tuple[str, str]]:
    """Return {addr: (unit, match_pct)} for ALL fn_ functions in the decomp.db.

    The DB has ~69k functions (including ones not yet in any split); the
    report.json only covers ~57k (those in pinned TUs). We union both so
    the index covers the full binary.
    """
    if "db_fn_set" in _cache:
        return _cache["db_fn_set"]
    db = _get_db()
    out: dict[int, tuple] = {}
    if db:
        for row in db.execute(
            "SELECT symbol, unit, current_percent FROM functions WHERE symbol LIKE 'fn_%'"
        ):
            sym = row["symbol"]
            try:
                addr = int(sym[3:], 16)
                out[addr] = (row["unit"] or "", row["current_percent"])
            except ValueError:
                pass
    _cache["db_fn_set"] = out
    return out


def cmd_index(args):
    out_path = Path(args.out) if args.out else DEFAULT_INDEX
    out_path.parent.mkdir(parents=True, exist_ok=True)

    print("Loading report.json function map…")
    fn_map = _load_report_fn_map()
    print(f"  {len(fn_map)} functions from report.json")

    print("Loading decomp.db anonymous function set…")
    db_fn_set = _get_db_fn_set()
    print(f"  {len(db_fn_set)} fn_ functions in decomp.db")

    # Union of addresses: report.json for rich metadata, DB for the rest
    all_addrs: dict[int, dict] = {}
    for addr, info in fn_map.items():
        all_addrs[addr] = info
    for addr, (unit, pct) in db_fn_set.items():
        if addr not in all_addrs:
            sym = fn_name(addr)
            all_addrs[addr] = {"name": sym, "unit": unit, "match_pct": pct, "size": 0}

    total_fns = len(all_addrs)
    anon_total = sum(1 for info in all_addrs.values() if info["name"].startswith("fn_"))
    print(f"  Combined total: {total_fns} (anonymous: {anon_total})")

    print("Aggregating identity evidence…")
    index: dict[str, dict] = {}
    tier_counts: dict[str, int] = defaultdict(int)
    strong_id = 0    # conf >= 0.70, non-unit source
    named_id = 0     # T1a (named symbol in report)
    unit_only = 0    # only decomp_db_unit evidence

    # gameid_crossval stats
    xval_new = 0         # addresses where gameid_crossval is the best (only) evidence
    xval_agree = 0       # addresses already covered by a higher tier
    xval_conflicts: list[tuple[int, str, str]] = []  # (addr, xval_stem, best_name)

    xval_idx = _get_gameid_xval_idx()

    for addr, fn_info in all_addrs.items():
        ids = resolve_all(addr)
        sym = fn_info["name"]
        is_anon = sym.startswith("fn_")

        if not ids:
            continue

        best = ids[0]
        tier_counts[best.source] += 1

        if best.source == "decomp_db_named":
            named_id += 1
        elif best.source == "decomp_db_unit":
            unit_only += 1
        elif best.confidence >= 0.70:
            strong_id += 1

        # gameid_crossval accounting
        if addr in xval_idx:
            xval_stem = xval_idx[addr].get("stem", "")
            xval_ids = [i for i in ids if i.source == "gameid_crossval"]
            other_ids = [i for i in ids if i.source != "gameid_crossval"]
            if best.source == "gameid_crossval":
                xval_new += 1
            else:
                # Already covered by a higher-confidence tier
                xval_agree += 1
                # Check for conflict: if the best non-crossval tier gives a
                # mangled name (not fn_), check whether its stem is inconsistent
                # with the crossval stem.  We do a loose check: if the best
                # identity's name does not contain the stem as a substring
                # (case-insensitive) and the best identity is a real named
                # symbol (not a unit-only or autoid placeholder), flag it.
                if other_ids:
                    top = other_ids[0]
                    is_named = (
                        top.source not in ("decomp_db_unit", "autoid_fingerprint")
                        and not top.mangled.startswith("fn_")
                        and top.confidence >= 0.90
                    )
                    if is_named:
                        # demangled often contains class::method; check stem in it
                        if xval_stem.lower() not in top.demangled.lower():
                            xval_conflicts.append((addr, xval_stem, top.demangled))

        key = f"0x{addr:08X}"
        index[key] = {
            "addr": key,
            "fn_name": sym,
            "unit": fn_info["unit"],
            "is_anon": is_anon,
            "best": best.to_dict(),
            "all": [i.to_dict() for i in ids],
        }

    covered_anon = sum(
        1 for key, entry in index.items() if entry["is_anon"]
    )

    print(f"\nCoverage summary:")
    print(f"  Total (report + DB)        : {total_fns}")
    print(f"  Anonymous (fn_)            : {anon_total}")
    print(f"  fn_ with any evidence      : {covered_anon}  ({100*covered_anon/max(anon_total,1):.1f}%)")
    print(f"  Named identity (T1)        : {named_id}")
    print(f"  Strong id (conf≥0.70, !T1) : {strong_id}")
    print(f"  Unit-only (T1b decomp_db)  : {unit_only}")
    print(f"\nBreakdown by best-tier source:")
    for tier in TIER_ORDER:
        n = tier_counts.get(tier, 0)
        if n > 0:
            print(f"  {tier:30s}: {n:5d}")

    total_xval = len(xval_idx)
    xval_not_indexed = total_xval - xval_new - xval_agree
    print(f"\ngameid_crossval tier stats ({total_xval} hint entries):")
    print(f"  New identities (best=gameid_crossval)  : {xval_new}")
    print(f"  Agreements (covered by higher tier)    : {xval_agree}")
    print(f"  Not in indexed universe (unpinned TUs) : {xval_not_indexed}")
    print(f"  Conflicts with named identity (≥0.90)  : {len(xval_conflicts)}")
    if xval_conflicts:
        print(f"\n  Conflict list (addr | xval_stem | named_identity):")
        for caddr, stem, named in sorted(xval_conflicts):
            print(f"    fn_{caddr:08X}  stem={stem!r:30s}  named={named!r}")

    with open(out_path, "w") as f:
        json.dump(index, f, indent=1)
    print(f"\nIndex written → {out_path}")
    print(f"  Entries: {len(index)}")


def cmd_unresolved(args):
    unit_arg = args.unit
    if not unit_arg.startswith("default/"):
        unit_arg_full = "default/" + unit_arg.replace(".cpp", "")
    else:
        unit_arg_full = unit_arg

    if not REPORT_PATH.exists():
        print("ERROR: report.json not found", file=sys.stderr)
        sys.exit(1)

    with open(REPORT_PATH) as f:
        rpt = json.load(f)

    splits = _parse_splits()

    # Find target unit
    target_unit = None
    for unit in rpt["units"]:
        if (unit["name"] == unit_arg_full
                or unit["name"].lower() == unit_arg_full.lower()
                or unit["name"].endswith("/" + unit_arg.replace(".cpp", ""))):
            target_unit = unit
            break

    if target_unit is None:
        print(f"ERROR: unit {unit_arg!r} not found in report.json", file=sys.stderr)
        print("  Available (sample):", [u["name"] for u in rpt["units"][:10]])
        sys.exit(1)

    unit_name = target_unit["name"]
    short = unit_name.replace("default/", "")
    cpp = short + ".cpp"
    text_start = splits.get(cpp, 0)

    # Collect near-miss (non-100%) functions from this unit
    near_miss_fns = []
    for fn in target_unit.get("functions", []):
        pct = fn.get("fuzzy_match_percent", fn.get("match_percent_normalized", 0.0))
        if pct is not None and 0 < pct < 100:
            fn_sym = fn["name"]
            offset = int(fn.get("address", 0))
            if fn_sym.startswith("fn_"):
                addr = int(fn_sym[3:], 16)
            elif text_start:
                addr = text_start + offset
            else:
                continue
            near_miss_fns.append((fn_sym, addr, pct))

    if not near_miss_fns:
        print(f"No near-miss functions found in unit {unit_name!r}")
        return

    # For each near-miss function, scan its ASM for bl targets
    # that are fn_ addresses (anonymous callees)
    asm_dir = ROOT / "build" / "45410914" / "asm"
    bl_re = re.compile(r'\bbl\s+(fn_[0-9A-Fa-f]{8})\b')

    all_callees: dict[int, set] = defaultdict(set)  # callee_addr -> callers

    for fn_sym, fn_addr, pct in near_miss_fns:
        # Find asm file
        asm_file = asm_dir / f"{fn_sym}.s"
        if not asm_file.exists():
            # Try searching by address in asm files
            # The asm dir contains per-unit files, not per-function
            # Let's search the unit .s file
            unit_asm = asm_dir / f"{short}.s"
            if unit_asm.exists():
                try:
                    content = unit_asm.read_text(errors="replace")
                    for m in bl_re.finditer(content):
                        callee_name = m.group(1)
                        callee_addr = int(callee_name[3:], 16)
                        all_callees[callee_addr].add(fn_sym)
                except Exception:
                    pass
            continue
        try:
            content = asm_file.read_text(errors="replace")
            for m in bl_re.finditer(content):
                callee_name = m.group(1)
                callee_addr = int(callee_name[3:], 16)
                all_callees[callee_addr].add(fn_sym)
        except Exception:
            pass

    # Filter to fn_ callees that lack strong identity
    print(f"Unit: {unit_name}")
    print(f"Near-miss functions: {len(near_miss_fns)}")
    print(f"Unique fn_ callees found in ASM: {len(all_callees)}")
    print()

    unresolved = []
    resolved = []
    for callee_addr, callers in sorted(all_callees.items()):
        best = resolve_best(callee_addr)
        if best is None or best.confidence < 0.5 or best.source in (
                "decomp_db_unit", "autoid_fingerprint"):
            unresolved.append((callee_addr, callers, best))
        else:
            resolved.append((callee_addr, callers, best))

    print(f"Resolved bl targets ({len(resolved)}):")
    for addr, callers, best in sorted(resolved, key=lambda x: -x[2].confidence):
        print(f"  fn_{addr:08X}  [{best.source:20s}] conf={best.confidence:.2f}  {best.demangled}")

    print(f"\nUnresolved bl targets ({len(unresolved)}) — worklist:")
    if not unresolved:
        print("  (none — all callees are identified!)")
    for addr, callers, best in sorted(unresolved, key=lambda x: -len(x[1])):
        caller_str = ", ".join(sorted(callers)[:3])
        if len(callers) > 3:
            caller_str += f" +{len(callers)-3} more"
        note = ""
        if best:
            note = f"  [{best.source}] conf={best.confidence:.2f}  {best.demangled}"
        print(f"  fn_{addr:08X}  called by: {caller_str}{note}")


def cmd_validate(args):
    n = args.n
    if not REPORT_PATH.exists():
        print("ERROR: report.json not found", file=sys.stderr)
        sys.exit(1)
    with open(REPORT_PATH) as f:
        rpt = json.load(f)
    splits = _parse_splits()

    # Collect functions that are: (a) named (not fn_), (b) 100% matched,
    # and for which we can compute an absolute address
    candidates = []
    for unit in rpt["units"]:
        short = unit["name"].replace("default/", "")
        cpp = short + ".cpp"
        text_start = splits.get(cpp, 0)
        for fn in unit.get("functions", []):
            if fn["name"].startswith("fn_"):
                continue
            pct = fn.get("fuzzy_match_percent", fn.get("match_percent_normalized", 0.0))
            if pct is None or pct < 100:
                continue
            offset = int(fn.get("address", 0))
            if text_start:
                addr = text_start + offset
                candidates.append((addr, fn["name"], unit["name"]))
    if not candidates:
        print("No 100%-matched named functions found in report.json")
        return

    import random
    random.seed(42)
    sample = random.sample(candidates, min(n, len(candidates)))

    print(f"Validating {len(sample)} known-matched functions:\n")
    passes = 0
    fails = 0
    for addr, expected_mangled, unit_name in sample:
        ids = resolve_all(addr)
        found = any(i.mangled == expected_mangled for i in ids)
        mark = "PASS" if found else "FAIL"
        if found:
            passes += 1
        else:
            fails += 1
        best_str = ids[0].demangled if ids else "<nothing>"
        status = f"{mark}  fn_{addr:08X}  expected={expected_mangled[:50]}"
        if found:
            tier = next(i.source for i in ids if i.mangled == expected_mangled)
            status += f"  found via [{tier}]"
        else:
            status += f"  best={best_str}"
        print("  " + status)

    print(f"\nResult: {passes}/{len(sample)} pass  {fails} fail")


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="fn_resolver — resolve anonymous fn_8XXXXXXX to identities",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    sub = parser.add_subparsers(dest="cmd", required=True)

    p_resolve = sub.add_parser("resolve", help="Resolve one or more addresses")
    p_resolve.add_argument("addrs", nargs="+",
                           help="Addresses: fn_82XXXXXX, 0x82XXXXXX, or 82XXXXXX")

    p_index = sub.add_parser("index", help="Build/refresh identity index for all functions")
    p_index.add_argument("--out", default=None,
                         help=f"Output JSON path (default: {DEFAULT_INDEX})")

    p_unresolved = sub.add_parser(
        "unresolved", help="List unresolved fn_ callees for a unit's near-miss functions")
    p_unresolved.add_argument("--unit", required=True,
                              help="Unit name e.g. 'VocalPlayer' or 'default/VocalPlayer'")

    p_validate = sub.add_parser("validate", help="Validate resolver against known-matched functions")
    p_validate.add_argument("--n", type=int, default=10,
                            help="Number of functions to sample (default: 10)")

    args = parser.parse_args()
    if args.cmd == "resolve":
        cmd_resolve(args)
    elif args.cmd == "index":
        cmd_index(args)
    elif args.cmd == "unresolved":
        cmd_unresolved(args)
    elif args.cmd == "validate":
        cmd_validate(args)


if __name__ == "__main__":
    main()
