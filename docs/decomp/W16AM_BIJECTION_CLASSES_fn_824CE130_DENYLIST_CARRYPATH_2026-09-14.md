# W16-AM — bijection classes, `fn_824CE130`, the `0x82c16aa0` denylist, and the carry path

**Lane:** W16-AM (identification). **Branch:** `w16-am`, based on main `ec6aa875`.
**Worktree:** `/home/free/tmp/wt-w16-am`. **Ruler:** `name_check`, objdiff 4.2.9 `5a51cd51fe0a353f`.

| | matched_functions | matched_code | matched_code_percent |
|---|---:|---:|---:|
| baseline (main `ec6aa875`) | 43,408 | 4,003,484 B | 39.073612 |
| lane tip | **43,413** | **4,004,464 B** | **39.083176** |
| **Δ** | **+5** | **+980 B** | **+0.009564 pp** |

`total_code` 10,246,004 · `total_functions` 69,217. Priced by set-diff of the
`fuzzy == 100` row set (`tools/rowset_snapshot.py`), not by reading a headline.

Five builds, all rc=0 (`~/tmp/rb3_build_w16am_{1..5}.log`).

---

## 0. Two briefed figures that did not survive contact

Applying the house rule *test a briefed figure literally before building on it*, two of
the brief's own premises measured false, and a third (Item 4's) is off by an order of
magnitude. They are recorded here because each one would have sent the next lane down a
drained vein.

1. **"listed under `_bijection_arbitrary` in `scripts/target_symbol_map.json`"** — FALSE.
   `_bijection_arbitrary` holds 1,013 *unrelated* addresses. The five classes are ICF alias
   **groups** in `scripts/symbol_aliases.json` (keys 478, 436, 387, 115, 369).
2. **Candidate counts** — `0x823f0b50` has **4** folded members, not 2 (2 present in our build).
3. **Item 2's premise and AE's recorded lead** — see §2. Both attach the right callee pair
   to the wrong address.

---

## 1. The five arbitrary-bijection classes — landed `547b2a19`

**Predicted:** if any relocation or unwind channel differs between candidates, the bijection
is not arbitrary and the discriminator names the row.

**Measured.** All five survivors' self-pairs REFUTE ("masked bodies DIFFER"), against a
control of 80 random decidable alias-group survivors that refutes at only **23.2 %**
(p ≈ 6.7e-4), with `icf_pair_adjudicate.py --chasetest`'s positive control
(`?Save@ADSRImpl@@`) proving the instrument is not stuck on REFUTED.

| addr | retail size | our survivor size | candidates present | distinct candidate signature |
|---|---:|---:|---:|---:|
| `0x823d3918` (CharLipSync) | 164 | 96 | 20/20 | 1 |
| `0x823f0b50` (SongLayout) | 80 | 100 | 2/4 | 1 |
| `0x8248f1c0` (AmbientOcclusion) | 92 | 96 | 2/2 | 1 |
| `0x82787718` (SongCollision) | 96 | 80 | 3/3 | 1 |
| `0x827d5bb0` (WaveFile) | 80 | 100 | 3/3 | 1 |

⇒ **W16-AB's framing was incomplete in a specific, useful way: in all five classes the map
row names the one candidate retail bytes refute.** The classes are real; the *assignment*
was not merely arbitrary, it was wrong.

**The arbitrariness claim itself is CONFIRMED for the 20-Handle class, with a mechanism.**
One single `(size, masked-body, reloc-target-vector)` signature across all 20, and **none of
the 7 relocations names the owning class** — a `Handle` with no custom cases is structurally
incapable of naming its class. The brief's `.xdata` channel **does not exist on X360 PPC**:
unwind lives in `.pdata` plus an 8-byte EH prefix (a `.text` pointer to `__CxxFrameHandler`
and an `.rdata` pointer to `__ehfuncinfo$…`). Those 20 prefixes differ only by symbol
*name*, over FuncInfo payloads that collapse to **1** distinct masked value.

**Retail-byte proof that sixteen names are simultaneously true in the shipped image:** 16
`.rdata` vtable slots — all slot #6 — plus 1 `.pdata` BeginAddress point at `0x823d3918`
(`BandSongPref`, `CharMeshHide`, `CharEyeDartRuleset`, `CharLipSync`, `RndMeshDeform`,
`RndFur`, `RndCubeTex`, `ColorPalette`, `PerformanceData`, `KeysFx`, `DxCubeTex`,
`MetaMusicManager`, `UIColor`, `UIListWidget`, `UIGuide`, `NgFur`).

