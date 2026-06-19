# W7 Hash-map Blob Scout — 2026-06-19

**Lane:** hashmap-blobs  
**Mode:** READ-ONLY SCOUT (no edits, no builds, no commits)  
**Baseline:** 8220 matched functions  
**Analyst:** Sonnet 4.6  

---

## Goal

Identify the owner TU, pin span, and wired-status for five unpinned auto_03 blob
labels that call hash_map find COMDATs (fn_82543F88 = Symbol-key; lbl_82552CD0 =
int-key):

1. auto_03_82272EB4 — claimed 76 fn_82543F88 calls (biggest)
2. auto_03_825458DC — claimed 18
3. auto_03_82621968 — claimed 11
4. auto_03_82627200 — claimed 10
5. auto_03_82783A00 — claimed 7

---

## Method

1. Read raw COFF reloc tables from `auto_03_82260000_text.obj` (411,183 relocs,
   extended-count format: first reloc entry's VirtualAddress = actual count).
2. Cross-ref caller VAs against `scripts/target_symbol_map.json` for named
   neighbors; confirm with `config/45410914/splits.txt` pin ranges.
3. Check `config/45410914/objects.json` for wired status; check
   `build/45410914/src/…` for compiled .obj.

---

## Key COFF facts

- **fn_82543F88** (Symbol-key hashtable find): symtab index 28632.
  Total COFF relocs referencing it: **86** (after counting all type-0x14 refs).
- **lbl_82552CD0** (int-key hashtable find): symtab index 29234.
  Total COFF relocs referencing it: **71**.

---

## Per-candidate findings

---

### Candidate 1: auto_03_82272EB4 — "76 fn_82543F88 calls" — STALE LABEL

**VA:** 0x82272EB4  
**Verdict:** REFUTED — label is a stale artifact of a deleted bogus pin.

#### Evidence

`git log --oneline` shows commit **6afb0f1** ("misc: remove bogus Main.cpp pin
that displaced BandCharacter") deleted the splits.txt entry:

```
Main.cpp:
    .text    start:0x82272E68 end:0x82272EB4
```

The VA 0x82272EB4 was that deleted pin's END address. It was never a standalone
auto-blob; the label was an artifact of the prior analysis that referenced the
pin boundary.

**Current coverage:** 0x82272EB4 falls inside the live BandCharacter.cpp pin
`[0x8226C738, 0x82275EA8)`.

**COFF reloc check:** Zero COFF relocs from any VA in
[0x8226C738, 0x82275EA8) reference fn_82543F88 (symbol index 28632).
BandCharacter uses `std::map<std::string,Transform>` — no Symbol hash_map.

**Wired/compiled:** YES — `build/45410914/src/system/bandobj/BandCharacter.obj`
exists. BandCharacter is `NonMatching` in objects.json (line 41).

**Action:** None. The 76-call claim was computed against the now-deleted pin
boundary; the real owner (BandCharacter) has zero hash_map find refs.

---

### Candidate 2: auto_03_825458DC — "18 fn_82543F88 calls" — ALREADY CONVERTED

**VA:** 0x825458DC  
**Verdict:** Inside AccomplishmentManager.cpp pin. hash_map conversion COMPLETE.
Remaining unmatched fns are structural/implementation issues, not hash_map
type gaps.

#### Evidence

**Pin:** `band3/meta_band/AccomplishmentManager.cpp: .text start:0x825426A0
end:0x8254BC90` (splits.txt). 0x825458DC is inside this range.

**Compiled:** YES — `build/45410914/src/band3/meta_band/AccomplishmentManager.obj`
exists. Wired as `NonMatching` in objects.json.

**hash_map status:** `AccomplishmentManager.h` (line 194+) declares 14+
`std::hash_map<Symbol, …>` members. `hash<Symbol>` specialization present
(lines 26-33, guarded by `#ifndef RB3_HASH_SYMBOL_DEFINED`). Conversion DONE.

**Match rate:** report.json → 186/314 = **59.2%**. 119 fns at 0% fuzzy (structural
issues — not hash_map). 225 anonymous fn_ in range; 89 named via target_symbol_map.

**Hash_map-calling unmatched fns** (6 total, all 0% fuzzy, per report.json):
- fn_82545610 (448 bytes) — calls fn_82543F88
- fn_82546058 (588 bytes) — calls fn_82543F88
- fn_82547F98 (236 bytes) — calls fn_82543F88
- fn_8254AB28 (400 bytes) — calls fn_82543F88
- fn_8254ACE8 (352 bytes) — calls fn_82543F88
- fn_8254AE70 (420 bytes) — calls fn_82543F88

**Why 0% fuzzy:** These functions have 0% fuzzy (structural body mismatch), NOT
a type-layout issue. The hash_map headers are already correct; the body porting
is incomplete (Wii source used `std::map<>` traversal; retail uses
hashtable::find). This is a body-port grind task, not a quick type-swap.

**COFF call count:** 32 fn_82543F88 COFF relocs from [0x825426A0, 0x8254BC90)
(vs claimed 18 — discrepancy is counting method: 32 total refs, some functions
call find multiple times).

**Action:** Port the 6 unmatched hash_map-using method bodies from Wii
`map::find` semantics to STLport `hash_map::find`. Each is an independent
body-port (no splits.txt change needed). Not a quick win; expected delta
is low until bodies are ported.

---

### Candidate 3: auto_03_82621968 — "11 fn_82543F88 calls" — NEAR-ZERO

**VA:** 0x82621968  
**Verdict:** Near-zero — only 1 actual COFF reloc to fn_82543F88 from this
region; the caller is a single small function just before the NameGenerator pin.

#### Evidence

**Gap:** [0x826135CC, 0x82626F80) — between end of an unidentified cluster and
start of `NameGenerator.cpp: .text start:0x82626F80` pin.

The single fn_82543F88 reloc from this gap is at VA ≈ **0x82626F54** (the
function immediately before NameGenerator begins). This is a tail function of
an unidentified class, containing one symbol-lookup by Symbol key.

**Named neighbors:** No target_symbol_map entries found inside the gap. The gap
is multi-class (multiple anonymous clusters with no string anchors resolving
to a single identified TU).

**NameGenerator.cpp** is wired (`NonMatching`, objects.json) and pinned.

**Claim discrepancy:** 11 calls claimed vs 1 found. The prior analysis likely
computed this from a different sub-range or included lbl_82552CD0 (int-key) refs
which were separately counted. Total COFF relocs (Symbol+int-key) from the full
gap [0x826135CC, 0x82626F80): approximately 4-6.

**Action:** None immediately. The gap contains ≥1 unidentified class with sparse
hash_map usage. Identifying the owner would require string-anchor or RTTI analysis
(not executable in read-only scout mode). Low EV — only 1 confirmed hash_map call.

---

### Candidate 4: auto_03_82627200 — "10 fn_82543F88 calls" — BLOB-PORT-THEN-PIN

**VA:** 0x82627200  
**Verdict:** Gap [0x82627200, 0x82635720) = 0xE520 bytes containing multiple
classes. The largest identified cluster (BandSongMgr, ~6.4 KB) is NOT wired.
Needs port-then-pin before any hash_map benefit.

#### Evidence

**Gap:** `[0x82627200, 0x82635720)` — between end of
`NameGenerator.cpp: end:0x82627200` and start of `HamCamShot.cpp:
start:0x82635720` (splits.txt).

**Named slivers inside gap** (from target_symbol_map.json):
- 0x8262F4B0: `?StaticClassName@GamePanel@@SA?AVSymbol@@XZ` — GamePanel sliver
- 0x82630880: `?Init@CharMeshHide@@SAXXZ` — CharMeshHide sliver
- 0x826311B8: `?HasLicense@LicenseMgr@@QBA_NVSymbol@@@Z` — LicenseMgr sliver

**Cluster B: BandSongMgr [≈0x82631350, ≈0x82632C54]**

Identified by:
1. COFF relocs show **10 fn_82543F88** (Symbol-key) + **6 lbl_82552CD0** (int-key)
   refs from this sub-range — matching BandSongMgr's mixed `map<int,Symbol>`,
   `map<Symbol,int>`, `map<int,Symbol>` members (rb3-Wii oracle confirms at 0xC4,
   0xDC, 0xF4).
2. `LicenseMgr::HasLicense` at 0x826311B8 is immediately before the cluster,
   consistent with BandSongMgr having `LicenseMgr *mLicenseMgr; // 0x12C`
   (rb3-Wii oracle).
3. Cluster size ≈ 6.4 KB contains 10+ method-sized anonymous fns.

**Wired status:** BandSongMgr.cpp is **NOT** in `config/45410914/objects.json`.
No source file at `src/band3/meta_band/BandSongMgr.cpp` (would need porting from
Wii). No compiled obj exists.

**Other slivers:**
- GamePanel: in objects.json (`NonMatching`), NOT compiled (no src obj)
- CharMeshHide: in objects.json (`NonMatching`), NOT compiled (no src obj)
- LicenseMgr: in objects.json (`NonMatching`), NOT compiled (no src obj)

**Total gap size:** 0xE520 bytes, ≈593 anonymous fn_ symbols — multi-class,
not a clean single-TU blob.

**Plan for Opus agent:**
1. Port `band3/meta_band/BandSongMgr.cpp` from `~/code/milohax/rb3/src/band3/meta_band/BandSongMgr.cpp` (Wii).
2. Convert `std::map<int,Symbol>` members to `std::hash_map<int,Symbol>` and
   `std::map<Symbol,int>` to `std::hash_map<Symbol,int>` (retail pattern).
3. Add `hash<Symbol>` specialization (same guard as AccomplishmentManager.h).
4. Add to objects.json as `NonMatching`.
5. Derive pin span for BandSongMgr cluster: start ≈ 0x82631350 (first fn after
   LicenseMgr::HasLicense at 0x826311B8), end ≈ 0x82632C54. Verify via Ghidra
   or RTTI before committing span.
6. Add splits.txt entry, run ninja, check fm%.

**Expected delta:** 15-25 matched fns if hash_map porting is correct and span
is accurate. Medium-EV (requires port + new TU wiring).

---

### Candidate 5: auto_03_82783A00 — "7 fn_82543F88 calls" — ALREADY CONVERTED

**VA:** 0x82783A00  
**Verdict:** Inside SongMgr.cpp pin. hash_map conversion COMPLETE. Remaining
unmatched fns are implementation/body grind, not type issues.

#### Evidence

**Pin:** `system/meta/SongMgr.cpp: .text start:0x82783A00 end:0x82785668`
(splits.txt). 0x82783A00 is the pin's START address.

**Compiled:** YES — `build/45410914/src/system/meta/SongMgr.obj` exists.
Wired as `NonMatching` in objects.json.

**hash_map status:** `src/system/meta/SongMgr.h` declares:
```cpp
std::hash_map<int,SongMetadata*> mUncachedSongMetadata;
std::hash_map<int,SongMetadata*> mCachedSongMetadata;
std::hash_map<Symbol,vector<int>> mSongIDsInContent;
std::hash_map<int,Symbol> mContentUsedForSong;
std::hash_map<Symbol,String> unkmap5;
```
Conversion DONE. Wii oracle used `std::map<>` for the same members.

**Match rate:** report.json → 51/64 = **79.7%**. 13 unmatched.

**Hash_map-calling unmatched fns** (2 total, both 0% fuzzy):
- fn_82784510 (336 bytes) — calls hash_map find
- fn_82784FA0 (164 bytes) — calls hash_map find

**COFF call count:** 7 fn_82543F88 + 7 lbl_82552CD0 = 14 total hash_map find
refs from [0x82783A00, 0x82785668) (vs claimed 7 — again counted Symbol-key only).

**Action:** Port fn_82784510 and fn_82784FA0 body implementations from Wii
source (`map::find`) to STLport `hash_map::find` semantics. Expected delta: 2
matched fns for body-port work, 11 remaining are likely struct/funclet issues.

---

## Bonus: Actual largest unpinned hash_map cluster

The scan found the **real** largest unpinned cluster is NOT any of the five task
labels. It is:

**[0x825B86A0, 0x825C10D8)** — between `MoggClip.cpp` and `OvershellSlot.cpp`
pins — with **43 lbl_82552CD0** (int-key) refs and additional Symbol-key refs.
This cluster is multi-class (FlowManager dtor-proxy, MainMenuPanel::DeleteDownloadedArts,
early OvershellSlot methods are identifiable from target_symbol_map). The
43-ref sub-cluster owner is not yet identified. This should be the next scout
target if the hash_map vein is being pursued systematically.

---

## Summary table

| Blob label | VA | Claimed calls | Actual COFF refs | Owner TU | Wired | Compiled | Actionable |
|---|---|---|---|---|---|---|---|
| 82272EB4 | 0x82272EB4 | 76 fn_82543F88 | 0 | BandCharacter.cpp (stale label) | YES | YES | NO (refuted) |
| 825458DC | 0x825458DC | 18 fn_82543F88 | 32 | AccomplishmentManager.cpp | YES | YES | NO (hash_map done, body grind) |
| 82621968 | 0x82621968 | 11 fn_82543F88 | 1 | Unknown (gap pre-NameGenerator) | N/A | N/A | NO (1 call, owner unknown) |
| 82627200 | 0x82627200 | 10 fn_82543F88 | 10+6 | BandSongMgr (not wired) + slivers | NO | NO | Conditional (port-then-pin) |
| 82783A00 | 0x82783A00 | 7 fn_82543F88 | 7+7 | SongMgr.cpp | YES | YES | NO (hash_map done, body grind) |

---

## Conclusions

1. **Two blobs are stale/refuted** (82272EB4 = deleted-pin artifact;
   82621968 = claimed 11 but only 1 real call).
2. **Two blobs are already-converted** (825458DC=AccomplishmentManager,
   82783A00=SongMgr): hash_map headers correct; remaining work is body porting.
3. **One blob is port-then-pin** (82627200=BandSongMgr cluster + slivers):
   BandSongMgr not wired at all; needs full port+wire before any pin harvest.
4. **Real largest cluster** [0x825B86A0, 0x825C10D8) with 43 int-key refs is
   not in the task list — should be next scout target.
5. **Hash_map vein expected delta from this set:** ~0 immediate (all either
   refuted, already done, or requiring multi-step port). BandSongMgr port
   could yield +15-25 after port+pin.
