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
    mPans = info->GetPans();
    mVols = info->GetVols();
    mCores = info->GetCores();
    mTrackChannels = info->GetTracks();
    int num_midis = info->NumExtraMidiFiles();
    mExtraMidiFiles.reserve(num_midis);
    for (int i = 0; i < num_midis; i++) {
        mExtraMidiFiles.push_back(info->GetExtraMidiFile(i));
    }
}
