# W16-BT — the `MemAlloc` align census, the macro hardening, and the dsp residue

**Lane:** W16-BT · **Date:** 2026-09-15 · **Branch:** `w16-bt` (off `main` `fe020891ba7e`)
**Worktree:** `~/tmp/wt-w16-bt` (all work; main never touched)

---

## 1. Result

| key | baseline | final | Δ |
|---|---:|---:|---:|
| `matched_functions` | 43,651 | **43,654** | **+3** |
| `matched_code` | 4,054,964 | **4,055,524** | **+560 B** |
| `matched_code_percent` | 39.571945 | **39.577408** | +0.005463 pp |
| `fuzzy_match_percent` | 49.804825 | **49.805874** | +0.001049 pp |
| `total_functions` | 69,240 | 69,240 | 0 |
| `total_code` | 10,247,068 | 10,247,068 | 0 |
| `masked_equal_functions` | 23,107 | 23,107 | 0 |

**Crossed in (3 rows, +560 B), 0 fell out:**

| bytes | row |
|---:|---|
| +248 | `default/SpectralAnalysis::?_M_insert_overflow@?$vector@MV?$XboxAllocator@M@@@…` |
| +196 | `default/FftIpp::??1FftIpp@@QAA@XZ` |
| +116 | `default/SpectralAnalysis::??0?$_Vector_base@MV?$XboxAllocator@M@@@…` |

Two findings are worth more than the bytes: a **live instance** of the align-swallowing
hazard (`XboxAllocator`, §4), and a **behavioural bug inherited from the oracle** — our
pitch→note conversion was computing a quantity that is not a note number (§7).

---

## 0. Baseline verification — the brief's figures, tested literally

All seven headline figures verified **exactly** on a full build in this worktree before
anything was edited.

| key | brief | measured | verdict |
|---|---:|---:|---|
| `matched_functions` | 43,651 | **43,651** | ✅ |
| `matched_code` | 4,054,964 | **4,054,964** | ✅ |
| `matched_code_percent` | 39.5719 | **39.571945** | ✅ |
| `total_functions` | 69,240 | **69,240** | ✅ |
| `total_code` | 10,247,068 | **10,247,068** | ✅ |
| `fuzzy_match_percent` | 49.804825 | **49.804825** | ✅ |
| `masked_equal_functions` | 23,107 | **23,107** | ✅ |

Rowset identical to `~/tmp/rows_w16bs_main.json`: **40,979 rows, 0 crossed in, 0 fell
out**. The four dsp rows also verified exactly: `AnalyzeBlock` 1,780 B @ **85.723595**,
`Detect@VibratoDetector` 376 B @ **20.861702**, `??0IIR4PoleFilter` 312 B @ **10.192307**,
`ShiftedDotProduct` 356 B @ **25.011236**.

### One brief figure that did NOT survive

The brief cites the three PitchDetector sites as `0x82B81050 / 0x82B8106C / 0x82B81088`.
**That triple is not a consistent set.** Measured from retail bytes, the three `li r4,0x10`
are at `0x82B81050 / 0x82B81064 / 0x82B81078` and the three `bl MemAlloc` are at
`0x82B81058 / 0x82B8106C / 0x82B81080`. The brief's list mixes one `li` address with one
`bl` address, and `0x82B81088` is neither. The **sites are real** and the self-validation
passes; only the addresses were garbled. Cite `0x82B80FC8` (`SetSampleRate`'s
`.pdata` start) and the two address sets above.

---

## 2. The census instrument, and how it was kept from being vacuous

Scanned **every** word of retail `.text` in `orig/45410914/band.exe` (the decrypted PE;
`.text` va `0x82270000`, 0x9DCE3C bytes) for `bl` with `AA=0, LK=1` whose computed target
is an allocator, then walked back for the last write to `r4`.

Three instrument defects were caught before they could produce a confident wrong answer —
recorded because each is reusable:

1. **capstone's `regs_access()` is not implemented for PPC in this build.** It raised
   `CS_ERR_ARCH` on every instruction, so the first run reported `DECODE_FAIL 300/300`.
   Loud, therefore harmless. Per-operand `access` flags are *also* absent (`PpcOp` has no
   `.access`), so the destination register had to be decoded from the mnemonic.
