# W16-FH — `RndPostProc::LoadRev` crosses to 100.0 (+1,728 B); FF's aggregate-temp hypothesis refuted on structure; `Load` stays sub-100 with a new structural diagnosis

Lane W16-FH, 2026-09-16. Worktree `~/tmp/wt-w16-fh`, branch `w16-fh` off `b3171b99`
(= `origin/main`, FF's `aed4e990` in history). Ruler `name_check` (graded), every
number below read from the worktree's `build/45410914/report.json` after a full
`./tools/ninja-locked` (logs `~/tmp/rb3_build_w16fh_{leg0,v1,v2,v3,e1,e2,e3}.log`,
all rc=0). Compiler cl 10224. Brief: `~/tmp/brief_w16fh.md`.

Leg-A assertion (brief §Base): first build of the worktree read LoadRev
**97.358795** / Load **99.854164** / ctor **100.0** — FF's base, not FC's.

## 0. Result in one table

| row | size | base (b3171b99) | final | pays |
|---|---:|---|---|---|
| `?LoadRev@RndPostProc@@QAAXAAVBinStream@@H@Z` | 1,728 | fuzzy 97.358795 / mpn 97.671295 | **100.000000 / 100.000000** | **+1,728 B, +1 fn** |
| `?Load@RndPostProc@@UAAXAAVBinStream@@@Z` | 192 | 99.854164 (7 charged sites) | **99.895836** (5 charged sites, §5) | 0 B |
| `??0RndPostProc@@IAA@XZ` (mandatory control) | 668 | 100.0 | **100.0** on every build | — |

Whole binary (V2→V3 build, same tree): `matched_functions` 44,011 → **44,012**,
`matched_code` 4,139,696 → **4,141,424** (+1,728 B), `matched_code_percent`
40.398834 → 40.415700, fuzzy 50.262543 → 50.262665, `masked_equal` 23,237 → 23,237,
`total_code` 10,247,068 / `total_functions` 69,240 (read, not inherited). Unit
`default/PostProc` 12,480 → 14,208 / 18,564 B, 118 → **119 / 130** fns.
Whole-report row diff V2→V3: **exactly 1 of 69,240 rows moved** (LoadRev).
**Authoritative `ab_measure --patch` A/B of the whole lane vs `b3171b99` (§6):
Δmatched +1 · Δcode +1,728 B · Δmasked_equal 0 · Δhonest +1 · Δcode% +0.016866 pp ·
leg B 965 recompiles · exactly 2 of 69,240 rows moved (both PostProc) · control 100.0.**

## 1. FF's hypothesis — REFUTED on structure before any build

FF: "retail divides into a local `Hmx::Color`, and it is the AGGREGATE TEMP that
denies MSVC the reciprocal CSE."

Read against the listing, the "stack-resident 4-float aggregate at
`0x80..0x8c(r31)`" is the local **`c`** — `Hmx::Color c` that LoadRev reads off
the stream, divides in place, and then copies to `mBloomColor`. The compiler's
own `/FAs` equates for LoadRev (scratch compile of the V3 tree, `r31` = frame
pointer) say so directly: `c$55635 = 128 (0x80), size = 16`, with the three
`fdivs` each storing to `c$55635+0/+4/+8(r31)` and `c.alpha` stored to
`c$55635+0xc`. So our source **already** divides into a stack-resident aggregate
whose red is read back before being overwritten — the variant FF proposed to
build is the code we had at 97.358795. The aggregate temp is present on both
sides and cannot be what denies the reciprocal CSE; the hypothesis was refuted
on structure and not built.

What actually denies the reciprocal CSE (§2) is the **named divisor**.

## 2. V1 — inline divisor at every site (CONFIRMED): 97.358795 → 97.995370

House rule for `/fp:fast` (DC3 `99ac433c2`): a **named local divisor** feeding
several divisions is what lets MSVC compute one reciprocal and multiply; spelling
the divisor **inline as an expression at each site** keeps a real `fdivs` per
site. FF's respellings all kept a named `range`/`float` divisor, which is why
they were byte-inert.

Edit (`e633f726`): the bloom block divides by `(4.0f - minVal)` at each of the
three sites. Predicted: three `fdivs`, one `fdivs`+two `fmuls` gone. Measured:
three `fdivs`, LoadRev 97.358795 → **97.995370**; ctor 100.0; Load unchanged.
The remaining charged sites were then all in two clusters: the `ObjPtr<RndDrawable>`
two-arg ctor call (retail inlines it as three stores) and the r10/r11 order in the
`mBloomColor` copy.

## 3. V2 — `RB3_OBJPTR_INLINE_TWOARG_CTOR`: 97.995370 → 99.187500

FF's declined lever, re-opened per the brief once the reciprocal was solved.
TU-local `#define` on line 1 of `rndobj/PostProc.cpp` (`rndobj/` is PCH-excluded,
so no cascade). Measured LoadRev **99.187500**, ctor **100.0** (the
`FORCEINLINE_CTOR` leak FF warned about did not occur — the ctor's 4-site
`ObjPtr<RndTex>` construction stays out-of-line under this lever, exactly as the
single-call-site inlining heuristic predicts). Two residual clusters:

1. inlined-ctor store order: retail `{lis vptr, stw mOwner, addi, stw mObject,
   stw vptr}`; ours stored `mOwner`/`mObject` from the mem-init list, which the
   base `ObjRefConcrete` ctor floats above the derived `lis`;
2. `mBloomColor = c;` — retail materialises `&mBloomColor` in r11 and `&c` in r10.

## 4. V3 — DEFER-BOTH inline lever + reference-copy: 99.187500 → **100.000000**

Pre-registered before the build: LoadRev +2.8125 pp to 100.0, Δmatched_functions
+1, Δmatched_code +1,728, ctor held, Load unchanged, no other row moving.
Falsifiers: ctor off 100; Load moving; any row outside PostProc moving; LoadRev
short of 100 with the store order still wrong (would mean the lever does not
control emission order).

- `src/system/obj/Object.h` — new `RB3_OBJPTR_INLINE_TWOARG_CTOR_DEFER_BOTH`
  branch: the inline two-arg `ObjPtr` ctor calls the **empty** base default ctor
  and assigns `mOwner` then `mObject` in the derived body, so both stores land in
  the derived `lis→addi→stw vptr` gaps in retail's order. `ObjPtr_p.h`'s
  out-of-line two-arg ctor is guarded off under the same define. Object.h is a
  PCH input (cascade served from objcache; the new branch is dead in every TU
  that does not define the lever).
- `rndobj/PostProc.cpp` — line 1 `#define RB3_OBJPTR_INLINE_TWOARG_CTOR_DEFER_BOTH 1`;
  `mBloomColor = c;` → `Hmx::Color &bloomColor = mBloomColor; bloomColor = c;`
  (DC3's spelling; makes the destination address a named value so it gets its
  own register).

Measured exactly as pre-registered (table in §0). Commit `1d492558`.

## 5. `Load` (192 B) — the aligned-bank finding, FF's "hole" claim refuted, and the residual

FF's Load table (retail / ours at 99.854164): `bool 0x50/0x50`, `dRev 0x58/0x5c`,
`int i 0x5c/0x60`, `Vector3 v 0x60/0x70`, `float f 0x70/0x58`, frame `0xa0/0xb0`
(7 charged sites), with the claim that "the 12-byte hole is the entire 16-byte
excess". FF measured declaration order (twice) and initializer form inert.

### 5.1 Why declaration order was inert: MSVC lays locals out in two banks

The compiler's own `/FAs` equates (scratch compile with `OBJCACHE=off`, `/Fo`
under `~/tmp`, tree untouched) show a **scalar bank** and a **16-aligned bank**:
`b70$ = 0x50 (1 B), rev$ = 0x54, dRev$ = 0x58, i5c$ = 0x5c`, then the Vector3s
at 0x60/0x70. A plain `float` is a scalar and lands in the scalar bank whatever
its declaration position — no reorder can move it above an aggregate. Retail's
"float" at 0x70 is above the aggregate, so it is not a plain scalar local. The
observed scalar order (`b70` first although declared third, then the ints in
declaration order) fits `(size ascending, then first use)`; the aligned bank puts
the Vector3 whose first use is a store (`f30 = 0`) low in both E1 and E2.

