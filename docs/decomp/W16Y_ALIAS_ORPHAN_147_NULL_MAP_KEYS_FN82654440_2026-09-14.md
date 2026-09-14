# W16-Y — alias orphan group 147, the 109 null map keys, and `fn_82654440`

**Lane:** W16-Y (opus) · **Branch:** `w16-y`, off main `b4104f3d` · **Worktree:** `~/tmp/wt-w16-y`
**objdiff 4.2.9, `provenance.tool_binary_hash 5a51cd51fe0a353f` on every leg quoted below.**

| | matched_functions | matched_code | matched_code_percent | fuzzy_match_percent |
|---|---:|---:|---:|---:|
| lane baseline (`b4104f3d`) | 43,291 | 3,986,972 | 38.912640 | 49.489254 |
| lane final | **43,292** | **3,987,072** | 38.913616 | 49.490227 |
| **Δ** | **+1** | **+100 B** | +0.000976 | +0.000973 |

Commits: `0b98cf97` (Item 1), `37f17d9a` (Item 2 consumer guards), `023f187f` (Item 2 naming).

---

## Item 1 (LEAD) — the orphan alias group 147 / duplicate-survivor defect

### The brief's premise was wrong in one load-bearing detail

The brief states `scripts/target_symbol_map.json` "has **no entry at `0x823c3ac8`**". Measured: the entry
**exists** and its value is `null`. That single fact couples Items 1 and 2 — `0x823c3ac8` is one of the 109
deliberate tombstones Item 2 is about. Everything below was re-derived from bytes rather than from the brief.

### What is at retail `0x823c3ac8`

It is **a function**, not a stub, not an EH prefix, not a dtk mis-carve:

- It is an exact `.pdata` BeginAddress with a decoded extent of **100 bytes**
  (`tools/pdata_extent.py`, whose must-fail `>>2` control selftest PASSes).
- Its body is 25 instructions with exactly **one** `bl`, at `+0x24`, to the node creator `0x823c3960`.
- `0x823c3960` does `li r3,24` = 8-byte `_List_node_base` + a **16-byte element**, then `addi r3,r3,8` and
  calls a copy-constructor.
- `sizeof(ConstraintSystem)` is **16** (compiler-authoritative,
  `scripts/harvest/class_layout_report.py`, not the `// 0xHEX` comments).
- Our compiled `CharBlendBone.obj` emits
  `?insert@?$list@UConstraintSystem@CharBlendBone@@…` at `fn_size 100` with exactly one reloc at `+0x24`,
  targeting `?_M_create_node@?$list@UConstraintSystem@…`, whose own body is `li r3,24`.

Same extent, same shape, same single relocation site, same node immediate. `0x823c3ac8` is
`list<ConstraintSystem>::insert`.

**It is answer (b) in the brief's taxonomy — a different function entirely**, and *not* the
`list<Hmx::Object*>::insert` instantiation the alias group claimed. Independently corroborated by
`docs/decomp/CONTAINER2_2026-09-10.md:151,303` ("`0x823c3ac8` is not `list<Object*>::insert` at all") and by
`docs/decomp/MAPVEIN3_2026-09-11.md:192`.

#### One prediction of mine failed — stated explicitly

I predicted the 8 bytes at `0x823c3ac0` were an **EH prefix** (a `.text` pointer + an `.rdata` pointer),
which would have made the neighbouring map row wrong. Measured bytes are `4bfffd68 00000000` =
`b 0x823c3828` plus alignment padding — a real **8-byte leaf tail thunk**, which gets no unwind record and
so is correctly absent from `.pdata` (the AUDIT-NC stratum). The map row naming `0x823c3ac0`
`??$_Destroy@UConstraintSystem@CharBlendBone@@…` is **correct**. I was wrong; the row stands.

#### A second instrument failure worth recording

