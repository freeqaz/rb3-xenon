#include "SaveLoadManager.h"
#include "game/BandUser.h"
#include "game/BandUserMgr.h"
#include "meta/FixedSizeSaveable.h"
#include "meta/FixedSizeSaveableStream.h"
#include "meta/MemcardMgr.h"
#include "meta/Profile.h"
#include "meta/WiiProfileMgr.h"
#include "meta_band/BandProfile.h"
#include "meta_band/BandSongMgr.h"
#include "meta_band/ProfileMgr.h"
#include "meta_band/UIEventMgr.h"
#include "utl/MakeString.h"
#include "net/Net.h"
#include "net/Server.h"
#include "net_band/EntityUploader.h"
#include "net_band/RockCentral.h"
#include "net_band/RockCentralMsgs.h"
#include "obj/Data.h"
#include "obj/MessageTimer.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "os/Memcard.h"
#include "os/PlatformMgr.h"
#include "os/User.h"
#include "utl/BufStream.h"
#include "utl/CacheMgr.h"
#include "utl/MemMgr.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"

// song_info_cache_* Symbols are used by the Wii song-info-cache dialog states
// (kS_SongCacheCreate*). Not present in the in-tree Symbols headers (likely
// because the retail 360 song-cache path diverges); declared here as externs so
// the TU compiles. If retail defines these globals, the refs resolve; if the
// path is absent on retail, those bodies simply won't match.
extern Symbol song_info_cache_name;
extern Symbol song_info_cache_button_create;
extern Symbol song_info_cache_button_corrupt_overwrite;
extern Symbol song_info_cache_button_cancel;
extern Symbol song_info_cache_create;
extern Symbol song_info_cache_missing;
extern Symbol song_info_cache_corrupt;

class SaveMemcardAction : public MemcardAction {
public:
    SaveMemcardAction(std::vector<BandProfile *> *);
    virtual ~SaveMemcardAction();
    virtual void PreAction();
    virtual void Action();
    virtual void PostAction();
    int unk24;
    int unk28;
};

class LoadMemcardAction : public MemcardAction {
public:
    LoadMemcardAction(std::vector<BandProfile *> *);
    virtual ~LoadMemcardAction();
    virtual void PreAction();
    virtual void Action();
    virtual void PostAction();
    int unk24; // 0x24
    std::vector<BandProfile *> *mProfiles; // 0x28
};

SaveLoadManager *TheSaveLoadMgr;

SaveLoadManager::SaveLoadManager()
    : mActivated(false), mInitialLoadNotDone(true), mState(kS_Idle),
      mStateAtSelectStart(kS_Idle), mUser(NULL), mLocalUser(NULL),
      unk3c(0), unk44(), unk48(0), mSaveSize(0), unk58(0), mCacheID(NULL), mCache(NULL),
      mData(NULL), unk68(false), mWaiting(false), unk6c(0), unk70(0),
      mRequestFlags(0), unk75(0), unk78(0), unk7c(0), mAction(NULL) {
    mUploadProfiles.reserve(4);
    SetName("saveload_mgr", ObjectDir::Main());
    ThePlatformMgr.AddSink(this, SigninChangedMsg::Type());
}

SaveLoadManager::~SaveLoadManager() {
    ThePlatformMgr.RemoveSink(this, SigninChangedMsg::Type());
    RELEASE(mAction);
}

bool SaveLoadManager::IsInitialLoadDone() const { return !mInitialLoadNotDone; }

bool SaveLoadManager::IsIdle() {
    bool idle = false;
    if (mState == kS_Idle && mRequestFlags == 0) {
        idle = true;
    }
    return idle;
}

int SaveLoadManager::GetDialogFocusOption() {
    int ret = 1;
    if (mState == kS_ManualLoadConfirm) {
        ret = 2;
    }
    return ret;
}

void SaveLoadManager::Activate() {
    if (!mActivated) {
        mActivated = true;
        mRequestFlags |= 2;
    }
}

void SaveLoadManager::HandleEventResponseStart(int) { mStateAtSelectStart = mState; }

void SaveLoadManager::Start() {
    mUser = NULL;
    mLocalUser = NULL;
    SetState(kS_Start);
    if (mMode == kMode_AutoLoad) {
        UpdateStatus(kSaveLoadMgrStatus_Start);
    }
}

void SaveLoadManager::Finish() {
    if (mMode == kMode_AutoLoad) {
        UpdateStatus(kSaveLoadMgrStatus_Finish);
    }
    SetState(kS_Finish);
}

void SaveLoadManager::AutoSave() {
    if (IsReasonToAutosave()) {
        mRequestFlags = 1;
    }
}

void SaveLoadManager::AutoLoad() {
    if (IsReasonToAutoload()) {
        mRequestFlags |= 2;
    }
}

void SaveLoadManager::Init() {
    MILO_ASSERT(!TheSaveLoadMgr, 0x57);
    TheSaveLoadMgr = new SaveLoadManager();
}

void SaveLoadManager::ManualDelete() {
    MILO_LOG("Manual Delete has been called\n");
    mRequestFlags |= 1;
}

namespace {
    Symbol kStrGlobalCacheName("globaloptions");
}

