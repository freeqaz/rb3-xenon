# W16-BH — `0x8227a828` is `??0BandSong@@QAA@XZ`, and the whole `Ham.cpp:` pin was a DC3 scaffold

**Date:** 2026-09-15 · **Branch:** `w16-bh` · **Base:** `a5a974aa3fc3`
**Commits:** `df0c3068`, `a62529c2`, `27f3515f` (+ this doc)
**Measured:** **+2 matched_functions / +160 matched_code bytes**, predicted +2 / +160.

---

## 1. The question

Map row `0x8227a828` was named `??0HamSong@@QAA@XZ`. RB3 has no `HamSong` —
that class is Dance Central 3's. Was this a **DC3 name transfer** onto a retail
RB3 address, i.e. is the row really `??0BandSong@@QAA@XZ`?

It was flagged by an earlier lane's `_dc3_only_pins_comment`, which nulled 69
DC3-only pins but deliberately **left 40 at-100% rows as a RENAME worklist, not
a deletion worklist**. `0x8227a828` is one of those 40.

## 2. Item 1 — decided on retail bytes (RTTI `??_R4` COL walk)

The ctor at `0x8227a828` is 164 B. Retail bytes (from `orig/45410914/band.exe`,
imagebase `0x82000000`):

```
7d8802a6 485aea31 3be1ff80 9421ff80 3ba00000 907f0094 7c7e1b78 2b040000
93bf0050 419a0020 3d608204 38630088 396b38d8 917e0004 484e2329 39600001
917f0050 38840000 7fc3f378 4854c2c5 3d608201 3d408201 396b0110 394a00e4
917e0000 3d608201 915e0010 3d208201 394b008c 3929007c 913e0018 817e0004
816b0004 7d6bf214 914b0004 817e0004 816b0004 7fabf12e 7fc3f378 383f0080
485ae9e4
```

This is multiple-**and**-virtual inheritance codegen: a vbtable pointer at
`this+4`, vfptrs stored at `this+0`, `this+0x10`, `this+0x18`, and a
virtual-base vfptr written through the vbtable
(`lwz r11,4(this); lwz r11,4(r11); add r11,r11,this; stw <vt>,4(r11)` — the
`817e0004 816b0004 7fabf12e` tail).

Walking each installed vtable back 4 bytes to its `??_R4` Complete Object
Locator (`{signature@0, offset@4, cdOffset@8, pTypeDescriptor@0xC,
pClassDescriptor@0x10}`, big-endian), then the TypeDescriptor's name at `TD+8`:

| stored at | vtable | COL | offset | cdOffset | TypeDescriptor |
|---|---|---|---|---|---|
| `this+0x00` | `0x82010110` | `0x821cf728` | 0 | 0 | `0x82c6b6f4` |
| `this+0x10` | `0x820100e4` | `0x821cf870` | 16 | 0 | `0x82c6b6f4` |
| virtual base | `0x8201008c` | `0x821cf884` | 136 | 4 | `0x82c6b6f4` |
| `this+0x18` | `0x8201007c` | `0x821cf898` | 24 | 0 | `0x82c6b6f4` |

**All four COLs point at one TypeDescriptor, `0x82c6b6f4`, whose name string at
`0x82c6b6fc` is `.?AVBandSong@@`.** The ctor constructs a `BandSong`.

Corroboration, five independent channels:

1. **Negative on the whole image.** A plain substring scan (Python, both
   `band.exe` and `default.xex` — `grep` is binary-blind in this shell) finds
   `HamSong` **0 times**, `.?AVHamSong@@` **0 times**, and `.?AVBandSong@@`
   **exactly once** (band.exe offset `0xc592fc` = VA `0x82c6b6fc`). A class with
   four RTTI-bearing vtables **must** have its type-name string in the image.
   `HamSong` has none. It is not in this binary in any form.
2. **DC3's own map.** `../dc3-decomp/orig/373307D9/ham_xbox_r.map` has
   `??0HamSong@@QAA@XZ` at `0x82459cc8` in `hamobj:Ham.obj`, with
   `?StaticClassName@HamSong@@` `0x82459c48`, `?StaticClassName@Song@@`
   `0x82459b28` and `?NewObject@HamSong@@` `0x8245bcf0` — the same neighbourhood
   geometry as our block. DC3's map contains **zero** occurrences of
   `"BandSong"`. The name came from DC3; the address is RB3's.
