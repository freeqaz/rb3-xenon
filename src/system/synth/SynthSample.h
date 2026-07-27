#pragma once
#include "obj/Object.h"
#include "os/Platform.h"
#include "synth/SampleData.h"
#include "utl/MemMgr.h"

class SampleInst;

class SynthSample : public Hmx::Object {
public:
    enum SyncType {
        sync0,
        sync1,
        sync2,
        sync3
    };
    // Hmx::Object
    virtual ~SynthSample();
    OBJ_CLASSNAME(SynthSample);
    OBJ_SET_TYPE_ENGINE(SynthSample);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    // SynthSample
#ifdef HX_NATIVE
    virtual SampleInst *NewInst(bool loop, int startSample, int endSample);
#else
    virtual SampleInst *NewInst(bool, int, int) { return nullptr; }
#endif
    virtual float LengthMs() const { return 0; }

    OBJ_MEM_OVERLOAD(0x18);

    int GetNumChannels() const;
    int GetSampleRate() const;
    const SampleData &GetSampleData() const { return mSampleData; }
    int NumMarkers() const;
    int GetPlatformSize(Platform);
    std::vector<SampleMarker> &AccessMarkers();
    void RegisterChild(SampleInst *);
    void UnregisterChild(SampleInst *);

    NEW_OBJ(SynthSample)
    static void Register() { REGISTER_OBJ_FACTORY(SynthSample) }
    static void Init();
    static void Disable();

protected:
    SynthSample();

    virtual void Sync(SyncType);

    static SynthSample *sLoading;
    static FileLoader *sLoader;

    // Retail RB3 layout, verified from the target binary's SynthSample base
    // ctor (mFile String ctor @ +0x28; bool@0x34=0, int@0x38=0, int@0x3c=-1;
    // SampleData ctor @ +0x40) and SynthSample360::IsXMA (mFormat @ +0x4c =
    // SampleData+0xc). DC3's newer engine dropped the loop members; RB3 keeps
    // them (cross-checked against rb3-Wii SynthSample.h).
    FilePath mFile; // 0x28
    bool mIsLooped; // 0x34
    int mLoopStartSamp; // 0x38
    int mLoopEndSamp; // 0x3c
    SampleData mSampleData; // 0x40 (object size 0x60)
#ifdef HX_NATIVE
    // Native build tracks live playing instances so the sample can stop them
    // on teardown; retail RB3 does not keep this list (object ends at 0x60).
    std::list<SampleInst *> mSampleInsts;
#endif
};
