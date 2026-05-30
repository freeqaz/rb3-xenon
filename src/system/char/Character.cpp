#include "char/Character.h"
#include "CharInterest.h"
#include "obj/ObjPtrVec_impl.h"
#include "Waypoint.h"
#include "char/CharEyes.h"
#include "char/CharPollable.h"
#include "char/CharServoBone.h"
#include "char/CharacterTest.h"
#include "char/CharUtl.h"
#include "char/Waypoint.h"
#include "math/Geo.h"
#include "math/Mtx.h"
#include "math/Utl.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/System.h"
#include "os/Timer.h"
#include "rndobj/Anim.h"
#include "rndobj/Cam.h"
#include "rndobj/Rnd.h"
#include "rndobj/Rnd_NG.h"
#include "rndobj/ShadowMap.h"
#include "rndobj/Dir.h"
#include "rndobj/Draw.h"
#include "rndobj/Env.h"
#include "rndobj/Group.h"
#include "rndobj/Mesh.h"
#include "rndobj/Trans.h"
#include "rndobj/Utl.h"
#include "hamobj/RhythmDetector.h"
#include "utl/BinStream.h"
#include "utl/Loader.h"
#include "utl/MemMgr.h"
#include "utl/Std.h"
#include "utl/Symbol.h"
#include <string.h>
#include <algorithm>
#include <list>

Character *Character::sCurrent;
Character *gCharMe;

// declaration goes here because of the MEM_OVERLOAD showing .cpp
class ShadowBone : public RndTransformable {
public:
    ShadowBone() : mParent(this) {}

    RndTransformable *Parent() const { return mParent; }
    void SetParent(RndTransformable *parent) { mParent = parent; }

    MEM_OVERLOAD(ShadowBone, 0x29)
private:
    ObjPtr<RndTransformable> mParent; // 0x90
};

#pragma region Hmx::Object

Character::Character()
    : mLods(this), mLastLod(0), mForceLod(kLOD0), mShadow(this), mTranslucent(this),
      mDriver(0), mSelfShadow(0), mSpotCutout(0), mFloorShadow(1), mSphereBase(this, this),
      mBounding(Vector3(0, 0, 0), 0), mPollState(kCharCreated),
      mTest(new CharacterTest(this)), mFrozen(0), mDrawMode(kCharDrawAll), mTeleported(1),
      unk2a0(this), mShowableProps(this), mDebugDrawInterestObjects(false) {}

Character::~Character() {
    UnhookShadow();
    delete mTest;
}

bool Character::Replace(ObjRef *from, Hmx::Object *to) {
    if (RefIs(from, mSphereBase)) {
        Hmx::Object *obj = mSphereBase.SetObj(to);
        if (!obj) {
            mSphereBase = this;
        }
        return true;
    } else {
        return RndDir::Replace(from, to);
    }
}

BEGIN_HANDLERS(Character)
    HANDLE_ACTION(teleport, Teleport(_msg->Obj<Waypoint>(2)))
    HANDLE(play_clip, OnPlayClip)
    HANDLE_ACTION(calc_bounding_sphere, CalcBoundingSphere())
    HANDLE(copy_bounding_sphere, OnCopyBoundingSphere)
    HANDLE_ACTION(merge_draws, MergeDraws(_msg->Obj<Character>(2)))
    HANDLE_ACTION(find_interest_objects, FindInterestObjects(_msg->Obj<ObjectDir>(2)))
    HANDLE_ACTION(force_interest, SetFocusInterest(_msg->Obj<CharInterest>(2), 0))
    HANDLE_ACTION(force_interest_named, SetFocusInterest(_msg->Sym(2), 0))
    HANDLE_ACTION_IF_ELSE(
        enable_blink,
        _msg->Size() > 3,
        EnableBlinks(_msg->Int(2), _msg->Int(3)),
        EnableBlinks(_msg->Int(2), false)
    )
    HANDLE(list_interest_objects, OnGetCurrentInterests)
    HANDLE_MEMBER_PTR(mTest)
    HANDLE_SUPERCLASS(RndDir)
END_HANDLERS

BEGIN_CUSTOM_PROPSYNC(Character::Lod)
    SYNC_PROP(screen_size, o.mScreenSize)
    SYNC_PROP(opaque, o.mOpaque)
    SYNC_PROP(translucent, o.mTranslucent)
END_CUSTOM_PROPSYNC

