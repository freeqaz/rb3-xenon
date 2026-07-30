#!/usr/bin/env python3
"""Refuse to load an address index whose addresses no longer exist in the binary.

WHY THIS EXISTS
---------------
Several cross-binary identification indices in this tree (``unified_id*.json``,
``dc3_oracle.json``, ``global_fuzzy_pairs.json``, ``tools/scope_data/uid_merge.json``)
were generated in the **TU0 era**.  Main flipped TU0 -> TU5 on 2026-07-15 and
``.text`` was re-laid-out; every TU0-era VA is now meaningless.

They are not merely *stale*, they are **informationless**.  Measured 2026-07-30
(lane BX-4, reproducing lane BX-3) against the 69,209 ``.text`` function starts in
``config/45410914/symbols.txt``, with a random-offset null control -- necessary
because retail function starts are 4-byte aligned and densely clustered, so *any*
plausible address list scores ~2-3% by chance:

    index                          n      true%   null%   best rigid shift
    unified_id_rb3wii.json       9,301     4.27    2.83   +0x12448 ->  6.53%
    dc3_oracle.json             33,987     4.25    2.59   +0x12448 ->  5.34%
    unified_id.json             11,582     3.66    2.40   +0x129e0 ->  4.79%
    uid_merge.json              10,671     3.81      --   +0x129e0 ->  4.70%
    unified_id_callgraph.json    1,555     5.53    2.83    +0xe828 ->  7.27%
    unified_id_rtti_low.json       730     4.93    3.87   +0x1df90 ->  8.90%
    unified_id_rtti.json           384     3.91    4.46    -0x5070 -> 10.16%   (below null)
    global_fuzzy_pairs.json      2,000     2.15    3.41   +0x12818 ->  7.55%   (below null)
    unified_id_vtable.json          37     2.70    2.03    +0xadf0 -> 27.03%
    -- live positive controls --
    scripts/target_symbol_map.json 27,549 99.79    3.67
    autoid.json                   1,619  100.00    2.88

NO REBASE RESCUES THEM.  The "best rigid shift" column is an *exhaustive* search
over every 4-byte-aligned offset in +/-0x20000; the best achievable score is still
single digits.  The best shifts cluster near +0x10000, which is exactly the delta
between TU0's ``.text`` start (0x82260000) and TU5's (0x82270000) -- i.e. the
*start* of ``.text`` moved, but the interior was re-laid-out rather than
translated, so a rigid rebase recovers essentially nothing.

DESIGN NOTE -- WHY THIS MEASURES INSTEAD OF REMEMBERING
-------------------------------------------------------
A hardcoded blacklist would be *the same bug this module exists to prevent*: a
tool acting on a baked-in fact that silently goes out of date.  So the guard
recomputes liveness from ``config/45410914/symbols.txt`` (the authoritative
universe) every run, with a small mtime/size-keyed cache.  Consequences:

  * If a future lane REGENERATES one of these indices against the current
    binary, every guarded consumer starts working again automatically -- no
    edit to this file, no blacklist to update.
  * If a currently-live index (``target_symbol_map.json``) ever dies, guarded
    consumers start failing loudly instead of silently emitting garbage.

ESCAPE HATCH
------------
``RB3_ALLOW_DEAD_INDEX=1`` downgrades the hard failure to a loud stderr banner.
It exists so the indices remain inspectable for archaeology/forensics, and so
nobody is tempted to delete a guard call to get unstuck.  Never set it in a
workflow or a landing gate.

CLI
---
    python3 tools/dead_index_guard.py --audit          # audit the known indices
    python3 tools/dead_index_guard.py FILE [FILE...]   # audit specific files

Exit 0 iff every audited file is LIVE.
"""

from __future__ import annotations

import json
import os
import re
import sys

# A live index scores ~100%; every dead one measures under 6%. 90 is a wide moat.
LIVE_THRESHOLD_PCT = 90.0

