# NOGROUP-1's 60 "fold-shaped but unproven" pairs, adjudicated

**Lane FOLDPROVE-1, 2026-08-14, on `c8f74dcb`.** NOGROUP-1 (`e17ad55d`) classified
1,392 ungrouped charged pairs, proved **4** folds on a deliberately strict gate, and
left **60 fold-shaped but unproven** (50 vacuous / 9 tolerance / 1 residency; 63
closable rows, 19,028 B). It also measured the key fact about its own instrument:

> the positive leg (1,519 installed groups) puts the strict gate's **RECALL against
> KNOWN folds at 19.5%** ⇒ **4 is an honest FLOOR, not a ceiling.**

So real folds were near-certainly in the 60. **Recall is raised by bringing
independent evidence, never by lowering a threshold** — the strictness is why
NOGROUP-1's 4 survived re-audit. This lane added two channels and touched no gate.

Reproduced exactly on `c8f74dcb`: 50 / 9 / 1. (`FOLD_PROVEN` now reads 0 because
NOGROUP-1's four are installed and therefore no longer "fresh" — the consistency
check that the population is the same one.)

## ⛔ First: the target set was never 60. It is 19.

**The census gate SHORT-CIRCUITS on vacuity/tolerance BEFORE it tests residency and
uniqueness** (`nogroup_census.py`, the `if vac: … elif not lit: … elif addr_ours: …
elif len(dupes) != 1:` chain). So "blocked on the weak gate" was never the same
claim as "blocked ONLY on the weak gate", and nobody had looked. Evaluated
non-short-circuited over all four gates:

| vacuous | literal | F map-resident | uniq==1 | pairs |
|---|---|---|---|---:|
| True | False | **True** | True | 31 |
| True | False | False | True | 17 |
| False | False | **True** | True | 9 |
| True | True | False | True | 2 |
| False | True | **True** | True | 1 |

**41 of the 60 are ALSO residency-blocked**, and — the decisive part —
**41 of 41 have `addr(F) != addr(S)`; zero have `addr(F) == addr(S)`.** The map
places a body at F's own address *and* at S's. **Retail keeping two addresses is the
definition of NOT folded**, so those 41 can never be aliased: an alias there would
forgive a genuinely wrong callee, which is the one thing an alias must never do.

⇒ the honest population blocked *only* on the weak gate is **19**.

★ **Reusable:** when a classifier's gates are ordered, its bucket labels report *the
first gate that fired*, not the full obstruction set. A bucket named for a weak gate
can be dominated by pairs a strong gate also rejects. Re-evaluate non-short-circuited
before funding a lane against the bucket's headline count.

## The two channels

Both are orthogonal to vacuity **by construction** — vacuity is a property of the
*body comparator*, so no amount of re-reading the body can answer it.

**1. Resolve the tolerated relocation slot instead of tolerating it**
(`tools/fold_literal_probe.py`). `relocs_agree` treats retail's `lbl_<VA>` **and our
`??_C@…`** as placeholders — both are in `icf_alias_build._PLACEHOLDER` — so two
statics interning *different* literals compare EQUAL. That is the hole NOGROUP-1
documented and it is the single largest contaminant of this population. Both sides
are in fact resolvable: MSVC encodes the literal TEXT in the mangled name, and the
`lbl_` address reads straight out of the extracted PE via `tools/xex_string_at.py`.

⚠ **The decoder's own trap, hit here and fixed:** the `??_C@` length field is **not
uniformly `@`-terminated** — lengths 0–9 are a bare digit glued to the hash
(`_04LMGLIOJM@Flow?$AA@`), lengths ≥10 are `@`-terminated letters
(`_0BA@KMFCJMLJ@`). An index-based parse (`parts[3]`) returns `''` for every SHORT
literal, which reads as *"cannot resolve"* and **UNDER-REFUTES** — the
defect-manufacturing direction. Index from the END. The first run reported 7
UNDECIDED that were purely this bug.

