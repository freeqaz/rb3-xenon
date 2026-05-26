#include "gesture/DepthBuffer3D.h"
#include "gesture/BaseSkeleton.h"
#include "gesture/GestureMgr.h"
#include "gesture/JointUtl.h"
#include "hamobj/HamGameData.h"
#include "hamobj/RhythmDetector.h"
#include "math/Mtx.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "rnddx9/Rnd.h"
#include "rndobj/Draw.h"
#include "rndobj/Rnd_NG.h"
#include "rndobj/Tex.h"
#include "rndobj/Trans.h"

LargeQuadRenderData DepthBuffer3D::mQuad;

namespace {
    void JointToVertexData(
        Vector3 &out, const Skeleton &skeleton, SkeletonJoint joint, const Vector4 &bounds
    ) {
        Vector3 screenPos;
        JointScreenPos(skeleton.TrackedJoints()[joint], screenPos);
        out.y = screenPos.z;
        out.x = ((screenPos.x - bounds.x) / (bounds.z - bounds.x) - 0.5f) * 318.0f - 1.0f;
        out.z = (0.5f - (screenPos.y - bounds.y) / (bounds.w - bounds.y)) * 238.0f - 1.0f;
    }

    void VertexToWorld(
        Vector3 &pos, const Transform &xfm, float stretchNearCamera, const Vector4 &depthRange
    ) {
        float depth = (pos.y - 256.0f) * (1.0f / 4096.0f);
        pos.y = depth;
        depth = 1.0f - (depth - depthRange.x) / (depthRange.y - depthRange.x);
        pos.y = depth;
        depth = Clamp(0.0f, 1.0f, depth);
        pos.y = depth;
        float y = (float)pow((double)depth, (double)stretchNearCamera) * -200.0f;
        pos.y = y;
        float x = pos.x;
        float z = pos.z;
        pos.x = xfm.m.x.x * x + xfm.m.y.x * y + xfm.m.z.x * z;
        pos.y = xfm.m.x.y * x + xfm.m.y.y * y + xfm.m.z.y * z;
        pos.z = xfm.m.x.z * x + xfm.m.y.z * y + xfm.m.z.z * z;
    }

    RndMat *SetUpWorkingMat();
}

DepthBuffer3D::DepthBuffer3D()
    : mDrawSheet(0), mDrawPlayer1(1), mDrawPlayer2(1), mDrawNonPlayers(1),
      mDebugLayout(0), mNobodyColor(0, 0, 0, 0), mPlayerPalette(this), mBoxymanPalette(this),
      mBoxymanPaletteAnim(1), mPlayerPaletteOffset(0), mPlayerPaletteScale(1), mMinimalMat(this),
      mMesh(this), mStretchNearCamera(1), mOpacity(1), mPlayer1Grooviness(0), mPlayer2Grooviness(0),
      mForceDrawSkeletonIdx(0xfffffc19), mForceDrawEnabled(1), mPlayerPaletteTex(this), mTile(1.5, 1.5), mScaleVoxel(1),
      mScaleVoxelGap(1), mFishEyeX(0), mFishEyeY(0), mGroovinessDetector1(this), mGroovinessDetector2(this),
      unk20c(80, 4, 4), unk220(40, 4, 4), unk234(60, 3, 3), unk248(30, 3, 3),
      unk25c(2048, 204.8f, 204.8f), unk270(4096, 204.8f, 204.8f), mMaxZoom(1),
      mMaxDepthZoom(1), unk28c(0) {}

DepthBuffer3D::~DepthBuffer3D() {}

