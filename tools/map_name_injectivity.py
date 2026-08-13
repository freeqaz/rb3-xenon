#!/usr/bin/env python3
"""Assert `scripts/target_symbol_map.json` is INJECTIVE ON NAME, globally.

THE INVARIANT
-------------
A linked image resolves every external / COMDAT symbol to exactly ONE
definition, so a mangled name cannot sit at two VAs.  The map is
address-keyed, which gives it no structural defence against stamping one name
onto several addresses -- so the defence has to be an assertion, and it has to
run on every build.

WHY A WRONG NAME IS NOT SELF-CORRECTING
---------------------------------------
objdiff pairs by NAME inside a unit and compares bytes with
`functionRelocDiffs` blind to relocation targets.  Adjustor thunks, deleting
dtors and template bodies are byte-identical modulo their relocations, so a
wrong name on a byte-twin VA reads a clean 100% -- and byte-exactness is the
ADMISSION gate for a crack and for a training label, not merely a metric.  A
duplicate name is therefore a live path to minting a byte-exact witness
against the wrong target body.  The near-100 case is the dangerous one: a
false 100 is already committed and can be audited, whereas a duplicate sitting
at 99.5% is an open invitation to close it with one edit, at a coin-flip on
which of the two targets the edit is closing.

THREE DESIGN DECISIONS, EACH PAID FOR
-------------------------------------
1. SET-BASED, NOT COUNT-BASED, and it EMITS THE OFFENDING SET.
   Commit 2eb6307a records the case that settles this: a tempting map plan
   left the collision COUNT unchanged (8 -> 8) while silently retiring one
   duplicate and INTRODUCING another.  A count check passes that plan.  This
   check compares the offending SET and prints every member, so "same number
   of problems" can never read as "no new problem".

2. IT READS THE MAP, NOT THE OBJECTS.
   An object-level audit only sees a name that two COMPILED objs both emit.
   `?DataDir@UIPanel@@$4PPPPPPPM@EM@AAPAVObjectDir@@XZ` was a two-VA map
   collision that no differ could see, because both VAs live in one unit and
   only one of them pairs.  The map is where the claim is made, so the map is
   where the claim is checked.

3. IT CHECKS THE *APPLIED* MAP, VIA THE LOADER ITSELF.
   The set of rows that actually reach a target obj is computed by
   `obj_target_symbol_renamer.load_address_map` -- null rows are "deliberately
   unclaimed", `_denylist` rows are refused.  This gate imports that function
   rather than re-deriving the filter, so the gate and the renamer cannot
   drift.  That matters here specifically: `_denylist` was declared in the map
   and IGNORED by the loader until commit f3fe9ab1, i.e. this repo has already
   shipped one safeguard that silently did nothing, and a gate carrying its
   own copy of the filter would be the second.

   Consequence, stated so nobody has to discover it: a duplicate confined to
   denied / nulled rows is NOT applied and does NOT fail this gate.  It is
   printed as an informational line instead.  Denying is a deliberate,
   rationale-carrying escape hatch (`_denylist_comment` requires a reason per
   address), not a way to hide a live collision -- the collision it hides is
   one that no longer reaches an obj.

THE ONE LEGITIMATE EXCEPTION
----------------------------
Internal linkage.  A `static` free function is not a COMDAT and is not
deduped by the linker, so the same mangled name legitimately sits at one VA
per defining TU.  `?NodeCmp@@YAHPBX0@Z` is a file-static qsort comparator in
both DataArray.cpp and BandWardrobe.cpp; both VAs score 100% against their own
unit's compiled body and the two bodies DIFFER (a single COMDAT would be
byte-identical everywhere).  That is why this gate cannot be a blanket "same
name, two addresses, refuse": the blanket rule is WRONG on a real case we
already hold.  The exception is enumerated by NAME in the map's
`_internal_linkage_allow`, with `_internal_linkage_allow_comment` carrying the
measured signature an entry has to meet.

There is deliberately NO second, softer allow-list of "known open collisions".
An entry there would be a name we have disproved or cannot adjudicate, left
applied and left scoring -- which is the hazard this gate exists to stop,
re-admitted under a comment.  The remedy for a duplicate is to null the
disproved rows (see 2eb6307a, and lane J2's Symbol::Null / PropSync / DataDir
repairs) or to deny the unadjudicated ones; both are metric-free when the
affected rows are sub-100, which the surplus copy usually is.

REPAIR
------
`scripts/harvest/dupname_rebijection.py` is the repair tool: for every surplus
VA it looks for a different, globally unused, byte-class-identical name in
that VA's own unit, so the map becomes injective without losing the 100%.
Where no spare name exists it drops the entry.  Identity questions it cannot
settle go to `scripts/harvest/dupname_identity_resolver.py`, and for adjustor
thunks the strongest available test is
`scripts/harvest/thunk_name_consistency_scan.py` -- a `$4` thunk's name is
fully determined by the name of the body it tail-jumps to, so a disagreement
is a self-contained proof, no oracle required.

WHY THE PER-UNIT CHECKS ARE NOT ENOUGH (this is how every recurrence got in)
---------------------------------------------------------------------------
`scripts/harvest/icf_class_bijection.py` and
`scripts/harvest/tu5_map_apply_fragment.py` both enforce injectivity, but only
WITHIN one unit / one fragment.  Cross-unit duplicates pass every one of them.
The debt has come back twice on exactly that gap (738 surplus VAs before
e7b8ba85, 533 again two days later from new fragments).  A fragment applier
being locally correct is not evidence the map is globally correct, and only a
global check run on every build can say otherwise.

USAGE
-----
    python3 tools/map_name_injectivity.py                 # check, rc 1 on fail
    python3 tools/map_name_injectivity.py --json OUT.json # + machine-readable
    python3 tools/map_name_injectivity.py --selftest      # prove it can FIRE

`--selftest` is not decoration.  A gate that has never been observed to go red
is indistinguishable from a gate that cannot: it re-runs the verdict function
on the NULL VECTOR (empty map -> no findings), on a synthetically
re-introduced duplicate (-> exactly that finding), and on the internal-linkage
exception (-> allowed), all in memory, touching nothing on disk.
"""
import argparse
import bisect
import collections
import contextlib
import io
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAP_PATH = ROOT / "scripts" / "target_symbol_map.json"
SPLITS_PATH = ROOT / "config" / "45410914" / "splits.txt"

