#pragma once
#include "utl/Symbol.h"
#include "obj/Object.h"
#include "obj/Data.h"
#include "meta/DataArraySongInfo.h"

class SongMetadata : public Hmx::Object {
public:
    virtual ~SongMetadata();
    virtual DataNode Handle(DataArray *, bool);
    virtual void Save(BinStream &);
    virtual void Load(BinStream &);
    virtual bool IsVersionOK() const = 0;

    int ID() const;
    bool IsOnDisc() const;
    Symbol GameOrigin() const;
    void PreviewTimes(float &, float &) const;
    DataArraySongInfo *SongBlock() const;
    int NumVocalParts() const;
    Symbol ShortName() const { return mShortName; }
    int Age() const { return mAge; }
    void IncrementAge() { mAge++; }
    void ResetAge() { mAge = 0; }
    short Version() const { return mVersion; }

private:
    static int sSaveVer;
    void InitSongMetadata();

protected:
    SongMetadata();
    SongMetadata(DataArray *main_arr, DataArray *backup_arr, bool onDisc);

    short mVersion; // 0x2c
    int mID; // 0x30
    Symbol mShortName; // 0x34
    bool mIsOnDisc; // 0x38
    Symbol mGameOrigin; // 0x3c
    float mPreviewStartTime; // 0x40
    float mPreviewEndTime; // 0x44
    DataArraySongInfo *mSongInfo; // 0x48
    int mAge; // 0x4c - used for song cache?
};
