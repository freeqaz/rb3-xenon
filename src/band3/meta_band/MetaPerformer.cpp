#include "meta_band/MetaPerformer.h"
#include "bandobj/BandDirector.h"
#include "beatmatch/TrackType.h"
#include "decomp.h"
#include "game/BandUser.h"
#include "game/BandUserMgr.h"
#include "game/Defines.h"
#include "game/GameConfig.h"
#include "game/Performer.h"
#include "math/Rand.h"
#include "meta_band/AccomplishmentManager.h"
#include "meta_band/BandProfile.h"
#include "meta_band/BandSongMgr.h"
#include "meta_band/Campaign.h"
#include "meta_band/Instarank.h"
#include "meta_band/OvershellPanel.h"
#include "meta_band/SongSortMgr.h"
#include "meta_band/SongStatusMgr.h"
#include "meta_band/Utl.h"
#include "game/Game.h"
#include "game/GameMode.h"
#include "meta_band/SessionMgr.h"
#include "net/Net.h"
#include "net/Server.h"
#include "net_band/DataResults.h"
#include "net_band/RockCentral.h"
#include "net_band/RockCentralMsgs.h"
#include "network/net/NetSession.h"
#include "meta_band/ProfileMgr.h"
#include "meta_band/BandSongMetadata.h"
#include "meta_band/ModifierMgr.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Msg.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "os/System.h"
#include "rndobj/Rnd.h"
#include "stl/_pair.h"
#include "synth/MicManagerInterface.h"
#include "synth/Synth.h"
#include "tour/Tour.h"
#include "ui/UILabel.h"
#include "utl/DataPointMgr.h"
#include "utl/Loader.h"
#include "utl/PoolAlloc.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"

MetaPerformer *MetaPerformer::sMetaPerformer;
MicClientID sNullMicClientID;

PerformerStatsInfo::PerformerStatsInfo()
    : mInstrumentMask(0), mScoreType(kScoreBand), unkc(-1), unk10(-1),
      mDifficulty(kDifficultyEasy), mScore(0), mStars(0), mAccuracy(0), mStreak(0),
      mAwesomes(0), mDoubleAwesomes(0), mTripleAwesomes(0), mSoloPercent(0),
      mHOPOPercent(0), mUnisonPhrasesHit(0) {}

PerformerStatsInfo::~PerformerStatsInfo() {}

void PerformerStatsInfo::Clear() {
    mInstrumentMask = 0;
    unkc = -1;
    unk10 = -1;
    mScoreType = kScoreBand;
    mDifficulty = kDifficultyEasy;
    mScore = 0;
    mStars = 0;
    mAccuracy = 0;
    mStreak = 0;
    mAwesomes = 0;
    mDoubleAwesomes = 0;
    mTripleAwesomes = 0;
    mSoloPercent = 0;
    mHOPOPercent = 0;
    mUnisonPhrasesHit = 0;
}

void PerformerStatsInfo::Update(
    int i1, int i2, ScoreType ty, Difficulty diff, short s, Performer *performer
) {
    MILO_ASSERT(!performer->GetQuarantined(), 0x62);
    Stats &stats = performer->mStats;
    unkc = i1;
    unk10 = i2;
    mInstrumentMask = s;
    mScoreType = ty;
    mDifficulty = diff;
    if (ty == kScoreBand)
        mScore = performer->GetScore();
    else
        mScore = performer->GetIndividualScore();
    mStars = performer->GetNumStars();
    mAccuracy = performer->GetNotesHitFraction(0) * 100.0f;
    mStreak = stats.GetLongestStreak();
    mAwesomes = stats.GetHitCount();
    mDoubleAwesomes = stats.GetDoubleHarmonyHit();
    mTripleAwesomes = stats.GetTripleHarmonyHit();
    mSoloPercent = stats.GetSoloPercentage();
    mHOPOPercent = stats.GetHopoPercent();
    mUnisonPhrasesHit = stats.GetUnisonPhrasesHit();
}

BandStatsInfo::BandStatsInfo() {}

BandStatsInfo::~BandStatsInfo() { Clear(); }

void BandStatsInfo::Clear() {
    mSoloStats.clear();
    mBandStats.Clear();
}

void BandStatsInfo::UpdateBandStats(Difficulty d, short s, Performer *p) {
    mBandStats.Update(-1, -1, kScoreBand, d, s, p);
}

void BandStatsInfo::AddSoloStats(
    int i1, int i2, ScoreType s, Difficulty d, BandProfile *bp, Performer *p
) {
    std::pair<BandProfile *, PerformerStatsInfo> thePair;
    thePair.second.Update(i1, i2, s, d, 0, p);
    thePair.first = bp;
    mSoloStats.push_back(thePair);
}

FORCE_LOCAL_INLINE
const PerformerStatsInfo &BandStatsInfo::GetBandStats() const { return mBandStats; }
END_FORCE_LOCAL_INLINE

BandProfile *BandStatsInfo::GetSoloProfile(int index) const {
    MILO_ASSERT(index < mSoloStats.size(), 0xB6);
    BandProfile *profile = mSoloStats[index].first;
    MILO_ASSERT(profile, 0xBB);
    return profile;
}

const PerformerStatsInfo &BandStatsInfo::GetSoloStats(int index) const {
    MILO_ASSERT(index < mSoloStats.size(), 0xC3);
    return mSoloStats[index].second;
}

QuickplayPerformerImpl::QuickplayPerformerImpl() {}
bool QuickplayPerformerImpl::IsRandomSetList() const { return false; }
bool QuickplayPerformerImpl::HasSyncPermission() const { return IsLeaderLocal(); }

void MetaPerformer::Init() {
    MILO_ASSERT(!sMetaPerformer, 0xE9);
    sMetaPerformer = new MetaPerformer(TheSongMgr, "meta_performer");
}

MetaPerformer::MetaPerformer(const BandSongMgr &mgr, const char *cc)
    : Synchronizable(cc),
#ifndef RB3_NO_WII_META_MEMBERS
      mWiiPending(0),
#endif
      mCreditsPending(0), mVenue(gNullStr),
#ifndef RB3_NO_WII_META_MEMBERS
      mLastVenue(),
#endif
      mSetlist(gNullStr), mSetlistIsLocal(0), mSetlistIsHmx(0),
      mSongMgr((BandSongMgr *)&mgr), mHasOnlineScoring(0), mSkippedSong(0), unk2c0(0),
      mFestivalReward(0), mCheatInFinale(0), mCheating(0), unk338(0), unk33c(-1),
      mRecordBattleContextID(-1), mHarmonyOverride(0), mRealDrumsOverride(0), unk360(2),
      mVenueOverride(gNullStr) {
    SetName(cc, ObjectDir::Main());
    mQpPerformer = new QuickplayPerformerImpl();
    if (TheGameMode)
        TheGameMode->AddSink(this, mode_changed);
    if (TheSessionMgr)
        TheSessionMgr->AddSink(this, new_remote_user);
    if (mVenueOverride == gNullStr)
        mVenueOverride = no_venue_override;
    TheNetSession->AddSink(this);
    TheProfileMgr.AddSink(this, Symbol("primary_profile_changed_msg"));
    mIsBattle = false;
}

MetaPerformer::~MetaPerformer() {
    TheNetSession->RemoveSink(this);
    if (TheGameMode)
        TheGameMode->RemoveSink(this, mode_changed);
    if (TheSessionMgr)
        TheSessionMgr->RemoveSink(this, new_remote_user);
    TheProfileMgr.RemoveSink(this, Symbol("primary_profile_changed_msg"));
    delete mQpPerformer;
}

MetaPerformer *MetaPerformer::Current() { return sMetaPerformer; }

MetaPerformerImpl *MetaPerformer::CurrentImpl() const {
    MetaPerformerImpl *ret;
    return TheTour && TheGameMode->InMode(tour) && (ret = TheTour->m_pTourPerformer, ret)
        ? ret
        : mQpPerformer;
}

FORCE_LOCAL_INLINE
Symbol MetaPerformer::GetVenue() const { return mVenue; }
END_FORCE_LOCAL_INLINE

Symbol MetaPerformer::GetVenueClass() const {
    String venue(GetVenue().Str());
    unsigned int idx = venue.find_last_of('_');
    if (idx == String::npos)
        return GetVenue();
    else
        return venue.substr(0, idx).c_str();
}

Symbol MetaPerformer::GetLastVenueClass() const {
#ifdef RB3_NO_WII_META_MEMBERS
    Symbol lastVenue = mVenue; // retail Xbox has no separate mLastVenue
#else
    Symbol lastVenue = mLastVenue;
#endif
    String venue(lastVenue.Str());
    unsigned int idx = venue.find_last_of('_');
    if (idx == String::npos)
        return lastVenue;
    else
        return venue.substr(0, idx).c_str();
}

bool MetaPerformer::SongEndsWithEndgameSequence() const {
    return IsWinning() && IsLastSong();
}

FORCE_LOCAL_INLINE
int MetaPerformer::NumSongs() const { return mSongs.size(); }
END_FORCE_LOCAL_INLINE

