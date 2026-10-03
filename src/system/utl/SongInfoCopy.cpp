#include "utl/SongInfoCopy.h"
#include "utl/Symbol.h"
#include "os/System.h"
#include <algorithm>

Symbol SongInfoCopy::GetName() const { return mName; }

const char *SongInfoCopy::GetBaseFileName() const { return mBaseFileName.c_str(); }

const std::vector<float> &SongInfoCopy::GetPans() const { return mPans; }

const std::vector<float> &SongInfoCopy::GetVols() const { return mVols; }

const std::vector<int> &SongInfoCopy::GetCores() const { return mCores; }

// Retail defines all seven in this TU (W16-PB). 0x827D10F8/1100/1108 (the three
// vector getters) and 0x827D1110/1118 (the two float getters, pinned to
// PanelDir as fold survivors) sit between GetVols and GetPackageName; the two
// int getters fold onto earlier identical bodies (0x8252E038, 0x8235AFA0).
// They were HX_NATIVE-only, which left five SongInfoCopy and
// DataArraySongInfo vtable slots pointing at symbols no object defined.
int SongInfoCopy::GetNumVocalParts() const { return mNumVocalParts; }

int SongInfoCopy::GetHopoThreshold() const { return mHopoThreshold; }

const std::vector<int> &SongInfoCopy::GetCrowdChannels() const { return mCrowdChannels; }

const std::vector<Symbol> &SongInfoCopy::GetDrumSoloSamples() const {
    return mDrumSoloSamples;
}

const std::vector<Symbol> &SongInfoCopy::GetDrumFreestyleSamples() const {
    return mDrumFreestyleSamples;
}

float SongInfoCopy::GetMuteVolume() const { return mMuteVolume; }

float SongInfoCopy::GetVocalMuteVolume() const { return mVocalMuteVolume; }

// GetTracks is in CharBoneDir.cpp (cross-unit)

const char *SongInfoCopy::GetPackageName() const {
    if (!mPackageName.empty())
        return mPackageName.c_str();
    else
        return 0;
}

int SongInfoCopy::NumChannelsOfTrack(SongInfoAudioType ty) const {
    const TrackChannels *tc = FindTrackChannel(ty);
    if (tc)
        return tc->mChannels.size();
    else
        return 0;
}

// TU5/retail-only virtual (vtable slot 0x4c, retail 0x827D1190); its real name is
// not known. MasterAudio passes the result to Synth::NewStream.
// Retail reads the name through a writable global pointer (.data 0x82C78F24),
// not a literal; the variable's name is descriptive.
static const char *gUGCAuditionTempSongName = "ugc_audition_temp_song";

// Retail: true for every song except the UGC audition temp song.
bool SongInfoCopy::UnkTU5Virtual_0x4c() const {
    static Symbol ugc_audition_temp_song(gUGCAuditionTempSongName);
    return GetName() != ugc_audition_temp_song;
}

int SongInfoCopy::NumExtraMidiFiles() const { return mExtraMidiFiles.size(); }

bool SongInfoCopy::IsPlayTrackChannel(int chan) const {
    for (int i = 0; i < mTrackChannels.size(); i++) {
        if (std::find(
                mTrackChannels[i].mChannels.begin(),
                mTrackChannels[i].mChannels.end(),
                chan
            )
            != mTrackChannels[i].mChannels.end()) {
            return true;
        }
    }
    return false;
}

const TrackChannels *SongInfoCopy::FindTrackChannel(SongInfoAudioType ty) const {
    for (int i = 0; i < mTrackChannels.size(); i++) {
        if (mTrackChannels[i].mAudioType == ty) {
            return &mTrackChannels[i];
        }
    }
    return 0;
}

int SongInfoCopy::TrackIndex(SongInfoAudioType ty) const {
    for (int i = 0; i < mTrackChannels.size(); i++) {
        if (mTrackChannels[i].mAudioType == ty)
            return i;
    }
    return -1;
}

const char *SongInfoCopy::GetExtraMidiFile(int idx) const {
    return mExtraMidiFiles[idx].c_str();
}

// Retail 0x827D1628 (648 B) stores
// mNumVocalParts=1, zeroes the threshold/volumes and reads beatmatcher config.
// Ours only set mName -- a default-constructed copy carried an uninitialized
// hopo threshold and mute volumes.
SongInfoCopy::SongInfoCopy() : mName(), mBaseFileName(), mPackageName() {
    mName = gNullStr;
    mNumVocalParts = 1;
    mHopoThreshold = 0;
    mMuteVolume = 0.0f;
    mVocalMuteVolume = 0.0f;
    DataArray *cfg = SystemConfig()->FindArray("beatmatcher", false);
    if (cfg) {
        mHopoThreshold = cfg->FindArray("parser")->FindInt("hopo_threshold");
        mMuteVolume = cfg->FindArray("audio")->FindFloat("mute_volume");
        mVocalMuteVolume = cfg->FindArray("audio")->FindFloat("mute_volume_vocals");
    }
}

SongInfoCopy::~SongInfoCopy() {}

SongInfoCopy::SongInfoCopy(const SongInfo *info) {
    mName = info->GetName();
    mBaseFileName = info->GetBaseFileName();
    mPackageName = info->GetPackageName();
    mNumVocalParts = info->GetNumVocalParts();
    mHopoThreshold = info->GetHopoThreshold();
    mMuteVolume = info->GetMuteVolume();
    mVocalMuteVolume = info->GetVocalMuteVolume();
    mPans = info->GetPans();
    mVols = info->GetVols();
    mCores = info->GetCores();
    mCrowdChannels = info->GetCrowdChannels();
    mDrumSoloSamples = info->GetDrumSoloSamples();
    mDrumFreestyleSamples = info->GetDrumFreestyleSamples();
    mTrackChannels = info->GetTracks();
    int num_midis = info->NumExtraMidiFiles();
    mExtraMidiFiles.reserve(num_midis);
    for (int i = 0; i < num_midis; i++) {
        mExtraMidiFiles.push_back(info->GetExtraMidiFile(i));
    }
}

