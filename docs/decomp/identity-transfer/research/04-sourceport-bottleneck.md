# Identity-transfer research 04: the SOURCE-PORT bottleneck

**Lane: sourceport.** Author pass 2026-06-21. Analysis-only (no code/build mutation).

## TL;DR

Identity-transfer's transport (micro-pin → carve → name → objdiff-pair) is **solved**
(case-A proven by RockCentral.cpp +17). Both case-A and the banked objdiff case-B fork
are **100% gated on one thing my lane owns: the ported MWCC→MSVC source compiling each
scattered method to a body that is *byte-identical* to retail.** Wave-16 BandProfile
reached **0/64 at 100%** not because the port was sloppy — the port was a near-verbatim
transcription of the rb3-Wii oracle (see "the port was faithful" below) — but because of
**four divergence axes that a whole-TU port cannot escape**: (1) **struct-layout drift
amplified by array members**, (2) **MWCC-vs-MSVC inlining policy**, (3) **`BEGIN_HANDLERS`
macro content divergence**, (4) baseline **MWCC-vs-MSVC codegen** noise.

The single highest-leverage change is **abandoning whole-TU porting for per-method
partial porting**, gated on a new static check (does the method touch any struct field
*above* the first layout-divergent member?), and **dropping the `sim≥0.5` oracle gate**
that the locator and the case-B fork both use — it is empirically disproven as a
portability predictor (the proven RockCentral win has **0 methods at sim≥0.5**).

---

## 1. The mechanism is gated on byte-exact compilation, nothing else

Confirmed from `docs/decomp/identity-transfer.md:45-55` and
`docs/decomp/handoff/objdiff-caseb-fork-banked.md:49-58`:

- **Case-A** (method in an unowned `auto_*` blob): objdiff pairs target↔base **by name**
  and reports the byte-match directly. No sim gate at match time. Works with stock tools.
- **Case-B** (method physically inside a foreign pin): needs the banked objdiff fork
  (`../objdiff` branch `caseb-global-byteeq @ b1c92be`), which promotes an unmatched
  named target fn iff a byte-identical base symbol exists anywhere AND it passes an
  **oracle gate of `sim≥0.5`** (`objdiff-caseb-fork-banked.md:28-33`).

Both paths require our compiled obj to **define each method byte-exact**. That is the port.
The honesty contract (`identity-transfer.md:68-74`) further demands the newly-100 be
**real-bodied (>44B), not ≤44B ICF-stub folds** — so trivial accessors can't carry the win
even when they "match."

---

## 2. Wave-16 BandProfile: why 0/64, dissected

Commit `ec65595` ("identity-transfer self-refute, net +0"). Roadmap WAVE-16 CLOSE
(`docs/plans/decomp-state-and-roadmap-2026-06-09.md:1809+`): "ZERO reached 100% (best
fuzzy 47.8%, ctor 1.7%)."

### The port was faithful (so porting *quality* is not the bug)

`diff w16-bandprofile:BandProfile.cpp  ../rb3/.../BandProfile.cpp` shows only **4 trivial
API-compat edits** across 1013 lines:

| ours | oracle | reason |
|---|---|---|
| `TheContentMgr.RefreshDone()` | `TheContentMgr->RefreshDone()` | ref vs ptr |
| `key.Str()` | `key.mStr` | Symbol accessor |
| `mod.Str()` | `mod.mStr` | Symbol accessor |
| `...->Compress((RndTex::AlphaCompress)0)` | `...->Compress(false)` | enum sig |

A faithful transcription still produced 0/64. **The bottleneck is structural, below the
source level.**

### Root cause A — struct-layout drift amplified by an array member (the dominant killer)

`BandProfile.h:167`: `PerformanceData mPerformanceDataList[50]; // 0x788`.

`PerformanceData` embeds `Stats mStats` (`PerformanceData.h:46 // 0x48`). Our `Stats`
(`src/band3/game/Stats.h`) is **byte-identical to the rb3-Wii oracle Stats.h** (verified —
both files diff clean). But the oracle Stats is the **Wii** layout, which is **~65B larger
than Xbox-360 retail** (per the `ec65595` commit message analysis). That per-element delta
is **multiplied ×50** by the array → our `unk6fb4` lands at `0x7c74` vs retail's `0x6fb4`
(**shift 0xcc0 = 3264 B**). Every method that touches a tail field
(`mProfilePicture`@0x6fbc, `mTourBand`@0x6fc0, `unk6f70`, `mProfileAssets`, the
`mPerformanceData*` upload fields) loads the wrong offset → instant divergence.

Field-access census of `BandProfile.cpp`: **95 source lines** reference tail-divergent
members vs the stable head — a large fraction of all 93 methods touch the poisoned tail.

