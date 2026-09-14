# SetDiskError geometry · offsetof · CharClipDriver mis-pins · dump_vtable primary selection (lane W16-P, 2026-09-14)

Branch `w16-p` on main `a5097b8e`, worktree `~/tmp/wt-w16-p`. Ruler: `name_check`
(`report.json` provenance). Every number below comes from a full `./tools/ninja-locked`
whose `rc` was tested on its own line, and a whole-binary set-diff of the `fuzzy == 100`
row set (`tools/rowset_snapshot.py` against `~/tmp/w16p_rowset_baseline.json`).
Under test: the five side findings W16-M recorded but did not act on
(`docs/decomp/PLATFORMMGR_ESCALATION_2026-09-14.md` §5–§6).

Baseline (settled, `a5097b8e` + reflink build): **43,212 matched_functions /
3,938,120 matched_code / 38.43585 % / fuzzy 49.33976**, `total_functions` 69,217,
`total_code` 10,245,956.

## 0. Verdict table

| # | Claim under test (W16-M / brief) | Instrument that decided it | Verdict | bytes |
|---|---|---|---|---:|
| 1 | `fn_82516320` = `SetDiskError` is a `blr` stub whose 208 B `.pdata` extent absorbed an unrelated fragment | retail `.pdata` decode + capstone disasm + **whole-image branch-target scan** + the function's own **EH `FuncInfo` / IP-to-state map** | **CONFIRMED, and sharpened**: the extent aggregates *three* things and the absorbed code is **SetDiskError's own body**, not a stranger's | 0 |
| 1a | brief's option (a): "our body should become `{}`" | EH tables bind the orphan block to this record; our source maps 1:1 onto it; mpn 86.04 % of 52 instructions ≈ the 45 surviving ones | **REFUTED — do not empty the body** | 0 |
| 1b | brief's option (b): entry is elsewhere / `fn_825162F0` is a truncated SetDiskError | `fn_825162F0` is a 40 B **EH funclet** (`addi r31,r12,-0xb0`), with its own `.pdata` record | **REFUTED** | 0 |
| 1c | `PlatformMgr.h:125` "mDiskError @0x34 because `stw r4,0x34(r3)`" | the cited store is in unreachable code; re-derived from the **ctor** `0x8251C320` `stw r29,0x34(r30)` (reachable) | **offset CORRECT, citation REPLACED** | 0 |
| 2 | `src/xdk/LIBCMT/stddef.h:20` `offsetof` mis-parenthesised | C++ parse (`->` binds tighter than the cast) + ninja graph / `objects.json` absence of the only 2 call sites | **CONFIRMED, fixed**; no TU expands it ⇒ Δ0 predicted and measured | 0 |
| 3 | `fn_823EC788` / `fn_823EC980` mis-pinned into `CharClipDriver.cpp` | 6 independent lines incl. `.rdata 0x82057978` = `??_7XboxServer` **slot 15**, and dtk's own `.pdata` re-derivation | **CONFIRMED, re-homed to `Server.cpp`** | **−80** |
| 3b | `fn_82A87F20` also mis-pinned into `CharClipDriver.s` | grep of the emitted `.s` + splits ownership scan | **REFUTED** — it is a `bl` **target**, not a definition; pinned to no unit at all | 0 |
| 4 | `scripts/dump_vtable.py` never selects the virtual-base primary | ran the tool on both objs; reproduced on the compiled obj | **CONFIRMED, fixed** (+ `--which`, choice reporting, failing-control `--selftest`) | 0 |
| 5 | `fn_8251D378` = `PlatformMgr::Init`; name it iff all 3 callees resolve | disasm + `target_symbol_map.json` + import-table scan | **1 of 3 resolved** (`XOnlineStartupEx`) ⇒ **NOT named**; brief's `XNetStartup` hypothesis **REFUTED** | 0 |

Net for the lane: **43,212 → 43,210 matched_functions (−2), 3,938,120 → 3,938,040
matched_code (−80 B)**. Deliberately net-negative; see §3.

## 1. `fn_82516320` — what the 208 bytes actually are

`.pdata` (BIG-ENDIAN; layout `flags:2 | FunctionLength:22 | PrologLen:8`, per
`tools/pdata_extent.py`, whose `>>8` decode fits 57,732/57,732 entries):

    begin 0x82516320  packed 0xC0003404
      -> flags 3 (ThirtyTwoBit + ExceptionFlag), FunctionLength 52 instr = 208 B,
         PrologLen 4 instr = 16 B,  end 0x825163F0

