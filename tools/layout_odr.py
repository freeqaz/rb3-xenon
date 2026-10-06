#!/usr/bin/env python3
"""layout_odr.py -- cross-TU class-layout consistency (ODR) check.

WHAT IT ANSWERS
---------------
"Is any class laid out differently in two translation units of the same
program?"  That is an ODR split: the C++ program has two definitions of one
class, and which one a given function sees depends on which TU it was compiled
in.  Nothing else in this repo can see it.  The X360 match build never links,
and the native link does not compare class layouts (clang has no ODR checker
outside modules/LTO).  Lane W16-PZ found four by hand on 61 TUs
(``docs/decomp/W16PZ_BANDOBJ_BAND3_LAYOUT_AUDIT_2026-10-06.md`` section 7):
``MetaPerformer`` was 940 B in its own TU and 952 B in 21 others, and five
classes had a 1-byte stub definition scatter-included into ``BandCharacter.cpp``.
This tool runs that check over every class in every compiled TU, for both builds.

TWO DOMAINS
-----------
* ``x360``: every TU in ``build.ninja`` (rules ``msvc``/``msvc_pch``).  The
  retail binary is one program, so all TUs are one ODR domain.  The layout comes
  from ``cl.exe /d1reportAllClassLayout``, run on the build's own compile
  command.  The edits are the ones ``scripts/harvest/class_layout_report.py``
  documents (strip the objcache prefix, force ``/Y-`` and drop
  ``/Yu``/``/Yc``/``/Fp``, scratch ``/Fo``), plus ``/w``.
* ``native``: every distinct clang compile in ``native/build``.  One ODR domain
  per linked executable (``ninja -t inputs <exe>``), because the same source is
  compiled with different flags for different targets (``-DRB3_ENGINE_RENDER=1``
  and extra include roots on ~450 of 551 sources).  Comparing across targets
  would report flag differences that never meet in one program.  The layout comes
  from ``clang -fsyntax-only -Xclang -fdump-record-layouts-complete``.

FINGERPRINT
-----------
The compiler's own text for a class, normalized: the MSVC block from
``class X size(N):`` to the next class header (members with offsets and types,
bitfields, padding, vftable slots, ``this`` adjustors, vbase table), or clang's
``*** Dumping AST Record Layout`` block.  Identical layouts print identical text,
so equal text means equal layout.  Unequal text means the class differs in size,
a member offset, name or type, a base, or a vtable slot.

PARSE TRAPS (each measured on real output, 2026-10-06; each produced false
splits before it was handled, and each has a selftest leg)
---------------------------------------------------------------------------
1. MSVC interleaves ``/showIncludes`` notes into the report MID-LINE
   (``\t+-Note: including file: x.h\n--``).  Notes are cut out as whole
   ``Note: including file: ...\n`` substrings, which splices the interrupted
   report line back together (A1).  Compiler warnings are suppressed with
   ``/w``; W16-PZ's instrument G read warning text as member names.
2. ``/w`` does NOT silence the DRIVER: a TU with per-TU flag overrides (the
   Quazal ``/Od`` region) prints ``cl : Command line warning D9025``, which
   lands inside whatever block is printing -- 47 TUs' copies of
   ``_RTL_CRITICAL_SECTION`` etc. read as a second layout (A4).
3. Bitfield rows are printed ``92.\t| mForceLod (bitstart=29,nbits=3)``.  The
   fingerprint is the text itself, so no row parser can get this wrong.
4. clang prints a C TU's trailer as ``[sizeof=16, align=8]`` and a C++ TU's as
   ``[sizeof=16, dsize=16, align=8, nvsize=16, nvalign=8]``; the class-key the
   TU spelled (``struct Hmx::Color`` vs ``class Hmx::Color``); anonymous
   declarations by the include SPELLING of their path; and an empty class as
   ``class HolmesInput (empty)``.  All four are normalized (A5 for the last,
   which had hidden every member-less stand-in for a real class).

CACHE
-----
``~/.cache/rb3-layout-odr`` (``RB3_LAYOUT_ODR_CACHE``).  A lookup key is
sha256(tool version, domain, normalized argv, TU path).  Each key holds entries
``{deps: {path: sha256}, res: <sha>}``, where ``deps`` is the full include
closure THE LAYOUT COMPILE ITSELF reported (``/showIncludes``, ``-H``) plus the
TU.  An entry is served only if every dependency still hashes the same.  A false
miss costs a compile; a false hit would hide a split, so nothing is trusted that
was not re-hashed.  The project root is written as ``@ROOT@``, so worktrees share
entries.

EXIT CODES (check)
-----------------
0  PASS        every TU answered, every SPLIT/UNRESOLVED name is allowlisted
               (a PIN entry with its exact fingerprints, or a reviewed LIST
               entry -- see ``evaluate``)
1  FAIL        an unexplained SPLIT or UNRESOLVED name (or, with --strict, a
               stale allowlist entry)
2  UNRUNNABLE  no build.ninja / no native build for a domain that was asked for
3  UNANSWERED  a TU could not answer (COMPILE_FAILED, timeout) or fewer TUs than
               the allowlist's floor -- never a pass
Precedence when domains disagree: 1 > 3 > 2 > 0.  The last line of every run is
``LAYOUT_ODR_RESULT verdict=... rc=N`` -- quote it, do not paraphrase it.

VERDICTS (per printed name with more than one layout)
-----------------------------------------------------
SPLIT       one class identity has two layouts in one program.
UNRESOLVED  (x360 only) a layout could not be tied to one source definition.
COLLISION   (x360 only) different classes sharing a bare name; passes.
TEMPLATE    (x360 only) template-scoped nested classes; passes, counted.
See ``classify_x360`` for how a bare MSVC name is tied to a definition.
"""

import argparse
import concurrent.futures as cf
import difflib
import hashlib
import json
import os
import re
import shlex
import subprocess
import sys
import tempfile
import time

# Per-domain parser versions: bumping one invalidates only that domain's cache.
TOOL_VERSIONS = {"x360": "layout-odr-6", "native": "layout-odr-native-5"}
TOOL_VERSION = "/".join(f"{k}={v}" for k, v in sorted(TOOL_VERSIONS.items()))
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VERSION = "45410914"
CACHE = os.environ.get("RB3_LAYOUT_ODR_CACHE",
                       os.path.expanduser("~/.cache/rb3-layout-odr"))
ALLOW_PATH = os.path.join("config", VERSION, "layout_odr_allow.json")
OUT_DIR = os.path.join("build", VERSION, "layout_odr")

OK, CLASS_ABSENT, COMPILE_FAILED = "OK", "CLASS_ABSENT", "COMPILE_FAILED"


# ------------------------------------------------------------------ helpers

def sha(data):
    if isinstance(data, str):
        data = data.encode()
    return hashlib.sha256(data).hexdigest()


class FileHasher:
    """Memoized content hash.  Missing file -> "MISSING" (a real value: a
    dependency that vanished invalidates the entry like any other change)."""

    def __init__(self, root):
        self.root = root
        self.memo = {}

    def path(self, p):
        return p.replace("@ROOT@", self.root, 1) if p.startswith("@ROOT@") else p

    def __call__(self, p):
        if p not in self.memo:
            try:
                with open(self.path(p), "rb") as f:
                    self.memo[p] = sha(f.read())
            except OSError:
                self.memo[p] = "MISSING"
        return self.memo[p]


_DIRLIST = {}


def real_case(path):
    """The on-disk spelling of `path`, resolving each component
    case-insensitively (wibo's file lookup is case-insensitive, and the notes it
    leaves unrewritten carry the guest's spelling: `src/xdk/libcmt` for
    `src/xdk/LIBCMT`).  Returns `path` unchanged if it exists or cannot be
    resolved; the MISSING guard in run_job handles the latter."""
    if os.path.exists(path):
        return path
    parts = path.split("/")
    cur = "/" if path.startswith("/") else "."
    out = []
    for comp in parts:
        if comp in ("", "."):
            continue
        if comp == "..":
            cur = os.path.dirname(cur)
            out.append(comp)
            continue
        if cur not in _DIRLIST:
            try:
                _DIRLIST[cur] = os.listdir(cur)
            except OSError:
                _DIRLIST[cur] = []
        hit = comp if comp in _DIRLIST[cur] else next(
            (e for e in _DIRLIST[cur] if e.lower() == comp.lower()), None)
        if hit is None:
            return path
        cur = os.path.join(cur, hit)
        out.append(hit)
    return ("/" if path.startswith("/") else "") + "/".join(out)


def rootify(path, root, cwd):
    """Absolute-or-cwd-relative path -> '@ROOT@/...' when inside root."""
    ap = real_case(os.path.normpath(path if os.path.isabs(path) else os.path.join(cwd, path)))
    r = os.path.normpath(root)
    if ap == r or ap.startswith(r + os.sep):
        return "@ROOT@" + ap[len(r):]
    return ap


def root_spellings(root):
    """Every spelling of the project root that can appear in compiler text,
    longest first (``native/..`` forms appear in clang's include paths)."""
    r = os.path.normpath(root)
    rr = os.path.realpath(root)
    s = {r, rr, os.path.join(r, "native", ".."), os.path.join(rr, "native", "..")}
    return sorted(s, key=len, reverse=True)


