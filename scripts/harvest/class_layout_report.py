#!/usr/bin/env python3
"""class_layout_report.py — GROUND TRUTH class layout, straight from the compiler.

WHY THIS EXISTS
---------------
Until 2026-07-26 every lane read class layout from two sources that are both
*silently wrong* in places:

  1. the ``// 0xHEX`` comments hand-written in our headers, and
  2. the orchestrator MCP tool ``lookup_struct_offset``, which is **derived from
     those same comments** (``tools/struct_db.py`` parses them into
     ``struct_db.sqlite``).

Measured defects: ``CharEyes.h`` carried 20 wrong offsets; ``SaveLoadManager.h``'s
comments are uniformly +4 stale.  A silently-wrong oracle is worse than a missing
one, so this script replaces both with the only authoritative source: the
compiler that actually lays the class out.

``cl.exe /d1reportSingleClassLayout<Name>`` (an undocumented MSVC flag that
**works through the wibo-wrapped X360 compiler**) prints, for every class whose
name *starts with* ``<Name>``:

  * ``size(N)`` — the real ``sizeof``
  * every member at its real byte offset, nested by base class
  * ``<alignment member> (size=k)`` rows — i.e. **padding, explicitly labelled**
  * the vtable (``Class::$vftable@:``) slot by slot, with the class that actually
    supplies each slot
  * ``this adjustor`` values per virtual — what a multiple-inheritance call site
    subtracts from ``r3``

That last pair is what makes it strictly better than the header comments: it
covers vtables and sub-object adjustment, which comments never encoded at all.

TYPICAL USES
------------
    # everything the compiler knows about one class
    python3 scripts/harvest/class_layout_report.py Synth

    # only that exact class, not every Synth* in the TU
    python3 scripts/harvest/class_layout_report.py Synth --exact

    # "which field is at 0x118?"  -- the lookup_struct_offset question,
    # answered against the compiler instead of against stale comments
    python3 scripts/harvest/class_layout_report.py BandCharacter --offset 0x118

    # force the translation unit (needed when the class lives in a header that
    # the same-stem .cpp does not include, or is not wired in objects.json)
    python3 scripts/harvest/class_layout_report.py SongPreview --tu src/system/meta/MetaPanel.cpp

    # machine-readable, for tools/skills
    python3 scripts/harvest/class_layout_report.py Synth --json

    # audit a header's `// 0xHEX` comments against the compiler
    python3 scripts/harvest/class_layout_report.py Synth --check-header

HOW IT FINDS A TU
-----------------
The flag is a *compile* flag, so we need a translation unit in which the class is
complete.  Resolution order:

  1. ``--tu`` if given.
  2. the header that declares the class (``class X``/``struct X`` in ``src/**``),
     then the same-directory same-stem ``.cpp`` if that file is compiled by
     ``build.ninja``.
  3. any compiled ``.cpp`` that ``#include``s that header directly (smallest
     first — cheapest compile).

The compile command is lifted verbatim from ``ninja -t commands`` for the chosen
object, so the include path, PCH, defines and ``/O1 /Oi /GR /EHsc`` all match the
real build exactly.  Three edits are applied: ``/showIncludes`` is dropped,
``/Fo`` is redirected to a scratch file, and the objcache/exec prefix is stripped
(the report goes to stdout, which objcache neither captures nor replays).  The
real build's objects are never touched, so this is safe to run concurrently with
a build in the same worktree.

CAVEATS
-------
  * The flag is a **prefix** match: ``...LayoutSynth`` also reports
    ``SynthSample`` (and ``SynthPollMe``, ...).  Use ``--exact`` to filter.
  * A class that the TU never *completes* (forward-declared only, or entirely
    unreferenced with all-inline members) produces **no output** — that is the
    ``SaveLoadManager`` case recorded in
    ``docs/plans/slm-setstate-reconstruction.md``.  Pass a ``--tu`` that really
    instantiates it.
  * Templates report per-instantiation, so ``vector<Foo*>`` shows up under its
    mangled instantiation name, not as ``vector``.
  * ★ For a class declared in a header shared by several TUs (``FlowSwitch``,
    ``PropertyEventListener``, ...) auto-resolution can land on a TU where the
    class is not complete — **pass ``--tu`` explicitly** rather than concluding
    the class does not exist.

★★ THE THREE-LABEL CONTRACT (lane DL-2, 2026-08-03)
---------------------------------------------------
This tool used to have **two** outcomes where the world has **three**, and the
collapse produced a *false answer*, not a missing one::

    OK             the compiler reported the class -> layout/adjustors are real
    CLASS_ABSENT   the compiler ran CLEAN but did not report this class
                   (forward-declared only / unreferenced / wrong TU).  A REAL
                   answer -- "not here" -- and legitimately empty.
    COMPILE_FAILED the compiler could not answer at all.  NEVER a zero.

⚠ AND IT IS SLOW ENOUGH THAT A SHORT TIMEOUT IMPERSONATES ``CLASS_ABSENT``.
The compile prints **nothing** until the TU finishes: measured ~10 min for
``BandSongMgr`` and up to ~50 min under fleet load.  A caller that kills it
early sees no class in the output and cannot distinguish that from the
legitimate "not here" answer above -- i.e. the three-label contract is only
honoured if you actually let the compiler finish.  **Budget 60 min.**
(Recorded 2026-07-26 in ``docs/plans/lane-ah-layout-oracle-2026-07-26.md``;
promoted here 2026-08-18 because a trap filed only in a dated lane write-up is
invisible to the person running the tool.)

The defect that forced this (reproduced before the fix): the compile command is
lifted verbatim from ninja, so for the **nine PCH-eligible engine dirs**
(``flow synth meta obj os utl movie hamobj gesture``) it carries
``/Yu"decomp_pch.h"`` and the layout compile dies with::

    fatal error C1094: '-Zm100' : command line option is inconsistent with
    value used to build precompiled header ('-Zm0')

``--json`` mode then emitted ``{"classes": {}, "adjustors": {}}`` **and exited
0** -- indistinguishable from "this class has no members and no adjustors".
Every ``Flow*``/``Meta*``/``Ham*``/``Obj*``/``Utl*``/``Os*``/``Synth*``/
``Movie*``/``Gesture*`` class read a silent false zero; lane DK-1 caught it only
because ``FlowSwitch`` read NONE where a prior lane had measured ``-120``.

Fix: force ``/Y-`` (ignore all PCH options) into the layout compile, **after the
guest program name** -- inserting it at ``argv[1]`` makes wibo try to ``exec``
``/Y-`` itself.  And make the three labels *structural*: ``--json`` carries a
``status`` key and the exit code is **0 / 1 / 3**, never 0-for-failure.

``--selftest`` proves all of it, and ``--selftest --sabotage pch`` restores the
PCH-carrying command line **and points ``/Fp`` at a file that cannot exist**, so
the PCH is unusable regardless of today's on-disk state.  ``/Y-`` ignores
``/Fp`` outright, so the FIXED tool is immune and the UNFIXED one always fails
=> the sabotage discriminates the fix DETERMINISTICALLY, and MUST fail (rule 2:
a control that only checks the happy path is vacuous).

⚠⚠ **THE C1094 IS INTERMITTENT, WHICH MAKES THE DEFECT WORSE, NOT BETTER.**
Measured 2026-08-03: it fires only while the **on-disk PCH disagrees with the
layout compile's command shape** -- the state a **fresh reflinked worktree
starts in**, because ``setup_worktree.sh`` reflinks main's ``build/45410914/``.
Run a local full ``ninja`` and the PCH is rebuilt, after which the ``/Yu`` path
*succeeds* and the false zero disappears until the next fresh worktree.  An
earlier draft of the regression control re-ran that compile and went **red after
an unrelated rebuild**.  So the committed pin (leg D) is **offline**: it feeds
`classify()` the verbatim captured C1094 transcript and the identical *empty*
parse with a *clean* transcript, and requires COMPILE_FAILED(3) vs
CLASS_ABSENT(1) -- same parsed content, different label.  The live PCH probe
survives as leg **E, informational, never gating**.
⇒ A tool that returns a false zero only *sometimes* is harder to catch than one
that always does; ``/Y-`` makes this tool immune to **both** PCH states.

COST
----
``/Y-`` means one PCH-less TU compile per class (~14 s here).  Use
``--all-classes`` (``/d1reportAllClassLayout``) to get **every** class in the TU
from a **single** compile -- measured 700 classes / 51 adjustor sets in 13.8 s,
against 700 separate compiles.
"""

