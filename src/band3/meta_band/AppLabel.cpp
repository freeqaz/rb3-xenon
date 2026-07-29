#include "meta_band/AppLabel.h"
#include "bandobj/BandTrack.h"
#include "decomp.h"
#include "game/BandUser.h"
#include "game/Scoring.h"
#include "macros.h"
#include "meta/StoreOffer.h"
#include "meta_band/BandMachineMgr.h"
#include "meta_band/BandProfile.h"
#include "meta_band/BandSongMetadata.h"
#include "meta_band/BandSongMgr.h"
#include "meta_band/MainHubMessageProvider.h"
#include "meta_band/MainHubPanel.h"
#include "meta_band/MusicLibrary.h"
#include "meta_band/MetaPerformer.h"
#include "meta_band/ProfileMgr.h"
#include "meta_band/SavedSetlist.h"
#include "meta_band/SessionMgr.h"
#include "meta_band/SongRecord.h"
#include "meta_band/SongSortNode.h"
#include "meta_band/BandStorePanel.h"
#include "meta_band/StoreInfoPanel.h"
#include "meta_band/StoreMainPanel.h"
#include "meta_band/StoreMenuPanel.h"
#include "meta_band/StoreOfferProvider.h"
#include "meta_band/ViewSetting.h"
#include "meta_band/Utl.h"
#include "obj/Dir.h"
#include "obj/Data.h"
#include "obj/ObjMacros.h"
#include "os/DateTime.h"
#include "os/Debug.h"
#include "os/PlatformMgr.h"
#include "os/System.h"
#include "ui/UIListSlot.h"
#include "ui/UIPanel.h"
#include "utl/Locale.h"
#include "utl/LocaleOrdinal.h"
#include "utl/MakeString.h"
#include "utl/Messages.h"
#include "utl/Messages2.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"

DECOMP_FORCEACTIVE(
    AppLabel,
    "%s) %s",
    "heading",
    "slot_heading",
    "slot_left",
    "slot_right",
    "slot_centered",
    "AppLabel.cpp",
    "profile",
    "<alt>%s</alt> %s",
    "%s %s",
    "ssn",
    "%i (%i)",
    "p",
    "Could not set user name, unknown class",
    "",
    "pProfile",
    "pMachineMgr",
    "<alt>%s</alt> %s (%s %s)",
    "panel",
    "practice_mbt",
    "%s (%d:%d:%d)",
    "!e->mOffer",
    "bsp",
    "pUser",
    "pitch %d doesn't map to a white key",
    "linking code is not 10 characters!\n",
    "linking code has spaces!\n",
    "setting",
    "%s",
    "%s%s",
    "smp",
    "store_browser_panel",
    "sbp",
    "%s::%s",
    "Bad ScoreType in AppLabel::SetBattleInstrumentString!",
    "setlist",
    "ratingIcons",
    "%s(%d): %s unhandled msg: %s"
)

AppLabel::~AppLabel() {}

bool UILabel::CanHaveFocus() { return false; }

void AppLabel::SetLeaderboardName(const LeaderboardRow &lb) {
    if (lb.mUnnamedBand) {
        static Symbol band_default_name("band_default_name");
        SetTokenFmt(band_default_name, lb.mName.c_str());
    } else {
        SetDisplayText(lb.mName.c_str(), true);
    }
}

void AppLabel::SetLeaderboardRankAndName(const LeaderboardRow &lb) {
    if (lb.mUnnamedBand) {
        SetDisplayText(
            MakeString(
                "%s) %s",
                LocalizeSeparatedInt(lb.mRank),
                MakeString(Localize(band_default_name, nullptr), lb.mName.c_str())
            ),
            true
        );
    } else {
        SetDisplayText(
            MakeString("%s) %s", LocalizeSeparatedInt(lb.mRank), lb.mName.c_str()),
            true
        );
    }
}

