#pragma once
#include "types.h"
#include "utl/BinStream.h"
#include "utl/MemMgr.h"
#include "utl/Str.h"
#include "xdk/XAPILIB.h"

// size 0x10 by default; 0x18 under RB3_ONLINEID_PLAYERNAME (per-TU DC3 variant).
// (retail X360: XUID@0x0, mValid@0x8; no inline player-name String —
// verified against retail OnlineID::SetXUID @0x82511030 and the Leaderboard
// ctor @0x826561F0 where EntityID is 0x18 = mType(4)+mPlayerID(4)+OnlineID(0x10)).
class OnlineID {
private:
    friend BinStream &operator<<(BinStream &, const OnlineID &);
    friend BinStream &operator>>(BinStream &, OnlineID &);

    XUID mXUID; // 0x0
#ifdef RB3_ONLINEID_PLAYERNAME
    // Per-TU ODR skew: some retail TUs (e.g. BandProfile.cpp via ProfilePicture)
    // were compiled against the DC3-era OnlineID that carries an inline player-name
    // String, making sizeof 0x18 instead of 0x10. Others (Leaderboard/EntityID)
    // saw the 0x10 variant — hence this is gated per-TU, not global.
    String mPlayerName; // 0x8
#endif
    bool mValid; // 0x8 (0x10 with player-name variant)
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
    bool operator==(const OnlineID &) const;

    MEM_OVERLOAD(OnlineID, 0x1E)
};

BinStream &operator>>(BinStream &, OnlineID &);
