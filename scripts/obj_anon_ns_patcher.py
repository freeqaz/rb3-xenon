#!/usr/bin/env python3
"""Post-build patcher for MSVC anonymous namespace hashes in .obj files.

What the hash is
----------------
MSVC spells an anonymous namespace `?A0x<8 hex>@@` inside a mangled name, and
that hash is a function of the BUILD MACHINE'S COMPUTER NAME and the CANONICAL
PATH of the file the namespace is declared in.  It is not a function of the
file's contents.  So NO SOURCE EDIT CAN PRODUCE RETAIL'S VALUE: it encodes a
fact about Harmonix's build host, in the same category as the pinned
`WIBO_COMPUTER_NAME`.  This pass reproduces that build-environment input by
rewriting our object after the compiler has run, and the honest reading of the
numbers it moves is "our instruction stream and its relocation targets agree
with retail once the build host's identity is normalised away" -- not "our
source now compiles to this".

The compile-time route is not merely harder, it is BLOCKED for the hashes that
matter: under wibo, a `namespace {}` declared in a HEADER is never hashed at all
-- cl only ever hashes the `.cpp` path -- so no combination of path mapping and
computer name reaches a header hash.  The rewrite is the available route, not
just the cheap one.

One hash per FILE, not one per object
-------------------------------------
Because the hash keys on the DECLARING FILE's path, an anonymous namespace in a
header gets one hash shared by every TU that includes it, while a TU-local one
gets the `.cpp`'s.  A retail object therefore routinely carries SEVERAL hashes
at once, while ours -- which declares those entities in the `.cpp` -- carries
one.  One of our hashes has to become several of retail's, chosen PER SYMBOL.

This is what the previous version of this script could not do.  It decided one
hash per object: it patched only when both sides had exactly one hash, and
otherwise tried to guess which of retail's hashes was "the file's own" by
dropping any hash seen in more than five retail objects.  That heuristic is a
frequency argument, not evidence, and on rb3-xenon it left the whole multi-hash
population -- `Sfx`, `CheatProvider`, `Joypad_Xbox`, `ChunkStream`,
`WaitingUserGate`, `UIListDir`, `WaveFile` -- unpatched.  Removed, not fixed.

How the assignment is decided
-----------------------------
By NAME.  Every hash occurrence sits inside a NUL-delimited mangled name.
Blank the hashes out of that name and you get a template; retail's object is
then asked what hashes belong in those positions, positionally.  Token-level
matching (the identifier immediately before `?A0x`) is genuinely ambiguous and
is therefore only a fallback.

Resolution order per occurrence:

  1. exact template in the paired retail object          (`template`)
  2. exact template anywhere in the retail tree          (`template_global`)
  3. same, after blanking the lexical-scope ordinal      (`template_ordinal`)
  4. same, after stripping a `<prefix>$` decoration      (`template_stripped`)
  5. the token before `?A0x`, from the paired object     (`token`)
  6. the token, from the retail tree                     (`token_global`)
  7. this object's majority target                       (`majority`)

Only 1-4 are evidence retail states outright.  5-7 exist for symbols we emit
that retail never did (STL instantiations it inlined, EH tables it did not
need); those cannot match a retail name whatever we write, so the fallback is
about keeping the object internally consistent, not about buying a match.  A
fallback provably cannot manufacture a match: if a fallback-assigned name
coincided with a retail name, the template lookup would have found that name
first.  Runs resolved by 7 are counted and reported.

MEASURED ON THIS TREE, not inherited from dc3.  dc3's headline check --
543 distinct name templates, ZERO mapping to two different hash tuples --
DOES NOT HOLD on rb3-xenon, and that is a finding, not a detail.  Here it is
161 templates with 4 ambiguous, and 119 tokens with 15 ambiguous:

  ?Dispatch@SyncLocalMachineMsg@?A0x*@@UAAXXZ                  6c4eb79b / 951deeb9
  ??$_Copy_Construct@UDebugGraph@?A0x*@@…                      b39b74bf / fa5cc2c6
  ??$__destroy_range_aux@…reverse_iterator@PAULabel@?A0x*@@…   81ddebd1 / 9335ac2a
  ?NewNetMessage@MainHubAdvanceMsg@?A0x*@@SAPAVNetMessage@@XZ  447fe1d1 / fb94c5e0

Two of the four are ambiguous only ACROSS objects and the paired-object rule
settles them.  The other two are ambiguous INSIDE the paired retail object --
retail's own `Sfx.obj` carries `_Copy_Construct<DebugGraph>` under both
`b39b74bf` and `fa5cc2c6`, and `BandMachineMgr.obj` carries
`SyncLocalMachineMsg::Dispatch` under both `6c4eb79b` and `951deeb9`.  No
name-based rule can settle those; `_lookup` returns None on an ambiguous key,
so they fall through to the fallbacks rather than resolving to whichever entry
was seen last.

dc3's second claim -- that a fallback provably cannot manufacture a match,
because a template lookup would have found any name-identical spelling first --
is a construction proof that DEPENDS on zero ambiguity, so it does not hold
here either.  Re-derived on this tree: 151 of our anonymous-namespace names
become byte-identical to a name in the paired retail object, 150 of them from
`template` and exactly 1 from `majority`.  That one is the `Sfx.obj`
`_Copy_Construct<DebugGraph>` above: the template is locally ambiguous, so the
evidence rules abstain, and `majority` (142 of 146 occurrences are `b39b74bf`)
lands on one of the two spellings retail actually uses.  It is a coin flip that
came up heads, not evidence, and it should be read that way.

Every rewrite is 8 hex characters over 8 hex characters, so nothing in the
object moves: no string-table offset, no section size, no relocation.  This
also means the pass cannot reach retail's OTHER anonymous-namespace spelling,
the bare `?A@@` with no hash at all -- `?A0x<h>@@` is 12 bytes and `?A@@` is 4.
Those are left alone and reported.

Patches are LOST on rebuild - this is a post-build step, re-run from
`configure.py`'s `post-compile` chain.  The pass is idempotent by construction:
once our names equal retail's, rule 1 maps them to themselves.

Usage:
    python3 scripts/obj_anon_ns_patcher.py --batch [--apply] [--verbose]
    python3 scripts/obj_anon_ns_patcher.py [--apply] <rel/path.obj> ...

Without --apply, performs a dry run showing what would be changed.

Per-file mode, and why it is still the same answer
--------------------------------------------------
The per-file form takes paths relative to --src-dir, matching
obj_guard_patcher/obj_bool_mangle_patcher.  Unlike those two this pass DOES
have cross-object state -- `global_index`, the union of every hash tuple in
every RETAIL object some compiled object is paired with -- so restricting the
work is not the same thing as restricting the inputs.  Per-file mode therefore
builds the index EXACTLY as batch does (whole `--obj-dir`, whole `--src-dir`,
whole objdiff.json) and only narrows the final rewrite loop.

That is sound for the single-TU rescore this exists for, because every input to
the index is either a retail target object or a compiled object's PATH, and a
recompile of one TU changes neither: `live_targets` reads relpaths, never
contents.  Two compiled objects never inform each other here.  Verified by
control rather than argued: a full-graph object and a single-TU compile +
per-file chain are byte-identical (<decomp-bench>/archive/runs/
2026-08-21-permuter-name-check-path/).
"""

