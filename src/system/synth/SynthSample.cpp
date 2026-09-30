#include "synth/SynthSample.h"
#include "../../Memory.h"
#include "SampleData.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/Platform.h"
#include "synth/SampleInst.h"
#include "utl/BinStream.h"
#include "utl/BufStream.h"
#include "utl/Loader.h"
#include "utl/MemMgr.h"

FileLoader *SynthSample::sLoader = nullptr;
SynthSample *SynthSample::sLoading = nullptr;
bool sDisabled = false;

void *SampleAlloc(int size) {
    return MemAlloc(size, __FILE__, 0x1C, "Sample Data");
}

#pragma region SynthSample

SynthSample::SynthSample() {}

SynthSample::~SynthSample() {
#ifdef HX_NATIVE
    FOREACH (it, mSampleInsts) {
        (*it)->Stop(true);
    }
#endif
    if (sLoading == this) {
        RELEASE(sLoader);
        sLoading = nullptr;
    }
}

BEGIN_HANDLERS(SynthSample)
    HANDLE_EXPR(platform_size_kb, GetPlatformSize(kPlatformNone) / 1024)
    HANDLE_EXPR(num_markers, NumMarkers())
    HANDLE_EXPR(marker_name, mSampleData.GetMarker(_msg->Int(2)).Name())
    HANDLE_EXPR(marker_sample, mSampleData.GetMarker(_msg->Int(2)).Sample())
    HANDLE_EXPR(sample_length, LengthMs() / 1000.0f)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_CUSTOM_PROPSYNC(SampleMarker)
    SYNC_PROP(sample, o.sample)
    SYNC_PROP(name, o.name)
END_CUSTOM_PROPSYNC

BEGIN_PROPSYNCS(SynthSample)
    SYNC_PROP_MODIFY(file, mFile, Sync(sync0))
    SYNC_PROP_SET(
        sample_rate,
        mSampleData.GetSampleRate(),
        MILO_NOTIFY("can't set property %s", "sample_rate")
    )
    SYNC_PROP(markers, mSampleData.AccessMarkers())
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

// RB3 retail is rev 5: the loop fields are still serialized (DC3's rev 6
// dropped them).
BEGIN_SAVES(SynthSample)
    SAVE_REVS(5, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    bs << mFile << mIsLooped << mLoopStartSamp << mLoopEndSamp;
    if (bs.Cached()) {
        mSampleData.Save(bs);
    }
END_SAVES

BEGIN_COPYS(SynthSample)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(SynthSample)
    BEGIN_COPYING_MEMBERS
        if (ty != kCopyFromMax) {
            COPY_MEMBER(mFile)
            COPY_MEMBER(mIsLooped)
            COPY_MEMBER(mLoopStartSamp)
            COPY_MEMBER(mLoopEndSamp)
        }
    END_COPYING_MEMBERS
    Sync(sync0);
END_COPYS

BEGIN_LOADS(SynthSample)
    PreLoad(bs);
    PostLoad(bs);
END_LOADS

INIT_REVS(5, 0)

void SynthSample::PreLoad(BinStream &bs) {
    // RB3 retail: a raw rev (no hmx/alt split), nothing newer than 5 is loaded,
    // and the loop fields are read into the members at every rev.
    int rev;
    bs >> rev;
    if (rev > 5) {
        return;
    }
    if (rev > 1) {
        Hmx::Object::Load(bs);
    }
    bs >> mFile >> mIsLooped >> mLoopStartSamp;
    if (rev >= 3) {
        bs >> mLoopEndSamp;
    }
    if (bs.Cached() && rev >= 5) {
        mSampleData.Load(bs, mFile);
    } else if (rev > 3
#ifdef HX_NATIVE
               && !sDisabled
#endif
    ) {
        sLoader = dynamic_cast<FileLoader *>(TheLoadMgr.AddLoader(mFile, kLoadFront));
        sLoading = this;
    }
}

void SynthSample::PostLoad(BinStream &bs) {
    sLoader = nullptr;
    sLoading = nullptr;
    Sync(bs.Cached() ? sync2 : sync0); // retail: cached -> 2
}

void SynthSample::Disable() { sDisabled = true; }
int SynthSample::GetNumChannels() const { return mSampleData.NumChannels(); }
int SynthSample::GetSampleRate() const { return mSampleData.GetSampleRate(); }
bool SynthSample::GetIsLooped() const { return mIsLooped; }
int SynthSample::GetLoopStartSamp() const { return mLoopStartSamp; }
int SynthSample::GetLoopEndSamp() const { return mLoopEndSamp; }
std::vector<SampleMarker> &SynthSample::AccessMarkers() {
    return mSampleData.AccessMarkers();
}
int SynthSample::NumMarkers() const { return mSampleData.NumMarkers(); }
int SynthSample::GetPlatformSize(Platform) {
    return mSampleData.SizeAs(SampleData::kPCM);
}

void SynthSample::Sync(SyncType ty) {
    if (ty == sync0) {
        mSampleData.Reset();
#ifdef HX_NATIVE
        if (!sDisabled && !mFile.empty()) {
#else
        // Retail 0x82728170: no sDisabled test and no PC WAV branch.
        if (!mFile.empty()) {
#endif
            FileLoader *fl = dynamic_cast<FileLoader *>(TheLoadMgr.ForceGetLoader(mFile));
            int i80;
            const char *cc;
            if (fl) {
                cc = fl->GetBuffer(&i80);
            } else
                cc = nullptr;
            delete fl;
            if (cc) {
                BufStream bs((void *)cc, i80, true);
#ifdef HX_NATIVE
                if (TheLoadMgr.GetPlatform() == kPlatformPC) {
                    mSampleData.LoadWAV(bs, mFile, false);
                } else
#endif
                {
                    mSampleData.Load(bs, mFile);
                }
                delete cc;
            }
        }
    }
}

void SynthSample::RegisterChild(SampleInst *inst) {
#ifdef HX_NATIVE
    mSampleInsts.push_back(inst);
#endif
}

void SynthSample::UnregisterChild(SampleInst *inst) {
#ifdef HX_NATIVE
    FOREACH (it, mSampleInsts) {
        if (*it == inst) {
            mSampleInsts.erase(it);
            return;
        }
    }
    MILO_NOTIFY("Could not find child instance for unregistration!");
#endif
}

void SynthSample::Init() {
    REGISTER_OBJ_FACTORY(SynthSample);
    SampleData::SetAllocator(SampleAlloc, (SampleDataFreeFunc)MemFree);
}

#pragma endregion
