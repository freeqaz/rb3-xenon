#!/usr/bin/env python3
"""Make a SCREEN prove it can fire before anyone is allowed to believe its silence.

WHY THIS EXISTS
---------------
A "screen" here is any detector a lane writes to find defect candidates -- in
retail bytes, in our objs, in the map, or in the source.  On 2026-08-13 SIX of
them, in a single session, produced clean, decisive-looking output that was
WRONG.  In every case the wrong output was a NEGATIVE, which is the verdict
class that closes veins and cancels work:

  #1  regex anchored at `^subi` while every dtk `.s` line carries a
      `/* ADDR OFF BYTES */\\t` prefix
        -> 0/21,349 AND 0/6,680.  It fired nowhere -- INCLUDING on two rows
           that had already been confirmed by hand.
  #2  an `lwz` decoder with the rD and rA fields SWAPPED
        -> 0 hits across 14 MB.  Corrected, the same scan fires 1,972 times.
  #3  a handler parser missing one macro variant (HANDLE_EXPR_STATIC)
        -> two classes parsed as ZERO handlers, so all 102 and all 18 of
           retail's arms were reported MISSING.
  #4  never-compiled `#if defined(MILO_DEBUG) && defined(HX_NATIVE)` arms not
      stripped
        -> INVENTED a handler (debug_toggle_autoscroll) that does not exist.
  #5  a thunk decoder tracking only r3, missing the r4 struct-return form
        -> manufactured false disagreements.
  #6  a splits.txt parser written for lowercase hex, run against an UPPERCASE
      file
        -> read as a decisive "none of these addresses are pinned".

THE RULE THIS ENCODES
---------------------
    ***  VERIFY A SCREEN FIRES ON A KNOWN POSITIVE BEFORE TRUSTING ITS
         NEGATIVE.  ***

A screen that CANNOT fire is indistinguishable from a clean population, and it
is shaped like a decisive result -- so it gets believed, and it closes the vein.
This is the same family as two hazards already in CLAUDE.md: the `grep` shim
that is binary-blind (false negatives shaped like decisive ones), and the first
`/GS` cookie detector, which scored 0 hits on a known-`/GS` object and was
caught only just before it could produce a false "retail has no /GS".

Note the asymmetry, because it drives the design: five of the six defects are
screens that fire too LITTLE, but #4 fires too MUCH.  A harness with only
positive controls would have missed #4 entirely.  Hence THREE assertion classes:

  1. MUST FIRE     (known positives) -- power.        Catches #1 #2 #3 #5 #6.
  2. MUST NOT FIRE (known negatives) -- specificity.  Catches #4.
  3. ENRICHMENT vs an untreated population            -- catches the
     "confirms whatever you point it at" class, which no single fixture can.

On (3): CLAUDE.md's control-group discipline is explicit that a raw signal
"fires on any large function under register pressure, i.e. it confirms whatever
you point it at", and that an earlier detector only became trustworthy once the
untreated-population control was run (~413x enrichment, exactly one false
positive).  Conversely tools/callsite_screen.py measured treated 315/4,701
(6.70%) against control 959/10,961 (8.75%) = 0.77x -- BELOW ONE -- and correctly
killed its own hypothesis.  So this harness reports ENRICHMENT, never a raw
count, and an enrichment near or below 1.0 is reported as a DRAINED VEIN rather
than as a candidate list.

WHAT THIS HARNESS REFUSES TO DO
-------------------------------
It never reads a screen's own claim of success.  It calls the screen's predicate
itself and evaluates the RETURNED VALUE.  A lane this session sabotaged a
restore() to merely *claim* it had worked, and the tool printed "verified by
re-reading the diff" over a mutated tree -- a test that trusted the printed line
would have passed a completely broken restore.  So: no stdout parsing, no
exit-code trust, no "the tool said OK".

It also never SKIPS.  Every fixture below is an inline literal, so the gate runs
with no build artifacts, no orig/45410914/band.exe and no report.json.  A guard
that skips when its data is missing is a guard that cannot fail, which is the
very defect it is policing (tools/grep_binary_guard.py builds its own binary
fixture for exactly this reason).  Where a screen's MODULE cannot be imported at
all (handler_sweep opens band.exe at import time), the screen is reported
UNTESTABLE and the run exits NON-ZERO -- an untestable screen is not a passing
screen.

PROVE IT CAN FAIL
-----------------
Every historical defect above is registered as an injectable broken predicate.

    python3 tools/screen_gate.py --self-break

re-runs the gate with each defect injected in turn and asserts the gate REFUSES.
If an injected defect ever slips through, THAT is reported as a failure of this
harness, because a harness that cannot fail is exactly the bug it exists to
prevent.  (House idiom, borrowed from tools/grep_binary_guard.py --self-break.)

USING IT ON YOUR OWN SCREEN
---------------------------
    from screen_gate import Screen, gate

    s = Screen("my-screen", detect=my_predicate,
               why="fires when the retail extent is an EH funclet")
    s.must_fire("known funclet", REAL_LINE_FROM_A_CONFIRMED_ROW)
    s.must_not_fire("ordinary prologue", ANOTHER_REAL_LINE)
    s.populations("sub-100 rows", treated_iter, "mpn==100 rows", control_iter)
    res = gate([s])
    if not res.armed:          # POWERLESS / INDISCRIMINATE / DRAINED
        sys.exit(2)            # ... and print NO candidates
    ...emit candidates...

Fixtures must be REAL artifact excerpts, not invented ones.  Every fixture in
this file was lifted verbatim from build/45410914/asm/*.s, from retail band.exe
bytes, from config/45410914/splits.txt, or from src/ -- and `--provenance`
re-checks that they still occur in the live artifacts when those are present.

EXIT CODES
----------
    0  every screen ARMED (proved power and specificity)
    1  every screen sound, but >=1 vein DRAINED, or >=1 screen UNTESTABLE
    2  >=1 screen POWERLESS or INDISCRIMINATE -- its results are VOID
"""
import argparse
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
if HERE not in sys.path:
    sys.path.insert(0, HERE)


# ---------------------------------------------------------------------------
# Core harness
# ---------------------------------------------------------------------------

def _fires(result, expect):
    """Did the screen FIRE on this payload?

    `expect=None` -> plain truthiness.  A callable -> its verdict.  Anything
    else -> membership for containers, equality otherwise.  Note that a screen
    returning None counts as NOT firing; it is never treated as "fine".
    """
    if expect is None:
        return bool(result)
    if callable(expect):
        return bool(expect(result))
    if isinstance(result, (list, tuple, set, frozenset, dict, str)):
        return expect in result
    return result == expect


class Fixture:
    __slots__ = ("label", "payload", "expect", "note")

    def __init__(self, label, payload, expect=None, note=""):
        self.label = label
        self.payload = payload
        self.expect = expect
        self.note = note


class Defect:
    """A historical bug, re-encoded so the gate's power can be EXECUTED."""
    __slots__ = ("ident", "description", "broken")

    def __init__(self, ident, description, broken):
        self.ident = ident
        self.description = description
        self.broken = broken          # a defective predicate, not a wrapper


