# W8 — hash_map vein EXHAUSTION claim: ADVERSARIAL FALSIFICATION

**Date:** 2026-06-19
**Mode:** PLANNER / ADVERSARIAL VERIFIER (read-only in main repo)
**Baseline:** main @ da8258f, 8234 / 65543 matched.
**Area:** hashmap-exhaustion
**Verdict:** **REFUTATION_WRONG** — the wave-7 "hash_map vein EXHAUSTED (4-for-4)"
claim is **FALSE**. It only ever measured the 4 already-pinned/already-converted
units and dismissed the rest as "mirage/stale". Ground-truth COFF reloc analysis
shows **75 distinct hash_map-find call-sites in UNCONVERTED / UNPINNED units**,
including three dense single-container clusters, plus a third distinct Symbol-key
find-COMDAT instantiation the prior scans never knew existed.

---

## How the claim was built (and why it's wrong)

A1 in the roadmap states: *"hash_map vein — EXHAUSTED (wave-7 closed it). 4-for-4
converted … the A6 'auto_03 blob mass' was a MIRAGE — the big fn_82543F88
call-counts were already-converted pins."*

The wave-7 scout (2026-06-19-w7-hashmap-blobs.md) reached this by scanning callers
of **exactly two** COMDAT addresses — `fn_82543F88` (Symbol-key find) and
`lbl_82552CD0` (int-key find) — and checking whether the **5 specific blob labels**
in its task list fell inside already-pinned units. They did. The scout then
generalised "these 5 are done/refuted" into "the vein is exhausted".

