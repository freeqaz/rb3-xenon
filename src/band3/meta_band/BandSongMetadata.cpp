#include "meta_band/BandSongMetadata.h"
#include "decomp.h"
#include "meta_band/BandMachineMgr.h"
#include "meta_band/SessionMgr.h"
#include "meta_band/SongUpgradeMgr.h"
#include "os/System.h"
#include "utl/MakeString.h"
#include "utl/UTF8.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"

void BandSongMetadata::InitBandSongMetadata() {
    static Symbol rock("rock");
    mTitle = 0;
    mArtist = 0;
    mAlbum = 0;
    mAlbumTrackNum = -1;
    mGenre = rock;
    mAnimTempo = 0;
    mLengthMs = 0;
    mHasAlternatePath = 0;
    mBasePoints = 0;
    mIsBonus = 0;
    mIsFake = 0;
    mIsTutorial = 0;
    mMuteWinCues = 0;
    mRating = 1;
    mGuidePitchVolume = 0;
    mVocalTonicNote = -1;
    mSongKey = -1;
    mSongTonality = -1;
    mSongScrollSpeed = 0;
    mTuningOffsetCents = 0;
    mBandFailCue = 0;
    mVocalPercussionBank = 0;
    mDrumKitBank = 0;
    mHasAlbumArt = 0;
    mIsMasterRecording = 0;
    for (int i = 0; i < 6; i++)
        mRealGuitarTuning[i] = 0;
    for (int i = 0; i < 4; i++)
        mRealBassTuning[i] = 0;
    mHasDiscUpdate = 0;
}

BandSongMetadata::BandSongMetadata(BandSongMgr *mgr) : mSongMgr(mgr) {
    InitBandSongMetadata();
}