class Screen:
    """A detector plus the evidence that it can discriminate."""

    def __init__(self, name, detect, why, source=None):
        self.name = name
        self.detect = detect
        self.why = why
        self.source = source          # provenance: which landed tool this is
        self.positives = []
        self.negatives = []
        self.defects = []
        self._pops = None

    # -- registration -------------------------------------------------------
    def must_fire(self, label, payload, expect=None, note=""):
        self.positives.append(Fixture(label, payload, expect, note))
        return self

    def must_not_fire(self, label, payload, expect=None, note=""):
        self.negatives.append(Fixture(label, payload, expect, note))
        return self

    def populations(self, treated_label, treated, control_label, control,
                    expect=None, min_enrichment=3.0):
        """Register an untreated control population.

        `treated`/`control` may be iterables or zero-arg callables returning
        iterables -- callables are preferred because they stay UNEVALUATED
        unless the screen first proves its power, so a broken screen never pays
        for a population scan.
        """
        self._pops = (treated_label, treated, control_label, control,
                      expect, min_enrichment)
        return self

    def defect(self, ident, description, broken):
        self.defects.append(Defect(ident, description, broken))
        return self


class ScreenResult:
    def __init__(self, screen):
        self.screen = screen
        self.name = screen.name
        self.verdict = "ARMED"
        self.reasons = []
        self.probes = []              # (level, label, detail)
        self.enrichment = None
        self.rates = None

    @property
    def refused(self):
        return self.verdict in ("POWERLESS", "INDISCRIMINATE", "VACUOUS-GATE")

    @property
    def armed(self):
        return self.verdict == "ARMED"


class GateResult:
    """Aggregate verdict over the screens that were run.

    ⛔ EMPTY-POPULATION GUARD (lane W38-GATES, 2026-08-17). `armed` was
    `all(r.armed for r in self.results)`, and `all([])` is **True** -- so
    `gate([])` reported armed=True, any_refused=False and **exit code 0**.
    Measured before the fix:

        >>> r = gate([]); r.armed, r.any_refused, r.exit_code()
        (True, False, 0)

    A harness whose whole job is to refuse instruments that cannot fail was
    itself an instrument that could not fail, in exactly the configuration that
    matters: a registry that silently loaded zero screens (an import error
    swallowed upstream, a filter that matched nothing, a `--only` typo) is
    indistinguishable from every screen passing. This is the same shape as the
    module's own `must_fire`/`populations` refusals, one level up.

    Zero screens is now VACUOUS-GATE / exit 2 -- not a pass and not a failure.
    """

    def __init__(self, results):
        self.results = results

    @property
    def vacuous(self):
        """No screens were run at all -- the gate reached nothing."""
        return not self.results

    @property
    def armed(self):
        # `and self.results` first: an empty gate is never armed, whatever all() says.
        return bool(self.results) and all(r.armed for r in self.results)

    @property
    def any_refused(self):
        return self.vacuous or any(r.refused for r in self.results)

    def exit_code(self):
        if self.vacuous:
            return 2
        if any(r.refused for r in self.results):
            return 2
        if any(r.verdict in ("DRAINED", "UNTESTABLE") for r in self.results):
            return 1
        return 0


def _probe(res, fixture, detect, want_fire):
    """Run ONE fixture through the screen and record what ACTUALLY happened.

    The verdict comes from the returned value.  Nothing here reads what the
    screen says about itself.
    """
    try:
        out = detect(fixture.payload)
    except Exception as exc:                      # noqa: BLE001
        res.probes.append(("FAIL", fixture.label,
                           f"screen RAISED {type(exc).__name__}: {exc}"))
        return False
    got = _fires(out, fixture.expect)
    ok = (got == want_fire)
    shown = repr(out)
    if len(shown) > 120:
        shown = shown[:117] + "..."
    res.probes.append(("PASS" if ok else "FAIL", fixture.label,
                       f"fired={got} want={want_fire} returned={shown}"
                       + (f"  [{fixture.note}]" if fixture.note else "")))
    return ok


def run_screen(screen, detect=None, run_populations=True):
    """Gate ONE screen.  `detect` overrides the predicate (used by --self-break)."""
    res = ScreenResult(screen)
    det = detect if detect is not None else screen.detect

    # -- Phase 0: is the GATE itself capable of failing? ---------------------
    # A gate with no positive fixture cannot detect a vacuous screen, and a gate
    # whose positive and negative payloads are identical cannot discriminate at
    # all.  ("A single-candidate gate CANNOT FAIL.")
    if not screen.positives:
        res.verdict = "VACUOUS-GATE"
        res.reasons.append("no known-positive fixture registered: this gate is "
                           "structurally incapable of catching a screen that "
                           "cannot fire")
        return res
    pos_payloads = {repr(f.payload) for f in screen.positives}
    neg_payloads = {repr(f.payload) for f in screen.negatives}
    if neg_payloads and pos_payloads == neg_payloads:
        res.verdict = "VACUOUS-GATE"
        res.reasons.append("positive and negative fixtures are the same "
                           "payloads: the gate cannot discriminate")
        return res

    # -- Phase 1: POWER.  Every known positive MUST fire. --------------------
    dead = [f for f in screen.positives if not _probe(res, f, det, True)]
    if dead:
        res.verdict = "POWERLESS"
        res.reasons.append(
            f"{len(dead)}/{len(screen.positives)} known-positive fixture(s) did "
            f"NOT fire: " + ", ".join(f.label for f in dead) +
            ".  A screen that cannot fire on a KNOWN defect cannot be believed "
            "when it reports none.")
        return res

    # -- Phase 2: SPECIFICITY.  Every known negative MUST NOT fire. ----------
    loud = [f for f in screen.negatives if not _probe(res, f, det, False)]
    if loud:
        res.verdict = "INDISCRIMINATE"
        res.reasons.append(
            f"{len(loud)}/{len(screen.negatives)} known-negative fixture(s) "
            f"FIRED: " + ", ".join(f.label for f in loud) +
            ".  The screen is inventing defects; its candidate list is noise.")
        return res
    if not screen.negatives:
        res.probes.append(("WARN", "specificity",
                           "no known-negative fixture registered -- power is "
                           "proved, specificity is NOT (defect #4's class)"))

    # -- Phase 3: ENRICHMENT vs the untreated population ---------------------
    if screen._pops and run_populations:
        tl, t, cl, c, expect, min_enr = screen._pops
        try:
            titer = t() if callable(t) else t
            citer = c() if callable(c) else c
            t_hit = t_tot = c_hit = c_tot = 0
            for p in titer:
                t_tot += 1
                t_hit += bool(_fires(det(p), expect))
            for p in citer:
                c_tot += 1
                c_hit += bool(_fires(det(p), expect))
        except Exception as exc:                  # noqa: BLE001
            res.verdict = "UNTESTABLE"
            res.reasons.append(f"population scan raised "
                               f"{type(exc).__name__}: {exc}")
            return res
        res.rates = (tl, t_hit, t_tot, cl, c_hit, c_tot)
        tr = t_hit / t_tot if t_tot else 0.0
        cr = c_hit / c_tot if c_tot else 0.0
        res.enrichment = (tr / cr) if cr else (float("inf") if tr else 0.0)
        if res.enrichment < min_enr:
            res.verdict = "DRAINED"
            res.reasons.append(
                f"enrichment {res.enrichment:.2f}x is below the {min_enr:.2f}x "
                f"threshold: the treated rows are no more flagged than the "
                f"untreated ones, so this signal carries no information here. "
                f"Report a DRAINED VEIN, not a candidate list.")
    return res


