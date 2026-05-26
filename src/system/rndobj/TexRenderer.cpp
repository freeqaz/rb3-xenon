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
    bs << 13; // Major revision 13. No alternative revision.
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
    bs << mEnviron;
    bs << mClearBuffer;
    bs << mClearColor;
}

BEGIN_HANDLERS(RndTexRenderer)
    HANDLE_SUPERCLASS(RndAnimatable)
    HANDLE_SUPERCLASS(RndDrawable)
    HANDLE_SUPERCLASS(RndPollable)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_EXPR(get_render_textures, 3);
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
        COPY_MEMBER(mEnviron)
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
    SYNC_PROP(prime_draw, mPrimeDraw)
    SYNC_PROP_MODIFY(force_mips, mForceMips, InitTexture())
    SYNC_PROP_MODIFY(mirror_cam, mMirrorCam, mDirty = true)
    SYNC_PROP_MODIFY(environ, mEnviron, mDirty = true)
    SYNC_PROP(clear_buffer, mClearBuffer)
    SYNC_PROP(clear_color, mClearColor)
    SYNC_PROP(clear_alpha, mClearColor.alpha)
    SYNC_SUPERCLASS(RndAnimatable)
    SYNC_SUPERCLASS(RndDrawable)
    SYNC_SUPERCLASS(RndPollable)
    SYNC_SUPERCLASS(Hmx::Object)
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

INIT_REVS(13, 0)

BEGIN_LOADS(RndTexRenderer)
    LOAD_REVS(bs)
    ASSERT_REVS(13, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    if (2 < d.rev) {
        LOAD_SUPERCLASS(RndAnimatable)
        LOAD_SUPERCLASS(RndDrawable)
        if (d.rev > 10)
            LOAD_SUPERCLASS(RndPollable)
    }
    if (d.rev < 1) {
        FilePath fp;
        bs >> fp;
    } else {
        mDrawable.Load(bs, false, nullptr);
    }
    if (d.rev > 3) {
        bs >> mCamera;
    } else {
        mCamera = nullptr;
    }
    bs >> mOutputTexture;
    InitTexture();
    if (d.rev > 1) {
        d >> mForce;
        bs >> mImpostorHeight;
    }
    if (d.rev > 4) {
        d >> mDrawResponsible;
    } else {
        mDrawResponsible = true;
    }
    if (d.rev > 5) {
        d >> mDrawPreClear;
    } else {
        mDrawPreClear = false;
    }
    if (d.rev > 6) {
        d >> mDrawWorldOnly;
    }
    if (d.rev > 7) {
        d >> mPrimeDraw;
    }
    if (d.rev > 8) {
        d >> mForceMips;
    }
    if (d.rev > 9) {
        bs >> mMirrorCam;
    }
    if (d.rev > 10) {
        d >> mNoPoll;
    }
    if (d.rev > 11) {
        bs >> mEnviron;
    }
    if (d.rev > 12) {
        d >> mClearBuffer;
        bs >> mClearColor;
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
        RndEnvironTracker tracker((RndEnviron *)mEnviron, nullptr);
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
                mat4 = dynamic_cast<RndMat *>(it->RefOwner());
                if (mat4)
                    break;
            }
            if (mat4) {
                for (ObjRef::iterator it = mat4->Refs().begin();
                     it != mat4->Refs().end(); ++it) {
                    mesh5 = dynamic_cast<RndMesh *>(it->RefOwner());
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
        }
        cam->SetTargetTex(mOutputTexture);
        cam->Select();
        if (mClearBuffer)
            TheRnd.Clear(1, mClearColor);
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

RndTexRenderer::RndTexRenderer()
    : mImpostorHeight(0), mDirty(1), mForce(0), mDrawPreClear(1), mDrawWorldOnly(0),
      mDrawResponsible(1), mNoPoll(0), mOutputTexture(this), mDrawable(this),
      mCamera(this), mEnviron(this), mFirstDraw(1), mPrimeDraw(0), mForceMips(0),
      mMirrorCam(this), mClearBuffer(0), mClearColor(0, 0, 0) {}
