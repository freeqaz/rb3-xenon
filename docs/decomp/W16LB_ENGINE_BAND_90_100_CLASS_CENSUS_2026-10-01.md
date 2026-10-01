# W16-LB — engine 90–99.99 band: charge-class census and fixes (2026-10-01)

**Branch** `w16-lb`, off main `9d627e235` (W16-KF landing). **Not merged to main**, per the brief.
**Ruler** `name_check` (graded; `report.json` `provenance.diff_config`).

**Population.** Every row with `90 ≤ fuzzy < 100` in a unit whose `objdiff.json` `metadata.source_path`
is under `src/system/{rndobj,synth,synth_xbox,ui,utl,obj,meta}/`. `bandobj`, `char` and `band3` were not
touched (other lanes own them). `matched_code` credits a row's full size only at fuzzy 100, so the band
is ranked by size.

## 1. Class census (main `9d627e235`)

The brief's figure reproduces exactly: **936 rows / 204,488 B** (`~/tmp/w16lb/pop.py`).

Each row was diffed with `objdiff-cli diff` under the project config (`~/tmp/w16lb/cls.py`); the diff's
`fuzzy_match_percent` equals `report.json`'s on **936 / 936** rows, so the census is on the graded ruler.
A row takes the first class that applies, in this order:

| class | rule |
|---|---|
| STRUCT_INSDEL | any inserted or deleted instruction |
| OPCODE | any replaced instruction or differing opcode, no ins/del |
| IMMEDIATE | an immediate argument differs (not `r1`-based) |
| STACK_REG | only `r1`-based immediates and registers differ (stack layout follows register residue) |
| NAME_ONLY | only relocation target names differ |
| NAME+REG | relocation names and registers |
| REG_ONLY | only registers differ (register allocation; skipped by brief) |

Bytes by class and directory:

| class | rndobj | synth | synth_xbox | ui | utl | obj | meta | rows | bytes |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| STRUCT_INSDEL | 56,816 | 6,260 | 8,856 | 5,044 | 8,820 | 6,704 | 2,188 | 152 | 94,688 |
| NAME_ONLY | 32,548 | 5,644 | 2,488 | 6,096 | 2,124 | 2,720 | 1,952 | 523 | 53,572 |
| REG_ONLY | 9,396 | 4,448 | 1,184 | 1,508 | 920 | 3,848 | 1,468 | 53 | 22,772 |
| IMMEDIATE | 10,520 | 872 | 2,056 | 1,152 | 1,696 | 200 | 1,780 | 159 | 18,276 |
| OPCODE | 836 | 332 | 168 | 0 | 1,228 | 3,848 | 40 | 22 | 6,452 |
| NAME+REG | 1,952 | 432 | 1,588 | 0 | 920 | 0 | 0 | 14 | 4,892 |
| STACK_REG | 2,036 | 440 | 0 | 0 | 0 | 548 | 0 | 6 | 3,024 |
| other (branch-dest / mixed arg kinds / one `diff_op`) | 276 | 216 | 0 | 0 | 92 | 0 | 228 | 7 | 812 |
| **total** | 114,380 | 18,644 | 16,340 | 13,800 | 15,800 | 17,868 | 7,656 | **936** | **204,488** |

Where the bytes sit inside the two big classes:
- **STRUCT_INSDEL** is dominated by large Load/Draw/Save bodies with 50–220 charged instructions
  (`RndMesh::Load` 3,452 B / 127 charges, `RndTexRenderer::DrawToTexture` 3,320 B / 220). Rows with
  under ~12 charges are the realistic targets.
- **NAME_ONLY**: one pair, retail `??1ObjRef@@QAA@XZ` vs our `??1ObjRefOwner@@UAA@XZ`, was the sole charge
  on **140 rows / 5,608 B** (§3.1). 368 NAME_ONLY rows / 14,968 B are anonymous `fn_` rows, mostly 40-byte EH funclets; most of them
  called the ObjRef survivor and closed with §3.1. After §3.1–3.3, 135 anonymous rows / 5,440 B remain.

