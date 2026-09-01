#pragma once
#include "hamobj/RhythmDetector.h"
#include "math/Color.h"
#include "math/DoubleExponentialSmoother.h"
#include "obj/Object.h"
#include "rnddx9/Rnd.h"
#include "rndobj/Draw.h"
#include "rndobj/Mat.h"
#include "rndobj/Mesh.h"
#include "rndobj/Tex.h"
#include "rndobj/Trans.h"
#include "utl/MemMgr.h"

struct DepthBuffer3DAttachment {
    RndTransformable *obj; // 0x0
    int player; // 0x4
    bool unk1c; // 0x8  (retail byte@8)
    int mJoint; // 0xc
    float mOffset; // 0x10
    // Retail RB3 sizeof(DepthBuffer3DAttachment) == 0x28 (verified against the retail XEX:
    // vector<...>::_M_fill_insert's `divw` divides the byte-span by `li 0x28`). The matched
    // RB3 paths only read obj/player/mJoint/mOffset, so keep those head offsets and reflect
    // the true retail size with the trailing bytes (DC3-newer reshaped this region into a
    // Vector3 mOffset + int unk20; RB3 retail is 0x28, 4 bytes larger than DC3's 0x24).
    char _pad14[0x14]; // 0x14..0x28
};

/** "Render the Kinect depth buffer as a 3D mesh" */
class DepthBuffer3D : public RndDrawable, public RndTransformable {
    friend class BustAMovePanel;
public:
    // Hmx::Object
    virtual ~DepthBuffer3D();
    OBJ_CLASSNAME(DepthBuffer3D);
    OBJ_SET_TYPE(DepthBuffer3D);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    // RndDrawable
    virtual void DrawShowing();
    virtual void ListDrawChildren(std::list<RndDrawable *> &);
    // RndHighlightable
    virtual void Highlight() { RndDrawable::Highlight(); }

    OBJ_MEM_OVERLOAD(0x2F);
    static void Init();
    NEW_OBJ(DepthBuffer3D)

    void SetPlayerPalette(RndTex *);
    void AddAttachment(const DepthBuffer3DAttachment &);
    void SetGrooviness(float);
    void SetGrooviness(RhythmDetector *, RhythmDetector *);
    void ForceDrawSkeletonIndex(int, bool);

    ObjPtr<RndTex> GetUnk18C() const { return mPlayerPaletteTex; }
    void SetUnk18C(RndTex *tex) { mPlayerPaletteTex = tex; }

protected:
    DepthBuffer3D();

    void UpdateAttachment(DepthBuffer3DAttachment &, const Vector4 &, const Vector4 &);

    static LargeQuadRenderData mQuad;

    /** "draw old school depth buffer - 1 plane" */
    bool mDrawSheet; // 0xd8
    /** "Whether Player 1 should be drawn in this DepthBuffer3D" */
    bool mDrawPlayer1; // 0xd9
    /** "Whether Player 2 should be drawn in this DepthBuffer3D" */
    bool mDrawPlayer2; // 0xda
    /** "Whether non-players should be drawn in this DepthBuffer3D" */
    bool mDrawNonPlayers; // 0xdb
    /** "enabled alters xbox rendering to display every voxel" */
    bool mDebugLayout; // 0xdc
    /** "Color for non-player pixels (i.e. the background)" */
    Hmx::Color mNobodyColor; // 0xe0
    /** "1D palette for player depth" */
    ObjPtr<RndTex> mPlayerPalette; // 0xf0
    ObjPtr<RndTex> mBoxymanPalette; // 0xfc
    float mBoxymanPaletteAnim; // 0x108
    /** "Starting point for palette". Ranges from -1 to 1. */
    float mPlayerPaletteOffset; // 0x10c
    /** "Scale the coordinate used to look up the palette value.
        If the scale is 2, you'll cycle through the palette twice as fast, and so on.".
        Ranges from -100 to 100. */
    float mPlayerPaletteScale; // 0x110
    /** "Some Mat properties are used to render the depth buffer" */
    ObjPtr<RndMat> mMinimalMat; // 0x114
    /** "Mesh to draw" */
    ObjPtr<RndMesh> mMesh; // 0x120
    /** "Stretch the depth buffer along an exponential curve.
        1 is the default; values greater than 1 mean more distortion
        for objects closer to the Kinect camera.". Ranges from 0 to 10. */
    float mStretchNearCamera; // 0x12c
    /** "Multiply palette alpha by this value.". Ranges from 0 to 1. */
    float mOpacity; // 0x130
    float mPlayer1Grooviness; // 0x134
    float mPlayer2Grooviness; // 0x138
    int mForceDrawSkeletonIdx; // 0x13c
    bool mForceDrawEnabled; // 0x140
    ObjPtr<RndTex> mPlayerPaletteTex; // 0x144
    int unk1a0; // 0x150
    int unk1a4; // 0x154
    int unk1a8; // 0x158
    int unk1ac; // 0x15c
    int unk1b0; // 0x160
    int unk1b4; // 0x164
    int unk1b8; // 0x168
    int unk1bc; // 0x16c
    /** "How many times to tile the mesh in the x-axis/y-axis" */
    Vector2 mTile; // 0x170
    /** "Voxel scalar" */
    float mScaleVoxel; // 0x178
    /** "Voxel gap scalar" */
    float mScaleVoxelGap; // 0x17c
    /** "horizontal fisheye coefficient" */
    float mFishEyeX; // 0x180
    /** "vertical fisheye coefficient" */
    float mFishEyeY; // 0x184
    ObjPtr<RhythmDetector> mGroovinessDetector1; // 0x188
    ObjPtr<RhythmDetector> mGroovinessDetector2; // 0x194
    std::vector<DepthBuffer3DAttachment> mAttachments; // 0x1a0
    DoubleExponentialSmoother unk20c; // 0x1ac
    DoubleExponentialSmoother unk220; // 0x1c0
    DoubleExponentialSmoother unk234; // 0x1d4
    DoubleExponentialSmoother unk248; // 0x1e8
    DoubleExponentialSmoother unk25c; // 0x1fc
    DoubleExponentialSmoother unk270; // 0x210
    /** "maximum uv zooming" */
    float mMaxZoom; // 0x224
    /** "maximum uv zooming" */
    float mMaxDepthZoom; // 0x228
    bool unk28c; // 0x22c
};