import argparse
import json
import os
import re
import shlex
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


# ---------------------------------------------------------------- TU discovery

def _ninja_targets(project_dir):
    """All object targets known to build.ninja, mapped source -> object."""
    out = subprocess.run(
        ["ninja", "-t", "targets", "all"],
        cwd=project_dir, capture_output=True, text=True,
    ).stdout
    objs = {}
    for line in out.splitlines():
        tgt = line.split(":", 1)[0].strip()
        if tgt.endswith(".obj") and "/45410914/src/" in tgt:
            objs[tgt] = True
    return list(objs)


def _obj_for_source(project_dir, src_rel):
    """build/45410914/<src path>.obj, if build.ninja knows it."""
    cand = os.path.join("build", "45410914", os.path.splitext(src_rel)[0] + ".obj")
    probe = subprocess.run(
        ["ninja", "-t", "commands", cand],
        cwd=project_dir, capture_output=True, text=True,
    )
    if probe.returncode == 0 and "cl.exe" in probe.stdout:
        return cand
    return None


def find_header(project_dir, cls):
    """Header declaring `class cls` / `struct cls`."""
    pat = r"^\s*(?:class|struct)\s+(?:[A-Za-z_]+\s+)?" + re.escape(cls) + r"\s*(?::|\{|$)"
    hits = []
    for root in ("src",):
        r = subprocess.run(
            ["grep", "-rlP", pat, "--include=*.h", "--include=*.hpp", os.path.join(project_dir, root)],
            capture_output=True, text=True,
        )
        hits += [os.path.relpath(p, project_dir) for p in r.stdout.split()]
    # prefer the header whose stem == class name, then shortest path
    hits.sort(key=lambda p: (os.path.splitext(os.path.basename(p))[0] != cls, len(p)))
    return hits


