#include "rndobj/Wind.h"
#include "math/Rand.h"
#include "math/Utl.h"
#include "obj/Object.h"
#include "utl/BinStream.h"
#include "math/Rand.h"
#include "math/Utl.h"
#include "math/Mtx.h"

extern float gUnitsPerMeter;
static Rand *sRand = nullptr;
static float sWhiteField[0x400] = { 0 };
static float sWindField[0x401] = { 0 };
Vector3 sOffset(0.0f, 0.3384f, 0.66843998f);

void SetWind(int start, int end, float startVal, float endVal, float amplitude) {
    sWindField[start] = startVal;
    if (end - start >= 2) {
        float half = 0.5f;
        float decay = 1.0f / sqrtf(2.0f);
        do {
            int mid = (start + end) / 2;
            float midVal =
                sRand->Gaussian() * amplitude + (startVal + endVal) * half;
            amplitude *= decay;
            SetWind(start, mid, startVal, midVal, amplitude);
            startVal = midVal;
            start = mid;
            sWindField[mid] = midVal;
        } while (end - start >= 2);
    }
}

float RndWind::GetWind(float x) {
    float f = Mod(x, 1.0f) * 1024.0f;
    int i = (int)f;
    return sWindField[i] + (sWindField[i + 1] - sWindField[i]) * (f - (float)(int)f);
}

float RndWind::GetWhiteNoise(float x) {
    float f = Mod(x, 1023.0f);
    int i = (int)f;
    return sWhiteField[i] + (sWhiteField[i + 1] - sWhiteField[i]) * (f - (float)(int)f);
}

void RndWind::SelfGetWind(const Vector3 &pos, float time, Vector3 &result) {
    result.x = GetWind(mTimeRate.x * time + mSpaceRate.x * pos.x + sOffset.x) * mRandom.x
        + mPrevailing.x;
    result.y = GetWind(mTimeRate.y * time + mSpaceRate.y * pos.y + sOffset.y) * mRandom.y
        + mPrevailing.y;
    result.z = GetWind(mSpaceRate.z * pos.z + mTimeRate.z * time + sOffset.z) * mRandom.z
        + mPrevailing.z;

    RndTransformable *trans = mTrans.Ptr();
    if ((int)trans) {
        const Transform &xfm = trans->WorldXfm();
        if (mAboutZ) {
            Vector3 zAxis(xfm.m.z);
            Vector3 diff(pos.x - xfm.v.x, pos.y - xfm.v.y, pos.z - xfm.v.z);
            float dot = -(diff.x * zAxis.x + diff.y * zAxis.y + diff.z * zAxis.z);
            Vector3 proj(diff.x + zAxis.x * dot, diff.y + zAxis.y * dot,
                diff.z + zAxis.z * dot);
            Vector3 cross(
                zAxis.y * proj.z - zAxis.z * proj.y,
                zAxis.z * proj.x - zAxis.x * proj.z,
                zAxis.x * proj.y - zAxis.y * proj.x);
            Normalize(cross, cross);
            float ry = result.y;
            float rz = result.z;
            float rx = result.x;
            result.y = rx * (cross.z * zAxis.x - zAxis.z * cross.x)
                + rz * zAxis.y + ry * cross.y;
            result.z = rz * zAxis.z
                + rx * (zAxis.y * cross.x - cross.y * zAxis.x) + ry * cross.z;
            result.x = rz * zAxis.x
                + ry * cross.x + rx * (cross.y * zAxis.z - cross.z * zAxis.y);
        } else {
            Multiply(result, xfm.m, result);
        }
    }

    float len = sqrtf(result.x * result.x + result.y * result.y + result.z * result.z);
    float limit;
    if (len > 0.0f) {
        if (len > mMaxSpeed) {
            limit = mMaxSpeed;
        } else if (len < mMinSpeed) {
            limit = mMinSpeed;
        } else {
            goto done;
        }
        float scale = limit / len;
        result.x *= scale;
        result.y *= scale;
        result.z *= scale;
    }
done:;
}

RndWind::RndWind()
    : mPrevailing(0.0f, 0.0f, 0.0f), mRandom(0.0f, 0.0f, 0.0f), mTimeLoop(100.0f),
      mSpaceLoop(gUnitsPerMeter * 10.0f), mTrans(this), mAboutZ(false), mMaxSpeed(1e30f),
      mMinSpeed(0.0f), mWindOwner(this, this) {
    SyncLoops();
}

RndWind::~RndWind() {}