BEGIN_PROPSYNCS(Character)
    SYNC_PROP_SET(
        sphere_base, mSphereBase.Ptr(), SetSphereBase(_val.Obj<RndTransformable>())
    )
    SYNC_PROP(lods, mLods)
    SYNC_PROP(force_lod, (int &)mForceLod)
    SYNC_PROP(translucent, mTranslucent)
    SYNC_PROP(self_shadow, mSelfShadow)
    SYNC_PROP(bounding, mBounding)
    SYNC_PROP(frozen, mFrozen)
    SYNC_PROP(shadow, mShadow)
    SYNC_PROP_SET(driver, mDriver, )
    SYNC_PROP_MODIFY(
        interest_to_force, mInterestToForce, SetFocusInterest(mInterestToForce, 0)
    )
    SYNC_PROP(showable_props, mShowableProps)
    SYNC_PROP(debug_draw_interest_objects, mDebugDrawInterestObjects)
    SYNC_PROP(CharacterTesting, *mTest)
    SYNC_SUPERCLASS(RndDir)
END_PROPSYNCS

template <>
BinStream &operator<<(BinStream &bs, const ObjPtrVec<RhythmDetector, ObjectDir> &c) {
    bs << (int)c.size();
    MILO_ASSERT(c.Owner(), 0x525);
    for (ObjPtrVec<RhythmDetector, ObjectDir>::const_iterator it = c.begin(); it != c.end(); ++it) {
        const RhythmDetector *obj = it->Obj();
        const char *name = obj ? obj->Name() : "";
        bs << name;
    }
    return bs;
}

BinStream &operator<<(BinStream &bs, const Character::Lod &lod) {
    bs << lod.mScreenSize;
    bs << lod.mOpaque;
    bs << lod.mTranslucent;
    return bs;
}

BEGIN_SAVES(Character)
    SAVE_REVS(0x15, 0)
    SAVE_SUPERCLASS(RndDir)
    if (!IsProxy()) {
        bs << mLods;
        bs << mShadow;
        bs << mSelfShadow;
        ObjPtr<RndTransformable> ptr(this);
        ptr = mSphereBase;
        bs << ptr;
        bs << mBounding;
        bs << mFrozen;
        bs << mForceLod;
        bs << mTranslucent;
        bs << mShowableProps;
    }
    mTest->Save(bs);
END_SAVES

BEGIN_COPYS(Character)
    COPY_SUPERCLASS(RndDir)
    CREATE_COPY(Character)
    BEGIN_COPYING_MEMBERS
        if (ty != kCopyFromMax) {
            COPY_MEMBER(mLastLod)
            COPY_MEMBER(mForceLod)
            COPY_MEMBER(mDriver)
            COPY_MEMBER(mSelfShadow)
            SetSphereBase(c->mSphereBase);
            COPY_MEMBER(mFrozen)
            COPY_MEMBER(mForceLod)
            COPY_MEMBER(mLods)
            COPY_MEMBER(mTranslucent)
            COPY_MEMBER(mShadow)
            COPY_MEMBER(mShowableProps)
        }
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(0x15, 0)

void Character::PreLoad(BinStream &bs) {
    LOAD_REVS(bs)
    ASSERT_REVS(0x15, 0)
    if (d.rev > 1) {
        RndDir::PreLoad(bs);
        if (d.rev < 7) {
            SetRate(k1_fpb);
        }
    } else {
        int revToPush;
        d >> revToPush;
        if (revToPush > 3) {
            RndTransformable::Load(bs);
            RndDrawable::Load(bs);
        }
        ObjectDir::PreLoad(bs);
        bs.PushRev(revToPush, this);
    }
    d.PushRev(this);
}

void OldGroupLoad(DrawPtrVec &vec, BinStream &bs) {
    vec.clear();
    ObjPtr<RndGroup> group(vec.Owner());
    bs >> group;
    if (group) {
        vec.push_back(group);
    }
}

BinStreamRev &operator>>(BinStreamRev &d, Character::Lod &lod) {
    d >> lod.mScreenSize;
    if (d.rev < 6) {
        lod.mScreenSize *= (4.0f / 3.0f);
    }
    if (gCharMe) {
        d >> lod.mOpaque;
    } else if (d.rev < 0x12) {
        OldGroupLoad(lod.mOpaque, d.stream);
        if (d.rev > 0xD) {
            OldGroupLoad(lod.mTranslucent, d.stream);
        }
    } else {
        d >> lod.mOpaque;
        d >> lod.mTranslucent;
    }
    return d;
}