★ Self-validating: where retail's literal resolved, it read out as **exactly S's own
class token** every time — `'Mat'` for `RndMat`, `'BandSongPref'`, `'ui_changed'` for
`UIChangedMsg`, `'PassiveMessagesPanel'`. A reader that always returns the name it
should is a reader that is reading the right bytes.

**2. Heterogeneous retail fan-in** (`tools/fold_fanin_probe.py`). Scans `.text` for
every `b`/`bl` reaching an address, attributes callers through
`target_symbol_map.json`, and scans the data sections for the address as a **pointer
word** (vtable / function-table slot). One address reached from contexts that cannot
all be calling one function is folded — internal inconsistency needs no fold model.

## Result: 3 proven, 9 refuted, 41 refuted structurally, 3 map defects, 4 undecided

| outcome | pairs |
|---|---:|
| **FOLD PROVEN + installed** | **3** |
| REFUTED — retail keeps two addresses (residency) | 41 |
| REFUTED — different interned string literal | 9 |
| MAP DEFECT, positively evidenced | 3 |
| UNDECIDED — slot unresolvable on both sides | 4 |

Queue for the 57 non-proven: **`docs/decomp/foldprove-reclassified-queue-FOLDPROVE1.tsv`**
(carries a `swap_pair` flag: **14** rows are bidirectional, i.e. both `S→F` and
`F→S` are charged with two adjacent retail addresses — the signature of our source
or the map having two functions SWAPPED, which is one edit for two charges).

### The three proven folds

| survivor (map) | folded (ours) | addr | B | nrel | channels |
|---|---|---|---:|---:|---|
| `ParticlePoolSize@?A0x1a651105` | `@?A0xb0344ebf` | `0x82446440` | 132 | 17 | resolved literals + same-fn-two-TUs |
| `?EndFrame@Song@@` | `?GetSyncOffsetRaw@ProfileMgr@@` | `0x82545c88` | 8 | 0 | nrel=0 + vtable/direct fan-in + oracle |
| `reverse_iterator<Synchronizable**>::ctor` | `?LiteralSym@DataNode@@` | `0x823ea598` | 12 | 0 | nrel=0 + 92-caller fan-in + oracle 3==3 |

