# W9-L1 — hash_map UNCONVERTED-caller census (ground-truth COFF)

**Date:** 2026-06-20
**Mode:** ADVERSARIAL DISCOVER/PLANNER (Opus L1), READ-ONLY in main.
**Baseline:** main, 8314 / matched (fixed baseline for all W9 agents).
**Frontier item:** `hashmap-unconverted-census` (kind=scout, est +15).
**Verdict:** **REAL_ACTIONABLE** — emitted self-contained work-items + adjacent frontier.

---

## TL;DR

The w8 dossier (`2026-06-19-w8-hashmap-exhaustion.md`) re-opened the vein and named
2 big clusters (α @825B8, β @82631) + residuals. This L1 census **re-derived the full
caller map from COFF ground truth** (`auto_03_82260000_text.obj`) and **corrected two
load-bearing mis-identifications** in the prior docs:

1. **Cluster β @0x82631xxx is `SongUpgradeMgr`, NOT `BandSongMgr`.** Container offsets
   decode to set@0x4 + map@0x1c + map@0x38 + map@0x54 — the exact RB3_MAP_0x1C-shifted
   SongUpgradeMgr layout (Wii: set@0x4, map@0x1c, map@0x34, map@0x4c). `fn_826311B8` is
   `SongUpgradeMgr::HasUpgrade(int)` (set::find at this+0x4), NOT `LicenseMgr::HasLicense`
   as the w7 doc claimed. `fn_82632150`=`GetUpgradeData(int)`, `fn_82631458`=
   `MarkAvailable(int,Symbol)` — bodies confirmed against rb3-Wii line-by-line. **This is
   the highest-confidence, cleanest port-then-pin in the entire vein.**

2. **BandSongMgr's maps DO NOT appear as hash_map callers at all.** Zero find-COMDAT
   callers hit container offsets 0xc4/0xdc/0xf4 (BandSongMgr's `mSongNameLookup`/
   `mSongIDLookup`/`mExtraSongIDMap`). So the prior "WI-2 BandSongMgr (=A6)" is **not
   corroborated by the hash_map vein** — if BandSongMgr is ported it's a generic port,
   not a hash_map-driven harvest. (Its int maps may stay `std::map`/inline-rbtree in retail,
   or its cluster lives elsewhere.) De-prioritize relative to SongUpgradeMgr.

3. **3rd find-COMDAT `82B23238` has ZERO callers in `.text`.** All 411,183 relocs scanned;
   no `.text` reloc targets it. It is ICF-dead in the main image or its callers live in a
   separate section/module not in auto_03. Drop WI-6 (w8) as a dead scout unless the
   82B2xxxx band is split as its own object later.

---

## Method (ground truth)

`/tmp/coff_hashmap_census.py` + `/tmp/pin_xref.py` (reproducible):
- Parsed `auto_03_82260000_text.obj` (1 .text section, vaddr=0, sym value = VA−0x82260000,
  IMAGE_BASE=0x82260000, 75,597 code syms, extended reloc count format).
- Found target COMDAT sym indices: `fn_82543F88` idx 28632 (Symbol-key hash find),
  `lbl_82552CD0` idx 29234 (int-key hash find — confirmed hashtable, modulo+slist, NOT
  rbtree), `fn_82B23238` idx 69268 (3rd find).
- Walked every .text reloc, resolved each caller VA → owning fn via sorted symtab,
  decoded `addi rX, r3, imm` before each `bl find` for the container offset.
- Intersected caller VAs against `config/45410914/splits.txt` pins.

**Caller totals:** 82543F88 = **86** reloc sites, 82552CD0 = **71**, 82B23238 = **0**.

---

## Census result (owner-aggregated, pinned vs unpinned)

### PINNED-converted units with residual find-using <100 fns (BODY-PORT grind)
| unit (pin) | hash_map callers | status |
|---|---|---|
| `band3/meta_band/AccomplishmentManager` | ~22 (offs 0x48..0x1bc) | 186/314; header converted; 6 named find-users @0% = vector/body ports |
| `band3/meta_band/AccomplishmentProgress` | 5 (offs 0x30, 0x5f8..0x64c) | 68/110; header converted; Poll/ClearNewRecords body-rederive |
| `SongMgr` | ~14 (offs 0x70, 0xa8, 0x8c, 0x1c) | 51/64; 2 Symbol-keyed maps converted (w6); 12 anon@0% body-ports remain |
| `FixedSizeSaveableStream` | 2 (off 0x30) | 9/12; ctor 55.65% / LoadTable 10% / AddSymbol 0% body-ports |
| `DataFunc` | 2 (stack-local containers) | NOT a type fix; resolves on body-port (w6 confirmed) |
| `MoviePanel` | 1 | already converted residue |

