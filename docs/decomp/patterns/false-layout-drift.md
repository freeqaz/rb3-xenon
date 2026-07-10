# False Layout Drift — offset diffs that are NOT struct/header bugs

A large fraction of "uniform offset delta" near-misses are **not** layout bugs.
They *look* like a member was added/removed (a clean `±N` across several
loads), but the class layout is byte-correct and editing the header will
regress. This doc catalogs the recurring false positives so you rule them out
**before** touching a header.

**Why this matters:** across two offset-drift sweep rounds (2026-07-10), recon
refuted the raw "layout drift" reading in the *majority* of ranked candidates.
Every refutation was one of the patterns below. The cost of the check is one
Ghidra decompile; the cost of skipping it is a header edit that shifts a
40-TU cascade and a wasted A/B.

See [`../playbooks/offset-drift-sweep.md`](../playbooks/offset-drift-sweep.md)
for the sweep that surfaces these, and
[`fixable-struct-layout.md`](fixable-struct-layout.md) for when the drift **is**
real.

> **The one-line test:** compute the **absolute** target address on both sides
> (`base_register_value + offset`), not the offset alone. If the absolutes
> match, there is no layout drift. `delta = ours − retail`; TGT = retail,
> SRC = ours.

---

## 1. Anchor-bias artifact (the dominant false positive)

**Symptom:** a small number of "uniform struct delta" loads where TGT and SRC
use **different base registers**.

```
retail: addi r25, r27, 0x18 ; lfs f0, 0x0,  r25 ; lfs f13, 0x18, r25
ours:   addi r25, r27, 0x30 ; lfs f0, -0x18, r25 ; lfs f13, 0x0,  r25
```

Both sides read `r27+0x18` and `r27+0x30` — **identical absolute addresses**.
The compiler simply chose a different base bias for the induction/anchor
pointer. The "+124"/"−24" deltas are `ours_offset − retail_offset` across
*different* bases, which is meaningless.

**Free witness:** the `__savegprlr_N` prologue index. Retail saving `r25`
where we save `r26` means retail's source keeps one extra long-lived address
local (a hoisted `&member` reference) — that's a *body-shape* lead, not a
layout bug.

**Verdict:** PERMUTER_CLASS or body-port. Do NOT touch the header.
Seen: `RGGemMatcher::FretMatchImpl`, `RandomGroupSeqInst` ctor,
`StoreMenuPanel::Handle` (anchor `this+0x5c` vs `this+0x54`),
`FreeCamera::Poll`, DSP `SynapseAPO`.

---

## 2. Vbase-anchor mirage (trailing member)

**Symptom:** a class with a **virtual base** (`public virtual Hmx::Object` —
the whole UIPanel/UIComponent family) shows uniform drift where the addressing
is anchor-relative *negative* offsets (`lwz r11, -0x64, r26`).

MSVC compiles methods of a vbase-derived class against the **vbase anchor**
register, not `this`. A trailing Wii-only member moves the anchor placement,
so every anchor-relative access shows a uniform delta — but the members' real
offsets from `this` are **identical** to retail.

**Distinguish from a real fix:** if the culprit is a *trailing* member, the fix
is still "delete the Wii-only member" and it IS real (deleting it moves the
anchor back). What's a *mirage* is concluding that some named member "moved" —
none did. Verify by decompiling the retail ctor and reading the actual member
store offsets.

Seen (real, closed by deleting the member): `LabelNumberTicker::unk74` (a
*mid-class* member — later members drift `+8`, earlier anchor-relative negative
offsets drift `−8`), `InlineHelp::mResourceDir` (`+0x10` vbase anchor bump).
Seen (pure mirage, no edit): the round-1 panels family where "member drift" was
entirely the moved anchor.

---

## 3. Diagonal-pairing artifact (insert/delete reorder)

**Symptom:** "uniform +N struct drift" but `inserts=0 deletes=0`, OR a diff
where each example's SRC equals the *next* row's TGT.

objdiff pairs instructions positionally. When one side schedules an
instruction a slot earlier (or an insert/delete cluster shifts the stream), the
differ pairs a load against its *neighbor*, manufacturing a phantom offset
delta between two identical instruction sequences.

**Tell:** run `run_objdiff full_listing=true`. If both sides emit the identical
sequence (e.g. the 4-word `lwz r7,0x0/r6,0x4/r5,0x8/r4,0xc` Vector3 copy) and
only the *scheduling* differs, there is no layout drift.

**Verdict:** scheduling/permuter-class. Seen: `UtilDrawPlane` (the "+4" was a
one-slot-early first `lwz`), `inflate` (the "+76" was inside a 2-delete/2-insert
`inf_leave` cluster), `Synth360::SetupHeadsetSubmixes` (+192 pairing noise).

---

## 4. Red-zone / stack addressing misread as struct

**Symptom:** "struct −16" deltas where the base register is actually pointing
into the function's **own stack** (`r10 = r1-0x10`, `r9 = r1-0xc`).

The sweep's `struct` class is "any base reg that isn't r1/r30/r31". A function
that materializes stack pointers into GPRs (e.g. a vector splat: load once,
store thrice through `r1`-relative pointers) trips this. The struct parameter
is never dereferenced.

**Verdict:** copy-idiom / permuter-class. Seen: `GainEffect::DoProcess` (the
param is ignored by both sides; the −16 is a red-zone splat).

---

## When it IS real — the promotion checklist

Promote to a header edit only when ALL hold:

1. The delta is **uniform-signed** across **≥3** struct/global diffs.
2. Base registers match (or resolve to the same absolute after bias) — i.e.
   it survived the anchor-bias check (#1).
3. `full_listing` shows real inserts/deletes, not a pure reorder (rules out #3).
4. A retail witness confirms the member count/size: the ctor's member-init
   stores, or a `li rN, <sizeof>` feeding operator new, decompiled from the
   retail address.

Then the fix is one of the confirmed families in the sweep playbook: delete a
Wii/DC3-only member, de-inline a helper, reorder TU statics, or adopt the
rb3-Wii convention. Register the *refuted* candidates in
`scripts/harvest/nearmiss_verdicts.json` so future rounds skip them.

## See Also

- [`../playbooks/offset-drift-sweep.md`](../playbooks/offset-drift-sweep.md) — the sweep + recon loop that produces these candidates
- [`fixable-struct-layout.md`](fixable-struct-layout.md) — real struct/member layout bugs and the `.bss` emission-order lever
- [`fixable-inline-boundary.md`](fixable-inline-boundary.md) — de-inline-to-match, the fix when a caller's "drift" is an inlined helper
- [`unfixable-compiler.md`](unfixable-compiler.md) / [`at-limit-systemic.md`](at-limit-systemic.md) — where the permuter-class residues land