Masked byte comparison **could not discriminate** `0x823c3ac8` from `0x823d14c0`: the two bodies differ in
exactly one of 25 words, and that word is the `bl` — precisely the word the mask zeroes. A masked compare
therefore reports "identical" for two functions that provably did not fold. The discriminator that works is
**relocation targets resolved by name, plus the node-allocation immediate**.

### Why the two groups collided, and why it was not cosmetic

Group 147 (`0x823c3ac8`, `folded: []`, 86 withdrawn, evidence T1) shared its `survivor` spelling with the
live group 1480 (`0x823d14c0`, 4 folded, evidence CF2). Group 1480's four folded memberships are all
**pointer-element** lists (`P6AXXZ`, `PAD`, `PAUDep@CharPollableSorter`, `PAVCharClip`) — 4-byte elements,
12-byte nodes, consistent with `0x823d14c0`'s creator `?_M_create_node@…<CharPollableSorter::Dep*>` at
`0x82520150` doing `li r3,12`.

Because `tools/alias_withdrawals.py` keys its denial table on `(survivor, spelling)` **and** on
`(address, spelling)`, group 147's 86 withdrawals were silently denying **two of group 1480's four live CF2
memberships**. The next `icf_alias_build.py --merge` would have dropped them. The duplicate survivor was a
live clobber hazard, not a cosmetic key collision — which is exactly what the two red tests were detecting.

### The repair

Smallest change the bytes license, under the existing `merged_in` convention, nothing deleted:

- Group 147 merged into group 1480; `groups` 1,632 → **1,631**.
- 84 withdrawal records carried over **verbatim** (same denial key, same meaning).
- 2 records whose spelling is a **live** member of 1480's `folded` list converted to `restored`, keeping the
  superseded record verbatim (the idx-4 / `??$MakeString@H@@` W9-D precedent). Those two were
  `?insert@?$list@PAD…` and `?insert@?$list@P6AXXZ…`.
- No new naming claim, no membership deleted without a record.

### Prediction vs measurement

**Predicted Δ0, by mechanism**, before building: objdiff's `parse_msvc_map` calls `add_group` only when
`symbols.len() > 1`, so a group forgiving nothing contributes no equivalence; retiring it cannot move a
score. **Measured exactly Δ0** — 0 rows crossed, 0 rows fell out, on a full build.

`python3 tools/icf_alias_finder.py --validate` → **PASS, 1,631 groups, 0 CONTRADICTED**.
`python3 -m pytest tools -q` → **326 passed, 0 failed** (from 324 passed / 2 failed).

No new test was added: the two existing tests assert survivor- and address-injectivity over groups, which is
exactly the invariant that was violated, so they already catch a regression of what I found.

---

## Item 2 — the null-valued `target_symbol_map.json` keys

### Count and verdict

