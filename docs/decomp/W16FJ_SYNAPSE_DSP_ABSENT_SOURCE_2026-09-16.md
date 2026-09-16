# W16-FJ — Synapse::DSP: two units whose source did not exist (2026-09-16)

**Result: +1,264 B / +10 functions / +0.012337 pp, six rows 0 -> 100, zero rows
lost anywhere in the binary.** Measured by `tools/ab_measure.py --from-dirty`,
both legs settled to zero work, both at a `symbols.txt` split fixed point,
`total_code` verified unchanged at 10,247,068 in both legs.

Branch `w16-fj`, commit `27b60df5`, off `main` at `63fa1e86`.

## 1. The 0.0% was NOT "no source". It was "no source IN THIS UNIT."

This is the transferable finding and it corrects the brief's framing.

Both units really do lack source — `PeakDetector.cpp` was a single comment line,
`PeakDetector.h` one line, and `GranularSynth.cpp` held only two STL vector
ctors against placeholder `u8 _pad[0x18]` structs. But that is not why the rows
scored zero. Measured directly on the built objects:

| symbol | `Synapse_dsp.obj` | `GranularSynth.obj` / `PeakDetector.obj` |
|---|---|---|
| `??1GranularSynth@Synapse@DSP@@QAA@XZ` | present (undefined extern) | **ABSENT** |
| `?Flush@GranularSynth@Synapse@DSP@@QAAXXZ` | present (undefined extern) | **ABSENT** |
| `?Detect@PeakDetector@Synapse@DSP@@QAAXI@Z` | present (undefined extern) | **ABSENT** |

