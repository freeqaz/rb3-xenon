# Console default.xex byte verification — Same-Instrument TU5 patch

**Session date:** 2026-07-09
**Console:** 192.168.8.180:21 (FtpDll, user xboxftp) — **UNREACHABLE this session** (see Task 1/4 status below).
All conclusions below rely on a local copy (`/tmp/xbxpull/verify_default.xex`) that was pulled from the
console in a prior session (file mtime 2026-07-09 03:12:54 UTC, ~9 min before this task started) and whose
SHA-256 hash matches the expected patched hash **exactly**. This is strong evidence of console state as of
that pull, but it is **not a live re-confirmation** — the console could theoretically have been modified
since. Flagged explicitly wherever this caveat applies.

---

## 0. Live console connectivity (BLOCKED)

- `ping 192.168.8.180`: 0/3, 0/2 replies — 100% loss.
- TCP connect to port 21: timed out on 3 separate attempts across the session (ftplib connect timeout,
  raw `/dev/tcp` probe, retried after ~10s).
- No ARP cache entry for 192.168.8.180.
- **Conclusion: console is currently off / not on the LAN / firewalled. Tasks 1 (live LIST/SIZE/RETR) and
  4 (FTP recon: xexp/TU/launch.ini/rb3.ini) could NOT be performed live this session.** They are marked
  BLOCKED below with whatever can be inferred from prior local artifacts.

---

## 1. Local file hashes (`/tmp/xbxpull/`)

| File | mtime (UTC) | Size | SHA-256 | Matches |
|---|---|---|---|---|
| `default.xex` | 03:07:40 | 13,971,456 | `6639ce25745505b598480499ca53b421fdec5604d813f5ee2c8152ecdad2a5ea` | **CLEAN TU5** (unpatched) reference hash — NOT the patched hash |
| `verify_default.xex` | 03:12:54 | 13,971,456 | `9c5965ad7df7e1d34f49501d6dbe1754520868c06156dfebd0ceb9f8707d1c6f` | **Expected PATCHED hash — EXACT MATCH** |
| `default_vanilla.xex` | 03:07:31 | 13,807,616 | `cd472d07f4657a92f6182d8bb2a2508059a545adde012642ac94af8375d34d73` | different size — pre-TU5 vanilla build (not directly relevant) |

Reference files (`.claude/worktrees/tu5-migrate/orig/45410914/`):
- `default_tu5_patched.xex` → `9c5965ad7df7e1d34f49501d6dbe1754520868c06156dfebd0ceb9f8707d1c6f` (matches expected patched hash)
- `default_tu5.xex` (clean) → `6639ce25745505b598480499ca53b421fdec5604d813f5ee2c8152ecdad2a5ea` (matches expected clean hash)

**Finding:** `/tmp/xbxpull/verify_default.xex` is byte-identical to the known-good patched reference XEX
(`default_tu5_patched.xex`). `/tmp/xbxpull/default.xex` — despite its filename — is byte-identical to the
**unpatched clean TU5** reference, i.e. it is NOT the patched file. This is most consistent with it being an
earlier pre-patch capture (pulled at 03:07:40, ~5 min before `verify_default.xex` at 03:12:54) rather than the
current console state; the `verify_` prefix and later timestamp indicate it was pulled specifically to confirm
the patch after applying it. **Do not use `/tmp/xbxpull/default.xex` as evidence of console state** — it is
stale/pre-patch. `verify_default.xex` is the operative artifact for this report.

No re-pull was necessary/possible (console offline); `console-default.xex` was NOT created.

---

## 2. Detour-site + cave byte verification

Ran `xex_binpatch_tu5.py verify` (full report: `/tmp/si-hw-fix/verify_default_report.json`) plus a second,
independent direct-read script using the same `Tu5Mapper` VA→file-offset logic, against
`/tmp/xbxpull/verify_default.xex`.

Also ran the same verify against `/tmp/xbxpull/default.xex` as a sanity/negative-control check — confirms the
tool correctly reports FAIL against the unpatched file (all 4 detours still `7D8802A6` = `mflr r12`, cave
region unwritten, whole-file diff = 0 changed bytes vs. clean reference). This validates the tool's mapping
logic is not silently always-passing.

### Detour sites — `verify_default.xex` (both `xex_binpatch_tu5.py verify` and direct raw read agree)

