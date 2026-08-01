#include "meta_band/ViewSetting.h"

#include "bandobj/BandLabel.h"
#include "bandobj/CheckboxDisplay.h"
#include "game/BandUser.h"
#include "game/Defines.h"
#include "game/Scoring.h"
#include "meta/Sorting.h"
#include <algorithm>
#include "meta_band/AppLabel.h"
#include "meta_band/BandProfile.h"
#include "meta_band/BandSongMgr.h"
#include "meta_band/MusicLibrary.h"
#include "meta_band/ProfileMgr.h"
#include "net_band/RockCentral.h"
#include "meta_band/SongSortMgr.h"
#include "obj/DataFunc.h"
#include "obj/Dir.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "os/PlatformMgr.h"
#include "rndobj/Dir.h"
#include "rndobj/Draw.h"
#include "rndobj/Mat.h"
#include "ui/UIColor.h"
#include "ui/UIList.h"
#include "ui/UIListCustom.h"
#include "ui/UIListLabel.h"
#include "ui/UIListMesh.h"
#include "ui/UIListSlot.h"
#include "ui/UIListWidget.h"
#include "utl/MakeString.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"

// ------------------------------------------------------------------
// ViewSetting (base)
// ------------------------------------------------------------------

void ViewSetting::InitData(RndDir *dir) {
    if (dir) {
        mEvenMat = dir->Find<RndMat>("bg_even.mat", false);
        mOddMat = dir->Find<RndMat>("bg_odd.mat", false);
    }
}

void ViewSetting::Custom(int, int, UIListCustom *, Hmx::Object *obj) const {
    RndDrawable *d = dynamic_cast<RndDrawable *>(obj);
    MILO_ASSERT(d, 0x2a);
    d->SetShowing(false);
}

RndMat *ViewSetting::Mat(int, int row, UIListMesh *) const {
    if (row % 2 != 0) {
        return mOddMat;
    }
    return mEvenMat;
}

void ViewSetting::Text(int, int, UIListLabel *, UILabel *label) const {
    label->SetTextToken(gNullStr);
}

bool ViewSetting::IsActive(int) const { return true; }

bool ViewSetting::CanSelectMultiple() const { return false; }

void ViewSetting::Reset() { SelectOption(0); }

void ViewSetting::Refresh() {}

bool ViewSetting::IsValid() const { return true; }

bool ViewSetting::IsHeader() const { return false; }

int ViewSetting::StartingOption() const { return 0; }

// ------------------------------------------------------------------
// HeaderViewSetting
// ------------------------------------------------------------------


// ------------------------------------------------------------------
// SortViewSetting
// ------------------------------------------------------------------

const char *SortViewSetting::GetCurrentStatus() const {
    return Localize(TheMusicLibrary->GetCurrentSortName(true), nullptr);
}

// Retail fn_825D4608 (43 instructions) writes ONE short-circuit expression: it
// accumulates into a single scratch register (li r11,1 / li r11,0) and byte-masks
// once at the return (`clrlwi r3,r11,24`). The two bool locals this used to carry
// (`ok`, `signedIn`) cost two extra callee-saved registers -- retail's prologue
// saves only r31 (`std r31,-0x10(r1)`) where ours reached for __savegprlr_29 --
// plus their `li r29,0`/`li r30,0` initialisers. Retail also inlines
// TheRockCentral.IsOnline() to a field compare (`lwz r11,0x38(TheRockCentral)` /
// `cmpwi r11,0x2`), which only happens when it sits inside the same expression.
bool SortViewSetting::IsActive(int idx) const {
    if (idx == 4) {
        // GetPrimaryProfile(), not HasPrimaryProfile(): retail tests the full
        // 32-bit result (`cmplwi r3,0x0`), which is a POINTER test -- a bool
        // return would be byte-masked (`clrlwi. r11,r3,24`). fn_82545DD8 is
        // duly called twice, once per use.
        return TheProfileMgr.GetPrimaryProfile()
            && ThePlatformMgr.IsUserSignedIntoLive(
                   TheProfileMgr.GetPrimaryProfile()->GetAssociatedLocalBandUser()
               )
            && TheRockCentral.IsOnline();
    }
    return true;
}

void SortViewSetting::Text(int, int row, UIListLabel *slot, UILabel *label) const {
    if (slot->Matches("name")) {
        Symbol s = TheSongSortMgr->GetSort((SongSortType)row)->GetName();
        label->SetTextToken(s);
    } else {
        label->SetTextToken(gNullStr);
    }
}