FORCE_LOCAL_INLINE
int MetaPerformer::NumCompleted() const { return mStars.size(); }
END_FORCE_LOCAL_INLINE

Symbol MetaPerformer::Song() const {
    int num_complete = NumCompleted();
    MILO_ASSERT(num_complete <= NumSongs(), 0x189);
    if (mSongs.empty())
        return gNullStr;
    else if (num_complete == NumSongs())
        return gNullStr;
    else {
        MILO_ASSERT(!mSongs[num_complete].Null(), 0x192);
        return mSongs[num_complete];
    }
}

int MetaPerformer::SongsID() const {
    return TheSongMgr.GetSongIDFromShortName(Song(), true);
}

bool MetaPerformer::HasSong() const { return Song() != gNullStr; }

bool MetaPerformer::IsWinning() const {
    // laneAY-B: retail's `is_winning` handler inlines to
    // `bl CurrentImpl(); lwz vtbl; lwz +0x5c; bctrl` with NO InFinale() branch
    // (target MetaPerformer::Handle +0x9cc) -- the cheat-finale short-circuit is
    // rb3-Wii DEV only, and it is what kept this body out of /Ob2's inline budget.
    MetaPerformerImpl *pImpl = CurrentImpl();
    MILO_ASSERT(pImpl, 0x1AA);
    return pImpl->IsWinning();
}

const char *MetaPerformer::GetSetlistName() const { return mSetlistTitle.c_str(); }
bool MetaPerformer::HasSetlist() const { return mSetlist != gNullStr; }

void MetaPerformer::SetSetlist(Symbol s) {
    MILO_ASSERT(HasSyncPermission(), 0x1C1);
    ResetSongs();
    mSetlist = s;
    mSetlistTitle = Localize(mSetlist, nullptr);
    mSetlistIsLocal = false;
    std::vector<Symbol> songs;
    TheSongSortMgr->InqSongsForSetlist(mSetlist, songs);
    MILO_ASSERT(!songs.empty(), 0x1CC);
    for (int i = 0; i < songs.size(); i++) {
        MILO_ASSERT(!songs[i].Null(), 0x1D1);
        mSongs.push_back(songs[i]);
    }
    SetSyncDirty(-1, true);
}

void MetaPerformer::SetSetlist(const SavedSetlist *setlist) {
    SetSetlistImpl(setlist, true);
    SavedSetlist::SetlistType ty = setlist->GetType();
    if (ty == SavedSetlist::kSetlistHarmonix || ty == SavedSetlist::kBattleHarmonix) {
        mSetlistIsHmx = true;
    }
}

void MetaPerformer::SetSetlistImpl(const SavedSetlist *setlist, bool reset) {
    MILO_ASSERT(HasSyncPermission(), 0x1E5);
    if (reset)
        ResetSongs();
    mSetlist = setlist->GetIdentifyingToken();
    mSetlistTitle = setlist->GetTitle();
    mSetlistIsLocal = setlist->GetType() == SavedSetlist::kSetlistLocal;
    const std::vector<int> &songs = setlist->mSongs;
    FOREACH (it, songs) {
        Symbol shortName = TheSongMgr.GetShortNameFromSongID(*it, true);
        MILO_ASSERT(!shortName.Null(), 0x1FB);
        mSongs.push_back(shortName);
    }
    SetSyncDirty(-1, true);
}

ScoreType MetaPerformer::GetBattleInstrument() const {
    if (mIsBattle)
        return mBattleScoreType;
    else
        return kScoreBand;
}

int MetaPerformer::GetBattleID() const {
    if (mIsBattle)
        return mSetlistBattleID;
    else
        return 0;
}

bool MetaPerformer::HasValidBattleInstarank() const {
    // Oracle-verbatim (rb3-Wii MetaPerformer.cpp:334). This ternary is the retail
    // shape: it makes the two arms come out in retail's physical order (0-arm
    // first, load second) -- verified, idx 1602/1604 go from mismatched to equal.
    // (laneAY-B's if/return form scored higher on fuzzy but had the arms swapped.)
    // Residue: retail normalizes the loaded byte with subic/subfe (!=0), and the
    // resulting 3-instruction arm is why retail emits a full if/else diamond
    // instead of our 2-instruction branch-over. Not reproducible from source:
    //  - Instarank::mIsValid really is bool (rb3-Wii agrees); a char/int flip
    //    WOULD emit the normalization here but retail's sibling band site
    //    (0x82583FF4: `lbz r11,-0x2a0(r24); stw r11,0(r29)`) stores the byte RAW
    //    with no normalize, so the flip breaks that site. Confirmed from retail
    //    asm, not inferred.
    //  - Forcing an int composite type (`? 0 :`) is folded away by MSVC, which
    //    still knows the bool is 0/1. Byte-identical output.
    return !HasBattle() ? false : mBattleInstarank.IsValid();
}

FORCE_LOCAL_INLINE
bool MetaPerformer::HasBattle() const { return GetBattleID() > 0; }
END_FORCE_LOCAL_INLINE

const char *MetaPerformer::GetBattleName() {
    MILO_ASSERT(HasBattle(), 0x22C);
    return mSetlistTitle.c_str();
}

int MetaPerformer::GetBattleInstrumentMask() {
    MILO_ASSERT(HasBattle(), 0x233);
    return 1 << mBattleScoreType;
}

int MetaPerformer::GetBattleScore() {
    MILO_ASSERT(HasBattle(), 0x23D);
    return mBattleScore;
}

void MetaPerformer::UpdateBattleTypeLabel(UILabel *label) {
    MILO_ASSERT(label, 0x244);
    MILO_ASSERT(HasBattle(), 0x245);
    ScoreType inst = GetBattleInstrument();
    Symbol scoreTypeSym = ScoreTypeToSym(inst);
    const char *font = GetFontCharFromScoreType(inst, 0);
    label->SetTokenFmt(battle_instrument_fmt, font, scoreTypeSym);
}

void MetaPerformer::SetBattle(const BattleSavedSetlist *setlist) {
    ResetSongs();
    mSetlistBattleID = setlist->mID;
    mBattleScoreType = setlist->unk6c;
    mBattleScore = 0;
    mIsBattle = true;
    SetSetlistImpl(setlist, false);
}

void MetaPerformer::SetSongs(const std::vector<Symbol> &songs) {
    MILO_ASSERT(HasSyncPermission(), 0x265);
    ResetSongs();
    for (int i = 0; i < songs.size(); i++) {
        MILO_ASSERT(!songs[i].Null(), 0x269);
        mSongs.push_back(songs[i]);
    }
    SetSyncDirty(-1, true);
}

void MetaPerformer::SetSongs(const std::vector<int> &vec) {
    std::vector<Symbol> songs;
    for (std::vector<int>::const_iterator it = vec.begin(); it != vec.end(); ++it) {
        songs.push_back(TheSongMgr.GetShortNameFromSongID(*it, true));
    }
    SetSongs(songs);
}

void MetaPerformer::SetSong(Symbol s) {
    std::vector<Symbol> songs;
    songs.push_back(s);
    SetSongs(songs);
}

void MetaPerformer::SetSongs(DataArray *arr) {
    std::vector<Symbol> songs;
    for (int i = 0; i < arr->Size(); i++) {
        songs.push_back(arr->Sym(i));
    }
    SetSongs(songs);
}

Symbol MetaPerformer::GetCompletedSong() const {
    if (mStars.empty()) {
        if (TheGameMode->Property("loop_setlist", true)->Int()) {
            MILO_ASSERT(!mSongs.empty(), 0x290);
            return mSongs.back();
        } else {
            MILO_FAIL("GetCompletedSong called without a song having been played!");
            return gNullStr;
        }
    } else {
        int idx = mStars.size() - 1;
        MILO_ASSERT(idx < mSongs.size(), 0x29C);
        return mSongs[idx];
    }
}

const std::vector<Symbol> &MetaPerformer::GetSongs() const { return mSongs; }

bool MetaPerformer::IsFirstSong() const { return mStars.empty(); }

bool MetaPerformer::IsLastSong() const {
    int num_complete = NumCompleted();
    MILO_ASSERT(num_complete < NumSongs(), 0x2B0);
    return num_complete == NumSongs() - 1;
}

bool MetaPerformer::IsSetComplete() const {
    MILO_ASSERT(NumCompleted() <= NumSongs(), 0x2B6);
    return NumCompleted() == NumSongs();
}

bool MetaPerformer::PartPlaysInSet(Symbol s) const {
    for (std::vector<Symbol>::const_iterator it = mSongs.begin(); it != mSongs.end();
         ++it) {
        if (*it == gNullStr || *it == any || *it == random)
            return true;
        BandSongMetadata *data =
            (BandSongMetadata *)mSongMgr->Data(mSongMgr->GetSongIDFromShortName(*it, true)
            );
        if (data && data->HasPart(s, false))
            return true;
    }
    return false;
}

bool MetaPerformer::PartPlaysInSong(Symbol s) const {
    int songID = mSongMgr->GetSongIDFromShortName(Song(), true);
    BandSongMetadata *data = (BandSongMetadata *)mSongMgr->Data(songID);
    return data && data->HasPart(s, false);
}

