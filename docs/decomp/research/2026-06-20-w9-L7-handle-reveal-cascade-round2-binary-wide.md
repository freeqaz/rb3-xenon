# W9 L7 — handle-reveal-cascade-round2-binary-wide (ADVERSARIAL DISCOVER)

**Date:** 2026-06-20  **Baseline:** main @812e1df (8314 matched)
**Verdict: REFUTED (literal frontier) → REAL_ACTIONABLE (re-scoped).**
The frontier's load-bearing claim — *"Pure map-add, builds straight on
main@812e1df ... many TUs now have a byte-exact ::Handle that only lacks a
target_symbol_map entry"* — is **FALSE on main**. Every byte-exact-Handle
premise it cites is true only on **unlanded branches** (the macro reconcile +
the SongStatusMgr base land), NOT on main@8314. The real work is **landing those
two already-built self-contained units**, not a free map-add sweep. Both already
exist as branches off main.
**Mode:** read-only in main; ground truth from COFF symbol dumps, `run_objdiff`,
`splits.txt`/`objects.json`, the L4/L5/L6 dossiers, and git branch inspection.

---

## TL;DR — what the frontier got RIGHT / WRONG

- **RIGHT (mechanism, on the right base):** a wired+pinned Handle body that is
  byte-exact with its target `fn_<VA>` only lacks a `target_symbol_map.json`
  `0xVA -> ?Handle@Class@@UAA?AVDataNode@@PAVDataArray@@_N@Z` entry to flip, and
  a wrong VA->name pair reads 0% (self-validating, never a false +). This IS the
  reveal mechanism and it IS zero-risk **once the body is byte-exact**.
- **WRONG #1 (the bodies are NOT byte-exact on main).** main@812e1df still emits
  the spurious head `MessageTimer timer(...)` (`Object.h:928/936`) AND the
  sizeof-stripped tail `MILO_NOTIFY("%s unhandled msg: %s", PathName(this), sym)`
  (`Object.h:1032`). PROOF: `?Handle@UIManager@@UAA…` IS already paired in the map
  on main yet `run_objdiff` reads **0.0% normalized — 812 insert / 497 delete**.
  A grossly divergent body, not a "just flip the map" reveal. The
  "just flipped Handle bodies binary-wide" the frontier cites is the state of the
  **spike worktree (8502) / SongStatusMgr branch (8348)**, NOT main (8314).
- **WRONG #2 (the SongStatusMgr star-cap +3 is impossible on main).** main has
  **no** SongStatusMgr.cpp, **no** objects.json entry, **no** splits.txt pin — only
  `SongStatusMgr.h`. The TU is UNWIRED. Worse, VA **0x825B8670** (named as a
  "star-cap" by the frontier) is currently owned by a **MoggClip.cpp sliver pin**
  `[0x825B8670,0x825B86A0)` — a dead misattributed sliver squatting on the
  SongStatusMgr cluster. You cannot add those 3 map entries on main; they require
  porting the 958-line .cpp + wiring + pinning + evicting the MoggClip sliver +
  the hash_map re-layout FIRST.
- **WRONG #3 (those 3 VAs are not Handle star-caps at all).** Per the L6 ground
  truth: 0x825B8670 = `?GetPossibleStars@SongStatusMgr` (91.7% near-miss, a
  5000→**15000** cap constant fix, NOT a reveal); 0x825B8FB0 =
  `?CalculateTotalScore@SongStatusMgr` (31.6%, real body divergence). Neither is a
  byte-exact `?Handle@…` star-cap. The frontier conflated the SongStatusMgr
  residual reveal sweep (10 byte-exact *non-Handle* methods) with the binary-wide
  Handle cascade.
- **WRONG #4 (the LEAD recipe is mechanically impossible).** "grep
  auto_03_*_text.obj for `?Handle@…` VAs not in map" cannot work: the COFF text
  obj (`auto_03_82260000_text.obj`, 107917 syms) carries ONLY anonymized
  `fn_<VA>` names — there are **zero** mangled `?Handle@` strings in it (verified).
  The mangled names come from the rb3-Wii/DC3 oracle + the retail vtable Handle
  slot, never from the text obj.

## Ground truth established (this investigation)

1. **map structure:** `scripts/target_symbol_map.json` is a flat dict, 12493
   entries, keyed `"0xVA" -> "mangled"`. Only **3** `?Handle@…@@UAA?AVDataNode@@
   PAVDataArray@@_N@Z` bodies are paired on main: UIManager (0x827DF8B8),
   DancerSequence (0x824841A0), GuitarController (0x82778070). The 3 SongStatusMgr
   star-cap VAs (0x825B8670/8FB0/9098) and all "proven-flip" Handles
   (UIListCustom/CharIKFoot/UIListSlot/WorldInstance) are **absent**.
2. **macro state on main (the blocker):** timer head present (`Object.h:928,936`),
   sizeof tail present (`Object.h:1032`). The Family-A path
   (`ObjMacros.h:73-79` head + `:210-214` HANDLE_CHECK tail) is likewise unmodified.
3. **UIManager::Handle = 0.0%/812-insert on main** (`run_objdiff`, project_dir=main)
   — the empirical disproof of "byte-exact, just add map entry".
4. **COFF text obj has no mangled names** — `auto_03_82260000_text.obj` symbol
   table is 100% `fn_<VA>` / `lbl_` / `except_data_`; section base 0x82260000,
   values section-relative. The frontier's grep target string never appears.
