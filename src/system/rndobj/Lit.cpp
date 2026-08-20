// Retail INLINES the `ObjPtr<RndDrawable> drawPtr(this)` owner-only ctor in
// RndLight::Load (target: `lis r9,lbl_82049FAC` + three stores). Without this we
// bind the out-of-line two-arg body (`li r5,0; bl ??0?$ObjPtr@VRndDrawable@@`).
// Must precede every include -- the gate is read when ObjPtr is defined.
#define RB3_OBJPTR_INLINE_OWNER_CTOR

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
    bs << mAnimateColorFromPreset;
    bs << mAnimatePositionFromPreset;
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
    // NOTE (lane INSDEL-4): retail does NOT copy mTextureXfm here. Removing this
    // line closed the row (95.604 -> 100, +364 B). Evidence is ABSENCE measured
    // against a positive control INSIDE the same function: every other
    // COPY_MEMBER_FROM in this list emits a visible copy in retail (mColor ->
    // stw -172/-168/-164/-160(r31), mType -136, the three bools -132/-131/-130,
    // mRange/mFalloffStart -144/-140, mTopRadius/mBotRadius -16/-12, mTexture +
    // mCubeTexture via SetObjConcrete, mShadowObjects via the ObjPtrList
    // operator=, mProjectedBlend -> stw -8(r31)). So the mechanism is provably
    // visible per member, and mTextureXfm alone has NO trace anywhere in the
    // body -- retail goes straight from the mShadowObjects assign to
    // `lwz r11, 344(r30)` (mProjectedBlend). Note r31 == this + 352 in this
    // function, so `subi r3, r31, 80` is this->mTextureXfm (272 = 352 - 80).
    // Our surplus was an inlined 64-byte memcpy(this+272, l+272, 64) -- exactly
    // the 4 charged instructions, and the only ones in the function.
    // Almost certainly a DC3-newer addition: DC3 cannot adjudicate this (our
    // src/system is a verbatim DC3 copy), which is why it was settled on retail
    // bytes alone.
    COPY_MEMBER_FROM(l, mProjectedBlend)
    if (ty == kCopyShallow || (ty == kCopyFromMax && l->mColorOwner != l)) {
        COPY_MEMBER_FROM(l, mColorOwner)
    } else {
        mColorOwner = this;
        COPY_MEMBER_FROM(l, mColor)
    }
END_COPYS

