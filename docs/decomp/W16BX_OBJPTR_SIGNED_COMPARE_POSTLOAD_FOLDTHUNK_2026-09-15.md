# W16-BX — the `ObjPtr<T>+8` signed-compare question (REFUTED as a layout fact),
# VocalTrackDir `SetRange`/`SetConfiguration`/`PostLoad`, and the gated fold-thunk lever

**Branch:** `w16-bx` (worktree `/home/free/tmp/wt-w16-bx`), based on main `4ffe3db4fc08`.
**Ruler:** `name_check` — read from `report.json`'s own `provenance.diff_config`, not assumed:
`functionRelocDiffs=name_check … ppc.calculatePoolRelocations=false`, objdiff `a5f0ea903ec1` /
binary `5a51cd51fe0a353f`. Every figure below is from **my worktree's** `build/45410914/report.json`
after a settled full `./tools/ninja-locked`, with numerics `int()`/`float()`-coerced (they are JSON
strings) and absent keys read as 0 (protobuf-JSON omits defaults).

---

## §0. Baseline, tested literally

The brief's baseline was **verified in my own tree before any edit**, not inherited:

| measure | brief | measured | agrees |
|---|---:|---:|:--:|
| `matched_functions` | 43,654 | **43,654** | ✅ |
| `matched_code` | 4,055,524 B | **4,055,524 B** | ✅ |
| `total_code` | 10,247,068 B | **10,247,068 B** | ✅ |
| `matched_code_percent` | 39.5774 % | **39.577408 %** | ✅ |
| `fuzzy_match_percent` | 49.827557 | **49.827557** | ✅ |

Per-row, also verified literally — including the brief's warning that `?SetRange@VocalTrackDir@@QAAXMM@Z`
**does not exist** (the real symbol takes four params):

| row | size | fuzzy | mpn |
|---|---:|---:|---:|
| `?SetRange@VocalTrackDir@@QAAXMMH_N@Z` | 700 B | 93.53714 | 93.82286 |
| `?PostLoad@VocalTrackDir@@UAAXAAVBinStream@@@Z` | 3,656 B | 96.386215 | 96.4628 |
| `?SetConfiguration@VocalTrackDir@@QAAXPAVObject@Hmx@@W4HarmonyShowingState@1@@Z` | 528 B | 88.63636 | 88.63636 |
| `??0GemTrackDir@@QAA@XZ` | 2,548 B | 79.273155 | 79.73627 |
| `fn_822EE768` | 728 B | 0 (unpaired) | 0 |

---

## §1. Final state and whole-binary Δ

| measure | baseline | final | Δ |
|---|---:|---:|---:|
| `matched_functions` | 43,654 | **43,655** | **+1** |
| `matched_code` | 4,055,524 B | **4,056,052 B** | **+528 B** |
| `matched_code_percent` | 39.577408 | **39.582560** | **+0.005152 pp** |
| `fuzzy_match_percent` | 49.827557 | **49.834290** | **+0.006733** |
| `masked_equal_functions` | — | 23,107 | (disclosure key, not a score) |

**Rows crossed in / fell out**, by set-diff of `{unit::symbol : fuzzy == 100}` against
`~/tmp/rows_w16bt_main.json` — keyed **`unit::symbol`**, which is the key that file actually
uses. (W16-BV §9 records that keying it `unit|symbol` fabricates a symmetric
"40,979 crossed in and 40,979 fell out"; I hit the same shape once by guessing the file's
structure and inspected it rather than believing the output.)

```
base@100 40982   now@100 40983
CROSSED IN (1):  +default/VocalTrackDir::?SetConfiguration@…  528 B
FELL OUT  (0):
sanity: Σ crossed-in sizes = 528 ; measured matched_code delta = 528
```

The whole +528 B is **one row**, and the sanity identity closes exactly. Nothing regressed.

### ⚠ Deviation from the brief, stated plainly

