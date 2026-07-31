#!/usr/bin/env python3
"""Lint the project's Claude memory graph for unreachable and dangling nodes.

The memory dir is a hub-and-spoke graph: MEMORY.md links HUBS, hubs link spokes.
Two failure modes make a finding effectively lost, and neither is visible by
reading any single file:

  ORPHAN   a memory reachable from nothing -- it exists, but recall never
           surfaces it.  (Found 8 on 2026-07-31, incl. the MILO_DEBUG
           force-define finding written the previous night.)
  DANGLING a [[link]] naming a memory that does not exist -- usually a slug
           typo.  (Found 2 on 2026-07-31: [[docs-index-2026-07-06]] and
           [[civetweb-port-2026-07-15]], both missing the `project-` prefix.
           Each silently orphaned its target.)

WHY THIS IS NOT A ONE-LINER -- the trap that motivated the tool:

  Links appear in THREE spellings that all denote the same node:
      [[project-foo-2026-07-30]]     kebab wiki-link
      [[project_foo_2026-07-30]]     snake wiki-link  (both are in live use)
      [Title](project_foo_2026-07-30.md)   markdown path
  A checker that normalizes only one spelling OVER-REPORTS.  The first version
  of this check tested kebab + path but not snake, and reported 35 orphans when
  the true count was 8 -- a 4x false-positive rate that would have sent an agent
  "repairing" 27 already-healthy nodes.  Normalize everything to lowercase-kebab
  before comparing, and resolve against BOTH the filename and the frontmatter
  `name:` slug (they do diverge).

  Per this project's standing rule that a detector is worthless until a control
  proves it can distinguish: --self-test injects a synthetic orphan and a
  synthetic dangling link into an in-memory copy of the graph and FAILS if the
  checker does not flag exactly those.  A detector that reports 0 findings on a
  healthy tree and 0 on a poisoned one is vacuous, not clean.

Usage:
  tools/memory_graph_lint.py              # lint, exit 1 if any finding
  tools/memory_graph_lint.py --self-test  # prove the detector can fire
  tools/memory_graph_lint.py --dir PATH   # lint a different memory dir
"""

from __future__ import annotations

import argparse
import glob
import os
import re
import sys

DEFAULT_DIR = os.path.expanduser(
    "~/.claude/projects/-home-free-code-milohax-rb3-xenon/memory"
)

INDEX = "MEMORY.md"

WIKILINK_RE = re.compile(r"\[\[([^\]|]+?)(?:\|[^\]]*)?\]\]")
# A real markdown link REQUIRES the [text](path) form.  Matching a bare
# parenthesized "(foo.md)" instead treats every prose mention of a filename as a
# link -- on first run that manufactured 2 dangling findings out of 3, both of
# them references to docs living in OTHER repos.  Keep the [..] prefix required.
MDLINK_RE = re.compile(r"\[[^\]]*\]\(([A-Za-z0-9_.\-]+\.md)\)")
NAME_RE = re.compile(r"^name:\s*(.+?)\s*$", re.M)


def norm(slug: str) -> str:
    """Collapse the three interchangeable spellings onto one key."""
    return slug.strip().replace("_", "-").lower()


def read_graph(mem_dir: str) -> dict[str, str]:
    return {
        os.path.basename(p): open(p, encoding="utf-8").read()
        for p in sorted(glob.glob(os.path.join(mem_dir, "*.md")))
    }


def analyze(docs: dict[str, str]) -> tuple[list[str], dict[str, list[str]]]:
    """Return (orphans, dangling{target: [sources]})."""
    # A node is addressable by its filename slug AND by its frontmatter name.
    known: set[str] = set()
    for fname, text in docs.items():
        known.add(norm(fname[:-3]))
        m = NAME_RE.search(text)
        if m:
            known.add(norm(m.group(1)))

    refs: set[str] = set()
    dangling: dict[str, list[str]] = {}
    for fname, text in docs.items():
        found = [norm(x) for x in WIKILINK_RE.findall(text)]
        found += [norm(x[:-3]) for x in MDLINK_RE.findall(text)]
        for key in found:
            refs.add(key)
            if key not in known:
                dangling.setdefault(key, []).append(fname)

    orphans = [
        f for f in docs
        if f != INDEX and norm(f[:-3]) not in refs
        and norm(NAME_RE.search(docs[f]).group(1) if NAME_RE.search(docs[f]) else "") not in refs
    ]
    return sorted(orphans), dangling


