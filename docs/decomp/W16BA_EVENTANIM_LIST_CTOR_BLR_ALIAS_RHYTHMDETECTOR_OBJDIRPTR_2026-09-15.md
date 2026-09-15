# W16-BA — EventAnim `list<EventCall>` range ctor re-home, the `blr` fold alias, and RhythmDetector's `ObjDirPtr<ObjectDir>` rows

Lane W16-BA. Worktree `~/tmp/wt-w16-ba`, branch `w16-ba`, based on main at `9f7c571a0204`
(baseline measures from its parent `8ecff1e77fdf`, roadmap-only diff).
Ruler `functionRelocDiffs=name_check` (read from `report.json`'s `provenance.diff_config`,
not assumed), objdiff `a5f0ea903ec1` / binary hash `5a51cd51fe0a353f`.

## Headline

| | matched_functions | matched_code | matched_code_percent |
|---|---|---|---|
| baseline `8ecff1e77fdf` | 43,489 | 4,033,120 B | 39.362900 % |
| **after this lane** | **43,494** | **4,034,132 B** | **39.372738 %** |
| delta | **+5** | **+1,012 B** | +0.009838 pp |

`total_code` unchanged at **10,246,004**, `total_functions` 69,217. FELL OUT **0 rows** on
every one of the three measured commits. Every figure below is a set-diff of the
`fuzzy==100` row set (`tools/rowset_snapshot.py`), taken inside this worktree after a
full `./tools/ninja-locked` with rc=0 — never an incremental or single-`.obj` build.

| item | commit | predicted | measured |
|---|---|---|---|
| AX-1 re-home + rename | `bbc214f8` | +116 B | **+1 fn / +116 B** (exact) |
| AX-2 `blr` fold alias | `b459650a` | +156 B | **+1 fn / +156 B** (exact) |
| 3 RhythmDetector `ObjDirPtr` | `a6cc14f1` | −6 rows / −344 B *if* include removed | **+3 fns / +740 B**, include KEPT |
| 4 `0x824C95F0–0x824C97D8` | — (no file change) | identify only | identified; **proposed, not moved** |

Branch tip: **`a6cc14f1`**.

---

## Item AX-1 — `0x824c97d8` is EventAnim's `list<EventCall>` range ctor (`bbc214f8`)

Predicted +116 B; measured **+1 fn / +116 B, exact**.

AX's premise was re-proved on raw retail bytes rather than inherited. Retail's 116-byte
body at `0x824c97d8` (file offset `0x4be5d8`) has exactly one non-save/restore `bl`, at
`+0x58`, to `0x822b5728`. Both candidate callees exist at **distinct** addresses —
`0x822b5728` = `?insert@list<EventCall@EventAnim>`, `0x824a07a0` =
`?insert@list<ProxyCall@EventTrigger>` — so they demonstrably did **not** fold, and retail
chose EventAnim's. The row was therefore misspelled as the `list<ProxyCall>` range ctor
and mis-homed to `EventTrigger.cpp:`.

**Refinement of the brief.** The brief suggested checking "node size / callees at the
site". **Node size cannot discriminate here**: the `addi r6,r29,8` at `+0x48` is the
`_List_node_base{next,prev}` value offset, which is 8 for *every* `T`, and node
allocation happens inside `insert`. The relocation target is the only discriminator.

Both halves landed in **one** commit, as AX documented (either alone is
accuracy-negative, because objdiff pairs by NAME and a target row whose base obj cannot
define that name reads 0%):
- `scripts/target_symbol_map.json`: `0x824c97d8` renamed to the 354-char
  `list<EventCall@EventAnim>` range-ctor spelling (verified injective first).
- `config/45410914/splits.txt`: `.text start:0x824C97D8 end:0x824C9878` moved from
  `EventTrigger.cpp:` to `EventAnim.cpp:` in address order.

Confirmed on our own COFF before editing: `EventAnim.obj` defines the proposed spelling
(sec 484, 164 B); `EventTrigger.obj` defines only the ProxyCall one.

**Prediction miss, chased rather than waved off.** I predicted EventTrigger would lose 1
row / 116 B; it lost **2 rows / 156 B**. The second row is `fn_824C984C`, the ctor's EH
funclet (`__unwind$75879` at `+0x7c` of the 164 B COMDAT), so the move was *mandatory*
rather than optional — and that row is metric-neutral (mpn 100 / fuzzy 99.5 on both
sides), which is why the net came out at exactly the predicted +116 B.