import argparse
import json
import os
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
OBJ_DIR = PROJECT_ROOT / "build" / "45410914" / "obj"
SRC_DIR = PROJECT_ROOT / "build" / "45410914" / "src"
OBJDIFF_CONFIG = PROJECT_ROOT / "objdiff.json"

ANON_NS_PATTERN = re.compile(rb'\?A0x([0-9a-fA-F]{8})@@')
#: The hashless spelling.  `(?!0x)` keeps it from eating the hashed one.
HASHLESS_PATTERN = re.compile(rb'\?A(?!0x)@@')
#: Placeholder a template puts where a hash was.  Same length is not required;
#: templates are only ever compared to other templates.
TEMPLATE_MARK = b'?A0x*@@'
#: MSVC decorations wrapped around a function's name to make a companion
#: symbol (`__ehfuncinfo$?Foo@...`).  Retail often has the function but not the
#: companion, so the companion's hash has to be read off the function's.
DECORATION = re.compile(rb'^__[A-Za-z_]+\$')
#: The lexical-scope ordinal MSVC stamps into a function-local static's name
#: (`?month_symbols@?1??MonthToken@...`).  It is a SEPARATE residual lane
#: (`local_static_scope_ordinal`) and disagreeing on it must not stop us
#: reading the anonymous-namespace hash off the same name.
SCOPE_ORDINAL = re.compile(rb'\?[0-9A-Za-z]\?\?')
#: The identifier at the tail of a mangled scope component.  Stops at `@`, at
#: the `?<ord>??` marker, and at the leading `??__E`-style decorations.
TRAILING_IDENT = re.compile(rb'[A-Za-z0-9_<>\-]+$')


