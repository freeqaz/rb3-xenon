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
    void SetWiiSpeakToggle(bool);
    void SetWiiSpeakFriendsVolume(int);
    void SetWiiSpeakMicrophoneSensitivity(int);
    void SetWiiSpeakHeadphoneMode(bool);
    void SetWiiSpeakEchoSuppression(bool);
    bool GetHasSeenFirstTimeCalibration() const;
    void SetHasSeenFirstTimeCalibration(bool);
    bool GetHasConnectedProGuitar() const;
    void SetHasConnectedProGuitar(bool);
    void SetWiiFriendsPromptShown();
    bool GetUsingWiiFriends();
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
    void SetUsingWiiFriends(int);
    void CheckProfileWebSetlistStatus();
    void HandlePendingProfileUploads();
    void SyncProfileSetlists();
    bool NeedsUpload();
    void UpdateAllMicLevels();
    void UpdateMicLevels(int);
    void UpdateMultiMicDeviceSliders(Mic *, int);
    void ForceMicGain(int, float);
    void ForceMicOutputGain(int, float);
    int GetCount() const;
    int GetUnregisteredCount() const;
    int GetRegisteredCount() const;
    bool ChooseNewPrimaryProfile();
    void SetPrimaryProfile(BandProfile *);
    bool CanChangePrimaryProfile() const;
    bool HasPrimaryProfile() const;
    void HandleProfileLoadComplete();
    void HandleProfileSaveComplete();
    void FakeProfileFill();
    void SetUploadFriendsToken(int);

    bool GetBassBoost() const { return mBassBoost; }
    bool GetDolby() const { return mDolby; }
    bool GetOverscan() const { return mOverscan; }
    bool GetWiiSpeakToggle() { return mWiiSpeakToggle; }
    int GetWiiSpeakFriendsVolume() { return mWiiSpeakFriendsVolume; }
    int GetWiiSpeakMicrophoneSensitivity() { return mWiiSpeakMicrophoneSensitivity; }
    bool GetWiiSpeakHeadphoneMode() { return mWiiSpeakHeadphoneMode; }
    bool GetWiiSpeakEchoSuppression() { return mWiiSpeakEchoSuppression; }
    bool GetSynapseEnabled() const { return mSynapseEnabled; }
    int GetSyncPresetIx() const { return mSyncPresetIx; }
    bool GetShouldShowWiiFriendsPrompt();

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
    bool mSecondPedalHiHat; // 0x6b
    DataResultList mDataResults; // 0x6c
    // Retail X360 places the mic-volume/profile block immediately after
    // mDataResults; the WiiSpeak/WiiFriends members (Wii-only feature) live at
    // the tail of the class. Verified from ProfileMgr::Poll (mProfiles @0x9c)
    // and UpdateMultiMicDeviceSliders (mMicVolumes @0x84).
    std::vector<int> mMicVolumes; // 0x84
    DataArray *mSliderConfig; // 0x90
    DataArray *mVoiceChatSliderConfig; // 0x94
    unsigned int mCymbalConfiguration; // 0x98
    std::vector<BandProfile *> mProfiles; // 0x9c
    BandProfile *mPrimaryProfile; // 0xa8
    bool mAllUnlocked; // 0xac
    std::vector<float> mForcedMicGains; // 0xb0
    bool mWiiSpeakToggle; // 0xbc
    int mWiiSpeakFriendsVolume; // 0xc0
    int mWiiSpeakMicrophoneSensitivity; // 0xc4
    bool mWiiSpeakHeadphoneMode; // 0xc8
    bool mWiiSpeakEchoSuppression; // 0xc9
    bool mHasLoaded; // 0xca
    bool mWiiFriendsPromptShown; // 0xcb
    bool mUsingWiiFriends; // 0xcc
    int unk5b8; // 0xd0
};

extern ProfileMgr TheProfileMgr;