The neighbours are a clean partition — `0x825162F0` (40 B) ends at `0x82516318`,
leaving exactly the 8-byte EH prefix `82829530 82087B70`, and the next record
begins exactly at `0x825163F0`. So the 208 B aggregate **originates in retail's
own unwind table**, not in `symbols.txt` and not in dtk. That much reproduces
W16-M.

What is new is the decomposition. The extent is **three** disjoint things,
1 + 6 + 45 = 52 instructions exactly:

| span | size | what it is | how that was decided |
|---|---:|---|---|
| `0x82516320` | 4 B | a bare `blr` | disasm; and **8 `bl` sites** in the image target it — every one loading `r3 = 0x82CC9D1C` (a fixed global) and `r4 = 1` or `3` (a small enum), i.e. the call shape of `Mgr->SetDiskError(err)` |
| `0x82516324–0x82516338` | 24 B | an **out-of-line fragment of `DataSet`** (`0x8275D670`, 2.4 MB away) | mutual branches: `DataSet+0x70` is `b 0x82516324`, and the fragment branches back to `0x8275D6F0` and `0x8275D6E4`. It also consumes `r30`, which `DataSet` sets at `0x8275D688` |
| `0x8251633C–0x825163EC` | 180 B | **SetDiskError's own body tail** | see below |

### The body tail is SetDiskError's, and it is unreachable

Two independent instruments agree it belongs to the `0x82516320` record:

* The EH prefix at `0x82516318` points at `FuncInfo` `0x82087B70`: magic
  `0x19930522`, `maxState 1`, `EHFlags 1` (`FI_EHS_FLAG`), `nIPMapEntries 3`,
  `pIPtoStateMap 0x82087B98`. That map is
  `{0x82516378, 0}, {0x825163B0, -1}, {0x825163EC, 0}` — **all three IPs are inside
  the orphan block**, bracketing the static-construction region. Its unwind action
  (`UnwindMapEntry` at `0x82087B68`) is `0x825163F0`, the funclet that clears bit 0
  of the guard word at `0x82CC9D18` — the same guard the block *sets* at
  `0x8251636C`.
* The block's epilogue `addi r1,r31,0x80; b 0x828292AC` and frame size `0x80` are
  the same form `DataSet` uses, and a 4-instruction prologue is exactly what this
  record's `PrologLen` claims.

And it cannot be entered. A scan of **all 2,571,663 `.text` words** for `b`/`bl`
(op 18) and `bc` (op 16) whose target lands in `[0x82516320, 0x825163F0)` returns
**9 branches: 8 `bl` to `0x82516320` and 1 `b` to `0x82516324`**. Nothing anywhere
in the image branches to `0x8251633C`, and it cannot be fallen into (`0x82516338`
is an unconditional `b`). It is not an EH funclet either — the only funclet the
tables name is `0x825163F0`.

⇒ **Caller-visible behaviour of retail `PlatformMgr::SetDiskError` is "return
immediately".** Its body still exists, still has live EH metadata, and is dead.

### The missing head is 7 instructions, and our source is right

`0x8251633C - 0x82516320 = 7` instructions, of which the record says 4 are
prologue. The body tail opens `beq cr6, <epilogue>` / `cmpw cr6,r11,r4` /
`beq cr6, <epilogue>` / `stw r4,0x34(r3)` with `r3`,`r4` still holding the
incoming arguments — so the 3 missing body instructions are a prologue plus
`lwz r11,0x34(r3)` and a compare. Our `src/system/os/PlatformMgr.cpp:155` maps
onto the survivors one-for-one:

| source | retail |
|---|---|
| `if (mDiskError == kFailedChecksum \|\| mDiskError == derr) return;` | the two `beq cr6` at `0x8251633C` / `0x82516344` |
| `mDiskError = derr;` | `stw r4,0x34(r3)` |
| `if (mDiskError != kNoDiskError) { static DiskErrorMsg msg; Handle(msg,false); }` | `cmpwi cr6,r4,0`; guard test; `bl 0x82514FF0` (`??0DiskErrorMsg`); `bl atexit`; the `bctrl` vcall |
| `func = GetDiskErrorCallback(); if (func) func();` | `bl 0x8251B860`; `cmplwi`; `mtctr`; `bctrl` |
| `while (true) { … Sleep(1); }` | `li r3,1; bl 0x8283D660; b` back to itself |

