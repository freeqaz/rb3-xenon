#include "synth/Faders.h"

#ifdef HX_NATIVE
inline double __fsel(double a, double b, double c) { return a >= 0.0 ? b : c; }
#include "obj/Dir.h"
#endif
#include "math/Easing.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "utl/BinStream.h"

#pragma region Fader

Fader::Fader()
    : mLevel(0), mLevelEaseParam(2), mLevelTarget(0), mPan(0), mPanEaseParam(1),
      mPanTarget(0), mTranspose(0), mTransposeEaseParam(1), mTransposeTarget(0),
      mFadeDuration(0) {
    mVolumeContributions.clear();
    mLevelEaseType = kEasePolyOut;
    mLevelEaseFunc = gEaseFuncs[mLevelEaseType];
    mPanEaseType = kEaseLinear;
    mPanEaseFunc = gEaseFuncs[mPanEaseType];
    mTransposeEaseType = kEaseLinear;
    mTransposeEaseFunc = gEaseFuncs[mTransposeEaseType];
}

Fader::~Fader() {
    mTimer.Stop();
    CancelPolling();
}

BEGIN_HANDLERS(Fader)
    HANDLE_ACTION(set_val, SetVolume(_msg->Float(2)))
    HANDLE_ACTION(fade, DoFade(_msg->Float(2), _msg->Float(3)))
    HANDLE_ACTION(do_fade, DoFade(_msg->Float(2)))
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(Fader)
    SYNC_PROP_SET(level, mLevel, SetVolume(_val.Float()))
    SYNC_PROP_SET(
        level_ease_type, mLevelEaseType, mLevelEaseType = (EaseType)_val.Int();
        mLevelEaseFunc = GetEaseFunction(mLevelEaseType)
    )
    SYNC_PROP_SET(level_ease_param, mLevelEaseParam, mLevelEaseParam = _val.Float())
    SYNC_PROP_SET(level_target, mLevelTarget, mLevelTarget = _val.Float())
    SYNC_PROP_SET(pan, mPan, SetPan(_val.Float()))
    SYNC_PROP_SET(
        pan_ease_type, mPanEaseType, mPanEaseType = (EaseType)_val.Int();
        mPanEaseFunc = GetEaseFunction(mPanEaseType)
    )
    SYNC_PROP_SET(pan_ease_param, mPanEaseParam, mPanEaseParam = _val.Float())
    SYNC_PROP_SET(pan_target, mPanTarget, mPanTarget = _val.Float())
    SYNC_PROP_SET(transpose, mTranspose, SetTranspose(_val.Float()))
    SYNC_PROP_SET(
        transpose_ease_type,
        mTransposeEaseType,
        mTransposeEaseType = (EaseType)_val.Int();
        mTransposeEaseFunc = GetEaseFunction(mTransposeEaseType)
    )
    SYNC_PROP_SET(
        transpose_ease_param, mTransposeEaseParam, mTransposeEaseParam = _val.Float()
    )
    SYNC_PROP_SET(transpose_target, mTransposeTarget, mTransposeTarget = _val.Float())
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

BEGIN_SAVES(Fader)
    if (!mLocalName.Null()) {
        MILO_FAIL("can't save local fader");
    }
    SAVE_REVS(1, 0)
    SAVE_SUPERCLASS(Hmx::Object);
    bs << mLevel;
    bs << mLevelEaseType;
    bs << mLevelEaseParam;
    bs << mLevelTarget;
    bs << mPan;
    bs << mPanEaseType;
    bs << mPanEaseParam;
    bs << mPanTarget;
    bs << mTranspose;
    bs << mTransposeEaseType;
    bs << mTransposeEaseParam;
    bs << mTransposeTarget;
END_SAVES

BEGIN_COPYS(Fader)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(Fader)
    BEGIN_COPYING_MEMBERS
        if (ty != kCopyFromMax) {
            SetVolume(c->mLevel);
            COPY_MEMBER(mLevelEaseType)
            mLevelEaseFunc = GetEaseFunction(mLevelEaseType);
            COPY_MEMBER(mLevelEaseParam)
            COPY_MEMBER(mLevelTarget)
            SetPan(c->mPan);
            COPY_MEMBER(mPanEaseType)
            mPanEaseFunc = GetEaseFunction(mPanEaseType);
            COPY_MEMBER(mPanEaseParam)
            COPY_MEMBER(mPanTarget)
            SetTranspose(c->mTranspose);
            COPY_MEMBER(mTransposeEaseType)
            mTransposeEaseFunc = GetEaseFunction(mTransposeEaseType);
            COPY_MEMBER(mTransposeEaseParam)
            COPY_MEMBER(mTransposeTarget)
        }
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(1, 0)

