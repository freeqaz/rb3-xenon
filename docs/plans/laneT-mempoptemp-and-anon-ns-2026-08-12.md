# MemPopTemp was a real divergence and a wrong diagnosis

2026-08-12, lane T. Follows `fold-thunk-alias-gate-2026-08-12.md` (lane H),
which flagged `??1MemDoTempAllocations@@QAA@XZ` / 44 sites as "the cheapest
actionable item on the board". Both halves of that flag were re-measured here.
The divergence is real. The causal claim attached to it is refuted.

Companion: `laneT-anon-ns-per-symbol-2026-08-12.md` (the second item in this
lane, unrelated to this one).

## What lane H said, and what is true

> Ours branches to `?MemPopTemp@@YAXXZ` (80 B in our obj); retail's branches to
> `fn_827BC2A0` (48 B), which is byte-for-byte the tail of ours. Our
> `MemPopTemp` carries a 32-byte `gNumHeaps` prologue retail's does not.
> **Repair that and the pair becomes admissible on its own evidence.**

The first three sentences are confirmed. The last one is false, and the reason
matters: `tools/fold_thunk_gate.py` never looks at `MemPopTemp`'s length. It
resolves retail's branch destination through `scripts/target_symbol_map.json`
(`Retail.canon`, `self.byva.get(t) or "fn_%08x" % t`) and compares that **name**
to the symbol name in our COFF relocation record. The refusal string is

    relocation target at 0x0: retail fn_827bc2a0 vs ours ?MemPopTemp@@YAXXZ

and the only thing in it that a source edit can move is the right-hand side.
The left-hand side is `fn_827bc2a0` because **the map has no name at
`0x827BC2A0`** — not because the two bodies differ.

Measured after the source divergence was fully repaired (below): the gate
re-runs to byte-identical verdicts, 9 ADMIT / 1,599 sites, 27 REFUSE / 198
sites, and the `??1MemDoTempAllocations` row is unchanged word for word.

## The divergence: retail's push/pop quartet has no `gNumHeaps` guard

Disassembled `0x827BC1F8`–`0x827BC2D0` out of `orig/45410914/band.exe`. Four
contiguous bodies, in our declaration order:

| VA | size | map name | what it does |
|---|--:|---|---|
| `0x827BC1F8` | 72 | `?MemPushHeap@@YAXH@Z` | `s.mStack[s.mSize] = iHeap; s.mSize++` |
| `0x827BC240` | 48 | *(unnamed, `fn_827BC240`)* | `s.mSize--` |
| `0x827BC270` | 48 | `?MemPushTemp@@YAXXZ` | `s.mTempRefs++` |
| `0x827BC2A0` | 48 | *(unnamed, `fn_827BC2A0`)* | `s.mTempRefs--` |

Every one is prologue → `ThreadMemStack(true)` → one load/add/store on `mSize`
(`0x40`) or `mTempRefs` (`0x44`) → epilogue. **None of the four reads `gInitted`
or `gNumHeaps`.** `MemMgr.cpp` wraps all four in `gInitted && gNumHeaps > 0`,
which is eight extra instructions: 32 B on the Temp pair, 48 B on
`MemPushHeap`. It is written in our source, so it is a source divergence and
not the compiler failing to prove something — the caution about widening
linkage does not apply here and nothing about `gNumHeaps`' linkage was touched.

Repaired by adding unguarded retail bodies to `MemHeap.cpp`'s existing
`#ifndef HX_NATIVE` TU-reunification block — the same duplicate-without-linking
pattern already carrying `MemNumHeaps` / `MemHeapSize` / `MemFindAddrHeap`
there. `MemMgr.cpp`'s guarded copies are deliberately left alone: they serve the
native (`HX_NATIVE`) link, where the guard is load-bearing before `MemInit`.

All four now compile byte-identical to retail modulo the `bl` displacement.

### Measurement — pinned `objdiff-cli-B`, both rulers, settled build

| ruler | before | after | Δ | complete fns |
|---|---|---|---|---|
| `none` | 42.220000% (4,357,396 B) | 42.221160% (4,357,516 B) | +120 B | **+2 / −0** |
| `name_check` | 32.462280% (3,350,332 B) | 32.463444% (3,350,452 B) | +120 B | **+2 / −0** |

GAINED `default/MemHeap :: ?MemPushHeap@@YAXH@Z` (72 B) and
`?MemPushTemp@@YAXXZ` (48 B). **Nothing lost at either ruler.** `none` moved,
which is expected and legitimate here: this is a codegen change, not a naming
change.

Only two of the four score. `MemPopHeap` and `MemPopTemp` are now byte-exact
and in the right unit, but `0x827BC240` and `0x827BC2A0` carry no map name, so
there is no target row to pair against. Naming them is worth a further
**+96 B / +2 complete functions** and is left to the map owner.

## What actually blocks the 44 sites: two map defects

### (a) `0x827BC2A0` is unnamed

