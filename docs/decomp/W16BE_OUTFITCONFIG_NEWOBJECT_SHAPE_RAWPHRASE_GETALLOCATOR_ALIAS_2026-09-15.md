# W16-BE — `OutfitConfig::NewObject` allocation shape + the `get_allocator<RawPhrase>` alias membership

**Lane:** W16-BE (Opus). **Branch:** `w16-be`, based on main `5e852e7c1755`.
**Date:** 2026-09-15. **objdiff** 4.2.9 `5a51cd51fe0a353f`, ruler `name_check`
(read from `report.json`'s `provenance.diff_config`, which is a **LIST** of
`key=value` strings, not a dict — a `.get()` on it raises `AttributeError`).

## Headline

| measure | before (main `153a9aa659fb`) | after (`w16-be` tip) | delta |
|---|---:|---:|---:|
| `matched_functions` | 43,506 | **43,515** | **+9** |
| `matched_code` | 4,036,280 B | **4,037,072 B** | **+792 B** |
| `matched_code_percent` | 39.3937 | 39.40143 | +0.00773 |
| `fuzzy_match_percent` | 49.679333 | 49.679604 | +0.000271 |

**CROSSED IN 7 rows / 792 B. FELL OUT 0 rows / 0 B.** Item 1 = 6 rows / 672 B,
item 2 = 1 row / 120 B. `total_code` 10,246,004, `total_functions` 69,216.

Every baseline figure in the brief was re-measured against my own build before
being used; all six agreed, 0 disagreements over 40,844 baseline rows. Nothing
below is inherited arithmetic.

---

## Item 1 — `?NewObject@OutfitConfig@@SAPAVObject@Hmx@@XZ` (0x822abdd8) and the bandobj sweep

### The three briefed pre-checks, tested literally

**(i) What the macros actually expand to.** `NEW_OVERLOAD` is
`__declspec(noinline) static void *operator new(unsigned int s) { return MemAlloc(s, __FILE__, 0, "unknown", 0); }`.
`OBJ_MEM_OVERLOAD(line)` is an **inlinable** `operator new` whose body is
`(void)StaticClassName().Str(); void *mem = (MemAlloc)(s, 0); return mem;`, plus a
placement `new`, plus a `__declspec(noinline)` `operator delete`.

⚠ **Two in-tree comments are STALE and were refuted here**: `MicInputArrow.h` and
`ScrollbarDisplay.h` both claimed "the tree-wide `OBJ_MEM_OVERLOAD` is
`__declspec(noinline)`". Only its **delete** is. That false premise is why both
files carried a hand-rolled `operator new` instead of the macro. Both notes have
been rewritten as dated records.

★ The load-bearing detail, and the reason a naive flip does not work: inside
`operator new`, the `.Str()` on the **discarded** `StaticClassName()` Symbol
temporary plus a **named** `mem` local is what homes the Symbol at `0x50` and the
returned pointer separately at `0x54`. A bare `StaticClassName();` homes the temp
in the SAME slot as `mem` and yields the 0x50-vs-0x54 residual. Emitting the
out-of-line `??3` COMDAT and inlining it are **not** exclusive.

**(ii) The `li r3, <size>` immediate.** Retail is `li r3, 0xfc` = **252**, and our
compiled `?NewObject@OutfitConfig@@` now emits `386000fc` at the same offset and
the row is at fuzzy 100 — so `sizeof(OutfitConfig) == 252` is **confirmed by the
match itself**, not by a separate layout report. No independent layout defect
exists here; nothing to file.

**(iii) The ctor extent.** ⛔ **AZ's "our `??0OutfitConfig@@QAA@XZ` is 1016 B" is
REFUTED — it is 564 B**, against retail's 588 B at `fn_822AB3E0`. The sizes
disagree, so per the brief the ctor is a **separate row** and is NOT part of this
item. Untouched.

### Result

Retail's body is the **inlined shape (b)**; ours was **shape (a)** because
`OutfitConfig.h:162-163` carried the rb3-Wii-inherited `NEW_OVERLOAD; DELETE_OVERLOAD;`.
Predicted 86.929 → 100, **+112 B / +1 fn**. **Measured exactly that.**

