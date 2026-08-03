#pragma once
#include "SaveLoadManager.h"
#include "game/BandUser.h"
#include "game/GameMic.h"
#include "meta_band/GameplayOptions.h"
#include "meta_band/ProfileMessages.h"
#include "net/Server.h"
#include "obj/Msg.h"
#include "os/Joypad.h"
#include "meta/Profile.h"
#include "meta_band/BandProfile.h"
#include "net_band/DataResults.h"
#include "os/User.h"

enum LagContext {
    kGame = 0,
    kVCal = 1,
    kACal = 2,
    kPractice90 = 3,
    kPractice80 = 4,
    kPractice70 = 5,
    kPractice60 = 6,
    kNumLagContexts = 7
};

class ProfileMgr : public MsgSource {
public:
    ProfileMgr();
    virtual DataNode Handle(DataArray *, bool);
    virtual ~ProfileMgr() {}
    virtual void SetMicVol(int, int);
    virtual int GetMicVol(int) const;

    int GetSliderStepCount() const;
    BandProfile *GetProfileForUser(const LocalUser *);
    // Retail SaveLoadManager::GetProfile (fn_8254C0B0) resolves the profile by
    // PAD NUM, not by user pointer: TheProfileMgr.<fn_82545E90>(mUser->GetPadNum()).
    // Decl-only (the definition lives in an unported TU).
    BandProfile *GetProfileForPad(int);
    void SetCymbalConfiguration(unsigned int);
    void UpdatePrimaryProfile();
    bool GetAllUnlocked();
    int GetMaxCharacters() const;
    bool UnlockAllSongs();
    void RelockSongs();
    void SetGlobalOptionsSaveState(ProfileSaveState);
    bool GlobalOptionsNeedsSave();
    int GetGlobalOptionsSize();
    void SaveGlobalOptions(FixedSizeSaveableStream &);
    void LoadGlobalOptions(FixedSizeSaveableStream &);
    void PushAllOptions();
    void InitSliders();
    float SliderIxToDb(int) const;
    void GetMicGainInfo(const Symbol &, float &, float &, float &, float &) const;
    void SetBackgroundVolume(int);
    void SetForegroundVolume(int);
    void SetFxVolume(int);
    int GetFxVolume() const;
    void SetCrowdVolume(int);
    void SetVocalCueVolume(int);
    void SetVoiceChatVolume(int);
    void SetBassBoost(bool);
    void SetDolby(bool);
    void SetOverscan(bool);
    void SetSynapseEnabled(bool);
    // retail X360 strips the WiiSpeak feature (setters absent)
    bool GetHasSeenFirstTimeCalibration() const;
    void SetHasSeenFirstTimeCalibration(bool);
    bool GetHasConnectedProGuitar() const;
    void SetHasConnectedProGuitar(bool);
    // retail X360 strips the Wii-friends feature
    bool GetSecondPedalHiHat() const;
    void SetSecondPedalHiHat(bool);
    void SetSyncPresetIx(int);
    float GetSongToTaskMgrMsRaw() const;
    void SetSongToTaskMgrMsRaw(float);
    float GetJoypadExtraLagInits(JoypadType, LagContext) const;
    float GetJoypadExtraLag(JoypadType, LagContext) const;
    void SetJoypadExtraLag(JoypadType, LagContext, float);
    float GetPadExtraLag(int, LagContext) const;
    void SetPlatformAudioLatency(float);
    void SetPlatformVideoLatency(float);
    float GetInGameExtraVideoLatency() const;
    void SetInGameExtraVideoLatency(float);
    float GetInGameSyncOffsetAdjustment() const;
    void SetInGameSyncOffsetAdjustment(float);
    float GetSyncOffset(int) const;
    float GetSyncOffsetRaw() const;
    void SetSyncOffsetRaw(float);
    float GetExcessVideoLag() const;
    void SetExcessVideoLag(float);
    void SetExcessAudioLag(float);
    float GetPlatformAudioLatency() const;
    float GetPlatformVideoLatency() const;
    float GetExcessAudioLagNeutral(int, bool) const;
    float GetExcessVideoLagNeutral(int, bool) const;
    float GetExcessAudioLag() const;
    float GetBackgroundVolumeDb() const;
    int GetBackgroundVolume() const { return mBackgroundVolume; }
    float GetForegroundVolumeDb() const;
    int GetForegroundVolume() const { return mForegroundVolume; }
    float GetFxVolumeDb() const;
    float GetCrowdVolumeDb();
    int GetCrowdVolume() const { return mCrowdVolume; }
    float GetVocalCueVolumeDb();
    int GetVocalCueVolume() const { return mVocalCueVolume; }
    float GetVoiceChatVolumeDb();
    int GetVoiceChatVolume() const { return mVoiceChatVolume; }
    unsigned int GetCymbalConfiguration() const;
    bool HasLoaded();
    float GetSongToTaskMgrMs(LagContext) const;
    BandProfile *FindTourProgressOwner(const TourProgress *);
    BandProfile *GetProfileForChar(const TourCharLocal *);
    std::vector<BandProfile *> GetSignedInProfiles();
    BandProfile *GetPrimaryProfile() const;
    BandProfile *GetProfileFromPad(int);
    void SetPrimaryProfileByUser(const LocalUser *);
    GameplayOptions *GetGameplayOptionsFromUser(LocalBandUser *);
    void Poll();
    std::vector<BandProfile *> GetParticipatingProfiles();
    bool IsPrimaryProfileCritical(const LocalUser *);
    void Init();
    std::vector<BandProfile *> GetNewlySignedInProfiles();
    std::vector<BandProfile *> GetShouldAutosaveProfiles();
    bool IsAutosaveEnabled(const LocalBandUser *);
    BandProfile *GetProfileForChar(const BandCharDesc *);
    bool HasUnsavedDataForPad(int);
    void PurgeOldData();
    BandProfile *FindCharOwnerFromGuid(const HxGuid &);
    void HandlePendingGamerpicRewards();
    void CheckProfileWebLinkStatus();
    int GetFirstTimeInstrumentFlag(JoypadType) const;
    bool GetHasSeenFirstTimeInstruments(const LocalUser *) const;
    void SetHasSeenFirstTimeInstruments(const LocalUser *, bool);
    void CheckProfileWebSetlistStatus();
    void HandlePendingProfileUploads();
    void SyncProfileSetlists();
    bool NeedsUpload();
    void UpdateAllMicLevels();
    void UpdateMicLevels(int);
    void UpdateMultiMicDeviceSliders(Mic *, int);
    void ForceMicGain(int, float);
    void ForceMicOutputGain(int, float);
    // retail X360 strips the Wii roster-count accessors (TheWiiProfileMgr)
    bool ChooseNewPrimaryProfile();
    void SetPrimaryProfile(BandProfile *);
    bool CanChangePrimaryProfile() const;
    bool HasPrimaryProfile() const;
    void HandleProfileLoadComplete();
    void HandleProfileSaveComplete();
    void FakeProfileFill();