### 5.2 E1 — host the float in a second Vector3 (DC3's spelling): 99.854164 → 99.895836

Pre-registered: scalar bank comes out `dRev@0x58, i5c@0x5c` (retail), charged
sites 7 → ≤5; whether the aligned bank orders `v40@0x60 / v30@0x70` (retail) or
the reverse was the open question. Measured: **99.895836**, 5 charged sites,
scalar bank now identical to retail; aligned bank **swapped** (`v30@0x60`,
`v40@0x70`, rows 26/28/32); frame still `0xb0` vs `0xa0` (rows 2/46) **with the
12-byte hole gone** — so FF's "the hole is the entire excess" is refuted: the
locals span 0x50..0x80 on both sides of that claim and the frame did not move.
Only the Load row moved (whole-report row diff V3→E1: 1 of 69,240).

### 5.3 E2 — declare `v30` before `v40`: byte-identical to E1 (declaration order inert)

Pre-registered on the (wrong) reading that the aligned bank fills top-down in
declaration order. Measured **99.895836**, equates `v30$55620 = 0x60,
v40$55622 = 0x70` — the same slots as E1 with the symbol numbers reversed. So the
aligned bank's order is driven by neither declaration order nor symbol number
(DC3 found the same on cl 11886; reproduced on 10224).