void Character::PostLoad(BinStream &bs) {
    BinStreamRev d(bs, bs.PopRev(this));
    if (d.rev > 1) {
        RndDir::PostLoad(bs);
        if (d.rev < 4 || !IsProxy()) {
            if (d.rev < 9) {
                ObjVector<ObjVector<Lod> > lods(this);
                d >> lods;
                if (lods.size() != 0)
                    mLods = lods[0];
                else
                    mLods.clear();
            } else {
                d >> mLods;
            }
            if (d.rev < 0x12) {
                OldGroupLoad(mShadow, bs);
            } else {
                bs >> mShadow;
            }
            if (d.rev > 2) {
                d >> mSelfShadow;
            } else {
                mSelfShadow = false;
            }
            if (d.rev > 4) {
                ObjPtr<RndTransformable> tPtr(this, 0);
                bs >> tPtr;
                RndTransformable *loadedPtr = tPtr.Ptr();
                if (loadedPtr) {
                    mSphereBase = loadedPtr;
                } else {
                    mSphereBase = this;
                }
            } else {
                mSphereBase = this;
            }
            if (d.rev > 0xA) {
                d >> mBounding;
            } else {
                mBounding.Zero();
            }
            if (d.rev < 0xC) {
                if (mSphereBase == this) {
                    if (mBounding.GetRadius() == 0) {
                        if (GetSphere().GetRadius() != 0) {
                            Multiply(GetSphere(), mSphereBase->WorldXfm(), mBounding);
                        }
                    }
                }
            }
            if (d.rev > 0xC) {
                d >> mFrozen;
            }
            if (d.rev > 0xE) {
                d >> (int &)mForceLod;
            }
            if (d.rev > 0x10) {
                if (d.rev < 0x12) {
                    OldGroupLoad(mTranslucent, bs);
                } else {
                    d >> mTranslucent;
                }
            }
            if (d.rev == 0x13 || d.rev == 0x14) {
                ObjPtrVec<RndGroup> vec(this);
                d >> vec;
            } else if (d.rev > 0x14) {
                d >> mShowableProps;
            }
            if (d.rev > 9) {
                mTest->Load(bs);
            }
        } else if (d.rev > 0xF) {
            mTest->Load(bs);
        }
    } else {
        int otherRev = bs.PopRev(this);
        ObjectDir::PostLoad(bs);
        if (otherRev > 4) {
            bs >> mEnv;
        }
        if (otherRev > 3) {
            gCharMe = otherRev < 6 ? this : nullptr;
            ObjVector<ObjVector<Character::Lod> > lods(this);
            d >> lods;
            if (lods.size() != 0)
                mLods = lods[0];
            else
                mLods.clear();
        } else {
            mLods.clear();
        }
        if (otherRev > 6) {
            if (d.rev < 0x12) {
                OldGroupLoad(mShadow, bs);
            } else {
                d >> mShadow;
            }
        }
    }
    if (d.rev < 8) {
        float rad = GetSphere().GetRadius();
        for (int i = 0; i < mLods.size(); i++) {
            mLods[i].mScreenSize /= rad;
        }
    }
}

#pragma endregion
#pragma region RndDrawable

void Character::UpdateSphere() {
    Sphere s78 = mBounding;
    Transform tf38;
    FastInvert(WorldXfm(), tf38);
    Transform tf68;
    Multiply(mSphereBase->WorldXfm(), tf38, tf68);
    FastInvert(tf68, tf68);
    Multiply(s78, tf68, s78);
    SetSphere(s78);
}

void Character::DrawShadow(const Transform &xfm, float planeD) {
    if (mShowing && !mShadow.empty()) {
        Vector3 worldPos = WorldXfm().v;

        Plane pl70;
        pl70.Set(0, 0, 1, -(worldPos.z + planeD));

        MILO_ASSERT(GetGfxMode() == kOldGfx, 0x2E7);
        Transform tf40;
        Transpose(xfm, tf40);
        Plane plb0;
        Multiply(pl70, tf40, plb0);

        Transform tf90;
        float scale = -1.0f / plb0.b;
        tf90.m.Set(1, plb0.a * scale, 0, 0, 0, 0, 0, plb0.c * scale, 1);
        tf90.v.Set(0, plb0.d * scale, 0);

        Transform tfa0;
        Multiply(tf40, tf90, tfa0);
        Multiply(tfa0, xfm, tfa0);

        for (int i = 0; i < mShadowBones.size(); i++) {
            ShadowBone *cur = mShadowBones[i];
            Multiply(cur->Parent()->WorldXfm(), tfa0, cur->DirtyLocalXfm());
        }

        mShadow.Draw();
    }
}

RndDrawable *Character::CollideShowing(const Segment &s, float &fl, Plane &pl) {
    Vector3 v70;
    RndDrawable *ret = nullptr;
    Segment mySegment = s;
    fl = 1;
    if (mLastLod >= 0 && mLastLod < mLods.size()) {
        RndDrawable *lodShowing = mLods[mLastLod].mOpaque.CollideShowing(s, v70.x, pl);
        if (lodShowing) {
            if (IsProxy()) {
                fl = v70.x;
                return this;
            }
            ret = lodShowing;
            float oldX = v70.x;
            Interp(mySegment.start, mySegment.end, v70.x, v70);
            fl *= oldX;
        }
    }
    RndDrawable *rndDirShowing = RndDir::CollideShowing(mySegment, v70.x, pl);
    if (rndDirShowing) {
        fl *= v70.x;
        ret = rndDirShowing;
    }
    return ret;
}

