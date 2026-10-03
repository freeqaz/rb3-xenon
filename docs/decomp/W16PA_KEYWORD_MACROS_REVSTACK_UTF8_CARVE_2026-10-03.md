# W16-PA: the keyword-erasing header, the rev-stack spelling, and the UTF8 carve (2026-10-03)

Lane W16-PA, branch `w16-pa`, on main `c11e2ebcb` (W16-OZ). Ruler `name_check` (graded).
Brief: W16-OZ's leftovers (`W16OZ_SOURCE_DIVERGENCE_AND_IDENTIFICATION_TAIL_2026-10-03.md`
§4.4, §5). Out of scope and not touched: `src/network`, the Quazal block. The permuter was
not run.

1. `src/compiler_macros.h` defined `__declspec(x)` to nothing on every compiler but
   Metrowerks. Every compiled TU was audited for that and for any other compiler keyword
   live as a macro, and the header itself was fixed.
2. `PushRev`/`PopRev` had two spellings (static `BinStream` members and an undefined free
   function). There is now one, matching retail's single body of each, and the bodies
   pair.
3. The `UTF8ToLower`/`UTF8ToUpper` mis-carve was fixed in `symbols.txt`, measured on its
   own, and both functions now match.

Working files (not committed): `~/tmp/w16pa/` (probe, `bodymatch.py`, `adiff.py`, the
settle helper, `ab_run/` with both legs' archived reports).

## 1. Whole-binary A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-pa-ab --patch ~/tmp/w16pa/ab.patch`,
in a fresh `setup_worktree.sh` worktree at main `c11e2ebcb`. `ab_measure` refuses a patch
that touches `symbols.txt`, so leg A's base carries the carve commit (W16-NV's recipe); its
effect alone is §2. The patch is the rest of the branch (source, map, splits, the census
tool), docs excluded. objdiff-cli `sha256:c1b7d952`, stable across legs. Run dir
`~/tmp/wt-w16-pa-ab/.ab_measure_runs/20261003-080156-ab-45686/` (copied to
`~/tmp/w16pa/ab_run/`).

```
leg A: matched=53332 masked=25150 honest=28182 code%=57.475056  (recompiles: 0, settled)
leg B: matched=53349 masked=25155 honest=28194 code%=57.492424  (recompiles: 1076, split=1, settle iterations: 2)
split fixed point: both legs converged with 0 extra re-splits
Δmatched=+17  Δmasked_equal=+5  Δhonest=+12  Δcode%=+0.017368pp  Δcode_bytes=+1780
Δfuzzy=+0.017300pp   (legA 63.607784 -> legB 63.625084)
unit net: BandLeadMeter +13 (77->90), Object +2 (90->92), UTF8 +2 (16->18) = +17
units at 100% [mpn]: 533 -> 533;  [all-rows-fuzzy]: 470 -> 470
[control none] +1,860 B -- NOT_APPLICABLE (source + map + splits in patch)
```

**Prediction, written before the run:** the twelve named rows of §4.3 and §5 reach 100:
**+12 fns / +1,640 B**, plus possibly the `vector<ObjVersion>` catch funclets.
**Measured: +12 honest exactly, +1,780 B** — the prediction plus three funclets.

**Row-level diff of the archived legs, each resolved through its own map, by address:
18 rows up, 0 down, 0 gone, 0 new.** The bytes decompose exactly:

| rows | bytes | |
|---|---:|---|
| 12 named rows 0 → 100 (§4.3, §5) | 1,640 | the prediction |
| `fn_822CAD1C` 0 → 100, `fn_822C9CB0` / `fn_822CA968` 61.3 → 100 | 140 | the `vector<ObjVersion>` catch funclets, now carried by our objects |
| `fn_822CAE38`, `fn_8275CE4C` mpn 99.9 → 100 (fuzzy 99.4 → 99.5) | 0 | +2 matched, not bytes |
| `fn_8275CD54` 0 → 93.45 | 0 | rose, did not cross |

+12 named + 3 funclets + 2 mpn-only = +17 matched; the 5 funclet rows are the +5 masked.
**No row went down.**

The `HX_NATIVE` forwarders (§4.5) were committed after the A/B. They are inert for the
match build by construction; checked anyway: a settled full build of the final branch reads
53,349 / 25,155 / 57.492424% / fuzzy 63.625084, identical to leg B on every key.

## 2. The UTF8 carve, measured alone

main → main + carve commit, in the A/B worktree, both builds settled, `report.json` and
`report.cache` wiped before each read:

