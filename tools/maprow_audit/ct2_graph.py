#!/usr/bin/env python3
"""Lane CT-2: the NAME-DEMAND GRAPH -- turn 'the name is taken' into a repair.

Edge A -> B means 'row A's evidence says it should carry the name row B holds'.
Three topologies, three different actions:

  CYCLE   A->B->...->A   a pure PERMUTATION of names already in the map.
                         Injectivity is preserved BY CONSTRUCTION (the multiset
                         of names is unchanged), so it needs no free name and
                         cannot introduce a duplicate. The safest repair there is.
  CHAIN   A->B where B has its own candidate  -- resolvable if applied in order,
                         head-first, so the name is vacated before it is claimed.
  STUB    A->B where B is TARGET_UNNAMED      -- blocked unless something else
                         supplies B a name. This is exactly where TASK A feeds
                         TASK B.

⚠ CS-1 reported 'no reciprocal 2-cycles (0)' -- but it searched for swap partners
inside SHAPE GROUPS, a different relation. This graph is built from the branch+
slot evidence, so a null result there does not predict one here. Measured below.
"""
import json, sys, os, collections
WT = "/home/free/tmp/laneCT2/wt"
sys.path.insert(0, WT + "/tools"); sys.path.insert(0, WT + "/tools/maprow_audit")
os.chdir(WT)
import thunk_identity as T, thunk_oracle as TO

raw = json.load(open("scripts/target_symbol_map.json"))
folded = {k.lower() if k.startswith("0x") else k: v for k, v in raw.items()}
rows = json.load(open("/home/free/tmp/laneCT2/taskB_rows.json"))
taskA = json.load(open("/home/free/tmp/laneCT2/taskA_detail.json"))

# every row that has a candidate name, from EITHER task
want = {}
for e in rows:
    want[e["addr"]] = e["want"]
name_owner = {}
for k, v in folded.items():
    if k.startswith("0x") and isinstance(v, str):
        name_owner.setdefault(v, []).append(k)

# demand edges
edge = {}
for a, n in want.items():
    ow = name_owner.get(n, [])
    if len(ow) == 1:
        edge[a] = ow[0]
print("demand edges: %d (from %d task-B rows)" % (len(edge), len(rows)))

# ---- cycles -----------------------------------------------------------------
seen, cycles = set(), []
for start in edge:
    if start in seen:
        continue
    path, cur = [], start
    local = {}
    while cur in edge and cur not in local:
        local[cur] = len(path)
        path.append(cur)
        cur = edge[cur]
    if cur in local:
        cyc = path[local[cur]:]
        if not any(x in seen for x in cyc):
            cycles.append(cyc)
    seen.update(path)
print("\nCYCLES found: %d  %s" % (len(cycles), [len(c) for c in cycles]))
for c in cycles:
    print("  cycle len %d:" % len(c))
    for x in c:
        print("     %s  %s\n            -> wants %s" % (x, folded[x][:66], want[x][:66]))

# ---- chain heads: rows nobody demands, i.e. safe to start a chain from -------
demanded = set(edge.values())
heads = [a for a in edge if a not in demanded]
print("\nchain heads (row wants a name, but nobody wants ITS name): %d" % len(heads))
term = collections.Counter()
for h in heads:
    cur, n = h, 0
    while cur in edge and n < 20:
        cur = edge[cur]; n += 1
    term[("cycle/loop" if cur in edge else
          ("terminal row has NO candidate -> BLOCKED on it" ))] += 1
for k, v in term.most_common():
    print("   %-52s %d" % (k, v))

# ---- how many task-B stubs are unblocked by a TASK-A candidate? -------------
a_cand = {d["addr"]: (d["slot"] or d["cothunk"]) for d in taskA
          if (d["slot"] or d["cothunk"])}
stub = [e for e in rows if e["verdict"] == "INCUMBENT_UNKNOWN"]
unblocked = [e for e in stub if e.get("incumbent_addr") in a_cand]
print("\nTASK A x TASK B INTERLOCK")
print("   task-B rows blocked on a TARGET_UNNAMED incumbent : %d" % len(stub))
print("   ... whose incumbent HAS a task-A slot/co-thunk candidate: %d" % len(unblocked))
for e in unblocked:
    b = e["incumbent_addr"]
    print("     %s wants %s\n        held by %s, whose own candidate is %s"
          % (e["addr"], e["want"][:56], b, str(a_cand[b])[:56]))

json.dump({"cycles": cycles, "edges": edge}, open("/home/free/tmp/laneCT2/graph.json", "w"), indent=1)