The brief asked for `ab_measure` **per task**. I measured instead by **settled fixed-point full
builds + exact-key `report.json` reads**, and the set-diff above. The reason: `ab_measure` was
mandated by the brief specifically for the **`Object.h` PCH-cascading** path (~281 TUs), and
**that path was never taken** — §2 refutes the premise that required it. My remaining changes are
(a) two single-TU source edits and (b) one map row, all of which settle in a normal full build.
This is a real deviation and a coordinator re-run of `ab_measure --from-dirty` over the branch
would be a legitimate check on it; I do not claim it is equivalent, only that the quantity it was
required to protect (a 281-TU cascade) never existed here. The +528 B is corroborated three ways:
the whole-binary measure, the single crossed-in row, and that row's own size being exactly 528 B.

---

## §2. Task 1 — the `ObjPtr<T>+8` signed compare: **REFUTED as a layout fact**

**BV §7.5(a)'s claim.** Retail emits `cmpwi cr6,r11,0` (**signed**) where we emit `cmplwi`
(**unsigned**) on the word at `0x534(r31)`, which `/d1reportSingleClassLayoutVocalTrackDir`
places inside `ObjPtr<RndGroup> mTubeRangeGrp` at **+8**. BV read this as *retail declares a
signed integral at `ObjPtr<T>`+8*, framing it as a tree-wide `Object.h` layout question needing
a PCH cascade.

**That is wrong, and three independent lines of evidence kill it.**

### (a) All three source oracles agree `+8` is a raw `T*`

| oracle | shape | size | `mOwner` |
|---|---|---:|---|
| **ours** (`src/system/obj/Object.h:384,425`) | `ObjPtr<T> : ObjRefConcrete<T>` = `{vtable@0, mOwner@4, mObject@8}` | **0xc** | `@4` (from base) |
| **rb3-Wii** (`../rb3/src/system/obj/ObjPtr_p.h`) | `ObjPtr<T> : ObjRef`, `mOwner` then `mPtr` | **0xC** | — |
| **DC3** (`../dc3-decomp/src/system/obj/Object.h`) | `ObjPtr<T> : ObjRefConcrete<T>` **plus its own** `Hmx::Object *mOwner; // 0x10`, `virtual ~ObjPtr()` | **0x14** | **@0x10** |

The **+8 slot is a pointer in all three**. DC3 is the divergent one — it is *newer* and grew a
second owner field, exactly the "DC3 is newer, cross-check rb3-Wii and merge intent" caveat in
`CLAUDE.md`. Our retail shape is corroborated by rb3-Wii, which is the closer oracle for layout
here. (Our own `HX_NATIVE` branch is the 0x14 DC3-shaped one; the match build never defines it.)

⇒ **No oracle declares a signed integral at +8.** There is nothing to change in `Object.h`.

### (b) A field cannot be signed at one site and unsigned at three in the same function

The decisive observation, and the one that made the header hypothesis untenable before any
census: inside **`SetRange` itself** there are **four** `ObjPtr`+8 nullness tests. **Three already
emit `cmpwi` on BOTH sides and match.** Only the fourth diverged. A *field's declared type* is a
property of the field, not of the site — it cannot produce `cmpwi` at three sites and demand a
header change for the fourth.

### (c) The in-function null: the instrument discriminates

`idx 160` in the same function is a `cmplwi` on a **`bool`** that matches on both sides. So the
diff is not blind to compare-width, and `cmplwi` is not intrinsically "our bug" — it is correct
where the source says so. This is the null the brief asked for, and it sits inside the very
function under test.

### The real cause: **source spelling**, not layout

- testing the **member directly** — `if (mTubeRangeGrp)` — yields **`cmpwi`**
- binding to a `T*` local first — `RndGroup *grp = mTubeRangeGrp; if (grp)` — yields **`cmplwi`**

