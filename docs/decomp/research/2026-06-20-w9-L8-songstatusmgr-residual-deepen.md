# W9 L8 — songstatusmgr-residual-deepen: REAL but base STILL NOT ON MAIN

**Date:** 2026-06-20
**Mode:** ADVERSARIAL DISCOVER/PLANNER (Opus, layer 8), READ-ONLY in main @ 812e1df (8314 matched).
**Frontier:** `songstatusmgr-residual-deepen` (kind=reveal, est +12).
**Verdict:** **REAL_ACTIONABLE**, but the frontier's load-bearing premise is **FALSE and
must be corrected before any agent picks this up.**

---

## THE CORRECTION (falsified premise)

Frontier says: *"DEEPEN the just-landed SongStatusMgr base (landed this layer +44, NOW
on main)."* **This is wrong. NOTHING SongStatusMgr is on main.** Ground truth:

- `git merge-base --is-ancestor <sha> HEAD` is **NO** for every SongStatusMgr branch.
- main @ 812e1df has **no** `src/band3/meta_band/SongStatusMgr.cpp`, **no** objects.json
  entry, **no** splits.txt pin. Verified by file/grep.
- The **+44 lives on a BRANCH**: `w9-land-songstatusmgr-base-plus-reveal` @ **bd9705b**
  (== `w9-songstatusmgr-base-land-plus-reveal`, same SHA). Built worktree at
  `/home/free/code/milohax/wt-w9-land-songstatusmgr-base-plus-reveal`,
  **report.json = 8358 (= +44 vs main@8314)**, SongStatusMgr unit **44/71 matched**.
  - `bd9705b` = base (+34, 5edc67f) + "reveal 10 byte-exact + 15000 star-cap (+10)".
  - `5edc67f` (parent) = "hash_map<int,SongStatus*> re-layout + find-accessor port; evict
    dead MoggClip orphan pin (+34)". `c1fd97e` is an earlier twin of the base.

**Consequence (SELF-CONTAINED RULE bites hard):** there is nothing on main to deepen.
Any residual work-item must carry the **entire bd9705b content** (port + hash_map
re-layout @ this+0x38 + MoggClip-sliver eviction + the pin + the 44 map entries + the
star-cap fix) in ONE worktree, AND add its own residual gains, to land independently vs
main@8314. Cleanest packaging: **rebase bd9705b onto main, add the residual on top, land
the whole +44+residual as one unit.** The +44 is already verified single-unit /
zero-regression per the L5/L6 dossiers and the worktree's report.

---

## Residual ground-truth (what bd9705b LEFT on the table)

bd9705b did the 10 L6 reveals AND applied the 5000→15000 star-cap .cpp fix in 3 methods
(GetTotalBestStars, CalculateTotalStars, GetPossibleStars — verified in
`git diff 5edc67f bd9705b -- src/band3/meta_band/SongStatusMgr.cpp`). BUT it **did not
map the star-cap trio's target VAs.** Remaining harvest:

### 1. GetPossibleStars reveal — CONFIRMED byte-exact free +1

- Target `fn_825B8670` (size 0x30) is **UNMAPPED** on bd9705b
  (`scripts/target_symbol_map.json` has no 0x825B8670 key).
- Hand byte-compare (our compiled obj `…/SongStatusMgr.obj` vs target `.s`): identical
  except ONE word at +0x0c, which is the `bl GetTotalSongs` relocation. Our reloc at
  +0xc resolves to `?GetTotalSongs@SongStatusMgr@@…` (COFF reloc symidx 954, type 6);
  target is `bl fn_825B8058` (= GetTotalSongs). **objdiff normalizes reloc targets → 100%.**
- Both sides emit the post-fix `cmpwi 0x3a98 / li 0x3a98` (15000). **ACTION: add
  `"0x825B8670": "?GetPossibleStars@SongStatusMgr@@QBAHW4ScoreType@@VSymbol@@@Z"` to the
  map. +1, objdiff-confirm before mapping.** attribution_risk (map/reveal).

### 2. CalculateTotalStars / GetTotalBestStars — NOT reveals (real divergence, defer)