def self_test(docs: dict[str, str]) -> int:
    """Poison a copy of the graph; the checker must flag exactly the poison."""
    base_orph, base_dang = analyze(docs)
    poisoned = dict(docs)
    poisoned["zz_synthetic_orphan_probe.md"] = (
        "---\nname: zz-synthetic-orphan-probe\n---\n\nUnreferenced on purpose.\n"
    )
    poisoned[INDEX] = poisoned.get(INDEX, "") + "\n- [[zz-synthetic-dangling-probe]]\n"
    # Negative control: a PROSE mention of a file in another repo is not a link
    # and must not be reported.  This is the exact false positive the first
    # version produced (2 of its 3 dangling findings were prose mentions).
    poisoned[INDEX] += "see the writeup (zz-prose-mention-not-a-link.md) for detail\n"

    orph, dang = analyze(poisoned)
    ok = True

    if "zz-prose-mention-not-a-link" in dang:
        print("SELF-TEST FAIL: prose filename mention reported as a dangling link")
        ok = False

    if "zz_synthetic_orphan_probe.md" not in orph:
        print("SELF-TEST FAIL: injected orphan was NOT detected -> checker is vacuous")
        ok = False
    if "zz-synthetic-dangling-probe" not in dang:
        print("SELF-TEST FAIL: injected dangling link was NOT detected -> checker is vacuous")
        ok = False

    # It must also not invent findings beyond the poison + pre-existing ones.
    extra_o = set(orph) - set(base_orph) - {"zz_synthetic_orphan_probe.md"}
    extra_d = set(dang) - set(base_dang) - {"zz-synthetic-dangling-probe"}
    if extra_o or extra_d:
        print(f"SELF-TEST FAIL: spurious findings {extra_o or ''} {extra_d or ''}")
        ok = False

    print("SELF-TEST PASS: detector fires on injected orphan + dangling link, "
          f"and adds nothing spurious (baseline {len(base_orph)} orphans, "
          f"{len(base_dang)} dangling)" if ok else "SELF-TEST FAILED")
    return 0 if ok else 1


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dir", default=DEFAULT_DIR, help="memory directory")
    ap.add_argument("--self-test", action="store_true",
                    help="prove the detector can fire, then exit")
    args = ap.parse_args()

    if not os.path.isdir(args.dir):
        print(f"no such memory dir: {args.dir}", file=sys.stderr)
        return 2

    docs = read_graph(args.dir)
    if not docs:
        print(f"no memories under {args.dir}", file=sys.stderr)
        return 2

    if args.self_test:
        return self_test(docs)

    orphans, dangling = analyze(docs)
    print(f"memories: {len(docs)}   orphans: {len(orphans)}   dangling: {len(dangling)}")

    if orphans:
        print("\nORPHANS (exist but unreachable -- recall will never surface these):")
        for f in orphans:
            print(f"  {f}  ({os.path.getsize(os.path.join(args.dir, f))}B)")
        print("  fix: add a line to the appropriate hub_*.md, not to MEMORY.md")

    if dangling:
        print("\nDANGLING (link target does not exist -- usually a slug typo):")
        for target, srcs in sorted(dangling.items()):
            print(f"  [[{target}]]  <- {', '.join(sorted(set(srcs)))}")
        print("  note: an intentional forward-reference to an unwritten memory is OK")

    return 1 if (orphans or dangling) else 0


if __name__ == "__main__":
    sys.exit(main())
