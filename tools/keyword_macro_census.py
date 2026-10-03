#!/usr/bin/env python3
"""Census: does any compiled TU end with a compiler keyword defined as a macro?

A header that does `#define __declspec(x)` (src/compiler_macros.h did, for every
compiler but Metrowerks) deletes every later `__declspec(noinline)` in the TU,
and nothing at the call site shows it (W16-OZ). This tool preprocesses, for every
msvc compile edge of `ninja -t commands all_source`, a wrapper that #includes the
TU and then emits a marker for each keyword in KW still #defined at the end of it.

Controls, all required for a PASS:
  * `_MSC_VER` must read defined in every TU (otherwise the probe is vacuous);
  * POSITIVE: a wrapper that does `#define __declspec(x)` before a real TU must be
    flagged (otherwise the probe cannot fail);
  * HEADER: a wrapper that includes src/compiler_macros.h before a real TU must
    flag nothing, and DONT_INLINE must expand to `__declspec(noinline)`.

Limits: a keyword macro that is defined and then #undef'd inside a TU is not
seen (grep for `#undef <keyword>` covers that). Allowed: `true`/`false` in C
(`/TC`) TUs, where MSVC C has no bool (curl's setup_once.h).

Usage: python3 tools/keyword_macro_census.py [--worktree DIR] [-j N] [--json OUT]
Exit: 0 PASS, 1 a keyword macro is live in some TU, 2 a control failed.
"""
import argparse, json, os, re, shlex, subprocess, sys
from collections import Counter
from concurrent.futures import ThreadPoolExecutor

KW = """__declspec __attribute__ __forceinline __inline inline __restrict
__cdecl __stdcall __fastcall __thiscall __unaligned __w64 __ptr32 __ptr64 __int8
__int16 __int32 __int64 __alignof __asm __assume __noop __super __uuidof __wchar_t
const volatile static register virtual explicit mutable typename template signed
unsigned bool true false wchar_t new delete this throw try catch operator sizeof
class struct friend extern auto char short int long float double void enum union
return goto namespace using private public protected""".split()
CONTROL = "_MSC_VER"
ALLOWED_C = {"true", "false"}
NAMES = KW + [CONTROL]
WRAPDIR = "build/45410914/kwprobe"

def wrapper(src, pre_lines=(), expand=False):
    out = list(pre_lines) + [f'#include "../../../{src}"']
    for i, k in enumerate(NAMES):
        out.append(f"#ifdef {k}\nKWPROBEHIT_{i}\n#endif")
    if expand:
        out.append("KWPROBE_DONT_INLINE_BEGIN DONT_INLINE KWPROBE_DONT_INLINE_END")
    return "\n".join(out) + "\n"

def jobs_from(wt):
    cmds = subprocess.run(["ninja", "-t", "commands", "all_source"], cwd=wt,
                          capture_output=True, text=True, check=True).stdout
    jobs = []
    for line in cmds.splitlines():
        if "cl.exe" not in line or "/Fo" not in line:
            continue
        toks = shlex.split(line)
        cl = toks[toks.index("--") + 1:] if "--" in toks else toks
        cl = [t for t in cl if "=" not in t or t.startswith("/")]
        keep = [t for t in cl[:-1] if not t.startswith(("/Yu", "/Yc", "/Fp", "/Fo"))
                and t not in ("/showIncludes", "/c")]
        jobs.append((cl[-1], keep))
    return jobs

def run(wt, job, tag="", pre_lines=(), expand=False):
    src, keep = job
    ext = ".c" if "/TC" in keep else ".cpp"
    name = re.sub(r"[^A-Za-z0-9]", "_", src) + tag + ext
    with open(os.path.join(wt, WRAPDIR, name), "w") as f:
        f.write(wrapper(src, pre_lines, expand))
    r = subprocess.run(keep + ["/EP", f"{WRAPDIR}/{name}"], cwd=wt,
                       env=dict(os.environ, WIBO_FS_CACHE="1"),
                       capture_output=True, text=True, errors="replace")
    hits = [NAMES[int(h)] for h in sorted({int(m) for m in
            re.findall(r"KWPROBEHIT_(\d+)", r.stdout)})]
    m = re.search(r"KWPROBE_DONT_INLINE_BEGIN(.*?)KWPROBE_DONT_INLINE_END", r.stdout, re.S)
    return {"src": src, "tag": tag, "rc": r.returncode, "hits": hits,
            "c": "/TC" in keep, "dont_inline": " ".join(m.group(1).split()) if m else None,
            "err": r.stderr[-400:] if r.returncode else ""}

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--worktree", default=".")
    ap.add_argument("-j", type=int, default=16)
    ap.add_argument("--json")
    a = ap.parse_args()
    wt = os.path.abspath(a.worktree)
    os.makedirs(os.path.join(wt, WRAPDIR), exist_ok=True)
    jobs = jobs_from(wt)
    cpp = next(j for j in jobs if "/TP" in j[1])
    with ThreadPoolExecutor(max_workers=a.j) as ex:
        res = list(ex.map(lambda j: run(wt, j), jobs))
        pos = run(wt, cpp, "__POS", ["#define __declspec(x)"])
        hdr = run(wt, cpp, "__HDR", ['#include "../../../src/compiler_macros.h"'], expand=True)

    fails = [r for r in res if r["rc"] != 0]
    noctrl = [r for r in res if r["rc"] == 0 and CONTROL not in r["hits"]]
    live = []
    for r in res:
        bad = [k for k in r["hits"] if k != CONTROL and not (r["c"] and k in ALLOWED_C)]
        if bad:
            live.append((r["src"], bad))
    allowed = [r["src"] for r in res if r["c"] and set(r["hits"]) & ALLOWED_C]

    print(f"TUs={len(jobs)} preprocess_failures={len(fails)} missing_{CONTROL}={len(noctrl)}")
    pos_ok = pos["rc"] == 0 and "__declspec" in pos["hits"]
    hdr_ok = (hdr["rc"] == 0 and set(hdr["hits"]) == {CONTROL}
              and hdr["dont_inline"] == "__declspec(noinline)")
    print(f"POSITIVE control ({pos['src']} + #define __declspec(x)): "
          f"{'flagged' if pos_ok else 'NOT FLAGGED'} hits={pos['hits']}")
    print(f"HEADER check (compiler_macros.h + {hdr['src']}): rc={hdr['rc']} "
          f"hits={hdr['hits']} DONT_INLINE -> {hdr['dont_inline']!r} "
          f"{'OK' if hdr_ok else 'FAIL'}{' ' + hdr['err'] if hdr['err'] else ''}")
    print(f"allowed C-mode true/false: {allowed}")
    print(f"keyword macros live at end of TU: {len(live)} TU(s) "
          f"{dict(Counter(k for _, ks in live for k in ks))}")
    for s, ks in live:
        print("   ", s, ks)
    for r in fails[:10]:
        print("PREPROCESS FAIL", r["src"], r["err"])
    if a.json:
        json.dump({"results": res, "positive": pos, "header": hdr}, open(a.json, "w"), indent=1)
    if fails or noctrl or not pos_ok or not hdr_ok:
        print("VERDICT: CONTROL FAILED (exit 2)")
        return 2
    if live:
        print("VERDICT: FAIL (exit 1)")
        return 1
    print("VERDICT: PASS")
    return 0

if __name__ == "__main__":
    sys.exit(main())
