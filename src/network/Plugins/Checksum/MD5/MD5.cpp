#include "network/Plugins/MD5.h"
#include <string.h>
#include "macros.h"

namespace Quazal {

    MD5::MD5() { init(); }

    void MD5::init() {
        finalized = false;
        count[0] = 0;
        count[1] = 0;
        state[0] = 0x67452301L;
        state[1] = 0xefcdab89L;
        state[2] = 0x98badcfeL;
        state[3] = 0x10325476L;
    }

    void MD5::update(const unsigned char *data, unsigned int length) {
        if (finalized) {
            return;
        }

        unsigned int nl = count[0] >> 3 & 0x3F;
        unsigned int l = count[0] + ((length) << 3);
        count[0] = l;

        if (l < (length << 3))
            count[1]++;

        count[1] += (length >> 29);
        unsigned int n = 0x40 - nl;

        unsigned int i;
        if (length >= n) {
            memcpy(buffer + nl, data, n);
            transform(buffer);
            for (i = n; i + 0x3F < length; i += 0x40) {
                transform(data + i);
            }
            nl = 0;
        } else {
            i = 0;
        }

        memcpy(buffer + nl, data + i, length - i);
    }

    // Retail emits encode/decode out of line just past this TU's .text
    // (fn_82B45480 / fn_82B45578, in auto_03), alongside the out-of-line II.
    void MD5::encode(unsigned char *output, const unsigned int *input, unsigned int len) {
        unsigned int i, j;
        for (i = 0, j = 0; j < len; i++, j += 4) {
            output[j] = (unsigned char)(input[i] & 0xff);
            output[j + 1] = (unsigned char)((input[i] >> 8) & 0xff);
            output[j + 2] = (unsigned char)((input[i] >> 16) & 0xff);
            output[j + 3] = (unsigned char)((input[i] >> 24) & 0xff);
        }
    }

    void MD5::finalize() {
        unsigned char bits[8];
        unsigned int index, padLen;
        static unsigned char PADDING[0x40] = { 0x80 };

        if (finalized) {
            return;
        }

        encode(bits, count, 8);

        index = count[0] >> 3 & 0x3F;
        padLen = index < 0x38 ? 0x38 - index : 0x78 - index;
        update(PADDING, padLen);
        update(bits, 8);

        encode(digest, state, 16);

        memset(buffer, 0, sizeof(*buffer));
        finalized = true;
    }

    // Abzug-style round helpers. Retail inlines 60 of the 64 round calls and
    // emits the last four II() calls out of line (fn_82B45630): MSVC's
    // forceinline budget runs out inside transform() even at /Od.
    static inline unsigned int rotate_left(unsigned int x, unsigned int n) {
        return (x << n) | (x >> (32 - n));
    }
    static inline unsigned int F(unsigned int x, unsigned int y, unsigned int z) {
        return (x & y) | (~x & z);
    }
    static inline unsigned int G(unsigned int x, unsigned int y, unsigned int z) {
        return (x & z) | (y & ~z);
    }
    static inline unsigned int H(unsigned int x, unsigned int y, unsigned int z) {
        return x ^ y ^ z;
    }
    static inline unsigned int I(unsigned int x, unsigned int y, unsigned int z) {
        return y ^ (x | ~z);
    }

    inline void MD5::FF(
        unsigned int &a,
        unsigned int b,
        unsigned int c,
        unsigned int d,
        unsigned int x,
        unsigned int s,
        unsigned int ac
    ) {
        a += F(b, c, d) + x + ac;
        a = rotate_left(a, s) + b;
    }
    inline void MD5::GG(
        unsigned int &a,
        unsigned int b,
        unsigned int c,
        unsigned int d,
        unsigned int x,
        unsigned int s,
        unsigned int ac
    ) {
        a += G(b, c, d) + x + ac;
        a = rotate_left(a, s) + b;
    }
    inline void MD5::HH(
        unsigned int &a,
        unsigned int b,
        unsigned int c,
        unsigned int d,
        unsigned int x,
        unsigned int s,
        unsigned int ac
    ) {
        a += H(b, c, d) + x + ac;
        a = rotate_left(a, s) + b;
    }
    inline void MD5::II(
        unsigned int &a,
        unsigned int b,
        unsigned int c,
        unsigned int d,
        unsigned int x,
        unsigned int s,
        unsigned int ac
    ) {
        a += I(b, c, d) + x + ac;
        a = rotate_left(a, s) + b;
    }