#pragma endregion
#pragma region RndPollable

void Character::Poll() {
    START_AUTO_TIMER("char_poll");
    AutoSetCurrentCharacter scope(this);
    if (mFrozen)
        return;
    else {
        if (TheLoadMgr.EditMode()) {
            mTest->Poll();
        }
        RndDir::Poll();
        if (mShowing) {
            mTeleported = false;
        }
        mPollState = kCharPolled;
    }
}

void Character::Enter() {
    AutoSetCurrentCharacter scope(this);
    mFrozen = false;
    mPollState = kCharEntered;
    mForceLod = kLODPerFrame;
    mLastLod = 0;
    mTeleported = true;
    mInterestToForce = Symbol();
    RndDir::Enter();
}

void Character::Exit() {
    AutoSetCurrentCharacter scope(this);
    mPollState = kCharExited;
    RndDir::Exit();
}

#pragma endregion
#pragma region ObjectDir

void Character::SyncObjects() {
    mPollState = kCharSyncObject;
    RndMesh *mesh = Find<RndMesh>("bone_pelvis.mesh", false);
    if (mesh) {
        ConvertBonesToTranses(this, false);
    }
    RndDir::SyncObjects();
    if (!IsSubDir()) {
        RemoveFromDraws(mTranslucent);
        for (int i = 0; i < mLods.size(); i++) {
            RemoveFromDraws(mLods[i].mOpaque);
            RemoveFromDraws(mLods[i].mTranslucent);
        }
        SyncShadow();
        CharPollableSorter sorter;
        sorter.Sort(mPolls);
    }
}

void Character::AddedObject(Hmx::Object *o) {
    if (dynamic_cast<CharPollable *>(o)) {
        CharDriver *driver = dynamic_cast<CharDriver *>(o);
        if (driver) {
            if (streq(driver->Name(), "main.drv")) {
                mDriver = driver;
            }
        }
    }
    ObjectDir::AddedObject(o);
}

void Character::RemovingObject(Hmx::Object *o) {
    if (o == mDriver) {
        mDriver = nullptr;
    }
    RndDir::RemovingObject(o);
}

#pragma endregion
#pragma region Character Virtual Methods

void Character::CollideListSubParts(
    const Segment &s, std::list<RndDrawable::Collision> &c
) {
    if (mLastLod >= 0 && mLastLod < mLods.size()) {
        mLods[mLastLod].mOpaque.CollideList(s, c);
    }
    RndDir::CollideListSubParts(s, c);
}

void Character::Teleport(Waypoint *wp) {
    if (wp) {
        Transform worldXfm = wp->WorldXfm();
        Normalize(worldXfm.m, worldXfm.m);
        SetLocalXfm(worldXfm);
    }
    if (BoneServo()) {
        BoneServo()->SetRegulateWaypoint(wp);
    }
    mTeleported = true;
}

void Character::CalcBoundingSphere() {
    Transform tf50(mLocalXfm);
    DirtyLocalXfm().Reset();
    mBounding.Zero();
    static const char *boneNames[5] = { "bone_head.mesh",
                                        "bone_R-ankle.mesh",
                                        "bone_L-ankle.mesh",
                                        "bone_R-toe.mesh",
                                        "bone_L-toe.mesh" };
    for (int i = 0; i <= 4; i++) {
        RndTransformable *t = Find<RndTransformable>(boneNames[i], false);
        if (t) {
            mBounding.GrowToContain(Sphere(t->WorldXfm().v, 0.1));
        }
    }

    RndTransformable *transLClavicle = CharUtlFindBoneTrans("bone_L-clavicle", this);
    if (transLClavicle) {
        RndTransformable *transLHand = CharUtlFindBoneTrans("bone_L-hand", this);
        if (transLHand) {
            Vector3 vClavicle = transLClavicle->WorldXfm().v;
            vClavicle.z += Distance(vClavicle, transLHand->WorldXfm().v);
            mBounding.GrowToContain(Sphere(vClavicle, 7.0f));
        }
    }
    RndTransformable *transRClavicle = CharUtlFindBoneTrans("bone_R-clavicle", this);
    if (transRClavicle) {
        RndTransformable *transRHand = CharUtlFindBoneTrans("bone_R-hand", this);
        if (transRHand) {
            Vector3 vClavicle = transRClavicle->WorldXfm().v;
            auto dist = Distance(vClavicle, transRHand->WorldXfm().v);
            vClavicle.z += dist;
            mBounding.GrowToContain(Sphere(vClavicle, 7.0f));
        }
    }
    if (mBounding.GetRadius() == 0) {
        for (ObjDirItr<RndTransformable> it(this, true); it != nullptr; ++it) {
            if (strneq(it->Name(), "bone_", 5) || strneq(it->Name(), "spot_", 5)) {
                mBounding.GrowToContain(Sphere(it->WorldXfm().v, 0.1));
            }
            RndMesh *mesh = dynamic_cast<RndMesh *>(&*it);
            if (mesh) {
                if (mesh->Showing()) {
                for (int i = 0; i < mesh->Verts().size(); i++) {
                    mBounding.GrowToContain(
                        Sphere(mesh->SkinVertex(mesh->Verts(i), nullptr), 0.001f)
                    );
                }
            }
            }
        }
    }
    UpdateSphere();
    DirtyLocalXfm() = tf50;
}