void SaveLoadManager::Poll() {
    if (!mActivated) return;
    State &_ref0 = mState;
    if (_ref0 == kS_Idle) {
        int flags = mRequestFlags;
        if (flags & 8) {
            mMode = kMode_ManualLoad;
            Start();
            mRequestFlags &= ~8;
            return;
        }
        if (flags & 1) {
            mMode = kMode_ManualDelete;
            Start();
            mRequestFlags &= ~1;
            return;
        }
        if (flags & 4) {
            mMode = kMode_AutoSave;
            Start();
            mRequestFlags &= ~4;
            return;
        }
        if (flags & 2) {
            if (IsReasonToAutosave()) {
                mMode = kMode_AutoSave;
                Start();
                return;
            }
            mMode = kMode_AutoLoad;
            Start();
            mRequestFlags &= ~2;
            return;
        }
        TheProfileMgr.PurgeOldData();
        AutoLoad();
        return;
    }
    if ((unsigned int)_ref0 > 0x6f) return;
    switch (_ref0) {
    case kS_Start:
        switch (mMode) {
        case kMode_AutoLoad:
            SetState((State)0x2);
            break;
        case kMode_AutoSave:
            SetState((State)0x56);
            break;
        case kMode_DisableAutoSave:
            mUser = NULL;
            SetState((State)0x42);
            break;
        case kMode_ManualDelete:
            SetState((State)0x69);
            break;
        case kMode_ManualLoad:
            SetState((State)0x51);
            break;
        default:
            TheDebug.Notify(MakeString<SaveLoadMode>("SaveLoadManager startup bad mode: %d\n", (SaveLoadMode)mMode));
            SetState((State)0x6e);
            break;
        }
        break;
    case (State)0x4:
        if (mWaiting) return;
        switch (unk6c) {
        case 7:
            SetState((State)0xb);
            break;
        case 8:
            SetState((State)0x5);
            break;
        case 9:
            SetState((State)0x7);
            break;
        default:
            SetState((State)0x42);
            break;
        }
        break;
    case (State)0x14:
        if (!TheCacheMgr->IsDone()) return;
        {
            CacheResult result = TheCacheMgr->GetLastResult();
            unk70 = (int)result;
                        switch (result) {
                case kCache_NoError:
                TheCacheMgr->AddCacheID(mCacheID, Symbol(unk4c.c_str()));
                SetState((State)0x1b);
                break;
                case kCache_ErrorCacheNotFound:
                SetState((State)0x15);
                break;
                default:
                TheDebug.Notify(MakeString<int>("SaveLoadManager - CacheMgr search returned error %d\n", (int)result));
                SetState((State)0x25);
                break;
            }
        }
        break;
    case (State)0x19:
        if (!TheCacheMgr->IsDone()) return;
        {
            CacheResult result = TheCacheMgr->GetLastResult();
            if (TheCacheMgr && result == kCache_NoError) {
                unk7c = 2;
                int sz = mCacheID->GetDeviceID();
                unk78 = sz;
                TheCacheMgr->AddCacheID(mCacheID, Symbol(unk4c.c_str()));
                SetState((State)0x20);
            } else if (result == kCache_ErrorUserCancel) {
                unk7c = 1;
                SetState((State)0x17);
            } else {
                TheDebug.Fail(MakeString<int>("SaveLoadManager - CacheMgr choose returned error %d\n", (int)result));
                SetState((State)0x25);
            }
        }
        break;
    case (State)0x1A:
        if (ThePlatformMgr.GuideShowing()) return;
        SetState((State)0x19);
        break;
    case (State)0x1B:
        if (!TheCacheMgr->IsDone()) return;
        {
            CacheResult result = TheCacheMgr->GetLastResult();
                        switch (result) {
                case kCache_NoError:
                SetState((State)0x1e);
                break;
                case kCache_ErrorStorageDeviceMissing:
                SetState((State)0x16);
                break;
                case kCache_ErrorCorrupt:
                SetState((State)0x1c);
                break;
                default:
                TheDebug.Fail(MakeString<int>("SaveLoadManager - kS_SongCacheCreateMountRead unhandled error %d\n", (int)result));
                SetState((State)0x25);
                break;
            }
        }
        break;
    case (State)0x20:
        if (!TheCacheMgr->IsDone()) return;
        {
            CacheResult result = TheCacheMgr->GetLastResult();
                        switch (result) {
                case kCache_NoError:
                SetState((State)0x21);
                break;
                case kCache_ErrorStorageDeviceMissing:
                UpdateStatus(kSaveLoadMgrStatus_Loading);
                SetState((State)0x16);
                break;
                case kCache_ErrorCorrupt:
                UpdateStatus(kSaveLoadMgrStatus_Loading);
                SetState((State)0x1c);
                break;
                default:
                UpdateStatus(kSaveLoadMgrStatus_Loading);
                TheDebug.Fail(MakeString<int>("SaveLoadManager - kS_SongCacheCreateMountWrite unhandled error %d\n", (int)result));
                SetState((State)0x25);
                break;
            }
        }
        break;
    case (State)0x1E:
        if (!mCache->IsDone()) return;
        {
            CacheResult result = mCache->GetLastResult();
                        switch (result) {
                case kCache_NoError:
                SetState((State)0x1f);
                break;
                case kCache_ErrorStorageDeviceMissing:
                SetState((State)0x16);
                break;
                default:
                SetState((State)0x25);
                break;
            }
        }
        break;
    case (State)0x1D:
        if (!TheCacheMgr->IsDone()) return;
        UpdateStatus(kSaveLoadMgrStatus_Loading);
        SetState((State)0x20);
        break;
    case (State)0x1F:
        if (!mCache->IsDone()) return;
        if (mCache->GetLastResult() == kCache_NoError) {
            BufStream stream(mData, mSaveSize, true);
            TheSongMgr.LoadCachedSongInfo(stream);
            SetState((State)0x22);
        } else {
            SetState((State)0x25);
        }
        break;
    case (State)0x21:
    case (State)0x33:
    case (State)0x3E:
        if (!mCache->IsDone()) return;
        unk70 = (int)mCache->GetLastResult();
                switch (_ref0) {
            case (State)0x21:
            SetState((State)0x23);
            break;
            case (State)0x33:
            SetState((State)0x35);
            break;
            case (State)0x3E:
            SetState((State)0x3f);
            break;
            default:
            TheDebug.Fail(MakeString("Impossible state.\n"));
            break;
        }
        break;
    case (State)0x22:
        if (!TheCacheMgr->IsDone()) return;
        if (TheCacheMgr->GetLastResult() == kCache_NoError) {
            SetState((State)0x26);
        } else {
            SetState((State)0x25);
        }
        break;
    case (State)0x23:
        if (!TheCacheMgr->IsDone()) return;
        UpdateStatus(kSaveLoadMgrStatus_Loading);
        if (unk70 == 0) {
            unk70 = (int)TheCacheMgr->GetLastResult();
        }
        if (unk70 == 0) {
            SetState((State)0x26);
        } else {
            SetState((State)0x25);
        }
        break;
    case (State)0x27:
        if (!TheCacheMgr->IsDone()) return;
        {
            CacheResult result = TheCacheMgr->GetLastResult();
            unk70 = (int)result;
                        switch (result) {
                case kCache_NoError:
                SetState((State)0x2e);
                break;
                case kCache_ErrorCacheNotFound:
                switch (unk7c) {
                case 0:
                    SetState((State)0x2b);
                    break;
                case 2:
                    SetState((State)0x2c);
                    break;
                default:
                    SetState((State)0x29);
                    break;
                }
                break;
                default:
                TheDebug.Fail(MakeString<int>("SaveLoadManager - CacheMgr search returned error %d\n", (int)result));
                SetState((State)0x37);
                break;
            }
        }
        break;
    case (State)0x2B:
    case (State)0x2C:
        if (!TheCacheMgr->IsDone()) return;
        {
            CacheResult result = TheCacheMgr->GetLastResult();
                        switch (result) {
                case kCache_NoError: {
                unk7c = 2;
                int sz = mCacheID->GetDeviceID();
                unk78 = sz;
                TheCacheMgr->AddCacheID(mCacheID, Symbol(kStrGlobalCacheName.Str()));
                SetState((State)0x31);
                break;
                }
                case kCache_ErrorUserCancel:
                unk7c = 1;
                SetState((State)0x29);
                break;
                default:
                TheDebug.Fail(MakeString<int>("SaveLoadManager - CacheMgr choose returned error %d\n", (int)result));
                SetState((State)0x37);
                break;
            }
        }
        break;
    case (State)0x2D:
        if (ThePlatformMgr.GuideShowing()) return;
        SetState((State)0x2b);
        break;
    case (State)0x2E:
        if (!TheCacheMgr->IsDone()) return;
        {
            CacheResult result = TheCacheMgr->GetLastResult();
                        switch (result) {
                case kCache_NoError:
                SetState((State)0x32);
                break;
                case kCache_ErrorStorageDeviceMissing:
                SetState((State)0x28);
                break;
                case kCache_ErrorCorrupt:
                SetState((State)0x2f);
                break;
                default:
                TheDebug.Notify(MakeString<int, int>("SaveLoadManager - unknown error %d during state %d.\n", (int)result, (int)_ref0));
                SetState((State)0x37);
                break;
            }
        }
        break;
    case (State)0x31:
        if (!TheCacheMgr->IsDone()) return;
        {
            CacheResult result = TheCacheMgr->GetLastResult();
                        switch (result) {
                case kCache_NoError:
                SetState((State)0x33);
                break;
                case kCache_ErrorStorageDeviceMissing:
                UpdateStatus(kSaveLoadMgrStatus_Loading);
                SetState((State)0x28);
                break;
                case kCache_ErrorCorrupt:
                UpdateStatus(kSaveLoadMgrStatus_Loading);
                SetState((State)0x2f);
                break;
                default:
                UpdateStatus(kSaveLoadMgrStatus_Loading);
                TheDebug.Notify(MakeString<int, int>("SaveLoadManager - unknown error %d during state %d.\n", (int)result, (int)_ref0));
                SetState((State)0x37);
                break;
            }
        }
        break;
    case (State)0x30:
        if (!TheCacheMgr->IsDone()) return;
        UpdateStatus(kSaveLoadMgrStatus_Loading);
        SetState((State)0x31);
        break;
    case (State)0x32:
        if (!mCache->IsDone()) return;
        if (mCache->GetLastResult() == kCache_NoError) {
            FixedSizeSaveableStream stream(mData, TheProfileMgr.GetGlobalOptionsSize(), true);
            TheProfileMgr.LoadGlobalOptions(stream);
            TheProfileMgr.SetGlobalOptionsSaveState(kMetaProfileLoaded);
        } else {
            TheProfileMgr.SetGlobalOptionsSaveState(kMetaProfileError);
        }
        SetState((State)0x34);
        break;
    case (State)0x3B:
        if (!TheCacheMgr->IsDone()) return;
        {
            CacheResult result = TheCacheMgr->GetLastResult();
                        switch (result) {
                case kCache_NoError: {
                unk7c = 2;
                int sz = mCacheID->GetDeviceID();
                unk78 = sz;
                TheCacheMgr->AddCacheID(mCacheID, Symbol(unk4c.c_str()));
                SetState((State)0x3d);
                break;
                }
                case kCache_ErrorUserCancel:
                unk7c = 1;
                SetState((State)0x3a);
                break;
                default:
                TheDebug.Notify(MakeString<int>("SaveLoadManager - CacheMgr choose returned error %d\n", (int)result));
                SetState((State)0x40);
                break;
            }
        }
        break;
    case (State)0x3C:
        if (ThePlatformMgr.GuideShowing()) return;
        SetState((State)0x3b);
        break;
    case (State)0x3D:
        if (!TheCacheMgr->IsDone()) return;
        {
            CacheResult result = TheCacheMgr->GetLastResult();
                        switch (result) {
                case kCache_NoError:
                SetState((State)0x3e);
                break;
                case kCache_ErrorStorageDeviceMissing:
                UpdateStatus(kSaveLoadMgrStatus_Loading);
                SetState((State)0x3a);
                break;
                default:
                UpdateStatus(kSaveLoadMgrStatus_Loading);
                TheDebug.Fail(MakeString<int>("SaveLoadManager - CacheMgr choose returned error %d\n", (int)result));
                SetState((State)0x40);
                break;
            }
        }
        break;
    case (State)0x34:
        if (!TheCacheMgr->IsDone()) return;
        if (TheCacheMgr->GetLastResult() == kCache_NoError) {
            TheProfileMgr.SetGlobalOptionsSaveState(kMetaProfileLoaded);
        } else {
            TheProfileMgr.SetGlobalOptionsSaveState(kMetaProfileError);
        }
        SetState((State)0x38);
        break;
    case (State)0x35:
        if (!TheCacheMgr->IsDone()) return;
        UpdateStatus(kSaveLoadMgrStatus_Loading);
        if (unk70 == 0) {
            unk70 = (int)TheCacheMgr->GetLastResult();
        }
        if (unk70 == 0) {
            TheProfileMgr.SetGlobalOptionsSaveState(kMetaProfileLoaded);
        } else {
            TheProfileMgr.SetGlobalOptionsSaveState(kMetaProfileError);
        }
        SetState((State)0x38);
        break;
    case (State)0x3F:
        if (!TheCacheMgr->IsDone()) return;
        UpdateStatus(kSaveLoadMgrStatus_Loading);
        if (unk70 == 0) {
            unk70 = (int)TheCacheMgr->GetLastResult();
        }
        if (unk70 == 0) {
            TheProfileMgr.SetGlobalOptionsSaveState(kMetaProfileLoaded);
        } else {
            TheProfileMgr.SetGlobalOptionsSaveState(kMetaProfileError);
        }
        SetState((State)0x41);
        break;
    case (State)0x46:
    case (State)0x47:
        if (mWaiting) return;
        UpdateStatus(kSaveLoadMgrStatus_Loading);
        switch (unk6c) {
        case 1:
            SetState((State)0x4c);
            break;
        case 7:
            MILO_ASSERT(_ref0 != kS_SaveOverwrite, 0x2fb);
            SetState((State)0x48);
            break;
        case 0:
            SetState((State)0x43);
            break;
        case 6:
            unk7c = 0;
            unk78 = 0;
            SetState((State)0x49);
            break;
        default:
            SetState((State)0x4e);
            break;
        }
        break;
    case (State)0x52:
        if (TheSongMgr.IsSongCacheWriteDone()) {
            SetState((State)0x54);
        }
        break;
    case kS_Done:
    case kS_Finish:
        if (mWaiting) return;
        if (mCache != NULL) {
            if (!mCache->IsDone()) return;
            TheCacheMgr->UnmountAsync(&mCache, NULL);
        } else {
            if (!TheCacheMgr->IsDone()) return;
            if (_ref0 == (State)0x6d) {
                SetState((State)0x6e);
            } else {
                SetState(kS_Idle);
            }
        }
        break;
    default:
        break;
    }
}

