# W9 L3 — DC3 meta_ham Rosetta batch-ID of RB3 manager owners

**Date:** 2026-06-20
**Mode:** ADVERSARIAL DISCOVER/PLANNER (Opus, layer 3), READ-ONLY in main @812e1df (8314 matched).
**Frontier:** `metaham-dc3-rosetta-batch-id` (kind=scout, est +20).
**Verdict:** **REAL_ACTIONABLE** (with major premise correction).

The frontier's premise — *"cross-index DC3 meta_ham Matching units against RB3 unpinned
hash_map clusters to batch-identify owners by the find-COMDAT signature"* — is **partly
wrong**: most map-bearing DC3 meta_ham managers in RB3 retail use **std::map (rbtree),
NOT hash_map**, so the find-COMDAT scan does NOT enumerate them. The hash_map vein
remains narrow (SongStatusMgr + BandSongMgr + the already-landed AccMgr/AccProg). BUT
the DC3 Rosetta + rb3-Wii oracle DID locate three genuine unpinned manager TUs by
**string-anchor + singleton-thunk** instead — these are port-then-pin targets, ranked
below. The batch produced a ranked owner list as requested; it just isn't a hash_map list.

---

## Method (ground truth)

1. **DC3 set:** all 126 `lazer/meta_ham/*.cpp` are `Matching` in `config/373307D9/objects.json`.
   Intersect with rb3-Wii `band3/meta_band/*.cpp` (the authoritative GAME oracle — DC3 is
   a FALSE FRIEND for game code) → **38 shared classes**.
2. **Container members** extracted from the 38 DC3 headers → only 5 have map/hash_map
   members (AccMgr, AccProg, Campaign, SongSortMgr, SongStatusMgr). AccMgr/AccProg are
   already WIRED + hash_map-converted (the W8 "4-for-4"). SongStatusMgr is the parent L2
   item. New: **Campaign, SongSortMgr**.
3. **Cross-check vs rb3-Wii** (DC3 is wrong for game layout): SongSortMgr's RB3 maps are
   `mSongs@0x4` (map<Symbol,SongRecord>) + `mSetlists@0x1c` (map<Symbol,SetlistRecord>),
   NOT DC3's single `mSongRecordMap@0x78`. Campaign's RB3 maps are at 0x34/0x4c/0x6c.
4. **Locate in retail COFF** (`auto_03_82260000_text.obj` + `auto_00_82000400_rdata.obj`):
   resolve distinctive config strings to rdata VA, scan `.text` for the `lis/addi` immediate
   pair that loads that VA → owning function (the config-loader anchors the class cluster).
