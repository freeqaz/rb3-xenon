# RndEnviron: the 21-revision schema gap DOES NOT EXIST — lane W8-C, 2026-09-13

Branch `w8-rndenviron`, worktree `~/tmp/wt-w8-c`, base main `2fc2552a`
(asserted ancestor before editing). Ruler: **`name_check` (graded)**, read from
`report.json` `provenance.diff_config` — `functionRelocDiffs=name_check`, not
assumed. Baseline at this lane's first full build (full build first; reflinked
target objs are pre-renamer):

```
matched_functions   42,738
matched_code        3,865,216 B
matched_code_percent  37.724308
fuzzy_match_percent   49.108723
total_code          10,245,956
total_functions      69,219
masked_equal         22,980
```

---

## 0. The headline

**W7-C's briefed finding is refuted.** `docs/decomp/SHADER_LAYOUT_2026-09-13.md`
§2 reported `RndEnviron` as a **21-revision schema gap** — retail writing
`li r11, 0x25` where we write `0x10`, streaming **49 offsets out to `0x208`**
where we stream ~20 out to `0x1a8` — and concluded that closing it needed a full
class port with `Save`/`Load`/`SyncProperty` moving in lockstep. Handoff §7.3
called it "the highest-value renderer-correctness item outstanding".

There is no gap. **`0x824302B0` is `?Save@RndPostProc@@`, not
`?Save@RndEnviron@@`.** The map row is wrong, `splits.txt` was pinned to agree
with it, and the real `RndEnviron::Save` sits unnamed at **`0x82407E58`**
streaming exactly the 22 fields our source already has, in our order, ending at
`0x1a8`.

Our `RndEnviron` schema was **complete and correct all along**. Its `Load` is a
gate-for-gate faithful port of retail's. The only genuine defects were the
revision *value*, one statement shape, and the identification itself.

### 0.1 Why W7-C's identity audit passed a wrong row

W7-C audited identity first, deliberately and correctly — that is the right
order and it is why this was cheap to unwind. Its discriminator was that the
body calls `?Save@RndColorXfm@@` and `?Save@Object@Hmx@@`.

**Both candidates satisfy that.** `RndPostProc` also owns a colour transform —
the map itself carries `?ColorXfmEnabled@RndPostProc@@QBA_NXZ` — so both Saves
call `RndColorXfm::Save`, and every Milo object calls `Hmx::Object::Save`. The
discriminator was **non-discriminating**, not mis-read. W7-C's own sentence
"the audit could have refuted it and did not" was true of the *procedure* and
false of the *instrument*.

★ The transferable rule: **an identity witness must be checked for
DISCRIMINATION, not just for presence.** A witness both candidates emit proves
nothing, and it reads exactly like a witness that proves something. Same family
as this campaign's "check a witness CAN DISCRIMINATE before it BLOCKS work" and
the vacuous-`grep` trap.

---

## 1. The four instruments that settle it

Each is independent of the others; no two share arithmetic.

**1. Spatial (decisive on its own).** `/O1`, no LTCG ⇒ TU grouping in `.text`
is preserved (CLAUDE.md, verified). `0x824302B0` sits inside an unbroken run of
`RndPostProc` methods:

| addr | symbol |
|---|---|
| `0x8242FBC8` | `??0RndPostProc@@IAA@XZ` |
| `0x8242FF68` | `?ClassName@RndPostProc@@` |
| `0x8242FFB0` | `?SetType@RndPostProc@@` |
| `0x824300D8` | `??1RndPostProc@@` |
| `0x82430270` | `?DoPost@RndPostProc@@` |
| **`0x824302B0`** | **the row in question** |
| `0x824306D0` | `?Copy@RndPostProc@@` |
| `0x82430970` | `?Handle@RndPostProc@@` |
| `0x824316E8` | `?SyncProperty@RndPostProc@@` |

The `RndEnviron` cluster lives at `0x82407948`–`0x8240ABF0`, about `0x27000`
away.

**2. Absence.** The map holds **38 `RndPostProc` rows and no
`?Save@RndPostProc@@` at all**. `Save` is the one virtual missing from that run.

