#pragma once
#include "SongSortNode.h"
#include "bandobj/BandLabel.h"
#include "bandobj/BandTrack.h"
#include "game/BandUser.h"
#include "game/Defines.h"
#include "game/PracticeSectionProvider.h"
#include "meta/StoreOffer.h"
#include "meta_band/BandProfile.h"
#include "meta_band/BandSongMetadata.h"
#include "meta_band/CharData.h"
#include "meta_band/Leaderboard.h"
#include "meta_band/MainHubPanel.h"
#include "meta_band/MetaPerformer.h"
#include "meta_band/SongRecord.h"
#include "meta_band/StoreInfoPanel.h"
#include "meta_band/TokenRedemptionPanel.h"
#include "obj/Data.h"
#include "ui/UIPanel.h"

class Node;
class StoreMainPanel;
class ViewSetting;
class StoreOfferProvider;

class AppLabel : public BandLabel {
public:
    // ⚠ DO NOT add an explicit `AppLabel() {}` here — it costs the 288-byte
    // ??0AppLabel@@QAA@XZ match (86.9% -> 100%, measured +1 matched / +288 B).
    // For a class with vtordisp'd virtual bases, MSVC X360 emits a DIFFERENT
    // default ctor body depending on how the ctor was declared:
    //   * user-declared (even `{}`): the general vtordisp form, recomputing
    //     `runtime_vboff - static_vboff` per vbase (`subi rN, r11, 0x258` /
    //     `0x284`) — this is what BandLabel::BandLabel (100%) emits.
    //   * implicitly declared: the most-derived form, folding both vtordisp
    //     values to a constant 0 kept in a callee-save reg (hence
    //     `bl __savegprlr_29`, a 0x80 frame, and a different vfptr store
    //     order). Retail's body at 0x825704A8 is this second form.
    // Retail uses the constant form in only 20 of 708 vtordisp-init sites, so
    // it is a genuine discriminator, not scheduling noise.
    //
    // Base-name registration: retail band.exe has no "AppLabel" C string.  The
    // producer 0x82570428 -- bl'd by SetType@AppLabel, and co-registered with
    // AppInlineHelp by the meta_band factory at 0x82574e20 -- builds "BandLabel",
    // this class's base.  NB the literal is NOT the prefix-stripped "Label"
    // (zero retail bodies build that): DC3's AppLabel likewise repeats ITS own
    // base, declaring the literal HamLabel.
    OBJ_CLASSNAME(BandLabel);
    OBJ_SET_TYPE(AppLabel);
    NEW_OBJ(AppLabel);
    virtual DataNode Handle(DataArray *, bool);
    virtual ~AppLabel();
    virtual void SetCreditsText(DataArray *, UIListSlot *);

    DataNode OnSetUserName(const DataArray *);
    DataNode OnSetBandName(const DataArray *);

    void SetFromCharacter(const CharData *);
    void SetSongName(Symbol, bool);
    void SetSongNameWithNumber(int, int, const char *);
    void SetFromScoreDisplayData(short, int, int, bool);
    void SetUserName(const User *);
    void SetUserName(int);
    void SetUserName(BandTrack *);
    void SetIntroName(BandUser *);
    void SetProfileName(const LocalBandUser *);
    void SetIconAndProfileName(ScoreType, const BandProfile *);
    void SetFormattedProfileName(Symbol, BandUser *);
    void SetSongAndArtistNameFromSymbol(Symbol, int);
    void SetSongYear(int, int);
    void SetArtistName(Symbol);
    void SetArtistName(const BandSongMetadata *);
    void SetArtistName(const char *, bool);
    void SetArtistName(const SortNode *);
    void SetAlbumName(const SongSortNode *);
    void SetMotd(MainHubPanel *);
    void SetDLCMotd(MainHubPanel *);
    void SetUnlinkedMotd(const MainHubMessageProvider *);
    void SetLeaderboardRankAndName(const LeaderboardRow &);
    void SetLeaderboardName(const LeaderboardRow &);
    void SetPitch(int, int);
    void SetSectionName(const PracticeSection &);
    void SetFromSongSelectNode(const Node *);
    void SetSongCount(int);
    void SetStarRating(int);
    void SetScoreOrStars(const MetaPerformer *, int);
    void SetSongAndArtistName(const SongSortNode *);
    void SetSongName(const SongSortNode *);
    void SetSetlistName(const SavedSetlist *);
    void SetSetlistDescription(const SavedSetlist *);
    // Retail 360-only pair (fns 0x825AD298/0x825AD2B0): label setters for a
    // friend/share list record {String name; bool online; String bandName}.
    // Sole pinned caller is an unmapped provider at 0x82649280 (vector of
    // records at this+0x2C, slots "name"/..., status_online/offline.mat) —
    // the record's real class name is unknown; layout is pinned by the
    // caller's lbz +0xC (online flag) and these thunks' +0x8/+0x18 c_str
    // loads. Wii source has no equivalent (LIVE friends UI).
    struct FriendRecord {
        String mName; // 0x0
        bool mOnline; // 0xc
        String mBandName; // 0x10
    };
    void SetFriendName(const FriendRecord *);
    void SetFriendBandName(const FriendRecord *);
    void SetSetlistOwner(const SetlistRecord *);
    void SetEditSetlistName(const UIPanel *);
    void SetEditSetlistDesc(const UIPanel *);
    void SetOfferName(const StoreOffer *);
    void SetOfferCost(const StoreOffer *);
    void SetOfferArtist(const StoreOffer *);
    void SetOfferAlbum(const StoreOffer *);
    void SetOfferDescription(const StoreOffer *);
    void SetStoreCrumbText();
    // Retail 360-only (fn 0x825AE018): sets label text from a store submenu
    // entry node - Symbol -> SetTextToken; (loc_sym str) array ->
    // "%s%s" of Localize(arr->Sym(0)) + arr->Str(1); else node.Str().
    // Only caller is StoreMenuProvider::Text.
    void SetStoreMenuText(const DataNode &);
    void SetMusicLibraryStatus();
    void SetTokenRedemptionString(const TokenRedemptionPanel *, int);
    void SetBandName(const LocalBandUser *);
    void SetBandName(const BandProfile *);
    void SetLinkingCode(const char *);
    void SetRecommendation(const StoreInfoPanel *);
    void SetRatingIcon(int);
    void SetBattleTimeLeft(int);
    void SetBattleInstrument(ScoreType);
    void SetBattleInstrument(const SetlistRecord *);
    void SetPrimaryBandName();
    void SetNewReleaseEntryText1(const StoreMainPanel *);
    void SetNewReleaseEntryText2(const StoreMainPanel *);
    void SetNewReleaseEntryText3(const StoreMainPanel *);
    void SetRawStoreShortcut(int);
    void SetViewSetting(const ViewSetting *);
    void SetViewSettingStatus(const ViewSetting *);
    void SetStoreGroupName(const StoreOfferProvider *, int);
};