def hashless_names(path):
    """Retail's OTHER anonymous-namespace spelling, `?A@@` with no hash.

    Returns `{template: name}` for every NUL-delimited run in the object that
    names the anonymous namespace hashlessly, keyed by the same template the
    hashed path uses so the two can be compared.

    This pass cannot produce it: every rewrite it makes is 8 hex characters
    over 8 hex characters so that nothing in the object moves, and `?A0x<h>@@`
    is 12 bytes against `?A@@`'s 4.  Reported rather than silently dropped.
    """
    with open(path, 'rb') as fh:
        data = fh.read()
    out = {}
    for m in HASHLESS_PATTERN.finditer(data):
        start = data.rfind(b'\0', 0, m.start()) + 1
        end = data.find(b'\0', m.end())
        run = data[start:end if end >= 0 else len(data)]
        out[HASHLESS_PATTERN.sub(TEMPLATE_MARK, ANON_NS_PATTERN.sub(
            TEMPLATE_MARK, run))] = run
    return out


def find_anon_ns_hashes(data: bytes) -> set:
    """Find all unique anonymous namespace hashes in a COFF .obj file.

    Returns set of 8-byte ASCII hash strings (e.g., {b'c9fefd64'}).
    """
    return set(ANON_NS_PATTERN.findall(data))


def hash_runs(data: bytes):
    """Yield `(start, end)` of every NUL-delimited run holding an anon-ns hash.

    A COFF holds these names in the string table, in `.debug$S`/`.debug$T`, and
    in the RTTI type-descriptor data -- all NUL-terminated in practice.  We do
    not care which: the run around a hash is the name that names it, wherever
    it is stored, and the mapping below is by name.
    """
    spans = set()
    for m in ANON_NS_PATTERN.finditer(data):
        start = data.rfind(b'\0', 0, m.start()) + 1
        end = data.find(b'\0', m.end())
        spans.add((start, end if end >= 0 else len(data)))
    return sorted(spans)


def template_of(run: bytes) -> bytes:
    return ANON_NS_PATTERN.sub(TEMPLATE_MARK, run)


def hashes_of(run: bytes):
    """`[(offset_within_run_of_the_8_hex_chars, hash_bytes), ...]`."""
    return [(m.start(1), m.group(1)) for m in ANON_NS_PATTERN.finditer(run)]


def token_of(run: bytes, hash_start: int) -> bytes:
    """The identifier immediately before the `?A0x` at `hash_start`.

    In a mangled name the scope chain runs right to left separated by `@`, so
    the component just before the anonymous-namespace marker is the entity
    declared in it: `?NewNetMessage@MainHubAdvanceMsg@?A0x...@@...` ->
    `MainHubAdvanceMsg`.  The `?1??` scope ordinal is deliberately not part of
    the token: it is a different residual lane and we must not let a
    disagreement there hide the entity.
    """
    head = run[:hash_start - 4].rstrip(b'@')   # -4 skips the literal '?A0x'
    m = TRAILING_IDENT.search(head)
    return m.group(0) if m else head


