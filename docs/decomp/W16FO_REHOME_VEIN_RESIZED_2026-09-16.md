# W16-FO — the foreign-COMDAT re-home vein is 4.84x smaller than briefed (2026-09-16)

Base: `main` @ `3c6eacd1` (= `origin/main`). Worktree `~/tmp/wt-w16-fo`, branch
`w16-fo`. Ruler `name_check` (graded), objdiff 4.2.9, `tool_binary_hash`
`5a51cd51fe0a353f`.

Leg A measured byte-identical to main on every key:
`matched_code 4,147,612 / matched_functions 44,030 / 40.476086% /
fuzzy 50.345950 / total_code 10,247,068 / total_functions 69,240`.

## Result in one line

The re-home vein is **162 rows / 10,200 B**, not the briefed **423 rows /
49,364 B** — a **4.84x byte over-count**. **81.5% of the briefed bytes were pure
call sites.** One pin was moved (`NoteTube::CreateMeshes`, +0 B, fuzzy 0 -> 80.7).

## 1. Why the briefed figure was wrong, measured rather than asserted

The briefed instrument was `symbol_name.encode() in open(obj,'rb').read()`. A
symbol NAME appears in a COFF whether the symbol is **defined** there or merely
**referenced**, so it reports every *caller* as a *definer*.

I reproduced both instruments over the same row set. **The row selection is not a
variable**: my fuzzy-0 NAMED population is **5,637 rows / 2,376,232 B**, matching
the briefed population exactly, so the only thing that differs is the DEFINED
test.

| bucket | rows | bytes | share of vein bytes |
|---|---:|---:|---:|
| `DEFINED_IN_OWN_UNIT` | 51 | 5,456 | 0.23% |
| **`DEFINED_ELSEWHERE`** (the honest vein) | **162** | **10,200** | **0.43%** |
| `REFERENCED_ONLY` (call sites — the miscount) | 269 | 45,068 | 1.90% |
| `ABSENT_EVERYWHERE` | 5,155 | 2,315,508 | 97.44% |

My reproduction of the vacuous instrument lands at **431 rows / 55,268 B**,
within 8 rows of the coordinator's 423 — close enough to confirm the mechanism
rather than merely allege it. Of those bytes **81.5% are `REFERENCED_ONLY`**.

⇒ The brief's own guess that the third figure (5,147 "nowhere") was the only safe
one is **confirmed**: it is 5,155, a drift of 8 rows.

## 2. The instrument needed fixing in the OPPOSITE direction too

`undefined_externals_census.py:parse_coff_symbols` — the correct in-tree tool the
brief pointed me at — returns only **EXTERNAL** (sclass 2) definitions. A
file-static or anonymous-namespace body is **STATIC** (sclass 3, section > 0),
which is equally a definition objdiff can pair against.

Counting only EXTERNAL definitions would have missed **49 rows / 1,904 B =
18.7% of the honest vein — including its single largest row.**
`tools/rehome_census.py` tracks the two classes separately.

★ Both vacuities have the same shape and point opposite ways: one inflates the
vein with callers, the other deletes real definitions from it. Neither announces
itself.

## 3. ⛔ `DEFINED_ELSEWHERE` is NECESSARY BUT NOT SUFFICIENT — same spelling is not the same function

The largest row in the corrected vein — **576 B `gen_bitlen`**, single
destination `default/trees`, which is a **10/10 rows, 6,612/6,612 B, 100%**
complete zlib unit — looked like the cleanest possible candidate. It is a **false
positive**.

`gen_bitlen` is at `0x8292f980`. Its neighbours in `target_symbol_map.json` are:

```
0x8292f7b8  ?EffectsApiCall@CParse@D3DXShader@@...
0x8292f980  gen_bitlen                 <- bare name
0x8292fe70  ?compress_block@D3DX@@YAXPAUinternal_state@1@PAUct_data_s@1@1@Z
0x829302a0  ?build_tree@D3DX@@YAXPAUinternal_state@1@PAUtree_desc_s@1@@Z
```