# Fields that hold an RB3 retail VA across the various index schemas.
ADDR_KEYS = frozenset({
    "rb3_addr", "rb3_va", "rb3_address", "va", "addr", "address", "target_va",
})

ENV_BYPASS = "RB3_ALLOW_DEAD_INDEX"

_REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_SYMBOLS = os.path.join("config", "45410914", "symbols.txt")

_universe_cache: dict[str, frozenset] = {}
_measure_cache: dict[tuple, tuple] = {}

_TEXT_FN = re.compile(
    r"^\s*\S+\s*=\s*\.text:0x([0-9A-Fa-f]+)\s*;.*\btype:function\b")


class DeadIndexError(RuntimeError):
    """Raised when a tool tries to act on an address index that is dead."""


def repo_root(start: str | None = None) -> str:
    """Repo root for this module (works from a worktree)."""
    return _REPO_ROOT if start is None else start


def text_function_universe(root: str | None = None) -> frozenset:
    """The authoritative address universe: every ``.text`` function start.

    Parsed from ``config/45410914/symbols.txt``.  This is the *only* sound
    universe -- note in particular that ``report.json``'s ``address`` field is a
    per-unit section-relative OFFSET, not a VA, and joining against it produces
    a meaningless near-zero resolve rate.
    """
    root = repo_root(root)
    path = os.path.join(root, _SYMBOLS)
    key = f"{path}:{os.path.getmtime(path):.0f}"
    hit = _universe_cache.get(key)
    if hit is not None:
        return hit
    uni = set()
    with open(path, errors="replace") as fh:
        for line in fh:
            m = _TEXT_FN.match(line)
            if m:
                uni.add(int(m.group(1), 16))
    if not uni:
        raise DeadIndexError(
            f"dead_index_guard: parsed ZERO .text function starts from {path} -- "
            "the guard cannot verify anything. Refusing to vouch for any index.")
    frozen = frozenset(uni)
    _universe_cache[key] = frozen
    return frozen


def extract_addresses(obj, keys=ADDR_KEYS) -> list:
    """Pull retail VAs out of an arbitrary index document.

    Handles the three shapes in this tree: a list of records with an address
    field, a dict keyed by ``"0x8XXXXXXX"``, and a dict keyed by bare
    ``"8XXXXXXX"`` (``uid_merge.json``).
    """
    out: list[int] = []

    def as_addr(v):
        if isinstance(v, int):
            return v
        if isinstance(v, str):
            s = v.strip()
            try:
                if s.lower().startswith("0x"):
                    return int(s, 16)
                if re.fullmatch(r"8[0-9A-Fa-f]{7}", s):
                    return int(s, 16)
                m = re.fullmatch(r"fn_([0-9A-Fa-f]{8})", s)
                if m:
                    return int(m.group(1), 16)
            except ValueError:
                return None
        return None

    def walk(o):
        if isinstance(o, dict):
            for k, v in o.items():
                ka = as_addr(k)
                if ka is not None:
                    out.append(ka)
                    continue
                if k in keys:
                    va = as_addr(v)
                    if va is not None:
                        out.append(va)
                        continue
                walk(v)
        elif isinstance(o, list):
            for v in o:
                walk(v)

    walk(obj)
    return out


def measure(path: str, *, keys=ADDR_KEYS, root: str | None = None, doc=None):
    """Return ``(n_addresses, live_pct)`` for an index file.

    ``live_pct`` is the share of its addresses that are real ``.text`` function
    starts in the current binary.
    """
    st = os.stat(path)
    ck = (os.path.abspath(path), st.st_mtime_ns, st.st_size, tuple(sorted(keys)))
    hit = _measure_cache.get(ck)
    if hit is not None:
        return hit
    if doc is None:
        with open(path, errors="replace") as fh:
            doc = json.load(fh)
    addrs = extract_addresses(doc, keys)
    if not addrs:
        res = (0, float("nan"))
    else:
        uni = text_function_universe(root)
        live = sum(1 for a in addrs if a in uni)
        res = (len(addrs), live / len(addrs) * 100.0)
    _measure_cache[ck] = res
    return res


