// [NCCC f278] opt this unity TU into the inline owner-only ObjPtr ctor.
// Must precede the first #include of obj/Object.h (rndobj/ is PCH-excluded).
#define RB3_OBJPTR_INLINE_OWNER_CTOR 1
#define RB3_OBJPTR_INLINE_OWNER_CTOR_EH 1
#include "rndobj/MeshAnim.h"
#include "obj/Object.h"
#include "rndobj/Anim.h"

#pragma region Hmx::Object

RndMeshAnim::RndMeshAnim() : mMesh(this, nullptr), mKeysOwner(this, this) {}

// Replace and SetFrame are declared in the header but never decomped.
// On GCC, Replace is the key function — without it, the vtable ends up in .bss as zeros,
// which crashes at construction time due to null VTT entries.
void RndMeshAnim::Replace(ObjRef *ref, Hmx::Object *obj) {
    if (RefIs(ref, mKeysOwner)) {
        RndMeshAnim *ma;
        if (mKeysOwner == this || !(ma = dynamic_cast<RndMeshAnim *>(obj))) {
            mKeysOwner.SetObjConcrete(this);
        } else {
            mKeysOwner.SetObjConcrete(ma->mKeysOwner.Ptr());
        }
        return;
    }
    Hmx::Object::Replace(ref, obj);
}

struct GetVertPoint {
    static Vector3 &get(RndMesh::Vert *v) { return v->pos; }
};
struct GetVertNormal {
    static Vector3 &get(RndMesh::Vert *v) { return v->norm; }
};
struct GetVertTex {
    static Vector2 &get(RndMesh::Vert *v) { return v->tex; }
};
struct GetVertColor {
    static Hmx::Color &get(RndMesh::Vert *v) { return v->color; }
};

template <class T1, class T2>
void InterpVertData(
    const std::vector<T1> &a,
    const std::vector<T1> &b,
    float ref,
    RndMesh::VertVector &verts,
    float blend
) {
    MILO_ASSERT(a.size() == b.size(), 0x133);
    typename std::vector<T1>::const_iterator ait = a.begin();
    typename std::vector<T1>::const_iterator bit = b.begin();
    typename std::vector<T1>::const_iterator aend = a.end();
    RndMesh::Vert *vertit = verts.begin();
    if (a.size() > verts.size()) {
        aend -= (a.size() - verts.size());
    }
    if (ref == 0.0f) {
        if (blend != 1.0f) {
            for (; ait != aend; ++ait, vertit++) {
                Interp(T2::get(vertit), *ait, blend, T2::get(vertit));
            }
        } else {
            for (; ait != aend; ++ait, ++vertit) {
                T2::get(vertit) = *ait;
            }
        }
    } else if (ref == 1.0f) {
        if (blend != 1.0f) {
            for (; ait != aend; ++ait, ++bit, ++vertit) {
                Interp(T2::get(vertit), *bit, blend, T2::get(vertit));
            }
        } else {
            for (; ait != aend; ++ait, ++bit, ++vertit) {
                T2::get(vertit) = *bit;
            }
        }
    } else if (blend != 1.0f) {
        for (; ait != aend; ++ait, ++bit, ++vertit) {
            T1 tmp;
            Interp(*ait, *bit, ref, tmp);
            Interp(T2::get(vertit), tmp, blend, T2::get(vertit));
        }
    } else {
        for (; ait != aend; ++ait, ++bit, ++vertit) {
            Interp(*ait, *bit, ref, T2::get(vertit));
        }
    }
}

void RndMeshAnim::SetFrame(float frame, float blend) {
    RndAnimatable::SetFrame(frame, blend);
    if (mMesh) {
        if ((mMesh->Mutable() & 0x1F) == 0) {
            MILO_NOTIFY_ONCE("Mesh %s is animated but not mutable.\n", mMesh->Name());
        } else {
            int syncnum = 0;
            if (!VertPointsKeys().empty()) {
                const Key<std::vector<Vector3> > *prev;
                const Key<std::vector<Vector3> > *next;
                float ref = 0;
                VertPointsKeys().AtFrame(frame, prev, next, ref);
                InterpVertData<Vector3, GetVertPoint>(
                    prev->value, next->value, ref, mMesh->Verts(), blend
                );
                syncnum |= 0x1F;
            }
            if (!VertNormalsKeys().empty()) {
                const Key<std::vector<Vector3> > *prev;
                const Key<std::vector<Vector3> > *next;
                float ref = 0;
                VertNormalsKeys().AtFrame(frame, prev, next, ref);
                InterpVertData<Vector3, GetVertNormal>(
                    prev->value, next->value, ref, mMesh->Verts(), blend
                );
                syncnum |= 0x1F;
            }
            if (!VertTexsKeys().empty()) {
                const Key<std::vector<Vector2> > *prev;
                const Key<std::vector<Vector2> > *next;
                float ref = 0;
                VertTexsKeys().AtFrame(frame, prev, next, ref);
                InterpVertData<Vector2, GetVertTex>(
                    prev->value, next->value, ref, mMesh->Verts(), blend
                );
                syncnum |= 0x1F;
            }
            if (!VertColorsKeys().empty()) {
                const Key<std::vector<Hmx::Color> > *prev;
                const Key<std::vector<Hmx::Color> > *next;
                float ref = 0;
                VertColorsKeys().AtFrame(frame, prev, next, ref);
                InterpVertData<Hmx::Color, GetVertColor>(
                    prev->value, next->value, ref, mMesh->Verts(), blend
                );
                syncnum |= 0x1F;
            }
            if (syncnum != 0) {
                mMesh->Sync(syncnum);
            }
        }
    }
}

