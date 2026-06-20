# W9 L6 — SongStatusMgr residual: REVEAL-dominated, not bodyport

**Date:** 2026-06-20
**Mode:** ADVERSARIAL DISCOVER/PLANNER (Opus, layer 6), read-only in main @ 812e1df (8314 matched).
**Frontier:** `songstatusmgr-residual-bodyports` (kind=bodyport, est +12).
**Verdict:** **REAL_ACTIONABLE** — but the frontier's framing ("0% unported
accessor/save/load **bodies**") is **REFUTED in mechanism**. The bodies ARE ported
(958-line .cpp, 101 methods, all out-of-line in our obj). The 37 non-100 are dominated
by **MISSING target_symbol_map entries (reveal), not missing bodies**. Hard COFF
byte-diff proves **≥10 of the 37 are byte-EXACT right now** and become +N the instant a
map entry pairs them. The "+12" is achievable but ~10 of it is a **reveal sweep**, 1-2
trivial constant fixes, and only a handful are genuine bodyports.

---

## Critical context — the +34 base land is NOT on main

- The SongStatusMgr pin/wire/port/+34 lives on branch
  `w9-land-songstatusmgr-hashmap-pin-evict` @ **5edc67f** (built worktree at
  `/home/free/code/milohax/wt-w9-land-songstatusmgr-hashmap-pin-evict`, report = **8348**).
  `c1fd97e` is an earlier twin on `w9-songstatusmgr-port-convert-pin-evict`.
- **main @ 812e1df has NO SongStatusMgr.cpp, no objects.json entry, no splits pin.**
  Verified: `git merge-base --is-ancestor 5edc67f HEAD` → NOT ancestor.
- The frontier's "Worktree report default/SongStatusMgr: 71/34/37" is read off the BRANCH
  (8348), not main. **Therefore the SELF-CONTAINED RULE bites hard:** any residual
  work-item that must "land independently vs main@8314" has to carry the ENTIRE base
  land (port + hash_map re-layout + wire + pin + MoggClip-sliver evict + the 34 map
  entries) AND its own residual gains, in one worktree. You cannot land a residual delta
  alone — there is nothing on main to deepen.
- Cleanest packaging (and what the actionable items below assume): **rebase 5edc67f onto
  main, ADD the reveal map entries + trivial fixes, land the whole thing as one +44-ish
  unit.** The base +34 is already verified single-unit/zero-regression per its commit msg.

## Ground-truth method (all from the built branch worktree)

