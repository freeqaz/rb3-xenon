#include "rndobj/AnimFilter.h"
#include "math/Rand.h"
#include "obj/Object.h"
#include "rndobj/Anim.h"
#include "rndobj/Utl.h"
#include "utl/BinStream.h"

RndAnimFilter::RndAnimFilter()
    : mAnim(this), mPeriod(0.0f), mStart(0.0f), mEnd(0.0f), mScale(1.0f), mOffset(0.0f),
      mSnap(0.0f), mJitter(0.0f), mJitterFrame(0.0f), mType(kRange) {}

BEGIN_HANDLERS(RndAnimFilter)
    HANDLE(safe_anims, OnSafeAnims)
    HANDLE_SUPERCLASS(RndAnimatable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(RndAnimFilter)
    SYNC_PROP_SET(anim, mAnim.Ptr(), SetAnim(_val.Obj<RndAnimatable>()))
    SYNC_PROP_SET(scale, mScale, mScale = std::fabs(_val.Float()))
    SYNC_PROP(offset, mOffset)
    SYNC_PROP(period, mPeriod)
    SYNC_PROP(start, mStart)
    SYNC_PROP(end, mEnd)
    SYNC_PROP(snap, mSnap)
    SYNC_PROP_MODIFY(jitter, mJitter, mJitterFrame = 0.0f)
    SYNC_PROP(type, (int &)mType)
    SYNC_SUPERCLASS(RndAnimatable)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

BEGIN_SAVES(RndAnimFilter)
    SAVE_REVS(2, 0)
    SAVE_SUPERCLASS(Hmx::Object);
    SAVE_SUPERCLASS(RndAnimatable);
    bs << mAnim << mScale << mOffset << mStart << mEnd << mType;
    bs << mPeriod << mSnap << mJitter;
END_SAVES

BEGIN_COPYS(RndAnimFilter)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndAnimatable)
    CREATE_COPY(RndAnimFilter)
    BEGIN_COPYING_MEMBERS
        if (ty != kCopyFromMax) {
            COPY_MEMBER(mScale)
            COPY_MEMBER(mOffset)
            COPY_MEMBER(mStart)
            COPY_MEMBER(mEnd)
            COPY_MEMBER(mType)
            COPY_MEMBER(mAnim)
            COPY_MEMBER(mPeriod)
            COPY_MEMBER(mSnap)
            COPY_MEMBER(mJitter)
        }
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(2, 0)

BEGIN_LOADS(RndAnimFilter)
    LOAD_REVS(bs);
    ASSERT_REVS(2, 0);
    LOAD_SUPERCLASS(Hmx::Object)
    LOAD_SUPERCLASS(RndAnimatable)
    d >> mAnim;
    d >> mScale;
    d >> mOffset;
    d >> mStart;
    d >> mEnd;
    if (d.rev > 0) {
        d >> (int &)mType;
        d >> mPeriod;
    } else {
        bool b;
        d >> b;
        mType = static_cast<RndAnimFilter::Type>(b);
    }
    if (d.rev > 1) {
        d >> mSnap >> mJitter;
    }
END_LOADS

void RndAnimFilter::SetFrame(float frame, float blend) {
    RndAnimatable::SetFrame(frame, blend);
    if (mAnim) {
        frame = frame * Scale() + FrameOffset();
        if (mSnap) {
            frame = mSnap * (int)(frame / mSnap + 0.5f);
        }
        if (mJitter && frame != mJitterFrame) {
            mJitterFrame = frame;
            frame += RandomFloat(-mJitter, mJitter);
        }
        float start, end;
        if (mEnd >= mStart) {
            start = mStart;
            end = mEnd;
        } else {
            start = mEnd;
            end = mStart;
        }
        Type ty = mType;
        if (ty == 1) {
            frame = ModRange(start, end, frame);
        } else if (ty == 0) {
            frame = Clamp(start, end, frame);
        } else if (ty == 2) {
            int iref;
            frame = Limit(start, end, frame, iref);
            if (iref & 1) {
                frame = mEnd - (frame - mStart);
            }
        }
        mAnim->SetFrame(frame, blend);
    }
}

float RndAnimFilter::StartFrame() {
    if (!mAnim) {
        return 0.0f;
    } else {
        float denom = Scale();
        if (denom == 0.0f) {
            denom = 1.0f;
        }
        return (mStart - FrameOffset()) / denom;
    }
}

float RndAnimFilter::EndFrame() {
    if (!mAnim) {
        return 0.0f;
    } else {
        float denom = Scale();
        if (denom == 0.0f) {
            denom = 1.0f;
        }
        float ret = (mEnd - FrameOffset()) / denom;
        if (mType == kShuttle) {
            ret *= 2.0f;
        }
        return ret;
    }
}

void RndAnimFilter::ListAnimChildren(std::list<RndAnimatable *> &theList) const {
    if (mAnim)
        theList.push_back(mAnim);
}

void RndAnimFilter::SetAnim(RndAnimatable *anim) {
    mAnim = anim;
    if (mAnim) {
        SetRate(mAnim->GetRate());
        mStart = mAnim->StartFrame();
        mEnd = mAnim->EndFrame();
    }
}

float RndAnimFilter::Scale() {
    if (mPeriod) {
        return (mEnd - mStart) / (mPeriod * FramesPerUnit());
    } else if (mEnd >= mStart) {
        return mScale;
    } else {
        return -mScale;
    }
}

DataNode RndAnimFilter::OnSafeAnims(DataArray *da) {
    ObjectDir *dir = da->Obj<ObjectDir>(2);
    int containsCount = 0;
    for (ObjDirItr<RndAnimatable> it(dir, true); it != nullptr; ++it) {
        if (!AnimContains(it, this))
            containsCount++;
    }
    containsCount++;
    DataArrayPtr ptr(new DataArray(containsCount));
    containsCount = 0;
    for (ObjDirItr<RndAnimatable> it(dir, true); it != nullptr; ++it) {
        if (!AnimContains(it, this)) {
            ptr->Node(containsCount++) = &*it;
        }
    }
    ptr->Node(containsCount) = NULL_OBJ;
    return ptr;
}