```
main : matched 53,332  matched_code 5,889,924  total_code 10,247,792  total_functions 68,913
carve: matched 53,332  matched_code 5,889,924  total_code 10,247,792  total_functions 68,909
```

The main reading reproduces W16-OZ's leg B exactly (53,332 / 57.475056%). Row level: the
four fragments (`fn_827CC5BC` 68 B, `fn_827CC628` 8, `fn_827CC630` 80, `fn_827CC680` 68)
leave the denominator. The two heads grow (124 → 192 B, 40 → 196 B) and stay anonymous
at 0 under BufStream. 0 rows up, 0 down. The re-split leaves `symbols.txt` at a fixed point.

## 3. Keywords redefined as macros

### 3.1 The census

`tools/keyword_macro_census.py` (new) preprocesses (`/EP`) every msvc compile edge of
`ninja -t commands all_source` behind a wrapper. The wrapper `#include`s the TU and then emits a
marker for each of 69 compiler keywords (MSVC's `__` keywords, `__attribute__`, and the
C/C++ keywords) still `#define`d at the end of the TU. It has three controls, all required
for PASS:

- `_MSC_VER` must read defined in every TU (otherwise the probe is vacuous);
- **positive**: a wrapper that does `#define __declspec(x)` before a real TU must be flagged;
- **header**: with `src/compiler_macros.h` included first, nothing may be flagged and
  `DONT_INLINE` must expand to `__declspec(noinline)`.

