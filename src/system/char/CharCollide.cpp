// Retail inlines the ObjPtr two-arg ctor at this TU's member-init sites; the
// in-class (plain inline) definition lets MSVC choose per site, as retail did.
#define RB3_OBJPTR_INLINE_TWOARG_CTOR
#include "char/CharCollide.h"
#include "CharCollide.h"
#include "math/Color.h"
#include "math/Mtx.h"
#include "obj/Object.h"
#include "rndobj/Trans.h"
#include "rndobj/Utl.h"

CharCollide::CharCollide()
    : mShape(kCollideSphere), mFlags(0), mMesh(this), mMeshYBias(false) {
    for (int i = 0; i < 2; i++) {
        mOrigLength[i] = 0;
        mOrigRadius[i] = 0;
    }
    CopyOriginalToCur();
    for (int i = 0; i < 8; i++) {
        unkStructs[i].vertIdx = 0;
        unkStructs[i].vec.Zero();
    }
    unk1a0.Reset();
}

CharCollide::~CharCollide() {}

BEGIN_HANDLERS(CharCollide)
    HANDLE_SUPERCLASS(RndTransformable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(CharCollide)
    SYNC_PROP_MODIFY(shape, (int &)mShape, SyncShape())
    SYNC_PROP(flags, mFlags)
    SYNC_PROP_MODIFY(radius0, mOrigRadius[0], SyncShape())
    SYNC_PROP_MODIFY(radius1, mOrigRadius[1], SyncShape())
    SYNC_PROP_MODIFY(length0, mOrigLength[0], SyncShape())
    SYNC_PROP_MODIFY(length1, mOrigLength[1], SyncShape())
    SYNC_PROP_MODIFY(mesh, mMesh, SyncShape())
    SYNC_PROP_MODIFY(mesh_y_bias, mMeshYBias, SyncShape())
    SYNC_SUPERCLASS(RndTransformable)
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

BEGIN_SAVES(CharCollide)
    SAVE_REVS(7, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(RndTransformable)
    bs << mShape;
    bs << mOrigRadius[0];
    bs << mOrigLength[0];
    bs << mOrigLength[1];
    bs << mFlags;
    bs << mCurRadius[0];
    bs << mOrigRadius[1];
    bs << mCurRadius[1];
    bs << mCurLength[0];
    bs << mCurLength[1];
    bs << unk1a0;
    bs << mMesh;
    for (int i = 0; i < 8; i++) {
        bs << unkStructs[i].vertIdx;
        bs << unkStructs[i].vec;
    }
    bs << mDigest;
    bs << mMeshYBias;
END_SAVES


BEGIN_COPYS(CharCollide)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndTransformable)
    CREATE_COPY(CharCollide)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mShape)
        COPY_MEMBER(mFlags)
        memcpy(mOrigRadius, c->mOrigRadius, 8);
        memcpy(mOrigLength, c->mOrigLength, 8);
        memcpy(mCurRadius, c->mCurRadius, 8);
        memcpy(mCurLength, c->mCurLength, 8);
        {
            void *src = (void *)&c->unk1a0;
            memcpy(&unk1a0, src, 0x40);
        }
        COPY_MEMBER(mMeshYBias)
        COPY_MEMBER(mMesh)
    END_COPYING_MEMBERS
END_COPYS

void CharCollide::Highlight() {
    Hmx::Color white(1, 1, 1, 1);
    Hmx::Color red(1, 0, 0, 1);
    unsigned int shape = mShape;
    if (shape >= 1) {
        if (shape >= 3) {
            if (shape < 5) {
                UtilDrawCigar(WorldXfm(), mOrigRadius, mOrigLength, red, 8);
                UtilDrawCigar(WorldXfm(), mCurRadius, mCurLength, white, 8);
            }
        } else {
            UtilDrawSphere(WorldXfm().v, mOrigRadius[0], red);
            UtilDrawSphere(WorldXfm().v, mCurRadius[0], white);
        }
    } else {
        Plane plane(WorldXfm().v, WorldXfm().m.x);
        UtilDrawPlane(plane, WorldXfm().v, red, 1, 12.0f, false);
    }
    if (mMesh) {
        int count;
        if (mShape == kCollideCigar || mShape == kCollideInsideCigar) {
            count = 2;
        } else if (mShape == kCollideSphere || mShape == kCollideInsideSphere) {
            count = 1;
        } else {
            count = 0;
        }
        int n = count << 2;
        if (n > 0) {
            CharCollideStruct *s = unkStructs;
            do {
                s++;
                Hmx::Color sphereColor(0, 0, 1, 1);
                UtilDrawSphere(
                    mMesh->Verts(s->vertIdx).pos,
                    0.1f, sphereColor
                );
                n--;
            } while (n != 0);
        }
    }
}

INIT_REVS(7, 0)

// Retail Load keeps no BinStreamRev: it splits the packed rev into one aligned
// file-scope aggregate (altRev +0, rev +4) and reads everything from the raw
// stream.
static struct {
    __declspec(align(4)) unsigned short altRev;
    __declspec(align(4)) unsigned short rev;
} gRevs_CharCollide;

BEGIN_LOADS(CharCollide)
    int rev;
    bs >> rev;
    gRevs_CharCollide.rev = getHmxRev(rev);
    gRevs_CharCollide.altRev = getAltRev(rev);
    Hmx::Object::Load(bs);
    RndTransformable::Load(bs);
    bs >> (int &)mShape;
    bs >> mOrigRadius[0];
    if (gRevs_CharCollide.rev > 4)
        bs >> mOrigLength[0];
    if (gRevs_CharCollide.rev > 2)
        bs >> mOrigLength[1];
    if (gRevs_CharCollide.rev > 1)
        bs >> mFlags;
    else
        mFlags = 0;
    if (gRevs_CharCollide.rev > 3)
        bs >> mCurRadius[0];
    else
        mCurRadius[0] = mOrigRadius[0];

    if (gRevs_CharCollide.rev > 5) {
        bs >> mOrigRadius[1];
        bs >> mCurRadius[1];
        bs >> mCurLength[0];
        bs >> mCurLength[1];
        bs >> unk1a0;
        bs >> mMesh;
        for (int i = 0; i < 8; i++) {
            bs >> unkStructs[i].vertIdx;
            bs >> unkStructs[i].vec;
        }
        bs >> mDigest;
        bs >> mMeshYBias;
        if (gRevs_CharCollide.rev < 7)
            CopyOriginalToCur();
    } else {
        mOrigRadius[1] = mOrigRadius[0];
        CopyOriginalToCur();
    }
END_LOADS

void CharCollide::SyncShape() {
    if (mCurLength[0] > mCurLength[1]) {
        mCurLength[0] = mCurLength[1];
    }
    CopyOriginalToCur();
}

void CharCollide::CopyOriginalToCur() {
    memcpy(mCurRadius, mOrigRadius, 8);
    memcpy(mCurLength, mOrigLength, 8);
}

int CharCollide::NumSpheres(Shape s) const {
    if (s == kCollideCigar || s == kCollideInsideCigar) {
        return 2;
    } else {
        return s == kCollideSphere || s == kCollideInsideSphere;
    }
}

// sw2 scatter-include (default/CharCollide <- midi/MidiParserMgr.cpp)
#define gRev gRev_MidiParserMgr
#define gAltRev gAltRev_MidiParserMgr
#include "midi/MidiParserMgr.cpp"
#undef gRev
#undef gAltRev