def _bypassed() -> bool:
    return os.environ.get(ENV_BYPASS, "") not in ("", "0", "false", "False")


def assert_live(path: str, *, what: str | None = None, keys=ADDR_KEYS,
                root: str | None = None, doc=None) -> float:
    """Hard-fail unless ``path`` is a live address index.

    Call this at the load site, *before* the tool acts on the contents.  Returns
    the measured live percentage on success.
    """
    label = what or os.path.basename(path)
    if not os.path.exists(path):
        raise DeadIndexError(
            f"dead_index_guard: {label}: {path} does not exist.\n"
            "  This index is gitignored and regenerable; nothing in the tree\n"
            "  will silently substitute for it. Regenerate it against the\n"
            "  CURRENT binary or use a live source\n"
            "  (scripts/target_symbol_map.json, autoid.json).")

    n, pct = measure(path, keys=keys, root=root, doc=doc)

    if n == 0:
        msg = (f"dead_index_guard: {label}: could not extract any retail VA from\n"
               f"  {path}\n"
               "  The guard cannot verify this index, so it will not vouch for it.\n"
               f"  If the addresses live under a non-standard key, pass keys=... .")
    elif pct < LIVE_THRESHOLD_PCT:
        msg = (
            f"dead_index_guard: {label} is a DEAD address index -- REFUSING to use it.\n"
            f"  file      : {path}\n"
            f"  measured  : {pct:.2f}% of its {n:,} addresses are real .text function\n"
            f"              starts in config/45410914/symbols.txt (need >= "
            f"{LIVE_THRESHOLD_PCT:.0f}%).\n"
            f"  baseline  : an ARBITRARY address list scores ~2-3% here by chance,\n"
            f"              because retail function starts are aligned and clustered.\n"
            f"              So this index carries essentially NO information.\n"
            "  cause     : generated in the TU0 era; main flipped TU0 -> TU5 on\n"
            "              2026-07-15 and .text was re-laid-out. An exhaustive search\n"
            "              over every 4-byte shift in +/-0x20000 cannot lift any of\n"
            "              these indices above single digits -- NO REBASE RESCUES IT.\n"
            "  do instead: use a LIVE source -- scripts/target_symbol_map.json (99.79%)\n"
            "              or autoid.json (100%, regenerate via\n"
            "              `python3 tools/fingerprint_match.py autoid`).\n"
            "              Deriving a splits.txt pin span, a landing gate verdict, or a\n"
            "              target-symbol map from this file produces a PLAUSIBLE-LOOKING\n"
            "              WRONG artifact, which is why this is fatal rather than a warning.\n"
            f"  override  : {ENV_BYPASS}=1 (forensics only -- never in a workflow or gate).")
    else:
        return pct

    if _bypassed():
        sys.stderr.write("\n" + "!" * 78 + "\n" + msg +
                         f"\n  ** {ENV_BYPASS} SET -- PROCEEDING ANYWAY. "
                         "ANY OUTPUT BELOW IS UNTRUSTWORTHY. **\n" +
                         "!" * 78 + "\n\n")
        sys.stderr.flush()
        return pct
    raise DeadIndexError(msg)


def load_guarded(path: str, *, what: str | None = None, keys=ADDR_KEYS,
                 root: str | None = None):
    """``json.load`` the index, but only if it is live. Otherwise hard-fail.

    Use this when the index is the tool's SOLE source -- i.e. the entire output
    is derived from it, so a dead index means a wholly fabricated artifact.
    """
    with open(path, errors="replace") as fh:
        doc = json.load(fh)
    assert_live(path, what=what, keys=keys, root=root, doc=doc)
    return doc


