#include "char/CharLipSync.h"
#include "math/Utl.h"
#include "obj/Data.h"
#include "obj/DataFile.h"
#include "obj/Msg.h"
#include "obj/Object.h"
#include "obj/PropSync.h"
#include "os/Debug.h"
#include "rndobj/PropAnim.h"
#include "utl/TextStream.h"

// RB3-360 retail rev storage. Retail's LOAD_REVS keeps NO BinStreamRev: it splits
// the packed rev into two mutable file-scope shorts, and ASSERT_REVS emits nothing.
// The two words must live in ONE aligned(4) aggregate (altRev +0, rev +4) -- MSVC
// does not lay .bss out in declaration order, so two separate statics get other
// globals interleaved between them and will not fold onto one base register.
static struct {
    __declspec(align(4)) unsigned short altRev;
    __declspec(align(4)) unsigned short rev;
} gRevs_CharLipSync;
#define gAltRev gRevs_CharLipSync.altRev
#define gRev gRevs_CharLipSync.rev


std::map<Symbol, CharLipSync *> *CharLipSync::sLipSyncMap;

CharLipSync::CharLipSync() : mPropAnim(this), mFrames(0) {}
CharLipSync::~CharLipSync() { UnregisterLipSync(this); }

BEGIN_HANDLERS(CharLipSync)
    HANDLE(parse, OnParse)
    HANDLE(parse_array, OnParseArray)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(CharLipSync)
    {
        static Symbol _s("frames");
        if (sym == _s && (_op & kPropGet))
            return PropSync(mFrames, _val, _prop, _i + 1, _op);
    }
    SYNC_PROP_SET(duration, Duration(), )

    {
        static Symbol _s("visemes");
        if (sym == _s && (_op & (kPropGet | kPropSize)))
            return PropSync(mVisemes, _val, _prop, _i + 1, _op);
    }
    SYNC_PROP(prop_anim, mPropAnim)
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

BEGIN_SAVES(CharLipSync)
    SAVE_REVS(2, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    bs << mVisemes;
    bs << mFrames;
    bs << mData;
END_SAVES

BEGIN_LOADS(CharLipSync)
    int rev;
    bs >> rev;
    gRev = getHmxRev(rev);
    gAltRev = getAltRev(rev);
    Hmx::Object::Load(bs);
    bs >> mVisemes;
    bs >> mFrames;
    bs >> mData;
    // RB3-360 retail: `gRev != 0` (not `== 1`), and NO RegisterLipSync(this) tail.
    if (gRev != 0) {
        bs >> mPropAnim;
    }
END_LOADS
#undef gRev
#undef gAltRev

BEGIN_COPYS(CharLipSync)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(CharLipSync)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mVisemes)
        COPY_MEMBER(mFrames)
        COPY_MEMBER(mData)
        COPY_MEMBER(mPropAnim)
    END_COPYING_MEMBERS
END_COPYS

void CharLipSync::Print(TextStream &ts) {
    std::vector<unsigned char> data;
    data.resize(mVisemes.size());
    for (int i = 0; i < mVisemes.size(); i++) {
        data[i] = 0;
    }
    ts << "; song: " << PathName(this) << "\n";
    ts << "(visemes\n";
    for (int i = 0; i < mVisemes.size(); i++) {
        ts << "   " << mVisemes[i].c_str() << "\n";
    }
    ts << ")\n";
    ts << "(frames ; @ 30fps\n";
    int idx = 0;
    for (int i = 0; i < mFrames; i++) {
        int count = mData[idx++];
        for (int j = 0; j < count; j++) {
            int visemeIdx = mData[idx++];
            data[visemeIdx] = mData[idx++];
        }
        ts << "   ( ";
        for (int j = 0; j < mVisemes.size(); j++) {
            ts << data[j] * 0.003921568859368563f << " ";
        }
        ts << ")\n";
    }
    ts << ")\n";
}

void CharLipSync::Init() { sLipSyncMap = new std::map<Symbol, CharLipSync *>(); }
void CharLipSync::Terminate() { RELEASE(sLipSyncMap); }

void CharLipSync::RegisterLipSync(CharLipSync *sync) {
    if (sLipSyncMap) {
        (*sLipSyncMap)[sync->Name()] = sync;
    }
}

void CharLipSync::UnregisterLipSync(CharLipSync *sync) {
    if (sLipSyncMap) {
        sLipSyncMap->erase(sync->Name());
    }
}

DataNode CharLipSync::OnParse(DataArray *a) {
    FilePath path(a->Str(2));
    DataArray *data = DataReadFile(path.c_str(), true);
    if (data) {
        Parse(data);
        data->Release();
    }
    return 0;
}

DataNode CharLipSync::OnParseArray(DataArray *a) {
    DataArray *data = a->Array(2);
    if (data) {
        Parse(data);
        data->Release();
    }
    return 0;
}