def index_object(path):
    """`(template_map, token_map, hash_weights)` for one object.

    The first two carry SETS so an ambiguous key is visible as such rather than
    silently resolving to whichever entry was seen last.  `hash_weights` counts
    occurrences per hash, which is how "this object's dominant namespace" is
    decided when nothing else resolves.
    """
    with open(path, 'rb') as fh:
        data = fh.read()
    templates = defaultdict(set)
    tokens = defaultdict(set)
    weights = Counter()
    for start, end in hash_runs(data):
        run = data[start:end]
        tmpl = template_of(run)
        hashes = tuple(h for _, h in hashes_of(run))
        templates[tmpl].add(hashes)
        templates[SCOPE_ORDINAL.sub(b'?#??', tmpl)].add(hashes)
        for off, h in hashes_of(run):
            tokens[token_of(run, off)].add(h)
            weights[h] += 1
    return templates, tokens, weights


def build_obj_mappings(obj_dir: Path, src_dir: Path):
    """Build mappings between original and decomp .obj files.

    Returns:
        orig_by_relpath: {relative_path: absolute_path} for original .obj files
        decomp_by_relpath: {relative_path: absolute_path} for decomp .obj files
    """
    orig_by_relpath = {}
    for root, dirs, files in os.walk(obj_dir):
        for f in files:
            if not f.endswith('.obj') or f.startswith('auto_'):
                continue
            abspath = os.path.join(root, f)
            relpath = os.path.relpath(abspath, obj_dir)
            orig_by_relpath[relpath] = abspath

    decomp_by_relpath = {}
    for root, dirs, files in os.walk(src_dir):
        for f in files:
            if not f.endswith('.obj'):
                continue
            abspath = os.path.join(root, f)
            relpath = os.path.relpath(abspath, src_dir)
            decomp_by_relpath[relpath] = abspath

    return orig_by_relpath, decomp_by_relpath


def load_config_pairs(config_path: Path, obj_dir: Path, src_dir: Path) -> dict:
    """Map compiled-obj relpath -> target-obj relpath using objdiff.json.

    WHY THIS EXISTS (the bug this fixes):
    ------------------------------------
    The target .obj files dtk emits are named after the *splits.txt heading*,
    while our compiled .obj files mirror the *source tree*. The overwhelming
    majority of splits headings are BARE basenames ("MasterAudio.cpp:"), so the
    target lands at   build/45410914/obj/MasterAudio.obj
    while the base is build/45410914/src/system/beatmatch/MasterAudio.obj.

    Keying both sides on their directory-relative path therefore only ever
    matched the minority of units whose splits heading carries a path, and
    silently dropped every bare-declared unit whose source lives in a
    subdirectory ("SKIP <unit>: no matching original .obj").

    ★ THIS IS WHY dc3's VERSION OF THIS SCRIPT CANNOT BE COPIED VERBATIM.  dc3
    pairs on relpath alone because dc3's splits headings carry paths (2,205 of
    2,223 target objects are nested).  Measured on this tree, of the 126 of our
    objects that carry an anon-ns hash, relpath keying reaches 17 and
    objdiff.json reaches 98 more.  Running dc3's script unmodified here skips
    109 objects -- i.e. the whole lane.

    objdiff.json is the AUTHORITATIVE answer: configure.py writes one unit per
    pinned TU carrying the explicit target_path/base_path pair, and it is the
    very same pairing objdiff uses to score. Deriving the mapping from it means
    the patcher can never disagree with the scorer.

    Deliberately NOT a basename heuristic: build/45410914/obj/ accumulates
    orphan target .obj files from splits headings that were since removed
    (measured: 118 of 1069 non-auto targets, some months stale and only ~1 KB).
    A "unique basename" fallback pairs live compiled objs against those dead
    stubs -- i.e. it invents pairings rather than recovering real ones.

    Environment-independent: objdiff.json's paths are repo-root-relative and
    the file is regenerated by configure.py in every worktree, so this resolves
    identically in main and in every worktree regardless of absolute location.
    Pairs whose two sides do not land under obj_dir/src_dir (e.g. when the
    caller overrides --obj-dir/--src-dir) are simply not offered.

    Returns {} if the config is absent or unreadable -- the caller then falls
    back to plain relpath keying, i.e. the historical behaviour.
    """
    try:
        with open(config_path) as fh:
            cfg = json.load(fh)
    except (OSError, ValueError):
        return {}

    root = Path(config_path).resolve().parent
    obj_dir = Path(obj_dir).resolve()
    src_dir = Path(src_dir).resolve()

    pairs = {}
    for unit in cfg.get("units", []):
        target_path = unit.get("target_path")
        base_path = unit.get("base_path")
        if not target_path or not base_path:
            continue
        target_abs = (root / target_path).resolve()
        base_abs = (root / base_path).resolve()
        try:
            target_rel = target_abs.relative_to(obj_dir)
            base_rel = base_abs.relative_to(src_dir)
        except ValueError:
            # One of the two sides lives outside the directories we were asked
            # to operate on -- not our business.
            continue
        pairs[str(base_rel)] = str(target_rel)

    return pairs