bool MetaPerformer::VocalHarmonyInSong() const {
    BandSongMetadata *data =
        (BandSongMetadata *)mSongMgr->Data(mSongMgr->GetSongIDFromShortName(Song(), true)
        );
    return data && data->HasVocalHarmony();
}

int MetaPerformer::GetSetlistMaxVocalParts() const {
    int parts = 1;
    for (std::vector<Symbol>::const_iterator it = mSongs.begin(); it != mSongs.end();
         ++it) {
        if (*it == gNullStr || *it == any || *it == random)
            continue;
        else {
            BandSongMetadata *data = (BandSongMetadata *)mSongMgr->Data(
                mSongMgr->GetSongIDFromShortName(*it, true)
            );
            if (data) {
                int dataparts = data->NumVocalParts();
                if (dataparts > parts)
                    parts = dataparts;
            }
        }
    }
    return parts;
}

bool MetaPerformer::SetlistHasVocalHarmony() const {
    for (std::vector<Symbol>::const_iterator it = mSongs.begin(); it != mSongs.end();
         ++it) {
        if (*it == gNullStr || *it == any || *it == random)
            continue;
        else {
            BandSongMetadata *data = (BandSongMetadata *)mSongMgr->Data(
                mSongMgr->GetSongIDFromShortName(*it, true)
            );
            if (data && data->HasVocalHarmony())
                return true;
        }
    }
    return false;
}

bool MetaPerformer::SetHasMissingPart(Symbol s) const {
    for (std::vector<Symbol>::const_iterator it = mSongs.begin(); it != mSongs.end();
         ++it) {
        int songID = mSongMgr->GetSongIDFromShortName(*it, true);
        BandSongMetadata *data = (BandSongMetadata *)mSongMgr->Data(songID);
        if (data && !data->HasPart(s, false))
            return true;
    }
    return false;
}

bool MetaPerformer::SetHasMissingVocalHarmony() const {
    for (std::vector<Symbol>::const_iterator it = mSongs.begin(); it != mSongs.end();
         ++it) {
        BandSongMetadata *data =
            (BandSongMetadata *)mSongMgr->Data(mSongMgr->GetSongIDFromShortName(*it, true)
            );
        if (data && !data->HasVocalHarmony())
            return true;
    }
    return false;
}

bool MetaPerformer::SongAllowsVocalHarmony() const {
    int id;
    if (IsSetComplete()) {
        id = mSongMgr->GetSongIDFromShortName(GetCompletedSong(), true);
    } else
        id = mSongMgr->GetSongIDFromShortName(Song(), true);
    BandSongMetadata *data = (BandSongMetadata *)mSongMgr->Data(id);
    if (data && !data->HasVocalHarmony())
        return false;
    else
        return true;
}

bool MetaPerformer::IsNowUsingVocalHarmony() const {
    if (!SongAllowsVocalHarmony())
        return false;
    else if (mHarmonyOverride)
        return true;
    else {
        std::vector<BandUser *> users;
        TheBandUserMgr->GetParticipatingBandUsers(users);
        FOREACH (it, users) {
            BandUser *cur = *it;
            if (cur->GetTrackType() == kTrackVocals) {
                return cur->GetPreferredScoreType() == kScoreHarmony;
            }
        }
        return false;
    }
}

bool MetaPerformer::IsUsingRealDrums() const {
    if (mRealDrumsOverride)
        return true;
    else {
        std::vector<BandUser *> users;
        TheBandUserMgr->GetParticipatingBandUsers(users);
        FOREACH (it, users) {
            BandUser *cur = *it;
            if (cur->GetTrackType() == kTrackDrum) {
                return cur->GetPreferredScoreType() == kScoreRealDrum;
            }
        }
        return false;
    }
}

ScoreType MetaPerformer::GetScoreTypeForUser(BandUser *user) {
    MILO_ASSERT(user, 0x366);
    TrackType ty = user->GetTrackType();
    return TrackTypeToScoreType(ty, IsNowUsingVocalHarmony(), IsUsingRealDrums());
}

bool MetaPerformer::IsPlayingDemo() const {
    Symbol thesong = Song();
    if (thesong.Null())
        return false;
    else
        return TheSongMgr.IsDemo(TheSongMgr.GetSongIDFromShortName(thesong, true));
}

bool MetaPerformer::IsNoFailActive() const {
    bool set = IsBandNoFailSet();
    bool ret = false;
    if (set) {
        if (TheGameMode->Property(nofail_allowed, true)->Int())
            ret = true;
    }
    return ret;
}

// laneAY-B: retail's `is_band_no_fail_set` handler is a bare
// `subi r3, this; bl <IsBandNoFailSet>` -- retail did NOT inline this body into
// MetaPerformer::Handle, while /Ob2 does for us. auto_inline(off) restores the
// retail call shape (6 base-only instructions in Handle).
#pragma auto_inline(off)
bool MetaPerformer::IsBandNoFailSet() const {
    if (TheModifierMgr)
        return TheModifierMgr->IsModifierActive(mod_no_fail_band);
    else
        return false;
}
#pragma auto_inline(on)

void MetaPerformer::LockBandOrSolo() {
    unk360 = TheBandUserMgr->GetNumParticipants() > 1;
}

void MetaPerformer::UnlockBandOrSolo() { unk360 = 2; }

bool MetaPerformer::CanUpdateScoreLeaderboards() {
    if (TheGameMode) {
        if (!TheGameMode->Property("update_leaderboards", true)->Int())
            return false;
    }
    if (TheGame->ResumedNoScore())
        return false;
    else
        return true;
}

int MetaPerformer::GetHighestDifficultyForPart(Symbol s) const {
    int diff = 0;
    FOREACH (it, mSongs) {
        if (*it == gNullStr || *it == any || *it == random)
            return -1;
        BandSongMetadata *data =
            (BandSongMetadata *)mSongMgr->Data(mSongMgr->GetSongIDFromShortName(*it, true)
            );
        if (data && data->HasPart(s, false)) {
            if (diff < mSongMgr->GetPartDifficulty(*it, s)) {
                diff = mSongMgr->GetPartDifficulty(*it, s);
            }
        }
    }
    return diff;
}

void MetaPerformer::PopulatePlayerBandScores(
    const BandStatsInfo &info, std::vector<PlayerScore> &scores
) {
    const PerformerStatsInfo &perfStats = info.mBandStats;
    int numStats = info.NumSoloStats();
    for (int i = 0; i < numStats; i++) {
        BandProfile *profile = info.GetSoloProfile(i);
        MILO_ASSERT(profile, 0x3DF);
        int padnum = profile->GetPadNum();
        Server *netServer = TheNet.GetServer();
        MILO_ASSERT(netServer, 0x3E3);
        if (netServer->GetPlayerID(padnum) != 0) {
            PlayerScore score;
            PopulateSoloPlayerScore(perfStats, profile, score);
            scores.push_back(score);
        }
    }
}

void MetaPerformer::PopulateSoloPlayerScore(
    const PerformerStatsInfo &info, BandProfile *profile, PlayerScore &score
) {
    MILO_ASSERT(profile, 0x3F3);
    SongStatusMgr *songStatusMgr = profile->GetSongStatusMgr();
    MILO_ASSERT(songStatusMgr, 0x3F6);
    Server *netServer = TheNet.GetServer();
    MILO_ASSERT(netServer, 0x3F9);
    int i4 = info.unkc;
    if (info.mScoreType == kScoreBand) {
        i4 = profile->GetPadNum();
    }
    score.mPlayerID = netServer->GetPlayerID(i4);
    score.unk18 = info.unk10;
    score.mScoreType = info.mScoreType;
    score.mDiff = info.mDifficulty;
    score.mTotalScore = songStatusMgr->GetCachedTotalScore(info.mScoreType);
    score.mTotalDiscScore = songStatusMgr->GetCachedTotalDiscScore(info.mScoreType);
    score.mAccuracy = info.mAccuracy;
    score.mScore = info.mScore;
    score.mStars = info.mStars;
}

void MetaPerformer::PopulatePlayerScores(
    const BandStatsInfo &info, std::vector<PlayerScore> &scores
) {
    PopulatePlayerBandScores(info, scores);
    int numStats = info.NumSoloStats();
    for (int i = 0; i < numStats; i++) {
        const PerformerStatsInfo &curPerfInfo = info.GetSoloStats(i);
        BandProfile *profile = info.GetSoloProfile(i);
        MILO_ASSERT(profile, 0x415);
        int padnum = profile->GetPadNum();
        Server *netServer = TheNet.GetServer();
        MILO_ASSERT(netServer, 0x419);
        if (netServer->GetPlayerID(padnum) != 0) {
            PlayerScore score;
            PopulateSoloPlayerScore(curPerfInfo, profile, score);
            scores.push_back(score);
        }
    }
}