void AppLabel::SetCreditsText(DataArray *arr, UIListSlot *slot) {
    static Symbol heading("heading");
    static Symbol image("image");
    static Symbol blank("blank");
    static Symbol heading2("heading2");
    static Symbol title_name("title_name");
    static Symbol centered("centered");
    Symbol sym = blank;
    if (0 != arr->Size()) {
        sym = arr->Sym(0);
    }
    if (sym == blank) {
        SetTextToken(gNullStr);
    } else if (heading == sym) {
        if (slot->Matches("heading")) {
            SetDisplayText(arr->Str(1), true);
        } else
            SetTextToken(gNullStr);
    } else if (heading2 == sym) {
        if (slot->Matches("slot_heading")) {
            SetDisplayText(arr->Str(1), true);
        } else
            SetTextToken(gNullStr);
    } else if (title_name == sym) {
        if (slot->Matches("slot_left")) {
            SetDisplayText(arr->Str(1), true);
        } else if (slot->Matches("slot_right")) {
            SetDisplayText(arr->Str(2), true);
        } else
            SetTextToken(gNullStr);
    } else if (centered == sym) {
        if (slot->Matches("slot_centered")) {
            SetDisplayText(arr->Str(1), true);
        } else
            SetTextToken(gNullStr);
    } else if (image == sym) {
        SetTextToken(gNullStr);
    } else
        SetTextToken(gNullStr);
}

void AppLabel::SetUserName(const User *user) { SetDisplayText(user->UserName(), true); }

void AppLabel::SetUserName(int i) { SetDisplayText(ThePlatformMgr.GetName(i), true); }

void AppLabel::SetUserName(BandTrack *track) { SetDisplayText(track->UserName(), true); }

void AppLabel::SetIntroName(BandUser *user) { SetDisplayText(user->IntroName(), true); }

void AppLabel::SetProfileName(const LocalBandUser *user) {
    if (user) {
        SetDisplayText(user->ProfileName(), true);
    }
}

void AppLabel::SetIconAndProfileName(ScoreType ty, const BandProfile *profile) {
    MILO_ASSERT(profile, 0xAD);
    SetDisplayText(
        MakeString(
            "<alt>%s</alt> %s", GetFontCharFromScoreType(ty, 0), profile->GetName()
        ),
        true
    );
}

void AppLabel::SetFormattedProfileName(Symbol s, BandUser *user) {
    SetTokenFmt(s, user->ProfileName());
}

void AppLabel::SetSongName(Symbol shortname, bool fail) {
    int songID = TheSongMgr.GetSongIDFromShortName(shortname, fail);
    SetSongNameWithNumber(songID, 0, 0);
}

void AppLabel::SetSongNameWithNumber(int songID, int i2, const char *cc) {
    BandSongMetadata *data = (BandSongMetadata *)TheSongMgr.Data(songID);
    const char *text;
    if (data) {
        text = data->Title();
    } else if (cc != 0) {
        text = cc;
    } else {
        static Symbol unknown_song("unknown_song");
        text = Localize(unknown_song, 0);
    }
    if (i2 > 0) {
        static Symbol setlist_song_fmt("setlist_song_fmt");
        SetTokenFmt(setlist_song_fmt, i2, text);
    } else
        SetDisplayText(text, true);
}

void AppLabel::SetSongAndArtistNameFromSymbol(Symbol shortname, int i) {
    int songID = TheSongMgr.GetSongIDFromShortName(shortname, true);
    String artistStr;
    String titleStr;
    BandSongMetadata *data = (BandSongMetadata *)TheSongMgr.Data(songID);
    if (!data) {
        static Symbol unknown_song("unknown_song");
        titleStr = Localize(unknown_song, 0);
    } else {
        static Symbol store_famous_by("store_famous_by");
        titleStr = data->Title();
        if (!data->IsMasterRecording()) {
            artistStr = MakeString("%s %s", Localize(store_famous_by, 0), data->Artist());
        } else {
            artistStr = data->Artist();
        }
    }
    if (i <= 0) {
        static Symbol song_artist_fmt("song_artist_fmt");
        SetDisplayText(
            MakeString(Localize(song_artist_fmt, 0), titleStr.c_str(), artistStr.c_str()),
            true
        );
    } else {
        static Symbol song_artist_fmt_number("song_artist_fmt_number");
        SetDisplayText(
            MakeString(
                Localize(song_artist_fmt_number, 0), i, titleStr.c_str(), artistStr.c_str()
            ),
            true
        );
    }
}