def resolve_orig(relpath, orig_by_relpath, config_pairs):
    """The target .obj relpath for one compiled .obj relpath, or None.

    Identical relpaths first (free, and unambiguous when the splits heading
    carries a path), then the authoritative objdiff.json pairing, which is what
    actually covers the bare-declared units.
    """
    if relpath in orig_by_relpath:
        return relpath, False
    mapped = config_pairs.get(relpath)
    if mapped in orig_by_relpath:
        return mapped, True
    return None, False


def _lookup(mapping, key):
    """The single value `key` maps to, or None if absent or ambiguous."""
    got = mapping.get(key)
    if got is None or len(got) != 1:
        return None
    return next(iter(got))


def _template_target(run, local_templates, global_templates):
    """Retail's hash tuple for this exact name, or None.  Returns (tuple, rule)."""
    tmpl = template_of(run)
    for mapping, rule in ((local_templates, 'template'),
                          (global_templates, 'template_global')):
        got = _lookup(mapping, tmpl)
        if got is not None:
            return got, rule
    # Same name, different lexical-scope ordinal.  Both index maps carry the
    # ordinal-blanked spelling as an extra key, so this is still a name match.
    flat = SCOPE_ORDINAL.sub(b'?#??', tmpl)
    if flat != tmpl:
        for mapping in (local_templates, global_templates):
            got = _lookup(mapping, flat)
            if got is not None:
                return got, 'template_ordinal'
    # A companion symbol we emit around a function retail also has:
    # `__ehfuncinfo$?Foo@?A0x...@@...` -> ask about `?Foo@?A0x...@@...`.
    m = DECORATION.match(tmpl)
    if m and m.end() < len(tmpl):
        for inner in (tmpl[m.end():], flat[m.end():]):
            for mapping in (local_templates, global_templates):
                got = _lookup(mapping, inner)
                if got is not None:
                    return got, 'template_stripped'
    return None, None


def plan_object(data, orig_index, global_index):
    """Decide a replacement hash for every anon-ns occurrence in `data`.

    Returns `(edits, stats, unresolved)` where `edits` is
    `{absolute_offset_of_8_hex_chars: new_hash_bytes}`.
    """
    o_templates, o_tokens, o_weight = orig_index
    g_templates, g_tokens = global_index
    edits = {}
    stats = Counter()
    deferred = []          # (absolute_offset, run, offset_in_run)

    for start, end in hash_runs(data):
        run = data[start:end]
        here = hashes_of(run)
        target, rule = _template_target(run, o_templates, g_templates)
        if target is not None and len(target) == len(here):
            for (off, _), new in zip(here, target):
                edits[start + off] = new
            stats[rule] += len(here)
            continue
        for off, _ in here:
            tok = token_of(run, off)
            got = _lookup(o_tokens, tok)
            if got is not None:
                edits[start + off] = got
                stats['token'] += 1
                continue
            got = _lookup(g_tokens, tok)
            if got is not None:
                edits[start + off] = got
                stats['token_global'] += 1
                continue
            deferred.append((start + off, run, off))

    unresolved = []
    if deferred:
        # Majority over what the evidence-backed rules decided for THIS object,
        # falling back to retail's own dominant hash in this object when
        # nothing at all resolved (we emit an anonymous-namespace entity retail
        # does not have, but retail's object does have the namespace).
        # Deterministic across passes: both sources are retail, never our
        # current spelling.
        source = Counter(edits.values()) or o_weight
        if source:
            majority = source.most_common(1)[0][0]
            for offset, _run, _off in deferred:
                edits[offset] = majority
            stats['majority'] += len(deferred)
        else:
            unresolved = [run for _o, run, _f in deferred]
    return edits, stats, unresolved