void SaveLoadManager::SetState(State newState) {
    State oldState = mState;
    if (oldState == newState) return;

    bool wasIdle = false;
    // Exit-state cleanup based on which state we're leaving.
    switch ((int)oldState) {
    case 0x0:
        wasIdle = true;
        break;
    case 0x1f:
    case 0x21:
    case 0x32:
    case 0x33:
    case 0x3e:
        if (newState != (State)0x6f) {
            if (mData != NULL) {
                _MemFree(mData);
                mData = NULL;
            }
        }
        break;
    case 0xb:
    case 0x46:
    case 0x47:
    case 0x64:
        if (newState != (State)0x6d) {
            delete mAction;
            mAction = NULL;
        }
        break;
    case 0x6f:
        if (mData != NULL) {
            _MemFree(mData);
            mData = NULL;
        }
        break;
    case 0x6d:
        delete mAction;
        mAction = NULL;
        break;
    default:
        break;
    }

    mState = newState;
    if (wasIdle) {
        UpdateStatus((SaveLoadMgrStatus)0);
    }
    switch ((int)mState) {
    case 0x0: // kS_Idle
        UpdateStatus((SaveLoadMgrStatus)5);
        break;
    case 0x1: // kS_Start
        unk7c = 0;
        break;
    case 0x2:
        if (mInitialLoadNotDone) {
            SetState((State)0x14);
        } else {
            SetState((State)0x3);
        }
        break;
    case 0x3: // kS_AutoloadSelectProfile
    {
        mUploadProfiles.erase(mUploadProfiles.begin(), mUploadProfiles.end());
        {
            std::vector<BandProfile *> newProfiles = TheProfileMgr.GetNewlySignedInProfiles();
            mUploadProfiles = newProfiles;
        }
        if (TheMemcardMgr.IsDisableWriting() || mUploadProfiles.size() == 0 && !mInitialLoadNotDone) {
            mUser = NULL;
            SetState((State)0x12);
        } else {
            SetState((State)0xb);
            mInitialLoadNotDone = false;
        }
        break;
    }
    case 0x4:
    {
        BandProfile *pProfile = GetProfile();
        MILO_ASSERT(pProfile, 0x538);
        mWaiting = true;
        TheMemcardMgr.AddSink(this);
        TheMemcardMgr.OnSearchForDevice(pProfile);
        break;
    }
    case 0x6: // kS_AutoloadNoSaveFound_Msg
    {
        Symbol dummy(saveload_dialog_event);
        TheUIEventMgr->TriggerEvent(saveload_dialog_event, NULL);
        break;
    }
    case 0x5:
        if (unk7c == 2) {
            SetState((State)0x9);
        } else {
            SetState((State)0x6);
        }
        break;
    case 0x7: // kS_AutoloadMultipleSavesFound
    {
        Symbol dummy(saveload_dialog_event);
        TheUIEventMgr->TriggerEvent(saveload_dialog_event, NULL);
        break;
    }
    case 0x8: // kS_AutoloadSetDevice
    {
        MILO_ASSERT(unk7c == 2, 0x559);
        TheMemcardMgr.SetDevice(unk78);
        SetState((State)0xb);
        break;
    }
    case 0x9: // kS_AutoloadSelectDevice2
    {
        BandProfile *pProfile = GetProfile();
        MILO_ASSERT(pProfile, 0x564);
        int devId = -1;
        if (mLocalUser != NULL) devId = mLocalUser->GetPadNum();
        mWaiting = true;
        TheMemcardMgr.AddSink(this);
        TheMemcardMgr.SelectDevice(pProfile, this, devId, false);
        break;
    }
    case 0xa: // kS_AutoloadSelectDevice3
    case 0xd:
    {
        BandProfile *pProfile = GetProfile();
        MILO_ASSERT(pProfile, 0x57b);
        int devId = -1;
        if (mLocalUser != NULL) devId = mLocalUser->GetPadNum();
        mWaiting = true;
        TheMemcardMgr.AddSink(this);
        TheMemcardMgr.SelectDevice(pProfile, this, devId, true);
        break;
    }
    case 0xb: // kS_AutoloadStartLoad
    {
        for (BandProfile **pp = mUploadProfiles.begin(); pp != mUploadProfiles.end(); pp++) {
            (*pp)->PreLoad();
        }
        if (TheWiiProfileMgr.NeedsLoading()) TheWiiProfileMgr.PreLoad();
        mWaiting = true;
        delete mAction;
        mAction = NULL;
        mAction = new LoadMemcardAction(&mUploadProfiles);
        mInitialLoadNotDone = false;
        TheMemcardMgr.AddSink(this);
        TheMemcardMgr.OnLoadGame(NULL, mAction);
        break;
    }
    case 0xc: // kS_AutoloadNotOwner
    case 0xe:
    case 0xf:
    {
        Symbol dummy(saveload_dialog_event);
        TheUIEventMgr->TriggerEvent(saveload_dialog_event, NULL);
        break;
    }
    case 0x10: // kS_AutoloadDeviceMissing
    case 0x11:
        SetState((State)0x46);
        break;
    case 0x12:
    {
        mInitialLoadNotDone = false;
        if (TheProfileMgr.GlobalOptionsNeedsSave()) {
            if (!TheMemcardMgr.IsDisableWriting()) {
                SetState((State)0x46);
                break;
            }
        }
        TheProfileMgr.HandleProfileLoadComplete();
        SetState((State)0x6e);
        break;
    }
    case 0x14: // kS_SongCacheCreateSearch (entry-like)
    {
        unk4c = TheSongMgr.GetCachedSongInfoName();
        if (mCacheID != NULL) {
            TheCacheMgr->RemoveCacheID(mCacheID);
            delete mCacheID;
            mCacheID = NULL;
        }
        if (!TheSongMgr.CreateSongCacheID(&mCacheID)) {
            TheDebug.Notify(MakeString("SaveLoadManager - CacheMgr search failed in CreateSongCacheID()\n"));
        }
        if (!TheCacheMgr->SearchAsync(unk4c.c_str(), &mCacheID)) {
#pragma dont_inline on
            TheDebug.Fail(MakeString<int>("TheCacheMgr->SearchAsync() failed with CacheResult %d\n", (int)TheCacheMgr->GetLastResult()));
#pragma dont_inline reset
        }
        break;
    }
    case 0x17:
    case 0x18:
    {
        Symbol dummy(saveload_dialog_event);
        TheUIEventMgr->TriggerEvent(saveload_dialog_event, NULL);
        break;
    }
    case 0x15: // kS_SongCacheCreateNotFound_Msg
    case 0x16:
    {
        TheCacheMgr->AddCacheID(mCacheID, unk4c.c_str());
        SetState((State)0x20);
        break;
    }
    case 0x19:
    {
        if (mCacheID != NULL) {
            TheCacheMgr->RemoveCacheID(mCacheID);
            delete mCacheID;
            mCacheID = NULL;
        }
        const char *cacheName = unk4c.c_str();
        const char *locName = Localize(song_info_cache_name, NULL);
        if (!TheCacheMgr->ShowUserSelectUIAsync(NULL, 0x25800ULL, cacheName, locName, &mCacheID)) {
            if (TheCacheMgr->GetLastResult() != 0) {
                SetState((State)0x1a);
            }
        }
        break;
    }
    // State 0x1a has no entry body in target (async-wait, polled).
    case 0x1b:
    {
        if (!TheCacheMgr->MountAsync(mCacheID, &mCache, NULL)) {
#pragma dont_inline on
            TheDebug.Fail(MakeString<int>("TheCacheMgr->MountAsync failed with CacheResult %d\n", (int)TheCacheMgr->GetLastResult()));
#pragma dont_inline reset
        }
        break;
    }
    case 0x20:
    {
        UpdateStatus((SaveLoadMgrStatus)1);
        if (!TheCacheMgr->MountAsync(mCacheID, &mCache, NULL)) {
#pragma dont_inline on
            TheDebug.Fail(MakeString<int>("TheCacheMgr->MountAsync failed with CacheResult %d\n", (int)TheCacheMgr->GetLastResult()));
#pragma dont_inline reset
        }
        break;
    }
    case 0x1c:
    {
        Symbol dummy(saveload_dialog_event);
        TheUIEventMgr->TriggerEvent(saveload_dialog_event, NULL);
        break;
    }
    case 0x1d:
    {
        UpdateStatus((SaveLoadMgrStatus)1);
        if (!TheCacheMgr->DeleteAsync(mCacheID)) {
#pragma dont_inline on
            TheDebug.Fail(MakeString<int>("TheCacheMgr->DeleteAsync failed with CacheResult %d\n", (int)TheCacheMgr->GetLastResult()));
#pragma dont_inline reset
        }
        break;
    }
    case 0x1f:
    {
        mData = (_MemAllocTemp)(mSaveSize, 0);
        if (!mCache->ReadAsync(unk4c.c_str(), mData, (unsigned int)mSaveSize, NULL)) {
#pragma dont_inline on
            TheDebug.Fail(MakeString<int>("mCache->ReadAsync failed with CacheResult %d\n", (int)TheCacheMgr->GetLastResult()));
#pragma dont_inline reset
        }
        break;
    }
    case 0x1e:
    {
        if (!mCache->GetFileSizeAsync(unk4c.c_str(), (unsigned int *)&mSaveSize, NULL)) {
#pragma dont_inline on
            TheDebug.Fail(MakeString<int>("mCache->GetFileSizeAsync failed with CacheResult %d\n", (int)TheCacheMgr->GetLastResult()));
#pragma dont_inline reset
        }
        break;
    }
    case 0x21:
    {
        int sz = TheSongMgr.GetCachedSongInfoSize();
        mData = (_MemAllocTemp)(sz, 0);
        BufStream stream(mData, sz, true);
        if (TheSongMgr.SaveCachedSongInfo(stream)) {
            if (!mCache->WriteAsync(unk4c.c_str(), mData, (unsigned int)sz, NULL)) {
#pragma dont_inline on
                TheDebug.Fail(MakeString<int>("mCache->WriteAsync failed with CacheResult %d\n", (int)TheCacheMgr->GetLastResult()));
#pragma dont_inline reset
            }
        }
        break;
    }
    case 0x22:
    case 0x23:
    {
        if (!TheCacheMgr->UnmountAsync(&mCache, NULL)) {
#pragma dont_inline on
            TheDebug.Fail(MakeString<int>("TheCacheMgr->UnmountAsync failed with CacheResult %d\n", (int)TheCacheMgr->GetLastResult()));
#pragma dont_inline reset
        }
        break;
    }
    case 0x24:
    {
        unk7c = 1;
        unk78 = 0;
        unk68 = true;
        SetState((State)(mCache != NULL ? 0x22 : 0x26));
        break;
    }
    case 0x25:
    {
        unk7c = 0;
        unk78 = 0;
        unk68 = true;
        SetState((State)(mCache != NULL ? 0x22 : 0x26));
        break;
    }
    case 0x26:
        mCacheID = NULL;
        SetState((State)0x3);
        break;
    case 0x13:
    {
        if (mCacheID == NULL) {
            mCacheID = TheCacheMgr->GetCacheID(kStrGlobalCacheName.Str());
        }
        if (mCacheID == NULL) {
            SetState((State)0x37);
        } else {
            SetState((State)0x31);
        }
        break;
    }
    case 0x27:
    {
        if (mCacheID != NULL) {
            TheCacheMgr->RemoveCacheID(mCacheID);
            delete mCacheID;
            mCacheID = NULL;
        }
        if (!TheCacheMgr->SearchAsync(kStrGlobalCacheName.Str(), &mCacheID)) {
#pragma dont_inline on
            TheDebug.Fail(MakeString<int>("TheCacheMgr->SearchAsync failed with CacheResult %d\n", (int)TheCacheMgr->GetLastResult()));
#pragma dont_inline reset
        }
        break;
    }
    case 0x29:
    case 0x2a:
    case 0x3a:
    {
        Symbol dummy(saveload_dialog_event);
        TheUIEventMgr->TriggerEvent(saveload_dialog_event, NULL);
        break;
    }
    case 0x28:
    {
        if (unk7c == 0) {
            SetState((State)0x2b);
        } else {
            unk7c = 0;
            SetState((State)0x2a);
        }
        break;
    }
    case 0x2b:
    {
        if (mCacheID != NULL) {
            TheCacheMgr->RemoveCacheID(mCacheID);
            delete mCacheID;
            mCacheID = NULL;
        }
        int sz = TheProfileMgr.GetGlobalOptionsSize();
        const char *locName = Localize(global_options_cache_name, NULL);
        if (!TheCacheMgr->ShowUserSelectUIAsync(NULL, (unsigned long long)sz, kStrGlobalCacheName.Str(), locName, &mCacheID)) {
            if (TheCacheMgr->GetLastResult() != kCache_NoError) {
                SetState((State)0x2d);
            }
        }
        break;
    }
    case 0x2c:
    {
        MILO_ASSERT(unk7c == 2, 0x720);
        if (mCacheID != NULL) {
            TheCacheMgr->RemoveCacheID(mCacheID);
            delete mCacheID;
            mCacheID = NULL;
        }
        const char *locName = Localize(global_options_cache_name, NULL);
        TheCacheMgr->CreateCacheIDFromDeviceID(unk78, kStrGlobalCacheName.Str(), locName, &mCacheID);
        break;
    }
    // State 0x2d has no entry body in target (async-wait, polled).
    case 0x39:
    {
        if (unk7c == 0 || unk7c == 2) {
            SetState((State)0x3b);
        } else {
            SetState((State)0x3a);
        }
        break;
    }
    case 0x3b:
    {
        if (mCacheID != NULL) {
            TheCacheMgr->RemoveCacheID(mCacheID);
            delete mCacheID;
            mCacheID = NULL;
        }
        int sz = TheProfileMgr.GetGlobalOptionsSize();
        const char *locName = Localize(global_options_cache_name, NULL);
        if (!TheCacheMgr->ShowUserSelectUIAsync(NULL, (unsigned long long)sz, kStrGlobalCacheName.Str(), locName, &mCacheID)) {
            if (TheCacheMgr->GetLastResult() != kCache_NoError) {
                SetState((State)0x3c);
            }
        }
        break;
    }
    case 0x2e:
    {
        if (!TheCacheMgr->MountAsync(mCacheID, &mCache, NULL)) {
#pragma dont_inline on
            TheDebug.Fail(MakeString<int>("TheCacheMgr->MountAsync failed with CacheResult %d\n", (int)TheCacheMgr->GetLastResult()));
#pragma dont_inline reset
        }
        break;
    }
    case 0x31:
    case 0x3d:
    {
        UpdateStatus((SaveLoadMgrStatus)1);
        if (!TheCacheMgr->MountAsync(mCacheID, &mCache, NULL)) {
#pragma dont_inline on
            TheDebug.Fail(MakeString<int>("TheCacheMgr->MountAsync failed with CacheResult %d\n", (int)TheCacheMgr->GetLastResult()));
#pragma dont_inline reset
        }
        break;
    }
    case 0x2f:
    {
        Symbol dummy(saveload_dialog_event);
        TheUIEventMgr->TriggerEvent(saveload_dialog_event, NULL);
        break;
    }
    case 0x30:
    {
        UpdateStatus((SaveLoadMgrStatus)1);
        if (!TheCacheMgr->DeleteAsync(mCacheID)) {
#pragma dont_inline on
            TheDebug.Fail(MakeString<int>("TheCacheMgr->DeleteAsync failed with CacheResult %d\n", (int)TheCacheMgr->GetLastResult()));
#pragma dont_inline reset
        }
        break;
    }
    case 0x32:
    {
        int sz = TheProfileMgr.GetGlobalOptionsSize();
        mData = (_MemAllocTemp)(sz, 0);
        if (!mCache->ReadAsync(kStrGlobalCacheName.Str(), mData, (unsigned int)sz, NULL)) {
#pragma dont_inline on
            TheDebug.Fail(MakeString<int>("TheCacheMgr->ReadAsync failed with CacheResult %d\n", (int)TheCacheMgr->GetLastResult()));
#pragma dont_inline reset
        }
        break;
    }
    case 0x33:
    case 0x3e:
    {
        UpdateStatus((SaveLoadMgrStatus)1);
        int sz = TheProfileMgr.GetGlobalOptionsSize();
        mData = (_MemAllocTemp)(sz, 0);
        FixedSizeSaveableStream stream(mData, sz, true);
        TheProfileMgr.SaveGlobalOptions(stream);
        if (!mCache->WriteAsync(kStrGlobalCacheName.Str(), mData, (unsigned int)sz, NULL)) {
#pragma dont_inline on
            TheDebug.Fail(MakeString<int>("mCache->WriteAsync failed with CacheResult %d\n", (int)TheCacheMgr->GetLastResult()));
#pragma dont_inline reset
        }
        break;
    }
    case 0x34:
    case 0x35:
    case 0x3f:
    {
        if (!TheCacheMgr->UnmountAsync(&mCache, NULL)) {
            if (TheCacheMgr->GetLastResult() != kCache_ErrorStorageDeviceMissing) {
#pragma dont_inline on
                TheDebug.Notify(MakeString<CacheResult>("UnmountAsync failed with error %d\n", TheCacheMgr->GetLastResult()));
#pragma dont_inline reset
            }
        }
        break;
    }
    case 0x36:
    {
        unk7c = 1;
        unk78 = 0;
        TheProfileMgr.SetGlobalOptionsSaveState(kMetaProfileLoaded);
        SetState((State)0x38);
        break;
    }
    case 0x37:
    {
        unk7c = 0;
        unk78 = 0;
        TheProfileMgr.SetGlobalOptionsSaveState(kMetaProfileLoaded);
        SetState((State)0x38);
        break;
    }
    case 0x38:
    {
        bool moreThanOne;
        {
            std::vector<BandProfile *> newProfiles = TheProfileMgr.GetNewlySignedInProfiles();
            moreThanOne = (newProfiles.size() > 1);
        }
        if (moreThanOne) unk7c = 1;
        SetState((State)0x3);
        break;
    }
    // State 0x3c has no entry body in target (async-wait, polled).
    case 0x40:
    {
        unk7c = 0;
        unk78 = 0;
        TheProfileMgr.SetGlobalOptionsSaveState(kMetaProfileLoaded);
        SetState((State)0x41);
        break;
    }
    case 0x41:
        SetState((State)0x54);
        break;
    case 0x42:
    {
        unk7c = 0;
        TheMemcardMgr.DisableWriting(true);
        for (BandProfile **pp = mUploadProfiles.begin(); pp != mUploadProfiles.end(); pp++) {
            TheMemcardMgr.SaveLoadProfileComplete(*pp, 2);
        }
        TheProfileMgr.SetGlobalOptionsSaveState(kMetaProfileLoaded);
        {
            Symbol dummy(saveload_dialog_event);
            TheUIEventMgr->TriggerEvent(saveload_dialog_event, NULL);
        }
        break;
    }
    case 0x43:
    case 0x44:
    {
        unk7c = 0;
        int saveResult = (newState == (State)0x43) ? 1 : -1;
        for (BandProfile **pp = mUploadProfiles.begin(); pp != mUploadProfiles.end(); pp++) {
            TheMemcardMgr.SaveLoadProfileComplete(*pp, saveResult);
        }
        TheProfileMgr.SetGlobalOptionsSaveState((ProfileSaveState)saveResult);
        switch (mMode) {
        case kMode_AutoLoad:
            SetState((State)0x3);
            break;
        case kMode_AutoSave:
        case kMode_ManualLoad:
            SetState((State)0x54);
            break;
        default:
            break;
        }
        break;
    }
    case 0x45:
    {
        BandProfile *pProfile = GetProfile();
        MILO_ASSERT(pProfile, 0x839);
        mWaiting = true;
        Hmx::Object *localUser = NULL;
        if (mLocalUser != NULL) localUser = mLocalUser;
        TheMemcardMgr.AddSink(this);
        TheMemcardMgr.OnCheckForSaveContainer(pProfile);
        break;
    }
    case 0x46:
        StartSaveAction(true);
        break;
    case 0x47:
        StartSaveAction(false);
        break;
    case 0x48:
    case 0x49:
    case 0x4c:
    case 0x4d:
    case 0x50:
    {
        Symbol dummy(saveload_dialog_event);
        TheUIEventMgr->TriggerEvent(saveload_dialog_event, NULL);
        break;
    }
    case 0x4e:
    {
        Symbol dummy(saveload_dialog_event);
        TheUIEventMgr->TriggerEvent(saveload_dialog_event, NULL);
        break;
    }
    case 0x4f:
    {
        Symbol dummy(saveload_dialog_event);
        TheUIEventMgr->TriggerEvent(saveload_dialog_event, NULL);
        break;
    }
    // States 0x4a, 0x4b have no entry body in target (async-wait, polled).
    case 0x51:
    {
        bool needsWrite = false;
        if (TheSongMgr.SongCacheNeedsWrite() && !unk68) {
            needsWrite = true;
        }
        if (needsWrite) {
            SetState((State)0x52);
        } else {
            SetState((State)0x54);
        }
        break;
    }
    case 0x52:
        TheSongMgr.StartSongCacheWrite();
        break;
    case 0x53:
    {
        if (mCacheID == NULL) {
            mCacheID = TheCacheMgr->GetCacheID(kStrGlobalCacheName.Str());
        }
        if (mCacheID == NULL) {
            SetState((State)0x40);
        } else {
            SetState((State)0x3d);
        }
        break;
    }
    case 0x54:
    {
        mUploadProfiles.erase(mUploadProfiles.begin(), mUploadProfiles.end());
        mUploadProfiles = TheProfileMgr.GetShouldAutosaveProfiles();
        if (!TheMemcardMgr.IsDisableWriting()) {
            if (mUploadProfiles.size() != 0 ||
                TheWiiProfileMgr.NeedsSave() ||
                TheProfileMgr.GlobalOptionsNeedsSave())
            {
                SetState((State)0x46);
                break;
            }
        }
        mUser = NULL;
        SetState((State)0x55);
        break;
    }
    case 0x55:
        SetState((State)0x6e);
        TheProfileMgr.HandleProfileSaveComplete();
        break;
    case 0x56:
    {
        mUploadProfiles.erase(mUploadProfiles.begin(), mUploadProfiles.end());
        if (IsReasonToUpload()) {
            mUploadProfiles = TheProfileMgr.GetSignedInProfiles();
        }
        if (mUploadProfiles.size() != 0) {
            SetState((State)0x58);
        } else {
            SetState((State)0x59);
        }
        break;
    }
    case 0x58:
    {
        MILO_ASSERT(mUploadProfiles.size() != 0, 0x8f9);
        BandProfile *pProfile = mUploadProfiles.front();
        mWaiting = true;
        Hmx::Object *localUser = NULL;
        if (mLocalUser != NULL) localUser = mLocalUser;
        TheMemcardMgr.AddSink(this);
        TheEntityUploader.UpdateFromProfile(pProfile, this);
        pProfile->SendBandLogo();
        break;
    }
    case 0x57:
    {
        mUploadProfiles.erase(mUploadProfiles.begin());
        if (mUploadProfiles.size() != 0) {
            SetState((State)0x58);
        } else {
            SetState((State)0x59);
        }
        break;
    }
    case 0x59:
        SetState((State)0x51);
        break;
    case 0x5a:
        SetState((State)0x46);
        break;
    case 0x5b:
    case 0x61:
        MILO_FAIL("SelectDevice not supported on the Wii.\n");
        break;
    case 0x5c:
    {
        Symbol dummy(saveload_dialog_event);
        TheUIEventMgr->TriggerEvent(saveload_dialog_event, NULL);
        break;
    }
    case 0x5d:
        SetState((State)0x6e);
        break;
    case 0x5e:
    {
        int padNum = 0;
        if (mUser != NULL) padNum = mUser->GetPadNum();
        if (TheProfileMgr.HasUnsavedDataForPad(padNum)) {
            SetState((State)0x5f);
        } else {
            SetState((State)0x64);
        }
        break;
    }
    case 0x5f:
    case 0x60:
    case 0x62:
    case 0x63:
    case 0x65:
    case 0x66:
    case 0x67:
    {
        Symbol dummy(saveload_dialog_event);
        TheUIEventMgr->TriggerEvent(saveload_dialog_event, NULL);
        break;
    }
    case 0x64:
    {
        for (BandProfile **pp = mUploadProfiles.begin(); pp != mUploadProfiles.end(); pp++) {
            (*pp)->PreLoad();
        }
        mWaiting = true;
        delete mAction;
        mAction = NULL;
        mAction = new LoadMemcardAction(&mUploadProfiles);
        Hmx::Object *localUser = NULL;
        if (mLocalUser != NULL) localUser = mLocalUser;
        TheMemcardMgr.AddSink(this);
        TheMemcardMgr.OnLoadGame(NULL, mAction);
        break;
    }
    case 0x68:
        SetState((State)0x6e);
        break;
    case 0x69:
        SetState((State)0x6a);
        break;
    case 0x6a:
    {
        MILO_LOG("\n**SaveLoadManager: kS_ManualDeleteStart\n");
        mWaiting = true;
        Hmx::Object *localUser = NULL;
        if (mLocalUser != NULL) localUser = mLocalUser;
        TheMemcardMgr.AddSink(this);
        TheMemcardMgr.OnDeleteSaves(NULL);
        break;
    }
    case 0x6b:
        SetState((State)0x6e);
        break;
    case 0x6c:
        SetState((State)0x6e);
        break;
    // State 0x6d has no entry body in target (async-wait, polled).
    case 0x6e:
    {
        TheMemcardMgr.SaveLoadAllComplete();
        Finish();
        break;
    }
    default:
        return;
    }
}

