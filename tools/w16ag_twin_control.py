"""Anti-vacuity control for w16ag_guard_adjudicate's strict twin test.

If the test can never return TWIN it cannot block anything, and the 8 SEPARATE
verdicts would be the instrument restating its own shape.  So: find, in retail
itself, a pair of DISTINCT .pdata functions whose bodies ARE identical including
call targets -- CD-7's 51-surplus class -- and require the test to call it TWIN.
Also decode the words the 8 rows actually differ in, so the evidence is concrete.
"""
import collections, struct, sys
sys.path.insert(0, 'tools'); sys.path.insert(0, 'scripts')
from w16ag_guard_adjudicate import Retail

R = Retail()
print("retail .pdata functions:", len(R.begins))

buckets = collections.defaultdict(list)
for va in R.begins:
    r = R.resolved(va)
    if r is None or len(r) < 6:
        continue
    buckets[tuple(r)].append(va)

twins = {k: v for k, v in buckets.items() if len(v) > 1}
print("distinct-address groups identical INCLUDING call targets:", len(twins))
print("surplus copies:", sum(len(v) - 1 for v in twins.values()))
ex = sorted(twins.items(), key=lambda kv: -len(kv[0]))[:3]
for k, v in ex:
    print("  len=%3d B  addrs=%s" % (len(k) * 4, [hex(x) for x in v[:4]]))

# the control: run the tool's own comparison on such a pair
if ex:
    k, v = ex[0]
    a, b = v[0], v[1]
    ra, rb = R.resolved(a), R.resolved(b)
    verdict = "TWIN" if (len(ra) == len(rb) and ra == rb) else "SEPARATE"
    print("\nCONTROL pair 0x%08x vs 0x%08x -> %s  (must be TWIN)" % (a, b, verdict))
    print("CONTROL", "PASS" if verdict == "TWIN" else "FAIL")

def dec(w):
    op = w >> 26
    d, a_, imm = (w >> 21) & 31, (w >> 16) & 31, w & 0xFFFF
    si = imm - 0x10000 if imm & 0x8000 else imm
    names = {14: "addi", 15: "addis", 32: "lwz", 36: "stw", 24: "ori",
             37: "stwu", 40: "lhz", 44: "sth", 34: "lbz", 38: "stb"}
    if op in names:
        return "%-5s r%d, %d(r%d)" % (names[op], d, si, a_) if op not in (14, 15, 24) \
               else "%-5s r%d, r%d, 0x%04x" % (names[op], d, a_, imm)
    return "op=%d raw=0x%08x" % (op, w)

print("\nDECODE of the differing non-branch words:")
for w in (963344948, 963318148, 1029734913, 1029734914, 963344624, 963316820,
          963367884, 963370196, 963361332, 963330484):
    print("   0x%08x  %s" % (w, dec(w)))