def resolve_tu(project_dir, cls, verbose=False):
    """Pick a compiled .cpp in which `cls` is complete."""
    headers = find_header(project_dir, cls)
    if verbose:
        print(f"# headers declaring {cls}: {headers[:5]}", file=sys.stderr)

    # (2) same-dir same-stem .cpp of the declaring header
    for h in headers:
        for ext in (".cpp", ".c"):
            src = os.path.splitext(h)[0] + ext
            if os.path.exists(os.path.join(project_dir, src)):
                obj = _obj_for_source(project_dir, src)
                if obj:
                    return src, obj

    # (3) a compiled .cpp that includes the header directly
    for h in headers:
        base = os.path.basename(h)
        r = subprocess.run(
            ["grep", "-rl", f'#include "{h[len("src/"):] if h.startswith("src/") else h}"',
             "--include=*.cpp", os.path.join(project_dir, "src")],
            capture_output=True, text=True,
        )
        cands = [os.path.relpath(p, project_dir) for p in r.stdout.split()]
        if not cands:
            r = subprocess.run(
                ["grep", "-rl", f'#include "{base}"', "--include=*.cpp",
                 os.path.join(project_dir, "src")],
                capture_output=True, text=True,
            )
            cands = [os.path.relpath(p, project_dir) for p in r.stdout.split()]
        cands.sort(key=lambda p: os.path.getsize(os.path.join(project_dir, p)))
        for src in cands:
            obj = _obj_for_source(project_dir, src)
            if obj:
                return src, obj

    return None, None


# ------------------------------------------------------------------ the invoke

def build_command(project_dir, obj, cls, scratch_obj, use_pch=False, all_classes=False,
                  break_pch=False):
    """The real compile command, minus caching, plus the layout flag.

    ``use_pch=False`` (the default, and the DL-2 fix) strips ``/Yu`` / ``/Yc`` /
    ``/Fp`` and forces ``/Y-``.  Without it the nine PCH-eligible engine dirs
    die with ``C1094: '-Zm100' inconsistent with ... ('-Zm0')`` and this tool
    used to report that as an empty layout.  ``use_pch=True`` exists ONLY so
    ``--selftest --sabotage pch`` can reproduce the defect and prove the
    detector fires.
    """
    r = subprocess.run(["ninja", "-t", "commands", obj],
                       cwd=project_dir, capture_output=True, text=True)
    if r.returncode != 0:
        raise SystemExit(f"ninja -t commands failed for {obj}:\n{r.stderr}")
    line = None
    for cand in r.stdout.splitlines():
        if "cl.exe" in cand and obj in cand:
            line = cand
    if line is None:
        raise SystemExit(f"no cl.exe edge found for {obj}")

    argv = shlex.split(line)
    # strip everything up to and including the objcache `exec ... --` prefix
    if "exec" in argv and "--" in argv:
        argv = argv[argv.index("--") + 1:]
    # drop leading VAR=VAL env assignments (re-applied below)
    env = dict(os.environ)
    while argv and re.match(r"^[A-Z_][A-Z0-9_]*=", argv[0]):
        k, v = argv[0].split("=", 1)
        env[k] = v
        argv = argv[1:]

    out = []
    for a in argv:
        if a == "/showIncludes":
            continue
        if a.startswith("/Fo"):
            out.append("/Fo" + scratch_obj)
            continue
        # ---- DL-2: neutralise the PCH, which the layout compile cannot honour.
        if not use_pch and (a.startswith("/Yu") or a.startswith("/Yc")
                            or a.startswith("/Fp")):
            continue
        # `--sabotage pch` DETERMINISM: point /Fp at a file that cannot exist, so
        # the PCH is unusable regardless of whether the on-disk one happens to be
        # consistent right now.  This reproduces the STATE DK-1 met (PCH not
        # usable by the layout compile) rather than the particular -Zm symptom,
        # which is not reproducible on demand.  `/Y-` ignores /Fp entirely, so
        # the FIXED tool is immune and the UNFIXED tool always fails => the
        # sabotage discriminates the fix, deterministically.
        if break_pch and a.startswith("/Fp"):
            out.append("/Fpthis-pch-does-not-exist-dl2.pch")
            continue
        out.append(a)
    if not use_pch:
        # ⚠ AFTER the guest program name.  `out[0]` is the objcache/wibo wrapper
        # and the cl.exe element follows it; putting `/Y-` at index 1 makes wibo
        # try to exec "/Y-" as the program.
        i = next((n for n, a in enumerate(out) if a.lower().endswith("cl.exe")), 0)
        out.insert(i + 1, "/Y-")

    if all_classes:
        # One compile, every class in the TU.  The flag takes no operand, so the
        # empty-name ICE below cannot apply to it.
        out.insert(-1, "/d1reportAllClassLayout")
        return out, env

    # A BARE `/d1reportSingleClassLayout` (empty `cls`) makes the MSVC front end
    # dereference a NULL class-name string and die.  Verified 2026-07-30 (lane BR-1):
    # under wine the same input yields a clean
    #   `c1xx : fatal error C1001: An internal error has occurred in the compiler
    #    (compiler file 'msc1.cpp', line 1420)`
    # but under wibo -- which has no SEH, so the guest's own handler never runs --
    # it is a hard SIGSEGV (exit 139) with an unattributable coredump.  This was the
    # mechanism behind the documented "class_layout_report.py silently emits NOTHING
    # while exiting 0" trap.  Refuse to emit the flag without a name.
    if not cls or not re.match(r"^[A-Za-z_][A-Za-z0-9_]*$", cls):
        raise SystemExit(
            f"refusing to run: class name {cls!r} is empty or not a plain identifier.\n"
            f"A bare /d1reportSingleClassLayout crashes cl.exe (MSVC C1001 ICE; hard\n"
            f"SIGSEGV under wibo). Pass a real class name, or use --all-classes-flag\n"
            f"semantics via /d1reportAllClassLayout if you want every class.")
    out.insert(-1, "/d1reportSingleClassLayout" + cls)
    return out, env


