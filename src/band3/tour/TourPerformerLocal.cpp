#include "tour/TourPerformerLocal.h"
#include "game/BandUserMgr.h"
#include "math/Rand.h"
#include "meta_band/BandSongMetadata.h"
#include "meta_band/MetaPerformer.h"
#include "meta_band/ModifierMgr.h"
#include "meta_band/SongSortMgr.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "os/Timer.h"
#include "tour/GigFilter.h"
#include "tour/FixedSetlist.h"
#include "tour/Quest.h"
#include "tour/QuestManager.h"
#include "tour/Tour.h"
#include "tour/TourDesc.h"
#include "tour/TourPerformer.h"
#include "tour/TourProgress.h"
#include "utl/MakeString.h"
#include "utl/Symbol.h"
#include <vector>

TourPerformerLocal::TourPerformerLocal(BandUserMgr &mgr) : TourPerformerImpl(mgr) {}

TourPerformerLocal::~TourPerformerLocal() {}

void TourPerformerLocal::SyncSave(BinStream &bs, unsigned int) const {
    TourProgress *pProgress = TheTour->GetTourProgress();
    MILO_ASSERT(pProgress, 0x37);
    pProgress->SyncSave(bs);
    bs << mQuestFilter;
    bs << mFilterType;
    int numgigs = mGigData.size();
    bs << numgigs;
    for (int i = 0; i < numgigs; i++) {
        const GigData &gd = mGigData[i];
        bs << gd.unk0;
        bs << gd.unk4;
        bs << gd.unk8;
        bs << gd.unkc;
    }
}

void TourPerformerLocal::MakeDirty() {
    MetaPerformer *pPerformer = MetaPerformer::Current();
    MILO_ASSERT(pPerformer, 0x4E);
    pPerformer->SetSyncDirty(-1, true);
}

void TourPerformerLocal::SelectVenue() {
    MILO_ASSERT(mMetaPerformer, 0x57);
    TourProgress *pProgress = TheTour->GetTourProgress();
    MILO_ASSERT(pProgress, 0x5B);
    static Symbol mod_auto_vocals("mod_auto_vocals");
    bool autovocalson = TheModifierMgr->IsModifierActive(mod_auto_vocals);
    Symbol curvenue = pProgress->GetVenueForCurrentGig();
    if (curvenue != gNullStr && !autovocalson) {
        mMetaPerformer->SetVenue(curvenue);
    } else
        mMetaPerformer->SelectRandomVenue();
}

void TourPerformerLocal::ClearCurrentQuest() {
    TourProgress *pProgress = TheTour->GetTourProgress();
    MILO_ASSERT(pProgress, 0x6E);
    pProgress->SetCurrentQuest("");
    MakeDirty();
}

void TourPerformerLocal::ClearCurrentQuestFilter() {
    TourProgress *pProgress = TheTour->GetTourProgress();
    MILO_ASSERT(pProgress, 0x78);
    pProgress->ClearQuestFilters();
    mQuestFilter = "";
    mFilterType = kTourSetlist_Invalid;
    MakeDirty();
}

void TourPerformerLocal::SetCurrentQuest(Symbol i_symQuest) {
    MILO_ASSERT(TheQuestMgr.HasQuest(i_symQuest), 132);
    TourProgress *pProgress = TheTour->GetTourProgress();
    MILO_ASSERT(pProgress, 135);
    pProgress->SetCurrentQuest(i_symQuest);
    MakeDirty();
}

void TourPerformerLocal::SetCurrentQuestFilter(Symbol quest, TourSetlistType ty) {
    mQuestFilter = quest;
    mFilterType = ty;
    MakeDirty();
}

void TourPerformerLocal::CompleteQuest() {
    TourPerformerImpl::CompleteQuest();
    TourProgress *pProgress = TheTour->GetTourProgress();
    MILO_ASSERT(pProgress, 0x9B);
    TheQuestMgr.CompleteQuest(pProgress, GetCurrentQuest());
    MakeDirty();
}

