#!/usr/bin/env python3
"""Generate an MSVC-linker-map-format file declaring PROVEN ICF symbol aliases.

Why this exists (the ICF-merged-symbol aliasing gap)
----------------------------------------------------
Retail RB3 has exactly ONE ``PoolAlloc``, at **0x827bb0e8**, and
``scripts/target_symbol_map.json`` names it with the 5-arg DEBUG mangling
``?PoolAlloc@@YAPAXHHPBDH0@Z``. Our compiled objs emit the byte-IDENTICAL call
site (``li r3,N`` / ``li r4,N`` / ``bl``) but reference the 2-arg name
(``?PoolAlloc@@YAPAXHH@Z``), so objdiff -- which compares ``bl`` relocations BY
NAME (objdiff-core ``reloc_eq``) -- flags a ``[sym]`` mismatch though the machine
code matches exactly. Same story for the 1-arg ``MemOrPoolAlloc`` family.

⚠ CORRECTION (lane ALIAS-X2, 2026-08-13). This docstring said ``@ 0x827960D8``
for months; that is a stale pre-TU5 address which names NOTHING in the current
map, and the two founding alias groups carried it in their ``address`` field --
2 of 1449 groups, the only two whose address disagreed with the map's placement
of their own survivor. Corrected to the real addresses (PoolAlloc 0x827bb0e8,
MemOrPoolAlloc 0x827bd208); measured Δ0 on BOTH rulers, as predicted, because
the address is only a bucket key (see ``render_map`` below and the note on
``g["address"]`` in scripts/symbol_aliases.json's header).

⚠ And the framing "ICF-folds the 2-arg path onto the 5-arg allocator" is
IMPRECISE, which matters when auditing this file. There are not two bodies here
for the linker to fold: retail's body at 0x827bb0e8 reads r3 only and never
touches r5/r6/r7, so it cannot be a 5-arg function at all. What these two
founding groups actually repair is a MAP NAMING defect -- a debug-build spelling
parked on a retail address -- not a fold. The alias is still a true statement
about which callee a ``bl`` denotes (retail has exactly one PoolAlloc and one
MemOrPoolAlloc; zero other retail bodies are masked-identical to either), which
is why it is KEPT. Do not let the word "fold" here be read as evidence that two
distinct functions were proven identical.

The seam
--------
The objdiff fork (``../objdiff``) ALREADY supports ICF symbol equivalences via a
``map_file`` declared in ``objdiff.json`` (the ``ProjectConfig.map_file`` field;
``objdiff-core/src/obj/map_file.rs`` ``parse_msvc_map`` groups every symbol name
sharing an 8-hex address, and ``reloc_eq`` treats names in the same group as
equal). We have no real MSVC ``.map``, but we can emit a *synthetic* one whose
only content is the proven folds: two lines per fold, both with the survivor's
address, so the parser groups them. NO objdiff source change is required -- the
mechanism is mature, tested, and threaded through the report path.

This is the SMALLEST mechanism (option (c)/data-driven from the task brief): a
single data file (``scripts/symbol_aliases.json``) -> a synthetic map -> one
``objdiff.json`` field, applied at ONE seam (report/diff time, not the renamer or
the post-compile patchers).

Why NOT the pre-compile renamer (option (a))
--------------------------------------------
The renamer would have to rewrite the target's 5-arg ``?PoolAlloc...PBDH0@Z`` ref
to the 2-arg name, but the target's 5-arg symbol is ALSO the legitimate callee of
real 5-arg debug call sites (utl/MemMgr.cpp, synth_xbox/StreamReceiver.cpp keep
the 5-arg form) AND the function definition itself. A blanket target rename would
un-pair those. An equivalence GROUP (this approach) keeps every real pairing
working in BOTH directions while also accepting the folded spelling -- it is the
correct, symmetric seam.

Usage
-----
    python3 tools/gen_symbol_alias_map.py            # write build/45410914/icf_aliases.map
    python3 tools/gen_symbol_alias_map.py --check     # verify it's up to date (exit 1 if stale)
    python3 tools/gen_symbol_alias_map.py --out PATH  # custom output path

The output path is what ``objdiff.json``'s ``map_file`` points to (wired by
``tools/project.py``). Editing ``scripts/symbol_aliases.json`` and running
``ninja`` is sufficient: the ``icf_alias_map`` edge re-renders it and the
``icf_alias_map_checked`` edge asserts it on every build. See the design comment
in ``tools/project.py``, which is the one place per repo this rule is written
down.
"""

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import stamp_if_changed  # noqa: E402

PROJECT_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_ALIASES = PROJECT_ROOT / "scripts" / "symbol_aliases.json"
DEFAULT_OUT = PROJECT_ROOT / "build" / "45410914" / "icf_aliases.map"

# The parse_msvc_map regex (objdiff-core/src/obj/map_file.rs) is:
#   ^\s*\d{4}:[0-9a-fA-F]+\s+(\S+)\s+([0-9a-fA-F]{8})\s+
# i.e.  SSSS:OOOOOOOO   <symbol>   AAAAAAAA   <rest>
# Group 1 = symbol name, group 2 = the 8-hex ADDRESS used to bucket ICF folds.
# We emit one line per symbol in each group, all sharing the group's address.
MAP_LINE = " 0001:00000000       {sym:<60} {addr}  f i icf_aliases.synthetic\n"

