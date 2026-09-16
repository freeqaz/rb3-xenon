#!/usr/bin/env python3
"""Known-answer tests for the PER-FUNCTION frame-pointer guard in
`DecompMCPServer._resolve_offset_mismatches` (lane W16-GL, 2026-09-16).

WHY THIS EXISTS
---------------
The resolver attributes a memory-operand offset to a named struct field.  It
guarded that with a STATIC `_NON_STRUCT_BASE_REGS = {'1','13'}` (lane W16-EA's
fix for r1 spill slots), which is structurally incapable of catching the
commonest MSVC X360 shape: `subi r31, r1, FRAMESIZE` executed BEFORE the `stwu`
aliases r31 to the new r1, so `0x70(r31)` is a STACK SLOT.  Measured twice in
two days -- W16-GF on ?SetupGems@GemManager@@ and W16-GH on
?OnMsg@OvershellSlot@@, the latter emitting 24 confident attributions (19 of
them naming an OvershellSlot member) of which ALL were false, pointing a lane
AWAY from the row's real cause.

THE HAZARD THIS FILE IS SHAPED AGAINST
--------------------------------------
A guard that suppresses EVERYTHING removes the false positives perfectly and is
indistinguishable from a correct fix if you only assert that the false ones are
gone.  So this file asserts DISCRIMINATION IN BOTH DIRECTIONS, and every check
is named so `scripts/sabotage_offset_resolver.py` can require a SPECIFIC check
to go red for a SPECIFIC defect.  A test that passes on a broken implementation
is worse than no test.

It also builds its OWN struct_db fixture rather than reading the repo's
`struct_db.sqlite` -- that file is gitignored and regenerable, and a resolver
handed a missing DB returns [] for everything, which would make every
"suppressed" check pass VACUOUSLY.

Run:  python3 scripts/test_offset_resolver_frame.py
      python3 scripts/test_offset_resolver_frame.py --only this_pointer_r31_resolves
      python3 scripts/test_offset_resolver_frame.py --impl-root /path/to/sandbox
"""
import argparse
import importlib.util
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent


# ── fixture construction ─────────────────────────────────────────────────────

def _ins(idx, mt, t_op, t_args, b_op=None, b_args=None):
    """One objdiff instruction row. Defaults the base side to equal the target."""
    return {
        "index": idx,
        "match_type": mt,
        "target": {"opcode": t_op, "args": t_args},
        "base": {"opcode": b_op if b_op is not None else t_op,
                 "args": b_args if b_args is not None else t_args},
    }


# W16-GH's real prologue, transcribed from objdiff on the pre-lane tree:
#   [2] subi r31, r1, 0xf0   executes BEFORE   [3] stwu r1, -0xf0(r1)
# => r31 is the FRAME POINTER and every 0xNN(r31) is a stack slot.
_PROLOGUE_FRAME_PTR = [
    _ins(0, "equal", "mflr", "r12"),
    _ins(1, "equal", "bl", "__savegprlr_22"),
    _ins(2, "equal", "subi", "r31, r1, 0xf0"),
    _ins(3, "equal", "stwu", "r1, -0xf0(r1)"),
]

# W16-GF's real prologue on ?SetupGems@GemManager@@QAAXH@Z: note the DECOY
# `subi r12, r1, 0x98` (a save-helper pointer, not a frame pointer) two
# instructions ahead of the real r31 alias -- and `this` lands in r26, not r31.
_PROLOGUE_FRAME_PTR_GEM = [
    _ins(0, "equal", "mflr", "r12"),
    _ins(1, "equal", "bl", "__savegprlr_14"),
    _ins(2, "equal", "subi", "r12, r1, 0x98"),
    _ins(3, "equal", "bl", "__savefpr_28"),
    _ins(4, "equal", "subi", "r31, r1, 0x260"),
    _ins(5, "equal", "stwu", "r1, -0x260(r1)"),
    _ins(6, "equal", "mr", "r26, r3"),
]