bool RndWind::Replace(ObjRef *from, Hmx::Object *to) {
    if (&mWindOwner == from) {
        if (mWindOwner != this) {
            RndWind *wind = dynamic_cast<RndWind *>(to);
            if (wind) {
                mWindOwner = wind;
            }
        } else {
            mWindOwner = this;
        }
        return true;
    } else {
        return Hmx::Object::Replace(from, to);
    }
}


BEGIN_HANDLERS(RndWind)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_ACTION(set_defaults, SetDefaults())
    HANDLE_ACTION(set_zero, Zero())
END_HANDLERS

BEGIN_PROPSYNCS(RndWind)
    SYNC_PROP(prevailing, mPrevailing)
    SYNC_PROP(random, mRandom)
    SYNC_PROP(max_speed, mMaxSpeed)
    SYNC_PROP(min_speed, mMinSpeed)
    SYNC_PROP_SET(wind_owner, mWindOwner.Ptr(), SetWindOwner(_val.Obj<RndWind>()))
    SYNC_PROP_MODIFY(time_loop, mTimeLoop, SyncLoops())
    SYNC_PROP_MODIFY(space_loop, mSpaceLoop, SyncLoops())
    SYNC_PROP(trans, mTrans)
    SYNC_PROP(about_z, mAboutZ)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

BEGIN_SAVES(RndWind)
    SAVE_REVS(4, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    bs << mPrevailing;
    bs << mRandom;
    bs << mTimeLoop;
    bs << mSpaceLoop;
    bs << mWindOwner;
    bs << mTrans;
    bs << mAboutZ;
    bs << mMinSpeed;
    bs << mMaxSpeed;
END_SAVES

BEGIN_COPYS(RndWind)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(RndWind)
    BEGIN_COPYING_MEMBERS
        if (ty == kCopyShallow) {
            mWindOwner = c->mWindOwner.Ptr();
        } else {
            mWindOwner = this;
            mWindOwner = c->mWindOwner.Ptr();
            COPY_MEMBER(mPrevailing)
            COPY_MEMBER(mRandom)
            COPY_MEMBER(mTimeLoop)
            COPY_MEMBER(mSpaceLoop)
            COPY_MEMBER(mTrans)
            COPY_MEMBER(mAboutZ)
            COPY_MEMBER(mMinSpeed)
            COPY_MEMBER(mMaxSpeed)
            SyncLoops();
        }
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(4, 0)

BEGIN_LOADS(RndWind)
    LOAD_REVS(bs)
    ASSERT_REVS(4, 0)
    LOAD_SUPERCLASS(RndHighlightable)
    d >> mPrevailing;
    d >> mRandom;
    d >> mTimeLoop;
    d >> mSpaceLoop;
    if (d.rev > 1) {
        d >> mWindOwner;
        SetWindOwner(mWindOwner);
    }
    if (d.rev > 2) {
        d >> mTrans;
        d >> mAboutZ;
    }
    if (d.rev > 3) {
        d >> mMinSpeed;
        d >> mMaxSpeed;
    }
    SyncLoops();
END_LOADS

void RndWind::SyncLoops() {
    float f1;
    f1 = (mTimeLoop == 0.0f) ? 0.0f : (1.0f / mTimeLoop);
    mTimeRate.Set(f1, f1 * 0.773437f, f1 * 1.38484f);
    f1 = (mSpaceLoop == 0.0f) ? 0.0f : (1.0f / mSpaceLoop);
    mSpaceRate.Set(f1, f1 * 0.773437f, f1 * 1.38484f);
}

void RndWind::Zero() {
    mRandom.Set(0.0f, 0.0f, 0.0f);
    mPrevailing.Set(0.0f, 0.0f, 0.0f);
}

void RndWind::SetDefaults() {
    mPrevailing.Set(0.0f, 0.0f, 0.0f);
    mRandom.Set(17.0f, 17.0f, 0.0f);
    mTimeLoop = 100.0f;
    mSpaceLoop = gUnitsPerMeter * 10;
}

void RndWind::SetWindOwner(RndWind *wind) { mWindOwner = wind ? wind : this; }

void RndWind::Init() {
    REGISTER_OBJ_FACTORY(RndWind)
    sRand = new Rand(0x7FEF8A);
    SetWind(0, 0x400, 0.0f, 0.0f, 0.5f);
    sWindField[0x400] = sWindField[0];
    for (int i = 0; i < 0x400; i++) {
        sWhiteField[i] = RandomFloat(0.0f, 1.0f);
    }
    delete sRand;
    sRand = 0;
}
