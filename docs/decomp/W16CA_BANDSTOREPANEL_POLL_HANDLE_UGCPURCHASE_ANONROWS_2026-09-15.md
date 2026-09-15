# W16-CA — BandStorePanel Poll/Handle, UGCPurchasePanel Poll, and the anonymous rows

**Lane:** W16-CA · **Branch:** `w16-ca` (off main `3185bd289d66`) · **Date:** 2026-09-15
**Worktree:** `~/tmp/wt-w16-ca` · **Ruler:** `functionRelocDiffs=name_check` (the shipped default)

---

## 0. Headline

| measure | before | after | Δ |
|---|---:|---:|---:|
| `matched_functions` | 43,681 | **43,703** | **+22** |
| `matched_code` | 4,068,364 | **4,069,204** | **+840 B** |
| `matched_code_percent` | 39.702713 | **39.710910** | +0.008197 pp |
| `total_functions` | 69,240 | 69,240 | 0 |
| `total_code` | 10,247,068 | 10,247,068 | 0 |
| `fuzzy_match_percent` | 49.854412 | 49.863712 | +0.009300 |

Rows crossed in: **19 / 840 B**. Rows fallen out: **0**.

Per-row movement:

| row | size | before | after |
|---|---:|---:|---:|
| `?Poll@BandStorePanel@@UAAXXZ` | 980 | 45.0449 | **90.2204** |
| `?Handle@BandStorePanel@@…` | 1,928 | 93.1224 | **98.0083** |
| `?SyncProperty@BandStorePanel@@…` | 120 | 68.3333 | **100.0** ✅ |
| `?Poll@UGCPurchasePanel@@UAAXXZ` | 1,104 | 87.7 | **99.99638** |
| `??0MetadataLoadedMsg@@QAA@PAVDataArray@@_NPBD11@Z` | 264 | — (unpaired) | **94.8485** |
| `?Poll@SetlistToStorePanel@@UAAXXZ` | 1,196 | 100.0 | **100.0** (dipped to 99.967 mid-lane, recovered) |

⚠ Note the shape: **four of the six rows above improved by 5–55 pp and collected
zero bytes**, because `matched_code` keys on `fuzzy == 100` and is all-or-nothing
per row. The +840 B is `SyncProperty` (120 B) plus **18 forty-byte EH funclets**
that crossed as a group once their parents' frame sizes and saved-register ranges
matched. Price this lane's value by the diagnoses, not by the byte counter.

### Baseline verification

All seven briefed figures reproduced **exactly** from
`build/45410914/report.json` before any edit:
`matched_functions 43681 · matched_code 4068364 · total_code 10247068 ·
total_functions 69240 · matched_code_percent 39.702713 ·
fuzzy_match_percent 49.854412 · masked_equal_functions 23111`,
provenance `objdiff 4.2.9, binary hash 5a51cd51fe0a353f, tool_commit a5f0ea903ec1,
functionRelocDiffs=name_check`.

### Two briefed rows were STALE and are corrected

- `?MakeNewOffer@BandStorePanel@@…` — briefed at census 60.92. **Measured 100.000.**
- `?IsLoaded@BandStorePanel@@…` — briefed as a 100 B target. **Absent from the
  sub-100 list entirely; already 100%.**

Both carried a "re-verify" flag in the brief, and both needed it. No work was
done on either.

---

## 1. Task 1 — the 0x50 phantom frame on `Poll@BandStorePanel`

**Diagnosis: ten `DataNode` temporaries, not a `MakeString` buffer.**

Retail's frame is `0xf0`; ours was `0x140`. The 0x50 difference is *exactly*
two message-construction sites × five constructor arguments × `sizeof(DataNode)` (8).

Instruction-level evidence chain, in the order it was actually followed:

1. `stack-layout` reported **19 BASE_ONLY slots**, of which eight form
   `(addr sz=4, int sz=4)` pairs — the footprint of a `DataNode` temporary
   (`union mValue` @0x0 + `DataType mType` @0x4).
2. The `replace` rows showed retail emitting `li <immediate>` where we emitted
   `addi rN, r31, <offset>` — i.e. retail passes **values**, we pass **addresses
   of stack temporaries**.