# ?Eof@ChunkStream@@UAA?AW4EofType@@XZ -- the SYMMETRIC opposite, and the shape
# stack_layout.py's fixture 6 encodes: the frame comes from `stwu r1` alone and
# `mr r31, r3` puts `this` in r31, so 0xNN(r31) IS a field.  Corroborated on the
# real row by `lbz r11, 0x8a8(r3)` at [3] -- the same object, via r3, at an
# offset adjacent to the two the resolver attributes on r31.
_PROLOGUE_THIS_PTR = [
    _ins(0, "equal", "mflr", "r12"),
    _ins(1, "equal", "bl", "__savegprlr_26"),
    _ins(2, "equal", "stwu", "r1, -0x90(r1)"),
    _ins(3, "equal", "lbz", "r11, 0x8a8(r3)"),
    _ins(4, "equal", "mr", "r31, r3"),
]

DEMANGLED_CHUNKSTREAM = "public: virtual enum EofType __cdecl ChunkStream::Eof(void)"
DEMANGLED_OVERSHELL = ("public: class DataNode __cdecl OvershellSlot::OnMsg"
                       "(class ButtonDownMsg const &)")
DEMANGLED_GEM = "public: void __cdecl GemManager::SetupGems(int)"


def _build_struct_db(path: Path):
    """Build a minimal struct_db fixture through StructDB's OWN initialiser, so
    the schema cannot drift away from the production one."""
    sys.path.insert(0, str(REPO))
    from tools.struct_db import StructDB
    with StructDB(str(path)) as db:
        db.create_schema()
        cur = db.conn.cursor()
        rows = {
            "ChunkStream": [("mCurBufOffset", "int", 0x8a4),
                            ("mCurChunk", "int *", 0x8ac)],
            "OvershellSlot": [("mAutohideEnabled", "bool", 0x60),
                              ("mCurrentView", "Symbol", 0x64),
                              ("mBlockAllInput", "bool", 0x68),
                              ("mPotentialUsers", "std::vector<int>", 0x6c)],
            "GemManager": [("mTrackDir", "TrackDir *", 0x70),
                           ("mConfig", "DataArray *", 0x78)],
        }
        for cls, members in rows.items():
            cur.execute("INSERT INTO classes (name, file_path, is_struct) VALUES (?,?,0)",
                        (cls, f"fixture/{cls}.h"))
            cid = cur.lastrowid
            for name, ty, off in members:
                cur.execute(
                    "INSERT INTO members (class_id, name, type_str, offset, guard, guard_kind)"
                    " VALUES (?,?,?,?,'','retail')", (cid, name, ty, off))
        db.conn.commit()


def _load_resolver(impl_root: Path, struct_db_dir: Path):
    saved = list(sys.path)
    sys.path.insert(0, str(impl_root))
    sys.path.insert(0, str(impl_root / "scripts"))
    spec = importlib.util.spec_from_file_location(
        "w16gl_mcp_under_test", str(impl_root / "scripts" / "orchestrator" / "mcp_server.py"))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    sys.path[:] = saved
    srv = mod.DecompMCPServer.__new__(mod.DecompMCPServer)
    srv.project_root = struct_db_dir
    return srv, mod


# ── checks ───────────────────────────────────────────────────────────────────

CHECKS = {}


def check(name):
    def deco(fn):
        CHECKS[name] = fn
        return fn
    return deco


def _resolve(srv, prologue, demangled, diff_rows):
    data = {"demangled": demangled, "instructions": list(prologue) + list(diff_rows)}
    return srv._resolve_offset_mismatches(data)


# A struct-field-shaped mismatch pair, parameterised by base register.
def _pair(idx, reg, t_off, b_off, op="stw"):
    return _ins(idx, "diff_arg", op, f"r11, {t_off}({reg})", op, f"r11, {b_off}({reg})")


