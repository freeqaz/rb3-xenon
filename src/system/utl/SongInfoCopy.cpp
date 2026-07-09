#include "utl/SongInfoCopy.h"
#include "utl/Symbol.h"
#include <algorithm>

Symbol SongInfoCopy::GetName() const { return mName; }

const char *SongInfoCopy::GetBaseFileName() const { return mBaseFileName.c_str(); }

const std::vector<float> &SongInfoCopy::GetPans() const { return mPans; }

const std::vector<float> &SongInfoCopy::GetVols() const { return mVols; }

const std::vector<int> &SongInfoCopy::GetCores() const { return mCores; }

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

SongInfoCopy::SongInfoCopy() { mName = gNullStr; }

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

#ifdef HX_NATIVE
// Trivial member accessors. In the retail X360 object these are emitted from a
// different TU / inlined into the vtable emitter, so their out-of-line bodies
// are absent from SongInfoCopy.obj — but the native rb3-dta build references
// them through the vtable and needs real definitions. Bodies are the verbatim
// rb3-Wii oracle (src/system/utl/SongInfoCopy.cpp), which returns the members.
// Guarded so retail bytes are byte-identical (HX_NATIVE is native-only).
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
#endif
