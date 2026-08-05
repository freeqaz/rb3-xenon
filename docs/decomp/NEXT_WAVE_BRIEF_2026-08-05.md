# Next-wave brief — written 2026-08-05, after waves EB→EE

**Audience: the next coordinator or lane, picking this up cold.** Everything here
is measured; where a number came from a single lane or a small `n`, it says so.
Read [`../INDEX.md`](../INDEX.md) § *2026-08-03 results* for the one-line verdicts,
and `CLAUDE.md` for the binding project rules.

---

## 0. State, and how to establish it

**Do not trust the numbers below as current.** They are composed lane deltas, not
a fresh absolute, and this project has burned real work on stale absolutes twice.

| | value | provenance |
|---|---|---|
| matched_functions | ~43,877 | composed from per-lane settled A/Bs |
| matched_code | ~4,242,936 B | composed |
| code% | ~39.694 | composed |
| units at 100% | 255 | EC-2 landed 254 → 255 |
| last measured absolute | **43,868 / 4,239,592 B / 39.663130 / honest 21,136** | EE-1 + EE-2 leg A at `4f05581a`, independently agreeing |

**First thing a new wave does: measure a real baseline** in a clean worktree built
from committed state — **never** from the shared `main` tree, where other agents'
uncommitted work is live. Two published baselines were wrong for exactly that
reason.

```bash
scripts/setup_worktree.sh ~/tmp/laneXX/wt laneXX
python3 tools/ab_measure.py --worktree ~/tmp/laneXX/wt --from-dirty
```

⚠ **`matched_code` and `functions[].size` are JSON *strings*.** `int()`-coerce
them or `+` concatenates and `>` compares lexicographically — one lane's size
filter returned a clean, decisive-looking `0 rows`.
⚠ **Read `total_code` / `total_functions` from `report.json` every time.** It moved
three times in one day (10,688,948 → 10,689,000 → …).
⚠ **The source-only unit ceiling is not constant and NOT MONOTONIC**: 253 → 253 →
293 → 290 in a day, in both directions. It is not a target and not a floor.

---

## 1. THE primary target: the body-port class

**This is the largest identified vein: 1,011 rows / 556,776 B — 61.5% of named
charged rows and 73.1% of charged bytes.** Full record:
[`EE2` sizing in INDEX](../INDEX.md), memory
`project_bodyport_class_is_the_vein_2026-08-03.md`.

| class | rows | bytes |
|---|---:|---:|
| `SOURCE_INSDEL` | 549 | 289,752 |
| `SOURCE_CALLCOUNT` | 462 | 267,024 |
| **total** | **1,011** | **556,776** |
| …excluding STL template COMDATs | 870 | 521,840 |

### The ready-made queue

**[`bodyport-queue-EE2.tsv`](bodyport-queue-EE2.tsv)** — 43 rows, fuzzy ≥97 and
≥500 B, cheapest-first. **66,088 B / +0.6183 pp if all cross.** Median 12
mismatches. Columns: `size · fuzzy · n_mismatch · signature · unit · symbol`.

Head of queue:

| bytes | fuzzy | mismatches | symbol |
|---:|---:|---:|---|
| 592 | 99.324 | **1** | `StoreMainPanel::FinishLoad` |
| 772 | 99.477 | 2 | `BandRetargetVignette::EnterDir` |
| 672 | 99.369 | 2 | `JoypadController::JoypadController` |
| **5,036** | 99.762 | **3** | `CustomizePanel::Handle` |
| 692 | 98.728 | 3 | `StreakFocusTracker::GetNextFocusPlayer` |

⚠ The queue is **unverified beyond census labels**, and EE-2's own result shows a
label can split one root cause across two classes. Treat rows as *pointers to a
locus*, not as facts.

### Two constraints that decide how you scope a lane