BandSongMetadata::BandSongMetadata(
    DataArray *main_arr, DataArray *backup_arr, bool onDisc, BandSongMgr *mgr
)
    : SongMetadata(main_arr, backup_arr, onDisc), mSongMgr(mgr) {
    InitBandSongMetadata();
    DataArray *member_arr;
    static Symbol name("name");
    if (FIND_WITH_BACKUP(name)) {
        mTitle = member_arr->Str(1);
    }
    static Symbol artist("artist");
    if (FIND_WITH_BACKUP(artist)) {
        mArtist = member_arr->Str(1);
    }
    static Symbol album_name("album_name");
    if (FIND_WITH_BACKUP(album_name)) {
        mAlbum = member_arr->Str(1);
    }
    static Symbol album_track_number("album_track_number");
    if (FIND_WITH_BACKUP(album_track_number)) {
        mAlbumTrackNum = member_arr->Int(1);
    }
    static Symbol year_released("year_released");
    if (FIND_WITH_BACKUP(year_released)) {
        mDateReleased = DateTime(member_arr->Int(1), 1, 1, 0, 0, 0);
    }
    static Symbol year_recorded("year_recorded");
    if (FIND_WITH_BACKUP(year_recorded)) {
        mDateRecorded = DateTime(member_arr->Int(1), 1, 1, 0, 0, 0);
    } else
        mDateRecorded = mDateReleased;
    static Symbol genre("genre");
    if (FIND_WITH_BACKUP(genre)) {
        mGenre = member_arr->Sym(1);
    }
    static Symbol ugc("ugc");
    if (GameOrigin() == ugc) {
        static Symbol poprock("poprock");
        static Symbol rock("rock");
        static Symbol urban("urban");
        static Symbol other("other");
        if (mGenre == poprock || mGenre == rock || mGenre == urban
            || mGenre == other) {
            static Symbol sub_genre("sub_genre");
            if (FIND_WITH_BACKUP(sub_genre)) {
                Symbol sub;
                sub = member_arr->Sym(1);
                static Symbol classical("classical");
                static Symbol hiphoprap("hiphoprap");
                static Symbol popdanceelectronic("popdanceelectronic");
                static Symbol rbsoulfunk("rbsoulfunk");
                static Symbol reggaeska("reggaeska");
                static Symbol subgenre_alternativerap("subgenre_alternativerap");
                static Symbol subgenre_ambient("subgenre_ambient");
                static Symbol subgenre_breakbeat("subgenre_breakbeat");
                static Symbol subgenre_chiptune("subgenre_chiptune");
                static Symbol subgenre_classical("subgenre_classical");
                static Symbol subgenre_dance("subgenre_dance");
                static Symbol subgenre_disco("subgenre_disco");
                static Symbol subgenre_downtempo("subgenre_downtempo");
                static Symbol subgenre_drumandbass("subgenre_drumandbass");
                static Symbol subgenre_dub("subgenre_dub");
                static Symbol subgenre_electronica("subgenre_electronica");
                static Symbol subgenre_funk("subgenre_funk");
                static Symbol subgenre_gangsta("subgenre_gangsta");
                static Symbol subgenre_garage("subgenre_garage");
                static Symbol subgenre_hardcoredance("subgenre_hardcoredance");
                static Symbol subgenre_hardcorerap("subgenre_hardcorerap");
                static Symbol subgenre_hiphop("subgenre_hiphop");
                static Symbol subgenre_house("subgenre_house");
                static Symbol subgenre_industrial("subgenre_industrial");
                static Symbol subgenre_motown("subgenre_motown");
                static Symbol subgenre_oldschoolhiphop("subgenre_oldschoolhiphop");
                static Symbol subgenre_other("subgenre_other");
                static Symbol subgenre_rap("subgenre_rap");
                static Symbol subgenre_reggae("subgenre_reggae");
                static Symbol subgenre_rhythmandblues("subgenre_rhythmandblues");
                static Symbol subgenre_ska("subgenre_ska");
                static Symbol subgenre_soul("subgenre_soul");
                static Symbol subgenre_techno("subgenre_techno");
                static Symbol subgenre_trance("subgenre_trance");
                static Symbol subgenre_triphop("subgenre_triphop");
                static Symbol subgenre_undergroundrap("subgenre_undergroundrap");
                if (mGenre == poprock) {
                    if (sub == subgenre_disco || sub == subgenre_motown
                        || sub == subgenre_rhythmandblues || sub == subgenre_soul)
                        mGenre = rbsoulfunk;
                } else if (mGenre == rock) {
                    if (sub == subgenre_funk)
                        mGenre = rbsoulfunk;
                    else if (sub == subgenre_reggae || sub == subgenre_ska)
                        mGenre = reggaeska;
                } else if (mGenre == urban) {
                    if (sub == subgenre_alternativerap || sub == subgenre_gangsta
                        || sub == subgenre_hardcorerap || sub == subgenre_hiphop
                        || sub == subgenre_oldschoolhiphop || sub == subgenre_rap
                        || sub == subgenre_triphop || sub == subgenre_undergroundrap)
                        mGenre = hiphoprap;
                    else if (sub == subgenre_downtempo || sub == subgenre_drumandbass
                             || sub == subgenre_dub || sub == subgenre_electronica
                             || sub == subgenre_garage || sub == subgenre_hardcoredance
                             || sub == subgenre_industrial)
                        mGenre = popdanceelectronic;
                    else if (sub == subgenre_other)
                        mGenre = other;
                } else if (mGenre == other) {
                    if (sub == subgenre_classical)
                        mGenre = classical;
                    else if (sub == subgenre_ambient || sub == subgenre_breakbeat
                             || sub == subgenre_chiptune || sub == subgenre_dance
                             || sub == subgenre_electronica || sub == subgenre_house
                             || sub == subgenre_techno || sub == subgenre_trance)
                        mGenre = popdanceelectronic;
                }
            }
        }
    }
    static Symbol anim_tempo("anim_tempo");
    if (FIND_WITH_BACKUP(anim_tempo)) {
        mAnimTempo = member_arr->Int(1);
    }
    static Symbol vocal_gender("vocal_gender");
    if (FIND_WITH_BACKUP(vocal_gender)) {
        mVocalGender = member_arr->Sym(1);
    }
    static Symbol song_length("song_length");
    if (FIND_WITH_BACKUP(song_length)) {
        mLengthMs = member_arr->Int(1);
    }
    static Symbol alternate_path("alternate_path");
    if (FIND_WITH_BACKUP(alternate_path)) {
        mHasAlternatePath = member_arr->Int(1);
    }
    static Symbol base_points("base_points");
    if (FIND_WITH_BACKUP(base_points)) {
        mBasePoints = member_arr->Int(1);
    }
    static Symbol bonus("bonus");
    if (FIND_WITH_BACKUP(bonus)) {
        mIsBonus = member_arr->Int(1);
    }
    static Symbol fake("fake");
    if (FIND_WITH_BACKUP(fake)) {
        // Retail computes this as ONE boolean expression: there is no pre-initialised
        // `bool ret = false` (that would emit an early `li rN, 0`), and the node address
        // is materialised once, AFTER the type test.
        mIsFake = CONST_ARRAY(member_arr)->Node(1).Type() == kDataInt
            && CONST_ARRAY(member_arr)->Node(1).LiteralInt();
    }
    static Symbol tutorial("tutorial");
    if (FIND_WITH_BACKUP(tutorial)) {
        mIsTutorial = member_arr->Int(1);
    }
    static Symbol mute_win_cues("mute_win_cues");
    if (FIND_WITH_BACKUP(mute_win_cues)) {
        mMuteWinCues = true;
    }
    static Symbol rank("rank");
    if (FIND_WITH_BACKUP(rank)) {
        for (int i = 1; i < member_arr->Size(); i++) {
            DataArray *arr = member_arr->Array(i);
            mRanks[arr->Sym(0)] = arr->Float(1);
        }
    }
    static Symbol rating("rating");
    if (FIND_WITH_BACKUP(rating)) {
        mRating = member_arr->Int(1);
    }
    static Symbol guide_pitch_volume("guide_pitch_volume");
    if (FIND_WITH_BACKUP(guide_pitch_volume)) {
        mGuidePitchVolume = member_arr->Float(1);
    }
    static Symbol vocal_tonic_note("vocal_tonic_note");
    if (FIND_WITH_BACKUP(vocal_tonic_note)) {
        mVocalTonicNote = member_arr->Int(1);
    }
    static Symbol song_key("song_key");
    if (FIND_WITH_BACKUP(song_key)) {
        mSongKey = member_arr->Int(1);
    }
    static Symbol song_tonality("song_tonality");
    if (FIND_WITH_BACKUP(song_tonality)) {
        mSongTonality = member_arr->Int(1);
    }
    static Symbol song_scroll_speed("song_scroll_speed");
    if (FIND_WITH_BACKUP(song_scroll_speed)) {
        mSongScrollSpeed = member_arr->Float(1);
    }
    static Symbol tuning_offset_cents("tuning_offset_cents");
    if (FIND_WITH_BACKUP(tuning_offset_cents)) {
        mTuningOffsetCents = member_arr->Float(1);
    }
    static Symbol bank("bank");
    if (FIND_WITH_BACKUP(bank)) {
        mVocalPercussionBank = member_arr->Str(1);
    }
    static Symbol drum_bank("drum_bank");
    if (FIND_WITH_BACKUP(drum_bank)) {
        mDrumKitBank = member_arr->Str(1);
    }
    static Symbol band_fail_cue("band_fail_cue");
    if (FIND_WITH_BACKUP(band_fail_cue)) {
        mBandFailCue = member_arr->Str(1);
    }
    static Symbol album_art("album_art");
    if (FIND_WITH_BACKUP(album_art)) {
        mHasAlbumArt = member_arr->Int(1);
    }
    static Symbol master("master");
    if (FIND_WITH_BACKUP(master)) {
        mIsMasterRecording = member_arr->Int(1);
    }
    static Symbol real_guitar_tuning("real_guitar_tuning");
    if (FIND_WITH_BACKUP(real_guitar_tuning)) {
        for (int i = 0; i < 6; i++) {
            mRealGuitarTuning[i] = member_arr->Array(1)->Int(i);
        }
    }
    static Symbol real_bass_tuning("real_bass_tuning");
    if (FIND_WITH_BACKUP(real_bass_tuning)) {
        for (int i = 0; i < 4; i++) {
            DataArray *arr = member_arr->Array(1);
            mRealBassTuning[i] = arr->Int(i);
        }
    }
    static Symbol extra_authoring("extra_authoring");
    static Symbol disc_update("disc_update");
    if (FIND_WITH_BACKUP(extra_authoring)) {
        mHasDiscUpdate = member_arr->Contains(disc_update);
    }
    static Symbol solo("solo");
    static Symbol vocals("vocals");
    static Symbol vocal_percussion("vocal_percussion");
    if (FIND_WITH_BACKUP(solo)) {
        DataArray *arr = member_arr->Array(1);
        for (int i = 0; i < arr->Size(); i++) {
            Symbol solosym = arr->Sym(i);
            if (solosym != vocals) {
                if (solosym == vocal_percussion)
                    solosym = vocals;
                mSolos.push_back(solosym);
            }
        }
    }
    static Symbol encoding("encoding");
    static Symbol latin1("latin1");
    bool islatin1 = false;
    if (FIND_WITH_BACKUP(encoding)) {
        islatin1 = member_arr->Sym(1) == latin1;
    } else if (GameOrigin() == "ugc") {
        // Retail BRANCHES on the comparison and stores a literal 1 rather than moving
        // the boolean result, i.e. an `else if`, not `islatin1 = GameOrigin() == "ugc"`.
        islatin1 = true;
    }
    if (islatin1) {
        char buf[0x100];
        ASCIItoUTF8(buf, 0x100, mTitle.c_str());
        mTitle = buf;
        ASCIItoUTF8(buf, 0x100, mArtist.c_str());
        mArtist = buf;
        ASCIItoUTF8(buf, 0x100, mAlbum.c_str());
        mAlbum = buf;
    }
}