void AppLabel::SetSongName(const SongSortNode *ssn) {
    MILO_ASSERT(ssn, 265);
    SetDisplayText(ssn->GetTitle(), 1);
}

void AppLabel::SetSongYear(int i1, int i2) {
    if (i1 == i2) {
        SetInt(i1, false);
    } else
        SetDisplayText(MakeString("%i (%i)", i1, i2), true);
}

void AppLabel::SetAlbumName(const SongSortNode *ssn) {
    MILO_ASSERT(ssn, 279);
    SetDisplayText(ssn->GetAlbum(), 1);
}

void AppLabel::SetArtistName(const SortNode *sn) {
    if (sn->GetType() == kNodeSubheader) {
        const SubheaderSortNode *shsn = dynamic_cast<const SubheaderSortNode *>(sn);
        SetDisplayText(shsn->GetArtist(), true);
    } else if (sn->GetType() == kNodeSong) {
        const SongSortNode *ssn = dynamic_cast<const SongSortNode *>(sn);
        MILO_ASSERT(ssn, 293);
        SetArtistName(ssn->GetArtist(), ssn->GetIsCover());
    } else {
        const SetlistSortNode *ssn = dynamic_cast<const SetlistSortNode *>(sn);
        MILO_ASSERT(ssn, 299);
        SavedSetlist *setlist = ssn->GetSetlistRecord()->GetSetlist();
        if (setlist->GetOwner() != nullptr) {
            SetDisplayText(setlist->GetOwner(), true);
        } else {
            SetDisplayText(gNullStr, true);
        }
    }
}

const char *SavedSetlist::GetOwner() const { return 0; }
bool SavedSetlist::IsBattle() const { return false; }

#pragma push
#pragma dont_inline on
DECOMP_FORCEBLOCK(AppLabel, (), {SetlistRecord* slr; slr->GetSetlist()->SavedSetlist::GetOwner();})
#pragma pop

void AppLabel::SetArtistName(Symbol shortname) {
    SetArtistName(
        (BandSongMetadata *)TheSongMgr.Data(
            TheSongMgr.GetSongIDFromShortName(shortname, true)
        )
    );
}

void AppLabel::SetArtistName(const BandSongMetadata *data) {
    SetArtistName(data->Artist(), !data->IsMasterRecording());
}

void AppLabel::SetArtistName(const char *name, bool cover) {
    if (!cover)
        SetDisplayText(name, true);
    else {
        static Symbol cover_artist_fmt("cover_artist_fmt");
        DataArrayPtr ptr;
        SetTokenFmt(cover_artist_fmt, ptr, name);
        ptr->Release();
    }
}

void AppLabel::SetSongAndArtistName(const SongSortNode *ssn) {
    static Symbol song_artist_fmt("song_artist_fmt");
    static Symbol store_famous_by("store_famous_by");
    String str;
    if (ssn->GetIsCover()) {
        str = MakeString("%s %s", Localize(store_famous_by, nullptr), ssn->GetArtist());
    } else {
        str = ssn->GetArtist();
    }

    SetDisplayText(
        MakeString(Localize(song_artist_fmt, nullptr), ssn->GetTitle(), str.c_str()), true
    );
}

void AppLabel::SetSongCount(int ct) {
    static Symbol song_select_song("song_select_song");
    static Symbol song_select_songs("song_select_songs");
    const Symbol *sptr = &song_select_songs;
    if (ct == 1)
        sptr = &song_select_song;
    Symbol sym = *sptr;
    SetTokenFmt(sym, LocalizeSeparatedInt(ct));
}

void AppLabel::SetStarRating(int i) {
    SetDisplayText(TheScoring->GetStarRating(i).Str(), true);
}

void AppLabel::SetMotd(MainHubPanel *hub) {
    static Message motd("motd");
    DataNode n = hub->Handle(motd, true);
    if (n.NotNull())
        SetDisplayText(n.Str(), true);
    return;
}