def apply_edits(data: bytes, edits: dict) -> bytes:
    """Write each 8-hex hash in place.  Length-preserving by construction."""
    out = bytearray(data)
    for offset, new in edits.items():
        assert len(new) == 8, new
        assert out[offset - 4:offset] == b'?A0x', offset
        assert out[offset + 8:offset + 10] == b'@@', offset
        out[offset:offset + 8] = new
    return bytes(out)


def patch_obj_file(obj_path: str, old_hash: bytes, new_hash: bytes,
                   apply: bool = False) -> int:
    """Replace all occurrences of one hash with another (whole-file).

    Retained for callers that want the old blunt behaviour; the batch pass
    below does NOT use it, because a single hash of ours routinely has to
    become several of retail's.
    """
    old_pattern = b'?A0x' + old_hash + b'@@'
    new_pattern = b'?A0x' + new_hash + b'@@'

    with open(obj_path, 'rb') as f:
        data = f.read()

    count = data.count(old_pattern)
    if count == 0:
        return 0

    if apply:
        new_data = data.replace(old_pattern, new_pattern)
        _write_preserving_mtime(obj_path, new_data)

    return count


def process_batch(args):
    """Process all decomp .obj files in batch mode."""
    obj_dir = Path(args.obj_dir) if args.obj_dir else OBJ_DIR
    src_dir = Path(args.src_dir) if args.src_dir else SRC_DIR

    if not obj_dir.exists():
        print(f"ERROR: Original .obj directory not found: {obj_dir}", file=sys.stderr)
        sys.exit(1)
    if not src_dir.exists():
        print(f"ERROR: Decomp .obj directory not found: {src_dir}", file=sys.stderr)
        sys.exit(1)

    # Build file mappings by relative path (handles duplicate filenames correctly)
    orig_by_relpath, decomp_by_relpath = build_obj_mappings(obj_dir, src_dir)

    # Authoritative target<->base pairing (see load_config_pairs). Plain relpath
    # keying alone misses every bare-declared splits unit whose source lives in
    # a subdirectory, which is the large majority of pinned units.
    config_path = Path(args.objdiff_config) if args.objdiff_config else OBJDIFF_CONFIG
    config_pairs = load_config_pairs(config_path, obj_dir, src_dir)
    if not config_pairs:
        print(f"WARN: no usable target<->base pairs from {config_path}; falling "
              "back to relative-path keying only, which cannot match "
              "bare-declared splits units whose source is in a subdirectory.",
              file=sys.stderr)

    if args.verbose:
        print(f"Found {len(orig_by_relpath)} original .obj files")
        print(f"Found {len(decomp_by_relpath)} decomp .obj files")
        print(f"Loaded {len(config_pairs)} target<->base pairs from {config_path}")
        print("Indexing original .obj files by mangled-name template...")

    # Index only the targets some compiled object is actually PAIRED with.
    # dc3 unions every retail object into the global index; here that would
    # fold in build/45410914/obj/'s orphan stubs from removed splits headings
    # (118 of 1069 non-auto targets, measured). Extra hash tuples make a global
    # key ambiguous, and `_lookup` then returns None -- so the orphans cannot
    # produce a WRONG answer, but they silently demote a `template_global` to a
    # `majority`. Restricting the union to live pairings keeps every hash a
    # header genuinely shares across objects and drops only the dead ones.
    live_targets = set()
    for relpath in decomp_by_relpath:
        got, _ = resolve_orig(relpath, orig_by_relpath, config_pairs)
        if got is not None:
            live_targets.add(got)

    orig_index = {}
    g_templates = defaultdict(set)
    g_tokens = defaultdict(set)
    for relpath in sorted(live_targets):
        templates, tokens, weights = index_object(orig_by_relpath[relpath])
        if not templates:
            continue
        orig_index[relpath] = (templates, tokens, weights)
        for k, v in templates.items():
            g_templates[k] |= v
        for k, v in tokens.items():
            g_tokens[k] |= v
    global_index = (g_templates, g_tokens)

    if args.verbose:
        amb_t = sum(1 for v in g_templates.values() if len(v) > 1)
        amb_k = sum(1 for v in g_tokens.values() if len(v) > 1)
        print(f"Indexed {len(orig_index)} paired retail objects carrying a hash "
              f"(of {len(live_targets)} live targets, {len(orig_by_relpath)} on disk)")
        print(f"Retail name templates: {len(g_templates)} ({amb_t} ambiguous); "
              f"tokens: {len(g_tokens)} ({amb_k} ambiguous)")

    patched_files = 0
    total_replacements = 0
    already_ok = 0
    skipped_no_orig = 0
    skipped_no_hash_orig = 0
    skipped_unresolved = 0
    resolved_via_config = 0
    rules = Counter()
    hashless = []            # (relpath, ours, retail's) -- see hashless_names()

    # The index above is always built over the WHOLE tree (see the module note);
    # only the rewrite loop narrows. `args.files` is relative to --src-dir.
    if args.batch:
        work = sorted(decomp_by_relpath.items())
    else:
        work = []
        for f in args.files:
            rel = os.path.normpath(f)
            if rel not in decomp_by_relpath:
                print(f"ERROR: {f} is not an object under {src_dir}", file=sys.stderr)
                sys.exit(1)
            work.append((rel, decomp_by_relpath[rel]))

    for relpath, decomp_path in work:
        with open(decomp_path, 'rb') as fh:
            data = fh.read()
        if not ANON_NS_PATTERN.search(data):
            continue

        orig_relpath, via_config = resolve_orig(relpath, orig_by_relpath,
                                                config_pairs)
        if orig_relpath is None:
            if args.verbose:
                mapped = config_pairs.get(relpath)
                why = (f"paired target {mapped} not built"
                       if mapped else "no objdiff.json unit pairs it")
                print(f"  SKIP {relpath}: no matching original .obj ({why})")
            skipped_no_orig += 1
            continue
        if via_config:
            resolved_via_config += 1

        # Retail's hashless `?A@@` spelling, wherever it names a symbol we
        # spell with a hash.  Counted whether or not the object also has hashed
        # names to patch, because an object can use BOTH spellings.
        for tmpl, retail_name in hashless_names(orig_by_relpath[orig_relpath]).items():
            for start, end in hash_runs(data):
                if template_of(data[start:end]) == tmpl:
                    hashless.append((relpath, data[start:end], retail_name))

        if orig_relpath not in orig_index:
            # Retail's object has no anonymous namespace where ours does.
            # That is a SOURCE difference -- we wrapped in an anonymous
            # namespace where retail used file-scope `static`, or retail
            # spelled it `?A@@` -- not a naming one, and no length-preserving
            # rewrite can fix it.
            if args.verbose:
                print(f"  SKIP {relpath}: original ({orig_relpath}) has no "
                      "anonymous namespace hashes")
            skipped_no_hash_orig += 1
            continue

        edits, stats, unresolved = plan_object(data, orig_index[orig_relpath],
                                               global_index)
        if unresolved:
            skipped_unresolved += 1
            if args.verbose:
                print(f"  SKIP {relpath}: {len(unresolved)} occurrence(s) with "
                      f"no retail evidence and no in-object majority")
                for run in unresolved[:3]:
                    print(f"        {run.decode('latin1')[:110]}")
            continue

        changed = {o: h for o, h in edits.items() if data[o:o + 8] != h}
        rules.update(stats)
        if not changed:
            already_ok += 1
            if args.verbose:
                summary = ', '.join(f"{k}={v}" for k, v in sorted(stats.items()))
                print(f"  OK   {relpath}: {len(edits)} occurrence(s) already "
                      f"correct ({summary})")
            continue

        patched_files += 1
        total_replacements += len(changed)
        if args.apply:
            _write_preserving_mtime(decomp_path, apply_edits(data, edits))
        if args.verbose:
            moves = Counter((data[o:o + 8], h) for o, h in changed.items())
            summary = ', '.join(f"{k}={v}" for k, v in sorted(stats.items()))
            action = "PATCH" if args.apply else "WOULD PATCH"
            print(f"  {action} {relpath}: {len(changed)} occurrence(s) [{summary}]")
            for (old, new), n in moves.most_common():
                print(f"        {old.decode()} -> {new.decode()}  ({n})")

    action_word = "Applied" if args.apply else "Would apply"
    print(f"\n{action_word} patches to {patched_files} files ({total_replacements} total replacements)")
    print(f"Already matching: {already_ok}")
    print(f"Skipped (no retail evidence): {skipped_unresolved}")
    print(f"Skipped (no matching original): {skipped_no_orig}")
    print(f"Skipped (original has no anon ns): {skipped_no_hash_orig}")
    print(f"Paired via objdiff.json (not relpath-identical): {resolved_via_config}")
    if hashless:
        units = sorted({r for r, _o, _t in hashless})
        print(f"Out of reach (retail spells it `?A@@`, hashless): "
              f"{len(hashless)} name(s) in {len(units)} object(s) -- a length "
              f"change this pass does not make")
        for relpath, ours, theirs in hashless:
            print(f"    {relpath}: {ours.decode('latin1')[:80]}")
            print(f"    {' ' * len(relpath)}  retail: {theirs.decode('latin1')[:80]}")
    if rules:
        print("Assignment rules: "
              + ', '.join(f"{k}={v}" for k, v in sorted(rules.items())))

    if not args.apply and patched_files > 0:
        print(f"\nRun with --apply to actually patch the files.")

    if getattr(args, 'check', False) and patched_files > 0:
        print('FAIL[anon_ns]: {n} pending patch(es) -- this build tree carries '
              'objects that were compiled but never post-processed.'.format(
                  n=patched_files), file=sys.stderr)
        sys.exit(2)


