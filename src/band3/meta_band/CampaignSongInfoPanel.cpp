#include "meta_band/CampaignSongInfoPanel.h"
#include "BandProfile.h"
#include "Campaign.h"
#include "MusicLibrary.h"
#include "SongSortMgr.h"
#include "SongStatusMgr.h"
#include "Utl.h"
#include "game/Defines.h"
#include "meta_band/BandSongMgr.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "ui/UILabel.h"
#include "ui/UIList.h"
#include "ui/UIListLabel.h"
#include "ui/PanelDir.h"
#include "ui/UIPanel.h"
#include "utl/Messages.h"
#include "utl/Messages2.h"
#include "utl/Messages3.h"
#include "utl/Messages4.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"

CampaignSongInfoPanel::CampaignSongInfoPanel() : mCampaignSourceProvider(0) {}

Symbol CampaignSongInfoPanel::SelectedSource() const {
    if (GetState() != kUp) {
        return "";
    } else {
        UIList *pSourcesList = mDir->Find<UIList>("sources.lst", true);
        MILO_ASSERT(pSourcesList, 0x71);
        return pSourcesList->SelectedSym(true);
    }
}

ScoreType CampaignSongInfoPanel::SelectedScoreType() const {
    if (GetState() != kUp) {
        return kScoreBand;
    } else {
        UIList *pInstrumentsList = mDir->Find<UIList>("instruments.lst", true);
        MILO_ASSERT(pInstrumentsList, 0x7F);
        // Retail reads the Symbol back out of its OWN stack slot
        // (lwz r3, 0x50(r1)) rather than dereferencing the sret pointer the
        // call returns (lwz r3, 0x0(r3)), so the temporary must be NAMED.
        // Same shape as GetCareerScore's `Symbol src = SelectedSource();`
        // below, which is at 100% and reads lwz r5, 0x50(r31).
        Symbol sym = pInstrumentsList->SelectedSym(true);
        return SymToScoreType(sym);
    }
}

inline void CampaignSourceProvider::Update() {
    unk20.clear();
    std::set<Symbol> srcs;
    TheSongMgr.InqAvailableSongSources(srcs);
    if (srcs.size() > 1) {
        // Function-local static, as retail 0x825F64E8 (guard 0x82E0041C,
        // storage 0x82E00418), tested only on the size > 1 path.
        static Symbol all("all");
        unk20.push_back(all);
    }
    for (std::set<Symbol>::iterator it = srcs.begin(); it != srcs.end(); ++it) {
        Symbol cur = *it;
        unk20.push_back(cur);
    }
}

void CampaignSongInfoPanel::Refresh() {
    MILO_ASSERT(mCampaignSourceProvider, 0x88);
    mCampaignSourceProvider->Update();
    UIList *pSourceList = mDir->Find<UIList>("sources.lst", true);
    MILO_ASSERT(pSourceList, 0x8D);
    pSourceList->SetProvider(mCampaignSourceProvider);
    // Retail 0x825F6680 initialises each local static at its point of use: the
    // two share one guard word (0x82E00430), bit 0 tested after SetProvider
    // (storage 0x82E00428, "refresh_instrument_list" at 0x820BBDD8) and bit 1
    // after SelectDefaultInstrument (storage 0x82E00420, "update_details").
    static Message refresh_instrument_list_msg("refresh_instrument_list");
    Handle(refresh_instrument_list_msg, true);
    SelectDefaultInstrument();
    static Message update_details_msg("update_details");
    Handle(update_details_msg, true);
}

