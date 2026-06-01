#include "tour/QuestFilterPanel.h"
#include "game/NetGameMsgs.h"
#include "meta_band/AccomplishmentManager.h"
#include "meta_band/AppLabel.h"
#include "meta_band/TexLoadPanel.h"
#include "net/NetSession.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "tour/FixedSetlist.h"
#include "tour/QuestManager.h"
#include "tour/Tour.h"
#include "tour/TourDesc.h"
#include "tour/TourPerformer.h"
#include "tour/TourPerformerLocal.h"
#include "tour/TourProgress.h"
#include "ui/UIList.h"
#include "ui/UIListLabel.h"
#include "ui/UIListMesh.h"
#include "ui/UIPanel.h"
#include "utl/MakeString.h"
#include "utl/Messages.h"
#include "utl/Messages2.h"
#include "utl/Messages4.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"

QuestFilterPanel::QuestFilterPanel() : m_symQuest(""), m_pQuestFilterProvider(0) {}

Symbol QuestFilterPanel::GetSelectedFilter() {
    if (GetState() != kUp)
        return "";
    else {
        DataNode handled = Handle(get_selected_filter_index_msg, true);
        int i = handled.Int();
        if (m_pQuestFilterProvider->NumData() > 0) {
            return m_pQuestFilterProvider->DataSymbol(i);
        } else
            return "";
    }
}

UIComponent::State QuestFilterProvider::ComponentStateOverride(int iRow, int iCol, UIComponent::State s) const {
    if (iCol != unk30->SelectedData()) {
        return (UIComponent::State)2;
    }
    return s;
}

inline Symbol QuestFilterProvider::DataSymbol(int i_iData) const {
    MILO_ASSERT_RANGE( i_iData, 0, NumData(), 0xD0);
    return m_vQuestFilters[i_iData];
}

inline String QuestFilterProvider::GetFilterName(int i_iData) const {
    MILO_ASSERT_RANGE( i_iData, 0, NumData(), 0xC6);
    return TheTour->GetFilterName(DataSymbol(i_iData));
}

inline void QuestFilterProvider::UpdateSongLabel(
    UILabel *pAppLabel, Symbol sFilter, TourSetlistType eType, int iSongNum
) const {
    int iNumSongs = m_rProgress.GetNumSongsForCurrentGig();
    if (eType == kTourSetlist_Random) {
        pAppLabel->SetTokenFmt(setlist_song_fmt, iSongNum, tour_random_song);
    } else if (eType == kTourSetlist_Custom) {
        pAppLabel->SetTokenFmt(setlist_song_fmt, iSongNum, tour_custom_song);
    } else if (eType == kTourSetlist_Fixed) {
        FixedSetlist *pFixedSetlist = TheQuestMgr.GetFixedSetlist(sFilter);
        MILO_ASSERT_FMT(pFixedSetlist, "Invalid fixed set list: %s", sFilter.Str());
        MILO_ASSERT(pFixedSetlist->GetNumSongs() == iNumSongs, 0x58);
        Symbol s = pFixedSetlist->GetSongName(iSongNum - 1);
        AppLabel *pLabel = dynamic_cast<AppLabel *>(pAppLabel);
        MILO_ASSERT(pLabel, 0x5C);
        pLabel->SetSongAndArtistNameFromSymbol(s, iSongNum);
    } else {
        MILO_FAIL("Invalid setlist type, Filter = %s", sFilter.Str());
    }
}