3. Retail's site is a single six-register `bl fn_82606020`.
4. `fn_82606020`'s body (264 B) builds five `DataNode`s **on its own frame**,
   obtains a `Symbol` by sret from `fn_826050A8`, calls `fn_822B1918` (the 5-arg
   `Message` ctor), and then stores **its own vftable** (`lbl_820BF538`) into
   `0(r30)` — so it is a `Message` **subclass constructor**, not a helper.
5. `fn_826050A8` is a function-local-static `Symbol` over `lbl_820BF1E8`, and
   `build/45410914/asm/auto_00_82000400_rdata.s:268298` gives
   `lbl_820BF1E8: .string "metadata_loaded"`.

So retail spells both sites as a typed `MetadataLoadedMsg` whose out-of-line ctor
hosts the temporaries. Declaring that ctor and defining it in this TU moved Poll
**45.0449 → 85.0735** and the frame to **0xf0 exactly**.

⚠ **A refuted first hypothesis, recorded because it is the obvious one.** I first
supposed retail's `Message` constructor took raw values in registers. Reading
`src/system/obj/Msg.h` in full **refuted** it: every `Message` ctor takes
`const DataNode &`; there is no raw-value overload. The correct conclusion is that
the *subclass* ctor takes scalars and wraps them itself.

⚠ **The scalars are `bool`, not `int`.** `fn_82606020` applies `clrlwi rN, rN, 24`
to r5/r7/r8 before storing into the `DataNode` integer — the bool→int widening,
which is not emitted for an `int` parameter. `SetlistToStorePanel.cpp` had reached
the same *call shape* from the caller side and spelled them `int`; a caller
structurally cannot tell the two apart, only the callee body can.

**`DataArrayPtr`** — objdiff demangled retail's callee at idx 175 as
`??0DataArrayPtr@@QAA@XZ`, so retail's source is `DataArrayPtr empty;`, not
`DataArray *empty = new DataArray(0)`. The explicit `empty->Release()` goes with
it (`~DataArrayPtr` supplies it inline). Removed a `diff_op` and an 11-instruction
cluster. **85.0735 → 90.2204.**

### NEGATIVE RESULT — `String path(mLastRequest)` is REAL

The brief suggested this local might be spurious. It is not: block 1 carries
`addi r3,r31,0xa0; bl fn_827BE608` (String copy-ctor into slot 0xa0) and a
matching `bl fn_827BDF38` (dtor) at the tail. Removing it would have regressed.

### NOT DONE — `GetIndexFile()`, and why

Predicted and stated in advance as the named risk: spelling
`mLastRequest == GetIndexFile()` would close the two
`lis r11, ?TheStoreMetadata@@…@h` sites at idx 77/189. **It did not.** MSVC
inlined our one-line `GetIndexFile` straight back into `Poll`.

That failure is the informative part: retail's compiler, same flags, did **not**
inline its `GetIndexFile`, so retail's body cannot be a one-line `MakeString`
wrapper. Read off retail bytes (`orig/45410914/band.exe`, `.text` file offset =
VA − 0x8200B200), `fn_82605C38` is 68 B and decodes to:

```
mflr r12 / stw r12,-8(r1) / stwu r1,-0x60(r1)
li   r4,2       ; addi r3,r1,0x54 ; bl fn_8250FDF8        -> sret Symbol @0x54
addi r3,r1,0x50 ; bl ?SystemLocale@@YA?AVSymbol@@XZ       -> sret Symbol @0x50
lis  r11,0x82c7 ; lwz r5,0x50(r1) ; lwz r4,0x54(r1) ; lwz r3,0x3fdc(r11)
bl   ??$MakeString@PBDPBD@@YAPBDPBD00@Z                   ; blr
```

i.e. **`MakeString(*(const char**)0x82c73fdc, F(2), SystemLocale())`** — a
three-argument, locale-dependent format whose format string is itself read from a
global pointer. At 68 B it is too large for the inliner, which is precisely why
retail keeps the call and we do not.

