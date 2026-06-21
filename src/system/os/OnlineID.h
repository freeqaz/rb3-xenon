#pragma once
#include "types.h"
#include "utl/BinStream.h"
#include "utl/MemMgr.h"
#include "utl/Str.h"
#include "xdk/XAPILIB.h"

// size 0x10 (retail X360: XUID@0x0, mValid@0x8; no inline player-name String —
// verified against retail OnlineID::SetXUID @0x82511030 and the Leaderboard
// ctor @0x826561F0 where EntityID is 0x18 = mType(4)+mPlayerID(4)+OnlineID(0x10)).
class OnlineID {
private:
    friend BinStream &operator<<(BinStream &, const OnlineID &);

    XUID mXUID; // 0x0
    bool mValid; // 0x8
public:
    OnlineID();
    OnlineID(const OnlineID &);
    OnlineID(const XUID &);
    void Clear();
    void SetXUID(const XUID &);
    void SetPlayerName(const char *);
    XUID GetXUID() const;
    const char *ToString() const;
    bool GetIsValid() const { return mValid; }
    bool IsInvalid() const { return !mValid; }

    MEM_OVERLOAD(OnlineID, 0x1E)
};

// BinStream &operator>>(BinStream &, OnlineID &);
