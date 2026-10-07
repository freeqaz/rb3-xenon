#!/usr/bin/env python3
"""Where did a native run's files come from? (lane W16-UC)

Retail RB3 reads every relative path out of the disc archive: its FileIsLocal
is "drive letter longer than one character", so a plain path is never local and
NewFile builds an ArkFile. The only host files a console opens are on the disc
itself (gen/main_xbox.hdr and the ten main_xbox_N.ark parts). This tool checks
that a native run did the same, and fails if any read reached outside the disc
image without being on an explicit allow-list.

Two records of the same run are read:

  * an strace of the process tree (open/openat/openat2/creat), which sees every
    open the process made, whoever made it -- engine, driver, libc, GPU stack;
  * the engine's own ledger (RB3_FILE_LEDGER, native/src/platform/
    FileLedger_Native.cpp), which says which door each engine open came
    through: ARK <key> ok|miss, HOST r|w <path> ok|fail, and LOCAL <name> <why>
    whenever FileIsLocal sent a name to the host instead of the archive, and
    MAP milo|dta <requested> <opened> for the two name rewrites (.milo ->
    gen/.milo_xbox in DirLoader, .dta -> gen/.dtb in DataReadFile/DataLoader).
    Each MAP is recomputed here with retail's rule; a mismatch is a violation.

Every host open is put in exactly one class:

  DISC      under the disc image (--assets): ARK_HDR, ARK_PART or DISC_LOOSE
  INPUT     a path the harness declared with --input (a chart, a reference)
  OUTPUT    opened write-only (dumps, PNGs, logs): reported, not judged
  ALLOW:<c> matched a line of the allow-list (system libraries, GPU stack ...)
  OUTSIDE   anything else -- a VIOLATION, whether the open succeeded or not

A failed open counts too: an attempt to read a host file the disc does not
hold is the bug whether or not the file happened to exist on this machine
(W16-UA's 16 host reads succeeded only because a sibling checkout was there).

USAGE
  native_file_audit.py check --trace T [--ledger L] --assets DIR
        [--input PATH]... [--allow FILE] [--expect-disc] [--label NAME]
        [--cwd DIR] [--json OUT]
  native_file_audit.py strace-argv --trace T     # the strace prefix to run under

Exit codes: 0 clean, 1 violation(s), 2 could not judge (no trace, empty trace,
or --expect-disc and the run read nothing from the disc -- a vacuous pass).
"""

import argparse
import fnmatch
import json
import os
import re
import shutil
import sys
from collections import Counter, defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_ALLOW = os.path.join(HERE, "native_file_audit_allow.txt")

TRACED = "open,openat,openat2,creat,chdir,fchdir"


def strace_argv(trace):
    exe = shutil.which("strace")
    if not exe:
        return None
    # -f: follow threads and children. -qq: no attach/exit chatter.
    # --seccomp-bpf: only the traced syscalls stop the tracee (cheap).
    # -y: print the path behind a dirfd, so openat(dirfd, rel) resolves.
    # -s 4096: never truncate a path.
    return [exe, "-f", "-qq", "--seccomp-bpf", "-y", "-s", "4096",
            "-e", "trace=" + TRACED, "-e", "signal=none", "-o", trace, "--"]


# --------------------------------------------------------------- strace ----
LINE = re.compile(r"^(?P<pid>\d+)\s+(?P<rest>.*)$")
UNFIN = re.compile(r"^(?P<call>\w+)\((?P<args>.*) <unfinished \.\.\.>$")
RESUMED = re.compile(r"^<\.\.\. (?P<call>\w+) resumed>(?P<tail>.*)$")
CALL = re.compile(r"^(?P<call>\w+)\((?P<args>.*)\)\s+=\s+(?P<ret>-?\d+|\?)(?P<err>.*)$")
STR = r'"((?:[^"\\]|\\.)*)"'


def unescape(s):
    out = bytearray()
    i = 0
    b = s.encode("latin-1", "surrogateescape")
    while i < len(b):
        c = b[i]
        if c == 0x5C and i + 1 < len(b):
            n = b[i + 1]
            if n == ord("x") and i + 3 < len(b):
                out.append(int(b[i + 2:i + 4], 16)); i += 4; continue
            m = {ord("n"): 10, ord("t"): 9, ord("r"): 13, ord("\\"): 92,
                 ord('"'): 34, ord("v"): 11, ord("f"): 12}.get(n)
            if m is not None:
                out.append(m); i += 2; continue
            if 0x30 <= n <= 0x37:
                j = i + 1; v = 0
                while j < len(b) and j < i + 4 and 0x30 <= b[j] <= 0x37:
                    v = v * 8 + (b[j] - 0x30); j += 1
                out.append(v & 0xFF); i = j; continue
        out.append(c); i += 1
    return out.decode("utf-8", "surrogateescape")


