#include "rndobj/MeshAnim.h"
#include "obj/Object.h"
#include "rndobj/Anim.h"

#pragma region Hmx::Object

RndMeshAnim::RndMeshAnim() : mMesh(this), mKeysOwner(this, this) {}

// Replace and SetFrame are declared in the header but never decomped.
// On GCC, Replace is the key function — without it, the vtable ends up in .bss as zeros,
// which crashes at construction time due to null VTT entries.
bool RndMeshAnim::Replace(ObjRef *ref, Hmx::Object *obj) {
    if (&mKeysOwner == ref) {
        RndMeshAnim *ma;
        if (mKeysOwner == this || !(ma = dynamic_cast<RndMeshAnim *>(obj))) {
            mKeysOwner.SetObjConcrete(this);
        } else {
            mKeysOwner.SetObjConcrete(ma->mKeysOwner.Ptr());
        }
        return true;
    }
    return Hmx::Object::Replace(ref, obj);
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
    SYNC_SUPERCLASS(Hmx::Object)
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
        MaxEq<int>(num, VertPointsKeys().size());
    }
    if (VertNormalsKeys().size() != 0) {
        MaxEq<int>(num, VertNormalsKeys().size());
    }
    if (VertTexsKeys().size() != 0) {
        MaxEq<int>(num, VertTexsKeys().size());
    }
    if (VertColorsKeys().size() != 0) {
        MaxEq<int>(num, VertColorsKeys().size());
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
