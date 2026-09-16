# W16-EH — `Game::OnMsg(ButtonDownMsg)` decoded, and `Shuttle::SetActive` defined

**Branch** `w16-eh` (off main `80c830d0`). **Ruler** `name_check` (graded), objdiff 4.2.9.
**Denominators read, not inherited:** `total_code` **10,247,068**, `total_functions` **69,240**.

**Headline: +0 bytes, +0 functions, and that was the pre-registered prediction.**
Two source changes landed on evidence, not on score. One of them fixes a real
behavioural bug; the other makes a fold provable that previously could not be
compared at all. A third deliverable is a **refusal** to install an alias, with the
root cause of the blocking tool defect diagnosed and its blast radius measured at zero.

---

## 0. Scorecard — pre-registered before measuring

| # | prediction | outcome |
|---|---|---|
| **P1** | The two ButtonMsg handlers are prologue-twins differing in one instruction (EF's claim), so the DOWN body can be written by copying UP and flipping `--` to `++` | ⛔ **MISSED — and this was the lane's most valuable finding.** They are *not* twins. See §2.1 |
| **P2** | Writing only the proven head lands the 1,664 B row in the **8–20%** band, **+0 B**, whole binary unchanged | ✅ **HELD.** fuzzy 3.2380 → **8.009615**, mpn 3.3702 → **8.262019**, Δbytes 0 |
| **P3** | Whole-binary A/B: Δmatched 0, Δcode 0 B, ≥2 TUs recompiled in leg B, **no** `ALIAS_SUSPECT` | ✅ **HELD.** Δ0/Δ0, 3 TUs recompiled, no alias fired |

P1's failure is the point. Had I inherited it, I would have written a body that is
wrong in three independent ways and scored it as progress.

---

## 1. The A/B (authoritative, settled)

`tools/ab_measure.py --worktree ~/tmp/wt-w16-eh --from-dirty`, patch `43de5c6d98f8ded2`,
objdiff-cli pinned across both legs (`sha256:c1b7d95240a35cd6`).

```
leg A: matched=43956 masked=23224 honest=20732 matched_code=4,123,700  code%=40.242733
leg B: matched=43956 masked=23224 honest=20732 matched_code=4,123,700  code%=40.242733
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0
Δfuzzy=+0.000795pp  (50.006115 -> 50.006910)
units at 100% [mpn]:   189 -> 189   (0 in, 0 out)
units at 100% [fuzzy]: 169 -> 169   (0 in, 0 out)
```

**Leg A reproduced the brief's baseline to the digit** (43,956 / 4,123,700). That is the
control that matters: a leg A *missing* the briefed baseline is the signature of the
worktree renamer race, not of the lane's work, and it would have voided the run.

⚠ **The `none` control printed `NOT_APPLICABLE`, which is not a clearance.** With
`source` in the patch it cannot adjudicate the alias shape, because default-UP /
none-FLAT is *also* the wrong-callee-fix signature. It declined to speak. I am not
citing it as evidence, and it is correct here only because **I installed no alias.**

### Row-level (live `report.json`, same tree)

| row | size | fuzzy | mpn |
|---|---:|---:|---:|
| `?OnMsg@Game@@...ButtonDownMsg@@@Z` | 1,664 | 3.2380 → **8.009615** | 3.3702 → **8.262019** |
| `?OnMsg@Game@@...ButtonUpMsg@@@Z` | 156 | 98.589745 | 100.0 (unchanged, EF's) |
| `?OnSetShuttle@Game@@...` | 260 | 99.92308 | 99.92308 (unchanged — no alias) |

Unit `default/band3/game/Game`: `matched_code` 32,332 / 40,844, fuzzy 90.365295 → **90.56459**.

⛔ **All of this is worth exactly 0 bytes** — `matched_code` is all-or-nothing per row.
Stated here because the brief demanded the work be priced honestly, and 3.2 → 8.0 on a
1,664 B row is the most seductive kind of nothing.

> ⚠ **Instrument note, recorded because it nearly produced a false number.** Reading
> these rows as `f.get('measures',{}).get('fuzzy_match_percent',0)` returns a confident
> **`0`** for every row. The keys are **flat on the function object**
> (`fuzzy_match_percent`, `match_percent_normalized`); there is no `measures` dict at
> function level, and protobuf-JSON omits defaults, so a wrong key and a genuine zero
> are indistinguishable. It was caught only because the *unit*-level read in the same
> pass returned real values. On a row you hope you improved this reads as a disaster;
> on a stubbed row it reads as confirmation.

---

## 2. Three refutations of the brief / W16-EF

The brief instructed that these claims be tested literally rather than inherited. All
three failed. **Two of them are repeated verbatim in
`docs/plans/ROADMAP_GAP_TO_TARGET_2026-09-01.md:2369`** (the W16-EF table row) and
should be corrected there by whoever next edits that file.

### 2.1 ⛔ "Instruction-for-instruction identical prologues differing in exactly ONE instruction" — **FALSE**

This is the claim that the DOWN handler is the UP handler with `addi r9,r9,1` swapped
for `addi r9,r9,-1`. Measured on retail bytes, the two bodies differ structurally:

| | `fn_82679900` (ButtonUp) | `fn_8267B808` (ButtonDown) |
|---|---|---|
| counter update | guarded by `cmpwi cr6,r10,0` / `ble cr6` (a `> 0` test) | **unconditional** |
| loads of `0x48(this)` | **two** — re-loads after the test | **one** |
| instruction count for the update | 9 | 5 |
| operand order | `lwzx r10,r10,r11` | `lwzx r9,r11,r10` (**reversed**) |
| frame | 0x80 | **0xe0** |
| save range | `__savegprlr_28` | `__savegprlr_23` |
| FPRs saved | none | **f30/f31** |

⇒ **A lane that copies the UP body and flips the sign produces wrong code**, and would
be misled by a plausible partial score. The prologues are not twins; they are the
press/release *pair*, which is a semantic relationship, not a syntactic one.

### 2.2 ⛔ "The `OnSetShuttle` call site passes `this+0xe0`" — **FALSE**

Retail:

```
8267d1c4  mr   r4, r30        ; the `active` bool
8267d1c8  lwz  r3, 0xe0(r31)  ; LOADS the pointer at this+0xe0
8267d1cc  bl   0x826f07b8
```

`lwz` **loads a pointer**; it does not form an interior address. EF's phrasing describes
an *embedded* `Shuttle` object. Our source has `Shuttle *mShuttle` — a pointer — and is
**right**. The verdict ("our source is correct") is unchanged; the stated reason was wrong,
and the wrong reason is what a future lane would have acted on if it tried to "fix" the
member into an embedded object.

### 2.3 The construct list is incomplete

EF names `audition_cam_toggle`, `deploy_if_possible`, `audition_keyboard_synth_volumes`,
and `DirectInstrument::Enable`/`Disable`/`SetVolume`. Three further Symbols are
referenced by the body and were not named:

- `audition_jump_forward_ms` (`lbl_820DD320`)
- `audition_jump_back_ms` (`lbl_820DD33C`)
- `audition_jump_end_buffer_ms` (`lbl_820DD304`)

---

## 3. Complete structural decode of all 1,664 B

This is the hard part of the reconstruction and it is **done**; what is missing is one
identification (§3.2), not understanding. Recorded in full so the next lane does not
re-derive it.

**Head** (written to source, proven instruction-for-instruction):

```
if (mProperties.mUnkTU5_movieSync)            // this+0x2f
    pad = msg.GetUser()->GetPadNum();
    if (pad >= 0 && pad < 4)
        if (JoypadGetPadData(pad)->mType == kJoypadAnalog)
            ((int*)mUnkTU5GuidePitch)[pad]++;  // [0x48(this)] + pad*4
```

**Gate before the switch:** `!mOvershellWantsPause && !mRealtime && counter == 1`,
then `bool stopped = (mMusicSpeed == 0.0f)`.

**Dispatch:** `switch (msg.GetButton())` — MSVC sparse-switch lowering, i.e. a
value-mapping cascade to a dense index followed by `mtctr` and a chain of `bdz`.

| idx | button | body |
|---|---|---|
| 0 | `kPad_L1` (2) | static float ← `TheGamePanel->Property("audition_jump_back_ms")->Float(0)`; `Jump(max(now - back, 0), true)` |
| 1 | `kPad_R1` (3) | statics `audition_jump_forward_ms` + `audition_jump_end_buffer_ms`; `limit = TheSongDB->GetSongDurationMs() - endbuf`; `Jump(min(now+fwd, limit), true)` if `now < limit` |
| 2 | `kPad_DLeft` (15) | `ns = (mMusicSpeed==0.25f) ? 0 : mMusicSpeed*0.5f`; if `ns==0 && !stopped` → `mMusicSpeed=0; mGameWantsPause=true; UpdatePausedState(1,1,1)` else `SetMusicSpeed(ns)` |
| 3 | `kPad_DRight` (13) | `ns = stopped ? 0.25f : mMusicSpeed*2.0f`; if stopped → `mGameWantsPause=false; UpdatePausedState(1,0,1)`; if `ns<=2.0f` → `SetMusicSpeed(ns)` |
| 4 | `kPad_R2` (1) | static `Message deploy_if_possible`; loop `mAllActivePlayers[i]->Handle(msg,true)`, **re-reading size each iteration** |
| 5 | `kPad_L2` (0) | static `Message audition_cam_toggle`; `TheGamePanel->Handle(msg,true)` |
| 6 | `kPad_DUp` (12) | `TheGamePanel->Property(sym,true)->Array(0)`; cycle index at `mUnkTU5GuidePitch+0x10` modulo `arr->Size()`; `TheGamePanel->mDirectInstrument` (@0x150) → `Disable()` if 0 else `Enable(); SetVolume(v)` |
| 7 | `kPad_DDown` (14) | `mUnkTU5GuidePitch+0x14` (`VocalGuidePitch*`) → `GetGuideTrack()+1`, wrap to −1 at `TheSongDB->GetVocalNoteListCount()`, `EnableGuideTrack(t)` |

**Shared jump tail:** if `stopped` → `mGameWantsPause=false; UpdatePausedState(1,0,1); SetMusicSpeed(0.25f)`.

### 3.1 Three map names proven to be ICF fold survivors (by body bytes)

The map's name at these addresses is the *arbitrary survivor*, not the callee our source
means. Each was read as raw bytes, not inferred from the score:

| address | body | map says | actually |
|---|---|---|---|
| `fn_822E4500` | `lwz r3,4(r3); blr` | `ObjPtrList<Fader>::size` | `VocalGuidePitch::GetGuideTrack()` |
| `fn_82368FC0` | `lfs f1,0x14(r3); blr` | `TourProperty::GetMaxValue` | `SongDB::GetSongDurationMs()` (`mSongDurationMs`@0x14) |
| `fn_8274B0F8` | — | `Int` | called **both** as `Int` and as `Array` |

### 3.2 The one genuine unknown

`fn_826C9160` — a no-argument `VocalGuidePitch` method invoked from the shared tail.
**Unidentified.** Not guessed at.

---

## 4. `Shuttle::SetActive` — defined, and why that was the actual blocker

### 4.1 The fold is real, proven on raw bytes

```
?SetActive@Shuttle@@QAAX_N@Z   raw=988300084e800020  relocs=[]
?Enable@Metronome@@QAAX_N@Z    raw=988300084e800020  relocs=[]
retail 0x826f07b8                  98830008 4e800020
```

8 bytes, **zero relocations** — maximally foldable: any `void f(bool)` storing its
argument to `+0x8` lands there. `Shuttle::mActive` is at `0x8`. Independently,
`comdat_retail_verify.py --pattern 'Enable@Metronome'` reports *"SECTION byte-identical
to retail (reloc words skipped): 1, differing: 0"*, and `SetActive@Shuttle` is confirmed
**absent** from `target_symbol_map.json`.

This is *stronger* than the gate's own masked test, which the brief correctly warns
admits such thunks at the vacuous `FT-EMPTY` tier. **An `FT-EMPTY` admission is not
proof of folding and is not cited as such anywhere in this document.** The evidence is
the call site (§2.2) plus byte identity.

### 4.2 Why the gate refused — two refusals, one real and one a tool defect

**Refusal #1 was correct and found a genuine hole in our tree:**

```
"reason": "no COMDAT for ?SetActive@Shuttle@@QAAX_N@Z in any of our compiled objs"
```

`Shuttle::SetActive` was **declared in `Shuttle.h`, called from `Game::OnSetShuttle`,
and defined nowhere in the tree** — an unresolved external. With no body on our side
there is nothing to compare, which is why the gate refused outright rather than
admitting it vacuously. **Defining it is what makes the fold provable on bytes**, and
that definition is this lane's second source change.

**Refusal #2, after the definition existed, is a one-sided instrument error:**

```
"body_evidence": "relocated fields at different offsets: retail [0] vs ours []"
```

The mechanism, in `tools/fold_thunk_gate.py`:

- `Retail.canon()` **infers** relocations *by instruction form*: op 38 (`stb`) is in
  `IMM16_OPS`, so it unconditionally records `targets[0] = None` ("unresolvable in a
  linked image").
- Our side **reads the real COFF relocation table**, which is correctly empty (`relocs=[]`).
- `compare()` then does `if set(rt) != set(ot): return False`, which fires on that pure
  asymmetry — **after the masked words have already compared equal.**

So the two sides are not the same measurement: one is inferred, one is observed. This
is the same disease as the "+8 B STLport source bug" that did not exist — *a one-sided
reader artifact*. Additionally, `mask_word` zeroes the displacement for `IMM16_OPS`
(`w & 0xFFFF0000`), so **the gate never actually verified the `+8`** it is nominally
checking; the byte proof in §4.1 does.

**Proposed fix (not applied):** treat an *inferred* `None` target on the retail side as
compatible with an absent relocation on ours — compare only offsets where both sides
have a *resolved* target — and verify IMM16 displacements explicitly rather than
masking them away.

### 4.3 Blast radius: measured at **zero**, which is why I did not patch the gate

Re-ran the house worklist to see whether this defect blocks any other pair:

```
total rows: 36     ADMIT: 7     REFUSE: 29
REFUSEs matching 'relocated fields at different offsets': 0
```

All 29 refusals are other reasons (body-length mismatches, relocation-target
disagreements, genuinely different retail bodies). **This defect blocks exactly one
known pair — ours.**

⇒ **Recorded REFUSAL, deliberately.** The house invariant is that alias groups are
never hand-edited, the gate backs **1,657** live groups, and a non-tooling lane
changing shared admission logic late in its budget is the documented
"unannounced fleet deployment" hazard. With a measured blast radius of 0, patching the
gate buys one 260 B row and risks 1,657 groups. The 260 B stays on the table **with a
diagnosed root cause and a proposed fix**, which is worth more than the bytes.

---

## 5. What I did NOT do

- **Did not write the 1,664 B switch tail.** It cannot reach 100% as things stand:
  fold-name charges on `Array`/`Int`, `GetGuideTrack` and `GetSongDurationMs` (§3.1),
  plus the unidentified `fn_826C9160` (§3.2). All-or-nothing scoring means a
  speculative tail pays **0 bytes either way**, while adding a large untested body that
  the mandatory native gate must survive. The decode is recorded instead.
- **Did not install the `Shuttle`/`Metronome` alias membership** (§4.3).
- **Did not patch `tools/fold_thunk_gate.py`** (§4.3) — diagnosed only.
- **Did not hand-edit `scripts/symbol_aliases.json`.** A backup was taken and the file
  is **unmodified**.
- **Did not run the permuter** (OFF by standing directive).
- **Did not correct `ROADMAP_GAP_TO_TARGET_2026-09-01.md:2369`**, which still repeats
  the two claims refuted in §2.1 and §2.2 — it is another lane's dated record and
  outside this lane's staged paths. Flagged for whoever edits it next.
- **Did not verify `fn_826C9160`'s identity** by any means.

---

## 6. Source changes landed

Both are behavioural/structural fixes that the metric cannot see, landed on merit.

1. **`src/band3/game/Game.cpp`** — `OnMsg(ButtonDownMsg)`: replaced the
   `if (msg.GetUser()) {}` stub with the proven head. ★ **This fixes a real bug:** the
   per-pad counter it increments is the same one `OnMsg(ButtonUpMsg)` decrements, so
   with the DOWN handler stubbed, the release handler has been decrementing a counter
   **nothing ever raised.**
2. **`src/band3/game/Shuttle.cpp`** — added `void Shuttle::SetActive(bool active) { mActive = active; }`,
   previously declared and called but never defined anywhere in the tree (§4.2).