bool Character::MakeWorldSphere(Sphere &s, bool b) {
    if (mSphere.GetRadius()) {
        Multiply(mSphere, mSphereBase->WorldXfm(), s);
        return true;
    } else
        return false;
}

float Character::ComputeScreenSize(RndCam *cam) {
    Sphere sphere;
    MakeWorldSphere(sphere, false);
    if (cam && sphere > cam->WorldFrustum()) {
        return 0;
    } else {
        sphere.radius = 1.0f;
        if (cam)
            return cam->CalcScreenHeight(sphere);
        else
            return 1;
    }
}

void Character::DrawOpaque() {
    FOREACH (it, mDraws) {
        (*it)->Draw();
    }
    Lod *lod = mLods.size() != 0 ? &mLods[mLastLod] : nullptr;
    if (lod) {
        lod->mOpaque.Draw();
    }
}

void Character::DrawTranslucent() {
    mTranslucent.Draw();
    Lod *lod = mLods.size() != 0 ? &mLods[mLastLod] : nullptr;
    if (lod) {
        lod->mTranslucent.Draw();
    }
}

CharEyes *Character::GetEyes() { return Find<CharEyes>("CharEyes.eyes", false); }


void Character::SetInterestFilterFlags(int i) {
    CharEyes *eyes = GetEyes();
    if (eyes) {
        eyes->SetInterestFilterFlags(i);
        eyes->SetEnabled(true);
    }
}

void Character::ClearInterestFilterFlags() {
    CharEyes *eyes = GetEyes();
    if (eyes) {
        eyes->ClearInterestFilterFlags();
    }
}

#pragma endregion
#pragma region Character Methods

void Character::Init() { REGISTER_OBJ_FACTORY(Character) }
void Character::Terminate() {}

ShadowBone *Character::AddShadowBone(RndTransformable *trans) {
    if (!trans)
        return 0;
    else {
        for (int i = 0; i < mShadowBones.size(); i++) {
            if (mShadowBones[i]->Parent() == trans)
                return mShadowBones[i];
        }
        mShadowBones.push_back(new ShadowBone());
        mShadowBones.back()->SetParent(trans);
        return mShadowBones.back();
    }
}

void Character::ForceBlink() {
    CharEyes *eyes = GetEyes();
    if (eyes)
        eyes->ForceBlink();
}

void Character::EnableBlinks(bool b1, bool b2) {
    CharEyes *eyes = GetEyes();
    if (eyes)
        eyes->SetEnableBlinks(b1, b2);
}

void Character::SetDebugDrawInterestObjects(bool b) { mDebugDrawInterestObjects = b; }

CharServoBone *Character::BoneServo() {
    if (mDriver)
        return dynamic_cast<CharServoBone *>(mDriver->GetBones());
    else
        return nullptr;
}

DataNode Character::OnCopyBoundingSphere(DataArray *da) {
    Character *c = da->Obj<Character>(2);
    if (c)
        CopyBoundingSphere(c);
    return 0;
}

void Character::MergeDraws(const Character *c) {
#ifdef HX_NATIVE
    if (!c) return;
#endif
    MILO_ASSERT(c, 0x57D);
    int numLods = Max<int>(c->mLods.size(), mLods.size());
    mLods.resize(numLods);
    for (int i = 0; i < c->mLods.size(); i++) {
        mLods[i].mOpaque.merge(c->mLods[i].mOpaque);
        mLods[i].mTranslucent.merge(c->mLods[i].mTranslucent);
    }
    mTranslucent.merge(c->mTranslucent);
    mShadow.merge(c->mShadow);
    mShowableProps.merge(c->mShowableProps);
}

void Character::SetInterestObjects(
    const ObjPtrList<CharInterest> &interests, ObjectDir *dir
) {
    CharEyes *eyes = GetEyes();
    if (eyes) {
        eyes->ClearAllInterestObjects();
        FOREACH (it, interests) {
            if (ValidateInterest(*it, dir ? dir : (*it)->Dir()))
                eyes->AddInterestObject(*it);
        }
    }
}

