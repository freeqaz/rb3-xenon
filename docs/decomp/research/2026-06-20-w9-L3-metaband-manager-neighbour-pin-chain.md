# W9 L3 dossier — metaband-manager-neighbour-pin-chain (ADVERSARIAL)

Frontier item (kind=identify-then-pin, est +24): pin two singleton-manager TUs
flanking Instarank/SongStatusMgr. Candidate (A) "ContentMgr::Callback manager
TU" [0x82632C98,0x82632F00) (6 fns, hash_map@0x1c+bool@0x38, "clean cut:
SongUpgradeMgr ends 0x82632C98, Instarank starts 0x82632F00"); candidate (B)
NetGameData/BandNetGameData TU @0x826340F0.

## VERDICT: REAL_ACTIONABLE — but the frontier's geometry & identity are WRONG.

Ground truth (auto_03 text COFF by VA + Ghidra decomp + fingerprints.json +
rb3-Wii oracle string match):

### Falsified claims
1. **The candidate span [0x82632C98,0x82632F00) is NOT an independent TU.** It is
   the *tail* of a larger TU. The 6 fns in that range call back DOWN into
   [0x82632040,0x82632C98): fn_82632CC0→fn_82632B98; fn_82632D18 is *called by*
   fn_82632150 & fn_82632A28; fn_82632B98 *called by* fn_82632A28. Pinning only
   the sliver slices a TU mid-body. The strongly-connected call cluster is
   [0x82632150,0x82632E88].
2. **The class is `LicenseMgr`, not "ContentMgr::Callback" generic.** Decisive
   string evidence in the range: fn_82632040 returns ptr→`"licenses.dta"`,
   fn_82632050 returns ptr→`"licenses"` (== `LicenseMgr::ContentPattern()` /
   `ContentDir()`). fn_82632E88 (`ContentLoaded`) does
   `__RTDynamicCast(loader,0,&.?AVLoader@@,&.?AVDataLoader@@,0)` then dispatches
   to fn_82632D18 (`AddLicenses`). Oracle: `../rb3/src/band3/meta_band/LicenseMgr.cpp`
   is the ONLY file in either oracle with `licenses.dta`. (ContentMgr.cpp is a
   DIFFERENT, already-pinned engine TU at [0x8250AFE8,0x8250B558].)
3. **"SongUpgradeMgr ends 0x82632C98" is wrong.** SongUpgradeMgr is the TU BELOW
   (`songs_upgrades`@0x82630A68, `song_id`@0x82631BD8, `real_bass`@0x82631068,
   int-key map@0x1c via find-COMDAT 82552cd0, struct@0x38). Oracle:
   `../rb3/src/band3/meta_band/SongUpgradeMgr.cpp` (header layout matches the
   decomp EXACTLY: set<int>@0x4, map<int,SongUpgradeData*>@0x1c,
   map<Symbol,vector<int>>@0x34, bool mSongCacheNeedsWrite@0x64). Nothing is
   pinned in [0x8262C000,0x82632040). SongUpgradeMgr is also NOT wired in
   objects.json.
4. **The UPPER bound IS clean.** fn_82632F00 is a ctor (loads vtable
   &PTR@820cf1a4, sets fields incl. `[4]=10`) — a genuine TU boundary
   (= the "Instarank" TU). So 0x82632F00 (exclusive) is a valid pin end.

### Confirmed facts
- **LicenseMgr TU = [0x82632040, 0x82632F00)** = 41 functions (not 6). The 41
  includes LicenseMgr methods + its STL instantiations (std::map<Symbol,...> /
  set<Symbol> / vector<Symbol>) + the Symbol-key find-COMDAT inline (82543f88).
- LicenseMgr.cpp is wired `NonMatching` (objects.json:696) but UNPINNED.
- find-COMDAT 82543f88 = Symbol-key hashtable-find (sret r3, &container=this+0x1c,
  &key r5, NULL-miss, value@node+0x8). The retail LicenseMgr has a
  **map@0x1c + bool@0x38 cache** that the rb3-Wii DEV header LACKS (Wii header
  only has `std::set<Symbol> mLicenses @0x4`; Wii `AddLicenses`/`ContentLoaded`
  bodies are simpler). This is the standard CLAUDE.md false-friend: rb3-Wii dev
  build predates retail's cache machinery. Layout must be RECONSTRUCTED from the
  decomp, cross-referenced against SongUpgradeMgr's caching pattern (which IS
  fully in the oracle: map@0x1c + bool cache flag + WriteCachedMetadataToStream).
- NetGameData/BandNetGameData @0x826340F0: fn_826340F0 is a 4-insn trivial stub
  (`imms:["0"]`, no callees). RTTI `.?AVNetGameData@@`/`.?AVBandNetGameData@@`
  give class names but this is a separate, less-developed lead — emitted as
  frontier, not planned here.

## Actionable work (each self-contained, independently landable vs main@8314)

### AI-1 (PRIMARY): LicenseMgr.cpp — wire(already)+pin+reconstruct+port
- Pin `band3/meta_band/LicenseMgr.cpp .text start:0x82632040 end:0x82632F00`.
- gen_game_target_map for the 41 fns; reconstruct retail layout from decomp
  (set<Symbol>@0x4 + map@0x1c[Symbol→vector<Symbol>?] + bool cache@0x38), port
  bodies from `../rb3/src/band3/meta_band/LicenseMgr.cpp` BUT extend ContentLoaded
  /AddLicenses to the retail caching shape (mirror SongUpgradeMgr's cache idiom).
  attribution_risk=true (pin/relocation + mid-cluster boundary judgment).
- Expected: a meaningful chunk of the 41 (LicenseMgr's own ~10 methods + folded
  STL). Conservative +12..+24.

### AI-2: SongUpgradeMgr.cpp — wire+pin+port (the abutting TU below)
- Bound the span precisely (start ≈ 0x82630988 SongUpgradeData::InitSongUpgradeData
  region — verify the SongUpgradeData ctor is the first fn and the panel TU below
  ends cleanly; end = 0x82632040 = LicenseMgr start). Wire in objects.json
  (currently absent). Port from `../rb3/.../SongUpgradeMgr.cpp` — header layout
  matches retail EXACTLY, so this is a cleaner port than LicenseMgr. 58 fns in
  [0x82630988,0x82632040).

## Tooling note
`/tmp/coff_va.py` (VA-range symbol dump from auto_03), `/tmp/coff_readva.py`
(read data/string at VA from any auto_*.obj) — both reusable for COFF ground truth.