void SortViewSetting::SelectOption(int idx) {
    TheMusicLibrary->SetSort((SongSortType)idx);
}

int SortViewSetting::StartingOption() const {
    return TheMusicLibrary->GetCurrentSortType(true);
}

// ------------------------------------------------------------------
// FilterViewSetting
// ------------------------------------------------------------------

void FilterViewSetting::SetFilterData(const std::map<Symbol, int> &m) {
    mFilters.clear();
    for (std::map<Symbol, int>::const_iterator it = m.begin(); it != m.end();
         ++it) {
        Filter f;
        f.mSym = it->first;
        f.mCount = it->second;
        mFilters.push_back(f);
    }
    std::sort(mFilters.begin(), mFilters.end(), CompareFilters);
}

const char *FilterViewSetting::GetCurrentStatus() const {
    String result;
    SongSortMgr::SongFilter &filter = TheMusicLibrary->GetFilter();
    const std::set<Symbol> &filterSet = filter.GetFilterSet(mFilterType);
    for (std::set<Symbol>::const_iterator it = filterSet.begin();
         it != filterSet.end();
         ++it) {
        if (result.c_str()[0]) {
            result += "/";
        }
        result += Localize(*it, nullptr);
    }
    if (!result.c_str()[0]) {
        static Symbol filter_none("filter_none");
        return Localize(filter_none, nullptr);
    }
    return MakeString(result.c_str());
}

void FilterViewSetting::Reset() {
    TheMusicLibrary->ResetFilter(mFilterType);
}

bool FilterViewSetting::IsValid() const {
    return !TheMusicLibrary->GetFilterLocked();
}

void FilterViewSetting::Custom(int, int idx, UIListCustom *, Hmx::Object *obj)
    const {
    CheckboxDisplay *cb = dynamic_cast<CheckboxDisplay *>(obj);
    MILO_ASSERT(cb, 0x9d);
    cb->SetShowing(true);
    SongSortMgr::SongFilter &filter = TheMusicLibrary->GetFilter();
    MILO_ASSERT(&filter, 0xa1);
    cb->SetChecked(filter.HasFilter(mFilterType, mFilters[idx].mSym));
}

int FilterViewSetting::NumData() const { return mFilters.size(); }

// Retail fn_825D5E88 is only 58 instructions and contains NO DataArray allocation
// and NO Release (lane CF-7, read off the asm). The hand-rolled `new DataArray(2)`
// inherited from the rb3-Wii DEV source was ~60 instructions of pure excess:
// retail calls the TEMPLATED overload ??$SetTokenFmt@PAD@UILabel@@QAAXVSymbol@@PAD@Z
// = SetTokenFmt(Symbol, char*), which builds the DataArrayPtr internally and
// already places the token at Node(0) and the argument at Node(1) -- so the
// ordering bug the old comment documented stays fixed, it is just fixed by the
// library rather than by hand.
//
// Two other retail details:
//  * LocalizeSeparatedInt is called with ONE argument (r3 = count only; no r4 is
//    set up at the call site), i.e. the `LocalizeSeparatedInt(int)` overload, not
//    the `(int, Locale&)` one.
//  * The two function-local statics are initialised BEFORE the count is loaded
//    (guard blocks at 0x825D5ED4-0x825D5F30 precede `lwz r3,0x4(r11)`), so they
//    must be declared ahead of `count`. Guard word lbl_82DFFB04:
//    bit 0x1 = song_select_song (lbl_820B0C14), bit 0x2 = song_select_songs
//    (lbl_820B0C00) -- singular first.
void FilterViewSetting::Text(int, int idx, UIListLabel *slot, UILabel *label)
    const {
    if (slot->Matches("name")) {
        label->SetTextToken(mFilters[idx].mSym);
    } else {
        static Symbol song_select_song("song_select_song");
        static Symbol song_select_songs("song_select_songs");
        int count = mFilters[idx].mCount;
        Symbol fmt = (count == 1) ? song_select_song : song_select_songs;
        label->SetTokenFmt(fmt, LocalizeSeparatedInt(count));
    }
}

void FilterViewSetting::SelectOption(int idx) {
    Filter &f = mFilters[idx];
    TheMusicLibrary->ToggleFilter(mFilterType, f.mSym);
}