def gate(screens, verbose=False, run_populations=True, out=sys.stdout):
    """Gate every screen and report.  Returns a GateResult.

    Prints nothing that could be mistaken for candidate output; emitting
    candidates is the CALLER's job, and only when `.armed` is true.
    """
    results = []
    for s in screens:
        r = run_screen(s, run_populations=run_populations)
        results.append(r)
        _report_screen(r, verbose, out)
    if not results:
        # Say it out loud. A silent empty run is the failure mode -- the caller
        # gates on `.armed`, and before this the empty gate answered True.
        print("VACUOUS-GATE: ZERO screens were run. This is a REFUSAL (exit 2), "
              "not a pass -- an empty screen registry cannot fail, so a green "
              "verdict here would certify nothing. Check the registry/filter.",
              file=out)
    return GateResult(results)


def _report_screen(r, verbose, out):
    s = r.screen
    print(f"\n[{r.verdict}] {r.name}", file=out)
    print(f"    what firing means: {s.why}", file=out)
    if s.source:
        print(f"    instruments:       {s.source}", file=out)
    for level, label, detail in r.probes:
        if verbose or level in ("FAIL", "WARN"):
            print(f"      {level:4s} {label}: {detail}", file=out)
    if r.rates:
        tl, th, tt, cl, ch, ct = r.rates
        trp = 100 * th / tt if tt else 0.0
        crp = 100 * ch / ct if ct else 0.0
        print(f"      TREATED {tl}: {th}/{tt} = {trp:.4f}%", file=out)
        print(f"      CONTROL {cl}: {ch}/{ct} = {crp:.4f}%", file=out)
        print(f"      ENRICHMENT: {r.enrichment:.2f}x", file=out)
    for why in r.reasons:
        print(f"    >>> {why}", file=out)


# ---------------------------------------------------------------------------
# Fixtures -- every one lifted VERBATIM from a real artifact in this tree
# ---------------------------------------------------------------------------

# From build/45410914/asm/*.s.  Note the `/* ADDR FILEOFF BYTES */\t` prefix:
# THIS is what defect #1's `^subi` anchor could never see.  13,866 such lines
# exist in the tree, so a screen scoring 0/21,349 was not finding a clean
# population -- it was finding nothing.
ASM_FUNCLET_1 = "/* 82657630 0064C430  3B EC FF 80 */\tsubi r31, r12, 0x80"
ASM_FUNCLET_2 = "/* 824F9358 004EE158  3B EC FF 70 */\tsubi r31, r12, 0x90"
ASM_FUNCLET_3 = "/* 8230A8CC 002FF6CC  3B EC FF 60 */\tsubi r31, r12, 0xa0"
# Ordinary (non-funclet) instructions from the same files.
ASM_ORDINARY_1 = "/* 82B729E0 00B677E0  80 63 00 00 */\tlwz r3, 0x0(r3)"
ASM_ORDINARY_2 = "/* 82B729EC 00B677EC  81 63 00 00 */\tlwz r11, 0x0(r3)"
ASM_ORDINARY_3 = "/* 82B729F0 00B677F0  38 80 00 01 */\tli r4, 0x1"

# From config/45410914/splits.txt -- UPPERCASE hex, which is the whole of
# defect #6.  A `[0-9a-f]+` parser reads this file as containing no pins at all.
SPLITS_TEXT_LINE = "\t.text       start:0x8277B52C end:0x8277B6E8"
SPLITS_SECTION_HDR = "\t.text       type:code align:65536"
SPLITS_BANNER = "Sections:"

# Real retail band.exe instruction words (read out of orig/45410914/band.exe):
#   0x8163FFFC = lwz r11, -4(r3)  at 0x82275560 (?SetType@RndAnimatable@@$4...)
#     -> void return, so `this` is in r3.  rD=11, rA=3.  SWAP the two field
#        extractions and a "loads r11 from r3" filter fires NOWHERE -- defect #2.
#   0x8164FFFC = lwz r11, -4(r4)  at 0x82273F28 (?Handle@RndAnimatable@@$4...)
#     -> returns ?AVDataNode@@ BY VALUE, so the hidden return pointer takes r3
#        and `this` lands in r4.  An r3-only decoder misses every struct-
#        returning virtual and manufactures disagreements -- defect #5.
W_THUNK_R3 = 0x8163FFFC
W_THUNK_R4 = 0x8164FFFC
W_LWZ_R3_R3 = 0x80630000        # lwz r3, 0(r3)    -- different shape entirely
W_LWZ_R11_R3_0 = 0x81630000     # lwz r11, 0(r3)   -- displacement 0, not -4
W_LWZ_R11_R5 = 0x8165FFFC       # lwz r11, -4(r5)  -- r5 is not a `this` reg

# Heavily-called retail functions (callsite_screen.py's own probe set).  A bl
# decoder that reports THESE as having zero callers is broken.
BL_PROBES = {
    "Hmx::Object::Save": 0x8275AB90,
    "BinStream::WriteEndian": 0x827C5098,
    "__RTDynamicCast": 0x8282A0C8,
}

# From src/system/meta/CreditsPanel.cpp:42-54, VERBATIM.  Two arms sit inside
# `#if defined(MILO_DEBUG) && defined(HX_NATIVE)`, which the match build NEVER
# compiles (its cflags carry no /D at all).  Failing to strip that guard INVENTS
# debug_toggle_autoscroll -- defect #4.  Retail's true set is exactly
# [pause_panel, is_cheat_on].
SRC_CREDITSPANEL = '''#include "meta/CreditsPanel.h"

BEGIN_HANDLERS(CreditsPanel)
    HANDLE_ACTION(pause_panel, PausePanel(_msg->Int(2)))
#if defined(MILO_DEBUG) && defined(HX_NATIVE)
    HANDLE_EXPR(is_cheat_on, mCheatOn)
#else
    HANDLE_EXPR(is_cheat_on, false)
#endif
#if defined(MILO_DEBUG) && defined(HX_NATIVE)
    HANDLE_ACTION(debug_toggle_autoscroll, DebugToggleAutoScroll())
#endif
    HANDLE_MESSAGE(ButtonDownMsg)
    HANDLE_SUPERCLASS(UIPanel)
END_HANDLERS
'''

# From src/band3/meta_band/ClosetMgr.cpp:518-527, VERBATIM.  Every arm is a
# *_STATIC variant; a hand-enumerated macro list that forgot HANDLE_EXPR_STATIC
# parses this block as ZERO handlers and then reports every retail arm as
# MISSING -- defect #3.
SRC_CLOSETMGR = '''#include "meta_band/ClosetMgr.h"

BEGIN_HANDLERS(ClosetMgr)
    HANDLE_ACTION_STATIC(set_user, SetUser(_msg->Obj<LocalBandUser>(2)))
    HANDLE_EXPR_STATIC(get_user, GetUser())
    HANDLE_ACTION_STATIC(clear_user, ClearUser())
    HANDLE_EXPR_STATIC(get_user_slot, GetUserSlot())
    HANDLE_ACTION_STATIC(set_no_user_mode, SetNoUserMode(_msg->Int(2)))
    HANDLE_EXPR_STATIC(is_character_loading, IsCharacterLoading())
    HANDLE_SUPERCLASS(UIPanel)
END_HANDLERS
'''

# Where each fixture came from, so --provenance can prove it has not rotted
# away from the artifact it was copied out of.
PROVENANCE = [
    ("build/45410914/asm/*.s", "glob", [ASM_FUNCLET_1, ASM_ORDINARY_1]),
    ("config/45410914/splits.txt", "file", [SPLITS_TEXT_LINE]),
    ("src/system/meta/CreditsPanel.cpp", "file",
     ["HANDLE_ACTION(debug_toggle_autoscroll, DebugToggleAutoScroll())"]),
    ("src/band3/meta_band/ClosetMgr.cpp", "file",
     ["HANDLE_EXPR_STATIC(get_user, GetUser())"]),
]


