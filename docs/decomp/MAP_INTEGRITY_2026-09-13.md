# Lane W9-B — two map-integrity questions two lanes deliberately left open

**Branch** `w9-map-integrity` · worktree `~/tmp/wt-w9-b` · base `1d560c2b`
(`git merge-base --is-ancestor 1d560c2b HEAD` asserted before the first edit).
Ruler: **`name_check` (graded)**, read from `report.json`'s
`provenance.diff_config` (`functionRelocDiffs=name_check`), never assumed.

Baseline after this lane's mandatory first full build (a reflinked worktree's
target objs are pre-renamer, so every mangled-name lookup reads "absent" until
the renamer's pre-compile step has run — FOLDPROVE-2):

```
matched_functions   42,739      masked_equal        22,980
matched_code     3,865,344 B    matched_code_percent 37.729187
fuzzy_match_percent  49.127808
verify_objs_patched --verify-manifest -> OK: 1205 decomp, 3085 target objects
```

**Both questions are answered, and they turned out to be the same question.**

| | verdict |
|---|---|
| **Q1 `PlatformMgr` four-cycle** | **Both remaining names PROVEN.** The repair was **five** rows, not four. Landed, measured **Δmatched +0 / Δbytes +0 / Δfuzzy +0.000709 pp**, exactly as pre-registered. |
| **Q2 is `0x823d14c0` mis-named?** | **NO.** It is a genuine pointer-element `list<T*>::insert` and the ICF survivor of that family. W8-D's *world 1* is confirmed and *world 2* refuted on a whole-binary population control. **The alias is still NOT installed** — the sanctioned chase gate refutes, for a real and separable reason (§3.4). |

---

## 0. The unifying finding

Both questions are instances of one mechanism, and naming it is the most
transferable thing in this document:

> ★★★ **When a family of byte-identical bodies is distinguished only by ONE
> relocation, and that relocation's retail target is an ANONYMOUS placeholder,
> the map names on that family are UNFALSIFIABLE BY THE SCORE — every
> permutation scores `fuzzy == 100`.**

`name_check` forgives a placeholder target (`fn_`/`lbl_`/…), so the single
differing field is uncharged and objdiff cannot tell a right name from a wrong
one. The score is not merely silent here; it *actively pays* for the wrong
answer. Measured on both families in this lane:

* the four `PlatformMgr` `LocalUser*` wrappers — all four **wrong**, all four at
  **fuzzy 100.0** (§2);
* three `list<T*>::insert` rows — all three **provably wrong**, all three at
  **fuzzy 100.0**, worth **300 B of false credit**, while the one **genuine**
  pointer insert scores **0.0** (§3.5).

⇒ **Only retail-byte adjudication of the differing relocation can resolve such a
family.** The instrument is: read the callee, identify *it*, and propagate up.

---

## 1. Every briefed figure, re-verified literally

Tested before anything was built on it, per the standing rule.

| briefed | measured | |
|---|---|---|
| `?Handle@PlatformMgr@@` is 3,112 B | **3,112 B**, `fuzzy 99.95501` | ✓ |
| four wrapper bodies `0x48` apart, byte-identical but the final `bl` | **0x82514CB8 / D00 / D48 / D90**, 72 B each, identical but for one `bl` | ✓ |
| `fn_8251CA08` passes `(padNum, 4, 0, -1)`; entrypoint constant `0x0004` | **`li r6,-1 / li r5,0 / li r4,4`**, `XSHOWMARKETPLACEUI_ENTRYPOINT_CONTENTLIST_BACKGROUND == 4` | ✓ |
| `fn_8251BFE8` defaults `ret=1`, tails `return ret==0` | **`li r30,1`**, `subic/subfe`, `extrwi r3,r11,1,26` | ✓ |
| five named pointer-element `list<T*>::insert` addresses | **exactly five** in the map | ✓ |
| `0x823d14c0` fan-in 289 sites / 118 files | not re-counted; superseded by a stronger instrument (§3.2) | — |
| a rename would fix 2 charges and create 287 | **accepted, and now for a stronger reason** (§3.6) | ✓ |
| `icf_pair_adjudicate.py --selftest` PASSES | **PASSES**, and `--chasetest` too, *with an in-family decoy REFUTED* | ✓ |

