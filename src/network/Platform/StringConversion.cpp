#include "StringConversion.h"
#include "types.h"
#include <string.h>

namespace {
    void Latin1ToUtf8(const char *in, char *out, unsigned int len) {
        const unsigned char *src = (const unsigned char *)in;
        unsigned char *dst = (unsigned char *)out;
        len--;
        int c = *src;
        while (c != 0 && len > 0) {
            if (c >= 0x80) {
                *dst = ((c & 0x7C0) >> 6) | 0xC0;
                dst++;
                len--;
                *dst = (c & 0x3F) | 0x80;
                dst++;
                len--;
            } else {
                *dst = c;
                dst++;
                len--;
            }
            src++;
            c = *src;
        }
        *dst = 0;
    }
    void Utf8ToLatin1(const char *in, char *out, unsigned int len) {
        u8 srch = *in;
        len--;
        bool hitNonLatin1Char = false;
        while (srch != 0 && len != 0 && !hitNonLatin1Char) {
            if (srch <= 0x7F) {
                *out = srch;
                out++;
                len--;
            } else {
                if ((unsigned int)(srch - 0xC0) <= 0x1F) {
                    u8 hi = *in;
                    len--;
                    u8 lo = *((const u8 *)++in);
                    *out = ((hi - 0xC0) << 6) + lo - 0x80;
                    out++;
                } else {
                    hitNonLatin1Char = true;
                }
            }
            srch = *((const u8 *)++in);
        }
        *out = 0;
    }
}

namespace Quazal {
    namespace StringConversion {
        void Char8_2T(const char *in, char *out, unsigned int len) {
            strcpy(out, in); // BUG: should be strncpy(out, in, len);
        }

        void T2Char8(const char *in, char *out, unsigned int len) {
            strcpy(out, in); // BUG: should be strncpy(out, in, len);
        }

        void Utf8ToT(const char *in, char *out, unsigned int len) {
            Utf8ToLatin1(in, out, len);
        }

        void TToUtf8(const char *in, char *out, unsigned int len) {
            Latin1ToUtf8(in, out, len);
        }

        int GetTToUtf8BufferSize(const char *str) { return strlen(str) * 2 + 1; }

    }
}
