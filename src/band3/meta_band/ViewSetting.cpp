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

bool SortViewSetting::IsActive(int idx) const {
    if (idx == 4) {
        bool ok = false;
        bool signedIn = false;
        if (TheProfileMgr.HasPrimaryProfile()) {
            BandProfile *prof = TheProfileMgr.GetPrimaryProfile();
            LocalBandUser *user = prof->GetAssociatedLocalBandUser();
            if (ThePlatformMgr.IsUserSignedIntoLive(user)) {
                signedIn = true;
            }
        }
        if (signedIn && TheRockCentral.IsOnline()) {
            ok = true;
        }
        return ok;
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

void FilterViewSetting::Text(int, int idx, UIListLabel *slot, UILabel *label)
    const {
    int _tmp0 = slot->Matches("name");
    if (_tmp0) {
        label->SetTextToken(mFilters[idx].mSym);
    } else {
        int count = mFilters[idx].mCount;
        Symbol fmt = (count == 1) ? song_select_song : song_select_songs;
        DataNode word(fmt);
        DataNode num(LocalizeSeparatedInt(count, TheLocale));
        DataArray *da = new DataArray(2);
        // Node(0) must be the locale-symbol token; Node(1) the substitution arg.
        // SetTokenFmt calls ForceSym(0) to get the symbol to localize, then
        // loops from index 1 onward to feed format arguments. The original code
        // had num first (kDataString) and word second (kDataSymbol), which made
        // ForceSym(0) intern the count string ("30") as a symbol, fail the
        // locale lookup, and show the count instead of "30 Songs".
        da->Node(0) = word;
        da->Node(1) = num;
        label->SetTokenFmt(da);
        da->Release();
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
            (*filterMaps[ft])[*it] = 0;
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
            (*filterMaps[ft])[filterSyms[ft]] += 1;
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