void CharLipSync::Parse(DataArray *data) {
    DataArray *visemeArr = data->FindArray("visemes");
    mVisemes.resize(visemeArr->Size() - 1);
    for (int i = 1; i < visemeArr->Size(); i++) {
        mVisemes[i - 1] = visemeArr->Str(i);
    }
    Generator gen;
    gen.Init(this);
    DataArray *frameArr = data->FindArray("frames");
    for (int i = 1; i < frameArr->Size(); i++) {
        DataArray *curArr = frameArr->Array(i);
        for (int j = 0; j < curArr->Size(); j++) {
            gen.AddWeight(j, curArr->Float(j));
        }
        gen.NextFrame();
    }
    gen.Finish();
    Print(TheDebug);
}

void CharLipSync::Generator::AddWeight(int visemeIdx, float weight) {
    float clamped = Clamp(0.0f, 255.0f, weight * 255.0f + 0.5f);
    unsigned char val = clamped;
    if (mWeights[visemeIdx].mPrev != val || mWeights[visemeIdx].mCur != val) {
        unsigned char idx = (unsigned char)visemeIdx;
        mLipSync->mData.push_back(idx);
        mLipSync->mData.push_back(val);
        mWeights[visemeIdx].mPrev = mWeights[visemeIdx].mCur;
        mWeights[visemeIdx].mCur = val;
    }
}

void CharLipSync::Generator::Init(CharLipSync *sync) {
    mLipSync = sync;
    mLipSync->mData.resize(0);
    mWeights.resize(mLipSync->mVisemes.size());
    for (int i = 0; i < mWeights.size(); i++) {
        mWeights[i].mPrev = 0;
        mWeights[i].mCur = 0;
    }
    mLastCount = mLipSync->mData.size();
    mLipSync->mData.push_back(0);
    mLipSync->mFrames = 0;
}

void CharLipSync::Generator::NextFrame() {
    int count = (mLipSync->mData.size() - 1 - mLastCount) / 2;
    MILO_ASSERT(count >= 0 && count < 256, 0x53);
    mLipSync->mData[mLastCount] = count;
    mLastCount = mLipSync->mData.size();
    mLipSync->mData.push_back(0);
    mLipSync->mFrames++;
}

void CharLipSync::Generator::Finish() {
    mLipSync->mData.pop_back();
    std::vector<bool> bools;
    bools.resize(mLipSync->mVisemes.size());
    for (int i = 0; i < bools.size(); i++) {
        bools[i] = 0;
    }

    std::vector<unsigned char> &data = mLipSync->mData;
    int idx = 0;
    for (int i = 0; i < mLipSync->mFrames; i++) {
        int count = data[idx++];
        MILO_ASSERT(count <= mLipSync->mVisemes.size(), 0x6A);
        for (int j = 0; j < count; j++) {
            int viseme = data[idx++];
            MILO_ASSERT(viseme < mLipSync->mVisemes.size(), 0x6E);
            if (data[idx++] != 0) {
                bools[viseme] = 1;
            }
        }
    }

    for (int i = 0; i < bools.size();) {
        if (!bools[i]) {
            bools.erase(bools.begin() + i);
            RemoveViseme(i);
        } else {
            i++;
        }
    }
}

void CharLipSync::Generator::RemoveViseme(int visemeIdx) {
    mLipSync->mVisemes.erase(mLipSync->mVisemes.begin() + visemeIdx);

    int cur = 0;
    CharLipSync *lipSync = mLipSync;
    int i = 0;
    std::vector<unsigned char> &data = lipSync->mData;
    if (lipSync->mFrames > 0) {
        do {
            int j = 0;
            int count = data[cur++];
            if (count > 0) {
                do {
                    if (data[cur] >= visemeIdx) {
                        data[cur]--;
                        MILO_ASSERT(data[cur] < mLipSync->mVisemes.size(), 0x96);
                    }
                    j++;
                    cur += 2;
                } while (j < count);
            }
            i++;
        } while (i < mLipSync->mFrames);
    }
}

CharLipSync *CharLipSync::FindLipSyncForSound(Sound *sound) {
    if (sLipSyncMap) {
        String name(sound->Name());
        int ext = name.find_last_of('.');
        if (ext >= 0) {
            name.resize(ext);
            name += ".lipsync";
            return (*sLipSyncMap)[name.c_str()];
        }
    }
    return nullptr;
}

CharLipSync::PlayBack::PlayBack()
    : mLipSync(nullptr), mClips(nullptr), mIndex(0), mOldIndex(0), mFrame(-1) {}

void CharLipSync::PlayBack::Set(CharLipSync *lipsync, ObjPtr<ObjectDir> clips) {
    mClips = clips;
    mLipSync = lipsync;

    int numVisemes = mLipSync->mVisemes.size();
    auto& _ref2 = mWeights;
    _ref2.resize(numVisemes);

    for (int i = 0; i < _ref2.size(); i++) {
        ObjPtr<CharClip> &clip = _ref2[i].mClip;
        clip = mClips->Find<CharClip>(mLipSync->mVisemes[i].c_str(), false);
        if (!clip) {
            MILO_LOG("could not find %s", (char *)mLipSync->mVisemes[i].c_str());
        }
    }

    static Message viseme_list("viseme_list");
    DataNode result = mLipSync->Handle(viseme_list, false);

    if (result.Type() == kDataArray) {
        DataArray *arr = result.Array(0);
        int arrSize = arr->Size();
        int newSize = numVisemes + arrSize;
        if (_ref2.size() != newSize) {
            _ref2.resize(newSize);
            for (int i = numVisemes; (unsigned int)i < newSize; i++) {
                Symbol visemeSym = arr->Sym(i - numVisemes);
                ObjPtr<CharClip> &clip = _ref2[i].mClip;
                clip = mClips->Find<CharClip>(visemeSym.Str(), false);
            }
        }
    }
}