def skip_if_dead(path: str, *, what: str | None = None, keys=ADDR_KEYS,
                 root: str | None = None) -> bool:
    """Return True if the caller should SKIP this index; warn loudly if so.

    Use this for MULTI-SOURCE tools that also consult live indices (e.g.
    ``fn_resolver``'s tier stack, ``scope_map``'s provenance layers). Dropping
    one dead tier degrades coverage honestly; hard-failing would destroy a
    tool that still works off its live tiers.

    Never use this where the index is the sole source -- silently dropping the
    only input reproduces the SILENT-ZERO bug. Use ``load_guarded`` there.
    """
    label = what or os.path.basename(path)
    if not os.path.exists(path):
        return True
    try:
        n, pct = measure(path, keys=keys, root=root)
    except (json.JSONDecodeError, OSError, DeadIndexError) as e:
        sys.stderr.write(f"[dead_index_guard] SKIP {label}: unreadable ({e.__class__.__name__})\n")
        return True
    if n and pct >= LIVE_THRESHOLD_PCT:
        return False
    if _bypassed():
        sys.stderr.write(
            f"[dead_index_guard] {label} is DEAD ({pct:.2f}% live) but "
            f"{ENV_BYPASS} is set -- USING IT ANYWAY, output untrustworthy.\n")
        return False
    sys.stderr.write(
        f"[dead_index_guard] SKIPPING DEAD INDEX: {label}\n"
        f"    {path}\n"
        f"    only {pct:.2f}% of its {n:,} addresses are real .text function starts "
        f"(chance ~2-3%).\n"
        f"    TU0-era; the 2026-07-15 TU0->TU5 flip re-laid-out .text. No rebase fixes it.\n"
        f"    -> this source contributes NOTHING to the result below; coverage is reduced.\n"
        f"    -> audit: python3 tools/dead_index_guard.py --audit\n")
    sys.stderr.flush()
    return True


# --------------------------------------------------------------------------- CLI

KNOWN = [
    "unified_id_rb3wii.json",
    "dc3_oracle.json",
    "unified_id.json",
    "tools/scope_data/uid_merge.json",
    "unified_id_callgraph.json",
    "unified_id_rtti_low.json",
    "unified_id_rtti.json",
    "global_fuzzy_pairs.json",
    "unified_id_vtable.json",
    "scripts/target_symbol_map.json",   # live positive control
    "autoid.json",                      # live positive control
]


def main(argv):
    args = [a for a in argv[1:] if a != "--audit"]
    root = _REPO_ROOT
    files = args or [os.path.join(root, f) for f in KNOWN]
    uni = text_function_universe(root)
    print(f"universe: {len(uni):,} .text function starts from {_SYMBOLS} "
          f"[{min(uni):#x}..{max(uni):#x}]")
    print(f"{'index':44} {'n':>8} {'live%':>8}  verdict")
    bad = 0
    for f in files:
        p = f if os.path.isabs(f) else os.path.join(root, f)
        rel = os.path.relpath(p, root)
        if not os.path.exists(p):
            print(f"{rel:44} {'-':>8} {'-':>8}  MISSING")
            bad += 1
            continue
        try:
            n, pct = measure(p, root=root)
        except (json.JSONDecodeError, OSError) as e:
            print(f"{rel:44} {'-':>8} {'-':>8}  UNREADABLE ({e.__class__.__name__})")
            bad += 1
            continue
        if n == 0:
            print(f"{rel:44} {0:>8} {'n/a':>8}  NO ADDRESSES")
            bad += 1
        elif pct < LIVE_THRESHOLD_PCT:
            print(f"{rel:44} {n:8,} {pct:7.2f}%  DEAD")
            bad += 1
        else:
            print(f"{rel:44} {n:8,} {pct:7.2f}%  live")
    print(f"\n{bad} of {len(files)} audited file(s) are unusable as address indices.")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