Single-pair closability, measured on the census: 511 NAME_ONLY rows / 49,624 B carry exactly one distinct
name pair.

⚠ **NAME_ONLY is an upper bound on "a name fix closes the row".** On an instruction whose relocation
target differs, `diff_breakdown` lists only the `symbol` argument even when a register also differs.
`EventTrigger::Replace` read NAME_ONLY with an unnamed retail target, and showed a register swap once the
target was named (§4.2).

## 2. Results

RESULTS

## 3. Lane work (global map and alias files)

### 3.1 `0x82270298` is `~ObjRefOwner`, not `~ObjRef` — 743 rows to 100, +29,704 B

The largest NAME_ONLY pair was retail `??1ObjRef@@QAA@XZ` against our `??1ObjRefOwner@@UAA@XZ`: 140 rows
/ 5,608 B in the band, each with that as its only charge.

- The retail body at `0x82270298` is `lis r11,0x8200 / addi r11,r11,0x9EC / stw r11,0(r3) / blr`. It
  installs the vtable at `0x820009EC`, whose RTTI is `.?AVObjRef@@` (`tools/retail_rtti.py vtable`).
  Retail has **no** `ObjRefOwner` class (`retail_rtti.py class ObjRefOwner`: no COL).
- Our X360 `ObjRefOwner` **is** retail's `ObjRef`: W17-OPTR re-based `ObjPtr`/`ObjOwnerPtr` on it, and
  `tools/icf_pair_adjudicate.py` carries `CLASS_RENAMES = {ObjRefOwner@@: ObjRef@@}` with a retail-byte
  witness. The map already names the same vtable's slot-0 deleting dtor `0x822702A8`
  `??_GObjRefOwner@@UAAPAXI@Z`.
- Our `ObjRef` has no vtable in the match build (`OBJREF_VIRTUAL` is empty without `HX_NATIVE`), so its
  dtor cannot be a body that stores a vtable.
- As an alias the pair is not provable: `--chase` reads it REFUTED, because the 4-word body with 2 masked
  words is vacuous at top level. The defect is the **map name**, so the fix is a map repair.
- Safety: no relocation in any of our objects targets `??1ObjRef@@QAA@XZ` (only Object.obj and
  Instance.obj define an unreferenced COMDAT), and `??1ObjRefOwner@@UAA@XZ` sat at no other address.
  MatAnim.obj defines it, so the row at the address pairs.

Full build vs main: **+29,704 B, +1 fn; 768 rows up (743 to 100), 0 down.** The prediction (140 rows in the
band, more binary-wide) was right in direction and about 5× low in magnitude: the pair was charged
binary-wide. `matched_functions` moves only +1 because `mpn` already excluded these charges.

### 3.2 67 chase-proven fold memberships — 118 rows to 100, +14,808 B

All 294 function-name pairs left in the band went through `tools/icf_pair_adjudicate.py --pairs --chase`:
**111 CHASED T1 PROVEN, 183 REFUTED**. Admissible = PROVEN with no `CYCLE` or `UNDISCHARGED` text in the
pair's own log block (W16-KD's rule): **89**. `~/tmp/w16lb/install.py` then screened each:

| screen | refused |
|---|---:|
| spelling already a member or survivor of another group | 15 |
| the map places the spelling at an address of its own (not a fold) | 7 |
| installed | **67** |

`tools/alias_placeholder_slot_audit.py`: **67 / 67 CLEAN, cycle 0**. `icf_alias_finder.py --validate`:
PASS, 1,667 map-consistent, 0 contradicted.

Ten admissions override earlier withdrawal records for the same spelling; each admitted record names the
withdrawal and why the per-pair chase answers it:
- 7 `FABRICATED_CLOSURE_NOT_PARTITION`: closure memberships refused for want of a per-pair proof;
- `CF2_WARRANT_WITHDRAWN` on `~list<RndMultiMesh::Instance>`. That record asked for a positive warrant.
  The chase supplies one: the 4-byte body's branch destination is recursively proven to be the same
  function on both sides. The address that used to separate the spellings, `0x8273e038`, was a wrong
  map row and is now `DxEnviron::Select` (W16-JA);