void MetaPerformer::UpdateScores(Symbol s, const BandStatsInfo &info, bool b3) {
    const PerformerStatsInfo &perfStats = info.mBandStats;
    int songID = mSongMgr->GetSongIDFromShortName(s, true);
    int numStats = info.NumSoloStats();
    for (int i = 0; i < numStats; i++) {
        BandProfile *profile = info.GetSoloProfile(i);
        MILO_ASSERT(profile, 0x432);
        int padnum = profile->GetPadNum();
        Server *netServer = TheNet.GetServer();
        bool b8 = !b3;
        MILO_ASSERT(netServer, 0x437);
        if (netServer->GetPlayerID(padnum) == 0) b8 = false;
        profile->UpdateScore(songID, info.GetSoloStats(i), b8);
        profile->UpdateScore(songID, perfStats, b8);
    }
}

void MetaPerformer::UpdateLastOfflineScores(Symbol s, const BandStatsInfo &info) {
    mSongID = mSongMgr->GetSongIDFromShortName(s, true);
    BandProfile *p = TheProfileMgr.GetPrimaryProfile();
    if (p) {
        mSongHighscore = p->GetSongHighScore(mSongID, kScoreBand);
    }
    int numStats = info.NumSoloStats();
    for (int i = 0; i < numStats; i++) {
        const PerformerStatsInfo &stats = info.GetSoloStats(i);
        BandProfile *profile = info.GetSoloProfile(i);
        MILO_ASSERT(profile, 0x459);
        int idx = stats.unk10;
        mInstarankScores[idx] = profile->GetSongHighScore(mSongID, stats.mScoreType);
        mInstarankScoreTypes[idx] = stats.mScoreType;
    }
}

void MetaPerformer::SaveAndUploadScores(
    std::vector<LocalBandUser *> &users, Symbol s, const BandStatsInfo &info
) {
    if (users.empty())
        return;
    if (!TheGame->IsInvalidScore()) {
        // Retail builds this dispatch Symbol as a FUNCTION-LOCAL STATIC, not the
        // centralized global from Symbols3.h: target fn_82580D80 tests guard word
        // lbl_82DFE9EC bit 0, then inline-constructs the Symbol object at the
        // adjacent lbl_82DFE9E8 via Symbol::Symbol(const char*). That guard emits
        // the mask-0x1 EH unwind funclet fn_8258102C (frame 0x70); with the global
        // we emit no funclet here at all. Same lever as RB3_HANDLE_LOCAL_STATIC.
        static Symbol insta_rank("insta_rank");
        bool instaRankProp = TheGameMode->Property(insta_rank, true)->Int();
        UpdateLastOfflineScores(s, info);
        UpdateScores(s, info, !instaRankProp);
        if (instaRankProp) {
            int songID = mSongMgr->GetSongIDFromShortName(s, true);
            mPendingData.friendMode = true;
            std::vector<PlayerScore> scores;
            PopulatePlayerScores(info, scores);
            int pID = 0;
            BandProfile *profile = TheProfileMgr.GetPrimaryProfile();
            if (profile) {
                int padnum = profile->GetPadNum();
                Server *netServer = TheNet.GetServer();
                MILO_ASSERT(netServer, 0x490);
                pID = netServer->GetPlayerID(padnum);
            }
            if (unk33c != -1) {
                MILO_WARN(
                    "We are sending out a record score (song = %s) while waiting for another to come back still...",
                    s.Str()
                );
            }
            if (TheNet.GetServer() && TheNet.GetServer()->IsConnected()
                && scores.size()) {
                mHasOnlineScoring = true;
                int old = unk338++;
                unk33c = old;
                TheRockCentral.RecordScore(
                    songID,
                    unk33c,
                    scores,
                    pID,
                    info.GetBandStats().mInstrumentMask,
                    true,
                    this,
                    mPendingData.ir_result
                );
                TheRockCentral.RecordOptionData();
            } else
                mHasOnlineScoring = false;
        }
        if (mIsBattle) {
            RecordBattleScore(info, IsLastSong());
        }
    }
}

ScoreType
MetaPerformer::GetInstarankScoreTypeForSlot(int slot, const BandStatsInfo &info) {
    Server *netServer = TheNet.GetServer();
    MILO_ASSERT(netServer, 0x4BE);
    std::vector<LocalBandUser *> users;
    TheBandUserMgr->GetLocalParticipants(users);
    FOREACH (it, users) {
        LocalBandUser *user = *it;
        MILO_ASSERT(user, 0x4C6);
        int userSlot = user->GetSlot();
        if (userSlot == slot) {
            int numStats = info.NumSoloStats();
            for (int i = 0; i < numStats; i++) {
                const PerformerStatsInfo &curPerfInfo = info.GetSoloStats(i);
                if (curPerfInfo.unk10 == userSlot) {
                    return curPerfInfo.mScoreType;
                }
            }
            break;
        }
    }
    return kScoreBand;
}

Instarank &MetaPerformer::GetInstarankForPlayerID(int slotNum) {
    MILO_ASSERT_RANGE(slotNum, 0, GameConfig::kMaxPlayers, 0x4DE);
    return mInstaranks[slotNum];
}

void MetaPerformer::UpdateInstarankData(
    DataResultList &results, const BandStatsInfo &info
) {
    for (int i = 0; i < results.NumDataResults(); i++) {
        DataResult *res = results.GetDataResult(i);

        DataNode ne8;
        res->GetDataResultValue("id", ne8);
        int i3 = ne8.Int();

        DataNode nf0;
        res->GetDataResultValue("slot", nf0);
        int i10 = nf0.Int();

        DataNode nf8;
        res->GetDataResultValue("is_boi", nf8);
        bool i4 = nf8.Int();

        DataNode n100;
        res->GetDataResultValue("insta_rank", n100);
        int i5 = n100.Int();

        DataNode n108;
        res->GetDataResultValue("is_percentile", n108);
        bool i6 = n108.Int();

        DataNode n110;
        res->GetDataResultValue("part_1", n110);
        String str50 = n110.Str();

        DataNode n118;
        res->GetDataResultValue("part_2", n118);
        String str5c = n118.Str();

        if (i4) {
            mBandInstarank.Init(i3, i4, kScoreBand, i5, i6, str50, str5c);
        } else {
            Instarank &rank = GetInstarankForPlayerID(i10);
            ScoreType s = GetInstarankScoreTypeForSlot(i10, info);
            rank.Init(i3, i4, s, i5, i6, str50, str5c);
        }
    }
}

void MetaPerformer::UpdateBattleInstarankData(DataResultList &results) {
    if (results.NumDataResults() < 1) {
        MILO_WARN("No data returned from RecordBattle call!\n");
    } else {
        DataResult *res = results.GetDataResult(0);

        DataNode n8c0;
        res->GetDataResultValue("id", n8c0);
        int i5 = n8c0.Int();

        DataNode n8c8;
        res->GetDataResultValue("is_boi", n8c8);
        bool i2 = n8c8.Int();

        DataNode n8d0;
        res->GetDataResultValue("insta_rank", n8d0);
        int i6 = n8d0.Int();

        DataNode n8d8;
        res->GetDataResultValue("is_percentile", n8d8);
        bool i7 = n8d8.Int();

        DataNode n8e0;
        res->GetDataResultValue("part_1", n8e0);
        String str84c = n8e0.Str();

        DataNode n8e8;
        res->GetDataResultValue("part_2", n8e8);
        String str858 = n8e8.Str();

        mBattleInstarank.Init(i5, i2, GetBattleInstrument(), i6, i7, str84c, str858);
    }
}

void MetaPerformer::ClearInstarankData() {
    mBandInstarank.Clear();
    for (int i = 0; i < 4; i++) {
        mInstaranks[i].Clear();
    }
    for (int i = 0; i < 4; i++) {
        mInstarankScores[i] = 0;
        mInstarankScoreTypes[i] = kScoreBand;
    }
}

void MetaPerformer::ClearBattleInstarankData() { mBattleInstarank.Clear(); }
Symbol MetaPerformer::GetVenueOverride() { return mVenueOverride; }

#pragma push
#pragma pool_data off
DataNode MetaPerformer::OnMsg(const RockCentralOpCompleteMsg &msg) {
    MILO_ASSERT(TheGameMode->Property(insta_rank), 0x566);
    int arg2 = msg.Arg2().Int();
    if (arg2 == unk33c) {
        unk33c = -1;
        if (msg.Success()) {
            ClearInstarankData();
            mPendingData.ir_result.Update(nullptr);
            UpdateInstarankData(mPendingData.ir_result, mPendingData.stats);
            static InstarankDoneMsg instarankDoneMsg;
            Export(instarankDoneMsg, true);
        } else {
            UpdateScores(mPendingData.song, mPendingData.stats, true);
        }
        mPendingData.Clear();
    } else if (arg2 == mRecordBattleContextID) {
        mRecordBattleContextID = -1;
        if (msg.Success()) {
            ClearBattleInstarankData();
            mDataResults.Update(nullptr);
            UpdateBattleInstarankData(mDataResults);
            static InstarankDoneMsg instarankDoneMsg;
            Export(instarankDoneMsg, true);
        }
    }
    return 1;
}
#pragma pop