HEADER = (
    "; SYNTHETIC ICF-alias map -- generated by tools/gen_symbol_alias_map.py\n"
    "; DO NOT EDIT BY HAND. Source of truth: scripts/symbol_aliases.json\n"
    "; Consumed by objdiff via objdiff.json 'map_file' -> parse_msvc_map ->\n"
    "; symbol_equivalences (objdiff-core reloc_eq). Each group below is a PROVEN\n"
    "; identical-COMDAT fold: all spellings share the survivor's address so\n"
    "; objdiff treats their bl relocations as name-equal.\n"
    ";\n"
    "; Address                        Publics by Value\n"
)


def rel(p: Path) -> str:
    """Repo-relative when possible: the defaults resolve absolute, and an absolute
    machine path in a build-failure message is noise nobody can paste."""
    try:
        return str(p.resolve().relative_to(Path.cwd().resolve()))
    except ValueError:
        return str(p)


def load_groups(aliases_path: Path) -> list:
    raw = json.loads(aliases_path.read_text())
    return raw.get("groups", [])


def normalize_addr(addr: str) -> str:
    """Return the 8-uppercase-hex form parse_msvc_map expects."""
    a = addr.lower().removeprefix("0x")
    val = int(a, 16)
    return f"{val:08X}"


def render_map(groups: list) -> str:
    out = [HEADER]
    for g in groups:
        # A group with no address has no retail placement, and this map is keyed
        # BY address -- parse_msvc_map groups whatever shares one. Borrowing some
        # other group's address to render it would put its spellings in that
        # group's bucket, i.e. assert a fold nobody proved. Skip it: no address,
        # no map equivalence. (A partitioned-out class from
        # decomp-synth tools/il_witness/alias_repair.py is exactly this shape.)
        if not g.get("address"):
            continue
        addr = normalize_addr(g["address"])
        syms = [g["survivor"], *g.get("folded", [])]
        out.append(f"; --- ICF group {g.get('name', addr)} @ {addr} ---\n")
        for sym in syms:
            out.append(MAP_LINE.format(sym=sym, addr=addr))
    return "".join(out)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--aliases", default=str(DEFAULT_ALIASES),
                    help="alias group JSON (default: %(default)s)")
    ap.add_argument("--out", default=str(DEFAULT_OUT),
                    help="output .map path (default: %(default)s)")
    ap.add_argument("--check", action="store_true",
                    help="verify the output is up to date; exit 1 if stale")
    stamp_if_changed.add_arguments(ap)
    args = ap.parse_args()

    aliases_path = Path(args.aliases)
    if not aliases_path.is_file():
        print(f"ERROR: no alias file at {aliases_path}", file=sys.stderr)
        return 1
    groups = load_groups(aliases_path)
    content = render_map(groups)

    out_path = Path(args.out)
    if args.check:
        # The remediation line is not decoration: --check is reached from the
        # build, where the reader is whoever ran `ninja` and not whoever left the
        # map stale.
        fix = f"  fix: python3 tools/gen_symbol_alias_map.py --out {rel(out_path)}"
        if not out_path.is_file():
            print(f"STALE: {rel(out_path)} does not exist\n{fix}", file=sys.stderr)
            return 1
        if out_path.read_text() != content:
            print(f"STALE: {rel(out_path)} disagrees with {rel(aliases_path)}.\n"
                  f"  ninja renders this map from that JSON by mtime, so a map that is\n"
                  f"  NEWER than the JSON (hand-edited, or rendered from a different\n"
                  f"  --aliases) or a JSON restored with an OLD mtime (cp -a, tar,\n"
                  f"  rsync -a) is invisible to the edge and measures as a silent lie.\n"
                  f"{fix}", file=sys.stderr)
            return 1
        print(f"OK: {out_path} up to date ({len(groups)} ICF groups)")
        # Green path only -- see the same note in tools/map_name_injectivity.py.
        stamp_if_changed.apply(args)
        return 0

    out_path.parent.mkdir(parents=True, exist_ok=True)
    # *** WRITE ONLY WHEN THE CONTENT CHANGES. ***
    # The map is a ninja output (tools/project.py's `icf_alias_map` edge) AND is
    # re-rendered at configure time, so an unconditional write bumps its mtime on
    # every configure -- which would drag the multi-minute `report generate`
    # behind it for a file whose bytes did not move. Paired with `restat = 1` on
    # the rule: ninja re-stats the output, sees an unchanged mtime, marks the
    # edge clean and does NOT rebuild the report. Without the restat this pairing
    # would be worse than an unconditional write (the output would stay older
    # than its input forever and the generator would re-run on every build); the
    # two go together, do not split them.
    unchanged = out_path.is_file() and out_path.read_text() == content
    if not unchanged:
        out_path.write_text(content)
    # Count what was RENDERED, not what was offered: an address-less group is
    # skipped above, so summing over `groups` overstates the map by exactly the
    # skipped memberships and invites a reader to price a channel off a number
    # the file does not contain.
    n_syms = sum(1 for l in content.splitlines() if l.startswith(" 0001:"))
    n_placed = sum(1 for g in groups if g.get("address"))
    verb = "unchanged" if unchanged else "wrote"
    skipped = len(groups) - n_placed
    tail = f" ({skipped} address-less group(s) skipped)" if skipped else ""
    print(f"{verb} {out_path}: {n_placed} ICF groups, {n_syms} symbol lines{tail}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