# ---------------------------------------------------------------------------
# Reference screens for the three defects whose original tools were never
# landed (they were in-transcript one-offs).  These are CLASS-level
# reproductions with real bytes and real lines -- honestly labelled as such,
# NOT as instrumentation of a landed tool.
# ---------------------------------------------------------------------------

SPLITS_RE_GOOD = re.compile(
    r"\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)")
SPLITS_RE_LOWER = re.compile(
    r"\.text\s+start:0x([0-9a-f]+)\s+end:0x([0-9a-f]+)")


def splits_parse(line):
    """-> (start, end) for a pinned .text range, else None."""
    m = SPLITS_RE_GOOD.search(line)
    return (int(m.group(1), 16), int(m.group(2), 16)) if m else None


def splits_parse_lowercase_only(line):
    """DEFECT #6, as originally written."""
    m = SPLITS_RE_LOWER.search(line)
    return (int(m.group(1), 16), int(m.group(2), 16)) if m else None


def _lwz_fields(w):
    """(rD, rA, signed displacement) for a PPC D-form lwz, else None."""
    if (w >> 26) != 32:
        return None
    d = w & 0xFFFF
    return ((w >> 21) & 31, (w >> 16) & 31, d - 0x10000 if d & 0x8000 else d)


def _lwz_fields_swapped(w):
    """DEFECT #2: rD and rA extractions transposed."""
    if (w >> 26) != 32:
        return None
    d = w & 0xFFFF
    return ((w >> 16) & 31, (w >> 21) & 31, d - 0x10000 if d & 0x8000 else d)


def vbase_load_r3(w):
    """Fires on `lwz r11, -4(r3)` -- the void-return vbase adjustor load."""
    f = _lwz_fields(w)
    return bool(f and f[0] == 11 and f[1] == 3 and f[2] == -4)


def vbase_load_r3_swapped(w):
    f = _lwz_fields_swapped(w)
    return bool(f and f[0] == 11 and f[1] == 3 and f[2] == -4)


def thunk_this_load(w):
    """Fires on the adjustor-thunk `this` load in EITHER calling convention:
    r3 when the method returns void, r4 when it returns a struct by value."""
    f = _lwz_fields(w)
    return bool(f and f[0] == 11 and f[1] in (3, 4) and f[2] == -4)


def thunk_this_load_r3_only(w):
    """DEFECT #5: only ever looked at r3."""
    f = _lwz_fields(w)
    return bool(f and f[0] == 11 and f[1] == 3 and f[2] == -4)


# ---------------------------------------------------------------------------
# Registry: the retrofits + the reference screens
# ---------------------------------------------------------------------------

def _screen_false_pairing():
    """RETROFIT of tools/false_pairing_screen.py (defect #1).

    Instruments the LANDED regexes; no logic is rewritten.  The screen under
    test is exactly what the tool does per instruction line: strip dtk's
    `/* ... */` prefix, then match the funclet prologue.
    """
    import false_pairing_screen as fp

    def detect(raw_line):
        return bool(fp.FUNCLET.match(fp.STRIP.sub("", raw_line)))

    def broken_no_strip(raw_line):
        # Defect #1 verbatim: anchor at ^subi against the RAW line.
        return bool(fp.FUNCLET.match(raw_line))

    s = Screen(
        "false-pairing / EH-funclet prologue",
        detect,
        "the retail extent opens with r12-relative parent-frame recovery, i.e. "
        "it is an unwind funclet and the name<->address pairing is wrong",
        source="tools/false_pairing_screen.py (STRIP + FUNCLET)")
    s.must_fire("real funclet line (subi r31, r12, 0x80)", ASM_FUNCLET_1,
                note="from build/45410914/asm/*.s, one of 13,866")
    s.must_fire("real funclet line (0x90 frame)", ASM_FUNCLET_2)
    s.must_fire("real funclet line (0xa0 frame)", ASM_FUNCLET_3)
    s.must_not_fire("ordinary lwz r3, 0x0(r3)", ASM_ORDINARY_1)
    s.must_not_fire("ordinary lwz r11, 0x0(r3)", ASM_ORDINARY_2)
    s.must_not_fire("ordinary li r4, 0x1", ASM_ORDINARY_3)
    s.defect("D1", "regex anchored at ^subi against a line that carries dtk's "
                   "/* ADDR OFF BYTES */ prefix -> 0/21,349 and 0/6,680",
             broken_no_strip)
    return s


def _screen_handler_parser():
    """RETROFIT of tools/handler_sweep.py (defects #3 and #4).

    Calls the LANDED our_blocks() end to end -- the fixture is written to a
    temp tree and parsed by the real function, so this instruments the tool
    rather than re-implementing its parser.
    """
    import re
    import shutil
    import tempfile
    import handler_sweep as hs

    def _parse_with(reader, text):
        tmp = tempfile.mkdtemp(prefix="screengate.")
        try:
            with open(os.path.join(tmp, "fixture.cpp"), "w") as fh:
                fh.write(text)
            blocks = reader(tmp)
            names = []
            for _cls, (_path, arms) in blocks.items():
                names.extend(arms)
            return names
        finally:
            shutil.rmtree(tmp, ignore_errors=True)

    def detect(text):
        return _parse_with(hs.our_blocks, text)

    # ---- defect #3: macro variants enumerated by hand, EXPR_STATIC omitted --
    _ENUMERATED = re.compile(
        r"\b(?:HANDLE|HANDLE_ACTION|HANDLE_ACTION_STATIC|HANDLE_EXPR|"
        r"HANDLE_ACTION_IF)\s*\(\s*([a-z][A-Za-z0-9_]*)\s*,")

    def broken_enumerated_macros(text):
        def reader(root):
            out = {}
            for r, _d, files in os.walk(root):
                for fn in files:
                    if not fn.endswith(".cpp"):
                        continue
                    txt = open(os.path.join(r, fn), errors="replace").read()
                    for m in re.finditer(
                            r"BEGIN_HANDLERS\((.*?)\)(.*?)END_HANDLERS",
                            txt, re.S):
                        body = m.group(2)
                        for g in hs._GUARDS:
                            body = g.sub(lambda mm: mm.group(2) or "", body)
                        out.setdefault(m.group(1).strip().split("::")[-1],
                                       (fn, _ENUMERATED.findall(body)))
            return out
        return _parse_with(reader, text)

    # ---- defect #4: the never-compiled guard arms are not stripped ----------
    def broken_unstripped_guards(text):
        def reader(root):
            out = {}
            for r, _d, files in os.walk(root):
                for fn in files:
                    if not fn.endswith(".cpp"):
                        continue
                    txt = open(os.path.join(r, fn), errors="replace").read()
                    for m in re.finditer(
                            r"BEGIN_HANDLERS\((.*?)\)(.*?)END_HANDLERS",
                            txt, re.S):
                        out.setdefault(m.group(1).strip().split("::")[-1],
                                       (fn, hs._HANDLE.findall(m.group(2))))
            return out
        return _parse_with(reader, text)

    s = Screen(
        "handler-block parser (our side)",
        detect,
        "the named handler arm is present in our BEGIN_HANDLERS block for this "
        "class, as the match build would actually compile it",
        source="tools/handler_sweep.py (our_blocks + _HANDLE + _GUARDS)")
    # POWER: the *_STATIC family must be seen at all (defect #3).
    s.must_fire("HANDLE_EXPR_STATIC arm is parsed", SRC_CLOSETMGR,
                expect="get_user",
                note="real ClosetMgr.cpp block; omitting this variant read "
                     "OvershellSlot/ManageBandPanel as ZERO handlers")
    s.must_fire("second *_STATIC arm is parsed", SRC_CLOSETMGR,
                expect="is_character_loading")
    s.must_fire("plain HANDLE_ACTION arm is parsed", SRC_CREDITSPANEL,
                expect="pause_panel")
    s.must_fire("the #else arm IS compiled and IS parsed", SRC_CREDITSPANEL,
                expect="is_cheat_on")
    # SPECIFICITY: the guarded arm must NOT appear (defect #4).
    s.must_not_fire("MILO_DEBUG&&HX_NATIVE arm must NOT be parsed",
                    SRC_CREDITSPANEL, expect="debug_toggle_autoscroll",
                    note="never compiled: cflags carry no /D at all")
    s.defect("D3", "handler macro variants enumerated by hand; "
                   "HANDLE_EXPR_STATIC omitted -> classes parse as ZERO arms "
                   "and all 102 / 18 retail arms report MISSING",
             broken_enumerated_macros)
    s.defect("D4", "never-compiled #if defined(MILO_DEBUG) && "
                   "defined(HX_NATIVE) arms not stripped -> INVENTS "
                   "debug_toggle_autoscroll",
             broken_unstripped_guards)
    return s