**3. Our own source, written before this lane.** `src/system/rndobj/PostProc.cpp`
already declared `SAVE_REVS(0x25, 2)` — **the same `0x25`** retail writes at
`0x824302B0`. `RndPostProc` is the class whose revision is 0x25. (RB3's
`RndPostProc` is the bloom / posterize / kaleidoscope / noise / vignette /
motion-blur / refract / chromatic-aberration / hall-of-time / gradient-map
class — a heavily-iterated class streaming ~49 fields to `0x208` is exactly what
it should look like.)

**4. The real Save exists and mirrors the real Load.** `fn_82407E58` (500 B),
inside Env.cpp's own already-pinned block `0x82407A70`–`0x82408050`, immediately
before `IsLightInList`. It is the only other `WriteEndian` caller in `Env.s`. Its
field list matches retail's `Load` at `0x82409348` offset-for-offset and matches
our source 22/22.

⚠ **Method note.** `Env.s`'s address column is **synthetic** past the block
discontinuity — `.fn fn_824302B0` renders with an address column reading
`8240B048`. Everything here is keyed on the `.fn fn_<addr>` symbol, never the
column, per CLAUDE.md. Also: `.s` files use **uppercase** hex, and my first
`grep` for `fn_824302b0` returned nothing — a false negative shaped like a
decisive one.

### 1.1 The defect was CIRCULAR, which is why it survived

`PostProc.cpp`'s `splits.txt` entry had a hole punched at **exactly**
`0x824302B0`–`0x824306D0`:

```
.text  start:0x8242EC80 end:0x824302B0      <- stops AT the Save
.text  start:0x824306D0 end:0x82430970      <- resumes AFTER it
```

and `Env.cpp` carried that block. The wrong map name justified the wrong pin,
and the wrong pin made the wrong name score. This is CLAUDE.md's
"⛔⛔ the SPLITS PIN can be CIRCULAR" in the wild.

---

## 2. Retail's `RndEnviron::Save` — full schema

`fn_82407E58`, 500 B, prologue at `0x82407E50`, 125 instructions. Every scalar
is staged into a stack scratch slot and written with
`WriteEndian(const void*, 4)` (`fn_827C5098`) or `Write(const void*, 1)`
(`fn_827C4F58`) — which is why floats and ints share one callee.

| # | `this` off | field | width | how / callee | evidence addr |
|---|---|---|---|---|---|
| 1 | — | **revision** = `gRev` | 4 | `lis/lwz lbl_82C6FB08` → `WriteEndian` | `0x82407E5C`, `0x82407E70`, bl `0x82407E7C` |
| 2 | — | `Hmx::Object::Save` | — | `bl fn_8275AB90` | `0x82407E88` |
| 3 | `0x28` | `mLightsReal` | ObjPtrList | `bl fn_8249BBB0` | `0x82407E98` |
| 4 | `0x3c` | `mLightsApprox` | ObjPtrList | `bl fn_8249BBB0` | `0x82407EA0` |
| 5 | `0x64` | `mAmbientColor` | 16 (Vector4) | `bl fn_822BB448` | `0x82407EB4` |
| 6 | `0x84` | `mFogStart` | 4 f | `WriteEndian` | `0x82407EC4` |
| 7 | `0x88` | `mFogEnd` | 4 f | `WriteEndian` | `0x82407EDC` |
| 8 | `0x8c` | `mFogColor` | 16 (Vector4) | `bl fn_822BB448` | `0x82407EE8` |
| 9 | `0x80` | `mFogEnable` | 1 | `Write` | `0x82407F00` |
| 10 | `0x15d` | `mAnimateFromPreset` | 1 | `Write` | `0x82407F18` |
| 11 | `0x9c` | `mFadeOut` | 1 | `Write` | `0x82407F30` |
| 12 | `0xa0` | `mFadeStart` | 4 f | `WriteEndian` (slot `0x58`) | `0x82407F48` |
| 13 | `0xa4` | `mFadeEnd` | 4 f | `WriteEndian` (slot `0x54`) | `0x82407F60` |
| 14 | `0xa8` | `mFadeMax` | 4 f | `WriteEndian` (slot `0x5c`) | `0x82407F78` |
| 15 | `0xac` | `mFadeRef` | ObjPtr | `bl fn_8229E5D0` | `0x82407F88` |
| 16 | `0xb8` | `mLRFade` | 16 (Vector4) | `bl fn_822BB448` | `0x82407F90` |
| 17 | `0x74` | `mAmbientFogOwner` | ObjOwnerPtr | `bl fn_8238B5B8` | `0x82407F9C` |
| 18 | `0x15c` | `mUseColorAdjust` | 1 | `Write` | `0x82407FB4` |
| 19 | `0xc8` | `mColorXfm` | RndColorXfm | `bl fn_82465468` | `0x82407FC0` |
| 20 | `0x160` | `mAOStrength` | 4 f | `WriteEndian` | `0x82407FD8` |
| 21 | `0x19c` | `mIntensityRate` | 4 f | `WriteEndian` | `0x82407FF0` |
| 22 | `0x1a0` | `mExposure` | 4 f | `WriteEndian` | `0x82408008` |
| 23 | `0x1a4` | `mWhitePoint` | 4 f | `WriteEndian` | `0x82408020` |
| 24 | `0x1a8` | `mUseToneMapping` | 1 | `Write` | `0x82408038` |