### The bandobj `?NewObject@` sweep

Every `?NewObject@…` row in `report.json` below fuzzy 100 was examined and its
retail shape read from retail bytes. **Do NOT bulk-flip** was observed: each fix
is its own commit with its own per-class byte evidence, and the two rows whose
retail shape does not license a macro swap were left alone.

| class | retail addr | size | before | after | retail shape | action |
|---|---|---:|---:|---:|---|---|
| `OutfitConfig` | 0x822abdd8 | 112 B | 86.929 | **100** | (b) inlined | `OBJ_MEM_OVERLOAD(0xa2)` |
| `BandLabel` | 0x82341ff8 | 112 B | 86.929 | **100** | (b) inlined | `OBJ_MEM_OVERLOAD(0x1f)` |
| `StarDisplay` | 0x8231d220 | 112 B | 86.929 | **100** | (b) inlined | `operator new` ONLY |
| `AppMiniLeaderboardDisplay` | 0x8264c828 | 112 B | 99.821 | **100** | (b) inlined | `operator new` ONLY, on the base header |
| `MicInputArrow` | 0x823192d8 | 112 B | 86.929 | **100** | (b) inlined | `OBJ_MEM_OVERLOAD(0x39)` |
| `ScrollbarDisplay` | 0x82323198 | 112 B | 86.929 | **100** | (b) inlined | `OBJ_MEM_OVERLOAD(0x3a)` |
| `BandSong` | 0x8227b710 | 112 B | 99.821 | 99.821 | (b) inlined | **NOT FIXED — not a source defect** |
| `UIPanel` | 0x8268b9a0 | 76 B | 68.368 | 68.368 | **(a) out-of-line** | **NOT FIXED — outside file bar + real layout defect** |

After the sweep: **209 of 211 `?NewObject@` rows tree-wide are at fuzzy 100.**

**Why `StarDisplay` and `MiniLeaderboardDisplay` get `operator new` ONLY rather
than the macro — a MEASURED negative result, not a style choice.** Using
`OBJ_MEM_OVERLOAD_INLINE_DEL` on StarDisplay won NewObject (+112 B) but **LOST**
the NewObject unwind funclet `fn_8231D290` (−40 B, fuzzy 100 → below): a
StarDisplay-owned *inlinable* delete gets inlined into the funclet as
`bl ?MemFree@@YAXPAX@Z`, where retail calls the out-of-line ICF survivor
`??3BinStream@@SAXPAX@Z`. Leaving `operator delete` inherited from `UIComponent`
keeps both that funclet and `??_GStarDisplay` at 100 **and** still wins NewObject.
The same reasoning applies to `MiniLeaderboardDisplay`. This is why FELL OUT is 0
rather than the 3-funclet cost ObjMacros.h records as acceptable — the cost was
avoidable here, so it was avoided.

`MiniLeaderboardDisplay.h` is edited rather than the App header because
`AppMiniLeaderboardDisplay` **includes** it and inherits the operator.

### The two NOT fixed, with what would change that

- **`BandSong` (0x8227b710, 99.821).** The residual is **not** an allocation
  shape — the shape is already (b) and correct. The single charged site is
  retail's ctor target being the NAMED `??0HamSong@@QAA@XZ` (0x8227a828) where we
  spell `??0BandSong@@QAA@XZ`. `?StaticClassName@Song@@` already matches. This is
  a **fold/alias or map-identification question, not source**: no edit to
  `BandSong.h` can change which name retail's map carries. **Evidence that would
  settle it:** whether `0x8227a828` is one body that both `HamSong` and `BandSong`
  ctors ICF-folded into (⇒ an alias membership, provable on retail bytes the way
  item 2 was), or whether the map row at `0x8227a828` is simply misnamed (⇒ a map
  repair). Both are other lanes' file bars. I did not install an alias on a
  guess — a fabricated alias lifts the score by construction.
