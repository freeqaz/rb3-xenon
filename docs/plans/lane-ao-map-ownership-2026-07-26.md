# laneAO — single-owner round on `scripts/target_symbol_map.json` (2026-07-26)

Baseline at lane start: main `a059e4a8`, **36,069** strict by a single build.
**Honest baseline 36,071** — see §1. Measured there: **36,084 = +13, 0 losses**.

★**Re-measured after main advanced twice mid-lane** (`e792cc00`, then
`9b2d2737`, the latter carrying a 1,525-line `splits.txt` change worth ~+590):
**36,661 → 36,674 = +13, 0 losses, the identical 13-function gain set.** The
lane's delta is fully independent of the concurrent splits work. Rebased onto
`9b2d2737`; `git merge-base --is-ancestor main laneAO-maphand` verified, and
`git diff --stat main..laneAO-maphand` touches only this lane's own files.

**With the displacement lane's two waves integrated (§8): 36,661 → 36,707 =
+46, 0 losses.**

★The ancestry trap fired here exactly as warned: the first symptom was
`git diff --stat main..laneAO-maphand` showing my branch *deleting* another
lane's whole `docs/plans/laneAN/` tree and `splits.txt`. That is not a conflict —
it is the signature of a stale branch point. **Re-check ancestry immediately
before the final measurement, not once at the start.**

Five subagents: one adjudicating the map handoff, one read-only auditing the
map, two on the source leads, one on the displacement/rotation channels.

## 1. ★The measurement correction that reframes everything else

A **negative control** — apply a map change, rebuild, then revert the map to
byte-identical and rebuild again — showed two functions flipping to 100% with
**no change of any kind**:

    default/CameraShot    ?Disable@CamShot@@QAAX_NH@Z
    default/CharLipSync   ??$__fill_n@U?$_Bit_iter@...

First-build `dynamic_init` instability. laneAO-a3 hit the identical pair
independently, from a different direction (refreshing
`build/45410914/target_symbol_renames.stamp`), and confirmed it persists with
its source reverted. **Two independent derivations of the same two functions.**

Consequences:

* **The honest baseline is 36,071, not 36,069.** Every number in this document
  is measured against 36,071. A lane that pickles one build and banks the
  difference over-credits itself by 2.
* This is exactly why both A/B legs must get the same number of builds. Every
  measurement below uses **two builds per leg**.
* ★**`target_symbol_map.json` is NOT a ninja input to the target-symbol
  renamer.** A map-only edit needs
  `rm -f build/45410914/target_symbol_renames.stamp` in addition to
  `touch config/45410914/config.yml`; `rm -f build/45410914/report.cache` alone
  is not enough.

## 2. The handoff backlog: what was actually free

### 2a. The 11-entry byte-identity homing handoff — 6 of 11 free

`/home/free/tmp/laneAL_fixA_map_handoff.json`, valued **+6** by the producing
lane. **Measured exactly +6.** The valuation was right in aggregate.

★**But the standing calibration did not fire, and its absence is the result.**
Prior calibration: over 4 handoffs called "free", 3 paid and **every one needed
its mangled name corrected**. Here, **all 11 names were COFF-correct verbatim —
zero corrections.** This handoff's defect was **100% homing, 0% naming**: 5 of
11 point at a VA pinned to a unit that does not define the symbol.

So the rule generalises rather than narrowing: **never author a mangled name,
read it from the obj's COFF symbol table** — and it was still worth verifying
all 11, because the verification is what proved the failure mode was elsewhere.
laneAO-a1 committed the primitive as `scripts/harvest/homing_reverse.py`
(VA → `.pdata` size → reloc-masked scan of all 1024 objs → exact COFF names +
retail-side ICF hit count; ~5 s for 11 VAs).

| verdict | n | |
|---|--:|---|
| FREE-AS-GIVEN, landed | 6 | +1 strict each |
| DEFERRED — needs splits | 3 | reported, not applied (§5) |
| REFUTED — ICF coin flip | 2 | |

