#include "meta_band/BandSongMgr.h"
#include "Utl.h"
#include "beatmatch/TrackType.h"
#include "decomp.h"
#include "meta/SongMgr.h"
#include "meta_band/BandMachine.h"
#include "meta_band/BandSongMetadata.h"
#include "meta_band/ProfileMgr.h"
#include "meta_band/SaveLoadManager.h"
#include "meta_band/SessionMgr.h"
#include "meta_band/SongUpgradeMgr.h"
#include "meta_band/UIEventMgr.h"
#include "net_band/RockCentral.h"
#include "obj/Data.h"
#include "obj/DataFile.h"
#include "obj/Dir.h"
#include "game/GameMode.h"
#include "obj/DirLoader.h"
#include "os/Archive.h"
#include "os/ContentMgr.h"
#include "os/Debug.h"
#include "os/File.h"
#include "os/System.h"
#include "stl/_pair.h"
#include "utl/CacheMgr.h"
#include "utl/FakeSongMgr.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"
#include "utl/Messages2.h"
#include "utl/Symbols4.h"

#ifdef HX_NATIVE
#include <sys/stat.h>
#endif

BandSongMgr gSongMgr;
BandSongMgr *TheSongMgrPtr = &gSongMgr;
SongMgr *TheBaseSongManger;

// Retail-only helper (target fn_82586AB0, called from AddSongData's 360-only
// tail block). Zero args, single caller, no rb3-Wii/dc3 oracle -- exact
// source/name unidentified (guarded local-static Symbols "rb1_dlc"/"ugc"/
// "rb3_dlc"/"ugc_plus" per Ghidra decompile). Extern-declared for call-site
// codegen only; see docs cited in AddSongData.
extern bool RB3AddSongDataUpgradeGate();

bool BandSongMgr::sFakeSongsAllowed;

const char *OLD_DLC_DIR = "songs/updates/";

struct ExclusionEntry {
    const char *name;
    int songID;
};

/* TWO entries, not the four the rb3-Wii dev oracle carries. This is a real
   behaviour change and it is deliberate: our target is TU5, and TU5 shrank this
   table. Evidence is retail bytes, not the oracle (lane CR-3):

     - TU5 .rdata @0x8209DE28 holds exactly two {const char*, int} pairs:
       0x8209DE00 "hierkommtalex"/1005106 and 0x8209DDF0 "rockandrollstar"/1005109.
       Index [2] is string-pool garbage, so the array genuinely ends at two.
     - TU5's IsInExclusionList (0x825755B8) bounds its loop `cmplwi r7,0x10`
       = 16 bytes = 2 entries, which is where our `i < 2U` came from.
     - TU0 (orig/45410914/tu0-archive/band.exe) has the OTHER shape: a FOUR-entry
       table @0x8209C1B4 in exactly the Wii order (danicalifornia/8,
       blackholesun/3, hierkommtalex/1005106, rockandrollstar/1005109) and a loop
       bounded `cmplwi r7,0x20` = 4 entries. So the Wii oracle == TU0, and TU5 is
       a deliberate Harmonix source change (those two songs stopped being excluded).
     - Corroborated independently by string presence: TU0 contains bare C-strings
       for all four names; TU5 contains bare strings ONLY for hierkommtalex and
       rockandrollstar. (danicalifornia/blackholesun still appear in TU5 but only
       inside "./songs/updates/<name>/<name>_update.mid" paths -- which is the
       false positive that made an earlier lane conclude "all four are present in
       retail". Presence is not table membership.)

   Keeping the Wii's four entries alongside TU5's `i < 2U` bound was the worst of
   both worlds: it checked danicalifornia/blackholesun and never looked at the two
   songs TU5 actually excludes. Not scored either way -- BandSongMgr.cpp pins only
   .text, so this array is not in the diffed sections. Note also that retail's copy
   lives in .rdata, i.e. it is const in the real source; left non-const here because
   nothing measures it and const-ness is not what this fix is about. */
ExclusionEntry exclusionList[] = {
    { "hierkommtalex", 1005106 },
    { "rockandrollstar", 1005109 },
};

BandSongMgr::BandSongMgr()
    : unkc0(0), unk124(1), mJukebox(2000), mUpgradeMgr(0), mLicenseMgr(0),
      mMaxSongCount(-1), unk13c(0) {
    ClearAndShrink(mContentAltDirs);
    TheBaseSongManger = this;
}

void BandSongMgr::Init() {
    SongMgr::Init();
    SetName("song_mgr", ObjectDir::Main());
    TheContentMgr.RegisterCallback(this, false);
    mSongNameLookup.clear();
    mSongIDLookup.clear();
    DataArray *cfg = SystemConfig(song_mgr);
    DataArray *altarr = cfg->FindArray(alt_dirs, false);
    if (altarr) {
        for (int i = 1; i < altarr->Size(); i++) {
            const char *str = altarr->Array(i)->Str(0);
            if (strlen(str) != 0) {
                mContentAltDirs.push_back(String(str));
            }
        }
    }
    mMaxSongCount = cfg->FindInt(max_song_count);
    mUpgradeMgr = new SongUpgradeMgr();
    mLicenseMgr = new LicenseMgr();
}

bool BandSongMgr::CreateSongCacheID(CacheID **id) {
    bool ret;
    *id = nullptr;
    DataArray *cfg = SystemConfig("net", "cache");
    const char *filename = cfg->FindStr("file_name");
    const char *bannername = cfg->FindStr("banner_name");
    const char *bannerdesc = cfg->FindStr("banner_desc");
    ret = TheCacheMgr->CreateCacheID(
        filename, bannername, nullptr, bannerdesc, nullptr, 0x400, id
    );
    MILO_ASSERT(ret, 0x8C);
    return ret;
}

void BandSongMgr::Terminate() {
    RELEASE(unkc0);
    RELEASE(mLicenseMgr);
    RELEASE(mUpgradeMgr);
    ClearAndShrink(mContentAltDirs);
    TheContentMgr.UnregisterCallback(this, false);
    mSongNameLookup.clear();
    mSongIDLookup.clear();
}