def normalize_root(text, spellings):
    for s in spellings:
        text = text.replace(s, "@ROOT@")
    return text


# ------------------------------------------------------------------- jobs

class Job:
    __slots__ = ("domain", "tu", "argv", "env", "cwd", "keyargv", "outputs")

    def __init__(self, domain, tu, argv, env, cwd, keyargv):
        self.domain, self.tu, self.argv, self.env = domain, tu, argv, env
        self.cwd, self.keyargv = cwd, keyargv
        self.outputs = []          # native: every .o this compile stands for

    @property
    def key(self):
        return sha(json.dumps([TOOL_VERSIONS[self.domain], self.domain, self.keyargv,
                               self.tu]))


def x360_layout_argv(line):
    """Compile line from ``ninja -t compdb`` -> (argv, env).  ``/Fo`` is left
    as the placeholder ``@FO@`` for the runner to fill in."""
    argv = shlex.split(line)
    if "exec" in argv and "--" in argv:          # objcache exec ... -- <cmd>
        argv = argv[argv.index("--") + 1:]
    env = {}
    while argv and re.match(r"^[A-Z_][A-Z0-9_]*=", argv[0]):
        k, v = argv[0].split("=", 1)
        env[k] = v
        argv = argv[1:]
    out = []
    for a in argv:
        if a.startswith("/Fo"):
            out.append("/Fo@FO@")
        elif a.startswith(("/Yu", "/Yc", "/Fp")):
            continue                              # PCH: see class_layout_report DL-2
        else:
            out.append(a)
    if "/showIncludes" not in out:
        out.insert(-1, "/showIncludes")
    i = next((n for n, a in enumerate(out) if a.lower().endswith("cl.exe")), None)
    if i is None:
        raise ValueError("no cl.exe in compile line: " + line[:200])
    out[i + 1:i + 1] = ["/Y-", "/w"]
    out.insert(-1, "/d1reportAllClassLayout")
    return out, env


def x360_jobs(root):
    if not os.path.exists(os.path.join(root, "build.ninja")):
        raise SystemExit(f"layout_odr: no build.ninja in {root} (run configure.py)")
    p = subprocess.run(["ninja", "-t", "compdb", "msvc", "msvc_pch"], cwd=root,
                       capture_output=True, text=True)
    if p.returncode:
        raise SystemExit("layout_odr: ninja -t compdb failed:\n" + p.stderr[-2000:])
    jobs = []
    prefix = f"build/{VERSION}/src/"
    for e in json.loads(p.stdout):
        out = e["output"]
        if not (out.startswith(prefix) and out.endswith(".obj")):
            continue
        argv, env = x360_layout_argv(e["command"])
        keyargv = [a for a in argv if not a.startswith("/Fo")]
        j = Job("x360", e["file"], argv, env, root, keyargv)
        j.outputs.append(out)
        jobs.append(j)
    return jobs


def native_layout_argv(cmd):
    argv = shlex.split(cmd)
    out, skip = [], False
    for a in argv:
        if skip:
            skip = False
            continue
        if a in ("-o", "-MF", "-MT", "-MQ"):
            skip = True
            continue
        if a in ("-MD", "-MMD", "-c") or a.startswith(("-o", "-MF")) and len(a) > 3:
            continue
        out.append(a)
    out[1:1] = ["-fsyntax-only", "-Xclang", "-fdump-record-layouts-complete", "-H"]
    return out


NATIVE_BUILD = None     # --native-build; default <root>/native/build


def native_build_dir(root):
    return NATIVE_BUILD or os.path.join(root, "native", "build")


def native_jobs(root):
    nb = native_build_dir(root)
    if not os.path.exists(os.path.join(nb, "build.ninja")):
        raise SystemExit(f"layout_odr: no native build at {nb} -- configure it "
                         f"(tools/native_build_gate.sh does) or pass --domain x360")
    p = subprocess.run(["ninja", "-t", "compdb"], cwd=nb, capture_output=True, text=True)
    if p.returncode:
        raise SystemExit("layout_odr: native ninja -t compdb failed:\n" + p.stderr[-2000:])
    spell = root_spellings(root)
    by_key = {}
    for e in json.loads(p.stdout):
        f = e.get("file", "")
        cmd = e.get("command", "")
        if not f.endswith((".cpp", ".cc", ".c")) or "clang" not in cmd.split()[0]:
            continue
        argv = native_layout_argv(cmd)
        tu = rootify(f, root, e["directory"])
        keyargv = [normalize_root(a, spell) for a in argv]
        j = Job("native", tu, argv, None, e["directory"], keyargv)
        j = by_key.setdefault(j.key, j)
        j.outputs.append(os.path.normpath(os.path.join(e["directory"], e["output"])))
    return list(by_key.values())


def native_programs(root):
    """{executable: set(object paths)} from the native build graph."""
    nb = native_build_dir(root)
    text = open(os.path.join(nb, "build.ninja")).read()
    exes = re.findall(r"^build ([^:\s]+): CXX_EXECUTABLE_LINKER", text, re.M)
    progs = {}
    for exe in exes:
        p = subprocess.run(["ninja", "-t", "inputs", exe], cwd=nb,
                           capture_output=True, text=True)
        progs[exe] = {os.path.normpath(os.path.join(nb, l.strip()))
                      for l in p.stdout.splitlines() if l.strip().endswith(".o")}
    return progs


# ---------------------------------------------------------------- parsing

NOTE_RE = re.compile(r"Note: including file:[ \t]*([^\n]*)\n")
CLWARN_RE = re.compile(r"cl : Command line warning [^\n]*\n")
DIAG_RE = re.compile(r"^.*?\b((?:fatal )?error\s+[A-Z]+\d+)\s*:\s*(.*)$", re.M)
MSVC_HDR = re.compile(r"^(class|struct|union)\s+(\S+)\s+size\((\d+)\):[ \t]*$", re.M)
CLANG_SEP = "*** Dumping AST Record Layout"
CLANG_NAME = re.compile(r"^\s*0 \| (?:class|struct|union) (.+?)\s*$")
KEY_RE = re.compile(r"\b(?:class|struct) (?=[A-Za-z_(])")
EMPTY_SUFFIX = re.compile(r"\s+\(empty\)$")
ANON_AT = re.compile(r"(\bat )(\S+?)(:\d+:\d+\))")


def tu_local_name(domain, name):
    """Names that cannot collide across TUs (anonymous / lambda / local), or
    that are not a single name at all.  Skipped, and counted."""
    if domain == "x360":
        return (name.startswith("<") or "?A0x" in name or "<lambda" in name
                or "<unnamed" in name)
    return ("(anonymous" in name or "(unnamed" in name or "(lambda" in name)


def norm_block(text):
    lines = [l.rstrip() for l in text.split("\n")]
    out = []
    for l in lines:
        if not l and (not out or not out[-1]):
            continue
        out.append(l)
    while out and not out[-1]:
        out.pop()
    return "\n".join(out) + "\n"


def parse_x360(text, root, cwd):
    """-> (deps, classes[(name, block)], diags)"""
    # wibo rewrites the note to a host path but keeps the include SPELLING's
    # backslashes (`src/band3\\game/DirectInstrument.h`), so normalize them.
    # Some notes are not rewritten at all and arrive as the guest's absolute
    # path (`z:\\home\\free\\...`); drop the drive letter.
    deps = [rootify(re.sub(r"^[A-Za-z]:/", "/", m.group(1).strip().replace("\\", "/")),
                    root, cwd)
            for m in NOTE_RE.finditer(text)]
    text = NOTE_RE.sub("", text)
    # `/w` does not silence the DRIVER's own warnings: a TU with per-TU flag
    # overrides (the Quazal /Od region) prints `cl : Command line warning D9025 :
    # overriding '/O1' with '/Od'`, which lands inside whatever block is being
    # printed and made 47 TUs' copy of that class read as a second layout.
    text = CLWARN_RE.sub("", text)
    diags = [f"{m.group(1)}: {m.group(2).strip()}" for m in DIAG_RE.finditer(text)]
    heads = list(MSVC_HDR.finditer(text))
    classes = []
    for i, m in enumerate(heads):
        end = heads[i + 1].start() if i + 1 < len(heads) else len(text)
        classes.append((m.group(2), norm_block(text[m.start():end])))
    return deps, classes, diags