sys.path.insert(0, str(ROOT / "scripts"))
from obj_target_symbol_renamer import load_address_map  # noqa: E402


# --------------------------------------------------------------------------
# verdict function -- pure, so --selftest can exercise it on the null vector
# --------------------------------------------------------------------------
def find_violations(name_to_addrs, allowed_names):
    """-> sorted [(name, [addr, ...])] for every name claimed at >1 address
    that is not an enumerated internal-linkage exception.

    Pure and total: no I/O, no globals.  `name_to_addrs` maps name -> iterable
    of ints; `allowed_names` is the internal-linkage allow-list.
    """
    allowed = set(allowed_names)
    out = []
    for name, addrs in name_to_addrs.items():
        uniq = sorted(set(addrs))
        if len(uniq) > 1 and name not in allowed:
            out.append((name, uniq))
    return sorted(out)


def stale_allow_entries(name_to_addrs, allowed_names):
    """Allow-list names that are no longer duplicated in the applied map.

    Reported, never fatal.  A name can legitimately fall to one address (or to
    none) because a unit stopped being split or a row was nulled, and failing
    the build for cruft would train people to edit the gate instead of the
    map.
    """
    return sorted(n for n in allowed_names
                  if len(set(name_to_addrs.get(n, ()))) < 2)


# --------------------------------------------------------------------------
# inputs
# --------------------------------------------------------------------------
def applied_name_to_addrs(map_path):
    """name -> [VA, ...] over the rows the renamer ACTUALLY applies.

    `load_address_map` returns {"fn_XXXXXXXX": name, "lbl_XXXXXXXX": name} for
    every surviving row -- the two spellings are the same address, so read the
    `fn_` half only.

    The loader prints its own null/denied/non-string tally; that line belongs
    to the PATCH step, which already emits it on every build, so it is
    swallowed here rather than doubled.
    """
    with contextlib.redirect_stdout(io.StringIO()):
        renames = load_address_map(map_path)
    out = collections.defaultdict(list)
    for key, name in renames.items():
        if key.startswith("fn_"):
            out[name].append(int(key[3:], 16))
    return dict(out)


