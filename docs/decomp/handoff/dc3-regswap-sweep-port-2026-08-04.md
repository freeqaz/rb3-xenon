# Porting the DC3 2026-08-04 regswap/AT_LIMIT sweep into rb3-xenon

**Date:** 2026-08-04 · **Branch:** `port/dc3-regswap-sweep` · **Worktree:** `~/tmp/wt-dc3port`

Source material: `../dc3-decomp/docs/sessions/2026-08-04-regswap-atlimit-sweep.md`,
`../dc3-decomp/docs/decomp/patterns/fixable-inline-boundary.md`, and the seven
`Merge sweep/regswap-*` lanes' per-function commits (the reasoning lives in the
individual commits, not the merges).

---

## TL;DR

- **4 live bugs ported** (Trans preserve-scale, OSC parser ×3, Locale MemPopTemp,
  Locale altCfg). rb3-xenon carried every one of them, because `src/system/` was
  ported wholesale from dc3-decomp and these files are byte-identical to DC3's
  pre-fix versions.
- **The biggest win was not in the DC3 material at all.** Chasing the Locale bugs
  forced an identification of `Locale::Init` in the RB3 binary, which then showed
  RB3 retail has **no devkit locale-override block**. Gating it took
  `Locale::Init` **67.9% → 87.2%**.
- **`NgMat::RefreshState`: DC3's open numeric bug is REPRODUCED in RB3 and is
  measurable here** (92.2%). The reciprocal-multiply divergence is real on RB3's
  retail target too. I did **not** crack a source shape that gets the target's
  exact form, and I now have evidence about *why* that DC3 did not (below).
- **The parameter-home-area lever DOES exist in retail `/O1`** — measured, with a
  control. The brief's prediction that `/O1` would elide the dead home stores is
  **wrong**.
- **The DC3 orchestrator MCP silently measures DC3 when handed an rb3-xenon
  `project_dir`.** This nearly produced a fabricated baseline. See "Measurement".

---

## Measurement — read this before trusting any number in a port like this

`mcp__orchestrator__run_objdiff(symbol=..., project_dir=<rb3-xenon worktree>)`
**does not measure the rb3-xenon worktree.** Asked for `?RefreshState@NgMat@@IAAXXZ`
with `project_dir` pointed at `~/tmp/wt-dc3port`, it returned **93.3%** — which is
DC3's own documented baseline for that function, not RB3's. RB3's value is
**92.2%**. The two are close enough that this would have passed unnoticed.

The `project_dir` argument does not redirect the DC3 server. Use rb3-xenon's own
tooling:

```bash
./bin/objdiff-cli diff -p . '<mangled>' \
    -c functionRelocDiffs=none -c ppc.calculatePoolRelocations=false
```

Validated against `report.json` on an untouched function: prints
`92.2% normalized`, report's `fuzzy_match_percent` = `92.1978`. Same ruler.

**Corollary for any future cross-repo port: validate the measurement path on an
untouched function whose value you already know from `report.json`, before you
measure anything you changed.**

---

## What was portable at all — the pairing wall

Most of the DC3 Tier-1 functions **cannot be verified against RB3's target asm**,
because RB3's targets are anonymous `fn_8XXXXXXX` and only pair with our symbols
when `scripts/target_symbol_map.json` has an entry. Unpaired rows read
`fuzzy=None / mpn=0.0` in `report.json` — which looks like "0%", not like
"absent". Do not read that as a bad match.