struct SongRankCmp {
    SongRankCmp(BandSongMgr *mgr, Symbol s) : mSongMgr(mgr), mPart(s) {}
    bool operator()(int x, int y) const {
        float rankX = ((BandSongMetadata *)mSongMgr->Data(x))->Rank(mPart);
        float rankY = ((BandSongMetadata *)mSongMgr->Data(y))->Rank(mPart);
        if (rankX == rankY)
            return x < y;
        else
            return rankX < rankY;
    }

    BandSongMgr *mSongMgr;
    Symbol mPart;
};

void BandSongMgr::ContentDone() {
    SongMgr::ContentDone();
    mSongRankings.clear();
    std::vector<int> songs;
    GetRankedSongs(songs, false, true);
    Symbol instSyms[9] = { "band", "guitar",      "vocals",    "drum",     "bass",
                           "keys", "real_guitar", "real_bass", "real_keys" };
    for (int i = 0; i < 9U; i++) {
        SongRanking ranking;
        ranking.mInstrument = instSyms[i];
        std::vector<int> curSongs = songs;
        std::vector<int> i80;
        std::sort(
            curSongs.begin(), curSongs.end(), SongRankCmp(this, ranking.mInstrument)
        );
        FOREACH (it, curSongs) {
            BandSongMetadata *data = (BandSongMetadata *)Data(*it);
            if (!data->IsDownload() && data->HasPart(ranking.mInstrument, false)) {
                i80.push_back(*it);
            }
        }
        int i4 = i80.size();
        if (i4 != 0) {
            int numRanks = SystemConfig("song_groupings", "rank")->Size() - 1;
            int i11 = 0;
            for (int i = 0; i < numRanks; i++) {
                int i8 = i4 / numRanks;
                if (i >= numRanks - (i4 % numRanks)) {
                    i8++;
                }
                if (i8 != 0) {
                    int i2 = i80[i11];
                    int i5 = i80[i11 + i8 - 1];
                    BandSongMetadata *b2 = (BandSongMetadata *)Data(i2);
                    BandSongMetadata *b5 = (BandSongMetadata *)Data(i5);
                    float r5 = b5->Rank(ranking.mInstrument);
                    float r2 = b2->Rank(ranking.mInstrument);
                    ranking.mTierRanges.push_back(std::make_pair(r2, r5));
                    i11 += i8;
                } else {
                    MILO_WARN(
                        "Instrument %s only has %d songs but %d groups. Group                        %d will have no songs.\n",
                        ranking.mInstrument,
                        i80.size(),
                        numRanks,
                        i
                    );
                }
            }
        }
        mSongRankings.push_back(ranking);
    }
    unk124 = false;
    SyncSharedSongs();
    if (TheRockCentral.IsOnline()) {
        std::vector<BandProfile *> profiles = TheProfileMgr.GetSignedInProfiles();
        std::vector<int> songs2;
        GetRankedSongs(songs2, false, true);
        std::vector<int> ic8;
        FOREACH (it, songs2) {
            int cur = *it;
            if (mUpgradeMgr->HasUpgrade(cur)) {
                ic8.push_back(cur);
            }
        }
        TheRockCentral.SyncAvailableSongs(profiles, songs2, ic8, nullptr);
    }
    if (mSongCacheWriteAllowed && TheSaveLoadMgr) {
        TheSaveLoadMgr->AutoSave();
    }
}

void BandSongMgr::ContentMounted(const char *c1, const char *c2) {
    SongMgr::ContentMounted(c1, c2);
}

const char *BandSongMgr::ContentPattern() {
    // Retail 360 returns a bare constant -- NOT the rb3-Wii dev build's
    // `TheArchive ? "&songs*.dta" : "&songs*.dt?"`, which is what this used to
    // be (../rb3/src/band3/meta_band/BandSongMgr.cpp:204, copied verbatim).
    // Adjudicated on retail bytes with no map involved: this is vtable slot 10
    // of the ContentMgr::Callback subobject, 0x82575558, whose ENTIRE body is
    // `lis r11,0x820a; addi r3,r11,-0x21c8; blr` -- 12 bytes, no load of
    // TheArchive, no compare, no branch, so the conditional form cannot
    // compile to it.  The constant it returns is 0x8209DE38 == "songs.dta".
    // (Slot 11 is 0x82575568 -> 0x82000FE4 == "songs", which is ContentDir --
    // that pair is what pins the slot numbering without trusting the map.)
    return "songs.dta";
}

const char *BandSongMgr::ContentDir() { return "songs"; }

Symbol BandSongMgr::GetShortNameFromSongID(int songID, bool fail) const {
    MILO_ASSERT(songID != kSongID_Invalid && songID != kSongID_Any && songID != kSongID_Random, 0x156);
    std::hash_map<int, Symbol>::const_iterator it = mSongNameLookup.find(songID);
    if (it != mSongNameLookup.end())
        return it->second;
    it = mExtraSongIDMap.find(songID);
    if (it != mExtraSongIDMap.end())
        return it->second;
    if (fail) {
        MILO_FAIL("Couldn't find song short name for SONG_ID %d", songID);
    }
    return gNullStr;
}

int BandSongMgr::GetSongIDFromShortName(Symbol shortName, bool fail) const {
    static const int kSongID_Invalid = 0; // shadow the wrong header constant in this fn (target asm uses cmpwi 0)
    std::hash_map<Symbol, int>::const_iterator it = mSongIDLookup.find(shortName);
    if (it != mSongIDLookup.end()) {
        MILO_ASSERT(it->second != kSongID_Invalid, 0x16D);
        return it->second;
    } else {
        for (std::hash_map<int, Symbol>::const_iterator it = mExtraSongIDMap.begin();
             it != mExtraSongIDMap.end();
             ++it) {
            if (it->second == shortName) {
                MILO_ASSERT(it->first != kSongID_Invalid, 0x176);
                return it->first;
            }
        }
        if (fail) {
            MILO_FAIL(
                "Couldn't find song ID for short name \"%s\", does song have a SONG_ID?",
                shortName.Str()
            );
        }
        return 0;
    }
}