def raw_name_to_addrs(map_path):
    """name -> [VA, ...] over every string-valued row, filter or no filter."""
    raw = json.loads(Path(map_path).read_text())
    out = collections.defaultdict(list)
    for key, val in raw.items():
        if key.lower().startswith("0x") and isinstance(val, str) and val:
            out[val].append(int(key, 16))
    return dict(out)


def load_unit_index(splits_path):
    """VA -> owning source unit, for annotating a finding.  Best-effort: a
    missing splits.txt costs annotation, never the verdict."""
    try:
        text = Path(splits_path).read_text()
    except OSError:
        return lambda _va: None
    ranges = []
    cur = None
    for line in text.splitlines():
        if not line.strip() or line.startswith("Sections:"):
            continue
        if not line[0].isspace():
            cur = line.strip().rstrip(":")
            continue
        parts = line.split()
        if len(parts) >= 3 and parts[0] == ".text" and parts[1].startswith("start:"):
            ranges.append((int(parts[1].split(":")[1], 16),
                           int(parts[2].split(":")[1], 16), cur))
    ranges.sort()
    starts = [r[0] for r in ranges]

    def unit_of(va):
        i = bisect.bisect_right(starts, va) - 1
        if i >= 0 and ranges[i][0] <= va < ranges[i][1]:
            return ranges[i][2]
        return None
    return unit_of


