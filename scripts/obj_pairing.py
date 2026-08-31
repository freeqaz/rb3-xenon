#!/usr/bin/env python3
"""The one place that answers "which target .obj does this compiled .obj pair with?"

Why this module exists
----------------------
Four of the post-compile patchers need retail's symbol table for the TU they
are rewriting, so each one has to map

    build/45410914/src/<source tree path>/Foo.obj   (what we compiled)
        -->  build/45410914/obj/<splits heading>.obj   (what dtk split out)

Three of them (`obj_guard_patcher`, `obj_bool_mangle_patcher`,
`obj_atexit_scope_patcher`) did that by RELPATH -- `obj_dir / rel` where `rel`
is the base's path under `src_dir` -- and silently `continue`d when the guessed
file did not exist.  That guess is structurally wrong on this repo, because
dtk names each target object after its **splits.txt heading** and the
overwhelming majority of headings are bare basenames:

    heading   `MasterAudio.cpp:`        -> build/45410914/obj/MasterAudio.obj
    our obj                                build/45410914/src/system/beatmatch/MasterAudio.obj

so the relpath guess `obj/system/beatmatch/MasterAudio.obj` does not exist and
the pair is INVISIBLE.  Measured on main at `0d125b35` (rb3-xenon, title
45410914): of **1,045 distinct compiled objects that objdiff.json declares a
target for, relpath pairing reached 344 and missed 701 (67.1%)** -- and a
silent `continue` is indistinguishable from "this object needed no patch".
That is the exact failure class this project has spent a week closing: a result
that cannot report its own gap.

The stake is not theoretical.  Running the three passes over the objdiff.json
pairing on a fully-built tree found **7 pending `$S` -> `??_B` guard renames,
all 7 of them in the 701 the relpath guess could not see**, on
NetGameMsgs, AccomplishmentSetlist, AppLabel, OvershellSlot, CharClipDriver,
DataArraySongInfo and LocaleOrdinal.  `bool_mangle` and `atexit_scope` had 0
pending over the full 1,045 (they are genuinely idle here, which is worth
knowing and is reported rather than hidden).

`obj_anon_ns_patcher.py` already read objdiff.json (lane CN-1) and is the model
this generalises; it is left alone deliberately -- its pairing already reaches
the whole population -- but it shares the ambiguity below.

objdiff.json is authoritative, not a heuristic
----------------------------------------------
`configure.py` writes one unit per pinned TU carrying the explicit
`target_path`/`base_path` pair, and it is the very same pairing objdiff uses to
SCORE.  Deriving the mapping from it means a patcher can never disagree with
the scorer.  Deliberately NOT a basename heuristic: `build/45410914/obj/`
accumulates orphan target objects from splits headings that were since removed,
and a "unique basename" fallback pairs live compiled objects against dead
stubs -- inventing pairings rather than recovering real ones.

★ Ambiguity is REAL here and is surfaced, not resolved by luck
--------------------------------------------------------------
Three compiled objects are declared by **two units each**, pointing at two
DIFFERENT target objects:

    src/band3/meta_band/UIStats.obj              obj/UIStats.obj
                                             +   obj/band3/meta_band/UIStats.obj
    src/band3/meta_band/AccomplishmentProgress.obj   (likewise)
    src/band3/game/Game.obj                          (likewise)

That is not a pairing bug, it is a **splits.txt defect**: `splits.txt` carries
both a path-qualified heading and a bare heading for one retail TU, and
`tools/project.py`'s basename alias then binds our single compiled object to
both.  The two halves are provably one TU -- their address ranges are
contiguous/interleaved (UIStats: flat `.text` 0x8255F5C8-0x825605F4, nested
0x825605F4-0x82560B08) and their report.json function sets are **disjoint**
(overlap 0), so nothing is double-counted, but each half is scored against a
source file that only supplies half its functions.  Fixing that moves matched
bytes and belongs to a splits lane.

What THIS module does about it: returns **every** declared target, in
objdiff.json order, and counts the object in `objects_multi_target`.  A caller
runs its pass once per target.  That is strictly more correct than picking one
-- the union is the complete retail symbol set for that source file -- and, in
the 1,042 unambiguous cases, byte-identically what a single target gives.
Silently taking `dict[base] = target` (last unit wins) was the previous
behaviour of every caller including `obj_anon_ns_patcher`, and it is precisely
the "picked one and said nothing" shape being removed.

Relpath is kept as a FALLBACK, not a preference
-----------------------------------------------
160 compiled objects on disk are not declared by any objdiff.json unit (they
are built but unpinned).  For those, and only those, the relpath guess is still
tried -- it costs nothing and cannot invent a pairing, because it only succeeds
when a file with that exact path really exists.  Where objdiff.json speaks, it
wins: preferring relpath is what made the three ambiguous objects resolve to
the nested target while objdiff.json scored against the flat one.
"""