The row scores **fuzzy 85.173 / mpn 86.038** on 208 B. 86.04 % of 52 instructions
is 44.7 ≈ the 45 surviving ones. **The row is at its structural ceiling**: the
target's first 28 bytes are a `blr` plus another function's fragment, and no C++
source can emit that. Emptying our body (the brief's option (a)) would fit our
source to a link-time artifact, destroy a correct body, and drive the row *down*
from 85.2 to near zero, so it was not done.

### Mechanism: NOT settled — and it is unique

A whole-binary census over all **57,733** `.pdata` records: exactly **one** record
in the entire image has `blr` as its first instruction, and it is this one
(`0x82516320`, and it is also the only such record claiming `PrologLen > 0` or
`FunctionLength > 8`). So this is **not** a systematic linker behaviour that other
rows could be re-read through; it is a singular incident.

What would settle it, for a later lane: compare against `default_patched.xex` and
`default_tu5.xex` (is the `blr` present in all three images?); and check whether
DC3's build carries the same TU with a live head. What is *excluded* already:
a predecessor over-carve (`0x825162F0` is a self-contained 40 B EH funclet with
its own `.pdata` record), and a mis-carve by dtk or `symbols.txt` (both mirror
retail `.pdata`).

### Actions taken for item 1

* **No re-carve.** Splitting the extent would diverge from retail's own unwind
  table, would drop the row's 208 B to a 4 B `blr` row our source cannot produce,
  and buys nothing — `matched_code` counts only `fuzzy == 100` rows and this row
  is not one under either carve. Accuracy argues for keeping retail's extent.
* **No source change** to `PlatformMgr.cpp` (see above).
* **`PlatformMgr.h:125` comment rewritten** to lead with the constructor
  (`0x8251C320`, `stw r29,0x34(r30)`, inside its member-zeroing run `0x1c`–`0x44`)
  and to flag the old `SetDiskError` citation as dead-code-only. The offset 0x34
  itself is unchanged and is correct.

## 2. `offsetof`

`#define offsetof(T, mem) ((int)&((T *)0->mem))` parses as `(T *)(0->mem)` because
`->` binds tighter than a cast. That is a **hard compile error on any expansion**,
not a wrong value — which is the interesting part, because it tells you nothing
expands it. Confirmed: the only two expansion sites in `src/` are
`src/system/net/curl/lib/memdebug.c:270` and `:301`; that TU is behind a
`CURLDEBUG` guard, is **absent from `config/45410914/objects.json`** and **absent
from the ninja graph** (`ninja -t targets all` matches nothing). The other four
`offsetof` hits under `src/` are prose inside comments — one of them,
`src/system/os/PlatformMgr_Xbox.cpp:872`, already says the macro is unusable.

Fixed to `((int)&(((T *)0)->mem))`. **Predicted Δ0; measured Δ0** (§5).

## 3. The `CharClipDriver.cpp` mis-pin

`CharClipDriver.cpp` carried a 4th `.text` block `0x823EC788–0x823ECD48` (1,428 B,
23 functions) alongside its real, contiguous `0x8239F5E0–0x823A0680`. Six
independent lines say it is not CharClipDriver's:

1. It is **311 KB** from the unit's real span. Retail has no whole-program
   optimization, so TU spatial grouping in `.text` is preserved.
2. It is bracketed by `Server.cpp`'s own blocks — `0x823EC418` and `0x823EC518`
   before it, `0x823ECD48` **immediately** after it — inside an unbroken run of
   network TUs (NetSession, RockCentral, ProfileMgr, NetworkEmulator,
   SessionMessages, SyncStore).
3. `fn_823EC788` is `lwz r3,0x7c(r3); b 0x82A89FF8` and is referenced from exactly
   one place, `.rdata 0x82057978`. `??_7XboxServer` is at `0x8205793C`, so that is
   slot `(0x82057978 − 0x8205793C)/4 = **15**` — precisely the XboxServer slot-15
   override W16-M RTTI-confirmed.
4. `fn_823EC840` is a bare `b 0x82A8A198` into the Quazal band.
5. The pin was added by `783ebf34`, "lane AP: **un-gated** byte-twin identification".
6. **dtk agrees.** When the `.text` line moved, the split re-derived the `.pdata`
   range `0x82208708–0x82208768` onto `Server.cpp` on its own (it derives `.pdata`
   ownership from the `.text` split owning each function). That re-derivation is
   what the split-guard reported as "the split rewrote its own input"; the
   rewritten file is the fixed point and is what is committed.

**`fn_82A87F20` is NOT mis-pinned** — refuting the third name in W16-M's list. It
occurs in `CharClipDriver.s` exactly once, as the target of a `bl`, never as a
definition, and no splits heading owns it (the nearest ends at `0x82A87D2C`), so it
lives in an `auto_*` unit.

Pre-registered, because re-homing is **not** metric-neutral (objdiff pairs by name,
so the base obj consulted changes): 4 rows in the block were at `fuzzy == 100`
under CharClipDriver, worth 144 B, so **between −144 B and a small gain**.

**Measured: −2 functions / −80 B.** Two of the four survived the move because
`Server.obj` supplies byte-identical COMDATs for them (they are small shared
thunks); two did not. Accepted as a deliberately net-negative accuracy fix on the
standing "accuracy beats headline %" directive and the MAPID-1 precedent: the
144 B was credit to `CharClipDriver` for code its TU does not contain. Unit health
improved sharply — `default/CharClipDriver` goes 25/46 rows matched to **21/23**.

## 4. `scripts/dump_vtable.py`

`find_vtable()` looked for `??_7{C}@@6B@` and otherwise took the **first** symbol
merely *containing* `??_7{C}` and `6B`, i.e. COFF symbol-table order. Two defects:

* A class with a virtual base has no `??_7C@@6B@`; its primary is `??_7C@@6B0@@`.
  For `Server` the old rule returned the **secondary** `??_7Server@@6BMsgSource@@@`,
  and nothing in the output said the table was secondary.
* `f'??_7{class}' in name` is an **unanchored substring** test, so class `Set` would
  also match `??_7Setlist…`.

Why it stayed latent — worth recording, because it is the reason a casual check
would have cleared the tool: `find_obj_file('Server')` resolves to the **dtk target**
obj `build/45410914/obj/Server.obj`, whose symbol table happens to list the primary
first, so the default invocation accidentally looks right (and prints 0 slots,
because there the symbol has section 0 / no data). The bug only appears with
`--obj build/45410914/src/network/net/Server.obj`, where the secondary is listed
first. Both orders are measured and both are encoded in the selftest.

Now: deterministic preference (single-inheritance primary → virtual-base primary →
secondaries sorted by **name**, never symbol-table order); `--which` override
accepting a full name or a suffix; every run prints which symbol was chosen, what
kind it is and what else was on offer; a section-0 symbol warns instead of silently
printing 0 slots.

`--selftest` asserts **both** directions on the `Server` fixture: the legacy rule
must pick the *wrong* table (if it ever stops doing so the fixture is no longer a
trap, and the test exits **3 VACUOUS** rather than passing), and the new rule must
pick `??_7Server@@6B0@@`; plus the `--which` suffix path and the anchoring check.
**Proved it can fail**: replacing the preference sort with a no-op makes it exit 1
with `new rule picked ??_7Server@@6BMsgSource@@@`; restored, exit 0.

RTTI cross-check against retail `.rdata`: `??_7Server` @`0x820577BC` and
`??_7XboxServer` @`0x8205793C` are **19** consecutive `.text` pointers each,
reproducing W16-M. Our compiled primary reports 22 relocation entries = 1 `??_R4`
COL + **21** function slots, i.e. the same 2 surplus virtuals W16-M found; their
removal remains unproven and no header was touched.

## 5. Predicted vs measured, per step

| step | commit | pre-registered prediction | measured |
|---|---|---|---|
| 1 — `offsetof` fix + `PlatformMgr.h` comment | `6715d7a9` | Δ0 / Δ0, 0 rows in, 0 rows out (a comment and a never-expanded macro cannot change codegen) | **Δ0 / Δ0, CROSSED IN 0, FELL OUT 0** ✔ exact |
| 2 — `dump_vtable.py` | `78fe092d` | no build effect (script only) | selftest PASS rc=0; sabotaged rc=1 ✔ |
| 3 — re-home the block to `Server.cpp` | `17b5fc37` | between −144 B and a small gain; most likely a partial loss | **−2 fns / −80 B**; CROSSED IN 2 rows/64 B, FELL OUT 4 rows/144 B ✔ in band |
| 4 — drop the `#ifdef` token from the new comment | `b6125e63` | Δ0; insurance against the `6c087cbd` class | native gate below |

One process note worth carrying forward: the step-2 build **failed rc=1** on the
split-guard (dtk rewrote `splits.txt` to re-derive `.pdata`), and a set-diff run at
that moment reported a clean `Δ0 / 0 rows` — **from a stale `report.json`**. The
`rc` test on its own line is what caught it. A second reading was likewise vacuous
because the background wrapper shell exited before `ninja` did, so the `.rc` file
did not yet exist. Both are the same trap: *an unverified build produces a
confident, wrong, zero*.

## 6. Native gate

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

## 7. Commits on `w16-p`

| sha | subject |
|---|---|
| `6715d7a9` | fix(offsetof) + PlatformMgr.h: correct a parse bug and a dead-code citation |
| `78fe092d` | fix(dump_vtable): select the PRIMARY vtable, add --which and a failing-control selftest |
| `17b5fc37` | splits: re-home 0x823EC788-0x823ECD48 from CharClipDriver.cpp to Server.cpp (-80 B, accuracy) |
| `b6125e63` | stddef.h: drop the literal '#ifdef' token from the new comment |

The gate above ran after the last `src/` edit (`b6125e63`) and relinked `rb3-milo`
and `rb3-render`, so the `src/` changes were genuinely exercised rather than
cache-served. It was re-run after this document was committed (docs-only, no
`src/` effect) and returned the identical line.

## 8. NOT done

* **`fn_8251D378` not named** (item 5). Of the three callees the brief asked about,
  **one resolved**: `fn_82A6AC18` is a 12 B wrapper (`r3 = &0x82C991AC; b 0x82A6AB90`)
  and `scripts/target_symbol_map.json` already names `0x82a6ab90`
  **`XOnlineStartupEx`** — corroborated by its body, which null-checks the params
  struct and its first two fields and then calls `0x8284DA30` (mapped
  **`WSAStartup`**) as `WSAStartup(2, &stack_buf)`. The other two did **not** resolve,
  so per the brief's own rule (and "a wrong name is charged, a placeholder is
  forgiven") the map was not edited. What they are **NOT**:
  * **None of the three is an XDK import thunk.** All have real prologues and call
    other local functions; the XEX's imports are `__imp_*` `.rdata` slots around
    `0x820005XX` (dtk: "300 imps and 287 import thunks"), and none of these
    addresses is among them.
  * `fn_8283EAE0` is **not `XNetStartup`** — the brief's hypothesis. `XNetStartup`
    takes one argument (`XNETSTARTUP_PARAMS*`); this call site passes **three**
    integers with the first `= 0`, and the callee treats `r3` as a boolean
    (`subfic`/`subfe`/`and r4,r11,0xF` ⇒ `r4 = r3 ? 0xF : 0`) before tail-calling
    `0x8283E650` with a table pointer `0x82C7AA28`. A scan for `.text` references
    to `__imp_NetDll_XNetStartup` (`.rdata 0x820005E4`) finds **none**.
  * `fn_8283EB28` is not a startup call in shape at all: it reads a global at
    `0x82E08C54` and returns the constant `0x10D0` if it is zero, else calls
    `0x8283E348` and returns 0.
  * Lead for a later lane: `0x8283Dxxx`–`0x8283Exxx` is a **platform-wrapper TU**
    (it also holds `fn_8283D810`, W16-M's `XNotifyCreateListener` wrapper, and
    `fn_8283D660`, SetDiskError's `Sleep`). Identify the TU, not the functions.
* **Mechanism of the `0x82516320` anomaly not settled** — §1. The census
  (1 of 57,733) bounds it to a singular incident; the cross-image comparison
  against `default_patched.xex` / `default_tu5.xex` was not run.
* **No `Server.h` virtual removed.** Retail 19 slots vs our 21 is reproduced, but
  *which* two are surplus is still not decidable (W16-M §1 row 2 stands).
* **`PlatformMgr_Xbox.s` vtable `lbl_8217E3B8` carries no RTTI COL** — W16-M's
  remaining §6 side finding, not investigated.
* The 23 re-homed rows are **not named**. `fn_823EC788` is provably XboxServer
  slot 15, but naming a previously-anonymous address under `name_check` is a bet
  that pays in bug exposure, not bytes, and the rest of the block is unidentified.