def parse_native(out, err, root):
    spell = root_spellings(root)
    deps = []
    for l in err.splitlines():
        m = re.match(r"^\.+ (\S.*)$", l)
        if m:
            deps.append(rootify(m.group(1).strip(), root, "/"))
    diags = [l.strip() for l in err.splitlines() if re.search(r"\berror:", l)]
    classes = []
    for blk in out.split(CLANG_SEP)[1:]:
        lines = blk.strip("\n").split("\n")
        m = CLANG_NAME.match(lines[0]) if lines else None
        if not m:
            continue
        body = normalize_root(blk.strip("\n"), spell)
        # A C TU prints `[sizeof=16, align=8]`, a C++ TU the same struct as
        # `[sizeof=16, dsize=16, align=8,\n | nvsize=16, nvalign=8]`.  Keep the
        # two facts both languages print, or every libc struct seen by one .c
        # file reads as a split (measured: 16 such names, mikktspace.c and
        # DataFlex.c against 548 C++ TUs).
        body = re.sub(r"\|\s*\[sizeof=(\d+),.*?align=(\d+)[,\]][^\]]*\]?",
                      r"| [sizeof=\1, align=\2]", body, flags=re.S)
        # clang prints the class-key a TU SPELLED (`struct Hmx::Color` in a TU
        # whose first declaration says struct, `class Hmx::Color` elsewhere).
        # Not layout; stripped.  (On MSVC the key changes MANGLING -- a separate
        # defect class, recorded in the W16-QD doc, not checked here.)
        body = KEY_RE.sub("", body)
        # `(anonymous at src/xdk/xnet/../win_types.h:69:5)` and
        # `(... at src/xdk/xapilibi/../win_types.h:69:5)` are one declaration.
        body = ANON_AT.sub(lambda m: m.group(1) + os.path.normpath(m.group(2)) + m.group(3),
                           body)
        # An empty class's header reads `0 | class HolmesInput (empty)`; the
        # annotation is part of the LAYOUT (kept in the body) but not of the
        # NAME.  Unstripped, every empty class -- a TU-local stand-in for a real
        # class above all -- is filed under a name no other TU uses, and the one
        # split this check exists for reads as two unrelated classes (measured:
        # os/HolmesClient.cpp's member-less HolmesInput, invisible on native
        # while the X360 domain reported it).
        name = KEY_RE.sub("", normalize_root(EMPTY_SUFFIX.sub("", m.group(1)), spell))
        classes.append((name, norm_block(body)))
    return sorted(set(deps)), classes, diags


# ------------------------------------------------------------------ cache

class Store:
    def __init__(self, base=CACHE):
        self.base = base
        for d in ("idx", "res", "lay"):
            os.makedirs(os.path.join(base, d), exist_ok=True)

    def _write(self, path, data):
        os.makedirs(os.path.dirname(path), exist_ok=True)
        fd, tmp = tempfile.mkstemp(dir=os.path.dirname(path), prefix=".tmp")
        with os.fdopen(fd, "w") as f:
            f.write(data)
        os.replace(tmp, path)

    def _idx(self, key):
        return os.path.join(self.base, "idx", key[:2], key + ".json")

    def lookup(self, job, hasher):
        try:
            entries = json.load(open(self._idx(job.key)))
        except (OSError, ValueError):
            return None
        for e in entries:
            if all(hasher(p) == h for p, h in e["deps"].items()):
                try:
                    return json.load(open(os.path.join(self.base, "res", e["res"][:2],
                                                       e["res"] + ".json")))
                except (OSError, ValueError):
                    return None
        return None

    def put(self, job, deps, result):
        blob = json.dumps(result, sort_keys=True)
        rk = sha(blob)
        self._write(os.path.join(self.base, "res", rk[:2], rk + ".json"), blob)
        try:
            entries = json.load(open(self._idx(job.key)))
        except (OSError, ValueError):
            entries = []
        entries = [e for e in entries if e["deps"] != deps][-7:] + [{"deps": deps, "res": rk}]
        self._write(self._idx(job.key), json.dumps(entries))

    def _lay(self, fp):
        return os.path.join(self.base, "lay", fp[:2], fp + ".txt")

    def put_layout(self, fp, text):
        p = self._lay(fp)
        if not os.path.exists(p):
            self._write(p, text)

    def layout(self, fp):
        try:
            return open(self._lay(fp)).read()
        except OSError:
            return None


# ----------------------------------------------------------------- runner

def run_job(job, root, store, hasher, timeout):
    t0 = time.time()
    # Whole seconds, rounded down: a file written in the same second the compile
    # started may or may not have been read before the write.
    started = int(t0) - 1
    with tempfile.TemporaryDirectory(dir=os.path.expanduser("~/tmp")) as td:
        argv = [a.replace("@FO@", os.path.join(td, "layout.obj")) for a in job.argv]
        env = dict(os.environ)
        env.update(job.env or {})
        try:
            p = subprocess.run(argv, cwd=job.cwd, capture_output=True, text=True,
                               errors="replace", env=env, timeout=timeout)
        except subprocess.TimeoutExpired:
            return {"status": COMPILE_FAILED, "diags": [f"timeout after {timeout}s"],
                    "classes": [], "secs": round(time.time() - t0, 1)}, False
    if job.domain == "x360":
        deps, classes, diags = parse_x360(p.stdout + p.stderr, root, job.cwd)
    else:
        deps, classes, diags = parse_native(p.stdout, p.stderr, root)
    if p.returncode and not diags:
        diags = [f"compiler exit {p.returncode}: " + (p.stdout + p.stderr).strip()[-400:]]
    secs = round(time.time() - t0, 1)
    if diags:
        return {"status": COMPILE_FAILED, "diags": diags[:20], "classes": [],
                "secs": secs}, False
    pairs, skipped = [], 0
    for name, block in classes:
        if tu_local_name(job.domain, name):
            skipped += 1
            continue
        fp = sha(block)[:20]
        store.put_layout(fp, block)
        pairs.append([name, fp])
    tu = job.tu if job.tu.startswith("@ROOT@") else rootify(job.tu, root, job.cwd)
    depset = sorted(set(deps) | {tu})
    result = {"status": OK if pairs else CLASS_ABSENT, "diags": [], "classes": pairs,
              "skipped_local": skipped, "secs": secs, "deps": depset}
    depmap = {d: hasher(d) for d in depset}
    missing = sorted(d for d, h in depmap.items() if h == "MISSING")
    moved = []
    for d in depset:
        try:
            if os.stat(hasher.path(d)).st_mtime >= started:
                moved.append(d)
        except OSError:
            pass
    if moved:
        # A dependency was written while this TU compiled.  Which version the
        # compiler read is unknowable, and the hash taken now (or memoized
        # earlier) may describe the other one -- an entry binding the two would
        # be served as current forever.  Answer this run, never cache it.
        result["uncached_deps_changed"] = moved[:5]
        return result, False
    if missing:
        # A dependency the compiler just read cannot be MISSING.  If it is, our
        # path translation is wrong, and an entry keyed on it would never be
        # invalidated by a change to that file (MISSING == MISSING forever).
        # Answer this run, never cache it.
        result["uncached_missing_deps"] = missing[:5]
        return result, False
    store.put(job, depmap, result)
    return result, True


def sweep(root, domain, jobs_n=12, timeout=3600, verbose=True, only=None):
    jobs = x360_jobs(root) if domain == "x360" else native_jobs(root)
    if only:
        jobs = [j for j in jobs if any(o in j.tu for o in only)]
    store = Store()
    hasher = FileHasher(root)
    results, todo = {}, []
    for j in jobs:
        r = store.lookup(j, hasher)
        if r is not None:
            results[j.key] = r
        else:
            todo.append(j)
    hits = len(results)
    if verbose:
        print(f"layout_odr[{domain}]: {len(jobs)} TU compiles, {hits} cached, "
              f"{len(todo)} to run (-j{jobs_n})", file=sys.stderr, flush=True)
    t0, done, last = time.time(), 0, time.time()
    # longest-first is unknowable up front; run in TU order so a progress line
    # names a recognisable place.
    with cf.ThreadPoolExecutor(max_workers=jobs_n) as ex:
        futs = {ex.submit(run_job, j, root, store, hasher, timeout): j for j in todo}
        for f in cf.as_completed(futs):
            j = futs[f]
            try:
                r, _ = f.result()
            except Exception as e:                      # never a silent zero
                r = {"status": COMPILE_FAILED, "diags": [f"runner: {e!r}"], "classes": []}
            results[j.key] = r
            done += 1
            if verbose and (r["status"] == COMPILE_FAILED or time.time() - last > 30
                            or done == len(todo)):
                last = time.time()
                tag = "FAILED " + j.tu + ": " + r["diags"][0][:160] \
                    if r["status"] == COMPILE_FAILED else ""
                print(f"  [{done}/{len(todo)}] {time.time() - t0:.0f}s {tag}",
                      file=sys.stderr, flush=True)
    return jobs, results, {"cached": hits, "ran": len(todo),
                           "secs": round(time.time() - t0, 1)}



# ------------------------------------------------- source definition index
#
# MSVC prints a nested class by its BARE name: `BandCamShot::Target` and
# `HamCamShot::Target` both print as `Target`, every `ObjPtrList<T>::Node` as
# `Node`, every STL `iterator` as `iterator`.  So on the X360 side the printed
# name is not an identity, and comparing by it reports hundreds of unrelated
# classes as "splits" (2 TUs alone gave 22 such names).  Clang prints qualified
# names, so the native side needs none of this.
#
# Attribution: for a layout seen in TU X, the candidates are the definitions of
# that bare name in files X actually includes (the layout compile's own include
# list).  Among those, the definition whose body contains every DIRECT member
# and base the compiler printed is the one.  The identity is then the
# definition's QUALIFIED name, so two headers defining the same global class
# (the dc3-era `src/meta_ham/MetaPerformer.h` vs `src/band3/meta_band/...`, or
# W16-PZ's 1-byte stubs) still group together, which is the point.

