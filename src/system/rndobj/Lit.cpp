#include "rndobj/Lit.h"
#include "Lit.h"
#include "obj/Object.h"
#include "rndobj/Trans.h"
#include "utl/BinStream.h"

void RndLight::SetShadowOverride(ObjPtrList<RndDrawable> *l) { mShadowOverride = l; }

void RndLight::SetPackedColor(int packed, float scalar) {
    Hmx::Color col;
    col.Unpack(packed);
    Multiply(col, scalar, col);
    SetColor(col);
}

const char *RndLight::TypeToStr(Type t) {
    const char *lightTypes[] = { "Point", "Directional", "Projected", "ShadowRef" };
    MILO_ASSERT(t < DIM(lightTypes), 0x17A);
    return lightTypes[t];
}

void RndLight::Save(BinStream &bs) {
    bs << 0x10;
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(RndTransformable)
    bs << mColor << mRange << mType;
    bs << mFalloffStart;
    bs << mAnimateColorFromPreset << mAnimatePositionFromPreset;
    bs << mTopRadius << mBotRadius;
    bs << mTexture;
    bs << mColorOwner;
    bs << mTextureXfm;
    bs << mCubeTexture;
    bs << mShadowObjects;
    bs << mProjectedBlend;
    bs << mAnimateRangeFromPreset;
}

BEGIN_COPYS(RndLight)
    CREATE_COPY_AS(RndLight, l)
    MILO_ASSERT(l, 0xC4);
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndTransformable)
    COPY_MEMBER_FROM(l, mColor)
    COPY_MEMBER_FROM(l, mType)
    COPY_MEMBER_FROM(l, mAnimateColorFromPreset)
    COPY_MEMBER_FROM(l, mAnimatePositionFromPreset)
    COPY_MEMBER_FROM(l, mAnimateRangeFromPreset)
    if (ty != kCopyFromMax)
        COPY_MEMBER_FROM(l, mRange)
    COPY_MEMBER_FROM(l, mFalloffStart)
    COPY_MEMBER_FROM(l, mTopRadius)
    COPY_MEMBER_FROM(l, mBotRadius)
    COPY_MEMBER_FROM(l, mTexture)
    COPY_MEMBER_FROM(l, mCubeTexture)
    COPY_MEMBER_FROM(l, mShadowOverride)
    COPY_MEMBER_FROM(l, mShadowObjects)
    COPY_MEMBER_FROM(l, mTextureXfm)
    COPY_MEMBER_FROM(l, mProjectedBlend)
    if (ty == kCopyShallow || (ty == kCopyFromMax && l->mColorOwner != l)) {
        COPY_MEMBER_FROM(l, mColorOwner)
    } else {
        mColorOwner = this;
        COPY_MEMBER_FROM(l, mColor)
    }
END_COPYS

bool RndLight::Replace(ObjRef *ref, Hmx::Object *obj) {
    if (&mColorOwner == ref) {
        RndLight *lit = NULL;
        if (mColorOwner != this) {
            lit = dynamic_cast<RndLight *>(obj);
        }
        if (lit) {
            mColorOwner = lit->mColorOwner;
        } else {
            mColorOwner = this;
        }
        return true;
    }
    return RndTransformable::Replace(ref, obj);
}

Transform RndLight::Projection() {
    Transform result;
    if (mRange == 0.0f) {
        result.Reset();
    } else {
        Vector3 xRow = WorldXfm().m.x;

        const Transform &wz = WorldXfm();
        float nzy = -wz.m.z.y;
        float nzx = -wz.m.z.x;
        float nzz = -wz.m.z.z;

        Vector3 yRow = WorldXfm().m.y;

        Vector3 pos = WorldXfm().v;

        float topR = mTopRadius;
        float slope = (mBotRadius - topR) / mRange;

        result.m.x.y = nzx;
        result.m.y.z = yRow.y * slope;
        result.m.z.z = yRow.z * slope;
        result.m.x.z = yRow.x * slope;

        result.v.x = -(pos.x * xRow.x + pos.y * xRow.y + pos.z * xRow.z);
        result.v.y = -(pos.x * nzx + pos.y * nzy + pos.z * nzz);
        result.v.z = topR - (pos.x * yRow.x * slope + pos.y * yRow.y * slope + pos.z * yRow.z * slope);

        result.m.x.x = xRow.x;
        result.m.y.x = xRow.y;
        result.m.z.x = xRow.z;
        result.m.y.y = nzy;
        result.m.z.y = nzz;

        Multiply(result, mTextureXfm, result);

        static bool sInit;
        static Transform sBias;
        if (!sInit) {
            sInit = true;
            sBias.m.x.Set(0.5f, 0.0f, 0.0f);
            sBias.m.y.Set(0.0f, 0.5f, 0.0f);
            sBias.m.z.Set(0.5f, 0.5f, 1.0f);
            sBias.v.Set(0.0f, 0.0f, 0.0f);
        }
        Multiply(result, sBias, result);
    }
    return result;
}