Both refutations are ICF ambiguity, and **one is backwards**: the proposed
`?resize@vector<TrackerPlayerDisplay>` at `0x822c87b0` has a byte-identical
sibling `0x826d2488` already mapped `vector<Unlockable>::resize` *and* sitting
inside `band3/game/Tracker.cpp`'s span — so the Tracker instantiation is the one
in Tracker's range and the proposal has the two inverted.
`??0WiiFriendsScreen` is a **Wii-only** `UIPanel` subclass carried into the tree
from the rb3-Wii oracle; the 360 retail binary has no such class.

★**A hand-read of unit ownership is not a substitute for `span_predictor.py`.**
I hand-derived the ownership table and called `??0DxLight@@IAA@XZ` a mismatch
because `DxLight.cpp` is absent from `splits.txt`. Wrong:
`system/rnddx9/CubeTex.obj` genuinely defines it (scatter-COMDAT), and it paid
+1. Hand-reading errs in the scatter direction — which is the direction that
pays. Run the tool.

★I also wrongly invoked the 0x828–0x82C vendor guard against
`0x82b90670`. The owner's guard is "exclude **`auto_03_*` spans** in
0x828–0x82C", not "all addresses there": the map already holds **7,259 entries
inside that window** and they are ordinary engine/UI code. The refutation stands
on ICF ambiguity and the class not existing, not on the address.

### 2b. The rest of the backlog was stale

Audited read-only against the live map:

| item | verdict |
|---|---|
| `HasCampaignKey@Campaign` → `GetCampaignKey`'s VA (false 48.9%) | **already fixed** (lane-AI `ccada9ac`/`43112f53`) |
| `GetHarmony@Stats` on a float getter at `+0xb8` | **already fixed** (`96a4069f`) |
| `SetFrameEx@HamCamShot` cross-class paired | **already fixed** (`96a4069f`) |
| "the map holds 11 `0X`-uppercase keys" | **1**, and it is *correct and harmless* |
| "2 stale-vs-carve + 5 size disagreements" | superseded — re-derived fresh (§5) |

★The `0X`-uppercase lint is **dead as a defect signal here.** Exactly one such
key remains (`0X82266BF8` → `?__stl_throw_length_error@stlpmtx_std@@YAXPBD@Z`),
its name is correct, its VA is inside no pinned range, and
`scripts/obj_target_symbol_renamer.py:load_address_map()` lowercases every key
before parsing — so the casing is *provably* invisible to the consumer. The
"10-of-18 on defects" measurement was a property of a former, larger population,
not of the casing. Leave it.

## 3. What paid: three map-owner waves

### 3a. Compiler-ordinal deletes — **+3, 0 losses** (`ed32aed4`)

The map held **5 entries naming MSVC compiler ordinals** (`__catch$97938`,
`__catch$295968`, `__catch$55351/55505/55648`). The ordinal is a per-build
counter, so our numbering can never agree with retail's; stamping one on a
target symbol displaces the `pair_funclets_by_bytes` byte-signature pairing that
would have scored it for free. All 3 gains are the freed funclets in
`GemTrackResourceManager`. The `UILabel` and `Font` deletes were inert.

A clean 1-for-1 confirmation of **unmapped beats wrongly-mapped**.

### 3b. Structurally-unpairable eviction — **+2, 0 losses** (`ca14e98f`)

New scan, and the more reusable result of this lane. For every map entry:
resolve the unit owning its VA from `splits.txt`, then ask **that unit's
`base_path` obj** (from `objdiff.json`) whether its COFF symbol table defines
the mapped name. If not, objdiff can never pair it. **456 entries fail.**

Two calibration points, both measured, both load-bearing:

* ★The naive global form — "no obj *anywhere* defines this name" — has a **22%
  false-positive rate** (100 of 456 read 100%). **Every FP is an
  anonymous-namespace `?A0x<hash>` name**, because objdiff matches on
  `normalized_name` and the hash difference is not a pairing failure. Excluding
  `?A0x` removes the entire FP class.
* It must be asked **per-unit, never globally** — scatter-COMDATs mean a symbol
  is often defined by a unit you would not guess from its name. Same recall bug
  as the `??0DxLight` error in §2a.

