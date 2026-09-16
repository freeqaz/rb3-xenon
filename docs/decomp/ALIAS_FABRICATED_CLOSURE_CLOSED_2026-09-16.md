# The 9,393 `FABRICATED_CLOSURE_NOT_PARTITION` withdrawals are CLOSED — not a lever

**Date:** 2026-09-16 · **Lane:** coordinator (no worktree, read-only) · **Tree:** `efbba060`
**Verdict:** do **not** dispatch a lane here, in either of the two forms it invites.

This class is the largest single population in the withdrawal ledger — 9,393 of
10,243 records (91.7%) — and it has now been picked up and put down three times
in this campaign. It reads like the biggest queued item in the tree. It is not a
queued item at all. This doc exists so the fourth pickup stops here.

## 0. What it looks like, and why that is misleading

`W16-CU` flagged it as untouched while building and validating the exact
instrument for its own (20× smaller) class, and both CU §7 and W16-CP §7 say the
same careful thing: *"Whether the same precedence governs them is unknown and
must not be assumed from this lane."* That is correct and was never a licence to
restore them — but at 20× the size it reads like the headline lever.

⚠ **Instrument note, recorded because it nearly closed the vein for the wrong
reason.** A first sample keyed on `w.get('reason')` printed **`groups carrying
this class: 0`** against a census of 9,393. That was a **wrong-key read**, not a
vacuity in the artifact: the class is carried under **`class`**, and the census
had used a fallback chain `reason or class or why`. A decisive-looking emptiness
is this project's signature failure mode, and here it appeared on the *coordinator's*
side. Re-read correctly before any of the below was believed.

## 1. The shape settles it — the groups are SHELLS, not traps

Measured on `scripts/symbol_aliases.json` at `efbba060`:

| measure | value |
|---|---:|
| groups carrying the class | **357** |
| withdrawn memberships of the class | **9,393** |
| live `folded[]` memberships in those same groups | **391** |
| withdrawn : live | **24.02 : 1** |
| star-groups now **EMPTY** (zero live folded members) | **340 of 357** |
| star-groups still carrying any live fold | **17** |

The largest are `_ECX2SubmixVoice` / `_ECX2SourceVoice` / `_GCAudioSRC` /
`_GCAudioFilter` / `_GCX2Engine` / `_ECX2SourceVoiceWMA`, each **217 withdrawn /
0 live**; then four separate `insert` survivors at 86 withdrawn / 0 live.

Group idx 95 (`insert` @ `0x822b55e0`) withdraws
`list<int>::insert`, `list<float>::insert` and 84 further instantiations against
one survivor. Those are **distinct template instantiations with different node
deallocators** — bodies that cannot fold, because MSVC folds only COMDATs
identical *including relocations*. Restoring them would forgive
`list<int>::insert` against `list<float>::insert`.

⇒ **There is no trapped real fold to recover in 340 of 357 groups.** The
withdrawals emptied them; what remains is a shell.

## 2. Form A — bulk restore — is fabrication at scale

`matched_code` forgiveness via `SymbolEquivalences` is worth **818,416 B /
7.93 pp** (lane ALIAS-2), i.e. ~22% of everything we count as matched. An
*unproven* alias therefore lifts the shipped `name_check` ruler **by
construction**, and the `none` control reads flat **by construction too** —
so flatness here is the hazard's signature, never a clearance. 9,393 restorations
would be the largest unproven bulk edit this campaign has ever been offered, and
ALIAS_HYGIENE §10 already names it as *"the unproven bulk edit this campaign
keeps refusing."*

## 3. Form B — the H1 partition repair — was BUILT, MEASURED and REVERTED

This is the part that is easy to miss, and it is written in the generator's own
source at `tools/icf_alias_build.py:668`:

> ⛔ THE UNION KEY IS `t` ALONE … the evidence is a **STAR** and the emitted
> group is a **CLIQUE**; two folded spellings are never compared.
>
> DO NOT "fix" this with decomp-synth's resolved-operand read (gate (g)). That
> was built, measured and reverted (**`760cb450`**): it asks whether OUR bodies
> agree, **ICF happened in RETAIL's link**, and on a 36.7%-matched tree the gap
> is systematic. Over the 517 memberships it withdraws, band.exe confirms **164
> TRUE** and refutes **at most 57** — **three true folds discarded per
> fabrication caught.** A correct partition must be anchored on the retail
> image; **none is ready.**

So the partition repair is not unexplored headroom. It is a **closed experiment
with a measured 3:1 loss ratio and a named unmet precondition.** `760cb450`'s
own subject line is the rule: *"Revert the generation-time partition: its
predicate is the wrong court."*

