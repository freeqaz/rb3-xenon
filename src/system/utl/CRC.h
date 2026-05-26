#pragma once
#include "os/Debug.h"

namespace Hmx {
    class CRC {
    public:
        CRC() : mCRC(0) {}
        CRC(const char *cstr) : mCRC(ComputeHash(cstr, strlen(cstr))) {
            MILO_ASSERT(ValidateCRC(mCRC, cstr), 0x20);
        }

        bool operator<(const CRC &c) const { return mCRC < c.mCRC; }
        void Reset() { mCRC = 0; }
        static bool ValidateCRC(int, const char *); // just returns true
        static int ComputeHash(const char *, unsigned int);

        int mCRC; // 0x0
    };
}

#include "utl/BinStream.h"
inline BinStream &operator<<(BinStream &bs, const Hmx::CRC &crc) {
    bs << crc.mCRC;
    return bs;
}

inline BinStream &operator>>(BinStream &bs, Hmx::CRC &crc) {
    int hash = 0;
    bs >> hash;
    crc.mCRC = hash;
    return bs;
}