DataNode MetaPerformer::OnMsg(const ModeChangedMsg &) {
    static Symbol local_vocalist_player_count("local_vocalist_player_count");
    int numPlayers = TheGameMode->Property(local_vocalist_player_count, true)->Int();
    MicClientMapper *mapper = TheSynth->GetMicClientMapper();
    if (mapper) {
        mapper->SetNumberOfPlayers(numPlayers);
    }
    return 1;
}

void MetaPerformer::SetBandNoFail(bool b1) {
    if (b1 != IsBandNoFailSet()) {
        TheModifierMgr->ToggleModifierEnabled(mod_no_fail_band);
        SetSyncDirty(-1, false);
        ExportUpdateMetaPerformer();
    }
}

void MetaPerformer::SelectRandomVenue() {
    DataArray *cfg = SystemConfig(video_venues);
    cfg->FindFloat(probability);
    cfg->FindFloat(artist_specific_probability);
    bool autoVox = TheModifierMgr->IsModifierActive(mod_auto_vocals);
    BandProfile *profile = TheProfileMgr.GetPrimaryProfile();
    if (autoVox || (profile && profile->HasCampaignKey(key_video_venues))) {
        Symbol s50 = gNullStr;
        DataArray *venuesCfg = SystemConfig(video_venues, venues);
        Symbol s54 = gNullStr;
        DataArray *artistArr;
        for (int i = 1; i < venuesCfg->Size(); i++) {
            DataArray *curArr = venuesCfg->Array(i);
            s54 = curArr->Sym(0);
            artistArr = curArr->FindArray(artists, false);
            bool b2 = false;
            if (artistArr && !mSongs.empty()) {
                b2 = true;
                FOREACH (it, mSongs) {
                    Symbol cur = *it;
                    bool b1 = false;
                    BandSongMetadata *data = (BandSongMetadata *)TheSongMgr.Data(
                        TheSongMgr.GetSongIDFromShortName(cur, false)
                    );
                    for (int j = 1; j < artistArr->Size(); j++) {
                        if (data && streq(artistArr->Str(j), data->Artist())) {
                            b1 = true;
                            break;
                        }
                    }
                    if (!b1) {
                        b2 = false;
                        break;
                    }
                }
            }
            if (b2) {
                s50 = s54;
                break;
            }
        }
        if (!s50.Null()) {
            float artistProb = SystemConfig(video_venues)
                                   ->FindFloat(artist_specific_probability);
            if (artistProb > RandomFloat() || autoVox) {
                SetVenue(s50);
                return;
            }
        } else {
            float prob1 = SystemConfig(video_venues)->FindFloat(probability);
            if (prob1 > RandomFloat() || autoVox) {
                std::vector<Symbol> validVenues;
                DataArray *venuesVideoCfg = SystemConfig(venues_video);
                for (int i = 1; i < venuesVideoCfg->Size(); i++) {
                    Symbol curSym = venuesVideoCfg->Sym(i);
                    if (!venuesCfg->FindArray(curSym)->FindArray(artists, false)) {
                        validVenues.push_back(curSym);
                    }
                }
                MILO_ASSERT(!validVenues.empty(), 0x620);
                SetVenue(validVenues[RandomInt(0, validVenues.size())]);
                return;
            }
        }
    }
    Symbol venueSym = subway_venues;
    if (profile) {
        LocalBandUser *user = profile->GetAssociatedLocalBandUser();
        MILO_ASSERT(user, 0x633);
        if (TheCampaign->HasReachedCampaignLevel(user, campaignlevel_jet)) {
            venueSym = jet_venues;
        } else if (TheCampaign->HasReachedCampaignLevel(user, campaignlevel_bus)) {
            venueSym = bus_venues;
        } else if (TheCampaign->HasReachedCampaignLevel(user, campaignlevel_van)) {
            venueSym = van_venues;
        }
    }
    DataArray *venueCfg = SystemConfig(venueSym);
    SetVenue(venueCfg->Sym(RandomInt(1, venueCfg->Size())));
}

void MetaPerformer::SetVenue(Symbol s) {
    bool changed = false;
    if (mVenue != s)
        changed = true;
    mVenue = s;
#ifndef RB3_NO_WII_META_MEMBERS
    mLastVenue = mVenue;
#endif
    if (mVenueOverride != no_venue_override) {
        mVenue = mVenueOverride;
#ifndef RB3_NO_WII_META_MEMBERS
        mLastVenue = mVenueOverride;
#endif
    }
    if (changed && TheSessionMgr && HasSyncPermission()) {
        SetSyncDirty(-1, false);
    }
    mFestivalReward = GetVenueClass() == festival
        && TheAccomplishmentMgr->HasNewRewardVignetteFestival();
}

void MetaPerformer::LoadFestival() {
    MILO_LOG("LOADING VENUE: %s\n", mVenue.Str());
    TheBandDirector->LoadVenue(mVenue, kLoadBack);
}

void MetaPerformer::ClearVenues() {
    bool hasVenue = false;
    if (mVenue != gNullStr)
        hasVenue = true;
    mVenue = gNullStr;
    mFestivalReward = false;
    if (hasVenue && TheSessionMgr && HasSyncPermission()) {
        SetSyncDirty(-1, false);
    }
    TheBandDirector->UnloadVenue(true);
}

void MetaPerformer::ResetSongs() {
    ResetCompletion();
    mSongs.clear();
    mSetlist = gNullStr;
    mSetlistIsHmx = false;
    mSetlistTitle = gNullStr;
    mSetlistIsLocal = false;
    mBattleScore = 0;
    mIsBattle = false;
    mSkippedSong = false;
}

void MetaPerformer::ResetCompletion() { mStars.clear(); }

void MetaPerformer::HostRestartLastSong() {
    MILO_ASSERT(IsLeaderLocal(), 0x6C2);
    MILO_ASSERT(HasSyncPermission(), 0x6C3);
    mStars.pop_back();
    CurrentImpl()->RestartLastSong();
    SetSyncDirty(-1, true);
}

void MetaPerformer::Restart() {
    MILO_ASSERT(HasSyncPermission(), 0x6D0);
    MILO_ASSERT(NumSongs() == NumCompleted(), 0x6D1);
    ResetCompletion();
    SetSyncDirty(-1, true);
}

void MetaPerformer::TriggerSongCompletion() {
    // Retail X360 opens this function with an audition-mode early-out that the
    // rb3-Wii DEV oracle does not carry (verified from the retail body at
    // 0x825827D8: a first-static guard on 0x82DFEA58 constructing Symbol
    // "audition" at 0x8202CD0C, then vtable slot 0 of *0x82E02530 == TheGameMode
    // -- GameMode's OWN vftable slot 0 is InMode(), since Handle/~GameMode
    // override virtual-base slots -- and `bne` straight to the epilogue before
    // any local is constructed).
    static Symbol audition("audition");
    if (TheGameMode->InMode(audition))
        return;
    bool m16 = GetCheating();
    SetCheating(false);
    Difficulty d15 = kDifficultyExpert;
    short i14 = 0;
    BandStatsInfo info;
    Band *band = TheGame->GetBand();
    Performer *perf = band->MainPerformer();
    std::vector<BandUser *> users;
    int numPlayers = band->NumActivePlayers();
    Symbol songSym = Song();
    int songID = TheSongMgr.GetSongIDFromShortName(songSym, true);
    Symbol gameMode = TheGameMode->GetMode();
    for (int i = 0; i < numPlayers; i++) {
        Player *player = band->GetActivePlayer(i);
        MILO_ASSERT(player, 0x6F0);
        BandUser *user = player->GetUser();
        users.push_back(user);
        ScoreType s = GetScoreTypeForUser(user);
        i14 |= 1 << s;
        if (d15 > user->GetDifficulty()) {
            d15 = user->GetDifficulty();
        }
        if (user->IsLocal()) {
            LocalBandUser *localUser = dynamic_cast<LocalBandUser *>(user);
            MILO_ASSERT(localUser, 0x700);
            BandProfile *profile = TheProfileMgr.GetProfileForUser(localUser);
            if (profile) {
                Difficulty d8 = localUser->GetDifficulty();
                if (!player->GetQuarantined() && localUser->unkc) {
                    int i7 = localUser->GetPadNum();
                    info.AddSoloStats(i7, localUser->GetSlot(), s, d8, profile, player);
                    if (player->IsAutoplay()) {
                        m16 = true;
                    }
                    profile->UpdatePerformanceData(
                        player->mStats,
                        songID,
                        s,
                        d8,
                        gameMode,
                        player->GetNumStars(),
                        GetBattleID(),
                        m16
                    );
                }
            }
        }
    }
    unk2c0 = i14;
    info.UpdateBandStats(d15, i14, perf);
    BandProfile *profile = TheProfileMgr.GetPrimaryProfile();
    if (profile) {
        profile->UpdatePerformanceData(
            perf->mStats,
            songID,
            kScoreBand,
            d15,
            gameMode,
            perf->GetNumStars(),
            GetBattleID(),
            m16
        );
    }
    CompleteSong(users, &info, false);
}