**`Save` is not revision-gated** — it always writes the current format. Gating
lives in `Load`.

★ **The revision is `0xF` (15), read from a mutable `.data` int.** `lbl_82C6FB08`
holds `0x0000000F` and is referenced only from this function. This is a house
pattern, not an oddity: `Draw.s` loads `lbl_82C6FB04` (= 3) the same way at
`0x8240681C`, and the two globals are 4 bytes apart in `.data` because `Draw.cpp`
and `Env.cpp` are adjacent TUs.

### 2.1 Retail's `Load` gating — and our source already matches it exactly

`?Load@RndEnviron@@` @ `0x82409348`, 700 B. It reloads the revision from
`0x50(r1)` before each test and compares **signed** (`cmpwi`), with no packed-rev
split — which `Env.cpp`'s own top-of-file comment already documented.

| gate (retail) | addr | fields admitted | our source |
|---|---|---|---|
| `rev > 1` | `0x82409368` | `Hmx::Object::Load` | ✅ |
| `rev < 3` | `0x82409380` | `RndDrawable::DumpLoad` | ✅ |
| `rev >= 0xF` | `0x8240939C` | `0x28`+`0x3c` **else** `0x50` (`mLightsOld`) | ✅ |
| — | — | `0x64`, `0x84`, `0x88` | ✅ |
| `rev < 1` | `0x824093F4` | discard int | ✅ |
| — | — | `0x8c` | ✅ |
| `rev < 1` | `0x82409420` | int → bool `0x80` (`subic`/`subfe`) **else** read bool | ✅ |
| `rev > 3` | `0x82409454` | `0x15d` | ✅ |
| `rev > 4` | `0x8240946C` | `0x9c`, `0xa0`, `0xa4` | ✅ |
| `rev > 5` | `0x824094A4` | `0xa8` | ✅ |
| `rev > 8` | `0x824094C0` | `0xac`, `0xb8` | ✅ |
| `rev > 6` | `0x824094EC` | `0x74`, then `if (!owner) owner = this` (`0x7c`) | ✅ |
| `rev > 7` | `0x82409528` | `0x15c`, `mColorXfm.Load` (`0xc8`) | ✅ |
| `rev > 9` | `0x8240954C` | (`rev < 0xD` → discard int) then `0x160` | ✅ |
| `rev > 0xA` | `0x82409580` | `0x19c`, `0x1a0`, `0x1a4`, `0x1a8` | ✅ |
| `rev == 0xB` / `0xB < rev < 0xE` | `0x824095C8`, `0x824095DC` | discard int | ✅ |

**16 gates, 16 matches, including both `rev < 1` discards, the `subic`/`subfe`
int→bool conversion, and both trailing discards.** `?Load@RndEnviron@@` scores
`mpn 100.0` / `fuzzy 98.31` in this tree and did so before this lane touched
anything.

### 2.2 Our layout is right — nothing moved