⇒ this region is **D3DX9's own embedded copy of zlib**. The map spelled its
neighbours with the `@D3DX@@` decoration but gave this one address the **bare**
zlib name, which collides with our `trees.obj` static. Corroboration: the row
immediately after it, `fn_8292FBC0`, is **688 B — exactly the size of the
already-matched `send_all_trees` in our zlib unit**, because it is the same
source compiled with the same flags.

Pinning it into `trees.c` would have scored **D3DX's copy against our zlib** and
produced bytes for a body that is not the one pinned — metric fitting on XDK code
that is explicitly out of scope.

★ **The screen this adds: a bare C-style name can collide across two copies of a
vendored library; a C++-mangled name encodes namespace and signature and cannot
(cheaply) collide.** In this vein the bare-name class is exactly **1 row /
576 B**; the other **161 rows / 9,624 B** are mangled. So the collision hazard is
sized, not hand-waved — but it removes **5.6% of the vein, and specifically its
top row**.

## 4. Most of what remains is not capturable by a pin

A pin captures an address RANGE, not a row. The two largest blocks are
**multi-destination** and therefore cannot be taken by one pin:

| block | re-homable | destinations |
|---|---|---|
| `auto_03_82C45EC4_text` | 1,188 B / 43 rows | OvershellSlot(25), EditSetlistPanel(7), … |
| `auto_03_82743BD0_text` | 992 B / 5 rows | BandSongMetadata(4), DataNode(4), StringTable |

Taking the first would need ~43 separate distant `.text` blocks for 1,188 B.
Split by pin class: **pin-ADD (row in an `auto_*` unit) 65 rows / 3,644 B**;
**pin-MOVE (row in a pinned unit) 97 rows / 6,556 B**. Median row **28 B**; only
**8 rows are >= 200 B**.

⇒ The brief predicted a further ~13x candidate-to-executable gap (267 -> 59 rows
in a prior lane). **That shape repeated.** The honest vein is 0.0996% of
`total_code`; the largest single-destination capturable block in it is the 392 B
one I executed.

## 5. ⛔ The brief's #1 hand-off is NOT executable as a pin move

W16-FL's explicit hand-off was to re-home `0x826ffe38` (392 B) to `Sfx.cpp`,
since our `Sfx.obj` already defines the correctly-named counterpart. Measured
against the corrected criterion, that row is **`ABSENT_EVERYWHERE`**:

- retail's map name is `?_M_fill_insert_aux@?$vector@V**StepMoves**@@…` (a DC3
  name — FL proved this),
- our `Sfx.obj` defines `?_M_fill_insert_aux@?$vector@V**SfxMap**@@…`.

**objdiff pairs by NAME.** Re-homing changes *which base object is consulted*; it
does not change the **target** symbol's name, which comes from
`target_symbol_map.json`. After the move the target would still be spelled
`StepMoves` and `Sfx.obj` still would not define it — **still fuzzy 0**.

⇒ That hand-off requires a **map rename AND a re-home**, which is a different and
riskier lever (map-name economics: un-pairing is 80.5% of a map edit's delta, and
"proving a name wrong" is not the same as "renaming is safe"). **I did not do
it** — see §7.

## 6. The one pin that moved: `NoteTube::CreateMeshes` (+0 B, and that is the point)

`?CreateMeshes@NoteTube@@QAAXXZ` (392 B, `0x82c2ab08`) sat ~3.5 KB above
`NoteTube.cpp`'s five pinned `.text` blocks, in `auto_03_82C29D84_text`, at
fuzzy 0. Our `NoteTube.obj` defines it (EXTERNAL, 400 B). A map sweep found
**exactly one** stranded `NoteTube`/`TubePlate` symbol, so the block **is** the
row — priced at 392 B with no hidden upside.