void MetaPerformer::CompleteSong(
    std::vector<BandUser *> &users, const BandStatsInfo *stats, bool b3
) {
    MILO_ASSERT(stats, 0x72A);
    MILO_ASSERT(NumCompleted() < NumSongs(), 0x72B);
    SendDataPoint(
        "debug/rc/context/highwatermark",
        "high",
        TheRockCentral.GetActiveContextHighWatermark()
    );
    CurrentImpl()->CompleteSong(users, stats, b3);
    Symbol songSym = Song();
    TheAccomplishmentMgr->HandlePreSongCompleted(songSym);
    PotentiallyUpdateLeaderboards(users, b3, songSym, *stats);
    IncrementSongPlayCount(users, songSym);
    TheAccomplishmentMgr->HandleSongCompleted(songSym, stats->GetBandStats().mDifficulty);
    if (IsLastSong() && !mSetlist.Null() && !mSkippedSong) {
        int totalStars = stats->GetBandStats().mStars + TotalStars(true);
        TheAccomplishmentMgr->HandleSetlistCompleted(
            mSetlist, mSetlistIsHmx, stats->GetBandStats().mDifficulty, totalStars
        );
    }
}

void MetaPerformer::IncrementSongPlayCount(std::vector<BandUser *> &users, Symbol s) {
    int songID = TheSongMgr.GetSongIDFromShortName(s, true);
    FOREACH (it, users) {
        BandUser *user = *it;
        MILO_ASSERT(user, 0x758);
        if (user->IsLocal()) {
            LocalBandUser *localUser = dynamic_cast<LocalBandUser *>(user);
            MILO_ASSERT(localUser, 0x75D);
            BandProfile *profile = TheProfileMgr.GetProfileForUser(localUser);
            if (profile) {
                SongStatusMgr *songStatusMgr = profile->GetSongStatusMgr();
                MILO_ASSERT(songStatusMgr, 0x763);
                songStatusMgr->SetSongPlayCount(
                    songID, songStatusMgr->GetSongPlayCount(songID) + 1
                );
                profile->MakeDirty();
            }
        }
    }
}

void MetaPerformer::RecordBattleScore(const BandStatsInfo &stats, bool battleFinished) {
    std::vector<BandProfile *> profiles;
    if (mBattleScoreType == kScoreBand) {
        mBattleScore += stats.GetBandStats().mScore;
        if (battleFinished) {
            profiles = TheProfileMgr.GetParticipatingProfiles();
        }
    } else {
        int numStats = stats.NumSoloStats();
        for (int i = 0; i < numStats; i++) {
            const PerformerStatsInfo &curPerfInfo = stats.GetSoloStats(i);
            if (curPerfInfo.mScoreType == mBattleScoreType) {
                mBattleScore += curPerfInfo.mScore;
                if (battleFinished) {
                    BandProfile *profile = stats.GetSoloProfile(i);
                    MILO_ASSERT(profile, 0x78D);
                    profiles.push_back(profile);
                }
            }
        }
    }
    Server *netServer = TheNet.GetServer();
    MILO_ASSERT(netServer, 0x795);

    std::vector<BandProfile *> profiles40;
    FOREACH (it, profiles) {
        BandProfile *cur = *it;
        if (netServer->GetPlayerID(cur->GetPadNum())) {
            profiles40.push_back(cur);
        }
    }
    if (!profiles40.empty()) {
        MILO_ASSERT(battleFinished, 0x7A6);
        MILO_ASSERT(mRecordBattleContextID == -1, 0x7A9);
        mRecordBattleContextID = unk338;
        unk338++;
        mDataResults.Clear();
        TheRockCentral.RecordBattleScore(
            mSetlistBattleID,
            mRecordBattleContextID,
            profiles40,
            mBattleScore,
            mDataResults,
            this
        );
    }
}

bool MetaPerformer::IsRandomSetList() const { return CurrentImpl()->IsRandomSetList(); }

void MetaPerformer::SkipSong() {
    mSkippedSong = true;
    AdvanceSong(0);
}

void MetaPerformer::AdvanceSong(int x) {
    MILO_ASSERT(NumCompleted() < NumSongs(), 0x7C3);
    mStars.push_back(x);
    if (HasSyncPermission()) {
        SetSyncDirty(-1, false);
    }
}

void MetaPerformer::OnSynchronized(unsigned int ui) {
    ExportUpdateMetaPerformer();
    CurrentImpl()->OnSynchronized(ui);
}

void MetaPerformer::ExportUpdateMetaPerformer() {
    DataArrayPtr ptr(update_meta_performer);
    ptr->Execute();
}

void MetaPerformer::SyncSave(BinStream &bs, unsigned int ui) const {
    bs << mVenue;
    bs << mFestivalReward;
    bs << mSetlist;
    bs << (mSetlistIsLocal ? String(gNullStr) : mSetlistTitle);
    bs << mSetlistIsHmx;
    bs << mSongs;
    bs << mStars;
    bs << IsBandNoFailSet();
    bs << mSkippedSong;
    bs << mIsBattle;
    if (mIsBattle) {
        bs << mSetlistBattleID;
        bs << mBattleScoreType;
    }
    CurrentImpl()->SyncSave(bs, ui);
}

void MetaPerformer::SyncLoad(BinStream &bs, unsigned int ui) {
    Symbol old = mVenue;
    bs >> mVenue;
    bs >> mFestivalReward;
    if (old != mVenue) {
        if (mVenue == gNullStr) {
            TheBandDirector->UnloadVenue(true);
        } else if (!mFestivalReward) {
            TheBandDirector->LoadVenue(mVenue, kLoadStayBack);
        }
    }
    bs >> mSetlist;
    bs >> mSetlistTitle;
    bs >> mSetlistIsHmx;
    bs >> mSongs;
    bs >> mStars;
    bool nofailset = IsBandNoFailSet();
    bool noFail;
    bs >> noFail;
    SetBandNoFail(noFail);
    if (nofailset != noFail && TheGame) {
        TheGame->SetNoFail(noFail);
    }
    bs >> mSkippedSong;
    bs >> mIsBattle;
    if (mIsBattle) {
        bs >> mSetlistBattleID;
        int score;
        bs >> score;
        mBattleScoreType = (ScoreType)score;
    }
    CurrentImpl()->SyncLoad(bs, ui);
}

bool MetaPerformer::HasSyncPermission() const {
    return CurrentImpl()->HasSyncPermission();
}

void MetaPerformer::PotentiallyUpdateLeaderboards(
    std::vector<BandUser *> &users, bool b2, Symbol s3, const BandStatsInfo &info
) {
    ClearInstarankData();
    ClearBattleInstarankData();
    mHasOnlineScoring = false;
    mPendingData.stats = info;
    mPendingData.song = s3;
    if (TheBandUserMgr && !b2 && CanUpdateScoreLeaderboards()) {
        int numUsers = users.size();
        std::vector<LocalBandUser *> users;
        std::vector<LocalBandUser *> filteredUsers;
        TheBandUserMgr->GetLocalParticipants(users);
        FOREACH (it, users) {
            if ((*it)->CanSaveData()) {
                filteredUsers.push_back(*it);
            }
        }
        if (numUsers == 1 && filteredUsers.empty()) {
            TheBandUserMgr->GetLocalBandUsers(users);
            FOREACH (it, users) {
                if ((*it)->CanSaveData()) {
                    filteredUsers.push_back(*it);
                }
            }
            if (filteredUsers.size() > 1)
                filteredUsers.clear();
        }
        SaveAndUploadScores(filteredUsers, s3, info);
    }
}

int MetaPerformer::TotalStars(bool b1) const {
    int stars = 0;
    FOREACH (it, mStars) {
        if (b1) {
            stars += Min(5, *it);
        } else
            stars += *it;
    }
    return stars;
}

bool MetaPerformer::HasBattleHighscore() { return mBattleInstarank.HasHighscore(); }

bool MetaPerformer::HasHighscore() {
    if (mHasOnlineScoring) {
        return mBandInstarank.HasHighscore();
    } else {
        BandProfile *profile = TheProfileMgr.GetPrimaryProfile();
        if (profile) {
            return profile->GetSongHighScore(mSongID, kScoreBand) > mSongHighscore;
        } else
            return false;
    }
}

int MetaPerformer::GetLastOfflineScore() { return mSongHighscore; }

int MetaPerformer::GetLastOfflineSoloScore(BandUser *user) {
    if (user->IsLocal()) {
        LocalBandUser *localUser = dynamic_cast<LocalBandUser *>(user);
        MILO_ASSERT(localUser, 0x878);
        return mInstarankScores[localUser->GetSlot()];
    } else
        return 0;
}

