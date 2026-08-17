# W21-CARVE — the `map<int,bool>` chain carved; and an anchor for the components no sweep could test

**2026-08-17.** Baseline reproduced **exactly** as briefed, ruler `name_check`:
**44,488 fns / 3,750,264 B / 36.337430%**, honest 21,588, `total_code`
10,320,664, `total_functions` 69,226.

**Shipped: +4 fns / +1,008 B / +4 honest / +0.009765 pp**, `none` control
**+1,008 B**, `masked_equal` unchanged at 22,900, units at 100% unchanged
(255 mpn / 132 fuzzy). Commits `0694600b` (carve), `d3a813ac` (`.pdata`
convergence), `c6af0fed` (Task-2 anchor).

**No `src/**` was touched**, so the native gate was not required and was not run.

---

## Task 1 — the carve

### What the brief got right, and the one thing it undercounted

Everything W17 handed over verified. The builder `0x8233bea0` is 84 B,
`li r3,0x18`, copy shape `[lwz+0, stw+0, lbz+4, stb+4, stw+8, stw+12]`; the
`lbz`/`stb` at +4 is a **one-byte value**, which refutes the map's
`map<int,Symbol>` — and equally `int` and `float` — **on retail bytes, not on a
name**.

⚠ **The chain has FOUR members, not three.** `0x8233c398` (232 B, anonymous,
pinned to CharSignalApplier) is reached only from `0x8233c668` and calls
`_M_insert`. So `0x8233c398`/`0x8233c668` are the NOHINT/HINT `insert_unique`
pair and `_M_insert` has **exactly** the two callers STLport predicts — the same
closure argument W17 used to settle its family C. A three-way carve would have
left a member behind.

The retail call graph, from a whole-`.text` `bl` scan keyed on `symbols.txt`
extents (never the synthetic `.s` address column):

```
fn_8233CB58 (operator[], 160 B, anon)
   -> 0x8233c668  insert_unique[HINT]  488
        -> 0x8233c2c8  _M_insert       204   -> 0x8233bea0 _M_create_node 84  (x3)
        -> 0x8233c398  insert_unique[NOHINT] 232 -> 0x8233c2c8
```

### The identity, proved five ways and reading the map for none of them

| member | ours | retail | |
|---|---|---|---|
| `_M_create_node` | 84 | `0x8233bea0` 84 | ✓ |
| `_M_insert` | 204 | `0x8233c2c8` 204 | ✓ |
| `insert_unique` NOHINT | 232 | `0x8233c398` 232 | ✓ |
| `insert_unique` HINT | 488 | `0x8233c668` 488 | ✓ |
| `operator[]` | 160 | `0x8233cb58` 160 | ✓ |

and **all five bodies are relocation-normalised IDENTICAL to ours** — zero
non-branch word differences, every `bl` at the same offset. `map<int,bool>` is
emitted by exactly two objs, `BandList.obj` and `HamNavList.obj` (W17's claim,
confirmed independently).

### Why `BandList.cpp` and not `HamNavList.cpp`

BandList owns **one contiguous 15,536 B region `0x8233b348–0x82340580`** and all
five addresses lie inside it. HamNavList contributes only **240 B + 68 B** here,
both *before* the chain, and does not span it. Corroborated semantically from
outside the tree family: **four of the six external users** of the chain's entry
point are named `BandList::` reveal/conceal methods.

The three donor pins are islands, which is why they were wrong: **Song.cpp**'s
block sits **5.5 MB** from its real body (`0x827c55e8–0x827c8c88`), and
**Rot.cpp**'s two blocks hold `map<int,float>`/`map<int,int>` STL COMDATs that
`math/Rot.cpp` cannot emit.

### The trade was priced before it was made

All five rows earned **zero bytes** beforehand: `0x8233bea0` sat at fuzzy
**94.2857** against the refuted `map<int,Symbol>` name (and `matched_code` counts
`fuzzy == 100` only), the other four were anonymous at 0. So the carve gave up
nothing — the W15 riskless-re-home situation, established rather than assumed.

### Predicted vs measured

| | predicted | measured | |
|---|---|---|---|
| `_M_insert` | +204 | +204 | ✓ |
| `insert_unique` NOHINT | +232 | +232 | ✓ |
| `insert_unique` HINT | +488 | +488 | ✓ |
| `_M_create_node` | **+0** (allocator name charge) | **+84** | ✗ |
| **graded** | **+924** | **+1,008** | ✗ +84 |
| **`none` control** | **+1,008** | **+1,008** | ✓ exact |
| Δfns | +4 | **+4** | ✓ |
| **caller cascade** | **0** | **0** | ✓ |