import json
import os
from pathlib import Path

DEFAULT_VERSION = os.environ.get("RB3_VERSION", "45410914")


class ObjPairing:
    """Resolve compiled-object -> target-object(s), and state its denominators.

    All paths in and out are **relpaths under `src_dir` / `obj_dir`**, matching
    what the patchers already pass around.
    """

    def __init__(self, repo, obj_dir=None, src_dir=None, config_path=None):
        self.repo = Path(repo).resolve()
        build = self.repo / "build" / DEFAULT_VERSION
        self.obj_dir = Path(obj_dir).resolve() if obj_dir else (build / "obj")
        self.src_dir = Path(src_dir).resolve() if src_dir else (build / "src")
        self.config_path = (Path(config_path) if config_path
                            else self.repo / "objdiff.json")

        #: base_rel -> [target_rel, ...] in objdiff.json declaration order.
        self.declared = {}
        #: units that named a target/base pair whose files are not both present
        self.declared_units = 0
        self.units_dropped_missing_file = 0
        self.units_dropped_outside_dirs = 0
        self.config_readable = False
        self._load()

    # ── loading ──────────────────────────────────────────────────────────

    def _load(self):
        try:
            cfg = json.loads(self.config_path.read_text())
        except (OSError, ValueError):
            return
        self.config_readable = True
        root = self.config_path.resolve().parent
        for unit in cfg.get("units") or []:
            t, b = unit.get("target_path"), unit.get("base_path")
            if not (t and b):
                continue
            tp, bp = (root / t).resolve(), (root / b).resolve()
            if not (tp.exists() and bp.exists()):
                self.units_dropped_missing_file += 1
                continue
            try:
                t_rel = str(tp.relative_to(self.obj_dir))
                b_rel = str(bp.relative_to(self.src_dir))
            except ValueError:
                # The caller pointed --obj-dir/--src-dir somewhere else; this
                # pair is not ours to offer.  Counted, never silent.
                self.units_dropped_outside_dirs += 1
                continue
            self.declared_units += 1
            seen = self.declared.setdefault(b_rel, [])
            if t_rel not in seen:
                seen.append(t_rel)

    # ── resolution ───────────────────────────────────────────────────────

    def targets_for(self, base_rel):
        """Every target relpath for one compiled object, most authoritative first.

        Returns `[]` when nothing pairs -- callers must treat that as "not
        examined", never as "examined and clean".
        """
        base_rel = str(base_rel)
        declared = self.declared.get(base_rel)
        if declared:
            return list(declared)
        # Undeclared object: the relpath guess is the only thing left, and it
        # only ever succeeds against a file that actually exists.
        if (self.obj_dir / base_rel).exists():
            return [base_rel]
        return []

    def target_paths_for(self, base_rel):
        """`targets_for`, as absolute `Path`s."""
        return [self.obj_dir / t for t in self.targets_for(base_rel)]

    def compiled_objects(self):
        """Every compiled object on disk, as relpaths under `src_dir`."""
        if not self.src_dir.is_dir():
            return []
        return sorted(str(p.relative_to(self.src_dir))
                      for p in self.src_dir.rglob("*.obj") if p.is_file())

    # ── denominators ─────────────────────────────────────────────────────

    def coverage(self):
        """What a green light from a pairing-driven pass is actually worth.

        Every count is over DISTINCT COMPILED OBJECTS except `declared_units`,
        which is over objdiff.json units -- the two differ by exactly the
        multi-target objects, and conflating them is how the historical figure
        "347 of 1048" was reported for a loop that examined 344.
        """
        on_disk = self.compiled_objects()
        declared = set(self.declared)
        paired, unpaired, multi, pairs = [], [], [], 0
        for rel in on_disk:
            tg = self.targets_for(rel)
            pairs += len(tg)
            if tg:
                paired.append(rel)
                if len(tg) > 1:
                    multi.append(rel)
            else:
                unpaired.append(rel)
        paired_set = set(paired)
        # The number that must never regress: a declared pair the chain cannot
        # see is a patch that silently did not happen.
        declared_unpaired = sorted(declared - paired_set)
        # Legacy relpath-only reach, kept so the before/after stays checkable.
        relpath_only = sum(1 for rel in declared
                           if (self.obj_dir / rel).exists())
        return {
            "config_readable": self.config_readable,
            "declared_units": self.declared_units,
            "units_dropped_missing_file": self.units_dropped_missing_file,
            "units_dropped_outside_dirs": self.units_dropped_outside_dirs,
            "objects_on_disk": len(on_disk),
            "objects_declared": len(declared),
            "objects_paired": len(paired),
            "objects_unpaired": len(unpaired),
            "objects_undeclared_but_on_disk": len(set(on_disk) - declared),
            "objects_multi_target": len(multi),
            "multi_target_objects": sorted(multi),
            "declared_unpaired": declared_unpaired,
            "pairs_resolved": pairs,
            "relpath_only_reachable": relpath_only,
        }


