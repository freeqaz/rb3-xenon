#!/usr/bin/env python3
"""Prove scripts/verify_objs_patched.py's DENYLIST EFFECT CHECK can fail (W16-AE).

W16-AB claimed `_denylist` "never polices the APPLIED map". Measured, the
renamer has honoured `_denylist` since f3fe9ab1; the real gap was that no gate
verified the refusal REACHED the built target objs -- the pre-f3fe9ab1
"declared and ignored" state would have passed every check there was.

Four arms, run against the real built tree (a pre-renamer tree makes the
MUTATION arm pass vacuously; that is reported as a failure, not a pass):

  TREE      --check-denylist on the live map                     -> rc 0
  MUTATION  sandbox map with 0x82553fc8 appended to `_denylist`  -> rc 7,
            output names 0x82553fc8 (its row is live and its name is defined
            in target objs: MemTracker/VocalTrack/PrefabMgr, measured 09-14)
  NULL      sandbox copy of the map, unchanged                   -> rc 0
  MALFORMED sandbox map whose `_denylist` is a string            -> rc 7

Exit 0 iff all four arms behave.  Registered in scripts/test_tools.py.
"""
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
VERIFIER = REPO / "scripts" / "verify_objs_patched.py"
MAP = REPO / "scripts" / "target_symbol_map.json"
MUTANT = "0x82553fc8"


def run(extra):
    p = subprocess.run([sys.executable, str(VERIFIER), "--check-denylist"] + extra,
                       cwd=str(REPO), capture_output=True, text=True)
    return p.returncode, (p.stdout + p.stderr)


def main():
    fails = 0

    def arm(label, rc, out, want_rc, must_contain=None):
        nonlocal fails
        ok = (rc == want_rc) and (must_contain is None or must_contain in out)
        print("  %s  %-10s rc=%d (want %d)%s" % (
            "ok " if ok else "FAIL", label, rc, want_rc,
            "" if must_contain is None else
            "  names %s: %s" % (must_contain, must_contain in out)))
        if not ok:
            fails += 1
            print("      ---- output tail ----")
            for line in out.strip().splitlines()[-8:]:
                print("      " + line)

    doc = json.loads(MAP.read_text())
    if MUTANT in doc.get("_denylist", []):
        print("FAIL: %s is already denylisted in the live map -- the mutation arm "
              "would not be a mutation; pick another live address" % MUTANT)
        return 1
    if not isinstance(doc.get(MUTANT), str):
        print("FAIL: %s has no live string name in the map (%r) -- the mutation "
              "arm would be vacuous" % (MUTANT, doc.get(MUTANT)))
        return 1

    rc, out = run([])
    arm("TREE", rc, out, 0)

    with tempfile.TemporaryDirectory(dir=os.path.expanduser("~/tmp")) as td:
        mut = dict(doc)
        mut["_denylist"] = list(doc["_denylist"]) + [MUTANT]
        mp = Path(td) / "map_mut.json"
        mp.write_text(json.dumps(mut, indent=1, ensure_ascii=False) + "\n")
        rc, out = run(["--map", str(mp)])
        arm("MUTATION", rc, out, 7, must_contain=MUTANT)
        if rc == 0:
            print("      ^ the mutation PASSED: either the tree is PRE-RENAMER "
                  "(build it: reflinked target objs carry no mangled names) or "
                  "the check is vacuous. Either way this test cannot vouch.")

        nul = Path(td) / "map_null.json"
        nul.write_text(json.dumps(doc, indent=1, ensure_ascii=False) + "\n")
        rc, out = run(["--map", str(nul)])
        arm("NULL", rc, out, 0)

        bad = dict(doc)
        bad["_denylist"] = "0x82553fc8"
        bp = Path(td) / "map_bad.json"
        bp.write_text(json.dumps(bad, indent=1, ensure_ascii=False) + "\n")
        rc, out = run(["--map", str(bp)])
        arm("MALFORMED", rc, out, 7)

    print("\n%s (%d failure(s))" % ("PASS" if fails == 0 else "FAIL", fails))
    return 0 if fails == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