2. **That hand decode needs an audit path or it is a silent filter.** The classifier
   whitelists the mnemonics whose FIRST operand is the destination and separately lists
   the forms that cannot write a GPR (stores, compares, branches, `mt*`); anything else
   mentioning `r4` is pushed to an UNCLASSIFIED list rather than skipped. **Final run:
   0 unclassified forms**, so the coverage claim is measured, not assumed.
3. **The `.pdata` length decode was wrong twice** and would have mislabelled which
   function each site sits in. X360 packs `RUNTIME_FUNCTION` LSB-first — `PrologLen` in
   the low 8 bits, `FunctionLen` in **words** in bits 8..29. Validated against an
   independent source: **57,621 of 57,627 extents agree with `config/45410914/symbols.txt`
   exactly, 0 differ** (6 absent from symbols.txt). The first two decodes would have
   agreed with nothing, which is how they were caught.

⚠ Scope bound: this census counts **direct `bl`** to the allocator only. The brief's
"~510+ callers" is not reproduced — there are **300** direct `bl` sites to
`?MemAlloc@@YAPAXHH@Z` and **21** to `?_MemAllocTemp@@YAPAXHH@Z`. Calls reaching the
allocator through a thunk or a wrapper are attributed to the wrapper, which is the
behaviour you want for an align census but means the figure is not a caller count.

---

## 3. The census — retail `bl ?MemAlloc@@YAPAXHH@Z` (`0x827bcd38`), 300 sites

**Histogram of the `r4` (align) argument:**

| `r4` source | sites |
|---|---:|
| `li r4, 0` | **285** |
| `li r4, 0x10` | **6** |
| `li r4, 0x80` | **1** |
| `li r4, 0x20` | **1** |
| register / computed (wrapper passing its own parameter) | 4 |
| live-in parameter, no write in the function (forwarding wrapper) | 3 |

**`?_MemAllocTemp@@YAPAXHH@Z` (`0x827bcff0`), 21 sites: align 0 at every one**
(20 by `li r4,0` inside the window, 1 at 13 instructions back — also `li r4,0`). It is
**named** in `scripts/target_symbol_map.json`, contrary to the brief's "if it is unnamed".

### 3.1 Every non-zero-align site, adjudicated

| retail `bl` | align | enclosing function | unit | our source spelling | verdict |
|---|---|---|---|---|---|
| `0x82b81058` | 0x10 | `?SetSampleRate@PitchDetector@@` | `system/dsp/PitchDetector.cpp` | `(MemAlloc)(n, 0x10)` | **AGREE** (row 100.0) |
| `0x82b8106c` | 0x10 | `?SetSampleRate@PitchDetector@@` | idem | idem | **AGREE** |
| `0x82b81080` | 0x10 | `?SetSampleRate@PitchDetector@@` | idem | idem | **AGREE** |
| `0x827199f0` | 0x80 | `?Init@RingBuffer@@QAAXH@Z` | `MidiSynth.cpp` (src: `synth/Mic.cpp`) | `(MemAlloc)(size, 0x80)` | **AGREE** (row 100.0) |
| `0x827bd1bc` | 0x10 | `?_MemAllocH@@YAPAVMemHandle@@H@Z` | `MemMgr.cpp` | `(MemAlloc)(…, 0x10)` | **AGREE** |
| `0x82b7599c` | 0x10 | `??0?$_Vector_base@MV?$XboxAllocator@M@@@…` | `SpectralAnalysis.cpp` | 5-arg debug, align **swallowed** | ⛔ **DIVERGE — fixed, §4** |
| `0x82b75c24` | 0x10 | `?_M_insert_overflow@?$vector@MV?$XboxAllocator@M@@@…` | `SpectralAnalysis.cpp` | idem | ⛔ **DIVERGE — fixed, §4** |
| `0x82bbb430` | 0x20 | unnamed `0x82bbb3f0` | **unpinned** | no source | **NO-SOURCE — filed, §6** |

**Wrapper sites (align is a pass-through parameter, correct by construction):**
`XMemAlloc` `0x822735ec`; `?MemAlloc@@` `0x827bce80`; `?_MemAllocTemp@@` `0x827bd024`;
`?MemRealloc@@` `0x827bd0dc`; `?LoadDtz@@` `0x8276a950`; `?RawAlloc@ReclaimableAlloc@@`
`0x827bb330`; `?DspAllocate@@` `0x82bba08c`.

### 3.2 Self-validation (a census that misses these is vacuous)

- ✅ The **three PitchDetector `0x10` sites** are found, with the right align, and their
  row `?SetSampleRate@PitchDetector@@` reads **fuzzy 100.0** — W16-BR's fix holds.
