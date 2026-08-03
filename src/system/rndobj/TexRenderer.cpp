// RB3-360 retail RndTexRenderer::RndTexRenderer() inlines the owner-only
// ObjPtr ctor for mDrawable/mCamera/mMirrorCam (mOwner store, then a
// scheduler-interleaved vtable-lis / mObject / vtable-addi+store -- the
// RB3_TU_OBJPTR_OWNER_CTOR_DEFER_OBJECT shape) but keeps a real out-of-line
// call for mOutputTexture. This TU opts the majority (one-arg `mFoo(this)`,
// already how the ctor below is spelled) into the inline form via
// obj/Object.h's RB3_OBJPTR_INLINE_OWNER_CTOR gate; mOutputTexture opts back
// out with the explicit two-arg `mOutputTexture(this, nullptr)` spelling
// below (see the "PER-SITE" doc block on that gate).
#define RB3_OBJPTR_INLINE_OWNER_CTOR
#define RB3_TU_OBJPTR_OWNER_CTOR_DEFER_OBJECT
#include "rndobj/TexRenderer.h"
#include "math/Mtx.h"
#include "math/Utl.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "rndobj/Anim.h"
#include "rndobj/Cam.h"
#include "rndobj/Dir.h"
#include "rndobj/Draw.h"
#include "rndobj/Env.h"
#include "rndobj/Graph.h"
#include "rndobj/Mat.h"
#include "rndobj/Mesh.h"
#include "rndobj/Poll.h"
#include "rndobj/Rnd.h"
#include "rndobj/Utl.h"
#include "utl/FilePath.h"
#include <cmath>

float ComputeAngle(const Vector3 &center, const Vector3 &b, const Vector3 &c) {
    Vector3 v1, v2;
    Subtract(b, center, v1);
    Subtract(c, center, v2);
    Normalize(v1, v1);
    Normalize(v2, v2);
    float dot = Dot(v1, v2);
    return std::acos(Clamp(-1.0f, 1.0f, dot));
}

void RndTexRenderer::UpdatePreClearState() {
    TheRnd.PreClearDrawAddOrRemove(this, mDrawPreClear, 0);
    mDirty = 1;
}

void RndTexRenderer::InitTexture(void) {
    if (mForceMips && mOutputTexture) {
        mOutputTexture->SetBitmap(
            mOutputTexture->Width(),
            mOutputTexture->Height(),
            mOutputTexture->Bpp(),
            mOutputTexture->GetType(),
            true,
            nullptr
        );
    }
    mDirty = true;
}

float RndTexRenderer::StartFrame(void) {
    RndAnimatable *anim = dynamic_cast<RndAnimatable *>((RndDrawable *)mDrawable);
    if (anim != nullptr) {
        return anim->StartFrame();
    } else
        return 0.0f;
}

float RndTexRenderer::EndFrame(void) {
    RndAnimatable *anim = dynamic_cast<RndAnimatable *>((RndDrawable *)mDrawable);
    if (anim != nullptr) {
        return anim->EndFrame();
    } else
        return 0.0f;
}

void RndTexRenderer::SetFrame(float frame, float blend) {
    RndAnimatable *anim = dynamic_cast<RndAnimatable *>((RndDrawable *)mDrawable);
    if (anim != nullptr) {
        anim->SetFrame(frame, blend);
        mDirty = true;
    }
}

void RndTexRenderer::Save(BinStream &bs) {
    bs << 11; // Retail RB3 major revision 11 (DC3's rev 13 adds environ/clear).
    Hmx::Object::Save(bs);
    RndAnimatable::Save(bs);
    RndDrawable::Save(bs);
    RndPollable::Save(bs);
    bs << mDrawable;
    bs << mCamera;
    bs << mOutputTexture;
    bs << mForce;
    bs << mImpostorHeight;
    bs << mDrawResponsible;
    bs << mDrawPreClear;
    bs << mDrawWorldOnly;
    bs << mPrimeDraw;
    bs << mForceMips;
    bs << mMirrorCam;
    bs << mNoPoll;
}

DataNode RndTexRenderer::OnGetRenderTextures(DataArray *) {
    return GetRenderTextures(Dir());
}

BEGIN_HANDLERS(RndTexRenderer)
    HANDLE_SUPERCLASS(RndAnimatable)
    HANDLE_SUPERCLASS(RndDrawable)
    HANDLE_SUPERCLASS(RndPollable)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE(get_render_textures, OnGetRenderTextures)
END_HANDLERS