def parse_trace(path, start_cwd):
    """Yield dicts {call, path, flags, ok, err} with an ABSOLUTE path."""
    pending = {}
    cwd = start_cwd
    events = []
    with open(path, "r", encoding="latin-1") as f:
        for raw in f:
            raw = raw.rstrip("\n")
            m = LINE.match(raw)
            if m:
                pid, rest = m.group("pid"), m.group("rest")
            else:
                pid, rest = "0", raw
            um = UNFIN.match(rest)
            if um:
                pending[pid] = (um.group("call"), um.group("args"))
                continue
            rm = RESUMED.match(rest)
            if rm and pid in pending:
                call, args = pending.pop(pid)
                rest = "%s(%s%s" % (call, args, rm.group("tail"))
            cm = CALL.match(rest)
            if not cm:
                continue
            call, args, ret = cm.group("call"), cm.group("args"), cm.group("ret")
            ok = ret != "?" and int(ret) >= 0
            err = cm.group("err").strip()
            if call == "chdir":
                sm = re.match(STR, args)
                if sm and ok:
                    cwd = os.path.normpath(os.path.join(cwd, unescape(sm.group(1))))
                continue
            if call == "fchdir":
                fm = re.match(r"\d+<(.*)>", args)
                if fm and ok:
                    cwd = unescape(fm.group(1))
                continue
            base = cwd
            if call in ("openat", "openat2"):
                # With -y the dirfd prints as `AT_FDCWD</cwd>` or `N</dir>`.
                dm = re.match(r"(?:AT_FDCWD|\d+)(?:<((?:[^>\\]|\\.)*)>)?\s*,\s*", args)
                if not dm:
                    continue
                if dm.group(1) is not None:
                    base = unescape(dm.group(1))
                args = args[dm.end():]
            sm = re.match(STR + r"\s*(?:,\s*(.*))?$", args)
            if not sm:
                continue
            p = unescape(sm.group(1))
            flags = sm.group(2) or ""
            if call == "creat":
                flags = "O_WRONLY|O_CREAT|O_TRUNC"
            absp = os.path.normpath(p if p.startswith("/") else os.path.join(base, p))
            events.append({"call": call, "path": absp, "flags": flags, "ok": ok,
                           "err": err})
    return events


def is_write_only(flags):
    return "O_WRONLY" in flags


# --------------------------------------------------------------- ledger ----
def parse_ledger(path):
    recs = []
    if not path or not os.path.exists(path):
        return recs
    with open(path, "r", encoding="utf-8", errors="surrogateescape") as f:
        for line in f:
            parts = line.rstrip("\n").split("\t")
            if parts and parts[0] in ("ARK", "HOST", "LOCAL", "MAP"):
                recs.append(parts)
    return recs


# ---------------------------------------------------- retail name rules ----
# Re-implemented from os/File.cpp and obj/{DirLoader,DataFile}.cpp, so a MAP
# record (requested name -> name the engine opened) is checked against the rule
# retail applies, not against the engine's own opinion of itself.
def file_get_path(f):
    i = max(f.rfind("/"), f.rfind("\\"))
    if i < 0:
        return "."
    if i == 0 or f[i - 1] == ":":
        return f[:i + 1]
    return f[:i]


def file_get_base(f):
    i = f.rfind("/")
    if i < 0:
        i = f.rfind("\\")
    b = f[i + 1:]
    j = b.rfind(".")
    return b[:j] if j >= 0 else b


def file_get_ext(f):
    for k in range(len(f) - 1, -1, -1):
        if f[k] == ".":
            return f[k + 1:]
        if f[k] in "/\\":
            return ""
    return ""


def retail_is_local(f):
    # File_Win.cpp FileIsLocal: the drive before ':' is longer than 1 char.
    # An absolute HOST path is native's stand-in for a device path (it is how
    # a harness hands an engine reader a declared input), so it is local too;
    # whether that host file was allowed is the host classifier's question.
    if f.startswith("/"):
        return True
    i = f.find(":")
    return i > 1


def retail_milo_path(req):
    # DirLoader::CachedPath under retail's unconditional cache mode.
    if file_get_ext(req) == "milo":
        return "%s/gen/%s.milo_xbox" % (file_get_path(req), file_get_base(req))
    return req