★ **The cascade was zero and that is the design, not luck.** Three consecutive
lanes undershot by ignoring cascade; here `unit net (+4, all BandList) ==
whole-binary Δ`, because the fold group's only external entry was deliberately
left forgiven (below).

⛔ **The +84 miss is a NEGATIVE RESULT ABOUT THIS LANE, and it is the most useful
line in this doc.** I predicted `_M_create_node` would be charged because retail's
allocator `0x827bd208` is mapped as the 4-arg `?MemOrPoolAlloc@@YAPAXHPBDH0@Z`
while we call the 1-arg `?MemOrPoolAllocSTL@@YAPAXH@Z`. I "discovered" that
defect from retail bytes — the body reads `r3` only (`cmpwi r3,0` / `cmpwi
r3,0x80` / `li r4,0` / `mr r4,r3`) and *clobbers* `r4` before tail-calling, which
a 4-arg function cannot do — counted its **403 call sites in 402 functions**, and
was ready to ship it as a large multiplier.

**It was already solved.** `scripts/symbol_aliases.json` carries a group at
`0x827bd208` folding `?MemOrPoolAlloc@@YAPAXH@Z` and `?MemOrPoolAllocSTL@@YAPAXH@Z`
onto that survivor, installed by **lane ALIAS-X2 on 2026-08-13 with the identical
r3-only argument**. The alias is why the builder crossed and why my predicted
charge never appeared.

⇒ **Grep `symbol_aliases.json` before treating a relocation-name divergence as a
find.** The standing directive *"read the in-tree record first"* cost this lane a
wrong prediction and would have cost a redundant patch. Corollary for anyone
tempted to repair the name: it is **worth ~0 bytes** (all 403 sites are already
forgiven), and per CLAUDE.md's rule about pruning classes that currently forgive
0 — a prior prune cost **+94,616 B** to reverse — **it should not be touched.**

### REFUSED — `operator[]` at `0x8233cb58` stays anonymous

`map<TrackType,bool>` (PerfectSectionTracker.obj) and `map<int,bool>`
(BandList/HamNavList.obj) compile to the **same five sizes**, and retail has
**one** address per member reached from **both** spellings:
`HandleEnterExtent@PerfectSectionTracker` (`0x826db188`, ~5 MB away) and four
`BandList::` methods all `bl 0x8233cb58`. `TrackType` is an **enum** — distinct
for mangling, identical code for a 4-byte key. **The fold is proven**, so the
survivor's name is arbitrary in the sense `_icf_arbitrary` means.

Naming it is **metric-negative, and the arithmetic is the argument**: three of its
callers sit at **fuzzy 100 today** — `StartRevealAnim` 296 B, `StartConcealAnim`
296 B, `HandleEnterExtent` 620 B = **1,212 B** — precisely *because*
`fn_8233CB58` is a placeholder that `reloc_eq` forgives, and our
PerfectSectionTracker legitimately spells `<TrackType,bool>` (verified in its
relocations). `name_check` conflates *folded* with *wrong*, so naming the address
charges 620 B of **correct** code. And the two `...Poll` rows (99.95 / 99.87)
cannot be helped by it either: a **forgiven** site cannot be their residual
charge.

★ So this is not "we don't know the name". It is **"the name is knowable and
naming it costs real bytes, because the grader cannot represent a fold."** The
honest remedy is a `symbol_aliases.json` entry — which this lane does **not**
make: adding forgiveness lifts the score by construction, and the alias file is
not this lane's to edit. Flagged as a follow-up.

**The four tree members are safe to name despite folding identically**, because
every call site among them is **internal** to the fold group; `0x8233cb58` is the
group's only external entry and it stays forgiven. All four are recorded in
`_icf_arbitrary` so no tool derives tree **identity** from the spelling.

### The denylist entry was lifted, deliberately

W15 denied `0x8233c668` to stop autoid re-inserting `map<int,int>::insert_unique`
by reloc-normalised shape identity and oscillating. **That denial was correct
while the address was UNIDENTIFIED**, and the oscillation hazard was a
*consequence* of it being unnamed and shape-attractive. It is now identified on
the evidence above; an explicit proved name closes the hazard, and keeping the
denial would permanently block the correct name — the opposite of the denylist's
purpose. The rationale was **appended to `_denylist_comment`**, not removed
silently.

---