- ✅ **`Mic.cpp`'s `0x80` site** is found (as `?Init@RingBuffer@@`, pinned under the
  `MidiSynth.cpp` unit) and its row reads **fuzzy 100.0**.
- ✅ **`MemMgr.cpp`'s `0x10` site** is found — it is `?_MemAllocH@@`, already parenthesized.
- ✅ 0 unclassified instruction forms; `.pdata` extents agree 57,621/57,621 with `symbols.txt`.

### 3.3 Our-side census (48 unparenthesized `MemAlloc(` invocations in `src/`)

By arity: **30× 5-arg, 12× 4-arg, 4× 2-arg, 2× 3-arg**. Of these:

- The three real **2-arg** sites (`rndobj/Part.h`, `utl/MemMgr.cpp` ×2) all pass `0`, so
  they were harmless *today* — the hazard was prospective, not live, at those sites.
- The three **5-arg sites with a non-zero align** (`Memory_Xbox.cpp` `align`,
  `BinkReader.cpp` `0x80`, `Mic.cpp` `0x80`) are all inside `#ifdef HX_NATIVE`, and each
  already carries a correct parenthesized match arm in its `#else`. Since the macro itself
  is `#ifndef HX_NATIVE`, **they never meet** — which is why the macro's swallowing of the
  5th argument had no match-build victim other than `XboxAllocator`.
- The two **3-arg** hits are `NUISPEECH::MemAlloc` in `src/xdk/nuispeech/xboxmem.{h,cpp}` —
  a different function in its own namespace, in files that do not include `utl/MemMgr.h`.
  No macro collision.

---

## 4. The one live DIVERGE: `XboxAllocator<T>::allocate` (+3 fns / +560 B)

`src/system/synth_xbox/FftIpp.h` spelled the allocator as

```cpp
return (pointer)MemAlloc(count * sizeof(T), __FILE__, __LINE__, "unknown", 0);
```

which `MemMgr.h`'s macro rewrites to `(MemAlloc)((size), 0)` — so **no value written in
that 5th position could ever have reached the callee.** Retail passes `0x10` at both
surviving out-of-line call sites, and additionally **guards `count == 0`**.

Retail `_Vector_base(n, alloc)` @ `0x82b75960`, read from `band.exe`:

```
li r11,0 ; stw r11,0(r3) ; stw r11,4(r3) ; stw r11,8(r3)
cmplwi cr6, r4, 0
beq     cr6, 0x82b759a4        <-- guard
li      r4, 0x10               <-- align
slwi    r3, r30, 2
bl      0x827bcd38
```

Three separate corrections were needed, each decided against a control rather than guessed:

1. **align `0` → `0x10`**, via the parenthesized bypass.
2. **`count == 0` guard.** ⚠ Localised to *this allocator*, not to STLport, because
   **all 20 `_Vector_base<T,StlNodeAlloc>` ctor rows are at fuzzy 100** — the guard is not
   generic STLport behaviour, and putting it in `stl/_vector.h` would have broken 20
   matching rows.