Measured on main + OZ (before this lane's header fix) and again on the final branch:
**1,265 TUs, 0 preprocess failures, `_MSC_VER` in all 1,265, 0 TUs with a live keyword
macro.** Only curl's `ssluse.c` and `sslgen.c` keep `true`/`false` macros. Those are
`/TC` files where MSVC C has no `bool`, so `setup_once.h`'s enum is the intended C
behaviour (allowed explicitly). 55 s at `-j 24`.

**The census fails when it should.** With the pre-fix header restored, the header check
fails and the tool exits 2: `DONT_INLINE` expands to `''`, which is the defect itself.

Limits, covered separately:

- A keyword defined and then `#undef`'d inside a TU is not seen at end of TU. A grep for
  `#undef <keyword>` finds only autoconf placeholders (`#undef const` in config headers)
  and STLport's debug-new guard (`_construct.h`).
- The dependency database (`ninja -t deps`, 1,266 objects) shows **0 TUs include
  `compiler_macros.h`**. W16-OZ removed the last three includes. `zlib/zconf.h` reaches
  10 TUs, but its `#define const` needs `STDC` undefined, and the census shows `const`
  is not a macro in any of them.
- No project macro meant to expand to `noinline` is defined empty. The `*_FORCEINLINE_*`
  hits are per-TU feature switches.

### 3.2 The root cause, fixed in the header

OZ fixed the three track TUs by dropping the include. The header stayed, so any port that
included it would erase `__declspec` again, silently. It also mapped `DONT_INLINE_CLASS` to
`__attribute__((never_inline))`, which would vanish under MSVC even with `__declspec` intact.
Now:

- the keyword-erasing block applies only to a tool that is no real compiler (MSVC,
  Metrowerks and GCC/clang keep their keywords); `DECOMP_IDE_FLAG` can no longer enable it
  under MSVC;
- an `#error` trips if `__declspec` or `__attribute__` is a macro under MSVC;
- under MSVC `DONT_INLINE` / `DONT_INLINE_CLASS` are `__declspec(noinline)` and `ALIGN` is
  `__declspec(align(x))`;
- the Metrowerks-only `DECL_SECTION` / `DECL_WEAK` are defined only under `__MWERKS__`, so a
  use elsewhere fails to compile instead of meaning something else.

Codegen effect: none, by construction (0 includers). The A/B's 1,076 recompiles are
`Object.h`/`BinStream.h` (§4), not this header.

## 4. PushRev / PopRev: one spelling

### 4.1 What retail has

- `PopRev` is **0x822CA890** (104 B), called by `ObjectDir::PostLoad` (0x827516A8) and
  `OvershellDir::PostLoad`. `PushRev` is **0x822CADD8** (96 B), called by
  `OvershellDir::PreLoad`.
- Both lie inside **BandLeadMeter.cpp's** `.text` block (0x822CA3E8..0x822CB008). BinStream's
  code is at 0x827C4xxx. A function defined out of line in BinStream.cpp cannot land in
  BandLeadMeter's span: they are header-inline COMDATs, kept from BandLeadMeter's object
  (whose `PreLoad`/`PostLoad` call both).
- Both read `sRevStack` as a **vector object** at 0x82E05C68 (`.data`, 0xC): end at +4,
  back = end − 0x10 (`ObjVersion` is 16 B, the `ObjPtr` pointer at +8, `revs` at +0xC).
  PopRev pops while `back->obj == 0` (`end = back`, then `~ObjPtr` 0x8228D498), reads `revs`,
  pops once more, returns. **There is no `o != back->obj` comparison.** PushRev builds a temp
  `ObjVersion` (`ObjPtr(0, o)` 0x8228D400, `revs` at +0xC), calls `push_back` (0x822CAD58),
  and destroys the temp.
- `sRevStack`'s atexit thunk 0x82C49CF0 calls `~vector<ObjVersion>` **0x8275CDC8**, which is
  in Object.cpp's block. The thunk sits right after DataUtl's `gMacroTable` thunk. So the
  vector is a global defined in `obj/Object.cpp`.

The *name* is not observable: an inline COMDAT in a stripped binary, and a static member
and a free function compile to the same call. What the bytes do fix is one body each, inline,
over a global object. The single spelling chosen is RB3's own declaration: free
`PushRev(int, Hmx::Object*)` / `PopRev(Hmx::Object*)`.

### 4.2 What we had

- static `BinStream::PushRev/PopRev` (mangled `SA`, so no receiver), defined out of line in
  `utl/BinStream.cpp` over a lazily `new`ed `std::vector<ObjVersion> *`;
- a free `PushRev`/`PopRev` redeclared in GemTrackDir.cpp, OvershellDir.cpp and Dir.cpp
  that **no object defined**: 6 objects referenced an undefined `?PopRev@@YAHPAVObject@Hmx@@@Z`
  and 2 an undefined `?PushRev@@…` (the match build never links, so nothing showed it).
  Dir.cpp supplied a native-only forwarder. The comment there said the member form "forces
  an `mr r3, <bs>`", which is stale: the members were already static.

Neither of our definitions could pair with retail's rows: wrong object, and out-of-line
where retail is a COMDAT. Both rows sat at 0 in BandLeadMeter, anonymous.

### 4.3 The change

- `obj/Object.h`: `extern std::vector<ObjVersion> sRevStack;` and inline free `PushRev` /
  `PopRev`; `obj/Object.cpp` defines `sRevStack`. Retail's PopRev has no mismatch report, and
  our `MILO_FAIL(...)` form evaluates its arguments (`((void)(args))`: four `PathName` calls
  and the `ClassName` vcalls), so the report is `#ifdef HX_NATIVE`. With it unguarded the
  row read 0 (mpn 2.69).
- `utl/BinStream.h` forward-declares the pair. `BinStreamRev::PushRev(obj)` keeps its
  receiver and calls `::PushRev`. The static members and BinStream.cpp's bodies are gone.
- Call sites: **65 member-spelled sites in 35 files** (34 `bs.PopRev`, 26 `bs.PushRev`,
  5 `BinStream::`) now use the free name, joining the 8 already free-spelled. One
  `bs.PushRev(this)` in `UIList::PreLoadWithRev` is a `BinStreamRev` call and keeps its
  receiver; a blind rewrite broke it, and the compiler caught it.
  (The brief's figure of 27 matches the `bs.PushRev` count alone.)
- Census after the change: `?PopRev@@` defined in 65 objects and `?PushRev@@` in 67, all
  COMDAT, none inlined. Both include BandLeadMeter.obj and Object.obj. No reference is
  left unresolved.

Ten anonymous rows were named from our own objects' symbol tables, each an **exact match on
relocation-masked bytes** against BandLeadMeter.obj / Object.obj:

| retail | size | name |
|---|---:|---|
| 0x822CA890 | 104 | `PopRev` |
| 0x822CADD8 | 96 | `PushRev` |
| 0x822CAD58 | 116 | `vector<ObjVersion>::push_back` |
| 0x822CABD0 | 324 | `vector<ObjVersion>::_M_insert_overflow_aux` |
| 0x822CAB58 | 112 | `vector<ObjVersion>::_M_clear_after_move` |
| 0x822C9BF0 | 80 | `_Destroy_Range<ObjVersion*>` |
| 0x822C9B90 | 96 | `__destroy_range_aux<reverse_iterator<ObjVersion*>>` |
| 0x822C9C48 | 96 | `__uninitialized_copy<ObjVersion*>` |
| 0x822CA900 | 96 | `__uninitialized_fill_n<ObjVersion*>` |
| 0x8275CDC8 | 132 | `~vector<ObjVersion>` (Object) |

`0x822C9BF0` matches two of our bodies (`__destroy_range` and `_Destroy_Range`, the same
bytes). It takes `_Destroy_Range`, the spelling every one of our callers (six catch funclets)
uses. None of the ten names was placed elsewhere in the map. The alias registry already
records `push_back` and `_M_clear_after_move` over `ObjVersion` as *withdrawn* from fold
groups, which fits retail keeping its own copies. All ten read 100 in-tree and in leg B.

### 4.4 Gates (rebased tip, full build)

- `tools/alias_survivor_drift.py`: **OK**, 2,083 placed groups.
- `tools/map_name_injectivity.py`: **OK**, 35,198 applied rows (35,186 + 12), injective.
- `tools/icf_alias_finder.py --validate`: **PASS**, 1,901 map-consistent, 225 tolerated,
  **0 contradicted**.

### 4.5 Engine change request (text; lanes do not edit the engine)

`../milo-native-engine/src/platform/RndTex_Native.cpp:65,73` calls `bs.PushRev(...)` and
`bs.PopRev(this)`. Requested change: call the free `PushRev(...)` / `PopRev(this)`. Until
then, `utl/BinStream.h` carries `#ifdef HX_NATIVE` static forwarders, which are inert for the
match build (§1). Remove them once the engine pin includes the change.

## 5. UTF8ToLower / UTF8ToUpper

### 5.1 The carve

dtk carved two red-zone leaves (no frame, no `.pdata`) at their own branch targets:

| function | was | is |
|---|---|---|
| `UTF8ToLower` 0x827CC540 | 0x7C + `fn_827CC5BC` 0x44 (the 3-byte tail, reached by `bge cr6`) | **0xC0** |
| `UTF8ToUpper` 0x827CC600 | 0x28 + 0x8 + 0x50 + 0x44 (`fn_827CC628/630/680`, the `blt`/`bgt`/`bge` arms) | **0xC4** |

The fragments are referenced only from inside their own function. None is a `.pdata`
BeginAddress, and no big-endian word in any non-`.text` section of `band.exe` points at
one. The heads' only callers are RndText's. Upper's last `blr` is at 0x827CC6C0 and the word
at 0x827CC6C4 is zero padding. The carve alone is §2.

### 5.2 Re-home and names

BufStream's code ends at 0x827CC540 (`??_GBufStream` at 0x827CC4F0). Its second `.text`
block (0x827CC4D0..0x827CC6C4) held both rows, and its object cannot define them. The block
now ends at 0x827CC540, and UTF8.cpp's starts there instead of at 0x827CC6C4 (`.pdata`
re-derived itself). Names lifted from UTF8.obj: `?UTF8ToLower@@YAXGPAD@Z`,
`?UTF8ToUpper@@YAXGPAD@Z`. They then paired at 50.98 / 56.90.

### 5.3 The bodies

Retail tests each range with two compares (`c >= 'A' && c <= 'Z'`, `0xC0..0xDD`,
`0xE0..0xFD`). It reassigns the `unsigned short` argument in the 2-byte arm; Upper adds
`0xFFE0` (`addis 1; subi 0x20`). It splits with signed `int % 64` (`srawi`/`addze`). Our
bodies used the subtract-and-compare range trick on an `int` copy. The results are the same
for every input. Both are now written to retail's shape: **50.98 → 100 and 56.90 → 100.**

## 6. What this lane did not do

- No alias group was added or changed.
- `fn_8275CD54` (Object, 44 B, 93.45 now) was not opened.
- The two catch-funclet rows at fuzzy 99.5 / mpn 100 (`fn_822CAE38`, `fn_8275CE4C`) were not
  opened.
- `compiler_macros.h` was not deleted; it is now safe to include.

## 7. Traps met

- **A comment can describe a mechanism that no longer exists.** Dir.cpp's note said the
  member spelling "forces an `mr r3, <bs>`". The members had been `static` for months, so
  the stated reason for the free redeclaration was void. What the free redeclaration really
  did was reference a symbol nothing defined.
- **`bs` is not always a `BinStream`.** A receiver-stripping rewrite turned a
  `BinStreamRev::PushRev(obj)` into a one-argument free call, and only the compiler caught it.
- **A placement is evidence of linkage.** A function's retail address inside another TU's
  span says *COMDAT*, which says *header-inline*. That, not the spelling, is what stopped
  either of our definitions from pairing.
- `ab_measure` deletes untracked files a patch creates when it restores (here
  `tools/keyword_macro_census.py`, in the throwaway A/B worktree only).

## 8. Native gate (run last)

`tools/native_build_gate.sh` on the final code (only this doc line follows it):