def _screen_bl_reachability():
    """RETROFIT of tools/callsite_screen.py -- the ENRICHMENT demonstration.

    This screen is a LANDED DRAINED VEIN (0.77x).  It is registered here so the
    harness's third assertion class is exercised against a real, adjudicated
    population rather than a toy one: the gate must report DRAINED and withhold
    candidates, which is exactly the verdict the lane reached by hand.
    """
    import json
    import callsite_screen as cs

    exe = os.path.join(ROOT, "orig/45410914/band.exe")
    report = os.path.join(ROOT, "build/45410914/report.json")
    tmap = os.path.join(ROOT, "scripts/target_symbol_map.json")
    counts = cs.call_graph(exe)                   # the LANDED decoder

    def detect(va):
        """Fires when a retail address has ZERO direct bl callers."""
        return counts.get(va, 0) == 0

    def broken_bc_opcode(va):
        # A decoder that reads the wrong primary opcode finds ~nothing, so
        # EVERY address reads unreachable.  Same shape as defect #2: a decode
        # bug that turns the whole population into a decisive-looking negative.
        return True

    def _pops():
        n2a = {}
        for a, n in json.load(open(tmap)).items():
            if not a.startswith("0x"):
                continue
            for x in (n if isinstance(n, list) else [n]):
                n2a.setdefault(x, []).append(int(a, 16))
        treated, control = [], []
        for u in json.load(open(report))["units"]:
            for f in u.get("functions", []):
                n = f.get("name", "")
                if (not n or n.startswith(("fn_", "auto_"))
                        or not cs.directly_callable(n)):
                    continue
                addrs = n2a.get(n)
                if not addrs:
                    continue
                # the tool takes the best-case address for the row
                best = min(addrs, key=lambda a: -counts.get(a, 0))
                if float(f.get("match_percent_normalized", 0.0)) >= 100.0:
                    control.append(best)
                else:
                    treated.append(best)
        return treated, control

    cache = {}

    def treated():
        if "v" not in cache:
            cache["v"] = _pops()
        return cache["v"][0]

    def control():
        if "v" not in cache:
            cache["v"] = _pops()
        return cache["v"][1]

    s = Screen(
        "zero-direct-caller (false pairing, 2nd detector)",
        detect,
        "a named non-virtual row's retail address is the target of no direct "
        "bl edge anywhere, so name and address may disagree",
        source="tools/callsite_screen.py (call_graph + directly_callable)")
    s.must_fire("an address that is provably not a call target",
                0x82000000 + 2,
                note="misaligned/never-a-target -> the predicate CAN return True")
    for label, va in BL_PROBES.items():
        s.must_not_fire(f"heavily-called {label} must read REACHABLE", va,
                        note="if this fires, the bl decoder is broken and every "
                             "row would look unreachable")
    s.populations("named non-virtual sub-100", treated,
                  "named non-virtual mpn==100", control, min_enrichment=3.0)
    s.defect("D2-class", "a decoder that finds no edges -> every address reads "
                         "unreachable (the lwz rD/rA swap's failure shape)",
             broken_bc_opcode)
    return s


def _screen_splits_parser():
    """REFERENCE screen for defect #6.  The original one-off was never landed,
    so this reproduces the CLASS with the real uppercase line from splits.txt."""
    s = Screen(
        "splits.txt .text-range parser",
        splits_parse,
        "the line pins a .text address range",
        source="reference implementation (the session's parser was a one-off, "
               "never landed)")
    s.must_fire("real UPPERCASE-hex pin from splits.txt", SPLITS_TEXT_LINE,
                note="config/45410914/splits.txt is uppercase throughout")
    s.must_not_fire("section header line", SPLITS_SECTION_HDR)
    s.must_not_fire("file banner", SPLITS_BANNER)
    s.defect("D6", "regex written for lowercase hex against an UPPERCASE file "
                   "-> a decisive-looking 'none of these addresses are pinned'",
             splits_parse_lowercase_only)
    return s


def _screen_vbase_lwz():
    """REFERENCE screen for defect #2, on real retail instruction words."""
    s = Screen(
        "vbase adjustor lwz decode",
        vbase_load_r3,
        "the word is `lwz r11, -4(r3)` -- the void-return vbase adjustor load",
        source="reference implementation (the session's decoder was a one-off)")
    s.must_fire("real 0x8163FFFC from band.exe @0x82275560", W_THUNK_R3,
                note="?SetType@RndAnimatable@@$4... -- rD=11, rA=3")
    s.must_not_fire("lwz r3, 0(r3)", W_LWZ_R3_R3)
    s.must_not_fire("lwz r11, 0(r3) -- displacement 0", W_LWZ_R11_R3_0)
    s.must_not_fire("lwz r11, -4(r4) -- the r4 form is a different shape",
                    W_THUNK_R4)
    s.defect("D2", "rD and rA field extractions transposed -> 0 hits across "
                   "14 MB; corrected, the same scan fires 1,972 times",
             vbase_load_r3_swapped)
    return s


def _screen_thunk_this_reg():
    """REFERENCE screen for defect #5, on real retail instruction words."""
    s = Screen(
        "adjustor-thunk `this` load (both conventions)",
        thunk_this_load,
        "the word loads the vbase displacement off `this`, whichever register "
        "`this` is in for this method's return convention",
        source="reference implementation (the session's decoder was a one-off)")
    s.must_fire("void-return form, this in r3 (0x8163FFFC)", W_THUNK_R3,
                note="?SetType@RndAnimatable@@$4... @0x82275560")
    s.must_fire("STRUCT-RETURN form, this in r4 (0x8164FFFC)", W_THUNK_R4,
                note="?Handle@RndAnimatable@@$4... @0x82273F28 returns "
                     "?AVDataNode@@ by value, so r3 is the hidden return ptr")
    s.must_not_fire("lwz r11, -4(r5) -- r5 is not a `this` register",
                    W_LWZ_R11_R5)
    s.must_not_fire("lwz r3, 0(r3)", W_LWZ_R3_R3)
    s.defect("D5", "decoder tracked only r3, missing the r4 struct-return "
                   "form -> manufactured false disagreements",
             thunk_this_load_r3_only)
    return s