# ------------------------------------------------------- the three-label gate
#
# A tool that cannot distinguish "the answer is zero" from "I could not answer"
# is the failure shape lane DL-2 exists to remove.  Keep these THREE labels
# apart structurally -- in the exit code and in the JSON -- never by prose.
OK = "OK"                          # class reported; layout is real
CLASS_ABSENT = "CLASS_ABSENT"      # clean compile, class not in this TU
COMPILE_FAILED = "COMPILE_FAILED"  # compiler could not answer -- NOT a zero

EXIT_FOR = {OK: 0, CLASS_ABSENT: 1, COMPILE_FAILED: 3}

# `c1xx : fatal error C1094:`, `foo.cpp(12) : error C2065:`, `LNK1104`, ...
RE_DIAG = re.compile(r"^.*?\b((?:fatal )?error\s+[A-Z]+\d+)\s*:\s*(.*)$", re.M)


def diagnostics(text):
    """Every compiler/linker diagnostic in the output (empty == clean run)."""
    return [f"{m.group(1)}: {m.group(2).strip()}" for m in RE_DIAG.finditer(text)]


def classify(text, parsed, cls, exact=True):
    """-> (status, diagnostics).  The whole point of this function is that
    COMPILE_FAILED and CLASS_ABSENT can never collapse into each other."""
    diags = diagnostics(text)
    if diags:
        # A diagnostic means the front end never laid the class out.  Even if
        # some earlier class parsed, the report is untrustworthy -- refuse.
        return COMPILE_FAILED, diags
    if cls is None:                     # --all-classes: no single class asked for
        return (OK if parsed["classes"] else CLASS_ABSENT), diags
    keys = [k for k in parsed["classes"]
            if k == cls or k.endswith("::" + cls)] if exact \
        else list(parsed["classes"])
    return (OK if keys else CLASS_ABSENT), diags


def run_report(project_dir, cls, tu=None, verbose=False,
               use_pch=False, all_classes=False, break_pch=False):
    if tu:
        obj = _obj_for_source(project_dir, tu)
        if not obj:
            raise SystemExit(f"{tu} has no compile edge in build.ninja "
                             f"(add it to config/45410914/objects.json first)")
        src = tu
    else:
        if not cls:
            raise SystemExit("--all-classes without a class name needs an "
                             "explicit --tu (there is nothing to resolve on).")
        src, obj = resolve_tu(project_dir, cls, verbose)
        if not src:
            raise SystemExit(
                f"could not find a compiled TU in which '{cls}' is complete.\n"
                f"pass --tu <path.cpp> explicitly.")
    with tempfile.TemporaryDirectory(dir=os.path.expanduser("~/tmp")) as td:
        argv, env = build_command(project_dir, obj, cls, os.path.join(td, "layout.obj"),
                                  use_pch=use_pch, all_classes=all_classes,
                                  break_pch=break_pch)
        if verbose:
            print("# " + " ".join(shlex.quote(a) for a in argv), file=sys.stderr)
        p = subprocess.run(argv, cwd=project_dir, capture_output=True, text=True, env=env)
    text = p.stdout + p.stderr
    # NEVER fail silently.  This return code used to be discarded, which is why a
    # crashed cl.exe surfaced as "emits nothing, exits 0" instead of an error.
    # subprocess reports signal death as a negative return code; a shell in between
    # would translate it to 128+N.
    signum = -p.returncode if p.returncode < 0 else (
        p.returncode - 128 if 128 < p.returncode < 160 else 0)
    if signum:
        raise SystemExit(
            f"cl.exe died from signal {signum} while reporting the layout of '{cls}'\n"
            f"(TU {src}).  This is an MSVC front-end crash (c1xx.dll), which real\n"
            f"Windows/wine reports as 'fatal error C1001: internal compiler error';\n"
            f"wibo has no SEH so it becomes a hard SIGSEGV.\n"
            f"Re-run with --verbose to get the exact command, and capture a\n"
            f"self-contained reproducer with tools/clcrash_capture.py.\n"
            f"--- compiler output ---\n{text.strip()[:2000]}")
    return src, text


# --------------------------------------------------------------------- parsing

