# Lane W5-C — the `FileGetBase` / `OnFileGetBase` cluster

**Branch** `w5-filegetbase` · worktree `~/tmp/wt-w5-c` · base `85af6249` (main),
which carries `a8fdfd65`.

**Result: +660 B / +6 functions, in four measured steps, three of which hit
their pre-registered Δ exactly.** Eight rows in `default/File` are now at
`fuzzy == 100`; the unit went 32 → 38 matched rows.

The alias — the third leg of the wave this lane was chartered to land — is
**PROVEN and deliberately NOT installed**, because it is measurably inert. §6.

---

## 1. W4-G's facts, re-verified

Every one was tested literally before anything was built on it.

| W4-G claim | verdict |
|---|---|
| `DataBasename` and `OnFileGetBase` are ICF-folded into one function at `0x82517120` | **CONFIRMED**, two ways (§2), and now also proven from our own COMDAT bytes (§6) |
| The `DataFunc.s` registration read is OFF BY ONE | **CONFIRMED**, control reproduced exactly (§2) |
| `FileGetBase` IS in the map at `0x825166E8` (refuting W3-A) | **CONFIRMED** — `target_symbol_map.json` names it |
| `FileGetBase` is a live 192 B near-miss at fuzzy 29.917 | **CONFIRMED** to the digit: `29.916666` |
| Retail's `OnFileGetBase` is `return FileGetBase(da->Str(1))` | **CONFIRMED** from `fn_82517120`'s bytes |
| Same shape applies to `OnFileGetPath` (`fn_825170D8` → `fn_82516550`) | **CONFIRMED** |
| "retail's body appears to inline `FileGetBaseBuf` where ours calls it out of line" | **CONFIRMED as a description, REFUTED as a mechanism.** Retail does not inline a helper — retail's source has no helper. §3 |

Two things W4-G did not say, both found here and both load-bearing:

- **`target_symbol_map.json` had `0x82517090` wrong, and the wrong name was
  reading a FALSE 100%.** §4.
- **`tools/ourside_fold_sweep.py` — the tool W4-G's handoff told this lane to
  adjudicate the alias with — has crashed on every invocation since
  2026-08-19.** §6.

---

## 2. The off-by-one, and the known-answer control

`DataRegisterFunc(name, fn)` pairs a string with a function pointer, so the
registration tables identify functions. **The two tables have different
shapes and only one of them is off by one** — which is exactly the trap.

**`File.s` is NOT off by one.** String and pointer sit in one block:

```
lis r11, lbl_82087DB4@ha ; addi r3,r1,0x50 ; addi r4,r11,lbl_82087DB4@l
bl fn_827C0728                      <- Symbol ctor
lis r11, fn_82517120@ha ; lwz r3,0x0(r3) ; addi r4,r11,fn_82517120@l
bl fn_827639C0                      <- DataRegisterFunc
```

String VAs read out of retail `band.exe` in Python (never `grep` — the shell's
`grep` is binary-blind), by parsing the PE section table and mapping VA→file
offset:

| VA | string | function |
|---|---|---|
| `0x82087DA4` | `file_get_ext` | `fn_82517168` |
| `0x82087DB4` | `file_get_base` | **`fn_82517120`** |
| `0x82087DC4` | `file_get_path` | `fn_825170D8` |

**`DataFunc.s` IS off by one**, and the asm says why rather than leaving it to
pattern-matching. Per entry the scheduler interleaves two different entries'
work:

```
... bl fn_82359F28              <- map insert for the PREVIOUS symbol
lis r11, fn_<FUNC of PREVIOUS>@ha ; lis r10, lbl_<STR of NEXT>@ha
addi r11,r11,..@l ; addi r4,r10,..@l
stw r11, 0x0(r3)                <- stores that func into the slot just inserted
addi r3,r1,0xNN ; bl fn_827C0728  <- Symbol ctor for the NEXT string
```

`stw r11, 0(r3)` writes into the slot created by the **preceding** insert, while
the string loaded beside it feeds the **next** `Symbol`. Hence: the pointer
belongs to the string loaded in the PREVIOUS block.

**Known-answer control (reproduced):**

| string | its own block stores | the NEXT block stores |
|---|---|---|
| `localize_separated_int` @`0x82108B84` | `fn_827600E8` = `?DataLocalize@@` | `fn_82760158` = **`?DataLocalizeSeparatedInt@@`** |
| `basename` @`0x821089F0` | `fn_82761A50` = `?DataFindObj@@` | **`fn_82517120`** |

The control's answer is independently in the map (`0x82760158` →
`?DataLocalizeSeparatedInt@@`), so the direction is fixed by evidence, not by
my reading of the scheduling. Applying it, `"basename"` → `fn_82517120` — the
same address `File.s` registers `"file_get_base"` to. **Two independent tables,
two different name strings, one pointer: the fold is real.**