`.pdata` was **never hand-edited**. dtk re-derived `.pdata start:0x82213E08
end:0x82213E18` from EventTrigger to EventAnim by itself, and the documented
re-derivation cycle reproduced exactly: first build rc=1 with `[split-guard] THE SPLIT
REWROTE ITS OWN INPUT`, second build is the fixed point, third does zero work.

## Item AX-2 — the `blr` fold is a MEMBERSHIP, not a new group (`b459650a`)

Predicted +156 B; measured **+1 fn / +156 B, exact**.

**Correction to the brief:** the alias group **already exists** at `0x826c3888` (survivor
= `StlNodeAlloc<_List_node<int>>`'s ctor, 8 folded members). This was a **9th
membership**, not a new group — which matters, because installing a new group would have
duplicated an existing survivor. Added in sorted position (index 8):
`?get_allocator@?$_List_base@VEventCall@EventAnim@@…`.

The byte proof was reproduced, not inherited, in three legs:

1. **Call-site role.** Retail's `addi r3,r1,0x50` → charged `bl 0x826c3888` →
   `mr r6,r3; …; addi r3,r1,0x58` → `bl 0x824c97d8` is STLport's
   `list<T> __tmp(first, last, this->get_allocator())` verbatim.
2. **Exhaustion.** Of **26,954** distinct in-`.text` `bl` targets, exactly **4** begin
   with a bare `blr`. Three are excluded: `0x82516320` has a `.pdata` BeginAddress and
   the 8-byte EH prefix `82829530 82087b70` (⇒ different `.xdata`, cannot fold), and
   `0x82aadf90` / `0x82aadf98` are in the measured Quazal `/Od` band and non-COMDAT. So
   an EH-free empty-leaf COMDAT from an HMX `/Gy` TU can fold **only** at `0x826c3888`.
3. **Blast radius measured.** Exactly **one** relocation site across all 1,051 declared
   objs, in `EventAnim.obj`.

**Safety argument.** Both bodies are a bare `blr` — they do nothing — so unlike the
`MemAlloc` / `_MemAllocTemp` case this alias cannot conceal a behavioural divergence.

⚠ **Negative result worth keeping.** My first fold instrument was the wrong population:
counting *map-named addresses whose body starts with a bare `blr`* gives **14** in HMX
code, which *looks* like a refutation of ICF. The correct population is distinct in-`.text`
`bl` **targets**. A census over the wrong population here produces a confident,
wrong refutation.

Corrected briefed figure: "18 objs" is **18 on disk / 17 declared** — the 18th is
`StreamRecorder.obj`.

`python3 tools/icf_alias_finder.py --validate` → **PASS, 0 contradicted**.

⚠ **Landing hazard, flagged for the coordinator.** This group's own evidence records that
a prior rebase resolver **replaced the whole group** instead of three-way merging it. The
landing tool must **append** to `folded`/`evidence`, never replace the group object.

## Item 3 — the brief's premise is REFUTED; the include stays; a map defect was found instead (`a6cc14f1`)

The brief asked me to replace the `band3/game/DirectInstrument.cpp` scatter-include in
`src/system/hamobj/RhythmDetector.cpp:929` with a genuine `ObjDirPtr<ObjectDir>`
instantiation, on the premise that *"retail placing them inside RhythmDetector's span
means retail's RhythmDetector TU instantiated them itself"*.

**That premise does not follow, and the include stays.** No source file was changed.

### Evidence against the premise

1. **The template is used binary-wide.** Retail `bl` caller counts: dtor `0x822709d8`
   **77**, `operator=` **39**, `LoadFile` **19**, `PostLoad` **16** — spread across `Dir`,
   `CharBoneDir`, `BandWardrobe`, `BandCharacter`, `VocalTrackDir`, `GameMicManager`,
   `UIPanel`, `FileMerger` and `App`. Every such TU emits the COMDAT; the linker keeps
   one copy. It landed at the head of `.text`, the COMDAT-scatter region where address
   enclosure carries **no** TU-attribution information.
2. **Our RhythmDetector carve is not a TU.** Seven discontiguous `.text` blocks from
   `0x822702F0` to `0x82667FD8`, with **`App.cpp`'s block `0x82270E68–0x822715B0` sitting
   between two of them**. That is the scatter made visible — the circular splits-pin
   hazard, where the carve is then read back as evidence for itself.
3. **Nothing RhythmDetector-typed is near the ObjDirPtr run.** The only genuinely
   RhythmDetector-typed symbol in the entire carve is
   `??$__uninitialized_copy@PAUFrame@RhythmDetector@@…` at `0x82657660` — **4 MB away**.
   (This corrects an interim claim of mine that the map named *no* RhythmDetector symbol
   at all; it names exactly this one, and it is in a *far* block, not the `0x8227xxxx` run.)
4. **The one RhythmDetector-body caller of the dtor is an EH funclet.** `fn_82271620`
   (`0x82271634` calls the dtor) decodes as
   `addi r31,r12,-592` (the MSVC X360 funclet prologue, establisher frame in r12);
   `mflr r12`; `stw r12,-8(r1)`; `stwu r1,-0x60(r1)`; `addi r3,r31,0x68`; `bl dtor`;
   epilogue. It is cleanup for *some* frame's slot at +0x68, and it is
   **reloc-masked-absent from BOTH `RhythmDetector.obj` and `DirectInstrument.obj`**, so
   its parent COMDAT belongs to a third TU entirely.
5. **Our `RhythmDetector.h` has no `ObjDirPtr<ObjectDir>` member** (size 0xc44, members
   `mTracked` 0xc … `mRecordData` 0xbf0); **DC3's `RhythmDetector` has no `ObjDirPtr` at
   all** (it uses `ObjectDir::Main()->Find<UIPanel>(…)` at lines 351/368/398/729/949);
   **rb3-Wii has no RhythmDetector**. And `ObjectDir::Find<T>` (`src/system/obj/Dir.h:540`)
   never touches `mSubDirs`, so `Find<Object>` **cannot** instantiate
   `ObjDirPtr<ObjectDir>` — which kills the only obvious mechanism.

AW's cost figure was reproduced exactly: removing the include costs **−6 rows / −344 B**
(`PostLoad` 100 + `??3DirLoader` 12 + ctor 28 + `fn_822709A8` 40 + dtor 88 + `??_G` 76 =
344 of the unit's 504 matched bytes). There is nothing genuine to replace it with, so
removing it is a pure loss.

**What would change this conclusion:** a retail RhythmDetector **member** function shown
to construct or destroy an `ObjDirPtr<ObjectDir>` (i.e. `fn_82271620`'s parent COMDAT
located and shown to be a RhythmDetector member), or oracle support for such a member in
a Milo tree closer to RB3 than DC3. Address enclosure inside our own carve will **not**
do it, and neither will DC3, which has no `ObjDirPtr` in this class at all.

⚠ Note the direction of travel: the two rows named below pair against COMDATs that exist
in `RhythmDetector.obj` **only because of the scatter-include**. The include is now
**more** load-bearing than AW measured, not less.

### What the investigation found instead: a wrong map name

Masked byte identity over the carve's unnamed rows (relocated words zeroed on **both**
sides, ≥67 % of words required unmasked, unique match required) resolved two rows against
whole COMDATs present identically in two independent objs:

| row | size | identity | evidence |
|---|---|---|---|
| `fn_82270690` | 300 B | `??4?$ObjDirPtr@VObjectDir@@@@QAAAAV0@PAVObjectDir@@@Z` | COMDAT size **exactly 300**, offset 0, **4/75** words masked |
| `fn_82270848` | 352 B | `?LoadFile@?$ObjDirPtr@VObjectDir@@@@QAAXABVFilePath@@_N1W4LoaderPos@@1@Z` | at **+0x8** of a 400 B COMDAT (8-byte EH prefix), **13/88** masked; its `__unwind$` at COMDAT +0x168 and `0x82270848+0x160 = 0x822709A8` closes exactly |

`LoadFile` was absent from the map. `operator=` was **already claimed at `0x82817ae8`**, so
both could not stand and `tools/map_name_injectivity.py` refused the build. The
relocations settle it — exactly as that tool's own option 1 says they do for byte-twin
template bodies. The two 300 B bodies are **instruction-identical** and differ in **one
word**, the `bl` at `+0x48`, where each tail-calls its own `T`'s `PostLoad`:

```
0x82270690 +0x48 -> bl 0x82270340 = ?PostLoad@?$ObjDirPtr@VObjectDir@@@@...      (map-named, 100%, RhythmDetector)
0x82817ae8 +0x48 -> bl 0x82817a68 = ?PostLoad@?$ObjDirPtr@VHamListRibbon@@@@...  (map-named, 100%, HamNavList)
```

A template's `operator=` calls its **own** `T`'s `PostLoad`, so `0x82817ae8` is
`ObjDirPtr<HamListRibbon>::operator=` and the `ObjectDir` name was on the wrong VA. These
two bodies did not fold precisely because that one relocation differs — ICF behaved
correctly.

`0x82817ae8` is therefore **nulled** (injectivity option 1: *disproved*, not merely
suspected), with the full rationale recorded in a new
`_objdirptr_objectdir_op_assign_comment` key. It is **not** renamed to the HamListRibbon
spelling, even though that name is free: `UILabel.obj` — `0x82817ae8`'s splits home —
does **not** define it, so the row would sit at a permanent 0%, whereas `HamNavList.obj`
**does** define it.

### Measured

Predicted +1…+2 rows / +300…+652 B. Measured **+3 rows / +740 B, FELL OUT 0**:

```
+ 352 B  default/RhythmDetector::?LoadFile@?$ObjDirPtr@VObjectDir@@@@QAAXABVFilePath@@_N1W4LoaderPos@@1@Z
+ 300 B  default/RhythmDetector::??4?$ObjDirPtr@VObjectDir@@@@QAAAAV0@PAVObjectDir@@@Z
+  88 B  default/UIFontImporter::??1?$ObjDirPtr@VUILabelDir@@@@UAA@XZ
matched_functions 43491 -> 43494 ; matched_code 4033392 -> 4034132
```

**The third row is a prediction miss in the favourable direction, and it is the
interesting one.** I expected nulling `0x82817ae8` to be metric-neutral, because that row
read fuzzy 99.933 / mpn 99.933 and is counted by *neither* measure. It was **+88 B**: the
wrong name was being **charged** at a call site inside an otherwise-perfect
`UIFontImporter` row. That is CLAUDE.md's *"repairing a WRONG existing map name PAYS"*
channel, measured here rather than cited.

### The two adjudications the brief asked for

**`fn_822709A8` — identified, deliberately NOT named.** It is `__unwind$` of
`ObjDirPtr<ObjectDir>::LoadFile`, proven by relocation-masked byte identity against
`__unwind$120936` at `+0x168` of sec 679 in `RhythmDetector.obj` **and** `__unwind$64088`
at `+0x168` of sec 98 in `DirectInstrument.obj`, both carrying the single relocation at
`+0x14` → `??3DirLoader@@SAXPAX@Z`. Naming it is contraindicated on two independent
grounds: it has **0 retail `bl` callers** (funclets are reached only through EH unwind
tables, never a `bl`), and it **already reads fuzzy 100 / mpn 100** by funclet byte
signature. A placeholder target is already forgiven, so naming has no call-site upside
and only downside. The brief's "name it only with the caller population checked" gate is
what settled this — the population is empty.

**`??3DirLoader@@SAXPAX@Z` — adjudicated, nothing to repair.** 12 B, already 100 %. Eleven
retail `bl` callers across `BandDirector`, `FileMerger`, `Instance`, `Dir` (×4), `UIPanel`,
`UIFontImporter` and RhythmDetector's own funclet — i.e. TU-agnostic
`DirLoader::operator delete`, pulled in wherever `utl/Loader.h`'s `DirLoader` is deleted.
Body is the standard `OBJ_MEM_OVERLOAD` shape: `mr r4,r3; li r3,0xa8; b 0x827badb0`
(`0xa8` = 168 = `sizeof(DirLoader)`). Correct as it stands.

**`fn_82270A30` and `fn_822716F8` — NOT named.** Masked identity matched them
**ambiguously** (3 and 2 candidates respectively: `__unwind$121934`/`__unwind$130778`/
`__unwind$64144`, and `??_GSkeletonCallback` vs `??_GObjRefOwner`). That is the
irreducible funclet / `??_G` class — relocation-free or near-enough that the bytes cannot
choose. `fn_82270A30` already reads mpn 100.

## Item 4 — `0x824C95F0–0x824C97D8` identified as EventAnim's; PROPOSED, not moved

The block is **one COMDAT**: `8` (EH prefix) + `388` (body) + `40` + `52` = **488 B**,
with the prefix at `0x824C95F0` = (`.text` ptr `0x82824AB0`, `.rdata` ptr `0x82088C38`).

**`fn_824C95F8` (388 B) = `??$PropSync@VEventCall@EventAnim@@@@YA_NAAV?$ObjList@VEventCall@EventAnim@@@@AAVDataNode@@PAVDataArray@@HW4PropOp@@@Z`**
— i.e. `PropSync<EventAnim::EventCall>(ObjList<EventCall>&, DataNode&, DataArray*, int, PropOp)`,
defined at `EventAnim.obj` **sec475 +0x8**. So the block belongs to **EventAnim**, not
EventTrigger.

Evidence, none of it spatial:

1. **The node-size literal.** Retail has `li r3,0x20` (32) at body `+0x0fc`, immediately
   between `bl ~T` and `bl ?MemOrPoolFreeSTL@@YAXHPAX@Z(size, ptr)`, so 32 =
   `sizeof(_List_node<T>)` = 8 + `sizeof(T)` ⇒ `sizeof(T) == 24`. `EventAnim.obj` emits
   `li r3,32` for `EventCall` nodes at every site (`_M_create_node`, `clear`, `erase`,
   `PropSync`) and **retail's word matches ours bit-for-bit at the same body offset**.
   `EventTrigger.obj`'s types give 36 (`ProxyCall`), 28 (`HideDelay`), 60 (`Anim`),
   40 (`PropTriggerDefn`) — **no T in EventTrigger has node size 32.**
2. **Funclet corroboration.** `fn_824C977C` (the 40 B funclet in the same COMDAT)
   masked-matches `__unwind$75474` at **+0x194 of that same sec475**.
3. **Neighbourhood, now byte-grounded rather than assumed.** AX-1 proved the adjacent row
   `0x824C97D8` is `list<EventCall@EventAnim>`'s range ctor, and `0x824C9430–0x824C95F0`
   is already EventAnim's.

⚠ **A near-miss that would have been a wrong answer.** A tree-wide anchored scan over
1,215 objs returned a **unique** hit on
`??$PropSync@UProxyCall@EventTrigger@@…` in `EventTrigger.obj` sec6869, body **exactly
388 B**, matching retail at **96/97 words**. Taken alone that reads as a clean
confirmation that the block is EventTrigger's. It is wrong: all `PropSync<ObjList<T>>`
instantiations are mutually masked-identical apart from relocations and the one node-size
literal, and that literal is the whole discriminator. **The single differing word was the
answer, not noise.**

⚠ **A struct-size hypothesis, raised and then refuted — do not re-open it.** The
retail-vs-ProxyCall diff read `li r3,0x20` vs `li r3,0x24`, which looks exactly like
"our `EventTrigger::ProxyCall` is 4 bytes too large (28 vs 24)". It is **not**: our
`EventTrigger.obj` uses `li r3,36` for `ProxyCall` nodes at **seven** sites, and
`_M_create_node`, `clear`, `erase` and `pop_back` for `list<ProxyCall>` **all read 100 %**
against retail. Retail therefore agrees `sizeof(ProxyCall) == 28`; our header is right.
The 0x20 belonged to a different `T` all along.

### Why it was NOT moved

- `fn_824C977C` already reads **fuzzy 100 / mpn 100** and `fn_824C97A4` reads mpn 100, both
  homed to EventTrigger. A re-home risks those 40 matched bytes.
- The named row **cannot reach 100 %** today: our `PropSync<ObjList<EventCall>>` body is
  **396 B** against retail's **388 B**.
- So the expected metric value of "re-home + name" right now is **0, with downside** — and
  re-homing an already-pinned address is explicitly not metric-neutral.
- Corrected briefed figure: `fn_824C97A4` measures **40 B**, not the briefed 52 B.

### The actionable blocker, for whoever picks this up

The 8-byte gap is **two extra instructions**, and it is a liveness difference, not a
source or struct defect:

```
retail  r+0x0c0  addi r6,r28,1        ; i+1 computed once into r6
        r+0x0c4  cmpw cr6,r6,r11      ; compared out of r6
ours    o+0x0c0  addi r28,r28,1       ; index mutated in place
        o+0x0c4  cmpw cr6,r28,r11
        o+0x120  mr   r6,r28          ; EXTRA +4 B  (before PropSync<EventCall>)
        o+0x170  mr   r6,r28          ; EXTRA +4 B  (before the second call)
```

Retail keeps `i+1` in r6 and reuses it for the compare and both call arguments; we mutate
the index and must re-materialise r6 twice. Common prefix 12 words, common suffix 6 words.
Our *ProxyCall* instantiation of the same shared template is exactly 388 B, so this is
**T-dependent register pressure**, not a template-source bug — the `fixable-liveness`
class (read `docs/decomp/patterns/fixable-liveness.md` first).

**Correct sequencing:** close the 8 B liveness gap **first** (that makes +388 B
collectable), *then* re-home `0x824C95F0–0x824C97D8` EventTrigger→EventAnim and name
`fn_824C95F8`. Doing the re-home first buys nothing and risks 40 B.

---

## Filed for another lane (outside W16-BA's concurrency bar)

**`0x82817ae8` — the complete repair.** This lane nulled the disproved name. The full fix
is a splits **re-home of `0x82817ae8` from `UILabel.cpp` to `HamNavList.cpp`** plus a
rename to `??4?$ObjDirPtr@VHamListRibbon@@@@QAAAAV0@PAVHamListRibbon@@@Z` — verified free
in the map, and **`HamNavList.obj` defines it** (as it defines the matching
`?PostLoad@?$ObjDirPtr@VHamListRibbon@@@@…`, already 100 % there). That is a 300 B row
with a real chance of 100 %. `UILabel.cpp` and `HamNavList.cpp` are splits headings this
lane does not own, so it is filed rather than done. Re-homing is not metric-neutral;
price it with an A/B.

## NOT done, and why

1. **The RhythmDetector scatter-include was NOT removed and no genuine instantiation was
   written.** The premise is refuted (§Item 3); removing it costs −6 rows / −344 B and
   there is nothing genuine to substitute. Reversal evidence is stated in §Item 3.
2. **`fn_822709A8` NOT named** — proven funclet, 0 `bl` callers, already 100 %. Naming is
   pure downside.
3. **`fn_82270A30`, `fn_822716F8` NOT named** — masked identity is ambiguous (3 and 2
   candidates); the irreducible funclet/`??_G` class.
4. **`??3DirLoader@@SAXPAX@Z` NOT touched** — adjudicated correct and already 100 %.
5. **Item 4 NOT moved and `fn_824C95F8` NOT named** — identified with byte evidence, but
   expected metric value is 0 with downside until the 8 B liveness gap closes (§Item 4).
6. **`0x82817ae8` NOT renamed and NOT re-homed** — outside this lane's bar; filed above.
7. **No `EventTrigger::ProxyCall` struct change** — the size hypothesis was refuted on our
   own 100 % rows (§Item 4).
8. **`fn_82271620`'s parent COMDAT NOT located.** It is absent from both candidate objs; a
   tree-wide indexed search for it is the one open thread that could revive the item-3
   premise.
9. **No source file was modified by this lane at all.** All three commits touch only
   `scripts/target_symbol_map.json`, `scripts/symbol_aliases.json` and
   `config/45410914/splits.txt`.

## Gates

Run in the worktree, in the prescribed order, with the native gate **last**.

```
1. full build ./tools/ninja-locked            -> rc=0   (~/tmp/rb3_build_w16ba_7.log)
2. python3 scripts/verify_ruler_agreement.py --check
3. python3 scripts/verify_objs_patched.py --verify-manifest
4. python3 tools/icf_alias_finder.py --validate
5. tools/native_build_gate.sh
```

All five passed, in order:

| # | gate | result |
|---|---|---|
| 1 | full `./tools/ninja-locked` | **rc=0** (`~/tmp/rb3_build_w16ba_8.log`) |
| 2 | `verify_ruler_agreement.py --check` | **rc=0** — `OK: both objdiff-cli entry points resolve the same ruler.` |
| 3 | `verify_objs_patched.py --verify-manifest` | **rc=0** — `1215 decomp, 3114 target objects match … tree_sha256=177e5b09e41c6094`; denylist OK (6 addresses, none named in 3,114 target objs / 495,650 symbols) |
| 4 | `tools/icf_alias_finder.py --validate` | **rc=0** — `VALIDATE: PASS -- 1399 map-consistent, 247 tolerated, 0 contradicted, 1647 total` |
| 5 | `tools/native_build_gate.sh` | **rc=0**, verbatim line below |

The native gate's own summary line, verbatim, with `skipped=0` as required:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

Group total held at **1,647** across the lane, confirming AX-2 added a *membership* to the
existing `0x826c3888` group rather than a new group.