**Stopped deliberately.** Closing it requires identifying `fn_8250FDF8` (itself a
function-local-static holder — guard word `0x82cc9a00` bit 0x1, object
`0x82cc99e8` — taking `(sret Symbol*, int)`) and the `.data` global at
`0x82c73fdc`. Both live in `src/system/os/Debug.cpp`, engine territory outside
this lane. ⚠ Also note `0x82c73fdc` is in `.data`, which has a **different
file-offset delta** from `.text`; reading it with the `.text` formula yields
`0x57e7fff1`, which is garbage, not a pointer.

**What would overturn this:** identify `fn_8250FDF8` and dereference
`0x82c73fdc` through the correct `.data` section delta. Worth the two idx 77/189
replaces plus Poll's clusters 4/5.

### Byte geometry of the 112-byte splits hole

`config/45410914/splits.txt` brackets `0x82605C38..0x82605CA8` **exactly**
(`… end:0x82605C38` then `.text start:0x82605CA8`), so it is a deliberate
excision, not a dtk mis-carve. It is fully accounted for:

| | |
|---|---|
| `fn_82605C38` | 68 B, ends `blr` @0x82605C78 — `GetIndexFile()` (above) |
| padding | 4 B of `00` |
| `fn_82605C80` | 40 B — reads `this->0xe4` (`mShortcutProvider`), indexes an 8-byte-stride array, tail-branches to `?Str@DataNode@@…` ⇒ `?ShortcutTextAtData@BandStorePanel@@QAAPBDH@Z` |
| **total** | **112 B — exact** |

---

## 2. Task 2 — `Handle` and `SyncProperty`

### `SyncProperty` 68.3333 → **100.0** (crossed, +120 B)

`SYNC_PROP(waiting, mUserCanDoInput)` does not exist on retail. Its nine
instructions are **all ours-only** — `lis`/`lwz` of `?waiting@@3VSymbol@@A@h`/`@l`,
the `cmplw` against the incoming Symbol, the `PropSync` call on `this+0xe1`, the
`bne` and the `b` — with no counterpart in retail's 30-instruction body. Retail's
`SyncProperty` is the bare `SYNC_SUPERCLASS` chain. Removing it also re-aligned
the six register `diff_arg`s around it.

### `Handle` 93.1224 → **98.0083** (1,928 B; did not cross)

Three defects.

**(a) `HANDLE_EXPR(sort_name, …)` takes the member, not the accessor.** Retail
does `lwz r11, -0x1c(r26)` == `this+0xd0` == `mSort` and builds the DataNode with
`li r10, 0x5` (`kDataSymbol`) — no call at all. Ours called
`?SortName@BandStorePanel@@QAA?AVSymbol@@XZ`, which returns `Symbol` by value and
so cannot be inlined away.

**(b) `user_can_do_input` — the Wii clause was PORTED, not dropped.** The comment
that stood in our source read:

> *"rb3-Wii's `user_can_do_input` tail checked `TheWiiCommerceMgr` async op state;
> there is no CommerceMgr on 360 (Xbox uses XboxEnumeration), so the Wii-only
> commerce clause is dropped."*

Retail refutes it. The clause survives in translated form, and it was our **extra
leading `mUserCanDoInput == 0`** that retail lacks. Retail's four terms, idx
322–342:

```
vcall slot 0x30                        -> IsLoaded()
bl fn_827B4CC0 ; clrlwi. ; bne         -> !IsEnumerating()
bl fn_827B4D10 ; clrlwi. ; bne         -> !InCheckout()
lwz -0x40(r26) ; lbz 0(r11) ; cmplwi 0 -> mLastRequest.empty()
```

Cluster 7 (idx 330–337, eight target-only instructions) was exactly the two
missing calls.