- **`UIPanel` (0x8268b9a0, 68.368).** Two independent blockers. (a) The file is
  `src/system/ui/UIPanel.h`, **outside this lane's bar**. (b) More importantly it
  is **not** this defect class at all: retail's shape is **(a) out-of-line**, and
  the row carries a **real layout divergence** — ours emits `li r3, 0x68` (104)
  where retail emits `li r3, 0x108` (264), and retail additionally calls
  `??2CriticalSection@@SAPAXI@Z`. A macro swap would not touch either. **Evidence
  that would change it:** a `UIPanel` class-layout adjudication against retail
  bytes identifying which members account for 264−104 = 160 bytes (the
  `CriticalSection` call strongly suggests an embedded member our header lacks).
  That is a struct-layout lane, and per the standing directive struct/vtable work
  is high value — I recommend it be dispatched as one.

### OutfitConfig COMDAT sizes — for lane W16-BD's masked-compare

Measured properly rather than reasoned about: both header variants compiled to the
**same** `/Fo` path with `OBJCACHE=off` (the construction `tools/gate_liveness.py`
exists to enforce — a raw `.obj` byte compare is otherwise a dead instrument
because objs embed the populating worktree's `/Fo`). 3,091 bodies on both sides.

**Only TWO real COMDATs changed:**

| symbol | before | after |
|---|---:|---:|
| `??2OutfitConfig@@SAPAXI@Z` | 8 B / 1 reloc | **60 B / 2 relocs** |
| `?NewObject@OutfitConfig@@SAPAVObject@Hmx@@XZ` | 100 B / 2 relocs | **112 B / 3 relocs** |

`??3OutfitConfig@@SAXPAX@Z` is **UNCHANGED** at 4 B / 1 reloc (a tail-jump thunk
under both spellings). `?StaticClassName@OutfitConfig@@` unchanged at 88 B.

⚠ **BD must expect 118 phantom rows.** The other 118 entries in a naive name-keyed
diff are **59 `__catch$N` EH funclets renumbered by exactly +12** — verified
programmatically: the delta histogram is `{12: 59}` with **0 unexplained**, and
the `(size, relocs)` multiset is **identical** on both sides. A compiler label
counter shifted because the new macro body introduces 12 numbered entities earlier
in the TU. None of them is a semantic change; a masked-compare keyed on `__catch$`
names will read 59 deletions + 59 insertions that are not there.

---

## Item 2 — `get_allocator<vector<RawPhrase>>` into alias group `0x826c3888`

Predicted **+1 fn / +120 B**. **Measured exactly +120 B**, row
`??0?$vector@URawPhrase@@V?$StlNodeAlloc@URawPhrase@@@stlpmtx_std@@@stlpmtx_std@@QAA@ABV01@@Z`
(retail 0x8278b8e8, 120 B, `default/system/beatmatch/PhraseAnalyzer`) crossing
99.833336 → 100. FELL OUT 0.

Group index **1543**, survivor
`??$?0H@?$StlNodeAlloc@V?$_List_node@H@stlpmtx_std@@@stlpmtx_std@@QAA@ABV?$StlNodeAlloc@H@1@@Z`,
folded **9 → 10**. Tier **FT-EMPTY**. Every leg was re-derived on retail bytes
rather than inherited from the W16-BC §4 proposal:

1. **Charged-site check FIRST, as the brief requires.** Of the row's five
   word-level diffs: idx 1 `__savegprlr_28` and idx 29 `__restgprlr_28` (retail
   `fn_82829258` / `fn_828292A8` — placeholder targets, forgiven, and additionally
   caught by the regalloc-save-helper screen); idx 17 `??0?$_Vector_base@URawPhrase…`
   (retail `fn_82B74488`, placeholder, forgiven); idx 25
   `__uninitialized_copy<PBURawPhrase>` vs retail's `<UIMesh>` spelling, which is
   **ALREADY folded in group 114 (0x827ffa50)**; idx 13 = this candidate. ⇒ this
   site is the row's **only** remaining charge, so +120 B is collectable. (This is
   the RESIDUAL-1 lesson: a row's headline prize can be uncollectable in
   principle; price it from the charged-site list, never from a mismatch count.)