inline void QuestFilterProvider::Text(
    int, int i_iData, UIListLabel *i_pSlot, UILabel *i_pLabel
) const {
    MILO_ASSERT(i_iData < NumData(), 0x67);
    TourSetlistType eType;
    Symbol sFilter = DataSymbol(i_iData);
    TourProgress *pProg = TheTour->GetTourProgress();
    if (pProg) {
        TourDesc *pTourDesc = TheTour->GetTourDesc(pProg->GetTourDesc());
        if (pTourDesc) {
            Symbol gigtype =
                pTourDesc->GetSetlistTypeForGigNum(pProg->GetCurrentGigNum(), i_iData);
#ifdef HX_NATIVE
            if (gigtype == Symbol("random")) // `random` collides with POSIX random()
#else
            if (gigtype == random)
#endif
                eType = kTourSetlist_Random;
            else if (gigtype == custom)
                eType = kTourSetlist_Custom;
            else
                goto useFixed;
        } else {
            goto useFixed;
        }
    } else {
useFixed:
        eType = kTourSetlist_Fixed;
    }
    if (i_pSlot->Matches("name")) {
        if (eType == kTourSetlist_Random) {
            i_pLabel->SetTokenFmt(tour_setlist_random, GetFilterName(i_iData));
        } else if (eType == kTourSetlist_Custom) {
            i_pLabel->SetTokenFmt(tour_setlist_custom, GetFilterName(i_iData));
        } else if (eType == kTourSetlist_Fixed) {
            i_pLabel->SetTokenFmt(tour_setlist_fixed, GetFilterName(i_iData));
        } else {
            MILO_ASSERT(false, 0x7E);
        }
    } else if (i_pSlot->Matches("song1")) {
        int iNumSongs = m_rProgress.GetNumSongsForCurrentGig();
        if (iNumSongs <= 0) {
            i_pLabel->SetTextToken(Symbol(gNullStr));
        } else {
            if (eType == kTourSetlist_Random) {
                i_pLabel->SetTokenFmt(setlist_song_fmt, 1, tour_random_song);
            } else if (eType == kTourSetlist_Custom) {
                i_pLabel->SetTokenFmt(setlist_song_fmt, 1, tour_custom_song);
            } else if (eType == kTourSetlist_Fixed) {
                FixedSetlist *pFixedSetlist = TheQuestMgr.GetFixedSetlist(sFilter);
                MILO_ASSERT_FMT(pFixedSetlist, "Invalid fixed set list: %s", sFilter.Str());
                MILO_ASSERT(pFixedSetlist->GetNumSongs() == iNumSongs, 0x58);
                Symbol s = pFixedSetlist->GetSongName(0);
                AppLabel *pLabel = dynamic_cast<AppLabel *>(i_pLabel);
                MILO_ASSERT(pLabel, 0x5C);
                pLabel->SetSongAndArtistNameFromSymbol(s, 1);
            } else {
                MILO_FAIL("Invalid setlist type, Filter = %s", sFilter.Str());
            }
        }
    } else if (i_pSlot->Matches("song2")) {
        int iNumSongs = m_rProgress.GetNumSongsForCurrentGig();
        if (iNumSongs <= 1) {
            i_pLabel->SetTextToken(Symbol(gNullStr));
        } else {
            if (eType == kTourSetlist_Random) {
                i_pLabel->SetTokenFmt(setlist_song_fmt, 2, tour_random_song);
            } else if (eType == kTourSetlist_Custom) {
                i_pLabel->SetTokenFmt(setlist_song_fmt, 2, tour_custom_song);
            } else if (eType == kTourSetlist_Fixed) {
                FixedSetlist *pFixedSetlist = TheQuestMgr.GetFixedSetlist(sFilter);
                MILO_ASSERT_FMT(pFixedSetlist, "Invalid fixed set list: %s", sFilter.Str());
                MILO_ASSERT(pFixedSetlist->GetNumSongs() == iNumSongs, 0x58);
                Symbol s = pFixedSetlist->GetSongName(1);
                AppLabel *pLabel = dynamic_cast<AppLabel *>(i_pLabel);
                MILO_ASSERT(pLabel, 0x5C);
                pLabel->SetSongAndArtistNameFromSymbol(s, 2);
            } else {
                MILO_FAIL("Invalid setlist type, Filter = %s", sFilter.Str());
            }
        }
    } else if (i_pSlot->Matches("song3")) {
        int iNumSongs = m_rProgress.GetNumSongsForCurrentGig();
        if (iNumSongs <= 2) {
            i_pLabel->SetTextToken(Symbol(gNullStr));
        } else {
            if (eType == kTourSetlist_Random) {
                i_pLabel->SetTokenFmt(setlist_song_fmt, 3, tour_random_song);
            } else if (eType == kTourSetlist_Custom) {
                i_pLabel->SetTokenFmt(setlist_song_fmt, 3, tour_custom_song);
            } else if (eType == kTourSetlist_Fixed) {
                FixedSetlist *pFixedSetlist = TheQuestMgr.GetFixedSetlist(sFilter);
                MILO_ASSERT_FMT(pFixedSetlist, "Invalid fixed set list: %s", sFilter.Str());
                MILO_ASSERT(pFixedSetlist->GetNumSongs() == iNumSongs, 0x58);
                Symbol s = pFixedSetlist->GetSongName(2);
                AppLabel *pLabel = dynamic_cast<AppLabel *>(i_pLabel);
                MILO_ASSERT(pLabel, 0x5C);
                pLabel->SetSongAndArtistNameFromSymbol(s, 3);
            } else {
                MILO_FAIL("Invalid setlist type, Filter = %s", sFilter.Str());
            }
        }
    } else {
        i_pLabel->SetTextToken(Symbol(i_pSlot->GetDefaultText()));
    }
}