void CampaignSongInfoPanel::SelectDefaultInstrument() {
    LocalBandUser *pUser = TheCampaign->GetUser();
    MILO_ASSERT(pUser, 0x9F);
    TrackType trackType = ControllerTypeToTrackType(pUser->GetControllerType(), false);
    Symbol scoreTypeSym = ScoreTypeToSym(TrackTypeToScoreType(trackType, false, false));
    UIList *pInstrumentList = mDir->Find<UIList>("instruments.lst", true);
    MILO_ASSERT(pInstrumentList, 0xA6);
    pInstrumentList->SetSelected(scoreTypeSym, true, -1);
    // Function-local static Message, as retail 0x825F6090 (guard 0x82E003FC,
    // storage 0x82E003F4, atexit 0x82C46D68), not the extern from Messages*.h.
    static Message update_details_msg("update_details");
    Handle(update_details_msg, true);
}

void CampaignSongInfoPanel::Enter() {
    UIPanel::Enter();
    MILO_ASSERT(!mCampaignSourceProvider, 0xB3);
    mCampaignSourceProvider = new CampaignSourceProvider();
    Refresh();
}

void CampaignSongInfoPanel::Unload() {
    UIPanel::Unload();
    // Releases the provider Enter() creates, per rb3-Wii. Retail's body for
    // THIS class is 0x825F58C8 (slot 33 of the vtable at 0x820BB894, COL
    // .?AVCampaignSongInfoPanel@@): lwz/stw 0x3c -- word-identical to ours.
    // The 0x44-slot body at 0x8261FEB8 the map used to pair us with is
    // MainHubPanel::Unload (slot 11 of the vtable at 0x820C5ED4, COL
    // .?AVMainHubPanel@@); see the note on mCampaignSourceProvider in the header.
    RELEASE(mCampaignSourceProvider);
}

void CampaignSongInfoPanel::Load() { UIPanel::Load(); }

int CampaignSongInfoPanel::GetCareerScore() const {
    ScoreType ty = SelectedScoreType();
    Symbol src = SelectedSource();
    BandProfile *pProfile = TheCampaign->GetProfile();
    MILO_ASSERT(pProfile, 0xCD);
    SongStatusMgr *pSongStatusMgr = pProfile->GetSongStatusMgr();
    MILO_ASSERT(pSongStatusMgr, 0xCF);
    // Retail materialises a FUNCTION-LOCAL `static Symbol all("all")` in every
    // method of this TU that names `all` (guard word + storage in .bss, built via
    // ??0Symbol@@QAA@PBD@Z from the .rdata literal at 0x82014840, guard rolled
    // back in the EH funclet), not the extern `all` from Symbols2.h that the
    // rb3-Wii dev source uses. Retail guard/storage: 0x82E003D0 / 0x82E003CC (fn 0x825F5C60). The oracle is wrong
    // here and the retail bytes are right (same finding as GoalCmp, W16-AI).
    static Symbol all("all");
    if (src == all) {
        return pSongStatusMgr->CalculateTotalScore(ty, gNullStr);
    } else {
        return pSongStatusMgr->CalculateTotalScore(ty, src);
    }
}

int CampaignSongInfoPanel::GetSongCount() const {
    Symbol src = SelectedSource();
    ScoreType ty = SelectedScoreType();
    BandProfile *pProfile = TheCampaign->GetProfile();
    MILO_ASSERT(pProfile, 0xE3);
    SongStatusMgr *pSongStatusMgr = pProfile->GetSongStatusMgr();
    MILO_ASSERT(pSongStatusMgr, 0xE5);
    // Retail materialises a FUNCTION-LOCAL `static Symbol all("all")` in every
    // method of this TU that names `all` (guard word + storage in .bss, built via
    // ??0Symbol@@QAA@PBD@Z from the .rdata literal at 0x82014840, guard rolled
    // back in the EH funclet), not the extern `all` from Symbols2.h that the
    // rb3-Wii dev source uses. Retail guard/storage: 0x82E003D8 / 0x82E003D4 (fn 0x825F5D30). The oracle is wrong
    // here and the retail bytes are right (same finding as GoalCmp, W16-AI).
    static Symbol all("all");
    if (src == all) {
        return pSongStatusMgr->GetTotalSongs(ty, gNullStr);
    } else {
        return pSongStatusMgr->GetTotalSongs(ty, src);
    }
}

