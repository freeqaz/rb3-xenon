#pragma once
#include "game/BandUser.h"
#include "meta/MemcardAction.h"
#include "meta/Profile.h"
#include "net_band/RockCentralMsgs.h"
#include "obj/Msg.h"
#include "os/Memcard.h"
#include "os/Timer.h"
#include "utl/Cache.h"
#include "utl/Str.h"
#include <vector>

class BandProfile;

enum SaveLoadMgrStatus {
    kSaveLoadMgrStatus0,
    kSaveLoadMgrStatus_Saving,
    kSaveLoadMgrStatus_Loading,
    kSaveLoadMgrStatus_Start,
    kSaveLoadMgrStatus_Finish,
};

class SaveLoadManager : public MsgSource {
public:
    enum SaveLoadMode {
        kMode_AutoLoad,
        kMode_AutoSave,
        kMode_DisableAutoSave,
        kMode_ManualDelete,
        kMode_ManualLoad,
    };
    enum State {
        kS_Idle = 0x0,
        kS_Start = 0x1,
        kS_AutoloadSelectProfile = 0x3,
        kS_AutoloadNoSaveFound_Msg = 0x6,
        kS_AutoloadMultipleSavesFound = 0x7,
        kS_AutoloadSetDevice = 0x8,
        kS_AutoloadSelectDevice2 = 0x9,
        kS_AutoloadSelectDevice3 = 0xA,
        kS_AutoloadStartLoad = 0xB,
        kS_AutoloadNotOwner = 0xC,
        kS_AutoloadStartLoad2 = 0xD,
        kS_AutoloadCorrupt = 0xE,
        kS_AutoloadObsolete = 0xF,
        kS_AutoloadFuture = 0x10,
        kS_AutoloadFuture2 = 0x11,
        kS_AutoloadDeviceMissing = 0x12,
        kS_SongCacheCreateMountRead = 0x1A,
        kS_SongCacheCreateSearch = 0x1F,
        kS_SongCacheCreateMountWrite = 0x20,
        kS_SongCacheCreateNotFound_Msg = 0x21,
        kS_SongCacheCreateMissing_Msg = 0x32,
        kS_SongCacheCreateCorrupt = 0x34,
        kS_SongCacheCreateMountRead2 = 0x3E,
        kS_SaveChooseDeviceInvalid = 0x45,
        kS_SaveOverwrite = 0x46,
        kS_SaveNoOverwrite = 0x47,
        kS_SaveDeviceInvalid = 0x48,
        kS_SaveNotEnoughSpacePS3 = 0x4A,
        kS_SaveChooseDevice = 0x4B,
        kS_GlobalCreateNotFound_Msg = 0x4C,
        kS_GlobalCreateMissing_Msg = 0x4D,
        kS_GlobalCreateCorrupt = 0x4E,
        kS_SaveFailed = 0x50,
        kS_SaveCheckProfile = 0x54,
        kS_SaveCheckAutosave = 0x55,
        kS_ManualLoadInit = 0x5A,
        kS_ManualSaveNoDevice = 0x5B,
        kS_ManualLoadNoDevice = 0x5C,
        kS_ManualLoadStartLoad = 0x5D,
        kS_ManualLoadConfirmUnsaved = 0x5E,
        kS_ManualLoadConfirm_Yes = 0x5F,
        kS_ManualLoadConfirm = 0x60,
        kS_ManualSaveChooseDevice = 0x61,
        kS_GlobalOptionsMissing_Msg = 0x62,
        kS_ManualLoadChooseDevice = 0x64,
        kS_Done = 0x6D,
        kS_LoadComplete = 0x6E,
        kS_Finish = 0x6F,
    };

    SaveLoadManager();
    virtual ~SaveLoadManager();
    void AutoSave();
    void AutoLoad();
    void EnableAutosave(LocalBandUser *);
    void DisableAutosave(LocalBandUser *);
    void ManualSave(LocalBandUser *);
    void ManualDelete();
    bool IsAutosaveEnabled(LocalBandUser *);
    DataNode GetDialogMsg();
    Symbol GetDialogOpt1();
    Symbol GetDialogOpt2();
    Symbol GetDialogOpt3();
    int GetDialogFocusOption();
    bool IsInitialLoadDone() const;
    bool IsIdle();
    void Activate();
    void PrintoutSaveSizeInfo();
    void Poll();
    void AutoSaveNow();
    virtual DataNode Handle(DataArray *, bool);
    void HandleEventResponseStart(int i1);
    void HandleEventResponse(LocalUser *, int i1);

    static void Init();

    DataNode OnMsg(const ProfileSwappedMsg &);
    DataNode OnMsg(const DeviceChosenMsg &);
    DataNode OnMsg(const NoDeviceChosenMsg &);
    DataNode OnMsg(const MCResultMsg &);
    DataNode OnMsg(const RockCentralOpCompleteMsg &);
    DataNode OnMsg(const SigninChangedMsg &);

protected:
    BandProfile *GetProfile();
    BandProfile *GetAutosavableProfile();
    BandProfile *GetNewSigninProfile();
    void SetState(State);
    void UpdateStatus(SaveLoadMgrStatus);
    void Start();
    void Finish();
    void SaveLoadErrorSetState();
    void StartSaveAction(bool);
    bool IsReasonToAutosave(bool);
    bool IsReasonToAutoload();
    bool IsReasonToUpload();

    bool mActivated; // 0x1c
    bool mInitialLoadNotDone; // 0x1d
    int mMode; // 0x20
    State mState; // 0x24
    State mStateAtSelectStart; // 0x28
    LocalBandUser *mUser; // 0x2c
    LocalUser *mLocalUser; // 0x30
    std::vector<BandProfile *> mUploadProfiles; // 0x34
    std::vector<BandProfile *> mSaveProfiles; // 0x3c
    DataArrayPtr unk44; // 0x44
    int unk48; // 0x48
    String unk4c; // 0x4c
    int mSaveSize; // 0x58
    CacheID *mCacheID; // 0x5c
    Cache *mCache; // 0x60
    void *mData; // 0x64
    bool unk68; // 0x68
    bool mWaiting; // 0x69
    int unk6c; // 0x6c
    int unk70; // 0x70
    int mRequestFlags; // 0x74
    int unk78; // 0x78
    int unk7c; // 0x7c
    MemcardAction *mAction; // 0x80
    int unk84; // 0x84
    Timer mTimer; // 0x88
};

extern SaveLoadManager *TheSaveLoadMgr;

DECLARE_MESSAGE(SaveLoadMgrStatusUpdateMsg, "saveloadmgr_status_update_msg")
SaveLoadMgrStatusUpdateMsg(int status) : Message(Type(), status) {}
int Status() const { return mData->Int(2); }
END_MESSAGE