void BandSongMgr::GetContentNames(Symbol s, std::vector<Symbol> &vec) const {
    SongMgr::GetContentNames(s, vec);
    int songID = GetSongIDFromShortName(s, false);
    if (mUpgradeMgr->HasUpgrade(songID)) {
        const char *contentName = mUpgradeMgr->ContentName(songID);
        // Retail emits a real null test here (mr. r4,r3 / beq), not an assert.
        if (contentName && !streq(contentName, ".")) {
            vec.push_back(contentName);
        }
    }
}

SongMetadata *BandSongMgr::Data(int i) const {
    return const_cast<SongMetadata *>(SongMgr::Data(i));
}

SongInfo *BandSongMgr::SongAudioData(int i) const {
    SongMetadata *data = Data(i);
    if (!data)
        return 0;
    else {
        SongInfo *songInfo = data->SongBlock();
        MILO_ASSERT(songInfo, 0x1a0);
        RELEASE(unkc0);
        unkc0 = new DataArraySongInfo(songInfo);
        const char *update = ((BandSongMetadata *)data)->MidiUpdate(); // lol can you
                                                                       // actually do this
        if (update)
            unkc0->AddExtraMidiFile(update, 0);
        if (mUpgradeMgr->HasUpgrade(i)) {
            SongUpgradeData *upgrade = mUpgradeMgr->UpgradeData(i);
            MILO_ASSERT(upgrade, 0x1C3);
            unkc0->AddExtraMidiFile(UpgradeMidiFile(i), 0);
        }
        return unkc0;
    }
}

const char *BandSongMgr::SongName(int i) const {
    BandSongMetadata *data = (BandSongMetadata *)Data(i);
    if (data)
        return data->Title();
    else
        return gNullStr;
}

const char *BandSongMgr::SongName(Symbol s) const {
    return SongName(GetSongIDFromShortName(s, true));
}

const char *BandSongMgr::UpgradeMidiFile(int i) const {
    const char *file = gNullStr;
    if (mUpgradeMgr->HasUpgrade(i)) {
        SongUpgradeData *upgrade = mUpgradeMgr->UpgradeData(i);
        MILO_ASSERT(upgrade, 0x20A);
        file = upgrade->MidiFile();
    }
    return file;
}

const char *BandSongMgr::MidiFile(Symbol s) const {
    SongInfo *info = SongMgr::SongAudioData(s);
    if (info)
        return FakeSongMgr::MidiFile(info);
    else
        return gNullStr;
}

const char *BandSongMgr::SongFilePath(Symbol s1, const char *cc) const {
    const char *path = gNullStr;
    BandSongMetadata *data = (BandSongMetadata *)Data(GetSongIDFromShortName(s1, true));
    if (data) {
        if (data->IsDownload()) {
            String str =
                MakeString("%s%s", SongMgr::SongAudioData(s1)->GetBaseFileName(), cc);
            unsigned int idx = str.find("_song");
            if (idx != String::npos) {
                str.erase(idx, 5);
            }
            path = MakeString("%s", str);
        } else if (data->HasAlternatePath()) {
            const char *base =
                FileGetBase(SongMgr::SongAudioData(s1)->GetBaseFileName());
            path = MakeString("%s%s/%s%s", OLD_DLC_DIR, base, base, cc);
        } else {
            path = MakeString("%s%s", SongMgr::SongAudioData(s1)->GetBaseFileName(), cc);
        }
    }
    if (!UsingCD()) {
        if (!data || (!data->IsOnDisc() && !data->HasAlternatePath())) {
            if (streq(cc, ".milo")) {
                DirLoader::SetCacheMode(true);
                path = DirLoader::CachedPath(path, false);
                DirLoader::SetCacheMode(false);
            }
        }
    }
    return path;
}

DECOMP_FORCEACTIVE(BandSongMgr, "")

const char *BandSongMgr::GetAlbumArtPath(Symbol s) const {
    if (HasSong(s, true)) {
        const char *path = SongFilePath(s, "_keep.png");
#ifdef HX_NATIVE
        // The extracted native asset set ships the song .mogg/.milo but NOT the
        // per-song "<name>_keep.png" album-art textures, so this path resolves to
        // a file that does not exist. The song-select UI then does
        // `album_art.pic set tex_file <path>`, which fails the texture load and
        // leaves a flat grey placeholder box obscuring the difficulty grid.
        // Mirror the no-art branch (OwnedSongSortNode::GetAlbumArtPath) and fall
        // back to the shipped blank-art texture when the file is absent, so the
        // box shows the proper "no art" placeholder instead of grey. Cheap local
        // stat() — the path is relative to the data dir the native boot chdir'd
        // into. Real art (if ever shipped) still resolves to the song path.
        if (path && *path) {
            struct stat st;
            if (::stat(path, &st) != 0)
                return "ui/image/blank_album_art_keep.png";
        }
#endif
        return path;
    } else
        return gNullStr;
}

const char *BandSongMgr::SongPath(Symbol s) const {
    return SongMgr::SongAudioData(s)->GetBaseFileName();
}

int BandSongMgr::RankTier(float f, Symbol s) const {
    std::list<SongRanking>::const_iterator r =
        std::find(mSongRankings.begin(), mSongRankings.end(), s);
    MILO_ASSERT(r != mSongRankings.end(), 0x278);
    int i = 0;
    for (; i < r->mTierRanges.size(); i++) {
        if (f <= r->mTierRanges[i].second)
            return i;
    }
    return i - 1;
}

int BandSongMgr::NumRankTiers(Symbol s) const {
    std::list<SongRanking>::const_iterator r =
        std::find(mSongRankings.begin(), mSongRankings.end(), s);
    MILO_ASSERT(r != mSongRankings.end(), 0x289);
    return r->mTierRanges.size();
}

Symbol BandSongMgr::RankTierToken(int i) const {
    return SystemConfig(song_groupings, rank)->Array(i + 1)->FindSym(band);
}

void BandSongMgr::GetRankedSongs(std::vector<int> &vec, bool b1, bool b2) const {
    if (b1) {
        TheGameMode->Property("demos_allowed", true)->Int();
    }
    vec.clear();
    for (std::set<int>::const_iterator it = mAvailableSongs.begin();
         it != mAvailableSongs.end();
         ++it) {
        int cur = *it;
        BandSongMetadata *data = (BandSongMetadata *)Data(cur);
        if (data->IsRanked() && !data->IsPrivate() && (b2 || !IsRestricted(cur))) {
            vec.push_back(*it);
        }
    }
}