bool MetaPerformer::HasSoloHighscore(BandUser *user) {
    if (!user->IsLocal())
        return false;
    else {
        LocalBandUser *localUser = dynamic_cast<LocalBandUser *>(user);
        MILO_ASSERT(localUser, 0x88A);
        Player *player = localUser->GetPlayer();
        if (player && !player->GetQuarantined()) {
            BandProfile *profile = TheProfileMgr.GetProfileForUser(localUser);
            if (profile) {
                int padnum = localUser->GetPadNum();
                int slot = localUser->GetSlot();
                Server *netServer = TheNet.GetServer();
                MILO_ASSERT(netServer, 0x895);
                int pID = netServer->GetPlayerID(padnum);
                if (mHasOnlineScoring && pID) {
                    Instarank &rank = GetInstarankForPlayerID(slot);
                    if (rank.IsValid()) {
                        return rank.HasHighscore();
                    }
                }
                int highscore =
                    profile->GetSongHighScore(mSongID, mInstarankScoreTypes[slot]);
                return highscore > mInstarankScores[slot];
            }
        }
    }
    return false;
}

bool MetaPerformer::HasValidBandScore() { return mPendingData.stats.NumSoloStats() > 0; }

bool MetaPerformer::HasValidUserScore(BandUser *user) {
    if (user->IsLocal()) {
        LocalBandUser *localUser = dynamic_cast<LocalBandUser *>(user);
        MILO_ASSERT(localUser, 0x8B4);
        BandProfile *profile = TheProfileMgr.GetProfileForUser(localUser);
        if (profile) {
            int numStats = mPendingData.stats.NumSoloStats();
            for (int i = 0; i < numStats; i++) {
                if (mPendingData.stats.GetSoloProfile(i) == profile)
                    return true;
            }
        }
    }
    return false;
}

bool MetaPerformer::HasValidInstarankData() const { return mBandInstarank.IsValid(); }

void MetaPerformer::UpdateInstarankRankLabel(UILabel *label) {
    MILO_ASSERT(label, 0x8D0);
    // laneAY-B: retail has NO IsValid() guard here (target MetaPerformer::Handle
    // inlines a bare `mBandInstarank.UpdateRankLabel(label)`); the guard + MILO_WARN are
    // rb3-Wii DEV-build only and cost 4 base-only instructions per call site.
    mBandInstarank.UpdateRankLabel(label);
}

void MetaPerformer::UpdateInstarankHighscore1Label(UILabel *label) {
    MILO_ASSERT(label, 0x8DE);
    // laneAY-B: retail has NO IsValid() guard here (target MetaPerformer::Handle
    // inlines a bare `mBandInstarank.UpdateString1Label(label)`); the guard + MILO_WARN are
    // rb3-Wii DEV-build only and cost 4 base-only instructions per call site.
    mBandInstarank.UpdateString1Label(label);
}

void MetaPerformer::UpdateInstarankHighscore2Label(UILabel *label) {
    MILO_ASSERT(label, 0x8EC);
    // laneAY-B: retail has NO IsValid() guard here (target MetaPerformer::Handle
    // inlines a bare `mBandInstarank.UpdateString2Label(label)`); the guard + MILO_WARN are
    // rb3-Wii DEV-build only and cost 4 base-only instructions per call site.
    mBandInstarank.UpdateString2Label(label);
}

void MetaPerformer::UpdateBattleInstarankHighscore1Label(UILabel *label) {
    MILO_ASSERT(label, 0x8FA);
    // laneAY-B: retail has NO IsValid() guard here (target MetaPerformer::Handle
    // inlines a bare `mBattleInstarank.UpdateString1Label(label)`); the guard + MILO_WARN are
    // rb3-Wii DEV-build only and cost 4 base-only instructions per call site.
    mBattleInstarank.UpdateString1Label(label);
}

void MetaPerformer::UpdateBattleInstarankHighscore2Label(UILabel *label) {
    MILO_ASSERT(label, 0x908);
    // laneAY-B: retail has NO IsValid() guard here (target MetaPerformer::Handle
    // inlines a bare `mBattleInstarank.UpdateString2Label(label)`); the guard + MILO_WARN are
    // rb3-Wii DEV-build only and cost 4 base-only instructions per call site.
    mBattleInstarank.UpdateString2Label(label);
}

const char *MetaPerformer::GetSoloScoreTypeIcon(BandUser *user) {
    if (user->IsLocal()) {
        LocalBandUser *localUser = dynamic_cast<LocalBandUser *>(user);
        MILO_ASSERT(localUser, 0x919);
        int padnum = localUser->GetPadNum();
        int slot = localUser->GetSlot();
        Server *netServer = TheNet.GetServer();
        MILO_ASSERT(netServer, 0x91E);
        int pID = netServer->GetPlayerID(padnum);
        ScoreType s;
        if (mHasOnlineScoring && pID) {
            Instarank &rank = GetInstarankForPlayerID(slot);
            if (rank.IsValid())
                s = rank.GetScoreType();
            else
                s = mInstarankScoreTypes[slot];
        } else
            s = mInstarankScoreTypes[slot];
        return GetFontCharFromScoreType(s, 0);
    } else
        return "j";
}

void MetaPerformer::UpdateSoloInstarankRankLabel(BandUser *user, UILabel *label) {
    MILO_ASSERT(label, 0x93B);
    MILO_ASSERT(user, 0x93C);
    if (!user->IsLocal()) {
        label->SetTextToken(gNullStr);
    } else {
        LocalBandUser *localUser = dynamic_cast<LocalBandUser *>(user);
        MILO_ASSERT(localUser, 0x946);
        Player *player = localUser->GetPlayer();
        if (!player || player->GetQuarantined()) {
            label->SetTextToken(gNullStr);
        } else if (!TheProfileMgr.GetProfileForUser(localUser)) {
            label->SetTextToken(gNullStr);
        } else {
            int padnum = localUser->GetPadNum();
            int slot = localUser->GetSlot();
            Server *netServer = TheNet.GetServer();
            MILO_ASSERT(netServer, 0x95B);
            if (!netServer->GetPlayerID(padnum)) {
                label->SetTextToken(gNullStr);
            } else {
                Instarank &rank = GetInstarankForPlayerID(slot);
                if (rank.IsValid()) {
                    rank.UpdateRankLabel(label);
                }
            }
        }
    }
}

void MetaPerformer::UpdateSoloInstarankHighscore1Label(BandUser *user, UILabel *label) {
    MILO_ASSERT(label, 0x96D);
    MILO_ASSERT(user, 0x96E);
    if (!user->IsLocal()) {
        label->SetTextToken(gNullStr);
    } else {
        LocalBandUser *localUser = dynamic_cast<LocalBandUser *>(user);
        MILO_ASSERT(localUser, 0x978);
        Player *player = localUser->GetPlayer();
        if (!player || player->GetQuarantined()) {
            label->SetTextToken(gNullStr);
        } else if (!TheProfileMgr.GetProfileForUser(localUser)) {
            label->SetTextToken(gNullStr);
        } else {
            int padnum = localUser->GetPadNum();
            int slot = localUser->GetSlot();
            Server *netServer = TheNet.GetServer();
            MILO_ASSERT(netServer, 0x98D);
            if (!netServer->GetPlayerID(padnum)) {
                label->SetTextToken(gNullStr);
            } else {
                Instarank &rank = GetInstarankForPlayerID(slot);
                if (rank.IsValid()) {
                    rank.UpdateString1Label(label);
                }
            }
        }
    }
}

void MetaPerformer::UpdateSoloInstarankHighscore2Label(BandUser *user, UILabel *label) {
    MILO_ASSERT(label, 0x99F);
    MILO_ASSERT(user, 0x9A0);
    if (!user->IsLocal()) {
        label->SetTextToken(gNullStr);
    } else {
        LocalBandUser *localUser = dynamic_cast<LocalBandUser *>(user);
        MILO_ASSERT(localUser, 0x9AA);
        Player *player = localUser->GetPlayer();
        if (!player || player->GetQuarantined()) {
            label->SetTextToken(gNullStr);
        } else if (!TheProfileMgr.GetProfileForUser(localUser)) {
            label->SetTextToken(gNullStr);
        } else {
            int padnum = localUser->GetPadNum();
            int slot = localUser->GetSlot();
            Server *netServer = TheNet.GetServer();
            MILO_ASSERT(netServer, 0x9BF);
            if (!netServer->GetPlayerID(padnum)) {
                label->SetTextToken(gNullStr);
            } else {
                Instarank &rank = GetInstarankForPlayerID(slot);
                if (rank.IsValid()) {
                    rank.UpdateString2Label(label);
                }
            }
        }
    }
}

// laneAY-B: retail's `upload_debug_stats` handler emits NO calls at all
// (target MetaPerformer::Handle +0xffc is a bare DataNode(0) tail) -- the
// TheRnd/Symbol/ChunkAllocator debug uploads are rb3-Wii DEV-build only.
void MetaPerformer::UploadDebugStats() {}

// laneAY-B: retail's set/clear_credits_pending handlers inline to a single
// `stb` on mCreditsPending (target +0xe00 / +0xe54); there is no overshell
// lookup and no function-local `static OvershellPanel *`.
void MetaPerformer::SetCreditsPending() { mCreditsPending = true; }

void MetaPerformer::ClearCreditsPending() { mCreditsPending = false; }