int BandSongMetadata::sBandSaveVer = 0x11; // put here to get this damn TU to link

const char *BandSongMetadata::Title() const { return mTitle.c_str(); }
const char *BandSongMetadata::Artist() const { return mArtist.c_str(); }
const char *BandSongMetadata::Album() const { return mAlbum.c_str(); }
int BandSongMetadata::AlbumTrackNum() const { return mAlbumTrackNum; }
Symbol BandSongMetadata::Genre() const { return mGenre; }
int BandSongMetadata::LengthMs() const { return mLengthMs; }
bool BandSongMetadata::HasAlternatePath() const { return mHasAlternatePath; }
bool BandSongMetadata::MuteWinCues() const { return mMuteWinCues; }
const std::hash_map<Symbol, float> &BandSongMetadata::Ranks() const { return mRanks; }
int BandSongMetadata::Rating() const { return mRating; }
float BandSongMetadata::GuidePitchVolume() const { return mGuidePitchVolume; }
int BandSongMetadata::VocalTonicNote() const { return mVocalTonicNote; }

int BandSongMetadata::SongKey() const {
    if (mSongKey >= 0)
        return mSongKey;
    return mVocalTonicNote;
}

int BandSongMetadata::SongTonality() const { return mSongTonality; }
float BandSongMetadata::ScrollSpeed() const { return mSongScrollSpeed; }
float BandSongMetadata::TuningOffset() const { return mTuningOffsetCents; }
const char *BandSongMetadata::VocalPercussionBank() const {
    return mVocalPercussionBank.c_str();
}
const char *BandSongMetadata::DrumKitBank() const { return mDrumKitBank.c_str(); }
bool BandSongMetadata::HasAlbumArt() const { return mHasAlbumArt; }
bool BandSongMetadata::IsMasterRecording() const { return mIsMasterRecording; }
Symbol BandSongMetadata::BandFailCue() const { return mBandFailCue.c_str(); }