⚠⚠ **METHOD ERROR WORTH PROPAGATING.** I first identified the two callees from
the **vtable slot** they dispatch — `fn_827B4CC0` vcalls slot 2, and the
`XboxPurchaser@StorePurchaser@` vtable dump names slot 2 `IsPurchasing`, so I
concluded `fn_827B4CC0 == InCheckout()`. **That was backwards.** The **header
offsets** corrected it: `StorePanel.h` has `XboxEnumeration *mEnum` at `0x70` and
`StorePurchaser *mPurchaser` at `0x78`, so `fn_827B4CC0` (reads `0x70`,
null-checks, vcalls slot 2 of *XboxEnumeration's* vtable) is `IsEnumerating()`,
and `fn_827B4D10` (`return this->0x78 != 0`) is `InCheckout()`.
**A slot name borrowed from a different class's vtable is not an identification.**

**(c) Implicit vs explicit `String` conversion** — overturns a deliberate
NOT-DONE by lane BODYPORT-3, whose note read:

> *"NOT attempted here: the obvious lever (construct the String as a named local,
> which would give MSVC a fixed addressable slot) does not fit inside
> HANDLE_ACTION's single-expression form, and inventing a different macro for the
> arm would be metric-fitting rather than reconstructing retail."*

Its diagnosis of the shape was exactly right and is what made this cheap. What it
missed is that a named local is not the only way to get MSVC to rematerialise
`&temp` from its stack slot — an **implicit conversion temporary** does it too,
and `String(const char *)` is not `explicit`:

```
Request(String(_msg->Str(2)), _msg->Int(3))   // explicit functional cast
Request(_msg->Str(2), _msg->Int(3))           // implicit conversion  <- retail
```

MSVC threads the ctor's returned `this` for the explicit form (`mr r30,r3` then
`mr r4,r30`, spilling across the intervening call) and re-forms
`addi r4,r31,0x58` for the implicit one. Three arms changed. **Predicted ~99,
measured 98.0083** — right direction, wrong magnitude; two of the four replaces I
expected to resolve did not.

### `Handle` residual — 10 sites of 486, and why they stay

- **idx 397–407 (six sites): pure instruction SCHEDULING.** The same three
  address computations (`addi r5,r31,0x58`, `subi r4,r26,0xec`,
  `addi r3,r31,0x68`) which retail emits immediately before
  `bl OnMsg(LocalUserLeftMsg)` and our compiler hoists above the intervening
  `stw`/`sth`. Permuter territory; the permuter is **off by standing directive**.
- **idx 94–98 (four sites): a real temporary-lifetime difference, BLOCKED BY AN
  ENGINE HEADER.**
  ```
  retail: … bl Request ; addi r3,r31,0x58 ; bl ~String ; li r11,1 ; stb
  ours:   … bl Request ; li r11,1 ; stb ; addi r3,r31,0x58 ; bl ~String
  ```
  Retail destroys the `String` temporary **before** `mStartBrowserAtBottom = true`,
  so retail's arm is **two statements**; a comma operator keeps both operands in a
  single full-expression, so the temporary must outlive the assignment. Our
  `src/system/obj/ObjMacros.h` spells the macro body `(action);`, and `(a; b)` does
  not compile.
  **What would overturn it:** change `ObjMacros.h` to `action;` — which cascades to
  every `HANDLE_ACTION` in the tree, and that header is out of this lane's scope.

**Side effect recorded, not acted on:** `mUserCanDoInput` (0xE1) is now referenced
by nothing in this TU — no ctor store (already established), no `SYNC_PROP`, no
handler. It may not exist on retail. The member is **left declared** because
removing it is layout-neutral either way (`mStartBrowserAtBottom` 0xE0 /
`mShortcutProvider` 0xE4 — the three bytes are padding with or without it), so
there is no evidence to decide it.

---

## 3. Task 3 — `Poll@UGCPurchasePanel` 87.7 → **99.99638**

### The "unidentified global singleton" is `TheNet`

Our own source carried:

> *"TODO(unresolved): retail guards this block on a global singleton (Ghidra
> `DAT_82cbfaec`) … Singleton's class could not be identified within budget, so
> this defaults to flags=0"*

objdiff names retail's two relocations at that site outright —
`lis r11, ?TheNet@@3VNet@@A@h` / `addi r11, r11, ?TheNet@@3VNet@@A@l` — and
`DAT_82cbfaec` is simply `TheNet + 0x34` (0x82cbfaec − 0x34 = 0x82cbfab8 =
`&TheNet`). `Net + 0x34` is `Server *mServer`, **verified by the compiler**
(`cl /d1reportSingleClassLayoutNet`), not by the header's `// 0xHEX` comments.

