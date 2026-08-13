#!/usr/bin/env python3
"""Adversarial re-derivation of the native link surface: what does the engine
archive still WANT that nothing on the link line supplies?

Originally written by lane X2/CENSUS to re-measure lane X1's "84 symbols"
claim without reusing X1's method. Harvested and hardened by lane HARVEST-TOOLS
(2026-08-13).

METHOD (stated explicitly so it can be re-run and criticised)
------------------------------------------------------------
  WANTS   = symbols UNDEFINED in libmilo-engine.a, accumulated over ALL archive
            members, symbol classes {U, w, v}.  (w/v = weak-undefined: they do
            NOT need a definition to link, so they are reported separately.)
  ARCHIVE = symbols DEFINED by the archive itself, classes {T,W,V,D,B,R,C} plus
            file-local {t,d,b,r,n} (a local def satisfies its own member).
  SUPPLY  = symbols DEFINED by a reference linked consumer (default rb3-ark),
            same defined-classes.
  EXTERN  = symbols defined by the external libs the consumer links anyway
            (libc/libstdc++/Dawn/glfw/imgui) -- gathered from the actual .a/.so
            files on the link line, not from a name heuristic.

  GAP = WANTS - ARCHIVE - SUPPLY - EXTERN

⛔ WHY THIS TOOL REFUSES INSTEAD OF REPORTING ZERO
--------------------------------------------------
As originally written this script was VACUOUS in the exact shape CLAUDE.md and
tools/screen_gate.py warn about. Measured 2026-08-13, before the guards below
existed::

    $ census_link_surface.py /nonexistent-build-dir --consumer rb3-ark
    archive defines   : 0
    archive UNDEF (U) : 0
    GAP (U - arc - consumer - extern) : 0        # rc=0

A missing build directory produced a clean, decisive ``GAP: 0`` at rc=0 --
which reads as "the link surface is fully satisfied, there is no porting work
left", the single most consequential *negative* this tool can emit. ``nm`` on a
nonexistent path fails silently, every set is empty, and an empty minus an
empty is an empty.

So every input this computation depends on is now asserted BEFORE the
subtraction, and a violated assertion EXITS NONZERO rather than printing a
number. The house rule this follows is the one ``tools/ab_measure.py`` and
``tools/prune_worktrees.py`` already encode: **a broken run must refuse, not
report.** Note the asymmetry that motivates guarding the consumer too -- a
missing archive deflates the GAP to a false "nothing to do", while a missing
consumer INFLATES it into a false worklist of porting that is already done.
Both are wrong; neither is safe.

Usage: census_link_surface.py <build-dir> [--consumer rb3-ark] [--list]
Output JSON goes to $OUT (default ~/tmp), never /tmp -- /tmp here is a
RAM-backed tmpfs shared across the box (see CLAUDE.md).
"""
import subprocess, sys, os, glob, json

DEFINED = set("TWVDBRCtdbrnGgSsi")
UNDEF_STRONG = set("U")
UNDEF_WEAK = set("wv")


def die(msg):
    """Refuse loudly. Exit 2 == 'this run proved nothing', never 'clean'."""
    print(f"REFUSED: {msg}", file=sys.stderr)
    sys.exit(2)


def nm(path, extra=(), required=False):
    """Return list of (klass, name). Uses --no-demangle for exact identity.

    ``required=True`` means a failing/empty nm is a broken run, not an empty
    answer -- that distinction is the whole point of this module.
    """
    cmd = ["nm", "--no-demangle", *extra, path]
    try:
        p = subprocess.run(cmd, capture_output=True, text=True)
    except OSError as e:
        die(f"could not execute nm ({e}) -- cannot measure the link surface")
    if required and p.returncode != 0:
        die(f"nm failed on {path} (rc={p.returncode}): {p.stderr.strip()[:200]}")
    out = []
    for line in p.stdout.splitlines():
        parts = line.split()
        if len(parts) < 2:
            continue
        # forms: "ADDR K name" | "K name" | "path:ADDR K name"
        k, n = parts[-2], parts[-1]
        if len(k) != 1:
            continue
        out.append((k, n))
    return out


def defined_of(path, extra=(), required=False):
    return {n for k, n in nm(path, extra, required) if k in DEFINED}


