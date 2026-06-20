# W9 L1 — BandSongMgr port-then-pin (frontier "bandsongmgr-port")

Verdict: **REAL_ACTIONABLE**. Date: 2026-06-20. Baseline: main @812e1df, 8314 matched.
Mode: adversarial discover/planner, READ-ONLY in main.

## Claim under test

Port `band3/meta_band/BandSongMgr.cpp` from rb3-Wii, convert 3 maps to hash_map,
wire objects.json, pin `[~0x82631350, ~0x82632C54]`. est +16.

## Ground truth gathered (all from COFF auto_03_82260000_text.obj + auto_00 rdata)

The auto COFF objs are XBOX360 PPC (machine 0x1f2); host objdump can't parse them.
Parser: `value` field = section-relative offset; VA = 0x82260000 + value (`.text` is
the only section, base from filename). Confirmed against `fn_<VA>` naming.

### Cluster identity — CONFIRMED BandSongMgr (5 independent tells)

1. **find-COMDAT density.** In the gap [NameGenerator end 0x82627200 .. HamCamShot
   0x82635720], 15 functions call a hash_map find-COMDAT. 12–13 of them cluster in
   `[0x82631298, 0x82632C18]`: BOTH `fn_82543F88` (Symbol-key find) and `lbl_82552CD0`
   (int-key find) appear — exactly BandSongMgr's mixed key types (mSongNameLookup
   int→Sym, mSongIDLookup Sym→int, mExtraSongIDMap int→Sym).