The two vcalls are slot 5 (`lwz r11,0x14(vptr)`) and slot 7 (`lwz r11,0x1c`) of
the primary `Server@Server@` vtable, which
`cl /d1reportSingleClassLayoutServer` prints as `[ 5] Server::IsConnected` and
`[ 7] Server::GetPlayerID`. Corroborated three ways before writing it:

- the slot arithmetic agrees with the retail attestation **already in `Server.h`**
  from lane W16-G (`GetPersistentStoreClient` 0x34 = 13, `GetCompetitionClient`
  0x38 = 14) — walking back lands on 5 and 7;
- retail tests slot 5's result with `clrlwi. r11,r3,24`, i.e. a bool, and
  `IsConnected` is the only bool-returning slot in that range;
- slot 7 is handed `mUser->GetPadNum()` and its result becomes the ctor's trailing
  `flags` argument, which is what `GetPlayerID(int)` is for.

So retail's case-3 prefix is:

```cpp
Server *server = TheNet.GetServer();
if (server && server->IsConnected())
    flags = server->GetPlayerID(mUser->GetPadNum());
```

Closed the whole 30-instruction target-only cluster at idx 179–209.
**87.7 → 99.239 — better than the "low/mid-90s" I pre-registered**, because the
missing code had also been displacing the register allocation around it, which
re-aligned for free once the body was present.

### One allocation temporary, not two — 99.239 → 99.99638

We had `void *mem = operator new(sizeof(XboxPurchaser)); StorePurchaser *purchaser;
if (mem) purchaser = new (mem) …`, which stores the pointer to **two** stack slots
(0x5c and 0x60); retail stores it once (`mr. r25,r3; stw r25,0x5c(r31)`). That
surplus 4-byte slot pushed every later local by +8 and rounded the frame
0xc0 → 0xd0 — **17 of the 35 charged sites**, plus most of the r25/r26/r27
renumbering. Plain `mPurchaser = new XboxPurchaser(...)` is retail's spelling.

⚠ The split-temp idiom came from `src/system/meta/StorePanel.cpp:302`, which still
has it and is out of this lane's scope — **likely the same defect there, unmeasured.**

### NOT DONE — a 1,104 B row behind ONE struct size

The row's **single remaining charged site** is:

```
idx 210  TGT `li r3, 0x28`   vs   SRC `li r3, 0x50`
```

`sizeof(XboxPurchaser)` is **40 bytes on retail and 80 in our headers**. The cause
is established, not guessed: the `XboxPurchaser` constructor `fn_827B2800` stores
**exactly one vptr**, at offset 0x0 —

```
stw r11(=lbl_82115258), 0x0(r3)   ; the ONE vtable
stw r8,  0x4(r3)   ; Symbol (arg5)      stw r9,  0x8(r3)   ; unsigned int (arg6)
stw r10, 0xc(r3)   ; = 0 (mState)       std r5,  0x10(r3)  ; unsigned long long (arg2)
stw r4,  0x18(r3)  ; int (arg1)
```

— so retail's `XboxPurchaser` **does not inherit `Hmx::Object` at all**. Our
`cl /d1reportSingleClassLayoutXboxPurchaser` shows the `Object` base contributing
exactly the 40 bytes of overshoot (0xc–0x34), and 0x50 − 0x28 = 40. The
single-inheritance arithmetic reproduces retail exactly: `StorePurchaser`(12) +
`mState`(4) + pad(4) + `mOfferID`(8) + `mUserIndex`(4) + pad = **0x28**.

**Why not done:** `src/system/meta/StorePurchaser.h` is an **engine header**,
explicitly out of this lane's scope, and the change is not cosmetic — it drops a
base class and re-orders fields, cascading into `StorePanel.cpp` (also fenced),
`StorePurchaser.cpp`, and `XboxMultipleItemsPurchaser`.

**What would overturn it:** a lane that owns `src/system/meta/StorePurchaser.h`.
The prize is a clean **+1,104 B / +1 function** on this row alone, since it is the
only remaining charge, plus whatever it unlocks in `StorePurchaser.cpp` and
`StorePanel.cpp`.