**`ParticlePoolSize`** — the two spellings differ only in the anonymous-namespace
hash MSVC derives from the source path: `?A0x1a651105` is emitted by
`system/rndobj/Part.obj`, `?A0xb0344ebf` by `system/char/CharLipSync.obj`. All 17
relocation slots resolved and agree; the 4 the comparator merely *tolerated* were
read out of retail `.rdata` as `'global_limit'` (`0x82066A6C`) and `'particlesys'`
(`0x82066A60`), identical to our decoded literals.
★ It fails the vacuity floor by **2 bytes** (unmasked 64 vs floor 66) — the floor
counts unmasked *bytes* and is blind to 17 agreeing relocation *targets*, which is
far more information than it was built to demand.
★ **No wrong-callee hazard exists BY CONSTRUCTION**: one source function, two
spellings, so there is no behavioural difference an alias could forgive. It prices
at **0 B** (`default/Part`'s `InitPool` row is already `fuzzy==100`) and was
installed for correctness, not for the metric.

**`EndFrame@Song` ← `GetSyncOffsetRaw@ProfileMgr`** — retail's body is 8 B / nrel=0:
`lfs f1,0x58(r3); blr`. At `nrel == 0` nothing is masked, so byte identity is
`/OPT:ICF`'s *complete* criterion. The address is **simultaneously** a Song-family
vtable slot (pointer words at `.rdata 0x82010124` and `0x82118fc8`, both inside
vtables carrying `SetFrame@Song`, `OnMBTFromSeconds@Song`, `CreateSong@Song`) **and**
the direct `bl` target of `RockCentral::RecordOptionData`. The *adjacent* vtable slot
is `?GetFileLength@StandardStream@@UBAMXZ` — another class's float getter sitting in a
Song vtable, independently showing these slot names are fold-survivor names. rb3-Wii
confirms `RockCentral::RecordOptionData()` (`RockCentral.cpp:1150`) calls
`TheProfileMgr.GetSyncOffsetRaw()` at line 1167, and `ProfileMgr.cpp:1179` defines it
as `float ProfileMgr::GetSyncOffsetRaw() const { return mSyncOffset; }` — a one-line
float getter matching the two observed instructions.

**`reverse_iterator<Synchronizable**>::ctor` ← `LiteralSym@DataNode`** — 12 B /
nrel=0: `lwz r11,0(r4); stw r11,0(r3); blr`. **92** retail call sites reach this ONE
address from mutually impossible contexts: `Curl_gethostname`, `MD5::transform`,
`_Rb_tree<DOHandle>::_S_right`, `ScopedCS::~ScopedCS`, `Locale::Init`,
`XShowSocialNetworkImagePostUI`, `MsgSource::OnAddSink`.
⚠ **I first read that fan-in as REFUTING the candidate** — "the body is so generic
the class must be huge, so membership is unpinned". That was wrong, and worth
recording: **92 heterogeneous callers together with `retail_addrs_for_body == 1` is
exactly what ICF predicts.** Without folding, a body that generic would appear at
*many* addresses; one address plus 92 unrelated callers **is** the fold. The fan-in
corroborates rather than refutes.
The membership is then pinned by the oracle: DC3's `OnAddSink` calls `LiteralSym`
exactly **three** times (`LiteralArray()->LiteralSym(1)`, `LiteralSym(0)`,
`eval.LiteralSym()`, all routing through `DataNode::LiteralSym(const DataArray*)` via
the `Data.h:403` inline) and the census charges exactly **3** sites — **3 == 3**. With
`MILO_FAIL_DTA` compiled out (the `MILO_*` family is `#ifdef HX_NATIVE`, never defined
in the match build) `LiteralSym` reduces to `return mValue.symbol`, i.e. precisely the
three observed instructions. **The count match is what rules out our source having
picked a different member of the same fold class** — byte identity alone could not.

### Three map defects, positively evidenced — NOT aliases

Aliasing these would forgive a real defect permanently. Each is evidenced by fan-in,
not by absence:

* **`0x8227a828`** — map says `??0HamSong@@QAA@XZ`, we say `??0BandSong@@QAA@XZ`. The
  **only** caller is `?NewObject@BandSong@@SAPAVObject@Hmx@@XZ`, and Milo's factory
  idiom means `BandSong::NewObject` constructs a `BandSong` ⇒ this address **is**
  `BandSong::BandSong`. The map is **self-inconsistent**: it carries DC3's class name
  `HamSong` for the ctor while naming the factory `BandSong::NewObject`. Same
  DC3-name-survived-the-port shape NOGROUP-1 recorded for `HamCamShot`, running the
  other way (here the **map** carries the DC3 spelling, not our source).
* **`0x82b93f58`** — map says `?Ignore@GemTrack@@QAAXH@Z`, we say
  `?SetFretButtonPressed@GemTrack@@QAAXH_N@Z`. All three callers are
  `GemPlayer::OnRefreshTrackButtons` / `FretButtonDown` / `FretButtonUp`.
  `SetFretButtonPressed(int,bool)` fits every one; the map's `Ignore(int)` has the
  **wrong arity** for a 2-arg call site (MPNGAP-1's signature test).
* **`0x82553f28`** — map says `?PrefabIsCustomizable@PrefabMgr@@SA_NXZ`, we say
  `?GetPrefabMgr@PrefabMgr@@SAPAV1@XZ`. ~10 callers, all character/prefab contexts
  fetching the singleton (`CharacterCreatorPanel`, `ManageBandPanel`,
  `FaceTypeProvider`, `OutfitProvider`, `BandProfile::GetLastCharUsed`).
  `GetPrefabMgr()` fits all of them; a bool `PrefabIsCustomizable()` fits none. This
  is the largest single prize left in the population (9 sites / 8 rows).

### The 4 genuinely undecided

`SystemLocale`↔`SystemLanguage`, `_List_base<MidiParser*>::~`↔`list<Symbol>::~`,
`_List_base<const char*>::~`↔`MetaTerminate`,
`_Destroy_Moved<map<int,float>>`↔`TambourineManager::PostDynamicAdd`. Each rests on a
relocation target that is unresolvable on **both** sides — retail spells it `fn_`/`lbl_`
(absent from the map) and our callee is not map-resident either, so neither the map nor
the string reader can decide it.

**The channel we do not have:** a resolver for *placeholder-vs-placeholder* callee
slots — i.e. an independent identification of the retail address our unnamed callee
would occupy. The best of the four is
`_List_base<MidiParser*>::~` ↔ `list<Symbol>::~`, where fan-in *is* suggestive
(all 3 retail callers are `_S_sort<Symbol, …>`, matching our `list<Symbol>` spelling,
and `Symbol` and `MidiParser*` are both 4-byte element types so their `clear` bodies
would fold too) — but its one relocation is a `b` to an unnamed `clear`, so proving it
requires first identifying **that** address. Not aliasable on today's evidence.

## Measured

Pre-registered **+2,884 B / Δmatched 0** before the run (priced per-row from
`report.json` under the all-or-nothing rule, *not* from a mismatch count); measured
exactly:

```
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.027940pp  Δcode_bytes=+2884
```

Both legs reached the `symbols.txt` split **fixed point after 0 extra re-splits** —
expected for map-only work, and the reason `ABSPLIT-1`'s fix is free here.
`VALIDATE: PASS` — 1,320 map-consistent, 202 tolerated, **0 CONTRADICTED**, 1,526
total; **no `contradiction_exempt` was used**.

`ALIAS_SUSPECT` fired (default ruler up, `none` flat). That is the documented shape of
**every** map-only patch and the `none` control **cannot** clear an alias — flatness is
the signature, not a clearance — so it is answered per group in the `witness` field on
retail bytes, never by citing the flat leg.

No `src/` movement, so the native gate is **not applicable** to this lane (stated
rather than skipped silently). Lane WRONGCALL-3 owns `src/` concurrently and was not
touched.

## Reusable findings

1. **A gate chain's bucket names report the FIRST gate that fired.** 60 "blocked on
   the weak gate" was really 19; the other 41 were rejected by the *decisive* gate and
   nobody had looked, because short-circuiting hid it. Re-evaluate non-short-circuited
   before pricing a lane against the bucket.
2. **`addr(F) != addr(S)` is a complete refutation of a fold** and it is cheap — the
   map already contains it. Two retail addresses means two bodies.
3. **A tolerance is not an obstruction — resolve what the comparator declined to
   compare.** Both sides of the `lbl_` vs `??_C@` slot are readable; that converted 9
   pairs from "unproven" to REFUTED and 1 to PROVEN. This raises recall *and*
   precision at once, which lowering the threshold could never do.
4. **The vacuity floor measures unmasked BYTES and is blind to agreeing RELOCATIONS.**
   `ParticlePoolSize` failed it by 2 bytes while carrying 17 agreeing relocation
   targets. The floor is a proxy; independent evidence can outweigh it without
   changing it.
5. **A generic body with huge heterogeneous fan-in and ONE address is evidence FOR
   folding, not against it.** I initially inverted this. Without ICF, a generic body
   appears at many addresses.
6. **When byte identity cannot pin WHICH member of a fold class our source picked, a
   call-COUNT match from the source oracle can.** 3 oracle calls == 3 charged sites
   closed the last gap on `LiteralSym`.
7. **Reclassifying is a better outcome than aliasing.** 50 of 60 turned out to be
   refutations and 3 more are map defects; the honest yield was 3. Insisting on
   evidence stayed cheap — the whole population was adjudicated in one pass.
