#pragma once
#include "net/HttpGet.h"
#include "utl/NetLoader.h"

class NetLoaderXbox : public NetLoader {
public:
    NetLoaderXbox(const String &);
    virtual ~NetLoaderXbox();
    virtual void PollLoading();
    virtual bool HasFailed();
    virtual bool IsSafeToDelete() const { return true; }

private:
    HttpGet *mHttpGet; // 0x20
    bool unk24; // 0x24
};
