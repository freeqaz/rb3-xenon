# The empty-EXTRN sweep: the masked wrong-callee class did NOT recur, the control fired 92 times, and the 20 hits are a different defect — lane W14-B, 2026-09-14

**Date** 2026-09-14 · **Branch** `w14-empty-extrn-sweep` off `main@90be524c` ·
**Worktree** `~/tmp/wt-w14-b` ·
**Ruler** `functionRelocDiffs=name_check` (graded) — but see §6: **this lane
changed no code, so no ruler reading is load-bearing.**
**Tool** `tools/empty_extrn_sweep.py` (committed; re-runnable).

## 0. Result in one line

The class this lane was funded to find — a **wrong callee masked by an empty
EXTRN stub**, the `?Terminate@Rnd@@UAAXXZ` shape — **did not recur: 0 instances
in 112 examined call sites.** The screen fired 20 times (17.9%), and all 20 are
a *different, real* defect class: **our body is empty where retail's is not.**

★ The in-pass positive control fired **92 times**, so this is a bounded negative
from a working instrument, not a silent one.

## 1. Populations, with every denominator stated

| population | count | how derived |
|---|---:|---|
| trivially-empty out-of-line defs in `src/` (ex `xdk`, `stlport`) | **191** | source regex, re-derived (§1.1) |
| distinct 4-byte-`blr` COMDATs in our compiled objs | **7,547** | COFF, `function_bodies_ext` |
| …of those, **directly called** (REL24) from our own objs | **195** | COFF relocation table |
| direct call sites to an empty callee | **1,843** | COFF relocation table |
| **call sites EXAMINED** (resolve to a retail destination) | **112** | paired via `objdiff.json` |
| — not examined: caller absent from the target obj | 1,645 | unpairable by name |
| — not examined: our obj has no target obj | 77 | one of the 162 unpaired objects |
| — not examined: no reloc at the paired offset | 9 | §5 |

⚠ **1,843 is the candidate ceiling and 112 is the denominator of every
precision figure below.** 94% of the call sites are **unpairable**, because
objdiff pairs target↔base **by name** and retail's counterpart of most of our
callers is an anonymous `fn_8XXXXXXX`. This is the standing finding that an
unpaired row *"is invisible to callee adjudication and looks IDENTICAL to a row
with nothing wrong"* — it bounds this sweep's reach and no amount of source work
changes it.

### 1.1 The briefed 182 is approximately right; I measured 191

Re-derived rather than inherited, as instructed. My regex finds **191**; the
brief said 182. The area breakdown tracks the brief closely — game **41**
(exact), moviebink 9, char 8, os 7, bandtrack 5 (all exact), meta_band 18 (vs
22), world 15 (vs 13), hamobj 14 (vs 13), bandobj 13 (vs 10) — and the brief
listed only its top 12 areas, which sum to 163 of 182. The ~9-row gap is regex
sensitivity, not a substantive disagreement.

★ **But the source count is the wrong instrument and I did not build on it.**
The property the discriminator needs is *"our callee compiles to 4 bytes of
`blr`"*, which is a fact about the **object**, not the source text. Measured at
the object level the population is **7,547**, of which only **195** are ever
directly called — and 195 is what independently corroborates the ~182–191
figure, from a completely different direction.

## 2. ★★ The control, and why it looked like a total failure

**92 of 112 examined sites pair to a retail destination that is itself a bare
`blr`.** That is the control population, and it fires.

⛔ **It nearly got thrown away.** Every trivially-empty function in retail is a
4-byte `blr` COMDAT **with no relocations**, so they are all byte-identical and
**ICF folds them into one survivor**. All 92 control rows resolve to the *same
address*, `0x826c3888`, which the map names — arbitrarily, as ICF survivor names
always are — `??$?0H@?$StlNodeAlloc@V?$_List_node@H@stlpmtx_std@@…`.

So my first validity check, *"does retail's destination name equal our callee's
name?"*, returned **0 of 92 (0%)** and a single constant address. That reads
exactly like a catastrophically broken scan. **It is instead the correct answer,
and 0% is what a working instrument MUST report here** — the fold destroys the
name by construction.

⚠ **Do not re-derive that check.** A name-agreement test on this control is
guaranteed to fail on a healthy tree, which makes it a gate that cannot pass —
the mirror image of the "gate that cannot fail" hazard this project already
records.

The two checks that **do** discriminate, and both passed:

1. **Caller body length, ours vs retail: 111/112 equal.** If the offset-based
   pairing were bogus, lengths would disagree constantly.
2. **Independent re-decode from retail bytes.** For all 20 suspects I recomputed
   the branch destination straight out of `band.exe` (opcode 18, sign-extended
   `LI`, `AA`/`LK`) from the *caller's own* map address, bypassing the object
   and map chain entirely: **20 of 20 agree exactly** with the relocation-derived
   address. 0 mismatches.