int BandSongMetadata::RealGuitarTuning(int i) const {
    SongUpgradeData *data = mSongMgr->GetUpgradeData(ID());
    if (data)
        return data->RealGuitarTuning(i);
    else
        return mRealGuitarTuning[i];
}

int BandSongMetadata::RealBassTuning(int i) const {
    SongUpgradeData *data = mSongMgr->GetUpgradeData(ID());
    if (data)
        return data->RealBassTuning(i);
    else
        return mRealBassTuning[i];
}

Symbol BandSongMetadata::Decade() const {
    int year = mDateReleased.Year();
    Symbol sym = MakeString("the%is", year - (year % 10));
    return sym;
}

// retail fn_82587490: 1-arg overload, no machine-mgr gate (called from Handle)
bool BandSongMetadata::HasPart(Symbol s) const {
    static Symbol real_guitar("real_guitar");
    static Symbol real_bass("real_bass");
    if (s == real_guitar || s == real_bass) {
        SongUpgradeData *upgradeData = mSongMgr->GetUpgradeData(ID());
        if (upgradeData) {
            return upgradeData->HasPart(s);
        }
    }
    std::hash_map<Symbol, float>::const_iterator it = mRanks.find(s);
    return it != mRanks.end() && it->second > 0;
}

bool BandSongMetadata::HasPart(Symbol s, bool b) const {
    BandMachineMgr *mgr = TheSessionMgr ? TheSessionMgr->mMachineMgr : nullptr;
    if (mgr && !b && !mgr->IsSongAllowedToHavePart(ID(), s)) {
        return false;
    } else {
        static Symbol real_guitar("real_guitar");
        static Symbol real_bass("real_bass");
        if (s == real_guitar || s == real_bass) {
            SongUpgradeData *upgradeData = mSongMgr->GetUpgradeData(ID());
            if (upgradeData) {
                return upgradeData->HasPart(s);
            }
        }
        std::hash_map<Symbol, float>::const_iterator it = mRanks.find(s);
        if (it != mRanks.end() && it->second > 0) {
            return true;
        } else
            return false;
    }
}