int BandSongMgr::GetValidSongCount(const std::hash_map<int, SongMetadata *> &songs) const {
    int count = 0;
    for (std::hash_map<int, SongMetadata *>::const_iterator it = songs.begin();
         it != songs.end();
         ++it) {
        BandSongMetadata *data = (BandSongMetadata *)it->second;
        if (data && data->IsRanked() && !data->IsPrivate())
            count++;
    }
    return count;
}

bool BandSongMgr::IsSongUnplayable(int songID, BandUserMgr &mgr, bool bvar3) const {
    BandSongMetadata *data = (BandSongMetadata *)Data(songID);
    Symbol shortname = GetShortNameFromSongID(songID, true);
    if (data->IsPrivate()) {
        MILO_WARN("%s is private!", shortname);
        return false;
    } else if (!data->IsRanked()) {
        MILO_WARN("%s is not ranked!", shortname);
        return false;
    } else {
        std::vector<BandUser *> users;
        mgr.GetParticipatingBandUsers(users);
        bool b5 = false;
        bool b4 = false;
        bool b3 = false;
        int i12 = 0;
        int i7 = 0;
        FOREACH (it, users) {
            Symbol controller = (*it)->GetControllerSym();
            MILO_ASSERT(controller != none, 0x2E3);
            if (controller == drum)
                b5 = true;
            else if (controller == vocals)
                b4 = true;
            else if (controller == keys)
                b3 = true;
            else if (controller == real_guitar)
                i7++;
            else
                i12++;
        }
        bool b1 = false;
        bool b11 = true;
        if (i12 == 1) {
            b1 = true;
            if (!data->HasPart(guitar, false) && !data->HasPart(bass, false)) {
                b1 = false;
            }
            if (!b1)
                b11 = false;
        }
        if (i12 > 1) {
            if (!data->HasPart(guitar, false) || !data->HasPart(bass, false)) {
                b11 = false;
            }
            if (data->HasPart(guitar, false) || data->HasPart(bass, false)) {
                b1 = true;
            }
        }
        if (i7 == 1) {
            if (data->HasPart(real_guitar, false) || data->HasPart(real_bass, false)) {
                b11 = false;
            } else
                b1 = true;
        }
        if (i7 > 1) {
            if (!data->HasPart(real_guitar, false) || !data->HasPart(real_bass, false)) {
                b11 = false;
            }
            if (data->HasPart(real_guitar, false) || data->HasPart(real_bass, false)) {
                b1 = true;
            }
        }
        if (b3) {
            if (data->HasPart(keys, false))
                b1 = true;
            else
                b11 = false;
        }
        if (b4) {
            if (data->HasPart(vocals, false))
                b1 = true;
            else
                b11 = false;
        }
        if (b5) {
            if (data->HasPart(drum, false))
                b1 = true;
            else
                b11 = false;
        }
        if (bvar3)
            return !b11;
        else
            return !b1;
    }
}

// Byte-adjudicated in AllowContentToBeAdded (retail 0x8257ADE8):
//     lwz r11,0x68(r30) / lwz r10,0x48(r30) / add r11,r10,r11
// i.e. TWO plain member loads and no call at all. 0x48-0x34 == 0x68-0x54 == 0x14,
// which is _M_num_elements inside each hash_map, so this is just the two sizes
// summed. The old form read a cache at +0x154 -- an offset that appears ZERO
// times in the whole pinned retail TU under any base register -- and a first
// attempt to replace it with GetValidSongCount() was refuted the same way:
// retail's AllowContentToBeAdded calls GetValidSongCount ZERO times.
int BandSongMgr::GetCurSongCount() const {
    return mUncachedSongMetadata.size() + mCachedSongMetadata.size();
}
bool BandSongMgr::CanAddSong() const {
    int maxSongCount = mMaxSongCount;
    return GetCurSongCount() + 1 < maxSongCount;
}
int BandSongMgr::GetMaxSongCount() const { return mMaxSongCount; }

void BandSongMgr::AddSongData(DataArray *a, DataLoader *dl, ContentLocT lt) {
    char cc[256] = ".";
    if (dl) {
        const char *path = FileGetPath(dl->LoaderFile().c_str());
        FileGetPathBuf(path, cc);
    }
    std::vector<int> vec;
    AddSongData(a, mUncachedSongMetadata, cc, lt, vec);
}

