#include "meta/DataArraySongInfo.h"

int DataArraySongInfo::sSaveVer = 1;

DataArraySongInfo::DataArraySongInfo(SongInfo *info) : SongInfoCopy(info) {}

DataArraySongInfo::DataArraySongInfo() {}

DataArraySongInfo::~DataArraySongInfo() {}

void DataArraySongInfo::SetBaseFileName(const char *name) { mBaseFileName = name; }

BinStream &operator<<(BinStream &bs, const DataArraySongInfo &dinfo) {
    dinfo.Save(bs);
    return bs;
}

BinStream &operator>>(BinStream &bs, DataArraySongInfo &dinfo) {
    dinfo.Load(bs);
    return bs;
}

BinStream &operator<<(BinStream &bs, const TrackChannels &chans) {
    bs << chans.mAudioType;
    bs << chans.mChannels;
    return bs;
}

void DataArraySongInfo::Save(BinStream &bs) const {
    bs << sSaveVer;
    bs << mName;
    bs << mBaseFileName;
    bs << mPackageName;
    bs << mPans;
    bs << mVols;
    bs << mCores;
    bs << mTrackChannels;
    bs << mExtraMidiFiles;
}

BinStream &operator>>(BinStream &bs, TrackChannels &chans) {
    int ty;
    bs >> ty;
    chans.mAudioType = (SongInfoAudioType)ty;
    bs >> chans.mChannels;
    return bs;
}

void DataArraySongInfo::AddExtraMidiFile(const char *cc1, const char *cc2) {
    mExtraMidiFiles.push_back(cc1);
}

DataArraySongInfo::DataArraySongInfo(
    DataArray *main_arr, DataArray *backup_arr, Symbol name_sym
) {
    mName = name_sym;
    DataArray *member_arr;

    static Symbol name("name");
    MILO_ASSERT(FIND_WITH_BACKUP(name), 0x1C);
    mBaseFileName = member_arr->Str(1);

    static Symbol pkg_name("pkg_name");
    if (FIND_WITH_BACKUP(pkg_name)) {
        mPackageName = member_arr->Str(1);
    }
    static Symbol pans("pans");
    if (FIND_WITH_BACKUP(pans)) {
        DataArray *pan_arr = member_arr->Array(1);
        mPans.reserve(pan_arr->Size());
        for (int i = 0; i < pan_arr->Size(); i++) {
            mPans.push_back(pan_arr->Float(i));
        }
    }
    static Symbol vols("vols");
    if (FIND_WITH_BACKUP(vols)) {
        DataArray *vol_arr = member_arr->Array(1);
        mVols.reserve(vol_arr->Size());
        for (int i = 0; i < vol_arr->Size(); i++) {
            mVols.push_back(vol_arr->Float(i));
        }
    }
    static Symbol cores("cores");
    if (FIND_WITH_BACKUP(cores)) {
        DataArray *core_arr = member_arr->Array(1);
        mCores.reserve(core_arr->Size());
        for (int i = 0; i < core_arr->Size(); i++) {
            mCores.push_back(core_arr->Int(i));
        }
    }
    static Symbol tracks("tracks");
    if (FIND_WITH_BACKUP(tracks)) {
        DataArray *track_arr = member_arr->Array(1);
        mTrackChannels.reserve(track_arr->Size());
        for (int i = 0; i < track_arr->Size(); i++) {
            DataArray *chan_arr = track_arr->Array(i);
            mTrackChannels.push_back(TrackChannels());
            mTrackChannels.back().mAudioType = SymbolToAudioType(chan_arr->Sym(0));
            DataNode &chan_node = chan_arr->Node(1);
            if (chan_node.Type() == kDataArray) {
                DataArray *chan_nums = chan_node.Array();
                for (int j = 0; j < chan_nums->Size(); j++) {
                    mTrackChannels.back().mChannels.push_back(chan_nums->Int(j));
                }
            } else {
                mTrackChannels.back().mChannels.push_back(chan_node.Int());
            }
        }
    }
}

void DataArraySongInfo::Load(BinStream &bs) {
    int rev;
    bs >> rev;
    bs >> mName;
    bs >> mBaseFileName;
    bs >> mPackageName;
    if (rev < 2) {
        int x, y, z;
        bs >> x;
        bs >> y;
        bs >> z;
    }
    bs >> mPans;
    bs >> mVols;
    bs >> mCores;
    if (rev < 2) {
        std::vector<int> ivec;
        bs >> ivec;
        std::vector<Symbol> s1, s2;
        bs >> s1;
        bs >> s2;
    }
    bs >> mTrackChannels;
    bs >> mExtraMidiFiles;
}