RE_CLASS = re.compile(r"^(?:class|struct|union)\s+(\S+)\s+size\((\d+)\):")
RE_ROW = re.compile(r"^\s*(\d+|)\s*((?:\|\s*)+|\+---.*)(.*)$")
RE_BASE_OPEN = re.compile(r"^\+---\s*\((?:base class|virtual base|vtordisp for vbase)\s+(\S+?)\)")
RE_VFT = re.compile(r"^(\S+)::\$vf(?:table|tables?)@?(\S*)@?:")
RE_ADJ = re.compile(r"^(\S+)::(\S+) this adjustor: (\d+)")


def _split_row(line):
    """(offset_or_None, bar_depth, body) for one report row.

    The report encodes base-class nesting with leading `|` bars:

         0      | | | {vfptr}          <- depth 3
                | | +---
         4      | | TypeProps mTypeProps
    """
    m = re.match(r"^\s*(\d+)?\s*((?:\|\s*)*)(.*)$", line)
    if not m:
        return None, 0, ""
    off = int(m.group(1)) if m.group(1) else None
    depth = m.group(2).count("|")
    return off, depth, m.group(3).strip()


def parse(text):
    """-> {classes: {name: {...}}, vtables: {...}, adjustors: {...}}"""
    classes, vtables, adjustors = {}, {}, {}
    cur = None            # current class dict
    base_at_depth = {}    # bar-depth -> base class name opened there
    vft = None

    for raw in text.splitlines():
        line = raw.rstrip()
        if not line.strip():
            continue
        m = RE_CLASS.match(line)
        if m:
            cur = {"name": m.group(1), "size": int(m.group(2)),
                   "members": [], "padding": []}
            classes[m.group(1)] = cur
            base_at_depth = {}
            vft = None
            continue

        m = RE_VFT.match(line)
        if m:
            vft = []
            key = m.group(1) + (("@" + m.group(2)) if m.group(2) else "")
            vtables[key] = vft
            cur = None
            continue

        m = RE_ADJ.match(line)
        if m:
            adjustors.setdefault(m.group(1), {})[m.group(2)] = int(m.group(3))
            continue

        off, depth, body = _split_row(line)

        if vft is not None:
            if off is not None:
                vft.append({"slot": off, "target": body.lstrip("&")})
            continue

        if cur is None or not body:
            continue

        bm = RE_BASE_OPEN.match(body)
        if bm:
            # this `+---` sits one level deeper than its bars
            base_at_depth[depth + 1] = bm.group(1)
            continue
        if body.startswith("+---"):
            base_at_depth.pop(depth + 1, None)
            continue

        scope = [base_at_depth[d] for d in sorted(base_at_depth) if d <= depth]

        if "<alignment member>" in body:
            sz = re.search(r"size=(\d+)", body)
            cur["padding"].append({
                "after_offset": cur["members"][-1]["offset"] if cur["members"] else 0,
                "size": int(sz.group(1)) if sz else None,
                "in_base": scope})
            continue

        if off is None:
            continue

        parts = body.split()
        name = parts[-1]
        typ = " ".join(parts[:-1]) if len(parts) > 1 else ""
        cur["members"].append({
            "offset": off, "name": name, "type": typ,
            "in_base": scope,
            "is_vfptr": name.startswith("{"),
        })

    return {"classes": classes, "vtables": vtables, "adjustors": adjustors}


# --------------------------------------------------------------- header audit

RE_HDR_COMMENT = re.compile(r"//\s*0x([0-9A-Fa-f]+)")


def audit_header(project_dir, header, cls_info):
    """Compare `// 0xHEX` trailing comments in a header to the compiler's truth.

    Returns list of (line_no, member, commented_offset, real_offset).
    """
    path = os.path.join(project_dir, header)
    if not os.path.exists(path):
        return []
    real = {}
    for m in cls_info["members"]:
        real.setdefault(m["name"].lstrip("*&"), m["offset"])
    bad = []
    with open(path, errors="replace") as fh:
        for i, line in enumerate(fh, 1):
            cm = RE_HDR_COMMENT.search(line)
            if not cm:
                continue
            decl = line[:cm.start()]
            ids = re.findall(r"([A-Za-z_][A-Za-z0-9_]*)\s*(?:\[[^\]]*\])?\s*;", decl)
            if not ids:
                continue
            name = ids[-1]
            if name in real and real[name] != int(cm.group(1), 16):
                bad.append((i, name, int(cm.group(1), 16), real[name]))
    return bad


# ------------------------------------------------------------------- printing