| DC3 function | in rb3-xenon? | measurable at start? |
|---|---|---|
| `RndMesh::SetVolume` | yes | **yes — already 100.0%** |
| `NgMat::RefreshState` | yes | **yes — 92.2%** |
| `Locale::Init` | yes | no → **identified this session**, now paired |
| `OSCMessenger::Poll` | yes | no (and not even inside the unit's `.text` pin) |
| `RndTransformable::Load` | yes | no |
| `RndPostProc::LoadRev` | yes | no |
| `DataNode::Equal` | yes | no |

`MemPushTemp`, `MemPopTemp`, `Locale::Init`, `OSCMessenger::Poll` and
`RndTransformable::Load` are all **absent from `target_symbol_map.json`**, so even
resolving callee names inside the pinned `.s` was impossible until `Locale::Init`
was identified by hand.

⚠ `OSCMessenger.cpp`'s pin is `0x825C5F00-0x825C5FD8` — **216 bytes**, containing
`MakeOSCAddress` and one stub. `Poll` is not in it. Worse, `0x825c5f00` is mapped
to `?SetBestBattleScore@AppLabel@@QAAXPAVHamProfile@@H@Z` — an **AppLabel/HamProfile
symbol, i.e. DC3 game code that cannot exist in RB3**. That map entry is wrong and
someone should look at it; I left it alone as out of scope.

---

## Per-item results

### Ported and verified against RB3's own target asm

**`Locale::Init` — MemPopTemp before the four permanent tables** (`bbca6c35`).
RB3 retail confirms DC3's finding independently. In `build/45410914/asm/Locale.s`:

```
827C9E5C  bl fn_827BCA50    ; ~vector<DataArray*>  (block exit)
827C9E60  bl fn_827BC2A0    ; no args set up -> void()  == MemPopTemp()
827C9E6C  lwz r30, 0x0(r22) ; mSize
827C9E74  slwi r3, r30, 2   ; -> new Symbol[mSize]
```

`fn_827BC270` (also no-args, immediately before the vector ctor) is `MemPushTemp`;
the two are 0x30 apart, i.e. adjacent bodies in Memory.cpp. Our source popped at
the *bottom* of the function, so all four permanent locale tables
(`mSymTable`/`mStringData`/`mStrTable`/`mUploadedFlags`) were allocated on the temp
arena. Read by every `Localize()` call for the whole run of the game.

**`Locale::Init` — altCfg node order** (`f06d2245`). Self-evident from the reader
in the same function (`Node(0)` = tag, `Node(1..)` = files), so the devkit override
opened the literal string `"locale"`.

Measured together against RB3's target: **66.1% → 67.9% (+1.8)**.

**`Locale::Init` — devkit block absent from retail** (`b90710df`). See below.
**67.9% → 87.2% (+19.3).**

### Ported on logic evidence, not verifiable here (kept regardless of match)

Per the sweep's "live bugs outrank match %" rule, and my teammate's stronger
restatement — *do not use match % as the accept/reject gate for a behavioural fix
at all*. All three are unambiguous logic defects arguable without any asm:

- **`RndTransformable::Load` rev 3-5 preserve-scale** (`80d04af2`). Ours had
  `mPreserveScale = unkb0` — set for any non-zero packed value. Corroborated
  **from our own file**: the switch below takes labels in 0x04/0x84, 0x08/0x88,
  0x10/0x90, 0x20/0xA0 pairs, so 0x80 is a flag bit orthogonal to the billboard
  mode. Match unchanged (unpaired).
- **`OSCMessenger::Poll` ×3** (`bf9ba98d`). `pos` a word index used as a byte
  offset; `i`/`f` payloads copied one byte at a time; `fff` vector payload read
  from packet start ignoring `pos`. Match unchanged (unpaired, not in the pin).

### Deliberately NOT ported

- **`RndMesh::SetVolume` MILO_LOG → MILO_NOTIFY.** rb3-xenon's `SetVolume` is
  **already 100.0%** while carrying `MILO_LOG`. And the lever is **structurally
  invisible here**: in the match build both macros expand to *the same thing* —
  `#define MILO_LOG(...) ((void)(__VA_ARGS__))` and
  `#define MILO_NOTIFY(...) ((void)(__VA_ARGS__))` (`os/Debug.h`; the
  `TheDebug`/`TheDebugNotifier` forms are `#ifdef HX_NATIVE`, never defined for
  the match build). So the change is byte-neutral by construction and cannot be
  adjudicated against RB3's target at all. DC3 could see it only because its
  target is a *debug* build that kept the diagnostics. Note rb3 also uses
  `MILO_WARN` for the sibling "Couldn't make BSP tree" diagnostic where DC3 uses
  `MILO_NOTIFY` — the diagnostic macros differ between the engine versions, so
  copying DC3's spelling would be guessing.
- **`RndTransformable::Load` switch → if/else-if chain.** A pure codegen-shape
  change justified entirely by DC3's target asm. `Load` is unpaired here, so it is
  unmeasurable and unfalsifiable. Not applied.
- **`RndTransformable::Load` rev 7/8 stale `_arg0`.** rb3-xenon already computes
  the remap from the freshly-read value — **DC3's bug was never present here.**
  Useful datum: that one *was* a DC3-side decomp regression, not shared-source rot.
- **`DataNode.cpp` `_outline_Name` noinline shim.** Absent from rb3-xenon. Nothing
  to do.
- **`RndPostProc::LoadRev` bloom-colour reciprocal.** The shape is present
  (`float range = 4.0f - minVal;`) but `LoadRev` is unpaired, so the change would
  be a numerically-significant edit with zero RB3 evidence. Recorded as a
  candidate, not applied — this is a codegen-inferred *source shape*, not a
  self-evident logic error, and that is the line I drew for unverifiable edits.

---

## The `/O1` question: does the parameter-home-area lever exist in retail?

**YES. The brief's prediction that `/O1` would elide these dead stores is wrong.**

Census over both trees' dtk-emitted target `.s` (functions delimited by
`.fn`/`.endfn`; a "dead home store" = `stw/std` to `r1`/`r31` at offset
`0x38..0x70` whose offset is **never loaded back** in the same function; asm
filtered to files regenerated after 2026-07-15 per the stale-generation trap):