def coverage_line(cov):
    """One line, always naming the denominator it is a fraction of."""
    d = cov["objects_declared"]
    pct = 100.0 * cov["objects_paired"] / d if d else 0.0
    return ("[pairing] {p}/{d} declared compiled objects pair with a target "
            "({pct:.1f}%); relpath-only would reach {r}; {m} object(s) declared "
            "by >1 unit; {u} on-disk object(s) undeclared"
            .format(p=cov["objects_paired"], d=d, pct=pct,
                    r=cov["relpath_only_reachable"],
                    m=cov["objects_multi_target"],
                    u=cov["objects_undeclared_but_on_disk"]))


class PairingCoverageError(Exception):
    """Raised when the pairing cannot see the population it claims to cover."""


def assert_full_coverage(cov, *, min_declared=1):
    """Refuse a pairing that is vacuous or incomplete.

    Two failure modes, deliberately separate:

    `min_declared`  "examined zero things" must not read as success.  A tree
                    with no objdiff.json, an unreadable one, or one whose units
                    all dropped, would otherwise satisfy every ratio below
                    trivially -- that vacuity is one of the original defects
                    this lane exists to close, so it is checked FIRST and
                    reported with a distinct message.

    `declared_unpaired`
                    the regression guard proper.  If a declared target/base
                    pair stops resolving -- someone reverts to relpath keying,
                    or renames a directory -- the patchers go quietly blind on
                    it.  Failing here is the only thing that turns that back
                    into a build error instead of a lower number.
    """
    if not cov.get("config_readable"):
        raise PairingCoverageError(
            "objdiff.json is absent or unreadable, so NO pairing could be "
            "derived.  A pass driven by this would examine nothing and exit "
            "clean -- refusing instead.  Run configure.py.")
    if cov["objects_declared"] < min_declared:
        raise PairingCoverageError(
            "objdiff.json declares {n} pairable compiled objects (expected at "
            "least {m}).  A green light over {n} objects is vacuous: it says "
            "nothing was examined, not that nothing was wrong.  {d} unit(s) "
            "were dropped for a missing file and {o} for landing outside "
            "obj_dir/src_dir."
            .format(n=cov["objects_declared"], m=min_declared,
                    d=cov["units_dropped_missing_file"],
                    o=cov["units_dropped_outside_dirs"]))
    bad = cov["declared_unpaired"]
    if bad:
        raise PairingCoverageError(
            "{n} of {d} declared compiled objects resolve to NO target "
            "object, so every pairing-driven post-compile pass skips them "
            "silently -- indistinguishable from 'needed no patch'.  "
            "First few: {sample}"
            .format(n=len(bad), d=cov["objects_declared"],
                    sample=", ".join(bad[:5])))


def main(argv=None):
    import argparse
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--repo", default=str(Path(__file__).resolve().parent.parent))
    ap.add_argument("--obj-dir")
    ap.add_argument("--src-dir")
    ap.add_argument("--objdiff-config")
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--check", action="store_true",
                    help="exit 2 if the pairing is vacuous or incomplete")
    a = ap.parse_args(argv)
    p = ObjPairing(a.repo, a.obj_dir, a.src_dir, a.objdiff_config)
    cov = p.coverage()
    if a.json:
        print(json.dumps(cov, indent=1, sort_keys=True))
    else:
        print(coverage_line(cov))
        for rel in cov["multi_target_objects"]:
            print("  multi-target: {} -> {}".format(rel, p.targets_for(rel)))
    if a.check:
        try:
            assert_full_coverage(cov)
        except PairingCoverageError as e:
            import sys
            print("FAIL[pairing]: %s" % e, file=sys.stderr)
            return 2
    return 0


if __name__ == "__main__":
    import sys
    sys.exit(main())
