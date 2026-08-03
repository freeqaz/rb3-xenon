#include "rndobj/SoftParticles.h"
#include "Rnd_NG.h"
#include "obj/Object.h"
#include "rndobj/BaseMaterial.h"
#include "rndobj/Draw.h"
#include "rndobj/SoftParticleBuffer.h"

// RETAIL-MATCH (lane DI-1, 2026-08-02): retail RB3's RndSoftParticles::Load
// stores the load revisions as two align(4) `unsigned short` GLOBALS -- the
// target emits `sth` into lbl_82CC6A68 and lbl_82CC6A68+4 -- and passes `bs`
// straight through to the superclass Load (`mr r4, r30`).
//
// The DC3-derived obj/Object.h dialect instead expands LOAD_REVS to a stack
// `BinStreamRev d(bs, revs)` temp: a ctor/dtor pair, `stw` of rev/altRev as
// ints, a vtable store, and +0x30 of stack frame (target frame 0x70, ours
// 0xa0).  This TU pays all of that for nothing -- its Load body reads through
// `bs` and never touches `d` (only the Object.h LOAD_SUPERCLASS does, via
// `d.stream`).  There are two competing LOAD_REVS definitions in the tree
// (obj/ObjMacros.h:614 = the rb3-Wii gRev/gAltRev dialect retail used,
// obj/Object.h:1611 = this DC3 one) and which a TU gets is decided purely by
// include order -- the same macro-competition hazard already documented for
// SYNC_PROP and OBJ_SET_TYPE.  Override per-TU rather than tree-wide: the
// blast radius of flipping the dialect globally is every engine Load, and it
// has not been priced.
//
// SAVE_REVS is unaffected -- it expands to `packRevs(alt, rev)` on literals
// and never reads gRev, so the already-matching Save is untouched.
#undef INIT_REVS
#define INIT_REVS(rev, alt)                                                              \
    static __declspec(align(4)) unsigned short gRev;                                     \
    static __declspec(align(4)) unsigned short gAltRev;
#undef LOAD_REVS
// Residue (lane DI-1): 4 instructions of pure register naming.  Retail copies
// the original word aside and shifts IN PLACE (`mr r10,r11; srwi r11,r11,16`);
// we shift into a fresh register (`srwi r10,r11,16`) and so emit ONE FEWER
// instruction than retail (160 B vs 164 B).  Closing it means making codegen
// strictly worse, i.e. it is register-allocation naming = permuter class, and
// the permuter is OFF by standing directive.  Writing the shift back into
// `revs` was tried and is WORSE: it forces a `stw r11, 0x50, r1` spill because
// MSVC keeps the stream-target's stack slot coherent.
#define LOAD_REVS(bs)                                                                    \
    int revs;                                                                            \
    bs >> revs;                                                                          \
    gRev = (unsigned short)((unsigned int)revs >> 16);                                   \
    gAltRev = (unsigned short)revs;
#undef LOAD_SUPERCLASS
#define LOAD_SUPERCLASS(parent) parent::Load(bs);

RndSoftParticles::RndSoftParticles()
    : mParticles(this), mBlend(BaseMaterial::kBlendSrcAlphaAdd) {}

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

INIT_REVS(1, 0)

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