void BandSongMgr::AddSongData(
    DataArray *a,
    std::hash_map<int, SongMetadata *> &map,
    const char *cc,
    ContentLocT loct,
    std::vector<int> &ivec
) {
    int arrSize = a->Size();
    int numSongs = CountSongsInArray(a);
    for (int i = arrSize - numSongs; i < arrSize; i++) {
        DataArray *curArray = a->Array(i);
        Symbol curSym = curArray->Sym(0);
        // Retail constructs this as a function-local static Symbol (guarded
        // lazy init, per-loop-iteration guard-bit check) rather than
        // referencing the Symbols4.h global of the same name -- confirmed
        // from the target asm listing (lis lbl_8209C958 / lbl_82DA0017 guard
        // pair + ??0Symbol ctor call), same RB3_HANDLE_LOCAL_STATIC lever
        // this TU is already gated for. The local shadows the extern global.
        static Symbol missing_song_data("missing_song_data");
        DataArray *cfgArr = SystemConfig(missing_song_data)->FindArray(curSym, false);
        int songID = GetSongID(curArray, cfgArr);
        // Retail-360 (TU5) tail guard: song ID 0x05E69EC1 is skipped for the
        // Data()/AddRecentSong priming below. Recovering this bool is what
        // makes the whole function's register allocation line up (retail saves
        // r16..r31, one more callee-save than the rb3-Wii shape) -- without it
        // every GPR in the loop is off by one.
        bool isReservedSongID = songID == 0x05E69EC1;
        /* ⛔ KNOWN 1-INSTRUCTION MISMATCH, DELIBERATELY NOT "FIXED" (lane CR-3).
           AddSongData is 99.632% / 652 B with exactly one mismatch in 163: retail
           TU5 has `li r3,0` at index [60] where we emit this `bl`. Do not chase it
           -- it is not reachable from C++, and both obvious source forms make the
           function strictly WORSE. Measured, not assumed:

             - TU5 @0x82579080: lwz r4,0x54(r31) / mr r3,r26 / mr r5,r27  -- a full,
               live 3-argument setup for IsInExclusionList(this, name, songID) --
               then `li r3,0`, then `clrlwi. r11,r3,0x18; bne`. The branch is NOT
               folded even though r3 is provably 0, and six instructions of dead
               code are left standing.
             - TU0 @0x8256160C has the SAME argument setup and the SAME
               normalize+branch, with a real `bl 0x8255DF90`. One instruction
               differs between the two builds.
             - In TU5 the callee survives at 0x825755B8 with ZERO callers in .text
               and ZERO data references anywhere in the image.

           Two source hypotheses were built and both were REFUTED empirically
           against this compiler (cl 10224, /O1):
             (A) `bool excluded = false; if (excluded)`  -> 94.9%
             (B) early `return false;` in IsInExclusionList so it inlines as a
                 constant (this was the standing hypothesis: "inlined callee folded
                 to return false with the call-site bool-normalize left in") -> 94.9%
           Both produce the IDENTICAL diff: MSVC deletes the three argument-setup
           instructions AND folds the branch away (7 deletes, 2 replaces). This
           compiler never leaves a bool-normalize on a constant, so hypothesis (B)
           is false as a mechanism, not merely unlucky.

           A live argument setup feeding a call that is not made, with the callee
           left in the image unreferenced, is the signature of a post-compile
           instruction substitution in the shipped TU5 image -- not of codegen.
           Trading one mismatched instruction for six is a regression; leaving the
           honest `bl` here costs exactly 1 matched function and 652 B. */
        if (IsInExclusionList(curSym.Str(), songID)) {
            MILO_LOG("Skipping song %s because not licensed for RB3.\n", curSym);
        } else if (songID == 0) {
            MILO_LOG("The song %s has an invalid songID. Skipping.\n", curSym);
        } else if (HasSong(songID)) {
            MILO_LOG("The song %s was found twice in the song manager data.\n", curSym);
        } else {
            bool isRoot = loct == kLocationRoot;
            AddSongIDMapping(songID, curSym);
            if (map.find(songID) != map.end()) {
                delete map.find(songID)->second;
            }
            if (cfgArr) {
                map[songID] = new BandSongMetadata(cfgArr, curArray, isRoot, this);
            } else {
                map[songID] = new BandSongMetadata(curArray, nullptr, isRoot, this);
            }
            mAvailableSongs.insert(songID);
            ivec.push_back(songID);
            // Retail-360-only tail (TU5-era, no rb3-Wii oracle -- confirmed via
            // Ghidra decompile of target 0x82561530 + raw asm listing): after
            // registering a newly-added song, prime its metadata (discarding
            // the result) and conditionally register it in the recent-songs
            // list. unk124 (0x144) is the cache-dirty flag SongMgr::
            // ClearCachedContent()/ReadCachedMetadataFromStream() already
            // toggle. Data(songID) dispatches through the vtable @+0x40
            // (confirmed: scripts/target_symbol_map.json maps 0x82783FA8 to
            // ?Data@SongMgr@@UBAPBVSongMetadata@@H@Z). The gate predicate
            // (target fn_82586AB0) is a single-caller, zero-arg static helper
            // with no rb3-Wii/dc3 counterpart -- extern-declared below for
            // call-site codegen only, exact source identity unresolved.
            if (!isReservedSongID && !unk124) {
                Data(songID);
                if (RB3AddSongDataUpgradeGate())
                    AddRecentSong(songID);
            }
        }
    }
}

/* Matches retail TU5 100% (27/27 instructions). The literal 2U is the length of
   exclusionList above and must move with it -- TU5 bounds this `cmplwi r7,0x10`
   (2 entries); TU0 bounds it `cmplwi r7,0x20` (4). Deliberately spelled as a
   literal rather than sizeof(): the literal is what currently reproduces retail
   byte-for-byte, and there is nothing to gain by perturbing a 100% function.
   NOTE: in TU5 this function is UNREACHABLE -- see the refusal note in
   AddSongData. It has zero callers anywhere in TU5's .text and zero data
   references; TU0 calls it exactly once, from AddSongData. */
bool BandSongMgr::IsInExclusionList(const char *name, int songID) const {
    unsigned int i = 0;
    do {
        if (songID == exclusionList[i].songID || strcmp(name, exclusionList[i].name) == 0)
            return true;
        i++;
    } while (i < 2U);
    return false;
}

bool BandSongMgr::AllowContentToBeAdded(DataArray *a, ContentLocT lt) {
    if (lt == kLocationRoot)
        return true;
    unsigned int count = CountSongsInArray(a);
    while (count + GetCurSongCount() >= mMaxSongCount) {
        if (!RemoveOldestCachedContent())
            break;
    }
    int maxCount = mMaxSongCount;
    int full = (count + GetCurSongCount() >= maxCount);
    if (full) {
        if (!unk13c) {
            static Symbol song_mgr_full("song_mgr_full");
            static Message init_msg("init");
            TheUIEventMgr->TriggerEvent(song_mgr_full, init_msg.mData);
            unk13c = true;
        }
    } else {
        unk13c = false;
    }
                                                                return !(full);
}

int BandSongMgr::GetValidSongs(
    const std::vector<int> &excludeList,
    BandUserMgr &mgr,
    std::vector<int> &outSongs,
    float minRank,
    float maxRank,
    bool filterUnplayable,
    bool allowUGC
) const {
    outSongs.clear();
    std::vector<int> ranked;
    GetRankedSongs(ranked, false, false);
    FOREACH (it, ranked) {
        int songID = *it;
        BandSongMetadata *songData = (BandSongMetadata *)Data(songID);
        if (!songData->IsVersionOK())
            continue;
        // Check if song is in the exclude list
        std::vector<int>::const_iterator found =
            std::find(excludeList.begin(), excludeList.end(), songID);
        if (found != excludeList.end())
            continue;
        if (TheSessionMgr && !TheSessionMgr->GetMachineMgr()->IsSongShared(songID))
            continue;
        if (songData->IsUGC() && !allowUGC)
            continue;
        if (IsSongUnplayable(songID, mgr, filterUnplayable))
            continue;
        int skipMinRank = 0;
        if (minRank != -1.0f) {
            if (songData->Rank(band) < minRank)
                skipMinRank = 1;
        }
        if (!skipMinRank) {
            int skipMaxRank = 0;
            if (maxRank != -1.0f) {
                if (songData->Rank(band) > maxRank)
                    skipMaxRank = 1;
            }
            if (!skipMaxRank)
                outSongs.push_back(songID);
        }
    }
    return outSongs.size();
}