Symbol TourPerformerLocal::ChooseRandomQuestForGroupAndTier(Symbol group, int tier) {
    TourProgress *pProgress = TheTour->GetTourProgress();
    MILO_ASSERT(pProgress, 166);
    std::vector<Symbol> availableQuests;
    std::map<Symbol, Quest *>::iterator it = TheQuestMgr.mMapQuests.begin();
    float totalWeight = 0.0f;
    for (; it != TheQuestMgr.mMapQuests.end(); ++it) {
        Symbol questSym = it->first;
        if (TheQuestMgr.IsQuestAvailable(*pProgress, questSym, group, tier)) {
            Quest *pQuest = TheQuestMgr.GetQuest(questSym);
            MILO_ASSERT(pQuest, 183);
            totalWeight += pQuest->GetWeight();
            availableQuests.push_back(questSym);
        }
    }
    float roll = RandomFloat(0.0f, totalWeight);
    float cumWeight = 0.0f;
    for (std::vector<Symbol>::iterator it = availableQuests.begin();
         it != availableQuests.end();
         ++it) {
        Symbol chosen = *it;
        Quest *pQuest = TheQuestMgr.GetQuest(chosen);
        MILO_ASSERT(pQuest, 204);
        cumWeight += pQuest->GetWeight();
        if (roll < cumWeight) {
            return chosen;
        }
    }
    MILO_ASSERT(false, 216);
    return Symbol("");
}

bool TourPerformerLocal::InqSongsInFilterData(
    Symbol i_symFilter,
    std::hash_map<Symbol, int> &o_rSongsInFilter,
    std::hash_map<Symbol, int> &o_rSongsWithArtist
) {
    MILO_ASSERT(o_rSongsInFilter.empty(), 0xdf);
    GigFilter *pSecondaryFilter = NULL;
    if (i_symFilter != gNullStr) {
        pSecondaryFilter = TheQuestMgr.GetQuestFilter(i_symFilter);
        MILO_ASSERT(pSecondaryFilter, 0xe5);
    }
    std::vector<int> cSongs;
    std::vector<int> cEmptySongs;
    TheSongMgr.GetValidSongs(
        cSongs, *TheBandUserMgr, cEmptySongs, -1.0f, -1.0f, true, true
    );
    int iHighArtistCount = 0;
    for (std::vector<int>::iterator it = cEmptySongs.begin(); it != cEmptySongs.end();
         ++it) {
        int songID = *it;
        if (pSecondaryFilter) {
            if (!TheSongSortMgr->DoesSongMatchFilter(
                    songID,
                    &pSecondaryFilter->GetFilter(),
                    pSecondaryFilter->GetFilteredPartSym()
                )) {
                continue;
            }
        }
        for (std::map<Symbol, GigFilter *>::iterator fit =
                 TheQuestMgr.mMapQuestFilters.begin();
             fit != TheQuestMgr.mMapQuestFilters.end();
             ++fit) {
            Symbol filterSym = fit->first;
            if (strncmp("tour", filterSym.Str(), 4) == 0) {
                continue;
            }
            GigFilter *pFilter = TheQuestMgr.GetQuestFilter(filterSym);
            MILO_ASSERT(pFilter, 0x10c);
            if (pFilter->IsInternal())
                continue;
            if (!TheSongSortMgr->DoesSongMatchFilter(
                    songID, &pFilter->GetFilter(), pFilter->GetFilteredPartSym()
                ))
                continue;
            int filterCount = 0;
            if (o_rSongsInFilter.find(filterSym) != o_rSongsInFilter.end()) {
                filterCount = o_rSongsInFilter[filterSym];
            }
            o_rSongsInFilter[filterSym] = filterCount + 1;
        }
        BandSongMetadata *pSongData =
            static_cast<BandSongMetadata *>(TheSongMgr.Data(songID));
        MILO_ASSERT(pSongData, 0x124);
        Symbol artist(pSongData->Artist());
        int artistCount;
        if (o_rSongsWithArtist.find(artist) != o_rSongsWithArtist.end()) {
            artistCount = o_rSongsWithArtist[artist];
        } else {
            artistCount = 0;
        }
        int newArtistCount = (o_rSongsWithArtist[artist] = artistCount + 1);
        if (newArtistCount > iHighArtistCount) {
            iHighArtistCount = newArtistCount;
        }
    }
    static Symbol filter_dynamic_artist("filter_dynamic_artist");
    o_rSongsInFilter[filter_dynamic_artist] = iHighArtistCount;
    return true;
}

