# Lane PINFIX-2 — the head-side over-carve class, sized. **0 rows reachable by `splits.txt`.**

Tree: `9dbea49f`. Ruler `functionRelocDiffs=name_check`, read from
`report.json` `provenance`. `total_code` **10,320,664** (read from the key,
never hardcoded). Instrument: `tools/headcarve_census.py`; per-row output
`docs/decomp/headcarve-census-PINFIX2.json`.

**No source, splits, symbols or map change was landed, so there is no A/B and no
pre-registered delta. The deliverable is the size of the class and the proof it
is unreachable.**

## ⛔ FINDING 0 — the class as BRIEFED does not exist. 0 of 6,592 blocks.

Two lanes described `??_DPropertyEventProvider` as a row whose *"`.text` pin
begins mid-function"*. Tested literally against `symbols.txt`:

| check over all 6,592 `.text` blocks in `splits.txt` | count |
|---|---:|
| block **start** strictly inside a sized symbol | **0** |
| block **end** strictly inside a sized symbol | **0** |
| block start not a symbol start (all in inter-symbol gaps/padding) | 304 |

**A pin cannot begin mid-function**, and not by luck — jeff's split validator
hard-fails with *"Split … ends within symbol"* (SPLITBLOCK-1 verified this by
executing it). The auditor was **mutation-tested**: injecting a start 8 bytes
into `fn_82270000` is reported, so the zero is not vacuous.

⇒ The defect is one level down. **dtk's SYMBOL TABLE declares a function start
mid-function, and `splits.txt` faithfully inherits it.** That reframing is the
whole result: it moves the lever from a file this lane may edit to one it may
not.

### The worked instance

Retail `0x8268FC18..0x8268FC5C` is **one** function — a vbase-dtor closure that
installs two vptrs through vbase displacements and tail-calls `b 0x827680D0`.
dtk carved it into `fn_8268FC18` (0x1C) + `fn_8268FC38` (0x24) **and left the
instruction at `0x8268FC34` (`lwz r10, 4(r9)`) claimed by neither** — a 4-byte
hole. The map then put `??_DPropertyEventProvider@@QAAXXZ` on the *tail*, so the
row opens `add r10, r10, r11` on r11 defined at FC20 and r10 at FC34, both above
the boundary. SIZE4-1's observation was right; its *diagnosis* ("the pin") was
not.

## The census — 247 blocks, 12,800 B of heads, 0.124% of `total_code`

Signature: **execution falls through into a declared function start.** A real
function is never entered by fall-through. Mirror of SPLITBLOCK-1, which keyed
on over-carved *tails* reached by a *branch*; a fall-through head is a different
sub-class, and the NAMED row is the spurious piece rather than the run's head.

| measure | value |
|---|---:|
| `.text` function symbols | 69,061 |
| adjacent pairs examined | 69,060 |
| **flagged (fall-through-reached) heads** | **247 (0.358% null)** |
| bytes of flagged heads | **12,800 B = 0.124023%** |
| bytes of pred+head runs | 32,044 B = 0.310484% |
| NAMED in the map | 27 rows / 1,088 B |
| **NAMED + live** (fuzzy<100, non-`auto_*`, non-xdk) | **13 rows / 404 B = 0.003914%** |

Sub-strata (never pooled — a conditional-branch tail is weak evidence, a tail
ending mid-computation on a non-branch is strong):

| stratum | rows | share of null | flagged among `fuzzy==100` | depletion |
|---|---:|---:|---:|---:|
| `nonbranch` | 210 | 0.304% | 3 / 19,000 = 0.016% | **19.3×** |
| `cond_branch` | 37 | 0.054% | 3 / 19,000 = 0.016% | 3.4× |

## ★ THE INSTRUMENT WAS ANTI-DISCRIMINATING FIRST, AND THE CONTROL IS WHAT CAUGHT IT

The first run reported **685 rows** and a negative control of **1.184% flagged
among `fuzzy==100` rows against a 0.992% null** — i.e. flagged rows were
*enriched* in perfectly-matching rows. **A head at 100% proves its carve is
right** (SPLITBLOCK-1), so that is a broken detector, not a finding. Three
distinct bugs, all mine, all found by chasing the control rather than the result:

1. **Trailing padding inside a declared size.** `fn_82697FE8`'s last word is
   `0x00000000`; `is_terminator(0)` is false ⇒ fake fall-through. I applied the
   padding test to *gaps* only, never to the predecessor's own last word. MSVC
   does not pad mid-function, so a padded tail is itself proof the function
   ended.
2. **The 8-byte EH prefix typed as a function.** A `.text` pointer + an `.rdata`
   pointer before a real function (documented in `CLAUDE.md`) disassembles to
   plausible non-terminator `lwz`s — it flagged `?Poll@BandCharacter@@` at
   fuzzy 100. Keying on `size==8` was insufficient because **dtk sometimes sizes
   the prefix 4**, leaving the second pointer as a 1-word "hole"; that variant
   flagged `?SyncProperty@CharBlendBone@@`, also at 100. Fixed by testing the two
   words *before the head*.
3. **`use-before-def` fired on every prologue.** `std r30, -0x18(r1)` is a
   callee-save *store*, so r14–r31 are read-before-def universally. Narrowed to
   **r11** — the one scratch register that is neither an argument nor saved, and
   the register both worked instances actually trip.