void CharLipSync::PlayBack::Poll(float time) {
    if (!mLipSync)
        return;

    float zero = 0.0f;
    static Message viseme_list("viseme_list");
    DataNode result = mLipSync->Handle(viseme_list, false);

    if (result.Type() == kDataArray) {
        int numVisemes = mLipSync->mVisemes.size();
        DataArray *arr = result.Array(0);
        int end = arr->Size() + numVisemes;
        if (numVisemes < end) {
            int visIdx = 0;
            float one = 1.0f;
            CharLipSync *ls = mLipSync;
            for (; numVisemes < end; visIdx++, numVisemes++) {
                Symbol visemeSym = result.Array(0)->Sym(visIdx);
                float weight = ls->Property(visemeSym, true)->Float(0);
                if ((unsigned int)numVisemes < mWeights.size()) {
                    mWeights[numVisemes].mCurWeight = Clamp(zero, one, weight);
                }
            }
        }
    }

    if (mLipSync->mFrames < 2) {
        return;
    }

    float frame = time * 30.0f;
    int frameIdx = (int)(float)ceil(frame);
    float frac = frame - (float)(frameIdx - 1);

    if (frameIdx < 1) {
        frameIdx = 1;
        frac = zero;
    } else if (frameIdx >= mLipSync->mFrames - 1) {
        frameIdx = mLipSync->mFrames - 1;
        frac = 0.9999999f;
    }

    CharLipSync *lipSync = mLipSync;
    if (frameIdx < mFrame) {
        Reset();
    }

    if (mFrame < frameIdx) {
        float conv = 1.0f / 255.0f;
        do {
            mOldIndex = mIndex++;
            int count = lipSync->mData[mOldIndex];
            if (count != 0) {
                for (int i = count; i != 0; i--) {
                    int idx = lipSync->mData[mIndex++];
                    Weight &w = mWeights[idx];
                    w.mPrevWeight = w.mNextWeight;
                    int val = lipSync->mData[mIndex++];
                    w.mNextWeight = (float)val * conv;
                    w.mCurWeight = Interp(w.mPrevWeight, w.mNextWeight, frac);
                }
            }
            mFrame++;
        } while (mFrame < frameIdx);
    } else if (mFrame >= 0 && mFrame == frameIdx) {
        int idx = mOldIndex + 1;
        int count = lipSync->mData[mOldIndex];
        if (count != 0) {
            for (int i = count; i != 0; i--) {
                int wIdx = lipSync->mData[idx];
                idx += 2;
                Weight &w = mWeights[wIdx];
                w.mCurWeight = Interp(w.mPrevWeight, w.mNextWeight, frac);
            }
        }
    }
}

void CharLipSync::PlayBack::SetClips(ObjPtr<ObjectDir> clips) {
    if (!mLipSync)
        return;
    mClips = clips;

    static Message viseme_list("viseme_list");
    DataNode result = mLipSync->Handle(viseme_list, false);

    if (result.Type() == kDataArray) {
        DataArray *arr = result.Array(0);
        int arrSize = arr->Size();
        if (mWeights.size() == arrSize) {
            mWeights.resize(arrSize);
            for (int i = 0; i < arrSize; i++) {
                Symbol visemeSym = result.Array(0)->Sym(i);
                ObjPtr<CharClip> &clip = mWeights[i].mClip;
                clip = mClips->Find<CharClip>(visemeSym.Str(), false);
            }
        }
    }
}

void CharLipSync::PlayBack::Reset() {
    mIndex = 0;
    mFrame = -1;
    for (int i = 0; i < mWeights.size(); i++) {
        Weight &weight = mWeights[i];
        weight.mNextWeight = 0;
        weight.mCurWeight = 0;
        weight.mPrevWeight = 0;
    }
}

// sw2 scatter-include (default/CharLipSync <- rndobj/Part.cpp)
#define gRev gRev_Part
#define gAltRev gAltRev_Part
#include "rndobj/Part.cpp"
#undef gRev
#undef gAltRev

// sw2 scatter-include (default/CharLipSync <- utl/UTF8.cpp)
#define gRev gRev_UTF8
#define gAltRev gAltRev_UTF8
#include "utl/UTF8.cpp"
#undef gRev
#undef gAltRev

// sw2 scatter-include (default/CharLipSync <- gesture/SkeletonClip.cpp)
#define gRev gRev_SkeletonClip
#define gAltRev gAltRev_SkeletonClip
#include "gesture/SkeletonClip.cpp"
#undef gRev
#undef gAltRev