1. ⛔ **`SOURCE_INSDEL` alone is the WRONG SCOPE — it drops 46% of the vein.**
   EE-2's two targets root-caused to the same kind of defect, but the census files
   them in different classes: a missing **argument copy** surfaces as extra
   ctor/dtor calls (`SOURCE_CALLCOUNT`), not as an insert/delete run. **Scope a
   lane to both classes.**
2. ⛔ **No cheap targeting filter exists — two were tested and both failed.**
   `Load`/`Save`/`Copy` bodies enrich only **1.31×** and are 6.9% of the class. The
   apparent "it's all `Load`s" was an artifact of an earlier lane's rev screen,
   which *by construction* only examined `Load` bodies. The class is **diffuse**:
   385 units, and the top 126 hold just 80% of the bytes.
   ⇒ **This is per-row work read off retail bytes. Budget it that way.**

### What a body-port row actually looks like (from the two that closed)

- **`RndText::Load` 87.96 → 100%** — the prior lane named **one** divergence;
  there were **five**, and #2 ran the *opposite* direction (retail builds a 16-byte
  `Vector3` temp and copies 4 words in — the `const Vector3&` `SetLocalPos`
  overload — while we stored 3 floats direct). It also contained a **real bug**:
  the inherited rb3-Wii shape assigns `mText.length()` into a `bool` and discards
  it, leaving `int fixedLength` **uninitialized**.
- **`CharLipSyncDriver::Load` 93.36 → 100%** — "our source is incomplete" was
  **refuted**; the statement was already there. `MILO_NOTIFY` expands to the comma
  form, which treats `(String&)fp` as a free reference cast where retail
  **copy-constructs**. Fixed **per-site** with `MiloStripEval` — ⛔ **not** by
  editing `os/Debug.h`, a PCH input cascading ~281 TUs whose own comment records
  that switching the NOTIFY *population* measured **−20**.

★ Both fixes brought **EH funclets into pairing** as a bonus (+2×40 B), exactly as
`Debug.h`'s comment predicts: destructible `String` temps → EH states → funclets.

---

## 2. Secondary targets, already diagnosed

- **`RndParticleSys::Load`'s `mBounce` wall** (~130 instructions at 23%, inside a
  2,436 B row already lifted to 86.1%). Retail builds the Transform with **whole
  16-byte row copies** — `tf140.m.z` ← the `Plane` at `r31+0x60`, `tf140.v` ← a
  Vector3 at `r31+0x70` holding `a*inv,b*inv,c*inv` — where our source assigns 12
  individual floats. Coupled **+0x20 frame delta**, heavy FP scheduling (f0↔f13
  ×9). ⚠ Multi-hour RE with permuter-class risk; the permuter is **OFF** by
  standing directive.
- **`MemcardMgr::SaveLoadAllComplete`** — unresolved, and **both** prior readings
  are refuted. It is *not* the vbase "fix" (496 B of matching retail code proves
  our layout), and the counter-argument is also weak (the vbase chain loads its
  displacement at runtime, so a 100% sibling proves the access *pattern*, not where
  `Object` lands). Retail's `this+0x20` collides with `SetProfileSaveBuffer` at
  100%, whose 12-byte body has **zero relocations** ⇒ a prime ICF fold. Treat as
  **attribution**, not layout.
- **`CharBlendBone::SetType`** — fuzzy 99.85, uniform **+8** Object-vbase
  displacement, BQ-2's proven +8 family. Sized and located but **deliberately not
  closed**: the only available close was anonymous padding, which is metric-fitting
  (BQ-2 had to undo exactly such a pad). Reopen only with a justified member.
- **`CharBlendBone::ClassName` scores 100% while calling
  `StaticClassName@CharIKFingers`** — a fresh wrong-callee-at-100%. Correctness
  only; it will measure Δ0.
- **Rev dialect leftovers** — the model is settled (§4). Remaining rows are in TUs
  whose dialect must still be read off target bytes.

---

## 3. ⛔ DRAINED / REFUTED — do not re-fund

