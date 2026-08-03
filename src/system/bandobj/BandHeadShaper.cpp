#include "bandobj/BandHeadShaper.h"
#include "bandobj/BandFaceDeform.h"
#include "char/CharUtl.h"
#include "os/Debug.h"
#include "utl/Symbols.h"
#include <list>
#ifdef HX_NATIVE
#include <cstdlib> // getenv for the RB3_NO_HEAD_SHAPER opt-out
#endif

int BandHeadShaper::sChinNum;
int BandHeadShaper::sEyeNum;
int BandHeadShaper::sMouthNum;
int BandHeadShaper::sNoseNum;
int BandHeadShaper::sShapeNum;
static ObjectDir *gFemaleDir;
static ObjectDir *gMaleDir;
std::vector<int> gHeadMaleMapping;
std::vector<int> gHeadFemaleMapping;
ObjDirPtr<ObjectDir> gVisemes[4] = { ObjDirPtr<ObjectDir>(0),
                                     ObjDirPtr<ObjectDir>(0),
                                     ObjDirPtr<ObjectDir>(0),
                                     ObjDirPtr<ObjectDir>(0) };

void SetMeshAnim(ObjectDir *dir, std::vector<int> &vec) {
    RndMeshAnim *manim = dir->Find<RndMeshAnim>("base.msnm", false);
    if (!manim)
        MILO_WARN("%s does not contain base.msnm, no head shaping", PathName(dir));
    else {
        if (manim->VertPointsKeys().size() == 0) {
            MILO_WARN("%s has no point verts", PathName(manim));
        } else {
            std::vector<Vector3> &vertkeys = manim->VertPointsKeys()[0].value;
            vec.resize(vertkeys.size());
            ObjectDir *headdir = DirLoader::LoadObjects(
                FilePath(MakeString(
                    "char/main/head/%s/head.milo",
                    strstr(dir->GetPathName(), "female") ? "female" : "male"
                )),
                0,
                0
            );
            RndMesh *headmesh = headdir->Find<RndMesh>("head.mesh", false);
            if (headmesh) {
                if (vertkeys.size() == headmesh->Verts().size()) {
                    for (int i = 0; i < vec.size(); i++) {
                        vec[i] = i;
                    }
                    for (int i = 0; i < vec.size(); i++) {
                        float f19 = 1.0E+30f;
                        int i16 = -1;
                        const Vector3 &v = vertkeys[i];
                        for (int j = i; j < vec.size(); j++) {
                            float distsq =
                                DistanceSquared(v, headmesh->Verts()[vec[j]].pos);
                            if (distsq < f19) {
                                f19 = distsq;
                                i16 = j;
                            }
                        }
                        std::swap(vec[i], vec[i16]);
                    }
                } else {
                    MILO_WARN(
                        "%s has %d verts, head %s has %d, need to match\n",
                        PathName(manim),
                        vec.size(),
                        PathName(headmesh),
                        headmesh->Verts().size()
                    );
                    vec.clear();
                }
            } else {
                MILO_WARN("%s must contain head.mesh", PathName(headdir));
                vec.clear();
            }
            delete headdir;
        }
    }
}

int GetNum(const char *cc, int i1, ObjectDir *dir, int i2) {
#ifdef HX_NATIVE
    // The female-head branch in BandHeadShaper::Init reuses gMaleDir for the load
    // (matched-fork transcription), leaving gFemaleDir null on native, so dir can
    // arrive null here; guard the deref (the menu boot needs no face-deform counts).
    if (!dir)
        return 0;
#endif
    BandFaceDeform *df = dir->Find<BandFaceDeform>(MakeString("%s.fdm", cc), false);
    int num = 0;
    if (df) {
        num = df->mFrames.size();
        if ((num - 1) % i1) {
            MILO_LOG(
                "NOTIFY: %s has %d frames, must be multiple of %d + 1\n",
                PathName(df),
                num,
                i1
            );
        }
    }
    int ret = (num - 1) / i1;
    if (i2 >= 0 && i2 != ret) {
        MILO_LOG(
            "NOTIFY: %s must have %d frames, has %d\n", PathName(df), i1 * i2 + 1, num
        );
    }
    return ret;
}

ObjectDir *BandHeadShaper::GetViseme(Symbol s, bool b) {
    static Symbol female("female");
    return gVisemes[b + 2 * (s != female)];
}

ObjectDir *FindSubdir(ObjectDir *dir, const char *cc) {
#ifdef HX_NATIVE
    if (!dir) // gFemaleDir can be null on native (see GetNum note)
        return 0;
#endif
    ObjectDir *subdir = dir->Find<ObjectDir>(cc, false);
    if (!subdir)
        return 0;
    return subdir->SubDir(0);
}

