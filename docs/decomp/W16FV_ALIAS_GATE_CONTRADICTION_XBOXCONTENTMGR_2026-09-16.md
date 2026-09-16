# W16-FV — the red `icf_alias_finder --validate` gate was a REAL refutation, adjudicated on retail bytes

**Date:** 2026-09-16 · **Base:** main `44213c04` · **Files:** `scripts/symbol_aliases.json` (+10/−3, one membership)
**Verdict:** one alias membership **withdrawn**; gate `rc=1 → rc=0`; whole-binary **Δ0 on every key, measured**.

---

## 1. What was red

`icf_alias_finder --validate` on main:

```
rc=1   CONTRADICTED 1   map-consistent 1407   spellings 7140
```

The single contradicted membership was `??0XboxContentMgr@@QAA@XZ`, listed as `folded` in
group 859 (`name: "_E?$StandardEffect"`, `address: "0x82b8c8c8"`, survivor spelled
`??_E?$StandardEffect@VCompressionEffect@@@@UAAPAXI@Z`).

## 2. The adjudication — on RETAIL bytes, not on our build

Both figures below come from **one reader** (`report.json` target extents), on both sides:

| symbol | retail address | size |
|---|---|---|
| `??0XboxContentMgr@@QAA@XZ` | `0x825213D0` | **184 B** |
| group 859's survivor | `0x82B8C8C8` | **60 B** |

`/OPT:ICF` leaves **one** survivor. It cannot leave two bodies of different size at two
addresses. So the membership cannot be a fold — **the contradiction is real and the gate was
right.** The bodies corroborate it independently (read out of the split asm, keyed on
`.fn fn_<addr>`, never the synthetic address column):

- `fn_825213D0` — a 184 B **manager ctor**: `__savegprlr_29`, 0x80 frame, vftable
  `lbl_8208992C`, three-plus `std::list` sentinel inits. It sits inside the coherent 21-row
  `XboxContentMgr` block `0x8251FAF8`–`0x825216E0` and pairs against our source at
  **fuzzy 23.5217 / mpn 24.6087** — a misidentification would read ~0.
- `fn_82B8C8C8` — a 60 B **derived-class ctor**: `bl` base ctor `fn_82B8D210`, store vftable
  `lbl_8219C1C8` at `0x0(r31)`, `blr`. That is the whole body.

## 3. The counter-precedent that does NOT apply here

On 2026-08-16 lane STLPORT-1 (`ff832b50`) refuted a *size-based* withdrawal and GROUNDED-2
restored **6 of 8** folds. That correction does not reach this case, and the difference is
structural, not a judgement call:

| | STLPORT-1 | here |
|---|---|---|
| gap | **8 B** | **124 B** |
| cause | EH-funclet prefix billed into a COMDAT span by `tools/coff_bodies_ext.py` | — |
| instrument | **two different readers** (`.pdata` extent vs COMDAT span) | **one reader, both sides** |

A one-sided instrument error is invisible to a two-sided control *because it cancels*; that is
exactly why a size test could not catch STLPORT-1's artifact. Here there is no second reader to
disagree with, and 124 B is not an 8 B prefix.

## 4. The argument I deliberately did NOT make

I did **not** argue from "our own compiler gives these two spellings different-sized COMDATs".
That is the ALIAS-REPAIR 2026-08-19 predicate — resolving operands through the ICF congruence
over *our* build — and lane **W16-CU reversed it on this very group**, restoring
`??0AnimPtr@@QAA@ABV0@@Z` after it had been withdrawn on exactly that reasoning. Reusing it here
would have re-litigated a settled reversal with a weaker instrument.

⛔ **Consequence for tooling: `tools/alias_apply_withdrawal.py` stamps that refuted predicate.**
Its hardcoded note reads *"Refuted WITHIN OUR BUILD: our own compiler gives these two spellings
… different-sized COMDATs"*. Running it would have written a justification this group's own
history refutes. **The record was hand-written in W16-AB's schema instead.** The tool is not
broken for its original population — but its canned note is not a general-purpose justification,
and it is not reviewed at the call site.

## 5. The measurement — and why Δ0 here is a *measured* zero

