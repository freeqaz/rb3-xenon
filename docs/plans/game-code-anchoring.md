# Game-Code Anchoring — type-name-string + vtable/RTTI (2026-05-29)

> **Read-only investigation.** No `ninja`, no `splits.txt`/`objects.json` edits,
> no commit. Goal: find a surviving identification signal for the **38
> wired-but-unpinned `src/band3/meta_band/` TUs** (the orchestrator's "30 have
> zero bindiff/autoid coverage" problem), using methods *beyond* the
> DC3-aliased bindiff oracle. All addresses verified live against
> `orig/45410914/band.exe` (the decompressed retail PE) and
> `config/45410914/symbols.txt`.

## TL;DR verdict

**Identification: partial WIN. Span derivation: NEGATIVE (loose-hull risk
confirmed).**

- **Signal (a) MILO_FAIL / MILO_ASSERT path strings — DEAD.** Confirmed
  empirically stripped (0 hits for every distinctive condition message).
- **Signal (a′) DataArray HANDLE_* reflection symbols — SURVIVE, and anchor a
  net-new function the bindiff oracle missed.** 11 of 38 TUs get a precise,
  *zero-prior-coverage* anchor function this way (mostly `Class::Handle`).
- **Signal (b) RTTI `.?AV<Class>@@` type descriptors — SURVIVE (`/GR` is on).
  36 of 38 TUs have full RTTI.** The TD→COL→vtable chain walks cleanly and
  *confirms the class exists and its region*. **BUT** the vtable slot functions
  are a *poor span locator* — validated against 4 already-pinned meta_band TUs,
  only 0–2 of ~9 slots land inside the known span. Slots scatter into base TUs,
  thunk pools, and ICF-folded shared destructors.
- **Tight per-TU `.text` spans cannot be derived** for these classes: the retail
  layout interleaves many TUs at fine granularity within each region (verified:
  the entire Accomplishment-conditional family's destructors are intermixed
  across a single 25 KB / 284-fn region, with `nuiapi`, `nuispeech`, `meta`
  TUs interleaved). Any per-TU pin = a loose hull that collides — exactly the
  failure mode `next-levers-2026-05-29.md` warns about.

**Net deliverable: 11 net-new *single-function* anchor pins** (the `Class::Handle`
methods + a few distinctive methods), each a 1-function `.text` span that pins
**only the anchor function**, not a hull. These are safe (no collision) and each
adds the one identified function. Multi-function spans are NOT recommended.

**Recommendation to the orchestrator:** the type-name/RTTI method is a genuine
*identification* advance over bindiff (it found 11 functions bindiff had at
0 coverage), but it does **not** unlock the "+20-60 from pinning the 38 TUs"
lever, because span derivation is blocked by fine-grained TU interleaving, not
by lack of an anchor. The realistic yield from this signal is **+~11 single-fn
pins** (and several may land 0% if the source `Handle` doesn't match yet). The
strategic game-code lever remains **RB3-Wii structural bindiff** (the designated
game-code oracle) to get *per-function* address attribution, which is what
tight spans actually need.

---

## Method & evidence

### The binary form used

`orig/45410914/band.exe` is the **decompressed retail PE** (`MZ` header,
`PE32 ... XBOX PowerPC 64-bit`). ASCII strings are directly readable
(`strings -n 4` → 41,503 lines). Section map (image base `0x82000000`):

| sec | VA | file rawptr | vsize |
|---|---|---|---|
| .rdata | 0x82000400 | 0x400 | 0x1e95ac |
| .text | 0x82260000 | 0x25b800 | 0x9b48d4 |
| .data | 0x82c34400 | 0xc20400 | 0x1f35ec |

For `.rdata`/`.data`, VA = `0x82000000 + file_offset` (vaddr==rawptr==0x400 etc.),
so any `strings`/`grep -bo` byte offset maps directly to a VA.

### Signal (a): MILO_FAIL / path strings are STRIPPED (confirmed)

Extracted every `MILO_FAIL`/`MILO_WARN` literal from the 38 TUs and grepped the
binary. **Zero survive:**

```
[0] "invalid type of node comparison"          (SongSortBy*)
[0] "Bad RankType in SongSortByRank"
[0] "vocals or harmony"                          (Accomplishment*Conditional)
[0] "Condition is not currently supported"
[0] "accomplishment category already exists"     (AccomplishmentManager)
[0] "Couldn't find a letter"                      (SongSortByArtist)
```

Sanity check that ASCII *is* readable: known-present DataArray symbols
(`cheat_display`, `is_loading_stickers`, `BandCrowdMeterDir`) all return 1 hit.
So the absence is real stripping, not an encoding artifact. This kills the
"MILO_FAIL message" sub-signal for **all** these TUs and explains why ~27 of
them have *no* distinctive plain-string content at all (their only non-path
literals were MILO_FAIL messages).

### Signal (a′): DataArray HANDLE_* reflection symbols SURVIVE

`HANDLE_EXPR(sym,…)` / `HANDLE_ACTION(sym,…)` expand (via
`_NEW_STATIC_SYMBOL(s)` → `static Symbol _s(#str)`,
`src/system/obj/Object.h:754`) to a **literal symbol-name string** baked into
the class's single `Class::Handle` function. These survive as ordinary
`.rdata`/`.data` C strings. Verified all present:

```
[1] earn_accomplishment   [1] get_award_description   [1] trigger_event
[1] current_dialog_event  [1] can_launch_goal         [1] get_random_name
```

**`fingerprints.json` is too sparse to use as the index** (only 4,164/66,838
functions carry any string; it misses most of these). Root cause: dtk emitted
**one** `_rdata.s` (whole `.rdata` is in `auto_00_82000400_rdata.s`) but only
labels a string as `.string "…"` when a `.text` ref resolved to it; many string
pointers land in raw `.byte` blobs that `fingerprint_match.parse_rdata_strings`
(matches only `^\s*\.string "…"`) skips.

**Built a complete string→.text xref directly from the binary bytes**
(`/tmp/strxref.py`): harvest every NUL-terminated C string in `.rdata`+`.data`
(19,536 strings), linearly decode `.text` PPC `lis/addi`, `lis/ori`, and
`lis`+D-form-load pairs, map computed VA → string, map the load instruction's
VA → containing function via `symbols.txt`. Result: **19,270 string loads,
5,541 functions referencing ≥1 string** (vs fingerprints' 4,164) — and it
resolves every handler symbol fingerprints missed.

### Signal (b): RTTI survives — `/GR` is ON

`strings band.exe | grep '.?A[VU]'` → **1,396 MSVC RTTI type descriptors**
(`.?AVString@@`, `.?AVObject@Hmx@@`, …). **36 of 38** TU primary classes have
their `.?AV<Class>@@` descriptor present (only `ContextChecker` and
`WiiBufStreamMgr` lack one — ContextChecker is anchored via strings anyway;
WiiBufStreamMgr is likely non-polymorphic).

Walked the full MSVC RTTI chain (`/tmp/rtti_chain.py`):
Type Descriptor (name string − 8) → Complete Object Locator (`??_R4`, holds
`pTypeDescriptor` at +0xC) → vtable (`??_7`, word at vtable−4 → COL) → vtable
slot function pointers → `.text` cluster. The chain resolves cleanly for all
36; every class has a vtable whose slots point into `.text`.

**But validation against the 4 already-pinned meta_band TUs shows slots are a
bad span locator:**

| pinned TU | known span | RTTI slots | slots INSIDE span |
|---|---|---|---|
| CalibrationPanel | [825ED848,825EF598) | 10 | **2** |
| MusicLibrary | [82528C50,82529D64) | 9 | **1** |
| OvershellSlot | [825C10D8,825C3A44) | 2 | **0** |
| PatchPanel | [8260DDD0,8260E290) | 9 | **0** |

Slots scatter because most virtuals are inherited (defined in base TUs) or are
thunks (the `0x827ed3xx` pool). Also confirmed **ICF noise**: `0x82465928` is a
shared deleting-destructor folded across **16** classes; `0x825a6be8` shared by
all 6 `SongSortBy*`. Filtering to ICF-unique slots helps but still doesn't bound
the TU.

---

## Per-TU anchorability (all 38)

### Tier 1 — STRONG net-new anchor (Handle method, ≥3 handler-string votes) — 4

The anchor function references *many* of the class's handler symbols → it is
unambiguously `Class::Handle`, defined in the TU. **All ZERO prior
bindiff/autoid coverage.**

| TU | anchor | evidence |
|---|---|---|
| AccomplishmentManager | `0x82548538` | refs **21** handler syms (`earn_accomplishment`, `get_award_*`, …); bindiff independently tags adjacent `0x825484b0` → `AccomplishmentManager.obj` |
| ContextChecker | `0x82555918` | refs **16** condition props (`new_ar_console`, `num_restarts_greater`, …) |
| UIEventMgr | `0x8257be58` | refs **10** event syms; bindiff tags adjacent `0x8257bdd8` → `UIEventMgr.obj:OnTriggerEvent` |
| SongSortByRank | `0x82641e18` | refs `is_percentile`,`rank`,`song_id` (the `OnMsg(RockCentralOpComplete)` body) |

### Tier 2 — net-new anchor, single distinctive string (1–2 votes) — 7

A single class-specific symbol pins one function (still zero prior coverage).

| TU | anchor | string |
|---|---|---|
| AccomplishmentProgress | `0x825796a8` | `add_award`,`get_icon_hardcore_status` |
| NameGenerator | `0x82627000` | `get_random_name` |
| UIEvent | `0x8263c098` | `allows_override` |
| CampaignLevel | `0x8263be80` | `%s_earned` |
| LicenseMgr | `0x82632050` | `licenses` (tiny `0xC` getter) |
| StoreSongSortNode | `0x826585e8` | `get_offer` |
| Accomplishment | `0x82348920` | `%s_desc` |

(For 5 of these, an ICF-unique RTTI slot lands in the *same* neighborhood —
e.g. CampaignLevel str `0x8263be80` ↔ rtti `0x8263bc10`; NameGenerator
`0x82627000` ↔ `0x826272b0` — corroborating the region.)

### Tier 3 — RTTI-only (class confirmed, slots scatter) — 21

All the Accomplishment `*Conditional` subclasses + CampaignKey,
CymbalSelectionProvider, OvershellSlotState, StandIn, UploadErrorMgr,
AccomplishmentCategory/Group/Setlist/OneShot. Each has a `.?AV…@@` descriptor
and a slot-0 deleting-destructor address, e.g.:

```
AccomplishmentDiscSongConditional   dtor 0x825cb938
AccomplishmentSetlist               dtor 0x825cbf00
AccomplishmentOneShot               dtor 0x825cc1c0
AccomplishmentPlayerConditional     dtor 0x825ce628
AccomplishmentTrainerCategoryCond   dtor 0x825ce950
AccomplishmentSongFilterConditional dtor 0x825cf978
... (all in /tmp/rtti_chain.json)
```

**Not pinnable.** These destructors all live intermixed in `0x825cb–0x825d1`
(25 KB, 284 functions). bindiff *corroborates the region's identity*
(`0x825cee08` → `AccomplishmentSongListConditional.obj`, `0x825d1240` →
`AccomplishmentSongConditional.obj`) — so we know it's the conditional family —
but a dozen different conditional TUs' functions are interleaved with each
other and with `nuiapi:randomforest.obj`, `nuispeech:srrecomaster.obj`,
`meta:FixedSizeSaveable.obj`. No individual conditional TU is contiguous.

### Tier 4 — NO usable surviving signal — 6

`SongSortByArtist`, `SongSortByDiff`, `SongSortByPlays`, `SongSortByRecent`,
`SongSortBySong` (RTTI present but all 4 slots are ICF-folded with the shared
`SongSort` base / `0x825a6be8` / destructor — no class-unique slot; their only
plain strings are stripped MILO_FAIL + include paths), and `WiiBufStreamMgr`
(no RTTI, no surviving string).

---

## Why tight spans fail (the load-bearing negative result)

1. **The anchor is correct; the *neighbors* aren't the same TU.** Around
   `AccomplishmentManager::Handle` (`0x82548538`): ~60 `0x20`-byte ICF/thunk
   stubs, then `PracticeSection.obj` (`0x825491f0`), then `HolmesClient.obj`
   (`0x82549a38`). The TU's own functions are *not* a contiguous run.
2. **"Foreign-free runs" are an illusion of sparse bindiff coverage.** Expanding
   each anchor to the nearest bindiff-attributed neighbor gives overlapping
   "runs" (UIEvent and CampaignLevel both start at `0x8263bc60`) — proving the
   boundaries are coverage gaps, not TU boundaries.
3. **ICF folds destructors across unrelated classes** (`0x82465928` in 16
   vtables), so even RTTI slot-0 is shared, not TU-local.

This is the same structural reality `next-levers-2026-05-29.md` §Q2 describes:
without **per-function** address attribution, you get loose hulls. RTTI/strings
give you *one* anchor per TU, not the *boundary pair* a span needs.

---

## Candidate single-function `.text` pins (safe, no hull)

If the orchestrator wants the +11 from this signal, pin each as a **1-function
span** (start = anchor, end = next function start, from `symbols.txt`). This
pins only the identified function and cannot collide. Caveat: each only matches
if the ported source `Class::Handle`/method already compiles to that body — some
may land 0% and should be dropped (same ICF-singleton risk that cost wave-1 its
drops).

```
AccomplishmentManager.cpp:   .text start:0x82548538 end:0x82548E5C   # ::Handle (0x924)
ContextChecker.cpp:          .text start:0x82555918 end:0x82555E60   # ::Handle (0x548)
UIEventMgr.cpp:              .text start:0x8257BE58 end:0x8257C2C4   # ::Handle (0x46C)
SongSortByRank.cpp:          .text start:0x82641E18 end:0x82641FAC   # ::OnMsg  (0x194)
AccomplishmentProgress.cpp:  .text start:0x825796A8 end:0x8257988C   # ::Handle (0x1E4)
NameGenerator.cpp:           .text start:0x82627000 end:0x82627120   # get_random_name (0x120)
StoreSongSortNode.cpp:       .text start:0x826585E8 end:0x82658788   # ::Handle? (0x1A0)
UIEvent.cpp:                 .text start:0x8263C098 end:0x8263C108   # (0x70)
CampaignLevel.cpp:           .text start:0x8263BE80 end:0x8263BEC8   # (0x48)
Accomplishment.cpp:          .text start:0x82348920 end:0x82348984   # (0x64)
LicenseMgr.cpp:              .text start:0x82632050 end:0x8263205C   # licenses getter (0xC)
```

(11 lines; `splits.txt` will back-fill matching `.pdata` on next SPLIT. Verify
each against objdiff before trusting — these are anchors, not confirmed
matches.)

## Reproduction

Scripts + outputs persisted to `build/scratch/game-anchoring/` (gitignored,
regenerable):
- `strxref.py` → string→.text-VA xref from raw bytes
- `build_idx.py` → `s2f.json`,`f2s.json` (string↔function via symbols.txt)
- `rtti_walk.py`/`rtti_chain.py` → `rtti_chain.json` (TD→COL→vtable→slots, incl.
  per-class slot-0 destructors and ICF-unique slots)
- `anchor.py` → `anchor_report.json` (per-TU string-anchor votes)
- `final_summary.json`, `uniq_slots.json` (the per-TU tiering)

Inputs: `orig/45410914/band.exe`, `config/45410914/symbols.txt`,
`unified_id.json` (bindiff oracle cross-check), `src/band3/meta_band/*.cpp`.
