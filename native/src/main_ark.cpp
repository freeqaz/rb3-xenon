// rb3-xenon native — M13: mount the real RB3 .ark archive and prove it.
//
// Usage:
//   ./build/rb3-ark <dataDir> <referenceFile> [arkPath] [--corrupt] [--dump <p>]
//
//   dataDir       directory containing gen/main_xbox.hdr and gen/main_xbox_*.ark
//   referenceFile a pre-extracted copy of arkPath, read with plain POSIX I/O
//   arkPath       archive-relative path (default songs/gen/songs.dtb)
//   --corrupt     NEGATIVE CONTROL: flip one byte of the ark-read buffer before
//                 hashing. The run MUST then report FAIL and exit non-zero.
//                 This exists because a pass criterion that cannot fail proves
//                 nothing (this project has shipped a test whose criterion was a
//                 negative grep, and which reported PASS while dumping core).
//   --dump <path> write the ark-read bytes out so an EXTERNAL tool (coreutils
//                 sha256sum) can verify them independently of the SHA-256
//                 implementation in this file.
//   --config-dump <path>
//                 write config/band_preinit_keep.dta, read as retail's
//                 PreInitSystem reads it, in tools/retail_boot_config.py's
//                 canonical form; that tool compares it with the config it
//                 rebuilds from the shipped .dtb files (W16-UA).
//
// WHY THIS FILE IS THE ORACLE, not a plausible-looking printout:
//   songs/gen/songs.dtb lives ~3.34 GB into the logical archive, i.e. past
//   2^31. Reaching it requires Archive::GetFileInfo's cumulative walk over the
//   ten ark parts to be done in 64-bit and the right part to be selected. Any
//   truncation, off-by-one part, or 32-bit overflow yields different bytes and
//   the digest diverges. A byte-exact SHA-256 match therefore certifies the
//   whole multi-ark path, not just "a file opened".
//
// Exit codes: 0 = all gates passed. 1 = a gate failed (or setup failed).

#include "obj/Data.h"
#include "obj/DataFile.h"
#include "os/Archive.h"
#include "os/Debug.h"
#include "os/File.h"
#include "os/System.h"
#include "utl/Str.h"
#include "utl/Symbol.h"

#include "ark_verify.h"
#include "retail_boot_macros.h"
#include "os/Joypad.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <vector>

extern void InitMakeString();
// native/src/platform/File_Native.cpp
extern void NativeSetDataDir(const char *dir);
// native/src/platform/System_Native.cpp
extern void NativeArchiveInit();

// ---------------------------------------------------------------------------
// SHA-256 and the POSIX reference reader now live in ark_verify.h, shared with
// main_midi.cpp (lane CD-3) instead of being copied into each driver. Behaviour
// here is unchanged -- the full gate suite below was re-run after this
// extraction and reproduces the identical digest, the same 8 gates and rc=0.
// The rationale for hashing at all is documented in that header: a self-hash
// cannot fail for the reason we care about, so --dump + an external checker and
// a direct memcmp carry the real weight.
// ---------------------------------------------------------------------------
namespace {

    using arkverify::ReadWholeFilePosix;
    using arkverify::Sha256Hex;

    int gFailures = 0;

    void Gate(const char *name, bool ok, const char *detail) {
        printf("  [%s] %s%s%s\n", ok ? "PASS" : "FAIL", name,
               detail && *detail ? " — " : "", detail ? detail : "");
        if (!ok) gFailures++;
    }
}

// ---------------------------------------------------------------------------
// W16-UA: config/band_preinit_keep.dta as retail's PreInitSystem reads it:
// with REGION_NA, HX_XBOX, HX_WIN, HX_NG and _SHIP defined (main() calls
// RetailBootMacros::Define() right after Symbol::Init), inside one
// BeginDataRead session, after DataInit (whose script functions the config's
// #autorun needs). The gates check what each content-bearing macro selects in
// the shipped data; the full comparison is tools/retail_boot_config.py over
// --config-dump.
namespace {
    DataArray *Sub(DataArray *a, const char *tag) {
        return a ? a->FindArray(Symbol(tag), false) : nullptr;
    }

