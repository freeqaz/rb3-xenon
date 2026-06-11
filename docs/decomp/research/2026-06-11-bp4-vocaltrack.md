# BP4 recon — lane `vocaltrack` (PORT-THEN-EXTEND) — 2026-06-11

**Verdict: GO. The wave-3 "port-then-extend" precondition is ALREADY SATISFIED — the port
landed on main in commit `c5011b0` ("band3: port OvershellSlot + Stats + VocalTrack(partial)
— +22 @100%") and the wave-3 refutation is now STALE. 30 of the 33 mapped target functions in
the extension range are BYTE-IDENTICAL (relocations masked) to our compiled
`build/45410914/src/band3/bandtrack/VocalTrack.obj`. The remaining work is a splits-only
extension + FontBase sliver eviction. Expected delta: +30 proven, up to +33 with optional
reveal entries.**

Baseline: main @78a6ee6, report.json fresh, 7785 matched. `default/VocalTrack`: 148 fns /
59 matched. Current pin `.text [0x82B727B8, 0x82B7A2A0)`, pdata `[0x82254EC0, 0x82255330)`.

---

## 1. Why the wave-3 refutation no longer holds

Wave-3 note (/tmp/sliver_workflow_notes.md §"VocalTrack+Gem") refuted extension because
"VocalTrack.obj .text = 0x7AE8 == pin size exactly; the 22 named methods are NOT in our
source". Measured TODAY on main:

- `src/band3/bandtrack/VocalTrack.cpp` (2764 lines) **defines every named method** in the
  extension range: PushGameplayOptions, ReadTimingData, ShowPitchCorrectionNotice,
  ConfigNoteTube, SetDir, WantBeatLines, CheckDeploySections, StartUpdateArrows,
  UpdateUnusedArrows, OnGet/OnSetDisplayMode, ClearTubePlates, ResetTubePlates, GetLastLyric,
  DumpLyricPlates, ClearLyrics, ReturnFirstMarker, InvalidateMarkers, ClearMarkers,
  UpdateAllTubePlates, HookupTubePlates, InitPlatePool, TambourineGemPool::FreeUsedGems.
  (Verified against oracle `/home/free/code/milohax/rb3/src/band3/bandtrack/VocalTrack.cpp`,
  2762 lines — same method set.)
- Compiled `build/45410914/src/band3/bandtrack/VocalTrack.obj` total `.text` COMDAT bytes =
  **0x12628** (1553 sections), far beyond the 0x7AE8 pin — the extension-range bodies are
  compiled and waiting, unmeasured because the pin doesn't cover their target VAs.

## 2. Decisive evidence: masked byte-equality, target vs compiled

Method: per mapped VA, take target bytes from the dtk blob asm
(`build/45410914/asm/auto_03_82B64D38_text.s` + `auto_03_82B6E934_text.s`, the
`/* ADDR ... B0 B1 B2 B3 */` columns), take our compiled COMDAT section bytes for the mapped
mangled symbol, zero the 4-byte word at every COFF relocation offset on both sides plus any
`b/bl` (opcode 18) word, compare. This emulates report `match_percent_normalized` (reloc-
address-insensitive) scoring.

Result over the 33 `scripts/target_symbol_map.json` entries in `[0x82B6D688, 0x82B727B8)`:

```
BYTE-EQ(masked): 30   DIFF: 0   ABSENT: 3
```

All 30 present symbols are also SIZE-EQ (e.g. ReadTimingData 572==572, ClearLyrics 600==600,
ConfigNoteTube 448==448, FreeUsedGems 172==172). Zero size or byte divergence. The 30:

```
0x82b6d688 ?PushGameplayOptions@VocalTrack@@UAAXW4VocalParam@@H@Z                      44B
0x82b6d768 ?ReadTimingData@VocalTrack@@QAAXPBVDataArray@@@Z                           572B
0x82b6d9a8 ?ShowPitchCorrectionNotice@VocalTrack@@UBA_NXZ                              24B
0x82b6d9c0 ?ConfigNoteTube@VocalTrack@@QAAX_NHH0M@Z                                   448B
0x82b6db80 ?SetDir@VocalTrack@@UAAXPAVRndDir@@@Z                                      104B
0x82b6dbe8 ?WantBeatLines@VocalTrack@@QAA_NH@Z                                        172B
0x82b6df00 ?CheckDeploySections@VocalTrack@@QAA_NPAVLyric@@MAAHABV?$vector@U?$pair@MM@stlpmtx_std@@V?$StlNodeAlloc@U?$pair@MM@stlpmtx_std@@@2@@stlpmtx_std@@_N0AAM@Z  208B
0x82b6e278 ?StartUpdateArrows@VocalTrack@@QAAXXZ                                      104B
0x82b6e2e0 ?UpdateUnusedArrows@VocalTrack@@QAAXXZ                                     128B
0x82b6e868 ?_M_pop_front_aux@?$deque@VRangeShift@VocalTrack@@...                      116B
0x82b6e938 ?_M_pop_front_aux@?$deque@VLyricShift@VocalTrack@@...                      116B
0x82b6ed10 ?OnGetDisplayMode@VocalTrack@@QAA?AVDataNode@@PBVDataArray@@@Z             100B
0x82b6ed78 ?OnSetDisplayMode@VocalTrack@@QAA?AVDataNode@@PBVDataArray@@@Z             176B
0x82b6f1c0 ??$__copy@U?$_Deque_iterator@PAVTubePlate@@...                             232B
0x82b6f2a8 ??0?$_Deque_base@PAVTubePlate@@...__move_source...                         136B
0x82b6f330 ?ClearTubePlates@VocalTrack@@QAAXAAV?$deque@PAVTubePlate@@...              104B
0x82b6f420 ?ResetTubePlates@VocalTrack@@QAAXAAV?$deque@PAVTubePlate@@...              160B
0x82b6f6e0 ?GetLastLyric@VocalTrack@@QAAPAVLyric@@AAV?$deque@PAVLyricPlate@@...       188B
0x82b6f858 ?DumpLyricPlates@VocalTrack@@QAAXAAV?$deque@PAVLyricPlate@@..._N@Z         212B
0x82b6ffb0 ?ClearLyrics@VocalTrack@@QAAXXZ                                            600B
0x82b70ad8 ?ReturnFirstMarker@VocalTrack@@QAAXXZ                                      100B
0x82b70ec0 ??0?$deque@PAVTubePlate@@...QAA@ABV01@@Z                                   224B
0x82b70fa0 ?_M_push_back_aux_v@?$deque@U?$pair@PAVRndMesh@@M@...                      176B
0x82b711a8 ?_M_push_back_aux_v@?$deque@VLyricShift@VocalTrack@@...                    184B
0x82b71478 ?InvalidateMarkers@VocalTrack@@QAAXM@Z                                     120B
0x82b714f0 ?ClearMarkers@VocalTrack@@QAAXXZ                                            92B
0x82b71af8 ?UpdateAllTubePlates@VocalTrack@@QAAXM@Z                                   296B
0x82b71d78 ?HookupTubePlates@VocalTrack@@QAAXPAVNoteTube@@@Z                          192B
0x82b71f68 ?FreeUsedGems@TambourineGemPool@@QAAXXZ                                    172B
0x82b72300 ?InitPlatePool@VocalTrack@@QAAXXZ                                          176B
```

The 3 ABSENT (foreign COMDATs the retail linker interleaved; `PoolVoice` and `FileMerger`
appear in NEITHER our VocalTrack.cpp/.h nor the rb3-Wii oracle's — grep-verified):

```
0x82b71268 ??$_Copy_Construct@UMerger@FileMerger@@...    60B   WALL: foreign COMDAT
0x82b712d0 ??$__destroy_range_aux@...Unlockable@?A0xf8e4b4b5...  96B  WALL: foreign anon-ns
                                                              COMDAT (ICF-alias name; same
                                                              ?A0xf8e4b4b5 family already
                                                              sits @0% inside current pin)
0x82b71730 ?push_back@?$deque@UPoolVoice@@...           100B   WALL: foreign COMDAT
```

Map entries for all 33 ALREADY EXIST (VA-keyed) — **zero new map entries required** for the
extension; the pre-compile renamer auto-pairs on re-pin.

## 3. Extension geometry + FontBase sliver eviction

Extension range `[0x82B6D688, 0x82B727B8)` = 120 target fns:
- tail of blob `auto_03_82B64D38_text` (35 fns, 0x82B6D688–0x82B6E8E0),
- **FontBase.cpp sliver pin `[0x82B6E8E0, 0x82B6E934)`** (1 fn, 0x54),
- all of blob `auto_03_82B6E934_text` (84 fns, ends exactly at current VocalTrack pin).

FontBase eviction is textbook wave-3 pattern (`requires_sliver_eviction`, unit-IS-the-sliver):
- report.json `default/FontBase`: total 1 fn (`fn_82B6E8E0`, 84B), **matched=0**.
- FontBase.cpp has **no objects.json entry** (target-only pin, no compiled obj, no source).
- Real RndFontBase content lives elsewhere (map: KerningTable cluster @0x82461xxx,
  `?CharAdvance@RndFontBase@@UBAMG@Z` @0x8270B818) — nowhere near 0x82B6Exxx.
- `fn_82B6E8E0` IDENTIFIED by masked-byte compare: it is one of VocalTrack.obj's own
  `??1?$_Deque_base@...` dtor instantiations (6 byte-identical ICF twins in our obj:
  TambourineGem/LyricPlate/TubePlate/pair<RndMesh*,float>/RangeShift/LyricShift). It even
  `bl`s `fn_82B6D710` — which masked-byte-matches our
  `?_M_destroy_nodes@?$_Deque_base@VRangeShift...` / `...VLyricShift...` — i.e. the sliver's
  callee is also inside the extension range. It is VocalTrack-TU content, full stop.
- Eviction = delete the FontBase.cpp block from splits.txt (text + its 8-byte pdata
  `[0x82254C48, 0x82254C50)`). Metric-safe: removes a 0-matched 1-fn unit.

Boundary sanity: only FontBase overlaps the extension range (checked every `.text` pin in
splits.txt). Below 0x82B6D688 the blob is foreign multi-TU content (WorldCrowd Char3D,
Symbol maps, PronunciationsLoc, `__unwind$125642` @0x82b6d528) — 0x82B6D688
(= PushGameplayOptions, first named VocalTrack method) is the correct conservative lower
bound. Upper bound = current pin start (contiguous, no gap).

## 4. Honesty gate pre-check

120 fns: 33 named (will pair), 87 anon. Max contiguous anon run = **13**
(`fn_82B70208`…`fn_82B70970`, sizes 76/4/132/96/96/96/96/132/160/356/356/268/356), sitting
directly between `?ClearLyrics@VocalTrack@@QAAXXZ` (0x82B6FFB0) and
`?ReturnFirstMarker@VocalTrack@@QAAXXZ` (0x82B70AD8) — lyric/marker helper family bracketed
by own named methods ⇒ **own-TU anon, gate-OK** per the wave-3 rule ("own STL/thunks
bracketed by own named = OK"; the prohibition is on FOREIGN runs). Next runs: 12 (idx
19–30, between UpdateUnusedArrows and pop_front_aux — deque/helper zone), 9, 6, then ≤4.
Several "anon" entries are jeff mis-nest/funclet artifacts (out-of-address-order entries
fn_82B6E500/fn_82B6E6F4/fn_82B6E6F8, fn_82B70E30 size 712 overlapping later fns) — known
tolerated noise, they read 0% in the blob today too.

## 5. UpdateScrolling assessment (current pin)

`?UpdateScrolling@VocalTrack@@QAAXM@Z` — report-normalized **52.34**, size 8948.
`run_diff_inspect mode=clusters`: **1015 insert/delete instructions in 161 clusters** —
pervasive, NOT one localized block. Decisive head divergence:

```
TGT: stwu r1, -0x370, r1          SRC: stwu r1, -0x3a0, r1     (frame -48: extra locals)
TGT: lbz  r11, 0x238, r3          SRC: lwz r11, 0x0, r3        (TGT reads a bool member
TGT: cmplwi r11, 0x0                   lwz r11, 0x1c, r11       @0x238; SRC virtual-calls
TGT: bne  0x22e4                       mtctr r11 / bctrl        vtable slot 0x1c and tests
                                       clrlwi. r11, r3, 24      the returned bool)
```

Retail body structurally differs from the Wii-oracle body throughout (devirtualized/changed
member tests, different local set). **WALL — body-divergence/port-mismatch. DEFER.** Fixing
needs retail-side re-derivation (Ghidra) of an 8.7KB function; not permuter-class, not a
single missing block. Full listing:
`function_analysis/diff_inspect_clusters__Q_UpdateScrolling_A_VocalTrack_A__A_QAAXM_A_Z.txt`.

Other current-pin named @0 (all excluded per lane brief / wall class):
- `?OnScreen@Gem@@QAA_NM@Z`, `?AddWidgetInstanceImpl@Gem@@QAAXPAVTrackWidget@@H@Z` — inline
  COMDATs already compiled in VocalTrack.obj; pairing artifact, do not chase here.
- `??1?$vector@V?$vector@PAUUnlockable@?A0xf8e4b4b5...` (136B) +
  `?_M_insert_overflow_aux@...Unlockable...` (336B) — foreign anon-ns ICF-alias names.
- `?push_back@?$vector@UMerger@FileMerger@@...` (116B) — foreign COMDAT.

## 6. Implementation plan (cold-executable)

1. `scripts/setup_worktree.sh /tmp/wt-vocaltrack bp4-vocaltrack` (buildable worktree; never
   touch main).
2. In `<wt>/config/45410914/splits.txt`:
   a. DELETE the whole `FontBase.cpp:` block (currently lines ~1684–1686:
      `.pdata start:0x82254C48 end:0x82254C50`, `.text start:0x82B6E8E0 end:0x82B6E934`).
   b. In the `VocalTrack.cpp:` block: change `.text start:0x82B727B8` →
      `start:0x82B6D688` (end stays `0x82B7A2A0`); DELETE the `.pdata` line
      (`start:0x82254EC0 end:0x82255330`) so dtk re-derives the widened pdata range
      (extension into unpinned blob = pdata-clean; FontBase's freed pdata entry is absorbed).
   No source edits. No objects.json edits. No new map entries needed (all 33 VA-keyed
   entries exist in `scripts/target_symbol_map.json`).
3. `touch config/45410914/config.yml && ./tools/ninja-locked 2>&1 | tee /tmp/rb3_build_bp4vt.log`
   (run from the worktree; if dtk/renamer staleness suspected:
   `rm build/45410914/target_symbol_renames.stamp` and re-ninja).
4. `tools/fresh_report.sh` in the worktree; this is a splits-only change so the divergence
   WARNING is a known false positive — re-run to confirm a stable count. Judge ONLY
   `measures.matched_functions`.
5. Expect: `default/VocalTrack` 148→268 total fns, matched 59→~89 (+30); whole-binary
   7785→≥7815 net, zero regressions elsewhere (no header/source touched). If any of the 30
   read <100, inspect with `run_objdiff` — likely a reloc-target pairing nit; do NOT force.
6. Honesty-gate check on the extended unit: confirm the 13-run (fn_82B70208…fn_82B70970) and
   12-run are bracketed by own named VocalTrack methods (they are, per §4) and that no
   ≥8-contiguous FOREIGN run exists. Record in landing note.
7. OPTIONAL reveal pass (+1..+3, only after step 5 is green): add VA-keyed entries to
   `scripts/target_symbol_map.json`:
   - `0x82B6E8E0` → `??1?$_Deque_base@VRangeShift@VocalTrack@@V?$StlNodeAlloc@VRangeShift@VocalTrack@@@stlpmtx_std@@@stlpmtx_std@@QAA@XZ` (ICF-ambiguous — any of the 6 byte-eq dtor instantiations works; use distinct symbols per VA),
   - `0x82B6D710` → `?_M_destroy_nodes@?$_Deque_base@VRangeShift@VocalTrack@@...` variant,
   - `0x82B6D6B8` → a pointer-deque `_M_destroy_nodes` variant (4 byte-eq candidates).
   Then `rm build/45410914/target_symbol_renames.stamp && ninja` + fresh_report. A broader
   masked-byte reveal sweep over the remaining ~84 anon vs VocalTrack.obj COMDATs is the
   follow-up vein (script = §2 method; reusable).
8. Land via the standard coordinator flow (rebase + ff-only, composed fresh verify). Do NOT
   commit from the agent.

## 7. Walls (do not re-attempt without new evidence)

| fn | class |
|---|---|
| `?UpdateScrolling@VocalTrack@@QAAXM@Z` @52.34 | body-divergence (161 clusters, frame -48, devirt/member-test rewrite); retail-rederive only |
| `??$_Copy_Construct@UMerger@FileMerger@@...` @0x82b71268 | foreign-COMDAT interleave (FileMerger absent from both sources) |
| `??$__destroy_range_aux@...Unlockable@?A0xf8e4b4b5...` @0x82b712d0 | foreign anon-ns ICF-alias |
| `?push_back@?$deque@UPoolVoice@@...` @0x82b71730 | foreign-COMDAT interleave (PoolVoice absent from both sources) |
| `?OnScreen@Gem@@QAA_NM@Z`, `?AddWidgetInstanceImpl@Gem@@...` | excluded per brief (inline-COMDAT pairing, not port) |
| current-pin `??1?$vector@...Unlockable...`, `?_M_insert_overflow_aux@...Unlockable...`, `?push_back@?$vector@UMerger@FileMerger@@...` | foreign/ICF-alias residue |

## 8. Oracle/file references

- Our source: `src/band3/bandtrack/VocalTrack.cpp` (+`.h`) — wired `NonMatching` at
  `config/45410914/objects.json:669`.
- Oracle: `/home/free/code/milohax/rb3/src/band3/bandtrack/VocalTrack.cpp` + `.h`
  (TambourineGemPool in header; FreeUsedGems already ported into our TU and byte-EQ).
- Compiled obj: `build/45410914/src/band3/bandtrack/VocalTrack.obj`.
- Target asm: `build/45410914/asm/auto_03_82B64D38_text.s`, `auto_03_82B6E934_text.s`,
  `FontBase.s` (note: `auto_03_82B768DC_text.s` is a STALE leftover, mtime 06-07; current
  report units are 82B64D38 + 82B6E934 only).
- Wave-3 refutation being superseded: /tmp/sliver_workflow_notes.md §"VocalTrack+Gem"
  (its obj-size facts predate commit `c5011b0`).