TOK_RE = re.compile(r"""
    (?P<ws>\s+)
  | (?P<lc>//[^\n]*)
  | (?P<bc>/\*.*?\*/)
  | (?P<pp>\#(?:[^\n\\]|\\.|\\\n)*)
  | (?P<str>"(?:\\.|[^"\\\n])*")
  | (?P<chr>'(?:\\.|[^'\\\n])*')
  | (?P<id>[A-Za-z_]\w*)
  | (?P<dc>::)
  | (?P<p>[{}();:<>=,\[\]])
  | (?P<o>.)
""", re.S | re.X)
CLASS_KEYS = ("class", "struct", "union")


SRC_INDEX_VERSION = "src3"
ACCESS = ("public", "private", "protected")
NOT_DATA = ("typedef", "using", "friend", "static", "enum", "template", "class",
            "struct", "union", "operator", "virtual", "explicit", "inline",
            "DECLARE_REVS", "OBJ_CLASSNAME", "OBJ_SET_TYPE")


def _data_members(stmt):
    """Non-static data member names declared by one class-body statement
    (tokens up to `;`).  Best effort; macros that declare members are missed,
    which only weakens disambiguation, never fabricates a split."""
    vals = [v for _k, v in stmt]
    while len(vals) >= 2 and vals[0] in ACCESS and vals[1] == ":":
        vals = vals[2:]
    if not vals or vals[0] in NOT_DATA or "operator" in vals or "~" in vals:
        return []
    # function pointer member:  R (*name)(args);
    if "(" in vals:
        for i in range(len(vals) - 3):
            if vals[i] == "(" and vals[i + 1] == "*" and vals[i + 3] == ")":
                return [vals[i + 2]]
        return []
    out, depth, last = [], 0, None
    for v in vals + [","]:
        if v in ("<", "["):
            depth += 1
        elif v in (">", "]"):
            depth = max(0, depth - 1)
        elif depth == 0 and v in (",", "=", ":"):
            if last:
                out.append(last)
            last = None
            if v in ("=", ":"):
                depth = 99          # skip initializer / bitfield width to next comma
            continue
        elif depth == 99:
            continue
        if depth == 0 and re.match(r"^[A-Za-z_]\w*$", v):
            last = v
        if depth == 99 and v == ",":
            depth = 0
    return out


class Def:
    __slots__ = ("name", "qname", "file", "line", "template", "local", "idents",
                 "members", "sig", "parent_idents")

    def __init__(self, name, qname, file, line, template, local):
        self.name, self.qname, self.file, self.line = name, qname, file, line
        self.template, self.local = template, local
        self.idents = set()
        self.members = []          # declared non-static data members, in order
        self.sig = ""              # hash of the body's token stream (twin test)
        self.parent_idents = set() # identifiers of the enclosing class body

    @property
    def identity(self):
        # a TU-local class is its own identity per file
        return (self.file + "#" + self.qname) if self.local else self.qname


def _classify_open(stmt):
    """Tokens since the last `;`/`{`/`}` -> what the next `{` opens.
    Returns (kind, name, qualifier, is_template)."""
    vals = [v for _k, v in stmt]
    if not vals:
        return "other", None, [], False
    if "namespace" in vals:
        i = vals.index("namespace")
        nm = vals[i + 1] if i + 1 < len(vals) and stmt[i + 1][0] == "id" else None
        return ("ns", nm, [], False) if nm else ("anon", None, [], False)
    if vals[0] == "extern" and len(stmt) > 1 and stmt[1][0] == "str":
        return "extern", None, [], False
    depth_p = depth_a = 0
    key_at = None
    for i, (k, v) in enumerate(stmt):
        if v == "(":
            depth_p += 1
        elif v == ")":
            depth_p -= 1
        elif v == "<":
            depth_a += 1
        elif v == ">":
            depth_a = max(0, depth_a - 1)
        elif v in CLASS_KEYS and depth_p == 0 and depth_a == 0:
            key_at = i
    if key_at is not None:
        if key_at > 0 and vals[key_at - 1] == "enum":
            return "enum", None, [], False
        rest = stmt[key_at + 1:]
        depth_p = depth_a = 0
        names = []
        for k, v in rest:
            if v == "(":
                depth_p += 1
            elif v == ")":
                depth_p -= 1
            elif v == "<":
                depth_a += 1
            elif v == ">":
                depth_a = max(0, depth_a - 1)
            elif depth_p == 0 and depth_a == 0:
                if v in (":", "=", ","):
                    break
                if k == "id" and v not in ("final", "__declspec", "alignas"):
                    names.append(v)
        if "(" in [v for _k, v in rest if True] and ":" not in vals[key_at:] \
                and "=" not in vals[key_at:]:
            # `struct X *f(...) {` -- a function returning a class pointer
            if vals[key_at:].index("(") < (vals[key_at:].index("{")
                                            if "{" in vals[key_at:] else 10 ** 9):
                tail = vals[key_at + 1:]
                if tail and tail[-1] == ")" or ")" in tail[-3:]:
                    return "func", None, [], False
        if "=" in vals[key_at:]:
            return "init", None, [], False
        if not names:
            return "anonclass", None, [], "template" in vals[:key_at]
        return "class", names[-1], names[:-1], "template" in vals[:key_at]
    if "(" in vals or ")" in vals:
        return "func", None, [], False
    if "=" in vals:
        return "init", None, [], False
    if "enum" in vals:
        return "enum", None, [], False
    return "other", None, [], False


def index_source(text, relpath):
    """All class definitions in one file."""
    stack, stmt, defs = [], [], []
    children = {}
    line = 1
    for m in TOK_RE.finditer(text):
        kind = m.lastgroup
        v = m.group()
        if kind in ("ws", "lc", "bc", "pp"):
            line += v.count("\n")
            continue
        line += v.count("\n")
        if kind == "id":
            for fr in stack:
                if fr[0] == "class":
                    fr[3].idents.add(v)
        for fr in stack:
            if fr[0] == "class":
                fr[4].append(v)
        if v == "{":
            k, name, qual, tmpl = _classify_open(stmt)
            if any(f[0] in ("func", "local") for f in stack):
                k2 = "class" if k == "class" else "local"
            else:
                k2 = k
            if k2 == "class":
                outer = [f for f in stack if f[0] in ("ns", "class")]
                qn = "::".join([f[1] for f in outer] + qual + [name])
                d = Def(name, qn, relpath, line,
                        tmpl or any(f[2] for f in stack if f[0] == "class"),
                        any(f[0] in ("anon", "func", "local") for f in stack))
                # base-clause identifiers count as "in the body"
                after = [vv for kk, vv in stmt if kk == "id"]
                d.idents.update(after)
                defs.append(d)
                stack.append(("class", name, d.template, d, []))
            elif k2 == "ns":
                stack.append(("ns", name, False, None, None))
            elif k2 in ("anon", "func", "local"):
                stack.append((k2, None, False, None, None))
            else:
                stack.append(("other", None, False, None, None))
            stmt = []
        elif v == "}":
            if stack:
                fr = stack.pop()
                if fr[0] == "class":
                    fr[3].sig = sha(" ".join(fr[4]))[:16]
                    outer = next((f for f in reversed(stack) if f[0] == "class"), None)
                    if outer is not None:
                        children.setdefault(id(outer[3]), []).append(fr[3])
            stmt = []
        elif v == ";":
            if stack and stack[-1][0] == "class":
                stack[-1][3].members.extend(_data_members(stmt))
            stmt = []
        else:
            stmt.append((kind, v))
    for d in defs:
        for c in children.get(id(d), []):
            c.parent_idents = d.idents
    return defs


class SourceIndex:
    def __init__(self, root, hasher, store=None):
        self.root, self.hasher = root, hasher
        self.memo = {}
        self.cache = os.path.join(store.base if store else CACHE, "src")
        os.makedirs(self.cache, exist_ok=True)

    def defs(self, relpath):
        if relpath in self.memo:
            return self.memo[relpath]
        h = self.hasher(relpath)
        cp = os.path.join(self.cache, SRC_INDEX_VERSION, h[:2], h + ".json")
        out = None
        try:
            raw = json.load(open(cp))
            out = []
            for r in raw:
                d = Def(r[0], r[1], relpath, r[2], r[3], r[4])
                d.idents, d.members, d.sig = set(r[5]), r[6], r[7]
                d.parent_idents = set(r[8])
                out.append(d)
        except (OSError, ValueError):
            try:
                text = open(self.hasher.path(relpath), errors="replace").read()
            except OSError:
                text = ""
            out = index_source(text, relpath)
            os.makedirs(os.path.dirname(cp), exist_ok=True)
            fd, tmp = tempfile.mkstemp(dir=os.path.dirname(cp), prefix=".tmp")
            with os.fdopen(fd, "w") as f:
                json.dump([[d.name, d.qname, d.line, d.template, d.local,
                            sorted(d.idents), d.members, d.sig,
                            sorted(d.parent_idents)] for d in out], f)
            os.replace(tmp, cp)
        # file is part of the identity of a TU-local def; keep relpath
        for d in out:
            d.file = relpath
        self.memo[relpath] = out
        return out


MSVC_ROW = re.compile(r"^\s*(\d+)\.?\t\| (.*)$")
MSVC_BASE = re.compile(r"^\t(\| )?\+--- \((?:base class|virtual base) (\S+?)\)")