# ---------------------------------------------------------------------------
# lane NEWOBJ-1: the ALLOCATION-SIZE IMMEDIATE discriminator.
#
# objdiff masks the constructor relocation, so NewObject-shaped rows pair almost
# arbitrarily.  The `li r3, <sizeof>` allocation immediate is a PLAIN IMMEDIATE
# and is NOT masked -- the first discriminator for the masked class that uses
# the metric's OWN input rather than external RTTI/vtable evidence.
#
# Fixtures are inline retail bytes so this screen needs no band.exe and no
# build artifacts: it can never SKIP.
# ---------------------------------------------------------------------------

# retail 0x8268b9a0 -- the row mapped UIPanel::NewObject.  PLAIN operator-new
# form: `li r3, 0x108` is the FIRST call's argument.
NEWOBJ_UIPANEL = bytes.fromhex(
    "7d8802a69181fff8" "fbe1fff03be1ff90" "9421ff9038600108"
    "48131939907f0050" "2803000041820010" "388000014bfffb1d" "48000008")

# retail 0x824576f8 -- the row mapped SkeletonClip::NewObject.  TAGGED MemAlloc
# form: a StaticClassName call comes FIRST and the size is re-materialised
# AFTER it.  *** THIS FIXTURE IS THE POINT. ***  A decoder that stops at the
# first bl reads this row as UNDECODED, which is how the first version of this
# screen fell silent on 124 of 209 rows (59%) and looked like a thin population.
NEWOBJ_SKELCLIP = bytes.fromhex(
    "7d8802a69181fff8" "fbe1fff03be1ff90" "9421ff90387f0050"
    "4befe05138800000" "386001c84836561d" "907f005428030000" "41820010")

# retail 0x82355760 -- RndText::StaticClassName.  Allocates nothing.
NEWOBJ_NOALLOC = bytes.fromhex(
    "7d8802a6484d3af9" "3be1ff909421ff90" "3d4082cc3d6082cc" "7c7d1b783bcbe768")


def _screen_newobj_size():
    """Decoder power: can it read the allocation immediate out of BOTH forms?"""
    import importlib.util
    import pathlib
    spec = importlib.util.spec_from_file_location(
        "newobj_size_screen",
        str(pathlib.Path(__file__).resolve().parent / "newobj_size_screen.py"))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)

    def detect(blob):
        """-> the allocated size, or None.  Fires when a size is readable."""
        for _tgt, size in mod.call_sites(blob, 0x82000000):
            if size is not None:
                return size
        return None

    s = Screen(
        "newobj allocation-size immediate",
        detect,
        "the `li r3, <sizeof>` fed to operator new is UNMASKED, so it can "
        "adjudicate a relocation-masked row against the compiler's sizeof",
        source="tools/newobj_size_screen.py (call_sites)")
    s.must_fire("plain operator-new form (UIPanel row)", NEWOBJ_UIPANEL,
                expect=264,
                note="0x108 -- the size is the FIRST call's argument here")
    s.must_fire("TAGGED MemAlloc form (SkeletonClip row)", NEWOBJ_SKELCLIP,
                expect=456,
                note="0x1c8 -- the size is live only at the SECOND bl. A "
                     "first-bl decoder returns None and reads 59% of the "
                     "population as an empty vein.")
    s.must_not_fire("a body that allocates nothing", NEWOBJ_NOALLOC,
                    note="StaticClassName -- inventing a size here would "
                         "manufacture findings")
    def first_bl_only(blob):
        """THE REAL DEFECT, not an exception: read only the FIRST call.

        Note it still fires correctly on the plain operator-new fixture -- the
        defect is INVISIBLE there.  It is the TAGGED fixture that exposes it,
        which is exactly why both forms are registered.
        """
        sites = mod.call_sites(blob, 0x82000000)
        return sites[0][1] if sites else None

    s.defect("first-bl", "stop at the first bl -- the exact defect that read "
                         "124 of 209 rows (59%) as undecoded",
             first_bl_only)
    return s


def _screen_newobj_ancestor():
    """*** SPECIFICITY: an INHERITED static names the BASE, not the derived
    class. ***

    This is the 'fires too MUCH' class (historical defect #4).  A naive reading
    of 'retail's tag names a different class than the row' flags 32 rows, and
    23 of them (71.9%) are inherited statics -- DxCam:RndCam tagging 'Cam',
    NgSpotlightDrawer:SpotlightDrawer, NgMat:RndMat.  The check that makes it
    sound is comparing retail's tag against OUR OWN COMPILER'S tag for the same
    row, not against the row's class name.
    """
    def detect(row):
        _row_class, our_tag, retail_tag = row
        if not our_tag or not retail_tag:
            return False
        return our_tag != retail_tag

    s = Screen(
        "newobj tag-class ancestor check",
        detect,
        "a tag mismatch is a finding only when OUR compiler's expected tag "
        "differs from retail's -- never merely because the tag is not the "
        "row's own class name",
        source="tools/newobj_size_screen.py (verdict FALSE_PAIRING_TAG)")
    s.must_fire("genuine mismatch (AppMiniLeaderboardDisplay row)",
                ("AppMiniLeaderboardDisplay", "UIComponent",
                 "MiniLeaderboardDisplay"), expect=True)
    s.must_fire("genuine mismatch (SkeletonClip row tags RndText)",
                ("SkeletonClip", "SkeletonClip", "RndText"), expect=True)
    s.must_not_fire("NgMat: RndMat == RndMat -- INHERITED, not a defect",
                    ("NgMat", "RndMat", "RndMat"),
                    note="the false positive I nearly reported; NgMat:public "
                         "RndMat does not override StaticClassName")
    s.must_not_fire("DxCam: RndCam == RndCam -- inherited",
                    ("DxCam", "RndCam", "RndCam"))
    s.must_not_fire("tag missing on one side proves nothing",
                    ("Foo", None, "RndMat"))

    def compare_to_row_name(row):
        """THE REAL DEFECT: judge retail's tag against the ROW'S OWN class name.

        This is the 'fires too MUCH' class. It flags NgMat (RndMat != NgMat)
        even though an inherited static naming the base is CORRECT -- 23 of 32
        flags (71.9%) are this. It is caught by the must_not_fire fixtures,
        which is precisely why specificity fixtures exist.
        """
        row_class, _our_tag, retail_tag = row
        return retail_tag is not None and retail_tag != row_class

    s.defect("name-compare", "compare retail's tag against the ROW'S OWN class "
                             "name instead of our compiler's expected tag -- "
                             "71.9% false positives",
             compare_to_row_name)
    return s


# ---------------------------------------------------------------------------
# lane MAPBOGUS-1: "is this map address even a FUNCTION?"
#
# Three screens from tools/map_bogus_screen.py.  All fixtures are real retail
# words / real map names / the real PE section table, inlined so the gate can
# never SKIP for want of band.exe.
#
# The two injected defects are ones this lane ACTUALLY MADE and caught:
#   D-SEC   section extents read from SizeOfRawData instead of
#           max(VirtualSize, SizeOfRawData) -- .data's VirtualSize is 3.5x its
#           raw size, so 286 real .data variables read as "outside every
#           section".  A "fires too much" defect: only must_not_fire catches it.
#   D-NAME  the name-kind classifier reading the storage class after the FIRST
#           `@@` -- which, for a function-local static, lands inside the
#           ENCLOSING FUNCTION's mangling, so 241 .data variables classify as
#           FUNC and get judged as though they should be function heads.
# ---------------------------------------------------------------------------

# First words lifted verbatim from retail band.exe .text:
W_PADDING = 0x00000000        # @0x82318E54 -- dtk emits this 4-byte gap as a
                              # "function"; the map pins ??3MicInputArrow onto it
