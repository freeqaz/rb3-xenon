#include "tour/Tour.h"
#include "tour/QuestManager.h"
#include "tour/TourPerformer.h"
#include "tour/TourProperty.h"
#include "game/BandUser.h"
#include "meta_band/BandProfile.h"
#include "meta_band/BandUI.h"
#include "meta_band/ProfileMessages.h"
#include "meta_band/ProfileMgr.h"
#include "meta_band/SessionMgr.h"
#include "game/GameMode.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "os/Debug.h"
#include "os/System.h"
#include "tour/TourDesc.h"
#include "tour/TourProgress.h"
#include "meta_band/UIEventMgr.h"
#include "net/NetSession.h"
#include "obj/DataFile.h"
#include "tour/TourPerformerLocal.h"
#include "tour/TourPerformerRemote.h"
#include "ui/UI.h"
#include "utl/Locale.h"
#include "utl/Str.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "meta_band/AccomplishmentManager.h"
#include "meta_band/NetSync.h"
#include "ui/UILabel.h"
#include "ui/UIScreen.h"
#include "decomp.h"

#pragma pool_data off

class MusicLibraryTaskMsg : public NetMessage {
public:
    MusicLibraryTaskMsg(MusicLibrary::MusicLibraryTask &);
    virtual ~MusicLibraryTaskMsg() {}
    virtual void Save(BinStream &) const;
    virtual void Load(BinStream &);
    virtual void Dispatch();
    NETMSG_BYTECODE(MusicLibraryTaskMsg);
    NETMSG_NAME(MusicLibraryTaskMsg);
    NETMSG_NEWNETMSG(MusicLibraryTaskMsg);
    MusicLibrary::MusicLibraryTask mTask; // 0x4
};

DataArray *s_pReloadedTourData;

Tour::Tour(DataArray *, const SongMgr &smgr, BandUserMgr &umgr, bool b)
    : mSongMgr(smgr), mBandUserMgr(umgr), m_pTourPerformer(0), m_pTourProgress(0),
      m_pProfile(0), mTourShowPostSeldiffScreen(0) {
    MILO_ASSERT(!TheTour, 0x3B);
    TheTour = this;
    SetName("tour", ObjectDir::Main());
    Init(SystemConfig("tour"));
    TheSessionMgr->AddSink(this, RemoteLeaderLeftMsg::Type());
    TheProfileMgr.AddSink(this, PrimaryProfileChangedMsg::Type());
}

Tour::~Tour() {
    TheProfileMgr.RemoveSink(this, PrimaryProfileChangedMsg::Type());
    TheSessionMgr->RemoveSink(this, RemoteLeaderLeftMsg::Type());
    ClearPerformer();
    TheTour = nullptr;
    if (s_pReloadedTourData)
        s_pReloadedTourData->Release();
    Cleanup();
}

void Tour::Cleanup() {
    for (std::map<Symbol, TourProperty *>::iterator it = m_mapTourProperties.begin();
         it != m_mapTourProperties.end();
         ++it) {
        TourProperty *pProperty = it->second;
        MILO_ASSERT(pProperty, 100);
        delete pProperty;
    }
    m_mapTourProperties.clear();
    for (std::map<Symbol, TourDesc *>::iterator it = m_mapTourDesc.begin();
         it != m_mapTourDesc.end();
         ++it) {
        TourDesc *pTourDesc = it->second;
        MILO_ASSERT(pTourDesc, 0x6F);
        delete pTourDesc;
    }
    m_mapTourDesc.clear();
    m_vTourStatus.clear();
}

void Tour::Init(DataArray *arr) {
    ConfigureTourPropertyData(arr->FindArray("tour_properties"));
    ConfigureTourStatusData(arr->FindArray("tour_status_info"));
    ConfigureTourDescData(arr->FindArray("tour_desc_info"));
    mWeightManager.Init(arr->FindArray("tour_weight_info"));
    arr->FindData(tour_show_post_seldiff_screen, mTourShowPostSeldiffScreen, false);
}

void Tour::ConfigureTourStatusData(DataArray *arr) {
    for (int i = 1; i < arr->Size(); i++) {
        DataArray *pStatusEntry = arr->Array(i);
        MILO_ASSERT(pStatusEntry->Size() == 2, 0x8A);
        TourStatusEntry entry;
        entry.mStatus = pStatusEntry->Sym(0);
        entry.mStars = pStatusEntry->Int(1);
        m_vTourStatus.push_back(entry);
    }
    for (int i = 1; i < m_vTourStatus.size(); i++) {
        if (m_vTourStatus[i - 1].mStars >= m_vTourStatus[i].mStars)
            MILO_WARN("Tour status fan requirement values are not increasing!");
    }
}