Symbol TourPerformerLocal::GetRandomArtistFromMap(
    const std::hash_map<Symbol, int> &i_rSongsWithArtist, int i_iNumSongs
) {
    std::vector<Symbol> validArtists;
    for (std::hash_map<Symbol, int>::const_iterator it = i_rSongsWithArtist.begin();
         it != i_rSongsWithArtist.end();
         ++it) {
        Symbol key = it->first;
        if (it->second >= i_iNumSongs) {
            validArtists.push_back(key);
        }
    }
    int idx = RandomInt(0, validArtists.size());
    int count = 0;
    for (std::vector<Symbol>::iterator it = validArtists.begin();
         it != validArtists.end();
         ++it, ++count) {
        if (count == idx) {
            return *it;
        }
    }
    MILO_ASSERT(false, 0x15a);
    return Symbol(gNullStr);
}

Symbol TourPerformerLocal::GetRandomQuestFilter(
    TourProgress *i_pProgress,
    int i_iNumSongs,
    const std::hash_map<Symbol, int> &i_rSongsInFilter,
    const std::hash_map<Symbol, int> &i_rSongsWithArtist
) {
    MILO_ASSERT(i_pProgress, 0x164);
    std::vector<Symbol> validFilters;
    float totalWeight = 0.0f;
    for (std::hash_map<Symbol, int>::const_iterator it = i_rSongsInFilter.begin();
         it != i_rSongsInFilter.end();
         ++it) {
        Symbol filterSym = it->first;
        if (!i_pProgress->HasQuestFilter(filterSym)) {
            if (it->second >= i_iNumSongs) {
                GigFilter *pFilter = TheQuestMgr.GetQuestFilter(filterSym);
                MILO_ASSERT(pFilter, 0x179);
                totalWeight += pFilter->GetWeight();
                validFilters.push_back(filterSym);
            }
        }
    }
    float roll = RandomFloat(0.0f, totalWeight);
    float cumWeight = 0.0f;
    for (std::vector<Symbol>::iterator it = validFilters.begin();
         it != validFilters.end();
         ++it) {
        Symbol filterSym = *it;
        GigFilter *pFilter = TheQuestMgr.GetQuestFilter(filterSym);
        MILO_ASSERT(pFilter, 0x18b);
        cumWeight += pFilter->GetWeight();
        static Symbol filter_dynamic_artist("filter_dynamic_artist");
        if (roll < cumWeight) {
            if (filterSym == filter_dynamic_artist) {
                Symbol artist = GetRandomArtistFromMap(i_rSongsWithArtist, i_iNumSongs);
                filterSym = Symbol(MakeString("filter_artist_%s", artist.Str()));
            }
            return filterSym;
        }
    }
    static Symbol filter_any("filter_any");
    return filter_any;
}

Symbol TourPerformerLocal::GetRandomFixedSetlist(
    TourProgress *i_pProgress, int i_iNumSongs, Symbol i_symFixedSetlistGroup
) {
    MILO_ASSERT(i_pProgress, 0x1a5);
    float totalWeight = 0.0f;
    std::vector<Symbol> validSetlists;
    for (std::map<Symbol, FixedSetlist *>::iterator it =
             TheQuestMgr.mMapFixedSetlists.begin();
         it != TheQuestMgr.mMapFixedSetlists.end();
         ++it) {
        Symbol sym = it->first;
        if (!i_pProgress->HasQuestFilter(sym)) {
            FixedSetlist *pFixedSetlist = TheQuestMgr.GetFixedSetlist(sym);
            MILO_ASSERT(pFixedSetlist, 0x1b8);
            if (pFixedSetlist->GetGroup() == i_symFixedSetlistGroup) {
                int numSongs = pFixedSetlist->GetNumSongs();
                if (numSongs == i_iNumSongs) {
                    totalWeight += pFixedSetlist->GetWeight();
                    validSetlists.push_back(sym);
                }
            }
        }
    }
    float roll = RandomFloat(0.0f, totalWeight);
    float cumWeight = 0.0f;
    for (std::vector<Symbol>::iterator it = validSetlists.begin();
         it != validSetlists.end();
         ++it) {
        Symbol sym = *it;
        FixedSetlist *pFixedSetlist = TheQuestMgr.GetFixedSetlist(sym);
        MILO_ASSERT(pFixedSetlist, 0x1d3);
        cumWeight += pFixedSetlist->GetWeight();
        if (roll < cumWeight) {
            return sym;
        }
    }
    static Symbol filter_any("filter_any");
    return filter_any;
}

