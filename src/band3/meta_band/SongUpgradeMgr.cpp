#include "meta_band/SongUpgradeMgr.h"
#include "meta_band/BandSongMgr.h"
#include "obj/Data.h"
#include "obj/DataFile.h"
#include "os/ContentMgr.h"
#include "os/Debug.h"
#include "os/File.h"
#include "utl/Symbol.h"

int SongUpgradeData::sSaveVer = 1;

// TU-local hash_map serializers (mirrors SongMgr.cpp): operator<< walks the
// hashtable slist in bucket order, operator>> reads size then inserts via
// operator[]. Kept TU-local so BinStream.h need not pull in <hash_map>.
template <class T1, class T2>
BinStream &operator<<(BinStream &bs, const std::hash_map<T1, T2> &map) {
    bs << map.size();
    for (typename std::hash_map<T1, T2>::const_iterator it = map.begin();
         it != map.end();
         ++it) {
        bs << it->first << it->second;
    }
    return bs;
}

template <class T1, class T2>
BinStream &operator>>(BinStream &bs, std::hash_map<T1, T2> &map) {
    unsigned int size;
    bs >> size;
    for (; size != 0; size--) {
        T1 key;
        bs >> key;
        bs >> map[key];
    }
    return bs;
}

void SongUpgradeData::InitSongUpgradeData() {
    mUpgradeVersion = -1;
    mRealGuitarRank = 0;
    mRealBassRank = 0;
    for (int i = 0; i < 6; i++) {
        mRealGuitarTuning[i] = 0;
    }
    for (int i = 0; i < 4; i++) {
        mRealBassTuning[i] = 0;
    }
}

SongUpgradeData::SongUpgradeData() { InitSongUpgradeData(); }

SongUpgradeData::SongUpgradeData(DataArray *da) {
    InitSongUpgradeData();
    static Symbol upgrade_version("upgrade_version");
    DataArray *upgradearr = da->FindArray(upgrade_version, false);
    if (upgradearr)
        mUpgradeVersion = upgradearr->Int(1);
    static Symbol midi_file("midi_file");
    DataArray *midifilearr = da->FindArray(midi_file, false);
    if (midifilearr)
        mMidiFile = midifilearr->Str(1);
    static Symbol rank("rank");
    static Symbol real_guitar("real_guitar");
    static Symbol real_bass("real_bass");
    DataArray *rankarr = da->FindArray(rank, false);
    if (rankarr) {
        DataArray *garr = rankarr->FindArray(real_guitar, false);
        if (garr)
            mRealGuitarRank = garr->Float(1);
        DataArray *barr = rankarr->FindArray(real_bass, false);
        if (barr)
            mRealBassRank = barr->Float(1);
    }
    static Symbol real_guitar_tuning("real_guitar_tuning");
    DataArray *rgtuningarr = da->FindArray(real_guitar_tuning, false);
    if (rgtuningarr) {
        for (int i = 0; i < 6; i++) {
            mRealGuitarTuning[i] = rgtuningarr->Array(1)->Int(i);
        }
    }
    static Symbol real_bass_tuning("real_bass_tuning");
    DataArray *rbtuningarr = da->FindArray(real_bass_tuning, false);
    if (rbtuningarr) {
        for (int i = 0; i < 4; i++) {
            mRealBassTuning[i] = rbtuningarr->Array(1)->Int(i);
        }
    }
}

void SongUpgradeData::Save(BinStream &bs) {
    bs << sSaveVer;
    bs << mUpgradeVersion;
    bs << mMidiFile;
    bs << mRealGuitarRank;
    bs << mRealBassRank;
    for (int i = 0; i < 6; i++) {
        bs << mRealGuitarTuning[i];
    }
    for (int i = 0; i < 4; i++) {
        bs << mRealBassTuning[i];
    }
}