    int DetectType(DataArray *controllers, const char *name) {
        DataArray *c = Sub(controllers, name);
        DataArray *t = Sub(Sub(c, "detect"), "type");
        return t && t->Size() > 1 && t->Node(1).Type() == kDataInt ? t->Int(1) : -1;
    }

    void ConfigChecks(const char *dumpPath) {
        printf("\n--- config/band_preinit_keep.dta as PreInitSystem reads it ---\n");
        DataInit();
        std::string boot = RetailBootMacros::TableNames();
        BeginDataRead();
        DataArray *cfg = DataReadFile("config/band_preinit_keep.dta", true);
        FinishDataRead();
        char d[256];
        snprintf(d, sizeof(d), "%d top-level sections; macros before the read: %s",
                 cfg ? cfg->Size() : -1, boot.c_str());
        Gate("preinit-config", cfg != nullptr, d);
        if (!cfg)
            return;
        if (dumpPath) {
            bool ok = RetailBootMacros::DumpConfig(cfg, boot, dumpPath);
            Gate("config-dump", ok, dumpPath);
        }

        // HX_XBOX: the system joypad.dta's Xbox controllers block, merged under
        // the game's joypad.dta. The detect types are Joypad.h's enum.
        DataArray *joy = Sub(cfg, "joypad");
        DataArray *ctl = Sub(joy, "controllers");
        int gType = DetectType(ctl, "strat_xbox_rb2");
        int dType = DetectType(ctl, "hx_drums_xbox");
        snprintf(d, sizeof(d), "%d controller entries; strat_xbox_rb2 type %d (want %d), "
                 "hx_drums_xbox type %d (want %d)", ctl ? ctl->Size() - 1 : -1, gType,
                 kJoypadXboxHxGuitarRb2, dType, kJoypadXboxDrums);
        Gate("joypad-controllers", ctl && gType == kJoypadXboxHxGuitarRb2
                 && dType == kJoypadXboxDrums, d);

        // The game's empty (ignore) wins the #merge over the system's
        // (ignore 1 2 3 4 5 6 7).
        DataArray *ign = Sub(joy, "ignore");
        snprintf(d, sizeof(d), "(ignore) has %d entr%s", ign ? ign->Size() - 1 : -1,
                 ign && ign->Size() == 2 ? "y" : "ies");
        Gate("joypad-ignore-empty", ign && ign->Size() == 1, d);

        // _SHIP: three dev-only blocks a console never reads.
        bool breed = Sub(joy, "breed_data_string_mappings") != nullptr;
        bool hosts = Sub(cfg, "hostnames") != nullptr;
        bool cheat = Sub(Sub(cfg, "ui"), "cheat_init") != nullptr;
        snprintf(d, sizeof(d), "joypad/breed_data_string_mappings %s, hostnames %s, "
                 "ui/cheat_init %s", breed ? "PRESENT" : "absent",
                 hosts ? "PRESENT" : "absent", cheat ? "PRESENT" : "absent");
        Gate("ship-dev-blocks-absent", !breed && !hosts && !cheat, d);
    }
}