float BandSongMetadata::Rank(Symbol s) const {
    if (s == real_guitar || s == real_bass) {
        SongUpgradeData *data = mSongMgr->GetUpgradeData(ID());
        if (data) {
            return data->Rank(s);
        }
    }
    std::hash_map<Symbol, float>::const_iterator it = mRanks.find(s);
    if (it != mRanks.end()) {
        return it->second;
    }
    return 0;
}

bool BandSongMetadata::HasVocalHarmony() const {
    static Symbol vocals("vocals");
    return HasPart(vocals, false) && SongMetadata::NumVocalParts() > 1;
}

bool BandSongMetadata::IsPrivate() const {
    return mIsTutorial || mIsFake;
}

bool BandSongMetadata::IsRanked() const { return !mRanks.empty(); }

bool BandSongMetadata::IsVersionOK() const {
    return mVersion >= 0 && mVersion <= 30;
}

Symbol BandSongMetadata::LengthSym() const {
    DataArray *cfg = SystemConfig(song_select, song_lengths);
    for (int i = 1; i < cfg->Size(); i++) {
        DataArray *arr = cfg->Array(i);
        if (arr->Size() == 1 || mLengthMs <= arr->Int(1)) {
            return arr->Sym(0);
        }
    }
    MILO_FAIL("No song_length definitions!");
    return gNullStr;
}

Symbol BandSongMetadata::RatingSym() const {
    return MakeString("rating_%i", (int)mRating);
}

Symbol BandSongMetadata::SourceSym() const {
    bool official_dlc = GameOrigin() == rb3_dlc || GameOrigin() == rb1_dlc;
    if (official_dlc)
        return dlc;
    else
        return GameOrigin() == ugc_plus ? ugc : GameOrigin();
}

Symbol BandSongMetadata::VocalPartsSym() const {
    return MakeString("vocal_parts_%i", NumVocalParts());
}

Symbol BandSongMetadata::HasProGuitarSym() const {
    static Symbol real_guitar("real_guitar");
    static Symbol real_bass("real_bass");
    static Symbol has_part_yes("has_part_yes");
    static Symbol has_part_no("has_part_no");
    if (HasPart(real_guitar, false) || HasPart(real_bass, false))
        return has_part_yes;
    return has_part_no;
}

DECOMP_FORCEFUNC(BandSongMetadata, BandSongMetadata, HasKeys())

#pragma push
#pragma force_active on
inline bool BandSongMetadata::HasKeys() const {
    static Symbol keys("keys");
    static Symbol real_keys("real_keys");
    return HasPart(keys, false) || HasPart(real_keys, false);
}
#pragma pop

