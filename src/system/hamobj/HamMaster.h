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

    HamSongData *mSongData; // 0x2c
    HamAudio *mAudio; // 0x30
    MidiParserMgr *mMidiParserMgr; // 0x34
    SongInfo *mSongInfo; // 0x38
    HamMasterLoader *mLoader; // 0x3c
    bool mSyncLoad; // 0x40
    bool mLoaded; // 0x41
    float mSongMs; // 0x44
    float mStreamMs; // 0x48
    bool mStreamJumped; // 0x4c
    float mPreJumpMs; // 0x50
    float mPostJumpMs; // 0x54
    float mStreamMsAtJump; // 0x58
    SongPos mSongPos; // 0x5c
    SongPos mPrevSongPos; // 0x70
    std::vector<int> mSubmixIdxs; // 0x84
    float unk9c; // 0x90
    float unka0; // 0x94
    float unka4; // 0x98
    std::list<Vector2> mLevelHistory; // 0x9c
    int unkb0; // 0xa4
    int mLastBeatIndex; // 0xa8
    int mBeatCount; // 0xac
    bool mMetronome; // 0xb0
};

extern HamMaster *TheMaster;