@check("frame_pointer_r31_suppressed")
def _c1(srv, mod):
    """W16-GH: r31 aliased to the frame => 0xNN(r31) is a STACK SLOT."""
    res = _resolve(srv, _PROLOGUE_FRAME_PTR, DEMANGLED_OVERSHELL,
                   [_pair(40, "r31", "0x60", "0x68"),
                    _pair(41, "r31", "0x64", "0x6c")])
    if res:
        return False, ("expected 0 attributions on a frame-pointer r31, got "
                       f"{len(res)}: {[(a.get('target_field'), a.get('base_field')) for a in res]}")
    return True, "0 attributions (both r31 operands read as stack slots)"


@check("frame_pointer_gemmanager_suppressed")
def _c2(srv, mod):
    """W16-GF: same class, with a decoy `subi r12, r1, 0x98` ahead of the alias."""
    res = _resolve(srv, _PROLOGUE_FRAME_PTR_GEM, DEMANGLED_GEM,
                   [_pair(43, "r31", "0x70", "0x78")])
    if res:
        return False, f"expected 0 attributions on GemManager's frame-pointer r31, got {len(res)}"
    return True, "0 attributions (decoy `subi r12, r1, 0x98` did not confuse it)"


@check("this_pointer_r31_resolves")
def _c3(srv, mod):
    """THE ANTI-VACUITY CHECK. `mr r31, r3` => r31 holds `this` => this MUST
    still resolve, with both field names.  If a guard silences this, it is
    over-broad and the fix has failed."""
    res = _resolve(srv, _PROLOGUE_THIS_PTR, DEMANGLED_CHUNKSTREAM,
                   [_pair(172, "r31", "0x8a4", "0x8ac")])
    if len(res) != 1:
        return False, f"expected 1 surviving attribution on a `this` r31, got {len(res)}"
    a = res[0]
    if "mCurBufOffset" not in (a.get("target_field") or ""):
        return False, f"target_field not resolved: {a.get('target_field')!r}"
    if "mCurChunk" not in (a.get("base_field") or ""):
        return False, f"base_field not resolved: {a.get('base_field')!r}"
    return True, "resolves to ChunkStream::mCurBufOffset / ChunkStream::mCurChunk"


@check("ea_stack_r1_suppressed")
def _c4(srv, mod):
    """Lane W16-EA's original case must keep working: r1 spill slots."""
    res = _resolve(srv, _PROLOGUE_THIS_PTR, DEMANGLED_CHUNKSTREAM,
                   [_ins(136, "diff_arg", "lwz", "r30, 0x8a4(r1)",
                         "lwz", "r11, 0x8ac(r1)")])
    if res:
        return False, f"expected 0 attributions on r1 spill slots, got {len(res)}"
    return True, "0 attributions (W16-EA guard intact)"


@check("tls_r13_suppressed")
def _c5(srv, mod):
    """r13 (small-data/TLS base) is never an object pointer."""
    res = _resolve(srv, _PROLOGUE_THIS_PTR, DEMANGLED_CHUNKSTREAM,
                   [_pair(50, "r13", "0x8a4", "0x8ac")])
    if res:
        return False, f"expected 0 attributions on r13, got {len(res)}"
    return True, "0 attributions"


@check("prologue_absent_is_hedged")
def _c6(srv, mod):
    """When the list is FILTERED (does not start at index 0) the prologue cannot
    be scanned.  Failing open would reinstate the false positives silently, so
    the entry must survive but carry an explicit `frame_check` hedge."""
    data = {"demangled": DEMANGLED_CHUNKSTREAM,
            "instructions": [_pair(172, "r31", "0x8a4", "0x8ac")]}
    res = srv._resolve_offset_mismatches(data)
    if len(res) != 1:
        return False, f"expected 1 hedged attribution, got {len(res)}"
    if not res[0].get("frame_check"):
        return False, "attribution is NOT hedged: no `frame_check` key"
    return True, "hedged"


