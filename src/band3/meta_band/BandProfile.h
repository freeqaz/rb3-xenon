#pragma once
#include "obj/Msg.h"
#include "rndobj/Tex.h"
#include "system/meta/Profile.h"
#include "game/Defines.h"
#include "meta_band/GameplayOptions.h"
#include <vector>
#include "StandIn.h"
#include "ProfileAssets.h"
#include "meta_band/AccomplishmentProgress.h"
#include "net_band/DataResults.h"
#include "meta_band/PerformanceData.h"
#include "tour/TourCharLocal.h"

#define kMaxCharacters 10
#define kMaxPatchesPerProfile 19
#define kMaxSavedSetlists 20
#define kMaxSymbols_CampaignKeys 20
#define kMaxSymbols_Modifiers 15

class PatchDir;
class CharData;
class TourProgress;
class PerformerStatsInfo;
class PerformanceData;
class PatchDescriptor;
class SongStatusMgr;
class RockCentralOpCompleteMsg;
class LocalBandUser;
class ProfilePicture;
class TourBand;
class SavedSetlist;
class LocalSavedSetlist;

class BandProfile : public Profile {
public:
    enum {
        kMaxPerformances = 50
    };
    BandProfile(int);
    virtual ~BandProfile();
    virtual void SaveFixed(FixedSizeSaveableStream &) const;
    virtual void LoadFixed(FixedSizeSaveableStream &, int);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool HasCheated() const;
    virtual bool IsUnsaved() const;
    virtual void SaveLoadComplete(ProfileSaveState);
    virtual bool HasSomethingToUpload();
    virtual void DeleteAll();
    virtual void PreLoad();

    void Poll();
    void GetAvailableStandins(int, std::vector<TourCharLocal *> &) const;
    void GetAllChars(std::vector<TourCharLocal *> &) const;
    void GetAvailableCharacters(std::vector<TourCharLocal *> &) const;
    CharData *GetCharFromGuid(const HxGuid &);
    int GetMaxChars() const;
    void AddNewChar(TourCharLocal *);
    void DeleteChar(TourCharLocal *);
    void RenameCharacter(TourCharLocal *, const char *);
    bool HasChar(const TourCharLocal *);
    PatchDir *GetFirstEmptyPatch();
    RndTex *GetTexAtPatchIndex(int) const;
    int GetPatchIndex(const PatchDir *) const;
    void PotentiallyDeleteStandin(HxGuid);
    int GetCharacterStandinIndex(CharData *) const;
    const StandIn &GetStandIn(int) const;
    StandIn &AccessStandIn(int);
    int GetNumStandins() const;
    TourProgress *GetTourProgress();
    bool OwnsTourProgress(const TourProgress *);
    void UpdateScore(int, const PerformerStatsInfo &, bool);
    void UploadPerformance(PerformanceData *);
    int GetNumDirtyPerformanceData();
    void UploadDirtyPerformanceData();
    void UploadDirtyAccomplishmentData();
    void UploadDirtyScoreData();
    void UploadDirtyData();
    void SetSongReview(int, int);
    int GetSongReview(int);
    SongStatusMgr *GetSongStatusMgr() const;
    int GetSongHighScore(int, ScoreType) const;
    const std::vector<LocalSavedSetlist *> &GetSavedSetlists() const;
    LocalSavedSetlist *
    AddSavedSetlist(const char *, const char *, bool, const PatchDescriptor &, const std::vector<int> &);
    void DeleteSavedSetlist(LocalSavedSetlist *);
    void SetlistChanged(LocalSavedSetlist *);
    int NumSavedSetlists() const;
    int GetUploadFriendsToken() const;
    void SetUploadFriendsToken(int);
    LocalBandUser *GetAssociatedLocalBandUser() const;
    void CheckForFinishedTrainerAccomplishments();
    void SetProGuitarSongLessonComplete(int, Difficulty);
    void SetProBassSongLessonComplete(int, Difficulty);
    void SetProKeyboardSongLessonComplete(int, Difficulty);
    void SetProGuitarSongLessonSectionComplete(int, Difficulty, int);
    void SetProBassSongLessonSectionComplete(int, Difficulty, int);
    void SetProKeyboardSongLessonSectionComplete(int, Difficulty, int);
    bool IsProGuitarSongLessonSectionComplete(int, Difficulty, int) const;
    bool IsProBassSongLessonSectionComplete(int, Difficulty, int) const;
    bool IsProKeyboardSongLessonSectionComplete(int, Difficulty, int) const;
    bool IsLessonComplete(const Symbol &, float) const;
    void GetLessonComplete(const Symbol &) const;
    float GetLessonCompleteSpeed(const Symbol &) const;
    void SetLessonComplete(const Symbol &, float);
    void EarnAccomplishment(Symbol);
    const AccomplishmentProgress &GetAccomplishmentProgress() const;
    AccomplishmentProgress &AccessAccomplishmentProgress();
    int GetHardcoreIconLevel() const;
    void SetHardcoreIconLevel(int);
    TourBand *GetTourBand();
    String GetBandName() const;
    bool HasBandNameBeenSet() const;
    bool IsBandNameProfanityChecked() const;
    RndTex *GetBandLogoTex();
    void SendBandLogo();
    void GrantCampaignKey(Symbol);
    bool HasCampaignKey(Symbol);
    void UnlockModifier(Symbol);
    bool HasUnlockedModifier(Symbol);
    void HandlePerformanceDataUploadSuccess();
    void UpdatePerformanceData(
        const Stats &, int, ScoreType, Difficulty, Symbol, int, int, bool
    );
    DataNode OnMsg(const RockCentralOpCompleteMsg &);
    LocalBandUser *GetLocalBandUser() const;
    void GetAssociatedUsers(std::vector<LocalBandUser *> &) const;
    void CheckWebLinkStatus();
    void CheckWebSetlistStatus();
    bool HasSeenHint(Symbol) const;
    void SetHasSeenHint(Symbol);
    CharData *GetLastCharUsed() const;
    void SetLastCharUsed(CharData *);
    void SetLastPrefabCharUsed(Symbol);
    void FakeProfileFill();
    RndTex *GetPictureTex();
    void AutoFakeFill(int);
    int NumChars() const;
    GameplayOptions *GetGameplayOptions() { return &mGameplayOptions; }
    TourCharLocal *CharAt(int idx) const { return mCharacters[idx]; }