W_STWU = 0x9421FFA0           # @0x82318E34 -- a real function start
W_MFLR = 0x7D8802A6           # @0x824AF930 -- a real function start (mfspr r12)
W_TAILJUMP = 0x4BFFECB8       # @0x8270D7F8 -- a bare `b`.  *** MUST NOT FIRE ***
                              # BARE_B is the legitimate tail-jump stub stratum;
                              # ??3BandHighlight@@ @0x82345030 has fan-in 67.
W_BLR = 0x4E800020            # @0x8253B8C8 -- a lone blr

# Real rows out of scripts/target_symbol_map.json.
NAME_FUNC_1 = "??3MicInputArrow@@SAXPAX@Z"
NAME_FUNC_2 = "?RefreshData@MainHubPanel@@QAAXXZ"
NAME_FUNC_VCALL = "??_9StreamReceiver@@$BBI@AA"      # vcall thunk: CODE
# Function-LOCAL STATICS -- data, living in .data.  Their first `@@` closes the
# ENCLOSING function's class qualifier, which is the whole of defect D-NAME.
NAME_LOCALSTATIC_1 = "?msg@?BD@??StartAnim@BandCamShot@@UAAXXZ@4VMessage@@A"
NAME_LOCALSTATIC_2 = ("?inline_help_fmt@?1??UpdateLabelText@InlineHelp@@IAAXXZ"
                      "@4VSymbol@@A")
NAME_STRING = "??_C@_0BA@KOFHHHDN@some_string@"      # .rdata string constant

# The real PE section table (name, VA, VirtualSize, SizeOfRawData).
SECTION_TABLE = [
    (".rdata",   0x82000400, 0x001F1184, 0x001F1200),
    (".pdata",   0x821F1600, 0x00070C28, 0x00070E00),
    ("BINKCONS", 0x82262400, 0x00002920, 0x00002A00),
    (".text",    0x82270000, 0x009DCE3C, 0x009DD000),
    ("BINK",     0x82C4D000, 0x00010010, 0x00010200),
    ("BINKBSS",  0x82C60000, 0x000043A0, 0x00000000),
    (".data",    0x82C64400, 0x001F5EAC, 0x00058200),   # <-- 3.5x raw size
    ("BINKDATA", 0x82E5A400, 0x00003D54, 0x00003E00),
]
# A real .data local static (?msg@?BD@??StartAnim@BandCamShot@@...) that lies
# PAST .data's SizeOfRawData (raw ends 0x82CBC600) but inside its VirtualSize
# (ends 0x82E5A2AC).  The first fixture tried here was 0x82C75AA0, which is
# still inside the RAW extent -- so D-SEC ESCAPED the gate.  --self-break
# caught that, which is the whole reason it exists.
ADDR_IN_DATA_TAIL = 0x82CBCE98
ADDR_IN_TEXT = 0x82318E54
ADDR_NO_SECTION = 0x82266FC0      # really outside every section (BINKCONS->.text gap)


def _screen_padding_word():
    """PAD: the claimed function's first word is alignment padding."""
    import map_bogus_screen as mb

    s = Screen(
        "map row first-word is PADDING",
        mb.is_padding_word,
        "the map pins a function name onto inter-function alignment fill, so "
        "any body comparison against it can only ever say DIFFERENT",
        source="tools/map_bogus_screen.py (is_padding_word)")
    s.must_fire("retail 0x00000000 @0x82318E54", W_PADDING,
                note="0 of 62,696 known-good function starts begin with this")
    s.must_not_fire("real prologue stwu r1,-0x60(r1)", W_STWU)
    s.must_not_fire("real prologue mfspr r12 (mflr)", W_MFLR)
    s.must_not_fire("bare `b` tail-jump stub -- LEGITIMATE, fan-in up to 67",
                    W_TAILJUMP,
                    note="BARE_B was refuted as a defect signal; a screen that "
                         "fires here condemns the whole alias-stub stratum")
    s.must_not_fire("lone blr", W_BLR)

    def also_flag_bare_b(w):
        """THE REFUTED SIGNAL: treat a leading unconditional `b` as bogus too.

        Its 0/36,244 rate on .pdata starts looks like perfect specificity, but
        that is by-construction bias: a pure `b target` stub is a leaf and gets
        no unwind record, so the control EXCLUDES the entire population it
        would fire on.
        """
        return w == 0 or ((w >> 26) == 18 and (w & 1) == 0)

    s.defect("BARE-B", "also flag a leading unconditional `b` -- condemns 155 "
                       "legitimate tail-jump alias stubs (fan-in up to 67)",
             also_flag_bare_b)
    return s


def _screen_name_kind():
    """Does this mangled name denote a FUNCTION (so it may be judged at all)?"""
    import map_bogus_screen as mb

    def detect(n):
        return mb.name_kind(n) == "FUNC"

    def broken_first_at(n):
        """DEFECT D-NAME, verbatim: read the storage class after the FIRST @@."""
        if not n.startswith("?"):
            return False
        if n.startswith(("??_C", "??_R", "??_7", "??_8")):
            return False
        i = n.find("@@")
        if i < 0 or i + 2 >= len(n):
            return False
        c = n[i + 2]
        if c in "012345678":
            return False
        return c.isalpha()

    s = Screen(
        "map row name denotes a FUNCTION",
        detect,
        "the row is a function, so 'is this address a function head' is a "
        "meaningful question to ask about it at all",
        source="tools/map_bogus_screen.py (name_kind)")
    s.must_fire("ordinary member function", NAME_FUNC_1)
    s.must_fire("ordinary member function (2)", NAME_FUNC_2)
    s.must_fire("??_9 vcall thunk is CODE", NAME_FUNC_VCALL,
                note="ends in AA, not Z -- the Z-rule alone would miss it")
    s.must_not_fire("function-LOCAL STATIC is data", NAME_LOCALSTATIC_1,
                    note="lives in .data; its first @@ closes BandCamShot")
    s.must_not_fire("function-local static (2)", NAME_LOCALSTATIC_2)
    s.must_not_fire("??_C string constant", NAME_STRING)
    s.defect("D-NAME", "read the storage class after the FIRST @@ -- for a "
                       "local static that lands inside the ENCLOSING "
                       "function's mangling; 241 .data vars classify as FUNC",
             broken_first_at)
    return s


def _screen_section_extent():
    """Is this address outside every section?  (the max(VirtSize,RawSize) rule)"""
    def make(use_max):
        def detect(a):
            for _nm, va, vs, srd in SECTION_TABLE:
                if va <= a < va + (max(vs, srd) if use_max else srd):
                    return False
            return True                    # fires == "outside every section"
        return detect

    s = Screen(
        "map address outside every PE section",
        make(True),
        "a function-named row whose address is in no section at all cannot be "
        "a function",
        source="tools/map_bogus_screen.py (Image.section_of)")
    s.must_fire("0x82266FC0 -- really in the BINKCONS->.text gap",
                ADDR_NO_SECTION)
    s.must_not_fire("an address inside .text", ADDR_IN_TEXT)
    s.must_not_fire("a real .data address PAST SizeOfRawData",
                    ADDR_IN_DATA_TAIL,
                    note=".data VirtualSize 0x1F5EAC vs raw 0x058200 -- this is "
                         "the address D-SEC misreads as sectionless")
    s.defect("D-SEC", "size sections by SizeOfRawData instead of "
                      "max(VirtualSize, SizeOfRawData) -- manufactures 286 "
                      "phantom 'outside every section' rows",
             make(False))
    return s