`Synapse_dsp.cpp` carries its *own* local class definitions of `PeakDetector`
and `GranularSynth` (layout-compatible: `sizeof 0x44`, `mVoices` at `0x2C` as a
raw pointer aliasing the vector's `_M_start`), so our `~GranularSynth` compiled
into `Synapse_dsp.obj`. objdiff pairs a target row against the base object **of
the same unit**, so a row whose name the unit's base object cannot define reads
0% no matter how correct the code is. This was already on the record — the
`_denylist_comment` entry for `0x82b74600` says in as many words *"our
~GranularSynth compiles into Synapse_dsp.obj, not GranularSynth.obj, so the row
cannot pair until the pin is re-homed"*. It suggested re-homing the **pin**; the
cheaper fix is to define the body in the right `.cpp`, which is what landed.

⇒ **Before pricing a fuzzy-0 row as "we do not hold the body", check whether the
symbol is merely defined in the WRONG OBJECT.** The two look identical from
`report.json` and the remedies are completely different.

## 2. The 136-vs-140 B destructor: adjudicated on retail bytes, DC3 vindicated

The brief flagged this as the one thing not to assume, citing W16-FG's genuine
version delta (a struct 0x34 in RB3, 0x38 in DC3). It is **not** that shape here.

Lane W12-D had already read `0x82b74600` and recorded, in the denylist rationale,
that the body destroys three container members: a `vector<vector<float>>` at
**+0x38**, a buffer at **+0x2c** with element stride **0x18**, and a buffer at
**+0x20** with element stride **0x40** (`srawi`/`slwi 6`). That is exactly DC3's
layout — `mGranules@0x20` (Granule = 0x40), `mVoices@0x2c` (Voice = 0x18),
`mWindows@0x38` — so the layout is confirmed on RB3 retail bytes, independently
of DC3. Our own `Synapse_dsp.cpp` model (`sizeof 0x44`, `mVoices@0x2C`) agrees a
third time.

Our 136 B body then reached **byte-exact 100.0** compiled from DC3's source.
⇒ the 4 bytes are a difference between the two **retail images**, not between
our source and DC3's. Nothing needed "fixing", and had the lane forced our size
to 140 it would have broken a row that was already correct.

## 3. `?Synthesize` crosses a floor DC3 documents as source-inert (+780 B)

`Synthesize` ported in at **99.9487** — DC3's score to the last decimal — with
exactly one charged site, the one DC3 documents:

```
| 150 | `fadds f0, f10, f0` | `fadds f0, f0, f10` | diff_arg |
```

DC3's in-source note records `+=` and `s + out[...][k]` as both measured inert,
concluding *"the operand order is chosen after the expression is scheduled, not
from the source order"* — a commutative-operand-order floor. A spelling DC3 did
not try closes it:

```c
float acc = out[gr.mVoice][k];
out[gr.mVoice][k] = acc + s;
```

Giving the loaded value its own named lifetime (rather than reassociating the
`+`) flips the operand order and takes the row to **100.0**, worth **780 B** —
the largest single item in this lane.

⇒ **A twin's "measured inert" is evidence about the spellings it tried, not
about the site.** DC3 tried two reassociations; the lever was a lifetime, not an
association. Worth backporting: dc3-decomp's identical 780 B row is still at
99.9487.

## 4. `0x82b737c0` -> `??0PeakDetector`: proven on retail bytes, then installed (+68 B)

The brief listed this 68 B row as "no DC3 row (anonymous)". It has an exact
oracle: DC3's `??0PeakDetector` is **68 B at fuzzy 100.0**. It was not
unoracled — it was **unnamed in our target map**. Adjudicated before installing:

| retail instruction | field |
|---|---|
| `stw r4, 0x0(r3)` | `mInput = &input` @0x00 |
| `stw r5, 0x8(r3)` / `stw r6, 0xc(r3)` | `mWindowSize` @0x08 / `mHop` @0x0c |
| `stw r11(=0), 0x10/0x14/0x18/0x1c` | `mOrigin`/`mPeakPos`/`mStartPos`/`mLength` |
| **`stb r11, 0x20`** | `mTracking = false` — a **one-byte** store |
| `stfs f0(=0.0f), 0x4/0x24/0x28/0x2c/0x30` | `mWidth`/`mCurWidth`/`mCenter`/`mPeakValue`/`mNextCenter` |

17 instructions = 68 B. Every offset, every access width, and the byte-vs-word
distinction match the ported layout. It reached **byte-exact 100.0** on install,
which a wrong name cannot do.

## 5. The residual is DC3's, reproduced to four decimal places

| row | size | ours after port | DC3 |
|---|---:|---|---|
| `?Detect@PeakDetector` | 948 B | **96.8819** | 96.8819 |
| `?gaussianWindow@PeakDetector` | 268 B | **92.7612** | 92.7612 |
| `?ExtractGranules@GranularSynth` | 900 B | **91.6222** | 91.6222 |
| `?Synthesize@GranularSynth` (before §3) | 780 B | **99.9487** | 99.9487 |

Four independent rows reproducing the twin's score to the last decimal is strong
evidence that (a) the port is faithful and (b) RB3 and DC3 carry the *same code*
here, with the same regalloc/scheduling residual, in two different images.

⚠ **Pricing consequence the brief's headline understates.** `matched_code` pays
only at `fuzzy == 100`, so a *faithful* port of a row whose oracle sits below 100
pays **zero bytes**. The brief's "ORACLE-BACKED PRIZE: 3,228 B across 6 named
rows" was priced as if oracle-backed meant collectable; only the rows whose
oracle is AT 100 (or one site away) were ever collectable. Of the 3,228 B,
1,112 B crossed (`Synthesize` + `Flush` + `~GranularSynth`) and 2,116 B
(`Detect` + `gaussianWindow` + `ExtractGranules`) landed exactly on DC3's wall.
**Price an oracle-backed row by the oracle's DISTANCE FROM 100, not by its size.**

## 6. What I did NOT do, and why

- **Did not install `0x82b746e8` -> `??0GranularSynth`, though I PROVED it.**
  916 B, exactly DC3's ctor size; the body is 229 instructions and opens
  `stw r4,0x0` (`mInput`), `rldicl`+`fcfid`+`frsp` of r6 (`mHopF = (float)hop`),
  `stw r6,0x10` (`mHop`), `stfs f31,0x8`/`0xc` (`mOffset`/`mLengthMix` = 0.0f),
  `stw r28,0x18` (`mFrame = 0`) — our ctor's init list exactly. **Not installed
  because DC3's row is 97.4018, so it cannot reach `fuzzy == 100` and the byte
  upside is ZERO**, while naming converts currently-forgiven placeholder call
  sites into `name_check`-checked ones — a real, unmeasured downside. Mixing a
  zero-upside unmeasured change into a clean measured result is the wrong trade.
  **Ready for a follow-up lane that measures it in isolation**; its value would
  be legibility/bug-exposure, not bytes (cf. W16-EW, W16-FC).
- **Did not grind `Detect` / `gaussianWindow` / `ExtractGranules`** (2,116 B).
  They land exactly on DC3's documented wall, and DC3's in-source notes record
  **seven** spellings measured on `ExtractGranules` alone (hoisting `length` =
  89.1, a `windowLen` local = 87.6, `rounded` before the loop = 86.5, three
  inert) plus two on `Detect`. Unlike `Synthesize` these are 20-40 charged sites,
  not one, so no single spelling closes them. §3 shows DC3's negatives do not
  always transfer — this is a real vein for a permuter/regalloc lane, but it is
  not a source-spelling lane.
- **Did not copy DC3's `__uninitialized_fill_n<ObjectDir::Viewport>` explicit
  instantiation.** DC3 emits it because the folded COMDAT survivor in *its*
  image carries that spelling and its row must pair. RB3's counterpart row is
  **anonymous** (`fn_82B744E8`, 80 B), and objdiff cannot pair a placeholder
  target name by emitting any spelling, so it would be inert; skipping it also
  keeps `obj/Dir.h` out of the TU.
- **Did not touch `Synapse_dsp.cpp`'s local class definitions.** They are
  layout-compatible and it still holds 3,864 B; the new headers are not included
  there, so the two never meet in one TU. Verified unmoved by the A/B.
- **Did not work the 344 B arg-only class** (the 44/40 B rows at 99.45-99.90),
  per the brief. Two of them (`fn_82B74AD4`, `fn_82B74B00`) crossed anyway as a
  side effect of the real Granule/Voice types replacing the 0x18 placeholders.

## 7. Provenance / how to re-derive

```
scripts/setup_worktree.sh ~/tmp/wt-w16-fj w16-fj      # from main @ 63fa1e86
./tools/ninja-locked                                   # FULL build (never ninja <one>.obj)
python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-fj --from-dirty
```
A/B run dir: `.ab_measure_runs/20260916-100048-from-dirty-1693255/`
(patch sha256/16 `1b22c57830b030a9`; leg A 44012/4142976, leg B 44022/4144240).
Ruler `name_check` (graded), resolved from `objdiff.json`; objdiff-cli pinned
stable across both legs (`sha256:c1b7d95240a35cd6`).