    static int SaveSize(int);

    std::vector<PatchDir *> mPatches; // 0x18
    std::vector<TourCharLocal *> mCharacters; // 0x24
    // mTourProgress is at 0x30, NOT 0x34: the retail BandProfile.s reads 0x30
    // eight times and 0x34 *never*, and all eight are pointer loads (vcall
    // through 0x0(r3), or `addi r4, r11, 0x14` after a null check) — none is the
    // int test `if (unk18)` would emit.  The two functions that touch unk18
    // (LoadFixed / AutoFakeFill) are not paired in the target at all, so the
    // swap costs nothing and fixes IsUnsaved / SaveLoadComplete.
    TourProgress *mTourProgress; // 0x30
    // unk18 sits AFTER mLessonCompletions (not before): BandProfile.s's
    // SaveFixed/LoadFixed both address mLessonCompletions at 0x34 (right after
    // mTourProgress, no gap) and mScores lands at 0x50 either way, so the
    // 4-byte unk18 slot must be *inside* the mTourProgress..mScores span but
    // past the 24-byte map, i.e. at 0x4c.
    // RETAIL X360 USES hash_map, NOT map (rb3-Wii dev build used map).
    // Evidence from retail bytes in SetLessonComplete: _M_find takes an sret
    // out-param (returns a _Slist_iterator BY VALUE) where _Rb_tree::_M_find
    // returns a bare node pointer; the end() test is `node == NULL` rather
    // than a compare against the tree header; and the mapped float sits at
    // node+0x8 (slist node: next@0, Symbol@4, float@8) rather than +0x14
    // (rb-tree node: links 0x0-0xc, Symbol@0x10, float@0x14).
    // ...and hash_map is 0x1c, not map's 0x18, so it reaches exactly to
    // mScores@0x50 on its own. The old 4-byte `int unk18; // 0x4c` existed
    // only to pad out the gap the smaller map left, was never referenced
    // anywhere in the tree, and is deleted here: keeping it would push every
    // member from mScores on by +4.
    std::hash_map<Symbol, float> mLessonCompletions; // 0x34
    SongStatusMgr *mScores; // 0x50
    std::vector<LocalSavedSetlist *> mSavedSetlists; // 0x54
    std::vector<StandIn> mStandIns; // 0x60
    HxGuid unk5c; // 0x5c
    // unk6c (mLastPrefabCharUsed) sits HERE, exactly as on Wii — an earlier
    // hypothesis relocated it below mAccomplishmentProgress to shift this whole
    // block -4.  That was wrong: the retail target reads mCampaignKeys at 0x80,
    // unk88 at 0x98 and mAccomplishmentProgress at 0x110 (see
    // build/45410914/asm/band3/meta_band/BandProfile.s), i.e. +4 vs the shifted
    // layout.  Moving it back is size-neutral, so the tail (unk740 …
    // mTourBand / 0x7c88) is unchanged.
    Symbol unk6c; // mLastPrefabCharUsed
    std::set<Symbol> mCampaignKeys; // retail 0x80
    std::set<Symbol> unk88; // retail 0x98
    std::set<Symbol> mUnlockedModifiers; // retail 0xb0
    GameplayOptions mGameplayOptions; // retail 0xb8
    AccomplishmentProgress mAccomplishmentProgress; // retail 0x110
    int unk740;
    int mAccomplishmentDataUploadContextID; // 0x784
    // Present on Wii (BandProfile.h:161) and in retail: CheckWebLinkStatus reads
    // unk74c at 0x78c / unk754 at 0x794, i.e. +4 vs a layout without this slot.
    // Restoring it makes the whole unk74c..mPerformanceDataList block land right
    // and removes the need for the old `unk6f78pad` tail compensator below.
    int unk748;
    int unk74c;
    int unk750;
    DataResultList unk754;
    DataResultList unk76c;
    int mPerformanceDataUploadContextID; // 0x7c4
    PerformanceData mPerformanceDataList[50]; // 0x788
    int unk6f70;
    int unk6f74;
    // (Was `int unk6f78pad;` — a tail compensator for the missing `unk748`
    // above.  Now that unk748 is restored the tail lands correctly without it,
    // so the +4 is paid once, in the right place.)
    ProfileAssets mProfileAssets; // 0x6f78
    int unk6fb4;
    int unk6fb8;
    ProfilePicture *mProfilePicture; // 0x7c7c
    TourBand *mTourBand; // 0x7c80
};

DECLARE_MESSAGE(ProfilePreDeleteMsg, "profile_pre_delete_msg");
ProfilePreDeleteMsg(BandProfile *p) : Message(Type(), p) {}
BandProfile *GetProfile() const { return mData->Obj<BandProfile>(2); }
END_MESSAGE
