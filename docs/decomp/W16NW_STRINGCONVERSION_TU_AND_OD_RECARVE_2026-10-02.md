# W16-NW — the whole StringConversion TU, and the frameless `/Od` mis-carve across the Quazal block (2026-10-02)

**Branch** `w16-nw`, on main `1bbe30628`. **Ruler** `name_check` (graded). The permuter was not run. Map names
were added only for the 26 new StringConversion rows; no alias was touched, and no other map entry was edited.

W16-NV (`docs/decomp/W16NV_STRINGCONVERSION_TRIE_DEADGATES_2026-10-02.md` §2.3) left the rest of the retail
StringConversion TU carved into pieces. This lane re-carves it, pins all of it, writes it from the retail asm, and
then screens the rest of the `/Od` Quazal block for the same defect and repairs it.

## 1. Measurements

`ab_measure` refuses a patch that touches `symbols.txt`, so (as W16-NV did) the change is measured in two legs in one
fresh `setup_worktree.sh` worktree (`~/tmp/wt-w16-nw-ab`) at main `1bbe30628`.

### 1.1 Leg 0: main → main + this branch's `symbols.txt` (both carve repairs)

Hand-measured: settled builds (the settle build did zero compile/split work), `report.json` + `report.cache` wiped
before each read.

**Prediction:** Δmatched 0, Δmatched_code 0 (every touched row is in an unpaired `auto_*` unit, the map-only
SecureStream scaffold, or an already-unpaired StringConversion fragment); `total_functions` −7 (90 symbols removed,
83 added); `total_code` "small, either sign".

| | main | + symbols.txt | Δ |
|---|---:|---:|---:|
| matched_functions | 51,559 | 51,559 | **0** |
| masked_equal_functions | 24,645 | 24,645 | 0 |
| matched_code | 5,636,332 | 5,636,332 | **0** |
| total_functions | 68,921 | 68,914 | **−7** |
| total_code | 10,247,368 | 10,247,792 | **+424** |
| matched_code_percent | 55.002730 | 55.000454 | −0.002276 pp (denominator only) |

Row level (keyed by row name): **0 up, 0 down**; 90 rows vanish (5,976 B, **every one at fuzzy 0 and mpn 0**),
83 appear (5,080 B), 77 resized (+1,320 B). The +424 B is code that was in no symbol before (an unsymbolized `/Od`
switch function at `0x82A9F8E8` and two orphan leaves, §3.4) minus trailing pad words the old fragment sizes billed.
The `total_code` sign was not predicted, only its smallness.