void BandHeadShaper::Init() {
    FilePathTracker tracker(FileRoot());
    const char *genderpath = "";
    DataArray *cfg = SystemConfig("objects", "BandCharDesc");
    auto _tmp0 = cfg->FindData("head_male_path", genderpath, false);
#ifdef HX_NATIVE
    // char-Load 5b: serialization is byte-correct on LE (CharLoad5b gtest); head
    // shapes now load by default on native. RB3_NO_HEAD_SHAPER=1 opts back out.
    if (getenv("RB3_NO_HEAD_SHAPER"))
        _tmp0 = false;
#endif
    if (_tmp0 && genderpath[0] != 0) {
        static int _x = MemFindHeap("char");
        MemPushHeap(_x);
        {
            FilePath fp(genderpath);
            gMaleDir = DirLoader::LoadObjects(fp, 0, 0);
        }
        SetMeshAnim(gMaleDir, gHeadMaleMapping);
        sChinNum = GetNum("chin", 5, gMaleDir, -1);
        sEyeNum = GetNum("eye", 7, gMaleDir, -1);
        sMouthNum = GetNum("mouth", 5, gMaleDir, -1);
        sNoseNum = GetNum("nose", 5, gMaleDir, -1);
        sShapeNum = GetNum("shape", 1, gMaleDir, -1);
        GetNum("jaw", 5, gMaleDir, 1);
        gVisemes[2] = FindSubdir(gMaleDir, "visemes");
        gVisemes[3] = FindSubdir(gMaleDir, "vignette_visemes");
        MemPopHeap();
    }
    auto _tmp1 = cfg->FindData("head_female_path", genderpath, false);
#ifdef HX_NATIVE
    if (getenv("RB3_NO_HEAD_SHAPER")) // see head_male_path note above
        _tmp1 = false;
#endif
    if (_tmp1 && genderpath[0] != 0) {
        static int _x = MemFindHeap("char");
        MemPushHeap(_x);
        {
            FilePath fp(genderpath);
            gFemaleDir = DirLoader::LoadObjects(fp, 0, 0);
        }
        SetMeshAnim(gFemaleDir, gHeadFemaleMapping);
        GetNum("chin", 5, gFemaleDir, sChinNum);
        GetNum("eye", 7, gFemaleDir, sEyeNum);
        GetNum("mouth", 5, gFemaleDir, sMouthNum);
        GetNum("nose", 5, gFemaleDir, sNoseNum);
        GetNum("shape", 1, gFemaleDir, sShapeNum);
        GetNum("jaw", 5, gFemaleDir, 1);
        gVisemes[0] = FindSubdir(gFemaleDir, "visemes");
        gVisemes[1] = FindSubdir(gFemaleDir, "vignette_visemes");
        MemPopHeap();
    }
    for (int i = 0; i < 4; i++) {
        if (!gVisemes[i]) // FindSubdir returns null when its head dir was null
            continue;
        gVisemes[i]->SetName("", 0);
    }
}

int BandHeadShaper::GetCount(Symbol s) {
    if (s == shape)
        return sShapeNum;
    if (s == chin)
        return sChinNum;
    if (s == eye)
        return sEyeNum;
    if (s == nose)
        return sNoseNum;
    return s == mouth ? sMouthNum : 0;
}

void BandHeadShaper::Terminate() {
    RELEASE(gFemaleDir);
    RELEASE(gMaleDir);
    for (int i = 0; i < 4; i++)
        gVisemes[i] = 0;
}

BandHeadShaper::BandHeadShaper() : mBones(0) {}

BandHeadShaper::~BandHeadShaper() { MILO_ASSERT(!mBones, 0xD6); }

bool BandHeadShaper::Start(
    ObjectDir *dir, Symbol s, RndMesh *mesh, SyncMeshCB *cb, bool b
) {
    if (mesh->Verts().size() == 0)
        return false;
    else {
        static Symbol female("female");
        ObjectDir *visemedir = GetViseme(s, false);
        if (visemedir) {
            CharClip *clip = visemedir->Find<CharClip>("Base", false);
            if (clip) {
                clip->PoseMeshes(dir, clip->StartBeat());
            }
        }
        cb->SyncMesh(mesh, 0x1F);
        mDst = mesh;
        mBonesOnly = b;
        mMapping = s == female ? &gHeadFemaleMapping : &gHeadMaleMapping;
        mHeadDir = s == female ? gFemaleDir : gMaleDir;
        if (mMapping->size() == 0)
            return false;
        else {
            if (mMapping->size() != mDst->Verts().size()) {
                MILO_WARN(
                    "%s claims to be %s but has wrong vert number %d, should be %d",
                    PathName(mDst),
                    s,
                    mDst->Verts().size(),
                    mMapping->size()
                );
                return false;
            } else {
                mAnim = mHeadDir->Find<RndMeshAnim>("base.msnm", true);
                const std::vector<Vector3> &vec = mAnim->VertPointsKeys()[0].value;
                for (int i = 0; i < vec.size(); i++) {
                    mDst->Verts()[(*mMapping)[i]].pos = vec[i];
                }
                mBones = new CharBonesMeshes();
                mBones->SetName("head_morph", dir);
                mBase = mHeadDir->Find<CharClip>("base", true);
                mBase->StuffBones(*mBones);
                mBase->ScaleDown(*mBones, 0);
                return true;
            }
        }
    }
}