void AppLabel::SetDLCMotd(MainHubPanel *hub) {
    static Message dlc_motd("dlc_motd");
    DataNode n = hub->Handle(dlc_motd, true);
    if (n.NotNull())
        SetDisplayText(n.Str(), true);
    return;
}

void AppLabel::SetUnlinkedMotd(const MainHubMessageProvider *mhmp) {
    SetDisplayText(mhmp->mUnlinkedMotd.c_str(), true);
}

void AppLabel::SetScoreOrStars(const MetaPerformer *p, int i) {
    MILO_ASSERT(p, 395);
    SetInt(i, true);
}

DataNode AppLabel::OnSetUserName(const DataArray *_msg) {
    const DataNode &node = _msg->Node(2).Evaluate();
    if (node.Type() == kDataInt) {
        SetUserName(node.Int());
    } else {
        Hmx::Object *obj = node.GetObj();
        User *user = dynamic_cast<User *>(obj);
        if (user != nullptr) {
            SetUserName(user);

        } else {
            BandTrack *bt = dynamic_cast<BandTrack *>(obj);
            if (bt != nullptr) {
                SetUserName(bt);
            } else
                MILO_WARN("Could not set user name, unknown class");
        }
    }
    return 1;
}

void AppLabel::SetFromSongSelectNode(const Node *n) {
    DateTime *dt = n->GetDateTime();
    if (dt) {
        SetDateTime(*dt, year_format);
    } else {
        if (n->LocalizeToken()) {
            SetTextToken(n->GetToken());
        } else {
            SetDisplayText(n->GetToken().Str(), true);
        }
    }
}

void AppLabel::SetFromCharacter(const CharData *cd) {
    const char *s = cd->GetCharacterName();
    if (strcmp(s, "") == 0) {
        SetTextToken(character_emptyname);
    } else
        SetDisplayText(s, 1);
}

void AppLabel::SetBandName(const LocalBandUser *lbu) {
    SetBandName(TheProfileMgr.GetProfileForUser(lbu));
}

void AppLabel::SetBandName(const BandProfile *pProfile) {
    String name = pProfile->GetBandName();
    const char *s = name.c_str();
    if (strcmp(s, "") == 0) {
        static Symbol band_emptyname("band_emptyname");
        SetTextToken(band_emptyname);
    } else
        SetDisplayText(s, 1);
}

void AppLabel::SetPrimaryBandName() {
    BandMachineMgr *pMachineMgr = TheSessionMgr->GetMachineMgr();
    String pbn = pMachineMgr->GetLeaderPrimaryBandName();
    const char *s = pbn.c_str();
    if (pbn.length() != 0) {
        SetDisplayText(s, true);
    } else {
        String ppn = pMachineMgr->GetLeaderPrimaryProfileName();
        static Symbol band_default_name("band_default_name");
        String defaultname =
            MakeString(Localize(band_default_name, nullptr), ppn.c_str());
        SetDisplayText(defaultname.c_str(), true);
    }
}

DataNode AppLabel::OnSetBandName(const DataArray *_msg) {
    Hmx::Object *obj = _msg->GetObj(2);
    LocalBandUser *lbu = dynamic_cast<LocalBandUser *>(obj);
    if (lbu) {
        SetBandName(lbu);
    } else {
        SetBandName(dynamic_cast<BandProfile *>(obj));
    }
    return 0;
}

void AppLabel::SetSetlistName(const SavedSetlist *setlist) {
    SetDisplayText(setlist->GetTitle(), true);
}

void AppLabel::SetSetlistDescription(const SavedSetlist *setlist) {
    SetDisplayText(setlist->GetDescription(), true);
}

// Retail 360-only pair - see AppLabel.h FriendRecord note.
void AppLabel::SetFriendName(const FriendRecord *record) {
    SetDisplayText(record->mName.c_str(), true);
}

void AppLabel::SetFriendBandName(const FriendRecord *record) {
    SetDisplayText(record->mBandName.c_str(), true);
}

void AppLabel::SetSetlistOwner(const SetlistRecord *setlist) {
    SetDisplayText(setlist->GetOwner(), true);
}

