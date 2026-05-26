#pragma once
#include "beatmatch/HxMaster.h"
#include "beatmatch/HxSongData.h"
#include "midi/Midi.h"
#include "obj/Data.h"
#include "rndobj/Anim.h"
#include "rndobj/Overlay.h"
#include "utl/BeatMap.h"
#include "utl/MBT.h"
#include "utl/MeasureMap.h"
#include "utl/MemMgr.h"
#include "utl/TempoMap.h"

class SongCallback {
public:
    SongCallback() {}
    virtual ~SongCallback() {}
    virtual void SongSetFrame(class Song *, float) = 0;
    virtual ObjectDir *SongMainDir() = 0;
    virtual void SongPlay(bool) = 0;
    virtual void UpdateObject(const Hmx::Object *, DataArray *) = 0;
    virtual void Preload() = 0;
    virtual void ProcessBookmarks(DataNode) = 0;
};

class Song : public RndAnimatable, public MidiReceiver, public RndOverlay::Callback {
public:
    // Hmx::Object
    virtual ~Song();
    OBJ_CLASSNAME(Song);
    OBJ_SET_TYPE(Song);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType) {}
    virtual void Load(BinStream &);

    // RndAnimatable
    virtual void SetFrame(float frame, float blend);
    virtual float EndFrame() { return mSongEndFrame; }
    // MidiReceiver
    virtual void OnNewTrack(int) {}
    virtual void OnEndOfTrack() {}
    virtual void OnAllTracksRead() {}
    virtual void OnMidiMessage(int, unsigned char, unsigned char, unsigned char) {}
    virtual void OnText(int tick, const char *text, unsigned char type);
    // RndOverlay::Callback
    virtual float UpdateOverlay(RndOverlay *, float);

    // Song
    virtual DataNode OnMBTFromSeconds(const DataArray *);
    virtual DataNode OnMBTFromTick(const DataArray *);
    virtual void Unload();

    OBJ_MEM_OVERLOAD(0x25)

    void SyncState();
    float GetBeat();
    TempoMap *GetTempoMap();
    BeatMap *GetBeatMap();
    MeasureMap *GetMeasureMap();
    MBT GetMBTFromFrame(float, int *);
    MBT GetMBTFromTick(int, int *);
    void Play();
    void Pause();
    float GetFrameFromMBT(int, int, int);
    int GetTickFromMBT(int, int, int);
    void SetSong(Symbol);

    static SongCallback *sCallback;
    static bool mFastSync;

private:
    virtual void CreateSong(Symbol, DataArray *, HxSongData **s, HxMaster **m) {
        *s = nullptr;
        *m = nullptr;
    }

    void SetStateDirty(bool);
    void AddSection(Symbol, float);
    void JumpTo(Symbol);
    void JumpTo(int);
    void SetLoopStart(float);
    void SetLoopEnd(float);
    void SetSpeed();
    void UpdateDebugParsers();
    ObjectDir *MainDir() const;

    DataNode GetBookmarks();
    DataNode GetMidiParsers();

#ifdef HX_NATIVE
    void PollAsyncState();

    enum AsyncState {
        kAsyncNone,
        kAsyncWaitPlay, // Play() deferred — waiting for IsReady()
        kAsyncWaitSync, // SyncState() deferred — waiting for IsReady()
    };
    AsyncState mAsyncState = kAsyncNone;
    bool mSavedWasPaused = false;
    float mSavedVolume = 0.0f;
#endif

protected:
    Song();

    void Load();
    void LoadSong();

    HxMaster *mHxMaster; // 0x1c
    HxSongData *mHxSongData; // 0x20
    // key = tick, value = section name
    std::map<int, Symbol> mSongSections; // 0x24
    ObjPtrList<Hmx::Object> mDebugParsers; // 0x3c
    Symbol mSongName; // 0x50
    Symbol mLastLoadedSong; // 0x54
    float mSongEndFrame; // 0x58
    float mSpeed; // 0x5c
    Vector2 mLoopPoints; // 0x60
    MBT mLoopStart; // 0x68
    MBT mLoopEnd; // 0x74
    bool mDirty; // 0x80
};