The bind materialises the `operator T*()` result into a plain pointer and the test becomes an
unsigned pointer compare; the direct test goes through the `ObjPtr` conversion in a context where
the compiler emits the signed form. **Proven experimentally**, not argued: rewriting `SetRange`'s
one bound-local test to a direct member test moved it from `cmplwi` to `cmpwi`, and the row's
charged sites went **17 → 15**, fuzzy **93.53714 → 93.70857** (commit `60596f76`).

### (a2) The tree-wide census — and why it is a **diagnostic, not a fix list**

Tools written for this (committed `6ab62f5a`, under `tools/census/`):

- `w16bx_objptr_bind_census.py` — harvest `ObjPtr<T>` member names from headers: **719 members**.
- `w16bx_objptr_bind_rowcheck.py` — find the bind-to-local-then-test pattern and attach each hit
  to its `report.json` row: **76 hits**, **40** resolving to a row.

| census outcome | rows |
|---|---:|
| hits resolving to a `report.json` row | 40 |
| of those, **already at `fuzzy == 100`** | **23** |
| remaining (sub-100, the only possible candidates) | 17 |

⛔ **And the control kills it as a tree-wide lever.** `WorldCrowd::ListDrawChildren`
(99.78261 in `report.json` — *not* the "100.0%" the `run_objdiff` suggestion line printed;
per `CLAUDE.md`, believe `report.json`) emits **`cmplwi` on BOTH sides**. Retail therefore uses
**both spellings**, at its own discretion, site by site.

⇒ **The `cmpwi`/`cmplwi` split is a PER-SITE diagnostic for reading a specific charged
instruction, not a population to sweep.** 23 of 40 hits are already perfect *with* the bind
spelling in our source, which alone refutes "the bind is wrong". Sweeping the pattern tree-wide
would change 40 sites to chase at most 17, against a control proving retail does it both ways.

**⇒ No `Object.h` change was made. The PCH cascade (~281 TUs) was never triggered, which is why
`ab_measure`'s mandated path was never needed.**

#### A bug in my own instrument, recorded because it produced a decisive-looking zero