void TourPerformerLocal::ChooseQuestFilters() {
    // Retail drops the dev build's `Timer cTimer` / `TheDebug << MakeString(...)`
    // timing instrumentation entirely, and declares `random`/`custom` inside the
    // loop body (their guard tests are at the loop head in the target).
    TourProgress *pProgress = TheTour->GetTourProgress();
    MILO_ASSERT(pProgress, 0x1eb);
    MILO_ASSERT(pProgress->AreQuestFiltersEmpty(), 0x1ec);
    Symbol symSecondaryFilter = pProgress->GetFilterForCurrentGig();
    int iNumSongs = pProgress->GetNumSongsForCurrentGig();
    std::hash_map<Symbol, int> mapSongsInFilter;
    std::hash_map<Symbol, int> mapSongsWithArtist;
    InqSongsInFilterData(symSecondaryFilter, mapSongsInFilter, mapSongsWithArtist);
    for (int i = 0; i < kTour_NumQuestFilters; i++) {
        static Symbol random("random");
        static Symbol custom("custom");
        Symbol setlistType = pProgress->GetSetlistTypeForCurrentGig(i);
        if (setlistType == random) {
            Symbol chosen = GetRandomQuestFilter(
                pProgress, iNumSongs + 3, mapSongsInFilter, mapSongsWithArtist
            );
            pProgress->SetQuestFilter(i, chosen);
        } else if (setlistType == custom) {
            Symbol chosen = GetRandomQuestFilter(
                pProgress, iNumSongs + 3, mapSongsInFilter, mapSongsWithArtist
            );
            pProgress->SetQuestFilter(i, chosen);
        } else {
            Symbol chosen = GetRandomFixedSetlist(pProgress, iNumSongs, setlistType);
            pProgress->SetQuestFilter(i, chosen);
        }
    }
}

bool TourPerformerLocal::SanityCheckFilterAgainstType(Symbol s1, Symbol s2) {
    // NOTE: these MUST stay at function scope -- `random`/`custom` also exist as
    // globals in utl/Symbols*.h, so a block-scoped declaration silently lets the
    // `else` arm bind the global instead (compiles clean, wrong codegen).
    static Symbol random("random");
    static Symbol custom("custom");
    if (TheQuestMgr.HasFixedSetlist(s1)) {
        if (s2 == random || s2 == custom)
            return 0;
    } else {
        if (s2 != random && s2 != custom)
            return 0;
    }
    return 1;
}

