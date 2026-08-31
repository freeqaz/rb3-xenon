#pragma once
#include "math/Mtx.h"
#include "obj/Object.h"
#include "rndobj/Mat.h"
#include "rnddx9/RenderState.h"

class NgMat : public RndMat {
    friend class RndShaderMultimesh;
    friend class RndShaderStandard;
    friend class RndShaderParticles;
    friend class RndShaderFur;
    friend class RndShaderSyncTrack;
public:
    NgMat();
    virtual ~NgMat();
    OBJ_CLASSNAME(Mat);
    OBJ_SET_TYPE(Mat);

    bool AllowFog() const;
    bool AllowHDR() const;
    void SetupShader(bool, bool);

    static Hmx::Object *NewObject();
    static NgMat *Current() { return sCurrent; }
    static void SetCurrent(NgMat *c) { sCurrent = c; }

protected:
    static NgMat *sCurrent;

    void SetupAmbient();
    void SetBasicState();
    void RefreshState();
    void SetRegularShaderConst(bool);

    // ==== Retail RB3-360 layout, 0x18c..0x250 (sizeof(NgMat) == 0x250 == 592). ====
    // Offsets below are READ OFF RETAIL, not inferred: every one appears as a
    // literal displacement in `?RefreshState@NgMat@@IAAXXZ` (0x824a9368) --
    // `stfs f13, 0x18c(r3)` for mTexHalfPixelX, the two `Matrix4::Zero` bases
    // `addi rN, r31, 0x1b4` / `0x1f4` with their +0/+0x14/+0x28/+0x3c diagonals,
    // and `stw ..., 0x248` / `stb ..., 0x24c` for the blend-op pair. The size is
    // corroborated by `?NewObject@NgMat@@SAPAVObject@Hmx@@XZ`'s `li r3, 0x250`.
    //
    // ⚠ The comments here used to carry DC3's offsets (0x22c/0x254/0x294/0x2d4/
    // 0x2e8), which are self-consistent only against DC3's LARGER RndMat
    // (0x22c vs retail's 0x18c -- a 0xa0 gap, mostly DC3-added material members).
    // They read as a live 16-byte layout bug long after the real 16-byte defect
    // was fixed. That defect was `ObjPtr<MetaMaterial>` + 3 flags in RndMat, a
    // DC3 block retail does not have; it was removed in 32f4fdb1 (lane-mat-1,
    // 2026-08-13) and RndMat has ended at 0x18c ever since. Declaration order
    // here already == retail offset order; do NOT reorder these members.
    float mTexHalfPixelX; // 0x18c
    float mTexHalfPixelY; // 0x190
    float mTexHalfPixelNegX; // 0x194
    float mTexHalfPixelNegY; // 0x198
    RndRenderState::Blend mBlendSrc; // 0x19c
    RndRenderState::Blend mBlendDest; // 0x1a0
    bool mDepthTestEnable; // 0x1a4
    bool mDepthWriteEnable; // 0x1a5
    RndRenderState::TestFunc mDepthFunc; // 0x1a8
    RndRenderState::TestFunc mStencilFunc; // 0x1ac
    RndRenderState::StencilOp mStencilZFail; // 0x1b0
    Hmx::Matrix4 mTexGenMatrix; // 0x1b4
    Hmx::Matrix4 mTexGenMatrix2; // 0x1f4
    // Blend-derived shader state, written by the second `mBlend` switch in
    // RefreshState and consumed by every RndShader*::CalcShaderOpts as a 2-bit
    // option field plus a Vector4 constant. Names still `unk` because the
    // semantics are unproven; the offsets are not.
    int unk234; // 0x234
    float unk238; // 0x238
    float unk23c; // 0x23c
    float unk240; // 0x240
    float unk244; // 0x244
    RndRenderState::BlendOp mBlendOp; // 0x248
    bool mBlendEnable; // 0x24c
};
