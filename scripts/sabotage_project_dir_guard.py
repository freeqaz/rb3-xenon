#!/usr/bin/env python3
"""Deliberately restore the main-repo fallback, and require the guard to notice.

Run: python3 -B scripts/sabotage_project_dir_guard.py     (exit 0 = every
                                                           sabotage was caught)

Why this file exists
--------------------
`scripts/orchestrator/test_project_dir_guard.py` is green. That is not evidence:
this repo has shipped guards that passed on deliberately broken code, including
one written specifically to catch the bug it then missed. Until each assertion
has been watched going red for the RIGHT reason, "the tests pass" is a claim
about nothing. Each entry below names the test that MUST fail.

WHAT IS SANDBOXED, AND WHY IT IS A SLICE AND NOT THE FILE
---------------------------------------------------------
`mcp_server.py` cannot be imported from a sandbox -- it pulls in `mcp`,
`orchestrator.database`, `tools.struct_db` and `analysis.ruler`. So the harness
EXTRACTS the guard region (`resolve_project_dir` + `redirect_compile_object` and
their helpers) out of the REAL file, verbatim, and sabotages that. The extracted
region is exactly the region the defect lived in, and the extraction is checked:
if the markers ever stop matching, or the region stops defining the names the
test imports, this harness REFUSES rather than reporting a clean sweep over an
empty slice (the vacuity that would otherwise make it agree with whatever it was
pointed at).

⚠ THE `.pyc` TRAP (see scripts/sabotage_obj_pairing.py for the full statement):
a byte-length-preserving edit applied and reverted inside one second can leave
the interpreter loading a STALE `.pyc`, so the sabotage never executes and the
harness reports that a test which never saw the bug caught it. Defences: every
subprocess runs `-B` with `PYTHONDONTWRITEBYTECODE=1`, the sandbox is a fresh
temp dir per run, and the NULL arm below asserts that a sabotage which changes
nothing is NOT reported as caught.
"""

import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SERVER = REPO / "scripts" / "orchestrator" / "mcp_server.py"
TEST = REPO / "scripts" / "orchestrator" / "test_project_dir_guard.py"
ENV = dict(os.environ, PYTHONDONTWRITEBYTECODE="1")

START = "# ── which tree do we measure?"
END_FN = "def redirect_compile_object("

#: name -> (defect, target test). The defect is a (old, new) substring pair
#: applied to the extracted region.
DEFECTS = [
    (
        "restore the three-tier main-repo fallback (THE original defect)",
        ('    raise ProjectDirRefusal(\n'
         '        "project_dir is REQUIRED and was not given,',
         '    return main_resolved\n'
         '    raise ProjectDirRefusal(\n'
         '        "project_dir is REQUIRED and was not given,'),
        "test_absent_project_dir_is_refused",
    ),
    (
        "accept an explicitly-spelled main repo",
        ("if p.resolve() == main_resolved and not allow_main:",
         "if False:"),
        "test_explicit_main_is_refused",
    ),
    (
        "compare paths as STRINGS, so `main/.` and a symlink slip through",
        ("if p.resolve() == main_resolved and not allow_main:",
         "if str(p) == str(main_resolved) and not allow_main:"),
        "test_main_by_a_different_spelling_is_still_main",
    ),
    (
        "treat any non-empty RB3_MCP_ALLOW_MAIN as true (so `=0` opens the door)",
        ('return str(env.get(MAIN_REPO_OPT_IN, "")).strip().lower() in _TRUE',
         'return bool(env.get(MAIN_REPO_OPT_IN, ""))'),
        "test_falsey_opt_in_does_not_open_the_door",
    ),
    (
        "default env to {} instead of os.environ (REPO_ROOT becomes invisible)",
        "env = os.environ if env is None else env",
        # replacement supplied below as a pair
    ),
]
# the 5th entry needs a 2-tuple defect; declare it explicitly rather than
# hand-waving the shape
DEFECTS[4] = (
    "default env to {} instead of os.environ (REPO_ROOT becomes invisible)",
    ("env = os.environ if env is None else env",
     "env = {} if env is None else env"),
    "test_real_os_environ_is_the_default_env",
)
DEFECTS += [
    (
        "let the compile redirect fall through instead of refusing",
        ("    if not n:\n        raise CompileRedirectRefusal(",
         "    if not n:\n        return compile_cmd, 0\n    if False:\n        raise CompileRedirectRefusal("),
        "test_redirect_refuses_when_it_cannot_find_the_object",
    ),
    (
        "redirect only the FIRST output path, leaving /Fo on the tree's object",
        ("return compile_cmd.replace(obj_target, str(dest)), n",
         "return compile_cmd.replace(obj_target, str(dest), 1), n"),
        "test_redirect_moves_every_output_path",
    ),
]