// Retail 0x82498380 (136 B) -- byte-for-byte the RndMesh::Replace shape with
// mColorOwner in place of mGeomOwner. See RndMesh::Replace above.
void RndLight::Replace(ObjRef *ref, Hmx::Object *obj) {
    RndTransformable::Replace(ref, obj);
    if (static_cast<Hmx::Object *>(mColorOwner.Ptr())
        == reinterpret_cast<Hmx::Object *>(ref)) {
        RndLight *lit = dynamic_cast<RndLight *>(obj);
        if (lit) {
            mColorOwner.SetOwnerObj(lit->mColorOwner.Ptr());
        } else {
            mColorOwner.SetOwnerObj(this);
        }
    }
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
      // TWO-ARG spelling: retail leaves these two ObjPtr ctors OUT OF LINE.
      // RB3_OBJPTR_INLINE_OWNER_CTOR (needed for the ObjPtr<RndDrawable> site in
      // RndLight::Load) is TU-wide and would otherwise inline them here, taking
      // this ctor 100% -> 70.6% and two 44 B funclets 100% -> 0%.
      mAnimateRangeFromPreset(1), mShowing(1), mTexture(this, nullptr),
      mCubeTexture(this, nullptr),
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
    SYNC_PROP_SET(type, mType, SetLightType((Type)_val.Int()))
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
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

// RB3-360 retail rev dialect (rb3-Wii/ObjMacros shape), not DC3's Object.h
// BinStreamRev stack decorator.  DC3's form emits a ??0BinStream, a
// ??_7BinStreamRev@@6B@ vtable store and a ??1BinStream destructor that retail
// has none of, and dispatches each read on `&d` instead of the raw `bs`.
//
// Adjudicated for THIS unit on retail bytes: the target obj carries NO symbol
// mangled with AAVBinStreamRev@@, i.e. retail instantiated no rev-decorated
// operator>> here, so forwarding the raw stream deletes nothing.
//
// Written longhand rather than by including obj/ObjMacros.h: that header also
// swaps the SYNC_PROP and HANDLE families, which are already byte-exact here.
// No `#define gRev` alias -- several of these TUs are scatter-INCLUDED into
// another unit whose own gRev macro such an alias would silently shadow.
// The pair MUST share ONE internal-linkage aggregate (two file statics get two
// `lis` pairs), altRev FIRST (MSVC lays .bss out in REVERSE), and the padding
// MUST be an explicit member -- __declspec(align(4)) is unreliable here.
static struct {
    unsigned short altRev;
    unsigned short pad;
    unsigned short rev;
} gRevs_Lit;
BEGIN_LOADS(RndLight)
    int rev;
    bs >> rev;
    gRevs_Lit.rev = getHmxRev(rev);
    gRevs_Lit.altRev = getAltRev(rev);
    if (gRevs_Lit.rev > 3)
        Hmx::Object::Load(bs);
    RndTransformable::Load(bs);
    bs >> mColor;
    if (gRevs_Lit.rev < 2) {
        Hmx::Color col1, col2;
        bs >> col1 >> col2;
    }
    if (gRevs_Lit.rev < 3) {
        int i, j;
        bs >> i >> j;
    }
    bs >> mRange;
    if (gRevs_Lit.rev < 3) {
        int i, j, k;
        bs >> i >> j >> k;
    }
    if (gRevs_Lit.rev > 0) {
        int count;
        bs >> count;
        if (gRevs_Lit.rev < 0xE) {
            if (count > 1)
                count--;
        }
        mType = (Type)count;
    }
    if (gRevs_Lit.rev > 0xB) {
        bs >> mFalloffStart;
    }
    // ELSE-IF, not two statements. The `rev > 4 && rev < 5` arm is dead code in
    // RETAIL TOO (target: `cmplwi 4; ble` then `cmplwi 5; bge` around the body),
    // so the transcription is faithful -- but retail CHAINS it to the `rev > 5`
    // test, keeping the revision live in r11 and exiting the dead arm with
    // `b 0x1c0` straight past the rev>5 block. As two separate `if`s we reload
    // the revision at that point (`lhz r11,0x4,r27`) and mismatch.
    // The arm really is unreachable, so clang -Werror rejects it
    // (-Wtautological-overlap-compare); MSVC never sees the pragma. Suppressed
    // rather than restructured -- any rewrite that satisfies clang by changing
    // the comparisons also changes the X360 codegen that reaches 100%.
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wtautological-overlap-compare"
#endif
    if (gRevs_Lit.rev > 4 && gRevs_Lit.rev < 5) {
        bool tmp;
        bs >> tmp;
        mAnimateColorFromPreset = tmp;
        mAnimatePositionFromPreset = tmp;
    } else if (gRevs_Lit.rev > 5) {
        bs >> mAnimateColorFromPreset;
        bs >> mAnimatePositionFromPreset;
    }
#ifdef __clang__
#pragma clang diagnostic pop
#endif
    if (gRevs_Lit.rev > 6) {
        bs >> mTopRadius >> mBotRadius;
        if (gRevs_Lit.rev < 0xE) {
            int i, j;
            bs >> i >> j;
        }
    }
    if (gRevs_Lit.rev > 7) {
        bs >> mTexture;
        if (gRevs_Lit.rev == 9) {
            ObjPtrList<RndDrawable> drawList(this);
            bs >> drawList;
        } else if (gRevs_Lit.rev == 8) {
            ObjPtr<RndDrawable> drawPtr(this);
            bs >> drawPtr;
        }
    }
    if (gRevs_Lit.rev > 10) {
        bs >> mColorOwner;
        if (!mColorOwner)
            mColorOwner = this;
    }
    if (gRevs_Lit.rev > 0xC)
        bs >> mTextureXfm;
    if (gRevs_Lit.rev > 0xD) {
        bs >> mCubeTexture;
    }
    if (gRevs_Lit.rev > 0xE) {
        bs >> mShadowObjects;
        bs >> mProjectedBlend;
    }
    if (gRevs_Lit.rev > 0xF)
        bs >> mAnimateRangeFromPreset;
    else
        mAnimateRangeFromPreset = mAnimateColorFromPreset;
END_LOADS