int Tour::GetTourStatusIndexForFanCount(int fanCount) const {
    int result = 0;
    for (int i = 0; i < m_vTourStatus.size(); i++) {
        if (fanCount < m_vTourStatus[i].mStars)
            break;
        result = i;
    }
    return result;
}

bool Tour::DoesTourStatusExist(int fanCount, int offset) const {
    int index = offset + GetTourStatusIndexForFanCount(fanCount);
    if (index >= 0 && index < m_vTourStatus.size()) return true;
    return false;
}

Symbol Tour::GetTourStatusForStarCount(int i, int j) const {
    int iStatusIndex = j + GetTourStatusIndexForFanCount(i);
    MILO_ASSERT(iStatusIndex >= 0, 0xCC);
    MILO_ASSERT(iStatusIndex < m_vTourStatus.size(), 0xCD);
    return m_vTourStatus[iStatusIndex].mStatus;
}

int Tour::GetStarsForTourStatus(Symbol s) const {
    for (int i = 0; i < m_vTourStatus.size(); i++) {
        if (m_vTourStatus[i].mStatus == s)
            return m_vTourStatus[i].mStars;
    }
    MILO_ASSERT(false, 0xDD);
    return 0;
}

void Tour::ConfigureTourPropertyData(DataArray *arr) {
    MILO_ASSERT(m_mapTourProperties.empty(), 0xF7);
    for (int i = 1; i < arr->Size(); i++) {
        TourProperty *pProperty = new TourProperty(arr->Array(i));
        Symbol name = pProperty->GetName();
        if (HasTourProperty(name)) {
            MILO_WARN("%s tour property already exists, skipping", name);
            delete pProperty;
        } else {
            std::map<Symbol, TourProperty *>::iterator it = m_mapTourProperties.lower_bound(name);
            bool canInsert = it == m_mapTourProperties.end() || name < it->first;
            if (canInsert) {
                std::map<Symbol, TourProperty *>::iterator hint = it;
                it = m_mapTourProperties.insert(hint, std::map<Symbol, TourProperty *>::value_type(name, (TourProperty *)0));
                it->second = pProperty;
            }
        }
    }
}

void Tour::ConfigureTourDescData(DataArray *arr) {
    MILO_ASSERT(m_mapTourDesc.empty(), 0x11E);
    for (int i = 1; i < arr->Size(); i++) {
        DataArray *pTourDescArray = arr->Array(i);
        MILO_ASSERT(pTourDescArray->Size() > 0, 0x123);
        TourDesc *pTourDesc = new TourDesc(pTourDescArray, i);
        Symbol name = pTourDescArray->Sym(0);
        if (HasTourDesc(name)) {
            MILO_WARN("%s tour desc already exists, skipping", name);
            delete pTourDesc;
        } else {
            std::map<Symbol, TourDesc *>::iterator it = m_mapTourDesc.lower_bound(name);
            bool canInsert = it == m_mapTourDesc.end() || name < it->first;
            if (canInsert) {
                std::map<Symbol, TourDesc *>::iterator hint = it;
                it = m_mapTourDesc.insert(hint, std::map<Symbol, TourDesc *>::value_type(name, (TourDesc *)0));
                it->second = pTourDesc;
            }
        }
    }
}

bool Tour::HasTourProperty(Symbol s) const {
    return m_mapTourProperties.find(s) != m_mapTourProperties.end();
}

TourProperty *Tour::GetTourProperty(Symbol s) const {
    std::map<Symbol, TourProperty *>::const_iterator it = m_mapTourProperties.find(s);
    if (it != m_mapTourProperties.end())
        return it->second;
    else
        return nullptr;
}

bool Tour::HasTourDesc(Symbol s) const {
    return m_mapTourDesc.find(s) != m_mapTourDesc.end();
}

TourDesc *Tour::GetTourDesc(Symbol s) const {
    std::map<Symbol, TourDesc *>::const_iterator it = m_mapTourDesc.find(s);
    if (it != m_mapTourDesc.end())
        return it->second;
    else
        return nullptr;
}

TourProgress *Tour::GetTourProgress() const { return m_pTourProgress; }

BandProfile *Tour::GetProfile() const { return m_pProfile; }

LocalBandUser *Tour::GetUser() const {
    return !m_pProfile ? nullptr : m_pProfile->GetAssociatedLocalBandUser();
}

bool Tour::HasPerformer() const { return m_pTourPerformer != 0; }

bool Tour::SyncProperty(DataNode &_val, DataArray *_prop, int _i, PropOp _op) {
    if (_prop->Size() == _i) {
        return true;
    } else {
        Symbol b = _prop->Sym(_i);
        return false;
    }
}

void Tour::ClearPerformer() {
    if (m_pTourPerformer) {
        if (!m_pTourPerformer->IsLocal()) {
            RELEASE(m_pTourProgress);
        }
        RELEASE(m_pTourPerformer);
        m_pTourProgress = 0;
    }
}

