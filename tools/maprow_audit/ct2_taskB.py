#!/usr/bin/env python3
"""Lane CT-2 TASK B: the NON-INJECTIVE rows -- 'the synthesized name is taken'.

CS-1 had 90 doubly-corroborated candidates and dropped 45 because the name it
wanted to write already sits on another map row. It called that a follow-on, not
a filter, and it is right: 'the name is taken' says nothing about who deserves
it. Two possibilities per collision, and they have OPPOSITE actions:

  INCUMBENT DEFECTIVE -> a 2-step PAIRED repair: move the incumbent off the name
                         first (to whatever ITS evidence says), then place mine.
  INCUMBENT CORRECT   -> MY candidate is wrong. Two rows cannot both be
                         '?Foo@Bar@@...' and the incumbent has independent
                         corroboration, so the collision REFUTES my proposal.
                         That is information, not a loss.

The adjudicator for the incumbent is the BRANCH channel (thunk_identity.
adjudicate_strict) plus, for non-thunk incumbents, the home-unit/objdiff view --
neither of which generated my candidate, so this is not self-confirmation.
"""
import json, sys, os, re, collections
sys.path.insert(0, "/home/free/tmp/laneCT2")
from ct2_slot import synth_from_parts
WT = "/home/free/tmp/laneCT2/wt"
sys.path.insert(0, WT + "/tools"); sys.path.insert(0, WT + "/tools/maprow_audit")
os.chdir(WT)
import thunk_identity as T, thunk_oracle as TO
from retail_reader import Image as RImage

raw = json.load(open("scripts/target_symbol_map.json"))
folded = {k.lower() if k.startswith("0x") else k: v for k, v in raw.items()}
img = T.Image("orig/45410914/band.exe")
bij = set(a.lower() for a in raw["_bijection_arbitrary"])
icf = set(a.lower() for a in raw["_icf_arbitrary"])
o = TO.Oracle(RImage("orig/45410914/band.exe")).prepare(folded)

treat = json.load(open("/home/free/tmp/laneCT2/treat602.json"))
rec = treat

# ---- reproduce CS-1's synthesis, with the adjustment-field regex FIXED -------
# CS-1's '\$([0-4])PPPPPPPM@([0-9A-P]+)@' cannot match the bare-digit adjustment
# form and can eat into the signature; that is 2 of its 'unsynthesizable'.
_VT = re.compile(r'\$([0-4])PPPPPPPM@(?:[A-P]+@|[0-9])')


def synth_thunk(thunkname, targetname):
    m = _VT.search(thunkname)
    if not m:
        return None
    cls = TO.qcls(thunkname)
    if not cls or not isinstance(targetname, str) or not targetname.startswith("?"):
        return None
    if "@@" not in targetname:
        return None
    _head, sig = targetname.split("@@", 1)
    if not sig or sig[0] not in "ABEFIJMNQRUV":
        return None
    meth = TO.method_of(targetname)
    if not meth:
        return None
    if targetname.startswith("??_G") or targetname.startswith("??_E"):
        return f"{targetname[:4]}{cls}@@{m.group(0)}{sig[1:]}"
    if targetname.startswith("??"):
        return None
    return f"?{meth}@{cls}@@{m.group(0)}{sig[1:]}"


names_ct = collections.Counter(v for k, v in raw.items()
                               if k.startswith("0x") and isinstance(v, str))
name_owner = collections.defaultdict(list)
for k, v in folded.items():
    if k.startswith("0x") and isinstance(v, str):
        name_owner[v].append(k)

cond = [a for a in rec if rec[a]["verdict"] == "METHOD_DIFFERS"]
both = [a for a in cond if rec[a].get("target_row") and o.slot_consensus(a)
        and TO.method_of(rec[a]["target_row"]) == o.slot_consensus(a)]
print("METHOD_DIFFERS in my baseline = %d (CS-1 had 129; its 5 landed edits "
      "moved 5 to AGREE)" % len(cond))
print("doubly-corroborated (branch method == slot vote) = %d (CS-1: 90)" % len(both))

stats = collections.Counter()
props = {}
for a in both:
    r = rec[a]
    if r["target"] in bij or r["target"] in icf:
        stats["reject ICF/bij target"] += 1; continue
    if not o.class_ok(a, r["proposed"]):
        stats["reject incumbent class contradicted"] += 1; continue
    n = synth_thunk(r["proposed"], r["target_row"])
    if not n:
        stats["reject unsynthesizable"] += 1; continue
    if n == r["proposed"]:
        stats["reject noop"] += 1; continue
    props[a] = n
for k, v in stats.most_common():
    print("   %-40s %d" % (k, v))
inj = {a: n for a, n in props.items() if not names_ct.get(n)}
non = {a: n for a, n in props.items() if names_ct.get(n)}
print("   synthesizable proposals                  %d" % len(props))
print("     -> injective (CS-1 landed from here)   %d" % len(inj))
print("     -> NON-INJECTIVE  <-- TASK B           %d  (CS-1: 45)" % len(non))

# ---------------- adjudicate each INCUMBENT ----------------------------------
print("\n=== IS THE INCUMBENT ITSELF DEFECTIVE? ===")
cls = collections.Counter()
rows = []
for a, newname in sorted(non.items()):
    owners = name_owner[newname]
    ent = {"addr": a, "want": newname, "cur": folded[a], "owners": owners}
    if len(owners) != 1:
        cls["collision with a DUPLICATED name (%d owners)" % len(owners)] += 1
        ent["verdict"] = "MULTI_OWNER"
        rows.append(ent); continue
    b = owners[0]
    ent["incumbent_addr"] = b
    # Is the incumbent a thunk we can adjudicate with the branch channel?
    if T.thunk_kind(folded[b]) is not None and T.is_forwarder(img, int(b, 16)):
        rb = T.adjudicate_strict(img, folded, b, folded[b])
        ent["incumbent_verdict"] = rb["verdict"]
        ent["incumbent_target_row"] = rb.get("target_row")
        if rb["verdict"] in ("AGREE", "AGREE_METHOD"):
            cls["incumbent CONFIRMED by branch channel -> MY CANDIDATE IS WRONG"] += 1
            ent["verdict"] = "INCUMBENT_CORRECT"
        elif rb["verdict"] == "METHOD_DIFFERS":
            cls["incumbent ALSO defective -> 2-step paired repair possible"] += 1
            ent["verdict"] = "INCUMBENT_DEFECTIVE"
        else:
            cls["incumbent unadjudicable (%s)" % rb["verdict"]] += 1
            ent["verdict"] = "INCUMBENT_UNKNOWN"
    else:
        # a PLAIN (non-thunk) row holding the name.  A vtordisp thunk name and a
        # plain body name are DIFFERENT strings, so a plain row can only collide
        # if it literally is that mangled thunk name -- i.e. it is a thunk body
        # the map mislabelled, or my synthesis produced a plain-shaped name.
        cls["incumbent is a NON-THUNK row"] += 1
        ent["verdict"] = "INCUMBENT_PLAIN"
    rows.append(ent)
for k, v in cls.most_common():
    print("   %-62s %d" % (k, v))

json.dump(rows, open("/home/free/tmp/laneCT2/taskB_rows.json", "w"), indent=1)
print("\nwrote taskB_rows.json (%d)" % len(rows))
print("\nsample:")
for e in rows[:8]:
    print("  %s [%s]\n     cur  %s\n     want %s\n     held by %s = %s"
          % (e["addr"], e["verdict"], e["cur"][:70], e["want"][:70],
             e.get("incumbent_addr"), str(folded.get(e.get("incumbent_addr", "")))[:70]))
