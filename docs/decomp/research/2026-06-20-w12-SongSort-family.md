# W12 — SongSortNode / SongSort family (band3/meta_band) — DEFER (COMDAT-scatter, no pinnable span)

Date: 2026-06-20. Main @ d2d3e53, baseline 9301. Mode: DISCOVER/PLANNER, read-only main.

## Task
Port the coupled SongSortNode.cpp (sibling of SongSort.cpp / StoreSongSortNode.cpp;
both included by MusicLibraryNetSetlists.h), wire + pin a bounded `.text` span,
emit SongSort.cpp as a discovered_frontier. Expected ~+12.

## VERDICT: DEFER — SongSortNode.cpp has NO contiguous, cleanly-pinnable `.text` span.

Ground truth: the retail linker scattered SongSortNode.cpp's COMDATs and folded its
Node-base members into a region dominated by a *different but structurally-identical*
Node-family TU (NavListSortNode.cpp) plus several unrelated meta_band/UI TUs. This is
the COMDAT-template-scatter + shared-base-codegen wall (same class as the REFUTED
"Waypoint relocate", roadmap close). Pinning any span here would harvest foreign
fn_@0% (NavList*, OvershellSlotState, ChooseModeProvider) and fail the honesty gate.

## Evidence (all ground-truth, not heuristic)

### 1. Source state (oracle HAVE, xenon partial)
- rb3-Wii oracle: `../rb3/src/band3/meta_band/SongSortNode.cpp` (453 lines, ~75 fns)
  and `SongSort.cpp` (350 lines) both present and complete.
- xenon: `src/band3/meta_band/SongSortNode.h` + `SongSort.h` ALREADY exist (headers
  near-identical to Wii; SongSort.h adds one `using Hmx::Object::Handle;`). The `.cpp`
  files do NOT exist in xenon and are NOT in objects.json. StoreSongSortNode.cpp/.h DO
  exist in xenon and ARE wired+pinned.

### 2. String fingerprints located the handler region — but it is NOT SongSortNode-owned
Unique `.rdata` strings (Ghidra `search_strings`):
- `ui/image/song_select_header_keep.png` @ `0x820d3110`
- `ui/image/blank_album_art_keep.png` @ `0x820d3138`

Ghidra `list_xrefs` (real DATA xrefs, not the semantic `search_code` which gave FPs
landing inside Memory_Xbox.cpp):
- `0x820d3110` ← `0x826439c4`
- `0x820d3138` ← `0x82643c2c`, `0x82643a84`

So the string-bearing handler code sits in the gap `0x82642270` (end CampaignKey.cpp)
.. `0x82649c38` (start PropKeys.cpp) — a ~30 KB MULTI-TU gap (333 named text syms,
parsed from `auto_03_82260000_text.obj` COFF symbol table, value = VA-0x82260000).

### 3. The decisive disproof — `scripts/target_symbol_map.json` already names this region
The map (authoritative, BinDiff/oracle-seeded) shows the `0x82643xxx-0x82646xxx`
functions belong to OTHER TUs that share the identical Milo Node-base codegen
(vtable +0x70/+0x74/+0x9c, blank_album_art return), making them indistinguishable from
SongSortNode by structure:

| VA | mapped symbol | owning TU |
|----|---------------|-----------|
| 0x82643628 | `UsesRemoteStatusView@OvershellSlotState` | OvershellSlot* |
| 0x82644048 | `IsEnabled@NavListHeaderNode` (I mis-traced as ShortcutNode::IsActive) | NavListSortNode.cpp |
| 0x826440b8 | `FirstClip@LayerArray@HamDriver` | HamDriver |
| 0x826454e8 | `__lower_bound<...NavListSortNode...CompareHeaders>` | NavListSortNode.cpp |
| 0x82645868 | `DeleteAll@NavListShortcutNode` | NavListSortNode.cpp |
| 0x826458f0 | `DeleteAll@NavListSortNode` | NavListSortNode.cpp |
| 0x82645a48 | `__equal_range<...NavListSortNode...>` | NavListSortNode.cpp |
| 0x82645da8 | `NavListShortcutNode::NavListShortcutNode` | NavListSortNode.cpp |
| 0x826461d0 | `ChooseModeProvider::scalar dtor` | ChooseModeProvider |
| 0x82646220 | `Insert@NavListShortcutNode` | NavListSortNode.cpp |