| build | fns | fns with ≥1 dead home store | dead stores | preceded by `addi` |
|---|---:|---:|---:|---:|
| **RB3 retail `/O1`** (cl 10224) | 82,230 | **8,711 (10.59%)** | 23,378 | 7,715 |
| DC3 debug (cl 11886) | 145 | 15 (10.34%) | 24 | 4 |

Per-function incidence is **essentially identical** (10.59% vs 10.34%). The DC3 leg
is a small sample (dc3 emits few `.s`), so treat it as indicative; the load-bearing
number is RB3's absolute population of **23,378 sites, 7,715 of them carrying DC3's
exact `addi`-then-store signature**.

Concrete RB3 retail instances, verbatim DC3 shape:

```
AccomplishmentConditional.s :: fn_8266AD88
    addi r25, r24, 0x90          ; &sub-object
    stw  r25, 0x50(r31)          ; dead — never reloaded

Accomplishment.s :: fn_82595D18
    addi r11, r11, 0x8
    stw  r11, 0x50(r31)
```

⚠ Scoping caveat (from the DC3 follow-up census on `BustAMovePanel::Poll`, which
found **zero** on both sides): the build only emits a home write when the inlined
callee's `this` is a **computed sub-object address**. A function that only calls
accessors on `this` produces none, so a zero census on such a function means "the
lever does not apply here", **not** "`/O1` elides them". That is exactly why the
census above is over the whole binary rather than a hand-picked probe.

⇒ **The lever transfers to rb3-xenon.** It is untested as a *fix* here (no function
was driven with it this session), but its precondition is confirmed present in bulk.

---

## `NgMat::RefreshState` — the reciprocal-multiply question

**Did I crack it? No. But the picture is much sharper, and DC3's open bug is
confirmed to exist on RB3 retail too.**

RB3 baseline **92.2%** (report fuzzy 92.1978). Target vs base in the half-pixel
block:

