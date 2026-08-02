#!/usr/bin/env python3
"""Relocation-masked byte-identity comparator, with the anti-vacuity guard as a
HARD REFUSAL rather than an advisory field.

    python3 tools/masked_byte_identity.py fn <unit> <VA>
    python3 tools/masked_byte_identity.py unit <unit> [--max-size N]
    python3 tools/masked_byte_identity.py --selftest

★★ THE STANDING CAVEAT (printed on every result, and meant literally)
---------------------------------------------------------------------
    BYTE IDENTITY PROVES WHAT A FUNCTION *EQUALS*, NOT WHERE IT *LIVES*.
    APPLY THE HOME-UNIT GATE BEFORE ANY REPOINT.

Lane DD-4 pushed three repoints that passed a strong byte test and all three
were wrong.  The mechanism is not subtle: identical bodies are *common*.  The
tool's own selftest specimen makes the point — the 88-byte target at
0x82637190 is reloc-masked-identical to BOTH `??_GSigninScreen@@UAAPAXI@Z`
(right) and `??_Gbad_alloc@std@@UAAPAXI@Z` (wrong, and already claimed
elsewhere).  A single exact hit is a *candidate*, never a verdict.

★ THE ANTI-VACUITY GUARD (memory: >=4 real words AND >=50% of body real)
-------------------------------------------------------------------------
A masked comparison over a body dominated by relocated (zeroed) words compares
almost nothing, so it "matches" almost everything.  Equally, a 2-3 word body is
too short to discriminate — the FastCos trap class.  This tool therefore
**REFUSES** such comparisons with a distinct exit code instead of reporting a
match that the caller will read as evidence:

    exit 0  OK        - guard passed; results reported
    exit 3  REFUSED   - guard failed; NO match is reported, by design
    exit 1  error / no data / selftest failure

Refusal is not conservatism: both selftest refusal specimens WOULD otherwise
have reported exactly one EXACT match each (see --selftest), i.e. the guard
suppresses would-be positives, not empty cases.

SUBSUMES ~/tmp/laneDE3/identity.py (guard was computed but only *reported*
there; here it is enforced).  Body masking/loading is reused from
scripts/harvest/size_order_automap.py rather than reimplemented — that module
owns the COFF + dtk-asm parsing and is the single source of truth for what
"reloc-masked bytes" means.
"""
from __future__ import annotations

import argparse
import json
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional, Tuple

EXIT_OK = 0
EXIT_ERROR = 1
EXIT_REFUSED = 3

MIN_REAL_WORDS = 4
MIN_REAL_FRACTION = 0.50

CAVEAT = ("byte identity proves what a function EQUALS, not where it LIVES "
          "- apply the home-unit gate before any repoint")


def _repo_root() -> Path:
    return Path(__file__).resolve().parent.parent


def _load_soa(project_dir: Optional[Path] = None):
    """Import scripts/harvest/size_order_automap.py, pointed at project_dir."""
    root = Path(project_dir).resolve() if project_dir else _repo_root()
    import importlib.util
    p = root / "scripts/harvest/size_order_automap.py"
    if not p.exists():
        raise SystemExit(f"missing {p}")
    spec = importlib.util.spec_from_file_location("_soa_mbi", p)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    m.PROJECT_ROOT = root
    m.BUILD = root / "build/45410914"
    return m


@dataclass
class Guard:
    n_words: int
    n_masked_words: int

    @property
    def n_real(self) -> int:
        return self.n_words - self.n_masked_words

    @property
    def fraction(self) -> float:
        return (self.n_real / self.n_words) if self.n_words else 0.0

    @property
    def ok(self) -> bool:
        return (self.n_words > 0
                and self.n_real >= MIN_REAL_WORDS
                and self.fraction >= MIN_REAL_FRACTION)

    @property
    def why(self) -> str:
        if self.n_words == 0:
            return "empty body"
        bad = []
        if self.n_real < MIN_REAL_WORDS:
            bad.append(f"only {self.n_real} real word(s) < {MIN_REAL_WORDS}")
        if self.fraction < MIN_REAL_FRACTION:
            bad.append(f"real fraction {self.fraction:.0%} < {MIN_REAL_FRACTION:.0%}")
        return " AND ".join(bad) if bad else "ok"

    def line(self) -> str:
        return (f"words={self.n_words} masked={self.n_masked_words} "
                f"real={self.n_real} ({self.fraction:.0%})")