BEGIN_COPYS(RndTexRenderer)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndAnimatable)
    COPY_SUPERCLASS(RndDrawable)
    COPY_SUPERCLASS(RndPollable)
    CREATE_COPY(RndTexRenderer)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mDrawable)
        COPY_MEMBER(mCamera)
        COPY_MEMBER(mOutputTexture)
        COPY_MEMBER(mForce)
        COPY_MEMBER(mDrawWorldOnly)
        COPY_MEMBER(mDrawResponsible)
        COPY_MEMBER(mImpostorHeight)
        COPY_MEMBER(mDrawPreClear)
        COPY_MEMBER(mPrimeDraw)
        COPY_MEMBER(mForceMips)
        COPY_MEMBER(mMirrorCam)
        COPY_MEMBER(mNoPoll)
        InitTexture();
        mDirty = true;
    END_COPYING_MEMBERS
END_COPYS

BEGIN_PROPSYNCS(RndTexRenderer)
    SYNC_PROP_MODIFY(draw, mDrawable, mDirty = true; mFirstDraw = true)
    SYNC_PROP_MODIFY(cam, mCamera, mDirty = true)
    SYNC_PROP_MODIFY(output_texture, mOutputTexture, InitTexture())
    SYNC_PROP_MODIFY(force, mForce, mDirty = true)
    SYNC_PROP_MODIFY(imposter_height, mImpostorHeight, mDirty = true)
    SYNC_PROP_MODIFY(draw_pre_clear, mDrawPreClear, UpdatePreClearState())
    SYNC_PROP(draw_world_only, mDrawWorldOnly)
    SYNC_PROP(draw_responsible, mDrawResponsible)
    SYNC_PROP(no_poll, mNoPoll)
    SYNC_PROP_MODIFY(prime_draw, mPrimeDraw, mDirty = true)
    SYNC_PROP_MODIFY(force_mips, mForceMips, InitTexture())
    SYNC_PROP_MODIFY(mirror_cam, mMirrorCam, mDirty = true)
    SYNC_SUPERCLASS(RndAnimatable)
    SYNC_SUPERCLASS(RndDrawable)
    SYNC_SUPERCLASS(RndPollable)
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

void RndTexRenderer::ListAnimChildren(std::list<RndAnimatable *> &list) const {
    RndAnimatable *anim = dynamic_cast<RndAnimatable *>((RndDrawable *)mDrawable);
    if (anim != nullptr) {
        list.insert(list.end(), anim);
    }
}
void RndTexRenderer::ListDrawChildren(std::list<RndDrawable *> &list) {
    if (mDrawable != nullptr && mDrawResponsible) {
        list.insert(list.end(), mDrawable);
    }
}
void RndTexRenderer::ListPollChildren(std::list<RndPollable *> &list) const {
    if (mDrawable != nullptr && mNoPoll) {
        RndPollable *poll = dynamic_cast<RndPollable *>((RndDrawable *)mDrawable);
        if (poll != nullptr) {
            list.insert(list.end(), poll);
        }
    }
}

// RB3-360 retail rev dialect (rb3-Wii/ObjMacros shape): the packed rev is split
// into two HALFWORDS stored four bytes apart onto ONE internal-linkage align(4)
// base, and the RAW incoming BinStream is forwarded to every read and to the
// superclass Load.  DC3's Object.h BinStreamRev stack decorator additionally
// emits ??0BinStream, a ??_7BinStreamRev@@6B@ vtable store and a ??1BinStream
// destructor that retail has none of, and dispatches each read on `&d`.
//
// Written longhand rather than by including obj/ObjMacros.h: that header also
// swaps the SYNC_PROP and HANDLE families, which are already byte-exact here.
// The pair MUST share one aggregate -- two separate file statics are laid out
// independently and will not fold onto a single base register.  No `#define
// gRev` alias: several of these TUs are scatter-INCLUDED into another unit
// (e.g. rndobj/Anim.cpp includes rndobj/MotionBlur.cpp) whose own gRev macro
// the alias would silently shadow for the rest of the amalgamated TU.
static struct {
    __declspec(align(4)) unsigned short altRev;
    __declspec(align(4)) unsigned short rev;
} gRevs_TexRenderer;
BEGIN_LOADS(RndTexRenderer)
    int rev;
    bs >> rev;
    gRevs_TexRenderer.rev = getHmxRev(rev);
    gRevs_TexRenderer.altRev = getAltRev(rev);
    Hmx::Object::Load(bs);
    if (2 < gRevs_TexRenderer.rev) {
        RndAnimatable::Load(bs);
        RndDrawable::Load(bs);
        if (gRevs_TexRenderer.rev > 10)
            RndPollable::Load(bs);
    }
    if (gRevs_TexRenderer.rev < 1) {
        FilePath fp;
        bs >> fp;
    } else {
        mDrawable.Load(bs, false, nullptr);
    }
    if (gRevs_TexRenderer.rev > 3) {
        bs >> mCamera;
    } else {
        mCamera = nullptr;
    }
    bs >> mOutputTexture;
    InitTexture();
    if (gRevs_TexRenderer.rev > 1) {
        bs >> mForce;
        bs >> mImpostorHeight;
    }
    if (gRevs_TexRenderer.rev > 4) {
        bs >> mDrawResponsible;
    } else {
        mDrawResponsible = true;
    }
    if (gRevs_TexRenderer.rev > 5) {
        bs >> mDrawPreClear;
    } else {
        mDrawPreClear = false;
    }
    if (gRevs_TexRenderer.rev > 6) {
        bs >> mDrawWorldOnly;
    }
    if (gRevs_TexRenderer.rev > 7) {
        bs >> mPrimeDraw;
    }
    if (gRevs_TexRenderer.rev > 8) {
        bs >> mForceMips;
    }
    if (gRevs_TexRenderer.rev > 9) {
        bs >> mMirrorCam;
    }
    if (gRevs_TexRenderer.rev > 10) {
        bs >> mNoPoll;
    }
    mDirty = true;