---

## 3. What was actually wrong with `FileGetBase` — and a negative worth keeping

Retail `fn_825166E8` (192 B) computes the basename inline against the static
`lbl_82CCA2B0`, with two inline `strcpy` loops and three `strrchr` calls, and
**never calls a helper and never calls `MainThread()`.**

**Step A, run alone, made the row WORSE: 29.917 → 5.833.** This was
pre-registered as "up to ~45" and the sign was wrong, which is the most
informative measurement in the lane. Deleting the discarded `MainThread();`
turns `FileGetBase` into a leaf tail-call, so MSVC emits **12 bytes** —
`lis/addi` of the static and `b FileGetBaseBuf`, no frame at all — against a
192 B target. Two things follow:

1. The original 29.917 was largely **prologue/epilogue coincidence**, not
   agreement. A near-miss percentage is not evidence that the skeleton is right.
2. MSVC at `/O1` will **not** inline a ~40-instruction helper here. So retail's
   source cannot have been calling one, and "retail inlines `FileGetBaseBuf`"
   is the wrong mechanism for the right observation.

**DC3 is not the oracle for this file.** DC3's `File.cpp` is byte-for-byte our
shape (`static buf; MainThread(); return FileGet*Buf(...)`), so a source diff
against it shows nothing. **rb3-Wii has no `*Buf` helper at all**, and its
`FileGetName`/`FileGetDrive` bodies map 1:1 onto retail's `fn_825167A8` /
`fn_82516680`. DC3 refactored the bodies out into helpers; RB3 did not.

**rb3-Wii is not the oracle either, and retail settles it.** rb3-Wii's is
`FileGetBase(const char *file, char *base)` — two args, with `if (base == 0)
base = my_path;`. Retail's `fn_825166E8` clobbers `r4` at instruction 5 and
never reads it, so retail's is **one-arg**, matching our `File.h` declaration.
Retail bytes outrank both oracles; they disagreed with each other here.

The `MainThread();` statements are not free and cannot be made free by wrapping
them in `MILO_ASSERT`: `MILO_ASSERT(cond,line)` is `((void)(cond))` in this
build, which still *evaluates*, and `MainThread()` is
`gMainThreadID == -1 || gMainThreadID == CurrentThreadId()` where
`CurrentThreadId()` is `GetCurrentThreadId()` — an extern call MSVC cannot
elide. They had to be deleted.

---

## 4. A map defect the re-verification exposed: `0x82517090`

`target_symbol_map.json` named `0x82517090` `?OnFileGetExt@@`, and **that row
was reporting a false 100%**. It is `?OnFileGetDrive@@`:

- `fn_82517090` calls `fn_82516680`, whose body is `strchr(file,':')` +
  `strncpy` into a static = `FileGetDrive`.
- `fn_82517168` calls `fn_82516618`, which **the map already names
  `FileGetExt`** — so the true `OnFileGetExt` is `fn_82517168`.
- `File.s`'s registration order (§2) and rb3-Wii's source order agree:
  drive, path, base, ext.

It scored 100 because the only differing thing is the callee, and retail's
callee `fn_82516680` was **unnamed** ⇒ a placeholder ⇒ **forgiven** under
`name_check`. This is the textbook "a wrong callee reads 100" hazard, and the
useful generalisation is that **a 100% row is not evidence that the row is the
function you think it is.**

Map rows changed:

```
0x82517090  ?OnFileGetExt@@ -> ?OnFileGetDrive@@     (defect repair)
0x825170d8  (new)           -> ?OnFileGetPath@@
0x82517120  (new)           -> ?OnFileGetBase@@
0x82517168  (new)           -> ?OnFileGetExt@@
0x82516680  (new)           -> FileGetDrive
0x825167a8  (new)           -> FileGetName
```