⚠ **One briefed figure needed correcting, and it changed the repair.** W8-B's
handoff describes the `PlatformMgr` repair as *"four coupled rows"*. It is
**five**: closing the cycle displaces `?IsUserAGuest@…`, which is not a spurious
name but a **real name sitting on the wrong address** (§2.3). Landing only four
would have evicted it into nowhere.

---

## 2. Q1 — the `PlatformMgr` four-name cycle: PROVEN, and it was five rows

### 2.1 The method, and why it is map-independent

The four wrappers differ in exactly one field, so **identifying the inner callee
identifies the wrapper**. Every identification below is anchored either on a
literal constant in the instruction stream (which no map row can influence) or
on a structural match to DC3's source — never on another map name.

W8-B proved two. Here are the other two.

**`fn_8251BFA8` = `PlatformMgr::ShowFriendsUI(int)`.**
```
mr r31, r4                ; padNum
bl fn_82514988            ; IsSignedIn(this, padNum)
clrlwi. r11, r3, 24 / beq
mr r3, r31 / bl fn_8283D710   ; ONE argument
blr                       ; void
```
`void f(int) { if (IsSignedIn(padNum)) <one-arg XAM UI>(padNum); }` is the shape
of exactly one `PlatformMgr` member (`PlatformMgr_Xbox.cpp:142`), and
`XShowFriendsUI(DWORD dwUserIndex)` is one-arg (`src/xdk/xapilibi/xbox.h:25`).
`fn_8283D710` is a one-instruction import thunk (`b 0x82C4BD9C`) in the same
adapter block as `fn_8283D728`, W8-B's proven `XShowMarketplaceUI`.

**`fn_8251C118` = `PlatformMgr::InviteParty(int)`.** This one is proven
*structurally*, not by elimination:
```
mr r30,r3 / mr r31,r4
bl fn_8251C048            ; IsInParty()  -- RESULT DISCARDED
mr r4,r31 / mr r3,r30
bl fn_82514988            ; IsSignedIn(padNum)
clrlwi./beq
li r4,0 / mr r3,r31 / bl fn_82B54130   ; (padNum, 0)
blr                       ; void
```
and DC3's body (`dc3-decomp/src/system/os/PlatformMgr_Xbox.cpp:386`) is
```cpp
void PlatformMgr::InviteParty(int padNum) {
    MILO_ASSERT(IsInParty(), 0x87B);
    if (IsSignedIn(padNum)) { XPartySendGameInvites(padNum, 0); }
}
```
— line for line, **including the discarded return**, which is exactly the
`MILO_ASSERT(cond,line) == ((void)(cond))` *evaluates-but-discards* signature
this tree documents. `XPartySendGameInvites(DWORD, XOVERLAPPED*)` is two-arg
with a NULL second (`src/xdk/xparty/xparty.h:32`).

The bottom anchor is map-independent: **`fn_8251C048` = `IsInParty()`**, proven
on the literal **`0x807D0003`** (`lis 0x807d / ori 0x3` = DC3's `noPartyResult`),
the four `IsSignedIntoLive(0..3)` short-circuits, a `stwu r1,-0xf80` frame sized
for `XPARTY_USER_LIST`, and a `subf/subic/subfe` = `result != noPartyResult`
tail. That also fixes `fn_82B54168 = XShowPartyUI` and
`fn_82B54100 = XPartyGetUserList` — three entries of the xparty import table at
`0x82E11E98` (`+0x0`, `+0x4`, `+0x10`).

### 2.2 ⛔ "What else satisfies the witness" — the check that found the fifth row

W8-C's rule applied to my own result. I scanned **all of retail `.text`** for the
wrapper's exact 72-byte shape (`lwz r11,0(r4) / … / mtctr / bctrl` = vtable slot
0 = `GetPadNum()`, then one `bl`), rather than trusting the four I was handed.