| channel | verdict |
|---|---|
| **Extent carving** | **DRAINED.** Symmetric direction empty (`OVER_PINNED = 0` binary-wide); 400 hits → 10 named+charged, none able to cross. Its advertised null **could not fire**. |
| **Uniform immediate delta** | **CLOSED.** 47 rows / 9,268 B → **2 rows / 900 B real**; 49% ICF fold aliases. ⚠ "47" was a subset; full population is 91 rows / 32,992 B. |
| **Sliver-pin misattribution** | **REFUTED as a sweep.** 80%/9.58× was n=17 and confounded; standardised 1.23×, no stratum significant. **Convertibility ZERO.** ~36 real rows binary-wide (3.16%). |
| **"Defects are in the low-% rows"** | **REFUTED** on the full 1,644-row population. `<40` is 37.3% map/stub; `≥99` is still 61.1% source-shaped at median **2** mismatches vs 40. |
| **Statement-order permutation** | **INERT** — 10 variants, all byte-identical. MSVC /O1 normalises independent assignment order. |
| **`_M_insert_overflow_aux` `sizeof(T)`** | **No classes to name.** Retail's own immediates contradict themselves (`NavItem` 12/32/48/96) ⇒ fold aliases. |
| **`EQEffect::Process`** | **Closed as codegen.** Every effective address is identical (`0xbc−0x20 == 0xa4−0x8`); the `off:±24` is a base-anchor choice. |
| **Storage-class lever** | Drained as a byte lever (0 rows moved, −868 B cost). |
| **`RndText` as a rev row** | Refuted — zero rev-static mismatches in 99 instructions. It was a body port (now closed). |
| Older: `??_E`, over-pinned-span as a population lever, container member-type, SYNC_PROP local-static, `base_path`, DC3 surplus handler arms | all previously drained — see `MEMORY.md`. |

---

## 4. The rev/`BinStreamRev` model — settled, and it is a *reading*, not a rule

Retail **never constructs** a `BinStreamRev`; it **CASTS** a raw `BinStream&`
(`??_R0` count **0**, against 1 each for `BinStream`/`MemStream`/`FileStream`,
with `/GR` on). Its base **is** at offset 0 — proven by 188 B of byte-exact retail
code passing it straight to `?ReadEndian@BinStream@@` as `this`.

★★★ **Three dialects exist. Read the target bytes; never apply a form by rule.**

| target signature | dialect |
|---|---|
| `lwz` + `cmpwi` on a **stack slot** | **no storage at all** — a plain `int` local. The static is not merely redundant, it **burns a callee-saved register and inflates the frame**. |
| `lhz` + `cmplwi` off a **base register** | keep **one aggregate** (altRev@+0, rev@+4) |
| two separate statics | CharHair — an aggregate *dropped* `Strand::Load` 100 → 96.6 |

⚠ Our source's `__declspec(align(4))` **predicts nothing** — a lane predicted the
no-storage form would generalise across all such TUs and its **very first control
refuted it**.

★ **The `revB` gate is load-bearing.** Keep `d >>` for **container** reads: it
preserves retail's rev-mangled instantiations. Converting them to `bs >>` deleted
**412 B of byte-exact retail code** in one lane and **968 B** in another — and in
both cases **the whole-binary aggregate still read positive**.

---

## 5. Instrument hazards to inherit — every one of these produced a *clean* wrong answer

1. ⚠⚠ **`RB3_OBJPTR_INLINE_OWNER_CTOR` is TU-WIDE.** The define that closes a
   `Load` row **destroys constructor rows in the same TU** — observed in *both*
   polarities across two lanes, and **the whole-binary aggregate read POSITIVE
   every time**. Remedy: per-site 1-arg/2-arg `ObjPtr` spelling. **Never land it on
   aggregate numbers alone.**