void AppLabel::SetEditSetlistName(const UIPanel *panel) {
    SetDisplayText(panel->Property(setlist_name, true)->Str(nullptr), true);
}
void AppLabel::SetEditSetlistDesc(const UIPanel *panel) {
    SetDisplayText(panel->Property(setlist_desc, true)->Str(nullptr), true);
}

void AppLabel::SetOfferName(const StoreOffer *offer) {
    SetDisplayText(offer->OfferName(), true);
}
void AppLabel::SetOfferCost(const StoreOffer *offer) {
    SetDisplayText(offer->CostStr(), true);
}
void AppLabel::SetOfferArtist(const StoreOffer *offer) {
    static Symbol artist("artist");
    static Symbol cover("cover");
    if (offer->HasData(artist)) {
        bool isCover = false;
        if (offer->HasData(cover))
            isCover = offer->GetData(DataArrayPtr(cover), false).Int();
        SetArtistName(offer->GetData(DataArrayPtr(artist), false).Str(), isCover);
    } else {
        SetDisplayText(gNullStr, true);
    }
}
void AppLabel::SetOfferAlbum(const StoreOffer *offer) {
    static Symbol pack("pack");
    static Symbol album_name("album_name");
    if (offer->OfferType() == pack || !offer->HasData(album_name)) {
        SetDisplayText(gNullStr, true);
    } else {
        SetDisplayText(offer->GetData(DataArrayPtr(album_name), false).Str(), true);
    }
}
void AppLabel::SetOfferDescription(const StoreOffer *offer) {
    SetDisplayText(offer->Description(), true);
}

void AppLabel::SetStoreCrumbText() {
    StoreMenuPanel *smp = ObjectDir::Main()->Find<StoreMenuPanel>("store_menu_panel", true);
    UIPanel *sbp = ObjectDir::Main()->Find<UIPanel>("store_browser_panel", true);
    BandStorePanel *bsp = BandStorePanel::Instance();
    const char *title;
    if (sbp->GetState() != UIPanel::kUp || strlen(title = bsp->MenuTitle().c_str()) == 0) {
        SetDisplayText(smp->GetCrumbText(), true);
    } else {
        SetDisplayText(
            MakeString("%s::%s", smp->GetCrumbText(), title),
            true
        );
    }
}
// Retail 360-only: sets label text from a store submenu entry node.
// Only caller is StoreMenuProvider::Text.
void AppLabel::SetStoreMenuText(const DataNode &node) {
    if (node.Type() == kDataSymbol) {
        SetTextToken(node.Sym());
    } else if (node.Type() == kDataArray) {
        SetDisplayText(
            MakeString(
                "%s%s", Localize(node.Array()->Sym(0), nullptr), node.Array()->Str(1)
            ),
            true
        );
    } else {
        SetDisplayText(node.Str(), true);
    }
}

void AppLabel::SetMusicLibraryStatus() {
    SetDisplayText(TheMusicLibrary->GetStatusText(), true);
}

void AppLabel::SetRecommendation(const StoreInfoPanel *panel) {
    static Symbol store_recommended("store_recommended");
    SetDisplayText(panel->CurrentRecommendation()->unk0.c_str(), true);
}

void AppLabel::SetLinkingCode(const char *cc) {
    String s(cc);
    if (s.length() != 10)
        MILO_WARN("linking code is not 10 characters!\n");
    if (s.find(' ') != String::npos)
        MILO_WARN("linking code has spaces!\n");
    SetDisplayText(cc, 1);
}