BEGIN_LOADS(Fader)
    LOAD_REVS(bs)
    ASSERT_REVS(1, 0)
    Hmx::Object::Load(bs);
    float volume;
    bs >> volume;
    SetVolume(volume);
    if (d.rev >= 1) {
        bs >> (int &)mLevelEaseType;
        mLevelEaseFunc = GetEaseFunction(mLevelEaseType);
        bs >> mLevelEaseParam;
        bs >> mLevelTarget;
        bs >> mPan;
        bs >> (int &)mPanEaseType;
        mPanEaseFunc = GetEaseFunction(mPanEaseType);
        bs >> mPanEaseParam;
        bs >> mPanTarget;
        bs >> mTranspose;
        bs >> (int &)mTransposeEaseType;
        mTransposeEaseFunc = GetEaseFunction(mTransposeEaseType);
        bs >> mTransposeEaseParam;
        bs >> mTransposeTarget;
    }
END_LOADS

void Fader::SynthPoll() {
    float t = mTimer.SplitMs() / mFadeDuration;
    if (t >= 1.0f) {
        mTimer.Stop();
        CancelPolling();
        t = 1.0f;
    }

    float levelEase = mLevelEaseFunc(t, mLevelEaseParam, 0.0f);
    levelEase = (float)__fsel(-levelEase, 0.0f, levelEase);
    levelEase = (float)__fsel(levelEase - 1.0f, 1.0f, levelEase);
    float level = mLevelStart + (mLevelTarget - mLevelStart) * levelEase;

    float panEase = mPanEaseFunc(t, mPanEaseParam, 0.0f);
    panEase = (float)__fsel(-panEase, 0.0f, panEase);
    panEase = (float)__fsel(panEase - 1.0f, 1.0f, panEase);
    float pan = mPanStart + (mPanTarget - mPanStart) * panEase;

    float transposeEase = mTransposeEaseFunc(t, mTransposeEaseParam, 0.0f);
    transposeEase = (float)__fsel(-transposeEase, 0.0f, transposeEase);
    transposeEase = (float)__fsel(transposeEase - 1.0f, 1.0f, transposeEase);
    float transpose = mTransposeStart + (mTransposeTarget - mTransposeStart) * transposeEase;

    UpdateValue(level, pan, transpose);
}

void Fader::DoFade(float durationMs) {
    MILO_ASSERT(durationMs >= 0.0f, 0x5B);
    mFadeDuration = durationMs;
    mLevelStart = mLevel;
    mPanStart = mPan;
    mTransposeStart = mTranspose;
    mTimer.Stop();
    CancelPolling();
    StartPolling();
    mTimer.Restart();
    MILO_ASSERT(mTimer.Running(), 0x65);
}

float Fader::GetDuckedVolume() const {
    float vol = 0;
    FOREACH (it, mVolumeContributions) {
        if (*it < vol) {
            vol = *it;
        }
    }
    return vol;
}

void Fader::DoFade(float targetVol, float durationMs) {
    mLevelTarget = targetVol;
    DoFade(durationMs);
}

void Fader::UpdateValue(float vol, float pan, float transpose) {
    mLevel = vol;
    mPan = pan;
    mTranspose = transpose;
    FOREACH (it, mClients) {
        (*it)->SetDirty();
    }
}

void Fader::RemoveDuckedVolume(float f1) {
    std::list<float>::iterator it = mVolumeContributions.begin();
    for (; it != mVolumeContributions.end(); ++it) {
        if (*it == f1) {
            mVolumeContributions.erase(it);
            FOREACH (it, mClients) {
                (*it)->SetDirty();
            }
            return;
        }
    }
}

void Fader::SetVolume(float vol) {
    mTimer.Stop();
    CancelPolling();
    UpdateValue(vol, mPan, mTranspose);
}

void Fader::SetPan(float pan) {
    mTimer.Stop();
    CancelPolling();
    UpdateValue(mLevel, pan, mTranspose);
}

void Fader::SetTranspose(float transpose) {
    mTimer.Stop();
    CancelPolling();
    UpdateValue(mLevel, mPan, transpose);
}

void Fader::AddDuckedVolume(float vol) {
    mVolumeContributions.push_back(vol);
    FOREACH (it, mClients) {
        (*it)->SetDirty();
    }
}

#pragma endregion
#pragma region FaderGroup

FaderGroup::FaderGroup(Hmx::Object *owner) : mFaders(owner), mDirty(true) {}

FaderGroup::~FaderGroup() {
#ifdef HX_NATIVE
    if (ObjectDir::InDeleteObjects()) {
        // During cascade, local Faders (owned by this group) may already be
        // destroyed — skip delete. But non-local Faders (e.g. TheSynth's
        // master fader) survive the cascade and still hold this FaderGroup
        // in their mClients set. We MUST call RemoveClient on surviving
        // faders, otherwise Fader::UpdateValue() iterates mClients and
        // calls SetDirty() on freed memory (heap-use-after-free).
        while (!mFaders.empty()) {
            Fader *f = mFaders.front();
            mFaders.pop_front();
            if (f && f->IsRefAlive()) {
                f->RemoveClient(this);
            }
        }
        return;
    }
#endif
    while (!mFaders.empty()) {
        Fader *frontObj = mFaders.front();
        mFaders.pop_front();
        frontObj->RemoveClient(this);
        if (!frontObj->LocalName().Null()) {
            delete frontObj;
        }
    }
}