**But unit ownership does single one out.** The address lies in CharLipSync.cpp's span
between two CharLipSync symbols, and `.?AVCharLipSync@@` is among the 16 ⇒ the true name is
`?Handle@CharLipSync@@`. That identification exposed a **real source defect**: our
`?Handle@CharLipSync@@` is **412 B** with `parse` / `parse_array` cases, against retail's
generic **164 B** forwarder — a DC3-newer over-implementation.

**Landed the null, not the name.** Renaming to `?Handle@CharLipSync@@` was measured first and
read **−1 fn / −164 B**: `build/45410914/icf_aliases.map` binds the folded spellings to
`823D3918`, and it regenerates only when `symbol_aliases.json` changes — a file barred to this
lane. Removing a name from that address without adding the replacement to the alias group
un-forgives a relocation-name charge elsewhere (`?Handle@UIListSlot@@` fell 100 → 99.87805 in
a *different* unit). Nulling measured exactly **Δ0 / Δ0**: it removes the refuted name, keeps
UIListSlot at 100 via native placeholder forgiveness, and couples into no barred file. The
full repair (source fix + rename + alias membership, worth +164 B) is documented for a
follow-up lane.

**The other four were deliberately NOT nulled.** For them the map name is plausibly *correct*
by unit ownership — each unit's own obj instantiates exactly that template — and the defect is
in **our source / struct layout** (e.g. our `vector<Triangle>::_M_erase` is 96 B vs retail 92 B,
one extra element-size instruction). Those are body leads, not map errors, and nulling them
would destroy the evidence.

---

## 2. `0x822b6538` / `fn_824CE130` — landed `d2d0feac` (+5 fns / +980 B)

**The brief's premise and AE's lead are both wrong, in the same way.** Measured on the built
worktree's retail target objs (post-renamer):

```
0x822b6538  BandCamShot.obj  144 B  -> erase 0x822b5648 + insert 0x822b55e0   (both NAMED)
0x824cf890  PanelDir.obj     144 B  -> erase 0x824cdbf0 + insert fn_824CE130  (anonymous)
```

So `0x822b6538`'s insert callee is **not** anonymous; the anonymous `fn_824CE130` hangs off
`0x824cf890`. AE's commit `1fdcb69e` recorded `0x824cf890`'s callee pair against `0x822b6538`.

### The discriminator is the element destructor

An `erase<list<T>>` body is 108 B and calls exactly `~T` plus `MemOrPoolFreeSTL`. All 23 such
bodies in the image are byte-identical once relocations are masked — which is exactly why an
arbitrary bijection was possible — but **the relocation TARGET names `T`**. A `resize<list<T>>`
(144 B) then inherits `T` from its erase callee, and its second callee is `insert<list<T>>`.
⇒ **The bijection in this family is NOT arbitrary**, and this is the general answer to Item 1's
question applied to a second family.

| address | element dtor it calls | ⇒ true `T` |
|---|---|---|
| `0x824cdbf0` | `??1MatOverride@WorldDir@@` | `MatOverride@WorldDir` |
| `0x824cdb10` | `??1PresetOverride@WorldDir@@` | `PresetOverride@WorldDir` |
| `0x824cdb80` | `??1BitmapOverride@WorldDir@@` | `BitmapOverride@WorldDir` |
| `0x822b56b8` | `??1EventCall@EventAnim@@` | `EventCall@EventAnim` |
| `0x8230e658` | `??1PracticeSectionMapping@SongSectionController@@` | `PracticeSectionMapping@SSC` |
| `0x822b5648` | `??1?$ObjRefConcrete@VRndEnviron@@VObjectDir@@@@` | `Target@BandCamShot` |

The last row is the load-bearing one. `BandCamShot::Target` (`src/system/bandobj/BandCamShot.h`)
has exactly **one** member with a non-trivial destructor — `ObjPtr<RndEnviron> mEnvOverride` —
`Symbol`/`Transform`/`float`/bitfields all being trivially destructible, so `~Target` inlines to
precisely that one call.

### Eight rows corrected