void AppLabel::SetBattleTimeLeft(int seconds) {
    static Symbol battle_time_left_none("battle_time_left_none");
    static Symbol battle_time_left_second("battle_time_left_second");
    static Symbol battle_time_left_seconds("battle_time_left_seconds");
    static Symbol battle_time_left_minute("battle_time_left_minute");
    static Symbol battle_time_left_minutes("battle_time_left_minutes");
    static Symbol battle_time_left_hour("battle_time_left_hour");
    static Symbol battle_time_left_hours("battle_time_left_hours");
    static Symbol battle_time_left_day("battle_time_left_day");
    static Symbol battle_time_left_days("battle_time_left_days");
    static Symbol battle_time_left_week("battle_time_left_week");
    static Symbol battle_time_left_weeks("battle_time_left_weeks");
    int minutes = seconds / 60;
    int hours = minutes / 60;
    int days = hours / 24;
    int weeks = days / 7;
    const char *text;
    if (seconds <= 0) {
        text = Localize(battle_time_left_none, nullptr);
    } else if (seconds < 60) {
        if (seconds == 1)
            text = Localize(battle_time_left_second, nullptr);
        else
            text = MakeString(Localize(battle_time_left_seconds, nullptr), seconds);
    } else if (minutes < 60) {
        if (minutes == 1)
            text = Localize(battle_time_left_minute, nullptr);
        else
            text = MakeString(Localize(battle_time_left_minutes, nullptr), minutes);
    } else if (hours < 24) {
        if (hours == 1)
            text = Localize(battle_time_left_hour, nullptr);
        else
            text = MakeString(Localize(battle_time_left_hours, nullptr), hours);
    } else if (days < 14) {
        if (days == 1)
            text = Localize(battle_time_left_day, nullptr);
        else
            text = MakeString(Localize(battle_time_left_days, nullptr), days);
    } else if (weeks == 1) {
        text = Localize(battle_time_left_week, nullptr);
    } else {
        text = MakeString(Localize(battle_time_left_weeks, nullptr), weeks);
    }
    SetDisplayText(text, true);
}

void AppLabel::SetBattleInstrument(ScoreType ty) {
    static Symbol battle_instrument_fmt("battle_instrument_fmt");
    const char *result = gNullStr;
    if ((unsigned)ty < 0xB) {
        const char *name = Localize(ScoreTypeToSym(ty), nullptr);
        const char *fontChar = GetFontCharFromScoreType(ty, 0);
        result = MakeString(Localize(battle_instrument_fmt, nullptr), fontChar, name);
    }
    SetDisplayText(result, true);
}

void AppLabel::SetBattleInstrument(const SetlistRecord *slr) {
    SavedSetlist *setlist = slr->GetSetlist();
    MILO_ASSERT(setlist, 930);
    if (setlist->IsBattle()) {
        SetBattleInstrument((ScoreType)slr->unk30);
    } else {
        SetTextToken(gNullStr);
    }
}

void AppLabel::SetRatingIcon(int i) {
    static Symbol song_select("song_select");
    static Symbol rating_icons("rating_icons");
    Symbol locale = SystemLocale();
    SetIcon(SystemConfig(song_select, rating_icons, locale)->Str(i)[0]);
}

void AppLabel::SetNewReleaseEntryText1(const StoreMainPanel *panel) {
    MILO_ASSERT(panel, 570);
    SetDisplayText(panel->CurrentEntry()->mText1.c_str(), true);
}

void AppLabel::SetNewReleaseEntryText2(const StoreMainPanel *panel) {
    MILO_ASSERT(panel, 576);
    SetDisplayText(panel->CurrentEntry()->mText2.c_str(), true);
}

void AppLabel::SetNewReleaseEntryText3(const StoreMainPanel *panel) {
    MILO_ASSERT(panel, 582);
    SetDisplayText(panel->CurrentEntry()->mText3.c_str(), true);
}

void AppLabel::SetSectionName(const PracticeSection &section) {
    if (DataVariable("practice_mbt").Int(nullptr) != 0) {
        SetDisplayText(
            MakeString(
                "%s (%d:%d:%d)",
                Localize(section.unk0, nullptr),
                section.unk18 + 1,
                section.unk1c + 1,
                section.unk20
            ),
            true
        );
    } else {
        SetDisplayText(Localize(section.unk0, nullptr), true);
    }
}

void AppLabel::SetRawStoreShortcut(int i) {
    BandStorePanel *bsp = BandStorePanel::Instance();
    MILO_ASSERT(bsp, 659);
    SetDisplayText(bsp->ShortcutTextAtData(i), true);
}

