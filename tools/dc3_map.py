#!/usr/bin/env python3
"""dc3_map.py -- parse the leaked Microsoft linker map for ham_xbox_r.exe and
the dc3 objects.json, then expose `mangled_name -> source .cpp` lookups.

The map file is the load-bearing slow input (~120k lines, ~13 MB), so the parse
result is cached as JSON keyed by the map's mtime under tools/.cache/.

Map line shapes (anchored on the trailing whitespace-delimited '...obj' token):

    0005:000001a0       ?getMasher@KeyChain@@YAXPAE@Z 823301a0 f   keygen_xbox.obj
    0005:000009b0       ?MakeString@@YAPBDPBD@Z      823309b0 f i App.obj
    0001:00000028       ??_C@_05F@hello@               82200028          rndobj:Trans.obj

The 'tag block' between the address and the obj token is whitespace-delimited
single tokens drawn from {'f', 'i', ...}. Only 'f' marks an emitted function;
'f i' means the function was inlined into every caller and never emitted as a
distinct .text symbol. The map still contains both -- we tag them so the caller
can filter.

`.obj` tokens are either `Foo.obj` or `lib:Foo.obj`; the lib prefix is a
*hint*, not a path. dc3 `objects.json` keys like `system/rndobj/Trans.cpp` are
matched by basename first; the lib prefix is used to disambiguate if multiple
.cpp basenames collide.
"""
from __future__ import annotations

import json
import os
import re
import sys
from typing import Iterable

# Match the last whitespace-delimited token: `[lib:]Basename.obj`
# Lib prefix is colon-separated. Allow upper- and lower-case (LIBCMT exists).
OBJ_RE = re.compile(r"^[A-Za-z0-9_\-]+(?::[A-Za-z0-9_\-]+)?\.obj$")
# 8 hex chars; load address is 0x82xxxxxx so we don't accept arbitrary hex
ADDR_RE = re.compile(r"^[0-9a-fA-F]{8}$")

# Tokens that may appear in the tag block between the va and the obj token.
TAG_TOKENS = frozenset({"f", "i"})

DEFAULT_MAP = "../dc3-decomp/orig/373307D9/ham_xbox_r.map"
DEFAULT_OBJECTS = "../dc3-decomp/config/373307D9/objects.json"
CACHE_DIR = os.path.join(os.path.dirname(__file__), ".cache")


def _cache_path(map_path: str) -> str:
    base = os.path.basename(map_path).replace(".", "_")
    return os.path.join(CACHE_DIR, f"dc3_map_index_{base}.json")


def parse_map(path: str = DEFAULT_MAP, *, use_cache: bool = True) -> dict:
    """Return dict mangled_name -> {addr: int, tags: [str], obj: str}.

    Cache key is the .map file's mtime; cache invalidates automatically.
    """
    path = os.path.abspath(path)
    map_mtime = os.path.getmtime(path)
    cache_file = _cache_path(path)

    if use_cache and os.path.exists(cache_file):
        try:
            with open(cache_file) as f:
                blob = json.load(f)
            if blob.get("_meta", {}).get("mtime") == map_mtime:
                return blob["data"]
        except (OSError, ValueError, KeyError):
            pass  # fall through to re-parse

    print(f"[dc3_map] parsing {path} ...", file=sys.stderr)
    out = _parse_map_uncached(path)
    print(f"[dc3_map] indexed {len(out)} symbols", file=sys.stderr)

    if use_cache:
        os.makedirs(CACHE_DIR, exist_ok=True)
        tmp = cache_file + ".tmp"
        with open(tmp, "w") as f:
            json.dump({"_meta": {"mtime": map_mtime, "src": path}, "data": out}, f)
        os.replace(tmp, cache_file)
    return out


def _parse_map_uncached(path: str) -> dict:
    out: dict[str, dict] = {}
    n_dup = 0
    with open(path, "r", errors="replace") as f:
        for line in f:
            line = line.rstrip("\n")
            if not line or not line.startswith(" 0005:"):
                # only .text/.text$yc/.text$yd content has addresses we care
                # about for function attribution. (Filtering early dominates
                # runtime: most of the 120k lines aren't .text.)
                continue
            toks = line.split()
            # shape: [sec:off, name, vaddr, tag*, obj]
            if len(toks) < 4:
                continue
            obj = toks[-1]
            if not OBJ_RE.match(obj):
                continue
            # walk backwards: skip tag tokens until we hit the vaddr
            i = len(toks) - 2
            tags: list[str] = []
            while i > 0 and toks[i] in TAG_TOKENS:
                tags.append(toks[i])
                i -= 1
            tags.reverse()
            if i < 2:
                continue
            vaddr_tok = toks[i]
            if not ADDR_RE.match(vaddr_tok):
                continue
            # everything between the section:off and the vaddr is the name --
            # the linker pads with spaces but mangled names don't contain
            # whitespace, so toks[1] is the name. (Some names contain '@'/'?'
            # and survive .split() unchanged.)
            name = toks[1]
            addr = int(vaddr_tok, 16)
            entry = {"addr": addr, "tags": tags, "obj": obj}
            if name in out:
                # duplicates do exist (templates emitted in multiple TUs prior
                # to COMDAT merge); keep the first .text entry and count.
                n_dup += 1
                continue
            out[name] = entry
    if n_dup:
        print(f"[dc3_map] {n_dup} duplicate symbol entries skipped", file=sys.stderr)
    return out


