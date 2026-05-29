#pragma once
#include "meta/MemcardAction.h"
#include "meta/Profile.h"
#include "obj/Data.h"
#include "obj/Msg.h"
#include "obj/Object.h"
#include "os/Memcard.h"
#include "os/PlatformMgr.h"
#include "os/ThreadCall.h"
#include "ui/UI.h"

// Retail RB3-360 layout (verified vs ctor fn_82787030 + OnMsg/Init/ThreadCall
// targets in build/45410914/asm/MemcardMgr_Xbox.s):
//   MemcardMgr : ThreadCallback, MsgSource
//   - ThreadCallback vfptr @ 0x0 (primary base; ThreadCall(this) is passed
//     this+0, e.g. fn_82786DD8 `mr r3,r31; bl ThreadCall`).
//   - MsgSource subobject @ 0x4 {vbptr@4, mSinks@8, mEventSinks@0x10,
//     mExporting@0x18} — its ctor fn_827432A0 runs on this+0x4.
//   - MemcardMgr's own members start @ 0x1c (mState).
//   - Hmx::Object virtual base trails @ 0x8c (ctor: addi r3,r3,0x8c; bl
//     Object::Object).
class MemcardMgr : public ThreadCallback, public MsgSource {
    friend class SaveMemcardAction; // hack
    friend class LoadMemcardAction; // hack

public:
    enum State {
        kS_None = 0,
        kS_Search = 1,
        kS_CheckForSaveContainer = 2,
        kS_SaveGame = 3,
        kS_LoadGame = 4,
        kS_DeleteSaves = 5
    };
    MemcardMgr();
    // Hmx::Object
    virtual ~MemcardMgr();
    virtual DataNode Handle(DataArray *, bool);
    // ThreadCallback
    virtual int ThreadStart();
    virtual void ThreadDone(int);

    void SetProfileSaveBuffer(void *, int);
    void SaveLoadProfileComplete(Profile *, int);
    void SaveLoadAllComplete();

    void Init();
    bool IsStorageDeviceValid(Profile *);
    void OnCheckForSaveContainer(Profile *);
    void OnDeleteSaves(Profile *);
    void OnSaveGame(Profile *, MemcardAction *, int);
    void OnLoadGame(Profile *, MemcardAction *);
    void OnSearchForDevice(Profile *);
    void SetDevice(unsigned int);
    void SelectDevice(Profile *, Hmx::Object *, int, bool);
    int GetSizeNeeded();

private:
    MCResult ThreadCall_SearchForDevice();
    MCResult ThreadCall_CheckForSaveContainer();
    MCResult ThreadCall_SaveGame();
    MCResult ThreadCall_LoadGame();
    MCResult PerformRead(MCContainer *);
    MCResult PerformWrite(MCContainer *);

protected:
    DataNode OnMsg(const DeviceChosenMsg &);
    DataNode OnMsg(const NoDeviceChosenMsg &);
    DataNode OnMsg(const UIChangedMsg &);
    DataNode OnMsg(const StorageChangedMsg &);
    DataNode OnMsg(const SigninChangedMsg &);

    State mState; // 0x1c
    void *mSaveDataBuffer; // 0x20
    int mSaveDataLength; // 0x24
    MemcardAction *mAction; // 0x28
    int mSaveCreateType; // 0x2c
    // indexed by padnums
    ContainerId mContainerIDs[4]; // 0x30 (ContainerId is 0xc bytes)
    MCContainer *mContainers[4]; // 0x60
    bool mValidDevices[4]; // 0x70
    int mPendingDeviceSelectorIndex; // 0x74
    bool mSelectDeviceWaiting; // 0x78 (ctor also zeroes 0x79 padding byte)
    Hmx::Object *mSelectDeviceCallBackObj; // 0x7c
    int mPadNum; // 0x80
    Profile *mProfile; // 0x84
    // Hmx::Object virtual base @ 0x8c
};

extern MemcardMgr TheMemcardMgr;

DECLARE_MESSAGE(SaveLoadAllCompleteMsg, "save_load_all_complete_msg")
SaveLoadAllCompleteMsg() : Message(Type()) {}
END_MESSAGE

// MCResultMsg is defined in os/Memcard.h (included transitively)