bool Character::SetFocusInterest(CharInterest *interest, int i) {
    CharEyes *eyes = GetEyes();
    if (eyes) {
        return eyes->SetFocusInterest(interest, i);
    }
    return false;
}

bool Character::SetFocusInterest(Symbol symbol, int i) {
    CharEyes *eyes = GetEyes();
    if (eyes) {
        CharInterest *interest = nullptr;
        int count = eyes->mInterests.size();
        for (int idx = 0; idx < count; idx++) {
            CharInterest *ci =
                (unsigned int)idx >= eyes->mInterests.size()
                    ? 0
                    : (CharInterest *)eyes->mInterests[idx].mInterest;
            if (symbol == ci->Name()) {
                interest = (unsigned int)idx >= eyes->mInterests.size()
                    ? 0
                    : (CharInterest *)eyes->mInterests[idx].mInterest;
                break;
            }
        }
        if (!symbol.Null() && !interest) {
            MILO_NOTIFY("Couldn't find interest named %s to force on %s", symbol.Str(), Name());
        }
        return SetFocusInterest(interest, i);
    }
    return false;
}

void Character::SetSphereBase(RndTransformable *trans) {
    if (!trans)
        trans = this;
    Sphere s18;
    MakeWorldSphere(s18, false);
    MultiplyTranspose(s18.center, trans->WorldXfm(), s18.center);
    SetSphere(s18);
    mSphereBase = trans;
}

void Character::CopyBoundingSphere(Character *c) {
    MILO_ASSERT(c, 0x46D);
    SetSphere(c->mSphere);
    mBounding = c->mBounding;
    SetSphereBase(c->mSphereBase);
}

void Character::RemoveFromDraws(DrawPtrVec &vec) {
    FOREACH (it, vec) {
        RndDrawable *cur = *it;
        VectorRemove(mDraws, cur);
    }
}

DataNode Character::OnPlayClip(DataArray *msg) {
    if (mDriver) {
        int playint = msg->Size() > 3 ? msg->Int(3) : 4;
        MILO_ASSERT(msg->Size()<=4, 0x5CF);
        return mDriver->Play(msg->Node(2), playint, -1, kHugeFloat, 0) != nullptr;
    } else
        return 0;
}

DataNode Character::OnGetCurrentInterests(DataArray *da) {
    int size = 0;
    CharEyes *eyes = GetEyes();
    if (eyes)
        size = eyes->mInterests.size();
    DataArrayPtr ptr;
    ptr->Resize(size + 1);
    ptr->Node(0) = Symbol();
    for (int i = 0; i < size; i++) {
        ptr->Node(i + 1) = Symbol(((unsigned int)i >= eyes->mInterests.size()
            ? (CharInterest *)0
            : (CharInterest *)eyes->mInterests[i].mInterest)->Name());
    }
    return ptr;
}

void Character::DrawShowing() {
    START_AUTO_TIMER("char_draw");
    if (mDebugDrawInterestObjects) {
        CharEyes *eyes = GetEyes();
        if (eyes)
            eyes->Highlight();
    }
    float screenSize = ComputeScreenSize(RndCam::Current());
    int lod;
    if (mForceLod < 0) {
        for (lod = 0; (int)lod < (int)mLods.size() - 1; lod++) {
            float hysteresis;
            if (lod < mLastLod)
                hysteresis = 0.09f;
            else
                hysteresis = -0.09f;
            if (screenSize >= (hysteresis + 1.0f) * mLods[lod].mScreenSize)
                break;
        }
    } else {
        lod = Clamp<int>(0, mLods.size() - 1, mForceLod);
    }
    bool doSelfShadow = mSelfShadow && TheRnd.GetDrawMode() == 0 && lod <= 1 && (mDrawMode & 1);
    if (doSelfShadow) {
        if (GetGfxMode() == kNewGfx) {
            if (TheNgRnd.Offscreen())
                doSelfShadow = false;
        } else {
            doSelfShadow = false;
        }
    }
    if (doSelfShadow) {
        int savedForceLod = mForceLod;
        mForceLod = (LODType)lod;
        RndShadowMap::PrepShadow(this, mEnv);
        mForceLod = (LODType)savedForceLod;
    }
    DrawLod(lod);
    if (doSelfShadow)
        RndShadowMap::EndShadow();

    if (TheLoadMgr.EditMode() && TheRnd.GetDrawMode() == 0) {
        mTest->Draw();
    }

    static DataNode *sShowName = &DataVariable(Symbol("character.show_name"));

    if (sShowName->Int()) {
        static Symbol sCrowd("crowd");
        if (Type() != sCrowd) {
            RndTransformable *bone = CharUtlFindBoneTrans("bone_head", this);
            if (bone) {
                const Transform &xfm = bone->WorldXfm();
                Vector3 worldPos = xfm.v;
                worldPos.z += 24.0f;
                Hmx::Color white(1.0f, 1.0f, 1.0f, 1.0f);
                Vector2 screenPos;
                RndCam::Current()->WorldToScreen(worldPos, screenPos);
                screenPos.x *= TheRnd.Width();
                screenPos.y *= TheRnd.Height();
                const Vector2 &extent = TheRnd.DrawString(Name(), screenPos, white, false);
                screenPos.x = screenPos.x - (extent.x - screenPos.x) * 0.5f;
                TheRnd.DrawString(Name(), screenPos, white, true);
            }
        }
    }
}

