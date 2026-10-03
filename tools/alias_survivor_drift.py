#!/usr/bin/env python3
"""Fail when an alias group's `survivor` is not the map's name at its address.

WHY THIS EXISTS (lane W16-OS, 2026-10-03)
------------------------------------------
`tools/gen_symbol_alias_map.py` renders every placed group of
`scripts/symbol_aliases.json` as ONE objdiff equivalence bucket keyed by the
group's `address`: `[survivor, *folded]`.  objdiff forgives a relocation-name
difference only when the RETAIL name and OUR name share a bucket, and the
retail target objs spell each address with the name
`scripts/target_symbol_map.json` gives it (the renamer applies it).

So a survivor is load-bearing, and it is only right when it IS that name:

  * survivor unmapped (the map named the address differently since) -- the
    bucket holds no name retail ever uses at the address, so every folded
    member forgives nothing, silently;
  * survivor mapped at ANOTHER address -- retail calls to that other address
    are spelled with it, so the bucket forgives our calls to the folded members
    against the WRONG function;
  * and every tool that adjudicates a group by its `survivor` field
    (`tools/icf_pair_adjudicate.py --chase`, the re-chase censuses) reads the
    retail body named by the stale label -- at the wrong address, or MISSING.
    W16-OM's census counted 321 `MISSING(retail)` memberships as REFUTED that
    way, and W16-NK admitted a fold at 0x827d5bb0 whose proof was of the body
    at 0x824f18c8 (the stale label's map address).

Lane W16-OA measured 228 of 2,062 placed groups drifted at d792b486f; W16-OS
measured 186 at 4466f2a76 (excluding placeholder survivors naming their own
address) and repaired them with `tools/alias_survivor_relabel.py`.  Nothing
pinned the invariant, so it drifted every time a lane renamed a map row.

THE RULE
--------
For every group with an `address` (address-less partition classes render into
no bucket and are exempt):

  * the applied map names the address N  ->  survivor must equal N;
  * the applied map names nothing there   ->  survivor must be a dtk
    placeholder (`fn_`/`lbl_`/`vftable_`) for THAT address, which is what the
    target objs call it.

"Applied" means `obj_target_symbol_renamer.load_address_map` -- null rows and
`_denylist` rows name nothing -- so this check and the renamer cannot disagree
about which name the target objs carry.  Same loader `map_name_injectivity.py`
uses, for the same reason.

The fix for a failure is `python3 tools/alias_survivor_relabel.py --write`,
which relabels AND re-chases every membership against the corrected survivor
(relabelling alone would switch on forgiveness nobody proved at that address).

    python3 tools/alias_survivor_drift.py            # report; rc 1 on drift
    python3 tools/alias_survivor_drift.py --selftest # frozen fixtures
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))
sys.path.insert(0, str(ROOT / "tools"))
from obj_target_symbol_renamer import load_address_map  # noqa: E402
import stamp_if_changed  # noqa: E402

ALIASES = ROOT / "scripts" / "symbol_aliases.json"
MAP = ROOT / "scripts" / "target_symbol_map.json"
PLACEHOLDER_PREFIXES = ("fn_", "lbl_", "vftable_")


def applied_names(map_path=MAP) -> dict:
    """va -> the name the renamer installs there (null/_denylist rows absent)."""
    out = {}
    for k, v in load_address_map(Path(map_path)).items():
        out[int(k.split("_", 1)[1], 16)] = v
    return out


def placeholder_va(name):
    if not isinstance(name, str) or not name.startswith(PLACEHOLDER_PREFIXES):
        return None
    try:
        return int(name.split("_", 1)[1], 16)
    except ValueError:
        return None


def expected(va, applied):
    """The survivor a group at `va` must carry: the applied name, else None
    (meaning: a placeholder for `va`)."""
    return applied.get(va)


def find_drift(groups, applied):
    """-> list of (group, expected_or_None, reason).  Empty == no drift."""
    out = []
    for g in groups:
        a = g.get("address")
        if not a:
            continue
        va = int(a, 16)
        s = g.get("survivor")
        want = expected(va, applied)
        if want is not None:
            if s != want:
                out.append((g, want, "map names %s %s" % (a, want)))
        elif placeholder_va(s) != va:
            out.append((g, None, "map names nothing at %s; survivor must be "
                                  "its placeholder" % a))
    return out


# ---------------------------------------------------------------------------
# --selftest: frozen fixtures.  The HEALTHY leg must be green or every red
# below proves nothing (a check that fails on everything is not a check).
# ---------------------------------------------------------------------------
def selftest() -> int:
    ok = True

    def check(label, cond):
        nonlocal ok
        print("  [%s] %s" % ("PASS" if cond else "FAIL", label))
        ok &= bool(cond)

    applied = {0x82000010: "?a@@YAXXZ", 0x82000020: "?b@@YAXXZ"}
    healthy = [
        {"address": "0x82000010", "survivor": "?a@@YAXXZ", "folded": ["?x@@YAXXZ"]},
        {"address": "0x82000030", "survivor": "fn_82000030", "folded": []},
        {"address": "0x82000034", "survivor": "lbl_82000034", "folded": []},
        {"address": None, "survivor": "?anything@@YAXXZ", "folded": []},
    ]
    check("healthy population reports no drift", find_drift(healthy, applied) == [])
    for label, g in (
        ("survivor unmapped where the map names the address",
         {"address": "0x82000010", "survivor": "?stale@@YAXXZ"}),
        ("survivor is the map name of ANOTHER address",
         {"address": "0x82000010", "survivor": "?b@@YAXXZ"}),
        ("placeholder survivor where the map has since named the address",
         {"address": "0x82000020", "survivor": "fn_82000020"}),
        ("real name at an address the map leaves unnamed",
         {"address": "0x82000030", "survivor": "?a@@YAXXZ"}),
        ("placeholder naming a DIFFERENT address",
         {"address": "0x82000030", "survivor": "fn_82000040"}),
    ):
        check("drift fires: " + label, len(find_drift([g], applied)) == 1)
    # the real loader honours null rows and _denylist (a null row names nothing)
    import tempfile
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump({"0x82000010": "?a@@YAXXZ", "0x82000020": None,
                   "0x82000030": "?c@@YAXXZ", "_denylist": ["0x82000030"]}, fh)
    ap = applied_names(fh.name)
    check("applied_names: a mapped row is applied", ap.get(0x82000010) == "?a@@YAXXZ")
    check("applied_names: a null row names nothing", 0x82000020 not in ap)
    check("applied_names: a _denylist row names nothing", 0x82000030 not in ap)
    Path(fh.name).unlink()
    print("SELFTEST:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--aliases", default=str(ALIASES))
    ap.add_argument("--map", default=str(MAP))
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--quiet", action="store_true", help="print nothing when green")
    stamp_if_changed.add_arguments(ap)
    a = ap.parse_args(argv)
    if a.selftest:
        return selftest()
    groups = json.loads(Path(a.aliases).read_text()).get("groups", [])
    placed = sum(1 for g in groups if g.get("address"))
    if not placed:
        # an empty population passes every check by construction
        print("REFUSING: %s declares no placed alias group" % a.aliases, file=sys.stderr)
        return 2
    drift = find_drift(groups, applied_names(a.map))
    if not drift:
        if not a.quiet:
            print("[alias-survivor] OK: %d placed groups, every survivor is the "
                  "applied map name at its address (or its placeholder)" % placed)
        stamp_if_changed.apply(a)
        return 0
    print("", file=sys.stderr)
    print("ALIAS SURVIVOR DRIFT: %d of %d placed groups in %s carry a survivor "
          "that is not the map's name at their address:" % (len(drift), placed, a.aliases),
          file=sys.stderr)
    for g, want, why in drift[:40]:
        print("  %s  survivor %s  -- %s" % (g["address"], g["survivor"][:90], why[:160]),
              file=sys.stderr)
    if len(drift) > 40:
        print("  ... and %d more" % (len(drift) - 40), file=sys.stderr)
    print("The rendered bucket at each such address omits the name retail uses "
          "there, so its folded\nmembers forgive the wrong thing (or nothing). "
          "Fix: python3 tools/alias_survivor_relabel.py --write\n(relabels AND "
          "re-chases every membership against the corrected survivor).",
          file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