void Tour::UseUsersProgress() {
    MILO_ASSERT(m_pProfile, 0x173);
    m_pTourProgress = m_pProfile->GetTourProgress();
}

void Tour::ResetTourData(BandProfile *i_pProfile) {
    MILO_ASSERT(i_pProfile, 0x17C);
    TourProgress *pProgress = i_pProfile->GetTourProgress();
    MILO_ASSERT(pProgress, 0x180);
    pProgress->ResetTourData();
}

bool Tour::IsUnderway(BandProfile *i_pProfile) const {
    MILO_ASSERT(i_pProfile, 0x1A5);
    TourProgress *pProgress = i_pProfile->GetTourProgress();
    MILO_ASSERT(pProgress, 0x1A9);
    return pProgress->IsOnTour();
}

void Tour::InitializeTour() {
    ClearPerformer();
    TheGameMode->SetMode(tour);
    if (TheSessionMgr->IsLeaderLocal()) {
        m_pTourPerformer = new TourPerformerLocal(mBandUserMgr);
        MetaPerformer *pPerformer = MetaPerformer::Current();
        MILO_ASSERT(pPerformer, 0x192);
        UseUsersProgress();
        pPerformer->SetSyncDirty(0xFFFFFFFF, true);
    } else {
        m_pTourPerformer = new TourPerformerRemote(mBandUserMgr);
        m_pTourProgress = new TourProgress();
    }
}

bool Tour::HasGigSpecificIntro() const {
    MILO_ASSERT(m_pTourPerformer, 0x1B1);
    Quest *quest = TheQuestMgr.GetQuest(m_pTourPerformer->GetCurrentQuest());
    if (quest)
        return quest->HasCustomIntro();
    else
        return false;
}

bool Tour::HasGigSpecificOutro() const {
    MILO_ASSERT(m_pTourPerformer, 0x1C3);
    Quest *quest = TheQuestMgr.GetQuest(m_pTourPerformer->GetCurrentQuest());
    if (quest)
        return quest->HasCustomOutro();
    else
        return false;
}

Symbol Tour::GetGigSpecificIntro() const {
    MILO_ASSERT(m_pTourPerformer, 0x1D4);
    Quest *pQuest = TheQuestMgr.GetQuest(m_pTourPerformer->GetCurrentQuest());
    MILO_ASSERT(pQuest, 0x1D9);
    return pQuest->GetCustomIntro();
}

Symbol Tour::GetGigSpecificOutro() const {
    MILO_ASSERT(m_pTourPerformer, 0x1E1);
    Quest *pQuest = TheQuestMgr.GetQuest(m_pTourPerformer->GetCurrentQuest());
    MILO_ASSERT(pQuest, 0x1E6);
    return pQuest->GetCustomOutro();
}

Quest *Tour::GetQuest() {
    Quest *ret = 0;
    if (m_pTourPerformer) {
        ret = TheQuestMgr.GetQuest(m_pTourPerformer->GetCurrentQuest());
    }
    return ret;
}

TourPerformerImpl *Tour::GetPerformer() const { return m_pTourPerformer; }

DECOMP_FORCEACTIVE(Tour, "pUser")

bool Tour::HasAnnouncement() const { return GetAnnouncement() != ""; }

Symbol Tour::GetGigFlavor() const {
    if (m_pTourProgress) {
        Symbol tourSym = m_pTourProgress->GetTourDesc();
        TourDesc *pTourDesc = GetTourDesc(tourSym);
        MILO_ASSERT(pTourDesc, 0x226);
        return pTourDesc->GetFlavorForGigNum(m_pTourProgress->GetNumCompletedGigs());
    } else
        return "";
}

Symbol Tour::GetTourGigGuideMap() const {
    if (m_pTourProgress) {
        Symbol tourSym = m_pTourProgress->GetTourDesc();
        TourDesc *pTourDesc = GetTourDesc(tourSym);
        if (pTourDesc)
            return pTourDesc->GetGigGuideMap();
    }
    return "";
}

Symbol Tour::GetConclusionText() const {
    if (m_pTourProgress) {
        Symbol tourSym = m_pTourProgress->GetTourDesc();
        TourDesc *pTourDesc = GetTourDesc(tourSym);
        if (pTourDesc)
            return pTourDesc->GetConclusionText();
    }
    return "";
}

Symbol Tour::GetAnnouncement() const {
    if (m_pTourProgress) {
        Symbol tourSym = m_pTourProgress->GetTourDesc();
        TourDesc *pTourDesc = GetTourDesc(tourSym);
        MILO_ASSERT(pTourDesc, 0x255);
        int gigs = m_pTourProgress->GetNumCompletedGigs();
        if (pTourDesc->HasAnnouncementScreen(gigs)) {
            return pTourDesc->GetAnnouncementScreenForGigNum(gigs);
        }
    }
    return "";
}

