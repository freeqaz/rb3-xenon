#pragma once
#include "utl/BinStream.h"
#include "utl/MemMgr.h"

class AppChild {
private:
    bool mEnabled;
    BinStream *mStream;
    bool mSync;

    AppChild(const char *);
    ~AppChild();

public:
    void Sync();
    void Sync(unsigned short);
    void Poll();
    void SetEnabled(bool b) { mEnabled = b; }

    static void Init();
    static void Terminate();

    MEM_OVERLOAD(AppChild, 0x16);
};

extern AppChild *TheAppChild;