★**Evicted only the low-information 133 of 330 candidates.** The other 197 name
real member functions (`MidiInstrument::SynthPoll`, `GamePanel::SetGameOver`, …)
— correct identities we simply do not compile yet, and forward-useful the moment
their source is ported — and were deliberately **restored**. Narrowing from 330
to 133 **cost nothing: both sets measure the same +2.** The gain comes entirely
from freed `vector<CartRow>` / `vector<bool>` instantiations whose
template-argument class is unported, i.e. mechanically re-derivable identity.

Generalisation: **"delete the wrong entry" is not the same lever as "delete the
not-yet-realisable entry".** Only the first is free. Evicting 197 correct
identities would have destroyed forward evidence for exactly +0.

The evicted set is preserved in
`docs/plans/laneAO-map-evictions-2026-07-26.json`; its 9 `MOVE`/`MOVE-AMBIG`
rows are splits requests, not map defects.

### 3c. Coupled signature repoints — **+2, 0 losses** (`d0be1c4d`)

Both source leads changed a mangled name, so **map and source had to land
together** — the source commits alone are fuzzy-negative (`NewStreamDecoder`
measured 96.92% → 0.0% with the source change and no map edit).

### 3d. Hygiene deletes — **+0** (`dfed70a6`)

`?PreSave@CharClip@@` at `0x8237fbd0` sits 4 bytes inside `.obj
except_data_8236D69C`; `gRevs` at `0x82cbe480` is a `.data` object. Neither can
ever be a function rename.

★**A third proposal in the same batch was refuted, and it would have HURT.** It
wanted to repoint `?SetCharacter@HamPlayerData@@` from `0x8237fbd8` to
`0x8237fbd4` on the arithmetic that the key sat "one instruction past the real
start". Reading the carve instead of the arithmetic: `fn_8237FBD8`
(`HamPlayerData.s`, 0x9C) starts **at** `0x8237FBD8` with a real `mflr r12`
prologue — the existing mapping is right — while `fn_8237FBD4` (`CharClip.s`,
0x2C) opens `subi r31,r12,0x80 / mflr r12`, the MSVC PPC **EH-cleanup funclet**
signature, and **is already at 100.0%**. The repoint would have named an
already-matched anonymous funclet (measured −13 elsewhere) *and* stolen a
correct name off a real function. The two carves genuinely overlap — a dtk
over-carve in `CharClip.s`, a splits matter.

## 4. The two source leads: one refuted-then-solved, one verified-and-bigger

### `BandWardrobe::SetPlayMode` — lead **REFUTED**, +1 anyway

The claimed missing `ObjRefConcrete<BandCamShot,ObjectDir>` destructor does not
exist: the diff is **2 deletes and zero inserts**, no ctor, no `__EH_prolog`, no
extra `.pdata` funclet, and the receiver is loaded from the global
`TheBandDirector`, not a stack slot. The real missing call is
`TheBandDirector->SetCharacterLipSyncs()`.

★**The `ObjRefConcrete` name came from a bad map entry** — `0x8228dd38` was
mapped to that destructor but is a **0x244-byte BandDirector method** (the
per-character lip-sync loop; strings `"guitar"`/`"bass"`/`"drum"`/`"mic"`). The
mis-binding cost a real mispair: `default/BandDirector`'s
`??1?$ObjRefConcrete<BandCamShot,ObjectDir>` read **13.01%** because our
0x74-byte dtor was being diffed against a 0x244-byte method. Repointed; the
genuine dtor at `0x822b0c28` remains. **A wrong map entry generated a false
source lead** — worth remembering when a lead names a type.

### `VorbisReader` 5th parameter — **VERIFIED**, and one level bigger

Retail's ctor at `0x82BB41A0` reads `r7` before any def and stores it as a byte
to `this+0x119` (`mr r25,r7` → `stb r25,0x119`). Its only caller,
`Synth360::NewStreamDecoder`, reads `r7` at entry and forwards it unchanged, and
*its* callers load the value with `lbz r7,0xe8(this)` = `StandardStream::
mFloatSamples` — so the parameter is a `bool`, and the fix propagates up a
level. objdiff corroborated independently before any edit.