2. **Call-site role, from retail's own code.** Retail's `bl 0x826c3888` at +0x34
   sits between `divw r28,r11,r10` and `mr r5,r3; mr r3,r30; mr r4,r28; bl
   _Vector_base<RawPhrase>`, all byte-equal on both sides — STLport's vector copy
   ctor feeding `__x.get_allocator()` into the 3rd argument of its base-init. The
   role is proven by retail's surrounding words; our spelling is not the evidence.
3. **Bytes.** Our `PhraseAnalyzer.obj` compiles the folded spelling to exactly 4 B
   `4e800020` with **zero** relocations; retail `0x826c3888` is `4e800020` +
   padding. **Byte-identical — and VACUOUS by construction**, as the tier says.
   Every empty body equals every other. This leg proves nothing on its own.
4. **Exhaustion, recomputed over `band.exe` `.text`.** Of **26,970** distinct
   in-`.text` `bl` targets, exactly **four** begin with a bare `blr`. I verified
   all three exclusions myself rather than inherit the W16-J / W16-BA note already
   in the field: `0x82516320` = `?SetDiskError@PlatformMgr@@` **has** a `.pdata`
   BeginAddress and the EH prefix `82829530 82087b70`, so it carries associated
   `.xdata` and cannot fold with an EH-free leaf; `0x82aadf90` and `0x82aadf98`
   are both inside the measured Quazal `/Od` band `0x82A6D168–0x82B54190` and
   non-COMDAT; `0x826c3888` itself has **no** `.pdata` BeginAddress (EH-free
   leaf). ⇒ an EH-free empty-leaf COMDAT from an HMX `/Gy` TU can fold **only** here.
5. **Blast radius, measured.** Across every compiled obj the folded spelling is
   the target of **exactly one** relocation — `PhraseAnalyzer.obj` +0x34, inside
   that same row. The forgiveness cannot leak.
6. **FT1.** The spelling has no row in `target_symbol_map.json` at all.
7. **Safety.** Both bodies are a bare `blr` and therefore do nothing, so unlike
   the `MemAlloc`/`_MemAllocTemp` case this alias cannot conceal a behavioural
   divergence. There is no behaviour to differ.

Installed by **APPEND**, never replace — the same field records the W16-J landing
repair where a rebase resolver replaced the whole group. Verified after writing:
1,650 groups before and after, group 1543 the only one changed, survivor
untouched, old memberships a strict subset, whole-file diff 3 insertions /
2 deletions (no reformat).

---

## The native gate caught a real defect of mine

My **first** gate run **FAILED**:

```
NATIVE_GATE_RESULT verdict=FAIL expected=18 verified=16 skipped=0 partial=0 failed=2 rc=1
```

16 error lines; `rb3-milo` and `rb3-render` produced no executable:

```
MiniLeaderboardDisplay.h:45:18: error: 'operator new' takes type size_t ('unsigned long') as 1st parameter
MiniLeaderboardDisplay.h:47:36: error: too few arguments to function call, expected at least 4, have 2
StarDisplay.h:62:18: error: 'operator new' takes type size_t ('unsigned long') as 1st parameter
StarDisplay.h:64:36: error: too few arguments to function call, expected at least 4, have 2
```

Both are mine, and both come from **hand-rolling** `operator new` instead of using
the macro: on LP64 native it must take `size_t`, and `MemAlloc` there is the 5-arg
debug form — the 2-arg `(MemAlloc)(s, 0)` spelling exists only for the match build,
where MemMgr.h's arg-stripping macro creates it. **The matching build is
structurally incapable of catching this**: it compiles `src/` but never links the
native superset. This is the fifth recorded time the gate has caught `main` being
broken by a matching lane — this time before landing.

Fixed by mirroring MemMgr.h's own `#ifdef HX_NATIVE` split **inside each header**;
MemMgr.h itself is another lane's file and was left untouched. I did not simply
use `OBJ_MEM_OVERLOAD`, because it also declares `operator delete` — the exact
thing that costs the funclet row.

