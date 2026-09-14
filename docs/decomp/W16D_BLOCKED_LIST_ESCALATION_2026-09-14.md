# Escalation of W16-D's "blocked" list — what the retail bytes say about each "can't be fixed"

**Lane W16-J (Fable), 2026-09-14.** Worktree `~/tmp/wt-w16-j`, branch `w16-j`, off `main` at
`a8f9e92b`. Audits the blocked list in `INVISIBLE_SOURCE_SWEEP_2026-09-14.md` (Opus lane W16-D,
merge `83488617`) and the refusal in W16-I's `GEM_REGION_RESIDUE_2026-09-14.md` item 4 (`~/tmp/wt-w16-i`,
not landed in this base). Standing rule under test: *an Opus "can't be fixed" is the claim most worth
auditing.* Every number is from `build/45410914/report.json` after a **full** `./tools/ninja-locked`
(`rc=` read from the log file, never from a pipe), ruler **`name_check`**, and every price is a
**set-diff of the `fuzzy == 100` row set** (`tools/rowset_snapshot.py save|diff`) — so "crossed in" and
"fell out" are enumerated rows, not inferred from a count.

## Verdict table

| # | item | Opus's reason | verdict on retail bytes | Δ (pre-registered → measured) |
|---|---|---|---|---|
| 1 | `MemFreeH` + `_MemAllocH` | "blocked on an inline `MainThread`" | **REFUTED** — `/O1` does not inline it; 31 of our objs already emit the COMDAT out-of-line, retail has it at `0x824A4C10` | +2 fns / +192 B → **+2 / +192** |
| 2a | `DataResultList` family | "behind a 19-block grab-bag heading" | **REFUTED as stated** — the 19 blocks are `DataFile`'s own TU; the real defect was the DataResults TU sliced across 4 units | 27–35 fns → **+35 / +3,132 B** (two builds) |
| 2b | `JoypadPollCommon` | "six EEPROM `unkXX` members, `ReadSingleJoypad` missing" | **HALF-RIGHT** — layout was the easy part; the residual is a register/LICM shape, four source probes inert | body 93.35, **+1 / +4 B** (`JoypadSendKeepAlive`) |
| 3 | no-oracle bodies | "no oracle in either sibling repo" | **TRUE but not a stop** — each body is read and classified below; none is < 600 B class (i) | 0 (nothing collected; shapes recorded) |
| 4 | 126 unpairable rows | "need a pin, not source" | **RIGHT about the mechanism, and the size is ~0** — 121 rows / 26,728 B are CRT/XDK/XAPO, pins there are metric fitting; **one exception collected (zlib)** | +6 / ~5,000 → **+12 / +8,784 B** (two commits) |
| 5a | `Handle@OvershellSlot` (W16-I) | "alias evidence would be *both are a blr*" | **DECIDED, outcome (i)** — retail compiled `ToggleMuteStatus` EMPTY (inline witness in retail's own `Handle@SessionUsersProvider`) | +1 / +112 then +1 / +9,276 → **exactly that** |
| 5b | `0x822dea78` survivor (W16-I) | "CharLipSync.obj defines no set spelling" | **RESOLVED** — it is `set<Symbol>::clear`, BandProfile.obj emits it; re-homed + renamed in one commit | +1 / +80 → **+1 / +80** |

**Whole-binary (`name_check`, full builds):** pre-lane **43,174 / 3,929,984 B / 38.35644 % / fuzzy 49.3139**
(`total_functions` 69,217, `total_code` 10,245,956) → post-lane **43,227 / 3,951,564 B / 38.56706 % / fuzzy
49.448895** (`total_functions` 69,216, `total_code` unchanged). **Net +53 fns / +21,580 B / +0.21062 pp**, which
is the exact sum of the nine per-commit set-diffs (2+35+1+0+1+11+1+1+1 fns; 192+3,132+4+0+80+8,392+392+112+9,276 B).
`total_functions` −1 is the `_tr_stored_block` phantom-row correction (item 4), not a loss.

Commits on `w16-j`, in order: `15188a09` (item 1) · `c17a9f9e` (2a) · `cdaa2afb` (2b) · `07d3d3d2` (2b negative
results) · `29309b76` (5b) · `73a39e29` (4, zlib) · `772ea67e` (4, mis-carve) · `20afd9c6` (5a source) ·
`f53af16c` (5a alias) · `9339a725` (this record, first cut) · `e3196d13` (native-gate fix 1: MemMgr) ·
`aae976f8` (native-gate fix 2: Joypad stubs) · the amendment adding the "Native gate" section below.

---

## Item 1 — `MemFreeH` (72 B) + `_MemAllocH` (120 B): the "inline `MainThread`" block — REFUTED

**Opus's reason.** Both bodies open with a bare `bl ?MainThread@@YA_NXZ` (a `MILO_ASSERT` whose value dies);
our `MainThread` is `inline` in `os/OSFuncs.h:8`, so at `/O1 /Ob2` "we cannot emit that call" without a
PCH-path header change cascading ~281 TUs.

**What the bytes say.** The premise — that an `inline` definition means the compiler inlines it — is false for
this compiler at `/O1`. Measured on our own objects: the COMDAT `?MainThread@@YA_NXZ` is **already emitted
out-of-line in 31 of our compiled objs**, retail has it out-of-line at **`0x824A4C10`** (pairs at 100 in
`default/EventTrigger`), and both bodies compiled with the `bl` exactly as retail has it. **No header change was
needed; the A/B the brief budgeted for was never run because the thing it would price does not exist.**

**What changed** (`15188a09`, `src/system/utl/MemMgr.cpp` + `.h`). Both bodies ported, with the oracle
corrections W16-D found applied: `MemFreeH` calls `?MemFree@@YAXPAX@Z` (`0x827BC430`) not `_MemFree`, and 2-arg
`?PoolFree@@YAXHPAX@Z` (`0x827BADB0`) not 3-arg `_PoolFree`. Three further corrections the port needed that
neither oracle carries:

- the debug-arity `MemAlloc(size, file, line, name, ...)` macro in `MemMgr.h` swallows a 2-arg call and forces
  align 0 (`li r4,0`); the real-align site must be spelled **`(MemAlloc)(size, 0x10)`** (parenthesised to defeat
  the macro);
- retail has an explicit `li r3,0` else-arm after the `PoolAlloc` null test — MSVC null-checks a `throw()`
  placement `operator new`, so the source shape is `return new (PoolAlloc(...)) MemHandle(data);`, not rb3-Wii's
  `if (h) new (h) ...; return h;` (which drops the arm);
- rb3-Wii's `heap->mUseHeapAlign` half is omitted: RB3-360 `MemHeap` has no such member (stride 0x24 matches
  retail's addressing without it).

**`0x827BBA68` — the heap-assert callee — deliberately left UNNAMED.** The brief called it `GetCurrentHeapNum`
"if you can prove it". The bytes refute that name: `GetCurrentHeapNum` is `0x827BBA20` (already at 100), and
`0x827BBA68` is a 104 B **current-heap-pointer helper** (`GetCurrentHeapNum() > -1 ? &gHeaps[n] : NULL`). It is
spelled `MemCurrentHeap()` out-of-line in our source so the assert emits the `bl`; the retail address stays
unnamed because no oracle spells it, a placeholder target is already forgiven under `name_check`, and naming it
would be a bet with zero byte upside (CLAUDE.md, map/name economics).

**Measured** (set-diff vs the lane baseline): CROSSED IN 2 rows / +192 B (`?_MemAllocH@@` 120, `?MemFreeH@@` 72,
both 100/100), FELL OUT 0. 43,174 → 43,176; 3,929,984 → 3,930,176.

---

## Item 2a — `DataResultList` family: the "19-block grab-bag" — REFUTED AS STATED, real defect found

**Opus's reason.** Three rows live in `default/DataFile`, "whose splits heading is an over-broad 19-block
grab-bag spanning `0x822E4F70`–`0x8276D808`", the fourth (`Clear`) in `default/RockCentral`; "belongs to a
splits lane".

**What the bytes say.** The 19 blocks are **`DataFile`'s own TU** — MSVC emits one COMDAT per function and the
linker interleaves them with other TUs' COMDATs, so a multi-block heading is the normal shape of a `/Gy` TU, not
a grab-bag. The real defect was narrower and worse: the **DataResults TU `[0x8250af88, 0x8250bf98)` was sliced
across the tail of `RockCentral`, four `DataFile` blocks, one `WebSvcMgrCurl` block and the head of
`ContextWrapper`** — five units each holding a fragment, none able to pair it.

**What changed** (`c17a9f9e`, two builds). `DataResults.cpp` carved into its own unit (`.text` only; dtk
back-filled `.pdata` on the split-guard retry), `src/band3/net_band/DataResults.cpp` ported from rb3-Wii
against our `JsonUtils` API (`JsonObject::Double()` added). `GetType`'s 8 B move `SndAnalysis → JsonUtils` was
pre-registered at 0 and measured 0.

- **Build 1 (carve + source): +27 fns / +1,456 B** — CROSSED IN 36 rows / 2,300 B, FELL OUT 10 rows / 844 B, of
  which **8 are the same rows re-homed** (they appear on both sides of the diff under different unit names) and
  **2 are funclet-pairing noise** (40 B EH funclets at 99.5 / `mpn` 100 whose pairing flipped with the carve).
  ⚠ A naive "10 fell out" reading would have called this a regression; the set-diff *names* the rows, which is
  why the rule is set-diff and not Δcount.
- **Build 2 (map + alias): +8 fns / +1,676 B, 0 fell out.** `Update` (968 B) sat at 99.96 with exactly two
  charged sites, both ICF fold-aliases **proven byte-identical on retail bytes** (T1): `pair<const String,
  DataNode>`'s copy ctor vs its converting ctor from `pair<String,DataNode>` (`0x8250b138`), and the two `pair<>`
  dtors (`0x8250b050`). Map correction: `0x8250b1a0` was **wrongly** named
  `pair<String,String>(const String&, const String&)`; it is `pair<String,DataNode>(const String&, const
  DataNode&)` (called by retail's `make_pair` at `0x8250b208`). Six previously anonymous COMDATs were named from a
  relocation-normalised byte compare of `DataResults.obj` against the retail `.s` (`_M_find`,
  `_List_base<DataResult>::clear`, `make_pair`, `_Copy_Construct<DataResult>` — disambiguated from the `pair<>`
  instantiation by its `bl` to the already-named `DataResult` copy ctor `0x824fa0b8` — `list::_M_create_node`,
  `list::insert`).

**Unit after:** `default/DataResults` 46/47 fns, 3,936/3,980 B. Residue: `fn_8250B400` (4 B) and one 40 B
funclet at 99.5.

**Side finding for a JsonUtils lane (not fixed here).** Retail RB3's `JsonUtils` is rb3-Wii's
`JsonInt`/`JsonDouble`/`JsonString` **hierarchy** with 8-byte tail-call accessors (`0x82B81DD8` `GetType`,
`0x82B81EE8`, `0x82B81F40`, `0x82B81F98`, `0x82B81FE8`, `LoadFromString` `0x82B82440`, `GetValue` `0x82B82500`),
**not** DC3's flat `JsonObject` that our header carries. DC3 is the wrong oracle for this file.

---

## Item 2b — `JoypadPollCommon` (2,468 B): ported to 93.35, residual is allocator shape, NOT crossed

**Opus's reason.** "Six EEPROM members exist only as `unkXX` in our `JoypadData`, and `ReadSingleJoypad` is
missing entirely. Needs struct-layout renaming on a widely-used class first; a first-pass port would land at
outcome C and pay 0."

**What the bytes say.** Half right. The layout was the cheap part (tail laid out to `sizeof 0xd4`,
`mEepromWriteState 0x90 … mLastActivityMs 0xd0`, matching retail's addressing), and `ReadSingleJoypad` is a
one-line `extern "C"` declaration. ⚠ **The brief's oracle was wrong**: rb3-Wii's `JoypadPollCommon` is a
**stub**; the oracle is **DC3's `src/system/os/Joypad.cpp:511-792`** (same engine, same compiler).

**What changed** (`cdaa2afb`). Body ported (EEPROM write state machine, keepalive threshold, connect/disconnect
messages, stick/trigger/pressure/pro-guitar diffing, button msgs, Holmes). `gKeepaliveThresholdMs`'s initialiser
is **`0x7FFFFFFF`, which is what retail `.data` holds at `0x82C71AFC`** (DC3 has −1). `JoypadConnectionMsg` is
3-arg here (DC3's 4-arg form is C2661 on our header — a real RB3/DC3 delta). `JoypadSendKeepAlive(int)` =
`XamInputSendStayAliveRequest` added to `Joypad_Xbox.cpp` + the import decl in `xinput.h`. **Map correction:
`0x82529af0` was mis-named `?ReceiveUpstreamCalbertResponse@@YAXHPAE@Z`**; the retail body is a single `b` to the
`XamInputSendStayAliveRequest` import stub and its one caller is `JoypadPollCommon` passing a pad mask — renamed
`JoypadSendKeepAlive` (bare, `extern "C"`).

**Measured:** CROSSED IN 1 row / +4 B (`JoypadSendKeepAlive`, exactly as pre-registered), FELL OUT 0.
**`JoypadPollCommon` itself: fuzzy 93.353325 / `mpn` 93.93679 — NOT crossed.** 43,211 → 43,212.

**Residual, diagnosed and timeboxed (`07d3d3d2`).** 112 mismatched rows in 13 clusters, **no logic diff** — two
allocator decisions and their consequences:

1. retail never keeps `0.0f` in an FPR — it re-`lfs`'s it from the `.rdata` pool (`lbl_82000D78`) at three
   sites (one hoisted a single level into `f12`) and saves only `f30` (127.0f) + `f31`, frame `0x2a0`; ours
   hoists `0.0f` into `f30`, saves `f29/f30/f31`, frame `0x2b0`, and every local slot shifts +16;
2. retail hoists the **literal 2** into `r14` (`mtctr`, `mr`, `stwx`) and does `lis/lwz gKeepaliveThresholdMs`
   at its single use; ours hoists the `lis` into `r14` and materialises `li 2` at each use;
3. `mConnected` test `cmplwi cr6,r10,1; bne` then `cmplwi r10,0` vs ours `cmplwi 0 / beq`;
4. sensor copy through a pointer pair (`addi r10,r1,0xd8; addi r11,r24,0x24`) vs ours by stack offset.

Four source probes, each measured on a full objdiff of the one obj, **all inert at 112 mismatches**: V1 external
linkage on `gKeepaliveThresholdMs`; V2 `volatile` (+ `const_cast` at `FindData`) — **116, worse**; V3 `extern`,
defined in `Joypad_Xbox.cpp`; F1 unconditional `x = y = 0.0f` before the `mHasAnalogSticks` branch. A comment at
the definition records them so the next lane does not re-run them. **Sharper reason than Opus's:** this row is a
`fixable-liveness.md`-class residual (rematerialisation-vs-hoist), not a layout problem — permuter territory,
which is OFF by directive.

**NOT done:** the retail disassembly is at `~/tmp/w16j_joypadpoll_retail.txt` for the next lane; no further
probes were run after the four.

---

## Item 3 — no-oracle bodies: read from retail and classified; NOTHING collected

**Opus's reason.** "No oracle in either sibling repo" — for `SyncEffectParams@FxSendReverb360` (3,280 B), the
FFT quartet (6,432 B), `FriendsProvider` ctor + `Reload` (140 B).

**What the bytes say.** "No oracle" is true and is not a reason to stop: the retail `.s` is an oracle. Each body
was read from `build/45410914/asm/` and classified per the brief's (i)/(ii)/(iii). None qualified for the "collect
if class (i) and < ~600 B" rule, so **nothing was written**; the shapes are recorded so the next lane starts from
a reading.

| body | bytes | class | shape read from retail |
|---|---:|---|---|
| `?SyncEffectParams@FxSendReverb360@@` | 3,280 | **(i)** — straightforward C++, but 5× the collect bound | A static **preset table** of `{Symbol, XAUDIO2FX_REVERB_I3DL2_PARAMETERS}` rows (the 3,280 B is mostly the table-search + a long parameter copy), followed by a call to `ReverbConvertI3DL2ToNative` and the XAPO parameter set. Writing it means transcribing the I3DL2 preset constants from `.rdata` — mechanical, sizeable, worth a lane of its own with the table read by a script, not by hand. |
| `fft_altivec` | 3,044 | **(ii)** — `__vector4` intrinsics | Radix-2/4 butterfly stages over `__vector4` twiddles; every op is an XDK intrinsic our compiler has (`__vmulfp`, `__vmaddfp`, `__vpermwi`, `__vmrghw`/`__vmrglw`, `__lvx`/`__stvx`). Expressible, but the twiddle-permute constants must be read from `.rdata` and the VMX128 register allocation is scheduling-sensitive; expect a long grind below 100. |
| `fft_recursive` | 2,128 | **(ii)** | Recursive driver that calls `fft_altivec` on halves and does the final combine with the same intrinsic vocabulary. |
| `fft_real_forward_altivec` | 964 | **(ii)** | Real-input packing (split-even/odd) then one `fft_altivec` call and a post-twiddle pass. |
| `SquareComplexTransposeVector` | 296 | **(ii)** — the cheap VMX probe | A 4×4 complex transpose via `__vmrghw`/`__vmrglw` pairs; the natural first target if someone opens the quartet. |
| `fft_scalar` | 772 | **(i)** — NOT in the quartet | Already at fuzzy 76.24 in our tree; a scalar radix-2 FFT. Note the quartet the brief priced is exactly the four VMX rows (3,044 + 2,128 + 964 + 296 = 6,432 B); `fft_scalar` is a fifth, scalar row and the only one where source work alone could plausibly cross. |
| `??0FriendsProvider@@` + `?Reload@FriendsProvider@@` | 140 | **(i)**, but blocked by a PIN defect, not by source | See below. |

**`FriendsProvider` — the cheap probe turned into a mis-pin finding.** Retail shape: ctor `fn_826661D8`, dtor
`fn_826662F0`, `Reload` = `DeleteAll<vector<Friend*>>` (a `DeleteAll` template instantiation over a
`vector<Friend*>`), and the scalar-deleting dtor `fn_826663F0` sits **at the start of the `BandLabel` pin**; a
`FlowSlider` COMDAT `fn_82666290` is interleaved in the middle of the FriendsProvider run. Two corrections to our
tree fall out of the reading:

- `src/band3/meta_band/FriendsProvider.h` declares `std::vector<int> unk2c`; retail's `Reload` deletes through
  it, so it is **`std::vector<Friend*>`** — a container-type defect the metric cannot see (`mpn` is arg-blind).
  **Not edited** — no body compiles against it yet, so an edit would be unmeasurable; the next lane doing the
  bodies should fix the member first.
- the rows are pinned under `UIList` (the enclosing heading), which is why they read as orphans: the honest
  attribution needs a **three-carve pin** (ctor+dtor+Reload block; the interleaved FlowSlider COMDAT left where it
  is; the scalar-deleting dtor pulled out of `BandLabel`'s head). ⚠ Re-homing an already-pinned address is NOT
  metric-neutral (CLAUDE.md) — pre-register per moved block.

**Sharper reason than Opus's:** nothing here is "unreachable"; it is a 3.3 kB table transcription (class i),
~6.4 kB of VMX intrinsic work (class ii), and a 140 B body behind a mis-pin + a wrong member type.

---

## Item 4 — the 126 unpairable rows / 26,376 B: mechanism RIGHT, size ≈ 0, one exception collected

**Opus's reason.** "Need a pin, not source" — the rows live in `auto_*` units with no base obj; a pin would let
them pair.

**What the census says.** Re-run from `tools/undefined_externals_census.py`'s worklist against this tree:
**121 rows / 26,728 B in `auto_*` units** (the brief's 126 / 26,376 was W16-D's snapshot on its own base; the
five-row / +352 B difference is pin drift between the bases, not a disagreement). Grouped by contiguous run and
by what the run's neighbours are, the population is **CRT / XDK / XAPO** essentially in its entirety — the only
`.cpp` we could compile against them is Microsoft vendor source we do not have. A pin over such a run buys a
pairable row **at 0% with no content** — exactly the `ForceEmit_*`-class metric fitting CLAUDE.md forbids
(NOOBJ-1). ⇒ **Opus was right about the mechanism and the lever is ~empty**, with two exceptions:

- `?ReleaseAutoRelease@DxRnd@@QAAXXZ` (652 B, `default/auto_03_8273CBFC_text`) — DC3 has the body; needs a pin
  **and** the source together, and the `auto_03` run is D3D-adjacent so the unit identity needs its own proof.
  Left for a rnddx9 lane.
- **zlib** — three units the game compiles that were never declared. Collected below.

**zlib, collected (`73a39e29`).** Block `[0x82B58594, 0x82B59F78)` is the **game's** `trees.c`, not D3DX's copy:
it is contiguous with `deflate.c`/`crc32.c` (both already 100%), D3DX's own `trees.cpp` pin at `0x8292FE70` holds
only `compress_block`/`build_tree`/`_tr_flush_block`, and a retail `bl` census shows the three map rows spelled
`@D3DX@@` (`pqdownheap` `0x82b58598`, `scan_tree` `0x82b58688`, `send_all_trees` `0x82b58d00`) are called from
**both** copies' `build_tree`/`_tr_flush_block` — i.e. they are **ICF fold survivors whose D3DX spelling was
arbitrary**. Renamed to the C spellings so our `trees.obj` can pair them. `objects.json` declares
`system/zlib/{trees,inffast,inftrees}.c` NonMatching; `splits.txt` pins `.text` `0x82B57818-0x82B57C20`
(inffast), `0x82B57C20-0x82B58098` (inftrees), `0x82B58594-0x82B59F78` (trees); `.pdata` dtk-derived.
Pre-registered ≥ +6 fns / ~5,000 B; **measured CROSSED IN 11 rows / 8,392 B** (all 9 named `trees.c` rows +
`inflate_fast` 1,032 + `inflate_table` 1,140), FELL OUT 0. 43,213 → 43,224. Validator PASS.

**Mis-carve merged (`772ea67e`).** `fn_82B59748` (112 B) + `fn_82B597B8` (280 B) were **one function**,
`_tr_stored_block`: `fn_82B597B8` opens with `slw r9,r6,r11` on an `r11` no caller sets (it is the else-arm of an
inlined `send_bits` reached by the forward `ble cr6` at `0x82B59754`); **zero** `bl`/`b` callers of `0x82B597B8`
anywhere in `.text` and no pointer reference in the image; `fn_82B59748`'s three callers (`0x829306c8` D3DX
`_tr_flush_block`, `0x82b56248` `deflate_stored`, `0x82b59d58` the game's `_tr_flush_block`) are exactly
`_tr_stored_block`'s caller set; retail `.pdata` has no entry at either address (leaf), so the carve came from
dtk flow analysis; our compiled `_tr_stored_block` is 392 B and **byte-identical to retail at `0x82B59748` — 0
differing words over the relocation-free body**. `symbols.txt`: `fn_82B59748 size:0x70 → 0x188`, `fn_82B597B8`
line removed; map `0x82b59748 = _tr_stored_block`. Pre-registered +1 fn / +392 B / `total_functions` −1;
**measured exactly that** (43,224 → 43,225; 69,217 → 69,216; `total_code` unchanged). `trees` unit 10/10.
⚠ This is the CLAUDE.md "ask if the thing needing a name *exists*" rule demonstrating itself: a phantom row from a
mis-carve is indistinguishable from an unidentified one until you check byte geometry.

---

## Item 5a — `Handle@OvershellSlot` (9,276 B): W16-I's refusal — DECIDED, outcome (i), collected in two legs

**Opus's reason (W16-I item 4, quoted).** Our call site is provably right (`lwz r3,168(r26)` = `0xA8` =
`mMuteUsersProvider`, `int` from the preceding call); the divergence is the **callee**: retail's `0x826c3888` is a
4 B `blr`, ours 56 B because "our tree has no `VoiceChatMgr.cpp` at all — only a header". Refused because (a) a
`src/` change making the callee empty "rests on inference", and (b) an alias "whose only byte evidence would be
*both are a 4-byte `blr`*" would be a fabricated 9,276 B. Recommended experiment: establish non-metrically whether
RB3-360 retail has any `VoiceChatMgr` body at all.

**Correction to the framing first.** The callee is **`SessionUsersProvider::ToggleMuteStatus(int)`**, not a
`VoiceChatMgr` method; `VoiceChatMgr` is what rb3-Wii's version of that body *calls*. So the question is not
"does retail have a `VoiceChatMgr`" but "what did retail compile `SessionUsersProvider::ToggleMuteStatus` to".
That has a direct witness.

**The evidence chain (all retail bytes, none metric):**

1. **Retail's own `Handle@SessionUsersProvider` (`fn_826545B8`, 380 B; ours already 100) inlines the callee.**
   Its case strings are `kick_player` (`0x820D3370`), `toggle_mute_status` (`0x820D335C`), `get_size`
   (`0x8204788C`). The `toggle_mute_status` arm at **`0x82654694`–`0x826546A4` is `bl fn_8274B0F8` (`Int`) then
   `b .L_82654650` — nothing between**, while the adjacent `kick_player` arm is `bl Int; bl fn_826542B0`
   (`KickPlayer(int)`). `/Ob2` inlined an **empty** `ToggleMuteStatus(int)`; the argument is still evaluated
   because `Int()` has side effects. ⇒ **outcome (i): the 360 build compiled the body to nothing.**
   ⚠ Our tree had **metric-fitted** this handler to `HANDLE_ACTION(toggle_mute_status, _msg->Int(2))` — the call
   deleted — so that `Handle` would read 100 while `ToggleMuteStatus` was 56 B. `20afd9c6` restores the rb3-Wii
   spelling `ToggleMuteStatus(_msg->Int(2))`; `Handle` **still reads 100** because the body is now empty.
2. **`Mat@SessionUsersProvider` (`fn_826541B0`, 112 B; ours was 5.32) corroborates `IsMuted ≡ false`.** Retail:
   `bl fn_82814E00(slot, "check")`; if true, `lbz 0x38(this)` (`unk28`) → `lwz r3,0x40(this)` (**`mUncheckedMat`
   only**) else `li r3,0`; else `bl fn_82791F58(slot)` (`DefaultMat`). `mCheckedMat` (`0x3c`) is **never read in
   the unit**. With `IsMuted()` returning constant `false`, our `Mat` compiles to exactly this and crossed.
3. **No 360 `VoiceChatMgr`.** No `.?AVVoiceChatMgr@@` RTTI, no voice-chat strings anywhere in retail (PE sections
   parsed, `.rdata` VA `0x82000400`, `.data` `0x82c64400`); rb3-Wii's `VoiceChatMgr.cpp:80` is a WiiSpeak
   implementation (`std::find` over a mute vector) with no 360 counterpart. Our tree's `net/VoiceChatMgr.h` is a
   header with no `.cpp` and no native references.
4. **Exhaustion over retail's `bl` targets** (census: `(w & 0xfc000003) == 0x48000001`, sign-extended 26-bit
   displacement, whole `.text`): **27,085 distinct targets, of which exactly 4 begin with `blr`**:
   - `0x826c3888` — **1,099 callers**, no `.pdata` entry, callers HMX-wide (`?BandTerminate@@YAXXZ`,
     `vector::_M_fill_assign`, …) — the survivor;
   - `0x82516320` = `?SetDiskError@PlatformMgr@@QAAXW4DiskError@@@Z` — 8 callers (ArkFilesInit,
     StreamChecksumValidator), **HAS a `.pdata` entry and an 8-byte EH prefix `0x82829530 0x82087b70`** ⇒ different
     `.xdata`, and ICF folds only COMDATs identical *including* `.xdata`, so it **cannot** be the fold twin of an
     EH-free leaf. (Passed to W16-M as a PlatformMgr fact: retail's `SetDiskError` is empty.)
   - `0x82aadf90` / `0x82aadf98` — inside the Quazal `/Od` band (2 and 1 callers), non-COMDAT.
   Targets that are `li r3,0; blr` (the `IsMuted` shape): `0x82a6fc60` (126 callers), `0x823591e8` (29) — not
   relevant to the site, recorded for completeness.
   ⇒ **An EH-free empty leaf COMDAT from an HMX `/Gy` TU has exactly one place it can have folded.**
5. **FT1 holds:** `target_symbol_map.json` places `?ToggleMuteStatus@SessionUsersProvider@@QAAXH@Z` nowhere.
6. **The site:** `0x825e2f38` in `Handle@OvershellSlot` is `mr r4,r3; lwz r3,0xa8(r26); bl 0x826c3888` =
   `mMuteUsersProvider->ToggleMuteStatus(i)` with `ToggleMuteUser` inlined (rb3-Wii `OvershellSlot.cpp:722` =
   ours `:697`). W16-I already proved this; nothing about the site changed.

**Why this is not "both are a `blr`".** The byte comparison of two 4 B bodies is vacuous and was never the
evidence. The evidence is (1) a retail inline witness that the *named* function is empty, plus (4) exhaustion
showing the survivor is the only address such a body can have folded to. That is the same tier W15-D used to add
`?Copy@BandTrack@@` to this same group (VT1 vtable-geometry witness, FT-EMPTY), and it is recorded verbatim in the
group's `evidence` string.

**What changed, two commits so each leg is priced alone:**

- **`20afd9c6` (source).** `src/band3/meta_band/SessionUsersProviders.cpp`: `VoiceChatMgr.h` include dropped;
  `ToggleMuteStatus` = the asserted range check only (compiles to nothing); `IsMuted` returns `false`; handler
  call restored. Our COMDATs: `ToggleMuteStatus` **4 B `4e800020`, 0 relocations**; `IsMuted` 8 B `38600000
  4e800020`. Pre-registered **+1 fn / +112 B** (`Mat` crosses; `Handle@SessionUsersProvider` stays 100;
  `Handle@OvershellSlot` unchanged because the site is still name-charged). **Measured exactly that**
  (set-diff vs `~/tmp/w16j_rows_pre_5a2.json`): CROSSED IN `?Mat@SessionUsersProvider@@` 112 B, FELL OUT 0;
  43,225 → 43,226; 3,942,176 → 3,942,288. `default/SessionUsersProviders` 20/24 → 21/24.
- **`f53af16c` (alias).** Spelling appended to the `folded` list of group `0x826c3888` in
  `scripts/symbol_aliases.json` with a bracketed `[W16-J 2026-09-14, …]` evidence note (round-trip
  `json.dumps(a, indent=1) + '\n'`, verified byte-identical on the untouched file first). `touch config.yml`, full
  build. Pre-registered **+1 fn / +9,276 B**, no fall-outs. **Measured exactly that** (set-diff vs
  `~/tmp/w16j_rows_pre_5a3.json`): CROSSED IN `?Handle@OvershellSlot@@` 9,276 B (99.99784 → 100/100), FELL OUT 0;
  43,226 → 43,227; 3,942,288 → 3,951,564 (38.47653 → 38.56706). `default/OvershellSlot` 405/430 → 406/430,
  35,632 → 44,908 B. `icf_alias_finder.py --validate`: PASS, 0 contradicted, 1,632 groups.

**Open, unexplained, in the same unit:** `fn_82654440` (76 B, 0%) takes `(this, int, ptr)`, indexes `mUsers`,
vtable-adjusts, and tail-`b`s `fn_8251C960` with `r3` = `0x82CC9D1C` (a `.data` object) — no rb3-Wii method of
`SessionUsersProvider` has this shape; and `fn_82654324` (40 B, 99.5) is a funclet-pairing row. Neither was
touched.

---

## Item 5b — `0x822dea78`: proven-wrong name re-homed and renamed in one commit

**Opus's reason (W16-I item 2).** The map spelling at `0x822dea78` (`map<Symbol,CharLipSync*>::clear`) is proven
wrong but `CharLipSync.obj` defines no set-family spelling, so renaming risks un-pairing the row permanently.

**What the bytes say.** The 80 B body `bl`s `0x822dd9a0`, which the map names `set<Symbol>::_M_erase` (`li r3,20`
node size), so it is **`set<Symbol>::clear`**; the `map<Symbol,CharLipSync*>` instantiation the map claimed would
carry `li r3,24`, and a different-size COMDAT cannot ICF-fold with it. Retail `bl` census: **62 callers, 0 in
CharLipSync** (ChordShapeGenerator 10, AccomplishmentProgress 8, CharClip 6, BandMachine 4, …, BandProfile 2).
`BandProfile.obj` emits both `set<Symbol>::_M_erase` and `set<Symbol>::clear` and already owns the single-function
carve `[0x822DD9A0, 0x822DD9FC)` at 100/100.

**What changed (`29309b76`).** The 80 B carve `[0x822DEA78, 0x822DEAC8)` moved to `BandProfile.cpp`
(CharLipSync's block `[0x822DEA4C, 0x822DEB3C)` split around it), map renamed, and the alias group's survivor
becomes the set spelling with the CharLipSync spelling moved to `withdrawn` as CONTRADICTED (nothing pruned).
Pre-registered +1 fn / +80 B; **measured exactly that**: CROSSED IN `BandProfile::?clear@?$_Rb_tree@VSymbol@@…`
80 B, FELL OUT 0, the `0x822dd9a0` row unchanged at 100/100. Validator PASS.

---

## Native gate — FAILED twice on this lane's own source, fixed, PASSED (run LAST, after every `src/` edit)

The brief requires `tools/native_build_gate.sh` as the final action whenever `src/` is touched, with the
`NATIVE_GATE_RESULT` line pasted verbatim and `skipped=0`. Three runs, all in `~/tmp/wt-w16-j`:

| run | log | verdict line (verbatim) | cause |
|---|---|---|---|
| 1 | `~/tmp/native_w16j.log` | `NATIVE_GATE_RESULT verdict=FAIL expected=18 verified=1 skipped=0 partial=0 failed=17 rc=1` | **item 1** — `_MemAllocH`'s retail spelling `(MemAlloc)(sz, 0x10)` is a 2-arg call to the `#ifndef HX_NATIVE` overload; under `HX_NATIVE` only the 5-arg debug `MemAlloc` exists ⇒ clang `MemMgr.cpp:888:61: too few arguments to function call, expected at least 4, have 2`; 17/18 targets NOBINARY |
| 2 | `~/tmp/native_w16j2.log` | `NATIVE_GATE_RESULT verdict=FAIL expected=18 verified=10 skipped=0 partial=0 failed=8 rc=1` | **item 2b** — the `JoypadPollCommon` port calls three `extern "C"` back-end functions (`ReadSingleJoypad`, `requestBreedWrite`, `JoypadSendKeepAlive`) defined only in `Joypad_Xbox.cpp` (XamInput), which the native link does not compile ⇒ `undefined reference` in the 8 targets that link `Joypad.cpp` |
| 3 | `~/tmp/native_w16j3.log` | **`NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`** | — |

Fixes, each with the match build proved unmoved:

- **`e3196d13`** — `_MemAllocH` gets the `MemRealloc` precedent from the same file: `#ifdef HX_NATIVE` spells
  the 5-arg debug form with the same `0x10` align, `#else` keeps `(MemAlloc)(sz, 0x10)` **textually unchanged**.
  Full match build (`~/tmp/rb3_build_w16j_gatefix.log`, rc=0, exactly one MemMgr recompile): rowset diff vs a
  snapshot saved immediately before = **0 crossed / 0 fell**, all four measures identical to the post-lane
  figures above; `_MemAllocH` stays fuzzy 100 / mpn 100. Predicted Δ0 (the preprocessed match TU is
  identical), measured Δ0.
- **`aae976f8`** — three `.weak` stubs in `native/src/dta_link_stubs.s` beside the existing
  `JoypadSetActuatorsImp` (same class: Xbox-side Joypad back end). `ReadSingleJoypad` returns 0 ==
  `kJoypadNone`, so the ported poll loop stays inert natively; `requestBreedWrite` returns false;
  `JoypadSendKeepAlive` is a no-op. `native/` is not a match-build input, so no report.json movement is
  possible and none was measured.

Neither compile error nor link error is visible to the match build (it compiles, never links, and never
defines `HX_NATIVE`) — this is exactly the class the gate exists for, and the reason the brief says LAST.

## Traps recorded for the next lane (each one cost this lane a measurement or a re-run)

- **The parenthesised `(MemAlloc)(size, align)` retail spelling is native-hostile.** It bypasses the debug-arity
  macro on purpose, but under `HX_NATIVE` the 2-arg overload does not exist. Any new site needs the
  `#ifdef HX_NATIVE` 5-arg / `#else` 2-arg pair (`MemRealloc` and now `_MemAllocH` in `MemMgr.cpp`). Cost
  here: one failed gate run.
- **Porting a body that calls `extern "C"` platform back-end functions (`Joypad_Xbox.cpp` etc.) adds native
  link dependencies the match build cannot see.** Grep `native/src/dta_link_stubs.s` for each new callee before
  running the gate; the linker lists every undefined reference in one pass, so one re-run suffices once they
  are all stubbed. Cost here: a second failed gate run.

- **`scripts/symbol_aliases.json` and `scripts/target_symbol_map.json` round-trip with DIFFERENT `ensure_ascii`.**
  Aliases: `json.dumps(a, indent=1) + '\n'` (default `ensure_ascii=True`); the map needs `ensure_ascii=False`.
  W16-I's note that one formula serves both is wrong by one flag. Prove the round-trip is byte-identical on the
  untouched file before editing — then the diff is only your edit.
- **A `splits.txt` edit whose `.pdata` record moves FAILS its first build by design** (the split-guard refuses the
  non-fixed point: "REWROTE ITS OWN INPUT"); the second build passes. **A failed build leaves `report.json`
  STALE** — check its mtime against the build log before reading a number.
- **`cmd > log; echo rc=$?` in a background wrapper reports the wrapper's exit code.** Append `rc=` to the log
  from inside the subshell and `grep '^rc='` the log.
- **A Python walk over the aliases file printed NOTHING for `826c3888` because of a value-length filter; plain
  `/usr/bin/grep -c -a` found 3 hits.** Verify every negative with a second instrument — a vacuity that agrees
  with your prior is the hardest kind to catch. Same family: `(g.get('address') or '')` — 51 groups have
  `address: None` and `.lower()` on them aborts the walk.
- **`build/45410914/asm/<unit>.s` uses pre-renamer `fn_<addr>` names**, never mangled names; map `.s` bodies to
  `report.json` rows by address order + size. And the unit's `.s` is bare `asm/SessionUsersProviders.s`, not under
  a subdirectory — a nested zsh glob aborts the whole command on "no matches found".
- **`VA − 0x82000000` is only a valid file offset for `.text`.** For `.rdata`/`.data` strings parse the PE section
  headers (`.rdata` VA `0x82000400`, `.text` VA `0x82270000` @ file `0x264e00`, `.data` VA `0x82c64400`); `.pdata`
  at file `0x1f1600`, 8-byte **big-endian** entries.
- **Funclet-pairing noise in a set-diff:** a carve can flip 40 B EH funclets at 99.5/`mpn` 100 in and out of the
  `fuzzy == 100` set. They are recognisable by size and by appearing as re-homed pairs; do not book them as
  regressions or gains.
- **"quartet already at 100" is not a claim this record makes**: the four VMX FFT rows are at 0; only
  `fft_scalar` (not in the quartet) has a partial score. Stated here because the brief's 6,432 B figure is the
  quartet and a reader could pair it with the wrong row.

## NOT done, and why

- `JoypadPollCommon` body not crossed (93.35): residual is regalloc/LICM shape, four probes inert, permuter OFF.
- Item 3 bodies not written: none is class (i) under 600 B; shapes recorded above.
  `FriendsProvider.h`'s `vector<int>` → `vector<Friend*>` member fix **not applied** (unmeasurable until a body
  compiles against it).
- `?ReleaseAutoRelease@DxRnd@@` (652 B) not pinned: unit identity in the `auto_03` D3D run needs its own proof.
- `0x827BBA68` deliberately unnamed (zero byte upside, no oracle spelling).
- `0x82529ae8 ?ReceiveUpstreamAccelerometerResponse@@YAXHPAE@Z` (the neighbour of the corrected `0x82529af0`)
  **not audited** — the same `Calbert`/`Accelerometer` naming family; open question.
- W16-D's smaller leftovers `?Release@VertexBufferData@DxMesh@@` (68 B) and `??0Shuttle@@QAA@XZ` (32 B) not
  opened.
- `fn_82654440` (76 B) in `SessionUsersProviders` unexplained.
- `PlatformMgr.h:125` / `PlatformMgr.cpp:155` (`SetDiskError`, retail `0x82516320`) **not touched**: W16-M
  (running concurrently) reports the 208 B extent is a retail `.pdata` artefact — the 4 B `blr` stub's record
  swallowed a following fragment (two raw branches into `DataSet`, `bl ??0DiskErrorMsg`, …), and
  `symbols.txt:147625 size:0xD0` merely mirrors it. Same mis-carve class as item 4's `_tr_stored_block`, but
  it is W16-M's row and belongs to a splits/map lane; recorded by them in
  `PLATFORMMGR_ESCALATION_2026-09-14.md` §6 on `w16-m`.
- No `ab_measure.py` A/B was run: every price here is a full-build set-diff against a snapshot saved on the
  same tree immediately before the change, which is the same-ruler protocol; `ab_measure` would have added
  nothing for single-commit source/map legs and refuses `--from-dirty` with a staged index.
