# Open leads from round nine — three lanes interrupted by an API outage

**Status: UNVERIFIED HYPOTHESES, not findings.** All three lanes (W23-FRAMESWEEP,
W24-PAIRING, W25-UI) were terminated mid-investigation by repeated API 529
capacity errors on 2026-08-17 — not by anything they measured, and not by
reaching a conclusion. Their committed tools survive in their branches; what
follows is the reasoning that was **in flight** when each died.

**Read this as a to-do list with stated tests, never as a result.** Each lead
below names the experiment that would settle it. Nobody has run those
experiments. The whole point of recording them is that an interrupted lane's
best thinking is otherwise lost, and re-deriving it costs more than the test
does.

---

## LEAD 1 (W24-PAIRING) — ★ `none` may measure the pairing channel EXACTLY, not as a bound

**The claim in flight.** W20-CASCADE measured `functionRelocDiffs=none` at
**−2,520** and separately quoted **2,520** as a *bound* on the pairing channel
(2,520 candidate vs 2,396 recovered). W24 noticed those are two different
things being read off one number, and that the relationship should be tighter
than "bound":

> Under `none`, relocation-name charges vanish. So the **cascade contributes
> exactly 0** to a `none` reading, and `none` therefore measures the **pairing
> channel alone**. A pairing model should predict `none` **exactly**, with no
> bracket.

**Why it matters.** It would convert the dominant term (un-pairing, 80.5% of a
map edit's delta) from a bounded quantity into a *predicted* one, and it would
mean we already have a direct instrument for it — the control leg we have been
running all along for a different purpose.

**The stated test, not yet run:** `0x82456190` reads graded **99.7059**. If the
claim holds it must read **100 under `none`**.

⚠ **Do not adopt this before running that test.** It is a clean, falsifiable
prediction from a lane that never got to execute it. Note also the standing
caution that `none` **cannot** validate an alias — it ignores relocation names,
so it reads +0 there *by construction*, and that flatness is the signature of
the hazard rather than a clearance. The claim above is about **pairing**, which
is a different mechanism from name forgiveness; that distinction is exactly what
needs checking.

---

## LEAD 2 (W23-FRAMESWEEP) — unnamed vs named temporaries as a stack-overlay discriminator

**The observation (measured, and this part is solid).** In `SampleData.cpp`,
both temps are the identical 7-instruction sequence — `String` copy-ctor,
`FilePath` vptr store, `~String` — i.e. exactly our `{ FilePath tmp(fp); }`.
Everything else in the function matches. **Retail puts both at `0x60`; we put
the second at `0x70`.**

**The hypothesis in flight.** Retail's are **unnamed temporaries** (from
`MILO_LOG("%s", fp)` building an argument), and **MSVC overlays unnamed
temporaries in disjoint full-expressions where it will not overlay two *named*
locals in sibling scopes.**

**Why it matters.** It would generalise well past this row, and it rhymes with
what W22-FRAME *proved* on `Handle@VocalPlayer`: that frame shortfall was
**escape / memory-effect analysis**, not inlining — MSVC overlaid a message temp
onto shared scratch instead of granting it 8 private bytes. Same family of
reasoning, different trigger. If named-vs-unnamed is really the discriminator,
it is a source-level lever on stack layout, which we currently have very few of.

**Status:** the lane said *"Let me test that directly"* and was killed before it
could. **Untested.**

---

## LEAD 3 (W25-UI) — an alias edit left dirty and unadjudicated

W25 had `scripts/symbol_aliases.json` modified and had just decided — correctly
— to commit its analysis tools first so that only the alias edit would be dirty
for the A/B. It never got to the A/B.

⛔ **Whoever picks this up must not simply measure it.** Adding an alias lifts
the score **by construction**, and the `none` control **cannot catch a
fabrication**. The alias needs **retail-byte evidence that the fold is real**
(relocation-normalised body identity with target names compared), not a
name-similarity argument — and `TEMPLATE_ARGS_DIFFER` is *what a proven fold
looks like*, so name similarity is the least informative feature available.

**If it is not provable, withdraw it and record why.** That is a full result.

---

## Committed and surviving from these lanes

| lane | branch | committed |
|---|---|---|
| W23 | `w23-frame` | whole-binary detector for the W22 frame-shortfall signature; collectability + cross-instrument frame control |
| W24 | `w24-pairing` | pairing-channel model (W20's bound → a prediction); re-home census + `whereis` gate |
| W25 | `w25-ui` | scope + collectability census for the UI cluster |

W24's commit message asserts *"the inverse lever is REAL and NEARLY EMPTY"* —
⚠ **that is the lane's own in-flight summary and it was never A/B'd.** Treat it
as a lead like everything else here.
