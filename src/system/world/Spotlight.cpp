// Two ObjPtr sites in Spotlight::Load pull OPPOSITE ways, and they are different
// ctors: `ObjPtr<RndGroup> group(this)` is the 1-arg OWNER-ONLY ctor, which retail
// inlines with the vptr lis/addi HOISTED ABOVE the member store; the ObjPtr<RndMat>
// temp is the 2-arg ctor, which retail leaves OUT OF LINE (`bl fn_8229D9C8`).
// RB3_OBJPTR_FORCEINLINE_CTOR is TU-wide and gets the second one wrong (it inlines
// both, and pins the vptr AFTER the stores). The owner-only lever hits exactly the
// 1-arg ctor and leaves the 2-arg one alone.
#define RB3_OBJPTR_INLINE_OWNER_CTOR

#include "world/Spotlight.h"
#include "char/Character.h"
#include "Spotlight.h"
#include "SpotlightDrawer.h"
#include "math/Color.h"
#include "math/Geo.h"
#include "math/Mtx.h"
#include "math/Rot.h"
#include "math/Utl.h"
#include "math/Vec.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "os/Debug.h"
#include "os/Timer.h"
#include "rnddx9/Mesh.h"
#include "rndobj/Cam.h"
#include "rndobj/Draw.h"
#include "rndobj/Env.h"
#include "rndobj/Flare.h"
#include "rndobj/Group.h"
#include "rndobj/Mat.h"
#include "rndobj/Poll.h"
#include "rndobj/Rnd.h"
#include "rndobj/Trans.h"
#include "utl/BinStream.h"
#include "utl/Loader.h"
#include "world/LightPreset.h"

#ifdef HX_NATIVE
inline double __fsel(double a, double b, double c) { return a >= 0.0 ? b : c; }
#endif

#ifdef HX_NATIVE
RndMesh *Spotlight::sDiskMesh;
#endif
RndEnviron *Spotlight::sEnviron;

#pragma region BeamDef

Spotlight::BeamDef::BeamDef(Hmx::Object *owner)
    : mBeam(nullptr), mIsCone(false), mLength(100), mTopRadius(4), mBottomRadius(30),
      mTopSideBorder(0.1), mBottomSideBorder(0.3), mBottomBorder(0.5), mOffset(0),
      mTargetOffset(0, 0), mBrighten(1), mExpand(1), mShape(), mNumSections(0),
      // TWO-ARG spelling: retail leaves these two ObjPtr ctors OUT OF LINE here.
      // RB3_OBJPTR_INLINE_OWNER_CTOR (needed for the ObjPtr<RndGroup> site in
      // Spotlight::Load) is TU-wide and would otherwise inline them, which took
      // this ctor 100% -> 51.4% and two 44 B funclets off 100% as well.
      mNumSegments(0), mXSection(owner, nullptr), mCutouts(owner),
      mMat(owner, nullptr) {}

Spotlight::BeamDef::BeamDef(const Spotlight::BeamDef &def)
    : mBeam(0), mIsCone(def.mIsCone), mLength(def.mLength), mTopRadius(def.mTopRadius),
      mBottomRadius(def.mBottomRadius), mTopSideBorder(def.mTopSideBorder),
      mBottomSideBorder(def.mBottomSideBorder), mBottomBorder(def.mBottomBorder),
      mOffset(def.mOffset), mTargetOffset(def.mTargetOffset), mBrighten(def.mBrighten),
      mExpand(def.mExpand), mShape(def.mShape), mNumSections(def.mNumSections),
      mNumSegments(def.mNumSegments),
      mXSection(def.mXSection.Owner(), def.mXSection.Ptr()), mCutouts(def.mCutouts),
      mMat(def.mMat.Owner(), def.mMat.Ptr()) {
    if (def.mBeam) {
        mBeam = Hmx::Object::New<RndMesh>();
        mBeam->Copy(def.mBeam, kCopyDeep);
    }
}

Spotlight::BeamDef::~BeamDef() { RELEASE(mBeam); }

void Spotlight::BeamDef::OnSetMat(RndMat *mat) {
    mMat = mat;
    if (mBeam)
        mBeam->SetMat(mMat);
}

void Spotlight::BeamDef::Save(BinStream &bs) const {
    bs << mIsCone;
    bs << mLength;
    bs << mBottomRadius;
    bs << mTopRadius;
    bs << mTopSideBorder;
    bs << mBottomSideBorder;
    bs << mBottomBorder;
    bs << mMat;
    bs << mOffset;
    bs << mTargetOffset;
    bs << mBrighten;
    bs << mXSection;
    bs << mExpand;
    bs << mShape;
    bs << mCutouts;
    bs << mNumSections;
    bs << mNumSegments;
}

// RB3-360 retail rev storage (cast model, lane EB-2): retail never constructs a
// BinStreamRev -- `.?AVBinStreamRev@@` is absent from the retail RTTI pool while
// BinStream/MemStream/FileStream are all present, and Load has no ctor/vptr/dtor.
// The loaded revision lives in ONE aligned(4) aggregate -- retail addresses both
// words off a SINGLE base (altRev @+0, rev @+4: `sth r11,0x4,rX,lbl_82CC7860`).
// Two separate statics do NOT reproduce that: MSVC scattered them here, resolving
// altRev off ?sEnviron@Spotlight@@ +8 while rev became its own base (14 `lhz`
// offset mismatches). CharHair needs the separate form; this TU needs the
// aggregate -- check the emitted offsets per TU, the rule does not generalise.
static struct {
    __declspec(align(4)) unsigned short altRev;
    __declspec(align(4)) unsigned short rev;
} gRevs_Spotlight;
#define gSpotlightAltRev gRevs_Spotlight.altRev
#define gSpotlightRev gRevs_Spotlight.rev

void Spotlight::BeamDef::Load(BinStream &d) {
    d >> mIsCone;
    d >> mLength;
    d >> mBottomRadius;
    d >> mTopRadius;
    d >> mTopSideBorder;
    d >> mBottomSideBorder;
    d >> mBottomBorder;
    d >> mMat;
    if (gSpotlightRev > 0x11 && gSpotlightRev < 0x13) {
        char name[0x80];
        d.ReadString(name, 0x80);
    }
    d >> mOffset;
    if (gSpotlightRev < 10) {
        Vector4 v;
        d >> v;
    }
    d >> mTargetOffset;
    if (gSpotlightRev > 0x14) {
        d >> mBrighten;
        d >> mXSection;
    }
    if (gSpotlightRev > 0x17) {
        d >> mExpand;
    }
    if (gSpotlightRev > 0x1A) {
        d >> (int &)mShape;
    }
    if (gSpotlightRev > 0x18) {
        d >> mCutouts;
    }
    if (gSpotlightRev > 0x1F) {
        d >> mNumSections;
        d >> mNumSegments;
    }
}

Vector2 Spotlight::BeamDef::NGRadii() const {
    Vector2 v;
    float vx = mTopRadius * mExpand;
    float vy = mBottomRadius * mExpand;
    if (!mIsCone) {
        vy *= 1.0f - mBottomSideBorder * 0.7f;
        vx *= 1.0f - mTopSideBorder * 0.7f;
    }
    v.Set(vx, vy);
    return v;
}

#pragma endregion
#pragma region Spotlight

Spotlight::Spotlight()
    : mSpotMaterial(this), mFlare(Hmx::Object::New<RndFlare>()), mFlareEnabled(true),
      mFlareVisibilityTest(true), mFlareOffset(0), mSpotScale(30), mSpotHeight(0.25),
      mColor(1, 1, 1), mIntensity(1), mColorOwner(this, this), mLensSize(0),
      mLensOffset(0), mLensMaterial(this), mBeam(this), mSlaves(this),
      mLightCanMesh(this), mLightCanOffset(0), mTarget(this), mTargetLoaded(true),
      mSpotTarget(this), mFloorSpotTargetZ(-1e33), mTargetShadow(false), mLightCanSort(false),
      mSnapToTarget(true), mDampingConstant(1), mAdditionalObjects(this),
      mAnimateColorFromPreset(true), mAnimateOrientationFromPreset(true), mUpdating(false) {
    mFlare->SetTransParent(this, false);
    mFloorSpotXfm.Reset();
    mLensXfm.Reset();
    mLightCanXfm.Reset();
    mOrientMatrix.Identity();
    mLastTargetPos.Zero();
    mDampQuat.Reset();
    mOrder = -1000;
}

Spotlight::~Spotlight() {
    CloseSlaves();
    SpotlightDrawer::RemoveFromLists(this);
    RELEASE(mFlare);
}

// Retail 0x824D81C0 (144 B). Same virtual-base compare as the rest of the
// Replace family (see RndCamAnim::Replace), but the body is TWO INDEPENDENT
// ifs, not an if/else: retail sets the owner from the cast (unguarded -- no
// null test on the cast result), then RE-READS the member from memory
// (lwz r11,-0x180(r31)) and, if it is now null, falls back to `this`.
// The old `if (!SetObj(to)) mColorOwner = this;` collapsed both into one
// test against SetObj's return value, which retail does not have.
void Spotlight::Replace(ObjRef *from, Hmx::Object *to) {
    RndTransformable::Replace(from, to);
    if (static_cast<Hmx::Object *>(mColorOwner.Ptr())
        == reinterpret_cast<Hmx::Object *>(from)) {
        mColorOwner.SetOwnerObj(dynamic_cast<Spotlight *>(to));
    }
    if (!mColorOwner.Ptr()) {
        mColorOwner.SetOwnerObj(this);
    }
}