2. ★★★ **Always do a per-row `AT_100` set-diff.** Aggregates concealed a 968 B
   deletion of byte-exact retail code, and constructor destruction twice, in a
   single day. `ab_measure` labels each completion `MATCHED_ROSE` /
   `DENOMINATOR_SHRANK` / `MIXED` / `NEW_UNIT`.
3. ★★★ **`mpn == 100` is not evidence a member type or callee is right** — it
   excludes arg-only penalties and is blind to wrong callees. **Expect correct
   fixes to measure Δmatched 0. Land them anyway.**
4. ★★★ **A `REGISTER_SWAP` label is a SYMPTOM, not a diagnosis.** Swaps of 13, 24,
   27, 28 and 46 instructions have all **dissolved** once the real source defect was
   fixed — four independent confirmations. Never defer a row as permuter-bound on
   that label; conversely, a cascade dissolving means you found the real defect.
5. ★★★ **Check a witness CAN DISCRIMINATE before accepting one that BLOCKS work.**
   A lane declined a real +1 unit on a row that scored 100 under *both* hypotheses.
6. ★★★ **Matching retail code PROVES a layout is already right** — the cheapest
   available refutation of any proposed structural change (496 B killed one, 1,300 B
   another).
7. ★★★ **The CALLEE PROLOGUE is the strong witness for arity** (preserved+forwarded
   registers). Caller literals are one weak witness; caller constants are **masked
   reloc args scoring 100 either way**.
8. ★ **A uniform offset delta is not a layout defect until you check the base
   register** — and **r1 is not the only stack register**: MSVC establishes a
   **second frame pointer** (`subi r31,r1,N`). The largest row in one class was a
   `String` temp at frame+104 vs +120.
9. ★ **`run_objdiff` can read 100 while `report.json`'s `mpn` stays < 100.** `mpn`
   is not derivable from any `diff` field. **Believe `report.json`.** Its ruler was
   aligned to the grader at `131c723d` — **never compare a reading across that
   commit**.
10. ⚠⚠ **Assert `substitution count == site count` on every scripted edit.** A
    converter regex `[^;]*` matched newlines, ate a statement, and **compiled
    clean**. Count assertions caught real miscounts in four separate lanes.
11. **`grep` in a Claude shell is BINARY-BLIND** (a shell function routing through
    `ugrep -I`) ⇒ false negatives shaped exactly like decisive ones. Use
    `command grep -a` or Python. Guard: `python3 tools/grep_binary_guard.py`.
12. **Run `tools/native_build_gate.sh` before landing any shared-`src/` change** —
    require **PASS 18/18 with 0 SKIPs**. A PASS *with* SKIPs is a false green (the
    gate's own cmake omits `-DMILO_ENGINE_PATH=`/`-DDawn_DIR=`; pass them as
    **absolute** paths *and* pin the compilers to the gate's own absolute paths).
    This gate has caught `main` broken by a matching lane **five** times.
13. ⛔ **Adjudicate on retail bytes** or `scripts/harvest/class_layout_report.py`
    (wraps `cl.exe /d1reportSingleClassLayout`, authoritative). **Never** the
    `// 0xHEX` header comments — measurably wrong in places.

---

## 6. Method notes that paid

- **A control that cannot fail is worth nothing, and reads exactly like success.**
  Three separate claims died on this in one day: a null whose condition was
  unsatisfiable, a ratio that rose monotonically with its own threshold, and an
  enrichment whose control was 5× less isolated than its treatment. **Before
  quoting a null, ask whether the treated condition is even reachable in the null
  population.**
- **Size the class before working it, and re-measure inherited numbers.** Four
  handed-on figures were wrong this wave (a queue head that wasn't the right lever,
  an `n` that was a subset, a dead field, a ceiling that had moved).
- **A refutation with numbers is a full-value lane result.** Half of this wave's
  value was closing veins so the next wave doesn't pay for them again.
- **Measure cross-lane interaction, don't assume it either way** — one lane proved
  a sibling's header fix closed *none* of its gap via a byte-identical residual
  control.
