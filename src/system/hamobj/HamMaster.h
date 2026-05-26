#pragma once
#include "hamobj/HamAudio.h"
#include "beatmatch/HxMaster.h"
#include "hamobj/HamSongData.h"
#include "math/Vec.h"
#include "midi/Midi.h"
#include "midi/MidiParserMgr.h"
#include "obj/Object.h"
#include "utl/Loader.h"
#include "utl/SongInfoCopy.h"
#include "utl/SongPos.h"

class HamMaster;

class HamMasterLoader : public Loader {
public:
    HamMasterLoader(HamMaster *);
    virtual ~HamMasterLoader() {}
    virtual const char *DebugText() { return "HamMasterLoader"; }
    virtual bool IsLoaded() const { return false; }

protected:
    virtual void PollLoading();

    HamMaster *mMaster; // 0x1c
};

class HamMaster : public Hmx::Object, public HxMaster {
public:
    HamMaster(HamSongData *, MidiParserMgr *);
    // Hmx::Object
    virtual ~HamMaster();
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    // HxMaster
    virtual void Poll(float);
    virtual void Jump(float);
    virtual void Reset();
    virtual HxAudio *GetHxAudio() { return mAudio ? mAudio : nullptr; }
    virtual float SongDurationMs();
    virtual bool IsLoaded() { return mLoaded; }

    void
    Load(SongInfo *, bool, int, bool, HamSongDataValidate, std::vector<MidiReceiver *> *);
    void LoadOnlySongData(SongInfo *, bool, HamSongDataValidate);
    void ResetAudio();
    float StreamMs() const;
    bool DetectStreamJump(float &, float &, float &) const;
    float EventBeat(Symbol);
    void AddMusicFader(Fader *);
    void SetMaps();
    void LoaderPoll();
    int GetMeasure() const { return mSongPos.GetMeasure(); }
    float TotalBeat1() const { return mSongPos.GetTotalBeat(); }
    float TotalBeat2() const { return mPrevSongPos.GetTotalBeat(); }
    HamAudio *GetAudio() const { return mAudio; }
    HamSongData *SongData() const { return mSongData; }
    MidiParserMgr *GetMidiParserMgr() const { return mMidiParserMgr; }

private:
    void CheckBeat();
    void CheckLevels();

    HamSongData *mSongData; // 0x30
    HamAudio *mAudio; // 0x34
    MidiParserMgr *mMidiParserMgr; // 0x38
    SongInfo *mSongInfo; // 0x3c
    HamMasterLoader *mLoader; // 0x40
    bool mSyncLoad; // 0x44
    bool mLoaded; // 0x45
    float mSongMs; // 0x48
    float mStreamMs; // 0x4c
    bool mStreamJumped; // 0x50
    float mPreJumpMs; // 0x54
    float mPostJumpMs; // 0x58
    float mStreamMsAtJump; // 0x5c
    SongPos mSongPos; // 0x60
    SongPos mPrevSongPos; // 0x78
    std::vector<int> mSubmixIdxs; // 0x90
    float unk9c; // 0x9c
    float unka0; // 0xa0
    float unka4; // 0xa4
    std::list<Vector2> mLevelHistory; // 0xa8
    int unkb0; // 0xb0
    int mLastBeatIndex; // 0xb4
    int mBeatCount; // 0xb8
    bool mMetronome; // 0xbc
};

extern HamMaster *TheMaster;
