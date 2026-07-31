#pragma once
#include "obj/Object.h"
#include "os/Timer.h"
#include "os/User.h"

class UserStat {
public:
};

class NetGameData {
public:
    NetGameData() {}
    virtual ~NetGameData() {}
    virtual int GetNumPlayersAllowed() const = 0;
    // NOTE: retail vtable has one extra slot here (AuthenticationData sits at
    // vtable+0x14, not +0x10 as it would with only the 5 methods below rb3-Wii's
    // BandNetGameData.h declares) -- confirmed via ??0AddUserRequestMsg's
    // AuthenticationData call site (target lwz off 0x14 vs our 0x10) while
    // GetNumPlayersAllowed's call site (NetSession::NumOpenSlots) confirms slot 1
    // (off 0x4) is unchanged. Exact name/semantics unverified -- rb3-Wii (no
    // Xbox Live) doesn't need it; guessed as a public/private XSession slot split
    // since XSESSION_CREATE_PARAMETERS needs both counts and GetNumPlayersAllowed
    // only exposes one. Placeholder pending a stronger oracle.
    virtual int GetNumPrivateSlotsAllowed() const = 0;
    virtual void GetEndGameStats(std::vector<UserStat> &) const = 0;
    virtual int PublicID() const = 0;
    virtual void AuthenticationData(BinStream &, const User *) const = 0;
    virtual bool AuthenticateJoin(BinStream &, int &) const = 0;
};

class BandNetGameData : public NetGameData, public Hmx::Object {
public:
    BandNetGameData();
    virtual ~BandNetGameData();
    virtual int GetNumPlayersAllowed() const;
    virtual int GetNumPrivateSlotsAllowed() const;
    virtual void GetEndGameStats(std::vector<UserStat> &) const;
    virtual int PublicID() const;
    virtual void AuthenticationData(BinStream &, const User *) const;
    virtual bool AuthenticateJoin(BinStream &, int &) const;
    virtual DataNode Handle(DataArray *, bool);

    void Poll();

    Timer unk20;
};