def guard_of(masked: bytes) -> Guard:
    n = len(masked) // 4
    z = sum(1 for i in range(n) if masked[i * 4:(i + 1) * 4] == b"\0\0\0\0")
    return Guard(n, z)


@dataclass
class Result:
    unit: str
    va: int
    size: int
    guard: Guard
    exact: List[Tuple[str, bool]]      # (name, already_claimed_elsewhere)
    near: List[Tuple[float, int, str, bool]]
    refused: bool
    reason: str = ""
    bypassed: bool = False      # guard failed but --no-enforce reported anyway


class Comparator:
    def __init__(self, project_dir: Optional[Path] = None):
        self.root = Path(project_dir).resolve() if project_dir else _repo_root()
        self.S = _load_soa(self.root)
        mp = self.root / "scripts/target_symbol_map.json"
        raw = json.loads(mp.read_text()) if mp.exists() else {}
        self.map_i = {int(k, 16): v for k, v in raw.items()
                      if k.startswith("0x") and isinstance(v, str)}
        self.all_values = {v for k, v in raw.items()
                           if k.startswith("0x") and isinstance(v, str)}

    def compare(self, unit: str, va: int, *, enforce: bool = True,
                near_threshold: float = 75.0,
                include_mapped: bool = False) -> Result:
        """include_mapped=True keeps names already paired via
        target_symbol_map.json.  Default False ("what is still unclaimed?");
        True answers "what is this body byte-equal to, regardless of current
        pairing" — which is what you want to BYTE-VERIFY AN EXISTING ROW, and
        what the controls use so they do not depend on which rows have landed.
        """
        S = self.S
        name, tobj, tasm, cobj = S.resolve_unit(unit)
        if not cobj.exists():
            raise SystemExit(f"no compiled obj for {unit} ({cobj})")
        T = S.load_target(tobj, tasm)
        tf = next((t for t in T if t.va == va), None)
        if tf is None:
            raise SystemExit(f"{va:#x} not present in target asm for {unit}")
        g = guard_of(tf.masked)
        if enforce and not g.ok:
            return Result(name, va, tf.size, g, [], [], refused=True, reason=g.why)

        mapped = (set() if include_mapped
                  else {S.anon_ns_strip(self.map_i[t.va]) for t in T if t.va in self.map_i})
        C = S.load_compiled(cobj)
        exact, near = [], []
        for c in C:
            if S.anon_ns_strip(c.name) in mapped:
                continue                                   # already paired
            claimed = c.name in self.all_values
            if c.size == tf.size and c.masked == tf.masked:
                exact.append((c.name, claimed))
            else:
                ident = S._identity_pct(tf.masked, c.masked)
                if ident >= near_threshold and abs(c.size - tf.size) <= 8:
                    near.append((round(ident, 1), c.size, c.name, claimed))
        near.sort(reverse=True)
        return Result(name, va, tf.size, g, exact, near[:6], refused=False,
                      bypassed=not g.ok)


