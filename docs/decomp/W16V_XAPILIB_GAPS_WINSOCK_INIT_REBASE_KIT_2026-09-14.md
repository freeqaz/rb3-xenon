# W16-V — W16-R's leftovers: xapilib gaps, WinSockSocket::Init, a mis-attributed 1,864 B row, ReleaseAutoRelease, and the landing kit

Branch `w16-v`, base main `814c6135`. Ruler: `functionRelocDiffs=name_check`,
`ppc.calculatePoolRelocations=false` (read from `report.json`'s
`provenance.diff_config`, not assumed).

## Lane-internal measures

| | matched_functions | matched_code | total_code | total_functions |
|---|---:|---:|---:|---:|
| base `814c6135` | 43,272 | 3,960,812 | 10,245,956 | 69,216 |
| tip | **43,273** | **3,960,912** | 10,245,956 | 69,216 |
| Δ | **+1** | **+100 B** | 0 | 0 |

The entire lane delta is Item 1. Items 2, 3 and 4 were each predicted Δ0 and
each measured Δ0 exactly. `total_code` and `total_functions` never moved, so no
pin evicted a phantom `type:label` row.

Set-diff of the `fuzzy==100` row set, base → tip: **crossed in 1, fell out 0**.

```
+ default/NetworkSocket_Win::?Init@WinSockSocket@@SAXXZ   100 B
```

## Gate chain (run after the last source edit)

```
BUILD rc=0                      (~/tmp/rb3_build_w16v_9.log)
verify_ruler_agreement --check  rc=0   "OK: both objdiff-cli entry points resolve the same ruler."
verify_objs_patched --verify-manifest rc=0
  [patch-state] OK: 1210 decomp, 3113 target objects match 2026-09-14T14:58:12Z (tree_sha256=5512fb57f9bb699c)
```

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

`skipped=0`, as required.

---

## Item 1 (LEAD, source) — `?Init@WinSockSocket@@SAXXZ`, 104 B vs retail 100 B — FIXED

Commit `31570c39`.

**Retail bytes.** The map address is `0x82531980`; the `.pdata` extent is
`[0x82531980, 0x825319E4)` = **100 B / 25 instructions**. Reading the body, the
`XNetStartupParams` local is zeroed by an inlined 13-byte `memset`, and then
retail writes **exactly one** field before calling `XNetStartup`:

```
825319C0   98E10050   stb  r7, 0x50(r1)      ; r7 = 0xd  -> cfgSizeOfStruct
```

There is **no store to `0x51(r1)`**. Our side had one, from

```cpp
params.cfgFlags = 1;
```

which is a verbatim inheritance from DC3 (`lookup_dc3` has the assignment; DC3
is *newer* than RB3, and this is the expected divergence class — our engine is a
verbatim DC3 copy). rb3-Wii has no winsock at all, so retail bytes were the only
oracle available, and they are unambiguous: `cfgFlags` stays 0 in retail.

**Change.** Deleted that one line from `src/system/os/NetworkSocket_Win.cpp`.

**Predicted** (pre-registered before the build): the row crosses, **+1 fn /
+100 B**.
**Measured:** +1 fn / +100 B; the row went `fuzzy 95.0 → 100.0`, `mpn → 100`.

One unpredicted bonus worth recording: removing the store also resolved a
pre-existing **r9↔r10 swap** further down the body. That was not a separate
defect — deleting the store made the live ranges identical to retail's, so the
allocator made retail's choice. This is the documented pattern that a
`REGISTER_SWAP` label is a *symptom*, not a diagnosis; nothing was done to the
registers directly.

---

## Item 2 (pins) — the two unpinned xapilib gaps — 7 PROVEN PINS, Δ0

Commit `05670a3d` (with Item 6a).

Gaps `0x8283C6AC–0x8283D628` and `0x8283D668–0x8283D818`. Seven headings added
to `config/45410914/splits.txt` + `config/45410914/objects.json` as source-less
`xdk/xapilibi/<name>.cpp`, following W16-R's `sleep.cpp` precedent:

| heading | `.text` |
|---|---|
| `xapi0.cpp` | `0x8283CA00–0x8283CED8` |
| `xgetoverlappedextendederror.cpp` | `0x8283D2B0–0x8283D2D8` |
| `closehandle.cpp` | `0x8283D2D8–0x8283D320` |
| `xgetoverlappedresult.cpp` | `0x8283D320–0x8283D3B8` |
| `getlocaltime.cpp` | `0x8283D590–0x8283D624` |
| `xusergetsignininfo.cpp` | `0x8283D7C8–0x8283D810` |
| `xnotify.cpp` | `0x8283D810–0x8283D818` |

