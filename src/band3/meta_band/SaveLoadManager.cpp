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
    LoadMemcardAction(BandProfile *);
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
      mProfile(NULL), unk44(), unk48(0), mSaveSize(0), unk58(0), mCacheID(NULL), mCache(NULL),
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
    if ((unsigned int)_ref0 > 0x6b) return;
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
            SetState((State)0x4b);
            break;
        case kMode_ManualLoad:
            SetState((State)0x51);
            break;
        default:
            MILO_NOTIFY("SaveLoadManager startup bad mode: %d\n", (SaveLoadMode)mMode);
            SetState((State)0x6a);
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
                MILO_NOTIFY("SaveLoadManager - CacheMgr search returned error %d\n", (int)result);
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
                MILO_FAIL("SaveLoadManager - CacheMgr choose returned error %d\n", (int)result);
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
                MILO_FAIL("SaveLoadManager - kS_SongCacheCreateMountRead unhandled error %d\n", (int)result);
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
                MILO_FAIL("SaveLoadManager - kS_SongCacheCreateMountWrite unhandled error %d\n", (int)result);
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
            MILO_FAIL("Impossible state.\n");
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
                MILO_FAIL("SaveLoadManager - CacheMgr search returned error %d\n", (int)result);
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
                MILO_FAIL("SaveLoadManager - CacheMgr choose returned error %d\n", (int)result);
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
                MILO_NOTIFY("SaveLoadManager - unknown error %d during state %d.\n", (int)result, (int)_ref0);
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
                MILO_NOTIFY("SaveLoadManager - unknown error %d during state %d.\n", (int)result, (int)_ref0);
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
                MILO_NOTIFY("SaveLoadManager - CacheMgr choose returned error %d\n", (int)result);
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
                MILO_FAIL("SaveLoadManager - CacheMgr choose returned error %d\n", (int)result);
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
            if (_ref0 == (State)0x69) {
                SetState((State)0x6a);
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
    if (mState == newState) return;

    // Retail holds the dialog event Symbol in a FUNCTION-LOCAL static (guard bit 0
    // of the word at 0x82DFDA28, storage 0x82DFDA24, string "saveload_dialog_event"
    // @0x820953A4). It shadows the file-scope extern of the same name.
    static Symbol saveload_dialog_event("saveload_dialog_event");

    bool wasIdle = false;
    // Exit-state cleanup based on which state we're leaving. Retail RE-READS mState
    // here (lwz r11,0x20(r30) after the static init) rather than caching it in a
    // callee-saved reg -- caching it costs a whole-function regalloc cascade.
    switch ((int)mState) {
    case 0x0:
        wasIdle = true;
        break;
    case 0x1f:
    case 0x21:
    case 0x32:
    case 0x33:
    case 0x3e:
        if (newState != (State)0x6b) {
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
        if (newState != (State)0x69) {
            delete mAction;
            mAction = NULL;
        }
        break;
    case 0x6b:
        if (mData != NULL) {
            _MemFree(mData);
            mData = NULL;
        }
        break;
    case 0x69:
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
        // Retail (Ghidra TU5 0x82550880 case 3): no vector churn, no IsDisableWriting.
        mProfile = GetNewSigninProfile();
        if (mProfile == NULL) {
            mUser = NULL;
            SetState((State)0x12);
        } else {
            mUser = mProfile->GetLocalBandUser();
            SetState((State)0x4);
        }
        break;
    case 0x4:
        mWaiting = true;
        TheMemcardMgr.OnSearchForDevice(GetProfile());
        break;
    case 0x5:
        if (unk7c == 2) {
            SetState((State)0x9);
        } else {
            SetState((State)0x6);
        }
        break;
    // Retail (Ghidra TU5) has 8 = SelectDevice and 9 = SetDevice -- our port had
    // them swapped. Retail also passes SelectDevice's bool SECOND
    // (profile, bool, sink, padNum) and never calls AddSink here.
    case 0x8: // kS_AutoloadSelectDevice2
    {
        BandProfile *pProfile = GetProfile();
        int devId = -1;
        if (mLocalUser != NULL) devId = mLocalUser->GetPadNum();
        mWaiting = true;
        TheMemcardMgr.SelectDevice(pProfile, false, this, devId);
        break;
    }
    case 0x9: // kS_AutoloadSetDevice
        MILO_ASSERT(unk7c == 2, 0x559);
        TheMemcardMgr.SetDevice(unk78);
        SetState((State)0xb);
        break;
    case 0xa: // kS_AutoloadSelectDevice3
    case 0xd:
    case 0x4d:
    {
        BandProfile *pProfile = GetProfile();
        int devId = -1;
        if (mLocalUser != NULL) devId = mLocalUser->GetPadNum();
        mWaiting = true;
        TheMemcardMgr.SelectDevice(pProfile, true, this, devId);
        break;
    }
    case 0xb: // kS_AutoloadStartLoad
    {
        BandProfile *pProfile = GetProfile();
        mWaiting = true;
        delete mAction;
        mAction = NULL;
        mAction = new LoadMemcardAction(pProfile);
        pProfile->PreLoad();
        TheMemcardMgr.OnLoadGame(pProfile, mAction);
        break;
    }
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
        SetState((State)0x6a);
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
            MILO_NOTIFY("SaveLoadManager - CacheMgr search failed in CreateSongCacheID()\n");
        }
        if (!TheCacheMgr->SearchAsync(unk4c.c_str(), &mCacheID)) {
#pragma dont_inline on
            MILO_FAIL("TheCacheMgr->SearchAsync() failed with CacheResult %d\n", (int)TheCacheMgr->GetLastResult());
#pragma dont_inline reset
        }
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
        static Symbol song_info_cache_name("song_info_cache_name");
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
            MILO_FAIL("TheCacheMgr->MountAsync failed with CacheResult %d\n", (int)TheCacheMgr->GetLastResult());
#pragma dont_inline reset
        }
        break;
    }
    case 0x20:
    {
        UpdateStatus((SaveLoadMgrStatus)1);
        if (!TheCacheMgr->MountAsync(mCacheID, &mCache, NULL)) {
#pragma dont_inline on
            MILO_FAIL("TheCacheMgr->MountAsync failed with CacheResult %d\n", (int)TheCacheMgr->GetLastResult());
#pragma dont_inline reset
        }
        break;
    }
    case 0x1d:
    {
        UpdateStatus((SaveLoadMgrStatus)1);
        if (!TheCacheMgr->DeleteAsync(mCacheID)) {
#pragma dont_inline on
            MILO_FAIL("TheCacheMgr->DeleteAsync failed with CacheResult %d\n", (int)TheCacheMgr->GetLastResult());
#pragma dont_inline reset
        }
        break;
    }
    case 0x1f:
    {
        mData = (_MemAllocTemp)(mSaveSize, 0);
        if (!mCache->ReadAsync(unk4c.c_str(), mData, (unsigned int)mSaveSize, NULL)) {
#pragma dont_inline on
            MILO_FAIL("mCache->ReadAsync failed with CacheResult %d\n", (int)TheCacheMgr->GetLastResult());
#pragma dont_inline reset
        }
        break;
    }
    case 0x1e:
    {
        if (!mCache->GetFileSizeAsync(unk4c.c_str(), (unsigned int *)&mSaveSize, NULL)) {
#pragma dont_inline on
            MILO_FAIL("mCache->GetFileSizeAsync failed with CacheResult %d\n", (int)TheCacheMgr->GetLastResult());
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
                MILO_FAIL("mCache->WriteAsync failed with CacheResult %d\n", (int)TheCacheMgr->GetLastResult());
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
            MILO_FAIL("TheCacheMgr->UnmountAsync failed with CacheResult %d\n", (int)TheCacheMgr->GetLastResult());
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
            MILO_FAIL("TheCacheMgr->SearchAsync failed with CacheResult %d\n", (int)TheCacheMgr->GetLastResult());
#pragma dont_inline reset
        }
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
        // Retail order: static-init, THEN GetGlobalOptionsSize, THEN Localize.
        static Symbol global_options_cache_name("global_options_cache_name");
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
        static Symbol global_options_cache_name("global_options_cache_name");
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
        // Retail order: static-init, THEN GetGlobalOptionsSize, THEN Localize.
        static Symbol global_options_cache_name("global_options_cache_name");
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
            MILO_FAIL("TheCacheMgr->MountAsync failed with CacheResult %d\n", (int)TheCacheMgr->GetLastResult());
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
            MILO_FAIL("TheCacheMgr->MountAsync failed with CacheResult %d\n", (int)TheCacheMgr->GetLastResult());
#pragma dont_inline reset
        }
        break;
    }
    case 0x30:
    {
        UpdateStatus((SaveLoadMgrStatus)1);
        if (!TheCacheMgr->DeleteAsync(mCacheID)) {
#pragma dont_inline on
            MILO_FAIL("TheCacheMgr->DeleteAsync failed with CacheResult %d\n", (int)TheCacheMgr->GetLastResult());
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
            MILO_FAIL("TheCacheMgr->ReadAsync failed with CacheResult %d\n", (int)TheCacheMgr->GetLastResult());
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
            MILO_FAIL("mCache->WriteAsync failed with CacheResult %d\n", (int)TheCacheMgr->GetLastResult());
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
                MILO_NOTIFY("UnmountAsync failed with error %d\n", TheCacheMgr->GetLastResult());
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
        unk7c = 0;
        TheMemcardMgr.SaveLoadProfileComplete(GetProfile(), 2);
        // fall through into the shared dialog-event block
    case 0x6: // kS_AutoloadNoSaveFound_Msg
    case 0x7: // kS_AutoloadMultipleSavesFound
    case 0xc: // kS_AutoloadNotOwner
    case 0xe:
    case 0xf:
    case 0x10: // kS_AutoloadDeviceMissing
    case 0x11:
    case 0x17:
    case 0x18:
    case 0x1c:
    case 0x29:
    case 0x2a:
    case 0x2f:
    case 0x3a:
    case 0x48:
    case 0x49:
    case 0x4a:
    case 0x4c:
    case 0x4e:
    case 0x4f:
    case 0x50:
    case 0x5c:
    case 0x5f:
    case 0x60:
    case 0x62:
    case 0x63:
    case 0x65:
    case 0x66:
    case 0x67:
        TheUIEventMgr->TriggerEvent(saveload_dialog_event, NULL);
        break;
    case 0x43:
    case 0x44:
    {
        unk7c = 0;
        int saveResult = (mState == (State)0x43) ? 1 : -1;
        TheMemcardMgr.SaveLoadProfileComplete(GetProfile(), saveResult);
        if (mMode == kMode_AutoLoad) {
            SetState((State)0x3);
        } else if (mMode == kMode_AutoSave) {
            SetState((State)0x54);
        }
        break;
    }
    case 0x45:
        mWaiting = true;
        TheMemcardMgr.OnCheckForSaveContainer(GetProfile());
        break;
    case 0x46:
        StartSaveAction(true);
        break;
    case 0x47:
        StartSaveAction(false);
        break;
    case 0x4b: // kS_ManualDeleteStart (retail numbering)
        mWaiting = true;
        TheMemcardMgr.OnDeleteSaves(GetProfile());
        break;
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
        // Retail (Ghidra TU5 case 0x54): mirrors case 3 but with the autosavable
        // profile, then branches on a MemcardMgr predicate (fn_827ABB60).
        mProfile = GetAutosavableProfile();
        if (mProfile == NULL) {
            mUser = NULL;
            SetState((State)0x55);
        } else {
            mUser = mProfile->GetLocalBandUser();
            if (TheMemcardMgr.IsStorageDeviceValid(mProfile)) {
                SetState((State)0x46);
            } else {
                SetState((State)0x4c);
            }
        }
        break;
    case 0x55:
        SetState((State)0x6a);
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
        SetState((State)0x5b);
        break;
    case 0x5b:
    case 0x61:
    {
        // Retail: same as case 8 but with NO null-check on mLocalUser.
        BandProfile *pProfile = GetProfile();
        mWaiting = true;
        TheMemcardMgr.SelectDevice(pProfile, false, this, mLocalUser->GetPadNum());
        break;
    }
    case 0x5d:
        SetState((State)0x6a);
        break;
    case 0x5e:
    {
        int padNum = 0;
        if (mUser != NULL) padNum = mUser->GetPadNum();
        if (TheProfileMgr.HasUnsavedDataForPad(padNum)) {
            SetState((State)0x5f);
        } else {
            SetState((State)0x60);
        }
        break;
    }
    case 0x64:
    {
        BandProfile *pProfile = GetProfile();
        mWaiting = true;
        delete mAction;
        mAction = NULL;
        mAction = new LoadMemcardAction(pProfile);
        pProfile->PreLoad();
        TheMemcardMgr.OnLoadGame(pProfile, mAction);
        break;
    }
    case 0x68:
        SetState((State)0x6a);
        break;
    // State 0x69 has no entry body in target (async-wait, polled).
    case 0x6a:
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
    static Symbol s1("mc_button_create_data");
    static Symbol s2("mc_button_choose_device");
    static Symbol s3("mc_button_continue");
    static Symbol s4("mc_button_overwrite");
    static Symbol s5("song_info_cache_button_create");
    static Symbol s6("song_info_cache_button_corrupt_overwrite");
    static Symbol s7("global_options_button_create");
    static Symbol s8("global_options_button_corrupt_overwrite");
    static Symbol s9("mc_button_delete_saves");
    static Symbol s10("upload_button_view_first");
    static Symbol s11("upload_button_return");
    static Symbol s12("upload_button_view_prev");
    static Symbol s13("mc_button_yes");
    Symbol sym(gNullStr);
    switch (mState) {
    case (State)0x6:
        sym = s1;
        break;
    case (State)0x7:
    case (State)0xC:
    case (State)0x4C:
    case (State)0x5C:
    case (State)0x62:
        sym = s2;
        break;
    case (State)0x5F:
        sym = s3;
        break;
    case (State)0xE:
    case (State)0xF:
    case (State)0x10:
    case (State)0x11:
    case (State)0x48:
        sym = s4;
        break;
    case (State)0x17:
    case (State)0x18:
        sym = s5;
        break;
    case (State)0x1C:
        sym = s6;
        break;
    case (State)0x29:
    case (State)0x2A:
    case (State)0x3A:
        sym = s7;
        break;
    case (State)0x2F:
        sym = s8;
        break;
    case (State)0x4A:
        sym = s9;
        break;
    case (State)0x60:
        sym = s13;
        break;
    default:
        break;
    }
    return sym;
}

Symbol SaveLoadManager::GetDialogOpt2() {
    static Symbol s1("mc_button_cancel");
    static Symbol s2("mc_button_continue_no_save");
    static Symbol s3("song_info_cache_button_cancel");
    static Symbol s4("global_options_button_cancel");
    static Symbol s5("mc_button_retry");
    static Symbol s6("mc_button_disable_autosave");
    static Symbol s7("upload_button_return");
    static Symbol s8("upload_button_view_next");
    static Symbol s9("mc_button_no");
    Symbol sym(gNullStr);
    switch (mState) {
    case (State)0x6:
    case (State)0x7:
    case (State)0xC:
    case (State)0x48:
    case (State)0x5C:
    case (State)0x5F:
    case (State)0x62:
        sym = s1;
        break;
    case (State)0xE:
    case (State)0xF:
    case (State)0x10:
    case (State)0x11:
        sym = s2;
        break;
    case (State)0x17:
    case (State)0x18:
    case (State)0x1C:
        sym = s3;
        break;
    case (State)0x29:
    case (State)0x2A:
    case (State)0x2F:
    case (State)0x3A:
        sym = s4;
        break;
    case (State)0x4A:
        sym = s5;
        break;
    case (State)0x4C:
        sym = s6;
        break;
    case (State)0x60:
        sym = s9;
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
    case (State)0x6: {
        static Symbol s1("mc_auto_load_no_save_found_fmt");
        return DataArrayPtr(s1, DataArrayPtr(), profileName, playerNum);
    }
    case (State)0x7: {
        static Symbol s2("mc_auto_load_multiple_saves_found_fmt");
        return DataArrayPtr(s2, DataArrayPtr(), profileName, playerNum);
    }
    case (State)0xC: {
        static Symbol s3("mc_load_device_missing_fmt");
        return DataArrayPtr(s3, DataArrayPtr(), profileName, playerNum);
    }
    case (State)0xE: {
        static Symbol s4("mc_auto_load_corrupt");
        return DataArrayPtr(s4, DataArrayPtr(), GetProfile()->GetName());
    }
    case (State)0xF: {
        static Symbol s5("mc_auto_load_not_owner");
        return DataArrayPtr(s5, DataArrayPtr());
    }
    case (State)0x10:
        if (playerNum != -1) {
            static Symbol s6("mc_auto_load_obsolete_version_fmt");
            return DataArrayPtr(s6, DataArrayPtr(), profileName, playerNum);
        } else {
            static Symbol s7("mc_auto_load_obsolete_version");
            return DataArrayPtr(s7, DataArrayPtr());
        }
    case (State)0x11:
        if (playerNum != -1) {
            static Symbol s8("mc_auto_load_newer_version_fmt");
            return DataArrayPtr(s8, DataArrayPtr(), profileName, playerNum);
        } else {
            static Symbol s9("mc_auto_load_newer_version");
            return DataArrayPtr(s9, DataArrayPtr());
        }
    case (State)0x17: {
        static Symbol s10("song_info_cache_create");
        return DataArrayPtr(s10, DataArrayPtr());
    }
    case (State)0x18: {
        static Symbol s11("song_info_cache_missing");
        return DataArrayPtr(s11, DataArrayPtr());
    }
    case (State)0x1C: {
        static Symbol s12("song_info_cache_corrupt");
        return DataArrayPtr(s12, DataArrayPtr());
    }
    case (State)0x29: {
        static Symbol s13("global_options_create");
        return DataArrayPtr(s13, DataArrayPtr());
    }
    case (State)0x2A:
    case (State)0x3A: {
        static Symbol s14("global_options_missing");
        return DataArrayPtr(s14, DataArrayPtr());
    }
    case (State)0x2F: {
        static Symbol s15("global_options_corrupt");
        return DataArrayPtr(s15, DataArrayPtr());
    }
    case (State)0x42: {
        static Symbol s16("mc_autosave_disabled");
        return DataArrayPtr(s16, DataArrayPtr());
    }
    case (State)0x48: {
        static Symbol s17("mc_save_confirm_overwrite");
        return DataArrayPtr(s17, DataArrayPtr());
    }
    case (State)0x49: {
        static Symbol s18("mc_save_not_enough_space");
        return DataArrayPtr(s18, DataArrayPtr());
    }
    case (State)0x4A: {
        static Symbol s19("mc_save_not_enough_space");
        return DataArrayPtr(s19, DataArrayPtr(), -TheMemcardMgr.GetSizeNeeded());
    }
    case (State)0x4C: {
        static Symbol s20("mc_save_device_missing_fmt");
        return DataArrayPtr(s20, DataArrayPtr(), profileName, playerNum);
    }
    case (State)0x4E: {
        static Symbol s21("mc_save_failed");
        return DataArrayPtr(s21, DataArrayPtr());
    }
    case (State)0x4F: {
        static Symbol s22("mc_save_disabled_by_cheat");
        return DataArrayPtr(s22, DataArrayPtr());
    }
    case (State)0x50: {
        static Symbol s23("mc_load_failed");
        return DataArrayPtr(s23, DataArrayPtr());
    }
    case (State)0x5C: {
        static Symbol s24("mc_manual_save_no_selection");
        return DataArrayPtr(s24, DataArrayPtr());
    }
    case (State)0x5F:
        if (playerNum != -1) {
            static Symbol s25("mc_manual_load_confirm_unsaved_fmt");
            return DataArrayPtr(s25, DataArrayPtr(), profileName, playerNum);
        } else {
            static Symbol s26("mc_manual_load_confirm_unsaved");
            return DataArrayPtr(s26, DataArrayPtr());
        }
    case (State)0x60: {
        static Symbol s27("mc_manual_load_confirm");
        return DataArrayPtr(s27, DataArrayPtr());
    }
    case (State)0x62: {
        static Symbol s28("mc_manual_load_no_selection");
        return DataArrayPtr(s28, DataArrayPtr());
    }
    case (State)0x63: {
        static Symbol s29("mc_manual_load_storage_missing");
        return DataArrayPtr(s29, DataArrayPtr());
    }
    case (State)0x65: {
        static Symbol s30("mc_manual_load_no_file");
        return DataArrayPtr(s30, DataArrayPtr());
    }
    case (State)0x66: {
        static Symbol s31("mc_manual_load_corrupt");
        return DataArrayPtr(s31, DataArrayPtr());
    }
    case (State)0x67: {
        static Symbol s32("mc_manual_load_not_owner");
        return DataArrayPtr(s32, DataArrayPtr());
    }
    default:
        MILO_ASSERT(false, 0xE00);
        return DataNode(0);
    }
}
Symbol SaveLoadManager::GetDialogOpt3() {
    static Symbol s1("mc_button_continue_no_save");
    Symbol sym(gNullStr);
    if (mState == (State)0x4A) {
        sym = s1;
    }
    return sym;
}

BandProfile *SaveLoadManager::GetProfile() {
    // Retail fn_8254C0B0: vbase-adjusts mUser, virtual GetPadNum(), then a
    // pad-indexed profile lookup on TheProfileMgr (fn_82545E90).
    return TheProfileMgr.GetProfileForPad(mUser->GetPadNum());
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
    case (State)0x4b:
        if (res == kMCNoError || res == kMCFileNotFound) {
            SetState((State)0x4c);
        } else {
            SetState((State)0x4a);
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
    if ((unsigned int)(mState - 0x69) <= 2) {
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
                MILO_NOTIFY(
                    "Expected active dialog event during signin change on pad %d while in state %d.\n",
                    padNum, mState
                );
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
            MILO_NOTIFY(
                "Expected active dialog event during signin change on pad %d while in state %d.\n",
                padNum, mState
            );
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