**There are SIX, not four** — and widening to *any* function making that
GetPadNum vtable call gives **ten**:

| body | inner | shape | map name | verdict |
|---|---|---|---|---|
| `825149B8` | `82514A08` | padnum<0 | `IsUserSignedIn` | ✓ correct |
| `82514A20` | `82514A70` | padnum<0 | ABSENT | unnamed |
| `82514A88` | `8251BE30` = `IsSignedIntoLive` | plain | `IsUserSignedIntoLive` | ✓ **correct** |
| `82514B30` | `8251C778` = `HasOnlinePrivilege` | plain | `UserHasOnlinePrivilege` | ✓ **correct** |
| **`82514B78`** | `8251C1A0` = `IsPadAGuest` | padnum<0 | ABSENT | ⛔ name displaced |
| **`82514CB8`** | `8251BFA8` = `ShowFriendsUI` | plain | `InviteUserParty` | ⛔ |
| **`82514D00`** | `8251CA08` = `ShowOfferUI(int)` | plain | `IsUserAGuest` | ⛔ |
| **`82514D48`** | `8251BFE8` = `ShowPartyUI` | plain | `ShowOfferUI` | ⛔ |
| **`82514D90`** | `8251C118` = `InviteParty` | plain | `ShowUserFriendsUI` | ⛔ |
| `82514DD8` | `8251CB20` | plain | `GetOwnerUserOfGuestUser` | ✓ correct |

★ **Two of the six are named CORRECTLY, and my inner-callee method reproduces
both.** That is a control that could have failed and did not — without it, a
method that returns "the map is wrong" for every input would be
indistinguishable from a method that works.

### 2.3 The fifth row: `IsUserAGuest` was DISPLACED, not spurious

`fn_8251C1A0 = PlatformMgr::IsPadAGuest(int)`, proven map-independently on the
literal **`0x525`** (`cmplwi cr6, r3, 0x525` = `ERROR_NO_SUCH_USER`, 1317) and
`extrwi r3, r11, 1, 30` = `signinInfo.dwInfoFlags >> 1 & 1`. Its caller
`0x82514B78` is therefore `IsUserAGuest(const LocalUser*)`.

⇒ the name was **on the wrong address**, and a four-row fix would have thrown it
away. Note also the structural refutation available *before any byte is read*:
`IsUserAGuest` returns `bool`, is `const` (`QBA`), and carries a `padnum < 0`
arm — it cannot be one of the four identical `void` plain wrappers. **A layout
difference forecloses an identification the same way it forecloses a fold.**

### 2.4 ★ Corroboration found AFTER the identification, not before

The map's own **`_bijection_arbitrary`** list flags **exactly** the four cycle
addresses (`CB8/D00/D48/D90`) and **none** of the six correctly-named ones. Its
own comment explains both the defect and the score:

> *VAs whose NAME was assigned by a bijection over a reloc-masked BYTE-IDENTICAL
> equivalence class … ANY bijection within such a class scores 100% on every
> pair — the MATCH is true and byte-verified, but WHICH name belongs on WHICH VA
> is NOT established … an arbitrary pick is now a LIABILITY rather than a free
> choice, and refining one is POSITIVE-yield repair work.*

So the map recorded its own uncertainty, and §0's mechanism is the reason the
uncertainty is invisible to the metric. The four are removed from that list, per
the MAPDEF-3 (`db9eb318`) and `MAP_NAME_INJECTIVITY.md` precedent.

### 2.5 Pairability, checked BEFORE writing