NavListSortNode is RB3-specific (absent in rb3-Wii `band_r_wii.map`); it is a parallel
copy of the Node/ShortcutNode/HeaderSortNode/SortNode pattern, so its handlers, blank-
album-art returns, and `__lower_bound/__equal_range<...CompareHeaders>` STL helpers are
byte-shaped like SongSortNode's. The handler cluster is THEIR code, not SongSortNode's.

### 4. SongSortNode.cpp's own base members are ~25 KB away, already inside a pin
`Node::Node(SongSortCmp*)` = `0x825A6640`, `SortNode::SortNode(SongSortCmp*)` =
`0x825A6FD0`, `Object::ClassName` = `0x825A6708` — all INSIDE the existing
`band3/meta_band/StoreSongSortNode.cpp` pin (`0x825a6640-0x825a7038`). That pinned unit
currently reads **4/25 matched, fuzzy 10.8%** (report.json) — i.e. retail folded
SongSortNode base ctors + StoreSongSortNode + `UITransitionHandler::vdtor` into one
mixed region. SongSortNode's remaining methods/handlers are scattered elsewhere
(handler bodies near 0x82643-0x82645, interleaved with NavList*), with no run long
enough to bound.

## Why a pin fails the honesty gate
Any `.text` start:end over 0x82643xxx-0x82646xxx would bracket ≥8 contiguous FOREIGN
fn_@0% (NavListShortcutNode/NavListSortNode/OvershellSlotState/ChooseModeProvider),
violating the HONESTY GATE ("no ≥8-contiguous FOREIGN fn_@0% run"). The SongSortNode
base ctors already sit in a *different* pinned unit. There is no own-named bracket.

## What WOULD be needed (not self-contained, not this lane)
1. Identify + wire NavListSortNode.cpp FIRST (the dominant owner of the 0x82643-0x82646
   region; RB3-specific, no Wii oracle — needs Ghidra/BinDiff reconstruction).
2. Re-attribute the StoreSongSortNode.cpp pin (it is a folded mixed region; the SongSort
   Node/SortNode base ctors there should be re-mapped, possibly splitting the pin).
3. Only after the surrounding TUs are pinned/owned can SongSortNode's residual fns be
   reveal-mapped per-VA (re-pins need no new map entries; the VA-keyed map auto-pairs).
This is a multi-TU disentangle, not a port-then-pin — out of scope for an INDEPENDENT
single-TU lane.

## SongSort.cpp (NodeSort/SongSort/SetlistSort) — same wall, emitted as frontier
SongSort.cpp (`NodeSort`, `SongSort::BuildSongTree/BuildSongList`,
`SetlistSort::BuildSetlistTree`) shares the identical `CompareShortcuts`/`equal_range`
STL-template + Node-base codegen and the same MusicLibrary coupling. It will land in the
same scattered/interleaved region and is blocked by the same NavList* ownership problem.
Defer until NavListSortNode.cpp is owned.

## Tooling notes (reusable)
- `auto_03_82260000_text.obj` is PowerPC64-BE COFF (machine 0x1f2, LE header). llvm-nm/
  objdump can't parse it; parse the symbol table by hand: 18-byte records at symptr,
  `value` = VA − 0x82260000, string table at symptr+nsym*18. Script pattern in this
  session's /tmp parse worked (333 syms enumerated in the gap).
- Ghidra `list_xrefs(addr)` gives REAL data xrefs to `.rdata` strings; `search_code`
  (semantic) gives FALSE clusters (it matched Memory_Xbox.cpp for these strings).
- `scripts/target_symbol_map.json` is the fastest disambiguator for "which TU owns this
  fn_" in an interleaved gap — query it BEFORE trusting structural identity, because the
  Milo Node base-class codegen is shared verbatim across SongSortNode / NavListSortNode.