int CampaignSongInfoPanel::GetSongsCompleted(Difficulty diff) const {
    ScoreType ty = SelectedScoreType();
    Symbol src = SelectedSource();
    BandProfile *pProfile = TheCampaign->GetProfile();
    MILO_ASSERT(pProfile, 0xF9);
    SongStatusMgr *pSongStatusMgr = pProfile->GetSongStatusMgr();
    MILO_ASSERT(pSongStatusMgr, 0xFB);
    // Retail materialises a FUNCTION-LOCAL `static Symbol all("all")` in every
    // method of this TU that names `all` (guard word + storage in .bss, built via
    // ??0Symbol@@QAA@PBD@Z from the .rdata literal at 0x82014840, guard rolled
    // back in the EH funclet), not the extern `all` from Symbols2.h that the
    // rb3-Wii dev source uses. Retail guard/storage: 0x82E003DC / 0x82E003E0 (fn 0x825F5E08). The oracle is wrong
    // here and the retail bytes are right (same finding as GoalCmp, W16-AI).
    static Symbol all("all");
    if (src == all) {
        return pSongStatusMgr->GetCompletedSongs(ty, diff, gNullStr);
    } else {
        return pSongStatusMgr->GetCompletedSongs(ty, diff, src);
    }
}

int CampaignSongInfoPanel::GetStarCount() const {
    Symbol src = SelectedSource();
    ScoreType ty = SelectedScoreType();
    BandProfile *pProfile = TheCampaign->GetProfile();
    MILO_ASSERT(pProfile, 0x10F);
    SongStatusMgr *pSongStatusMgr = pProfile->GetSongStatusMgr();
    MILO_ASSERT(pSongStatusMgr, 0x111);
    // Retail materialises a FUNCTION-LOCAL `static Symbol all("all")` in every
    // method of this TU that names `all` (guard word + storage in .bss, built via
    // ??0Symbol@@QAA@PBD@Z from the .rdata literal at 0x82014840, guard rolled
    // back in the EH funclet), not the extern `all` from Symbols2.h that the
    // rb3-Wii dev source uses. Retail guard/storage: 0x82E003E4 / 0x82E003E8 (fn 0x825F5EE0). The oracle is wrong
    // here and the retail bytes are right (same finding as GoalCmp, W16-AI).
    static Symbol all("all");
    if (src == all) {
        return pSongStatusMgr->GetPossibleStars(ty, gNullStr);
    } else {
        return pSongStatusMgr->GetPossibleStars(ty, src);
    }
}

int CampaignSongInfoPanel::GetStarsEarned(Difficulty diff) const {
    ScoreType ty = SelectedScoreType();
    Symbol src = SelectedSource();
    BandProfile *pProfile = TheCampaign->GetProfile();
    MILO_ASSERT(pProfile, 0x125);
    SongStatusMgr *pSongStatusMgr = pProfile->GetSongStatusMgr();
    MILO_ASSERT(pSongStatusMgr, 0x127);
    // Retail materialises a FUNCTION-LOCAL `static Symbol all("all")` in every
    // method of this TU that names `all` (guard word + storage in .bss, built via
    // ??0Symbol@@QAA@PBD@Z from the .rdata literal at 0x82014840, guard rolled
    // back in the EH funclet), not the extern `all` from Symbols2.h that the
    // rb3-Wii dev source uses. Retail guard/storage: 0x82E003EC / 0x82E003F0 (fn 0x825F5FB8). The oracle is wrong
    // here and the retail bytes are right (same finding as GoalCmp, W16-AI).
    static Symbol all("all");
    if (src == all) {
        return pSongStatusMgr->GetTotalBestStars(ty, diff, gNullStr);
    } else {
        return pSongStatusMgr->GetTotalBestStars(ty, diff, src);
    }
}