void SaveLoadManager::SaveLoadErrorSetState() {
    switch (mMode) {
    case kMode_AutoLoad:
        SetState(kS_AutoloadSelectProfile);
        break;
    case kMode_AutoSave:
    case kMode_ManualLoad:
        SetState(kS_SaveCheckProfile);
        break;
    case kMode_DisableAutoSave:
        SetState(kS_SaveCheckAutosave);
        break;
    default:
        break;
    }
}

void SaveLoadManager::UpdateStatus(SaveLoadMgrStatus status) {
    static SaveLoadMgrStatusUpdateMsg msg(-1);
    msg[0] = (int)status;
    Export(msg, true);
}

bool SaveLoadManager::IsReasonToAutosave() {
    if (GetAutosavableProfile()) {
        return true;
    }
    if (IsReasonToUpload()) {
        return true;
    }
    if (TheProfileMgr.GlobalOptionsNeedsSave()) {
        return true;
    }
    if (TheSongMgr.SongCacheNeedsWrite() && !unk68) {
        return true;
    }
    return false;
}

void SaveLoadManager::AutoSaveNow() {
    if (IsReasonToAutosave()) {
        int i = 0x20;
        mRequestFlags |= 8;
        TheEntityUploader.Abort();
        do {
            TheEntityUploader.Poll();
            Poll();
            i--;
        } while (mState != kS_Idle && i > 0);
    }
}