void SongUpgradeData::Load(BinStream &bs) {
    int rev;
    bs >> rev;
    if (rev >= 1)
        bs >> mUpgradeVersion;
    else
        mUpgradeVersion = 0;
    bs >> mMidiFile;
    bs >> mRealGuitarRank;
    bs >> mRealBassRank;
    for (int i = 0; i < 6; i++) {
        bs >> mRealGuitarTuning[i];
    }
    for (int i = 0; i < 4; i++) {
        bs >> mRealBassTuning[i];
    }
}

const char *SongUpgradeData::MidiFile() const { return mMidiFile.c_str(); }

bool SongUpgradeData::HasPart(Symbol) const {
    return mRealGuitarRank != 0 || mRealBassRank != 0;
}

float SongUpgradeData::Rank(Symbol inst) const {
    static Symbol real_guitar("real_guitar");
    static Symbol real_bass("real_bass");
    if (inst == real_guitar)
        return mRealGuitarRank;
    else if (inst == real_bass)
        return mRealBassRank;
    else {
        MILO_FAIL(
            "Asking a song upgrade for a part rank that's not real_guitar or real_bass!"
        );
        return 0;
    }
}

int SongUpgradeData::RealGuitarTuning(int string) const {
    return mRealGuitarTuning[string];
}

int SongUpgradeData::RealBassTuning(int string) const { return mRealBassTuning[string]; }

bool SongUpgradeData::CorrectMidiFile(ContentLocT loc, Symbol s) {
    return FileExists(mMidiFile.c_str(), 0, nullptr);
}

SongUpgradeMgr::SongUpgradeMgr() : mSongCacheNeedsWrite(0) {
    TheContentMgr.RegisterCallback(this, false);
}

bool SongUpgradeMgr::HasUpgrade(int id) const {
    return mAvailableUpgrades.find(id) != mAvailableUpgrades.end();
}

SongUpgradeData *SongUpgradeMgr::UpgradeData(int id) const {
    if (mAvailableUpgrades.find(id) == mAvailableUpgrades.end())
        return nullptr;
    else {
        std::hash_map<int, SongUpgradeData *>::const_iterator it = mUpgradeData.find(id);
        if (it != mUpgradeData.end())
            return it->second;
        else {
            MILO_ASSERT(false, 0xF9);
            return nullptr;
        }
    }
}

void SongUpgradeMgr::ContentStarted() {
    mAvailableUpgrades.clear();
    unk4c.clear();
}

bool SongUpgradeMgr::ContentDiscovered(Symbol s) {
    if (unk34.find(s) != unk34.end()) {
        std::vector<int> songs;
        GetUpgradeSongsInContent(s, songs);
        for (std::vector<int>::iterator sit = songs.begin(); sit != songs.end(); ++sit) {
            int cur = *sit;
            if (mAvailableUpgrades.find(cur) == mAvailableUpgrades.end()) {
                MarkAvailable(cur, s);
            }
        }
        return true;
    } else {
        return false;
    }
}

const char *SongUpgradeMgr::ContentPattern() { return "&upgrades.dta"; }

const char *SongUpgradeMgr::ContentDir() { return "songs_upgrades"; }

void SongUpgradeMgr::ContentMounted(const char *c1, const char *c2) {
    if (unk34.find(c1) == unk34.end()) {
        std::vector<int> vec;
        unk34[c1] = vec;
    }
}

void SongUpgradeMgr::ContentLoaded(Loader *loader, ContentLocT loct, Symbol s) {
    DataLoader *d = dynamic_cast<DataLoader *>(loader);
    MILO_ASSERT(d, 0x144);
    DataArray *data = d->Data();
    if (data) {
        AddUpgradeData(data, d, loct, s);
    } else {
        ClearFromCache(s);
    }
}

const char *SongUpgradeMgr::ContentName(int i) const {
    return unk4c.find(i)->second.Str();
}

