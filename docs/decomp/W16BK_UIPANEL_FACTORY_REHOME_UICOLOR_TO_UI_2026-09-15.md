# W16-BK — re-home the `UIPanel` factory `0x82802418` from `UIColor.cpp:` to `UI.cpp:`, and the `?NewObject@UIScreen@@` name is on the WRONG ADDRESS

Lane W16-BK, 2026-09-15. Worktree `~/tmp/wt-w16-bk`, branch `w16-bk`, based on
main `312da843` (roadmap-only on top of `0416c2ec`).

**Headline.** The re-home landed as filed by W16-BI. The *unexpected* result is
item 3: BI's identification of `0x828023A0` as `?NewObject@UIScreen@@` is
**correct**, and therefore the map row that currently carries that name —
`0x823f5c20`, scoring **100.0 % on 72 B in `default/UI`** — is **WRONG**. It is
`?NewObject@NetSearchResult@@`. Its 100 % is a **false credit** that survives
only because its ctor callee is unnamed and `name_check` forgives placeholder
targets. I did not act on it (out of bar); it is filed with complete
retail-byte + RTTI evidence below.

---

## 0. Leg A, verified literally (not inherited)

The brief's baseline was **reproduced exactly**, in this worktree, after a
settle build. ⚠ The worktree as handed over was **NOT settled** — my first
`ninja` ran **409 edges**. Reading its `report.json` as leg A without building
would have been an unsettled read, the exact failure CLAUDE.md forbids.

| key | leg A (measured) | brief said |
|---|---:|---:|
| `matched_functions` | 43,552 | 43,552 ✓ |
| `matched_code` | 4,040,380 | 4,040,380 ✓ |
| `matched_code_percent` | 39.42962 | 39.4296 ✓ |
| `total_functions` | 69,240 | 69,240 ✓ |
| `total_code` | 10,247,068 | 10,247,068 ✓ |
| `masked_equal_functions` | 23,076 | 23,076 ✓ |
| `fuzzy_match_percent` | 49.699745 | — |

Provenance read from `report.json` (never inferred): objdiff **4.2.9**,
`tool_binary_hash 5a51cd51fe0a353f`, `tool_commit a5f0ea903ec1`, ruler
**`functionRelocDiffs=name_check`**. ⚠ `provenance.diff_config` is a **list of
`"k=v"` strings**, not a dict — a `.get('functionRelocDiffs')` on it raises.

Quiescence: builds 2 and 3 each ran **7** edges, all always-run checks
(`CHECK ICF-ALIAS MAP`, `CHECK SPLIT CURRENT`, `CHECK MAP NAME-INJECTIVITY`,
`CHECK TARGET OBJS RENAMED`, `PROGRESS`), and `git status config/45410914/` was
clean ⇒ `symbols.txt` is at a fixed point.

**Anti-vacuity for every COFF/name read in this document:**
`[renamed-check] 25837/29358 map names present in 3113 target objs = 88.0%`.
A reflinked pre-renamer tree reads ~0 %, so the pre-compile renamer has run and
a *negative* name lookup here is informative rather than structural.

**Baseline control.** My own leg A rowset set-diffs against the brief's
`~/tmp/rows_w16bh_main.json` (copied to `rows_w16bk_base.json`, original
untouched) at **CROSSED IN 0 / FELL OUT 0 / all four measures Δ0** — so the two
baselines are the same measurement and either may be used. 40,887 rows at
`fuzzy == 100`.

---

## 1. Block census on retail bytes — what actually lives in `0x82802080–0x828024B0`

Method: retail bytes only (`tools/retail_body.py` over
`orig/45410914/band.exe`), plus **RTTI Complete Object Locator** resolution for
class identity. Row identity was fixed by matching the `report.json` row order
to the `symbols.txt` address order **and the size sequence**
(40,124,84,48,252,32,100,120,184,32,72,40,100,40 — an exact 14/14 match), so no
row is attributed by guess.

The block's tail is **two `[8 B EH prefix][factory][40 B unwind funclet]`
groups**, a structure the brief did not know about:

| addr | size | fuzzy | what it is (retail-byte evidence) | correct home |
|---|---:|---:|---|---|
| `0x82801F80` | 40 | 100 | `?SetColor@UIColor@@` | UIColor |
| `0x82801FA8` | 124 | 100 | `?Copy@UIColor@@` | UIColor |
| `0x82802028` | 84 | 100 | `??0UIColor@@IAA@XZ` | UIColor |
| `0x82802080` | 48 | 100 | `?ClassName@UIColor@@` | UIColor |
| `0x828020B8` | 252 | 100 | `?SetType@UIColor@@` | UIColor |
| `0x828021B4` | 32 | 100 `me` | funclet | UIColor |
| `0x828021D8` | 100 | 100 | `?Save@UIColor@@` | UIColor |
| `0x82802240` | 120 | **0** | **`UIColor::Load`** — `bl ?ReadEndian@BinStream@@`, `bl ?Load@Object@Hmx@@`, `bl ??5@YAAAVBinStream@@AAV0@AAVColor@Hmx@@@Z` | UIColor (**stays**) |
| `0x828022C0` | 184 | 100 | `?SyncProperty@UIColor@@` | UIColor |
| `0x82802378` | 32 | 100 `me` | funclet | UIColor |
| `0x82802398` | 8 | — | `except_data` = EH prefix of `0x828023A0` | boundary |
| `0x828023A0` | 72 | **0** | **`?NewObject@UIScreen@@`** — `li r3,0x40` (=`sizeof(UIScreen)`), `bl 0x827bd2f0` (operator new), `bl 0x827f1e90` | **UI.cpp** |
| `0x828023E8` | 40 | 93.4 `me` | unwind funclet of `0x828023A0` (`lwz r3,80(r31)`; `bl 0x8240ddb0` = operator delete) | **UI.cpp** |
| `0x82802410` | 8 | — | `except_data` = EH prefix of `0x82802418` | **UI.cpp** |
| `0x82802418` | 100 | **0** | **`?NewObject@UIPanel@@`** — `li r3,0x68` (=104=`sizeof(UIPanel)`), operator new, `bl 0x82812920` = `??0UIPanel@@QAA@XZ` | **UI.cpp** |
| `0x8280247C` | 40 | 93.4 `me` | unwind funclet of `0x82802418` | **UI.cpp** |
| `0x828024A8` | 8 | — | `except_data` = EH prefix of `0x828024B0`, which is **already** `UI.cpp:`'s | **UI.cpp** |

Funclet attribution is not by adjacency alone: the factory frames are
`addi r31,r1,-112` + `stw r3,80(r31)` and each funclet is
`addi r31,r12,-112` + `lwz r3,80(r31)` + `bl` operator delete — the same frame,
reloading the same saved allocation to free it on a ctor throw.

### Why the factories belong to `UI.cpp` — three independent lines

1. **Source.** `src/system/ui/UI.cpp` is the **only** TU that registers them:
   line 963 `REGISTER_OBJ_FACTORY(UIScreen)`, **964 `REGISTER_OBJ_FACTORY(UIPanel)`**,
   968 `REGISTER_OBJ_FACTORY(UIColor)`. `X::NewObject` is a header-inline static,
   so it is emitted only in a TU that references it.
2. **Our objects.** `UI.obj` defines **10** `?NewObject@…` COMDATs, all
   `IMAGE_COMDAT_SELECT_ANY`, including `?NewObject@UIPanel@@SAPAVObject@Hmx@@XZ`
   and `?NewObject@UIScreen@@SAPAVObject@Hmx@@XZ`.
   **`UIColor.obj` defines 58 function symbols and NOT ONE `NewObject`.**
   ⇒ the 0 % on `0x82802418` has a single complete cause: *the unit it is pinned
   to cannot define the name*. No source defect is involved.
3. **Retail neighbourhood.** `default/UI`'s already-pinned `0x8280xxxx` blocks
   are UI.obj's COMDAT run, and they already score 100 % on
   `?NewObject@UIColor@@` (84 B), `PanelDir` (112), `Screenshot` (112),
   `UITrigger` (112), `UIGuide` (84), `UIFontImporter` (108). Note
   `?NewObject@UIColor@@` itself is in **`default/UI`**, not in `default/UIColor`
   — the strongest possible confirmation that a factory's home is `UI.cpp`.
   ⚠ And `0x828024B0`/`0x82802508`, the 88+32 B pair immediately after my new
   boundary, are **not** factories: `0x828024B0` forms the `.rdata` string
   `"UIPicture"`, calls `??0Symbol@@QAA@PBD@Z` and sets a guard bit at
   `0x82e07808` ⇒ it is a **dynamic initializer** (`??__E`-class) for a static
   class-name `Symbol`, and `0x82802508` is its matching atexit destructor. Same
   TU, different COMDAT kind.

### Boundary decision

**`0x828023A0`.** `UIColor.cpp:` keeps `0x82802080–0x828023A0`; `UI.cpp:`
receives a new adjacent block `0x828023A0–0x828024B0`.

* Both factory groups move, not just the UIPanel one, because both are UI.obj
  COMDATs on the evidence above. Leaving one behind would be knowingly wrong,
  and it makes the `0x828023A0` follow-up a **map-only** edit later.
* The cut is at a symbol start (`except_data_827DCEC0` ends exactly at
  `0x828023A0`), so dtk cannot "end within symbol".
* The EH prefix `0x82802398` stays with the **preceding** block, matching the
  file's existing convention — `0x828024A8` is today the last 8 bytes of
  `UIColor.cpp:`'s block and is the prefix of `UI.cpp:`'s `0x828024B0`. Prefixes
  are not function rows and are not in `total_code`, and the empirical proof that
  a prefix outside the block is harmless is that every 100 % factory row listed
  above already has its prefix in a neighbouring block.
* A **separate** `.text` line, not a merge with `0x828024B0–0x82802530`: the
  minimal edit, and `UI.cpp:` already uses adjacent separate blocks throughout.