`0x827BC270` is `?MemPushTemp@@YAXXZ` in the map; `0x827BC2A0` is the
immediately following 48-byte body, identical except `addi r11,r11,-1` for
`+1` on the same field `0x44`. Half of a pair got named. Independent
corroboration already in the tree: `src/system/utl/MemMgr.h` records "retail
fn_827BC270 / fn_827BC2A0: bump the per-thread temp-alloc refcount at +0x44".
Our recompiled `?MemPopTemp@@YAXXZ` is now byte-identical to `0x827BC2A0`
modulo the `bl`. Same argument names `0x827BC240` `?MemPopHeap@@YAXXZ`.

Simulated (map patched in memory, no file written): gate 1 then passes clean —
`identical: 1 word(s), 1 resolved relocation target(s)`.

### (b) `0x82774068` is named `??1MemDoTempAllocations@@QAA@XZ`, and that is wrong

With (a) fixed the pair still refuses, at the **second** gate:

> retail has a DIFFERENT body named `??1MemDoTempAllocations@@QAA@XZ` at
> `0x82774068` (fan-in 1) … the map entry stands and aliasing would hide a
> source defect

That entry does not survive a call-site oracle:

- `0x82774068` is a 4-byte thunk `b ??1FillInfo@@UAA@XZ`. It is the **last word
  of `SongData.cpp`'s split range** `0x82773FB0`–`0x82774070`, whose *first*
  entry is `??1FillInfo@@UAA@XZ` @ `0x82773FB0`.
- Its only caller is `fn_8278C438` (`PhraseList.cpp`), an MSVC PPC **unwind
  funclet** — `addi r31, r12, -0x70` — which does `lwz r3, 0x84(r31)` and then
  `bl 0x82774068`. It destroys an object whose `this` lives at parent-frame
  `+0x84`.
- `~MemDoTempAllocations()` compiles to `b MemPopTemp`, proved: our COMDAT is
  byte-identical to retail's `0x82345030`, and `MemPopTemp` ignores `r3`
  entirely (it loads `r3 = 1` and calls `ThreadMemStack`). It touches no
  object. It cannot be the body that destroys a `FillInfo`.

So `0x82774068` is a fold-naming artifact of exactly the class lane E
described, and `target_symbol_map.json` — which is **our reconstruction**, not
retail's linker map — records the wrong one of the folded names there.

**Not landed.** Both (a) and (b) are `target_symbol_map.json` edits, which this
lane was told to leave to the owner, and (b) additionally repoints an entry
another lane adjudicated. The alias group that would follow
(`??3BandHighlight@@SAXPAX@Z` ≡ `??1MemDoTempAllocations@@QAA@XZ`) is
`scripts/symbol_aliases.json`, owned by another lane this session. Nothing was
written to either file.

## `??3Loader` (22 sites) and `??3Task` (10 sites): still refused

Lane H asked for a call-site semantic oracle. The one-off oracle above was
pointed at both, and it does produce a discriminator — but not the same one,
and not one this lane will act on alone.

| | `??3Loader@@SAXPAX@Z` | `??3Task@@SAXPAX@Z` |
|---|---|---|
| map's other entry | `0x823F4698`, fan-in 1 | `0x822EAB90`, fan-in 4 |
| body | `b fn_82A808D8` | `b _Rb_tree<Symbol,CatData>::clear()` |
| caller shape | unwind funclet, `addi r3, r31, 0x68` | unwind funclets, `lwz r11,0x174(r31); addi r3, r11, 0x780` |

The `takes a this in r3` test that settled `MemDoTempAllocations` does **not**
separate these: `operator delete(void*)` also consumes `r3`. The discriminator
that does is narrower — *what kind of pointer*. `addi r3, r31, 0x68` is the
address of a **stack local**, and `addi r3, r11, 0x780` is an **interior
sub-object** of a heap object. Neither is ever the argument of an
`operator delete`; both are destructor / `clear()` receivers. On that reading
both map entries are fold-naming artifacts too.

Refused anyway, deliberately:

- "Very likely" is still not this gate's bar, and the gate is fail-closed by
  design.
- A pointer-provenance discriminator is a **new tier**, not a one-pair
  judgement. It needs a census over all 27 refusals and a positive control on
  the 9 already-admitted pairs before any of it is load-bearing. That is a
  lane, and it is not this one.
- 32 sites is not worth spending the gate's fail-closed property on.

The oracle sketch and the per-pair evidence are recorded here so that lane
starts with them rather than re-deriving them.

## What is left

- Map: name `0x827BC240` `?MemPopHeap@@YAXXZ` and `0x827BC2A0`
  `?MemPopTemp@@YAXXZ` (+96 B / +2 complete functions, immediately).
- Map: repoint or drop `0x82774068` → `??1MemDoTempAllocations@@QAA@XZ`; with
  that and the above, the 44-site pair admits on its own evidence.
- Alias: `??3BandHighlight@@SAXPAX@Z` ≡ `??1MemDoTempAllocations@@QAA@XZ`,
  after the map moves, in whichever lane owns `symbol_aliases.json`.
- A pointer-provenance discredit tier for `fold_thunk_gate.py`, censused and
  positively controlled, worth 32 sites across `??3Loader` and `??3Task`.
