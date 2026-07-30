#!/usr/bin/env python3
"""laneBV3 -- CLASS-CONSISTENCY post-filter for collision-adjudication winners.

WHAT IT CAUGHT
--------------
The collision channel asserted `?SetType@PracticeSection@@UAAXVSymbol@@@Z`
belongs at 0x823c7960 with **agree=5**, a cut at which the truth-ablation
control (collision_ablation.py) measures a 0.00% false-plant rate. It is wrong.
That body calls fn_8236A8A8, whose local-static string decodes to
"CharTransCopy", so it is `CharTransCopy::SetType`. main's rival was wrong too
(a 124-byte target against a 316-byte COMDAT), i.e. **truth was absent from both
candidates and the channel did not refuse.**

WHY agree=5 WAS WORTHLESS THERE
-------------------------------
Every OBJ_SET_TYPE body carries the same relocations -- the "types" string
literal, the Symbol ctor, its own static guard. Those agree with *any* SetType
body, so they are evidence-shaped non-evidence. This is exactly the mechanism
laneBU4 documented for the ByteCode / StaticByteCode sibling pair in the LIVE
channel and fixed there with `--scope-unique`; **the COLLISION channel never
received that cut**, and collision_ablation.py under-estimates the risk because
it draws random byte-twin decoys rather than same-family siblings.

So: `agree` counts shared boilerplate. This filter asks the one question that
discriminates *within* a family -- does the body reference a CLASS TOKEN that
contradicts the name it was assigned? For the thunk families
(`$4PPPPPPPM@...`) it is near-decisive: a thunk's only relocation is the real
method, so the class must match exactly.

READ THE OUTPUT WITH CARE -- CONTRADICTED IS A HEURISTIC
--------------------------------------------------------
Measured on the 32 asserted main-map defects: CONSISTENT 15, CONTRADICTED 6,
NO_CLASS_TOKEN 11 (STL template instantiations, no class component to extract).
Of the 6 CONTRADICTED **only one was a real refutation**; the other five are
artifacts and on inspection all five actually corroborate:

  * `??0NgFur@@`      -> calls `??0RndFur@@`  (base-class ctor: expected)
  * `??_GDxShader@@`  -> calls `??1DxShader@@` (deleting dtor: expected; the
                          `??_G` prefix also defeats the class regex)
  * `?RequirePushToTalk@Synth360@@` -> `?RequirePushToTalk@MicManagerXbox@@`
                          (delegation to the same method name)
  * two message-posters whose own string is their METHOD name
    ('show_brief_band_message', 'recommendations_ready'), never their class.

Treat CONTRADICTED as "look at this by hand", not as a verdict. The genuine
refutation is recognisable because a *sibling in the same family* resolves
correctly: `?SetType@RndPropAnim@@` -> `?StaticClassName@RndPropAnim@@` is
CONSISTENT right next to the PracticeSection row that is not.

USAGE
    collision_classcheck.py --worktree WT --lblidx LBL --supply supply.json
"""
import argparse
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import reloclib as R          # noqa: E402
import relocdisc as D         # noqa: E402

STR = re.compile(rb"^([\x20-\x7e]{2,63})\x00")
IDENT = re.compile(r"^[A-Za-z_]\w*$")


def cls_of(mangled):
    m = re.match(r"\?\?[0-9A-Z_]?([A-Za-z_]\w*)@@", mangled)
    if m:
        return m.group(1)
    m = re.match(r"\?[A-Za-z_]\w*@([A-Za-z_]\w*)@@", mangled)
    if m:
        return m.group(1)
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--worktree", required=True)
    ap.add_argument("--lblidx", required=True)
    ap.add_argument("--supply", required=True,
                    help="repoint_supply.py output (carries win VA + unit)")
    ap.add_argument("--out")
    args = ap.parse_args()

    wt = Path(args.worktree).resolve()
    lbl = {int(k): bytes.fromhex(v) for k, v in
           json.loads(Path(args.lblidx).read_text()).items()}
    cur = json.loads((wt / "scripts/target_symbol_map.json").read_text())
    va2name = {}
    for k, v in cur.items():
        if isinstance(v, str) and k.startswith("0x"):
            try:
                va2name[int(k, 16)] = v
            except ValueError:
                pass

    allt = {}
    for uname, tobj, tasm, cobj in D.unit_iter(wt):
        if not tasm.exists():
            continue
        try:
            for va, ti in R.target_funcs(tasm).items():
                allt.setdefault(va, ti)
        except Exception:
            pass
    print(f"indexed {len(allt)} target fns", file=sys.stderr)

    def static_string_of(va):
        ti = allt.get(va)
        if ti is None:
            return None
        for off, tok in ti["relocs"]:
            m = re.match(r"lbl_([0-9A-Fa-f]{8})$", tok)
            if m:
                b = lbl.get(int(m.group(1), 16))
                if b:
                    mm = STR.match(b)
                    if mm:
                        return mm.group(1).decode("latin1")
        return None

    sup = json.loads(Path(args.supply).read_text())
    verdicts, tally = {}, {}
    for r in sup:
        want = cls_of(r["name"])
        ti = allt.get(int(r["win"], 16))
        if ti is None:
            verdicts[r["name"]] = "NO_TARGET"
            tally["NO_TARGET"] = tally.get("NO_TARGET", 0) + 1
            continue
        toks, evid = set(), []
        for off, tok in ti["relocs"]:
            m = re.match(r"fn_([0-9A-Fa-f]{8})$", tok)
            if not m:
                continue
            cva = int(m.group(1), 16)
            nm, s = va2name.get(cva), static_string_of(cva)
            evid.append((nm or f"fn_{cva:08x}", cls_of(nm) if nm else None, s))
            if nm and cls_of(nm):
                toks.add(cls_of(nm))
            if s and IDENT.match(s):
                toks.add(s)
        own = static_string_of(int(r["win"], 16))
        if own and IDENT.match(own):
            toks.add(own)
        if not want:
            v = "NO_CLASS_TOKEN"
        elif not toks:
            v = "NO_EVIDENCE"
        elif want in toks:
            v = "CONSISTENT"
        else:
            v = "CONTRADICTED"
        verdicts[r["name"]] = v
        tally[v] = tally.get(v, 0) + 1
        if v == "CONTRADICTED":
            print(f"\n[CONTRADICTED] want={want} agree={r.get('win_agree')} "
                  f"{r['name'][:70]}")
            print(f"   win {r['win']}  own_string={own!r}")
            for nm, c, s in evid[:8]:
                print(f"      {nm[:62]:64s} cls={c} str={s!r}")

    print("\n=== TALLY ===")
    for k, v in sorted(tally.items(), key=lambda kv: -kv[1]):
        print(f"  {k:18s} {v:4d}")
    print("\n  NOTE: CONTRADICTED is a HEURISTIC -- see this file's docstring.")
    print("  On the 32 asserted defects only 1 of 6 was a real refutation.")
    if args.out:
        Path(args.out).write_text(json.dumps(verdicts, indent=1))


if __name__ == "__main__":
    main()