The frontier lumps these into the "byte-exact reveal-trio". **They are not byte-exact.**
- `fn_825B8FB0` CalculateTotalStars: ours 0xac (43 insns) vs target 0xe4 (57) — −14 insns.
- `fn_825B9098` GetTotalBestStars: ours 0xec (59 insns) vs target 0xcc (51) — +8 insns.
Both emit 0x3a98 (cap fix applied) but the **map-iteration body differs in size** = real
codegen divergence (the begin/end iteration + per-song cap accumulation). These are
genuine bodyports (per-fn objdiff grind vs rb3-Wii), NOT free reveals. Defer to a focused
bodyport wave. (L6 also flagged CalculateTotalScore raw 31.6% — same family.)

### 3. SongLessonComplete bodyports — BLOCKED on inlined-callee codegen (deeper)

Report on bd9705b: `SetProGuitarSongLessonComplete` **30%**,
`SetProKeyboardSongLessonSectionComplete` **0%** (others in the trio similar). **The .cpp
bodies are ALREADY exact copies of the rb3-Wii oracle** (`../rb3/.../SongStatusMgr.cpp:898`
== bd9705b line 750, byte-for-byte). So the divergence is NOT a missing source line —
it's that the **inlined callees** (`SongStatus::SetFlag` mask-write
`ori r10,r10,0x8 ; stb r10,0x184(r11)` that L6 saw, and
`SongStatus::SetProGuitarLessonSectionComplete`) emit different code, i.e. the SongStatus
flag-storage layout / SetFlag body is not yet retail-exact. This is a SongStatus-internal
fix, deeper than a one-liner; defer as a focused SongStatus-SetFlag bodyport (may +3-5 if
SetFlag is cracked, cascading across the Set*Complete trio + Section trio).

### 4. Near-100 permuter/ICF residue (defer, per L6)

- `?GetAwesomes@…` 99.97, `?IsSongPlayed@…` 96.91 (BOOL_MASK permuter-class).
- `fn_825B9F80` 99.90 / `fn_825B9E08` 99.80 — deleting-dtor funclets; target calls
  `OggFree`, ours `operator delete` (ICF-alias, 1 mismatch each). icf_aliases entry or
  accept at_limit. Likely a binary-wide lever, not SongStatusMgr-specific.

---

## Honest residual yield (on top of the +44 base, ALL in one rebased worktree)

| item | yield | class |
|---|---|---|
| GetPossibleStars reveal map entry | **+1** | confirmed byte-exact reveal |
| SongLessonComplete trio + Section trio bodyport (via SongStatus::SetFlag) | +0–5 | blocked on SetFlag inlined codegen; tractable but a real grind |
| CalculateTotalStars / GetTotalBestStars / CalculateTotalScore | +0–3 | real map-iter bodyport, per-fn |
| GetAwesomes / IsSongPlayed permuter | +0–2 | permuter-class, /permute |
| OggFree-vs-delete dtor funclets | +0–2 | ICF-alias (binary-wide, not here) |

So the **+12 frontier estimate is over-optimistic for what's left after bd9705b**: the
guaranteed deepen is **+1 (GetPossibleStars)**; the rest is a bodyport grind gated on
SongStatus::SetFlag (+0–5 realistic) plus permuter/ICF (defer). The big "+12" the frontier
imagined was MOSTLY ALREADY HARVESTED by bd9705b (the 10 reveals + star cap). The
remaining clean win is small.

## Adjacent leads discovered (seed later layers)

- **map-coverage reveal-audit TOOL** (re-confirmed from L6): a generic tool that, for any
  wired+pinned unit, reloc-aware byte-diffs every UNMAPPED target `fn_` against same-size
  own methods and emits byte-exact ones as a reveal worklist. My ad-hoc version
  (`/tmp/reveal_audit.py`) found GetPossibleStars on bd9705b. Productionize + run across
  every W9 port-then-pin land (BandSongMgr, SongSortMgr, SongUpgradeMgr, LicenseMgr, FSSS).
- **SongStatus::SetFlag / lesson-bitmap layout** is the keystone for the Set*Complete
  family (5–6 fns at 0%/30%). Crack the SongStatus flag-storage offset+SetFlag body once →
  cascade the Guitar/Bass/Keyboard Complete + Section trios. Cross-check SongStatus layout
  vs rb3-Wii `SongStatus`/`SongStatusData`.
- **OggFree-vs-operator-delete ICF** (fn_825B9F80/E08): recurring deleting-dtor funclet
  where retail folded `operator delete` → `OggFree`. Likely binary-wide icf_aliases lever.
- **0x825BA440→0x825BB090 pin-extension seam** (from L5): unresolved band above the pin;
  re-probe empirically once bd9705b lands and objdiff can A/B an extension.