* `UIColor.cpp:` is **not** drained (keeps `0x82801F80–0x8280207C` and
  `0x82802080–0x828023A0`), so the vanishing-unit hazard does not apply.

---

## 2. PRE-REGISTERED PREDICTION

*Committed before the splits edit was made — this section's commit precedes the
edit's commit on `w16-bk`, so it cannot have been fitted to the result.*

Four function rows + two `except_data` objects move `default/UIColor` →
`default/UI`.

| # | claim | predicted |
|---|---|---|
| 1 | `?NewObject@UIPanel@@SAPAVObject@Hmx@@XZ` (100 B) | 0 → **100** ⇒ **+1 fn / +100 B** |
| 2 | `fn_828023A0` (72 B) | 0 → **0**, Δ0 |
| 3 | `fn_828023E8`, `fn_8280247C` (40 B, 93.4, `masked_equal`) | central **Δ0**; band ±40–80 B |
| 4 | collateral funclet re-pairing in UI.obj's pool | not predicted; down to −40 B possible |
| 5 | `total_functions` / `total_code` | **UNCHANGED** 69,240 / 10,247,068 |
| 6 | `masked_equal_functions` | 23,076 ± a few |
| 7 | `default/UIColor` unit | 14 fns/9 matched/1,268 B → **10 fns/9 matched/1,016 B** |
| 8 | `default/UI` unit | 200/133/23,612 → **204/134/23,864 B** |
| 9 | whole binary | 43,552 → **43,553**; 4,040,380 → **4,040,480 B**; 39.42962 → **≈39.43060 %** (+0.000976 pp) |

Basis for (1): `UI.obj` defines the name (verified above); W16-BI measured our
COMDAT **RAW-identical at 23 of 25 words**, the two differing words being the
only reloc-bearing ones, with operator new alias-forgiven (group 1546) and
`??0UIPanel@@QAA@XZ` exactly map-named at `0x82812920`.

Basis for (2): the name it deserves is already held by `0x823f5c20`, and
**two map rows may never carry one name**, so it stays a forgiven placeholder
wherever it is pinned. Re-homing it buys **pairability for later**, not bytes
now.

Basis for (3): our funclets are anonymous `__unwind$<ordinal>` (208 of them in
UI.obj) and can pair **only by byte signature**, which is why these read 93.4 %
`masked_equal` today. At 93.4 they contribute **0 functions and 0 bytes**, so
the downside is structurally bounded at zero and any crossing is unbanked
upside. The ±40 B band is the mechanism CLAUDE.md records for `b341d7ab` (a
40 B EH funclet re-paired to a different byte-equal counterpart in a larger
merged pool).

Basis for (5): a re-home creates and destroys no function row and changes no
size. **If `total_code` or `total_functions` moves, my model is wrong** and the
`.pdata` re-derivation or a phantom `type:label` row is the cause — I must
explain it rather than smooth it.

---

## 3. MEASURED — the re-home

`splits.txt`, two `.text` lines, nothing else by hand:

```diff
 UIColor.cpp:
-	.text       start:0x82802080 end:0x828024B0
+	.text       start:0x82802080 end:0x828023A0
 UI.cpp:
 	.text       start:0x823F4A30 end:0x823F5C98
+	.text       start:0x828023A0 end:0x828024B0
 	.text       start:0x828024B0 end:0x82802530
```

dtk then re-derived `.pdata` **by itself** and the `[split-guard]` edge fired on
build 1 exactly as designed ("THE SPLIT REWROTE ITS OWN INPUT"), rc=1. Recovery
was one build, as its message says. What dtk wrote is a **geometric corroboration
of the move** and was committed as it came out:

```diff
 UIColor.cpp:
-	.pdata      start:0x82245D08 end:0x82245D60      (0x58)
+	.pdata      start:0x82245D08 end:0x82245D40      (0x38)  -- shrank 0x20
 UI.cpp:
+	.pdata      start:0x82245D40 end:0x82245D60      (0x20)  -- gained 0x20
```

`0x20` = 32 B = **4 `.pdata` entries × 8 B = exactly the 4 function rows I
moved**. Fixed point proved by `sha256sum` of `splits.txt` **and** `symbols.txt`
across a further build (7 always-run edges, `symbols.txt` not even dirty vs git).

### Prediction vs measurement

| key | leg A | leg B | Δ measured | Δ predicted | verdict |
|---|---:|---:|---:|---:|---|
| `matched_functions` | 43,552 | **43,555** | **+3** | +1 | **miss, +2 upside** |
| `matched_code` | 4,040,380 | **4,040,560** | **+180 B** | +100 B | **miss, +80 B upside** |
| `matched_code_percent` | 39.429620 | **39.431377** | +0.001757 pp | ≈+0.000976 | follows bytes |
| `total_functions` | 69,240 | 69,240 | **+0** | +0 | **EXACT** |
| `total_code` | 10,247,068 | 10,247,068 | **+0** | +0 | **EXACT** |
| `masked_equal_functions` | 23,076 | 23,078 | +2 | ±few | consistent |
| `fuzzy_match_percent` | 49.699745 | 49.700764 | +0.001019 | — | — |