### UNPINNED / UNCONVERTED clusters (the remaining mass)
| cluster VA-span | find COMDAT / offsets | owner | gap |
|---|---|---|---|
| **β 0x826311B8–0x82632c98** | SYM@0x1c/0x38 + INT@0x4/0x1c/0x54 | **SongUpgradeMgr** (CONFIRMED) | NameGenerator→HamCamShot |
| **α 0x825B8738–0x825B9ED0** | INT@0x38 (×23), value+0xc=short, value+0xa4=bitfield | **singleton mgr, owner UNRESOLVED** | MoggClip→OvershellSlot |
| 0x82563038 (+4 more) | INT, singleton FUN_82803f30, multi | unidentified, Meta→StreamRecorder gap | large multi-class |
| 0x8234a2e8/+0x328 (off 0x38), 0x8234a370 (0x80) | SYM | Char/skeleton, AnimFilter→SkeletonUpdate gap | recon |
| 0x82551e98/ee0/f28 (off 0x6c/0x34/0x50) | SYM | unidentified, DataUtl→ThreeDSoundManager gap | recon |
| 0x82266d60/0x82267040 (off 0x1f0) | SYM | big-container class, WebSvcReq/NetStream region | recon |
| scattered singletons (5+9 in Acc*/MiniGame gaps) | SYM/INT | mixed game gaps | low-density |

---

## WI-1 — SongUpgradeMgr port-then-pin-then-convert  ⭐ TOP (was mis-labeled BandSongMgr)

**One worktree does ALL of: port .cpp + convert 3 maps→hash_map + wire NonMatching + pin + map-entries.**

- **Source oracle:** `../rb3/src/band3/meta_band/SongUpgradeMgr.cpp` (335 lines, 19 methods,
  Wii MWCC→MSVC port). Header **already present** at `src/band3/meta_band/SongUpgradeMgr.h`
  (and `BandSongMgr.h` already `#include`s it). Also has nested `SongUpgradeData` class.
- **Convert (the hash_map fix):** in `SongUpgradeMgr.h`, change the **3 maps** to
  `std::hash_map` (keep `mAvailableUpgrades` as `std::set` — set::find is `FUN_822e58a8`,
  rbtree, unchanged):
  - `mUpgradeData` (map<int,SongUpgradeData*>@0x1c) → `hash_map<int,SongUpgradeData*>`
  - `unk34` (map<Symbol,vector<int>>@0x34) → `hash_map<Symbol,vector<int>>`
  - `unk4c` (map<int,Symbol>@0x4c) → `hash_map<int,Symbol>`
  Add `hash<Symbol>` specialization (same guard idiom as AccomplishmentManager.h /
  SongMgr.h: `#ifndef RB3_HASH_SYMBOL_DEFINED`). Drop any RB3_MAP_0x1C gate for the
  converted maps; if int-key maps need 0x1c they are now hash_map (0x1c naturally) — but
  VERIFY: hashtable sizeof is 0x1c already, so the "dead pad" the gate added = the float
  `_M_max_load_factor@0x18` (per W9 master note). Net: container sizes already 0x1c.
- **Caller-method ground truth (drives the body port, confirmed line-by-line vs Wii):**
  - `fn_826311B8` HasUpgrade(int): `mAvailableUpgrades.find(id)!=end()` (set@0x4).
  - `fn_82631458` MarkAvailable(int,Symbol): find mUpgradeData@0x1c, test `version<=1`,
    insert set@0x4 + `unk4c[i]=s`@0x54.
  - `fn_82631318` ContentName(int): `unk4c.find(i)` @0x54 → `it->second.Str()`.
  - `fn_82632150` GetUpgradeData/UpgradeData(int): find@0x1c, copy value+8 (SongUpgradeData*).
  - `fn_82631350`/`826314f8`/`82631658` GetUpgradeSongsInContent/ClearFromCache: unk34@0x38.
  - `fn_82631298` (set@0x4 + map@0x1c): UpgradeData(int) full-path (set guard then map).