bool BandSongMetadata::HasGuitar() const {
    static Symbol guitar("guitar");
    static Symbol real_guitar("real_guitar");
    return HasPart(guitar, false) || HasPart(real_guitar, false);
}

bool BandSongMetadata::HasBass() const {
    static Symbol bass("bass");
    static Symbol real_bass("real_bass");
    return HasPart(bass, false) || HasPart(real_bass, false);
}

Symbol BandSongMetadata::HasKeysSym() const {
    static Symbol has_part_yes("has_part_yes");
    static Symbol has_part_no("has_part_no");
    return HasKeys() ? has_part_yes : has_part_no;
}

bool BandSongMetadata::HasSolo(Symbol s) const {
    static Symbol real_guitar("real_guitar");
    static Symbol guitar("guitar");
    static Symbol real_bass("real_bass");
    static Symbol bass("bass");
    static Symbol real_keys("real_keys");
    static Symbol keys("keys");
    static Symbol real_drum("real_drum");
    static Symbol drum("drum");
    if (s == real_guitar)
        s = guitar;
    else if (s == real_bass)
        s = bass;
    else if (s == real_keys)
        s = keys;
    else if (s == real_drum)
        s = drum;
    return std::find(mSolos.begin(), mSolos.end(), s) != mSolos.end();
}

Symbol BandSongMetadata::HasSoloSym(Symbol s) const {
    static Symbol has_part_yes("has_part_yes");
    static Symbol has_part_no("has_part_no");
    return HasSolo(s) ? has_part_yes : has_part_no;
}

bool BandSongMetadata::IsUGC() const {
    // retail fn_82586C88: function-local lazy statics (this is also what keeps
    // MSVC from inlining this function into Handle)
    static Symbol ugc("ugc");
    static Symbol ugc_plus("ugc_plus");
    return GameOrigin() == ugc || GameOrigin() == ugc_plus;
}

// retail fn_8259E890 (between IsUGC and IsDownload): one function-local lazy
// static "ugc_plus" + GameOrigin() compare (cntlzw/srwi == idiom). Reached
// from the retail-only `is_ugc_plus` handler arm; absent from the rb3-Wii dev
// source, which inlines the ugc_plus test into `is_ugc`.
bool BandSongMetadata::IsUGCPlus() const {
    static Symbol ugc_plus("ugc_plus");
    return GameOrigin() == ugc_plus;
}

const char *BandSongMetadata::MidiUpdate() const {
    if (mHasDiscUpdate)
        return MakeString("./songs/updates/%s/%s_update.mid", mShortName, mShortName);
    else
        return 0;
}

bool BandSongMetadata::IsDownload() const {
    // retail fn_82586DB8: function-local lazy static, non-inlinable
    static Symbol rb3("rb3");
    return GameOrigin() != rb3;
}

// retail mRanks is an STLport hash_map (find helper FUN_82543f88: modulo bucket
// walk, null-node end();  value pair at node+4). Stream format matches the
// std::map operators: size, then key/value pairs in iteration order.
template <class T1, class T2>
BinStream &operator<<(BinStream &bs, const std::hash_map<T1, T2> &map) {
    bs << map.size();
    for (typename std::hash_map<T1, T2>::const_iterator it = map.begin();
         it != map.end(); ++it) {
        bs << it->first << it->second;
    }
    return bs;
}

template <class T1, class T2>
BinStream &operator>>(BinStream &bs, std::hash_map<T1, T2> &map) {
    unsigned int size;
    bs >> size;
    map.clear();
    while (size-- != 0) {
        T1 key;
        bs >> key;
        bs >> map[key];
    }
    return bs;
}