3. **`deallocate` null-guards the free.** Retail `_M_insert_overflow` has
   `cmplwi cr6,r3,0; beq cr6` before `bl MemFree`. Again localised to the allocator, on an
   untreated control: our STLport `_M_clear()` is **unguarded**, and the StlNodeAlloc
   vectors that route through it (e.g. `default/Archive`'s 408 B `_M_insert_overflow_aux`)
   sit at fuzzy 100 — so retail's `_M_clear` is unguarded too, and the guard must be the
   allocator's. `~_Vector_base` shows only one test because `/O1` collapses its own
   `if (_M_start != 0)` with this one.

A fourth correction was **guard polarity**: `if (count == 0) return 0;` reproduced retail
in the ctor but *inverted* the branch inside `_M_insert_overflow`. Rewriting it as the
positive test `if (count != 0) return MemAlloc(...); return 0;` reproduces retail in both.

| edit | predicted | measured |
|---|---|---|
| align 0x10 + guard | +2 fns / +364 B | **+1 fn / +116 B** (`??0_Vector_base` 41.448 → 100.0) |
| polarity flip | — | `_M_insert_overflow` 89.016 → 95.726 |
| `deallocate` guard | +1 fn / +248 B | **+2 fns / +444 B** (`_M_insert_overflow` → 100.0; `??1FftIpp` → 100.0, unpredicted) |

★ **The r30/r31 register swap that dominated `_M_insert_overflow`'s residual DISSOLVED
once the guard was right.** It was a symptom, not a diagnosis — a permuter run against it
would have been wasted budget. This is CLAUDE.md's `REGISTER_SWAP` rule reproducing itself.

---

## 5. Hardening the header (Δ exactly 0, option (a) achieved)

The old macro, and the comment above it claiming the 2-arg form "bypasses" it, were the
reason this bug was invisible. MSVC's traditional preprocessor expands a function-like
macro **even when invoked with fewer arguments than it declares**.

Installed an arity dispatcher that carries the align for the 2-arg retail spelling **and**
the 5-arg debug spelling, leaving the 4-arg spelling forcing 0 exactly as before.
`MEMALLOC_EXPAND` is load-bearing: without it MSVC passes `__VA_ARGS__` to the nested
macro as a single token, which is why most arg-counting tricks fail on this preprocessor.

**Proven with `cl.exe /E` on the real compiler (X360 16.00.10224, through wibo) BEFORE the
tree was touched**, as the brief required:

| spelling | before | after |
|---|---|---|
| `MemAlloc(sz, 0x10)` | `(MemAlloc)((sz), 0)` ⛔ | `(MemAlloc)((sz), (0x10))` ✅ |
| `MemAlloc(sz, __FILE__, 0x2C, "n")` | `(MemAlloc)((sz), 0)` | `(MemAlloc)((sz), 0)` ✅ |
| `MemAlloc(sz, __FILE__, 0x2C, "n", 0x80)` | `(MemAlloc)((sz), 0)` ⛔ | `(MemAlloc)((sz), (0x80))` ✅ |
| `(MemAlloc)(sz, 0x10)` | untouched | untouched ✅ |

**Whole-tree effect: Δ exactly 0 on every headline key, zero rowset churn, with 1,050 TUs
recompiled** — so this is a real cascade, not an absent-vs-absent reading. Δ0 is the
expected and desired outcome: every live match-build call site passes align 0 (§3.3). The
value is that the defect can no longer be reintroduced silently. A 1- or 3-arg call now
names an undefined `MEMALLOC_1`/`MEMALLOC_3` and fails to compile.

Option (b) (make the 2-arg form an error) and option (c) (a `scripts/` grep guard) were
**not** needed: option (a), the preferred one, works on this compiler.

### `_MemAllocTemp` is structurally sound and was deliberately left alone

Probed on the same compiler. Its 5-parameter macro turns any non-5-arg call into
`(_MemAllocTemp)((sz), ())` — **a compile error**, not a silent drop:

```
T: _MemAllocTemp(sz, 0x20);                       -> (_MemAllocTemp)((sz), ());   // error
U: _MemAllocTemp(sz, __FILE__, 0x2C, "n", 0x20);  -> (_MemAllocTemp)((sz), (0x20));
```

Its only *compiling* spelling already preserves the align. It fails loudly, so it is not
the same hazard and needs no dispatcher. All 10 of our call sites use the 5-arg form with
align 0, and all 21 retail sites pass 0 — **uniformly AGREE, no divergence**.

---

## 6. Filed for other lanes / other work

- **`0x82bbb3f0` (align `0x20`)** — a ctor-shaped function, `mSize=arg; …=0;
  mBuf = MemAlloc(arg, 0x20)`. **Unpinned and unnamed**, sitting in the gap between
  `MultiTempoTempoMap.cpp` (ends `0x82BBAA6C`) and `xdk/xaudio2/leapfxlib.cpp` (starts
  `0x82BBE650`) — XAudio2/XDK territory, out of scope per the standing directive except
  for pinning. Filed, not chased.
- Nothing was found inside W16-BU's (`0x8235`–`0x8236` Tour, `BandProfile`, `0x8258b`) or
  W16-BV's (`0x822E`/`0x822F` VocalTrackDir/GemTrackDir) surfaces. **No census site falls
  in either lane's units**, so there is nothing to hand over.

---

## 7. dsp residue

### 7.1 `?AnalyzeBlock@PitchDetector@@` — 1,780 B, 85.723595 → **85.85843**

The row does **not** cross (it is 1,780 B behind 122 charged sites, and `matched_code` is
all-or-nothing), but it yielded the lane's most valuable non-metric finding.

⭐ **THE ORACLE IS WRONG AND RETAIL IS RIGHT.** rb3-Wii writes
`mPitch = 39.863136f + -36.376316f * log10(pitchHz)`. Retail emits

```
lfs f0,  lbl_8219AF54   ; = 39.863136   (float read out of band.exe .rdata)
lfs f13, lbl_8219AF50   ; = 36.376316
fmsubs f0, f12, f0, f13 ; = log10 * 39.863136 - 36.376316
```

while the oracle's spelling compiles to `fnmsubs` with the **same two constants in the
opposite operand roles**. That is why it survived — it reads like a transcription of the
same formula. Retail's is also the only form that *means* anything: it is the standard
Hz→MIDI-note conversion `69 + 12·log2(f/440)`, since `12/log10(2) = 39.863136` and
`39.863136·log10(440) − 69 = 36.376316`. The inherited spelling returned a quantity that is
not a note number at all. **Behavioural fix first, codegen fix second**; the charged
`replace` at that site is gone (both sides now byte-identical there).

⚠ Note the constants were read as **floats out of `.rdata`**, not inferred. The two retail
labels are `lbl_`-prefixed, i.e. **placeholder names that `name_check` forgives**, which is
why rows 454–457 were never charged and the defect hid behind an *uncharged* pair of loads.

**Negative result, recorded in-source:** rewriting
`confidenceOut = fixedGain * (pitchHint * mAveEnergy) / unk38` as
`fixedGain / unk38 * pitchHint * mAveEnergy` to chase retail's association (retail divides
`fixedGain` by `unk38` first: `fdivs f0,f25,f0; fmuls f0,f0,f24; fmuls f0,f0,f13`) does
**not** reproduce it. `/fp:fast` reassociates either spelling; 5 charged sites remain
either way and total charges went 122 → 123. **The association is chosen by the scheduler,
not by the parentheses.** Reverted rather than leave an unjustified rewrite; the target
association is written at the site.

