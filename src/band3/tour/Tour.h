#pragma once
#include "TourDesc.h"
class UILabel;
#include "TourWeightManager.h"
#include "game/BandUser.h"
#include "meta_band/BandProfile.h"
#include "meta_band/MusicLibrary.h"
#include "meta_band/ProfileMessages.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "tour/TourProgress.h"
#include "tour/TourProperty.h"
#include "utl/Symbol.h"
#include "meta/SongMgr.h"
#include "game/BandUserMgr.h"
#include "tour/Quest.h"
#include "tour/TourPerformer.h"

enum TourMode {
    kMetaTour_Nil = -1,
    kMetaTour_UnknownRemote = 0,
    kMetaTour_StartNew = 1,
    kMetaTour_KnownLocal = 2,
    kMetaTour_KnownRemote = 3,
    kMetaTour_BrowsingRemote = 4,
};

class TourStatusEntry {
public:
    TourStatusEntry() {}
    Symbol mStatus; // 0x0
    int mStars; // 0x4
};

class Tour : public Hmx::Object {
public:
    Tour(DataArray *, const SongMgr &, BandUserMgr &, bool);
    virtual ~Tour();
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);

    bool HasTourProperty(Symbol) const;
    TourProperty *GetTourProperty(Symbol) const;
    void UpdateProgressWithCareerData();
    TourDesc *GetTourDesc(Symbol) const;
    Symbol GetTourStatusForStarCount(int, int) const;
    int GetStarsForTourStatus(Symbol) const;
    bool DoesTourStatusExist(int, int) const;
    const std::map<Symbol, TourProperty *> &TourProperties() const {
        return m_mapTourProperties;
    }
    TourProgress *GetTourProgress() const;
    Quest *GetQuest();
    void Init(DataArray *);
    void Cleanup();
    int GetTourStatusIndexForFanCount(int) const;
    void ConfigureTourPropertyData(DataArray *);
    void ConfigureTourStatusData(DataArray *);
    void ConfigureTourDescData(DataArray *);
    bool HasTourDesc(Symbol) const;
    BandProfile *GetProfile() const;
    LocalBandUser *GetUser() const;
    void ClearPerformer();
    void InitializeTour();
    void UseUsersProgress();
    void ResetTourData(BandProfile *);
    bool IsUnderway(BandProfile *) const;
    bool HasGigSpecificIntro() const;
    bool HasGigSpecificOutro() const;
    Symbol GetGigSpecificIntro() const;
    Symbol GetGigSpecificOutro() const;
    bool HasAnnouncement() const;
    Symbol GetAnnouncement() const;
    Symbol GetGigFlavor() const;
    Symbol GetTourGigGuideMap() const;
    Symbol GetConclusionText() const;
    Symbol GetMapScreen() const;
    const char *GetProgressOwnerName();
    int GetBronzeMedalGoalInCurrentTour() const;
    int GetSilverMedalGoalInCurrentTour() const;
    int GetGoldMedalGoalInCurrentTour() const;
    bool HasBronzeMedalInCurrentTour() const;
    bool HasSilverMedalInCurrentTour() const;
    bool HasGoldMedalInCurrentTour() const;
    bool HasBronzeMedal(Symbol) const;
    bool HasSilverMedal(Symbol) const;
    bool HasGoldMedal(Symbol) const;
    DataNode OnMsg(const RemoteLeaderLeftMsg &);
    DataNode OnMsg(const PrimaryProfileChangedMsg &);
    void LaunchQuestFilter(
        int, Symbol, Symbol, Symbol, TourSetlistType, Symbol, Symbol, Symbol
    );
    String GetCurrentFilterName() const;
    String GetFilterName(Symbol) const;
    void ClearCurrentQuest();
    bool ShouldShowPostSelDiffScreen() const;
    void CheatReloadTourData();
    TourMode GetMode();
    Symbol CombinePartSymbols(Symbol, Symbol);
    String GetBronzeGoalIcon();
    String GetSilverGoalIcon();
    String GetGoldGoalIcon();
    void SetupSongsForFixedSetlist(Symbol);
    void UpdateFinishedMedalLabel(UILabel *);
    void UpdateNextMedalLabel(UILabel *);

    bool HasPerformer() const;
    TourPerformerImpl *GetPerformer() const;
    SongSortMgr::SongFilter CreateArtistFilter(const char *);
    void ChooseRandomSongsForQuestFilter(int, Symbol, Symbol, bool);
    void InitializeMusicLibraryTaskForArtist(MusicLibrary::MusicLibraryTask &, int, const char *, Symbol);
    void CreateAndSubmitMusicLibraryTask(int, Symbol, Symbol, Symbol, Symbol, bool);

    const SongMgr &mSongMgr; // 0x1c
    BandUserMgr &mBandUserMgr; // 0x20
    TourPerformerImpl *m_pTourPerformer; // 0x24
    TourProgress *m_pTourProgress; // 0x28
    std::map<Symbol, TourProperty *> m_mapTourProperties; // 0x2c
    std::vector<TourStatusEntry> m_vTourStatus; // 0x44
    TourWeightManager mWeightManager; // 0x4c
    std::map<Symbol, TourDesc *> m_mapTourDesc; // 0x68
    BandProfile *m_pProfile; // 0x80
    bool unk84; // 0x84
    bool mTourShowPostSeldiffScreen; // 0x85
};

extern Tour *TheTour;