inline RndMat *QuestFilterProvider::Mat(int, int i_iData, UIListMesh *i_pSlot) const {
    MILO_ASSERT(i_iData < NumData(), 0x95);
    DataSymbol(i_iData);
    TourSetlistType eType;
    TourProgress *pProg = TheTour->GetTourProgress();
    if (pProg) {
        TourDesc *pTourDesc = TheTour->GetTourDesc(pProg->GetTourDesc());
        if (pTourDesc) {
            Symbol gigtype =
                pTourDesc->GetSetlistTypeForGigNum(pProg->GetCurrentGigNum(), i_iData);
#ifdef HX_NATIVE
            if (gigtype == Symbol("random")) // `random` collides with POSIX random()
#else
            if (gigtype == random)
#endif
                eType = kTourSetlist_Random;
            else if (gigtype == custom)
                eType = kTourSetlist_Custom;
            else
                goto useFixedMat;
        } else {
            goto useFixedMat;
        }
    } else {
useFixedMat:
        eType = kTourSetlist_Fixed;
    }
    if (i_pSlot->Matches("icon")) {
        String str;
        switch (eType) {
        case kTourSetlist_Random:
            str = "setlist_random";
            break;
        case kTourSetlist_Custom:
            str = "setlist_custom";
            break;
        case kTourSetlist_Fixed:
            str = "setlist_fixed";
            break;
        default:
            MILO_ASSERT(false, 0xAF);
        }
        std::vector<DynamicTex *>::const_iterator it =
            std::find(m_rIcons.begin(), m_rIcons.end(), str);
        if (it != m_rIcons.end())
            return (*it)->mMat;
        return i_pSlot->DefaultMat();
    }
    return i_pSlot->DefaultMat();
}

TourSetlistType QuestFilterPanel::GetSelectedSetlistType() {
    TourSetlistType ret;
    if (kUp != GetState())
        return kTourSetlist_Invalid;
    DataNode handled = Handle(get_selected_filter_index_msg, true);
    int i = handled.Int();
    if (m_pQuestFilterProvider->NumData() > 0) {
        TourProgress *prog = TheTour->GetTourProgress();
        if (prog) {
            TourDesc *desc = TheTour->GetTourDesc(prog->GetTourDesc());
            if (desc) {
                Symbol gigtype =
                    desc->GetSetlistTypeForGigNum(prog->GetCurrentGigNum(), i);
#ifdef HX_NATIVE
                if (gigtype == Symbol("random")) // `random` collides with POSIX random()
#else
                if (gigtype == random)
#endif
                    ret = kTourSetlist_Random;
                else if (gigtype == custom)
                    ret = kTourSetlist_Custom;
                else
                    ret = kTourSetlist_Fixed;
            } else {
                ret = kTourSetlist_Fixed;
            }
        } else {
            ret = kTourSetlist_Fixed;
        }
        return ret;
    }
    return kTourSetlist_Invalid;
}