Symbol Tour::GetMapScreen() const {
    if (m_pTourProgress && m_pTourProgress->IsOnTour()) {
        Symbol tourSym = m_pTourProgress->GetTourDesc();
        TourDesc *pTourDesc = GetTourDesc(tourSym);
        MILO_ASSERT(pTourDesc, 0x268);
        return pTourDesc->GetMapScreenForGigNum(m_pTourProgress->GetNumCompletedGigs());
    }
    return "";
}

const char *Tour::GetProgressOwnerName() {
    MILO_ASSERT(m_pTourProgress, 0x274);
    BandProfile *pProfile = TheProfileMgr.FindTourProgressOwner(m_pTourProgress);
    MILO_ASSERT(pProfile, 0x277);
    return pProfile->GetName();
}

int Tour::GetBronzeMedalGoalInCurrentTour() const {
    TourProgress *pProgress = m_pTourProgress;
    MILO_ASSERT(pProgress, 0x3A1);
    Symbol tourSym = pProgress->GetTourDesc();
    TourDesc *pTourDesc = GetTourDesc(tourSym);
    MILO_ASSERT(pTourDesc, 0x3A6);
    return pTourDesc->GetTourStarsBronzeGoalValue();
}

int Tour::GetSilverMedalGoalInCurrentTour() const {
    TourProgress *pProgress = m_pTourProgress;
    MILO_ASSERT(pProgress, 0x3AF);
    Symbol tourSym = pProgress->GetTourDesc();
    TourDesc *pTourDesc = GetTourDesc(tourSym);
    MILO_ASSERT(pTourDesc, 0x3B4);
    return pTourDesc->GetTourStarsSilverGoalValue();
}

int Tour::GetGoldMedalGoalInCurrentTour() const {
    TourProgress *pProgress = m_pTourProgress;
    MILO_ASSERT(pProgress, 0x3BD);
    Symbol tourSym = pProgress->GetTourDesc();
    TourDesc *pTourDesc = GetTourDesc(tourSym);
    MILO_ASSERT(pTourDesc, 0x3C2);
    return pTourDesc->GetTourStarsGoldGoalValue();
}

bool Tour::HasBronzeMedalInCurrentTour() const {
    TourProgress *pProgress = m_pTourProgress;
    MILO_ASSERT(pProgress, 0x3CB);
    return HasBronzeMedal(pProgress->GetTourDesc());
}

bool Tour::HasSilverMedalInCurrentTour() const {
    TourProgress *pProgress = m_pTourProgress;
    MILO_ASSERT(pProgress, 0x3D6);
    return HasSilverMedal(pProgress->GetTourDesc());
}

bool Tour::HasGoldMedalInCurrentTour() const {
    TourProgress *pProgress = m_pTourProgress;
    MILO_ASSERT(pProgress, 0x3E1);
    return HasGoldMedal(pProgress->GetTourDesc());
}

bool Tour::HasBronzeMedal(Symbol s) const {
    TourProgress *pProgress = m_pTourProgress;
    MILO_ASSERT(pProgress, 0x3EC);
    int stars = pProgress->GetTourMostStars(s);
    TourDesc *pTourDesc = GetTourDesc(s);
    MILO_ASSERT(pTourDesc, 0x3F1);
    int goal = pTourDesc->GetTourStarsBronzeGoalValue();
    return goal <= stars;
}

bool Tour::HasSilverMedal(Symbol s) const {
    TourProgress *pProgress = m_pTourProgress;
    MILO_ASSERT(pProgress, 0x3FA);
    int stars = pProgress->GetTourMostStars(s);
    TourDesc *pTourDesc = GetTourDesc(s);
    MILO_ASSERT(pTourDesc, 0x3FF);
    int goal = pTourDesc->GetTourStarsSilverGoalValue();
    return goal <= stars;
}

bool Tour::HasGoldMedal(Symbol s) const {
    TourProgress *pProgress = m_pTourProgress;
    MILO_ASSERT(pProgress, 0x408);
    int stars = pProgress->GetTourMostStars(s);
    TourDesc *pTourDesc = GetTourDesc(s);
    MILO_ASSERT(pTourDesc, 0x40D);
    int goal = pTourDesc->GetTourStarsGoldGoalValue();
    return goal <= stars;
}

void Tour::UpdateProgressWithCareerData() {
    TourProgress *pProgress = m_pTourProgress;
    MILO_ASSERT(pProgress, 0x46a);
    const AccomplishmentProgress &progress = m_pProfile->GetAccomplishmentProgress();
    pProgress->SetMetaScore(progress.GetMetaScore());
    {
        const std::hash_map<Symbol, int> &hmap = progress.GetToursPlayedMap();
        std::map<Symbol, int> smap(hmap.begin(), hmap.end());
        pProgress->SetToursPlayedMap(smap);
    }
    {
        const std::hash_map<Symbol, int> &hmap = progress.GetToursMostStarsMap();
        std::map<Symbol, int> smap(hmap.begin(), hmap.end());
        pProgress->SetTourMostStarsMap(smap);
    }
    MetaPerformer *performer = MetaPerformer::Current();
    MILO_ASSERT(performer, 0x473);
    performer->SetSyncDirty(0xFFFFFFFF, true);
}