int TourPerformerLocal::SanityCheckQuestFilters() {
    GigFilter *pSecondaryFilter;
    TourProgress *pProgress = TheTour->GetTourProgress();
    MILO_ASSERT(pProgress, 0x231);
    MILO_ASSERT(!pProgress->AreQuestFiltersEmpty(), 0x233);
    int questFilterCounts[kTour_NumQuestFilters] = { 0, 0, 0 };
    Symbol filt = pProgress->GetFilterForCurrentGig();
    int numSongs = pProgress->GetNumSongsForCurrentGig();
    pSecondaryFilter = NULL;
    if (filt != gNullStr) {
        pSecondaryFilter = TheQuestMgr.GetQuestFilter(filt);
        MILO_ASSERT(pSecondaryFilter, 0x242);
    }
    std::vector<int> validSongIDs;
    std::vector<int> dummy;
    TheSongMgr.GetValidSongs(
        validSongIDs, *TheBandUserMgr, dummy, -1.0f, -1.0f, true, true
    );
    for (std::vector<int>::iterator it = validSongIDs.begin(); it != validSongIDs.end();
         ++it) {
        int songID = *it;
        if (pSecondaryFilter) {
            if (!TheSongSortMgr->DoesSongMatchFilter(
                    songID,
                    &pSecondaryFilter->GetFilter(),
                    pSecondaryFilter->GetFilteredPartSym()
                )) {
                continue;
            }
        }
        int *pCounts = questFilterCounts;
        for (int i = 0; i < kTour_NumQuestFilters; i++, pCounts++) {
            Symbol questFilter = pProgress->GetQuestFilter(i);
            Symbol setlistType = pProgress->GetSetlistTypeForCurrentGig(i);
            if (!SanityCheckFilterAgainstType(questFilter, setlistType)) {
                return 0;
            }
            GigFilter *pGigFilter = TheQuestMgr.GetQuestFilter(questFilter);
            if (pGigFilter) {
                if (!TheSongSortMgr->DoesSongMatchFilter(
                        songID, &pGigFilter->GetFilter(), pGigFilter->GetFilteredPartSym()
                    )) {
                    continue;
                }
            } else if (!TheQuestMgr.HasFixedSetlist(questFilter)) {
                if (strncmp(questFilter.Str(), "filter_artist_", 14) == 0) {
                    String artistStr(questFilter.Str());
                    String artistSubstr = artistStr.substr(14);
                    BandSongMetadata *pSongData =
                        static_cast<BandSongMetadata *>(TheSongMgr.Data(songID));
                    MILO_ASSERT(pSongData, 0x27c);
                    if (strcmp(pSongData->Artist(), artistSubstr.c_str()) != 0) {
                        continue;
                    }
                } else {
                    MILO_ASSERT(false, 0x286);
                    continue;
                }
            }
            (*pCounts)++;
        }
    }
    int *pCounts = questFilterCounts;
    for (int i = 0; i < kTour_NumQuestFilters; i++, pCounts++) {
        if (*pCounts < numSongs) {
            return 0;
        }
    }
    return 1;
}

void TourPerformerLocal::InitializeNextGig() {
    TourProgress *pProgress = TheTour->GetTourProgress();
    MILO_ASSERT(pProgress, 0x29f);
    pProgress->ClearNewStars();
    pProgress->SetCurrentGigNum(pProgress->GetNumCompletedGigs());
    Symbol currentQuestSym = pProgress->mCurrentQuest;
    if (currentQuestSym != gNullStr) {
        if (!SanityCheckQuestFilters()) {
            pProgress->ClearQuestFilters();
            ChooseQuestFilters();
            mMetaPerformer->SetSyncDirty(-1, true);
        }
    } else {
        Symbol symQuest = gNullStr;
        Symbol tourDescSym = pProgress->GetTourDesc();
        TourDesc *pTourDesc = TheTour->GetTourDesc(tourDescSym);
        if (!pTourDesc)
            return;
        MILO_ASSERT(pTourDesc, 0x2be);
        int currentGigNum = pProgress->GetCurrentGigNum();
        if (pTourDesc->HasSpecificQuest(currentGigNum)) {
            symQuest = pTourDesc->GetSpecificQuestForGigNum(currentGigNum);
        } else if (pTourDesc->HasQuestTier(currentGigNum)) {
            int tier = pTourDesc->GetQuestTierForGigNum(currentGigNum);
            symQuest = ChooseRandomQuestForGroupAndTier(Symbol(""), tier);
        } else if (pTourDesc->HasQuestGroup(currentGigNum)) {
            Symbol group = pTourDesc->GetQuestGroupForGigNum(currentGigNum);
            symQuest = ChooseRandomQuestForGroupAndTier(group, -1);
        } else {
            MILO_ASSERT(false, 0x2d1);
        }
        MILO_ASSERT(symQuest != gNullStr, 0x2d4);
        pProgress->SetCurrentQuest(symQuest);
        ChooseQuestFilters();
        mMetaPerformer->SetSyncDirty(-1, true);
    }
}

