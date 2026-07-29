#include "tour/TourDesc.h"
#include "meta_band/Accomplishment.h"
#include "meta_band/AccomplishmentManager.h"
#include "meta_band/AccomplishmentTourConditional.h"
#include "obj/Data.h"
#include "os/Debug.h"
#include "utl/MakeString.h"
#include "utl/Symbol.h"

TourDesc::TourDesc(DataArray *arr, int i)
    : mName(""), mIndex(i), mRequiredCampaignLevel(gNullStr), mLeaderboardGoal(gNullStr),
      mDifficulty(gNullStr), mGigGuideMap(gNullStr) {
    Configure(arr);
}

TourDesc::~TourDesc() { Cleanup(); }

void TourDesc::Cleanup() {
    for (std::vector<TourDescEntry *>::iterator it = m_vEntries.begin();
         it != m_vEntries.end();
         ++it) {
        TourDescEntry *pEntry = *it;
        MILO_ASSERT(pEntry, 0x2A);
        delete pEntry;
    }
    m_vEntries.clear();
}

void TourDesc::Configure(DataArray *i_pConfig) {
    // NOTE (retail-vs-Wii-dev): RB3 retail spells every one of these as a
    // FUNCTION-LOCAL static Symbol, not a global from utl/Symbols*.h -- the
    // target carries a single guard word (0x82CBEC10) with 19 bits, one per
    // symbol, plus 19 matching `??__F` atexit funclets.  The DECLARATION
    // POSITION is codegen-load-bearing: it fixes both the guard-bit numbering
    // (hence the funclet bodies) and where the guard test lands in the stream.
    // The outer eight are at point of use; the ten inner ones are hoisted to
    // the top of the loop body (retail tests bits 8..17 immediately after
    // DataArray::Array(i), before `operator new`), while setlist_type stays at
    // its point of use.  Do not "tidy" any of this.
    MILO_ASSERT(i_pConfig, 0x33);
    mName = i_pConfig->Sym(0);
    MILO_ASSERT(m_vEntries.empty(), 0x38);
    static Symbol required_campaign_level("required_campaign_level"); // bit 0
    i_pConfig->FindData(required_campaign_level, mRequiredCampaignLevel, true);
    static Symbol gigguide_map("gigguide_map"); // bit 1
    i_pConfig->FindData(gigguide_map, mGigGuideMap, true);
    static Symbol difficulty_token("difficulty_token"); // bit 2
    i_pConfig->FindData(difficulty_token, mDifficulty, true);
    static Symbol leaderboard_goal("leaderboard_goal"); // bit 3
    i_pConfig->FindData(leaderboard_goal, mLeaderboardGoal, false);
    static Symbol tour_stars_bronze_goal("tour_stars_bronze_goal"); // bit 4
    i_pConfig->FindData(tour_stars_bronze_goal, mTourStarsBronzeGoal, true);
    static Symbol tour_stars_silver_goal("tour_stars_silver_goal"); // bit 5
    i_pConfig->FindData(tour_stars_silver_goal, mTourStarsSilverGoal, true);
    static Symbol tour_stars_gold_goal("tour_stars_gold_goal"); // bit 6
    i_pConfig->FindData(tour_stars_gold_goal, mTourStarsGoldGoal, true);
    static Symbol gigs("gigs"); // bit 7
    DataArray *pGigArray = i_pConfig->FindArray(gigs);
    MILO_ASSERT(pGigArray, 0x51);

    for (int i = 1; i < pGigArray->Size(); i++) {
        DataArray *pGigEntry = pGigArray->Array(i);
        MILO_ASSERT(pGigEntry, 0x55);
        static Symbol filter("filter"); // bit 8
        static Symbol gig_tier("gig_tier"); // bit 9
        static Symbol gig("gig"); // bit 10
        static Symbol gig_group("gig_group"); // bit 11
        static Symbol num_songs("num_songs"); // bit 12
        static Symbol announce("announce"); // bit 13
        static Symbol flavor("flavor"); // bit 14
        static Symbol map("map"); // bit 15
        static Symbol venue("venue"); // bit 16
        static Symbol city("city"); // bit 17
        TourDescEntry *entry = new TourDescEntry();
        if (!pGigEntry->FindData(gig_tier, entry->mTier, false)) {
            if (!pGigEntry->FindData(gig_group, entry->mGroup, false)) {
                pGigEntry->FindData(gig, entry->mQuest, true);
            }
        }
        pGigEntry->FindData(filter, entry->mFilter, false);
        pGigEntry->FindData(num_songs, entry->mNumSongs, true);
        pGigEntry->FindData(city, entry->mCity, true);
        pGigEntry->FindData(announce, entry->mAnnouncementScreen, false);
        pGigEntry->FindData(flavor, entry->mFlavor, false);
        pGigEntry->FindData(map, entry->mMapScreen, true);
        pGigEntry->FindData(venue, entry->mVenue, false);
        static Symbol setlist_type("setlist_type"); // bit 18
        std::vector<Symbol> &setlistTypes = entry->mSetlistTypes;
        DataArray *pSetlistTypeArray = pGigEntry->FindArray(setlist_type);
        MILO_ASSERT(pSetlistTypeArray, 0x79);
        MILO_ASSERT(pSetlistTypeArray->Size() == 4, 0x7C);
        for (int j = 1; j < pSetlistTypeArray->Size(); j++) {
            Symbol sym = pSetlistTypeArray->Node(j).Sym();
            setlistTypes.push_back(sym);
        }
        m_vEntries.push_back(entry);
    }
}