int BandSongMgr::GetPosInRecentList(int) { return -1; }
// Retail fn_82575F68 (0xA4 B). rb3-Wii's DEV decomp has `return false;` here and
// so did we -- BYTE-IDENTICAL to the oracle, so a source diff showed NOTHING.
// The tell was in the CALLER: retail's `is_demo` handler in Handle() emits
// `bl fn_82575F68`, while a `return false;` stub gets /Ob2-inlined to nothing,
// costing Handle 4 instructions and perturbing its register allocation.
// Reconstructed from the retail bytes, not from oracle preference:
//   vtable slot 0x40 -> Data(int) (same slot fn_827A8EC8/ContentName uses),
//   bl ?IsUGC@BandSongMetadata@@QBA_NXZ, the 0x05E69EC1 reserved-song-ID guard
//   this TU already names at AddSongs, ??0Symbol@@QAA@PBD@Z on ContentName's
//   result, then __find over the vector at +0x138/+0x13c (= unk11c) with the
//   result compared against the END pointer -- `subf/cntlzw/extrwi ...,1,26`
//   sets r3 iff (result == end), i.e. the return is TRUE when NOT FOUND.
bool BandSongMgr::IsDemo(int songID) const {
    BandSongMetadata *data = (BandSongMetadata *)Data(songID);
    if (data->IsUGC())
        return false;
    if (songID == 0x05E69EC1)
        return false;
    Symbol shortname(ContentName(songID));
    return std::find(unk11c.begin(), unk11c.end(), shortname) == unk11c.end();
}

bool BandSongMgr::IsRestricted(int songID) const {
    BandSongMetadata *data = (BandSongMetadata *)Data(songID);
    int rating = data->Rating();
    bool notAllowed = !AllowedToAccessContent(rating);
    if (notAllowed) {
        MILO_WARN(
            "Song %d has rating %d, which should mean it is restricted", songID, rating
        );
    }
    return false;
}

SongUpgradeData *BandSongMgr::GetUpgradeData(int i) const {
    return mUpgradeMgr->UpgradeData(i);
}

bool BandSongMgr::HasLicense(Symbol s) const { return mLicenseMgr->HasLicense(s); }

void BandSongMgr::AddSongIDMapping(int i, Symbol s) {
    // Retail compiled these two duplicate-detection probes OUT. Proven from
    // retail bytes for ?AddSongIDMapping@BandSongMgr@@ (lane MATCH-F): the
    // target is 72 B / 30 instructions and performs ONLY the two operator[]
    // insertions below -- it contains ZERO calls to hash_map::_M_find, while
    // ours inserts exactly two (`_M_find@H@...` and `_M_find@VSymbol@@...`),
    // i.e. the `.find()` in each `if` condition. Each `if` body is nothing but
    // a MILO_WARN, so the whole probe is diagnostics-only and the conditions
    // must be guarded along with the warning: the non-HX_NATIVE MILO_WARN is
    // `MiloStripEval(__VA_ARGS__)`, which strips the OUTPUT but still
    // EVALUATES its arguments. House per-site guard, not a blanket removal --
    // the measured whole-binary control for blanket-stripping MILO_DEBUG is
    // -21. See docs/decomp/patterns/milo-debug-force-define.md.
#if defined(MILO_DEBUG) && defined(HX_NATIVE)
    std::hash_map<int, Symbol>::const_iterator nameIt = mSongNameLookup.find(i);
    if (nameIt != mSongNameLookup.end() && i != 0 && nameIt->second != s) {
        Symbol val = nameIt->second;
        MILO_WARN("Song %s and song %s have duplicate song_id %d!", s, val, i);
    }
    std::hash_map<Symbol, int>::const_iterator idIt = mSongIDLookup.find(s);
    if (idIt != mSongIDLookup.end() && i != idIt->second) {
        MILO_WARN(
            "SongID %d and SongID %d have duplicate short name %s!", i, idIt->second, s
        );
    }
#endif
    mSongNameLookup[i] = s;
    mSongIDLookup[s] = i;
}

bool BandSongMgr::SongCacheNeedsWrite() const {
    return SongMgr::SongCacheNeedsWrite() || mUpgradeMgr->SongCacheNeedsWrite()
        || mLicenseMgr->LicenseCacheNeedsWrite();
}

void BandSongMgr::ClearSongCacheNeedsWrite() {
    SongMgr::ClearSongCacheNeedsWrite();
    if (mUpgradeMgr)
        mUpgradeMgr->ClearSongCacheNeedsWrite();
}

void BandSongMgr::ReadCachedMetadataFromStream(BinStream &bs, int rev) {
    int count;
    bs >> count;
    for (int i = 0; (int)(int)(int)i < count; i++) {
        int i40;
        bs >> i40;
        bool remove;
        int maxCount;
        do {
            maxCount = mMaxSongCount;
            auto _tmp0 = GetCurSongCount();
            if (_tmp0 >= maxCount)
                break;
            remove = RemoveOldestCachedContent();
        } while (remove);
        maxCount = mMaxSongCount;
        if (GetCurSongCount() >= maxCount) {
            BandSongMetadata data(this);
            data.Load(bs);
        } else {
            BandSongMetadata *data = new BandSongMetadata(this);
            data->Load(bs);
            mCachedSongMetadata[i40] = data;
        }
    }
    if (rev >= 5) {
        BinStreamRev d(bs, rev);
        d >> unk114;
    }
    if (rev >= 11) {
        BinStreamRev d(bs, rev);
        d >> unk11c;
    }
    if (rev >= 6) {
        mUpgradeMgr->ReadCachedMetadataFromStream(bs, rev);
    }
    if (rev >= 8) {
        mLicenseMgr->ReadCachedMetadataFromStream(bs, rev);
    }
    unk124 = false;
}

