#!/usr/bin/env python3
"""Prove `scripts/test_offset_resolver_frame.py` CAN FAIL.

House pattern (cf. scripts/sabotage_obj_pairing.py, tools/grep_binary_guard.py
--self-break): apply a set of DISTINCT, REALISTIC defects to a sandbox copy of
`scripts/orchestrator/mcp_server.py` and require a SPECIFIC named check to go
red for each.  A test that passes on a broken implementation is worse than no
test, and a guard that suppresses everything "fixes" the false positives while
producing no output at all -- so one of the sabotages below is exactly that
vacuity, and it must be caught by the surviving-true-positive check.

The sandbox is built out of SYMLINKS to the real tree, with only the one
patched file materialised, so nothing here can touch the repo.

Run:  python3 scripts/sabotage_offset_resolver.py
"""
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
TARGET = "scripts/orchestrator/mcp_server.py"
TEST = "scripts/test_offset_resolver_frame.py"


# (name, expected-failing check, description, [(find, replace), ...])
SABOTAGES = [
    ("restore_static_floor", "frame_pointer_r31_suppressed",
     "revert to W16-EA's static {r1,r13} -- the original defect",
     [("""        if prologue_ok:
            nonstruct_t = self._non_struct_base_regs(instructions, "target")
            nonstruct_b = self._non_struct_base_regs(instructions, "base")
        else:
            nonstruct_t = nonstruct_b = frozenset(self._NON_STRUCT_BASE_REGS)""",
       """        nonstruct_t = nonstruct_b = frozenset(self._NON_STRUCT_BASE_REGS)""")]),

    ("suppress_everything", "this_pointer_r31_resolves",
     "THE VACUITY DEFECT: suppress every base register, so no attribution is "
     "ever emitted and the false positives are 'fixed' by producing no output",
     [("        regs = set(cls._NON_STRUCT_BASE_REGS)",
       "        regs = {f'r{i}' for i in range(32)}")]),

    ("bare_digit_comparison", "frame_pointer_r31_suppressed",
     "compare _MEM_ARG_RE's BARE-DIGIT group(3) against the r-prefixed set, so "
     "the guard silently never matches (the exact trap the fix warns about)",
     [('                    reg_t = f"r{base_reg_t}"\n'
       '                    reg_b = f"r{base_reg_b}"',
       '                    reg_t = base_reg_t\n'
       '                    reg_b = base_reg_b')]),

    ("drop_tls_floor", "tls_r13_suppressed",
     "drop r13 from the static floor -- the prologue scan never yields it",
     [("    _NON_STRUCT_BASE_REGS = frozenset(['r1', 'r13'])",
       "    _NON_STRUCT_BASE_REGS = frozenset(['r1'])")]),

    ("never_hedge", "prologue_absent_is_hedged",
     "claim the prologue was inspected even when the list is filtered, so the "
     "resolver fails open silently instead of hedging",
     [("        if not instructions:\n            return False\n"
       "        first = instructions[0].get(\"index\")",
       "        if not instructions:\n            return False\n"
       "        return True\n"
       "        first = instructions[0].get(\"index\")")]),

    ("blind_the_prologue_scan", "frame_pointer_gemmanager_suppressed",
     "neuter the frame_base_regs derivation to a constant {r1} -- 'could not "
     "look' rendered indistinguishable from 'r31 is not a frame base'",
     [("            for r in frame_base_regs(instructions, side_key):",
       "            for r in ({'r1'} if True else frame_base_regs(instructions, side_key)):")]),
]


def build_sandbox(root: Path, patched_text: str):
    """Symlink the real tree; materialise only the patched file."""
    (root / "scripts" / "orchestrator").mkdir(parents=True)
    for entry in REPO.iterdir():
        if entry.name == "scripts":
            continue
        (root / entry.name).symlink_to(entry)
    for entry in (REPO / "scripts").iterdir():
        if entry.name == "orchestrator":
            continue
        (root / "scripts" / entry.name).symlink_to(entry)
    for entry in (REPO / "scripts" / "orchestrator").iterdir():
        if entry.name == "mcp_server.py":
            continue
        (root / "scripts" / "orchestrator" / entry.name).symlink_to(entry)
    (root / "scripts" / "orchestrator" / "mcp_server.py").write_text(patched_text)


def run_check(impl_root: Path, only: str):
    r = subprocess.run([sys.executable, str(REPO / TEST),
                        "--impl-root", str(impl_root), "--only", only,
                        "--no-vacuity-probe"],
                       capture_output=True, text=True)
    return r.returncode, (r.stdout + r.stderr).strip()


def main():
    src = (REPO / TARGET).read_text()

    # Control: the UNSABOTAGED tree must pass every check, or a "FAIL" below
    # would prove nothing about the sabotage.
    r = subprocess.run([sys.executable, str(REPO / TEST)], capture_output=True, text=True)
    if r.returncode != 0:
        print("CONTROL FAILED -- the unsabotaged tree does not pass its own tests:")
        print(r.stdout + r.stderr)
        return 2
    print("control: unsabotaged tree passes all checks\n")

    caught = 0
    for name, expect_fail, desc, edits in SABOTAGES:
        text = src
        applied = True
        for find, repl in edits:
            if text.count(find) != 1:
                print(f"  SKIP  {name}: anchor not found exactly once "
                      f"({text.count(find)} hits) -- sabotage is STALE")
                applied = False
                break
            text = text.replace(find, repl, 1)
        if not applied:
            continue
        with tempfile.TemporaryDirectory() as td:
            root = Path(td) / "sandbox"
            root.mkdir()
            build_sandbox(root, text)
            rc, out = run_check(root, expect_fail)
        if rc == 1:
            caught += 1
            print(f"  CAUGHT  {name}  ->  {expect_fail} went RED")
            print(f"          ({desc})")
        else:
            print(f"  MISSED  {name}  ->  {expect_fail} still passed (rc={rc})")
            print(f"          ({desc})")
            print("          " + out.replace("\n", "\n          "))

    # Belt and braces: the suppress-everything defect must ALSO trip the
    # harness-level vacuity probe (exit 4) when that probe is left enabled.
    # The named check above proves the defect is caught on its own merits; this
    # proves the harness would shout even if that check did not exist.
    text = src.replace("        regs = set(cls._NON_STRUCT_BASE_REGS)",
                       "        regs = {f'r{i}' for i in range(32)}", 1)
    probe_ok = False
    with tempfile.TemporaryDirectory() as td:
        root = Path(td) / "sandbox"
        root.mkdir()
        build_sandbox(root, text)
        r = subprocess.run([sys.executable, str(REPO / TEST), "--impl-root", str(root)],
                           capture_output=True, text=True)
        probe_ok = (r.returncode == 4 and "HARNESS VACUOUS" in r.stdout)
    print()
    print(f"  {'OK    ' if probe_ok else 'MISSED'}  harness vacuity probe trips on "
          f"suppress_everything (exit 4)")

    print()
    print(f"{caught}/{len(SABOTAGES)} sabotages caught")
    return 0 if (caught == len(SABOTAGES) and probe_ok) else 1


if __name__ == "__main__":
    sys.exit(main())
