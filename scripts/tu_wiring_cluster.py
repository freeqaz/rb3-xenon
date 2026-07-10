#!/usr/bin/env python3
"""Cluster the census orphans by address (gap > GAP splits a cluster) and
summarize dominant class + fn count + span per cluster."""
import json
import re
from collections import Counter
from pathlib import Path

ROOT = Path("/home/free/tmp/wt-tucensus")
GAP = 0x1000  # 4KB

orphans = json.loads((ROOT / "scripts" / "_census_orphans.json").read_text())
# already sorted by addr (json written sorted); resort to be safe
recs = sorted(({"addr": int(o["addr"], 16), "name": o["name"], "class": o["class"]} for o in orphans),
              key=lambda r: r["addr"])

clusters = []
cur = []
for r in recs:
    if cur and r["addr"] - cur[-1]["addr"] > GAP:
        clusters.append(cur)
        cur = []
    cur.append(r)
if cur:
    clusters.append(cur)


def top_classes(cl):
    c = Counter(r["class"] for r in cl)
    return c.most_common()


summ = []
for cl in clusters:
    span_lo = cl[0]["addr"]
    span_hi = cl[-1]["addr"]
    tc = top_classes(cl)
    # dominant class = most common non-global/nonstd
    dom = None
    for name, cnt in tc:
        if name not in ("<global>", "<nonstd>", "<n>"):
            dom = name
            break
    if dom is None:
        dom = tc[0][0]
    summ.append({
        "n": len(cl),
        "lo": span_lo, "hi": span_hi,
        "span_kb": (span_hi - span_lo) / 1024.0,
        "dom": dom,
        "classes": tc[:6],
    })

summ.sort(key=lambda s: -s["n"])
print(f"total clusters: {len(clusters)}  orphans: {len(recs)}")
print(f"clusters >=5 fns: {sum(1 for s in summ if s['n']>=5)}")
print()
print(f"{'#fn':>4} {'span_lo':>10} {'span_hi':>10} {'kb':>7}  dominant / classes")
for s in summ[:60]:
    cls = ", ".join(f"{n}:{c}" for n, c in s["classes"])
    print(f"{s['n']:>4} 0x{s['lo']:08X} 0x{s['hi']:08X} {s['span_kb']:7.1f}  {cls}")

# dump full for downstream source classification
Path(ROOT / "scripts" / "_census_clusters.json").write_text(json.dumps(
    [{"n": s["n"], "lo": f"0x{s['lo']:08X}", "hi": f"0x{s['hi']:08X}",
      "span_kb": round(s["span_kb"], 1), "dom": s["dom"],
      "classes": s["classes"]} for s in summ], indent=1))