BEGIN_HANDLERS(RndMeshAnim)
    HANDLE_SUPERCLASS(RndAnimatable)
    HANDLE_EXPR(num_verts, NumVerts())
    HANDLE_ACTION(shrink_verts, ShrinkVerts(_msg->Int(2)))
    HANDLE_ACTION(shrink_keys, ShrinkKeys(_msg->Int(2)))
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(RndMeshAnim)
    SYNC_PROP(mesh, mMesh)
    SYNC_SUPERCLASS(RndAnimatable)
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

BEGIN_SAVES(RndMeshAnim)
    SAVE_REVS(2, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(RndAnimatable)
    bs << mMesh;
    bs << mVertPointsKeys;
    bs << mVertNormalsKeys;
    bs << mVertTexsKeys;
    bs << mVertColorsKeys;
    bs << mKeysOwner;
END_SAVES

BEGIN_COPYS(RndMeshAnim)
    CREATE_COPY_AS(RndMeshAnim, m)
    MILO_ASSERT(m, 0xD8);
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndAnimatable)
    COPY_MEMBER_FROM(m, mMesh)
    if (ty == kCopyShallow || (ty == kCopyFromMax && m->mKeysOwner != m)) {
        COPY_MEMBER_FROM(m, mKeysOwner)
    } else {
        MILO_ASSERT(m->mKeysOwner != this, 0xE5);
        mKeysOwner = this;
        mVertPointsKeys = m->mKeysOwner->mVertPointsKeys;
        mVertNormalsKeys = m->mKeysOwner->mVertNormalsKeys;
        mVertTexsKeys = m->mKeysOwner->mVertTexsKeys;
        mVertColorsKeys = m->mKeysOwner->mVertColorsKeys;
    }
END_COPYS

INIT_REVS(2, 0)

BEGIN_LOADS(RndMeshAnim)
    LOAD_REVS(bs)
    ASSERT_REVS(2, 0)
    if (d.rev > 0)
        LOAD_SUPERCLASS(Hmx::Object)
    LOAD_SUPERCLASS(RndAnimatable)
    d >> mMesh;
    d >> mVertPointsKeys;
    if (d.rev > 1)
        d >> mVertNormalsKeys;
    d >> mVertTexsKeys;
    d >> mVertColorsKeys;
    d >> mKeysOwner;
    if (!mKeysOwner)
        mKeysOwner = this;
END_LOADS

void RndMeshAnim::Print() {
    TheDebug << "   mesh: " << mMesh << "\n";
    TheDebug << "   keysOwner: " << mKeysOwner << "\n";
    TheDebug << "   vertPointsKeys: " << mVertPointsKeys << "\n";
    TheDebug << "   vertNormalsKeys: " << mVertNormalsKeys << "\n";
    TheDebug << "   vertTexsKeys: " << mVertTexsKeys << "\n";
    TheDebug << "   vertColorsKeys: " << mVertColorsKeys << "\n";
}

#pragma endregion
#pragma region RndAnimatable

float RndMeshAnim::EndFrame() {
    float end = VertPointsKeys().LastFrame();
    end = Max(end, VertNormalsKeys().LastFrame());
    end = Max(end, VertTexsKeys().LastFrame());
    end = Max(end, VertColorsKeys().LastFrame());
    return end;
}

#pragma endregion
#pragma region RndMeshAnim

int RndMeshAnim::NumVerts() {
    int num = 0;
    if (VertPointsKeys().size() != 0) {
        MaxEq<int>(num, VertPointsKeys()[0].value.size());
    }
    if (VertNormalsKeys().size() != 0) {
        MaxEq<int>(num, VertNormalsKeys()[0].value.size());
    }
    if (VertTexsKeys().size() != 0) {
        MaxEq<int>(num, VertTexsKeys()[0].value.size());
    }
    if (VertColorsKeys().size() != 0) {
        MaxEq<int>(num, VertColorsKeys()[0].value.size());
    }
    return num;
}

void RndMeshAnim::ShrinkVerts(int num) {
    for (Keys<std::vector<Vector3>, std::vector<RndMesh::Vert> >::iterator it =
             VertPointsKeys().begin();
         it != VertPointsKeys().end(); ++it) {
        it->value.resize(num);
    }
    for (Keys<std::vector<Vector3>, std::vector<RndMesh::Vert> >::iterator it =
             VertNormalsKeys().begin();
         it != VertNormalsKeys().end(); ++it) {
        it->value.resize(num);
    }
    for (Keys<std::vector<Vector2>, std::vector<RndMesh::Vert> >::iterator it =
             VertTexsKeys().begin();
         it != VertTexsKeys().end(); ++it) {
        it->value.resize(num);
    }
    for (Keys<std::vector<Hmx::Color>, std::vector<RndMesh::Vert> >::iterator it =
             VertColorsKeys().begin();
         it != VertColorsKeys().end(); ++it) {
        it->value.resize(num);
    }
}