# --------------------------------------------------------------------------
def selftest():
    """Prove the verdict function can say both words."""
    ok = True

    def check(label, got, want):
        nonlocal ok
        if got != want:
            ok = False
            print(f"  FAIL {label}\n       got  {got!r}\n       want {want!r}")
        else:
            print(f"  ok   {label}")

    # null vector: an empty map has no findings and no stale entries
    check("null vector -> no violations", find_violations({}, []), [])
    check("null vector -> no stale allows", stale_allow_entries({}, []), [])

    # an injective map is green
    injective = {"?A@@YAXXZ": [0x8200_0000], "?B@@YAXXZ": [0x8200_0010]}
    check("injective map -> no violations",
          find_violations(injective, []), [])

    # a synthetically RE-INTRODUCED duplicate is red, and names itself
    reintroduced = dict(injective)
    reintroduced["?B@@YAXXZ"] = [0x8200_0010, 0x8200_0020]
    check("re-introduced duplicate -> exactly that finding",
          find_violations(reintroduced, []),
          [("?B@@YAXXZ", [0x8200_0010, 0x8200_0020])])

    # ... and stays red when the COUNT is unchanged but the SET moved:
    # one duplicate retired, a different one introduced (commit 2eb6307a).
    swapped = {"?A@@YAXXZ": [0x8200_0000, 0x8200_0030],
               "?B@@YAXXZ": [0x8200_0010]}
    before = find_violations(reintroduced, [])
    after = find_violations(swapped, [])
    check("count-neutral swap is still 1 finding (a count check passes it)",
          len(before) == len(after) == 1, True)
    check("...and the SET differs, which is what this gate compares",
          before != after, True)

    # the internal-linkage exception is honoured, by NAME
    check("internal-linkage allow -> permitted",
          find_violations(reintroduced, ["?B@@YAXXZ"]), [])
    check("allow-list does not permit a DIFFERENT name",
          find_violations(reintroduced, ["?A@@YAXXZ"]),
          [("?B@@YAXXZ", [0x8200_0010, 0x8200_0020])])

    # a stale allow entry is detected but is not a violation
    check("stale allow entry detected",
          stale_allow_entries(injective, ["?A@@YAXXZ"]), ["?A@@YAXXZ"])

    print("SELFTEST", "PASS" if ok else "FAIL")
    return 0 if ok else 1


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--map", default=str(MAP_PATH))
    ap.add_argument("--splits", default=str(SPLITS_PATH))
    ap.add_argument("--json", dest="json_out",
                    help="write the finding set here (machine-readable)")
    ap.add_argument("--selftest", action="store_true",
                    help="exercise the verdict function in memory and exit")
    ap.add_argument("--quiet", action="store_true",
                    help="print nothing when green")
    args = ap.parse_args(argv)

    if args.selftest:
        return selftest()

    map_path = Path(args.map)
    raw = json.loads(map_path.read_text())
    allowed = raw.get("_internal_linkage_allow", [])
    if not isinstance(allowed, list) or any(not isinstance(x, str)
                                            for x in allowed):
        # Same strictness as the loader's `_denylist` parse, for the same
        # reason: a malformed exception list that silently reads as empty (or
        # as "allow everything") is how a safeguard stops meaning anything.
        print(f"ERROR: _internal_linkage_allow in {map_path} must be a list "
              f"of name strings", file=sys.stderr)
        return 2

    applied = applied_name_to_addrs(map_path)
    violations = find_violations(applied, allowed)
    stale = stale_allow_entries(applied, allowed)

    # duplicates that exist in the file but are filtered out before any obj
    # sees them -- visible, never fatal (see design note 3).
    raw_dups = {n: a for n, a in
                find_violations(raw_name_to_addrs(map_path), allowed)}
    applied_dups = {n for n, _ in violations}
    suppressed = sorted((n, a) for n, a in raw_dups.items()
                        if n not in applied_dups)

    unit_of = load_unit_index(args.splits)

    if args.json_out:
        Path(args.json_out).write_text(json.dumps({
            "map": str(map_path),
            "applied_rows": sum(len(v) for v in applied.values()),
            "violations": [
                {"name": n,
                 "addresses": [f"0x{a:08x}" for a in addrs],
                 "units": [unit_of(a) for a in addrs]}
                for n, addrs in violations],
            "suppressed_by_denylist_or_null": [
                {"name": n, "addresses": [f"0x{a:08x}" for a in addrs]}
                for n, addrs in suppressed],
            "internal_linkage_allow": list(allowed),
            "stale_allow_entries": stale,
        }, indent=1) + "\n")

    for name in stale:
        print(f"[map-injectivity] NOTE: _internal_linkage_allow lists "
              f"{name!r}, which is no longer duplicated in the applied map "
              f"-- the entry is stale and can be dropped.")
    for name, addrs in suppressed:
        where = " ".join(f"0x{a:08x}" for a in addrs)
        print(f"[map-injectivity] not applied (denylisted/null), so not a "
              f"finding: {name} at {where}")

    if not violations:
        if not args.quiet:
            print(f"[map-injectivity] OK: {sum(len(v) for v in applied.values())} "
                  f"applied rows, {len(applied)} distinct names, injective "
                  f"(+{len(allowed)} enumerated internal-linkage exception(s))")
        return 0

    total = sum(len(a) for _, a in violations)
    print("", file=sys.stderr)
    print(f"MAP NAME-INJECTIVITY VIOLATED: {len(violations)} name(s) claimed "
          f"at {total} addresses.", file=sys.stderr)
    print("A linked image defines each COMDAT/extern symbol at exactly ONE "
          "address, so at", file=sys.stderr)
    print("most one VA per set below is real -- and objdiff will still score "
          "the wrong one,", file=sys.stderr)
    print("because it pairs by name and is blind to the relocation targets "
          "that separate them.", file=sys.stderr)
    print("", file=sys.stderr)
    for name, addrs in violations:
        print(f"  {name}", file=sys.stderr)
        for a in addrs:
            unit = unit_of(a)
            print(f"      0x{a:08x}  {unit or '<unattributed>'}",
                  file=sys.stderr)
    print("", file=sys.stderr)
    print("FIX, in order of preference:", file=sys.stderr)
    print("  1. Disprove the surplus copies and set them to `null` in "
          "scripts/target_symbol_map.json.", file=sys.stderr)
    print("     For $4 adjustor thunks the name is fully determined by the "
          "callee it tail-jumps", file=sys.stderr)
    print("     to: scripts/harvest/thunk_name_consistency_scan.py. For "
          "byte-twin template bodies", file=sys.stderr)
    print("     the relocations separate them where the bytes cannot.",
          file=sys.stderr)
    print("  2. Re-home them onto a free, byte-class-identical name: "
          "scripts/harvest/dupname_rebijection.py.", file=sys.stderr)
    print("     Never GUESS a re-home target -- an unfree target just moves "
          "the collision (2eb6307a).", file=sys.stderr)
    print("  3. If identity cannot be established at all, add the addresses "
          "to `_denylist` WITH a", file=sys.stderr)
    print("     rationale in `_denylist_comment`. That unclaims them; it does "
          "not assert they are wrong.", file=sys.stderr)
    print("  4. Only if the name is a file-static (internal linkage, one VA "
          "per defining TU, and the", file=sys.stderr)
    print("     compiled bodies DIFFER between units): add the NAME to "
          "`_internal_linkage_allow`.", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