**Match build verified unaffected, not assumed:** full build rc=0 and the
whole-rowset set-diff after the guard is identical to the pre-guard build **to the
last digit** (43,515 / 4,037,072 / 39.40143 / fuzzy 49.679604, CROSSED IN 7,
FELL OUT 0). The `#else` branch is byte-identical to the code it replaced.

### Final gate line, verbatim

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

All gates, in the briefed order: full build **rc=0** →
`verify_ruler_agreement.py --check` **OK** (both entry points resolve the same
ruler) → `verify_objs_patched.py --verify-manifest` **OK** (1,215 decomp +
3,114 target objects, `tree_sha256=67258d8b4061d9a2`) →
`icf_alias_finder.py --validate` **PASS** (1,402 map-consistent, 247 tolerated,
**0 CONTRADICTED**, 1,650 total) → native gate **PASS 18/18, skipped=0**.

---

## Commits

| sha | what |
|---|---|
| `2cdbd93f` | OutfitConfig: `NEW_OVERLOAD` → `OBJ_MEM_OVERLOAD(0xa2)` |
| `c64810d0` | BandLabel: same flip, same 86.929 signature |
| `9a050fae` | StarDisplay: own `operator new` only |
| `cf83fd5f` | MiniLeaderboardDisplay: own `operator new` only (fixes the App row) |
| `6bac186c` | MicInputArrow: `OBJ_MEM_OVERLOAD(0x39)`; NOTE(INSDEL-1) refuted |
| `c81aafb5` | ScrollbarDisplay: `OBJ_MEM_OVERLOAD(0x3a)` |
| `4505686c` | alias 0x826c3888: fold `get_allocator<vector<RawPhrase>>` |
| `a622a8c5` | StarDisplay/MiniLeaderboardDisplay: `HX_NATIVE` guard (gate repair) |

## NOT done, with reasons

- **`?NewObject@BandSong@@` (0x8227b710)** — not a source defect; retail's named
  ctor target differs. Fold/alias or map question, both outside my bar. Evidence
  that would settle it is stated above.
- **`?NewObject@UIPanel@@` (0x8268b9a0)** — outside my file bar **and** a real
  layout defect (`li r3,0x68` vs retail `li r3,0x108`, plus a `CriticalSection`
  ctor call), not an allocation-shape defect. Recommend a struct-layout lane.
- **`??0OutfitConfig@@QAA@XZ`** — 564 B ours vs 588 B retail. A separate row per
  the brief's own pre-check (iii); diagnosed only far enough to establish it is
  separate. Not touched.
- **No `splits.txt` line edited**, per the concurrency bar with W16-BD.
- **MemMgr.h / ObjMacros.h not edited** — outside the file bar, even though the
  cleanest fix for items like StarDisplay would be a new-only macro there. Filed
  as a suggestion rather than taken.
- **`??3` ICF-survivor funclet residuals not chased.** The 40 B / 44 B funclets
  sitting at mpn 100 / fuzzy 99.5 across OutfitConfig, BandLabel, MicInputArrow
  and ScrollbarDisplay are charged on the ICF `??3` survivor name. ObjMacros.h
  explicitly says not to recover these by alias or source bending, and I did not.

## One measurement caveat, stated rather than buried

`matched_functions` moved **+9** while **7** rows crossed on fuzzy. The 2-row gap
is movement to `mpn == 100` **without** reaching `fuzzy == 100` — the two rulers
are independent by construction (`mpn` excludes arg-only penalties). The candidate
rows are the NewObject unwind funclets `fn_82319348` (MicInputArrow) and
`fn_82323208` (ScrollbarDisplay), both now at mpn 100 / fuzzy 99.5 and both
belonging to classes I changed; the mechanism is that `lwz r3,0x54` now matches
while the ICF `??3` survivor name still charges at argument level.

⚠ **This attribution rests on mechanism, not on a saved set-diff**:
`tools/rowset_snapshot.py` records **fuzzy==100 membership only** and does not
snapshot the `mpn` set, so I have no baseline `mpn` set to difference against.
**Evidence that would settle it:** an `mpn`-set snapshot facility in
`rowset_snapshot.py` (or a one-off revert-those-two-headers + full-build leg).
The +9 / +792 B headline is measured directly and does not depend on this.