bool FilterViewSetting::CompareFilters(const Filter &a, const Filter &b) {
    return AlphaKeyStrCmp(a.mSym.Str(), b.mSym.Str(), true) < 0;
}

Symbol FilterViewSetting::FilterTypeToSym(FilterType ft) {
    static Symbol filter_setting_genres("filter_setting_genres");
    static Symbol filter_setting_decades("filter_setting_decades");
    static Symbol filter_setting_difficulties("filter_setting_difficulties");
    static Symbol filter_setting_lengths("filter_setting_lengths");
    static Symbol filter_setting_ratings("filter_setting_ratings");
    static Symbol filter_setting_sources("filter_setting_sources");
    static Symbol filter_setting_vocal_parts("filter_setting_vocal_parts");
    static Symbol filter_setting_pro_guitar("filter_setting_pro_guitar");
    static Symbol filter_setting_keys("filter_setting_keys");
    switch (ft) {
    case kFilterGenre: return filter_setting_genres;
    case kFilterDecade: return filter_setting_decades;
    case kFilterDifficulty: return filter_setting_difficulties;
    case kFilterLength: return filter_setting_lengths;
    case kFilterRating: return filter_setting_ratings;
    case kFilterSource: return filter_setting_sources;
    case kFilterVocalParts: return filter_setting_vocal_parts;
    case 7: return filter_setting_pro_guitar;
    case 8: return filter_setting_keys;
    default:
        MILO_FAIL("no symbol for FilterType %i", ft);
        return gNullStr;
    }
}

bool FilterViewSetting::CanSelectMultiple() const { return true; }

// ------------------------------------------------------------------
// ScoreTypeViewSetting
// ------------------------------------------------------------------

const char *ScoreTypeViewSetting::GetCurrentStatus() const {
    return Localize(ScoreTypeToSym((ScoreType)mScoreType), nullptr);
}

void ScoreTypeViewSetting::Refresh() {
    mScoreType = TheMusicLibrary->ActiveScoreType();
    MILO_ASSERT(mScoreType != kNumScoreTypes, 0x104);
}

bool ScoreTypeViewSetting::IsValid() const {
    return mScoreType != kScoreBand;
}

void ScoreTypeViewSetting::Text(int, int idx, UIListLabel *slot, UILabel *label)
    const {
    if (slot->Matches("name")) {
        if (idx == 0) {
            label->SetTextToken(ScoreTypeToSym(GetBaseScoreType()));
        } else {
            label->SetTextToken(ScoreTypeToSym(GetAlternateScoreType()));
        }
    } else {
        label->SetTextToken(gNullStr);
    }
}

void ScoreTypeViewSetting::SelectOption(int idx) {
    ScoreType st;
    if (idx == 0) {
        st = GetBaseScoreType();
    } else {
        st = GetAlternateScoreType();
    }
    mScoreType = st;
    TheMusicLibrary->SetTaskScoreType(st);
}

int ScoreTypeViewSetting::StartingOption() const {
    ScoreType base = GetBaseScoreType();
    return mScoreType != base;
}

ScoreType ScoreTypeViewSetting::GetBaseScoreType() const {
    switch (mScoreType) {
    case kScoreBand:
        return kScoreBand;
    case kScoreBass:
    case kScoreGuitar:
        return kScoreGuitar;
    case kScoreDrum:
    case kScoreRealDrum:
        return kScoreDrum;
    case kScoreVocals:
    case kScoreHarmony:
        return kScoreVocals;
    case kScoreKeys:
    case kScoreRealKeys:
        return kScoreKeys;
    case kScoreRealGuitar:
    case kScoreRealBass:
        return kScoreRealGuitar;
    default:
        MILO_FAIL("Bad ScoreType in ScoreTypeViewSetting::GetBaseScoreType!");
        return kNumScoreTypes;
    }
}

ScoreType ScoreTypeViewSetting::GetAlternateScoreType() const {
    switch (mScoreType) {
    case kScoreBand:
        return kScoreBand;
    case kScoreBass:
    case kScoreGuitar:
        return kScoreBass;
    case kScoreDrum:
    case kScoreRealDrum:
        return kScoreRealDrum;
    case kScoreVocals:
    case kScoreHarmony:
        return kScoreHarmony;
    case kScoreKeys:
    case kScoreRealKeys:
        return kScoreRealKeys;
    case kScoreRealGuitar:
    case kScoreRealBass:
        return kScoreRealBass;
    default:
        MILO_FAIL("Bad ScoreType in ScoreTypeViewSetting::GetAlternateScoreType!");
        return kNumScoreTypes;
    }
}