objdiff pairs target↔base by name per unit, so a row whose base obj cannot
define the name reads 0% forever. Our compiled `PlatformMgr.obj` defines **all
five** target names, including `?ShowUserPartyUI@PlatformMgr@@QAA_NPBVLocalUser@@@Z`
which the map had placed nowhere (taken from our own compiler output, not
hand-mangled). `0x82514B78` is inside the pinned span
(`.text 0x82514968–0x825164A8`) and is a live 84 B target row.

### 2.6 Pre-registered vs measured

**Pre-registered Δmatched 0 / Δcode_bytes 0**, reasoned as: the four rows are
*already* at `fuzzy 100` (§0), so a correct name cannot add bytes there;
`Handle@PlatformMgr` loses 4 of 7 charges but keeps 3 (the empty-body
`EnableXMP`/`CheckMailbox`/`RunNetStartUtility` folds) so it stays under 100 and
`matched_code` keys on `fuzzy == 100`; and `IsUserAGuest` trades a **false** 100
at `D00` for a **true** partial at `B78` (our 72 B body vs retail's 84 B — we
lack retail's `padnum<0 → return false` arm), while `ShowUserPartyUI` newly pairs
at 100. −72 +72 = 0.

**Measured** (`ab_measure --from-dirty`, kind `map` ⇒ forced re-split both legs,
`renamer_patched=1823`, split fixed point on both legs, objdiff-cli sha
`a5c35b15d7d46ac4`):

```
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0
Δfuzzy=+0.000709pp   (legA 49.127808 -> legB 49.128517)
units at 100% [mpn]: 160 -> 160 (0 reached, 0 fell off)
units at 100% [fuzzy]: 134 -> 134 (0 reached, 0 fell off)
[control none] FLAT -- consistent with a pure RE-name
```

Exactly as pre-registered on every key. The accuracy shows up **only** in
`fuzzy` (+0.000709 pp), which is the correct signature: `Handle`'s charged-site
count drops 7→3 and `0x82514B78` goes from unpaired to paired.

`tools/icf_alias_finder.py --validate` after the forced re-split and full build:
**`PASS — 1357 map-consistent, 236 tolerated, 0 contradicted, 1595 total`**.

**Landed** (`74063c30`). Δ0 is the honest result and the reason to land it
anyway: the map was wrong on four rows and is right on five now.

---

## 3. Q2 — `0x823d14c0` is NOT mis-named

### 3.1 What had to be decided

W8-D refused the `list<char*>::insert` ≡ `list<Object*>::insert` alias as
**UNPROVABLE rather than REFUTED**, because two worlds fit its evidence equally:

1. the pointer family **folds**, `0x823d14c0` is the ICF survivor, alias real; or
2. the family does **not** fold (48 retail body-twins), `0x823d14c0` really is
   `list<char*>::insert`, and **the map name is wrong**.

Its own note said what would settle it: *are the other four named
pointer-element `insert` addresses genuine, or four more mis-namings?*

### 3.2 ★★★ The instrument: a whole-binary population control on retail bytes

Not the map, not the score, not `objdiff` — a byte scan of the complete `.text`
of `orig/45410914/band.exe` (`0x82270000`, `0x9DCE3C` bytes), so coverage does
not depend on what happens to be pinned.

`list<T*>::insert` is a 100 B / 25-instruction body whose **only** relocation is
`_M_create_node<T>`. So the family's fold classes are *determined* by that one
callee — W7-D's rule exactly. And a pointer-element `_M_create_node` is
identifiable without any name: it allocates **`0xc`** (8-byte `_List_node_base`
+ a 4-byte pointer) and copies the payload with a **raw word `lwz`/`stw` and no
constructor call**.

```
pointer-element _M_create_node (0xc alloc, word copy, NO ctor):  1
    0x82520150  -> MemOrPoolAllocSTL @0x827BD208
list<T>::insert-shaped bodies (100 B, 25 instr):                48
    of which call a POINTER-element create_node:                 1
    0x823D14C0 -> 0x82520150
    distinct create_node targets among all 48 inserts:          48
```

**Exactly one pointer create_node and exactly one pointer insert in the whole
image.** World 2 requires one address per distinct pointer `T` (RB3 has many:
`Object*`, `char*`, `CharClip*`, `RndDrawable*`, `MidiParser*`, …). It predicts
*many*; the binary contains *one*. **World 2 is refuted; world 1 is confirmed.**

### 3.3 ⛔ Why the "48 retail bodytwins" never refuted the fold

W8-D's methodological note guessed the mechanism and it is now measured. The 48
twins carry **48 distinct `_M_create_node` targets** — they are 48 different
**value-type** instantiations kept apart by a per-`T` copy constructor. The
gate's population control counts **masked** body-twins, and masking discards
precisely the relocation that defines the fold class. It was measuring the wrong
population, so its refutation was vacuous for the pointer subclass.

Three controls that make the instrument trustworthy rather than merely
agreeable:

* **positive** — it finds 48 non-folded value inserts, so it *can* see a
  non-folding family;
* **negative** — `list<bool>::insert` (`0x8274D128`) also allocates `0xc` but
  copies with a **byte** op, so it sits at its own separate create_node
  (`0x8274CF60`). The mechanism discriminates on real bytes, exactly as ICF does;
* **anti-vacuity** — widening to *any* `li r3,0xc` + `bl MemOrPoolAllocSTL`
  regardless of shape yields 4 sites; two use a `+4` (singly-linked) node header
  and one is the `list<bool>` byte-copy. Only `0x82520150` survives.

**In-tree corroboration, found after the scan:** `0x823d14c0` *already* carries a
gate-proven alias group (survivor `list<CharClip*>::insert`, folded
`list<CharPollableSorter::Dep*>::insert` — **both pointer types**, tier CF2), and
the **root** `0x82520150` carries a **T1 byte-identity** group folding the same
two pointer `_M_create_node` spellings. The tree already accepts that this family
folds at these exact two addresses.

⇒ **`0x823d14c0` is a genuine `list<T*>::insert` and the ICF survivor of the
pointer-element family. Its map name `list<Object*>::insert` is one arbitrary
surviving spelling — which is what an ICF survivor name IS, not a defect.**

### 3.4 ⛔ The alias is still NOT installed — the sanctioned gate refutes

Having proved the fold, I ran the tree's own adjudicator rather than
hand-installing. Controls first, because a gate that cannot fail proves nothing:
`--selftest` **PASSES** and `--chasetest` **PASSES with its in-family decoy
REFUTED**, so `--chase` does not admit everything in a family.

`--chase` on the actual pair returns **CHASED T1: REFUTED**, and the chain is the
finding:

```
insert<Dep*>            vs insert<char*>
 └ _M_create_node<Dep*> vs _M_create_node<char*>
    └ ?MemOrPoolAlloc@@YAPAXHPBDH0@Z  (4-arg) vs ?MemOrPoolAllocSTL@@YAPAXH@Z (1-arg)
       └ ?PoolAlloc@@YAPAXHHPBDH0@Z   (5-arg) vs ?PoolAlloc@@YAPAXHH@Z        (2-arg)
          └ ??2CriticalSection@@SAPAXI@Z      vs ??2ChunkAllocator@@SAPAXI@Z
```

The chase bottoms out **below** the container layer entirely, in the
**allocator debug-overload stratum**: at each level retail's map name is the
*instrumented* overload (`file, line, name`) and ours is the *stripped* one.
That is the same `POOL_OVERLOAD` phenomenon `symbol_aliases.json`'s own comment
describes, and closing it is `tools/alloc_fold_gate.py`'s territory — a separate
and much larger change.

⇒ **The refusal is a real, separable blocker, not noise, and it is not evidence
against §3.2.** I did not hand-install over a refuting gate: the standing rule is
that a fabricated alias lifts `name_check` **by construction** while the `none`
control reads flat **by construction**, so an install that the sanctioned
instrument refuses would produce a confident "+904 B measured" worth nothing.

⚠ **And I nearly asserted a fourth map defect on a witness that cannot
discriminate.** Retail `0x827BD208` is
`cmpwi r3,0 / blr; cmpwi r3,0x80; li r4,0; b MemAlloc` — it reads **only `r3`**,
which looks like it refutes the 4-arg `?MemOrPoolAlloc@@YAPAXHPBDH0@Z` name by
the MPNGAP-1 call-site-arity rule. **It does not.** A release-build 4-arg
`MemOrPoolAlloc` that discards its three stripped debug arguments is
byte-identical to a 1-arg `MemOrPoolAllocSTL`. **Both candidate classes satisfy
the witness** — W8-C's trap, landing on this lane's own analysis, caught before
it became a claim.

### 3.5 ⛔ But FOUR of the five named pointer-element addresses ARE mis-named

The same census answers W8-D's actual question, and the answer is worse than it
expected. Reading each named address's `_M_create_node` gives its element's real
shape:

| map name | create_node | alloc | ctor? | element | verdict | fuzzy |
|---|---|---:|---|---|---|---:|
| `list<BandCamShot*>::insert` @`822b55e0` | `822b4c28` | **0x6c** | **yes** | value, 0x64 | ⛔ **wrong** | **100.0** |
| `list<RndDrawable*>::insert` @`822b5728` | `822b4ca8` | **0x20** | **yes** | value, 0x18 | ⛔ **wrong** | **100.0** |
| `list<MidiParser*>::insert` @`827e5318` | `827e4bf0` | **0x5c** | **yes** | value, 0x54 | ⛔ **wrong** | **100.0** |
| `list<RndTransformable*>::insert` @`823b60c0` | — | — | — | **not an `insert` at all** | ⛔ **wrong** | 64.6 |
| `list<Object*>::insert` @`823d14c0` | `82520150` | **0xc** | **no** | **pointer** | ✅ **genuine** | **0.0** |

A `T*` element has no copy constructor and needs exactly 4 bytes; three of these
allocate 0x20–0x6c **and call a real copy constructor**, so they are value-type
instantiations carrying pointer names. `0x823b60c0` is not even in the 48-body
population — it is a two-argument, three-call function of an entirely different
shape, foreclosed by layout before any byte is read.

★ **And note the last column, which is §0 again.** The three wrong names score
**100.0** — **300 B of false credit** — because their retail `_M_create_node`
targets are **anonymous** (`0x822b4c28`, `0x822b4ca8`, `0x827e4bf0` are all
absent from the map), so `name_check` forgives the one differing relocation. The
one **genuine** row scores **0.0**. The metric is paying for the wrong answers
and withholding from the right one.

⚠ A fifth, **repairable** instance fell out: `0x824e0f68` is named
`?insert@?$list@I…@Z` (`list<unsigned>`) but its create_node `0x824e0458` is
named `?_M_create_node@?$list@UOldMMInst@@…@Z`. Its element is a value type, so
the row is `list<OldMMInst>::insert`. Unlike the three above this one **charges**
(fuzzy 99.8) because its create_node *is* named — so repairing it should pay.
**H3.**

### 3.6 The instrument question, answered

W8-D's *"a map rename is the wrong instrument — it would fix 2 charges and create
287"* is **correct, and now for a stronger reason**: the name is not wrong at
all, so renaming `0x823d14c0` would not be a costly repair, it would be
**introducing an error**. The mechanism that expresses "two spellings share one
address because ICF folded them" is a `SymbolEquivalences` alias — and that
remains blocked on §3.4, not on the identification.

---

## 4. What this lane did NOT do, and why

* **Did not install the `list<char*>` alias** (§3.4). The fold is proven; the
  sanctioned gate refuses on the allocator-overload chain. Installing over a
  refuting gate is the exact fabrication hazard the standing rule names. This
  keeps W8-D's 904 B `FileRelativePath` prize on the table — deliberately.
* **Did not touch the allocator stratum.** `MemOrPoolAlloc`/`PoolAlloc`/
  `operator new` debug-vs-stripped overloads reach far past this lane and belong
  to `alloc_fold_gate.py`. Sized as the blocker, not attempted. **H1.**
* **Did not delete the three false-100 `insert` names** (§3.5). I can prove they
  are wrong but not what they should be, and deleting un-pairs three rows for a
  predicted **−300 B**. Per the "accuracy > headline %" directive that is
  arguably still correct, but it is a separate measured change and a partial fix
  here is the same trap W8-B refused. **H2.**
* **Did not repair `0x824e0f68`** — provable and probably positive, but a fourth
  change. **H3.**
* **Did not name `0x82514A20`** (the one unnamed `GetPadNum` wrapper; by
  elimination `HasUserSigninChanged`). Elimination is not proof, and CLAUDE.md is
  explicit that naming an anonymous address is a bet, not a freebie. **H4.**
* **Did not touch `PlatformMgr_Xbox.cpp`'s NUI arms** (W8-B's §4.3 side-finding).
  Retail has no `sXShowCallback` branch; that blocks the three inner `(int)`
  functions independently of the map, and it is a source change, not a map one.