DataNode Tour::OnMsg(const RemoteLeaderLeftMsg& msg) {
    if (m_pTourPerformer) {
        MILO_ASSERT(!m_pTourPerformer->IsLocal(), 0x38b);
        TheBandUI.TriggerDisbandEvent(BandUI::kNoLeader);
    }
    return 1;
}

DataNode Tour::OnMsg(const PrimaryProfileChangedMsg& msg) {
    BandProfile *profile = TheProfileMgr.GetPrimaryProfile();
    if (profile) {
        m_pProfile = profile;
    }
    if (!TheGameMode->InMode(tour)) {
        return 1;
    }
    int isPostScreen = 0;
    UIScreen *pScreen = TheUI->CurrentScreen();
    if (pScreen) {
        isPostScreen = streq(pScreen->Name(), tour_customize_post_screen.Str());
    }
    bool shouldSignOut = false;
    if (isPostScreen) {
        if (!profile) {
            shouldSignOut = true;
        }
    } else {
        TourProgress *pProgress = TheTour->m_pTourProgress;
        if (!profile || (pProgress && !profile->OwnsTourProgress(pProgress))) {
            shouldSignOut = true;
        }
    }
    if (shouldSignOut) {
        static Message sMsg("sign_out_notify", 0);
        sMsg[0] = 2;
        TheUIEventMgr->TriggerEvent(sign_out, sMsg);
    }
    return 1;
}

String Tour::GetFilterName(Symbol filter) const {
    if (TheQuestMgr.HasQuestFilter(filter)) {
        return String(Localize(filter, nullptr));
    }
    if (TheQuestMgr.HasFixedSetlist(filter)) {
        return String(Localize(fixedset1, nullptr));
    }
    if (strncmp("filter_artist_", filter.Str(), 14) == 0) {
        String full(filter.Str());
        String sub = full.substr(14);
        return sub;
    }
    MILO_ASSERT(false, 0x497);
    return String("");
}

String Tour::GetCurrentFilterName() const {
    TourPerformerImpl *pPerformer = TheTour->m_pTourPerformer;
    MILO_ASSERT(pPerformer, 0x47c);
    Symbol filter = pPerformer->GetCurrentQuestFilter();
    return TheTour->GetFilterName(filter);
}

void Tour::ClearCurrentQuest() {
    TourPerformerLocal *pLocal = dynamic_cast<TourPerformerLocal *>(m_pTourPerformer);
    if (pLocal) {
        pLocal->ClearCurrentQuest();
        pLocal->ClearCurrentQuestFilter();
    }
}

bool Tour::ShouldShowPostSelDiffScreen() const {
    return mTourShowPostSeldiffScreen;
}

void Tour::CheatReloadTourData() {
    if (s_pReloadedTourData)
        s_pReloadedTourData->Release();
    s_pReloadedTourData = DataReadFile("config/tour.dta", true);
    Cleanup();
    Init(s_pReloadedTourData);
    TheQuestMgr.Cleanup();
    TheQuestMgr.Init(s_pReloadedTourData);
}

Symbol Tour::CombinePartSymbols(Symbol part1, Symbol part2) {
    Symbol result = part1;
    if (part2 != gNullStr && part1 != band) {
        if (part1 == gNullStr) result = part2;
        else result = band;
    }
    return result;
}

String Tour::GetBronzeGoalIcon() {
    TourProgress *pProgress = TheTour->m_pTourProgress;
    MILO_ASSERT(pProgress, 0x49f);
    Symbol tourDescSym = pProgress->GetTourDesc();
    TourDesc *pTourDesc = TheTour->GetTourDesc(tourDescSym);
    MILO_ASSERT(pTourDesc, 0x4a2);
    Symbol goal = pTourDesc->GetTourBronzeGoal();
    Accomplishment *pAccomplishment = TheAccomplishmentMgr->GetAccomplishment(goal);
    MILO_ASSERT(pAccomplishment, 0x4a7);
    return String(pAccomplishment->GetIconArt());
}

String Tour::GetSilverGoalIcon() {
    TourProgress *pProgress = TheTour->m_pTourProgress;
    MILO_ASSERT(pProgress, 0x4b0);
    Symbol tourDescSym = pProgress->GetTourDesc();
    TourDesc *pTourDesc = TheTour->GetTourDesc(tourDescSym);
    MILO_ASSERT(pTourDesc, 0x4b3);
    Symbol goal = pTourDesc->GetTourSilverGoal();
    Accomplishment *pAccomplishment = TheAccomplishmentMgr->GetAccomplishment(goal);
    MILO_ASSERT(pAccomplishment, 0x4b8);
    return String(pAccomplishment->GetIconArt());
}

