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
// SHA-256 (FIPS 180-4). Self-contained so the demo adds no link dependency.
// ⚠ A hash function written here and then used to compare two buffers that are
// BOTH hashed by it would agree even if it were wrong — so the run also emits
// --dump output for external coreutils verification, and additionally does a
// direct memcmp. Three independent checks, one of which needs no trust at all.
// ---------------------------------------------------------------------------
namespace {

    struct Sha256 {
        uint32_t h[8];
        uint64_t len;
        unsigned char buf[64];
        size_t buflen;
    };

    inline uint32_t Ror(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

    const uint32_t kK[64] = {
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
        0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
        0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
        0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
        0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
        0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
        0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
        0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
        0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
        0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
        0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
    };

    void Sha256Init(Sha256 &s) {
        s.h[0] = 0x6a09e667u; s.h[1] = 0xbb67ae85u; s.h[2] = 0x3c6ef372u;
        s.h[3] = 0xa54ff53au; s.h[4] = 0x510e527fu; s.h[5] = 0x9b05688cu;
        s.h[6] = 0x1f83d9abu; s.h[7] = 0x5be0cd19u;
        s.len = 0;
        s.buflen = 0;
    }

    void Sha256Block(Sha256 &s, const unsigned char *p) {
        uint32_t w[64];
        for (int i = 0; i < 16; i++) {
            w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16)
                | ((uint32_t)p[i * 4 + 2] << 8) | (uint32_t)p[i * 4 + 3];
        }
        for (int i = 16; i < 64; i++) {
            uint32_t s0 = Ror(w[i - 15], 7) ^ Ror(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = Ror(w[i - 2], 17) ^ Ror(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = s.h[0], b = s.h[1], c = s.h[2], d = s.h[3];
        uint32_t e = s.h[4], f = s.h[5], g = s.h[6], hh = s.h[7];
        for (int i = 0; i < 64; i++) {
            uint32_t S1 = Ror(e, 6) ^ Ror(e, 11) ^ Ror(e, 25);
            uint32_t ch = (e & f) ^ ((~e) & g);
            uint32_t t1 = hh + S1 + ch + kK[i] + w[i];
            uint32_t S0 = Ror(a, 2) ^ Ror(a, 13) ^ Ror(a, 22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t t2 = S0 + maj;
            hh = g; g = f; f = e; e = d + t1;
            d = c; c = b; b = a; a = t1 + t2;
        }
        s.h[0] += a; s.h[1] += b; s.h[2] += c; s.h[3] += d;
        s.h[4] += e; s.h[5] += f; s.h[6] += g; s.h[7] += hh;
    }

    void Sha256Update(Sha256 &s, const void *data, size_t n) {
        const unsigned char *p = (const unsigned char *)data;
        s.len += n;
        while (n > 0) {
            size_t take = 64 - s.buflen;
            if (take > n) take = n;
            memcpy(s.buf + s.buflen, p, take);
            s.buflen += take;
            p += take;
            n -= take;
            if (s.buflen == 64) {
                Sha256Block(s, s.buf);
                s.buflen = 0;
            }
        }
    }

    void Sha256Final(Sha256 &s, char *outHex) {
        uint64_t bits = s.len * 8;
        unsigned char pad = 0x80;
        Sha256Update(s, &pad, 1);
        unsigned char zero = 0;
        while (s.buflen != 56) Sha256Update(s, &zero, 1);
        unsigned char lenb[8];
        for (int i = 0; i < 8; i++) lenb[i] = (unsigned char)(bits >> (56 - i * 8));
        Sha256Update(s, lenb, 8);
        for (int i = 0; i < 8; i++) {
            sprintf(outHex + i * 8, "%08x", s.h[i]);
        }
        outHex[64] = '\0';
    }

    void Sha256Hex(const void *data, size_t n, char *outHex) {
        Sha256 s;
        Sha256Init(s);
        Sha256Update(s, data, n);
        Sha256Final(s, outHex);
    }

    // Read a file with plain POSIX I/O — deliberately NOT through the engine, so
    // the reference side of the comparison shares no code with the ark side.
    bool ReadWholeFilePosix(const char *path, std::vector<unsigned char> &out) {
        int fd = open(path, O_RDONLY);
        if (fd < 0) return false;
        out.clear();
        unsigned char tmp[65536];
        for (;;) {
            ssize_t got = read(fd, tmp, sizeof(tmp));
            if (got < 0) { close(fd); return false; }
            if (got == 0) break;
            out.insert(out.end(), tmp, tmp + got);
        }
        close(fd);
        return true;
    }

    int gFailures = 0;

    void Gate(const char *name, bool ok, const char *detail) {
        printf("  [%s] %s%s%s\n", ok ? "PASS" : "FAIL", name,
               detail && *detail ? " — " : "", detail ? detail : "");
        if (!ok) gFailures++;
    }
}

int main(int argc, char **argv) {
    const char *dataDir = nullptr;
    const char *refPath = nullptr;
    const char *arkPath = "songs/gen/songs.dtb";
    const char *dumpPath = nullptr;
    bool corrupt = false;

    std::vector<const char *> pos;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--corrupt") == 0) {
            corrupt = true;
        } else if (strcmp(argv[i], "--dump") == 0 && i + 1 < argc) {
            dumpPath = argv[++i];
        } else {
            pos.push_back(argv[i]);
        }
    }
    if (pos.size() < 2) {
        fprintf(stderr,
                "usage: %s <dataDir> <referenceFile> [arkPath] "
                "[--corrupt] [--dump <path>]\n", argv[0]);
        return 1;
    }
    dataDir = pos[0];
    refPath = pos[1];
    if (pos.size() >= 3) arkPath = pos[2];

    // ---- engine bring-up -------------------------------------------------
    InitMakeString();
    Symbol::Init(); // mandatory before any Symbol is interned

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
    Gate("song-count == 138", songCount == 138, "");

    printf("\nRESULT: %s", gFailures == 0 ? "ALL GATES PASSED\n" : "FAILED\n");
    if (gFailures) printf("  %d gate(s) failed\n", gFailures);
    return gFailures == 0 ? 0 : 1;
}