BandProfile *SaveLoadManager::GetNewSigninProfile() {
    std::vector<BandProfile *> profiles = TheProfileMgr.GetNewlySignedInProfiles();
    if (!profiles.empty()) {
        BandProfile *pProfile = profiles[0];
        MILO_ASSERT(pProfile, 0x484);
        return pProfile;
    }
    return NULL;
}

BandProfile *SaveLoadManager::GetAutosavableProfile() {
    std::vector<BandProfile *> profiles = TheProfileMgr.GetShouldAutosaveProfiles();
    if (!profiles.empty()) {
        BandProfile *pProfile = profiles[0];
        MILO_ASSERT(pProfile, 0x494);
        return pProfile;
    }
    return NULL;
}

Symbol SaveLoadManager::GetDialogOpt1() {
    Symbol sym(gNullStr);
    switch (mState) {
    case (State)0x49:
    case kS_GlobalCreateCorrupt:        // 0x4E
        sym = global_options_button_cancel;
        break;
    case kS_AutoloadNoSaveFound_Msg:    // 0x06
        sym = mc_button_create_data;
        break;
    case kS_ManualLoadConfirm_Yes:      // 0x5F
        sym = mc_button_continue;
        break;
    case kS_AutoloadMultipleSavesFound: // 0x07
    case kS_AutoloadNotOwner:           // 0x0C
    case kS_GlobalCreateNotFound_Msg:   // 0x4C
    case kS_ManualLoadNoDevice:         // 0x5C
    case (State)0x62:
        sym = mc_button_choose_device;
        break;
    case kS_AutoloadCorrupt:            // 0x0E
    case kS_AutoloadObsolete:           // 0x0F
    case kS_AutoloadFuture:             // 0x10
    case kS_AutoloadFuture2:            // 0x11
    case kS_SaveDeviceInvalid:          // 0x48
        sym = mc_button_overwrite;
        break;
    case (State)0x17:
    case (State)0x18:
        sym = song_info_cache_button_create;
        break;
    case (State)0x1C:
        sym = song_info_cache_button_corrupt_overwrite;
        break;
    case (State)0x29:
    case (State)0x2A:
    case (State)0x3A:
        sym = global_options_button_create;
        break;
    case (State)0x2F:
        sym = global_options_button_corrupt_overwrite;
        break;
    case kS_ManualLoadConfirm:          // 0x60
        sym = mc_button_yes;
        break;
    default:
        break;
    }
    return sym;
}