⚠ **`scripts/target_symbol_map.json` is not plain `{addr: name}`** — it also
carries a `_bijection_arbitrary` **list**. A naive flat rewriter reformatted it
into a 1,073-line diff. The file's exact style is `json.dumps(m, indent=1) +
"\n"`, and every edit here asserts that round-trip is a fixed point *before*
writing, so the diff is only the rows intended.

---

## 5. Per-step predicted vs measured

All four legs settled, both sides, `functionRelocDiffs=name_check`, via
`tools/ab_measure.py --from-dirty`. Map-bearing legs forced a re-split
(`renamer_patched=1823`) and **both legs reached a `symbols.txt` fixed point**.

| # | step | predicted | measured | |
|---|---|---|---|---|
| 0 | drop `MainThread()` from `FileGetBase` alone | fuzzy 30 → ~45 | **30 → 5.833** | ✗ sign wrong, §3 |
| 1 | source: `FileGetBase` body + 3 wrapper one-liners | +192 B / +1 | **+192 B / +1** | ✓ |
| 2 | map: the 4 `OnFileGet*` rows | +216 B / +3 | **+72 B / +1** | ✗ §5.1 |
| 3 | source: `FileGetPath` / `FileGetDrive` bodies | +144 B / +2 | **+144 B / +2** | ✓ |
| 4 | source `FileGetName` + map `FileGetDrive`/`FileGetName` | +252 B / +2 | **+252 B / +2** | ✓ |
| | **lane total** | | **+660 B / +6 fns** | |

`matched` 42,627 → 42,633 · `matched_code_percent` 37.552258 → 37.558700
(+0.006442 pp) · `honest` 19,699 → 19,705 · `masked_equal` unchanged at 22,928.

### 5.1 Why step 2 missed, and what it taught

Predicted all four named rows would reach 100; only `?OnFileGetBase@@` and
`?OnFileGetExt@@` did. `?OnFileGetDrive@@`/`?OnFileGetPath@@` sat at 60.333
because **MSVC inlined our then-tiny `FileGetDrive`/`FileGetPath` wrappers into
them**, re-exposing `bl ?MainThread@@` and `bl FileGetPathBuf` where retail
emits a plain `bl fn_82516550`. The Ext/Drive repair turned one paired row into
two, so the arithmetic is exactly 144 − 72 = **+72 B**.

`?OnFileGetBase@@` reached 100 for the complementary reason: step 1 had already
made `FileGetBase` too big to inline. **The wrapper rows and the functions they
call are one coupled system** — which is precisely the "cannot be half-landed"
property W4-G flagged, appearing in a place neither of us predicted. Step 3 was
written directly off this miss and hit its prediction exactly.

---

## 6. The alias: PROVEN, and deliberately NOT installed

Naming `0x82517120` converts the `DataFunc.cpp` registration site — which
spells our `?DataBasename@@` — from a forgiven placeholder target into a
checked one against retail's `?OnFileGetBase@@`. That is the coupling the
charter warned about. **The fold is proven to T1 standard:**

```
?DataBasename@@YA?AVDataNode@@PAVDataArray@@@Z   (DataFunc.obj)
?OnFileGetBase@@YA?AVDataNode@@PAVDataArray@@@Z  (File.obj)

  size 72 B, sha256 636b3dd852dde8dc  -- IDENTICAL
  relocs [(28,'?Str@DataNode@@QBAPBDPBVDataArray@@@Z',6),
          (32,'FileGetBase',6),
          (44,'??0DataNode@@QAA@PBD@Z',6)]  -- IDENTICAL, target NAMES compared