END_LOADS

void RndTexRenderer::DrawToTexture() {
    if (TheRnd.GetDrawMode() != 0)
        return;
    if (((Hmx::Object *)Dir() == (Hmx::Object *)mDrawable) || !Showing())
        return;
    if (mDrawWorldOnly && !(TheRnd.ProcCmds() & kProcessWorld))
        return;
    if (mDirty && mDrawable && mOutputTexture) {
        if (!(mOutputTexture->GetType() & kProcessPost)) {
            MILO_NOTIFY_ONCE("%s not renderable", mOutputTexture->Name());
            return;
        }
        Transform tf98;
        float f33 = 0;
        if (!mForce) {
            static Message pre_render_msg("pre_render");
            HandleType(pre_render_msg);
        }
        RndCam *cam;
        RndDir *rdir = dynamic_cast<RndDir *>((RndDrawable *)mDrawable);
        if (mImpostorHeight != 0.0f && rdir) {
            cam = RndCam::Current();
            tf98 = cam->WorldXfm();
            f33 = cam->YFov();
            Transform tfc8;
            const Hmx::Matrix3 &rm = rdir->WorldXfm().m;
            tfc8.m.Set(
                rm.x.x, rm.y.x, rm.z.x, rm.x.y, rm.y.y, rm.z.y, rm.x.z, rm.y.z, rm.z.z
            );
            Multiply(cam->WorldXfm().m, tfc8.m, tfc8.m);
            Subtract(cam->WorldXfm().v, rdir->WorldXfm().v, tfc8.v);
            tfc8.v.z -= mImpostorHeight / 2.0f;
            float f34 = Max(
                Length(tfc8.v),
                mImpostorHeight / 2.0f + cam->NearPlane()
            );
            Multiply(Vector3(0, -f34, 0), tfc8.m, tfc8.v);
            tfc8.v.z += mImpostorHeight / 2.0f;
            cam->SetWorldXfm(tfc8);
            float atanned = atanf(mImpostorHeight / 2.0f / f34);
            cam->SetFrustum(
                cam->NearPlane(), cam->FarPlane(), atanned * 2.0f, 1.0f
            );
        } else {
            cam = mCamera;
            if (!cam)
                cam = mDrawable->CamOverride();
            if (rdir && !cam)
                cam = dynamic_cast<RndCam *>(rdir->CurCam());
            if (!cam)
                cam = TheRnd.GetDefaultCam();
            if (cam == TheRnd.GetDefaultCam()) {
                tf98 = cam->WorldXfm();
                if (rdir) {
                    cam->SetWorldXfm(rdir->CurViewport().mXfm);
                }
            }
        }
        RndCam *current = RndCam::Current();
        RndTex *targetTex = current->TargetTex();
        if (targetTex) {
            MILO_NOTIFY_ONCE(
                "%s: Cannot render to texture (%s) while already rendering to texture (%s).",
                PathName(targetTex),
                PathName(this),
                PathName(targetTex)
            );
        }
        RndMesh *mesh5 = nullptr;
        if (mMirrorCam) {
            RndMat *mat4 = nullptr;
            for (ObjRef::iterator it = mOutputTexture->Refs().begin();
                 it != mOutputTexture->Refs().end(); ++it) {
                mat4 = dynamic_cast<RndMat *>(RefPtrOf(it)->RefOwner());
                if (mat4)
                    break;
            }
            if (mat4) {
                for (ObjRef::iterator it = mat4->Refs().begin();
                     it != mat4->Refs().end(); ++it) {
                    mesh5 = dynamic_cast<RndMesh *>(RefPtrOf(it)->RefOwner());
                    if (mesh5)
                        break;
                }
            }
            if (!mesh5) {
                MILO_NOTIFY_ONCE(
                    "%s could not find mesh to mirror about. Is %s not being mapped onto a mesh?",
                    Name(),
                    mOutputTexture->Name()
                );
                return;
            }
            if (!mesh5->GetKeepMeshData()) {
                MILO_NOTIFY_ONCE(
                    "%s could not do mirroring because the mesh %s doesn't have its keep_mesh_data flag turned on. ",
                    Name(),
                    mesh5->Name()
                );
                return;
            }
            RndMesh::Face &curFace = mesh5->Faces(0);
            const Transform &meshXfm = mesh5->WorldXfm();
            RndMesh::Vert *verts[3] = {
                &mesh5->Verts(curFace.v1),
                &mesh5->Verts(curFace.v2),
                &mesh5->Verts(curFace.v3)
            };
            Vector3 vertVectors[3] = {
                verts[0]->pos,
                verts[1]->pos,
                verts[2]->pos
            };
            Multiply(vertVectors[0], meshXfm, vertVectors[0]);
            Multiply(vertVectors[1], meshXfm, vertVectors[1]);
            Multiply(vertVectors[2], meshXfm, vertVectors[2]);
            Vector3 v294;
            mesh5->SkinVertex(*verts[0], &v294);
            Normalize(v294, v294);
            Transform tf120;
            tf120.v = meshXfm.v;
            tf120.m.z = v294;
            Subtract(vertVectors[1], vertVectors[0], tf120.m.x);
            Normalize(tf120.m.x, tf120.m.x);
            Cross(tf120.m.z, tf120.m.x, tf120.m.y);
            Transform tf150;
            Invert(tf120, tf150);
            cam->SetWorldXfm(mMirrorCam->WorldXfm());
            Transform tf180;
            tf180.Reset();
            tf180.m.z.z = -1.0f;
            Multiply(tf150, tf180, tf180);
            Multiply(tf180, tf120, tf180);
            Multiply(mMirrorCam->WorldXfm(), tf180, cam->DirtyLocalXfm());
            Hmx::Matrix3 m1a8;
            Hmx::Matrix3 m1cc;
            for (int i = 0; i < 3; i++) {
                m1a8[i].Set(verts[i]->tex.x, verts[i]->tex.y, 1.0f);
                m1cc[i] = vertVectors[i];
            }
            Hmx::Matrix3 m1f0;
            Invert(m1a8, m1a8);
            Multiply(m1a8, m1cc, m1f0);
            Vector3 v2a0(0.5f, 0.0f, 1.0f);
            Vector3 v2ac(0.5f, 1.0f, 1.0f);
            Multiply(v2a0, m1f0, v2a0);
            Multiply(v2ac, m1f0, v2ac);
            float f28 = ComputeAngle(cam->WorldXfm().v, v2a0, v2ac);
            Transform tf220(cam->WorldXfm());
            Vector3 v2b8;
            Multiply(Vector3(0.5f, 0.5f, 1.0f), m1f0, v2b8);
            tf220.LookAt(v2b8, Vector3(0, 0, 1));
            cam->SetWorldXfm(tf220);
            Vector3 vecs248[3] = {
                Vector3(0, 0, 1),
                Vector3(0, 1, 1),
                Vector3(1, 0, 1)
            };
            for (int i = 0; i < 3; i++) {
                Multiply(vecs248[i], m1f0, vecs248[i]);
            }
            Vector3 v2c4;
            Subtract(vecs248[2], vecs248[0], v2c4);
            Vector3 va0;
            Subtract(vecs248[1], vecs248[0], va0);
            cam->SetFrustum(
                cam->NearPlane(),
                cam->FarPlane(),
                f28,
                Length(va0) / Length(v2c4)
            );
#if defined(MILO_DEBUG) && defined(HX_NATIVE)
            // "rndtex.debug_mirror" occurs 0 times in retail band.exe -- this whole
            // RndGraph debug-sphere overlay is dev-build only.
            if (DataVariable("rndtex.debug_mirror").Int()) {
                RndGraph *graph = RndGraph::GetOneFrame();
                Vector3 vecs278[4] = {
                    Vector3(0, 0, 1),
                    Vector3(0, 1, 1),
                    Vector3(1, 0, 1),
                    Vector3(1, 1, 1)
                };
                for (int i = 0; i < 4; i++) {
                    Multiply(vecs278[i], m1f0, vecs278[i]);
                    graph->AddSphere(vecs278[i], 2.0f, Hmx::Color(1, 1, 1));
                }
                graph->AddSphere(v2a0, 1.0f, Hmx::Color(0, 0, 1));
                graph->AddSphere(v2ac, 1.0f, Hmx::Color(0, 0, 1));
                for (int i = 0; i < 3; i++) {
                    graph->AddSphere(vertVectors[i], 1.0f, Hmx::Color(1, 0, 0));
                }
            }
#endif
        }
        cam->SetTargetTex(mOutputTexture);
        cam->Select();
        int cap = (mFirstDraw && mPrimeDraw) ? 2 : 1;
        if (cap > 0) {
            int j = cap;
            do {
                DrawBefore();
                if (rdir && rdir->ClassName() == "WorldDir") {
                    rdir->RndDir::DrawShowing();
                } else
                    mDrawable->DrawShowing();
                DrawAfter();
                j--;
            } while (j != 0);
        }
        cam->SetTargetTex(nullptr);
        if (!mMirrorCam) {
            if (mImpostorHeight != 0.0f) {
                cam->SetWorldXfm(tf98);
                cam->SetFrustum(cam->NearPlane(), cam->FarPlane(), f33, 1.0f);
            } else if (cam == TheRnd.GetDefaultCam()) {
                cam->SetWorldXfm(tf98);
            }
        }
        current->Select();
        mFirstDraw = false;
        if (!mForce) {
            static Message post_render_msg("post_render");
            HandleType(post_render_msg);
        }
    }
    if (!mForce)
        mDirty = false;
}