**Remaining 122 charges, classified** (for whoever takes this next):

| class | ~charges | note |
|---|---:|---|
| whole-function **f28/f29 swap** | ~35 | retail holds `0.0f` in f29 and `5.0f` in f28; we do the reverse. ⚠ Do **not** attack by reordering declarations — CLAUDE.md records that as measured **inert** for register-only swaps (12+ byte-identical hand variants, two zero-gain beam sweeps). |
| `Time2IirA` out-of-lining | ~16 | retail `bl ?Time2IirA@?A0xa7b3dd7d@@`, we inline `exp`. W16-BR §10: writing the helper measures Δ0 because `/O1 /Ob2` inlines it back. The hash is a **foreign** anon namespace, so retail reaches a definition in another TU. |
| ICF fold-alias `bl` names | 2 | idx 164/220: target names an `StlNodeAlloc` ctor where we call `IIR4PoleFilter::Begin`/`End`. Relocation-name class; needs retail-byte adjudication, **not** a source edit. |
| integer arith ordering (idx 94–111) | ~14 | real codegen difference around a div/mod. |
| `/fp:fast` fuse (idx 187–191) | 3 | we emit `fmsubs` where retail keeps `fmuls`+`fsubs` — a paren-barrier case. |
| misc real shape diffs | rest | incl. idx 469–474 above. |

### 7.2 `?ShiftedDotProduct@@` — 356 B @ 25.011236, NOT worked (already a documented wall)

⚠ **The brief's "Task 6, never started" is out of date.** `src/system/dsp/SndAnalysis.cpp`
already carries the adjudication: retail's fast path is **hand-written VMX128**
(`0x82B81758..0x82B817C0`, `vmaddfp` plus primary-opcode-4/5/6 VMX128 loads/stores through
a 16-byte stack accumulator at `r1-0x20`), selected by the **fourth parameter**
(`clrlwi. r11, r6, 0x18`) — not by the `(vlen & 15) == 0` test the Wii oracle uses, where
that parameter is marked `/*unused*/` and the fast path is a Gekko **paired-single** asm
block. The scalar else-arm already matches retail instruction for instruction. This is a
different ISA doing a different (4-wide, not 2-wide) blocking; it is not reconstructible
from the oracle.

### 7.3 `?Detect@VibratoDetector@@` — 376 B @ 20.861702, NOT worked

Target 376 B vs base 356 B but diff score 7,439/9,400 — objdiff's own read is "missing
implementation or wrong skeleton". This is a from-scratch reconstruction against retail
bytes, not a near-miss repair. Deferred on budget, untouched.