**The flaw:** the scan never enumerated *all* callers of those two COMDATs, never
looked for *other* find-COMDAT instantiations, and never asked "which callers live
in units that are not pinned at all?" The w7 doc itself flagged this gap in its own
"Bonus" section ("the real largest unpinned cluster [0x825B86A0,0x825C10D8) … should
be next scout target") — and that lead was dropped, not pursued.

This is the Waypoint pattern: a verdict ("exhausted") promoted to fact without
enumerating the full ground-truth set.

---

## Method (ground truth, not labels)

All facts below are from `auto_03_82260000_text.obj` (the authoritative retail
COFF: full `.text`, 75,597 text symbols, 411,183 relocs, image base 0x82260000).

1. **Shape-scan** every text function for the STLport hashtable-modulo signature
   (`divwu` + `mullw` + `twllei rX,0` — the bucket = hash % bucket_count guard),
   size 0x30–0x140. Result: **54 COMDATs**, clustering into **46 distinct
   normalized bodies** (relative branches masked).
2. **Caller-map** every reloc that targets the find COMDATs, resolving each caller
   VA to its owning function via the sorted text-symbol table.
3. **Pin-attribution**: intersect caller VAs against `config/45410914/splits.txt`
   to separate converted-pinned callers from UNPINNED ones.
4. **Container decode**: for each caller, decode the `addi rX, r3, imm` that forms
   `&container = this + off` immediately before the `bl find`.

---

## CHECK 1 — other find-COMDAT instantiations beyond the two known? **YES.**

The find COMDATs are 128 bytes. Three distinct 128-byte normalized bodies exist:

| VA | size | norm-hash | status |
|---|---|---|---|
| `82543F88` | 128 | 693ac385 | KNOWN Symbol-key find |
| `82552CD0` | 128 | 9ef00483 | KNOWN int-key find |
| **`82B23238`** | 128 | **db5f9468** | **THIRD distinct find-COMDAT — never in any prior scan** |

`82B23238` is a distinct `(key,value,hash,equal)` find instantiation living in the
high `.text` (the `82B2xxxx` region — a different module/library cluster). Its mere
existence means there are hash_map members in TUs the Symbol-/int-key scans never
touched. Additionally there is a whole family of larger (256/264/288/304B) hashtable
COMDATs (insert / operator[] / rehash) — 46 distinct bodies total — confirming
hash_map is used far more widely than 2 instantiations.

> NOTE: 82B23238 lives in the `82B2xxxx` band (likely a vendored/library cluster).
> Owner identification + whether it is pin-worthy is **un-scouted** — flagged as a
> follow-up, not yet an actionable work_item (no source oracle confirmed).

## CHECK 2 — hash_map members that only iterate/operator[] (won't show in a find-scan)

Confirmed present. Example: `AccomplishmentProgress::Poll` (140B, **1.14%**) — retail
iterates `TheAccomplishmentMgr->mAwards` / `m_mapTourDesc` (cross-class hash_maps),
and Wii's `Poll()` is `{}` (empty in the dev build) — so this is a body-port that
*reads* hash_maps without calling the find COMDAT. The container-layout tell
(member sizeof 0x1c, slist value@+0x8) holds across all 23 `this+0x38` accessors in
the 825B8/825B9 cluster (Check 4). A find-only scan structurally **cannot** see
these — the prior "exhausted" scan was blind to this entire access mode.

## CHECK 3 — are the 4 landed conversions TRULY complete? **3 of 4 have residual find/iterate near-misses.**

| unit | matched | residual find/iterate near-misses |
|---|---|---|
| SongMgr | 51/64 | CLEAN (only an 8-byte `lbl_82783A60` stub left) — conversion complete |
| FixedSizeSaveableStream | 8/11 | ctor 55.65%, LoadTable 10%, AddSymbol 0% — body-port residue |
| AccomplishmentManager | 186/314 | 6 named <100 (mostly STL vector helpers; not hash_map) — type OK, vector body-ports |
| AccomplishmentProgress | 65/109 | **Poll 1.14% (hash_map-iterate), ClearNewRecords 11.96%, IsUploadDirty 71.43%** |

So even the "done" set has actionable hash_map-adjacent body-ports left (B-tier).

## CHECK 4 — blob-82627200 (and the bigger 825B8 cluster) for a tight single-TU sub-cluster

Two real, dense, single-container clusters confirmed:

### Cluster α — `[0x825B86A0, 0x825C10D8)` (gap MoggClip → OvershellSlot) — THE BIG ONE
- **23 thin accessor methods**, ALL hitting the SAME `hash_map<int, short>` at
  **this+0x38** (decoded: `addi rX,r3,0x38; bl lbl_82552CD0; … lhz node+0xC`).
  Plus the int-key COMDAT has 28 distinct callers in 825B8xxx/825B9xxx total.
- Nearest named anchors: a `CuePoint` `sort_heap` in anon-ns `?A0x81ddebd1`
  (0x825BC7E8), `FlowManager::~FlowManager` (0x825BD6E0), `MainMenuPanel::
  DeleteDownloadedArts` (0x825BE388). The 23 accessors sit BEFORE FlowManager's dtor.
- This is a single class with one big `hash_map<int,short>`-by-ID member + ~23
  one-line accessors. Owner not yet name-resolved (no string anchor inside the
  accessor run) — needs Ghidra RTTI/vtable + oracle match. **This is exactly the
  cluster the w7 doc named "the real largest unpinned hash_map cluster … next scout
  target" and then never scouted.**

### Cluster β — `[0x82631350, 0x82632C54]` inside the [NameGenerator → HamCamShot] gap = BandSongMgr (A6)
- Mixed Symbol-key (this+0x38, this+0x1c) and int-key (this+? ) accessors — 11
  Symbol-key + 5 int-key callers — matching BandSongMgr's `map<int,Symbol>`,
  `map<Symbol,int>`, `map<int,Symbol>` members (oracle 0xC4/0xDC/0xF4).
- `BandSongMgr.h` already exists (created 2026-06-16) but `.cpp` is **not wired,
  not compiled, not pinned**. This corroborates roadmap A6 — it is a genuine
  port-then-pin, NOT a mirage.

---

## Full unconverted-caller census (the headline number)

| find COMDAT | total distinct callers | in CONVERTED units | **UNPINNED/UNCONVERTED** |
|---|---|---|---|
| `82543F88` Symbol-key | 75 | 33 (AccMgr 22, AccProg 4, SongMgr 7) + DataFunc/MoviePanel | **40** |
| `82552CD0` int-key | 43 | 8 (SongMgr 6, AccProg 1, FSSS 1) | **35** |
| `82B23238` (3rd) | un-scanned for callers | — | unknown (>0) |

**75 find call-sites sit in units that are not pinned and not converted.** The
"exhausted" verdict accounted for none of them.

Unpinned Symbol-key callers cluster by gap (nearest-pin context):
- AnimFilter→SkeletonUpdate: 5 (this+0x38 / +0x80) — a Char/skeleton class
- SkeletonUpdate→StubCameraInput: 2 (this+0xac / +0x90)
- DataUtl→ThreeDSoundManager: 3 (this+0x6c/+0x34/+0x50)
- Meta→StreamRecorder / StreamRecorder→AccProg: 3
- NetSync→MoveVariant→AccTourConditional→MiniGameMgr→StreamRenderer: 9
- NameGenerator→HamCamShot (BandSongMgr β): 11
- WebSvcReq/NetStream/World region: 3 (this+0x1f0 — a big container)
- HamMaster→CameraManager: 1
- SongMetadata→FSSS: 1 (`GetID` at 0x82786420, this+0x30 = FSSS base, pin-extend)

---

## Work items (cold-executable)

### WI-1 — Cluster α single-TU hash_map<int,short>-by-ID owner (BIG, identify-then-port-then-pin)
`[0x825B86A0, 0x825C10D8)`, container hash_map<int,short> @ this+0x38, 23 accessors
+ FlowManager/CuePoint/MainMenuPanel neighbors. Needs Ghidra RTTI/vtable owner ID
+ oracle match before pin. attribution_risk: multi-class gap. expected +15–25 once
owner identified, converted (std::map→hash_map<int,short>@0x38), wired, pinned.

### WI-2 — BandSongMgr (β) port-then-pin (= roadmap A6, NOT a mirage)
Port `band3/meta_band/BandSongMgr.cpp` from rb3-Wii; `BandSongMgr.h` already exists.
Convert the 3 maps to hash_map per retail. Wire NonMatching. Pin
`[≈0x82631350, ≈0x82632C54]` (verify via Ghidra). expected +12–20. attribution_risk:
gap is multi-class (GamePanel/CharMeshHide/LicenseMgr slivers precede the cluster).

### WI-3 — AccomplishmentProgress hash_map-iterate body-ports (header ALREADY converted; body grind)
The AccProg header is ALREADY hash_map (verified: src/.../AccomplishmentProgress.h
lines 9/166-175/218/264-267 declare std::hash_map). So this is a BODY-port, not a
type fix. CAVEAT: retail `Poll` (140B, 1.14%) differs from Wii's empty `Poll(){}`,
and `ClearNewRecords` has no same-named Wii body — so these are retail-REDERIVE
(cross-reference the Wii `FakeFill` map-iterate idiom at lines 484-516 as the
pattern, but the exact body must be reconstructed from retail asm). IsUploadDirty
(71.43%) is a known dtk TARGET_BOUNDARY divergence (roadmap D1, jeff-side, NOT a
source fix). expected +1–2, MEDIUM risk (retail-rederive, oracle is only a pattern).

### WI-4 — FixedSizeSaveableStream residual body-ports + GetID pin-extend
Finish ctor (55.65%) / LoadTable (10%) / AddSymbol (0%); separately the
own-unit `GetID` at 0x82786420 sits just before the pin — extend the FSSS pin back
to 0x82786420 + map-entry (attribution_risk: 0x827863E8↓ may be SongMetadata/base).
expected +1–4.

### WI-5 — DataUtl/ThreeDSoundManager-gap symbol-key cluster (scout-then-pin)
3 Symbol-key accessors (this+0x6c/+0x34/+0x50) in the [DataUtl→ThreeDSoundManager]
gap — an unidentified class with ≥3 Symbol-keyed hash_maps. Identify owner, port,
pin. expected +3–8. Recon-gated.

### WI-6 (follow-up, not yet actionable) — 3rd find-COMDAT 82B23238 owner scout
Enumerate callers of 82B23238 + identify the 82B2xxxx-band owner class. No oracle
confirmed yet — pure scout. expected unknown.

---

## Bottom line

The hash_map vein is **NOT exhausted**. The "4-for-4 / mirage" verdict was a
scoping artifact: it scanned 2 COMDAT addresses against 5 pre-pinned labels and
never enumerated the 75 unconverted find call-sites, the 23-accessor `this+0x38`
cluster α, or the 3rd distinct find-COMDAT (82B23238). Two of the three checks the
prior scout *itself* deferred (its own "Bonus" 825B8 cluster; the find-only blind
spot for iterate-only members) turn out to be live. Re-open A1 from EXHAUSTED to
ACTIVE-with-recon-gated-leads.