def block_facts(block):
    """(size, direct member names, direct base names, own virtual names,
    identifiers in DIRECT member type names, identifiers in ALL row types
    including inherited rows)."""
    lines = block.split("\n")
    m = MSVC_HDR.match(lines[0])
    size = int(m.group(3)) if m else None
    cname = m.group(2) if m else ""
    members, bases, virt, types, alltypes = set(), set(), set(), set(), set()
    in_layout = True
    frame = 0
    for l in lines[1:]:
        if l.startswith("\t+---"):
            # The class's own frame is the first top-level `+---` pair.  Rows
            # after it belong to a VIRTUAL base (`+--- (virtual base Object)`),
            # whose members are not declared in this class and whose name need
            # not be either (it can be inherited indirectly).
            frame += 1
            if frame >= 2:
                in_layout = False
            continue
        if re.match(r"^\S+::\$v[fb]|^\S+ this adjustor|^vbi:", l):
            in_layout = False          # vftable / vbtable / adjustor / vbi section
        r = MSVC_ROW.match(l) if in_layout else None
        if r:
            body = r.group(2)
            for t in body.replace("|", " ").split()[:-1]:
                alltypes.update(re.findall(r"(?:^|\?\$|@[VUTW]?)([A-Za-z_]\w*)", t))
            if body.startswith(("|", "+", "{", "<")):
                continue
            body = re.sub(r"\s*\(bitstart=.*\)$", "", body).strip()
            if body:
                parts = body.split()
                members.add(parts[-1])
                for t in parts[:-1]:
                    # `Symbol`, or mangled `?$ObjPtr@VRndEnviron@@`
                    types.update(re.findall(r"(?:^|\?\$|@[VUTW]?)([A-Za-z_]\w*)", t))
            continue
        b = MSVC_BASE.match(l) if in_layout else None
        if b:
            nm = b.group(2)
            if nm.startswith("?$"):
                nm = nm[2:].split("@")[0]
            bases.add(nm)
            continue
        for fn in re.findall(r"&" + re.escape(cname) + r"::(\w+)", l):
            virt.add(fn)
    return size, members, bases, virt, types, alltypes



# ------------------------------------------------------------- classify
#
# Verdicts, per printed name with more than one layout:
#   SPLIT       one identity (qualified definition) has >1 layout -- a real ODR
#               split.  Fails the check unless allowlisted with these exact
#               fingerprints.
#   UNRESOLVED  some (layout, TU) could not be tied to exactly one definition.
#               "I cannot answer" -- fails the check unless allowlisted.
#   COLLISION   every layout was tied to a definition and no definition has two
#               layouts: different classes sharing a bare name.  Passes.
#   TEMPLATE    every layout belongs to a template-scoped definition (one per
#               instantiation, e.g. `ObjPtrList<T>::Node`) or is a `size(0)`
#               uninstantiated pattern.  Not comparable by name; passes, and
#               is counted so the size of this blind spot stays visible.

def attribute(name, need, cands, soft=frozenset(), mem_only=frozenset(),
              soft_all=frozenset()):
    """-> (identity | None, kind, note).  kind: def | template | none | ambiguous

    `need` (direct members + bases) must be in the definition's body.  `soft`
    (virtual names, member type names) only breaks ties between definitions
    that all contain `need` -- e.g. `MsgSinks::Sink` vs `MsgSource::Sink`."""
    if not cands:
        return None, "none", "no definition of this name in the TU's include closure"
    full = [d for d in cands if need <= d.idents]
    chosen = full
    note = "full"
    if not full and need:
        best = max(len(need & d.idents) for d in cands)
        if best / len(need) >= 0.6:
            chosen = [d for d in cands if len(need & d.idents) == best]
            note = f"partial {best}/{len(need)}"
    if not chosen:
        return None, "none", f"no definition contains the members {sorted(need)[:6]}"
    if len({d.identity for d in chosen}) > 1:
        exact = [d for d in chosen if set(d.members) == mem_only] if mem_only else []
        if exact:
            chosen = exact
            note += " +exact-members"
    if len({d.identity for d in chosen}) > 1 and soft:
        top = max(len(soft & d.idents) for d in chosen)
        chosen = [d for d in chosen if len(soft & d.idents) == top]
        note += " +tiebreak"
    if len({d.identity for d in chosen}) > 1 and soft_all:
        # siblings: the deciding type may be declared next door, one scope up
        # (MsgSinks::EventSinkElem's base Sink holds the ObjOwnerPtr)
        top = max(len(soft_all & d.parent_idents) for d in chosen)
        if top:
            chosen = [d for d in chosen if len(soft_all & d.parent_idents) == top]
            note += " +parent-scope"
    if len({d.identity for d in chosen}) > 1 and len({d.sig for d in chosen}) == 1 \
            and not any(d.template for d in chosen):
        # token-identical twins (MsgSinks::Sink / MsgSource::Sink): no source
        # evidence can tell them apart, so they are one joint identity.  A split
        # in either still shows as two layouts of the joint identity.
        joint = "|".join(sorted({d.identity for d in chosen}))
        return joint, "def", f"{note} twins {chosen[0].file}:{chosen[0].line}"
    ids = {d.identity for d in chosen}
    tm = {d.template for d in chosen}
    if tm == {True}:
        return None, "template", "template-scoped"
    if len(ids) == 1 and tm == {False}:
        d = chosen[0]
        return d.identity, "def", f"{note} {d.file}:{d.line}"
    return sorted(ids), "ambiguous", "candidates: " + ", ".join(
        sorted(f"{d.qname}@{d.file}:{d.line}" for d in chosen)[:6])


def classify_x360(div, tu_deps, root, store, hasher):
    sidx = SourceIndex(root, hasher, store)
    files = set()
    for deps in tu_deps.values():
        files |= set(deps)
    by_name = {}
    for f in sorted(files):
        for d in sidx.defs(f):
            by_name.setdefault(d.name, []).append(d)
    depsets = {tu: set(d) for tu, d in tu_deps.items()}
    out = {}
    for name, fps in sorted(div.items()):
        ndefs = by_name.get(name, [])
        groups, unresolved, sites = {}, [], {}
        facts, memo, per_tu = {}, {}, {}
        for fp, tus in fps.items():
            size, mem, bases, virt, types, alltypes = block_facts(store.layout(fp) or "")
            if size == 0:
                continue                      # uninstantiated template pattern
            facts[fp] = (mem | bases, frozenset(virt | types), frozenset(mem),
                         frozenset(alltypes))
            for tu in tus:
                per_tu.setdefault(tu, []).append(fp)
        if name.startswith("?$"):
            # A mangled template-id carries its arguments, so the printed name
            # IS the identity (only its enclosing SCOPE is dropped -- see the
            # TEMPLATE verdict for the nested-template blind spot).
            for fp, tus in fps.items():
                if fp in facts:
                    groups.setdefault(name, {})[fp] = list(tus)
                    sites.setdefault(name, {})[fp] = {"(template-id)"}
            per_tu = {}
        for tu, tfps in per_tu.items():
            ds = depsets.get(tu, set())
            cands = tuple(d for d in ndefs if d.file in ds)
            ck = tuple(id(d) for d in cands)
            got = {}
            for fp in tfps:
                k = (fp, ck)
                if k not in memo:
                    need, soft, mem, soft_all = facts[fp]
                    memo[k] = attribute(name, need, cands, soft, mem, soft_all)
                got[fp] = memo[k]
            # EXCLUSION: one definition has one layout within a TU, so an
            # identity claimed by another layout of this name here cannot be
            # this layout's.
            claimed = {r[0]: fp for fp, r in got.items() if r[1] == "def"}
            for fp, (ident, kind, note) in list(got.items()):
                if kind != "ambiguous":
                    continue
                left = [i for i in ident if claimed.get(i, fp) == fp]
                if len(left) == 1:
                    got[fp] = (left[0], "def", note + " +exclusion")
            for fp, (ident, kind, note) in got.items():
                if kind == "def":
                    groups.setdefault(ident, {}).setdefault(fp, []).append(tu)
                    sites.setdefault(ident, {}).setdefault(fp, set()).add(note.split()[-1])
                elif kind in ("none", "ambiguous"):
                    unresolved.append({"fp": fp, "tu": tu, "kind": kind, "note": note})
        splits = {i: {fp: sorted(t) for fp, t in g.items()}
                  for i, g in groups.items() if len(g) > 1}
        if splits:
            verdict = "SPLIT"
        elif unresolved:
            verdict = "UNRESOLVED"
        elif groups:
            verdict = "COLLISION"
        else:
            verdict = "TEMPLATE"
        # one representative per unresolved fp keeps the summary small
        ur = {}
        for u in unresolved:
            ur.setdefault(u["fp"], u)
        # where each split layout was tied to: one site under different macros,
        # or two sites defining one qualified name (a duplicate definition)
        split_sites = {i: {fp: sorted(sites[i][fp]) for fp in g} for i, g in splits.items()}
        out[name] = {"verdict": verdict, "splits": splits, "sites": split_sites,
                     "unresolved": list(ur.values()),
                     "identities": sorted(groups)}
    return out


def classify_native(div):
    return {n: {"verdict": "SPLIT", "splits": {n: v}, "unresolved": [],
                "identities": [n]} for n, v in div.items()}