int main(int argc, char **argv) {
    const char *dataDir = nullptr;
    const char *refPath = nullptr;
    const char *arkPath = "songs/gen/songs.dtb";
    const char *dumpPath = nullptr;
    const char *configDumpPath = nullptr;
    bool corrupt = false;

    std::vector<const char *> pos;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--corrupt") == 0) {
            corrupt = true;
        } else if (strcmp(argv[i], "--dump") == 0 && i + 1 < argc) {
            dumpPath = argv[++i];
        } else if (strcmp(argv[i], "--config-dump") == 0 && i + 1 < argc) {
            configDumpPath = argv[++i];
        } else {
            pos.push_back(argv[i]);
        }
    }
    if (pos.size() < 2) {
        fprintf(stderr,
                "usage: %s <dataDir> <referenceFile> [arkPath] "
                "[--corrupt] [--dump <path>] [--config-dump <path>]\n", argv[0]);
        return 1;
    }
    dataDir = pos[0];
    refPath = pos[1];
    if (pos.size() >= 3) arkPath = pos[2];

    // ---- engine bring-up -------------------------------------------------
    InitMakeString();
    Symbol::Init(); // mandatory before any Symbol is interned
    gFailures += RetailBootMacros::Define(); // retail's boot DTA macros, before any read (W16-UA)

    NativeSetDataDir(dataDir);

    // ★ MUST precede any NewFile() call. os/File.cpp:646 gates ArkFile creation
    // on UsingCD() — without this, NewFile silently falls through to AsyncFile
    // and tries to open the archive-relative path on the host filesystem, which
    // "works" (fails to open) in a way that looks like a missing file rather
    // than a misconfiguration.
    SetUsingCD(true);

    printf("=== rb3-ark: mounting real RB3 archive ===\n");
    printf("dataDir  : %s\n", dataDir);
    printf("arkPath  : %s\n", arkPath);
    printf("reference: %s\n", refPath);

    NativeArchiveInit();
    if (!TheArchive) {
        fprintf(stderr, "FATAL: TheArchive is null after NativeArchiveInit()\n");
        return 1;
    }

    // Retail reads the preinit config before any other data (PreInitSystem),
    // so this does too: the macro table then holds the boot macros and nothing
    // a later file #defines (songs.dta defines three COMMON_* macros).
    ConfigChecks(configDumpPath);

    // ---- where does the engine think this file lives? --------------------
    // Print the resolved (ark part, offset-within-part) so a digest mismatch is
    // debuggable rather than just "wrong".
    int arkNum = 0, fileSize = 0, ucSize = 0;
    unsigned long long byteOff = 0;
    bool found = TheArchive->GetFileInfo(
        FileMakePath(".", arkPath), arkNum, byteOff, fileSize, ucSize
    );
    printf("\nArchive::GetFileInfo(%s) -> %s\n", arkPath, found ? "found" : "NOT FOUND");
    if (found) {
        printf("  ark part      : %d (%s)\n", arkNum, TheArchive->GetArkfileName(arkNum));
        printf("  offset in part: %llu (0x%llx)\n", byteOff, byteOff);
        printf("  size          : %d\n", fileSize);
        printf("  uncompressed  : %d %s\n", ucSize,
               ucSize == 0 ? "(0 = stored uncompressed)" : "(COMPRESSED!)");
    }
    printf("\n--- gates ---\n");
    Gate("archive-lookup", found, found ? "" : "file not present in archive index");
    if (!found) {
        printf("\nRESULT: FAILED (%d gate(s))\n", gFailures);
        return 1;
    }

    // ---- read the bytes THROUGH the engine's ArkFile ---------------------
    // mode 2 == kRead. FileIsLocal() is false for a relative path, so with
    // UsingCD() true NewFile constructs an ArkFile, whose Read() calls
    // NativeArkRead().
    File *f = NewFile(arkPath, 2);
    if (!f) {
        Gate("open-through-ArkFile", false, "NewFile returned null");
        printf("\nRESULT: FAILED (%d gate(s))\n", gFailures);
        return 1;
    }
    Gate("open-through-ArkFile", true, "");

    int size = f->Size();
    std::vector<unsigned char> arkBytes((size_t)size);
    int total = 0;
    while (total < size) {
        int got = f->Read(arkBytes.data() + total, size - total);
        if (got <= 0) break;
        total += got;
    }
    bool fullRead = (total == size) && !f->Fail();
    {
        char d[128];
        snprintf(d, sizeof(d), "read %d of %d bytes%s", total, size,
                 f->Fail() ? ", ArkFile::Fail() set" : "");
        Gate("full-read", fullRead, d);
    }
    delete f;

    if (corrupt && !arkBytes.empty()) {
        printf("  [note] --corrupt: flipping one bit of byte 0 (negative control)\n");
        arkBytes[0] ^= 0x01;
    }

    if (dumpPath) {
        FILE *out = fopen(dumpPath, "wb");
        if (out) {
            fwrite(arkBytes.data(), 1, arkBytes.size(), out);
            fclose(out);
            printf("  [note] wrote ark-read bytes to %s for external verification\n",
                   dumpPath);
        } else {
            printf("  [note] could not write dump to %s\n", dumpPath);
        }
    }

    // ---- the unfakeable comparison ---------------------------------------
    std::vector<unsigned char> refBytes;
    bool refOk = ReadWholeFilePosix(refPath, refBytes);
    Gate("reference-readable", refOk, refPath);

    char arkHex[80] = {0}, refHex[80] = {0};
    Sha256Hex(arkBytes.data(), arkBytes.size(), arkHex);
    if (refOk) Sha256Hex(refBytes.data(), refBytes.size(), refHex);

    printf("\n  sha256(ark read via ArkFile) = %s  (%zu bytes)\n", arkHex,
           arkBytes.size());
    printf("  sha256(reference on disk)    = %s  (%zu bytes)\n",
           refOk ? refHex : "<unreadable>", refBytes.size());

    bool sizeOk = refOk && (arkBytes.size() == refBytes.size());
    Gate("size-match", sizeOk, "");
    bool hashOk = refOk && strcmp(arkHex, refHex) == 0;
    Gate("sha256-match", hashOk, "");
    bool memOk = sizeOk
        && memcmp(arkBytes.data(), refBytes.data(), arkBytes.size()) == 0;
    Gate("byte-for-byte memcmp", memOk, "");

    // ---- semantic half: parse it through the engine ----------------------
    // Ask for the .dta the way game code does; CachedDataFile() rewrites it to
    // <path>/gen/<base>.dtb when UsingCD() && !FileIsLocal, so this also
    // exercises the archive's dta->dtb redirection.
    printf("\n--- parsing songs through the archive ---\n");
    DataArray *root = DataReadFile("songs/songs.dta", true);
    int songCount = 0;
    if (!root) {
        Gate("DataReadFile(songs/songs.dta)", false, "returned null");
    } else {
        Gate("DataReadFile(songs/songs.dta)", true, "");
        songCount = root->Size();
        printf("  top-level nodes: %d\n", songCount);
        int shown = 0;
        for (int i = 0; i < root->Size(); i++) {
            if (root->Node(i).Type() != kDataArray) continue;
            DataArray *song = root->Array(i);
            if (!song || song->Size() < 1 || song->Node(0).Type() != kDataSymbol)
                continue;
            const char *id = song->Sym(0).Str();
            DataArray *nameArr = song->FindArray(Symbol("name"), false);
            DataArray *artistArr = song->FindArray(Symbol("artist"), false);
            printf("  [%3d] %-24s \"%s\" by %s\n", i, id,
                   nameArr ? nameArr->Str(1) : "(none)",
                   artistArr ? artistArr->Str(1) : "(none)");
            shown++;
        }
        printf("  printed %d song entries\n", shown);
    }
    // 130 under retail's boot macros. songs.dtb holds 138 top-level songs, but
    // eight (coldasice_nobass and seven test charts: _budget_test,
    // _invalid_version_test, runs_16s, sustains, _bre_test, framerate,
    // vocaltrainertest) sit inside `#ifndef _SHIP`, which a console never
    // reads. The 138 this gate used to expect was measured before the native
    // drivers defined _SHIP (W16-UA); tools/retail_boot_config.py gives 130.
    Gate("song-count == 130", songCount == 130, "");


    printf("\nRESULT: %s", gFailures == 0 ? "ALL GATES PASSED\n" : "FAILED\n");
    if (gFailures) printf("  %d gate(s) failed\n", gFailures);
    return gFailures == 0 ? 0 : 1;
}