```
TARGET (retail)                     BASE (ours)
fdivs f13, f29, f13   ; 0.5 /h1     fdivs f0,  f30, f0    ; 1.0/w   <- reciprocal
fdivs f13, f29, f0    ; 0.5 /w      fdivs f13, f30, f13   ; 1.0/h   <- reciprocal
fdivs f12, f28, f12   ; -0.5/h2     fmuls f12, f13, f29
fdivs f0,  f28, f0    ; -0.5/w      fmuls f13, f13, f28
                                    fmuls f13, f0,  f29
                                    fmuls f0,  f0,  f28
```

**Four real `fdivs` in RB3's retail target**, against our two reciprocals + four
`fmuls`. The target also loads only **three** FP constants where we load four — the
extra one is the `1.0f` reciprocal numerator. So the numeric divergence DC3 flagged
as open is **real on RB3 as well**, and unlike DC3 it is *measurable* here.

New structural detail readable off RB3's target: it makes **three** int→float
conversions for **four** divisions — Width converted once (`f0`, shared by two
`fdivs`), Height converted **twice** (`f13`, `f12`). And it reuses the guard's
`lwz` loads; there is not a single reload.

### What I tried (all reverted)

| variant | result |
|---|---|
| baseline (`int w,h` locals) | **92.2%** |
| all four divisors inline (`0.5f / mDiffuseTex->Height()` …) | 88.1% |
| hybrid: `int w` local, `Height()` inline twice | 89.1% |
| per-TU `/fp:precise` | **92.2% — byte-identical** |
| per-TU `/fp:strict` (control) | **92.2% — byte-identical** |

Inlining **does** kill the reciprocal (4 real `fdivs` appear, and the `1.0f`
constant disappears), but MSVC then re-loads the ints with `lwa` at each site
instead of reusing the guard's `lwz` — costing more than the reciprocal saves.
Same failure mode DC3 reported, now confirmed **on a different compiler build**
(RB3 = cl 10224, DC3 = cl 11886), so it is not a build-specific quirk.

### The genuinely new (negative) result

**The reciprocal-multiply transform is NOT `/fp:`-gated on this compiler.**
`/fp:precise` and `/fp:strict` both leave the codegen byte-identical to
`/fp:fast`. This was verified live, not assumed: the flag is present in
`build.ninja`, the obj genuinely rebuilt (`OBJCACHE=off`, obj mtime moved), and
the `/fp:strict` control was run precisely because two identical readings smell
like a vacuous instrument. So the "the original TU used a different `/fp` flag"
hypothesis is **dead** — do not re-fund it.

That leaves an unexplained asymmetry worth recording for the next attempt: in the
target, Width's converted value `f0` **is** used as a divisor twice and still does
*not* get reciprocal-multiplied, whereas our build applies the transform at exactly
two uses. Whatever suppresses it is neither the source shapes tried above nor the
`/fp` level.

### Do not spend more here without reading `matng-deferral.md`

`docs/decomp/matng-deferral.md` already **formally defers** Mat_NG: retail's
`RndMat`/`BaseMaterial` layout is fully reordered and bool-repacked vs our
DC3-derived headers (34 opposite-sign offset deltas). My own diff shows the same
wall — `mTexHalfPixel{X,Y,NegX,NegY}` sit at **0x19c/0x1a0/0x1a4/0x1a8** in ours vs
**0x18c/0x190/0x194/0x198** in retail (+0x10). So `RefreshState` cannot reach 100%
from the reciprocal alone regardless. **The reciprocal is still worth fixing for
numeric fidelity** (half-pixel offsets feed every NgMat texture sample), just not
as a match play.

---

## Engine-version divergence: the hazard ran the *opposite* way

The brief warned "a DC3 fix may describe code RB3's original didn't have". The
larger effect turned out to be the inverse: **a DC3 *feature* that RB3 never had,
inherited into rb3-xenon by the wholesale `src/system/` port.**