void Character::FindInterestObjects(ObjectDir *dir) {
    if (dir) {
        Timer timer;
        timer.Restart();
        CharEyes *eyes = GetEyes();
        if (eyes) {
            eyes->ClearAllInterestObjects();
            for (ObjDirItr<CharInterest> it(dir, true); it != nullptr; ++it) {
                if (ValidateInterest(it, dir)) {
                    eyes->AddInterestObject(it);
                }
            }
            for (ObjDirItr<Character> it(dir, true); it != nullptr; ++it) {
                if (!streq(it->Name(), Name())) {
                    for (ObjDirItr<CharInterest> it2(it, true); it2 != nullptr; ++it2) {
                        if (ValidateInterest(it2, it)) {
                            eyes->AddInterestObject(it2);
                        }
                    }
                }
            }
        }
    }
}

void Character::UnhookShadow() {
    for (int i = 0; i < mShadowBones.size(); i++) {
        ShadowBone *cur = mShadowBones[i];
        cur->ReplaceRefs(cur->Parent());
    }
    DeleteAll(mShadowBones);
}

void Character::SyncShadow() {
    UnhookShadow();
    if (!mShadow.empty()) {
        if (GetGfxMode() == kOldGfx) {
            FOREACH (it, mShadow) {
                RndDrawable *drawable = *it;
                RndMesh *mesh = dynamic_cast<RndMesh *>(drawable);
                if (mesh) {
                    if (mesh->NumBones() != 0) {
                        for (int i = 0; i < mesh->NumBones(); i++) {
                            mesh->SetBone(i, AddShadowBone(mesh->BoneTransAt(i)), false);
                        }
                    } else {
                        mesh->SetTransParent(AddShadowBone(mesh->TransParent()), false);
                    }
                }
            }
        }
    }
}

void Character::DrawLod(int lod) {
    int rndDrawMode = TheRnd.GetDrawMode();
    unsigned char opaqueBit = mDrawMode & 1;
    if (rndDrawMode == 5)
        return;
    if (rndDrawMode == 3) {
        if (!mSpotCutout)
            return;
        if (!opaqueBit)
            return;
    }
    if (rndDrawMode == 4) {
        if (!mFloorShadow)
            return;
        if (!opaqueBit)
            return;
    }
    DrawMode charDrawMode;
    if (rndDrawMode == 3 || rndDrawMode == 4) {
        charDrawMode = (DrawMode)4;
    } else {
        bool isExtrude = (rndDrawMode == 2);
        charDrawMode = isExtrude ? kCharDrawTranslucent : mDrawMode;
    }
    DrawLodOrShadow(lod, charDrawMode);
}

void Character::DrawLodOrShadow(int lod, DrawMode drawMode) {
    mPollState = (PollState)5;
#ifdef HX_NATIVE
    if (drawMode == 4) {
        if (mShadow.size() != 0) {
            mShadow.Draw();
        }
        return;
    }

    mLastLod = Clamp<int>(0, mLods.size() - 1, lod);
    Lod *curLod = mLods.empty() ? nullptr : &mLods[mLastLod];

    if (drawMode & 1) {
        RndEnvironTracker tracker(mEnv, &WorldXfm().v);
        RndDir::DrawShowing();
        if (curLod) {
            curLod->mOpaque.Draw();
        }
        if (drawMode == 1) {
            unk2a0 = RndEnviron::Current();
            unk2b4 = RndEnviron::CurrentPos();
        }
    }

    if (drawMode & 2) {
        if (drawMode == 2) {
            RndEnvironTracker tracker(unk2a0, unk2b4);
            mTranslucent.Draw();
            if (curLod) {
                curLod->mTranslucent.Draw();
            }
        } else {
            mTranslucent.Draw();
            if (curLod) {
                curLod->mTranslucent.Draw();
            }
        }
    }
#else
    mLastLod = Clamp<int>(0, mLods.size() - 1, lod);
    if (drawMode == 4) {
        if (mShadow.size() != 0) {
            mShadow.Draw();
            return;
        }
    } else {
        bool drawNormal = (drawMode & 1) != 0;
        if (drawNormal) {
            RndEnvironTracker tracker(mEnv, &WorldXfm().v);
            DrawShowing();
            if (drawMode == 1) {
                unk2a0 = RndEnviron::Current();
                unk2b4 = RndEnviron::CurrentPos();
            }
        }
        if (drawMode & 2) {
            if (drawMode == 2) {
                RndEnvironTracker tracker(unk2a0, unk2b4);
                DrawShowing();
                return;
            }
            DrawShowing();
            return;
        }
    }
    DrawShowing();
#endif
}

