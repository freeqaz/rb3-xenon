# W16-CD — the `GetOfferIDsToEnumerate` override, and the `StoreOfferProvider` rows W16-CA never started

Lane W16-CD, 2026-09-15. Branch `w16-cd`, base `a0d5e55b91ad`. Continuation of W16-CA
(`W16CA_BANDSTOREPANEL_POLL_HANDLE_UGCPURCHASE_ANONROWS_2026-09-15.md`).

## 0. Headline

**+2,732 B / +16 matched functions on the whole binary, 0 rows lost.** Ruler `name_check`.

| measure | base `a0d5e55b` | tip `03a33bb9` | delta |
|---|---:|---:|---:|
| `matched_functions` | 43,800 | 43,816 | **+16** |
| `matched_code` | 4,082,116 | 4,084,848 | **+2,732 B** |
| `matched_code_percent` | 39.836918 | 39.863580 | +0.026662 |
| `fuzzy_match_percent` | 49.916763 | 49.943066 | +0.026303 |
| `total_code` / `total_functions` | 10,247,068 / 69,240 | unchanged | 0 |
| `masked_equal_functions` | 23,222 | unchanged | 0 |

Units: `BandStorePanel` 43.4571% → **59.4948%** (102/127 → 115/127);
`StoreOfferProvider` 53.6295% → **59.2633%** (83/93 → 86/93).

All seven briefed baseline figures reproduced **exactly** before any work started.

## 1. ⚠ The brief's headline estimate was right in total and wrong in mechanism

The brief priced `GetOfferIDsToEnumerate` at ≈2,016 B and told me to price it myself.
Measured: **2,108 B**. Close — but the composition is the part that matters, and it is
not what the estimate implied:

| component | bytes | kind of work |
|---|---:|---|
| the three **named** `_K` template rows | 472 | source |
| `fn_82608B70` + nine anonymous STL helper rows | 1,636 | **identification (map)** |

**Only 472 B of it is decomp. 1,636 B is a map problem.** A lane that implemented the
function perfectly and stopped would have measured **+472** and reported the estimate as
4× optimistic. This is the same shape as the `StoreOfferProvider` result below, and it
generalises: *when a retail row is anonymous, source quality is invisible to the ruler.*

## 2. `BandStorePanel::GetOfferIDsToEnumerate` (retail `fn_82608B70`)