# ---------------------------------------------------------------------------
# objects.json -> obj -> cpp lookup
# ---------------------------------------------------------------------------

def load_objects(path: str = DEFAULT_OBJECTS) -> dict:
    """Return a basename -> [relative_cpp_path, ...] lookup table.

    The dc3 objects.json shape is {module_name: {progress_category, objects:
    {relpath.cpp: status_or_dict}}}. We flatten across modules. Multiple
    modules can claim the same basename (rare but happens for LIBCMT / SDK
    glue) -- the caller disambiguates with the lib prefix.
    """
    with open(path) as f:
        raw = json.load(f)
    by_basename: dict[str, list[str]] = {}
    for module, body in raw.items():
        if not isinstance(body, dict):
            continue
        objs = body.get("objects") or {}
        for relpath, _status in objs.items():
            base = os.path.basename(relpath)
            stem, ext = os.path.splitext(base)
            if ext.lower() not in {".cpp", ".c", ".cc", ".cxx"}:
                continue
            by_basename.setdefault(stem, []).append(relpath)
    return by_basename


def obj_to_cpp(obj_token: str, by_basename: dict, *,
               src_prefix: str = "../dc3-decomp/src") -> str | None:
    """Resolve a map `.obj` token to a relative-to-cwd source path.

    `obj_token` is `[lib:]Basename.obj`. Strategy:
    1. Strip `lib:` prefix (if any). Stem = basename without `.obj`.
    2. Look up `stem` in by_basename. 0 matches -> None.
    3. 1 match -> return prefix/relpath.
    4. >1 matches -> use lib prefix as a directory hint. The .map's `rndobj`
       prefix corresponds to dc3's `system/rndobj/` path, `obj` to
       `system/obj/`, etc. We match by checking which candidate relpath
       contains `/{lib}/` (case-insensitive).
    """
    lib_prefix = None
    tok = obj_token
    if ":" in tok:
        lib_prefix, tok = tok.split(":", 1)
    if not tok.endswith(".obj"):
        return None
    stem = tok[:-4]
    candidates = by_basename.get(stem)
    if not candidates:
        return None
    if len(candidates) == 1:
        return f"{src_prefix}/{candidates[0]}"
    if lib_prefix:
        needle = f"/{lib_prefix.lower()}/"
        for c in candidates:
            if needle in ("/" + c.lower()):
                return f"{src_prefix}/{c}"
    # fallback: shortest path (usually the canonical one)
    candidates.sort(key=len)
    return f"{src_prefix}/{candidates[0]}"


# ---------------------------------------------------------------------------
# MSVC demangling (Itanium ABI tools won't touch MSVC's `?...` scheme)
# ---------------------------------------------------------------------------

_CTOR_DTOR = {
    "?0": "{ctor}",
    "?1": "{dtor}",
    "?2": "operator new",
    "?3": "operator delete",
    "?4": "operator=",
    "?5": "operator>>",
    "?6": "operator<<",
    "?7": "operator!",
    "?8": "operator==",
    "?9": "operator!=",
    "?A": "operator[]",
    "?B": "operator{cast}",
    "?C": "operator->",
    "?D": "operator*",
    "?E": "operator++",
    "?F": "operator--",
    "?G": "operator-",
    "?H": "operator+",
    "?I": "operator&",
    "?J": "operator->*",
    "?K": "operator/",
    "?L": "operator%",
    "?M": "operator<",
    "?N": "operator<=",
    "?O": "operator>",
    "?P": "operator>=",
    "?Q": "operator,",
    "?R": "operator()",
    "?S": "operator~",
    "?T": "operator^",
    "?U": "operator|",
    "?V": "operator&&",
    "?W": "operator||",
    "?X": "operator*=",
    "?Y": "operator+=",
    "?Z": "operator-=",
}


