#include "CRC.h"

bool Hmx::CRC::ValidateCRC(int, const char *) { return true; }

int Hmx::CRC::ComputeHash(const char *c, unsigned int size) {
    if (size != 0 && c != 0 && *c != '\0') {
        // FNV-1a style hash (a DC3-era file: band.exe holds no 0x811C9DC5 constant, so
        // RB3 retail has no body to compare). The multiplies wrap; on int they were
        // signed overflow (UBSan, lane asanwalk). They are done unsigned now, while the
        // shifts stay on int so they remain arithmetic: the hash bits are unchanged.
        int number = 0x811c9dc5;
        for (int i = 0; i < size; i++) {
            number = (int)((unsigned int)(c[i] ^ number) * 0x1000193u);
        }
        int m = (int)((unsigned int)number * 0x2001u);
        number = (int)((unsigned int)(m >> 7 ^ m) * 9u);
        return (int)((unsigned int)(number >> 0x11 ^ number) * 0x21u);
    }
    return 0;
}