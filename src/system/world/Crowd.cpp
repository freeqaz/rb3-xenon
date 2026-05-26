#include "char/CharCollide.h"
#include "char/Character.h"
#include "math/Mtx.h"
#include "math/Rand.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "os/System.h"
#include "os/Timer.h"
#include "rndobj/Cam.h"
#include "rndobj/Draw.h"
#include "rndobj/Env.h"
#include "rndobj/Mat.h"
#include "rndobj/Mesh.h"
#include "rndobj/MultiMesh.h"
#include "rndobj/Poll.h"
#include "rndobj/Rnd.h"
#include "rndobj/Tex.h"
#include "rndobj/Trans.h"
#include "rndobj/Utl.h"
#include "utl/BinStream.h"
#include "utl/Loader.h"
#include "world/ColorPalette.h"
#include "world/Crowd.h"
#include "world/Crowd3DCharHandle.h"
#include <cmath>
#include <cfloat>
#ifdef HX_NATIVE
inline double __fsel(double a, double b, double c) { return a >= 0.0 ? b : c; }
#include <unordered_map>
#else
#include "xdk/LIBCMT/ppcintrinsics.h"
#endif

RndTex *gImpostorTex[kNumLods];
RndCam *gImpostorCamera;
RndMat *gImpostorMat;
int gNumCrowd;
WorldCrowd *gParent;
#ifdef HX_NATIVE
static std::unordered_map<Character *, RndTex *> sImpostorCache;
#endif

namespace {
    void GetMeshShaderFlags(RndMat *mat, std::list<unsigned int> &flags) {
        ObjRef::iterator it = mat->Refs().begin();
        ObjRef::iterator itEnd = mat->Refs().end();
        for (; it != itEnd; ++it) {
            RndMesh *mesh = dynamic_cast<RndMesh *>(it->RefOwner());
            if (mesh) {
                unsigned int flag = 0;
                flag |= mesh->IsSkinned();
                flag |= -(mesh->HasAOCalc()) & 2;
                flags.push_back(flag);
            }
        }
        flags.sort();
        flags.unique();
    }
}

const Hmx::Color &ColorPalette::GetColor(int idx) const {
    MILO_ASSERT(mColors.size(), 0x18);
    int colorIdx = idx % mColors.size();
    return mColors[colorIdx];
}

#pragma region CharDef

void WorldCrowd::CharDef::Save(BinStream &bs) const {
    bs << mChar;
    bs << mHeight;
    bs << mDensity;
    bs << mRadius;
    bs << mUseRandomColor;
}

void WorldCrowd::CharDef::Load(BinStreamRev &d) {
    d >> mChar;
    d >> mHeight;
    d >> mDensity;
    if (d.rev > 1) {
        d >> mRadius;
    }
    if (d.rev > 8) {
        d >> mUseRandomColor;
    }
}

#pragma endregion
#pragma region CharData

void WorldCrowd::CharData::Save(BinStream &bs) const { mDef.Save(bs); }

BinStream &operator<<(BinStream &bs, const WorldCrowd::CharData &cd) {
    cd.Save(bs);
    return bs;
}

BinStreamRev &operator>>(BinStreamRev &d, WorldCrowd::CharData &cd) {
    cd.mDef.Load(d);
    return d;
}

#pragma endregion
#pragma region WorldCrowd

WorldCrowd::WorldCrowd()
    : mPlacementMesh(this), mCharacters(this), mNum(0), mCrowdRotate((CrowdRotate)0), mForce3DCrowd(0),
      mShow3DOnly(0), mCharFullness(1), mFlatFullness(1), mLod(0), mEnviron(this),
      mEnviron3D(this), mFocus(this), mCharForceLod(kLODPerFrame), unkd0(0),
      mModifyStamp(0) {
    if (gNumCrowd++ == 0) {
        int w, h, bpp;
        if (GetGfxMode() == kNewGfx) {
            w = 256;
            h = 512;
            bpp = 32;
        } else {
            w = 128;
            h = 256;
            bpp = 16;
        }
        for (int i = 0; i < kNumLods; i++) {
            gImpostorTex[i] = Hmx::Object::New<RndTex>();
            gImpostorTex[i]->SetBitmap(w, h, bpp, RndTex::kRendered, true, nullptr);
        }
        RELEASE(gImpostorMat);
        RndMat *mat = Hmx::Object::New<RndMat>();
        gImpostorMat = mat;
        mat->SetUseEnv(true);
        mat->SetPreLit(false);
        mat->SetBlend(RndMat::kBlendSrc);
        mat->SetZMode(kZModeNormal);
        mat->SetAlphaCut(true);
        mat->SetAlphaThreshold(0x80);
        mat->SetTexWrap(kTexWrapClamp);
        mat->SetPerPixelLit(false);
        mat->SetPointLights(true);
        CreateAndSetMetaMat(mat);
        gImpostorCamera = Hmx::Object::New<RndCam>();
        SetMatAndCameraLod();
    }
}

WorldCrowd::~WorldCrowd() {
    Delete3DCrowdHandles();
    for (ObjList<CharData>::iterator it = mCharacters.begin(); it != mCharacters.end();
         ++it) {
        if (it->mMMesh) {
            delete it->mMMesh->Mesh();
            RELEASE(it->mMMesh);
        }
    }
    gNumCrowd--;
    if (gNumCrowd == 0) {
        for (int i = 0; i < kNumLods; i++) {
            RELEASE(gImpostorTex[i]);
        }
        RELEASE(gImpostorCamera);
        RELEASE(gImpostorMat);
#ifdef HX_NATIVE
        for (auto &pair : sImpostorCache) {
            RELEASE(pair.second);
        }
        sImpostorCache.clear();
#endif
    }
}

DataNode WorldCrowd::OnRebuild(DataArray *) { return 0; }