def retail_dta_path(req):
    # CachedDataFile, inlined into DataReadFile and DataLoader's ctor.
    if ".dtb" in req:
        return req
    if not retail_is_local(req):
        return "%s/gen/%s.dtb" % (file_get_path(req), file_get_base(req))
    return req


# ------------------------------------------------------------- classify ----
def load_allow(path):
    rules = []
    if not path:
        return rules
    with open(path) as f:
        for n, line in enumerate(f, 1):
            line = line.split("#", 1)[0].strip()
            if not line:
                continue
            cat, _, pat = line.partition(" ")
            pat = os.path.expanduser(pat.strip())
            if not pat.startswith("/"):
                raise SystemExit("%s:%d: allow-list pattern must be absolute: %r"
                                 % (path, n, pat))
            rules.append((cat, pat))
    return rules


class Classifier:
    def __init__(self, assets, inputs, allow):
        self.disc = os.path.realpath(assets) if assets else None
        self.disc_lex = os.path.normpath(os.path.abspath(assets)) if assets else None
        self.inputs = {os.path.realpath(p) for p in inputs}
        self.allow = allow

    def _under(self, p, root):
        return root and (p == root or p.startswith(root + "/"))

    def classify(self, path, write_only):
        real = os.path.realpath(path)
        for p in (real, path):
            for root in (self.disc, self.disc_lex):
                if self._under(p, root):
                    rel = os.path.relpath(p, root)
                    if re.fullmatch(r"gen/main_xbox\.hdr", rel):
                        return "DISC", "ARK_HDR"
                    if re.fullmatch(r"gen/main_xbox_\d+\.ark", rel):
                        return "DISC", "ARK_PART"
                    if rel == ".":
                        return "DISC", "DISC_ROOT"
                    return "DISC", "DISC_LOOSE"
        if real in self.inputs or path in self.inputs:
            return "INPUT", "INPUT"
        if write_only:
            return "OUTPUT", "OUTPUT"
        for cat, pat in self.allow:
            if fnmatch.fnmatchcase(path, pat) or fnmatch.fnmatchcase(real, pat):
                return "ALLOW", "ALLOW:" + cat
        return "OUTSIDE", "OUTSIDE"