### 2.1 Corroborated independently by the coordinator

An independent enumeration of every retail `bl` destination whose body starts
with a bare `blr` found **4 destinations / 1,110 inbound calls**:
`0x826c3888` (1,099 callers, named), `0x82516320` (8), `0x82aadf90` (2),
`0x82aadf98` (1). My 92 control rows all land on `0x826c3888`. Two independent
methods, same survivor.

⇒ ★ **Our survivor is NAMED**, so the proposed shortcut *"name the ICF survivor
and the whole class closes at once"* — which held on dc3-decomp, where the
survivor is `OnlyReturns` — **does not exist here and must not be attempted.**

### 2.2 Where the `Rnd::Terminate` masking actually came from

Not from the survivor being unnamed. Retail's `Rnd::Terminate` called
`0x82466080`, a real **80-byte** `RELEASE(global)` — not a trivial body at all —
and *that address* was unnamed. **The masked class is "our empty callee pairs
against a retail destination that is BOTH non-trivial AND unnamed."** That is
narrower and more specific than "unnamed survivor", and it is the filter this
sweep applies.

⇒ Control rows pair against a **named** symbol and are therefore charged or
alias-forgiven — **not** placeholder-forgiven. The healthy case is visible to
the score; only the masked case is silent.

## 3. The 20 hits, and the reclassification that is the lane's main result

| class | n | metric-visible? | is it the briefed wrong-callee class? |
|---|---:|---|---|
| `UNIMPLEMENTED_BODY` (thunk-pinned) | **12** | 6 charged / 6 masked | **No — structurally impossible** |
| `SUSPECT_MASKED` (non-thunk, dest unnamed) | **4** | no — silent | No — same callee, absent body |
| `SUSPECT_CHARGED` (non-thunk, dest named) | **4** | yes — already charged | 1 arguable, 2 map-defect suspects, 1 discarded |
| **wrong callee, masked (the target class)** | **0** | — | — |

### 3.1 ⛔ An adjustor-thunk caller CANNOT be a wrong-callee hit

12 of the 20 have an MSVC **vtordisp adjustor thunk** as the caller. Their retail
shape, verified in the bytes:

```
+0   8163fffc   lwz  r11, -4(r3)
+4   7c6b1850   subf r3, r11, r3
+8   4bffef90   b    <target>          <-- the REL24 my scan reads
+12  00000000   (padding)
```

A thunk named `?Save@BandList@@$4PPPPPPPM@A@AAXAAVBinStream@@@Z` forwards **by
definition** to `BandList::Save`. Its destination is therefore *pinned by its own
name* and cannot be "the wrong function". These rows mean exactly one thing:
**retail's body is real and ours is empty.**

⚠ Conflating them with wrong-callees would have overstated the class the lane was
funded to find by **12 of 20**. They are classified apart in the tool.

⚠ Note these thunks are 12 bytes and differ in their branch displacement, so —
unlike the empty bodies — they do **not** fold, and their map names are reliable.

### 3.2 The 4 genuinely masked, non-thunk rows

Silent by construction: retail's destination is non-trivial **and** unnamed.

| retail | size | unit | our (empty) callee |
|---|---:|---|---|
| `0x8258fe88` | 176 B | `band3/meta_band/ProfileMgr` | `AccomplishmentProgress::HandlePendingGamerRewards` |
| `0x8268f6c8` | 168 B | `band3/tour/TourChar` | `CensorString(String&)` |
| `0x82b98d78` | 96 B | `band3/bandtrack/GemTrack` | `GemManager::Ignore(int)` |
| `0x8253abb8` | 96 B | `band3/meta_band/SongSelectPanel` | `MusicLibrary::OnLoad()` |

★★ **These are NOT porting mistakes — our source is faithful to its oracle.**
All four are `{}` in **rb3-Wii too** (`Defines.cpp:265`,
`AccomplishmentProgress.cpp:686`, `MusicLibrary.cpp:202`, `GemManager.cpp:1271`).
Retail **Xbox 360** simply has bodies the Wii **dev** build never had, and the
names say why: *Gamerpic* rewards and *string censoring* are Xbox Live features
with no Wii counterpart.

⇒ This is the **"we do not hold the body"** class, reached by a new route. Each
is a real decomp job, not an edit — deliberately **not** attempted here (§6).

### 3.3 The 4 charged non-thunk rows — visible to the score already

* **`Object::AddSink` / `Object::RemoveSink`** (`OnlineID`, retail 316 B / 300 B).
  Retail calls `MsgSource::AddSink`/`RemoveSink`; we call `Hmx::Object::AddSink`.
  Our whole sink system is `#ifdef HX_NATIVE`-gated (`Object.cpp:669,687`), so it
  compiles to `blr` and **every message subscription is a no-op in the match
  build**. This is a class-hierarchy divergence, not a mistyped call, and it is
  **charged** — the score can already see it. Large; not this lane's to fix.