| VA | Section | File offset | Expected word | Actual word | Decoded | Result |
|---|---|---|---|---|---|---|
| `0x826684C0` | `.text` | `0x6634C0` | `0x48621BC0` | `0x48621BC0` | `b 0x82C8A080` | **PASS** |
| `0x825B6488` | `.text` | `0x5B1488` | `0x486D3C38` | `0x486D3C38` | `b 0x82C8A0C0` | **PASS** |
| `0x8276FA08` | `.text` | `0x76AA08` | `0x4851AEA8` | `0x4851AEA8` | `b 0x82C8A8B0` | **PASS** |
| `0x82794740` | `.text` | `0x78F740` | `0x484F5E50` | `0x484F5E50` | `b 0x82C8A590` | **PASS** |

Capstone confirms all 4 decode as plain unconditional `b` branches whose displacement lands exactly on the
expected cave sub-entry point.

### Detour sites — `/tmp/xbxpull/default.xex` (negative control, confirms this file is UNPATCHED)

All 4 sites read back `0x7D8802A6` (`mflr r12`, the original/pre-patch instruction) — i.e. this file has
**none** of the patch applied. (This is expected/consistent with it being the pre-patch capture, not evidence
of a problem with the console.)

### Cave region (`0x82C8A000`..`0x82C8AAF0`, .data section)

- `xex_binpatch_tu5.py verify --cave-bin same_instrument_cave_tu5.bin`: **700 words compared, 0 mismatches**
  (the one intentional exception — flag word blob-0 → patched-1 — is accounted for by the tool's comparator).
- Direct read confirms cave base (`0x82C8A000`) file offset `0xC85000` in `.data`, first 0x40 bytes:
  `81630050 548a103a 7c6b502e 4e800020 816300b0 548a103a 7c6b502e 4e800020 80630010 c4e80002 00000000 00000000 90830010 c4e80002 00000000 00000000`
  — matches the expected cave blob bytes 1:1.

### Flag word `gSameInstrumentEnabled` @ `0x82C8AAA0`

- Section `.data`, file offset `0xC85AA0`.
- **Value read: `0x00000001` → flag is SET/ENABLED.**

### Whole-file diff (patched vs. clean `default_tu5.xex`)

- `changed=592, expect=675, extra=0, noop=83 (all noop bytes == clean-file value, i.e. they were no-op writes even in the toml) → whole_file_diff exact: PASS`.
- **No bytes outside the intended write-set differ.** The patched file is the clean TU5 image plus exactly
  the intended 675 word-writes (4 detours + 671 cave words), nothing else.

**Task 2 verdict: ALL 4 detour sites PASS, cave region matches blob byte-for-byte, enable flag = 1, whole-file
diff shows no stray changes.** (Caveat: verified against the local `verify_default.xex` copy, not a live
re-pull — see §0.)

---

## 3. PE section table / NX risk analysis

Parsed the XEX2's own embedded basefile PE header directly (`pe_off = 0x3000`, confirmed `MZ`/`PE\0\0` magic
present; `compression=1` "basic"/block format consistent with the TU5 section-mapped image type the patch
tool assumes). Cross-checked against `band_tu5.exe` (dtk-recovered reference PE) — **section table is
byte-identical** between the two (same VAs, sizes, and `Characteristics` values), which is expected since both
describe the same original build's memory layout.

| Section | VA range | Characteristics (raw) | Decoded flags |
|---|---|---|---|
| `.rdata` | `0x82000400`-`0x821F1584` | `0x40000040` | READ, INITIALIZED_DATA |
| `.pdata` | `0x821F1600`-`0x82262228` | `0x40000040` | READ, INITIALIZED_DATA |
| `BINKCONS` | `0x82262400`-`0x82264D20` | `0x40000040` | READ, INITIALIZED_DATA |
| `.text` | `0x82270000`-`0x82C4CE3C` | `0x60000020` | EXECUTE, READ, CODE |
| `BINK` | `0x82C4D000`-`0x82C5D010` | `0x60000020` | EXECUTE, READ, CODE |
| `BINKBSS` | `0x82C60000`-`0x82C643A0` | `0xC0000080` | READ, WRITE, UNINITIALIZED_DATA |
| **`.data`** | **`0x82C64400`-`0x82E5A2AC`** | **`0xC0000040`** | **READ, WRITE, INITIALIZED_DATA — NO EXECUTE** |
| `BINKDATA` | `0x82E5A400`-`0x82E5E154` | `0xC0000040` | READ, WRITE, INITIALIZED_DATA |
| `.XBMOVIE` | `0x82E5E200`-`0x82E5E20C` | `0xC0000040` | READ, WRITE, INITIALIZED_DATA |
| `.idata` | `0x82E60000`-`0x82E60528` | `0xC0000040` | READ, WRITE, INITIALIZED_DATA |
| `.XBLD` | `0x82E70000`-`0x82E70160` | `0x42000040` | READ, DISCARDABLE, INITIALIZED_DATA |
| `.reloc` | `0x82E70200`-`0x82F74298` | `0x42000040` | READ, DISCARDABLE, INITIALIZED_DATA |