void QuestFilterPanel::LoadIcons() {
    const char *artStr = "ui/tour/setlist_art/%s_keep.png";
    AddTex(MakeString(artStr, "setlist_random"), "setlist_random", true, false);
    AddTex(MakeString(artStr, "setlist_custom"), "setlist_custom", true, false);
    AddTex(MakeString(artStr, "setlist_fixed"), "setlist_fixed", true, false);
}

void QuestFilterPanel::Load() {
    TexLoadPanel::Load();
    MILO_ASSERT(!m_pQuestFilterProvider, 0x147);
    LoadIcons();
}

void QuestFilterPanel::FinishLoad() {
    TexLoadPanel::FinishLoad();
    MILO_ASSERT(!m_pQuestFilterProvider, 0x150);
    TourProgress *pProgress = TheTour->GetTourProgress();
    MILO_ASSERT(pProgress, 0x153);
    UIList *pList = mDir->Find<UIList>("filters.lst", true);
    MILO_ASSERT(pList, 0x156);
    m_pQuestFilterProvider = new QuestFilterProvider(mTexs, *pProgress, pList);
}

void QuestFilterPanel::Enter() {
    UIPanel::Enter();
    TourProgress *pProgress = TheTour->GetTourProgress();
    MILO_ASSERT(pProgress, 0x161);
    pProgress->ClearPerformanceState();
    TourPerformerImpl *pPerformer = TheTour->m_pTourPerformer;
    MILO_ASSERT(pPerformer, 0x166);
    Refresh();
}

void QuestFilterPanel::Unload() {
    TexLoadPanel::Unload();
    RELEASE(m_pQuestFilterProvider);
}

void QuestFilterPanel::UpdateFilters() {
    MILO_ASSERT(m_pQuestFilterProvider, 0x17B);
    m_pQuestFilterProvider->Update();
    static Message cUpdateFilterProviderMsg("update_filter_provider", 0);
    cUpdateFilterProviderMsg[0] = m_pQuestFilterProvider;
    Handle(cUpdateFilterProviderMsg, true);
}

void QuestFilterPanel::Refresh() {
    TourProgress *pProgress = TheTour->GetTourProgress();
    MILO_ASSERT(pProgress, 0x187);
    m_symQuest = pProgress->mCurrentQuest;
    UpdateFilters();
    Handle(update_all_msg, true);
}

Symbol QuestFilterPanel::GetBackScreen() {
    MILO_ASSERT(GetState() == kUp, 0x194);
    return Handle(get_backscreen_msg, true).Sym();
}

Symbol QuestFilterPanel::GetSongSelectScreen() {
    MILO_ASSERT(GetState() == kUp, 0x1A0);
    return Handle(get_songselect_screen_msg, true).Sym();
}

Symbol QuestFilterPanel::GetDiffSelectScreen() {
    MILO_ASSERT(GetState() == kUp, 0x1AC);
    return Handle(get_diffselect_screen_msg, true).Sym();
}

void QuestFilterPanel::HandleFilterSelected() {
    TourSetlistType ty = GetSelectedSetlistType();
    TourPerformerLocal *pPerformer =
        dynamic_cast<TourPerformerLocal *>(TheTour->m_pTourPerformer);
    MILO_ASSERT(pPerformer, 0x1BB);
    pPerformer->SetCurrentQuest(m_symQuest);
    Symbol filter = GetSelectedFilter();
    pPerformer->SetCurrentQuestFilter(filter, ty);
    TourProgress *pProgress = TheTour->GetTourProgress();
    MILO_ASSERT(pProgress, 0x1C3);
    int n = pProgress->GetNumSongsForCurrentGig();
    Symbol gigFilter = GetGigFilter();
    TheTour->LaunchQuestFilter(
        n, m_symQuest, filter, gigFilter, ty,
        GetSongSelectScreen(), GetDiffSelectScreen(), GetBackScreen()
    );
}