String Tour::GetGoldGoalIcon() {
    TourProgress *pProgress = TheTour->m_pTourProgress;
    MILO_ASSERT(pProgress, 0x4c1);
    Symbol tourDescSym = pProgress->GetTourDesc();
    TourDesc *pTourDesc = TheTour->GetTourDesc(tourDescSym);
    MILO_ASSERT(pTourDesc, 0x4c4);
    Symbol goal = pTourDesc->GetTourGoldGoal();
    Accomplishment *pAccomplishment = TheAccomplishmentMgr->GetAccomplishment(goal);
    MILO_ASSERT(pAccomplishment, 0x4c9);
    return String(pAccomplishment->GetIconArt());
}

TourMode Tour::GetMode() {
    TourPerformerRemote *pRemote = dynamic_cast<TourPerformerRemote *>(m_pTourPerformer);
    if (pRemote) {
        if (!TheNetSession->IsLocal()) return kMetaTour_KnownRemote;
        return kMetaTour_BrowsingRemote;
    }
    TourPerformerLocal *pLocal = dynamic_cast<TourPerformerLocal *>(m_pTourPerformer);
    if (!pLocal) return kMetaTour_Nil;
    return kMetaTour_KnownLocal;
}

void Tour::SetupSongsForFixedSetlist(Symbol sym) {
    FixedSetlist *fixedSetlist = TheQuestMgr.GetFixedSetlist(sym);
    MILO_ASSERT(fixedSetlist, 0x2e9);
    MetaPerformer *performer = MetaPerformer::Current();
    MILO_ASSERT(performer, 0x2ec);
    std::vector<Symbol> songs;
    fixedSetlist->InqSongs(songs);
    performer->SetSongs(songs);
}

void Tour::UpdateFinishedMedalLabel(UILabel *label) {
    MILO_ASSERT(label, 0x415);
    TourProgress *pProgress = m_pTourProgress;
    MILO_ASSERT(pProgress, 0x418);
    int numStars = pProgress->GetNumStars();
    Symbol tourDescSym = pProgress->GetTourDesc();
    TourDesc *pTourDesc = GetTourDesc(tourDescSym);
    MILO_ASSERT(pTourDesc, 0x41e);
    if (numStars < pTourDesc->GetTourStarsBronzeGoalValue()) {
        label->SetTokenFmt(tour_finished_no_medal, pTourDesc->GetTourStarsBronzeGoalValue() - numStars);
    } else if (numStars < pTourDesc->GetTourStarsSilverGoalValue()) {
        label->SetTokenFmt(tour_finished_bronze_medal, pTourDesc->GetTourStarsSilverGoalValue() - numStars);
    } else if (numStars < pTourDesc->GetTourStarsGoldGoalValue()) {
        label->SetTokenFmt(tour_finished_silver_medal, pTourDesc->GetTourStarsGoldGoalValue() - numStars);
    } else {
        label->SetTextToken(tour_finished_gold_medal);
    }
}

void Tour::UpdateNextMedalLabel(UILabel *label) {
    MILO_ASSERT(label, 0x43f);
    TourProgress *pProgress = m_pTourProgress;
    MILO_ASSERT(pProgress, 0x442);
    int numStars = pProgress->GetNumStars();
    Symbol tourDescSym = pProgress->GetTourDesc();
    TourDesc *pTourDesc = GetTourDesc(tourDescSym);
    MILO_ASSERT(pTourDesc, 0x448);
    if (numStars < pTourDesc->GetTourStarsBronzeGoalValue()) {
        label->SetTokenFmt(tourdesc_bronze_starcount_needed, pTourDesc->GetTourStarsBronzeGoalValue() - numStars);
    } else if (numStars < pTourDesc->GetTourStarsSilverGoalValue()) {
        label->SetTokenFmt(tourdesc_silver_starcount_needed, pTourDesc->GetTourStarsSilverGoalValue() - numStars);
    } else if (numStars < pTourDesc->GetTourStarsGoldGoalValue()) {
        label->SetTokenFmt(tourdesc_gold_starcount_needed, pTourDesc->GetTourStarsGoldGoalValue() - numStars);
    } else {
        label->SetTextToken(tourdesc_max_starcount_needed);
    }
}

SongSortMgr::SongFilter Tour::CreateArtistFilter(const char *artistName) {
    Symbol artistSym(artistName);
    SongSortMgr::SongFilter filter;
    filter.AddFilter((FilterType)9, artistSym);
    return filter;
}