const char *CampaignSongInfoPanel::GetInstrumentIcon() {
    return GetFontCharFromScoreType(SelectedScoreType(), 0);
}

Symbol CampaignSongInfoPanel::GetMusicLibraryBackScreen() {
    MILO_ASSERT(GetState() == kUp, 0x13E);
    // Function-local static Message, as retail 0x825F61C8 (guard 0x82E00408,
    // storage 0x82E00400, literal 0x820BBB88, atexit 0x82C46D88).
    static Message get_musiclibrary_backscreen_msg("get_musiclibrary_backscreen");
    DataNode handled = Handle(get_musiclibrary_backscreen_msg, true);
    return handled.Sym();
}

Symbol CampaignSongInfoPanel::GetMusicLibraryNextScreen() {
    MILO_ASSERT(GetState() == kUp, 0x14A);
    // Function-local static Message, as retail 0x825F62D8 (guard 0x82E00414,
    // storage 0x82E0040C, literal 0x820BBC08, atexit 0x82C46DA8).
    static Message get_musiclibrary_nextscreen_msg("get_musiclibrary_nextscreen");
    DataNode handled = Handle(get_musiclibrary_nextscreen_msg, true);
    return handled.Sym();
}

void CampaignSongInfoPanel::CreateAndSubmitMusicLibraryTask() {
    Symbol src = SelectedSource();
    SongSortMgr::SongFilter filt;
    MusicLibrary::MusicLibraryTask task;
    // Function-local static, as retail 0x825F68C0 (guard 0x82E00438, storage
    // 0x82E00434), initialised after both ctors and before the compare.
    static Symbol all("all");
    if (src != all) {
        filt.AddFilter((FilterType)5, src);
        task.filter = filt;
    }
    task.allowDuplicates = false;
    task.backScreen = GetMusicLibraryBackScreen();
    task.nextScreen = GetMusicLibraryNextScreen();
    TheMusicLibrary->SetTask(task);
}

void CampaignSongInfoPanel::Launch() {
    CreateAndSubmitMusicLibraryTask();
    // Function-local static Message, as retail 0x825F6A28 (guard 0x82E00444,
    // storage 0x82E0043C, literal 0x820BBF00, atexit 0x82C46DC8), initialised
    // after CreateAndSubmitMusicLibraryTask.
    static Message handle_goto_musiclibrary_msg("handle_goto_musiclibrary");
    Handle(handle_goto_musiclibrary_msg, true);
}

BEGIN_HANDLERS(CampaignSongInfoPanel)
    HANDLE_EXPR(get_career_score, GetCareerScore())
    HANDLE_EXPR(get_song_count, GetSongCount())
    HANDLE_EXPR(get_songs_completed, GetSongsCompleted((Difficulty)_msg->Int(2)))
    HANDLE_EXPR(get_star_count, GetStarCount())
    HANDLE_EXPR(get_stars_earned, GetStarsEarned((Difficulty)_msg->Int(2)))
    HANDLE_EXPR(get_instrument_icon, GetInstrumentIcon())
    HANDLE_ACTION(launch, Launch())
    HANDLE_SUPERCLASS(UIPanel)
    HANDLE_CHECK(0x18A)
END_HANDLERS

inline Symbol CampaignSourceProvider::DataSymbol(int i_iData) const {
    MILO_ASSERT(i_iData < NumData(), 0x55);
    return unk20[i_iData];
}

inline int CampaignSourceProvider::NumData() const { return unk20.size(); }

inline void
CampaignSourceProvider::Text(int, int i_iData, UIListLabel *slot, UILabel *label) const {
    MILO_ASSERT(i_iData < NumData(), 0x41);
    Symbol sym = DataSymbol(i_iData);
    if (slot->Matches("name")) {
        label->SetTextToken(sym);
    } else {
        label->SetTextToken(gNullStr);
    }
}