void DrawPtrVec::Draw() const {
    for (const_iterator it = begin(); it != end(); ++it) {
#ifdef HX_NATIVE
        if (it->Obj())
#endif
            it->Obj()->DrawShowing();
    }
}

RndDrawable *DrawPtrVec::CollideShowing(const Segment &s, float &dist, Plane &pl) const {
    Segment seg;
    RndDrawable *result = nullptr;
    memcpy(&seg, &s, sizeof(Segment));
    dist = 1.0f;
    for (const_iterator it = begin(); it != end(); ++it) {
        float d;
        RndDrawable *hit = (*it)->Collide(seg, d, pl);
        if (hit) {
            result = hit;
            Interp(seg.start, seg.end, d, seg.end);
            dist *= d;
        }
    }
    return result;
}

// On native, these are defined in CharPollGroup.cpp (same TU on PPC via ICF)
#ifndef HX_NATIVE
int CharPollableSorter::sSearchID = 0;

void CharPollableSorter::AddDeps(
    Dep *dep,
    const std::list<Hmx::Object *> &objs,
    std::list<Dep *> &deps,
    bool isChangedBy
) {
    for (std::list<Hmx::Object *>::const_iterator it = objs.begin(); it != objs.end();
         ++it) {
        Hmx::Object *cur = *it;
        if (cur) {
            Dep *mapDep = &mDeps[cur];
            if (!mapDep->obj) {
                mapDep->obj = cur;
                deps.push_back(mapDep);
            }
            if (isChangedBy) {
                dep->changedBy.push_back(mapDep);
            } else {
                mapDep->changedBy.push_back(dep);
            }
        }
    }
}

bool CharPollableSorter::ChangedByRecurse(Dep *dep) {
    if (!dep)
        return false;
    if (dep == mTarget)
        return true;
    if (dep->searchID == sSearchID)
        return false;
    dep->searchID = sSearchID;
    for (std::list<Dep *>::iterator it = dep->changedBy.begin();
         it != dep->changedBy.end();
         ++it) {
        if (ChangedByRecurse(*it))
            return true;
    }
    return false;
}

bool CharPollableSorter::ChangedBy(Dep *a, Dep *b) {
    mTarget = b;
    sSearchID++;
    return ChangedByRecurse(a);
}

void CharPollableSorter::Sort(std::vector<RndPollable *> &polls) {
    std::vector<Dep *> deps;
    deps.reserve(polls.size());
    for (int i = polls.size() - 1, last = i; i >= 0; i--) {
        CharPollable *c = dynamic_cast<CharPollable *>(polls[i]);
        if (c) {
            Dep &dep = mDeps[c];
            dep.obj = c;
            dep.poll = c;
            deps.push_back(&dep);
        } else {
            polls[last--] = polls[i];
        }
    }
    if (deps.empty())
        return;

    std::sort(deps.begin(), deps.end(), CharPollableSorter::AlphaSort());
    std::list<Dep *> depList;
    for (int i = 0; i < deps.size(); i++)
        depList.push_back(deps[i]);
    while (!depList.empty()) {
        Dep *curDep = depList.back();
        depList.pop_back();
        CharPollable *c = dynamic_cast<CharPollable *>(curDep->obj);
        if (c) {
            std::list<Hmx::Object *> depList1;
            std::list<Hmx::Object *> depList2;
            c->PollDeps(depList1, depList2);
            AddDeps(curDep, depList1, depList, true);
            AddDeps(curDep, depList2, depList, false);
        }
        RndTransformable *t = dynamic_cast<RndTransformable *>(curDep->obj);
        if (t) {
            std::list<Hmx::Object *> tDepList;
            tDepList.push_back(t->TransParent());
            AddDeps(curDep, tDepList, depList, true);
        }
    }

    std::list<Dep *> otherDepList;
    for (int i = 0; i < deps.size(); i++) {
        Dep *curDep = deps[i];
        std::list<Dep *>::iterator it = otherDepList.begin();
        for (; it != otherDepList.end(); ++it) {
            if (ChangedBy(curDep, *it))
                break;
        }
        otherDepList.insert(it, curDep);
    }

    int idx = 0;
    for (std::list<Dep *>::iterator it = otherDepList.begin();
         it != otherDepList.end();
         ++it) {
        polls[idx++] = (*it)->poll;
    }
}
#endif // HX_NATIVE