### 7.4 `??0IIR4PoleFilter@@QAA@PAM0@Z` — 312 B @ 10.192307, still blocked

Unchanged and blocked exactly where W16-BR §9 left it: the object is 0xE0 bytes (proved by
`li r3,0xE0` at the `new` site), the first 0x60 is pinned by the byte-identical
`FilterSlow`, and **`0x60..0xE0` is unidentified**. The ctor does not initialise that tail,
which is most of the 80-byte shortfall. I did **not** get a read of those offsets out of
`AnalyzeBlock`/`Analyze` — see §8 for exactly what would settle it.

---

## 8. NOT done — and the evidence that would change each verdict

- **`?Detect@VibratoDetector@@` (376 B @ 20.86).** Not attempted. *What would change it:*
  a body reconstruction driven by the retail disassembly of `0x82B81...`'s `Detect` extent
  rather than by the Wii oracle — specifically, how retail unrolls the 4-iteration loop and
  whether `mBuffer`/`mPitches` are indexed `% 5` or by a rotating pointer. It is normal
  source work, just larger than the budget left; nothing about it is proven hard.
- **`?ShiftedDotProduct@@` (356 B @ 25.01).** Wall, not backlog. *What would change it:*
  someone writing the fast path in **VMX128 intrinsics** (`__vmaddfp` / `__lvx` / `__stvx`
  via `ppcintrinsics.h`) against the retail block at `0x82B81758..0x82B817C0`, with the
  4-wide blocking and the 16-byte stack accumulator at `r1-0x20`. The oracle cannot help;
  only retail bytes can. Worth 356 B and no more, so price it accordingly.
- **`??0IIR4PoleFilter@@` (312 B @ 10.19).** Blocked on layout. *What would change it:*
  an identification of `0x60..0xE0`. The cheapest instrument is an xref sweep — find every
  retail instruction that loads or stores at `+0x60..+0xE0` off an `IIR4PoleFilter*` (the
  class's own methods plus `PitchDetector::Analyze`/`AnalyzeBlock`) and read the access
  widths and strides. 128 B = 8×16 is the shape of VMX128 scratch, and the exposed
  `FilterSlow` implies a fast path that tail probably serves; if the sweep shows only
  `lvx`/`stvx`-width access it is scratch and the ctor may legitimately not initialise it,
  which would mean the 80-byte shortfall is elsewhere and the row is **not** layout-blocked
  at all. I did not run that sweep.
- **`AnalyzeBlock`'s remaining 122 charges.** Classified in §7.1 but not closed. The
  f28/f29 swap is explicitly **not** a declaration-order lever. *What would change it:*
  fixing the integer-arithmetic cluster at idx 94–111 first and re-reading the FPR
  assignment — swaps dissolve when real defects are fixed (this lane saw exactly that in
  `_M_insert_overflow`).
- **`Time2IirA`'s out-of-lining** (W16-BR's open question) — untouched. *What would change
  it:* identifying which TU owns anon-namespace hash `?A0xa7b3dd7d` and giving our tree the
  same declaration-here / definition-there split, so `/Ob2` structurally cannot inline it.
  ⚠ Not `__declspec(noinline)`, which would be metric-chasing.
- **`0x82bbb3f0`'s `0x20` align** — filed (§6), not chased: unpinned XDK.
- **`_MemAllocTemp` dispatcher** — deliberately not added; §5 proves it fails loudly
  already, so it is not the same hazard.
- **No permuter run**, per the standing directive.

---

## 9. Gates

All run in the worktree, on a fully built tree, in the brief's order.

```
BUILD                                                    rc=0
python3 scripts/verify_ruler_agreement.py --check         rc=0
python3 scripts/verify_objs_patched.py --verify-manifest  rc=0
python3 tools/icf_alias_finder.py --validate              rc=0
python3 tools/funclet_homing.py --validate                rc=0
```

### Native gate

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

---

## 10. Branch

`w16-bt`, off `main` `fe020891ba7e`. Four code commits plus this record:

| sha | subject |
|---|---|
| `879f3657` | XboxAllocator: retail passes align 0x10 and guards count==0 — the macro ate both |
| `9c01c0db` | XboxAllocator: null-guard deallocate, and write the alloc guard as a positive test |
| `55877dab` | MemMgr: arity-dispatch MemAlloc so a 2-arg align can never be swallowed again |
| `e5f84faa` | PitchDetector: the Hz->note formula — the ORACLE is wrong and retail is right |