#pragma push
#pragma dont_inline on
#pragma pool_data off
void Tour::ChooseRandomSongsForQuestFilter(int count, Symbol questSym1, Symbol questSym2, bool allowDups) {
    SongSortMgr::SongFilter filter;
    Symbol partSym(gNullStr);
    GigFilter *pFilter1 = TheQuestMgr.GetQuestFilter(questSym1);
    if (pFilter1) {
        filter = pFilter1->GetFilter();
        partSym = pFilter1->GetFilteredPartSym();
    } else if (strncmp("filter_artist_", questSym1.Str(), 14) == 0) {
        String s(questSym1.Str());
        String substr = s.substr(14);
        filter = CreateArtistFilter(substr.c_str());
    } else {
        MILO_FAIL("Invalid Random Filter = %s\n", questSym1);
    }
    GigFilter *pFilter2 = TheQuestMgr.GetQuestFilter(questSym2);
    if (pFilter2) {
        SongSortMgr::SongFilter filter2 = pFilter2->GetFilter();
        filter.IntersectFilter(&filter2);
        Symbol other(pFilter2->GetFilteredPartSym());
        partSym = CombinePartSymbols(partSym, other);
    }
    TheMusicLibrary->SetRandomSongs(count, filter, partSym, true, allowDups);
}
#pragma pop

void Tour::InitializeMusicLibraryTaskForArtist(
    MusicLibrary::MusicLibraryTask &task, int maxSize, const char *artistName, Symbol questSym
) {
    task.maxSetlistSize = maxSize;
    {
        SongSortMgr::SongFilter artistFilter = CreateArtistFilter(artistName);
        task.filter = artistFilter;
    }
    task.partSym = Symbol(gNullStr);
    if (questSym != gNullStr) {
        GigFilter *pFilter = TheQuestMgr.GetQuestFilter(questSym);
        MILO_ASSERT(pFilter, 0x307);
        SongSortMgr::SongFilter questFilter = pFilter->GetFilter();
        task.filter.IntersectFilter(&questFilter);
        task.partSym = TheTour->CombinePartSymbols(task.partSym, pFilter->GetFilteredPartSym());
    }
    task.allowDuplicates = false;
    task.setlistMode = MusicLibrary::kSetlistForced;
}

void Tour::CreateAndSubmitMusicLibraryTask(
    int maxSize, Symbol questSym, Symbol artistSym, Symbol nextScreen, Symbol backScreen, bool requireStandardParts
) {
    MusicLibrary::MusicLibraryTask task;
    task.requiresStandardParts = requireStandardParts;
    GigFilter *pFilter = TheQuestMgr.GetQuestFilter(questSym);
    if (pFilter) {
        pFilter->InitializeMusicLibraryTask(task, maxSize, artistSym);
    } else {
        const char *questStr = questSym.Str();
        if (strncmp("filter_artist_", questStr, 14) == 0) {
            String nameStr(questStr);
            String artistName = nameStr.substr(14);
            InitializeMusicLibraryTaskForArtist(task, maxSize, artistName.c_str(), artistSym);
        } else {
            MILO_ASSERT(false, 0x32d);
        }
    }
    task.backScreen = backScreen;
    task.nextScreen = nextScreen;
    Quest *quest = GetQuest();
    MILO_ASSERT(quest, 0x336);
    task.titleToken = quest->GetDisplayName();
    task.makingSetlistToken = quest->GetDisplayName();
    TheMusicLibrary->SetTask(task);
    if (TheNetSession) {
        MusicLibraryTaskMsg msg(task);
        TheNetSession->SendMsgToAll(msg, kReliable);
    }
}

void Tour::LaunchQuestFilter(
    int count, Symbol questSym, Symbol filterSym1, Symbol filterSym2,
    TourSetlistType setlistType, Symbol screenSym1, Symbol screenSym2, Symbol screenSym3
) {
    Quest *quest = TheQuestMgr.GetQuest(questSym);
    MILO_ASSERT(quest, 0x287);
    bool ugcAllowed = quest->IsUGCAllowed();
    if (setlistType == kTourSetlist_Random) {
        ChooseRandomSongsForQuestFilter(count, filterSym1, filterSym2, ugcAllowed);
        UIScreen *pScreen = ObjectDir::Main()->Find<UIScreen>(screenSym2.Str(), true);
        MILO_ASSERT(pScreen, 0x290);
        TheNetSync->SyncScreen(pScreen, 0);
    } else if (setlistType == kTourSetlist_Custom) {
        CreateAndSubmitMusicLibraryTask(count, filterSym1, Symbol(gNullStr), screenSym2, screenSym3, ugcAllowed);
        UIScreen *pScreen = ObjectDir::Main()->Find<UIScreen>(screenSym1.Str(), true);
        MILO_ASSERT(pScreen, 0x297);
        TheNetSync->SyncScreen(pScreen, 0);
    } else if (setlistType == kTourSetlist_Fixed) {
        SetupSongsForFixedSetlist(filterSym1);
        UIScreen *pScreen = ObjectDir::Main()->Find<UIScreen>(screenSym2.Str(), true);
        MILO_ASSERT(pScreen, 0x29e);
        TheNetSync->SyncScreen(pScreen, 0);
    } else {
        MILO_ASSERT(false, 0x2a3);
    }
}