# -------------------------------------------------------------- aggregate

def aggregate(domain, root, jobs, results):
    """-> summary dict with `divergent: {name: {fp: [tu, ...]}}`."""
    failed = [{"tu": j.tu, "diags": results[j.key]["diags"][:3]}
              for j in jobs if results[j.key]["status"] == COMPILE_FAILED]
    meta = {"domain": domain, "tool": TOOL_VERSION, "tu_compiles": len(jobs),
            "answered": len(jobs) - len(failed), "failed": failed,
            "class_rows": sum(len(results[j.key]["classes"]) for j in jobs),
            "skipped_local": sum(results[j.key].get("skipped_local", 0) for j in jobs)}

    def fold(job_list):
        names = {}
        for j in job_list:
            for name, fp in results[j.key]["classes"]:
                names.setdefault(name, {}).setdefault(fp, set()).add(j.tu)
        return names

    if domain == "x360":
        names = fold(jobs)
        div = {n: {fp: sorted(t) for fp, t in v.items()} for n, v in names.items()
               if len(v) > 1}
        meta["distinct_names"] = len(names)
        tu_deps = {j.tu: results[j.key].get("deps", []) for j in jobs}
        return {"meta": meta, "divergent": div, "programs": {},
                "classified": classify_x360(div, tu_deps, root, Store(), FileHasher(root))}

    progs = native_programs(root)
    by_out = {}
    for j in jobs:
        for o in j.outputs:
            by_out[o] = j
    # Clang qualifies every class EXCEPT a function-local one, which prints bare
    # (`_Guard` in four libstdc++ functions, `__loadu_epi16` in the intrinsics
    # headers).  A real class cannot have two layouts in one TU, so a name that
    # does is several local entities: set aside, and listed.
    multi = {}
    for j in jobs:
        seen = {}
        for n, fp in results[j.key]["classes"]:
            seen.setdefault(n, set()).add(fp)
        for n, v in seen.items():
            if len(v) > 1:
                multi.setdefault(n, j.tu)
    div, where, allnames = {}, {}, set()
    unmapped = {}
    for exe, objs in sorted(progs.items()):
        pj = {by_out[o].key: by_out[o] for o in objs if o in by_out}
        unmapped[exe] = sorted(o for o in objs if o not in by_out)
        names = fold(pj.values())
        allnames |= set(names)
        for n, v in names.items():
            if len(v) > 1 and n not in multi:
                d = div.setdefault(n, {})
                for fp, t in v.items():
                    d.setdefault(fp, set()).update(t)
                where.setdefault(n, []).append(exe)
    meta["distinct_names"] = len(allnames)
    meta["programs"] = {e: len(o) for e, o in progs.items()}
    meta["unmapped_objects"] = {e: u for e, u in unmapped.items() if u}
    meta["intra_tu_multiple"] = dict(sorted(multi.items()))
    div = {n: {fp: sorted(t) for fp, t in v.items()} for n, v in div.items()}
    return {"meta": meta, "divergent": div, "programs": where,
            "classified": classify_native(div)}


# -------------------------------------------------------------------- CLI

def write_summary(root, domain, summ):
    d = os.path.join(root, OUT_DIR)
    os.makedirs(d, exist_ok=True)
    p = os.path.join(d, domain + ".json")
    with open(p, "w") as f:
        json.dump(summ, f, indent=1, sort_keys=True)
    return p


def load_summary(root, domain):
    p = os.path.join(root, OUT_DIR, domain + ".json")
    if not os.path.exists(p):
        raise SystemExit(f"layout_odr: no {p}; run `sweep --domain {domain}` first")
    return json.load(open(p))


def domains_of(arg):
    return ["x360", "native"] if arg == "all" else [arg]


def cmd_sweep(a):
    rc = 0
    for dom in domains_of(a.domain):
        jobs, res, stats = sweep(a.project_dir, dom, a.jobs, a.timeout, not a.quiet, a.only)
        summ = aggregate(dom, a.project_dir, jobs, res)
        summ["meta"].update(stats)
        p = write_summary(a.project_dir, dom, summ)
        m = summ["meta"]
        print(f"layout_odr[{dom}]: {m['answered']}/{m['tu_compiles']} TUs answered, "
              f"{m['class_rows']} class rows, {m['distinct_names']} names, "
              f"{len(summ['divergent'])} with >1 layout, {len(m['failed'])} failed "
              f"(cached {stats['cached']}, ran {stats['ran']}, {stats['secs']}s) -> {p}")
        vc = {}
        for c in summ["classified"].values():
            vc[c["verdict"]] = vc.get(c["verdict"], 0) + 1
        print(f"layout_odr[{dom}]: verdicts {dict(sorted(vc.items()))}")
        if m["failed"]:
            rc = 3
    return rc


def cmd_show(a):
    store = Store()
    for dom in domains_of(a.domain):
        summ = load_summary(a.project_dir, dom)
        for name in a.names:
            v = summ["divergent"].get(name)
            if not v:
                print(f"[{dom}] {name}: one layout (or not seen)")
                continue
            c = summ.get("classified", {}).get(name, {})
            if c:
                print(f"[{dom}] {name}: verdict {c['verdict']}  identities={c['identities']}")
                for i, g in c.get("splits", {}).items():
                    print(f"    SPLIT {i}: " + "; ".join(f"{fp} x{len(t)}" for fp, t in g.items()))
                for u in c.get("unresolved", []):
                    print(f"    unresolved {u['fp']} in {u['tu']}: {u['kind']}: {u['note']}")
            fps = sorted(v, key=lambda f: -len(v[f]))
            print(f"[{dom}] {name}: {len(fps)} layouts"
                  + (f"  programs={summ['programs'].get(name)}" if dom == "native" else ""))
            for fp in fps:
                t = v[fp]
                print(f"  {fp}  {len(t)} TU(s): " + ", ".join(t[:a.max_tus])
                      + (" ..." if len(t) > a.max_tus else ""))
            base = (store.layout(fps[0]) or "").splitlines()
            for fp in fps[1:]:
                other = (store.layout(fp) or "").splitlines()
                print(f"  --- diff {fps[0]} -> {fp}")
                for l in difflib.unified_diff(base, other, lineterm="", n=a.context):
                    print("   " + l)


# ------------------------------------------------------------------ check

def entry_fps(c):
    """The fingerprints an allowlist entry pins for one classified name: the
    split identities' layouts, or the unresolved layouts.  Collision siblings
    are deliberately NOT pinned, so editing an unrelated same-named class does
    not re-fire an accepted entry."""
    if c["verdict"] == "SPLIT":
        return sorted({fp for g in c["splits"].values() for fp in g})
    return sorted({u["fp"] for u in c["unresolved"]})


def load_allow(root):
    p = os.path.join(root, ALLOW_PATH)
    if not os.path.exists(p):
        return {"min_tu_compiles": {}, "entries": []}
    return json.load(open(p))


def evaluate(summ, allow):
    """-> (bad [(name, classified, fps)], stale [entry]).

    Two entry shapes:
      PIN   {"domain", "name", "verdict", "fps": [...], "reason"}
            accepts exactly these layouts of this name.  Any change to any of
            them -- or a new layout -- re-fires.
      LIST  {"domain", "verdict", "names": [...], "identity_prefix", "reason"}
            accepts the listed names whatever their layouts.  For a reviewed
            FAMILY whose splits are expected to churn (the Quazal per-TU
            mockups); a name not on the list still fails, and so does a listed
            name with a split identity outside `identity_prefix` (MSVC prints
            `Quazal::Station` as `Station`, so the printed name alone cannot
            keep an RB3 class of the same bare name out of the family).  A
            listed name that no longer has that verdict is reported stale.
    """
    dom = summ["meta"]["domain"]
    entries = [e for e in allow.get("entries", []) if e["domain"] == dom]
    pins = [(i, e) for i, e in enumerate(entries) if "names" not in e]
    lists = [(i, e) for i, e in enumerate(entries) if "names" in e]
    used, used_names, bad = set(), set(), []
    for name, c in sorted(summ["classified"].items()):
        if c["verdict"] not in ("SPLIT", "UNRESOLVED"):
            continue
        fps = entry_fps(c)
        hit = next((i for i, e in pins
                    if e["name"] == name and e["verdict"] == c["verdict"]
                    and sorted(e["fps"]) == fps), None)
        if hit is None:
            hit = next((i for i, e in lists
                        if e["verdict"] == c["verdict"] and name in e["names"]
                        and all(ident.startswith(e.get("identity_prefix", ""))
                                for ident in c["splits"])), None)
            if hit is not None:
                used_names.add((hit, name))
        if hit is None:
            bad.append((name, c, fps))
        else:
            used.add(hit)
    stale = [e for i, e in pins if i not in used]
    for i, e in lists:
        gone = [n for n in e["names"] if (i, n) not in used_names]
        if gone:
            stale.append({"domain": e["domain"], "verdict": e["verdict"],
                          "name": f"{len(gone)} listed name(s)", "fps": gone[:8]})
    return bad, stale


RC_RANK = {0: 0, 2: 1, 3: 2, 1: 3}     # FAIL > UNANSWERED > UNRUNNABLE > PASS