**This is the general scattered-TU trap: a single wrong member size in an *embedded* or
*array* member shifts every subsequent offset, and array multiplicity (`[50]`) makes a tiny
delta catastrophic.** The fix (port the *retail* Stats layout) is a shared-game-type change
that ripples across Stats/PerformanceData/Game/GamePanel — out of any single port's scope,
and itself a struct-lever wave.

### Root cause B — oracle VA mis-attribution on ≤44B ICF stubs

`ec65595` + `locator.py:23-33`. The rb3-Wii BinDiff oracle maps tiny accessors to **wrong
VAs**: e.g. `GetUploadFriendsToken` (a 1-instr `lwz r3,off(r3); blr`) was carved against a
32B target fn doing global-state mutation. 4 named VAs were ICF-collisions where the map
already held a foreign name (`_M_insert_overflow_aux`, `CleanGraph`, `SubmitLeapPacket`,
`TB_SAFE_CLOSE_HANDLE`) — correctly KEPT-EXISTING by `identity_transfer.py`'s add-only rule.
`tools/locator.py` exists precisely to demote these (its MISATTRIBUTED class), but they
contribute nothing either way (≤44B = honesty-disqualified anyway).

---

## 3. The four divergence axes (generalized)

| # | axis | mechanism | escapable by a faithful port? | mitigation |
|---|---|---|---|---|
| **A** | **struct-layout drift + array amplification** | Wii oracle layout ≠ retail; embedded/array members multiply the delta (BandProfile `[50]` → 0xcc0 shift) | **No** — header is shared, fix is a struct-lever | per-method skip of tail-touching methods; OR fix the retail layout first (own wave) |
| **B** | **MWCC vs MSVC inlining policy** | `FORCE_LOCAL_INLINE` = MWCC `force_active` pragma; **expands to NOTHING under MSVC** (`src/decomp.h:5-12`). `/Ob2` decides inline-ness independently. `Node::Compare` (SongSortNode.cpp:30-34) is forced-inline on Wii → may be out-of-line on MSVC, flipping `bl` vs inlined body at every caller | **Partly** — depends on `/Ob2` heuristics, not controllable per-method | accept as permuter-class; prefer methods with no force-inlined callees |
| **C** | **`BEGIN_HANDLERS` macro divergence** | The `Handle()` methods are the lowest-sim WALL (SongSortNode: sim 0.008–0.05, sizes 52–168B). Macro expansion depends on `MILO_MESSAGE_TIMERS` (keystone, gated per-TU in `objects.json:43+`), `HANDLE_CHECK` line numbers, and engine-header versions | **Mostly no** — `Handle` bodies are TU-content + macro-version sensitive | **defer all `::Handle` methods**; they are never cheap-port wins |
| **D** | **baseline MWCC↔MSVC codegen** | register allocation, instruction scheduling, branch shape differ even for identical source | per-function; permuter territory | run the permuter on the residual after A–C are handled |

---

## 4. The locator's `sim≥0.5` gate is empirically WRONG as a portability predictor

`tools/locator.py` classifies scattered methods CONFIRMED/RECON/WALL/UNPLACEABLE/
MISATTRIBUTED. Its CONFIRMED gate is `sim≥0.5 AND S≥0.60` (docstring line 31). The case-B
fork uses the same `sim≥0.5` oracle gate. **Both are mis-calibrated for scattered game TUs,
because the oracle sim measures rb3-Wii(MWCC PPC) vs rb3-retail(MSVC X360) — two different
compilers — so it carries a large constant noise floor unrelated to source portability.**

Decisive evidence (cross-referenced `unified_id_rb3wii.json` against the build):

- **SongSortNode ground truth** (`docs/decomp/research/2026-06-21-songsortnode-va-confirmation.json`):
  **0/53 CONFIRMED**, yet the verdict states all 53 are *correctly attributed to the right
  named method* — "Reconstruction is a body-divergence problem, not a VA-finding problem."
  Every method sits at sim ≈ 0.41 (the MWCC↔MSVC floor), not because it's un-portable.

- **Already-pinned-and-matched game TUs** have low oracle sim too: RockCentral median
  **0.099** / max 0.433 / **0 at sim≥0.5**; MusicLibrary median 0.042 / 0 at ≥0.5;
  AccomplishmentManager median 0.041 / 0 at ≥0.5; SongSortMgr 0.049 / 0 at ≥0.5. These TUs
  are substantially matched in the build — proving sim≥0.5 is **not** required to match.