- `WRONG_MAP_NAME_AT_ADDRESS` on `MakeString<int,int>`, withdrawn from a *different* group (`0x82399348`);
- group `0x82440118` `Keys<Quat>::Remove`: its withdrawn member is the `Keys<Color>` spelling (KeyLessEq vs
  KeyGreaterEq). The `Keys<Vector3>` spelling's chase recursed through its own callees and they agree.

Full build: **+14,808 B, +40 fns; 128 rows up (118 to 100), 0 down.** The prediction was 90 band rows /
12,436 B. `matched_functions` moves because objdiff `b14ba45` lets a vetted wrong-callee charge reach
`mpn`.

### 3.3 `ShaderMacro` needs a user-declared copy constructor — GenerateMacros 99.55 → 100, +3,084 B

`ShaderOptions::GenerateMacros` (3,084 B) had one charge: retail's survivor
`push_back<pair<const VocalPhrase*,VocalPart*>>` against our `push_back<ShaderMacro>`. The chase refuted
at one leaf: our `__uninitialized_fill_n<ShaderMacro>` differs in bytes from retail's
`fill_n<pair<int,int>>` at `0x824F9020`. Retail counts with `mtctr`/`bdnz`; ours counted with
`addic.`/`bne`.

Across all 48-byte `fill_n` COMDATs in our build, the `mtctr` form appears for exactly the element types
that have a **user-declared copy constructor**: every `std::pair`, `CharBones::Bone`, `MicClientID`. Every
other type uses the counter form. Our own `fill_n<pair<int,int>>` is at 100 (Rot), so the compiler
reproduces retail's form when the type has one.

- First attempt, **failed prediction**: removing the user `operator=` left `fill_n` unchanged and dropped
  GenerateMacros 99.55 → 86.80. Reverted.
- Adding `ShaderMacro(const ShaderMacro&)` makes `fill_n<ShaderMacro>` byte-identical to `0x824F9020`. The
  `push_back` pair then reads CHASED T1 PROVEN, no CYCLE, and is installed.

Full build: GenerateMacros to 100, **+3,084 B, +1 fn, 0 down.**

### 3.4 Negative result: naming retail RTTI TypeDescriptors and literals

Nine data addresses that rows charge as unnamed `lbl_` against our named data were named from retail
content: six `??_R0` TypeDescriptors (each by its own `.?AV…` string at +8), two string literals (`free`,
`alloc`) and `__real@46fffe00`. All gates passed. Prediction: 4 rows / 1,752 B to 100.

**Measured: 0 rows up, 8 down, −2 fns** (two AmbientOcclusion `GatherObjects*<RndMesh>` rows fall from
`mpn` 100). Two things were wrong with the model:
- The `lbl_82C70AD0` ↔ {Object, RndMesh} pairings are **not wrong casts**. The function loads three
  TypeDescriptors and retail schedules the three `lis`/`addi` pairs in a different order.
- Unnamed `lbl_` targets were partly forgiven. Naming them turned forgiven sites into checked ones and
  exposed ordering and register residue: `EventTrigger::Replace` is name-correct afterwards and shows a
  register swap.

Reverted in full (nothing committed); the tree's measures came back bit-identical. The names are right on
retail bytes, but under the brief's no-row-off-100 rule they are a net loss here.

### 3.5 Not taken

- 15 chase-proven spellings are already members of a group whose survivor is a *different* retail
  address. Retail keeps two byte-equivalent bodies, and a spelling can fold to only one survivor. Not
  changed.
- 40-byte anonymous EH funclets whose "callee" pairs (`~ObjRefOwner` vs `~String`, …) come from
  byte-signature pairing of unrelated funclets: the `masked_equal` class, not a defect.

## 4. Forks

FORKS

## 5. Gates

GATES

## 6. Rows left, by blocker

LEFT
