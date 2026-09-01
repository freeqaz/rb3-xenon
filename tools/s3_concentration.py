#!/usr/bin/env python3
"""S3-ABLATE: the concentration curve of alias forgiveness over GROUPS.

    python3 tools/s3_concentration.py <jsonl> [<jsonl> ...]

⚠ Per-group NECESSITY bytes OVERLAP and MUST NOT be summed as if they
partitioned the total: a row needing two groups is counted under both. So this
reports TWO different denominators and never conflates them:

  (a) share of the SUM of per-group necessity bytes -- the statistic GROUNDED-1
      quoted ("top 10 = 55.6%"), comparable to it, but NOT a share of the
      measured 811,492 B total;
  (b) share of the 811,492 B FULL-vs-EMPTY total, via the UNION of rows
      attributed -- which is a genuine partition question and is reported as
      coverage, showing how much of the total the top-N groups actually reach.
"""
import json, sys, collections

recs, seen = [], set()
for p in sys.argv[1:]:
    for line in open(p):
        if not line.strip():
            continue
        r = json.loads(line)
        if r["i"] in seen:
            continue
        seen.add(r["i"]); recs.append(r)

TOTAL = 811492
recs.sort(key=lambda r: -r["fell_bytes"])
nz = [r for r in recs if r["fell_bytes"] > 0]
sum_nec = sum(r["fell_bytes"] for r in recs)

print("groups ablated (unique): %d" % len(recs))
print("  forgiving >0 bytes : %d (%.1f%%)" % (len(nz), 100.0 * len(nz) / max(len(recs), 1)))
print("  forgiving 0 bytes  : %d" % (len(recs) - len(nz)))
print("  sum of per-group necessity bytes: %d  (OVERLAPPING -- not a partition)" % sum_nec)
print("  measured FULL-vs-EMPTY total    : %d" % TOTAL)

print("\n(a) share of the SUM of per-group necessity bytes  [GROUNDED-1-comparable]")
for n in (1, 5, 10, 25, 50, 100, 200, 448):
    if n <= len(nz):
        print("    top %4d groups = %6.2f%%" % (n, 100.0 * sum(r["fell_bytes"] for r in nz[:n]) / sum_nec))

print("\n(b) UNION coverage of the 811,492 B total")
un = set(); cum = []
for r in nz:
    for k, v in r["fell"]:
        un.add((k, v))
    cum.append(sum(v for _, v in un))
for n in (1, 5, 10, 25, 50, 100, 200, 448):
    if n <= len(cum):
        print("    top %4d groups reach %8d B = %6.2f%% of total"
              % (n, cum[n - 1], 100.0 * cum[n - 1] / TOTAL))
if cum:
    print("    ALL %d measured groups reach %d B = %.2f%% of total"
          % (len(nz), cum[-1], 100.0 * cum[-1] / TOTAL))

print("\ntop 20 groups by necessity bytes")
print("%-52s %6s %8s %7s %6s" % ("group", "rows", "bytes", "d_fns", "nfold"))
for r in nz[:20]:
    print("%-52s %6d %8d %7d %6d"
          % (str(r["name"])[:52], r["n_fell"], r["fell_bytes"], r["d_fns"], r["n_folded"]))

d = collections.Counter()
for r in recs:
    b = r["fell_bytes"]
    d["0" if b == 0 else "1-99" if b < 100 else "100-999" if b < 1000
      else "1k-9k" if b < 10000 else ">=10k"] += 1
print("\nper-group necessity-byte distribution: %s" % dict(d))

# ---- population estimates from an unbiased sample -------------------------
# The sweep walks a SEEDED SHUFFLE, so any prefix is a uniform random sample of
# the 1,107 live groups. Statistic (a) computed inside a partial sample is
# BIASED HIGH (its denominator is only the sample's sum), so the population
# top-N share is estimated by rescaling: population top-k corresponds to sample
# top-(k*n/N), and the population sum is estimated as sample_sum * N/n.
N_LIVE = 1107
n = len(recs)
if n and n < N_LIVE:
    import math
    f = N_LIVE / n
    est_sum = sum_nec * f
    p = len(nz) / n
    se = math.sqrt(p * (1 - p) / n) * (1 - n / N_LIVE) ** 0.5   # FPC
    print("\n--- POPULATION ESTIMATES from an unbiased sample of %d/%d (%.1f%%) ---"
          % (n, N_LIVE, 100.0 * n / N_LIVE))
    print("  groups forgiving >0 bytes: %.1f%% +- %.1f%%  => ~%d of %d groups"
          % (100 * p, 196 * se, round(p * N_LIVE), N_LIVE))
    print("  estimated population sum of per-group necessity bytes: ~%d"
          % round(est_sum))
    print("  (vs the measured FULL-vs-EMPTY total of %d; the sum EXCEEDS the total"
          % TOTAL)
    print("   when rows need >1 group, and falls short when they need none.)")
    for k in (10, 50, 100):
        j = max(1, round(k * n / N_LIVE))
        if j <= len(nz):
            print("  est. population top-%d share ~ sample top-%d / est_sum = %.1f%%"
                  % (k, j, 100.0 * sum(r["fell_bytes"] for r in nz[:j]) / est_sum))
    print("  ^ crude: each estimate rests on only %d-%d sampled groups."
          % (max(1, round(10 * n / N_LIVE)), max(1, round(100 * n / N_LIVE))))