@check("prologue_present_is_not_hedged")
def _c7(srv, mod):
    """...and the hedge must NOT fire when the prologue WAS inspected, or it is
    just noise on every row."""
    res = _resolve(srv, _PROLOGUE_THIS_PTR, DEMANGLED_CHUNKSTREAM,
                   [_pair(172, "r31", "0x8a4", "0x8ac")])
    if len(res) != 1:
        return False, f"expected 1 attribution, got {len(res)}"
    if res[0].get("frame_check"):
        return False, "hedge fired even though the prologue was present"
    return True, "not hedged"


@check("derivation_is_per_function_and_prefixed")
def _c8(srv, mod):
    """Direct check of the derivation: symmetric, and spelled `r31` not `31`.
    _MEM_ARG_RE captures the base register as BARE DIGITS, so a set spelled in
    one convention and compared in the other silently never matches."""
    cls = mod.DecompMCPServer
    frame = cls._non_struct_base_regs(_PROLOGUE_FRAME_PTR, "target")
    this_ = cls._non_struct_base_regs(_PROLOGUE_THIS_PTR, "target")
    if "r31" not in frame:
        return False, f"frame-pointer prologue did not yield r31: {sorted(frame)}"
    if "r31" in this_:
        return False, f"`mr r31, r3` prologue wrongly yielded r31: {sorted(this_)}"
    for s in (frame, this_):
        if not {"r1", "r13"} <= s:
            return False, f"static floor r1/r13 missing from {sorted(s)}"
        if any(not str(r).startswith("r") for r in s):
            return False, f"bare-digit spelling leaked into {sorted(s)}"
    return True, f"frame={sorted(frame)} this={sorted(this_)}"


# ── driver ───────────────────────────────────────────────────────────────────

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--impl-root", default=str(REPO),
                    help="repo root whose scripts/orchestrator/mcp_server.py is tested")
    ap.add_argument("--only", help="run a single named check")
    ap.add_argument("--no-vacuity-probe", action="store_true",
                    help="skip the harness-level 'did the fixture DB resolve anything at all' "
                         "probe. FOR scripts/sabotage_offset_resolver.py ONLY: under a "
                         "suppress-everything defect the probe aborts first (exit 4), which is "
                         "the right diagnosis but hides whether the NAMED check would have "
                         "caught it on its own. Disabling it proves each named check is "
                         "independently load-bearing.")
    args = ap.parse_args()

    impl_root = Path(args.impl_root).resolve()
    names = [args.only] if args.only else list(CHECKS)
    for n in names:
        if n not in CHECKS:
            print(f"no such check: {n}\nknown: {', '.join(CHECKS)}")
            return 2

    with tempfile.TemporaryDirectory() as td:
        td = Path(td)
        _build_struct_db(td / "struct_db.sqlite")
        srv, mod = _load_resolver(impl_root, td)

        # Anti-vacuity guard on the HARNESS itself: if the fixture DB were not
        # readable the resolver would return [] for everything and every
        # "suppressed" check would pass for the wrong reason.
        probe = _resolve(srv, _PROLOGUE_THIS_PTR, DEMANGLED_CHUNKSTREAM,
                         [_pair(1, "r30", "0x8a4", "0x8ac")])
        if (not args.no_vacuity_probe) and (not probe or not probe[0].get("target_field")):
            print("HARNESS VACUOUS: the fixture struct_db resolved no field at all; "
                  "the suppression checks would pass for the wrong reason.")
            return 4

        failed = 0
        for n in names:
            ok, msg = CHECKS[n](srv, mod)
            print(f"  {'PASS' if ok else 'FAIL'}  {n}" + (f"  -- {msg}" if msg else ""))
            if not ok:
                failed += 1

    print()
    print(f"{len(names) - failed}/{len(names)} checks passed")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