def emit(parsed, cls, exact, offset_query, raw_text, show_vtable):
    classes = parsed["classes"]
    keys = [k for k in classes if (k == cls or k.endswith("::" + cls))] if exact \
        else list(classes)
    if not keys:
        if exact:
            print(f"!! '{cls}' not reported (prefix hits: {sorted(classes)[:8]})")
        else:
            print("!! compiler emitted no layout report -- the class is probably "
                  "incomplete in this TU; pass a different --tu")
        return 1

    for k in keys:
        c = classes[k]
        print(f"\n=== {k}   sizeof = {c['size']} (0x{c['size']:x}) ===")
        pad_by_off = {}
        for p in c["padding"]:
            pad_by_off.setdefault(p["after_offset"], 0)
            pad_by_off[p["after_offset"]] += p["size"] or 0
        for m in c["members"]:
            scope = ("  [" + " > ".join(m["in_base"]) + "]") if m["in_base"] else ""
            print(f"  0x{m['offset']:<5x} {m['offset']:<6} {m['type'] + ' ' if m['type'] else ''}{m['name']}{scope}")
            if m["offset"] in pad_by_off:
                print(f"  {'':<12} <padding {pad_by_off[m['offset']]} byte(s)>")
        if offset_query is not None:
            best = None
            for m in c["members"]:
                if m["offset"] <= offset_query and (best is None or m["offset"] > best["offset"]):
                    best = m
            if best:
                d = offset_query - best["offset"]
                print(f"  -> 0x{offset_query:x} is {best['name']}"
                      + (f" + {d}" if d else "")
                      + (f"   (in {' > '.join(best['in_base'])})" if best["in_base"] else ""))
            else:
                print(f"  -> 0x{offset_query:x}: no member at or below that offset")

    if show_vtable:
        for name, slots in parsed["vtables"].items():
            if exact and not name.split("@")[0].endswith(cls):
                continue
            print(f"\n=== vtable {name} ({len(slots)} slots) ===")
            for s in slots:
                print(f"  [{s['slot']:>3}] {s['target']}")
        for name, adj in parsed["adjustors"].items():
            nz = {k2: v for k2, v in adj.items() if v}
            if nz:
                print(f"\n=== {name} nonzero `this` adjustors ===")
                for k2, v in sorted(nz.items(), key=lambda kv: -kv[1]):
                    print(f"  {k2}: -{v}")
    return 0


# ============================================================================
# CONTROLS
#
# INSTRUMENT_DESIGN rules 1, 2, 4 and 6 made runnable:
#   1  a known-positive is asserted BEFORE any zero from this tool is believed;
#   2  the empty-check is PAIRED with a deliberately sabotaged leg (leg D always
#      runs, so a revert of the /Y- fix fails a plain --selftest);
#   4  the classifier is shown producing a SECOND label (CLASS_ABSENT) on a
#      known-opposite case -- a one-label classifier is not believed;
#   6  a failure REFUSES (exit 3) rather than returning a misleading zero.
# ============================================================================

#: `__delDtor` this-adjustor, the value lanes actually consume as "K".
def _delDtor_K(parsed, cls):
    return (parsed["adjustors"].get(cls) or {}).get("__delDtor")


def _probe(project_dir, cls, tu=None, use_pch=False, verbose=False, break_pch=False):
    """-> (status, diagnostics, K, parsed, text).  Never raises for a compiler fault.

    ⚠ MEASURED 2026-08-03, and it is the reason the diagnostic is the ONLY
    admissible discriminator: for a prefix that matches nothing the compiler
    reports **no classes at all**, so ``classes == {}`` is byte-for-byte the
    same shape as the C1094 failure.  "Did it produce output?" therefore cannot
    separate CLASS_ABSENT from COMPILE_FAILED -- only "did it emit a
    diagnostic?" can.  Any future refactor that tries to infer status from the
    parsed content instead re-introduces the false zero.
    """
    try:
        _src, text = run_report(project_dir, cls, tu, verbose, use_pch=use_pch,
                                break_pch=break_pch)
    except SystemExit as e:
        return COMPILE_FAILED, [str(e)[:200]], None, {"classes": {}, "adjustors": {}}, ""
    parsed = parse(text)
    status, diags = classify(text, parsed, cls, exact=True)
    return status, diags, _delDtor_K(parsed, cls), parsed, text


