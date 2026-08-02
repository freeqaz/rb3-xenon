#!/usr/bin/env python3
"""Lane CT-2 (A): the CO-THUNK channel -- the one built FOR the TARGET_UNNAMED case.

CS-1's branch channel fails on these 107 rows for one reason: the body the thunk
jumps to carries no map row, so there is nothing to compare against. But a body
reached by one vtordisp thunk is usually reached by SEVERAL -- one per
sub-object offset in a multiple-inheritance class. Those siblings are separate
map rows, and some of them are NAMED and are NOT bijection-arbitrary.

So: index every map row by the body it dethunks to. For an unnamed body, the
other incoming thunks vote on (method, signature).

CIRCULARITY GUARD -- the reason this is not just the bijection talking to itself:
require at least one voting co-thunk that is NOT in _bijection_arbitrary (an
UNTAINTED witness). Without that, a group of co-thunks drawn from the same
arbitrary equivalence class could 'corroborate' each other. (Note they usually
could not anyway: different sub-object offsets mean different `addi` immediates
mean different reloc-masked shapes, hence different draw classes -- but the gate
costs nothing and removes the argument.)

VALIDATION: restrict to rows whose target IS named, hide the target row, and ask
the co-thunk channel to predict it. Known answer, independent of the channel.
Null: predict from a RANDOM other body's co-thunk group.
"""
import json, sys, os, collections, random
sys.path.insert(0, "/home/free/tmp/laneCT2")
from ct2_slot import split_sig, synth_from_parts
WT = "/home/free/tmp/laneCT2/wt"
sys.path.insert(0, WT + "/tools"); sys.path.insert(0, WT + "/tools/maprow_audit")
os.chdir(WT)
import thunk_identity as T, thunk_oracle as TO

raw = json.load(open("scripts/target_symbol_map.json"))
folded = {k.lower() if k.startswith("0x") else k: v for k, v in raw.items()}
img = T.Image("orig/45410914/band.exe")
bij = set(a.lower() for a in raw["_bijection_arbitrary"])

pop = T.code_population(img, folded)
recs = {a: T.adjudicate_strict(img, folded, a, n) for a, n in pop.items()}

# body VA -> [thunk rows landing on it]
bodies = collections.defaultdict(list)
for a, r in recs.items():
    if r.get("target"):
        bodies[r["target"]].append(a)

sizes = collections.Counter(len(v) for v in bodies.values())
print("distinct dethunked bodies: %d   incoming-thunk histogram: %s"
      % (len(bodies), sorted(sizes.items())[:8]))


def sigparts(name):
    """(method, sig_tail) implied by a THUNK row's own name: strip the vtordisp
    token and read what remains -- that is the virtual's real signature."""
    if not isinstance(name, str):
        return None
    import re
    m = re.search(r'\$([0-4])PPPPPPPM@(?:[A-P]+@|[0-9])', name)
    if not m:
        m = re.search(r'@@([WXOPGH])(?:[A-P]+@|[0-9])(?=[A-Z]{2})', name)
        if not m:
            return None
        tail = name[m.end():]
    else:
        tail = name[m.end():]
    meth = TO.method_of(name)
    if not meth:
        return None
    if name.startswith("??_E") or name.startswith("??_G"):
        meth = name[:4]
    return (meth, tail)


def cothunk_vote(addr, exclude_target=None, require_untainted=True, pool=None):
    """Votes on (method, sig_tail) from the OTHER thunks hitting the same body."""
    r = recs.get(addr)
    tgt = pool if pool is not None else (r.get("target") if r else None)
    if not tgt:
        return None, 0, 0, False
    votes = collections.Counter()
    untainted = False
    n = 0
    for b in bodies.get(tgt, []):
        if b == addr:
            continue
        p = sigparts(folded[b])
        if not p:
            continue
        votes[p] += 1
        n += 1
        if b not in bij:
            untainted = True
    if not votes:
        return None, 0, 0, False
    (top, c) = votes.most_common(1)[0]
    if c != n:                       # unanimity, same rule the slot channel needed
        return None, n, len(votes), untainted
    if require_untainted and not untainted:
        return None, n, len(votes), untainted
    return top, n, len(votes), untainted


print("\n=== INSTRUMENT VALIDATION: co-thunk predicts a KNOWN target row ===")
named = [a for a, r in recs.items()
         if r["verdict"] in ("AGREE", "AGREE_METHOD") and r.get("target_row")]
hit = cov = 0
for a in named:
    p, n, nv, unt = cothunk_vote(a)
    if p is None:
        continue
    tp = split_sig(recs[a]["target_row"])
    if tp is None:
        continue
    cov += 1
    want = (tp[0], tp[2])
    # ??_E aliases ??_G: accept either (documented in thunk_identity)
    ok = (p == want) or (p[1] == want[1] and {p[0], want[0]} <= {"??_E", "??_G"})
    hit += ok
print("  co-thunk reproduces the real target row : %d/%d = %.1f%%  (coverage %d/%d = %.1f%%)"
      % (hit, cov, 100.0 * hit / max(cov, 1), cov, len(named), 100.0 * cov / len(named)))

print("  -- NULL: vote from a RANDOM other body's co-thunk group --")
allb = [b for b, v in bodies.items() if len(v) >= 2]
for seed in (11, 12, 13):
    rnd = random.Random(seed)
    h = c = 0
    for a in named:
        p, n, nv, unt = cothunk_vote(a, pool=rnd.choice(allb))
        if p is None:
            continue
        tp = split_sig(recs[a]["target_row"])
        if tp is None:
            continue
        c += 1
        want = (tp[0], tp[2])
        h += (p == want) or (p[1] == want[1] and {p[0], want[0]} <= {"??_E", "??_G"})
    print("     random-body null s=%d : %d/%d = %.1f%%" % (seed, h, c, 100.0 * h / max(c, 1)))

json.dump({b: v for b, v in bodies.items()}, open("/home/free/tmp/laneCT2/bodies.json", "w"))
print("\nwrote bodies.json")
