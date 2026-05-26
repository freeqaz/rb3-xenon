#pragma once
#include "utl/MemMgr.h"
#include "utl/SongInfoAudioType.h"
#include "utl/BinStream.h"
#include <vector>

struct TrackChannels {
    SongInfoAudioType mAudioType; // 0x0
    std::vector<int> mChannels; // 0x4
};

BinStream &operator<<(BinStream &, const TrackChannels &);
BinStream &operator>>(BinStream &, TrackChannels &);

class SongInfo {
public:
    SongInfo() {}
    virtual ~SongInfo() {}
    virtual Symbol GetName() const = 0;
    virtual const char *GetBaseFileName() const = 0;
    virtual const char *GetPackageName() const = 0;
    virtual const std::vector<TrackChannels> &GetTracks() const = 0;
    virtual bool IsPlayTrackChannel(int) const = 0;
    virtual const TrackChannels *FindTrackChannel(SongInfoAudioType) const = 0;
    virtual int NumChannelsOfTrack(SongInfoAudioType) const = 0;
    virtual int TrackIndex(SongInfoAudioType) const = 0;
    virtual const std::vector<float> &GetPans() const = 0;
    virtual const std::vector<float> &GetVols() const = 0;
    virtual const std::vector<int> &GetCores() const = 0;
    virtual int NumExtraMidiFiles() const = 0;
    virtual const char *GetExtraMidiFile(int) const = 0;

    MEM_OVERLOAD(SongInfo, 0x1C);
};

class SongInfoCopy : public SongInfo {
public:
    SongInfoCopy(const SongInfo *);
    virtual ~SongInfoCopy();
    virtual Symbol GetName() const;
    virtual const char *GetBaseFileName() const; //{ return mBaseFileName.c_str(); }
    virtual const char *GetPackageName() const;
    virtual const std::vector<TrackChannels> &GetTracks() const;
    virtual bool IsPlayTrackChannel(int) const;
    virtual const TrackChannels *FindTrackChannel(SongInfoAudioType) const;
    virtual int NumChannelsOfTrack(SongInfoAudioType) const;
    virtual int TrackIndex(SongInfoAudioType) const;
    virtual const std::vector<float> &GetPans() const;
    virtual const std::vector<float> &GetVols() const;
    virtual const std::vector<int> &GetCores() const;
    virtual int NumExtraMidiFiles() const;
    virtual const char *GetExtraMidiFile(int) const;

protected:
    SongInfoCopy();

    Symbol mName; // 0x4
    String mBaseFileName; // 0x8
    String mPackageName; // 0x10
    std::vector<float> mPans; // 0x18
    std::vector<float> mVols; // 0x24
    std::vector<int> mCores; // 0x30
    std::vector<TrackChannels> mTrackChannels; // 0x3c
    std::vector<String> mExtraMidiFiles; // 0x48
};