def run_selftest(project_dir, sabotage=None, verbose=False):
    """`--selftest` must PASS on the fixed tool and FAIL under `--sabotage pch`."""
    use_pch = (sabotage == "pch")
    print(f"class_layout_report selftest  project_dir={project_dir}"
          + (f"  SABOTAGE={sabotage}" if sabotage else ""))
    if sabotage:
        print("  (sabotage leg: this run is EXPECTED to FAIL -- it proves the "
              "selftest is not vacuous)")
    rows = []

    def chk(name, ok, detail):
        rows.append((name, bool(ok), detail))

    # -- A  KNOWN POSITIVE, IN A PCH DIR.  This is the exact reading the
    #       unpatched tool lost: FlowSwitch lives in src/system/flow, one of the
    #       nine PCH-eligible dirs, and read NONE where DK-1 measured -120.
    st, dg, k, _p, _t = _probe(project_dir, "FlowSwitch", use_pch=use_pch,
                               verbose=verbose, break_pch=use_pch)
    chk("A. PCH-dir known positive: FlowSwitch K == 0x78",
        st == OK and k == 0x78,
        f"status={st} K={k if k is None else hex(k)} diags={dg[:2]} (want OK / 0x78)")

    # -- B  NON-PCH CONTROL.  src/system/char is excluded from the PCH, so this
    #       path worked BEFORE the fix; it proves /Y- did not break it.
    st, dg, k, _p, _t = _probe(project_dir, "CharHair", use_pch=use_pch,
                               verbose=verbose, break_pch=use_pch)
    chk("B. non-PCH control: CharHair K == 0x7c",
        st == OK and k == 0x7C,
        f"status={st} K={k if k is None else hex(k)} diags={dg[:2]} (want OK / 0x7c)")

    # -- C  THE SECOND LABEL (rule 4).  A clean compile that simply does not
    #       report the class must read CLASS_ABSENT -- a REAL "not here" answer,
    #       with NO diagnostics -- and must not be confused with a failure.
    st, dg, _k, p, text = _probe(project_dir, "ZzzNoSuchClassDL2",
                                 tu="src/system/flow/FlowSwitch.cpp",
                                 use_pch=use_pch, verbose=verbose,
                                 break_pch=use_pch)
    # cl.exe echoes the source file name when it actually runs, so "banner
    # present AND zero diagnostics" is what makes this absence a REAL answer
    # rather than a crash.  (The parsed content cannot tell them apart -- see
    # _probe's docstring.)
    chk("C. clean compile, class absent -> CLASS_ABSENT (not COMPILE_FAILED)",
        st == CLASS_ABSENT and not dg and "FlowSwitch.cpp" in text,
        f"status={st} diags={dg[:2]} compiler_ran={'FlowSwitch.cpp' in text} "
        f"classes_seen={len(p['classes'])} (want CLASS_ABSENT / no diags / ran)")

    # -- D  ★ THE REGRESSION PIN, ALWAYS ON, AND DELIBERATELY OFFLINE.
    #
    #       This pins the actual bug: **an empty parse PLUS a diagnostic must
    #       classify COMPILE_FAILED (exit 3), never CLASS_ABSENT and never OK.**
    #       It runs `classify()` against the VERBATIM C1094 transcript captured
    #       from the real failure, so it is deterministic.
    #
    #       ⚠ WHY NOT JUST RE-RUN THE PCH COMPILE:  measured 2026-08-03, the
    #       C1094 IS NOT REPRODUCIBLE ON DEMAND.  It fires only while the
    #       on-disk PCH disagrees with the layout compile's command shape --
    #       the state a FRESH REFLINKED WORKTREE starts in, since
    #       setup_worktree.sh reflinks main's build/45410914/.  A local full
    #       `ninja` rebuilds the PCH and the /Yu path then SUCCEEDS.  An earlier
    #       draft of this leg did re-run the compile and went RED after an
    #       unrelated rebuild.  That makes the defect INTERMITTENT, which is
    #       strictly worse than permanent: a tool that returns a false zero only
    #       sometimes is harder to catch.  Leg E below still probes the live
    #       environment, but INFORMATIONALLY -- it must never gate.
    C1094_TRANSCRIPT = (
        "FlowSwitch.cpp\n"
        "src/system/flow/FlowSwitch.cpp : fatal error C1094: '-Zm100' : command "
        "line option is inconsistent with value used to build precompiled "
        "header ('-Zm0')\n")
    empty = {"classes": {}, "vtables": {}, "adjustors": {}}
    st_f, dg_f = classify(C1094_TRANSCRIPT, empty, "FlowSwitch", exact=True)
    # and the mirror: the SAME empty parse with a CLEAN transcript is the real
    # "not here" answer.  Identical parsed content, different label -- which is
    # the whole three-label contract in one assertion.
    st_c, dg_c = classify("FlowSwitch.cpp\n", empty, "FlowSwitch", exact=True)
    chk("D. REGRESSION PIN: diagnostic+empty => COMPILE_FAILED(3); "
        "clean+empty => CLASS_ABSENT(1)",
        (st_f == COMPILE_FAILED and EXIT_FOR[st_f] == 3
         and any("C1094" in d for d in dg_f)
         and st_c == CLASS_ABSENT and EXIT_FOR[st_c] == 1 and not dg_c),
        f"C1094-transcript -> {st_f}/exit {EXIT_FOR[st_f]} diags={len(dg_f)}; "
        f"clean-transcript -> {st_c}/exit {EXIT_FOR[st_c]} diags={len(dg_c)}; "
        f"(want COMPILE_FAILED/3/>=1 and CLASS_ABSENT/1/0 -- SAME empty parse, "
        f"different label)")

    # -- E  INFORMATIONAL ONLY, NEVER GATES.  What does the live PCH path do
    #       right now?  Both outcomes are legitimate (see leg D's note), so this
    #       reports rather than judges -- a leg that cannot fail must not be
    #       allowed to pass either.
    st_e, dg_e, k_e, _p, _t = _probe(project_dir, "FlowSwitch", use_pch=True,
                                     verbose=verbose)
    live = ("reproduces the C1094 (PCH disagrees with the layout compile)"
            if any("C1094" in d for d in dg_e)
            else f"PCH currently CONSISTENT -- /Yu path succeeds (status={st_e}, "
                 f"K={k_e if k_e is None else hex(k_e)})")
    print(f"  [INFO] E. live PCH path: {live}")
    print( "         Not a control: this flips with the on-disk PCH.  /Y- makes "
           "the tool\n         immune to BOTH states, which is the point.")

    nfail = 0
    for name, ok, detail in rows:
        print(f"  [{'PASS' if ok else 'FAIL'}] {name}\n         {detail}")
        nfail += (not ok)
    print(f"  {len(rows) - nfail}/{len(rows)} controls passed")
    return 1 if nfail else 0