void BandSongMgr::WriteCachedMetadataToStream(BinStream &bs) const {
    bs << mCachedSongMetadata.size();
    FOREACH (it, mCachedSongMetadata) {
        bs << it->first;
        it->second->Save(bs);
    }
    bs << unk114;
    bs << unk11c;
    mUpgradeMgr->WriteCachedMetadataToStream(bs);
    mLicenseMgr->WriteCachedMetadataToStream(bs);
}

bool BandSongMgr::RemoveOldestCachedContent() {
    if (mCachedSongMetadata.size() < 1)
        return false;

    std::hash_map<int, SongMetadata *>::iterator oldest = mCachedSongMetadata.begin();
    int maxAge = oldest->second->Age();
    for (std::hash_map<int, SongMetadata *>::iterator it = mCachedSongMetadata.begin();
         it != mCachedSongMetadata.end();
         ++it) {
        if (it->second->Age() > maxAge) {
            maxAge = it->second->Age();
            oldest = it;
        }
    }

    if (maxAge < 1)
        return false;

    int songID = oldest->second->ID();
    if (songID == 0) {
        MILO_WARN(
            "Invalid SongID for song %s\n",
            dynamic_cast<BandSongMetadata *>(oldest->second)->Title()
        );
        return false;
    }

    Symbol contentName;
    std::hash_map<int, Symbol>::iterator contentIt = mContentUsedForSong.find(songID);
    if (contentIt == mContentUsedForSong.end()) {
        for (std::hash_map<Symbol, std::vector<int> >::iterator mit = mSongIDsInContent.begin();
             mit != mSongIDsInContent.end();
             ++mit) {
            for (std::vector<int>::iterator vit = mit->second.begin();
                 vit != mit->second.end();
                 ++vit) {
                if (*vit == songID) {
                    contentName = mit->first;
                    break;
                }
            }
            if (!contentName.Null())
                break;
        }
    } else {
        contentName = contentIt->second;
    }

    if (contentName.Null()) {
        mContentUsedForSong.erase(songID);
        mCachedSongMetadata.erase(songID);
    } else {
        std::vector<int> songsToRemove = mSongIDsInContent[contentName];
        ClearFromCache(contentName);
        for (std::vector<int>::iterator vit = songsToRemove.begin();
             vit != songsToRemove.end();
             ++vit) {
            mContentUsedForSong.erase(*vit);
            mCachedSongMetadata.erase(*vit);
        }
    }

    return true;
}

void BandSongMgr::ClearCachedContent() {
    SongMgr::ClearCachedContent();
    mUpgradeMgr->ClearCachedContent();
    mLicenseMgr->ClearCachedContent();
    unk114.clear();
    unk11c.clear();
    unk124 = true;
}

void BandSongMgr::AddSongs(DataArray *a) {
    AddSongData(a, nullptr, kLocationRoot);
    ContentDone();
}

// Retail 360 keeps a genuine MRU list here (unk114, 0x130), capped at 20
// entries -- confirmed via Ghidra decompile of target fn_8255F488, the sole
// caller of which is AddSongData's tail block (see there). Both rb3-Wii's dev
// decomp and dc3 stub this out; retail's TU5-era body is real. push_back +
// walk-count (STLport list::size() is O(n)) + pop_front matches the
// decompiled insert-at-end/erase-at-begin/size-walk shape exactly.
void BandSongMgr::AddRecentSong(int songID) {
    unk114.push_back(songID);
    while (unk114.size() > 20) {
        unk114.pop_front();
    }
}

DECOMP_FORCEACTIVE(BandSongMgr, "mSongNameLookup.find(id) == mSongNameLookup.end()")

bool BandSongMgr::InqAvailableSongSources(std::set<Symbol> &sourceSet) {
    MILO_ASSERT(sourceSet.empty(), 0x5B8);
    std::vector<int> songs;
    GetRankedSongs(songs, false, false);
    FOREACH (it, songs) {
        BandSongMetadata *songData = (BandSongMetadata *)Data(*it);
        MILO_ASSERT(songData, 0x5C4);
        Symbol source = songData->SourceSym();
        sourceSet.insert(source);
    }
    return sourceSet.size();
}

int BandSongMgr::GetPartDifficulty(Symbol s1, Symbol s2) const {
    BandSongMetadata *songMetaData =
        (BandSongMetadata *)Data(GetSongIDFromShortName(s1, true));
    MILO_ASSERT(songMetaData, 0x5D6);
    float rank = songMetaData->Rank(s2);
    return RankTier(rank, s2);
}

int BandSongMgr::GetNumVocalParts(Symbol s) const {
    BandSongMetadata *songData =
        (BandSongMetadata *)Data(GetSongIDFromShortName(s, true));
    MILO_ASSERT(songData, 0x5E4);
    return songData->NumVocalParts();
}

int BandSongMgr::NumRankedSongs(TrackType ty, bool b2, Symbol s3) const {
    int num = 0;
    std::vector<int> songs;
    GetRankedSongs(songs, false, false);
    FOREACH (it, songs) {
        BandSongMetadata *data = (BandSongMetadata *)Data(*it);
        MILO_ASSERT(data, 0x5F3);
        // goto-into-else lets both filter branches share the `checkSym` tail; rewrite breaks match.
        if (b2) {
            if (!data->HasVocalHarmony())
                continue;
            goto checkSym;
        } else {
            if (ty != kNumTrackTypes) {
                Symbol trackSym = TrackTypeToSym(ty);
                if (!data->HasPart(trackSym, false))
                    continue;
            }
        checkSym:
            if (s3 == gNullStr || data->SourceSym() == s3) {
                num++;
            }
        }
    }
    return num;
}