★**Both oracles were wrong and were correctly overruled by the retail bytes.**
DC3's map has the 4-param mangling and rb3-Wii declares 4 — **RB3-360 retail
added one.** A textbook case of the DC3-is-newer / Wii-is-a-different-cut
caveat: when the oracles agree with each other and disagree with the bytes, the
bytes win.

## 5. Reported, not applied — splits requests (out of lane scope)

1. **`0x827a5508` `?Init@SongPreview@@QAAXXZ`** — strongest.
   `StorePreviewMgr.cpp` holds `.text 0x827A5508-0x827A5664`, length **exactly**
   our 348-byte function, in the gap between two SongPreview.cpp pins. A clean
   whole-range MOVE, and SongPreview's ownership of the neighbourhood is now
   confirmed by the `0x827a5338` match this lane landed.
2. **`0x823423e0` `?Save@FlowWhile@@`** (88 B) — inside `BandLabel.cpp .text
   0x823423DC-0x823424A4`; needs a carve, not a move.
3. **`0x826fe7f0` `?OnPassthrough@Synth@@`** (180 B) — inside
   `PracticeSection.cpp .text 0x826FE7F0-0x826FE8A8`; cheap, because **5** of our
   objs define the symbol.
4. **`0x82BB41A0..0x82BB439C`** — the now-correct `VorbisReader` ctor is
   unpinned and unmapped, so the verified fix cannot score.
5. **`fn_826C44F8`** (`GemPlayer.cpp`, 0x15EC) — one un-split blob holding 5
   DirectInstrument/MidiInstrument accessors the current carve has not split out.
6. **`CharClip.s` / `HamPlayerData.s` overlap at `0x8237FBD4`/`0x8237FBD8`** —
   a dtk over-carve (§3d).

★**The stale-vs-carve class regenerates.** A `96a4069f` checkpoint recorded
"ZERO stale, ZERO size-mismatch"; laneAM's `a0725939` (+1,600) reshuffled
`splits.txt` and reintroduced **20 VAs absent from `symbols.txt`**, 14 of them
inside pinned ranges. This class is a **function of splits churn** — re-derive
it after every large splits wave rather than trusting a checkpoint.

## 6. Counters, before → after

| counter | before | after | |
|---|--:|--:|---|
| strict (honest, 2 builds) | 36,071 | **36,084** | +13, 0 losses |
| map entries | 24,241 | 24,107 | −134 net |
| duplicate VAs (case-insensitive) | 0 | **0** | invariant held |
| compiler-ordinal names | 5 | **0** | |
| argreg `MISPAIR_FORWARD` | 27 | 27 | untouched ground (§7) |
| argreg `INVERSE_WEAK` | 20 | 20 | |
| argreg sub-100 named-paired pool | 1,765 | 1,763 | |
| trust-audit contradicted | 90 | 90 | of 12,127 checked |

`argreg --fp-control` over the 15,033 strict-100 population: **1**
`MISPAIR_FORWARD` (0.0073% FP) and 65 `INVERSE_WEAK` (0.48%) — the tool remains
a proof of wrongness. Its `--self-test` **fails 1 of 3 canaries**, but the
failure is a **stale hardcoded canary in the script**
(`ObjRefConcrete<CharClip,ObjectDir>::SetObj` no longer instantiates anywhere in
`report.json`), not a regression.

The trust audit's contradicted count is **90 of 12,127**, down from a historical
298 of 10,906 — and the `operator<<(TextStream&,…)` / debug-printer artifact now
fires **0 times**, so that exclusion is fully resolved upstream. **48 of the 90
are a suspected untagged fourth arbitrary class** (`StaticClassName` ×42 at a
uniform 88 bytes / 100%, `Type` ×3, `ByteCode` ×3) with the same structural
signature as `_icf_arbitrary`/`_bijection_arbitrary`. If confirmed, the fix is
**bulk-tagging, not renaming** — any assignment within an arbitrary class is
equally arbitrary, and "unmapped beats wrongly-mapped" only applies when a
*specific* binding is provably wrong. The residual ~42 are heterogeneous
singletons and are the honest unadjudicated defect population.

