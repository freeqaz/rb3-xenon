# BinDiff Integration Plan

Ingest `tools/bindiff_match.json` (11,057 dc3 -> RB3 cross-binary function matches
harvested via Ghidra + BinDiff) into the matching pipeline that currently runs on
string-content autoid alone.

## Inputs verified

* `tools/bindiff_match.json` -- list of 11,057 entries. Schema confirmed:
  `rb3_addr`, `rb3_fn`, `dc3_addr`, `dc3_name` (MSVC mangled), `similarity`
  (>=0.80; 87% are 1.0), `confidence` (avg 0.98), `algorithm`, `size`, `source`.
  `dc3_name` values are uniformly mangled (no demangled mixed in). 11,052 of
  11,057 dc3 names are unique.
* `dc3-decomp/orig/373307D9/ham_xbox_r.map` -- leaked Microsoft linker map.
  Per-function lines look like
  `0005:000001a0  ?getMasher@KeyChain@@YAXPAE@Z  823301a0 f   keygen_xbox.obj`
  and inline-only forms
  `0005:000009b0  ?MakeString@@YAPBDPBD@Z         823309b0 f i App.obj`.
  The `.obj` token is the library + object name (e.g. `rndobj:Trans.obj`,
  `obj:Dir.obj`). Lib prefix is colon-separated, not a path.
* `dc3-decomp/config/373307D9/objects.json` -- module groups (`main`, `lazer`,
  `system`, ...) with relative `.cpp` paths. Mapping `.obj` -> `.cpp` is
  basename-driven (`App.obj` <-> `App.cpp`); the lib prefix narrows the lookup.

## Demangling

`dc3_name` only needs to be human-readable for the unified report; the *key* for
lookup is the raw mangled string (matches the map and dc3 `symbols.txt`
verbatim).