W16-CA inferred the identity from a callee set and said so. I upgraded it to a proof
before implementing: the `bool` parameter selects `this+0x48` (`mPendingOffers`) vs
`this+0x3c` (`mOffers`); the `__RTDynamicCast` descriptors decode (through the **`.data`**
delta `0x82012400`, not `.text`'s `0x8200B200`) to `.?AVStoreOffer@@` → `.?AVBandStoreOffer@@`;
and five `StorePurchaseable` probes at `+0x0/+0x80/+0x40/+0xe0/+0x120` each read `+0x30`
(`songID`), matching our headers exactly.

Three sub-findings read off retail bytes, each of which changed the source:

* the pushed value is a **prvalue** ⇒ the `SongID()` accessor, not the member;
* the tail is `sort` + `resize(unique(..) - begin())`, **not** `erase(unique(..), end())`
  — retail carries both arms of STLport's `resize` plus its unconditional zero
  default-argument temp;
* `lbz r5, 0x50(r1)` is the empty `equal_to<u64>` functor passed by value.

The last 96.28 → 100 was entirely the **placement of `resize`'s default-argument temp**:
hoisting `std::unique(...)` into its own statement completes it before `resize`'s
arguments are evaluated, which sinks the temp.

### A refuted hypothesis, recorded because it was load-bearing
I first explained that temp placement by claiming retail used STLport's 1-argument
`resize` forwarder under `_STLP_DONT_SUP_DFLT_PARAM`. **Checking the premise refuted it:**
`stl/_config.h:875` gates that define on `_STLP_DEF_CONST_DEF_PARAM_BUG`, which
`config/stl_msvc.h:194` defines only inside `#if defined(_STLP_MSVC) && (_STLP_MSVC < 1200)`
— pre-VC6, inactive here, and retail used the same STLport. The real cause was
full-expression structure. The right answer and the wrong one predicted the same fix;
only the premise check separated them.

### `StoreOffer.h` widened `protected:` → `public:` — and *proved* layout-neutral
Retail reads `mAlbum`/`mPack` from a class that is neither subclass nor friend, so on
retail they were reachable. I did not assert neutrality, I measured it:
`cl /d1reportSingleClassLayoutStoreOffer` before and after, **diff empty, vtable included**
(single access section ⇒ declaration order preserved). `BandStoreOffer::mDemo`/`mUpgrade`
— the exact analogues probed by the same retail loop — were already public.
**This edit paid a second time in §3**: it was one of the two blockers the
`ShowBrowserPurchased` stub named.

### The `erase` ICF fold, settled on bytes
`fn_824B06C8` is mapped as `erase@vector<unsigned int>` but the call site erases 8-byte
elements. Settled on retail bytes rather than by argument: the body passes the byte
difference `finish - last` straight to `memmove` with **no element-size shift**, so
`vector<unsigned int>::erase` and `vector<u64>::erase` compile byte-identically and
folded; `I` is the arbitrary survivor. In the final diff the charge is forgiven anyway.

### Nine anonymous STL rows identified (+1,184 B)
Method: extract each retail body's callee set from the split asm, extract our object's
COFF relocation graph, match structurally, and break the ties callee sets cannot with the
**saved-register boundary** (`__make_heap` r27 vs `sort_heap` r28). Anchored on a
known-correct pair: retail `__introsort_loop` (`0x82608a28`) calls exactly four things
including `0x82608980`, *already* mapped to `__partial_sort<u64*>`, and ours calls
`__median`/`__partial_sort`/`__unguarded_partition`/`__savegprlr_27` — 4-for-4.
**All nine landed at exactly 100.0, which is what confirms the identifications.**

## 3. `StoreOfferProvider` — W16-CA's task 5 (+624 B)

### ★ The in-tree record outranked the brief, and it was already most of the answer
The brief listed `fn_826635D8` (392 B) and `fn_82663328` (180 B) at fuzzy 0 as unexamined
anonymous rows. **They were not unexamined**: lane BV-1 had reconstructed both bodies
instruction by instruction in `StoreOfferProvider.cpp`'s own comment block, and every
particular of that reconstruction verified against the retail bytes — including the
pack-before-album disjunction order and the re-called `OfferType()`. Reading the file the
work was about, before working, was worth more than any analysis I did in this lane.

Two claims in that block were **stale and load-bearing**, both now corrected in-tree:

* *"Neither function is in splits.txt, so neither is scored."* — false. Both are scored
  rows in the unit. They read 0.0 because the **target** symbols are anonymous `fn_`,
  not because the source was absent.
* *"needs … public access to StoreOffer's protected mPack/mAlbum"* — already removed by
  this lane's own §2 edit.

### The measurement that makes the point
| step | `matched_code` Δ | the two rows |
|---|---:|---|
| both bodies implemented, no map entries | **+0 B** | 0.0 / 0.0 |
| + two `target_symbol_map.json` entries | +0 B | **89.27 / 92.56** |
| + source fixes below | **+572 B** | 100.0 / 100.0 |

**Pre-registered and confirmed: correct source bought exactly zero until the map named the
rows.** Naming also carried a real risk here — it converts `Text`'s three *forgiven*
placeholder call sites into *checked* ones, and `Text` is 1,756 B at fuzzy 100. I checked
all four call sites spelled `ShowBrowserPurchased` before building; `Text` held.

### Three source shapes, each adjudicated on retail bytes

**`FindSongOffer` — one line, 92.56 → 100.0.** Retail loads `*it` **once** into a
callee-saved register and reuses it three times; `(*it)->` re-loaded it at all three uses,
which also cost a callee-saved register. objdiff labelled the residue `REGISTER_SWAP` +
`PROLOGUE_MISMATCH` + `REGISTER_SAVE_HELPER_MISMATCH` — two of them **`RarelyHandFixable`** —
and recommended the permuter. A named local `StoreOffer *offer = *it;` dissolved **all 15
charges**. *Fourteenth recorded instance of that label being a symptom, not a diagnosis.*

**`ShowBrowserPurchased` — two independent halves, 89.27 → 95.56 → 100.0.**
* `return a || b;` puts one `li r3,1` *after* both tests; retail puts it **between** them
  and the second test's `bne` jumps **backward** into it ⇒ two standalone `if`s.
* the song arm must **not** carry its own `return false`: with one, MSVC folds the second
  test into the branchless `subic`/`subfe` to-bool idiom and moves the shared `li r3,0` to
  the function tail (which also moved three branch destinations) ⇒ if/**else-if** chain.

⚠ **A pre-registered prediction FAILED here and that failure is what solved it.** I
predicted the else-if alone would close the row; measured **95.561 → 95.551**, i.e.
nothing. Comparing the two attempts showed each had fixed a different end and broken the
other — which identified the halves as independent and named the untried cell of the 2×2.

**`IsActive` — 71.92 → 100.0, and it overturns a recorded verdict.** The comment in the
file classified this row as `BOOL_MASK` / permuter-class and "left at the best-scoring
shape", listing two failed variants. **The plain `return a || b;` was not among them.**
Retail materialises the result in a scratch register and masks it once; the `bool result`
temporary is what made MSVC fuse the returns into `beqlr`. The evidence was in hand rather
than guessed — `ShowBrowserPurchased`'s `return a || b;`, same TU and same build minutes
earlier, emitted exactly retail's four instructions.
⇒ **A prior lane's "tried X and Y, accepted" is a list of what was tried, not a proof of
exhaustion.** Fifteenth instance of the permuter-class-label-as-symptom pattern.

Note the two rows pull in **opposite** directions on the same idiom: retail wants the
masked form in `IsActive` and the branchy form in `ShowBrowserPurchased`'s song arm. The
lever is not "prefer one shape" — it is what sits immediately after the test.

## 4. Closed with evidence — do not re-fund without new information

**`?Handle@StoreOfferProvider@@` (1,556 B, fuzzy 99.8458, `mpn` already 100).**
The brief asked whether its charges are instruction differences or relocation names.
**Neither, exactly**: 12 `diff_arg` in three identical 4-instruction groups, all
`__RTDynamicCast` argument setup. I decoded **both** type descriptors from the retail
binary — `0x82C649D4` = `.?AVObject@Hmx@@`, `0x82C72564` = `.?AVStoreOffer@@` — identical
to ours. Both sides end with `r5 = SrcType = Hmx::Object`, `r6 = TargetType = StoreOffer`;
the `lbl_*` target names are placeholders and forgiven; both `li` values are zero. The
**sole** charged difference is which descriptor the compiler materialised into r11 vs r10
(retail Object→r11, ours StoreOffer→r11) — emission order of two independent address
materialisations, with identical surroundings (indices 0–123 and 128–153 all equal).
That is permuter-class, and the permuter is off by standing directive.
⚠ Note the pricing trap: `mpn` is **already 100**, so this row costs `matched_functions`
nothing — the entire 1,556 B is a `fuzzy`-only prize.

**`?BuildList@StoreOfferProvider@@` (2,536 B, 96.04).** Lane CF-7 records `diff_op: none`
(zero opcode differences); the residue is one regalloc tie-break (retail spills `this` to
its parameter home slot 0x194 and keeps both RTTI descriptors in r14/r15) plus two
stack-slot swaps. Its comment also carries a **decisive negative** on the empty-path test
that should not be retried. I did not re-open it.

**`?PosToNextGroupPos@` (92 B, 93.17) — time-boxed, with a structural finding for the
next lane.** Its 13 charges reduce to **one** real difference: retail **re-loads**
`mElements`' start pointer (`lwz r7, 0x34(r3)`) inside the loop while we hoist it. The
mechanism is visible — retail's `srawi r9, r10, 2` writes the size *over* the start
register, so start cannot survive; ours writes to r8 and keeps it live. Everything else is
the register renaming that follows. I found no source lever for it and stopped rather than
guess; the next lane should start from that sentence, not from the `REGISTER_SWAP` label.

## 5. Handed over, not done

* **`fn_82606280` (908 B, BandStorePanel) — identified but not implemented.** It is the
  **store index-`.dta` parser**. Callees resolve to `FindArray` ×6, `DataNode::Int/Str/Sym`,
  `String::operator=`/`operator+`/`substr`, `Localize`, `DataArray::Release`, and **seven**
  `Symbol` ctors. Its `.rdata` strings read `sorted`, `next_chunk`, `previous_chunk`,
  `index_info`, `offers`, `/`. The rb3-Wii oracle's counterpart logic is
  `BandStorePanel.cpp:215-262` (the `else` arm) plus `StoreMenuProviderGetTitleFromData`
  (`StoreMenuProvider.cpp:18-31`), but **retail is the `.dta` arm and the Wii dev build is
  the packed-`StoreMetadata` arm**, so the oracle is a skeleton, not a body. It sets
  `mNextChunkPath`/`mPrevChunkPath` (our 0xB8/0xC4, compiler-verified).
* **Bonus for W16-CA's NOT-DONE #3 (`GetIndexFile`)**: `0x82c73fdc` read through the
  correct **`.data`** delta yields `0x820bf520` → **`/dlc_top_%s_%s.dta`** — the format
  string W16-CA could not read. `fn_82606280`'s strings sit immediately beside it.
* **Structural side-finding**: `adjacent_find`, `__unguarded_insertion_sort_aux`,
  `__adjust_heap` and `_M_insert_overflow` are map-named but are **not rows in this unit** —
  they sit in the unpinned 728 B splits hole `0x82605D20..0x82605FF8`. Out of scope
  (pinning ≠ decomp), recorded so nobody hunts them as missing source.
* Untouched from W16-CA's list: `sizeof(XboxPurchaser)` (+1,104 B, shared engine header),
  `Handle` idx 94–98 (blocked on `ObjMacros.h`), `StorePanel.cpp:302`'s split-allocation
  temp, and `BandStorePanel`'s six remaining small anonymous rows.

## 6. Predictions, scored honestly

| prediction | outcome |
|---|---|
| source alone buys ~472 B, not ~2,016 B, because the rest of the family is anonymous | ✅ measured +3 fns / +472 B |
| the nine helper identifications land at exactly 100.0 if correct | ✅ 9/9 at 100.0 |
| `FindSongOffer`/`ShowBrowserPurchased` source alone buys **0 B** | ✅ exactly 0 |
| one named local takes `FindSongOffer` to 100 and dissolves the permuter labels | ✅ 92.56 → 100.0, all 15 |
| the else-if alone closes `ShowBrowserPurchased` | ❌ **95.561 → 95.551** — and the failure identified the two independent halves |
| two `if`s inside the else-if chain closes it | ✅ 100.0 |
| a plain `return a \|\| b;` closes `IsActive`, overturning the recorded permuter verdict | ✅ 71.92 → 100.0 |
| naming `fn_826635D8` does not knock `Text` (1,756 B) off 100 | ✅ held |