void QuestFilterPanel::HandleLeaderToggledFilters(bool bShowMode) {
    if (TheNetSession != NULL) {
        TourHideShowFiltersMsg msg(bShowMode);
        TheNetSession->SendMsgToAll(msg, kReliable);
    }
}

Symbol QuestFilterPanel::GetGigFilter() {
    MILO_ASSERT(m_symQuest != "", 0x1D4);
    TourProgress *pProgress = TheTour->GetTourProgress();
    MILO_ASSERT(pProgress, 0x1D8);
    return pProgress->GetFilterForCurrentGig();
}

void QuestFilterPanel::CheatWinQuest() {
    if (m_symQuest != "") {
        TourPerformerImpl *pPerformer;
        TourProgress *pProgress = TheTour->GetTourProgress();
        MILO_ASSERT(pProgress, 0x214);
        pPerformer = TheTour->m_pTourPerformer;
        MILO_ASSERT(pPerformer, 0x217);
        pPerformer->HandleCheatWinQuest(m_symQuest);
        TheQuestMgr.CompleteQuest(pProgress, m_symQuest);
        pProgress->HandleQuestFinished();
        if (pProgress->IsTourComplete()) {
            pPerformer->UpdateCompleteTourStats(pProgress);
        }
        TheAccomplishmentMgr->CheckForFinishedTourAccomplishmentsForUser(
            TheTour->GetUser()
        );
    }
}

void QuestFilterPanel::CheatCycleChallenge() {
    TourPerformerImpl *pPerformer = TheTour->m_pTourPerformer;
    MILO_ASSERT(pPerformer, 0x222);
    MILO_ASSERT(pPerformer->IsLocal(), 0x223);
    TourPerformerLocal *pLocalPerformer = dynamic_cast<TourPerformerLocal *>(pPerformer);
    MILO_ASSERT(pLocalPerformer, 0x225);
    pLocalPerformer->CheatCycleChallenge();
}

void QuestFilterPanel::CheatCycleSetlist() {
    TourPerformerImpl *pPerformer = TheTour->m_pTourPerformer;
    MILO_ASSERT(pPerformer, 0x206);
    MILO_ASSERT(pPerformer->IsLocal(), 0x207);
    TourPerformerLocal *pLocalPerformer = dynamic_cast<TourPerformerLocal *>(pPerformer);
    MILO_ASSERT(pLocalPerformer, 0x209);
    pLocalPerformer->CheatCycleSetlist();
}

int QuestFilterPanel::AreCurrentFiltersValid() {
    TourPerformerImpl *pPerformer = TheTour->m_pTourPerformer;
    MILO_ASSERT(pPerformer, 0x1EC);
    MILO_ASSERT(pPerformer->IsLocal(), 0x1ED);
    TourPerformerLocal *pLocalPerformer = dynamic_cast<TourPerformerLocal *>(pPerformer);
    MILO_ASSERT(pLocalPerformer, 0x1EF);
    return pLocalPerformer->SanityCheckQuestFilters();
}

BEGIN_HANDLERS(QuestFilterPanel)
    HANDLE_ACTION(cheat_win_quest, CheatWinQuest())
    HANDLE_ACTION(cheat_cycle_challenge, CheatCycleChallenge())
    HANDLE_ACTION(cheat_cycle_setlist, CheatCycleSetlist())
    HANDLE_EXPR(update_details, 0)
    HANDLE_ACTION(handle_leader_toggled_filters, HandleLeaderToggledFilters(_msg->Int(2)))
    HANDLE_ACTION(handle_filter_selected, HandleFilterSelected())
    HANDLE_EXPR(are_current_filters_valid, AreCurrentFiltersValid())
    HANDLE_SUPERCLASS(TexLoadPanel)
    HANDLE_CHECK(0x244)
END_HANDLERS