Choice: **pure-Python `cxxfilt`** package, falling back to a 30-line in-tree
shim that handles only the patterns we see (`?name@Class@@...` -> `Class::name`,
`??0/1/2/3/...` -> `ctor/dtor/op new/op delete/...`). Avoid `undname.exe` (Wine
hassle), `c++filt` (Itanium ABI, won't touch MSVC mangling), and
`msvc-demangler` (Rust binary -- another build dep).

`cxxfilt` is a thin ctypes wrapper around libiberty and *does* handle MSVC
mangling (`__unDName`-style) when the system has it; if it doesn't, we ship the
shim. Demangling failures are non-fatal -- print the mangled name with a `?`
prefix and continue.

## dc3_name -> source-file lookup

Two-stage:

1. **Parse `ham_xbox_r.map` once.** Build
   `mapinfo: dict[mangled_name] -> {addr, tags, obj}` where `tags` is a tuple
   like `('f',)` or `('f','i')`. Use the awk-style heuristic: any line whose
   final whitespace-delimited token matches `^[a-z0-9_]+(:[A-Za-z0-9_]+)?\.obj$`
   is a per-symbol line. The address token is the 8-hex value immediately
   before the tag block. **Cache as JSON** (`tools/.cache/dc3_map_index.json`)
   keyed by the .map file's mtime so subsequent runs skip the ~120k-line parse.

2. **Build `obj -> cpp` from dc3 `objects.json`.** Strip the `lib:` prefix; for
   the remaining `Foo.obj` look up `Foo.cpp` (case-preserving) in any module's
   `objects` map. Resolve to the relative path dc3 uses
   (`lazer/game/HamUser.cpp`, `system/obj/Dir.cpp`, ...). Prefix with
   `../dc3-decomp/src/` to produce a path comparable to autoid's `src` field.
   Cache misses (unmatched basenames) get logged and counted; a small number
   are expected for STL/SDK code without a matching dc3 cpp.

Final mapping: `bindiff entry -> dc3_cpp_path` via
`objects.json[lookup(mapinfo[dc3_name].obj)]`.

## Inline-tag risk verification (measured)

I sampled all 11,057 entries against the parsed map:

* **Emitted (`f`)**:        6,999  (63%)
* **Inline-only (`f i`)**:  4,057  (37%)
* **Missing from map**:         1

37% inline-only is meaningful but not fatal. Interpretation: in dc3 these were
inlined into their callers, so they have no distinct `.text` section. BinDiff
still matched them against an RB3 function because in RB3 they survived as a
distinct function (or BinDiff matched an RB3 caller body to a dc3 caller that
happens to contain the inlined dc3 callee). For a pure-attribution use
(`rb3_fn -> dc3_name`) that's still useful -- a name is a name. For
**clustering by source file**, inline-only entries are still attributable to the
same `.cpp` (the inline definition lives somewhere), so they contribute to
cluster density. The plan: **do not drop f-i entries**; tag them.

## New subcommand: `merge_bindiff`

`tools/fingerprint_match.py merge_bindiff`. Output: `unified_id.json`.

Inputs:
* `--autoid autoid.json` (string-based proposals, current 537-after-Symbols-filter)
* `--bindiff tools/bindiff_match.json`
* `--dc3-map ../dc3-decomp/orig/373307D9/ham_xbox_r.map`
* `--dc3-objects ../dc3-decomp/config/373307D9/objects.json`

For each RB3 `fn_XXXXXXXX` we emit one record:

```jsonc
{
  "rb3_fn": "fn_82260018",
  "rb3_addr": "82260018",
  "size": 104,
  "source": "both" | "autoid" | "bindiff",
  // autoid leg (if present):
  "autoid_src": "../dc3-decomp/src/system/rndobj/MetaMaterial.cpp",
  "autoid_score": 22,
  // bindiff leg (if present):
  "dc3_name": "?DrawRegular@App@@IAAXXZ",
  "dc3_name_demangled": "App::DrawRegular",
  "dc3_obj": "App.obj",
  "dc3_inline_only": false,
  "bindiff_src": "../dc3-decomp/src/main/App.cpp",
  "similarity": 1.0,
  "confidence": 0.982,
  // agreement flag (only when source == "both"):
  "agreed": true
}
```

Agreement: same `.cpp` path (case-insensitive, normalized). Disagreements are
emitted but flagged `"agreed": false` -- never silently picks a winner.

## New subcommand: `bindiff_clusters`

For each dc3 `.cpp`, gather every RB3 fn attributed to it (bindiff leg, with
inline-only entries included so density survives), compute the convex hull
`[min(rb3_addr), max(rb3_addr + size))`, and reject clusters that don't pass
the autoid criteria already in use for the existing 8 pinned ranges:

* density `>= 3%` of total functions in [min,max) covered by bindiff entries
* span `<= 256 KiB`
* `>= 3` distinct bindiff hits in the cluster (anti-singleton)

Output: `proposed_splits_bindiff.txt` with verbatim splits.txt entries
(`Foo.cpp:` header, `.text start:0xAAAA end:0xBBBB`, blank line).
**Skip clusters whose `.cpp` already appears in `config/45410914/splits.txt`** so
this only proposes *new* (unpinned) clusters.

**Spot-check pass**: for each `.cpp` already pinned in splits.txt, print
`bindiff_in_range / bindiff_total_for_this_cpp` and the min/max bindiff addr.
This is the cross-validation: if the existing pin spans `[0x82758380,
0x8275A534)` and 80% of bindiff's MasterAudio entries fall inside it, the two
methods corroborate; if 5% do, one of them is wrong.

Pre-flight reality check on MasterAudio (`[0x82758380, 0x8275A534)`): only
**2 bindiff entries** land in that range, and both (`DrawBounds`, `AddHeap`) are
generic utilities -- *not* MasterAudio members. Implication: bindiff coverage is
**sparse on already-pinned, string-rich clusters** (autoid already nailed
those). bindiff's value-add is the long tail of low-string-content code
(allocators, accessors, STL instantiations) that autoid can't see. The spot-
check report will quantify this overlap-vs-complement split.

Also: `fn_8275A2C0` (the expected `SetupTrackChannel`-shaped function) is
**not present in bindiff_match.json**, so no cross-check from that direction.
That itself is a useful datapoint -- BinDiff didn't find a near-identical dc3
counterpart for it. Document this in the report.

## Verification protocol

Run after merge_bindiff and bindiff_clusters land:

1. `wc -l autoid.json bindiff_match.json unified_id.json` -- expect
   `unified` >= `bindiff` (since every bindiff hit produces a record) plus an
   `autoid`-only delta where autoid mapped a fn bindiff didn't touch.
2. `jq '[.[] | select(.source=="both")] | length' unified_id.json` -- intersect
   count. Then `select(.source=="both" and .agreed==false) | length` -- the
   disagreement set is the human-review queue.
3. Sample 30 random `source=="both" and agreed==true` records, confirm the
   `dc3_name_demangled` reads sensibly against the resolved `.cpp` by spot-
   greping (e.g. demangle `?Foo@Bar@@QAAXXZ` -> `Bar::Foo`; grep `Bar::Foo` in
   the resolved .cpp).
4. `bindiff_clusters` against the 8 existing pinned clusters: emit a CSV
   `cpp,pinned_range,bindiff_in_range,bindiff_total_for_cpp,agreement_pct`.
5. Inspect `proposed_splits_bindiff.txt`: top 20 by density. Anything with
   density >= 10% over <= 16 KiB is a candidate for immediate pinning.

## Risks

* **f-i ratio (37%) skews density** if we count inline-only matches the same as
  emitted matches. Mitigation: report two densities per cluster
  (`density_total`, `density_emitted_only`); a cluster that's 30% f-i may still
  be real but flag it.
* **`.obj -> .cpp` gaps.** The map's `.obj` carries a library prefix
  (`rndobj:Trans.obj`, `xaudio2:filterskin.obj`). dc3 `objects.json` keys are
  paths like `system/rndobj/Trans.cpp` -- the lib prefix is *informative* (it
  hints `system/rndobj/`) but not authoritative. Strategy: try basename match
  first; on ambiguity (multiple matches), use the library prefix as a directory
  hint (`rndobj` -> `system/rndobj/`). Log unresolved obj names; some are
  SDK/STL bits dc3 doesn't ship as source (xgraphics, d3dx9, BINK, RAD), and
  those legitimately have no dc3 .cpp.
* **Autoid <-> bindiff re-attribution wars.** Different signals
  (RB3-side string content vs dc3-side flowgraph), different file. **Never
  pick a winner silently** -- emit both, flag, surface the disagreement queue.
* **Demangling.** A bad demangle should never block a record; raw mangled name
  is always preserved.
* **BinDiff false positives at similarity < 0.95.** 1,417 entries have
  `similarity < 1.0`. They'll be weighted lower in cluster density (count them
  as `0.5` instead of `1.0`?), or just reported with the `similarity` field
  intact and let the human reviewer judge per-cluster.