BEGIN_HANDLERS(DepthBuffer3D)
    HANDLE_SUPERCLASS(RndDrawable)
    HANDLE_SUPERCLASS(RndTransformable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(DepthBuffer3D)
    SYNC_PROP(nobody_color, mNobodyColor)
    SYNC_PROP(nobody_alpha, mNobodyColor.alpha)
    SYNC_PROP_SET(
        player_palette, mPlayerPalette.Ptr(), SetPlayerPalette(_val.Obj<RndTex>())
    )
    SYNC_PROP(player_palette_offset, mPlayerPaletteOffset)
    SYNC_PROP(player_palette_scale, mPlayerPaletteScale)
    SYNC_PROP(minimal_mat, mMinimalMat)
    SYNC_PROP(draw_sheet, mDrawSheet)
    SYNC_PROP(mesh, mMesh)
    SYNC_PROP(stretch_near_camera, mStretchNearCamera)
    SYNC_PROP(opacity, mOpacity)
    SYNC_PROP(draw_player_1, mDrawPlayer1)
    SYNC_PROP(draw_player_2, mDrawPlayer2)
    SYNC_PROP(draw_non_players, mDrawNonPlayers)
    SYNC_PROP(tile_x, mTile.x)
    SYNC_PROP(tile_y, mTile.y)
    SYNC_PROP(scale_voxel, mScaleVoxel)
    SYNC_PROP(scale_voxelgap, mScaleVoxelGap)
    SYNC_PROP(fisheye_x, mFishEyeX)
    SYNC_PROP(fisheye_y, mFishEyeY)
    SYNC_PROP(max_zoom, mMaxZoom)
    SYNC_PROP(max_depth_zoom, mMaxDepthZoom)
    SYNC_PROP(debug_layout, mDebugLayout)
    SYNC_SUPERCLASS(RndDrawable)
    SYNC_SUPERCLASS(RndTransformable)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

void DepthBuffer3D::Init() {
    REGISTER_OBJ_FACTORY(DepthBuffer3D);
    TheNgRnd.CreateLargeQuad(0x140, 0xF0, mQuad);
}

void DepthBuffer3D::UpdateAttachment(
    DepthBuffer3DAttachment &attachment, const Vector4 &v1, const Vector4 &v2
) {
    bool b5 = false;
    Vector3 newPos;
    int skelIdx = TheGestureMgr->GetSkeletonIndexByTrackingID(
        TheGameData->Player(attachment.player)->GetSkeletonTrackingID()
    );
    if (skelIdx + 1 > 0) {
        Skeleton &skeleton = TheGestureMgr->GetSkeleton(skelIdx);
        Vector3 localPos = LocalXfm().v;
        Vector3 pos;
        JointToVertexData(pos, skeleton, (SkeletonJoint)attachment.mJoint, v1);
        VertexToWorld(pos, LocalXfm(), mStretchNearCamera, v2);
        Add(pos, localPos, newPos);
        attachment.obj->SetTransConstraint(mConstraint, nullptr, false);
        Normalize(mWorldXfm.m, attachment.obj->DirtyLocalXfm().m);
        b5 = true;
    }
    if (!b5) {
        newPos.Set(100000, 100000, 100000);
    }
    attachment.obj->SetLocalPos(newPos);
}

void DepthBuffer3D::AddAttachment(const DepthBuffer3DAttachment &attachment) {
    MILO_ASSERT(attachment.obj, 0x390);
    std::vector<DepthBuffer3DAttachment>::iterator it;
    for (it = mAttachments.begin(); it != mAttachments.end(); ++it) {
        if (it->obj == attachment.obj) {
            break;
        }
    }
    if (it == mAttachments.end()) {
        mAttachments.resize(mAttachments.size() + 1);
        DepthBuffer3DAttachment &back = mAttachments[mAttachments.size() - 1];
        back = attachment;
        back.unk20 = (int)back.obj->TransParent();
        back.obj->SetTransParent(mParent, false);
        back.obj->SetTransConstraint(mConstraint, nullptr, false);
    }
}

void DepthBuffer3D::SetPlayerPalette(RndTex *tex) {
    if (tex && mPlayerPalette != tex) {
        if (mBoxymanPaletteAnim != 1) {
            MILO_WARN_ONCE("dropping boxyman palette animation %f\n", mBoxymanPaletteAnim);
        }
        mBoxymanPaletteAnim = 0;
        if (mBoxymanPalette) {
            mBoxymanPalette = mPlayerPalette;
        }
        mPlayerPalette = tex;
    }
}

void DepthBuffer3D::SetGrooviness(float f1) {
    mPlayer1Grooviness = f1;
    mPlayer2Grooviness = f1;
    mGroovinessDetector1 = nullptr;
    mGroovinessDetector2 = nullptr;
}

void DepthBuffer3D::SetGrooviness(RhythmDetector *r1, RhythmDetector *r2) {
    mGroovinessDetector1 = r1;
    mGroovinessDetector2 = r2;
}

void DepthBuffer3D::ForceDrawSkeletonIndex(int i1, bool b2) {
    mForceDrawSkeletonIdx = i1;
    mForceDrawEnabled = b2;
}

void DepthBuffer3D::ListDrawChildren(std::list<RndDrawable *> &out) {
    if (mMesh.Ptr() != nullptr) {
        out.push_back(mMesh.Ptr());
    }
}

#ifdef HX_NATIVE
void DepthBuffer3D::DrawShowing() {
    // Kinect depth rendering not available on native
}
void DepthBuffer3D::Save(BinStream &) {}
void DepthBuffer3D::Copy(const Hmx::Object *, Hmx::Object::CopyType) {}
void DepthBuffer3D::Load(BinStream &) {}
#else

BEGIN_SAVES(DepthBuffer3D)
    SAVE_REVS(11, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(RndDrawable)
    SAVE_SUPERCLASS(RndTransformable)
    bs << mNobodyColor;
    bs << mPlayerPalette;
    bs << mPlayerPaletteOffset;
    bs << mMinimalMat;
    bs << mDrawSheet;
    bs << mMesh;
    bs << mPlayerPaletteScale;
    bs << mStretchNearCamera;
    bs << mOpacity;
    bs << mDrawPlayer1;
    bs << mDrawPlayer2;
    bs << mDrawNonPlayers;
    bs << mTile;
    bs << mScaleVoxel;
    bs << mScaleVoxelGap;
    bs << mFishEyeX;
    bs << mFishEyeY;
    bs << mMaxZoom;
    bs << mDebugLayout;
    bs << mMaxDepthZoom;
END_SAVES

INIT_REVS(11, 0)

BEGIN_LOADS(DepthBuffer3D)
    LOAD_REVS(bs)
    ASSERT_REVS(11, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    LOAD_SUPERCLASS(RndDrawable)
    LOAD_SUPERCLASS(RndTransformable)
    if (d.rev > 1) {
        d.stream >> mNobodyColor;
        d >> mPlayerPalette;
        d >> mPlayerPaletteOffset;
        d >> mMinimalMat;
        if (d.rev < 3) {
            int dummy;
            d >> dummy;
        }
    }
    if (d.rev > 2) {
        d >> mDrawSheet;
        d >> mMesh;
    }
    if (d.rev > 3) {
        d >> mPlayerPaletteScale;
        d >> mStretchNearCamera;
    }
    if (d.rev > 4) {
        d >> mOpacity;
    }
    if (d.rev > 5) {
        d >> mDrawPlayer1;
        d >> mDrawPlayer2;
        d >> mDrawNonPlayers;
    }
    if (d.rev > 6) {
        d.stream >> mTile;
        d >> mScaleVoxel;
        d >> mScaleVoxelGap;
    }
    if (d.rev > 7) {
        d >> mFishEyeX;
    }
    if (d.rev > 8) {
        d >> mFishEyeY;
    }
    if (d.rev > 9) {
        d >> mMaxZoom;
        d >> mDebugLayout;
    }
    if (d.rev > 10) {
        d >> mMaxDepthZoom;
    }
END_LOADS

BEGIN_COPYS(DepthBuffer3D)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndDrawable)
    COPY_SUPERCLASS(RndTransformable)
    CREATE_COPY(DepthBuffer3D)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mNobodyColor)
        COPY_MEMBER(mPlayerPalette)
        COPY_MEMBER(mPlayerPaletteOffset)
        COPY_MEMBER(mPlayerPaletteScale)
        COPY_MEMBER(mMinimalMat)
        COPY_MEMBER(mDrawSheet)
        COPY_MEMBER(mMesh)
        COPY_MEMBER(mStretchNearCamera)
        COPY_MEMBER(mOpacity)
        COPY_MEMBER(mDrawPlayer1)
        COPY_MEMBER(mDrawPlayer2)
        COPY_MEMBER(mDrawNonPlayers)
        COPY_MEMBER(mTile)
        COPY_MEMBER(mScaleVoxel)
        COPY_MEMBER(mScaleVoxelGap)
        COPY_MEMBER(mFishEyeX)
        COPY_MEMBER(mFishEyeY)
    END_COPYING_MEMBERS
END_COPYS

#endif