BEGIN_HANDLERS(Tour)
    HANDLE_EXPR(progress, DataNode(GetTourProgress()))
    HANDLE_EXPR(performer, DataNode(GetPerformer()))
    HANDLE_EXPR(has_performer, DataNode(HasPerformer()))
    HANDLE_EXPR(mode, DataNode(GetMode()))
    HANDLE_EXPR(get_profile, DataNode(GetProfile()))
    HANDLE_ACTION(clear_performer, ClearPerformer())
    HANDLE_ACTION(initialize_tour, InitializeTour())
    HANDLE_ACTION(reset_tour_data, ResetTourData(_msg->Obj<BandProfile>(2)))
    HANDLE_EXPR(should_show_post_seldiff_screen, DataNode(ShouldShowPostSelDiffScreen()))
    HANDLE_EXPR(is_underway, DataNode(IsUnderway(_msg->Obj<BandProfile>(2))))
    HANDLE_EXPR(has_gig_specific_intro, DataNode(HasGigSpecificIntro()))
    HANDLE_EXPR(has_gig_specific_outro, DataNode(HasGigSpecificOutro()))
    HANDLE_EXPR(get_gig_specific_intro, DataNode(GetGigSpecificIntro()))
    HANDLE_EXPR(get_gig_specific_outro, DataNode(GetGigSpecificOutro()))
    HANDLE_EXPR(get_progress_owner_name, DataNode(GetProgressOwnerName()))
    HANDLE_EXPR(has_announcement, DataNode(HasAnnouncement()))
    HANDLE_EXPR(get_announcement, DataNode(GetAnnouncement()))
    HANDLE_EXPR(get_map_screen, DataNode(GetMapScreen()))
    HANDLE_EXPR(get_gig_flavor, DataNode(GetGigFlavor()))
    HANDLE_EXPR(get_tour_gigguide_map, DataNode(GetTourGigGuideMap()))
    HANDLE_EXPR(get_tour_conclusion_text, DataNode(GetConclusionText()))
    HANDLE_EXPR(get_bronze_medal_goal, DataNode(GetBronzeMedalGoalInCurrentTour()))
    HANDLE_EXPR(get_silver_medal_goal, DataNode(GetSilverMedalGoalInCurrentTour()))
    HANDLE_EXPR(get_gold_medal_goal, DataNode(GetGoldMedalGoalInCurrentTour()))
    // retail X360: these three arms call Has*Medal(m_pTourProgress->GetTourDesc())
    // directly (GetTourDesc sret to a named local read back from its slot), not the
    // *InCurrentTour wrappers
    {
        static Symbol _hs("has_bronze_medal");
        if (sym == _hs) {
            Symbol tourSym = m_pTourProgress->GetTourDesc();
            return DataNode(HasBronzeMedal(tourSym));
        }
    }
    {
        static Symbol _hs("has_silver_medal");
        if (sym == _hs) {
            Symbol tourSym = m_pTourProgress->GetTourDesc();
            return DataNode(HasSilverMedal(tourSym));
        }
    }
    {
        static Symbol _hs("has_gold_medal");
        if (sym == _hs) {
            Symbol tourSym = m_pTourProgress->GetTourDesc();
            return DataNode(HasGoldMedal(tourSym));
        }
    }
    HANDLE_EXPR(get_current_filter_name, DataNode(GetCurrentFilterName()))
    HANDLE_EXPR(get_bronze_medal_icon, DataNode(GetBronzeGoalIcon()))
    HANDLE_EXPR(get_silver_medal_icon, DataNode(GetSilverGoalIcon()))
    HANDLE_EXPR(get_gold_medal_icon, DataNode(GetGoldGoalIcon()))
    HANDLE_ACTION(update_finished_medal_label, UpdateFinishedMedalLabel(_msg->Obj<UILabel>(2)))
    HANDLE_ACTION(update_next_medal_label, UpdateNextMedalLabel(_msg->Obj<UILabel>(2)))
    HANDLE_ACTION(clear_current_quest, ClearCurrentQuest())
    HANDLE_ACTION(update_progress_with_career_data, UpdateProgressWithCareerData())
    // retail X360: no cheat_reload_data arm (cheat handler stripped from retail)
    HANDLE_MESSAGE(PrimaryProfileChangedMsg)
    HANDLE_MESSAGE(RemoteLeaderLeftMsg)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x54C)
END_HANDLERS
