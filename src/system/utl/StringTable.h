#pragma once
#include "utl/MemMgr.h"
#include <vector>

class StringTable {
private:
    struct Buf {
        int size;
        char *chars;
    };

    std::vector<Buf> mBuffers; // 0x0
    char *mCurChar; // 0xc
    int mCurBuf; // 0x10

    void AddBuf(int);

public:
    MEM_OVERLOAD(StringTable, 0x11);

    StringTable(int);
    ~StringTable();
    const char *Add(const char *);
    void Reserve(int);
    int Size() const;
    int UsedSize() const;
};