- **The proven RockCentral +17 win** has **26 real-bodied (>44B) methods, ZERO at sim≥0.5**
  (max 0.433; 10 at sim≥0.12). The case-B fork's oracle gate and the locator's CONFIRMED
  gate would have **rejected the entire proven win.** Case-A succeeded only because objdiff
  pairs by *name* and checks *byte-equality* directly, never consulting sim.

**Implication:** do not let `sim≥0.5` gate which methods to port. It buys near-zero recall.
Replace it with the only ground truth that matters: **compile the method and let objdiff
report byte-equality.** Sim is useful only for the locator's *MISATTRIBUTED* demotion
(very-low sim + size-ratio), not for predicting a match.

---

## 5. Portability ranking of scattered game TUs

Ranked by **real-bodied (>44B) method count** (stub-folds are honesty-disqualified) among
**unpinned game TUs** (`band3/`, `net_band/`). The `port` column = methods at >44B AND
sim≥0.5 (the *over-strict* locator yield — treat as a floor, real yield is higher per §4).

| TU | total | real(>44B) | stub(≤44B) | med sim | locator-yield | notes |
|---|---|---|---|---|---|---|
| GemPlayer.cpp | 169 | 35 | 134 | 0.41 | 8 | large; many stubs |
| **BandProfile.cpp** | 104 | 20 | 84 | 0.18 | 4 | tail-shift wall (§2A) |
| DuplicationSpace.cpp | 40 | 20 | 20 | 0.18 | 6 | network, balanced |
| Game.cpp | 103 | 20 | 83 | 0.21 | 5 | shared Stats risk |
| NetSession.cpp | 111 | 18 | 93 | 0.15 | 7 | network |
| ObjDupProtocol.cpp | 36 | 18 | 18 | 0.16 | 4 | network, low-stub |
| **MetaPerformer.cpp** | 70 | 16 | 54 | **0.41** | 5 | high floor sim |
| TrackPanel.cpp | 67 | 17 | 50 | 0.31 | 3 | UI |
| **Station.cpp** | 51 | 14 | 37 | **0.41** | 5 | network |
| TourProgress.cpp | 48 | 10 | 38 | **0.41** | 5 | already partly pinned-adjacent |
| SessionMessages.cpp | 53 | 10 | 43 | **0.43** | 9 | highest locator-yield |

### What actually makes a TU portable (the real criteria, in priority order)