`scripts/harvest/class_layout_report.py RndEnviron` (compiler-authoritative,
`cl.exe /d1reportSingleClassLayoutRndEnviron`) gives `sizeof = 432 (0x1b0)` and
resolves **every one of the 22 streamed offsets** to the expected member —
`0x28`/`0x3c`/`0x50` lists, `0x64`, `0x74` (`.Ptr()`@`0x7c`), `0x80`–`0x8c`,
`0x9c`–`0xa8`, `0xac`, `0xb8`, `0xc8`, `0x15c`/`0x15d`/`0x15e`, `0x160`,
`0x168` Timer, `0x198`–`0x1a4`, `0x1a8`.

⇒ **No member offset moves, so the task's "check every consumer of the fields
whose offsets move" has an empty set.**

This also **withdraws** W7-C's secondary claim that `Env.h`'s layout comments
"disagree with what the TU5 body actually streams". They disagreed with
`RndPostProc`'s body. Against the real body they are correct, and the compiler
agrees.

---

## 3. Oracle comparison

Retail outranked both oracles, and they disagreed with each other — the pattern
three lanes hit this session.

| claim | DC3 (`../dc3-decomp`) | rb3-Wii (`../rb3`) | RETAIL | verdict |
|---|---|---|---|---|
| save revision | `bs << 0x10` (Env.cpp:39) | *no `RndEnviron::Save` at all* | **`0xF` from `.data`** | **retail; DC3 is NEWER and bumped it** |
| superclass saves | `Hmx::Object` **+ `RndDrawable` + `RndTransformable`** | — | `Hmx::Object` only | our source already correct |
| fade floats | **chained** `mFadeStart << mFadeEnd << mFadeMax` | — | 3 distinct slots ⇒ chained | **DC3 right, our source had drifted** |
| field list / order | same 22 | — | same 22 | all three agree |

★ Note the two rows point **opposite ways**: on the revision DC3 is wrong and our
source inherited its error; on the chaining DC3 is right and our source had
drifted away from it. **Neither oracle is trustworthy as a class** — each claim
needed its own retail adjudication. rb3-Wii is simply silent here.

---

## 4. The port — five changes, each measured separately and committed immediately

Every A/B: `python3 tools/ab_measure.py --worktree ~/tmp/wt-w8-c --from-dirty`,
one change per run, ruler `name_check`.

| # | change | predicted | measured | per-unit |
|---|---|---|---|---|
| 1 | map + splits re-home (coupled) | Δfns **0**, Δbytes **0** | Δfns **+0**, Δbytes **+0**, Δcode% +0.000000pp, Δfuzzy **+0.009647pp** | 0 units reached 100, **0 fell off** (both rulers) |
| 2 | revision `0x10` → `gSaveRev_RndEnviron = 0xF` | row **≥96**, Δ **0/0** | row **99.904**, Δfns **+0**, Δbytes **+0**, Δfuzzy +0.000446pp | 0 fell off; leg B recompiles **1** (live) |
| 3 | chain the three fade floats | row **100.0**, Δfns **+1**, Δbytes **+500** | row **100.0**, Δfns **+1**, Δbytes **+500**, Δcode% **+0.004879pp** | **+1 `default/Env` (103→104)**; unit net (ALL units) **+1** vs whole-binary **+1** |
| 4 | `PostProc` `SAVE_REVS(0x25, 2)` → `(0x25, 0)` | row ~95.5, Δ **0/0** | row **96.36**, Δfns **+0**, Δbytes **+0**, Δfuzzy +0.000164pp | 0 fell off; leg B recompiles 1 |
| 5 | `Env.h` comment (TU0 addresses) | Δ **0/0** | Δfns **+0**, Δbytes **+0**, Δfuzzy **+0.000000pp** | 0 fell off; leg B recompiles **570** (header cascade live, byte-neutral) |

**Lane total: +1 matched function, +500 B, code% 37.724308 → 37.729187.**

⚠ `rndobj/` is the perturbation-prone directory (the PCH experiment regressed it;
W7-B priced an unrelated perturbation there at 128 B). Change 3's per-unit line
is the guard that matters: `unit net (ALL units) = +1` equals the whole-binary
`Δmatched = +1`, so no perfect row elsewhere was broken to pay for it — the exact
failure W7-B's joint run hid behind a +14 headline.

### 4.1 Row movements