**The decisive instrument was NOT a DC3 body twin.** A relocation-normalized
twin scan over both gaps returned `-- no twin --` everywhere. That is not a
failed hypothesis, it is a property of the version gap: retail links XAPILIB
**2.0.11164.0** and DC3 links a newer XDK, so the bodies genuinely differ. The
scan was re-run dropping instrumentation nops and save/restore sequences
(`twin2.py`) with the same result. Three other evidence classes carried the
identifications instead:

* **Dispatch-table isomorphism** (the `CloseHandle` proof). Both images dispatch
  file I/O through a `_fileIoHooks` global at the **identical slot multiset**
  (+0x4, +0x18, +0x20 ×2, +0x2c). DC3's slot +4 is `CloseHandle`; retail's slot
  +4 is `0x8283D2D8`.
* **PE entry point** (the `mainCRTStartup` proof, hash-free). The image's entry
  is `0x8283CD20`, 440 B, matching DC3's `mainCRTStartup` at `0x82335EE0`.
  Independently corroborated by `config.yml`'s own pre-existing header, which
  already recorded `entry 0x8283CD20` — a third witness nobody had connected.
* **Import-ordinal decode.** XEX import stubs are
  `01 MM OOOO / 02 MM OOOO / mtctr / bctr`, where `MM` is the module index and
  the low 16 bits the ordinal; module `0x00` = `xam.xex`, `0x01` = `xboxkrnl.exe`
  (established this lane). `GetLocalTime` is proven **twice independently** —
  a body twin *and* its `KeQuerySystemTime` / `RtlTimeToTimeFields` ordinals.
  `XUserGetSigninInfo` and `XShowDirtyDiscErrorUI` are proven by a whole-image
  sole-caller census on their `xam` ordinals.

**Map names.** Six names added to `scripts/target_symbol_map.json`
(`XapiLookupString`, `GetLocalTime`, `XUserGetSigninInfo`,
`XShowDirtyDiscErrorUI`, `WaitForSingleObject`, `RtlSetLastNTError`). Five
further identifications this lane derived independently
(`XapiShowErrorAndWait`, `XapiPAL50Incompatible`, `XGetOverlappedExtendedError`,
`CloseHandle`, `XGetOverlappedResult`) turned out to be **already present in the
map with exactly the names derived here** — an unplanned control on the method,
5 for 5.

`0x8283cd20` was deliberately **left as `"entry"`** rather than renamed to
`mainCRTStartup`. The existing name is not wrong, DC3 only *attests* the other
spelling, and under `name_check` renaming an already-correct name is a bet with
no upside.

**Predicted** Δ0 on the matching keys (adding a pin over `auto_*` code is
reattribution, which is metric-neutral by construction).
**Measured** Δ0 exactly; `total_code` **unchanged**, so no phantom label row was
evicted. The 3 identifications left unpinned cost 0.

### Sub-item — the `xutilitydrive.cpp` placeholder: NOT renamed

W16-R's heading is a placeholder and the brief asked for the real XDK object
name if the import-ordinal decode yields it. **It does not.** The cluster's
stubs decode to ordinals whose names do not appear in DC3 at all
(`command grep -ac "XMountUtilityDrive"` over the DC3 tree returns **0**), so
there is no second witness for any candidate spelling. The heading was left as
`xutilitydrive.cpp` rather than renamed on one-sided evidence. Renaming it is
cosmetic — it changes no measure — so this is a deliberate no-op, not a
deferral of value.

---

## Item 3 (pin re-home; NOT neutral) — `0x82270E68` / 1,864 B — RE-HOMED to `App.cpp`, Δ0

Commit `c33be954`.

**Byte geometry first**, because a dtk mis-carve and a mis-pin are
indistinguishable from the row list: `.pdata` has a **real 1,864 B function
extent** at `0x82270E68`. So this is a genuine function, wrongly attributed —
a mis-pin, not a phantom. Nothing would have been moved had the extent been
absent.

**What the body is.** 18 string references, all App startup text. Both oracles
(DC3 and rb3-Wii) place those strings in `src/App.cpp`. It is not
`RhythmDetector` code by any reading.

**A near-miss worth recording.** I almost concluded the block was correctly
pinned because 9 sibling rows in `default/RhythmDetector` score 99.3–100%. That
is not evidence of TU membership: **all of them carry `masked_equal: True`**,
i.e. they pair by *funclet byte signature*, which is name-independent. A
`masked_equal` pairing says nothing about which TU owns the code. Checking that
flag is what stopped a wrong "leave it alone" verdict.

**Change.** `RhythmDetector.cpp`'s `.text start:0x82270B84 end:0x82271748` split
into `[0x82270B84, 0x82270E68)` + `[0x822715B0, 0x82271748)`, and a new
source-less heading added:

```
App.cpp:
	.text       start:0x82270E68 end:0x822715B0
```

**Pre-registered prediction** (written before the build, because re-homing an
already-pinned address is explicitly NOT metric-neutral): Δ0, with a stated
**risk of −3 fns / −96 B** if three `RhythmDetector` rows that currently pair
were to lose their base obj.
**Measured:** Δ0 exactly. The risk did not fire — `default/RhythmDetector` went
38 → 37 rows and still holds **10 at fuzzy 100**.

---

## Item 4 (pin) — `?ReleaseAutoRelease@DxRnd@@QAAXXZ`, 652 B — PARTIALLY PROVEN, pinned to the proven span only

Commit `b850ce16`.

**Identity of the function: PROVEN.** Retail `0x8273CC08` (652 B) is a
near-perfect twin of DC3's `0x8261A5B0` (680 B), which DC3's leaked
`ham_xbox_r.map` places in **`rnddx9:Rnd_Xbox.obj`**. Every difference is
version drift, none structural:

| | retail | DC3 |
|---|---|---|
| frame | `-0xe0` | `-0xf0` |
| member offset | `0x1c4` | `0x224` (DC3's `DxRnd` is newer/bigger) |
| register | `r24` | `r22` |
| call targets | differ | differ |

**Extent of the pin: proven only to `0x8273CEF0`.** DC3's in-object ordering is
`ReleaseAutoRelease` → `__unwind$170672` (`0x8261A858`) → `__unwind$170673`
(`0x8261A880`) → `BeginDrawing`. Retail mirrors the funclet pair exactly:
`fn_8273CE94` (40 B) and `fn_8273CEBC` (40 B). So `[0x8273CC08, 0x8273CEF0)` is
proven and is what was pinned, appended to `Rnd_Xbox.cpp`'s existing blocks
(its previous last block ended at `0x8273CA70`, 408 B earlier).

**What the bytes show beyond the funclets — and why it was NOT pinned.** Retail
`0x8273CEF0` (396 B) does **not** match DC3's `BeginDrawing`. It opens with a
bit-flag test:

```
lis    r10, -0x7d20
lwz    r11, 0x4ffc(r10)
clrlwi. r9, r11, 0x1f
bne    ...
ori    r11, r11, 1
```

where DC3's `BeginDrawing` opens with several `lis` and a different structure.
A string sweep over `[0x8273CEF0, 0x8273D7C8)` produced exactly one candidate,
`'t + nBassCt + nGuitarCt + i )'` at `0x8273D464` — which is a **mid-string
fragment**, the signature of a spurious `lis`/`addi` reconstruction, not a
reference. So there is no positive evidence in either direction, and the run was
left unpinned rather than pinned on adjacency.

**Predicted** Δ0: the map names the row, but our `src/system/rnddx9/Rnd_Xbox.cpp`
does **not define `ReleaseAutoRelease`**, so the row cannot pair by name even
after the pin — it reads 0% in either unit. Stated risk: perturbing the 121 rows
already at fuzzy 100 in `default/Rnd_Xbox`.
**Measured** Δ0 exactly; `default/Rnd_Xbox` 170 → 173 rows, still **121 at fuzzy
100** — the risk did not fire.

The remainder re-carved cleanly as `default/auto_03_8273CEF0_text`, exactly the
8 rows predicted (`fn_8273CEF0` 396 B, `fn_8273D07C` 32, `fn_8273D0A0` 1,392,
`fn_8273D610` 72, `fn_8273D658` 20, `fn_8273D670` 20, `fn_8273D688` 156,
`fn_8273D728` 160). **That is the escalation surface for this item.**

---

## Item 5 (tooling) — the landing kit — COMMITTED as `tools/rebase_kit/`

Commit `522b01dd`.

Ported from `~/tmp` with **no hardcoded `/home/free/tmp` paths**
(`rebase_drive.py` resolves its siblings from `__file__`). Merge semantics are
unchanged as instructed — the code below the docstring in `resolve_aliases.py`
is **byte-identical** to `~/tmp/resolve_aliases.py`, verified by `diff`.

```
rebase_drive.py     resolver per conflicted file, exit 3 on anything else
resolve_aliases.py  v2 field-wise three-way merge (symbol_aliases.json)
resolve_map.py      key-delta three-way merge (target_symbol_map.json)
_v1_frozen.py       the OLD alias merge, frozen ONLY as the test's control
README.md           the invariants
test_rebase_kit.py  replays the real W16-J landing through both
```

**The briefed figure was verified literally before anything was built on it**
(house rule — briefed numbers have been stale before). Replaying
`OURS=f29132d7 BASE=a8f9e92b THEIRS=95ebc590 EXPECT=dcd8d6fe` reproduces
`TEST: 4 deltas applied; 1632 groups vs expect 1632; differing groups: 2`
exactly, both differences in the `evidence` string only.

**The test asserts structurally, not by exit code.** `--test` exits 1 on *any*
difference from EXPECT including an `evidence`-only one, which is not a merge
defect; so the test calls `three_way()` and compares `survivor`, `folded`,
`withdrawn`, `address` on every group, treating `evidence` as informational.

| | deltas | groups | structural diffs | folded memberships missing |
|---|---:|---:|---:|---:|
| **v2** | 4 | 1,632 | **0** | **0** |
| **v1** (control) | 5 | 1,632 | **2** | **3** |

The v1 arm reproduces the real W16-J loss (−3,728 B of main's concurrent
memberships) and is the point of the exercise: a control that cannot fail proves
nothing. 3 tests, 3 passed.

**Wiring:** `tools/` is already a pytest root in `scripts/test_tools.py`'s
`STATIC_ROOTS`, so the file is claimed automatically —
`coverage_gaps()` returns `[]` and `_claimed_by_a_root()` is `True`. No table
entry was needed. Module names in the kit collide with nothing else tracked
(checked), so putting the directory on `sys.path` shadows nothing.

---

## Item 6 (doc) — BOTH LINES LANDED

* **6a** `config/45410914/config.yml` (commit `05670a3d`): a lineage comment
  block citing W16-R §1 — the target is the **RB3DX-lineage TU5**
  (`orig/45410914/default.xex`, sha1 `c5a17091cb44c0119424390a1738d161995e430e`,
  byte-identical to `RB3DX-Xbox/default.xex`), **not** clean retail TU5
  (`_tu5probe/clean/clean_tu5.xex`, PE sha1 `5f3f667a689e804c1efc736cae4ff2416a278747`),
  which differs by 53 words / 10 byte-patch groups.
* **6b** `CLAUDE.md` line 43 only (commit `048b7fa7`): "Target binary: vanilla
  retail XEX" is refuted; replaced with the RB3DX-lineage TU5 statement citing
  W16-R's doc, and naming the rows that are structurally unmatchable against
  this image (`DataSet`, `IsDemo`, `AddSongData`, `SetDiskError`, `main`).
  Nothing else in `CLAUDE.md` was touched.

---

## NOT done, with reasons

1. **The 8-row remainder of the `auto_03` D3D run** (`fn_8273CEF0` … `fn_8273D728`,
   now `default/auto_03_8273CEF0_text`). Retail `0x8273CEF0`'s head diverges from
   DC3's `BeginDrawing` and the one string hit in the range is a mid-string
   fragment. Pinning on adjacency alone would be an unproven pin. **This is the
   item to escalate.**
2. **`ReleaseAutoRelease` still reads 0%** even though it is now correctly
   homed. Our `src/system/rnddx9/Rnd_Xbox.cpp` does not define it, so the row
   cannot pair by name. The pin is correct and worth having; collecting the
   652 B needs the body written, which is source work, not pin work.
3. **`xutilitydrive.cpp` not renamed** — the import-ordinal decode does not
   yield a name with a second witness (DC3 has 0 occurrences of the candidate).
   Renaming on one-sided evidence would be guessing, and the rename is
   metric-neutral either way.
4. **`0x8283cd20` left spelled `"entry"`** rather than `mainCRTStartup`. The
   existing name is not wrong; under `name_check`, renaming a correct name has
   no upside and a real downside.
5. **3 further xapilib identifications left unpinned** — proven enough to name
   but not to carve a heading around. They cost 0.
6. **Two PRE-EXISTING test failures on main, NOT caused by this lane and NOT
   fixed here.** `python3 -m pytest tools -q` is **324 passed / 2 failed** at
   base `814c6135`:
   `tools/test_alias_group_key.py::test_survivor_identifies_a_group` and
   `tools/test_icf_alias_withdrawal_guard.py::test_the_ledger_keys_do_not_alias_two_groups_together`.
   `scripts/symbol_aliases.json` has **1,632 groups but only 1,631 unique
   survivors**: `?insert@?$list@PAVObject@Hmx@@...` appears at **both**
   `0x823c3ac8` (0 folded, 86 withdrawn — forgives nothing) and `0x823d14c0`
   (4 folded). Present at `eb7fda86~1`, so it predates W16-S. This lane never
   touched `symbol_aliases.json` (`git diff main...HEAD` confirms) and the two
   modules fail identically when run alone. **Neither is in
   `scripts/test_tools_known_bad.txt`, so the runner reports them as NEW
   failures** — a live hole for an alias lane.
7. **The `Server.h` surplus pair** — explicitly out of scope per the brief.
8. **No permuter, no alias additions, no `symbol_aliases.json` edits** of any
   kind this lane.