Symbol TourDesc::GetName() const { return mName; }

Symbol TourDesc::GetDescription() const { return MakeString("%s_desc", mName); }

Symbol TourDesc::GetWelcome() const { return MakeString("%s_welcome", mName); }

Symbol TourDesc::GetConclusionText() const { return MakeString("%s_conclusion", mName); }

int TourDesc::GetIndex() const { return mIndex; }
int TourDesc::GetNumGigs() const { return m_vEntries.size(); }

TourDescEntry *TourDesc::GetTourDescEntryForGigNum(int i_iGigNum) const {
    MILO_ASSERT_RANGE(i_iGigNum, 0, m_vEntries.size(), 0xB0);
    return m_vEntries[i_iGigNum];
}

int TourDesc::GetNumSongsForGigNum(int num) const {
    TourDescEntry *pEntry = GetTourDescEntryForGigNum(num);
    MILO_ASSERT(pEntry, 0xB9);
    return pEntry->mNumSongs;
}

Symbol TourDesc::GetFilterForGigNum(int num) const {
    TourDescEntry *pEntry = GetTourDescEntryForGigNum(num);
    MILO_ASSERT(pEntry, 0xC2);
    return pEntry->mFilter;
}

Symbol TourDesc::GetSetlistTypeForGigNum(int num, int i_iIndex) const {
    TourDescEntry *pEntry = GetTourDescEntryForGigNum(num);
    MILO_ASSERT(pEntry, 0xCB);
    std::vector<Symbol> &rSetlistTypeVector = pEntry->mSetlistTypes;
    MILO_ASSERT(i_iIndex < rSetlistTypeVector.size(), 0xCE);
    return rSetlistTypeVector[i_iIndex];
}

bool TourDesc::HasSpecificQuest(int num) const {
    return GetSpecificQuestForGigNum(num) != "";
}

bool TourDesc::HasQuestGroup(int num) const { return GetQuestGroupForGigNum(num) != ""; }

bool TourDesc::HasQuestTier(int num) const { return GetQuestTierForGigNum(num) != -1; }

bool TourDesc::HasAnnouncementScreen(int num) const {
    return GetAnnouncementScreenForGigNum(num) != "";
}

int TourDesc::GetQuestTierForGigNum(int num) const {
    TourDescEntry *pEntry = GetTourDescEntryForGigNum(num);
    MILO_ASSERT(pEntry, 0xF0);
    return pEntry->mTier;
}

Symbol TourDesc::GetQuestGroupForGigNum(int num) const {
    TourDescEntry *pEntry = GetTourDescEntryForGigNum(num);
    MILO_ASSERT(pEntry, 0xF9);
    return pEntry->mGroup;
}

Symbol TourDesc::GetSpecificQuestForGigNum(int num) const {
    TourDescEntry *pEntry = GetTourDescEntryForGigNum(num);
    MILO_ASSERT(pEntry, 0x102);
    return pEntry->mQuest;
}

Symbol TourDesc::GetCityForGigNum(int num) const {
    TourDescEntry *pEntry = GetTourDescEntryForGigNum(num);
    MILO_ASSERT(pEntry, 0x10B);
    return pEntry->mCity;
}

Symbol TourDesc::GetAnnouncementScreenForGigNum(int num) const {
    TourDescEntry *pEntry = GetTourDescEntryForGigNum(num);
    MILO_ASSERT(pEntry, 0x114);
    return pEntry->mAnnouncementScreen;
}

