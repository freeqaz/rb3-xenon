#!/usr/bin/env python3
"""Lane CZ-3 item (B): is the icf_site_census our-side obj STEM COLLISION closed
by CY-1's f592571a pairing fix?

CY-4 (@4d1ff149, which does NOT contain f592571a) flagged 14 paired units with
our-side obj stem collisions -- Dir, Rnd, Utl, Synth, FxSend* -- first-match-wins
across e.g. system/synth vs system/synth_xbox. Campaign memory calls that
collision folklore STALE. Both cannot be right about the SAME tree, so measure
both implementations against ONE tree.

FALSIFICATION CRITERIA, stated before running:
  * "closed" FAILS if the NEW load_unit_objs maps >=2 distinct unit names to the
    SAME our-side obj path (a unit compared against another unit's obj).
  * The test is VACUOUS unless the POSITIVE CONTROL fires: the OLD (pre-fix)
    logic must exhibit >=1 collision on THIS SAME TREE. If old shows 0 too, then
    CY-4's observation does not reproduce and that is the finding.

Read-only.
"""
import collections
import glob
import json
import sys
from pathlib import Path

ROOT = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()


def new_load_unit_objs(root):
    """Verbatim behaviour of the CURRENT tools/icf_site_census.py:load_unit_objs."""
    cfg = json.loads((root / "objdiff.json").read_text())
    out = {}
    for u in cfg.get("units", []):
        t, b = u.get("target_path"), u.get("base_path")
        if not t or not b:
            continue
        tp, bp = root / t, root / b
        if tp.exists() and bp.exists():
            out[u["name"]] = (str(tp), str(bp))
    return out


def old_load_unit_objs(root):
    """Verbatim pre-f592571a implementation (git show f592571a^)."""
    tgt = {Path(p).stem: p
           for p in glob.glob(str(root / "build/45410914/obj/*.obj"))}
    ours = {}
    for p in glob.glob(str(root / "build/45410914/src/**/*.obj"), recursive=True):
        ours.setdefault(Path(p).stem, p)
    return {k: (tgt[k], ours[k]) for k in tgt if k in ours}


def collisions(pairs, label):
    """Distinct unit keys sharing one our-side obj path."""
    byobj = collections.defaultdict(list)
    for unit, (_t, b) in pairs.items():
        byobj[b].append(unit)
    coll = {b: us for b, us in byobj.items() if len(us) > 1}
    print("  %-28s units=%d  distinct our-objs=%d  COLLIDING our-objs=%d"
          % (label, len(pairs), len(byobj), len(coll)))
    return coll


print("root: %s" % ROOT)
print("\n=== A. does one our-obj serve two units? ===")
new = new_load_unit_objs(ROOT)
old = old_load_unit_objs(ROOT)
cnew = collisions(new, "NEW (objdiff.json pairing)")
cold = collisions(old, "OLD (glob + bare stem)")

# --- the real question the OLD code got wrong: a unit paired to the WRONG obj.
# Under OLD, the key is a bare stem, so a unit whose true our-obj is
# src/system/synth_xbox/Foo.obj may be served src/system/synth/Foo.obj. Compare
# OLD's choice against the AUTHORITATIVE base_path objdiff.json states.
print("\n=== B. did OLD serve the WRONG obj (vs objdiff.json's authoritative base_path)? ===")
auth = {}          # stem -> set of authoritative our-obj paths across units
for unit, (_t, b) in new.items():
    auth.setdefault(Path(b).stem, set()).add(b)
multi = {s: v for s, v in auth.items() if len(v) > 1}
print("  stems with >1 DISTINCT authoritative our-obj: %d" % len(multi))
for s in sorted(multi)[:25]:
    print("     %-14s %s" % (s, sorted(str(Path(p)).split('/45410914/')[-1] for p in multi[s])))

wrong = []
for stem, (_t, ob) in old.items():
    a = auth.get(stem)
    if a and ob not in a:
        wrong.append((stem, ob, sorted(a)))
    elif a and len(a) > 1:
        wrong.append((stem, ob, sorted(a)))    # ambiguous: one stem, many真 objs
print("  OLD pairings that are wrong-or-ambiguous vs authoritative: %d" % len(wrong))
for stem, ob, a in sorted(wrong)[:25]:
    print("     %-14s OLD->%-46s  AUTH=%s"
          % (stem, str(ob).split('/45410914/')[-1],
             [str(p).split('/45410914/')[-1] for p in a]))

print("\n=== VERDICT ===")
if cold or wrong:
    print("  POSITIVE CONTROL FIRED: old logic is defective on THIS tree "
          "(%d colliding objs, %d wrong/ambiguous pairings)" % (len(cold), len(wrong)))
else:
    print("  ** POSITIVE CONTROL DID NOT FIRE -- test is VACUOUS, "
          "CY-4's observation does not reproduce **")
if cnew:
    print("  NEW logic STILL COLLIDES on %d our-objs -> NOT CLOSED" % len(cnew))
    for b, us in sorted(cnew.items())[:20]:
        print("     %s <- %s" % (str(b).split('/45410914/')[-1], us))
else:
    print("  NEW logic: 0 colliding our-objs -> CLOSED")
