#pragma once
#include "BandMachineMgr.h"
#include "game/NetGameMsgs.h"
#include "obj/Object.h"
#include "utl/Str.h"
#include <set>

class BandMachine : public Hmx::Object {
public:
    BandMachine();
    // virtual ~BandMachine() {}  // retail uses implicit dtor (elides vptr store)
    virtual DataNode Handle(DataArray *, bool);
    virtual bool IsLocal() const = 0;
    virtual String GetPrimaryBandName();

    String GetPrimaryProfileName();
    void Reset();
    void SyncSave(BinStream &, unsigned char) const;
    void SyncLoad(BinStream &, unsigned char);
    NetUIState GetNetUIState() const;
    int GetNetUIStateParam() const;
    bool HasSong(int) const;
    bool HasProGuitarOrBass(int) const { return false; }

    NetUIState mNetUIState; // 0x28
    int mNetUIStateParam; // 0x2c
    std::set<int> mAvailableSongs; // 0x30
    // TU5-inserted: parallel set to mAvailableSongs, backs
    // SetProGuitarOrBassSongs()/HasProGuitarOrBass(). std::set<int> == 0x18.
    std::set<int> mProGuitarOrBassSongs; // 0x48
    String mCurrentSongPreview; // 0x60
    String mPrimaryBandName; // 0x6c
    String mPrimaryProfileName; // 0x78
    int mPrimaryMetaScore; // 0x84
};

class LocalBandMachine : public BandMachine {
public:
    LocalBandMachine(BandMachineMgr *);
    // virtual ~LocalBandMachine() {}  // retail uses implicit dtor
    virtual bool IsLocal() const { return true; }
    virtual String GetPrimaryBandName();

    void SetNetUIState(NetUIState);
    void SetNetUIStateParam(int);
    void SetPrimaryBandName(String);
    void SetPrimaryProfileName(String);
    void SetPrimaryMetaScore(int);
    void SetAvailableSongs(const std::set<int> &);
    void SetProGuitarOrBassSongs(const std::set<int> &);
    void SetCurrentSongPreview(const char *);

    BandMachineMgr *mMachineMgr; // 0x88
};

class RemoteBandMachine : public BandMachine {
public:
    RemoteBandMachine();
    // virtual ~RemoteBandMachine() {}  // retail uses implicit dtor
    virtual bool IsLocal() const { return false; }

    void Activate(unsigned int);
    void Deactivate();
    bool IsActive() const;
    unsigned int GetMachineID() const;

    unsigned int mID; // 0x88
    bool mActive; // 0x8c
};

#include "obj/Msg.h"

class LocalMachineUpdatedMsg : public Message {
public:
    LocalMachineUpdatedMsg(LocalBandMachine *machine, unsigned char mask)
        : Message(Type(), machine, mask) {}
    LocalMachineUpdatedMsg(DataArray *da) : Message(da) {}
    virtual ~LocalMachineUpdatedMsg() {}
    static Symbol Type() {
        static Symbol t("local_machine_updated");
        return t;
    }
};

class RemoteMachineUpdatedMsg : public Message {
public:
    RemoteMachineUpdatedMsg(RemoteBandMachine *machine, unsigned char mask)
        : Message(Type(), machine, mask) {}
    RemoteMachineUpdatedMsg(DataArray *da) : Message(da) {}
    virtual ~RemoteMachineUpdatedMsg() {}
    static Symbol Type() {
        static Symbol t("remote_machine_updated");
        return t;
    }
    int GetMask() const { return mData->Int(3); }
};

class NewRemoteMachineMsg : public Message {
public:
    NewRemoteMachineMsg(RemoteBandMachine *machine) : Message(Type(), machine) {}
    NewRemoteMachineMsg(DataArray *da) : Message(da) {}
    virtual ~NewRemoteMachineMsg() {}
    static Symbol Type() {
        static Symbol t("new_remote_machine");
        return t;
    }
};

class RemoteMachineLeftMsg : public Message {
public:
    RemoteMachineLeftMsg(RemoteBandMachine *machine) : Message(Type(), machine) {}
    RemoteMachineLeftMsg(DataArray *da) : Message(da) {}
    virtual ~RemoteMachineLeftMsg() {}
    static Symbol Type() {
        static Symbol t("remote_machine_left");
        return t;
    }
    RemoteBandMachine *GetMachine() const { return mData->Obj<RemoteBandMachine>(2); }
};

bool IsWaitingNetUIState(NetUIState state);