bool MetaPerformer::AreCreditsPending() const { return mCreditsPending; }
#ifdef RB3_NO_WII_META_MEMBERS
// Wii-only pending flags; retail Xbox carries no mWiiPending member.
void MetaPerformer::SetWiiPending(WiiPendingFlags) {}
void MetaPerformer::ClearWiiPending(WiiPendingFlags) {}
bool MetaPerformer::IsWiiPending(WiiPendingFlags) const { return false; }
#else
void MetaPerformer::SetWiiPending(WiiPendingFlags flags) { mWiiPending |= flags; }
void MetaPerformer::ClearWiiPending(WiiPendingFlags flags) { mWiiPending &= ~flags; }
bool MetaPerformer::IsWiiPending(WiiPendingFlags flags) const {
    return mWiiPending & flags;
}
#endif
void MetaPerformer::SetCheating(bool b) { mCheating = b; }
short MetaPerformer::GetRecentInstrumentMask() const { return 0x400; }
bool MetaPerformer::CheatToggleFinale() { return mCheatInFinale = !mCheatInFinale; }

#pragma push
#pragma dont_inline on
BEGIN_HANDLERS(MetaPerformer)
    HANDLE_EXPR(current, Current())
    HANDLE_ACTION(set_venue, SetVenue(_msg->ForceSym(2)))
    HANDLE_ACTION(load_festival, LoadFestival())
    HANDLE_EXPR(festival_reward, mFestivalReward)
    HANDLE_ACTION(select_random_venue, SelectRandomVenue())
    HANDLE_EXPR(get_venue, GetVenue())
    HANDLE_EXPR(get_venue_class, GetVenueClass())
#ifdef HX_NATIVE
    HANDLE_EXPR(get_last_venue_class, GetLastVenueClass())
#endif
    HANDLE_ACTION(clear_venues, ClearVenues())
    HANDLE_EXPR(has_setlist, HasSetlist())
    HANDLE_ACTION(set_song, SetSong(_msg->Sym(2)))
    HANDLE_ACTION(set_songs, SetSongs(_msg->Array(2)))
    HANDLE_ACTION(reset_songs, ResetSongs())
    HANDLE_ACTION(restart, Restart())
    HANDLE_ACTION(host_restart_last_song, HostRestartLastSong())
    HANDLE_EXPR(num_songs, NumSongs())
    HANDLE_EXPR(num_completed, NumCompleted())
    HANDLE_EXPR(song, Song())
    HANDLE_EXPR(song_id, SongsID())
    HANDLE_EXPR(indexed_song, GetSongSymbol(_msg->Int(2)))
    HANDLE_EXPR(win_metric, GetWinMetric())
    HANDLE_EXPR(get_completed_song, GetCompletedSong())
    HANDLE_EXPR(is_first_song, IsFirstSong())
    HANDLE_EXPR(is_last_song, IsLastSong())
    HANDLE_EXPR(is_set_complete, IsSetComplete())
    HANDLE_EXPR(part_plays_in_set, PartPlaysInSet(_msg->Sym(2)))
    HANDLE_EXPR(part_plays_in_song, PartPlaysInSong(_msg->Sym(2)))
    HANDLE_EXPR(is_winning, IsWinning())
    HANDLE_EXPR(has_sync_permission, HasSyncPermission())
#ifdef HX_NATIVE
    HANDLE_EXPR(cheat_toggle_finale, CheatToggleFinale())
#endif
    HANDLE_ACTION(trigger_song_completion, TriggerSongCompletion())
    HANDLE_ACTION(advance_song, AdvanceSong(_msg->Int(2)))
    HANDLE_ACTION(skip_song, SkipSong())
    HANDLE_EXPR(is_no_fail_active, IsNoFailActive())
    HANDLE_EXPR(is_band_no_fail_set, IsBandNoFailSet())
    HANDLE_ACTION(set_band_no_fail, SetBandNoFail(_msg->Int(2)))
    HANDLE_ACTION(set_harmony_override, mHarmonyOverride = _msg->Int(2))
    HANDLE_ACTION(set_realdrums_override, mRealDrumsOverride = _msg->Int(2))
#ifdef HX_NATIVE
    HANDLE_EXPR(get_venue_override, GetVenueOverride())
    HANDLE_ACTION(set_venue_override, mVenueOverride = _msg->Sym(2))
#endif
    HANDLE_EXPR(is_now_using_vocal_harmony, IsNowUsingVocalHarmony())
    HANDLE_EXPR(is_playing_demo, IsPlayingDemo())
    HANDLE_EXPR(are_credits_pending, AreCreditsPending())
    HANDLE_ACTION(set_credits_pending, SetCreditsPending())
    HANDLE_ACTION(clear_credits_pending, ClearCreditsPending())
#ifdef HX_NATIVE
    HANDLE_EXPR(is_wiiprofile_create_pending, IsWiiPending((WiiPendingFlags)1))
    HANDLE_ACTION(set_wiiprofile_create_pending, SetWiiPending((WiiPendingFlags)1))
    HANDLE_ACTION(clear_wiiprofile_create_pending, ClearWiiPending((WiiPendingFlags)1))
#endif
    HANDLE_EXPR(is_using_persistent_data, GetPersistentGameData() != 0)
    HANDLE_EXPR(total_stars, TotalStars(_msg->Int(2)))
    HANDLE_EXPR(get_total_stars_capped, TotalStars(true))
    HANDLE_EXPR(get_recent_instrument_mask, GetRecentInstrumentMask())
    HANDLE_ACTION(upload_debug_stats, UploadDebugStats())
    // laneAY-B: retail's handler list carries `has_online_scoring` TWICE
    // (guard 0x82dfeb00 bits 14 and 17, target +0x104c and +0x1184) -- these
    // were previously gated behind HX_NATIVE, which dropped both from the X360
    // build and left our static count at 70 vs retail's 72.
    HANDLE_EXPR(has_online_scoring, mHasOnlineScoring)
    HANDLE_EXPR(has_valid_band_score, HasValidBandScore())
    HANDLE_EXPR(has_valid_user_score, HasValidUserScore(_msg->Obj<BandUser>(2)))
    HANDLE_EXPR(has_online_scoring, mHasOnlineScoring)
    HANDLE_EXPR(has_valid_instarank_data, HasValidInstarankData())
    HANDLE_ACTION(
        update_instarank_rank_label, UpdateInstarankRankLabel(_msg->Obj<UILabel>(2))
    )
    HANDLE_ACTION(
        update_instarank_highscore_1_label,
        UpdateInstarankHighscore1Label(_msg->Obj<UILabel>(2))
    )
    HANDLE_ACTION(
        update_instarank_highscore_2_label,
        UpdateInstarankHighscore2Label(_msg->Obj<UILabel>(2))
    )
    HANDLE_EXPR(has_highscore, HasHighscore())
    HANDLE_EXPR(get_last_offline_score, GetLastOfflineScore())
    HANDLE_EXPR(get_solo_score_type_icon, GetSoloScoreTypeIcon(_msg->Obj<BandUser>(2)))
    HANDLE_ACTION(
        update_instarank_solo_rank_label,
        UpdateSoloInstarankRankLabel(_msg->Obj<BandUser>(2), _msg->Obj<UILabel>(3))
    )
    HANDLE_ACTION(
        update_instarank_solo_highscore_1_label,
        UpdateSoloInstarankHighscore1Label(_msg->Obj<BandUser>(2), _msg->Obj<UILabel>(3))
    )
    HANDLE_ACTION(
        update_instarank_solo_highscore_2_label,
        UpdateSoloInstarankHighscore2Label(_msg->Obj<BandUser>(2), _msg->Obj<UILabel>(3))
    )
    HANDLE_EXPR(has_solo_highscore, HasSoloHighscore(_msg->Obj<BandUser>(2)))
    HANDLE_EXPR(
        get_last_offline_solo_score, GetLastOfflineSoloScore(_msg->Obj<BandUser>(2))
    )
    HANDLE_ACTION(
        update_battle_instarank_highscore_1_label,
        UpdateBattleInstarankHighscore1Label(_msg->Obj<UILabel>(2))
    )
    HANDLE_ACTION(
        update_battle_instarank_highscore_2_label,
        UpdateBattleInstarankHighscore2Label(_msg->Obj<UILabel>(2))
    )
    HANDLE_EXPR(has_battle_highscore, HasBattleHighscore())
    HANDLE_EXPR(has_valid_battle_instarank_data, HasValidBattleInstarank())
    HANDLE_EXPR(has_battle, HasBattle())
    HANDLE_EXPR(get_battle_name, GetBattleName())
    HANDLE_EXPR(get_battle_instrument_mask, GetBattleInstrumentMask())
    HANDLE_EXPR(get_battle_score, GetBattleScore())
    HANDLE_ACTION(update_battle_type_label, UpdateBattleTypeLabel(_msg->Obj<UILabel>(2)))
    HANDLE_ACTION(set_cheating, SetCheating(_msg->Int(2)))
    HANDLE_MESSAGE(RockCentralOpCompleteMsg)
    HANDLE_MESSAGE(ModeChangedMsg)
    HANDLE_MEMBER_PTR(CurrentImpl())
    HANDLE_SUPERCLASS(MsgSource)
    HANDLE_CHECK(0xADB)
END_HANDLERS
#pragma pop
