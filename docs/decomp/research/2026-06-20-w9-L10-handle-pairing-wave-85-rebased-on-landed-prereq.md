# W9 L10 — Handle-pairing wave "85" (rebased on landed prereq a7175af)

**Date:** 2026-06-20
**Mode:** Adversarial DISCOVER/PLANNER (read-only in main @ 812e1df)
**Frontier item:** `handle-pairing-wave-85-rebased-on-landed-prereq` (kind=pin-pair, est +45)
**Verdict:** REAL_ACTIONABLE — workflow proven, but the "85 / +45" estimate is **optimistic**;
ground-truthed verified floor is **22 byte-exact unmapped Handles** (with a handful more
plausibly recoverable by objdiff that my offline tool can't measure).

---

## 1. Premise check — is a7175af landed?

NO. main HEAD = `812e1df` (WAVE-8 CLOSE, 8314 matched). `a7175af` is a **single commit on top
of 812e1df** that is NOT yet an ancestor of main:

```
git merge-base --is-ancestor a7175af 812e1df  -> FALSE (exit 1)
git log HEAD..a7175af  -> a7175af "Handle macros: reconcile MessageTimer gate + END PathName tail (+196 @100%)"
```

So the entire frontier is **gated on landing a7175af first.** a7175af content
(verified by `git show --stat`):
- `src/system/obj/Object.h` (+43): BEGIN_HANDLERS drops per-handler `MessageTimer` behind a
  new `MILO_MESSAGE_TIMERS` macro (undefined = retail shape); END_HANDLERS/END_CUSTOM_HANDLERS
  emit the comma-evaluated `(void)(PathName(this), sym)` tail (retail keeps the virtual
  FindPathName side-effect even though MILO_NOTIFY text is stripped — same shape as the
  MILO_FAIL Find<T> fix).
- `config/45410914/objects.json` (6 units opt **in** to `/DMILO_MESSAGE_TIMERS`: BandDirector,
  CharBoneDir, UI, Dir, Group, CharLipSync — these units' Handle DOES emit the timer).
- `src/system/ui/UIComponent.{cpp,h}`: get_resource_dir / get_resources_path port.
- `scripts/target_symbol_map.json` +7 Handle pairings.
- Claimed whole-binary A/B: 8314 -> 8510 (+196). **This commit must land before any handle-pair
  wave can be measured.** It is binary-wide (Object.h is a foundational header) so it is the
  classic #1 cross-TU regression source — it needs its own composed whole-binary verify on land.

`a7175af` exists in two equivalent forms: branch `w9-land-reconcile-handle-prereq-9fb9016`
(checked out at `wt-w9-land-reconcile-handle-prereq-9fb9016`, HEAD a7175af) and the chain
`9fb9016` under `wt-w9-handle-pair-tier1-char-clean`. Pick a7175af to land.

## 2. Worktree census — which are on-prereq vs need re-cut

`git merge-base --is-ancestor a7175af <branch>`:

| worktree | branch HEAD | on-prereq? |
|---|---|---|
| wt-w9-handle-pair-clean-char-tier-7-post-prereq | 80a2857 | **YES** (harvest first) |
| wt-w9-handle-pair-batch-mid-tier-per-fn-verify | 812e1df | NO — re-cut |
| wt-w9-handle-pair-batch-small-tier1-char-clean | 812e1df | NO — re-cut |
| wt-w9-handle-pair-tier1-char-clean | 2260dd0 (on 9fb9016==prereq) | YES actually* |
| wt-w9-handle-pair-tier2-single-handler | 117cab5 | NO — re-cut |
| wt-w9-handle-wave-tiny-engine-ui-rnd | f9588ba | NO — re-cut |

\* `tier1-char-clean` (2260dd0) sits on `9fb9016` which is byte-identical content to a7175af
(both "+196 @100%" Object.h fix) — its 5 added entries are valid IF 9fb9016 is what lands.
But the *clean* path is: land a7175af, then re-cut all batch worktrees onto it. The bare-812e1df
worktrees (batch-mid-tier, batch-small-tier1, tier2, tiny) measure the **pre-fix Object.h** so
their match%/objdiff results are INVALID and must be discarded, not trusted.

`clean-char-tier-7` (80a2857 = a7175af + 5 entries) has a FRESH report at 8515 = a7175af's 8510
+ 5. Its 5 entries (CharFaceServo 0x823909C0, CharIKFingers 0x8239FC68, CharSleeve 0x823BD8A8,
Waypoint 0x823C8630, CharPosConstraint 0x823B04E8) are the harvest-first set.

## 3. The pin-pair mechanism (proven REAL)

For class C with a `Handle` virtual: our compiled obj emits
`?Handle@C@@UAA?AVDataNode@@PAVDataArray@@_N@Z`; retail's split target shows anonymous
`fn_<VA>`. Post-prereq the **bodies are byte-identical**, but objdiff can't auto-pair (name
mismatch) so the function reads 0%. Adding ONE line to `scripts/target_symbol_map.json`:

```json
"0xVA": "?Handle@C@@UAA?AVDataNode@@PAVDataArray@@_N@Z",
```

makes `obj_target_symbol_renamer` rename the target `fn_<VA>` -> mangled, objdiff auto-pairs,
+1 @100%. **Confirmed end-to-end:** CharFaceServo 0x823909C0 reads 100.0% in the clean-char-tier-7
report after exactly this one-line add.

## 4. Ground-truth measurement (my independent analysis)

Tooling: parsed `auto_03_82260000_text.obj` (authoritative VA->symbol; LE COFF, machine 0x01F2,
107917 syms, VA = symval + 0x82260000) and the prereq worktree's compiled + split objs. For each
of the **348** compiled `?Handle@...DataNode...` symbols, computed reloc-normalized bytes (zero
4 bytes at each COFF reloc site) and looked for the unique unmapped target `fn_<VA>` with
identical normalized bytes in the same pinned unit.

Result:
- 348 compiled Handle symbols; 14 already mapped in prereq+5; **334 unmapped**.
- **22 RELOC-NORM-EXACT unique pairings** (verified byte-exact, ready to map). 0 ICF-ambiguous.
- 53 `NO_CBYTES` (Handle is last code symbol in its section — my size-bounding can't measure;
  objdiff would). Dominated by FxSend family + Flow* + ObjectDir/BaseMaterial/Character/Rnd.
- 27 `NO_TARGET` (unit not pinned).
- 232 `NO_EXACT` (Handle body genuinely diverges, OR a divergence my mask can't see).

**Adversarial cross-check of a "DIFF" case (CharBone 0x823C4A38):** the diff is real —
target has ~8 extra bytes (2 instructions), shifting all relative branches by +0x8 (primop 18
`b`) AND different registers at 0x9c-0xa4 (primop 31 X-form). This is genuine regalloc/body
divergence, NOT reloc-mask noise — so my DIFF/NO_EXACT classification is sound and the 22 EXACT
are robust. (CharBone is also NOT in the MILO_MESSAGE_TIMERS opt-in set, so it's a true body-port,
not a timer artifact.)

**Honesty correction to the frontier:** "~85 byte-exact / +45" is not supported by ground truth.
Verified floor = **22**. The 53 NO_CBYTES *may* contribute a handful more under objdiff (it
normalizes branch displacement and 2-byte relocs my offline mask doesn't), but my best estimate
for the realistic deliverable is **+22 to ~+30**, not +45. Treat +45 as the optimistic ceiling.

## 5. The 22 verified ready-to-add entries (all VAs free, 0 map collisions, all target fn_VA @0%)

All VAs confirmed as exact function boundaries in auto_03, all inside their unit's pinned
.text span, all units wired (NonMatching), NONE in the MILO_MESSAGE_TIMERS opt-in set.

```
0x823004D8  ?Handle@CrowdAudio@@UAA?AVDataNode@@PAVDataArray@@_N@Z            CrowdAudio.cpp
0x823C6458  ?Handle@CharBonesBlender@@UAA?AVDataNode@@PAVDataArray@@_N@Z      CharBonesBlender.cpp
0x8244EE40  ?Handle@RndMatAnim@@UAA?AVDataNode@@PAVDataArray@@_N@Z            MatAnim.cpp
0x82455138  ?Handle@RndConsole@@UAA?AVDataNode@@PAVDataArray@@_N@Z            Console.cpp
0x8246B950  ?Handle@RndGenerator@@UAA?AVDataNode@@PAVDataArray@@_N@Z          Gen.cpp
0x8246C968  ?Handle@RndParticleSysAnim@@UAA?AVDataNode@@PAVDataArray@@_N@Z    PartAnim.cpp
0x82475348  ?Handle@RndPollAnim@@UAA?AVDataNode@@PAVDataArray@@_N@Z           PollAnim.cpp
0x824769F8  ?Handle@RndTexBlendController@@UAA?AVDataNode@@PAVDataArray@@_N@Z TexBlendController.cpp
0x82477758  ?Handle@RndTexBlender@@UAA?AVDataNode@@PAVDataArray@@_N@Z         TexBlender.cpp
0x824A3170  ?Handle@LightPreset@@UAA?AVDataNode@@PAVDataArray@@_N@Z           LightPreset.cpp
0x824C8800  ?Handle@Spotlight@@UAA?AVDataNode@@PAVDataArray@@_N@Z             Spotlight.cpp
0x824D4978  ?Handle@LightHue@@UAA?AVDataNode@@PAVDataArray@@_N@Z              LightHue.cpp
0x8250FE28  ?Handle@UserMgr@@UAA?AVDataNode@@PAVDataArray@@_N@Z               UserMgr.cpp
0x82515E18  ?Handle@JoypadClient@@UAA?AVDataNode@@PAVDataArray@@_N@Z          JoypadClient.cpp
0x827059B0  ?Handle@FxSendMeterEffect@@UAA?AVDataNode@@PAVDataArray@@_N@Z     FxSendMeterEffect.cpp
0x82788E90  ?Handle@HeldButtonPanel@@UAA?AVDataNode@@PAVDataArray@@_N@Z       HeldButtonPanel.cpp
0x8278AD70  ?Handle@MoviePanel@@UAA?AVDataNode@@PAVDataArray@@_N@Z            MoviePanel.cpp
0x82794BA0  ?Handle@ButtonHolder@@UAA?AVDataNode@@PAVDataArray@@_N@Z          ButtonHolder.cpp
0x82795638  ?Handle@ConnectionStatusPanel@@UAA?AVDataNode@@PAVDataArray@@_N@Z ConnectionStatusPanel.cpp
0x827E5008  ?Handle@UISlider@@UAA?AVDataNode@@PAVDataArray@@_N@Z              UISlider.cpp
0x827E6A10  ?Handle@UIListDir@@UAA?AVDataNode@@PAVDataArray@@_N@Z             UIListDir.cpp
0x827F13B0  ?Handle@UIPicture@@UAA?AVDataNode@@PAVDataArray@@_N@Z             UIPicture.cpp
```

These 22 do NOT overlap the 5 in clean-char-tier-7 nor the 7 in a7175af — they are net-new.

## 6. ICF / interleave trap-ledger note

The LEAD references a `/tmp/census-catb-trap-ledger` gate; that file does not exist this session
(ephemeral). I gated independently: my matcher rejects any candidate with >1 identical-normalized
target body (ICF alias) — got **0 EXACT_MULTI** across all 348, so no ICF mis-pair risk in the 22.
Each VA is also a unique fn boundary in auto_03 and at 0% (unpaired) in the report.

## 6b. ADJACENT DISCOVERY — Handle adjustor-thunks (second clean wave, +33 verified)

There are 218 compiled `?Handle@C@@$4PPPPPPPM@A@...` symbols — multiple-inheritance vtable
adjustor thunks for the real Handle (a fixed `this`-adjust + branch to the real Handle body).
These also become byte-exact once the prereq lands (they only contain the adjustor + a branch).
Running the same reloc-normalized matcher over the **unmapped** thunks:

**33 byte-exact unique pairings** (of 215 unmapped candidates). Same one-line-map-entry mechanism,
same prereq gate, zero extra source. This is a *separate, additive* work-item from §5's 22 —
realistically pushing the combined deliverable to **~+55** (22 main Handles + 33 thunks), still
under the +45 only if you count thunks. Note: a thunk pairs cleanly whether or not its real
Handle is mapped (it's matched by its own bytes), so it can be batched independently. Generate the
exact 33 VA->name list with `python3 /tmp/handle_thunks.py $WT` (extend it to emit the list — it
currently prints only the count; the inner `hits[0]` is the target fn_VA and `h` the mangled name).

## 7. Reproduce

```
WT=/home/free/code/milohax/wt-w9-handle-pair-clean-char-tier-7-post-prereq
python3 /tmp/handle_full.py $WT      # the 22 EXACT pairings (reloc-normalized)
python3 /tmp/check_va.py 0x...       # confirm VA is a fn boundary in auto_03
```
(Scripts left in /tmp this session: handle_full.py, handle_match.py, handle_sizematch.py,
handle_relocmask.py, check_va.py, scan_handles.py.)