def main():
    if len(sys.argv) < 2:
        die("usage: census_link_surface.py <build-dir> [--consumer NAME] [--list]")
    bd = sys.argv[1]
    consumer = sys.argv[sys.argv.index("--consumer") + 1] if "--consumer" in sys.argv else "rb3-ark"
    want_list = "--list" in sys.argv

    # ---- GUARD 1: the build dir and the archive must actually exist --------
    if not os.path.isdir(bd):
        die(f"build dir does not exist: {bd}  (a missing dir yields GAP=0, "
            f"which reads as 'nothing left to port')")
    arc = os.path.join(bd, "milo-engine", "libmilo-engine.a")
    if not os.path.isfile(arc):
        die(f"engine archive not found: {arc}  (configure/build the native tree first)")

    syms = nm(arc, required=True)
    archive_def = {n for k, n in syms if k in DEFINED}
    wants_strong = {n for k, n in syms if k in UNDEF_STRONG} - archive_def
    wants_weak = {n for k, n in syms if k in UNDEF_WEAK} - archive_def

    # ---- GUARD 2: an archive that defines nothing is not a real archive ----
    if not archive_def:
        die(f"{arc} defines 0 symbols -- nm produced no usable output, so every "
            f"set below would be empty and the GAP would be a false 0")

    # ---- GUARD 3: a named consumer that is absent INFLATES the gap ---------
    consumer_path = os.path.join(bd, consumer)
    if not os.path.exists(consumer_path):
        die(f"consumer '{consumer}' not found at {consumer_path} -- its absence "
            f"would inflate the GAP into a worklist of already-done work")
    supply = defined_of(consumer_path, required=True)
    if not supply:
        die(f"consumer {consumer_path} defines 0 symbols -- refusing rather than "
            f"attributing its entire surface to the gap")

    # External libs, harvested from the real link line of rb3-frame if present.
    extern = set()
    extern_srcs = []
    for pat in ("_deps/**/*.a", "**/libimgui*.a", "**/libglfw*.a"):
        for f in glob.glob(os.path.join(bd, pat), recursive=True):
            extern_srcs.append(f)
    # Dawn + system libs from the frame link line
    lt = os.path.join(bd, "CMakeFiles", "rb3-frame.dir", "link.txt")
    if os.path.exists(lt):
        txt = open(lt).read()
        for tok in txt.split():
            if tok.endswith((".a", ".so")) and os.path.exists(tok):
                extern_srcs.append(tok)
    for so in ("/usr/lib/libc.so.6", "/usr/lib/libm.so.6", "/usr/lib/libstdc++.so.6",
               "/usr/lib/libgcc_s.so.1", "/usr/lib/libpthread.so.0"):
        if os.path.exists(so):
            extern_srcs.append(so)
    for f in sorted(set(extern_srcs)):
        try:
            extern |= defined_of(f, ("-D",) if f.endswith(".so") or ".so." in f else ())
        except SystemExit:
            raise
        except Exception:
            pass

    gap = sorted(wants_strong - supply - extern)
    gap_no_extern = sorted(wants_strong - supply)

    print(f"archive           : {arc}")
    print(f"archive defines   : {len(archive_def)}")
    print(f"archive UNDEF (U) : {len(wants_strong)}   [weak-undef w/v: {len(wants_weak)}]")
    print(f"consumer          : {consumer}  defines {len(supply)}")
    print(f"extern libs       : {len(extern_srcs)} files, {len(extern)} defined syms")
    print(f"GAP (U - arc - consumer - extern) : {len(gap)}")
    print(f"GAP (U - arc - consumer, extern NOT subtracted) : {len(gap_no_extern)}")
    if want_list:
        for s in gap:
            print(f"    {s}")

    outdir = os.environ.get("OUT", os.path.expanduser("~/tmp"))
    os.makedirs(outdir, exist_ok=True)
    dest = os.path.join(outdir, "census.json")
    json.dump({"gap": gap, "gap_no_extern": gap_no_extern,
               "weak_undef": sorted(wants_weak),
               "archive_defines": len(archive_def), "supply": len(supply)},
              open(dest, "w"), indent=1)
    print(f"[out] {dest}")
    return gap


if __name__ == "__main__":
    main()
