#pragma once
#include "obj/Object.h"
#include "os/Timer.h"
#include "os/User.h"

// One player's end-of-game result, as XboxSession::WriteStats (retail
// 0x823EF9A8) hands it to WriteTrueSkillJob: stride 0x10.
class UserStat {
public:
    User *mUser; // 0x0
    int mTeam; // 0x4  X_PROPERTY_SESSION_TEAM
    int mScore; // 0x8  X_PROPERTY_RELATIVE_SCORE
    int mViewID; // 0xc  the stats view written to
};

class NetGameData {
public:
    NetGameData() {}
    virtual ~NetGameData() {}
    virtual int GetNumPlayersAllowed() const = 0;
    virtual void GetEndGameStats(std::vector<UserStat> &) const = 0;
    // Retail BandNetGameData vtable 0x820D27FC, slot 3 (+0xc): returns the
    // XUser property id 0x1000000E. XboxSession sets that property to the
    // session's public flag (UpdateSettings, 0x823EF7E8) and passes it to
    // MakeSessionJob as its publicPropertyId (0x823EEB58). Slot 2 (+0x8) is
    // GetEndGameStats: XboxSession::EndSession (0x823F08D0) calls it with a
    // vector<UserStat>&. Slot 4 (+0x10, PublicID) returns the title id.
    virtual int PublicPropertyID() const = 0;
    virtual int PublicID() const = 0;
    virtual void AuthenticationData(BinStream &, const User *) const = 0;
    virtual bool AuthenticateJoin(BinStream &, int &) const = 0;
};

class RemovingRemoteUserMsg;

// Retail clean TU5 0x82650B78..0x82651924 (BandNetGameData.cpp). Layout, from
// the ctor at 0x82650E48: NetGameData vfptr @0x0, Hmx::Object @0x4 (vtable
// 0x820D27A4), Timer @0x30.
class BandNetGameData : public NetGameData, public Hmx::Object {
public:
    BandNetGameData();
    virtual ~BandNetGameData();
    virtual int GetNumPlayersAllowed() const;
    virtual void GetEndGameStats(std::vector<UserStat> &) const;
    virtual int PublicPropertyID() const;
    virtual int PublicID() const;
    virtual void AuthenticationData(BinStream &, const User *) const;
    virtual bool AuthenticateJoin(BinStream &, int &) const;
    virtual DataNode Handle(DataArray *, bool);

    void Poll();

private:
    bool IsRanked() const;
    int StatsViewID() const;
    void DefaultRankedMatch(bool);
    DataNode OnMsg(const RemovingRemoteUserMsg &);

    // Runs while a refused invite is being reported: AuthenticateJoin raises
    // the invite-failed message only when it is idle, and Poll stops it after
    // 25 seconds.
    mutable Timer mInviteFailedTimer; // 0x30
};