    void MD5::transform(const unsigned char *data) {
        unsigned int a = state[0], b = state[1], c = state[2], d = state[3], x[16];

        decode(x, data, sizeof(x));


        // Round 0
        FF(a, b, c, d, x[0], 7, 0xd76aa478);
        FF(d, a, b, c, x[1], 12, 0xe8c7b756);
        FF(c, d, a, b, x[2], 17, 0x242070db);
        FF(b, c, d, a, x[3], 22, 0xc1bdceee);
        FF(a, b, c, d, x[4], 7, 0xf57c0faf);
        FF(d, a, b, c, x[5], 12, 0x4787c62a);
        FF(c, d, a, b, x[6], 17, 0xa8304613);
        FF(b, c, d, a, x[7], 22, 0xfd469501);
        FF(a, b, c, d, x[8], 7, 0x698098d8);
        FF(d, a, b, c, x[9], 12, 0x8b44f7af);
        FF(c, d, a, b, x[10], 17, 0xffff5bb1);
        FF(b, c, d, a, x[11], 22, 0x895cd7be);
        FF(a, b, c, d, x[12], 7, 0x6b901122);
        FF(d, a, b, c, x[13], 12, 0xfd987193);
        FF(c, d, a, b, x[14], 17, 0xa679438e);
        FF(b, c, d, a, x[15], 22, 0x49b40821);

        // Round 1
        GG(a, b, c, d, x[1], 5, 0xf61e2562);
        GG(d, a, b, c, x[6], 9, 0xc040b340);
        GG(c, d, a, b, x[11], 14, 0x265e5a51);
        GG(b, c, d, a, x[0], 20, 0xe9b6c7aa);
        GG(a, b, c, d, x[5], 5, 0xd62f105d);
        GG(d, a, b, c, x[10], 9, 0x02441453);
        GG(c, d, a, b, x[15], 14, 0xd8a1e681);
        GG(b, c, d, a, x[4], 20, 0xe7d3fbc8);
        GG(a, b, c, d, x[9], 5, 0x21e1cde6);
        GG(d, a, b, c, x[14], 9, 0xc33707d6);
        GG(c, d, a, b, x[3], 14, 0xf4d50d87);
        GG(b, c, d, a, x[8], 20, 0x455a14ed);
        GG(a, b, c, d, x[13], 5, 0xa9e3e905);
        GG(d, a, b, c, x[2], 9, 0xfcefa3f8);
        GG(c, d, a, b, x[7], 14, 0x676f02d9);
        GG(b, c, d, a, x[12], 20, 0x8d2a4c8a);

        // Round 2
        HH(a, b, c, d, x[5], 4, 0xfffa3942);
        HH(d, a, b, c, x[8], 11, 0x8771f681);
        HH(c, d, a, b, x[11], 16, 0x6d9d6122);
        HH(b, c, d, a, x[14], 23, 0xfde5380c);
        HH(a, b, c, d, x[1], 4, 0xa4beea44);
        HH(d, a, b, c, x[4], 11, 0x4bdecfa9);
        HH(c, d, a, b, x[7], 16, 0xf6bb4b60);
        HH(b, c, d, a, x[10], 23, 0xbebfbc70);
        HH(a, b, c, d, x[13], 4, 0x289b7ec6);
        HH(d, a, b, c, x[0], 11, 0xeaa127fa);
        HH(c, d, a, b, x[3], 16, 0xd4ef3085);
        HH(b, c, d, a, x[6], 23, 0x04881d05);
        HH(a, b, c, d, x[9], 4, 0xd9d4d039);
        HH(d, a, b, c, x[12], 11, 0xe6db99e5);
        HH(c, d, a, b, x[15], 16, 0x1fa27cf8);
        HH(b, c, d, a, x[2], 23, 0xc4ac5665);

        // Round 3
        II(a, b, c, d, x[0], 6, 0xf4292244);
        II(d, a, b, c, x[7], 10, 0x432aff97);
        II(c, d, a, b, x[14], 15, 0xab9423a7);
        II(b, c, d, a, x[5], 21, 0xfc93a039);
        II(a, b, c, d, x[12], 6, 0x655b59c3);
        II(d, a, b, c, x[3], 10, 0x8f0ccc92);
        II(c, d, a, b, x[10], 15, 0xffeff47d);
        II(b, c, d, a, x[1], 21, 0x85845dd1);
        II(a, b, c, d, x[8], 6, 0x6fa87e4f);
        II(d, a, b, c, x[15], 10, 0xfe2ce6e0);
        II(c, d, a, b, x[6], 15, 0xa3014314);
        II(b, c, d, a, x[13], 21, 0x4e0811a1);
        II(a, b, c, d, x[4], 6, 0xf7537e82);
        II(d, a, b, c, x[11], 10, 0xbd3af235);
        II(c, d, a, b, x[2], 15, 0x2ad7d2bb);
        II(b, c, d, a, x[9], 21, 0xeb86d391);

        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;

        memset(x, 0, sizeof(x));
    }

    void MD5::decode(unsigned int *output, const unsigned char *input, unsigned int len) {
        unsigned int i, j;
        for (i = 0, j = 0; j < len; i++, j += 4) {
            output[i] = ((unsigned int)input[j]) | (((unsigned int)input[j + 1]) << 8)
                | (((unsigned int)input[j + 2]) << 16) | (((unsigned int)input[j + 3]) << 24);
        }
    }
}