void BandSongMgr::SyncSharedSongs() {
    if (!TheSessionMgr)
        return;
    std::set<int> availableSongs;
    for (std::set<int>::const_iterator it = mAvailableSongs.begin();
         it != mAvailableSongs.end();
         ++it) {
        int id = *it;
        if (!IsRestricted(id)) {
            availableSongs.insert(id);
        }
    }
    std::set<int> proGuitarBassSongs;
    for (std::set<int>::const_iterator it = availableSongs.begin();
         it != availableSongs.end();
         ++it) {
        int id = *it;
        BandSongMetadata *data = (BandSongMetadata *)Data(id);
        bool isProGuitarOrBass = false;
        bool isDownload = false;
        if (data) {
            if (data->IsDownload()) {
                isDownload = true;
            }
        }
        if (isDownload) {
            bool hasProGuitarOrBass = true;
            if (!data->HasPart(real_guitar, true)) {
                if (!data->HasPart(real_bass, true)) {
                    hasProGuitarOrBass = false;
                }
            }
            if (hasProGuitarOrBass) {
                isProGuitarOrBass = true;
            }
        }
        if (isProGuitarOrBass) {
            proGuitarBassSongs.insert(id);
        }
    }
    LocalBandMachine *machine = TheSessionMgr->mMachineMgr->GetLocalMachine();
    MILO_ASSERT(machine, 0x631);
    machine->SetProGuitarOrBassSongs(proGuitarBassSongs);
    machine->SetAvailableSongs(availableSongs);
}

bool BandSongMgr::GetFakeSongsAllowed() { return sFakeSongsAllowed; }
void BandSongMgr::SetFakeSongsAllowed(bool b) { sFakeSongsAllowed = b; }

void BandSongMgr::AllowCacheWrite(bool b) {
    bool old = mSongCacheWriteAllowed;
    mSongCacheWriteAllowed = b;
    if (!old && b && SongCacheNeedsWrite() && TheSaveLoadMgr) {
        TheSaveLoadMgr->AutoSave();
    }
}

void BandSongMgr::CheatToggleMaxSongCount() {
    int max;
    DataArray *cfg = SystemConfig(song_mgr);
    max = cfg->FindInt(max_song_count);
    int max_debug = cfg->FindInt(max_song_count_debug);
    if (mMaxSongCount == max)
        max = max_debug;
    mMaxSongCount = max;
}

#pragma push
#pragma dont_inline on
BEGIN_HANDLERS(BandSongMgr)
    HANDLE_EXPR(has_song, HasSong(_msg->Sym(2), true))
    HANDLE_EXPR(song_path, SongPath(_msg->Sym(2)))
    HANDLE_EXPR(song_file_path, SongFilePath(_msg->Sym(2), _msg->Str(3)))
#ifdef HX_NATIVE // X360-match: retail's dispatch chain has no `song_name` arm
    HANDLE_EXPR(song_name, SongName(_msg->Sym(2)))
#endif
    HANDLE_EXPR(
        upgrade_midi_file, UpgradeMidiFile(GetSongIDFromShortName(_msg->Sym(2), true))
    )
    HANDLE_EXPR(get_meta_data, Data(GetSongIDFromShortName(_msg->Sym(2), true)))
    HANDLE_EXPR(rank_tier, RankTier(_msg->Float(2), _msg->Sym(3)))
    HANDLE_EXPR(
        rank_tier_for_song,
        RankTier(
            ((BandSongMetadata *)Data(GetSongIDFromShortName(_msg->Sym(2), true)))
                ->Rank(_msg->Sym(3)),
            _msg->Sym(3)
        )
    )
    HANDLE_EXPR(num_rank_tiers, NumRankTiers(_msg->Sym(2)))
    HANDLE_EXPR(rank_tier_token, RankTierToken(_msg->Int(2)))
    HANDLE_EXPR(num_vocal_parts, GetNumVocalParts(_msg->Sym(2)))
    HANDLE_ACTION(
        // Retail calls ?Play@Jukebox@@QAAXH@Z here, NOT AddRecentSong -- both are
        // `void f(int)` so the callee name is a relocation argument and therefore
        // SCORE-INVISIBLE; what exposed it was the `this` pointer, retail passing
        // `this + 0x148` where we passed `this`. AddRecentSong is still real and
        // still 100%; it is just called from AddSongs, not from here.
        add_recent_song, mJukebox.Play(GetSongIDFromShortName(_msg->Sym(2), true))
    )
    HANDLE_EXPR(
        part_plays_in_song,
        ((BandSongMetadata *)Data(GetSongIDFromShortName(_msg->Sym(2), true)))
            ->HasPart(_msg->Sym(3), false)
    )
    HANDLE_EXPR(
        mute_win_cues,
        ((BandSongMetadata *)Data(GetSongIDFromShortName(_msg->Sym(2), true)))
            ->MuteWinCues()
    )
    HANDLE_EXPR(midi_file, MidiFile(_msg->Sym(2)))
    HANDLE_EXPR(is_demo, IsDemo(_msg->Int(2)))
    HANDLE_EXPR(
        has_upgrade, GetUpgradeData(GetSongIDFromShortName(_msg->Sym(2), true)) != 0
    )
    HANDLE_EXPR(has_license, HasLicense(_msg->Sym(2)))
#ifdef HX_NATIVE // X360-match: retail stripped these two dev handlers
    HANDLE_EXPR(get_fake_songs_allowed, GetFakeSongsAllowed())
    HANDLE_ACTION(set_fake_songs_allowed, SetFakeSongsAllowed(_msg->Int(2)))
#endif
    HANDLE_ACTION(sync_shared_songs, SyncSharedSongs())
    HANDLE_EXPR(get_max_song_count, GetMaxSongCount())
#ifdef HX_NATIVE // X360-match: retail stripped this cheat handler
    HANDLE_ACTION(cheat_toggle_max_song_count, CheatToggleMaxSongCount())
#endif
    HANDLE_SUPERCLASS(SongMgr)
    HANDLE_CHECK(0x6DF)
END_HANDLERS
#pragma pop

// sw2 scatter-include (default/band3/meta_band/BandSongMgr <- math/Geo.cpp)
// X360-match only: retail folded Geo's COMDATs into this TU. Native compiles
// math/Geo.cpp as its own TU, so including it here would double-define.
#if !HX_NATIVE
#define gRev gRev_Geo
#define gAltRev gAltRev_Geo
#include "math/Geo.cpp"
#undef gRev
#undef gAltRev
#endif