def print_result(r: Result) -> int:
    print(f"{r.unit}  {r.va:#010x}  size={r.size}  {r.guard.line()}")
    if r.refused:
        print(f"  REFUSED: {r.reason}")
        print("  -> NO match is reported. A masked comparison this thin does not")
        print("     discriminate; reporting one would manufacture evidence.")
        print(f"  NOTE: {CAVEAT}")
        return EXIT_REFUSED
    if r.bypassed:
        print(f"  guard BYPASSED (--no-enforce): {r.guard.why}. "
              f"THIS OUTPUT IS NOT EVIDENCE.")
    else:
        print(f"  guard PASS ({r.guard.n_real} real words, {r.guard.fraction:.0%} of body)")
    if not r.exact and not r.near:
        print("  no exact or near candidate")
    for n, claimed in r.exact:
        print(f"    EXACT {'[CLAIMED ELSEWHERE] ' if claimed else ''}{n}")
    for pct, sz, n, claimed in r.near:
        print(f"    near {pct:5.1f}% size={sz:<5d} {'[CLAIMED] ' if claimed else ''}{n}")
    if len(r.exact) > 1:
        print(f"  !! {len(r.exact)} exact candidates - identity ALONE cannot choose between them")
    print(f"  CAVEAT: {CAVEAT}")
    return EXIT_OK


# ==========================================================================
# CONTROLS
# ==========================================================================
def _selftest(project_dir: Optional[Path]) -> int:
    """Three controls: a guard-passing positive, and two refusals that would
    OTHERWISE have reported a match (so the refusal path is not vacuous)."""
    try:
        cmp_ = Comparator(project_dir)
    except SystemExit as e:
        print(f"SELFTEST SETUP FAILED: {e}")
        return EXIT_ERROR
    print(f"masked_byte_identity selftest  root={cmp_.root}")
    print("  (controls use include_mapped=True so they do not depend on which "
          "map rows have landed)")
    rows: List[Tuple[str, bool, str]] = []

    # (1) POSITIVE (lane DE-3): 88 B, 22 words, 4 masked -> 18 real, guard PASS,
    #     and TWO exact hits - which is itself the caveat made concrete.
    try:
        r = cmp_.compare("default/band3/meta_band/SigninScreen", 0x82637190,
                         include_mapped=True)
        names = {n for n, _ in r.exact}
        ok = (not r.refused and r.guard.n_words == 22 and r.guard.n_masked_words == 4
              and r.guard.ok and "??_GSigninScreen@@UAAPAXI@Z" in names)
        rows.append(("positive: SigninScreen 0x82637190 guard PASS 18/22 real", ok,
                     f"{r.guard.line()} refused={r.refused} exact={sorted(names)}"))
        rows.append(("positive: identity alone is AMBIGUOUS (>1 exact hit)",
                     len(r.exact) > 1,
                     f"{len(r.exact)} exact candidates: {sorted(names)}"))
    except Exception as e:
        rows.append(("positive: SigninScreen 0x82637190 guard PASS 18/22 real", False, f"EXC {e!r}"))

    # (2) REFUSAL, word-count arm ALONE: 12 B, 3 words, 0 masked. 100% real yet
    #     too short to discriminate - the FastCos trap class. Would otherwise
    #     have reported EXACT ?GetWhammyBar@JoypadMidiController@@UBAMXZ.
    for label, unit, va, want_would_be in [
        ("refusal(word-count arm): JoypadMidiController 0x82797880 12B 3 words 0 masked",
         "system/beatmatch/JoypadMidiController", 0x82797880,
         "?GetWhammyBar@JoypadMidiController@@UBAMXZ"),
        # (3) REFUSAL, both arms: 16 B, 4 words, 3 masked -> 1 real (25%).
        ("refusal(both arms): JoypadGuitarController 0x8279b270 16B 4 words 3 masked",
         "system/beatmatch/JoypadGuitarController", 0x8279B270,
         "??1BeatMatchController@@UAA@XZ"),
    ]:
        try:
            r = cmp_.compare(unit, va, include_mapped=True)   # enforced
            u = cmp_.compare(unit, va, enforce=False,         # what it WOULD say
                             include_mapped=True)
            would = {n for n, _ in u.exact}
            ok = (r.refused and not r.exact and want_would_be in would)
            rows.append((label, ok,
                         f"refused={r.refused} reason='{r.reason}' "
                         f"would-be exact={sorted(would)} (must contain {want_would_be})"))
        except Exception as e:
            rows.append((label, False, f"EXC {e!r}"))

    nfail = 0
    for name, ok, detail in rows:
        print(f"  [{'PASS' if ok else 'FAIL'}] {name}\n         {detail}")
        nfail += (not ok)
    print(f"  {len(rows) - nfail}/{len(rows)} controls passed")
    print(f"  CAVEAT: {CAVEAT}")
    return EXIT_ERROR if nfail else EXIT_OK


