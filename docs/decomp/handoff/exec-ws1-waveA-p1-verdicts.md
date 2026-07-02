# exec ws1-waveA — P1 confirm-on-consume verdicts (2026-07-02)

Packet: **p1-verify-applied**. Worktree `/home/free/tmp/wt-exec-ws1-waveA`
(branch `exec/ws1-waveA-0702`, on top of pin commit `57345e0`). Per-symbol
objdiff via MCP `run_objdiff` (`project_dir=<this worktree>`, renamer refreshed
via `pre-compile`). insert/delete direction confirmed from full listings:
**insert = base-only, delete = target-only** (target = retail reference).

## Result

- **31 verdicted. 21 kept, 10 removed (FAIL).** 10 FAIL < 15 KILL threshold → no kill.
- Map `scripts/target_symbol_map.json`: 13707 → **13697** keys (10 removed).
- `config/45410914/splits.txt`: removed 2 failed micro-pin `.text` lines
  (Faders `0x826EDAB0`, SongData `0x8274C518`). config.json re-split + `pre-compile`
  clean (no dtk errors; kept SongData/Faders spans still resolve).
- Post-removal spot check: `?GetTargetDb@Fader@@QBAMXZ` → *not found* (removed);
  `?GetVal@FaderGroup@@QAAMXZ` → 100%; `?UnflipGems@SongData@@QAAXHHH@Z` → 95.1% (intact).

## Verdict table

| addr | symbol | pct | insns | verdict | evidence |
|---|---|---|---|---|---|
| 0x82328ea0 | Conceal@BandList | 99.8% | 19 | PASS-fuzzy | 3 member-store arg diffs [off:+8] + 1 stack SHIFTED; real body |
| 0x82328ef0 | ConcealNow@BandList | 99.9% | 16 | PASS-fuzzy | 2 store [off:+8] arg diffs |
| 0x8232c410 | Reveal@BandList | 99.9% | 18 | PASS-fuzzy | 2 store [off:+8] arg diffs |
| 0x8235b138 | DrawLod@Character | 82.5% | 37 | PASS-fuzzy-named | right class+sig void(int); COMPARISON_STYLE cmpwi off-by-1 x6 + branch polarity |
| 0x82370ed8 | ForceBlink@CharEyes | 60.0% | 27 | PASS-fuzzy-named | TARGET = clean 3-member-set matching Wii exactly (mProceduralBlink@0x144, mBlinkTimestamp@0x148 via TaskMgr::Seconds, mBlinkWindowCount++@0x14c); base has extra 0x169/0x144 guard = source bug, not misname |
| 0x82373240 | NextLook@CharEyes | 66.9% | 496 | PASS-fuzzy-named | huge unique protected body; r30↔r31 regalloc cascade + frame Δ |
| 0x82374110 | Poll@CharEyes | 87.8% | 430 | PASS-fuzzy-named | huge unique virtual body; regalloc cascade |
| 0x823871f0 | CaptureBefore@CharIKScale | 0.0% | 6 | **FAIL** (wrong-fn) | target = 12B fragment (lwz 0x28/nullcheck/beqlr, NO store); base IS the real body (lfs v.z@0x54→stfs mScale@0x2c). Target≠CaptureBefore |
| 0x8244e668 | SetBones@CharDriver | 5.7% | 33 | **FAIL** (wrong-fn) | Wii SetBones=1-liner `mBones=obj` (base=8B tail-call to ObjRefConcrete::SetObjConcrete); target=132B/-0x70-frame multi-ObjRef fn → not SetBones |
| 0x824c1a60 | DeSelect@SpotlightDrawer | n/a | — | **FAIL** (not-a-boundary) | not-found; 0x824C1A60 is mid-function (`lis r10,...`), no fn symbol to rename |
| 0x8250fc50 | ToDateString@DateTime | 0.0% | 68 | **FAIL** (<50, body-divergent) | identity CONFIRMED (shared GetDateFormatting + String::+= epilogue + fields@0x76c) but retail=88B single-format vs base=272B 3-way DateFormat switch (DC3-newer). Re-addable when body ported (p4) |
| 0x826e6d08 | Init@StandardStream | 99.3% | 167 | PASS-fuzzy | 167-insn body, right private sig(float,float,Symbol,bool); 1 stfs delete + 2 arg |
| 0x826edab0 | GetTargetDb@Fader | 47.9% | 12 | **FAIL** (<50) | identity CONFIRMED (mFaderTask nullcheck→mInterp->Y1()/mVal) but base over-inlined Y1 switch (cmpwi 0x1/0x2). Removed key + micro-pin line |
| 0x826ede48 | GetVal@FaderGroup | 100.0% | 11 | PASS | distinctive loop summing (*it)->GetVal() over mFaders (virtual call + fadd); not a fold (icf map clean) |
| 0x8274bf50 | UnflipGems@SongData | 95.1% | 165 | PASS-fuzzy-named | 165-insn unique body void(int,int,int); regalloc/offset swaps |
| 0x8274c230 | ValidateVocalSPPhrases@SongData | 54.0% | 258 | PASS-fuzzy-named | 258-insn unique body; r27↔r28 regalloc cascade |
| 0x8274c518 | AddMultiGem@SongData | 45.1% | 46 | **FAIL** (<50) | virtual; target=real -0x70 body, base body diverges heavily. Removed key + micro-pin line |
| 0x8276ccf8 | ResetPitchBend@BeatMatcher | 87.4% | 34 | PASS-fuzzy-named | right virtual(int); small switch (cmpwi 0x4/0x5) + r10↔r11 regswap |
| 0x82778738 | Jump@TrackWatcher | 100.0% | 5 | PASS | mImpl(off0)->Jump vtable slot 0x14; name pairs, slot matches; monotonic addr↔slot |
| 0x82778780 | NonStrumSwing@TrackWatcher | 100.0% | 5 | PASS | mImpl->NonStrumSwing slot 0x24 matches |
| 0x827787c8 | RGFretButtonDown@TrackWatcher | 99.8% | 5 | PASS-fuzzy | mImpl->RGFretButtonDown forwarder; tgt slot 0x2c vs base 0x30 = Impl-vtable decl-order header bug, NOT misname |
| 0x827787e0 | Enable@TrackWatcher | 100.0% | 5 | PASS | mImpl->Enable slot 0x3c matches |
| 0x82778810 | Restart@TrackWatcher | 99.8% | 5 | PASS-fuzzy | **PACKET-FLAGGED — scrutinized:** IS Restart (mImpl->Restart forwarder), NOT a sibling; tgt slot 0x48 vs base 0x18 = Impl-vtable ordering bug; monotonic addr↔slot (0x14<0x24<0x2c<0x3c<0x48) proves correct binding |
| 0x82780788 | Terminate@SongPreview | 84.0% | 82 | PASS-fuzzy-named | 82-insn unique body; member 0x40/0x48 flag divergence |
| 0x8278a2b8 | OnMsg@CreditsPanel | n/a | — | **FAIL** (not-a-boundary) | not-found; 0x8278A2B8 = `lbl_8278A2B8` size 0x0, internal `bl` target (called from 0x8278A7A8 in-TU), not OnMsg start |
| 0x827aa8f8 | DetachBuffer@NetLoader | 100.0% | 10 | PASS | distinctive dual-return detach (flag@0x10, buffer@0x14 nulled); matches char* DetachBuffer (40B) |
| 0x827ccd80 | Poll@UILabel | 3.4% | 28 | **FAIL** (<50, base-fwd) | target=full 28-insn virtual body (frame -0x60, members 0x68/0xf4); base=1-insn tail-call `b`. Name correct (p3 port target) but <50 |
| 0x827dc698 | HasTransitions@UITransitionHandler | n/a | — | **FAIL** (not-a-boundary) | not-found; 0x827DC698 = `lbl_827DC698` size 0x0 (mid-fn `lwz r11,0xc(r3)`), inlined-away method fragment |
| 0x827e4c98 | DrawShowing@UISlider | 4.2% | 24 | **FAIL** (<50, base-stub) | target=real 24-insn virtual body (frame -0x60, calls fn_827E4A38, addi 0x140); base=stub |
| 0x827ed440 | Draw@UIPanel | 0-byte | 0 | **DEFER** (kept) | fn_827ED440 IS a real 0x34-byte boundary matching Wii Draw (`if(mDir&&!mLoaded) mDir->DrawShowing()` ~13i); NOT in ICF map; objdiff 0/0-byte reading = duplicate-symbol measurement artifact. Name correct; NOT a claimable strict match — reviewer composed report re-measures. KEPT in map |