Provenance markers unchanged and untouched: `_icf_arbitrary` 25,
`_bijection_arbitrary` 1,207, `_denylist` 3.

## 7. What remains

* **27 argreg-proven `MISPAIR_FORWARD` entries** — the count is **identical
  before and after this whole lane**, so it is untouched ground and the single
  best-evidenced remaining map-defect set. Argreg is a proof of wrongness (0 FP
  in 12,183): e.g. `?Register@OutfitConfig@@SAXXZ` is declared 0-param static
  void but its mapped body reads `r3`, `r4` and `r5`. ★**Prefer repoint over
  delete** — they score 40–96% fuzzy, so blind deletion is a fuzzy regression
  even though the credit is fake. Set in `/home/free/tmp/laneAO_argreg_after.json`.
* **48 suspected untagged-arbitrary entries** — confirm with a byte-diff across
  2–3 `StaticClassName` siblings, then bulk-tag. Do not rename.
* **323 remaining structurally-unpairable entries** (456 found − 133 evicted) —
  correct-but-unported identities. Not defects; they realise when their source
  is ported. Do not evict them for score.
* The `??2Node@?$ObjPtrList<T>::operator new` family and
  `ObjRefConcrete::SetObj` argreg hits look like further untagged ICF-twin
  families — cross-reference DC3/rb3-Wii before touching.
* `BandDirector`'s tail layout is wrong by ~0xC (`OnFileLoaded` at 68.06%,
  3816 B) — retail evidence for `0x118`–`0x130` is recorded in commit
  `ba773bd8`; needs its own A/B lane because the tail is load-bearing for 39
  vbtable/vtordisp functions.
* `VorbisReader`'s `mHdrSize` is likely at `0xec`, not `0xc0`; currently
  match-neutral because no pinned function touches either.

## 8. The displacement channel (laneAO-a5) — +33 net-new, and an accounting trap

`map_displace_round.py` was run against a **freshly regenerated whole-tree homing
scan** (1024 TUs, current obj state) with every measured guard armed:
`--strict-guard` refused 359 holders already at strict-100, `--pays-only`
(span_predictor) refused 12 WRONG-UNIT + 1 UNPINNED, contested claims refused
244, and the ICF-twin-on-non-PAYS shape (measured −23/+0) **never fired**. Two
filters the tool does not apply were added by hand: the map's own `_denylist`
(it re-proposed `0x82553fc8 ?Terminate@RndMat@@SAXXZ`, a known argreg-refuted
FP) and the "never name an already-matched anonymous funclet" −13 rule (all 21
free VAs were verified to read 0.0% first).

★**Negative control:** an independent from-scratch reloc-masked identity
recheck accepted 24/24, and **the same checker fed those 24 VAs with the names
rotated by one accepted 0/24.**

Waves measured +24 and +15 in the lane's own worktree. ★**But +6 of the +24 are
the same six bindings the handoff lane had already landed** (§2a) — the
displacement resolver **independently rediscovered all six, character for
character, from a different starting point.** That is a cross-validation of the
whole byte-identity channel worth more than the matches themselves, *and* an
accounting trap: **39 raw − 6 overlap = +33 net-new**, which is exactly what the
integrated measurement shows (36,674 → 36,707). Two lanes mining the same
evidence channel will double-count unless the union is measured, not the sum.

★**Merge hazard found while integrating: an evict-then-restore round trip can
RESURRECT an entry a concurrent wave deliberately displaced.** `0x827c7bb0` was
displaced `??_GBandSong` → `??_GSong` on byte-identity evidence; this lane's
restore list (§3b), computed against the *pre-displacement* map, put
`??_GBandSong` back, producing a duplicate VA. Kept the displacement. When two
map waves merge: re-derive the restore set against the merged map, and
re-assert the duplicate-VA invariant **on the raw lines** — `json.load` silently
keeps the last of a duplicate key and hides the problem entirely.

The lane was cut short by an infrastructure error before reporting a fixpoint,
so the `--include-free` splits/no-splits split and the 27-entry argreg target
set (§7) remain open.

Final invariants on the merged map: **24,131 address entries, 0 duplicate VAs,
0 compiler-ordinal names.**