## Task 2 — an anchor for the components UNTESTED BY CONSTRUCTION

`tools/rbtree_body_anchor.py` (`--selftest` to prove it can fail).

W17's sweep needs an **allocation site** (`li r3,N`), so `_M_find`, `swap`,
`begin`, `_M_lower_bound` and bare `_M_erase` are structurally invisible to it.
The anchor changes the reference instead of the net: compare the retail body
against **our own compiled COMDAT of the same mangled name**, word by word, with
branch displacements masked. No `li r3,N` required. It is the instrument that
settled the chain above five-for-five.

**Re-derived, not inherited: the class is 34 members today, not W17's 35** — the
map moved under W17's own renames.

| | |
|---|---|
| all mapped tree rows | 251 |
| `BODY_IDENTICAL` | 215 |
| `SHAPE_MISMATCH` | 16 |
| `SIZE_MISMATCH` | 9 |
| `NO_OUR_COMDAT` | 9 |
| `NO_EXTENT` | 2 |

**On the `NO_NODE_FN` class (34 members): 30 newly REACHED, 4 still untestable.**
⇒ **the structurally-silent class shrinks 34 → 4.**

**Vacuity control.** Swap a `BODY_IDENTICAL` row's declared name for a different
tree's name of the same member kind; the verdict must move. **156 of 202
sabotages flagged (77.2%) — PASS.** The 22.8% that survive are **ICF twins**, the
instrument's real blind spot, and are reported rather than hidden.

**What a pass does NOT mean**, stated in the tool: `BODY_IDENTICAL` is *shape
consistency*, never identity — this lane proved two spellings compile to identical
bodies. `NO_OUR_COMDAT` means **nothing was tested** and is never folded into the
clean count (the `all([])` trap).

### Found in the previously-silent class

* **`0x826dc4c0` `_M_erase@_Rb_tree<unsigned short, RndFont::CharInfo>` — name
  PROVEN WRONG, replacement NOT proven, deliberately not renamed.** Exactly one
  differing word: retail `li r3,0x28`, ours `li r3,0x24`. That is
  `deallocate(node, sizeof(node))`, so retail's node is **40 B ⇒ a 24-byte
  pair** — but `pair<const u16, CharInfo>` is 20 B, with the 16-byte `CharInfo`
  W17 **confirmed by compiling it**. Existence of the defect is proven; the true
  tree is not, and guessing one is what W15 refused.
* **`0x826f0f78` `clear@_Rb_tree<Symbol,float>`** — retail 32 B vs our 80 B, and
  **immediately adjacent** to `0x826f0f98`, the row W17 refused as *"not a tree
  member at all… needs an RGTutor lane"*. Same neighbourhood; they should be
  opened together.
* **`0x826e0f28` `~_Rb_tree<TrackType,PlayerContribData>`** — retail extent 8, +4.

### ⚠ A negative result about the instrument itself

I expected every **+8** `SIZE_MISMATCH` to be the STLPORT-1 funclet-prefix
artifact in `coff_bodies_ext` — and **tested it instead of assuming**.
`0x8243c3c0` (`_M_find<Edge@RndAmbientOcclusion>`) has **no `except_data` at
`addr+extent`**; retail calls the `__savegprlr` helper where our build **inlines**
the save/restore, and our 8 extra bytes are an inlined `mtlr r12; blr` epilogue,
with registers differing throughout. **Real codegen divergence — not a reader
artifact and not a map defect.**

⇒ **Never infer the funclet artifact from the number 8; test for the
`except_data` per row.** The tool now says so, so the next lane does not
"correct" a real finding into an artifact by pattern-matching.

---

## Deliberately not done

* **`operator[]` `0x8233cb58` not named and not moved** — refused above, with
  arithmetic.
* **No alias added or withdrawn**, including the one that would legitimise
  `0x8233cb58`. Adding forgiveness lifts the score by construction.
* **`0x827bd208` `MemOrPoolAlloc` NOT renamed** — already alias-handled, worth
  ~0 bytes, and CLAUDE.md warns against pruning classes that forgive 0 today.
* **The three new defect candidates were adjudicated but not shipped** — each
  needs a proved replacement name, and the pinned unit's obj must be shown to
  define it. Proving a name wrong does not make renaming it safe.
* **4 members remain untestable** (`NO_OUR_COMDAT`: our build never instantiates
  that name). No instrument here reaches them, and inventing one would have been
  unmeasured.
* **No `src/**` touched ⇒ native gate not required and not run.**
