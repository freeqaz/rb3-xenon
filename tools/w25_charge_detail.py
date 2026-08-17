#!/usr/bin/env python3
"""W25-UI: dump the exact charged sites for one symbol, with context.

Used to adjudicate a name-charged row: is the charged callee pair an ICF
fold (two unrelated bodies the linker merged) or a genuinely wrong callee
(a real source defect)? The label alone carries no information -- objdiff's
LINKER_MERGED detector fires on exactly the definition of a wrong callee.
"""
import json
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from scripts.analysis import ruler as ruler_mod  # noqa: E402


def main():
    proj = Path(sys.argv[1]).resolve()
    unit = sys.argv[2]
    sym = sys.argv[3]
    rk = ruler_mod.resolve_ruler(proj)
    cli = str(proj / "bin/objdiff-cli")
    cmd = [cli, "diff", "-p", str(proj), "-u", unit, "--batch",
           "-f", "json", "-o", "-", "--include-instructions"] + rk.args
    out = subprocess.run(cmd, capture_output=True, text=True,
                         input=sym + "\n", timeout=900)
    txt = out.stdout.strip()
    j = json.loads(txt)
    rec = j[0] if isinstance(j, list) else j
    if rec.get("error"):
        print("ERROR:", rec)
        return
    ins = rec.get("instructions", []) or []
    print(f"symbol : {sym}")
    print(f"unit   : {unit}")
    print(f"ruler  : {rk.reloc_mode}")
    print(f"fuzzy  : {rec.get('fuzzy_match_percent')}")
    print(f"instructions: {len(ins)}  "
          f"charged: {sum(1 for i in ins if i.get('match_type') != 'equal')}")
    print()
    for n, i in enumerate(ins):
        mt = i.get("match_type")
        if mt == "equal":
            continue
        t = i.get("target") or {}
        b = i.get("base") or {}
        print(f"[{n}] match_type={mt}")
        print(f"    TARGET: {t.get('formatted') or t.get('mnemonic')}")
        print(f"    OURS  : {b.get('formatted') or b.get('mnemonic')}")
        ta = t.get("typed_args", []) or []
        ba = b.get("typed_args", []) or []
        for x, y in zip(ta, ba):
            if x.get("value") != y.get("value"):
                print(f"      DIFF arg type={x.get('type')}")
                print(f"        target: {x.get('value')}")
                print(f"        ours  : {y.get('value')}")
        print()


if __name__ == "__main__":
    main()