⚠ Independent smell found in the same layout dump, not investigated: `mUserIndex`
is declared **twice** — `StorePurchaser::mUserIndex` at 0x8 and
`XboxPurchaser::mUserIndex` at 0x48.

---

## 4. Task 4 — naming `fn_82606020`, and the anonymous-row census

### 4.1 `fn_82606020` is `MetadataLoadedMsg`'s ctor; `SetlistMetadataLoadedMsg` does not exist

`SetlistToStorePanel.cpp` declared a local decl-only
`class SetlistMetadataLoadedMsg : public Message`, and — more consequentially —
`scripts/target_symbol_map.json` asserted the same class at `0x82e01810` via
`?msg@?BO@??Poll@SetlistToStorePanel@@UAAXXZ@4V**SetlistMetadataLoadedMsg**@@A`.
A mangled function-local-static name **encodes its type**, so that map entry was a
claim that such a class exists. RB3 ships no PDB; the name was inferred, and it is
wrong. Three independent lines:

1. `fn_82606020` is **one** function, and `BandStorePanel::Poll` (two sites) and
   `SetlistToStorePanel::Poll` all `bl` it. One function cannot be two classes'
   constructors.
2. It calls `?Type@MetadataLoadedMsg@@SA?AVSymbol@@XZ` over `"metadata_loaded"`.
   A second message class needs its own `Type()` over a *different* string, so the
   two ctor bodies could not have ICF-folded into this one.
3. `SetlistToStorePanel::Poll` hands the message to
   `StorePanel::Instance()->Handle(msg.mData, true)`, and the receiving panel's
   table declares `HANDLE_MESSAGE(MetadataLoadedMsg)`.

### ★ The measured sequence — read this before doing a naming like it

| step | `matched_functions` | `matched_code` |
|---|---:|---:|
| start | 43,699 | 4,069,004 |
| (a) map entry `0x82606020` alone | **43,698 (−1)** | **4,067,808 (−1,196 B)** |
| (b) + source uses `MetadataLoadedMsg` | 43,698 | 4,067,808 (**no recovery**) |
| (c) + repair the `0x82e01810` type name | 43,699 | 4,069,004 (**fully recovered**) |
| (d) + implicit `DataNode` conversion | **43,703** | **4,069,204** |

- **(a)** is the documented hazard firing verbatim: `name_check` **forgives** a
  placeholder target name, so naming a previously-anonymous address converts
  forgiven call sites into **checked** ones. The −1,196 B is precisely
  `?Poll@SetlistToStorePanel@@UAAXXZ` (1,196 B) falling off 100%.
- **(b)** is the instructive step. I **predicted full recovery and got none**,
  because I had fixed only *our* spelling while the **target** still carried the
  wrong type in its own symbol. A map-name repair, not a source change, was the
  missing half. *Both sides of a name comparison have to be right.*
- **(d)** the ctor row 90.4545 → 94.8485; the +200 B is five 40-byte EH funclets
  crossing elsewhere, not the ctor itself.

The (d) lever is the same one that moved `Handle`: explicit functional-cast
temporary vs implicit conversion. `Message`'s ctor takes `const DataNode &` and
`DataNode(int)` / `DataNode(const char *)` are not `explicit`, so the four scalar
arguments convert implicitly. Confirmed by register evidence — retail
`bl __savegprlr_27` vs our `__savegprlr_26` (one fewer callee-saved register) and
frame 0xb0 vs 0xc0 — both of which the change closed exactly.

**Residual on the ctor row (264 B @ 94.8485), 14 sites:** two `DataNode`
temporaries occupy **swapped** stack slots (0x70 ↔ 0x78), i.e. argument evaluation
order; and the one remaining explicit temporary `DataNode(arr, kDataArray)` is
still threaded through r29 where retail re-forms `addi r5,r31,0x70` — it is the
two-argument ctor and has **no implicit form**, so the (d) lever cannot reach it.

### 4.2 Anonymous-row census — `BandStorePanel`

Unit has **127 rows, 88 anonymous, 16 anonymous at 0% = 3,000 B** (the brief's
≈3.3 kB, less the 264 B just named).