### 5.4 The frame arithmetic closes exactly, and it identifies what retail's object is NOT

`__savegprlr_28` needs 0x28 B of save area at the top of the frame and MSVC sizes
the frame as `roundup16(locals_end + 0x28)`: ours `0x80 + 0x28 = 0xa8 → 0xb0`
(v40's 16-B slot at 0x70 ends at 0x80) — the "16 B unexplained" is exactly v40's
slot. Retail's `0xa0` forces `locals_end ≤ 0x78`, i.e. retail's object at 0x70 is
**≤ 8 bytes** (a 4-byte float read with `ReadEndian(…, 4)`), sitting **above**
the 16-aligned Vector3 at 0x60. ⚠ This differs from DC3's image, where the
target frame is 0xc0 vs DC3's 0xb0 — there the retail slot *is* 16-aligned
(cl 11886); on our 10224 image it is a small object above the aligned bank.

### 5.5 E3 — an inlined helper's `float` local (temp-after-named-locals hypothesis): 71.041664, REFUTED twice over

Pre-registered outcomes: 100.0 if the temp is allocated above the aggregate and
its zero-store schedules before the bool-read call; ~99.85 if it sorts into the
scalar bank; <99 if the zero-store is emitted at the inlining point. Measured
**71.041664**: the inlined `f` was allocated at **0x58 in the scalar bank**
(dRev → 0x5c, i5c → 0x60 — inlined-callee locals are sorted with the named
scalars on 10224), **and** the zero-store was emitted at the inlining point, not
hoisted above the preceding call (6-insert/6-delete cluster, rows 22–40). Only
the Load row moved. Reverted to the E2 spelling (`e4` build re-reads 99.895836).

⇒ Retail's 4-byte object at 0x70 is not a named scalar (FF), not a Vector3
member (E1/E2), and not an inlined temp (E3); its zero-store precedes the
`bs >> bool` call in retail's *source* order (E3 shows 10224 does not hoist it).
The residual is 2 slot sites + the 2 frame sites; Load pays 0 B at 99.895836.
Kept at E2 on merit: the scalar bank now matches retail and the spelling is
DC3's, with the comment in `PostProc.cpp` recording the measured facts.


## 6. Authoritative A/B (`tools/ab_measure.py --patch`, detached at `b3171b99`)

### 6.1 Pre-registration (written and committed BEFORE the run — commit `5fd321bd` is the patch tip)

Patch: `git diff b3171b99 5fd321bd -- src` → `~/tmp/w16fh_lane.patch` (3 files:
`src/system/obj/Object.h`, `src/system/obj/ObjPtr_p.h`,
`src/system/rndobj/PostProc.cpp`). Worktree detached at `b3171b99`, then
`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-fh --patch ~/tmp/w16fh_lane.patch`.

Leg A assertions (the brief's wrong-base guard): `LoadRev` reads **97.358795**
and `Load` **99.854164** — FF's state, not FC's 94.0000 / 95.6875.

Predicted DELTAS (leg B − leg A):

| measure | predicted Δ | basis |
|---|---:|---|
| `matched_functions` | **+1** | LoadRev mpn 97.671295 → 100 |
| `matched_code` | **+1,728 B** | LoadRev fuzzy → 100; Load stays < 100 (pays 0) |
| `masked_equal_functions` | **0** | no funclet pairing touched |
| honest (`matched − masked_equal`) | **+1** | |
| `matched_code_percent` | **≈ +0.01686 pp** | 1,728 / `total_code` read from leg A (10,247,068 on my builds) |
| `fuzzy_match_percent` | **≈ +0.00012** | LoadRev +2.64 pp and Load +0.04 pp over 69,240 rows |
| `total_code`, `total_functions` | **0** | source-only patch, no pins |
| leg B recompiles | **≥ 1**, plausibly several hundred | `Object.h` is a PCH input (DS-1 measured 956 on an `Object.h` patch) |
| `none`-ruler control | moves the SAME direction (LoadRev's residual was real `fdivs`/`fmuls` bytes, not names) | patch kind = `source` ⇒ ALIAS_SUSPECT must NOT fire |

The prediction set is not "multiples of 1,728": the true Δcode is
`1,728 + Σ size(rows crossing to fuzzy 100) − Σ size(rows falling off)`, and I
will compute both sums from the row-diff of the two leg `report.json`s rather
than back out a story from the total.

Falsifiers (any one refutes the lane's claim that only the two PostProc rows moved):

1. Any row other than `?LoadRev@RndPostProc@@…` and `?Load@RndPostProc@@…`
   changes `fuzzy` or `mpn` between the leg reports (row-diff, all 69,240 rows).
2. `??0RndPostProc@@IAA@XZ` reads anything but 100.0 in leg B (the
   FORCEINLINE-leak control).
3. Δ`matched_code` ≠ +1,728 or Δ`matched_functions` ≠ +1.
4. Leg A does not read 97.358795 / 99.854164 on the two rows (wrong base).
5. ab_measure REFUSES (absent-vs-absent, unsettled, ruler swap) — then there is
   no number and this section says so.

### 6.2 Result — MEASURED, every falsifier held

Run dir `.ab_measure_runs/20260916-101100-w16fh-lane-1738841` (archived to
`~/tmp/w16fh_ab_result.json`, `~/tmp/w16fh_report_leg{A,B}.json`, log
`~/tmp/rb3_ab_w16fh_lane.log`). Tool blob matches HEAD; objdiff-cli stable
(`sha256:c1b7d952…`); ruler `name_check` from `objdiff.json`; leg A settled in 2
iterations (first did 965 msvc — the reflinked tree's own settling, discarded),
leg A report build 0 recompiles; patch applied to exactly the 3 files; leg B first
iteration **965 msvc / 6 patch steps** (the `Object.h` PCH cascade, as predicted),
settled in 2; tree restored and verified on exit.

| measure | leg A | leg B | Δ measured | Δ predicted |
|---|---:|---:|---:|---:|
| `matched_functions` | 44,011 | 44,012 | **+1** | +1 ✓ |
| `matched_code` (B) | 4,139,696 | 4,141,424 | **+1,728** | +1,728 ✓ |
| `masked_equal_functions` | 23,237 | 23,237 | **0** | 0 ✓ |
| honest | 20,774 | 20,775 | **+1** | +1 ✓ |
| `matched_code_percent` | 40.398834 | 40.415700 | **+0.016866 pp** | ≈+0.01686 ✓ |
| `fuzzy_match_percent` | 50.262268 | 50.262665 | **+0.000397 pp** | ≈+0.00012 ✗ (see below) |
| `total_code` / `total_functions` | 10,247,068 / 69,240 | same | 0 | 0 ✓ |
| `none` ruler `matched_code` | — | — | +1,728 B (44.534145 → 44.551006 %) | same direction ✓; NOT_APPLICABLE as an alias control (patch kind `source`) |
| units at 100 (mpn / all-rows-fuzzy) | 191 / 171 | 191 / 171 | 0 / 0 | — |

Falsifiers:

1. **Row-diff of the two leg reports, all 69,240 rows: exactly 2 rows moved** —
   `?LoadRev@RndPostProc@@…` 97.358795 → 100.000000 (mpn 97.671295 → 100) and
   `?Load@RndPostProc@@…` 99.854164 → 99.895836. Nothing else; no row appeared
   or vanished. ⇒ the `Object.h` change, recompiled into 965 TUs, moved nothing
   outside PostProc (the DEFER-BOTH lever is a per-TU opt-in and only
   `PostProc.cpp` defines it).
2. `??0RndPostProc@@IAA@XZ` = **100.000000** in leg B (668 B; the FORCEINLINE
   leak control).
3. Δ`matched_code` = +1,728 exactly; Δ`matched_functions` = +1 exactly.
4. Leg A read **97.358795 / 99.854164** — FF's base, not FC's.
5. Not refused.

The one miss is my own bookkeeping, not the measurement: the "leg0" aggregate I
predicted from (50.262543) is **V2's** aggregate (its archived report reads
exactly that), so the ≈+0.00012 was V2→V3 mis-transcribed as base→V3. Leg A's
50.262268 → V3/E4's 50.262665 is +0.000397 on both the in-run pair and my
archived per-build reports. (Side-fact worth keeping: objdiff's whole-binary
`fuzzy_match_percent` is **not** the byte-weighted row mean — that reads
50.193259 / 50.193705 on the same reports — so it cannot be derived by hand from
row scores; predict it only from an archived report.) Δfuzzy was never a
falsifier and no row-level claim depends on it.

**Verdict: the lane pays +1 function / +1,728 B / +0.016866 pp, exactly as
pre-registered, with the whole effect localised to the two PostProc rows.**


## 7. What was NOT done, and why

- FF's closed veins were not re-measured (rb3-Wii bloom respelling, declaration
  reorder, initializer form, `MILO_ASSERT` removal, if/else inversion,
  `FORCEINLINE_CTOR`).
- No permuter, no register-swap chasing: every residual charged site on LoadRev
  named a source construct and closed.
- Load was not driven past its residual (§5): the remaining two charged sites
  and the frame delta need a spelling neither oracle has; a 192 B row after
  three pre-registered builds is where the lane stopped.
- Nothing pushed; nothing touched in the main repo; no `function_analysis/`
  staged.
- The DEFER-BOTH lever was not surveyed for other TUs. It is a per-TU opt-in
  (`#define RB3_OBJPTR_INLINE_TWOARG_CTOR_DEFER_BOTH 1` on line 1), and the
  whole-binary row-diff shows it is inert everywhere it is not defined; whether
  any other row wants retail's `{lis, stw mOwner, addi, stw mObject, stw vptr}`
  store order is a separate survey lane, not this one.
- `RB3_OBJPTR_INLINE_TWOARG_CTOR` (FF's moot lever) was not re-opened on any
  other row.
- The frame-shape difference between our image and DC3's for `Load` (retail
  frame 0xa0 here vs 0xc0 there) was noted, not chased — DC3's spelling is the
  one kept, and its residual is the same residual.
- The orchestrator DB has no seeded row for either symbol (`report_result`
  answered "function not found in database" for both; the notes were recorded
  against the symbol name), so the DB was not updated with a row score — the
  numbers live in this doc and in `report.json`.