void AppLabel::SetViewSetting(const ViewSetting *setting) {
    static Symbol setting_option_fmt("setting_option_fmt");
    SetTokenFmt(
        setting_option_fmt,
        Localize(setting->GetName(), nullptr),
        setting->GetCurrentStatus()
    );
}

void AppLabel::SetViewSettingStatus(const ViewSetting *setting) {
    MILO_ASSERT(setting, 771);
    SetDisplayText(setting->GetCurrentStatus(), true);
}

void AppLabel::SetStoreGroupName(const StoreOfferProvider *provider, int i) {
    const StoreOfferProvider::Element *e = provider->GetElementAtIndex(i);
    MILO_ASSERT(!e->mOffer, 648);
    if (e->mLocalize) {
        SetTextToken(e->mGroupHeading);
    } else {
        SetDisplayText(e->mGroupHeading.Str(), true);
    }
}

void AppLabel::SetPitch(int pitch, int chrom) {
    pitch = pitch % 12;
    int c;
    if (chrom > 1) {
        c = 1;
    } else if (chrom < -1) {
        c = -1;
    } else {
        c = chrom;
    }
    switch (pitch) {
    case 1:
    case 6:
    case 8:
        if (c == 0)
            c = 1;
        pitch -= c;
        break;
    case 3:
    case 10:
        if (c == 0)
            c = -1;
        pitch -= c;
        break;
    default:
        c = 0;
        break;
    }
    char buf[3];
    buf[0] = 0;
    buf[1] = 0;
    buf[2] = 0;
    switch (pitch) {
    case 0:
        buf[0] = 'C';
        break;
    case 2:
        buf[0] = 'D';
        break;
    case 4:
        buf[0] = 'E';
        break;
    case 5:
        buf[0] = 'F';
        break;
    case 7:
        buf[0] = 'G';
        break;
    case 9:
        buf[0] = 'A';
        break;
    case 11:
        buf[0] = 'B';
        break;
    default:
        MILO_FAIL("pitch %d doesn't map to a white key", pitch);
        break;
    }
    switch (c) {
    case 1:
        buf[1] = '#';
        break;
    case -1:
        buf[1] = 'b';
        break;
    }
    SetDisplayText(buf, true);
}

void AppLabel::SetTokenRedemptionString(const TokenRedemptionPanel *panel, int ix) {
    MILO_ASSERT(panel, 757);
    SetDisplayText(panel->GetListString(ix), true);
}

void AppLabel::SetFromScoreDisplayData(short mask, int score, int rank, bool amongAll) {
    String icons;
    for (int i = 0; i < 11; i++) {
        if (mask & (1 << i)) {
            icons += GetFontCharFromScoreType((ScoreType)i, 0);
        }
    }
    if (rank == 0) {
        SetDisplayText(
            MakeString("<alt>%s</alt> %s", icons.c_str(), LocalizeSeparatedInt(score)), true
        );
    } else {
        static Symbol ir_among_all("ir_among_all");
        static Symbol ir_among_friends("ir_among_friends");
        Symbol sym;
        if (amongAll) {
            sym = ir_among_all;
        } else {
            sym = ir_among_friends;
        }
        SetDisplayText(
            MakeString(
                "<alt>%s</alt> %s (%s %s)",
                icons.c_str(),
                LocalizeSeparatedInt(score),
                LocalizeOrdinal(rank, LocaleGenderMasculine, LocaleSingular, true),
                Localize(sym, nullptr)
            ),
            true
        );
    }
}

