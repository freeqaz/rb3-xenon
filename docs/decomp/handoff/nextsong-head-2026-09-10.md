# Handoff — `NextSongPanel::CountOrCreateExpandedDetails` head residual (lane L3-NEXTSONG, 2026-09-10)

**Row:** `?CountOrCreateExpandedDetails@NextSongPanel@@QAAHHAAVDataArrayPtr@@_N@Z`,
unit `default/NextSongPanel`, source `src/band3/meta_band/NextSongPanel.cpp`.
**Prize if it crosses:** +12,220 B / +0 functions (`mpn` is already 100; the byte
gate is `fuzzy == 100`).

| state | fuzzy | charged sites | base size |
|---|---|---|---|
| main `dd4dd0ae` | 99.993454 | 2 (idx 1284, 1630) | 12,220 |
| **landed here (`.`→`->` at one site)** | **99.99673** | **1 (idx 1284)** | 12,220 |

The row did **not** cross. Verdict: the one remaining charge is a single integer
`add` operand-order swap at the `songGoals` loop site, and it resisted every
codegen-invisible source handle I could find (5 builds). It is **not** an FP
reassociation, **not** a reloc-name charge, and **not** a liveness swap — the
same two registers appear on both sides.

## The charged instructions (graded ruler, `functionRelocDiffs=name_check`)

Both are the node-address computation inside the inlined
`DataArray::Node(int)` for `ptr.Node(count++) = DataArrayPtr(...)`:

```
r27 = &ptr                 (DataArrayPtr &)
lwz  r11, 0x0(r27)         ; ptr.mData
lwz  r11, 0x0(r11)         ; mData->mNodes
add  r3, <mNodes>, <off>   ; &mNodes[count]   <-- the charged instruction
```

where `r28` is the loop-strength-reduced `count*8` (`addi r28, r28, 0x8` after
every `count++`) and `r29` is `count`.

### idx 1284 — goals loop (`for it in songGoals: ptr.Node(count++) = DataArrayPtr(label, cur)`) — STILL OPEN

```
target                                   base (ours)
1262 lwz   r11, 0x0(r26)                 lwz   r11, 0x0(r26)        ; cur = *it (hoisted)
1263 cmplwi cr6, r20, 0x0                cmplwi cr6, r20, 0x0       ; if (b)
1264 beq   cr6, else                     beq   cr6, else
1265 addi  r29, r29, 0x1                 addi  r29, r29, 0x1        ; count++ (b path)
1266 addi  r28, r28, 0x8                 addi  r28, r28, 0x8
1267 b     latch                         b     latch
1268 stw   r11, 0x2e8(r31)               stw   r11, 0x2e8(r31)      ; DataNode(cur)
1269 stw   r30, 0x2ec(r31)               stw   r30, 0x2ec(r31)
1270 lwz   r11, 0x0(r25)                 lwz   r11, 0x0(r25)        ; label
1271 stw   r30, 0x1fc(r31)               stw   r30, 0x1fc(r31)
1272 stw   r11, 0x1f8(r31)               stw   r11, 0x1f8(r31)
1273 addi  r5, r31, 0x2e8                addi  r5, r31, 0x2e8
1274 addi  r4, r31, 0x1f8                addi  r4, r31, 0x1f8
1275 addi  r3, r31, 0x4b0                addi  r3, r31, 0x4b0
1276 bl    ??0DataArrayPtr@@QAA@ABVDataNode@@0@Z   (same)
1277 mr    r4, r3                        mr    r4, r3
1278 addi  r3, r31, 0x518                addi  r3, r31, 0x518
1279 bl    ??0DataNode@@QAA@ABVDataArrayPtr@@@Z    (same)
1280 lwz   r11, 0x0(r27)                 lwz   r11, 0x0(r27)
1281 addi  r4, r31, 0x518                addi  r4, r31, 0x518
1282 addi  r29, r29, 0x1                 addi  r29, r29, 0x1
1283 lwz   r11, 0x0(r11)                 lwz   r11, 0x0(r11)
1284 add   r3, r11, r28        <<<       add   r3, r28, r11         ; ONLY DIFFERENCE
1285 addi  r28, r28, 0x8                 addi  r28, r28, 0x8
1286 bl    ??4DataNode@@QAAAAV0@ABV0@@Z            (same)
```

### idx 1630 — section loop, `unk4 < 0` branch, `left_label` site — CLOSED by `ptr->Node(count++)`

Identical shape (3-arg `DataArrayPtr` ctor, temps at 0xc0/0x98/0xd0, DataNode
at 0x588). Retail `add r3, r11, r28`; ours was `add r3, r28, r11`; with
`ptr->Node(count++)` (i.e. `DataArrayPtr::operator->` + `DataArray::Node`
instead of `DataArrayPtr::Node`) it is now `add r3, r11, r28` — equal.

## Census that frames the problem

The function has **50** node-address `add`s:

| form | count | where |
|---|---|---|
| `add r3, r10, r11` (fresh `slwi r10, r29, 3`) | 45 | every site outside a loop — all equal |
| `add r3, r28, r11` (IV first) | retail 3 / ours (main) 5 | loop sites |
| `add r3, r11, r28` (mNodes first) | retail 2 / ours (main) 0 | idx 1284, 1630 |

So the swap only exists where the compiler strength-reduced `count*8` into a
loop IV. The five IV sites are: 1284 (goals loop, its only site) and
1630/1674/1720/1783 (section loop: `<0` left, `<0` right, `else` left,
`else` right).

## Variant table (each = one full `ninja-locked` build, read from report.json)