1. **No embedded/array struct members with retail-vs-Wii layout drift** (the §2A trap).
   Flat structs of pointers/ints/Symbols (like SongSortNode's nodes, 0x1c–0x54) are safe;
   anything with `Foo mArr[N]` or a big embedded `Stats`/`GameplayOptions`/`AccomplishmentProgress`
   is a layout minefield. **This is the #1 filter.**
2. **Small flat structs** → low offsets → fewer ways to diverge (SongSortNode nodes vs
   BandProfile's 0x6fc0-deep tail).
3. **Few `BEGIN_HANDLERS`/`::Handle` methods** (axis C is unwinnable cheaply — defer them).
4. **Few `FORCE_LOCAL_INLINE`/force-inlined helpers** (axis B).
5. **High *floor* sim is a weak positive** (MetaPerformer/Station/SessionMessages at ~0.41)
   but NOT decisive — RockCentral matched at 0.10 median.

By these criteria **SongSortNode is the best first target** despite 0/53 locator-CONFIRMED:
small flat node structs (no arrays, no embedded heavy types), and ~15 RECON methods that are
plain accessors/loops. Its WALL set is exactly the 13 `Handle` + STL-`Insert`/`equal_range`
methods we'd defer anyway. The **network DDL/Operation/State TUs**
(`SharedSessionDescription` sim 0.63, `StationState` 0.54, `KerberosEncryption` 0.55,
`StationIdentificationDDL` 0.67) are tiny (3–10 methods) with genuinely high sim — cheap
warm-up wins to validate the partial-port pipeline before the big TUs.

---

## 6. Per-method partial porting — the key mitigation (yes, it helps a lot)

The whole-TU port loses because it pins *all* methods, including the ones doomed by axes A–C.
**Partial porting** — compile the whole TU (so the obj defines every symbol) but **micro-pin
+ oracle-name only the methods that can byte-match** — converts a 0/64 whole-TU loss into a
small-but-positive win, exactly as RockCentral did (it pinned 104 case-A of 129).

`identity_transfer.py` already supports this: it carves per-method and the add-only map only
names what we choose. The missing piece is a **per-method portability triage** that decides
*which* methods to pin. Concretely, pin a method only if ALL hold:

- `true_size > 44` (real body, honesty rule).
- **No field access at or above the first layout-divergent member offset.** For a TU with a
  known divergence point D (e.g. BandProfile D = 0x788, the array start), a method that only
  touches head fields (`GetAllChars`, `GetCharFromGuid`, `GetMaxChars`, `NumChars`,
  `CharAt`) is safe; one touching `mProfilePicture`/`mTourBand`/`mPerformanceData*` is not.
  This is a static scan of the method's `lwz/lfs/stw off(this)` against D.
- Not a `::Handle` method (axis C).
- Then **verify by compiling + objdiff byte-equality** — the only real gate.

This turns BandProfile from 0 into "the head-only accessor methods" (a handful), and—once
the retail Stats layout is fixed in a separate struct-lever wave—**reveals the whole tail at
once** (the wave-17 OnlineID reveal-cascade pattern).

---

## 7. Can locator.py let us skip porting? No — but it changes *what* we port

The locator confirms **VA placement** (which retail fn is which named method), not body
equality. Its own ground truth says placement is solved; the wall is body divergence
(§4). So it **cannot** substitute for the port. Its real value in this pipeline:

- **MISATTRIBUTED demotion** — drop the oracle's wrong-VA accessors (§2B) before pinning, so
  we don't carve a garbage VA. Keep this.
- **WALL flagging** — the `::Handle` + STL methods it walls are exactly the ones partial-port
  should skip. Use it as the *skip* list, not the *port* list.
- **Stop trusting its CONFIRMED/RECON sim split as a go/no-go** (§4). Re-task it to emit the
  partial-port pin-set = (real-bodied) ∧ (not MISATTRIBUTED) ∧ (not WALL) ∧
  (no divergent-tail field access), then let objdiff make the final call.

---

## GAPS / what to build

1. **`field_offset_gate` for partial porting (highest leverage).** A static analyzer that,
   given a TU + a divergence-point offset D, scans each Wii method body
   (`../rb3/build/.../asm`) for any `this`-relative load/store ≥ D and tags it
   POISONED-TAIL. Feed the clean set to `identity_transfer.py --pin-only <list>`. This is
   the concrete mechanism that raises hit-rate above wave-16's 0/64. ~150 LOC; reuses
   `locator.py`'s asm-walk primitives.

2. **Drop/replace the `sim≥0.5` oracle gate** in both `locator.py` (CONFIRMED) and the
   case-B objdiff fork (`--global-byte-eq-oracle`, `objdiff-caseb-fork-banked.md:28`). It
   rejected the entire proven RockCentral win. Replace with: real-bodied (>44B) ∧
   not-MISATTRIBUTED ∧ **byte-equality verified by objdiff** (case-A) or by the fork's
   masked-bytes + reloc-NAME equality (case-B, already implemented — it's the *oracle* sub-gate
   that's wrong). Keep a *low* sim floor (≥0.02) only to kill the zero-corroboration MISATTRIB
   class.

3. **Retail-Stats struct-lever wave (unblocks BandProfile/Game/GemPlayer at once).** The
   Wii `Stats` is ~65B fatter than retail; fix is a single struct-lever on `game/Stats.h`
   that cascades through `PerformanceData`/`BandProfile`/`Game`/`GamePanel`/`GemPlayer`.
   This is the §2A root and the prerequisite for the whole meta_band performance-data family.
   Hand to the struct-lever lane, not the port lane — but it is THE force multiplier here.

4. **`identity_transfer.py --pin-only <method-list>` flag** so a partial port can pin an
   explicit subset (it currently classifies case-A/B for all oracle methods of a TU). Trivial.

5. **A "warm-up" validation pass on the tiny high-sim network TUs**
   (`SharedSessionDescription`/`StationState`/`KerberosEncryption`/`StationIdentificationDDL`,
   3–10 methods, sim 0.5–0.67) to prove the partial-port + byte-equality pipeline end-to-end
   on easy mode before spending the multi-hour BandProfile/GemPlayer ports.

### Recommended porting playbook (replaces whole-TU porting)

```
per TU:
  1. Determine divergence-point D = first member offset whose retail layout != Wii
     (default D = first embedded heavy/array member; for flat-struct TUs D = ∞ = no trap).
  2. Port the TU faithfully (whole file) so the obj DEFINES every symbol. Wire NonMatching.
  3. field_offset_gate: drop methods with a this-load ≥ D  (axis A).
  4. drop ::Handle methods (axis C) and ≤44B stubs (honesty).
  5. identity_transfer.py --tu X --pin-only <surviving set> --apply.
  6. objdiff per-unit A/B: the BYTE-EQUALITY result is the only gate. Keep the matchers.
  7. permuter pass on the >97% residual (axis D).
  8. if D was a real layout bug, file a struct-lever for D → reveal-cascade the tail later.
```
