# W13 — SavedSetlist RETRY: header overload already landed → clean self-contained port

**Date:** 2026-06-20 (wave-13)
**Mode:** DISCOVER/PLANNER (Opus), READ-ONLY in main.
**Baseline:** main @ c00664c, **9404 matched** (fixed for all agents).
**TU owned:** `band3/meta_band/SavedSetlist.cpp`
**Verdict:** **REAL_ACTIONABLE.** Identity unchanged from the wave-12 dossier
(string-fingerprint + ICF-alias + caller-triangulated to near-certainty). The
ONE thing that made wave-12 net +0 — the missing 4-arg `SaveStd`/`LoadStd`
`vector<T,Allocator>` template overloads in `FixedSizeSaveable.h` — **is already
in-tree at HEAD** (landed by commit `e49d006` AccomplishmentProgress, lines
94–137). So this retry is a **fully self-contained, zero-shared-header-edit** TU
port. `flag_foundational = false`. **~+12.**

---

## What changed since wave-12 (the +0 cause is GONE)

Wave-12 deferred this TU with the note *"needs additive FixedSizeSaveable.h 4-arg
SaveStd/LoadStd vector template overloads — flagged foundational… didn't land
this pass."* That premise is now **stale**:

- `git log -- src/system/meta/FixedSizeSaveable.h` → the 4-arg templated
  overloads landed in **`e49d006` ("AccomplishmentProgress: hash_map container +
  GamerAwardStatus 360 tail (+9 @100%)")**.
- `git show HEAD:src/system/meta/FixedSizeSaveable.h` confirms at HEAD `c00664c`:
  - L94–116 `template <class T, class Allocator> static void SaveStd(FixedSizeSaveableStream&, const std::vector<T,Allocator>&, int maxsize, int savesize)`
  - L118–137 `template <class T, class Allocator> static void LoadStd(FixedSizeSaveableStream&, std::vector<T,Allocator>&, int maxsize, int savesize)`
  - L139–141 in-tree comment explicitly notes these 4-arg overloads are already
    used (by AccomplishmentProgress).
- The oracle bodies (`../rb3/src/system/meta/FixedSizeSaveable.h` L83–125) are
  **byte-equivalent** to the in-tree ones — same `vecsize`/`maxsize` clamp, same
  `PadStream`/`DepadStream` tails. `SaveFixed`/`LoadFixed` in the oracle `.cpp`
  call `SaveStd(fs, mSongs, 100, 4)` / `LoadStd(fs, mSongs, 100, 4)` with
  `mSongs` = `std::vector<int>`; these resolve to the in-tree templates verbatim.

**Net effect:** the prompt's instruction to "ADD the two 4-arg template overloads
to FixedSizeSaveable.h" is **already satisfied — DO NOT re-add them** (would be a
no-op at best, a duplicate-definition error at worst). There is **no shared-header
edit** in this work-item. The whole-binary A/B is still the honesty gate (always),
but there is no foundational lever to schedule keystone-first. This is the single
biggest correction vs the prior plan and it de-risks the item to "plain TU port."

---

## The gap (re-verified at HEAD c00664c)

```
Campaign.cpp      .text 0x8258C3D8–0x82590C70   .pdata 0x822193C0–0x822198D0   (lower neighbour, splits L2535)
[GAP]             .text 0x82590C70–0x82592270   .pdata auto-derived            ← SavedSetlist
LockStepMgr.cpp   .text 0x82592270–~0x82595540  (UNPINNED, UNWIRED)            (upper neighbour, out of scope)
MiniGameMgr.cpp   .text 0x82595540…             (next PINNED unit, splits L1338)
```

Gap = **0x1600 bytes / ~65 anonymous `fn_`** in the `band3/meta_band` spatial
cluster. Re-confirmed:
- Campaign `.text` ends **exactly** at `0x82590C70` (splits L2537) → lower edge
  adjacent, no overlap.
- **No `LockStepMgr` split exists** (`grep LockStepMgr splits.txt` = none); it is
  unpinned/unwired so `0x82592270` as `end` collides with nothing. The next
  pinned unit is `MiniGameMgr.cpp` at `0x82595540`, well above.
- **Overlap self-check (parsed ALL splits `.text` stanzas):** `[0x82590C70,
  0x82592270)` overlaps **0** existing ranges. (Verified programmatically this
  pass.)

---

## Identity ground-truth (unchanged, near-certain — see wave-12 dossier for full detail)

`docs/decomp/research/2026-06-20-w12-SavedSetlist.md` carries the full evidence;
re-summarised + re-verified:

1. **Per-function string fingerprints** (fingerprints.json): `fn_82590C70` →
   `battle_friend`/`battle_friend_archived`/`setlist_local` = `SetlistTypeToSym`
   (first fn, abuts Campaign end); `fn_82591090` → `hmx_%s` =
   `NetSavedSetlist::GetIdentifyingToken`; `fn_825912A8` → `set_description`/
   `set_shared` = the `BEGIN_HANDLERS(LocalSavedSetlist)` dispatcher. Across all
   of `../rb3/src`, the `fnd_%s`/`hmx_%s`/`btl_%i` + `setlist_*`/`battle_*` set
   co-occur in **exactly one file** = `SavedSetlist.cpp`.