Two of them (`0x824ce130`, `0x8230e658`) were **absent keys**, not JSON-null rows — `.get()`
returns `None` either way, and I recorded the wrong one until the diff showed pure additions.

| address | old | new |
|---|---|---|
| `0x822b6538` | `resize<BitmapOverride@WorldDir>` | `resize<Target@BandCamShot>` |
| `0x824cf890` | `resize<Target@BandCamShot>` | `resize<MatOverride@WorldDir>` |
| `0x824ce130` | *(absent)* | `insert<MatOverride@WorldDir>` |
| `0x823106d8` | `resize<MatOverride@WorldDir>` | `resize<PracticeSectionMapping@SSC>` |
| `0x8230ed80` | `insert<MatOverride@WorldDir>` | `insert<PracticeSectionMapping@SSC>` |
| `0x8230e658` | *(absent)* | `erase<PracticeSectionMapping@SSC>` |
| `0x822b56b8` | `erase<BitmapOverride@WorldDir>` | `erase<EventCall@EventAnim>` |
| `0x824cdb80` | `erase<EventCall@EventAnim>` | `erase<BitmapOverride@WorldDir>` |

The last two are an exact transposition. `0x823106d8` is corroborated a second, independent
way: `?resize@?$ObjList@VPracticeSectionMapping@SongSectionController@@@@` sits at
`0x823109a0`, immediately after it, and `ObjList<T>::resize` forwards to `list<T>::resize`.
Spellings were lifted verbatim from existing rows with a single name fragment substituted, so
MSVC back-reference indices are preserved. Injectivity held — duplicate-name count unchanged at
the 2 pre-existing internal-linkage entries, and `map_name_injectivity_check` is ninja-wired.

### Price — predicted +2..+8 fns / a few hundred bytes; measured **+5 / +980 B**

**9 rows crossed in (1,456 B):**

| bytes | row |
|---:|---|
| +456 | `SongSectionController::??$PropSync@VPracticeSectionMapping@SongSectionController@@@@` |
| +188 | `PanelDir::??4?$list@UBitmapOverride@WorldDir@@…` (operator=) |
| +144 | `BandCamShot::?resize@?$list@UTarget@BandCamShot@@…` |
| +144 | `CharIKFingers::?resize@?$list@VPracticeSectionMapping@SongSectionController@@…` |
| +144 | `PanelDir::?resize@?$list@UMatOverride@WorldDir@@…` |
| +136 | `CharIKFingers::?resize@?$ObjList@VPracticeSectionMapping@SongSectionController@@@@` |
| +108 | `BandCamShot::?erase@?$list@VEventCall@EventAnim@@…` |
| +68 | `BandCamShot::?resize@?$ObjList@UTarget@BandCamShot@@@@` |
| +68 | `system/world/Dir::?resize@?$ObjList@UMatOverride@WorldDir@@@@` |

**4 rows fell out (476 B)** — the un-pairing cost of names that moved: `CharIKFingers`'s
`resize<list<MatOverride>>` (144) and `list<MatOverride>` range ctor (116), `Shockwave`'s
`list<Target@BandCamShot>` range ctor (116), `Group`'s `insert<list<MatOverride>>` (100).

★ **The crossed-in set is the confirmation, not the payout.** Six of the nine are *callers* of
the corrected symbols — the three `ObjList<T>::resize` wrappers and `PropSync<PracticeSectionMapping>`.
A **wrong** name cannot make an unrelated caller row cross to 100 %. This is
"a wrong name is financed by its callers" running in reverse, and it is a stronger signal than
the +980 B.

---

## 3. `0x82c16aa0` denylist entry — landed `a8bc98df`

**The rationale existed; it was never transcribed.** Lane PIN-A adjudicated it in commit
`49b18c95` (2026-08-06) and recorded the reasoning only in the commit message.

⚠ **Methodological trap worth keeping:** `git log -S '82c16aa0'` **does not find** the move
from map-key to `_denylist`, because the occurrence count of the address string stays at 1
across that edit. `git log -G` does. `-S` is why this read as "no rationale on record".

**Re-verified on retail bytes rather than trusting the message.** `band.exe` `.text`
va `0x82270000` raw `0x264e00` ⇒ file offset `0xC0B8A0`; `symbols.txt` gives `size:0x10`:

```
82c16aa0: 3d6082c1  lis  r11, 0x82c1
82c16aa4: 386b6a80  addi r3, r11, 0x6a80      ; r3 = 0x82c16a80
82c16aa8: 4e800020  blr
82c16aac: 00000000  (pad)
```

It **writes** `r3` with a constant code address and returns it, ignoring its incoming
argument. `?Clear@MetaPerformer@@UAAXXZ` is a `virtual void(void)` — it receives `this` in `r3`
and returns nothing. **The name is refuted by the ABI alone**, without needing PIN-A's other
two legs (1.6 MB from the pinned MetaPerformer cluster; only referent is XDK loopback
voice-chat code).

**Verdict: the entry HOLDS. Keep the denial. No removal proposed, so no price to measure.**
Rationale appended to `_denylist_comment` — 1 line changed, 0 address rows touched.

Also recorded: PIN-A's flagged renamer defect ("the renamer does NOT consult `_denylist`") is
**fixed** — honoured since `f3fe9ab1`, and W16-AE's `check_denylist_applied()` (exit 7) now
verifies the refusal reaches the built objs. Live proof on this tree:
`[denylist] OK: 6 denylisted address(es), 3 with a live map string, none named in 3116 target
objects (495665 symbols scanned)`.

---

## 4. The carry-path contradictions — landed `62792c3f` (report only)

**The brief asked for the 8/14/rest split to be derived. It cannot be — the population it
describes is not the population the tool reports.**

```
python3 tools/icf_alias_build.py --merge scripts/symbol_aliases.json \
    --survivor-self-check shape --out ~/tmp/w16am_icf_probe.map
```

(`--out` is required — AE's quoted invocation omits it and exits 2. It was pointed **outside
the repo** so no `icf_alias_*` file was touched; `git status` clean after the run.)

| counter | AE | W16-AM (this tree) |
|---|---:|---:|
| generation pairs REFUSED | 336 | **331** |
| landed groups carried unchanged | 173 | **159** |
| "contradictions" | 38 | — |

AE's own §4 records **336 / 173** for these counters after its equivalence fix, which is
consistent with the 331 / 159 measured here. ⇒ **the "38" was an adjudicated SUBSET, not the
printed population**, and the 8/14/rest split describes 26 members of that 38-member sample.
It should not be carried forward as a figure for the population.

**Confounder ruled out:** 0 of the 168 distinct contradiction addresses are among the 8 rows
this lane renamed in `d2d0feac`.

188 membership records / **168 distinct survivor addresses**:

| class | records | |
|---|---:|---|
| reloc targets differ (after alias-equivalence resolution) | 168 | 148 distinct addrs |
| size differs | 12 | **20 distinct addrs, structural** |
| reloc count differs | 5 | |
| reloc shape differs | 3 | |

The 20 structural ones are decisive: a differing size or relocation count means the two
COMDATs cannot be the same function, so either our port of the survivor spelling is wrong or
the survivor NAME on that address is wrong.

**Two independent cross-confirmations fell out:**

- **4 of the 5 Item-1 bijection addresses** appear in the structural class on *size* grounds
  (`0x823f0b50`, `0x8248f1c0`, `0x82787718`, `0x827d5bb0`) — reproducing §1's
  survivor-self-pair REFUTED result by a **different instrument**. `0x823d3918` is absent
  because `547b2a19` nulled it.
- `0x824cf800` (`resize<Sink@MsgSinks>`) and `0x822a8668` (`resize<OldMMInst>`) are flagged
  here **and** were independently flagged by §2's element-destructor sweep as carrying the
  wrong `T`.

Delivered as **`docs/decomp/W16AM_ALIAS_PROPOSALS_FOR_W16AL.json`** — report-only, never
written to the alias file. It carries the **STLPORT-1 hazard banner** explicitly: a previous
"+8 B" finding of exactly this shape was an artifact of `coff_bodies_ext.py` billing the
successor symbol's EH funclet prefix into the COMDAT span (fixed in `ff832b50`), and a *size*
test cancels a one-sided reader error on both sides — so re-confirm any size delta against the
`.pdata` extent before withdrawing a membership on it.

---

## 5. The pre-existing red test arms — landed `d71ec557`