`splits.txt` pins **13,352 B** across nine `.text` blocks, with eight holes:
`0x826058E8..0x82605970` (136 B), **`0x82605C38..0x82605CA8` (112 B — fully
accounted in §1)**, `0x82605D20..0x82605FF8` (728 B), `0x82606004..0x82606008`
(4 B), `0x8260625C..0x82606260` (4 B), `0x826067C0..0x826068A8` (232 B),
`0x826078B0..0x82607A10` (352 B), `0x82607A90..0x82607D30` (672 B).

| row | size | callee signature | reading |
|---|---:|---|---|
| `fn_82606A08` | 108 | `__adjust_heap<u64>` | `vector<u64>` / sort helper |
| `fn_82607570` | 328 | self-recursive, `__ucopy_trivial`, `memmove` | `vector<u64>` growth |
| `fn_826076B8` | 104 | self-recursive | ditto |
| `fn_82607720` | 80 | → `fn_82606960` | ditto |
| `fn_82607770` | 108 | `_M_insert_overflow<vector<u64>>` | ditto |
| `fn_826077E0` | 96 | `__unguarded_insertion_sort_aux<u64>` | ditto |
| `fn_82607840` | 100 | `__adjust_heap<u64>` | ditto |
| `fn_82606960` | 168 | `memmove` | ditto |
| **subtotal** | **1,092** | | **one coherent `vector<u64>`/`sort<u64>` cluster** |
| `fn_82608B70` | 452 | `__RTDynamicCast`, `StorePurchaseable::Exists`, `push_back<vector<u64>>`, `sort<u64>`, `adjacent_find`, `erase` | **`GetOfferIDsToEnumerate`** (below) |
| `fn_82606280` | 908 | `DataNode::Int/Str`, `String` ctor+assign, `Symbol` ctor, `FindArray` | metadata-array parse |
| `fn_82605878` | 104 | `__RTDynamicCast`, `InputMgr::SetUser(BandUser*)` | user-swap handler |
| `fn_82605B48` | 100 | `DataNode::Sym` | small accessor |
| `fn_826068A8` | 84 | `fn_827B4AE0` (a `StorePanel` member) | — |
| `fn_82606900` | 92 | leaf, no calls; called by `fn_82608B70` | sort comparator / predicate |
| `fn_82605720` | 80 | `fn_8278EC28` | — |
| `fn_82608D38` | 88 | `fn_8278EC28` | — |

### ★ The single biggest lever left in this unit — ONE unimplemented override

`BandStorePanel.h:65` overrides `virtual bool EnumerateSubsetOfOfferIDs() const
{ return true; }`. Returning **true** obliges the class to also override
`StorePanel::GetOfferIDsToEnumerate(std::vector<u64> &, bool)` — whose base
implementation is an empty default. **We do not override it.** `fn_82608B70`
(452 B) *is* that override: its callee set (`__RTDynamicCast`,
`StorePurchaseable::Exists`, `push_back<vector<u64>>`, `sort<u64>`,
`adjacent_find`, `erase<vector<unsigned int>>`) is exactly what such a function
does.

Implementing it would instantiate the whole `vector<unsigned __int64>` /
`sort<unsigned __int64>` template family, which is:

- the **1,092 B** of anonymous STL helpers above, **plus**
- the three briefed template rows, all at 0%:
  `??$__partial_sort@PA_K…` 152 B, `??$__introsort_loop@PA_K…` 188 B,
  `??$sort@PA_K@stlpmtx_std@@YAXPA_K0@Z` 132 B = **472 B**, **plus**
- `fn_82608B70` itself, **452 B**.

**≈ 2,016 B behind one missing member function.** This is the highest-value
NOT-done item in the lane and is entirely inside game code (`src/band3/`), i.e.
nothing fences it — it simply needs more budget than remained.

---

## 5. Task 5 (STRETCH) — NOT STARTED