5. **find-COMDAT census** (the frontier's intended signal): map every reloc into
   `fn_82543F88` (Sym-key), `lbl_82552CD0` (int-key), `fn_82B23238` (3rd) to its owner;
   intersect owners against `splits.txt` pins → 74 UNPINNED owner-fns, 101 finds total.

---

## KEY RESULT — find-COMDAT (hash_map) owners are NOT the meta_ham managers

The unpinned find-COMDAT census is dominated by **SongStatusMgr** (`825B8xxx`, 23×
`int@0x38`, already the parent L2 item) and scattered singles. The named meta_ham
manager candidates are MOSTLY rbtree, not hash_map:

| candidate | retail location (anchor) | pinned? | find-COMDAT in range | container kind |
|---|---|---|---|---|
| **SongSortMgr** | fn_82581280 (`review_weights` @0x820a39a4) | NO | **0** in [0x82580000,0x82583DD8) (172 fns) | std::map (rbtree) — NOT hash_map |
| **Campaign** | fn_82590910 (`campaign_levels` @0x820a7470) | NO | 1 (fn_82590258) | mostly std::map |
| AccomplishmentManager | 0x825426A0–0x8254BC90 | YES | dense Sym-key (its 6 maps) | hash_map (converted) |
| SongStatusMgr | 0x825B8xxx | NO (parent L2) | 23× int@0x38 | hash_map (parent item) |
| BandSongMgr | 0x82631298 (parent L1) | NO | mixed Sym/int | hash_map (parent item) |

So the frontier's batch *does* feed the neighbour-pin-chain with a ranked owner list, but
two of the three new owners are **port-then-pin** (std::map kept), not hash_map-convert.

---

## Owner identifications (ground-truth chain per class)

### 1. SongSortMgr — fn_82581280 region [~0x82580040, 0x82583DD8), UNPINNED
- **Anchor:** `review_weights` (rdata 0x820a39a4) referenced UNIQUELY at fn_82581280;
  `song_select` at fn_82580160/825810A0/82581280. Singleton thunk `FUN_82803f14()`
  (== `TheSongSortMgr`, rb3-Wii SongSortMgr.cpp l.40/44).
- **Bounds:** pin below = `UIEventMgr.cpp` (ends 0x8257be58); pin above = `NetSync.cpp`
  (starts 0x82583dd8). The gap [0x8257be58, 0x82580040) likely holds the SongSort*
  comparator TUs (SongSortByDiff/BySong already pinned elsewhere — verify). SongSortMgr
  core = [~0x82580040, 0x82583DD8), 172 anon fns.
- **Container:** rb3-Wii `mSongs@0x4` map<Symbol,SongRecord>, `mSetlists@0x1c`
  map<Symbol,SetlistRecord>, `unk34@0x34` vector, `mInternalSetlists@0x3c` vector.
  ZERO find-COMDAT in range ⇒ retail kept these as `std::map` (rbtree _M_find).
  **Do NOT convert to hash_map** (the W8 vein does not apply here).
- **Oracle:** rb3-Wii `band3/meta_band/SongSortMgr.cpp` (597 lines, ~20 methods) +
  `.h` (singleton `TheSongSortMgr`, the FilterSet helper struct @0x0/0xc).
  Depends on SongRecord (map<Symbol,int> mTier@0x24) + SetlistRecord.
- **Effort/EV:** big multi-method class; some methods folded/permuter-class. Realistic
  +8..+15. attribution_risk=true.

### 2. Campaign — fn_82590910 region (~0x82590000), UNPINNED
- **Anchor:** `campaign_levels` (rdata 0x820a7470) + `campaign_keys` (0x820a7460) both
  referenced at fn_82590910 (the config DataArray loader).
- **Container (rb3-Wii):** `m_mapCampaignLevels@0x34` map<Symbol,CampaignLevel*>,
  `unk4c@0x4c` map<Symbol,Symbol>, `m_mapCampaignKeys@0x6c` map<Symbol,CampaignKey*>.
  Only 1 find-COMDAT in region ⇒ mostly rbtree; port-then-pin, keep std::map.
- **Oracle:** rb3-Wii `band3/meta_band/Campaign.{cpp,h}` + CampaignKey/CampaignLevel/
  CampaignEra deps. Bounds need derivation (no pin near 0x82590000 yet).
- **Effort/EV:** medium. Lower priority than SongSortMgr (more deps: Campaign pulls in
  CampaignKey/Level/Era/Performer). +5..+12. attribution_risk=true. **RECON-GATED** —
  emit as frontier-leaning actionable; bound it before paying a build.

### 3. NEW unidentified SongMgr-family manager — fn_8255F858 region [~0x8255F000, 0x82563400), UNPINNED
- **NOT a named meta_ham candidate** — discovered via the find-COMDAT census.
- **Tells:** fn_8255F858 = `int@0xd4` + `int@0x10c` hash_map<int,Symbol> getter
  (value@+8, NULL→`Symbol(PTR_DAT_82c411b0)`). fn_82561530 calls `FUN_82783aa8`
  (a SongMgr-base method — SongMgr.cpp pinned @0x82783A00–0x82785668) and allocates
  0x138-byte objects (SongMetadata/SongData sized). fn_82563038 iterates `this+0x58`
  / `this+0x68`. Singleton thunks `FUN_82803f30/0c`.
- **NOT BandSongMgr** (β is @0x82631298 with maps @0xc4/0xdc/0xf4, not 0xd4/0x10c) and
  NOT the SongMgr base (that's pinned @0x82783A00). This is a THIRD song-collection
  manager (a SongMgr subclass or sibling) using the int-key hash_map vein — a GENUINE
  hash_map find-COMDAT owner the prior scans never identified by class.
- **Action:** scout owner class (Ghidra RTTI/vtable + oracle). Likely a `BandSongMgr`
  sibling or `MusicLibrary`/`FakeSongMgr`-adjacent. Emitted as discovered_frontier.

---

## What the frontier got wrong (adversarial honesty)

1. **"byte-identical-to-retail manager structure" via DC3** — FALSE for game-layout. DC3
   meta_ham is Dance Central data; SongSortMgr/Campaign layouts DIFFER from RB3 (DC3
   SongSortMgr is `mSongRecordMap@0x78`; RB3 is `mSongs@0x4`). Use rb3-Wii for layout,
   DC3 only for the engine-base inheritance order + which container type retail picked.
2. **"unpinned hash_map clusters → batch-identify"** — the map-bearing meta_ham managers
   (SongSortMgr, Campaign) are rbtree in retail, invisible to a find-COMDAT scan. The
   batch found them by STRING ANCHOR, not hash_map signature. Only SongStatusMgr (parent
   L2), BandSongMgr (parent L1) and the new fn_8255F858 cluster are true hash_map owners.
3. **Candidate list pruning:** of the frontier's named candidates — SongSortMgr (real,
   port-then-pin, rbtree), MetaPerformer (vectors+sets, no map find-COMDAT — refuted as a
   hash_map target), ProfileMgr (vectors only — refuted), ContentMgr-Callback users
   (SongStatusMgr=parent L2, BandSongMgr=parent L1, SongUpgradeMgr=parent L2 sibling).

---

## Ranked owner worklist (feeds the neighbour-pin-chain)

| rank | owner | location | kind | pin? | EV | note |
|---|---|---|---|---|---|---|
| 1 | SongStatusMgr | 0x825B8xxx | hash_map convert+port | parent L2 | +18 | already actioned (L2 doc) |
| 2 | BandSongMgr | 0x82631298 | hash_map convert+port | parent L1 | +16 | already actioned (L1 doc) |
| 3 | **SongSortMgr** | ~0x82580040 | **port-then-pin (rbtree)** | NO | **+8..+15** | NEW — this batch |
| 4 | fn_8255F858 cluster | ~0x8255F000 | hash_map (scout owner) | NO | +5..+12 | NEW — needs class ID (frontier) |
| 5 | **Campaign** | ~0x82590000 | port-then-pin (rbtree) | NO | +5..+12 | NEW — recon-gate bounds first |

---

## Adjacent leads (seed later layers)

- **fn_8255F858 / fn_82561530 / fn_82563038 cluster [~0x8255F000, 0x82563400)** — a
  third song-collection manager (int-key hash_map @0xd4/0x10c, 0x138-byte records,
  calls SongMgr-base FUN_82783aa8). Identify the class via Ghidra RTTI/vtable + oracle
  (candidate: a BandSongMgr sibling, MusicLibrary, or a SongMgr subclass). True hash_map
  vein owner — high value if it's a portable RB3 class. **discovered_frontier.**
- **The 0x1f0-container Symbol-key callers** (fn_82266D60/267040, W8 "WebSvcReq/World")
  — a big-this class with a Symbol-key hash_map @0x1f0; un-IDed. **discovered_frontier.**
- **SongSort comparator gap [0x8257be58, 0x82580040)** — between UIEventMgr and
  SongSortMgr; likely SongSort/SongSortByArtist/ByRank/ByRecent/ByReview/ByStars (rb3-Wii
  has all). These are tiny comparator TUs (DC3 has SongSort/SongSortByDiff/BySong
  Matching) — a cheap pin-cluster once SongSortMgr lands. **discovered_frontier.**
- **SongRecord** (rb3-Wii: map<Symbol,int> mTier@0x24, Hmx::Object) — SongSortMgr's
  value type; needed for the SongSortMgr port; DC3 has SongRecord.cpp Matching. May be
  its own small TU.