2. **String tells (resolved from rdata).** Functions in the span reference
   `'song_id'` (0x820802A8) and `'licenses.dta'`/`'licenses'` (0x820CEEB8/0x820CEEC8).
   The oracle BandSongMgr.cpp has the exact `song_id` MILO_WARN ("...duplicate
   song_id %d!", AddSongIDMapping line 672) and creates/uses `LicenseMgr` (licenses).
3. **Dispatch tables.** lbl_820CEC7C / lbl_820CEFC4 in rdata are pointer arrays
   holding back-references into the span (0x82631970, 0x826314F8, 0x826329D0,
   0x82632238, 0x82632C18...) — BandSongMgr method/handler tables.
4. **Adjacency.** w7 found `LicenseMgr::HasLicense` sliver at 0x826311B8 immediately
   below the cluster; BandSongMgr holds `LicenseMgr *mLicenseMgr // 0x12C`.
5. **Oracle map shape.** rb3-Wii BandSongMgr.cpp declares all 3 as `std::map`
   (lines 102–104, 208–227, 669–681) but retail inlines the hashtable-find COMDAT =
   retail compiled them as `std::hash_map`. This is the W9 hash_map vein.

### The hash_map conversion is already PROVEN for the base class

`src/system/meta/SongMgr.h` (BandSongMgr's parent) ALREADY converted all 5 of its
maps to `std::hash_map` (lines 17–34, members at 0x34/0x54/0x70/0x8c/0xa8) and
defines the `hash<Symbol>` spec under guard `RB3_HASH_SYMBOL_DEFINED` (lines 27–34).
BandSongMgr.h `#include`s SongMgr.h transitively (via the parent), so the spec is
already in scope — **do NOT redefine `hash<Symbol>`** (guarded, would no-op anyway).
SongMgr.cpp is wired + pinned. Full write-up: `2026-06-11-bp4-songmgr.md`.

### BandSongMgr.h already partly converted

`src/band3/meta_band/BandSongMgr.h` exists (created 2026-06-16). Line 45 already
uses `std::hash_map<int, SongMetadata *>` in an AddSongData signature. BUT the 3
member maps at lines 102–104 are still `std::map<int,Symbol>` / `std::map<Symbol,int>`
/ `std::map<int,Symbol>`. These are the ones to convert.

## BinDiff oracle is NOISE here (Waypoint-lesson confirmed)

`unified_id_rb3wii.json` places **ZERO** BandSongMgr functions in the gap; it scatters
50 low-confidence matches across 15 unrelated TUs (MainHubPanel, GemPlayer,
StationManager, VocalOverlay...). Classic BinDiff FP for an UNWIRED TU: MD-index
matching can't anchor to BandSongMgr because it isn't compiled, so it spreads
BandSongMgr's fns onto structurally-similar functions elsewhere. **The COFF
find-COMDAT + string tells are the authoritative signal, not BinDiff.**

## Boundary correction — the frontier's pin span is WRONG at BOTH edges

Frontier claimed `[0x82631350, 0x82632C54]`. COFF disassembly-boundary truth:

- **Upper edge: 0x82632C54 SPLITS fn_82632C18** (which runs 0x82632C18..0x82632C98
  and calls fn_82543F88 Sym-find = BandSongMgr). A pin must end on a function start.
  fn_82632C98 + fn_82632CC0 (calls fn_82632B98 = BandSongMgr) are BandSongMgr tail.
  **fn_82632D18 starts the next TU** (calls `insert_unique<pair<Symbol,
  AccomplishmentCategory*>>` — an AccomplishmentCategory map, NOT a BandSongMgr
  member; AccomplishmentCategory.cpp is pinned elsewhere @0x8243EF98 = a scattered
  COMDAT cluster). So **BandSongMgr ends at 0x82632D18.**
- **Lower edge: 0x82631350 EXCLUDES genuine BandSongMgr fns.** fn_82631298,
  fn_82631318, fn_826313A8 all reference lbl_82552CD0 (int-find) = BandSongMgr.
  CAVEAT: fn_826311B8 + fn_82631298 ALSO call `_Rb_tree::_M_find<H>` on a
  `map<int, SongStatus>` (NOT a BandSongMgr member — SongStatus belongs to
  SongStatus/BandUserMgr). These are either BandSongMgr methods that take a
  user/status map by reference (e.g. GetValidSongs / IsSongUnplayable, which take
  `BandUserMgr&`) or an adjacent class. The lower edge is therefore FUZZY between
  ~0x82631158 and 0x82631350. Functions 0x82630E18..0x82631134 above are a
  vtable/RTTI block + BinStream cache (Read/WriteCachedMetadata) — likely
  BandSongMgr's serialization, but verify per-fn at apply time.

**Recommended starting pin:** `.text start:0x82631298 end:0x82632D18`. The worktree
agent MUST tune empirically: extend the lower edge down toward 0x82631158 (or
0x82630EB8 if the BinStream cache fns pair) only if fm stays >0 and no ≥8-contiguous
foreign fn_@0% run appears; pull the upper edge back to 0x82632C98 if fn_82632C98/CC0
read 0%. Pin to a function START boundary on both ends (use the COFF fn_ list).

Gap composition is MULTI-class (≈593 anon fns, slivers: GamePanel @0x8262F4B0,
CharMeshHide @0x82630880, LicenseMgr @0x826311B8) — do NOT pin the whole gap.

## Existing target_symbol_map entries (scattered, do not conflict)

A few BandSongMgr methods are already mapped to OTHER compiled TUs (linker placed
them there): 0x822A7908 IsDummySong, 0x8255E450 GetAlbumArtPath, 0x8264C8F0 ??_E
(scalar-deleting dtor). These are outside the pin span and are unaffected.

## EV honesty note (CLAUDE.md EV rule)

Rank by RETAIL-MAP named-method count in range, NOT oracle obj fn count. The oracle
has 56 named BandSongMgr methods but many are tiny inline accessors that retail
folded into callers or that landed in other TUs (the scattered tsm entries prove
this). The pin span `[0x82631298, 0x82632D18)` holds ≈40 anonymous fns; realistic
matched yield after porting the real-bodied methods is **+12..+18** (est +16 fair),
NOT 40. Many will be ICF/funclet/permuter-class; the find-using methods are the
high-confidence wins once the maps are hash_map.

## Self-contained work-item (one worktree does ALL of: port + convert + wire + pin)

See actionable_items. attribution_risk=true (it's a pin/relocation).
