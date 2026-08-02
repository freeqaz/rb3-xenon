#!/usr/bin/env python3
"""Split the WRONG_2ch stratum into MAP defects vs candidate SOURCE defects.

THE STRUCTURAL ARGUMENT (no measurement needed, and it cannot be got from bytes)
--------------------------------------------------------------------------------
A charge T<-B says: retail's caller invokes T, ours invokes B. When T and B are
two INSTANTIATIONS OF THE SAME TEMPLATE, our source is structurally incapable of
the alleged bug: `sort<T,Cmp>` calls `__introsort_loop<T,...,Cmp>` because the
template says so; the compiler cannot emit a call from one instantiation into a
sibling's body. Therefore such a charge CANNOT be a source defect and MUST be a
map defect -- some address is named after the wrong sibling.

When T and B are different functions entirely (GetMaxPoints vs GetBonusPoints),
both readings stay open and only evidence can separate them.

This is the cheapest high-value cut available on the wrong stratum, and it is
orthogonal to every byte-level channel used so far.

CAVEAT, stated because it bounds the claim: "same template" is decided by
demangling the mangled name's leading identifier, which is a syntactic test. It
proves the two names are sibling instantiations; it does NOT by itself say WHICH
map row (caller or callee) is misassigned.
"""
import collections, json, os, re, sys

V = json.load(open(sys.argv[1]))["pairs"]


def parse(n):
    """-> (kind, identifier, rest). Enough to decide sibling-hood."""
    if n.startswith("??$"):                      # template FUNCTION
        i = n.find("@", 3)
        return ("tmplfn", n[3:i] if i > 0 else n[3:], n)
    m = re.match(r'^\?(\??[A-Za-z0-9_]+|\?[0-9A-Z])@(.+)$', n)
    if n.startswith("??"):                       # operator / ctor / dtor
        m2 = re.match(r'^(\?\?[0-9A-Za-z_]+)@(.*)$', n)
        if m2:
            return ("special", m2.group(1), m2.group(2))
    if m:
        rest = m.group(2)
        cls = rest.split("@@")[0]
        return ("method", m.group(1), cls)
    return ("other", n, "")


def tmpl_base(cls):
    """?$Name@ARGS -> Name ; plain Class -> Class"""
    if cls.startswith("?$"):
        i = cls.find("@")
        return cls[2:i] if i > 0 else cls[2:]
    return cls


def classify(t, b):
    kt, it_, rt_ = parse(t)
    kb, ib, rb = parse(b)
    if kt != kb:
        return "DIFFERENT_KIND"
    if kt == "tmplfn":
        return "SIBLING_TEMPLATE_FN" if it_ == ib else "DIFFERENT_TEMPLATE_FN"
    if kt in ("method", "special"):
        if it_ != ib:
            return "DIFFERENT_MEMBER"
        ct, cb = tmpl_base(rt_), tmpl_base(rb)
        if ct == cb:
            return "SIBLING_SAME_CLASS"          # same class template, diff args
        return "SAME_MEMBER_DIFF_CLASS"
    return "OTHER"


rows = [(t, b, cl, v, n) for t, b, cl, v, det, hs, n in V]
w2 = [(t, b, cl, n) for t, b, cl, v, n in rows if v == "WRONG_2ch"]
print("WRONG_2ch pairs %d / sites %d" % (len(w2), sum(n for *_x, n in w2)))

cat = collections.Counter(); cats = collections.Counter()
bycat = collections.defaultdict(list)
for t, b, cl, n in w2:
    c = classify(t, b)
    cat[c] += 1; cats[c] += n
    bycat[c].append((n, t, b, cl))

STRUCT = {"SIBLING_TEMPLATE_FN", "SIBLING_SAME_CLASS"}
print("\n%-26s %6s %6s" % ("category", "pairs", "sites"))
for c, n in cat.most_common():
    flag = "  <-- MAP DEFECT (source structurally cannot do this)" if c in STRUCT else ""
    print("%-26s %6d %6d%s" % (c, n, cats[c], flag))

sp = sum(cat[c] for c in STRUCT); ss = sum(cats[c] for c in STRUCT)
print("\nSTRUCTURALLY-IMPOSSIBLE-AS-SOURCE-DEFECT: %d pairs (%.1f%%) / %d sites (%.1f%%)"
      % (sp, 100.0 * sp / len(w2), ss, 100.0 * ss / sum(n for *_x, n in w2)))
print("remaining CANDIDATE SOURCE DEFECTS       : %d pairs / %d sites"
      % (len(w2) - sp, sum(n for *_x, n in w2) - ss))

for c in ("DIFFERENT_MEMBER", "SAME_MEMBER_DIFF_CLASS", "DIFFERENT_TEMPLATE_FN",
          "DIFFERENT_KIND"):
    if not bycat[c]:
        continue
    print("\n--- top %s ---" % c)
    for n, t, b, cl in sorted(bycat[c], reverse=True)[:8]:
        print("  %3d [%s] %s\n           <- %s" % (n, cl, t[:92], b[:92]))