Symbol SaveLoadManager::GetDialogOpt2() {
    Symbol sym(gNullStr);
    switch (mState) {
    case kS_AutoloadNoSaveFound_Msg:
    case kS_AutoloadMultipleSavesFound:
    case kS_AutoloadDeviceMissing:
    case kS_SaveOverwrite:
    case kS_ManualSaveNoDevice:
    case kS_ManualLoadConfirmUnsaved:
    case kS_ManualLoadNoDevice:
        sym = mc_button_cancel;
        break;
    case kS_AutoloadNotOwner:
    case kS_AutoloadCorrupt:
    case kS_AutoloadObsolete:
    case kS_AutoloadFuture:
        sym = mc_button_continue_no_save;
        break;
    case kS_SongCacheCreateNotFound_Msg:
    case kS_SongCacheCreateMissing_Msg:
    case kS_SongCacheCreateCorrupt:
        sym = song_info_cache_button_cancel;
        break;
    case kS_GlobalCreateNotFound_Msg:
    case kS_GlobalCreateMissing_Msg:
    case kS_GlobalCreateCorrupt:
    case kS_GlobalOptionsMissing_Msg:
        sym = global_options_button_cancel;
        break;
    case kS_SaveDeviceInvalid:
        sym = mc_button_disable_autosave;
        break;
    case kS_ManualLoadConfirm:
        sym = mc_button_no;
        break;
    default:
        break;
    }
    return sym;
}

DataNode SaveLoadManager::GetDialogMsg() {
    String profileName(gNullStr);
    LocalBandUser * &_ref0 = mUser;
    int playerNum = -1;
    if (_ref0 != NULL) {
        profileName = _ref0->UserName();
        auto _tmp0 = _ref0->GetPadNum();
        playerNum = _tmp0 + 1;
    }
    switch (mState) {
    case (State)0x6:
        return DataArrayPtr(
            mc_auto_load_no_save_found_fmt, DataArrayPtr(), profileName, playerNum
        );
    case (State)0x7:
        return DataArrayPtr(
            mc_auto_load_multiple_saves_found_fmt,
            DataArrayPtr(),
            profileName,
            playerNum
        );
    case (State)0xC:
        return DataArrayPtr(
            mc_load_device_missing_fmt, DataArrayPtr(), profileName, playerNum
        );
    case (State)0xE: {
        BandProfile *pProfile = GetProfile();
        if (pProfile == NULL) {
            return DataArrayPtr(mc_manual_load_corrupt, DataArrayPtr());
        }
        MILO_ASSERT(pProfile, 0xD4D);
        return DataArrayPtr(
            mc_auto_load_corrupt, DataArrayPtr(), pProfile->GetName()
        );
    }
    case (State)0xF:
        return DataArrayPtr(mc_auto_load_not_owner, DataArrayPtr());
    case (State)0x10:
        if (playerNum != -1) {
            return DataArrayPtr(
                mc_auto_load_obsolete_version_fmt,
                DataArrayPtr(),
                profileName,
                playerNum
            );
        }
        return DataArrayPtr(mc_auto_load_obsolete_version, DataArrayPtr());
    case (State)0x11:
        if (playerNum != -1) {
            return DataArrayPtr(
                mc_auto_load_newer_version_fmt,
                DataArrayPtr(),
                profileName,
                playerNum
            );
        }
        return DataArrayPtr(mc_auto_load_newer_version, DataArrayPtr());
    case (State)0x17:
        return DataArrayPtr(song_info_cache_create, DataArrayPtr());
    case (State)0x18:
        return DataArrayPtr(song_info_cache_missing, DataArrayPtr());
    case (State)0x1C:
        return DataArrayPtr(song_info_cache_corrupt, DataArrayPtr());
    case (State)0x29:
        return DataArrayPtr(global_options_create, DataArrayPtr());
    case (State)0x2A:
        return DataArrayPtr(global_options_missing, DataArrayPtr());
    case (State)0x2F:
        return DataArrayPtr(global_options_corrupt, DataArrayPtr());
    case (State)0x3A:
        return DataArrayPtr(global_options_missing, DataArrayPtr());
    case (State)0x42:
        return DataArrayPtr(mc_autosave_disabled, DataArrayPtr());
    case (State)0x48:
        return DataArrayPtr(mc_save_confirm_overwrite, DataArrayPtr());
    case (State)0x49:
        if (TheMemcardMgr.GetSizeNeeded() > 0) {
            int sz = TheMemcardMgr.GetSizeNeeded();
            if (!TheCacheMgr || !TheCacheMgr->IsDone() ||
                TheCacheMgr->GetLastResult() != kCache_NoError) {
                sz += 0x10;
            }
            return DataArrayPtr(mc_save_not_enough_space_fmt, DataArrayPtr(), sz);
        }
        return DataArrayPtr(mc_save_not_enough_space, DataArrayPtr());
    case (State)0x4C:
        return DataArrayPtr(
            mc_save_device_missing_fmt, DataArrayPtr(), profileName, playerNum
        );
    case (State)0x4E:
        return DataArrayPtr(mc_save_failed, DataArrayPtr());
    case (State)0x4F:
        return DataArrayPtr(mc_save_disabled_by_cheat, DataArrayPtr());
    case (State)0x50:
        return DataArrayPtr(mc_load_failed, DataArrayPtr());
    case (State)0x5C:
        return DataArrayPtr(mc_manual_save_no_selection, DataArrayPtr());
    case (State)0x5F:
        if (playerNum != -1) {
            return DataArrayPtr(
                mc_manual_load_confirm_unsaved_fmt,
                DataArrayPtr(),
                profileName,
                playerNum
            );
        }
        return DataArrayPtr(mc_manual_load_confirm_unsaved, DataArrayPtr());
    case (State)0x60:
        return DataArrayPtr(mc_manual_load_confirm, DataArrayPtr());
    case (State)0x62:
        return DataArrayPtr(mc_manual_load_no_selection, DataArrayPtr());
    case (State)0x63:
        return DataArrayPtr(mc_manual_load_storage_missing, DataArrayPtr());
    case (State)0x65:
        return DataArrayPtr(mc_manual_load_no_file, DataArrayPtr());
    case (State)0x66:
        return DataArrayPtr(mc_manual_load_corrupt, DataArrayPtr());
    case (State)0x67:
        return DataArrayPtr(mc_manual_load_not_owner, DataArrayPtr());
    default:
        MILO_ASSERT(false, 0xE00);
        return DataNode(0);
    }
}

Symbol SaveLoadManager::GetDialogOpt3() {
    Symbol sym(gNullStr);
    if (mState == kS_SaveNotEnoughSpacePS3) {
        sym = mc_button_continue_no_save;
    }
    return sym;
}

BandProfile *SaveLoadManager::GetProfile() {
    return TheProfileMgr.GetProfileForUser(mUser);
}

bool SaveLoadManager::IsReasonToAutoload() {
    return GetNewSigninProfile() != NULL || mInitialLoadNotDone;
}

bool SaveLoadManager::IsAutosaveEnabled(LocalBandUser *user) {
    Profile *profile = TheProfileMgr.GetProfileForUser(user);
    if (!profile) {
        MILO_WARN("Tried to get autosave enabled status without a valid profile.\n");
        return false;
    }
    return profile->IsAutosaveEnabled();
}