5. **SongStatusMgr unwired on main; MoggClip sliver owns 0x825B8670.**

## The two real, already-built, self-contained work-items (off main@8314)

Both are the *correct* packaging the L5/L6 dossiers prescribed, and they ALREADY
EXIST as branches. The L7 actionable is to **land them** (rebase onto current
main, whole-binary A/B, honesty gate), carrying ALL prereq+reveal in one unit.

### A. SongStatusMgr base-land + reveal (the frontier's SongStatusMgr lead, done right)
- Branch **`w9-songstatusmgr-base-land-plus-reveal` @bd9705b** (off main@8314).
- `bd9705b` "reveal 10 byte-exact methods + fix retail 15000 star-cap (+10)" on top
  of `5edc67f` "hash_map<int,SongStatus*> re-layout + find-accessor port; evict
  dead MoggClip orphan pin (+34)". Total **+44 as ONE self-contained unit**.
- Touches exactly: `objects.json` (+1 wire), `splits.txt` (pin + MoggClip evict),
  `target_symbol_map.json` (34 find-accessors + 10 reveals), `SongStatusMgr.cpp`
  (NEW 958 lines, ported), `SongStatusMgr.h`. Conflict-free, self-contained.
- This is NOT a "pure map-add" — it is port+wire+pin+evict+reveal. The frontier's
  "+3 star-cap, builds straight on main" is wrong; the reality is +44 carrying the
  whole base. The 10 reveals + the GetPossibleStars 15000 cap are the *real*
  SongStatusMgr reveal yield (NOT Handle star-caps).

### B. Family-A Handle reconcile + per-TU reveal (the binary-wide Handle cascade, Family-A half)
- Branch **`w9-family-a-reconcile-handle` @2073e3a** (off main@8314), +21.
- "Family-A Handle timer-off + HANDLE_CHECK comma-eval reconcile" — edits
  `ObjMacros.h` (head timer drop + tail comma-form) so Family-A Handle bodies
  become byte-exact, PLUS per-TU pairings for GuitarController, BandCamShot,
  VocalTrack, VocalPlayer (the reveal half) in the SAME unit.
- Independent of the Family-B `Object.h` reconcile (different header). Self-contained.

### Prereq for the BROADER binary-wide cascade (Family-B + the 100-fn wave) — NOT yet on main
- The Family-B macro reconcile (`reconcile-handle-prereq-FINAL`, branch
  `w9-reconcile-handle-prereq-FINAL` @9fb9016 / land-candidate
  `w9-land-reconcile-handle-prereq-9fb9016` @a7175af) is **NOT an ancestor of
  main**. Until it lands, the 100 unpaired END_HANDLERS-shaped wired Handle bodies
  (L4 census) are NOT byte-exact and CANNOT be revealed. The frontier's
  "binary-wide" reveal is gated on this prereq landing first.

## Verdict & sequencing

The frontier is **REFUTED as literally stated** (no free map-add reveal exists on
main; the SongStatusMgr star-cap framing is factually wrong on 4 counts). But the
underlying veins are REAL and packaged. Emitting REAL_ACTIONABLE for the two
existing self-contained branches (land A and B), and discovered_frontier for the
gated binary-wide cascade (needs the Family-B prereq first, then the pairing wave).
This is the WAYPOINT/WAVE-8 discipline: the "reveal cascade is free on main"
verdict was a hypothesis — falsified by `run_objdiff` ground truth — but the work
itself is not refuted, just re-scoped to carry its prereqs.

## Discovered frontier (adjacent leads, seed later layers)

- **Family-B reconcile-handle-prereq LAND** (`w9-reconcile-handle-prereq-FINAL`
  @9fb9016, ~+225 projected per L5): the DOMINANT gate. Land the reconciled head
  (`MILO_MESSAGE_TIMERS` gate, NOT HX_NATIVE) + global END_HANDLERS PathName tail
  + `/DMILO_MESSAGE_TIMERS` per-TU restore for ALL timer-on TUs (BandDirector +24
  the dominant). Unblocks the entire binary-wide Handle reveal wave. kind=header-macro.
- **The 100-fn wired Handle pairing wave** (L4 `/tmp/wired_handles2.json`): AFTER the
  Family-B prereq lands, each small/mid wired Handle (UIGuide/FlowIf/Flow need a
  1-line superclass fix first; the rest are bare HANDLE_SUPERCLASS reveals) is a
  self-contained pin-already-present + map-entry + objdiff-verify item. EV +18..+30.
  kind=pin/pair, attribution_risk=true.
- **map re-serialization conflict risk:** the SongStatusMgr branch rewrites
  ~5700 lines of `target_symbol_map.json` (re-sort/re-serialize). Concurrent
  map-touching lands (Family-A reconcile, any reveal batch) will conflict on the
  serialized form. A coordinator should land map-touching units serially or adopt a
  stable-key-order serializer. kind=tooling.
- **map-coverage reveal-audit TOOL** (carried from L6): for any wired+pinned unit,
  byte-diff every unmapped target `fn_` against same-size own methods, emit the
  byte-exact ones as a reveal worklist. Auto-harvests the residual reveals across
  ALL W9 port-then-pin lands (BandSongMgr/SongSortMgr/SongUpgradeMgr/LicenseMgr).
  THIS is the legitimate "reveal cascade" tool — but it only finds reveals on the
  LANDED base, never on main where the base isn't yet present. kind=tooling.