Symbol TourDesc::GetFlavorForGigNum(int num) const {
    TourDescEntry *pEntry = GetTourDescEntryForGigNum(num);
    MILO_ASSERT(pEntry, 0x11D);
    return pEntry->mFlavor;
}

Symbol TourDesc::GetMapScreenForGigNum(int num) const {
    TourDescEntry *pEntry = GetTourDescEntryForGigNum(num);
    MILO_ASSERT(pEntry, 0x126);
    return pEntry->mMapScreen;
}

Symbol TourDesc::GetVenueForGigNum(int num) const {
    TourDescEntry *pEntry = GetTourDescEntryForGigNum(num);
    MILO_ASSERT(pEntry, 0x12F);
    return pEntry->mVenue;
}

int TourDesc::GetNumStarsPossibleForTour() const {
    int size = m_vEntries.size();
    int stars = 0;
    for (int i = 0; i < size; i++) {
        stars += GetNumSongsForGigNum(i) * 10;
    }
    return stars;
}

int TourDesc::GetNumSongs() const {
    int size = m_vEntries.size();
    int songs = 0;
    for (int i = 0; i < size; i++) {
        songs += GetNumSongsForGigNum(i);
    }
    return songs;
}

int TourDesc::GetTourStarsBronzeGoalValue() const {
    Accomplishment *pAccomplishment =
        TheAccomplishmentMgr->GetAccomplishment(mTourStarsBronzeGoal);
    MILO_ASSERT(pAccomplishment, 0x157);
    MILO_ASSERT(pAccomplishment->GetType() == kAccomplishmentTypeTourConditional, 0x158);
    AccomplishmentTourConditional *pTourGoal =
        dynamic_cast<AccomplishmentTourConditional *>(pAccomplishment);
    MILO_ASSERT(pTourGoal, 0x15A);
    return pTourGoal->GetTourValue();
}

int TourDesc::GetTourStarsSilverGoalValue() const {
    Accomplishment *pAccomplishment =
        TheAccomplishmentMgr->GetAccomplishment(mTourStarsSilverGoal);
    MILO_ASSERT(pAccomplishment, 0x163);
    MILO_ASSERT(pAccomplishment->GetType() == kAccomplishmentTypeTourConditional, 0x164);
    AccomplishmentTourConditional *pTourGoal =
        dynamic_cast<AccomplishmentTourConditional *>(pAccomplishment);
    MILO_ASSERT(pTourGoal, 0x166);
    return pTourGoal->GetTourValue();
}

int TourDesc::GetTourStarsGoldGoalValue() const {
    Accomplishment *pAccomplishment =
        TheAccomplishmentMgr->GetAccomplishment(mTourStarsGoldGoal);
    MILO_ASSERT(pAccomplishment, 0x16F);
    MILO_ASSERT(pAccomplishment->GetType() == kAccomplishmentTypeTourConditional, 0x170);
    AccomplishmentTourConditional *pTourGoal =
        dynamic_cast<AccomplishmentTourConditional *>(pAccomplishment);
    MILO_ASSERT(pTourGoal, 0x172);
    return pTourGoal->GetTourValue();
}

Symbol TourDesc::GetTourBronzeGoal() const { return mTourStarsBronzeGoal; }
Symbol TourDesc::GetTourSilverGoal() const { return mTourStarsSilverGoal; }
Symbol TourDesc::GetTourGoldGoal() const { return mTourStarsGoldGoal; }

bool TourDesc::HasRequiredCampaignLevel() const {
    Symbol level = mRequiredCampaignLevel;
    return level != gNullStr;
}

Symbol TourDesc::GetRequiredCampaignLevel() const { return mRequiredCampaignLevel; }
Symbol TourDesc::GetLeaderboardGoal() const { return mLeaderboardGoal; }

bool TourDesc::HasLeaderboardGoal() const {
    Symbol goal = mLeaderboardGoal;
    return goal != gNullStr;
}

const char *TourDesc::GetArt() const {
    return MakeString("ui/tour/tour_art/%s_keep.png", mName.Str());
}

const char *TourDesc::GetGrayArt() const {
    const char *gray = MakeString("%s_gray", mName.Str());
    return MakeString("ui/tour/tour_art/%s_keep.png", gray);
}

Symbol TourDesc::GetGigGuideMap() const { return mGigGuideMap; }