* **Did not run the native gate** — this lane changed exactly one file,
  `scripts/target_symbol_map.json`. `src/` did not move, so the gate is not
  applicable.

## 5. Handoffs, in priority order

1. ★★★ **H1 — the allocator debug-overload stratum** (§3.4). This is what blocks
   `list<char*>::insert`, and the chase chain shows it blocks *everything*
   underneath the STL container layer, not one pair. Prerequisite for W8-D's
   904 B and probably far more. Start from `tools/alloc_fold_gate.py` and the
   `POOL_OVERLOAD` comment in `scripts/symbol_aliases.json`.
2. ★★ **H3 — `0x824e0f68` → `?insert@?$list@UOldMMInst@@…@Z`** (§3.5). Proven by
   its named create_node, currently charging at 99.8, so unlike its three
   siblings this one should pay. Small and self-contained.
3. ★ **H2 — the 300 B of false credit** (§3.5). Three provably-wrong names paid
   at 100.0. Removing them is an accuracy win and a measured **−300 B**; repairing
   them properly needs identifying three value types first.
4. ⚠ **H4 — `0x82514A20`** is the last unnamed `GetPadNum` wrapper; identify
   `fn_82514A70` before naming it.
5. ⚠ **Re-derive, never inherit.** Every number here is measured at `1d560c2b`
   with the ruler read from `report.json`. The two figures worth re-checking
   first are the whole-binary population counts in §3.2 — they are the load
   bearing ones, and they are cheap to re-run.

## 6. Reusable lessons

* ★★★ **§0** — a byte-identical family distinguished by one *forgiven*
  relocation has map names that are unfalsifiable by the score, and the score
  pays for the wrong ones. Adjudicate the differing relocation on retail bytes.
* ★★★ **A masked-body population control measures the wrong population when the
  fold class is defined by the relocation that masking discards** (§3.3). This
  is why "48 retail bodytwins" refuted nothing.
* ★★ **Scan the whole binary for the shape before trusting the count you were
  handed** (§2.2). It turned a four-row repair into a correct five-row one, and
  it supplied the control (two already-correct names) that made the method
  credible.
* ★★ **Both candidate classes can satisfy an arity witness** when the extra
  arguments are unused (§3.4). A release-stripped debug overload is
  byte-identical to the short form.
* ★ **A tool's own uncertainty marker is corroboration, but only if you find it
  after** (§2.4). `_bijection_arbitrary` flagged exactly the four wrong rows.
* ★ **A Δ0 result is worth landing** when it converts four arbitrary names into
  five proven ones; the metric was structurally incapable of registering it.