Set-diff (`tools/rowset_snapshot.py`), the byte ledger:

```
CROSSED IN : 3 rows, 180 B
   +    100 B  default/UI::?NewObject@UIPanel@@SAPAVObject@Hmx@@XZ
   +     40 B  default/UI::fn_828023E8
   +     40 B  default/UI::fn_8280247C
FELL OUT   : 0 rows, 0 B
NET bytes  : +180
```

Per-unit, against the pre-registered numbers:

| unit | predicted (fns, matched, code, matched_code) | measured | |
|---|---|---|---|
| `default/UIColor` | (10, 9, 1,016, 896) | **(10, 9, 1,016, 896)** | **EXACT** |
| `default/UI` | (204, 134, 23,864, 11,692) | (204, **136**, 23,864, **11,772**) | fns/code exact; matched +2 |

### The prediction miss is the most useful line here

I predicted the two 40 B funclets at **Δ0 central, with an explicitly
pre-registered upside band**. They both crossed, `93.4 → 100`, `+2 fns / +80 B`.
My *reasoning* for the band was wrong even though the direction was right: I
attributed a possible crossing to luck in a larger byte-signature pool. The
measured mechanism is sharper and reusable —

> **`fn_828023E8` reached 100 % while ITS PARENT `fn_828023A0` IS STILL 0 %.**

Our funclets are anonymous `__unwind$<ordinal>` (208 of them in `UI.obj`; there
is **no** `__unwind$?NewObject@…` spelling), so a funclet can pair **only by byte
signature, and only within its own unit's pool**. A funclet pinned into a unit
whose obj does not emit its parent can therefore only ever find a *near-miss*
counterpart — which is exactly the 93.4 % both rows sat at inside
`default/UIColor`. Re-homing them into the unit that **does** emit the parent
makes an exact counterpart available, **independently of whether the parent row
itself pairs**. The two retail funclets are also identical to each other modulo
one `bl` displacement that resolves to the same target (`0x8240ddb0`, the ICF
survivor of global `operator delete`), so under relocation normalisation they are
one body.

⇒ **Reusable rule: a factory-group re-home pays its unwind-funclet bytes even
when the factory's own name is blocked.** Price a factory re-home as
`parent + Σ funclets`, not `parent`. This lane's realised split was 100 B parent
+ 80 B funclets — **44 % of the yield was in the funclets I did not bank.**

The downside was structurally zero rather than luckily absent: at 93.4 % the rows
contributed **0 functions and 0 bytes**, so they could not lose anything. The
feared collateral (an incumbent `UI.obj` funclet displaced by the arrivals, the
−40 B mechanism CLAUDE.md records for `b341d7ab`) **did not occur** —
`FELL OUT: 0 rows`.

### Side finding — `default/UIColor` is now one function from completion

10 fns / 9 matched / 1,016 B, matched_code 896 B. The single hold-out is
**`fn_82802240`, 120 B, fuzzy 0 — `UIColor::Load`**, identified on retail bytes
(`bl ?ReadEndian@BinStream@@QAAXPAXH@Z`, `bl ?Load@Object@Hmx@@UAAXAAVBinStream@@@Z`,
`bl ??5@YAAAVBinStream@@AAV0@AAVColor@Hmx@@@Z`). It is un-named in the map and
`UIColor.obj` does define `UIColor` methods, so this is a **naming + body**
question, not a re-home. Filed, not taken (no map bar for that row, and the brief
scoped me to `0x828023a0`/`0x82802418`).

---

## 4. `0x828023A0` adjudicated — NOT named, and the reason is a map defect elsewhere

**Verdict: `0x828023A0` IS `?NewObject@UIScreen@@SAPAVObject@Hmx@@XZ`. I did not
name it, because the name is currently held by a DIFFERENT and WRONG address.**

The brief's hypothesis was that the surviving twin must be "the factory of a
different UI class", since reloc-identical bodies fold. The premise is right and
the conclusion is backwards: **the relocations do differ, and it is the
`0x823f5c20` row that belongs to the other class.**

### The two 72 B bodies

Byte-for-byte identical **except one `bl` target**:

| | `0x828023A0` (ours, fuzzy 0) | `0x823f5c20` (named `?NewObject@UIScreen@@`, **fuzzy 100**) |
|---|---|---|
| allocation | `li r3, 0x40` (64) | `li r3, 0x40` (64) |
| allocator | `bl 0x827bd2f0` (operator new) | `bl 0x827bd2f0` (operator new) |
| **ctor** | **`bl 0x827f1e90`** | **`bl 0x823f5a90`** |

⚠ **`sizeof` cannot discriminate them** — the compiler says
`sizeof(UIScreen) = 64 (0x40)`, so *both* are consistent with `UIScreen` on the
allocation immediate alone. **The ctor relocation is the only discriminator**, and
this is precisely the trap the brief warned about, in mirror image.