#: The NULL arm. A no-op "sabotage" must NOT be reported as caught; if it is,
#: the harness is measuring something other than the code it thinks it patched.
NULL_ARM = ("no-op edit (the harness's own control)",
            ("allow_main = _opt_in_to_main(env)",
             "allow_main = _opt_in_to_main(env)  # noqa"),
            None)


def extract_region():
    src = SERVER.read_text()
    i = src.find(START)
    if i < 0:
        sys.exit(f"REFUSING: marker {START!r} not found in {SERVER} -- the "
                 f"guard region moved and this harness would sabotage nothing.")
    j = src.find(END_FN, i)
    if j < 0:
        sys.exit("REFUSING: redirect_compile_object not found after the marker.")
    # to the end of that function: next top-level `def`/`class`, or EOF
    m = re.search(r"\n(?=[A-Za-z@#])", src[j:])
    k = j + (m.start() if m else len(src[j:]))
    region = src[i:k]
    needed = ("MAIN_REPO_OPT_IN", "ProjectDirRefusal", "resolve_project_dir",
              "CompileRedirectRefusal", "redirect_compile_object")
    missing = [n for n in needed if n not in region]
    if missing:
        sys.exit(f"REFUSING: extracted region does not define {missing} -- a "
                 f"sweep over it would be vacuous.")
    return "import os\nfrom pathlib import Path\n\n" + region


def sandbox(region):
    d = Path(tempfile.mkdtemp(prefix="sabotage-projectdir-"))
    (d / "guard_region.py").write_text(region)
    t = TEST.read_text()
    t = t.replace("from orchestrator.mcp_server import (",
                  "from guard_region import (")
    t = re.sub(r"sys\.path\.insert\([^\n]*\n", "", t)
    (d / "test_guard.py").write_text(t)
    return d


def failures(d):
    r = subprocess.run([sys.executable, "-B", "-m", "pytest", "test_guard.py",
                        "-q", "--no-header", "-rf"],
                       cwd=d, env=ENV, capture_output=True, text=True)
    return {m.group(1) for m in re.finditer(r"^FAILED test_guard\.py::(\w+)",
                                            r.stdout, re.M)}, r


def main():
    region = extract_region()

    # Baseline: the pristine region must be GREEN, or nothing below means
    # anything (a red baseline makes every sabotage look "caught").
    d = sandbox(region)
    base_fail, r = failures(d)
    shutil.rmtree(d, ignore_errors=True)
    if base_fail:
        sys.exit(f"REFUSING: baseline is RED ({sorted(base_fail)}). Fix the "
                 f"guard before asking whether sabotage is detected.\n{r.stdout[-2000:]}")
    print("[sabotage] baseline green on the extracted region")

    caught = wrong = 0
    for label, (old, new), target in DEFECTS:
        if old not in region:
            sys.exit(f"REFUSING: defect anchor for {label!r} not present -- the "
                     f"code moved and this arm tests nothing.")
        d = sandbox(region.replace(old, new, 1))
        fail, _ = failures(d)
        shutil.rmtree(d, ignore_errors=True)
        if target in fail:
            caught += 1
            extra = sorted(fail - {target})
            note = f"  (+{len(extra)} collateral)" if extra else ""
            print(f"  [caught] {label}\n           -> {target}{note}")
        else:
            wrong += 1
            print(f"  [MISSED] {label}\n           expected {target} to fail, "
                  f"red was {sorted(fail) or 'NOTHING'}")

    # NULL arm
    label, (old, new), _ = NULL_ARM
    assert old in region
    d = sandbox(region.replace(old, new, 1))
    nfail, _ = failures(d)
    shutil.rmtree(d, ignore_errors=True)
    if nfail:
        print(f"  [VACUOUS] {label} was reported as breaking {sorted(nfail)} -- "
              f"the harness is not measuring what it patched")
        wrong += 1
    else:
        print(f"  [ok]     null arm: {label} broke nothing, as required")

    print(f"\n[sabotage] {caught}/{len(DEFECTS)} caught by the intended test, "
          f"{wrong} not caught (or caught vacuously)")
    return 1 if wrong or caught != len(DEFECTS) else 0


if __name__ == "__main__":
    sys.exit(main())