void RndTexRenderer::DrawShowing() {
    if (!mDrawPreClear)
        DrawToTexture();
}

// ORDER-FORCING SPELLING -- `mFirstDraw = 1` rides inside mMirrorCam's
// initializer on purpose; it is NOT ordinary logic. Retail emits the three
// bool stores as 0x6c(mPrimeDraw), 0x6e(mForceMips), 0x6d(mFirstDraw), i.e.
// mFirstDraw LAST. Measured facts behind that (all four legs objdiff'd):
//   * The scheduler keeps the IR-last store of the mem-init group above
//     mMirrorCam's unwind `stw r11,0x54(r31)` and sinks the rest below it,
//     preserving their relative order. Verified on 4 different member sets.
//   * cl 10224 canonicalizes a mem-init list to DECLARATION order: spelling
//     the list as mFirstDraw,mPrimeDraw,mForceMips (DC3's own order, which
//     would have reproduced retail's bytes had written order been honored)
//     is byte-identical. So no mem-init permutation can put 0x6d last.
//   * A ctor-BODY `mFirstDraw = 1;` sinks past mMirrorCam's stores to the
//     function tail -- the scheduler will not hoist it back across them.
// Member->offset is pinned by RETAIL bytes, not header comments: InitTexture
// reads 0x6e (mForceMips), Save reads 0x6c then 0x6e (mPrimeDraw,mForceMips),
// DrawToTexture reads 0x6d then 0x6c and later stores 0x6d (mFirstDraw).
// Semantics are unchanged: mFirstDraw is still set to 1 exactly once, before
// any ctor body runs. Reverting to `mFirstDraw(1)` in the list costs the
// match (99.98529%) but is otherwise equivalent.
RndTexRenderer::RndTexRenderer()
    : mDirty(1), mForce(0), mDrawPreClear(1), mDrawWorldOnly(0), mDrawResponsible(1),
      mNoPoll(0), mPrimeDraw(0), mForceMips(0), mImpostorHeight(0),
      mOutputTexture(this, nullptr), mDrawable(this), mCamera(this),
      mMirrorCam((mFirstDraw = 1, this)) {}

// sw2 scatter-include (default/TexRenderer <- math/mtx.cpp)
#define gRev gRev_mtx
#define gAltRev gAltRev_mtx
#include "math/mtx.cpp"
#undef gRev
#undef gAltRev