### Class identity via RTTI — zero map dependence

Both callees are constructors (each calls `??0Object@Hmx@@QAA@XZ` and then stores
its own vptr into `0(this)`). Reading the stored vtable's **Complete Object
Locator** at `vt-4` → `pTypeDescriptor+8`:

| ctor | vtable stored | `??_R4` COL → type descriptor name |
|---|---|---|
| `0x827f1e90` | `0x82120244` | **`.?AVUIScreen@@`** |
| `0x823f5a90` | `0x820599d4` | **`.?AVNetSearchResult@@`** |

⇒ `0x828023A0` constructs a **UIScreen** ⇒ it **is** `?NewObject@UIScreen@@`.
⇒ `0x823f5c20` constructs a **NetSearchResult** ⇒ its map name is **WRONG**.

Three further facts corroborate the `NetSearchResult` half, all from retail bytes
against our own header `src/network/net/NetSearchResult.h`:

* the ctor does `addi r3, r30, 52` then `bl ??0String@@QAA@XZ` — the header
  documents `String mHostName; // 0x34` = **52** ✓
* it does `stw r3, 40(r30)` — the header documents
  `SessionData *mSessionData; // 0x28` = **40** ✓
* `0x40` = 64 is then `sizeof(NetSearchResult)` ✓ (the compiler cannot confirm
  this one: `NetSearchResult.cpp` is **not in `objects.json`**, so
  `/d1reportSingleClassLayout` emits nothing for it — stated as a gap, not
  smoothed over)

The map's `0x827f1e90 = ??0UIScreen@@QAA@XZ` is therefore **right**, and it is the
corroborating half rather than the evidence — the RTTI read does not consult it.

### Why the wrong row scores a clean 100 %

`0x823f5a90` is **absent from `scripts/target_symbol_map.json`**. Under the
shipped `name_check` ruler objdiff **forgives a placeholder target**
(`is_placeholder_symbol_name`), so the one relocation that separates the two
bodies is **not charged**; the other (`operator new`) is alias-forgiven by group
1546. Zero charged sites ⇒ **100.0 %**.

⇒ **the row is at 100 % *because* its callee is unidentified.** Naming
`0x823f5a90` as `??0NetSearchResult@@QAA@XZ` would immediately drop it below 100
by exposing a genuine wrong-callee charge against our `??0UIScreen@@`. This is
CLAUDE.md's "naming an anonymous address pays in **bug exposure**, not bytes",
with the exposed bug being a **wrong map name** rather than wrong source.

### Why I left `0x828023A0` unnamed

Naming it is **mechanically blocked**, not merely discouraged. I proved the gate
discriminates rather than assuming it: injecting
`"0x828023a0": "?NewObject@UIScreen@@SAPAVObject@Hmx@@XZ"` makes
`tools/map_name_injectivity.py` exit **rc=1** —

```
MAP NAME-INJECTIVITY VIOLATED: 1 name(s) claimed at 2 addresses.
  ?NewObject@UIScreen@@SAPAVObject@Hmx@@XZ
      0x823f5c20  UI.cpp
      0x828023a0  UI.cpp
```

— and that checker is a wired build edge (`CHECK MAP NAME-INJECTIVITY`), so the
duplicate would have failed the build. The map was then restored **byte-exactly**
(`sha256sum -c` OK, `git status` clean) and the checker is green again: *29,342
applied rows, 29,341 distinct names, injective*. **This lane changed no map row.**

Freeing the name requires correcting `0x823f5c20` — a map row **outside my bar**.

### ★ The reusable finding: injectivity is necessary and NOT sufficient