| row | size | before | after |
|---|---|---|---|
| `?Save@RndEnviron@@` | 500 B | **fuzzy 0** (unpaired, anonymous `fn_82407E58`) | **100.0** |
| `?Save@RndEnviron@@` *(the impostor)* | 1,056 B | 43.95 (RndPostProc body vs our Env source) | — retired |
| `?Save@RndPostProc@@` | 1,056 B | did not exist | **96.36** |
| `?Load@RndEnviron@@` | 700 B | 98.31 / mpn 100 | unchanged |

The 43.95 row was the metric confidently scoring two different functions against
each other — the same disease as W7-C §5's `CharTransDraw` at 99.78, caught from
the low end instead of the high end.

### 4.2 Why change 2 beat its prediction

The residual after change 2 was predicted to keep a 12-site stack scratch-slot
rotation (retail rotates `0x54`/`0x58`/`0x5c`, we always used `0x58`). It mostly
did not: adding the global load added a temporary and shifted MSVC's slot
allocator, which was the flagged upside case. What remained was exactly the three
fade floats, closed by change 3 — retail gives them **three distinct slots**,
which is what one full-expression's simultaneously-live temporaries produce and
what three separate statements do not.

---

## 5. What the native runtime did wrong, and what it does now

**This is where the brief was most wrong, and it matters to state plainly.**

W7-C's native claim was: *"every RB3 `.milo` environment (rev `0x25`) is parsed
against a 20-field schema and desynchronises after the first absent field — fog,
colour-xfm and every tail float read from the wrong stream position."*

**That was false, and nothing was ever desynchronising.** It followed from
reading `RndPostProc::Save` as `RndEnviron`'s. RB3 environments are rev `0xF`,
not `0x25`; our `Load` implements all 16 of retail's gates (§2.1) and our layout
matches retail's on all 22 offsets (§2.2). The native port has been parsing
environments **correctly**.

What was actually wrong, and is now fixed:

- **Before:** `RndEnviron::Save` wrote revision **`0x10` (16)** — a format number
  the retail game never emits, inherited from DC3, which bumped the format.
  `RndPostProc::Save` wrote **`0x20025`** (alt-rev 2 packed into the high half)
  where retail writes a bare **`0x25`**.
- **After:** `0xF` and `0x25` respectively, byte-identical to retail.
- **Blast radius:** *write-side only.* Read-back is unchanged — our `Load`'s only
  gate above `0xB` is `rev < 0xF`, and 15 and 16 take the same branch, so no
  field moves and no existing asset reads differently. This is a **fidelity**
  fix (a `.milo` we author is now a format the real game produced), not a
  data-corruption fix. Overstating it would repeat the error this lane exists to
  correct.

### 5.1 Native driver coverage — measured, not implied

- **Environment LOAD is exercised** by `native/src/main_render.cpp`: it registers
  `RndEnviron` as an object factory (`milo_object_factories.cpp:199`) and walks
  `ObjDirItr<RndEnviron>` over a loaded `.milo` (`FindEnv`, line 2116), so
  loading a venue root runs `RndEnviron::Load`. Game data is present at
  `~/tmp/dlcroot/gen`.
- **Environment SAVE — the path this lane actually changed — is exercised by NO
  checked-in driver.** `main_save.cpp` is the `FixedSizeSaveable` profile/memcard
  round-trip; it never touches `RndEnviron` or a `BinStream` object save. So there
  is **no before/after native fixture for the revision change**, and none is
  claimed. A `.milo` object-save round-trip driver would be the right fixture and
  does not exist.
- The load-path run is therefore a **no-regression** check, not a demonstration
  of the fix: `Load` is byte-for-byte unchanged by this lane.

**What was actually run (native build `EXIT=0`, `rb3-render` linked; data at
`~/tmp/dlcroot`):**