```

Byte- *and* relocation-identical is the linker's own `/OPT:ICF` condition
applied to two of our COMDATs, and retail's body at `0x82517120` **is** that
COMDAT. Combined with §2's two registration tables, nothing about this fold is
taken on trust.

**It is nevertheless inert, and installing it would forgive nothing:**

1. `tools/ourside_fold_sweep.py` never offers it. The sweep walks existing
   `name_check` **charges**; there is no charge here to forgive.
2. `?DataInitFuncs@@` is the *only* referencer of `?DataBasename@@`, and its
   `fuzzy` is **byte-identical before and after the naming — 71.4467 both
   times**. The naming levied no charge at all.
3. Direct inspection of that function's instruction diff shows exactly **one**
   aligned row touching either spelling, and it pairs retail's
   `?OnFileGetBase@@` **symbol** against a base-side **immediate `Signed 84`**.
   An alias equates two symbols; it cannot forgive a symbol-vs-immediate
   pairing. Our `?DataBasename@@` relocation never reaches an aligned
   comparison — its instruction is inside an insert/delete region of a row
   sitting at 71.45%.
4. Even a levied charge would cost **0 B**: `matched_code` is all-or-nothing per
   row and that row is nowhere near 100.

So the honest action is to **record the proof and not touch the alias DB**.
Hand-installing a group the sanctioned gate never admitted, to forgive a charge
that does not exist, would add an ungated 1,592nd group for zero measurable
reason — the same hand edit W4-G refused for `list<AwardEntry>::insert`. The
proof above is what makes it a one-line install the moment `?DataInitFuncs@@`
realigns.

⚠ **`tools/ourside_fold_sweep.py` could not run at all.** It died with
`AttributeError: 'NoneType' object has no attribute 'lower'` on the 51
`address: None` groups added by ALIAS-REPAIR 2026-08-19 — groups whose own
evidence string says they "render into no map bucket". Pre-existing;
`symbol_aliases.json` is untouched by this lane. Fixed fail-closed (a sentinel,
not a skip, so it can only refuse more pairs, never admit more); the sweep now
completes with 682 rows / 647 ADMIT. **W4-G's handoff pointed the next lane at
a tool that could not start** — worth noting whenever a handoff names a tool.

---

## 6b. ⚠ `tools/symbols_fixpoint_guard.py` LEAVES THE TREE UN-RENAMED — gate ORDER matters

Found while running this lane's own gate list, isolated with a one-variable
control on `build/45410914/obj/File.obj`:

| step | mangled symbols in the target obj |
|---|---|
| after a full `./tools/ninja-locked` | **70** |
| after `scripts/verify_split_current.py --check` | 70 (harmless) |
| after `tools/symbols_fixpoint_guard.py` | **0** |

The guard re-runs the dtk split, which rewrites every target `.obj` in
`build/45410914/obj/` — and the **pre-compile** `obj_target_symbol_renamer` does
not re-run, so the objs revert to anonymous `fn_<addr>` symbols. That is
exactly the FOLDPROVE-2 state in which *every mangled-name lookup answers
"absent"* and any name-keyed negative result is vacuous.

It bit this lane immediately: `tools/icf_alias_finder.py --validate`, run after
the guard, **REFUSED (exit 2)** with `UNRENAMED_TARGET_OBJS`. The validator's
own vacuity guard caught it — which is the only reason this was noticed rather
than becoming a confident wrong answer.

⇒ **Run `symbols_fixpoint_guard.py` LAST, or follow it with
`rm build/45410914/target_objs_renamed_checked.stamp && touch
config/45410914/config.yml && ./tools/ninja-locked`.** Two things that do NOT
protect you:

- `verify_objs_patched.py --check` / `--verify-manifest` pass happily in the
  degraded state; they cover the six **post-compile** passes on decomp objs, not
  the **pre-compile** renamer on target objs. (In this lane's batch the manifest
  check merely happened to run *before* the guard.)
- The guard prints `SPLIT ran`, which reads as a reassurance. It is the tell.

The re-split is byte-neutral once the renamer runs again — `tree_sha256`
returned to `55e5d6afae2487d1` and `report.json` to the identical
`42646 / 3,850,120 / 37.576970%` — so nothing here invalidates a measurement
taken *before* the guard, which is where all of §5's readings were taken
(target objs rewritten 06:39:59, `report.json` written 06:39:27).

## 7. What this lane did NOT do

- ⛔ **Did not install any alias group**, and did not edit
  `scripts/symbol_aliases.json` at all. §6 — proven but inert, with the reason
  measured rather than argued.
- ⛔ **Did not name `0x82516550` (`FileGetPath`).** dtk carves retail's
  `FileGetPath` into **three** symbols — `fn_82516550` (0x34) falls through into
  `fn_82516584` (0x74) and branches to the shared tail `fn_82516604` (0x14) — so
  a name on the first would score 52 B of target against our whole function.
  That is a **carving** question, not a source one. Our `FileGetPath` body is
  already retail's shape, so the source half is done and waiting.
- ⛔ **Did not chase `?DataInitFuncs@@` (8,068 B, fuzzy 71.45).** It is the
  gate on the alias becoming live and on any further `DataFunc` registration
  work, but it is a different lane.
- ⚠ **Did not delete `FileGetBaseBuf` / `FileGetPathBuf` / `FileGetDriveBuf`.**
  `FileGetBaseBuf` now has zero callers. Retail's `File.obj` contains no
  out-of-line `*Buf` helper for any of the four — but an unreferenced COMDAT is
  exactly what `/OPT:REF` strips, so retail's *source* may well still have had
  them. Leaving them is the weaker, safer claim; they cost nothing (a base-only
  symbol does not score) and removing them is a native-visible API change.
- ⚠ **Did not re-examine the other `MainThread();` sites** in `File.cpp`
  (lines ~519, ~601, ~791 and `NewFile`). The four in this cluster are
  retail-proven absent; the rest are unexamined and must not be assumed.

## 8. Handoffs

| # | handoff | evidence in hand |
|---|---|---|
| H1 | `0x82516550` `FileGetPath` needs a **splits/carving** fix, not source | §7 — three dtk symbols, sizes quoted; our body already matches retail's shape |
| H2 | The `?DataBasename@@` ≡ `?OnFileGetBase@@` alias, ready to install | §6 — COMDAT sha + relocs, two registration tables; install when `?DataInitFuncs@@` realigns |
| H3 | `?DataInitFuncs@@` 8,068 B at fuzzy 71.45, badly misaligned | §6 — a symbol operand pairs against an immediate; gates H2 |
| H4 | The remaining `MainThread();` sites in `File.cpp` are unaudited | §7 — the four audited ones were all spurious, so the prior is that more are |
| H5 | `target_symbol_map.json` carries a non-`{addr:name}` key | §4 — `_bijection_arbitrary`; assert the `indent=1` round-trip before writing |
| H6 | `symbols_fixpoint_guard.py` degrades the tree for every name-keyed instrument | §6b — isolated 70 -> 0 mangled syms; run it last or rebuild after |