def worst(a, b):
    return a if RC_RANK[a] >= RC_RANK[b] else b


def cmd_check(a):
    allow = load_allow(a.project_dir)
    floors = allow.get("min_tu_compiles", {})
    fields, rc = [], 0
    for dom in domains_of(a.domain):
        try:
            jobs, res, stats = sweep(a.project_dir, dom, a.jobs, a.timeout, not a.quiet)
        except SystemExit as e:
            print(str(e), file=sys.stderr)
            fields.append(f"{dom}=UNRUNNABLE")
            rc = worst(rc, 2)
            continue
        summ = aggregate(dom, a.project_dir, jobs, res)
        summ["meta"].update(stats)
        write_summary(a.project_dir, dom, summ)
        m = summ["meta"]
        bad, stale = evaluate(summ, allow)
        vc = {}
        for c in summ["classified"].values():
            vc[c["verdict"]] = vc.get(c["verdict"], 0) + 1
        floor = int(floors.get(dom, 0))
        print(f"layout_odr[{dom}]: {m['answered']}/{m['tu_compiles']} TU compiles answered"
              f" (floor {floor}), {m['class_rows']} class rows, {m['distinct_names']} names;"
              f" names with >1 layout: {len(summ['divergent'])} = {dict(sorted(vc.items()))};"
              f" cached {stats['cached']}, ran {stats['ran']} in {stats['secs']}s")
        for f in m["failed"]:
            print(f"  UNANSWERED {f['tu']}: {f['diags'][0] if f['diags'] else '?'}")
        for name, c, fps in bad:
            print(f"  {c['verdict']} {name}")
            for ident, g in c["splits"].items():
                for fp, t in g.items():
                    print(f"      {ident} {fp}: {len(t)} TU(s), e.g. {', '.join(t[:3])}")
            for u in c["unresolved"][:4]:
                print(f"      {u['fp']} in {u['tu']}: {u['note'][:160]}")
            print(f"      -> python3 tools/layout_odr.py show --domain {dom} '{name}'")
        for e in stale:
            print(f"  STALE allowlist entry {e['verdict']} {e['name']} {e['fps']} -- the"
                  f" layouts it pins no longer occur; remove it"
                  + ("" if a.strict else " (warning; fatal under --strict)"))
        vac = m["tu_compiles"] < floor
        if vac:
            print(f"  VACUOUS: {m['tu_compiles']} TU compiles < floor {floor}")
        drc = 3 if (m["failed"] or vac) else (1 if (bad or (stale and a.strict)) else 0)
        rc = worst(rc, drc)
        fields.append(f"{dom}_tus={m['tu_compiles']} {dom}_failed={len(m['failed'])}"
                      f" {dom}_split={sum(1 for b in bad if b[1]['verdict'] == 'SPLIT')}"
                      f" {dom}_unresolved={sum(1 for b in bad if b[1]['verdict'] == 'UNRESOLVED')}"
                      f" {dom}_allowed={sum(1 for c in summ['classified'].values() if c['verdict'] in ('SPLIT', 'UNRESOLVED')) - len(bad)}"
                      f" {dom}_stale={len(stale)}")
    verdict = {0: "PASS", 1: "FAIL", 2: "UNRUNNABLE", 3: "UNANSWERED"}[rc]
    print(f"LAYOUT_ODR_RESULT verdict={verdict} " + " ".join(fields) + f" rc={rc}")
    if a.allow_template:
        out = []
        for dom in domains_of(a.domain):
            try:
                summ = load_summary(a.project_dir, dom)
            except SystemExit:
                continue
            for name, c, fps in evaluate(summ, allow)[0]:
                out.append({"domain": dom, "name": name, "verdict": c["verdict"],
                            "fps": fps, "reason": "TODO: why this is accepted"})
        print(json.dumps(out, indent=1))
    return rc


# --------------------------------------------------------------- selftest
#
# Every leg must be able to FAIL.  The positive control (a macro-gated member,
# i.e. the MetaPerformer shape) must come out SPLIT; the negative controls
# (nested classes sharing a bare name, anonymous-namespace classes in two TUs)
# must NOT.  A classifier that calls everything COLLISION passes the negatives
# and fails the positive; one that calls everything SPLIT does the reverse.

FIX_H = """\
struct LayoutOdrProbe {
    int a;
#ifdef LAYOUT_ODR_PROBE_WIDE
    int b;
#endif
};
struct LayoutOdrHost1 { struct Inner { int x; }; Inner i; };
struct LayoutOdrHost2 { struct Inner { double y; int z; }; Inner i; };
"""
FIX_A = """\
#include "odr_probe.h"
namespace { struct LayoutOdrLocal { int q; }; }
LayoutOdrProbe g_probe; LayoutOdrHost1 g_h1; LayoutOdrHost2 g_h2; LayoutOdrLocal g_l;
"""
FIX_B = "#define LAYOUT_ODR_PROBE_WIDE\n" + FIX_A.replace(
    "struct LayoutOdrLocal { int q; };", "struct LayoutOdrLocal { float r[3]; char c; };")


def _check(results, name, cond, detail):
    results.append((name, bool(cond), detail))


def selftest_offline():
    R = []
    # A. interleaved notes, all three path spellings
    raw = ("x.cpp\nclass Foo\tsize(8):\n\t+-Note: including file: src/band3\\game/D.h\n--\n"
           " 0\t| a\n 4\tNote: including file: z:\\tmp\\Q.h\n| b\n\t+---\n")
    deps, classes, diags = parse_x360(raw, "/r", "/r")
    _check(R, "A1 notes are cut out mid-line", classes and "\t+---\n 0\t| a\n 4\t| b" in classes[0][1],
           repr(classes[0][1] if classes else None))
    _check(R, "A2 backslash + drive-letter deps normalized",
           deps == ["@ROOT@/src/band3/game/D.h", "/tmp/Q.h"], deps)
    _check(R, "A3 an error line is a diagnostic", parse_x360(
        "a.cpp(3) : error C2065: 'x' : undeclared identifier\n", "/r", "/r")[2], "")
    raw2 = ("class P\tsize(4):\n\t+---\ncl : Command line warning D9025 : overriding '/O1'"
            " with '/Od'\n 0\t| a\n\t+---\n")
    _, cl2, d2 = parse_x360(raw2, "/r", "/r")
    _check(R, "A4 driver warnings (which /w does not silence) are cut out",
           cl2 and "Command line" not in cl2[0][1] and not d2, cl2)
    nat = ("*** Dumping AST Record Layout\n         0 | class HolmesInput (empty)\n"
           "           | [sizeof=1, dsize=1, align=1,\n           |  nvsize=1, nvalign=1]\n")
    _, ncl, _d = parse_native(nat, "", "/r")
    _check(R, "A5 clang's `(empty)` header annotation is not part of the name",
           ncl and ncl[0][0] == "HolmesInput" and "(empty)" in ncl[0][1], ncl)
    # B. block facts
    blk = ("class Target\tsize(100):\n\t+---\n\t| +--- (base class ?$ObjRefConcrete@VX@@)\n"
           " 0\t| | mOwner\n\t| +---\n 4\t| Symbol mTarget\n"
           "92.\t| mForceLod (bitstart=29,nbits=3)\n\t+---\n\nTarget::$vftable@:\n"
           " 0\t| &Target::{dtor}\n 1\t| &Target::Poll\n")
    size, mem, bases, virt, types, _all = block_facts(blk)
    _check(R, "B1 direct members, bitfield name, no vftable rows",
           mem == {"mTarget", "mForceLod"}, mem)
    _check(R, "B2 template base demangled", bases == {"ObjRefConcrete"}, bases)
    _check(R, "B3 virtuals + member types", virt == {"Poll"} and "Symbol" in types, (virt, types))
    vb = ("class I\tsize(16):\n\t+---\n 0\t| {vbptr}\n 4\t| mIsValid\n\t+---\n"
          "\t+--- (virtual base Object)\n\t| +--- (base class ObjRefOwner)\n 8\t| | {vfptr}\n"
          "\t| +---\n12\t| mDir\n\t+---\n\nI::$vbtable@:\n 0\t| 0\n 1\t| 8 (Id(I+0)Object)\n")
    _s, vm, vbases, *_r = block_facts(vb)
    _check(R, "B4 virtual-base and vbtable rows are not this class's members",
           vm == {"mIsValid"} and not vbases, (vm, vbases))
    # C. source index
    src = """
namespace NS { class Outer { struct In { int a; }; }; }
template <class T> struct Tm { struct Nest { T t; }; };
namespace { struct Anon { int z; }; }
struct Ret *MakeRet(int a) { struct InFn { int y; } v; return 0; }
enum class Colour { Red, Green };
struct Plain { int p, q[4]; static int s; void (*fn)(int); unsigned bits : 3; };
"""
    ds = {d.qname: d for d in index_source(src, "f.h")}
    _check(R, "C1 nested qualification", "NS::Outer::In" in ds, sorted(ds))
    _check(R, "C2 template nest flagged", ds.get("Tm::Nest") and ds["Tm::Nest"].template, "")
    _check(R, "C3 anonymous namespace is local", ds.get("Anon") and ds["Anon"].local, "")
    _check(R, "C4 function returning a class pointer is not a class", "Ret" not in ds, sorted(ds))
    _check(R, "C5 function-local class is local", ds.get("InFn") and ds["InFn"].local, "")
    _check(R, "C6 enum class is not a class", "Colour" not in ds, sorted(ds))
    _check(R, "C7 data members (not static, fn-ptr, bitfield)",
           ds.get("Plain") and ds["Plain"].members == ["p", "q", "fn", "bits"],
           ds.get("Plain") and ds["Plain"].members)
    # D. allowlist semantics
    def summ(**cls):
        return {"meta": {"domain": "x360"}, "classified": {
            n: {"verdict": "SPLIT", "splits": {n: {fp: ["t.cpp"] for fp in fps}},
                "unresolved": []} for n, fps in cls.items()}}
    pin = {"domain": "x360", "name": "A", "verdict": "SPLIT", "fps": ["f1", "f2"]}
    lst = {"domain": "x360", "verdict": "SPLIT", "names": ["Q::B", "Q::C"]}
    allow = {"entries": [pin, lst]}
    bad, stale = evaluate(summ(A=["f1", "f2"], **{"Q::B": ["g1", "g2"]}), allow)
    _check(R, "D1 a PIN and a LIST accept what they name; the unused list name is stale",
           not bad and len(stale) == 1 and stale[0]["fps"] == ["Q::C"], (bad, stale))
    bad, stale = evaluate(summ(A=["f1", "f3"]), allow)
    _check(R, "D2 a PIN re-fires when one pinned layout changes",
           [b[0] for b in bad] == ["A"], bad)
    bad, _ = evaluate(summ(**{"Q::D": ["h1", "h2"]}), allow)
    _check(R, "D3 a LIST does not accept a name it does not list",
           [b[0] for b in bad] == ["Q::D"], bad)
    pre = {"entries": [dict(lst, identity_prefix="Q::")]}
    bad, _ = evaluate(summ(**{"Q::B": ["g1", "g2"]}), pre)
    bad2, _ = evaluate({"meta": {"domain": "x360"}, "classified": {"Q::B": {
        "verdict": "SPLIT", "splits": {"B": {"g1": ["t.cpp"], "g2": ["u.cpp"]}},
        "unresolved": []}}}, pre)
    _check(R, "D4 a LIST's identity_prefix keeps a same-named class outside it out",
           not bad and [b[0] for b in bad2] == ["Q::B"], (bad, bad2))
    return R