2. **ICF-alias map entry** `0x82592060 → ?AdvanceSong@MetaPerformer@@UAAXH@Z` =
   `SavedSetlist::AddSong` (push int onto `mSongs` @ this+0x10). Re-confirmed
   **still present** at `target_symbol_map.json` L10275 and the **only** map key
   in the span. Preserve verbatim (lesson #4).
3. **Cross-TU caller triangulation:** `fn_825921F0` (gap tail) is called from
   `0x825B71A0` inside the landed MusicLibraryNetSetlists pin
   (0x825B6808–0x825B7C0C), whose `ParseDataResultsIntoSetlists` constructs
   `NetSavedSetlist`/`BattleSavedSetlist`.
4. **Oracle exists, DC3 false-friend:** `../rb3/src/band3/meta_band/SavedSetlist.
   {cpp,h}` present; no DC3 equivalent. In-tree `src/band3/meta_band/SavedSetlist.h`
   is **byte-identical** to the Wii oracle header (`diff` = identical, re-verified)
   — pulled in by the MLNS port. Only the `.cpp` is missing + unwired.

Report-side cross-check: `report.json` enumerates **0** functions in
`[0x82590C70, 0x82592270)` — exactly the expected state for an unpinned gap whose
fns live anonymously in the auto-blob. The +12 is *new* coverage from pinning.

---

## Dependency audit (every include resolves in-tree — re-verified this pass)

| oracle `.cpp` include | resolves to | status |
|---|---|---|
| `meta/FixedSizeSaveable.h` | `src/system/meta/FixedSizeSaveable.h` (has 4-arg SaveStd/LoadStd, REPORT_SIZE @L430, sSaveVersion) | OK |
| `meta_band/BandSongMetadata.h` | `src/band3/meta_band/…` (`TheSongMgr.Data`, `LengthMs`) | OK |
| `meta_band/BandSongMgr.h` | `src/band3/meta_band/…` | OK |
| `meta_band/UIEventMgr.h` | `src/band3/meta_band/…` (`TheUIEventMgr->TriggerEvent`) | OK |
| `net_band/RockCentralMsgs.h` | **`src/band3/net_band/RockCentralMsgs.h`** (via `/I src/band3`, NOT `src/network/` — wave-12 path note was off; resolves fine) | OK |
| `obj/ObjMacros.h` | `src/system/obj/…` (BEGIN_HANDLERS/HANDLE_*) | OK |
| `os/DateTime.h` | `src/system/os/…` (`ToCode`, `DateTime(unsigned)`) | OK |
| `os/OnlineID.h` | `src/system/os/…` | OK |
| `rndobj/Tex.h` | `src/system/rndobj/…` | OK |
| `tour/TourSavable.h` | `src/band3/tour/…` (`UploadComplete`/`SecBetweenUploads`) | OK |
| `utl/HxGuid.h` | `src/system/utl/…` (`Generate`/`ToString`/`SaveSize`; BinStream `<<`/`>>`) | OK |
| `bandobj/PatchDir.h` (via header) | **`src/system/bandobj/PatchDir.h`** (`PatchDescriptor{patchType,patchIndex}`) | OK |
| `meta_band/BandProfile.h` (via header) | `src/band3/meta_band/…` (`GetTexAtPatchIndex`/`GetName`) | OK |
| `game/Defines.h` (via header → `ScoreType`) | `src/band3/game/Defines.h` | OK |

Symbols verified declared: all 8 enum syms (`setlist_internal/local/friend/
harmonix`, `battle_harmonix[_archived]`, `battle_friend[_archived]`) + `error_message`
+ `error_setlist_title_profane`/`error_setlist_description_profane` (Symbols2.h
L1387/L1389) + `gNullStr` (Symbol.h). Macros `FOREACH`/`MakeString`/`MILO_ASSERT`/
`MILO_ASSERT_RANGE`/`MILO_FAIL`/`REPORT_SIZE` all present.

---

## Pin (fully bounded, overlap-checked)

```
band3/meta_band/SavedSetlist.cpp:
	.text       start:0x82590C70 end:0x82592270
```

- Pin `.text` ONLY; let dtk **auto-derive `.pdata`** (splits-bootstrap recipe).
  Derived pdata starts at `0x822198D0` (= Campaign pdata end, adjacent below).
- Use the **full-path key** form (`band3/meta_band/SavedSetlist.cpp:`) matching
  the Campaign / MusicLibraryNetSetlists stanzas (splits L2535 / L1338 convention).
- **Mandatory pre-land self-check (lesson #3):** after the pdata back-fill, re-parse
  ALL `.text`+`.pdata` ranges and assert zero pairwise overlaps (the derived pdata
  must not overlap Campaign's `0x822193C0–0x822198D0` nor LockStepMgr's pdata).

---

## Self-contained port plan (ONE worktree, independently landable vs main@9404)

1. **Add the source.** Copy `../rb3/src/band3/meta_band/SavedSetlist.cpp` →
   `src/band3/meta_band/SavedSetlist.cpp` (201 lines). The header is already
   present and byte-identical — **do NOT touch SavedSetlist.h**.
2. **DO NOTHING to FixedSizeSaveable.h.** The 4-arg `SaveStd`/`LoadStd`
   `vector<T,Allocator>` overloads the oracle needs are **already in-tree**
   (HEAD, L94–137). Re-adding them would duplicate-define. This is the deviation
   from the wave-12 plan and the reason this retry is foundational-free.
3. **Wire** in `config/45410914/objects.json`, in the `meta_band` block
   (alongside `band3/meta_band/Campaign.cpp` @ L698):
   `"band3/meta_band/SavedSetlist.cpp": "NonMatching",`
4. **Pin** the splits stanza above into `config/45410914/splits.txt`. Run the
   overlap self-check.
5. **Map** — run `tools/gen_game_target_map.py` for this TU; **ADD** its emitted
   keys to `scripts/target_symbol_map.json`, **skipping the pre-existing
   `0x82592060`** (the AddSong ICF alias). Never re-serialize the map wholesale
   (lesson #4 poison rule).
6. **Port MWCC→MSVC X360.** The oracle `.cpp` is clean and already uses the
   project macros. Watchpoints:
   - `MILO_ASSERT`/`MILO_FAIL` message strings are gated/stripped to match retail
     (Debug.h handles this; preserve line numbers `0x4D`/`0x91`/`0x107`/`0x10B`/
     `0xFE`/`0x12F`).
   - `static Message msg("init", 0)` in `ProcessRetCode` is a function-local
     static → emits a guard (`??_B`)/dynamic-init (`??__E`) thunk; the wired
     post-compile patchers (`guard`/`dynamic_init`) normalise these — expect them
     to pair, don't hand-fight.
   - `FixedSizeSaveableStream` derives from `BinStream`, so `fs << mIsShared` /
     `fs >> dtCode` resolve through the BinStream `<<`/`>>` overloads.
7. **Convert + reveal + A/B.** Build with the VERIFY COMMAND
   (`rm -f build/45410914/target_symbol_renames.stamp && touch
   config/45410914/config.yml && NINJA_JOBS=8 tools/fresh_report.sh`), read
   `measures.matched_functions`; **re-run once** for the splits-only FP warning.
   Iterate per-fn body-port from the oracle until framed/real-bodied fns hit
   normalized 100%.

---

## Expected delta

**~+12** honest. 65 emitted fns, 1 already counted (AddSong ICF alias). The TU is
a 5-class hierarchy (SavedSetlist abstract base + Internal/Net/Battle/Local, the
last with triple inheritance SavedSetlist/TourSavable/FixedSizeSaveable), so ~44
of 65 are small (≤44B): vtable thunks, scalar-deleting dtors, dtor funclets,
trivial virtual forwarders (SetTitle/SetDescription/SetDateTime/AddSong/SetSongs
×classes), plus `vector<int>`/`vector<String>`/`vector<Symbol>` STL instantiations.
Trivial accessors + STL instantiations byte-match on reveal = the core +12. The
large real bodies — `SetlistTypeToSym` (switch), `LocalSavedSetlist::Handle`
(~1056B dispatcher), the `SaveFixed`/`LoadFixed` pair, `NetSavedSetlist::
GetIdentifyingToken` (switch + MakeString) — may land near-miss (regalloc/funclet
class) on the first pass. Honest ceiling if all trivial-virtual fan-out + STL
match: **~+20**. Land whatever is net-positive; defer regalloc/funclet near-misses
with root cause.

---

## Honesty gate notes for the lander

- whole-binary composed A/B vs main@9404; net ≥ +1 AND == sum of intended
  SavedSetlist gains. **No FixedSizeSaveable.h edge to absorb this time** (already
  landed) — so any binary-wide drift would be a genuine red flag, not expected
  noise.
- No ≥8-contiguous FOREIGN `fn_@0%` run inside the pin. The only foreign-NAMED
  entry (AdvanceSong @0x82592060) is an ICF alias to AddSong, already 100%/counted;
  its own STL/funclets/trivial virtuals bracketed by its own named fns are OK.
  No other TU owns the span (Campaign below excluded, LockStepMgr above excluded).
- Include-graph blast radius: **zero shared-header touch**. `SavedSetlist.h`
  unchanged (already in-tree, identical). `FixedSizeSaveable.h` untouched.

---

## flag_foundational

**false.** Unlike the wave-12 plan, this retry requires **no** shared-header edit
— the 4-arg `SaveStd`/`LoadStd` overloads landed in `e49d006`. The work-item is a
self-contained single-TU port (source + wire + pin + map + reveal in one worktree).
Standard whole-binary A/B applies as always, but there is no binary-wide lever for
the coordinator to schedule keystone-first.