One `.text` line added; the `.pdata` line is dtk's **derived output** (one 8-byte
unwind record — independent corroboration that the block holds exactly one
function).

### Pre-registration vs outcome

| | predicted | measured |
|---|---|---|
| P1 Δ`matched_code` | 0 B (our 400 B vs retail 392 B ⇒ fuzzy < 100) | **+0 B** ✓ |
| P2 Δ`matched_functions` | 0 | **+0** ✓ |
| P3 row fuzzy | 0.00 -> [50, 99.99] (it PAIRS) | **0.00 -> 80.714** (mpn 82.398) ✓ |
| P4 `total_code` | unchanged | **10,247,068 both legs** ✓ |
| P5 unit row counts | NoteTube 19->20; auto 16->15 | NoteTube 19->20 ✓; **auto 16->2** ✗ |

Δfuzzy `+0.003150pp` (50.345950 -> 50.349100) was the **only** whole-binary
movement. Units at 100%: 191 -> 191 (mpn), 171 -> 171 (fuzzy), 0 reached, 0 fell
off. **No falsifier fired** (F1 crossed-to-100, F2 stayed-0, F3 negative,
F4 `total_code` moved — all negative).

★ **P5 was wrong in an instructive way: dtk RE-CARVES an `auto_*` block around a
new pin**, splitting the remainder into a fresh auto unit (16 rows/6,888 B -> 2
rows/3,456 B) rather than dropping one row. `Δtotal_code = 0` proves that is
reattribution, not loss — but a lane that predicted row counts and read 16->2 as
damage would have reverted a clean change.

★ **The stop at 80.7 IS the control.** The brief warns that a re-home alone
reaching 100 should be treated as suspicious (forgiveness, not earned score). It
stopped well short, exactly as the 400-vs-392 body size predicted. The re-home
bought **legibility**: 392 B of real decomp work went from invisible-and-
unattributed to paired and adjudicable. The remaining 8 B is ordinary source work
for whoever takes it.

## 7. What I did NOT do, and why

- **Did not execute the FL hand-off (`0x826ffe38` -> `Sfx.cpp`).** It is not a
  pin move (§5); it needs a coupled map rename, whose economics are dominated by
  un-pairing risk. Doing it as a pin alone would have measured Δ0 and been
  written up as "the vein does not work", which would have been a **false
  negative about the mechanism**.
- **Did not pin `gen_bitlen`.** It is D3DX's zlib, not ours (§3). This was the
  single largest row in the corrected vein and the most tempting thing in the
  lane.
- **Did not attempt the 43-row / 1,188 B `auto_03_82C45EC4_text` block.** ~43
  distant `.text` blocks for 1,188 B, with 25+ distinct destinations (§4).
- **Did not verify all 161 mangled rows are genuine same-function matches.** I
  verified the one I executed. The bare-vs-mangled screen bounds the collision
  class at 1 row but does not prove the remaining 161 are all real; a
  same-spelling-different-function case among C++ mangled names would need a
  different copy of the same *class*, which I did not rule out per-row.
- **Did not run the permuter** (standing directive: off).
- **Did not touch `target_symbol_map.json`** — no map edits in this lane.

## 8. Reusable

`tools/rehome_census.py` (`--selftest`, `--json`). The selftest builds two
synthetic COFFs — one DEFINING a name, one only REFERENCING it — and requires the
honest instrument to separate them **while asserting the vacuous substring test
hits BOTH**, so it demonstrates the defect on a known answer instead of merely
passing.

⚠ Its own first version was **vacuous**: the synthetic symbol record was 16 bytes
(the 2-byte `Type` field omitted), so name lookup was misaligned and two
"agreement with `parse_coff_symbols`" checks passed by comparing **empty set to
empty set**. Non-emptiness assertions were added. That is the third instrument in
this lane whose failure mode was silence.