void RndMeshAnim::ShrinkKeys(int num) {
    if (VertPointsKeys().size() != 0) {
        VertPointsKeys().resize(num);
    }
    if (VertNormalsKeys().size() != 0) {
        VertNormalsKeys().resize(num);
    }
    if (VertTexsKeys().size() != 0) {
        VertTexsKeys().resize(num);
    }
    if (VertColorsKeys().size() != 0) {
        VertColorsKeys().resize(num);
    }
}

// ---------------------------------------------------------------------------
// SCATTER TAIL -- X360 ONLY.
//
// Everything below this line exists to reproduce retail's COMDAT placement and
// emits OTHER TUs' bodies from this one. Natively it is not merely unnecessary,
// it is unbuildable and would be wrong even if it built:
//
//   * synth_xbox/Voice.cpp does not compile off-Xbox (XAUDIO2 send descriptors,
//     CreateThread) -- MEASURED, 2 errors under this build's exact flags.
//   * band3/game/NetGameMsgs.cpp is a game TU no rndobj target compiles.
//   * MultiMesh.cpp / Fur.cpp / ShaderMgr.cpp / mtx.cpp are ALREADY emitted by
//     other TUs in the native rndobj source set, so emitting them here as well
//     is a duplicate definition, not a gap.
//
// So the tail is guarded rather than the TU excluded. The difference matters:
// excluding MeshAnim.cpp lost RndMeshAnim ENTIRELY -- and rndobj/Rnd.cpp:313
// `RndMeshAnim::Init()` is an inline REGISTER_OBJ_FACTORY, so the moment
// anything calls Rnd::PreInit (X3 does) the link demands the ctor and typeinfo
// that the exclusion had removed. Guarding gives the native build the class and
// costs the X360 build nothing: it passes no /D, so HX_NATIVE is never defined
// there and the preprocessed token stream is byte-identical.
// ---------------------------------------------------------------------------
#ifndef HX_NATIVE

// RB3 retail linker interleaved MultiMesh.cpp / ShaderMgr.cpp / mtx.cpp COMDATs
// into this TU's .text span. Compile their bodies here so objdiff pairs them (bp2r).
// MultiMesh's INIT_REVS collides with MeshAnim's own gRev/gAltRev (both file-scope
// static const), so rename them for the include; they are compile-time literals
// referenced only inside MultiMesh's functions, so this is byte-neutral.
#define gRev gRev_MultiMesh
#define gAltRev gAltRev_MultiMesh
#include "rndobj/MultiMesh.cpp"
#undef gRev
#undef gAltRev

// laneW homing scan: RndFur's ctor/Save/Copy COMDATs were scattered by the retail
// linker into this TU's pinned .text span (0x8246BEF0, 0x8246C250, 0x8246C3B0).
// Must sit here, BEFORE ShaderMgr/NetGameMsgs/Voice: those pull in obj/ObjMacros.h,
// which redefines INIT_REVS to the 1-arg dialect and breaks Fur.cpp's INIT_REVS(3, 0).
#define gRev gRev_Fur
#define gAltRev gAltRev_Fur
#include "rndobj/Fur.cpp"
#undef gRev
#undef gAltRev
#include "rndobj/ShaderMgr.cpp"
#include "math/mtx.cpp"

// sw2 scatter-include (default/MeshAnim <- band3/game/NetGameMsgs.cpp)
#define gRev gRev_NetGameMsgs
#define gAltRev gAltRev_NetGameMsgs
#include "band3/game/NetGameMsgs.cpp"
#undef gRev
#undef gAltRev

// sw2 scatter-include (default/MeshAnim <- synth_xbox/Voice.cpp)
#define gRev gRev_Voice
#define gAltRev gAltRev_Voice
#include "synth_xbox/Voice.cpp"
#undef gRev
#undef gAltRev

#endif // !HX_NATIVE (scatter tail)

// See the specialization declaration + rationale comment in obj/ObjPtr_p.h
// (lane DR-2 census).  Retail's ObjRefConcrete<BandIKEffector, ObjectDir> dtor
// passes mOwner, not `this`, as the ring-ref to Release -- a single-instruction
// `replace` at 116 B / fuzzy 97.931.  This unity TU is the one whose pinned
// .text range retail placed the COMDAT in, and it already emits the primary
// template's copy of this instantiation (verified in the compiled obj's symbol
// table), so BandIKEffector's complete type is in reach here.  X360 only -- see
// PartAnim.cpp for the ODR rationale.
#ifndef HX_NATIVE
template <>
ObjRefConcrete<BandIKEffector, ObjectDir>::~ObjRefConcrete() {
    if (mObject)
        mObject->Release(reinterpret_cast<ObjRefOwner *>(mOwner));
}
#endif