    bool GetBassBoost() const { return mBassBoost; }
    bool GetDolby() const { return mDolby; }
    bool GetOverscan() const { return mOverscan; }
    bool GetSynapseEnabled() const { return mSynapseEnabled; }
    int GetSyncPresetIx() const { return mSyncPresetIx; }

    DataNode OnMsg(const SaveLoadMgrStatusUpdateMsg &);
    DataNode OnMsg(const UserLoginMsg &);
    DataNode OnMsg(const ServerStatusChangedMsg &);
    DataNode OnMsg(const GameMicsChangedMsg &);
    DataNode OnMsg(const SigninChangedMsg &);
    DataNode OnMsg(const ProfileChangedMsg &);

    DECLARE_REVS;

    float mPlatformAudioLatency; // 0x1c
    float mPlatformVideoLatency; // 0x20
    float mInGameExtraVideoLatency; // 0x24
    float mInGameSyncOffsetAdjustment; // 0x28
    // Retail X360: the inline array is heap-allocated behind a float** pointer.
    // Each mJoypadExtraLagOffsets[type] is a separately allocated float[kNumLagContexts].
    // Layout verified from ctor (fn_82534980) and GetPadExtraLag (fn_825323D0).
    float **mJoypadExtraLagOffsets; // 0x2c
    int unk30; // 0x30
    ProfileSaveState mGlobalOptionsSaveState; // 0x34
    bool mGlobalOptionsDirty; // 0x38
    int mBackgroundVolume; // 0x3c
    int mForegroundVolume; // 0x40
    int mFxVolume; // 0x44
    int mCrowdVolume; // 0x48
    int mVocalCueVolume; // 0x4c
    int mVoiceChatVolume; // 0x50
    bool mHasSeenFirstTimeCalibration; // 0x54
    bool mHasConnectedProGuitar; // 0x55
    float mSyncOffset; // 0x58
    float mSongToTaskMgrMs; // 0x5c
    bool mBassBoost; // 0x60
    bool mDolby; // 0x61
    bool unk582; // 0x62
    int mSyncPresetIx; // 0x64
    bool mOverscan; // 0x68
    bool mSynapseEnabled; // 0x69
    bool unk58a; // 0x6a
    // TU5 (2026-07-16 reseed) originally put a 4-byte placeholder at 0x6c to push
    // mDataResults to 0x70. That slot assignment was one byte off: retail
    // BYTE-loads mSecondPedalHiHat at 0x6c, not 0x6b -- see ProfileMgr::Handle
    // HANDLE_EXPR(get_second_pedal_hihat), where retail emits `lbz r11,-0x5c(r26)`
    // and we emitted `lbz r11,-0x5d(r26)` (r26 = this+200, so 200-0x5c = 0x6c).
    // So the unknown survivor is a single BYTE at 0x6b and mSecondPedalHiHat
    // follows at 0x6c; 0x6d..0x6f is tail padding, which still lands
    // mDataResults @0x70 and preserves the verified mMicVolumes @0x88 /
    // mProfiles @0xa0 / sizeof 0xf0 anchors.
    bool unk58b; // 0x6b TODO: identify (read by SongSortMgr::BuildFilteredSongList)
    bool mSecondPedalHiHat; // 0x6c
    DataResultList mDataResults; // 0x70 (ends 0x88)
    std::vector<int> mMicVolumes; // 0x88
    DataArray *mSliderConfig; // 0x94
    DataArray *mVoiceChatSliderConfig; // 0x98
    unsigned int mCymbalConfiguration; // 0x9c
    std::vector<BandProfile *> mProfiles; // 0xa0
    BandProfile *mPrimaryProfile; // 0xac
    bool mAllUnlocked; // 0xb0
    // mHasLoaded has zero verified callers/writers anywhere else in the tree
    // (only its own accessor + the LoadGlobalOptions write site), so its exact
    // retail offset is unconstrained by any measured evidence -- parked in the
    // existing tail-padding gap after mAllUnlocked rather than at 0xc0. Compiler-
    // verified (class_layout_report.py): sizeof(ProfileMgr) is 0xf0, not 0xc8;
    // 0xc8 is the Object-vbase {vfptr}, not total size.
    bool mHasLoaded; // was 0xc0; relocated (see above)
    std::vector<float> mForcedMicGains; // 0xb4
    // ProfileMgr::Init() does a pointer-sized stw/lwz through this+0xc0 to a
    // MemAlloc'd buffer immediately handed to TheMemcardMgr.SetProfileSaveBuffer
    // (Ghidra oracle @0x82548418). CONFIRMED by objdiff: repointing this slot
    // as a member instead of a local collapsed the entire register-swap/reload
    // residue in ProfileMgr::Init (81.9%->96.6% normalized over this change +
    // the two extraneous-call removals). DC3's ProfileMgr has the equivalent
    // `void *mProfileSaveBuffer` member in the same relative slot.
    void *mProfileSaveBuffer; // 0xc0
};

extern ProfileMgr TheProfileMgr;