`map_name_injectivity.py` is passing, and it is passing **truthfully** — the map
*is* injective. The defect here is a **correctly-unique name attached to the wrong
member of a byte-twin pair**, which injectivity is structurally incapable of
seeing: it audits *how many* addresses claim a name, never *which*. The tool's own
docstring describes the neighbouring hazard ("a wrong name on a byte-twin VA reads
a clean 100 %") and its remedy text even names the right instrument ("for
byte-twin template bodies the relocations separate them where the bytes cannot")
— so the gap is in coverage, not in understanding. A checker that could catch this
class would have to compare each named row's **relocation targets** against the
name's expected callees; the `?NewObject@X@@` family is an ideal first population,
because the expected ctor callee is mechanically derivable from the class name.

### Filed for a follow-up lane (map-only, two rows, coupled)

Do **not** do half of this — renaming either row alone re-breaks injectivity.

1. `0x823f5c20` → the correct `NetSearchResult` factory spelling. ⚠ The exact
   spelling is **not** proven: the header declares `static NetSearchResult *New();`
   (not `NewObject`), so the leading candidate is
   **`?New@NetSearchResult@@SAPAV1@XZ`**, not `?NewObject@NetSearchResult@@…`. My
   *proven* claim is only "this row constructs a `NetSearchResult` and is not
   `UIScreen`'s". Settle the spelling before writing it.
2. `0x828023A0` → `?NewObject@UIScreen@@SAPAVObject@Hmx@@XZ`.
3. Optionally `0x823f5a90` → `??0NetSearchResult@@QAA@XZ` (264 B, currently
   fuzzy 0 in `default/UI`).

**Predicted sign: NET NEGATIVE on the headline, positive on accuracy.**
`0x823f5c20` loses its false 100 % (**−1 fn / −72 B**) because **no obj of ours
defines any `NetSearchResult` factory** (`NetSearchResult.cpp` is unwired), so it
cannot pair under a true name. `0x828023A0` gains **+1 fn / +72 B** *if* it pairs
— and it now sits in `default/UI`, whose obj **does** define
`?NewObject@UIScreen@@`, which is the whole reason I moved it here even though the
move bought 0 bytes today. Expected net ≈ **0 bytes, one false credit retired, one
real match installed**. Per the standing directive (accuracy over headline), that
is a win even at −72 B.

### Also filed — `UI.cpp:`'s block `0x823F4A30–0x823F5C98` looks MIS-PINNED

Reported, not touched. It is 4,712 B pinned to `UI.cpp:` and it contains
`NetSearchResult`'s constructor (`0x823F5A90`, 264 B, fuzzy 0) and its factory
(`0x823f5c20`). Every named `NetSearchResult` map row clusters **immediately
below** it — `?AllocateNetSearchResults@SessionSearcher@@` `0x823eae00`,
`?GetNextResult@SessionSearcher@@` `0x823eaee0`,
`?UpdateSearchList@SessionSearcher@@` `0x823eb5d8` — and essentially every row in
the block reads **fuzzy 0**, the sole exception being the false credit above. That
is the signature of a block attributed to the wrong TU (a session-search /
`NetSearchResult` unit). I did **not** act: it is a 4,712 B re-home into a TU we
may not compile, it would strip the one 100 % row, and it is far outside this
lane's scope. It is coupled to item 1 of the filing above and should be taken
with it.

---

## 5. W16-BE's `UIPanel` recommendation annotated as WITHDRAWN

Commit `67a26c7c`, **48 insertions / 0 deletions** — additive only; BE's original
text is a dated record and was not rewritten. Two dated
`> **CORRECTION (2026-09-15, W16-BK):**` blocks: one after the `UIPanel` paragraph
(BE line 124, "I recommend it be dispatched as one.") and one after the NOT-done
entry (BE line 279, "Recommend a struct-layout lane.").

I **re-verified BE's numbers myself** rather than relaying W16-BI, because BE's row
is at a **different address** than the one this lane worked (`0x8268b9a0`, not
`0x82802418`) and that distinction is the whole correction:

| | address | allocation | ctor called | compiler `sizeof` |
|---|---|---|---|---|
| what BE read | `0x8268b9a0` = `?NewRemoteBandUser@BandUser@@SAPAVRemoteBandUser@@XZ` | `li r3,0x108` (**264**) | `bl 0x8268b4e8` = `??0RemoteBandUser@@QAA@XZ` | `RemoteBandUser` = **264 (0x108)** ✓ |
| the real row | `0x82802418` = `?NewObject@UIPanel@@SAPAVObject@Hmx@@XZ` | `li r3,0x68` (**104**) | `bl 0x82812920` = `??0UIPanel@@QAA@XZ` | `UIPanel` = **104 (0x68)** ✓ |

Both of BE's observations are **accurate readings of the wrong function**:

* the **264** is `sizeof(RemoteBandUser)`, confirmed exactly by
  `scripts/harvest/class_layout_report.py RemoteBandUser` in this worktree;
* `??2CriticalSection@@SAPAXI@Z` at `0x827bd2f0` is the **ICF-survivor spelling of
  the global `operator new`** (alias group 1546, 123 members) — *every* factory in
  this census calls it — and not an embedded `CriticalSection` member.

⇒ the `264 − 104 = 160` bytes of missing members **do not exist**;
`sizeof(UIPanel) == 104` is confirmed by the compiler **and** by retail's own
immediate at the real factory. **Our header was right and retail agrees with it.**
A struct-layout lane would have hunted 160 bytes of nothing — and the reason it
would have is that **the row was diagnosed before it was identified**.

`docs/INDEX.md` was checked: it lists **neither** `W16BE` nor any `UIPanel` layout
claim, so no INDEX edit is warranted (the brief conditioned one on such a listing
existing).

---

## 6. Report-only sweep — other factories pinned outside their class's TU

**Named population: DRAINED BY THIS LANE.** Of **211** `?NewObject@…` rows in
`report.json`, leg A had **exactly 1** below 100 % — the `UIPanel` row — and leg B
has **0**. So the brief's literal question ("other `NewObject@*` map rows that read
0/low") answers **none remain**, and the up-to-20 list is empty.

Since a name-keyed query is structurally blind to an **unnamed** factory (my own
`fn_828023A0` is invisible to it), I extended the sweep on the retail-byte
signature instead: `fn_*` rows at fuzzy 0, 56–152 B, containing `bl 0x827bd2f0`
(operator new) preceded by an `li r3,<imm>` and followed by another `bl`. Of
**5,396** candidates, **38** match the factory shape; **19** have a map-named ctor,
so the class is identifiable. The detector finds `fn_828023A0` → `??0UIScreen@@`
as a **positive control**, and its false positives are visible and honest
(`memcpy`, `json_object_array_get_idx`, `?Begin@StreamChecksumValidator@@` as the
"ctor") — call it ~50 % precision on shape alone.

The decisive column is the one that diagnosed this lane's row: **does the row's own
unit obj define the implied factory name?**

| unit | row | size | implied class | unit obj defines it? | our obj(s) that do |
|---|---|---:|---|---|---|
| `band3/game/Game` | `fn_82677BD0` | 144 | `VocalGuidePitch` | no | **(none)** |
| `ExternalMic` | `fn_82B67A00` | 120 | `ExternalMic` | no | **(none)** |
| `band3/meta_band/PrefabMgr` | `fn_825557E8` | 120 | `PrefabMgr` | no | **(none)** |
| `JsonUtils` | `fn_82B82260` | 108 | `JsonArray` | no | **(none)** |
| `Flow` | `fn_82574D90` | 100 | `NextSongPanel` | no | `MetaPanel.obj` |
| `Gesture` | `fn_823134F8` | 100 | `CrowdAudio` | no | `CrowdAudio.obj` |
| `MusicLibrary` | `fn_8253ABB8` | 96 | `MusicLibraryStore` | no | **(none)** |
| `EventTrigger` | `fn_8274DF40` | 96 | `MsgSource` | no | `Dir.obj`, `MeshDeform.obj`, `Cache_Xbox.obj`, `Shockwave.obj` |
| `synth_xbox/FxSendPitchShift` | `fn_82B6A0E8` | 72 | `PitchShiftEffect` | no | **(none)** |
| `Gesture` | `fn_82768A98` | 72 | `TextFile` | no | `DataUtl.obj` |
| `MidiInstrument` | `fn_823E3E90` | 72 | `VoiceDataMsg` | no | **(none)** |
| `network/net/NetSession` | `fn_823E3C88` | 72 | `NewUserMsg` | no | **(none)** |
| `SetlistMergePanel` | `fn_825AAFF0` | 72 | `LockResponseMsg` | no | **(none)** |

**Read this sweep conservatively.** `0 of 13` have the name in their own unit's
obj — so all 13 are *structurally* the same disease as `0x82802418`. But only
**4 rows / 368 B** have the implied name emitted by **any** obj of ours, i.e. only
those four are **re-home-shaped**; the other 9 need the factory *emitted at all*
(a source `REGISTER_OBJ_FACTORY` we do not have), which is source work, not a pin
move. And each of the four is still **a bet**, not a booked gain: the implied
signature `SAPAVObject@Hmx@@XZ` is assumed, the class is inferred from the ctor
reloc, and `MsgSource` is the weakest of them (a base class whose derived ctor may
have inlined). **368 B is an upper bound on a 4-row vein, not a forecast** — and
this lane's own realised yield came 44 % from funclets that no name-keyed or
shape-keyed sweep would have listed.

⇒ Cheapest next step, if anyone funds this: the two `Gesture` rows
(`CrowdAudio` 100 B, `TextFile` 72 B) sit in one unit with their names in named
objs, so they are one re-home + two map rows, priced exactly like this lane.

---

## 7. Gates

Brief's order, native **last**. All in `~/tmp/wt-w16-bk`.

```
./tools/ninja-locked                                          rc=0  (7 edges, quiescent)
python3 scripts/verify_ruler_agreement.py --check              rc=0
  OK  ppc.calculatePoolRelocations = false
  OK: both objdiff-cli entry points resolve the same ruler.
python3 scripts/verify_objs_patched.py --verify-manifest        rc=0
  [denylist] OK: 6 denylisted address(es), 3 with a live map string, none named
            in 3113 target objects (495609 symbols scanned)
  [patch-state] OK: 1215 decomp, 3113 target objects match
            (tree_sha256=c6474b1b6f771e4a)
python3 tools/icf_alias_finder.py --validate                   rc=0
  VALIDATE: PASS -- 1404 map-consistent, 247 tolerated, 0 contradicted, 1652 total
```

The alias total is **1,652, unchanged** — this lane installed, withdrew and
touched **no** alias group. `CHECK MAP NAME-INJECTIVITY` passes inside the build
at *29,342 applied rows / 29,341 distinct names*, identical to leg A.

**Determinism control.** Build 4 ran **19** edges rather than 7, because
restoring `scripts/target_symbol_map.json` from the injectivity experiment changed
its mtime even though the content is byte-identical (`sha256sum -c` OK). That
structurally different build reproduced `matched_functions 43,555`,
`matched_code 4,040,560`, `matched_code_percent 39.431377` and a set-diff of
`CROSSED IN 3 / 180 B, FELL OUT 0` **to the last digit** ⇒ the +3 / +180 B is
entirely the splits change, with zero build nondeterminism.

### Native gate

```
tools/native_build_gate.sh                                     rc=0
```

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

`skipped=0` and `verified=18` of `expected=18`, so this is **full coverage** and
not the `PASS (INCOMPLETE: …)` shape the 0-SKIP rule exists to catch. The gate was
this lane's last action; only this markdown file is committed after it, and a
`.md` cannot reach the native link. This lane made **no `src/` edit whatsoever**
— its entire footprint is `config/45410914/splits.txt` (2 `.text` + 2 derived
`.pdata` lines) and two documents.

---

## 8. NOT done, and why

* **`0x828023A0` was NOT named**, though this lane *proved* it is
  `?NewObject@UIScreen@@SAPAVObject@Hmx@@XZ` on retail bytes + RTTI. The name is
  held by `0x823f5c20`, and the wired `CHECK MAP NAME-INJECTIVITY` edge would
  **fail the build** on the duplicate (demonstrated: rc=1, both addresses named).
  Freeing it means editing `0x823f5c20`, a map row outside my bar. **This lane
  changed no map row at all** — the injectivity experiment was restored
  byte-exactly (`sha256sum -c` OK, `git status` clean).
* **`0x823f5c20`'s false 100 % was NOT retired.** It is the coupled half of the
  filing in §4 and must be done *with* the rename, never alone, or injectivity
  re-breaks. I also did not name `0x823f5a90` (`??0NetSearchResult@@QAA@XZ`,
  264 B) — doing so would drop `0x823f5c20` below 100 while its correct name is
  still unavailable, i.e. it would book the loss without the offsetting gain.
* **`UI.cpp:`'s `0x823F4A30–0x823F5C98` block was NOT re-pinned** despite reading
  as a mis-pin (4,712 B, `NetSearchResult` ctor + factory inside, essentially all
  rows at fuzzy 0, every named `NetSearchResult` map row clustered just below it).
  It is within my splits bar, so this is a scope choice, not a bar: it is a large
  re-home into a TU that is **not in `objects.json`**, it would strip the one
  100 % row in the block, and it is unpriced. Filed coupled to §4.
* **`fn_82802240` (`UIColor::Load`, 120 B, fuzzy 0) was NOT closed**, so
  `default/UIColor` sits at **9/10** instead of completing. It correctly stays in
  `UIColor.cpp:` (its callees are `BinStream::ReadEndian`, `Object::Load`,
  `operator>>(BinStream&, Hmx::Color&)`), so it is a naming + body question, not a
  re-home, and its map row is outside my bar.
* **No `src/` file was edited**, and none was needed: the 0 % had a complete
  non-source cause (`UIColor.obj` defines no `NewObject` symbol). Had source been
  required I would have filed the patch and stopped, per the bar.
* **No alias group touched** (total unchanged at 1,652), **no `symbols.txt` edit**
  (it never even went dirty), **no hand-written `.pdata`** (both `.pdata` lines in
  the diff are dtk's own re-derivation, and their `0x20` = 4 × 8 B geometry
  corroborates the move).
* **Concurrency bars honoured.** I touched only `.text` lines under `UIColor.cpp:`
  and `UI.cpp:`. None of W16-BJ's fourteen headings, and neither of W16-BL's
  (`Line.cpp:`, `BandCharacter.cpp:`) nor its map rows (`0x82289748`,
  `0x8227a528`, `Ham*`) were read or written.
* **The 9 sweep candidates with no obj defining their factory name were NOT
  pursued** — they need the factory *emitted* (source), not a pin move.
* **Did not rebase; did not push; did not touch the main repo.**

## What would change my conclusions

* **The `0x828023A0` identification** would be overturned by showing the vtable at
  `0x82120244` is not `UIScreen`'s — but its `??_R4` COL type descriptor reads
  `.?AVUIScreen@@` and the map independently agrees, so this would require the
  RTTI read itself to be wrong. Likewise `0x823f5c20`'s would need
  `0x820599d4`'s COL not to read `.?AVNetSearchResult@@`, against which stand two
  independent member-offset confirmations (`mHostName` at 52, `mSessionData`
  at 40) from our own header.
* **The `NetSearchResult` factory SPELLING is the genuinely open question** —
  `?New@NetSearchResult@@SAPAV1@XZ` (the header declares `static NetSearchResult
  *New()`) versus `?NewObject@NetSearchResult@@SAPAVObject@Hmx@@XZ`. I did not
  settle it and deliberately did not write it. Settle it from the caller: whoever
  calls `0x823f5c20` in retail, and whether the call site passes/returns an
  `Object*` or a `NetSearchResult*`.
* **The funclet rule** (`a factory re-home pays parent + Σ funclets`) rests on one
  instance, `n = 2` funclets. The four re-home-shaped sweep candidates in §6 are
  its natural test: if a `CrowdAudio`/`TextFile` re-home also carries its funclets
  across, the rule generalises; if not, my mechanism story is wrong and the 93.4 %
  → 100 % here was pool luck after all — which is what I originally predicted and
  would be happy to be re-taught.
