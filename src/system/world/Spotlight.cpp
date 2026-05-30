#include "world/Spotlight.h"
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
      mNumSegments(0), mXSection(owner), mCutouts(owner), mMat(owner) {}

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

void Spotlight::BeamDef::Load(BinStreamRev &d) {
    d >> mIsCone;
    d >> mLength;
    d >> mBottomRadius;
    d >> mTopRadius;
    d >> mTopSideBorder;
    d >> mBottomSideBorder;
    d >> mBottomBorder;
    d >> mMat;
    if (d.rev > 0x11 && d.rev < 0x13) {
        char name[0x80];
        d.stream.ReadString(name, 0x80);
    }
    d >> mOffset;
    if (d.rev < 10) {
        Vector4 v;
        d >> v;
    }
    d >> mTargetOffset;
    if (d.rev > 0x14) {
        d >> mBrighten;
        d >> mXSection;
    }
    if (d.rev > 0x17) {
        d >> mExpand;
    }
    if (d.rev > 0x1A) {
        d >> (int &)mShape;
    }
    if (d.rev > 0x18) {
        d >> mCutouts;
    }
    if (d.rev > 0x1F) {
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

bool Spotlight::Replace(ObjRef *from, Hmx::Object *to) {
    if (RefIs(from, mColorOwner)) {
        if (!mColorOwner.SetObj(to)) {
            mColorOwner = this;
        }
        return true;
    } else {
        return RndTransformable::Replace(from, to);
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
    SYNC_SUPERCLASS(RndPollable)
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
            Generate();
        }
    END_COPYING_MEMBERS
END_COPYS

BinStreamRev &operator>>(BinStreamRev &d, Spotlight::BeamDef &bd) {
    bd.Load(d);
    return d;
}

INIT_REVS(0x21, 0)

BEGIN_LOADS(Spotlight)
    LOAD_REVS(bs)
    ASSERT_REVS(0x21, 0)
    if (d.rev < 9) {
        MILO_FAIL("Unsupported spotlight version");
    } else {
        RndPollable::Load(bs);
        RndDrawable::Load(bs);
        RndTransformable::Load(bs);
        bs >> mSpotScale;
        bs >> mSpotHeight;
        if (d.rev > 0x16) {
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
        if (d.rev > 0x15) {
            d >> mLightCanMesh;
        } else {
            ObjPtr<RndGroup> group(this);
            d >> group;
            ConvertGroupToMesh(group);
        }
        if (!mTarget.Load(bs, false, 0)) {
            mTargetLoaded = false;
        }
        if (d.rev > 0x1C) {
            d >> mSpotTarget;
        }
        d >> mLightCanOffset;
        if (d.rev > 0x1E) {
            d >> mLightCanSort;
        }
        d >> mColor;
        mColor.alpha = 1;
        if (d.rev > 9) {
            d >> mIntensity;
        }
        d >> mSpotMaterial;
        if (d.rev > 0x11 && d.rev < 0x13) {
            char buf[0x80];
            bs.ReadString(buf, 0x80);
            if (!mSpotMaterial && buf[0] != '\0') {
                mSpotMaterial = LookupOrCreateMat(buf, Dir());
            }
        }
        d >> mDampingConstant;
        if (d.rev < 0x21) {
            Symbol s;
            d >> s;
        }
        if (d.rev > 10) {
            ObjPtr<RndMat> mat(this);
            d >> mat;
            mFlare->SetMat(mat);
            if (d.rev > 0x11 && d.rev < 0x13) {
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
        if (d.rev > 0xD) {
            d >> mFlareEnabled;
        }
        if (d.rev > 0xE) {
            d >> mFlareVisibilityTest;
        }
        UpdateFlare();
        if (d.rev > 0xB) {
            d >> mLensSize;
            d >> mLensOffset;
            d >> mLensMaterial;
        }
        if (d.rev > 0x11 && d.rev < 0x13) {
            char buf[0x80];
            bs.ReadString(buf, 0x80);
            if (!mLensMaterial && buf[0] != '\0') {
                mLensMaterial = LookupOrCreateMat(buf, Dir());
            }
        }
        if (d.rev > 0xC) {
            d >> mAdditionalObjects;
        }
        if (d.rev > 0x1B) {
            d >> mSlaves;
        }
        if (d.rev > 0xF) {
            d >> mTargetShadow;
        }
        if (d.rev > 0x19) {
            d >> mAnimateColorFromPreset;
            d >> mAnimateOrientationFromPreset;
        } else if (d.rev > 0x10) {
            d >> mAnimateColorFromPreset;
            mAnimateOrientationFromPreset = mAnimateColorFromPreset;
        }
        if (d.rev > 0x1D) {
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
    if (TheRnd.GetDrawMode() == Rnd::kDrawNormal) {
        SpotlightDrawer::DrawLight(this);
    } else if (mTargetLoaded) {
        UpdateTransforms();
        Hmx::Color c(Color());
        Multiply(c, Intensity(), c);
        sEnviron->SetAmbientColor(c);
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
        if (_ref3.mBeam && TheRnd.GetDrawMode() != 5) {
            _ref3.mBeam->DrawShowing();
        }
        if (mFlare && mFlare->GetMat()) {
            mFlare->Draw();
        }
        if (mTarget) {
            if (mTargetShadow) {
                RndDrawable *drawable = dynamic_cast<RndDrawable *>(mTarget.Ptr());
                if (drawable) {
                    drawable->DrawShadow(WorldXfm(), 3.0f);
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
    if (!TheLoadMgr.EditMode()) {
        if (!Showing())
            return;
        if (mIntensity == 0)
            return;
    }
    Hmx::Matrix3 m;
    if (!mUpdating) {
        RndTransformable *target = nullptr;
        if (mTargetLoaded)
            target = mTarget;
        if (!target
            || (!TheLoadMgr.EditMode() && !mSnapToTarget
                && target->WorldXfm().v == mLastTargetPos)) {
            if (!target && !mAnimateOrientationFromPreset && !DoFloorSpot()) {
                UpdateTransforms();
            } else {
                CheckFloorSpotTransform();
                mOrientMatrix = WorldXfm().m;
                UpdateSlaves();
            }
            Normalize(mLocalXfm.m, m);
            SetLocalRot(m);
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
    Normalize(m, m);
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
        Multiply(vd8, thetf.m, vd8);
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
    mFloorSpotXfm.Reset();
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
            mFloorSpotXfm = Transform(m70, vac);
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
    Hmx::Matrix3 orientMtx;
    memcpy(&orientMtx, pMtx, 0x30);

    def.mBeam = Hmx::Object::New<RndMesh>();
    int numVerts = numSegments * 3;
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
    float csAngle = 0.0f;
    float xsAngle = 0.7853982f;
    short baseIdx = 2;
    int iFace = 0;
    for (int seg = 0; seg != numSegments; seg++) {
        float cosH = (float)std::cos((double)halfAngle);
        float sinH = (float)std::sin((double)halfAngle);
        float segU = (float)seg / numSegsF;

        for (unsigned int v = 0; v < 3; v++) {
            float uvV = (float)v * halfStep;
            if (v <= 1) {
                float t = (float)v;
                float radius = (bottomRadius - topRadius) * t + topRadius;
                verts[iVert].pos.Set(radius * cosH, t * length, radius * sinH);

                float px = verts[iVert].pos.x;
                float py = verts[iVert].pos.y;
                float pz = verts[iVert].pos.z;
                verts[iVert].pos.x =
                    orientMtx.y.x * py + orientMtx.z.x * pz + orientMtx.x.x * px;
                verts[iVert].pos.z =
                    orientMtx.y.z * py + (orientMtx.x.z * px + orientMtx.z.z * pz);
                verts[iVert].pos.y =
                    orientMtx.y.y * py + (orientMtx.x.y * px + orientMtx.z.y * pz);
            } else {
                float cosCs = (float)std::cos((double)csAngle);
                float sinCs = (float)std::sin((double)csAngle);
                csAngle = csAngle + xsAngle;
                verts[iVert].pos.y = sinCs * bottomRadius + length;
                verts[iVert].pos.z = cosCs * sinH * bottomRadius;
                verts[iVert].pos.x = cosCs * cosH * bottomRadius;

                float px = verts[iVert].pos.x;
                float pz = verts[iVert].pos.z;
                float py = verts[iVert].pos.y;
                verts[iVert].pos.x =
                    orientMtx.x.x * px + (orientMtx.y.x * py + orientMtx.z.x * pz);
                verts[iVert].pos.z =
                    orientMtx.z.z * pz + (orientMtx.x.z * px + orientMtx.y.z * py);
                verts[iVert].pos.y =
                    orientMtx.z.y * pz + (orientMtx.x.y * px + orientMtx.y.y * py);
            }
            verts[iVert].color.Set(1.0f, 1.0f, 1.0f, 1.0f);
            verts[iVert].tex.Set(segU, uvV);
            iVert++;
        }

        short sideWidth;
        if (seg < numSegments - 1) {
            sideWidth = 3;
        } else {
            sideWidth = 3 - (short)numVerts;
        }

        short cur = baseIdx - 1;
        int curFlip = flip;
        int fCount = 2;
        do {
            flip = curFlip + 1;
            short nextRow = cur - 1 + sideWidth;
            if (curFlip & 1) {
                faces[iFace].Set(nextRow, cur - 1, nextRow + 1);
                faces[iFace + 1].Set(nextRow + 1, cur - 1, cur);
            } else {
                faces[iFace].Set(cur - 1, cur, nextRow);
                faces[iFace + 1].Set(nextRow, cur, nextRow + 1);
            }
            cur = cur + 1;
            iFace += 2;
            curFlip = flip;
            fCount--;
        } while (fCount != 0);

        halfAngle = halfAngle + angleStep;
        faces[iFace].Set(baseIdx - 2, baseIdx + sideWidth - 2, numVerts);
        faces[iFace + 1].Set(baseIdx + sideWidth, baseIdx, numVerts + 1);
        baseIdx = baseIdx + 3;
        iFace += 2;
    }

    verts[numVerts].pos.Set(0.0f, 0.0f, 0.0f);
    verts[numVerts].color.Set(1.0f, 1.0f, 1.0f, 1.0f);
    verts[numVerts].tex.Set(0.0f, 0.0f);

    int baseVertIdx = numVerts + 1;
    verts[baseVertIdx].pos.Set(0.0f, length, 0.0f);
    float px = verts[baseVertIdx].pos.x;
    float py = verts[baseVertIdx].pos.y;
    float pz = verts[baseVertIdx].pos.z;
    verts[baseVertIdx].pos.z =
        orientMtx.y.z * py + (orientMtx.x.z * px + orientMtx.z.z * pz);
    verts[baseVertIdx].pos.y =
        orientMtx.y.y * py + (orientMtx.x.y * px + orientMtx.z.y * pz);
    verts[baseVertIdx].pos.x =
        px * orientMtx.x.x + (orientMtx.z.x * pz + orientMtx.y.x * py);
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
    int numSections = def.mNumSections;
    std::vector<RndMesh::Face> &faces = def.mBeam->Faces();

    RndMesh::VertVector &verts = def.mBeam->Verts();
    if (numSections <= 1) numSections = 5;
    int numSegments = def.mNumSegments;
    if (numSegments <= 2) numSegments = 10;

    int numRows = numSections + 1;
    int numCols = numSegments + 1;
    int kNumVerts = numRows * numCols;
    int kNumFaces = (numSegments * (numSections * 2));

    verts.resize(kNumVerts);
    faces.resize(kNumFaces);

    Vector2 radii = def.NGRadii();
    float topRadius = radii.x;
    float bottomRadius = radii.y;

    static float kSheetFade = 0.3f;

    int iVert = 0;
    for (int row = 0; row < numRows; row++) {
        float t = (float)row / (float)numSections;
        float oneMinusT = 1.0f - t;
        for (int col = 0; col < numCols; col++) {
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
            float px = p.x, py = p.y, pz = p.z;
            p.x = (pz * orientMtx.z.x + (px * orientMtx.x.x + py * orientMtx.y.x));
            p.z = py * orientMtx.y.z + px * orientMtx.x.z + pz * orientMtx.z.z;
            p.y = py * orientMtx.y.y + px * orientMtx.x.y + pz * orientMtx.z.y;

            verts[iVert].norm.Set(0.0f, 0.0f, 1.0f);

            Vector3 &n = verts[iVert].norm;
            float nx = n.x, ny = n.y, nz = n.z;
            n.x = nx * orientMtx.x.x + nz * orientMtx.z.x + ny * orientMtx.y.x;
            n.z = ny * orientMtx.y.z + nx * orientMtx.x.z + nz * orientMtx.z.z;
            n.y = ny * orientMtx.y.y + nx * orientMtx.x.y + nz * orientMtx.z.y;

            verts[iVert].color.red = oneMinusT;
            verts[iVert].color.green = oneMinusT;
            verts[iVert].color.blue = oneMinusT;
            verts[iVert].color.alpha = oneMinusT;
            verts[iVert].tex.Set(absSegFrac, t);
            iVert++;
        }
    }
    MILO_ASSERT(iVert == kNumVerts, 0x526);

    int iFace = 0;
    int rowStart = 0;
    for (int row = 0; row < numSections; row++) {
        for (int col = 0; col < numSegments; col++) {
            short base = (short)(rowStart + col);
            short next = base + 1;
            short baseNext = base + (short)numCols;
            short nextNext = baseNext + 1;
            if ((iFace & 2) == 0) {
                faces[iFace].Set(base, next, baseNext);
                faces[iFace + 1].Set(baseNext, next, nextNext);
            } else {
                faces[iFace].Set(baseNext, base, nextNext);
                faces[iFace + 1].Set(nextNext, base, next);
            }
            iFace += 2;
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
    float bottomRadius = def.mBottomRadius;
    float topRadius = def.mLength;

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

    int iFace = 0;
    for (int row = 0; row < nMinus1; row++) {
        for (int col = 0; col < nMinus1; col++) {
            short base = (short)(row + 1 + col * n);
            unsigned short uBase = (unsigned short)base;
            unsigned short uPrev = (unsigned short)(base - 1);
            unsigned short uBaseN = (unsigned short)(base + (short)n - 1);
            unsigned short uBasePN = (unsigned short)(base + (short)n);
            if (!((iFace & 2) == 0)) {
                faces[iFace].v1 = uBaseN;
                faces[iFace].v2 = uPrev;
                faces[iFace].v3 = uBasePN;
                faces[iFace + 1].v1 = uBasePN;
                faces[iFace + 1].v2 = uPrev;
                faces[iFace + 1].v3 = uBase;
            } else {
                faces[iFace].v1 = uPrev;
                faces[iFace].v2 = uBase;
                faces[iFace].v3 = uBaseN;
                faces[iFace + 1].v1 = uBaseN;
                faces[iFace + 1].v2 = uBase;
                faces[iFace + 1].v3 = uBasePN;
            }
            iFace += 2;
        }
    }

    def.mBeam->Sync(0x13F);
    def.mBeam->SetMat(def.mMat);
    def.mBeam->SetTransConstraint(constraint, nullptr, false);
    RndTransformable *parent = this ? static_cast<RndTransformable *>(this) : nullptr;
    def.mBeam->SetTransParent(parent, false);
}