// ------------------------------------------------------------------
// ViewSettingsProvider
// ------------------------------------------------------------------

ViewSettingsProvider::ViewSettingsProvider() : mActiveSetting(nullptr),
    mDisabledColor(nullptr), mHeaderMat(nullptr), mEvenMat(nullptr),
    mOddMat(nullptr) {
    mSettings.push_back(new HeaderViewSetting(options));
    mSettings.push_back(new SortViewSetting());
    mSettings.push_back(new ScoreTypeViewSetting());
    mSettings.push_back(new HeaderViewSetting(filters));
    for (int i = 0; i < kNumFilterTypes - 2; i++) {
        mSettings.push_back(new FilterViewSetting((FilterType)i));
    }
}

ViewSettingsProvider::~ViewSettingsProvider() {
    std::vector<ViewSetting *>::iterator it = mSettings.begin();
    std::vector<ViewSetting *>::iterator end = mSettings.end();
    for (; it != end; ++it) {
        delete *it;
    }
    mSettings.clear();
}

void ViewSettingsProvider::InitData(RndDir *dir) {
    if (dir) {
        mDisabledColor = dir->Find<UIColor>("disabled.color", true);
        mHeaderMat = dir->Find<RndMat>("header.mat", false);
        mEvenMat = dir->Find<RndMat>("bg_even.mat", false);
        mOddMat = dir->Find<RndMat>("bg_odd.mat", false);
    }
}

int ViewSettingsProvider::NumData() const { return mSettings.size(); }

RndMat *ViewSettingsProvider::Mat(int, int row, UIListMesh *) const {
    ViewSetting *setting = mSettings[row];
    if (setting->IsHeader()) {
        return mHeaderMat;
    }
    if (row % 2 != 0) {
        return mOddMat;
    }
    return mEvenMat;
}

void ViewSettingsProvider::Text(int, int row, UIListLabel *slot, UILabel *label)
    const {
    ViewSetting *setting = mSettings[row];
    if (slot->Matches("header") && setting->IsHeader()) {
        label->SetTextToken(setting->GetName());
    } else if (slot->Matches("name") && !setting->IsHeader()) {
        label->SetTextToken(setting->GetName());
    } else if (slot->Matches("status") && !setting->IsHeader()) {
        AppLabel *al = dynamic_cast<AppLabel *>(label);
#ifdef HX_NATIVE
        // 360-ARK milo authors the status slot as a plain UILabel; the Wii
        // path asserts. Fall back to writing the status string via the base
        // UILabel API so view-settings rows show their current value instead
        // of being empty (W6 V1).
        if (al) {
            al->SetViewSettingStatus(setting);
        } else {
            label->SetDisplayText(setting->GetCurrentStatus(), true);
        }
#else
        MILO_ASSERT(al, 0x1e2);
        al->SetViewSettingStatus(setting);
#endif
    } else {
        label->SetTextToken(gNullStr);
    }
}

UIColor *ViewSettingsProvider::SlotColorOverride(
    int, int row, UIListWidget *, UIColor *c
) const {
    if (!mSettings[row]->IsValid() && !mSettings[row]->IsHeader()
        && mDisabledColor) {
        return mDisabledColor;
    }
    return c;
}

bool ViewSettingsProvider::IsActive(int idx) const {
    return !mSettings[idx]->IsHeader();
}

int ViewSettingsProvider::SelectSetting(int idx) {
    ViewSetting *setting = mSettings[idx];
    if (setting->IsValid()) {
        mActiveSetting = mSettings[idx];
        return 1;
    }
    return 0;
}