**109** null-valued address keys (the figure in the lane assignment; the brief's own list is a prefix of it).
First few: `0x82266fc0`, `0x822707c8`, `0x82273dd0`, `0x82289748`, `0x822a0fa0`, `0x822a8108`, `0x822a83c0`,
`0x822a8778`.

**Verdict: deliberate, documented "deliberately unclaimed" tombstones — NOT a defect.** Provenance
`cd54323b` and `a79ab221` (lane L7). `scripts/obj_target_symbol_renamer.py`'s `load_address_map` documents
them and **skips** them with an `n_null` counter; the build log reads
`[map] skipped 108 null … 5 denylisted`. The 108-vs-109 gap reconciles **exactly**: `0x826dbc68` is both
null and denylisted, so it is counted once, on the denylist arm.

The canonical census `pdata_map_audit.py` already guards correctly. So the build path and the headline census
were never wrong.

### What *was* wrong: four consumers, all off the build path

Four tools mishandled a `null`. Each is fixed with the minimal guard, and each guard was proven load-bearing
by a **must-fail control** (delete the guard → the tool dies; restore it → rc=0):

| tool | defect | fix |
|---|---|---|
| `scripts/mangle_backref_scan.py` | `norm(None)` → `TypeError` | `if v is None: continue` |
| `scripts/signature_mismatch_scan.py` | same | same |
| `tools/ec2_neighbourhood_attribution.py` | buckets all 109 addresses under a single `None` "name" | skip `None` before `rev[n].append(va)` |
| `tools/eh_prefix_map_audit.py` | counts nulls in its "map rows" denominator | `isinstance(v, str)` filter |

`eh_prefix_map_audit.py` verified: `map rows` 29,373 → **29,264** (exactly −109), `EH-PREFIX DEFECT rows: 0`
unchanged. So no census was counting nulls as *named* rows in a way that changed a verdict; the denominator
was inflated and the defect count was not.

**Not fixed, deliberately out of scope:** both scan scripts hardcode
`ROOT = /home/free/tmp/wt-residue`, a dead lane worktree, so they die at `config/45410914/splits.txt`
*before* reaching the map loop at all. That is a separate pre-existing defect; fixing it is not this lane's
brief and it is recorded in `37f17d9a`'s message.

### The constructive half — naming `0x823c3ac8`, and the wrong name it exposed

Having proven `0x823c3ac8`'s identity for Item 1, I converted its tombstone to the **witnessed** spelling
(guarded: the name must exist in our compiled COMDAT at `fn_size 100`, and the JSON must round-trip
byte-identically or the edit is refused).

**My prediction failed, and the failure was the valuable part.** I predicted **+1 fn / +100 B**. Measured:
+100 B crossed, **−116 B fell out**, net **−16 B**. The row that fell out was
`??$?0U?$_List_iterator@UPresetOverride@WorldDir@@…` (116 B), the `list<PresetOverride>` **range
constructor**.

My first explanation — a shared 16-byte-element ICF fold — was **refuted by the compiler**:
`sizeof(PresetOverride) = 24`, `sizeof(BitmapOverride) = 24`, `sizeof(MatOverride) = 36`,
`sizeof(ConstraintSystem) = 16`.

`run_diff_inspect mode=mismatches` gave the true mechanism: 1 of 29 instructions charged, a `diff_arg` `[sym]`:

```
| 22 | diff_arg | bl ?insert@?$list@UConstraintSystem@CharBlendBone@@… | bl ?insert@?$list@UPresetOverride@WorldDir@@… |
```

That shape is *identical* to an ICF fold-alias, and CLAUDE.md warns it is exactly what a fold looks like.
**It was not a fold.** Adjudicated on bytes:

- Our two `insert` COMDATs are shape-identical and differ in exactly one relocation **target name**:
  `_M_create_node@list<ConstraintSystem>` vs `_M_create_node@list<PresetOverride>`.
- Those two creators differ in an **immediate**: `li r3,24` vs `li r3,32`.
- `/OPT:ICF` folds only COMDATs identical *including relocations*. Different immediates ⇒ the creators cannot
  fold ⇒ the two `insert`s cannot fold. **An alias here would have been fabricated forgiveness** — and a
  fabricated alias lifts `name_check` *by construction*, with the `none` control flat, so the metric could
  never have caught it.

So the charge was real, and it was pointing at a **pre-existing wrong map name**:

- Retail `0x823c4188` (116 B) has its `bl` at `+0x58` targeting `0x823c3ac8` =
  `insert<ConstraintSystem>`. A function that inserts `PresetOverride`s cannot branch to a creator that
  allocates 24-byte nodes.
- Enumerating `.pdata` over CharBlendBone's pins: **exactly one** 116-byte range ctor and **exactly one**
  100-byte `insert` exist there, and the insert's creator allocates `8+16`.
- Our obj defines **four** shape-identical 116-byte range ctors (Bitmap / Constraint / Mat / PresetOverride)
  differing *only* in that `bl insert<T>` reloc — so the name is decided entirely by the callee, and the
  ConstraintSystem spelling **is** defined by our obj (so the rename is pairable, not permanently 0%).

**Why the wrong name had survived:** while `0x823c3ac8` was anonymous, `name_check` **forgave** that call
site (placeholder target `fn_823C3AC8`), so the wrong spelling scored a clean 100. *A wrong name is financed
by its callers.* Naming the callee converted a forgiven site into a checked one and exposed it. This is the
documented payout of naming an anonymous address: **bug exposure, not bytes**.

### Prediction vs measurement (after the rename)

Predicted **+100 B, Δfns +1** — the 116-byte row returns at 100 under its corrected name, and the newly named
`insert` crosses. **Measured exactly +100 B, Δfns +1:**

```
CROSSED IN : 2 rows, 216 B
   +    116 B  default/CharBlendBone::??$?0U?$_List_iterator@UConstraintSystem@CharBlendBone@@…
   +    100 B  default/CharBlendBone::?insert@?$list@UConstraintSystem@CharBlendBone@@…
FELL OUT   : 1 rows, 116 B
   -    116 B  default/CharBlendBone::??$?0U?$_List_iterator@UPresetOverride@WorldDir@@…
NET bytes  : +100
  matched_functions      43291 -> 43292   delta +1
  matched_code           3986972 -> 3987072   delta +100
```

Map name injectivity unchanged: 2 duplicate names before and after, both pre-existing and unrelated
(`?NodeCmp@@YAHPBX0@Z`, `??$__destroy_aux@ULevelData@@…`).

**Not done:** `list<PresetOverride>`'s real range constructor lives at some other retail address and remains
anonymous. Retiring a wrong claim does not oblige placing the right one, and I had no byte evidence for its
address.

---

## Item 3 — `fn_82654440` (76 B, `default/SessionUsersProviders`): **NO WITNESS. Left anonymous.**

### What I searched, and what each search returned

| witness source | result |
|---|---|
| `lookup_rb3wii SessionUsersProvider` | Class exists (`SessionUsersProviders.{h,cpp}`). Read in full: methods are `GetUser`, `KickPlayer`×2, `ToggleMuteStatus`, `RefreshUserList`, `IsMuted`, `InitData`, `Text`, `Mat`, `NumData`, `Handle`. **No Gamercard method of any kind.** Expected — Gamercard is an Xbox Live concept and rb3-Wii is the Wii dev build. |
| `grep -i gamercard` over all of rb3-Wii `src/` | Only `OvershellSlotState.h` (`kState_GamercardUsers`), `OvershellSlot.cpp` (`show_gamercard_users`), and Symbol declarations. **`ShowGamercard` appears nowhere in rb3-Wii.** |
| retail `.rdata` / RTTI | The **only** occurrence of `SessionUsersProvider` in `band.exe` is the RTTI type descriptor `.?AVSessionUsersProvider@@` in `.data`. That is a **class** name. MSVC release binaries carry no method-name strings, so the binary is structurally incapable of witnessing a method spelling. |
| retail Gamercard handler strings | `view_gamercard`, `has_viewable_gamercard` (`0x820ad625`); `view_user_gamercard`, `show_gamercard_users`, `update_gamercard_users_list` (`0x820b6593`, `0x820b6880`). **All accounted for by other classes**: the map already names `?ViewGamercard@SetlistRecord@@QAAXPAVLocalBandUser@@@Z` (`0x825badc0`), `?HasViewableGamercard@SetlistRecord@@QBA_NXZ` (`0x825bad60`), `?UpdateGamercardUsersList@OvershellSlot@@QAAXXZ` (`0x825ded28`); `view_user_gamercard` is OvershellSlot's. **None belongs to `SessionUsersProvider`** — consistent with rb3-Wii, whose `SessionUsersProvider::Handle` dispatches only `kick_player`, `toggle_mute_status`, `get_size`. |
| the caller's handler symbol | The caller is itself **anonymous** (see below), so it supplies no symbol. |
| DC3 | No `SessionUsersProvider`. `lookup_dc3 ShowGamercard` finds `Leaderboards::ShowGamercard(int, HamProfile*)` — a **different class in a different game**, with range checks and Symbol-mapped error returns our 76-byte pure tail-caller does not have. Porting that spelling is forbidden by the brief and would be fabrication. |

### A correction to W16-T's reading

W16-T records the caller as "`OvershellSlot` at `0x8249BEDC`". Measured by scanning every `b`/`bl` in
`.text` for a target of `0x82654440`: there is **exactly one** call site, at **`0x825d9034`**, inside the
function beginning at **`0x825d8fd0`** (112 B). The *class* attribution is right — `0x825d8fd0` falls inside
`OvershellSlot.cpp`'s pin `0x825D8A88–0x825D944C` — but the address `0x8249BEDC` is wrong. Both
`0x825d8fd0` and `0x82654440` are unnamed in the map.

The caller's body, for the next lane:

```
lwz  r4, 0x40(r3)            ; slot number
lwz  r3, 0x38(r3)            ; mBandUserMgr
bl   ?GetUserFromSlot@BandUserMgr@@QBAPAVBandUser@@H@Z
... two vbtable-guarded virtual calls (vt+0x5c, then vt+0x1c) on the returned user ...
mr   r5, r3                  ; arg3 = that result
lwz  r3, 0xac(r30)           ; mGamercardUsersProvider   (our header: +0xac)
mr   r4, r29                 ; arg2 = the incoming int
bl   fn_82654440             ; result returned unchanged by the caller
```

So the shape is `SessionUsersProvider::???(int index, <user-ish> *)`, and the caller returns its result
unchanged.

### Why I refuse to name it

Two elements would have to be invented, not derived:

1. **The method name.** Every Gamercard handler symbol in the binary resolves to a different class. The
   convention `view_gamercard` → `SetlistRecord::ViewGamercard` is real but it is a convention, and applying
   it to a class that has no such handler is an analogy, not a witness.
2. **The signature.** The arity differs from every witnessed sibling (ours takes an extra `int` index), and
   the **return type is genuinely ambiguous**: `fn_82654440` *tail-calls* `PlatformMgr::ShowGamercard`, which
   returns `ShowGamercardResult`, and its single caller also returns that value unchanged — but a `void`
   function ending in a tail call is byte-identical. Nothing in the bytes distinguishes
   `void` from `ShowGamercardResult`, and the mangled name encodes it.

An invented mangled name is charged by `name_check` at every call site **and** is read by the next lane as
evidence. That is worse than 76 B. Per the brief: **no witness exists, so it stays anonymous.** The reading
is complete; only the spelling is missing. An escalation lane that can pin the name needs nothing else.

---

## NOT done, with reasons

- **`fn_82654440` not named** — no witness; see Item 3. 76 B left on the table deliberately.
- **`0x825d8fd0` (112 B) not named** — the OvershellSlot caller is anonymous too, and naming it has the same
  fabrication problem plus an unpriced `name_check` exposure at its own call sites.
- **`list<PresetOverride>`'s real range ctor not placed** — no byte evidence for its address; retiring a
  wrong claim does not oblige placing the right one.
- **No alias group added at `0x823c3ac8`** — the fold is *disproven* (`li r3,24` vs `li r3,32`). An alias
  would have recovered 116 B "for free" and been fabricated forgiveness that neither ruler could catch.
- **`ROOT = /home/free/tmp/wt-residue` hardcoding in the two scan scripts not fixed** — pre-existing, out of
  this brief's scope, recorded in `37f17d9a`.
- **`0x823c3960` (the node creator) not named** — its relocations would expose
  `MemOrPoolAlloc`/`MemOrPoolAllocSTL` and `_Copy_Construct<…>` name questions, an unpriced bet beyond this
  brief.