- **Pin span (attribution_risk=true):** the cluster is contiguous small accessors. SongUpgradeData
  methods precede SongUpgradeMgr's. Bound by inspection: lower edge ≈ **0x82630A98**
  (SongUpgradeData Save/Load region; verify it's SongUpgradeData not the prior sliver),
  upper edge ≈ **0x82632c98** (last accessor fn_82632c18 +0x80). The whole gap is
  `[0x82627200(NameGenerator end), 0x82635720(HamCamShot start))`; SongUpgradeMgr is the
  82630A98–82632C98 sub-band. **Derive the exact span in-worktree via the per-fn `.s` and
  the report pairing; over-pin to the next-pin edge is clean, gap-shrink is not.**
  GamePanel(0x8262F4B0)/CharMeshHide(0x82630880) StaticClassName slivers precede — keep the
  pin start AFTER them or accept they're SongUpgradeData-adjacent; verify which TU owns
  0x826305E0..0x82630A80 before fixing the lower edge.
- **Wire:** add `"band3/meta_band/SongUpgradeMgr.cpp": "NonMatching"` to objects.json.
  Gen target-map entries (`tools/gen_game_target_map.py`) so the renamer pairs the pinned
  anon fns by name.
- **expected_delta:** +12–20 (19 methods, most are thin accessors that go byte-exact once
  the 3 maps are hash_map and the find COMDAT pairs). attribution_risk: TRUE (new pin +
  multi-class gap lower edge).

## WI-2 — SongMgr hash_map body-port residuals (header already converted)

**No splits/type change — pure per-fn body-port grind. Self-contained (one worktree).**
12 anon@0% in the SongMgr pin (`[0x82783A00,0x82785668)`); the hash_map-using ones are
already type-correct (w6 converted mSongIDsInContent@0x70 + unkmap5@0xa8). Port from
`../dc3-decomp/src/system/meta/SongMgr.cpp` (closest twin) the find/iterate bodies:
`fn_82784510` ContentDiscovered (336B), `fn_82784FA0` ContentMounted (164B),
`fn_82784308` hash_map::erase (260B), `fn_82783CD8` HasSong/Cached (68B). Skip the EH
funclets (fn_82784DB8/82785398/827855A8) and the 1112B SaveCachedSongInfo (fn_82784830 —
permuter-class). `fn_82784D88` @99.80% = a 1-line permuter near-miss (try /permute).
**expected_delta:** +2–4. attribution_risk: FALSE.

## WI-3 — AccomplishmentProgress hash_map-iterate body-rederives

Header ALREADY hash_map (w8 verified). `Poll` (1.14%) + `ClearNewRecords` (11.96%) are
retail-REDERIVE (Wii `Poll(){}` empty; no same-named Wii body). Use the Wii `FakeFill`
map-iterate idiom (lines 484-516) as the PATTERN; reconstruct exact body from retail asm.
`IsUploadDirty` (71.43%) is a dtk TARGET_BOUNDARY divergence (roadmap D1, jeff-side, NOT a
source fix — do not touch). **expected_delta:** +1–2, MEDIUM risk (oracle is pattern-only).
attribution_risk: FALSE.

## WI-4 — FixedSizeSaveableStream residual body-ports

**Correction to w8 WI-4:** GetID at 0x82786420 is the FSSS pin START (already inside
`[0x82786420,0x82786788)`) — **NO pin-extend needed/possible.** Just body-port the 3 named
residuals: `??0FixedSizeSaveableStream` ctor (55.65%), `LoadTable` (10%), `AddSymbol` (0%)
from `../dc3-decomp/src/system/meta/FixedSizeSaveableStream.cpp` (has the map<int,X> member).
**expected_delta:** +1–3. attribution_risk: FALSE.

---

## Adjacent frontier (seeds for L2 — found, not fully planned)

- **Cluster α @0x825B8 owner ID (BIG, recon-gated).** 23 accessors, INT-key hash_map@this+0x38,
  value+0xc=short, value+0xa4=bit-array indexed `(idx*4+base)<<3`. Singleton via FUN_82803f24/34
  ('TheX' returning a global), references `TheSongMgr` (calls `Function_82783CD8` = SongMgr
  HasSong/Cached) and a vtable@+0x40/+0x5c. NO string/code anchor in the gap (all except_data).
  Candidate not SongUpgradeMgr (its map is @0x1c not 0x38) — a per-song bit-state mgr. Needs
  Ghidra RTTI/vtable owner-ID + oracle match. Adjacent gaps to bracket: prior anchors CuePoint
  sort @0x825BC7E8, FlowManager dtor @0x825BD6E0, MainMenuPanel::DeleteDownloadedArts @0x825BE388.
- **Meta→StreamRecorder gap singleton @0x82563038** (5 int-key callers, FUN_82803f30 singleton,
  iterates a list@+0x58 with count@+0x68). Multi-class 0x19000-byte gap; lower confidence.
- **BandSongMgr "where is it / do its maps stay std::map?"** — zero hash_map callers at 0xc4+.
  Decide: is BandSongMgr ported generically (no hash_map harvest), or are its int maps inline-rbtree
  in retail? Resolve before spending a BandSongMgr port slot on the "hash_map" premise.
- **DataUtl→ThreeDSoundManager Symbol-key trio** (0x82551e98/ee0/f28 @0x6c/0x34/0x50) — an
  unidentified class with ≥3 Symbol-keyed hash_maps. Scout-then-pin.
- **AnimFilter→SkeletonUpdate Symbol-key @0x38/0x80** (0x8234a2e8/328/370) — a Char/skeleton class.

---

## Bottom line

The vein has **one A-tier clean win (SongUpgradeMgr, WI-1)** the prior docs misattributed to
BandSongMgr; **three B-tier body-port grinds** (SongMgr/AccProg/FSSS residuals); and **two
real recon-gated clusters** (α @825B8, Meta-gap) needing owner-ID before they're actionable.
The 82B23238 3rd-COMDAT is a dead scout (0 callers). BandSongMgr is NOT a hash_map play.