def main(argv: Optional[List[str]] = None) -> int:
    ap = argparse.ArgumentParser(
        description="Relocation-masked byte-identity comparator. "
                    "REFUSES (exit 3) when the anti-vacuity guard fails.",
        epilog="CAVEAT PRINTED ON EVERY RESULT: " + CAVEAT.upper()
               + ".  Lane DD-4 landed 3 repoints that passed a strong byte "
                 "test and were all wrong.",
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--project-dir", type=Path, default=None,
                    help="worktree to read build/45410914 from (default: this repo)")
    ap.add_argument("--selftest", action="store_true",
                    help="run controls (positive + two non-vacuous refusals)")
    ap.add_argument("--no-enforce", action="store_true",
                    help="DIAGNOSTIC ONLY: report despite a failing guard. "
                         "Never cite output produced with this flag as evidence.")
    ap.add_argument("--include-mapped", action="store_true",
                    help="also report names already paired in target_symbol_map.json "
                         "(use to BYTE-VERIFY an existing row; default hides them)")
    ap.add_argument("--json", action="store_true", help="machine-readable output")
    sub = ap.add_subparsers(dest="cmd")
    p = sub.add_parser("fn", help="compare one target function")
    p.add_argument("unit"); p.add_argument("va")
    p = sub.add_parser("unit", help="compare every unmapped target function in a unit")
    p.add_argument("unit"); p.add_argument("--max-size", type=int, default=0)
    a = ap.parse_args(argv)

    if a.selftest:
        return _selftest(a.project_dir)
    if not a.cmd:
        ap.print_help()
        return EXIT_OK

    cmp_ = Comparator(a.project_dir)
    enforce = not a.no_enforce
    if a.no_enforce:
        print("!! --no-enforce: guard NOT enforced. Output is NOT evidence.", file=sys.stderr)

    if a.cmd == "fn":
        r = cmp_.compare(a.unit, int(a.va, 16), enforce=enforce,
                         include_mapped=a.include_mapped)
        if a.json:
            print(json.dumps(dict(unit=r.unit, va=hex(r.va), size=r.size,
                                  n_words=r.guard.n_words,
                                  n_masked_words=r.guard.n_masked_words,
                                  guard_ok=r.guard.ok, refused=r.refused,
                                  bypassed=r.bypassed,
                                  reason=r.reason, exact=r.exact, near=r.near,
                                  caveat=CAVEAT), indent=1))
            return EXIT_REFUSED if r.refused else EXIT_OK
        return print_result(r)

    if a.cmd == "unit":
        S = cmp_.S
        name, tobj, tasm, cobj = S.resolve_unit(a.unit)
        rc = EXIT_OK
        n_ref = 0
        for t in S.load_target(tobj, tasm):
            if t.va in cmp_.map_i:
                continue
            if a.max_size and t.size > a.max_size:
                continue
            r = cmp_.compare(a.unit, t.va, enforce=enforce,
                             include_mapped=a.include_mapped)
            if r.refused:
                n_ref += 1
                print(f"{r.unit}  {r.va:#010x}  size={r.size}  REFUSED ({r.reason})")
                continue
            if r.exact or r.near:
                print_result(r)
        print(f"\n[{n_ref} comparison(s) refused by the anti-vacuity guard]")
        print(f"CAVEAT: {CAVEAT}")
        return rc

    return EXIT_OK


if __name__ == "__main__":
    sys.exit(main())
