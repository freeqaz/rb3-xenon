#include "char/CharMeshHide.h"
#include "obj/Object.h"
// Retail folds both rev words onto ONE base register at +0/+4, which only
// happens for internal-linkage align(4) file-scope storage laid out as a single
// aggregate (altRev at +0, rev at +4). Two separate statics do NOT fold.
static struct {
    __declspec(align(4)) unsigned short altRev;
    __declspec(align(4)) unsigned short rev;
} gRevs;
#define gAltRev gRevs.altRev
#define gRev gRevs.rev

#pragma region CharMeshHide::Hide

CharMeshHide::Hide::Hide(Hmx::Object *o) : mDraw(o), mFlags(0), mShow(0) {}

CharMeshHide::Hide::Hide(const CharMeshHide::Hide &hide)
    : mDraw(hide.mDraw), mFlags(hide.mFlags), mShow(hide.mShow) {}

CharMeshHide::Hide &CharMeshHide::Hide::operator=(const CharMeshHide::Hide &hide) {
    mDraw = hide.mDraw;
    mFlags = hide.mFlags;
    mShow = hide.mShow;
    return *this;
}

BEGIN_CUSTOM_PROPSYNC(CharMeshHide::Hide)
    SYNC_PROP(drawable, o.mDraw)
    SYNC_PROP(flags, o.mFlags)
    SYNC_PROP(show, o.mShow)
END_CUSTOM_PROPSYNC

BinStream &operator>>(BinStream &bs, CharMeshHide::Hide &hide) {
    bs >> hide.mDraw;
    bs >> hide.mFlags;
    bs >> hide.mShow;
    return bs;
}

BinStreamRev &operator>>(BinStreamRev &d, CharMeshHide::Hide &hide) {
    d >> hide.mDraw;
    d >> hide.mFlags;
    if (d.rev > 1) {
        d >> hide.mShow;
    }
    return d;
}

BinStream &operator<<(BinStream &bs, const CharMeshHide::Hide &hide) {
    bs << hide.mDraw;
    bs << hide.mFlags;
    bs << hide.mShow;
    return bs;
}

#pragma endregion CharMeshHide::Hide
#pragma region CharMeshHide

CharMeshHide::CharMeshHide() : mHides(this), mFlags(0) {}

CharMeshHide::~CharMeshHide() {}

BEGIN_PROPSYNCS(CharMeshHide)
    SYNC_PROP(flags, mFlags)
    SYNC_PROP(hides, mHides)
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

void CharMeshHide::Save(BinStream &bs) {
    int data[2];  // Array sized for stack alignment
    data[0] = 2;
    bs.WriteEndian(data, 4);
    Hmx::Object::Save(bs);
    data[0] = mFlags;
    bs.WriteEndian(data, 4);
    bs << mHides;
}

BEGIN_COPYS(CharMeshHide)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(CharMeshHide)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mFlags)
        COPY_MEMBER(mHides)
    END_COPYING_MEMBERS
END_COPYS

BEGIN_LOADS(CharMeshHide)
    // RB3-360 retail uses the rb3-Wii rev dialect: the packed rev int is split
    // into two mutable file-scope shorts and read back directly; no BinStreamRev
    // shim and no ASSERT_REVS block. Written longhand so the DC3 macros stay
    // intact for the scatter-included TUs further down this file.
    int rev;
    bs >> rev;
    gRev = getHmxRev(rev);
    gAltRev = getAltRev(rev);
    Hmx::Object::Load(bs);
    bs >> mFlags >> mHides;
END_LOADS
#undef gRev
#undef gAltRev

BEGIN_HANDLERS(CharMeshHide)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

void CharMeshHide::Init() { REGISTER_OBJ_FACTORY(CharMeshHide) }

#pragma endregion CharMeshHide

// sw2 scatter-include (default/CharMeshHide <- synth/Sfx.cpp)
#define gRev gRev_Sfx
#define gAltRev gAltRev_Sfx
#include "synth/Sfx.cpp"
#undef gRev
#undef gAltRev