## Removed keys (10) — reasons

wrong-function bindings: `0x8244e668` SetBones (target=132B fn, Wii=1-liner),
`0x823871f0` CaptureBefore (target=12B fragment, no store).
not-a-boundary (dtk size-0 label / mid-fn, symbol never resolves):
`0x824c1a60` DeSelect, `0x8278a2b8` OnMsg@CreditsPanel, `0x827dc698` HasTransitions.
<50% (real function, our body diverges/under-built): `0x8250fc50` ToDateString (0%,
DC3-newer body), `0x826edab0` GetTargetDb (47.9%, +micro-pin line removed),
`0x8274c518` AddMultiGem (45.1%, +micro-pin line removed), `0x827ccd80` Poll@UILabel
(3.4%), `0x827e4c98` DrawShowing@UISlider (4.2%).

Re-addable in port packets once bodies are fixed: ToDateString (p4), Poll@UILabel (p3),
GetTargetDb/GetVal are in the same Faders TU (GetVal kept).

## Notes for reviewer

- Two survivors carry known **Impl-vtable decl-order** bugs (TrackWatcher RGFretButtonDown
  0x2c-vs-0x30, Restart 0x18-vs-0x48) — a header reorder in TrackWatcherImpl, not a naming
  issue; both are genuinely the named forwarders (monotonic addr↔slot). Left as fuzzy.
- ForceBlink our-side has a spurious 0x169/0x144 guard vs retail's clean body — a Wii-source
  divergence to fix later; name is correct.
- **UIPanel::Draw (0x827ed440)** kept as DEFER: objdiff reads 0/0 bytes (duplicate-symbol
  artifact) though fn_827ED440 is a real 0x34-byte boundary consistent with Wii Draw. It will
  not count as a strict match; verify in the composed A/B / icf_alias_check.
- Git: per worker rules, edits are left in the working tree **uncommitted** (map + splits +
  this doc). Reviewer to commit on `exec/ws1-waveA-0702` (suggested prefix
  `verify(ws1-waveA-p1):`).