* **`__destroy_aux<GameGem>` → `?IsLoaded@UIPanel@@UBA_NXZ`** and
  **`_M_fill_assign` → `_Vector_base` ctor**: nonsense destinations. The second
  is the one row with `lenmatch=False`, so its offset pairing is invalid and I
  **discard it**. The first is a map-defect suspect (§4).

## 4. Secondary finding: 6 map names contradicted by a vtordisp thunk

6 of the 12 thunk rows have a **named** destination, and in every case the name
is a **different method of the same class**:

| thunk (map) | branches to | map calls it |
|---|---|---|
| `?Copy@BandTrack@@$4…` | `0x82351fc0` (1580 B) | `?SyncProperty@BandTrack@@` |
| `?Highlight@Waypoint@@$4…` | `0x823dce48` (356 B) | `?Load@Waypoint@@` |
| `?Save@RndMultiMeshProxy@@$4…` | `0x824813b8` (316 B) | `?SetType@RndMultiMeshProxy@@` |
| `?Save@CrowdMeterIcon@@$4…` | `0x822b91d0` (312 B) | `?PostLoad@CrowdMeterIcon@@` |
| `?Load@BandTrack@@$4…` | `0x822ead70` (104 B) | `?PostLoad@GemTrackDir@@` |
| `?Save@BandTrack@@$4…` | `0x8234fca8` (68 B) | `??_GBandTrack@@UAAPAXI@Z` |

A `Save` thunk cannot forward to a deleting destructor. Since the thunks are
verified real vtordisp thunks *by their bytes*, and the thunk→target relation is
definitional, **one of the two map names in each row is wrong** — most likely the
destination's.

⚠ **I did not adjudicate which side is wrong.** Settling it needs the retail
vtable per class, which is a lane of its own. I record the contradiction and the
evidence, not a verdict — and note the within-class regularity (every wrong name
is a sibling method) is suggestive of a systematic boundary shift, which is a
hypothesis, **not** a measurement.

⛔ **And do NOT "fix" this by naming the 6 unnamed destinations in §3.2/§4.**
Naming an anonymous address converts a **forgiven** call site into a **charged**
one. Because our bodies are empty and retail's are not, naming first would
*cost* bytes — precisely the ordering error W12-C priced at −180 B for
`0x82466080`. **Fix the body first, then name.**

## 5. The 9 unpaired-offset rows — mostly an artifact, one live question

9 sites have no REL24 at the paired offset in retail. **8 of 9 have
`our_len != tgt_len`**, so the offset pairing is simply invalid there and they
say nothing. The exception is `band3/game/TrainerGemTab.obj` off=40, where both
bodies are **96 bytes** yet retail has no call at that offset (ours calls
`~GameGem`). That is a possible spurious-call divergence; **not chased.**

## 6. What this lane deliberately did NOT do

* **No `src/` change, no `config/` change, and therefore no A/B.** Nothing was
  edited, so there is no delta to price and `ab_measure` was not run. Reporting a
  Δ would be reporting noise. Every defect found is either a multi-function decomp
  job (§3.2), an architectural gating divergence (§3.3), or a map adjudication
  needing vtable work (§4) — none is a `Rnd.cpp`-shaped one-line repair, which is
  itself the lane's finding.
* **Did not name any of the 10 unnamed retail destinations** — wrong order, §4.
* **Did not adjudicate the 6 contradicted map rows** — §4.
* **Did not chase the TrainerGemTab row** — §5.
* **Did not put findings in source comments.** A comment asserting what retail
  does is not evidence (four have been refuted by bytes), and a comment-only
  commit has already broken the native link once via `ScatterIncludes`. The
  findings live here and in the JSON.

## 7. Handoffs

1. **4 Xbox-only bodies to write** (§3.2), sized and addressed:
   `AccomplishmentProgress::HandlePendingGamerRewards` (176 B, `0x8258fe88`),
   `CensorString` (168 B, `0x8268f6c8`), `GemManager::Ignore` (96 B,
   `0x82b98d78`), `MusicLibrary::OnLoad` (96 B, `0x8253abb8`). The rb3-Wii oracle
   is `{}` for all four and will **not** help — these need retail-byte decomp.
   Expect **Δ0 on the metric** (the sites are forgiven today); land on accuracy.
2. **6 `Save`/`PreSave`/`Copy` bodies** behind unnamed destinations (§3.1):
   `BandList::Save` (388 B), `WorldInstance::PreSave` (136 B),
   `BandScoreboard::Save` (128 B), `OverdriveMeter::Save` (96 B),
   `BandSwatch::Save` (80 B), `EventAnim::Save` (68 B).
3. **6 contradicted map rows** (§4) — needs per-class retail vtables.
4. **The `HX_NATIVE`-gated sink system** (§3.3) — charged, so it is visible to the
   score and can be prioritised against other charged work normally.

## 8. Gates, verbatim