def main():
    parser = argparse.ArgumentParser(
        description='Patch anonymous namespace hashes in decomp .obj files to match originals')
    parser.add_argument('--apply', action='store_true',
                        help='Actually apply patches (default: dry run)')
    parser.add_argument('--verbose', '-v', action='store_true',
                        help='Show per-file details')
    parser.add_argument('--batch', action='store_true',
                        help='Process all decomp .obj files')
    parser.add_argument('--obj-dir',
                        help='Original .obj directory (default: build/45410914/obj)')
    parser.add_argument('--src-dir',
                        help='Decomp .obj directory (default: build/45410914/src)')
    parser.add_argument('--objdiff-config',
                        help='objdiff config supplying the authoritative '
                             'target<->base obj pairing (default: objdiff.json '
                             'at the repo root)')
    parser.add_argument('--check', action='store_true',
                        help='Dry-run and EXIT 2 if any object in the build tree '
                             'still needs this pass')
    parser.add_argument('files', nargs='*',
                        help='Specific .obj files to patch (paths relative to --src-dir)')
    args = parser.parse_args()

    if not args.batch and not args.files:
        parser.error('Specify --batch or provide specific files')

    process_batch(args)


def _write_preserving_mtime(path, data):
    """Write `data` to `path`, then restore the file's original mtime.

    ★ LOAD-BEARING, NOT COSMETIC (lane CN-1, measured). ninja's `deps = msvc`
    stores each obj's mtime beside its dep record in .ninja_deps. If the obj on
    disk is later NEWER than that stored mtime, ninja reports
        ninja explain: stored deps info out of date for '<obj>'
    and RECOMPILES the obj -- silently discarding every symbol-table patch this
    script just applied. Because the patcher stamps take `all_source` as an
    implicit input, ninja then re-runs the patchers, which bump the mtime again:
    without this mtime restore the build OSCILLATES forever. See
    scripts/obj_eh_boundary_patcher.py, which has done this since lane CM-3
    (its comment misnames the file as .ninja_log -- the real one is .ninja_deps).
    """
    import os as _os
    p = str(path)
    st = _os.stat(p)
    with open(p, 'wb') as f:
        f.write(data)
    _os.utime(p, ns=(st.st_atime_ns, st.st_mtime_ns))


if __name__ == '__main__':
    main()