⚠ **Do not read ALIAS_HYGIENE §3.4's "all 10 invariant-breakers are PROVEN" as
this population's upside.** That is a **10-member** survivor-collision set
(a spelling that is also another group's survivor), not the 9,393. Conflating
the two inflates the apparent prize by three orders of magnitude — the
coordinator did exactly that before measuring.

## 4. The durability half is already DONE, not pending

ALIAS_VEIN §H1 recommended *"the generator read `withdrawn` as a hard denylist
keyed on (address, spelling)"* and marked it not done. **ALIAS_DURABILITY landed
it**, and it is live in the current source — verified by reading, not inherited:
`ledger.lookup(t, addr_of.get(t), b)` gating every accept (`icf_alias_build.py`
~:655, `reject_withdrawn` stat, `--allow-withdrawn` as the explicit override),
and applied again to the `--merge` carry-forward (~:833).

| | groups | live memberships | withdrawn re-emitted as LIVE |
|---|---:|---:|---:|
| unguarded regeneration (pre-W8-A) | 1,002 | 4,884 | **110** |
| guarded regeneration | 942 | 4,775 | **0** |

**14 of those 110 were this very class.** So the standing risk ALIAS_VEIN warned
about — a regeneration silently re-fabricating these — is closed.

## 5. What would reopen it

Exactly one thing: **a partition predicate anchored on the RETAIL image**
(relocation-normalized body hashing over `.pdata`-authoritative extents against
a random-offset null — the CD-7 instrument), not on our-build ICF congruence.
Until such an instrument exists and is shown able to FAIL, this vein stays shut.
Artifacts from the reverted attempt, if anyone builds that:
`<decomp-bench>/archive/runs/2026-08-20-gen-partition/` (numbers) and
`2026-08-20-reloc-reconcile/tools/` (instruments).

## 6. What this lane did NOT do

* Did not edit `scripts/symbol_aliases.json` — W16-CX holds it this block.
* Did not regenerate the alias artifact. A regeneration is guarded now, but it
  is still not free, and nothing here needed one.
* Did not adjudicate the **17** star-groups that still carry live folds. They are
  a small, real population and are *not* covered by this closure — but at 391
  live memberships across 17 groups they are an ordinary alias-audit question,
  not the 9,393-row lever this doc closes.

---

## Appendix (coordinator, 2026-09-16, later the same day) — WHOLE-LEDGER COROLLARY

**This doc closed ONE class. A second instrument, run independently while three
lanes were in flight, closes the ENTIRE withdrawal ledger.** I came back to this
vein anyway — I was the fourth pickup this doc was written to stop — and the
measurement is recorded here rather than in a new file so the two cannot drift.

**Instrument:** survivor-keyed, deduplicated liveness over every group in
`scripts/symbol_aliases.json` carrying a `withdrawn` record, against the current
`report.json`. Keyed on the group's **survivor** (the name a report row actually
carries), *not* on the withdrawn spelling.

| measure | value |
|---|---:|
| groups with a withdrawn record | 503 |
| distinct survivors (deduplicated) | 503 |
| **STILL LIVE (survivor row < 100 fuzzy)** | **33, worth 3,020 B** |
| DEAD (survivor already at fuzzy 100) | 433, was 54,784 B |
| UNRESOLVED | 37 (7.4%) |

⇒ **~95% drained by bytes.** The largest live survivor is **164 B**, and several
top "live" rows sit at **fuzzy 0.0000**, i.e. unpaired/unpairable — not
forgiveness candidates at all. **Do not fund a lane on the withdrawal ledger in
any class, not just this one.**

⛔ **Two instrument failures on the coordinator's side, both worth more than the
result.**

1. **A first pass keyed on the withdrawn SPELLING returned 9,766 UNKNOWN against
   338 resolved — 96.7% unresolved — and I nearly reported its 136-live / 14,572 B
   split as a sizing.** It is not a sizing; it was computed on 3.3% of the data,
   with no deduplication (one 116 B row appeared ten times). The cause is
   conceptual, not clerical: **a withdrawn spelling is BY DEFINITION a name the
   row does not carry**, so looking it up among report rows asks the wrong
   question. The 7.4% unresolved figure above is quoted precisely because the
   96.7% one disqualified its own pass.
2. **That same pass used the fallback chain `reason or class or why`** — the
   exact wrong-key family §0 of this doc already warns about. The warning was in
   the file I was standing in and I reproduced the error anyway. ⇒ **§0's
   instrument note is not a historical footnote; re-read it before keying any
   census on this artifact.**

★ **Why the byte figure above must NOT be reused as a sizing of alias
forgiveness:** ALIASAUDIT-1 established that this mechanism is sized by
**ABLATION, never by a name-keyed census** — the name-keyed census booked its own
blindness as risk. The table above is a **liveness** measurement ("is this
group's row still open?"), which is a legitimate question for a name key. Pricing
the bytes is not.