`Locale::Init`'s devkit locale-override block (`DmMapDevkitDrive` / `FileExists` /
`DataArrayPtr altCfg`) does not appear anywhere in RB3's target body. Our version
was **1632 B vs the target's 1304 B** — 328 bytes larger, and the block is the only
candidate. Gating it behind `HX_NATIVE` moved the function **67.9% → 87.2%**.

**Reusable heuristic: when a ported engine file's function is much LARGER than its
retail target, suspect an inherited newer-engine block before suspecting codegen.**
Size delta is a cheap first-pass instrument and it pointed straight at a +19.3.

---

## Fixes kept despite a non-positive / absent delta

Requested explicitly, because this list is what future sweeps need:

| fix | delta | why kept |
|---|---|---|
| `RndTransformable::Load` preserve-scale bit | **none measurable** (unpaired) | Real bug; corroborated by our own switch labels. |
| `OSCMessenger::Poll` ×3 | **none measurable** (unpaired, not in pin) | Three unambiguous logic errors. |
| `Locale::Init` altCfg order | folded into +1.8 with MemPopTemp | Self-evidently broken against the reader in the same function. |

Nothing regressed and needed parking this session. The one edit family that
*measured worse* (the RefreshState inlining variants) was reverted, correctly —
those are codegen-shape experiments, not behavioural fixes, so the number **is**
the right gate for them. That distinction is the useful one: **park-and-retry
applies to correct-behaviour edits; measured regression is still decisive for
pure codegen-shape edits.**

---

## Methodology corrections imported from the DC3 sweep

Recorded here rather than duplicated into the pattern index; all four are DC3's
findings, restated:

1. **Statement-vs-expression is a filter, not a success predictor.** Open
   statement-level residuals; drop expression-level ones (commutative order, flat
   sum term order) as floors. Only about half of statement-level ones convert. The
   discriminator: fixes that *remove work* pay; fixes that *add a local* (raising
   register/stack pressure) regress.
2. **A third residual bucket: stack-slot allocation.** If insert/delete clusters
   contain the *same* instructions and `stack-layout` shows many DIFFER/PERMUTED
   slots, it is MSVC reusing slots across disjoint scopes — drop it.
3. **Do not discard a correct edit because it measures worse.** DC3 hit this in
   three independent lanes; a follow-up added a fourth where the same correct edit
   measured −0.3% at *two different baselines* before paying +0.1% once an
   unrelated defect stopped masking it. Two negative measurements are not enough to
   condemn a correct edit. Park and retry after every subsequent landed change.
4. **Register swaps were symptoms in 100% of cases** across all seven DC3 lanes —
   including 45- and 54-instruction swap cascades that reached byte-exact 100%
   without a single register-motivated edit. Never chase them directly. (This
   agrees with rb3-xenon's own existing note in CLAUDE.md that a `REGISTER_SWAP`
   label on a sub-100 row is a symptom, not a diagnosis.)

---

## Open leads for whoever picks this up

- `RndPostProc::LoadRev`, `DataNode::Equal`, `RndTransformable::Load`,
  `OSCMessenger::Poll` are all unpaired. **Identifying them the way `Locale::Init`
  was identified here would make four more DC3 fixes verifiable** — and the
  Locale case suggests the payoff is not just confirmation but new levers.
- `0x825c5f00` → `?SetBestBattleScore@AppLabel@@QAAXPAVHamProfile@@H@Z` in
  `target_symbol_map.json` is a **wrong entry** (DC3 game symbol in the RB3 map),
  inside OSCMessenger's 216-byte pin. Worth auditing that map for other DC3-only
  class names.
- The parameter-home-area lever is confirmed present but **unused** here. 7,715
  RB3 sites carry the signature.
- `RefreshState`'s Width-divisor asymmetry (twice-used divisor, no reciprocal in
  the target) is still unexplained and is the last live thread on the numeric bug.