void FaderGroup::SetDirty() { mDirty = true; }
void FaderGroup::ClearDirty() { mDirty = false; }
bool FaderGroup::Dirty() { return mDirty; }

float FaderGroup::GetPan() const {
    float pan = 0;
    FOREACH (it, mFaders) {
        pan += (*it)->GetPan();
    }
    return pan;
}

float FaderGroup::GetTranspose() const {
    float transpose = 0;
    FOREACH (it, mFaders) {
        transpose += (*it)->GetTranspose();
    }
    return transpose;
}

Fader *FaderGroup::FindLocal(Symbol name, bool fail) {
    if (!name.Null()) {
        FOREACH (it, mFaders) {
            if ((*it)->LocalName() == name) {
                return *it;
            }
        }
    }
    if (fail) {
        MILO_FAIL("bad local fader: %s", name);
    }
    return nullptr;
}

float FaderGroup::GetVolume() const {
    float vol = 0;
    FOREACH (it, mFaders) {
        Fader *fader = *it;
        float duckedVol = 0;
        FOREACH (cit, fader->mVolumeContributions) {
            if (*cit < duckedVol) {
                duckedVol = *cit;
            }
        }
        vol += fader->mLevel + duckedVol;
    }
    return vol;
}

void FaderGroup::GetVal(float &vol, float &pan, float &transpose) const {
    vol = 0;
    pan = 0;
    transpose = 0;
    FOREACH (it, mFaders) {
        Fader *fader = *it;
        float val = 0;
        FOREACH (cit, fader->mVolumeContributions) {
            if (*cit < val) {
                val = *cit;
            }
        }
        vol += fader->mLevel + val;
        pan += (*it)->mPan;
        transpose += (*it)->mTranspose;
    }
}

bool PropSync(FaderGroup &grp, DataNode &node, DataArray *prop, int i, PropOp op) {
    ObjPtrList<Fader> pList(grp.Faders());
    for (ObjPtrList<Fader>::iterator it = pList.begin(); it != pList.end(); it) {
        if ((*it)->Dir() && (*it)->LocalName().Null())
            ++it;
        else
            it = pList.erase(it);
    }
    bool sync = PropSync(pList, node, prop, i, op);
    for (ObjPtrList<Fader>::iterator it = grp.Faders().begin(); it != grp.Faders().end();
         it) {
        Fader *f = *it++;
        if (f->Dir() && f->LocalName().Null())
            grp.Remove(f);
    }
    for (ObjPtrList<Fader>::iterator it = pList.begin(); it != pList.end(); ++it) {
        grp.Add(*it);
    }
    return sync;
}

Fader *FaderGroup::AddLocal(Symbol name) {
    MILO_ASSERT(!name.Null(), 0x14F);
    FOREACH (it, mFaders) {
        if ((*it)->LocalName() == name) {
            return *it;
        }
    }
    Fader *f = Hmx::Object::New<Fader>();
    f->SetLocalName(name);
    f->AddClient(this);
    mFaders.push_back(f);
    mDirty = true;
    return f;
}

void FaderGroup::Add(Fader *f) {
    FOREACH (it, mFaders) {
        if (*it == f)
            return;
    }
    if (f) {
        f->AddClient(this);
        mFaders.push_back(f);
    }
    mDirty = true;
}

void FaderGroup::Remove(Fader *f) {
    FOREACH (it, mFaders) {
        if (*it == f) {
            f->RemoveClient(this);
            mFaders.erase(it);
            mDirty = true;
            return;
        }
    }
}

void FaderGroup::Save(BinStream &bs) {
    bs << 0;
    ObjPtrList<Fader> faders(mFaders);
    for (auto it = faders.begin(); it != faders.end(); it) {
        if ((*it)->Dir() && (*it)->LocalName().Null()) {
            ++it;
        } else {
            it = faders.erase(it);
        }
    }
    bs << faders;
}

#define kGroupRev 0

void FaderGroup::Load(BinStream &bs) {
    int rev;
    bs >> rev;
    MILO_ASSERT(rev <= kGroupRev, 0x213);
    ObjPtrList<Fader> pList(mFaders.Owner());
    bs >> pList;
    for (ObjPtrList<Fader>::iterator it = mFaders.begin(); it != mFaders.end(); it) {
        Fader *f = *it++;
        if (f->Dir() && f->LocalName().Null())
            Remove(f);
    }
    for (ObjPtrList<Fader>::iterator it = pList.begin(); it != pList.end(); ++it) {
        Add(*it);
    }
}