`?BuildList@StoreOfferProvider@@…` (2,536 B @ 96.0426) and its anonymous rows
(`fn_82663328` 180 B, `fn_826635D8` 392 B, both 0%) were **not opened**. Adjacent
sub-100 rows measured in passing and left alone:
`?Handle@StoreOfferProvider@@…` 1,556 B @ 99.8458,
`?PosToNextGroupPos@StoreOfferProvider@@QAAHH@Z` 92 B @ 93.1739,
`?IsActive@StoreOfferProvider@@UBA_NH@Z` 52 B @ 71.9231.

`IsActive` at 52 B / 71.9% is the cheapest-looking of these and is untouched.

---

## 6. NOT-DONE list, with the evidence that would overturn each

| # | item | blocked by | what would overturn it |
|---|---|---|---|
| 1 | `sizeof(XboxPurchaser)` 0x50 → 0x28 | engine header `src/system/meta/StorePurchaser.h`, out of lane scope | a lane owning that header. **Prize: +1,104 B / +1 fn**, the row's only charge |
| 2 | `GetOfferIDsToEnumerate` override | budget only — **nothing fences it** | ≈2,016 B across `fn_82608B70` + 8 anon STL rows + 3 template rows |
| 3 | `GetIndexFile()` real body | needs `fn_8250FDF8` identified and `0x82c73fdc` read through the **`.data`** offset delta | closes Poll idx 77/189 + clusters 4/5 |
| 4 | `Handle` idx 94–98 (temporary lifetime) | `ObjMacros.h` spells `(action);`; `(a; b)` will not compile | change to `action;` — cascades to every `HANDLE_ACTION` tree-wide |
| 5 | `Handle` idx 397–407 (6 sites) | pure instruction scheduling | permuter, which is **off by standing directive** |
| 6 | ctor row swapped slots 0x70↔0x78 | argument evaluation order | an ordering that reproduces it without metric-fitting |
| 7 | `StorePanel.cpp:302` split-temp idiom | `src/system/meta/StorePanel.*` fenced | likely the same defect fixed in UGCPurchasePanel; **unmeasured** |
| 8 | Task 5 (`BuildList@StoreOfferProvider`) | budget | not started |
| 9 | `mUserCanDoInput` existence | no evidence either way — removal is layout-neutral | a retail site that reads/writes `this+0xe1` |

---

## 7. Gate chain — RUN, in order, in the worktree, as the lane's last actions

| # | gate | result |
|---|---|---|
| 1 | `./tools/ninja-locked` (full) | **rc=0**, 86-line log (the expected steady state), 0 recompiles |
| 2 | `scripts/verify_ruler_agreement.py --check` | **rc=0** — both objdiff-cli entry points resolve `name_check` / `combineDataSections=true` / `combineTextSections=true` / `ppc.calculatePoolRelocations=false` |
| 3 | `scripts/verify_objs_patched.py --verify-manifest` | **rc=0** — 1,219 decomp + 3,105 target objects match `tree_sha256=cee36cb4d0b521e7`; denylist clean (6 addresses, none named in 495,491 symbols) |
| 4 | `tools/icf_alias_finder.py --validate` | **rc=0** — PASS, 1,407 map-consistent, 249 tolerated, **0 CONTRADICTED**, 1,657 total |
| 5 | `tools/funclet_homing.py --validate` | **rc=0** — PASS, 25,052 HOMED / 1,226 ORPHAN / 1 MIS-PINNED / 42 UNPINNED-FUNCLET, fan-in uniformly 1 |
| 6 | `tools/native_build_gate.sh` | **rc=0** — see verbatim line below |

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

`skipped=0` is the load-bearing field, not the word `PASS` — an INCOMPLETE run
also prints `PASS` one space apart from a full one, and the 0-SKIP rule is what
has caught every false green so far.

### Figures re-read from `report.json` AFTER the final full build

```
matched_functions       43703
matched_code            4069204
matched_code_percent    39.71091
total_functions         69240
total_code              10247068
fuzzy_match_percent     49.863712
masked_equal_functions  23132
provenance.diff_config  functionRelocDiffs=name_check, combineDataSections=true,
                        combineTextSections=true, ppc.calculatePoolRelocations=false
```

Identical to §0 to the last digit, so the headline is a post-gate reading, not a
mid-lane one.