#pragma push
#pragma dont_inline on
BEGIN_HANDLERS(AppLabel)
    HANDLE(set_user_name, OnSetUserName)
    HANDLE_ACTION(set_intro_name, SetIntroName(_msg->Obj<BandUser>(2)))
    HANDLE_ACTION(
        set_icon_and_profile_name,
        SetIconAndProfileName((ScoreType)_msg->Int(2), _msg->Obj<BandProfile>(3))
    )
    HANDLE_ACTION(set_profile_name, SetProfileName(_msg->Obj<LocalBandUser>(2)))
    HANDLE_ACTION(
        set_formatted_profile_name,
        SetFormattedProfileName(_msg->Sym(2), _msg->Obj<BandUser>(3))
    )
    HANDLE_ACTION(set_star_rating, SetStarRating(_msg->Int(2)))
    HANDLE_ACTION(set_motd, SetMotd(_msg->Obj<MainHubPanel>(2)))
    HANDLE_ACTION(set_song_name, SetSongName(_msg->Sym(2), true))
    HANDLE_ACTION(set_song_name_from_node, SetSongName(_msg->Obj<SongSortNode>(2)))
    HANDLE_ACTION(set_album_name, SetAlbumName(_msg->Obj<SongSortNode>(2)))
    HANDLE_ACTION(set_artist_name, SetArtistName(_msg->Obj<SortNode>(2)))
    HANDLE_ACTION(set_artist_name_from_shortname, SetArtistName(_msg->Sym(2)))
    HANDLE_ACTION(
        set_song_and_artist_name, SetSongAndArtistName(_msg->Obj<SongSortNode>(2))
    )
    HANDLE_ACTION(
        set_song_and_artist_name_from_sym,
        SetSongAndArtistNameFromSymbol(_msg->Sym(2), _msg->Int(3))
    )
    HANDLE_ACTION(set_song_count, SetSongCount(_msg->Int(2)))
    HANDLE_ACTION(set_song_year, SetSongYear(_msg->Int(2), _msg->Int(3)))
    HANDLE_ACTION(
        set_score_or_stars, SetScoreOrStars(_msg->Obj<MetaPerformer>(2), _msg->Int(3))
    )
    HANDLE_ACTION(set_from_song_select_node, SetFromSongSelectNode(_msg->Obj<Node>(2)))
    HANDLE(set_band_name, OnSetBandName)
    HANDLE_ACTION(set_primary_band_name, SetPrimaryBandName())
    HANDLE_ACTION(
        set_setlist_name, SetSetlistName(_msg->Obj<SetlistRecord>(2)->GetSetlist())
    )
    HANDLE_ACTION(
        set_setlist_description,
        SetSetlistDescription(_msg->Obj<SetlistRecord>(2)->GetSetlist())
    )
    HANDLE_ACTION(set_setlist_owner, SetSetlistOwner(_msg->Obj<SetlistRecord>(2)))
    HANDLE_ACTION(set_edit_setlist_name, SetEditSetlistName(_msg->Obj<UIPanel>(2)))
    HANDLE_ACTION(set_edit_setlist_desc, SetEditSetlistDesc(_msg->Obj<UIPanel>(2)))
    HANDLE_ACTION(set_offer_name, SetOfferName(_msg->Obj<StoreOffer>(2)))
    HANDLE_ACTION(set_offer_cost, SetOfferCost(_msg->Obj<StoreOffer>(2)))
    HANDLE_ACTION(set_offer_artist, SetOfferArtist(_msg->Obj<StoreOffer>(2)))
    HANDLE_ACTION(set_offer_album, SetOfferAlbum(_msg->Obj<StoreOffer>(2)))
    HANDLE_ACTION(set_store_crumb_text, SetStoreCrumbText())
    HANDLE_ACTION(set_offer_description, SetOfferDescription(_msg->Obj<StoreOffer>(2)))
    HANDLE_ACTION(set_linking_code, SetLinkingCode(_msg->Str(2)))
    HANDLE_ACTION(set_music_library_status, SetMusicLibraryStatus())
    HANDLE_ACTION(set_battle_time_left, SetBattleTimeLeft(_msg->Int(2)))
    HANDLE_ACTION(set_battle_instrument, SetBattleInstrument((ScoreType)_msg->Int(2)))
    HANDLE_ACTION(
        set_battle_instrument_str, SetBattleInstrument(_msg->Obj<SetlistRecord>(2))
    )
    HANDLE_ACTION(set_rating_icon, SetRatingIcon(_msg->Int(2)))
    HANDLE_ACTION(set_recommendation, SetRecommendation(_msg->Obj<StoreInfoPanel>(2)))
    HANDLE_SUPERCLASS(BandLabel)
    HANDLE_CHECK(1040)
END_HANDLERS
#pragma pop