BEGIN_HANDLERS(RndLight)
    HANDLE_ACTION(set_showing, SetShowing(_msg->Int(2)))
    HANDLE_SUPERCLASS(RndTransformable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

RndLight::RndLight()
    : mColor(1, 1, 1), mColorOwner(this, this), mRange(1000.0f), mFalloffStart(0),
      mType(kPoint), mAnimateColorFromPreset(1), mAnimatePositionFromPreset(1),
      mAnimateRangeFromPreset(1), mShowing(1), mTexture(this), mCubeTexture(this),
      mShadowOverride(nullptr), mShadowObjects(this, kObjListNoNull), mTopRadius(0),
      mBotRadius(30.0f), mProjectedBlend(0) {
    mTextureXfm.Reset();
}

int RndLight::PackedColor() const {
    Hmx::Color col;
    Multiply(GetColor(), 1.0f / Intensity(), col);
    return col.Pack();
}

float RndLight::Intensity() const {
    Hmx::Color col(GetColor());
    return Max(1.0f, Max(col.red, col.green, col.blue));
}

BEGIN_PROPSYNCS(RndLight)
    SYNC_PROP(animate_color_from_preset, mAnimateColorFromPreset)
    SYNC_PROP(animate_position_from_preset, mAnimatePositionFromPreset)
    SYNC_PROP(animate_range_from_preset, mAnimateRangeFromPreset)
    SYNC_PROP_SET(light_type, mType, SetLightType((Type)_val.Int()))
    SYNC_PROP_SET(range, mRange, SetRange(_val.Float()))
    SYNC_PROP_SET(falloff_start, mFalloffStart, SetFalloffStart(_val.Float()))
    SYNC_PROP_SET(color, PackedColor(), SetPackedColor(_val.Int(), Intensity()))
    SYNC_PROP_SET(intensity, Intensity(), SetPackedColor(PackedColor(), _val.Float()))
    SYNC_PROP_SET(topradius, mTopRadius, SetTopRadius(_val.Float()))
    SYNC_PROP_SET(botradius, mBotRadius, SetBotRadius(_val.Float()))
    SYNC_PROP(color_owner, mColorOwner)
    SYNC_PROP(texture, mTexture)
    SYNC_PROP(cube_texture, mCubeTexture)
    SYNC_PROP(texture_xfm, mTextureXfm)
    SYNC_PROP_SET(projected_blend, mProjectedBlend, SetProjectedBlend(_val.Int()))
    SYNC_PROP(shadow_objects, mShadowObjects)
    SYNC_SUPERCLASS(RndTransformable)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

INIT_REVS(0x10, 0)

BEGIN_LOADS(RndLight)
    LOAD_REVS(bs)
    ASSERT_REVS(0x10, 0)
    if (d.rev > 3)
        LOAD_SUPERCLASS(Hmx::Object)
    LOAD_SUPERCLASS(RndTransformable)
    bs >> mColor;
    if (d.rev < 2) {
        Hmx::Color col1, col2;
        bs >> col1 >> col2;
    }
    if (d.rev < 3) {
        int i, j;
        bs >> i >> j;
    }
    bs >> mRange;
    if (d.rev < 3) {
        int i, j, k;
        bs >> i >> j >> k;
    }
    if (d.rev > 0) {
        int count;
        bs >> count;
        if (d.rev < 0xE) {
            if (count > 1)
                count--;
        }
        mType = (Type)count;
    }
    if (d.rev > 0xB) {
        bs >> mFalloffStart;
    }
    if (d.rev > 4) {
        if (d.rev < 5) {
            bool tmp;
            d >> tmp;
            mAnimateColorFromPreset = tmp;
            mAnimatePositionFromPreset = tmp;
        }
    }
    if (d.rev > 5) {
        d >> mAnimateColorFromPreset;
        d >> mAnimatePositionFromPreset;
    }
    if (d.rev > 6) {
        bs >> mTopRadius >> mBotRadius;
        if (d.rev < 0xE) {
            int i, j;
            bs >> i >> j;
        }
    }
    if (d.rev > 7) {
        bs >> mTexture;
        if (d.rev == 9) {
            ObjPtrList<RndDrawable> drawList(this);
            bs >> drawList;
        } else if (d.rev == 8) {
            ObjPtr<RndDrawable> drawPtr(this);
            bs >> drawPtr;
        }
    }
    if (d.rev > 10) {
        bs >> mColorOwner;
        if (!mColorOwner)
            mColorOwner = this;
    }
    if (d.rev > 0xC)
        bs >> mTextureXfm;
    if (d.rev > 0xD) {
        bs >> mCubeTexture;
    }
    if (d.rev > 0xE) {
        bs >> mShadowObjects;
        bs >> mProjectedBlend;
    }
    if (d.rev > 0xF)
        d >> mAnimateRangeFromPreset;
    else
        mAnimateRangeFromPreset = mAnimateColorFromPreset;
END_LOADS
