#pragma once
#include "beatmatch/BeatMasterSink.h"
#include "beatmatch/SongParserSink.h"
#include "beatmatch/HxMaster.h"
#include "utl/SongPos.h"
#include "beatmatch/SongData.h"
#include "beatmatch/MasterAudio.h"
#include "utl/Loader.h"

class MasterAudio;
class MidiParserMgr;
class SongInfo;
class PlayerTrackConfigList;
class BeatMasterLoader;
class MidiReceiver;

class BeatMaster : public SongParserSink, public HxMaster {
public:
    BeatMaster(SongData *, int);
    virtual ~BeatMaster();
    virtual void AddTrack(int, Symbol, SongInfoAudioType, TrackType, bool);
    virtual void AddMultiGem(int, const GameGem &) {}
    virtual void AddPhrase(BeatmatchPhraseType, int, const Phrase &) {}
    virtual void Poll(float);
    virtual void Jump(float);
    virtual void Reset();
    virtual class HxAudio *GetHxAudio() { return mAudio; }
    virtual float SongDurationMs();

    void RegisterSink(BeatMasterSink &);
    void HandleBeatCallback(Symbol);
    void LoaderPoll();
    void
    Load(SongInfo *, int, PlayerTrackConfigList *, bool, SongDataValidate, std::vector<MidiReceiver *> *);
    bool IsLoaded();
    void CheckBeat();
    void CheckSubmixes(int);
    void ResetAudio();
    void ExportInitialSubmixes();

    MasterAudio *GetAudio() { return mAudio; }
    MidiParserMgr *GetMidiParserMgr() const { return mMidiParserMgr; }

    bool mRecording; // 0x8
    SongData *mSongData; // 0xc
    std::vector<BeatMasterSink *> mSinks; // 0x10
    MasterAudio *mAudio; // 0x1c
    MidiParserMgr *mMidiParserMgr; // 0x20
    SongInfo *mSongInfo; // 0x24
    PlayerTrackConfigList *mPtCfg; // 0x28
    BeatMasterLoader *mLoader; // 0x2c
    bool mLoaded; // 0x30
    bool unk2d; // 0x31
    SongPos mSongPos; // 0x34
    SongPos mLastSongPos; // 0x48
    std::vector<int> mSubmixIdxs; // 0x5c
    DataArray *mHandlers; // 0x68
};

class BeatMasterLoader : public Loader {
public:
    BeatMasterLoader(BeatMaster *bm)
        : Loader(FilePath(""), kLoadFrontStayBack), mBeatMaster(bm) {}
    virtual ~BeatMasterLoader() {}
    virtual const char *DebugText();
    virtual bool IsLoaded() const { return false; }
    virtual const char *StateName() const { return "BeatMasterLoader"; }
    virtual void PollLoading() { mBeatMaster->LoaderPoll(); }

    BeatMaster *mBeatMaster; // 0x1c
};