void BandHeadShaper::AddChildBones(RndTransformable *t) {
    std::vector<RndTransformable *>::iterator it =
        std::find(unk18.begin(), unk18.end(), t);
    if (it == unk18.end()) {
        unk18.push_back(t);
        for (std::list<RndTransformable *>::const_iterator child =
                 t->TransChildren().begin();
             child != t->TransChildren().end();
             ++child) {
            AddChildBones(*child);
        }
    }
}

void TestMesh(RndTransformable *start, RndTransformable *top) {
    Transform ident;
    ident.Reset();
    for (RndTransformable *cur = start; cur != top;) {
        RndTransformable *parent = cur->TransParent();
        if (!parent) {
            MILO_WARN(
                "%s needs to have eventual parent of %s, does not, stops at %s",
                PathName(start),
                PathName(top),
                cur->Name()
            );
            return;
        }
        const Transform &xfm = cur->LocalXfm();
        bool bad = xfm.v.x != 0.0f || xfm.v.y != 0.0f || xfm.v.z != 0.0f;
        if (!bad)
            bad = xfm.m.x != ident.m.x;
        if (!bad)
            bad = xfm.m.y != ident.m.y;
        if (!bad)
            bad = xfm.m.z != ident.m.z;
        if (bad) {
            MILO_WARN(
                "%s needs to be all zero'd xfms all the way up, but %s is not",
                PathName(start),
                PathName(cur)
            );
            return;
        }
        cur = parent;
    }
}

void BandHeadShaper::AddFrame(const char *cc, int frame, float weight) {
    BandFaceDeform *df =
        mAnim->Dir()->Find<BandFaceDeform>(MakeString("%s.fdm", cc), false);
    if (df && !mBonesOnly) {
        int fi = frame + 1;
        if ((unsigned int)fi < (unsigned short)df->mFrames.size()) {
            BandFaceDeform::DeltaArray &da = df->mFrames[fi];
            signed char *bytes;
            for (Delta *d = (Delta *)da.begin(); d < da.end();
                 d = (Delta *)d->next()) {
                bytes = (signed char *)d;
                for (int j = 0; j < d->num; j++) {
                    Vector3 delta;
                    delta.x = 0.015748031f * (float)bytes[4];
                    delta.y = 0.015748031f * (float)bytes[5];
                    delta.z = 0.015748031f * (float)bytes[6];
                    int vi = (*mMapping)[j + *(unsigned short *)d];
                    RndMesh::Vert &v = mDst->Verts()[vi];
                    v.pos.x = delta.x * weight + v.pos.x;
                    v.pos.y = delta.y * weight + v.pos.y;
                    v.pos.z = delta.z * weight + v.pos.z;
                    bytes += 3;
                }
            }
        }
    }
    CharClip *clip = mAnim->Dir()->Find<CharClip>(cc, false);
    if (clip) {
        clip->ScaleAdd(*mBones, weight, clip->FrameToBeat(frame + 1), 0.0f);
    }
}

void BandHeadShaper::AddFrameHelper(
    const char *cc, int i1, int i2, float f, float &fref
) {
    int frame;
    float weight;
    if (f < 0.5f) {
        frame = i1 + i2;
        weight = -(2.0f * f - 1.0f);
    } else {
        frame = i1 + i2 + 1;
        weight = 2.0f * (f - 0.5f);
    }
    AddFrame(cc, frame, weight);
    fref -= weight;
}

void BandHeadShaper::AddDegrees(const char *cc, int i1, float *degrees, int count) {
    int base = i1 * (count * 2 + 1);
    float remainder = 1.0f;
    for (int i = 0; i < count; i++) {
        AddFrameHelper(cc, base, i * 2 + 1, degrees[i], remainder);
    }
    AddFrame(cc, base, remainder);
}

void BandHeadShaper::Reskin() {
    if (mBonesOnly)
        return;
    RndTransformable *topTrans =
        dynamic_cast<RndTransformable *>(mBones->Dir());
    std::list<CharBones::Bone> bones;
    mBase->ListBones(bones);
    for (std::list<CharBones::Bone>::iterator it = bones.begin(); it != bones.end();
         ++it) {
        AddChildBones(CharUtlFindBoneTrans(it->name.Str(), mBones->Dir()));
    }
    for (int i = 0; i < unk18.size(); i++) {
        RndTransformable *bone = unk18[i];
        for (ObjRef::iterator rit = bone->Refs().begin(); rit != bone->Refs().end();
             ++rit) {
            RndMesh *mesh = dynamic_cast<RndMesh *>(RefPtrOf(rit)->RefOwner());
            if (!mesh)
                continue;
            if (strcmp(mesh->Name(), "head.mesh") != 0)
                continue;
#if defined(MILO_DEBUG) && defined(HX_NATIVE)
            TestMesh(mesh, topTrans);
#endif
            for (int j = 0; j < mesh->NumBones(); j++) {
                if (mesh->BoneTransAt(j) == bone) {
                    mesh->SetBone(j, bone, true);
                }
            }
        }
        bone->SetDirty();
    }
}

void BandHeadShaper::End() {
    mBones->ScaleAddIdentity();
    mBase->RotateBy(*mBones, mBase->StartBeat());
    mBones->PoseMeshes();
    Reskin();
    RELEASE(mBones);
}