**`0x82C8A000` (cave base) falls inside `.data` (`0x82C64400`-`0x82E5A2AC`).**

**NX finding: `.data`'s PE `Characteristics` = `0xC0000040` — `IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE |
IMAGE_SCN_CNT_INITIALIZED_DATA`. The `IMAGE_SCN_MEM_EXECUTE` bit (`0x20000000`) is NOT set.** By the PE
section table alone, this region is RW but not X.

**Risk implication for real hardware:** the code cave lives in a section the shipped binary's own PE metadata
marks non-executable. Whether this actually causes an instruction-fetch fault on the real console depends on
how the XEX loader on that specific console sets up page protection:
- If the loader configures NX/XN page attributes strictly from these PE section characteristics (as a stock,
  signed-kernel Xbox 360 title loader nominally would), branching into `.data` would fault.
- In practice, homebrew-capable consoles (JTAG/RGH, which is required to run an unsigned/patched XEX at all)
  typically run under a hacked kernel/hypervisor (e.g. via a devkit-mode exploit) that either does not enforce
  per-section NX for title memory, or maps the whole title image RWX. This is the usual reason self-modifying-
  code / code-cave patches work in practice on jailbroken 360s despite PE flags saying otherwise — but it is a
  property of the **kernel/hypervisor exploit environment**, not of this XEX, and was **not independently
  confirmed on this console** (couldn't reach it this session). If the patch fails to run / crashes on real
  hardware despite byte-correct patching, this NX gap is the top suspect to investigate next (e.g. via
  DashLaunch's memory-protection-relaxation options, or moving the cave into `.text`/`BINK` (already
  EXECUTE+READ) instead of `.data`).

This finding is **independent of and orthogonal to** the byte-correctness result in §2 — the bytes are
provably correct; whether the CPU is permitted to execute them is a separate, unverified question on this
console.

---

## 4. FTP recon (BLOCKED — console unreachable)

Could not perform any of: `/Usb1/Games/rb3` LIST/SIZE (beyond the local-hash cross-check in §1),
`/Hdd1/Cache` LIST, `/Hdd1/Content/0000000000000000/45410914/000B0000/` LIST, `launch.ini` pulls
(`/Hdd1/`, `/Usb0/`, `/Usb1/`, `/Flash/`), `rb3.ini` pull, or `/Usb1` root LIST (RB3ELoader.xex size/date).

**No alternate-image-shadowing risk (title update / .xexp) could be positively ruled in or out this session.**
This is an open item — re-run Tasks 1 and 4 next time the console is reachable. Nothing under
`/tmp/si-hw-fix/pulled/` was populated (no files pulled).

**Recommendation when console is back online:** prioritize the TU/xexp checks first (`Hdd1/Cache`,
`Hdd1/Content/.../45410914/000B0000/`, any `default.xexp` next to `default.xex`) — a title update package or
`.xexp` patch delta shadowing `default.xex` would be a materially different risk than the NX question above,
since it could mean a *different* image runs even though `default.xex` itself is correctly byte-patched.

---

## Summary verdict

| Question | Answer |
|---|---|
| Does console-matching `default.xex` (via `verify_default.xex`, last known good, NOT live-reconfirmed) contain the patch at all 4 detour sites? | **YES — all 4 PASS**, byte-exact |
| Does the cave region match the expected blob? | **YES**, 700/700 words (with expected flag exception) |
| Is `gSameInstrumentEnabled` set? | **YES**, `= 1` |
| Any stray/unexpected byte changes outside the intended write-set? | **NO** — whole-file diff exact |
| Is the cave's containing section (`.data`) marked executable in the PE header? | **NO** — `READ|WRITE` only, `MEM_EXECUTE` bit absent. Real-hardware NX risk, unconfirmed either way on this console. |
| Live TU/xexp shadow-image risk on console right now? | **UNKNOWN — console unreachable this session, recon blocked** |
| `rb3.ini` / `launch.ini` contents? | **UNKNOWN — console unreachable this session, recon blocked** |

**Bottom line:** the file we have strong hash-based reason to believe is (or very recently was) the console's
`default.xex` is byte-perfect against the intended patch. The two open risks this session could NOT close are
(a) whether a title update or `.xexp` is shadowing that file at boot, and (b) whether the `.data` section's
lack of the PE `MEM_EXECUTE` flag actually blocks execution of the cave on this console's specific
loader/kernel. Both require live console access to resolve.