| # | change | fuzzy | base size | sites charged | note |
|---|---|---|---|---|---|
| 0 | main | 99.993454 | 12,220 | 1284, 1630 | baseline |
| V1 | split increment at 1284 & 1630: `ptr.Node(count) = X; count++;` | **99.519806** | 12,196 | many | compiler MERGED the two `count++` paths (`beq`→`bne`, −24 B). Confirms retail's source keeps separate increments in `if (b) count++; else ptr.Node(count++) = …`. Add order at both sites UNCHANGED |
| V2 | `ptr->Node(count++)` at 1284 & 1630 | **99.99673** | 12,220 | 1284 | **1630 FLIPPED to retail**; 1284 unmoved |
| V3 | `->` at all 5 loop sites | 99.993454 | 12,220 | 1284, **1783** | 1783 flipped the WRONG way; 1674/1720 unmoved ⇒ `->` is a per-site toggle, not a rule |
| V4 | V2 section loop + `((DataArray *)ptr)->Node(count++)` at 1284 (conversion-operator chain) | 99.99673 | 12,220 | 1284 | inert at 1284 |
| V5 | `->` at 1630 only, plain `.` at 1284, goals loop `++it` → `it++` | 99.99673 | 12,220 | 1284 | inert |
| landed | `->` at 1630 only (everything else as main) | 99.99673 | 12,220 | 1284 | = V5 without `it++` |

Not tried, and why:
- Syntactic operand swap inside `Node()`: nothing to swap (both operands come
  from the inlined accessor), and DQ-3 / `fixable-operators.md` measured the
  class inert.
- Declaration reorder: measured inert for register-only swaps
  (`MSVC_X360_REGALLOC.md` correction); and this is not even a register
  choice, it is operand order with identical registers.
- `#pragma opt_usedef_mem_limit 300` above the function: it is a **Metrowerks**
  pragma copied verbatim from the rb3-Wii oracle by the port commit
  `15b0f378`; MSVC ignores it. It is not a lever and can be deleted at leisure.
- Permuter: deferred by user directive.

## Diagnosis

- The 45 non-loop sites and 3 of the 5 loop sites show MSVC's canonical order
  for `mNodes[i]`: **index first, base second**. Retail's two `mNodes`-first
  sites are therefore *deviations*, produced by something about the tuple /
  temporary creation order at those specific sites when the IV rewrite
  substitutes `count<<3` → `r28`.
- Evidence that the order is decided by front-end temporary ordering rather
  than by liveness: V2/V3 change nothing about liveness (identical bytes
  otherwise) yet flip the order at 1630 and 1783 by changing only the *inline
  chain* through which `i` reaches `mNodes[i]` (`DataArrayPtr::Node(i)` adds a
  parameter-copy level that `operator->` + `DataArray::Node(i)` lacks).
- At 1284 the same lever is inert through three different chains, and so are
  the two IV-neighbourhood perturbations (increment placement, `it++`). The
  goals loop is the only one of the two with a second, pointer-typed IV (`it`,
  r26, latch reloads `songGoals.end()`), so the ordering there is presumably
  pinned by the `it` IV's tuples rather than by anything at the site.

## What I would try next (in order, one build each)

1. `const Symbol &cur = *it;` vs `Symbol cur = *it;` — the copy is a temp in the
   same block as the IV substitution; a reference removes it. Risk: the hoisted
   `lwz r11,0(r26)` at 1262 moves into the else block (worse), but a flip at
   1284 with an otherwise-equal body would settle it.
2. `DataArrayPtr(label, *it)` with no local — same idea, more aggressive.
3. Spell the goals loop with an explicit end iterator declared *inside* the
   `for` init (`it = begin(), end = end()`) — changes the `end()` reload
   (probably worse) but re-orders the pointer IV's tuples.
4. `unsigned`/`size_t` for `count` — signedness affects whether/how the IV is
   created; the 50 `slwi`/`addi` sites are signedness-blind so it may be
   codegen-invisible except for this order.
5. Only if 1–4 are inert: accept `AT_LIMIT` on a 12,220 B row with **one**
   integer-commute charge, and record it in the crossing worklist's
   PURE ARITH_COMMUTE bucket with this table attached.

## EH funclets

The DEFER doc's "230 × 40 B funclets keyed to this parent" — the unit has 350
rows / 333 matched; the funclets already match (frame is 0x880 in both). This
lane changed no funclet state: unit `matched_functions` 333/350 and
`matched_code` 16,088/33,456 B are identical before and after (see the
ab_measure block below).

## Measured

`tools/ab_measure.py --worktree ~/tmp/wt-l3nextsong --from-dirty`, run dir
`.ab_measure_runs/20260910-233959-from-dirty-3426115`, patch `b98907c69123e19e`,
objdiff-cli `a5c35b15d7d46ac4`, both legs settled:

```
leg A: matched=42305 masked=22915 honest=19390 code%=36.843063  (recompiles: 0)
leg B: matched=42305 masked=22915 honest=19390 code%=36.843063  (recompiles: 1, patch_steps=6)
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0
Δfuzzy=+0.000000pp   (legA 48.946476 -> legB 48.946476)
units at 100% [mpn]: 149 -> 149 ; [all-rows-fuzzy]: 121 -> 121
```

Pre-registered: Δ0 on every counted key (row stays under `fuzzy == 100`), aggregate
fuzzy ≈ +0.000004 pp — measured +0.000000 at the tool's precision. Row-level, from
report.json on the same tree: fuzzy 99.993454 → **99.99673**, mpn 100 → 100,
size 12,220 → 12,220.