def demangle_msvc(name: str) -> str:
    """Best-effort MSVC demangler covering the patterns we see in the map.

    Returns a human-readable signature; on failure, returns `"?"+name` so
    downstream code can detect failures cheaply (the original mangled name
    starts with `?`, the failure-marked version starts with `??` only if the
    original did, otherwise the leading `?` survives).

    This is intentionally narrow: we only need the Class::method body, not the
    full type signature. Types after `@@` are dropped.
    """
    if not name or not name.startswith("?"):
        return name  # not mangled

    # Strip the type/calling-convention suffix at the first standalone @@.
    # The qualified name lives between the leading `?` and `@@`.
    s = name
    # Handle "??_C" string-literal symbol stubs -- skip (return as-is, useful
    # as a label for the consumer).
    if s.startswith("??_"):
        # Compiler-generated thunks. Common ones:
        #   ??_EClass@@... -> Class::`vector deleting destructor'
        #   ??_GClass@@... -> Class::`scalar deleting destructor'
        #   ??_RnClass@@   -> Class::`RTTI ...' (multiple flavors; rough)
        #   ??_C@_05...    -> string-literal symbol (leave as-is)
        if s.startswith("??_C"):
            return name
        tag = s[3:4]  # char after `??_`
        tag_name = {
            "E": "{vector deleting destructor}",
            "G": "{scalar deleting destructor}",
            "R": "{RTTI}",
        }.get(tag)
        if tag_name:
            rest = s[4:]
            # ??_R has a single-char subcode after _R; skip it
            if tag == "R" and rest and rest[0] in "0123456789":
                rest = rest[1:]
            idx = rest.find("@@")
            if idx > 0:
                qual = rest[:idx]
                parts = [p for p in qual.split("@") if p]
                parts.reverse()
                return "::".join(parts + [tag_name])
        return name

    if s.startswith("??"):
        # Special ctor/dtor/operator form: ??0Class@@... or ??1Class@@...
        # Token after ?? is one char (the op code), rest is the qualified name.
        # The literal mangling is `??<op>Class@Outer@@type-info`.
        op_marker = s[1:3]  # e.g. '?0'
        op_name = _CTOR_DTOR.get(op_marker, f"operator?{op_marker[1]}?")
        rest = s[3:]
        idx = rest.find("@@")
        if idx < 0:
            return name
        qual = rest[:idx]
        parts = [p for p in qual.split("@") if p]
        # parts are in reverse order: ['Class', 'Outer'] -> 'Outer::Class'
        parts.reverse()
        return "::".join(parts + [op_name])

    # Normal form: ?name@Class@Outer@@type-info
    rest = s[1:]
    idx = rest.find("@@")
    if idx < 0:
        return name
    qual = rest[:idx]
    parts = [p for p in qual.split("@") if p]
    if not parts:
        return name
    # first token is the method name; remainder is the qualifier (reversed)
    method = parts[0]
    quals = parts[1:]
    quals.reverse()
    if quals:
        return "::".join(quals + [method])
    return method


def demangle_safe(name: str) -> str:
    """Wrap demangle_msvc; never raise."""
    try:
        return demangle_msvc(name)
    except Exception:
        return name


# ---------------------------------------------------------------------------
# Smoke test
# ---------------------------------------------------------------------------

def _selftest() -> None:
    samples = [
        ("?getMasher@KeyChain@@YAXPAE@Z", "KeyChain::getMasher"),
        ("?DrawRegular@App@@IAAXXZ", "App::DrawRegular"),
        ("?MakeString@@YAPBDPBD@Z", "MakeString"),
        ("??0App@@QAA@XZ", "App::{ctor}"),
        ("??1App@@QAA@XZ", "App::{dtor}"),
        ("?SplitMs@Timer@@QAAMXZ", "Timer::SplitMs"),
    ]
    fails = 0
    for inp, want in samples:
        got = demangle_msvc(inp)
        ok = got == want
        if not ok:
            fails += 1
        print(f"  {inp!r:55s} -> {got!r}  {'OK' if ok else 'FAIL want=' + repr(want)}")
    print(f"demangle: {len(samples) - fails}/{len(samples)} ok", file=sys.stderr)


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "selftest":
        _selftest()
    elif len(sys.argv) > 1 and sys.argv[1] == "parse":
        m = parse_map(sys.argv[2] if len(sys.argv) > 2 else DEFAULT_MAP)
        # print a few entries
        for i, (k, v) in enumerate(m.items()):
            if i >= 5:
                break
            print(k, v)
    else:
        print("usage: dc3_map.py {selftest|parse [mapfile]}", file=sys.stderr)