void SaveLoadManager::EnableAutosave(LocalBandUser *user) {
    Profile *profile = TheProfileMgr.GetProfileForUser(user);
    if (!profile) {
        MILO_WARN("Tried to enable autosave without a valid profile.\n");
        return;
    }
    TheMemcardMgr.DisableWriting(false);
    profile->SetSaveState(kMetaProfileLoaded);
    ManualSave(user);
}

void SaveLoadManager::DisableAutosave(LocalBandUser *user) {
    Profile *profile = TheProfileMgr.GetProfileForUser(user);
    if (!profile) {
        MILO_WARN("Tried to disable autosave without a valid profile.\n");
        return;
    }
    bool idle = false;
    if (mState == kS_Idle && mRequestFlags == 0) {
        idle = true;
    }
    if (!idle) {
        MILO_WARN("Tried to disable autosave while saveloadmgr is not idle.\n");
        return;
    }
    profile->SetSaveState(kMetaProfileError);
}

void SaveLoadManager::ManualSave(LocalBandUser *user) {
    if (mState != kS_Idle) {
        MILO_WARN(
            "Attempted to perform a manual save, but saveloadmgr is not idle (state = %d).\n",
            mState
        );
        return;
    }
    mUser = user;
    mLocalUser = user;
    TheMemcardMgr.AddSink(this);
    SetState(kS_ManualLoadInit);
}
void SaveLoadManager::PrintoutSaveSizeInfo() {
    FixedSizeSaveable::EnablePrintouts(true);
    FormatString fmt("SAVESIZE\n");
    TheDebug << fmt.Str();
    unsigned int profileSize = BandProfile::SaveSize(0x97);
    int symbolSize = FixedSizeSaveableStream::GetSymbolTableSize(0x97);
    TheDebug << MakeString<int>("Symbol Table Size = %i\n", symbolSize);
    TheDebug << MakeString<int>("SAVESIZE TOTAL = %i \n", WiiProfileMgr::SaveSize(0x97) + (symbolSize + profileSize));
}

bool SaveLoadManager::IsReasonToUpload() {
    DataNode &var = DataVariable(saveload_skip_upload);
    int skipUpload = var.Int(NULL) != 0;
    bool isConnected = TheNet.mServer->IsConnected();
    bool needsUpload = TheProfileMgr.NeedsUpload();
    bool allUnlocked = TheProfileMgr.mAllUnlocked;
    return !skipUpload && !allUnlocked && isConnected && needsUpload;
}

void SaveLoadManager::StartSaveAction(bool b) {
    UpdateStatus(kSaveLoadMgrStatus_Saving);
    MILO_ASSERT(mState == kS_SaveOverwrite || mState == kS_SaveNoOverwrite, 0x9c9);
    for (BandProfile **p = (BandProfile **)mUploadProfiles.begin(); p != (BandProfile **)mUploadProfiles.end(); p++) {
        TheWiiProfileMgr.SetLocked(*p, true);
    }
    mWaiting = true;
    delete mAction;
    mAction = NULL;
    mAction = new SaveMemcardAction(&mUploadProfiles);
    TheMemcardMgr.AddSink(this);
    TheMemcardMgr.OnSaveGame(NULL, mAction, b);
}

DataNode SaveLoadManager::OnMsg(const DeviceChosenMsg &msg) {
    MILO_ASSERT(mWaiting, 0xa41);
    mWaiting = false;
    TheMemcardMgr.RemoveSink(this);
    switch (mState) {
    case kS_AutoloadSetDevice:
    case kS_AutoloadSelectDevice2:
    case kS_AutoloadSelectDevice3:
    case kS_AutoloadStartLoad2:
        unk78 = msg.Device();
        SetState(kS_AutoloadStartLoad);
        break;
    case kS_GlobalCreateMissing_Msg:
        SetState(kS_SaveNoOverwrite);
        break;
    case kS_ManualSaveNoDevice:
        unk78 = msg.Device();
        SetState(kS_SaveChooseDeviceInvalid);
        break;
    case kS_ManualSaveChooseDevice:
        SetState(kS_ManualLoadChooseDevice);
        break;
    case kS_Done:
    case kS_LoadComplete:
    case kS_Finish:
        break;
    default:
        MILO_FAIL(
            "Unhandled DeviceChosenMsg in state %d and mode %d\n", (int)mState, (int)mMode
        );
        break;
    }
    return DataNode(0);
}

DataNode SaveLoadManager::OnMsg(const NoDeviceChosenMsg &) {
    MILO_ASSERT(mWaiting, 0xa73);
    mWaiting = false;
    TheMemcardMgr.RemoveSink(this);
    switch (mState) {
    case kS_AutoloadSetDevice:
        SetState(kS_AutoloadNoSaveFound_Msg);
        break;
    case kS_AutoloadSelectDevice3:
        SetState(kS_AutoloadMultipleSavesFound);
        break;
    case kS_AutoloadStartLoad2:
        SetState(kS_AutoloadNotOwner);
        break;
    case kS_GlobalCreateMissing_Msg:
        SetState(kS_GlobalCreateNotFound_Msg);
        break;
    case kS_ManualSaveNoDevice:
        SetState(kS_ManualLoadNoDevice);
        break;
    case kS_ManualSaveChooseDevice:
        SetState(kS_GlobalOptionsMissing_Msg);
        break;
    case kS_Done:
    case kS_LoadComplete:
    case kS_Finish:
        break;
    default:
        MILO_FAIL(
            "Unhandled NoDeviceChosenMsg in state %d and mode %d\n",
            (int)mState,
            (int)mMode
        );
        break;
    }
    return DataNode(0);
}

DataNode SaveLoadManager::OnMsg(const MCResultMsg &msg) {
    MILO_ASSERT(mWaiting, 0xaa3);
    mWaiting = false;
    TheMemcardMgr.RemoveSink(this);
    MCResult res = (MCResult)msg.mData->Int(2);
    switch (mState) {
    case (State)0x4:
        unk6c = res;
        break;
    case kS_AutoloadStartLoad: {
        switch (res) {
        case kMCNoCard:
            SetState(kS_AutoloadNotOwner);
            break;
        case kMCCorrupt:
            SetState(kS_AutoloadCorrupt);
            break;
        case kMCNotOwner:
            SetState(kS_AutoloadObsolete);
            break;
        case kMCNotEnoughSpace:
        case kMCFileNotFound:
            SetState(kS_SaveOverwrite);
            break;
        case kMCObsoleteVersion:
            SetState(kS_AutoloadFuture);
            break;
        case kMCNewerVersion:
            SetState(kS_AutoloadFuture2);
            break;
        case kMCNoError:
            unk6c = res;
            SetState((State)0x43);
            break;
        default:
            SetState(kS_SaveFailed);
            break;
        }
        break;
    }
    case kS_SaveChooseDeviceInvalid: // 0x45
        switch (res) {
        case kMCNoCard:
            SetState(kS_GlobalCreateNotFound_Msg);
            break;
        case kMCNoError:
        case kMCFileExists:
        case kMCCorrupt:
        case kMCNotOwner:
            SetState(kS_SaveDeviceInvalid);
            break;
        case kMCFileNotFound:
        case kMCNotEnoughSpace:
            SetState(kS_SaveOverwrite);
            break;
        default:
            SetState(kS_GlobalCreateCorrupt);
            break;
        }
        break;
    case kS_SaveOverwrite: // 0x46
    case kS_SaveNoOverwrite: // 0x47
        unk6c = res;
        break;
    case kS_ManualLoadChooseDevice: // 0x64
        switch (res) {
        case kMCNoCard:
            SetState((State)0x63);
            break;
        case kMCFileNotFound:
            SetState((State)0x65);
            break;
        case kMCCorrupt:
            SetState((State)0x66);
            break;
        case kMCNotOwner:
            SetState((State)0x67);
            break;
        case kMCObsoleteVersion:
            SetState(kS_AutoloadFuture);
            break;
        case kMCNewerVersion:
            SetState(kS_AutoloadFuture2);
            break;
        case kMCNoError:
            unk6c = res;
            SetState((State)0x43);
            break;
        default:
            SetState(kS_SaveFailed);
            break;
        }
        break;
    case (State)0x6a:
        if (res == kMCNoError || res == kMCFileNotFound) {
            SetState((State)0x6b);
        } else {
            SetState((State)0x6c);
        }
        break;
    case kS_Done:
    case kS_LoadComplete:
    case kS_Finish:
        break;
    default:
        MILO_FAIL("Unhandled MCResultMsg in state %d and mode %d\n", (int)mState, (int)mMode);
        break;
    }
    return DataNode(0);
}

DataNode SaveLoadManager::OnMsg(const RockCentralOpCompleteMsg &) {
    MILO_ASSERT(mWaiting, 0xb55);
    mWaiting = false;
    if ((unsigned int)(mState - 0x6D) <= 2) {
        // Done/LoadComplete/Finish states - do nothing
    } else if (mState == (State)0x58) {
        SetState((State)0x57);
    } else {
        MILO_FAIL("Unhandled RockCentralOpCompleteMsg\n");
    }
    return DataNode(0);
}

