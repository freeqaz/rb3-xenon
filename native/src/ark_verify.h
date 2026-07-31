// rb3-xenon native — shared verification helpers for the archive drivers.
//
// Extracted from main_ark.cpp (lane CC-9 / M13) so main_midi.cpp can reuse the
// same SHA-256, the same POSIX reference reader and the same gate accounting
// instead of growing a second copy. Everything is `inline`, so including it
// from several TUs in one target is ODR-clean.
//
// ⚠ THE POINT OF THE DESIGN, worth restating where it will be read:
// hashing two buffers with the SAME hash function and finding they agree proves
// only that the function is deterministic. It cannot detect a wrong hash. So the
// drivers that use this header must ALSO (a) write the bytes out for an EXTERNAL
// checker (coreutils sha256sum / cmp) and (b) do a direct memcmp that needs no
// hash at all, and (c) ship a negative control that is actually executed and
// actually fails. This project has shipped a test whose pass criterion was a
// negative grep and which reported PASS while the process dumped core; a
// criterion that cannot fail is not a criterion.

#pragma once

#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <vector>

namespace arkverify {

    struct Sha256 {
        uint32_t h[8];
        uint64_t len;
        unsigned char buf[64];
        size_t buflen;
    };

    inline uint32_t Ror(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

    inline const uint32_t *K() {
        static const uint32_t kK[64] = {
            0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu,
            0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u,
            0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u,
            0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
            0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u,
            0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
            0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
            0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
            0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u,
            0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u, 0x1e376c08u,
            0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu,
            0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
            0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
        };
        return kK;
    }

    inline void Sha256Init(Sha256 &s) {
        s.h[0] = 0x6a09e667u; s.h[1] = 0xbb67ae85u; s.h[2] = 0x3c6ef372u;
        s.h[3] = 0xa54ff53au; s.h[4] = 0x510e527fu; s.h[5] = 0x9b05688cu;
        s.h[6] = 0x1f83d9abu; s.h[7] = 0x5be0cd19u;
        s.len = 0;
        s.buflen = 0;
    }

    inline void Sha256Block(Sha256 &s, const unsigned char *p) {
        const uint32_t *kK = K();
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

    inline void Sha256Update(Sha256 &s, const void *data, size_t n) {
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

    inline void Sha256Final(Sha256 &s, char *outHex) {
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

    inline void Sha256Hex(const void *data, size_t n, char *outHex) {
        Sha256 s;
        Sha256Init(s);
        Sha256Update(s, data, n);
        Sha256Final(s, outHex);
    }

    // Read a file with plain POSIX I/O — deliberately NOT through the engine, so
    // the reference side of the comparison shares no code with the ark side.
    inline bool ReadWholeFilePosix(const char *path, std::vector<unsigned char> &out) {
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

    // Gate accounting. Every gate prints its own verdict, and the failure count
    // is what the process exit code is derived from — so "no stderr" is never
    // mistaken for success.
    struct Gates {
        int failures = 0;
        int total = 0;

        void Check(const char *name, bool ok, const char *detail = "") {
            total++;
            printf("  [%s] %s%s%s\n", ok ? "PASS" : "FAIL", name,
                   detail && *detail ? " — " : "", detail ? detail : "");
            if (!ok) failures++;
        }

        int Finish() {
            printf("\nRESULT: %s\n",
                   failures == 0 ? "ALL GATES PASSED" : "FAILED");
            printf("  %d of %d gate(s) passed", total - failures, total);
            if (failures) printf("; %d FAILED", failures);
            printf("\n");
            return failures == 0 ? 0 : 1;
        }
    };

} // namespace arkverify