Pre-registered (`~/tmp/predict_w16fv.txt`, written before any A/B output was read): **Δ0 on
every key**, with six falsifiers. The central one: **Δ0 is not self-certifying** — "nothing
moved" and "nothing was measured" are indistinguishable, so the zero needs an instrument that
is not the metric.

Two independent instruments certified it, one of them stronger than pre-registered:

1. **The validator flip, in the SAME built worktree** (pre *and* post — a fresh worktree's
   reflinked objs are pre-renamer, so it was built first; index counts reproduced main's
   exactly: 3,111 live target objs / 28,397 mangled names / 1,220 compiled objs / 841,715 symbols):
   ```
   pre : rc=1  CONTRADICTED 1  map-consistent 1407  spellings 7140
   post: rc=0  CONTRADICTED 0  map-consistent 1408  spellings 7139
         VALIDATE: PASS -- 1408 map-consistent, 249 tolerated, 0 contradicted, 1658 total
   ```
2. **The equivalence table objdiff actually loaded**, straight out of the two legs' build logs —
   this is the sharper one, because it lands on the scoring path itself:

   | | rendered map | objdiff loaded |
   |---|---|---|
   | leg A | 6,918 symbol lines | **6,102 ICF equivalence entries** |
   | leg B | 6,917 symbol lines | **6,101 ICF equivalence entries** |

   Exactly one fewer alias reached the ruler. `scripts/symbol_aliases.json` is a declared ninja
   input to the `icf_alias_map` edge that renders `build/45410914/icf_aliases.map`, which *is*
   `objdiff.json`'s `map_file` — so the edit is consumed by the report, not merely by the gate.

**A/B (`tools/ab_measure.py --from-dirty`, classified `map` ⇒ forced re-split on both legs,
both at a `symbols.txt` fixed point, leg B `renamer_patched=1832`):**

```
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0
Δfuzzy=+0.000000pp   units at 100%: 192 -> 192 [mpn], 171 -> 171 [fuzzy]
control none: FLAT — ALIAS_SUSPECT did not fire
```

Row-level set-diff of the two archived leg reports: **0 of 69,240 rows moved, 0 appeared, 0
vanished.** Not even offsetting movement.

⇒ **The forgiveness removed was never load-bearing.** Sign was constrained in advance: deleting
forgiveness can only hold or fall, so a *rise* would have meant the tool measured something
else. It held.

## 6. FLAGGED, NOT ACTED ON — the survivor's own label is suspect

`fn_82B8C8C8`'s 60 B body is a **derived-class constructor**, not a vector deleting destructor.
So `??_E?$StandardEffect@VCompressionEffect@@@@UAAPAXI@Z` is plausibly an arbitrary spelling the
ICF survivor inherited rather than this address's true identity. **This lane did not act on
that** — it is a separate identification with its own A/B, and naming an address is a bet whose
payout is bug exposure, not bytes. Recorded here so it is not re-discovered from scratch.

## 7. Side discovery — `scripts/target_symbol_map.json` has a heterogeneous value shape

29,590 rows: `str` 29,484, **`NoneType` 101**, **`list` 5**. The five list-valued keys are not
addresses at all — they are control keys: `_bijection_arbitrary`, `_denylist` (6),
`_denylist_unadjudicated` (1), `_icf_arbitrary` (35), `_internal_linkage_allow`
(`?NodeCmp@@YAHPBX0@Z`). A naive inverse index crashes twice — `unhashable type: 'list'`, then
`'NoneType' object is not iterable`. Both crashes were informative; a `names(v)` helper
returning `[]` / `[v]` / `list(v)` is the fix. **Worth knowing before writing any map-wide scan.**

## 8. What this lane did NOT do

- **Nothing was pruned.** Group 859 keeps its other **12 memberships** and its group entry; the
  withdrawal is **per-membership**, recorded in `withdrawn[]` so a future generator cannot
  silently re-propose it. (A prior prune, `a745039e`, cost **+94,616 B** to reverse.)
- No claim that the other 12 memberships are right — they were not adjudicated.
- No re-identification of `0x82b8c8c8` (§6).
- No use of `tools/alias_apply_withdrawal.py` (§4).