bool SongUpgradeMgr::SongCacheNeedsWrite() const { return mSongCacheNeedsWrite; }
void SongUpgradeMgr::ClearSongCacheNeedsWrite() { mSongCacheNeedsWrite = false; }

bool SongUpgradeMgr::WriteCachedMetadataToStream(BinStream &bs) const {
    bs << unk34;
    bs << mUpgradeData.size();
    for (std::hash_map<int, SongUpgradeData *>::const_iterator it = mUpgradeData.begin();
         it != mUpgradeData.end();
         ++it) {
        bs << it->first;
        it->second->Save(bs);
    }
    return true;
}

bool SongUpgradeMgr::ReadCachedMetadataFromStream(BinStream &bs, int rev) {
    ClearCachedContent();
    bs >> unk34;
    int size;
    bs >> size;
    for (int i = 0; i < size; i++) {
        int key;
        bs >> key;
        SongUpgradeData *udata = new SongUpgradeData();
        udata->Load(bs);
        mUpgradeData[key] = udata;
    }
    return true;
}

void SongUpgradeMgr::ClearCachedContent() {
    unk34.clear();
    for (std::hash_map<int, SongUpgradeData *>::iterator it = mUpgradeData.begin();
         it != mUpgradeData.end();
         ++it) {
        delete it->second;
    }
    mUpgradeData.clear();
}

void SongUpgradeMgr::ClearFromCache(Symbol s) {
    std::hash_map<Symbol, std::vector<int> >::iterator it = unk34.find(s);
    MILO_ASSERT_FMT(it != unk34.end(), "Content %s isn't cached!", s);
    unk34.erase(it);
}

void SongUpgradeMgr::GetUpgradeSongsInContent(Symbol key, std::vector<int> &upgradeSongs)
    const {
    std::hash_map<Symbol, std::vector<int> >::const_iterator it = unk34.find(key);
    if (it != unk34.end())
        upgradeSongs = it->second;
}

void SongUpgradeMgr::AddUpgradeData(
    DataArray *arr, DataLoader *loader, ContentLocT loc, Symbol s
) {
    if (!streq(s.Str(), ".")) {
        std::vector<int> upgradesongs;
        GetUpgradeSongsInContent(s, upgradesongs);
        if (!upgradesongs.empty())
            return;
    }
    static Symbol song_id("song_id");
    for (int i = 0; i < arr->Size(); i++) {
        DataArray *curArr = arr->Array(i);
        int songID = curArr->FindInt(song_id);
        if (mAvailableUpgrades.find(songID) != mAvailableUpgrades.end()) {
            MILO_LOG("The upgrade for %s was found twice.\n", curArr->Sym(0));
        } else {
            if (mUpgradeData.find(songID) != mUpgradeData.end()) {
                delete mUpgradeData.find(songID)->second;

                SongUpgradeData *udata = new SongUpgradeData(curArr);
                if (udata->CorrectMidiFile(loc, s)) {
                    mUpgradeData[songID] = udata;
                    MarkAvailable(songID, s);
                } else
                    delete udata;
            }
        }
    }
    if (!streq(s.Str(), ".")) {
        std::vector<int> songs;
        for (int i = 0; i < arr->Size(); i++) {
            int songID = arr->Array(i)->FindInt(song_id);
            MILO_ASSERT(songID != kSongID_Invalid, 0x212);
            songs.push_back(songID);
        }
        unk34[s] = songs;
        mSongCacheNeedsWrite = true;
    }
}

void SongUpgradeMgr::MarkAvailable(int i, Symbol s) {
    std::hash_map<int, SongUpgradeData *>::iterator it = mUpgradeData.find(i);
    MILO_ASSERT(it != mUpgradeData.end(), 0x220);
    bool canInsert = it->second->mUpgradeVersion >= 0 && it->second->mUpgradeVersion <= 1;
    if (canInsert) {
        mAvailableUpgrades.insert(i);
        unk4c[i] = s;
    }
}