def main():
    ap = argparse.ArgumentParser(
        description="Authoritative class layout from cl.exe "
                    "/d1reportSingleClassLayout (replaces stale // 0xHEX comments).")
    ap.add_argument("cls", nargs="?", default=None,
                    help="class name (matched as a PREFIX by the compiler); "
                         "optional with --selftest or --all-classes --tu")
    ap.add_argument("--tu", help="translation unit to compile (default: auto-resolve)")
    ap.add_argument("--project-dir", default=os.environ.get("RB3_PROJECT_DIR", REPO),
                    help="worktree to compile in (default: this repo)")
    ap.add_argument("--exact", action="store_true",
                    help="report only the exactly-named class")
    ap.add_argument("--offset", help="answer 'which member is at this offset?'")
    ap.add_argument("--no-vtable", action="store_true", help="skip vtable/adjustor dump")
    ap.add_argument("--raw", action="store_true", help="dump the compiler output verbatim")
    ap.add_argument("--json", action="store_true", help="emit parsed JSON")
    ap.add_argument("--check-header", action="store_true",
                    help="audit the declaring header's // 0xHEX comments against truth")
    ap.add_argument("--fix-header", action="store_true",
                    help="rewrite wrong // 0xHEX comments in place (comments only, "
                         "never code). Implies --check-header.")
    ap.add_argument("-v", "--verbose", action="store_true")
    ap.add_argument("--all-classes", action="store_true",
                    help="/d1reportAllClassLayout: EVERY class in the TU from ONE "
                         "compile (13.8s / 700 classes measured, vs one compile "
                         "per class)")
    ap.add_argument("--selftest", action="store_true",
                    help="run the three-label controls; non-zero exit on failure")
    ap.add_argument("--sabotage", choices=["pch"], default=None,
                    help="restore the PCH-carrying command line that produced the "
                         "silent false zero (vacuity control: --selftest MUST fail)")
    args = ap.parse_args()

    if args.selftest:
        return run_selftest(args.project_dir, args.sabotage, args.verbose)

    off = int(args.offset, 16) if args.offset and args.offset.lower().startswith("0x") \
        else (int(args.offset) if args.offset else None)

    src, text = run_report(args.project_dir, args.cls, args.tu, args.verbose,
                           use_pch=(args.sabotage == "pch"),
                           all_classes=args.all_classes)
    if args.raw:
        print(text)
        return 0
    parsed = parse(text)
    # ★ THE THREE-LABEL GATE.  Computed once, reported in BOTH output modes, and
    # carried in the exit code -- a consumer must never have to infer "did the
    # compiler actually answer?" from an empty dict.
    status, diags = classify(text, parsed,
                             None if args.all_classes else args.cls, args.exact)
    if args.json:
        parsed["_tu"] = src
        parsed["status"] = status
        parsed["diagnostics"] = diags
        parsed["_note"] = ("status=COMPILE_FAILED means the compiler could not "
                           "answer; it is NOT 'no members / no adjustors'.")
        print(json.dumps(parsed, indent=2))
        return EXIT_FOR[status]

    flag = ("/d1reportAllClassLayout" if args.all_classes
            else "/d1reportSingleClassLayout" + args.cls)
    print(f"# class layout for '{args.cls}' via TU {src}  (cl.exe {flag})")
    print(f"# STATUS: {status}")
    if status == COMPILE_FAILED:
        print("!! REFUSING TO REPORT A LAYOUT -- the compiler emitted a diagnostic,")
        print("!! so an empty report here would be a FALSE ZERO, not an answer.")
        for d in diags:
            print(f"!!   {d}")
        print("# --- compiler said: ---")
        print(text[:2000])
        return EXIT_FOR[status]
    if not parsed["classes"] and text.strip():
        print("# --- compiler said: ---")
        print(text[:2000])
    rc = emit(parsed, args.cls, args.exact, off, text, not args.no_vtable)
    rc = EXIT_FOR[status] if status != OK else rc

    if args.check_header or args.fix_header:
        hdrs = find_header(args.project_dir, args.cls)
        info = parsed["classes"].get(args.cls)
        if hdrs and info:
            bad = audit_header(args.project_dir, hdrs[0], info)
            print(f"\n=== header comment audit: {hdrs[0]} ===")
            if not bad:
                print("  all // 0xHEX comments agree with the compiler")
            for ln, name, got, real in bad:
                print(f"  WRONG line {ln}: {name} commented 0x{got:x} but is really 0x{real:x}")
            if args.fix_header and bad:
                path = os.path.join(args.project_dir, hdrs[0])
                lines = open(path, errors="replace").read().split("\n")
                for ln, name, got, real in bad:
                    old = lines[ln - 1]
                    lines[ln - 1] = RE_HDR_COMMENT.sub(
                        lambda m, r=real: f"// 0x{r:x}", old, count=1)
                open(path, "w").write("\n".join(lines))
                print(f"  -> rewrote {len(bad)} comment(s) in {hdrs[0]} "
                      f"(comments only; no code touched)")
    return rc


if __name__ == "__main__":
    sys.exit(main())