BEGIN_HANDLERS(Spotlight)
    HANDLE_ACTION(propogate_targeting_to_presets, PropogateToPresets(2))
    HANDLE_ACTION(propogate_coloring_to_presets, PropogateToPresets(1))
    HANDLE_SUPERCLASS(RndDrawable)
    HANDLE_SUPERCLASS(RndTransformable)
    HANDLE_SUPERCLASS(RndPollable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(Spotlight)
    SYNC_PROP_MODIFY(length, mBeam.mLength, Generate())
    SYNC_PROP_MODIFY(top_radius, mBeam.mTopRadius, Generate())
    SYNC_PROP_MODIFY(bottom_radius, mBeam.mBottomRadius, Generate())
    SYNC_PROP_MODIFY(top_side_border, mBeam.mTopSideBorder, Generate())
    SYNC_PROP_MODIFY(bottom_side_border, mBeam.mBottomSideBorder, Generate())
    SYNC_PROP_MODIFY(bottom_border, mBeam.mBottomBorder, Generate())
    SYNC_PROP_SET(material, mBeam.mMat.Ptr(), mBeam.OnSetMat(_val.Obj<RndMat>()))
    SYNC_PROP_MODIFY(offset, mBeam.mOffset, Generate())
    SYNC_PROP_MODIFY(angle_offset, mBeam.mTargetOffset, Generate())
    SYNC_PROP_MODIFY(is_cone, mBeam.mIsCone, Generate())
    SYNC_PROP(brighten, mBeam.mBrighten)
    SYNC_PROP_MODIFY(expand, mBeam.mExpand, Generate())
    SYNC_PROP_MODIFY(shape, (int &)mBeam.mShape, Generate())
    SYNC_PROP(xsection, mBeam.mXSection)
    SYNC_PROP(cutouts, mBeam.mCutouts)
    SYNC_PROP_MODIFY(sections, mBeam.mNumSections, Generate())
    SYNC_PROP_MODIFY(segments, mBeam.mNumSegments, Generate())
    SYNC_PROP_MODIFY(light_can, mLightCanMesh, UpdateBounds())
    SYNC_PROP_MODIFY(light_can_offset, mLightCanOffset, UpdateBounds())
    SYNC_PROP(light_can_sort, mLightCanSort)
    SYNC_PROP_MODIFY(target, mTarget, UpdateTransforms())
    SYNC_PROP(target_shadow, mTargetShadow)
    SYNC_PROP_SET(flare_material, mFlare->GetMat(), mFlare->SetMat(_val.Obj<RndMat>()))
    SYNC_PROP(flare_size, mFlare->Sizes())
    SYNC_PROP(flare_range, mFlare->Range())
    SYNC_PROP_SET(flare_steps, mFlare->GetSteps(), mFlare->SetSteps(_val.Int()))
    SYNC_PROP_MODIFY(flare_offset, mFlareOffset, UpdateBounds())
    SYNC_PROP_MODIFY(flare_enabled, mFlareEnabled, UpdateFlare())
    SYNC_PROP_SET(
        flare_visibility_test, !mFlareVisibilityTest, SetFlareIsBillboard(!_val.Int())
    )
    SYNC_PROP_MODIFY(spot_target, mSpotTarget, UpdateBounds())
    SYNC_PROP_MODIFY(spot_scale, mSpotScale, UpdateBounds())
    SYNC_PROP_MODIFY(spot_height, mSpotHeight, UpdateBounds())
    SYNC_PROP_MODIFY(spot_material, mSpotMaterial, UpdateBounds())
    SYNC_PROP_SET(color, Color().Pack(), SetColor(_val.Int()))
    SYNC_PROP_SET(intensity, Intensity(), SetIntensity(_val.Float())) // fix this line
    SYNC_PROP(color_owner, mColorOwner)
    SYNC_PROP(damping_constant, mDampingConstant)
    SYNC_PROP_MODIFY(lens_size, mLensSize, UpdateBounds())
    SYNC_PROP_MODIFY(lens_offset, mLensOffset, UpdateBounds())
    SYNC_PROP_MODIFY(lens_material, mLensMaterial, UpdateBounds())
    SYNC_PROP(additional_objects, mAdditionalObjects)
    SYNC_PROP(slaves, mSlaves)
    SYNC_PROP(animate_orientation_from_preset, mAnimateOrientationFromPreset)
    SYNC_PROP(animate_color_from_preset, mAnimateColorFromPreset)
    SYNC_SUPERCLASS(RndDrawable)
    SYNC_SUPERCLASS(RndTransformable)
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain does not include this superclass;
    // DC3's newer engine added it. Native-only.
    SYNC_SUPERCLASS(RndPollable)
#endif
END_PROPSYNCS

void Spotlight::InitObject() {
    Hmx::Object::InitObject();
    Generate();
}

BEGIN_SAVES(Spotlight)
    SAVE_REVS(0x21, 0)
    SAVE_SUPERCLASS(RndPollable)
    SAVE_SUPERCLASS(RndDrawable)
    SAVE_SUPERCLASS(RndTransformable)
    bs << mSpotScale;
    bs << mSpotHeight;
    mBeam.Save(bs);
    bs << mLightCanMesh;
    bs << mTarget;
    bs << mSpotTarget;
    bs << mLightCanOffset;
    bs << mLightCanSort;
    bs << mColor;
    bs << mIntensity;
    bs << mSpotMaterial;
    bs << mDampingConstant;
    ObjPtr<RndMat> mat(this, mFlare->GetMat());
    bs << mat;
    bs << mFlare->Sizes();
    bs << mFlare->Range();
    bs << mFlare->GetSteps();
    bs << mFlareOffset;
    bs << mFlareEnabled;
    bs << mFlareVisibilityTest;
    bs << mLensSize;
    bs << mLensOffset;
    bs << mLensMaterial;
    bs << mAdditionalObjects;
    bs << mSlaves;
    bs << mTargetShadow;
    bs << mAnimateColorFromPreset;
    bs << mAnimateOrientationFromPreset;
    bs << mColorOwner;
END_SAVES

BEGIN_COPYS(Spotlight)
    COPY_SUPERCLASS(RndPollable)
    COPY_SUPERCLASS(RndTransformable)
    COPY_SUPERCLASS(RndDrawable)
    CREATE_COPY(Spotlight)
    BEGIN_COPYING_MEMBERS
        if (ty != kCopyFromMax) {
            mFlare->Copy(c->mFlare, kCopyDeep);
            COPY_MEMBER(mFlareOffset)
            COPY_MEMBER(mLightCanMesh)
            COPY_MEMBER(mTarget)
            COPY_MEMBER(mSpotTarget)
            COPY_MEMBER(mSpotScale)
            COPY_MEMBER(mSpotHeight)
            SetColorIntensity(c->Color(), c->Intensity());
            COPY_MEMBER(mSpotMaterial)
            COPY_MEMBER(mDampingConstant)
            COPY_MEMBER(mLensSize)
            COPY_MEMBER(mLensOffset)
            COPY_MEMBER(mLensMaterial)
            COPY_MEMBER(mLightCanOffset)
            COPY_MEMBER(mLightCanSort)
            COPY_MEMBER(mFlareEnabled)
            COPY_MEMBER(mFlareVisibilityTest)
            UpdateFlare();
            COPY_MEMBER(mTargetShadow)
            COPY_MEMBER(mAnimateColorFromPreset)
            COPY_MEMBER(mAnimateOrientationFromPreset)
            COPY_MEMBER(mAdditionalObjects)
            COPY_MEMBER(mSlaves)
            COPY_MEMBER(mBeam.mIsCone)
            COPY_MEMBER(mBeam.mLength)
            COPY_MEMBER(mBeam.mBottomRadius)
            COPY_MEMBER(mBeam.mTopRadius)
            COPY_MEMBER(mBeam.mTopSideBorder)
            COPY_MEMBER(mBeam.mBottomSideBorder)
            COPY_MEMBER(mBeam.mBottomBorder)
            COPY_MEMBER(mBeam.mMat)
            COPY_MEMBER(mBeam.mTargetOffset)
            COPY_MEMBER(mBeam.mBrighten)
            COPY_MEMBER(mBeam.mExpand)
            COPY_MEMBER(mBeam.mShape)
            COPY_MEMBER(mBeam.mXSection)
            COPY_MEMBER(mBeam.mCutouts)
            COPY_MEMBER(mBeam.mOffset)
            COPY_MEMBER(mBeam.mNumSections)
            COPY_MEMBER(mBeam.mNumSegments)
            if (c->mBeam.mBeam) {
                mBeam.mBeam = Hmx::Object::New<RndMesh>();
                mBeam.mBeam->Copy(c->mBeam.mBeam, kCopyDeep);
            }
            // NO Generate() here. DC3 (newer engine) added a trailing
            // `Generate();` at this point; RB3 does
            // not have it -- the target
            // has no `bl ?Generate@Spotlight@@IAAXXZ` anywhere in Copy, while
            // our DC3-derived copy emitted `mr r3, r29` / `bl Generate` as the
            // last two instructions. Retail decides.
        }
    END_COPYING_MEMBERS
END_COPYS

BinStream &operator>>(BinStream &d, Spotlight::BeamDef &bd) {
    bd.Load(d);
    return d;
}

INIT_REVS(0x21, 0)

BEGIN_LOADS(Spotlight)
    int revs;
    bs >> revs;
    gSpotlightRev = getHmxRev(revs);
    gSpotlightAltRev = getAltRev(revs);
    BinStream &d = bs; // retail passes the raw stream (no BinStreamRev in RB3)
    if (gSpotlightRev < 9) {
        MILO_FAIL("Unsupported spotlight version");
    } else {
        RndPollable::Load(bs);
        RndDrawable::Load(bs);
        RndTransformable::Load(bs);
        bs >> mSpotScale;
        bs >> mSpotHeight;
        if (gSpotlightRev > 0x16) {
            mBeam.Load(d);
        } else {
            ObjVector<BeamDef> beams(this);
            d >> beams;
            MILO_ASSERT(beams.size() <= 1, 0xCD);
            if (beams.size() != 0) {
                mBeam = beams[0];
            } else {
                mBeam.mLength = 0;
            }
        }
        if (gSpotlightRev > 0x15) {
            d >> mLightCanMesh;
        } else {
            ObjPtr<RndGroup> group(this);
            d >> group;
            ConvertGroupToMesh(group);
        }
        if (!mTarget.Load(bs, false, 0)) {
            mTargetLoaded = false;
        }
        if (gSpotlightRev > 0x1C) {
            d >> mSpotTarget;
        }
        d >> mLightCanOffset;
        if (gSpotlightRev > 0x1E) {
            d >> mLightCanSort;
        }
        d >> mColor;
        mColor.alpha = 1;
        if (gSpotlightRev > 9) {
            d >> mIntensity;
        }
        d >> mSpotMaterial;
        if (gSpotlightRev > 0x11 && gSpotlightRev < 0x13) {
            char buf[0x80];
            bs.ReadString(buf, 0x80);
            if (!mSpotMaterial && buf[0] != '\0') {
                mSpotMaterial = LookupOrCreateMat(buf, Dir());
            }
        }
        d >> mDampingConstant;
        if (gSpotlightRev < 0x21) {
            Symbol s;
            d >> s;
        }
        if (gSpotlightRev > 10) {
            // TWO-ARG spelling on purpose: retail leaves this ctor OUT OF LINE
            // (`li r5,0; addi r3,r31,0x50; bl fn_8229D9C8`). The one-arg owner-only
            // form is inlined by RB3_OBJPTR_INLINE_OWNER_CTOR above, which is right
            // for the ObjPtr<RndGroup> site but wrong here -- same per-site split
            // documented for Part.cpp's mMat.
            ObjPtr<RndMat> mat(this, nullptr);
            d >> mat;
            mFlare->SetMat(mat);
            if (gSpotlightRev > 0x11 && gSpotlightRev < 0x13) {
                char buf[0x80];
                bs.ReadString(buf, 0x80);
                if (!mat && buf[0] != '\0') {
                    mat = LookupOrCreateMat(buf, Dir());
                    mFlare->SetMat(mat);
                }
            }
            d >> mFlare->Sizes();
            d >> mFlare->Range();
            int steps;
            d >> steps;
            mFlare->SetSteps(steps);
            d >> mFlareOffset;
        }
        if (gSpotlightRev > 0xD) {
            d >> mFlareEnabled;
        }
        if (gSpotlightRev > 0xE) {
            d >> mFlareVisibilityTest;
        }
        UpdateFlare();
        if (gSpotlightRev > 0xB) {
            d >> mLensSize;
            d >> mLensOffset;
            d >> mLensMaterial;
        }
        if (gSpotlightRev > 0x11 && gSpotlightRev < 0x13) {
            char buf[0x80];
            bs.ReadString(buf, 0x80);
            if (!mLensMaterial && buf[0] != '\0') {
                mLensMaterial = LookupOrCreateMat(buf, Dir());
            }
        }
        if (gSpotlightRev > 0xC) {
            d >> mAdditionalObjects;
        }
        if (gSpotlightRev > 0x1B) {
            d >> mSlaves;
        }
        if (gSpotlightRev > 0xF) {
            d >> mTargetShadow;
        }
        if (gSpotlightRev > 0x19) {
            d >> mAnimateColorFromPreset;
            d >> mAnimateOrientationFromPreset;
        } else if (gSpotlightRev > 0x10) {
            d >> mAnimateColorFromPreset;
            mAnimateOrientationFromPreset = mAnimateColorFromPreset;
        }
        if (gSpotlightRev > 0x1D) {
            d >> mColorOwner;
            if (!mColorOwner) {
                mColorOwner = this;
            }
        }
        Generate();
    }
END_LOADS

void Spotlight::DrawShowing() {
    START_AUTO_TIMER("spotlight");
    if (mLightCanSort && mLightCanMesh) {
        mLightCanMesh->SetWorldXfm(mLightCanXfm);
        Sphere s(mLightCanMesh->GetSphere());
        if (s.GetRadius() > 0) {
            Multiply(s, mLightCanXfm, s);
            if (!(s > RndCam::Current()->WorldFrustum())) {
                mLightCanMesh->DrawShowing();
            }
        }
    }
    // Two early returns, not an if/else-if chain.  82828C9C `bne cr6` skips the
    // DrawLight arm, and 82828CB4 `bne` is followed by its OWN scope exit
    // (`addi r3, r31, 0x80; b <~AutoTimer>`) rather than a branch into the
    // common tail -- that second copy only appears for an explicit `return`.
    if (TheRnd.DrawMode() == Rnd::kDrawNormal) {
        SpotlightDrawer::DrawLight(this);
        return;
    }
    if (!mTargetLoaded)
        return;
    UpdateTransforms();
    // w7-bo (2026-09-15): 97.56 -> 97.60 canonical, 21 mismatch rows -> 8.
    // Lever: SIBLING block scopes for `c` and `tracker`.  The image gives
    // `tracker` the SAME frame word as `c` -- it builds the colour at r31+0x60
    // (82828D08..D18, target idx 87 `addi r9, r31, 0x60`) and then passes
    // r31+0x60 to ??0RndEnvironTracker (82828D44, target idx 92 `addi r3, r31,
    // 0x60`) and to ??1RndEnvironTracker (82828FB0).  With `c` at function
    // scope our build put it at 0x60 and `tracker` at 0xa0; that one extra word
    // shifted `_at` 0x70->0x80 and the Sphere 0x80->0x90, i.e. all fourteen
    // [off:-16] diff_arg rows plus a frame Δ +0x10.
    // Measured negatives:
    //   - `c` alone in its own closing scope: byte-identical, 97.56 both ways
    //     (the earlier note here claimed the reuse was therefore unobtainable;
    //     it is not -- `tracker` has to be scoped TOO, so the two scopes are
    //     siblings and MSVC coalesces the slots).
    // Residual (8 rows) is a store-scheduling group around the tracker ctor --
    // the image sinks `stw r8, 0x50(r31)` past the two `stw`s our build emits
    // first, and hoists `addi r8, r10, 0x13c` (the RndEnviron vtable/field
    // pointer) above them:
    //   [95]  insert   stw  r7, 0x4(r9)        [96]  insert   stw  r6, 0x8(r9)
    //   [102] delete   stw  r8, 0x0(r9)        [103] delete   stw  r7, 0x4(r9)
    //   [104] delete   addi r8, r10, 0x13c     [105] diff_arg stw [reg:r6->r8, off:-8]
    //   [109] replace  stw r8, 0x50(r31) vs addi r8, r10, 0x13c
    //   [117] insert   stw  r8, 0x50(r31)
    // Same instruction MULTISET on both sides -- only the schedule differs.
    // Target spends r8 on `addi r8, r10, 0x13c` at idx 104, which forces
    // `stw r8, 0x0(r9)` ahead of it at 102 and the whole c-copy lands in
    // address order 0x0/0x4/0x8/0xc; our build keeps r8 holding the 0x1b0
    // word longer, fires 0x4/0x8 as soon as their loads retire (95/96), and
    // pushes the `addi`+home store past the fmuls.  Pure MSVC store
    // scheduling driven by r8's live range.
    // Further measured negatives, both 97.60 / 8 rows, byte-for-byte the same
    // eight rows:
    //   - `Hmx::Color c = Color();` copy-initialisation instead of direct-init
    //   - `UpdateTransforms();` moved INSIDE the scope (the RB3 sibling's
    //     shape -- rb3 Spotlight.cpp puts UpdateTransforms, c48 and tracker in
    //     one block; that block shape is what we had at 97.56/21 rows, so the
    //     sibling-scope split is the DC3-specific lever, not a port of rb3).
    {
        Hmx::Color c(Color());
        Multiply(c, Intensity(), c);
        sEnviron->SetAmbientColor(c);
    }
    {
        RndEnvironTracker tracker(sEnviron, nullptr);
        FOREACH (it, mAdditionalObjects) {
            MILO_ASSERT(*it != this, 0x3E3);
            if (*it != this)
                (*it)->DrawShowing();
        }
        if (mLensMaterial) {
            MILO_ASSERT(sDiskMesh, 0x3ED);
            sDiskMesh->SetWorldXfm(mLensXfm);
            sDiskMesh->SetMat(mLensMaterial);
            sDiskMesh->DrawShowing();
        }
        auto& _ref3 = mBeam;
        if (_ref3.mBeam && TheRnd.DrawMode() != 4) {
            _ref3.mBeam->DrawShowing();
        }
        if (mFlare && mFlare->GetMat()) {
            mFlare->Draw();
        }
        if (mTarget) {
            if (mTargetShadow) {
                Character *c = dynamic_cast<Character *>(mTarget.Ptr());
                if (c) {
                    // Ground plane 3 units above the character's origin.
                    Vector3 pos(c->WorldXfm().v);
                    pos.z += 3.0f;
                    Plane plane(pos, Vector3(0, 0, 1));
                    c->DrawShadow(WorldXfm(), plane);
                }
            }
            if (DoFloorSpot()) {
                MILO_ASSERT(sDiskMesh, 0x40F);
                sDiskMesh->SetWorldXfm(mFloorSpotXfm);
                sDiskMesh->SetMat(mSpotMaterial);
                sDiskMesh->DrawShowing();
            }
        }
    }
}

bool Spotlight::MakeWorldSphere(Sphere &s, bool b) {
    if (b) {
        s.Zero();
        if (mBeam.mBeam) {
            Sphere s28;
            if (mBeam.mBeam->MakeWorldSphere(s28, true)) {
                s.GrowToContain(s28);
            }
        }
        if (DoFloorSpot()) {
            MILO_ASSERT(sDiskMesh, 0x2FD);
            Sphere s38;
            sDiskMesh->SetWorldXfm(mFloorSpotXfm);
            if (sDiskMesh->MakeWorldSphere(s38, true)) {
                s.GrowToContain(s38);
            }
        }
        if (mFlare) {
            Sphere s48;
            if (mFlare->MakeWorldSphere(s48, true)) {
                s.GrowToContain(s48);
            }
        }
        if (mLightCanMesh) {
            Sphere s58;
            mLightCanMesh->SetWorldXfm(mLightCanXfm);
            if (mLightCanMesh->MakeWorldSphere(s58, true)) {
                s.GrowToContain(s58);
            }
        }
        return true;
    } else if (mSphere.GetRadius()) {
        Multiply(mSphere, WorldXfm(), s);
        return true;
    } else
        return false;
}

void Spotlight::Mats(std::list<RndMat *> &mats, bool addAO) {
    if (mLensMaterial && addAO) {
        mats.push_back(mLensMaterial);
        for (unsigned int i = 0; i < 2U; i++) {
            MatShaderOptions opts;
            opts.SetLast5(0xC);
            opts.mTempMat = true;
            opts.SetHasAOCalc(i);
            RndMat *mat = Hmx::Object::New<RndMat>();
            mat->Copy(mLensMaterial, kCopyDeep);
            mat->SetShaderOpts(opts);
            mats.push_back(mat);
        }
    }
    if (mSpotMaterial) {
        mats.push_back(mSpotMaterial);
    }
    if (mLightCanMesh && mLightCanMesh->Mat()) {
        MatShaderOptions opts;
        opts.SetLast5(0xC);
        RndMat *lightMat = mLightCanMesh->Mat();
        lightMat->SetShaderOpts(opts);
        mats.push_back(lightMat);
        if (addAO) {
            for (unsigned int i = 0; i < 2U; i++) {
                MatShaderOptions opts2;
                opts2.SetLast5(0xC);
                opts2.mTempMat = true;
                opts2.SetHasAOCalc(i);
                RndMat *mat = Hmx::Object::New<RndMat>();
                mat->Copy(mLightCanMesh->Mat(), kCopyDeep);
                mat->SetShaderOpts(opts2);
                mats.push_back(mat);
            }
        }
    }
    if (mBeam.mMat) {
        mats.push_back(mBeam.mMat);
    }
}

void Spotlight::ListDrawChildren(std::list<RndDrawable *> &draws) {
    if (mLightCanMesh)
        draws.push_back(mLightCanMesh);
    FOREACH (it, mAdditionalObjects) {
        draws.push_back(*it);
    }
}

RndDrawable *Spotlight::CollideShowing(const Segment &s, float &f, Plane &p) {
    if (mLightCanMesh) {
        mLightCanMesh->SetWorldXfm(mLightCanXfm);
        bool showing = mLightCanMesh->Showing();
        mLightCanMesh->SetShowing(true);
        bool collide = mLightCanMesh->Collide(s, f, p);
        mLightCanMesh->SetShowing(showing);
        if (collide) {
            return this;
        }
    }
    return nullptr;
}

int Spotlight::CollidePlane(const Plane &pl) {
    if (mLightCanMesh) {
        mLightCanMesh->SetWorldXfm(mLightCanXfm);
        bool oldshowing = mLightCanMesh->Showing();
        mLightCanMesh->SetShowing(true);
        int coll = mLightCanMesh->CollidePlane(pl);
        mLightCanMesh->SetShowing(oldshowing);
        if (coll)
            return coll;
    }
    return -1;
}

void Spotlight::UpdateBounds() {
    UpdateTransforms();
    UpdateSphere();
}

void Spotlight::SetFlareIsBillboard(bool b) {
    mFlareVisibilityTest = b;
    UpdateFlare();
}

void Spotlight::SetColor(int packed) {
    Hmx::Color color;
    color.Unpack(packed);
    color.alpha = 1.0f;
    SetColorIntensity(color, Intensity());
}
void Spotlight::SetIntensity(float f) { SetColorIntensity(Color(), f); }

void Spotlight::SetColorIntensity(const Hmx::Color &c, float f) {
    mColorOwner->mColor = c;
    mColorOwner->mIntensity = f;
}

void Spotlight::Init() {
    REGISTER_OBJ_FACTORY(Spotlight)
    sEnviron = Hmx::Object::New<RndEnviron>();
    BuildBoard();
}

void Spotlight::BuildBoard() {
#ifdef HX_NATIVE
    return; // Skip mesh setup on native — no renderer
#endif
    MILO_ASSERT(!sDiskMesh, 0x42E);
    sDiskMesh = Hmx::Object::New<RndMesh>();
    RndMesh::VertVector &verts = sDiskMesh->Verts();
    std::vector<RndMesh::Face> &faces = sDiskMesh->Faces();
    verts.resize(4);
    faces.resize(2);

    verts[0].pos.Set(-0.5, -0.5, 0);
    verts[0].color = Hmx::Color(1, 1, 1);
    verts[0].tex.Set(0, 0);

    verts[1].pos.Set(0.5, -0.5, 0);
    verts[1].color = Hmx::Color(1, 1, 1);
    verts[1].tex.Set(1, 0);

    verts[2].pos.Set(-0.5, 0.5, 0);
    verts[2].color = Hmx::Color(1, 1, 1);
    verts[2].tex.Set(0, 1);

    verts[3].pos.Set(0.5, 0.5, 0);
    verts[3].color = Hmx::Color(1, 1, 1);
    verts[3].tex.Set(1, 1);

    faces[0].Set(0, 1, 2);
    faces[1].Set(1, 3, 2);
    sDiskMesh->Sync(0x13F);
    DxMesh *dxDiskMesh = static_cast<DxMesh *>(sDiskMesh);
    dxDiskMesh->GetMultimeshFaces();
    sDiskMesh->UpdateSphere();
}

void Spotlight::UpdateFlare() {
    // Configure flare visibility and testing modes based on enabled/visibility flags.
    // Note: Local variable 'flare' required for register allocation match.
    RndFlare *flare;
    if (!mFlareEnabled) {
        // Flare disabled: hide and disable point testing
        flare = mFlare;
        flare->SetOcclusionReady(true);
        flare->SetVisible(false);
        mFlare->SetPointTest(false);
    } else if (mFlareVisibilityTest) {
        // Flare with visibility test: show but disable point testing
        flare = mFlare;
        flare->SetOcclusionReady(true);
        flare->SetVisible(true);
        mFlare->SetPointTest(false);
    } else
        // Flare always visible: enable point testing (billboard mode)
        mFlare->SetPointTest(true);
}

bool Spotlight::DoFloorSpot() const {
    return mSpotMaterial && GetFloorSpotTarget()
        && GetFloorSpotTarget()->WorldXfm().m.y.z;
}

void Spotlight::CalculateDirection(RndTransformable *target, Hmx::Matrix3 &mtx) {
    MILO_ASSERT(target, 0x2CE);
    Vector3 v20;
    Subtract(target->WorldXfm().v, WorldXfm().v, v20);
    Vector3 v2c;
    Cross(v20, Vector3(1.0f, 0.0f, 0.0f), v2c);
    Normalize(v2c, v2c);
    MakeRotMatrix(v20, v2c, mtx);
}

void Spotlight::SetFlareEnabled(bool b) {
    mFlareEnabled = b;
    UpdateFlare();
}

void Spotlight::CloseSlaves() {
    FOREACH (it, mSlaves) {
        RndLight *lit = *it;
        if (lit)
            lit->SetShadowOverride(0);
    }
}

void Spotlight::UpdateSlaves() {
    if (mSlaves.empty())
        return;
    else {
        FOREACH (it, mSlaves) {
            RndLight *lit = *it;
            Transform tf40(WorldXfm());
            if (lit->TransParent()) {
                Transform tf70;
                Invert(lit->TransParent()->WorldXfm(), tf70);
                Multiply(WorldXfm(), tf70, tf40);
            }
            lit->SetLocalXfm(tf40);
            lit->SetShadowOverride(&mBeam.mCutouts);
            lit->SetShowing(Showing());
        }
    }
}

void Spotlight::CheckFloorSpotTransform() {
    if (DoFloorSpot()) {
        if (GetFloorSpotTarget()->WorldXfm().v.z != mFloorSpotTargetZ) {
            UpdateFloorSpotTransform(WorldXfm());
        }
    }
}

void Spotlight::ConvertGroupToMesh(RndGroup *grp) {
    if (grp) {
        int count = 0;
        std::vector<RndDrawable *>::const_iterator it = grp->Draws().begin();
        std::vector<RndDrawable *>::const_iterator itEnd = grp->Draws().end();
        for (; it != itEnd; it++) {
            RndMesh *cur = dynamic_cast<RndMesh *>(*it);
            if (cur) {
                count++;
                if (!mLightCanMesh)
                    mLightCanMesh = cur;
            }
        }
        if (count > 1) {
            MILO_NOTIFY(
                "Multiple meshes (%d) found converting light can group %s to mesh",
                count,
                grp->Name()
            );
        }
    }
}

void Spotlight::PropogateToPresets(int i) {
    for (ObjDirItr<LightPreset> it(Dir(), false); it != nullptr; ++it) {
        it->SetSpotlight(this, i);
    }
}

void Spotlight::Generate() {
#ifdef HX_NATIVE
    if (!mBeam.mBeam || TheLoadMgr.EditMode()) {
        RELEASE(mBeam.mBeam);
        if (mBeam.HasLength()) {
            if (SpotlightDrawer::DrawNGSpotlights()) {
                BuildNGShaft(mBeam);
            } else if (mBeam.IsCone()) {
                BuildCone(mBeam);
            } else {
                BuildBeam(mBeam);
            }
        }
        UpdateBounds();
        UpdateSphere();
    }
#else
    // RB3 360 retail: no edit-mode regen and only the NG shaft path
    // (retail tests mBeam, releases, then calls BuildNGShaft directly).
    if (!mBeam.mBeam) {
        RELEASE(mBeam.mBeam);
        if (mBeam.HasLength()) {
            BuildNGShaft(mBeam);
        }
        UpdateBounds();
        UpdateSphere();
    }
#endif
}

void Spotlight::BuildNGShaft(Spotlight::BeamDef &def) {
    switch (def.mShape) {
    case BeamDef::kBeamRect:
        BuildNGCone(def, 4);
        break;
    case BeamDef::kBeamSheet:
        BuildNGSheet(def);
        break;
    case BeamDef::kBeamQuadXYZ:
        BuildNGQuad(def, RndTransformable::kConstraintBillboardXYZ);
        break;
    case BeamDef::kBeamQuadZ:
        BuildNGQuad(def, RndTransformable::kConstraintBillboardZ);
        break;
    default:
        int num = def.mNumSegments;
        if (def.mNumSegments <= 3) {
            num = 10;
        }
        BuildNGCone(def, num);
        break;
    }
}

void Spotlight::Poll() {
    if (!Showing())
        return;
    if (mIntensity == 0)
        return;
    Hmx::Matrix3 m;
    if (!mUpdating) {
        RndTransformable *target = nullptr;
        if (mTargetLoaded)
            target = mTarget;
        if (!target
            || (!mSnapToTarget
                && target->WorldXfm().v == mLastTargetPos)) {
            if (!target && !mAnimateOrientationFromPreset && !DoFloorSpot()) {
                UpdateTransforms();
                return;
            }
            CheckFloorSpotTransform();
            mOrientMatrix = WorldXfm().m;
            UpdateSlaves();
            return;
        }
        mLastTargetPos = target->WorldXfm().v;
        CalculateDirection(target, m);
        if (!mSnapToTarget && mDampingConstant != 1.0f) {
            Interp(mOrientMatrix, m, TheTaskMgr.DeltaSeconds() * mDampingConstant, m);
        } else {
            mSnapToTarget = false;
        }
    } else {
        MakeRotMatrix(mDampQuat, m);
    }
    SetLocalRot(m);
    mOrientMatrix = m;
    UpdateTransforms();
    mUpdating = false;
}

void Spotlight::UpdateTransforms() {
    START_AUTO_TIMER("spotlight_xfm");
    const Transform &thetf = WorldXfm();
    mLightCanXfm = thetf;
    Vector3 vcc(mLightCanXfm.m.y);
    vcc *= mLightCanOffset;
    Add(mLightCanXfm.v, vcc, mLightCanXfm.v);
    static Hmx::Matrix3 ident(
        Vector3(1.0f, 0.0f, 0.0f), Vector3(0.0f, 1.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f)
    );
    static Hmx::Matrix3 rot(
        Vector3(1.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f), Vector3(0.0f, -1.0f, 0.0f)
    );
    if (mLensMaterial) {
        Vector3 vd8(0.0f, mLensOffset, 0.0f);
        // Multiply(vd8, thetf.m, vd8) written out with the sums right
        // associated.  vd8's x and z are literal 0.0f, and fed to the shared
        // overload in Mtx.h that lets /fp:fast reassociate
        // `m.x.c*0 + m.y.c*off + m.z.c*0` into `(m.x.c + m.z.c)*0 + ...`,
        // emitting a leading `fadds` of two matrix elements.  The target emits
        // the three products straight and seeds each accumulator from the z
        // term.  The parentheses are load-bearing; without them this function
        // reads 91.96.  See the comment above the overload in Mtx.h for why the
        // fix belongs here and not there.
        {
            const Hmx::Matrix3 &m = thetf.m;
            vd8.Set(
                m.x.x * vd8.x + (m.y.x * vd8.y + m.z.x * vd8.z),
                m.x.y * vd8.x + (m.y.y * vd8.y + m.z.y * vd8.z),
                m.x.z * vd8.x + (m.y.z * vd8.y + m.z.z * vd8.z)
            );
        }
        Add(vd8, thetf.v, vd8);
        Hmx::Matrix3 m48;
        m48.Set(
            Vector3(-mLensSize, 0.0f, 0.0f),
            Vector3(0.0f, 0.0f, mLensSize),
            Vector3(0.0f, mLensSize, 0.0f)
        );
        Multiply(m48, thetf.m, m48);
        mLensXfm = Transform(m48, vd8);
    }
    if (mBeam.mBeam) {
        Vector3 ve4(0.0f, mBeam.mOffset, 0.0f);
        mBeam.mBeam->SetLocalPos(ve4);
        Hmx::Matrix3 m6c(mBeam.mIsCone ? rot : ident);
        Hmx::Matrix3 m90;
        MakeRotMatrix(
            Vector3(
                mBeam.mTargetOffset.x * DEG2RAD, 0.0f, mBeam.mTargetOffset.y * DEG2RAD
            ),
            m90,
            true
        );
        Multiply(m6c, m90, m6c);
        mBeam.mBeam->SetLocalRot(m6c);
    }
    if (mFlare && mFlare->GetMat()) {
        Vector3 vf0(0.0f, mFlareOffset, 0.0f);
        mFlare->SetLocalPos(vf0);
        mFlare->SetLocalRot(ident);
    }
    UpdateFloorSpotTransform(thetf);
    UpdateSlaves();
}

void Spotlight::UpdateFloorSpotTransform(const Transform &tf) {
    Transform &floorSpotXfm = mFloorSpotXfm;
    floorSpotXfm.Reset();
    if (DoFloorSpot()) {
        float f1 = GetFloorSpotTarget()->WorldXfm().v.z;
        Vector3 vac(tf.m.y);
        if (vac.z != 0) {
            float absed = std::fabs(((f1 - tf.v.z) / vac.z) / (f1 - tf.v.z));
            vac = tf.m.y;
            float curz = vac.z;
            vac.z = 0;
            Hmx::Matrix3 m70;
            if (curz > -0.9999999f && curz < 0.9999999f)
                MakeRotMatrix(vac, Vector3(0.0f, 0.0f, 1.0f), m70);
            else
                m70.Identity();
            vac.Set(mSpotScale, mSpotScale * absed, 1.0f);
            Scale(vac, m70, m70);
            float scalar = (f1 + mSpotHeight - tf.v.z) / curz;
            vac = tf.m.y;
            vac *= scalar;
            Add(vac, tf.v, vac);
            floorSpotXfm = Transform(m70, vac);
        }
        mFloorSpotTargetZ = f1;
    }
}

void Spotlight::BuildBeam(BeamDef &def) {
    MILO_ASSERT(!SpotlightDrawer::DrawNGSpotlights(), 0x609);
    def.mIsCone = false;
    def.mBeam = Hmx::Object::New<RndMesh>();
    float bottomBorderLen = def.mBottomBorder * def.mLength;
    float topSideBorderVal = def.mTopSideBorder * def.mTopRadius;
    RndMesh::VertVector &verts = def.mBeam->Verts();
    std::vector<RndMesh::Face> &faces = def.mBeam->Faces();
    float bottomSideBorderVal = def.mBottomSideBorder * def.mBottomRadius;

    int numSectionsTop = (int)((def.mLength - bottomBorderLen) / 15.0f);
    if (numSectionsTop <= 4) numSectionsTop = 4;

    int numSectionsBottom = (int)(bottomBorderLen / 15.0f);
    if (numSectionsBottom <= 1) numSectionsBottom = 1;

    int totalSections = numSectionsBottom + numSectionsTop;

    verts.resize(totalSections * 4);
    faces.resize(totalSections * 6);

    float topLen = def.mLength - bottomBorderLen;
    float topRadius = def.mTopRadius;
    float borderTopRadius = (topLen / def.mLength) * (def.mBottomRadius - topRadius) + topRadius;
    float radiusStepTop = borderTopRadius - topRadius;
    float topSectionLen = 1.0f / (float)numSectionsTop;
    float botSectionLen = 1.0f / (float)numSectionsBottom;
    float radiusStepTopVal = radiusStepTop * topSectionLen;
    float radiusStepBotVal = (def.mBottomRadius - borderTopRadius) * botSectionLen;

    if (totalSections != 0) {
        float halfWidth = topRadius;
        int fi = 0;
        int lVar31 = -numSectionsTop;
        short s = 6;
        int count = totalSections;
        unsigned int i = 0;
        do {
            float y;
            float alpha;
            if (i == (unsigned int)(totalSections - 1)) {
                y = def.mLength;
                alpha = 0.0f;
            } else if (!(i < (unsigned int)numSectionsTop)) {
                y = (botSectionLen * bottomBorderLen) * (float)lVar31 + topLen;
                alpha = 1.0f - (float)lVar31 / (float)numSectionsBottom;
            } else {
                y = (topLen * topSectionLen) * (float)i;
                alpha = 1.0f;
            }

            float yFrac = y / def.mLength;
            float negY = -y;
            float sideBorder = (bottomSideBorderVal - topSideBorderVal) * yFrac + topSideBorderVal;
            float borderRatio = sideBorder / (halfWidth * 2.0f);

            float leftInner = sideBorder - halfWidth;
            float rightInner = halfWidth - sideBorder;

            // Column 0: left edge
            verts[i * 4].pos.z = negY;
            verts[i * 4].pos.x = -halfWidth;
            verts[i * 4].pos.y = 0.0f;
            verts[i * 4].color.Set(0.0f, 0.0f, 0.0f, 0.0f);
            verts[i * 4].tex.Set(0.0f, yFrac);

            // Column 1: left inner
            if (-leftInner < 0.0f) leftInner = 0.0f;
            verts[i * 4 + 1].pos.x = leftInner;
            verts[i * 4 + 1].pos.y = 0.0f;
            verts[i * 4 + 1].pos.z = negY;
            verts[i * 4 + 1].color.Set(alpha, alpha, alpha, alpha);
            verts[i * 4 + 1].tex.Set(borderRatio, yFrac);

            // Column 2: right inner
            if (-rightInner < 0.0f) rightInner = 0.0f;
            verts[i * 4 + 2].pos.x = rightInner;
            verts[i * 4 + 2].pos.y = 0.0f;
            verts[i * 4 + 2].pos.z = negY;
            verts[i * 4 + 2].color.Set(alpha, alpha, alpha, alpha);
            verts[i * 4 + 2].tex.Set(1.0f - borderRatio, yFrac);

            // Column 3: right edge
            verts[i * 4 + 3].pos.x = halfWidth;
            verts[i * 4 + 3].pos.y = 0.0f;
            verts[i * 4 + 3].pos.z = negY;
            verts[i * 4 + 3].color.Set(0.0f, 0.0f, 0.0f, 0.0f);
            verts[i * 4 + 3].tex.Set(1.0f, yFrac);

            if (i != (unsigned int)(totalSections - 1)) {
                short c0 = s - 6;
                short c1 = s - 5;
                short c2 = s - 4;
                short c3 = s - 3;
                short n0 = s - 2;
                short n1 = s - 1;
                short n2 = s;
                short n3 = s + 1;

                if ((i & 1) == 0) {
                    faces[fi].Set(c0, n0, c1);
                    faces[fi + 1].Set(c1, n0, n1);
                    faces[fi + 2].Set(c1, n2, c2);
                    faces[fi + 3].Set(c1, n1, n2);
                    faces[fi + 4].Set(c2, n2, c3);
                    faces[fi + 5].v1 = c3;
                } else {
                    faces[fi].Set(c0, n0, n1);
                    faces[fi + 1].Set(c0, n1, c1);
                    faces[fi + 2].Set(c1, n1, c2);
                    faces[fi + 3].Set(c2, n1, n2);
                    faces[fi + 4].Set(c2, n3, c3);
                    faces[fi + 5].v1 = c2;
                }
                faces[fi + 5].v2 = n2;
                faces[fi + 5].v3 = n3;

                if (i == (unsigned int)(totalSections - 2)) {
                    faces[fi].Set(c0, n0, c1);
                    faces[fi + 1].Set(c1, n0, n1);
                    faces[fi + 4].Set(c2, n2, n3);
                    faces[fi + 5].Set(c3, c2, n3);
                }
            }

            if (i < (unsigned int)numSectionsTop) {
                halfWidth = radiusStepTopVal + halfWidth;
            } else {
                halfWidth = radiusStepBotVal + halfWidth;
            }

            i++;
            lVar31++;
            s += 4;
            fi += 6;
            count--;
        } while (count != 0);
    }

    def.mBeam->Sync(0x13F);
    def.mBeam->SetMat(def.mMat);
    def.mBeam->SetTransConstraint(kConstraintBillboardZ, nullptr, false);
    RndTransformable *parent;
    parent = this ? static_cast<RndTransformable *>(this) : nullptr;
    def.mBeam->SetTransParent(parent, false);
}

void Spotlight::BuildCone(BeamDef &def) {
    MILO_ASSERT(!SpotlightDrawer::DrawNGSpotlights(), 0x5B6);
    def.mIsCone = true;
    def.mBeam = Hmx::Object::New<RndMesh>();
    RndMesh::VertVector &verts = def.mBeam->Verts();
    std::vector<RndMesh::Face> &faces = def.mBeam->Faces();

    verts.resize(0x30);
    faces.resize(60);

    float len = def.mLength;
    float bottomBorderLen = def.mBottomBorder * len;
    bottomBorderLen = (float)__fsel(len - bottomBorderLen, bottomBorderLen, len);
    float borderY = len - bottomBorderLen;
    float borderRadius = (borderY / len) * (def.mBottomRadius - def.mTopRadius) + def.mTopRadius;

    float angle = 0.0f;
    float uvStep = 1.0f / 15.0f;
    float angleStep = 0.4188790f;

    for (int i = 0; i != 15; i++) {
        float cosA = std::cos(angle);
        float sinA = std::sin(angle);

        float uvX = (float)i * uvStep;

        verts[i].pos.Set(def.mTopRadius * cosA, 0.0f, def.mTopRadius * sinA);
        verts[i].color.Set(1.0f, 1.0f, 1.0f, 1.0f);
        verts[i].tex.Set(uvX, 0.0f);

        verts[i + 16].pos.Set(borderRadius * cosA, borderY, borderRadius * sinA);
        verts[i + 16].color.Set(1.0f, 1.0f, 1.0f, 1.0f);
        verts[i + 16].tex.Set(uvX, borderY / len);

        verts[i + 32].pos.Set(def.mBottomRadius * cosA, len, def.mBottomRadius * sinA);
        verts[i + 32].color.Set(0.0f, 0.0f, 0.0f, 0.0f);
        verts[i + 32].tex.Set(uvX, 1.0f);

        short s = (short)(i + 17);
        int fi = i * 4;
        faces[fi].Set(s - 17, s - 1, s);
        faces[fi + 1].Set(s - 17, s, s - 16);
        faces[fi + 2].Set(s - 1, s + 15, s + 16);
        faces[fi + 3].Set(s - 1, s + 16, s);

        angle += angleStep;
    }

    verts[15].pos.Set(def.mTopRadius, 0.0f, 0.0f);
    verts[15].color.Set(1.0f, 1.0f, 1.0f, 1.0f);
    verts[15].tex.Set(1.0f, 0.0f);

    verts[31].pos.Set(borderRadius, borderY, 0.0f);
    verts[31].color.Set(1.0f, 1.0f, 1.0f, 1.0f);
    verts[31].tex.Set(1.0f, 1.0f);

    verts[15].pos.Set(def.mTopRadius, 0.0f, 0.0f);
    verts[15].color.Set(1.0f, 1.0f, 1.0f, 1.0f);
    verts[15].tex.Set(1.0f, 0.0f);

    verts[31].pos.Set(borderRadius, borderY, 0.0f);
    verts[31].color.Set(1.0f, 1.0f, 1.0f, 1.0f);
    verts[31].tex.Set(1.0f, borderY / len);

    verts[47].pos.Set(def.mBottomRadius, len, 0.0f);
    verts[47].color.Set(0.0f, 0.0f, 0.0f, 0.0f);
    verts[47].tex.Set(1.0f, 1.0f);

    def.mBeam->Sync(0x13F);
    RndTransformable *parent = this ? static_cast<RndTransformable *>(this) : nullptr;
    def.mBeam->SetTransParent(parent, false);
    def.mBeam->SetMat(def.mMat);
}

void Spotlight::BuildNGCone(BeamDef &def, int numSegments) {
    Hmx::Matrix3 identMtx;
    identMtx.x.Set(1.0f, 0.0f, 0.0f);
    identMtx.y.Set(0.0f, 1.0f, 0.0f);
    identMtx.z.Set(0.0f, 0.0f, 1.0f);

    Hmx::Matrix3 *pMtx;
    Hmx::Matrix3 rotMtx;
    if (def.mIsCone) {
        pMtx = &identMtx;
    } else {
        rotMtx.Set(
            Vector3(1.0f, 0.0f, 0.0f),
            Vector3(0.0f, 0.0f, -1.0f),
            Vector3(0.0f, 1.0f, 0.0f)
        );
        pMtx = &rotMtx;
    }
    // MEASURED NEGATIVE (w8-q): the target holds orientMtx at frame 0xb0 and
    // identMtx at 0xe0; we hold them the other way round (16 SWAPPED slots,
    // every `stfs` in the identity init carries off:-48). Hoisting
    // `Hmx::Matrix3 orientMtx;` above identMtx -- so declaration order matches
    // the target's slot order -- is BYTE-INERT: 79.0% canonical and the same
    // 318 mismatch rows before and after. MSVC/Xenon assigns these slots by
    // liveness/first-use, not by declaration order, and all three matrices have
    // identical type, size and alignment, so nothing in the declaration list
    // discriminates them.
    Hmx::Matrix3 orientMtx;
    memcpy(&orientMtx, pMtx, 0x30);

    def.mBeam = Hmx::Object::New<RndMesh>();
    int numVerts = numSegments * 3;
    int baseVertIdx = numVerts + 1;
    int capBase = numSegments * 4;
    int topBase = capBase + numSegments;
    RndMesh *mesh = def.mBeam;
    RndMesh::VertVector &verts = mesh->Verts();
    std::vector<RndMesh::Face> &faces = mesh->Faces();

    verts.resize(numVerts + 2);
    faces.resize(numSegments * 6);

    float length = def.mLength;
    Vector2 radii = def.NGRadii();
    float halfStep = 0.5f;
    float numSegsF = (float)numSegments;
    float angleStep = 6.2831855f / numSegsF;
    float halfAngle = angleStep * 0.5f;
    float invCosHalf = 1.0f / (float)std::cos((double)halfAngle);
    float topRadius = radii.x * invCosHalf;
    float bottomRadius = radii.y * invCosHalf;

    int flip = 0;
    int iVert = 0;
    float xsAngle = 0.7853982f;
    int baseIdx = 2;
    int iFace = 0;
    for (int seg = 0; seg != numSegments; seg++) {
        float cosH = (float)std::cos((double)halfAngle);
        float sinH = (float)std::sin((double)halfAngle);
        float csAngle = 0.0f;
        // Own divisor temp: naming numSegsF here too lets /fp:fast fold both
        // divisions into one reciprocal (fdivs f31/x + fmuls), the image divides
        // twice (0x8282C448, 0x8282C4F4).
        float segU = (float)seg / (float)numSegments;

        for (unsigned int v = 0; v < 3; v++) {
            float uvV = (float)v * halfStep;
            if (v <= 1) {
                float t = (float)v;
                float radius = (bottomRadius - topRadius) * t + topRadius;
                verts[iVert].pos.Set(radius * cosH, t * length, radius * sinH);
                Multiply(verts[iVert].pos, orientMtx, verts[iVert].pos);
            } else {
                float cosCs = (float)std::cos((double)csAngle);
                float sinCs = (float)std::sin((double)csAngle);
                csAngle = csAngle + xsAngle;
                verts[iVert].pos.Set(
                    cosCs * cosH * bottomRadius,
                    sinCs * bottomRadius + length,
                    cosCs * sinH * bottomRadius
                );
                {
                    Vector3 &p = verts[iVert].pos;
                    float px = p.x, py = p.y, pz = p.z;
                    float rx = orientMtx.y.x * py;
                    rx += orientMtx.z.x * pz;
                    rx += orientMtx.x.x * px;
                    float rz = orientMtx.x.z * px;
                    rz += orientMtx.y.z * py;
                    rz += orientMtx.z.z * pz;
                    float ry = orientMtx.x.y * px;
                    ry += orientMtx.y.y * py;
                    ry += orientMtx.z.y * pz;
                    p.Set(rx, ry, rz);
                }
            }
            verts[iVert].color.Set(1.0f, 1.0f, 1.0f, 1.0f);
            verts[iVert].tex.Set(segU, uvV);
            iVert++;
        }

        int sideWidth;
        if (seg < numSegments - 1) {
            sideWidth = 3;
        } else {
            sideWidth = 3 - numVerts;
        }

        int cur = baseIdx - 1;
        int fCount = 2;
        do {
            int nextRow = cur - 1 + sideWidth;
            // Bitwise, not logical: the target emits `clrlwi. rN, rM, 31`
            // (an explicit AND with 1), so the winding alternates every
            // iteration. `flip && 1` compiles to `cmpwi rM, 0` instead and is
            // true for every iteration after the first.
            //
            // ONE flip variable, not two (w8-q, 77.766 -> 79.0 canonical). The
            // image reads the flip counter from its stack home, tests bit 0 and
            // writes back the incremented value in three adjacent instructions
            // at 0x8282C6C8/C6CC/C6D0/C6D8 -- `lwz r8,0x54(r1)`,
            // `clrlwi. r6,r8,31`, `addi r8,r8,1`, `stw r8,0x54(r1)` -- i.e. ONE
            // object read-tested-incremented, not a `flip = curFlip + 1` /
            // `curFlip = flip` pair. The old two-variable spelling kept the copy
            // live in r18 and forced a genuine spill in the else arm
            // (`sth r10,0x50(r1)` / `lhz r3,0x50(r1)`), which is now gone.
            if (flip & 1) {
                faces[iFace].Set(nextRow, cur - 1, nextRow + 1);
                faces[iFace + 1].Set(nextRow + 1, cur - 1, cur);
            } else {
                faces[iFace].Set(cur - 1, cur, nextRow);
                faces[iFace + 1].Set(nextRow, cur, nextRow + 1);
            }
            flip++;
            cur = cur + 1;
            iFace += 2;
            fCount--;
        } while (fCount != 0);

        halfAngle = halfAngle + angleStep;
        // The two cap faces of each segment live after ALL the side faces:
        // bottom caps at [4n, 5n), top caps at [5n, 6n). The image walks
        // two extra face cursors seeded at 4n*6 and 5n*6 (0x8282C4B8-C4C4)
        // and the side-face cursor advances only 4 faces per segment
        // (0x8282C7AC); interleaving them 4+2 per segment was wrong.
        faces[capBase + seg].Set(baseIdx - 2, baseIdx + sideWidth - 2, numVerts);
        faces[topBase + seg].Set(baseIdx + sideWidth, baseIdx, numVerts + 1);
        baseIdx = baseIdx + 3;
    }

    verts[numVerts].pos.Set(0.0f, 0.0f, 0.0f);
    verts[numVerts].color.Set(1.0f, 1.0f, 1.0f, 1.0f);
    verts[numVerts].tex.Set(0.0f, 0.0f);

    verts[baseVertIdx].pos.Set(0.0f, length, 0.0f);
    Multiply(verts[baseVertIdx].pos, orientMtx, verts[baseVertIdx].pos);
    verts[baseVertIdx].color.Set(1.0f, 1.0f, 1.0f, 1.0f);
    verts[baseVertIdx].tex.Set(0.0f, 1.0f);

    def.mBeam->Sync(0x13F);
    def.mBeam->SetMat(def.mMat);
    RndTransformable *parent = this ? static_cast<RndTransformable *>(this) : nullptr;
    def.mBeam->SetTransParent(parent, false);
}
void Spotlight::BuildNGSheet(BeamDef &def) {
    Hmx::Matrix3 identMtx;
    identMtx.x.Set(1.0f, 0.0f, 0.0f);
    identMtx.y.Set(0.0f, 1.0f, 0.0f);
    identMtx.z.Set(0.0f, 0.0f, 1.0f);

    Hmx::Matrix3 rotMtx;
    Hmx::Matrix3 *pMtx;
    if (def.mIsCone) {
        pMtx = &identMtx;
    } else {
        rotMtx.Set(
            Vector3(1.0f, 0.0f, 0.0f),
            Vector3(0.0f, 0.0f, -1.0f),
            Vector3(0.0f, 1.0f, 0.0f)
        );
        pMtx = &rotMtx;
    }
    Hmx::Matrix3 orientMtx;
    memcpy(&orientMtx, pMtx, 0x30);

    def.mBeam = Hmx::Object::New<RndMesh>();
    int defSections = def.mNumSections;
    RndMesh::VertVector &verts = def.mBeam->Verts();

    std::vector<RndMesh::Face> &faces = def.mBeam->Faces();
    int numSections = defSections > 1 ? defSections : 5;
    int numSegments = def.mNumSegments > 2 ? def.mNumSegments : 10;

    // NEGATIVE RESULT (w7-am, 2026-09-14): swapping these two declarations to
    // try to flip the r23<->r24 / r26<->r27 cascade keeps the score at 96.3
    // (95 rows either way) and is very slightly worse on the raw ruler
    // (94.2 vs 94.3), so the residual is not a declaration-order effect.
    int numRows = numSections + 1;
    int numCols = numSegments + 1;
    int kNumVerts = numRows * numCols;
    int kNumFaces = (numSegments * (numSections * 2));

    verts.resize(kNumVerts);
    faces.resize(kNumFaces);

    Vector2 radii = def.NGRadii();
    float topRadius = radii.x;
    float bottomRadius = radii.y;

    static float kSheetFade = 1.0f; // RB3 retail 0x82C7120C = 1.0f

    int iVert = 0;
    for (int row = 0; row < numRows; row++) {
        float t = (float)row / (float)numSections;
        for (int col = 0; col < numCols; col++) {
            // Stays INSIDE the col loop.  RB3's Spotlight.cpp has it in the row
            // loop, but hoisting it here measures 93.7 against 96.3 and permutes
            // a stack slot; DC3's codegen wants it recomputed per column.
            float oneMinusT = 1.0f - t;
            float segFrac = (float)col / (float)numSegments * 2.0f - 1.0f;
            float xTop = segFrac * topRadius;
            float xBot = segFrac * bottomRadius;
            float absSegFrac = std::fabs(segFrac);

            verts[iVert].pos.Set(
                (xBot - xTop) * t + xTop,
                def.mLength * t,
                (1.0f - absSegFrac) * kSheetFade
            );

            Vector3 &p = verts[iVert].pos;
            float px = p.x, pz = p.z, py = p.y;
            p.z = pz * orientMtx.z.z + px * orientMtx.x.z + py * orientMtx.y.z;
            p.y = pz * orientMtx.z.y + px * orientMtx.x.y + py * orientMtx.y.y;
            p.x = pz * orientMtx.z.x + px * orientMtx.x.x + py * orientMtx.y.x;

            verts[iVert].norm.Set(0.0f, 0.0f, 1.0f);

            Vector3 &n = verts[iVert].norm;
            Multiply(n, orientMtx, n);

            verts[iVert].color.Set(oneMinusT, oneMinusT, oneMinusT, oneMinusT);
            verts[iVert].tex.Set(std::fabs(segFrac), t);
            iVert++;
        }
    }
    MILO_ASSERT(iVert == kNumVerts, 0x526);

    // NEGATIVE RESULT (w7-bw, 96.30232 canonical, no change).  Three more
    // spellings refuted, each measured whole-function under name_check:
    // (1) `int base` with the four indices as unsigned short locals derived
    //     from it and from `int baseNext` (what 8282CD40..8282CD5C looks like:
    //     untruncated adds, clrlwi at the use) plus faces[iFace + 1] and a
    //     single iFace += 2: 93.6, and the prologue grows to __savegprlr_18.
    // (2) reordering the hand-inlined p.x sum to py*yx + pz*zx + px*xx (the
    //     image's x' at 8282CBFC..8282CC18 is x*xx + (z*zx + y*yx)): inert,
    //     byte-for-byte the same block -- /fp:fast canonicalises the three
    //     sums regardless of source order, so the 24 fmadds rows of the pos
    //     and norm blocks are a lowering floor from this TU.
    // (3) The two MakeString rows (MILO_ASSERT at 0x526 / 0x53F) are charged
    //     because the target's whole-TU ICF representative is
    //     MakeString<char[19], int, char[5]> and our <char[14], int, char[19]>
    //     is not in scripts/symbol_aliases.json's accepted classes; the same
    //     representative is uncharged in this TU's Handle/Load/BuildBoard.
    //     Instrument gap, not source.
    int iFace = 0;
    int rowStart = 0;
    for (int row = 0; row < numSections; row++) {
        for (int col = 0; col < numSegments; col++) {
            // `base` really is a u16 here: measured, spelling all four indices as
            // plain ints -- which is what 8282CD40's untruncated `add r9, r4, r3`
            // looks like in isolation -- drops the function from 96.3 to 94.4 and
            // adds 35 rows of GPR renumbering across the whole body.
            unsigned short base = (unsigned short)(rowStart + col);
            int next = base + 1;
            int baseNext = base + numCols;
            int nextNext = baseNext + 1;
            if (iFace & 2) {
                faces[iFace].Set(
                    (unsigned short)baseNext, (unsigned short)base, (unsigned short)nextNext
                );
                iFace++;
                faces[iFace].Set(
                    (unsigned short)nextNext, (unsigned short)base, (unsigned short)next
                );
            } else {
                faces[iFace].Set(
                    (unsigned short)base, (unsigned short)next, (unsigned short)baseNext
                );
                iFace++;
                faces[iFace].Set(
                    (unsigned short)baseNext, (unsigned short)next, (unsigned short)nextNext
                );
            }
            iFace++;
        }
        rowStart += numCols;
    }
    MILO_ASSERT(iFace == kNumFaces, 0x53F);

    def.mBeam->Sync(0x13F);
    def.mBeam->SetMat(def.mMat);
    RndTransformable *parent = this ? static_cast<RndTransformable *>(this) : nullptr;
    def.mBeam->SetTransParent(parent, false);
}


void Spotlight::BuildNGQuad(BeamDef &def, RndTransformable::Constraint constraint) {
    auto mesh = Hmx::Object::New<RndMesh>();
    def.mBeam = mesh;
    std::vector<RndMesh::Face> &faces = def.mBeam->Faces();
    int gridSize = def.mNumSegments;
    RndMesh::VertVector &verts = def.mBeam->Verts();
    if (def.mNumSections >= gridSize) {
        gridSize = def.mNumSections;
    }
    static int sGridSize = (gridSize > 0) ? gridSize + 1 : 2;

    int nMinus1 = sGridSize - 1;
    int totalVerts = sGridSize * sGridSize;
    int totalFaces = (nMinus1 * (nMinus1 * 2));

    verts.resize(totalVerts);
    faces.resize(totalFaces);

    int n = sGridSize;
    float topRadius = def.mLength;
    float bottomRadius = def.mBottomRadius;

    // SURVEY 2026-09-14 (w7-ae), 88.1% canonical, 145 mismatch rows, no edit.
    // The pos matrix-multiply block (diff rows 113-127) is structurally IDENTICAL
    // to the image, term for term and store for store: both sides load y,z,x,
    // both multiply by the ZERO elements rather than folding them away, both
    // associate the three-term dot product left to right, and both store z,y,x in
    // that order.  Exactly ONE row differs, and it is instruction selection for
    // the -1 element:
    //     target  .L_82684f80  fmadds f4, f4, f9, f1     (f9 = -1.0, hoisted from
    //                                                     __real@bf800000 at
    //                                                     .L_82684edc/.L_82684ee4,
    //                                                     BEFORE the loop)
    //     ours    0x10fc8      fsubs  f5, f2, f5
    // i.e. the image carries the nine matrix elements in REGISTERS across the
    // whole loop (f0 = 0.0, f13 = 1.0, f9 = -1.0) and therefore multiplies by a
    // register, while our build still knows the multiplier is literally -1.0 at
    // the multiply site and strength-reduces `a + b * -1.0f` to `a - b`.  That
    // one choice is what forces the image to hold TWO callee-saved FPRs where we
    // hold one (`stfd f30`/`stfd f31` vs `stfd f31`) and one extra GPR
    // (`bl __savegprlr_22` vs `__savegprlr_23`), which is the whole reported
    // frame delta of -0x10 and nearly all 21 register-swap pairs -- so the single
    // fsubs row is worth ~12pp of renaming behind it.
    // (w7-bw correction: the store order does NOT already match -- the image
    // stores z,y,x (stfs 0x8/0x4/0x0 at 8282D048/50/58) and ours stores x,z,y,
    // and the x' / y' sums associate differently under /fp:fast.  Same root:
    // the literal -1 is visible at our multiply site and not at the image's.)
    // NOT a spelling of Multiply(): what would have to change is whether MSVC can see the
    // literal at the multiply, and no value-preserving source form of a
    // Matrix3 built from literals was found that hides it.  Recorded, not fixed.
    Hmx::Matrix3 rot;
    rot.Set(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f);

    int idx = 0;
    float rowFrac;
    for (int row = 0; row < n; row++) {
                rowFrac = (float)row / (float)(n - 1);
        float colFrac;
        for (int col = 0; col < n; col++) {
                        colFrac = (float)col / (float)(n - 1);

            verts[idx].pos.Set(
                (colFrac * 2.0f - 1.0f) * bottomRadius,
                (rowFrac * 2.0f - 1.0f) * topRadius,
                0.0f
            );
            Multiply(verts[idx].pos, rot, verts[idx].pos);

            verts[idx].norm.Set(0.0f, 0.0f, 1.0f);
            Multiply(verts[idx].norm, rot, verts[idx].norm);

            verts[idx].color.Set(1.0f, 1.0f, 1.0f, 1.0f);
            verts[idx].tex.Set(colFrac, rowFrac);
            idx++;
        }
    }

    // RESIDUAL (w7-bw, 90.62 canonical, face loop 8282D100..8282D1C0): ours
    // computes base + n once at the loop top as the next IV value and then
    // derives uPrev from it as (0xffff - n) + (base + n); the image adds
    // 0xffff to base directly (8282D13C) and forms base + n inside each
    // branch (8282D150 / 8282D190), feeding the IV update from that register
    // (mr r11, r8 at 8282D1B0).  Refuted, each measured whole-function:
    // explicit `int base = row + 1` + `col++, base += n` gives two bottom-
    // updated IVs and a down-counted outer loop (88.5, one more GPR saved);
    // `(unsigned short)base - 1` for uPrev is inert; the RB3 ibase form
    // (uPrev = ibase, uBase = ibase + 1, ...) makes ibase the IV and derives
    // uBase from uBaseN + (1 - n) instead (88.6); a hoisted `int nm1 = n - 1`
    // is inert; unsigned short locals for all four values pre-computed
    // before the branch is the 88.4 state.  The rot lowering in the vertex
    // loop (fmadds f9 vs fsubs, and the store order z,y,x vs x,z,y) is in
    // the off-limits math header's Multiply and is the ae-recorded floor.
    int iFace = 0;
    for (int row = 0; row < nMinus1; row++) {
        for (int col = 0; col < nMinus1; col++) {
            int base = row + 1 + col * n;
            int uBaseN = base + n - 1;
            unsigned short uPrev = base - 1;
            if (iFace & 2) {
                faces[iFace++].Set(uBaseN, uPrev, base + n);
                faces[iFace++].Set(base + n, uPrev, base);
            } else {
                faces[iFace++].Set(uPrev, base, uBaseN);
                faces[iFace++].Set(uBaseN, base, base + n);
            }
        }
    }

    def.mBeam->Sync(0x13F);
    def.mBeam->SetMat(def.mMat);
    def.mBeam->SetTransConstraint(constraint, nullptr, false);
    RndTransformable *parent = this ? static_cast<RndTransformable *>(this) : nullptr;
    def.mBeam->SetTransParent(parent, false);
}

// sw2 scatter-include (default/Spotlight <- world/ColorPalette.cpp)
#define gRev gRev_ColorPalette
#define gAltRev gAltRev_ColorPalette
#include "world/ColorPalette.cpp"
#undef gRev
#undef gAltRev

// W17-OWN: the ObjRefConcrete<RndGroup, ObjectDir> dtor specialisation that lived
// here is gone -- see the note after ~ObjRefConcrete in obj/ObjPtr_p.h.