void BandSongMetadata::Save(BinStream &bs) {
    bs << sBandSaveVer;
    SongMetadata::Save(bs);
    bs << mTitle;
    bs << mArtist;
    bs << mAlbum;
    bs << mAlbumTrackNum;
    bs << mDateRecorded;
    bs << mDateReleased;
    bs << mGenre;
    bs << mBasePoints;
    bs << mIsBonus;
    bs << mIsFake;
    bs << mIsTutorial;
    bs << mMuteWinCues;
    bs << mRanks;
    bs << mRating;
    bs << mGuidePitchVolume;
    bs << mSongScrollSpeed;
    bs << mTuningOffsetCents;
    bs << mVocalPercussionBank;
    bs << mDrumKitBank;
    bs << mVocalTonicNote;
    bs << mSongKey;
    bs << mSongTonality;
    bs << mLengthMs;
    bs << mHasAlbumArt;
    bs << mIsMasterRecording;
    bs << mHasAlternatePath;
    bs << mAnimTempo;
    bs << mVocalGender;
    for (int i = 0; i < 6; i++)
        bs << mRealGuitarTuning[i];
    for (int i = 0; i < 4; i++)
        bs << mRealBassTuning[i];
    bs << mHasDiscUpdate;
    bs << mSolos;
}

void BandSongMetadata::Load(BinStream &bs) {
    int rev;
    bs >> rev;
    SongMetadata::Load(bs);
    bs >> mTitle;
    bs >> mArtist;
    bs >> mAlbum;
    bs >> mAlbumTrackNum;
    bs >> mDateRecorded;
    bs >> mDateReleased;
    bs >> mGenre;
    if (rev < 0xD) {
        String s;
        bs >> s;
    }
    bs >> mBasePoints;
    bs >> mIsBonus;
    bs >> mIsFake;
    bs >> mIsTutorial;
    bs >> mMuteWinCues;
    bs >> mRanks;
    bs >> mRating;
    if (rev < 0xC) {
        short s;
        bs >> s;
    }
    bs >> mGuidePitchVolume;
    if (rev < 0xF) {
        String s;
        bs >> s;
    }
    bs >> mSongScrollSpeed;
    bs >> mTuningOffsetCents;
    bs >> mVocalPercussionBank;
    if (rev >= 9)
        bs >> mDrumKitBank;
    if (rev >= 1)
        bs >> mVocalTonicNote;
    if (rev >= 11) {
        bs >> mSongKey;
        bs >> mSongTonality;
    }
    if (rev >= 2 && rev < 0xE) {
        std::vector<std::map<Symbol, String> > gross;
        bs >> gross;
    }
    if (rev >= 3)
        bs >> mLengthMs;
    if (rev >= 4) {
        bs >> mHasAlbumArt;
        bs >> mIsMasterRecording;
    }
    if (rev >= 5) {
        bs >> mHasAlternatePath;
        if (rev < 8) {
            bool b, c;
            bs >> b;
            bs >> c;
        }
    }
    if (rev >= 6)
        bs >> mAnimTempo;
    if (rev >= 16)
        bs >> mVocalGender;
    if (rev >= 7) {
        for (int i = 0; i < 6; i++)
            bs >> mRealGuitarTuning[i];
        for (int i = 0; i < 4; i++)
            bs >> mRealBassTuning[i];
    }
    if (rev >= 10)
        bs >> mHasDiscUpdate;
    if (rev >= 0x11)
        bs >> mSolos;
}

BEGIN_HANDLERS(BandSongMetadata)
    HANDLE_EXPR(id, ID())
    HANDLE_EXPR(title, mTitle.c_str())
    HANDLE_EXPR(genre, mGenre)
    HANDLE_EXPR(anim_tempo, mAnimTempo)
    HANDLE_EXPR(vocal_gender, mVocalGender)
    HANDLE_EXPR(year_released, mDateReleased.Year())
    HANDLE_EXPR(year_recorded, mDateRecorded.Year())
    HANDLE_EXPR(length_ms, mLengthMs)
    HANDLE_EXPR(has_part, HasPart(_msg->Sym(2), false))
    HANDLE_EXPR(is_ugc, IsUGC())
    HANDLE_EXPR(is_ugc_plus, IsUGCPlus())
    HANDLE_EXPR(is_download, IsDownload())
    HANDLE_EXPR(rating, mRating)
    HANDLE_SUPERCLASS(SongMetadata)
    HANDLE_CHECK(0x379)
END_HANDLERS