**`scripts/unicorn_runner/tests/test_prober.py` — FIXED (clear fixture defect).**
`test_format_input_sensitive` asserted `"Equiv fills:"` / `"Div fills:"` while
`prober.format_probe_result` emits `"Equiv inputs:"` / `"Div inputs:"`. **Both spellings landed
in the same commit `06022df7`** (the DC3 port), so this test has never passed since it landed:
DC3's original said `fills`, the ported implementation says `inputs`. Tracking the rename does
not weaken the assertion — it still requires both labels and their content. **11 passed / 1
failed → 12/12.**

**`scripts/test_patch_state.py` — DIAGNOSED, deliberately NOT fixed. The brief's premise is
wrong.** It now reports **25 failures + 2 errors** (brief said 21, "red before AE"). *Every*
one fails on the same line — the test's own **control**,
`self.fx.run("--check", "--emit").returncode == 0`, which returns **7**. Exit 7 is
"DENYLIST NOT APPLIED" from `check_denylist_applied()`, which **W16-AE added in `e63056f0`**;
`test_patch_state.py` has not been touched since `19687598`, which predates it.
⇒ **These arms are red BECAUSE OF AE, not before it, and the count grew 21 → 27.**

The new check is healthy on a real tree (`--check` rc=0 here). The defect is that the sandbox
fixture builds a synthetic tree that cannot satisfy it, so every test fails at its control
before exercising anything. **Not fixed deliberately:** repairing it means either teaching the
fixture to satisfy the denylist check or giving it an opt-out, and both are design decisions on
a tool whose entire purpose is to refuse rather than default. Getting that wrong is worse than
a recorded diagnosis.

---

## 6. NOT done, and why

- **The four non-CharLipSync bijection rows were not renamed.** Their map names are plausibly
  correct by unit ownership; the evidence points at our source/struct layout, not the map.
  Nulling them would destroy evidence for no gain.
- **The `0x823d3918` full repair (+164 B) was not landed.** It needs a source fix (our 412 B
  Handle vs retail's 164 B forwarder) *and* alias-group membership in
  `scripts/symbol_aliases.json`, which is lane W16-AL's file. The null is the
  score-neutral, coupling-free part.
- **~10 further `list<T>` rows remain inconsistent** under the same element-destructor anchor,
  forming a longer permutation chain outside this lane's briefed rows:
  `0x822b6798`, `0x824cf800`, `0x824cf770`, `0x823c40f0`, `0x823296d0`, `0x824a1b40`,
  `0x824a1ab0`, `0x824e1a28`, `0x8246af50`, `0x822a8668`. Several are corroborated
  independently by the carry path (§4). **This is a live, priced-positive vein** — the 8 rows
  here bought +980 B — and it is the single best follow-up from this lane.
- **The 148 reloc-target-only carry-path contradictions were not adjudicated** individually.
  They need retail-byte work per group and the file that would record the outcome is W16-AL's.
- **`scripts/test_patch_state.py` not fixed** — see §5.
- **No edits to** `scripts/symbol_aliases.json`, any `icf_alias_*` file, `config/45410914/splits.txt`,
  any OvershellPanel file, or `src/`. No identification here required a source declaration.

---

## 7. Gates

All run in the worktree, in order, on the final tree.

```
BUILD                                     rc=0   (~/tmp/rb3_build_w16am_5.log)
scripts/verify_ruler_agreement.py --check rc=0   "OK: both objdiff-cli entry points resolve the same ruler."
scripts/verify_objs_patched.py --verify-manifest rc=0
    [denylist] OK: 6 denylisted address(es), 3 with a live map string, none named in 3116 target objects
    [patch-state] OK: 1215 decomp, 3116 target objects match (tree_sha256=b9adcbd63cfe70b1)
```

`NATIVE_GATE_RESULT` line: see the lane's final report (the native gate is the lane's last action).

## 8. Commits

| sha | what |
|---|---|
| `547b2a19` | Item 1 — null the refuted `0x823d3918` row; bijection evidence (Δ0/Δ0) |
| `d2d0feac` | Item 2 — identify `fn_824CE130`, repair 8 shuffled `list<T>` rows (**+5 / +980 B**) |
| `a8bc98df` | Item 3 — record the `0x82c16aa0` denylist rationale, re-verified on retail bytes |
| `62792c3f` | Item 4 — re-derive the carry-path population; AE's 38/26 does not reproduce |
| `d71ec557` | Item 5 — fix the born-red unicorn prober arm; diagnose `test_patch_state.py` |
