#pragma once
#include "MatchmakingSettings.h"
// #include "NetSession.h"
#include "obj/Object.h"
#include "utl/BinStream.h"

class SessionData;

class NetSearchResult : public Hmx::Object {
public:
    NetSearchResult();
    virtual ~NetSearchResult();
    virtual DataNode Handle(DataArray *, bool);
    virtual void Load(BinStream &);
    virtual void Save(BinStream &) const;
    virtual bool Equals(const NetSearchResult *) const;

    static NetSearchResult *New();
    int NumOpenSlots() const { return mNumOpenSlots; }

    SessionData *mSessionData; // 0x28
    MatchmakingSettings *mSettings; // 0x2c
    int mNumOpenSlots; // 0x30
    String mHostName; // 0x34
};