3. **`0x8227b038`** — slot 0 of the `this+0x10` vtable — is *already* mapped
   `??_EBandSong@@WBA@AAPAXI@Z` in our own map. The map already said BandSong
   one word away.
4. **Our compiled objects.** `??0BandSong@@QAA@XZ` (in
   `src/system/bandobj/BandCharacter.obj`) and `??0HamSong@@QAA@XZ` (in
   `src/system/hamobj/Ham.obj`) are **byte-identical** COMDATs (240 B, symval
   `0x8`), differing only in how their relocations are spelled. So the rename
   could not lose information — but see §3, it could lose *pairing*.
5. **The apparent contradiction, resolved.** `0x8227A7A8` (88 B) is
   `?StaticClassName@Song@@SA?AVSymbol@@XZ`; it interns the literal at
   `0x82010000`, which is **`"Song"`, not `"BandSong"`**. That looked like it
   impeached the map. It does the opposite: DC3's
   `src/system/hamobj/HamSong.h` declares `OBJ_CLASSNAME(Song)` — the Milo idiom
   deliberately registers the game's `Song` subclass under the **generic** name.
   The string is what a `BandSong`/`HamSong`-shaped class is *supposed* to
   intern. (Our own `src/system/hamobj/HamSong.h` says `OBJ_CLASSNAME(HamSong)`,
   which is why this looked wrong at first glance — ours is the DC3 port, not
   retail's shape.)

**VERDICT: `0x8227a828` is `??0BandSong@@QAA@XZ`. Renamed.**

## 3. Item 2 — the rename alone would have LOST bytes; the block had to move

`??0BandSong@@QAA@XZ` is defined by exactly one compiled object in the tree —
`src/system/bandobj/BandCharacter.obj` (verified by COFF symbol scan over all
1,215 compiled objects). The `0x8227a828` block was pinned to `Ham.cpp:`, whose
base object is `Ham.obj`, which defines only the *HamSong* spelling.

**objdiff pairs target↔base by NAME**, so a rename in place would have left the
row naming a symbol its own base object cannot define — a permanent 0%, i.e. a
net **loss** of 164 B for a *more accurate* map. The rename is only safe
together with a re-home to the unit whose base object **defines** the name.

Adjudicating the other two blocks of the `Ham.cpp:` entry then drained it
entirely:

- **Block 1** (`0x8227A728`–`0x8227A7A8`) is the `Song` slot of a 128-byte-
  per-class `OBJ_CLASSNAME` run whose order matches the `.rdata` class-name
  table; its neighbours on both sides are already `BandCharacter.cpp:`.
- **Block 2**'s lower boundary (`0x8227A800`) even **split
  `?StaticClassName@BandSong@@` from its own guard thunk** — a boundary no real
  TU has.
- **Block 3** (`0x82545C88`–`0x82545CA0`) is three 8-byte leaf accessors
  (`c0230058 4e800020`, `c023001c 4e800020`, `c0230020 4e800020`).

All three are carved out of `BandCharacter`'s interleaved address range. Our
`src/system/hamobj/Ham.cpp` is DC3's `HamInit()`/`HamTerminate()`, registering
HamCharacter, HamDirector, HamSong, DancerSequence, RhythmBattle,
HollaBackMinigame, MoveGraph — **none of which exist in RB3**. It is
compile-only scaffolding, and it was pinned over BandCharacter's code.

⚠ **One brief statement was wrong and is corrected here:** the brief said
`0x8227a7a8` "falls in the GAP between blocks 1 and 2 — currently unpinned".
It does not. `BandCharacter.cpp:` already owned `.text 0x8227A7A8–0x8227A800`,
and `fn_8227A7A8` already appeared in `BandCharacter.s`.

### splits.txt — before / after

**BEFORE** (`Ham.cpp:` — the whole entry):

```
Ham.cpp:
	.pdata      start:0x821F1FC8 end:0x821F1FD8
	.pdata      start:0x821F1FE0 end:0x821F2000
	.text       start:0x8227A728 end:0x8227A7A8
	.text       start:0x8227A800 end:0x8227A948
	.text       start:0x82545C88 end:0x82545CA0
```

**AFTER:** the entire `Ham.cpp:` entry is **deleted** (a move that drains a
unit's last `.text` block must delete the heading, or the split emits a 42-byte
object and `report.json` hard-fails on it). The three `.text` lines were merged
into `BandCharacter.cpp:` in ascending order:

```
BandCharacter.cpp:
	…
	.text       start:0x8227A728 end:0x8227A7A8      <- moved in
	.text       start:0x8227A800 end:0x8227A948      <- moved in
	…
	.text       start:0x82545C88 end:0x82545CA0      <- moved in
	…
	.pdata      start:0x821F1FC8 end:0x821F1FD8      <- re-derived by dtk
	.pdata      start:0x821F1FE0 end:0x821F2000      <- re-derived by dtk
```

Only `.text` was hand-edited. The two `.pdata` ranges are **derived output** —
dtk re-derives the entire `.pdata` split set from `.text` on every split run.
The first build after the move therefore **failed rc=1 with the split guard**
("THE SPLIT REWROTE ITS OWN INPUT … its output is not a fixed point"), exactly
as the house rule predicts; recovery is one build, and the re-derived file is
committed as `a62529c2`. I read that as dtk **independently corroborating** the
re-home: it assigns `.pdata` ownership from function ownership, and it moved
those two ranges onto `BandCharacter.cpp:` on its own.

### Map edits (3 keys, 29,484 → 29,485 rows)

| address | before | after |
|---|---|---|
| `0x8227a828` | `??0HamSong@@QAA@XZ` | `??0BandSong@@QAA@XZ` |
| `0x8227a910` | *(absent)* | `?ClassName@BandSong@@UBA?AVSymbol@@XZ` |
| `0x8227b050` | `?ClassName@BandCharacter@@$4PPPPPPPM@A@BA?AVSymbol@@XZ` | `?ClassName@BandSong@@$4PPPPPPPM@A@BA?AVSymbol@@XZ` |

Written with `json.dumps(d, indent=1, ensure_ascii=False)+'\n'`; no reformat.
Injectivity checked before and after — the map's two duplicate names
(`?NodeCmp@@YAHPBX0@Z`, `??$__destroy_aux@ULevelData@@…`) are **pre-existing**
and I introduced none.

### The third edit was NOT in the plan — it is the one FELL OUT row I had to diagnose

The first measurement read **+1 fn / +148 B** against a predicted +2 / +160.
The shortfall was one unpredicted row:
`default/BandCharacter::?ClassName@BandCharacter@@$4PPPPPPPM@A@BA?AVSymbol@@XZ`
(12 B) falling 100 → 98.333336, its sole charge at instruction 2 —
target `b ?ClassName@BandSong@@UBA?AVSymbol@@XZ` vs base
`b ?ClassName@BandCharacter@@UBA?AVSymbol@@XZ`.

That is not damage from my edit; it is a **pre-existing map defect my naming of
`0x8227A910` EXPOSED**. Retail `0x8227b050` is

```
8164fffc  lwz  r11,-4(r4)
7c8b2050  subf r4,r11,r4
4bfff8b8  b    0x8227A910      <- BandSong::ClassName
```

while `?ClassName@BandCharacter@@UBA?AVSymbol@@XZ` is at `0x822896e0` and its
real adjustor is the separate `0x82289748` (`b 0x822896E0`). Before I named
`0x8227A910`, the branch target was an unnamed placeholder — which
`name_check` **forgives** — so the wrong name cost nothing and was invisible.
Naming the destination converted a forgiven site into a checked one and the
defect surfaced. This is the "naming is a bet, not a freebie" mechanism working
exactly as documented, and the payout is **bug exposure**.

Adjudicated rather than guessed: a scan of all **452** twelve-byte
`lwz r11,-4(r4); subf r4,r11,r4; b X` adjustor thunks in retail finds **exactly
one thunk per destination**, so ICF cannot have folded two adjustors together
and the destination address *is* the identification. In-tree doctrine sanctions
the repair — `_bijection_arbitrary_comment` (which lists `0x8227b050`) calls
swapped-name rows "a REPAIRABLE row defect … Repairing such a row is
positive-yield; aliasing it destroys the evidence", and
`_single_branch_thunk_misnames_comment` says each is "verified by dumping its
destination … repair is positive-yield".

## 4. Prediction vs measurement

Predicted, written into `df0c3068` **before** building: **+2 functions /
+160 bytes** — the six moved rows netting 0, plus `?NewObject@BandSong@@` (112 B)
and `?ClassName@BandSong@@UBA…` (48 B) newly pairing.

Measured by set-diff of the `fuzzy == 100` row set
(`tools/rowset_snapshot.py`, baseline `~/tmp/w16bh/rows_base.json`, 40,860 rows),
after a full `./tools/ninja-locked` at rc=0:

```
CROSSED IN : 9 rows, 564 B
   +    164 B  default/BandCharacter::??0BandSong@@QAA@XZ
   +    112 B  default/BandCharacter::?NewObject@BandSong@@SAPAVObject@Hmx@@XZ
   +     88 B  default/BandCharacter::?StaticClassName@Song@@SA?AVSymbol@@XZ
   +     68 B  default/BandCharacter::fn_8227A8CC
   +     48 B  default/BandCharacter::?ClassName@BandSong@@UBA?AVSymbol@@XZ
   +     32 B  default/BandCharacter::fn_8227A780
   +     32 B  default/BandCharacter::fn_8227A800
   +     12 B  default/BandCharacter::?ClassName@BandSong@@$4PPPPPPPM@A@BA?AVSymbol@@XZ
   +      8 B  default/BandCharacter::?EndFrame@Song@@UAAMXZ
FELL OUT   : 7 rows, 404 B
   -    164 B  default/Ham::??0HamSong@@QAA@XZ
   -     88 B  default/Ham::?StaticClassName@Song@@SA?AVSymbol@@XZ
   -     68 B  default/Ham::fn_8227A8CC
   -     32 B  default/Ham::fn_8227A780
   -     32 B  default/Ham::fn_8227A800
   -     12 B  default/BandCharacter::?ClassName@BandCharacter@@$4PPPPPPPM@A@BA?AVSymbol@@XZ
   -      8 B  default/Ham::?EndFrame@Song@@UAAMXZ
NET bytes  : +160
  matched_functions      43525 -> 43527   delta +2
  matched_code           4038804 -> 4038964   delta +160
  matched_code_percent   39.418335 -> 39.419895   delta +0.001560
  fuzzy_match_percent    49.690746 -> 49.691216   delta +0.000470
```

**Every FELL OUT row explained** (the snapshot's row key is
`unit::name`, so a row that changes unit necessarily appears on both sides):

| fell out | bytes | why |
|---|---|---|
| `default/Ham::??0HamSong@@QAA@XZ` | 164 | MOVE + RENAME → `default/BandCharacter::??0BandSong@@QAA@XZ`, crossed in at 164 |
| `default/Ham::?StaticClassName@Song@@SA?AVSymbol@@XZ` | 88 | MOVE → same name under BandCharacter |
| `default/Ham::fn_8227A8CC` | 68 | MOVE |
| `default/Ham::fn_8227A780` | 32 | MOVE |
| `default/Ham::fn_8227A800` | 32 | MOVE |
| `default/Ham::?EndFrame@Song@@UAAMXZ` | 8 | MOVE |
| `default/BandCharacter::?ClassName@BandCharacter@@$4…` | 12 | the exposed thunk defect (§3), crossed back in as `?ClassName@BandSong@@$4…` at 12 |

Moves net **exactly 0** (392 B out of `default/Ham`, 392 B into
`default/BandCharacter`). The `+160` is precisely the two predicted new
pairings. **Prediction met on the nose.**

## 5. Item 3 — not applicable

Item 3 was conditional on `0x8227a828` being genuinely `HamSong`, in which case
a fold to T1 standard would have had to be proven before installing an alias.
It is not HamSong (§2), so **no alias was installed and none was considered.**

## 6. Item 4 — REPORT ONLY: other `Ham*` classes with no RTTI in retail

Method: collect every `Ham`-prefixed identifier token appearing in a **named**
value of `scripts/target_symbol_map.json`, then test each for `.?AVHam…@@`
(RTTI type-name) and for the bare class name as a plain substring, in **both**
`band.exe` and `default.xex` (Python, not `grep`).

**5 distinct tokens remain, across 9 named map rows. All 5 have neither an RTTI
string nor a bare substring anywhere in either retail image.**

| class token | named map rows | `.?AV…@@` in retail | bare string | in `.xex` | addresses |
|---|---:|---|---|---|---|
| `HamMove` | 7 | no | no | no | `0x82714858`, `0x827148b0`, `0x82715530`, … |
| `HamLabelCountDoneMsg` | 2 | no | no | no | `0x82340580`, `0x82340940` |
| `HamIKEffector` | 2 | no | no | no | `0x823c3300`, `0x823c35d8` |
| `HamNavProvider` | 1 | no | no | no | `0x8230c200` |
| `HamMasterLoader` | 1 | no | no | no | `0x8276e328` |

Before this lane's rename the set was **6** — `HamSong` was the sixth, and
`0x8227a828` was its only row. It is now drained.

⚠ **Absence of an RTTI string is NOT by itself proof of a DC3 name transfer.**
`.?AVX@@` is emitted for a polymorphic class or one used in EH; a non-polymorphic
helper legitimately has none. `HamSong` was settled because a **4-vtable**
class necessarily carries one, plus four other channels. These 5 are **reported,
not adjudicated, and nothing was edited.** `HamMove` (7 rows, one address
cluster around `0x827148xx`) is the obvious next candidate for the same
treatment.

## 7. Gates

Run in order in the worktree, native gate LAST:

- full `./tools/ninja-locked` → **rc=0** (`~/tmp/rb3_build_w16bh_4.log`)
- `scripts/verify_ruler_agreement.py --check` → **OK**, both entry points
  resolve `name_check` / `combineDataSections` / `combineTextSections` /
  `ppc.calculatePoolRelocations=false`
- `scripts/verify_objs_patched.py --verify-manifest` → **OK**, 1,215 decomp +
  3,113 target objects, `tree_sha256=889375c6931e4b91`; denylist clean
- `tools/icf_alias_finder.py --validate` → **PASS** — 1,404 map-consistent,
  247 tolerated, **0 CONTRADICTED**, 1,652 total
- `tools/native_build_gate.sh` → see the verbatim `NATIVE_GATE_RESULT` line in
  the lane report.

## 8. What I did NOT do, and why

1. **`0x82289748` → `?ClassName@BandCharacter@@$4PPPPPPPM@A@BA?AVSymbol@@XZ`.**
   Proven by its branch target (`b 0x822896E0` = `ClassName@BandCharacter`),
   currently **null-valued** in the map. Not named, because it is also
   **UNPINNED**: it sits in the 12-byte gap between the `BandCharacter.cpp:`
   blocks `0x82289710–0x82289748` and `0x82289754–0x8228A23C`. A name on an
   unpinned address is inert, and extending an unrelated block is outside this
   lane's splits scope. **The full proof is handed to the follow-up lane** — this
   is a one-line splits edit plus a one-key map edit, worth 12 B.
2. **The now-stale `_bijection_arbitrary` membership for `0x8227b050`.** That
   annotation is a **shared** key and the brief restricts lanes to disjoint
   keys; leaving it costs nothing (it is an advisory channel, not a scoring one).
3. **Naming `fn_8227A780` / `fn_8227A800`.** Both already cross at 100 via
   funclet/byte-signature pairing, so naming them is a bet with **zero byte
   upside** and real downside under `name_check`.
4. **`config/45410914/objects.json` untouched.** `system/hamobj/Ham.cpp` stays
   declared `NonMatching` compile-only scaffolding. It no longer owns any
   address range, which is correct — it is DC3 source with no RB3 counterpart —
   but removing the compile edge is a separate decision with a link-surface risk
   the native gate would have to price.
5. **`src/system/hamobj/Ham.cpp` and `HamSong.h` untouched.** Our `HamSong.h`
   says `OBJ_CLASSNAME(HamSong)` where DC3 says `OBJ_CLASSNAME(Song)`; fixing
   that is a source-accuracy change with no pinned address behind it now.
6. **Side observation, flagged UNVERIFIED and not acted on:** the 128-byte block
   at `0x8227A528`, currently on `BandCharacter.cpp:`, looks like it may be
   `BandFaceDeform`'s slot in the same `OBJ_CLASSNAME` run. I did not walk its
   RTTI and make no claim.

**If the verdict in §2 is wrong, here is what would change it:** a `.?AVHamSong@@`
byte string found anywhere in `orig/45410914/default.xex` or `band.exe`, or a
`??_R4` at `[vtable-4]` for any of the four vtables above resolving to a
TypeDescriptor other than `0x82c6b6f4`. Both were tested directly on retail
bytes and both came back negative.