- Target obj: `build/45410914/obj/SongStatusMgr.obj` (dtk-split, big-endian PPC COFF,
  GNU/LLVM can't parse → hand-parsed). 71 functions in `.text`, span
  **[0x825B8058, 0x825BA440) = 9192 bytes**. 34 mangled (the matched accessors) + 37
  `fn_<addr>`.
- Our obj: `build/45410914/src/band3/meta_band/SongStatusMgr.obj` (LE COFF, 2973 syms).
  Total own `.text` = **9192 bytes = exact target span**. Emits the full class
  out-of-line: ctor/dtor, Clear, CalculateTotal{Score,Stars}, UpdateSong(Stats),
  Save/LoadFixed, OnMsg, PopulatePlayerScore, every Get*/Set*/Update*, all SongStatus +
  SongStatusData members, plus the STLport hash_map<int,SongStatus*> instantiations.
- The 37 target `fn_` are **all UNMAPPED** in `scripts/target_symbol_map.json` (branch).
  The 34 mapped = exactly the 34 that match. So the +34 land mapped only the cheap
  find-accessors. **Reveal pairing requires adding map entries** (objdiff cannot pair our
  mangled symbol to a target `fn_<addr>` — confirmed: `run_objdiff ?Clear@SongStatusMgr…`
  → "Symbol not found", because it only exists our side).

## Hard byte-diff (size-bucket + raw COFF byte compare, our method ↔ target fn)

**BYTE-EXACT (0 diffs, raw) — 10 free reveals (just add map entry):**

| target fn (size) | our method | note |
|---|---|---|
| fn_825BA438 (8)  | `??_ESongStatusMgr@@WCI@AAPAXI@Z` | adj-this dtor thunk |
| fn_825B81D0 (76) | `??0SongStatus@@QAA@XZ` | ctor (2 vtbl ptrs, MI) |
| fn_825B8058 (92) | `?GetTotalSongs@SongStatusMgr@@QBAHW4ScoreType@@VSymbol@@@Z` | |
| fn_825B98C0 (100)| `?GetBestSongStatusFlag@SongStatusMgr@@QBAHVSymbol@@…` | |
| fn_825B9928 (100)| `?UpdateCachedTotalScore@SongStatusMgr@@QAAXW4ScoreType@@@Z` | |
| fn_825B9E30 (148)| `?Clear@SongStatusMgr@@QAAXXZ` | |
| fn_825B9638 (168)| `?GetSongStatusFlag@SongStatusMgr@@QBA_NVSymbol@@…` | |
| fn_825B8280 (200)| `?SaveToStream@SongStatusData@@QBAXAAVBinStream@@W4ScoreType@@@Z` | |
| fn_825B8540 (300)| `?LoadFixed@SongStatus@@UAAXAAVFixedSizeSaveableStream@@H@Z` | |
| fn_825B83D8 (356)| `?SaveFixed@SongStatus@@UBAXAAVFixedSizeSaveableStream@@@Z` | |

NB raw byte-diff INCLUDES reloc slots (objdiff normalizes those away), so the true
normalized count of free reveals is **≥10** — some "near" rows below may also normalize
to 100. Validate each by adding its map entry and reading report-normalized.

**TRIVIAL CONSTANT FIX — +1 (maybe cascade):**

- fn_825B8670 (48) ↔ `?GetPossibleStars@SongStatusMgr` @ raw 91.7%. The ONLY two
  word-diffs are immediates: target `cmpwi 0x3A98` / `li 0x3A98` (15000) vs ours
  `0x1388` (5000). Our source caps per-song at 5000; **retail caps at 15000**. One-line
  data fix. Check CalculateTotalScore / GetTotalBestStars for the same 5000→15000 cap
  (they may share it — potential small cascade).

**GENUINE BODYPORT (real logic/divergence, port from oracle):**

- `?SetProGuitarSongLessonComplete@…` — MAPPED, reads **30.0%**, 7-insert: target does
  `ori r10,r10,0x8 ; stb r10, 0x184(r11)` (lesson-mask byte write) our body omits. The
  ProGuitar/Bass/Keyboard SongLessonComplete trio likely share the bug.
- `?SetProKeyboardSongLessonSectionComplete@…` — MAPPED, reads **0.0%**, 12-insert (the
  bitmap-set tail). Section-complete trio.
- fn_825B8FB0 (228) ↔ `?CalculateTotalScore@SongStatusMgr` raw 31.6% — real divergence
  (iterate + dynamic_cast<BandSongMetadata*> + SourceSym + caps). Port carefully vs
  rb3-Wii; cross-check the cap constant.
- Larger un-size-matched target fns (UpdateSong 556, UpdateSongStats 480, OnMsg 172,
  PopulatePlayerScore 116, LoadFixed-Mgr 184) — our compiler emitted different sizes ⇒
  real bodyports, per-fn grind, defer to a second wave after the reveal.

**PERMUTER / ICF (defer):**

- `?IsSongPlayed@…` @ **96.9%** — BOOL_MASK permuter-class (`subfe` reg + trailing
  `clrlwi r3,r11,24` delete). `/permute` or hand bool-mask edit.
- fn_825B9F80, fn_825B9E08 (40 each) @ 99.9/99.8 — deleting-dtor funclets: target calls
  `OggFree`, ours calls `??3@YAXPAX@Z` (operator delete). ICF-alias / custom-allocator
  fold, 1 mismatch each. Permuter-immune; needs an icf_aliases entry or accept at_limit.

## Yield estimate (honest)

Base land (already verified) **+34**. Residual realistically: **+10 reveal**
(byte-exact map entries) **+1 GetPossibleStars** **+~3-5 bodyport** (SetProGuitar trio,
SetProKeyboardSection trio share fixes; CalculateTotalScore) once ported. Defer 3
permuter/ICF. So the L6 residual is **≈ +12-16 on top of the +34**, dominated by reveal.
Frontier est +12 is right in magnitude, wrong in mechanism.

## Adjacent leads discovered (seed later layers)

- **map-coverage reveal-audit TOOL**: this whole finding (34 mapped = 34 matched, 37
  unmapped where ≥10 are byte-exact) is a GENERIC pattern for every "+N pin-evict" land
  that mapped only the cheap accessors. A tool that, for any wired+pinned unit, byte-diffs
  every unmapped target `fn_` against our same-size own methods and emits the byte-exact
  ones as a reveal worklist would auto-harvest these across ALL recent W9 port-then-pin
  lands (BandSongMgr, SongSortMgr, SongUpgradeMgr, LicenseMgr, FSSS, …). High EV.
- **SongUpgradeMgr / BandSongMgr / SongSortMgr** neighbours (per branch list +
  cluster-alpha dossier) are sibling hash_map/port-then-pin lands in flight — apply the
  same reveal-audit the moment they land.
- **OggFree-vs-operator-delete ICF** (fn_825B9F80/E08): recurring deleting-dtor funclet
  mismatch where retail folded `operator delete` into `OggFree`. Likely a binary-wide
  icf_aliases lever, not a SongStatusMgr one-off.
- **5000→15000 star/score cap**: verify whether this is a DC3-vs-RB3 constant the rb3-Wii
  oracle already has right (the .cpp on the branch may have been ported from DC3 which
  uses a different cap). If systemic, a sweep target.