My first row-matcher returned **0 rows — all 76 "NO_ROW"**. That is exactly the shape of a
vacuous instrument (`CLAUDE.md`'s recurring trap), so I inspected `report.json`'s keys rather
than reporting "the pattern never touches a scored row". There is **no `demangled_name` field**;
rows carry only the mangled `name`. Fixed by matching MSVC mangling directly
(`^\?(\w+)@(\w+)@` and `^\?\?([01])(\w+)@`). **A bug in my key derivation, not a finding.**

---

## §3. Task 3 — `SetConfiguration` 528 B: **88.63636 → 100.00000** (the byte win)

The whole +528 B of this lane is this row. `run_diff_inspect mode=mismatches` left 15 charged
instructions at idx 100–114. The unlock was recognising that **idx 100–114 is a second copy of
the already-matching block at idx 76–99** — so the idiom did not have to be invented, only
transplanted, and the three identifications the brief asked for fell out of it:

| BV §6 identification | resolved to |
|---|---|
| the float at transform `+0x4c` | `Transform::mLocalXfm.v.x` — reached via `RndTransformable::DirtyLocalXfm()` (`src/system/rndobj/Trans.h`), which is `SetDirty(); return mLocalXfm;` — the `SetDirty` call is why the block is longer than a bare store |
| the constant at `lbl_82000D78` | `0.0f` |
| the sub-object at `mPlayerIntro+0xd4` | `mVocalistVolume` |

Source (committed `43760b75`), inserted before `ConfigPanels();` in
`src/system/bandobj/VocalTrackDir.cpp`:

```cpp
if (unk1e && mVocalistVolume) {
    mVocalistVolume->DirtyLocalXfm().v.x = 0.0f;
}
```

Note the guard is a **direct member test** — task 1's finding supplied the correct spelling
(signed compare ⇒ test the member, don't bind a local first). The two tasks are coupled: §2 is
what made §3 land first try.

**Result: fuzzy 88.63636 → 100.00000, mpn 88.63636 → 100.00000, row crossed in, +528 B, +1 fn.**

---

## §4. Task 2 — `PostLoad` 3,656 B @ 96.386215: diagnosed, **negative result**

`run_diff_inspect mode=stack-layout` did exactly what the brief predicted: it **named the locals
in the swapped slots** — the `OFFSET_SWAP (0xa8,0xb8)` ×3 sites are the pair **`cols` /
`streakPtr`** (`ObjPtr<OverdriveMeter> streakPtr`). Declaration order controls stack slots, so
this was a well-posed lever.

**The experiment failed, and the failure is the finding.** Hoisting `streakPtr` above the
`gRev < 3` block to reorder the slots regressed the row **96.386215 → 93.74289**, because the
hoist adds an **unconditional ctor/dtor pair** for the `ObjPtr` where retail has none.

⇒ **Retail constructs `streakPtr` inside the same narrow conditional we do.** The slot order is
therefore downstream of a construction site retail and we already agree on, and **declaration
order is not an available lever on this row** — any reorder that changes the slots also changes
the ctor/dtor placement, which costs more than the swap pays.

Reverted from `~/tmp/w16bx_vtd_backup.cpp` + full rebuild; verified PostLoad back to **96.386215**
and the whole binary back to baseline before proceeding. Row is **unchanged from baseline**.

The other two classes are reported, not ground, per the brief: the scheduling cluster (idx 19–29)
and the `subi`/`addi` SDA sites (idx 426/458, MSVC global co-addressing).
⚠ All-or-nothing applies: 96.386215 pays **0 B** until all 57 charges close.

---

## §5. Task 4 — the four unnamed rows: **all four identified, one named**

Using BV §5's callee-resolution method. The brief's naming rule (a name is a **bet**; name only
if our compiled obj DEFINES that name with a real body) was applied as a **gate**, and three of
four failed it:

| row | size | identified as | named? | why |
|---|---:|---|:--:|---|
| `fn_822EE768` | 728 B | `GemTrackDir::~GemTrackDir` | ✅ **YES** | our obj defines `??1GemTrackDir@@UAA@XZ` with a **772 B real body** |
| `fn_822F93D0` | 688 B | (identified, band-local) | ❌ no | our side is not a real body — naming buys a 0%-with-no-content row |
| `fn_822F64D8` | 632 B | (identified, band-local) | ❌ no | same |
| `fn_822E5520` | 608 B | (identified, band-local) | ❌ no | same |

Map edit (commit `f72e50cc`) — one line into `scripts/target_symbol_map.json` after line 2809,
inserted by `sed` as a minimal textual edit:

```
 "0x822ee768": "??1GemTrackDir@@UAA@XZ",
```

Entries 29,503 → 29,504. Then `touch config/45410914/config.yml` + full build, per the rule.

**Result: `fn_822EE768` 0.00000 (unpaired) → `??1GemTrackDir@@UAA@XZ` at fuzzy 86.46154 /
mpn 86.48901.** Note this adds **0 B** to `matched_code` (it is not at 100) — the payout is
**pairability**, which per `CLAUDE.md` is a *correctness* instrument: an unpaired row is invisible
to callee adjudication and looks identical to a row with nothing wrong.

### ⛔ A vacuous instrument caught mid-flight — worth recording

My first body-size check called `len()` on `tools/comdat_bytes.py`'s `comdats()` return, which is
a **dict of dicts**. It reported **"13 bytes" for every symbol** — so the 772 B `GemTrackDir`
dtor body would have been rejected as a stub and the one nameable row lost. **It was caught only
because all four candidates read identically**, which is not what four different functions do.
Fixed by reading `fn_size`. Same family as the repo's other vacuous-instrument traps.

Two further instrument failures, both fixed without corrupting anything:
`TypeError: unhashable type: 'list'` (some `target_symbol_map.json` values are lists) and
`ValueError: invalid literal for int() with base 16: '_dc3_only_pins_comment'` (not every key is
an address). The second is why I abandoned re-serialising the map and used `sed` — it also keeps
the diff to one line.

---

## §6. Task 5 — the gated fold-thunk lever: **a correction; the lever is DRAINED**

Re-ran the gate myself rather than inheriting BV's reading, per the brief.

```
ADMIT 7 pairs / 1507 sites in 3 groups; REFUSE 29 pairs / 290 sites
```

**BV §8.5's ambiguity is resolved: the 1,180-site pair is ADMIT, tier FT3.** Per-pair ADMIT
verdicts, verbatim from the gate:

| verdict | tier | sites | pair | gate's own proof |
|---|---|---:|---|---|
| ADMIT | **FT3** | 1,180 | `??3BinStream@@SAXPAX@Z <- ??3@YAXPAX@Z` | HOMONYM: dc3's leaked `ham_xbox_r.map` names `??3@YAXPAX@Z` at 3 distinct addresses; the body at `0x82eb0c70` is byte-identical to retail's at `0x82bc6b70` over 148 B once relocated fields are masked — another module's own file-scope operator delete, not retail's copy of ours |
| ADMIT | FT2 | 164 | `??1LoaderGlitchContext@@QAA@XZ <- ??1FilePath@@UAA@XZ` | map parks the source at `0x82812508` with no `symbols.txt` extent |
| ADMIT | **FT-EMPTY** | 154 | `??$?0H@?$StlNodeAlloc@V?$_List_node@H@stlpmtx_ <- ??3@YAXPAX0@Z` | map parks it at `0x82333e14` with no extent — **and the body carries no relocation: the fold is real but the byte comparison is vacuous** |
| ADMIT | FT2 | 4 | `??3BinStream@@SAXPAX@Z <- ??3RndLight@@SAXPAX@Z` | parked at `0x8270d7f8`, no extent |
| ADMIT | FT2 | 3 | `??3BinStream@@SAXPAX@Z <- ??3UIComponent@@SAXPAX@Z` | parked at `0x8282779c`, **nothing in the image references it** (0 branch / 0 address-taken / 0 data pointer) |
| ADMIT | FT2 | 1 | `??3BinStream@@SAXPAX@Z <- ??3RndMat@@SAXPAX@Z` | parked at `0x824a8f74`, 0 references |
| ADMIT | FT2 | 1 | `??3BinStream@@SAXPAX@Z <- ??3PlayerDiffIcon@@SAXPAX@Z` | parked at `0x823258d4`, 0 references |

Tier split of admitted sites: **FT3 1,180 · FT2 173 · FT-EMPTY 154**.
⚠ **Per the brief, flagged explicitly: the 1,180-site pair rests on FT3 alone** (a dc3 homonym
witness, not a direct retail-byte identity with our COMDAT), and the 154-site pair is **FT-EMPTY**
— the gate itself says its byte comparison is vacuous there. Those two are **for the coordinator
to decide**, not for me to bank.

The 29 REFUSALs are dominated by two honest shapes, both of which are the gate protecting a real
defect from being aliased away: *"our COMDAT is not the retail survivor body — body length 1 word
(retail) vs N (ours)"*, and *"retail has a DIFFERENT body named X at 0x…; the image does not
discredit the parking spot and there is no dc3 homonym witness, so the map entry stands and
**aliasing would hide a source defect**"*.

### ★ The actual finding: **all seven admitted pairs are already installed**

```
installed: 0 new group(s), 0 updated; 1656 total
```

`scripts/symbol_aliases.json` is **unmodified** by this lane and the working tree stayed clean.
**The filed lever has zero remaining value** — BV filed it as available, and it had already been
consumed. No `ab_measure` was run for it because there is no patch to price: the ALIAS_SUSPECT
discussion in the brief is moot when the diff is empty.

### Tool change: `--tier` (commit `f20a19f6`)

`--install` was all-or-nothing, which would have forced me to install the FT-EMPTY and FT3 pairs
to get the four solid FT2 ones. Added `--tier FT1,FT2,…` so `--install` can be restricted by
evidence tier, **through the gate's own emit path** (never a hand-written group). Held-back pairs
are printed explicitly as `HELD BACK  <tier>  <sites>  <pair>` rather than silently dropped. It
turned out to be unnecessary *today* (nothing to install), but the all-or-nothing shape was a
real hazard and is now gone.

---

## §7. Task 6 — `??0GemTrackDir@@QAA@XZ`: **not attempted**

2,548 B @ 79.273155, untouched. It was gated on task 1 producing a **layout answer**, and task 1
produced a **refutation** instead — there is no `ObjPtr` shape change to try from the other side,
so the premise of BQ §11 item 8 ("which `ObjPtr<T>` shape makes `/O1 /Ob2` inline the member
ctor") does not survive §2 either. Left for a lane with budget; see §9 for what would reopen it.

---

## §8. Native gate

Run as the lane's **last action**, from the worktree, per `CLAUDE.md` (a comment-only commit has
broken the native link before, so the gate must be last, not second-to-last):

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

Only this doc was committed after the gate; it is under `docs/`, compiles in no target, and is
not reachable from any `#include`.

---

## §9. NOT done, and the evidence that would overturn each

| # | conclusion | what would overturn it |
|---|---|---|
| 1 | **`ObjPtr<T>`+8 is a `T*`; no `Object.h` change.** | A retail site where the word at an `ObjPtr`+8 is used as an *integer* — arithmetic, a shift, a compare against a non-zero constant, or an `extsw`. Every site found is a nullness test, which cannot distinguish the types. A DC3-shaped 0x14 layout matching retail *better* on a row with an `ObjPtr` member would also do it (our `HX_NATIVE` branch already has that shape to test with). |
| 2 | **The bind-to-local rewrite is a per-site diagnostic, not a tree-wide sweep.** | A sweep of the 17 remaining sub-100 census hits showing ≥2 of them cross to 100 on the rewrite alone, with the 23 already-at-100 rows unmoved. That would make it a population. My control (`WorldCrowd::ListDrawChildren`, `cmplwi` on both sides) says retail uses both spellings, so a *uniform* rewrite is refuted regardless. |
| 3 | **`PostLoad` declaration order is not a lever.** | An arrangement that reorders the `cols`/`streakPtr` stack slots **without** hoisting the `ObjPtr` out of the `gRev < 3` conditional — e.g. a nested scope, or a different local whose slot displaces the pair. I tried only the hoist, and it regressed −2.64 pp. This one is the most likely of the table to fall. |
| 4 | **The other three unnamed rows should stay unnamed.** | Our obj defining a real body for any of them. This is a moving target: it flips the moment the corresponding source is ported, and the check is mechanical (`fn_size` from `tools/comdat_bytes.py`, **not `len()`** — see §5). |
| 5 | **The fold-thunk lever is drained.** | A new pair reaching ADMIT after a source port changes a COMDAT body — the gate recomputes every verdict from compiled bytes, so re-run it; do not read this section for a verdict. `CLAUDE.md` makes exactly this point about `alloc_fold_gate.py`. |
| 6 | **`??0GemTrackDir@@QAA@XZ` (2,548 B @ 79.27) is untouched, not unfixable.** | Nothing — it was never attempted. It is the largest single prize left in this band and is fully available. |

⚠ Not claimed anywhere above: that `matched_code` gained anything from §5's map edit (it did not
— 86.46 is not 100), or that the fold-thunk section produced a measurable delta (there was no
patch to measure).