def cmd_check(a):
    label = a.label or os.path.basename(a.trace)
    if not os.path.exists(a.trace):
        print("FILE-AUDIT %s: UNRUNNABLE -- no trace at %s" % (label, a.trace))
        return 2
    extra = []
    for spec in a.allow_glob or []:
        cat, _, pat = spec.partition("=")
        if not pat.startswith("/"):
            print("FILE-AUDIT %s: UNRUNNABLE -- --allow-glob needs CAT=/abs/glob, "
                  "got %r" % (label, spec))
            return 2
        extra.append((cat, pat))
    cls = Classifier(a.assets, a.input or [], load_allow(a.allow) + extra)
    events = parse_trace(a.trace, os.path.abspath(a.cwd or os.getcwd()))
    if not events:
        print("FILE-AUDIT %s: UNRUNNABLE -- the trace holds no open at all "
              "(strace did not trace this run)" % label)
        return 2

    by_sub = Counter()
    violations = []
    disc_reads = 0
    seen = set()
    for e in events:
        wo = is_write_only(e["flags"])
        top, sub = cls.classify(e["path"], wo)
        key = (e["path"], wo, e["ok"])
        by_sub[(sub, "ok" if e["ok"] else "fail")] += 1
        if top == "DISC" and e["ok"] and not wo:
            disc_reads += 1
        if top == "OUTSIDE" and key not in seen:
            violations.append(("strace", e["path"], "ok" if e["ok"] else e["err"]))
        seen.add(key)

    ledger = parse_ledger(a.ledger)
    ark = Counter(); ark_miss = []; host = Counter(); local = Counter()
    maps = Counter()
    for r in ledger:
        if r[0] == "ARK" and len(r) >= 3:
            ark[r[2]] += 1
            if r[2] == "miss":
                ark_miss.append(r[1])
        elif r[0] == "HOST" and len(r) >= 5 and r[4] == "nodevice":
            # A device path ("devkit:/...") the host has no device for: refused
            # before any filesystem call, as a console without that device
            # fails it. Nothing was read, so nothing to judge.
            host[(r[1], "NO_DEVICE", r[3])] += 1
        elif r[0] == "HOST" and len(r) >= 4:
            mode, path, res = r[1], r[2], r[3]
            absp = os.path.normpath(path if path.startswith("/")
                                    else os.path.join(os.path.abspath(a.cwd or os.getcwd()), path))
            top, sub = cls.classify(absp, mode == "w")
            host[(mode, sub, res)] += 1
            if top == "OUTSIDE":
                violations.append(("ledger HOST", absp, res))
        elif r[0] == "LOCAL" and len(r) >= 3:
            local[r[2]] += 1
        elif r[0] == "MAP" and len(r) >= 4:
            kind, req, got = r[1], r[2], r[3]
            want = retail_milo_path(req) if kind == "milo" else retail_dta_path(req)
            maps[(kind, "mapped" if got != req else "as-is")] += 1
            if got != want:
                violations.append(("name map %s" % kind, req,
                                   "engine opened %s, retail opens %s" % (got, want)))

    # One line per source class, then the details that matter.
    print("FILE-AUDIT %s: %d host open(s) traced, %d engine archive lookup(s)"
          % (label, len(events), sum(ark.values())))
    for (sub, res), n in sorted(by_sub.items()):
        print("  host  %-18s %-4s %6d" % (sub, res, n))
    if ledger:
        print("  ark   lookups ok %d, miss %d (a miss is a name the disc does not "
              "hold; retail fails it the same way)" % (ark["ok"], ark["miss"]))
        for (mode, sub, res), n in sorted(host.items()):
            print("  engine HOST %s %-16s %-4s %6d" % (mode, sub, res, n))
        for (kind, how), n in sorted(maps.items()):
            print("  name  %-4s %-7s %6d  (requested -> opened, checked against retail's rule)"
                  % (kind, how, n))
        for why, n in sorted(local.items()):
            print("  engine LOCAL (FileIsLocal said host) %-9s %6d" % (why, n))
        for name in sorted(set(ark_miss))[:a.show]:
            print("    ark miss: %s" % name)
    elif a.ledger:
        print("  ledger: none written (this target links no engine file layer, "
              "or opened nothing through it)")

    rc = 0
    if violations:
        uniq = sorted(set(violations))
        print("FILE-AUDIT %s: FAIL -- %d read(s) outside the disc image and not "
              "on the allow-list, or name(s) not mapped as retail maps them:"
              % (label, len(uniq)))
        for src, p, res in uniq[:a.show]:
            print("    OUTSIDE [%s] %s  (%s)" % (src, p, res))
        if len(uniq) > a.show:
            print("    ... %d more" % (len(uniq) - a.show))
        rc = 1
    elif a.expect_disc and (disc_reads == 0 or (a.ledger and ark["ok"] == 0)):
        print("FILE-AUDIT %s: UNRUNNABLE -- --expect-disc, but the run read %d "
              "disc file(s) and made %d archive read(s); a clean result here "
              "would be vacuous" % (label, disc_reads, ark["ok"]))
        rc = 2
    else:
        print("FILE-AUDIT %s: PASS -- every read was on the disc image, a "
              "declared input, or allow-listed" % label)

    if a.json:
        with open(a.json, "w") as f:
            json.dump({"label": label, "rc": rc,
                       "host": {"%s/%s" % k: v for k, v in by_sub.items()},
                       "ark": dict(ark), "ark_miss": sorted(set(ark_miss)),
                       "engine_host": {"%s/%s/%s" % k: v for k, v in host.items()},
                       "engine_local": dict(local),
                       "name_map": {"%s/%s" % k: v for k, v in maps.items()},
                       "violations": [list(v) for v in sorted(set(violations))]},
                      f, indent=1)
    return rc


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sp = ap.add_subparsers(dest="cmd", required=True)
    c = sp.add_parser("check")
    c.add_argument("--trace", required=True)
    c.add_argument("--ledger")
    c.add_argument("--assets")
    c.add_argument("--input", action="append")
    c.add_argument("--allow", default=DEFAULT_ALLOW)
    c.add_argument("--allow-glob", action="append", metavar="CAT=GLOB",
                   help="one more allow-list line for this run only (the "
                        "executable under test, its RPATH library probes)")
    c.add_argument("--expect-disc", action="store_true")
    c.add_argument("--label")
    c.add_argument("--cwd", help="the traced process's starting directory")
    c.add_argument("--json")
    c.add_argument("--show", type=int, default=20)
    s = sp.add_parser("strace-argv")
    s.add_argument("--trace", required=True)
    a = ap.parse_args()
    if a.cmd == "strace-argv":
        argv = strace_argv(a.trace)
        if not argv:
            print("strace not found", file=sys.stderr)
            return 2
        print("\n".join(argv))
        return 0
    return cmd_check(a)


if __name__ == "__main__":
    sys.exit(main())
