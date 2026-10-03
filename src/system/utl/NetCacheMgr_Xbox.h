#pragma once
#include "utl/NetCacheMgr.h"
#include "net/XLSPConnection.h"

class NetCacheMgrXbox : public NetCacheMgr {
public:
    NetCacheMgrXbox();
    // No Handle override: retail NetCacheMgrXbox's slot 6 is NetCacheMgr::Handle
    // itself (0x827CE8A8, shared by both tables). The forwarding wrapper that
    // used to be here is a distinct body ICF cannot fold into its target
    // (lane W16-OR; DC3 removed the same wrapper).
    virtual void Poll();

    unsigned int GetIP();

protected:
    virtual void LoadInit();
    virtual bool IsDoneLoading() const { return mDoneLoading; }
    virtual void UnloadInit();
    virtual bool IsDoneUnloading() const;

    bool mDoneLoading; // 0x64
    XLSPConnection mConnection; // 0x68
};