DataNode SaveLoadManager::OnMsg(const SigninChangedMsg &) {
    switch (mState) {
    case kS_AutoloadNoSaveFound_Msg:
    case kS_AutoloadMultipleSavesFound:
    case kS_AutoloadNotOwner:
    case kS_AutoloadCorrupt:
    case kS_AutoloadObsolete:
    case kS_AutoloadFuture:
    case kS_AutoloadFuture2:
    case (State)0x17:
    case (State)0x18:
    case (State)0x1c:
    case (State)0x29:
    case (State)0x2a:
    case (State)0x2f:
    case (State)0x3a:
    case (State)0x42:
    case kS_SaveDeviceInvalid:
    case (State)0x49:
    case kS_SaveNotEnoughSpacePS3:
    case kS_GlobalCreateNotFound_Msg:
    case kS_GlobalCreateCorrupt:
    case (State)0x4f:
    case kS_SaveFailed:
    case kS_ManualLoadNoDevice:
    case kS_ManualLoadConfirm_Yes:
    case kS_ManualLoadConfirm:
    case kS_GlobalOptionsMissing_Msg:
    case (State)0x63:
    case (State)0x65:
    case (State)0x66:
    case (State)0x67:
        if (!mUser)
            break;
        if (ThePlatformMgr.HasUserSigninChanged(mUser)) {
            bool dismissed = false;
            if (TheUIEventMgr->HasActiveDialogEvent()) {
                if (TheUIEventMgr->CurrentDialogEvent() == saveload_dialog_event) {
                    dismissed = true;
                }
            }
            if (dismissed) {
                TheUIEventMgr->DismissDialogEvent();
            } else {
                int padNum = mUser ? mUser->GetPadNum() : -1;
                TheDebug.Notify(MakeString<int, State>(
                    "Expected active dialog event during signin change on pad %d while in state %d.\n",
                    padNum, mState
                ));
            }
            SetState(kS_LoadComplete);
        }
        break;
    case kS_AutoloadStartLoad:
    case kS_SaveOverwrite:
    case kS_SaveNoOverwrite:
    case kS_ManualLoadChooseDevice:
        SetState(kS_Done);
        break;
    default:
        if (!mUser)
            break;
        if (ThePlatformMgr.HasUserSigninChanged(mUser)) {
            int padNum = mUser ? mUser->GetPadNum() : -1;
            TheDebug.Notify(MakeString<int, State>(
                "Expected active dialog event during signin change on pad %d while in state %d.\n",
                padNum, mState
            ));
            SetState(kS_Done);
        }
        break;
    case kS_Idle:
    case kS_Done:
    case kS_LoadComplete:
    case kS_Finish:
        break;
    }
    return DataNode(0);
}

DataNode SaveLoadManager::OnMsg(const ProfileSwappedMsg &msg) {
    LocalUser *pUser1 = msg.GetUser1();
    MILO_ASSERT(pUser1, 0xbcc);
    MILO_ASSERT(pUser1->IsLocal(), 0xbcd);
    LocalBandUser *pLocalUser1 = BandUserMgr::GetLocalBandUser(pUser1);
    MILO_ASSERT(pLocalUser1, 0xbcf);
    LocalUser *pUser2 = msg.GetUser2();
    MILO_ASSERT(pUser2, 0xbd1);
    MILO_ASSERT(pUser2->IsLocal(), 0xbd2);
    LocalBandUser *pLocalUser2 = BandUserMgr::GetLocalBandUser(pUser2);
    MILO_ASSERT(pLocalUser2, 0xbd4);
    if (mUser != NULL) {
        if (mUser == pLocalUser1) mUser = pLocalUser2;
        else if (mUser == pLocalUser2) mUser = pLocalUser1;
    }
    if (mLocalUser != NULL) {
        if (mLocalUser == pUser1) mLocalUser = pUser2;
        else if (mLocalUser == pUser2) mLocalUser = pUser1;
    }
    return DataNode(1);
}

void SaveLoadManager::HandleEventResponse(LocalUser *localUser, int choiceIdx) {
    State start = mStateAtSelectStart;
    State state = mState;
    mStateAtSelectStart = kS_Idle;
    if (start != state) {
        MILO_WARN(
            "States changed between UIComponentSelectMsg (%d) and UIComponentSelectDoneMsg (%d).\n",
            start,
            state
        );
        return;
    }
    if ((unsigned int)(choiceIdx - 1) > 2U) {
        MILO_FAIL("Bad choice index %i\n", choiceIdx);
        return;
    }
    mLocalUser = localUser;
    int isFirst = (choiceIdx == 1);
    switch (mState) {
    case kS_AutoloadNoSaveFound_Msg: // 0x6
        if (choiceIdx == 1) {
            if (unk7c == 2) {
                SetState(kS_AutoloadSelectDevice2);
            } else {
                SetState(kS_AutoloadSetDevice);
            }
        } else {
            SetState((State)0x42);
        }
        break;
    case (State)0x7:
        SetState(isFirst ? kS_AutoloadSelectDevice3 : (State)0x42);
        break;
    case kS_AutoloadNotOwner: // 0xc
        SetState(isFirst ? kS_AutoloadStartLoad2 : (State)0x42);
        break;
    case kS_SaveChooseDevice: // 0x4b
        SetState(isFirst ? kS_GlobalCreateMissing_Msg : (State)0x42);
        break;
    case kS_AutoloadCorrupt: // 0xe
    case kS_AutoloadObsolete: // 0xf
    case kS_AutoloadFuture: // 0x10
    case kS_AutoloadFuture2: // 0x11
    case kS_SaveNoOverwrite: // 0x47
        SetState(isFirst ? kS_SaveOverwrite : (State)0x42);
        break;
    case (State)0x17:
    case (State)0x18:
        SetState(isFirst ? (State)0x19 : (State)0x24);
        break;
    case (State)0x1c:
        SetState(isFirst ? (State)0x1d : (State)0x24);
        break;
    case (State)0x29:
    case (State)0x2a:
        SetState(isFirst ? (State)0x2b : (State)0x36);
        break;
    case (State)0x2f:
        SetState(isFirst ? (State)0x30 : (State)0x36);
        break;
    case (State)0x39:
        SetState(isFirst ? (State)0x3b : (State)0x40);
        break;
    case kS_GlobalCreateNotFound_Msg: // 0x4d
    case kS_GlobalCreateMissing_Msg: // 0x4e
    case (State)0x4f:
    case (State)0x63:
    case kS_ManualLoadChooseDevice: // 0x64
    case (State)0x65:
        SetState((State)0x42);
        break;
    case (State)0x41:
    case kS_SaveDeviceInvalid: // 0x48
        SaveLoadErrorSetState();
        break;
    case kS_ManualLoadInit: // 0x5a
        SetState(isFirst ? kS_ManualSaveNoDevice : (State)0x42);
        break;
    case kS_ManualLoadStartLoad: // 0x5d
    case kS_ManualLoadConfirmUnsaved: // 0x5e
        if (choiceIdx == 1) {
            SetState(kS_ManualLoadChooseDevice);
        } else {
            SetState((State)0x44);
        }
        break;
    case kS_ManualLoadConfirm: // 0x60
        SetState(isFirst ? kS_ManualSaveChooseDevice : (State)0x42);
        break;
    case (State)0x61:
        SetState((State)0x42);
        break;
    default:
    case (State)0x66:
    case (State)0x67:
        MILO_FAIL(
            "Unhandled UIComponentSelectDoneMsg from choice index %i in state %d and mode %d\n",
            (int)choiceIdx, (int)mState, (int)mMode
        );
        break;
    }
}

#pragma push
#pragma dont_inline on
BEGIN_HANDLERS(SaveLoadManager)
    HANDLE_ACTION(autosave, AutoSave())
    HANDLE_ACTION(autoload, AutoLoad())
    HANDLE_ACTION(delete_saves, ManualDelete())
    HANDLE_ACTION(manual_save, ManualSave(_msg->Obj<LocalBandUser>(2)))
    HANDLE_EXPR(is_autosave_enabled, IsAutosaveEnabled(_msg->Obj<LocalBandUser>(2)))
    HANDLE_ACTION(enable_autosave, EnableAutosave(_msg->Obj<LocalBandUser>(2)))
    HANDLE_ACTION(disable_autosave, DisableAutosave(_msg->Obj<LocalBandUser>(2)))
    HANDLE_ACTION(handle_eventresponse_start, HandleEventResponseStart(_msg->Int(2)))
    HANDLE_ACTION(
        handle_eventresponse, HandleEventResponse(_msg->Obj<LocalUser>(2), _msg->Int(3))
    )
    HANDLE_EXPR(get_dialog_msg, GetDialogMsg())
    HANDLE_EXPR(get_dialog_opt1, GetDialogOpt1())
    HANDLE_EXPR(get_dialog_opt2, GetDialogOpt2())
    HANDLE_EXPR(get_dialog_opt3, GetDialogOpt3())
    HANDLE_EXPR(get_dialog_focus_option, GetDialogFocusOption())
    HANDLE_EXPR(is_initial_load_done, IsInitialLoadDone())
    HANDLE_EXPR(is_idle, IsIdle())
    HANDLE_ACTION(activate, Activate())
    HANDLE_ACTION(printout_savesize_info, PrintoutSaveSizeInfo())
    HANDLE_MESSAGE(ProfileSwappedMsg)
    HANDLE_MESSAGE(DeviceChosenMsg)
    HANDLE_MESSAGE(NoDeviceChosenMsg)
    HANDLE_MESSAGE(MCResultMsg)
    HANDLE_MESSAGE(RockCentralOpCompleteMsg)
    HANDLE_MESSAGE(SigninChangedMsg)
    HANDLE_SUPERCLASS(MsgSource)
    HANDLE_CHECK(0xF27)
END_HANDLERS
#pragma pop