## References

* `tools/bindiff_match.json` -- input.
* `tools/fingerprint_match.py` -- where new subcommands live.
* `autoid.json` -- existing string-based proposals.
* `../dc3-decomp/orig/373307D9/ham_xbox_r.map` -- name->obj source of truth.
* `../dc3-decomp/config/373307D9/objects.json` -- obj->cpp source of truth.
* `~/.claude/projects/-home-free-code-milohax-rb3-xenon/memory/project_function_identification.md`
  -- existing autoid method context.
* `~/.claude/projects/-home-free-code-milohax-rb3-xenon/memory/project_rb3_xenon_roadmap.md`
  -- phase tracking; add a "Phase: bindiff integration" entry on completion.

## Implementation steps (for Opus impl agent)

1. **Add `cxxfilt` dependency or in-tree shim.** Try `import cxxfilt`; on
   `ImportError` register a fallback `demangle_msvc(s)` that handles the
   patterns we see in the 20-sample (`?name@Class@@`, `??0/1`, simple
   templates). Confirm both paths return readable text for the 8 sample names
   in this plan.

2. **Add `tools/dc3_map.py` (new module).** Functions: `parse_map(path)`,
   `load_objects(path)`, `obj_to_cpp(obj, objects_db)`. Include the mtime-keyed
   JSON cache for the map parse. Unit-test against the known
   `?getMasher@KeyChain@@YAXPAE@Z` -> `keygen_xbox.obj` -> `keygen_xbox.cpp`
   round-trip.

3. **Wire `merge_bindiff` subcommand** in `fingerprint_match.py`. Take
   `--autoid`, `--bindiff`, `--dc3-map`, `--dc3-objects`, `--out`. Emit the
   record schema above. Stderr summary: total records, source counts
   (`autoid` / `bindiff` / `both`), agreement percentage, demangle failures,
   obj-to-cpp misses.

4. **Wire `bindiff_clusters` subcommand.** Inputs: `unified_id.json`,
   `--rb3-symbols config/45410914/symbols.txt`, `--existing-splits
   config/45410914/splits.txt`, `--out proposed_splits_bindiff.txt`. Apply the
   density/span/min-hits filters. Emit `proposed_splits_bindiff.txt` for *new*
   clusters and a `cluster_spotcheck.csv` for pinned clusters.

5. **Run, verify, iterate.** Execute the verification protocol above; check
   the agreement queue against 30 random `agreed==true` records by hand; tune
   density threshold if needed (start at 3%, may need 5% for bindiff-only
   clusters since f-i entries inflate the count).

6. **Update memory.** Append a status block to
   `project_function_identification.md`: `Status YYYY-MM-DD: bindiff merged;
   N_unified records; N_clusters proposed; M auto-pinnable at density >= 10%.`

7. **Do not** modify `splits.txt`, `objects.json`, or `symbols.txt` from this
   work. The output is candidates; pinning is a separate manual step gated on
   human review of `proposed_splits_bindiff.txt` plus a verification ninja
   build per pin.
