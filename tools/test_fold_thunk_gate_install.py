#!/usr/bin/env python3
"""tools/fold_thunk_gate.py install(): a PARTIAL worklist must never shrink a
group or discard its evidence, and the file must come back in the house
round-trip (indent=1 + newline).

Measured defect (W16-AE, 2026-09-14): an OWNED group was "regenerated
wholesale" from a two-pair worklist, dropping five prior memberships and two
hand-appended evidence records, and the file was re-serialised at indent=2
(75,719-line diff).  Set FTG_MODULE=<path> to run this test against another
copy of the module -- against the pre-fix module it must FAIL.
"""
import importlib.util
import json
import os
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "scripts"))

mod_path = os.environ.get("FTG_MODULE") or str(ROOT / "tools" / "fold_thunk_gate.py")
spec = importlib.util.spec_from_file_location("ftg_under_test", mod_path)
G = importlib.util.module_from_spec(spec)
spec.loader.exec_module(G)

S = "?Survivor@@YAXXZ"
ADDR = "0x826c3888"
A, B, C = "?A@@YAXXZ", "?B@@YAXXZ", "?C@@YAXXZ"
HAND = "[W15-D 2026-09-14, VT1: hand-appended evidence that must survive]"


def row(F):
    return {"folded": F, "tier": "FT-EMPTY", "sites": 1, "survivor_fanin": 1116,
            "discredit": " | body carries no relocation", "body_evidence": "identical"}


def make_doc(owned):
    ev = (G.OWNED + " Evidence tier(s) FT-EMPTY. ... " + HAND) if owned else ("hand-written evidence " + HAND)
    return {"_comment": ["existing comment", "FOLD-THUNK TIER (FT, lane H, 2026-08-12) -- existing line"],
            "groups": [{"name": "Survivor", "address": ADDR, "survivor": S,
                        "folded": sorted([A, B]), "evidence": ev}]}


def run(owned):
    with tempfile.TemporaryDirectory() as td:
        p = Path(td) / "aliases.json"
        p.write_text(json.dumps(make_doc(owned), indent=1) + "\n")
        G.install({(S, ADDR): [row(C)]}, p)
        raw = p.read_text()
        doc = json.loads(raw)
        return doc, raw


failures = []
for owned in (True, False):
    doc, raw = run(owned)
    g = doc["groups"][0]
    tag = "owned" if owned else "hand"
    if set(g["folded"]) != {A, B, C}:
        failures.append("%s: partial install changed membership to %s (expected A,B,C)" % (tag, g["folded"]))
    if HAND not in g["evidence"]:
        failures.append("%s: prior evidence record discarded" % tag)
    if C not in g["evidence"]:
        failures.append("%s: new member's evidence not appended" % tag)
    if raw != json.dumps(doc, indent=1) + "\n":
        failures.append("%s: file not in house round-trip (indent=1 + newline)" % tag)
    ft = [c for c in doc["_comment"] if isinstance(c, str) and c.startswith("FOLD-THUNK TIER")]
    if len(ft) != 1 or not ft[0].endswith("existing line"):
        failures.append("%s: existing FOLD-THUNK TIER comment line was rewritten or duplicated: %r" % (tag, ft))
    if doc["_comment"][0] != "existing comment":
        failures.append("%s: unrelated comment line disturbed" % tag)

if failures:
    print("FAIL test_fold_thunk_gate_install (%s):" % mod_path)
    for f in failures:
        print("  - " + f)
    sys.exit(1)
print("PASS test_fold_thunk_gate_install: partial install unions membership, keeps evidence, house round-trip (%s)" % mod_path)