⚠ **Stated cost of fix (2):** `lwz r16`–`lwz r23` encode as `0x82xxxxxx` and are
genuinely ambiguous with pointers, so the exclusion can over-fire. It therefore
biases toward **under**-counting — the safe direction for a census whose verdict
is "small", but it means 247 is a floor, not a point estimate.

## ⛔ FIXABILITY: 0 of 247 reachable by `splits.txt`, and the reason is structural

jeff's Class-4 pass (`merge_branch_reached_overcarve_tails`, `src/cmd/xex.rs`)
builds `xref_src` from **relocations** and requires a branch into the tail
(P2′/P3′ `has_external_ref`). Measured over the whole image by decoding every
branch and scanning every data word:

- **155 of 247 have neither a branch reference nor a data-pointer reference.**
  Pure fall-through ⇒ **Class-4 can never fire**, whatever `splits.txt` says.
- **12 of the 13 live named rows have zero branch refs.** The 13th
  (`??0ExpInterpolator@@`, 20 B) has one *internal* ref — P2′/P3′ satisfied — but
  its gap is `hole:1w`, so **P1 exact adjacency fails by exactly 4 bytes**, which
  is bit-for-bit DataNode's blocker.
- **0 of 247 are Morph-shaped.** Of the 38 rows satisfying P1+P2′+P3′, **every
  one has pred and head in the SAME split block**, so P5 — the only guard
  `splits.txt` controls — is already satisfied. And **all 38 are unnamed in the
  map**, so by SPLITBLOCK-1's decisive lesson they move 0 → 0 anyway.

`splits.txt` gates **P5 only**. Nothing in this class is blocked on P5. That is
why the lane lands no fix, and it is a proof rather than a shrug.

## What was deliberately NOT done

- **jeff was not touched, built, or benchmarked.** A fall-through over-carve
  merge is a *new* pass (Class-4 is branch-gated by construction), i.e. strictly
  more than DataNode's P1 relaxation, for **404 B / 0.0039%** — against a
  fleet-shared binary consumed by rb3-xenon *and* dc3-decomp. The standing
  recommendation against the P1 relaxation applies a fortiori; I found no new
  evidence that satisfies it.
- **The map was not re-homed.** Moving `??_DPropertyEventProvider` from
  `0x8268FC38` to `0x8268FC18` cannot close the row: the target symbol would be
  0x1C bytes against our 0x44-byte function, so it still cannot reach `fuzzy`
  100. Re-homing is *not* metric-neutral (+3 fns / +428 B measured elsewhere), so
  this would be an unpriced change with no mechanism to pay.
- **No pin was extended.** Pulling `PropertyEventProvider.cpp`'s block back to
  `0x8268FC18` is *adding* a pin over `auto_*` code — metric-neutral, Δ exactly
  0 — and merges no symbols, so it buys nothing.
- **Mission item #2 (`DataNode::operator==`) was not re-opened.** SPLITBLOCK-1
  already censused and retired it (246 blocks / 9,968 B; 1 closed, 1 priced and
  deferred, 244 gated behind the identification map). Re-funding it was declined
  on the in-tree record, not re-measured.

## The 13 live named rows (the entire economic surface, 404 B)

| addr | B | fuzzy | mpn | unit |
|---|---:|---:|---:|---|
| `0x8235c610` | 96 | 0.00 | 3.54 | CharClip (`_Rb_tree::_M_copy`) |
| `0x826efc98` | 88 | 0.00 | 4.09 | band3/game/TrainerGemTab (`Render`) |
| `0x82433b38` | 84 | 0.00 | 1.67 | Cam (`_outline_SetFrustum`) |
| `0x8268fc38` | 36 | 0.00 | 5.00 | PropertyEventProvider (`??_D`) |
| `0x826f0f78` | 28 | 0.00 | 7.14 | PropertyEventProvider (`_Rb_tree::clear`) |
| `0x824f6340` | 20 | 0.00 | 0.00 | Interp (`ExpInterpolator::ctor`) |
| `0x825f22f8` | 16 | 25.00 | 25.00 | TexLoadPanel (`Object::PostSave`) |
| `0x82b90798` | 12 | 40.00 | 40.00 | band3/bandtrack/TrackPanel |
| `0x82422ce8` | 8 | 0.00 | 2.50 | PropKeys (`KeyGreaterEq`) |
| `0x82376d38` | 4 | 0.00 | 0.00 | CharServoBone (XDK name, homing defect) |
| `0x8268b8a0` | 4 | 95.00 | 100.00 | BandUser |
| `0x8271eef0` | 4 | 95.00 | 100.00 | Emitter |
| `0x828175e8` | 4 | 40.00 | 40.00 | UIComponent |

## Verdict

**The head-side over-carve class is RETIRED alongside SPLITBLOCK-1's tail-side
one.** 247 blocks / 12,800 B exist; 13 rows / **404 B (0.0039% of `total_code`)**
are economically live; **zero** are reachable by `splits.txt`, and closing them
needs a *new* jeff pass rather than a relaxation of an existing one. Nobody
should grind these rows, and nobody should re-brief "the pin begins
mid-function" — **that class is empty by construction.**
