#include "rndobj/SoftParticles.h"
#include "Rnd_NG.h"
#include "obj/Object.h"
#include "rndobj/BaseMaterial.h"
#include "rndobj/Draw.h"
#include "rndobj/SoftParticleBuffer.h"

// ---------------------------------------------------------------------------
// Retail RB3 uses the obj/ObjMacros.h rev dialect (file-scope statics gRev /
// gAltRev written at load time), not obj/Object.h's local BinStreamRev wrapper.
// Proven from retail asm for ?Load@RndSoftParticles@@ (target 164 B): after
// `bl BinStream::ReadEndian` it does `mr r10,r11 ; srwi r11,r11,16` and two
// `sth`s into a static pair, then passes the ORIGINAL bs to both
// Hmx::Object::Load and RndDrawable::Load.  No BinStream ctor/dtor pair and no
// ??_7BinStreamRev@@6B@ vtable store.  gAltRev is declared first because retail
// puts it at the base address and gRev at +4.  See ui/UILabel.cpp for the same
// treatment.  Bracketed with push_macro/pop_macro so the dialect cannot leak.
// ---------------------------------------------------------------------------
#pragma push_macro("INIT_REVS")
#pragma push_macro("LOAD_REVS")
#pragma push_macro("ASSERT_REVS")
#pragma push_macro("LOAD_SUPERCLASS")
#undef INIT_REVS
#undef LOAD_REVS
#undef ASSERT_REVS
#undef LOAD_SUPERCLASS
#define INIT_REVS(objType)                                                               \
    static unsigned short gAltRev = 0;                                                   \
    static unsigned short gRev = 0;
#define LOAD_REVS(bs)                                                                    \
    int rev;                                                                             \
    bs >> rev;                                                                           \
    gRev = getHmxRev(rev);                                                               \
    gAltRev = getAltRev(rev);
#define ASSERT_REVS(rev1, rev2)
#define LOAD_SUPERCLASS(parent) parent::Load(bs);

RndSoftParticles::RndSoftParticles()
    : mParticles(this), mBlend(RndMat::kBlendSrcAlphaAdd) {}

BEGIN_HANDLERS(RndSoftParticles)
    HANDLE_SUPERCLASS(RndDrawable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(RndSoftParticles)
    SYNC_PROP(particles, mParticles)
    SYNC_PROP(blend_mode, (int &)mBlend)
    SYNC_SUPERCLASS(RndDrawable)
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

BEGIN_SAVES(RndSoftParticles)
    SAVE_REVS(1, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(RndDrawable)
    bs << mParticles;
    bs << mBlend;
END_SAVES

BEGIN_COPYS(RndSoftParticles)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndDrawable)
    CREATE_COPY(RndSoftParticles)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mParticles)
        COPY_MEMBER(mBlend)
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(RndSoftParticles)

BEGIN_LOADS(RndSoftParticles)
    LOAD_REVS(bs)
    ASSERT_REVS(1, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    LOAD_SUPERCLASS(RndDrawable)
    bs >> mParticles;
    bs >> (int &)mBlend;
END_LOADS

void RndSoftParticles::DrawShowing() {
    RndSoftParticleBuffer *buffer = TheNgRnd.ParticleBuffer();
    if (buffer) {
        FOREACH (it, mParticles) {
            buffer->Queue(*it, mBlend);
        }
    }
}

void RndSoftParticles::ListDrawChildren(std::list<RndDrawable *> &draws) {
    FOREACH (it, mParticles) {
        draws.push_back(*it);
    }
}

#pragma pop_macro("LOAD_SUPERCLASS")
#pragma pop_macro("ASSERT_REVS")
#pragma pop_macro("LOAD_REVS")
#pragma pop_macro("INIT_REVS")