### 1.2 Leg 1: `ab_measure --patch` (pin, `objects.json`, source, map) on the leg-0 base

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-nw-ab --patch ~/tmp/w16nw/ab.patch`, objdiff-cli
`sha256:c1b7d952` stable across legs, run dir `~/tmp/wt-w16-nw-ab/.ab_measure_runs/20261002-193000-ab-2382421/`.

**Prediction, written before the run:** Δmatched +23 (24 rows at mpn 100 minus `Latin1ToUtf8`, already matched),
Δmatched_code +4,148 B (4,424 − 276), Δmasked_equal 0, 0 rows down; StringConversion leaves the at-100 unit list.

```
leg A: matched=51559 masked=24645 honest=26914 code%=55.000454  (recompiles: 0, settled)
leg B: matched=51582 masked=24645 honest=26937 code%=55.040930  (recompiles: 1, split=1, patch_steps=7, settle iterations: 2)
split fixed point: leg A converged after 0 extra re-split(s), leg B after 0
Δmatched=+23  Δmasked_equal=+0  Δhonest=+23  Δcode%=+0.040476pp  Δcode_bytes=+4148
Δfuzzy=+0.046226pp   (legA 60.427418 -> legB 60.473644)
units at 100% [mpn ruler]: legA 490 -> legB 489  (StringConversion fell off: matched 1->24, rows 1->27)
```

**Measured: +23 / +4,148 B / masked 0, exactly as predicted.** Row level on the archived legs: 0 up, **0 down**; the
26 vanish/appear pairs are the same 26 rows renamed from `fn_` placeholders (all at 0 in leg A) to their mapped
names, with identical byte totals. `total_functions` and `total_code` do not move in this leg.

### 1.3 Whole branch against main, and what went down

matched_functions 51,559 → 51,582 (**+23**), honest +23, matched_code 5,636,332 → 5,640,480 (**+4,148 B**),
`total_functions` −7, `total_code` +424 B.

**No row goes down in either leg.** One *unit* goes down: StringConversion was 1/1 at 100% on main and is 24/27 now,
because the unit grew from one row to the 27 retail functions and three of them stay below 100 (§2.6). That is a
unit-count effect only; no row in it lost anything.

## 2. StringConversion

### 2.1 The retail TU

The TU is `0x82AE5FA8..0x82AE73A0`: 27 functions. All five file/line arguments in it are
`.\Core\StringConversion.cpp` (`0x82184910`, `…492C`, `…4948`, `…4964`, `…4980`), the next `.rdata` strings are this
TU's `"%02X"` and `L"%02X"`, and `0x82AE73A0` is the 8-byte EH prefix of LANSessionDiscovery's first function.

| addr | size | name (ours) | fuzzy |
|---|---:|---|---:|
| 82AE5FA8 | 0x114 | `Latin1ToUtf8` (anon) | 100 |
| 82AE60C0 | 0x168 | `Utf8ToLatin1` (anon) | 100 |
| 82AE6228 | 0xAC | `GetUtf16ToUtf8BufferSize` | 100 |
| 82AE62D8 | 0x21C | `Utf16ToUtf8` | 100 |
| 82AE64F8 | 0xAC | `GetUtf8ToUtf16BufferSize` | 100 |
| 82AE65A8 | 0x27C | `Utf8ToUtf16` | 100 |
| 82AE6828 | 0x34 | `Char8_2T` | 100 |
| 82AE6860 | 0x34 | `T2Char8` | 100 |
| 82AE6898 | 0x64 | `Char8_2T(const char*, char**)` | 100 |
| 82AE6900 | 0x38 | `Utf8ToT` | 100 |
| 82AE6938 | 0x38 | `TToUtf8` | 100 |
| 82AE6970 | 0x30 | `GetTToUtf8BufferSize` | 100 |
| 82AE69A0 | 0x48 | `Char8ToWide` | 100 |
| 82AE69E8 | 0x50 | `WideToChar8` | 100 |
| 82AE6A38 | 0x11C | `Char8ToUtf8` | 95.211 |
| 82AE6B58 | 0xB4 | `WideToUtf8` | 92.444 |
| 82AE6C10 | 0xA4 | `Utf8ToWide` | 90.244 |
| 82AE6CB8 | 0x28 | `FreeWide` | 100 |
| 82AE6CE0 | 0x13C | `BufferToHexString` | 100 |
| 82AE6E20 | 0x170 | `HexStringToBuffer` | 100 |
| 82AE6F90 | 0x90 | `HexCharToNibble(char)` | 100 |
| 82AE7020 | 0xAC | `HexToBuffer(const char*, …)` | 100 |
| 82AE70D0 | 0x90 | `BufferToHex(…, char*, …)` | 100 |
| 82AE7160 | 0x88 | `HexCharToNibble(wchar_t)` | 100 |
| 82AE71E8 | 0xB4 | `HexToBuffer(const wchar_t*, …)` | 100 |
| 82AE72A0 | 0x94 | `BufferToHex(…, wchar_t*, …)` | 100 |
| 82AE7338 | 0x68 | `swprintf` (inline, COMDAT) | 100 |

**Names.** The two anon statics and the five `Char8_2T`/`T2Char8`/`Utf8ToT`/`TToUtf8`/`GetTToUtf8BufferSize`
wrappers are attested by symbol (the Wii build's symbol tables carry exactly these seven). The other 20 retail
functions have no name anywhere we hold — not in either Wii symbol table, not in DC3's map. Their names here are
descriptive and chosen by this lane; they are not retail-attested. Nothing in our tree calls them, and their few
retail callers (`0x82A8E148`, `0x82AA35F8`, `0x82AA78E4`, `0x82B4EFF4`, `0x82AEAE2C`, `0x82AF1F6C` units) are all
unwritten, so no call site is checked against these names today. The `swprintf` name is the CRT's.

### 2.2 The carve

dtk had carved the five red-zone `/Od` leaves in `0x82AE60C0..0x82AE6828` into 15 symbols, the same way W16-NV
found for `Latin1ToUtf8`: `fn_82AE6218` ended `Utf8ToLatin1` and swallowed the first six instructions of
`0x82AE6228`, and so on. From `0x82AE6828` on every symbol already had its `.pdata`-exact extent. `symbols.txt` now
has retail's five extents (0x168, 0xAC, 0x21C, 0xAC, 0x27C — each ends at its `blr`, the zero pad word excluded as
for `Latin1ToUtf8`); the pin is extended to `0x82AE73A0` and dtk back-filled the `.pdata` range
(`0x822555D8..0x82255668`). The re-split is a fixed point.

### 2.3 Flags: `/Oi-`

Retail calls `strlen` and `strcpy` out of line everywhere in the TU. Our `/Od` build with intrinsics on expanded
them inline (`GetTToUtf8BufferSize` 0%, both `Char8_2T` 0%, `HexStringToBuffer` 71%). `/Od /Oi-` is the set; with it
all of those reach 100. `/Od` alone was already the TU's flag (W16-NV).

### 2.4 Two things the CRT headers cannot express, so the TU does not include them

- **`T2Char8` calls `strncpy` with two arguments.** `0x82AE6860` stores `r3/r4/r5` to their home slots, reloads
  only `r4` (`in`) and `r3` (`out`), and calls `0x8282B238`, whose body is a real `strncpy` (copies up to `r5` bytes,
  then zero-fills). Under `/Od` every argument is reloaded from its home slot (`Utf8ToT` reloads all three), so the
  call genuinely passes no count. `strncpy` is declared locally as `char *strncpy(char *, const char *)`.
- **`swprintf` is the old two-argument inline**, emitted out of line at the end of the TU and called from the wide
  `BufferToHex`. Retail's `va_start` stores through `&ap` (`addi r11,r1,0x50; addi r10,r1,0x80; stw r10,0(r11)`),
  which is the variadic `__va_start` intrinsic. The shared `src/xdk/LIBCMT/va_list_def.h` declares
  `__va_start(va_list *, va_list)`; at `/Od` that declaration **crashes cl** (wibo: SIGSEGV), and at `/O1` it only
  warns C4392 (as it does in every TU that includes it). Declared locally as `void __va_start(va_list *, ...)` it
  compiles at `/Od` and the row is 100. `<stringapiset.h>` reaches `va_list_def.h` through `win_types.h` →
  `wchar.h`, so `MultiByteToWideChar`/`WideCharToMultiByte` are declared locally too (same C names, so the
  relocations are unchanged). The shared header was **not** changed; that would be a tree-wide edit.

### 2.5 `/Od` locals are laid out by a hash walk over their names

The only other residue was stack offsets of locals. Measured with a probe TU (`~/tmp/w16nw/probe.py`, same cl and
flags, one function of `volatile` locals):

- Slot order is a walk over the scope's symbol hash buckets; **renaming one local moves it** (`count` → `numChars`
  moved it from `-0x1c` to `-0x14` with nothing else changed).
- Within a bucket, declaration order. Across buckets, a fixed order. Single letters fall in 13 groups, walked
  `{j,m,o} {e,t} {c,d,r} {l} {k,z} {b,g} {q} {f} {s,u} {i,n} {a,x,y} {h,v} {p,w}`, confirmed by declaring a–z in both
  orders (the members of a group swap with declaration order; the groups never do).
- The **last** name in the walk gets the **lowest** address: leaf frames fill the 16-byte-rounded local area from the
  bottom, framed functions fill upward from `0x50`. Bytes pack when adjacent in the walk; a word re-aligns.
  Block-scope locals go above their enclosing scope's.

Classifying ~90 candidate names against the letter groups (two compiles each) let every function be named to
retail's layout on the first try: 9 functions went from 84–99.9 to 100 with no code change. The names are
therefore load-bearing (the source says so at the top). Examples: `Char8ToUtf8`'s `str, wide, cchWide, cbUtf8`
walk G0 → G1 → G6.5 → G9 and so land at `0x5c/0x58/0x54/0x50` like retail's; the UTF-8 lead/continuation bytes are
`c0, c1, c2`, which walk `c2, c1, c0` and so ascend in memory as retail's do.

### 2.6 The three rows below 100

`Char8ToUtf8` (95.211), `WideToUtf8` (92.444) and `Utf8ToWide` (90.244) differ only where they read code page 1.
Retail loads `cp[0]` as `lis; lwz lo(r)` but `cp[1]` as `li r11,4; lis; addi; lwzx` — an unfolded constant index.
Our build folds it into `lwz 4(r)`, and the knock-on register numbering is the rest of the diff. **The idiom occurs
at these six sites and nowhere else in the retail binary** (scan of all split asm for `li rX,const` feeding a
`lwzx` off an `@ha/@l` base), so there is no matched function to learn its spelling from. Tried, all fold
identically: `cp[1]`, an enum index, a `static const int` index, `*(cp + 1)`, `cp[true]`, `cp[(char)1]`, `cp[1u]`,
`cp[(unsigned char)1]`, `(&cp[0])[1]`, `cp[(short)1]`, `cp[1LL]`, `cp[1ULL]`, and the table as `extern []`,
`extern [2]`, `static`, `volatile`. A non-const `static int` index makes it worse (loads the index). Left at 628 B
below 100 rather than inventing a construct.

## 3. The `/Od` block screen

### 3.1 Two defects, one cause

A red-zone `/Od` leaf has no `.pdata`, and an uncalled one is not a `bl` target either, so dtk has no start to
anchor it. It then errs both ways:

- **over-carve**: a branch target inside a function becomes a "function" (`fn_82AA6E88` was one function in seven
  symbols);
- **under-carve**: a whole function following a `blr` is absorbed into its predecessor (`fn_82A6E620` was a getter
  and a setter; `fn_82A7E470` three `int64` accessors; `fn_82A9F850` five getters, after which a **switch function
  at `0x82A9F8E8` with an inline jump table was in no symbol at all**, and dtk had started a "function" inside the
  table at `0x82A9F930`).

### 3.2 Method (scripts in `~/tmp/w16nw/`)

On retail bytes (`band.exe`), keyed on `.fn` symbol names, never on the asm address column:

- `carve2.py` (over-carve): a symbol is a fragment iff it has no `.pdata` entry, is never a `bl` target, is never
  address-taken (`@ha/@l` or a data `.4byte`), and either code in its span branches into its extent from outside it,
  or it is entered by fall-through (the previous non-pad instruction is not `blr`/`bctr`/`b`).
- `carvescreen.py` (under-carve): a first-parameter home-slot store (`stw r3,0x14(r1)` / `sth …0x16` / `stb …0x17` /
  `std …0x18`) right after a `blr` and optional zero pad, inside a symbol.
- `extents.py`: retail extents by CFG walk from hard seeds (`.pdata`, `bl` targets, address-taken, first-param store
  after a terminator), following branches and consuming inline `bctr` jump tables
  (`lis r12; addi r12; lwzx; mtctr; bctr`); existing symbols that `carve2` does not flag are soft seeds, dropped if a
  walk passes them; any non-zero word left uncovered starts a new function, iterated to closure.

A first version globbed only `asm/*.s` and missed nested unit asm (`asm/network/…`); it was fixed to recurse and
re-run, and the counts did not change.

### 3.3 Controls

- On **main's** `symbols.txt`, `carve2` flags exactly the 14 StringConversion fragments and `carvescreen` the 4
  swallowed starts; on the branch both read 0.
- `extents.py` on main's `symbols.txt` reproduces the hand-derived StringConversion repair exactly (14 deleted,
  4 added, `0x82AE60C0` → 0x168, every size equal), and on the branch it is a no-op.
- Two seed rules were caught by checks before anything was applied: the first plan would have **deleted 12 genuine
  functions** (no-arg accessors after a `blr`, and `fn_82AF8A58`, a leaf after `b __restgprlr_29`) — caught because
  they were left covered by no new extent — and would have kept `0x82AC54D0`, an unreachable `/Od` dead `b` between
  two returns inside `fn_82AC5478`, as a function (caught by dtk re-splitting it on the first build).

### 3.4 Result and repair

Over the `/Od` block `0x82A6D168..0x82B54190`: **75 fragments in 42 functions** (one in a pinned unit:
`SecureStream.cpp`, `fn_82ADD828` + its `li r3,0; blr` tail `fn_82ADD87C`; the rest unpinned), **74 swallowed
starts**, 2 swallowed no-argument leaves (`0x82A8EC98`, `0x82AAB9A0`), and 3 starts in no symbol (`0x82A9F8E8`,
`0x82AABB08`, `0x82AAD318`). All other hits are unpinned `auto_*` code. The `keygen_xbox` island
(`0x82724A90..0x82725440`) is clean, and none of the source-bearing `/Od` units (DuplicatedObject, Scheduler, MD5,
KeyedChecksumAlgorithm, ChecksumAlgorithm, MemoryManager, BandwidthCounter) has a hit.

`symbols.txt`: 76 deleted, 79 added, 76 resized. dtk then added 10 `.rdata` string labels and
`jumptable_82A9F91C` (it now analyzes the new functions), and the next split is a fixed point. Afterwards both screens
read 0 over the block and `extents.py` is a no-op except for 10 symbols whose size includes a trailing zero pad
word, a convention left alone. No map-named address is touched. Metric effect is leg 0 (§1.1).

## 4. Gates

Branch builds clean through `./tools/ninja-locked` (patch-state fixed point, every declared object pairs). The added
lines and commit messages carry no port-provenance citation and no Co-Authored-By line.

Native gate: §5 (run last).

## 5. Native gate

Run last, on the code at `ed7ae5795` (only this docs edit follows it):

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

## 6. Not done

- Not merged to main.
- The three `cp[1]` rows (§2.6).
- `src/xdk/LIBCMT/va_list_def.h`'s `__va_start` prototype, which is wrong (C4392 in every TU that includes it, and a
  cl crash at `/Od`). Fixing it is a shared-header change with its own A/B.
- The `/Od` hash-walk probe is a scratch script, not a tool.
- The 10 pad-inclusive symbol sizes in the block (§3.4).

Scratch: `~/tmp/w16nw/` (`carve2.py`, `carvescreen.py`, `extents.py`, `plan.py`, `plan.json`, `probe.py`,
`classify.py`, `try.sh`, `show.sh`, `rowdiff.py`, `ab.patch`, `report_main.json`, `report_base.json`, `legA.json`,
`legB.json`, build logs `~/tmp/rb3_build_w16nw_*.log`).