void ViewSettingsProvider::BuildFilters(Symbol s) {
    std::vector<int> validSongs;
    std::map<Symbol, int> *filterMaps[9];
    for (int i = 0; i < 9; i++) {
        filterMaps[i] = new std::map<Symbol, int>();
    }
    SongSortMgr::SongFilter &filter = TheMusicLibrary->GetFilter();
    for (int ft = 0; ft < 9; ft++) {
        const std::set<Symbol> &fset = filter.GetFilterSet((FilterType)ft);
        for (std::set<Symbol>::const_iterator it = fset.begin();
             it != fset.end();
             ++it) {
            const Symbol tempSym(*it);
            (*filterMaps[ft])[tempSym] = 0;
        }
    }
    Symbol filterSyms[9];
    TheSongMgr.GetRankedSongs(validSongs, true, true);
    for (std::vector<int>::iterator it = validSongs.begin();
         it != validSongs.end();
         ++it) {
        BandSongMetadata *data = (BandSongMetadata *)TheSongMgr.Data(*it);
        int rankTier = TheSongMgr.RankTier(data->Rank(s), s);
        filterSyms[0] = data->Genre();
        filterSyms[1] = data->Decade();
        filterSyms[6] = TheSongMgr.RankTierToken(rankTier);
        filterSyms[7] = data->LengthSym();
        filterSyms[8] = data->RatingSym();
        filterSyms[5] = data->SourceSym();
        filterSyms[4] = data->VocalPartsSym();
        filterSyms[3] = data->HasProGuitarSym();
        filterSyms[2] = data->HasKeysSym();
        for (int ft = 0; ft < 9; ft++) {
            if (ft == 6 && data->Rank(s) == 0.0f) {
                continue;
            }
            const Symbol tempSym(filterSyms[ft]);
            (*filterMaps[ft])[tempSym] += 1;
        }
    }
    for (std::vector<ViewSetting *>::iterator it = mSettings.begin();
         it != mSettings.end();
         ++it) {
        FilterViewSetting *fvs = dynamic_cast<FilterViewSetting *>(*it);
        if (fvs) {
            fvs->SetFilterData(*filterMaps[fvs->mFilterType]);
        }
    }
    for (int i = 0; i < 9; i++) {
        delete filterMaps[i];
    }
}

void ViewSettingsProvider::RefreshAllSettings() {
    for (std::vector<ViewSetting *>::iterator it = mSettings.begin();
         it != mSettings.end();
         ++it) {
        (*it)->Refresh();
    }
}

void ViewSettingsProvider::ResetAllSettings() {
    for (std::vector<ViewSetting *>::iterator it = mSettings.begin();
         it != mSettings.end();
         ++it) {
        (*it)->Reset();
    }
}

void ViewSettingsProvider::ResetActiveSetting() {
    mActiveSetting->Reset();
}

BEGIN_HANDLERS(ViewSettingsProvider)
    HANDLE_ACTION(select_setting_option, mActiveSetting->SelectOption(_msg->Int(2)))
    HANDLE_ACTION(
        set_to_setting_options,
        dynamic_cast<UIList *>(_msg->GetObj(2))->SetProvider(mActiveSetting)
    )
    HANDLE_EXPR(select_setting, SelectSetting(_msg->Int(2)))
    HANDLE_ACTION(
        set_view_setting_to_label,
        dynamic_cast<AppLabel *>(_msg->GetObj(2))->SetViewSetting(mActiveSetting)
    )
    HANDLE_EXPR(can_select_multiple_options, mActiveSetting->CanSelectMultiple())
    HANDLE_ACTION(refresh_all_settings, RefreshAllSettings())
    HANDLE_ACTION(reset_all_settings, ResetAllSettings())
    HANDLE_ACTION(reset_active_setting, ResetActiveSetting())
    HANDLE_EXPR(starting_option, mActiveSetting->StartingOption())
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x280)
END_HANDLERS


// COMDAT-scatter owner-TU includes (sw scatter-scan): retail linker
// interleaved these owners' COMDATs into this TU's .text span.
#define gRev gRev_CriticalUserListener
#define gAltRev gAltRev_CriticalUserListener
#include "band3/meta_band/CriticalUserListener.cpp"
#undef gRev
#undef gAltRev

// sw3 cross-dialect scatter-include (default/band3/meta_band/ViewSetting <- char/CharIKScale.cpp) [Object owner]
#ifndef SW_SCATTER_OWNER_INCLUDE
#define SW_SCATTER_OWNER_INCLUDE
#define gRev gRev_CharIKScale
#define gAltRev gAltRev_CharIKScale
#include "obj/dialect_object_push.h"
#include "char/CharIKScale.cpp"
#include "obj/dialect_object_pop.h"
#undef gRev
#undef gAltRev
#undef SW_SCATTER_OWNER_INCLUDE
#endif
