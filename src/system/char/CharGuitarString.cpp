// Retail inlines the ObjPtr two-arg ctor at this TU's member-init sites; the
// in-class (plain inline) definition lets MSVC choose per site, as retail did.
#define RB3_OBJPTR_INLINE_TWOARG_CTOR
#include "char/CharGuitarString.h"
#include "math/Mtx.h"
#include "math/Utl.h"
#include "math/Vec.h"
#include "obj/Object.h"

CharGuitarString::CharGuitarString()
    : mOpen(false), mNut(this), mBridge(this), mBend(this), mTarget(this) {}

CharGuitarString::~CharGuitarString() {}

BEGIN_HANDLERS(CharGuitarString)
    HANDLE_ACTION(set_open, mOpen = _msg->Int(2))
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(CharGuitarString)
    SYNC_PROP(nut, mNut)
    SYNC_PROP(bridge, mBridge)
    SYNC_PROP(bend, mBend)
    SYNC_PROP(target, mTarget)
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

BEGIN_SAVES(CharGuitarString)
    SAVE_REVS(0, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    bs << mNut;
    bs << mBridge;
    bs << mBend;
    bs << mTarget;
END_SAVES

BEGIN_COPYS(CharGuitarString)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(CharGuitarString)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mTarget)
        COPY_MEMBER(mNut)
        COPY_MEMBER(mBridge)
        COPY_MEMBER(mBend)
    END_COPYING_MEMBERS
END_COPYS

// Retail Load keeps no BinStreamRev: it splits the packed rev into one aligned
// file-scope aggregate (altRev +0, rev +4) and reads everything from the raw
// stream.
static struct {
    __declspec(align(4)) unsigned short altRev;
    __declspec(align(4)) unsigned short rev;
} gRevs_CharGuitarString;

BEGIN_LOADS(CharGuitarString)
    int rev;
    bs >> rev;
    gRevs_CharGuitarString.rev = getHmxRev(rev);
    gRevs_CharGuitarString.altRev = getAltRev(rev);
    Hmx::Object::Load(bs);
    bs >> mNut;
    bs >> mBridge;
    bs >> mBend;
    bs >> mTarget;
END_LOADS

void CharGuitarString::Poll() {
    if (!mNut || !mBridge || !mBend || !mTarget)
        return;
    Transform tf50(mBend->WorldXfm());
    const Vector3 &nutvec = mNut->WorldXfm().v;
    const Vector3 &bridgevec = mBridge->WorldXfm().v;
    const Transform &tf4 = mTarget->WorldXfm();
    // w17-c: both differences spelled out at the call site in y, z, x order and
    // the denominator written as the image's (y + z) + x; with Subtract() the
    // load order could not follow (the w14-b note below measured parentheses
    // alone at 87.9).  Found by a 36x8 non-PCH probe sweep: this is the only
    // combination with zero diff rows.
    Vector3 tmp;
    tmp.y = tf4.v.y - nutvec.y;
    tmp.z = tf4.v.z - nutvec.z;
    tmp.x = tf4.v.x - nutvec.x;
    Vector3 tmp2;
    tmp2.y = bridgevec.y - nutvec.y;
    tmp2.z = bridgevec.z - nutvec.z;
    tmp2.x = bridgevec.x - nutvec.x;
    // w14-b: the numerator is the image's own association, (z + x) + y
    // (fmuls z, fmadds x, fmadds y); Dot(tmp, tmp2) sums x, z, y. MSVC honours
    // the parentheses, so this is the same float result the image computes.
    // 98.65 -> 99.96. Residual (12 rows): the denominator, image (y + z) + x,
    // ours (x + y) + z; spelling it with explicit parentheses in any order is
    // 87.9 (MSVC re-schedules every load), LengthSquared/flat sum inert.
    float clamped = Clamp(
        0.0f,
        1.0f,
        ((tmp.z * tmp2.z + tmp.x * tmp2.x) + tmp.y * tmp2.y)
            / ((tmp2.y * tmp2.y + tmp2.z * tmp2.z) + tmp2.x * tmp2.x)
    );
    if (mOpen)
        clamped = 0.0f;
    Interp(nutvec, bridgevec, clamped, tf50.v);
    mBend->SetWorldXfm(tf50);
}

void CharGuitarString::PollDeps(
    std::list<Hmx::Object *> &changedBy, std::list<Hmx::Object *> &change
) {
    changedBy.push_back(mNut);
    changedBy.push_back(mBridge);
    changedBy.push_back(mTarget);
    change.push_back(mBend);
}