def selftest_live(root, domain):
    """Compile the fixture with the real toolchain and run the real classifier."""
    R = []
    tmp = tempfile.mkdtemp(dir=os.path.expanduser("~/tmp"), prefix="layout_odr_selftest_")
    store = Store(os.path.join(tmp, "cache"))
    hasher = FileHasher(root)
    for n, t in (("odr_probe.h", FIX_H), ("a.cpp", FIX_A), ("b.cpp", FIX_B)):
        open(os.path.join(tmp, n), "w").write(t)
    jobs_all = x360_jobs(root) if domain == "x360" else native_jobs(root)
    if not jobs_all:
        return [("L0 have a template compile", False, "no jobs")]
    tpl = jobs_all[0]
    mine = []
    for n in ("a.cpp", "b.cpp"):
        src = os.path.join(tmp, n)
        argv = list(tpl.argv)
        # the source operand is the last argument in both builds; cl parses an
        # absolute /home path as an option, so hand it a relative one
        argv[-1] = os.path.relpath(src, tpl.cwd) if domain == "x360" else src
        if domain == "native":
            argv = [a for a in argv if a != tpl.argv[-1]] + [src]
        j = Job(domain, rootify(src, root, "/"), argv, tpl.env, tpl.cwd,
                [a for a in argv if not a.startswith("/Fo")])
        r, _ = run_job(j, root, store, hasher, 900)
        mine.append((j, r))
    for j, r in mine:
        _check(R, f"L1 {os.path.basename(j.tu)} compiled", r["status"] == OK,
               r.get("diags"))
    if not all(r["status"] == OK for _j, r in mine):
        return R
    names = {}
    for j, r in mine:
        for n, fp in r["classes"]:
            names.setdefault(n, {}).setdefault(fp, []).append(j.tu)
    div = {n: v for n, v in names.items() if len(v) > 1}
    if domain == "x360":
        cls = classify_x360(div, {j.tu: r["deps"] for j, r in mine}, root, store, hasher)
        v = {n: cls.get(n, {}).get("verdict") for n in ("LayoutOdrProbe", "Inner", "LayoutOdrLocal")}
        _check(R, "L2 POSITIVE: macro-gated member is a SPLIT", v["LayoutOdrProbe"] == "SPLIT", v)
        _check(R, "L3 NEGATIVE: nested same-name classes are a COLLISION",
               v["Inner"] == "COLLISION", cls.get("Inner"))
        _check(R, "L4 NEGATIVE: anonymous-namespace classes are not a SPLIT",
               v["LayoutOdrLocal"] in (None, "COLLISION"), cls.get("LayoutOdrLocal"))
        summ = {"meta": {"domain": "x360"}, "classified": cls}
        fps = entry_fps(cls["LayoutOdrProbe"]) if "LayoutOdrProbe" in cls else []
        ok_allow = {"entries": [{"domain": "x360", "name": "LayoutOdrProbe",
                                 "verdict": "SPLIT", "fps": fps}]}
        bad_allow = {"entries": [{"domain": "x360", "name": "LayoutOdrProbe",
                                  "verdict": "SPLIT", "fps": fps[:1] + ["0" * 20]}]}
        b1, s1 = evaluate(summ, ok_allow)
        b2, s2 = evaluate(summ, bad_allow)
        _check(R, "L5 allowlist accepts the exact fingerprints",
               not [x for x in b1 if x[0] == "LayoutOdrProbe"] and not s1, (b1, s1))
        _check(R, "L6 allowlist REJECTS changed fingerprints (and reports it stale)",
               [x for x in b2 if x[0] == "LayoutOdrProbe"] and s2, (b2, s2))
    else:
        _check(R, "L2 POSITIVE: macro-gated member diverges", "LayoutOdrProbe" in div, sorted(div))
        _check(R, "L3 NEGATIVE: qualified nested names do not collide",
               not any("Inner" in n for n in div), sorted(div))
        _check(R, "L4 NEGATIVE: anonymous-namespace classes are skipped",
               not any("LayoutOdrLocal" in n for n in div), sorted(div))
    import shutil
    shutil.rmtree(tmp, ignore_errors=True)
    return R


def cmd_selftest(a):
    R = selftest_offline()
    if a.live:
        for dom in domains_of(a.domain):
            try:
                R += [(f"[{dom}] {n}", ok, d) for n, ok, d in selftest_live(a.project_dir, dom)]
            except SystemExit as e:
                R.append((f"[{dom}] live leg runnable", False, str(e)))
    fails = 0
    for n, ok, d in R:
        print(("PASS " if ok else "FAIL ") + n + ("" if ok else f"   -- {str(d)[:300]}"))
        fails += not ok
    print(f"layout_odr selftest: {len(R) - fails}/{len(R)} legs pass")
    return 1 if fails else 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0],
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--project-dir", default=os.environ.get("RB3_PROJECT_DIR", REPO))
    ap.add_argument("--native-build", default=None,
                    help="native CMake build dir to read (default <project-dir>/native/build);"
                         " e.g. one configured against an engine branch")
    sub = ap.add_subparsers(dest="cmd", required=True)

    def common(p):
        p.add_argument("--domain", choices=["x360", "native", "all"], default="all")

    s = sub.add_parser("sweep", help="lay out every class in every TU (cached)")
    common(s)
    s.add_argument("-j", "--jobs", type=int, default=12)
    s.add_argument("--timeout", type=int, default=3600)
    s.add_argument("--only", action="append", help="restrict to TUs containing this substring")
    s.add_argument("--quiet", action="store_true")
    s.set_defaults(func=cmd_sweep)

    s = sub.add_parser("check", help="sweep (cached) + classify + compare with the allowlist")
    common(s)
    s.add_argument("-j", "--jobs", type=int, default=12)
    s.add_argument("--timeout", type=int, default=3600)
    s.add_argument("--quiet", action="store_true")
    s.add_argument("--strict", action="store_true",
                   help="a stale allowlist entry fails the check")
    s.add_argument("--allow-template", action="store_true",
                   help="also print allowlist entries for every failing name (reason TODO)")
    s.set_defaults(func=cmd_check)

    s = sub.add_parser("selftest", help="offline legs; --live also compiles a fixture")
    common(s)
    s.add_argument("--live", action="store_true")
    s.set_defaults(func=cmd_selftest)

    s = sub.add_parser("show", help="print every layout of a class and the diffs")
    common(s)
    s.add_argument("names", nargs="+")
    s.add_argument("--max-tus", type=int, default=6)
    s.add_argument("--context", type=int, default=2)
    s.set_defaults(func=cmd_show)

    a = ap.parse_args(argv)
    a.project_dir = os.path.abspath(a.project_dir)
    global NATIVE_BUILD
    NATIVE_BUILD = os.path.abspath(a.native_build) if a.native_build else None
    return a.func(a)


if __name__ == "__main__":
    sys.exit(main())