| invocation | result |
|---|---|
| default X3 cells (`tracksystem_meshes`, `crowd_female01`), `--frames 1` | **rc=0**, 2 PNGs `[PASS]` — engine healthy with this lane's changes |
| `world/venue/arena/arena_02/props/gen/backwall_angles_02.milo_xbox` | scene **loads**, 4/4 meshes drawn; `[FAIL] image-not-empty` (coverage 0.00%) — a camera/framing assertion, **not** a load failure |
| `world/venue/small_club/small_club_01/gen/small_club_01.milo_xbox` | **SIGSEGV (rc=139)** in `WorldInstance` rev-stack handling — `rev stack $this mismatch` on `amp_fnr_bassman` between the venue's `.milo` and the shared `world/shared/amps/` instance, then a `String chars 259440589 > 128` blowup |

⚠ **Every scene tried reported `environ: SYNTHETIC (scene has no RndEnviron)` or
`postproc: none selected`, so `RndEnviron::Load` was NOT observed executing on
real data.** I am not claiming load-path coverage I did not get.

★ **The `small_club_01` crash is pre-existing and structurally cannot be this
lane's**, and the argument is stronger than "it looks unrelated": all three
source edits live in `RndEnviron::Save`, `RndPostProc::Save` and a comment
block. **`Save` is never called by the loader** — there is no path from
`.milo` parsing to either function. The crash is in `WorldInstance`'s nested-
instance rev stack. The default-cell `rc=0` is the corroborating control.
⇒ Fixing the `WorldInstance` rev-stack desync is the prerequisite for ever
exercising a real venue `RndEnviron` natively, and is a native-lane handoff.

---

## 6. What this lane did NOT do

- **Did not port a 21-revision schema gap** — it does not exist (§0). The briefed
  "highest-value renderer-correctness item outstanding" is closed as refuted,
  not deferred.
- **Did not finish `RndPostProc::Save`.** It is real and still divergent at
  96.36% (1,056 B). Beyond the alt-rev, retail writes one more field before the
  `0x54` aggregate (charged indices 28–33). That is a genuine port and a separate
  lane — see §7.
- **Did not withdraw or add any ICF alias group.** Checked first: no group in
  `scripts/symbol_aliases.json` references `0x82407e58`, `0x824302b0`,
  `Save@RndEnviron` or `Save@RndPostProc` (0 hits), so the map edit could not
  have been laundering a fabricated fold.
- **Did not run a native render fixture as a before/after** — see §5.1 for exactly
  why that would be implying coverage that does not exist.
- **Did not touch `SyncProperty`.** W7-C's §7.3 said `Save`/`Load`/`SyncProperty`
  must move in lockstep. With the schema unchanged, nothing moved, so
  `SyncProperty` needed no edit. `?SyncProperty@RndEnviron@@` @ `0x82409BA8` is
  untouched by this lane.
- **Did not re-derive the shader-register audit** (W7-C §1) or the `NgStats`
  finding (§4). Out of scope, and both were left as Δ0 results by design.

---

## 7. Handoffs

1. **`RndPostProc::Save` (1,056 B, 96.36%)** — now correctly identified and
   pinned, so it is workable for the first time. The residual is a real field
   divergence around charged indices 28–33 (retail stages one extra scalar
   through `WriteEndian` before the `fn_8238B5B8` aggregate at `0x54`), plus the
   familiar scratch-slot rotation. `?Load@RndPostProc@@` is **also absent from
   the map** and should be identified in the same lane — it will be inside the
   `RndPostProc` run, the same way `RndEnviron::Save` was inside Env's.
2. **Audit the map for more non-discriminating identity witnesses.** This defect
   was survivable because "calls `X::Save` + `Hmx::Object::Save`" looked like
   proof. Any map row whose only evidence is a shared-callee witness is suspect;
   the cheap screen is **spatial** — a row whose address falls outside its
   class's `.text` cluster while sitting inside another class's unbroken run.
   That screen found this in one query and would generalise.
3. **`?Load@RndEnviron@@`** sits at `fuzzy 98.31` / `mpn 100.0`, 700 B. Its
   charges are relocation-name/arg class, so it is a `matched_code` prize of
   700 B behind a small residual — but price it from `report.json`'s charged-site
   list, not from a mismatch count (CLAUDE.md's RESIDUAL-1 rule).
4. **W7-C handoffs 1, 2 and 6 remain open and untouched by this lane** — the
   fabricated `0x823c8908` alias group, pinning `TexProc`/`Spline`/
   `StreamRenderer`/`Shockwave`, and the `_S_sort` group.