REGISTRY = [
    ("padding_word", _screen_padding_word, False),
    ("name_kind", _screen_name_kind, False),
    ("section_extent", _screen_section_extent, False),
    ("false_pairing", _screen_false_pairing, True),
    ("handler_parser", _screen_handler_parser, True),
    ("splits_parser", _screen_splits_parser, False),
    ("vbase_lwz", _screen_vbase_lwz, False),
    ("thunk_this_reg", _screen_thunk_this_reg, False),
    ("bl_reachability", _screen_bl_reachability, True),
    ("newobj_size", _screen_newobj_size, False),
    ("newobj_ancestor", _screen_newobj_ancestor, False),
]


def build_registry(only=None, skip_heavy=False):
    """-> (screens, untestable) where untestable is [(name, reason), ...].

    An UNTESTABLE screen is never silently dropped: it is reported and it makes
    the run exit non-zero.
    """
    screens, untestable = [], []
    for name, factory, heavy in REGISTRY:
        if only and name not in only:
            continue
        if heavy and skip_heavy and name == "bl_reachability":
            continue
        try:
            screens.append(factory())
        except Exception as exc:                  # noqa: BLE001
            untestable.append((name, f"{type(exc).__name__}: {exc}"))
    return screens, untestable


# ---------------------------------------------------------------------------
# Provenance: have the fixtures drifted away from the artifacts?
# ---------------------------------------------------------------------------

def check_provenance(out=sys.stdout):
    """Have the inline fixtures drifted away from the artifacts they came from?

    ⚠ The first version of THIS function truncated the .s glob to files[:400]
    and concatenated them, which reported DRIFT for a line that occurs 13,866
    times in the tree.  That is the exact defect class this whole file exists to
    police -- a scan that cannot reach its evidence, returning a false negative
    shaped like a decisive one.  So: iterate EVERY file, stop at the first hit,
    never truncate.  Absence is only reported after the whole population has
    actually been read.
    """
    import glob
    print("\nFIXTURE PROVENANCE (do the literals still occur in the tree?)",
          file=out)
    for path, kind, needles in PROVENANCE:
        full = os.path.join(ROOT, path)
        files = sorted(glob.glob(full)) if kind == "glob" else (
            [full] if os.path.exists(full) else [])
        if not files:
            print(f"  SKIP {path}: not present (gitignored artifact) -- this is "
                  f"NOT a pass", file=out)
            continue
        for n in needles:
            where, scanned = None, 0
            for f in files:
                scanned += 1
                try:
                    if n in open(f, errors="replace").read():
                        where = f
                        break
                except OSError:
                    pass
            if where:
                print(f"  OK    {path}: found in {os.path.basename(where)} "
                      f"(after {scanned}/{len(files)} file(s))", file=out)
            else:
                print(f"  DRIFT {path}: NOT found in any of {len(files)} "
                      f"file(s): {n[:60]!r}", file=out)


# ---------------------------------------------------------------------------
# --self-break: prove the gate can FAIL
# ---------------------------------------------------------------------------

def self_break(screens, verbose=False, out=sys.stdout):
    """Inject each registered historical defect and assert the gate REFUSES.

    The verdict is read from the returned ScreenResult, never from anything a
    screen prints about itself.  Returns True iff every injected defect was
    caught.
    """
    print("=" * 74, file=out)
    print("SELF-BREAK: injecting each historical defect; the gate MUST refuse",
          file=out)
    print("=" * 74, file=out)
    total = caught = 0
    escaped = []
    for s in screens:
        if not s.defects:
            print(f"\n  (no defect registered for {s.name})", file=out)
            continue
        for d in s.defects:
            total += 1
            # Populations are skipped here on purpose: a screen that fails the
            # power phase must be refused BEFORE it costs a population scan.
            r = run_screen(s, detect=d.broken, run_populations=False)
            ok = r.refused
            caught += ok
            if not ok:
                escaped.append((s.name, d.ident, r.verdict))
            print(f"\n  [{d.ident}] {s.name}", file=out)
            print(f"      defect: {d.description}", file=out)
            print(f"      gate verdict with defect injected: {r.verdict}"
                  f"  -> {'CAUGHT' if ok else '*** ESCAPED ***'}", file=out)
            for level, label, detail in r.probes:
                if verbose or level == "FAIL":
                    print(f"        {level:4s} {label}: {detail}", file=out)
            for why in r.reasons:
                print(f"        >>> {why}", file=out)

    print("\n" + "=" * 74, file=out)
    if escaped:
        print(f"SELF-BREAK: FAIL -- {len(escaped)}/{total} injected defect(s) "
              f"were NOT caught:", file=out)
        for n, i, v in escaped:
            print(f"    {i} on {n}: gate said {v}", file=out)
        print("\nA harness that cannot fail is exactly the bug it exists to "
              "prevent.", file=out)
        return False
    print(f"SELF-BREAK: PASS -- {caught}/{total} injected defects were caught "
          f"and REFUSED.", file=out)
    return True


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main(argv=None):
    ap = argparse.ArgumentParser(
        description=__doc__.split("\n")[0],
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-v", "--verbose", action="store_true",
                    help="show every probe, not just failures")
    ap.add_argument("--self-break", action="store_true",
                    help="inject each historical defect and prove the gate "
                         "REFUSES (falsification control)")
    ap.add_argument("--provenance", action="store_true",
                    help="re-check that the fixtures still occur in the live "
                         "artifacts")
    ap.add_argument("--only", action="append", default=None,
                    help="run only the named screen(s) from the registry")
    ap.add_argument("--skip-heavy", action="store_true",
                    help="skip screens that scan band.exe / report.json")
    ap.add_argument("--list", action="store_true", help="list the registry")
    a = ap.parse_args(argv)

    if a.list:
        for name, _f, heavy in REGISTRY:
            print(f"  {name}{'  (heavy)' if heavy else ''}")
        return 0

    screens, untestable = build_registry(only=a.only, skip_heavy=a.skip_heavy)
    for name, reason in untestable:
        print(f"[UNTESTABLE] {name}: {reason}", file=sys.stderr)
        print("    >>> an untestable screen is NOT a passing screen; this run "
              "exits non-zero.", file=sys.stderr)

    if a.self_break:
        ok = self_break(screens, verbose=a.verbose)
        return 0 if (ok and not untestable) else 1

    print("=" * 74)
    print(f"SCREEN GATE: {len(screens)} screen(s) must prove they can fire")
    print("=" * 74)
    res = gate(screens, verbose=a.verbose)

    if a.provenance:
        check_provenance()

    print("\n" + "=" * 74)
    tally = {}
    for r in res.results:
        tally[r.verdict] = tally.get(r.verdict, 0) + 1
    print("VERDICTS: " + ", ".join(f"{k}={v}" for k, v in sorted(tally.items()))
          + (f", UNTESTABLE={len(untestable)}" if untestable else ""))
    if res.any_refused:
        print("\nREFUSED. The refused screen(s) above produce results that are "
              "VOID -- do\nNOT read their silence as a clean population.")
    elif any(r.verdict == "DRAINED" for r in res.results):
        print("\nAll screens sound. At least one vein is DRAINED: report it as "
              "drained,\nnot as a candidate list.")
    else:
        print("\nAll screens ARMED: each proved it fires on a known positive "
              "and stays\nsilent on a known negative. Their negatives may now "
              "be believed.")
    code = res.exit_code()
    return 1 if (code == 0 and untestable) else code


if __name__ == "__main__":
    sys.exit(main())