BEGIN_HANDLERS(WorldCrowd)
    HANDLE(rebuild, OnRebuild)
    HANDLE_ACTION(assign_random_colors, AssignRandomColors(true))
    HANDLE(iterate_frac, OnIterateFrac)
    HANDLE_ACTION(set_fullness, SetFullness(_msg->Float(2), _msg->Float(3)))
    HANDLE_SUPERCLASS(RndDrawable)
    HANDLE_SUPERCLASS(RndPollable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_CUSTOM_PROPSYNC(WorldCrowd::CharData)
    SYNC_PROP(character, o.mDef.mChar)
    SYNC_PROP(height, o.mDef.mHeight)
    SYNC_PROP(density, o.mDef.mDensity)
    SYNC_PROP(radius, o.mDef.mRadius)
END_CUSTOM_PROPSYNC

BEGIN_PROPSYNCS(WorldCrowd)
    gParent = this;
    SYNC_PROP(num, mNum)
    SYNC_PROP(placement_mesh, mPlacementMesh)
    SYNC_PROP(characters, mCharacters)
    SYNC_PROP(show_3d_only, mShow3DOnly)
    SYNC_PROP(environ, mEnviron)
    SYNC_PROP(environ_3d, mEnviron3D)
    SYNC_PROP_SET(lod, mLod, SetLod(_val.Int()))
    SYNC_PROP_SET(force_3D_crowd, mForce3DCrowd, Force3DCrowd(_val.Int()))
    SYNC_PROP(focus, mFocus)
    SYNC_PROP(char_force_lod, (int &)mCharForceLod)
    SYNC_SUPERCLASS(RndDrawable)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

void WorldCrowd::SetLod(int lod) { mLod = Clamp(0, 2, lod); }

BEGIN_SAVES(WorldCrowd)
    SAVE_REVS(0x10, 0)
    SAVE_SUPERCLASS(RndDrawable)
    bool force = mForce3DCrowd;
    Force3DCrowd(false);
    bs << mPlacementMesh << mNum << mCharacters << mEnviron;
    bs << mEnviron3D;
    FOREACH (it, mCharacters) {
        std::list<Transform> transforms;
        RndMultiMesh *mesh = it->mMMesh;
        if (mesh) {
            FOREACH (t, mesh->Instances()) {
                transforms.push_back(t->mXfm);
            }
        }
        bs << transforms;
    }
    bs << mModifyStamp;
    bs << force;
    bs << mShow3DOnly;
    bs << mFocus;
    bs << mCharForceLod;
    bs << unkd0;
    Force3DCrowd(force);
    SAVE_SUPERCLASS(RndPollable)
END_SAVES

BEGIN_COPYS(WorldCrowd)
    COPY_SUPERCLASS(RndDrawable)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(WorldCrowd)
    BEGIN_COPYING_MEMBERS
        Delete3DCrowdHandles();
        COPY_MEMBER(mPlacementMesh)
        COPY_MEMBER(mNum)
        COPY_MEMBER(mCenter)
        COPY_MEMBER(mCharFullness)
        COPY_MEMBER(mFlatFullness)
        COPY_MEMBER(mLod)
        COPY_MEMBER(mEnviron)
        COPY_MEMBER(mEnviron3D)
        COPY_MEMBER(mForce3DCrowd)
        COPY_MEMBER(mShow3DOnly)
        COPY_MEMBER(mFocus)
        COPY_MEMBER(mCharForceLod)
        COPY_MEMBER(unkd0)

        mCharacters.clear();
        mCharacters.resize(c->mCharacters.size());
        ObjList<CharData>::const_iterator j = c->mCharacters.begin();
        ObjList<CharData>::iterator i = mCharacters.begin();
        for (; i != mCharacters.end(); ++i, ++j) {
            i->mDef = j->mDef;
            i->mBackup = j->mBackup;
            i->m3DChars = j->m3DChars;
            i->m3DCharsCreated = j->m3DCharsCreated;
        }
        CreateMeshes();
        j = c->mCharacters.begin();
        for (ObjList<CharData>::iterator i = mCharacters.begin(); i != mCharacters.end();
             ++i, ++j) {
            if (i->mMMesh) {
                MILO_ASSERT(j->mMMesh, 0x1DD);
                i->mMMesh->Instances() = j->mMMesh->Instances();
            }
        }
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(0x10, 0)

BEGIN_LOADS(WorldCrowd)
    LOAD_REVS(bs)
    ASSERT_REVS(0x10, 0)
    LOAD_SUPERCLASS(RndDrawable)
    Reset3DCrowd();
    d >> mPlacementMesh;
    if (d.rev < 3) {
        int x;
        d >> x;
    }
    d >> mNum;
    if (d.rev < 8) {
        bool b;
        d >> b;
    }
    d >> mCharacters;
    if (d.rev > 6) {
        d >> mEnviron;
    }
    if (d.rev > 9) {
        d >> mEnviron3D;
    } else {
        mEnviron3D = mEnviron;
    }
    if (d.rev > 1) {
        CreateMeshes();
        FOREACH (it, mCharacters) {
            if (d.rev < 0xE) {
                std::list<Transform> xfmList;
                std::list<RndMultiMesh::Instance> instancesList;
                std::list<OldMMInst> oldmmiList;
                if (it->mMMesh) {
                    if (d.rev < 9) {
                        d >> xfmList;
                        it->mMMesh->Instances().clear();
                        FOREACH (transIt, xfmList) {
                            it->mMMesh->Instances().push_back(
                                RndMultiMesh::Instance(*transIt)
                            );
                        }
                    } else if (d.rev < 0xB) {
                        d >> oldmmiList;
                        FOREACH (mmiIt, oldmmiList) {
                            OldMMInst &old = *mmiIt;
                            it->mMMesh->Instances().push_back(
                                RndMultiMesh::Instance(old.mOldXfm)
                            );
                        }
                    } else {
                        InstanceList &instances = it->mMMesh->Instances();
                        unsigned int count;
                        d >> count;
                        instances.resize(count);
                        FOREACH (instIt, instances) {
                            instIt->LoadRev(d.stream, 3);
                        }
                    }
                } else if (d.rev > 3) {
                    if (d.rev < 9)
                        d >> xfmList;
                    else if (d.rev < 0xB)
                        d >> oldmmiList;
                    else
                        d >> instancesList;
                }
            } else {
                std::list<Transform> xfms;
                d >> xfms;
                if (it->mMMesh) {
                    it->mMMesh->Instances().clear();
                    FOREACH (xfmIt, xfms) {
                        it->mMMesh->Instances().push_back(RndMultiMesh::Instance(*xfmIt));
                    }
                }
            }
            AssignRandomColors(false);
        }
    } else {
        OnRebuild(nullptr);
    }
    if (d.rev > 4) {
        d >> mModifyStamp;
    }
    if (d.rev > 0xC) {
        bool force = false;
        d >> force;
#ifdef HX_NATIVE
        // On native we only have billboard rendering (no 3D character RTT pipeline).
        // Force3DCrowd(true) moves all instances from MultiMesh to m3DChars,
        // emptying the billboard instance list. Always use billboard path instead.
        Force3DCrowd(false);
#else
        Force3DCrowd(force);
#endif
    }
    if (d.rev > 5) {
        d >> mShow3DOnly;
    }
    if (d.rev > 0xB) {
        d >> mFocus;
    }
    if (d.rev > 0xE) {
        d >> (int &)mCharForceLod;
    }
    if (d.rev > 0xF) {
        d >> unkd0;
    }
    if (d.rev > 0) {
        LOAD_SUPERCLASS(RndPollable);
    }
END_LOADS

void RndMultiMesh::UpdateSphere() {
    Sphere s;
    MakeWorldSphere(s, true);
    SetSphere(s);
}

void WorldCrowd::UpdateSphere() {
    Sphere s;
    MakeWorldSphere(s, true);
    SetSphere(s);
}

float WorldCrowd::GetDistanceToPlane(const Plane &p, Vector3 &vout) {
    if (mCharacters.empty())
        return 0;
    else {
        float dist = 0;
        bool b1 = true;
        FOREACH (it, mCharacters) {
            RndMultiMesh *multimesh = it->mMMesh;
            if (multimesh) {
                Vector3 v4c;
                float f5 = multimesh->GetDistanceToPlane(p, v4c);
                if (b1 || (std::fabs(f5) < std::fabs(dist))) {
                    b1 = false;
                    vout = v4c;
                    dist = f5;
                }
            }
        }
        return dist;
    }
}

bool WorldCrowd::MakeWorldSphere(Sphere &s, bool b) {
    if (b) {
        s.Zero();
        FOREACH (it, mCharacters) {
            RndMultiMesh *multimesh = it->mMMesh;
            if (multimesh) {
                Sphere local;
                multimesh->MakeWorldSphere(local, true);
                s.GrowToContain(local);
            }
        }
        return true;
    } else if (mSphere.GetRadius()) {
        s = mSphere;
        return true;
    } else
        return false;
}

void WorldCrowd::ListDrawChildren(std::list<RndDrawable *> &draws) {
    FOREACH (it, mCharacters) {
        Character *curChar = it->mDef.mChar;
        if (curChar)
            draws.push_back(curChar);
    }
}

void WorldCrowd::CollideList(const Segment &seg, std::list<Collision> &colls) {
    if (TheLoadMgr.EditMode() && CollideSphere(seg)) {
        ObjList<CharData>::iterator end = mCharacters.end();
        ObjList<CharData>::iterator it = mCharacters.begin();
        while (it != end) {
            if (it->mMMesh) {
                it->mMMesh->CollideList(seg, colls);
            }
            Character *theChar = it->mDef.mChar;
            for (unsigned int i = 0; i != it->m3DChars.size(); i++) {
                Apply3DCharXfm(it, i, 0);
                float dist;
                Plane plane;
                RndDrawable *draw = theChar->CollideShowing(seg, dist, plane);
                if (draw) {
                    if (it->m3DChars[i].mHandle == 0) {
                        it->m3DChars[i].mHandle = Hmx::Object::New<WorldCrowd3DCharHandle>();
                        it->m3DChars[i].mHandle->Set3DChar(this, it, i, it->m3DChars[i].mXfm);
                    }
                    WorldCrowd3DCharHandle *h = it->m3DChars[i].mHandle;
                    RndDrawable *drawable = h ? (RndDrawable *)h : 0;
                    Collision coll;
                    coll.object = drawable;
                    coll.distance = dist;
                    coll.plane = plane;
                    colls.insert(colls.end(), coll);
                }
            }
            ++it;
        }
    }
}

void WorldCrowd::Poll() {
    if (Showing()) {
        FOREACH (it, mCharacters) {
            Character *curChar = it->mDef.mChar;
            if (curChar && curChar->GetPollState() != 3) {
                curChar->Poll();
            }
        }
    }
}

void WorldCrowd::Enter() {
    RndPollable::Enter();
    FOREACH (it, mCharacters) {
        it->mDef.mMats.clear();
        Character *curChar = it->mDef.mChar;
        if (curChar) {
            if (curChar->GetPollState() != 2)
                curChar->Enter();
            ColorPalette *randPal = curChar->Find<ColorPalette>("random1.pal", false);
            if (randPal && randPal->NumColors() != 0) {
                for (ObjDirItr<RndMat> objIt(curChar, true); objIt; ++objIt) {
                    it->mDef.mMats.push_back(objIt);
                }
            }
        }
    }
}

void WorldCrowd::Exit() {
    RndPollable::Exit();
    FOREACH (it, mCharacters) {
        Character *curChar = it->mDef.mChar;
        if (curChar)
            curChar->Exit();
    }
}

void WorldCrowd::ListPollChildren(std::list<RndPollable *> &polls) const {
    FOREACH (it, mCharacters) {
        Character *curChar = it->mDef.mChar;
        if (curChar)
            polls.push_back(curChar);
    }
}

void WorldCrowd::Delete3DCrowdHandles() {
    if (TheLoadMgr.EditMode()) {
        FOREACH (it, mCharacters) {
            for (int i = 0; i != it->m3DChars.size(); i++) {
                RELEASE(it->m3DChars[i].mHandle);
            }
        }
    }
}

bool WorldCrowd::Crowd3DExists() {
    FOREACH (it, mCharacters) {
        if (it->mDef.mChar && it->mMMesh && !it->m3DChars.empty()) {
            return true;
        }
    }
    return false;
}

void WorldCrowd::SetMatAndCameraLod() {
    RndTex *tex = gImpostorTex[mLod];
    gImpostorCamera->SetTargetTex(tex);
    gImpostorMat->SetDiffuseTex(tex);
}

void WorldCrowd::CreateMeshes() {
    mCharFullness = 1.0f;
    mFlatFullness = 1.0f;
    mLod = 0;
    FOREACH (it, mCharacters) {
        if (it->mMMesh) {
            delete it->mMMesh->Mesh();
            RELEASE(it->mMMesh);
        }
        it->mBackup.clear();
        if (it->mDef.mChar) {
            RndMesh *built = BuildBillboard(it->mDef.mChar, it->mDef.mHeight);
            it->mMMesh = Hmx::Object::New<RndMultiMesh>();
            it->mMMesh->SetMesh(built);
        }
    }
}

struct Sort3DChars {
    bool operator()(
        const WorldCrowd::CharData::Char3D &char1,
        const WorldCrowd::CharData::Char3D &char2
    ) const {
        return char1.mIdx < char2.mIdx;
    }
};

void WorldCrowd::Sort3DCharList() {
    FOREACH (it, mCharacters) {
        std::sort(it->m3DChars.begin(), it->m3DChars.end(), Sort3DChars());
        it->m3DCharsCreated = it->m3DChars;
    }
}

void WorldCrowd::Set3DCharAll() {
    START_AUTO_TIMER("crowd_set3d");
    float fvar1 = mFlatFullness;
    Reset3DCrowd();
    FOREACH (it, mCharacters) {
        RndMultiMesh *multiMesh = it->mMMesh;
        if (multiMesh) {
            std::list<RndMultiMesh::Instance>::iterator instIt = multiMesh->Instances().begin();
            int idx = 0;
            for (; instIt != multiMesh->Instances().end(); ++instIt, ++idx) {
                CharData::Char3D char3D(instIt->mXfm, idx);
                it->m3DChars.push_back(char3D);
            }
            multiMesh->Instances().clear();
            multiMesh->InvalidateProxies();
        }
    }
    Sort3DCharList();
    SetFullness(fvar1, mCharFullness);
    AssignRandomColors(false);
}

void WorldCrowd::Force3DCrowd(bool force) {
    mForce3DCrowd = force;
    if (mForce3DCrowd) {
        Set3DCharAll();
    } else {
        SetFullness(1, 1);
        Hmx::Object *obj = this;
        std::vector<std::pair<int, int> > v;
        Set3DCharList(v, obj);
    }
}

RndMesh *WorldCrowd::BuildBillboard(Character *c, float height) {
    RndMesh *mesh = Hmx::Object::New<RndMesh>();
    mesh->SetMutable(0x1f);
    RndMesh::VertVector &verts = mesh->Verts();
    std::vector<RndMesh::Face> &faces = mesh->Faces();
    float halfHeight = height * 0.5f;
    float halfWidth = halfHeight * 0.5f;
    verts.resize(4);
    float negHalfWidth = -halfWidth;
    verts[0].pos.Set(negHalfWidth, 0, halfHeight);
    float negHalfHeight = -halfHeight;
    verts[1].pos.Set(negHalfWidth, 0, negHalfHeight);
    verts[2].pos.Set(halfWidth, 0, halfHeight);
    verts[3].pos.Set(halfWidth, 0, negHalfHeight);

    verts[0].tex.Set(0, 0);
    verts[1].tex.Set(0, 1);
    verts[2].tex.Set(1, 0);
    verts[3].tex.Set(1, 1);

    faces.resize(2);
    faces[0].Set(0, 1, 2);
    faces[1].Set(1, 3, 2);
    mesh->Sync(0x3F);
    mesh->SetMat(gImpostorMat);
    mesh->SetTransConstraint(RndTransformable::kConstraintFastBillboardXYZ, gImpostorCamera, false);
    return mesh;
}

void SetMatColorFlags(ObjPtrList<RndMat, ObjectDir> &, BaseMaterial::ColorModFlags, std::vector<Hmx::Color> *);

void WorldCrowd::Draw3DChars() {
    if (!Crowd3DExists()) return;
    // Use mEnviron3D if it has a pointer, else mEnviron
    RndEnviron *env;
    env = mEnviron3D ? mEnviron3D : mEnviron;
    // Save and clear the environ's use-approx-global flag
    bool savedApprox = true;
    if (env) {
        savedApprox = env->UsesApproxGlobal();
        env->SetUseApproxGlobal(false);
    }
    RndEnvironTracker tracker(env, nullptr);
    ObjList<CharData>::iterator charIt = mCharacters.begin();
    auto charsEnd = mCharacters.end();
    for (; charIt != charsEnd; ++charIt) {
        Character *curChar = charIt->mDef.mChar;
        if (!curChar || !charIt->mMMesh) continue;
        int numChars = (int)charIt->m3DChars.size();
        for (int i = 0; (unsigned int)i < numChars; i++) {
            Apply3DCharXfm(charIt, i, RndCam::Current());
#ifdef HX_NATIVE
            // 3D crowd characters not used in normal rendering —
            // Force3DCrowd is always false on native, so m3DChars is empty.
            // The impostor billboard path in DrawShowing handles all crowd rendering.
            (void)i;
            break;
#else
            if (charIt->mDef.mUseRandomColor) {
                SetMatColorFlags(charIt->mDef.mMats, RndMat::kColorModModulate, &charIt->m3DChars[i].mColors);
            }
            bool savedSelfShadow = curChar->SelfShadow();
            bool savedUnk252 = *(bool *)((char *)curChar + 0x252);
            bool savedUnk251 = *(bool *)((char *)curChar + 0x251);
            bool isInGame = *(bool *)((char *)&TheRnd + 0x143);
            if (isInGame) {
                curChar->SetSelfShadow(false);
                *(bool *)((char *)curChar + 0x252) = false;
                *(bool *)((char *)curChar + 0x251) = false;
            }
            if (mCharForceLod != kLODPerFrame) {
                curChar->SetLodType(mCharForceLod);
            }
            curChar->DrawShowing();
            if (mCharForceLod != kLODPerFrame) {
                curChar->SetLodType(kLODPerFrame);
            }
            curChar->SetSelfShadow(savedSelfShadow);
            *(bool *)((char *)curChar + 0x252) = savedUnk252;
            *(bool *)((char *)curChar + 0x251) = savedUnk251;
#endif
        }
    }
    if (env) {
        env->SetUseApproxGlobal(savedApprox);
    }
}

void WorldCrowd::AssignRandomColors(bool incrementStamp) {
    if (incrementStamp) {
        mModifyStamp++;
    }
    FOREACH (it, mCharacters) {
        if (it->mDef.mChar && it->mMMesh && it->m3DChars.size() > 0) {
            it->mDef.mUseRandomColor = false;
            std::vector<ColorPalette *> colorPaletteList;
            for (int i = 0; i < 3; ++i) {
                ColorPalette *randPal = it->mDef.mChar->Find<ColorPalette>(
                    MakeString("random%d.pal", i + 1), false
                );
                if (randPal) {
                    colorPaletteList.push_back(randPal);
                }
            }
            if (colorPaletteList.size() > 0) {
                for (unsigned int i = 0; i < it->m3DChars.size(); ++i) {
                    CharData::Char3D &char3D = it->m3DChars[i];
                    char3D.mColors.clear();
                    it->mDef.mUseRandomColor = true;
                    while (char3D.mColors.size() < 3) {
                        ColorPalette *randPal =
                            colorPaletteList[RandomInt(0, colorPaletteList.size())];
                        Hmx::Color randColor =
                            randPal->GetColor(RandomInt(0, randPal->NumColors()));
                        char3D.mColors.push_back(randColor);
                    }
                }
            }
        }
    }
}

void WorldCrowd::Reset3DCrowd() {
    SetFullness(1.0f, mCharFullness);
    FOREACH (it, mCharacters) {
        RndMultiMesh *multiMesh = it->mMMesh;
        if (multiMesh) {
            InstanceList &instances = multiMesh->Instances();
            InstanceList::iterator instIt = instances.begin();
            int curInstIdx = 0;
            for (int i = 0; (unsigned int)i != it->m3DCharsCreated.size(); i++) {
                int targetInstIdx = (int)(intptr_t)it->m3DCharsCreated[i].mHandle;
                while (curInstIdx != targetInstIdx) {
                    ++instIt;
                    curInstIdx++;
                }
                RndMultiMesh::Instance inst(it->m3DCharsCreated[i].mXfm);
                instIt = instances.insert(instIt, inst);
                ++instIt;
                curInstIdx++;
            }
        }
        it->m3DCharsCreated.clear();
        it->m3DChars.clear();
    }
}

void WorldCrowd::SetFullness(float flatFullness, float charFullness) {
    START_AUTO_TIMER("crowd_set");
    mCharFullness = charFullness;
    mFlatFullness = flatFullness;
    Delete3DCrowdHandles();
    FOREACH (it, mCharacters) {
        RndMultiMesh *multiMesh = it->mMMesh;
        if (multiMesh) {
            InstanceList &instances = multiMesh->mInstances;
            int instanceCount = (int)instances.size();
            InstanceList &backup = it->mBackup;
            int backupCount = (int)backup.size();
            int targetInstances = (int)((float)(instanceCount + backupCount) * mFlatFullness);
            if (instanceCount < targetInstances) {
                int toMove = targetInstances - instanceCount;
                InstanceList::iterator backIt = backup.begin();
                for (int i = 0; i < toMove; i++) {
                    ++backIt;
                }
                instances.splice(instances.end(), backup, backup.begin(), backIt);
            } else if (targetInstances < instanceCount) {
                int toRemove = instanceCount - targetInstances;
                InstanceList::iterator instIt = instances.begin();
                for (int i = 0; i < toRemove; i++) {
                    ++instIt;
                }
                backup.splice(backup.end(), instances, instances.begin(), instIt);
                multiMesh->InvalidateProxies();
            }
            unsigned int totalChars3D = it->m3DCharsCreated.size();
            int targetChars3D = (int)((float)totalChars3D * charFullness);
            if (targetChars3D >= (int)totalChars3D) {
                targetChars3D = (int)totalChars3D;
            }
            int currentChars3D = (int)it->m3DChars.size();
            if (currentChars3D < targetChars3D) {
                int toAdd = targetChars3D - currentChars3D;
                for (int i = 0; i < toAdd; i++) {
                    it->m3DChars.push_back(it->m3DCharsCreated[(int)it->m3DChars.size()]);
                }
            } else if (targetChars3D < currentChars3D) {
                int toRemove = currentChars3D - targetChars3D;
                for (int i = 0; i < toRemove; i++) {
                    it->m3DChars.pop_back();
                }
            }
        }
    }
    AssignRandomColors(false);
}

void WorldCrowd::Set3DCharXfm(
    const std::list<CharData>::iterator &charItr, int charIdx, const Transform &xfm
) {
    MILO_ASSERT(charIdx >= 0 && charIdx < (int)charItr->m3DChars.size(), 0x289);
    CharData::Char3D &char3D = charItr->m3DChars[charIdx];
    char3D.mXfm = xfm;
    // Also update the matching entry in m3DCharsCreated (matched by handle)
    WorldCrowd3DCharHandle *handle = char3D.mHandle;
    for (int i = 0; i < (int)charItr->m3DCharsCreated.size(); i++) {
        if (charItr->m3DCharsCreated[i].mHandle == handle) {
            charItr->m3DCharsCreated[i].mXfm = xfm;
            break;
        }
    }
    MILO_ASSERT(true, 0x297); // always passes - just for line number
}

void WorldCrowd::Apply3DCharXfm(
    const std::list<CharData>::iterator &charItr, int charIdx, RndCam *cam
) {
    MILO_ASSERT(charIdx >= 0 && charIdx < (int)charItr->m3DChars.size(), 0x29d);
    int charHandle = (int)charItr->m3DChars[charIdx].mHandle;
    if (charHandle == 0) return;
    RndTransformable *environ = mEnviron;
    if (environ == 0) return;

    float charPosX = charItr->m3DChars[charIdx].mXfm.v.x;
    float charPosY = charItr->m3DChars[charIdx].mXfm.v.y;
    float charPosZ = charItr->m3DChars[charIdx].mXfm.v.z;
    float charRadiusAdjust = -(charItr->mDef.mRadius * 0.5f - charItr->m3DChars[charIdx].mXfm.m.z.z);

    bool useCam = (mCrowdRotate != 0) && (cam != 0);
    if ((!useCam) && (!mFocus)) {
        Transform newXfm = environ->WorldXfm();
        ((WorldCrowd3DCharHandle *)charHandle)->SetWorldXfm(newXfm);
        return;
    }

    const Transform &environXfm = environ->WorldXfm();
    float envY_x = environXfm.m.y.x;
    float envY_y = environXfm.m.y.y;
    float envY_z = environXfm.m.y.z;

    float forwardDir_x, forwardDir_y, forwardDir_z;

    if (mCrowdRotate == 1) {
        const Transform &camXfm = cam->WorldXfm();
        forwardDir_x = camXfm.m.z.y * envY_x - camXfm.m.z.x * envY_y;
        forwardDir_y = camXfm.m.z.x * envY_z - camXfm.m.z.z * envY_x;
        forwardDir_z = camXfm.m.z.z * envY_y - camXfm.m.z.y * envY_z;
    } else if (mCrowdRotate == 2) {
        const Transform &camXfm = cam->WorldXfm();
        forwardDir_x = camXfm.m.z.x * envY_y - camXfm.m.z.y * envY_x;
        forwardDir_y = camXfm.m.z.z * envY_x - camXfm.m.z.x * envY_z;
        forwardDir_z = camXfm.m.z.y * envY_z - camXfm.m.z.z * envY_y;
    } else {
        const Transform &focusXfm = mFocus->WorldXfm();
        forwardDir_x = focusXfm.v.x - charPosX;
        forwardDir_y = focusXfm.v.y - charPosY;
        forwardDir_z = focusXfm.v.y * envY_z - envY_y * 0.0f;
        float fx = forwardDir_x, fy = forwardDir_y;
        forwardDir_x = fy * envY_z - envY_y * 0.0f;
        forwardDir_y = envY_z * fx - envY_x * 0.0f;
        forwardDir_z = envY_y * fx - fy * envY_x;
    }

    Vector3 forwardVec;
    forwardVec.x = forwardDir_x;
    forwardVec.y = forwardDir_y;
    forwardVec.z = forwardDir_z;
    Normalize(forwardVec, forwardVec);

    float rightDir_x = forwardVec.z * envY_y - forwardVec.y * envY_z;
    float rightDir_y = forwardVec.x * envY_z - forwardVec.z * envY_x;
    float rightDir_z = forwardVec.y * envY_x - forwardVec.x * envY_y;

    Transform newXfm;
    newXfm.m.x.x = forwardVec.x;
    newXfm.m.x.y = forwardVec.y;
    newXfm.m.x.z = forwardVec.z;
    newXfm.m.y.x = envY_x;
    newXfm.m.y.y = envY_y;
    newXfm.m.y.z = envY_z;
    newXfm.m.z.x = rightDir_x;
    newXfm.m.z.y = rightDir_y;
    newXfm.m.z.z = rightDir_z;
    newXfm.v.x = charPosX;
    newXfm.v.y = charPosY;
    newXfm.v.z = charPosZ;

    ((WorldCrowd3DCharHandle *)charHandle)->SetWorldXfm(newXfm);
}

void WorldCrowd::Set3DCharList(
    const std::vector<std::pair<int, int> > &pairVec, Hmx::Object *obj
) {
    START_AUTO_TIMER("crowd_set3d");
    if (mForce3DCrowd) {
        AssignRandomColors(false);
    } else {
        float oldFullness = mFlatFullness;
        Reset3DCrowd();
        std::vector<std::pair<RndMultiMesh *, InstanceList::iterator> > grosserPairs;
        grosserPairs.reserve(pairVec.size());
        for (int i = 0; (unsigned int)i != pairVec.size(); i++) {
            int meshIdx = pairVec[i].first;
            if ((unsigned int)meshIdx >= (int)mCharacters.size()) {
                MILO_NOTIFY(
                    "%s setting bad mesh %d, only has %d",
                    PathName(obj),
                    meshIdx,
                    mCharacters.size()
                );
                continue;
            }
            ObjList<CharData>::iterator charIt = mCharacters.begin();
            for (int n = 0; n < meshIdx; ++n, ++charIt)
                ;
            if (charIt->mMMesh) {
                int charInstIdx = pairVec[i].second;
                if ((unsigned int)charInstIdx >= charIt->mMMesh->Instances().size()) {
                    MILO_NOTIFY(
                        "%s setting bad 3d char %d on mmesh %s, only has %d chars",
                        PathName(obj),
                        charInstIdx,
                        charIt->mMMesh->Name(),
                        charIt->mMMesh->Instances().size()
                    );
                } else {
                    InstanceList::iterator instIt = charIt->mMMesh->Instances().begin();
                    for (int n = 0; n < charInstIdx; ++instIt, ++n)
                        ;
                    charIt->m3DChars.push_back(CharData::Char3D(instIt->mXfm, charInstIdx));
                    grosserPairs.push_back(std::make_pair(charIt->mMMesh, instIt));
                }
            }
        }
        for (int i = 0; (unsigned int)i != grosserPairs.size(); i++) {
            grosserPairs[i].first->Instances().erase(grosserPairs[i].second);
            grosserPairs[i].first->InvalidateProxies();
        }
        Sort3DCharList();
        SetFullness(oldFullness, mCharFullness);
        AssignRandomColors(false);
    }
}

void WorldCrowd::Mats(std::list<RndMat *> &mats, bool additive) {
    if (additive) {
        MatShaderOptions opts;
        int shaderTypes[2] = {0xd, 0x13};

        // Set i5 flag on opts
        opts.pack |= 0x20;

        // Create impostor mats for all shader type / useEnviron / hasAO combinations
        for (int p = 0; p < 2; p++) {
            opts.SetLast5(shaderTypes[p]);
            for (int envIdx = 0; envIdx < 2; envIdx++) {
                bool useEnv = (envIdx != 0);
                for (int aoIdx = 0; aoIdx < 2; aoIdx++) {
                    RndMat *mat = Hmx::Object::New<RndMat>();
                    mat->Copy(gImpostorMat, kCopyDeep);
                    mat->SetUseEnv(useEnv);
                    opts.mTempMat = true;
                    opts.shader_struct.mHasAOCalc = 0;
                    opts.shader_struct.mHasAOCalc = aoIdx;
                    mat->SetShaderOpts(opts);
                    mats.insert(mats.end(), mat);
                }
            }
        }

        // Initialize white color vector for color mod
        std::vector<Hmx::Color> colors;
        Hmx::Color white(1.0f, 1.0f, 1.0f, 1.0f);
        for (int i = 0; i < 3; i++) {
            colors.push_back(white);
        }

        // Iterate over color mod flags, skipping kColorModAlphaUnpackModulate
        for (int colorIdx = 0; colorIdx <= 3; colorIdx++) {
            if (colorIdx == 2) continue;

            for (std::list<CharData>::iterator charIt = mCharacters.begin();
                 charIt != mCharacters.end(); ++charIt) {
                if (charIt->mDef.mUseRandomColor) {
                    SetMatColorFlags(charIt->mDef.mMats, (BaseMaterial::ColorModFlags)colorIdx, &colors);
                }

                for (ObjPtrList<RndMat>::iterator matIt = charIt->mDef.mMats.begin();
                     matIt != charIt->mDef.mMats.end(); ++matIt) {
                    std::list<unsigned int> flags;
                    GetMeshShaderFlags(*matIt, flags);
                    for (std::list<unsigned int>::iterator flagIt = flags.begin();
                         flagIt != flags.end(); ++flagIt) {
                        unsigned int flag = *flagIt;
                        opts.pack = 0x12;
                        opts.SetHasBones(flag & 1);
                        opts.SetHasAOCalc((flag >> 1) & 1);
                        RndMat *newMat = Hmx::Object::New<RndMat>();
                        newMat->Copy(*matIt, kCopyDeep);
                        opts.mTempMat = true;
                        newMat->SetShaderOpts(opts);
                        mats.insert(mats.end(), newMat);
                    }
                }
            }
        }
    }
}

DataNode WorldCrowd::OnIterateFrac(DataArray *da) {
    START_AUTO_TIMER("crowd_iter");

    if (mCharacters.empty()) {
        return DataNode(0);
    }

    // Collect non-null Character pointers into local array
    Character *chars[64];
    int count = 0;
    for (std::list<CharData>::iterator it = mCharacters.begin();
         it != mCharacters.end(); ++it) {
        Character *c = it->mDef.mChar.Ptr();
        if (c) {
            chars[count++] = c;
        }
    }

    // Fisher-Yates shuffle
    for (int i = count - 1; i > 0; i--) {
        int j = RandomInt() % (i + 1);
        Character *tmp = chars[i];
        chars[i] = chars[j];
        chars[j] = tmp;
    }

    // Calculate total fraction weight
    float totalWeight = 0.0f;
    for (int i = 2; i < da->Size(); i++) {
        DataArray *sub = da->Array(i);
        float frac = sub->Float(0);
        if (frac > 0.0f) {
            totalWeight += frac;
        }
    }

    // Iterate characters, executing scripts by fraction
    float charsPerWeight = (float)count / totalWeight;
    float threshold = -0.5f;
    int charIdx = 0;
    for (int i = 2; i < da->Size(); i++) {
        DataArray *sub = da->Array(i);
        float frac = sub->Float(0);
        threshold += frac * charsPerWeight;
        while ((float)charIdx < threshold) {
            sub->ExecuteScript(1, chars[charIdx], 0, 1);
            charIdx++;
        }
    }

    return DataNode(0);
}

static inline void DrawMultiMeshWithEnviron(RndMultiMesh *mmesh) {
    RndEnviron *curEnv = RndEnviron::Current();
    bool savedApprox = true;
    if (curEnv) {
        savedApprox = curEnv->UsesApproxGlobal();
        curEnv->SetUseApproxGlobal(false);
    }
    {
        RndEnvironTracker tracker(curEnv, nullptr);
        mmesh->DrawShowing();
    }
    if (curEnv) {
        curEnv->SetUseApproxGlobal(savedApprox);
    }
}

static const char *sCollideNames[] = {
    "chest.coll", "head.coll", "l_arm.coll", "l_foot.coll", "l_hand.coll", "l_knee.coll",
    "pelvis.coll", "r_arm.coll", "r_foot.coll", "r_hand.coll", "r_knee.coll", ""
};

void WorldCrowd::DrawShowing() {
    START_AUTO_TIMER("crowd_draw");
    if (!mPlacementMesh) return;
    Draw3DChars();
    if (Rnd::kDrawOcclusionDepth == TheRnd.GetDrawMode()) return;
    MILO_ASSERT(!dynamic_cast<RndMat*>(gImpostorMat->NextPass()), 0x3A0);
    std::vector<Hmx::Rect> rects;
    rects.reserve(12);
    FOREACH (charIt, mCharacters) {
        Character *curChar = charIt->mDef.mChar;
        RndMultiMesh *mmesh = charIt->mMMesh;
        if (curChar && mmesh && !mShow3DOnly
            && TheRnd.GetDrawMode() != Rnd::kDrawOcclusionDepth) {
            int numInstances = 0;
            for (InstanceList::iterator instIt = mmesh->Instances().begin();
                 instIt != mmesh->Instances().end(); ++instIt) {
                numInstances++;
            }
            if ((unsigned long)(int)numInstances != 0) {
#ifdef HX_NATIVE
                // Native cached impostor path: render each character type to a
                // dedicated texture ONCE, then reuse on all subsequent frames.
                // Characters don't animate on native (no clip subdirs), so a
                // single snapshot is sufficient.
                RndTex *cachedTex = nullptr;
                {
                    auto cacheIt = sImpostorCache.find(curChar);
                    if (cacheIt != sImpostorCache.end()) {
                        cachedTex = cacheIt->second;
                    }
                }
                if (!cachedTex) {
                    cachedTex = Hmx::Object::New<RndTex>();
                    cachedTex->SetBitmap(256, 512, 32, RndTex::kRendered, true, nullptr);

                    gImpostorCamera->SetTargetTex(cachedTex);

                    RndCam *curCam = RndCam::Current();
                    Transform camXfmCopy;
                    memcpy(&camXfmCopy, &curCam->WorldXfm(), sizeof(Transform) - sizeof(Vector3));

                    float halfHeight = charIt->mDef.mHeight * 0.5f;

                    // Position impostor camera at distance along camera's -Y axis
                    const Transform &placementXfm = mPlacementMesh->WorldXfm();
                    const Transform &curCamXfm = curCam->WorldXfm();
                    float dx = curCamXfm.v.x - placementXfm.v.x;
                    float dy = curCamXfm.v.y - placementXfm.v.y;
                    float dz = curCamXfm.v.z - placementXfm.v.z - halfHeight;
                    float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
                    float minDist = curCam->NearPlane() + halfHeight;
                    if (dist < minDist) dist = minDist;
                    float negDist = -dist;
                    camXfmCopy.v.x = camXfmCopy.m.y.x * negDist;
                    camXfmCopy.v.y = camXfmCopy.m.y.y * negDist;
                    camXfmCopy.v.z = camXfmCopy.m.y.z * negDist + halfHeight;
                    gImpostorCamera->SetLocalXfm(camXfmCopy);
                    float yFov = std::atan((double)(halfHeight / dist)) * 2.0f;
                    gImpostorCamera->SetFrustum(
                        curCam->NearPlane(), curCam->FarPlane(), yFov, 1.0f
                    );

                    // Orient character based on crowd rotation mode
                    Transform charXfm;
                    if (mCrowdRotate == kCrowdRotateNone) {
                        memcpy(&charXfm, &mPlacementMesh->WorldXfm(), 0x30);
                    } else {
                        const Transform &meshXfm = mPlacementMesh->WorldXfm();
                        float upX = meshXfm.m.z.x;
                        float upY = meshXfm.m.z.y;
                        float upZ = meshXfm.m.z.z;
                        float fwdY, fwdZ, tempA, tempB;
                        if (mCrowdRotate == kCrowdRotateFace) {
                            const Transform &camWXfm = curCam->WorldXfm();
                            fwdZ = camWXfm.m.y.y * upX - camWXfm.m.y.x * upY;
                            fwdY = camWXfm.m.y.x * upZ - camWXfm.m.y.z * upX;
                            tempA = upZ; tempB = upY;
                        } else {
                            const Transform &camWXfm = curCam->WorldXfm();
                            fwdY = camWXfm.m.y.z * upX - camWXfm.m.y.x * upZ;
                            fwdZ = camWXfm.m.y.x * upY - camWXfm.m.y.y * upX;
                            tempA = upY; tempB = upZ;
                        }
                        charXfm.m.x.x = curCam->WorldXfm().m.y.y * tempB - curCam->WorldXfm().m.y.z * tempA;
                        charXfm.m.x.y = fwdY;
                        charXfm.m.x.z = fwdZ;
                        Normalize(charXfm.m.x, charXfm.m.x);
                        charXfm.m.y.x = charXfm.m.x.y * upX - upY * charXfm.m.x.x;
                        charXfm.m.y.y = upZ * charXfm.m.x.x - charXfm.m.x.z * upX;
                        charXfm.m.y.z = charXfm.m.x.z * upY - charXfm.m.x.y * upZ;
                        charXfm.m.z.x = upX;
                        charXfm.m.z.y = upY;
                        charXfm.m.z.z = upZ;
                    }
                    charXfm.v.x = 0;
                    charXfm.v.y = 0;
                    charXfm.v.z = 0;
                    curChar->SetWorldXfm(charXfm);

                    // Render character to the cached impostor texture
                    RndEnviron *env = mEnviron;
                    bool savedApprox = true;
                    if (env) {
                        savedApprox = env->UsesApproxGlobal();
                        env->SetUseApproxGlobal(false);
                    }
                    {
                        const Transform &charWorldXfm = curChar->WorldXfm();
                        RndEnvironTracker tracker(env, &charWorldXfm.v);
                        gImpostorCamera->Select();
                        curChar->SetShowing(true);
                        if (mCharForceLod != kLODPerFrame) {
                            curChar->SetLodType(mCharForceLod);
                        }
                        curChar->DrawShowing();
                        if (mCharForceLod != kLODPerFrame) {
                            curChar->SetLodType(kLODPerFrame);
                        }
                    }
                    if (env) {
                        env->SetUseApproxGlobal(savedApprox);
                    }
                    curCam->Select();

                    gImpostorCamera->SetTargetTex(gImpostorTex[mLod]);
                    sImpostorCache[curChar] = cachedTex;
                }

                gImpostorMat->SetDiffuseTex(cachedTex);
                // Character materials don't write alpha, so the RT alpha stays
                // at the clear value (0). Use additive blend to bypass alpha:
                // black background adds nothing (0+scene=scene), character
                // color adds on top. Disable alpha cut since alpha is all zero.
                gImpostorMat->SetAlphaCut(false);
                gImpostorMat->SetBlend(RndMat::kBlendAdd);
                DrawMultiMeshWithEnviron(mmesh);
                gImpostorMat->SetAlphaCut(true);
                gImpostorMat->SetBlend(RndMat::kBlendSrc);
                continue;
#endif
                SetMatAndCameraLod();
                RndCam *curCam = RndCam::Current();
                Transform camXfmCopy;
                memcpy(&camXfmCopy, &curCam->WorldXfm(), sizeof(Transform) - sizeof(Vector3));

                float halfHeight = charIt->mDef.mHeight * 0.5f;
                float halfWidth = halfHeight * 0.5f;

                // --- Set up impostor camera: position at -dist along camera's Y axis ---
                const Transform &placementXfm = mPlacementMesh->WorldXfm();
                const Transform &curCamXfm = curCam->WorldXfm();
                float dx = curCamXfm.v.x - placementXfm.v.x;
                float dy = curCamXfm.v.y - placementXfm.v.y;
                float dz = curCamXfm.v.z - placementXfm.v.z - halfHeight;
                float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
                float minDist = curCam->NearPlane() + halfHeight;
                dist = (float)__fsel(dist - minDist, dist, minDist);
                float negDist = -dist;
                // NOTE: 0.0f multiplications are dead math required for codegen match (fmul)
                camXfmCopy.v.x = camXfmCopy.m.x.x * 0.0f + (camXfmCopy.m.y.x * negDist + camXfmCopy.m.z.x * 0.0f);
                camXfmCopy.v.y = camXfmCopy.m.z.y * 0.0f + (camXfmCopy.m.y.y * negDist + camXfmCopy.m.x.y * 0.0f);
                camXfmCopy.v.z = (camXfmCopy.m.z.z * 0.0f + (camXfmCopy.m.y.z * negDist + camXfmCopy.m.x.z * 0.0f)) + halfHeight;
                gImpostorCamera->SetLocalXfm(camXfmCopy);
                float yFov = (float)std::atan((double)(halfHeight / dist)) * 2.0f;
                gImpostorCamera->SetFrustum(
                    curCam->NearPlane(), curCam->FarPlane(), yFov, 1.0f
                );

                // --- Compute character orientation based on crowd rotation mode ---
                Transform charXfm;
                if (mCrowdRotate == kCrowdRotateNone) {
                    const Transform &meshXfm = mPlacementMesh->WorldXfm();
                    memcpy(&charXfm, &meshXfm, 0x30);
                } else {
                    const Transform &meshXfm2 = mPlacementMesh->WorldXfm();
                    float upX = meshXfm2.m.z.x;
                    float upY = meshXfm2.m.z.y;
                    float upZ = meshXfm2.m.z.z;

                    // Cross product of camera Y-axis with mesh up vector,
                    // component swaps determine Face vs Away rotation
                    float fwdY, fwdZ;
                    float tempA, tempB;
                    if (mCrowdRotate == kCrowdRotateFace) {
                        const Transform &camWXfm = curCam->WorldXfm();
                        fwdZ = camWXfm.m.y.y * upX - camWXfm.m.y.x * upY;
                        fwdY = camWXfm.m.y.x * upZ - camWXfm.m.y.z * upX;
                        tempA = upZ;
                        tempB = upY;
                    } else {
                        const Transform &camWXfm = curCam->WorldXfm();
                        fwdY = camWXfm.m.y.z * upX - camWXfm.m.y.x * upZ;
                        fwdZ = camWXfm.m.y.x * upY - camWXfm.m.y.y * upX;
                        tempA = upY;
                        tempB = upZ;
                    }

                    // Forward (x-row): normalize cross product result
                    const Transform &camWXfm3 = curCam->WorldXfm();
                    charXfm.m.x.x = camWXfm3.m.y.y * tempB - camWXfm3.m.y.z * tempA;
                    charXfm.m.x.y = fwdY;
                    charXfm.m.x.z = fwdZ;
                    Normalize(charXfm.m.x, charXfm.m.x);

                    // Right (y-row): forward × up using normalized forward values
                    charXfm.m.y.x = charXfm.m.x.y * upX - upY * charXfm.m.x.x;
                    charXfm.m.y.y = upZ * charXfm.m.x.x - charXfm.m.x.z * upX;
                    charXfm.m.y.z = charXfm.m.x.z * upY - charXfm.m.x.y * upZ;
                    charXfm.m.z.x = upX;
                    charXfm.m.z.y = upY;
                    charXfm.m.z.z = upZ;
                }
                charXfm.v.x = 0;
                charXfm.v.y = 0;
                charXfm.v.z = 0;
                curChar->SetWorldXfm(charXfm);

                // --- Project colliders to screen-space rects ---
                rects.erase(rects.begin(), rects.end());
                for (int ci = 0; *sCollideNames[ci] != '\0'; ci++) {
                    CharCollide *collide =
                        curChar->Find<CharCollide>(sCollideNames[ci], false);
                    if (collide) {
                        Vector2 screenPos;
                        gImpostorCamera->WorldToScreen(
                            collide->WorldXfm().v, screenPos
                        );
                        float radius = collide->GetCurRadius();
                        const Transform &collideXfm = collide->WorldXfm();
                        Sphere sphere(collideXfm.v, radius);
                        float screenHeight =
                            gImpostorCamera->CalcScreenHeight(sphere);
                        if (screenHeight != kHugeFloat) {
                            float screenWidth = screenHeight * 2.0f;
                            Hmx::Rect rect(
                                -(screenWidth * 0.5f - screenPos.x),
                                -(screenHeight * 0.5f - screenPos.y),
                                screenWidth, screenHeight
                            );
                            rects.push_back(rect);
                        }
                    } else {
                        MILO_NOTIFY_ONCE(
                            "crowd char %s doesn't have %s\n",
                            PathName(curChar), sCollideNames[ci]
                        );
                    }
                }

                // --- Compute bounding rect (branchless fsel min/max) ---
                float minX = FLT_MAX;
                float maxX = -FLT_MAX;
                float maxY = -FLT_MAX;
                float minY = FLT_MAX;
                int numRects = (int)rects.size();
                if (numRects != 0) {
                    unsigned int ri = 0;
                    do {
                        float ry = rects[ri].y;
                        float rx = rects[ri].x;
                        ry = (float)__fsel(minX - ry, ry, minX);
                        rx = (float)__fsel(minY - rx, rx, minY);
                        float ryh = ry + rects[ri].h;
                        maxX = (float)__fsel(maxX - ryh, maxX, ryh);
                        float rxw = rx + rects[ri].w;
                        maxY = (float)__fsel(maxY - rxw, maxY, rxw);
                        minX = ry;
                        minY = rx;
                        ri++;
                    } while (ri != numRects);
                }
                // Clamp bounds to [0,1] screen space
                float clampedMinY = (float)__fsel(-minY, 0.0f, minY);
                float clampedMaxY = (float)__fsel(maxY - 1.0f, 1.0f, maxY);
                float clampedMinX = (float)__fsel(-minX, 0.0f, minX);
                float clampedMaxX = (float)__fsel(maxX - 1.0f, 1.0f, maxX);

                // --- Render character to impostor texture ---
                if (TheRnd.GetDrawMode() == Rnd::kDrawNormal) {
                    if (!mEnviron) {
                        MILO_NOTIFY_ONCE(
                            "%s: Rendering 2D crowd character texture without an environment, set the environ property on the WorldCrowd object.",
                            PathName(this)
                        );
                    }
                    RndEnviron *env = mEnviron;
                    bool savedApprox = true;
                    if (env) {
                        savedApprox = env->UsesApproxGlobal();
                        env->SetUseApproxGlobal(false);
                    }
                    {
                        const Transform &charWorldXfm = curChar->WorldXfm();
                        RndEnvironTracker tracker(env, &charWorldXfm.v);
                        gImpostorCamera->Select();
                        curChar->SetShowing(true);
                        if (mCharForceLod != kLODPerFrame) {
                            curChar->SetLodType(mCharForceLod);
                        }
                        curChar->DrawShowing();
                        if (mCharForceLod != kLODPerFrame) {
                            curChar->SetLodType(kLODPerFrame);
                        }
                    }
                    if (env) {
                        env->SetUseApproxGlobal(savedApprox);
                    }
                    curCam->Select();
                }

                // --- Update billboard quad vertices ---
                float uvLeft = -(clampedMinX * charIt->mDef.mHeight - halfHeight);
                float posLeft = clampedMinY * halfHeight - halfWidth;
                float uvBottom = -(clampedMaxX * charIt->mDef.mHeight - halfHeight);
                float posRight = clampedMaxY * halfHeight - halfWidth;

                RndMesh *billboardMesh = mmesh->Mesh();
                RndMesh::Vert *verts = billboardMesh->Verts().begin();
                verts[0].pos.x = posLeft;
                verts[0].pos.y = 0;
                verts[0].pos.z = uvLeft;
                verts[1].pos.x = posLeft;
                verts[1].pos.y = 0;
                verts[1].pos.z = uvBottom;
                verts[2].pos.x = posRight;
                verts[2].pos.y = 0;
                verts[2].pos.z = uvLeft;
                verts[3].pos.x = posRight;
                verts[3].pos.y = 0;
                verts[3].pos.z = uvBottom;
                verts[0].tex.x = clampedMinY;
                verts[0].tex.y = clampedMinX;
                verts[1].tex.x = clampedMinY;
                verts[1].tex.y = clampedMaxX;
                verts[2].tex.x = clampedMaxY;
                verts[2].tex.y = clampedMinX;
                verts[3].tex.x = clampedMaxY;
                verts[3].tex.y = clampedMaxX;
                billboardMesh->Sync(0x1F);

                // --- Draw billboarded multimesh instances ---
                DrawMultiMeshWithEnviron(mmesh);
            }
        }
    }
}

void SetMatColorFlags(ObjPtrList<RndMat, ObjectDir> &matList, BaseMaterial::ColorModFlags flags,
                      std::vector<Hmx::Color> *modulate) {
    FOREACH (it, matList) {
        (*it)->SetColorModFlags(flags);
        if (modulate) {
            MILO_ASSERT(BaseMaterial::kColorModNum == modulate->size(), 0x33b);
            for (int i = 0; i < modulate->size(); i++) {
                (*it)->SetColorMod(modulate->at(i), i);
            }
        }
    }
}