void TourPerformerLocal::CheatCycleChallenge() {
    std::vector<Symbol> availableQuests;
    unsigned int currentIdx = 0;
    for (std::map<Symbol, Quest *>::iterator it = TheQuestMgr.mMapQuests.begin();
         it != TheQuestMgr.mMapQuests.end();
         ++it) {
        Symbol questSym = it->first;
        if (questSym == GetCurrentQuest()) {
            currentIdx = availableQuests.size();
        }
        availableQuests.push_back(questSym);
    }
    unsigned int nextIdx = currentIdx + 1;
    if (nextIdx >= availableQuests.size())
        nextIdx = 0;
    Symbol picked = availableQuests[nextIdx];
    SetCurrentQuest(picked);
}

void TourPerformerLocal::CheatCycleSetlist() {
    static Symbol filter_dynamic_artist("filter_dynamic_artist");
    static Symbol random("random");
    static Symbol custom("custom");
    TourProgress *pProgress = TheTour->GetTourProgress();
    MILO_ASSERT(pProgress, 0x301);
    Symbol symFilter = pProgress->GetFilterForCurrentGig();
    int iNumSongs = pProgress->GetNumSongsForCurrentGig();
    if (pProgress->AreQuestFiltersEmpty()) {
        return;
    }
    std::hash_map<Symbol, int> mapSongsInFilter;
    unsigned int filterIndices[kTour_NumQuestFilters];
    std::hash_map<Symbol, int> mapSongsWithArtist;
    InqSongsInFilterData(symFilter, mapSongsInFilter, mapSongsWithArtist);
    filterIndices[0] = 0;
    filterIndices[1] = 0;
    filterIndices[2] = 0;
    std::vector<Symbol> validFilters;
    for (std::hash_map<Symbol, int>::iterator it = mapSongsInFilter.begin();
         mapSongsInFilter.end() != it;
         ++it) {
        Symbol current = it->first;
        if (mapSongsInFilter.find(current) != mapSongsInFilter.end()
            && mapSongsInFilter[current] >= iNumSongs) {
            unsigned int *pIndices = filterIndices;
            for (int i = 0; i < kTour_NumQuestFilters; i++, pIndices++) {
                if (current == filter_dynamic_artist) {
                    Symbol artist = GetRandomArtistFromMap(mapSongsWithArtist, iNumSongs);
                    current = Symbol(MakeString("filter_artist_%s", artist.Str()));
                }
                if (current == pProgress->GetQuestFilter(i)) {
                    *pIndices = validFilters.size();
                }
            }
            validFilters.push_back(current);
        }
    }
    unsigned int *pIndices = filterIndices;
    for (int i = 0; i < kTour_NumQuestFilters; i++, pIndices++) {
        Symbol setlistType = pProgress->GetSetlistTypeForCurrentGig(i);
        if (pProgress && setlistType == random || setlistType == custom) {
            unsigned int nextIdx = *pIndices + 1;
            if (nextIdx >= validFilters.size())
                nextIdx = 0;
            Symbol picked = validFilters[nextIdx];
            pProgress->SetQuestFilter(i, picked);
        } else {
            std::vector<Symbol> validSetlists;
            for (std::map<Symbol, FixedSetlist *>::iterator it =
                     TheQuestMgr.mMapFixedSetlists.begin();
                 it != TheQuestMgr.mMapFixedSetlists.end();
                 ++it) {
                Symbol sym = it->first;
                FixedSetlist *pFixedSetlist = TheQuestMgr.GetFixedSetlist(sym);
                MILO_ASSERT(pFixedSetlist, 0x353);
                if (pFixedSetlist->GetGroup() == setlistType) {
                    int numSongs = pFixedSetlist->GetNumSongs();
                    if (numSongs == iNumSongs) {
                        validSetlists.push_back(sym);
                    }
                }
            }
            unsigned int nextIdx = *pIndices + 1;
            if (nextIdx >= validSetlists.size())
                nextIdx = 0;
            Symbol picked = validSetlists[nextIdx];
            pProgress->SetQuestFilter(i, picked);
        }
    }
}

BEGIN_HANDLERS(TourPerformerLocal)
    HANDLE_ACTION(select_venue, SelectVenue())
    HANDLE_ACTION(initialize_next_gig, InitializeNextGig())
    HANDLE_SUPERCLASS(TourPerformerImpl)
    HANDLE_CHECK(889)
END_HANDLERS
